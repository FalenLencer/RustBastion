/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "net_transport.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define SOCK_INVALID    INVALID_SOCKET
  #define closesock       closesocket
  #define SOCK_WOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
  #define SOCK_INPROGRESS (WSAGetLastError() == WSAEWOULDBLOCK)
  static void net_plat_init(void) {
      static int started = 0;
      if (!started) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); started = 1; }
  }
  static void sock_nonblock(sock_t s) { u_long m = 1; ioctlsocket(s, FIONBIO, &m); }
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int sock_t;
  #define SOCK_INVALID    (-1)
  #define closesock       close
  #define SOCK_WOULDBLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
  #define SOCK_INPROGRESS (errno == EINPROGRESS)
  static void net_plat_init(void) {}
  static void sock_nonblock(sock_t s) {
      int f = fcntl(s, F_GETFL, 0);
      fcntl(s, F_SETFL, f | O_NONBLOCK);
  }
#endif

#define RING_CAP  16384
#define ASM_CAP    4096

typedef struct { unsigned char buf[RING_CAP]; int head, tail; } Ring;
typedef struct { Ring a2b, b2a; int refs; } LoopPipe;

struct Transport {
    int      kind;        // 0 = loopback, 1 = tcp
    int      connected;
    int      failed;
    // loopback
    LoopPipe *pipe;
    Ring     *rx, *tx;
    // tcp
    sock_t   listen_fd, fd;
    int      is_host;
    int      connecting;
    time_t   connect_deadline;   // échéance d'abandon de la connexion (client)
    // préambule à émettre EN PREMIER une fois connecté (mode relais)
    unsigned char preamble[6];
    int      preamble_len;       // 0 = aucun
    int      preamble_sent;
    // tampon d'assemblage des octets entrants (framing)
    unsigned char asmbuf[ASM_CAP];
    int      asmlen;
};

// ── Ring buffer (loopback) ───────────────────────────────────
static int ring_count(const Ring *r) { return (r->tail - r->head + RING_CAP) % RING_CAP; }
static int ring_space(const Ring *r) { return RING_CAP - 1 - ring_count(r); }
static int ring_write(Ring *r, const unsigned char *d, int n) {
    if (n > ring_space(r)) return 0;
    for (int i = 0; i < n; i++) { r->buf[r->tail] = d[i]; r->tail = (r->tail + 1) % RING_CAP; }
    return n;
}
static int ring_read(Ring *r, unsigned char *d, int n) {
    int avail = ring_count(r);
    if (n > avail) n = avail;
    for (int i = 0; i < n; i++) { d[i] = r->buf[r->head]; r->head = (r->head + 1) % RING_CAP; }
    return n;
}

// ── Création ─────────────────────────────────────────────────
void transport_loopback_pair(Transport **pa, Transport **pb) {
    LoopPipe *pipe = calloc(1, sizeof(LoopPipe));
    Transport *a = calloc(1, sizeof(Transport));
    Transport *b = calloc(1, sizeof(Transport));
    if (!pipe || !a || !b) {        // alloc echouee : aucun transport publie
        free(pipe); free(a); free(b);
        *pa = NULL; *pb = NULL;
        return;
    }
    pipe->refs = 2;
    a->kind = 0; a->connected = 1; a->pipe = pipe; a->tx = &pipe->a2b; a->rx = &pipe->b2a;
    b->kind = 0; b->connected = 1; b->pipe = pipe; b->tx = &pipe->b2a; b->rx = &pipe->a2b;
    *pa = a; *pb = b;
}

Transport *transport_tcp_host(int port) {
    net_plat_init();
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == SOCK_INVALID) return NULL;
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0) { closesock(ls); return NULL; }
    if (listen(ls, 1) != 0) { closesock(ls); return NULL; }
    sock_nonblock(ls);
    Transport *t = calloc(1, sizeof(Transport));
    if (!t) { closesock(ls); return NULL; }   // pas de fuite du socket d'ecoute
    t->kind = 1; t->is_host = 1; t->listen_fd = ls; t->fd = SOCK_INVALID;
    return t;
}

Transport *transport_tcp_join(const char *ip, int port) {
    net_plat_init();
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALID) return NULL;
    sock_nonblock(s);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) { closesock(s); return NULL; }
    Transport *t = calloc(1, sizeof(Transport));
    if (!t) { closesock(s); return NULL; }     // pas de fuite du socket
    t->kind = 1; t->is_host = 0; t->listen_fd = SOCK_INVALID; t->fd = s;
    t->connect_deadline = time(NULL) + 8;   // abandon après 8 s sans connexion
    int r = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (r == 0)              t->connected = 1;
    else if (SOCK_INPROGRESS) t->connecting = 1;
    else                      t->failed = 1;
    return t;
}

Transport *transport_tcp_relay(const char *ip, int port, const unsigned char room[4]) {
    Transport *t = transport_tcp_join(ip, port);
    if (!t) return NULL;
    t->preamble[0] = 'R'; t->preamble[1] = 'B';
    memcpy(t->preamble + 2, room, 4);   // 'R''B' + room(4) = 6 octets
    t->preamble_len  = 6;
    t->preamble_sent = 0;
    return t;
}

// Convertit "a.b.c.d" → IP (ordre réseau). 1 = OK.
int net_str_to_ip(const char *s, uint32_t *ip_be) {
    net_plat_init();
    struct in_addr a;
    if (!s || inet_pton(AF_INET, s, &a) != 1) return 0;
    if (ip_be) *ip_be = a.s_addr;
    return 1;
}

// ── Mise à jour (accept/connect + tirage des octets) ─────────
static void tcp_pull(Transport *t) {
    if (t->fd == SOCK_INVALID || !t->connected) return;
    for (;;) {
        if (t->asmlen >= ASM_CAP) break;
        int n = recv(t->fd, (char *)t->asmbuf + t->asmlen, ASM_CAP - t->asmlen, 0);
        if (n > 0)       { t->asmlen += n; }
        else if (n == 0) { t->failed = 1; break; }            // pair fermé
        else             { if (!SOCK_WOULDBLOCK) t->failed = 1; break; }
    }
}

void transport_update(Transport *t) {
    if (!t || t->failed) return;
    if (t->kind == 0) {
        int n;
        do {
            int space = ASM_CAP - t->asmlen;
            if (space <= 0) break;
            n = ring_read(t->rx, t->asmbuf + t->asmlen, space);
            t->asmlen += n;
        } while (n > 0);
        return;
    }
    // ── TCP ──
    if (t->is_host && t->fd == SOCK_INVALID) {
        sock_t c = accept(t->listen_fd, NULL, NULL);
        if (c != SOCK_INVALID) {
            sock_nonblock(c);
            int one = 1;
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
            t->fd = c; t->connected = 1;
        }
    }
    if (t->connecting && t->fd != SOCK_INVALID) {
        // Surveille l'écriture (succès) ET l'exception (échec — indispensable
        // sous Winsock : un connect raté arrive dans le set "except").
        fd_set wf, ef;
        FD_ZERO(&wf); FD_ZERO(&ef);
        FD_SET(t->fd, &wf); FD_SET(t->fd, &ef);
        struct timeval tv = {0, 0};
        int r = select((int)t->fd + 1, NULL, &wf, &ef, &tv);
        if (r > 0) {
            if (FD_ISSET(t->fd, &ef)) {
                t->failed = 1;
            } else {
                int err = 0; socklen_t l = sizeof(err);
                getsockopt(t->fd, SOL_SOCKET, SO_ERROR, (char *)&err, &l);
                if (err == 0) {
                    int one = 1;
                    setsockopt(t->fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
                    t->connecting = 0; t->connected = 1;
                } else {
                    t->failed = 1;
                }
            }
        }
        if (t->connecting && time(NULL) > t->connect_deadline)
            t->failed = 1;   // timeout : hôte injoignable
    }
    // Préambule relais : émis EN PREMIER dès la connexion établie, avant tout
    // message de session (le relais lit ces 6 octets pour apparier le salon).
    if (t->connected && !t->failed && t->preamble_len && !t->preamble_sent) {
        int sent = 0;
        while (sent < t->preamble_len) {
            int n = send(t->fd, (const char *)t->preamble + sent, t->preamble_len - sent, 0);
            if (n > 0) sent += n;
            else if (SOCK_WOULDBLOCK) continue;
            else { t->failed = 1; break; }
        }
        t->preamble_sent = 1;
    }
    tcp_pull(t);
}

// ── Statut / fermeture ───────────────────────────────────────
int transport_connected(const Transport *t) { return t && t->connected && !t->failed; }
int transport_failed   (const Transport *t) { return t && t->failed; }

void transport_close(Transport *t) {
    if (!t) return;
    if (t->kind == 1) {
        if (t->fd != SOCK_INVALID)        closesock(t->fd);
        if (t->listen_fd != SOCK_INVALID) closesock(t->listen_fd);
    } else if (t->pipe) {
        if (--t->pipe->refs <= 0) free(t->pipe);
    }
    free(t);
}

// ── Messages framés ──────────────────────────────────────────
int transport_send_msg(Transport *t, int type, const void *payload, int len) {
    if (!t || t->failed || !t->connected) return 0;
    if (len < 0 || len > NET_MAX_PAYLOAD) return 0;
    NetMsgHeader h;
    h.magic = NET_MAGIC; h.version = NET_PROTO_VERSION;
    h.type = (uint16_t)type; h.len = (uint16_t)len;
    unsigned char buf[NET_HEADER_SIZE + NET_MAX_PAYLOAD];
    memcpy(buf, &h, NET_HEADER_SIZE);
    if (len > 0 && payload) memcpy(buf + NET_HEADER_SIZE, payload, len);
    int total = NET_HEADER_SIZE + len;

    if (t->kind == 0) return ring_write(t->tx, buf, total) == total;

    int sent = 0;
    while (sent < total) {
        int n = send(t->fd, (const char *)buf + sent, total - sent, 0);
        if (n > 0) sent += n;
        else if (SOCK_WOULDBLOCK) continue;   // tampon plein : on patiente (msgs petits)
        else { t->failed = 1; return 0; }
    }
    return 1;
}

// ── Aides adresse ────────────────────────────────────────────
uint32_t net_local_ip_be(void) {
    net_plat_init();
    // Astuce portable : "connecter" un socket UDP vers une IP publique (sans
    // rien envoyer) fait choisir l'interface sortante → getsockname donne l'IP LAN.
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    uint32_t ip = htonl(0x7F000001u);   // repli 127.0.0.1
    if (s == SOCK_INVALID) return ip;
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
    if (connect(s, (struct sockaddr *)&dst, sizeof(dst)) == 0) {
        struct sockaddr_in local;
        socklen_t l = sizeof(local);
        if (getsockname(s, (struct sockaddr *)&local, &l) == 0)
            ip = local.sin_addr.s_addr;
    }
    closesock(s);
    return ip;
}

void net_ip_to_str(uint32_t ip_be, char out[16]) {
    struct in_addr a;
    a.s_addr = ip_be;
    if (!inet_ntop(AF_INET, &a, out, 16)) {
        out[0] = '\0';
    }
}

int transport_recv_msg(Transport *t, NetMsgHeader *h, void *payload, int maxlen) {
    if (!t || t->asmlen < NET_HEADER_SIZE) return 0;
    NetMsgHeader hh;
    memcpy(&hh, t->asmbuf, NET_HEADER_SIZE);
    if (hh.magic != NET_MAGIC || hh.len > NET_MAX_PAYLOAD) {
        t->asmlen = 0;      // flux désynchronisé → purge défensive
        return 0;
    }
    int total = NET_HEADER_SIZE + hh.len;
    if (t->asmlen < total) return 0;          // message encore incomplet
    if (h) *h = hh;
    int cpy = hh.len; if (cpy > maxlen) cpy = maxlen;
    if (payload && cpy > 0) memcpy(payload, t->asmbuf + NET_HEADER_SIZE, cpy);
    memmove(t->asmbuf, t->asmbuf + total, t->asmlen - total);
    t->asmlen -= total;
    return 1;
}

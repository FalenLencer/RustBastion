/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_relay.c ─ Serveur relais TCP (select-based, sans raylib).
 *  Voir net_relay.h pour le protocole d'appairage. Module isolé/retirable.
 */
#include "net_relay.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET rsock_t;
  #define RSOCK_INVALID  INVALID_SOCKET
  #define rclose         closesocket
  #define RSOCK_WOULDBLK (WSAGetLastError() == WSAEWOULDBLOCK)
  static void rplat_init(void) {
      static int s = 0;
      if (!s) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); s = 1; }
  }
  static void rnonblock(rsock_t s) { u_long m = 1; ioctlsocket(s, FIONBIO, &m); }
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int rsock_t;
  #define RSOCK_INVALID  (-1)
  #define rclose         close
  #define RSOCK_WOULDBLK (errno == EAGAIN || errno == EWOULDBLOCK)
  static void rplat_init(void) {}
  static void rnonblock(rsock_t s) { int f = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, f | O_NONBLOCK); }
#endif

#define RELAY_MAX     64        /* connexions simultanées max          */
#define RELAY_HDR     6         /* 'R''B' + room(4)                    */
#define RELAY_STASH   4096      /* octets gardés avant appairage       */
#define RELAY_BUF     8192      /* taille de recopie                   */
#define RELAY_WAIT_S  180       /* abandon d'un salon orphelin (s)     */

enum { ST_HDR = 0, ST_WAIT = 1, ST_PIPE = 2 };

typedef struct {
    rsock_t       fd;
    int           used;
    int           state;
    unsigned char hdr[RELAY_HDR];
    int           hdrlen;
    uint32_t      room;
    int           peer;            /* index du pair apparié, -1 sinon */
    unsigned char stash[RELAY_STASH];
    int           stashlen;
    time_t        since;
} RClient;

static uint32_t rd_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void rnodelay(rsock_t s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
}

/* Envoi best-effort (borné) ; 0 = échec persistant. */
static int rsend_all(rsock_t fd, const unsigned char *d, int n) {
    int sent = 0, spins = 0;
    while (sent < n) {
        int k = (int)send(fd, (const char *)d + sent, n - sent, 0);
        if (k > 0) { sent += k; spins = 0; }
        else if (RSOCK_WOULDBLK) { if (++spins > 100000) return 0; }
        else return 0;
    }
    return 1;
}

static void rclient_free(RClient *c) {
    if (c->used && c->fd != RSOCK_INVALID) rclose(c->fd);
    memset(c, 0, sizeof(*c));
    c->fd = RSOCK_INVALID; c->peer = -1;
}

/* Ferme un client ET son pair éventuel (fin de partie/déconnexion). */
static void rdisconnect(RClient *cl, int i) {
    int p = cl[i].peer;
    rclient_free(&cl[i]);
    if (p >= 0 && p < RELAY_MAX && cl[p].used) rclient_free(&cl[p]);
    printf("[relay] partie terminee (un joueur s'est deconnecte)\n");
    fflush(stdout);
}

/* Traite l'arrivée de données sur le client i. */
static void rhandle(RClient *cl, int i) {
    RClient *c = &cl[i];

    if (c->state == ST_HDR) {
        int n = (int)recv(c->fd, (char *)c->hdr + c->hdrlen, RELAY_HDR - c->hdrlen, 0);
        if (n <= 0) { if (!RSOCK_WOULDBLK) rclient_free(c); return; }
        c->hdrlen += n;
        if (c->hdrlen < RELAY_HDR) return;
        if (c->hdr[0] != 'R' || c->hdr[1] != 'B') { rclient_free(c); return; }
        c->room = rd_be32(c->hdr + 2);
        /* Cherche un salon en attente avec le même code. */
        int j = -1;
        for (int k = 0; k < RELAY_MAX; k++)
            if (k != i && cl[k].used && cl[k].state == ST_WAIT && cl[k].room == c->room) { j = k; break; }
        if (j >= 0) {
            c->state = ST_PIPE;     c->peer = j;
            cl[j].state = ST_PIPE;  cl[j].peer = i;
            printf("[relay] salon %08X : 2 joueurs relies, partie active\n", c->room);
            fflush(stdout);
            /* Vide le tampon du client qui attendait → vers le nouvel arrivant. */
            if (cl[j].stashlen > 0) {
                if (!rsend_all(c->fd, cl[j].stash, cl[j].stashlen)) { rdisconnect(cl, i); return; }
                cl[j].stashlen = 0;
            }
        } else {
            c->state = ST_WAIT;
            c->since = time(NULL);
        }
        return;
    }

    if (c->state == ST_WAIT) {
        /* Pas encore de pair : on garde les octets (ex. HELLO du client). */
        if (c->stashlen >= RELAY_STASH) { rclient_free(c); return; }
        int n = (int)recv(c->fd, (char *)c->stash + c->stashlen, RELAY_STASH - c->stashlen, 0);
        if (n <= 0) { if (!RSOCK_WOULDBLK) rclient_free(c); return; }
        c->stashlen += n;
        return;
    }

    /* ST_PIPE : recopie vers le pair. */
    {
        unsigned char buf[RELAY_BUF];
        int n = (int)recv(c->fd, (char *)buf, RELAY_BUF, 0);
        if (n <= 0) { if (!RSOCK_WOULDBLK) rdisconnect(cl, i); return; }
        int p = c->peer;
        if (p < 0 || !cl[p].used) { rdisconnect(cl, i); return; }
        if (!rsend_all(cl[p].fd, buf, n)) rdisconnect(cl, i);
    }
}

int net_relay_run(int port) {
#ifdef _WIN32
    /* Le jeu est en sous-système GUI (-mwindows) → pas de console par défaut.
       On en attache une (celle du cmd lanceur, sinon une nouvelle) pour
       afficher l'état du relais et permettre Ctrl+C. */
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    FILE *cf;
    cf = freopen("CONOUT$", "w", stdout); (void)cf;
    cf = freopen("CONOUT$", "w", stderr); (void)cf;
#endif
    rplat_init();
    rsock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == RSOCK_INVALID) return 1;
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons((unsigned short)port);
    if (bind(ls, (struct sockaddr *)&a, sizeof(a)) != 0) { rclose(ls); return 2; }
    if (listen(ls, 8) != 0) { rclose(ls); return 3; }
    rnonblock(ls);

    printf("[relay] en ecoute sur le port %d (Ctrl+C pour arreter)\n", port);
    fflush(stdout);

    RClient cl[RELAY_MAX];
    for (int i = 0; i < RELAY_MAX; i++) { memset(&cl[i], 0, sizeof(cl[i])); cl[i].fd = RSOCK_INVALID; cl[i].peer = -1; }

    for (;;) {
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(ls, &rf);
#ifndef _WIN32
        int maxfd = (int)ls;
#endif
        for (int i = 0; i < RELAY_MAX; i++) {
            if (!cl[i].used) continue;
            FD_SET(cl[i].fd, &rf);
#ifndef _WIN32
            if ((int)cl[i].fd > maxfd) maxfd = (int)cl[i].fd;
#endif
        }
        struct timeval tv = { 1, 0 };
#ifdef _WIN32
        int r = select(0, &rf, NULL, NULL, &tv);
#else
        int r = select(maxfd + 1, &rf, NULL, NULL, &tv);
#endif
        if (r < 0) continue;

        if (FD_ISSET(ls, &rf)) {
            rsock_t c = accept(ls, NULL, NULL);
            if (c != RSOCK_INVALID) {
                int slot = -1;
                for (int i = 0; i < RELAY_MAX; i++) if (!cl[i].used) { slot = i; break; }
                if (slot < 0) { rclose(c); }
                else {
                    rnonblock(c); rnodelay(c);
                    memset(&cl[slot], 0, sizeof(cl[slot]));
                    cl[slot].used = 1; cl[slot].fd = c; cl[slot].state = ST_HDR;
                    cl[slot].peer = -1; cl[slot].since = time(NULL);
                    printf("[relay] connexion entrante (en attente d'appairage)\n");
                    fflush(stdout);
                }
            }
        }
        for (int i = 0; i < RELAY_MAX; i++) {
            if (cl[i].used && FD_ISSET(cl[i].fd, &rf)) rhandle(cl, i);
        }
        /* Ménage : abandonne les salons jamais appariés trop vieux. */
        time_t now = time(NULL);
        for (int i = 0; i < RELAY_MAX; i++)
            if (cl[i].used && cl[i].state == ST_WAIT && (now - cl[i].since) > RELAY_WAIT_S)
                rclient_free(&cl[i]);
    }
    /* jamais atteint */
}

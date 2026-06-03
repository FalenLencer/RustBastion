/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "net_session.h"
#include <string.h>
#include <stdio.h>

// ── Code de session : 6 octets (IP[4] + port[2]) ⇄ base32 ────
// Alphabet Crockford (sans I, L, O, U → pas d'ambiguïté de lecture).
static const char B32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static int b32val(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c == 'I' || c == 'L') c = '1';
    if (c == 'O') c = '0';
    for (int i = 0; i < 32; i++) if (B32[i] == c) return i;
    return -1;
}

void net_session_make_code(uint32_t ip_be, uint16_t port, char out[20]) {
    unsigned char b[6];
    memcpy(b, &ip_be, 4);                 // 4 octets IP (ordre réseau natif)
    b[4] = (unsigned char)(port >> 8);
    b[5] = (unsigned char)(port & 0xFF);
    uint64_t v = 0;
    for (int i = 0; i < 6; i++) v = (v << 8) | b[i];   // 48 bits, MSB d'abord
    char raw[10];
    for (int i = 9; i >= 0; i--) { raw[i] = B32[v & 31]; v >>= 5; }
    int o = 0;
    for (int i = 0; i < 10; i++) { if (i == 5) out[o++] = '-'; out[o++] = raw[i]; }
    out[o] = '\0';
}

int net_session_parse_code(const char *code, uint32_t *ip_be, uint16_t *port) {
    if (!code) return 0;
    int vals[10], n = 0;
    for (const char *p = code; *p && n < 10; p++) {
        if (*p == '-' || *p == ' ') continue;
        int v = b32val(*p);
        if (v < 0) return 0;
        vals[n++] = v;
    }
    if (n != 10) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 10; i++) v = (v << 5) | (unsigned)vals[i];
    unsigned char b[6];
    for (int i = 5; i >= 0; i--) { b[i] = (unsigned char)(v & 0xFF); v >>= 8; }
    if (ip_be) memcpy(ip_be, b, 4);
    if (port)  *port = (uint16_t)(((uint16_t)b[4] << 8) | b[5]);
    return 1;
}

// ── Lobby ────────────────────────────────────────────────────
int net_session_host(NetSession *s, int port, uint32_t seed, uint8_t mode,
                     const char *name) {
    memset(s, 0, sizeof(*s));
    s->is_host = 1; s->seed = seed; s->mode = mode; s->port = port;
    snprintf(s->my_name, sizeof(s->my_name), "%s", name ? name : "Hote");
    s->tr = transport_tcp_host(port);
    if (!s->tr) { s->state = SESS_FAILED; return 0; }
    net_session_make_code(net_local_ip_be(), (uint16_t)port, s->code);
    s->state = SESS_HOSTING;
    return 1;
}

int net_session_join(NetSession *s, const char *code, const char *name) {
    memset(s, 0, sizeof(*s));
    s->is_host = 0;
    snprintf(s->my_name, sizeof(s->my_name), "%s", name ? name : "Client");
    uint32_t ip; uint16_t port;
    if (!net_session_parse_code(code, &ip, &port)) { s->state = SESS_FAILED; return 0; }
    s->port = port;
    char ipstr[16];
    net_ip_to_str(ip, ipstr);
    s->tr = transport_tcp_join(ipstr, port);
    if (!s->tr) { s->state = SESS_FAILED; return 0; }
    snprintf(s->code, sizeof(s->code), "%s", code);
    s->state = SESS_CONNECTING;
    return 1;
}

// Copie un champ nom REÇU (NET_NAME_LEN octets) en garantissant la
// terminaison (un pair malveillant pourrait l'omettre) → lecture bornée.
static void copy_peer_name(char dst[NET_NAME_LEN], const char *src) {
    memcpy(dst, src, NET_NAME_LEN);
    dst[NET_NAME_LEN - 1] = '\0';
}

static void send_hello(NetSession *s) {
    NetHello h;
    h.proto = NET_PROTO_VERSION;
    snprintf(h.name, sizeof(h.name), "%s", s->my_name);
    transport_send_msg(s->tr, NMSG_HELLO, &h, sizeof(h));
}

void net_session_update(NetSession *s) {
    if (!s || !s->tr || s->state == SESS_FAILED || s->state == SESS_CLOSED) return;
    transport_update(s->tr);
    if (transport_failed(s->tr)) {
        s->peer_gone = 1;
        if (s->state < SESS_LOBBY) s->state = SESS_FAILED;
        return;
    }
    // Connexion établie → handshake
    if ((s->state == SESS_HOSTING || s->state == SESS_CONNECTING) &&
        transport_connected(s->tr)) {
        s->state = SESS_HANDSHAKE;
        if (!s->is_host) send_hello(s);   // le client se présente
    }
    if (s->state == SESS_INGAME) return;  // en jeu : le jeu draine via net_session_recv

    NetMsgHeader h;
    unsigned char pl[NET_MAX_PAYLOAD];
    while (transport_recv_msg(s->tr, &h, pl, sizeof(pl))) {
        switch (h.type) {
            case NMSG_HELLO:
                if (s->is_host && h.len >= (int)sizeof(NetHello)) {
                    NetHello *hi = (NetHello *)pl;
                    copy_peer_name(s->peer_name, hi->name);
                    NetWelcome w;
                    w.seed = s->seed; w.mode = s->mode;
                    w.accepted = (hi->proto == NET_PROTO_VERSION) ? 1 : 0;
                    snprintf(w.host_name, sizeof(w.host_name), "%s", s->my_name);
                    transport_send_msg(s->tr, NMSG_WELCOME, &w, sizeof(w));
                    s->state = SESS_LOBBY;
                }
                break;
            case NMSG_WELCOME:
                if (!s->is_host && h.len >= (int)sizeof(NetWelcome)) {
                    NetWelcome *w = (NetWelcome *)pl;
                    if (!w->accepted) { s->state = SESS_FAILED; break; }
                    s->seed = w->seed; s->mode = w->mode;
                    copy_peer_name(s->peer_name, w->host_name);
                    s->state = SESS_LOBBY;
                }
                break;
            case NMSG_READY:
                if (h.len >= (int)sizeof(NetReady)) s->peer_ready = ((NetReady *)pl)->ready;
                break;
            case NMSG_START:
                if (!s->is_host) { s->started = 1; s->state = SESS_INGAME; }
                break;
            case NMSG_BYE:
                s->peer_gone = 1;
                break;
            default: break;
        }
    }
}

void net_session_set_ready(NetSession *s, int ready) {
    if (!s || !s->tr) return;
    s->my_ready = ready ? 1 : 0;
    NetReady r; r.ready = (uint8_t)s->my_ready;
    transport_send_msg(s->tr, NMSG_READY, &r, sizeof(r));
}

void net_session_start(NetSession *s) {
    if (!s || !s->is_host || s->state != SESS_LOBBY) return;
    if (!s->my_ready || !s->peer_ready) return;
    transport_send_msg(s->tr, NMSG_START, NULL, 0);
    s->started = 1;
    s->state = SESS_INGAME;
}

int net_session_send(NetSession *s, int type, const void *payload, int len) {
    if (!s || !s->tr) return 0;
    return transport_send_msg(s->tr, type, payload, len);
}

int net_session_recv(NetSession *s, NetMsgHeader *h, void *payload, int maxlen) {
    if (!s || !s->tr) return 0;
    NetMsgHeader hh;
    unsigned char tmp[NET_MAX_PAYLOAD];
    while (transport_recv_msg(s->tr, &hh, tmp, sizeof(tmp))) {
        if (hh.type == NMSG_BYE)  { s->peer_gone = 1; continue; }
        if (hh.type == NMSG_PING) continue;
        if (h) *h = hh;
        int c = hh.len; if (c > maxlen) c = maxlen;
        if (payload && c > 0) memcpy(payload, tmp, c);
        return 1;
    }
    return 0;
}

void net_session_close(NetSession *s) {
    if (!s) return;
    if (s->tr) {
        if (transport_connected(s->tr)) transport_send_msg(s->tr, NMSG_BYE, NULL, 0);
        transport_close(s->tr);
        s->tr = NULL;
    }
    s->state = SESS_CLOSED;
}

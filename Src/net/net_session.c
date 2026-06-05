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

// Encode/décode base32 génériques sur un flux d'octets.
static int b32_encode(const unsigned char *b, int n, char *out) {
    int bits = 0, val = 0, o = 0;
    for (int i = 0; i < n; i++) {
        val = (val << 8) | b[i]; bits += 8;
        while (bits >= 5) { out[o++] = B32[(val >> (bits - 5)) & 31]; bits -= 5; }
    }
    if (bits > 0) out[o++] = B32[(val << (5 - bits)) & 31];
    out[o] = '\0';
    return o;
}
static int b32_decode(const char *code, unsigned char *out, int outn) {
    int bits = 0, val = 0, o = 0;
    for (const char *p = code; *p; p++) {
        if (*p == '-' || *p == ' ') continue;
        int v = b32val(*p);
        if (v < 0) return -1;
        val = (val << 5) | v; bits += 5;
        if (bits >= 8) { if (o < outn) out[o++] = (unsigned char)((val >> (bits - 8)) & 0xFF); bits -= 8; }
    }
    return o;
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

// ── Code RELAIS : 10 octets (relayIP[4] + port[2] + room[4]) ──
void net_session_make_relay_code(uint32_t ip_be, uint16_t port, uint32_t room, char out[20]) {
    unsigned char b[10];
    memcpy(b, &ip_be, 4);
    b[4] = (unsigned char)(port >> 8);  b[5] = (unsigned char)(port & 0xFF);
    b[6] = (unsigned char)(room >> 24); b[7] = (unsigned char)(room >> 16);
    b[8] = (unsigned char)(room >> 8);  b[9] = (unsigned char)(room & 0xFF);
    char raw[20];
    int n = b32_encode(b, 10, raw);     // 16 caractères
    int o = 0;
    for (int i = 0; i < n; i++) { if (i == 8) out[o++] = '-'; out[o++] = raw[i]; }
    out[o] = '\0';
}

int net_session_parse_relay_code(const char *code, uint32_t *ip_be, uint16_t *port, uint32_t *room) {
    if (!code) return 0;
    unsigned char b[10];
    if (b32_decode(code, b, 10) != 10) return 0;
    if (ip_be) memcpy(ip_be, b, 4);
    if (port)  *port = (uint16_t)(((uint16_t)b[4] << 8) | b[5]);
    if (room)  *room = ((uint32_t)b[6] << 24) | ((uint32_t)b[7] << 16) |
                       ((uint32_t)b[8] << 8)  |  (uint32_t)b[9];
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

// ── Mode relais : les DEUX se connectent au relais (rôles inchangés) ──
static void room_bytes(uint32_t room, unsigned char rb[4]) {
    rb[0] = (unsigned char)(room >> 24); rb[1] = (unsigned char)(room >> 16);
    rb[2] = (unsigned char)(room >> 8);  rb[3] = (unsigned char)(room & 0xFF);
}

int net_session_host_relay(NetSession *s, uint32_t relay_ip, uint16_t relay_port,
                           uint32_t room, uint32_t seed, uint8_t mode, const char *name) {
    memset(s, 0, sizeof(*s));
    s->is_host = 1; s->seed = seed; s->mode = mode; s->port = relay_port;
    snprintf(s->my_name, sizeof(s->my_name), "%s", name ? name : "Hote");
    unsigned char rb[4]; room_bytes(room, rb);
    char ipstr[16]; net_ip_to_str(relay_ip, ipstr);
    s->tr = transport_tcp_relay(ipstr, relay_port, rb);
    if (!s->tr) { s->state = SESS_FAILED; return 0; }
    net_session_make_relay_code(relay_ip, relay_port, room, s->code);
    s->state = SESS_CONNECTING;          // l'hôte se connecte AUSSI au relais
    return 1;
}

int net_session_join_relay(NetSession *s, const char *code, const char *name) {
    memset(s, 0, sizeof(*s));
    s->is_host = 0;
    snprintf(s->my_name, sizeof(s->my_name), "%s", name ? name : "Client");
    uint32_t ip, room; uint16_t port;
    if (!net_session_parse_relay_code(code, &ip, &port, &room)) { s->state = SESS_FAILED; return 0; }
    s->port = port;
    unsigned char rb[4]; room_bytes(room, rb);
    char ipstr[16]; net_ip_to_str(ip, ipstr);
    s->tr = transport_tcp_relay(ipstr, port, rb);
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
                if (!s->is_host) {
                    if (h.len >= 4) memcpy(&s->seed, pl, 4);  // adopte le seed de l'hôte
                    int ex = (int)h.len - 4;                  // config de partie jointe
                    if (ex < 0) ex = 0;
                    if (ex > (int)sizeof(s->start_extra)) ex = (int)sizeof(s->start_extra);
                    if (ex > 0) memcpy(s->start_extra, pl + 4, ex);
                    s->start_extra_len = ex;
                    s->started = 1; s->state = SESS_INGAME;
                }
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

void net_session_start(NetSession *s, const void *extra, int extra_len) {
    if (!s || !s->is_host || s->state != SESS_LOBBY) return;
    if (!s->my_ready || !s->peer_ready) return;
    if (extra_len < 0) extra_len = 0;
    if (extra_len > (int)sizeof(s->start_extra)) extra_len = (int)sizeof(s->start_extra);
    unsigned char buf[4 + sizeof(s->start_extra)];
    memcpy(buf, &s->seed, 4);                 // seed (frais à chaque lancement) → carte commune
    if (extra && extra_len > 0) memcpy(buf + 4, extra, extra_len);
    transport_send_msg(s->tr, NMSG_START, buf, 4 + extra_len);
    // Mémorise l'extra côté hôte (mp_launch_game lira start_extra des 2 côtés).
    if (extra && extra_len > 0) memcpy(s->start_extra, extra, extra_len);
    s->start_extra_len = extra_len;
    s->started = 1;
    s->state = SESS_INGAME;
}

// Retour au salon entre deux parties (rematch) : conserve la connexion,
// réinitialise prêts + état. Sans effet si le pair est parti.
void net_session_rematch(NetSession *s) {
    if (!s || s->peer_gone) return;
    s->my_ready   = 0;
    s->peer_ready = 0;
    s->started    = 0;
    if (s->state == SESS_INGAME) s->state = SESS_LOBBY;
}

// 1 = pair connecté et présent (pour proposer un rematch).
int net_session_connected(const NetSession *s) {
    return s && s->tr && !s->peer_gone &&
           (s->state == SESS_LOBBY || s->state == SESS_INGAME);
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

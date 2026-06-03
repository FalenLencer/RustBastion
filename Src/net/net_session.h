/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_session.h ─ Lobby multijoueur : héberger / rejoindre par CODE,
 *  handshake (HELLO/WELCOME/READY/START), partage du seed + mode.
 *
 *  Le CODE de session encode l'IP:port de l'hôte (base32). Marche tout de
 *  suite en localhost et en LAN. (Cross-réseau « sans config » = relais, phase
 *  online ultérieure : seul ce qu'encode le code changera.)
 */
#pragma once
#include "net_transport.h"

typedef enum {
    SESS_IDLE = 0,
    SESS_HOSTING,     // hôte : attend qu'un pair rejoigne
    SESS_CONNECTING,  // client : connexion TCP en cours
    SESS_HANDSHAKE,   // connecté, échange HELLO/WELCOME
    SESS_LOBBY,       // dans le lobby (ready-up)
    SESS_INGAME,      // partie lancée
    SESS_FAILED,      // erreur (refus, perte, code invalide)
    SESS_CLOSED,
} SessState;

typedef struct {
    Transport *tr;
    SessState  state;
    int        is_host;
    uint32_t   seed;          // seed commun (équité)
    uint8_t    mode;          // MpMode
    char       my_name[NET_NAME_LEN];
    char       peer_name[NET_NAME_LEN];
    int        my_ready;
    int        peer_ready;
    int        started;       // 1 = START reçu/émis → passage en jeu
    int        peer_gone;     // 1 = pair déconnecté
    char       code[20];      // code de session (rempli côté hôte)
    int        port;
} NetSession;

// Hôte : écoute sur `port`, fixe seed+mode, génère s->code. 1 = OK.
int  net_session_host(NetSession *s, int port, uint32_t seed, uint8_t mode,
                      const char *name);
// Client : rejoint via un code. 1 = lancement OK (connexion async ensuite).
int  net_session_join(NetSession *s, const char *code, const char *name);
// Chaque frame : avance accept/connect + handshake + lecture messages.
void net_session_update(NetSession *s);
// Bascule l'état "prêt" (et l'envoie au pair).
void net_session_set_ready(NetSession *s, int ready);
// Hôte uniquement : lance la partie si les deux sont prêts.
void net_session_start(NetSession *s);
// Envoi/réception de messages applicatifs (status, etc.) une fois en jeu.
int  net_session_send(NetSession *s, int type, const void *payload, int len);
int  net_session_recv(NetSession *s, NetMsgHeader *h, void *payload, int maxlen);
void net_session_close(NetSession *s);

// ── Code de session (IP:port ⇄ code base32) ──────────────────
void net_session_make_code(uint32_t ip_be, uint16_t port, char out[20]);
int  net_session_parse_code(const char *code, uint32_t *ip_be, uint16_t *port);

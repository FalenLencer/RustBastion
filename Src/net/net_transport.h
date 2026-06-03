/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_transport.h ─ Couche de transport abstraite (1 connexion duplex).
 *
 *  Deux backends interchangeables, MÊME API :
 *    • loopback  : deux extrémités reliées en mémoire (auto-test headless) ;
 *    • TCP       : sockets non bloquants, cross-plateforme (WSL/Linux + Win).
 *
 *  Le gameplay ne voit QUE cette interface → on change de backend (test ↔
 *  réseau réel) sans toucher à la logique des modes.
 */
#pragma once
#include "net_protocol.h"

typedef struct Transport Transport;

// ── Création ─────────────────────────────────────────────────
// Loopback : crée deux extrémités reliées (a ↔ b). À libérer via transport_close.
void  transport_loopback_pair(Transport **a, Transport **b);
// TCP hôte : écoute sur `port` (non bloquant). NULL si échec.
Transport *transport_tcp_host(int port);
// TCP client : se connecte à ip:port (non bloquant). NULL si échec d'init.
Transport *transport_tcp_join(const char *ip, int port);

// ── Cycle de vie ─────────────────────────────────────────────
// Fait avancer accept/connect et tire les octets entrants en tampon.
// À appeler chaque frame avant transport_recv_msg().
void  transport_update(Transport *t);
// 1 = pair connecté (accepté / connexion établie).
int   transport_connected(const Transport *t);
// 1 = erreur fatale (socket fermé/refusé).
int   transport_failed(const Transport *t);
void  transport_close(Transport *t);

// ── Messages framés ──────────────────────────────────────────
// Envoie un message (en-tête + payload). Retourne 1 si OK.
int   transport_send_msg(Transport *t, int type, const void *payload, int len);
// Récupère un message complet si dispo (remplit *h et payload). 1 = obtenu.
// Appeler en boucle après transport_update() pour vider la file.
int   transport_recv_msg(Transport *t, NetMsgHeader *h, void *payload, int maxlen);

// ── Aides adresse (pour le code de session) ──────────────────
// IP LAN locale (ordre réseau / big-endian) ; 127.0.0.1 en repli.
uint32_t net_local_ip_be(void);
// Convertit une IP (big-endian) en chaîne "a.b.c.d".
void  net_ip_to_str(uint32_t ip_be, char out[16]);

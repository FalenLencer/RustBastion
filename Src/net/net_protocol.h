/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_protocol.h ─ Format des messages multijoueur (framés, versionnés).
 *
 *  Chaque message = en-tête fixe (NetMsgHeader) + payload de taille `len`.
 *  Les structs sont `packed` (pas de padding) → envoi/réception bruts sûrs
 *  EN LOCAL et entre machines de même architecture (localhost / LAN).
 *  NOTE : pour l'online cross-plateforme final, sérialiser explicitement en
 *  ordre réseau (htonl/htons). Suffisant ici pour la phase localhost.
 */
#pragma once
#include <stdint.h>

#define NET_MAGIC          0x52424E54u   /* "RBNT" */
#define NET_PROTO_VERSION  1
#define NET_MAX_PAYLOAD    512
#define NET_NAME_LEN       24

// ── Types de message ─────────────────────────────────────────
typedef enum {
    NMSG_HELLO    = 1,   // join → host : version + nom
    NMSG_WELCOME,        // host → join : seed + mode + accepté
    NMSG_READY,          // ↔        : bascule "prêt"
    NMSG_START,          // host → all: lancement (compte à rebours fini)
    NMSG_STATUS,         // ↔ péri.  : vague/kills/or/vies/vivant (mode Course)
    NMSG_GAMEOVER,       // ↔        : board tombé / extrait
    NMSG_PING,           // ↔        : maintien de connexion
    NMSG_BYE,            // ↔        : déconnexion propre
    NMSG_SEND_ENEMY,     // Duel     : injecte un ennemi chez l'adversaire (payload : 1 octet EnemyType)
    NMSG_AID,            // Co-op    : transfert d'or au partenaire (payload : int32)
    NMSG_BOARD,          // Asym     : snapshot plateau défenseur → envahisseur
                         //   payload : [nt][nt×(tx,ty,type)][nu][nu×(tx,ty,type)]
    // Modes futurs : NMSG_COMMAND, NMSG_SNAPSHOT…
} NetMsgType;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint16_t len;        // taille du payload qui suit (octets)
} NetMsgHeader;

typedef struct {
    uint16_t proto;
    char     name[NET_NAME_LEN];
} NetHello;

typedef struct {
    uint32_t seed;       // seed commun (équité des deux boards)
    uint8_t  mode;       // MpMode
    uint8_t  accepted;   // 1 = rejoint accepté
    char     host_name[NET_NAME_LEN];
} NetWelcome;

typedef struct {
    uint8_t  ready;
} NetReady;

typedef struct {
    int32_t  wave;
    int32_t  kills;
    int32_t  gold;
    int32_t  lives;
    uint8_t  alive;      // 0 = board tombé
} NetStatus;
#pragma pack(pop)

#define NET_HEADER_SIZE  ((int)sizeof(NetMsgHeader))

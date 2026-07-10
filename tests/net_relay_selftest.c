/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 *
 * Test BOUT-EN-BOUT du relais (HORS Makefile — possède son main()).
 * Lance net_relay_run dans un thread sur 127.0.0.1, crée deux sessions
 * (hôte + client) qui passent PAR le relais, et vérifie :
 *   handshake (LOBBY + noms) → ready → START (INGAME) → transfert d'un message.
 *
 * Compilation (depuis la racine) :
 *   gcc -Wall -Wextra -pthread Src/net/net_transport.c Src/net/net_session.c \
 *       Src/net/net_relay.c Src/net/net_relay_selftest.c -o /tmp/relay_test \
 *   && /tmp/relay_test
 */
#include "net_session.h"
#include "net_relay.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define RELAY_PORT 47820

static void *relay_thread(void *arg) {
    (void)arg;
    net_relay_run(RELAY_PORT);   // boucle infinie ; meurt avec le process
    return NULL;
}

/* Pompe les deux sessions jusqu'à ce que `cond` soit vrai ou timeout (ms). */
static int pump_until(NetSession *a, NetSession *b, int (*cond)(NetSession*, NetSession*), int ms) {
    struct timespec ts = {0, 5 * 1000 * 1000};   // 5 ms
    for (int t = 0; t < ms; t += 5) {
        net_session_update(a);
        net_session_update(b);
        if (cond(a, b)) return 1;
        nanosleep(&ts, NULL);
    }
    return 0;
}

static int both_lobby(NetSession *a, NetSession *b) {
    return a->state == SESS_LOBBY && b->state == SESS_LOBBY;
}
static int both_ingame(NetSession *a, NetSession *b) {
    return a->state == SESS_INGAME && b->state == SESS_INGAME;
}
static int peer_ready_both(NetSession *a, NetSession *b) {
    return a->peer_ready && b->peer_ready;
}

int main(void) {
    printf("=== net_relay : test bout-en-bout ===\n");
    int fails = 0;

    pthread_t th;
    pthread_create(&th, NULL, relay_thread, NULL);
    pthread_detach(th);
    usleep(200 * 1000);   // laisse le relais binder/écouter

    uint32_t relay_ip;
    net_str_to_ip("127.0.0.1", &relay_ip);
    uint32_t room = 0xC0FFEE42u;
    uint32_t seed = 12345;

    NetSession host, join;
    if (!net_session_host_relay(&host, relay_ip, RELAY_PORT, room, seed, 1, "Hote")) {
        printf("  [X] host_relay echoue\n"); return 1;
    }
    printf("  code relais : %s\n", host.code);

    /* Le client rejoint via le code généré par l'hôte. */
    if (!net_session_join_relay(&join, host.code, "Client")) {
        printf("  [X] join_relay echoue (parse code ?)\n"); return 1;
    }

    /* 1) Handshake → LOBBY */
    if (pump_until(&host, &join, both_lobby, 5000))
        printf("  [OK] handshake : les deux en LOBBY\n");
    else { printf("  [X] pas de LOBBY (host=%d join=%d)\n", host.state, join.state); fails++; }

    /* 2) Noms échangés via le relais */
    if (strcmp(host.peer_name, "Client") == 0 && strcmp(join.peer_name, "Hote") == 0)
        printf("  [OK] noms echanges (host voit '%s', join voit '%s')\n", host.peer_name, join.peer_name);
    else { printf("  [X] noms : host='%s' join='%s'\n", host.peer_name, join.peer_name); fails++; }

    /* 3) Seed propagé */
    if (join.seed == seed) printf("  [OK] seed propage (%u)\n", join.seed);
    else { printf("  [X] seed join=%u attendu %u\n", join.seed, seed); fails++; }

    /* 4) Ready des deux côtés → START (avec config jointe) → INGAME */
    net_session_set_ready(&host, 1);
    net_session_set_ready(&join, 1);
    pump_until(&host, &join, peer_ready_both, 2000);
    unsigned char cfg_extra[12] = { 3, 0,0,0, 7, 0,0,0, 42, 0, 0, 1 };  // config factice + rôle
    net_session_start(&host, cfg_extra, sizeof(cfg_extra));
    if (pump_until(&host, &join, both_ingame, 3000))
        printf("  [OK] START : les deux INGAME\n");
    else { printf("  [X] pas INGAME (host=%d join=%d)\n", host.state, join.state); fails++; }

    /* 4b) La config jointe (start_extra) a-t-elle traversé le relais ? */
    if (join.start_extra_len == (int)sizeof(cfg_extra) &&
        memcmp(join.start_extra, cfg_extra, sizeof(cfg_extra)) == 0)
        printf("  [OK] config de partie transmise via START (%d octets)\n", join.start_extra_len);
    else { printf("  [X] config NON transmise (len=%d)\n", join.start_extra_len); fails++; }

    /* 5) Transfert d'un message applicatif hôte → client À TRAVERS le relais */
    NetStatus st = { .wave = 7, .kills = 42, .gold = 100, .lives = 3, .alive = 1 };
    net_session_send(&host, NMSG_STATUS, &st, sizeof(st));
    int got = 0;
    struct timespec ts = {0, 5 * 1000 * 1000};
    for (int t = 0; t < 2000 && !got; t += 5) {
        net_session_update(&host);
        net_session_update(&join);
        NetMsgHeader h; unsigned char pl[NET_MAX_PAYLOAD];
        if (net_session_recv(&join, &h, pl, sizeof(pl)) && h.type == NMSG_STATUS) {
            NetStatus *r = (NetStatus *)pl;
            if (r->wave == 7 && r->kills == 42 && r->lives == 3) got = 1;
        }
        nanosleep(&ts, NULL);
    }
    if (got) printf("  [OK] message transfere par le relais (wave=7 kills=42)\n");
    else { printf("  [X] message NON recu via le relais\n"); fails++; }

    net_session_close(&host);
    net_session_close(&join);

    printf("=== %s ===\n", fails == 0 ? "TOUT OK (0 echec)" : "ECHECS DETECTES");
    return fails == 0 ? 0 : 1;
}

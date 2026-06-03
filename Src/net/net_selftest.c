/*
 * RUST BASTION — auto-test headless du transport réseau.
 * Compilation autonome (hors jeu) :
 *   gcc -Wall -Wextra Src/net/net_transport.c Src/net/net_selftest.c -o /tmp/net_test
 *   /tmp/net_test
 * Vérifie loopback + TCP localhost SANS rendu (respecte « pas d'affichage »).
 */
#include "net_transport.h"
#include "net_session.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [OK] %s\n", msg); } \
    else      { printf("  [FAIL] %s\n", msg); fails++; } } while (0)

// Pompe jusqu'à obtenir un message du type attendu (ou abandon après N essais).
static int pump_until(Transport *t, int want_type, void *payload, int maxlen) {
    for (int i = 0; i < 100000; i++) {
        transport_update(t);
        NetMsgHeader h;
        if (transport_recv_msg(t, &h, payload, maxlen)) {
            if (h.type == want_type) return 1;
        }
        if (transport_failed(t)) return 0;
    }
    return 0;
}

static void test_loopback(void) {
    printf("== Loopback ==\n");
    Transport *a, *b;
    transport_loopback_pair(&a, &b);
    CHECK(transport_connected(a) && transport_connected(b), "paire reliee");

    NetHello hello = { NET_PROTO_VERSION, "joueur_A" };
    CHECK(transport_send_msg(a, NMSG_HELLO, &hello, sizeof(hello)), "A envoie HELLO");

    NetHello got;
    memset(&got, 0, sizeof(got));
    CHECK(pump_until(b, NMSG_HELLO, &got, sizeof(got)), "B recoit HELLO");
    CHECK(got.proto == NET_PROTO_VERSION && strcmp(got.name, "joueur_A") == 0,
          "payload HELLO intact");

    NetWelcome wel = { 12345u, 3 /*Course*/, 1, "hote_B" };
    transport_send_msg(b, NMSG_WELCOME, &wel, sizeof(wel));
    NetWelcome gw; memset(&gw, 0, sizeof(gw));
    CHECK(pump_until(a, NMSG_WELCOME, &gw, sizeof(gw)), "A recoit WELCOME");
    CHECK(gw.seed == 12345u && gw.mode == 3 && gw.accepted == 1, "payload WELCOME intact");

    // Rafale : 50 STATUS de suite, tous reçus dans l'ordre
    for (int i = 0; i < 50; i++) {
        NetStatus st = { i, i * 2, 100 + i, 20, 1 };
        transport_send_msg(a, NMSG_STATUS, &st, sizeof(st));
    }
    int recv_count = 0, ordered = 1;
    for (int guard = 0; guard < 200; guard++) {
        transport_update(b);
        NetMsgHeader h; NetStatus st;
        while (transport_recv_msg(b, &h, &st, sizeof(st))) {
            if (h.type == NMSG_STATUS) {
                if (st.wave != recv_count) ordered = 0;
                recv_count++;
            }
        }
        if (recv_count >= 50) break;
    }
    CHECK(recv_count == 50 && ordered, "rafale de 50 STATUS recue dans l'ordre");

    transport_close(a);
    transport_close(b);
}

static void test_tcp_localhost(void) {
    printf("== TCP localhost ==\n");
    const int PORT = 47711;
    Transport *host = transport_tcp_host(PORT);
    CHECK(host != NULL, "hote en ecoute");
    Transport *join = transport_tcp_join("127.0.0.1", PORT);
    CHECK(join != NULL, "client lance la connexion");
    if (!host || !join) { fails++; return; }

    // Pompe la connexion (accept + connect)
    int connected = 0;
    for (int i = 0; i < 100000 && !connected; i++) {
        transport_update(host);
        transport_update(join);
        if (transport_connected(host) && transport_connected(join)) connected = 1;
        if (transport_failed(host) || transport_failed(join)) break;
    }
    CHECK(connected, "connexion TCP etablie");

    if (connected) {
        NetHello hello = { NET_PROTO_VERSION, "client" };
        transport_send_msg(join, NMSG_HELLO, &hello, sizeof(hello));
        NetHello got; memset(&got, 0, sizeof(got));
        CHECK(pump_until(host, NMSG_HELLO, &got, sizeof(got)), "hote recoit HELLO via socket");
        CHECK(strcmp(got.name, "client") == 0, "payload HELLO TCP intact");

        NetWelcome wel = { 9999u, 3, 1, "hote" };
        transport_send_msg(host, NMSG_WELCOME, &wel, sizeof(wel));
        NetWelcome gw; memset(&gw, 0, sizeof(gw));
        CHECK(pump_until(join, NMSG_WELCOME, &gw, sizeof(gw)), "client recoit WELCOME via socket");
        CHECK(gw.seed == 9999u && gw.accepted == 1, "payload WELCOME TCP intact");
    }

    transport_close(host);
    transport_close(join);
}

static void pump2(NetSession *a, NetSession *b) {
    net_session_update(a);
    net_session_update(b);
}

static void test_session(void) {
    printf("== Code de session ==\n");
    unsigned char ipb[4] = {192, 168, 1, 42};
    uint32_t ip; memcpy(&ip, ipb, 4);
    char code[20];
    net_session_make_code(ip, 47000, code);
    printf("  code genere : %s\n", code);
    uint32_t ip2; uint16_t port2;
    CHECK(net_session_parse_code(code, &ip2, &port2), "code parse");
    CHECK(ip2 == ip && port2 == 47000, "round-trip IP:port exact");
    CHECK(!net_session_parse_code("trop-court", &ip2, &port2), "code invalide rejete");

    printf("== Lobby (handshake TCP localhost) ==\n");
    const int PORT = 47712;
    NetSession host, join;
    int oh = net_session_host(&host, PORT, 4242u, 3 /*Course*/, "Hote");
    CHECK(oh, "hote demarre");
    // Code forcé sur 127.0.0.1 pour l'auto-test mono-machine
    unsigned char lo[4] = {127, 0, 0, 1};
    uint32_t loip; memcpy(&loip, lo, 4);
    char lcode[20]; net_session_make_code(loip, PORT, lcode);
    int oj = net_session_join(&join, lcode, "Client");
    CHECK(oj, "client rejoint via code");

    int ok = 0;
    for (int i = 0; i < 100000 && !ok; i++) {
        pump2(&host, &join);
        if (host.state == SESS_LOBBY && join.state == SESS_LOBBY) ok = 1;
        if (host.state == SESS_FAILED || join.state == SESS_FAILED) break;
    }
    CHECK(ok, "les deux dans le LOBBY");
    CHECK(join.seed == 4242u && join.mode == 3, "seed + mode propages au client");
    CHECK(strcmp(host.peer_name, "Client") == 0 &&
          strcmp(join.peer_name, "Hote") == 0, "noms echanges");

    net_session_set_ready(&host, 1);
    net_session_set_ready(&join, 1);
    int both_ready = 0;
    for (int i = 0; i < 100000 && !both_ready; i++) {
        pump2(&host, &join);
        if (host.peer_ready && join.peer_ready) both_ready = 1;
    }
    CHECK(both_ready, "ready-up propage");

    net_session_start(&host);
    int started = 0;
    for (int i = 0; i < 100000 && !started; i++) {
        pump2(&host, &join);
        if (host.started && join.started) started = 1;
    }
    CHECK(started && host.state == SESS_INGAME && join.state == SESS_INGAME,
          "partie lancee (START recu)");

    // Échange d'un STATUS en jeu
    NetStatus st = {7, 14, 175, 20, 1};
    net_session_send(&host, NMSG_STATUS, &st, sizeof(st));
    NetStatus gr; memset(&gr, 0, sizeof(gr));
    int got = 0;
    for (int i = 0; i < 100000 && !got; i++) {
        net_session_update(&join);
        NetMsgHeader h;
        if (net_session_recv(&join, &h, &gr, sizeof(gr)) && h.type == NMSG_STATUS) got = 1;
    }
    CHECK(got && gr.wave == 7 && gr.gold == 175, "STATUS applicatif recu en jeu");

    net_session_close(&host);
    net_session_close(&join);
}

int main(void) {
    printf("=== RUST BASTION — auto-test reseau ===\n");
    test_loopback();
    test_tcp_localhost();
    test_session();
    printf("=== %s (%d echec%s) ===\n",
           fails == 0 ? "TOUT OK" : "ECHECS", fails, fails > 1 ? "s" : "");
    return fails == 0 ? 0 : 1;
}

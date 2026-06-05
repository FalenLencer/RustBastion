/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 *
 * Test FUMÉE autonome de net_upnp (HORS Makefile — possède son main()).
 * But : vérifier que net_upnp_map_tcp s'exécute SANS bloquer ni crasher,
 * et échoue proprement là où la box ne supporte pas UPnP/NAT-PMP (WSL).
 *
 * Compilation (depuis la racine du dépôt) :
 *   gcc -Wall -Wextra Src/net/net_transport.c Src/net/net_upnp.c \
 *       Src/net/net_upnp_selftest.c -o /tmp/upnp_test && /tmp/upnp_test
 */
#include "net_upnp.h"
#include "net_transport.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    printf("=== net_upnp : test fumee ===\n");
    time_t t0 = time(NULL);

    UpnpResult r;
    int ok = net_upnp_map_tcp(47777, &r);

    double secs = difftime(time(NULL), t0);
    printf("duree           : %.0f s\n", secs);
    printf("resultat        : %s\n", ok ? "MAPPE" : "non mappe (attendu en WSL)");
    printf("methode         : %s\n", r.method[0] ? r.method : "(aucune)");
    if (r.external_ip_be) {
        char ip[16]; net_ip_to_str(r.external_ip_be, ip);
        printf("IP externe      : %s (%s)\n", ip, r.external_public ? "publique" : "privee/CGNAT");
    }
    printf("message         : %s\n", r.message);

    if (ok) net_upnp_unmap_tcp(47777);

    /* Critère du test : pas de hang (borné) + message non vide. */
    int pass = (secs <= 12.0) && r.message[0];
    printf("=== %s ===\n", pass ? "OK (pas de blocage)" : "ECHEC");
    return pass ? 0 : 1;
}

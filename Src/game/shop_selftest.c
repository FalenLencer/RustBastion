/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 *
 * Test FUMÉE de la boutique (HORS Makefile — possède son main).
 * Simule la séquence de game_handle_shop : tirage des offres puis ACHATS
 * RÉPÉTÉS sur le même emplacement → vérifie que :
 *   - chaque achat remplace l'offre de l'emplacement (rachat infini) ;
 *   - on peut acheter PLUSIEURS fois tant que le Renfort suffit ;
 *   - le Renfort décroît et le build (count) s'incrémente.
 *
 * Compile : gcc -Wall -Wextra -Isrc Src/game/runperks.c Src/game/shop_selftest.c \
 *           -o /tmp/shop_test -lraylib -lGL -lm -lpthread -ldl -lrt \
 *           -lX11 -lXi -lXcursor -lXrandr -lXinerama && /tmp/shop_test
 */
#include "runperks.h"
#include <stdio.h>

int main(void) {
    printf("=== boutique : test rachat/remplacement ===\n");
    int fails = 0;

    RunBuild rb;
    runbuild_reset(&rb);
    rb.renfort = 300;   // ~ce qu'on a au 1er passage boutique (3 actes)

    rb.shop_n = runbuild_roll_offers(&rb, rb.shop_offer, MAX_SHOP_OFFER, 0, 1);
    printf("  offres initiales : %d\n", rb.shop_n);
    if (rb.shop_n < MAX_SHOP_OFFER) { printf("  [X] tirage incomplet\n"); fails++; }

    // Achats répétés sur l'emplacement 0 (réplique exacte de game_handle_shop)
    int buys = 0;
    for (int iter = 0; iter < 12; iter++) {
        int id = rb.shop_offer[0];
        if (id < 0 || id >= PERK_COUNT) { printf("  [X] offre invalide (iter %d)\n", iter); fails++; break; }
        const PerkDef *pd = &RUN_PERKS[id];
        if (rb.count[id] >= pd->max_stack || rb.renfort < pd->shop_cost)
            break;                                     // « ne peut plus acheter »
        int renfort_avant = rb.renfort;
        rb.renfort -= pd->shop_cost;
        runbuild_add(&rb, id);
        int nouveau = runbuild_reroll_slot(&rb, rb.shop_offer, rb.shop_n, 0);
        buys++;
        printf("  achat %d : %-22s -%d RNF (reste %d) -> slot remplace par %s\n",
               buys, pd->name, pd->shop_cost, rb.renfort,
               (nouveau >= 0) ? RUN_PERKS[nouveau].name : "(RIEN : echec reroll)");
        if (renfort_avant - rb.renfort != pd->shop_cost) { printf("  [X] renfort\n"); fails++; }
        if (nouveau < 0) { printf("  [X] le slot n'a pas ete remplace !\n"); fails++; break; }
    }

    if (buys >= 3) printf("  [OK] %d achats successifs possibles (rachat infini)\n", buys);
    else { printf("  [X] seulement %d achat(s) possible(s)\n", buys); fails++; }

    int total = 0;
    for (int i = 0; i < PERK_COUNT; i++) total += rb.count[i];
    if (total == buys) printf("  [OK] build : %d perks acquis\n", total);
    else { printf("  [X] build incoherent (%d vs %d)\n", total, buys); fails++; }

    printf("=== %s ===\n", fails == 0 ? "TOUT OK (0 echec)" : "ECHECS");
    return fails ? 1 : 0;
}

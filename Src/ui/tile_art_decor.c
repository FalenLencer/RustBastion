/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  tile_art_decor.c ─ décor pixel-art posé SUR les tuiles : bunkers de
 *  base. Extrait de tile_art.c (règle < 500 l.).
 *  (Le sol des tours — socle + ombre de contact : renderer_tower_fx.c.)
 */
#include "tile_art.h"
#include "tile_art_internal.h"
#include <math.h>

// ════════════════════════════════════════════════════
// BASE — bunker fortifié
// ════════════════════════════════════════════════════
/* Bunker en pixel art (cellules de TA_CELL px, grille 20×20 sur la tuile) :
   anneau de sacs de sable, dôme de béton ombré, meurtrière, bande et balise
   aux couleurs de l'équipe. Détruit : dôme noirci fendu + gravats + croix. */
#define TA_BASE_N      (TILE_SIZE / TA_CELL)   // 20 cellules
#define TA_DOME_RX     7.0f
#define TA_DOME_RY     6.2f
#define TA_OUTLINE     ((Color){16, 12, 10, 255})
void tile_art_draw_base(int px, int py, ThemeID theme,
                        Color accent, int destroyed)
{
    (void)theme;
    const float t  = (float)GetTime();
    const float pu = (sinf(t * 3.0f) + 1.0f) * 0.5f;
    const Color con  = destroyed ? (Color){58, 52, 50, 255} : (Color){132, 128, 120, 255};
    const Color bag  = destroyed ? (Color){96, 84, 66, 255} : (Color){182, 156, 108, 255};

    /* Sacs de sable : anneau ovale de sacs de 3 cellules, ombrés + coutures. */
    for (int j = 11; j < TA_BASE_N; j++)
        for (int i = 0; i < TA_BASE_N; i++) {
            float dx = (i + 0.5f - 10.0f) / 9.6f, dy = (j + 0.5f - 15.0f) / 4.8f;
            float r = dx * dx + dy * dy;
            if (r > 1.0f || r < 0.42f) continue;
            if (r > 0.86f || r < 0.48f) { ta_cell(px, py, i, j, TA_OUTLINE); continue; }
            int seam = ((i + (j & 1) * 2) % 3) == 0;
            float lit = (dy < 0.0f) ? 1.18f : 0.88f;
            Color c = seam ? shade(bag, -46) : shade(bag, (int)((lit - 1.0f) * 120.0f));
            ta_cell(px, py, i, j, c);
        }
    /* Dôme de béton : sphère éclairée en haut à gauche, contour sombre. */
    for (int j = 2; j < 18; j++)
        for (int i = 2; i < 18; i++) {
            float dx = (i + 0.5f - 10.0f) / TA_DOME_RX, dy = (j + 0.5f - 11.0f) / TA_DOME_RY;
            float r = dx * dx + dy * dy;
            if (r > 1.0f || j > 14) continue;
            if (r > 0.80f) { ta_cell(px, py, i, j, TA_OUTLINE); continue; }
            int d = (int)((-dx - dy) * 34.0f) + (int)(tile_hash(i, j) & 7u) - 3;
            Color c = shade(con, d);
            if (!destroyed && j == 13 && dx > -0.7f && dx < 0.7f) c = shade(accent, -30);   // bande d'équipe
            if (destroyed && ((i * 7 + j * 3) % 11 == 0)) c = (Color){20, 16, 14, 255};      // fissures
            ta_cell(px, py, i, j, c);
        }
    for (int i = 6; i <= 13; i++) ta_cell(px, py, i, 10, (Color){8, 6, 6, 255});           // meurtrière
    if (!destroyed) {
        for (int i = 7; i <= 12; i += 2)                                                    // lueur intérieure
            ta_cell(px, py, i, 10, (Color){accent.r, accent.g, accent.b, (unsigned char)(90 + pu * 120)});
        /* Antenne + voyant clignotant. */
        for (int j = 2; j <= 6; j++) ta_cell(px, py, 14 + (j < 4), j, (Color){40, 38, 36, 255});
        ta_cell(px, py, 15, 1, fmodf(t, 1.0f) < 0.5f ? (Color){255, 70, 60, 255} : (Color){90, 20, 18, 255});
        /* Balise d'équipe au sommet (halo pulsé). */
        DrawCircle(px + 10 * TA_CELL, py + 5 * TA_CELL, 5.0f + pu * 3.0f,
                   (Color){accent.r, accent.g, accent.b, (unsigned char)(50 + pu * 60)});
        for (int j = 4; j <= 5; j++)
            for (int i = 9; i <= 10; i++)
                ta_cell(px, py, i, j, (i == 9 && j == 4) ? ta_mix(accent, (Color){255, 255, 255, 255}, 0.6f) : accent);
    } else {
        for (int k = 0; k < 6; k++)                                                         // gravats
            ta_cell(px, py, 4 + (int)(tile_hash(k, 1) % 12u), 14 + (int)(tile_hash(k, 2) % 4u), shade(con, -20));
        Color x = {200, 50, 46, 230};                                                       // croix
        for (int k = 3; k <= 16; k++) { ta_cell(px, py, k, k - 1, x); ta_cell(px, py, 19 - k, k - 1, x); }
    }
}

/* (Filons de minerai : peints dans terrain_ore.c, comme les ruines.) */

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  tile_art_decor.c ─ décor pixel-art posé SUR les tuiles : bunkers de
 *  base, filons de minerai, roche minée et ombres portées des tours. Extrait de tile_art.c (règle < 500 l.).
 */
#include "tile_art.h"
#include "tile_art_internal.h"
#include "../combat/tower.h"   // TowerPool (ombres portées des tours)
#include <math.h>

// ── Réglages ─────────────────────────────────────────────────
#define TA_SHADOW_LEN   5    // portée de l'ombre SE (px)
#define TA_SHADOW_A     52   // opacité de l'ombre portée

// ════════════════════════════════════════════════════
// OMBRES PORTÉES COURTES (lumière NO → ombre au SUD-EST)
// Ruines/gratte-ciels + tours. À dessiner APRÈS les routes
// (l'ombre tombe dessus) et AVANT bases/minerais/tours.
// ════════════════════════════════════════════════════
static void shadow_se(int px, int py, int e_ok, int s_ok) {
    Color sh = (Color){0, 0, 0, TA_SHADOW_A};
    if (e_ok)
        DrawRectangle(px + TILE_SIZE, py + TA_SHADOW_LEN,
                      TA_SHADOW_LEN, TILE_SIZE - TA_SHADOW_LEN, sh);
    if (s_ok)
        DrawRectangle(px + TA_SHADOW_LEN, py + TILE_SIZE,
                      TILE_SIZE - TA_SHADOW_LEN, TA_SHADOW_LEN, sh);
    if (e_ok && s_ok)
        DrawRectangle(px + TILE_SIZE, py + TILE_SIZE,
                      TA_SHADOW_LEN, TA_SHADOW_LEN, sh);
}

void tile_art_draw_shadows(const Map *map, const struct TowerPool *tp) {
    /* Les ruines portent leur propre ombre (sprites de terrain_ruins.c). */
    if (tp) {
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *tw = &tp->towers[i];
            if (!tw->active) continue;
            shadow_se(tw->tile_x * TILE_SIZE, tw->tile_y * TILE_SIZE,
                      tw->tile_x + 1 < map->w, tw->tile_y + 1 < map->h);
        }
    }
}

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

// ════════════════════════════════════════════════════
// MINERAI — filon de cristaux (remplace la tuile) + roche minée
// ════════════════════════════════════════════════════
typedef struct { Color core, glow; } OrePalette;

// Palette par type de matériau (MaterialType : 0=fer … 4=nano).
static OrePalette ore_palette(int mat) {
    switch (mat) {
        case 0:  return (OrePalette){{182, 188, 202, 255}, {150, 160, 185, 255}}; // FER  (acier)
        case 1:  return (OrePalette){{122, 214,  72, 255}, { 90, 200,  50, 255}}; // ACIDE
        case 2:  return (OrePalette){{ 98, 170, 255, 255}, { 70, 150, 255, 255}}; // PLASMA
        case 3:  return (OrePalette){{158, 228, 255, 255}, {120, 210, 255, 255}}; // CRYO
        case 4:  return (OrePalette){{206, 116, 255, 255}, {180,  90, 255, 255}}; // NANO
        default: return (OrePalette){{200, 200, 210, 255}, {170, 170, 185, 255}};
    }
}

// Un cristal hexagonal facetté (volume par 3 polygones + arêtes + étincelle).
static void draw_crystal(float cx, float cy, float rad, float rot, Color c) {
    Color lite = shade(c,  55);
    Color dark = shade(c, -55);
    Color edge = shade(c,  95);
    DrawPoly((Vector2){cx, cy}, 6, rad,         rot, dark);   // base ombrée
    DrawPoly((Vector2){cx, cy}, 6, rad * 0.82f, rot, c);      // corps
    DrawPoly((Vector2){cx - rad*0.18f, cy - rad*0.18f},
             6, rad * 0.40f, rot, lite);                       // facette éclairée
    DrawPolyLines((Vector2){cx, cy}, 6, rad, rot,
                  (Color){edge.r, edge.g, edge.b, 200});       // arêtes
    DrawRectangle((int)(cx - rad*0.30f), (int)(cy - rad*0.42f),
                  2, 2, (Color){255, 255, 255, 210});          // étincelle
}

void tile_art_draw_deposit(int px, int py, int mat_type, float t) {
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    OrePalette op = ore_palette(mat_type);

    // 1) Matrice rocheuse minéralisée — opaque : remplace la tuile.
    Color matrix_d = {28, 25, 34, 255};
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){44, 39, 50, 255});
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE / 2, (Color){255, 255, 255, 8});
    DrawRectangle(px, py + TILE_SIZE / 2, TILE_SIZE, TILE_SIZE / 2, (Color){0, 0, 0, 30});
    DrawLine(px + 5, py + TILE_SIZE - 7, px + 15, py + 10, matrix_d);    // veines sombres
    DrawLine(px + TILE_SIZE - 6, py + TILE_SIZE - 9, px + 24, py + 12, matrix_d);
    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0, 0, 0, 60});  // bord
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0, 0, 0, 60});

    // 2) Halo lumineux pulsant (couleur du minerai).
    float pulse = (sinf(t * 2.2f + (float)(px * 3 + py) * 0.05f) + 1.0f) * 0.5f;
    unsigned char ga = (unsigned char)(38 + pulse * 70);
    DrawCircle(cx, cy, TILE_SIZE * 0.42f, (Color){op.glow.r, op.glow.g, op.glow.b, ga});

    // 3) Amas de cristaux (le filon proprement dit).
    draw_crystal((float)cx - 9, (float)cy + 6, 6.0f, -8.0f,             op.core);
    draw_crystal((float)cx + 9, (float)cy + 5, 6.5f, 18.0f,             op.core);
    draw_crystal((float)cx - 3, (float)cy - 8, 5.0f,  4.0f,             op.core);
    draw_crystal((float)cx + 6, (float)cy - 5, 4.0f, 24.0f,             op.core);
    draw_crystal((float)cx,     (float)cy + 3, 9.0f, 10.0f + pulse*4.0f, op.core);
}

void tile_art_draw_mined_rock(int px, int py) {
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    unsigned int h = tile_hash(px / TILE_SIZE, py / TILE_SIZE);

    // Roche épuisée, terne — laissée après extraction du filon.
    Color rock_d = {36, 32, 29, 255};
    Color rock_l = {78, 72, 66, 255};
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){58, 53, 49, 255});
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE / 2, (Color){255, 255, 255, 6});
    DrawRectangle(px, py + TILE_SIZE / 2, TILE_SIZE, TILE_SIZE / 2, (Color){0, 0, 0, 34});

    // Cratère creusé au centre → signale une tuile difficile à bâtir.
    DrawCircle(cx, cy + 1, TILE_SIZE * 0.30f, rock_d);
    DrawCircle(cx, cy + 2, TILE_SIZE * 0.20f, (Color){20, 17, 15, 255});
    DrawCircleLines(cx, cy + 1, TILE_SIZE * 0.30f, (Color){18, 15, 13, 220});

    // Éboulis + fissures.
    DrawRectangle(px + 5, py + 6, 4, 3, rock_l);
    DrawRectangle(px + TILE_SIZE - 9, py + 8, 4, 3, rock_d);
    DrawRectangle(px + 7, py + TILE_SIZE - 9, 3, 3, rock_d);
    DrawRectangle(px + TILE_SIZE - 10, py + TILE_SIZE - 8, 4, 3, rock_l);
    DrawLine(cx, cy, px + 6, py + 7, (Color){22, 19, 17, 200});
    DrawLine(cx, cy, px + TILE_SIZE - 7, py + 9, (Color){22, 19, 17, 180});
    speck(px, py, h, 2, 2, rock_l);

    // Quelques résidus de minerai ternes au fond du cratère.
    DrawRectangle(cx - 2, cy - 1, 2, 2, (Color){120, 116, 110, 200});
    DrawRectangle(cx + 3, cy + 2, 1, 1, (Color){120, 116, 110, 160});

    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0, 0, 0, 60});
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0, 0, 0, 60});
}

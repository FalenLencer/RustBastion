/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tile_art.h"
#include "tile_art_internal.h"   // ta_cell / ta_mix / tile_hash / shade
#include <math.h>

#define SPAWN_RIM_R    8      // rayon du cratère (cellules)
#define SPAWN_RIFT_H   6      // demi-hauteur de la faille (cellules)
#define SPAWN_RIFT_W   3.2f   // demi-largeur max de la faille (cellules)
#define SPAWN_SHARDS   5      // débris en orbite

// ════════════════════════════════════════════════════
// SPAWN — portail d'invasion
// ════════════════════════════════════════════════════
static void draw_spawn_tile(int tx, int ty) {
    const int px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;

    float t     = (float)GetTime();
    float pulse = (sinf(t * 3.0f) + 1.0f) * 0.5f;   // 0..1

    // ── Onde de repérage : large vague rouge foncé émise par le spawn ──
    // Rend l'emplacement d'invasion hyper visible dès l'arrivée sur la carte.
    {
        float maxR = TILE_SIZE * 4.2f;
        for (int k = 0; k < 2; k++) {
            float ph = fmodf(t/2.0f + (float)(tx*7 + ty)*0.13f + k*0.5f, 1.0f);
            unsigned char a = (unsigned char)((1.0f - ph) * 130);
            DrawCircleLines(cx, cy, ph*maxR,     (Color){180, 30, 30, a});
            DrawCircleLines(cx, cy, ph*maxR + 1, (Color){120, 16, 16, (unsigned char)(a/2)});
        }
    }

    /* (La route sous le portail est peinte dans le terrain : terrain_bake.c.)
       Portail en pixel art (cellules de TA_CELL px, centrées sur la tuile) :
       cratère cerclé de gravats, faille verticale incandescente qui vacille,
       débris en orbite. */
    const int N = SPAWN_RIM_R + 1;
    for (int j = -N; j <= N; j++)
        for (int i = -N; i <= N; i++) {
            float r = sqrtf((float)(i * i + j * j)) + (float)(tile_hash(i + tx * 64, j + ty * 64) & 3u) * 0.25f;
            if (r > SPAWN_RIM_R + 0.6f) continue;
            Color c;
            if (r > SPAWN_RIM_R - 0.5f)       c = (Color){16, 10, 8, 255};             // contour
            else if (r > SPAWN_RIM_R - 2.0f)  c = shade((Color){110, 92, 76, 255},      // gravats du bord
                                                        (i + j < 0 ? 26 : -18) + (int)(tile_hash(i, j) & 15u) - 8);
            else                              c = ta_mix((Color){22, 6, 6, 255}, (Color){70, 10, 8, 255},
                                                         (1.0f - r / SPAWN_RIM_R) * (0.6f + 0.4f * pulse));
            ta_cell(cx - TA_CELL / 2, cy - TA_CELL / 2, i, j, c);
        }
    /* Faille : largeur par rangée bruitée dans le temps (elle « respire »). */
    for (int j = -SPAWN_RIFT_H; j <= SPAWN_RIFT_H; j++) {
        float k = 1.0f - (float)(j * j) / (float)(SPAWN_RIFT_H * SPAWN_RIFT_H);
        float w = SPAWN_RIFT_W * sqrtf(k > 0.0f ? k : 0.0f) *
                  (0.75f + 0.35f * sinf(t * 7.0f + (float)j * 1.7f + (float)(tx * 3 + ty)));
        for (int i = -(int)w - 1; i <= (int)w + 1; i++) {
            float e = fabsf((float)i) / (w + 0.01f);
            Color c = e > 1.0f ? (Color){200, 30, 30, (unsigned char)(90 + 80 * pulse)}   // halo
                    : e > 0.6f ? (Color){210, 40, 30, 255}
                    : e > 0.3f ? (Color){255, 140, 50, 255}
                               : (Color){255, 238, 200, 255};                          // cœur
            ta_cell(cx - TA_CELL / 2, cy - TA_CELL / 2, i, j, c);
        }
    }
    for (int k = 0; k < SPAWN_SHARDS; k++) {                 // débris aspirés en orbite
        float a = t * (1.2f + 0.3f * k) + (float)k * 1.26f;
        float rr = SPAWN_RIM_R - 3.0f + sinf(t * 2.0f + k) * 1.0f;
        ta_cell(cx - TA_CELL / 2, cy - TA_CELL / 2, (int)(cosf(a) * rr), (int)(sinf(a) * rr * 0.8f),
                (k & 1) ? (Color){150, 128, 104, 255} : (Color){255, 120, 70, 230});
    }
}

void tile_art_draw_spawns(const Map *map) {
    for (int y = 0; y < map->h; y++)
        for (int x = 0; x < map->w; x++)
            if (map->tiles[y][x].type == TILE_SPAWN)
                draw_spawn_tile(x, y);
}

/* (Sol, routes, liquides et ruines : terrain*.c — peints au pixel.
   Bases, minerais, roche minée et ombres portées :
   voir tile_art_decor.c — extrait pour la règle < 500 lignes.) */

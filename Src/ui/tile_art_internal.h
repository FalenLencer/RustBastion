/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  tile_art_internal.h ─ helpers partagés entre tile_art.c (fonds,
 *  routes, spawns) et tile_art_decor.c (bases, minerais, franges,
 *  ombres). Interne au pixel-art des tuiles — ne pas inclure ailleurs.
 */
#include "raylib.h"
#include "../map/map_gen.h"   /* TILE_SIZE */

// Hash déterministe (stable par tuile) → pseudo-aléatoire reproductible.
static inline unsigned int tile_hash(int x, int y) {
    /* Produits en NON signé : en int, x × 73856093 déborde dès x ≥ 30. */
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// Éclaircit (d>0) ou assombrit (d<0) une couleur — délègue à la source
// unique ui_shade (ui_utils.h) ; nom court conservé (sites 2D).
#include "ui_utils.h"
static inline Color shade(Color c, int d) { return ui_shade(c, d); }

// ── Cellule de pixel art (même grain que le terrain peint : 2 px) ──
#define TA_CELL 2
static inline void ta_cell(int px, int py, int i, int j, Color c) {
    DrawRectangle(px + i * TA_CELL, py + j * TA_CELL, TA_CELL, TA_CELL, c);
}
static inline Color ta_mix(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

// ── Petit speck déterministe ──────────────────────────────────
static inline void speck(int px, int py, unsigned int h, int k, int sz, Color c) {
    // & 31 : borne le décalage sous la precision (32 bits) — evite l'UB quand
    // k*5(+7) atteint 32 (ex. k=5). Sans effet visuel sur les k valides.
    int gx = px + 4 + (int)((h >> ((k*5)     & 31)) % (TILE_SIZE - 8));
    int gy = py + 4 + (int)((h >> ((k*5 + 7) & 31)) % (TILE_SIZE - 8));
    DrawRectangle(gx, gy, sz, sz, c);
}

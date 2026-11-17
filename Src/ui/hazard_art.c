/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hazard_art.c ─ Rendu pixel-art animé des hasards de terrain.
 *
 *  Dessiné dans la caméra de la carte, par-dessus la route et sous les
 *  entités, au GRAIN du terrain peint (cellules de 2 px), en flaque
 *  organique qui tient dans la chaussée. Une silhouette par hasard, lisible d'un coup d'œil : braises
 *  qui rougeoient, flaque verte qui bouillonne, remous de sable, câbles qui
 *  crachent des étincelles, rigole de métal incandescent. Les positions des
 *  détails sont hachées par tuile : deux tuiles voisines ne clignotent
 *  jamais en phase.
 */
#include "../game/hazard.h"
#include "ui_utils.h"
#include <math.h>

#define HZ_CELL        2       // pixel d'art = celui du terrain peint (TB_PX)
#define HZ_R           6       // rayon de la flaque (cellules) : tient dans la chaussée
#define HZ_LABEL_FS    9       // infobulle : nom
#define HZ_LABEL_FS2   8       // infobulle : effet
#define HZ_LABEL_PAD   4

static unsigned int hz_hash(int x, int y, int k) {
    /* Produits en NON signé : en int, k × 83492791 déborde (comportement
       indéfini, relevé par UBSan). */
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u
                   ^ (unsigned int)k * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static float hz_rand01(int x, int y, int k) {
    return (float)(hz_hash(x, y, k) & 0xffffu) / 65535.0f;
}
static Color hz_mix(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
/* Cellule (i, j) autour du centre (cx, cy) de la tuile. */
static void hz_cell(int cx, int cy, int i, int j, Color c) {
    DrawRectangle(cx + i * HZ_CELL, cy + j * HZ_CELL, HZ_CELL, HZ_CELL, c);
}
/* Flaque organique : 0 = hors, 1 = bord, 2 = intérieur. */
static int hz_mask(int tx, int ty, int i, int j) {
    float r = sqrtf((float)(i * i + j * j)) + (hz_rand01(tx * 31 + i, ty * 17 + j, 3) - 0.5f) * 1.6f;
    return r > HZ_R ? 0 : r > HZ_R - 1.2f ? 1 : 2;
}

static void draw_embers(int cx, int cy, int tx, int ty, float t) {
    for (int j = -HZ_R; j <= HZ_R; j++)
        for (int i = -HZ_R; i <= HZ_R; i++) {
            int m = hz_mask(tx, ty, i, j);
            if (!m) continue;
            float r = hz_rand01(i + tx * 40, j + ty * 40, 5);
            Color c = m == 1 ? (Color){40, 20, 12, 200} : (Color){28, 12, 8, 220};   // sol calciné
            if (m == 2 && r > 0.62f) {                    // braise qui respire
                float f = 0.5f + 0.5f * sinf(t * (2.5f + r * 4.0f) + r * 30.0f);
                c = hz_mix((Color){150, 30, 10, 255}, (Color){255, 200, 80, 255}, f * (r - 0.62f) * 2.6f);
            }
            hz_cell(cx, cy, i, j, c);
        }
    for (int k = 0; k < 3; k++) {                         // étincelles qui montent
        float ph = fmodf(t * 0.8f + hz_rand01(tx, ty, 20 + k), 1.0f);
        int i = (int)((hz_rand01(tx, ty, 30 + k) - 0.5f) * 8.0f), j = -(int)(ph * 9.0f);
        hz_cell(cx, cy, i, j, (Color){255, 210, 110, (unsigned char)(230.0f * (1.0f - ph))});
    }
}

static void draw_toxic(int cx, int cy, int tx, int ty, float t) {
    for (int j = -HZ_R; j <= HZ_R; j++)
        for (int i = -HZ_R; i <= HZ_R; i++) {
            int m = hz_mask(tx, ty, i, j);
            if (!m) continue;
            float n = hz_rand01(i + tx * 40, j + ty * 40, 7);
            Color c = m == 1 ? (Color){150, 210, 70, 235}
                             : hz_mix((Color){34, 80, 26, 230}, (Color){70, 140, 40, 230}, n * 0.8f - (i + j) * 0.03f);
            hz_cell(cx, cy, i, j, c);
        }
    for (int k = 0; k < 3; k++) {                         // bulles qui gonflent puis crèvent
        float ph = fmodf(t * 0.9f + hz_rand01(tx, ty, k), 1.0f);
        int bi = (int)((hz_rand01(tx, ty, 40 + k) - 0.5f) * 7.0f), bj = (int)((hz_rand01(tx, ty, 50 + k) - 0.5f) * 7.0f);
        Color c = {200, 255, 130, (unsigned char)(240.0f * (1.0f - ph))};
        int rr = 1 + (int)(ph * 2.0f);
        hz_cell(cx, cy, bi - rr, bj, c); hz_cell(cx, cy, bi + rr, bj, c);
        hz_cell(cx, cy, bi, bj - rr, c); hz_cell(cx, cy, bi, bj + rr, c);
    }
}

static void draw_quicksand(int cx, int cy, int tx, int ty, float t) {
    for (int j = -HZ_R; j <= HZ_R; j++)
        for (int i = -HZ_R; i <= HZ_R; i++) {
            int m = hz_mask(tx, ty, i, j);
            if (!m) continue;
            float r = sqrtf((float)(i * i + j * j));
            float sp = fmodf(r * 0.9f + atan2f((float)j, (float)i) * 2.0f - t * 2.2f + 100.0f, 3.2f);
            Color c = m == 1 ? (Color){120, 86, 46, 220}
                             : sp < 1.0f ? (Color){96, 66, 34, 235} : (Color){176, 132, 76, 230};
            if (r < 1.5f) c = (Color){60, 40, 20, 240};   // œil du tourbillon
            hz_cell(cx, cy, i, j, c);
        }
}

static void draw_wires(int cx, int cy, int tx, int ty, float t) {
    const Color cable = {22, 22, 26, 240}, sheath = {170, 70, 34, 240};
    for (int k = -HZ_R; k <= HZ_R; k++) {                 // deux câbles en croix
        hz_cell(cx, cy, k, k / 2 - 1, cable);
        hz_cell(cx, cy, k, -k / 2 + 2, cable);
        if ((k & 3) == 0) { hz_cell(cx, cy, k, k / 2 - 2, sheath); hz_cell(cx, cy, k, -k / 2 + 1, sheath); }
    }
    float ph = fmodf(t * 1.3f + hz_rand01(tx, ty, 5), 1.0f);
    if (ph < 0.22f) {                                      // arc électrique zigzag
        int j = 0;
        for (int i = -4; i <= 4; i++) {
            j += (hz_hash(i, tx + ty, (int)(t * 18.0f)) & 1) ? 1 : -1;
            if (j > 3) j = 3;
            if (j < -3) j = -3;
            hz_cell(cx, cy, i, j, (Color){210, 240, 255, 250});
            hz_cell(cx, cy, i, j - 1, (Color){120, 200, 255, 140});
        }
    }
}

static void draw_molten(int cx, int cy, int tx, int ty, float t) {
    for (int j = -HZ_R; j <= HZ_R; j++)
        for (int i = -HZ_R; i <= HZ_R; i++) {
            int m = hz_mask(tx, ty, i, j);
            if (!m) continue;
            if (m == 1) { hz_cell(cx, cy, i, j, (Color){40, 16, 10, 235}); continue; }   // croûte
            float heat = 0.5f + 0.5f * sinf(i * 0.8f + j * 0.5f + t * 3.0f + hz_rand01(tx, ty, 9) * 6.0f);
            heat = heat * 0.7f + hz_rand01(i + tx * 40, j + ty * 40, 11) * 0.3f;
            hz_cell(cx, cy, i, j, heat > 0.8f ? (Color){255, 230, 120, 245}
                                  : hz_mix((Color){170, 40, 12, 240}, (Color){255, 130, 30, 245}, heat * 1.25f));
        }
}

void hazard_render_label(const Map *map, int tx, int ty) {
    if (tx < 0 || ty < 0) return;
    HazardType h = hazard_at(map, tx, ty);
    if (h == HAZ_NONE) return;
    const HazardDef *d = hazard_def(h);
    const char *l2 = d->effect;
    int w = mtxt(d->name, HZ_LABEL_FS), w2 = mtxt(l2, HZ_LABEL_FS2);
    if (w2 > w) w = w2;
    int bw = w + HZ_LABEL_PAD * 2, bh = fh(HZ_LABEL_FS) + fh(HZ_LABEL_FS2) + HZ_LABEL_PAD * 2;
    int bx = tx * TILE_SIZE + TILE_SIZE / 2 - bw / 2, by = ty * TILE_SIZE - bh - 4;
    DrawRectangle(bx, by, bw, bh, (Color){10, 7, 4, 220});
    DrawRectangleLines(bx, by, bw, bh, d->color);
    dtxt(d->name, bx + HZ_LABEL_PAD, by + HZ_LABEL_PAD, HZ_LABEL_FS, d->color);
    dtxt(l2, bx + HZ_LABEL_PAD, by + HZ_LABEL_PAD + fh(HZ_LABEL_FS), HZ_LABEL_FS2,
         (Color){220, 205, 170, 255});
}

void hazard_render(const Map *map) {
    if (!map || hazard_tile_count(map) == 0) return;
    float t = (float)GetTime();
    for (int ty = 0; ty < map->h && ty < MAX_MAP_H; ty++) {
        for (int tx = 0; tx < map->w && tx < MAX_MAP_W; tx++) {
            HazardType h = hazard_at(map, tx, ty);
            if (h == HAZ_NONE) continue;
            int cx = tx * TILE_SIZE + TILE_SIZE / 2, cy = ty * TILE_SIZE + TILE_SIZE / 2;
            switch (h) {
                case HAZ_EMBERS:    draw_embers   (cx, cy, tx, ty, t); break;
                case HAZ_TOXIC:     draw_toxic    (cx, cy, tx, ty, t); break;
                case HAZ_QUICKSAND: draw_quicksand(cx, cy, tx, ty, t); break;
                case HAZ_WIRES:     draw_wires    (cx, cy, tx, ty, t); break;
                case HAZ_MOLTEN:    draw_molten   (cx, cy, tx, ty, t); break;
                default: break;
            }
        }
    }
}

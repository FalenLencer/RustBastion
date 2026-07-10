/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  move.c ─ collision-déplacement commune des entités (voir move.h).
 *  Même principe éprouvé que la collision du héros : test de la tuile
 *  d'arrivée du bord avant, axe par axe (glissement), avec la règle
 *  anti-blocage « on peut toujours SORTIR d'une tuile bloquée ».
 */
#include "move.h"
#include <math.h>

#define MOVE_EPS 1e-3f   /* seuil « la position a changé » (px) */

int move_tile_blocked(const Map *map, const TowerPool *tp,
                      int tx, int ty, int flags) {
    if (tx < 0 || ty < 0 || tx >= map->w || ty >= map->h) return 1;
    const Tile *t = &map->tiles[ty][tx];

    if (t->type == TILE_WATER) return 1;
    if (t->type == TILE_RUIN)  return 1;
    if (t->type == TILE_BASE && (flags & MOVE_F_ALLY)) return 1;
    if (!t->passable && t->type != TILE_BASE) return 1;   /* filet générique */

    if (tp) {
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *tw = &tp->towers[i];
            if (tw->active && tw->tile_x == tx && tw->tile_y == ty) return 1;
        }
    }
    return 0;
}

int move_slide(const Map *map, const TowerPool *tp,
               float *px, float *py, float dx, float dy,
               float radius, int flags) {
    float ox = *px, oy = *py;

    /* Anti-blocage : si la tuile COURANTE est déjà bloquée (spawn raté,
       tour posée sous l'entité…), tout mouvement de sortie est permis. */
    int stuck = move_tile_blocked(map, tp, (int)(*px / TILE_SIZE),
                                  (int)(*py / TILE_SIZE), flags);

    float nx  = *px + dx;
    int   etx = (int)((nx + (dx > 0.0f ? radius : -radius)) / TILE_SIZE);
    int   ety = (int)(*py / TILE_SIZE);
    if (stuck || !move_tile_blocked(map, tp, etx, ety, flags)) *px = nx;

    float ny  = *py + dy;
    int   ftx = (int)(*px / TILE_SIZE);
    int   fty = (int)((ny + (dy > 0.0f ? radius : -radius)) / TILE_SIZE);
    if (stuck || !move_tile_blocked(map, tp, ftx, fty, flags)) *py = ny;

    /* Bornes dures de la carte. */
    float maxx = map->w * (float)TILE_SIZE - radius;
    float maxy = map->h * (float)TILE_SIZE - radius;
    if (*px < radius) *px = radius;
    if (*py < radius) *py = radius;
    if (*px > maxx)   *px = maxx;
    if (*py > maxy)   *py = maxy;

    return (fabsf(*px - ox) > MOVE_EPS || fabsf(*py - oy) > MOVE_EPS);
}

int move_toward(const Map *map, const TowerPool *tp,
                float *px, float *py, float tx, float ty,
                float step, float radius, int flags) {
    float dx = tx - *px, dy = ty - *py;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < MOVE_EPS || step <= 0.0f) return 0;
    dx /= dist; dy /= dist;

    /* Direction directe, puis déviations vers le but (±45°, ±90°) :
       contournement LOCAL des obstacles sans pathfinding complet. */
    static const float DEV[5] = {
        0.0f, MOVE_DEVIATE_1, -MOVE_DEVIATE_1, MOVE_DEVIATE_2, -MOVE_DEVIATE_2
    };
    for (int k = 0; k < 5; k++) {
        float c = cosf(DEV[k]), s = sinf(DEV[k]);
        float rx = (dx * c - dy * s) * step;
        float ry = (dx * s + dy * c) * step;
        if (move_slide(map, tp, px, py, rx, ry, radius, flags)) return 1;
    }
    return 0;
}

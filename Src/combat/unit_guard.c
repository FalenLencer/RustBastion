/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  unit_guard.c ─ Postes de garde sur les routes d'approche (cf. unit_guard.h).
 *  Calcul pur, recalculé à chaque frame à partir des chemins ennemis : un
 *  chemin qui change (tour, nouvelle carte) déplace aussitôt la garde, sans
 *  aucun état ajouté à Unit (donc aucun impact sur les sauvegardes).
 */
#include "unit_guard.h"
#include "combat_math.h"   /* gdist */
#include <float.h>
#include <math.h>
#include <stdlib.h>

typedef struct { float x, y, dx, dy; } Approach;   /* point + direction vers la base */

/* Unité qui garde sa base de sa propre initiative. */
static int is_guard(const Unit *u) {
    return u->active && u->hp > 0.0f && u->type != UNIT_WORKER && u->behavior == UBEH_PATROL;
}
static int same_base(const Unit *a, const Unit *b) {
    return fabsf(a->home_base_px - b->home_base_px) < 1.0f &&
           fabsf(a->home_base_py - b->home_base_py) < 1.0f;
}

/* Routes d'approche d'une base : pour chaque chemin ennemi actif qui y
   aboutit, le point situé UNIT_GUARD_STEPS cases en amont. Des routes qui
   se rejoignent avant ce point donnent un seul poste. */
static int approaches(const Map *map, const PathSet *ps, float bx, float by,
                      Approach *out, int max) {
    int n = 0, back = UNIT_GUARD_STEPS;
    int btx = (int)floorf(bx / TILE_SIZE), bty = (int)floorf(by / TILE_SIZE);
    for (int p = 0; p < ps->count; p++) {
        const Path *pa = &ps->paths[p];
        if (!pa->found || pa->len < 2) continue;
        if (pa->path_id >= 0 && pa->path_id < map->path_count && !map->paths[pa->path_id].active)
            continue;
        Point end = pa->steps[pa->len - 1];
        if (abs(end.x - btx) > 1 || abs(end.y - bty) > 1) continue;   /* mène ailleurs */
        int   k  = pa->len - 1 - back;
        if (k < 0) k = 0;
        Point a  = pa->steps[k], nb = pa->steps[k + 1];
        float ax = (a.x + 0.5f) * TILE_SIZE, ay = (a.y + 0.5f) * TILE_SIZE;
        float dx = (float)(nb.x - a.x), dy = (float)(nb.y - a.y), dl = sqrtf(dx * dx + dy * dy);
        if (dl < 1e-3f) { dx = bx - ax; dy = by - ay; dl = sqrtf(dx * dx + dy * dy); }
        if (dl < 1e-3f) continue;
        int dup = 0;
        for (int j = 0; j < n && !dup; j++)
            dup = hypotf(out[j].x - ax, out[j].y - ay) < UNIT_GUARD_MERGE * TILE_SIZE;
        if (!dup && n < max) out[n++] = (Approach){ax, ay, dx / dl, dy / dl};
    }
    return n;
}

int unit_guard_post(const UnitPool *up, int idx, const Map *map, const PathSet *paths,
                    float *px, float *py) {
    if (!up || !map || !paths || idx < 0 || idx >= MAX_UNITS) return 0;
    const Unit *u = &up->units[idx];
    if (!is_guard(u)) return 0;

    Approach ap[UNIT_GUARD_MAX_APPROACH];
    int n = approaches(map, paths, u->home_base_px, u->home_base_py, ap, UNIT_GUARD_MAX_APPROACH);
    if (n == 0) return 0;

    /* Rang parmi les gardes de la même base → route (répartition) et place
       dans le groupe. Ordre des slots : stable d'une frame à l'autre. */
    int rank = 0;
    for (int j = 0; j < idx; j++)
        if (is_guard(&up->units[j]) && same_base(&up->units[j], u)) rank++;
    const Approach *a = &ap[rank % n];
    int g   = rank / n;
    int lat = (g == 0) ? 0 : ((g & 1) ? (g + 1) / 2 : -(g / 2));   /* 0, +1, -1, +2, -2… */

    /* La mêlée tient la route ; le tir et le médic restent en retrait. */
    int   behind = (u->atk_range > UNIT_MELEE_ATK_THRESHOLD) || u->type == UNIT_MEDIC;
    float depth  = behind ? UNIT_GUARD_BACK : 0.0f;
    float side   = (float)lat * UNIT_GUARD_SPREAD + sinf(u->patrol_angle) * UNIT_GUARD_SWAY;
    *px = a->x + (a->dx * depth - a->dy * side) * TILE_SIZE;
    *py = a->y + (a->dy * depth + a->dx * side) * TILE_SIZE;
    return 1;
}

int unit_find_enemy_target(const Unit *u, const EnemyPool *ep,
                           float base_px, float base_py, int guard_post) {
    // L'ouvrier ne cherche pas de cible spontanément
    if (u->type == UNIT_WORKER) return -1;

    float intercept_px = u->intercept_range * TILE_SIZE;
    int   best         = -1;
    float best_score   = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;

        float d_base = gdist(e->x, e->y, base_px, base_py);
        float d_unit = gdist(e->x, e->y, u->x,    u->y);
        float d_home = guard_post ? gdist(e->x, e->y, u->home_base_px, u->home_base_py) : d_base;

        if (d_base > intercept_px && d_home > intercept_px &&
            d_unit > u->atk_range * TILE_SIZE * 3.0f)
            continue;

        float score = guard_post ? -d_home : -d_base;
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}

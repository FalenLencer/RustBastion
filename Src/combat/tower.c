/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tower.h"
#include "projectile.h"
#include "../engine/audio.h"
#include "../map/pathfinding.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "../game/meta.h"
#include <stdlib.h>

/* ════════════════════════════════════════════════════
   STATS DE BASE
   ════════════════════════════════════════════════════ */
const TowerStats TOWER_BASE_STATS[TOWER_TYPE_COUNT] = {
    [TOWER_GUN] = {
        .name="Tourelle", .cost=15, .damage=20.0f, .range=3.5f,
        .fire_rate=1.5f, .proj_speed=10.0f, .splash=0,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=0,
        .description="Polyvalente. Cible le premier ennemi en portee.",
    },
    [TOWER_SNIPER] = {
        .name="Sniper", .cost=25, .damage=90.0f, .range=6.5f,
        .fire_rate=0.4f, .proj_speed=18.0f, .splash=0,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=0,
        .description="Longue portee. Cible l'ennemi le plus avance.",
    },
    [TOWER_FLAME] = {
        .name="Lance-flammes", .cost=30, .damage=12.0f, .range=2.0f,
        .fire_rate=3.0f, .proj_speed=5.0f, .splash=1,
        .slow_factor=0.5f, .slow_duration=1.5f, .chain_count=0,
        .description="Zone courte. Ralentit et brule les ennemis.",
    },
    [TOWER_TESLA] = {
        .name="Tesla", .cost=50, .damage=31.0f, .range=4.0f,
        .fire_rate=0.8f, .proj_speed=20.0f, .splash=1,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=2,
        .description="Chaine sur 2 ennemis proches. Ignore l'armure.",
    },
};

/* ════════════════════════════════════════════════════
   UTILITAIRES
   ════════════════════════════════════════════════════ */
static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax-bx, dy = ay-by;
    return dx*dx + dy*dy;
}

static float px_of(int tile) {
    return tile * TILE_SIZE + TILE_SIZE * 0.5f;
}

/* ════════════════════════════════════════════════════
   LIMITE ACTIVE
   ════════════════════════════════════════════════════ */
int tower_active_limit(const MetaBonuses *bonuses) {
    int extra = bonuses ? bonuses->tower_limit_bonus : 0;
    int limit = MAX_TOWERS_BASE + extra * MAX_TOWERS_UPGR;
    if (limit > MAX_TOWERS_HARD) limit = MAX_TOWERS_HARD;
    return limit;
}

/* ════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════ */
void tower_pool_init(TowerPool *tp) {
    memset(tp, 0, sizeof(TowerPool));
    tp->tower_limit = MAX_TOWERS_BASE;
}

/* ════════════════════════════════════════════════════
   VÉRIFICATION PLACEMENT — inclut zone anti-spawn
   ════════════════════════════════════════════════════ */
int tower_can_place(const TowerPool *tp, const Map *map, int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;

    const Tile *t = &map->tiles[ty][tx];
    if (!t->buildable) return 0;

    // Zone d'exclusion autour de chaque spawn
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp   = map->paths[i].spawn;
        int   dist = abs(tx - sp.x) + abs(ty - sp.y);
        if (dist <= SPAWN_EXCLUSION_RADIUS) return 0;
    }

    // Pas de tour déjà là
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (tp->towers[i].active &&
            tp->towers[i].tile_x == tx &&
            tp->towers[i].tile_y == ty) return 0;
    }
    return 1;
}

/* ════════════════════════════════════════════════════
   COÛT RÉEL SELON LA TUILE
   ════════════════════════════════════════════════════ */
int tower_cost_on_tile(TowerType type, const Map *map, int tile_x, int tile_y) {
    int base_cost = TOWER_BASE_STATS[type].cost;
    if (tile_x < 0 || tile_x >= MAP_W || tile_y < 0 || tile_y >= MAP_H)
        return base_cost;
    if (map->tiles[tile_y][tile_x].type == TILE_RUIN)
        return base_cost * 2;
    return base_cost;
}

/* ════════════════════════════════════════════════════
   PLACEMENT D'UNE TOUR
   ════════════════════════════════════════════════════ */
int tower_place(TowerPool *tp, TowerType type,
                int tile_x, int tile_y, Map *map,
                int *gold, const MetaBonuses *bonuses)
{
    if (!tower_can_place(tp, map, tile_x, tile_y)) return 0;
    if (tp->tower_count >= tp->tower_limit) return 0;

    int real_cost = tower_cost_on_tile(type, map, tile_x, tile_y);
    if (*gold < real_cost) return 0;

    Tower *tw = NULL;
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!tp->towers[i].active) { tw = &tp->towers[i]; break; }
    }
    if (!tw) return 0;

    *gold -= real_cost;

    tw->type      = type;
    tw->tile_x    = tile_x;
    tw->tile_y    = tile_y;
    tw->cx        = px_of(tile_x);
    tw->cy        = px_of(tile_y);
    tw->level     = 0;
    tw->active    = 1;
    tw->angle     = 0.0f;
    tw->hp        = 100.0f;   // HP de base pour Artillery

    tw->damage    = TOWER_BASE_STATS[type].damage    * (bonuses ? bonuses->tower_dmg_mult   : 1.0f);
    tw->range     = TOWER_BASE_STATS[type].range     * (bonuses ? bonuses->tower_range_mult : 1.0f);
    tw->fire_rate = TOWER_BASE_STATS[type].fire_rate * (bonuses ? bonuses->tower_rate_mult  : 1.0f);

    tw->dmg_type = DMG_PHYSICAL;
    tw->material = MAT_NONE;
    switch (type) {
        case TOWER_FLAME: tw->dmg_type = DMG_POISON;   break;
        case TOWER_TESLA: tw->dmg_type = DMG_ELECTRIC;  break;
        default:          tw->dmg_type = DMG_PHYSICAL;  break;
    }

    tw->fire_timer = 1.0f / tw->fire_rate;

    map->tiles[tile_y][tile_x].buildable = 0;
    tp->tower_count++;
    return 1;
}

/* ════════════════════════════════════════════════════
   CIBLAGE — Healer priorité absolue
   ════════════════════════════════════════════════════ */
static int find_target(const Tower *tw, const EnemyPool *ep) {
    float rng  = tw->range * TILE_SIZE;
    float rng2 = rng * rng;

    int   best_idx   = -1;
    float best_score = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        if (e->invisible) continue;

        float d2 = dist2(tw->cx, tw->cy, e->x, e->y);
        if (d2 > rng2) continue;

        float score;
        if (e->type == ENEMY_HEALER)
            score = 10000.0f;             // priorité absolue
        else if (tw->type == TOWER_SNIPER)
            score = (float)e->path_index;
        else
            score = -d2;

        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
    }
    return best_idx;
}

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void tower_pool_update(TowerPool *tp, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *tw = &tp->towers[i];
        if (!tw->active) continue;

        tw->fire_timer -= dt;
        if (tw->fire_timer > 0.0f) continue;

        int target = find_target(tw, ep);
        if (target == -1) continue;

        const Enemy *e = &ep->enemies[target];
        tw->angle = atan2f(e->y - tw->cy, e->x - tw->cx);

        projectile_spawn(tp, tw, target, ep);
        switch (tw->type) {
            case TOWER_GUN:    audio_play_sfx(AUDIO_SFX_TOWER_FIRE_GUN);    break;
            case TOWER_SNIPER: audio_play_sfx(AUDIO_SFX_TOWER_FIRE_SNIPER); break;
            case TOWER_FLAME:  audio_play_sfx(AUDIO_SFX_TOWER_FIRE_FLAME);  break;
            case TOWER_TESLA:  audio_play_sfx(AUDIO_SFX_TOWER_FIRE_TESLA);  break;
            default: break;
        }
        tw->fire_timer = 1.0f / tw->fire_rate;
    }

    projectile_update(tp, ep, dt);
}
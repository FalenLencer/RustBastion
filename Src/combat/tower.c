/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tower.h"
#include "projectile.h"
#include "combat_math.h"
#include "../game/runperks.h"   // g_run_mods (build de run)
_Static_assert(META_TOWER_COUNT == TOWER_TYPE_COUNT,
               "META_TOWER_COUNT dans meta.h ne correspond pas a TOWER_TYPE_COUNT");
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
        .name="Sniper", .cost=25, .damage=115.0f, .range=6.5f,
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

const char *TOWER_LORE[TOWER_TYPE_COUNT] = {
    [TOWER_GUN] =
        "La tourelle automatique est l'epine dorsale de toute defense.\n"
        "Peu couteuse, robuste, facile a upgrader. Son canon rotatif\n"
        "cible le premier ennemi entrant dans sa zone sans jamais faillir.\n"
        "Ideale en masse sur les couloirs droits.",

    [TOWER_SNIPER] =
        "Construite a partir de pieces de fusil de precision recuperees.\n"
        "Un seul tir suffisant pour traverser un blindage leger. La balle\n"
        "cherche toujours l'ennemi le plus avance — celui qui est\n"
        "le plus pres de votre base.",

    [TOWER_FLAME] =
        "Alimentee par des bonbonnes de carburant recuperees en zone industrielle.\n"
        "Projette un cone de feu persistant qui inflige des degats sur la duree\n"
        "et ralentit les survivants de 50%. Devastatrice sur les groupes serres.\n"
        "Portee courte — a placer dans les goulots d'etranglement.",

    [TOWER_TESLA] =
        "Generateur electromagnetique assemble depuis des equipements militaires.\n"
        "L'arc initial rebondit sur 2 cibles supplementaires dans un rayon proche.\n"
        "Les degats electriques ignorent la resistance physique. Parfaite contre\n"
        "les essaims et les ennemis groupes.",
};

/* ════════════════════════════════════════════════════
   COÛTS DES AMÉLIORATIONS
   ════════════════════════════════════════════════════ */
const int TOWER_UPG_COST_DMG  [TOWER_UPG_MAX] = { 25,  45,  75, 115, 170 };
const int TOWER_UPG_COST_RANGE[TOWER_UPG_MAX] = { 20,  35,  55,  85, 125 };
const int TOWER_UPG_COST_RATE [TOWER_UPG_MAX] = { 30,  55,  90, 140, 210 };

/* ════════════════════════════════════════════════════
   UTILITAIRES
   ════════════════════════════════════════════════════ */
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
    if (tx < 0 || tx >= map->w || ty < 0 || ty >= map->h) return 0;

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
    if (tile_x < 0 || tile_x >= map->w || tile_y < 0 || tile_y >= map->h)
        return base_cost;
    if (map->tiles[tile_y][tile_x].type == TILE_RUIN)
        return base_cost * TOWER_RUIN_COST_MULT;
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
    tw->hp        = TOWER_MAX_HP;

    // Modificateurs rogue-lite (build de run) appliqués par-dessus le meta.
    float rl_dmg   = g_run_mods.tdmg_all *
                     (type < TOWER_TYPE_COUNT ? g_run_mods.tdmg_type[type] : 1.0f);
    tw->damage    = TOWER_BASE_STATS[type].damage    * (bonuses ? bonuses->tower_dmg_mult   : 1.0f) * rl_dmg;
    tw->range     = TOWER_BASE_STATS[type].range     * (bonuses ? bonuses->tower_range_mult : 1.0f) * g_run_mods.trange_all;
    tw->fire_rate = TOWER_BASE_STATS[type].fire_rate * (bonuses ? bonuses->tower_rate_mult  : 1.0f) * g_run_mods.trate_all;

    // Sauvegarde des stats de base (avant upgrades individuels)
    tw->base_damage    = tw->damage;
    tw->base_range     = tw->range;
    tw->base_fire_rate = tw->fire_rate;
    tw->upg_dmg        = 0;
    tw->upg_range      = 0;
    tw->upg_rate       = 0;

    tw->dmg_type     = DMG_PHYSICAL;
    tw->material     = MAT_NONE;
    tw->mat_dmg_mult = 1.0f;
    switch (type) {
        case TOWER_FLAME: tw->dmg_type = DMG_FIRE;      break;
        case TOWER_TESLA: tw->dmg_type = DMG_ELECTRIC;  break;
        default:          tw->dmg_type = DMG_PHYSICAL;  break;
    }

    tw->fire_timer = 1.0f / tw->fire_rate;

    map->tiles[tile_y][tile_x].buildable = 0;
    tp->tower_count++;
    return 1;
}

/* ════════════════════════════════════════════════════
   AMÉLIORATIONS INDIVIDUELLES
   ════════════════════════════════════════════════════ */
int tower_upg_next_cost_dmg(const Tower *t) {
    if (t->upg_dmg >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_DMG[t->upg_dmg];
}
int tower_upg_next_cost_range(const Tower *t) {
    if (t->upg_range >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_RANGE[t->upg_range];
}
int tower_upg_next_cost_rate(const Tower *t) {
    if (t->upg_rate >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_RATE[t->upg_rate];
}

void tower_upgrade_dmg(Tower *t) {
    if (t->upg_dmg >= TOWER_UPG_MAX) return;
    t->upg_dmg++;
    // Inclut le multiplicateur de matériau → le bonus n'est plus effacé par un upgrade.
    t->damage = t->base_damage * (1.0f + t->upg_dmg * TOWER_UPG_DMG_MULT) * t->mat_dmg_mult;
}

void tower_set_material(Tower *t, MaterialType mat) {
    if (!t) return;
    float mult = 1.0f;
    switch (mat) {
        case MAT_IRON:   mult = 1.45f; break;                 // +45% (garde le type de dégâts)
        case MAT_ACID:   t->dmg_type = DMG_POISON;   mult = 1.25f; break;
        case MAT_PLASMA: t->dmg_type = DMG_ELECTRIC; mult = 1.30f; break;
        case MAT_CRYO:   t->dmg_type = DMG_CRYO;     mult = 1.20f; break;
        case MAT_NANO:   t->dmg_type = DMG_NANO;     mult = 1.20f; break;
        default: return;
    }
    t->material     = mat;
    t->mat_dmg_mult = mult;   // remplace proprement (pas d'empilement)
    t->damage = t->base_damage * (1.0f + t->upg_dmg * TOWER_UPG_DMG_MULT) * t->mat_dmg_mult;
}
void tower_upgrade_range(Tower *t) {
    if (t->upg_range >= TOWER_UPG_MAX) return;
    t->upg_range++;
    t->range = t->base_range * (1.0f + t->upg_range * TOWER_UPG_RANGE_MULT);
}
void tower_upgrade_rate(Tower *t) {
    if (t->upg_rate >= TOWER_UPG_MAX) return;
    t->upg_rate++;
    t->fire_rate = t->base_fire_rate * (1.0f + t->upg_rate * TOWER_UPG_RATE_MULT);
}

void tower_do_repair(Tower *t) {
    t->hp = TOWER_MAX_HP;
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

        float d2 = gdist2(tw->cx, tw->cy, e->x, e->y);
        if (d2 > rng2) continue;

        float score;
        if (e->type == ENEMY_HEALER)
            score = TOWER_HEALER_PRIORITY; // priorité absolue
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

        // Étourdie par une onde EMP de boss → ne tire pas
        if (tw->stun_timer > 0.0f) {
            tw->stun_timer -= dt;
            continue;
        }

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
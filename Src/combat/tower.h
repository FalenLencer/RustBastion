/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "enemy.h"
#include "../game/meta.h"
#include "../combat/material.h"

#define MAX_TOWERS_HARD  64
#define MAX_TOWERS_BASE  12
#define MAX_TOWERS_UPGR   2
#define MAX_TOWERS       MAX_TOWERS_HARD
#define MAX_PROJECTILES  256
typedef enum {
    TOWER_GUN    = 0,
    TOWER_SNIPER,
    TOWER_FLAME,
    TOWER_TESLA,
    TOWER_TYPE_COUNT
} TowerType;

typedef struct {
    const char *name;
    int         cost;
    float       damage;
    float       range;
    float       fire_rate;
    float       proj_speed;
    int         splash;
    float       slow_factor;
    float       slow_duration;
    int         chain_count;
    const char *description;
} TowerStats;

extern const TowerStats TOWER_BASE_STATS[TOWER_TYPE_COUNT];

typedef struct {
    TowerType    type;
    int          tile_x, tile_y;
    float        cx, cy;
    float        fire_timer;
    int          level;
    int          active;
    float        angle;
    float        damage;
    float        range;
    float        fire_rate;
    DamageType   dmg_type;
    MaterialType material;
    float        hp;          // ← points de vie (pour Artillery)
} Tower;

typedef struct {
    float     x, y;
    float     tx, ty;
    int       target_idx;
    float     damage;
    float     speed;
    int       splash;
    float     splash_radius;
    float     slow_duration;
    int       chain_left;
    int       active;
    TowerType  origin;
    DamageType dmg_type;
} Projectile;

// ── Pool ─────────────────────────────────────────────────────
typedef struct TowerPool {
    Tower      towers[MAX_TOWERS];
    int        tower_count;
    int        tower_limit;
    Projectile projectiles[MAX_PROJECTILES];
    int        proj_count;
} TowerPool;

// ── API ──────────────────────────────────────────────────────
void tower_pool_init   (TowerPool *tp);
int  tower_place       (TowerPool *tp, TowerType type,
                        int tile_x, int tile_y, Map *map,
                        int *gold, const MetaBonuses *bonuses);
void tower_pool_update (TowerPool *tp, EnemyPool *ep, float dt);
int  tower_can_place   (const TowerPool *tp, const Map *map,
                        int tile_x, int tile_y);
int  tower_cost_on_tile(TowerType type, const Map *map,
                        int tile_x, int tile_y);
int  tower_active_limit(const MetaBonuses *bonuses);
/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../map/pathfinding.h"
#include "../map/theme.h"

#define MAX_ENEMIES 256

typedef enum {
    ENEMY_RAIDER      = 0,
    ENEMY_BRUTE,
    ENEMY_RUNNER,
    ENEMY_VEHICLE,
    ENEMY_MUTANT,
    ENEMY_GHOST,
    ENEMY_PATHBREAKER,
    ENEMY_HEALER,       // soigne les ennemis proches, priorité de ciblage max
    ENEMY_HUNTER,       // traque les unités alliées, ignore le chemin
    ENEMY_ARTILLERY,    // s'arrête à portée et détruit les tours
    ENEMY_TYPE_COUNT
} EnemyType;

typedef struct {
    EnemyType type;

    float hp, max_hp;
    float speed;
    float size;
    int   reward;
    int   damage;

    float x, y;

    int   path_id;
    int   path_index;

    int   active;
    int   dead;
    int   reached_base;

    // Effets de statut
    float slow_timer;
    float poison_timer;
    float poison_damage;
    float regen_timer;
    float spawn_delay;

    // Ghost
    int   invisible;

    // Pathbreaker
    int   path_broken;
    int   break_at;
    float target_x;
    float target_y;

    // Combat mêlée
    float melee_range;
    float engage_timer;
    float atk_timer;

    // Healer
    float heal_timer;
    float heal_range;
    float heal_amount;

    // Hunter
    int   hunt_target;
    float hunt_range;

    // Artillery
    float arty_range;
    float arty_timer;
    int   arty_target;

    // Flammes — accumulation de brûlure
    int   burn_stacks;       // paliers actifs (0..BURN_MAX_STACKS)
    float burn_decay_timer;  // temps avant perte des stacks (s)
} Enemy;

typedef struct {
    Enemy enemies[MAX_ENEMIES];
    int   count;
} EnemyPool;

typedef struct {
    float      hp;
    float      speed;
    float      size;
    int        reward;
    int        damage;
    float      melee_range;
    const char *name;
} EnemyStats;

extern const EnemyStats ENEMY_BASE_STATS[ENEMY_TYPE_COUNT];

// ── Résistances / faiblesses ─────────────────────────────────
// [EnemyType][DamageType] → multiplicateur de dégâts
// DMG_PHYSICAL=0, DMG_POISON=1, DMG_ELECTRIC=2, DMG_CRYO=3, DMG_NANO=4
#define DAMAGE_TYPE_COUNT 5
extern const float ENEMY_DMG_MULT[ENEMY_TYPE_COUNT][DAMAGE_TYPE_COUNT];

// ── Descriptions bestiaire ───────────────────────────────────
extern const char *ENEMY_DESC[ENEMY_TYPE_COUNT];
extern const char *ENEMY_SPEC[ENEMY_TYPE_COUNT];

// ── Paliers de brûlure (lance-flammes) ───────────────────────
#define BURN_MAX_STACKS 10

// Forward declarations
typedef struct UnitPool  UnitPool;
typedef struct TowerPool TowerPool;

void  enemy_pool_init  (EnemyPool *pool);
void  enemy_pool_update(EnemyPool *pool, const PathSet *paths,
                        UnitPool *units, TowerPool *towers,
                        Map *map,
                        float dt, int *lives, int *gold, int *kills);
void  enemy_spawn      (EnemyPool *pool, EnemyType type,
                        int path_id, const PathSet *paths,
                        float spawn_delay, float wave_scale,
                        float speed_mult);
int   enemy_pool_alive (const EnemyPool *pool);
void  enemy_damage     (Enemy *e, float dmg);
#pragma once
#include "raylib.h"
#include "../map/pathfinding.h"
#include "../map/theme.h"

#define MAX_ENEMIES 256

// ── Types d'ennemis ──────────────────────────────────────────
typedef enum {
    ENEMY_RAIDER  = 0,  // rapide, fragile
    ENEMY_BRUTE,        // lent, très résistant
    ENEMY_RUNNER,       // très rapide, très fragile
    ENEMY_VEHICLE,      // blindé, mini-boss
    ENEMY_MUTANT,       // régénère, résiste poison
    ENEMY_TYPE_COUNT
} EnemyType;

// ── Structure d'un ennemi ────────────────────────────────────
typedef struct {
    EnemyType type;

    float hp, max_hp;
    float speed;         // tuiles/seconde
    float size;          // rayon visuel en pixels
    int   reward;        // or donné à la mort
    int   damage;        // dégâts infligés à la base

    // Position en pixels (interpolée entre tuiles)
    float x, y;

    // Progression sur le chemin A*
    int   path_id;       // quel chemin cet ennemi emprunte
    int   path_index;    // tuile actuelle dans path->steps[]

    // État
    int   active;        // 1 = en jeu
    int   dead;          // 1 = tué par une tour
    int   reached_base;  // 1 = a atteint la base

    // Effets de statut
    float slow_timer;    // secondes de ralentissement restantes
    float regen_timer;   // pour ENEMY_MUTANT
    float spawn_delay;   // délai avant apparition (échelonnement vague)
} Enemy;

// ── Pool d'ennemis ───────────────────────────────────────────
typedef struct {
    Enemy enemies[MAX_ENEMIES];
    int   count;         // nombre d'ennemis actifs
} EnemyPool;

// ── Stats de base par type (indépendantes du thème) ─────────
typedef struct {
    float hp;
    float speed;
    float size;
    int   reward;
    int   damage;
    const char *name;
} EnemyStats;

extern const EnemyStats ENEMY_BASE_STATS[ENEMY_TYPE_COUNT];

// ── API ──────────────────────────────────────────────────────
void  enemy_pool_init  (EnemyPool *pool);
void  enemy_pool_update(EnemyPool *pool, const PathSet *paths,
                        float dt, int *lives, int *gold, int *kills);
void  enemy_spawn      (EnemyPool *pool, EnemyType type,
                        int path_id, const PathSet *paths,
                        float spawn_delay, float wave_scale,
                        float speed_mult);
int   enemy_pool_alive (const EnemyPool *pool);  // nb d'ennemis encore actifs
void  enemy_damage     (Enemy *e, float dmg);    // inflige des dégâts
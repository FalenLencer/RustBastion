#pragma once
#include "raylib.h"
#include "map_gen.h"
#include "enemy.h"
#include "meta.h"

#define MAX_TOWERS      64
#define MAX_PROJECTILES 256

// ── Types de tours ───────────────────────────────────────────
typedef enum {
    TOWER_GUN     = 0,  // tourelle polyvalente
    TOWER_SNIPER,       // longue portée, gros dégâts
    TOWER_FLAME,        // courte portée, zone, ralentit
    TOWER_TESLA,        // électrique, rebond sur plusieurs cibles
    TOWER_TYPE_COUNT
} TowerType;

// ── Statistiques de base d'un type de tour ───────────────────
typedef struct {
    const char *name;
    int         cost;        // coût en or
    float       damage;      // dégâts par tir
    float       range;       // portée en tuiles
    float       fire_rate;   // tirs par seconde
    float       proj_speed;  // vitesse projectile (tuiles/s)
    int         splash;      // 1 = dégâts de zone
    float       slow_factor; // 0 = pas de ralentissement, 0.5 = -50%
    float       slow_duration;
    int         chain_count; // TESLA : nombre de rebonds
    const char *description;
} TowerStats;

extern const TowerStats TOWER_BASE_STATS[TOWER_TYPE_COUNT];

// ── Structure d'une tour placée ──────────────────────────────
typedef struct {
    TowerType type;
    int       tile_x, tile_y;   // position en tuiles
    float     cx, cy;           // centre en pixels
    float     fire_timer;       // temps avant prochain tir
    int       level;            // niveau méta (0 = base)
    int       active;
    float     angle;            // angle visuel vers la cible
    float damage;      // ← stocke la valeur bonifiée
    float range;
    float fire_rate;
} Tower;

// ── Projectile ───────────────────────────────────────────────
typedef struct {
    float  x, y;           // position actuelle
    float  tx, ty;         // position cible (snapshot)
    int    target_idx;     // index dans EnemyPool
    float  damage;
    float  speed;          // pixels/seconde
    int    splash;
    float  splash_radius;
    float  slow_duration;
    int    chain_left;     // rebonds Tesla restants
    int    active;
    TowerType origin;
} Projectile;

// ── Pool de tours et projectiles ─────────────────────────────
typedef struct {
    Tower       towers[MAX_TOWERS];
    int         tower_count;
    Projectile  projectiles[MAX_PROJECTILES];
    int         proj_count;
} TowerPool;

// ── API ──────────────────────────────────────────────────────
void tower_pool_init  (TowerPool *tp);
int  tower_place(TowerPool *tp, TowerType type,
                 int tile_x, int tile_y, Map *map,
                 int *gold, const MetaBonuses *bonuses);
void tower_pool_update(TowerPool *tp, EnemyPool *ep, float dt);
int  tower_can_place  (const TowerPool *tp, const Map *map,
                       int tile_x, int tile_y);
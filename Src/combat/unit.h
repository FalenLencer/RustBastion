#pragma once
#include "raylib.h"
#include "enemy.h"
#include "../map/map_gen.h"
#include "../game/meta.h"
#include "../combat/material.h"

#define MAX_UNITS_HARD  32   // taille du pool statique
#define MAX_UNITS_PER_BASE   8   // limite de jeu par défaut
#define MAX_UNITS_UPGR   2   // unités bonus par niveau d'upgrade
#define MAX_UNITS        MAX_UNITS_HARD  // garde la taille du pool

typedef enum {
    UNIT_SOLDIER = 0,
    UNIT_HEAVY,
    UNIT_MEDIC,
    UNIT_DOG,
    UNIT_WORKER,        // ← nouveau : ouvrier collecteur
    UNIT_TYPE_COUNT
} UnitType;

typedef enum {
    USTATE_PATROL = 0,
    USTATE_CHASE,
    USTATE_ATTACK,
    USTATE_RETURN,
    USTATE_HEAL,
    USTATE_GOTO_DEPOSIT, // ← se dirige vers un dépôt
    USTATE_COLLECT,      // ← en train de collecter
    USTATE_GOTO_BASE,    // ← revient à la base avec le matériau
} UnitState;

typedef struct {
    const char *name;
    int         cost;
    float       hp;
    float       damage;
    float       speed;
    float       atk_range;
    float       atk_rate;
    float       intercept_range;
    float       size;
    const char *description;
} UnitStats;

extern const UnitStats UNIT_BASE_STATS[UNIT_TYPE_COUNT];

typedef struct {
    UnitType   type;
    UnitState  state;

    float      hp, max_hp;
    float      damage;
    float      speed;
    float      atk_range;
    float      atk_rate;
    float      intercept_range;
    float      size;

    float      x, y;
    float      patrol_angle;
    float      patrol_radius;

    int        target_idx;     // index ennemi (-1 = aucun)
    float      atk_timer;
    float      heal_timer;

    // ── Ouvrier ──────────────────────────────────────
    int          deposit_idx;      // index du dépôt cible (-1 = aucun)
    float        collect_timer;    // temps restant de collecte
    float        collect_duration; // durée totale de collecte
    MaterialType carried_mat;      // matériau en cours de transport
    int          has_material;     // 1 = porte un matériau

    int        active;
    int        slot;
} Unit;

typedef struct UnitPool {
    Unit  units[MAX_UNITS];
    int   count;
    float base_px, base_py;

    int   unit_limit;

    // Index de l'unité sélectionnée par le joueur (-1 = aucune)
    int   selected_unit;
} UnitPool;

// ── API ──────────────────────────────────────────────────────
void unit_pool_init    (UnitPool *up, float base_px, float base_py);
int  unit_spawn        (UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses);
void unit_pool_update  (UnitPool *up, EnemyPool *ep, Map *map, float dt,
                        MaterialType *inventory, int *inv_count);
void unit_damage       (Unit *u, float dmg);
void unit_assign_deposit(UnitPool *up, int unit_idx, int deposit_idx);
int unit_active_limit(const MetaBonuses *bonuses, int base_count);
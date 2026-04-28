#pragma once
#include "raylib.h"
#include "enemy.h"
#include "map_gen.h"
#include "meta.h"

#define MAX_UNITS 32

typedef enum {
    UNIT_SOLDIER = 0,  // infanterie polyvalente
    UNIT_HEAVY,        // tank lent, très résistant
    UNIT_MEDIC,        // soigne les alliés
    UNIT_DOG,          // très rapide, harcèle
    UNIT_TYPE_COUNT
} UnitType;

typedef enum {
    USTATE_PATROL = 0, // tourne autour de la base
    USTATE_CHASE,      // poursuit un ennemi
    USTATE_ATTACK,     // frappe un ennemi à portée
    USTATE_RETURN,     // revient vers la base après combat
    USTATE_HEAL,       // reçoit des soins (médic)
} UnitState;

typedef struct {
    const char *name;
    int         cost;
    float       hp;
    float       damage;
    float       speed;        // tuiles/seconde
    float       atk_range;   // portée d'attaque en tuiles
    float       atk_rate;    // attaques/seconde
    float       intercept_range; // distance depuis la base pour sortir
    float       size;         // rayon visuel
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

    float      x, y;           // position en pixels
    float      patrol_angle;   // angle de patrouille autour de la base
    float      patrol_radius;  // rayon de patrouille en pixels

    int        target_idx;     // index dans EnemyPool (-1 = aucun)
    float      atk_timer;
    float      heal_timer;     // pour UNIT_MEDIC

    int        active;
    int        slot;           // indice dans le pool (pour le calcul patrol)
} Unit;

typedef struct {
    Unit units[MAX_UNITS];
    int  count;
    // Position de la base en pixels (référence pour la patrouille)
    float base_px, base_py;
} UnitPool;

// ── API ──────────────────────────────────────────────────────
void unit_pool_init  (UnitPool *up, float base_px, float base_py);
int unit_spawn(UnitPool *up, UnitType type,int *gold, const MetaBonuses *bonuses);
void unit_pool_update(UnitPool *up, EnemyPool *ep, float dt);
void unit_damage     (Unit *u, float dmg);
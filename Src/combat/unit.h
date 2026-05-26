/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

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

// ── Médic ─────────────────────────────────────────────────────
#define UNIT_MEDIC_HEAL_AMOUNT        20.0f  /* PV restaurés par soin               */
#define UNIT_MEDIC_HEAL_TIMER          1.5f  /* cooldown entre soins (s)            */
#define UNIT_MEDIC_HEAL_RANGE          3.0f  /* portée de soin (en tiles)           */

// ── Ouvrier ───────────────────────────────────────────────────
#define UNIT_WORKER_COLLECT_DURATION   8.0f  /* durée de collecte au dépôt (s)      */
#define UNIT_WORKER_ENEMY_SLOW_RANGE   3.5f  /* ennemis dans ce rayon = collecte slow*/
#define UNIT_WORKER_ENEMY_SLOW_FACTOR  0.35f /* facteur de vitesse collecte ralentie */
#define UNIT_DEPOSIT_ARRIVE_DIST       0.8f  /* seuil d'arrivée au dépôt (en tiles) */
#define UNIT_BASE_ARRIVE_DIST          1.0f  /* seuil d'arrivée à la base (en tiles)*/
#define UNIT_WORKER_PATROL_ANGLE_SPEED 0.3f  /* vitesse angulaire patrouille (rad/s)*/
#define UNIT_WORKER_PATROL_RADIUS      1.5f  /* rayon de patrouille ouvrier (tiles) */
#define UNIT_WORKER_PATROL_SPEED_FRAC  0.5f  /* fraction de vitesse en patrouille   */

// ── Combat ────────────────────────────────────────────────────
#define UNIT_COUNTER_DMG_MULT          2.0f  /* multiplicateur riposte ennemie      */
#define UNIT_PATROL_ANGLE_SPEED        0.5f  /* vitesse angulaire patrouille (rad/s)*/
#define UNIT_PATROL_SPEED_FRAC         0.6f  /* fraction de vitesse en patrouille   */
#define UNIT_PATROL_SLACK              2.0f  /* seuil min déplacement patrouille (px)*/

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
    USTATE_MOVE_MANUAL,  // ← déplacement manuel vers destination
} UnitState;

// ── Comportement (ordre du joueur) ───────────────────────────
typedef enum {
    UBEH_PATROL = 0,    // patrouille automatique (défaut)
    UBEH_GUARD_TOWER,   // défend une tourelle spécifique
    UBEH_ESCORT_WORKER, // escorte un ouvrier spécifique
    UBEH_MANUAL,        // position manuelle fixée par le joueur
    UBEH_FOLLOW_UNIT,   // (médic) suit n'importe quelle unité alliée
} UnitBehavior;

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
extern const char     *UNIT_LORE      [UNIT_TYPE_COUNT];  // description longue (bestiaire)

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

    // Position de la base d'origine (pour patrouille et retour)
    float      home_base_px, home_base_py;

    // ── Comportement (unités de combat) ──────────────
    UnitBehavior behavior;
    int          escort_idx;       // UBEH_ESCORT_WORKER: index de l'ouvrier ; UBEH_FOLLOW_UNIT: index de l'unité suivie
    int          guard_tower_idx;  // UBEH_GUARD_TOWER: index de la tour
    float        manual_x, manual_y; // UBEH_MANUAL: destination
    int          manual_moving;    // 1 = se déplace vers manual_x/y
} Unit;

typedef struct UnitPool {
    Unit  units[MAX_UNITS];
    int   count;
    float base_px, base_py;

    int   unit_limit;

    // Index de l'unité sélectionnée par le joueur (-1 = aucune)
    int   selected_unit;

    // 1 = ouvriers actifs (PHASE_WAVE), 0 = figés (PHASE_PREP)
    int   mining_enabled;
} UnitPool;

// ── API ──────────────────────────────────────────────────────
void unit_pool_init    (UnitPool *up, float base_px, float base_py);
int  unit_spawn        (UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses);
int  unit_spawn_at     (UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses, float bpx, float bpy);
void unit_pool_update  (UnitPool *up, EnemyPool *ep, Map *map, float dt,
                        MaterialType *inventory, int *inv_count,
                        const TowerPool *towers);
void unit_damage       (Unit *u, float dmg);
void unit_assign_deposit(UnitPool *up, int unit_idx, int deposit_idx);
int unit_active_limit(const MetaBonuses *bonuses, int base_count);
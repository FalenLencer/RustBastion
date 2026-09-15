/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "enemy.h"
#include "move.h"               /* MoveNav (navigation à waypoint) */
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
#define MAX_MEDICS_PER_BASE            4     /* médics max par base                 */

// ── Ouvrier ───────────────────────────────────────────────────
#define UNIT_WORKER_COLLECT_DURATION   5.0f  /* durée de collecte au dépôt (s)      */
#define UNIT_WORKER_ENEMY_SLOW_RANGE   3.5f  /* ennemis dans ce rayon = collecte slow*/
#define UNIT_WORKER_ENEMY_SLOW_FACTOR  0.35f /* facteur de vitesse collecte ralentie */
/* Rayon de DÉPLOIEMENT autour d'une base (tuiles). SOURCE UNIQUE : le
   cercle d'aperçu dessiné par le HUD et le test de validité du clic
   DOIVENT être le même nombre — sinon le cercle affiché ment au joueur. */
#define UNIT_DEPLOY_RADIUS_TILES       5.0f
/* Fraction du coût remboursée au renvoi d'une unité. SOURCE UNIQUE :
   montant AFFICHÉ sur le bouton et montant réellement crédité. */
#define UNIT_SELL_REFUND               0.5f
#define UNIT_DEPOSIT_ARRIVE_DIST       0.8f  /* seuil d'arrivée au dépôt (en tiles) */
/* ── Déblaiement d'obstacle (ruines / roches minées) ─────────── */
#define UNIT_WORKER_CLEAR_COST         5     /* or dépensé à l'ordre        */
#define UNIT_WORKER_CLEAR_DURATION     3.0f  /* durée du déblaiement (s)    */
/* L'obstacle étant INFRANCHISSABLE, l'ouvrier travaille depuis une tuile
   voisine : 1.0 tuile en orthogonal, 1.41 en diagonale → 1.6 couvre les deux. */
#define UNIT_WORKER_CLEAR_REACH        1.6f  /* portée d'action (tiles)     */
#define UNIT_BASE_ARRIVE_DIST          1.0f  /* seuil d'arrivée à la base (en tiles)*/
#define UNIT_WORKER_PATROL_ANGLE_SPEED 0.3f  /* vitesse angulaire patrouille (rad/s)*/
#define UNIT_WORKER_PATROL_RADIUS      1.5f  /* rayon de patrouille ouvrier (tiles) */
#define UNIT_WORKER_PATROL_SPEED_FRAC  0.5f  /* fraction de vitesse en patrouille   */

// ── Combat ────────────────────────────────────────────────────
#define UNIT_COUNTER_DMG_MULT          2.0f  /* multiplicateur riposte ennemie (corps à corps) */
#define UNIT_MELEE_ATK_THRESHOLD       1.5f  /* atk_range (tiles) ≤ seuil → mêlée → contre-dégâts */
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
    /* Déblaiement d'un obstacle (ruine / roche minée). Ajoutés EN FIN
       d'énumération : sizeof(Unit) est inchangé, donc les sauvegardes
       existantes restent lisibles (rsec compare les tailles de section). */
    USTATE_GOTO_CLEAR,   // ← se dirige vers l'obstacle à déblayer
    USTATE_CLEARING,     // ← en train de déblayer
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
    float        manual_x, manual_y; // UBEH_MANUAL: destination (unités de combat)
                                   // OUVRIER en USTATE_GOTO_CLEAR/CLEARING :
                                   // centre de la tuile à déblayer. Réemploi sûr —
                                   // manual_* n'est lu que sous UBEH_MANUAL, un
                                   // comportement réservé aux unités de COMBAT
                                   // (la sélection de groupe exclut les ouvriers).
    int          manual_moving;    // 1 = se déplace vers manual_x/y

    // Navigation à waypoint (déblocage BFS autour du relief profond)
    MoveNav      nav;
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
/* Envoie un ouvrier déblayer l'obstacle de la tuile (tx,ty). L'appelant a
   déjà validé la tuile et débité UNIT_WORKER_CLEAR_COST. */
void unit_assign_clear  (UnitPool *up, int unit_idx, int tx, int ty);
/* 1 si la tuile porte un obstacle déblayable par un ouvrier (ruine ou
   roche minée). SOURCE UNIQUE : partagée par l'input, le rendu et la
   logique. `tp` exclut les tuiles occupées par une TOUR — une roche minée
   reste TILE_RUIN après construction, sans quoi cliquer sa propre tour
   déclencherait un déblaiement payant au lieu de la sélectionner. */
int  unit_tile_clearable(const Map *map, const TowerPool *tp, int tx, int ty);
int unit_active_limit(const MetaBonuses *bonuses, int base_count);
/* Médics rattachés à la base (bpx,bpy) — pour le cap MAX_MEDICS_PER_BASE. */
int unit_medic_count_at_base(const UnitPool *up, float bpx, float bpy);
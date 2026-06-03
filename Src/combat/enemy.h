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
    int   is_boss;        // 1 = boss de fin de chapitre (PV massifs, phases, rendu spécial)

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

    // Combat contre les unités alliées
    float melee_range;      /* portée de contact mêlée (px) — inchangée, utilisée en dernier recours */
    float unit_atk_range;   /* portée d'attaque effective sur unités (px) — peut être > melee_range  */
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

    // Raider — détachement vers un ouvrier
    int   raiding;           // 1 = cet ennemi traque un ouvrier
    int   raid_target;       // index de l'ouvrier dans UnitPool (-1 = aucun)
    float raid_base_x;       // position base cible (pour après la traque)
    float raid_base_y;

    // Feedback visuel (« jus ») : éclair blanc bref quand touché
    float hit_flash;         // 0 = aucun, >0 = intensité décroissante

    // Boss — capacité spéciale télégraphiée
    int   boss_ability;      // BossAbility (0 si non-boss)
    float ability_timer;     // cooldown avant la prochaine capacité
    float telegraph_timer;   // >0 = préavis en cours (avant l'effet)
    float boss_shield;       // >0 = invulnérable (capacité Bouclier)
} Enemy;

typedef struct {
    Enemy enemies[MAX_ENEMIES];
    int   count;
    int   raider_count;      // ennemis actuellement en mode raid
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
// DMG_PHYSICAL=0, DMG_POISON=1, DMG_ELECTRIC=2, DMG_CRYO=3, DMG_NANO=4, DMG_FIRE=5
#define DAMAGE_TYPE_COUNT 6
extern const float ENEMY_DMG_MULT[ENEMY_TYPE_COUNT][DAMAGE_TYPE_COUNT];

// ── Descriptions bestiaire ───────────────────────────────────
extern const char *ENEMY_DESC[ENEMY_TYPE_COUNT];
extern const char *ENEMY_SPEC[ENEMY_TYPE_COUNT];

// ── Paliers de brûlure (lance-flammes) ───────────────────────
#define BURN_MAX_STACKS 10

// ── Synergies élémentaires (matériaux) ───────────────────────
// Un ennemi sous effet d'état devient vulnérable → récompense la combinaison
// d'une tour de contrôle (cryo/acide) avec des tours de dégâts.
#define SYN_CRYO_VULN     1.30f  /* +30% de dégâts subis si RALENTI (gelé)      */
#define SYN_ACID_CORRODE  1.25f  /* +25% de dégâts phys./feu si EMPOISONNÉ      */

// ── Boss : capacités spéciales télégraphiées ─────────────────
typedef enum {
    BOSS_ABILITY_NONE = 0,
    BOSS_SUMMON,    // invoque des renforts sur son chemin
    BOSS_STUN,      // onde EMP : étourdit les tours proches
    BOSS_SHIELD,    // bouclier : invulnérable un court instant
} BossAbility;
#define BOSS_ABILITY_PERIOD   9.0f  /* intervalle entre deux capacités (s)      */
#define BOSS_TELEGRAPH_TIME   1.3f  /* préavis avant l'effet (s)                */
#define BOSS_STUN_RADIUS      3.0f  /* portée de l'onde EMP (tiles)             */
#define BOSS_STUN_DURATION    3.0f  /* durée d'étourdissement des tours (s)     */
#define BOSS_SHIELD_DURATION  3.5f  /* durée d'invulnérabilité (s)              */
#define BOSS_SUMMON_COUNT     3     /* renforts invoqués par capacité           */

// ── Comportement général ──────────────────────────────────────
#define ENEMY_SLOW_SPEED_MULT        0.5f  /* vitesse sous ralentissement              */
#define ENEMY_SIEGE_ATK_TIMER        1.0f  /* cooldown attaque sur base intermédiaire (s) */
#define ENEMY_MELEE_DMG_MULT         3.0f  /* multiplicateur dégâts mêlée sur unités  */
#define ENEMY_MELEE_ENGAGE_TIMER     0.3f  /* délai de dégagement après mêlée (s)     */

// ── Vitesses d'attaque mêlée par type (attaques/s) ───────────
#define ENEMY_MELEE_RATE_DEFAULT     1.0f  /* Raider (valeur par défaut)   */
#define ENEMY_MELEE_RATE_BRUTE       0.6f  /* Brute  — lent mais puissant  */
#define ENEMY_MELEE_RATE_RUNNER      2.0f  /* Runner — rafale rapide       */
#define ENEMY_MELEE_RATE_VEHICLE     0.4f  /* Véhicule — très lent         */
#define ENEMY_MELEE_RATE_GHOST       1.5f  /* Spectre                      */
#define ENEMY_MELEE_RATE_PATHBREAKER 1.2f  /* Pathbreaker                  */
#define ENEMY_MELEE_RATE_HEALER      0.5f  /* Healer — combat secondaire   */

// ── Pathbreaker ───────────────────────────────────────────────
#define ENEMY_PATHBREAKER_SPEED_MULT 1.3f  /* bonus vitesse au spawn       */

// ── Mutant ────────────────────────────────────────────────────
#define ENEMY_MUTANT_REGEN_RATE      5.0f  /* PV régénérés par seconde     */

// ── Healer ────────────────────────────────────────────────────
#define ENEMY_HEALER_HEAL_RANGE      2.0f  /* portée de soin (en tiles)    */
#define ENEMY_HEALER_HEAL_AMOUNT    15.0f  /* PV/s restaurés aux alliés    */

// ── Hunter ────────────────────────────────────────────────────
#define ENEMY_HUNTER_HUNT_RANGE       6.0f  /* portée de détection unités (en tiles)          */
#define ENEMY_HUNTER_ATK_TIMER        0.4f  /* cooldown d'attaque Hunter (s)                   */
#define ENEMY_HUNTER_UNIT_ATK_RANGE   3.0f  /* portée de tir sur unités (tiles) — longue portée*/

// ── Artillerie — attaque secondaire sur unités ────────────────
#define ENEMY_ARTY_UNIT_ATK_RANGE     3.5f  /* portée de tir sur unités (tiles)               */

// ── Artillerie ────────────────────────────────────────────────
#define ENEMY_ARTY_RANGE             4.0f  /* portée de tir (en tiles)     */
#define ENEMY_ARTY_DAMAGE           40.0f  /* dégâts par tir               */
#define ENEMY_ARTY_FIRE_TIMER        3.0f  /* cooldown entre tirs (s)      */

// ── Raider (détachement vers ouvrier) ────────────────────────
#define ENEMY_RAID_DETECT_RANGE      5.0f  /* détection ouvrier (en tiles) */
#define ENEMY_RAID_MAX_FRACTION      0.3f  /* max 30% des ennemis actifs   */
#define ENEMY_RAID_ABANDON_RANGE    14.0f  /* abandon si ouvrier trop loin */

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
// Fait apparaître un boss de fin de chapitre sur un chemin donné.
// hp_scale module ses PV selon le chapitre (croissance de campagne).
void  enemy_spawn_boss (EnemyPool *pool, int path_id, const PathSet *paths,
                        float hp_scale, int boss_chapter);
int   enemy_pool_alive (const EnemyPool *pool);
void  enemy_damage     (Enemy *e, float dmg);
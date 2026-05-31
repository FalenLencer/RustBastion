/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "campaign_data.h"   // CAMPAIGN_TOTAL = 15
#include "../combat/enemy.h"       // ENEMY_TYPE_COUNT (bestiaire)
#include "../combat/material.h"    // MAT_COUNT (bestiaire minerais)

#define META_MAGIC     0x52425354u  // "RBST"
#define META_VERSION   8            // bump : unit_discovered

// ── Niveaux maximum par amélioration ─────────────────────────
#define MAX_LVL_TOWER_DMG    5
#define MAX_LVL_TOWER_RANGE  3
#define MAX_LVL_TOWER_RATE   4
#define MAX_LVL_UNIT_HP      5
#define MAX_LVL_UNIT_DMG     4
#define MAX_LVL_START_GOLD   4
#define MAX_LVL_LIVES        3
#define MAX_LVL_SCRAP_BONUS  3
#define MAX_LVL_TOWER_LIMIT  3
#define MAX_LVL_UNIT_LIMIT   3


// ── Coûts par niveau ─────────────────────────────────────────
extern const int COST_TOWER_DMG  [MAX_LVL_TOWER_DMG];
extern const int COST_TOWER_RANGE[MAX_LVL_TOWER_RANGE];
extern const int COST_TOWER_RATE [MAX_LVL_TOWER_RATE];
extern const int COST_UNIT_HP    [MAX_LVL_UNIT_HP];
extern const int COST_UNIT_DMG   [MAX_LVL_UNIT_DMG];
extern const int COST_START_GOLD [MAX_LVL_START_GOLD];
extern const int COST_LIVES      [MAX_LVL_LIVES];
extern const int COST_SCRAP_BONUS[MAX_LVL_SCRAP_BONUS];
extern const int COST_TOWER_LIMIT[MAX_LVL_TOWER_LIMIT];
extern const int COST_UNIT_LIMIT [MAX_LVL_UNIT_LIMIT];

// ── Ordre des thèmes en campagne (cycle fixe) ─────────────────
// Les 5 thèmes sont parcourus dans cet ordre, aléatoirement tirés
// parmi la liste complète. L'ordre est dérivé du numéro de campagne.
#define CAMPAIGN_STAGES  5   // = THEME_COUNT (ancien système 5 stages)

// ── Structure de méta-progression ────────────────────────────
typedef struct {
    unsigned int magic;
    int          version;

    // Monnaie persistante (gagnée uniquement en campagne)
    int scrap;
    int total_scrap_earned;

    // Statistiques globales
    int runs_completed;       // parties arcade terminées
    int campaigns_completed;  // campagnes complètes (tous les stages)
    int best_wave;
    int total_kills;

    // Niveaux d'amélioration
    int lvl_tower_dmg;
    int lvl_tower_range;
    int lvl_tower_rate;
    int lvl_unit_hp;
    int lvl_unit_dmg;
    int lvl_start_gold;
    int lvl_lives;
    int lvl_scrap_bonus;
    int lvl_tower_limit;
    int lvl_unit_limit;
    int   endless_best_score;   // meilleur score (vague × mult × 10)
    int   endless_best_wave;    // meilleure vague atteinte en endless
    // Étoiles par acte (0=non joué, 1=complété, 2=objectif bonus)
    int act_stars[CAMPAIGN_TOTAL];  // CAMPAIGN_TOTAL = 15

    // Bestiaire — 1 si ce type d'ennemi a déjà été rencontré
    int bestiary_discovered[ENEMY_TYPE_COUNT];

    // Minerais — 1 si ce type de minerai a déjà été collecté (campagne uniquement)
    int material_discovered[MAT_COUNT];

    // Tours — 1 si ce type de tour a déjà été posé (campagne uniquement)
    // TOWER_TYPE_COUNT ne peut pas être inclus ici (dépendance circulaire avec tower.h).
    // META_TOWER_COUNT = 4 ; un _Static_assert dans tower.c le vérifie.
#define META_TOWER_COUNT 4
    int tower_discovered[META_TOWER_COUNT];

    // Unités — 1 si ce type d'unité a déjà été déployé (campagne uniquement)
    // Même contrainte circulaire que pour les tours.
    // META_UNIT_COUNT = 5 ; un _Static_assert dans unit.c le vérifie.
#define META_UNIT_COUNT 5
    int unit_discovered[META_UNIT_COUNT];
} MetaProgress;

// ── Multiplicateurs calculés depuis les niveaux ───────────────
typedef struct {
    float tower_dmg_mult;
    float tower_range_mult;
    float tower_rate_mult;
    float unit_hp_mult;
    float unit_dmg_mult;
    int   start_gold;
    int   start_lives;
    float scrap_mult;
    int   tower_limit_bonus;
    int   unit_limit_bonus;
} MetaBonuses;

// ── API ──────────────────────────────────────────────────────
void meta_init        (MetaProgress *meta);
void meta_save        (const MetaProgress *meta);
int  meta_load        (MetaProgress *meta);
void meta_compute     (const MetaProgress *meta, MetaBonuses *out);

// Fin de partie arcade (pas de ferraille)
void meta_end_of_run  (MetaProgress *meta, int wave_reached,
                       int kills, int gold_remaining);

// Fin d'un stage de campagne — retourne la ferraille gagnée
int  meta_end_of_campaign_stage(MetaProgress *meta, int wave_reached,
                                int kills, int gold_remaining,
                                int stage_index);

// Retourne l'ordre des CAMPAIGN_STAGES thèmes pour une campagne donnée
// (out_themes doit avoir CAMPAIGN_STAGES entrées)
void meta_campaign_theme_order(int seed, int out_themes[CAMPAIGN_STAGES]);

int  meta_upgrade_cost(const MetaProgress *meta, int upgrade_id);
int  meta_upgrade     (MetaProgress *meta, int upgrade_id);

// Mode endless
int  meta_endless_score(int wave, float mult);
void meta_endless_end  (MetaProgress *meta, int wave, float mult, int extracted);

// Enregistre le résultat d'un acte et retourne les étoiles (1 ou 2)
int  meta_record_act(MetaProgress *meta, int stage_index,
                     int objective_done, int bonus_done);

// Retourne 1 si un acte est débloqué (précédent complété)
int  meta_act_unlocked(const MetaProgress *meta, int stage_index);

// Retourne l'index du dernier acte complété (-1 si aucun)
int  meta_max_stage_completed(const MetaProgress *meta);
typedef enum {
    UPGRADE_TOWER_DMG   = 0,
    UPGRADE_TOWER_RANGE,
    UPGRADE_TOWER_RATE,
    UPGRADE_UNIT_HP,
    UPGRADE_UNIT_DMG,
    UPGRADE_START_GOLD,
    UPGRADE_LIVES,
    UPGRADE_SCRAP_BONUS,
    UPGRADE_TOWER_LIMIT,
    UPGRADE_UNIT_LIMIT,
    UPGRADE_COUNT
} UpgradeID;

extern const char *UPGRADE_NAMES[UPGRADE_COUNT];
extern const char *UPGRADE_DESC [UPGRADE_COUNT];
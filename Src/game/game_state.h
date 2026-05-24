/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "../map/map_gen.h"
#include "../map/pathfinding.h"
#include "../combat/enemy.h"
#include "../combat/wave.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include "meta.h"
#include "../ui/hud.h"

typedef enum { PHASE_PREP, PHASE_WAVE, PHASE_GAMEOVER } GamePhase;

struct GameState {
    Map          map;
    PathSet      enemy_paths;
    EnemyPool    enemies;
    WaveManager  wave_manager;
    TowerPool    towers;
    UnitPool     units;
    MetaProgress meta;
    MetaBonuses  bonuses;
    UIState      ui;
    GamePhase    phase;
    int          gold;
    int          lives;      // vies totales (somme des HP de toutes les bases)
    int          kills;

    // ── Mode de jeu ───────────────────────────────────────────
    int is_campaign;
    int campaign_num;
    int campaign_stage;
    int campaign_order_seed;
    int is_custom;  // 1 = partie personnalisée (pas de save)

    // ── Inventaire matériaux ──────────────────────────────────
    MaterialType inventory[MAX_INVENTORY];
    int          inventory_count;
    // ── Mode Endless ──────────────────────────────────────
    int   is_endless;          // 1 = mode arcade endless
    int   endless_series;      // série en cours (0-based, +1 tous les 10 vagues)
    float endless_multiplier;  // multiplicateur de ferraille (1.0, 1.5, 2.0...)
    int   endless_pending_extract; // 1 = fenêtre extraction à afficher
    // ── Suivi objectif de l'acte en cours ─────────────────────
    int act_objective_done;      // 1 = objectif accompli ce stage
    int act_no_unit_lost;        // 1 = aucune unité perdue (pour OBJ_NO_UNIT_LOST)
    int act_materials_collected; // compteur matériaux collectés ce stages

    // ── Achats de slots en jeu (or) ───────────────────────────
    int slots_tower_bought;      // slots tours achetés en cours de partie
    int slots_unit_bought;       // slots unités achetés en cours de partie
};

typedef struct GameState GameState;

// Retourne 1 si toutes les bases sont tombées
int  game_all_bases_fallen(const GameState *gs);

// Applique des dégâts à une base spécifique
void game_damage_base(GameState *gs, int base_id, int dmg);

void game_state_init  (GameState *gs);
void game_state_update(GameState *gs, float dt);
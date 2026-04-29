#pragma once
#include "map_gen.h"
#include "pathfinding.h"
#include "enemy.h"
#include "wave.h"
#include "tower.h"
#include "unit.h"
#include "meta.h"
#include "ui.h"

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
    int          lives;
    int          kills;

    // ── Infos mode de jeu ──────────────────────────────────
    int is_campaign;      // 0 = arcade, 1 = campagne
    int campaign_num;     // numéro de campagne (0-based)
    int campaign_stage;   // stage dans le cycle 0..CAMPAIGN_STAGES-1
                          // (thème joué = campaign_theme_order[campaign_stage])
};

typedef struct GameState GameState;

void game_state_init  (GameState *gs);
void game_state_update(GameState *gs, float dt);
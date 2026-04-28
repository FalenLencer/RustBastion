#pragma once
#include "map_gen.h"
#include "pathfinding.h"
#include "enemy.h"
#include "wave.h"
#include "tower.h"
#include "unit.h"
#include "meta.h"
#include "ui.h"      // ← en dernier, après tous les autres

typedef enum { PHASE_PREP, PHASE_WAVE, PHASE_GAMEOVER } GamePhase;

// La struct s'appelle bien GameState — correspond à la forward decl dans ui.h
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
};

// Typedef pour usage normal partout
typedef struct GameState GameState;

void game_state_init  (GameState *gs);
void game_state_update(GameState *gs, float dt);
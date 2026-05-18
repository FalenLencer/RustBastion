#pragma once
#include "enemy.h"
#include "../map/pathfinding.h"
#include "../map/map_gen.h"
#include "../map/theme.h"

typedef enum {
    WAVE_IDLE = 0,
    WAVE_SPAWNING,
    WAVE_ONGOING,
    WAVE_COMPLETE,
} WaveState;

typedef struct {
    int        number;
    WaveState  state;
    float      prep_timer;
    float      scale;

    int        total_spawned;
    int        total_to_spawn;

    // Pression par base — nombre d'ennemis envoyés vers chaque base
    // cette vague. Remis à zéro à chaque nouvelle vague.
    // Permet le comportement "intelligent" de ciblage équilibré.
    int        base_pressure[MAX_BASES];
} WaveManager;

void wave_init  (WaveManager *wm);
void wave_update(WaveManager *wm, EnemyPool *pool,
                 const PathSet *paths, const Map *map,
                 const Theme *theme, float dt);
int  wave_ready (const WaveManager *wm);
void wave_start (WaveManager *wm);
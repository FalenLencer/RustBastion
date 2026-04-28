#pragma once
#include "enemy.h"
#include "pathfinding.h"
#include "theme.h"

typedef enum {
    WAVE_IDLE = 0,    // entre deux vagues
    WAVE_SPAWNING,    // spawn en cours
    WAVE_ONGOING,     // ennemis en jeu, plus rien à spawner
    WAVE_COMPLETE,    // tous morts ou arrivés
} WaveState;

typedef struct {
    int        number;          // numéro de vague (commence à 1)
    WaveState  state;
    float      prep_timer;      // compte à rebours avant la vague
    float      scale;           // multiplicateur HP/difficulté

    int        total_spawned;   // ennemis spawned cette vague
    int        total_to_spawn;  // total prévu
} WaveManager;

void wave_init   (WaveManager *wm);
void wave_update (WaveManager *wm, EnemyPool *pool,
                  const PathSet *paths, const Theme *theme,
                  float dt, int *lives, int *gold,
                  int *kills);   // ← AJOUT : compteur kills pour méta

// Retourne 1 si on peut lancer la vague suivante
int  wave_ready  (const WaveManager *wm);
void wave_start  (WaveManager *wm);
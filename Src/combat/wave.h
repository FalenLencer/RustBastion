/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

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

    // Plafond de scaling HP/dégâts (défaut WAVE_SCALE_CAP = 6.0).
    // Mode custom : configurable entre 2.0 et 12.0.
    float      scale_cap;
    // Multiplicateur du nombre d'ennemis par vague (défaut 1.0).
    // Mode custom : 0.5 (lente) à 3.0 (extreme).
    float      count_mult;
    // Multiplicateur de vitesse des ennemis (défaut 1.0).
    // Utilisé par les mutateurs de campagne (ex : tempête de sable).
    float      speed_mult;

    int        total_spawned;
    int        total_to_spawn;

    // Pression par base — nombre d'ennemis envoyés vers chaque base
    // cette vague. Remis à zéro à chaque nouvelle vague.
    // Permet le comportement "intelligent" de ciblage équilibré.
    int        base_pressure[MAX_BASES];

    // Biais de répartition des ennemis en arcade — tiré aléatoirement
    // au lancement de chaque partie.  Vaut 1.0 en campagne (neutre).
    // >1 = type plus fréquent, <1 = type plus rare.
    float      arcade_bias[ENEMY_TYPE_COUNT];
} WaveManager;

void wave_init             (WaveManager *wm);
// Randomise les biais de répartition pour une partie arcade.
// À appeler une seule fois après wave_init(), avant la première vague.
void wave_arcade_bias_init (WaveManager *wm);
// is_campaign : 1 = mode campagne (filtre les ennemis non débloqués)
// max_stage   : campaign_stage-1 en campagne, ignoré en arcade
void wave_update           (WaveManager *wm, EnemyPool *pool,
                            const PathSet *paths, const Map *map,
                            const Theme *theme, float dt,
                            int is_campaign, int max_stage);
int  wave_ready            (const WaveManager *wm);
void wave_start            (WaveManager *wm);

// Aperçu : remplit out[] avec les types d'ennemis susceptibles d'apparaître
// à la vague `wave_num` (triés du plus probable au moins). Retourne le nombre.
int  wave_preview_types    (int wave_num, ThemeID theme, int is_campaign,
                            int max_stage, const float *bias,
                            EnemyType *out, int max_out);
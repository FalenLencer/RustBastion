/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "../map/theme.h"

#define CAMPAIGN_CHAPTERS  5
#define CAMPAIGN_ACTS      3   // actes par chapitre
#define CAMPAIGN_TOTAL     (CAMPAIGN_CHAPTERS * CAMPAIGN_ACTS)  // 15 actes

// ── Objectif de l'acte ───────────────────────────────────────
typedef enum {
    OBJ_SURVIVE_WAVES = 0,  // survivre N vagues
    OBJ_RECRUIT_UNITS,      // recruter N unités avant la vague N
    OBJ_KILL_ENEMIES,       // tuer N ennemis
    OBJ_DEFEND_BASES,       // défendre 2 bases simultanément
    OBJ_COLLECT_MATERIALS,  // collecter N matériaux
    OBJ_NO_UNIT_LOST,       // ne perdre aucune unité
} ObjectiveType;

typedef struct {
    ObjectiveType type;
    int           target;        // valeur cible (N vagues, N unités...)
    int           before_wave;   // contrainte "avant la vague X" (0 = pas de contrainte)
    const char   *description;   // texte affiché au joueur
} Objective;

// ── Données d'un acte ────────────────────────────────────────
typedef struct {
    int         chapter;         // 0-4
    int         act;             // 0-2
    const char *title;           // ex: "Premier contact"
    const char *subtitle;        // ex: "Chapitre 1 — Acte 1"
    Objective   objective;
    const char *dialog_before;   // dialogue affiché AVANT l'acte
    const char *dialog_after;    // dialogue affiché APRÈS l'acte (victoire)
    ThemeID     theme;           // thème de carte imposé
    int         min_waves;       // vagues minimum pour valider l'acte
    const char *unlock_msg;      // message de débloquage si acte terminé (peut être NULL)
} ActData;

// ── API ──────────────────────────────────────────────────────
// Retourne les données d'un acte (stage_index 0..14)
const ActData *campaign_act_get(int stage_index);

// Retourne l'index global d'un acte (chapter*3 + act)
int campaign_act_index(int chapter, int act);

// Vérifie si un objectif est accompli
int campaign_objective_check(const ActData *act,
                              int waves_done, int kills,
                              int units_alive, int materials_collected,
                              int no_unit_lost);
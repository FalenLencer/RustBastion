/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_tutorial.h ─ Tutoriel guidé (bulles d'aide séquencées).
 *  Extrait de app.c (modularisation). Voir app_tutorial.c.
 */
#pragma once
#include "app.h"

/* Démarre une partie de tutoriel (carte guidée, vagues de Raiders
 * uniquement, pas de sauvegarde) et active le guidage à l'étape 0. */
void tutorial_start (AppContext *ctx);

/* Rectangle (coords canvas) de la bulle d'aide : en haut par défaut,
 * amarrée en bas si elle masquerait une base. Sert aussi à décaler les
 * indicateurs qui partagent le haut de l'écran (pause tactique). */
Rectangle tutorial_panel_rect(const AppContext *ctx);

/* Avance le tutoriel (auto-validation des étapes d'action + clic SUIVANT).
 * À appeler AVANT l'input HUD. No-op si le tutoriel n'est pas actif. */
void tutorial_tick  (AppContext *ctx);

/* Dessine la bulle d'aide de l'étape courante. No-op si inactif/pause. */
void tutorial_render(AppContext *ctx);

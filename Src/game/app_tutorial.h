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

/* Avance le tutoriel (auto-validation des étapes d'action + clic SUIVANT).
 * À appeler AVANT l'input HUD. No-op si le tutoriel n'est pas actif. */
void tutorial_tick  (AppContext *ctx);

/* Dessine la bulle d'aide de l'étape courante. No-op si inactif/pause. */
void tutorial_render(AppContext *ctx);

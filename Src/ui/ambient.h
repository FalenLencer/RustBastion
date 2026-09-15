/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/ambient.h — couche d'ambiance météo 2D par thème de carte.

   Particules dérivantes plein écran (espace CANVAS) : poussière ocre
   (terres brûlées), rafales de sable (désert), pluie fine (marais),
   cendres (ville), suies montantes (usine). Pool statique, opacité
   faible, rebouclage aux bords (couche continue).

   Intégration (app.c) :
   - ambient_update(dt × speed_mult) à côté de fx_update — jamais
     appelé en pause (tactique/fiche/menu) → figé.
   - ambient_render(theme) APRÈS EndMode2D (carte) et AVANT ui_render
     (le HUD recouvre sa bande). Re-seed automatique si le thème change.
   Toggle fx_effects (g_fx.enabled) respecté partout.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../map/theme.h"

void ambient_update(float dt);
void ambient_render(ThemeID theme);

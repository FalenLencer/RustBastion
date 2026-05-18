/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../combat/tower.h"
#include "../combat/unit.h"

// ════════════════════════════════════════════════════
// ASSETS — splash arts tours et unités
//
// Chargés une fois au démarrage (après InitWindow),
// libérés avant CloseWindow.
// Texture.id == 0 signifie "pas de splash art" pour ce type.
// ════════════════════════════════════════════════════

extern Texture2D g_tower_splash[TOWER_TYPE_COUNT];
extern Texture2D g_unit_splash [UNIT_TYPE_COUNT];

// Police globale chargée avec les codepoints étendus (accents, tiret cadratin, etc.)
extern Font g_font;

void assets_load  (void);
void assets_unload(void);

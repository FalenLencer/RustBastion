/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../ui/hud.h"

// Dimensions du canvas virtuel
#define VIRT_W  (MAP_W * TILE_SIZE)
#define VIRT_H  (MAP_H * TILE_SIZE + UI_HUD_HEIGHT)

// Calcule le scaling non-uniforme pour remplir toute la fenêtre sans bordures noires.
// sx = sw/VIRT_W, sy = sh/VIRT_H — échelles indépendantes par axe.
// ox/oy = 0 (pas de décalage : le canvas remplit tout).
void canvas_compute(int sw, int sh, float *sx, float *sy, float *ox, float *oy);

// Applique les offsets souris (échelles x et y séparées)
void canvas_set_mouse_offset(float ox, float oy, float sx, float sy);
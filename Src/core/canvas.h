#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../ui/ui.h"

// Dimensions du canvas virtuel
#define VIRT_W  (MAP_W * TILE_SIZE)
#define VIRT_H  (MAP_H * TILE_SIZE + UI_HUD_HEIGHT)

// Calcule le scaling et l'offset pour adapter le canvas à la fenêtre
void canvas_compute(int sw, int sh, float *s, float *ox, float *oy);

// Applique les offsets souris (doit être appelé chaque frame)
void canvas_set_mouse_offset(float ox, float oy, float scale);
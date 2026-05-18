/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../map/pathfinding.h"
#include "../combat/enemy.h"
#include "../combat/tower.h"
#include "../combat/unit.h"
#include "hud.h"
#include "../game/meta.h"

typedef struct GameState GameState;

extern Color TOWER_FILL[TOWER_TYPE_COUNT];
extern Color UNIT_FILL [UNIT_TYPE_COUNT];
extern Color PROJ_COLOR[TOWER_TYPE_COUNT];

// Décalage horizontal de la carte dans le canvas (centrage, px virtuels)
extern int g_map_x_off;
// Largeur réelle du canvas virtuel (adapté au ratio de la fenêtre)
extern int g_canvas_virt_w;

Color renderer_tower_color(TowerType type);
Color renderer_unit_color (UnitType  type);

void render_map          (const Map *map);
void render_bases        (const Map *map);
void render_paths        (const PathSet *ps);
void render_enemies      (const EnemyPool *pool);
void render_towers       (const TowerPool *tp);
void render_spawn_exclusion_zones(const Map *map);
void render_projectiles  (const TowerPool *tp);
void render_units        (const UnitPool *up);
void render_tower_preview(const Map *map, const TowerPool *tp,
                          TowerType type, int tile_x, int tile_y);
void render_tile_detail  (int px, int py, TileType type, ThemeID theme);
void render_hud          (const GameState *gs);
void render_deposits     (const Map *map);
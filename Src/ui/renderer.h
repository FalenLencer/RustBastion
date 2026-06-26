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
// Couleur d'identité par matériau (source unique, cf. MaterialType).
extern Color MATERIAL_COLORS[MAT_COUNT];

// Décalage horizontal de la carte dans le canvas (centrage, px virtuels)
extern int g_map_x_off;
// Largeur réelle du canvas virtuel (adapté au ratio de la fenêtre)
extern int g_canvas_virt_w;
// Largeur de base de la carte (MAP_W * TILE_SIZE), sans le padding latéral
extern int g_canvas_virt_w_base;
// Hauteur du canvas virtuel (MAP_H * TILE_SIZE + UI_HUD_HEIGHT)
extern int g_canvas_virt_h;
// Facteur de zoom de la carte pour les grandes cartes (1.0 = taille standard)
// La carte est rendue dans un Camera2D avec ce zoom pour tenir dans le canvas fixe.
extern float g_map_render_scale;
// Zoom JOUEUR (molette) + décalage (pan). Le rendu carte ET le mappage souris
// passent par les helpers ci-dessous (source unique → alignement garanti).
extern float g_map_zoom;             // 1..3
extern float g_map_pan_x, g_map_pan_y;
extern int   g_colorblind;           // 1 = palette ennemis daltonien-safe
extern int   g_units_3d;             // 1 = unités/ennemis en 3D (modèles) ; 0 = sprites 2D
float   map_eff_scale(void);                 // g_map_render_scale * g_map_zoom
Vector2 map_origin(void);                    // (OX, OY) du rendu carte
Vector2 map_screen_to_world(Vector2 s);      // souris (coords canvas) → monde (px)

Color renderer_tower_color(TowerType type);
Color renderer_unit_color (UnitType  type);
Color renderer_enemy_color(EnemyType type);
// Couleur d'identité d'une base (distincte par index) — partagée entre le
// bunker, l'onde de repérage et la barre de vie pour différencier les bases.
Color renderer_base_color (int base_idx, int is_primary);

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
void render_hud          (const GameState *gs);
void render_deposits     (const Map *map);
void render_dropped_mats (const DroppedMat *mats, int count);
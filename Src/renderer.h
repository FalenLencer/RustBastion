#pragma once
#include "raylib.h"
#include "map_gen.h"
#include "pathfinding.h"
#include "enemy.h"
#include "tower.h"
#include "unit.h"
#include "ui.h"
#include "meta.h"

// Forward declaration — évite l'inclusion circulaire avec game_state.h
typedef struct GameState GameState;

// ── Tableaux de couleurs — extern pour ui.c ──────────────────
extern Color TOWER_FILL[TOWER_TYPE_COUNT];
extern Color UNIT_FILL [UNIT_TYPE_COUNT];
extern Color PROJ_COLOR[TOWER_TYPE_COUNT];

// ── Scaling global pour adaptation fenêtre ───────────────────
extern float RENDER_SCALE;  // ratio de scaling (1.0 = taille par défaut)

// ── Fonctions de couleur ──────────────────────────────────────
Color renderer_tower_color(TowerType type);
Color renderer_unit_color (UnitType  type);

// ── Fonctions de rendu ────────────────────────────────────────
void render_map          (const Map *map);
void render_paths        (const PathSet *ps);
void render_enemies      (const EnemyPool *pool);
void render_towers       (const TowerPool *tp);
void render_projectiles  (const TowerPool *tp);
void render_units        (const UnitPool *up);
void render_tower_preview(const Map *map, const TowerPool *tp,
                          TowerType type, int tile_x, int tile_y);
void render_tile_detail  (int px, int py, TileType type, ThemeID theme);
void render_hud          (const GameState *gs);
void render_gameover     (const GameState *gs);

// render_meta_menu est retiré — le système de menu (menu.c) gère son propre rendu
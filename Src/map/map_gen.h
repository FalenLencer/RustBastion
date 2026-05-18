/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "theme.h"
#include "../combat/material.h"

#define MAP_W       28
#define MAP_H       16
#define TILE_SIZE   40

// Spawns et bases sont indépendants et en quantités libres
#define MAX_PATHS    3   // spawners max
#define MAX_BASES    2   // bases défendables max (1 principale + 1 secondaire)

// Zone autour de chaque spawn interdite à la construction
#define SPAWN_EXCLUSION_RADIUS  3

typedef enum {
    TILE_GROUND = 0,
    TILE_RUIN,
    TILE_WATER,
    TILE_PATH,
    TILE_SPAWN,
    TILE_BASE,
} TileType;

typedef struct {
    TileType type;
    int      passable;
    int      buildable;
    float    noise_val;
    int      path_id;   // -1 = aucun
} Tile;

typedef enum { EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM } Edge;
typedef struct { int x, y; } Point;

// ── Base défendable ───────────────────────────────────────────
typedef struct {
    Point pos;
    int   hp;
    int   max_hp;
    int   is_primary;   // 1 = game over si elle tombe
    int   active;
    int   damage;       // dégâts infligés par ennemi qui la touche
} BaseInfo;

// ── Chemin spawn → base ───────────────────────────────────────
typedef struct {
    Point spawn;
    Point base;
    Edge  spawn_edge;
    int   base_id;      // index dans Map.bases[]
    int   active;
} PathDef;

typedef struct {
    Tile     tiles[MAP_H][MAP_W];
    PathDef  paths[MAX_PATHS];
    int      path_count;
    int      seed;
    ThemeID  theme;

    BaseInfo bases[MAX_BASES];
    int      base_count;

    MaterialDeposit deposits[MAX_MATERIAL_DEPOSITS];
    int             deposit_count;
} Map;

void generate_map(Map *map, int seed, int min_dist, ThemeID theme_id);
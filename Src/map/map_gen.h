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

// Taille maximale pour les tableaux statiques (carte personnalisée)
#define MAX_MAP_W   56
#define MAX_MAP_H   32

// Spawns et bases sont indépendants et en quantités libres
#define MAX_PATHS   20   // spawners max (valeurs >20 clampées à la génération)
#define MAX_BASES   10   // bases défendables max (valeurs >10 clampées à la génération)

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
    int   is_primary;    // 1 = game over si elle tombe
    int   active;
    int   damage;        // dégâts infligés par ennemi qui la touche
    int   repair_count;  // nombre de réparations effectuées (coût croissant)
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
    Tile     tiles[MAX_MAP_H][MAX_MAP_W];
    int      w, h;          // dimensions réelles (tuiles), au plus MAX_MAP_W × MAX_MAP_H
    PathDef  paths[MAX_PATHS];
    int      path_count;
    int      seed;
    ThemeID  theme;

    BaseInfo bases[MAX_BASES];
    int      base_count;

    MaterialDeposit deposits[MAX_MATERIAL_DEPOSITS];
    int             deposit_count;
} Map;

// forced_bases    : 0 = aléatoire, >0 = nombre exact (clampé à MAX_BASES=10)
// forced_spawns   : 0 = aléatoire, >0 = nombre exact (clampé à MAX_PATHS=20)
// forced_deposits : 0 = aléatoire (1-4), >0 = nombre exact (clampé à MAX_MATERIAL_DEPOSITS=20)
// map_w / map_h   : 0 = MAP_W / MAP_H (défaut), sinon taille en tuiles
void generate_map(Map *map, int seed, int min_dist, ThemeID theme_id,
                  int forced_bases, int forced_spawns,
                  int forced_deposits, int map_w, int map_h);
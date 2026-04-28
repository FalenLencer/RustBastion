#pragma once
#include "theme.h"

#define MAP_W       28
#define MAP_H       16
#define TILE_SIZE   40
#define MAX_PATHS    3   // nombre maximum de chemins simultanés

typedef enum {
    TILE_GROUND = 0,
    TILE_RUIN,
    TILE_WATER,
    TILE_PATH,       // chemin générique (partagé visuellement)
    TILE_SPAWN,
    TILE_BASE,
} TileType;

typedef struct {
    TileType type;
    int      passable;
    int      buildable;
    float    noise_val;
    int      path_id;    // ← quel chemin passe ici (-1 = aucun, 0/1/2 = chemin N)
} Tile;

typedef enum { EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM } Edge;
typedef struct { int x, y; } Point;

typedef struct {
    Point spawn;
    Point base;        // même valeur pour tous — base commune
    Edge  spawn_edge;  // chaque chemin a son propre spawn
    int   active;
} PathDef;

typedef struct {
    Tile    tiles[MAP_H][MAP_W];
    PathDef paths[MAX_PATHS];   // définitions des chemins
    int     path_count;         // combien de chemins actifs
    int     seed;
    ThemeID theme;
} Map;

void generate_map(Map *map, int seed, int min_dist, ThemeID theme_id);
#pragma once
#include "map_gen.h"

typedef struct {
    Point steps[MAP_W * MAP_H];
    int   len;
    int   found;
    int   path_id;   // index du chemin dans Map.paths[]
    int   base_id;   // index de la base cible dans Map.bases[]
} Path;

typedef struct {
    Path paths[MAX_PATHS];
    int  count;
} PathSet;

void astar_all   (const Map *map, PathSet *out);
void astar_single(const Map *map, Point spawn, Point base,
                  int path_id, int base_id, Path *out);
void pathset_apply(Map *map, const PathSet *ps);
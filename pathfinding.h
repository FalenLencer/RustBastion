#pragma once
#include "map_gen.h"

typedef struct {
    Point steps[MAP_W * MAP_H];
    int   len;
    int   found;
    int   path_id;   // quel chemin cette Path représente
} Path;

typedef struct {
    Path paths[MAX_PATHS];
    int  count;          // chemins valides trouvés par A*
} PathSet;

// Calcule A* pour tous les chemins de la carte
void astar_all(const Map *map, PathSet *out);

// Calcule A* pour un seul chemin (spawn/base donnés)
void astar_single(const Map *map, Point spawn, Point base, int path_id, Path *out);

// Nettoie les PATH inutilisés et force les tuiles A* en PATH
void pathset_apply(Map *map, const PathSet *ps);
#include "pathfinding.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// ════════════════════════════════════════════════════
// STRUCTURES INTERNES A*
// ════════════════════════════════════════════════════

#define MAX_NODES (MAP_W * MAP_H)

typedef struct {
    int x, y;
    int g;          // coût depuis le spawn
    int f;          // g + heuristique
    int parent;     // index dans nodes[], -1 si racine
} Node;

// Tas min sur f (priority queue simple)
typedef struct {
    int indices[MAX_NODES];  // indices dans nodes[]
    int size;
} MinHeap;

static Node   nodes[MAX_NODES];
static int    node_count;

// Table d'index : node_index[y][x] = index dans nodes[], -1 si absent
static int    node_index[MAP_H][MAP_W];

// Ensembles ouverts / fermés
static uint8_t in_open  [MAP_H][MAP_W];
static uint8_t in_closed[MAP_H][MAP_W];

static MinHeap heap;

// ── Tas min ──────────────────────────────────────────────────

static void heap_push(int idx) {
    int i = heap.size++;
    heap.indices[i] = idx;
    // Remonte
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (nodes[heap.indices[parent]].f <= nodes[heap.indices[i]].f) break;
        int tmp = heap.indices[parent];
        heap.indices[parent] = heap.indices[i];
        heap.indices[i] = tmp;
        i = parent;
    }
}

static int heap_pop(void) {
    int top = heap.indices[0];
    heap.indices[0] = heap.indices[--heap.size];
    // Descend
    int i = 0;
    for (;;) {
        int l = 2*i+1, r = 2*i+2, smallest = i;
        if (l < heap.size && nodes[heap.indices[l]].f < nodes[heap.indices[smallest]].f) smallest = l;
        if (r < heap.size && nodes[heap.indices[r]].f < nodes[heap.indices[smallest]].f) smallest = r;
        if (smallest == i) break;
        int tmp = heap.indices[i];
        heap.indices[i] = heap.indices[smallest];
        heap.indices[smallest] = tmp;
        i = smallest;
    }
    return top;
}

// ── Heuristique Manhattan ────────────────────────────────────

static int heuristic(int ax, int ay, int bx, int by) {
    return abs(ax - bx) + abs(ay - by);
}

// ── Voisins (4 directions) ───────────────────────────────────

static const int DX[4] = { 1, -1,  0,  0 };
static const int DY[4] = { 0,  0,  1, -1 };

static int move_cost(const Map *map, int x, int y) {
    switch (map->tiles[y][x].type) {
        case TILE_PATH:  return 1;    // chemin normal — coût minimal
        case TILE_SPAWN: return 1;    // spawn = début du chemin
        case TILE_BASE:  return 1;    // base  = fin du chemin
        case TILE_GROUND:return 50;   // terrain constructible — très cher
        case TILE_RUIN:  return 80;   // ruine — encore plus cher
        default:         return 100;  // eau / bloquant — jamais emprunté (ou les enemis doivent crée un ponts ?)
    }
}

// ════════════════════════════════════════════════════
// POINT D'ENTRÉE PUBLIC
// ════════════════════════════════════════════════════

// Lance A* pour chaque chemin défini dans la carte
void astar_all(const Map *map, PathSet *out) {
    out->count = 0;
    for (int i = 0; i < map->path_count && i < MAX_PATHS; i++) {
        if (!map->paths[i].active) continue;
        astar_single(map,
                     map->paths[i].spawn,
                     map->paths[i].base,
                     i,
                     &out->paths[out->count]);
        if (out->paths[out->count].found)
            out->count++;
    }
}

// astar_single = ton astar() actuel, renommé + path_id en paramètre
void astar_single(const Map *map, Point spawn, Point base, int path_id, Path *out) {
    out->len    = 0;
    out->found  = 0;
    out->path_id = path_id;
    
    out->len   = 0;
    out->found = 0;

    // ── Init ─────────────────────────────────────────────
    memset(node_index, -1, sizeof(node_index));
    memset(in_open,     0, sizeof(in_open));
    memset(in_closed,   0, sizeof(in_closed));
    node_count = 0;
    heap.size  = 0;

    int sx = spawn.x, sy = spawn.y;
    int ex = base.x,  ey = base.y;

    // Noeud de départ
    int start_idx = node_count++;
    nodes[start_idx] = (Node){
        .x = sx, .y = sy,
        .g = 0,
        .f = heuristic(sx, sy, ex, ey),
        .parent = -1
    };
    node_index[sy][sx] = start_idx;
    in_open[sy][sx]    = 1;
    heap_push(start_idx);

    // ── Boucle principale ─────────────────────────────────
    while (heap.size > 0) {
        int cur_idx = heap_pop();
        Node *cur   = &nodes[cur_idx];

        in_open[cur->y][cur->x]   = 0;
        in_closed[cur->y][cur->x] = 1;

        // Arrivée !
        if (cur->x == ex && cur->y == ey) {
            // Reconstruit le chemin en remontant les parents
            int tmp_path[MAX_NODES][2];
            int tmp_len = 0;
            int idx = cur_idx;
            while (idx != -1) {
                tmp_path[tmp_len][0] = nodes[idx].x;
                tmp_path[tmp_len][1] = nodes[idx].y;
                tmp_len++;
                idx = nodes[idx].parent;
            }
            // Inverse (spawn → base)
            for (int i = 0; i < tmp_len; i++) {
                out->steps[i].x = tmp_path[tmp_len - 1 - i][0];
                out->steps[i].y = tmp_path[tmp_len - 1 - i][1];
            }
            out->len   = tmp_len;
            out->found = 1;
            return;
        }

        // Explore les 4 voisins
        for (int d = 0; d < 4; d++) {
            int nx = cur->x + DX[d];
            int ny = cur->y + DY[d];

            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!map->tiles[ny][nx].passable)  continue;  // mur
            if (in_closed[ny][nx])             continue;  // déjà traité

            int tentative_g = cur->g + move_cost(map, nx, ny);
            int nb_idx = node_index[ny][nx];

            if (nb_idx == -1) {
                // Nouveau noeud
                nb_idx = node_count++;
                nodes[nb_idx] = (Node){
                    .x = nx, .y = ny,
                    .g = tentative_g,
                    .f = tentative_g + heuristic(nx, ny, ex, ey),
                    .parent = cur_idx
                };
                node_index[ny][nx] = nb_idx;
                in_open[ny][nx]    = 1;
                heap_push(nb_idx);

            } else if (tentative_g < nodes[nb_idx].g) {
                // Chemin plus court trouvé vers un noeud existant
                nodes[nb_idx].g      = tentative_g;
                nodes[nb_idx].f      = tentative_g + heuristic(nx, ny, ex, ey);
                nodes[nb_idx].parent = cur_idx;
                // Re-push (le tas garde les deux, le plus vieux sera ignoré via in_closed)
                if (!in_open[ny][nx]) {
                    in_open[ny][nx] = 1;
                    heap_push(nb_idx);
                }
            }
        }
    }
    // Pas de chemin trouvé — spawn et base sont déconnectés
    // Cela peut arriver sur certains seeds : on régénère dans main.c
}

// ════════════════════════════════════════════════════
// NETTOYAGE ET APPLICATION DES CHEMINS A* SUR LA CARTE
// ════════════════════════════════════════════════════

void pathset_apply(Map *map, const PathSet *ps) {

    // ── PASSE 1 : remet toutes les tuiles PATH en terrain ────
    // Tout ce que walk_to a tracé mais qu'A* n'emprunte pas
    // est converti en GROUND ou RUIN selon le bruit de Perlin
    const Theme *th = theme_get(map->theme);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (map->tiles[y][x].type == TILE_PATH) {
                float n = map->tiles[y][x].noise_val;
                if (n < th->noise.ruin_thresh) {
                    map->tiles[y][x].type      = TILE_RUIN;
                    map->tiles[y][x].passable  = 0;
                    map->tiles[y][x].buildable = 1;
                } else {
                    map->tiles[y][x].type      = TILE_GROUND;
                    map->tiles[y][x].passable  = 1;
                    map->tiles[y][x].buildable = 1;
                }
                map->tiles[y][x].path_id = -1;
            }
        }
    }

    // ── PASSE 2 : force en PATH uniquement les tuiles A* ─────
    for (int p = 0; p < ps->count; p++) {
        const Path *path = &ps->paths[p];
        if (!path->found) continue;

        for (int i = 0; i < path->len; i++) {
            int x = path->steps[i].x;
            int y = path->steps[i].y;

            // Ne touche pas spawn et base
            if (map->tiles[y][x].type == TILE_SPAWN) continue;
            if (map->tiles[y][x].type == TILE_BASE)  continue;

            map->tiles[y][x].type      = TILE_PATH;
            map->tiles[y][x].passable  = 1;
            map->tiles[y][x].buildable = 0;
            // path_id = celui du chemin qui passe ici
            // si plusieurs chemins se croisent, garde le plus petit
            if (map->tiles[y][x].path_id == -1 ||
                path->path_id < map->tiles[y][x].path_id)
                map->tiles[y][x].path_id = path->path_id;
        }
    }

    // ── PASSE 3 : re-pose spawn et base par-dessus ───────────
    // (A* a pu les écraser dans la passe 2)
    for (int i = 0; i < map->path_count; i++) {
        Point sp = map->paths[i].spawn;
        map->tiles[sp.y][sp.x].type      = TILE_SPAWN;
        map->tiles[sp.y][sp.x].passable  = 1;
        map->tiles[sp.y][sp.x].buildable = 0;
    }
    if (map->path_count > 0) {
        Point base = map->paths[0].base;
        map->tiles[base.y][base.x].type      = TILE_BASE;
        map->tiles[base.y][base.x].passable  = 1;
        map->tiles[base.y][base.x].buildable = 0;
    }

    // ── PASSE 4 : supprime les spawns sans chemin valide ─────
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;

        Point sp = map->paths[i].spawn;

        // Cherche si ce spawn a un chemin valide dans le PathSet
        int has_valid_path = 0;
        for (int p = 0; p < ps->count; p++) {
            if (ps->paths[p].found &&
                ps->paths[p].path_id == i) {
                has_valid_path = 1;
                break;
            }
        }

        // Pas de chemin valide → on efface le spawn
        if (!has_valid_path) {
            float n = map->tiles[sp.y][sp.x].noise_val;
            const Theme *th = theme_get(map->theme);

            map->tiles[sp.y][sp.x].path_id  = -1;
            map->paths[i].active             = 0;

            if (n < th->noise.ruin_thresh) {
                map->tiles[sp.y][sp.x].type      = TILE_RUIN;
                map->tiles[sp.y][sp.x].passable  = 0;
                map->tiles[sp.y][sp.x].buildable = 1;
            } else {
                map->tiles[sp.y][sp.x].type      = TILE_GROUND;
                map->tiles[sp.y][sp.x].passable  = 1;
                map->tiles[sp.y][sp.x].buildable = 1;
            }
        }
    }

    // ── Recalcule path_count après nettoyage ─────────────────
    int valid = 0;
    for (int i = 0; i < map->path_count; i++)
        if (map->paths[i].active) valid++;
    map->path_count = valid;
}
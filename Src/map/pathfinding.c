#include "pathfinding.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ════════════════════════════════════════════════════
   STRUCTURES INTERNES A*
   ════════════════════════════════════════════════════ */

#define MAX_NODES (MAP_W * MAP_H)

typedef struct {
    int x, y;
    int g;
    int f;
    int parent;
} Node;

typedef struct {
    int indices[MAX_NODES];
    int size;
} MinHeap;

static Node    nodes[MAX_NODES];
static int     node_count;
static int     node_index[MAP_H][MAP_W];
static uint8_t in_open  [MAP_H][MAP_W];
static uint8_t in_closed[MAP_H][MAP_W];
static MinHeap heap;

/* ── Tas min ──────────────────────────────────────────────── */

static void heap_push(int idx) {
    int i = heap.size++;
    heap.indices[i] = idx;
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

static int heuristic(int ax, int ay, int bx, int by) {
    return abs(ax - bx) + abs(ay - by);
}

static const int DX[4] = { 1, -1,  0,  0 };
static const int DY[4] = { 0,  0,  1, -1 };

static int move_cost(const Map *map, int x, int y) {
    switch (map->tiles[y][x].type) {
        case TILE_PATH:   return 1;
        case TILE_SPAWN:  return 1;
        case TILE_BASE:   return 1;
        case TILE_GROUND: return 50;
        case TILE_RUIN:   return 80;
        default:          return 100;
    }
}

/* ════════════════════════════════════════════════════
   ASTAR SINGLE
   Calcule le chemin spawn→base pour un path_id donné.
   base_id identifie quelle BaseInfo[] est la cible.
   ════════════════════════════════════════════════════ */
void astar_single(const Map *map, Point spawn, Point base,
                  int path_id, int base_id, Path *out)
{
    out->len     = 0;
    out->found   = 0;
    out->path_id = path_id;
    out->base_id = base_id;   // ← propagé depuis map->paths[i].base_id

    memset(node_index, -1, sizeof(node_index));
    memset(in_open,     0, sizeof(in_open));
    memset(in_closed,   0, sizeof(in_closed));
    node_count = 0;
    heap.size  = 0;

    int sx = spawn.x, sy = spawn.y;
    int ex = base.x,  ey = base.y;

    int start_idx = node_count++;
    nodes[start_idx] = (Node){
        .x      = sx,
        .y      = sy,
        .g      = 0,
        .f      = heuristic(sx, sy, ex, ey),
        .parent = -1
    };
    node_index[sy][sx] = start_idx;
    in_open[sy][sx]    = 1;
    heap_push(start_idx);

    while (heap.size > 0) {
        int cur_idx = heap_pop();
        Node *cur   = &nodes[cur_idx];

        in_open[cur->y][cur->x]   = 0;
        in_closed[cur->y][cur->x] = 1;

        if (cur->x == ex && cur->y == ey) {
            int tmp_path[MAX_NODES][2];
            int tmp_len = 0;
            int idx = cur_idx;
            while (idx != -1) {
                tmp_path[tmp_len][0] = nodes[idx].x;
                tmp_path[tmp_len][1] = nodes[idx].y;
                tmp_len++;
                idx = nodes[idx].parent;
            }
            for (int i = 0; i < tmp_len; i++) {
                out->steps[i].x = tmp_path[tmp_len - 1 - i][0];
                out->steps[i].y = tmp_path[tmp_len - 1 - i][1];
            }
            out->len   = tmp_len;
            out->found = 1;
            return;
        }

        for (int d = 0; d < 4; d++) {
            int nx = cur->x + DX[d];
            int ny = cur->y + DY[d];

            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!map->tiles[ny][nx].passable)  continue;
            if (in_closed[ny][nx])             continue;

            int tentative_g = cur->g + move_cost(map, nx, ny);
            int nb_idx = node_index[ny][nx];

            if (nb_idx == -1) {
                if (node_count >= MAX_NODES) continue;

                nb_idx = node_count++;
                nodes[nb_idx] = (Node){
                    .x      = nx,
                    .y      = ny,
                    .g      = tentative_g,
                    .f      = tentative_g + heuristic(nx, ny, ex, ey),
                    .parent = cur_idx
                };
                node_index[ny][nx] = nb_idx;
                in_open[ny][nx]    = 1;
                heap_push(nb_idx);

            } else if (tentative_g < nodes[nb_idx].g) {
                nodes[nb_idx].g      = tentative_g;
                nodes[nb_idx].f      = tentative_g + heuristic(nx, ny, ex, ey);
                nodes[nb_idx].parent = cur_idx;
                if (!in_open[ny][nx]) {
                    in_open[ny][nx] = 1;
                    heap_push(nb_idx);
                }
            }
        }
    }
    /* Pas de chemin trouvé */
}

/* ════════════════════════════════════════════════════
   ASTAR ALL
   Lance astar_single pour chaque chemin actif.
   Passe map->paths[i].base_id à chaque appel.
   ════════════════════════════════════════════════════ */
void astar_all(const Map *map, PathSet *out) {
    out->count = 0;
    for (int i = 0; i < map->path_count && i < MAX_PATHS; i++) {
        if (!map->paths[i].active) continue;

        astar_single(map,
                     map->paths[i].spawn,
                     map->paths[i].base,
                     i,
                     map->paths[i].base_id,   // ← base_id propagé
                     &out->paths[out->count]);

        if (out->paths[out->count].found)
            out->count++;
    }
}

/* ════════════════════════════════════════════════════
   PATHSET APPLY
   Nettoie les PATH inutilisés, force les tuiles A*,
   re-pose tous les spawns et TOUTES les bases.
   ════════════════════════════════════════════════════ */
void pathset_apply(Map *map, const PathSet *ps) {

    const Theme *th = theme_get(map->theme);

    /* Passe 1 : remet toutes les tuiles PATH en terrain */
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

    /* Passe 2 : force en PATH les tuiles empruntées par A* */
    for (int p = 0; p < ps->count; p++) {
        const Path *path = &ps->paths[p];
        if (!path->found) continue;

        for (int i = 0; i < path->len; i++) {
            int x = path->steps[i].x;
            int y = path->steps[i].y;

            if (map->tiles[y][x].type == TILE_SPAWN) continue;
            if (map->tiles[y][x].type == TILE_BASE)  continue;

            map->tiles[y][x].type      = TILE_PATH;
            map->tiles[y][x].passable  = 1;
            map->tiles[y][x].buildable = 0;
            if (map->tiles[y][x].path_id == -1 ||
                path->path_id < map->tiles[y][x].path_id)
                map->tiles[y][x].path_id = path->path_id;
        }
    }

    /* Passe 3 : re-pose tous les spawns */
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp = map->paths[i].spawn;
        map->tiles[sp.y][sp.x].type      = TILE_SPAWN;
        map->tiles[sp.y][sp.x].passable  = 1;
        map->tiles[sp.y][sp.x].buildable = 0;
    }

    /* Passe 3b : re-pose TOUTES les bases (pas seulement paths[0].base) */
    for (int b = 0; b < map->base_count; b++) {
        if (!map->bases[b].active) continue;
        Point bp = map->bases[b].pos;
        map->tiles[bp.y][bp.x].type      = TILE_BASE;
        map->tiles[bp.y][bp.x].passable  = 1;
        map->tiles[bp.y][bp.x].buildable = 0;
        map->tiles[bp.y][bp.x].path_id   = -1;
    }

    /* Passe 4 : supprime les spawns sans chemin A* valide */
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;

        Point sp = map->paths[i].spawn;
        int has_valid = 0;
        for (int p = 0; p < ps->count; p++) {
            if (ps->paths[p].found && ps->paths[p].path_id == i) {
                has_valid = 1;
                break;
            }
        }

        if (!has_valid) {
            float n = map->tiles[sp.y][sp.x].noise_val;
            map->tiles[sp.y][sp.x].path_id = -1;
            map->paths[i].active           = 0;

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

    /* Recalcule path_count */
    int valid = 0;
    for (int i = 0; i < map->path_count; i++)
        if (map->paths[i].active) valid++;
    map->path_count = valid;
}
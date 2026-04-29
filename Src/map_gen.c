#include "map_gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ════════════════════════════════════════════════════
// PRNG xorshift32
// ════════════════════════════════════════════════════
static uint32_t rng_state;
static void     rng_init(uint32_t s) { rng_state = s ? s : 1u; }
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static float rng_float(void) { return (float)(rng_next()&0x00ffffff)/(float)0x01000000; }
static int   rng_int(int n)  { return (int)(rng_float()*(float)n); }

// ════════════════════════════════════════════════════
// PERLIN 2D
// ════════════════════════════════════════════════════
static int perm[512];

static void build_perm(int seed) {
    rng_init((uint32_t)seed);
    for (int i = 0; i < 256; i++) perm[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = rng_int(i+1);
        int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    for (int i = 0; i < 256; i++) perm[256+i] = perm[i];
}

static float fade(float t) { return t*t*t*(t*(t*6-15)+10); }
static float flp(float a,float b,float t) { return a+t*(b-a); }
static float grad(int h,float x,float y) {
    switch(h&3){case 0:return x+y;case 1:return -x+y;case 2:return x-y;default:return -x-y;}
}
static float perlin2d(float x,float y) {
    int xi=(int)floorf(x)&255, yi=(int)floorf(y)&255;
    float xf=x-floorf(x), yf=y-floorf(y);
    float u=fade(xf), v=fade(yf);
    int aa=perm[perm[xi]+yi], ba=perm[perm[xi+1]+yi];
    int ab=perm[perm[xi]+yi+1], bb=perm[perm[xi+1]+yi+1];
    return flp(flp(grad(aa,xf,yf),grad(ba,xf-1,yf),u),
               flp(grad(ab,xf,yf-1),grad(bb,xf-1,yf-1),u),v);
}

// ════════════════════════════════════════════════════
// TERRAIN
// ════════════════════════════════════════════════════
static void gen_terrain(Map *map, const Theme *th) {
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            float n  = perlin2d(x*0.18f, y*0.18f);
            n       += perlin2d(x*0.36f, y*0.36f)*0.5f;
            n       += perlin2d(x*0.72f, y*0.72f)*0.25f;
            n = (n/1.75f+1.0f)*0.5f;

            map->tiles[y][x].noise_val = n;
            map->tiles[y][x].path_id  = -1;

            // Seuils depuis le thème
            if (n < th->noise.water_thresh) {
                map->tiles[y][x].type      = TILE_WATER;
                map->tiles[y][x].passable  = 0;
                map->tiles[y][x].buildable = 0;
            } else if (n < th->noise.ruin_thresh) {
                map->tiles[y][x].type      = TILE_RUIN;
                map->tiles[y][x].passable  = 0;
                map->tiles[y][x].buildable = 1;
            } else {
                map->tiles[y][x].type      = TILE_GROUND;
                map->tiles[y][x].passable  = 1;
                map->tiles[y][x].buildable = 1;
            }
        }
    }
}

// ════════════════════════════════════════════════════
// CHEMIN ORGANIQUE (identique à avant, path_id en plus)
// ════════════════════════════════════════════════════
static int manhattan(Point a, Point b) { return abs(a.x-b.x)+abs(a.y-b.y); }

static Point edge_point(Edge edge) {
    switch(edge) {
        case EDGE_LEFT:   return (Point){0,        1+rng_int(MAP_H-2)};
        case EDGE_RIGHT:  return (Point){MAP_W-1,  1+rng_int(MAP_H-2)};
        case EDGE_TOP:    return (Point){1+rng_int(MAP_W-2), 0};
        case EDGE_BOTTOM: return (Point){1+rng_int(MAP_W-2), MAP_H-1};
    }
    return (Point){0,0};
}

static void walk_to(Map *map, int *cx, int *cy,
                    int tx, int ty,
                    int *ldx, int *ldy,
                    uint8_t visited[MAP_H][MAP_W],
                    int path_id)
{
    int max_steps = MAP_W * MAP_H * 3;
    typedef struct { int dx, dy; } Dir;

    while ((*cx != tx || *cy != ty) && max_steps-- > 0) {
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx] = 1;
            map->tiles[*cy][*cx].type      = TILE_PATH;
            map->tiles[*cy][*cx].passable  = 1;
            map->tiles[*cy][*cx].buildable = 0;
            if (map->tiles[*cy][*cx].path_id == -1)
                map->tiles[*cy][*cx].path_id = path_id;
        }

        int wdx = (*cx < tx) ? 1 : (*cx > tx) ? -1 : 0;
        int wdy = (*cy < ty) ? 1 : (*cy > ty) ? -1 : 0;

        Dir pool[30]; int pn = 0;
        if (*ldx || *ldy) { for (int i = 0; i < 5; i++) pool[pn++] = (Dir){*ldx, *ldy}; }
        if (wdx) for (int i = 0; i < 4; i++) pool[pn++] = (Dir){wdx, 0};
        if (wdy) for (int i = 0; i < 4; i++) pool[pn++] = (Dir){0, wdy};
        pool[pn++]=(Dir){0,1}; pool[pn++]=(Dir){0,-1};
        pool[pn++]=(Dir){1,0}; pool[pn++]=(Dir){-1,0};

        // Plus on est proche, plus on biaise fortement vers la cible
        int dist = abs(*cx-tx) + abs(*cy-ty);
        if (dist <= 4) {
            // Force la direction cible — ignore le momentum
            if (wdx) { for (int i = 0; i < 8; i++) pool[pn++] = (Dir){wdx, 0}; }
            if (wdy) { for (int i = 0; i < 8; i++) pool[pn++] = (Dir){0, wdy}; }
        }

        int moved = 0;
        for (int attempt = 0; attempt < 20; attempt++) {
            Dir d = pool[rng_int(pn)];
            int nx = *cx + d.dx, ny = *cy + d.dy;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            *ldx = d.dx; *ldy = d.dy;
            *cx = nx; *cy = ny;
            moved = 1;
            break;
        }
        if (!moved) break;
    }

    // ── Garantie absolue : si pas encore arrivé, trace en ligne droite ──
    // Avance d'abord horizontalement puis verticalement
    while (*cx != tx) {
        int dx = (*cx < tx) ? 1 : -1;
        *cx += dx;
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx] = 1;
            map->tiles[*cy][*cx].type      = TILE_PATH;
            map->tiles[*cy][*cx].passable  = 1;
            map->tiles[*cy][*cx].buildable = 0;
            if (map->tiles[*cy][*cx].path_id == -1)
                map->tiles[*cy][*cx].path_id = path_id;
        }
    }
    while (*cy != ty) {
        int dy = (*cy < ty) ? 1 : -1;
        *cy += dy;
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx] = 1;
            map->tiles[*cy][*cx].type      = TILE_PATH;
            map->tiles[*cy][*cx].passable  = 1;
            map->tiles[*cy][*cx].buildable = 0;
            if (map->tiles[*cy][*cx].path_id == -1)
                map->tiles[*cy][*cx].path_id = path_id;
        }
    }
}

// Génère un chemin unique avec son propre seed dérivé
// Retourne 1 si spawn/base sont assez distants, 0 sinon
static int carve_one_path(Map *map, int path_id, uint32_t path_seed,
                           int min_dist, Point base) {
    rng_init(path_seed);

    Edge spawn_edge;
    Point spawn;
    int attempts = 100;
    do {
        spawn_edge = (Edge)rng_int(4);
        spawn = edge_point(spawn_edge);
        attempts--;
    } while (manhattan(spawn, base) < min_dist && attempts > 0);

    if (attempts == 0) return 0;

    // Vérifie que ce spawn n'est pas trop proche d'un spawn existant
    for (int i = 0; i < path_id; i++) {
        if (map->paths[i].active &&
            manhattan(spawn, map->paths[i].spawn) < 6) return 0;
    }

    map->paths[path_id].spawn      = spawn;
    map->paths[path_id].base       = base;   // base partagée
    map->paths[path_id].spawn_edge = spawn_edge;
    map->paths[path_id].active     = 1;

    // Waypoints vers la base commune
    int num_wp = 3 + rng_int(3);
    Point wp[7];
    wp[0] = spawn;
    for (int i = 1; i <= num_wp; i++) {
        float t = (float)i / (float)(num_wp + 1);
        int bx = (int)((1.0f-t)*spawn.x + t*base.x);
        int by = (int)((1.0f-t)*spawn.y + t*base.y);
        int rx = (int)((rng_float()-0.5f)*MAP_W*0.5f);
        int ry = (int)((rng_float()-0.5f)*MAP_H*0.5f);
        wp[i].x = (int)fmaxf(1, fminf(MAP_W-2, bx+rx));
        wp[i].y = (int)fmaxf(1, fminf(MAP_H-2, by+ry));
    }
    wp[num_wp+1] = base;

    uint8_t visited[MAP_H][MAP_W];
    memset(visited, 0, sizeof(visited));
    int cx=spawn.x, cy=spawn.y, ldx=0, ldy=0;

    for (int seg = 0; seg < num_wp+1; seg++)
        walk_to(map,&cx,&cy,wp[seg+1].x,wp[seg+1].y,&ldx,&ldy,visited,path_id);

    /* Spawn par-dessus, mais PAS la base (gérée une seule fois dans generate_map)
    map->tiles[spawn.y][spawn.x].type    = TILE_SPAWN;
    map->tiles[spawn.y][spawn.x].path_id = path_id;
    */
    return 1;
}
// ════════════════════════════════════════════════════
// POINT D'ENTRÉE PUBLIC
// ════════════════════════════════════════════════════
void generate_map(Map *map, int seed, int min_dist, ThemeID theme_id) {
    memset(map, 0, sizeof(Map));
    map->seed  = seed;
    map->theme = (theme_id == THEME_COUNT) ? theme_random(seed) : theme_id;
    const Theme *th = theme_get(map->theme);

    build_perm(seed);
    gen_terrain(map, th);

    rng_init((uint32_t)(seed + 999));

    Edge  base_edge = (Edge)rng_int(4);
    Point base      = edge_point(base_edge);

    // NE PAS poser TILE_BASE ici — les chemins vont l'écraser

    int num_paths = 2 + rng_int(th->max_paths - 1);
    map->path_count = 0;
    for (int i = 0; i < num_paths && i < MAX_PATHS; i++) {
        uint32_t path_seed = (uint32_t)(seed + 777 + i * 1234567);
        if (carve_one_path(map, i, path_seed, min_dist, base))
            map->path_count++;
    }

    for (int i = 0; i < map->path_count; i++)
        map->paths[i].base = base;

    // ── Pose la base EN DERNIER, après tous les chemins ──────
    map->tiles[base.y][base.x].type      = TILE_BASE;
    map->tiles[base.y][base.x].passable  = 1;
    map->tiles[base.y][base.x].buildable = 0;
    map->tiles[base.y][base.x].path_id   = -1;

    // Idem pour chaque spawn — les re-poser après génération
    for (int i = 0; i < map->path_count; i++) {
        Point sp = map->paths[i].spawn;
        map->tiles[sp.y][sp.x].type      = TILE_SPAWN;
        map->tiles[sp.y][sp.x].passable  = 1;
        map->tiles[sp.y][sp.x].buildable = 0;
    }
}
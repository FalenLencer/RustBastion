/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "map_gen.h"
#include "../combat/material.h"
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
static float rng_float(void) {
    return (float)(rng_next() & 0x00ffffffu) / (float)0x01000000;
}
static int rng_int(int n) {
    return n > 0 ? (int)(rng_float() * (float)n) : 0;
}

// ════════════════════════════════════════════════════
// PERLIN 2D
// ════════════════════════════════════════════════════
static int perm[512];

static void build_perm(int seed) {
    rng_init((uint32_t)seed);
    for (int i = 0; i < 256; i++) perm[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = rng_int(i + 1);
        int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    for (int i = 0; i < 256; i++) perm[256+i] = perm[i];
}

static float fade(float t) { return t*t*t*(t*(t*6-15)+10); }
static float flp(float a, float b, float t) { return a + t*(b-a); }
static float grad(int h, float x, float y) {
    switch(h&3){
        case 0: return  x+y;
        case 1: return -x+y;
        case 2: return  x-y;
        default: return -x-y;
    }
}
static float perlin2d(float x, float y) {
    int xi = (int)floorf(x) & 255, yi = (int)floorf(y) & 255;
    float xf = x-floorf(x), yf = y-floorf(y);
    float u = fade(xf), v = fade(yf);
    int aa = perm[perm[xi]+yi],   ba = perm[perm[xi+1]+yi];
    int ab = perm[perm[xi]+yi+1], bb = perm[perm[xi+1]+yi+1];
    return flp(flp(grad(aa,xf,yf),  grad(ba,xf-1,yf),  u),
               flp(grad(ab,xf,yf-1),grad(bb,xf-1,yf-1),u), v);
}

// ════════════════════════════════════════════════════
// TERRAIN
// ════════════════════════════════════════════════════
static void gen_terrain(Map *map, const Theme *th) {
    for (int y = 0; y < map->h; y++) {
        for (int x = 0; x < map->w; x++) {
            float n  = perlin2d(x*0.18f, y*0.18f);
            n       += perlin2d(x*0.36f, y*0.36f) * 0.5f;
            n       += perlin2d(x*0.72f, y*0.72f) * 0.25f;
            n = (n/1.75f + 1.0f) * 0.5f;

            map->tiles[y][x].noise_val = n;
            map->tiles[y][x].path_id  = -1;

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
// UTILITAIRES
// ════════════════════════════════════════════════════
static int manhattan(Point a, Point b) {
    return abs(a.x-b.x) + abs(a.y-b.y);
}

static Edge opposite_edge(Edge e) {
    switch(e) {
        case EDGE_LEFT:   return EDGE_RIGHT;
        case EDGE_RIGHT:  return EDGE_LEFT;
        case EDGE_TOP:    return EDGE_BOTTOM;
        case EDGE_BOTTOM: return EDGE_TOP;
    }
    return EDGE_RIGHT;
}

// Point sur un bord, en évitant l'eau
static Point point_on_edge(Edge edge, const Map *map) {
    int tries = 60;
    while (tries-- > 0) {
        Point p;
        switch (edge) {
            case EDGE_LEFT:   p = (Point){0,         1+rng_int(map->h-2)}; break;
            case EDGE_RIGHT:  p = (Point){map->w-1,  1+rng_int(map->h-2)}; break;
            case EDGE_TOP:    p = (Point){1+rng_int(map->w-2), 0};          break;
            case EDGE_BOTTOM: p = (Point){1+rng_int(map->w-2), map->h-1};   break;
            default:          p = (Point){0, map->h/2};
        }
        if (map->tiles[p.y][p.x].type != TILE_WATER)
            return p;
    }
    // Fallback milieu du bord
    switch (edge) {
        case EDGE_LEFT:   return (Point){0,        map->h/2};
        case EDGE_RIGHT:  return (Point){map->w-1, map->h/2};
        case EDGE_TOP:    return (Point){map->w/2, 0};
        case EDGE_BOTTOM: return (Point){map->w/2, map->h-1};
    }
    return (Point){0, map->h/2};
}

// Force une tuile et ses voisins à être passables
static void force_passable(Map *map, Point p) {
    if (p.x < 0 || p.x >= map->w || p.y < 0 || p.y >= map->h) return;
    if (map->tiles[p.y][p.x].type == TILE_WATER ||
        map->tiles[p.y][p.x].type == TILE_RUIN) {
        map->tiles[p.y][p.x].type     = TILE_GROUND;
        map->tiles[p.y][p.x].passable = 1;
    }
    const int dx4[4] = { 1,-1, 0, 0};
    const int dy4[4] = { 0, 0, 1,-1};
    for (int d = 0; d < 4; d++) {
        int nx = p.x+dx4[d], ny = p.y+dy4[d];
        if (nx < 0 || nx >= map->w || ny < 0 || ny >= map->h) continue;
        if (map->tiles[ny][nx].type == TILE_WATER) {
            map->tiles[ny][nx].type      = TILE_GROUND;
            map->tiles[ny][nx].passable  = 1;
            map->tiles[ny][nx].buildable = 1;
        }
    }
}

// ════════════════════════════════════════════════════
// CHEMIN ORGANIQUE — spawn → base cible
// ════════════════════════════════════════════════════
static void walk_to(Map *map, int *cx, int *cy,
                    int tx, int ty, int *ldx, int *ldy,
                    uint8_t visited[MAX_MAP_H][MAX_MAP_W], int path_id)
{
    int max_steps = map->w * map->h * 3;
    typedef struct { int dx, dy; } Dir;

    while ((*cx != tx || *cy != ty) && max_steps-- > 0) {
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx] = 1;
            if (map->tiles[*cy][*cx].type != TILE_BASE &&
                map->tiles[*cy][*cx].type != TILE_SPAWN) {
                map->tiles[*cy][*cx].type      = TILE_PATH;
                map->tiles[*cy][*cx].passable  = 1;
                map->tiles[*cy][*cx].buildable = 0;
            }
            if (map->tiles[*cy][*cx].path_id == -1)
                map->tiles[*cy][*cx].path_id = path_id;
        }

        int wdx = (*cx < tx) ? 1 : (*cx > tx) ? -1 : 0;
        int wdy = (*cy < ty) ? 1 : (*cy > ty) ? -1 : 0;

        Dir pool[30]; int pn = 0;
        if (*ldx || *ldy) for (int i=0;i<5;i++) pool[pn++]=(Dir){*ldx,*ldy};
        if (wdx) for (int i=0;i<4;i++) pool[pn++]=(Dir){wdx,0};
        if (wdy) for (int i=0;i<4;i++) pool[pn++]=(Dir){0,wdy};
        pool[pn++]=(Dir){0,1}; pool[pn++]=(Dir){0,-1};
        pool[pn++]=(Dir){1,0}; pool[pn++]=(Dir){-1,0};

        if (abs(*cx-tx)+abs(*cy-ty) <= 4) {
            if (wdx) for (int i=0;i<8;i++) pool[pn++]=(Dir){wdx,0};
            if (wdy) for (int i=0;i<8;i++) pool[pn++]=(Dir){0,wdy};
        }

        int moved = 0;
        for (int attempt = 0; attempt < 20; attempt++) {
            Dir d = pool[rng_int(pn)];
            int nx = *cx+d.dx, ny = *cy+d.dy;
            if (nx<0||nx>=map->w||ny<0||ny>=map->h) continue;
            *ldx=d.dx; *ldy=d.dy; *cx=nx; *cy=ny; moved=1; break;
        }
        if (!moved) break;
    }

    while (*cx != tx) {
        *cx += (*cx < tx) ? 1 : -1;
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx]=1;
            if (map->tiles[*cy][*cx].type!=TILE_BASE &&
                map->tiles[*cy][*cx].type!=TILE_SPAWN) {
                map->tiles[*cy][*cx].type=TILE_PATH;
                map->tiles[*cy][*cx].passable=1;
                map->tiles[*cy][*cx].buildable=0;
            }
            if (map->tiles[*cy][*cx].path_id==-1)
                map->tiles[*cy][*cx].path_id=path_id;
        }
    }
    while (*cy != ty) {
        *cy += (*cy < ty) ? 1 : -1;
        if (!visited[*cy][*cx]) {
            visited[*cy][*cx]=1;
            if (map->tiles[*cy][*cx].type!=TILE_BASE &&
                map->tiles[*cy][*cx].type!=TILE_SPAWN) {
                map->tiles[*cy][*cx].type=TILE_PATH;
                map->tiles[*cy][*cx].passable=1;
                map->tiles[*cy][*cx].buildable=0;
            }
            if (map->tiles[*cy][*cx].path_id==-1)
                map->tiles[*cy][*cx].path_id=path_id;
        }
    }
}

// Génère un chemin d'un spawn vers une base cible
// spawn_edge_hint : bord suggéré pour le spawn (bord opp. aux bases)
static int carve_path(Map *map, int path_id, int base_id,
                      uint32_t pseed, int min_dist,
                      Edge spawn_edge_hint)
{
    Point base = map->bases[base_id].pos;
    rng_init(pseed);

    Point spawn;
    int   ok = 0, tries = 80;
    while (tries-- > 0) {
        // Bord opposé 70%, bord perpendiculaire 30%
        Edge se;
        if (rng_int(10) < 7) {
            se = opposite_edge(spawn_edge_hint);
        } else {
            Edge perp[2];
            if (spawn_edge_hint == EDGE_LEFT || spawn_edge_hint == EDGE_RIGHT)
                { perp[0]=EDGE_TOP;  perp[1]=EDGE_BOTTOM; }
            else
                { perp[0]=EDGE_LEFT; perp[1]=EDGE_RIGHT;  }
            se = perp[rng_int(2)];
        }
        spawn = point_on_edge(se, map);

        if (manhattan(spawn, base) < min_dist) continue;

        // Pas trop proche d'un spawn existant
        int conflict = 0;
        for (int i=0; i<path_id; i++) {
            if (map->paths[i].active &&
                manhattan(spawn, map->paths[i].spawn) < 5) {
                conflict=1; break;
            }
        }
        if (conflict) continue;

        // Pas trop proche d'une base
        int near = 0;
        for (int b=0; b<map->base_count; b++) {
            if (manhattan(spawn, map->bases[b].pos) < 3) { near=1; break; }
        }
        if (near) continue;

        ok = 1;
        break;
    }
    if (!ok) return 0;

    map->paths[path_id].spawn      = spawn;
    map->paths[path_id].base       = base;
    map->paths[path_id].spawn_edge = opposite_edge(spawn_edge_hint);
    map->paths[path_id].base_id    = base_id;
    map->paths[path_id].active     = 1;

    // Waypoints organiques spawn → base
    int num_wp = 2 + rng_int(3);
    Point wp[8];
    wp[0] = spawn;
    for (int i=1; i<=num_wp; i++) {
        float t = (float)i / (float)(num_wp+1);
        int bx = (int)((1.0f-t)*spawn.x + t*base.x);
        int by = (int)((1.0f-t)*spawn.y + t*base.y);
        int rx = (int)((rng_float()-0.5f)*map->w*0.4f);
        int ry = (int)((rng_float()-0.5f)*map->h*0.4f);
        wp[i].x = (int)fmaxf(1, fminf(map->w-2, bx+rx));
        wp[i].y = (int)fmaxf(1, fminf(map->h-2, by+ry));
    }
    wp[num_wp+1] = base;

    uint8_t visited[MAX_MAP_H][MAX_MAP_W];
    memset(visited, 0, sizeof(visited));
    int cx=spawn.x, cy=spawn.y, ldx=0, ldy=0;
    for (int seg=0; seg<num_wp+1; seg++)
        walk_to(map,&cx,&cy,wp[seg+1].x,wp[seg+1].y,&ldx,&ldy,visited,path_id);

    return 1;
}

// ════════════════════════════════════════════════════
// PLACEMENT D'UNE BASE
// Toutes sur le même bord, groupées près de l'ancre
// ════════════════════════════════════════════════════
static int place_base(Map *map, int base_id, Edge base_edge,
                      Point anchor, int max_spread)
{
    int tries = 60;
    while (tries-- > 0) {
        Point pos;
        if (base_id == 0) {
            pos = point_on_edge(base_edge, map);
        } else {
            int offset = rng_int(2*max_spread+1) - max_spread;
            switch (base_edge) {
                case EDGE_LEFT:   pos=(Point){0,          anchor.y+offset}; break;
                case EDGE_RIGHT:  pos=(Point){map->w-1,   anchor.y+offset}; break;
                case EDGE_TOP:    pos=(Point){anchor.x+offset, 0};           break;
                case EDGE_BOTTOM: pos=(Point){anchor.x+offset, map->h-1};    break;
                default:          pos=anchor;
            }
            if (pos.x<0)          pos.x=0;
            if (pos.x>=map->w)    pos.x=map->w-1;
            if (pos.y<1)          pos.y=1;
            if (pos.y>=map->h-1)  pos.y=map->h-2;
        }

        if (map->tiles[pos.y][pos.x].type == TILE_WATER) continue;

        // Distance min entre bases = 4 tuiles
        int conflict=0;
        for (int i=0; i<base_id; i++) {
            if (!map->bases[i].active) continue;
            if (manhattan(pos, map->bases[i].pos) < 4) { conflict=1; break; }
        }
        if (conflict) continue;

        force_passable(map, pos);

        map->bases[base_id].pos        = pos;
        map->bases[base_id].is_primary = (base_id == 0);
        map->bases[base_id].active     = 1;
        map->bases[base_id].max_hp     = (base_id == 0) ? 20 : 10;
        map->bases[base_id].hp         = map->bases[base_id].max_hp;
        map->bases[base_id].damage     = (base_id == 0) ? 2 : 1;

        // Marque la tuile BASE immédiatement pour que walk_to ne la crase pas
        map->tiles[pos.y][pos.x].type      = TILE_BASE;
        map->tiles[pos.y][pos.x].passable  = 1;
        map->tiles[pos.y][pos.x].buildable = 0;
        return 1;
    }
    return 0;
}

// ════════════════════════════════════════════════════
// ASSIGNATION SPAWN → BASE
//
// total_paths = max(num_spawns, num_bases) — garantit que chaque base
// reçoit au moins un chemin d'attaque.
// Les chemins supplémentaires (si total_paths > num_bases) sont assignés
// en round-robin, puis mélangés pour l'incertitude tactique.
// ════════════════════════════════════════════════════
static void assign_spawns_to_bases(int total_paths, int num_bases,
                                   int out_base_ids[MAX_PATHS])
{
    // Round-robin : base i % num_bases → chaque base couverte au moins une fois
    for (int i = 0; i < total_paths; i++)
        out_base_ids[i] = i % num_bases;

    // Mélange Fisher-Yates — incertitude tactique
    for (int i = total_paths - 1; i > 0; i--) {
        int j = rng_int(i + 1);
        int tmp = out_base_ids[i];
        out_base_ids[i] = out_base_ids[j];
        out_base_ids[j] = tmp;
    }
}

// ════════════════════════════════════════════════════
// POINT D'ENTRÉE PUBLIC
// ════════════════════════════════════════════════════
void generate_map(Map *map, int seed, int min_dist, ThemeID theme_id,
                  int forced_bases, int forced_spawns,
                  int forced_deposits, int map_w, int map_h) {
    memset(map, 0, sizeof(Map));
    map->w     = (map_w > 0 && map_w <= MAX_MAP_W) ? map_w : MAP_W;
    map->h     = (map_h > 0 && map_h <= MAX_MAP_H) ? map_h : MAP_H;
    map->seed  = seed;
    map->theme = (theme_id == THEME_COUNT) ? theme_random(seed) : theme_id;
    const Theme *th = theme_get(map->theme);

    build_perm(seed);
    gen_terrain(map, th);
    rng_init((uint32_t)(seed + 999));

    // ── Bord des bases ────────────────────────────────────────
    Edge base_edge = (Edge)rng_int(4);

    // ── Nombre de bases (1 à MAX_BASES) ──────────────────────
    // forced_bases > 0 : imposé par le scénario / mode custom.
    // sinon (arcade) : 1-3 bases (50 % → 1, 33 % → 2, 17 % → 3)
    int num_bases;
    if (forced_bases > 0) {
        num_bases = forced_bases;
        if (num_bases > MAX_BASES) num_bases = MAX_BASES;
    } else {
        int rb = rng_int(6);
        num_bases = (rb < 3) ? 1 : (rb < 5) ? 2 : 3;
    }
    map->base_count = 0;

    // Base principale
    {
        int ok=0, atts=80;
        while (!ok && atts-- > 0) {
            rng_init((uint32_t)(seed + 999 + atts));
            ok = place_base(map, 0, base_edge, (Point){0,0}, 0);
        }
        if (!ok) {
            // Fallback absolu
            Point fb;
            switch(base_edge) {
                case EDGE_LEFT:   fb=(Point){0,         map->h/2}; break;
                case EDGE_RIGHT:  fb=(Point){map->w-1,  map->h/2}; break;
                case EDGE_TOP:    fb=(Point){map->w/2,  0};        break;
                case EDGE_BOTTOM: fb=(Point){map->w/2,  map->h-1}; break;
                default:          fb=(Point){0,         map->h/2};
            }
            force_passable(map, fb);
            map->bases[0]=(BaseInfo){fb,20,20,1,1,2,0};
        }
        map->base_count = 1;
    }

    Point anchor = map->bases[0].pos;

    /* ── Bases supplémentaires (1 .. num_bases-1) ────────────────
       On divise le bord en num_bases segments et on essaie de
       placer une base au centre de chaque segment.
       next_slot n'avance que si le placement réussit, ce qui
       garantit un tableau sans trous (bases 0..base_count-1 actives). */
    if (num_bases > 1) {
        int edge_len = (base_edge == EDGE_LEFT || base_edge == EDGE_RIGHT)
                       ? map->h : map->w;
        int seg = edge_len / num_bases;
        if (seg < 1) seg = 1;

        int spread = seg / 2 + 2;
        if (spread > 12) spread = 12;
        if (spread <  2) spread = 2;

        int next_slot = 1;   /* prochain slot disponible dans map->bases[] */

        for (int seg_b = 1; seg_b < num_bases && next_slot < MAX_BASES; seg_b++) {
            rng_init((uint32_t)(seed + 1337u + (uint32_t)seg_b * 997u));

            /* Centre du seg_b-ième segment */
            int tgt = seg_b * seg + seg / 2;
            if (tgt >= edge_len) tgt = edge_len - 1;

            Point tgt_pt;
            switch (base_edge) {
                case EDGE_LEFT:   tgt_pt = (Point){0,         tgt}; break;
                case EDGE_RIGHT:  tgt_pt = (Point){map->w-1,  tgt}; break;
                case EDGE_TOP:    tgt_pt = (Point){tgt,       0};   break;
                case EDGE_BOTTOM: tgt_pt = (Point){tgt, map->h-1};  break;
                default:          tgt_pt = anchor;                   break;
            }

            int ok = 0, atts = 100;
            while (!ok && atts-- > 0)
                ok = place_base(map, next_slot, base_edge, tgt_pt, spread);
            if (ok) {
                map->base_count = next_slot + 1;
                next_slot++;
            }
            /* Si le segment est trop dense, on tente quand même les
               segments suivants (next_slot reste inchangé). */
        }
    }

    // ── Nombre de spawns indépendant du nombre de bases ──────
    // forced_spawns > 0 : imposé (mode custom).
    // En arcade (forced_spawns = 0) : tirage libre entre 1 et 3,
    //   indépendamment du nombre de bases. Si num_spawns < base_count,
    //   les bases non couvertes ne seront simplement pas attaquées
    //   (comportement valide : elles servent de "vies bonus").
    rng_init((uint32_t)(seed + 4242));
    int num_spawns;
    if (forced_spawns > 0) {
        num_spawns = forced_spawns;
    } else {
        num_spawns = 1 + rng_int(3);   /* 1, 2 ou 3 — indépendant des bases */
    }
    if (num_spawns < 1)         num_spawns = 1;
    if (num_spawns > MAX_PATHS) num_spawns = MAX_PATHS;

    // Garantit un chemin vers chaque base : total_paths ≥ num_bases
    int total_paths = num_spawns > map->base_count ? num_spawns : map->base_count;
    if (total_paths > MAX_PATHS) total_paths = MAX_PATHS;

    // ── Assignation spawn → base ──────────────────────────────
    int base_ids[MAX_PATHS];
    assign_spawns_to_bases(total_paths, map->base_count, base_ids);

    // ── Génération des chemins ────────────────────────────────
    map->path_count = 0;
    for (int i = 0; i < total_paths; i++) {
        int bid = base_ids[i];
        int carved = 0;

        // Plusieurs tentatives avec seeds variés
        for (int attempt = 0; attempt < 8 && !carved; attempt++) {
            uint32_t pseed = (uint32_t)(seed + 777 + i*1234567 + attempt*54321);
            if (carve_path(map, map->path_count, bid,
                           pseed, min_dist, base_edge)) {
                map->path_count++;
                carved = 1;
            }
        }
        // Relâche min_dist si impossible
        if (!carved) {
            for (int attempt = 0; attempt < 4 && !carved; attempt++) {
                uint32_t pseed = (uint32_t)(seed+999+i*777+attempt*11111);
                if (carve_path(map, map->path_count, bid,
                               pseed, 4, base_edge)) {
                    map->path_count++;
                    carved = 1;
                }
            }
        }
    }

    // ── Pose les bases EN DERNIER ─────────────────────────────
    for (int b = 0; b < map->base_count; b++) {
        if (!map->bases[b].active) continue;
        Point bp = map->bases[b].pos;
        force_passable(map, bp);
        map->tiles[bp.y][bp.x].type      = TILE_BASE;
        map->tiles[bp.y][bp.x].passable  = 1;
        map->tiles[bp.y][bp.x].buildable = 0;
        map->tiles[bp.y][bp.x].path_id   = -1;
    }

    // ── Pose les spawns ───────────────────────────────────────
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp = map->paths[i].spawn;
        force_passable(map, sp);
        map->tiles[sp.y][sp.x].type      = TILE_SPAWN;
        map->tiles[sp.y][sp.x].passable  = 1;
        map->tiles[sp.y][sp.x].buildable = 0;
    }

    // ── Zone d'exclusion autour des spawns : non constructible ─
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp = map->paths[i].spawn;
        for (int ey = -SPAWN_EXCLUSION_RADIUS; ey <= SPAWN_EXCLUSION_RADIUS; ey++) {
            for (int ex = -SPAWN_EXCLUSION_RADIUS; ex <= SPAWN_EXCLUSION_RADIUS; ex++) {
                if (abs(ex) + abs(ey) > SPAWN_EXCLUSION_RADIUS) continue;
                int nx = sp.x + ex, ny = sp.y + ey;
                if (nx < 0 || nx >= map->w || ny < 0 || ny >= map->h) continue;
                TileType tt = map->tiles[ny][nx].type;
                if (tt == TILE_BASE || tt == TILE_PATH || tt == TILE_SPAWN) continue;
                map->tiles[ny][nx].buildable = 0;
            }
        }
    }

    // ── Dépôts de matériaux ───────────────────────────────────
    {
        rng_init((uint32_t)(seed + 8888));
        int dep_count;
        if (forced_deposits > 0) {
            dep_count = forced_deposits;
            if (dep_count > MAX_MATERIAL_DEPOSITS) dep_count = MAX_MATERIAL_DEPOSITS;
        } else {
            dep_count = 1 + rng_int(4);   // 1..4 aléatoire
            if (dep_count > MAX_MATERIAL_DEPOSITS) dep_count = MAX_MATERIAL_DEPOSITS;
        }
        int placed=0, tries=0;
        map->deposit_count = 0;

        while (placed < dep_count && tries < 500) {
            tries++;
            int dx = 1 + rng_int(map->w-2);
            int dy = 1 + rng_int(map->h-2);

            if (map->tiles[dy][dx].type != TILE_GROUND &&
                map->tiles[dy][dx].type != TILE_RUIN) continue;
            if (!map->tiles[dy][dx].buildable) continue;

            int near=0;
            for (int b=0; b<map->base_count; b++) {
                if (manhattan((Point){dx,dy}, map->bases[b].pos) < 3) {
                    near=1; break;
                }
            }
            if (near) continue;

            int dup=0;
            for (int k=0; k<placed; k++) {
                if (map->deposits[k].tile_x==dx &&
                    map->deposits[k].tile_y==dy) { dup=1; break; }
            }
            if (dup) continue;

            // Distance manhattan à la base la plus proche
            int min_dist = 9999;
            for (int b2 = 0; b2 < map->base_count; b2++) {
                int md = manhattan((Point){dx,dy}, map->bases[b2].pos);
                if (md < min_dist) min_dist = md;
            }
            // spawn_wave : 0 = immédiat, 4 = vague 4, 8 = vague 8
            int sw = 0;
            if (min_dist >= 12) sw = 8;
            else if (min_dist >= 7) sw = 4;

            map->deposits[placed].tile_x    = dx;
            map->deposits[placed].tile_y    = dy;
            map->deposits[placed].type      = (MaterialType)rng_int(MAT_COUNT);
            map->deposits[placed].spawn_wave = sw;
            map->deposits[placed].active    = (sw == 0) ? 1 : 0;
            map->tiles[dy][dx].buildable = 0;   // pas de tour sur un dépôt
            placed++;
        }
        map->deposit_count = placed;
    }
}
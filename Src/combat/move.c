/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  move.c ─ collision-déplacement commune des entités (voir move.h).
 *  Même principe éprouvé que la collision du héros : test de la tuile
 *  d'arrivée du bord avant, axe par axe (glissement), avec la règle
 *  anti-blocage « on peut toujours SORTIR d'une tuile bloquée ».
 */
#include "move.h"
#include <math.h>
#include <string.h>

#define MOVE_EPS 1e-3f   /* seuil « la position a changé » (px) */

int move_tile_blocked(const Map *map, const TowerPool *tp,
                      int tx, int ty, int flags) {
    if (tx < 0 || ty < 0 || tx >= map->w || ty >= map->h) return 1;
    const Tile *t = &map->tiles[ty][tx];

    if (t->type == TILE_WATER) return 1;
    if (t->type == TILE_RUIN)  return 1;
    if (t->type == TILE_BASE && (flags & MOVE_F_ALLY)) return 1;
    if (!t->passable && t->type != TILE_BASE) return 1;   /* filet générique */

    if (tp) {
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *tw = &tp->towers[i];
            if (tw->active && tw->tile_x == tx && tw->tile_y == ty) return 1;
        }
    }
    return 0;
}

int move_slide(const Map *map, const TowerPool *tp,
               float *px, float *py, float dx, float dy,
               float radius, int flags) {
    float ox = *px, oy = *py;

    /* Anti-blocage : si la tuile COURANTE est déjà bloquée (spawn raté,
       tour posée sous l'entité…), tout mouvement de sortie est permis. */
    int stuck = move_tile_blocked(map, tp, (int)(*px / TILE_SIZE),
                                  (int)(*py / TILE_SIZE), flags);

    float nx  = *px + dx;
    int   etx = (int)((nx + (dx > 0.0f ? radius : -radius)) / TILE_SIZE);
    int   ety = (int)(*py / TILE_SIZE);
    if (stuck || !move_tile_blocked(map, tp, etx, ety, flags)) *px = nx;

    float ny  = *py + dy;
    int   ftx = (int)(*px / TILE_SIZE);
    int   fty = (int)((ny + (dy > 0.0f ? radius : -radius)) / TILE_SIZE);
    if (stuck || !move_tile_blocked(map, tp, ftx, fty, flags)) *py = ny;

    /* Bornes dures de la carte. */
    float maxx = map->w * (float)TILE_SIZE - radius;
    float maxy = map->h * (float)TILE_SIZE - radius;
    if (*px < radius) *px = radius;
    if (*py < radius) *py = radius;
    if (*px > maxx)   *px = maxx;
    if (*py > maxy)   *py = maxy;

    return (fabsf(*px - ox) > MOVE_EPS || fabsf(*py - oy) > MOVE_EPS);
}

int move_toward(const Map *map, const TowerPool *tp,
                float *px, float *py, float tx, float ty,
                float step, float radius, int flags) {
    float dx = tx - *px, dy = ty - *py;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < MOVE_EPS || step <= 0.0f) return 0;
    dx /= dist; dy /= dist;

    /* Direction directe, puis déviations vers le but (±45°, ±90°) :
       contournement LOCAL des obstacles sans pathfinding complet. */
    static const float DEV[5] = {
        0.0f, MOVE_DEVIATE_1, -MOVE_DEVIATE_1, MOVE_DEVIATE_2, -MOVE_DEVIATE_2
    };
    for (int k = 0; k < 5; k++) {
        float c = cosf(DEV[k]), s = sinf(DEV[k]);
        float rx = (dx * c - dy * s) * step;
        float ry = (dx * s + dy * c) * step;
        if (move_slide(map, tp, px, py, rx, ry, radius, flags)) return 1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════
   GRILLE DE NAVIGATION PARTAGÉE
   ────────────────────────────────────────────────────────────────
   La ligne de vue et le BFS interrogent des milliers de tuiles par
   frame ; passer par move_tile_blocked à chaque test rescannerait les
   MAX_TOWERS tours (des centaines de milliers d'opérations). On
   construit donc UNE grille O(1) par frame, partagée par TOUTES les
   entités : la première qui se déplace la paie, les autres la lisent.

   SÛRETÉ : cette grille ne sert qu'à CHOISIR une direction. Le
   déplacement réel reste validé par move_slide → move_tile_blocked,
   qui fait autorité — une grille d'une frame de retard ne peut donc
   jamais faire traverser un mur.
   ════════════════════════════════════════════════════════════════ */
static unsigned char g_nav_blk[MAX_MAP_H][MAX_MAP_W];
static int g_nav_ready = 0, g_nav_flags = -1, g_nav_w = 0, g_nav_h = 0;

void move_nav_begin_frame(void) { g_nav_ready = 0; }

static void nav_grid_sync(const Map *map, const TowerPool *tp, int flags) {
    if (g_nav_ready && flags == g_nav_flags &&
        map->w == g_nav_w && map->h == g_nav_h) return;

    g_nav_w = map->w; g_nav_h = map->h; g_nav_flags = flags;
    for (int y = 0; y < g_nav_h; y++) {
        for (int x = 0; x < g_nav_w; x++)
            g_nav_blk[y][x] =
                (unsigned char)move_tile_blocked(map, NULL, x, y, flags);
    }
    if (tp) {
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *tw = &tp->towers[i];
            if (!tw->active) continue;
            if (tw->tile_x < 0 || tw->tile_x >= g_nav_w) continue;
            if (tw->tile_y < 0 || tw->tile_y >= g_nav_h) continue;
            g_nav_blk[tw->tile_y][tw->tile_x] = 1;
        }
    }
    g_nav_ready = 1;
}

/* Tuile d'une coordonnée monde. floorf (et non un cast) : un cast
   tronque vers zéro, donc -5 donnerait la tuile 0 au lieu de -1. */
static int tile_of(float v) { return (int)floorf(v / (float)TILE_SIZE); }

static int nav_blocked(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= g_nav_w || ty >= g_nav_h) return 1;
    return g_nav_blk[ty][tx];
}

int move_line_clear(const Map *map, const TowerPool *tp,
                    float x0, float y0, float x1, float y1,
                    float radius, int flags) {
    nav_grid_sync(map, tp, flags);

    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < MOVE_EPS) return 1;
    float ux = dx / len, uy = dy / len;
    /* Perpendiculaire : on teste la LARGEUR de l'entité, pas seulement
       son centre — sinon elle « voit » un passage où elle ne tient pas
       et vient se coincer dans le goulot. */
    float rx = -uy * radius, ry = ux * radius;

    float stepd = (float)TILE_SIZE * MOVE_NAV_LOS_STEP;
    int   n     = (int)(len / stepd) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i * stepd;
        if (t > len) t = len;
        float cx = x0 + ux * t, cy = y0 + uy * t;
        if (nav_blocked(tile_of(cx + rx), tile_of(cy + ry))) return 0;
        if (nav_blocked(tile_of(cx - rx), tile_of(cy - ry))) return 0;
    }
    return 1;
}

/* ── BFS tuile-à-tuile → chemin COMPLET (départ → but) ────────────
   Le but lui-même bloqué (bâtiment de base pour un allié…) est atteint
   par ADJACENCE. Retourne la longueur du chemin (0 = aucun chemin).
   Lit la grille partagée ; tableaux statiques (une seule sim). */
static short g_path_x[MAX_MAP_W * MAX_MAP_H];
static short g_path_y[MAX_MAP_W * MAX_MAP_H];

static int move_bfs_path(int sx, int sy, int gx, int gy) {
    enum { NMAX = MAX_MAP_W * MAX_MAP_H };
    static unsigned short vis[NMAX];
    static unsigned short gen = 0;
    static short qx[NMAX], qy[NMAX];
    static int   par[NMAX];

    int w = g_nav_w, hgt = g_nav_h;
    if (sx < 0 || sy < 0 || sx >= w || sy >= hgt) return 0;
    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    if (gx >= w)   gx = w - 1;
    if (gy >= hgt) gy = hgt - 1;
    if (sx == gx && sy == gy) return 0;

    gen++;
    if (gen == 0) { memset(vis, 0, sizeof(vis)); gen = 1; }

    int goal_blocked = g_nav_blk[gy][gx];
    int head = 0, tail = 0;
    qx[0] = (short)sx; qy[0] = (short)sy; par[0] = -1;
    vis[sy * w + sx] = gen;
    tail = 1;

    static const int DX[4] = {1, -1, 0, 0};
    static const int DY[4] = {0, 0, 1, -1};
    int found = -1;

    while (head < tail && found < 0) {
        int cx = qx[head], cy = qy[head];
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= hgt) continue;
            if (vis[ny * w + nx] == gen) continue;
            int at_goal = (nx == gx && ny == gy);
            if (g_nav_blk[ny][nx]) {
                if (at_goal && goal_blocked) { found = head; break; }
                continue;
            }
            vis[ny * w + nx] = gen;
            qx[tail] = (short)nx; qy[tail] = (short)ny;
            par[tail] = head;
            if (at_goal) found = tail;
            tail++;
            if (tail >= NMAX) break;
            if (found >= 0) break;
        }
        head++;
    }
    if (found < 0) return 0;

    /* Remonte la chaîne des parents, puis inverse : départ → but. */
    int n = 0;
    for (int i = found; i >= 0 && n < NMAX; i = par[i]) {
        g_path_x[n] = qx[i]; g_path_y[n] = qy[i]; n++;
        if (par[i] < 0) break;
    }
    for (int a = 0, b = n - 1; a < b; a++, b--) {
        short sw = g_path_x[a]; g_path_x[a] = g_path_x[b]; g_path_x[b] = sw;
        sw = g_path_y[a];       g_path_y[a] = g_path_y[b]; g_path_y[b] = sw;
    }
    return n;
}

int move_nav_toward(const Map *map, const TowerPool *tp, MoveNav *nav,
                    float *px, float *py, float tx, float ty,
                    float step, float radius, int flags) {
    nav_grid_sync(map, tp, flags);

    /* But déplacé de plus d'une tuile → le chemin courant est caduc. */
    float gdx = tx - nav->gx, gdy = ty - nav->gy;
    if (gdx * gdx + gdy * gdy > (float)(TILE_SIZE * TILE_SIZE)) {
        nav->gx = tx; nav->gy = ty;
        nav->active = 0;
        nav->stuck  = 0;
    }

    /* ── 1. LIGNE DÉGAGÉE → tout droit, aucun calcul de chemin ────
       C'est le cas de très loin le plus fréquent : terrain ouvert. */
    if (move_line_clear(map, tp, *px, *py, tx, ty, radius, flags)) {
        nav->active = 0;
        nav->stuck  = 0;
        return move_toward(map, tp, px, py, tx, ty, step, radius, flags);
    }

    /* ── 2. Waypoint courant : atteint, ou devenu invalide ? ────── */
    if (nav->active) {
        float wdx = nav->wx - *px, wdy = nav->wy - *py;
        float arrive = (float)TILE_SIZE * MOVE_NAV_ARRIVE_FRAC;
        if (wdx * wdx + wdy * wdy < arrive * arrive)
            nav->active = 0;                    /* atteint → suite du chemin */
        else if (!move_line_clear(map, tp, *px, *py, nav->wx, nav->wy,
                                  radius, flags))
            nav->active = 0;                    /* une tour a coupé la route */
    }

    /* ── 3. (Re)planification : BFS + TIR À LA CORDE ──────────────
       On vise le point le PLUS LOIN du chemin encore atteignable en
       ligne droite : trajectoire longue et naturelle, au lieu d'un
       cheminement case par case qui « colle » à la grille. */
    if (!nav->active) {
        int n = move_bfs_path(tile_of(*px), tile_of(*py),
                              tile_of(tx),  tile_of(ty));
        if (n > 1) {
            int hi = n - 1;
            if (hi > MOVE_NAV_PULL_MAX) hi = MOVE_NAV_PULL_MAX;
            int best = 1;
            for (int i = hi; i >= 1; i--) {
                float cxp = ((float)g_path_x[i] + 0.5f) * (float)TILE_SIZE;
                float cyp = ((float)g_path_y[i] + 0.5f) * (float)TILE_SIZE;
                if (move_line_clear(map, tp, *px, *py, cxp, cyp,
                                    radius, flags)) { best = i; break; }
            }
            nav->wx = ((float)g_path_x[best] + 0.5f) * (float)TILE_SIZE;
            nav->wy = ((float)g_path_y[best] + 0.5f) * (float)TILE_SIZE;
            nav->active = 1;
            nav->stuck  = 0;
        }
    }

    /* ── 4. Avance. On NE retombe PAS en « tout droit vers le but »
       tant qu'un waypoint est actif : c'était la cause exacte du
       va-et-vient (un pas de BFS, puis retour dans l'obstacle). */
    int moved = nav->active
        ? move_toward(map, tp, px, py, nav->wx, nav->wy, step, radius, flags)
        : move_toward(map, tp, px, py, tx, ty, step, radius, flags);

    /* Filet de sécurité : plus aucun mouvement pendant un moment (but
       muré, entité prise entre deux tours) → on force une nouvelle
       planification au lieu de rester figé. */
    if (moved) {
        nav->stuck = 0;
    } else if (++nav->stuck >= MOVE_NAV_STUCK_FRAMES) {
        nav->stuck  = 0;
        nav->active = 0;
    }
    return moved;
}

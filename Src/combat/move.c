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

int move_free_spot(const Map *map, const TowerPool *tp,
                   float tx, float ty, float radius, int flags,
                   float *out_x, float *out_y) {
    /* floorf : (int) tronque vers zéro, donc une cible à coordonnée
       NÉGATIVE (orbite d'une base collée au bord) retomberait sur la
       tuile 0 et serait acceptée telle quelle, HORS carte. */
    int cx = (int)floorf(tx / TILE_SIZE), cy = (int)floorf(ty / TILE_SIZE);
    if (move_can_stand(map, tp, tx, ty, radius, flags)) {
        /* Cible déjà valide : on la garde telle quelle (pas de recentrage
           sur la tuile, sinon une formation se tasserait sur les centres). */
        *out_x = tx; *out_y = ty;
        return 1;
    }
    float best_d2 = 0.0f; int found = 0;
    for (int r = 1; r <= MOVE_FREE_MAX_RING && !found; r++) {
        for (int oy = -r; oy <= r; oy++) {
            for (int ox = -r; ox <= r; ox++) {
                if (ox > -r && ox < r && oy > -r && oy < r) continue;  /* anneau */
                int nx = cx + ox, ny = cy + oy;
                if (move_tile_blocked(map, tp, nx, ny, flags)) continue;
                float px2 = (nx + 0.5f) * TILE_SIZE, py2 = (ny + 0.5f) * TILE_SIZE;
                float dx2 = px2 - tx, dy2 = py2 - ty;
                float d2  = dx2 * dx2 + dy2 * dy2;
                if (!found || d2 < best_d2) {
                    best_d2 = d2; *out_x = px2; *out_y = py2; found = 1;
                }
            }
        }
    }
    return found;
}

/* ════════════════════════════════════════════════════════════════
   PRÉDICAT UNIQUE DE PLACEMENT — « ce corps tient-il ICI ? »
   ────────────────────────────────────────────────────────────────
   TOUT le système s'appuie sur ce seul test : déplacement réel, ligne
   de vue, validation d'une destination. Avant, le déplacement testait
   UN point (le bord avant, au centre) pendant que la ligne de vue
   testait un COULOIR de la largeur du corps : les deux se
   contredisaient, d'où l'unité qui « voit » un passage où elle ne
   tient pas, s'y engage, et pousse contre le mur indéfiniment.

   On teste toutes les tuiles recouvertes par la boîte [x±r, y±r] —
   2×2 au plus pour un gabarit normal, et correct quel que soit le
   rayon (un gros gabarit ne peut plus enjamber un mur entre deux
   coins). C'est aussi ce qui empêche le corps d'entrer dans le décor.
   ════════════════════════════════════════════════════════════════ */
int move_can_stand(const Map *map, const TowerPool *tp,
                   float x, float y, float radius, int flags) {
    int tx0 = (int)floorf((x - radius) / (float)TILE_SIZE);
    int tx1 = (int)floorf((x + radius) / (float)TILE_SIZE);
    int ty0 = (int)floorf((y - radius) / (float)TILE_SIZE);
    int ty1 = (int)floorf((y + radius) / (float)TILE_SIZE);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++)
            if (move_tile_blocked(map, tp, tx, ty, flags)) return 0;
    return 1;
}

int move_slide(const Map *map, const TowerPool *tp,
               float *px, float *py, float dx, float dy,
               float radius, int flags) {
    float ox = *px, oy = *py;

    /* Anti-blocage : si la position COURANTE est déjà invalide (spawn
       raté, tour posée sous l'entité…), tout mouvement est permis —
       c'est la seule façon de ressortir. */
    if (!move_can_stand(map, tp, *px, *py, radius, flags)) {
        *px += dx; *py += dy;
    } else if (move_can_stand(map, tp, *px + dx, *py + dy, radius, flags)) {
        /* 1. PAS COMPLET (x ET y ensemble) — a tester EN PREMIER.
           Le glissement axe par axe evalue le pas en x avec l'ANCIEN y :
           un corps a cheval sur deux rangees se voyait refuser un pas que
           le deplacement complet autorise (mesure : refus pour 2/100 de
           pixel de difference en y). C'etait la contradiction exacte avec
           la ligne de vue, qui, elle, avance les deux axes ensemble —
           donc « je vois la voie libre mais je n'avance pas ». */
        *px += dx; *py += dy;
    } else {
        /* 2. GLISSEMENT le long du mur : un seul axe passe. */
        if (move_can_stand(map, tp, *px + dx, *py, radius, flags)) *px += dx;
        if (move_can_stand(map, tp, *px, *py + dy, radius, flags)) *py += dy;
    }

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

/* Variante « grille » du prédicat de placement : même géométrie que
   move_can_stand mais lue dans la grille O(1) de la frame. C'est ce
   qui garantit que la ligne de vue et le déplacement réel répondent
   la MÊME chose — la contradiction entre les deux était la cause du
   « elle fonce dans le mur et pousse ». */
static int nav_can_stand(float x, float y, float radius) {
    int tx0 = (int)floorf((x - radius) / (float)TILE_SIZE);
    int tx1 = (int)floorf((x + radius) / (float)TILE_SIZE);
    int ty0 = (int)floorf((y - radius) / (float)TILE_SIZE);
    int ty1 = (int)floorf((y + radius) / (float)TILE_SIZE);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++)
            if (nav_blocked(tx, ty)) return 0;
    return 1;
}

int move_line_clear(const Map *map, const TowerPool *tp,
                    float x0, float y0, float x1, float y1,
                    float radius, int flags) {
    nav_grid_sync(map, tp, flags);

    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < MOVE_EPS) return nav_can_stand(x0, y0, radius);
    float ux = dx / len, uy = dy / len;

    /* Pas d'échantillonnage <= rayon : deux boîtes consécutives se
       recouvrent, aucun obstacle ne peut se glisser entre elles.
       (L'ancien pas de 0,25 tuile laissait un coin « piquer » dans le
       couloir sans être vu.) */
    float stepd = radius > 2.0f ? radius : 2.0f;
    if (stepd > (float)TILE_SIZE * MOVE_NAV_LOS_STEP)
        stepd = (float)TILE_SIZE * MOVE_NAV_LOS_STEP;
    int n = (int)(len / stepd) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i * stepd;
        if (t > len) t = len;
        if (!nav_can_stand(x0 + ux * t, y0 + uy * t, radius)) return 0;
    }
    return 1;
}

/* ── BFS tuile-à-tuile → chemin COMPLET (départ → but) ────────────
   Le but lui-même bloqué (bâtiment de base pour un allié…) est atteint
   par ADJACENCE. BUT INATTEIGNABLE (poche enclavée, autre rive) : on
   rend le chemin vers la tuile atteignable la PLUS PROCHE du but —
   même règle qu'un clic de minicarte, « va aussi loin que possible »,
   au lieu de laisser l'entité vibrer contre l'obstacle.
   Retourne la longueur du chemin (0 = on est déjà au plus près).
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

    /* 8 voisins : les diagonales suppriment les trajets « en escalier »
       (le joueur voyait ses unités tourner inutilement). Une diagonale
       n'est autorisée que si les DEUX orthogonales adjacentes sont
       libres : sinon l'entité couperait un coin où son corps ne passe
       pas — exactement ce que le déplacement refusera ensuite. */
    static const int DX[8] = {1, -1, 0, 0,  1,  1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1,  1, -1,  1, -1};
    int found = -1;
    /* Repli « au plus près » : meilleure tuile atteinte (départ inclus). */
    int best_i  = 0;
    int best_d2 = (sx - gx) * (sx - gx) + (sy - gy) * (sy - gy);

    while (head < tail && found < 0) {
        int cx = qx[head], cy = qy[head];
        for (int d = 0; d < 8; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= hgt) continue;
            if (d >= 4 &&                       /* diagonale : pas de coin coupé */
                (g_nav_blk[cy][nx] || g_nav_blk[ny][cx])) continue;
            if (vis[ny * w + nx] == gen) continue;
            int at_goal = (nx == gx && ny == gy);
            if (g_nav_blk[ny][nx]) {
                if (at_goal && goal_blocked) { found = head; break; }
                continue;
            }
            vis[ny * w + nx] = gen;
            qx[tail] = (short)nx; qy[tail] = (short)ny;
            par[tail] = head;
            int d2 = (nx - gx) * (nx - gx) + (ny - gy) * (ny - gy);
            if (d2 < best_d2) { best_d2 = d2; best_i = tail; }
            if (at_goal) found = tail;
            tail++;
            if (tail >= NMAX) break;
            if (found >= 0) break;
        }
        head++;
    }
    /* But hors d'atteinte → chemin vers le point atteignable le plus
       proche. `best_i == 0` = on y est déjà : aucun chemin à suivre. */
    if (found < 0) found = best_i;
    if (found <= 0) return 0;

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

/* Approche MONOTONE : on n'avance que si le pas RAPPROCHE reellement
   de la cible. Regle unique du deplacement, et c'est elle qui tue les
   deux pathologies observees :
     - l'oscillation (un pas lateral qui ne rapproche pas est REFUSE,
       donc le filet anti-blocage finit par se declencher au lieu de
       croire que « ca bouge ») ;
     - l'arret trop loin d'un but situe DANS un mur (centre d'une base) :
       l'entite glisse le long de l'obstacle tant qu'elle se rapproche,
       puis s'arrete NET au contact, au plus pres possible.
   Les deviations ±45°/±90° restent proposees (contournement local),
   mais seules celles qui rapprochent sont retenues. */
static int move_approach(const Map *map, const TowerPool *tp,
                         float *px, float *py, float tx, float ty,
                         float step, float radius, int flags) {
    float dx = tx - *px, dy = ty - *py;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < MOVE_EPS || step <= 0.0f) return 0;
    if (step > dist) step = dist;          /* ne jamais depasser la cible */
    dx /= dist; dy /= dist;

    static const float DEV[5] = {
        0.0f, MOVE_DEVIATE_1, -MOVE_DEVIATE_1, MOVE_DEVIATE_2, -MOVE_DEVIATE_2
    };
    for (int k = 0; k < 5; k++) {
        float c = cosf(DEV[k]), sn = sinf(DEV[k]);
        float rx = (dx * c - dy * sn) * step;
        float ry = (dx * sn + dy * c) * step;
        float cx = *px, cy = *py;
        if (!move_slide(map, tp, &cx, &cy, rx, ry, radius, flags)) continue;
        float nd = sqrtf((tx - cx) * (tx - cx) + (ty - cy) * (ty - cy));
        if (nd < dist - MOVE_PROGRESS_EPS) { *px = cx; *py = cy; return 1; }
    }
    return 0;
}

int move_nav_toward(const Map *map, const TowerPool *tp, MoveNav *nav,
                    float *px, float *py, float tx, float ty,
                    float step, float radius, int flags) {
    nav_grid_sync(map, tp, flags);

    /* But deplace de plus d'une tuile -> le chemin courant est caduc. */
    float gdx = tx - nav->gx, gdy = ty - nav->gy;
    if (gdx * gdx + gdy * gdy > (float)(TILE_SIZE * TILE_SIZE)) {
        nav->gx = tx; nav->gy = ty;
        nav->active = 0;
        nav->stuck  = 0;
        nav->best_d = 0.0f;        /* 0 = « pas encore mesure » */
    }

    /* ── 0. CORPS DANS LE DECOR ───────────────────────────
       Tour posee sous l'entite, apparition ratee, save heritee... Il
       faut POUSSER pour ressortir : la regle monotone l'en empecherait
       (sortir d'un mur eloigne souvent du but). Cas rare et transitoire. */
    if (!move_can_stand(map, tp, *px, *py, radius, flags)) {
        nav->active = 0;
        return move_toward(map, tp, px, py, tx, ty, step, radius, flags);
    }

    /* ── 1. LIGNE DEGAGEE -> tout droit, aucun calcul de chemin ────
       Cas de tres loin le plus frequent : terrain ouvert. On NE sort PAS
       de la fonction : le filet anti-blocage (etape 5) doit couvrir ce
       cas aussi, sinon une entite qui « voit » son but sans pouvoir
       avancer reste figee sans jamais replanifier. */
    int los_direct = move_line_clear(map, tp, *px, *py, tx, ty, radius, flags);
    if (los_direct) nav->active = 0;

    /* ── 2. Waypoint courant : atteint, ou devenu invalide ? ────── */
    if (!los_direct && nav->active) {
        float wdx = nav->wx - *px, wdy = nav->wy - *py;
        float arrive = (float)TILE_SIZE * MOVE_NAV_ARRIVE_FRAC;
        if (wdx * wdx + wdy * wdy < arrive * arrive) {
            nav->active = 0;                 /* atteint -> suite du chemin */
            nav->best_d = 0.0f;
        } else if (!move_line_clear(map, tp, *px, *py, nav->wx, nav->wy,
                                    radius, flags)) {
            nav->active = 0;                 /* une tour a coupe la route */
            nav->best_d = 0.0f;
        }
    }

    /* ── 3. (Re)planification : BFS + TIR A LA CORDE ────────────
       On vise le point le PLUS LOIN du chemin encore atteignable en
       ligne droite : trajectoire longue et naturelle au lieu d'un
       cheminement case par case colle a la grille. */
    if (!los_direct && !nav->active) {
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
            nav->best_d = 0.0f;
        }
    }

    /* ── 4. Avance, TOUJOURS en approche monotone ──────────────
       - waypoint actif : on le rejoint (ligne de vue garantie) ;
       - aucun chemin : approche FINALE directe du but. Elle amene au
         contact de l'obstacle (un but dans un mur, comme le centre
         d'une base, est ainsi approche a ~1/2 tuile) puis s'arrete. */
    float ptx = nav->active ? nav->wx : tx;
    float pty = nav->active ? nav->wy : ty;
    int   moved = move_approach(map, tp, px, py, ptx, pty, step, radius, flags);

    /* ── 5. Filet anti-blocage fonde sur le PROGRES ────────────
       L'ancien filet comptait les frames SANS MOUVEMENT : une entite
       qui oscillait « bougeait », donc il ne se declenchait jamais.
       On mesure desormais la meilleure distance atteinte vers la cible
       COURANTE (waypoint ou but) : sans gain reel pendant
       MOVE_NAV_STUCK_FRAMES, on jette le chemin et on replanifie. */
    float cd = sqrtf((ptx - *px) * (ptx - *px) + (pty - *py) * (pty - *py));
    if (nav->best_d <= 0.0f || cd < nav->best_d - MOVE_PROGRESS_MIN) {
        nav->best_d = cd;
        nav->stuck  = 0;
    } else if (++nav->stuck >= MOVE_NAV_STUCK_FRAMES) {
        nav->stuck  = 0;
        nav->active = 0;          /* replanification a la frame suivante */
        nav->best_d = 0.0f;
    }
    return moved;
}

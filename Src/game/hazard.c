/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hazard.c ─ Hasards de terrain : génération déterministe + effets.
 *
 *  Placement : quelques tronçons de CHEMIN (là où passent les ennemis), loin
 *  des spawns (le joueur doit avoir le temps d'y combiner ses tours) et loin
 *  des bases (les unités y patrouillent : piège trop facile à subir). Chaque
 *  tronçon = une tuile-graine + ses voisines de chemin (remplissage borné).
 *
 *  Effets : par IMPULSIONS (HAZ_PULSE), pas à chaque frame — chaque coup passe
 *  par combat_apply_damage (résistances, brûlure, poison, retours EFFICACE /
 *  RÉSISTE), qui joue un son : à la frame, ce serait un mitraillage.
 */
#include "hazard.h"
#include "game_state.h"
#include "../combat/projectile.h"   // combat_apply_damage (chokepoint dégâts typés)
#include "../ui/hud.h"              // ui_push_notif (annonce du hasard)
#include "achievements.h"           // « Le terrain pour allié »
#include <string.h>
#include <stdlib.h>

// ── Placement ─────────────────────────────────────────────────────
#define HAZ_PATCHES_PER_PATH  1    // tronçons piégés par chemin actif…
#define HAZ_MIN_PATCHES       2    // …au moins (carte à 1 chemin)
#define HAZ_MAX_PATCHES       4    // …au plus
#define HAZ_PATCH_TILES       3    // tuiles par tronçon
#define HAZ_MIN_DIST_SPAWN    4    // tuiles (Manhattan) d'un spawn
#define HAZ_MIN_DIST_BASE     6    // tuiles (Manhattan) d'une base
#define HAZ_MIN_SPACING       7    // tuiles entre deux graines de tronçon
#define HAZ_SEED_SALT         0x5A17C0DEu   // décorrèle du tirage de la carte

// ── Effets (par impulsion de HAZ_PULSE s de temps de jeu) ─────────
#define HAZ_PULSE             0.5f
#define HAZ_EMBER_DMG         5.0f   // feu (ajoute un palier de brûlure)
#define HAZ_TOXIC_DMG         6.0f   // poison (DoT + corrosion)
#define HAZ_SAND_SLOW         0.7f   // s de ralentissement, recharge à chaque impulsion
#define HAZ_WIRE_DMG          8.0f   // électrique
#define HAZ_MOLTEN_DMG        8.0f   // feu
#define HAZ_UNIT_DMG          3.0f   // PV par impulsion pour une unité du joueur

static const HazardDef HAZ_DEFS[HAZ_TYPE_COUNT] = {
    [HAZ_NONE]      = { "",                 "",                               {0, 0, 0, 0} },
    [HAZ_EMBERS]    = { "Braises",          "brûlent ce qui les traverse",    {255, 140,  50, 255} },
    [HAZ_TOXIC]     = { "Mare toxique",     "empoisonne et corrode",          {120, 220,  70, 255} },
    [HAZ_QUICKSAND] = { "Sables mouvants",  "ralentissent (cible vulnérable)", {226, 196, 120, 255} },
    [HAZ_WIRES]     = { "Câbles à nu",      "électrocutent ce qui passe",     {120, 200, 255, 255} },
    [HAZ_MOLTEN]    = { "Métal en fusion",  "brûle très fort",                {255,  96,  40, 255} },
};

const HazardDef *hazard_def(HazardType t) {
    if (t < 0 || t >= HAZ_TYPE_COUNT) t = HAZ_NONE;
    return &HAZ_DEFS[t];
}

static HazardType theme_hazard(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return HAZ_TOXIC;
        case THEME_DESERT:  return HAZ_QUICKSAND;
        case THEME_CITY:    return HAZ_WIRES;
        case THEME_FACTORY: return HAZ_MOLTEN;
        case THEME_WASTELAND:
        default:            return HAZ_EMBERS;
    }
}

// ════════════════════════════════════════════════════════════════
// COUCHE (cache non sauvegardé, clé = identité de la carte)
// ════════════════════════════════════════════════════════════════
static unsigned char g_layer[MAX_MAP_H][MAX_MAP_W];
static int g_count;
static int g_key_valid;
static int g_key_seed, g_key_w, g_key_h, g_key_paths;
static ThemeID g_key_theme;

static unsigned int lcg(unsigned int *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}
static int manhattan(int ax, int ay, int bx, int by) {
    return abs(ax - bx) + abs(ay - by);
}

static int far_from_ends(const Map *m, int x, int y) {
    for (int p = 0; p < m->path_count && p < MAX_PATHS; p++) {
        if (!m->paths[p].active) continue;
        if (manhattan(x, y, m->paths[p].spawn.x, m->paths[p].spawn.y) < HAZ_MIN_DIST_SPAWN)
            return 0;
    }
    for (int b = 0; b < m->base_count && b < MAX_BASES; b++)
        if (manhattan(x, y, m->bases[b].pos.x, m->bases[b].pos.y) < HAZ_MIN_DIST_BASE)
            return 0;
    return 1;
}

static void build_layer(const Map *m) {
    memset(g_layer, 0, sizeof(g_layer));
    g_count = 0;
    int active_paths = 0;
    for (int p = 0; p < m->path_count && p < MAX_PATHS; p++)
        if (m->paths[p].active) active_paths++;
    int want = active_paths * HAZ_PATCHES_PER_PATH;
    if (want < HAZ_MIN_PATCHES) want = HAZ_MIN_PATCHES;
    if (want > HAZ_MAX_PATCHES) want = HAZ_MAX_PATCHES;

    /* Candidats : tuiles de chemin bien placées. */
    static int cx[MAX_MAP_W * MAX_MAP_H], cy[MAX_MAP_W * MAX_MAP_H];
    int nc = 0;
    for (int y = 0; y < m->h && y < MAX_MAP_H; y++)
        for (int x = 0; x < m->w && x < MAX_MAP_W; x++)
            if (m->tiles[y][x].type == TILE_PATH && far_from_ends(m, x, y)) {
                cx[nc] = x; cy[nc] = y; nc++;
            }
    if (nc == 0) return;

    HazardType kind = theme_hazard(m->theme);
    unsigned int rs = (unsigned int)m->seed ^ HAZ_SEED_SALT;
    int seeds_x[HAZ_MAX_PATCHES], seeds_y[HAZ_MAX_PATCHES], ns = 0;
    for (int tries = 0; tries < nc * 2 && ns < want; tries++) {
        int k = (int)(lcg(&rs) % (unsigned int)nc);
        int ok = 1;
        for (int s = 0; s < ns; s++)
            if (manhattan(cx[k], cy[k], seeds_x[s], seeds_y[s]) < HAZ_MIN_SPACING) ok = 0;
        if (!ok) continue;
        seeds_x[ns] = cx[k]; seeds_y[ns] = cy[k]; ns++;

        /* Tronçon : la graine puis ses voisines de chemin, en largeur. */
        int qx[HAZ_PATCH_TILES], qy[HAZ_PATCH_TILES], qn = 0, qi = 0;
        qx[qn] = cx[k]; qy[qn] = cy[k]; qn++;
        g_layer[cy[k]][cx[k]] = (unsigned char)kind;
        while (qi < qn && qn < HAZ_PATCH_TILES) {
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4 && qn < HAZ_PATCH_TILES; d++) {
                int nx = qx[qi] + DX[d], ny = qy[qi] + DY[d];
                if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h) continue;
                if (m->tiles[ny][nx].type != TILE_PATH || g_layer[ny][nx]) continue;
                if (!far_from_ends(m, nx, ny)) continue;
                g_layer[ny][nx] = (unsigned char)kind;
                qx[qn] = nx; qy[qn] = ny; qn++;
            }
            qi++;
        }
        g_count += qn;
    }
}

static void ensure_layer(const Map *m) {
    if (g_key_valid && g_key_seed == m->seed && g_key_w == m->w && g_key_h == m->h &&
        g_key_paths == m->path_count && g_key_theme == m->theme)
        return;
    build_layer(m);
    g_key_valid = 1;
    g_key_seed  = m->seed;  g_key_w = m->w;  g_key_h = m->h;
    g_key_paths = m->path_count;  g_key_theme = m->theme;
}

HazardType hazard_at(const Map *map, int tx, int ty) {
    if (!map || tx < 0 || ty < 0 || tx >= map->w || ty >= map->h ||
        tx >= MAX_MAP_W || ty >= MAX_MAP_H)
        return HAZ_NONE;
    ensure_layer(map);
    return (HazardType)g_layer[ty][tx];
}

int hazard_tile_count(const Map *map) {
    if (!map) return 0;
    ensure_layer(map);
    return g_count;
}

// ════════════════════════════════════════════════════════════════
// EFFETS
// ════════════════════════════════════════════════════════════════
static HazardType hazard_under(const Map *m, float px, float py) {
    return hazard_at(m, (int)(px / TILE_SIZE), (int)(py / TILE_SIZE));
}

static void strike_enemy(Enemy *e, HazardType h) {
    float k = g_run_mods.hazard_mult;   // clé de voûte Géomancien (1 sinon)
    switch (h) {
        case HAZ_EMBERS:    combat_apply_damage(e, HAZ_EMBER_DMG  * k, DMG_FIRE);     break;
        case HAZ_TOXIC:     combat_apply_damage(e, HAZ_TOXIC_DMG  * k, DMG_POISON);   break;
        case HAZ_WIRES:     combat_apply_damage(e, HAZ_WIRE_DMG   * k, DMG_ELECTRIC); break;
        case HAZ_MOLTEN:    combat_apply_damage(e, HAZ_MOLTEN_DMG * k, DMG_FIRE);     break;
        case HAZ_QUICKSAND: /* immunités (Blindé, Briseur) gérées par enemy.c */
            if (e->slow_timer < HAZ_SAND_SLOW) e->slow_timer = HAZ_SAND_SLOW;
            break;
        default: break;
    }
}

void hazard_update(GameState *gs, float dt) {
    static float pulse_acc   = 0.0f;
    static int   announced   = 0;
    static int   ann_seed    = 0;
    static int   terrain_kills = 0;   // ennemis achevés par le terrain (cette carte)
    const Map *m = &gs->map;
    if (hazard_tile_count(m) == 0) return;

    /* Annonce une fois par carte (au premier tick de jeu) : le joueur doit
       savoir que ce décor-là MORD avant de placer ses tours. Nom seul : une
       notification tient en 51 octets ; l'effet s'affiche au survol. */
    if (!announced || ann_seed != m->seed) {
        announced = 1; ann_seed = m->seed; terrain_kills = 0;
        const HazardDef *d = hazard_def(theme_hazard(m->theme));
        ui_push_notif(&gs->ui, TextFormat("Terrain piégé : %s", d->name), d->color);
    }

    pulse_acc += dt;
    if (pulse_acc < HAZ_PULSE) return;
    pulse_acc -= HAZ_PULSE;
    if (pulse_acc > HAZ_PULSE) pulse_acc = 0.0f;   // pas de rafale après une pause

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies.enemies[i];
        if (!e->active || e->dead) continue;
        HazardType h = hazard_under(m, e->x, e->y);
        if (h == HAZ_NONE) continue;
        strike_enemy(e, h);
        if ((e->dead || e->hp <= 0.0f) && ++terrain_kills >= ACH_TERRAIN_KILLS)
            ach_unlock(ACH_TERRAIN, gs);   // idempotent
    }
    if (gs->phase != PHASE_WAVE) return;   // hors vague : on ne punit pas le rangement
    if (g_run_mods.hazard_units_immune) return;   // clé de voûte Géomancien
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &gs->units.units[i];
        if (!u->active) continue;
        HazardType h = hazard_under(m, u->x, u->y);
        if (h != HAZ_NONE && h != HAZ_QUICKSAND) unit_damage(u, HAZ_UNIT_DMG);
    }
}

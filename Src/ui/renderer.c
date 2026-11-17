/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

// renderer.c
#include "renderer.h"
#include "tile_art.h"   // pixel-art procédural : spawns, bunkers, minerais
#include "terrain.h"    // terrain peint au pixel (sol, liquides, routes, ruines)
#include "render3d.h"   // rendu 3D des tours (textures pré-rendues)
#include "render3d_units.h"   // rendu 3D des unités (mode de vue 3D)
#include "render3d_enemies.h" // rendu 3D des ennemis (mode de vue 3D)
#include "../engine/assets.h"  // splash arts tours/unités
#include "raylib.h"
#include "ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "ui_palette.h" // palette UI (source unique) : UI_DANGER…
#include "../map/pathfinding.h"
#include "../map/map_gen.h"
#include "../map/theme.h"
#include "../combat/enemy.h"
#include "../combat/tower.h"
#include <math.h>
#include "../combat/unit.h"
#include "../game/game_state.h"
#include "hud.h"
#include "../combat/material.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* memset : grille d'occupation des tours */

// ── Couleurs des tours, unités et projectiles ─────────────────
// Sans static → accessibles via extern dans renderer.h
Color TOWER_FILL[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = {192, 57,  43,  255},
    [TOWER_SNIPER] = { 52,152, 219,  255},
    [TOWER_FLAME]  = {230,126,  34,  255},
    [TOWER_TESLA]  = {155, 89, 182,  255},
};
Color UNIT_FILL[UNIT_TYPE_COUNT] = {
    [UNIT_SOLDIER] = { 39,174,  96,  255},
    [UNIT_HEAVY]   = { 41,128, 185,  255},
    [UNIT_MEDIC]   = {231, 76,  60,  255},
    [UNIT_DOG]     = {243,156,  18,  255},
    [UNIT_WORKER]  = {200,200,  50, 255},
};
Color PROJ_COLOR[TOWER_TYPE_COUNT] = {
    [TOWER_GUN]    = {255,128,128,  255},
    [TOWER_SNIPER] = {135,206,235,  255},
    [TOWER_FLAME]  = {255,107, 53,  255},
    [TOWER_TESLA]  = {218,112,214,  255},
};
// Couleur d'identité de chaque matériau — SOURCE UNIQUE (était dupliquée et
// incohérente entre dépôts, ouvriers, bestiaire et boutique).
Color MATERIAL_COLORS[MAT_COUNT] = {
    [MAT_IRON]   = {160, 160, 180, 255},  // acier
    [MAT_ACID]   = { 80, 200,  50, 255},  // vert toxique
    [MAT_PLASMA] = { 80, 160, 255, 255},  // bleu plasma
    [MAT_CRYO]   = {140, 220, 255, 255},  // cyan glacial
    [MAT_NANO]   = {200, 100, 255, 255},  // violet nano
};


int   g_map_x_off          = 0;
int   g_canvas_virt_w      = MAP_W * TILE_SIZE;
int   g_canvas_virt_w_base = MAP_W * TILE_SIZE;
int   g_canvas_virt_h      = MAP_H * TILE_SIZE + UI_HUD_HEIGHT;
float g_map_render_scale   = 1.0f;   // zoom de la carte (1 = standard, <1 = grande carte)
int   g_colorblind         = 0;      // 1 = palette ennemis daltonien-safe (Okabe-Ito)
int   g_show_entity_names  = 0;      // 1 = nom des ennemis au-dessus des sprites (option)
int   g_units_3d           = 0;      // 1 = unités/ennemis en 3D ; 0 = sprites 2D (défaut)
float g_map_zoom           = 1.0f;   // zoom JOUEUR (molette), 1..3
float g_map_pan_x          = 0.0f;   // décalage horizontal (en pixels canvas) quand zoom>1
float g_map_pan_y          = 0.0f;   // décalage vertical

float map_eff_scale(void) { return g_map_render_scale * g_map_zoom; }

Vector2 map_origin(void) {
    return (Vector2){ (float)g_map_x_off + g_map_pan_x, g_map_pan_y };
}

Vector2 map_screen_to_world(Vector2 s) {
    float S = map_eff_scale();
    if (S < 0.0001f) S = 0.0001f;
    return (Vector2){ (s.x - ((float)g_map_x_off + g_map_pan_x)) / S,
                      (s.y - g_map_pan_y) / S };
}

Color renderer_tower_color(TowerType type) { return TOWER_FILL[type]; }
Color renderer_unit_color (UnitType  type) { return UNIT_FILL[type];  }

// Couleur d'identité par type d'ennemi (source unique : rendu + aperçu HUD).
Color renderer_enemy_color(EnemyType type) {
    static const Color C[ENEMY_TYPE_COUNT] = {
        [ENEMY_RAIDER]      = {231,  76,  60, 255},
        [ENEMY_BRUTE]       = {230, 126,  34, 255},
        [ENEMY_RUNNER]      = {243, 156,  18, 255},
        [ENEMY_VEHICLE]     = {127, 140, 141, 255},
        [ENEMY_MUTANT]      = { 39, 174,  96, 255},
        [ENEMY_GHOST]       = {180, 180, 255, 255},
        [ENEMY_PATHBREAKER] = {255,  80, 180, 255},
        [ENEMY_HEALER]      = {255, 100, 100, 255},
        [ENEMY_HUNTER]      = {255, 165,   0, 255},
        [ENEMY_ARTILLERY]   = { 90,  90, 100, 255},
    };
    // Palette daltonien-safe (Okabe-Ito) — activée par l'option « Daltonisme ».
    static const Color CB[ENEMY_TYPE_COUNT] = {
        [ENEMY_RAIDER]      = {213,  94,   0, 255},   // vermillon
        [ENEMY_BRUTE]       = {230, 159,   0, 255},   // orange
        [ENEMY_RUNNER]      = {240, 228,  66, 255},   // jaune
        [ENEMY_VEHICLE]     = {150, 150, 150, 255},   // gris
        [ENEMY_MUTANT]      = {  0, 158, 115, 255},   // vert bleuté
        [ENEMY_GHOST]       = { 86, 180, 233, 255},   // bleu ciel
        [ENEMY_PATHBREAKER] = {204, 121, 167, 255},   // mauve
        [ENEMY_HEALER]      = {  0, 114, 178, 255},   // bleu
        [ENEMY_HUNTER]      = {245, 245, 245, 255},   // blanc
        [ENEMY_ARTILLERY]   = {140, 110, 180, 255},   // violet
    };
    if (type < 0 || type >= ENEMY_TYPE_COUNT) return WHITE;
    return g_colorblind ? CB[type] : C[type];
}

// Couleur d'identité par base : vert pour la principale, palette distincte
// pour les secondaires (les différencie d'un coup d'œil).
Color renderer_base_color(int base_idx, int is_primary) {
    if (is_primary) return (Color){46, 204, 113, 255};   // vert
    static const Color SEC[6] = {
        { 52, 152, 219, 255},   // bleu
        { 26, 188, 156, 255},   // turquoise
        {155,  89, 182, 255},   // violet
        {241, 196,  15, 255},   // or
        {230, 126,  34, 255},   // orange
        { 80, 200, 120, 255},   // vert-mer
    };
    int i = base_idx % 6; if (i < 0) i = 0;
    return SEC[i];
}


// ── Une couleur distincte par chemin (20 entrées = MAX_PATHS) ─
static Color PATH_COLORS[MAX_PATHS] = {
    {239, 159,  39, 160},   //  0 — ocre
    { 39, 159, 239, 160},   //  1 — bleu acier
    {159, 239,  39, 160},   //  2 — vert radioactif
    {239,  39, 159, 160},   //  3 — rose vif
    { 39, 239, 159, 160},   //  4 — turquoise
    {159,  39, 239, 160},   //  5 — violet
    {239, 110,  39, 160},   //  6 — orange
    { 39, 210, 239, 160},   //  7 — cyan
    {210, 239,  39, 160},   //  8 — jaune-vert
    {239,  39, 110, 160},   //  9 — rouge-rose
    {110, 110, 239, 160},   // 10 — lavande
    {239, 210,  39, 160},   // 11 — jaune
    { 80, 230,  80, 160},   // 12 — vert clair
    {230,  80,  80, 160},   // 13 — rouge clair
    { 80,  80, 230, 160},   // 14 — bleu foncé
    {200, 150,  90, 160},   // 15 — brun doré
    { 90, 200, 150, 160},   // 16 — vert mer
    {150,  90, 200, 160},   // 17 — lilas
    {200,  90,  90, 160},   // 18 — brique rose
    { 90, 200, 200, 160},   // 19 — turquoise clair
};

// ════════════════════════════════════════════════════
// DÉTAILS PIXEL ART PAR THÈME
// ════════════════════════════════════════════════════
// ════════════════════════════════════════════════════
// CARTE — fonds pixel-art (délégués à tile_art)
// Les décors PATH/SPAWN/BASE sont dessinés ensuite par
// tile_art_draw_paths / tile_art_draw_spawns / render_bases.
// ════════════════════════════════════════════════════
/* Occupation des tuiles par les tours — CONSTRUITE UNE FOIS PAR FRAME.
   Interroger tower_at_tile() pour chaque tuile coûte w×h×MAX_TOWERS, soit
   jusqu'à 1792 × 64 ≈ 115 000 tests par frame sur une grande carte, alors
   qu'il n'y a JAMAIS plus de MAX_TOWERS tours à marquer. On inverse donc
   la boucle : on efface la grille puis on pose les ≤64 tours. */
static unsigned char g_tower_occ[MAX_MAP_H][MAX_MAP_W];

static void tower_occ_build(const Map *map, const TowerPool *tp) {
    for (int y = 0; y < map->h; y++)
        memset(g_tower_occ[y], 0, (size_t)map->w);
    if (!tp) return;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;
        if (tw->tile_x < 0 || tw->tile_x >= map->w) continue;
        if (tw->tile_y < 0 || tw->tile_y >= map->h) continue;
        g_tower_occ[tw->tile_y][tw->tile_x] = 1;
    }
}

void render_map(const Map *map, const TowerPool *tp, int cull_to_view) {
    tower_occ_build(map, tp);

    int x0 = 0, y0 = 0, x1 = map->w, y1 = map->h;
    if (cull_to_view) {
        /* CULLING : chaque tuile coûte plusieurs primitives procédurales.
           Hors zoom, tout tient à l'écran et rien n'est coupé ; zoomé (ou
           sur une grande carte), on évite de redessiner ce qui est hors
           cadre. Marge d'1 tuile : bords partiels + secousse caméra. */
        float area_h = (float)(g_canvas_virt_h - UI_HUD_HEIGHT);
        Vector2 w0 = map_screen_to_world((Vector2){0.0f, 0.0f});
        Vector2 w1 = map_screen_to_world((Vector2){(float)g_canvas_virt_w,
                                                   area_h});
        x0 = (int)(w0.x / TILE_SIZE) - 1;
        y0 = (int)(w0.y / TILE_SIZE) - 1;
        x1 = (int)(w1.x / TILE_SIZE) + 2;
        y1 = (int)(w1.y / TILE_SIZE) + 2;
        if (x0 < 0)      x0 = 0;
        if (y0 < 0)      y0 = 0;
        if (x1 > map->w) x1 = map->w;
        if (y1 > map->h) y1 = map->h;
    }

    /* Sol + liquides + routes : UNE texture peinte au pixel (terrain.h). */
    terrain_draw_base(map);
    float t = (float)GetTime();
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            TileType ty = map->tiles[y][x].type;
            /* Une tour sur la ruine la déblaie : sprite non dessiné. */
            if (ty == TILE_RUIN && !g_tower_occ[y][x]) terrain_draw_ruin(map, x, y);
            else if (ty == TILE_WATER) terrain_draw_liquid_fx(map, x, y, t);
        }
}

void render_spawn_exclusion_zones(const Map *map) {
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp = map->paths[i].spawn;
        int cx   = sp.x * TILE_SIZE + TILE_SIZE / 2;
        int cy   = sp.y * TILE_SIZE + TILE_SIZE / 2;
        int r    = SPAWN_EXCLUSION_RADIUS * TILE_SIZE + TILE_SIZE / 2;

        // Cercle rouge semi-transparent
        DrawCircle(cx, cy, (float)r, (Color){200, 40, 40, 18});
        DrawCircleLines(cx, cy, (float)r, (Color){200, 40, 40, 90});
    }
}

// ════════════════════════════════════════════════════
// BARRES DE VIE DES BASES
// Appelé depuis main.c après render_map()
// ════════════════════════════════════════════════════
void render_bases(const Map *map) {
    float t = (float)GetTime();
    for (int b = 0; b < map->base_count; b++) {
        const BaseInfo *base = &map->bases[b];

        int bpx = base->pos.x * TILE_SIZE;
        int bpy = base->pos.y * TILE_SIZE;
        int destroyed = (!base->active || base->hp <= 0);
        Color accent = renderer_base_color(b, base->is_primary);

        if (!destroyed) {
            // ── Onde de repérage : anneaux concentriques qui s'étendent ──
            // Rend l'emplacement de la base hyper visible (couleur d'identité).
            int   cx   = bpx + TILE_SIZE/2, cy = bpy + TILE_SIZE/2;
            float maxR = TILE_SIZE * 3.4f;
            for (int k = 0; k < 2; k++) {
                float ph = fmodf(t/2.2f + (float)b*0.31f + k*0.5f, 1.0f);
                unsigned char a = (unsigned char)((1.0f - ph) * 115);
                DrawCircleLines(cx, cy, ph*maxR, (Color){accent.r, accent.g, accent.b, a});
            }
            // Halo central doux pulsé
            float pul = (sinf(t*3.0f) + 1.0f) * 0.5f;
            DrawCircle(cx, cy, TILE_SIZE*0.55f + pul*4.0f,
                       (Color){accent.r, accent.g, accent.b, (unsigned char)(22 + pul*32)});
        }

        // Bunker pixel-art (détail HP/labels gérés par le HUD à gauche).
        tile_art_draw_base(bpx, bpy, map->theme, accent, destroyed);
    }
}

// ════════════════════════════════════════════════════
// HUD IN-GAME
// ════════════════════════════════════════════════════
void render_hud(const GameState *gs) {
    // render_hud est maintenu pour compatibilité mais le HUD principal
    // est maintenant entièrement géré par ui_render() en coords virtuelles.
    // Cette fonction affiche uniquement les infos debug optionnelles
    // qui ne rentrent pas dans le HUD UI (chemins, seed, etc.)
    // Elle n'est appelée QUE si on veut un overlay debug — sinon inutilisée.
    (void)gs;
}
// ════════════════════════════════════════════════════
// CHEMINS A*
// ════════════════════════════════════════════════════
void render_paths(const PathSet *ps) {
    for (int p = 0; p < ps->count; p++) {
        const Path *path = &ps->paths[p];
        if (!path->found) continue;
        Color col = PATH_COLORS[path->path_id % MAX_PATHS];
        for (int i = 0; i < path->len - 1; i++) {
            DrawLineEx(
                (Vector2){path->steps[i].x  *TILE_SIZE + TILE_SIZE/2.0f,
                          path->steps[i].y  *TILE_SIZE + TILE_SIZE/2.0f},
                (Vector2){path->steps[i+1].x*TILE_SIZE + TILE_SIZE/2.0f,
                          path->steps[i+1].y*TILE_SIZE + TILE_SIZE/2.0f},
                2.5f, col
            );
            DrawCircle(path->steps[i].x*TILE_SIZE+TILE_SIZE/2,
                       path->steps[i].y*TILE_SIZE+TILE_SIZE/2,
                       3, col);
        }
    }
}

// ════════════════════════════════════════════════════
// DÉPÔTS DE MATÉRIAUX
// ════════════════════════════════════════════════════
void render_deposits(const Map *map, const TowerPool *tp,
                     int hover_tx, int hover_ty, int wave_num) {
    static const char *MAT_ICONS[MAT_COUNT] = {
        [MAT_IRON]  = "Fe",
        [MAT_ACID]  = "Ac",
        [MAT_PLASMA]= "Pl",
        [MAT_CRYO]  = "Cr",
        [MAT_NANO]  = "Na",
    };

    float t = (float)GetTime();

    for (int i = 0; i < map->deposit_count; i++) {
        const MaterialDeposit *d = &map->deposits[i];
        int px = d->tile_x * TILE_SIZE;
        int py = d->tile_y * TILE_SIZE;

        // Filon actif : pixel-art plein qui remplace la tuile.
        if (d->active) {
            tile_art_draw_deposit(px, py, d->type, t);
            continue;
        }
        // Filon épuisé : roche minée (difficile à construire). Une tour
        // posée dessus déblaie le cratère (sol nu, dessiné par render_map).
        if (d->mined) {
            if (!tower_at_tile(tp, d->tile_x, d->tile_y))
                tile_art_draw_mined_rock(px, py);
            continue;
        }
        // Filon verrouillé (apparaît à une vague future) : indicateur discret
        // teinté de la couleur du minerai à venir, par-dessus la tuile de ruine.
        if (d->spawn_wave > 0) {
            int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
            Color mc = MATERIAL_COLORS[d->type];
            DrawCircle(cx, cy, TILE_SIZE / 2 - 6, (Color){18, 14, 8, 175});
            DrawCircleLines(cx, cy, TILE_SIZE / 2 - 4, (Color){mc.r, mc.g, mc.b, 130});
            int iw = mtxt(MAT_ICONS[d->type], 9);
            dtxt(MAT_ICONS[d->type], cx - iw/2, cy - 9, 9,
                 (Color){mc.r, mc.g, mc.b, 160});
            char _wbuf[8];
            snprintf(_wbuf, sizeof(_wbuf), "V%d", d->spawn_wave);
            int ww = mtxt(_wbuf, 9);
            dtxt(_wbuf, cx - ww/2, cy + 1, 9, (Color){150, 130, 70, 210});

            /* Survol : « V5 » seul est cryptique → phrase explicite. */
            if (d->tile_x == hover_tx && d->tile_y == hover_ty) {
                /* Ouverture au LANCEMENT de la vague spawn_wave (game_state.c). */
                int left = d->spawn_wave - wave_num;
                const char *tip = (left > 1)
                    ? TextFormat("Gisement %s : s'ouvre à la vague %d (dans %d vagues)",
                                 MATERIAL_NAMES[d->type], d->spawn_wave, left)
                    : TextFormat("Gisement %s : s'ouvre à la prochaine vague",
                                 MATERIAL_NAMES[d->type]);
                int tw2 = mtxt(tip, 10);
                int lx = cx - tw2 / 2, ly = py - 16;
                if (lx < 2) lx = 2;
                if (lx + tw2 > map->w * TILE_SIZE - 2) lx = map->w * TILE_SIZE - tw2 - 2;
                if (ly < 2) ly = py + TILE_SIZE + 2;
                DrawRectangle(lx - 3, ly - 2, tw2 + 6, 14, (Color){0, 0, 0, 170});
                dtxt(tip, lx, ly, 10, (Color){mc.r, mc.g, mc.b, 255});
            }
        }
    }
}

// ════════════════════════════════════════════════════
// MATÉRIAUX LÂCHÉS AU SOL
// ════════════════════════════════════════════════════
void render_dropped_mats(const DroppedMat *mats, int count) {
    static const char *MAT_ICONS[MAT_COUNT] = {
        [MAT_IRON]  = "Fe", [MAT_ACID]  = "Ac", [MAT_PLASMA]= "Pl",
        [MAT_CRYO]  = "Cr", [MAT_NANO]  = "Na",
    };
    float t = (float)GetTime();
    for (int i = 0; i < count; i++) {
        const DroppedMat *dm = &mats[i];
        if (!dm->active || dm->type < 0 || dm->type >= MAT_COUNT) continue;
        Color col = MATERIAL_COLORS[dm->type];
        // Clignotement si < 5s restantes
        float fade = (dm->lifetime < 5.0f)
                   ? (0.3f + 0.7f * ((sinf(t * 6.0f) + 1.0f) * 0.5f))
                   : 1.0f;
        // Halo rouge (danger)
        DrawCircle((int)dm->x, (int)dm->y, 10,
                   (Color){200,50,30,(unsigned char)(80*fade)});
        DrawCircleLines((int)dm->x, (int)dm->y, 10,
                        (Color){200,80,50,(unsigned char)(180*fade)});
        // Icône matériau
        int iw = mtxt(MAT_ICONS[dm->type], 9);
        dtxt(MAT_ICONS[dm->type], (int)dm->x - iw/2, (int)dm->y - 5, 9,
             (Color){col.r, col.g, col.b, (unsigned char)(220*fade)});
    }
}
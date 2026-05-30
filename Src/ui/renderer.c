/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

// renderer.c
#include "renderer.h"
#include "tile_art.h"   // pixel-art procédural : routes, spawns, bunkers
#include "../engine/assets.h"  // splash arts tours/unités
#include "raylib.h"
#include "ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
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


int   g_map_x_off          = 0;
int   g_canvas_virt_w      = MAP_W * TILE_SIZE;
int   g_canvas_virt_w_base = MAP_W * TILE_SIZE;
int   g_canvas_virt_h      = MAP_H * TILE_SIZE + UI_HUD_HEIGHT;
float g_map_render_scale   = 1.0f;   // zoom de la carte (1 = standard, <1 = grande carte)

Color renderer_tower_color(TowerType type) { return TOWER_FILL[type]; }
Color renderer_unit_color (UnitType  type) { return UNIT_FILL[type];  }

// ── Facteurs d'échelle des sprites (× le rayon logique de l'entité) ──
// SPRITE = côté de la box de rendu ; HALF = demi-côté (rayon visuel).
#define UNIT_SPRITE_SCALE   4.2f
#define UNIT_SPRITE_HALF    (UNIT_SPRITE_SCALE * 0.5f)
#define ENEMY_SPRITE_SCALE  3.8f
#define ENEMY_SPRITE_HALF   (ENEMY_SPRITE_SCALE * 0.5f)

// ── Dessine une texture centrée dans une box, ratio conservé ──
// Retourne 1 si une image a été dessinée, 0 si texture absente.
static int draw_sprite_fit(Texture2D tex, float cx, float cy,
                           float box_w, float box_h, Color tint) {
    if (tex.id == 0) return 0;
    float scale = fminf(box_w / (float)tex.width, box_h / (float)tex.height);
    float dw = (float)tex.width * scale, dh = (float)tex.height * scale;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){cx - dw/2.0f, cy - dh/2.0f, dw, dh},
        (Vector2){0, 0}, 0.0f, tint);
    return 1;
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
void render_map(const Map *map) {
    for (int y = 0; y < map->h; y++)
        for (int x = 0; x < map->w; x++)
            tile_art_draw_tile_bg(map, x, y);
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
    for (int b = 0; b < map->base_count; b++) {
        const BaseInfo *base = &map->bases[b];

        int bpx = base->pos.x * TILE_SIZE;
        int bpy = base->pos.y * TILE_SIZE;

        int   destroyed = (!base->active || base->hp <= 0);
        float ratio = (base->max_hp > 0)
                    ? (float)base->hp / (float)base->max_hp : 0.0f;

        // Couleur d'équipe : vert (primaire) / bleu (secondaire),
        // vire à l'orange puis au rouge quand les PV chutent.
        Color accent;
        if (base->is_primary) {
            accent = ratio > 0.5f ? (Color){46, 204, 113, 255}
                   : ratio > 0.25f ? (Color){243, 156, 18, 255}
                                   : (Color){231, 76, 60, 255};
        } else {
            accent = ratio > 0.5f ? (Color){52, 152, 219, 255}
                   : ratio > 0.25f ? (Color){155, 89, 182, 255}
                                   : (Color){231, 76, 60, 255};
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
// ENNEMIS
// ════════════════════════════════════════════════════
void render_enemies(const EnemyPool *pool) {
    float t = (float)GetTime();
 
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &pool->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
 
        // Ombre
        DrawEllipse((int)e->x + 2, (int)e->y + (int)e->size,
                    (int)(e->size * 0.8f), (int)(e->size * 0.3f),
                    (Color){0,0,0,80});
 
        // Couleur de base par type
        Color col;
        switch (e->type) {
            case ENEMY_RAIDER:      col = (Color){231, 76,  60,  255}; break;
            case ENEMY_BRUTE:       col = (Color){230, 126, 34,  255}; break;
            case ENEMY_RUNNER:      col = (Color){243, 156, 18,  255}; break;
            case ENEMY_VEHICLE:     col = (Color){127, 140, 141, 255}; break;
            case ENEMY_MUTANT:      col = (Color){ 39, 174, 96,  255}; break;
            case ENEMY_GHOST:       col = (Color){180, 180, 255, 255}; break;
            case ENEMY_PATHBREAKER: col = (Color){255, 80,  180, 255}; break;
            case ENEMY_HEALER:      col = (Color){255, 100, 100, 255}; break;
            case ENEMY_HUNTER:      col = (Color){255, 165,   0, 255}; break;
            case ENEMY_ARTILLERY:   col = (Color){ 90,  90, 100, 255}; break;
            default:                col = WHITE;
        }
 
        // ── Teinte du sprite ──────────────────────────────────
        // WHITE = couleurs natives ; bleutée si ralenti ; alpha réduit
        // et clignotant pour le spectre.
        Color tint = WHITE;
        if (e->slow_timer > 0.0f) {
            tint = (Color){130, 185, 255, 255};   // gel → bleu
            col  = (Color){100, 180, 255, 255};   // (repli cercle)
        }
        unsigned char body_alpha = 255;
        if (e->type == ENEMY_GHOST) {
            float flicker = sinf(t * 8.0f + (float)i) * 0.5f + 0.5f;
            body_alpha = (unsigned char)(70 + (int)(flicker * 110));
        }
        tint.a = body_alpha;

        // ── Auras / cercles de portée (sous le sprite) ────────
        if (e->type == ENEMY_GHOST) {
            float flicker = sinf(t * 8.0f + (float)i) * 0.5f + 0.5f;
            DrawCircleLines((int)e->x, (int)e->y, (int)(e->size + 3),
                            (Color){180, 180, 255,
                                    (unsigned char)(40 + (int)(flicker * 50))});
        } else if (e->type == ENEMY_HEALER) {
            DrawCircle((int)e->x, (int)e->y, (int)e->heal_range,
                       (Color){46, 204, 113, 18});
            DrawCircleLines((int)e->x, (int)e->y, (int)e->heal_range,
                            (Color){46, 204, 113, 60});
        } else if (e->type == ENEMY_ARTILLERY && e->arty_target >= 0) {
            DrawCircleLines((int)e->x, (int)e->y, (int)e->arty_range,
                            (Color){231, 76, 60, 80});
        }

        // ── Corps : splash art (repli cercle coloré si absent) ─
        int e_drawn = 0;
        if (e->type < ENEMY_TYPE_COUNT)
            e_drawn = draw_sprite_fit(g_enemy_splash[e->type], e->x, e->y,
                                      e->size * ENEMY_SPRITE_SCALE,
                                      e->size * ENEMY_SPRITE_SCALE, tint);
        if (!e_drawn) {
            DrawCircle((int)e->x, (int)e->y, (int)e->size,
                       (Color){col.r, col.g, col.b, body_alpha});
            DrawCircleLines((int)e->x, (int)e->y, (int)e->size,
                            (Color){255,255,255,60});
        }

        // ── Marqueurs (au-dessus du sprite) ───────────────────
        // Pathbreaker dévié : flèche de trajectoire
        if (e->type == ENEMY_PATHBREAKER && e->path_broken) {
            float dx   = e->target_x - e->x;
            float dy   = e->target_y - e->y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > 1.0f) {
                float nx = dx/dist, ny = dy/dist;
                DrawLineEx((Vector2){e->x, e->y},
                           (Vector2){e->x + nx*20.0f, e->y + ny*20.0f},
                           2.0f, (Color){255, 80, 180, 180});
            }
        }
        // Hunter en traque : point jaune
        if (e->type == ENEMY_HUNTER && e->hunt_target >= 0)
            DrawCircle((int)e->x, (int)e->y, 3, (Color){255, 255, 0, 220});

        // ── Barre de vie ──────────────────────────────────────
        if (e->type != ENEMY_GHOST || e->hp < e->max_hp * 0.8f) {
            float ratio = e->hp / e->max_hp;
            int   bw    = (int)(e->size * 2.5f);
            int   bx    = (int)e->x - bw / 2;
            int   by    = (int)e->y - (int)(e->size * ENEMY_SPRITE_HALF) - 6;  // au-dessus du sprite
            DrawRectangle(bx, by, bw, 3, (Color){30,10,10,200});
            Color hp_col = ratio > 0.6f ? (Color){46,204,113,255}
                         : ratio > 0.3f ? (Color){243,156,18,255}
                                        : (Color){231,76,60,255};
            DrawRectangle(bx, by, (int)(bw * ratio), 3, hp_col);
            dtxt(ENEMY_BASE_STATS[e->type].name,
                     bx, by-12, 8, (Color){200,200,180,180});
        }
    }
}
// ════════════════════════════════════════════════════
// TOURS ET PROJECTILES
// ════════════════════════════════════════════════════
void render_towers(const TowerPool *tp) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (!tw->active) continue;

        int px = tw->tile_x * TILE_SIZE;
        int py = tw->tile_y * TILE_SIZE;
        int s  = TILE_SIZE;
        Color col = TOWER_FILL[tw->type];

        // Socle sombre
        DrawRectangle(px+2, py+2, s-4, s-4, (Color){30,20,10,255});

        // Corps : splash art à la place du rectangle central.
        // Repli sur le rectangle coloré si la texture est absente.
        float cx = tw->cx, cy = tw->cy;
        if (!draw_sprite_fit(g_tower_splash[tw->type], cx, cy,
                             (float)(s-8), (float)(s-8), WHITE)) {
            DrawRectangle(px+6, py+6, s-12, s-12, col);
        }

        // Canon : conserve sa couleur d'équipe et pivote vers la cible.
        float ex = cx + cosf(tw->angle) * (s*0.4f);
        float ey = cy + sinf(tw->angle) * (s*0.4f);
        DrawLineEx((Vector2){cx,cy}, (Vector2){ex,ey}, 3.0f, col);
        // Embout du canon
        DrawCircle((int)ex, (int)ey, 2.5f, col);
        DrawRectangleLines(px+6, py+6, s-12, s-12, (Color){255,255,255,40});

        for (int l = 0; l < tw->level && l < 3; l++)
            DrawRectangle(px+4+l*4, py+s-6, 3, 3, (Color){255,215,0,255});

        // Barre HP (seulement si la tour est endommagée par artillerie)
        if (tw->hp < 100.0f && tw->hp > 0.0f) {
            float ratio = tw->hp / 100.0f;
            int   bx    = px + 4;
            int   bw    = s - 8;
            int   by2   = py + 3;
            DrawRectangle(bx, by2, bw,                      3, (Color){12,  6,  2, 200});
            DrawRectangle(bx, by2, (int)(bw * ratio + 0.5f), 3,
                          ratio > 0.5f ? (Color){230,160, 20,255}
                        : ratio > 0.25f? (Color){230, 80, 20,255}
                                       : (Color){220, 30, 30,255});
        }
    }
}

void render_projectiles(const TowerPool *tp) {
    static const Color DMG_COLORS[5] = {
        [DMG_PHYSICAL] = {255, 200, 100, 255},
        [DMG_POISON]   = { 80, 220,  60, 255},
        [DMG_ELECTRIC] = {180, 100, 255, 255},
        [DMG_CRYO]     = {140, 220, 255, 255},
        [DMG_NANO]     = {220, 100, 220, 255},
    };

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;

        // Couleur selon type de dégâts si matériau appliqué,
        // sinon couleur de base de la tour
        Color col;
        if (p->dmg_type != DMG_PHYSICAL ||
            p->origin == TOWER_FLAME ||
            p->origin == TOWER_TESLA) {
            col = DMG_COLORS[p->dmg_type];
        } else {
            col = PROJ_COLOR[p->origin];
        }

        if (p->splash) {
            DrawCircle((int)p->x, (int)p->y, 5, col);
            DrawCircleLines((int)p->x, (int)p->y, 8,
                            (Color){col.r, col.g, col.b, 100});
        } else {
            DrawCircle((int)p->x, (int)p->y, 3, col);
        }
    }
}

void render_tower_preview(const Map *map, const TowerPool *tp,
                           TowerType type, int tile_x, int tile_y)
{
    // Zone de spawn — affiche le message d'interdiction
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp   = map->paths[i].spawn;
        int   dist = abs(tile_x - sp.x) + abs(tile_y - sp.y);
        if (dist <= SPAWN_EXCLUSION_RADIUS) {
            int px = tile_x * TILE_SIZE;
            int py = tile_y * TILE_SIZE;
            // Tuile rouge
            DrawRectangle(px+2, py+2, TILE_SIZE-4, TILE_SIZE-4,
                          (Color){200, 40, 40, 80});
            // Message centré
            const char *msg = "Zone spawn - interdit";
            int mw = mtxt(msg, 9);
            int lx = px + TILE_SIZE/2 - mw/2;
            int ly = py - 14;
            if (ly < 2) ly = py + TILE_SIZE + 2;
            DrawRectangle(lx-3, ly-2, mw+6, 13, (Color){0,0,0,180});
            dtxt(msg, lx, ly, 9, (Color){220, 80, 80, 255});
            return;
        }
    }

    if (!tower_can_place(tp, map, tile_x, tile_y)) return;

    const TowerStats *st  = &TOWER_BASE_STATS[type];
    Color col = TOWER_FILL[type];
    int px = tile_x * TILE_SIZE, py = tile_y * TILE_SIZE;

    // Tuile de relief — teinte orange pour signaler le surcoût
    int is_ruin = (map->tiles[tile_y][tile_x].type == TILE_RUIN);
    Color preview_col = is_ruin
        ? (Color){230, 126, 34, 120}   // orange = surcoût
        : (Color){col.r, col.g, col.b, 100};

    DrawRectangle(px+6, py+6, TILE_SIZE-12, TILE_SIZE-12, preview_col);

    float range_px = st->range * TILE_SIZE;
    DrawCircleLines(px+TILE_SIZE/2, py+TILE_SIZE/2,
                    range_px, (Color){col.r, col.g, col.b, 150});
    DrawCircle(px+TILE_SIZE/2, py+TILE_SIZE/2,
               range_px, (Color){col.r, col.g, col.b, 20});

    // Label surcoût — affiché au-dessus de la tuile
    int real_cost = tower_cost_on_tile(type, map, tile_x, tile_y);
    const char *cost_label;
    if (is_ruin)
        cost_label = TextFormat("%s — %dor (relief x2)", st->name, real_cost);
    else
        cost_label = TextFormat("%s — %dor", st->name, real_cost);

    Color label_col = is_ruin ? (Color){230, 126, 34, 255}
                               : (Color){239, 159,  39, 220};

    int lw = mtxt(cost_label, 10);
    int lx = px + TILE_SIZE/2 - lw/2;
    int ly = py - 16;

    // S'assure que le label reste dans la zone de jeu
    if (lx < 2) lx = 2;
    if (lx + lw > g_canvas_virt_w_base - 2) lx = g_canvas_virt_w_base - lw - 2;
    if (ly < 2) ly = py + TILE_SIZE + 2;

    // Fond du label pour lisibilité
    DrawRectangle(lx - 3, ly - 2, lw + 6, 14, (Color){0, 0, 0, 160});
    dtxt(cost_label, lx, ly, 10, label_col);
}

// ════════════════════════════════════════════════════
// UNITÉS
// ════════════════════════════════════════════════════
void render_units(const UnitPool *up) {
    DrawCircleLines((int)up->base_px, (int)up->base_py,
                    UNIT_BASE_STATS[UNIT_SOLDIER].intercept_range * TILE_SIZE,
                    (Color){231,76,60,40});

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &up->units[i];
        if (!u->active) continue;

        // Couleur selon type
        Color col = (u->type < UNIT_TYPE_COUNT) ? UNIT_FILL[u->type]
                  : (Color){200, 200, 50, 255};

        // ── Ouvrier : halo et barre de collecte ──────────
        if (u->type == UNIT_WORKER) {
            // Note: ligne pointillée vers dépôt non implémentée
            // (Raylib n'a pas de DrawLineDashed natif)

            // Si porte un matériau : halo coloré (rayon = autour du sprite)
            if (u->has_material && u->carried_mat != MAT_NONE) {
                static const Color MAT_COL[MAT_COUNT] = {
                    {160,160,180,255},{80,220,60,255},
                    {80,160,255,255},{140,220,255,255},{200,100,255,255},
                };
                DrawCircle((int)u->x, (int)u->y, (int)(u->size * UNIT_SPRITE_HALF + 5),
                           (Color){MAT_COL[u->carried_mat].r,
                                   MAT_COL[u->carried_mat].g,
                                   MAT_COL[u->carried_mat].b, 80});
            }

            // Barre de collecte (au-dessus du sprite)
            if (u->state == USTATE_COLLECT && u->collect_duration > 0.0f) {
                float ratio = 1.0f - (u->collect_timer / u->collect_duration);
                int   bw    = (int)(u->size * 3.0f);
                int   bx    = (int)u->x - bw/2;
                int   by    = (int)u->y - (int)(u->size * UNIT_SPRITE_HALF) - 10;
                DrawRectangle(bx, by, bw, 4, (Color){20, 20, 20, 200});
                DrawRectangle(bx, by, (int)(bw * ratio), 4,
                              (Color){80, 200, 220, 255});
                dtxt("...", bx, by - 10, 8, (Color){80, 200, 220, 200});
            }
        }

        // ── Auras visibles quand l'unité est sélectionnée ────────
        if (up->selected_unit == i && u->type != UNIT_WORKER) {
            Color uc = UNIT_FILL[u->type];
            // Portée d'attaque — uniquement pour les unités à longue portée
            if (u->atk_range > UNIT_MELEE_ATK_THRESHOLD) {
                int ar = (int)(u->atk_range * TILE_SIZE);
                DrawCircle((int)u->x, (int)u->y, ar,
                           (Color){uc.r, uc.g, uc.b, 14});
                DrawCircleLines((int)u->x, (int)u->y, ar,
                                (Color){uc.r, uc.g, uc.b, 100});
            }
            // Aura de soin du médic
            if (u->type == UNIT_MEDIC) {
                int hr = (int)(UNIT_MEDIC_HEAL_RANGE * TILE_SIZE);
                DrawCircle((int)u->x, (int)u->y, hr,
                           (Color){231, 76, 60, 20});
                DrawCircleLines((int)u->x, (int)u->y, hr,
                                (Color){231, 76, 60, 110});
            }
        }

        // ── Ombre au sol ──────────────────────────────────
        float rad = u->size * UNIT_SPRITE_HALF;   // demi-taille visuelle du sprite
        DrawEllipse((int)u->x+1, (int)(u->y + rad),
                    (int)(rad*0.8f), (int)(rad*0.3f),
                    (Color){0,0,0,70});

        // ── Corps : splash art (cercle = gabarit d'origine) ───────
        int drawn = 0;
        if (u->type < UNIT_TYPE_COUNT)
            drawn = draw_sprite_fit(g_unit_splash[u->type], u->x, u->y,
                                    u->size * UNIT_SPRITE_SCALE,
                                    u->size * UNIT_SPRITE_SCALE, WHITE);
        if (!drawn) {
            DrawCircle((int)u->x, (int)u->y, (int)u->size, col);
            DrawCircleLines((int)u->x, (int)u->y, (int)u->size,
                            (Color){255,255,255,80});
        }

        // ── Anneaux (DESSUS du sprite, encerclent le corps) ───────
        // Sélection : anneau blanc
        if (up->selected_unit == i)
            DrawCircleLines((int)u->x, (int)u->y, rad + 2.0f,
                            (Color){255,255,255,200});
        // Comportement : anneau coloré
        if (u->type != UNIT_WORKER && u->behavior != UBEH_PATROL) {
            static const Color BCOLS[5] = {
                {0,0,0,0},{192,57,43,220},{200,200,50,220},
                {82,155,200,220},{231,76,60,220}
            };
            int _bidx = (int)u->behavior;
            if (_bidx < 0 || _bidx >= 5) _bidx = 0;
            DrawCircleLines((int)u->x, (int)u->y, rad + 4.0f, BCOLS[_bidx]);
        }

        // ── Barre de vie (au-dessus du sprite) ────────────────────
        int bw = (int)(u->size*2.5f);
        int bx = (int)u->x - bw/2;
        int by = (int)(u->y - rad) - 6;
        DrawRectangle(bx, by, bw, 3, (Color){10,30,10,200});
        float ratio = u->hp / u->max_hp;
        DrawRectangle(bx, by, (int)(bw*ratio), 3,
                      ratio > 0.5f ? (Color){46,204,113,255}
                                   : (Color){231,76,60,255});
    }
}
// ════════════════════════════════════════════════════
// DÉPÔTS DE MATÉRIAUX
// ════════════════════════════════════════════════════
void render_deposits(const Map *map) {
    static const Color MAT_COLORS[MAT_COUNT] = {
        [MAT_IRON]  = {160, 160, 180, 255},
        [MAT_ACID]  = { 80, 200,  50, 255},
        [MAT_PLASMA]= { 80, 160, 255, 255},
        [MAT_CRYO]  = {140, 220, 255, 255},
        [MAT_NANO]  = {200, 100, 255, 255},
    };
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
        int cx = px + TILE_SIZE / 2;
        int cy = py + TILE_SIZE / 2;

        if (!d->active) {
            if (d->spawn_wave > 0) {
                // Dépôt verrouillé : icône grisée + numéro de vague
                DrawCircle(cx, cy, TILE_SIZE / 2 - 6, (Color){18,14,8,160});
                DrawCircleLines(cx, cy, TILE_SIZE / 2 - 4, (Color){60,50,25,180});
                char _wbuf[8];
                snprintf(_wbuf, sizeof(_wbuf), "V%d", d->spawn_wave);
                int _iw = mtxt(_wbuf, 9);
                dtxt(_wbuf, cx - _iw/2, cy - 5, 9, (Color){90,75,35,200});
            }
            continue;
        }

        Color col = MAT_COLORS[d->type];

        // Halo pulsant
        float pulse = (sinf(t * 2.5f + (float)i) + 1.0f) * 0.5f;
        unsigned char alpha = (unsigned char)(60 + pulse * 80);
        DrawCircle(cx, cy, TILE_SIZE / 2 - 4,
                   (Color){col.r, col.g, col.b, alpha});
        DrawCircleLines(cx, cy, TILE_SIZE / 2 - 2,
                        (Color){col.r, col.g, col.b, 200});

        // Icône centré
        int iw = mtxt(MAT_ICONS[d->type], 11);
        dtxt(MAT_ICONS[d->type], cx - iw/2, cy - 5, 11, col);
    }
}

// ════════════════════════════════════════════════════
// MATÉRIAUX LÂCHÉS AU SOL
// ════════════════════════════════════════════════════
void render_dropped_mats(const DroppedMat *mats, int count) {
    static const Color MAT_COLORS[MAT_COUNT] = {
        [MAT_IRON]  = {160, 160, 180, 255},
        [MAT_ACID]  = { 80, 200,  50, 255},
        [MAT_PLASMA]= { 80, 160, 255, 255},
        [MAT_CRYO]  = {140, 220, 255, 255},
        [MAT_NANO]  = {200, 100, 255, 255},
    };
    static const char *MAT_ICONS[MAT_COUNT] = {
        [MAT_IRON]  = "Fe", [MAT_ACID]  = "Ac", [MAT_PLASMA]= "Pl",
        [MAT_CRYO]  = "Cr", [MAT_NANO]  = "Na",
    };
    float t = (float)GetTime();
    for (int i = 0; i < count; i++) {
        const DroppedMat *dm = &mats[i];
        if (!dm->active || dm->type < 0 || dm->type >= MAT_COUNT) continue;
        Color col = MAT_COLORS[dm->type];
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
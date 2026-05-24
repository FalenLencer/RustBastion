/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

// renderer.c
#include "renderer.h"
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

// ── Une couleur ocre distincte par chemin ─────────────────────
static Color PATH_COLORS[MAX_PATHS] = {
    {239, 159,  39, 160},   // chemin 0 — ocre
    { 39, 159, 239, 160},   // chemin 1 — bleu acier
    {159, 239,  39, 160},   // chemin 2 — vert radioactif
};

// ════════════════════════════════════════════════════
// DÉTAILS PIXEL ART PAR THÈME
// ════════════════════════════════════════════════════
void render_tile_detail(int px, int py, TileType type, ThemeID theme) {
    switch (theme) {
        case THEME_WASTELAND:
            if (type==TILE_RUIN)  { DrawRectangle(px+4,py+4,6,4,(Color){90,65,40,180}); DrawRectangle(px+14,py+10,8,3,(Color){90,65,40,150}); }
            if (type==TILE_GROUND){ DrawRectangle(px+8,py+6,3,2,(Color){55,38,15,200}); DrawRectangle(px+20,py+18,4,2,(Color){55,38,15,180}); }
            if (type==TILE_WATER) { DrawRectangle(px+3,py+12,TILE_SIZE-6,2,(Color){26,90,53,160}); }
            if (type==TILE_PATH)  { for(int i=0;i<3;i++) DrawRectangle(px+4+i*10,py+TILE_SIZE/2-1,7,2,(Color){122,82,0,200}); }
            break;
        case THEME_SWAMP:
            if (type==TILE_RUIN)  { DrawRectangle(px+3,py+3,4,TILE_SIZE-6,(Color){61,92,48,180}); DrawRectangle(px+10,py+5,3,TILE_SIZE-8,(Color){61,92,48,150}); DrawRectangle(px+18,py+2,4,TILE_SIZE-4,(Color){61,92,48,130}); }
            if (type==TILE_WATER) { DrawRectangle(px+2,py+8,TILE_SIZE-4,3,(Color){45,107,45,160}); DrawRectangle(px+4,py+18,TILE_SIZE-8,2,(Color){26,74,26,140}); }
            if (type==TILE_PATH)  { for(int i=0;i<4;i++) DrawRectangle(px+2+i*7,py+TILE_SIZE/2-1,5,2,(Color){61,163,91,200}); }
            break;
        case THEME_DESERT:
            if (type==TILE_RUIN)  { DrawRectangle(px+6,py+8,TILE_SIZE-12,4,(Color){139,94,42,200}); DrawRectangle(px+8,py+4,4,TILE_SIZE-8,(Color){139,94,42,180}); }
            if (type==TILE_GROUND){ DrawRectangle(px+4,py+10,TILE_SIZE-8,1,(Color){107,74,32,160}); DrawRectangle(px+6,py+18,TILE_SIZE-12,1,(Color){107,74,32,140}); }
            if (type==TILE_PATH)  { DrawRectangle(px+4,py+TILE_SIZE/2-2,TILE_SIZE-8,4,(Color){196,150,42,220}); }
            break;
        case THEME_CITY:
            if (type==TILE_RUIN)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){92,92,92,200}); DrawRectangle(px+4,py+4,6,8,(Color){42,42,42,255}); DrawRectangle(px+13,py+4,7,8,(Color){42,42,42,255}); DrawRectangle(px+4,py+15,6,6,(Color){42,42,42,255}); }
            if (type==TILE_GROUND){ DrawLine(px,py+TILE_SIZE/2,px+TILE_SIZE,py+TILE_SIZE/2,(Color){61,61,61,200}); DrawLine(px+TILE_SIZE/2,py,px+TILE_SIZE/2,py+TILE_SIZE,(Color){61,61,61,200}); }
            if (type==TILE_PATH)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){58,58,58,200}); DrawRectangle(px+TILE_SIZE/2-1,py+2,1,8,(Color){255,255,0,180}); DrawRectangle(px+TILE_SIZE/2-1,py+TILE_SIZE-10,1,8,(Color){255,255,0,180}); }
            break;
        case THEME_FACTORY:
            if (type==TILE_RUIN)  { DrawRectangle(px+4,py+4,TILE_SIZE-8,TILE_SIZE-8,(Color){107,58,26,200}); DrawRectangle(px+8,py+8,TILE_SIZE-16,TILE_SIZE-16,(Color){45,26,10,255}); DrawRectangle(px+6,py+TILE_SIZE/2-1,TILE_SIZE-12,2,(Color){139,69,19,200}); }
            if (type==TILE_GROUND){ for(int i=0;i<3;i++) DrawLine(px+i*(TILE_SIZE/3),py,px+i*(TILE_SIZE/3),py+TILE_SIZE,(Color){45,30,26,200}); }
            if (type==TILE_PATH)  { DrawRectangle(px+2,py+2,TILE_SIZE-4,TILE_SIZE-4,(Color){74,42,10,200}); for(int i=0;i<4;i++) DrawRectangle(px+4+i*6,py+TILE_SIZE/2-2,4,4,(Color){139,90,26,220}); }
            break;
        default: break;
    }
}

// ════════════════════════════════════════════════════
// CARTE AVEC CULLING (optimisation)
// ════════════════════════════════════════════════════
void render_map(const Map *map) {
    const Theme *th = theme_get(map->theme);
    const ThemePalette *p = &th->palette;

    // Calculer la zone visible (viewport)
    // La carte fait MAP_W * TILE_SIZE x MAP_H * TILE_SIZE
    // On affiche tout car la vue est fixe, mais on peut optimiser
    // en ne dessinant que les tuiles nécessaires
    
    for (int y = 0; y < map->h; y++) {
        for (int x = 0; x < map->w; x++) {
            TileType t  = map->tiles[y][x].type;
            int      px = x * TILE_SIZE;
            int      py = y * TILE_SIZE;

            Color fill, stroke;
            switch (t) {
                case TILE_GROUND: fill=p->ground_fill; stroke=p->ground_stroke; break;
                case TILE_RUIN:   fill=p->ruin_fill;   stroke=p->ruin_stroke;   break;
                case TILE_WATER:  fill=p->water_fill;  stroke=p->water_stroke;  break;
                case TILE_PATH:   fill=p->path_fill;   stroke=p->path_stroke;   break;
                case TILE_SPAWN:  fill=p->spawn_fill;  stroke=p->spawn_stroke;  break;
                case TILE_BASE:   fill=p->base_fill;   stroke=p->base_stroke;   break;
                default:          fill=p->ground_fill; stroke=p->ground_stroke; break;
            }

            DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, fill);
            DrawRectangleLines(px, py, TILE_SIZE, TILE_SIZE, stroke);
            render_tile_detail(px, py, t, map->theme);
        }
    }
    
    // ── Pulses spawn (rouge) et base (vert) ──────────────────
    float t = (float)GetTime();
    float pulse = (sinf(t * 3.0f) + 1.0f) * 0.5f;  // 0..1, 3 Hz

    for (int y = 0; y < map->h; y++) {
        for (int x = 0; x < map->w; x++) {
            TileType type = map->tiles[y][x].type;
            int px2 = x * TILE_SIZE, py2 = y * TILE_SIZE;

            if (type == TILE_SPAWN) {
                // Halo rouge clignotant + flèche entrée
                int alpha = (int)(80 + pulse * 120);
                DrawRectangle(px2, py2, TILE_SIZE, TILE_SIZE,
                              (Color){231, 76, 60, (unsigned char)alpha});
                // Icône "!" centré
                dtxt("!", px2 + TILE_SIZE/2 - 3, py2 + TILE_SIZE/2 - 7,
                         14, (Color){255, 180, 180, 220});
            }
            else if (type == TILE_BASE) {
                // Halo vert clignotant + étoile
                int alpha = (int)(80 + pulse * 140);
                DrawRectangle(px2, py2, TILE_SIZE, TILE_SIZE,
                              (Color){46, 204, 113, (unsigned char)alpha});
                dtxt("*", px2 + TILE_SIZE/2 - 5, py2 + TILE_SIZE/2 - 8,
                         16, (Color){180, 255, 200, 230});
            }
        }
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
    for (int b = 0; b < map->base_count; b++) {
        const BaseInfo *base = &map->bases[b];
 
        int bpx = base->pos.x * TILE_SIZE;
        int bpy = base->pos.y * TILE_SIZE;
        (void)bpy; // coordonnée y utilisée pour la croix uniquement
 
        // Couleur selon état
        Color bar_col;
        float ratio = (base->max_hp > 0)
                    ? (float)base->hp / (float)base->max_hp : 0.0f;
        if (!base->active || base->hp <= 0) {
            bar_col = (Color){80, 30, 30, 200};  // détruite = rouge foncé
        } else if (base->is_primary) {
            bar_col = ratio > 0.5f ? (Color){46, 204, 113, 255}
                    : ratio > 0.25f ? (Color){243, 156, 18, 255}
                                    : (Color){231, 76, 60, 255};
        } else {
            // Base secondaire = bleu
            bar_col = ratio > 0.5f ? (Color){52, 152, 219, 255}
                    : ratio > 0.25f ? (Color){155, 89, 182, 255}
                                    : (Color){231, 76, 60, 255};
        }
 
        // Les barres HP/labels sont affichées dans le HUD (panneau gauche).
        // Sur la carte on garde uniquement un indicateur visuel minimal.

        // Liseré coloré sur le bord de la tuile selon l'état
        DrawRectangleLinesEx(
            (Rectangle){(float)bpx, (float)bpy, TILE_SIZE, TILE_SIZE},
            2.0f, (Color){bar_col.r, bar_col.g, bar_col.b, 180});

        // Croix si détruite
        if (!base->active || base->hp <= 0) {
            DrawLine(bpx + 4, bpy + 4, bpx + TILE_SIZE - 4, bpy + TILE_SIZE - 4,
                     (Color){180, 40, 40, 200});
            DrawLine(bpx + TILE_SIZE - 4, bpy + 4, bpx + 4, bpy + TILE_SIZE - 4,
                     (Color){180, 40, 40, 200});
        }
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
 
        // Slow — teinte bleue
        if (e->slow_timer > 0.0f)
            col = (Color){100, 180, 255, 255};
 
        // ── Ghost : semi-transparent + scintillement ──────────
        if (e->type == ENEMY_GHOST) {
            float flicker = sinf(t * 8.0f + (float)i) * 0.5f + 0.5f;
            unsigned char alpha = (unsigned char)(60 + (int)(flicker * 80));
            DrawCircle((int)e->x, (int)e->y, (int)e->size,
                       (Color){col.r, col.g, col.b, alpha});
            DrawCircleLines((int)e->x, (int)e->y, (int)e->size,
                            (Color){200, 200, 255,
                                    (unsigned char)(120 + (int)(flicker * 80))});
            DrawCircleLines((int)e->x, (int)e->y, (int)(e->size + 3),
                            (Color){180, 180, 255,
                                    (unsigned char)(40 + (int)(flicker * 40))});
        }
        // ── Pathbreaker : flèche si dévié ────────────────────
        else if (e->type == ENEMY_PATHBREAKER) {
            DrawCircle((int)e->x, (int)e->y, (int)e->size, col);
            DrawCircleLines((int)e->x, (int)e->y, (int)e->size,
                            (Color){255,255,255,60});
            if (e->path_broken) {
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
        }
        // ── Healer : croix blanche + aura de soin verte ──────
        else if (e->type == ENEMY_HEALER) {
            // Aura de soin (rayon)
            DrawCircle((int)e->x, (int)e->y, (int)e->heal_range,
                       (Color){46, 204, 113, 18});
            DrawCircleLines((int)e->x, (int)e->y, (int)e->heal_range,
                            (Color){46, 204, 113, 60});
            // Corps
            DrawCircle((int)e->x, (int)e->y, (int)e->size, col);
            // Croix blanche
            int sz = (int)e->size;
            DrawRectangle((int)e->x - 2, (int)e->y - sz + 2,
                          4, sz*2 - 4, (Color){255,255,255,230});
            DrawRectangle((int)e->x - sz + 2, (int)e->y - 2,
                          sz*2 - 4, 4, (Color){255,255,255,230});
        }
        // ── Hunter : point jaune si traque active ─────────────
        else if (e->type == ENEMY_HUNTER) {
            DrawCircle((int)e->x, (int)e->y, (int)e->size, col);
            DrawCircleLines((int)e->x, (int)e->y, (int)e->size,
                            (Color){255,255,255,60});
            if (e->hunt_target >= 0)
                DrawCircle((int)e->x, (int)e->y, 3,
                           (Color){255, 255, 0, 220});
        }
        // ── Artillery : carré + cercle de portée rouge si actif
        else if (e->type == ENEMY_ARTILLERY) {
            int sz = (int)e->size;
            DrawRectangle((int)e->x - sz, (int)e->y - sz,
                          sz*2, sz*2, col);
            DrawRectangleLines((int)e->x - sz, (int)e->y - sz,
                               sz*2, sz*2, (Color){255,255,255,60});
            if (e->arty_target >= 0)
                DrawCircleLines((int)e->x, (int)e->y,
                                (int)e->arty_range,
                                (Color){231, 76, 60, 80});
        }
        // ── Ennemis normaux ───────────────────────────────────
        else {
            DrawCircle((int)e->x, (int)e->y, (int)e->size, col);
            DrawCircleLines((int)e->x, (int)e->y, (int)e->size,
                            (Color){255,255,255,60});
        }
 
        // ── Barre de vie ──────────────────────────────────────
        if (e->type != ENEMY_GHOST || e->hp < e->max_hp * 0.8f) {
            float ratio = e->hp / e->max_hp;
            int   bw    = (int)(e->size * 2.5f);
            int   bx    = (int)e->x - bw / 2;
            int   by    = (int)e->y - (int)e->size - 6;
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

        DrawRectangle(px+2, py+2, s-4, s-4, (Color){30,20,10,255});
        DrawRectangle(px+6, py+6, s-12, s-12, col);

        float cx = tw->cx, cy = tw->cy;
        float ex = cx + cosf(tw->angle) * (s*0.4f);
        float ey = cy + sinf(tw->angle) * (s*0.4f);
        DrawLineEx((Vector2){cx,cy}, (Vector2){ex,ey}, 3.0f, col);
        DrawRectangleLines(px+6, py+6, s-12, s-12, (Color){255,255,255,60});

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

            // Si porte un matériau : halo coloré
            if (u->has_material && u->carried_mat != MAT_NONE) {
                static const Color MAT_COL[MAT_COUNT] = {
                    {160,160,180,255},{80,220,60,255},
                    {80,160,255,255},{140,220,255,255},{200,100,255,255},
                };
                DrawCircle((int)u->x, (int)u->y, (int)(u->size + 4),
                           (Color){MAT_COL[u->carried_mat].r,
                                   MAT_COL[u->carried_mat].g,
                                   MAT_COL[u->carried_mat].b, 80});
            }

            // Barre de collecte
            if (u->state == USTATE_COLLECT && u->collect_duration > 0.0f) {
                float ratio = 1.0f - (u->collect_timer / u->collect_duration);
                int   bw    = (int)(u->size * 3.0f);
                int   bx    = (int)u->x - bw/2;
                int   by    = (int)u->y - (int)u->size - 10;
                DrawRectangle(bx, by, bw, 4, (Color){20, 20, 20, 200});
                DrawRectangle(bx, by, (int)(bw * ratio), 4,
                              (Color){80, 200, 220, 255});
                dtxt("...", bx, by - 10, 8, (Color){80, 200, 220, 200});
            }
        }

        // ── Médic : aura de soin ──────────────────────────
        if (u->type == UNIT_MEDIC) {
            DrawCircle((int)u->x, (int)u->y, (int)(3.0f*TILE_SIZE),
                       (Color){231,76,60,20});
            DrawCircleLines((int)u->x, (int)u->y, (int)(3.0f*TILE_SIZE),
                            (Color){231,76,60,80});
        }

        // ── Corps ─────────────────────────────────────────
        DrawEllipse((int)u->x+1, (int)u->y+(int)u->size,
                    (int)(u->size*0.8f), (int)(u->size*0.3f),
                    (Color){0,0,0,60});

        // Ouvrier sélectionné = contour blanc plus épais
        if (up->selected_unit == i)
            DrawCircle((int)u->x, (int)u->y, (int)(u->size + 2),
                       (Color){255,255,255,100});

        DrawCircle((int)u->x, (int)u->y, (int)u->size, col);
        DrawCircleLines((int)u->x, (int)u->y, (int)u->size,
                        (Color){255,255,255,80});

        // ── Icône état ────────────────────────────────────
        const char *icon;
        switch (u->state) {
            case USTATE_ATTACK:       icon = "X"; break;
            case USTATE_CHASE:        icon = ">"; break;
            case USTATE_RETURN:       icon = "<"; break;
            case USTATE_HEAL:         icon = "+"; break;
            case USTATE_GOTO_DEPOSIT: icon = "D"; break;
            case USTATE_COLLECT:      icon = "C"; break;
            case USTATE_GOTO_BASE:    icon = "B"; break;
            default:                  icon = "."; break;
        }
        dtxt(icon, (int)u->x-3, (int)u->y-4, 8, (Color){255,255,255,200});

        // ── Barre de vie ──────────────────────────────────
        int bw = (int)(u->size*2.5f);
        int bx = (int)u->x - bw/2;
        int by = (int)u->y - (int)u->size - 6;
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
        if (!d->active) continue;

        int px = d->tile_x * TILE_SIZE;
        int py = d->tile_y * TILE_SIZE;
        int cx = px + TILE_SIZE / 2;
        int cy = py + TILE_SIZE / 2;

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
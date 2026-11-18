/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  renderer_towers.c ─ Rendu 2D des tours, projectiles et aperçu de pose.
 *
 *  Extrait de renderer.c (règle projet : fichiers < 500 lignes). Les
 *  projectiles, le socle et l'animation de pose : renderer_tower_fx.c.
 */

#include "renderer.h"
#include "tile_art.h"   // pixel-art procédural : routes, spawns, bunkers
#include "render3d.h"   // rendu 3D des tours (textures pré-rendues)
#include "render3d_units.h"   // rendu 3D des unités (mode de vue 3D)
#include "render3d_enemies.h" // rendu 3D des ennemis (mode de vue 3D)
#include "../engine/assets.h"  // splash arts tours/unités
#include "terrain.h"   // socle peint (aperçu de pose)
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

// ════════════════════════════════════════════════════
// TOURS ET PROJECTILES
// ════════════════════════════════════════════════════

// Arcs électriques de la Tesla : éclairs jagged qui crépitent autour de l'orbe
// (plus nombreux/longs quand la tour tire). Dessinés en 2D par-dessus le sprite.
static float tesla_hash(unsigned x) {
    x = (x ^ 61u) ^ (x >> 16); x *= 9u; x ^= x >> 4; x *= 0x27d4eb2du; x ^= x >> 15;
    return (float)(x & 0xffffu) / 65535.0f;
}
static void draw_tesla_arcs(float ox, float oy, float tile, int idx, float phase) {
    // Éclairs SEULEMENT au tir : phase = fire_timer*fire_rate ≈ 1 juste après le
    // tir puis décroît. On n'affiche rien hors de la fenêtre de décharge.
    if (phase < 0.70f) return;
    float inten = (phase - 0.70f) / 0.30f;            // 1 au tir → 0 en fin de fenêtre
    if (inten > 1.0f) inten = 1.0f;
    int           nb = 6 + (int)(inten * 4.0f);
    unsigned      tb = (unsigned)(GetTime() * 30.0);  // scintillement rapide
    unsigned char ha = (unsigned char)( 40.0f + 150.0f*inten);   // alpha halo
    unsigned char ca = (unsigned char)( 80.0f + 175.0f*inten);   // alpha cœur
    for (int b = 0; b < nb; b++) {
        unsigned seed = (unsigned)idx*131u + (unsigned)b*17u + tb*7u;
        float ang = tesla_hash(seed) * 6.2832f;
        float len = (0.50f + 0.55f*tesla_hash(seed+1u)) * tile * (0.85f + 0.25f*inten);
        int   segs = 4;
        Vector2 prev = {ox, oy};
        for (int sx = 1; sx <= segs; sx++) {
            float t  = (float)sx / segs;
            float px = ox + cosf(ang)*len*t;
            float py = oy + sinf(ang)*len*t;
            float j  = (tesla_hash(seed + (unsigned)sx*13u)*2.0f - 1.0f) * len*0.16f*(1.2f - t);
            px += cosf(ang + 1.5708f)*j;
            py += sinf(ang + 1.5708f)*j;
            Vector2 cur = {px, py};
            DrawLineEx(prev, cur, 3.0f, (Color){ 90, 150, 255, ha});   // halo bleu
            DrawLineEx(prev, cur, 1.3f, (Color){200, 230, 255, ca});   // cœur clair
            prev = cur;
        }
    }
}

/* Recul + éclair de bouche (vus depuis la carte, par-dessus le modèle 3D). */
#define TWR_KICK_PX    1.6f    // la tour tressaille à l'opposé du tir (px)
#define TWR_FLASH_MIN  0.35f   // fenêtre de l'éclair 2D (part de fire_k)
#define TWR_PREVIEW_PAD_A 0.45f // opacité du socle fantôme (aperçu de pose)

/* Éclair de bouche 2D (mitrailleuse/sniper), lueur du lance-flammes, arcs
   de la Tesla autour de son orbe — tous calés sur la VRAIE bouche 3D. */
static void draw_muzzle_fx(const Tower *tw, int i, Rectangle dst, float fk) {
    Vector2 m;
    if (!render3d_tower_muzzle(i, dst, 0, &m)) return;
    if (tw->type == TOWER_TESLA) {
        float phase = (tw->fire_rate > 0.0f) ? tw->fire_timer * tw->fire_rate : 0.0f;
        draw_tesla_arcs(m.x, m.y, (float)TILE_SIZE, i, phase);
        return;
    }
    if (fk <= TWR_FLASH_MIN) return;
    float k = (fk - TWR_FLASH_MIN) / (1.0f - TWR_FLASH_MIN);
    if (tw->type == TOWER_FLAME) {
        DrawCircleV(m, 4.0f + 5.0f * k, (Color){255, 140, 40, (unsigned char)(90.0f * k)});
        return;
    }
    float ca = cosf(tw->angle), sa = sinf(tw->angle);
    float big = (tw->type == TOWER_SNIPER) ? 1.5f : 1.0f;
    DrawCircleV(m, (3.0f + 6.0f * k) * big, (Color){255, 200, 110, (unsigned char)(120.0f * k)});
    DrawLineEx(m, (Vector2){m.x + ca * (4.0f + 9.0f * k) * big, m.y + sa * (4.0f + 9.0f * k) * big},
               2.0f, (Color){255, 236, 170, (unsigned char)(230.0f * k)});
    for (int s = -1; s <= 1; s += 2)
        DrawLineEx(m, (Vector2){m.x - sa * s * (2.0f + 4.0f * k), m.y + ca * s * (2.0f + 4.0f * k)},
                   1.4f, (Color){255, 220, 150, (unsigned char)(200.0f * k)});
    DrawCircleV(m, (1.2f + 2.2f * k) * big, (Color){255, 252, 230, (unsigned char)(255.0f * k)});
}

void render_towers(const TowerPool *tp) {
    /* Tri par profondeur (cy croissant) : les tours à l'AVANT (cy grand) sont
       dessinées EN DERNIER, donc au-dessus. Indispensable depuis que le Sniper
       a un sprite 3D HAUT qui dépasse au-dessus de sa case : sans ce tri, une
       tour située derrière mais dessinée après masquerait une tour de devant. */
    int order[MAX_TOWERS], nord = 0;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (tp->towers[i].active) order[nord++] = i;
    for (int a = 1; a < nord; a++) {           /* insertion sort (nord petit) */
        int   key = order[a];
        float ky  = tp->towers[key].cy;
        int   b   = a - 1;
        while (b >= 0 && tp->towers[order[b]].cy > ky) { order[b+1] = order[b]; b--; }
        order[b+1] = key;
    }

    for (int o = 0; o < nord; o++) {
        int i = order[o];
        const Tower *tw = &tp->towers[i];

        int px = tw->tile_x * TILE_SIZE;
        int py = tw->tile_y * TILE_SIZE;
        int s  = TILE_SIZE;
        Color col = TOWER_FILL[tw->type];

        float cx = tw->cx, cy = tw->cy;

        /* Pose en cours : chute + écrasement autour du pied (renderer_tower_fx). */
        float ddy, dsx, dsy, dal;
        tower_fx_drop(i, &ddy, &dsx, &dsy, &dal);
        Color tint = {255, 255, 255, (unsigned char)(255.0f * dal)};

        // Corps : modèle 3D live si disponible (tourelle orientée vers la cible),
        // sinon splash art (repli rect coloré), posés sur le socle peint.
        Texture2D t3d   = render3d_tower_tex(i);
        int       drew3d = 0;
        if (t3d.id != 0) {
            // Destination : pied du modèle sur le centre de la case (render3d).
            Rectangle dst = render3d_tower_dst(i, cx, cy, (float)s);
            float fk = render3d_tower_fire_k(i);
            if (tw->type == TOWER_GUN || tw->type == TOWER_SNIPER) {
                dst.x -= cosf(tw->angle) * TWR_KICK_PX * fk;   /* recul */
                dst.y -= sinf(tw->angle) * TWR_KICK_PX * fk;
            }
            render3d_tower_blit(i, renderer_squash_rect(dst, cx, cy, ddy, dsx, dsy), tint);
            drew3d = 1;
            if (dal >= 1.0f && ddy == 0.0f) draw_muzzle_fx(tw, i, dst, fk);
        } else {
            Rectangle box = renderer_squash_rect((Rectangle){cx - (s-8)*0.5f, cy - (s-8)*0.5f,
                                                          (float)(s-8), (float)(s-8)},
                                              cx, cy + (s-8)*0.5f, ddy, dsx, dsy);
            if (!renderer_sprite_fit(g_tower_splash[tw->type], box.x + box.width*0.5f,
                                     box.y + box.height*0.5f, box.width, box.height, tint)) {
                DrawRectangleRec(box, col);
            }
        }

        if (!drew3d) {
            // Canon 2D : pivote vers la cible (le modèle 3D a déjà ses canons).
            float ex = cx + cosf(tw->angle) * (s*0.4f);
            float ey = cy + sinf(tw->angle) * (s*0.4f);
            DrawLineEx((Vector2){cx,cy}, (Vector2){ex,ey}, 3.0f, col);
            DrawCircle((int)ex, (int)ey, 2.5f, col);
        }

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

        // Étourdie par une onde EMP de boss : halo bleu pulsé + marqueur
        if (tw->stun_timer > 0.0f) {
            float sp = (sinf((float)GetTime() * 12.0f) + 1.0f) * 0.5f;
            DrawRectangle(px+2, py+2, s-4, s-4,
                          (Color){80, 150, 255, (unsigned char)(35 + sp * 45)});
            DrawCircleLines((int)cx, (int)cy, s * 0.4f, (Color){130, 200, 255, 210});
            dtxt("Z", (int)cx - 3, py + 2, 9, (Color){160, 215, 255, 235});
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

    /* Socle fantôme : montre où et comment la tour s'ancrera au décor */
    terrain_draw_tower_pad(map->theme, px + TILE_SIZE * 0.5f, py + TILE_SIZE * 0.5f, 1.0f,
                           TWR_PREVIEW_PAD_A, 0u);
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

    /* Qualité de l'emplacement : ce qu'un débutant ignore (une tour loin du
       chemin ne tire jamais). Juste au-dessus du coût, ou dessous si le coût
       a été rabattu sous la tuile (bord haut de la carte). */
    int cover = tower_path_coverage(map, tile_x, tile_y, st->range);
    const char *cover_label;
    Color cover_col;
    if (cover == 0) {
        cover_label = "Trop loin du chemin : ne tirera pas";
        cover_col   = (Color){231, 76, 60, 255};
    } else if (cover < TOWER_COVER_GOOD) {
        cover_label = TextFormat("Couvre %d cases de chemin", cover);
        cover_col   = (Color){243, 156, 18, 255};
    } else {
        cover_label = TextFormat("Bon emplacement : %d cases de chemin", cover);
        cover_col   = (Color){46, 204, 113, 255};
    }
    int cw  = mtxt(cover_label, 9);
    int cx2 = px + TILE_SIZE/2 - cw/2;
    if (cx2 < 2) cx2 = 2;
    if (cx2 + cw > g_canvas_virt_w_base - 2) cx2 = g_canvas_virt_w_base - cw - 2;
    int cy2 = (ly < py) ? ly - fh(9) - 3 : ly + fh(10) + 2;
    if (cy2 < 2) cy2 = ly + fh(10) + 2;
    DrawRectangle(cx2 - 3, cy2 - 2, cw + 6, fh(9) + 2, (Color){0, 0, 0, 160});
    dtxt(cover_label, cx2, cy2, 9, cover_col);
}

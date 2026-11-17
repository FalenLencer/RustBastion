/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  renderer_towers.c ─ Rendu 2D des tours, projectiles et aperçu de pose.
 *
 *  Extrait de renderer.c (règle projet : fichiers < 500 lignes) ; fonctions
 *  déplacées à l'identique.
 */

#include "renderer.h"
#include "tile_art.h"   // pixel-art procédural : routes, spawns, bunkers
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

        // Corps : modèle 3D live si disponible (tourelle orientée vers la cible),
        // sinon socle sombre + splash art (repli rect coloré).
        Texture2D t3d   = render3d_tower_tex(i);
        int       drew3d = 0;
        if (t3d.id != 0) {
            // RenderTexture raylib : verticalement retournée → hauteur source négative.
            // Destination calculée par render3d (carré pour la Mitrailleuse, HAUT
            // ancré par le bas pour le Sniper qui dépasse au-dessus de la case).
            Rectangle dst = render3d_tower_dst(i, cx, cy, (float)s);
            DrawTexturePro(t3d,
                (Rectangle){0, 0, (float)t3d.width, -(float)t3d.height},
                dst,
                (Vector2){0, 0}, 0.0f, WHITE);
            drew3d = 1;
            // Tesla : éclairs électriques qui crépitent autour de l'orbe.
            if (tw->type == TOWER_TESLA) {
                float ox    = dst.x + dst.width  * 0.5f;
                float oy    = dst.y + dst.height * 0.27f;   // ~position de l'orbe
                float phase = (tw->fire_rate > 0.0f) ? tw->fire_timer * tw->fire_rate : 0.0f;
                draw_tesla_arcs(ox, oy, (float)s, i, phase);
            }
        } else {
            DrawRectangle(px+2, py+2, s-4, s-4, (Color){30,20,10,255});
            if (!renderer_sprite_fit(g_tower_splash[tw->type], cx, cy,
                                 (float)(s-8), (float)(s-8), WHITE)) {
                DrawRectangle(px+6, py+6, s-12, s-12, col);
            }
        }

        if (!drew3d) {
            // Canon 2D : pivote vers la cible (le modèle 3D a déjà ses canons).
            float ex = cx + cosf(tw->angle) * (s*0.4f);
            float ey = cy + sinf(tw->angle) * (s*0.4f);
            DrawLineEx((Vector2){cx,cy}, (Vector2){ex,ey}, 3.0f, col);
            DrawCircle((int)ex, (int)ey, 2.5f, col);
            DrawRectangleLines(px+6, py+6, s-12, s-12, (Color){255,255,255,40});
        }

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

void render_projectiles(const TowerPool *tp) {
    /* Dimensionné par DAMAGE_TYPE_COUNT : DMG_FIRE (= 5, ajouté après coup)
       lisait hors tableau à chaque projectile de lance-flammes. */
    static const Color DMG_COLORS[DAMAGE_TYPE_COUNT] = {
        [DMG_PHYSICAL] = {255, 200, 100, 255},
        [DMG_POISON]   = { 80, 220,  60, 255},
        [DMG_ELECTRIC] = {180, 100, 255, 255},
        [DMG_CRYO]     = {140, 220, 255, 255},
        [DMG_NANO]     = {220, 100, 220, 255},
        [DMG_FIRE]     = {255, 107,  53, 255},   /* = PROJ_COLOR[TOWER_FLAME] */
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

        // ── Traçante lumineuse : traînée orientée vers la cible +
        //    halo coloré + cœur brillant (animation de balle).
        float dx = p->tx - p->x, dy = p->ty - p->y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.001f) len = 0.001f;
        float ux = dx/len, uy = dy/len;
        Vector2 head = { p->x, p->y };
        Vector2 tail = { p->x - ux*16.0f, p->y - uy*16.0f };

        DrawLineEx(tail, head, 2.4f, (Color){col.r, col.g, col.b, 110});
        if (p->splash) {
            DrawCircleV(head, 8.0f, (Color){col.r, col.g, col.b, 90});
            DrawCircleV(head, 5.0f, col);
            DrawCircleV(head, 2.5f, (Color){255, 255, 235, 255});
        } else {
            DrawCircleV(head, 6.0f, (Color){col.r, col.g, col.b, 90});
            DrawCircleV(head, 3.2f, col);
            DrawCircleV(head, 1.6f, (Color){255, 255, 235, 255});
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

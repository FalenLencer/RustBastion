/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  tile_art_decor.c ─ décor pixel-art posé SUR les tuiles : bunkers de
 *  base, filons de minerai, roche minée, franges de transition entre
 *  terrains et ombres portées. Extrait de tile_art.c (règle < 500 l.).
 */
#include "tile_art.h"
#include "tile_art_internal.h"
#include "../combat/tower.h"   // TowerPool (ombres portées des tours)
#include <math.h>

// ── Réglages ─────────────────────────────────────────────────
#define TA_SHORE_SEG    5    // largeur d'un cran de rive (px)
#define TA_SHADOW_LEN   5    // portée de l'ombre SE (px)
#define TA_SHADOW_A     52   // opacité de l'ombre portée

// ════════════════════════════════════════════════════
// TRANSITIONS ENTRE TERRAINS — franges sur les bords
// Rive dentelée quand une tuile de terre touche l'EAU,
// éboulis discrets le long d'une RUINE voisine.
// ════════════════════════════════════════════════════
static TileType tile_type_at(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->w || y >= map->h) return TILE_GROUND;
    return map->tiles[y][x].type;
}

// Rive : l'eau « lèche » le bord de la tuile de terre par crans
// de profondeur variable (hash), sur fond de terre mouillée.
static void shore_edge(int px, int py, int edge, unsigned int h,
                       Color wat, Color wet) {
    for (int s = 0; s < TILE_SIZE / TA_SHORE_SEG; s++) {
        int d = 2 + (int)((h >> ((s + edge * 3) & 15)) & 3);   // 2..5 px
        int o = s * TA_SHORE_SEG;
        switch (edge) {
            case 0:   // nord
                DrawRectangle(px + o, py, TA_SHORE_SEG, d,
                              (Color){wet.r, wet.g, wet.b, 160});
                DrawRectangle(px + o, py, TA_SHORE_SEG, d - 1,
                              (Color){wat.r, wat.g, wat.b, 130});
                break;
            case 1:   // sud
                DrawRectangle(px + o, py + TILE_SIZE - d, TA_SHORE_SEG, d,
                              (Color){wet.r, wet.g, wet.b, 160});
                DrawRectangle(px + o, py + TILE_SIZE - d + 1, TA_SHORE_SEG,
                              d - 1, (Color){wat.r, wat.g, wat.b, 130});
                break;
            case 2:   // ouest
                DrawRectangle(px, py + o, d, TA_SHORE_SEG,
                              (Color){wet.r, wet.g, wet.b, 160});
                DrawRectangle(px, py + o, d - 1, TA_SHORE_SEG,
                              (Color){wat.r, wat.g, wat.b, 130});
                break;
            default:  // est
                DrawRectangle(px + TILE_SIZE - d, py + o, d, TA_SHORE_SEG,
                              (Color){wet.r, wet.g, wet.b, 160});
                DrawRectangle(px + TILE_SIZE - d + 1, py + o, d - 1,
                              TA_SHORE_SEG, (Color){wat.r, wat.g, wat.b, 130});
                break;
        }
    }
}

// Éboulis : quelques éclats sombres tombés de la ruine voisine.
static void rubble_edge(int px, int py, int edge, unsigned int h, Color dark) {
    for (int k = 0; k < 3; k++) {
        int o = 4 + (int)((h >> ((k * 4 + edge * 5) & 15)) % (TILE_SIZE - 10));
        int gx = px, gy = py;
        switch (edge) {
            case 0:  gx += o; gy += 1;              break;   // nord
            case 1:  gx += o; gy += TILE_SIZE - 3;  break;   // sud
            case 2:  gx += 1; gy += o;              break;   // ouest
            default: gx += TILE_SIZE - 3; gy += o;  break;   // est
        }
        DrawRectangle(gx, gy, 2, 2, (Color){dark.r, dark.g, dark.b,
                                            (unsigned char)(150 - k * 30)});
    }
}

void tile_art_draw_fringes(const Map *map, int tx, int ty) {
    const Theme *T = theme_get(map->theme);
    Color wat  = T->palette.water_fill;
    Color wet  = shade(T->palette.ground_fill, -34);
    Color dark = shade(T->palette.ruin_fill, -30);
    int px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    unsigned int h  = tile_hash(tx, ty);
    int self_ruin   = (map->tiles[ty][tx].type == TILE_RUIN);
    static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
    for (int e = 0; e < 4; e++) {
        TileType nb = tile_type_at(map, tx + DX[e], ty + DY[e]);
        if (nb == TILE_WATER)
            shore_edge(px, py, e, h, wat, wet);
        else if (nb == TILE_RUIN && !self_ruin)
            rubble_edge(px, py, e, h, dark);
    }
}

// ════════════════════════════════════════════════════
// OMBRES PORTÉES COURTES (lumière NO → ombre au SUD-EST)
// Ruines/gratte-ciels + tours. À dessiner APRÈS les routes
// (l'ombre tombe dessus) et AVANT bases/minerais/tours.
// ════════════════════════════════════════════════════
static void shadow_se(int px, int py, int e_ok, int s_ok) {
    Color sh = (Color){0, 0, 0, TA_SHADOW_A};
    if (e_ok)
        DrawRectangle(px + TILE_SIZE, py + TA_SHADOW_LEN,
                      TA_SHADOW_LEN, TILE_SIZE - TA_SHADOW_LEN, sh);
    if (s_ok)
        DrawRectangle(px + TA_SHADOW_LEN, py + TILE_SIZE,
                      TILE_SIZE - TA_SHADOW_LEN, TA_SHADOW_LEN, sh);
    if (e_ok && s_ok)
        DrawRectangle(px + TILE_SIZE, py + TILE_SIZE,
                      TA_SHADOW_LEN, TA_SHADOW_LEN, sh);
}

void tile_art_draw_shadows(const Map *map, const struct TowerPool *tp) {
    for (int y = 0; y < map->h; y++)
        for (int x = 0; x < map->w; x++)
            if (map->tiles[y][x].type == TILE_RUIN)
                shadow_se(x * TILE_SIZE, y * TILE_SIZE,
                          x + 1 < map->w, y + 1 < map->h);
    if (tp) {
        for (int i = 0; i < MAX_TOWERS; i++) {
            const Tower *tw = &tp->towers[i];
            if (!tw->active) continue;
            shadow_se(tw->tile_x * TILE_SIZE, tw->tile_y * TILE_SIZE,
                      tw->tile_x + 1 < map->w, tw->tile_y + 1 < map->h);
        }
    }
}

// ════════════════════════════════════════════════════
// BASE — bunker fortifié
// ════════════════════════════════════════════════════
void tile_art_draw_base(int px, int py, ThemeID theme,
                        Color accent, int destroyed)
{
    (void)theme;
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    const int m  = 3;   // marge

    // Murs (pierre/béton)
    Color wall      = destroyed ? (Color){48, 40, 36, 255} : (Color){86, 80, 70, 255};
    Color wall_dark = destroyed ? (Color){28, 22, 20, 255} : (Color){54, 50, 44, 255};

    DrawRectangle(px + m, py + m, TILE_SIZE - 2*m, TILE_SIZE - 2*m, wall_dark);
    DrawRectangle(px + m + 2, py + m + 2,
                  TILE_SIZE - 2*m - 4, TILE_SIZE - 2*m - 4, wall);

    // Créneaux (haut)
    for (int i = 0; i < 4; i++) {
        int bx = px + m + 1 + i * ((TILE_SIZE - 2*m - 2) / 4);
        DrawRectangle(bx, py + m - 2, (TILE_SIZE - 2*m - 2) / 4 - 2, 4, wall_dark);
    }

    if (!destroyed) {
        // Cœur d'énergie central (couleur d'équipe, pulsé)
        float t  = (float)GetTime();
        float pu = (sinf(t * 3.0f) + 1.0f) * 0.5f;
        int   r  = (int)(6 + pu * 2);
        DrawCircle(cx, cy, (float)(r + 3),
                   (Color){accent.r, accent.g, accent.b, (unsigned char)(40 + (int)(pu*60))});
        DrawCircle(cx, cy, (float)r, accent);
        DrawCircleLines(cx, cy, (float)r, (Color){255, 255, 255, 120});

        // Liseré d'équipe sur le mur
        DrawRectangleLinesEx(
            (Rectangle){(float)(px + m), (float)(py + m),
                        (float)(TILE_SIZE - 2*m), (float)(TILE_SIZE - 2*m)},
            2.0f, (Color){accent.r, accent.g, accent.b, 200});
    } else {
        // Ruine : décombres + croix
        DrawRectangle(cx - 6, cy - 2, 5, 5, wall_dark);
        DrawRectangle(cx + 2, cy + 1, 4, 4, wall_dark);
        DrawLine(px + 6, py + 6, px + TILE_SIZE - 6, py + TILE_SIZE - 6,
                 (Color){190, 50, 50, 220});
        DrawLine(px + TILE_SIZE - 6, py + 6, px + 6, py + TILE_SIZE - 6,
                 (Color){190, 50, 50, 220});
    }
}

// ════════════════════════════════════════════════════
// MINERAI — filon de cristaux (remplace la tuile) + roche minée
// ════════════════════════════════════════════════════
typedef struct { Color core, glow; } OrePalette;

// Palette par type de matériau (MaterialType : 0=fer … 4=nano).
static OrePalette ore_palette(int mat) {
    switch (mat) {
        case 0:  return (OrePalette){{182, 188, 202, 255}, {150, 160, 185, 255}}; // FER  (acier)
        case 1:  return (OrePalette){{122, 214,  72, 255}, { 90, 200,  50, 255}}; // ACIDE
        case 2:  return (OrePalette){{ 98, 170, 255, 255}, { 70, 150, 255, 255}}; // PLASMA
        case 3:  return (OrePalette){{158, 228, 255, 255}, {120, 210, 255, 255}}; // CRYO
        case 4:  return (OrePalette){{206, 116, 255, 255}, {180,  90, 255, 255}}; // NANO
        default: return (OrePalette){{200, 200, 210, 255}, {170, 170, 185, 255}};
    }
}

// Un cristal hexagonal facetté (volume par 3 polygones + arêtes + étincelle).
static void draw_crystal(float cx, float cy, float rad, float rot, Color c) {
    Color lite = shade(c,  55);
    Color dark = shade(c, -55);
    Color edge = shade(c,  95);
    DrawPoly((Vector2){cx, cy}, 6, rad,         rot, dark);   // base ombrée
    DrawPoly((Vector2){cx, cy}, 6, rad * 0.82f, rot, c);      // corps
    DrawPoly((Vector2){cx - rad*0.18f, cy - rad*0.18f},
             6, rad * 0.40f, rot, lite);                       // facette éclairée
    DrawPolyLines((Vector2){cx, cy}, 6, rad, rot,
                  (Color){edge.r, edge.g, edge.b, 200});       // arêtes
    DrawRectangle((int)(cx - rad*0.30f), (int)(cy - rad*0.42f),
                  2, 2, (Color){255, 255, 255, 210});          // étincelle
}

void tile_art_draw_deposit(int px, int py, int mat_type, float t) {
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    OrePalette op = ore_palette(mat_type);

    // 1) Matrice rocheuse minéralisée — opaque : remplace la tuile.
    Color matrix_d = {28, 25, 34, 255};
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){44, 39, 50, 255});
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE / 2, (Color){255, 255, 255, 8});
    DrawRectangle(px, py + TILE_SIZE / 2, TILE_SIZE, TILE_SIZE / 2, (Color){0, 0, 0, 30});
    DrawLine(px + 5, py + TILE_SIZE - 7, px + 15, py + 10, matrix_d);    // veines sombres
    DrawLine(px + TILE_SIZE - 6, py + TILE_SIZE - 9, px + 24, py + 12, matrix_d);
    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0, 0, 0, 60});  // bord
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0, 0, 0, 60});

    // 2) Halo lumineux pulsant (couleur du minerai).
    float pulse = (sinf(t * 2.2f + (float)(px * 3 + py) * 0.05f) + 1.0f) * 0.5f;
    unsigned char ga = (unsigned char)(38 + pulse * 70);
    DrawCircle(cx, cy, TILE_SIZE * 0.42f, (Color){op.glow.r, op.glow.g, op.glow.b, ga});

    // 3) Amas de cristaux (le filon proprement dit).
    draw_crystal((float)cx - 9, (float)cy + 6, 6.0f, -8.0f,             op.core);
    draw_crystal((float)cx + 9, (float)cy + 5, 6.5f, 18.0f,             op.core);
    draw_crystal((float)cx - 3, (float)cy - 8, 5.0f,  4.0f,             op.core);
    draw_crystal((float)cx + 6, (float)cy - 5, 4.0f, 24.0f,             op.core);
    draw_crystal((float)cx,     (float)cy + 3, 9.0f, 10.0f + pulse*4.0f, op.core);
}

void tile_art_draw_mined_rock(int px, int py) {
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    unsigned int h = tile_hash(px / TILE_SIZE, py / TILE_SIZE);

    // Roche épuisée, terne — laissée après extraction du filon.
    Color rock_d = {36, 32, 29, 255};
    Color rock_l = {78, 72, 66, 255};
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){58, 53, 49, 255});
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE / 2, (Color){255, 255, 255, 6});
    DrawRectangle(px, py + TILE_SIZE / 2, TILE_SIZE, TILE_SIZE / 2, (Color){0, 0, 0, 34});

    // Cratère creusé au centre → signale une tuile difficile à bâtir.
    DrawCircle(cx, cy + 1, TILE_SIZE * 0.30f, rock_d);
    DrawCircle(cx, cy + 2, TILE_SIZE * 0.20f, (Color){20, 17, 15, 255});
    DrawCircleLines(cx, cy + 1, TILE_SIZE * 0.30f, (Color){18, 15, 13, 220});

    // Éboulis + fissures.
    DrawRectangle(px + 5, py + 6, 4, 3, rock_l);
    DrawRectangle(px + TILE_SIZE - 9, py + 8, 4, 3, rock_d);
    DrawRectangle(px + 7, py + TILE_SIZE - 9, 3, 3, rock_d);
    DrawRectangle(px + TILE_SIZE - 10, py + TILE_SIZE - 8, 4, 3, rock_l);
    DrawLine(cx, cy, px + 6, py + 7, (Color){22, 19, 17, 200});
    DrawLine(cx, cy, px + TILE_SIZE - 7, py + 9, (Color){22, 19, 17, 180});
    speck(px, py, h, 2, 2, rock_l);

    // Quelques résidus de minerai ternes au fond du cratère.
    DrawRectangle(cx - 2, cy - 1, 2, 2, (Color){120, 116, 110, 200});
    DrawRectangle(cx + 3, cy + 2, 1, 1, (Color){120, 116, 110, 160});

    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0, 0, 0, 60});
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0, 0, 0, 60});
}

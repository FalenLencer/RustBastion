/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tile_art.h"
#include "tile_art_internal.h"  // tile_hash / shade / speck (partagés decor)
#include <math.h>
#include <stdlib.h>     // abs

// Horloge interne (eau animée) : accumulée via GetFrameTime par
// tile_art_tick — jamais GetTime pour la logique (convention projet).
static float g_ta_clock = 0.0f;
void tile_art_tick(float dt) { g_ta_clock += dt; }

// ════════════════════════════════════════════════════
// PALETTE ROUTE PAR THÈME
// road      : remplissage principal de la chaussée
// road_dark : liseré / ombre sous la route
// road_lite : marquage central / reflets
// ════════════════════════════════════════════════════
typedef struct { Color road, road_dark, road_lite; } RoadPalette;

static RoadPalette road_palette(ThemeID theme) {
    switch (theme) {
        case THEME_SWAMP:   return (RoadPalette){
            {64, 74, 48, 255}, {38, 46, 28, 255}, {96, 112, 72, 255}};
        case THEME_DESERT:  return (RoadPalette){
            {158, 126, 70, 255}, {112, 86, 44, 255}, {200, 170, 104, 255}};
        case THEME_CITY:    return (RoadPalette){
            {66, 66, 72, 255}, {38, 38, 44, 255}, {120, 120, 128, 255}};
        case THEME_FACTORY: return (RoadPalette){
            {84, 62, 40, 255}, {50, 36, 22, 255}, {150, 110, 60, 255}};
        case THEME_WASTELAND:
        default:            return (RoadPalette){
            {96, 70, 42, 255}, {58, 42, 24, 255}, {150, 116, 64, 255}};
    }
}

// Teste si une tuile fait partie du réseau de route (chemin/spawn/base).
static int is_road(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->w || y >= map->h) return 0;
    TileType t = map->tiles[y][x].type;
    return (t == TILE_PATH || t == TILE_SPAWN || t == TILE_BASE);
}

// ════════════════════════════════════════════════════
// FOND DE TUILE — base + relief subtil + séparation douce
// ════════════════════════════════════════════════════
static void draw_base_fill(int px, int py, Color fill) {
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, fill);
    // Dégradé vertical léger : haut éclairé, bas ombré (volume)
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE/2, (Color){255,255,255,8});
    DrawRectangle(px, py + TILE_SIZE/2, TILE_SIZE, TILE_SIZE/2, (Color){0,0,0,20});
    // Séparation de tuile discrète (bord bas + droit)
    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0,0,0,45});
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0,0,0,45});
}

// ════════════════════════════════════════════════════
// SOL (buildable)
// ════════════════════════════════════════════════════
static void draw_ground(int px, int py, ThemeID theme, unsigned int h) {
    const Theme *T = theme_get(theme);
    Color base = T->palette.ground_fill;
    draw_base_fill(px, py, base);
    Color lite = shade(base, 28), dark = shade(base, -20);
    int cx  = px + TILE_SIZE/2;
    int var = (int)((h >> 8) % 3u);   // 3 variantes de détail par tuile

    switch (theme) {
        case THEME_DESERT: {  // sable : dunes ondulées + grains + galet
            int rows = (var == 1) ? 2 : 3;         // densité de dunes
            for (int i = 0; i < rows; i++) {
                int yy  = py + 8 + var * 2 + i*10;
                int amp = 2 + (int)((h >> (i*3)) & 3);
                DrawRectangle(px + 3, yy,         TILE_SIZE/2 - 3, 1,
                              (Color){lite.r, lite.g, lite.b, 130});
                DrawRectangle(cx,     yy + amp-1, TILE_SIZE/2 - 3, 1,
                              (Color){lite.r, lite.g, lite.b, 100});
            }
            speck(px, py, h, 0, 1, (Color){lite.r, lite.g, lite.b, 210});
            if (var != 2) {
                speck(px, py, h, 3, 2, (Color){dark.r, dark.g, dark.b, 150});  // galet
            } else {          // dalle enfouie qui affleure du sable
                DrawRectangle(px + 22, py + 24, 11, 6, shade(base, -12));
                DrawRectangle(px + 22, py + 24, 11, 1,
                              (Color){lite.r, lite.g, lite.b, 120});
            }
        } break;
        case THEME_CITY: {    // bitume : joints + fissure/égout + marquage
            DrawRectangle(px + 2, py + TILE_SIZE/2, TILE_SIZE - 4, 1,
                          (Color){dark.r, dark.g, dark.b, 150});
            if (var == 0) {              // fissure qui court vers le SE
                DrawLine(px + 7, py + 6, px + 15, py + 20, (Color){dark.r, dark.g, dark.b, 200});
                DrawLine(px + 15, py + 20, px + 28, py + 27, (Color){dark.r, dark.g, dark.b, 150});
            } else if (var == 1) {       // fissure miroir (vers le SO)
                DrawLine(px + 30, py + 6, px + 22, py + 19, (Color){dark.r, dark.g, dark.b, 200});
                DrawLine(px + 22, py + 19, px + 9,  py + 26, (Color){dark.r, dark.g, dark.b, 150});
            } else {                     // plaque d'égout
                DrawCircle(cx + 4, py + 23, 4, (Color){dark.r, dark.g, dark.b, 220});
                DrawCircleLines(cx + 4, py + 23, 4, (Color){lite.r, lite.g, lite.b, 90});
            }
            if (h & 1)  // fragment de bande jaune (1 tuile sur 2)
                DrawRectangle(cx - 1, py + 6, 2, 9, (Color){190, 170, 40, 110});
            speck(px, py, h, 1, 1, (Color){lite.r, lite.g, lite.b, 120});
        } break;
        case THEME_FACTORY:   // tôle : plaque rivetée + variantes techniques
            DrawRectangle(px + 2, py + TILE_SIZE/2, TILE_SIZE - 4, 1,
                          (Color){dark.r, dark.g, dark.b, 170});   // joint horizontal
            DrawRectangle(cx - 1, py + 2, 1, TILE_SIZE - 4,
                          (Color){dark.r, dark.g, dark.b, 150});   // joint vertical
            DrawRectangle(px + 4,  py + 4,  2, 2, (Color){lite.r, lite.g, lite.b, 210}); // rivets
            DrawRectangle(px + TILE_SIZE - 6, py + 4, 2, 2, (Color){lite.r, lite.g, lite.b, 210});
            DrawRectangle(px + 4, py + TILE_SIZE - 6, 2, 2, (Color){lite.r, lite.g, lite.b, 170});
            DrawRectangle(px + TILE_SIZE - 6, py + TILE_SIZE - 6, 2, 2, (Color){lite.r, lite.g, lite.b, 170});
            if (var == 1) {              // hachures d'avertissement usées
                for (int k = 0; k < 3; k++)
                    DrawLine(px + 5 + k*5, py + TILE_SIZE - 5,
                             px + 10 + k*5, py + TILE_SIZE - 10,
                             (Color){190, 170, 40, 90});
            } else if (var == 2) {       // grille d'aération
                for (int k = 0; k < 3; k++)
                    DrawRectangle(px + 7, py + 8 + k*4, 10, 2,
                                  (Color){dark.r, dark.g, dark.b, 200});
            }
            if (h & 2)  // tache d'huile
                DrawCircle(px + 13 + (int)(h & 7), py + 24, 3, (Color){0, 0, 0, 90});
            break;
        case THEME_SWAMP: {   // boue : mousse + flaque(s), disposition variable
            int dx = var * 4, dy = (var == 2) ? 3 : 0;
            DrawCircle(px + 10 + dx, py + 14 + dy, 4, (Color){lite.r, lite.g, lite.b, 100});
            DrawCircle(px + 27 - dx, py + 25, 3, (Color){lite.r, lite.g, lite.b, 80});
            DrawCircle(px + 26 - dx, py + 12, 3, (Color){dark.r, dark.g, dark.b, 150});      // flaque
            DrawCircleLines(px + 26 - dx, py + 12, 3, (Color){lite.r, lite.g, lite.b, 70});  // reflet
            if (var == 2) {              // 2e flaque + roseau
                DrawCircle(px + 12, py + 28, 3, (Color){dark.r, dark.g, dark.b, 140});
                DrawRectangle(px + 31, py + 17, 1, 9, (Color){lite.r, lite.g, lite.b, 130});
            }
        } break;
        case THEME_WASTELAND:
        default:              // terre brûlée : cendres + variantes
            speck(px, py, h, 0, 1, (Color){lite.r, lite.g, lite.b, 160});
            speck(px, py, h, 1, 2, (Color){lite.r, lite.g, lite.b, 110});
            speck(px, py, h, 3, 1, (Color){dark.r, dark.g, dark.b, 170});
            if (var == 1) {              // fissure de sécheresse
                DrawLine(px + 8,  py + 26, px + 18, py + 15, (Color){dark.r, dark.g, dark.b, 190});
                DrawLine(px + 18, py + 15, px + 29, py + 11, (Color){dark.r, dark.g, dark.b, 140});
            } else if (var == 2) {       // vieil os / branche blanchie
                DrawRectangle(px + 20, py + 27, 9, 2, (Color){lite.r, lite.g, lite.b, 190});
                DrawRectangle(px + 27, py + 25, 2, 2, (Color){lite.r, lite.g, lite.b, 190});
            }
            if ((h & 7) == 0)  // rare braise rougeoyante
                speck(px, py, h, 5, 1, (Color){200, 90, 40, 200});
            break;
    }
}

// ════════════════════════════════════════════════════
// RUINE / TERRAIN ENCOMBRÉ (constructible mais surcoût x2)
// Chaque thème a une structure identifiable au premier regard.
// ════════════════════════════════════════════════════
static void draw_ruin(int px, int py, ThemeID theme, unsigned int h) {
    const Theme *T = theme_get(theme);
    Color g = T->palette.ground_fill;   // la structure repose sur le terrain
    Color r = T->palette.ruin_fill;
    draw_base_fill(px, py, g);

    switch (theme) {
        // ── VILLE : gratte-ciel à moitié détruit ──────────────────
        case THEME_CITY: {
            Color body   = (Color){72, 74, 84, 255};
            Color body_d = (Color){46, 48, 56, 255};
            Color win_on = (Color){250, 214, 96, 255};
            Color win_of = (Color){26, 28, 38, 255};
            const int cols = 4;
            const int x0 = px + 4, totalw = TILE_SIZE - 8, cw = totalw / cols;
            for (int c = 0; c < cols; c++) {
                int cut   = (int)((h >> (c*3)) & 7);          // toit en dents de scie
                int colx  = x0 + c*cw;
                int ctop  = py + 4 + cut;
                int colh  = py + TILE_SIZE - 3 - ctop;
                Color bc  = (c == 0) ? body_d : body;          // face latérale ombrée
                DrawRectangle(colx, ctop, cw - 1, colh, bc);
                DrawRectangle(colx, ctop, cw - 1, 1, shade(bc, 24)); // arête éclairée
                // fenêtres : grille, certaines allumées
                for (int wy = ctop + 3; wy < py + TILE_SIZE - 5; wy += 6)
                    for (int wx = colx + 1; wx < colx + cw - 2; wx += 4) {
                        int lit = (h >> (((wx + wy) & 15))) & 1;
                        DrawRectangle(wx, wy, 2, 3, lit ? win_on : win_of);
                    }
            }
            DrawRectangle(px + 4, py + TILE_SIZE - 4, TILE_SIZE - 8, 2, (Color){0,0,0,110}); // ombre au sol
        } break;

        // ── DÉSERT : épave de véhicule à moitié ensablée ──────────
        case THEME_DESERT: {
            Color hull   = (Color){74, 66, 58, 255};
            Color hull_d = (Color){50, 44, 38, 255};
            Color rust   = (Color){150, 82, 36, 230};
            Color sand   = shade(g, 16);
            DrawRectangle(px + 6, py + 16, TILE_SIZE - 12, 12, hull);
            DrawRectangle(px + 6, py + 16, TILE_SIZE - 12, 3, shade(hull, 22));  // capot éclairé
            DrawRectangle(px + 10, py + 9, 15, 9, hull_d);                       // cabine
            DrawRectangle(px + 12, py + 11, 10, 5, (Color){34, 38, 46, 255});    // vitre brisée
            DrawCircle(px + 12, py + 28, 3, (Color){22, 22, 24, 255});           // roues
            DrawCircle(px + 27, py + 28, 3, (Color){22, 22, 24, 255});
            DrawRectangle(px + 8,  py + 20, 3, 3, rust);                         // rouille
            DrawRectangle(px + 28, py + 18, 3, 3, (Color){rust.r,rust.g,rust.b,180});
            DrawRectangle(px + 4, py + 30, TILE_SIZE - 8, 4, (Color){sand.r, sand.g, sand.b, 230}); // sable au pied
        } break;

        // ── MARAIS : arbres morts tordus + lianes ─────────────────
        case THEME_SWAMP: {
            Color trunk   = (Color){48, 40, 28, 255};
            Color trunk_d = (Color){30, 26, 16, 255};
            Color leaf    = shade(r, 34);
            DrawRectangle(px + 9,  py + 10, 4, TILE_SIZE - 13, trunk);   // tronc 1
            DrawRectangle(px + 9,  py + 10, 2, TILE_SIZE - 13, trunk_d);
            DrawRectangle(px + 24, py + 14, 4, TILE_SIZE - 17, trunk);   // tronc 2
            DrawLine(px + 11, py + 13, px + 5,  py + 6,  trunk);         // branches
            DrawLine(px + 11, py + 13, px + 18, py + 8,  trunk);
            DrawLine(px + 26, py + 16, px + 33, py + 11, trunk);
            DrawCircle(px + 8,  py + 8,  4, (Color){leaf.r, leaf.g, leaf.b, 200}); // feuillage/mousse
            DrawCircle(px + 20, py + 9,  5, (Color){leaf.r, leaf.g, leaf.b, 180});
            DrawCircle(px + 31, py + 11, 3, (Color){leaf.r, leaf.g, leaf.b, 160});
            for (int i = 0; i < 3; i++) {  // lianes pendantes
                int lx = px + 8 + i*10;
                DrawRectangle(lx, py + 16, 1, 10 + (int)((h >> (i*2)) & 7),
                              (Color){leaf.r, leaf.g, leaf.b, 150});
            }
        } break;

        // ── USINE : cuve chimique rouillée + tuyaux ───────────────
        case THEME_FACTORY: {
            Color metal   = (Color){80, 72, 62, 255};
            Color metal_d = (Color){52, 46, 38, 255};
            Color rust    = (Color){152, 84, 34, 235};
            DrawRectangle(px + 2,  py + 12, 8, 3, metal_d);   // tuyau gauche
            DrawRectangle(px + 30, py + 22, 8, 3, metal_d);   // tuyau droit
            DrawRectangle(px + 9,  py + 7, 22, 26, metal);    // cuve
            DrawRectangle(px + 9,  py + 7, 6,  26, shade(metal, 16));  // reflet vertical
            DrawRectangle(px + 9,  py + 7,  22, 2, metal_d);  // cerclages
            DrawRectangle(px + 9,  py + 19, 22, 2, metal_d);
            DrawRectangle(px + 9,  py + 31, 22, 2, metal_d);
            DrawCircle(px + 20, py + 7, 3, rust);             // valve
            DrawRectangle(px + 13, py + 10, 2, 8, rust);      // coulures
            DrawRectangle(px + 25, py + 21, 2, 7, (Color){rust.r, rust.g, rust.b, 170});
            DrawRectangle(px + 11, py + 9, 1, 1, (Color){205, 195, 175, 200}); // boulons
            DrawRectangle(px + 28, py + 9, 1, 1, (Color){205, 195, 175, 200});
        } break;

        // ── TERRES BRÛLÉES : blocs de béton brisés + ferraille ────
        case THEME_WASTELAND:
        default: {
            Color c1 = shade(r, 12), c2 = shade(r, -8), c3 = shade(r, -24);
            DrawRectangle(px + 6,  py + 14, 16, 16, c1);          // gros bloc
            DrawRectangle(px + 6,  py + 14, 16, 3, shade(r, 30)); // arête éclairée
            DrawRectangle(px + 22, py + 18, 12, 12, c2);          // bloc plus petit
            DrawRectangle(px + 22, py + 18, 12, 3, shade(r, 22));
            DrawRectangle(px + 9,  py + 31, 6, 2, c3);            // débris
            DrawRectangle(px + 25, py + 32, 5, 2, c3);
            DrawLine(px + 10, py + 14, px + 9,  py + 6, (Color){122, 112, 96, 220}); // ferraille
            DrawLine(px + 16, py + 14, px + 18, py + 7, (Color){122, 112, 96, 200});
            DrawRectangle(px + 6, py + 30, 16, 2, (Color){0, 0, 0, 90});            // ombre
        } break;
    }
}

// ════════════════════════════════════════════════════
// EAU / OBSTACLE LIQUIDE (animée)
// ════════════════════════════════════════════════════
static void draw_water(int px, int py, ThemeID theme, unsigned int h, float t) {
    const Theme *T = theme_get(theme);
    Color base = T->palette.water_fill, stroke = T->palette.water_stroke;
    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, base);

    // Reflets horizontaux qui dérivent
    for (int i = 0; i < 3; i++) {
        float phase = t * 0.6f + i * 1.7f + (float)(h & 7) * 0.3f;
        int   yy    = py + 6 + i * 11;
        int   off   = (int)((sinf(phase) * 0.5f + 0.5f) * 7.0f);
        DrawRectangle(px + 3 + off, yy, TILE_SIZE - 6 - off, 2,
                      (Color){stroke.r, stroke.g, stroke.b, 90});
    }
    // Bulles montantes
    for (int k = 0; k < 2; k++) {
        float ph = t * 0.8f + (float)k * 2.3f + (float)((h >> k) & 15);
        float fy = 1.0f - fmodf(ph * 0.25f, 1.0f);   // 1 → 0 (remonte)
        int   bx = px + 8 + (int)((h >> (k*4)) % (TILE_SIZE - 16));
        int   by = py + 4 + (int)(fy * (TILE_SIZE - 8));
        DrawCircle(bx, by, 1.5f, (Color){stroke.r, stroke.g, stroke.b, 150});
    }
    // Séparation discrète
    DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, (Color){0,0,0,55});
    DrawRectangle(px + TILE_SIZE - 1, py, 1, TILE_SIZE, (Color){0,0,0,55});
}

// ════════════════════════════════════════════════════
// DISPATCHER — fond pixel-art d'une tuile quelconque
// Les PATH/SPAWN/BASE reçoivent un fond de sol (les routes,
// portails et bunkers sont dessinés ensuite par-dessus).
// ════════════════════════════════════════════════════
void tile_art_draw_tile_bg(const Map *map, int tx, int ty, int cleared) {
    int px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    unsigned int h = tile_hash(tx, ty);
    TileType type = map->tiles[ty][tx].type;
    // Tour posée sur une ruine → chantier déblayé : sol nu (comme en 3D).
    if (cleared && type == TILE_RUIN) type = TILE_GROUND;
    switch (type) {
        case TILE_RUIN:  draw_ruin (px, py, map->theme, h); break;
        case TILE_WATER: draw_water(px, py, map->theme, h, g_ta_clock); break;
        case TILE_GROUND:
        case TILE_PATH:
        case TILE_SPAWN:
        case TILE_BASE:
        default:         draw_ground(px, py, map->theme, h); break;
    }
    // Transitions : rives / éboulis selon les 4 voisins (tile_art_decor)
    if (type != TILE_WATER)
        tile_art_draw_fringes(map, tx, ty);
}

// ════════════════════════════════════════════════════
// ROUTE CONNECTÉE (une tuile)
// ════════════════════════════════════════════════════
static void draw_road_tile(const Map *map, int tx, int ty) {
    const int    px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    const int    cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
    RoadPalette  pal = road_palette(map->theme);

    int N = is_road(map, tx,   ty-1);
    int S = is_road(map, tx,   ty+1);
    int W = is_road(map, tx-1, ty  );
    int E = is_road(map, tx+1, ty  );
    int links = N + S + E + W;

    const int hw  = (int)(TILE_SIZE * 0.30f);   // demi-largeur chaussée (~12)
    const int hwo = hw + 2;                      // + liseré

    // ── Couche liseré (sous-couche sombre, légèrement plus large) ──
    DrawRectangle(cx - hwo, cy - hwo, hwo*2, hwo*2, pal.road_dark);
    if (N) DrawRectangle(cx - hwo, py,        hwo*2, TILE_SIZE/2 + 2, pal.road_dark);
    if (S) DrawRectangle(cx - hwo, cy - 2,    hwo*2, TILE_SIZE/2 + 2, pal.road_dark);
    if (W) DrawRectangle(px,       cy - hwo,  TILE_SIZE/2 + 2, hwo*2, pal.road_dark);
    if (E) DrawRectangle(cx - 2,   cy - hwo,  TILE_SIZE/2 + 2, hwo*2, pal.road_dark);

    // ── Chaussée ──────────────────────────────────────────────────
    DrawRectangle(cx - hw, cy - hw, hw*2, hw*2, pal.road);
    if (N) DrawRectangle(cx - hw, py,     hw*2, TILE_SIZE/2, pal.road);
    if (S) DrawRectangle(cx - hw, cy,     hw*2, TILE_SIZE/2, pal.road);
    if (W) DrawRectangle(px,      cy - hw, TILE_SIZE/2, hw*2, pal.road);
    if (E) DrawRectangle(cx,      cy - hw, TILE_SIZE/2, hw*2, pal.road);

    // ── Tuile isolée (aucun voisin) : petit rond de route ─────────
    if (links == 0)
        DrawCircle(cx, cy, (float)hw, pal.road);

    // ── Texture : graviers / fissures déterministes ───────────────
    unsigned int h = tile_hash(tx, ty);
    for (int k = 0; k < 4; k++) {
        int gx = px + 6 + (int)((h >> (k*5))      & 0x1F);   // 6..37
        int gy = py + 6 + (int)((h >> (k*5 + 3))   & 0x1F);
        // garde le gravier sur la chaussée (proche du centre/arms)
        if (abs(gx - cx) > hw && abs(gy - cy) > hw) continue;
        int sz = 1 + (int)((h >> (k*3)) & 1);
        Color c = (k & 1) ? pal.road_dark : pal.road_lite;
        DrawRectangle(gx, gy, sz, sz, (Color){c.r, c.g, c.b, 150});
    }

    // ── Marquage central (pointillés) sur les segments droits ─────
    if ((N && S) && !E && !W) {        // vertical
        for (int yy = py + 4; yy < py + TILE_SIZE - 3; yy += 9)
            DrawRectangle(cx - 1, yy, 2, 5, (Color){pal.road_lite.r, pal.road_lite.g, pal.road_lite.b, 110});
    } else if ((E && W) && !N && !S) { // horizontal
        for (int xx = px + 4; xx < px + TILE_SIZE - 3; xx += 9)
            DrawRectangle(xx, cy - 1, 5, 2, (Color){pal.road_lite.r, pal.road_lite.g, pal.road_lite.b, 110});
    }
}

void tile_art_draw_paths(const Map *map) {
    for (int y = 0; y < map->h; y++)
        for (int x = 0; x < map->w; x++)
            if (map->tiles[y][x].type == TILE_PATH)
                draw_road_tile(map, x, y);
}

// ════════════════════════════════════════════════════
// SPAWN — portail d'invasion
// ════════════════════════════════════════════════════
static void draw_spawn_tile(const Map *map, int tx, int ty) {
    const int px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    const int cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;

    float t     = (float)GetTime();
    float pulse = (sinf(t * 3.0f) + 1.0f) * 0.5f;   // 0..1

    // ── Onde de repérage : large vague rouge foncé émise par le spawn ──
    // Rend l'emplacement d'invasion hyper visible dès l'arrivée sur la carte.
    {
        float maxR = TILE_SIZE * 4.2f;
        for (int k = 0; k < 2; k++) {
            float ph = fmodf(t/2.0f + (float)(tx*7 + ty)*0.13f + k*0.5f, 1.0f);
            unsigned char a = (unsigned char)((1.0f - ph) * 130);
            DrawCircleLines(cx, cy, ph*maxR,     (Color){180, 30, 30, a});
            DrawCircleLines(cx, cy, ph*maxR + 1, (Color){120, 16, 16, (unsigned char)(a/2)});
        }
    }

    // Raccord à la route sous le portail
    draw_road_tile(map, tx, ty);

    // Cuvette sombre
    DrawCircle(cx, cy, TILE_SIZE * 0.40f, (Color){18, 6, 6, 230});

    // Halo rouge pulsé
    unsigned char ga = (unsigned char)(50 + (int)(pulse * 90));
    DrawCircle(cx, cy, TILE_SIZE * 0.42f, (Color){200, 40, 40, (unsigned char)(ga/3)});

    // Anneaux concentriques
    DrawCircleLines(cx, cy, TILE_SIZE * 0.36f, (Color){231, 76, 60, ga});
    DrawCircleLines(cx, cy, TILE_SIZE * 0.24f,
                    (Color){255, 140, 110, (unsigned char)(ga + 40)});

    // Chevrons descendants (les ennemis émergent vers le bas)
    Color cv = (Color){255, 170, 150, (unsigned char)(140 + (int)(pulse * 80))};
    for (int i = 0; i < 2; i++) {
        int oy = cy - 6 + i * 7;
        DrawTriangle(
            (Vector2){(float)(cx),     (float)(oy + 6)},
            (Vector2){(float)(cx - 6), (float)(oy)},
            (Vector2){(float)(cx + 6), (float)(oy)},
            cv);
    }
}

void tile_art_draw_spawns(const Map *map) {
    for (int y = 0; y < map->h; y++)
        for (int x = 0; x < map->w; x++)
            if (map->tiles[y][x].type == TILE_SPAWN)
                draw_spawn_tile(map, x, y);
}

/* (Bases, minerais, roche minée, franges et ombres portées :
   voir tile_art_decor.c — extrait pour la règle < 500 lignes.) */

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tile_art.h"
#include <math.h>
#include <stdlib.h>     // abs

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

// Hash déterministe (stable par tuile) → pseudo-aléatoire reproductible.
static unsigned int tile_hash(int x, int y) {
    unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// Teste si une tuile fait partie du réseau de route (chemin/spawn/base).
static int is_road(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->w || y >= map->h) return 0;
    TileType t = map->tiles[y][x].type;
    return (t == TILE_PATH || t == TILE_SPAWN || t == TILE_BASE);
}

// Éclaircit (d>0) ou assombrit (d<0) une couleur, alpha conservé.
static Color shade(Color c, int d) {
    int r = c.r + d, g = c.g + d, b = c.b + d;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, c.a};
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

// ── Petit speck déterministe ──────────────────────────────────
static void speck(int px, int py, unsigned int h, int k, int sz, Color c) {
    int gx = px + 4 + (int)((h >> (k*5))     % (TILE_SIZE - 8));
    int gy = py + 4 + (int)((h >> (k*5 + 7)) % (TILE_SIZE - 8));
    DrawRectangle(gx, gy, sz, sz, c);
}

// ════════════════════════════════════════════════════
// SOL (buildable)
// ════════════════════════════════════════════════════
static void draw_ground(int px, int py, ThemeID theme, unsigned int h) {
    const Theme *T = theme_get(theme);
    Color base = T->palette.ground_fill;
    draw_base_fill(px, py, base);
    Color lite = shade(base, 28), dark = shade(base, -20);
    int cx = px + TILE_SIZE/2;

    switch (theme) {
        case THEME_DESERT: {  // sable : dunes ondulées + grains + galet
            for (int i = 0; i < 3; i++) {
                int yy  = py + 9 + i*10;
                int amp = 2 + (int)((h >> (i*3)) & 3);
                DrawRectangle(px + 3, yy,         TILE_SIZE/2 - 3, 1,
                              (Color){lite.r, lite.g, lite.b, 130});
                DrawRectangle(cx,     yy + amp-1, TILE_SIZE/2 - 3, 1,
                              (Color){lite.r, lite.g, lite.b, 100});
            }
            speck(px, py, h, 0, 1, (Color){lite.r, lite.g, lite.b, 210});
            speck(px, py, h, 3, 2, (Color){dark.r, dark.g, dark.b, 150});  // galet
        } break;
        case THEME_CITY: {    // bitume : joints de dilatation + fissure + marquage
            DrawRectangle(px + 2, py + TILE_SIZE/2, TILE_SIZE - 4, 1,
                          (Color){dark.r, dark.g, dark.b, 150});
            DrawLine(px + 7, py + 6, px + 15, py + 20, (Color){dark.r, dark.g, dark.b, 200});
            DrawLine(px + 15, py + 20, px + 28, py + 27, (Color){dark.r, dark.g, dark.b, 150});
            if (h & 1)  // fragment de bande jaune (1 tuile sur 2)
                DrawRectangle(cx - 1, py + 6, 2, 9, (Color){190, 170, 40, 110});
            speck(px, py, h, 1, 1, (Color){lite.r, lite.g, lite.b, 120});
        } break;
        case THEME_FACTORY:   // tôle : plaque rivetée + tache d'huile
            DrawRectangle(px + 2, py + TILE_SIZE/2, TILE_SIZE - 4, 1,
                          (Color){dark.r, dark.g, dark.b, 170});   // joint horizontal
            DrawRectangle(cx - 1, py + 2, 1, TILE_SIZE - 4,
                          (Color){dark.r, dark.g, dark.b, 150});   // joint vertical
            DrawRectangle(px + 4,  py + 4,  2, 2, (Color){lite.r, lite.g, lite.b, 210}); // rivets
            DrawRectangle(px + TILE_SIZE - 6, py + 4, 2, 2, (Color){lite.r, lite.g, lite.b, 210});
            DrawRectangle(px + 4, py + TILE_SIZE - 6, 2, 2, (Color){lite.r, lite.g, lite.b, 170});
            DrawRectangle(px + TILE_SIZE - 6, py + TILE_SIZE - 6, 2, 2, (Color){lite.r, lite.g, lite.b, 170});
            if (h & 2)  // tache d'huile
                DrawCircle(px + 13 + (int)(h & 7), py + 24, 3, (Color){0, 0, 0, 90});
            break;
        case THEME_SWAMP:     // boue : plaques de mousse + flaque cernée
            DrawCircle(px + 12, py + 14, 4, (Color){lite.r, lite.g, lite.b, 100});
            DrawCircle(px + 27, py + 25, 3, (Color){lite.r, lite.g, lite.b, 80});
            DrawCircle(px + 26, py + 12, 3, (Color){dark.r, dark.g, dark.b, 150});      // flaque
            DrawCircleLines(px + 26, py + 12, 3, (Color){lite.r, lite.g, lite.b, 70});  // reflet
            break;
        case THEME_WASTELAND:
        default:              // terre brûlée : cendres + braise + fissure
            speck(px, py, h, 0, 1, (Color){lite.r, lite.g, lite.b, 160});
            speck(px, py, h, 1, 2, (Color){lite.r, lite.g, lite.b, 110});
            speck(px, py, h, 3, 1, (Color){dark.r, dark.g, dark.b, 170});
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
void tile_art_draw_tile_bg(const Map *map, int tx, int ty) {
    int px = tx * TILE_SIZE, py = ty * TILE_SIZE;
    unsigned int h = tile_hash(tx, ty);
    switch (map->tiles[ty][tx].type) {
        case TILE_RUIN:  draw_ruin (px, py, map->theme, h); break;
        case TILE_WATER: draw_water(px, py, map->theme, h, (float)GetTime()); break;
        case TILE_GROUND:
        case TILE_PATH:
        case TILE_SPAWN:
        case TILE_BASE:
        default:         draw_ground(px, py, map->theme, h); break;
    }
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

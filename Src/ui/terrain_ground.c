/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  terrain_ground.c ─ Peinture pixel du SOL de chaque thème.
 *
 *  Deux passes sur l'image du terrain (1 pixel d'art = TB_PX px monde) :
 *    1. le sol continu — bruit fractal sur une rampe à 6 tons tramés, plus
 *       la matière du thème (cendre, mousse, rides de dune, dalles, tôles) ;
 *    2. les détails semés tuile par tuile — galets ombrés, fissures en
 *       relief, touffes, objets propres au thème.
 *  Routes et liquides sont peints ENSUITE par terrain_bake.c et recouvrent
 *  ce qui passe dessous : pas besoin de les éviter ici.
 */
#include "terrain_internal.h"
#include "../map/theme.h"

// ── Teintes moyennes des matières (rampes dérivées par tb_make_ramp) ──
#define TG_SALT          0x7E44A1u
#define TG_GRID_A        0.38f   // opacité des repères de coin de tuile

typedef struct {
    Color ground;     // sol principal
    Color patch;      // seconde matière (cendre, mousse, asphalte, rouille…)
    Color grass;      // touffes / herbes (alpha 0 = aucune)
    Color accent;     // petit détail lumineux (braise, éclat radioactif…)
} GroundStyle;

static GroundStyle style_of(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (GroundStyle){{ 62,  70,  42, 255}, { 74, 108,  44, 255},
                                                 { 96, 146,  58, 255}, {196,  80,  60, 255}};
        case THEME_DESERT:  return (GroundStyle){{182, 136,  78, 255}, {150, 112,  62, 255},
                                                 {150, 132,  70,   0}, {140, 255, 120, 255}};
        case THEME_CITY:    return (GroundStyle){{ 92,  92, 100, 255}, { 58,  60,  66, 255},
                                                 { 92, 120,  56, 255}, {170,  70,  52, 255}};
        case THEME_FACTORY: return (GroundStyle){{ 86,  80,  76, 255}, {124,  74,  42, 255},
                                                 {  0,   0,   0,   0}, {226, 186,  44, 255}};
        case THEME_WASTELAND:
        default:            return (GroundStyle){{108,  74,  44, 255}, {134, 118,  98, 255},
                                                 {172, 142,  74, 255}, {255, 120,  40, 255}};
    }
}

void tb_ground_ramp(ThemeID th, Color out[TB_RAMP]) {
    tb_make_ramp(style_of(th).ground, out);
}

static TileType tile_at(const Map *m, int tx, int ty) {
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= m->w) tx = m->w - 1;
    if (ty >= m->h) ty = m->h - 1;
    return m->tiles[ty][tx].type;
}

// ════════════════════════════════════════════════════════════════
// PASSE 1 — SOL CONTINU
// ════════════════════════════════════════════════════════════════
void tb_paint_ground(Image *im, const Map *map, unsigned int seed) {
    const ThemeID th = map->theme;
    const GroundStyle st = style_of(th);
    Color rg[TB_RAMP], rp[TB_RAMP];
    tb_make_ramp(st.ground, rg);
    tb_make_ramp(st.patch, rp);
    const unsigned int s = seed ^ TG_SALT;

    for (int y = 0; y < im->height; y++) {
        for (int x = 0; x < im->width; x++) {
            float fx = (float)x, fy = (float)y;
            float v = 0.50f + (tb_fbm(fx / 18.0f, fy / 18.0f, s, 3) - 0.5f) * 0.85f
                            + (tb_fbm(fx / 4.0f,  fy / 4.0f,  s + 5u, 2) - 0.5f) * 0.32f
                            + (tb_rand(x, y, s + 9u) - 0.5f) * 0.10f;
            const Color *ramp = rg;
            float m = tb_fbm(fx / 24.0f + 40.0f, fy / 24.0f, s + 11u, 3);   // 2e matière
            int lx = x % TB_TILE, ly = y % TB_TILE;

            switch (th) {
                case THEME_WASTELAND:                 // cendre (gris chaud) + brûlures sombres
                    if (m > 0.64f) { ramp = rp; v += (m - 0.64f) * 0.6f; }
                    else if (m < 0.34f) v -= 0.20f;
                    break;
                case THEME_SWAMP:                     // mousse verte sur la boue
                    if (m > 0.55f) { ramp = rp; v += (m - 0.55f) * 0.5f; }
                    else if (m < 0.30f) v -= 0.14f;   // boue détrempée
                    break;
                case THEME_DESERT: {                  // rides de dune
                    float r = sinf(fx * 0.55f + fy * 0.26f +
                                   tb_fbm(fx / 10.0f, fy / 10.0f, s + 31u, 2) * 9.0f);
                    if (r > 0.80f)       v += 0.24f;  // crête éclairée
                    else if (r < -0.86f) v -= 0.18f;  // creux ombré
                    if (m > 0.66f) { ramp = rp; }     // sable compacté plus sombre
                } break;
                case THEME_CITY: {                    // dalles de béton / plaques d'asphalte
                    if (m > 0.65f) { ramp = rp; v = 0.45f + (v - 0.5f) * 0.5f; break; }
                    int sx = x / (TB_TILE / 2), sy = y / (TB_TILE / 2);
                    int bx = x % (TB_TILE / 2), by = y % (TB_TILE / 2);
                    v += (tb_rand(sx, sy, s + 41u) - 0.5f) * 0.20f;
                    if (bx == TB_TILE / 2 - 1 || by == TB_TILE / 2 - 1) v -= 0.24f;  // joint
                    else if (bx == 0 || by == 0) v += 0.08f;                        // biseau
                } break;
                case THEME_FACTORY: {                 // tôles rivetées (1 par tuile) + rouille
                    int tx = x / TB_TILE, ty = y / TB_TILE;
                    v += (tb_rand(tx, ty, s + 51u) - 0.5f) * 0.16f;
                    if (lx == 0 || ly == 0)                       v += 0.22f;   // biseau éclairé
                    else if (lx == TB_TILE - 1 || ly == TB_TILE - 1) v = 0.06f; // arête d'ombre
                    if (tb_rand(tx, ty, s + 53u) > 0.62f &&                     // tôle larmée
                        (x & 3) == 0 && ((y + ((x >> 2) & 1) * 2) & 3) == 0)
                        v += 0.22f;
                    int rv = (lx == 2 || lx == TB_TILE - 3) && (ly == 2 || ly == TB_TILE - 3);
                    if (rv) v = 0.92f;                                          // rivets
                    if (m > 0.63f) { ramp = rp; v = 0.30f + (m - 0.63f) * 1.4f + (v - 0.5f) * 0.4f; }
                } break;
                default: break;
            }
            tb_put(im, x, y, tb_ramp(ramp, v, x, y));
        }
    }

    /* Repères de coin de tuile : une croix pâle de 3 px, sur le sol
       constructible seulement — le placement reste lisible sans grille. */
    if (th != THEME_FACTORY) {   // l'usine a déjà ses tôles en grille
        Color mk = tb_scale(st.ground, 0.55f);
        for (int ty = 1; ty < map->h; ty++)
            for (int tx = 1; tx < map->w; tx++) {
                if (tile_at(map, tx, ty) != TILE_GROUND || tile_at(map, tx - 1, ty - 1) != TILE_GROUND)
                    continue;
                int cx = tx * TB_TILE, cy = ty * TB_TILE;
                for (int k = -1; k <= 1; k++) {
                    tb_blend(im, cx + k, cy, mk, TG_GRID_A);
                    if (k) tb_blend(im, cx, cy + k, mk, TG_GRID_A);
                }
            }
    }
}

// ════════════════════════════════════════════════════════════════
// PASSE 2 — DÉTAILS (galets, fissures, touffes, objets du thème)
// ════════════════════════════════════════════════════════════════
static void pebble(Image *im, int x, int y, int r, const Color rg[TB_RAMP]) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r * r + 1) continue;
            int lit = (dx + dy < 0), dark = (dx + dy > r - 1);
            tb_put(im, x + dx, y + dy, lit ? rg[5] : dark ? rg[1] : rg[3]);
        }
    tb_blend(im, x + r + 1, y + 1, rg[0], 0.6f);        // ombre portée SE
    tb_blend(im, x + 1, y + r + 1, rg[0], 0.6f);
}

static void crack(Image *im, int x, int y, unsigned int h, const Color rg[TB_RAMP]) {
    int dx = (h & 1) ? 1 : -1, len = 6 + (int)(h >> 3 & 7);
    for (int i = 0; i < len; i++) {
        tb_put(im, x, y, rg[0]);
        tb_blend(im, x, y + 1, rg[4], 0.5f);             // lèvre éclairée (relief)
        if ((h >> (i & 15)) & 1) x += dx; else y += 1;
        if (((h >> ((i + 5) & 15)) & 3) == 0) {          // ramification
            tb_put(im, x - dx, y + 1, rg[1]);
        }
    }
}

static void tuft(Image *im, int x, int y, Color g, unsigned int h) {
    Color gr[TB_RAMP];
    tb_make_ramp(g, gr);
    int blades = 3 + (int)(h & 1);
    for (int b = 0; b < blades; b++) {
        int bx = x - 1 + b, hgt = 2 + (int)((h >> (b * 2)) & 1) + (b == 1);
        int lean = (b == 0) ? -1 : (b == blades - 1) ? 1 : 0;
        for (int k = 0; k < hgt; k++)
            tb_put(im, bx + (k == hgt - 1 ? lean : 0), y - k, k == hgt - 1 ? gr[5] : gr[2 + (k & 1)]);
    }
    tb_blend(im, x, y + 1, (Color){0, 0, 0, 255}, 0.35f);
}

static void theme_object(Image *im, int x, int y, ThemeID th, unsigned int h,
                         const GroundStyle *st, const Color rg[TB_RAMP]) {
    switch (th) {
        case THEME_WASTELAND:
            if (h & 1) {                                   // os blanchi
                Color b = {214, 204, 180, 255};
                for (int i = 0; i < 4; i++) tb_put(im, x + i, y, b);
                tb_put(im, x - 1, y - 1, b); tb_put(im, x - 1, y + 1, b);
                tb_put(im, x + 4, y - 1, b); tb_put(im, x + 4, y + 1, b);
                tb_blend(im, x + 1, y + 1, rg[0], 0.6f);
            } else {                                       // braise rougeoyante
                tb_put(im, x, y, st->accent);
                tb_blend(im, x + 1, y, st->accent, 0.45f); tb_blend(im, x - 1, y, st->accent, 0.45f);
                tb_blend(im, x, y - 1, (Color){255, 200, 90, 255}, 0.5f);
            }
            break;
        case THEME_SWAMP:
            if (h & 1) {                                   // flaque avec reflet
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -3; dx <= 3; dx++)
                        if (dx * dx / 9.0f + dy * dy <= 1.2f) tb_put(im, x + dx, y + dy, (Color){28, 44, 40, 255});
                tb_put(im, x - 1, y - 1, (Color){120, 160, 140, 255});
            } else {                                       // champignons
                for (int k = 0; k < 2; k++) {
                    int mx = x + k * 3, my = y + k;
                    tb_put(im, mx, my, (Color){200, 190, 170, 255});
                    tb_put(im, mx - 1, my - 1, st->accent); tb_put(im, mx, my - 1, st->accent);
                    tb_put(im, mx + 1, my - 1, tb_scale(st->accent, 0.7f));
                    tb_put(im, mx, my - 2, (Color){240, 150, 130, 255});
                }
            }
            break;
        case THEME_DESERT:                                 // éclat radioactif
            tb_put(im, x, y, st->accent);
            tb_blend(im, x + 1, y, st->accent, 0.35f); tb_blend(im, x, y + 1, st->accent, 0.35f);
            tb_blend(im, x - 1, y, st->accent, 0.2f);  tb_blend(im, x, y - 1, (Color){255, 255, 255, 255}, 0.5f);
            break;
        case THEME_CITY:                                   // briques tombées
            for (int k = 0; k < 2 + (int)(h & 1); k++) {
                int bx = x + k * 3 - (int)((h >> 3) & 1), by = y + (k & 1);
                tb_put(im, bx, by, st->accent); tb_put(im, bx + 1, by, tb_scale(st->accent, 0.8f));
                tb_put(im, bx, by - 1, tb_scale(st->accent, 1.25f));
                tb_blend(im, bx + 1, by + 1, rg[0], 0.6f);
            }
            break;
        case THEME_FACTORY:
            if (h & 1) {                                   // tache d'huile irisée
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -3; dx <= 3; dx++)
                        if (dx * dx / 9.0f + dy * dy / 4.0f <= 1.0f)
                            tb_blend(im, x + dx, y + dy, (Color){12, 10, 14, 255}, 0.75f);
                tb_put(im, x - 1, y - 1, (Color){120, 90, 200, 255});
                tb_put(im, x + 1, y - 1, (Color){70, 170, 150, 255});
            } else {                                       // bande de danger usée
                for (int dx = 0; dx < 6; dx++)
                    for (int dy = 0; dy < 2; dy++)
                        tb_blend(im, x + dx, y + dy, ((dx + dy) / 2 & 1) ? (Color){30, 26, 22, 255}
                                                                       : st->accent, 0.8f);
            }
            break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════
// PASSE 3 — SOL DES RUINES : amas organique de gravats / broussailles
// (le sprite de ruine se pose dessus ; sous une tour, il reste le chantier)
// ════════════════════════════════════════════════════════════════
static Color floor_color(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (Color){ 56,  98,  42, 255};   // broussailles
        case THEME_DESERT:  return (Color){140, 100,  62, 255};   // sable + ferraille
        case THEME_CITY:    return (Color){116, 112, 116, 255};   // béton concassé
        case THEME_FACTORY: return (Color){100,  88,  80, 255};   // ferraille
        case THEME_WASTELAND:
        default:            return (Color){124, 106,  88, 255};   // gravats
    }
}

void tb_paint_ruin_floor(Image *im, const Map *map, unsigned int seed) {
    const ThemeID th = map->theme;
    const GroundStyle st = style_of(th);
    Color fr[TB_RAMP];
    tb_make_ramp(floor_color(th), fr);
    const unsigned int s = seed ^ (TG_SALT * 5u);

    for (int ty = 0; ty < map->h; ty++)
        for (int tx = 0; tx < map->w; tx++) {
            if (map->tiles[ty][tx].type != TILE_RUIN) continue;
            int ox = tx * TB_TILE, oy = ty * TB_TILE;
            float cx = ox + TB_TILE * 0.5f, cy = oy + TB_TILE * 0.62f;
            for (int y = oy - 3; y < oy + TB_TILE + 3; y++)
                for (int x = ox - 3; x < ox + TB_TILE + 3; x++) {
                    float dx = (x + 0.5f - cx) / 11.5f, dy = (y + 0.5f - cy) / 9.5f;
                    float r = sqrtf(dx * dx + dy * dy) + (tb_fbm(x / 4.0f, y / 4.0f, s, 2) - 0.5f) * 0.45f;
                    if (r > 1.0f) continue;
                    float v = 0.42f + (tb_fbm(x / 3.0f, y / 3.0f, s + 3u, 2) - 0.5f) * 0.55f + (1.0f - r) * 0.12f;
                    Color c = tb_ramp(fr, v, x, y);
                    if (r > 0.85f) tb_blend(im, x, y, c, 0.55f);   // bord fondu
                    else tb_put(im, x, y, c);
                }
            unsigned h = tb_hash(tx, ty, s + 7u);
            for (int k = 0; k < 6; k++) {                  // fragments ombrés
                int px = ox + 3 + (int)((tb_hash(tx, ty, s + 10u + k) >> 4) % (TB_TILE - 6));
                int py = oy + 5 + (int)((tb_hash(tx, ty, s + 10u + k) >> 12) % (TB_TILE - 8));
                if (th == THEME_SWAMP) {                   // touffes de feuillage
                    Color g[TB_RAMP];
                    tb_make_ramp(st.grass, g);
                    for (int dy = -2; dy <= 1; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (dx * dx + dy * dy <= 4)
                                tb_put(im, px + dx, py + dy, g[(dx + dy < -1) ? 5 : (dx + dy > 1) ? 1 : 3]);
                } else {
                    pebble(im, px, py, k < 2 ? 2 : 1, fr);
                }
            }
            if (th == THEME_CITY && (h & 1)) theme_object(im, ox + 6, oy + 14, th, h, &st, fr);
            if (th == THEME_WASTELAND || th == THEME_DESERT) {   // ferraille rouillée
                Color rust = {150, 80, 40, 255};
                int x = ox + 4 + (int)(h % 10u), y = oy + 8 + (int)((h >> 5) % 8u);
                for (int k = 0; k < 4; k++) tb_put(im, x + k, y - (k >> 1), k ? rust : tb_scale(rust, 1.3f));
            }
        }
}

void tb_paint_details(Image *im, const Map *map, unsigned int seed) {
    const ThemeID th = map->theme;
    const GroundStyle st = style_of(th);
    Color rg[TB_RAMP];
    tb_make_ramp(st.ground, rg);
    const unsigned int s = seed ^ (TG_SALT * 3u);

    for (int ty = 0; ty < map->h; ty++) {
        for (int tx = 0; tx < map->w; tx++) {
            if (map->tiles[ty][tx].type != TILE_GROUND) continue;
            unsigned int h = tb_hash(tx, ty, s);
            int ox = tx * TB_TILE, oy = ty * TB_TILE;
            #define RX(k) (ox + 3 + (int)((tb_hash(tx, ty, s + (k)) >> 4) % (TB_TILE - 6)))
            #define RY(k) (oy + 3 + (int)((tb_hash(tx, ty, s + (k)) >> 12) % (TB_TILE - 6)))
            int pebbles = 1 + (int)(h & 1) + (th == THEME_DESERT || th == THEME_WASTELAND);
            for (int k = 0; k < pebbles; k++)
                pebble(im, RX(10 + k), RY(10 + k), (tb_hash(tx, ty, s + 30u + k) & 3) == 0 ? 2 : 1, rg);
            if (th != THEME_FACTORY && (h >> 4) % 3 == 0)
                crack(im, RX(40), RY(40), tb_hash(tx, ty, s + 41u), rg);
            if (st.grass.a && (h >> 6) % 3 != 0)
                tuft(im, RX(50), RY(50) + 2, st.grass, tb_hash(tx, ty, s + 51u));
            if ((h >> 9) % (th == THEME_FACTORY ? 7u : 4u) == 0)   // usine : objets plus rares
                theme_object(im, RX(60), RY(60), th, tb_hash(tx, ty, s + 61u), &st, rg);
            #undef RX
            #undef RY
        }
    }
}

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  terrain_bake.c ─ Cuisson du terrain d'une carte + liquides + routes.
 *
 *  Ordre des passes (chacune recouvre la précédente) : sol → détails →
 *  liquides (rives ORGANIQUES : champ bilinéaire des tuiles d'eau, bruité,
 *  coupé à 0,5 — la rive tombe sur la frontière de tuile, en courbe) →
 *  routes (distance au réseau de segments centre-à-centre : la chaussée
 *  épouse les virages, ornières et marquages suivent la direction).
 */
#include "terrain.h"
#include "terrain_internal.h"
#include "../map/theme.h"
#include <string.h>

#define TB_CACHE        2       // cartes en cache (partie + carte de référence MP)
#define TB_LIQ_NOISE    0.22f   // amplitude du bruit de rive (× champ 0..1)
#define TB_LIQ_RIM      0.045f  // épaisseur du liseré de rive (champ)
#define TB_WET          0.12f   // bande de sol mouillé autour du liquide (champ)
#define TB_ROAD_HW      6.6f    // demi-largeur de chaussée (px d'art)
#define TB_ROAD_EDGE_N  1.6f    // irrégularité du bord de route (px d'art)
#define TB_ROAD_SHOULD  2.5f    // bas-côté (px d'art)

typedef struct {
    int       used;
    unsigned  key;
    unsigned  stamp;
    Texture2D tex;
} TerrainSlot;
static TerrainSlot g_slots[TB_CACHE];
static unsigned    g_stamp;

static unsigned map_key(const Map *m) {
    unsigned h = 2166136261u;
    #define MIX(v) (h = (h ^ (unsigned)(v)) * 16777619u)
    MIX(m->seed); MIX(m->w); MIX(m->h); MIX(m->theme); MIX(m->path_count);
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) MIX(m->tiles[y][x].type);
    #undef MIX
    return h;
}

static TileType tt(const Map *m, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= m->w) x = m->w - 1;
    if (y >= m->h) y = m->h - 1;
    return m->tiles[y][x].type;
}
static int is_road(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    TileType t = m->tiles[y][x].type;
    return t == TILE_PATH || t == TILE_SPAWN || t == TILE_BASE;
}

// ════════════════════════════════════════════════════════════════
// LIQUIDES
// ════════════════════════════════════════════════════════════════
typedef enum { LQ_TOXIC, LQ_MARSH, LQ_SAND, LQ_FLOOD, LQ_CHEM } LiquidKind;
typedef struct { LiquidKind kind; Color deep, shallow, rim, extra; } LiquidStyle;

static LiquidStyle liquid_of(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (LiquidStyle){LQ_MARSH, {14, 40, 34, 255}, {44, 92, 70, 255},
                                                 {110, 150, 80, 255}, {72, 140, 54, 255}};
        case THEME_DESERT:  return (LiquidStyle){LQ_SAND,  {104, 70, 34, 255}, {150, 108, 58, 255},
                                                 {84, 56, 28, 255},  {196, 152, 92, 255}};
        case THEME_CITY:    return (LiquidStyle){LQ_FLOOD, {16, 24, 40, 255}, {46, 66, 92, 255},
                                                 {150, 164, 180, 255}, {110, 76, 46, 255}};
        case THEME_FACTORY: return (LiquidStyle){LQ_CHEM,  {22, 14, 30, 255}, {56, 36, 70, 255},
                                                 {120, 240, 90, 255}, {200, 90, 220, 255}};
        case THEME_WASTELAND:
        default:            return (LiquidStyle){LQ_TOXIC, {20, 36, 20, 255}, {62, 92, 42, 255},
                                                 {160, 170, 66, 255}, {190, 200, 90, 255}};
    }
}

/* Champ « liquide » en (ax, ay) px d'art : interpolation bilinéaire de
   l'indicateur d'eau aux CENTRES de tuiles → 0,5 sur les frontières. */
static float liquid_field(const Map *m, float ax, float ay) {
    float fx = ax / TB_TILE - 0.5f, fy = ay / TB_TILE - 0.5f;
    int   x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float u  = fx - (float)x0, v = fy - (float)y0;
    float a = tt(m, x0, y0) == TILE_WATER,     b = tt(m, x0 + 1, y0) == TILE_WATER;
    float c = tt(m, x0, y0 + 1) == TILE_WATER, d = tt(m, x0 + 1, y0 + 1) == TILE_WATER;
    return (a + (b - a) * u) * (1.0f - v) + (c + (d - c) * u) * v;
}

static int near_liquid(const Map *m, int tx, int ty) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (tx + dx >= 0 && ty + dy >= 0 && tx + dx < m->w && ty + dy < m->h &&
                m->tiles[ty + dy][tx + dx].type == TILE_WATER) return 1;
    return 0;
}

static void paint_liquids(Image *im, const Map *m, unsigned int s) {
    const LiquidStyle L = liquid_of(m->theme);
    Color r[TB_RAMP];
    for (int i = 0; i < TB_RAMP; i++)
        r[i] = (i < TB_RAMP - 1) ? tb_mix(L.deep, L.shallow, (float)i / (TB_RAMP - 2))
                                 : tb_mix(L.shallow, L.rim, 0.45f);
    for (int y = 0; y < im->height; y++) {
        for (int x = 0; x < im->width; x++) {
            int tx = x / TB_TILE, ty = y / TB_TILE;
            if (!near_liquid(m, tx, ty)) continue;
            float f = liquid_field(m, x + 0.5f, y + 0.5f) +
                      (tb_fbm(x / 7.0f, y / 7.0f, s + 71u, 2) - 0.5f) * TB_LIQ_NOISE;
            if (f <= 0.5f - TB_WET) continue;
            if (f <= 0.5f) {                                      // sol mouillé
                float k = (f - (0.5f - TB_WET)) / TB_WET;
                Color o = tb_get(im, x, y);
                /* Bord humide ; autour des sables mouvants, sable tassé plus sombre. */
                tb_put(im, x, y, tb_scale(o, 1.0f - (L.kind == LQ_SAND ? 0.25f : 0.45f) * k));
                if (L.kind == LQ_CHEM && k > 0.6f) tb_blend(im, x, y, L.rim, 0.18f);  // halo néon
                continue;
            }
            float d = (f - 0.5f) / 0.35f;                         // profondeur 0..1
            if (d > 1.0f) d = 1.0f;
            float v = 0.92f - 0.72f * d + (tb_fbm(x / 9.0f, y / 9.0f, s + 73u, 2) - 0.5f) * 0.30f;
            if (L.kind != LQ_SAND && tb_fbm(x / 16.0f, y / 3.5f, s + 75u, 2) > 0.70f)
                v += 0.26f;                                       // reflets étirés
            Color c;
            if (f < 0.5f + TB_LIQ_RIM) {                          // liseré de rive
                c = (L.kind == LQ_SAND) ? L.rim : tb_mix(L.rim, r[4], 0.35f);
            } else if (L.kind == LQ_SAND) {                       // spirale de sable mouvant
                float cx = tx * TB_TILE + TB_TILE * 0.5f, cy = ty * TB_TILE + TB_TILE * 0.5f;
                float ddx = x + 0.5f - cx, ddy = y + 0.5f - cy;
                float sp = fmodf(sqrtf(ddx * ddx + ddy * ddy) * 0.8f + atan2f(ddy, ddx) * 2.0f + 40.0f, 3.2f);
                if (sp < 0.9f) v -= 0.30f;
                c = tb_ramp(r, v, x, y);
            } else {
                c = tb_ramp(r, v, x, y);
                float n = tb_fbm(x / 6.0f + 9.0f, y / 6.0f, s + 77u, 2);
                if (L.kind == LQ_TOXIC && n > 0.68f) c = tb_mix(c, L.extra, 0.55f);   // écume
                if (L.kind == LQ_CHEM) {                           // irisation
                    if (n > 0.60f && n < 0.64f) c = tb_mix(c, L.extra, 0.6f);
                    else if (n >= 0.64f && n < 0.67f) c = tb_mix(c, (Color){70, 200, 200, 255}, 0.5f);
                }
            }
            tb_put(im, x, y, c);
        }
    }
    /* Ornements par tuile : nénuphars, bulles, débris flottants. */
    for (int ty = 0; ty < m->h; ty++)
        for (int tx = 0; tx < m->w; tx++) {
            if (m->tiles[ty][tx].type != TILE_WATER) continue;
            unsigned h = tb_hash(tx, ty, s + 79u);
            int cx = tx * TB_TILE + 6 + (int)(h % 8u), cy = ty * TB_TILE + 6 + (int)((h >> 5) % 8u);
            if (L.kind == LQ_MARSH && (h & 1)) {                   // nénuphar échancré
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        if (dx * dx + dy * dy <= 4 && !(dx > 0 && dy == 0))
                            tb_put(im, cx + dx, cy + dy, (dx + dy < 0) ? tb_scale(L.extra, 1.3f) : L.extra);
                tb_put(im, cx - 1, cy - 2, (Color){236, 150, 190, 255});   // fleur
            } else if (L.kind == LQ_TOXIC || L.kind == LQ_CHEM) {  // bulles figées
                tb_put(im, cx, cy, L.rim); tb_put(im, cx + 1, cy - 1, tb_scale(L.rim, 0.7f));
                tb_put(im, cx + 3, cy + 2, L.rim);
            } else if (L.kind == LQ_FLOOD && (h & 3) == 0) {       // planche à la dérive
                for (int k = 0; k < 5; k++) tb_put(im, cx + k, cy, k ? L.extra : tb_scale(L.extra, 1.3f));
                tb_put(im, cx + 1, cy + 1, (Color){8, 12, 20, 255});
            }
        }
}

// ════════════════════════════════════════════════════════════════
// ROUTES
// ════════════════════════════════════════════════════════════════
/* Distance (px d'art) au réseau de route + coordonnée LE LONG de la route. */
static float road_dist(const Map *m, float px, float py, float *along) {
    int tx = (int)(px / TB_TILE), ty = (int)(py / TB_TILE);
    float best = 1e9f;
    *along = px + py;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = tx + dx, cy = ty + dy;
            if (!is_road(m, cx, cy)) continue;
            float ax = cx * TB_TILE + TB_TILE * 0.5f, ay = cy * TB_TILE + TB_TILE * 0.5f;
            float dd = hypotf(px - ax, py - ay);
            if (dd < best) best = dd;
            static const int NX[4] = {1, -1, 0, 0}, NY[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                if (!is_road(m, cx + NX[k], cy + NY[k])) continue;
                if (NX[k]) {                                       // segment horizontal
                    float x0 = ax, x1 = ax + NX[k] * TB_TILE;
                    if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
                    if (px >= x0 && px <= x1 && fabsf(py - ay) < best) { best = fabsf(py - ay); *along = px; }
                } else {                                           // segment vertical
                    float y0 = ay, y1 = ay + NY[k] * TB_TILE;
                    if (y0 > y1) { float t = y0; y0 = y1; y1 = t; }
                    if (py >= y0 && py <= y1 && fabsf(px - ax) < best) { best = fabsf(px - ax); *along = py; }
                }
            }
        }
    return best;
}

typedef struct { Color surf, edge, mark; } RoadStyle;
static RoadStyle road_of(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (RoadStyle){{122, 86, 50, 255},  {58, 40, 24, 255},  {150, 150, 156, 255}};
        case THEME_DESERT:  return (RoadStyle){{204, 166, 108, 255}, {150, 112, 64, 255}, {120, 90, 52, 255}};
        case THEME_CITY:    return (RoadStyle){{62, 64, 70, 255},   {156, 156, 160, 255}, {222, 190, 70, 255}};
        case THEME_FACTORY: return (RoadStyle){{104, 106, 112, 255}, {32, 28, 26, 255},  {226, 186, 44, 255}};
        case THEME_WASTELAND:
        default:            return (RoadStyle){{130, 96, 60, 255},  {66, 46, 28, 255},  {170, 136, 90, 255}};
    }
}

static Color road_pixel(ThemeID th, const RoadStyle *R, const Color rr[TB_RAMP],
                        int x, int y, float d, float along, float hw, unsigned int s) {
    float v = 0.55f + (tb_fbm(x / 5.0f, y / 5.0f, s + 81u, 2) - 0.5f) * 0.30f
                    + (tb_rand(x, y, s + 83u) - 0.5f) * 0.10f;
    switch (th) {
        case THEME_SWAMP: {                                       // passerelle en planches
            float a = fmodf(along + 400.0f, 4.0f);
            if (a < 0.9f) return R->edge;                         // jour entre planches
            v += (tb_rand((int)((along + 400.0f) / 4.0f), 7, s + 85u) - 0.5f) * 0.35f;
            if (d > hw - 1.4f) { v -= 0.28f; if (a > 1.6f && a < 2.6f) return R->mark; }  // clous
            if (d > hw - 2.4f && d <= hw - 1.4f) v += 0.12f;
        } break;
        case THEME_CITY:                                          // asphalte + marquage
            if (d > hw - 1.3f) return (d > hw - 0.6f) ? tb_scale(R->edge, 0.7f) : R->edge;  // bordure
            if (d > hw - 2.2f) v -= 0.20f;                        // ombre de bordure
            if (d < 0.75f && fmodf(along + 400.0f, 10.0f) < 5.0f) return R->mark;
            if (tb_fbm(x / 3.0f, y / 3.0f, s + 87u, 2) > 0.74f) v -= 0.30f;   // fissures
            break;
        case THEME_FACTORY: {                                     // tôle striée + bandes de danger
            if (d > hw - 1.7f)
                return (((int)((along + d + 400.0f) / 2.5f)) & 1) ? R->edge : R->mark;
            int ia = (int)(along + 400.0f), id = (int)(d * 1.0f + 100.0f);
            if ((ia % 3) == 0 && (id % 3) == 1) v += 0.30f;        // stries en relief
            else if ((ia % 3) == 1 && (id % 3) == 2) v -= 0.18f;
        } break;
        default:                                                  // piste : ornières
            if (fabsf(d - 3.2f) < 0.9f) v -= 0.24f;
            else if (d < 1.6f) v += 0.10f;                        // bombé central
            if (d > hw - 1.2f) v -= 0.22f;                        // bord émietté
            if (tb_rand(x, y, s + 89u) > 0.975f) return R->mark;  // gravillon clair
            break;
    }
    return tb_ramp(rr, v, x, y);
}

static void paint_roads(Image *im, const Map *m, unsigned int s) {
    const RoadStyle R = road_of(m->theme);
    Color rr[TB_RAMP];
    tb_make_ramp(R.surf, rr);
    for (int y = 0; y < im->height; y++)
        for (int x = 0; x < im->width; x++) {
            float along, d = road_dist(m, x + 0.5f, y + 0.5f, &along);
            if (d > TB_ROAD_HW + TB_ROAD_SHOULD + TB_ROAD_EDGE_N) continue;
            float hw = TB_ROAD_HW + (tb_fbm(x / 5.0f, y / 5.0f, s + 91u, 2) - 0.5f) * TB_ROAD_EDGE_N *
                                    (m->theme == THEME_CITY || m->theme == THEME_FACTORY ? 0.0f : 2.0f);
            if (d < hw) {
                tb_put(im, x, y, road_pixel(m->theme, &R, rr, x, y, d, along, hw, s));
            } else if (d < hw + 1.0f) {                           // contour sombre (pixel art)
                tb_blend(im, x, y, (Color){10, 8, 6, 255}, 0.55f);
            } else if (d < hw + TB_ROAD_SHOULD) {                 // bas-côté tassé
                tb_blend(im, x, y, (Color){0, 0, 0, 255}, 0.18f);
                if (tb_rand(x, y, s + 93u) > 0.86f) tb_put(im, x, y, rr[4]);
            }
        }
}

// ════════════════════════════════════════════════════════════════
// CUISSON + CACHE
// ════════════════════════════════════════════════════════════════
static Texture2D bake(const Map *m) {
    Image im = GenImageColor(m->w * TB_TILE, m->h * TB_TILE, BLANK);
    unsigned s = (unsigned)m->seed * 2654435761u ^ ((unsigned)m->theme * 40503u);
    tb_paint_ground(&im, m, s);
    tb_paint_details(&im, m, s);
    tb_paint_ruin_floor(&im, m, s);
    paint_liquids(&im, m, s);
    paint_roads(&im, m, s);
    Texture2D tex = LoadTextureFromImage(im);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(im);
    return tex;
}

static const TerrainSlot *slot_for(const Map *m) {
    unsigned key = map_key(m);
    TerrainSlot *lru = &g_slots[0];
    for (int i = 0; i < TB_CACHE; i++) {
        if (g_slots[i].used && g_slots[i].key == key) { g_slots[i].stamp = ++g_stamp; return &g_slots[i]; }
        if (!g_slots[i].used || g_slots[i].stamp < lru->stamp) lru = &g_slots[i];
    }
    if (lru->used) UnloadTexture(lru->tex);
    lru->tex = bake(m);
    lru->used = 1; lru->key = key; lru->stamp = ++g_stamp;
    return lru;
}

void terrain_draw_base(const Map *map) {
    if (!map || map->w <= 0 || map->h <= 0) return;
    const TerrainSlot *sl = slot_for(map);
    DrawTexturePro(sl->tex, (Rectangle){0, 0, (float)sl->tex.width, (float)sl->tex.height},
                   (Rectangle){0, 0, (float)(map->w * TILE_SIZE), (float)(map->h * TILE_SIZE)},
                   (Vector2){0, 0}, 0.0f, WHITE);
}

void terrain_draw_liquid_fx(const Map *map, int tx, int ty, float t) {
    const LiquidStyle L = liquid_of(map->theme);
    unsigned h = tb_hash(tx, ty, 0xF00Du);
    float cx = (tx + 0.5f) * TILE_SIZE, cy = (ty + 0.5f) * TILE_SIZE;
    if (L.kind == LQ_SAND) {                                      // remous qui s'enfonce
        float ph = fmodf(t * 0.35f + (float)(h & 15) / 15.0f, 1.0f);
        DrawCircleLines((int)cx, (int)cy, (1.0f - ph) * TILE_SIZE * 0.36f,
                        (Color){70, 46, 22, (unsigned char)(160.0f * ph)});
        return;
    }
    for (int k = 0; k < 3; k++) {                                 // reflets qui dérivent
        float ph = t * (0.6f + 0.15f * k) + (float)((h >> (k * 4)) & 15);
        float ox = fmodf(ph * 4.0f, (float)(TILE_SIZE - 16)) - (TILE_SIZE - 16) * 0.5f;
        float oy = ((float)((h >> (k * 5 + 2)) & 7) - 3.5f) * 2.0f;
        unsigned char a = (unsigned char)(70.0f + 90.0f * (0.5f + 0.5f * sinf(ph * 2.1f)));
        DrawRectangle((int)(cx + ox) & ~1, (int)(cy + oy) & ~1, TB_PX * 2, TB_PX,
                      (Color){L.rim.r, L.rim.g, L.rim.b, a});
    }
}

void terrain_unload(void) {
    for (int i = 0; i < TB_CACHE; i++)
        if (g_slots[i].used) { UnloadTexture(g_slots[i].tex); g_slots[i].used = 0; }
    terrain_ruins_unload();
}

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  terrain_ore.c ─ Filons de minerai peints comme les ruines (pixel art).
 *
 *  Un filon n'est plus une tuile opaque posée SUR le décor : c'est un
 *  AFFLEUREMENT de la roche du thème (grès, béton, scories…) d'où jaillissent
 *  des cristaux facettés de la couleur du minerai, avec le contour d'1 px et
 *  l'ombre portée des ruines, dessiné à son rang de profondeur (render_map).
 *  Le sol autour est MINÉRALISÉ dans la cuisson du terrain
 *  (tb_paint_ore_floor) : terre assombrie au bord tramé, cailloux, paillettes.
 *
 *  États : ACTIF (cristaux + halo qui respire + scintillements), DORMANT
 *  (filon pas encore ouvert : pointes ternes prises dans la roche), ÉPUISÉ
 *  (fosse d'extraction, moignons de cristaux, éclats).
 *  Atlas par thème : colonnes = minerais, lignes = états + calque de halo.
 */
#include "terrain.h"
#include "terrain_internal.h"
#include "pixel_paint.h"
#include "renderer.h"            /* MATERIAL_COLORS (source unique) */
#include "../map/theme.h"
#include "../combat/material.h"

#define OR_W          TB_TILE              /* largeur du sprite (px d'art)        */
#define OR_H          (TB_TILE + 5)        /* 5 px de surplomb : cristaux dressés */
#define OR_GROUND     (OR_H - 3)           /* ligne de sol dans le sprite         */
#define OR_ROWS       (ORE_STATE_COUNT + 1)
#define OR_ROW_GLOW   ORE_STATE_COUNT      /* calque de halo (filon actif)        */
#define OR_OUTLINE    ((Color){16, 12, 10, 255})
#define OR_SHADOW_A   95
#define OR_GLOW_R     4                    /* rayon du halo (px d'art)            */
#define OR_PULSE_HZ   0.45f                /* respiration du halo (cycles/s)      */
#define OR_PULSE_MIN  0.30f                /* intensité mini du halo              */
#define OR_SPARK_T    1.6f                 /* période des scintillements (s)      */
#define OR_SPARK_DUR  0.30f                /* durée d'un scintillement (s)        */
#define OR_SPARK_TIPS 3                    /* pointes qui scintillent (3 grandes) */
#define OR_FLOOR_R    13.0f                /* rayon du sol minéralisé (px d'art)  */
#define OR_DULL       ((Color){96, 96, 104, 255})   /* gris des cristaux éteints */

static Texture2D g_atlas[THEME_COUNT];
static int       g_atlas_ok[THEME_COUNT];

// ════════════════════════════════════════════════════════════════
// ROCHE DU THÈME
// ════════════════════════════════════════════════════════════════
typedef struct { Color rock, accent; } OreRock;

static OreRock rock_of(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (OreRock){{ 92,  98,  80, 255}, { 84, 136,  52, 255}};  // pierre moussue
        case THEME_DESERT:  return (OreRock){{176, 128,  84, 255}, {204, 162, 100, 255}};  // grès + sable
        case THEME_CITY:    return (OreRock){{124, 126, 134, 255}, {150,  78,  40, 255}};  // béton + fers
        case THEME_FACTORY: return (OreRock){{ 86,  80,  82, 255}, {236, 128,  44, 255}};  // scories
        case THEME_WASTELAND:
        default:            return (OreRock){{122, 106,  92, 255}, {150, 142,  92, 255}};  // roche + lichen
    }
}

// ════════════════════════════════════════════════════════════════
// CRISTAUX
// ════════════════════════════════════════════════════════════════
/* Un cristal : base (x, base), hauteur, largeur, inclinaison de la pointe. */
typedef struct { float x, base, h, w, lean; } Shard;

/* Amas d'un filon ouvert — de l'arrière vers l'avant. Les 3 premiers sont
   les grands (ils scintillent). */
static const Shard ACTIVE_SHARDS[] = {
    {10.0f, 19.5f, 13.0f, 5.0f,  1.0f},
    { 6.0f, 19.0f,  9.0f, 4.0f, -2.0f},
    {14.5f, 19.0f, 10.0f, 4.0f,  2.0f},
    {12.5f, 20.0f,  6.0f, 3.0f,  1.0f},
    { 4.0f, 20.5f,  5.0f, 3.0f, -1.0f},
};
#define N_ACTIVE ((int)(sizeof(ACTIVE_SHARDS) / sizeof(ACTIVE_SHARDS[0])))

/* Silhouette par minerai : le fer sort en blocs trapus, la cryo en aiguilles. */
static const float SHARD_HK[MAT_COUNT] = {
    [MAT_IRON] = 0.72f, [MAT_ACID] = 1.00f, [MAT_PLASMA] = 1.10f, [MAT_CRYO] = 1.18f, [MAT_NANO] = 1.00f,
};
static const float SHARD_WK[MAT_COUNT] = {
    [MAT_IRON] = 1.30f, [MAT_ACID] = 1.00f, [MAT_PLASMA] = 0.92f, [MAT_CRYO] = 0.85f, [MAT_NANO] = 1.00f,
};

static Shard shard_of(Shard s, int mat) {
    s.h *= SHARD_HK[mat];
    s.w *= SHARD_WK[mat];
    return s;
}
static Vector2 shard_tip(Shard s) { return (Vector2){s.x + s.lean, s.base - s.h}; }

/* Prisme à deux faces : face gauche éclairée, face droite dans l'ombre,
   arête vive en haut à gauche, pointe presque blanche. */
static void shard(PxCanvas *p, Shard s, Color col) {
    float   hw  = s.w * 0.5f, sh = s.w * 0.55f;      // demi-largeur, épaule
    Vector2 tip = shard_tip(s);
    Vector2 shl = {s.x - hw + s.lean, s.base - s.h + sh};
    Vector2 shr = {s.x + hw + s.lean, s.base - s.h + sh};
    Vector2 L[4] = {{s.x - hw, s.base}, {s.x + 0.5f, s.base}, {tip.x + 0.5f, tip.y}, shl};
    Vector2 R[4] = {{s.x, s.base}, {s.x + hw, s.base}, shr, tip};
    pp_poly(p, R, 4, col, 0.38f);
    pp_poly(p, L, 4, col, 0.76f);
    Color r[TB_RAMP];
    tb_make_ramp(col, r);
    pp_line(p, (int)shl.x, (int)shl.y, (int)tip.x, (int)tip.y, r[TB_RAMP - 1]);   // arête vive
    pp_px(p, (int)tip.x, (int)tip.y, tb_mix(r[TB_RAMP - 1], (Color){255, 255, 255, 255}, 0.6f));
}

// ════════════════════════════════════════════════════════════════
// SPRITES
// ════════════════════════════════════════════════════════════════
/* Touche du thème sur la roche : mousse, sable, fer à béton, scorie, lichen. */
static void accent(PxCanvas *p, ThemeID th, const OreRock *R) {
    switch (th) {
        case THEME_SWAMP:
            for (int x = 3; x <= 17; x += 3) pp_px(p, x, 17 + (x & 1), R->accent);
            break;
        case THEME_DESERT: {                                    // congère de sable au pied
            Color r[TB_RAMP];
            tb_make_ramp(R->accent, r);
            for (int x = 1; x <= 8; x++)
                for (int y = OR_GROUND - 1 - (x > 2 && x < 7); y <= OR_GROUND; y++)
                    pp_px(p, x, y, tb_ramp(r, y == OR_GROUND - 1 ? 0.8f : 0.55f, x, y));
        } break;
        case THEME_CITY:
            pp_line(p, 16, 18, 18, 14, R->accent);              // fer à béton tordu
            break;
        case THEME_FACTORY:
            pp_px(p, 4, 19, R->accent); pp_px(p, 16, 20, tb_scale(R->accent, 0.8f));
            break;
        default:
            pp_px(p, 5, 18, R->accent); pp_px(p, 15, 19, R->accent); pp_px(p, 16, 19, tb_scale(R->accent, 0.8f));
            break;
    }
}

/* Contour sombre d'1 px + ombre portée au sol (comme les ruines). */
static void finish(PxCanvas *p) {
    pp_outline(p, OR_OUTLINE);
    for (int y = OR_GROUND - 1; y < OR_H; y++)
        for (int x = 0; x < OR_W; x++) {
            float dx = (x + 0.5f - (OR_W * 0.5f + 1.5f)) / 9.0f, dy = (y + 0.5f - (OR_GROUND + 0.5f)) / 2.3f;
            if (dx * dx + dy * dy <= 1.0f && pp_get(p, x, y).a == 0)
                pp_px(p, x, y, (Color){0, 0, 0, OR_SHADOW_A});
        }
}

static void paint_active(PxCanvas *p, ThemeID th, const OreRock *R, int mat) {
    Color mc = MATERIAL_COLORS[mat];
    pp_disc(p, 10.0f, 17.5f, 6.0f, tb_scale(R->rock, 0.85f), 0.42f);       // bloc arrière
    for (int k = 0; k < N_ACTIVE; k++) shard(p, shard_of(ACTIVE_SHARDS[k], mat), mc);
    pp_box(p, 7, 19, 7, 3, R->rock, 0.50f);                                 // gravats au pied
    pp_disc(p, 4.5f, 19.5f, 3.2f, R->rock, 0.56f);
    pp_disc(p, 15.5f, 20.0f, 3.0f, R->rock, 0.48f);
    pp_px(p, 8, 20, tb_scale(mc, 1.1f)); pp_px(p, 4, 20, mc);              // veines du minerai
    pp_px(p, 15, 21, tb_scale(mc, 0.9f));
    accent(p, th, R);
    finish(p);
}

static void paint_dormant(PxCanvas *p, ThemeID th, const OreRock *R, int mat) {
    Color dim = tb_scale(tb_mix(MATERIAL_COLORS[mat], OR_DULL, 0.55f), 0.80f);
    pp_disc(p, 10.0f, 18.0f, 7.0f, tb_scale(R->rock, 0.9f), 0.46f);        // roche qui l'enferme
    pp_disc(p, 4.5f, 19.5f, 3.4f, R->rock, 0.55f);
    pp_disc(p, 15.5f, 19.5f, 3.2f, R->rock, 0.50f);
    shard(p, shard_of((Shard){8.5f, 14.5f, 4.0f, 3.0f, -1.0f}, mat), dim);  // pointes qui percent
    shard(p, shard_of((Shard){12.0f, 15.0f, 5.0f, 3.0f, 1.0f}, mat), dim);
    pp_line(p, 6, 19, 9, 17, tb_scale(dim, 0.85f));                         // fissure veinée
    pp_line(p, 13, 18, 15, 20, tb_scale(dim, 0.85f));
    pp_box(p, 7, 20, 6, 2, R->rock, 0.50f);
    accent(p, th, R);
    finish(p);
}

static void paint_mined(PxCanvas *p, ThemeID th, const OreRock *R, int mat) {
    Color dull = tb_scale(tb_mix(MATERIAL_COLORS[mat], OR_DULL, 0.6f), 0.70f);
    pp_disc(p, 4.5f, 19.5f, 2.8f, R->rock, 0.52f);                          // rebord de déblais
    pp_disc(p, 15.5f, 19.5f, 3.0f, R->rock, 0.48f);
    pp_disc(p, 10.5f, 16.5f, 2.6f, tb_scale(R->rock, 0.85f), 0.44f);
    for (int y = 17; y <= OR_GROUND; y++)                                   // fosse d'extraction
        for (int x = 4; x <= 16; x++) {
            float dx = (x + 0.5f - 10.0f) / 5.0f, dy = (y + 0.5f - 19.8f) / 2.4f;
            float r = dx * dx + dy * dy;
            if (r > 1.0f) continue;
            pp_px(p, x, y, r < 0.45f ? (Color){12, 10, 9, 255}
                         : (dy < 0.0f ? (Color){26, 22, 19, 255} : tb_scale(R->rock, 0.55f)));
        }
    shard(p, shard_of((Shard){7.5f, 19.0f, 3.0f, 3.0f, 0.0f}, mat), dull);  // moignons
    shard(p, shard_of((Shard){13.0f, 20.0f, 2.0f, 2.0f, 0.0f}, mat), dull);
    pp_px(p, 2, 21, dull); pp_px(p, 18, 21, dull); pp_px(p, 6, 16, dull);   // éclats
    accent(p, th, R);
    finish(p);
}

/* Calque de halo : seulement la lueur AUTOUR des cristaux (la silhouette
   est effacée) — dessiné en mode additif, son intensité respire. */
static void paint_glow(PxCanvas *p, int mat) {
    Color mc = MATERIAL_COLORS[mat];
    for (int k = 0; k < N_ACTIVE; k++) shard(p, shard_of(ACTIVE_SHARDS[k], mat), mc);
    pp_glow(p, mc, OR_GLOW_R);
    for (int y = 0; y < OR_H; y++)
        for (int x = 0; x < OR_W; x++)
            if (pp_get(p, x, y).a == 255) pp_px(p, x, y, (Color){0, 0, 0, 0});
}

static void bake_atlas(ThemeID th) {
    Image   im = GenImageColor(OR_W * MAT_COUNT, OR_H * OR_ROWS, BLANK);
    OreRock R  = rock_of(th);
    for (int m = 0; m < MAT_COUNT; m++)
        for (int row = 0; row < OR_ROWS; row++) {
            PxCanvas p = {&im, m * OR_W, row * OR_H, OR_W, OR_H,
                          0x0E5Eu + (unsigned)th * 131u + (unsigned)m * 17u + (unsigned)row * 7u};
            if      (row == ORE_ACTIVE)  paint_active (&p, th, &R, m);
            else if (row == ORE_DORMANT) paint_dormant(&p, th, &R, m);
            else if (row == ORE_MINED)   paint_mined  (&p, th, &R, m);
            else                         paint_glow   (&p, m);
        }
    g_atlas[th] = LoadTextureFromImage(im);
    SetTextureFilter(g_atlas[th], TEXTURE_FILTER_POINT);
    UnloadImage(im);
    g_atlas_ok[th] = 1;
}

// ════════════════════════════════════════════════════════════════
// SOL MINÉRALISÉ (cuisson du terrain)
// ════════════════════════════════════════════════════════════════
void tb_paint_ore_floor(Image *im, const Map *m, unsigned int s) {
    Color rr[TB_RAMP];
    tb_make_ramp(rock_of(m->theme).rock, rr);
    for (int i = 0; i < m->deposit_count; i++) {
        const MaterialDeposit *d = &m->deposits[i];
        if ((int)d->type < 0 || (int)d->type >= MAT_COUNT) continue;
        Color mr[TB_RAMP];
        tb_make_ramp(MATERIAL_COLORS[d->type], mr);
        float cx = d->tile_x * TB_TILE + TB_TILE * 0.5f, cy = d->tile_y * TB_TILE + TB_TILE * 0.6f;
        int   r0 = (int)OR_FLOOR_R + 3;
        for (int y = (int)cy - r0; y <= (int)cy + r0; y++)
            for (int x = (int)cx - r0; x <= (int)cx + r0; x++) {
                float dx = x + 0.5f - cx, dy = (y + 0.5f - cy) * 1.2f;
                float dd = sqrtf(dx * dx + dy * dy) / OR_FLOOR_R
                         + (tb_fbm(x / 4.0f, y / 4.0f, s + 301u, 2) - 0.5f) * 0.5f;
                if (dd >= 1.0f) continue;
                float k = 1.0f - dd;                              // 1 au cœur → 0 au bord
                if (k * 0.85f > tb_bayer(x, y))                   // terre tassée, bord tramé
                    tb_put(im, x, y, tb_mix(tb_get(im, x, y), rr[1], 0.45f));
                float r = tb_rand(x, y, s + 303u);
                if (r > 0.95f - 0.08f * k)                        // cailloux
                    tb_put(im, x, y, rr[(r > 0.985f) ? 4 : 2]);
                else if (dd < 0.75f && r < 0.03f * k)             // paillettes de minerai
                    tb_put(im, x, y, mr[3 + (r < 0.012f)]);
            }
    }
}

// ════════════════════════════════════════════════════════════════
// DESSIN
// ════════════════════════════════════════════════════════════════
static ThemeID ensure_theme(ThemeID th) {
    if (th < 0 || th >= THEME_COUNT) th = THEME_WASTELAND;
    if (!g_atlas_ok[th]) bake_atlas(th);
    return th;
}

static void draw_cell(ThemeID th, int mat, int row, int flip, Rectangle dst, Color tint) {
    Rectangle src = {(float)(mat * OR_W), (float)(row * OR_H), (float)(flip ? -OR_W : OR_W), (float)OR_H};
    DrawTexturePro(g_atlas[th], src, dst, (Vector2){0, 0}, 0.0f, tint);
}

/* Halo qui respire + scintillement d'une des grandes pointes. `px` = taille
   d'un pixel d'art à l'écran ; `seed` décale les phases d'un filon à l'autre. */
static void active_fx(ThemeID th, int mat, int flip, Rectangle dst, float t,
                      unsigned seed, unsigned char alpha) {
    float px    = dst.width / (float)OR_W;
    float pulse = OR_PULSE_MIN + (1.0f - OR_PULSE_MIN) *
                  (0.5f + 0.5f * sinf((t * OR_PULSE_HZ + (float)(seed & 255u) / 255.0f) * 6.2832f));
    BeginBlendMode(BLEND_ADDITIVE);
    draw_cell(th, mat, OR_ROW_GLOW, flip, dst,
              (Color){255, 255, 255, (unsigned char)(pulse * (float)alpha)});
    EndBlendMode();

    float ph    = t + (float)((seed >> 8) & 255u) / 255.0f * OR_SPARK_T;
    int   cycle = (int)floorf(ph / OR_SPARK_T);
    float local = ph - (float)cycle * OR_SPARK_T;
    if (local >= OR_SPARK_DUR) return;
    Vector2 tip = shard_tip(shard_of(ACTIVE_SHARDS[tb_hash(cycle, 7, seed) % OR_SPARK_TIPS], mat));
    float   tx  = flip ? (float)OR_W - 1.0f - tip.x : tip.x;
    float   a   = sinf(3.14159f * local / OR_SPARK_DUR) * (float)alpha / 255.0f;
    float   cx  = dst.x + tx * px, cy = dst.y + tip.y * px;
    Color   arm = tb_mix(MATERIAL_COLORS[mat], (Color){255, 255, 255, 255}, 0.6f);
    arm.a = (unsigned char)(a * 200.0f);
    DrawRectangleV((Vector2){cx - px, cy}, (Vector2){px * 3.0f, px}, arm);       // branches
    DrawRectangleV((Vector2){cx, cy - px}, (Vector2){px, px * 3.0f}, arm);
    DrawRectangleV((Vector2){cx, cy}, (Vector2){px, px}, (Color){255, 255, 255, (unsigned char)(a * 255.0f)});
}

void terrain_draw_ore(ThemeID th, int tx, int ty, int mat, OreState st, float t) {
    if (mat < 0 || mat >= MAT_COUNT) return;
    if ((int)st < 0 || (int)st >= ORE_STATE_COUNT) st = ORE_DORMANT;
    th = ensure_theme(th);
    unsigned  h    = tb_hash(tx, ty, 0x0E5Eu);
    int       flip = (int)((h >> 4) & 1u);                  // miroir : plus de variété
    Rectangle dst  = {(float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE - (OR_H - TB_TILE) * TB_PX),
                      (float)(OR_W * TB_PX), (float)(OR_H * TB_PX)};
    draw_cell(th, mat, st, flip, dst, WHITE);
    if (st == ORE_ACTIVE) active_fx(th, mat, flip, dst, t, h, 255);
}

void terrain_draw_ore_icon(Rectangle dst, int mat, float t, unsigned char alpha) {
    if (mat < 0 || mat >= MAT_COUNT) return;
    ThemeID th = ensure_theme(THEME_WASTELAND);
    float k = floorf(fminf(dst.width / OR_W, dst.height / OR_H));   // échelle entière : net
    if (k < 1.0f) k = 1.0f;
    Rectangle r = {dst.x + (dst.width - OR_W * k) * 0.5f, dst.y + (dst.height - OR_H * k) * 0.5f,
                   OR_W * k, OR_H * k};
    draw_cell(th, mat, ORE_ACTIVE, 0, r, (Color){255, 255, 255, alpha});
    active_fx(th, mat, 0, r, t, 0x5Au, alpha);
}

void terrain_ore_unload(void) {
    for (int i = 0; i < THEME_COUNT; i++)
        if (g_atlas_ok[i]) { UnloadTexture(g_atlas[i]); g_atlas_ok[i] = 0; }
}

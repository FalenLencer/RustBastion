/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  terrain_pad.c ─ Socle peint sous chaque tour : la tour s'ANCRE au décor.
 *
 *  Par thème, PAD_VARIANTS socles de PAD_N × PAD_N pixels d'art (même grain
 *  que le sol : 1 px d'art = TB_PX px monde). De l'extérieur vers le centre :
 *    1. sol TASSÉ (rampe du sol du thème, assombrie) au bord irrégulier et
 *       TRAMÉ (Bayer) → il se fond dans le terrain au lieu d'un tampon net ;
 *    2. dalle de fondation propre au thème (béton fissuré, plancher sur
 *       pilotis, dalles de grès, béton à bande de danger, tôle rivetée),
 *       éclairée en haut à gauche avec un biseau ;
 *    3. pierres / blocs ombrés au pourtour (contour sombre d'1 px).
 *  Le socle déborde un peu de sa case (≈ 1,4 case) : la transition se fait
 *  sur le sol voisin, comme les rives et les gravats.
 */
#include "terrain.h"
#include "terrain_internal.h"
#include "pixel_paint.h"
#include "../map/theme.h"

#define PAD_N          28        // côté du socle (px d'art) → 56 px monde
#define PAD_VARIANTS   3
#define PAD_R_EDGE     12.5f     // rayon moyen du sol tassé (px d'art)
#define PAD_EDGE_NOISE 0.30f     // irrégularité du bord (part du rayon)
#define PAD_EDGE_FADE  3.0f      // largeur du tramage de bord (px d'art)
#define PAD_R_SLAB     6.5f      // demi-largeur de la dalle (px d'art)
#define PAD_STONES     8         // cailloux au pourtour
#define PAD_STONE_MIN  0.7f      // rayon des cailloux (px d'art)
#define PAD_STONE_VAR  0.7f

static Texture2D g_pad[THEME_COUNT];
static int       g_pad_ok[THEME_COUNT];

/* Matière de la dalle par thème (teinte moyenne → rampe à 6 tons). */
static Color slab_color(ThemeID th) {
    switch (th) {
        case THEME_SWAMP:   return (Color){118,  86,  52, 255};   // planches
        case THEME_DESERT:  return (Color){198, 160, 108, 255};   // grès
        case THEME_CITY:    return (Color){136, 136, 140, 255};   // béton
        case THEME_FACTORY: return (Color){104, 110, 118, 255};   // tôle
        case THEME_WASTELAND:
        default:            return (Color){132, 120, 104, 255};   // béton sale
    }
}

/* Forme de la dalle : octogone (coins plus ou moins abattus selon la
   matière : tôle presque carrée, béton/grès/plancher bien arrondis). */
static int in_slab(ThemeID th, float dx, float dy) {
    float ax = fabsf(dx), ay = fabsf(dy);
    if (ax > PAD_R_SLAB || ay > PAD_R_SLAB) return 0;
    return ax + ay <= PAD_R_SLAB * ((th == THEME_FACTORY) ? 1.65f : 1.38f);
}

/* Joint / motif interne de la dalle (1 = trait sombre). */
static int slab_seam(ThemeID th, int x, int y, float dx, float dy, unsigned s) {
    switch (th) {
        case THEME_SWAMP:   return (y % 3) == 0;                         // planches
        case THEME_DESERT:                                               // dalles en quinconce
            return ((x + (y / 4) * 2) % 4) == 0 || (y % 4) == 0;
        case THEME_FACTORY: return ((x + y) % 4 == 0) && ((x - y) % 4 == 0)
                                   && fabsf(dx) < PAD_R_SLAB - 1.5f;    // tôle larmée
        default:
            /* béton : quelques fissures en zigzag */
            return tb_rand(x / 2, y, s + 5u) > 0.93f && fabsf(dx - dy) < 3.0f;
    }
}

static void paint_pad(Image *im, int ox, ThemeID th, unsigned s) {
    Color gr[TB_RAMP], sl[TB_RAMP];
    tb_ground_ramp(th, gr);
    tb_make_ramp(slab_color(th), sl);
    const float c = PAD_N * 0.5f - 0.5f;

    /* 1+2. Sol tassé puis dalle */
    for (int y = 0; y < PAD_N; y++)
        for (int x = 0; x < PAD_N; x++) {
            float dx = x - c, dy = y - c, r = sqrtf(dx * dx + dy * dy);
            float n = tb_fbm(x * 0.32f, y * 0.32f, s, 3);
            float edge = PAD_R_EDGE * (1.0f - PAD_EDGE_NOISE * 0.5f + PAD_EDGE_NOISE * n);
            if (r >= edge) continue;
            float rim = (edge - r) / PAD_EDGE_FADE;              // 0 au bord → 1 dedans
            if (rim < 1.0f && rim < tb_bayer(x, y)) continue;     // bord tramé
            if (in_slab(th, dx, dy)) {
                float lit = 0.50f - 0.18f * (dx + dy) / (2.0f * PAD_R_SLAB)
                          + 0.14f * (tb_fbm(x * 0.7f, y * 0.7f, s + 31u, 2) - 0.5f);
                int bev_hi = in_slab(th, dx, dy) && !in_slab(th, dx - 1.0f, dy - 1.0f);
                int bev_lo = !in_slab(th, dx + 1.0f, dy + 1.0f);
                if (bev_hi) lit += 0.22f;
                if (bev_lo) lit -= 0.30f;
                if (slab_seam(th, x, y, dx, dy, s)) lit -= 0.24f;
                tb_put(im, ox + x, y, tb_ramp(sl, lit, x, y));
            } else {
                /* terre tassée : plus sombre que le sol, ombrée vers le SE,
                   un peu plus sombre au pied de la dalle (piétinée) */
                float v = 0.16f + 0.30f * tb_fbm(x * 0.55f, y * 0.55f, s + 11u, 2)
                        - 0.08f * (dx + dy) / PAD_R_EDGE + 0.10f * (r / PAD_R_EDGE);
                tb_put(im, ox + x, y, tb_ramp(gr, v, x, y));
            }
        }

    /* Bande de danger (ville) : arc jaune/noir au bas de la dalle */
    if (th == THEME_CITY)
        for (int x = (int)(c - 4); x <= (int)(c + 4); x++) {
            int y = (int)(c + PAD_R_SLAB - 2.0f);
            tb_put(im, ox + x, y, ((x / 2) & 1) ? (Color){36, 32, 28, 255} : (Color){222, 184, 52, 255});
        }

    /* 3. Cailloux au pourtour : petite ombre portée SE (ton le plus sombre du
       sol) puis caillou éclairé — pas de contour noir (trop « posé »). */
    PxCanvas p = {im, ox, 0, PAD_N, PAD_N, s};
    Color stone = (th == THEME_SWAMP) ? (Color){104, 98, 82, 255} : tb_scale(slab_color(th), 0.92f);
    for (int k = 0; k < PAD_STONES; k++) {
        float a  = (k + 0.45f * tb_rand(k, 1, s)) * 6.2832f / PAD_STONES;
        float rr = PAD_R_SLAB + 1.8f + 2.6f * tb_rand(k, 2, s);
        float sx = c + cosf(a) * rr, sy = c + sinf(a) * rr;
        float sz = PAD_STONE_MIN + PAD_STONE_VAR * tb_rand(k, 3, s);
        pp_disc(&p, sx + 0.8f, sy + 0.8f, sz, gr[0], 0.0f);
        pp_disc(&p, sx, sy, sz, stone, 0.35f);
    }
}

static void bake_pads(ThemeID th) {
    Image im = GenImageColor(PAD_N * PAD_VARIANTS, PAD_N, BLANK);
    for (int v = 0; v < PAD_VARIANTS; v++)
        paint_pad(&im, v * PAD_N, th, 0xBA5Eu + (unsigned)th * 97u + (unsigned)v * 13u);
    g_pad[th] = LoadTextureFromImage(im);
    SetTextureFilter(g_pad[th], TEXTURE_FILTER_POINT);
    UnloadImage(im);
    g_pad_ok[th] = 1;
}

void terrain_draw_tower_pad(ThemeID th, float cx, float cy, float scale,
                            float alpha, unsigned key) {
    if (th < 0 || th >= THEME_COUNT) th = THEME_WASTELAND;
    if (!g_pad_ok[th]) bake_pads(th);
    if (scale <= 0.0f || alpha <= 0.0f) return;
    int   v    = (int)(key % PAD_VARIANTS);
    int   flip = (int)((key >> 4) & 1u);                 // miroir : plus de variété
    float sz   = PAD_N * TB_PX * scale;
    Rectangle src = {(float)(v * PAD_N), 0, (float)(flip ? -PAD_N : PAD_N), (float)PAD_N};
    Rectangle dst = {cx - sz * 0.5f, cy - sz * 0.5f, sz, sz};
    unsigned char a = (unsigned char)(255.0f * (alpha > 1.0f ? 1.0f : alpha));
    DrawTexturePro(g_pad[th], src, dst, (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, a});
}

void terrain_pad_unload(void) {
    for (int i = 0; i < THEME_COUNT; i++)
        if (g_pad_ok[i]) { UnloadTexture(g_pad[i]); g_pad_ok[i] = 0; }
}

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  terrain_internal.h ─ Outils de PEINTURE PIXEL des terrains (interne aux
 *  terrain*.c) : bruit de valeur, fbm, tramage de Bayer, rampes de couleur
 *  à décalage de teinte (ombres froides, lumières chaudes — la recette des
 *  splash arts), accès pixel sur une Image RGBA.
 *
 *  Tout est DÉTERMINISTE (hachage de coordonnées + graine) : une carte donne
 *  toujours la même peinture, et le tirage aléatoire du jeu n'est pas touché.
 */
#include "raylib.h"
#include "../map/map_gen.h"
#include <math.h>

#define TB_PX     2                       // px monde par pixel d'art
#define TB_TILE   (TILE_SIZE / TB_PX)     // pixels d'art par tuile (20)
#define TB_RAMP   6                       // tons par rampe de matière

// ── Hachage / bruit ───────────────────────────────────────────────
static inline unsigned int tb_hash(int x, int y, unsigned int s) {
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ s * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static inline float tb_rand(int x, int y, unsigned int s) {
    return (float)(tb_hash(x, y, s) & 0xffffu) / 65535.0f;
}
/* Bruit de valeur lissé (interpolation smoothstep), sortie 0..1. */
static inline float tb_noise(float x, float y, unsigned int s) {
    int   ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = tb_rand(ix, iy, s),     b = tb_rand(ix + 1, iy, s);
    float c = tb_rand(ix, iy + 1, s), d = tb_rand(ix + 1, iy + 1, s);
    return (a + (b - a) * fx) * (1.0f - fy) + (c + (d - c) * fx) * fy;
}
/* Bruit fractal (octaves), sortie ~0..1. */
static inline float tb_fbm(float x, float y, unsigned int s, int oct) {
    float v = 0.0f, amp = 0.5f, tot = 0.0f;
    for (int i = 0; i < oct; i++) {
        v += tb_noise(x, y, s + (unsigned int)i * 977u) * amp;
        tot += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
    }
    return v / tot;
}
/* Seuil de Bayer 4×4 normalisé (0..1) : tramage ordonné des dégradés. */
static inline float tb_bayer(int x, int y) {
    static const unsigned char B[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6},
                                          {3, 11, 1, 9}, {15, 7, 13, 5}};
    return ((float)B[y & 3][x & 3] + 0.5f) / 16.0f;
}

// ── Couleurs ──────────────────────────────────────────────────────
static inline Color tb_mix(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                   (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
static inline Color tb_scale(Color c, float k) {
    float r = c.r * k, g = c.g * k, b = c.b * k;
    return (Color){(unsigned char)(r > 255 ? 255 : r), (unsigned char)(g > 255 ? 255 : g),
                   (unsigned char)(b > 255 ? 255 : b), c.a};
}
/* Rampe à décalage de teinte autour d'une couleur moyenne : les ombres
   glissent vers le froid (bleu-violet), les lumières vers le chaud. */
static inline void tb_make_ramp(Color base, Color out[TB_RAMP]) {
    const Color cold = {40, 36, 70, 255}, warm = {255, 236, 190, 255};
    for (int i = 0; i < TB_RAMP; i++) {
        float t = (float)i / (TB_RAMP - 1);          // 0 = ombre, 1 = lumière
        Color c = tb_scale(base, 0.45f + 1.05f * t);
        out[i] = (t < 0.5f) ? tb_mix(c, cold, (0.5f - t) * 0.45f)
                            : tb_mix(c, warm, (t - 0.5f) * 0.30f);
    }
}
/* Ton de la rampe pour v ∈ [0,1], tramé entre deux tons voisins. */
static inline Color tb_ramp(const Color r[TB_RAMP], float v, int x, int y) {
    if (v < 0.0f) v = 0.0f;
    if (v > 0.999f) v = 0.999f;
    float f = v * (TB_RAMP - 1);
    int   i = (int)f;
    return (f - (float)i > tb_bayer(x, y) && i + 1 < TB_RAMP) ? r[i + 1] : r[i];
}

// ── Accès pixel (Image RGBA 8 bits) ───────────────────────────────
static inline void tb_put(Image *im, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= im->width || y >= im->height) return;
    ((Color *)im->data)[y * im->width + x] = c;
}
static inline Color tb_get(const Image *im, int x, int y) {
    if (x < 0 || y < 0 || x >= im->width || y >= im->height) return (Color){0, 0, 0, 0};
    return ((const Color *)im->data)[y * im->width + x];
}
/* Mélange alpha d'une couleur par-dessus le pixel existant. */
static inline void tb_blend(Image *im, int x, int y, Color c, float a) {
    if (x < 0 || y < 0 || x >= im->width || y >= im->height) return;
    Color *p = &((Color *)im->data)[y * im->width + x];
    Color  o = tb_mix(*p, c, a);
    o.a = p->a > c.a ? p->a : c.a;
    *p = o;
}

// ── Peintres (terrain_ground.c / terrain_ruins.c) ─────────────────
/* Sol du thème sur toute l'image (1 pixel d'art = TB_PX px monde). */
void tb_paint_ground(Image *im, const Map *map, unsigned int seed);
/* Détails semés sur le sol constructible (galets, fissures, touffes…). */
void tb_paint_details(Image *im, const Map *map, unsigned int seed);
/* Sol des ruines : amas de gravats / broussailles sous les sprites. */
void tb_paint_ruin_floor(Image *im, const Map *map, unsigned int seed);
/* Sol minéralisé autour des filons (terrain_ore.c). */
void tb_paint_ore_floor(Image *im, const Map *map, unsigned int seed);
/* Rampe de sol du thème (partagée : rives, bas-côtés). */
void tb_ground_ramp(ThemeID th, Color out[TB_RAMP]);

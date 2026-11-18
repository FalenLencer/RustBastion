/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_portraits_paint.c ─ Bustes peints des personnages du récit.
 *
 *  Pixel art « de portrait » : bustes de TROIS-QUARTS tournés vers le texte,
 *  volumes ombrés en APLATS sur des rampes à décalage de teinte (ombres
 *  froides, lumières chaudes — recette des splash arts du jeu), lumière
 *  principale en haut à gauche. Le liseré coloré du personnage, le contour
 *  et le fond sont posés par story_portraits.c.
 *  Un accessoire distinctif par personnage : casquette et cicatrice de Voss,
 *  lunettes relevées sur le casque de Kira, lunettes de soudure et chignon de
 *  Varela, heaume à cimier de l'Acier, œil de NEXUS, casquette d'officier sur
 *  crâne de tôle du général machine, casque-micro de l'opérateur radio.
 */
#include "story_portraits_internal.h"
#include "terrain_internal.h"   /* tb_make_ramp / tb_mix / tb_scale (source unique) */
#include <math.h>

#define R6   TB_RAMP
static Image *g_L;               /* calque peint (SP_ART × SP_ART) */

// ════════════════════════════════════════════════════════════════
// PRIMITIVES (coordonnées en px d'art, y vers le bas)
// ════════════════════════════════════════════════════════════════
static void px(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= SP_ART || y >= SP_ART) return;
    ((Color *)g_L->data)[y * SP_ART + x] = c;
}
static Color band(const Color r[R6], float v) {
    int i = (int)floorf(v * (R6 - 1) + 0.5f);
    return r[i < 0 ? 0 : i >= R6 ? R6 - 1 : i];
}
static void ramp(Color base, Color out[R6]) { tb_make_ramp(base, out); }

/* Ellipse ombrée comme un volume rond (lumière haut-gauche). */
static void ell(float cx, float cy, float rx, float ry, const Color r[R6], float b) {
    for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++)
        for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
            float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry, d = nx * nx + ny * ny;
            if (d > 1.0f) continue;
            float v = b + 0.40f * (-0.55f * nx - 0.70f * ny) + 0.12f * sqrtf(1.0f - d) - 0.06f;
            px(x, y, band(r, v));
        }
}
static void disc(float cx, float cy, float r, Color c) {
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
        for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) px(x, y, c);
        }
}

static int inside(const Vector2 *p, int n, float x, float y) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if (((p[i].y > y) != (p[j].y > y)) &&
            (x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x))
            in = !in;
    return in;
}
/* Polygone ombré par un dégradé linéaire (gx gauche→droite, gy haut→bas). */
static void poly(const Vector2 *p, int n, const Color r[R6], float b, float gx, float gy) {
    float x0 = p[0].x, x1 = p[0].x, y0 = p[0].y, y1 = p[0].y;
    for (int i = 1; i < n; i++) {
        x0 = fminf(x0, p[i].x); x1 = fmaxf(x1, p[i].x);
        y0 = fminf(y0, p[i].y); y1 = fmaxf(y1, p[i].y);
    }
    float w = fmaxf(x1 - x0, 1.0f), h = fmaxf(y1 - y0, 1.0f);
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x <= (int)ceilf(x1); x++) {
            if (!inside(p, n, x + 0.5f, y + 0.5f)) continue;
            float v = b + gx * ((x + 0.5f - x0) / w - 0.5f) + gy * ((y + 0.5f - y0) / h - 0.5f);
            px(x, y, band(r, v));
        }
}
static void flat(const Vector2 *p, int n, Color c) {
    Color r[R6];
    for (int i = 0; i < R6; i++) r[i] = c;
    poly(p, n, r, 0.5f, 0.0f, 0.0f);
}
/* Trait d'1 px (DDA). */
static void line(float x0, float y0, float x1, float y1, Color c) {
    float dx = x1 - x0, dy = y1 - y0;
    int   n  = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (n < 1) n = 1;
    for (int i = 0; i <= n; i++)
        px((int)floorf(x0 + dx * i / n), (int)floorf(y0 + dy * i / n), c);
}
#define PTS(...) ((const Vector2[]){__VA_ARGS__})
#define NPTS(...) ((int)(sizeof((Vector2[]){__VA_ARGS__}) / sizeof(Vector2)))
#define POLY(r, b, gx, gy, ...) poly(PTS(__VA_ARGS__), NPTS(__VA_ARGS__), r, b, gx, gy)
#define FLAT(c, ...)            flat(PTS(__VA_ARGS__), NPTS(__VA_ARGS__), c)

// ════════════════════════════════════════════════════════════════
// TRAITS PARTAGÉS
// ════════════════════════════════════════════════════════════════
#define C_EYE_W   ((Color){232, 226, 210, 255})
#define C_IRIS    ((Color){ 40,  30,  26, 255})
#define C_IRIS_L  ((Color){ 84,  62,  46, 255})   /* bas de l'iris (plus clair) */
#define C_MOUTH   ((Color){ 56,  26,  22, 255})
#define C_TEETH   ((Color){226, 216, 196, 255})
#define C_BLACK   ((Color){ 16,  13,  12, 255})

/* Yeux de trois-quarts : œil proche (3 px) en (nx, y), œil lointain (2 px)
   en (fx, y) ; regard vers la droite. Fermés : un trait de paupière. */
static void eyes34(int nx, int fx, int y, int blink, Color lid) {
    if (blink) {
        line((float)nx, (float)y, (float)nx + 2.0f, (float)y, lid);
        line((float)fx, (float)y, (float)fx + 1.0f, (float)y, lid);
        return;
    }
    line((float)nx - 0.5f, (float)y - 1.0f, (float)nx + 2.0f, (float)y - 1.0f, lid);   // paupière haute
    px(nx, y, C_EYE_W); px(nx + 1, y, C_IRIS); px(nx + 2, y, tb_scale(C_EYE_W, 0.8f));
    px(nx + 1, y + 1, C_IRIS_L);                                                       // iris sur 2 px
    line((float)fx, (float)y - 1.0f, (float)fx + 1.0f, (float)y - 1.0f, lid);
    px(fx, y, C_IRIS); px(fx, y + 1, C_IRIS_L); px(fx + 1, y, tb_scale(C_EYE_W, 0.75f));
}
/* Bouche (x0..x0+w-1, y) : trait fermé, ou ouverte (dents + ombre). */
static void mouth(int x0, int y, int w, int open, Color lip) {
    if (!open) { line((float)x0, (float)y, (float)(x0 + w - 1), (float)y, lip); return; }
    line((float)x0, (float)y, (float)(x0 + w - 1), (float)y, C_TEETH);
    line((float)x0, (float)y + 1.0f, (float)(x0 + w - 1), (float)y + 1.0f, C_MOUTH);
    px(x0 - 1, y, lip); px(x0 + w, y, lip);
}
/* Tête humaine de trois-quarts : oreille, crâne, mâchoire, cou, nez. */
static void head34(const Color sk[R6], float jaw_w, int female) {
    POLY(sk, 0.36f, -0.2f, 0.0f, {16.6f, 23.0f}, {23.4f, 23.0f}, {23.8f, 29.6f}, {16.2f, 29.6f});   // cou
    ell(12.7f, 18.4f, 1.7f, 2.5f, sk, 0.42f);                                                      // oreille
    px(13, 18, sk[1]);
    ell(19.6f, 15.8f, female ? 7.2f : 7.6f, female ? 8.3f : 8.6f, sk, 0.60f);                      // crâne
    POLY(sk, 0.56f, -0.30f, -0.15f, {12.4f, 17.0f}, {13.3f, 22.4f}, {16.9f, 26.6f + jaw_w * 0.4f},
         {22.2f, 27.2f + jaw_w * 0.4f}, {26.3f + jaw_w * 0.3f, 23.6f}, {27.4f, 18.0f});            // mâchoire
    POLY(sk, 0.68f, -0.45f, 0.0f, {24.6f, 15.6f}, {27.8f, 20.3f}, {26.6f, 21.2f}, {24.8f, 20.6f}); // nez
    px(25, 21, sk[1]);                                                                            // narine
    px(27, 21, sk[2]);                                                                            // ombre portée
}
/* Épaules / buste : vêtement ombré, lumière à gauche. */
static void bust(const Color r[R6], float b) {
    POLY(r, b, -0.40f, -0.25f, {1.0f, 40.0f}, {2.0f, 34.0f}, {7.0f, 30.4f}, {14.0f, 28.6f},
         {26.0f, 28.6f}, {33.0f, 30.4f}, {38.0f, 34.0f}, {39.0f, 40.0f});
}

// ════════════════════════════════════════════════════════════════
// PERSONNAGES
// ════════════════════════════════════════════════════════════════
static void paint_voss(int fl) {
    Color sk[R6], coat[R6], gr[R6], cap[R6];
    ramp((Color){205, 152, 116, 255}, sk);   ramp((Color){ 70,  60,  46, 255}, coat);
    ramp((Color){150, 150, 144, 255}, gr);   ramp((Color){ 60,  76,  50, 255}, cap);
    const Color amber = {236, 172, 48, 255}, scar = {150, 74, 62, 255};
    bust(coat, 0.48f);
    POLY(coat, 0.64f, -0.2f, -0.2f, {14.0f, 28.6f}, {19.5f, 34.0f}, {16.0f, 40.0f}, {10.0f, 40.0f}, {9.0f, 31.0f});  // revers
    POLY(coat, 0.40f, -0.2f, -0.2f, {26.0f, 28.6f}, {21.0f, 34.0f}, {23.5f, 40.0f}, {30.0f, 40.0f}, {31.0f, 31.0f});
    FLAT(((Color){44, 38, 30, 255}), {17.6f, 28.6f}, {22.4f, 28.6f}, {20.6f, 35.0f}, {19.6f, 35.0f});              // col
    line(11.0f, 32.0f, 12.0f, 32.0f, amber); line(28.0f, 32.0f, 29.0f, 32.0f, amber);                          // galons
    line(4.0f, 33.0f, 11.0f, 30.4f, coat[1]);                                                                    // patte d'épaule
    head34(sk, 0.0f, 0);
    POLY(gr, 0.50f, -0.3f, 0.0f, {12.5f, 14.6f}, {14.6f, 14.6f}, {14.8f, 20.6f}, {12.8f, 20.0f});                // favoris
    line(16.8f, 12.6f, 18.6f, 20.6f, scar);                                                                      // cicatrice
    POLY(gr, 0.56f, -0.3f, -0.25f, {12.8f, 19.5f}, {14.0f, 24.6f}, {17.5f, 28.6f}, {23.0f, 29.0f},
         {27.2f, 25.0f}, {27.6f, 21.2f}, {25.6f, 22.6f}, {22.5f, 23.2f}, {18.5f, 22.8f}, {15.2f, 20.6f});      // barbe
    for (int k = 0; k < 7; k++) px(15 + (k * 5) % 11, 23 + (k * 3) % 5, gr[2]);                                  // poils
    POLY(gr, 0.78f, -0.3f, 0.0f, {21.0f, 21.6f}, {26.6f, 21.2f}, {26.2f, 22.8f}, {23.8f, 23.0f}, {21.6f, 23.2f}); // moustache
    mouth(22, 24, 3, fl & SP_TALK, (Color){70, 50, 42, 255});
    line(15.5f, 14.2f, 20.0f, 13.6f, gr[1]); line(15.8f, 14.8f, 18.0f, 14.6f, gr[1]);                          // sourcils
    line(23.0f, 13.8f, 26.0f, 14.4f, gr[1]);
    line(17.0f, 11.4f, 22.0f, 11.2f, sk[2]);                                                                     // ride
    eyes34(17, 24, 16, fl & SP_BLINK, sk[1]);
    POLY(cap, 0.56f, -0.30f, -0.30f, {10.6f, 10.6f}, {11.5f, 6.0f}, {15.5f, 3.2f}, {22.5f, 2.6f}, {28.6f, 5.2f}, {29.2f, 10.4f});
    FLAT(cap[1], {10.6f, 9.4f}, {29.2f, 9.2f}, {29.2f, 11.0f}, {10.6f, 11.2f});                                 // bandeau
    disc(22.5f, 6.2f, 1.5f, amber); px(22, 5, (Color){255, 226, 140, 255});                                    // insigne
    FLAT(((Color){24, 20, 16, 255}), {17.0f, 11.0f}, {31.5f, 11.2f}, {31.2f, 12.6f}, {18.0f, 12.8f});           // visière
    line(19.0f, 11.2f, 30.5f, 11.4f, (Color){70, 68, 58, 255});
    line(18.0f, 13.0f, 28.0f, 13.2f, sk[1]);                                                                     // ombre portée
}

static void paint_kira(int fl) {
    Color sk[R6], vest[R6], sand[R6], helm[R6], hair[R6];
    ramp((Color){178, 126,  94, 255}, sk);   ramp((Color){ 70,  80,  52, 255}, vest);
    ramp((Color){196, 168, 116, 255}, sand); ramp((Color){ 86,  98,  62, 255}, helm);
    ramp((Color){128,  56,  38, 255}, hair);
    const Color strap = {36, 34, 28, 255}, lens = {110, 210, 214, 255}, paint = {38, 56, 32, 255};
    bust(vest, 0.50f);
    line(9.0f, 31.0f, 12.0f, 40.0f, strap);  line(10.0f, 31.0f, 13.0f, 40.0f, strap);                        // bretelles
    line(30.0f, 31.0f, 28.0f, 40.0f, strap); line(31.0f, 31.0f, 29.0f, 40.0f, strap);
    POLY(vest, 0.30f, 0.0f, 0.0f, {23.5f, 35.0f}, {28.0f, 35.0f}, {28.0f, 38.5f}, {23.5f, 38.5f});             // poche
    head34(sk, -0.6f, 1);
    POLY(hair, 0.50f, -0.3f, 0.0f, {11.2f, 13.5f}, {14.5f, 13.5f}, {14.0f, 19.5f}, {12.0f, 21.0f}, {11.0f, 17.5f});
    POLY(sand, 0.62f, -0.35f, -0.2f, {12.0f, 29.5f}, {15.5f, 26.8f}, {25.5f, 26.8f}, {29.0f, 29.5f},
         {27.5f, 32.5f}, {13.5f, 32.5f});                                                                        // chèche
    line(16.0f, 29.0f, 24.0f, 28.6f, sand[1]); line(14.5f, 31.3f, 26.5f, 31.0f, sand[1]);
    line(15.6f, 15.0f, 19.8f, 14.6f, hair[1]); line(23.0f, 14.7f, 25.6f, 15.1f, hair[1]);                    // sourcils
    eyes34(17, 24, 17, fl & SP_BLINK, sk[1]);
    line(15.6f, 18.8f, 19.6f, 18.6f, paint); line(23.0f, 18.8f, 25.4f, 18.9f, paint);                        // peinture
    mouth(22, 24, 3, fl & SP_TALK, (Color){128, 70, 56, 255});
    line(12.8f, 14.5f, 15.5f, 24.5f, strap);                                                                     // jugulaire
    POLY(helm, 0.56f, -0.35f, -0.35f, {9.4f, 14.2f}, {10.0f, 8.2f}, {13.4f, 4.2f}, {19.6f, 2.6f}, {26.0f, 4.0f},
         {29.6f, 8.4f}, {30.2f, 14.0f}, {26.0f, 13.0f}, {19.0f, 12.6f}, {12.8f, 13.6f});                       // casque
    line(9.6f, 14.0f, 30.0f, 13.6f, helm[1]);
    disc(14.0f, 6.6f, 1.5f, helm[2]); disc(24.0f, 5.8f, 1.3f, helm[2]); disc(27.4f, 10.4f, 1.1f, helm[2]);    // camouflage
    line(10.4f, 10.2f, 29.4f, 9.4f, strap);  line(10.4f, 11.0f, 29.4f, 10.2f, strap);                        // lunettes relevées
    disc(17.4f, 10.0f, 2.0f, strap); disc(17.4f, 10.0f, 1.3f, lens); px(16, 9, (Color){226, 255, 250, 255});
    disc(24.2f, 9.5f, 2.0f, strap);  disc(24.2f, 9.5f, 1.3f, lens);  px(23, 8, (Color){226, 255, 250, 255});
}

static void paint_varela(int fl) {
    Color sk[R6], cov[R6], hair[R6], metal[R6];
    ramp((Color){224, 174, 136, 255}, sk);   ramp((Color){ 66,  92, 118, 255}, cov);
    ramp((Color){ 52,  38,  34, 255}, hair); ramp((Color){126, 130, 136, 255}, metal);
    const Color tan = {150, 112, 70, 255}, amb = {240, 176, 48, 255}, smudge = {70, 64, 60, 255};
    bust(cov, 0.52f);
    POLY(cov, 0.72f, -0.2f, 0.0f, {15.0f, 28.6f}, {19.6f, 32.0f}, {17.0f, 33.6f}, {13.0f, 30.0f});            // col
    POLY(cov, 0.42f, -0.2f, 0.0f, {25.0f, 28.6f}, {20.4f, 32.0f}, {23.0f, 33.6f}, {27.0f, 30.0f});
    for (int k = 0; k < 3; k++) line(24.0f + k, 29.0f, 31.0f + k, 40.0f, tan);                                 // sangle
    POLY(cov, 0.28f, 0.0f, 0.0f, {4.5f, 34.5f}, {9.0f, 34.5f}, {9.0f, 39.0f}, {4.5f, 39.0f});                 // poche à outils
    line(6.0f, 33.0f, 6.0f, 35.0f, amb);
    ell(10.2f, 6.8f, 3.4f, 3.1f, hair, 0.60f);                                                                   // chignon
    POLY(hair, 0.48f, -0.3f, -0.2f, {11.0f, 22.0f}, {10.2f, 13.0f}, {12.5f, 6.5f}, {18.0f, 3.6f}, {24.5f, 4.2f},
         {28.4f, 8.4f}, {28.6f, 12.2f}, {25.5f, 10.0f}, {19.5f, 9.6f}, {15.0f, 12.0f}, {14.2f, 19.0f}, {12.8f, 23.0f});
    head34(sk, -0.6f, 1);
    POLY(hair, 0.55f, -0.3f, 0.0f, {13.6f, 12.8f}, {15.5f, 8.6f}, {21.0f, 7.4f}, {27.0f, 8.6f}, {28.4f, 11.6f},
         {24.0f, 10.2f}, {18.0f, 10.6f});                                                                        // frange
    line(13.0f, 19.0f, 12.5f, 25.0f, hair[2]);                                                                   // mèche
    line(16.0f, 15.0f, 19.6f, 14.8f, hair[1]); line(23.2f, 14.9f, 25.6f, 15.3f, hair[1]);
    eyes34(17, 24, 17, fl & SP_BLINK, sk[1]);
    px(25, 21, smudge); px(26, 21, smudge); px(25, 22, smudge);                                                  // cambouis
    mouth(22, 24, 3, fl & SP_TALK, (Color){160, 74, 68, 255});
    line(10.8f, 11.2f, 28.6f, 10.4f, metal[1]); line(10.8f, 12.0f, 28.6f, 11.2f, metal[1]);                   // lunettes de soudure
    ell(17.6f, 10.8f, 2.4f, 2.4f, metal, 0.6f); disc(17.6f, 10.8f, 1.5f, amb);
    ell(23.9f, 10.4f, 2.3f, 2.3f, metal, 0.5f); disc(23.9f, 10.4f, 1.4f, amb);
}

static void paint_steel(int fl) {
    Color st[R6], cloth[R6], red[R6];
    ramp((Color){132, 138, 148, 255}, st);  ramp((Color){ 44,  38,  38, 255}, cloth);
    ramp((Color){156,  32,  32, 255}, red);
    POLY(cloth, 0.45f, -0.2f, 0.0f, {13.0f, 28.0f}, {27.0f, 28.0f}, {26.0f, 40.0f}, {14.0f, 40.0f});           // plastron
    POLY(st, 0.62f, -0.40f, -0.40f, {0.0f, 40.0f}, {0.5f, 31.0f}, {5.0f, 27.0f}, {12.0f, 26.5f}, {15.5f, 29.0f}, {15.0f, 40.0f});
    POLY(st, 0.44f, -0.30f, -0.40f, {25.0f, 29.0f}, {28.5f, 26.5f}, {35.0f, 27.0f}, {39.5f, 31.0f}, {40.0f, 40.0f}, {25.0f, 40.0f});
    line(4.0f, 27.0f, 3.0f, 22.0f, st[5]);  line(8.0f, 26.6f, 8.0f, 21.5f, st[5]);                           // pointes
    line(32.0f, 26.8f, 32.5f, 22.0f, st[4]); line(36.0f, 28.0f, 37.5f, 23.5f, st[4]);
    int rv[6][2] = {{3, 33}, {7, 35}, {11, 32}, {29, 33}, {33, 35}, {37, 32}};
    for (int k = 0; k < 6; k++) px(rv[k][0], rv[k][1], st[1]);                                                  // rivets
    POLY(red, 0.55f, -0.3f, 0.0f, {17.8f, 3.0f}, {16.0f, 0.0f}, {22.5f, 0.0f}, {21.6f, 3.2f});                 // cimier
    POLY(st, 0.58f, -0.45f, -0.25f, {11.0f, 26.0f}, {10.4f, 11.0f}, {12.6f, 5.4f}, {19.4f, 2.6f}, {26.4f, 4.8f},
         {29.4f, 10.6f}, {29.4f, 24.4f}, {25.6f, 28.0f}, {14.4f, 28.0f});                                       // heaume
    line(12.6f, 8.0f, 12.4f, 24.0f, st[5]);  line(13.4f, 6.5f, 13.2f, 23.0f, st[4]);                          // reflet
    line(24.4f, 4.6f, 24.6f, 27.4f, st[1]);  line(23.6f, 5.0f, 23.8f, 27.0f, st[4]);                          // arête (3/4)
    line(28.6f, 11.0f, 28.6f, 24.0f, st[1]);
    FLAT(C_BLACK, {11.0f, 13.2f}, {29.4f, 12.6f}, {29.4f, 15.2f}, {11.0f, 15.6f});                             // fente de visée
    FLAT((fl & SP_TALK) ? st[4] : st[2], {19.0f, 18.0f}, {27.0f, 18.0f}, {27.0f, 23.0f}, {19.0f, 23.0f});      // grille
    for (int x = 20; x <= 26; x += 2) line((float)x, 18.5f, (float)x, 22.5f, C_BLACK);
    px(14, 9, st[1]); px(27, 9, st[1]); px(14, 22, st[1]); px(27, 21, st[1]);
}

static void paint_nexus(int fl) {
    (void)fl;
    Color core[R6], ringc[R6];
    ramp((Color){ 44,  14,  16, 255}, core); ramp((Color){ 96,  26,  28, 255}, ringc);
    const Color trace = {132, 28, 28, 255}, node = {210, 60, 48, 255};
    Vector2 O[8];
    for (int k = 0; k < 8; k++) {
        float a = (k * 45.0f + 22.5f) * 0.0174533f;
        O[k] = (Vector2){20.0f + cosf(a) * 18.5f, 20.0f + sinf(a) * 18.5f};
    }
    poly(O, 8, core, 0.50f, -0.35f, -0.35f);                                                                     // boîtier
    for (int k = 0; k < 8; k++) line(O[k].x, O[k].y, O[(k + 1) % 8].x, O[(k + 1) % 8].y, ringc[3]);          // arêtes
    line(20.0f, 4.0f, 20.0f, 12.0f, trace); line(20.0f, 28.0f, 20.0f, 36.0f, trace);                          // pistes
    line(4.0f, 20.0f, 12.0f, 20.0f, trace); line(28.0f, 20.0f, 36.0f, 20.0f, trace);
    line(20.0f, 6.0f, 25.0f, 6.0f, trace);  line(20.0f, 34.0f, 15.0f, 34.0f, trace);
    line(6.0f, 20.0f, 6.0f, 15.0f, trace);  line(34.0f, 20.0f, 34.0f, 25.0f, trace);
    px(25, 6, node); px(15, 34, node); px(6, 15, node); px(34, 25, node);
    px(9, 9, node); px(30, 9, node); px(9, 30, node); px(30, 30, node);
    ell(20.0f, 20.0f, 8.0f, 8.0f, ringc, 0.55f);                                                                // orbite
    disc(20.0f, 20.0f, 5.6f, (Color){8, 2, 4, 255});
    disc(20.0f, 20.0f, 3.2f, (Color){190, 40, 32, 255});                                                       // iris (animé par-dessus)
    disc(20.0f, 20.0f, 1.3f, (Color){255, 200, 180, 255});
}

static void paint_general(int fl) {
    Color hull[R6], tun[R6], gold[R6], cap[R6];
    ramp((Color){150, 142, 112, 255}, hull); ramp((Color){ 60,  64,  44, 255}, tun);
    ramp((Color){230, 186,  70, 255}, gold); ramp((Color){ 64,  70,  46, 255}, cap);
    const Color amber = {255, 178, 48, 255}, ambd = {140, 86, 26, 255};
    bust(tun, 0.50f);
    POLY(tun, 0.70f, -0.2f, 0.0f, {16.0f, 29.0f}, {24.0f, 29.0f}, {23.0f, 40.0f}, {17.0f, 40.0f});            // plastron
    px(20, 32, gold[4]); px(20, 35, gold[4]); px(20, 38, gold[4]);                                             // boutons
    POLY(gold, 0.62f, -0.3f, -0.3f, {2.0f, 32.0f}, {5.0f, 29.5f}, {12.0f, 29.0f}, {12.5f, 31.5f}, {4.0f, 33.5f}); // épaulettes
    POLY(gold, 0.44f, -0.3f, -0.3f, {28.0f, 29.0f}, {35.0f, 29.5f}, {38.0f, 32.0f}, {36.0f, 33.5f}, {27.5f, 31.5f});
    for (int x = 3; x <= 11; x += 2) px(x, 34, gold[2]);
    for (int x = 29; x <= 37; x += 2) px(x, 34, gold[1]);
    const Color rib[3] = {{170, 50, 40, 255}, {60, 90, 150, 255}, {230, 186, 70, 255}};
    for (int k = 0; k < 3; k++) { px(7 + k, 36, rib[k]); px(7 + k, 37, rib[(k + 1) % 3]); }                // barrettes
    POLY(hull, 0.30f, 0.0f, 0.0f, {17.0f, 24.0f}, {23.0f, 24.0f}, {23.0f, 29.0f}, {17.0f, 29.0f});             // cou à vérins
    line(19.0f, 24.0f, 19.0f, 28.0f, hull[4]);
    POLY(hull, 0.58f, -0.40f, -0.30f, {12.0f, 25.5f}, {11.4f, 11.0f}, {13.6f, 8.6f}, {27.4f, 8.6f}, {29.6f, 11.0f},
         {29.6f, 23.4f}, {26.6f, 26.4f}, {15.0f, 26.4f});                                                       // crâne de tôle
    line(24.4f, 9.0f, 24.6f, 26.0f, hull[1]); line(23.6f, 9.2f, 23.8f, 25.8f, hull[4]);                       // arête
    POLY(hull, 0.28f, 0.0f, 0.0f, {10.6f, 15.0f}, {12.0f, 15.0f}, {12.0f, 20.0f}, {10.6f, 20.0f});            // capteur
    px(11, 17, ambd);
    px(14, 12, hull[1]); px(28, 12, hull[1]); px(14, 23, hull[1]); px(28, 22, hull[1]);
    FLAT(C_BLACK, {12.4f, 14.6f}, {29.4f, 14.2f}, {29.4f, 16.8f}, {12.4f, 17.2f});                             // fente
    FLAT((fl & SP_TALK) ? amber : hull[1], {19.0f, 20.0f}, {27.0f, 20.0f}, {27.0f, 23.0f}, {19.0f, 23.0f});    // grille
    for (int x = 20; x <= 26; x += 2) line((float)x, 20.5f, (float)x, 22.5f, C_BLACK);
    POLY(cap, 0.56f, -0.30f, -0.30f, {10.6f, 10.6f}, {11.5f, 6.0f}, {15.5f, 3.2f}, {22.5f, 2.6f}, {28.6f, 5.2f}, {29.2f, 10.4f});
    line(10.8f, 9.6f, 29.0f, 9.4f, gold[4]);                                                                     // cordon
    disc(22.5f, 6.0f, 1.5f, gold[4]); px(22, 5, gold[5]);
    FLAT(C_BLACK, {17.0f, 10.6f}, {31.5f, 10.8f}, {31.2f, 12.2f}, {18.0f, 12.4f});                             // visière
    line(27.6f, 8.0f, 28.6f, 1.6f, hull[1]);                                                                    // antenne
    px(28, 1, (fl & SP_TALK) ? amber : ambd);
}

static void paint_radio(int fl) {
    Color sk[R6], jack[R6], cap[R6], set[R6];
    ramp((Color){196, 148, 112, 255}, sk);  ramp((Color){ 62,  66,  52, 255}, jack);
    ramp((Color){ 74,  70,  56, 255}, cap); ramp((Color){ 44,  42,  40, 255}, set);
    const Color dark = {28, 26, 24, 255}, led_on = {130, 236, 140, 255}, led_off = {40, 70, 44, 255};
    bust(jack, 0.50f);
    POLY(jack, 0.70f, -0.2f, 0.0f, {14.0f, 28.6f}, {19.0f, 31.6f}, {16.0f, 33.0f}, {12.0f, 30.0f});            // col
    POLY(jack, 0.42f, -0.2f, 0.0f, {26.0f, 28.6f}, {21.0f, 31.6f}, {24.0f, 33.0f}, {28.0f, 30.0f});
    head34(sk, 0.0f, 0);
    line(15.6f, 14.6f, 19.6f, 14.2f, dark); line(23.0f, 14.4f, 25.6f, 14.8f, dark);
    eyes34(17, 24, 16, fl & SP_BLINK, sk[1]);
    mouth(22, 24, 3, fl & SP_TALK, (Color){110, 64, 52, 255});
    POLY(cap, 0.55f, -0.30f, -0.30f, {11.0f, 13.8f}, {11.6f, 7.6f}, {15.0f, 4.2f}, {20.5f, 3.2f}, {26.0f, 4.6f},
         {29.0f, 8.6f}, {29.2f, 13.2f}, {20.0f, 11.8f});                                                        // bonnet
    line(11.0f, 12.6f, 29.0f, 11.8f, cap[1]);
    for (int x = 12; x <= 28; x += 2) px(x, 13, cap[2]);
    line(12.0f, 15.0f, 12.0f, 9.0f, dark); line(12.0f, 9.0f, 16.0f, 4.2f, dark);                             // arceau
    line(16.0f, 4.2f, 22.0f, 3.0f, dark);  line(22.0f, 3.0f, 27.6f, 6.4f, dark);
    ell(12.6f, 18.4f, 2.6f, 3.2f, set, 0.55f);                                                                  // écouteur
    line(13.8f, 21.0f, 24.0f, 24.8f, dark);                                                                     // perche du micro
    disc(25.0f, 25.0f, 1.2f, dark);
    px(25, 25, (fl & SP_TALK) ? led_on : led_off);
    px(11, 17, (fl & SP_TALK) ? led_on : led_off);                                                              // voyant d'émission
}

void sp_paint_figure(Image *layer, SpeakerId id, int flags) {
    g_L = layer;
    switch (id) {
        case SPEAKER_VOSS:    paint_voss(flags);    break;
        case SPEAKER_KIRA:    paint_kira(flags);    break;
        case SPEAKER_VARELA:  paint_varela(flags);  break;
        case SPEAKER_STEEL:   paint_steel(flags);   break;
        case SPEAKER_NEXUS:   paint_nexus(flags);   break;
        case SPEAKER_GENERAL: paint_general(flags); break;
        case SPEAKER_RADIO:
        default:              paint_radio(flags);   break;
    }
}

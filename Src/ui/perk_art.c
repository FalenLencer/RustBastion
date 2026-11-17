/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  perk_art.c ─ Emblèmes des perks peints comme des mini splash arts.
 *
 *  Chaque perk = un OBJET de PA_N × PA_N pixels d'art (volumes ombrés sur
 *  rampe à 6 tons, contour sombre, halo à la couleur de sa rareté,
 *  étincelles pour les épiques et clés de voûte). Atlas peint une fois :
 *  rangée 0 = normal, rangée 1 = grisé (achat impossible). Dessiné net :
 *  1:1 sur les cartes, multiple entier en grand, lissé en petit.
 */
#include "perk_art.h"
#include "pixel_paint.h"
#include "terrain_internal.h"     // tb_* (rampes, tramage)
#include "../game/runperks.h"
#include <math.h>

#define PA_N        24                       // pixels d'art par emblème
#define PA_GLOW     2                        // rayon du halo de rareté (px d'art)
#define PA_DIM      0.55f                    // luminosité de la version grisée
#define PA_OUTLINE  ((Color){14, 10, 10, 255})

static const Color PA_STEEL = {146, 150, 160, 255}, PA_DSTEEL = {84, 88, 100, 255};
static const Color PA_GOLD  = {236, 188, 64, 255},  PA_RED    = {206, 54, 44, 255};
static const Color PA_GLASS = {112, 206, 236, 255}, PA_WOOD   = {154, 106, 58, 255};
static const Color PA_WHITE = {244, 240, 232, 255}, PA_DARK  = {20, 16, 16, 255};
static const Color PA_GREEN = {96, 196, 92, 255},   PA_ELEC   = {126, 206, 255, 255};

static Texture2D g_point, g_smooth;
static int       g_ok;

static void ellipse(PxCanvas *c, float cx, float cy, float rx, float ry, Color base, float b) {
    Color r[TB_RAMP];
    tb_make_ramp(base, r);
    for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy > 1.0f) continue;
            pp_px(c, x, y, tb_ramp(r, b - (dx + dy) * 0.22f, c->ox + x, y));
        }
}
#define POLY(base, b, ...) do { const Vector2 _p[] = {__VA_ARGS__}; \
    pp_poly(c, _p, (int)(sizeof(_p) / sizeof(_p[0])), base, b); } while (0)
#define V(x, y) (Vector2){(float)(x), (float)(y)}

static void coin(PxCanvas *c, float cx, float cy, float rad) {
    pp_disc(c, cx, cy, rad, PA_GOLD, 0.62f);
    for (int a = 0; a < 24; a++) {                       // tranche gravée
        float t = a * 0.2618f;
        pp_px(c, (int)(cx + cosf(t) * (rad - 1.5f)), (int)(cy + sinf(t) * (rad - 1.5f)), tb_scale(PA_GOLD, 0.62f));
    }
}

/* Sac noué à oreilles (Capitaliste, Butin de guerre). */
static void sack(PxCanvas *c, float cx, float cy, Color cloth) {
    pp_disc(c, cx, cy + 2.5f, 6.4f, cloth, 0.58f);                       // panse
    POLY(tb_scale(cloth, 0.85f), 0.55f, V(cx - 3, cy - 5), V(cx + 3, cy - 5), V(cx + 2, cy - 2), V(cx - 2, cy - 2));
    POLY(cloth, 0.66f, V(cx - 6, cy - 8), V(cx - 1, cy - 5), V(cx - 3, cy - 3));    // oreilles
    POLY(cloth, 0.48f, V(cx + 6, cy - 8), V(cx + 3, cy - 3), V(cx + 1, cy - 5));
    pp_line(c, (int)cx - 3, (int)cy - 3, (int)cx + 3, (int)cy - 3, (Color){110, 70, 34, 255});   // lien
}

// ════════════════════════════════════════════════════════════════
// PEINTURE DES 22 EMBLÈMES
// ════════════════════════════════════════════════════════════════
static void paint(PxCanvas *c, int id) {
    switch (id) {
    case PERK_GUN:                                        // tourelle blindée à double canon
        pp_box(c, 3, 17, 15, 4, PA_DSTEEL, 0.5f);
        pp_px(c, 5, 18, PA_WHITE); pp_px(c, 15, 18, PA_WHITE);
        pp_disc(c, 10.0f, 11.0f, 4.6f, (Color){200, 64, 46, 255}, 0.62f);           // dôme
        pp_box(c, 5, 11, 10, 6, (Color){178, 54, 40, 255}, 0.55f);                   // tête
        pp_box(c, 14, 9, 8, 2, PA_STEEL, 0.62f); pp_box(c, 14, 14, 8, 2, PA_STEEL, 0.5f);
        pp_box(c, 7, 13, 5, 1, PA_DARK, 0.2f);                                         // fente
        pp_sparkle(c, 22, 10, 1, (Color){255, 170, 60, 255});                         // éclat de tir
        pp_sparkle(c, 22, 15, 1, (Color){255, 170, 60, 255});
        break;
    case PERK_SNIPER:                                     // lunette de précision
        POLY(PA_DSTEEL, 0.55f, V(4, 15), V(16, 6), V(19, 9), V(7, 18));
        pp_line(c, 9, 12, 11, 15, tb_scale(PA_DSTEEL, 0.6f));
        pp_disc(c, 18.0f, 7.0f, 3.2f, PA_GLASS, 0.66f);
        pp_px(c, 17, 6, PA_WHITE);
        pp_px(c, 18, 7, (Color){230, 60, 50, 255}); pp_px(c, 19, 7, (Color){230, 60, 50, 140});
        pp_disc(c, 5.0f, 16.5f, 2.2f, PA_STEEL, 0.5f);
        break;
    case PERK_FLAME:                                      // flamme à cœur blanc
        POLY(((Color){214, 64, 30, 255}), 0.55f, V(12, 3), V(16, 9), V(18, 14), V(16, 19),
             V(12, 21), V(8, 19), V(6, 14), V(9, 9));
        POLY(((Color){250, 150, 40, 255}), 0.6f, V(12, 8), V(15, 13), V(15, 17), V(12, 19), V(9, 17), V(9, 13));
        pp_disc(c, 12.0f, 16.5f, 2.2f, (Color){255, 238, 170, 255}, 0.8f);
        pp_px(c, 15, 5, (Color){250, 120, 40, 255}); pp_px(c, 8, 7, (Color){250, 120, 40, 255});
        break;
    case PERK_TESLA:                                      // éclair
        POLY(PA_ELEC, 0.6f, V(15, 3), V(7, 13), V(11, 13), V(8, 21), V(18, 10), V(13, 10), V(17, 3));
        pp_line(c, 14, 5, 10, 12, PA_WHITE); pp_line(c, 13, 12, 10, 18, PA_WHITE);
        break;
    case PERK_PUISSANCE:                                  // surcharge : flèche incandescente
        POLY(((Color){240, 136, 40, 255}), 0.6f, V(12, 3), V(20, 11), V(15, 11), V(15, 20),
             V(9, 20), V(9, 11), V(4, 11));
        pp_line(c, 12, 6, 12, 18, (Color){255, 228, 150, 255});
        break;
    case PERK_PORTEE: {                                   // tourelle + flèche (cercles : paint_fx)
        pp_disc(c, 8.0f, 15.0f, 2.6f, PA_STEEL, 0.6f);
        pp_line(c, 9, 13, 18, 5, (Color){240, 140, 40, 255}); pp_line(c, 10, 14, 19, 6, (Color){200, 100, 30, 255});
        POLY(((Color){240, 140, 40, 255}), 0.65f, V(15, 2), V(21, 2), V(21, 8));
    } break;
    case PERK_CADENCE:                                    // engrenage
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f;
            pp_box(c, 11 + (int)roundf(cosf(a) * 7.5f) - 1, 11 + (int)roundf(sinf(a) * 7.5f) - 1, 3, 3, PA_STEEL, 0.55f);
        }
        pp_disc(c, 12.0f, 12.0f, 6.4f, PA_STEEL, 0.6f);
        pp_disc(c, 12.0f, 12.0f, 2.8f, PA_DSTEEL, 0.45f);
        pp_disc(c, 12.0f, 12.0f, 1.1f, PA_DARK, 0.3f);
        break;
    case PERK_GLASS:                                      // canon de verre : gemme fêlée
        POLY(PA_GLASS, 0.62f, V(12, 3), V(19, 10), V(12, 21), V(5, 10));
        pp_line(c, 5, 10, 19, 10, tb_scale(PA_GLASS, 1.25f));
        pp_line(c, 12, 3, 9, 10, tb_scale(PA_GLASS, 1.2f)); pp_line(c, 12, 3, 15, 10, tb_scale(PA_GLASS, 0.8f));
        pp_line(c, 13, 6, 11, 12, PA_WHITE); pp_line(c, 11, 12, 13, 18, PA_WHITE);
        POLY(PA_GLASS, 0.7f, V(19, 15), V(21, 14), V(20, 18));
        break;
    case PERK_BUTIN: {                                    // pièce frappée d'un crâne
        coin(c, 12.0f, 12.0f, 8.0f);
        Color bone = {72, 50, 14, 255};
        pp_box(c, 10, 8, 5, 4, (Color){252, 230, 150, 255}, 0.7f);
        pp_px(c, 11, 9, bone); pp_px(c, 13, 9, bone);
        pp_line(c, 10, 13, 14, 13, (Color){252, 230, 150, 255});
        pp_px(c, 11, 13, bone); pp_px(c, 13, 13, bone);
        pp_line(c, 8, 15, 16, 17, tb_scale(PA_GOLD, 0.55f)); pp_line(c, 16, 15, 8, 17, tb_scale(PA_GOLD, 0.55f));
    } break;
    case PERK_SUBV:                                       // pile de pièces
        for (int k = 0; k < 3; k++) {
            int y = 16 - k * 4;
            pp_box(c, 6 + (k & 1), y, 12, 3, tb_scale(PA_GOLD, 0.8f), 0.5f);
            ellipse(c, 12.0f + (k & 1), (float)y, 6.0f, 1.8f, PA_GOLD, 0.68f);
        }
        pp_px(c, 12, 8, (Color){255, 250, 200, 255});
        break;
    case PERK_REVENTE:                                    // recyclage : flèches + pièce
        for (int a = 0; a < 40; a++) {
            float t = a * 0.157f;
            if ((t > 1.2f && t < 1.9f) || (t > 4.3f && t < 5.0f)) continue;   // coupures
            for (float rr = 6.4f; rr <= 8.2f; rr += 0.9f)
                pp_px(c, 12 + (int)roundf(cosf(t) * rr), 12 + (int)roundf(sinf(t) * rr), tb_scale(PA_GREEN, 0.7f + 0.15f * (rr - 6.4f)));
        }
        POLY(PA_GREEN, 0.7f, V(8, 2), V(13, 5), V(8, 8));
        POLY(PA_GREEN, 0.5f, V(16, 22), V(11, 19), V(16, 16));
        coin(c, 12.0f, 12.0f, 3.4f);
        break;
    case PERK_CAPITAL:                                    // sac d'or + flèche montante
        sack(c, 10.5f, 12.0f, (Color){182, 140, 86, 255});
        pp_line(c, 10, 11, 10, 19, PA_GOLD);                                           // « $ »
        pp_line(c, 8, 12, 12, 12, PA_GOLD); pp_line(c, 8, 15, 12, 15, PA_GOLD); pp_line(c, 8, 18, 12, 18, PA_GOLD);
        pp_px(c, 8, 13, PA_GOLD); pp_px(c, 12, 17, PA_GOLD); pp_px(c, 8, 14, PA_GOLD); pp_px(c, 12, 16, PA_GOLD);
        POLY(PA_GREEN, 0.65f, V(18, 2), V(22, 7), V(20, 7), V(20, 12), V(16, 12), V(16, 7), V(14, 7));
        break;
    case PERK_BLINDAGE:                                   // bouclier riveté
        POLY(PA_STEEL, 0.62f, V(4, 4), V(20, 4), V(20, 12), V(12, 21), V(4, 12));
        pp_box(c, 11, 5, 2, 13, (Color){190, 60, 50, 255}, 0.5f);
        pp_line(c, 5, 5, 19, 5, PA_WHITE);
        pp_px(c, 6, 7, PA_WHITE); pp_px(c, 17, 7, PA_WHITE); pp_px(c, 6, 11, PA_WHITE); pp_px(c, 17, 11, PA_WHITE);
        break;
    case PERK_FUSIL:                                      // armement lourd
        POLY(PA_WOOD, 0.55f, V(3, 17), V(8, 13), V(10, 16), V(5, 21));
        POLY(PA_STEEL, 0.6f, V(8, 13), V(20, 5), V(21, 7), V(10, 16));
        POLY(PA_DSTEEL, 0.45f, V(11, 14), V(14, 12), V(16, 16), V(13, 18));
        pp_box(c, 13, 8, 4, 2, PA_DSTEEL, 0.5f);
        pp_px(c, 21, 5, (Color){255, 200, 90, 255});
        break;
    case PERK_MEDIC:                                      // trousse de soin
        pp_box(c, 9, 5, 6, 3, PA_DSTEEL, 0.5f); pp_box(c, 11, 6, 2, 2, PA_DARK, 0.2f);
        pp_box(c, 4, 8, 16, 12, PA_WHITE, 0.55f);
        pp_box(c, 11, 10, 2, 8, PA_RED, 0.6f); pp_box(c, 8, 13, 8, 2, PA_RED, 0.6f);
        pp_px(c, 5, 9, (Color){255, 255, 255, 255});
        break;
    case PERK_GARNISON:                                   // cœur (vies)
        pp_disc(c, 8.5f, 9.5f, 4.4f, PA_RED, 0.62f); pp_disc(c, 15.5f, 9.5f, 4.4f, PA_RED, 0.52f);
        POLY(PA_RED, 0.5f, V(4, 11), V(20, 11), V(12, 20));
        pp_px(c, 7, 7, PA_WHITE); pp_px(c, 8, 7, (Color){255, 190, 180, 255});
        break;
    case PERK_ARSENAL:                                    // étoile-tourelle
        POLY(PA_GOLD, 0.62f, V(12, 2), V(14, 9), V(21, 9), V(15, 13), V(18, 21), V(12, 16),
             V(6, 21), V(9, 13), V(3, 9), V(10, 9));
        pp_disc(c, 12.0f, 12.0f, 3.0f, (Color){196, 62, 46, 255}, 0.6f);
        pp_box(c, 13, 11, 5, 2, PA_STEEL, 0.6f);
        break;
    case PERK_FORTUNE:                                    // grosse pièce sertie
        coin(c, 12.0f, 12.0f, 9.0f);
        POLY(((Color){255, 226, 120, 255}), 0.7f, V(12, 6), V(14, 11), V(18, 11), V(15, 14),
             V(16, 18), V(12, 15), V(8, 18), V(9, 14), V(6, 11), V(10, 11));
        pp_px(c, 6, 6, (Color){230, 60, 70, 255}); pp_px(c, 18, 18, (Color){80, 140, 240, 255});
        break;
    case PERK_LEGION:                                     // épées croisées
        for (int s = -1; s <= 1; s += 2) {
            int x0 = s < 0 ? 5 : 19, x1 = s < 0 ? 18 : 6;
            pp_line(c, x0, 4, x1, 17, PA_WHITE); pp_line(c, x0 + (s < 0 ? 1 : -1), 4, x1 + (s < 0 ? 1 : -1), 17, PA_STEEL);
            pp_box(c, (s < 0 ? 16 : 5), 16, 4, 2, PA_GOLD, 0.6f);
            pp_disc(c, (s < 0 ? 19.5f : 4.5f), 20.0f, 1.5f, PA_GOLD, 0.6f);
        }
        break;
    case PERK_K_FORTRESS:                                 // tour crénelée + bannière
        pp_box(c, 7, 8, 10, 13, (Color){156, 146, 128, 255}, 0.56f);
        pp_box(c, 6, 6, 3, 3, (Color){156, 146, 128, 255}, 0.6f);
        pp_box(c, 11, 6, 3, 3, (Color){156, 146, 128, 255}, 0.6f);
        pp_box(c, 15, 6, 3, 3, (Color){156, 146, 128, 255}, 0.6f);
        pp_box(c, 10, 15, 4, 6, PA_DARK, 0.2f); pp_box(c, 11, 10, 2, 3, PA_DARK, 0.2f);
        pp_line(c, 12, 1, 12, 6, PA_DSTEEL);
        POLY(PA_RED, 0.6f, V(13, 1), V(18, 2), V(13, 4));
        break;
    case PERK_K_SPOILS:                                   // sac de butin qui déborde
        sack(c, 10.0f, 12.0f, (Color){172, 140, 96, 255});
        coin(c, 17.5f, 18.0f, 2.6f); coin(c, 19.5f, 14.0f, 2.2f); coin(c, 15.0f, 21.0f, 1.8f);
        break;
    case PERK_K_GEOMANCER:                                // bloc fendu de lave + cristal
        pp_box(c, 4, 12, 16, 9, (Color){118, 90, 62, 255}, 0.55f);
        pp_line(c, 4, 12, 19, 12, (Color){96, 150, 70, 255});
        pp_line(c, 6, 14, 10, 18, (Color){255, 150, 50, 255}); pp_line(c, 10, 18, 14, 14, (Color){255, 190, 80, 255});
        pp_line(c, 14, 14, 17, 19, (Color){255, 120, 40, 255});
        POLY(((Color){120, 232, 150, 255}), 0.65f, V(12, 2), V(16, 7), V(13, 12), V(9, 8));
        break;
    default:
        pp_disc(c, 12.0f, 12.0f, 6.0f, PA_STEEL, 0.5f);
        break;
    }
}

/* Effets peints APRÈS contour et halo (lumières, ondes) : sans contour, ils
   restent fins et lumineux au lieu de devenir des pâtés cernés. */
static void paint_fx(PxCanvas *c, int id) {
    if (id == PERK_TESLA) {                               // étincelles électriques
        pp_sparkle(c, 5, 8, 1, PA_ELEC); pp_sparkle(c, 19, 16, 1, PA_ELEC);
        pp_px(c, 6, 19, PA_WHITE);
    } else if (id == PERK_FLAME) {                        // braises qui s'envolent
        pp_px(c, 16, 3, (Color){255, 170, 60, 230}); pp_px(c, 8, 5, (Color){255, 120, 40, 200});
        pp_px(c, 17, 7, (Color){255, 220, 120, 180});
    } else if (id == PERK_PUISSANCE) {                    // éclats d'énergie
        pp_sparkle(c, 5, 17, 1, (Color){255, 200, 90, 255}); pp_sparkle(c, 19, 18, 1, (Color){255, 200, 90, 255});
    } else if (id == PERK_PORTEE) {                       // cercles de portée
        for (int k = 1; k <= 3; k++)
            for (int a = 0; a < 40; a++) {
                float t = -2.4f + a * 0.085f, rr = 3.3f * k + 1.0f;
                pp_px(c, 8 + (int)roundf(cosf(t) * rr), 15 + (int)roundf(sinf(t) * rr),
                      (Color){120, 232, 224, (unsigned char)(250 - k * 55)});
            }
    }
}

// ════════════════════════════════════════════════════════════════
// ATLAS
// ════════════════════════════════════════════════════════════════
static void bake(void) {
    Image im = GenImageColor(PA_N * PERK_COUNT, PA_N * 2, BLANK);
    for (int id = 0; id < PERK_COUNT; id++) {
        PxCanvas c = {&im, id * PA_N, 0, PA_N, PA_N, 0xA27u + (unsigned)id * 97u};
        paint(&c, id);
        pp_outline(&c, PA_OUTLINE);
        RunColor rc = runperk_rarity_color(RUN_PERKS[id].rarity);
        pp_glow(&c, (Color){rc.r, rc.g, rc.b, 255}, PA_GLOW);
        paint_fx(&c, id);
        if (RUN_PERKS[id].rarity >= RAR_EPIC) {           // éclat des objets précieux
            pp_sparkle(&c, 19, 4, 2, (Color){rc.r, rc.g, rc.b, 255});
            pp_sparkle(&c, 4, 19, 1, (Color){rc.r, rc.g, rc.b, 255});
        }
        if (RUN_PERKS[id].rarity == RAR_KEYSTONE) {       // signature : pierre d'angle dorée
            for (int y = 1; y <= 3; y++)
                for (int x = 9 - (3 - y); x <= 14 + (3 - y); x++)
                    pp_px(&c, x, y, (y == 1 || x == 9 - (3 - y) || x == 14 + (3 - y)) ? PA_OUTLINE
                                                                                   : tb_scale(PA_GOLD, 1.0f + 0.1f * (3 - y)));
        }
        for (int y = 0; y < PA_N; y++)                     // rangée grisée (achat impossible)
            for (int x = 0; x < PA_N; x++) {
                Color p = tb_get(&im, id * PA_N + x, y);
                unsigned char g = (unsigned char)((p.r * 0.30f + p.g * 0.59f + p.b * 0.11f) * PA_DIM);
                tb_put(&im, id * PA_N + x, PA_N + y, (Color){g, g, g, p.a});
            }
    }
    g_point  = LoadTextureFromImage(im);
    g_smooth = LoadTextureFromImage(im);
    SetTextureFilter(g_point, TEXTURE_FILTER_POINT);
    SetTextureFilter(g_smooth, TEXTURE_FILTER_BILINEAR);
    UnloadImage(im);
    g_ok = 1;
}

void perk_art_draw(int perk_id, int cx, int cy, int r, Color col) {
    if (perk_id < 0 || perk_id >= PERK_COUNT || r <= 0) return;
    if (!g_ok) bake();
    /* Gris pur (hors blanc) = carte désactivée (boutique : achat impossible). */
    int disabled = (col.r == col.g && col.g == col.b && col.r < 255);
    float size = 2.0f * (float)r;
    Texture2D tex = g_smooth;
    if (size >= 2.0f * PA_N) { size = floorf(size / PA_N) * PA_N; tex = g_point; }   // net, pixels entiers
    else if (size >= PA_N * 0.85f) tex = g_point;                                     // ~1:1
    Rectangle src = {(float)(perk_id * PA_N), (float)(disabled * PA_N), (float)PA_N, (float)PA_N};
    Rectangle dst = {cx - size * 0.5f, cy - size * 0.5f, size, size};
    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, col.a});
}

void perk_art_unload(void) {
    if (!g_ok) return;
    UnloadTexture(g_point);
    UnloadTexture(g_smooth);
    g_ok = 0;
}

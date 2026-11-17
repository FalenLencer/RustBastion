/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  terrain_ruins.c ─ Ruines peintes comme des mini splash arts.
 *
 *  Par thème, un atlas de RS_VARIANTS sprites (TB_TILE × RS_H pixels d'art,
 *  qui débordent vers le haut pour donner du VOLUME). Recette commune : des
 *  volumes ombrés sur rampe à 6 tons (lumière en haut à gauche), du bruit de
 *  matière, puis un CONTOUR sombre d'un pixel et une ombre portée au sol.
 *  Une tour posée sur la ruine la fait disparaître (sprite non dessiné).
 */
#include "terrain.h"
#include "terrain_internal.h"
#include "pixel_paint.h"
#include "../map/theme.h"

#define RS_W         TB_TILE
#define RS_H         (TB_TILE + 6)    // 6 px d'art de surplomb (hauteur)
#define RS_VARIANTS  5                // 0..2 grandes structures, 3..4 petits objets
#define RS_LARGE     3
/* % de ruines portant une GRANDE structure, par thème (ville = blocs
   d'immeubles ; ailleurs, alternance avec des petits objets). */
static const unsigned char RS_LARGE_PCT[THEME_COUNT] = {
    [THEME_WASTELAND] = 50, [THEME_SWAMP] = 55, [THEME_DESERT] = 45,
    [THEME_CITY] = 80, [THEME_FACTORY] = 55,
};
#define RS_GROUND    (RS_H - 3)       // ligne de sol dans le sprite
#define RS_OUTLINE   ((Color){16, 12, 10, 255})
#define RS_SHADOW_A  105

static Texture2D g_atlas[THEME_COUNT];
static int       g_atlas_ok[THEME_COUNT];

/* Monticule de gravats (petits blocs ombrés) au pied d'une structure. */
static void rubble(PxCanvas *p, int x0, int x1, Color base) {
    Color r[TB_RAMP];
    tb_make_ramp(base, r);
    for (int x = x0; x <= x1; x++) {
        int top = RS_GROUND - 1 - (int)(tb_rand(x, 3, p->s) * 2.5f);
        for (int y = top; y <= RS_GROUND; y++)
            pp_px(p, x, y, r[1 + (int)(tb_rand(x, y, p->s + 7u) * 4.0f) - (y == top ? -1 : 0)]);
    }
}
/* Contour sombre d'1 px + ombre portée au sol (après tout le dessin). */
static void finish(PxCanvas *p) {
    pp_outline(p, RS_OUTLINE);
    for (int y = RS_GROUND - 1; y < RS_H; y++)
        for (int x = 0; x < RS_W; x++) {
            float dx = (x + 0.5f - (RS_W * 0.5f + 2.0f)) / 9.0f, dy = (y + 0.5f - (RS_GROUND + 0.5f)) / 2.4f;
            if (dx * dx + dy * dy <= 1.0f && pp_get(p, x, y).a == 0)
                pp_px(p, x, y, (Color){0, 0, 0, RS_SHADOW_A});
        }
}

// ════════════════════════════════════════════════════════════════
// VILLE — immeubles éventrés
// ════════════════════════════════════════════════════════════════
static void windows(PxCanvas *p, int x0, int x1, int y0, int y1) {
    for (int y = y0; y + 1 <= y1; y += 3)
        for (int x = x0; x + 1 <= x1; x += 3) {
            if (!pp_solid(p, x, y) || !pp_solid(p, x + 1, y + 1)) continue;
            float r = tb_rand(x, y, p->s + 13u);
            Color c = r > 0.72f ? (Color){250, 214, 96, 255} : r > 0.25f ? (Color){28, 32, 46, 255}
                                                                       : (Color){8, 8, 10, 255};
            pp_px(p, x, y, r > 0.72f ? (Color){255, 240, 170, 255} : c);
            pp_px(p, x + 1, y, c); pp_px(p, x, y + 1, c); pp_px(p, x + 1, y + 1, tb_scale(c, 0.75f));
        }
}
static void city(PxCanvas *p, int v) {
    const Color con = {118, 120, 132, 255}, side = {78, 80, 92, 255}, rust = {150, 78, 40, 255};
    if (v == 0) {
        for (int x = 3; x <= 14; x++) {                    // façade au sommet déchiqueté
            int top = 2 + (int)(tb_rand(x / 2, 1, p->s) * 6.0f);
            pp_box(p, x, top, 1, RS_GROUND - top, con, x == 3 ? 0.7f : 0.52f);
            pp_px(p, x, top, tb_scale(con, 1.35f));
        }
        pp_box(p, 15, 6, 3, RS_GROUND - 6, side, 0.40f);      // face latérale
        windows(p, 5, 13, 6, RS_GROUND - 3);
        pp_line(p, 6, 3, 6, 0, rust); pp_line(p, 11, 4, 12, 1, rust);   // fers à béton
        rubble(p, 1, 18, con);
    } else if (v == 1) {
        pp_box(p, 2, 5, 7, RS_GROUND - 5, con, 0.56f);
        pp_box(p, 9, 11, 7, RS_GROUND - 11, side, 0.48f);
        pp_box(p, 16, 13, 2, RS_GROUND - 13, tb_scale(side, 0.8f), 0.4f);
        windows(p, 3, 8, 7, RS_GROUND - 3); windows(p, 10, 15, 13, RS_GROUND - 3);
        pp_line(p, 9, 11, 13, 8, rust);                       // poutre tordue
        rubble(p, 1, 18, side);
    } else if (v == 2) {
        pp_box(p, 3, 9, 6, RS_GROUND - 9, con, 0.55f);        // pan de mur restant
        windows(p, 4, 8, 11, RS_GROUND - 3);
        pp_line(p, 8, 9, 17, 18, rust); pp_line(p, 8, 10, 16, 18, tb_scale(rust, 0.7f));   // poutrelle
        pp_disc(p, 13, RS_GROUND - 2, 5.0f, con, 0.5f);       // éboulis
        pp_disc(p, 6, RS_GROUND - 1, 3.0f, side, 0.45f);
    } else if (v == 3) {                                   // benne renversée
        const Color green = {64, 104, 72, 255};
        pp_box(p, 3, 15, 13, 7, green, 0.55f);
        pp_box(p, 2, 13, 15, 2, tb_scale(green, 0.75f), 0.5f);
        pp_line(p, 4, 17, 14, 17, tb_scale(green, 0.6f));
        pp_disc(p, 5.0f, 22.5f, 1.4f, (Color){30, 30, 32, 255}, 0.4f);
        pp_disc(p, 14.0f, 22.5f, 1.4f, (Color){30, 30, 32, 255}, 0.4f);
    } else {                                               // glissière béton + cône
        pp_box(p, 2, 17, 10, 5, con, 0.6f);
        pp_line(p, 3, 19, 10, 19, tb_scale(con, 0.75f));
        for (int y = 14; y <= RS_GROUND - 1; y++) {
            int w = (y - 14) / 2;
            for (int x = 15 - w; x <= 15 + w; x++)
                pp_px(p, x, y, (y == 18 || y == 19) ? (Color){236, 236, 228, 255}
                                                 : tb_scale((Color){236, 110, 40, 255}, x <= 15 ? 1.1f : 0.85f));
        }
    }
}

// ════════════════════════════════════════════════════════════════
// DÉSERT — épaves ensablées
// ════════════════════════════════════════════════════════════════
static void sand_drift(PxCanvas *p, int x0, int x1, int h) {
    const Color sand = {196, 152, 92, 255};
    Color r[TB_RAMP];
    tb_make_ramp(sand, r);
    for (int x = x0; x <= x1; x++) {
        int top = RS_GROUND - h + (int)((sinf(x * 0.7f) + 1.0f) * 1.2f);
        for (int y = top; y <= RS_GROUND; y++)
            pp_px(p, x, y, tb_ramp(r, (y == top ? 0.85f : 0.55f) + (tb_rand(x, y, p->s) - 0.5f) * 0.25f, x, y));
    }
}
static void desert(PxCanvas *p, int v) {
    const Color rust = {150, 84, 44, 255}, glass = {60, 92, 104, 255}, tire = {30, 28, 30, 255};
    if (v == 0) {                                          // voiture rouillée
        pp_box(p, 2, 15, 16, 6, rust, 0.55f);
        pp_box(p, 5, 10, 9, 5, tb_scale(rust, 0.85f), 0.5f);
        pp_box(p, 6, 11, 3, 3, glass, 0.6f); pp_box(p, 10, 11, 3, 3, glass, 0.45f);
        pp_disc(p, 5.5f, 21.5f, 2.2f, tire, 0.4f); pp_disc(p, 14.5f, 21.5f, 2.2f, tire, 0.4f);
        pp_px(p, 7, 17, (Color){10, 8, 8, 255}); pp_px(p, 12, 16, (Color){10, 8, 8, 255});   // impacts
        sand_drift(p, 1, 9, 4);
    } else if (v == 1) {                                   // voiture sur le toit
        pp_box(p, 3, 12, 14, 6, tb_scale(rust, 0.9f), 0.5f);
        pp_box(p, 6, 18, 8, 3, tb_scale(rust, 0.7f), 0.45f);
        pp_disc(p, 5.5f, 11.0f, 2.0f, tire, 0.45f); pp_disc(p, 14.5f, 11.0f, 2.0f, tire, 0.45f);
        sand_drift(p, 0, 19, 3);
    } else if (v == 2) {                                   // bus à moitié enfoui
        const Color paint = {182, 146, 64, 255};
        pp_box(p, 1, 9, 18, 10, paint, 0.55f);
        for (int x = 3; x <= 16; x += 3) pp_box(p, x, 11, 2, 3, glass, 0.5f);
        pp_line(p, 1, 15, 18, 15, tb_scale(paint, 0.6f));
        pp_disc(p, 14.5f, 19.5f, 2.2f, tire, 0.4f);
        sand_drift(p, 0, 11, 7);
    } else if (v == 3) {                                   // dune + crâne blanchi
        sand_drift(p, 2, 17, 5);
        const Color bone = {226, 216, 192, 255};
        pp_box(p, 7, 15, 5, 4, bone, 0.65f);
        pp_px(p, 8, 16, (Color){20, 16, 14, 255}); pp_px(p, 10, 16, (Color){20, 16, 14, 255});
        pp_line(p, 13, 19, 16, 18, bone); pp_line(p, 4, 20, 6, 19, bone);
    } else {                                               // fût enfoui + arbuste mort
        pp_box(p, 4, 14, 6, 8, rust, 0.55f);
        pp_line(p, 4, 17, 9, 17, tb_scale(rust, 0.6f));
        const Color wood = {120, 92, 60, 255};
        pp_line(p, 14, RS_GROUND, 14, 13, wood); pp_line(p, 14, 16, 17, 12, wood);
        pp_line(p, 14, 17, 11, 13, wood); pp_line(p, 15, 14, 16, 11, wood);
        sand_drift(p, 2, 17, 3);
    }
}

// ════════════════════════════════════════════════════════════════
// MARAIS — arbres morts moussus
// ════════════════════════════════════════════════════════════════
/* Arbre de marais : tronc noueux, racines de palétuvier en arche, houppier
   de mousse en grappes de sphères ombrées, lianes qui pendent. */
static void tree(PxCanvas *p, int bx, int top, int lean, unsigned h, float size) {
    const Color bark = {82, 66, 50, 255}, moss = {80, 132, 50, 255};
    Color br[TB_RAMP];
    tb_make_ramp(bark, br);
    int ctop = top + 4;                                    // haut du tronc (sous le houppier)
    for (int y = ctop; y <= RS_GROUND; y++) {              // tronc qui s'évase
        float k = (float)(y - ctop) / (RS_GROUND - ctop);
        int cx = bx + (int)((1.0f - k) * lean), w = 2 + (int)(k * 2.0f);
        for (int i = 0; i < w; i++)
            pp_px(p, cx + i, y, tb_ramp(br, (i == 0 ? 0.82f : i == w - 1 ? 0.25f : 0.5f)
                                     + (tb_rand(cx + i, y, p->s) - 0.5f) * 0.3f, cx + i, y));
    }
    for (int k = -1; k <= 1; k += 2) {                     // racines en arche
        pp_line(p, bx + 1, RS_GROUND - 3, bx + 1 + k * 3, RS_GROUND - 1, br[2]);
        pp_line(p, bx + 1 + k * 3, RS_GROUND - 1, bx + 1 + k * 5, RS_GROUND + 1, br[1]);
    }
    int cx = bx + lean / 2 + 1, cy = top + 3;
    pp_disc(p, cx, cy + 1.5f, 4.2f * size, tb_scale(moss, 0.55f), 0.4f);   // sous-couche d'ombre
    for (int k = 0; k < 5; k++) {                          // grappes du houppier
        float ax = cx + ((float)((h >> (k * 3)) & 7) - 3.5f) * 1.2f * size;
        float ay = cy + ((float)((h >> (k * 3 + 1)) & 3) - 1.5f) * 1.1f;
        pp_disc(p, ax, ay, (2.0f + (float)((h >> (k + 9)) & 1)) * size, moss, 0.62f);
    }
    for (int k = 0; k < 4; k++) {                          // lianes
        int vx = cx - 3 + k * 2 + (int)((h >> (k + 4)) & 1);
        int vy = cy + 3, len = 3 + (int)((h >> (k * 2 + 13)) & 3);
        pp_line(p, vx, vy, vx, vy + len, tb_scale(moss, 0.75f));
        pp_px(p, vx, vy + len, tb_scale(moss, 1.2f));
    }
}
static void swamp(PxCanvas *p, int v) {
    unsigned h = tb_hash(v, 1, p->s);
    const Color moss = {80, 132, 50, 255};
    if (v == 0) tree(p, 8, 2, 3, h, 1.15f);
    else if (v == 1) { tree(p, 4, 7, -2, h, 0.85f); tree(p, 13, 2, 2, h >> 7, 1.0f); }
    else if (v == 3) {                                     // buisson à baies
        pp_disc(p, 7.0f, 19.0f, 4.0f, tb_scale(moss, 0.6f), 0.4f);
        pp_disc(p, 6.0f, 18.0f, 3.4f, moss, 0.6f); pp_disc(p, 11.0f, 19.0f, 3.0f, moss, 0.55f);
        pp_disc(p, 14.0f, 20.0f, 2.4f, moss, 0.5f);
        pp_px(p, 7, 16, (Color){220, 60, 80, 255}); pp_px(p, 11, 18, (Color){220, 60, 80, 255});
        pp_px(p, 13, 19, (Color){240, 120, 140, 255});
    } else if (v == 4) {                                   // souche + champignons + roseaux
        const Color bark = {96, 76, 54, 255};
        pp_box(p, 6, 15, 6, 7, bark, 0.55f);
        for (int x = 7; x <= 10; x++) pp_px(p, x, 15, (Color){176, 146, 104, 255});   // cerne
        pp_px(p, 12, 18, (Color){200, 70, 50, 255}); pp_px(p, 13, 18, (Color){220, 90, 70, 255});
        pp_px(p, 12, 17, (Color){240, 160, 140, 255});
        for (int k = 0; k < 3; k++) pp_line(p, 15 + k, RS_GROUND, 15 + k, 15 + k * 2, tb_scale(moss, 0.9f + k * 0.1f));
    } else {                                               // tronc couché + souche
        const Color bark = {96, 76, 54, 255};
        pp_box(p, 1, 17, 17, 4, bark, 0.5f);
        pp_disc(p, 1.5f, 19.0f, 2.0f, (Color){170, 140, 100, 255}, 0.6f);  // cerne
        pp_box(p, 12, 11, 5, 7, bark, 0.55f);
        pp_px(p, 13, 10, (Color){200, 70, 50, 255}); pp_px(p, 14, 10, (Color){220, 90, 70, 255});
        pp_px(p, 15, 10, (Color){200, 70, 50, 255}); pp_px(p, 14, 9, (Color){240, 160, 140, 255});
        pp_disc(p, 6.0f, 16.5f, 1.8f, (Color){84, 136, 52, 255}, 0.6f);
    }
}

// ════════════════════════════════════════════════════════════════
// USINE — cuves, caisses, machines
// ════════════════════════════════════════════════════════════════
static void factory(PxCanvas *p, int v) {
    const Color steel = {120, 116, 112, 255}, rust = {150, 74, 34, 255};
    if (v == 0) {                                          // cuve chimique cerclée
        Color r[TB_RAMP];
        tb_make_ramp(steel, r);
        for (int y = 4; y <= RS_GROUND - 1; y++)
            for (int x = 4; x <= 15; x++) {
                float v2 = 0.25f + 0.62f * sinf(3.14159f * (x - 3.5f) / 12.5f) + (x == 7 ? 0.2f : 0.0f)
                         + (tb_rand(x, y, p->s) - 0.5f) * 0.12f;
                if (y == 9 || y == 15 || y == 21) v2 -= 0.32f;    // cerclages
                if (y == 8 || y == 14 || y == 20) v2 += 0.14f;
                pp_px(p, x, y, tb_ramp(r, v2, x, y));
            }
        for (int x = 5; x <= 14; x++) pp_px(p, x, 3, r[4]);
        for (int k = 0; k < 4; k++) {                      // coulures de rouille
            int x = 5 + (int)(tb_rand(k, 9, p->s) * 9.0f), y0 = (k & 1) ? 10 : 16;
            for (int y = y0; y < y0 + 3 + k % 3; y++) pp_px(p, x, y, tb_scale(rust, 1.0f - 0.08f * (y - y0)));
        }
        pp_box(p, 8, 11, 4, 3, (Color){226, 186, 44, 255}, 0.6f);     // étiquette danger
        pp_px(p, 9, 12, (Color){20, 16, 12, 255}); pp_px(p, 10, 12, (Color){20, 16, 12, 255});
        pp_box(p, 16, 16, 4, 3, tb_scale(steel, 0.8f), 0.5f);          // tuyau
    } else if (v == 1) {                                   // pile de caisses
        const Color wood = {150, 108, 60, 255};
        pp_box(p, 2, 15, 8, 8, wood, 0.55f); pp_box(p, 10, 16, 8, 7, tb_scale(wood, 0.9f), 0.5f);
        pp_box(p, 5, 8, 7, 7, wood, 0.6f);
        pp_line(p, 2, 15, 9, 22, tb_scale(wood, 0.6f)); pp_line(p, 5, 8, 11, 14, tb_scale(wood, 0.6f));
        pp_px(p, 12, 19, (Color){226, 186, 44, 255}); pp_px(p, 13, 19, (Color){226, 186, 44, 255});
    } else if (v == 3) {                                   // fûts de produits dangereux
        const Color yel = {214, 176, 40, 255};
        for (int k = 0; k < 3; k++) {
            int bx = 2 + k * 5 + (k == 1), by = (k == 1) ? 12 : 15;
            pp_box(p, bx, by, 5, RS_GROUND - by, k == 2 ? tb_scale(rust, 1.1f) : yel, 0.55f);
            pp_line(p, bx, by + 2, bx + 4, by + 2, (Color){24, 20, 16, 255});
            pp_px(p, bx + 2, by + 4, (Color){24, 20, 16, 255});
        }
    } else if (v == 4) {                                   // nœud de tuyaux + vanne
        pp_box(p, 1, 17, 18, 3, tb_scale(steel, 0.85f), 0.55f);
        pp_box(p, 8, 9, 3, 9, tb_scale(steel, 0.85f), 0.55f);
        pp_box(p, 7, 16, 5, 5, steel, 0.6f);
        pp_disc(p, 9.5f, 8.0f, 3.0f, (Color){190, 50, 40, 255}, 0.6f);   // volant rouge
        pp_disc(p, 9.5f, 8.0f, 1.0f, steel, 0.4f);
        pp_px(p, 15, 16, rust); pp_px(p, 15, 15, tb_scale(rust, 0.8f));
    } else {                                               // machine éventrée + engrenage
        pp_box(p, 3, 10, 13, RS_GROUND - 10, tb_scale(steel, 0.7f), 0.5f);
        pp_disc(p, 13.0f, 9.0f, 4.0f, rust, 0.55f);
        for (int k = 0; k < 8; k++) {                       // dents de l'engrenage
            float a = k * 0.785f;
            pp_px(p, 13 + (int)roundf(cosf(a) * 5.0f), 9 + (int)roundf(sinf(a) * 5.0f), tb_scale(rust, 0.8f));
        }
        pp_disc(p, 13.0f, 9.0f, 1.2f, steel, 0.3f);
        pp_px(p, 5, 13, (Color){255, 70, 50, 255}); pp_px(p, 7, 13, (Color){90, 240, 110, 255});   // voyants
        pp_line(p, 4, 17, 6, RS_GROUND, (Color){30, 30, 34, 255});                             // câble
    }
}

// ════════════════════════════════════════════════════════════════
// TERRES BRÛLÉES — béton, ferraille, carcasse calcinée
// ════════════════════════════════════════════════════════════════
static void wasteland(PxCanvas *p, int v) {
    const Color con = {132, 118, 100, 255}, rust = {150, 80, 40, 255}, brick = {150, 72, 52, 255};
    if (v == 0) {                                          // mur effondré
        for (int x = 2; x <= 16; x++) {
            int top = 7 + (int)(fabsf(sinf(x * 0.9f)) * 4.0f + tb_rand(x, 2, p->s) * 3.0f);
            pp_box(p, x, top, 1, RS_GROUND - top, con, x < 4 ? 0.68f : 0.52f);
        }
        pp_box(p, 6, 14, 4, 3, brick, 0.5f); pp_box(p, 11, 17, 3, 3, brick, 0.45f);   // briques à nu
        pp_line(p, 5, 8, 4, 3, rust); pp_line(p, 9, 9, 10, 4, rust); pp_line(p, 13, 10, 15, 6, rust);
        rubble(p, 1, 18, con);
    } else if (v == 1) {                                   // dalles empilées de travers
        for (int j = 0; j < 4; j++) pp_box(p, 3 + j, 14 + j, 11, 1, con, 0.6f - j * 0.05f);
        for (int j = 0; j < 4; j++) pp_box(p, 8 - j, 9 + j, 9, 1, tb_scale(con, 0.9f), 0.55f);
        pp_line(p, 9, 9, 7, 3, rust); pp_line(p, 12, 10, 15, 5, rust);
        rubble(p, 2, 17, con);
    } else if (v == 3) {                                   // pneus empilés
        const Color rub = {44, 40, 42, 255};
        pp_disc(p, 9.0f, 20.0f, 4.2f, rub, 0.45f); pp_disc(p, 9.0f, 20.0f, 1.5f, (Color){12, 10, 10, 255}, 0.2f);
        pp_disc(p, 12.0f, 15.0f, 3.8f, rub, 0.5f); pp_disc(p, 12.0f, 15.0f, 1.3f, (Color){12, 10, 10, 255}, 0.2f);
    } else if (v == 4) {                                   // bidon rouillé + gravats
        pp_box(p, 6, 13, 6, 9, rust, 0.55f);
        pp_line(p, 6, 16, 11, 16, tb_scale(rust, 0.6f)); pp_line(p, 6, 19, 11, 19, tb_scale(rust, 0.6f));
        rubble(p, 2, 17, con);
    } else {                                               // carcasse de voiture calcinée
        const Color char_ = {58, 52, 50, 255};
        pp_box(p, 2, 14, 16, 7, char_, 0.5f);
        pp_box(p, 5, 9, 9, 5, tb_scale(char_, 0.85f), 0.45f);
        pp_box(p, 6, 10, 7, 3, (Color){14, 12, 12, 255}, 0.3f);
        pp_disc(p, 5.5f, 21.0f, 2.0f, (Color){24, 22, 22, 255}, 0.4f);
        pp_disc(p, 14.5f, 21.0f, 2.0f, (Color){24, 22, 22, 255}, 0.4f);
        for (int k = 0; k < 4; k++) pp_px(p, 4 + k * 4, 15 + (k & 1), tb_mix(rust, (Color){255, 120, 40, 255}, (k & 1) * 0.6f));
    }
}

// ════════════════════════════════════════════════════════════════
// ATLAS + DESSIN
// ════════════════════════════════════════════════════════════════
static void bake_atlas(ThemeID th) {
    Image im = GenImageColor(RS_W * RS_VARIANTS, RS_H, BLANK);
    for (int v = 0; v < RS_VARIANTS; v++) {
        PxCanvas p = {&im, v * RS_W, 0, RS_W, RS_H, 0xB0A7u + (unsigned)th * 131u + (unsigned)v * 7u};
        switch (th) {
            case THEME_CITY:    city(&p, v);      break;
            case THEME_DESERT:  desert(&p, v);    break;
            case THEME_SWAMP:   swamp(&p, v);     break;
            case THEME_FACTORY: factory(&p, v);   break;
            case THEME_WASTELAND:
            default:            wasteland(&p, v); break;
        }
        finish(&p);
    }
    g_atlas[th] = LoadTextureFromImage(im);
    SetTextureFilter(g_atlas[th], TEXTURE_FILTER_POINT);
    UnloadImage(im);
    g_atlas_ok[th] = 1;
}

void terrain_draw_ruin(const Map *map, int tx, int ty) {
    ThemeID th = map->theme;
    if (th < 0 || th >= THEME_COUNT) th = THEME_WASTELAND;
    if (!g_atlas_ok[th]) bake_atlas(th);
    /* Grande structure ou petit objet selon le thème : une zone de ruines
       devient un paysage varié, pas une rangée de pierres tombales. */
    unsigned h = tb_hash(tx, ty, 0x5EEDu + (unsigned)map->seed);
    int v = (h % 100u < RS_LARGE_PCT[th]) ? (int)((h >> 8) % RS_LARGE)
                                      : RS_LARGE + (int)((h >> 8) % (RS_VARIANTS - RS_LARGE));
    int flip = (int)((tb_hash(tx, ty, 0xF11Bu) >> 3) & 1);      // miroir : plus de variété
    Rectangle src = {(float)(v * RS_W), 0, (float)(flip ? -RS_W : RS_W), (float)RS_H};
    Rectangle dst = {(float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE - (RS_H - TB_TILE) * TB_PX),
                     (float)(RS_W * TB_PX), (float)(RS_H * TB_PX)};
    DrawTexturePro(g_atlas[th], src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

void terrain_ruins_unload(void) {
    for (int i = 0; i < THEME_COUNT; i++)
        if (g_atlas_ok[i]) { UnloadTexture(g_atlas[i]); g_atlas_ok[i] = 0; }
}

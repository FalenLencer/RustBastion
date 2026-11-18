/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  ui_plate.c ─ Habillage « plaque de métal rivetée » (cf. ui_plate.h). */
#include "ui_plate.h"
#include "ui_utils.h"   /* dtxt / mtxt / fh */
#include "ui_palette.h" /* UI_ACCENT_DIM / UI_TEXT_DIM (indications de touches) */

#define PLATE_SHADOW_KX  0.45f  /* ombre portée : décalage = chanfrein × k   */
#define PLATE_SHADOW_KY  0.55f
#define PLATE_SHADOW_A   110
#define PLATE_RIVET_STEP 64     /* pas des rivets sur le cadre (px)          */
#define PLATE_GLOW_H     26     /* lueur colorée sous l'arête haute (px)     */
#define PLATE_GLOW_K     0.22f  /* opacité de la lueur / opacité du liseré   */
#define PLATE_KEY_FS     9      /* police des touches                        */
#define PLATE_SEC_FS     9      /* police des titres de section              */
#define PLATE_GRIP_N     3      /* prises latérales d'un bouton (traits)     */
#define PLATE_STRIPE     5      /* largeur d'une rayure de danger (px)       */

static float g_pa = 1.0f;       /* opacité globale (plate_alpha)             */

void plate_alpha(float a) { g_pa = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a; }
/* Toute couleur dessinée ici passe par A() : fondu d'ouverture uniforme. */
static Color A(Color c) { c.a = (unsigned char)((float)c.a * g_pa); return c; }

Color plate_fade(Color c, float a) {
    c.a = (unsigned char)((float)c.a * (a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a));
    return c;
}
Rectangle plate_inset(Rectangle r, float d) {
    return (Rectangle){r.x + d, r.y + d, r.width - 2.0f * d, r.height - 2.0f * d};
}

/* raylib veut le sens trigonométrique, sinon le triangle est éliminé. */
static void tri_raw(Vector2 a, Vector2 b, Vector2 c, Color col) {
    if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0.0f) { Vector2 t = b; b = c; c = t; }
    DrawTriangle(a, b, c, col);
}
void plate_tri(Vector2 a, Vector2 b, Vector2 c, Color col) { tri_raw(a, b, c, A(col)); }

/* 3 bandes + 4 triangles, SANS recouvrement : une couleur translucide
   reste uniforme. */
void plate_chamfer_fill(Rectangle r, float c, Color col) {
    float x0 = r.x, y0 = r.y, x1 = r.x + r.width, y1 = r.y + r.height;
    col = A(col);
    DrawRectangleRec((Rectangle){x0 + c, y0, r.width - 2.0f * c, r.height}, col);
    DrawRectangleRec((Rectangle){x0, y0 + c, c, r.height - 2.0f * c}, col);
    DrawRectangleRec((Rectangle){x1 - c, y0 + c, c, r.height - 2.0f * c}, col);
    tri_raw((Vector2){x0, y0 + c}, (Vector2){x0 + c, y0 + c}, (Vector2){x0 + c, y0}, col);
    tri_raw((Vector2){x1 - c, y0}, (Vector2){x1 - c, y0 + c}, (Vector2){x1, y0 + c}, col);
    tri_raw((Vector2){x0, y1 - c}, (Vector2){x0 + c, y1}, (Vector2){x0 + c, y1 - c}, col);
    tri_raw((Vector2){x1 - c, y1 - c}, (Vector2){x1 - c, y1}, (Vector2){x1, y1 - c}, col);
}

void plate_chamfer_line(Rectangle r, float c, Color col) {
    float x0 = r.x + 0.5f, y0 = r.y + 0.5f, x1 = r.x + r.width - 0.5f, y1 = r.y + r.height - 0.5f;
    Vector2 p[8] = {{x0 + c, y0}, {x1 - c, y0}, {x1, y0 + c}, {x1, y1 - c},
                    {x1 - c, y1}, {x0 + c, y1}, {x0, y1 - c}, {x0, y0 + c}};
    col = A(col);
    for (int i = 0; i < 8; i++) DrawLineV(p[i], p[(i + 1) % 8], col);
}

void plate_draw(Rectangle box, float c) {
    plate_chamfer_fill((Rectangle){box.x + c * PLATE_SHADOW_KX, box.y + c * PLATE_SHADOW_KY,
                                   box.width, box.height}, c, (Color){0, 0, 0, PLATE_SHADOW_A});
    plate_chamfer_fill(box, c, PLATE_FRAME_D);
    plate_chamfer_fill(plate_inset(box, 1.0f), c - 0.5f, PLATE_FRAME);
    DrawLineV((Vector2){box.x + c, box.y + 1.5f},
              (Vector2){box.x + box.width - c, box.y + 1.5f}, A(PLATE_FRAME_L));    // biseau haut
    DrawLineV((Vector2){box.x + 1.5f, box.y + c},
              (Vector2){box.x + 1.5f, box.y + box.height - c}, A(PLATE_FRAME_L));   // biseau gauche
    for (int x = (int)(box.x + c) + PLATE_RIVET_STEP / 2; x < (int)(box.x + box.width - c);
         x += PLATE_RIVET_STEP) {                                                  // rivets
        DrawRectangle(x, (int)box.y + 1, 2, 2, A(PLATE_FRAME_L));
        DrawRectangle(x, (int)(box.y + box.height) - 3, 2, 2, A(PLATE_FRAME_L));
    }
}

void plate_panel(Rectangle box, float c, Color acc, float acc_a) {
    plate_draw(box, c);
    Rectangle in = plate_inset(box, PLATE_BORDER);
    float     ic = c - PLATE_BORDER * 0.6f;
    if (ic < 2.0f) ic = 2.0f;
    plate_chamfer_fill(in, ic, PLATE_PANEL);
    int gh = (int)(in.height / 3.0f);
    if (gh > PLATE_GLOW_H) gh = PLATE_GLOW_H;
    DrawRectangleGradientV((int)(in.x + ic), (int)in.y + 1, (int)(in.width - 2.0f * ic), gh,
                           A(plate_fade(acc, acc_a * PLATE_GLOW_K)), (Color){acc.r, acc.g, acc.b, 0});
    plate_chamfer_line(plate_inset(in, 1.0f), ic, plate_fade(acc, acc_a));
}

void plate_button(Rectangle b, Color acc, int hov, int enabled) {
    float c = PLATE_CHAMFER_S;
    if (hov && enabled)                                    /* halo de survol */
        plate_chamfer_line(plate_inset(b, -2.0f), c + 1.0f, plate_fade(acc, 0.45f));
    plate_draw(b, c);
    Rectangle in = plate_inset(b, 3.0f);
    float     ic = c - 2.0f;
    Color body = enabled ? (Color){(unsigned char)(acc.r * (hov ? 0.30f : 0.16f)),
                                   (unsigned char)(acc.g * (hov ? 0.30f : 0.16f)),
                                   (unsigned char)(acc.b * (hov ? 0.30f : 0.16f)), 255}
                         : (Color){24, 22, 20, 255};
    plate_chamfer_fill(in, ic, body);
    DrawRectangleGradientV((int)(in.x + ic), (int)in.y + 1, (int)(in.width - 2.0f * ic),
                           (int)(in.height * 0.45f), A(plate_fade(WHITE, enabled ? 0.07f : 0.03f)),
                           (Color){255, 255, 255, 0});
    plate_chamfer_line(plate_inset(in, 1.0f), ic,
                       plate_fade(acc, !enabled ? 0.25f : hov ? 1.0f : 0.70f));
    Color grip = A(plate_fade(PLATE_FRAME_L, enabled ? 0.8f : 0.4f));
    for (int k = 0; k < PLATE_GRIP_N; k++) {               /* prises latérales */
        int gx = (int)in.x + 6 + k * 3, gx2 = (int)(in.x + in.width) - 7 - k * 3;
        int gy = (int)in.y + 6, gh = (int)in.height - 12;
        DrawRectangle(gx, gy, 1, gh, grip);
        DrawRectangle(gx2, gy, 1, gh, grip);
    }
}

int plate_keycap(int x, int y, const char *lbl, Color col) {
    int w = mtxt(lbl, PLATE_KEY_FS) + 10, h = fh(PLATE_KEY_FS) + 4;
    DrawRectangle(x, y, w, h + 2, A(PLATE_FRAME_D));                    // épaisseur de la touche
    DrawRectangle(x + 1, y + 1, w - 2, h - 1, A(PLATE_FRAME));          // dessus
    DrawRectangle(x + 1, y + 1, w - 2, 1, A(PLATE_FRAME_L));            // arête éclairée
    DrawRectangle(x + 1, y + h, w - 2, 1, A(plate_fade(col, 0.55f)));   // filet coloré
    dtxt(lbl, x + 5, y + 2, PLATE_KEY_FS, A(col));
    return w;
}

void plate_section(int x, int y, int w, const char *txt, Color col) {
    int h = fh(PLATE_SEC_FS);
    DrawRectangle(x, y, 3, h, A(col));                                   // repère
    dtxt(txt, x + 8, y, PLATE_SEC_FS, A(col));
    DrawRectangleGradientH(x, y + h + 2, w, 1, A(plate_fade(col, 0.55f)), (Color){col.r, col.g, col.b, 0});
}

int plate_key_hint(int x, int y, const char *key, const char *txt, int draw) {
    int kw = mtxt(key, PLATE_KEY_FS) + 10;
    if (draw) {
        plate_keycap(x, y, key, UI_ACCENT_DIM);
        dtxt(txt, x + kw + 6, y + 2, PLATE_KEY_FS, A(UI_TEXT_DIM));
    }
    return kw + 6 + mtxt(txt, PLATE_KEY_FS);
}

/* Par ligne, des segments de PLATE_STRIPE px qui alternent (décalés d'1 px
   par ligne → diagonales) : quelques rectangles par ligne, pas un par pixel. */
void plate_hazard(int x, int y, int w, int h, Color col) {
    Color dark = A(PLATE_FRAME_D), lite = A(col);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w;) {
            int ph  = (i + j) % (2 * PLATE_STRIPE);
            int run = (ph < PLATE_STRIPE ? PLATE_STRIPE : 2 * PLATE_STRIPE) - ph;
            if (i + run > w) run = w - i;
            DrawRectangle(x + i, y + j, run, 1, ph < PLATE_STRIPE ? lite : dark);
            i += run;
        }
}

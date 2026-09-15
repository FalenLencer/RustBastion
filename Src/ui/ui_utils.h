/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../engine/assets.h"
#include <string.h>

// ── Helpers texte avec police étendue g_font ─────────────────────────────
// Utiliser dtxt() / mtxt() à la place de DrawText() / MeasureText()
// pour afficher les caractères accentués, tirets cadratins, symboles, etc.
//
// FONT_SCALE : facteur d'agrandissement global de Rajdhani par rapport à
// GROBOLD (même fontSize mais glyphes visuellement plus petits).
// Changer cette valeur agrandit/réduit tout le texte du jeu d'un coup.
#define FONT_SCALE 1.6f

static inline void dtxt(const char *text, int x, int y, int fontSize, Color color) {
    DrawTextEx(g_font, text,
               (Vector2){(float)x, (float)y},
               (float)fontSize * FONT_SCALE, 1.0f, color);
}
static inline int mtxt(const char *text, int fontSize) {
    return (int)MeasureTextEx(g_font, text, (float)fontSize * FONT_SCALE, 1.0f).x;
}
// Hauteur réelle d'un texte rendu à fontSize (pour le centrage vertical)
static inline int fh(int fontSize) {
    return (int)((float)fontSize * FONT_SCALE + 0.5f);
}
// Flottant aléatoire uniforme dans [lo, hi] — source unique (était
// dupliqué dans render3d_fx.c et ambient.c).
static inline float ui_frnd(float lo, float hi) {
    return lo + (hi - lo) * (float)GetRandomValue(0, 1000) / 1000.0f;
}
// Teinte décalée par canal, bornée [0,255] — SOURCE UNIQUE des helpers
// de couleur (2D tile_art + 3D w3d_* délèguent ici).
static inline Color ui_tint3(Color c, int dr, int dg, int db) {
    int r = c.r + dr, g = c.g + dg, b = c.b + db;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, c.a};
}
static inline Color ui_shade(Color c, int d) { return ui_tint3(c, d, d, d); }
// Texte avec OMBRE portée 1 px : pour tout texte posé sur la carte ou le
// monde 3D (popups de dégâts/or…), où le fond varie — lisibilité garantie.
static inline void dtxt_o(const char *text, int x, int y, int fontSize,
                          Color color) {
    dtxt(text, x + 1, y + 1, fontSize,
         (Color){10, 8, 6, (unsigned char)(color.a * 3 / 4)});
    dtxt(text, x, y, fontSize, color);
}
// Dessine une icône texturée carrée de `size` pixels, tintée par `tint`.
// Sans effet si la texture n'est pas chargée (id == 0).
static inline void draw_icon(Texture2D tex, int x, int y, int size, Color tint) {
    if (tex.id == 0) return;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){(float)x, (float)y, (float)size, (float)size},
        (Vector2){0, 0}, 0.0f, tint);
}
// ─────────────────────────────────────────────────────────────────────────

void ui_clip_text(const char *src, int max_w, int fs,
                  char *buf, int buf_sz);

static inline void clip_text(const char *src, int max_w, int fs,
                             char *buf, int buf_sz)
{
    ui_clip_text(src, max_w, fs, buf, buf_sz);
}

int adaptive_fs(int base_fs);
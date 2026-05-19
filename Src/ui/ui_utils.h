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
#define FONT_SCALE 1.4f

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
// ─────────────────────────────────────────────────────────────────────────

void ui_clip_text(const char *src, int max_w, int fs,
                  char *buf, int buf_sz);

static inline void clip_text(const char *src, int max_w, int fs,
                             char *buf, int buf_sz)
{
    ui_clip_text(src, max_w, fs, buf, buf_sz);
}

static inline void safe_clip(const char *src, int max_w, int fs,
                             char *buf, int buf_sz)
{
    ui_clip_text(src, max_w, fs, buf, buf_sz);
}

int adaptive_fs(int base_fs);
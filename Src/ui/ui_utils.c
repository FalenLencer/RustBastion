/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "ui_utils.h"
#include "raylib.h"
#include <string.h>

/* Recule i jusqu'au début d'un caractère UTF-8 : une coupe ne tombe jamais au
 * milieu d'une séquence (sinon raylib affiche « ? » à la place de l'accent). */
static int utf8_floor(const char *s, int i)
{
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
    return i;
}

void ui_clip_text(const char *src, int max_w, int fs,
                  char *buf, int buf_sz)
{
    if (!src || !buf || buf_sz <= 0) return;

    int n = (int)strlen(src);
    int i = (n < buf_sz - 1) ? n : utf8_floor(src, buf_sz - 1);
    memcpy(buf, src, (size_t)i);
    buf[i] = '\0';

    if (max_w <= 0) return;

    while (i > 0 && mtxt(buf, fs) > max_w) {
        i = utf8_floor(buf, i - 1);
        buf[i] = '\0';
    }

    if (i < n && i > 3) {
        int j = utf8_floor(buf, i - 3);   /* "..." remplace des caractères entiers */
        memcpy(buf + j, "...", 4);
    }
}

/* Longueur ANNONCÉE par l'octet de tête d'un caractère UTF-8 (1 à 4). */
static int utf8_need(unsigned char c)
{
    return (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2
         : ((c & 0xF0) == 0xE0) ? 3 : ((c & 0xF8) == 0xF0) ? 4 : 1;
}

/* Longueur (octets) du caractère UTF-8 qui commence en s, bornée au NUL. */
static int utf8_char_len(const char *s)
{
    int n = utf8_need((unsigned char)s[0]);
    for (int k = 1; k < n; k++)
        if (!s[k]) return k;
    return n;
}

void ui_utf8_fix_tail(char *s)
{
    if (!s || !s[0]) return;
    int j = utf8_floor(s, (int)strlen(s) - 1);     /* début du dernier caractère */
    if (utf8_char_len(s + j) < utf8_need((unsigned char)s[j]))
        s[j] = '\0';                               /* séquence coupée : on l'ôte */
}

void ui_draw_tracked(const char *s, float cx, int y, int fs, float spacing, Color col)
{
    if (!s) return;
    char one[5];
    float total = 0.0f;
    for (int i = 0; s[i]; ) {                       /* largeur totale */
        int n = utf8_char_len(s + i);
        memcpy(one, s + i, (size_t)n); one[n] = '\0';
        total += (float)mtxt(one, fs);
        i += n;
        if (s[i]) total += spacing;
    }
    float lx = cx - total * 0.5f;
    for (int i = 0; s[i]; ) {                       /* dessin lettre à lettre */
        int n = utf8_char_len(s + i);
        memcpy(one, s + i, (size_t)n); one[n] = '\0';
        dtxt(one, (int)lx, y, fs, col);
        lx += (float)mtxt(one, fs) + spacing;
        i += n;
    }
}

int adaptive_fs(int base_fs) {
    (void)base_fs;
    return base_fs;
}
/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  pixel_paint.c ─ Primitives de sprites pixel art (cf. pixel_paint.h).
 *  Rampes, bruit et tramage : terrain_internal.h (source unique).
 */
#include "pixel_paint.h"
#include "terrain_internal.h"
#include <string.h>

#define PP_MAX 64   // côté max d'une case (masques temporaires)

void pp_px(PxCanvas *c, int x, int y, Color col) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    tb_put(c->im, c->ox + x, c->oy + y, col);
}
Color pp_get(const PxCanvas *c, int x, int y) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return (Color){0, 0, 0, 0};
    return tb_get(c->im, c->ox + x, c->oy + y);
}
int pp_solid(const PxCanvas *c, int x, int y) {
    return pp_get(c, x, y).a > 200;
}

void pp_box(PxCanvas *c, int x, int y, int w, int h, Color base, float b) {
    Color r[TB_RAMP];
    tb_make_ramp(base, r);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            float v = b + (tb_fbm((x + i) / 3.0f, (y + j) / 3.0f, c->s, 2) - 0.5f) * 0.28f;
            if (j == 0) v += 0.26f; else if (i == 0) v += 0.16f;
            if (i == w - 1) v -= 0.22f;
            if (j == h - 1) v -= 0.18f;
            pp_px(c, x + i, y + j, tb_ramp(r, v, c->ox + x + i, c->oy + y + j));
        }
}

void pp_disc(PxCanvas *c, float cx, float cy, float rad, Color base, float b) {
    Color r[TB_RAMP];
    tb_make_ramp(base, r);
    for (int y = (int)(cy - rad - 1); y <= (int)(cy + rad + 1); y++)
        for (int x = (int)(cx - rad - 1); x <= (int)(cx + rad + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy > rad * rad) continue;
            float v = b - (dx + dy) / (rad * 2.6f) + (tb_rand(x, y, c->s) - 0.5f) * 0.18f;
            pp_px(c, x, y, tb_ramp(r, v, c->ox + x, c->oy + y));
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

void pp_poly(PxCanvas *c, const Vector2 *pts, int n, Color base, float b) {
    float x0 = pts[0].x, x1 = pts[0].x, y0 = pts[0].y, y1 = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].x < x0) x0 = pts[i].x;
        if (pts[i].x > x1) x1 = pts[i].x;
        if (pts[i].y < y0) y0 = pts[i].y;
        if (pts[i].y > y1) y1 = pts[i].y;
    }
    Color r[TB_RAMP];
    tb_make_ramp(base, r);
    float span = (x1 - x0) + (y1 - y0) + 0.01f;
    for (int y = (int)y0; y <= (int)y1; y++)
        for (int x = (int)x0; x <= (int)x1; x++) {
            if (!inside(pts, n, x + 0.5f, y + 0.5f)) continue;
            float v = b + 0.30f - 0.60f * ((x - x0) + (y - y0)) / span
                    + (tb_rand(x, y, c->s + 3u) - 0.5f) * 0.14f;
            pp_px(c, x, y, tb_ramp(r, v, c->ox + x, c->oy + y));
        }
}

void pp_line(PxCanvas *c, int x0, int y0, int x1, int y1, Color col) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        pp_px(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

void pp_outline(PxCanvas *c, Color col) {
    static unsigned char m[PP_MAX][PP_MAX];
    int w = c->w < PP_MAX ? c->w : PP_MAX, h = c->h < PP_MAX ? c->h : PP_MAX;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) m[y][x] = (unsigned char)pp_solid(c, x, y);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (m[y][x]) continue;
            if ((x > 0 && m[y][x - 1]) || (x < w - 1 && m[y][x + 1]) ||
                (y > 0 && m[y - 1][x]) || (y < h - 1 && m[y + 1][x]))
                pp_px(c, x, y, col);
        }
}

void pp_glow(PxCanvas *c, Color col, int radius) {
    /* Masque de la silhouette FIGÉ avant d'écrire le halo : sinon le halo
       déjà posé compterait comme silhouette et s'étendrait en cascade. */
    static unsigned char m[PP_MAX][PP_MAX];
    int w = c->w < PP_MAX ? c->w : PP_MAX, h = c->h < PP_MAX ? c->h : PP_MAX;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) m[y][x] = (unsigned char)(pp_get(c, x, y).a > 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (m[y][x]) continue;
            float best = 1e9f;                          // distance à la silhouette
            for (int j = -radius; j <= radius; j++)
                for (int i = -radius; i <= radius; i++) {
                    int xx = x + i, yy = y + j;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h || !m[yy][xx]) continue;
                    float dd = sqrtf((float)(i * i + j * j));
                    if (dd < best) best = dd;
                }
            if (best > radius) continue;
            Color g = col;
            g.a = (unsigned char)(col.a * (1.0f - (best - 1.0f) / (float)radius) * 0.55f);
            if (g.a) pp_px(c, x, y, g);
        }
}

void pp_sparkle(PxCanvas *c, int x, int y, int len, Color col) {
    Color core = {255, 255, 255, 255};
    pp_px(c, x, y, core);
    for (int k = 1; k <= len; k++) {
        Color e = col;
        e.a = (unsigned char)(255 - k * (200 / (len + 1)));
        pp_px(c, x + k, y, e); pp_px(c, x - k, y, e);
        pp_px(c, x, y + k, e); pp_px(c, x, y - k, e);
    }
}

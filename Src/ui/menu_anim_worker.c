/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim_worker.c ─ Ouvrier de l'écran titre en pixel art articulé
 *  (cf. menu_anim_worker.h).
 *
 *  Recette de peinture (identique aux sprites du jeu) : chaque membre est un
 *  segment épais peint en ton d'OMBRE puis recouvert du ton MOYEN décalé
 *  vers la lumière (haut-gauche) → aplats nets, ombre propre côté bas-droit ;
 *  membres lointains assombris ; puis liseré chaud de contre-jour (le soleil
 *  couchant du décor est derrière) et contour sombre d'1 px. Pas de bruit ni
 *  de tramage : l'animation ne « grouille » pas.
 *  Ouvrier rust-punk : casque de chantier à lampe, lunettes, écharpe-masque
 *  qui flotte, veste olive à bandes réfléchissantes, ceinture à sacoche,
 *  genouillère, rangers. La caisse-tour est peinte dans le repère LOCAL de
 *  la caisse (échantillonnage inverse) : elle tourne sans pixels flous.
 */
#include "menu_anim_worker.h"
#include <math.h>
#include <string.h>

// ── Squelette (px d'art) ────────────────────────────────────────────
#define WK_THIGH     11.0f
#define WK_SHIN      10.5f
#define WK_TORSO     14.0f
#define WK_NECK       1.6f
#define WK_UPPER      8.5f
#define WK_FORE       8.0f
#define WK_HEAD_R     4.3f
#define WK_FAR        0.70f     /* assombrissement des membres lointains   */
#define WK_PI         PI        /* (raylib)                                */

// ── Images de travail ───────────────────────────────────────────────
#define WK_IMG_W     64
#define WK_IMG_H     86
#define WK_ORX       26         /* point au sol dans l'image               */
#define WK_ORY       82
#define CR_IMG       32         /* caisse en vol (image carrée)            */
#define CR_HX         6.0f      /* demi-largeur / demi-hauteur de la caisse */
#define CR_HY         4.5f
#define CR_SQUASH     0.35f     /* écrasement max à l'impact               */

// ── Palette ─────────────────────────────────────────────────────────
#define C_JACKET     ((Color){ 98, 108,  64, 255})
#define C_JACKET_D   ((Color){ 60,  68,  42, 255})
#define C_STRIPE     ((Color){236, 132,  48, 255})
#define C_STRIPE_L   ((Color){255, 204, 120, 255})
#define C_PANTS      ((Color){ 88,  80,  62, 255})
#define C_PANTS_D    ((Color){ 56,  52,  42, 255})
#define C_BOOT       ((Color){ 58,  44,  34, 255})
#define C_BOOT_L     ((Color){ 96,  76,  58, 255})
#define C_SOLE       ((Color){ 28,  22,  18, 255})
#define C_GLOVE      ((Color){104,  80,  54, 255})
#define C_GLOVE_D    ((Color){ 64,  48,  34, 255})
#define C_SKIN       ((Color){204, 148, 104, 255})
#define C_SKIN_D     ((Color){150, 100,  72, 255})
#define C_HELMET     ((Color){232, 176,  58, 255})
#define C_HELMET_D   ((Color){170, 116,  34, 255})
#define C_HELMET_L   ((Color){255, 226, 140, 255})
#define C_BAND       ((Color){ 40,  34,  30, 255})
#define C_LENS       ((Color){110, 220, 196, 255})
#define C_LENS_L     ((Color){226, 255, 246, 255})
#define C_LAMP       ((Color){255, 244, 196, 255})
#define C_SCARF      ((Color){172,  66,  46, 255})
#define C_SCARF_D    ((Color){112,  40,  30, 255})
#define C_BELT       ((Color){ 92,  62,  38, 255})
#define C_BUCKLE     ((Color){214, 182,  96, 255})
#define C_POUCH      ((Color){116,  88,  56, 255})
#define C_STEEL      ((Color){130, 134, 146, 255})
#define C_STEEL_D    ((Color){ 82,  84,  96, 255})
#define C_STEEL_L    ((Color){184, 188, 198, 255})
#define C_HAZ_Y      ((Color){234, 188,  52, 255})
#define C_HAZ_K      ((Color){ 34,  28,  26, 255})
#define C_BARREL     ((Color){ 58,  60,  68, 255})
#define C_BARREL_L   ((Color){110, 114, 124, 255})
#define C_CORE       ((Color){120, 232, 255, 255})
#define C_RIM        ((Color){255, 178,  98, 255})   /* contre-jour du couchant */
#define C_OUTLINE    ((Color){ 20,  14,  12, 255})
#define WK_RIM_TOP    0.45f     /* force du liseré sur les arêtes hautes   */
#define WK_RIM_SIDE   0.22f     /* … et sur le flanc arrière               */

static const Vector2 LIGHT = {-0.55f, 0.55f};   /* décalage vers la lumière */

typedef struct { Image *im; int ox, oy; } Canvas;   /* origine de la pose */

static Image     g_img, g_cimg;
static Texture2D g_tex, g_ctex;
static int       g_ok;

// ════════════════════════════════════════════════════════════════
// OUTILS
// ════════════════════════════════════════════════════════════════
static Vector2 V(float x, float y)               { return (Vector2){x, y}; }
static Vector2 add(Vector2 a, Vector2 b)         { return (Vector2){a.x + b.x, a.y + b.y}; }
static Vector2 mix2(Vector2 a, Vector2 b, float k) {
    return (Vector2){a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k};
}
static Color sc(Color c, float k) {
    return (Color){(unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), c.a};
}
static Color blend(Color a, Color b, float k) {
    return (Color){(unsigned char)(a.r + (b.r - a.r) * k), (unsigned char)(a.g + (b.g - a.g) * k),
                   (unsigned char)(a.b + (b.b - a.b) * k), a.a};
}

static void put(Canvas *c, int x, int y, Color col) {
    if (x < 0 || y < 0 || x >= c->im->width || y >= c->im->height) return;
    ((Color *)c->im->data)[y * c->im->width + x] = col;
}

static int inside(const Vector2 *p, int n, float x, float y) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if (((p[i].y > y) != (p[j].y > y)) &&
            (x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x))
            in = !in;
    return in;
}

/* Polygone plein, sommets en repère POSE (y vers le haut). */
#define WK_POLY_MAX 12
static void fill_poly(Canvas *c, const Vector2 *pp, int n, Color col) {
    Vector2 q[WK_POLY_MAX];
    if (n > WK_POLY_MAX) n = WK_POLY_MAX;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        q[i] = V((float)c->ox + pp[i].x, (float)c->oy - pp[i].y);
        x0 = fminf(x0, q[i].x); x1 = fmaxf(x1, q[i].x);
        y0 = fminf(y0, q[i].y); y1 = fmaxf(y1, q[i].y);
    }
    for (int y = (int)floorf(y0); y <= (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x <= (int)ceilf(x1); x++)
            if (inside(q, n, x + 0.5f, y + 0.5f)) put(c, x, y, col);
}

static void fill_disc(Canvas *c, Vector2 p, float r, Color col) {
    float cx = (float)c->ox + p.x, cy = (float)c->oy - p.y;
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
        for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) put(c, x, y, col);
        }
}

/* Segment épais a→b (largeurs wa → wb). */
static void quad(Canvas *c, Vector2 a, Vector2 b, float wa, float wb, Color col) {
    float dx = b.x - a.x, dy = b.y - a.y, l = sqrtf(dx * dx + dy * dy);
    if (l < 1e-4f) { fill_disc(c, a, wa * 0.5f, col); return; }
    Vector2 n = {-dy / l, dx / l};
    Vector2 P[4] = {{a.x + n.x * wa * 0.5f, a.y + n.y * wa * 0.5f}, {b.x + n.x * wb * 0.5f, b.y + n.y * wb * 0.5f},
                    {b.x - n.x * wb * 0.5f, b.y - n.y * wb * 0.5f}, {a.x - n.x * wa * 0.5f, a.y - n.y * wa * 0.5f}};
    fill_poly(c, P, 4, col);
}

/* Membre ombré : ton d'ombre, puis ton moyen décalé vers la lumière. */
static void limb(Canvas *c, Vector2 a, Vector2 b, float wa, float wb, Color dark, Color mid) {
    quad(c, a, b, wa, wb, dark);
    fill_disc(c, a, wa * 0.5f, dark); fill_disc(c, b, wb * 0.5f, dark);
    float sa = fmaxf(wa - 1.2f, 0.8f), sb = fmaxf(wb - 1.2f, 0.8f);
    Vector2 a2 = add(a, LIGHT), b2 = add(b, LIGHT);
    quad(c, a2, b2, sa, sb, mid);
    fill_disc(c, a2, sa * 0.5f, mid); fill_disc(c, b2, sb * 0.5f, mid);
}

/* Polygone ombré (même recette que limb). */
static void shape(Canvas *c, const Vector2 *p, int n, Color dark, Color mid) {
    Vector2 q[WK_POLY_MAX];
    if (n > WK_POLY_MAX) n = WK_POLY_MAX;
    fill_poly(c, p, n, dark);
    for (int i = 0; i < n; i++) q[i] = add(p[i], V(LIGHT.x * 0.9f, LIGHT.y * 0.9f));
    fill_poly(c, q, n, mid);
}

/* IK à deux os : coude/genou entre `root` et `target` (atteinte bornée).
   bend +1 : articulation vers l'avant (genou) ; -1 : vers l'arrière (coude). */
static Vector2 ik(Vector2 root, Vector2 target, float a, float b, float bend, Vector2 *end) {
    float dx = target.x - root.x, dy = target.y - root.y, d = sqrtf(dx * dx + dy * dy);
    float dmax = a + b - 0.05f, dmin = fabsf(a - b) + 0.05f;
    float dd = fminf(fmaxf(d, dmin), dmax);
    float phi = (d > 1e-4f) ? atan2f(dy, dx) : -WK_PI * 0.5f;
    *end = V(root.x + cosf(phi) * dd, root.y + sinf(phi) * dd);
    float ca = (a * a + dd * dd - b * b) / (2.0f * a * dd);
    ca = fminf(fmaxf(ca, -1.0f), 1.0f);
    float ang = phi + bend * acosf(ca);
    return V(root.x + cosf(ang) * a, root.y + sinf(ang) * a);
}

// ════════════════════════════════════════════════════════════════
// PEINTURE DU PERSONNAGE
// ════════════════════════════════════════════════════════════════
static void boot(Canvas *c, Vector2 a, float k) {
    Vector2 B[5] = {{a.x - 2.0f, a.y + 1.3f}, {a.x + 1.6f, a.y + 1.3f}, {a.x + 4.6f, a.y - 0.6f},
                    {a.x + 4.6f, a.y - 2.2f}, {a.x - 2.2f, a.y - 2.2f}};
    shape(c, B, 5, sc(C_BOOT, 0.75f * k), sc(C_BOOT, k));
    quad(c, V(a.x - 2.2f, a.y - 1.75f), V(a.x + 4.6f, a.y - 1.75f), 0.9f, 0.9f, sc(C_SOLE, k));
    fill_disc(c, V(a.x + 3.3f, a.y - 0.3f), 0.8f, sc(C_BOOT_L, k));      // bout renforcé
}

static void leg(Canvas *c, Vector2 hip, Vector2 ankle, int far) {
    float   k = far ? WK_FAR : 1.0f;
    Vector2 end, knee = ik(hip, ankle, WK_THIGH, WK_SHIN, 1.0f, &end);
    limb(c, hip, knee, 5.0f, 4.4f, sc(C_PANTS_D, k), sc(C_PANTS, k));
    limb(c, knee, end, 4.2f, 3.6f, sc(C_PANTS_D, k), sc(C_PANTS, k));
    if (!far) fill_disc(c, add(knee, V(0.7f, 0.2f)), 1.3f, C_PANTS_D);   // genouillère
    boot(c, end, k);
}

static void arm(Canvas *c, Vector2 sh, Vector2 hand, int far) {
    float   k = far ? WK_FAR : 1.0f;
    Vector2 end, el = ik(sh, hand, WK_UPPER, WK_FORE, -1.0f, &end);
    limb(c, sh, el, 4.0f, 3.6f, sc(C_JACKET_D, k), sc(C_JACKET, k));
    limb(c, el, end, 3.6f, 3.0f, sc(C_JACKET_D, k), sc(C_JACKET, k));
    quad(c, mix2(sh, el, 0.42f), mix2(sh, el, 0.60f), 3.4f, 3.2f, sc(C_STRIPE, k));  // bande réfléchissante
    fill_disc(c, end, 1.8f, sc(C_GLOVE_D, k));
    fill_disc(c, add(end, V(LIGHT.x * 0.6f, LIGHT.y * 0.6f)), 1.2f, sc(C_GLOVE, k));
}

static void torso(Canvas *c, const WorkerPose *p, Vector2 S) {
    Vector2 d = {sinf(p->torso), cosf(p->torso)};       // vers le haut du buste
    Vector2 n = {d.y, -d.x};                             // vers l'avant
    Vector2 H = p->hip;
    #define TP(B, fn, fd) V((B).x + n.x * (fn) + d.x * (fd), (B).y + n.y * (fn) + d.y * (fd))
    Vector2 J[7] = {TP(H, 3.6f, -1.2f), TP(H, 4.3f, 6.0f), TP(S, 4.5f, -2.2f), TP(S, 2.2f, 1.4f),
                    TP(S, -2.6f, 1.2f), TP(S, -4.7f, -2.6f), TP(H, -4.0f, -1.2f)};
    shape(c, J, 7, C_JACKET_D, C_JACKET);
    quad(c, TP(H, 2.8f, 0.2f), TP(S, 3.2f, -2.4f), 0.7f, 0.7f, C_JACKET_D);   // fermeture
    quad(c, TP(H, -4.3f, 7.4f), TP(H, 4.6f, 7.4f), 1.8f, 1.8f, C_STRIPE);     // bande réfléchissante
    quad(c, TP(H, -4.0f, 7.9f), TP(H, 4.3f, 7.9f), 0.6f, 0.6f, C_STRIPE_L);
    quad(c, TP(H, -4.3f, 0.6f), TP(H, 4.1f, 0.6f), 1.8f, 1.8f, C_BELT);       // ceinture
    fill_disc(c, TP(H, 3.6f, 0.6f), 0.9f, C_BUCKLE);
    Vector2 P[4] = {TP(H, -5.7f, 1.4f), TP(H, -3.0f, 1.4f), TP(H, -3.0f, -2.6f), TP(H, -5.7f, -2.6f)};
    shape(c, P, 4, sc(C_POUCH, 0.7f), C_POUCH);                               // sacoche
    quad(c, TP(H, -5.7f, 0.5f), TP(H, -3.0f, 0.5f), 0.6f, 0.6f, sc(C_POUCH, 0.6f));
    #undef TP
}

static void head(Canvas *c, const WorkerPose *p, Vector2 S, float wind) {
    Vector2 d  = {sinf(p->torso), cosf(p->torso)};
    Vector2 N  = add(S, V(d.x * WK_NECK, d.y * WK_NECK));
    float   ha = p->torso * 0.5f + p->head;
    Vector2 u  = {sinf(ha), cosf(ha)}, f = {u.y, -u.x};
    Vector2 C  = add(N, V(u.x * WK_HEAD_R * 0.95f + f.x * 0.6f, u.y * WK_HEAD_R * 0.95f + f.y * 0.6f));
    #define HP(a, b) V(C.x + f.x * (a) + u.x * (b), C.y + f.y * (a) + u.y * (b))
    /* Pan d'écharpe qui flotte derrière la nuque (sous la tête). */
    float w1 = sinf(wind * 7.0f), w2 = sinf(wind * 7.0f + 1.3f);
    Vector2 T[5] = {HP(-2.0f, -3.0f), HP(-6.0f, -3.0f + w1 * 0.8f), HP(-9.2f, -3.4f + w2 * 1.3f),
                    HP(-8.6f, -5.0f + w2 * 1.0f), HP(-2.5f, -5.4f)};
    shape(c, T, 5, C_SCARF_D, C_SCARF);
    fill_disc(c, add(N, V(u.x * 0.8f, u.y * 0.8f)), 2.0f, C_SKIN_D);       // cou
    fill_disc(c, C, WK_HEAD_R, C_SKIN_D);                                    // visage
    fill_disc(c, add(C, V(LIGHT.x * 0.7f, LIGHT.y * 0.7f)), WK_HEAD_R - 0.6f, C_SKIN);
    fill_disc(c, HP(4.4f, -0.9f), 0.9f, C_SKIN);                             // nez
    fill_disc(c, HP(-1.0f, 0.0f), 0.7f, C_SKIN_D);                          // oreille
    Vector2 M[6] = {HP(4.9f, -1.8f), HP(4.4f, -3.8f), HP(1.5f, -5.4f), HP(-2.6f, -5.4f),
                    HP(-3.4f, -2.4f), HP(0.8f, -1.9f)};
    shape(c, M, 6, C_SCARF_D, C_SCARF);                                      // écharpe-masque
    quad(c, HP(4.2f, -3.0f), HP(-2.8f, -3.8f), 0.7f, 0.7f, C_SCARF_D);
    quad(c, HP(-3.8f, 0.5f), HP(2.6f, -0.1f), 1.1f, 1.1f, C_BAND);          // lunettes
    fill_disc(c, HP(3.4f, -0.2f), 1.6f, C_BAND);
    fill_disc(c, HP(3.6f, -0.1f), 1.1f, C_LENS);
    fill_disc(c, HP(3.2f, 0.4f), 0.5f, C_LENS_L);
    Vector2 Cd = HP(-0.2f, 2.0f), D[11];                                     // dôme du casque
    for (int k = 0; k <= 10; k++) {
        float th = WK_PI * (float)k / 10.0f;
        D[k] = V(Cd.x + f.x * 5.0f * cosf(th) + u.x * 4.6f * sinf(th),
                 Cd.y + f.y * 5.0f * cosf(th) + u.y * 4.6f * sinf(th));
    }
    shape(c, D, 11, C_HELMET_D, C_HELMET);
    Vector2 B[4] = {HP(-5.4f, 2.2f), HP(7.0f, 1.8f), HP(7.0f, 1.0f), HP(-5.4f, 1.2f)};   // bord
    fill_poly(c, B, 4, C_HELMET_D);
    quad(c, HP(-5.2f, 2.0f), HP(6.8f, 1.6f), 0.6f, 0.6f, C_HELMET);
    fill_disc(c, add(Cd, V(f.x * -1.2f + u.x * 3.0f, f.y * -1.2f + u.y * 3.0f)), 1.1f, C_HELMET_L);
    fill_disc(c, HP(4.0f, 3.6f), 1.3f, C_BAND);                              // lampe frontale
    fill_disc(c, HP(4.6f, 3.7f), 0.7f, C_LAMP);
    #undef HP
}

// ════════════════════════════════════════════════════════════════
// CAISSE-TOUR (repère local : rotation propre, sans pixels tournés)
// ════════════════════════════════════════════════════════════════
static Color crate_px(float lx, float ly, float hx, float hy) {
    if (ly > hy && ly < hy + 1.6f && lx > -4.0f && lx < hx + 2.6f)           // canon replié
        return (ly > hy + 0.9f) ? C_BARREL_L : C_BARREL;
    if (fabsf(lx) > hx || fabsf(ly) > hy) return (Color){0, 0, 0, 0};
    if (ly > hy - 1.2f) return C_STEEL_L;                                    // dessus éclairé
    if (lx > hx - 1.2f || ly < -hy + 1.0f) return C_STEEL_D;                 // flanc / bas
    if (fabsf(lx + 3.4f) < 0.9f && fabsf(ly - 2.2f) < 0.9f) return C_CORE;  // voyant
    if (fabsf(ly + 0.6f) < 1.1f)                                             // bande de danger
        return (((int)floorf((lx - ly) * 0.5f + 40.0f)) & 1) ? C_HAZ_Y : C_HAZ_K;
    if (fabsf(fabsf(lx) - (hx - 2.0f)) < 0.5f && fabsf(fabsf(ly) - (hy - 2.0f)) < 0.5f)
        return C_STEEL_D;                                                    // rivets
    return (lx < -hx + 1.0f) ? blend(C_STEEL, C_STEEL_L, 0.5f) : C_STEEL;
}

static void paint_crate(Canvas *c, Vector2 ctr, float rot, float squash) {
    float hx = CR_HX * (1.0f + CR_SQUASH * squash), hy = CR_HY * (1.0f - CR_SQUASH * squash);
    ctr.y -= CR_HY - hy;                                     // la base reste posée
    float cr = cosf(rot), sr = sinf(rot), R = hx + 4.0f;
    float cx = (float)c->ox + ctr.x, cy = (float)c->oy - ctr.y;
    for (int y = (int)floorf(cy - R); y <= (int)ceilf(cy + R); y++)
        for (int x = (int)floorf(cx - R); x <= (int)ceilf(cx + R); x++) {
            float dx = x + 0.5f - cx, dy = cy - (y + 0.5f);   // repère pose (y en haut)
            float lx = dx * cr + dy * sr, ly = -dx * sr + dy * cr;
            Color col = crate_px(lx, ly, hx, hy);
            if (col.a) put(c, x, y, col);
        }
}

// ════════════════════════════════════════════════════════════════
// FINITION : contre-jour + contour
// ════════════════════════════════════════════════════════════════
static void finish(Canvas *c) {
    static unsigned char m[WK_IMG_H][WK_IMG_W];
    int w = c->im->width, h = c->im->height;
    Color *px = (Color *)c->im->data;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) m[y][x] = px[y * w + x].a > 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (!m[y][x]) continue;
            if (y == 0 || !m[y - 1][x])
                px[y * w + x] = blend(px[y * w + x], C_RIM, WK_RIM_TOP);
            else if (x > 0 && !m[y][x - 1])
                px[y * w + x] = blend(px[y * w + x], C_RIM, WK_RIM_SIDE);
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (m[y][x]) continue;
            if ((x > 0 && m[y][x - 1]) || (x < w - 1 && m[y][x + 1]) ||
                (y > 0 && m[y - 1][x]) || (y < h - 1 && m[y + 1][x]))
                px[y * w + x] = C_OUTLINE;
        }
}

// ════════════════════════════════════════════════════════════════
// API
// ════════════════════════════════════════════════════════════════
void worker_art_init(void) {
    if (g_ok) return;
    g_img  = GenImageColor(WK_IMG_W, WK_IMG_H, BLANK);
    g_cimg = GenImageColor(CR_IMG, CR_IMG, BLANK);
    g_tex  = LoadTextureFromImage(g_img);
    g_ctex = LoadTextureFromImage(g_cimg);
    SetTextureFilter(g_tex, TEXTURE_FILTER_POINT);
    SetTextureFilter(g_ctex, TEXTURE_FILTER_POINT);
    g_ok = 1;
}

void worker_art_unload(void) {
    if (!g_ok) return;
    UnloadTexture(g_tex); UnloadTexture(g_ctex);
    UnloadImage(g_img);   UnloadImage(g_cimg);
    g_ok = 0;
}

Vector2 worker_shoulder(const WorkerPose *p) {
    return V(p->hip.x + sinf(p->torso) * WK_TORSO, p->hip.y + cosf(p->torso) * WK_TORSO);
}

float worker_crate_half_h(void) { return CR_HY; }

void worker_art_draw(const WorkerPose *p, float feet_x, float feet_y, float wind) {
    worker_art_init();
    memset(g_img.data, 0, (size_t)WK_IMG_W * WK_IMG_H * sizeof(Color));
    Canvas  c = {&g_img, WK_ORX, WK_ORY};
    Vector2 S = worker_shoulder(p);
    Vector2 d = {sinf(p->torso), cosf(p->torso)}, n = {d.y, -d.x};
    Vector2 sh_n = V(S.x + n.x * 0.6f - d.x, S.y + n.y * 0.6f - d.y);
    Vector2 sh_f = V(S.x - n.x * 0.9f - d.x, S.y - n.y * 0.9f - d.y);
    Vector2 hp_n = V(p->hip.x + n.x * 0.5f, p->hip.y + n.y * 0.5f);
    Vector2 hp_f = V(p->hip.x - n.x * 0.5f, p->hip.y - n.y * 0.5f);

    leg  (&c, hp_f, p->foot_f, 1);                  // de l'arrière vers l'avant
    arm  (&c, sh_f, p->hand_f, 1);
    leg  (&c, hp_n, p->foot_n, 0);
    torso(&c, p, S);
    if (p->carry > 0.5f) paint_crate(&c, p->crate, p->crate_rot, 0.0f);
    head (&c, p, S, wind);
    arm  (&c, sh_n, p->hand_n, 0);
    finish(&c);

    UpdateTexture(g_tex, g_img.data);
    DrawTexturePro(g_tex, (Rectangle){0, 0, (float)WK_IMG_W, (float)WK_IMG_H},
                   (Rectangle){floorf(feet_x - WK_ORX * WK_PX), floorf(feet_y - WK_ORY * WK_PX),
                               WK_IMG_W * WK_PX, WK_IMG_H * WK_PX},
                   (Vector2){0, 0}, 0.0f, WHITE);
}

void worker_art_draw_crate(float cx, float cy, float rot, float squash) {
    worker_art_init();
    memset(g_cimg.data, 0, (size_t)CR_IMG * CR_IMG * sizeof(Color));
    Canvas c = {&g_cimg, CR_IMG / 2, CR_IMG / 2};
    paint_crate(&c, V(0.0f, 0.0f), rot, squash);
    finish(&c);
    UpdateTexture(g_ctex, g_cimg.data);
    DrawTexturePro(g_ctex, (Rectangle){0, 0, (float)CR_IMG, (float)CR_IMG},
                   (Rectangle){floorf(cx - CR_IMG * 0.5f * WK_PX), floorf(cy - CR_IMG * 0.5f * WK_PX),
                               CR_IMG * WK_PX, CR_IMG * WK_PX},
                   (Vector2){0, 0}, 0.0f, WHITE);
}

/* (Bibliothèque de poses : menu_anim_poses.c.) */

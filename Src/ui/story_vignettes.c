/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_vignettes.c ─ Décors pixel-art des chapitres de campagne.
 *
 *  Un paysage par thème, composé de couches (ciel en bandes, astre,
 *  reliefs lointains, éléments du lieu, sol) puis d'une animation
 *  d'ambiance (braises, brume, sable, projecteurs, fumées). Tout est
 *  posé sur une grille de `px` pixels : c'est elle qui donne le grain
 *  pixel-art, quelle que soit la taille du bandeau.
 *
 *  Certains actes ont un LIEU CLÉ (convoi blindé, Reine des marais,
 *  parlement, œil de NEXUS…) : le nœud sélectionne la variante.
 *
 *  Pas de scissor : il opère en coordonnées écran et se désalignerait du
 *  canvas virtuel. Chaque primitive passe par vr(), qui découpe à la
 *  grille du bandeau.
 */
#include "story_art.h"
#include <math.h>

typedef struct {
    int   x, y;          /* coin haut-gauche (px canvas)         */
    int   cols, rows;    /* taille en cellules                   */
    int   px;            /* côté d'une cellule (px)              */
    float t;             /* horloge d'animation (s)              */
} Vg;

// ── Primitives ───────────────────────────────────────────────────
/* Rectangle en cellules, DÉCOUPÉ au bandeau. */
static void vr(const Vg *v, int c, int r, int w, int h, Color col) {
    if (c < 0) { w += c; c = 0; }
    if (r < 0) { h += r; r = 0; }
    if (c + w > v->cols) w = v->cols - c;
    if (r + h > v->rows) h = v->rows - r;
    if (w <= 0 || h <= 0) return;
    DrawRectangle(v->x + c * v->px, v->y + r * v->px, w * v->px, h * v->px, col);
}

/* Hachage entier → [0,1[ : variété déterministe (même décor à chaque frame). */
static float vf(unsigned a) {
    a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16;
    return (float)(a & 0xffffU) / 65536.0f;
}

static Color mix(Color a, Color b, float k) {
    return (Color){
        (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * k),
        (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * k),
        (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * k),
        (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * k) };
}
static Color al(Color c, float a) {
    c.a = (unsigned char)(a < 0.0f ? 0.0f : a > 255.0f ? 255.0f : a);
    return c;
}

#define VG_SKY_BANDS 9   /* bandes du dégradé de ciel (aplats, pas de fondu) */
static void sky(const Vg *v, Color top, Color bot) {
    for (int b = 0; b < VG_SKY_BANDS; b++) {
        int r0 = v->rows * b / VG_SKY_BANDS;
        int r1 = v->rows * (b + 1) / VG_SKY_BANDS;
        vr(v, 0, r0, v->cols, r1 - r0,
           mix(top, bot, (float)b / (float)(VG_SKY_BANDS - 1)));
    }
}

static void disc(const Vg *v, int cc, int cr, int rad, Color col) {
    for (int dy = -rad; dy <= rad; dy++) {
        int half = (int)sqrtf((float)(rad * rad - dy * dy));
        vr(v, cc - half, cr + dy, 2 * half + 1, 1, col);
    }
}

/* Relief : silhouette pleine dont la crête ondule (deux sinus). `base` =
   hauteur moyenne en cellules depuis le bas du bandeau. */
static void ridge(const Vg *v, float base, float amp, float f1, float f2,
                  float ph, Color col) {
    for (int c = 0; c < v->cols; c++) {
        float h = base + amp * sinf((float)c * f1 + ph)
                       + amp * 0.5f * sinf((float)c * f2 + ph * 1.7f);
        int r = v->rows - (int)h;
        vr(v, c, r, 1, v->rows - r, col);
    }
}

// ════════════════════════════════════════════════════════════════
// CH.1 — LES TERRES BRÛLÉES : couchant de braise, arbres morts
// ════════════════════════════════════════════════════════════════
static void armored_truck(const Vg *v, int c, int ground, float t) {
    const Color body = {26, 14, 12, 255}, edge = {70, 34, 22, 255};
    vr(v, c, ground - 6, 16, 5, body);          // caisse blindée
    vr(v, c + 16, ground - 5, 5, 4, body);      // cabine
    vr(v, c + 3, ground - 8, 6, 2, body);       // tourelle
    vr(v, c + 9, ground - 8, 5, 1, body);       // canon
    vr(v, c, ground - 6, 16, 1, edge);          // arête éclairée par le couchant
    for (int w = 0; w < 3; w++) vr(v, c + 2 + w * 7, ground - 1, 3, 2, body);
    /* Il brûle : flammes à 2 temps au-dessus de la caisse. */
    int f = ((int)(t * 7.0f)) & 1;
    vr(v, c + 5, ground - 10 - f, 2, 2 + f, (Color){255, 150, 50, 235});
    vr(v, c + 6, ground - 12 - f, 1, 2, (Color){255, 220, 120, 220});
    vr(v, c + 11, ground - 9, 1, 1 + f, (Color){255, 120, 40, 220});
}

static void vg_wasteland(const Vg *v, int node) {
    const Color far_c = {122, 50, 28, 255}, mid_c = {78, 32, 20, 255};
    const Color tree  = { 22, 12, 10, 255};
    sky(v, (Color){52, 20, 14, 255}, (Color){224, 110, 40, 255});

    int sc = v->cols * 72 / 100, sr = v->rows * 46 / 100;
    disc(v, sc, sr, 11, (Color){255, 150, 60, 70});         // halo
    disc(v, sc, sr, 8,  (Color){255, 208, 112, 255});       // soleil bas
    vr(v, sc - 9, sr + 2, 19, 1, (Color){214, 100, 38, 255});   // voiles de chaleur
    vr(v, sc - 7, sr + 5, 15, 1, (Color){214, 100, 38, 255});

    ridge(v, (float)v->rows * 0.44f, 3.0f, 0.045f, 0.11f, 1.3f, far_c);   // mesas
    ridge(v, (float)v->rows * 0.31f, 2.5f, 0.070f, 0.17f, 4.1f, mid_c);

    int ground = v->rows - 7;
    vr(v, 0, ground, v->cols, 7, (Color){40, 18, 12, 255});
    vr(v, 0, ground, v->cols, 1, (Color){70, 30, 18, 255});

    for (int i = 0; i < 7; i++) {                           // arbres calcinés
        int c = (int)(vf((unsigned)i * 31u + 5u) * (float)v->cols);
        int h = 10 + (int)(vf((unsigned)i * 17u + 3u) * 9.0f);
        vr(v, c, ground - h, 2, h, tree);                   // tronc
        vr(v, c - 3, ground - h * 2 / 3, 3, 1, tree);       // branches
        vr(v, c - 3, ground - h * 2 / 3 - 3, 1, 3, tree);
        vr(v, c + 2, ground - h / 2, 4, 1, tree);
        vr(v, c + 5, ground - h / 2 - 4, 1, 4, tree);
        vr(v, c + 1, ground - h - 2, 1, 2, tree);           // cime brisée
    }

    /* LIEU CLÉ : le convoi blindé (route du nord, convoi, embuscade). */
    if (node == 1 || node == 2 || node == 17)
        armored_truck(v, v->cols * 28 / 100, ground, v->t);

    for (int i = 0; i < 26; i++) {                          // braises
        float sp = 3.0f + vf((unsigned)i * 7u + 1u) * 5.0f;
        float y  = fmodf(v->t * sp + vf((unsigned)i * 13u) * (float)v->rows,
                         (float)v->rows);
        int c = (int)(vf((unsigned)i * 29u + 2u) * (float)v->cols
                      + sinf(v->t * 0.9f + (float)i) * 2.0f);
        vr(v, c, v->rows - 1 - (int)y, 1, 1,
           al((Color){255, 170, 70, 255}, 220.0f * (1.0f - y / (float)v->rows)));
    }
}

// ════════════════════════════════════════════════════════════════
// CH.2 — LE MARAIS TOXIQUE : lune pâle, cyprès, brume, des yeux
// ════════════════════════════════════════════════════════════════
static void glow_eyes(const Vg *v, int c, int r, int size, float phase) {
    /* Clignement lent : fermés une fraction de seconde. */
    if (fmodf(v->t + phase, 3.7f) < 0.18f) return;
    const Color eye = {214, 240, 90, 255};
    vr(v, c, r, size, size > 1 ? size - 1 : 1, eye);
    vr(v, c + size + 2, r, size, size > 1 ? size - 1 : 1, eye);
}

static void vg_swamp(const Vg *v, int node) {
    /* Arbres presque noirs sur un ciel franchement plus clair : à contraste
       trop faible, les cyprès disparaissaient dans la brume. */
    const Color trunk = {6, 16, 14, 255}, leaf = {8, 22, 18, 255};
    sky(v, (Color){10, 30, 30, 255}, (Color){66, 116, 84, 255});

    int mc = v->cols * 20 / 100, mr = v->rows * 28 / 100;
    disc(v, mc, mr, 9, (Color){190, 226, 180, 36});         // halo
    disc(v, mc, mr, 6, (Color){204, 228, 190, 255});        // lune
    vr(v, mc - 3, mr - 2, 2, 2, (Color){170, 200, 160, 255});   // cratères
    vr(v, mc + 1, mr + 1, 2, 1, (Color){170, 200, 160, 255});

    ridge(v, (float)v->rows * 0.46f, 2.0f, 0.06f, 0.15f, 0.7f,
          (Color){30, 68, 54, 255});                        // lisière lointaine

    int water = v->rows - 9;
    for (int i = 0; i < 9; i++) {                           // cyprès
        int c = (int)(vf((unsigned)i * 23u + 9u) * (float)v->cols);
        int h = 18 + (int)(vf((unsigned)i * 11u + 4u) * 12.0f);
        int top = water - h;
        vr(v, c, top + 3, 2, h - 3, trunk);
        vr(v, c - 1, water - 2, 4, 2, trunk);               // contrefort
        vr(v, c - 3, top, 8, 3, leaf);                      // houppier
        vr(v, c - 5, top + 3, 12, 2, leaf);
        for (int m = 0; m < 4; m++) {                       // mousse pendante
            int mc2 = c - 5 + m * 3 + (int)(vf((unsigned)(i * 7 + m)) * 2.0f);
            vr(v, mc2, top + 5, 1, 2 + (int)(vf((unsigned)(i * 13 + m)) * 5.0f), leaf);
        }
    }

    /* LIEUX CLÉS */
    if (node == 4) {                    /* le laboratoire englouti */
        int lc = v->cols * 58 / 100;
        vr(v, lc, water - 9, 22, 9, (Color){30, 44, 44, 255});
        vr(v, lc, water - 9, 22, 1, (Color){56, 76, 72, 255});
        vr(v, lc + 3, water - 11, 6, 2, (Color){30, 44, 44, 255});   // édicule
        float fl = 0.75f + 0.25f * sinf(v->t * 5.0f);
        for (int w = 0; w < 4; w++)                         // fenêtres encore alimentées
            vr(v, lc + 3 + w * 5, water - 6, 2, 2, al((Color){250, 196, 80, 255}, 255.0f * fl));
    }
    if (node == 5 || node == 21) {      /* la Reine des marais veille */
        const Color q = {4, 12, 10, 255};
        int qc = v->cols * 62 / 100, qr = v->rows * 30 / 100;
        disc(v, qc + 4, qr + 12, 13, q);                    // masse
        vr(v, qc - 5, qr + 12, 19, water - qr - 12, q);     // corps jusqu'à l'eau
        for (int k = 0; k < 5; k++) {                       // couronne d'épines
            int kc = qc - 6 + k * 5, kh = (k == 2) ? 8 : (k % 2 ? 6 : 4);
            vr(v, kc, qr + 2 - kh, 2, kh + 2, q);
        }
        /* Deux yeux fixes, larges : c'est ELLE qu'on doit remarquer. */
        vr(v, qc - 1, qr + 8, 4, 2, (Color){214, 240, 90, 255});
        vr(v, qc + 6, qr + 8, 4, 2, (Color){214, 240, 90, 255});
        vr(v, qc, qr + 8, 1, 2, q);                         // pupilles fendues
        vr(v, qc + 7, qr + 8, 1, 2, q);
    }

    vr(v, 0, water, v->cols, 9, (Color){14, 40, 38, 255});  // eau stagnante
    for (int i = 0; i < 4; i++) {                           // reflets
        int r  = water + 1 + i * 2;
        int dx = (int)(sinf(v->t * 0.8f + (float)i * 1.9f) * 3.0f);
        for (int s = 0; s < 7; s++) {
            int c = (int)(vf((unsigned)(i * 19 + s)) * (float)v->cols) + dx;
            vr(v, c, r, 5 + (int)(vf((unsigned)(i * 5 + s)) * 9.0f), 1,
               (Color){40, 84, 74, 255});
        }
    }
    vr(v, mc - 3, water + 2, 7, 1, (Color){150, 190, 150, 120});   // reflet de lune
    vr(v, mc - 2, water + 4, 5, 1, (Color){150, 190, 150, 80});

    for (int i = 0; i < 4; i++)                             // regards dans le noir
        glow_eyes(v, (int)(vf((unsigned)i * 41u + 6u) * (float)(v->cols - 8)),
                  water - 5 - (int)(vf((unsigned)i * 9u) * 10.0f), 1, (float)i * 0.9f);

    for (int b = 0; b < 3; b++) {                           // brume qui dérive
        int r = v->rows * 52 / 100 + b * 5;
        float drift = fmodf(v->t * (2.0f + (float)b), (float)v->cols);
        for (int s = 0; s < 6; s++) {
            int len = 18 + (int)(vf((unsigned)(b * 31 + s)) * 26.0f);
            int c = (int)(vf((unsigned)(b * 17 + s)) * (float)v->cols + drift)
                    % (v->cols + len) - len;
            vr(v, c, r, len, 2, (Color){160, 210, 180, 24});
        }
    }
}

// ════════════════════════════════════════════════════════════════
// CH.3 — LE DÉSERT IRRADIÉ : dunes, pylône du signal, vent de sable
// ════════════════════════════════════════════════════════════════
static void radio_tower(const Vg *v, int c, int ground, int h, float t) {
    const Color st = {70, 46, 26, 255};
    for (int k = 0; k < h; k++) {                           // treillis qui s'affine
        int half = 1 + (h - k) * 4 / h;
        vr(v, c - half, ground - k, 1, 1, st);
        vr(v, c + half, ground - k, 1, 1, st);
        if (k % 4 == 0) vr(v, c - half, ground - k, 2 * half + 1, 1, st);
    }
    vr(v, c, ground - h - 3, 1, 4, st);                     // antenne
    if (fmodf(t, 1.2f) < 0.6f)                              // feu de balisage
        vr(v, c, ground - h - 4, 1, 1, (Color){255, 70, 50, 255});
}

static void vg_desert(const Vg *v, int node) {
    sky(v, (Color){150, 100, 54, 255}, (Color){242, 208, 132, 255});

    int sc = v->cols / 2, sr = v->rows * 24 / 100;
    disc(v, sc, sr, 8, (Color){255, 240, 200, 60});         // halo
    disc(v, sc, sr, 5, (Color){255, 248, 222, 255});        // soleil blanc

    ridge(v, (float)v->rows * 0.40f, 3.0f, 0.030f, 0.085f, 2.2f,
          (Color){208, 160, 90, 255});                      // dunes lointaines
    int ground = v->rows - 5;

    /* LIEUX CLÉS (posés AVANT la dune proche : elle les enterre à demi). */
    if (node == 7) {                    /* les ruines de Vegas */
        static const Color NEON[3] = {{255, 80, 150, 255}, {90, 220, 240, 255},
                                      {255, 220, 90, 255}};
        for (int b = 0; b < 6; b++) {
            int c = v->cols * 6 / 100 + b * 9;
            int h = 10 + (int)(vf((unsigned)b * 37u + 8u) * 14.0f);
            vr(v, c, ground - h, 7, h, (Color){104, 70, 40, 255});
            vr(v, c + 2, ground - h - 2, 3, 2, (Color){104, 70, 40, 255});   // toit éventré
            /* Néons mourants : un tube sur trois tient encore, par à-coups. */
            if (b % 2 == 0 && fmodf(v->t * 1.3f + (float)b, 2.1f) > 0.35f)
                vr(v, c + 1, ground - h + 3, 5, 1, NEON[b % 3]);
        }
    }
    if (node == 8 || node == 18 || node == 22) {            /* le général machine */
        int dc = v->cols * 24 / 100, dr = ground - 19;
        const Color st = {70, 46, 26, 255}, stl = {110, 76, 44, 255};
        vr(v, dc - 1, ground - 13, 3, 13, st);              // mât
        vr(v, dc - 4, ground - 2, 9, 2, st);                // embase
        for (int dy = -7; dy <= 7; dy++) {                  // parabole : demi-disque
            int half = (int)sqrtf((float)(49 - dy * dy));   // ouvert vers la droite
            vr(v, dc - half, dr + dy, half + 1, 1, st);
        }
        for (int dy = -5; dy <= 5; dy++) {                  // creux éclairé
            int half = (int)sqrtf((float)(25 - dy * dy));
            vr(v, dc - half + 1, dr + dy, half, 1, stl);
        }
        vr(v, dc + 1, dr, 7, 1, st);                        // bras de la tête
        if (fmodf(v->t, 0.8f) < 0.4f)                       // elle émet
            vr(v, dc + 8, dr - 1, 2, 3, (Color){255, 70, 50, 255});
        else
            vr(v, dc + 8, dr, 2, 1, st);
    }
    radio_tower(v, v->cols * 78 / 100, ground, 26, v->t);   // la source du signal

    ridge(v, (float)v->rows * 0.22f, 4.0f, 0.050f, 0.13f, 5.0f,
          (Color){172, 124, 66, 255});                      // dunes proches
    vr(v, 0, ground, v->cols, 5, (Color){140, 98, 50, 255});

    for (int i = 0; i < 34; i++) {                          // stries de sable
        int   len = 6 + (int)(vf((unsigned)i + 50u) * 14.0f);
        float sp  = 40.0f + vf((unsigned)i) * 40.0f;
        int   c   = (int)fmodf(v->t * sp + vf((unsigned)i * 3u) * (float)v->cols,
                               (float)(v->cols + len)) - len;
        vr(v, c, (int)(vf((unsigned)i * 7u + 3u) * (float)v->rows), len, 1,
           (Color){252, 228, 172, 36});
    }
}

// ════════════════════════════════════════════════════════════════
// CH.4 — LA VILLE EN RUINE : nuit, immeubles éventrés, projecteurs
// ════════════════════════════════════════════════════════════════
static void searchlight(const Vg *v, int base_c, int base_r, float phase) {
    float ang = sinf(v->t * 0.5f + phase) * 0.55f;          // balayage lent
    for (int s = 1; s < base_r; s++) {
        int c = base_c + (int)((float)s * tanf(ang));
        vr(v, c - s / 9, base_r - s, 1 + s / 4, 1, (Color){214, 226, 255, 20});
    }
}

static void vg_city(const Vg *v, int node) {
    sky(v, (Color){14, 18, 32, 255}, (Color){60, 66, 94, 255});

    for (int i = 0; i < 34; i++) {                          // étoiles
        float tw = 0.5f + 0.5f * sinf(v->t * 1.7f + (float)i * 2.3f);
        vr(v, (int)(vf((unsigned)i * 53u + 1u) * (float)v->cols),
           (int)(vf((unsigned)i * 19u + 7u) * (float)v->rows * 0.5f), 1, 1,
           al((Color){220, 226, 255, 255}, 90.0f + 130.0f * tw));
    }

    int ground = v->rows - 6;
    for (int c = 0, b = 0; c < v->cols; b++) {              // skyline lointaine
        int bw = 4 + (int)(vf((unsigned)b * 13u + 2u) * 6.0f);
        int h  = 12 + (int)(vf((unsigned)b * 29u + 5u) * 15.0f);
        vr(v, c, ground - h, bw, h, (Color){38, 46, 72, 255});
        /* Sommet arraché : une moitié plus basse que l'autre. */
        vr(v, c, ground - h - 2, bw / 2, 2, (Color){38, 46, 72, 255});
        c += bw + 1;
    }
    searchlight(v, v->cols * 30 / 100, ground, 0.0f);
    searchlight(v, v->cols * 70 / 100, ground, 2.4f);

    /* LIEU CLÉ : le parlement en ruine (siège et détour de l'Acier). */
    if (node == 11 || node == 19 || node == 10) {
        const Color pal = {42, 48, 70, 255}, pall = {70, 78, 108, 255};
        int pc = v->cols / 2 - 17;
        vr(v, pc, ground - 9, 34, 9, pal);                  // corps
        vr(v, pc, ground - 9, 34, 1, pall);
        for (int k = 0; k < 7; k++)                         // colonnade
            vr(v, pc + 2 + k * 5, ground - 8, 1, 8, pall);
        for (int dy = 0; dy <= 6; dy++) {                   // coupole
            int half = (int)sqrtf((float)(49 - (6 - dy) * (6 - dy)));
            vr(v, pc + 17 - half, ground - 16 + dy, 2 * half + 1, 1, pal);
        }
        vr(v, pc + 17, ground - 20, 1, 4, pall);            // flèche
        vr(v, pc + 15, ground - 4, 5, 4, (Color){250, 190, 90, 220});   // porche éclairé
    }

    for (int c = 0, b = 0; c < v->cols; b++) {              // immeubles proches
        int bw = 7 + (int)(vf((unsigned)b * 7u + 11u) * 7.0f);
        int h  = 7 + (int)(vf((unsigned)b * 43u + 3u) * 11.0f);
        int centre = (c + bw / 2 > v->cols / 2 - 20 && c + bw / 2 < v->cols / 2 + 20);
        /* Devant le parlement, les immeubles sont rasés : on dégage la vue. */
        if ((node == 11 || node == 19 || node == 10) && centre) h = 2;
        vr(v, c, ground - h, bw, h, (Color){16, 18, 28, 255});
        for (int wy = 2; wy < h - 1; wy += 3)
            for (int wx = 1; wx < bw - 1; wx += 2) {
                unsigned id = (unsigned)(b * 131 + wy * 17 + wx);
                if (vf(id) > 0.10f) continue;               // ~1 fenêtre sur 10 allumée
                /* Elle vacille : éteinte une fraction du temps. */
                if (fmodf(v->t * 0.7f + vf(id + 9u) * 9.0f, 5.0f) < 0.3f) continue;
                vr(v, c + wx, ground - h + wy, 1, 1, (Color){255, 196, 96, 255});
            }
        c += bw + 2;
    }
    vr(v, 0, ground, v->cols, 6, (Color){12, 12, 18, 255});
    for (int i = 0; i < 22; i++)                            // gravats
        vr(v, (int)(vf((unsigned)i * 61u) * (float)v->cols), ground - 1,
           2 + (int)(vf((unsigned)i * 5u) * 3.0f), 1, (Color){24, 26, 36, 255});
}

// ════════════════════════════════════════════════════════════════
// CH.5 — L'USINE ABANDONNÉE : ciel de fournaise, cheminées, NEXUS
// ════════════════════════════════════════════════════════════════
static void vg_factory(const Vg *v, int node) {
    const Color wall = {22, 10, 12, 255}, wall2 = {36, 16, 16, 255};
    sky(v, (Color){18, 8, 10, 255}, (Color){116, 36, 22, 255});
    int ground = v->rows - 5;

    for (int i = 0; i < 9; i++) {                           // ateliers lointains
        int c = i * (v->cols / 9), h = 9 + (int)(vf((unsigned)i * 27u + 4u) * 8.0f);
        vr(v, c, ground - h, v->cols / 9 - 1, h, wall2);
        for (int s = 0; s < 4; s++)                         // toit en dents de scie
            vr(v, c + s * 5, ground - h - 2 + s % 2, 4, 2, wall2);
    }

    int x0 = v->cols * 24 / 100, x1 = v->cols * 80 / 100;   // complexe principal
    vr(v, x0, ground - 18, x1 - x0, 18, wall);
    vr(v, x0, ground - 18, x1 - x0, 1, (Color){70, 26, 20, 255});

    float furnace = 170.0f + 85.0f * sinf(v->t * 2.4f);     // les fours respirent
    for (int c = x0 + 4; c < x1 - 4; c += 7)
        vr(v, c, ground - 8, 3, 3, al((Color){255, 116, 40, 255}, furnace));

    for (int k = 0; k < 3; k++) {                           // cheminées + fumée
        int c = x0 + 10 + k * ((x1 - x0 - 20) / 2);
        vr(v, c, ground - 32, 4, 14, wall);
        vr(v, c - 1, ground - 32, 6, 1, (Color){70, 26, 20, 255});
        for (int p = 0; p < 6; p++) {
            float life = fmodf(v->t * 0.55f + (float)p / 6.0f + (float)k * 0.31f, 1.0f);
            int size = 2 + (int)(life * 5.0f);
            vr(v, c + 1 - size / 2 + (int)(life * 6.0f),
               ground - 33 - (int)(life * 12.0f) - size, size, size,
               al((Color){96, 74, 72, 255}, 150.0f * (1.0f - life)));
        }
    }

    /* LIEUX CLÉS */
    if (node == 14) {                   /* NEXUS : l'œil dans la tour centrale */
        int ec = (x0 + x1) / 2, er = ground - 26;
        vr(v, ec - 9, ground - 36, 19, 18, wall);           // tour
        vr(v, ec - 9, ground - 36, 19, 1, (Color){110, 30, 24, 255});
        float pulse = 0.5f + 0.5f * sinf(v->t * 2.6f);
        disc(v, ec, er, 7, al((Color){255, 60, 44, 255}, 40.0f + 50.0f * pulse));
        disc(v, ec, er, 5, (Color){(unsigned char)(170.0f + 70.0f * pulse), 34, 28, 255});
        disc(v, ec, er, 2, (Color){255, 214, 186, 255});
        for (int k = 1; k <= 8; k++) {                      // pistes vers l'œil
            vr(v, ec - 9 - k * 3, er, 2, 1, al((Color){200, 44, 36, 255}, 200.0f - (float)k * 20.0f));
            vr(v, ec + 9 + k * 3, er, 2, 1, al((Color){200, 44, 36, 255}, 200.0f - (float)k * 20.0f));
        }
    }
    if (node == 16) {                   /* le réacteur s'emballe */
        int rc = v->cols * 86 / 100;
        float p = 0.5f + 0.5f * sinf(v->t * 7.0f);
        disc(v, rc, ground - 6, 10, al((Color){255, 150, 70, 255}, 30.0f + 60.0f * p));
        disc(v, rc, ground - 6, 6, (Color){60, 24, 20, 255});
        disc(v, rc, ground - 6, 3, al((Color){255, 236, 200, 255}, 150.0f + 100.0f * p));
    }

    vr(v, x0 - 6, ground - 3, x1 - x0 + 12, 1, (Color){54, 24, 20, 255});   // convoyeur
    for (int i = 0; i < 12; i++) {                          // pièces en transit
        int span = x1 - x0 + 12;
        int c = x0 - 6 + (int)fmodf(v->t * 9.0f + (float)i * ((float)span / 12.0f), (float)span);
        vr(v, c, ground - 5, 2, 2, (Color){150, 70, 40, 255});
    }
    vr(v, 0, ground, v->cols, 5, (Color){14, 6, 8, 255});
    vr(v, 0, ground, v->cols, 1, (Color){60, 22, 18, 255});
}

// ════════════════════════════════════════════════════════════════
void story_vignette_draw(ThemeID theme, int node_id,
                         int x, int y, int w, int h, int px, float t) {
    if (px <= 0 || w < px || h < px) return;
    Vg v = { x, y, w / px, h / px, px, t };
    switch (theme) {
        case THEME_SWAMP:   vg_swamp    (&v, node_id); break;
        case THEME_DESERT:  vg_desert   (&v, node_id); break;
        case THEME_CITY:    vg_city     (&v, node_id); break;
        case THEME_FACTORY: vg_factory  (&v, node_id); break;
        case THEME_WASTELAND:
        default:            vg_wasteland(&v, node_id); break;
    }
}

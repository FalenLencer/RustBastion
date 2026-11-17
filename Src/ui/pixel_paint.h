/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  pixel_paint.h ─ Peinture de SPRITES en pixel art, façon splash art.
 *
 *  Source unique des primitives des sprites peints (ruines du terrain,
 *  emblèmes de perks) : volumes ombrés sur rampe à 6 tons (lumière en haut
 *  à gauche), sphères, polygones, traits, puis contour sombre d'1 px et
 *  halo coloré. On peint dans une zone (ox, oy, w, h) d'une Image RGBA ;
 *  tout ce qui sort de la zone est ignoré (atlas sans débordement).
 */
#include "raylib.h"

typedef struct {
    Image   *im;
    int      ox, oy, w, h;   // zone de la case dans l'atlas
    unsigned s;              // graine du bruit de matière
} PxCanvas;

void  pp_px     (PxCanvas *c, int x, int y, Color col);
Color pp_get    (const PxCanvas *c, int x, int y);
int   pp_solid  (const PxCanvas *c, int x, int y);
/* Volume parallélépipédique (arête haute et flanc gauche éclairés). */
void  pp_box    (PxCanvas *c, int x, int y, int w, int h, Color base, float b);
/* Sphère/disque éclairé en haut à gauche. */
void  pp_disc   (PxCanvas *c, float cx, float cy, float rad, Color base, float b);
/* Polygone plein (convexe ou non), ombré en diagonale haut-gauche → bas-droite. */
void  pp_poly   (PxCanvas *c, const Vector2 *pts, int n, Color base, float b);
/* Trait d'1 px (Bresenham). */
void  pp_line   (PxCanvas *c, int x0, int y0, int x1, int y1, Color col);
/* Contour d'1 px autour de tout pixel opaque (à faire APRÈS le dessin). */
void  pp_outline(PxCanvas *c, Color col);
/* Halo dégradé de `radius` px autour de la silhouette (après le contour). */
void  pp_glow   (PxCanvas *c, Color col, int radius);
/* Étincelle à 4 branches (accent des objets précieux). */
void  pp_sparkle(PxCanvas *c, int x, int y, int len, Color col);

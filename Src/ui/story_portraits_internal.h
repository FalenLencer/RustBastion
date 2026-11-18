/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  story_portraits_internal.h ─ Interne aux portraits du récit :
 *  story_portraits.c (atlas, fond, liseré, animations) ↔
 *  story_portraits_paint.c (peinture des bustes).
 */
#include "raylib.h"
#include "story_art.h"

#define SP_ART    40     /* côté d'un portrait (px d'art)                  */
#define SP_BLINK  1      /* variante : yeux fermés                         */
#define SP_TALK   2      /* variante : bouche ouverte / grille allumée     */
#define SP_FRAMES 4      /* neutre, clignement, parole, parole+clignement */

/* Peint le personnage SANS fond (pixels transparents autour) dans `layer`
   (SP_ART × SP_ART, RGBA). Lumière principale en haut à gauche, buste de
   trois-quarts tourné vers la droite (vers le texte). */
void sp_paint_figure(Image *layer, SpeakerId id, int flags);

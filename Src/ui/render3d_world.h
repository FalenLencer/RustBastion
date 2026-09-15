/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/render3d_world.h — MODE HÉROS : scène 3D complète.

   Rend la partie ENTIÈRE en vraie 3D (une seule passe, pas de
   RenderTexture par instance) : terrain extrudé depuis la carte
   générée, bases, portails, minerais, tours/unités/ennemis via les
   modèles GLB déjà chargés (repli formes colorées si absents),
   projectiles, héros, fantôme de placement.

   CORRESPONDANCE UNIQUE sim → monde (source de vérité du mode) :
   (px, py) pixels carte  →  (px·S, hauteur, py·S), S = W3D_TILE/TILE_SIZE.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../map/map_gen.h"   /* TILE_SIZE */

#define W3D_TILE    2.0f                              /* unités monde / tuile */
#define W3D_PER_PX  (W3D_TILE / (float)TILE_SIZE)     /* px sim → monde       */

static inline Vector3 w3d_from_sim(float px, float py, float h) {
    return (Vector3){ px * W3D_PER_PX, h, py * W3D_PER_PX };
}

/* Hauteur (monde) du centre de masse des ennemis : partagée entre le
   rendu, le tir du héros et les traceurs. */
#define W3D_ENEMY_BODY_H  0.62f

/* Hauteur (monde) des blocs de ruine — partagée rendu / collision héros
   (le saut permet de grimper dessus / les franchir). */
#define W3D_RUIN_H        0.85f

struct AppContext;

/* Teinte décalée par canal — délègue à la source unique ui_tint3
   (ui_utils.h) ; noms w3d_* conservés pour les ~40 sites d'appel 3D. */
#include "ui_utils.h"
static inline Color w3d_tint3(Color c, int dr, int dg, int db) {
    return ui_tint3(c, dr, dg, db);
}
static inline Color w3d_shade(Color c, int d) { return ui_shade(c, d); }

/* 1 = position monde devant la caméra (marge dos-caméra incluse) :
   test de cull partagé par toute la scène (terrain, props, entités). */
int w3d_in_front(Camera3D cam, Vector3 p);

/* ── Brouillard CPU (formes au shader raylib par défaut : terrain, props,
   bases, portails). Les modèles GLB, eux, ont le fog DANS leurs shaders.
   setup à chaque frame (render3d_world_render) ; factor = 0..1 selon la
   distance caméra ; mix = mélange vers la couleur d'horizon.            */
void  w3d_fog_setup(Color col, float density);
float w3d_fog_factor(Vector3 p, Camera3D cam);
Color w3d_fog_mix(Color c, float f);

/* Couleur des projectiles par type de dégâts (indices DamageType,
   définition dans render3d_world.c) — réutilisée par les FX 3D. */
extern const Color W3D_PROJ_COL[];

/* Dessine la scène 3D complète (ClearBackground inclus) + les éléments
   monde du mode héros (traceur, fantôme de placement). */
void render3d_world_render(struct AppContext *ctx, Camera3D cam);

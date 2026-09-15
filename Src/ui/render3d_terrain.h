/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/render3d_terrain.h — MODE HÉROS : sol procédural habillé.

   Aucune texture : tuiles teintées par la palette du thème + grain
   déterministe (hash par tuile), joints sombres entre tuiles, houle
   animée sur l'eau, routes usées (bande centrale + traces), props
   décoratifs (cailloux / touffes / débris) et minerais.
   Extrait de render3d_world.c (règle < 500 lignes).
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../map/map_gen.h"

struct TowerPool;

/* Dessine le sol complet (tuiles, ruines, routes, eau, props, minerais).
   À appeler dans BeginMode3D ; cull dos-caméra via w3d_in_front. */
void render3d_terrain_draw(const Map *map, const struct TowerPool *tp,
                           Camera3D cam);

/* Ombre portée « blob » : disque sombre semi-transparent au sol qui
   ancre un modèle (unité/ennemi/héros/projectile). height_above estompe
   l'ombre (saut, projectile en vol) ; la brume l'estompe aussi. */
#define W3D_SHADOW_Y       0.026f /* hauteur du disque au-dessus du sol   */
#define W3D_SHADOW_R_MULT  1.15f  /* rayon = taille sim (monde) × facteur */
#define W3D_SHADOW_R_MIN   0.24f  /* rayon plancher (petites unités)      */
#define W3D_SHADOW_HERO_R  0.34f  /* rayon de l'ombre du héros            */
#define W3D_SHADOW_PROJ_R  0.10f  /* rayon des ombres de projectiles      */
void w3d_blob_shadow(Camera3D cam, float x, float y, float z,
                     float r, float height_above);

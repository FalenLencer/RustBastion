/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/render3d_enemies.h — rendu 3D des ennemis (jumeau de render3d_units).

   Même principe que les unités : pré-passe qui rend chaque ennemi
   3D-capable (modèle GLB skinné par EnemyType) dans une RenderTexture,
   puis renderer.c blitte cette texture à la place du sprite quand le
   mode de vue 3D est actif (toggle PARTAGÉ `g_units_3d`, touche F4).

   Orientation : l'ennemi regarde sa DIRECTION DE DÉPLACEMENT (atan2 du
   delta de position, lissé), via le helper caméra-oblique de render3d.h.
   Dégradé gracieux : modèle absent → repli sur le sprite.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../combat/enemy.h"

void      render3d_enemies_init(void);
void      render3d_enemies_shutdown(void);
int       render3d_enemies_available(void);          /* 1 = au moins un modèle chargé */
int       render3d_enemy_has_model(int enemy_type);  /* type a-t-il un modèle 3D ?     */

/* Pré-passe : rend chaque ennemi actif (avec modèle) dans sa texture. */
void      render3d_enemies_prepass(const EnemyPool *ep);

/* Brouillard de distance (MODE HÉROS) sur le shader des ennemis —
   density 0 = coupé (remis à 0 par la pré-passe 2D à chaque frame). */
void      render3d_enemies_set_fog(Color col, float density);

/* Texture 3D prête pour l'ennemi d'indice i (.id==0 si aucune → repli sprite).
   Verticalement retournée (convention RenderTexture raylib). */
Texture2D render3d_enemy_tex(int enemy_index);

/* Rectangle de destination pour blitter la texture 3D de l'ennemi i
   (centre x,y ; rayon size px). Cadrage propre à chaque type. */
Rectangle render3d_enemy_dst(int enemy_index, float x, float y, float size);

/* MODE HÉROS : dessine l'ennemi `type` DIRECTEMENT dans la scène 3D courante
   (BeginMode3D actif). heading_rad = cap monde (atan2(x, z)) ; anim_kind :
   0=repos 1=marche 2=attaque. update_anim : 0 = pose précédente (throttle
   perf du skinning CPU). Retourne 1 si un modèle existait.                 */
int render3d_enemies_draw_world(int type, Vector3 pos, float heading_rad,
                                float scale, int anim_kind, float anim_time,
                                int update_anim);

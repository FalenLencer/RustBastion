/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/render3d.h — rendu 3D temps réel des tours (jeu 2D, tours en 3D).

   Le jeu dessine tout dans UN canvas ; raylib n'autorise pas les
   RenderTexture imbriqués. On rend donc chaque tour 3D articulée en
   texture lors d'une PRÉ-PASSE (render3d_prepass, AVANT la passe canvas),
   puis le renderer blitte ces textures à la place des sprites.

   La tourelle VISE l'ennemi (azimut depuis tw->angle + élévation vers la
   cible) et joue l'animation de TIR (recul + flash de bouche, canon par
   canon) quand la tour tire.

   Dégradé gracieux : modèles GLB absents → repli sur les sprites.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "../combat/tower.h"
#include "../combat/enemy.h"   /* EnemyPool (pour l'élévation vers la cible) */
#include <math.h>

/* ── VISÉE 3D correcte sous caméra oblique ────────────────────────────
   Problème : le jeu rend chaque tour/unité 3D dans une RT (caméra 3/4 fixe)
   puis blitte le sprite sur la carte 2D. Un simple `yaw = -angle` ne fait
   APPARAÎTRE le modèle orienté vers la cible QUE pour une vue plongée droite.
   En 3/4, l'axe de profondeur est écrasé → l'orientation paraît fausse.

   Ce helper rend le yaw (degrés, rotation autour de +Y) à appliquer pour que
   l'AVANT du modèle — au repos il pointe la direction sol (sin·rest_phi,0,
   cos·rest_phi) en repère raylib — APPARAISSE, une fois projeté par `cam` et
   blitté (RT flip → haut-caméra = haut-carte), orienté vers `map_angle`
   (rad, atan2(dy,dx), repère écran y-bas). Une erreur de rest_phi se corrige
   par un offset constant ajouté au retour (vrai pour TOUT angle). */
static inline float render3d_yaw_for_aim(Camera3D cam, float map_angle, float rest_phi) {
    float fx = cam.target.x - cam.position.x;
    float fy = cam.target.y - cam.position.y;
    float fz = cam.target.z - cam.position.z;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz); if (fl < 1e-6f) fl = 1e-6f;
    fx /= fl; fy /= fl; fz /= fl;
    float rx = -fz, rz = fx;                    /* droite = fwd × up(0,1,0)   */
    float rl = sqrtf(rx*rx + rz*rz); if (rl < 1e-6f) rl = 1e-6f;
    rx /= rl; rz /= rl;
    float ux = -rz*fy, uz = rx*fy;              /* up_cam = droite × fwd (x,z) */
    float ct = cosf(map_angle), st = sinf(map_angle);
    float det = rx*uz - rz*ux;
    float sgn = (det >= 0.0f) ? 1.0f : -1.0f;
    float vs  = ( uz*ct + rz*st) * sgn;         /* ∝ sin(a)                    */
    float vc  = (-ux*ct - rx*st) * sgn;         /* ∝ cos(a)                    */
    float a   = atan2f(vs, vc);                 /* angle sol requis (rad)      */
    return (a - rest_phi) * RAD2DEG;
}

void      render3d_init(void);
void      render3d_shutdown(void);
int       render3d_available(void);   /* 1 = modèles chargés, 3D active */

/* Pré-passe : rend chaque tour active (avec modèle) dans sa texture, visée +
   animation de tir. À appeler hors passe canvas. */
void      render3d_prepass(const TowerPool *tp, const EnemyPool *ep);

/* Texture 3D prête pour la tour d'indice i (.id==0 si aucune → repli sprite).
   Verticalement retournée (convention RenderTexture raylib). */
Texture2D render3d_tower_tex(int tower_index);

/* Rectangle de destination pour blitter la texture 3D de la tour i (centre
   case cx,cy ; tile = TILE_SIZE). Carré pour la Mitrailleuse ; HAUT et ancré
   par le bas pour le Sniper (le sprite dépasse au-dessus de la case). */
Rectangle render3d_tower_dst(int tower_index, float cx, float cy, float tile);

/* MODE HÉROS : dessine la tour `type` (TowerType) DIRECTEMENT dans la scène
   3D courante (BeginMode3D actif), tourelle orientée selon map_angle
   (= tw->angle, repère écran). Retourne 1 si modèle dispo, 0 sinon.       */
int render3d_tower_draw_world(int type, Vector3 pos, float map_angle,
                              float scale);

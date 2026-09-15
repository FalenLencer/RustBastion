/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
/* ════════════════════════════════════════════════════════════════
   ui/render3d_units.h — rendu 3D des UNITÉS (et, à terme, ennemis).

   Module SÉPARÉ de render3d.c (qui gère les tours). Permet un MODE DE
   VUE alternatif : au lieu du splash art 2D, l'unité est rendue via un
   modèle 3D animé (GLB skinné) pré-blitté dans une RenderTexture par
   instance, puis dessiné à la place du sprite (cf. g_units_3d).

   Modèles PROPRES dans assets/3d/3D_Troupes/ (maillage skinné + vertex-
   colors). UNIT_HEAVY = iron_juggernaut.glb, UNIT_DOG = spiked-hound.glb.
   Les autres types (soldat/médic/ouvrier) retombent sur le sprite 2D ;
   leurs anciennes versions sont archivées dans assets/3d/test/.

   Un modèle peut être SANS animation (rendu statique en pose de repos)
   ou animé (USTATE_ATTACK → Attack ; en mouvement → Walk ; sinon Idle).
   Orientation (yaw) déduite du déplacement/cible ; rest_phi PAR TYPE.
   ════════════════════════════════════════════════════════════════ */
#pragma once
#include "raylib.h"
#include "../combat/unit.h"
#include "../combat/enemy.h"

void      render3d_units_init(void);
void      render3d_units_shutdown(void);
int       render3d_units_available(void);      /* 1 si au moins un modèle chargé   */
int       render3d_unit_has_model(int unit_type);

/* Pré-passe : rend chaque unité 3D-capable dans sa RenderTexture (anim
   à jour + orientation vers cible/déplacement). À appeler AVANT le canvas,
   comme render3d_prepass. `ep` sert à orienter l'unité vers sa cible.     */
void      render3d_units_prepass(const UnitPool *up, const EnemyPool *ep);

/* Brouillard de distance (MODE HÉROS) sur le shader des unités —
   density 0 = coupé (remis à 0 par la pré-passe 2D à chaque frame). */
void      render3d_units_set_fog(Color col, float density);

/* Texture de l'unité i (sprite 3D pré-rendu), .id==0 si non dispo.     */
Texture2D render3d_unit_tex(int unit_index);
/* Rectangle de blit (centré sur cx,cy=position monde, ancré par le bas).*/
Rectangle render3d_unit_dst(int unit_index, float cx, float cy, float size);

/* MODE HÉROS : dessine l'unité `type` DIRECTEMENT dans la scène 3D courante
   (BeginMode3D actif) — anim + gain + orientation gérés ici. heading_rad =
   cap monde (atan2(x, z)) ; anim_kind : 0=repos 1=marche 2=attaque.
   update_anim : 0 = garde la pose précédente (throttle perf, le skinning
   CPU est coûteux). Retourne 1 si un modèle existait (0 → repli).        */
int render3d_units_draw_world(int type, Vector3 pos, float heading_rad,
                              float scale, int anim_kind, float anim_time,
                              int update_anim);

/* MODE HÉROS : modèle DÉDIÉ du héros (assets/3d/3D_Troupes/hero.glb,
   manteau + casque + fusil, anims Idle/Run/Shoot). Même contrat que
   render3d_units_draw_world ; 0 si le GLB manque (repli appelant). */
int render3d_hero_draw_world(Vector3 pos, float heading_rad, float scale,
                             int anim_kind, float anim_time,
                             int update_anim);

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

/* Texture de l'unité i (sprite 3D pré-rendu), .id==0 si non dispo.     */
Texture2D render3d_unit_tex(int unit_index);
/* Rectangle de blit (centré sur cx,cy=position monde, ancré par le bas).*/
Rectangle render3d_unit_dst(int unit_index, float cx, float cy, float size);

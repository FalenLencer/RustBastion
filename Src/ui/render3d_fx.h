/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   ui/render3d_fx.h — MODE HÉROS : vrais FX 3D (plus que des popups).

   Particules d'impact typées, fragments de désintégration à la mort
   d'un ennemi, traînées de projectiles différenciées par type de
   dégâts. Pool STATIQUE (aucune alloc par frame).

   Émission : branchée aux MÊMES endroits que fx_burst via les hooks
   de fx.h (g_fx3d_burst / g_fx3d_impact / g_fx3d_update) — le combat
   émet sans dépendre de ui/. Les hooks restent installés hors mode
   héros : le pool s'y vide tout seul (tick via fx_update) et n'est
   simplement jamais rendu. Toggle fx_effects (g_fx.enabled) respecté.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"

/* Branche les hooks de fx.h et vide le pool (appelé à hero_start,
   idempotent). */
void r3dfx_install(void);

/* Gerbe d'impact typée (couleur W3D_PROJ_COL, style selon le type :
   braises feu, étincelles élec…). Appelée par le tir du héros ET par
   les projectiles de tour (via le hook g_fx3d_impact). */
void r3dfx_impact(float x, float y, int dmg_type);

/* Traînée d'un projectile en vol : 2-3 fantômes derrière (feu = flamme
   montante, élec = arc brisé qui grésille, sinon sphères estompées).
   dxn/dyn = direction de vol normalisée (repère sim). */
void r3dfx_proj_trail(Camera3D cam, Vector3 pos, float dxn, float dyn,
                      int dmg_type);

/* Dessine les particules du pool (dans BeginMode3D, monde héros). */
void r3dfx_render(Camera3D cam);

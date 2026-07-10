/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   combat/move.h — SOURCE UNIQUE de collision-déplacement des entités.

   Empêche unités alliées et ennemis hors-chemin de pénétrer les
   obstacles : eau, blocs de ruine, bâtiments de base (alliés),
   tuiles occupées par une tour, bords de carte. Les ennemis qui
   SUIVENT leur chemin A* ne passent pas par ici (chemin déjà valide).

   Utilisé par unit.c (tous déplacements) et enemy.c (Hunter en
   chasse, Raider en raid, Pathbreaker en ligne droite). Le héros
   (game/hero.c) garde sa collision propre (hauteur/saut).
   ════════════════════════════════════════════════════════════════ */
#include "../map/map_gen.h"
#include "tower.h"

/* Gabarit de collision */
#define MOVE_F_ALLY   0x1  /* allié : la tuile de BASE (bâtiment) bloque  */
#define MOVE_F_ENEMY  0x2  /* ennemi : peut entrer sur la tuile de BASE
                              (c'est sa cible d'attaque)                  */

/* Angles de contournement local essayés quand la direction directe est
   entièrement bloquée (ordre : plus proche du but d'abord). */
#define MOVE_DEVIATE_1  0.7853982f   /* ±45°  */
#define MOVE_DEVIATE_2  1.5707963f   /* ±90°  */

/* La tuile (tx,ty) bloque-t-elle une entité de ce gabarit ? */
int  move_tile_blocked(const Map *map, const TowerPool *tp,
                       int tx, int ty, int flags);

/* Déplacement (dx,dy) avec GLISSEMENT axe par axe le long des murs,
   règle anti-blocage (une entité DANS une tuile bloquée peut toujours
   en sortir) et bornes de carte. radius = demi-largeur de l'entité (px).
   Retourne 1 si la position a réellement changé. */
int  move_slide(const Map *map, const TowerPool *tp,
                float *px, float *py, float dx, float dy,
                float radius, int flags);

/* Avance de `step` px vers (tx,ty) : glissement, puis si AUCUN mouvement
   possible, tente les directions déviées ±45° puis ±90° vers le but
   (contournement local des obstacles). Retourne 1 si on a bougé. */
int  move_toward(const Map *map, const TowerPool *tp,
                 float *px, float *py, float tx, float ty,
                 float step, float radius, int flags);

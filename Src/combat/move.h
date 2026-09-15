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

/* ── NAVIGATION ──────────────────────────────────────────────────
   Principe : « ligne droite quand c'est possible, VRAI chemin sinon ».

   1. Si la ligne directe vers le but est dégagée → on fonce (cas de
      loin le plus fréquent, aucun calcul de chemin).
   2. Sinon on suit un chemin BFS, et on le suit JUSQU'AU BOUT : on ne
      repasse en direct que lorsque la ligne vers le but se dégage.
   3. Le waypoint est « tiré à la corde » (string pulling) : on vise le
      point le PLUS LOIN du chemin encore visible en ligne droite, ce
      qui donne des trajectoires longues et naturelles au lieu d'un
      cheminement case par case.

   C'est le point 2 qui corrige le bug de va-et-vient : l'ancienne
   version ne demandait au BFS qu'UN pas, puis retombait aussitôt en
   « tout droit vers le but » — l'entité se remettait dans l'obstacle,
   dérivait à ±90° (« elle recule »), attendait 36 frames et
   recommençait indéfiniment devant tout obstacle concave.            */
#define MOVE_NAV_STUCK_FRAMES 20    /* filet de sécurité : replanif.   */
#define MOVE_NAV_ARRIVE_FRAC  0.35f /* seuil d'arrivée au waypoint     */
#define MOVE_NAV_LOS_STEP     0.25f /* échantillonnage de la ligne (tuiles) */
#define MOVE_NAV_PULL_MAX     24    /* portée max du tir à la corde
                                       (borne le coût d'une replanif.) */

typedef struct {
    float wx, wy;      /* waypoint courant (px monde sim)               */
    int   active;      /* 1 = waypoint en cours de suivi                */
    float gx, gy;      /* dernier but connu (détection de changement)   */
    float best_d;      /* meilleure distance au but atteinte (progrès)  */
    int   stuck;       /* frames consécutives sans progrès              */
} MoveNav;             /* TAILLE INCHANGÉE — Unit est sérialisée :
                          l'agrandir remettrait à zéro toute la section
                          UnitPool des sauvegardes existantes.          */

/* À appeler UNE FOIS par frame AVANT tout déplacement (unit_pool_update).
   Invalide la grille de navigation partagée : elle est alors reconstruite
   au premier appel de move_nav_toward et réutilisée par toutes les
   entités de la frame (une seule construction pour tout le monde). */
void move_nav_begin_frame(void);

/* 1 si le segment (x0,y0)→(x1,y1) est franchissable par une entité de
   rayon `radius` (aucune tuile bloquée sur le passage). */
int  move_line_clear(const Map *map, const TowerPool *tp,
                     float x0, float y0, float x1, float y1,
                     float radius, int flags);

/* Déplacement navigué complet. `nav` = état persistant de l'entité. */
int  move_nav_toward(const Map *map, const TowerPool *tp, MoveNav *nav,
                     float *px, float *py, float tx, float ty,
                     float step, float radius, int flags);

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  unit_guard.h ─ Postes de garde des unités de combat en PATROUILLE.
 *
 *  Une garnison ne tourne plus en orbite autour de sa base (la moitié du
 *  temps du côté opposé à la menace, et repoussée par le décor : la « garde
 *  dans le vide ») : chaque unité tient une ROUTE D'APPROCHE de sa base,
 *  c.-à-d. un chemin ennemi qui y mène, à UNIT_GUARD_STEPS cases en amont.
 *  La mêlée tient le chemin (c'est elle qui arrête les ennemis), le tir et
 *  le médic se placent en retrait ; les unités d'une même base se
 *  répartissent entre les routes. Leur VISION part de ce poste.
 */
#include "unit.h"

/* Poste : cases de chemin EN AMONT de la base. Réglé au banc (8 actes de
   campagne, 7 unités) : 5 bat l'ancienne orbite sur toutes les mesures
   (fuites 83 → 68, ennemis tués 340 → 341, survivants 13 → 18 sur 4 vagues) ;
   plus près (2-4) la garnison subit l'assaut au pied de la base, plus loin
   (≥ 6) elle est isolée et les ennemis la contournent. */
#define UNIT_GUARD_STEPS    5
#define UNIT_GUARD_BACK     1.0f   /* tir / médic : cases en retrait de la mêlée       */
#define UNIT_GUARD_SPREAD   0.7f   /* écart latéral entre deux gardes (cases)          */
#define UNIT_GUARD_SWAY     0.25f  /* va-et-vient de sentinelle au repos (cases)       */
#define UNIT_GUARD_MERGE    2.0f   /* routes dont les postes sont plus proches que ça
                                      (cases) : un seul poste, garnison concentrée     */
#define UNIT_GUARD_MAX_APPROACH 8  /* routes distinctes retenues par base              */

/* Poste de garde de l'unité `idx` (en patrouille) en px monde. Retourne 0
   si sa base n'a aucune route d'approche (repli : orbite classique). Le
   point n'est PAS validé contre le décor (move_free_spot chez l'appelant). */
int unit_guard_post(const UnitPool *up, int idx, const Map *map, const PathSet *paths,
                    float *px, float *py);

/* Vision : ennemi à cibler (-1 = aucun) parmi ceux à `intercept_range` du
   point gardé (base, tour, poste) ou tout près de l'unité. `guard_post`
   (garde sur une route d'approche) : la vision part du POSTE, mais une
   menace qui approche de la BASE par un autre côté reste vue, et la
   priorité va à la menace la plus proche de la base. */
int unit_find_enemy_target(const Unit *u, const EnemyPool *ep,
                           float base_px, float base_py, int guard_post);

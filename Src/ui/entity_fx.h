/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  entity_fx.h ─ Vie visuelle des unités et des ennemis (entity_fx.c).
 *
 *  Pendant des tours (renderer_tower_fx.c) pour les entités mobiles :
 *    • APPARITION : une unité déployée tombe et s'écrase au sol (poussière) ;
 *      un ennemi surgit du sol (croît en fondu, fumée sombre).
 *    • ATTAQUE    : détectée sur le front de son minuteur (atk_timer /
 *      arty_timer remonte) → fente vers la cible au corps à corps, TIR
 *      visible (traçante, obus en cloche) à distance, étincelles d'impact.
 *  État purement visuel (rien de sauvegardé, aucun tirage aléatoire du jeu).
 *  Valable en vue 2D comme en vue 3D (F4).
 */
#include "raylib.h"
#include "../combat/unit.h"
#include "../combat/enemy.h"
#include "../combat/tower.h"
#include "../map/map_gen.h"

/* Transformation du corps d'une entité (à appliquer autour de son pied). */
typedef struct {
    float dy;          /* décalage vertical (chute)                 */
    float sx, sy;      /* échelles autour du pied (écrasement, pop)  */
    float alpha;       /* opacité (apparition)                       */
    float ox, oy;      /* fente d'attaque vers la cible (px)         */
} EntBody;

/* Cible d'attaque d'un ennemi (unité engagée, tour visée par l'artillerie,
   ouvrier traqué) : 1 + position si elle existe. Lecture seule des pools. */
int  ent_enemy_target(const Enemy *e, const UnitPool *up, const TowerPool *tp,
                      float *tx, float *ty);

/* 1×/frame AVANT render_units / render_enemies : apparitions + attaques. */
void ent_fx_update(const UnitPool *up, const EnemyPool *ep,
                   const TowerPool *tp, const Map *map);
void ent_fx_unit_body (int i, EntBody *b);
void ent_fx_enemy_body(int i, EntBody *b);
/* Poussière / fumée d'apparition, au sol (avant les corps). */
void ent_fx_draw_ground(const UnitPool *up, const EnemyPool *ep);
/* Tirs des unités et des ennemis (après les projectiles des tours). */
void render_combat_fx(void);

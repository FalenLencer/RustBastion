/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hazard.h ─ Hasards de terrain : tronçons de route piégés par le décor.
 *
 *  Chaque thème a SON hasard (braises, mare toxique, sables mouvants, câbles
 *  à nu, métal en fusion), posé sur quelques tronçons de chemin. Il frappe
 *  tout ce qui le traverse — ennemis ET unités du joueur — via les mêmes
 *  mécaniques que les tours (résistances, brûlure, poison, ralentissement).
 *
 *  AUCUN état sauvegardé : la couche est recalculée à l'identique depuis la
 *  graine de la carte (générateur PROPRE, ne consomme pas GetRandomValue) →
 *  zéro impact sur les sauvegardes et sur le déroulé aléatoire du jeu.
 */
#pragma once
#include "raylib.h"
#include "../map/map_gen.h"

typedef enum {
    HAZ_NONE = 0,
    HAZ_EMBERS,      // Terres brûlées : braises — brûlure (feu)
    HAZ_TOXIC,       // Marais : mare toxique — poison + corrosion
    HAZ_QUICKSAND,   // Désert : sables mouvants — ralentit (synergie cryo)
    HAZ_WIRES,       // Ville : câbles à nu — décharges électriques
    HAZ_MOLTEN,      // Usine : métal en fusion — brûlure forte
    HAZ_TYPE_COUNT
} HazardType;

typedef struct {
    const char *name;     // « Mare toxique »
    const char *effect;   // effet court, pour l'infobulle / l'annonce
    Color       color;    // teinte d'identité (texte, liseré)
} HazardDef;

const HazardDef *hazard_def(HazardType t);

/* Hasard de la tuile (tx, ty) de cette carte (HAZ_NONE hors limites). La
   couche est (re)construite à la demande quand la carte change. */
HazardType hazard_at(const Map *map, int tx, int ty);
/* Nombre de tuiles piégées de la carte (0 = aucun hasard). */
int        hazard_tile_count(const Map *map);

struct GameState;
/* Applique les hasards (ennemis toujours, unités pendant une vague) et
   annonce le hasard de la carte une fois. dt = temps de jeu (avance rapide). */
void hazard_update(struct GameState *gs, float dt);

/* Rendu (ui/hazard_art.c), dans la caméra de la carte : le sol piégé après la
   route et avant les entités ; l'infobulle de la tuile survolée (-1 = aucune)
   APRÈS les entités, pour rester lisible. */
void hazard_render(const Map *map);
void hazard_render_label(const Map *map, int tx, int ty);

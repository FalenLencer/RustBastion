/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/*  terrain.h ─ Terrain 2D peint au pixel, façon splash art.
 *
 *  Le décor STATIQUE d'une carte (sol, liquides, routes) est peint une seule
 *  fois, pixel par pixel, dans une texture (1 pixel d'art = 2 px monde,
 *  filtre « nearest » → pixel art net à tout zoom), puis dessiné en UN quad
 *  par frame. Les ruines (qui disparaissent sous une tour) sont des sprites
 *  peints à part ; les reflets des liquides sont animés par-dessus.
 *  Re-peint automatiquement quand la carte change (clé = identité + tuiles).
 */
#include "../map/map_gen.h"

/* Sol + liquides + routes de la carte (dans la caméra de la carte). */
void terrain_draw_base(const Map *map);
/* Sprite de ruine de la tuile (tx, ty) — à ne pas dessiner sous une tour. */
void terrain_draw_ruin(const Map *map, int tx, int ty);
/* Reflets / remous animés d'une tuile de liquide. t = horloge (s). */
void terrain_draw_liquid_fx(const Map *map, int tx, int ty, float t);
/* Socle peint sous une tour (terrain_pad.c), centré en (cx, cy) monde.
   scale/alpha : animation de pose (1 = posé) ; key : variante (hash de case). */
void terrain_draw_tower_pad(ThemeID th, float cx, float cy, float scale,
                            float alpha, unsigned key);
/* Filons de minerai peints (terrain_ore.c) : affleurement de la roche du
   thème + cristaux, dessiné à son rang de profondeur (comme une ruine). */
typedef enum { ORE_ACTIVE, ORE_DORMANT, ORE_MINED, ORE_STATE_COUNT } OreState;
void terrain_draw_ore(ThemeID th, int tx, int ty, int mat, OreState st, float t);
/* Même filon (actif) mis à l'échelle dans `dst` (fiche de découverte). */
void terrain_draw_ore_icon(Rectangle dst, int mat, float t, unsigned char alpha);
/* Libère les textures (fermeture du jeu). */
void terrain_unload(void);

/* Interne terrain*.c : sprites de ruines (terrain_ruins.c), socles, filons. */
void terrain_ruins_unload(void);
void terrain_pad_unload(void);
void terrain_ore_unload(void);

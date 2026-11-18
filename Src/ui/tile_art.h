/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  tile_art.h ─ Pixel-art procédural des tuiles spéciales.
 *
 *  Dessine, via le moteur (raylib), des chemins, spawns et bases
 *  bien plus soignés que les simples rectangles/traits d'origine.
 *  Tout est généré par code — aucune texture externe requise.
 *
 *  Ordre de rendu conseillé (dans le Camera2D de la carte) :
 *      render_map(map);            // terrain peint (terrain.h) + ruines
 *      tile_art_draw_spawns(map);  // portails d'invasion
 *      render_bases(map);          // bunkers (utilise tile_art_draw_base)
 *      ...
 */
#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../map/theme.h"

// Dessine tous les spawns (portails d'invasion clignotants).
void tile_art_draw_spawns(const Map *map);

// Dessine un bunker de base sur une tuile.
//   accent    : couleur d'équipe (vert primaire / bleu secondaire / rouge bas-PV)
//   destroyed : 1 = base tombée (ruine + croix)
void tile_art_draw_base(int px, int py, ThemeID theme,
                        Color accent, int destroyed);

// (Filons de minerai : terrain_draw_ore / terrain_draw_ore_icon, terrain.h.)

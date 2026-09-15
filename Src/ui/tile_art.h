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
 *      render_map(map);            // fonds de tuiles (sol/ruine/eau)
 *      tile_art_draw_paths(map);   // routes connectées sur les TILE_PATH
 *      tile_art_draw_spawns(map);  // portails d'invasion
 *      render_bases(map);          // bunkers (utilise tile_art_draw_base)
 *      ...
 */
#pragma once
#include "raylib.h"
#include "../map/map_gen.h"
#include "../map/theme.h"

struct TowerPool;

// Avance l'horloge d'animation du module (eau). À appeler UNE fois par
// frame (début de render_map) avec GetFrameTime() — convention projet :
// jamais GetTime() pour la logique.
void tile_art_tick(float dt);

// Dessine le fond pixel-art d'une tuile (sol / ruine / eau animée),
// puis ses franges de transition (rive dentelée contre l'eau, éboulis
// le long d'une ruine voisine). Les tuiles PATH/SPAWN/BASE reçoivent
// un fond de sol ; routes/portails/bunkers dessinés ensuite par-dessus.
//   cleared : 1 = une tour occupe la tuile → le décor de RUINE est
//   DÉBLAYÉ (sol nu constructible), comme en 3D (sinon la ruine reste
//   visible sous la tour).
void tile_art_draw_tile_bg(const Map *map, int tx, int ty, int cleared);

// Franges de transition d'UNE tuile (rive dentelée contre l'eau,
// éboulis le long d'une ruine voisine) — appelé par tile_art_draw_tile_bg
// (implémentation dans tile_art_decor.c).
void tile_art_draw_fringes(const Map *map, int tx, int ty);

// Ombres portées courtes vers le SUD-EST (lumière NO) des ruines/
// gratte-ciels et des tours. À appeler APRÈS tile_art_draw_paths
// (l'ombre tombe sur les routes) et AVANT bases/minerais/tours.
void tile_art_draw_shadows(const Map *map, const struct TowerPool *tp);

// Dessine toutes les tuiles de chemin de la carte, avec raccordement
// automatique aux voisins (route continue plutôt que traits).
void tile_art_draw_paths(const Map *map);

// Dessine tous les spawns (portails d'invasion clignotants).
void tile_art_draw_spawns(const Map *map);

// Dessine un bunker de base sur une tuile.
//   accent    : couleur d'équipe (vert primaire / bleu secondaire / rouge bas-PV)
//   destroyed : 1 = base tombée (ruine + croix)
void tile_art_draw_base(int px, int py, ThemeID theme,
                        Color accent, int destroyed);

// Filon de minerai actif : remplit toute la tuile (matrice rocheuse opaque
// + amas de cristaux + halo animé). Remplace visuellement la tuile.
//   mat_type : MaterialType (0=fer … 4=nano).  t : GetTime() pour l'animation.
void tile_art_draw_deposit(int px, int py, int mat_type, float t);

// Roche minée (filon épuisé) : cratère + éboulis. Signale une tuile
// difficilement constructible (la ruine sous-jacente applique le coût x2).
void tile_art_draw_mined_rock(int px, int py);

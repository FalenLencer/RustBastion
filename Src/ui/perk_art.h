/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  perk_art.h ─ Emblèmes des perks, peints au pixel façon splash art.
 *
 *  Un objet par perk (PerkId 0..PERK_COUNT-1) : volumes ombrés, contour
 *  sombre, halo à la couleur de sa rareté. Atlas peint une fois au premier
 *  dessin (perk_art.c). Utilisé par le bestiaire (onglet Butin) et les
 *  écrans de butin / boutique.
 */
#pragma once
#include "raylib.h"

// Dessine l'emblème du perk `perk_id` centré en (cx,cy), demi-taille ~r px.
// `col` : seul son ALPHA est appliqué (fondus) ; un gris pur hors blanc
// (r == g == b < 255) demande la version grisée (achat impossible). Pas de fond : à composer
// par-dessus un badge/cartouche. Net à 2r = 24 px ou multiple de 24.
void perk_art_draw(int perk_id, int cx, int cy, int r, Color col);
// Libère l'atlas (fermeture du jeu).
void perk_art_unload(void);

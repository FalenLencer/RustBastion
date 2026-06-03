/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  perk_art.h ─ Emblèmes pixel-art procéduraux des perks (butin & boutique).
 *
 *  Un dessin distinct par perk (PerkId 0..PERK_COUNT-1), généré au moteur
 *  (raylib) — aucune texture externe. Le symbole est teinté par `col`
 *  (couleur de rareté) et centré sur (cx,cy) dans un rayon ~r.
 *  Utilisé par le bestiaire (onglet Butin) et les écrans d'interlude.
 */
#pragma once
#include "raylib.h"

// Dessine l'emblème du perk `perk_id` centré en (cx,cy), demi-taille ~r px,
// teinté par `col`. Ne dessine que le symbole (pas de fond) : à composer
// par-dessus un badge/cartouche.
void perk_art_draw(int perk_id, int cx, int cy, int r, Color col);

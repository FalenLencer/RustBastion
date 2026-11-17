/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hints.h ─ Indices contextuels : un conseil quand la SITUATION l'appelle.
 *
 *  Pas de tutoriel bis : chaque conseil répond à une erreur ou une occasion
 *  observée en jeu (tours hors du chemin, or qui dort, minerai jamais
 *  appliqué, base en danger…). Un conseil s'affiche au plus UNE fois par
 *  carte, jamais deux à la fois, et plus du tout une fois une campagne
 *  terminée : le vétéran n'a pas à être materné. Aucun état sauvegardé.
 */
#pragma once

#define HINT_SHOW_TIME  7.0f   // durée d'affichage d'un conseil (s) — lue par le HUD

struct GameState;
/* À appeler à chaque frame de simulation, dt = temps RÉEL (affichage). */
void hints_update(struct GameState *gs, float dt);

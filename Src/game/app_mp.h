/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_mp.h ─ Sous-système MULTIJOUEUR (extrait de app.c).
 *  Boucle réseau en jeu, rendus d'overlay, écran envahisseur, lobby→partie.
 *  app.c (handle_menu / app_update / game_do_render) appelle ces fonctions.
 */
#pragma once
#include "app.h"
#include <stdint.h>

#define MP_PORT 47777   // port TCP par défaut (LAN / relais)

// Analyse "IP:port" (port optionnel → MP_PORT). 1 = OK. (serveur relais)
int  parse_relay_addr(const char *s, uint32_t *ip, uint16_t *port);

// Lance la partie MP (décode la config jointe au START + rôle Asym).
void mp_launch_game   (AppContext *ctx);

// Tick réseau en jeu (Course / Duel / Asym / Co-op). Hors garde de pause.
void mp_game_tick     (AppContext *ctx, float dt);

// Bandeau de résultat (REMATCH / MENU) : clics.
void mp_handle_result (AppContext *ctx);

// Écran de l'ENVAHISSEUR (Asym) : carte figée + compositeur de vagues.
void mp_invader_render(AppContext *ctx);

// Mini-HUD rival + boutons d'envoi Duel + bandeau résultat (par-dessus le jeu).
void mp_render_overlay(AppContext *ctx);

// Glissement des panneaux flottants MP (avant l'input HUD).
void mp_panels_input  (AppContext *ctx);

// Duel : boutons d'envoi (avant l'input HUD).
void mp_duel_send_input(AppContext *ctx);

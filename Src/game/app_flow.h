/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_flow.h ─ Enchaînement des fins d'acte et de partie (extrait de
 *  app.c, comme app_mp et app_tutorial). Tous appelés à chaque frame par
 *  app_update ; chacun est un no-op hors de l'état qui le concerne.
 */
#pragma once
#include "app.h"

/* Coût (Renfort) d'une relance de la boutique rogue-lite. PARTAGÉ : la
   logique d'achat (app_flow.c) ET l'affichage du prix (rendu, app.c). */
#define SHOP_REROLL_COST 25

void game_handle_gameover    (AppContext *ctx);  /* défaite / récap / replis  */
void game_handle_campaign_end(AppContext *ctx);  /* fin de campagne           */
void game_handle_dialog_after(AppContext *ctx);  /* après-acte + choix        */
void game_handle_draft       (AppContext *ctx);  /* butin rogue-lite          */
void game_handle_shop        (AppContext *ctx);  /* boutique rogue-lite       */
void game_handle_extract     (AppContext *ctx);  /* extraction endless        */

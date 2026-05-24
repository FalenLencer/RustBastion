/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "game_state.h"
#include "../map/theme.h"
#include "../ui/menu.h"   // CustomConfig

/* Génère une nouvelle carte avec un thème donné.
   forced_bases : 0 = aléatoire, >0 = nombre de bases imposé par le scénario.
   Boucle jusqu'à obtenir au moins un chemin A* valide. */
void game_init_map(GameState *gs, ThemeID theme, int forced_bases);

/* Variante étendue : forced_spawns, min_dist, deposits et taille configurables.
   Passer 0 pour forced_deposits/map_w/map_h reproduit le comportement par défaut. */
void game_init_map_full(GameState *gs, ThemeID theme,
                        int forced_bases, int forced_spawns, int min_dist,
                        int forced_deposits, int map_w, int map_h);

/* Démarre une partie arcade */
void game_init_arcade(GameState *gs, ThemeID theme, int slot);

/* Démarre une campagne depuis un acte donné (start_stage 0..CAMPAIGN_TOTAL-1).
   campaign_order_seed est maintenant toujours 0 (ordre fixe). */
void game_init_campaign(GameState *gs, int campaign_num, int slot,
                        int seed, int start_stage);

/* Passe au stage suivant de la campagne */
void game_next_campaign_stage(GameState *gs);

/* Démarre une partie personnalisée selon la configuration donnée. */
void game_init_custom(GameState *gs, const CustomConfig *cfg);
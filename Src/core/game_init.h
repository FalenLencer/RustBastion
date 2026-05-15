#pragma once
#include "game_state.h"
#include "../map/theme.h"

/* Génère une nouvelle carte avec un thème donné.
   Boucle jusqu'à obtenir au moins un chemin A* valide. */
void game_init_map(GameState *gs, ThemeID theme);

/* Démarre une partie arcade */
void game_init_arcade(GameState *gs, ThemeID theme, int slot);

/* Démarre une campagne depuis le début.
   CORRECTIF #2 : campaign_order_seed passé explicitement pour être
   affecté AVANT l'appel à game_init_map (qui a besoin du seed). */
void game_init_campaign(GameState *gs, int campaign_num, int slot, int seed);

/* Passe au stage suivant de la campagne */
void game_next_campaign_stage(GameState *gs);
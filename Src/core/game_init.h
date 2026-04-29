#pragma once
#include "game_state.h"
#include "../map/theme.h"

// Génère une nouvelle carte avec un thème
void game_init_map(GameState *gs, ThemeID theme);

// Démarre une partie arcade
void game_init_arcade(GameState *gs, ThemeID theme, int slot);

// Démarre une campagne depuis le début
void game_init_campaign(GameState *gs, int campaign_num, int slot);

// Passe au stage suivant de la campagne
void game_next_campaign_stage(GameState *gs);
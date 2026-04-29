#pragma once
#include "../core/game_state.h"
#include "../core/canvas.h"

// Affiche l'écran d'interlude entre les stages de campagne
// scrap_earned : ferraille gagnée ce stage
// last_stage : 1 si c'est le dernier stage de la campagne
void interlude_render(const GameState *gs, int scrap_earned, int last_stage);
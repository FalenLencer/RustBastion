#pragma once
#include "../game/game_state.h"
#include "../engine/canvas.h"
#include "campaign_data.h"

// Écran de dialogue AVANT un acte
void interlude_render_dialog_before(const ActData *act, int vw, int vh);

// Écran de dialogue APRÈS un acte (victoire)
void interlude_render_dialog_after(const ActData *act, int stars,
                                   int scrap_earned, int vw, int vh);

// Écran game over
void interlude_render_gameover(const GameState *gs, int vw, int vh);
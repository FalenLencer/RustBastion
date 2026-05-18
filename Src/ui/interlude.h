#pragma once
#include "../game/game_state.h"
#include "../engine/canvas.h"
#include "campaign_data.h"
#include "raylib.h"

// ════════════════════════════════════════════════════
// ÉTAT DES INTERLUDES
// ════════════════════════════════════════════════════
typedef enum {
    INTER_NONE          = 0,
    INTER_DIALOG_BEFORE,
    INTER_STAGE_WIN,
    INTER_DIALOG_AFTER,
    INTER_EXTRACT,
} InterludeState;

// ════════════════════════════════════════════════════
// ÉCRANS DE RENDU
// ════════════════════════════════════════════════════
void interlude_render_dialog_before(const ActData *act, int vw, int vh);
void interlude_render_dialog_after(const ActData *act, int stars, int scrap_earned, int vw, int vh);
void interlude_render_gameover(const GameState *gs, int vw, int vh);
void interlude_render_extract(const GameState *gs, int vw, int vh, Vector2 vmouse);

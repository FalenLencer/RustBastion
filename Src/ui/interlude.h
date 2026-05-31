/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "../game/game_state.h"
#include "../engine/canvas.h"
#include "../game/campaign_data.h"
#include "raylib.h"

// ════════════════════════════════════════════════════
// ÉTAT DES INTERLUDES
// ════════════════════════════════════════════════════
typedef enum {
    INTER_NONE          = 0,
    INTER_DIALOG_BEFORE,
    INTER_DIALOG_AFTER,
    INTER_EXTRACT,
    INTER_DRAFT,        // butin rogue-lite : choisir 1 perk parmi N (après chaque acte)
    INTER_SHOP,         // boutique rogue-lite : dépenser le Renfort (entre chapitres)
} InterludeState;

// ════════════════════════════════════════════════════
// ÉCRANS DE RENDU
// ════════════════════════════════════════════════════
void interlude_render_dialog_before(const ActData *act, int node_id, int flags,
                                    int vw, int vh);
void interlude_render_dialog_after(const ActData *act, int stars, int scrap_earned,
                                   int vw, int vh, int node_id, int flags);
void interlude_render_gameover(const GameState *gs, int vw, int vh);
void interlude_render_extract(const GameState *gs, int vw, int vh, Vector2 vmouse);

// ── Rogue-lite ────────────────────────────────────────────────
struct RunBuild;
// Écran BUTIN : présente rb->draft_offer[0..draft_n] (clic ou touches 1..N).
void interlude_render_draft(const struct RunBuild *rb, Vector2 vmouse, int vw, int vh);
// Écran BOUTIQUE : rb->shop_offer + solde Renfort (clic/touches 1..N, R, ESPACE).
void interlude_render_shop (const struct RunBuild *rb, int reroll_cost,
                            Vector2 vmouse, int vw, int vh);
// Index de l'offre de butin sous la souris (-1 si aucune).
int  interlude_draft_pick_at(const struct RunBuild *rb, Vector2 vmouse, int vw, int vh);
// Élément de boutique sous la souris : 0..N-1 = achat, -2 = relancer, -3 = partir, -1 = rien.
int  interlude_shop_pick_at (const struct RunBuild *rb, Vector2 vmouse, int vw, int vh);

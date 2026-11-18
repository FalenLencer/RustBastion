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
// Briefing / bilan de campagne (interlude_story.c). input_ok = 0 sur la
// frame d'entrée (anti-rebond d'input entre deux écrans).
// Briefing : retourne 1 quand le joueur lance l'acte ; la consigne retenue
// se lit avec story_scene_pick() (-1 = aucune).
int  interlude_render_dialog_before(const GameState *gs, Vector2 vmouse,
                                    int input_ok, int vw, int vh);
// Bilan : la carte de doctrine cliquée se lit avec story_scene_take_pick().
void interlude_render_dialog_after (const GameState *gs, int stars, int scrap_earned,
                                    int last, Vector2 vmouse, int input_ok,
                                    int vw, int vh);
/* P0.2 — écran de fin de partie. Retourne le choix cliqué (GoChoice).
   L'action est exécutée par l'appelant (app.c, game_handle_gameover). */
typedef enum {
    GO_CHOICE_NONE     = 0,
    GO_CHOICE_REPLAY   = 1,   // REJOUER (si allow_replay)
    GO_CHOICE_MENU     = 2,   // MENU / RETOUR A LA CARTE
    GO_CHOICE_CONTINUE = 3,   // campagne : reprise affaiblie / repli (= ESPACE)
    GO_CHOICE_ABANDON  = 4,   // campagne : abandon de la run (= ECHAP)
} GoChoice;
int  interlude_render_gameover(const GameState *gs, Vector2 vm,
                               int vw, int vh, int allow_replay);
void interlude_render_extract(const GameState *gs, int vw, int vh, Vector2 vmouse);
// Boutons de l'écran d'extraction (SOURCE UNIQUE : rendu + clic dans app_flow.c).
void interlude_extract_buttons(int vw, int vh, Rectangle *extract, Rectangle *cont);

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

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_flow.c ─ Enchaînement des écrans de fin d'acte et de partie :
 *  défaite / récapitulatif, fin de campagne, dialogue d'après-acte
 *  (choix narratifs), butin et boutique rogue-lite, extraction endless.
 *  Extrait TEL QUEL de app.c (comportement inchangé). Voir app_flow.h.
 */
#include "app_flow.h"
#include "game_init.h"   // inclut menu.h pour CustomConfig
#include "save.h"
#include "meta.h"
#include "../engine/audio.h"
#include "../engine/canvas.h"
#include "../engine/window.h"
#include "../ui/renderer.h"
#include "../ui/tile_art.h"
#include "../ui/ambient.h"    // météo d'ambiance 2D par thème
#include "../ui/hud.h"
#include "../ui/interlude.h"
#include "campaign_data.h"
#include "../ui/ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "../map/theme.h"
#include "../combat/fx.h"     // effets de jus (particules, secousse, pops)
#include "app_tutorial.h"     // tutoriel guidé (module extrait)
#include "app_mp.h"           // sous-système multijoueur (module extrait)
#include "../net/net_upnp.h"  // UPNP_HOOK : ouverture auto du port (module isolé)
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Reprise affaiblie (défaite en campagne) : fraction de l'or de départ
   conservée — le handicap de la reprise. */
#define RETRY_WEAK_GOLD_MULT  0.7f

/* P0.2 — REJOUER : relance immédiatement le même mode (arcade : même
   thème + même slot ; custom : la config du menu, qui persiste). */
static void gameover_replay(AppContext *ctx) {
    GameState *gs  = &ctx->gs;
    ThemeID    th  = gs->map.theme;
    int        slt = ctx->active_slot;
    if (gs->is_custom) {
        game_init_custom(gs, &ctx->menu.custom_cfg);
    } else {
        if (slt >= 0) save_delete(slt);
        game_init_arcade(gs, th, slt);
        if (slt >= 0) save_write(gs, slt);
    }
    ctx->gameover_meta_done = 0;
    ctx->gameover_choice    = 0;
    ctx->banner_timer       = 5.0f;
    gs->ui.show_fps = ctx->menu.opts.show_fps;
    menu_refresh_slots(&ctx->menu);
    audio_play_theme_music(gs->map.theme);
}

void game_handle_gameover(AppContext *ctx) {
    if (ctx->gs.phase != PHASE_GAMEOVER || ctx->interlude != INTER_NONE)
        return;
    /* Enregistre le score endless une seule fois */
    if (ctx->gs.is_endless && !ctx->gameover_meta_done) {
        meta_endless_end(&ctx->gs.meta,
                         ctx->gs.wave_manager.number,
                         ctx->gs.endless_multiplier, 0);
        menu_refresh_slots(&ctx->menu);
        ctx->gameover_meta_done = 1;
    }

    /* ── Campagne : la défaite peut bifurquer selon le nœud ──────
       ESPACE = continuer (repli / reprise affaiblie), ECHAP = abandonner.
       Sur un nœud critique (DEFEAT_GAMEOVER), on tombe dans le flux
       classique ci-dessous (retour à la carte).                       */
    /* Choix cliqué sur l'écran (posé par le rendu, frame précédente). */
    int choice = ctx->gameover_choice;
    ctx->gameover_choice = GO_CHOICE_NONE;

    if (ctx->gs.is_campaign && ctx->active_slot >= 0) {
        DefeatMode dm = campaign_defeat_mode(ctx->gs.campaign_stage);
        if (dm != DEFEAT_GAMEOVER) {
            if (IsKeyPressed(KEY_SPACE) || choice == GO_CHOICE_CONTINUE) {
                int stage = ctx->gs.campaign_stage;
                if (dm == DEFEAT_RETREAT) {
                    /* Trace narrative de la défaite (avant transition → préservée). */
                    if (stage == 5) ctx->gs.campaign_flags |= CFLAG_LOST_QUEEN;
                    if (stage == 8) ctx->gs.campaign_flags |= CFLAG_LOST_GENERAL;
                    int rn = campaign_defeat_node(stage);
                    game_goto_campaign_node(&ctx->gs, rn >= 0 ? rn : stage);
                    ui_push_notif(&ctx->gs.ui, "Repli strategique...",
                                  (Color){239, 159, 39, 255});
                } else {   /* DEFEAT_RETRY_WEAK */
                    /* MÊME carte : le joueur rejoue en corrigeant son placement
                       (avant, une carte neuve — parfois plus dure — effaçait
                       tout ce qu'il venait d'apprendre). */
                    game_init_reuse_map_seed(ctx->gs.map.seed);
                    game_goto_campaign_node(&ctx->gs, stage);
                    ctx->gs.gold = (int)((float)ctx->gs.gold * RETRY_WEAK_GOLD_MULT);
                    ui_push_notif(&ctx->gs.ui, "Reprise affaiblie — tenez bon !",
                                  (Color){239, 159, 39, 255});
                }
                ctx->interlude    = INTER_NONE;
                ctx->banner_timer = 5.0f;
                campaign_save_write(&ctx->gs, ctx->active_slot,
                                    INTER_NONE, 0, 0, 0);
                audio_play_theme_music(ctx->gs.map.theme);
            } else if (IsKeyPressed(KEY_ESCAPE) || choice == GO_CHOICE_ABANDON) {
                campaign_save_delete(ctx->active_slot);
                ctx->active_slot = -1;
                menu_refresh_slots(&ctx->menu);
                ctx->menu.paused = 0;
                ctx->menu.screen = MENU_WORLD_MAP;
                ctx->screen      = SCREEN_MENU;
                ctx->interlude   = INTER_NONE;  /* rien ne survit au retour menu */
                audio_play_menu_music();
            }
            return;   /* attend le choix du joueur */
        }
    }

    /* P0.2 — choix cliqué sur le récap (posé par le rendu) ; ESPACE
       reste un raccourci clavier vers le menu. */
    if (choice == GO_CHOICE_REPLAY) {
        gameover_replay(ctx);
        return;
    }
    if (choice == GO_CHOICE_MENU || IsKeyPressed(KEY_SPACE)) {
        if (ctx->active_slot >= 0) {
            if (ctx->gs.is_campaign)
                campaign_save_delete(ctx->active_slot);
            else
                save_delete(ctx->active_slot);
        }
        ctx->active_slot        = -1;
        ctx->gameover_meta_done = 0;  /* reset pour la prochaine partie */
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused  = 0;
        ctx->menu.screen  = ctx->gs.is_campaign ? MENU_WORLD_MAP
                          : ctx->gs.is_custom    ? MENU_CUSTOM
                          : MENU_ARCADE;
        ctx->screen       = SCREEN_MENU;
        ctx->interlude    = INTER_NONE;  /* rien ne survit au retour menu */
        audio_play_menu_music();
    }
}

void game_handle_campaign_end(AppContext *ctx) {
    if (!ctx->gs.is_campaign            ||
        ctx->gs.phase   != PHASE_PREP   ||
        ctx->gs.wave_manager.number < 1 ||
        ctx->interlude  != INTER_NONE)
        return;

    const ActData *ad = campaign_act_get(ctx->gs.campaign_stage);
    int _wave_done = (ctx->gs.wave_manager.number >= ad->min_waves);
    if (!_wave_done) return;   /* quota de vagues pas encore atteint */

    /* L'acte se termine au quota de vagues. L'objectif n'est plus bloquant :
       le réussir ou non ORIENTE la suite du parcours (graphe de branches).
       Échouer une quête devient une bifurcation, pas une impasse. */
    int obj_ok = campaign_objective_check(
                     ad, ctx->gs.wave_manager.number,
                     ctx->gs.kills, ctx->gs.units.count,
                     ctx->gs.act_materials_collected,
                     ctx->gs.act_no_unit_lost);

    int stage_idx       = ctx->gs.campaign_stage;
    int next_node       = campaign_next_node(stage_idx, obj_ok, 0);
    int last            = (next_node < 0);
    int earned          = meta_end_of_campaign_stage(
                              &ctx->gs.meta, ctx->gs.wave_manager.number,
                              ctx->gs.kills, ctx->gs.gold, stage_idx);
    int stars           = meta_record_act(&ctx->gs.meta, stage_idx, obj_ok, 0);

    audio_play_sfx(AUDIO_SFX_VICTORY);
    ctx->interlude       = INTER_DIALOG_AFTER;
    ctx->interlude_scrap = earned;
    ctx->interlude_last  = last;
    ctx->interlude_stars = stars;
    /* Sauvegarde avec état interlude pour éviter le double-ferraille au rechargement */
    campaign_save_write(&ctx->gs, ctx->active_slot,
                        INTER_DIALOG_AFTER, earned, stars, last);
}

void game_handle_dialog_after(AppContext *ctx) {
    if (ctx->interlude != INTER_DIALOG_AFTER) return;
    if (ctx->interlude != ctx->interlude_prev) return;   // ignore le frame d'entrée

    /* Fin de campagne : un simple ESPACE/clic ramène à la carte. */
    if (ctx->interlude_last) {
        if (!IsKeyPressed(KEY_SPACE) && !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            return;
        if (ctx->active_slot >= 0) campaign_save_delete(ctx->active_slot);
        ctx->active_slot  = -1;
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused  = 0;
        ctx->menu.screen  = MENU_WORLD_MAP;
        ctx->screen       = SCREEN_MENU;
        ctx->interlude    = INTER_NONE;
        audio_play_menu_music();
        return;
    }

    int stage  = ctx->gs.campaign_stage;
    int choice = 0;

    if (campaign_has_choice(stage)) {
        /* Bifurcation pilotée par le joueur : touches 1 / 2. */
        if      (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) choice = 0;
        else if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) choice = 1;
        else return;   /* on attend que le joueur tranche */
    } else {
        if (!IsKeyPressed(KEY_SPACE) && !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            return;
    }

    /* Route vers le nœud suivant selon l'issue + le choix.
       obj_ok recalculé depuis les étoiles (>0 ⇒ objectif réussi) → fiable
       même après rechargement d'une sauvegarde en état DIALOG_AFTER. */
    int next_node = campaign_next_node(stage, ctx->interlude_stars > 0, choice);

    /* Trace narrative : le choix laisse un drapeau persistant (callbacks + épilogue). */
    ctx->gs.campaign_flags |= campaign_choice_flag(stage, choice);

    /* Sécurité : un branchement menant à -1 = fin de campagne. */
    if (next_node < 0) {
        if (ctx->active_slot >= 0) campaign_save_delete(ctx->active_slot);
        ctx->active_slot = -1;
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused = 0;
        ctx->menu.screen = MENU_WORLD_MAP;
        ctx->screen      = SCREEN_MENU;
        ctx->interlude   = INTER_NONE;
        audio_play_menu_music();
        return;
    }

    /* ── Rogue-lite : Renfort gagné + transition + tirage du butin ── */
    int obj_ok      = (ctx->interlude_stars > 0);
    int cur_ch      = campaign_difficulty_stage(stage)     / CAMPAIGN_ACTS;
    int next_ch     = campaign_difficulty_stage(next_node) / CAMPAIGN_ACTS;
    int new_chapter = (next_ch > cur_ch);

    /* Renfort de l'acte accompli (ajouté AVANT la transition → préservé).
       ~90-130 par acte → ~4-6 achats communs (ou 2 épiques) par chapitre :
       la boutique à rachat infini doit permettre PLUSIEURS achats. */
    int gain = 40 + ctx->gs.wave_manager.number * 4 + (obj_ok ? 30 : 0);
    if (ctx->gs.challenge_mode)
        gain += gain / 4;            /* MODE CHALLENGE : +25 % de Renfort */
    ctx->gs.run.renfort += gain;

    game_goto_campaign_node(&ctx->gs, next_node);

    /* Butin : 3 offres (2 si objectif raté), rareté biaisée par la voie. */
    int draft_count = obj_ok ? 3 : 2;
    int bias        = (choice == 1) ? 1 : 0;
    ctx->gs.run.draft_n = runbuild_roll_offers(&ctx->gs.run,
                              ctx->gs.run.draft_offer, draft_count, bias, 0);
    ctx->gs.run.shop_pending = new_chapter ? 1 : 0;

    ctx->interlude = INTER_DRAFT;
    campaign_save_write(&ctx->gs, ctx->active_slot, INTER_DRAFT, 0, 0, 0);
}

/* Butin : choisir 1 perk parmi N (touches 1..N). Enchaîne sur la boutique
   (si on entre dans un nouveau chapitre) ou directement sur l'acte. */
void game_handle_draft(AppContext *ctx) {
    if (ctx->interlude != INTER_DRAFT) return;
    if (ctx->interlude != ctx->interlude_prev && ctx->gs.run.draft_n > 0)
        return;   // ignore le frame d'entrée (sauf s'il n'y a rien à choisir)
    RunBuild *rb = &ctx->gs.run;
    int pick = -1;

    if (rb->draft_n <= 0) {
        pick = -2;   // rien à proposer → on enchaîne directement
    } else {
        if      (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) pick = 0;
        else if (rb->draft_n > 1 && (IsKeyPressed(KEY_TWO)  || IsKeyPressed(KEY_KP_2))) pick = 1;
        else if (rb->draft_n > 2 && (IsKeyPressed(KEY_THREE)|| IsKeyPressed(KEY_KP_3))) pick = 2;
        else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int p = interlude_draft_pick_at(rb, virt_mouse(),
                                            g_canvas_virt_w, g_canvas_virt_h);
            if (p >= 0 && p < rb->draft_n) pick = p;
            else return;
        }
        else return;   // attend le choix
    }

    if (pick >= 0) {
        runbuild_add(rb, rb->draft_offer[pick]);
        ui_push_notif(&ctx->gs.ui, "Renfort acquis !", (Color){232, 200, 80, 255});
    }
    rb->draft_n = 0;

    if (rb->shop_pending) {
        rb->shop_n = runbuild_roll_offers(rb, rb->shop_offer, MAX_SHOP_OFFER, 0, 1);
        ctx->interlude = INTER_SHOP;
        campaign_save_write(&ctx->gs, ctx->active_slot, INTER_SHOP, 0, 0, 0);
    } else {
        ctx->interlude    = INTER_DIALOG_BEFORE;
        ctx->banner_timer = 5.0f;
        campaign_save_write(&ctx->gs, ctx->active_slot, INTER_DIALOG_BEFORE, 0, 0, 0);
        audio_play_theme_music(ctx->gs.map.theme);
    }
}

/* Boutique : acheter des perks en Renfort (1..4), relancer [R], partir [ESPACE]. */
void game_handle_shop(AppContext *ctx) {
    if (ctx->interlude != INTER_SHOP) return;
    if (ctx->interlude != ctx->interlude_prev) return;   // ignore le frame d'entrée
    RunBuild *rb = &ctx->gs.run;

    int buy = -1, do_reroll = 0, do_done = 0;
    if      (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) buy = 0;
    else if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) buy = 1;
    else if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) buy = 2;
    else if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) buy = 3;
    else if (IsKeyPressed(KEY_R))     do_reroll = 1;
    else if (IsKeyPressed(KEY_SPACE)) do_done   = 1;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int r = interlude_shop_pick_at(rb, virt_mouse(),
                                       g_canvas_virt_w, g_canvas_virt_h);
        if      (r >= 0)  buy = r;
        else if (r == -2) do_reroll = 1;
        else if (r == -3) do_done   = 1;
        else return;
    }
    else return;

    if (buy >= 0 && buy < rb->shop_n) {
        int id = rb->shop_offer[buy];
        if (id >= 0 && id < PERK_COUNT) {
            const PerkDef *pd = &RUN_PERKS[id];
            if (rb->count[id] < pd->max_stack && rb->renfort >= pd->shop_cost) {
                rb->renfort -= pd->shop_cost;
                runbuild_add(rb, id);
                // Rachat infini : l'article acheté est REMPLACÉ par un nouveau.
                runbuild_reroll_slot(rb, rb->shop_offer, rb->shop_n, buy);
                ui_push_notif(&ctx->gs.ui, "Achat effectue !", (Color){120, 210, 120, 255});
                campaign_save_write(&ctx->gs, ctx->active_slot, INTER_SHOP, 0, 0, 0);
            } else {
                ui_push_notif(&ctx->gs.ui, "Indisponible (Renfort ou max)",
                              (Color){231, 76, 60, 255});
            }
        }
        return;
    }

    if (do_reroll) {
        if (rb->renfort >= SHOP_REROLL_COST) {
            rb->renfort -= SHOP_REROLL_COST;
            rb->shop_n = runbuild_roll_offers(rb, rb->shop_offer, MAX_SHOP_OFFER, 0, 1);
            campaign_save_write(&ctx->gs, ctx->active_slot, INTER_SHOP, 0, 0, 0);
        } else {
            ui_push_notif(&ctx->gs.ui, "Renfort insuffisant", (Color){231, 76, 60, 255});
        }
        return;
    }

    if (do_done) {
        rb->shop_pending  = 0;
        ctx->interlude    = INTER_DIALOG_BEFORE;
        ctx->banner_timer = 5.0f;
        campaign_save_write(&ctx->gs, ctx->active_slot, INTER_DIALOG_BEFORE, 0, 0, 0);
        audio_play_theme_music(ctx->gs.map.theme);
    }
}

void game_handle_extract(AppContext *ctx) {
    if (ctx->interlude != INTER_EXTRACT) return;

    /* Souris virtuelle (offset + échelle de présentation) */
    Vector2 vm = virt_mouse();

    /* Rectangles des boutons — doivent correspondre à interlude_render_extract */
    {
        int vw = g_canvas_virt_w, vh = g_canvas_virt_h;
        int cx = vw / 2, cy = vh / 2;
        int ph = 260, bw = 160, bh = 32, ms = 8;
        int by2 = cy + ph / 2 - ms - bh;
        Rectangle extract_btn  = {(float)(cx - bw - ms), (float)by2,
                                   (float)bw,             (float)bh};
        Rectangle continue_btn = {(float)(cx + ms),      (float)by2,
                                   (float)bw,             (float)bh};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int extract_clicked  = click && CheckCollisionPointRec(vm, extract_btn);
        int continue_clicked = click && CheckCollisionPointRec(vm, continue_btn);

        if (IsKeyPressed(KEY_E) || extract_clicked) {
            meta_endless_end(&ctx->gs.meta,
                             ctx->gs.wave_manager.number,
                             ctx->gs.endless_multiplier, 1);
            menu_refresh_slots(&ctx->menu);
            if (ctx->active_slot >= 0) save_delete(ctx->active_slot);
            ctx->active_slot  = -1;
            ctx->menu.paused  = 0;
            ctx->menu.screen  = MENU_ARCADE;
            ctx->screen       = SCREEN_MENU;
            ctx->interlude    = INTER_NONE;
            audio_play_menu_music();
            return;
        }
        if (IsKeyPressed(KEY_SPACE) || continue_clicked) {
            ctx->gs.endless_multiplier *= 1.5f;
            if (ctx->gs.endless_multiplier > 6.0f)
                ctx->gs.endless_multiplier = 6.0f;
            ctx->gs.endless_series++;
            ctx->gs.endless_pending_extract = 0;
            game_init_map(&ctx->gs,
                          (ThemeID)((ctx->gs.map.theme + 1) % THEME_COUNT),
                          0);  // arcade : nombre de bases aléatoire
            audio_play_theme_music(ctx->gs.map.theme);
            ctx->interlude = INTER_NONE;
            return;
        }
    }
}

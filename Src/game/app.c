/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/* ════════════════════════════════════════════════════════════════
   game/app.c — logique de boucle principale
   Contient toute la logique de haut niveau extraite de main.c :
   gestion des écrans, interludes, transitions, rendu de la scène.
   ════════════════════════════════════════════════════════════════ */
#include "app.h"
#include "game_init.h"   // inclut menu.h pour CustomConfig
#include "save.h"
#include "meta.h"
#include "../engine/audio.h"
#include "../engine/canvas.h"
#include "../engine/window.h"
#include "../ui/renderer.h"
#include "../ui/tile_art.h"
#include "../ui/hud.h"
#include "../ui/interlude.h"
#include "campaign_data.h"
#include "../ui/ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "../map/theme.h"
#include "../combat/fx.h"     // effets de jus (particules, secousse, pops)
#include "raylib.h"
#include <string.h>
#include <stdio.h>

// Multijoueur
#define MP_PORT             47777
#define MP_STATUS_INTERVAL  0.4f    // période d'envoi du statut (s)

// ════════════════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════════════════
void app_init(AppContext *ctx) {
    memset(ctx, 0, sizeof(AppContext));

    game_state_init(&ctx->gs);
    snprintf(ctx->mp_name, sizeof(ctx->mp_name), "Joueur");

    AppOptions opts = {
        .win_width     = VIRT_W,
        .win_height    = VIRT_H,
        .target_fps    = 60,
        .master_volume = 50,
        .music_volume  = 50,
        .sfx_volume    = 50,
        .fx_effects    = 1,
    };
    opts_load(&opts);
    audio_set_master_volume(opts.master_volume / 100.0f);
    audio_set_music_volume (opts.music_volume  / 100.0f);
    audio_set_sfx_volume   (opts.sfx_volume    / 100.0f);
    g_fx.enabled = opts.fx_effects;

    menu_init(&ctx->menu, &opts);

    ctx->active_slot      = -1;
    ctx->screen           = SCREEN_MENU;
    ctx->interlude        = INTER_NONE;
    ctx->applied_fps      = 60;
    ctx->prev_menu_screen = ctx->menu.screen;

    audio_play_menu_music();
}

// ════════════════════════════════════════════════════════════════
// SAVE / CLEANUP
// ════════════════════════════════════════════════════════════════
/* ── Sauvegarde de sortie (fermeture fenêtre ou Alt+F4) ─────── */
void app_save_on_exit(const AppContext *ctx) {
    if (ctx->screen != SCREEN_GAME) return;
    if (ctx->active_slot < 0) return;
    if (ctx->gs.phase == PHASE_GAMEOVER) return;
    if (ctx->gs.is_campaign) {
        /* Campagne : sauvegarde même si un interlude est actif */
        campaign_save_write(&ctx->gs, ctx->active_slot,
                            (int)ctx->interlude,
                            ctx->interlude_scrap,
                            ctx->interlude_stars,
                            ctx->interlude_last);
    } else {
        /* Arcade/endless : sauvegarde uniquement hors interlude */
        if (ctx->interlude == INTER_NONE)
            save_write(&ctx->gs, ctx->active_slot);
    }
}

void app_cleanup(AppContext *ctx) {
    opts_save(&ctx->menu.opts);
    menu_cleanup(&ctx->menu);
}

// Lance la partie multijoueur (mode Course) avec le seed partagé.
static void mp_launch_game(AppContext *ctx) {
    NetSession *s = &ctx->session;
    ctx->mp_in_game      = 1;
    ctx->mp_peer_valid   = 0;
    ctx->mp_result       = 0;
    ctx->mp_status_timer = 0.0f;
    memset(&ctx->mp_peer, 0, sizeof(ctx->mp_peer));

    // Course : board type arcade, MÊME seed → même carte (équité).
    unsigned int seed = s->seed ? s->seed : 1u;
    SetRandomSeed(seed);
    ThemeID theme = (ThemeID)(seed % THEME_COUNT);
    ctx->active_slot = -1;                 // pas de sauvegarde en multijoueur
    game_init_arcade(&ctx->gs, theme, -1);

    ctx->menu.screen    = MENU_TITLE;
    ctx->screen         = SCREEN_GAME;
    ctx->banner_timer   = 5.0f;
    ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
    audio_play_theme_music(ctx->gs.map.theme);
}

// ════════════════════════════════════════════════════════════════
// ÉCRAN MENU
// ════════════════════════════════════════════════════════════════
static int handle_menu(AppContext *ctx) {
    /* Réinitialise le zoom de carte au retour au menu */
    g_map_render_scale = 1.0f;
    menu_update(&ctx->menu, &ctx->gs.meta);
    ClearBackground((Color){8,5,3,255});
    MenuAction act = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                         g_canvas_virt_w, g_canvas_virt_h);

    if (act.quit_app) return 0;

    if (act.toggle_fs == 1) {
        ToggleFullscreen();
        ctx->menu.opts.fullscreen = IsWindowFullscreen();
    }
    if (act.toggle_fs == 2 && !IsWindowFullscreen())
        window_apply_size(ctx->menu.opts.win_width,
                          ctx->menu.opts.win_height);

    if (act.start_arcade) {
        ctx->active_slot = act.new_slot;
        game_init_arcade(&ctx->gs, act.new_theme, ctx->active_slot);
        save_write(&ctx->gs, ctx->active_slot);
        ctx->menu.screen  = MENU_TITLE;
        ctx->screen       = SCREEN_GAME;
        ctx->banner_timer = 5.0f;
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.start_custom) {
        ctx->active_slot = -1;   // pas de sauvegarde pour le mode custom
        game_init_custom(&ctx->gs, &act.custom_cfg);
        ctx->menu.screen  = MENU_TITLE;
        ctx->screen       = SCREEN_GAME;
        ctx->banner_timer = 5.0f;
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.start_campaign) {
        ctx->active_slot = act.new_slot;
        ctx->gs.campaign_order_seed = act.campaign_order_seed;
        game_init_campaign(&ctx->gs, ctx->gs.meta.campaigns_completed,
                           ctx->active_slot, act.campaign_order_seed,
                           act.start_campaign_act);
        /* Sauvegarde initiale avec état interlude = DIALOG_BEFORE */
        campaign_save_write(&ctx->gs, ctx->active_slot,
                            INTER_DIALOG_BEFORE, 0, 0, 0);
        ctx->menu.screen  = MENU_TITLE;
        ctx->screen       = SCREEN_GAME;
        ctx->interlude    = INTER_DIALOG_BEFORE;
        ctx->banner_timer = 5.0f;
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.go_game && !act.start_arcade && !act.start_campaign) {
        int slot = act.resume_slot;
        if (act.resume_is_campaign) {
            int inter = 0, scrap = 0, stars = 0, last = 0;
            if (campaign_save_read(&ctx->gs, slot, &inter, &scrap, &stars, &last)) {
                ctx->active_slot     = slot;
                ctx->interlude       = (InterludeState)inter;
                ctx->interlude_scrap = scrap;
                ctx->interlude_stars = stars;
                ctx->interlude_last  = last;
                ctx->menu.screen     = MENU_TITLE;
                ctx->screen          = SCREEN_GAME;
                ctx->banner_timer    = 5.0f;

                /* L'état du boss n'est pas sérialisé : on le recalcule depuis
                   le nœud chargé. Pending uniquement si l'acte est une finale
                   ET que la vague du boss n'est pas encore atteinte (un boss
                   déjà apparu est, lui, restauré dans le pool d'ennemis). */
                {
                    int dstage = campaign_difficulty_stage(ctx->gs.campaign_stage);
                    const ActData *na = campaign_act_get(ctx->gs.campaign_stage);
                    if ((dstage % CAMPAIGN_ACTS) == (CAMPAIGN_ACTS - 1)) {
                        ctx->gs.boss_chapter = dstage / CAMPAIGN_ACTS;
                        ctx->gs.boss_wave    = na->min_waves;
                        ctx->gs.boss_pending =
                            (ctx->gs.wave_manager.number < na->min_waves);
                    } else {
                        ctx->gs.boss_pending = 0;
                    }
                }
            } else {
                /* Fichier campagne illisible — repart de l'acte 0 */
                ctx->active_slot  = slot;
                game_init_campaign(&ctx->gs, ctx->gs.meta.campaigns_completed,
                                   slot, 0, 0);
                campaign_save_write(&ctx->gs, slot, INTER_DIALOG_BEFORE, 0, 0, 0);
                ctx->interlude    = INTER_DIALOG_BEFORE;
                ctx->screen       = SCREEN_GAME;
                ctx->banner_timer = 5.0f;
            }
        } else {
            if (save_read(&ctx->gs, slot)) {
                ctx->active_slot  = slot;
                ctx->menu.screen  = MENU_TITLE;
                ctx->screen       = SCREEN_GAME;
                ctx->banner_timer = 5.0f;
            } else {
                ctx->active_slot  = slot;
                game_init_arcade(&ctx->gs, THEME_COUNT, slot);
                save_write(&ctx->gs, ctx->active_slot);
                ctx->screen       = SCREEN_GAME;
                ctx->banner_timer = 5.0f;
            }
        }
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    // ── Multijoueur : cycle de vie de la session ─────────────
    if (act.mp_host) {
        net_session_close(&ctx->session);
        ctx->mp_active = 0;
        int seed = GetRandomValue(1, 0x3FFFFFFF);
        ctx->mp_mode = act.mp_mode ? act.mp_mode : MP_COURSE;
        if (net_session_host(&ctx->session, MP_PORT, (uint32_t)seed,
                             (uint8_t)ctx->mp_mode, ctx->mp_name))
            ctx->mp_active = 1;
    }
    if (act.mp_join) {
        net_session_close(&ctx->session);
        ctx->mp_active = 0;
        if (net_session_join(&ctx->session, act.mp_code, ctx->mp_name))
            ctx->mp_active = 1;
    }
    if (ctx->mp_active && act.mp_ready)
        net_session_set_ready(&ctx->session, !ctx->session.my_ready);
    if (ctx->mp_active && act.mp_start)
        net_session_start(&ctx->session);
    if (act.mp_leave) {
        net_session_close(&ctx->session);
        ctx->mp_active = 0;
        ctx->menu.mp_view.active = 0;
    }

    // ── Avance la session + remplit la vue + lance la partie ──
    {
        int in_mp = (ctx->menu.screen == MENU_MP_HUB ||
                     ctx->menu.screen == MENU_MP_LOBBY);
        if (ctx->mp_active && in_mp) {
            net_session_update(&ctx->session);
            NetSession  *s = &ctx->session;
            MpLobbyView *v = &ctx->menu.mp_view;
            v->active         = 1;
            v->is_host        = s->is_host;
            v->peer_connected = (s->state == SESS_LOBBY || s->state == SESS_INGAME);
            v->my_ready       = s->my_ready;
            v->peer_ready     = s->peer_ready;
            v->started        = s->started;
            v->failed         = (s->state == SESS_FAILED);
            snprintf(v->code, sizeof(v->code), "%s", s->code);
            snprintf(v->peer_name, sizeof(v->peer_name), "%s", s->peer_name);
            if (s->mode) { ctx->mp_mode = s->mode; ctx->menu.mp_mode = s->mode; }
            if (s->started) mp_launch_game(ctx);
        } else if (ctx->mp_active && !in_mp && ctx->screen == SCREEN_MENU) {
            net_session_close(&ctx->session);   // quitté les écrans MP → ferme
            ctx->mp_active = 0;
            ctx->menu.mp_view.active = 0;
        }
    }

    return 1;
}

// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — MISE À JOUR
// ════════════════════════════════════════════════════════════════
static void game_do_input(AppContext *ctx) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        ctx->menu.paused ^= 1;
        ctx->menu.screen  = ctx->menu.paused ? MENU_PAUSE : MENU_TITLE;
    }
    /* Bouton pause cliqué dans le HUD */
    if (!ctx->menu.paused && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 vm = virt_mouse();
        if (CheckCollisionPointRec(vm, ctx->gs.ui.pause_btn)) {
            ctx->menu.paused = 1;
            ctx->menu.screen = MENU_PAUSE;
        }
    }
}

static void game_do_update(AppContext *ctx, float dt) {
    if (ctx->menu.paused || ctx->interlude != INTER_NONE) return;

    /* Décompte du minuteur de bannière */
    if (ctx->banner_timer > 0.0f) {
        ctx->banner_timer -= dt;
        if (ctx->banner_timer < 0.0f) ctx->banner_timer = 0.0f;
    }

    ui_update(&ctx->gs.ui, &ctx->gs);
    // Fiche de découverte visible → jeu gelé (ui_update gère la fermeture)
    if (ctx->gs.ui.disc_count > 0) return;
    game_state_update(&ctx->gs, dt);
    fx_update(dt);   // particules / popups / secousse

    /* Déclenchement extraction endless toutes les 10 vagues (désactivé en MP) */
    if (!ctx->mp_in_game                  &&
        ctx->gs.is_endless               &&
        !ctx->gs.endless_pending_extract  &&
        ctx->gs.phase == PHASE_PREP       &&
        ctx->gs.wave_manager.number > 0   &&
        ctx->gs.wave_manager.number % 10 == 0)
    {
        ctx->gs.endless_pending_extract = 1;
        ctx->interlude = INTER_EXTRACT;
    }
}

// Tick réseau en jeu (mode Course) : réception du statut adverse + envoi du
// nôtre + détection de victoire/défaite. Tourne même en pause (keep-alive).
static void mp_game_tick(AppContext *ctx, float dt) {
    if (!ctx->mp_active || !ctx->mp_in_game) return;
    NetSession *s = &ctx->session;
    net_session_update(s);

    NetMsgHeader h;
    unsigned char pl[NET_MAX_PAYLOAD];
    while (net_session_recv(s, &h, pl, sizeof(pl))) {
        if (h.type == NMSG_STATUS && h.len >= (int)sizeof(NetStatus)) {
            memcpy(&ctx->mp_peer, pl, sizeof(NetStatus));
            ctx->mp_peer_valid = 1;
        } else if (h.type == NMSG_GAMEOVER) {
            if (ctx->mp_result == 0 && ctx->gs.lives > 0) ctx->mp_result = 1; // adversaire tombé
        }
    }
    if (s->peer_gone && ctx->mp_result == 0) ctx->mp_result = 1;  // déconnexion adverse

    if (!ctx->menu.paused) {
        ctx->mp_status_timer -= dt;
        if (ctx->mp_status_timer <= 0.0f) {
            ctx->mp_status_timer = MP_STATUS_INTERVAL;
            NetStatus st;
            st.wave  = ctx->gs.wave_manager.number;
            st.kills = ctx->gs.kills;
            st.gold  = ctx->gs.gold;
            st.lives = ctx->gs.lives;
            st.alive = (ctx->gs.lives > 0) ? 1 : 0;
            net_session_send(s, NMSG_STATUS, &st, sizeof(st));
        }
    }
    if (ctx->mp_result == 0 && ctx->gs.lives <= 0) {   // défaite locale
        net_session_send(s, NMSG_GAMEOVER, NULL, 0);
        ctx->mp_result = 2;
    }
}

// Résultat MP affiché → un input ramène au menu (ferme la session).
static void mp_handle_result(AppContext *ctx) {
    if (ctx->mp_result == 0) return;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        net_session_close(&ctx->session);
        ctx->mp_active   = 0;
        ctx->mp_in_game  = 0;
        ctx->menu.paused = 0;
        ctx->screen      = SCREEN_MENU;
        ctx->menu.screen = MENU_MP_HUB;
    }
}

static void game_handle_gameover(AppContext *ctx) {
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
    if (ctx->gs.is_campaign && ctx->active_slot >= 0) {
        DefeatMode dm = campaign_defeat_mode(ctx->gs.campaign_stage);
        if (dm != DEFEAT_GAMEOVER) {
            if (IsKeyPressed(KEY_SPACE)) {
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
                    game_goto_campaign_node(&ctx->gs, stage);
                    ctx->gs.gold = (int)((float)ctx->gs.gold * 0.7f);  /* handicap */
                    ui_push_notif(&ctx->gs.ui, "Reprise affaiblie — tenez bon !",
                                  (Color){239, 159, 39, 255});
                }
                ctx->interlude    = INTER_NONE;
                ctx->banner_timer = 5.0f;
                campaign_save_write(&ctx->gs, ctx->active_slot,
                                    INTER_NONE, 0, 0, 0);
                audio_play_theme_music(ctx->gs.map.theme);
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                campaign_save_delete(ctx->active_slot);
                ctx->active_slot = -1;
                menu_refresh_slots(&ctx->menu);
                ctx->menu.paused = 0;
                ctx->menu.screen = MENU_WORLD_MAP;
                ctx->screen      = SCREEN_MENU;
                audio_play_menu_music();
            }
            return;   /* attend le choix du joueur */
        }
    }

    if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        audio_play_menu_music();
    }
}

static void game_handle_campaign_end(AppContext *ctx) {
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

static void game_handle_dialog_after(AppContext *ctx) {
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
       ~75-110 par acte → ~2-3 achats communs (ou 1 épique) par chapitre. */
    int gain = 25 + ctx->gs.wave_manager.number * 3 + (obj_ok ? 25 : 0);
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

#define SHOP_REROLL_COST 25

/* Butin : choisir 1 perk parmi N (touches 1..N). Enchaîne sur la boutique
   (si on entre dans un nouveau chapitre) ou directement sur l'acte. */
static void game_handle_draft(AppContext *ctx) {
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
static void game_handle_shop(AppContext *ctx) {
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

static void game_handle_extract(AppContext *ctx) {
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

// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — RENDU
// retourne 0 si l'utilisateur demande à quitter (pause → quitter)
// ════════════════════════════════════════════════════════════════
// Overlay multijoueur : mini-HUD du rival + bandeau de résultat.
static void mp_render_overlay(AppContext *ctx) {
    if (!ctx->mp_in_game) return;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;

    // Mini-HUD rival (haut-centre)
    int pw = 240, ph = 34, px = base_cx - pw/2, py = 4;
    DrawRectangleRounded((Rectangle){(float)px,(float)py,(float)pw,(float)ph},
                         0.22f, 5, (Color){10, 7, 3, 215});
    DrawRectangleRoundedLinesEx((Rectangle){(float)px,(float)py,(float)pw,(float)ph},
                         0.22f, 5, 1.2f, (Color){70, 48, 16, 210});
    const char *nm = ctx->session.peer_name[0] ? ctx->session.peer_name : "Adversaire";
    char top[72];
    snprintf(top, sizeof(top), "RIVAL : %s", nm);
    int tw0 = mtxt(top, 9);
    dtxt(top, base_cx - tw0/2, py + 3, 9, (Color){200, 180, 120, 255});
    char cmp[72];
    if (ctx->mp_peer_valid)
        snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui V%d PV%d",
                 ctx->gs.wave_manager.number, ctx->gs.lives,
                 ctx->mp_peer.wave, ctx->mp_peer.lives);
    else
        snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui (connexion...)",
                 ctx->gs.wave_manager.number, ctx->gs.lives);
    int cw0 = mtxt(cmp, 8);
    dtxt(cmp, base_cx - cw0/2, py + 18, 8, (Color){150, 130, 80, 255});

    // Bandeau de résultat
    if (ctx->mp_result != 0) {
        int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
        DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0, 0, 0, 175});
        const char *t = (ctx->mp_result == 1) ? "VICTOIRE" : "DEFAITE";
        Color tc = (ctx->mp_result == 1) ? (Color){240, 200, 60, 255}
                                         : (Color){220, 70, 60, 255};
        int tw = mtxt(t, 40);
        dtxt(t, cx - tw/2, cy - 46, 40, tc);
        const char *sub = (ctx->mp_result == 1) ? "L'adversaire est tombe."
                                                : "Vos bases sont tombees.";
        int sw = mtxt(sub, 12);
        dtxt(sub, cx - sw/2, cy + 16, 12, (Color){200, 180, 140, 255});
        const char *hint = "Clic ou ESPACE pour revenir au menu";
        int hw = mtxt(hint, 10);
        dtxt(hint, cx - hw/2, cy + 44, 10, (Color){140, 120, 80, 255});
    }
}

static int game_do_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;

    ClearBackground(theme_get(gs->map.theme)->palette.bg);

    /* Décale la carte horizontalement pour centrer dans le canvas large.
       + secousse caméra (jus) : décalage purement visuel (le mappage souris
       reste sur g_map_x_off non secoué). */
    Camera2D map_cam = {0};
    map_cam.offset = (Vector2){(float)g_map_x_off + fx_shake_dx(),
                               fx_shake_dy()};
    map_cam.zoom   = g_map_render_scale;
    BeginMode2D(map_cam);
        render_map(&gs->map);
        tile_art_draw_paths(&gs->map);    // routes connectées (remplace les traits)
        tile_art_draw_spawns(&gs->map);   // portails d'invasion
        render_spawn_exclusion_zones(&gs->map);
        render_bases(&gs->map);
        render_deposits(&gs->map);
        render_dropped_mats(gs->dropped_mats, gs->dropped_mat_count);
        render_towers(&gs->towers);
        render_units(&gs->units);
        render_enemies(&gs->enemies);
        render_projectiles(&gs->towers);
        fx_render_world();   // particules + popups d'or (jus)
    EndMode2D();

    ui_render(&gs->ui, gs);

    /* ── Bannière (mode de jeu + infos de carte) ─────────────── */
    /* • Démarrage : grand overlay centré sur la carte, fade-in 0.3 s,
         plein 4.2 s, fade-out 0.5 s  (banner_timer compte de 5 → 0).
       • Pause      : bandeau compact fixe en haut de la carte.      */
    /* Variables de titre hoistées au scope fonction : réutilisées par le
       bandeau de pause dessiné plus bas (après le voile du menu pause). */
    char big_title[80]  = {0};   /* titre principal (gros, centré)  */
    char line1    [120] = {0};   /* sous-titre / détails            */
    char line2    [80]  = {0};   /* info secondaire                 */
    Color ctitle = {255, 210,  80, 255};
    Color c1     = {200, 170,  60, 255};
    Color c2     = {150, 215, 110, 255};
    {
        if (gs->is_campaign && ctx->interlude == INTER_NONE) {
            const ActData *ad = campaign_act_get(gs->campaign_stage);
            const char *mut = campaign_mutator_name(campaign_mutator_for_stage(
                                  campaign_difficulty_stage(gs->campaign_stage)));
            snprintf(big_title, sizeof(big_title), "%s", ad->title);
            /* line1 : chapitre/acte + mutateur signature s'il y en a un */
            if (mut)
                snprintf(line1, sizeof(line1), "Ch.%d Acte %d   ·   %s",
                         ad->chapter + 1, ad->act + 1, mut);
            else
                snprintf(line1, sizeof(line1), "Campagne  —  Ch.%d  Acte %d",
                         ad->chapter + 1, ad->act + 1);
            snprintf(line2, sizeof(line2), "Objectif : %s",
                     ad->objective.description);
            ctitle = (Color){255, 220, 100, 255};
            c1     = (Color){180, 160,  80, 255};
        } else if (gs->is_endless) {
            int score = (int)((float)gs->wave_manager.number
                              * gs->endless_multiplier * 10.0f);
            snprintf(big_title, sizeof(big_title), "ENDLESS");
            snprintf(line1, sizeof(line1), "Serie %d  ·  x%.1f  ·  Score %d",
                     gs->endless_series + 1, gs->endless_multiplier, score);
            int next_extr = 10 - (gs->wave_manager.number % 10);
            if (next_extr == 10 && gs->wave_manager.number > 0) next_extr = 0;
            if (next_extr > 0)
                snprintf(line2, sizeof(line2),
                         "Extraction dans %d vague(s)", next_extr);
            ctitle = (Color){160, 225, 255, 255};
            c1     = (Color){120, 185, 225, 255};
            c2     = (Color){ 90, 155, 200, 255};
        } else if (gs->is_custom) {
            const Theme *th = theme_get(gs->map.theme);
            snprintf(big_title, sizeof(big_title), "CUSTOM GAME");
            snprintf(line1, sizeof(line1), "%s  ·  %d spawn(s)  ·  %d base(s)",
                     th->name, gs->enemy_paths.count, gs->map.base_count);
            ctitle = (Color){200, 130, 255, 255};
            c1     = (Color){165, 100, 220, 255};
        } else {
            const Theme *th = theme_get(gs->map.theme);
            snprintf(big_title, sizeof(big_title), "ARCADE");
            snprintf(line1, sizeof(line1), "%s", th->name);
        }

        /* ── A) Overlay centré — intro au démarrage ───────────── */
        if (!ctx->menu.paused && ctx->banner_timer > 0.0f) {
            float t  = ctx->banner_timer;   /* 5.0 → 0.0 */
            float af;
            if      (t > 4.7f) af = (5.0f - t) / 0.3f;  /* fade-in  0.3 s */
            else if (t < 0.5f) af = t / 0.5f;             /* fade-out 0.5 s */
            else               af = 1.0f;
            if (af < 0.0f) af = 0.0f;
            if (af > 1.0f) af = 1.0f;
            unsigned char a = (unsigned char)(af * 255.0f);

            if (a > 0 && big_title[0]) {
                const int FS_T = 26, FS_1 = 13, FS_2 = 10;
                const int PAD_H = 26, PAD_V = 18, GAP = 9;

                int cx = g_map_x_off + g_canvas_virt_w_base / 2;
                int cy = (g_canvas_virt_h - UI_HUD_HEIGHT) / 2;

                int twt = mtxt(big_title, FS_T);
                int tw1 = line1[0] ? mtxt(line1, FS_1) : 0;
                int tw2 = line2[0] ? mtxt(line2, FS_2) : 0;
                int mxw = twt > tw1 ? twt : tw1;
                if (tw2 > mxw) mxw = tw2;

                int ch = fh(FS_T);
                if (line1[0]) ch += GAP + 1 + GAP + fh(FS_1); /* +1 = séparateur */
                if (line2[0]) ch += GAP + fh(FS_2);

                int pw = mxw + PAD_H * 2;
                int ph = ch  + PAD_V * 2;
                float px = (float)(cx - pw / 2);
                float py = (float)(cy - ph / 2);

                /* Fond + cadre à équerres (identité militaire) */
                Rectangle _br = {px, py, (float)pw, (float)ph};
                DrawRectangleRounded(_br, 0.16f, 6,
                    (Color){3, 2, 0, (unsigned char)((int)a * 225 / 255)});
                DrawRectangleRoundedLinesEx(_br, 0.16f, 6, 2.0f,
                    (Color){(unsigned char)(ctitle.r/2), (unsigned char)(ctitle.g/2),
                            (unsigned char)(ctitle.b/2),
                            (unsigned char)((int)a * 190 / 255)});
                /* Filet d'accent supérieur */
                DrawRectangle((int)px + 10, (int)py + 6, pw - 20, 2,
                    (Color){ctitle.r, ctitle.g, ctitle.b,
                            (unsigned char)((int)a * 200 / 255)});
                /* Équerres d'angle */
                {
                    int L = 10;
                    unsigned char ca = (unsigned char)((int)a * 220 / 255);
                    Color cc = {ctitle.r, ctitle.g, ctitle.b, ca};
                    int x0 = (int)px + 4, y0 = (int)py + 4;
                    int x1 = (int)px + pw - 4, y1 = (int)py + ph - 4;
                    DrawRectangle(x0,     y0,     L, 2, cc); DrawRectangle(x0,     y0,     2, L, cc);
                    DrawRectangle(x1 - L, y0,     L, 2, cc); DrawRectangle(x1 - 2, y0,     2, L, cc);
                    DrawRectangle(x0,     y1 - 2, L, 2, cc); DrawRectangle(x0,     y1 - L, 2, L, cc);
                    DrawRectangle(x1 - L, y1 - 2, L, 2, cc); DrawRectangle(x1 - 2, y1 - L, 2, L, cc);
                }

                int ly = (int)py + PAD_V;

                /* Titre principal */
                dtxt(big_title, cx - twt / 2, ly, FS_T,
                     (Color){ctitle.r, ctitle.g, ctitle.b, a});
                ly += fh(FS_T);

                /* Séparateur fin + line1 */
                if (line1[0]) {
                    ly += GAP;
                    DrawLineEx(
                        (Vector2){px + 12.0f, (float)ly},
                        (Vector2){px + (float)pw - 12.0f, (float)ly},
                        1.0f,
                        (Color){ctitle.r / 3, ctitle.g / 3, ctitle.b / 3,
                                (unsigned char)((int)a * 150 / 255)});
                    ly += 1 + GAP;
                    dtxt(line1, cx - tw1 / 2, ly, FS_1,
                         (Color){c1.r, c1.g, c1.b, a});
                    ly += fh(FS_1);
                }

                /* line2 */
                if (line2[0]) {
                    ly += GAP;
                    dtxt(line2, cx - tw2 / 2, ly, FS_2,
                         (Color){c2.r, c2.g, c2.b,
                                 (unsigned char)((int)a * 200 / 255)});
                }
            }
        }

        /* Le bandeau de pause est dessiné plus bas, APRÈS le voile du
           menu pause, pour qu'il reste bien lisible (voir game_do_render). */
    }

    /* ── Interludes ───────────────────────────────────────────── */
    if (gs->phase == PHASE_GAMEOVER)
        interlude_render_gameover(gs, g_canvas_virt_w, g_canvas_virt_h);

    if (ctx->interlude == INTER_DIALOG_BEFORE) {
        interlude_render_dialog_before(campaign_act_get(gs->campaign_stage),
                                       gs->campaign_stage, gs->campaign_flags,
                                       g_canvas_virt_w, g_canvas_virt_h);
        /* Input ignoré le frame d'entrée (anti-bleed depuis butin/boutique). */
        if (ctx->interlude == ctx->interlude_prev &&
            (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
            ctx->interlude = INTER_NONE;
    }
    if (ctx->interlude == INTER_DIALOG_AFTER) {
        interlude_render_dialog_after(campaign_act_get(gs->campaign_stage),
                                      ctx->interlude_stars,
                                      ctx->interlude_scrap, g_canvas_virt_w, g_canvas_virt_h,
                                      gs->campaign_stage, gs->campaign_flags);
    }
    if (ctx->interlude == INTER_EXTRACT) {
        /* Souris en coordonnées virtuelles (offset + échelle uniforme) */
        Vector2 vm = virt_mouse();
        interlude_render_extract(gs, g_canvas_virt_w, g_canvas_virt_h, vm);
    }
    if (ctx->interlude == INTER_DRAFT)
        interlude_render_draft(&gs->run, virt_mouse(),
                               g_canvas_virt_w, g_canvas_virt_h);
    if (ctx->interlude == INTER_SHOP)
        interlude_render_shop(&gs->run, SHOP_REROLL_COST, virt_mouse(),
                              g_canvas_virt_w, g_canvas_virt_h);

    /* ── Menu pause superposé ─────────────────────────────────── */
    if (ctx->menu.paused) {
        MenuAction pact = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                              g_canvas_virt_w, g_canvas_virt_h);

        /* ── Bandeau d'info en haut — dessiné APRÈS le voile du menu ──
           pour rester parfaitement lisible (avant il était noyé sous le
           rectangle d'assombrissement plein écran du menu pause).        */
        if (big_title[0]) {
            char compact[256];   // assez large pour "titre — sous-titre"
            if (line1[0])
                snprintf(compact, sizeof(compact), "%s  —  %s", big_title, line1);
            else
                snprintf(compact, sizeof(compact), "%s", big_title);

            int fs = 12;
            int cx = g_map_x_off + g_canvas_virt_w_base / 2;
            int tw = mtxt(compact, fs);
            int bw = tw + 48;          /* place pour l'icône pause + marges */
            int bx = cx - bw / 2;
            int by = 10;
            int bh = fh(fs) + 16;
            Rectangle pr = {(float)bx, (float)by, (float)bw, (float)bh};

            /* Panneau opaque + filet d'accent + bordure vive */
            DrawRectangleRounded(pr, 0.35f, 6, (Color){12, 8, 3, 248});
            DrawRectangle(bx + 8, by + 5, bw - 16, 2,
                          (Color){ctitle.r, ctitle.g, ctitle.b, 235});
            DrawRectangleRoundedLinesEx(pr, 0.35f, 6, 2.0f,
                          (Color){ctitle.r, ctitle.g, ctitle.b, 240});

            /* Icône pause (deux barres) dans la marge gauche */
            int icy = by + bh/2 - 5;
            DrawRectangle(bx + 13, icy, 3, 10, (Color){ctitle.r, ctitle.g, ctitle.b, 240});
            DrawRectangle(bx + 19, icy, 3, 10, (Color){ctitle.r, ctitle.g, ctitle.b, 240});

            /* Texte clair et contrasté */
            dtxt(compact, cx - tw/2 + 12, by + (bh - fh(fs))/2, fs,
                 (Color){255, 244, 210, 255});

            /* line2 (objectif campagne, etc.) sous le bandeau */
            if (line2[0]) {
                int fs2 = 10;
                int tw2 = mtxt(line2, fs2);
                int by2 = by + bh + 5;
                Rectangle pr2 = {(float)(cx - tw2/2 - 10), (float)by2,
                                 (float)(tw2 + 20), (float)(fh(fs2) + 8)};
                DrawRectangleRounded(pr2, 0.4f, 5, (Color){12, 8, 3, 240});
                DrawRectangleRoundedLinesEx(pr2, 0.4f, 5, 1.2f,
                              (Color){c2.r, c2.g, c2.b, 170});
                dtxt(line2, cx - tw2/2, by2 + 4, fs2,
                     (Color){c2.r, c2.g, c2.b, 240});
            }
        }
        /* Sauvegarde rapide (sans quitter) */
        if (pact.save_and_quit == 2 && ctx->active_slot >= 0) {
            if (ctx->gs.is_campaign)
                campaign_save_write(&ctx->gs, ctx->active_slot,
                                    (int)ctx->interlude,
                                    ctx->interlude_scrap,
                                    ctx->interlude_stars,
                                    ctx->interlude_last);
            else
                save_write(&ctx->gs, ctx->active_slot);
        }
        /* Sauvegarder et retour au menu */
        if (pact.save_and_quit == 1) {
            if (ctx->active_slot >= 0) {
                if (ctx->gs.is_campaign)
                    campaign_save_write(&ctx->gs, ctx->active_slot,
                                        (int)ctx->interlude,
                                        ctx->interlude_scrap,
                                        ctx->interlude_stars,
                                        ctx->interlude_last);
                else
                    save_write(&ctx->gs, ctx->active_slot);
            }
            menu_refresh_slots(&ctx->menu);
            ctx->menu.paused = 0;
            ctx->menu.screen = MENU_TITLE;
            ctx->screen      = SCREEN_MENU;
            ctx->interlude   = INTER_NONE;
            audio_play_menu_music();
        }
        /* Quitter l'application — sauvegarde d'urgence */
        if (pact.quit_app) {
            if (ctx->active_slot >= 0) {
                if (ctx->gs.is_campaign)
                    campaign_save_write(&ctx->gs, ctx->active_slot,
                                        (int)ctx->interlude,
                                        ctx->interlude_scrap,
                                        ctx->interlude_stars,
                                        ctx->interlude_last);
                else
                    save_write(&ctx->gs, ctx->active_slot);
            }
            return 0;   /* quitter l'application */
        }
        if (pact.toggle_fs == 1) {
            ToggleFullscreen();
            ctx->menu.opts.fullscreen = IsWindowFullscreen();
        }
        if (pact.toggle_fs == 2 && !IsWindowFullscreen())
            window_apply_size(ctx->menu.opts.win_width,
                              ctx->menu.opts.win_height);
    }

    mp_render_overlay(ctx);   // mini-HUD rival + résultat (par-dessus tout)
    return 1;
}

// ════════════════════════════════════════════════════════════════
// POINT D'ENTRÉE DE FRAME
// ════════════════════════════════════════════════════════════════
int app_update(AppContext *ctx, float dt) {
    /* Ajustement FPS si l'option a changé */
    int wfps = ctx->menu.opts.target_fps ? ctx->menu.opts.target_fps : 0;
    if (wfps != ctx->applied_fps) {
        SetTargetFPS(wfps);
        ctx->applied_fps = wfps;
    }

    /* Sauvegarde automatique des options quand on quitte l'écran Options */
    if (ctx->prev_menu_screen == MENU_OPTIONS &&
        ctx->menu.screen      != MENU_OPTIONS)
        opts_save(&ctx->menu.opts);
    ctx->prev_menu_screen = ctx->menu.screen;

    if (ctx->screen == SCREEN_MENU)
        return handle_menu(ctx);

    /* SCREEN_GAME */
    game_do_input(ctx);
    game_do_update(ctx, dt);
    mp_game_tick(ctx, dt);   // réseau multijoueur (no-op hors MP)

    /* Sync show_fps opts ↔ ui :
       - En jeu (non pausé) : [F] peut avoir changé ui → on met à jour opts pour persistance.
       - En pause          : le checkbox Options peut avoir changé opts → on l'applique à ui. */
    if (ctx->menu.paused)
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
    else
        ctx->menu.opts.show_fps = ctx->gs.ui.show_fps;

    if (ctx->mp_in_game) {
        mp_handle_result(ctx);          // partie MP : résultat → retour menu
    } else {
        game_handle_gameover(ctx);
        game_handle_campaign_end(ctx);
        game_handle_dialog_after(ctx);
        game_handle_draft(ctx);
        game_handle_shop(ctx);
        game_handle_extract(ctx);
    }
    int keep = game_do_render(ctx);
    /* Mémorise l'interlude pour le frame suivant (garde anti-bleed d'input). */
    ctx->interlude_prev = ctx->interlude;
    return keep;
}

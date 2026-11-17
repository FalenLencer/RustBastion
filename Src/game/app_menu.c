/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_menu.c ─ Écran menu : actions du menu → lancement des parties.
 *
 *  Extrait d'app.c (règle projet : fichiers < 500 lignes) ; fonction
 *  déplacée à l'identique, appelée par app_update().
 */

#include "app.h"
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
#include "../ui/story_scene.h"   /* briefing : consigne retenue */
#include "../ui/ui_palette.h"   /* UI_ACCENT (notification d'ordre) */
#include "campaign_data.h"
#include "../ui/ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "../map/theme.h"
#include "../combat/fx.h"     // effets de jus (particules, secousse, pops)
#include "app_tutorial.h"     // tutoriel guidé (module extrait)
#include "app_mp.h"           // sous-système multijoueur (module extrait)
#include "app_flow.h"         // fins d'acte/partie (module extrait)
#include "../net/net_upnp.h"  // UPNP_HOOK : ouverture auto du port (module isolé)
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════════════════
// ÉCRAN MENU
// ════════════════════════════════════════════════════════════════
int app_handle_menu(AppContext *ctx) {
    /* Réinitialise le zoom de carte au retour au menu */
    g_map_render_scale = 1.0f;
    g_map_zoom = 1.0f; g_map_pan_x = 0.0f; g_map_pan_y = 0.0f;   // reset zoom joueur
    ctx->tactical_pause = 0;   // en menu = jamais en pause tactique
    ctx->tutorial_active = 0;  // quitter vers le menu arrête le tutoriel
    ctx->show_help = 0;        // ferme l'aide en quittant vers le menu
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

    if (act.start_hero) {
        ctx->active_slot = -1;                   /* beta : pas de sauvegarde */
        game_init_arcade(&ctx->gs,
                         (ThemeID)GetRandomValue(0, THEME_COUNT - 1), -1);
        hero_start(ctx);                         /* capture curseur + mode   */
        ctx->menu.screen  = MENU_TITLE;
        ctx->screen       = SCREEN_GAME;
        ctx->banner_timer = 0.0f;                /* bannière 2D non rendue   */
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.start_tutorial) {
        tutorial_start(ctx);                     // carte guidée + vagues de Raiders
        ctx->menu.screen   = MENU_TITLE;
        ctx->screen        = SCREEN_GAME;
        ctx->banner_timer  = 0.0f;               // le tuto guide (pas de bannière)
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.start_campaign) {
        ctx->active_slot = act.new_slot;
        ctx->gs.campaign_order_seed = act.campaign_order_seed;
        game_init_campaign(&ctx->gs, ctx->gs.meta.campaigns_completed,
                           ctx->active_slot, act.campaign_order_seed,
                           act.start_campaign_act);
        game_campaign_set_challenge(&ctx->gs, act.campaign_challenge);
        if (ctx->gs.challenge_mode)
            ui_push_notif(&ctx->gs.ui, "MODE CHALLENGE ACTIF !",
                          (Color){235, 140, 50, 255});
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
        ctx->mp_active = 0; ctx->mp_via_relay = 0;
        int seed = GetRandomValue(1, 0x3FFFFFFF);
        ctx->mp_mode = act.mp_mode ? act.mp_mode : MP_COURSE;
        uint32_t rip; uint16_t rport;
        if (parse_relay_addr(ctx->menu.opts.relay_addr, &rip, &rport)) {
            // ── Via serveur relais : l'hôte s'y connecte aussi ──
            uint32_t room = (uint32_t)GetRandomValue(1, 0x7FFFFFFE);
            if (net_session_host_relay(&ctx->session, rip, rport, room,
                                       (uint32_t)seed, (uint8_t)ctx->mp_mode,
                                       ctx->menu.opts.player_name)) {
                ctx->mp_active = 1; ctx->mp_via_relay = 1;
            }
        } else if (net_session_host(&ctx->session, MP_PORT, (uint32_t)seed,
                                    (uint8_t)ctx->mp_mode, ctx->menu.opts.player_name)) {
            ctx->mp_active = 1;
            // IP par défaut = celle détectée (encodée dans le code) ; modifiable.
            net_ip_to_str(net_local_ip_be(), ctx->menu.mp_host_ip);
        }
    }
    if (act.mp_join) {
        net_session_close(&ctx->session);
        ctx->mp_active = 0; ctx->mp_via_relay = 0;
        // Un code relais (10 octets) se distingue d'un code direct (6 octets) :
        // on tente d'abord le relais, sinon connexion directe.
        if (net_session_parse_relay_code(act.mp_code, NULL, NULL, NULL)) {
            if (net_session_join_relay(&ctx->session, act.mp_code, ctx->menu.opts.player_name)) {
                ctx->mp_active = 1; ctx->mp_via_relay = 1;
            }
        } else if (net_session_join(&ctx->session, act.mp_code, ctx->menu.opts.player_name)) {
            ctx->mp_active = 1;
        }
    }
    if (act.mp_host || act.mp_join) opts_save(&ctx->menu.opts);   // persiste le pseudo
    if (ctx->mp_active && act.mp_ready)
        net_session_set_ready(&ctx->session, !ctx->session.my_ready);
    if (ctx->mp_active && act.mp_start) {
        ctx->session.seed = (uint32_t)GetRandomValue(1, 0x3FFFFFFE);  // carte fraîche
        // Joint la config de partie (carte/difficulté) + rôle Asym au START.
        unsigned char extra[sizeof(CustomConfig) + 1];
        memcpy(extra, &ctx->menu.mp_cfg, sizeof(CustomConfig));
        extra[sizeof(CustomConfig)] = (unsigned char)(ctx->menu.mp_host_invader ? 1 : 0);
        net_session_start(&ctx->session, extra, (int)sizeof(extra));
    }
    // UPNP_HOOK : autorise le pare-feu (UAC) PUIS ouvre le port (bloquant ~6 s max).
    if (act.mp_upnp) {
        int fw = net_upnp_request_firewall(MP_PORT);  // invite UAC sous Windows
        UpnpResult r;
        net_upnp_map_tcp(MP_PORT, &r);
        if (r.mapped && r.external_ip_be && r.external_public)
            net_ip_to_str(r.external_ip_be, ctx->menu.mp_host_ip);  // code → IP publique
        ctx->menu.mp_upnp_ok = (r.mapped && r.external_public);
        snprintf(ctx->menu.mp_upnp_msg, sizeof(ctx->menu.mp_upnp_msg),
                 "%s %.130s", fw ? "Pare-feu OK." : "Pare-feu KO (refusé/AV ?).",
                 r.message);
    }
    if (act.mp_leave) {
        net_upnp_unmap_tcp(MP_PORT);     // UPNP_HOOK : retire la redirection (best effort)
        net_session_close(&ctx->session);
        ctx->mp_active = 0; ctx->mp_via_relay = 0;
        ctx->menu.mp_view.active = 0;
        ctx->menu.mp_upnp_msg[0] = '\0';
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
            v->peer_connected = (s->state == SESS_LOBBY || s->state == SESS_INGAME)
                                && !s->peer_gone;
            v->my_ready       = s->my_ready;
            v->peer_ready     = s->peer_ready;
            v->started        = s->started;
            v->failed         = (s->state == SESS_FAILED) || s->peer_gone;
            v->via_relay      = ctx->mp_via_relay;
            // Code = fonction de l'IP hôte modifiable (LAN détectée par défaut,
            // ou IP publique si l'hôte a redirigé le port 47777).
            uint32_t hip;
            if (s->is_host && net_str_to_ip(ctx->menu.mp_host_ip, &hip))
                net_session_make_code(hip, MP_PORT, v->code);
            else
                snprintf(v->code, sizeof(v->code), "%s", s->code);
            snprintf(v->peer_name, sizeof(v->peer_name), "%s", s->peer_name);
            if (s->mode) { ctx->mp_mode = s->mode; ctx->menu.mp_mode = s->mode; }
            if (s->started) mp_launch_game(ctx);
        } else {
            // Pas de session active affichée → ferme une session orpheline et
            // VIDE la vue (sinon le lobby garderait un état "connecté" périmé
            // après une partie → impossible de re-saisir un code).
            if (ctx->mp_active && !in_mp && ctx->screen == SCREEN_MENU) {
                net_session_close(&ctx->session);
                ctx->mp_active = 0;
            }
            memset(&ctx->menu.mp_view, 0, sizeof(ctx->menu.mp_view));
        }
    }

    return 1;
}

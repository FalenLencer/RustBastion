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
#include "../net/net_upnp.h"  // UPNP_HOOK : ouverture auto du port (module isolé)
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Multijoueur
#define MP_PORT             47777
#define MP_STATUS_INTERVAL  0.4f    // période d'envoi du statut (s)

// Analyse "IP:port" (port optionnel → MP_PORT). 1 = OK. (mode serveur relais)
static int parse_relay_addr(const char *s, uint32_t *ip, uint16_t *port) {
    if (!s || !s[0]) return 0;
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%s", s);
    int p = MP_PORT;
    char *colon = strchr(tmp, ':');
    if (colon) { *colon = '\0'; p = atoi(colon + 1); }
    if (p <= 0 || p > 65535) p = MP_PORT;
    if (!net_str_to_ip(tmp, ip)) return 0;
    *port = (uint16_t)p;
    return 1;
}

// Duel : monnaie de sabotage (par kill) + ennemis envoyables (type, coût, touche).
#define DUEL_SABOTAGE_PER_KILL  1
// Asym : le défenseur gagne s'il survit N vagues ; l'envahisseur gagne du budget
// avec le temps.
#define ASYM_SURVIVE_WAVES   15
#define ASYM_BUDGET_PERIOD   0.8f   // +1 de budget toutes les 0.8 s
// Co-op : survie commune ; si l'un des deux boards tombe, tous perdent.
#define COOP_WAVES           20     // vagues à tenir ensemble pour gagner
#define COOP_AID_GOLD        50     // or transféré au partenaire (touche G)
// Ennemi envoyable : type + coût + nom. BOUTONS cliquables (pas de touche →
// fonctionne quel que soit le clavier, AZERTY inclus).
typedef struct { int type; int cost; const char *name; } MpSend;
#define DUEL_SEND_COUNT 4
static const MpSend DUEL_SENDS[DUEL_SEND_COUNT] = {
    { ENEMY_RAIDER,   3, "Raider" },
    { ENEMY_RUNNER,   6, "Runner" },
    { ENEMY_BRUTE,   14, "Brute"  },
    { ENEMY_VEHICLE, 30, "Blinde" },
};
// Asym envahisseur : roster COMPLET (tous les types) + budget par vague.
// Coûts croissants selon la dangerosité ; l'envahisseur compose librement.
#define INV_ROSTER_COUNT 10
static const MpSend INV_ROSTER[INV_ROSTER_COUNT] = {
    { ENEMY_RAIDER,       3, "Raider"  },
    { ENEMY_RUNNER,       6, "Runner"  },
    { ENEMY_MUTANT,      10, "Mutant"  },
    { ENEMY_PATHBREAKER, 12, "Perceur" },
    { ENEMY_BRUTE,       14, "Brute"   },
    { ENEMY_GHOST,       16, "Spectre" },
    { ENEMY_HEALER,      18, "Soigneur"},
    { ENEMY_HUNTER,      20, "Chasseur"},
    { ENEMY_ARTILLERY,   24, "Artilleur"},
    { ENEMY_VEHICLE,     30, "Blinde"  },
};
#define INV_WAVE_BUDGET_BASE 80
#define INV_WAVE_BUDGET_STEP 30
#define INV_WAVE_BUDGET(n)   (INV_WAVE_BUDGET_BASE + (n) * INV_WAVE_BUDGET_STEP)
#define INV_WAVE_COOLDOWN    7.0f   // délai imposé entre deux vagues envoyées (s)

// ════════════════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════════════════
void app_init(AppContext *ctx) {
    memset(ctx, 0, sizeof(AppContext));

    game_state_init(&ctx->gs);

    AppOptions opts = {
        .win_width     = VIRT_W,
        .win_height    = VIRT_H,
        .target_fps    = 60,
        .master_volume = 50,
        .music_volume  = 50,
        .sfx_volume    = 50,
        .fx_effects    = 1,
        .player_name   = "Joueur",
    };
    opts_load(&opts);
    if (!opts.player_name[0]) snprintf(opts.player_name, sizeof(opts.player_name), "Joueur");
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
    ctx->mp_sabotage     = 0;
    ctx->mp_prev_kills   = 0;
    ctx->mp_budget_t     = 0.0f;
    ctx->mp_inject_acc    = 0.0f;
    ctx->mp_wave_num      = 0;
    ctx->mp_wave_budget   = INV_WAVE_BUDGET(0);
    ctx->mp_wave_cooldown = 0.0f;
    memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
    // ── Config de partie jointe au START (carte/difficulté + rôle Asym) ──
    CustomConfig cfg; memset(&cfg, 0, sizeof(cfg));
    int host_invader = 0;
    if (s->start_extra_len >= (int)sizeof(CustomConfig)) {
        memcpy(&cfg, s->start_extra, sizeof(CustomConfig));
        if (s->start_extra_len > (int)sizeof(CustomConfig))
            host_invader = s->start_extra[sizeof(CustomConfig)] ? 1 : 0;
    }
    // Asym : qui joue l'ENVAHISSEUR (sim figée) selon le choix de l'hôte.
    ctx->mp_invader      = (s->mode == MP_ASYM) && (host_invader ? s->is_host : !s->is_host);
    memset(&ctx->mp_peer, 0, sizeof(ctx->mp_peer));
    // Plateau partagé + panneaux déplaçables : reset.
    ctx->mp_board_tn = 0; ctx->mp_board_un = 0; ctx->mp_board_timer = 0.0f;
    ctx->mp_pos_rival = (Vector2){-1, -1};   // <0 → placement par défaut au 1er rendu
    ctx->mp_pos_duel  = (Vector2){-1, -1};
    ctx->mp_drag      = 0;

    // MÊME seed → même carte (équité). Thème aléatoire résolu de façon
    // déterministe depuis le seed partagé. La config (spawns/minerais/taille/
    // difficulté) choisie par l'hôte s'applique aux DEUX joueurs.
    unsigned int seed = s->seed ? s->seed : 1u;
    if (cfg.theme < 0 || cfg.theme >= THEME_COUNT)
        cfg.theme = (int)(seed % THEME_COUNT);
    SetRandomSeed(seed);
    ctx->active_slot = -1;                 // pas de sauvegarde en multijoueur
    game_init_custom(&ctx->gs, &cfg);

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
    g_map_zoom = 1.0f; g_map_pan_x = 0.0f; g_map_pan_y = 0.0f;   // reset zoom joueur
    ctx->tactical_pause = 0;   // en menu = jamais en pause tactique
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
                 "%s %.130s", fw ? "Pare-feu OK." : "Pare-feu KO (refuse/AV ?).",
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

// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — MISE À JOUR
// ════════════════════════════════════════════════════════════════
// Zoom carte (molette, centré sur le curseur) + déplacement (bouton du milieu).
// Tout passe par g_map_zoom/pan → le rendu ET le mappage souris restent alignés.
static void game_zoom_input(AppContext *ctx) {
    if (ctx->menu.paused || ctx->interlude != INTER_NONE) return;
    Vector2 m = virt_mouse();
    float map_area_h = (float)(g_canvas_virt_h - UI_HUD_HEIGHT);

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && m.y >= 0 && m.y < map_area_h) {
        Vector2 w = map_screen_to_world(m);                 // point monde sous le curseur
        float nz = g_map_zoom * (wheel > 0.0f ? 1.15f : 1.0f / 1.15f);
        if (nz < 1.0f) nz = 1.0f;
        if (nz > 3.0f) nz = 3.0f;
        g_map_zoom = nz;
        float S = map_eff_scale();
        g_map_pan_x = m.x - w.x * S - (float)g_map_x_off;   // garde le point fixe sous le curseur
        g_map_pan_y = m.y - w.y * S;
    }

    // Déplacement à la molette enfoncée (bouton du milieu).
    static int panning = 0;
    static Vector2 grab, start;
    if (g_map_zoom > 1.0f && IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        panning = 1; grab = m; start = (Vector2){ g_map_pan_x, g_map_pan_y };
    }
    if (panning && IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        g_map_pan_x = start.x + (m.x - grab.x);
        g_map_pan_y = start.y + (m.y - grab.y);
    } else {
        panning = 0;
    }

    // Clamp : la carte zoomée reste plaquée sur la zone de jeu (pas de vide).
    if (g_map_zoom <= 1.0001f) {
        g_map_zoom = 1.0f; g_map_pan_x = 0.0f; g_map_pan_y = 0.0f;
    } else {
        float S  = map_eff_scale();
        float W  = (float)(ctx->gs.map.w * TILE_SIZE) * S;
        float Hh = (float)(ctx->gs.map.h * TILE_SIZE) * S;
        float ov_x = W  - (float)g_canvas_virt_w_base; if (ov_x < 0.0f) ov_x = 0.0f;
        float ov_y = Hh - map_area_h;                  if (ov_y < 0.0f) ov_y = 0.0f;
        if (g_map_pan_x >  0.0f)  g_map_pan_x =  0.0f;
        if (g_map_pan_x < -ov_x)  g_map_pan_x = -ov_x;
        if (g_map_pan_y >  0.0f)  g_map_pan_y =  0.0f;
        if (g_map_pan_y < -ov_y)  g_map_pan_y = -ov_y;
    }
}

static void game_do_input(AppContext *ctx) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        ctx->menu.paused ^= 1;
        ctx->menu.screen  = ctx->menu.paused ? MENU_PAUSE : MENU_TITLE;
        ctx->tactical_pause = 0;   // le menu pause remplace la pause tactique
    }
    // Pause TACTIQUE (solo) : ESPACE gèle la simulation mais on peut continuer à
    // placer/vendre/déplacer pour réfléchir. Désactivée en multijoueur (équité).
    if (IsKeyPressed(KEY_SPACE) && !ctx->menu.paused &&
        ctx->gs.ui.disc_count == 0 && !ctx->mp_in_game &&
        ctx->interlude == INTER_NONE)
        ctx->tactical_pause ^= 1;
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
    // Pause tactique : on a pu agir (ui_update ci-dessus) mais la SIMULATION ne
    // s'avance pas → temps de réfléchir/placer.
    if (ctx->tactical_pause) return;
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

// Fait apparaître un ennemi injecté par l'adversaire sur un portail. Les
// injections rapprochées sont ÉTALÉES (mp_inject_acc) → effet de vague plutôt
// qu'un bloc instantané.
static void mp_spawn_injected(AppContext *ctx, int etype) {
    GameState *gs = &ctx->gs;
    if (etype < 0 || etype >= ENEMY_TYPE_COUNT) return;
    int path = -1;
    for (int p = 0; p < gs->enemy_paths.count; p++)
        if (gs->enemy_paths.paths[p].found) { path = p; break; }
    if (path < 0) return;
    const Theme *th = theme_get(gs->map.theme);
    float delay = ctx->mp_inject_acc;
    ctx->mp_inject_acc += 0.45f;
    if (ctx->mp_inject_acc > 12.0f) ctx->mp_inject_acc = 12.0f;
    enemy_spawn(&gs->enemies, (EnemyType)etype, path, &gs->enemy_paths,
                delay, gs->wave_manager.scale,
                th->enemy_speed_mult * gs->wave_manager.speed_mult);
}

// Rectangle du bouton d'envoi Duel n°i (bandeau haut-centre).
// Dimensions du panneau Duel (grip de glissement + rangée de boutons).
#define MP_DUEL_BARW  (DUEL_SEND_COUNT*100 + (DUEL_SEND_COUNT-1)*8)  // 424
#define MP_DUEL_GRIP_H 18
#define MP_RIVAL_W    240
#define MP_RIVAL_H     34

static Rectangle mp_duel_btn_rect(const AppContext *ctx, int i) {
    int bw = 100, bh = 30, gap = 8;
    float bx = ctx->mp_pos_duel.x;
    float by = ctx->mp_pos_duel.y + MP_DUEL_GRIP_H + 2;   // sous le grip
    return (Rectangle){ bx + (float)(i * (bw + gap)), by, (float)bw, (float)bh };
}

// Duel : clic sur un bouton d'envoi (traité AVANT l'input HUD → bloque le
// placement de tour sous le bouton via ui.mp_block_click).
static void mp_duel_send_input(AppContext *ctx) {
    if (ctx->mp_mode != MP_DUEL || !ctx->mp_in_game ||
        ctx->mp_result != 0 || ctx->menu.paused || ctx->mp_drag) return;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    Vector2 vm = virt_mouse();
    for (int i = 0; i < DUEL_SEND_COUNT; i++) {
        if (CheckCollisionPointRec(vm, mp_duel_btn_rect(ctx, i))) {
            if (ctx->mp_sabotage >= DUEL_SENDS[i].cost) {
                ctx->mp_sabotage -= DUEL_SENDS[i].cost;
                uint8_t t = (uint8_t)DUEL_SENDS[i].type;
                net_session_send(&ctx->session, NMSG_SEND_ENEMY, &t, 1);
                ui_push_notif(&ctx->gs.ui, TextFormat("%s envoye !", DUEL_SENDS[i].name),
                              (Color){200, 90, 200, 255});
            } else {
                ui_push_notif(&ctx->gs.ui, "Sabotage insuffisant", (Color){231, 76, 60, 255});
            }
            ctx->gs.ui.mp_block_click = 1;   // ne pas placer de tour sous le bouton
            return;
        }
    }
}

// Glissement des panneaux flottants MP (HUD rival + barre Duel). Traité AVANT
// l'input HUD pour ne pas placer de tour en déplaçant un panneau.
static void mp_panels_input(AppContext *ctx) {
    if (!ctx->mp_in_game || ctx->mp_invader || ctx->mp_result != 0 || ctx->menu.paused) return;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;
    if (ctx->mp_pos_rival.x < 0) ctx->mp_pos_rival = (Vector2){ (float)(base_cx - MP_RIVAL_W/2), 4.0f };
    if (ctx->mp_pos_duel.x  < 0) ctx->mp_pos_duel  = (Vector2){ (float)(base_cx - MP_DUEL_BARW/2), 44.0f };

    Vector2 m = virt_mouse();
    Rectangle rival = { ctx->mp_pos_rival.x, ctx->mp_pos_rival.y, MP_RIVAL_W, MP_RIVAL_H };
    Rectangle dgrip = { ctx->mp_pos_duel.x,  ctx->mp_pos_duel.y,  MP_DUEL_BARW, MP_DUEL_GRIP_H };

    if (ctx->mp_drag == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, rival)) {
            ctx->mp_drag = 1;
            ctx->mp_drag_grab = (Vector2){ m.x - ctx->mp_pos_rival.x, m.y - ctx->mp_pos_rival.y };
        } else if (ctx->mp_mode == MP_DUEL && CheckCollisionPointRec(m, dgrip)) {
            ctx->mp_drag = 2;
            ctx->mp_drag_grab = (Vector2){ m.x - ctx->mp_pos_duel.x, m.y - ctx->mp_pos_duel.y };
        }
    }
    if (ctx->mp_drag != 0) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 *p = (ctx->mp_drag == 1) ? &ctx->mp_pos_rival : &ctx->mp_pos_duel;
            p->x = m.x - ctx->mp_drag_grab.x;
            p->y = m.y - ctx->mp_drag_grab.y;
            if (p->x < 0) p->x = 0;
            if (p->y < 0) p->y = 0;
            if (p->x > g_canvas_virt_w - 40)  p->x = (float)(g_canvas_virt_w - 40);
            if (p->y > g_canvas_virt_h - 24)  p->y = (float)(g_canvas_virt_h - 24);
            ctx->gs.ui.mp_block_click = 1;   // ne pas placer de tour pendant le drag
        } else {
            ctx->mp_drag = 0;
        }
    }
}

// Fixe le résultat local (won=1 → VICTOIRE) et l'annonce au pair (octet = vainqueur).
static void mp_set_result(AppContext *ctx, int won) {
    if (ctx->mp_result != 0) return;
    ctx->mp_result = won ? 1 : 2;
    uint8_t w = won ? 1 : 0;
    net_session_send(&ctx->session, NMSG_GAMEOVER, &w, 1);
}

// Tick réseau en jeu (Course / Duel / Asym). Tourne même en pause (keep-alive).
static void mp_game_tick(AppContext *ctx, float dt) {
    if (!ctx->mp_active || !ctx->mp_in_game) return;
    NetSession *s = &ctx->session;
    int inv = ctx->mp_invader;
    net_session_update(s);

    NetMsgHeader h;
    unsigned char pl[NET_MAX_PAYLOAD];
    while (net_session_recv(s, &h, pl, sizeof(pl))) {
        if (h.type == NMSG_STATUS && h.len >= (int)sizeof(NetStatus)) {
            memcpy(&ctx->mp_peer, pl, sizeof(NetStatus));
            ctx->mp_peer_valid = 1;
        } else if (h.type == NMSG_GAMEOVER) {
            int peer_won = (h.len >= 1) ? pl[0] : 0;
            if (ctx->mp_result == 0)
                ctx->mp_result = (ctx->mp_mode == MP_COOP)
                               ? (peer_won ? 1 : 2)    // co-op : sort partagé (adopté)
                               : (peer_won ? 2 : 1);   // versus : inversé
        } else if (h.type == NMSG_SEND_ENEMY && h.len >= 1 && !inv) {
            mp_spawn_injected(ctx, pl[0]);   // on subit l'envoi (défenseur / Duel)
        } else if (h.type == NMSG_AID && h.len >= 4) {
            int32_t g; memcpy(&g, pl, 4);
            if (g > 0 && g < 100000) {       // co-op : or reçu du partenaire
                ctx->gs.gold += g;
                ui_push_notif(&ctx->gs.ui, "Or recu du partenaire !", (Color){120,210,120,255});
            }
        } else if (h.type == NMSG_BOARD && inv) {
            // Asym : snapshot du plateau du défenseur (tours + unités).
            int o = 0, len = h.len;
            ctx->mp_board_tn = 0; ctx->mp_board_un = 0;
            if (o < len) {
                int nt = pl[o++]; if (nt > 64) nt = 64;
                if (o + nt * 3 <= len) {
                    memcpy(ctx->mp_board_t, pl + o, nt * 3);
                    ctx->mp_board_tn = nt; o += nt * 3;
                }
            }
            if (o < len) {
                int nu = pl[o++]; if (nu > 32) nu = 32;
                if (o + nu * 3 <= len) {
                    memcpy(ctx->mp_board_u, pl + o, nu * 3);
                    ctx->mp_board_un = nu;
                }
            }
        }
    }
    if (s->peer_gone && ctx->mp_result == 0)
        ctx->mp_result = (ctx->mp_mode == MP_COOP) ? 2 : 1;  // co-op : sans partenaire = perdu

    // Étalement des ennemis injectés : décroît quand on n'en reçoit plus.
    if (ctx->mp_inject_acc > 0.0f) {
        ctx->mp_inject_acc -= dt;
        if (ctx->mp_inject_acc < 0.0f) ctx->mp_inject_acc = 0.0f;
    }
    // Asym envahisseur : cooldown entre vagues.
    if (inv && ctx->mp_wave_cooldown > 0.0f) {
        ctx->mp_wave_cooldown -= dt;
        if (ctx->mp_wave_cooldown < 0.0f) ctx->mp_wave_cooldown = 0.0f;
    }

    // Duel : sabotage gagné aux kills (les ENVOIS se font aux BOUTONS, cf.
    // mp_duel_send_input). L'envahisseur Asym compose ses vagues (mp_invader_render).
    if (ctx->mp_mode == MP_DUEL && ctx->mp_result == 0 && !ctx->menu.paused) {
        int dk = ctx->gs.kills - ctx->mp_prev_kills;
        if (dk > 0) {
            ctx->mp_sabotage += dk * DUEL_SABOTAGE_PER_KILL;
            ctx->mp_prev_kills = ctx->gs.kills;
        }
    }

    // ── Co-op : entraide (transfert d'or au partenaire, touche G) ──
    if (ctx->mp_mode == MP_COOP && !inv && ctx->mp_result == 0 && !ctx->menu.paused) {
        if (IsKeyPressed(KEY_G) && ctx->gs.gold >= COOP_AID_GOLD) {
            ctx->gs.gold -= COOP_AID_GOLD;
            int32_t g = COOP_AID_GOLD;
            net_session_send(s, NMSG_AID, &g, 4);
            ui_push_notif(&ctx->gs.ui, "+50 or envoye au partenaire", (Color){120,210,120,255});
        }
    }

    // ── Asym : le DÉFENSEUR streame son plateau à l'envahisseur (plateau partagé) ──
    if (ctx->mp_mode == MP_ASYM && !inv && !ctx->menu.paused) {
        ctx->mp_board_timer -= dt;
        if (ctx->mp_board_timer <= 0.0f) {
            ctx->mp_board_timer = 0.3f;
            unsigned char b[NET_MAX_PAYLOAD];
            int o = 0, nt = 0, nu = 0;
            int nt_pos = o++;                       // emplacement du compteur tours
            TowerPool *tp = &ctx->gs.towers;
            for (int i = 0; i < tp->tower_count && nt < 64; i++) {
                Tower *t = &tp->towers[i];
                if (!t->active) continue;
                b[o++] = (unsigned char)t->tile_x;
                b[o++] = (unsigned char)t->tile_y;
                b[o++] = (unsigned char)t->type;
                nt++;
            }
            b[nt_pos] = (unsigned char)nt;
            int nu_pos = o++;                       // emplacement du compteur unités
            UnitPool *up = &ctx->gs.units;
            for (int i = 0; i < up->count && nu < 32; i++) {
                Unit *u = &up->units[i];
                if (!u->active) continue;
                b[o++] = (unsigned char)((int)(u->x / TILE_SIZE));
                b[o++] = (unsigned char)((int)(u->y / TILE_SIZE));
                b[o++] = (unsigned char)u->type;
                nu++;
            }
            b[nu_pos] = (unsigned char)nu;
            net_session_send(s, NMSG_BOARD, b, o);
        }
    }

    // ── Statut : seuls les joueurs AVEC un board l'envoient (pas l'envahisseur) ──
    if (!inv && !ctx->menu.paused) {
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

    // ── Conditions de fin ──
    if (ctx->mp_result == 0 && !inv) {
        if (ctx->mp_mode == MP_COOP) {
            // L'HÔTE arbitre le sort partagé (les deux gagnent ou perdent ensemble).
            if (ctx->session.is_host) {
                int peer_dead = (ctx->mp_peer_valid && ctx->mp_peer.lives <= 0);
                if (ctx->gs.lives <= 0 || peer_dead)
                    mp_set_result(ctx, 0);                          // un board tombe → tous perdent
                else if (ctx->gs.wave_manager.number >= COOP_WAVES &&
                         ctx->mp_peer_valid && ctx->mp_peer.wave >= COOP_WAVES)
                    mp_set_result(ctx, 1);                          // survie commune → tous gagnent
            }
            // le client attend le verdict de l'hôte (GAMEOVER)
        } else if (ctx->gs.lives <= 0) {
            mp_set_result(ctx, 0);                                  // mes bases tombent → je perds
        } else if (ctx->mp_mode == MP_ASYM &&
                   ctx->gs.wave_manager.number >= ASYM_SURVIVE_WAVES) {
            mp_set_result(ctx, 1);                                  // défenseur survit → gagne
        }
    }
}

// Rectangles des boutons du bandeau de résultat (partagés rendu + input).
static Rectangle mp_result_btn(int which) {  // 0 = REMATCH, 1 = MENU
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
    return (which == 0) ? (Rectangle){(float)(cx - 220), (float)(cy + 12), 200.0f, 46.0f}
                        : (Rectangle){(float)(cx + 20),  (float)(cy + 12), 200.0f, 46.0f};
}

// Bandeau de résultat VICTOIRE / DEFAITE + boutons REMATCH / MENU.
static void mp_draw_result(AppContext *ctx) {
    if (ctx->mp_result == 0) return;
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
    DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0, 0, 0, 188});
    const char *t = (ctx->mp_result == 1) ? "VICTOIRE" : "DEFAITE";
    Color tc = (ctx->mp_result == 1) ? (Color){240, 200, 60, 255}
                                     : (Color){220, 70, 60, 255};
    int tw = mtxt(t, 40);
    dtxt(t, cx - tw/2, cy - 72, 40, tc);

    int can_rematch = net_session_connected(&ctx->session);
    Vector2 vm = virt_mouse();
    Rectangle rb = mp_result_btn(0), mb = mp_result_btn(1);
    int hov_r = can_rematch && CheckCollisionPointRec(vm, rb);
    int hov_m = CheckCollisionPointRec(vm, mb);

    DrawRectangleRounded(rb, 0.3f, 5, !can_rematch ? (Color){25,22,18,225}
                                    : hov_r ? (Color){40,120,60,245} : (Color){22,70,38,238});
    DrawRectangleRoundedLinesEx(rb, 0.3f, 5, hov_r ? 2.2f : 1.2f,
                                can_rematch ? (Color){80,200,110,235} : (Color){70,65,55,180});
    int rw = mtxt("REMATCH", 14);
    dtxt("REMATCH", (int)rb.x + 100 - rw/2, (int)rb.y + 23 - fh(14)/2, 14,
         can_rematch ? (Color){200,240,200,255} : (Color){100,95,85,255});

    DrawRectangleRounded(mb, 0.3f, 5, hov_m ? (Color){72,56,20,245} : (Color){46,36,12,238});
    DrawRectangleRoundedLinesEx(mb, 0.3f, 5, hov_m ? 2.2f : 1.2f, (Color){150,120,40,225});
    int mw = mtxt("MENU", 14);
    dtxt("MENU", (int)mb.x + 100 - mw/2, (int)mb.y + 23 - fh(14)/2, 14, (Color){230,200,140,255});

    const char *hint = can_rematch ? "REMATCH : rejouer ensemble  -  MENU : quitter"
                                   : "Adversaire deconnecte  -  MENU pour revenir";
    int hw = mtxt(hint, 9);
    dtxt(hint, cx - hw/2, (int)rb.y + 56, 9, (Color){150, 130, 90, 230});
}

// Clics sur le bandeau de résultat (REMATCH garde la session, MENU la ferme).
static void mp_handle_result(AppContext *ctx) {
    if (ctx->mp_result == 0) return;
    Vector2 vm = virt_mouse();
    int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    int can_rematch = net_session_connected(&ctx->session);

    if (click && can_rematch && CheckCollisionPointRec(vm, mp_result_btn(0))) {
        net_session_rematch(&ctx->session);      // retour au salon, session conservée
        ctx->mp_in_game = 0;
        ctx->mp_invader = 0;
        ctx->mp_result  = 0;
        ctx->menu.paused = 0;
        ctx->screen      = SCREEN_MENU;
        ctx->menu.screen = MENU_MP_LOBBY;
        audio_play_menu_music();
    } else if ((click && CheckCollisionPointRec(vm, mp_result_btn(1))) ||
               IsKeyPressed(KEY_ESCAPE)) {
        net_session_close(&ctx->session);
        ctx->mp_active   = 0;
        ctx->mp_in_game  = 0;
        ctx->mp_invader  = 0;
        ctx->menu.paused = 0;
        ctx->screen      = SCREEN_MENU;
        ctx->menu.screen = MENU_MP_HUB;
        audio_play_menu_music();
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
    if (ctx->mp_pos_rival.x < 0) ctx->mp_pos_rival = (Vector2){ (float)(base_cx - MP_RIVAL_W/2), 4.0f };
    if (ctx->mp_pos_duel.x  < 0) ctx->mp_pos_duel  = (Vector2){ (float)(base_cx - MP_DUEL_BARW/2), 44.0f };

    const char *nm = ctx->session.peer_name[0] ? ctx->session.peer_name : "Adversaire";
    char top[80], cmp[96];
    if (ctx->mp_mode == MP_ASYM) {           // défenseur : progression de survie
        snprintf(top, sizeof(top), "ENVAHISSEUR : %s", nm);
        snprintf(cmp, sizeof(cmp), "Survie  V%d / %d     PV %d",
                 ctx->gs.wave_manager.number, ASYM_SURVIVE_WAVES, ctx->gs.lives);
    } else if (ctx->mp_mode == MP_COOP) {    // partenaire : sort partagé + entraide
        snprintf(top, sizeof(top), "PARTENAIRE : %s   [G]=+50or", nm);
        if (ctx->mp_peer_valid)
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d  +  Lui V%d PV%d   (objectif V%d)",
                     ctx->gs.wave_manager.number, ctx->gs.lives,
                     ctx->mp_peer.wave, ctx->mp_peer.lives, COOP_WAVES);
        else
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui (connexion...)",
                     ctx->gs.wave_manager.number, ctx->gs.lives);
    } else {
        snprintf(top, sizeof(top), "RIVAL : %s", nm);
        if (ctx->mp_peer_valid)
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui V%d PV%d",
                     ctx->gs.wave_manager.number, ctx->gs.lives,
                     ctx->mp_peer.wave, ctx->mp_peer.lives);
        else
            snprintf(cmp, sizeof(cmp), "Vous V%d PV%d   |   Lui (connexion...)",
                     ctx->gs.wave_manager.number, ctx->gs.lives);
    }
    // Panneau rival déplaçable — largeur adaptée au texte (aucun débordement).
    int tw0 = mtxt(top, 9), cw0 = mtxt(cmp, 8);
    int twmax = tw0 > cw0 ? tw0 : cw0;
    int pw = twmax + 24; if (pw < MP_RIVAL_W) pw = MP_RIVAL_W;
    int ph = MP_RIVAL_H;
    int rx = (int)ctx->mp_pos_rival.x, ry = (int)ctx->mp_pos_rival.y;
    int rcx = rx + pw/2;
    Rectangle rpanel = {(float)rx,(float)ry,(float)pw,(float)ph};
    DrawRectangleRounded(rpanel, 0.22f, 5, (Color){10, 7, 3, 222});
    DrawRectangleRoundedLinesEx(rpanel, 0.22f, 5, 1.2f, (Color){70, 48, 16, 210});
    // 2 lignes empilées via fh() — pas de superposition.
    dtxt(top, rcx - tw0/2, ry + 3, 9, (Color){200, 180, 120, 255});
    dtxt(cmp, rcx - cw0/2, ry + 3 + fh(9), 8, (Color){150, 130, 80, 255});

    // Panneau Duel déplaçable : grip (sabotage) + BOUTONS d'envoi cliquables
    if (ctx->mp_mode == MP_DUEL && ctx->mp_result == 0) {
        int dx = (int)ctx->mp_pos_duel.x, dy = (int)ctx->mp_pos_duel.y;
        Rectangle grip = {(float)dx,(float)dy,(float)MP_DUEL_BARW,(float)MP_DUEL_GRIP_H};
        DrawRectangleRounded(grip, 0.4f, 4, (Color){26, 12, 32, 225});
        char sb[64];
        snprintf(sb, sizeof(sb), ":::  SABOTAGE %d  -  envoyez des ennemis", ctx->mp_sabotage);
        int sw = mtxt(sb, 8);
        dtxt(sb, dx + MP_DUEL_BARW/2 - sw/2, dy + (MP_DUEL_GRIP_H - fh(8))/2, 8,
             (Color){220, 140, 230, 255});
        Vector2 vm = virt_mouse();
        for (int i = 0; i < DUEL_SEND_COUNT; i++) {
            Rectangle r = mp_duel_btn_rect(ctx, i);
            int afford = (ctx->mp_sabotage >= DUEL_SENDS[i].cost);
            int hov    = afford && CheckCollisionPointRec(vm, r);
            Color bg  = !afford ? (Color){20, 16, 22, 225}
                      : hov     ? (Color){64, 30, 74, 240}
                                : (Color){30, 14, 38, 230};
            Color brd = afford ? (Color){175, 85, 195, 235} : (Color){70, 60, 75, 200};
            DrawRectangleRounded(r, 0.25f, 4, bg);
            DrawRectangleRoundedLinesEx(r, 0.25f, 4, hov ? 2.0f : 1.2f, brd);
            char b[28];
            snprintf(b, sizeof(b), "%s  -%d", DUEL_SENDS[i].name, DUEL_SENDS[i].cost);
            int bw2 = mtxt(b, 9);
            dtxt(b, (int)r.x + (int)r.width/2 - bw2/2, (int)r.y + (int)r.height/2 - fh(9)/2, 9,
                 afford ? (Color){210, 235, 180, 255} : (Color){110, 100, 110, 255});
        }
    }

    mp_draw_result(ctx);   // bandeau VICTOIRE / DEFAITE
}

// Écran de l'ENVAHISSEUR (Asym) : carte figée + panneau de commande + statut défenseur.
static void mp_invader_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;
    int cx = g_canvas_virt_w / 2;
    int base_cx = g_map_x_off + g_canvas_virt_w_base / 2;
    Vector2 vm = virt_mouse();
    int click = (ctx->mp_result == 0) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // ── Carte figée (référence visuelle des portails/bases/chemins) ──
    ClearBackground(theme_get(gs->map.theme)->palette.bg);
    Camera2D cam = {0};
    cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
    cam.zoom   = g_map_render_scale;
    BeginMode2D(cam);
        render_map(&gs->map);
        tile_art_draw_paths(&gs->map);
        tile_art_draw_spawns(&gs->map);
        render_spawn_exclusion_zones(&gs->map);
        render_bases(&gs->map);
        // ── PLATEAU PARTAGÉ : tours + unités du défenseur (marqueurs live) ──
        for (int i = 0; i < ctx->mp_board_tn; i++) {
            int tx = ctx->mp_board_t[i*3], ty = ctx->mp_board_t[i*3+1];
            int tt = ctx->mp_board_t[i*3+2];
            Color c = renderer_tower_color((TowerType)tt);
            Rectangle rr = {tx*(float)TILE_SIZE + 5, ty*(float)TILE_SIZE + 5,
                            TILE_SIZE - 10.0f, TILE_SIZE - 10.0f};
            DrawRectangleRounded(rr, 0.3f, 4, c);
            DrawRectangleRoundedLinesEx(rr, 0.3f, 4, 2.0f, (Color){10, 8, 6, 230});
        }
        for (int i = 0; i < ctx->mp_board_un; i++) {
            int tx = ctx->mp_board_u[i*3], ty = ctx->mp_board_u[i*3+1];
            int ut = ctx->mp_board_u[i*3+2];
            Color c = renderer_unit_color((UnitType)ut);
            float px = tx*(float)TILE_SIZE + TILE_SIZE/2.0f;
            float py = ty*(float)TILE_SIZE + TILE_SIZE/2.0f;
            DrawCircle((int)px, (int)py, 6.0f, c);
            DrawCircleLines((int)px, (int)py, 6.0f, (Color){10, 8, 6, 230});
        }
    EndMode2D();

    // ── En-tête + statut du défenseur + légende plateau ──
    const char *title = "ENVAHISSEUR — composez vos vagues";
    int tw = mtxt(title, 13);
    dtxt(title, cx - tw/2, 6, 13, (Color){220, 140, 230, 255});
    char ds[96];
    if (ctx->mp_peer_valid)
        snprintf(ds, sizeof(ds), "Defenseur %s : survie  V%d / %d     PV %d",
                 ctx->session.peer_name[0] ? ctx->session.peer_name : "?",
                 ctx->mp_peer.wave, ASYM_SURVIVE_WAVES, ctx->mp_peer.lives);
    else
        snprintf(ds, sizeof(ds), "Connexion au defenseur...");
    int dw = mtxt(ds, 11);
    dtxt(ds, cx - dw/2, 6 + fh(13) + 4, 11, (Color){200, 180, 120, 255});
    const char *leg = "Carte LIVE  -  carres = tours, points = unites du defenseur";
    int lw = mtxt(leg, 8);
    dtxt(leg, cx - lw/2, 6 + fh(13) + 4 + fh(11) + 3, 8, (Color){150, 130, 160, 255});

    // ── Compositeur de vague (bas de l'écran) : 2 rangées de 5 ──
    int bw = 120, bh = 28, gap = 6;
    int cols = 5;
    int rowsw = cols*bw + (cols-1)*gap;             // largeur d'une rangée
    int panel_h = 162, panel_y = g_canvas_virt_h - panel_h - 8;
    int pw = rowsw + 56, px = base_cx - pw/2;
    DrawRectangleRounded((Rectangle){(float)px,(float)panel_y,(float)pw,(float)panel_h},
                         0.06f, 6, (Color){12, 6, 16, 236});
    DrawRectangleRoundedLinesEx((Rectangle){(float)px,(float)panel_y,(float)pw,(float)panel_h},
                         0.06f, 6, 1.5f, (Color){120, 50, 140, 220});

    int max_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "VAGUE %d    Budget : %d / %d",
             ctx->mp_wave_num + 1, ctx->mp_wave_budget, max_budget);
    int hw = mtxt(hdr, 11);
    dtxt(hdr, base_cx - hw/2, panel_y + 6, 11, (Color){220, 160, 240, 255});

    // Roster cliquable (ajoute un ennemi à la vague) — 2 rangées de 5
    int rx0 = base_cx - rowsw/2, ry0 = panel_y + 6 + fh(11) + 4;
    for (int i = 0; i < INV_ROSTER_COUNT; i++) {
        int col = i % cols, row = i / cols;
        Rectangle r = {(float)(rx0 + col*(bw+gap)), (float)(ry0 + row*(bh+gap)),
                       (float)bw, (float)bh};
        int afford = ctx->mp_wave_budget >= INV_ROSTER[i].cost;
        int hov    = afford && CheckCollisionPointRec(vm, r);
        if (click && hov) {
            ctx->mp_wave_compose[INV_ROSTER[i].type]++;
            ctx->mp_wave_budget -= INV_ROSTER[i].cost;
        }
        Color bg  = !afford ? (Color){18,14,20,230} : hov ? (Color){62,30,72,240} : (Color){28,14,36,235};
        Color brd = afford ? (Color){175,85,195,235} : (Color){70,60,75,200};
        DrawRectangleRounded(r, 0.25f, 4, bg);
        DrawRectangleRoundedLinesEx(r, 0.25f, 4, hov ? 2.0f : 1.2f, brd);
        char b[28]; snprintf(b, sizeof(b), "%s -%d", INV_ROSTER[i].name, INV_ROSTER[i].cost);
        int b2 = mtxt(b, 9);
        dtxt(b, (int)r.x + bw/2 - b2/2, (int)r.y + bh/2 - fh(9)/2, 9,
             afford ? (Color){210,235,180,255} : (Color){110,100,110,255});
    }
    int roster_bottom = ry0 + 2*bh + gap;           // bas de la 2e rangée

    // Résumé de la vague composée
    char comp[200]; snprintf(comp, sizeof(comp), "%s", "Votre vague : ");
    int any = 0, tcount = 0;
    for (int i = 0; i < INV_ROSTER_COUNT; i++) {
        int n = ctx->mp_wave_compose[INV_ROSTER[i].type];
        if (n > 0) {
            char piece[40]; snprintf(piece, sizeof(piece), "%s%dx %s", any ? ", " : "", n, INV_ROSTER[i].name);
            strncat(comp, piece, sizeof(comp) - strlen(comp) - 1);
            any = 1; tcount += n;
        }
    }
    if (!any) strncat(comp, "(vide - cliquez des ennemis ci-dessus)", sizeof(comp) - strlen(comp) - 1);
    int cw = mtxt(comp, 9);
    dtxt(comp, base_cx - cw/2, roster_bottom + 6, 9, (Color){180,170,190,255});

    // Boutons ENVOYER / VIDER
    int by = roster_bottom + 6 + fh(9) + 6;
    Rectangle bsend = {(float)(base_cx - 170), (float)by, 200.0f, 30.0f};
    Rectangle bclr  = {(float)(base_cx + 40),  (float)by, 130.0f, 30.0f};
    int ready    = (ctx->mp_wave_cooldown <= 0.0f);
    int can_send = (tcount > 0) && ready;
    int hov_s = can_send && CheckCollisionPointRec(vm, bsend);
    int hov_c = (tcount > 0) && CheckCollisionPointRec(vm, bclr);

    DrawRectangleRounded(bsend, 0.3f, 5, hov_s ? (Color){40,120,60,240}
                                       : can_send ? (Color){20,70,35,235} : (Color){25,22,18,220});
    DrawRectangleRoundedLinesEx(bsend, 0.3f, 5, hov_s ? 2.0f : 1.2f,
                                can_send ? (Color){80,200,110,235} : (Color){70,65,55,200});
    char eb[48];
    if (!ready)  snprintf(eb, sizeof(eb), "RECHARGE  %.0fs", ctx->mp_wave_cooldown + 0.9f);
    else         snprintf(eb, sizeof(eb), "ENVOYER LA VAGUE (%d)", tcount);
    int ew = mtxt(eb, 10);
    dtxt(eb, (int)bsend.x + 100 - ew/2, (int)bsend.y + 15 - fh(10)/2, 10,
         can_send ? (Color){200,240,200,255} : (Color){110,105,95,255});

    DrawRectangleRounded(bclr, 0.3f, 5, hov_c ? (Color){90,40,40,235} : (Color){40,22,22,225});
    DrawRectangleRoundedLinesEx(bclr, 0.3f, 5, hov_c ? 2.0f : 1.2f, (Color){160,70,70,210});
    int vw2 = mtxt("VIDER", 10);
    dtxt("VIDER", (int)bclr.x + 65 - vw2/2, (int)bclr.y + 15 - fh(10)/2, 10, (Color){220,160,160,255});

    if (click && hov_s) {              // envoie la vague composée
        for (int i = 0; i < INV_ROSTER_COUNT; i++) {
            int t = INV_ROSTER[i].type;
            for (int k = 0; k < ctx->mp_wave_compose[t]; k++) {
                uint8_t et = (uint8_t)t;
                net_session_send(&ctx->session, NMSG_SEND_ENEMY, &et, 1);
            }
        }
        memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
        ctx->mp_wave_num++;
        ctx->mp_wave_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
        ctx->mp_wave_cooldown = INV_WAVE_COOLDOWN;   // pacing entre vagues
    } else if (click && hov_c) {       // vide la composition (rembourse)
        memset(ctx->mp_wave_compose, 0, sizeof(ctx->mp_wave_compose));
        ctx->mp_wave_budget = INV_WAVE_BUDGET(ctx->mp_wave_num);
    }

    mp_draw_result(ctx);
}

static int game_do_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;

    ClearBackground(theme_get(gs->map.theme)->palette.bg);

    /* Décale la carte horizontalement pour centrer dans le canvas large.
       + secousse caméra (jus) : décalage purement visuel (le mappage souris
       reste sur g_map_x_off non secoué). */
    Camera2D map_cam = {0};
    Vector2 _mo = map_origin();   // inclut le zoom joueur + pan (source unique)
    map_cam.offset = (Vector2){_mo.x + fx_shake_dx(), _mo.y + fx_shake_dy()};
    map_cam.zoom   = map_eff_scale();
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

    /* ── Indicateur de PAUSE TACTIQUE (solo) ─────────────────── */
    if (ctx->tactical_pause && !ctx->menu.paused) {
        int cxp = g_map_x_off + g_canvas_virt_w_base / 2;
        const char *pt = "PAUSE  -  ESPACE pour reprendre";
        int tw = mtxt(pt, 11);
        int bw = tw + 28, bx = cxp - bw/2, by = 40, bh = fh(11) + 12;
        Rectangle pr = {(float)bx, (float)by, (float)bw, (float)bh};
        DrawRectangleRounded(pr, 0.4f, 6, (Color){10, 20, 40, 230});
        DrawRectangleRoundedLinesEx(pr, 0.4f, 6, 1.8f, (Color){90, 150, 220, 240});
        dtxt(pt, cxp - tw/2, by + (bh - fh(11))/2, 11, (Color){175, 215, 255, 255});
    }

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

    /* SCREEN_GAME — Asym ENVAHISSEUR : sim figée, écran de commande dédié */
    if (ctx->mp_invader) {
        mp_game_tick(ctx, dt);
        mp_handle_result(ctx);
        mp_invader_render(ctx);
        ctx->interlude_prev = ctx->interlude;
        return 1;
    }

    /* SCREEN_GAME */
    ctx->gs.ui.mp_block_click = (ctx->mp_result != 0) ? 1 : 0;
    mp_panels_input(ctx);      // MP : glissement des panneaux (avant l'input HUD)
    mp_duel_send_input(ctx);   // Duel : boutons d'envoi (avant l'input HUD)
    game_zoom_input(ctx);      // zoom molette + déplacement carte
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

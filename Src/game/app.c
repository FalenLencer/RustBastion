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
#include "../ui/terrain.h"
#include "../ui/perk_art.h"
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
    terrain_unload();   /* textures du terrain peint */
    perk_art_unload();  /* atlas des emblèmes de perks */
    opts_save(&ctx->menu.opts);
    menu_cleanup(&ctx->menu);
}



// ════════════════════════════════════════════════════════════════
// ÉCRAN JEU — MISE À JOUR
// ════════════════════════════════════════════════════════════════
// Zoom carte (molette, centré sur le curseur) + déplacement (bouton du milieu
// OU flèches du clavier).
// Tout passe par g_map_zoom/pan → le rendu ET le mappage souris restent alignés.
// Vitesse de déplacement au clavier (px de canvas virtuel par seconde).
#define MAP_PAN_KEY_SPEED  700.0f
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

    // Déplacement au CLAVIER (flèches) — indispensable sans clic-molette
    // (portables, trackpads) : sans cela une carte zoomée est infranchissable.
    // Placé AVANT le clamp ci-dessous, qui borne les deux méthodes pareil.
    if (g_map_zoom > 1.0f) {
        float pan_step = MAP_PAN_KEY_SPEED * GetFrameTime();
        if (IsKeyDown(KEY_RIGHT)) g_map_pan_x -= pan_step;
        if (IsKeyDown(KEY_LEFT))  g_map_pan_x += pan_step;
        if (IsKeyDown(KEY_DOWN))  g_map_pan_y -= pan_step;
        if (IsKeyDown(KEY_UP))    g_map_pan_y += pan_step;
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
    /* ECHAP = « revenir en arrière d'un cran », convention RTS :
       s'il y a une sélection en cours, on l'annule ; sinon seulement,
       on (dé)pause. Traité ICI et NULLE PART AILLEURS — quand ce même
       ECHAP dépausait, game_do_update tournait dans la foulée et
       l'ancien traitement dans ui_update effaçait la sélection que le
       joueur venait de mettre en place avant de faire une pause. */
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (!ctx->menu.paused && ui_has_selection(&ctx->gs.ui)) {
            ui_clear_selection(&ctx->gs.ui, &ctx->gs);
        } else {
            ctx->menu.paused ^= 1;
            ctx->menu.screen  = ctx->menu.paused ? MENU_PAUSE : MENU_TITLE;
            ctx->tactical_pause = 0;   // le menu pause remplace la pause tactique
        }
    }
    // Pause TACTIQUE (solo) : ESPACE gèle la simulation mais on peut continuer à
    // placer/vendre/déplacer pour réfléchir. Désactivée en multijoueur (équité).
    // Exclue sur l'écran de défaite : ESPACE y signifie « Reprendre » et ce
    // même appui, lu ici AVANT game_handle_gameover, gelait l'acte relancé.
    if (IsKeyPressed(KEY_SPACE) && !ctx->menu.paused &&
        ctx->gs.ui.disc_count == 0 && !ctx->mp_in_game &&
        ctx->interlude == INTER_NONE && ctx->gs.phase != PHASE_GAMEOVER)
        ctx->tactical_pause ^= 1;
    // Aide des commandes (touche H) — affiche/masque le panneau.
    if (IsKeyPressed(KEY_H) && ctx->interlude == INTER_NONE)
        ctx->show_help ^= 1;
    // Mode de vue (F4) : bascule unités/ennemis entre sprites 2D et modèles 3D.
    if (IsKeyPressed(KEY_F4))
        g_units_3d ^= 1;
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

    /* Fenêtre en arrière-plan (alt-tab) : la simulation GÈLE en solo.
       Perdre une vague pendant qu'on regarde ailleurs est une frustration
       pure, sans le moindre intérêt de jeu. La reprise est immédiate au
       retour du focus — aucun état de menu n'est touché.
       EXCLU EN MULTIJOUEUR : l'adversaire, lui, continue de simuler ;
       geler ici désynchroniserait la session (et avantagerait le joueur). */
    if (!ctx->mp_in_game && !IsWindowFocused()) return;

    /* Décompte du minuteur de bannière — figé tant qu'une fiche de
       découverte est ouverte (la bannière est masquée pendant ce temps). */
    if (ctx->banner_timer > 0.0f && ctx->gs.ui.disc_count == 0) {
        ctx->banner_timer -= dt;
        if (ctx->banner_timer < 0.0f) ctx->banner_timer = 0.0f;
    }

    ui_update(&ctx->gs.ui, &ctx->gs);
    // Fiche de découverte visible → jeu gelé (ui_update gère la fermeture)
    if (ctx->gs.ui.disc_count > 0) return;
    // Pause tactique : on a pu agir (ui_update ci-dessus) mais la SIMULATION ne
    // s'avance pas → temps de réfléchir/placer.
    if (ctx->tactical_pause) return;
    // Equite / anti-desync : l'avance rapide (X) est un outil SOLO. En MP chaque
    // joueur simule son propre plateau → accelerer le sien = triche (Course/Duel)
    // ou desynchronise le plateau partage (Asym). On force x1 en multijoueur.
    if (ctx->mp_in_game && ctx->gs.ui.speed_mult != 1) ctx->gs.ui.speed_mult = 1;
    game_state_update(&ctx->gs, dt);
    // Météo d'ambiance : même rythme que la sim (figée en pause, ×vitesse)
    ambient_update(dt * (float)ctx->gs.ui.speed_mult);
    // FX au meme rythme que la sim : en avance rapide (X), particules/popups
    // suivent le jeu au lieu de trainer a x1 (et de saturer le pool de popups).
    fx_update(dt * (float)ctx->gs.ui.speed_mult);

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

/* ════════════════════════════════════════════════════════════════
   INVARIANT : un interlude DOIT correspondre au MODE de la partie
   ────────────────────────────────────────────────────────────────
   `ctx->interlude` est un état d'écran GLOBAL, écrit par une dizaine
   de sites et restauré depuis la sauvegarde de campagne, alors que la
   plupart des retours au menu (héros, tutoriel, game-over, fin de MP)
   ne le remettaient PAS à zéro. Un POINT D'EXTRACTION — écran propre
   au mode endless — pouvait donc s'afficher en pleine CAMPAGNE, où ses
   deux boutons sont destructeurs : [E] supprime la sauvegarde ARCADE
   (save_delete) et abandonne la run, [ESPACE] RÉGÉNÈRE la carte au
   milieu de l'acte. Même chose en miroir pour butin/boutique/dialogues
   qui réapparaîtraient dans une partie arcade.

   Vérifié ici, à l'entrée de frame AVANT tout rendu ou handler : un
   interlude hors de son mode est annulé, quelle que soit sa provenance
   — y compris une sauvegarde écrite par une version précédente.      */
static void interlude_check_mode(AppContext *ctx) {
    const GameState *gs = &ctx->gs;
    int ok;
    switch (ctx->interlude) {
    case INTER_NONE:    ok = 1; break;
    case INTER_EXTRACT: ok = gs->is_endless  && !ctx->mp_in_game; break;
    case INTER_DIALOG_BEFORE:
    case INTER_DIALOG_AFTER:
    case INTER_DRAFT:
    case INTER_SHOP:    ok = gs->is_campaign && !ctx->mp_in_game; break;
    default:            ok = 0; break;   /* valeur inconnue (vieille save) */
    }
    if (!ok) ctx->interlude = INTER_NONE;
}

// ════════════════════════════════════════════════════════════════
// POINT D'ENTRÉE DE FRAME
// ════════════════════════════════════════════════════════════════
int app_update(AppContext *ctx, float dt) {
    g_colorblind        = ctx->menu.opts.colorblind;        // palette daltonisme
    g_show_entity_names = ctx->menu.opts.show_entity_names; // noms des ennemis

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
        return app_handle_menu(ctx);

    /* Interlude cohérent avec le mode (cf. interlude_check_mode) —
       AVANT tout handler et tout rendu de cette frame. */
    interlude_check_mode(ctx);

    /* SCREEN_GAME — Asym ENVAHISSEUR : sim figée, écran de commande dédié */
    if (ctx->mp_invader) {
        mp_game_tick(ctx, dt);
        mp_handle_result(ctx);
        mp_invader_render(ctx);
        ctx->interlude_prev = ctx->interlude;
        return 1;
    }

    /* SCREEN_GAME — MODE HÉROS : boucle dédiée (input + sim + rendu 3D) */
    if (ctx->hero_mode) {
        int keep = hero_frame(ctx, dt);
        ctx->interlude_prev = ctx->interlude;
        return keep;
    }

    /* SCREEN_GAME */
    ctx->gs.ui.mp_block_click = (ctx->mp_result != 0) ? 1 : 0;
    mp_panels_input(ctx);      // MP : glissement des panneaux (avant l'input HUD)
    mp_duel_send_input(ctx);   // Duel : boutons d'envoi (avant l'input HUD)
    game_zoom_input(ctx);      // zoom molette + déplacement carte
    tutorial_tick(ctx);        // tutoriel : avance d'étape (avant l'input HUD)
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
    int keep = app_game_render(ctx);
    /* Mémorise l'interlude pour le frame suivant (garde anti-bleed d'input). */
    ctx->interlude_prev = ctx->interlude;
    return keep;
}

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
#include "game_init.h"
#include "save.h"
#include "meta.h"
#include "../engine/audio.h"
#include "../engine/canvas.h"
#include "../engine/window.h"
#include "../ui/renderer.h"
#include "../ui/hud.h"
#include "../ui/interlude.h"
#include "../ui/campaign_data.h"
#include "../ui/ui_utils.h"   // DrawText/MeasureText → g_font (support accents)
#include "../map/theme.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

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
    };
    opts_load(&opts);
    audio_set_master_volume(opts.master_volume / 100.0f);
    audio_set_music_volume (opts.music_volume  / 100.0f);
    audio_set_sfx_volume   (opts.sfx_volume    / 100.0f);

    menu_init(&ctx->menu, &opts);

    ctx->active_slot      = -1;
    ctx->screen           = SCREEN_MENU;
    ctx->interlude        = INTER_NONE;
    ctx->applied_fps      = 60;
    ctx->prev_menu_screen = ctx->menu.screen;
}

// ════════════════════════════════════════════════════════════════
// SAVE / CLEANUP
// ════════════════════════════════════════════════════════════════
void app_save_on_exit(const AppContext *ctx) {
    if (ctx->screen == SCREEN_GAME &&
        ctx->active_slot >= 0     &&
        ctx->gs.phase != PHASE_GAMEOVER &&
        ctx->interlude == INTER_NONE)
        save_write(&ctx->gs, ctx->active_slot);
}

void app_cleanup(AppContext *ctx) {
    opts_save(&ctx->menu.opts);
    menu_cleanup(&ctx->menu);
}

// ════════════════════════════════════════════════════════════════
// ÉCRAN MENU
// ════════════════════════════════════════════════════════════════
static int handle_menu(AppContext *ctx) {
    menu_update(&ctx->menu, &ctx->gs.meta);
    ClearBackground((Color){8,5,3,255});
    MenuAction act = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                         g_canvas_virt_w, VIRT_H);

    if (act.quit_app) return 0;

    if (act.toggle_fs == 1) ToggleFullscreen();
    if (act.toggle_fs == 2 && !IsWindowFullscreen())
        window_apply_size(ctx->menu.opts.win_width,
                          ctx->menu.opts.win_height);

    if (act.start_arcade) {
        ctx->active_slot = act.new_slot;
        game_init_arcade(&ctx->gs, act.new_theme, ctx->active_slot);
        save_write(&ctx->gs, ctx->active_slot);
        ctx->menu.screen = MENU_TITLE;
        ctx->screen      = SCREEN_GAME;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.start_campaign) {
        ctx->active_slot = act.new_slot;
        ctx->gs.campaign_order_seed = act.campaign_order_seed;
        game_init_campaign(&ctx->gs, ctx->gs.meta.campaigns_completed,
                           ctx->active_slot, act.campaign_order_seed);
        save_write(&ctx->gs, ctx->active_slot);
        ctx->menu.screen = MENU_TITLE;
        ctx->screen      = SCREEN_GAME;
        ctx->interlude   = INTER_DIALOG_BEFORE;
        audio_play_theme_music(ctx->gs.map.theme);
    }

    if (act.go_game && !act.start_arcade && !act.start_campaign) {
        int slot = act.resume_slot;
        if (save_read(&ctx->gs, slot)) {
            ctx->active_slot = slot;
            ctx->menu.screen = MENU_TITLE;
            ctx->screen      = SCREEN_GAME;
        } else {
            ctx->active_slot = slot;
            game_init_arcade(&ctx->gs, THEME_COUNT, slot);
            save_write(&ctx->gs, ctx->active_slot);
            ctx->screen = SCREEN_GAME;
        }
        audio_play_theme_music(ctx->gs.map.theme);
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
        int sh = GetScreenHeight();
        float scale = (float)sh / VIRT_H;
        if (scale < 0.05f) scale = 0.05f;
        Vector2 mp = GetMousePosition();
        Vector2 vm = { mp.x / scale, mp.y / scale };
        if (CheckCollisionPointRec(vm, ctx->gs.ui.pause_btn)) {
            ctx->menu.paused = 1;
            ctx->menu.screen = MENU_PAUSE;
        }
    }
}

static void game_do_update(AppContext *ctx, float dt) {
    if (ctx->menu.paused || ctx->interlude != INTER_NONE) return;

    ui_update(&ctx->gs.ui, &ctx->gs);
    game_state_update(&ctx->gs, dt);

    /* Déclenchement extraction endless toutes les 10 vagues */
    if (ctx->gs.is_endless               &&
        !ctx->gs.endless_pending_extract  &&
        ctx->gs.phase == PHASE_PREP       &&
        ctx->gs.wave_manager.number > 0   &&
        ctx->gs.wave_manager.number % 10 == 0)
    {
        ctx->gs.endless_pending_extract = 1;
        ctx->interlude = INTER_EXTRACT;
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
    if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (ctx->active_slot >= 0) save_delete(ctx->active_slot);
        ctx->active_slot        = -1;
        ctx->gameover_meta_done = 0;  /* reset pour la prochaine partie */
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused  = 0;
        ctx->menu.screen  = ctx->gs.is_campaign ? MENU_WORLD_MAP : MENU_ARCADE;
        ctx->screen       = SCREEN_MENU;
        audio_stop_music();
    }
}

static void game_handle_campaign_end(AppContext *ctx) {
    if (!ctx->gs.is_campaign            ||
        ctx->gs.phase   != PHASE_PREP   ||
        ctx->gs.wave_manager.number < 1 ||
        ctx->interlude  != INTER_NONE   ||
        !IsKeyPressed(KEY_TAB))
        return;

    int stage_idx       = ctx->gs.campaign_stage;
    const ActData *ad   = campaign_act_get(stage_idx);
    int last            = (stage_idx == CAMPAIGN_TOTAL - 1);
    int earned          = meta_end_of_campaign_stage(
                              &ctx->gs.meta, ctx->gs.wave_manager.number,
                              ctx->gs.kills, ctx->gs.gold, stage_idx);
    int obj_ok          = campaign_objective_check(
                              ad, ctx->gs.wave_manager.number,
                              ctx->gs.kills, ctx->gs.units.count,
                              ctx->gs.act_materials_collected,
                              ctx->gs.act_no_unit_lost);
    int stars           = meta_record_act(&ctx->gs.meta, stage_idx, obj_ok, 0);

    audio_play_sfx(AUDIO_SFX_VICTORY);
    ctx->interlude       = INTER_DIALOG_AFTER;
    ctx->interlude_scrap = earned;
    ctx->interlude_last  = last;
    ctx->interlude_stars = stars;
    save_write(&ctx->gs, ctx->active_slot);
}

static void game_handle_dialog_after(AppContext *ctx) {
    if (ctx->interlude != INTER_DIALOG_AFTER) return;
    if (!IsKeyPressed(KEY_SPACE) && !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return;
    if (ctx->interlude_last) {
        if (ctx->active_slot >= 0) save_delete(ctx->active_slot);
        ctx->active_slot  = -1;
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused  = 0;
        ctx->menu.screen  = MENU_WORLD_MAP;
        ctx->screen       = SCREEN_MENU;
        ctx->interlude    = INTER_NONE;
        audio_stop_music();
    } else {
        game_next_campaign_stage(&ctx->gs);
        save_write(&ctx->gs, ctx->active_slot);
        audio_play_theme_music(ctx->gs.map.theme);
        ctx->interlude = INTER_DIALOG_BEFORE;
    }
}

static void game_handle_extract(AppContext *ctx) {
    if (ctx->interlude != INTER_EXTRACT) return;

    /* Souris virtuelle (même calcul que dans game_do_render) */
    int sh = GetScreenHeight();
    float scale = (float)sh / VIRT_H;
    if (scale < 0.05f) scale = 0.05f;
    Vector2 mp = GetMousePosition();
    Vector2 vm = { mp.x / scale, mp.y / scale };

    /* Rectangles des boutons — doivent correspondre à interlude_render_extract */
    {
        int vw = g_canvas_virt_w, vh = VIRT_H;
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
            audio_stop_music();
            return;
        }
        if (IsKeyPressed(KEY_SPACE) || continue_clicked) {
            ctx->gs.endless_multiplier *= 1.5f;
            if (ctx->gs.endless_multiplier > 6.0f)
                ctx->gs.endless_multiplier = 6.0f;
            ctx->gs.endless_series++;
            ctx->gs.endless_pending_extract = 0;
            game_init_map(&ctx->gs,
                          (ThemeID)((ctx->gs.map.theme + 1) % THEME_COUNT));
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
static int game_do_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;

    ClearBackground(theme_get(gs->map.theme)->palette.bg);

    /* Décale la carte horizontalement pour centrer dans le canvas large */
    Camera2D map_cam = {0};
    map_cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
    map_cam.zoom   = 1.0f;
    BeginMode2D(map_cam);
        render_map(&gs->map);
        render_spawn_exclusion_zones(&gs->map);
        render_bases(&gs->map);
        render_deposits(&gs->map);
        render_paths(&gs->enemy_paths);
        render_towers(&gs->towers);
        render_units(&gs->units);
        render_enemies(&gs->enemies);
        render_projectiles(&gs->towers);
    EndMode2D();

    ui_render(&gs->ui, gs);

    /* ── Bannière centrale (mode + nom de la carte) ──────────── */
    {
        char line1[80] = {0};
        char line2[80] = {0};
        Color col1 = {255, 220, 100, 255};
        Color col2 = {150, 215, 110, 255};

        if (gs->is_campaign && ctx->interlude == INTER_NONE) {
            const ActData *ad = campaign_act_get(gs->campaign_stage);
            snprintf(line1, sizeof(line1), "Campagne  —  CH.%d  —  %s",
                     ad->chapter + 1, ad->title);
            if (gs->phase == PHASE_PREP && gs->wave_manager.number >= 1)
                snprintf(line2, sizeof(line2), "TAB : Terminer l'acte   |   %s",
                         ad->objective.description);
        } else if (gs->is_endless) {
            int score = (int)((float)gs->wave_manager.number
                              * gs->endless_multiplier * 10.0f);
            snprintf(line1, sizeof(line1),
                     "Endless  —  Serie %d  —  x%.1f  —  Score %d",
                     gs->endless_series + 1, gs->endless_multiplier, score);
            int next_extr = 10 - (gs->wave_manager.number % 10);
            if (next_extr == 10 && gs->wave_manager.number > 0) next_extr = 0;
            if (next_extr > 0)
                snprintf(line2, sizeof(line2),
                         "Extraction dans %d vague(s)", next_extr);
            col1 = (Color){160, 225, 255, 255};
            col2 = (Color){120, 185, 225, 255};
        } else {
            // Arcade classique
            const Theme *th = theme_get(gs->map.theme);
            snprintf(line1, sizeof(line1), "Arcade  —  %s", th->name);
            col1 = (Color){225, 200, 100, 255};
        }

        if (line1[0]) {
            int fs1 = 14;
            int tw1 = mtxt(line1, fs1);
            int bx  = g_map_x_off + MAP_W * TILE_SIZE / 2 - tw1 / 2;
            int by  = 6;
            DrawRectangleRounded(
                (Rectangle){bx - 12, by - 4, tw1 + 24, 23},
                0.35f, 4, (Color){6, 4, 1, 230});
            DrawRectangleRoundedLinesEx(
                (Rectangle){bx - 12, by - 4, tw1 + 24, 23},
                0.35f, 4, 1.0f, (Color){80, 60, 18, 200});
            dtxt(line1, bx, by, fs1, col1);
        }
        if (line2[0]) {
            int fs2 = 11;
            int tw2 = mtxt(line2, fs2);
            int bx  = g_map_x_off + MAP_W * TILE_SIZE / 2 - tw2 / 2;
            int by  = 33;
            DrawRectangleRounded(
                (Rectangle){bx - 8, by - 3, tw2 + 16, 18},
                0.35f, 4, (Color){6, 4, 1, 215});
            DrawRectangleRoundedLinesEx(
                (Rectangle){bx - 8, by - 3, tw2 + 16, 18},
                0.35f, 4, 1.0f, (Color){60, 45, 12, 160});
            dtxt(line2, bx, by, fs2, col2);
        }
    }

    /* ── Interludes ───────────────────────────────────────────── */
    if (gs->phase == PHASE_GAMEOVER)
        interlude_render_gameover(gs, g_canvas_virt_w, VIRT_H);

    if (ctx->interlude == INTER_DIALOG_BEFORE) {
        interlude_render_dialog_before(campaign_act_get(gs->campaign_stage),
                                       g_canvas_virt_w, VIRT_H);
        if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            ctx->interlude = INTER_NONE;
    }
    if (ctx->interlude == INTER_DIALOG_AFTER) {
        interlude_render_dialog_after(campaign_act_get(gs->campaign_stage),
                                      ctx->interlude_stars,
                                      ctx->interlude_scrap, g_canvas_virt_w, VIRT_H);
    }
    if (ctx->interlude == INTER_EXTRACT) {
        /* Convertit la souris en coordonnées virtuelles */
        int sh = GetScreenHeight();
        float scale = (float)sh / VIRT_H;
        if (scale < 0.05f) scale = 0.05f;
        Vector2 mp = GetMousePosition();
        Vector2 vm = { mp.x / scale, mp.y / scale };
        interlude_render_extract(gs, g_canvas_virt_w, VIRT_H, vm);
    }

    /* ── Menu pause superposé ─────────────────────────────────── */
    if (ctx->menu.paused) {
        MenuAction pact = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                              g_canvas_virt_w, VIRT_H);
        if (pact.save_and_quit == 2 && ctx->active_slot >= 0)
            save_write(&ctx->gs, ctx->active_slot);
        if (pact.save_and_quit == 1) {
            if (ctx->active_slot >= 0)
                save_write(&ctx->gs, ctx->active_slot);
            menu_refresh_slots(&ctx->menu);
            ctx->menu.paused = 0;
            ctx->menu.screen = ctx->gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
            ctx->screen      = SCREEN_MENU;
            ctx->interlude   = INTER_NONE;
            audio_stop_music();
        }
        if (pact.quit_app) {
            if (ctx->active_slot >= 0)
                save_write(&ctx->gs, ctx->active_slot);
            return 0;   /* quitter l'application */
        }
        if (pact.toggle_fs == 1) ToggleFullscreen();
        if (pact.toggle_fs == 2 && !IsWindowFullscreen())
            window_apply_size(ctx->menu.opts.win_width,
                              ctx->menu.opts.win_height);
    }
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
    game_handle_gameover(ctx);
    game_handle_campaign_end(ctx);
    game_handle_dialog_after(ctx);
    game_handle_extract(ctx);
    return game_do_render(ctx);
}

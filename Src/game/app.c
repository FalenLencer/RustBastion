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
        float scale = (float)sh / g_canvas_virt_h;
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

    /* Décompte du minuteur de bannière */
    if (ctx->banner_timer > 0.0f) {
        ctx->banner_timer -= dt;
        if (ctx->banner_timer < 0.0f) ctx->banner_timer = 0.0f;
    }

    ui_update(&ctx->gs.ui, &ctx->gs);
    // Fiche de découverte visible → jeu gelé (ui_update gère la fermeture)
    if (ctx->gs.ui.disc_count > 0) return;
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

    const ActData *_ad_check = campaign_act_get(ctx->gs.campaign_stage);
    int _wave_done = (ctx->gs.wave_manager.number >= _ad_check->min_waves);

    /* Déclenche automatiquement quand le quota est atteint,
       ou sur TAB si le quota est déjà atteint (sortie manuelle). */
    if (!_wave_done && !IsKeyPressed(KEY_TAB)) return;
    if (!_wave_done) return; /* TAB avant le quota : ignoré */

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
    /* Sauvegarde avec état interlude pour éviter le double-ferraille au rechargement */
    campaign_save_write(&ctx->gs, ctx->active_slot,
                        INTER_DIALOG_AFTER, earned, stars, last);
}

static void game_handle_dialog_after(AppContext *ctx) {
    if (ctx->interlude != INTER_DIALOG_AFTER) return;
    if (!IsKeyPressed(KEY_SPACE) && !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return;
    if (ctx->interlude_last) {
        /* Dernière étape terminée : supprime la save campagne et retour menu */
        if (ctx->active_slot >= 0) campaign_save_delete(ctx->active_slot);
        ctx->active_slot  = -1;
        menu_refresh_slots(&ctx->menu);
        ctx->menu.paused  = 0;
        ctx->menu.screen  = MENU_WORLD_MAP;
        ctx->screen       = SCREEN_MENU;
        ctx->interlude    = INTER_NONE;
        audio_play_menu_music();
    } else {
        /* Passe à l'acte suivant — sauvegarde en état DIALOG_BEFORE */
        game_next_campaign_stage(&ctx->gs);
        campaign_save_write(&ctx->gs, ctx->active_slot,
                            INTER_DIALOG_BEFORE, 0, 0, 0);
        audio_play_theme_music(ctx->gs.map.theme);
        ctx->interlude    = INTER_DIALOG_BEFORE;
        ctx->banner_timer = 5.0f;
    }
}

static void game_handle_extract(AppContext *ctx) {
    if (ctx->interlude != INTER_EXTRACT) return;

    /* Souris virtuelle (même calcul que dans game_do_render) */
    int sh = GetScreenHeight();
    float scale = (float)sh / g_canvas_virt_h;
    if (scale < 0.05f) scale = 0.05f;
    Vector2 mp = GetMousePosition();
    Vector2 vm = { mp.x / scale, mp.y / scale };

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
static int game_do_render(AppContext *ctx) {
    GameState *gs = &ctx->gs;

    ClearBackground(theme_get(gs->map.theme)->palette.bg);

    /* Décale la carte horizontalement pour centrer dans le canvas large */
    Camera2D map_cam = {0};
    map_cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
    map_cam.zoom   = g_map_render_scale;
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

    /* ── Bannière (mode de jeu + infos de carte) ─────────────── */
    /* • Démarrage : grand overlay centré sur la carte, fade-in 0.3 s,
         plein 4.2 s, fade-out 0.5 s  (banner_timer compte de 5 → 0).
       • Pause      : bandeau compact fixe en haut de la carte.      */
    {
        char big_title[80]  = {0};   /* titre principal (gros, centré)  */
        char line1    [120] = {0};   /* sous-titre / détails            */
        char line2    [80]  = {0};   /* info secondaire                 */
        Color ctitle = {255, 210,  80, 255};
        Color c1     = {200, 170,  60, 255};
        Color c2     = {150, 215, 110, 255};

        if (gs->is_campaign && ctx->interlude == INTER_NONE) {
            const ActData *ad = campaign_act_get(gs->campaign_stage);
            snprintf(big_title, sizeof(big_title), "%s", ad->title);
            snprintf(line1, sizeof(line1), "Campagne  —  Ch.%d  Acte %d",
                     ad->chapter + 1, ad->act + 1);
            if (gs->wave_manager.number >= 1)
                snprintf(line2, sizeof(line2), "Objectif : %d vague(s)  |  %s",
                         ad->min_waves, ad->objective.description);
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
            snprintf(big_title, sizeof(big_title), "PERSONNALISE");
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

                /* Fond */
                DrawRectangleRounded(
                    (Rectangle){px, py, (float)pw, (float)ph},
                    0.16f, 6,
                    (Color){3, 2, 0, (unsigned char)((int)a * 220 / 255)});
                DrawRectangleRoundedLinesEx(
                    (Rectangle){px, py, (float)pw, (float)ph},
                    0.16f, 6, 2.2f,
                    (Color){ctitle.r/2, ctitle.g/2, ctitle.b/2,
                            (unsigned char)((int)a * 180 / 255)});

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

        /* ── B) Bandeau compact en haut — pendant la pause ────── */
        else if (ctx->menu.paused && big_title[0]) {
            /* Combine big_title + line1 en une seule ligne compacte */
            char compact[200];
            if (line1[0])
                snprintf(compact, sizeof(compact),
                         "%s  —  %s", big_title, line1);
            else
                snprintf(compact, sizeof(compact), "%s", big_title);

            int fs  = 11;
            int cx  = g_map_x_off + g_canvas_virt_w_base / 2;
            int tw  = mtxt(compact, fs);
            int bx  = cx - tw / 2;
            int by  = 8;
            int bh  = fh(fs) + 10;

            DrawRectangleRounded(
                (Rectangle){(float)(bx - 12), (float)(by - 4),
                            (float)(tw + 24),  (float)bh},
                0.4f, 4, (Color){4, 3, 1, 215});
            DrawRectangleRoundedLinesEx(
                (Rectangle){(float)(bx - 12), (float)(by - 4),
                            (float)(tw + 24),  (float)bh},
                0.4f, 4, 1.2f,
                (Color){ctitle.r / 2, ctitle.g / 2, ctitle.b / 2, 180});
            dtxt(compact, bx, by, fs,
                 (Color){ctitle.r, ctitle.g, ctitle.b, 255});

            /* line2 sous le bandeau (ex : objectif campagne) */
            if (line2[0]) {
                int fs2 = 10;
                int tw2 = mtxt(line2, fs2);
                int by2 = by + bh + 4;
                DrawRectangleRounded(
                    (Rectangle){(float)(cx - tw2 / 2 - 8), (float)(by2 - 3),
                                (float)(tw2 + 16), (float)(fh(fs2) + 8)},
                    0.4f, 4, (Color){4, 3, 1, 195});
                dtxt(line2, cx - tw2 / 2, by2, fs2,
                     (Color){c2.r, c2.g, c2.b, 220});
            }
        }
    }

    /* ── Interludes ───────────────────────────────────────────── */
    if (gs->phase == PHASE_GAMEOVER)
        interlude_render_gameover(gs, g_canvas_virt_w, g_canvas_virt_h);

    if (ctx->interlude == INTER_DIALOG_BEFORE) {
        interlude_render_dialog_before(campaign_act_get(gs->campaign_stage),
                                       g_canvas_virt_w, g_canvas_virt_h);
        if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            ctx->interlude = INTER_NONE;
    }
    if (ctx->interlude == INTER_DIALOG_AFTER) {
        interlude_render_dialog_after(campaign_act_get(gs->campaign_stage),
                                      ctx->interlude_stars,
                                      ctx->interlude_scrap, g_canvas_virt_w, g_canvas_virt_h);
    }
    if (ctx->interlude == INTER_EXTRACT) {
        /* Convertit la souris en coordonnées virtuelles */
        int sh = GetScreenHeight();
        float scale = (float)sh / g_canvas_virt_h;
        if (scale < 0.05f) scale = 0.05f;
        Vector2 mp = GetMousePosition();
        Vector2 vm = { mp.x / scale, mp.y / scale };
        interlude_render_extract(gs, g_canvas_virt_w, g_canvas_virt_h, vm);
    }

    /* ── Menu pause superposé ─────────────────────────────────── */
    if (ctx->menu.paused) {
        MenuAction pact = menu_render_and_act(&ctx->menu, &ctx->gs.meta,
                                              g_canvas_virt_w, g_canvas_virt_h);
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
            ctx->menu.screen = ctx->gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
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

    /* Sync show_fps opts ↔ ui :
       - En jeu (non pausé) : [F] peut avoir changé ui → on met à jour opts pour persistance.
       - En pause          : le checkbox Options peut avoir changé opts → on l'applique à ui. */
    if (ctx->menu.paused)
        ctx->gs.ui.show_fps = ctx->menu.opts.show_fps;
    else
        ctx->menu.opts.show_fps = ctx->gs.ui.show_fps;

    game_handle_gameover(ctx);
    game_handle_campaign_end(ctx);
    game_handle_dialog_after(ctx);
    game_handle_extract(ctx);
    return game_do_render(ctx);
}

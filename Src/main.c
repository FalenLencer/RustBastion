#include "raylib.h"
#include "raymath.h"
#include "core/game_state.h"
#include "core/window.h"
#include "core/canvas.h"
#include "core/game_init.h"
#include "core/save.h"
#include "ui/renderer.h"
#include "ui/ui.h"
#include "ui/menu.h"
#include "ui/interlude.h"
#include "map/theme.h"
#include "audio.h"
#include "combat/tower.h"
#include "combat/unit.h"
#include "combat/enemy.h"
#include "meta/meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef enum { SCREEN_MENU = 0, SCREEN_GAME } Screen;
typedef enum {
    INTER_NONE      = 0,
    INTER_STAGE_WIN,
    INTER_EXTRACT       // ← nouveau : fenêtre extraction endless
} InterludeState;

// Dessine la fenêtre EXTRAIRE / CONTINUER en endless
static void draw_extract_screen(const GameState *gs, int vw, int vh) {
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,170});

    int pw = 420, ph = 260;
    float rnd = 5.0f/ph;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, (Color){232,152,32,255});

    int px = cx - pw/2 + 16;
    int py = cy - ph/2 + 16;
    int iw = pw - 32;

    // Titre
    const char *t1 = "POINT D'EXTRACTION";
    DrawText(t1, cx - MeasureText(t1,18)/2, py, 18, (Color){232,152,32,255});
    py += 24;
    DrawLine(px, py, px+iw, py, (Color){60,40,12,180}); py += 10;

    // Infos
    DrawText(TextFormat("Serie       : %d", gs->endless_series+1),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    DrawText(TextFormat("Vague       : %d", gs->wave_manager.number),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    DrawText(TextFormat("Multiplicateur : x%.1f", gs->endless_multiplier),
             px, py, 12, (Color){232,152,32,255}); py += 16;

    int score = (int)((float)gs->wave_manager.number * gs->endless_multiplier * 10.0f);
    int scrap  = score / 10 > 200 ? 200 : score / 10;
    DrawText(TextFormat("Ferraille si extrait : +%d", scrap),
             px, py, 12, (Color){118,188,45,255}); py += 16;

    float next_mult = gs->endless_multiplier * 1.5f;
    DrawText(TextFormat("Continuer → multiplicateur x%.1f", next_mult),
             px, py, 10, (Color){100,160,220,255}); py += 20;

    DrawLine(px, py, px+iw, py, (Color){40,28,8,140}); py += 10;

    // Boutons
    int bw = 160, bh = 32;
    int by2 = cy + ph/2 - 16 - bh;
    int bx1 = cx - bw - 8;
    int bx2 = cx + 8;

    // EXTRAIRE
    {
        Rectangle r = {(float)bx1,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){8,28,8,255} : (Color){4,16,4,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){42,190,105,255} : (Color){20,80,40,255});
        const char *lbl = "[E] EXTRAIRE";
        DrawText(lbl, bx1+bw/2-MeasureText(lbl,13)/2, by2+bh/2-7, 13,
                 (Color){42,190,105,255});
    }
    // CONTINUER
    {
        Rectangle r = {(float)bx2,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){6,18,32,255} : (Color){4,12,20,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){52,140,210,255} : (Color){24,70,110,255});
        const char *lbl = "[ESPACE] CONTINUER";
        DrawText(lbl, bx2+bw/2-MeasureText(lbl,11)/2, by2+bh/2-7, 11,
                 (Color){52,140,210,255});
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(VIRT_W, VIRT_H, "RUST BASTION");
    window_disable_vsync();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    window_center();

    audio_init();
    save_init();

    Image icon = LoadImage("assets/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    RenderTexture2D canvas = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    RENDER_SCALE = 1.0f;

    static GameState gs;
    memset(&gs, 0, sizeof(gs));
    Screen         screen          = SCREEN_MENU;
    int            active_slot     = -1;
    InterludeState interlude       = INTER_NONE;
    int            interlude_scrap = 0;
    int            interlude_last  = 0;

    game_state_init(&gs);

    AppOptions opts = {.win_width=VIRT_W, .win_height=VIRT_H, .target_fps=60};
    MenuState menu  = {0};
    menu_init(&menu, &opts);
    int applied_fps = 60;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        audio_update();

        {
            int wfps = menu.opts.target_fps ? menu.opts.target_fps : 0;
            if (wfps != applied_fps) { SetTargetFPS(wfps); applied_fps = wfps; }
        }

        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float csc, cox, coy;
        canvas_compute(sw, sh, &csc, &cox, &coy);
        canvas_set_mouse_offset(cox, coy, csc);
        menu_set_mouse_offset(cox, coy, csc);

        BeginTextureMode(canvas);

        if (screen == SCREEN_MENU) {
            menu_update(&menu, &gs.meta);
            ClearBackground((Color){8,5,3,255});
            MenuAction act = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);

            if (act.quit_app)       { EndTextureMode(); break; }
            if (act.toggle_fs == 1) ToggleFullscreen();
            if (act.toggle_fs == 2 && !IsWindowFullscreen())
                window_apply_size(menu.opts.win_width, menu.opts.win_height);

            if (act.start_arcade) {
                active_slot = act.new_slot;
                game_init_arcade(&gs, act.new_theme, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
                audio_play_theme_music(gs.map.theme);
            }

            if (act.start_campaign) {
                active_slot = act.new_slot;
                gs.campaign_order_seed = act.campaign_order_seed;
                game_init_campaign(&gs, gs.meta.campaigns_completed,
                                   active_slot, act.campaign_order_seed);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
                audio_play_theme_music(gs.map.theme);
            }

            if (act.go_game && !act.start_arcade && !act.start_campaign) {
                int slot = act.resume_slot;
                if (save_read(&gs, slot)) {
                    active_slot = slot;
                    menu.screen = MENU_TITLE;
                    screen = SCREEN_GAME;
                } else {
                    active_slot = slot;
                    game_init_arcade(&gs, THEME_COUNT, slot);
                    save_write(&gs, active_slot);
                    screen = SCREEN_GAME;
                }
                audio_play_theme_music(gs.map.theme);
            }

        } else { // SCREEN_GAME

            if (IsKeyPressed(KEY_ESCAPE)) {
                menu.paused ^= 1;
                menu.screen = menu.paused ? MENU_PAUSE : MENU_TITLE;
            }

            if (!menu.paused && interlude == INTER_NONE) {
                ui_update(&gs.ui, &gs);
                game_state_update(&gs, dt);

                // ── Déclenchement extraction endless ──────────────
                if (gs.is_endless && !gs.endless_pending_extract &&
                    gs.phase == PHASE_PREP &&
                    gs.wave_manager.number > 0 &&
                    gs.wave_manager.number % 10 == 0)
                {
                    gs.endless_pending_extract = 1;
                    interlude = INTER_EXTRACT;
                }
            }

            // ── Game over ─────────────────────────────────────────
            if (gs.phase == PHASE_GAMEOVER && interlude == INTER_NONE) {
                // En endless : 0 ferraille si pas extrait
                if (gs.is_endless) {
                    meta_endless_end(&gs.meta,
                                     gs.wave_manager.number,
                                     gs.endless_multiplier, 0);
                    menu_refresh_slots(&menu);
                }
                if (IsKeyPressed(KEY_SPACE)) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    audio_stop_music();
                }
            }

            // ── Fin de stage campagne ─────────────────────────────
            if (gs.is_campaign && gs.phase == PHASE_PREP &&
                gs.wave_manager.number >= 1 &&
                interlude == INTER_NONE && IsKeyPressed(KEY_TAB))
            {
                int last   = (gs.campaign_stage == CAMPAIGN_STAGES - 1);
                int earned = meta_end_of_campaign_stage(
                    &gs.meta, gs.wave_manager.number,
                    gs.kills, gs.gold, gs.campaign_stage);
                interlude       = INTER_STAGE_WIN;
                interlude_scrap = earned;
                interlude_last  = last;
                save_write(&gs, active_slot);
            }

            // ── Interlude campagne ────────────────────────────────
            if (interlude == INTER_STAGE_WIN && IsKeyPressed(KEY_SPACE)) {
                if (interlude_last) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = MENU_TITLE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                } else {
                    game_next_campaign_stage(&gs);
                    save_write(&gs, active_slot);
                    interlude = INTER_NONE;
                }
            }

            // ── Interlude extraction endless ──────────────────────
            if (interlude == INTER_EXTRACT) {
                // EXTRAIRE — [E]
                if (IsKeyPressed(KEY_E)) {
                    meta_endless_end(&gs.meta,
                                     gs.wave_manager.number,
                                     gs.endless_multiplier, 1);
                    menu_refresh_slots(&menu);
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu.paused = 0;
                    menu.screen = MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                }
                // CONTINUER — [ESPACE]
                else if (IsKeyPressed(KEY_SPACE)) {
                    // Monte le multiplicateur : ×1 → ×1.5 → ×2 → ×3
                    gs.endless_multiplier *= 1.5f;
                    if (gs.endless_multiplier > 6.0f) gs.endless_multiplier = 6.0f;
                    gs.endless_series++;
                    gs.endless_pending_extract = 0;

                    // Change de thème toutes les 10 vagues
                    ThemeID next_theme = (ThemeID)((gs.map.theme + 1) % THEME_COUNT);
                    game_init_map(&gs, next_theme);
                    audio_play_theme_music(gs.map.theme);
                    interlude = INTER_NONE;
                }
            }

            // ── Rendu ─────────────────────────────────────────────
            ClearBackground(theme_get(gs.map.theme)->palette.bg);
            render_map(&gs.map);
            render_spawn_exclusion_zones(&gs.map);
            render_bases(&gs.map);
            render_deposits(&gs.map);
            render_paths(&gs.enemy_paths);
            render_towers(&gs.towers);
            render_units(&gs.units);
            render_enemies(&gs.enemies);
            render_projectiles(&gs.towers);
            ui_render(&gs.ui, &gs);

            // Badge campagne
            if (gs.is_campaign) {
                int themes[CAMPAIGN_STAGES];
                meta_campaign_theme_order(gs.campaign_order_seed, themes);
                const Theme *sth = theme_get((ThemeID)themes[gs.campaign_stage]);
                DrawText(TextFormat("CAMPAGNE %d  |  Stage %d/%d  |  %s",
                             gs.campaign_num+1, gs.campaign_stage+1,
                             CAMPAIGN_STAGES, sth->name),
                         8, 8, 11, (Color){200,180,120,210});
                if (gs.phase == PHASE_PREP && gs.wave_manager.number >= 1)
                    DrawText("[TAB] Terminer ce stage",
                             8, 22, 10, (Color){100,180,80,200});
            }

            // Badge endless
            if (gs.is_endless) {
                int score = (int)((float)gs.wave_manager.number
                                  * gs.endless_multiplier * 10.0f);
                DrawText(TextFormat("ENDLESS  |  Serie %d  |  x%.1f  |  Score %d",
                             gs.endless_series+1,
                             gs.endless_multiplier, score),
                         8, 8, 11, (Color){140,200,240,210});

                // Prochaine extraction dans N vagues
                int next_extr = 10 - (gs.wave_manager.number % 10);
                if (next_extr == 10 && gs.wave_manager.number > 0) next_extr = 0;
                if (next_extr > 0)
                    DrawText(TextFormat("Extraction dans %d vague(s)", next_extr),
                             8, 22, 9, (Color){80,140,180,180});
            }

            if (gs.phase == PHASE_GAMEOVER)
                render_gameover(&gs);

            if (interlude == INTER_STAGE_WIN)
                interlude_render(&gs, interlude_scrap, interlude_last);

            // Fenêtre extraction endless
            if (interlude == INTER_EXTRACT)
                draw_extract_screen(&gs, VIRT_W, VIRT_H);

            if (menu.paused) {
                MenuAction pact = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);
                if (pact.save_and_quit == 2 && active_slot >= 0)
                    save_write(&gs, active_slot);
                if (pact.save_and_quit == 1) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
                    audio_stop_music();
                }
                if (pact.quit_app) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    EndTextureMode(); break;
                }
                if (pact.toggle_fs == 1) ToggleFullscreen();
                if (pact.toggle_fs == 2 && !IsWindowFullscreen())
                    window_apply_size(menu.opts.win_width, menu.opts.win_height);
            }
        }

        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
            (Rectangle){0,0,(float)canvas.texture.width,
                             -(float)canvas.texture.height},
            (Rectangle){cox, coy, VIRT_W*csc, VIRT_H*csc},
            (Vector2){0,0}, 0.0f, WHITE);
        EndDrawing();
    }

    if (screen == SCREEN_GAME && active_slot >= 0 &&
        gs.phase != PHASE_GAMEOVER && interlude == INTER_NONE)
        save_write(&gs, active_slot);

    menu_cleanup(&menu);
    audio_shutdown();
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
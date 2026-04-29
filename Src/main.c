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
#include "combat/tower.h"
#include "combat/unit.h"
#include "combat/enemy.h"
#include "meta/meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ════════════════════════════════════════════════════
// ÉTATS DU JEU
// ════════════════════════════════════════════════════
typedef enum { SCREEN_MENU = 0, SCREEN_GAME } Screen;
typedef enum { INTER_NONE = 0, INTER_STAGE_WIN } InterludeState;

// ════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════
int main(void) {
    InitWindow(VIRT_W, VIRT_H, "RUST BASTION");
    
    Image icon = LoadImage("assets/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    window_center();

    RenderTexture2D canvas = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    RENDER_SCALE = 1.0f;

    GameState gs           = {0};
    Screen    screen       = SCREEN_MENU;
    int       active_slot  = -1;
    InterludeState interlude     = INTER_NONE;
    int            interlude_scrap = 0;
    int            interlude_last  = 0;

    game_state_init(&gs);

    AppOptions opts = { .win_width=VIRT_W, .win_height=VIRT_H };
    MenuState menu  = {0};
    menu_init(&menu, &opts);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float csc, cox, coy;
        canvas_compute(sw, sh, &csc, &cox, &coy);
        canvas_set_mouse_offset(cox, coy, csc);
        menu_set_mouse_offset(cox, coy, csc);

        BeginTextureMode(canvas);

        // ════════════════════════════════════════════════════
        if (screen == SCREEN_MENU) {
        // ════════════════════════════════════════════════════

            menu_update(&menu, &gs.meta);
            ClearBackground((Color){8,5,3,255});
            MenuAction act = menu_render_and_act(&menu, &gs.meta, VIRT_W, VIRT_H);

            if (act.quit_app) { EndTextureMode(); break; }
            if (act.toggle_fs == 1) ToggleFullscreen();
            if (act.toggle_fs == 2 && !IsWindowFullscreen())
                window_apply_size(menu.opts.win_width, menu.opts.win_height);

            if (act.start_arcade) {
                active_slot = act.new_slot;
                game_init_arcade(&gs, act.new_theme, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
            }

            if (act.start_campaign) {
                active_slot = act.new_slot;
                game_init_campaign(&gs, gs.meta.campaigns_completed, active_slot);
                save_write(&gs, active_slot);
                menu.screen = MENU_TITLE;
                screen = SCREEN_GAME;
            }

            if (act.go_game && !act.start_arcade && !act.start_campaign) {
                int slot = act.resume_slot;
                if (save_read(&gs, slot)) {
                    active_slot = slot;
                    menu.screen = MENU_TITLE;
                    screen = SCREEN_GAME;
                    printf("Reprise slot %d\n", slot);
                } else {
                    active_slot = slot;
                    game_init_arcade(&gs, THEME_COUNT, slot);
                    save_write(&gs, active_slot);
                    screen = SCREEN_GAME;
                }
            }

        // ════════════════════════════════════════════════════
        } else {   // SCREEN_GAME
        // ════════════════════════════════════════════════════

            if (IsKeyPressed(KEY_ESCAPE)) {
                menu.paused ^= 1;
                menu.screen = menu.paused ? MENU_PAUSE : MENU_TITLE;
            }

            // Mise à jour jeu (hors pause et interlude)
            if (!menu.paused && interlude == INTER_NONE) {
                ui_update(&gs.ui, &gs);
                game_state_update(&gs, dt);
            }

            // ── Game over ─────────────────────────────────────
            if (gs.phase == PHASE_GAMEOVER && interlude == INTER_NONE) {
                if (IsKeyPressed(KEY_SPACE)) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                }
            }

            // ── Fin de stage campagne (TAB en phase PREP) ─────
            if (gs.is_campaign &&
                gs.phase == PHASE_PREP &&
                gs.wave_manager.number >= 1 &&
                interlude == INTER_NONE &&
                IsKeyPressed(KEY_TAB))
            {
                int last = (gs.campaign_stage == CAMPAIGN_STAGES - 1);
                int earned = meta_end_of_campaign_stage(
                    &gs.meta,
                    gs.wave_manager.number,
                    gs.kills,
                    gs.gold,
                    gs.campaign_stage);
                interlude       = INTER_STAGE_WIN;
                interlude_scrap = earned;
                interlude_last  = last;
                save_write(&gs, active_slot);
            }

            // ── Interlude ─────────────────────────────────────
            if (interlude == INTER_STAGE_WIN && IsKeyPressed(KEY_SPACE)) {
                if (interlude_last) {
                    if (active_slot >= 0) save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = MENU_TITLE;
                    screen    = SCREEN_MENU;
                    interlude = INTER_NONE;
                } else {
                    game_next_campaign_stage(&gs);
                    save_write(&gs, active_slot);
                    interlude = INTER_NONE;
                }
            }

            // ── Rendu ─────────────────────────────────────────
            ClearBackground(theme_get(gs.map.theme)->palette.bg);
            render_map(&gs.map);
            render_paths(&gs.enemy_paths);
            render_towers(&gs.towers);
            render_units(&gs.units);
            render_enemies(&gs.enemies);
            render_projectiles(&gs.towers);
            ui_render(&gs.ui, &gs);

            // Badge campagne
            if (gs.is_campaign) {
                int themes[CAMPAIGN_STAGES];
                meta_campaign_theme_order(gs.campaign_num, themes);
                const Theme *sth = theme_get((ThemeID)themes[gs.campaign_stage]);
                DrawText(TextFormat("CAMPAGNE %d  |  Stage %d/%d  |  %s",
                             gs.campaign_num+1, gs.campaign_stage+1,
                             CAMPAIGN_STAGES, sth->name),
                         8, 8, 11, (Color){200,180,120,210});
                if (gs.phase == PHASE_PREP && gs.wave_manager.number >= 1)
                    DrawText("[TAB] Terminer ce stage et progresser",
                             8, 22, 10, (Color){100,180,80,200});
            }

            if (gs.phase == PHASE_GAMEOVER)
                render_gameover(&gs);

            if (interlude == INTER_STAGE_WIN)
                interlude_render(&gs, interlude_scrap, interlude_last);

            // ── Pause overlay ─────────────────────────────────
            if (menu.paused) {
                MenuAction pact = menu_render_and_act(
                    &menu, &gs.meta, VIRT_W, VIRT_H);
                if (pact.save_and_quit == 2 && active_slot >= 0) {
                    save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                }
                if (pact.save_and_quit == 1) {
                    if (active_slot >= 0) save_write(&gs, active_slot);
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = gs.is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                    screen = SCREEN_MENU;
                    interlude = INTER_NONE;
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

    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
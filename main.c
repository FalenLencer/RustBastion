#include "raylib.h"
#include "raymath.h"
#include "game_state.h"
#include "map_gen.h"
#include "pathfinding.h"
#include "renderer.h"
#include "theme.h"
#include "tower.h"
#include "unit.h"
#include "enemy.h"
#include "meta.h"
#include "ui.h"
#include "menu.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>

// ════════════════════════════════════════════════════
// ÉCRANS DE L'APPLICATION
// ════════════════════════════════════════════════════
typedef enum {
    SCREEN_MENU = 0,   // menu principal (slots, upgrades, options)
    SCREEN_GAME,       // partie en cours
} Screen;

// ════════════════════════════════════════════════════
// HELPERS : création / chargement de partie
// ════════════════════════════════════════════════════

// Génère une nouvelle carte jusqu'à avoir au moins un chemin valide
static void new_game(GameState *gs, ThemeID theme) {
    int seed;
    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, 20, theme);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
    } while (gs->enemy_paths.count == 0 || gs->map.path_count == 0);

    float base_px = gs->map.paths[0].base.x * TILE_SIZE + TILE_SIZE / 2.0f;
    float base_py = gs->map.paths[0].base.y * TILE_SIZE + TILE_SIZE / 2.0f;
    unit_pool_init(&gs->units, base_px, base_py);

    ui_init(&gs->ui);

    printf("Nouvelle partie — seed=%d chemins=%d theme=%d\n",
           seed, gs->enemy_paths.count, gs->map.theme);
}

// Applique la résolution choisie dans les options
static void apply_window_size(int w, int h) {
    SetWindowSize(w, h);
    // Recentre sur le moniteur principal
    int mw = GetMonitorWidth(0);
    int mh = GetMonitorHeight(0);
    Vector2 mpos = GetMonitorPosition(0);
    SetWindowPosition(
        (int)(mpos.x + (mw - w) / 2),
        (int)(mpos.y + (mh - h) / 2)
    );
}

// ════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════
int main(void) {
    // Taille de fenêtre par défaut
    const int DEFAULT_W = MAP_W * TILE_SIZE;          // 1120
    const int DEFAULT_H = MAP_H * TILE_SIZE + UI_HUD_HEIGHT; // 770

    InitWindow(DEFAULT_W, DEFAULT_H, "RUST BASTION");
    SetTargetFPS(60);

    // Centrage initial
    {
        int mw = GetMonitorWidth(0);
        int mh = GetMonitorHeight(0);
        Vector2 mpos = GetMonitorPosition(0);
        SetWindowPosition(
            (int)(mpos.x + (mw - DEFAULT_W) / 2),
            (int)(mpos.y + (mh - DEFAULT_H) / 2)
        );
    }

    // ── Init état global ──────────────────────────────────────
    GameState gs     = {0};
    Screen    screen = SCREEN_MENU;
    int       active_slot = -1;   // slot actuellement en jeu (-1 = aucun)

    game_state_init(&gs);

    // ── Init menu ─────────────────────────────────────────────
    AppOptions opts = {
        .fullscreen = 0,
        .win_width  = DEFAULT_W,
        .win_height = DEFAULT_H,
    };
    MenuState menu = {0};
    menu_init(&menu, &opts);

    // ── Boucle principale ─────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ════════════════════════════════════════════════════
        // ÉCRAN MENU
        // ════════════════════════════════════════════════════
        if (screen == SCREEN_MENU) {

            menu_update(&menu, &gs.meta);

            BeginDrawing();

            MenuAction act = menu_render_and_act(&menu, &gs.meta,
                                                  GetScreenWidth(),
                                                  GetScreenHeight());
            EndDrawing();

            // ── Interprète les actions ────────────────────────

            // Quitter l'application
            if (act.quit_app) break;

            // Basculer plein écran
            if (act.toggle_fs == 1) {
                if (menu.opts.fullscreen)
                    ToggleFullscreen();
                else
                    ToggleFullscreen();
            }
            // Changer de résolution fenêtrée
            if (act.toggle_fs == 2) {
                if (!IsWindowFullscreen())
                    apply_window_size(menu.opts.win_width,
                                      menu.opts.win_height);
            }

            // Nouvelle partie
            if (act.start_new) {
                game_state_init(&gs);
                new_game(&gs, menu.new_theme);
                active_slot = menu.new_slot;
                // Sauvegarde initiale du slot
                save_write(&gs, active_slot);
                menu.screen = MENU_MAIN;
                screen = SCREEN_GAME;
            }

            // Reprendre une partie sauvegardée
            if (act.go_game && !act.start_new) {
                int slot = act.resume_slot;
                if (save_read(&gs, slot)) {
                    active_slot = slot;
                    menu.screen = MENU_MAIN;
                    screen = SCREEN_GAME;
                    printf("Partie chargée — slot %d\n", slot);
                } else {
                    // Sauvegarde corrompue — nouvelle partie dans ce slot
                    game_state_init(&gs);
                    new_game(&gs, THEME_COUNT);
                    active_slot = slot;
                    save_write(&gs, active_slot);
                    screen = SCREEN_GAME;
                }
            }

        // ════════════════════════════════════════════════════
        // ÉCRAN JEU
        // ════════════════════════════════════════════════════
        } else {

            // ── Touche ECHAP → bascule pause ─────────────────
            if (IsKeyPressed(KEY_ESCAPE)) {
                menu.paused ^= 1;
                menu.screen = menu.paused ? MENU_PAUSE : MENU_MAIN;
            }

            // ── Mise à jour jeu (seulement hors pause) ───────
            if (!menu.paused) {
                ui_update(&gs.ui, &gs);
                game_state_update(&gs, dt);
            }

            // ── Game over → retour menu ───────────────────────
            if (gs.phase == PHASE_GAMEOVER) {
                if (IsKeyPressed(KEY_SPACE)) {
                    // Supprime le slot (partie terminée)
                    if (active_slot >= 0)
                        save_delete(active_slot);
                    active_slot = -1;
                    menu_refresh_slots(&menu);
                    menu.paused = 0;
                    menu.screen = MENU_MAIN;
                    screen = SCREEN_MENU;
                }
            }

            // ── Rendu jeu ─────────────────────────────────────
            BeginDrawing();
                ClearBackground(theme_get(gs.map.theme)->palette.bg);

                render_map(&gs.map);
                render_paths(&gs.enemy_paths);
                render_towers(&gs.towers);
                render_units(&gs.units);
                render_enemies(&gs.enemies);
                render_projectiles(&gs.towers);

                ui_render(&gs.ui, &gs);

                if (gs.phase == PHASE_GAMEOVER)
                    render_gameover(&gs);

                // ── Overlay pause (par-dessus tout) ──────────
                if (menu.paused) {
                    MenuAction pact = menu_render_and_act(
                        &menu, &gs.meta,
                        GetScreenWidth(), GetScreenHeight());

                    // Reprendre depuis le menu pause
                    if (!menu.paused)   // draw_pause a mis paused=0
                        (void)0;        // déjà géré dans draw_pause

                    // Sauvegarder seulement
                    if (pact.save_and_quit == 2 && active_slot >= 0) {
                        save_write(&gs, active_slot);
                        menu_refresh_slots(&menu);
                    }

                    // Sauvegarder + retour menu
                    if (pact.save_and_quit == 1) {
                        if (active_slot >= 0)
                            save_write(&gs, active_slot);
                        menu_refresh_slots(&menu);
                        menu.paused = 0;
                        menu.screen = MENU_MAIN;
                        screen = SCREEN_MENU;
                    }

                    // Quitter sans sauvegarder
                    if (pact.quit_app) {
                        if (active_slot >= 0)
                            save_write(&gs, active_slot);
                        break;
                    }

                    // Options depuis la pause
                    if (pact.toggle_fs == 1)
                        ToggleFullscreen();
                    if (pact.toggle_fs == 2 && !IsWindowFullscreen())
                        apply_window_size(menu.opts.win_width,
                                          menu.opts.win_height);
                }

            EndDrawing();
        }
    }

    // Sauvegarde automatique à la fermeture si partie en cours
    if (screen == SCREEN_GAME && active_slot >= 0 &&
        gs.phase != PHASE_GAMEOVER)
        save_write(&gs, active_slot);

    CloseWindow();
    return 0;
}
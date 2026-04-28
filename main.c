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
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    SCREEN_META = 0,
    SCREEN_GAME,
} Screen;

static void new_game(GameState *gs) {
    int seed;
    do {
        seed = GetRandomValue(1, 99999);
        generate_map(&gs->map, seed, 20, THEME_COUNT);
        astar_all(&gs->map, &gs->enemy_paths);
        pathset_apply(&gs->map, &gs->enemy_paths);
    } while (gs->enemy_paths.count == 0 || gs->map.path_count == 0);

    float base_px = gs->map.paths[0].base.x * TILE_SIZE + TILE_SIZE / 2.0f;
    float base_py = gs->map.paths[0].base.y * TILE_SIZE + TILE_SIZE / 2.0f;
    unit_pool_init(&gs->units, base_px, base_py);

    // Init UI après que la carte est générée
    ui_init(&gs->ui);

    printf("Carte seed=%d : %d chemin(s) | theme=%d\n",
           seed, gs->enemy_paths.count, gs->map.theme);
}

int main(void) {
    InitWindow(MAP_W * TILE_SIZE, MAP_H * TILE_SIZE + UI_HUD_HEIGHT,
               "RUST BASTION");
    SetTargetFPS(60);

    // ── Positionnement multi-écrans ───────────────────────────
    int targetMonitor = 0;
    int monitorCount  = GetMonitorCount();
    if (targetMonitor >= monitorCount) targetMonitor = 0;

    Vector2 monitorPos    = GetMonitorPosition(targetMonitor);
    int     monitorWidth  = GetMonitorWidth(targetMonitor);
    int     monitorHeight = GetMonitorHeight(targetMonitor);
    int     windowWidth   = MAP_W * TILE_SIZE;
    int     windowHeight  = MAP_H * TILE_SIZE + UI_HUD_HEIGHT;
    SetWindowPosition(
        (int)(monitorPos.x + (monitorWidth  - windowWidth)  / 2),
        (int)(monitorPos.y + (monitorHeight - windowHeight) / 2)
    );

    // ── Init état global ──────────────────────────────────────
    GameState gs     = {0};
    Screen    screen = SCREEN_META;
    int       sel_upg = 0;

    game_state_init(&gs);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ════════════════════════════════════════════════════
        // ÉCRAN MÉTA
        // ════════════════════════════════════════════════════
        if (screen == SCREEN_META) {

            // Navigation clavier
            if (IsKeyPressed(KEY_UP))
                sel_upg = (sel_upg - 1 + UPGRADE_COUNT) % UPGRADE_COUNT;
            if (IsKeyPressed(KEY_DOWN))
                sel_upg = (sel_upg + 1) % UPGRADE_COUNT;

            // Achat clavier
            if (IsKeyPressed(KEY_ENTER))
                meta_upgrade(&gs.meta, sel_upg);

            // Navigation souris — survol des upgrades
            Vector2 mouse = GetMousePosition();
            int sw = GetScreenWidth();
            for (int i = 0; i < UPGRADE_COUNT; i++) {
                // Chaque ligne fait 24px, commence à y=130
                Rectangle row = {40, 130 + i * 24, (float)(sw - 80), 22};
                if (CheckCollisionPointRec(mouse, row)) {
                    sel_upg = i;
                    // Clic souris sur une amélioration
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                        meta_upgrade(&gs.meta, sel_upg);
                }
            }

            // Lance une partie — clavier ou souris sur bouton ESPACE
            if (IsKeyPressed(KEY_SPACE)) {
                game_state_init(&gs);
                new_game(&gs);
                screen = SCREEN_GAME;
            }

            // Bouton "JOUER" cliquable à la souris
            Rectangle play_btn = {
                (float)(sw/2 - 100),
                (float)(130 + UPGRADE_COUNT*24 + 30),
                200, 32
            };
            if (CheckCollisionPointRec(mouse, play_btn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                game_state_init(&gs);
                new_game(&gs);
                screen = SCREEN_GAME;
            }

            BeginDrawing();
                ClearBackground((Color){8, 5, 3, 255});
                render_meta_menu(&gs.meta, sel_upg);

                // Bouton JOUER dessiné ici pour avoir la position
                int sh = GetScreenHeight();
                (void)sh;
                DrawRectangleRec(play_btn, (Color){13, 61, 26, 255});
                DrawRectangleLinesEx(play_btn, 1.5f,
                                     (Color){39, 174, 96, 255});
                DrawText("▶  JOUER",
                         (int)(play_btn.x + 50),
                         (int)(play_btn.y + 8),
                         16, (Color){46, 204, 113, 255});
            EndDrawing();

        // ════════════════════════════════════════════════════
        // ÉCRAN DE JEU
        // ════════════════════════════════════════════════════
        } else {

            // ── Mise à jour UI (clics, hover, placement) ─────
            ui_update(&gs.ui, &gs);

            // ── Mise à jour état du jeu ───────────────────────
            game_state_update(&gs, dt);

            // ── Retour au menu après game over ────────────────
            if (gs.phase == PHASE_GAMEOVER && IsKeyPressed(KEY_SPACE))
                screen = SCREEN_META;

            // ── Rendu ─────────────────────────────────────────
            BeginDrawing();
                ClearBackground(theme_get(gs.map.theme)->palette.bg);

                // Carte et entités
                render_map(&gs.map);
                render_paths(&gs.enemy_paths);
                render_towers(&gs.towers);
                render_units(&gs.units);
                render_enemies(&gs.enemies);
                render_projectiles(&gs.towers);

                // UI (HUD + boutons + tooltip + preview)
                ui_render(&gs.ui, &gs);

                // Écran game over par-dessus tout
                if (gs.phase == PHASE_GAMEOVER)
                    render_gameover(&gs);
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}
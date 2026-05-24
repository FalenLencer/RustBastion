/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/* ════════════════════════════════════════════════════════════════
   main.c — RUST BASTION
   Point d'entrée : initialisation, boucle principale, fermeture.
   Toute la logique métier est dans game/app.c.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"
#include "engine/paths.h"
#include "engine/window.h"
#include "engine/canvas.h"
#include "engine/audio.h"
#include "engine/assets.h"
#include "game/save.h"
#include "game/app.h"
#include "ui/renderer.h"
#include <math.h>

int main(void) {
    /* ── Répertoire de travail ──────────────────────────────────── */
    setup_working_dir();

    /* ── Fenêtre ────────────────────────────────────────────────── */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(VIRT_W, VIRT_H, "RUST BASTION");
    window_disable_vsync();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    window_center();

    /* ── Audio / Assets / Saves ─────────────────────────────────── */
    audio_init();
    assets_load();
    save_init();

    /* ── Icône ──────────────────────────────────────────────────── */
    Image icon = LoadImage("assets/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    /* ── Canvas virtuel (largeur initiale = MAP_W×TILE, sera redimensionné) */
    g_canvas_virt_w_base = VIRT_W;
    g_canvas_virt_h      = VIRT_H;
    g_canvas_virt_w      = VIRT_W;
    g_map_x_off          = 0;
    RenderTexture2D canvas = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);

    /* ── Contexte applicatif ────────────────────────────────────── */
    static AppContext ctx;
    app_init(&ctx);
    SetWindowTitle("RUST BASTION");   // réinitialise le titre après tout le chargement

    /* ════════════════════════════════════════════════════════════
       BOUCLE PRINCIPALE
       ════════════════════════════════════════════════════════════ */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        audio_update();

        /* Canvas dynamique : hauteur = g_canvas_virt_h, largeur adaptée au ratio fenêtre */
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (sh < 1) sh = 1;
        float scale  = (float)sh / g_canvas_virt_h;
        int   virt_w = (int)ceilf((float)sw / scale);
        if (virt_w < g_canvas_virt_w_base) virt_w = g_canvas_virt_w_base;

        g_map_x_off     = (virt_w - g_canvas_virt_w_base) / 2;
        g_canvas_virt_w = virt_w;

        /* Redimensionne le canvas si le ratio ou la hauteur ont changé */
        if (virt_w != (int)canvas.texture.width ||
            g_canvas_virt_h != (int)canvas.texture.height) {
            UnloadRenderTexture(canvas);
            canvas = LoadRenderTexture(virt_w, g_canvas_virt_h);
            SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
        }

        /* Transformation souris : même échelle x et y (uniforme) */
        canvas_set_mouse_offset(0.0f, 0.0f, scale, scale);
        menu_set_mouse_offset  (0.0f, 0.0f, scale, scale);

        /* Frame */
        BeginTextureMode(canvas);
        int keep_running = app_update(&ctx, dt);
        EndTextureMode();

        if (!keep_running) break;

        /* Présentation canvas → écran physique (remplit toute la fenêtre) */
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
            (Rectangle){0, 0,
                        (float) canvas.texture.width,
                        -(float)canvas.texture.height},
            (Rectangle){0, 0, (float)sw, (float)sh},
            (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    /* ── Nettoyage ──────────────────────────────────────────────── */
    app_save_on_exit(&ctx);
    app_cleanup(&ctx);
    audio_shutdown();
    assets_unload();
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}

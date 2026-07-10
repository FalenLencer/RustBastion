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
#include "ui/render3d.h"     /* rendu 3D des tours (pré-passe hors canvas) */
#include "ui/render3d_units.h" /* rendu 3D des unités (mode de vue 3D) */
#include "ui/render3d_enemies.h" /* rendu 3D des ennemis (mode de vue 3D) */
#include "net/net_relay.h"   /* mode serveur relais (headless) */
#include <math.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    /* ── Mode SERVEUR RELAIS (headless, sans raylib) ─────────────
       Usage : rustbastion --relay [port]  (défaut 47777).
       À lancer sur un PC toujours allumé dont le port est déjà
       redirigé par la box (ex. celui du serveur Minecraft). */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--relay") == 0) {
            int port = (i + 1 < argc) ? atoi(argv[i + 1]) : 0;
            if (port <= 0 || port > 65535) port = 47777;
            return net_relay_run(port);
        }
    }

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
    render3d_init();          /* charge les GLB des tours (repli sprites si absents) */
    render3d_units_init();    /* charge les GLB des unités (mode de vue 3D) */
    render3d_enemies_init();  /* charge les GLB des ennemis (mode de vue 3D) */
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

        /* ── Échelle UNIFORME (pixels carrés quelle que soit la fenêtre) ──
           • Fenêtre assez large  → on cale sur la hauteur, la largeur
             virtuelle s'étend pour remplir les côtés (pas de bandes).
           • Fenêtre trop étroite → on cale sur la largeur (carte entière
             visible) et on centre verticalement (bandes noires).
           Dans les deux cas l'échelle X = échelle Y : aucune déformation,
           et la carte, les unités et les tours suivent exactement.        */
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (sw < 1) sw = 1;
        if (sh < 1) sh = 1;

        float scale;
        int   virt_w;
        float fit_h = (float)sh / (float)g_canvas_virt_h;
        int   wfit  = (int)ceilf((float)sw / fit_h);
        if (wfit >= g_canvas_virt_w_base) {
            scale  = fit_h;          /* large : cale sur la hauteur  */
            virt_w = wfit;
        } else {
            scale  = (float)sw / (float)g_canvas_virt_w_base;  /* étroit : cale sur la largeur */
            virt_w = g_canvas_virt_w_base;
        }

        g_map_x_off     = (virt_w - g_canvas_virt_w_base) / 2;
        g_canvas_virt_w = virt_w;

        /* Redimensionne le canvas si le ratio ou la hauteur ont changé */
        if (virt_w != (int)canvas.texture.width ||
            g_canvas_virt_h != (int)canvas.texture.height) {
            UnloadRenderTexture(canvas);
            canvas = LoadRenderTexture(virt_w, g_canvas_virt_h);
            SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
        }

        /* Rectangle de présentation centré, échelle uniforme */
        float pres_w = (float)virt_w * scale;
        float pres_h = (float)g_canvas_virt_h * scale;
        float pres_x = ((float)sw - pres_w) * 0.5f;
        float pres_y = ((float)sh - pres_h) * 0.5f;

        /* Transformation souris : décalage de présentation + échelle uniforme */
        canvas_set_mouse_offset(pres_x, pres_y, scale, scale);
        menu_set_mouse_offset  (pres_x, pres_y, scale, scale);

        /* Pré-passe 3D : rend les tours articulées (visée + tir) en textures
           AVANT la passe canvas (raylib interdit les RenderTexture imbriqués).
           Inutile en MODE HÉROS : la scène 3D dessine les modèles en direct. */
        if (ctx.screen == SCREEN_GAME && !ctx.hero_mode) {
            render3d_prepass(&ctx.gs.towers, &ctx.gs.enemies);
            if (g_units_3d) {
                render3d_units_prepass(&ctx.gs.units, &ctx.gs.enemies);   /* mode de vue 3D */
                render3d_enemies_prepass(&ctx.gs.enemies);
            }
        }

        /* Frame */
        BeginTextureMode(canvas);
        int keep_running = app_update(&ctx, dt);
        EndTextureMode();

        if (!keep_running) break;

        /* Présentation canvas → écran physique (centré, sans déformation) */
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
            (Rectangle){0, 0,
                        (float) canvas.texture.width,
                        -(float)canvas.texture.height},
            (Rectangle){pres_x, pres_y, pres_w, pres_h},
            (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    /* ── Nettoyage ──────────────────────────────────────────────── */
    app_save_on_exit(&ctx);
    app_cleanup(&ctx);
    render3d_shutdown();
    render3d_units_shutdown();
    render3d_enemies_shutdown();
    audio_shutdown();
    assets_unload();
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}

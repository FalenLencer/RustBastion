/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_tutorial.c ─ Tutoriel guidé interactif.
 *  Chaque étape d'action verrouille l'input tant que le joueur n'a pas
 *  accompli l'action attendue, avec guidage visuel et surbrillance ciblée.
 *  Le tutoriel reste une surcouche : il ne change pas la simulation.
 */
#include "app_tutorial.h"
#include "../ui/renderer.h"    // g_map_x_off, g_canvas_virt_w_base, g_map_zoom
#include "../ui/ui_utils.h"    // dtxt / mtxt / fh
#include "../ui/ui_anim.h"     // ui_dt / ea_out_cubic / ea_out_back
#include "../ui/hud.h"         // ToolID / rectangles HUD
#include "../combat/tower.h"   // Tower
#include "raylib.h"
#include <math.h>
#include <stdio.h>

typedef enum { TUT_INFO, TUT_ACTION } TutMode;
typedef struct {
    TutMode mode;
    const char *title;
    const char *l1;
    const char *l2;
    int (*is_done)(AppContext*);
    Rectangle (*target)(AppContext*);
    int gate_input;
    const char *deny_msg;
} TutStep;

static int tutorial_is_select_tower(AppContext *ctx);
static int tutorial_is_place_tower(AppContext *ctx);
static int tutorial_is_wave(AppContext *ctx);
static int tutorial_is_kill(AppContext *ctx);
static int tutorial_is_upgrade(AppContext *ctx);
static int tutorial_is_pause(AppContext *ctx);
static int tutorial_is_zoom(AppContext *ctx);
static Rectangle tutorial_target_tool_button(AppContext *ctx);
static Rectangle tutorial_target_map(AppContext *ctx);
static Rectangle tutorial_target_wave(AppContext *ctx);
static Rectangle tutorial_target_upgrade(AppContext *ctx);

#define TUT_PW 560
#define TUT_PH 122
#define TUT_Y  10
#define TUT_DENY_DURATION 1.0f
#define TUT_ACTION_HINT_TIME 8.0f
#define TUT_SPOTLIGHT_ALPHA 150
#define TUT_RING_THICK_MIN 1.6f
#define TUT_RING_THICK_MAX 3.2f
#define TUT_ARROW_LEN 56
#define TUT_SKIP_W 82
#define TUT_SKIP_H 24
#define TUT_SKIP_PX 10
#define TUT_SKIP_PY 10

static const TutStep TUT_STEPS[] = {
    { TUT_INFO,  "Bienvenue, Commandant",
      "Defends ta base contre des vagues d'ennemis.",
      "Suis les indications. Clique SUIVANT pour commencer.",
      NULL, NULL, 0, NULL },
    { TUT_ACTION, "1. Selectionne une tour",
      "Clique sur une tour en bas de l'ecran pour l'armer.",
      "La cible est mise en valeur, et le reste est verrouille.",
      tutorial_is_select_tower, tutorial_target_tool_button, 1,
      "Clique sur une tour en bas" },
    { TUT_ACTION, "2. Pose-la sur le chemin",
      "Clique sur la carte pour poser la tour a l'endroit voulu.",
      "Le clic hors carte reste refuse pendant cette etape.",
      tutorial_is_place_tower, tutorial_target_map, 1,
      "Clique sur la carte" },
    { TUT_ACTION, "3. Lance la vague",
      "Clique le bouton LANCER LA VAGUE quand tu es pret.",
      "Tes tours tirent automatiquement sur les ennemis.",
      tutorial_is_wave, tutorial_target_wave, 1,
      "Clique sur le bouton vague" },
    { TUT_ACTION, "4. Elimine un ennemi",
      "Tes tours tirent seules : observe le combat et laisse-les agir.",
      "Le premier ennemi elimine fait avancer l'etape.",
      tutorial_is_kill, NULL, 0, NULL },
    { TUT_ACTION, "5. Ameliore une tour",
      "Clique ta tour, puis degats, portee ou cadence.",
      "Une amelioration quelconque valide l'etape.",
      tutorial_is_upgrade, tutorial_target_upgrade, 1,
      "Clique sur une tour ou ses boutons d'amelioration" },
    { TUT_ACTION, "6. Pause tactique",
      "Appuie sur ESPACE pour figer le jeu et reflechir.",
      "La pause tactique valide cette etape.",
      tutorial_is_pause, NULL, 0, NULL },
    { TUT_ACTION, "7. Zoom",
      "Utilise la molette pour zoomer la carte.",
      "Un changement de zoom valide cette etape.",
      tutorial_is_zoom, NULL, 0, NULL },
    { TUT_INFO,  "Astuce : minerais",
      "Les ouvriers collectent des minerais (compteur Mat).",
      "Applique un minerai a une tour : enorme bonus de degats !",
      NULL, NULL, 0, NULL },
    { TUT_INFO,  "Tu es pret !",
      "Tiens le plus longtemps possible.",
      "Clique TERMINER pour continuer a jouer librement.",
      NULL, NULL, 0, NULL },
};

#define TUT_COUNT ((int)(sizeof(TUT_STEPS) / sizeof(TUT_STEPS[0])))

static float g_tut_accum = 0.0f;
static float g_tut_deny_timer = 0.0f;
static int   g_tut_deny_active = 0;
static int   g_tut_prev_step = -1;
static int   g_tut_pause_seen = 0;
static float g_tut_zoom_start = 1.0f;
static int   g_tut_upgrade_dmg = 0;
static int   g_tut_upgrade_range = 0;
static int   g_tut_upgrade_rate = 0;
static float g_tut_hint_timer = 0.0f;

static Rectangle tutorial_btn_rect(void) {
    int cx = g_map_x_off + g_canvas_virt_w_base / 2;
    return (Rectangle){ (float)(cx - 90), (float)(TUT_Y + TUT_PH - 40), 180.0f, 30.0f };
}

static Rectangle tutorial_skip_rect(void) {
    return (Rectangle){ (float)TUT_SKIP_PX, (float)TUT_SKIP_PY,
                        (float)TUT_SKIP_W, (float)TUT_SKIP_H };
}

static void tutorial_prepare_step(AppContext *ctx, int step) {
    if (step < 0 || step >= TUT_COUNT) return;
    g_tut_prev_step = step;
    g_tut_deny_timer = 0.0f;
    g_tut_deny_active = 0;
    g_tut_hint_timer = 0.0f;
    g_tut_pause_seen = 0;
    g_tut_zoom_start = g_map_zoom;
    g_tut_upgrade_dmg = 0;
    g_tut_upgrade_range = 0;
    g_tut_upgrade_rate = 0;
    if (step == 5) {
        if (ctx->gs.ui.selection.active && ctx->gs.ui.selection.tower_idx >= 0) {
            const Tower *tw = &ctx->gs.towers.towers[ctx->gs.ui.selection.tower_idx];
            if (tw->active) {
                g_tut_upgrade_dmg = tw->upg_dmg;
                g_tut_upgrade_range = tw->upg_range;
                g_tut_upgrade_rate = tw->upg_rate;
            }
        }
    }
}

static void tutorial_finish(AppContext *ctx) {
    ctx->tutorial_active = 0;
    ctx->gs.wave_manager.suppress_auto = 0;
}

static void tutorial_block_click(AppContext *ctx, const char *msg) {
    ctx->gs.ui.mp_block_click = 1;
    if (msg != NULL) {
        g_tut_deny_active = 1;
        g_tut_deny_timer = TUT_DENY_DURATION;
    }
}

static int tutorial_is_select_tower(AppContext *ctx) {
    return (ctx->gs.ui.selected_tool != TOOL_NONE);
}

static int tutorial_is_place_tower(AppContext *ctx) {
    return (ctx->gs.towers.tower_count >= 1);
}

static int tutorial_is_wave(AppContext *ctx) {
    return (ctx->gs.phase == PHASE_WAVE || ctx->gs.wave_manager.number >= 1);
}

static int tutorial_is_kill(AppContext *ctx) {
    return (ctx->gs.kills >= 1);
}

static int tutorial_is_upgrade(AppContext *ctx) {
    if (!ctx->gs.ui.selection.active || ctx->gs.ui.selection.tower_idx < 0) {
        return 0;
    }
    const Tower *tw = &ctx->gs.towers.towers[ctx->gs.ui.selection.tower_idx];
    if (!tw->active) return 0;
    return (tw->upg_dmg > g_tut_upgrade_dmg ||
            tw->upg_range > g_tut_upgrade_range ||
            tw->upg_rate > g_tut_upgrade_rate);
}

static int tutorial_is_pause(AppContext *ctx) {
    return ctx->tactical_pause || g_tut_pause_seen;
}

static int tutorial_is_zoom(AppContext *ctx) {
    (void)ctx;
    return fabsf(g_map_zoom - g_tut_zoom_start) > 0.0001f;
}

static Rectangle tutorial_target_tool_button(AppContext *ctx) {
    return ctx->gs.ui.tool_btns[TOOL_TOWER_GUN];
}

static Rectangle tutorial_target_map(AppContext *ctx) {
    (void)ctx;
    return (Rectangle){ (float)g_map_x_off, 0.0f,
                        (float)g_canvas_virt_w_base,
                        (float)(g_canvas_virt_h - UI_HUD_HEIGHT) };
}

static Rectangle tutorial_target_wave(AppContext *ctx) {
    return ctx->gs.ui.wave_btn;
}

static Rectangle tutorial_target_upgrade(AppContext *ctx) {
    if (ctx->gs.ui.selection.active && ctx->gs.ui.selection.tower_idx >= 0) {
        return (Rectangle){ ctx->gs.ui.upg_dmg_btn.x, ctx->gs.ui.upg_dmg_btn.y,
                            ctx->gs.ui.upg_dmg_btn.width + ctx->gs.ui.upg_range_btn.width +
                            ctx->gs.ui.upg_rate_btn.width + 2.0f * 8.0f,
                            ctx->gs.ui.upg_dmg_btn.height };
    }
    return tutorial_target_map(ctx);
}

void tutorial_tick(AppContext *ctx) {
    if (!ctx->tutorial_active) return;

    ui_anim_tick();
    g_tut_accum += ui_dt();

    int s = ctx->tutorial_step;
    if (s < 0 || s >= TUT_COUNT) {
        ctx->tutorial_active = 0;
        return;
    }

    if (g_tut_prev_step != s) {
        tutorial_prepare_step(ctx, s);
    }

    const TutStep *st = &TUT_STEPS[s];
    if (st->mode == TUT_ACTION && st->is_done != NULL && st->is_done(ctx)) {
        ctx->tutorial_step++;
        if (ctx->tutorial_step >= TUT_COUNT) {
            tutorial_finish(ctx);
            return;
        }
        tutorial_prepare_step(ctx, ctx->tutorial_step);
        return;
    }

    if (st->mode == TUT_ACTION) {
        g_tut_hint_timer += ui_dt();
        if (ctx->tactical_pause) g_tut_pause_seen = 1;
    }

    if (g_tut_deny_timer > 0.0f) {
        g_tut_deny_timer -= ui_dt();
        if (g_tut_deny_timer <= 0.0f) {
            g_tut_deny_timer = 0.0f;
            g_tut_deny_active = 0;
        }
    }

    if (st->mode == TUT_INFO || st->mode == TUT_ACTION) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 vm = virt_mouse();
            Rectangle skip = tutorial_skip_rect();
            if (CheckCollisionPointRec(vm, skip)) {
                tutorial_finish(ctx);
                return;
            }
            if (st->mode == TUT_INFO) {
                Rectangle b = tutorial_btn_rect();
                if (CheckCollisionPointRec(vm, b)) {
                    ctx->gs.ui.mp_block_click = 1;
                    ctx->tutorial_step++;
                    if (ctx->tutorial_step >= TUT_COUNT) {
                        tutorial_finish(ctx);
                        return;
                    }
                    tutorial_prepare_step(ctx, ctx->tutorial_step);
                }
            } else if (st->gate_input && st->target != NULL) {
                Rectangle tr = st->target(ctx);
                if (tr.width <= 0.0f || tr.height <= 0.0f || !CheckCollisionPointRec(vm, tr)) {
                    tutorial_block_click(ctx, st->deny_msg);
                }
            }
        }
    }
}

void tutorial_render(AppContext *ctx) {
    if (!ctx->tutorial_active || ctx->menu.paused) return;

    ui_anim_tick();
    int s = ctx->tutorial_step;
    if (s < 0 || s >= TUT_COUNT) return;

    const TutStep *st = &TUT_STEPS[s];
    int cx = g_map_x_off + g_canvas_virt_w_base / 2;
    int px = cx - TUT_PW / 2;
    Rectangle pr = { (float)px, (float)TUT_Y, (float)TUT_PW, (float)TUT_PH };
    DrawRectangleRounded(pr, 0.12f, 6, (Color){8, 16, 28, 240});
    DrawRectangleRoundedLinesEx(pr, 0.12f, 6, 2.0f, (Color){90, 170, 220, 235});

    char step[16];
    snprintf(step, sizeof(step), "%d / %d", s + 1, TUT_COUNT);
    dtxt(step, px + TUT_PW - 14 - mtxt(step, 9), TUT_Y + 9, 9, (Color){120, 150, 180, 255});

    int tw = mtxt(st->title, 13);
    dtxt(st->title, cx - tw/2, TUT_Y + 9, 13, (Color){170, 215, 255, 255});
    int ly = TUT_Y + 9 + fh(13) + 6;
    int w1 = mtxt(st->l1, 10);
    dtxt(st->l1, cx - w1/2, ly, 10, (Color){216, 226, 236, 255});
    ly += fh(10) + 3;
    int w2 = mtxt(st->l2, 10);
    dtxt(st->l2, cx - w2/2, ly, 10, (Color){216, 226, 236, 255});

    if (st->mode == TUT_ACTION) {
        int status_w = mtxt("⏳ en attente de ton action", 10);
        dtxt("⏳ en attente de ton action", cx - status_w/2, ly + fh(10) + 3, 10,
             (Color){255, 200, 120, 255});
    }

    if (st->mode == TUT_INFO) {
        Rectangle b = tutorial_btn_rect();
        int hov = CheckCollisionPointRec(virt_mouse(), b);
        const char *bl = (s == TUT_COUNT - 1) ? "TERMINER" : "SUIVANT";
        DrawRectangleRounded(b, 0.3f, 5, hov ? (Color){40, 110, 150, 245} : (Color){24, 70, 100, 235});
        DrawRectangleRoundedLinesEx(b, 0.3f, 5, hov ? 2.0f : 1.2f, (Color){90, 180, 230, 235});
        int bw = mtxt(bl, 11);
        dtxt(bl, (int)b.x + (int)b.width/2 - bw/2, (int)b.y + (int)b.height/2 - fh(11)/2, 11,
             (Color){200, 235, 255, 255});
    }

    Rectangle skip = tutorial_skip_rect();
    int skip_hov = CheckCollisionPointRec(virt_mouse(), skip);
    DrawRectangleRounded(skip, 0.25f, 4, skip_hov ? (Color){48, 90, 120, 240} : (Color){16, 34, 54, 220});
    DrawRectangleRoundedLinesEx(skip, 0.25f, 4, 1.0f, (Color){90, 150, 210, 220});
    dtxt("PASSER", (int)skip.x + 10, (int)skip.y + 6, 9, (Color){200, 224, 240, 255});

    if (st->mode == TUT_ACTION && st->target != NULL) {
        Rectangle tr = st->target(ctx);
        if (tr.width > 0.0f && tr.height > 0.0f) {
            Color dark = {0, 0, 0, TUT_SPOTLIGHT_ALPHA};
            DrawRectangle(0, 0, g_canvas_virt_w_base, (int)tr.y, dark);
            DrawRectangle(0, (int)(tr.y + tr.height), g_canvas_virt_w_base, g_canvas_virt_h - (int)(tr.y + tr.height), dark);
            DrawRectangle(0, (int)tr.y, (int)tr.x, (int)tr.height, dark);
            DrawRectangle((int)(tr.x + tr.width), (int)tr.y, g_canvas_virt_w_base - (int)(tr.x + tr.width), (int)tr.height, dark);

            float pulse = 0.5f + 0.5f * sinf(g_tut_accum * 5.0f);
            float thick = TUT_RING_THICK_MIN + (TUT_RING_THICK_MAX - TUT_RING_THICK_MIN) * pulse;
            float ring_alpha = 120.0f + 70.0f * pulse;
            Color ring = {180, 220, 255, (unsigned char)ring_alpha};
            DrawRectangleRoundedLinesEx(tr, 0.14f, 6, thick, ring);

            float hint_k = (g_tut_hint_timer / TUT_ACTION_HINT_TIME);
            if (hint_k > 1.0f) hint_k = 1.0f;
            float arrow_len = TUT_ARROW_LEN + 24.0f * hint_k;
            float arrow_scale = 1.0f + 0.15f * ea_out_back(fminf(1.0f, g_tut_accum * 2.0f));
            Vector2 a0 = { tr.x + tr.width/2.0f, tr.y - 8.0f };
            Vector2 a1 = { tr.x + tr.width/2.0f, tr.y - 8.0f - arrow_len * arrow_scale };
            DrawLineEx(a0, a1, thick, ring);
            DrawTriangle((Vector2){ a1.x - 8.0f, a1.y + 10.0f },
                         (Vector2){ a1.x + 8.0f, a1.y + 10.0f },
                         (Vector2){ a1.x, a1.y }, ring);

            if (hint_k >= 1.0f) {
                int hint_w = mtxt("Le repere reste visible : clique ici !", 9);
                dtxt("Le repere reste visible : clique ici !", cx - hint_w/2, TUT_Y + TUT_PH + 10, 9,
                     (Color){255, 220, 140, 255});
            }
        }
    }

    if (g_tut_deny_active && g_tut_deny_timer > 0.0f) {
        Vector2 vm = virt_mouse();
        int dw = mtxt(g_tut_deny_timer > 0.3f ? st->deny_msg : "", 10);
        if (dw > 0) {
            dtxt(st->deny_msg, (int)vm.x + 12, (int)vm.y + 8, 10,
                 (Color){255, 120, 120, 220});
        }
    }
}

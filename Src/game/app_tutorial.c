/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  app_tutorial.c ─ Tutoriel guidé (premier niveau accompagné).
 *  Séquence d'étapes : les étapes d'ACTION se valident automatiquement
 *  (placer une tour, lancer une vague, tuer un ennemi) ; les étapes d'INFO
 *  via le bouton SUIVANT. Purement superposé : ne change pas les règles.
 */
#include "app_tutorial.h"
#include "../ui/renderer.h"    // g_map_x_off, g_canvas_virt_w_base
#include "../ui/ui_utils.h"    // dtxt / mtxt / fh
#include "raylib.h"
#include <stdio.h>

typedef enum { TKIND_INFO, TKIND_TOWER, TKIND_WAVE, TKIND_KILL, TKIND_END } TutKind;
typedef struct { TutKind kind; const char *title; const char *l1; const char *l2; } TutStep;

static const TutStep TUT_STEPS[] = {
    { TKIND_INFO,  "Bienvenue, Commandant",
      "Defends ta base contre des vagues d'ennemis.",
      "Suis les indications. Clique SUIVANT pour commencer." },
    { TKIND_TOWER, "1. Place une tour",
      "En bas, choisis une tour, puis clique une case libre",
      "le long du chemin des ennemis (zone constructible)." },
    { TKIND_WAVE,  "2. Lance la vague",
      "Clique le bouton LANCER LA VAGUE quand tu es pret.",
      "Tes tours tirent automatiquement sur les ennemis." },
    { TKIND_KILL,  "3. Au combat",
      "Chaque ennemi elimine rapporte de l'or.",
      "Avec l'or : pose d'autres tours ou ameliore-les." },
    { TKIND_INFO,  "Astuce : pause tactique",
      "Appuie sur ESPACE pour figer le jeu et reflechir.",
      "Tu peux quand meme placer / vendre / deplacer." },
    { TKIND_INFO,  "Astuce : zoom",
      "Molette = zoomer la carte.  Clic-molette maintenu",
      "= deplacer la vue quand tu es zoome." },
    { TKIND_INFO,  "Astuce : minerais",
      "Les ouvriers collectent des minerais (compteur Mat).",
      "Applique un minerai a une tour : enorme bonus de degats !" },
    { TKIND_END,   "Tu es pret !",
      "Tiens le plus longtemps possible.",
      "Clique TERMINER pour continuer a jouer librement." },
};
#define TUT_COUNT ((int)(sizeof(TUT_STEPS) / sizeof(TUT_STEPS[0])))
#define TUT_PW 560
#define TUT_PH 122
#define TUT_Y  10

static Rectangle tutorial_btn_rect(void) {
    int cx = g_map_x_off + g_canvas_virt_w_base / 2;
    return (Rectangle){ (float)(cx - 90), (float)(TUT_Y + TUT_PH - 40), 180.0f, 30.0f };
}

/* Fin du tutoriel : on rend la main au jeu libre (auto-vagues réactivées). */
static void tutorial_finish(AppContext *ctx) {
    ctx->tutorial_active = 0;
    ctx->gs.wave_manager.suppress_auto = 0;   // l'arcade reprend ses vagues auto
}

void tutorial_tick(AppContext *ctx) {
    if (!ctx->tutorial_active) return;
    int s = ctx->tutorial_step;
    if (s < 0 || s >= TUT_COUNT) { ctx->tutorial_active = 0; return; }

    int done = 0;
    switch (TUT_STEPS[s].kind) {
        case TKIND_TOWER: done = (ctx->gs.towers.tower_count >= 1); break;
        case TKIND_WAVE:  done = (ctx->gs.phase == PHASE_WAVE ||
                                  ctx->gs.wave_manager.number >= 1); break;
        case TKIND_KILL:  done = (ctx->gs.kills >= 1); break;
        default: break;
    }
    if (done) {
        if (++ctx->tutorial_step >= TUT_COUNT) tutorial_finish(ctx);
        return;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 vm = virt_mouse();
        if (CheckCollisionPointRec(vm, tutorial_btn_rect())) {
            ctx->gs.ui.mp_block_click = 1;   // n'arme pas un placement sous le bouton
            if (++ctx->tutorial_step >= TUT_COUNT) tutorial_finish(ctx);
        }
    }
}

void tutorial_render(AppContext *ctx) {
    if (!ctx->tutorial_active || ctx->menu.paused) return;
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
    int w1 = mtxt(st->l1, 10); dtxt(st->l1, cx - w1/2, ly, 10, (Color){216, 226, 236, 255});
    ly += fh(10) + 3;
    int w2 = mtxt(st->l2, 10); dtxt(st->l2, cx - w2/2, ly, 10, (Color){216, 226, 236, 255});

    Rectangle b = tutorial_btn_rect();
    int hov = CheckCollisionPointRec(virt_mouse(), b);
    const char *bl = (st->kind == TKIND_END) ? "TERMINER" : "SUIVANT";
    DrawRectangleRounded(b, 0.3f, 5, hov ? (Color){40, 110, 150, 245} : (Color){24, 70, 100, 235});
    DrawRectangleRoundedLinesEx(b, 0.3f, 5, hov ? 2.0f : 1.2f, (Color){90, 180, 230, 235});
    int bw = mtxt(bl, 11);
    dtxt(bl, (int)b.x + (int)b.width/2 - bw/2, (int)b.y + (int)b.height/2 - fh(11)/2, 11,
         (Color){200, 235, 255, 255});
}

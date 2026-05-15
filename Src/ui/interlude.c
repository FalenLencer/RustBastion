#include "interlude.h"
#include "renderer.h"
#include "../meta/meta.h"
#include "../map/theme.h"
#include <math.h>

// ── Constantes layout (cohérentes avec menu.c) ───────────────
#define M_PAD  16
#define M_IN    8
#define M_LINE 12
#define BTN_H  36
#define BTN_R   5
#define PNL_R   6

void interlude_render(const GameState *gs, int scrap_earned, int last_stage) {
    const int CX = VIRT_W/2, CY = VIRT_H/2;

    // Fond assombri
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){0,0,0,195});

    int pw = 440, ph = last_stage ? 252 : 222;
    float rnd = (float)PNL_R / ph;

    // Panneau principal
    DrawRectangleRounded(
        (Rectangle){CX-pw/2.0f, CY-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){CX-pw/2.0f, CY-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f,
        last_stage ? (Color){232,152,32,255} : (Color){42,190,105,255});

    int px = CX - pw/2 + M_PAD;
    int iw = pw - M_PAD*2;
    int py = CY - ph/2 + M_PAD;

    // Titre
    const char *title = last_stage ? "CAMPAGNE TERMINEE !" : "STAGE TERMINE !";
    int fs_title = 20;
    int tw = MeasureText(title, fs_title);
    Color title_col = last_stage ? (Color){232,152,32,255}
                                 : (Color){42,190,105,255};
    DrawText(title, CX - tw/2, py, fs_title, title_col);
    py += fs_title + M_IN;

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){45,28,8,180});
    py += M_IN;

    // Stats
    DrawText(TextFormat("Vague atteinte  : %d", gs->wave_manager.number),
             px, py, 12, (Color){168,148,102,255});
    py += M_LINE + 2;

    DrawText(TextFormat("Ferraille gagnee : +%d", scrap_earned),
             px, py, 12, (Color){118,188,45,255});
    py += M_LINE;

    DrawText(TextFormat("Ferraille totale : %d", gs->meta.scrap),
             px, py, 10, (Color){72,112,52,255});
    py += M_LINE + M_IN;

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){38,24,6,150});
    py += M_IN;

    if (!last_stage) {
        // Prochain stage
        int themes[CAMPAIGN_STAGES];
        meta_campaign_theme_order(gs->campaign_order_seed, themes);
        int next = gs->campaign_stage + 1;
        if (next >= CAMPAIGN_STAGES) next = 0;
        const Theme *nth = theme_get((ThemeID)themes[next]);
        DrawText("Prochain stage :", px, py, 10, (Color){72,100,72,255});
        DrawText(nth->name,
                 px + MeasureText("Prochain stage : ", 10), py,
                 10, (Color){92,158,185,255});
        py += M_LINE + M_IN;
    } else {
        // Campagnes complétées
        DrawText(TextFormat("Campagnes terminees : %d",
                     gs->meta.campaigns_completed),
                 px, py, 10, (Color){232,152,32,255});
        py += M_LINE + M_IN;
    }

    // Bouton CONTINUER / RETOUR
    const char *hint = last_stage ? "[ESPACE]  Retour au menu"
                                  : "[ESPACE]  Continuer";
    // Fond bouton arrondi
    int bw = 200, bh = BTN_H;
    int bx = CX - bw/2;
    int by = CY + ph/2 - M_PAD - bh;
    float brnd = (float)BTN_R / bh;

    Color bcol = last_stage ? (Color){232,152,32,255}
                            : (Color){42,190,105,255};
    DrawRectangleRounded(
        (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
        brnd, 6, (Color){bcol.r/6,bcol.g/6,bcol.b/6,255});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
        brnd, 6, 1.5f, bcol);

    int hw = MeasureText(hint, 12);
    DrawText(hint, CX - hw/2, by + bh/2 - 6, 12,
             (Color){148,128,92,255});
}
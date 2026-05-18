/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "interlude.h"
#include "ui_utils.h"
#include "campaign_data.h"
#include "../game/meta.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

// ── Constantes layout ────────────────────────────────────────
#define M   16
#define M_S  8

static void txt_c(const char *s, int cx, int y, int fs, Color col) {
    dtxt(s, cx - mtxt(s, fs)/2, y, fs, col);
}

static void draw_stars(int cx, int y, int stars, int max_stars) {
    int sw = 20, gap = 6;
    int total = max_stars * sw + (max_stars-1) * gap;
    int sx = cx - total/2;
    for (int i = 0; i < max_stars; i++) {
        Color c = (i < stars) ? (Color){232,200,32,255}
                               : (Color){50,40,20,255};
        dtxt("*", sx + i*(sw+gap), y, sw, c);
    }
}

// ════════════════════════════════════════════════════
// DIALOGUE AVANT L'ACTE
// ════════════════════════════════════════════════════
void interlude_render_dialog_before(const ActData *act, int vw, int vh) {
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,220});

    int pw = 580, ph = 300;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, 2.0f, (Color){80,55,20,255});

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    // Sous-titre chapitre/acte
    txt_c(act->subtitle, cx, py, 9, (Color){100,80,50,255});
    py += 14;

    // Titre de l'acte
    txt_c(act->title, cx, py, 18, (Color){232,152,32,255});
    py += 24;

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){60,40,12,160});
    py += M_S;

    // Objectif
    dtxt("OBJECTIF :", px, py, 10, (Color){80,160,80,255});
    py += 14;
    char obj_buf[80];
    clip_text(act->objective.description, iw, 12, obj_buf, sizeof(obj_buf));
    dtxt(obj_buf, px + M_S, py, 12, (Color){150,220,150,255});
    py += 20;

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){60,40,12,160});
    py += M_S;

    // Dialogue (texte multi-ligne)
    // On affiche le dialogue ligne par ligne
    {
        const char *d = act->dialog_before;
        char line[96];
        int li = 0, di = 0;
        int dy = py;
        while (d[di] && dy < cy + ph/2 - 40) {
            li = 0;
            while (d[di] && d[di] != '\n' && li < 95)
                line[li++] = d[di++];
            if (d[di] == '\n') di++;
            line[li] = '\0';
            if (li > 0) {
                char tbuf[96];
                clip_text(line, iw, 10, tbuf, sizeof(tbuf));
                dtxt(tbuf, px, dy, 10, (Color){168,148,102,255});
            }
            dy += 13;
        }
    }

    // Hint
    txt_c("[ESPACE] Commencer l'acte", cx, cy + ph/2 - M - 12, 10,
          (Color){80,65,40,255});
}

// ════════════════════════════════════════════════════
// DIALOGUE APRÈS L'ACTE
// ════════════════════════════════════════════════════
void interlude_render_dialog_after(const ActData *act, int stars,
                                   int scrap_earned, int vw, int vh)
{
    int cx = vw/2, cy = vh/2;
    int is_last = (act->chapter == CAMPAIGN_CHAPTERS-1 &&
                   act->act == CAMPAIGN_ACTS-1);

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,200});

    int pw = 580, ph = 330;
    Color border_col = is_last ? (Color){232,152,32,255}
                               : (Color){42,190,105,255};
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, 2.0f, border_col);

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    // Titre
    const char *title = is_last ? "CAMPAGNE TERMINEE !" : "ACTE TERMINE !";
    txt_c(title, cx, py, 20, border_col);
    py += 28;

    // Étoiles
    draw_stars(cx, py, stars, 2);
    py += 28;

    DrawLine(px, py, px+iw, py, (Color){50,35,10,160});
    py += M_S;

    // Stats
    dtxt(TextFormat("Ferraille gagnee : +%d", scrap_earned),
             px, py, 12, (Color){118,188,45,255}); py += 16;

    // Message de débloquage éventuel
    if (act->unlock_msg) {
        dtxt(act->unlock_msg, px, py, 11,
                 (Color){140,200,240,255}); py += 16;
    }

    DrawLine(px, py, px+iw, py, (Color){50,35,10,160});
    py += M_S;

    // Dialogue après
    {
        const char *d = act->dialog_after;
        char line[96];
        int li = 0, di = 0;
        int dy = py;
        while (d[di] && dy < cy + ph/2 - 44) {
            li = 0;
            while (d[di] && d[di] != '\n' && li < 95)
                line[li++] = d[di++];
            if (d[di] == '\n') di++;
            line[li] = '\0';
            if (li > 0) {
                char tbuf[96];
                clip_text(line, iw, 10, tbuf, sizeof(tbuf));
                dtxt(tbuf, px, dy, 10, (Color){168,148,102,255});
            }
            dy += 13;
        }
    }

    const char *hint = is_last ? "[ESPACE] Retour au menu"
                                : "[ESPACE] Stage suivant";
    txt_c(hint, cx, cy + ph/2 - M - 12, 10, (Color){80,65,40,255});
}

// ════════════════════════════════════════════════════
// GAME OVER
// ════════════════════════════════════════════════════
void interlude_render_gameover(const GameState *gs, int vw, int vh) {
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,180});

    int pw = 440, ph = gs->is_campaign ? 220 : 190;
    float rnd = 5.0f/(float)ph;
    Color border = (Color){200,40,20,255};
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){20,4,4,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, border);

    int py = cy - ph/2 + M;

    txt_c("BASTION TOMBE", cx, py, 22, border);
    py += 32;

    if (gs->is_campaign) {
        const ActData *ad = campaign_act_get(gs->campaign_stage);
        char buf[80];
        clip_text(ad->subtitle, pw - M*2, 10, buf, sizeof(buf));
        txt_c(buf, cx, py, 10, (Color){100,80,50,255});
        py += 16;
    }

    txt_c(TextFormat("Vague %d  |  Ennemis elimines : %d",
                     gs->wave_manager.number, gs->kills),
          cx, py, 11, (Color){120,80,60,255});
    py += 24;

    const char *hint = gs->is_campaign
        ? "[ESPACE] Retour a la carte"
        : "[ESPACE] Retour au menu";
    txt_c(hint, cx, cy + ph/2 - M - 12, 11, (Color){80,65,40,255});
    (void)py;
}

// ════════════════════════════════════════════════════
// ÉCRAN EXTRACTION ENDLESS
// ════════════════════════════════════════════════════
void interlude_render_extract(const GameState *gs, int vw, int vh,
                              Vector2 vmouse)
{
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,170});

    int pw = 420, ph = 260;
    float rnd = 5.0f / ph;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, (Color){232,152,32,255});

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    /* Titre */
    txt_c("POINT D'EXTRACTION", cx, py, 18, (Color){232,152,32,255});
    py += 24;
    DrawLine(px, py, px+iw, py, (Color){60,40,12,180}); py += 10;

    /* Infos */
    dtxt(TextFormat("Serie          : %d",  gs->endless_series+1),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    dtxt(TextFormat("Vague          : %d",  gs->wave_manager.number),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    dtxt(TextFormat("Multiplicateur : x%.1f", gs->endless_multiplier),
             px, py, 12, (Color){232,152,32,255}); py += 16;

    int score = meta_endless_score(gs->wave_manager.number, gs->endless_multiplier);
    int scrap  = score / 10 > 200 ? 200 : score / 10;
    dtxt(TextFormat("Ferraille si extrait : +%d", scrap),
             px, py, 12, (Color){118,188,45,255}); py += 16;
    dtxt(TextFormat("Continuer -> mult. x%.1f", gs->endless_multiplier * 1.5f),
             px, py, 10, (Color){100,160,220,255}); py += 20;
    DrawLine(px, py, px+iw, py, (Color){40,28,8,140}); py += 10;

    /* Boutons */
    int bw = 160, bh = 32;
    int by2 = cy + ph/2 - M_S - bh;
    int bx1 = cx - bw - M_S;
    int bx2 = cx + M_S;

    /* [E] EXTRAIRE */
    {
        Rectangle r = {(float)bx1,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){8,28,8,255} : (Color){4,16,4,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){42,190,105,255} : (Color){20,80,40,255});
        const char *lbl = "[E] EXTRAIRE";
        dtxt(lbl, bx1+bw/2-mtxt(lbl,13)/2, by2+bh/2-7,
                 13, (Color){42,190,105,255});
    }
    /* [ESPACE] CONTINUER */
    {
        Rectangle r = {(float)bx2,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){6,18,32,255} : (Color){4,12,20,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){52,140,210,255} : (Color){24,70,110,255});
        const char *lbl = "[ESPACE] CONTINUER";
        dtxt(lbl, bx2+bw/2-mtxt(lbl,11)/2, by2+bh/2-7,
                 11, (Color){52,140,210,255});
    }
    (void)py;
}


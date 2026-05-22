/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_campaign.c ─ Carte de progression de campagne.
 *
 *  Contient :
 *    draw_world_map  — Vue d'ensemble des chapitres et actes débloqués
 */

#include "menu_internal.h"

// ════════════════════════════════════════════════════
// CARTE DU MONDE — progression campagne
// ════════════════════════════════════════════════════
MenuAction draw_world_map(MenuState *m, const MetaProgress *meta,
                          int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;

    draw_bg(m, vw, vh);
    draw_header("PROGRESSION DE CAMPAGNE", vw);

    txt_c_boxed("Completez chaque acte pour progresser vers la victoire.",
                cx, M_PAD + 86, 10, C_TEXT);

    static const Color CHAPTER_COLS[CAMPAIGN_CHAPTERS] = {
        {200,150, 80,255},  // Wasteland — ocre
        { 60,180, 80,255},  // Swamp     — vert
        {220,180, 80,255},  // Desert    — jaune
        {100,140,200,255},  // City      — bleu
        {180, 80, 80,255},  // Factory   — rouge
    };

    static const char *CHAPTER_NAMES[CAMPAIGN_CHAPTERS] = {
        "Les Terres Brulees",
        "Le Marais Toxique",
        "Le Desert Irradie",
        "La Ville en Ruine",
        "L'Usine Abandonnee",
    };

    int chapter_w = vw - M_PAD * 4;
    int chapter_h = 58;
    int chapter_x = M_PAD * 2;
    int chapter_y = M_PAD + 110;
    int gap       = M_IN;

    for (int ch = 0; ch < CAMPAIGN_CHAPTERS; ch++) {
        Color col = CHAPTER_COLS[ch];

        Rectangle cr = {(float)chapter_x, (float)chapter_y,
                        (float)chapter_w, (float)chapter_h};
        int ch_unlocked = meta_act_unlocked(meta, ch * CAMPAIGN_ACTS);
        Color bg = ch_unlocked ? (Color){col.r/8, col.g/8, col.b/8, 255}
                               : (Color){10, 8, 5, 255};
        DrawRectangleRounded(cr, 4.0f/chapter_h, 5, bg);
        DrawRectangleRoundedLinesEx(cr, 4.0f/chapter_h, 5, 1.5f,
            ch_unlocked ? (Color){col.r/3, col.g/3, col.b/3, 255}
                        : (Color){40,30,12,255});

        dtxt(TextFormat("CH.%d", ch+1),
                 chapter_x + M_IN, chapter_y + M_IN, 10, C_DIM);
        dtxt(CHAPTER_NAMES[ch],
                 chapter_x + M_IN + 34, chapter_y + M_IN, 14,
                 ch_unlocked ? col : C_DIM);

        int act_w = (chapter_w - M_IN * 4) / CAMPAIGN_ACTS;
        for (int a = 0; a < CAMPAIGN_ACTS; a++) {
            int stage_idx = ch * CAMPAIGN_ACTS + a;
            const ActData *ad = campaign_act_get(stage_idx);
            int stars    = meta->act_stars[stage_idx];
            int unlocked = meta_act_unlocked(meta, stage_idx);

            int ax = chapter_x + M_IN + a * (act_w + M_IN);
            int ay = chapter_y + 26;
            int ah = chapter_h - 30;

            Rectangle ar = {(float)ax, (float)ay, (float)act_w, (float)ah};
            Color abg = unlocked ? (stars > 0 ? (Color){col.r/6,col.g/6,col.b/6,255}
                                              : (Color){18,12,4,255})
                                 : (Color){8,6,3,255};
            DrawRectangleRounded(ar, 3.0f/ah, 4, abg);
            DrawRectangleRoundedLinesEx(ar, 3.0f/ah, 4, 1.0f,
                unlocked ? (stars > 0 ? col : (Color){60,45,18,255})
                         : (Color){30,24,10,255});

            char abuf[32];
            clip_text(ad->title, act_w - 6, 9, abuf, sizeof(abuf));
            dtxt(abuf, ax+3, ay+2, 9,
                     unlocked ? (stars>0 ? col : C_TEXT) : C_DIM);

            for (int s = 0; s < 2; s++) {
                Color sc = (s < stars) ? (Color){232,200,32,255}
                                       : (Color){40,32,12,255};
                dtxt("*", ax + 3 + s*12, ay + ah - 14, 12, sc);
            }

            if (!unlocked)
                dtxt("---", ax + act_w/2 - 8, ay + ah/2 - 5, 10, C_DIM);
        }

        chapter_y += chapter_h + gap;
    }

    {
        int bw = 200, bh = BTN_H;
        int bx = cx - bw/2;
        int by = vh - M_PAD * 2 - bh;
        if (draw_btn("JOUER UNE CAMPAGNE", bx, by, bw, bh, C_GOLD, 0)) {
            m->screen = MENU_CAMPAIGN;
            m->back_screen = MENU_WORLD_MAP;
        }
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

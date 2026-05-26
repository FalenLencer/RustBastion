/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_screens.c ─ Écrans principaux du menu.
 *
 *  Contient :
 *    draw_title          — Écran d'accueil (JOUER / OPTIONS / QUITTER)
 *    draw_play_hub       — Hub de sélection de mode
 *    draw_slot_list      — Liste des emplacements de sauvegarde (arcade & campagne)
 *    draw_new_campaign   — Configuration d'une nouvelle campagne
 *    draw_new_arcade     — Sélection de thème pour une partie arcade
 *    draw_confirm_del    — Confirmation de suppression d'une sauvegarde
 *    draw_pause          — Menu pause en jeu
 */

#include "menu_internal.h"

// ════════════════════════════════════════════════════
// ÉCRAN TITRE
// ════════════════════════════════════════════════════
MenuAction draw_title(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);

    txt_c_boxed("RUST BASTION", cx, vh/2 - 170, 46, C_GOLD);
    txt_c_boxed("Tower Defense Post-Apocalyptique", cx, vh/2 - 114, 13, C_DIM);
    draw_sep(cx - 180, vh/2 - 92, 360, C_BORDER);

    int bw = 240, bh = BTN_H + 4;
    int bx = cx - bw/2;
    int by = vh/2 - 52;

    if (draw_btn("JOUER",   bx, by, bw, bh, C_GREEN, 0)) {
        m->screen = MENU_PLAY_HUB;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (draw_btn("OPTIONS", bx, by, bw, bh, C_BLUE, 0)) {
        m->screen = MENU_OPTIONS;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (draw_btn("QUITTER", bx, by, bw, bh, C_RED, 0))
        act.quit_app = 1;

    dtxt("v0.1", vw - M_PAD - 28, vh - M_PAD - 12, 9, C_DIM);
    return act;
}

// ════════════════════════════════════════════════════
// HUB JOUER
// ════════════════════════════════════════════════════
MenuAction draw_play_hub(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("CHOISIR UN MODE", vw);

    int bw = 520, bh = 72, gap = M_IN + 2;
    int bx = cx - bw/2;
    int by = M_PAD + 76;

    if (draw_nav_btn("C", "CAMPAGNE",
                     "Carte de progression — 5 chapitres, 15 actes.",
                     C_GOLD, bx, by, bw, bh)) {
        push_back_screen(m);
        m->selected_campaign_act = -1;   // reset : aucun acte pre-selectionne
        m->screen      = MENU_WORLD_MAP;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    if (draw_nav_btn("A", "ARCADE",
                     "Choisissez un environnement et jouez librement.",
                     C_BLUE, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_ARCADE;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    if (draw_nav_btn("X", "PERSONNALISE",
                     "Carte, spawns, bases, terrain et difficulte sur mesure.",
                     C_ORANGE, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_CUSTOM;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    char upg_desc[80];
    snprintf(upg_desc, sizeof(upg_desc),
             "Depensez vos %d ferrailles pour ameliorer vos defenses.",
             meta->scrap);
    if (draw_nav_btn("*", "AMELIORATIONS", upg_desc,
                     C_ORANGE, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_UPGRADES;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    int nb_disc = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (meta->bestiary_discovered[i]) nb_disc++;
    char best_desc[80];
    snprintf(best_desc, sizeof(best_desc),
             "%d/%d ennemis identifies. Resistances et faiblesses.",
             nb_disc, ENEMY_TYPE_COUNT);
    if (draw_nav_btn("B", "BESTIAIRE", best_desc,
                     C_RED, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen      = MENU_BESTIARY;
        m->back_screen = MENU_PLAY_HUB;
        if (m->sel_bestiary < 0) m->sel_bestiary = 0;
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// LISTE DE SLOTS
// ════════════════════════════════════════════════════
MenuAction draw_slot_list(MenuState *m, int vw, int vh, int is_campaign) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header(is_campaign ? "CAMPAGNE" : "ARCADE", vw);

    const char *sub = is_campaign
        ? "15 actes en sequence — 5 chapitres — la ferraille se gagne ici"
        : "Mode libre — choisissez votre terrain";
    txt_c_boxed(sub, cx, M_PAD + 86, 10, C_TEXT);

    int sw = 540, sh = 68, sg = M_IN;
    int sx = cx - sw/2;
    int y  = M_PAD + 112;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        const SaveInfo *si = is_campaign ? &m->campaign_slots[i] : &m->slots[i];

        Rectangle r = {(float)sx,(float)y,(float)sw,(float)sh};
        int hov = vhov_r(r);
        Color brd = si->exists ? (is_campaign ? C_GOLD : C_BLUE) : C_BORDER;
        Color bg  = hov ? C_HOV : C_PANEL;

        DrawRectangleRounded(r, (float)PANEL_R/sh, 6, bg);
        DrawRectangleRoundedLinesEx(r, (float)PANEL_R/sh, 6, 1.5f, brd);

        int tx = sx + M_IN + 4;
        int ty = y  + M_IN;

        /* ── Boutons communs (calculés ici pour aligner le clip du titre) ── */
        const int bh2  = 26;
        const int bw_rep = 88, bw_del = 58, btn_gap = 4;
        const int bx_del = sx + sw - M_IN - bw_del;
        const int bx_rep = bx_del - btn_gap - bw_rep;
        const int by2    = y + sh/2 - bh2/2;
        const int txt_clip_w = bx_rep - tx - M_IN;   /* largeur dispo pour le titre */

        if (si->exists) {
            if (is_campaign) {
                const ActData *ad = campaign_act_get(si->campaign_stage);
                char raw[96];
                snprintf(raw, sizeof(raw),
                         "Campagne %d  —  Acte %d/%d  —  %s",
                         si->campaign_num + 1,
                         si->campaign_stage + 1,
                         CAMPAIGN_TOTAL,
                         ad ? ad->title : "?");
                char cbuf[96];
                clip_text(raw, txt_clip_w, 12, cbuf, sizeof(cbuf));
                draw_text_boxed(cbuf, tx, ty, 12, C_GOLD);
            } else {
                char arcbuf[64];
                clip_text(TextFormat("ARCADE  —  %s", si->theme_name),
                          txt_clip_w, 12, arcbuf, sizeof(arcbuf));
                dtxt(arcbuf, tx, ty, 12, C_BLUE);
            }
            ty += 16;
            {
                char s1[32], s2[24], s3[24];
                snprintf(s1, sizeof(s1), "Vague %d  |  ", si->wave);
                snprintf(s2, sizeof(s2), "  %d  |  ", si->lives);
                snprintf(s3, sizeof(s3), "  %d", si->gold);
                int ix = tx;
                dtxt(s1, ix, ty, 10, C_TEXT);   ix += mtxt(s1, 10);
                draw_icon(g_icon_heart, ix, ty, fh(10), WHITE); ix += fh(10);
                dtxt(s2, ix, ty, 10, C_TEXT);   ix += mtxt(s2, 10);
                draw_icon(g_icon_gold,  ix, ty, fh(10), WHITE); ix += fh(10);
                dtxt(s3, ix, ty, 10, C_TEXT);
            }
            ty += M_LINE;
            dtxt(TextFormat("Emplacement %d", i+1), tx, ty, 9, C_DIM);

            /* REPRENDRE + EFFACER côte à côte */
            if (draw_btn("REPRENDRE", bx_rep, by2, bw_rep, bh2, C_GREEN, 0)) {
                act.resume_slot        = i;
                act.resume_is_campaign = is_campaign;
                act.go_game            = 1;
            }
            if (draw_btn("EFFACER", bx_del, by2, bw_del, bh2, C_RED, 0)) {
                m->confirm_del_slot = i;
                m->back_screen = is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                m->screen = MENU_CONFIRM_DEL;
            }

        } else {
            if (is_campaign) {
                dtxt(TextFormat("Emplacement %d — vide", i+1),
                     tx, y + M_IN, 9, C_DIM);

                if (m->new_slot == i && m->selected_campaign_act >= 0) {
                    /* Acte sélectionné sur la carte → affiche + LANCER */
                    const ActData *sel_ad =
                        campaign_act_get(m->selected_campaign_act);
                    if (sel_ad) {
                        char albl[72];
                        snprintf(albl, sizeof(albl), "Acte %d — %s",
                                 m->selected_campaign_act + 1, sel_ad->title);
                        char aclip[72];
                        int lancer_w = bw_rep + btn_gap + bw_del; /* bouton large */
                        clip_text(albl, bx_rep - tx - M_IN, 11, aclip, sizeof(aclip));
                        dtxt(aclip, tx, y + M_IN + 14, 11, C_GOLD);
                        char sclip[64];
                        clip_text(sel_ad->subtitle, bx_rep - tx - M_IN, 9,
                                  sclip, sizeof(sclip));
                        dtxt(sclip, tx, y + M_IN + 28, 9, C_DIM);
                        if (draw_btn("LANCER", bx_rep, by2, lancer_w, bh2,
                                     C_GOLD, 0)) {
                            act.start_campaign      = 1;
                            act.new_slot            = i;
                            act.campaign_order_seed = 0;
                            act.start_campaign_act  = m->selected_campaign_act;
                        }
                    }
                } else {
                    /* Pas encore d'acte choisi → envoyer sur la carte du monde */
                    int bw_new = 150;
                    int bx_new = sx + sw - M_IN - bw_new;
                    if (draw_btn("NOUVELLE PARTIE", bx_new, by2, bw_new, bh2,
                                 C_GOLD, 0)) {
                        m->new_slot              = i;
                        m->selected_campaign_act = -1;
                        push_back_screen(m);
                        m->screen      = MENU_WORLD_MAP;
                        m->back_screen = MENU_CAMPAIGN;
                    }
                }
            } else {
                dtxt(TextFormat("Emplacement %d — vide", i+1),
                         tx, y + sh/2 - 8, 11, C_DIM);
                if (draw_btn("NOUVELLE ARCADE",
                             sx + sw - M_IN - 154, by2, 154, bh2, C_BLUE, 0)) {
                    m->new_slot            = i;
                    m->campaign_order_seed = 0;
                    m->screen = MENU_NEW_ARCADE;
                }
            }
        }

        y += sh + sg;
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// NOUVELLE CAMPAGNE
// ════════════════════════════════════════════════════
MenuAction draw_new_campaign(MenuState *m, const MetaProgress *meta,
                             int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;
    draw_bg(m, vw, vh);
    draw_header("NOUVELLE CAMPAGNE", vw);

    // Ordre fixe des chapitres — identique à campaign_data.c
    static const Color CHAPTER_COLS[CAMPAIGN_CHAPTERS] = {
        {200,150, 80,255},  // Ch.1 — Terres Brulees  (ocre)
        { 60,180, 80,255},  // Ch.2 — Marais Toxique  (vert)
        {220,180, 80,255},  // Ch.3 — Desert Irradie  (jaune)
        {100,140,200,255},  // Ch.4 — Ville en Ruine  (bleu)
        {180, 80, 80,255},  // Ch.5 — Usine Abandonnee (rouge)
    };
    static const char *CHAPTER_NAMES[CAMPAIGN_CHAPTERS] = {
        "Les Terres Brulees",
        "Le Marais Toxique",
        "Le Desert Irradie",
        "La Ville en Ruine",
        "L'Usine Abandonnee",
    };

    int pw = 480, ph = 310;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int py = cy - ph/2 + M_PAD;

    dtxt(TextFormat("Emplacement : %d", m->new_slot+1), px, py, 10, C_DIM);
    py += M_LINE + 2;
    dtxt(TextFormat("Campagne n°%d", meta->campaigns_completed+1), px, py, 17, C_GOLD);
    py += fh(17) + 4;
    draw_sep(px, py, pw - M_PAD*2, C_BORDER);
    py += M_IN + 2;
    /* Acte de départ sélectionné */
    {
        const ActData *sel_ad = campaign_act_get(m->selected_campaign_act);
        if (sel_ad && m->selected_campaign_act > 0) {
            char dbuf[80];
            snprintf(dbuf, sizeof(dbuf), "Depart : Acte %d — %s",
                     m->selected_campaign_act + 1, sel_ad->title);
            char dclip[80];
            clip_text(dbuf, pw - M_PAD*2, 11, dclip, sizeof(dclip));
            dtxt(dclip, px, py, 11, C_GREEN);
            py += M_LINE + 2;
        } else {
            dtxt("Depart : Acte 1 — Premier contact", px, py, 11, C_GREEN);
            py += M_LINE + 2;
        }
    }
    dtxt("15 actes — 5 chapitres, ordre fixe :", px, py, 10, C_DIM);
    py += M_LINE + 2;

    for (int i = 0; i < CAMPAIGN_CHAPTERS; i++) {
        Color col = CHAPTER_COLS[i];
        Rectangle row = {(float)px,(float)py,(float)(pw - M_PAD*2), 28};
        DrawRectangleRounded(row, (float)PANEL_R/28, 5, (Color){18,10,3,210});
        DrawRectangleRoundedLinesEx(row, (float)PANEL_R/28, 5, 1.0f,
            (Color){col.r/3, col.g/3, col.b/3, 180});
        dtxt(TextFormat("Ch.%d", i+1), px + M_IN, py + 8, 11, C_DIM);
        dtxt(CHAPTER_NAMES[i], px + M_IN + 34, py + 8, 11, col);
        py += 28 + 4;
    }

    py = cy + ph/2 - M_PAD - BTN_H;
    int half = (pw - M_PAD*2 - M_IN) / 2;

    if (draw_btn("LANCER", px, py, half, BTN_H, C_GREEN, 0)) {
        act.start_campaign      = 1;
        act.new_slot            = m->new_slot;
        act.campaign_order_seed = 0;
        act.start_campaign_act  = m->selected_campaign_act;
    }
    if (draw_btn("ANNULER", px + half + M_IN, py,
                 pw - M_PAD*2 - M_IN - half, BTN_H, C_DIM, 0)) {
        m->screen = MENU_CAMPAIGN;
    }
    return act;
}

// ════════════════════════════════════════════════════
// NOUVELLE ARCADE
// ════════════════════════════════════════════════════
static const char *THEME_LABELS[THEME_COUNT+1] = {
    "Terres devastees","Marais toxique","Desert irradie",
    "Ville en ruine","Usine abandonnee","Aleatoire",
};

MenuAction draw_new_arcade(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("NOUVELLE PARTIE ARCADE", vw);

    dtxt(TextFormat("Emplacement : %d", m->new_slot+1),
             cx - 100, M_PAD + 62, 11, C_TEXT);

    int bw = 300, bh = 30, gap = 5;
    int bx = cx - bw/2;
    int by = M_PAD + 82;

    draw_text_boxed("Choisir un environnement :", bx, by, 11, C_GOLD);
    by += M_LINE + 4;

    for (int i = 0; i <= THEME_COUNT; i++) {
        int is_sel = ((int)m->new_theme == i);
        Rectangle r = {(float)bx,(float)by,(float)bw,(float)bh};
        float rnd = (float)BTN_R/bh;
        Color bg  = is_sel ? (Color){6,18,32,255} : C_PANEL;
        Color brd = is_sel ? C_BLUE : C_BORDER;
        float lw  = is_sel ? 2.0f : 1.0f;
        DrawRectangleRounded(r, rnd, 5, bg);
        DrawRectangleRoundedLinesEx(r, rnd, 5, lw, brd);
        dtxt(THEME_LABELS[i], bx + M_IN, by + bh/2 - 5, 11,
                 is_sel ? C_BLUE : C_TEXT);
        if (is_sel) {
            const char *chk = ">";
            dtxt(chk, bx + bw - M_IN - mtxt(chk,11),
                     by + bh/2 - fh(11)/2, 11, C_BLUE);
        }
        if (vclick_r(r)) m->new_theme = (ThemeID)i;
        by += bh + gap;
    }

    by += M_IN;
    int half = (bw - M_IN) / 2;
    if (draw_btn("LANCER",  bx,           by, half, BTN_H, C_GREEN, 0)) {
        act.start_arcade = 1;
        act.new_theme    = m->new_theme;
        act.new_slot     = m->new_slot;
    }
    if (draw_btn("ANNULER", bx+half+M_IN, by,
                 bw-M_IN-half, BTN_H, C_DIM, 0))
        m->screen = MENU_ARCADE;

    return act;
}

// ════════════════════════════════════════════════════
// CONFIRMATION SUPPRESSION
// ════════════════════════════════════════════════════
MenuAction draw_confirm_del(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,140});

    int pw = 360, ph = 120;
    draw_panel(cx, cy, pw, ph, C_RED);

    txt_c(TextFormat("Effacer la partie %d ?", m->confirm_del_slot+1),
          cx, cy - ph/2 + M_PAD, 14, C_RED);
    txt_c("Cette action est irreversible.",
          cx, cy - ph/2 + M_PAD + 22, 10, C_DIM);

    int bw2 = 110, bh2 = BTN_H;
    int by2 = cy + ph/2 - M_PAD - bh2;

    if (draw_btn("EFFACER", cx - bw2 - M_IN/2, by2, bw2, bh2, C_RED, 0)) {
        if (m->back_screen == MENU_CAMPAIGN ||
            m->back_screen == MENU_WORLD_MAP)
            campaign_save_delete(m->confirm_del_slot);
        else
            save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        m->screen = (m->back_screen == MENU_CAMPAIGN ||
                     m->back_screen == MENU_ARCADE   ||
                     m->back_screen == MENU_WORLD_MAP)
                    ? m->back_screen : MENU_PLAY_HUB;
        set_msg(m, "Partie effacee.");
    }
    if (draw_btn("ANNULER", cx + M_IN/2, by2, bw2, bh2, C_TEXT, 0))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// MENU PAUSE
// ════════════════════════════════════════════════════
MenuAction draw_pause(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,155});

    int pw = 260, ph = 340;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int iw = pw - M_PAD*2;
    int py = cy - ph/2 + M_PAD;

    txt_c("PAUSE", cx, py, 22, C_GOLD);
    py += fh(22) + 3;
    draw_sep(px, py, iw, C_BORDER);
    py += M_IN + 4;

    int bh2 = BTN_H, gap = M_IN - 2;

    if (draw_btn("REPRENDRE",     px, py, iw, bh2, C_GREEN, 0))
        { m->paused = 0; m->screen = MENU_TITLE; }
    py += bh2 + gap;

    if (draw_btn("SAUVEGARDER",   px, py, iw, bh2, C_GOLD, 0))
        act.save_and_quit = 2;
    py += bh2 + gap;

    if (draw_btn("OPTIONS",        px, py, iw, bh2, C_BLUE, 0))
        m->screen = MENU_OPTIONS;
    py += bh2 + gap;

    if (draw_btn("MENU PRINCIPAL", px, py, iw, bh2, C_DIM, 0))
        act.save_and_quit = 1;
    py += bh2 + gap;

    if (draw_btn("QUITTER",        px, py, iw, bh2, C_RED, 0))
        act.quit_app = 1;

    char sz[48];
    snprintf(sz, sizeof(sz), "%dx%d  %s",
             GetScreenWidth(), GetScreenHeight(),
             IsWindowFullscreen() ? "Plein ecran" : "Fenetre");
    txt_c(sz, cx, cy + ph/2 - M_PAD - 10, 9, C_DIM);

    return act;
}

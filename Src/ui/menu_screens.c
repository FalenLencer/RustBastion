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
        m->screen = MENU_WORLD_MAP;
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
        ? "5 environnements en sequence — la ferraille se gagne ici"
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
                clip_text(raw, sw - 120 - M_IN*2, 12, cbuf, sizeof(cbuf));
                draw_text_boxed(cbuf, tx, ty, 12, C_GOLD);
            } else {
                dtxt(TextFormat("ARCADE  —  %s", si->theme_name),
                         tx, ty, 12, C_BLUE);
            }
            ty += 16;
            dtxt(TextFormat("Vague %d  |  %d vies  |  %d or",
                         si->wave, si->lives, si->gold),
                     tx, ty, 10, C_TEXT);
            ty += M_LINE;
            dtxt(TextFormat("Emplacement %d", i+1), tx, ty, 9, C_DIM);

            int bw2 = 88, bh2 = 26;
            int bx2 = sx + sw - bw2 - M_IN - 22;
            int by2 = y  + sh/2 - bh2/2;
            if (draw_btn("REPRENDRE", bx2, by2, bw2, bh2, C_GREEN, 0)) {
                act.resume_slot        = i;
                act.resume_is_campaign = is_campaign;
                act.go_game            = 1;
            }

            Rectangle xr = {(float)(sx+sw-M_IN-18), (float)(y+M_IN/2), 18, 18};
            int xhov = vhov_r(xr);
            DrawRectangleRounded(xr, 0.3f, 4,
                xhov ? (Color){48,6,6,255} : (Color){28,4,4,255});
            DrawRectangleRoundedLinesEx(xr, 0.3f, 4, 1.2f,
                xhov ? C_RED : (Color){90,16,16,255});
            int xw = mtxt("x", 10);
            dtxt("x", (int)(xr.x + xr.width/2 - xw/2),
                     (int)(xr.y + xr.height/2 - 5), 10, C_RED);
            if (vclick_r(xr)) {
                m->confirm_del_slot = i;
                m->screen = MENU_CONFIRM_DEL;
            }

        } else {
            dtxt(TextFormat("Emplacement %d — vide", i+1),
                     tx, y + sh/2 - 8, 11, C_DIM);
            int bw2 = 180, bh2 = 26;
            int bx2 = sx + sw - bw2 - M_IN;
            int by2 = y  + sh/2 - bh2/2;
            const char *lbl = is_campaign ? "NOUVELLE CAMPAGNE" : "NOUVELLE ARCADE";
            Color lc = is_campaign ? C_GOLD : C_BLUE;
            if (draw_btn(lbl, bx2, by2, bw2, bh2, lc, 0)) {
                m->new_slot            = i;
                m->campaign_order_seed = 0;
                m->screen = is_campaign ? MENU_NEW_CAMPAIGN : MENU_NEW_ARCADE;
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

    if (m->campaign_order_seed == 0)
        m->campaign_order_seed = GetRandomValue(1, 999999);

    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(m->campaign_order_seed, themes);

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
    dtxt("Ordre des environnements :", px, py, 10, C_DIM);
    py += M_LINE + 2;

    static const Color stage_cols[CAMPAIGN_STAGES] = {
        {232,152, 32,255},{42,190,105,255},{48,140,205,255},
        {142, 80,168,255},{215,118, 28,255},
    };

    for (int i = 0; i < CAMPAIGN_STAGES; i++) {
        const Theme *th = theme_get((ThemeID)themes[i]);
        Rectangle row = {(float)px,(float)py,(float)(pw - M_PAD*2), 28};
        DrawRectangleRounded(row, (float)PANEL_R/28, 5, (Color){18,10,3,210});
        DrawRectangleRoundedLinesEx(row, (float)PANEL_R/28, 5, 1.0f,
            (Color){stage_cols[i].r/3,stage_cols[i].g/3,stage_cols[i].b/3, 180});
        dtxt(TextFormat("%d.", i+1), px + M_IN, py + 8, 11, C_DIM);
        dtxt(th->name, px + M_IN + 22, py + 8, 11, stage_cols[i]);
        int avail = pw - M_PAD*2 - M_IN - 22 - mtxt(th->name, 11) - M_IN*2;
        if (avail > 40) {
            char dbuf[48];
            clip_text(th->description, avail, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf, px + M_IN + 22 + mtxt(th->name,11) + M_IN, py + 10, 9, C_DIM);
        }
        py += 28 + 4;
    }

    py = cy + ph/2 - M_PAD - BTN_H;
    int half = (pw - M_PAD*2 - M_IN) / 2;

    if (draw_btn("LANCER", px, py, half, BTN_H, C_GREEN, 0)) {
        act.start_campaign      = 1;
        act.new_slot            = m->new_slot;
        act.campaign_order_seed = m->campaign_order_seed;
    }
    if (draw_btn("ANNULER", px + half + M_IN, py,
                 pw - M_PAD*2 - M_IN - half, BTN_H, C_DIM, 0)) {
        m->campaign_order_seed = 0;
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
        if (m->back_screen == MENU_CAMPAIGN)
            campaign_save_delete(m->confirm_del_slot);
        else
            save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        m->screen = (m->back_screen == MENU_CAMPAIGN ||
                     m->back_screen == MENU_ARCADE)
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

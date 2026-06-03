/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_mp.c ─ Écrans multijoueur : hub (choix du mode) + lobby (code de
 *  session, ready-up, lancement). Les intentions (héberger/rejoindre/prêt/
 *  lancer/quitter) sont renvoyées via MenuAction et exécutées par app.c, qui
 *  gère la NetSession et remplit m->mp_view (état lu ici pour l'affichage).
 */
#include "menu_internal.h"
#include <ctype.h>

static const char *MP_MODE_NAMES[MP_MODE_COUNT] = {
    "", "COURSE", "DUEL", "BATISSEUR vs ENVAHISSEUR", "CO-OP",
};
static const char *MP_MODE_DESC[MP_MODE_COUNT] = {
    "",
    "Course : memes vagues, boards separes. Dernier debout / meilleur score.",
    "Duel : survivez ET envoyez des ennemis chez l'adversaire.",
    "Asymetrique : l'un defend, l'autre commande la horde.",
    "Co-op : defendez les memes bases ensemble.",
};
// Seul COURSE est jouable pour l'instant (les autres arrivent).
static const int MP_MODE_ENABLED[MP_MODE_COUNT] = { 0, 1, 0, 0, 0 };

// ════════════════════════════════════════════════════
// HUB — choix du mode + héberger / rejoindre
// ════════════════════════════════════════════════════
MenuAction draw_mp_hub(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("MULTIJOUEUR", vw);
    txt_c("Choisissez un mode, puis hebergez ou rejoignez par code.",
          cx, M_PAD + 78, 10, C_TEXT);

    if (m->mp_mode <= MP_NONE || m->mp_mode >= MP_MODE_COUNT) m->mp_mode = MP_COURSE;

    int bw = 540, bh = 44, gap = M_IN - 2;
    int bx = cx - bw/2, by = M_PAD + 104;
    for (int mode = MP_COURSE; mode < MP_MODE_COUNT; mode++) {
        int en   = MP_MODE_ENABLED[mode];
        int seld = (m->mp_mode == mode);
        char lbl[72];
        snprintf(lbl, sizeof(lbl), "%s%s", MP_MODE_NAMES[mode], en ? "" : "   (a venir)");
        Color c = en ? (seld ? C_GOLD : C_TEXT) : C_DIM;
        if (draw_btn(lbl, bx, by, bw, bh, c, seld) && en) m->mp_mode = mode;
        by += bh + gap;
    }

    by += 4;
    txt_c(MP_MODE_DESC[m->mp_mode], cx, by, 9, C_DIM);
    by += 24;

    int abw = 256, agap = M_IN;
    int ax  = cx - abw - agap/2;
    if (draw_btn("HEBERGER", ax, by, abw, BTN_H + 6, C_GREEN, 0)) {
        m->mp_role  = 1;
        m->screen   = MENU_MP_LOBBY;
        act.mp_host = 1;
        act.mp_mode = m->mp_mode;
    }
    if (draw_btn("REJOINDRE", ax + abw + agap, by, abw, BTN_H + 6, C_BLUE, 0)) {
        m->mp_role     = 2;
        m->screen      = MENU_MP_LOBBY;
        m->mp_code_input[0] = '\0';
        m->mp_code_len = 0;
    }

    if (draw_back_btn(vw, vh)) { m->screen = m->back_screen; pop_back_screen(m); }
    return act;
}

// ════════════════════════════════════════════════════
// LOBBY — code de session, ready-up, lancement
// ════════════════════════════════════════════════════
MenuAction draw_mp_lobby(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;
    draw_bg(m, vw, vh);
    draw_header(m->mp_role == 1 ? "HEBERGER" : "REJOINDRE", vw);
    const MpLobbyView *v = &m->mp_view;

    int pw = 540, ph = 330;
    draw_panel(cx, cy, pw, ph, C_GOLD);
    int py = cy - ph/2 + M_PAD;

    txt_c(TextFormat("Mode : %s", MP_MODE_NAMES[m->mp_mode]), cx, py, 12, C_GOLD);
    py += fh(12) + 10;

    int join_input = (m->mp_role == 2 && (!v->active || v->failed));

    if (join_input) {
        // ── Saisie du code (rejoindre) ───────────────────────
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (m->mp_code_len < 18 &&
                (isalnum(ch) || ch == '-')) {
                m->mp_code_input[m->mp_code_len++] = (char)toupper(ch);
                m->mp_code_input[m->mp_code_len] = '\0';
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && m->mp_code_len > 0)
            m->mp_code_input[--m->mp_code_len] = '\0';

        txt_c("Entrez le code transmis par l'hote :", cx, py, 10, C_TEXT);
        py += fh(10) + 8;
        Rectangle f = {(float)(cx - 160), (float)py, 320.0f, 36.0f};
        DrawRectangleRounded(f, 0.3f, 5, (Color){8, 5, 2, 255});
        DrawRectangleRoundedLinesEx(f, 0.3f, 5, 1.5f, C_BORDER);
        dtxt(m->mp_code_input[0] ? m->mp_code_input : "________",
             (int)f.x + 14, (int)f.y + 10, 16, C_GOLD);
        py += 46;
        if (v->failed) {
            txt_c("Echec : code invalide ou hote injoignable.", cx, py, 9, C_RED);
            py += fh(9) + 6;
        }
        int ok = (m->mp_code_len >= 10);
        if (draw_btn("REJOINDRE", cx - 110, py, 220, BTN_H, ok ? C_GREEN : C_DIM, 0) && ok) {
            act.mp_join = 1;
            snprintf(act.mp_code, sizeof(act.mp_code), "%s", m->mp_code_input);
        }
    } else {
        // ── Hôte : affiche le code ; sinon attente/connexion ─
        if (m->mp_role == 1) {
            txt_c("Code de session — transmettez-le a votre ami :", cx, py, 10, C_TEXT);
            py += fh(10) + 8;
            txt_c(v->code[0] ? v->code : "...", cx, py, 28, C_GOLD);
            py += fh(28) + 14;
        }

        if (!v->peer_connected) {
            txt_c(m->mp_role == 1 ? "En attente d'un joueur..." : "Connexion en cours...",
                  cx, py, 12, C_DIM);
            py += fh(12) + 10;
        } else {
            txt_c(TextFormat("Adversaire : %s", v->peer_name[0] ? v->peer_name : "?"),
                  cx, py, 12, C_TEXT);
            py += fh(12) + 8;
            txt_c(TextFormat("Vous : %s     Lui : %s",
                             v->my_ready ? "PRET" : "en attente",
                             v->peer_ready ? "PRET" : "en attente"),
                  cx, py, 10, v->my_ready && v->peer_ready ? C_GREEN : C_TEXT);
            py += fh(10) + 12;

            if (draw_btn(v->my_ready ? "ANNULER PRET" : "JE SUIS PRET",
                         cx - 130, py, 260, BTN_H,
                         v->my_ready ? C_ORANGE : C_GREEN, 0))
                act.mp_ready = 1;
            py += BTN_H + 8;

            if (m->mp_role == 1) {
                int can = v->my_ready && v->peer_ready;
                if (draw_btn("LANCER LA PARTIE", cx - 130, py, 260, BTN_H,
                             can ? C_GOLD : C_DIM, 0) && can)
                    act.mp_start = 1;
            } else {
                txt_c("En attente du lancement par l'hote...", cx, py + 6, 9, C_DIM);
            }
        }
    }

    if (draw_back_btn(vw, vh)) {
        act.mp_leave = 1;
        m->screen = MENU_MP_HUB;
    }
    return act;
}

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_mp.c ─ Écrans multijoueur : hub (mode + pseudo) + lobby (code de
 *  session, copier/coller, ready-up, lancement). Les intentions partent via
 *  MenuAction ; app.c gère la NetSession et remplit m->mp_view.
 */
#include "menu_internal.h"
#include <ctype.h>
#include <string.h>

static const char *MP_MODE_NAMES[MP_MODE_COUNT] = {
    "", "COURSE", "DUEL", "BATISSEUR vs ENVAHISSEUR", "CO-OP",
};
static const char *MP_MODE_DESC[MP_MODE_COUNT] = {
    "",
    "Course : memes vagues, boards separes. Dernier debout / meilleur score.",
    "Duel : survivez ET envoyez des ennemis chez l'adversaire (touches Z/C/V).",
    "Asymetrique : l'hote defend, l'invite commande la horde.",
    "Co-op : tenez ensemble, l'un tombe = tous perdent. Entraide [G].",
};
static const int MP_MODE_ENABLED[MP_MODE_COUNT] = { 0, 1, 1, 1, 1 };

// ── Champ texte cliquable (focus = m->mp_edit_field) ─────────
// filter : 0 = texte libre, 1 = code (alphanum + '-', maj), 2 = IP (chiffres + '.'),
//          3 = IP:port (chiffres + '.' + ':')
static void text_field(MenuState *m, int field_id, Rectangle r,
                       char *buf, int cap, int filter) {
    int focused = (m->mp_edit_field == field_id);
    if (vclick_r(r)) { m->mp_edit_field = field_id; focused = 1; }

    DrawRectangleRounded(r, 0.3f, 5, (Color){8, 5, 2, 255});
    DrawRectangleRoundedLinesEx(r, 0.3f, 5, focused ? 2.0f : 1.2f,
                                focused ? C_GOLD : C_BORDER);
    if (focused) {
        int ch;
        while ((ch = GetCharPressed()) != 0) {
            int len = (int)strlen(buf);
            int ok = (filter == 1) ? (isalnum(ch) || ch == '-')
                   : (filter == 2) ? (isdigit(ch) || ch == '.')
                   : (filter == 3) ? (isdigit(ch) || ch == '.' || ch == ':')
                   : (ch >= 32 && ch < 127);
            char c = (filter == 1) ? (char)toupper(ch) : (char)ch;
            if (ok && len < cap - 1) { buf[len] = c; buf[len + 1] = '\0'; }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = (int)strlen(buf);
            if (len > 0) buf[len - 1] = '\0';
        }
    }
    dtxt(buf[0] ? buf : "...", (int)r.x + 10,
         (int)r.y + (int)r.height/2 - fh(13)/2, 13, buf[0] ? C_GOLD : C_DIM);
    if (focused && ((int)(GetTime() * 2) % 2)) {       // curseur clignotant
        int w = mtxt(buf, 13);
        DrawRectangle((int)r.x + 12 + w, (int)r.y + 7, 2, (int)r.height - 14, C_GOLD);
    }
}

// Colle le presse-papiers dans `buf` (filtré code base32 + maj).
static void paste_code(char *buf, int cap) {
    const char *clip = GetClipboardText();
    if (!clip) return;
    int o = 0;
    for (int i = 0; clip[i] && o < cap - 1; i++) {
        char c = clip[i];
        if (isalnum((unsigned char)c) || c == '-')
            buf[o++] = (char)toupper((unsigned char)c);
    }
    buf[o] = '\0';
}

// ════════════════════════════════════════════════════
// HUB — mode + pseudo + héberger / rejoindre
// ════════════════════════════════════════════════════
MenuAction draw_mp_hub(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("MULTIJOUEUR", vw);

    if (m->mp_mode <= MP_NONE || m->mp_mode >= MP_MODE_COUNT) m->mp_mode = MP_COURSE;

    // ── Pseudo ───────────────────────────────────────────────
    int py = M_PAD + 86;
    txt_c("Votre pseudo :", cx, py, 10, C_TEXT);
    py += fh(10) + 4;
    text_field(m, 1, (Rectangle){(float)(cx - 140), (float)py, 280.0f, 30.0f},
               m->opts.player_name, (int)sizeof(m->opts.player_name), 0);
    py += 38;

    // ── Serveur relais (optionnel) ───────────────────────────
    txt_c("Serveur relais (IP:port)  -  vide = connexion directe", cx, py, 8, C_DIM);
    py += fh(8) + 3;
    text_field(m, 4, (Rectangle){(float)(cx - 140), (float)py, 280.0f, 26.0f},
               m->opts.relay_addr, (int)sizeof(m->opts.relay_addr), 3);
    py += 32;

    // ── Cartes de mode ───────────────────────────────────────
    int bw = 540, bh = 38, gap = 6, bx = cx - bw/2;
    for (int mode = MP_COURSE; mode < MP_MODE_COUNT; mode++) {
        int en   = MP_MODE_ENABLED[mode];
        int seld = (m->mp_mode == mode);
        char lbl[72];
        snprintf(lbl, sizeof(lbl), "%s%s", MP_MODE_NAMES[mode], en ? "" : "   (a venir)");
        Color c = en ? (seld ? C_GOLD : C_TEXT) : C_DIM;
        if (draw_btn(lbl, bx, py, bw, bh, c, seld) && en) { m->mp_mode = mode; m->mp_edit_field = 0; }
        py += bh + gap;
    }
    py += 6;
    txt_c(MP_MODE_DESC[m->mp_mode], cx, py, 9, C_DIM);
    py += fh(9) + 14;

    int abw = 256, agap = M_IN, ax = cx - abw - agap/2;
    if (draw_btn("HEBERGER", ax, py, abw, BTN_H + 6, C_GREEN, 0)) {
        m->mp_edit_field = 0;
        // Réglages PAR DÉFAUT (ajustables sur l'écran simple, ou avancé).
        m->mp_cfg = (CustomConfig){0};
        m->mp_cfg.theme          = THEME_COUNT;   // aléatoire
        m->mp_cfg.forced_bases   = 1;
        m->mp_cfg.forced_spawns  = 2;
        m->mp_cfg.min_dist       = 10;
        m->mp_cfg.forced_deposits = 0;
        m->mp_cfg.scale_cap      = 6.0f;
        m->mp_cfg.count_mult     = 1.0f;
        m->mp_cfg.map_w = 0; m->mp_cfg.map_h = 0;
        m->mp_host_invader = 0;
        m->screen = MENU_MP_CONFIG;   // écran simple (rapide) → avancé si besoin
    }
    if (draw_btn("REJOINDRE", ax + abw + agap, py, abw, BTN_H + 6, C_BLUE, 0)) {
        m->mp_role     = 2;  m->mp_edit_field = 2;
        m->screen      = MENU_MP_LOBBY;
        m->mp_code_input[0] = '\0';  m->mp_code_len = 0;
    }

    if (draw_back_btn(vw, vh)) {
        act.mp_leave = 1;                 // ferme toute session orpheline
        m->mp_edit_field = 0;
        m->screen = m->back_screen;
        pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// CONFIG SIMPLE — réglages rapides (ou accès aux réglages avancés)
// ════════════════════════════════════════════════════
MenuAction draw_mp_config(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("CONFIGURATION DE LA PARTIE", vw);

    if (m->mp_mode <= MP_NONE || m->mp_mode >= MP_MODE_COUNT) m->mp_mode = MP_COURSE;
    CustomConfig *c = &m->mp_cfg;

    int py = M_PAD + 96;
    txt_c(TextFormat("Mode : %s", MP_MODE_NAMES[m->mp_mode]), cx, py, 12, C_GOLD);
    py += fh(12) + 8;
    txt_c("Reglages rapides — le reste est aleatoire/equilibre par defaut.",
          cx, py, 9, C_DIM);
    py += fh(9) + 22;

    int bw = 170, gap = 10, x0 = cx - (3*bw + 2*gap)/2;

    // ── Difficulté ──
    txt_c("Difficulte", cx, py, 11, C_TEXT);
    py += fh(11) + 6;
    {
        const char *labels[3] = { "Facile", "Normal", "Difficile" };
        const float caps [3]  = { 4.0f, 6.0f, 9.0f };
        const float mults[3]  = { 0.5f, 1.0f, 1.5f };
        int sel = (c->scale_cap <= 4.5f) ? 0 : (c->scale_cap <= 7.0f) ? 1 : 2;
        for (int i = 0; i < 3; i++) {
            int active = (sel == i);
            if (draw_btn(labels[i], x0 + i*(bw+gap), py, bw, BTN_H,
                         active ? C_GOLD : C_TEXT, active)) {
                c->scale_cap = caps[i]; c->count_mult = mults[i];
            }
        }
    }
    py += BTN_H + 22;

    // ── Taille de la carte ──
    txt_c("Taille de la carte", cx, py, 11, C_TEXT);
    py += fh(11) + 6;
    {
        const char *labels[3] = { "Standard", "Grande", "Enorme" };
        const int ws[3] = { 0, 38, 48 };
        const int hs[3] = { 0, 22, 28 };
        int sel = (c->map_w <= 0) ? 0 : (c->map_w <= 40) ? 1 : 2;
        for (int i = 0; i < 3; i++) {
            int active = (sel == i);
            if (draw_btn(labels[i], x0 + i*(bw+gap), py, bw, BTN_H,
                         active ? C_GOLD : C_TEXT, active)) {
                c->map_w = ws[i]; c->map_h = hs[i];
            }
        }
    }
    py += BTN_H + 30;

    // ── Héberger (gros) + accès aux réglages avancés ──
    if (draw_btn("HEBERGER LA PARTIE", cx - 165, py, 330, BTN_H + 8, C_GREEN, 0)) {
        m->mp_role  = 1;  m->mp_edit_field = 0;
        m->screen   = MENU_MP_LOBBY;
        act.mp_host = 1;  act.mp_mode = m->mp_mode;
    }
    py += BTN_H + 8 + 12;
    if (draw_btn("Reglages avances...", cx - 125, py, 250, BTN_H, C_BLUE, 0))
        m->screen = MENU_MP_CONFIG_ADV;

    if (draw_back_btn(vw, vh))
        m->screen = MENU_MP_HUB;
    return act;
}

// ════════════════════════════════════════════════════
// LOBBY — code, copier/coller, ready-up, lancement
// ════════════════════════════════════════════════════
MenuAction draw_mp_lobby(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;
    draw_bg(m, vw, vh);
    draw_header(m->mp_role == 1 ? "HEBERGER" : "REJOINDRE", vw);
    const MpLobbyView *v = &m->mp_view;

    int pw = 560, ph = (m->mp_mode == MP_ASYM) ? 400 : 340;  // +place pour le rôle Asym
    draw_panel(cx, cy, pw, ph, C_GOLD);
    int py = cy - ph/2 + M_PAD + 2;

    // En-tête : mode (+ rôle en Asym)
    txt_c(TextFormat("Mode : %s", MP_MODE_NAMES[m->mp_mode]), cx, py, 12, C_GOLD);
    py += fh(12) + 6;
    if (m->mp_mode == MP_ASYM) {
        txt_c(m->mp_role == 1 ? "Votre role : DEFENSEUR" : "Votre role : ENVAHISSEUR",
              cx, py, 9, (Color){180, 140, 200, 255});
        py += fh(9) + 4;
    } else {
        py += 4;
    }
    draw_sep(cx - pw/2 + M_PAD, py, pw - M_PAD*2, C_BORDER);
    py += 12;

    if (!v->peer_connected) {
        // ════════ PRÉ-CONNEXION ════════
        if (m->mp_role == 1 && v->via_relay) {
            // ── HÔTE via SERVEUR RELAIS : juste le code (pas d'IP/UPnP) ──
            txt_c("Connecte au serveur relais.", cx, py, 10, (Color){140, 200, 160, 255});
            py += fh(10) + 8;
            txt_c("Code a transmettre :", cx, py, 10, C_TEXT);
            py += fh(10) + 4;
            txt_c(v->code[0] ? v->code : "...", cx, py, 24, C_GOLD);
            py += fh(24) + 8;
            if (draw_btn("COPIER", cx - 80, py, 160, 32, C_BLUE, 0))
                SetClipboardText(v->code);
            py += 42;
            txt_c("En attente d'un joueur (via serveur)...", cx, py, 11, C_DIM);
        } else if (m->mp_role == 1) {
            // ── HÔTE direct : IP + code + copier + UPnP + attente ──
            txt_c("Votre IP (LAN par defaut) :", cx, py, 9, C_TEXT);
            py += fh(9) + 4;
            text_field(m, 3, (Rectangle){(float)(cx - 100), (float)py, 200.0f, 30.0f},
                       m->mp_host_ip, (int)sizeof(m->mp_host_ip), 2);
            py += 38;
            txt_c("Code a transmettre :", cx, py, 10, C_TEXT);
            py += fh(10) + 4;
            txt_c(v->code[0] ? v->code : "...", cx, py, 24, C_GOLD);
            py += fh(24) + 6;
            if (draw_btn("COPIER", cx - 166, py, 160, 32, C_BLUE, 0))
                SetClipboardText(v->code);
            // UPNP_HOOK : ouverture automatique du port (sans config manuelle)
            if (draw_btn("OUVRIR PORT (UPnP)", cx + 6, py, 160, 32, C_ORANGE, 0))
                act.mp_upnp = 1;
            py += 40;
            txt_c("En attente d'un joueur...", cx, py, 11, C_DIM);
            py += fh(11) + 4;
            if (m->mp_upnp_msg[0])
                txt_c(m->mp_upnp_msg, cx, py, 8, m->mp_upnp_ok ? C_GREEN : C_RED);
            else
                txt_c("LAN : gardez l'IP.  A distance : Tailscale, ou bouton UPnP.",
                      cx, py, 8, (Color){110, 92, 58, 255});
        } else if (!v->active || v->failed) {
            // ── INVITÉ : saisie du code ──
            txt_c("Code transmis par l'hote :", cx, py, 10, C_TEXT);
            py += fh(10) + 6;
            text_field(m, 2, (Rectangle){(float)(cx - 150), (float)py, 224.0f, 34.0f},
                       m->mp_code_input, (int)sizeof(m->mp_code_input), 1);
            if (draw_btn("COLLER", cx + 86, py, 100, 34, C_BLUE, 0))
                paste_code(m->mp_code_input, (int)sizeof(m->mp_code_input));
            py += 42;
            if (v->failed) {
                txt_c("Echec : code invalide ou hote injoignable (meme LAN requis).",
                      cx, py, 9, C_RED);
                py += fh(9) + 6;
            }
            m->mp_code_len = (int)strlen(m->mp_code_input);
            int ok = (m->mp_code_len >= 10);
            if (draw_btn("REJOINDRE", cx - 110, py, 220, BTN_H, ok ? C_GREEN : C_DIM, 0) && ok) {
                act.mp_join = 1;
                snprintf(act.mp_code, sizeof(act.mp_code), "%s", m->mp_code_input);
            }
        } else {
            // ── INVITÉ : connexion en cours ──
            txt_c("Connexion a l'hote...", cx, py + 14, 13, C_DIM);
        }
    } else {
        // ════════ CONNECTÉ : adversaire + ready + lancement ════════
        txt_c(TextFormat("Adversaire : %s", v->peer_name[0] ? v->peer_name : "?"),
              cx, py, 14, C_TEXT);
        py += fh(14) + 12;
        txt_c(TextFormat("Vous : %s        Lui : %s",
                         v->my_ready ? "PRET" : "en attente",
                         v->peer_ready ? "PRET" : "en attente"),
              cx, py, 11, v->my_ready && v->peer_ready ? C_GREEN : C_TEXT);
        py += fh(11) + 18;

        if (draw_btn(v->my_ready ? "ANNULER PRET" : "JE SUIS PRET",
                     cx - 130, py, 260, BTN_H, v->my_ready ? C_ORANGE : C_GREEN, 0))
            act.mp_ready = 1;
        py += BTN_H + 12;

        // Asym (hôte) : choix de qui joue l'envahisseur, avant le lancement.
        if (m->mp_role == 1 && m->mp_mode == MP_ASYM) {
            txt_c("Qui joue l'ENVAHISSEUR ?", cx, py, 10, C_GOLD);
            py += fh(10) + 4;
            int bw3 = 150, gap3 = 10, x0 = cx - (2*bw3 + gap3)/2;
            int hi = m->mp_host_invader;
            if (draw_btn("VOUS (hote)", x0, py, bw3, 30, hi ? C_GOLD : C_TEXT, hi))
                m->mp_host_invader = 1;
            if (draw_btn("L'INVITE", x0 + bw3 + gap3, py, bw3, 30, hi ? C_TEXT : C_GOLD, !hi))
                m->mp_host_invader = 0;
            py += 30 + 12;
        }

        if (m->mp_role == 1) {
            int can = v->my_ready && v->peer_ready;
            if (draw_btn("LANCER LA PARTIE", cx - 130, py, 260, BTN_H,
                         can ? C_GOLD : C_DIM, 0) && can)
                act.mp_start = 1;
            if (!can) {
                py += BTN_H + 6;
                txt_c("(les deux joueurs doivent etre prets)", cx, py, 8, C_DIM);
            }
        } else {
            txt_c("En attente du lancement par l'hote...", cx, py + 8, 10, C_DIM);
        }
    }

    if (draw_back_btn(vw, vh)) {
        act.mp_leave     = 1;
        m->mp_edit_field = 0;
        m->screen        = MENU_MP_HUB;
    }
    return act;
}

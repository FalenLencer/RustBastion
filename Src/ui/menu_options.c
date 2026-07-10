/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_options.c ─ Écran des paramètres.
 *
 *  Contient :
 *    draw_options  — Panneau options (onglets : Général, Audio, Graphismes)
 *
 *  Notes de layout :
 *    • Chaque groupe = label doré (16 px) + bouton (BTN_H) + marge OPT_GAP.
 *      Toujours avancer de BTN_H + OPT_GAP après un bouton (sinon le
 *      groupe suivant chevauche — bug historique corrigé).
 *    • Les listes déroulantes (FPS, résolution) sont dessinées EN OVERLAY
 *      en fin de fonction : elles passent PAR-DESSUS le contenu au lieu de
 *      le pousser hors du panneau. Quand une liste est ouverte, les autres
 *      widgets ignorent les clics (ui_locked) ; un clic hors liste ferme.
 *    • Pas d'accents dans les chaînes UI (la police gère mal certains
 *      glyphes) — convention projet.
 */

#include "menu_internal.h"
#include "../combat/fx.h"   // g_fx.enabled (toggle effets de jus)

#define OPT_GAP    20   // espace vertical entre deux groupes label+bouton
#define OPT_GAP_S   8   // espace entre boutons d'une même section
#define DD_ITEM_H  26   // hauteur d'un item de liste déroulante
#define DD_ITEM_SP 30   // pas vertical entre items

// ── Choix des listes déroulantes ─────────────────────────────────
static const int   FPS_OPTS[5] = {30, 60, 120, 165, 0};
static const char *FPS_LBL [5] = {"30 FPS","60 FPS","120 FPS","165 FPS","Illimite"};

static const int RES[5][2] = {
    {1120,770},{1400,962},{1680,1154},{1960,1346},{2240,1540}
};
static const char *RLBL[5] = {
    "1120 x 770  (1x)","1400 x 962  (1.25x)","1680 x 1154 (1.5x)",
    "1960 x 1346 (1.75x)","2240 x 1540 (2x)",
};

// ── Nom lisible d'un code de touche raylib (buffer appelant) ─────
void opts_key_name(int key, char *out, int outsz) {
    if (key >= KEY_A && key <= KEY_Z) {
        snprintf(out, outsz, "%c", 'A' + (key - KEY_A));
        return;
    }
    if (key >= KEY_ZERO && key <= KEY_NINE) {
        snprintf(out, outsz, "%c", '0' + (key - KEY_ZERO));
        return;
    }
    if (key >= KEY_F1 && key <= KEY_F12) {
        snprintf(out, outsz, "F%d", key - KEY_F1 + 1);
        return;
    }
    if (key >= KEY_KP_0 && key <= KEY_KP_9) {
        snprintf(out, outsz, "PAVE %d", key - KEY_KP_0);
        return;
    }
    const char *n = NULL;
    switch (key) {
        case KEY_SPACE:         n = "ESPACE";    break;
        case KEY_LEFT_SHIFT:    n = "SHIFT G";   break;
        case KEY_RIGHT_SHIFT:   n = "SHIFT D";   break;
        case KEY_LEFT_CONTROL:  n = "CTRL G";    break;
        case KEY_RIGHT_CONTROL: n = "CTRL D";    break;
        case KEY_LEFT_ALT:      n = "ALT";       break;
        case KEY_TAB:           n = "TAB";       break;
        case KEY_ENTER:         n = "ENTREE";    break;
        case KEY_BACKSPACE:     n = "RETOUR AR"; break;
        case KEY_UP:            n = "FL. HAUT";  break;
        case KEY_DOWN:          n = "FL. BAS";   break;
        case KEY_LEFT:          n = "FL. GAUCHE";break;
        case KEY_RIGHT:         n = "FL. DROITE";break;
        case KEY_SEMICOLON:     n = "; (M azerty)"; break;
        case KEY_COMMA:         n = ",";         break;
        case KEY_PERIOD:        n = ".";         break;
        default: break;
    }
    if (n) snprintf(out, outsz, "%s", n);
    else   snprintf(out, outsz, "CODE %d", key);
}

// ════════════════════════════════════════════════════
// OPTIONS
// ════════════════════════════════════════════════════
MenuAction draw_options(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    if (m->paused)
        DrawRectangle(0, 0, vw, vh, (Color){0,0,0,155});
    else
        draw_bg(m, vw, vh);

    int pw = 600, ph = 500;
    draw_panel(cx, cy, pw, ph, C_BORDER);

    int px = cx - pw/2;
    int py = cy - ph/2;
    int iw = pw - M_PAD*2;

    txt_c_boxed("OPTIONS", cx, py + M_PAD, 19, C_GOLD);
    py += M_PAD + fh(19) + 3 + 6;
    draw_sep(px + M_PAD, py, iw, C_BORDER);
    py += M_IN + 4;

    /* Liste déroulante ouverte OU attente de touche = clics verrouillés. */
    int ui_locked = (m->opt_dropdown_open >= 0) || (m->opt_rebind > 0);

    // ── Onglets horizontaux ────────────────────────────────────
    const char *tabs[4] = {"General", "Audio", "Graphismes", "Commandes"};
    int tab_w = 120, tab_h = 24, tab_gap = 10;
    int tabs_total = tab_w * 4 + tab_gap * 3;
    int tab_start_x = cx - tabs_total/2;

    for (int i = 0; i < 4; i++) {
        int tx = tab_start_x + i * (tab_w + tab_gap);
        Rectangle tr = {(float)tx, (float)py, (float)tab_w, (float)tab_h};
        int is_sel = (m->opt_tab == i);
        int hov = vhov_r(tr);

        Color bg = is_sel ? (Color){20, 15, 5, 255} : C_PANEL;
        Color bd = is_sel ? C_GOLD : C_BORDER;
        float bw = is_sel ? 2.0f : 1.0f;

        DrawRectangleRounded(tr, 0.3f, 4, bg);
        DrawRectangleRoundedLinesEx(tr, 0.3f, 4, bw, bd);

        int tlen = mtxt(tabs[i], 11);
        dtxt(tabs[i], tx + tab_w/2 - tlen/2, py + tab_h/2 - 6, 11,
                 is_sel ? C_GOLD : C_TEXT);

        if (!ui_locked && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m->opt_tab = i;
            m->opt_dropdown_open = -1;   // change d'onglet = ferme la liste
        }
    }

    py += tab_h + 16;

    // ── Contenu des onglets ────────────────────────────────────
    int content_x = px + M_PAD;
    int content_y = py;
    int content_w = iw;

    /* Position de la liste déroulante ouverte (posée au dessin du bouton
       parent, consommée par l'overlay en fin de fonction). */
    int dd_x = 0, dd_y = 0;

    switch (m->opt_tab) {
    case 0: // Général — fenêtre et cadence
        {
            int y = content_y;

            draw_text_boxed("Plein ecran", content_x, y, 11, C_GOLD);
            y += 16;
            /* Utilise l'état réel de la fenêtre comme source de vérité */
            int _is_fs = IsWindowFullscreen();
            m->opts.fullscreen = _is_fs; /* sync systématique */
            const char *fsl = _is_fs ? "[*] Active" : "[ ] Desactive";
            if (draw_btn(fsl, content_x, y, content_w, BTN_H, C_BLUE, _is_fs)
                && !ui_locked)
                act.toggle_fs = 1;
            y += BTN_H + OPT_GAP;

            draw_text_boxed("FPS Cible", content_x, y, 11, C_GOLD);
            y += 16;
            char fps_display[32];
            if (m->opts.target_fps == 0)
                snprintf(fps_display, sizeof(fps_display), "Illimite");
            else
                snprintf(fps_display, sizeof(fps_display), "%d FPS",
                         m->opts.target_fps);
            if (draw_btn(fps_display, content_x, y, content_w, BTN_H, C_BLUE, 0)
                && !ui_locked)
                m->opt_dropdown_open = 0;
            draw_dropdown_arrow(content_x, y, content_w, BTN_H, C_BLUE);
            dd_x = content_x;
            dd_y = y + BTN_H + 4;
            y += BTN_H + OPT_GAP;

            draw_text_boxed("Compteur FPS", content_x, y, 11, C_GOLD);
            y += 16;
            {
                const char *fps_lbl = m->opts.show_fps
                    ? "[*] Active" : "[ ] Desactive";
                if (draw_btn(fps_lbl, content_x, y, content_w, BTN_H,
                             C_BLUE, m->opts.show_fps) && !ui_locked)
                    m->opts.show_fps ^= 1;
            }
        }
        break;

    case 1: // Audio
        {
            int y = content_y;
            draw_text_boxed("Parametres Audio", content_x, y, 11, C_GOLD);
            y += 22;

            const char *music_toggle = m->opts.music_volume > 0
                ? "[*] Musique" : "[ ] Musique";
            if (draw_btn(music_toggle, content_x, y, content_w, BTN_H, C_BLUE,
                         m->opts.music_volume > 0) && !ui_locked) {
                m->opts.music_volume = (m->opts.music_volume > 0) ? 0 : 60;
                audio_set_music_volume(m->opts.music_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Musique", content_x, y, content_w,
                                   m->opts.music_volume, &m->opts.music_volume))
                audio_set_music_volume(m->opts.music_volume / 100.0f);
            y += 42;

            const char *sfx_toggle = m->opts.sfx_volume > 0
                ? "[*] Effets" : "[ ] Effets";
            if (draw_btn(sfx_toggle, content_x, y, content_w, BTN_H, C_BLUE,
                         m->opts.sfx_volume > 0) && !ui_locked) {
                m->opts.sfx_volume = (m->opts.sfx_volume > 0) ? 0 : 95;
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Effets", content_x, y, content_w,
                                   m->opts.sfx_volume, &m->opts.sfx_volume))
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
            y += 42;

            const char *master_toggle = m->opts.master_volume > 0
                ? "[*] Volume general" : "[ ] Volume general";
            if (draw_btn(master_toggle, content_x, y, content_w, BTN_H, C_BLUE,
                         m->opts.master_volume > 0) && !ui_locked) {
                m->opts.master_volume = (m->opts.master_volume > 0) ? 0 : 80;
                audio_set_master_volume(m->opts.master_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume general", content_x, y, content_w,
                                   m->opts.master_volume, &m->opts.master_volume))
                audio_set_master_volume(m->opts.master_volume / 100.0f);
        }
        break;

    case 2: // Graphismes — résolution + affichage en jeu + accessibilité
        {
            int y = content_y;

            draw_text_boxed("Resolution", content_x, y, 11, C_GOLD);
            y += 16;
            char res_display[64];
            snprintf(res_display, sizeof(res_display), "%d x %d",
                     m->opts.win_width, m->opts.win_height);
            if (draw_btn(res_display, content_x, y, content_w, BTN_H, C_BLUE, 0)
                && !ui_locked)
                m->opt_dropdown_open = 1;
            draw_dropdown_arrow(content_x, y, content_w, BTN_H, C_BLUE);
            dd_x = content_x;
            dd_y = y + BTN_H + 4;
            y += BTN_H + OPT_GAP;

            draw_text_boxed("Affichage en jeu", content_x, y, 11, C_GOLD);
            y += 16;
            {
                const char *nm_lbl = m->opts.show_entity_names
                    ? "[*] Noms des ennemis au-dessus des sprites"
                    : "[ ] Noms des ennemis au-dessus des sprites";
                if (draw_btn(nm_lbl, content_x, y, content_w, BTN_H,
                             C_BLUE, m->opts.show_entity_names) && !ui_locked)
                    m->opts.show_entity_names ^= 1;   // appliqué via app.c
            }
            y += BTN_H + OPT_GAP_S;
            {
                const char *fx_lbl = m->opts.fx_effects
                    ? "[*] Effets visuels (jus)"
                    : "[ ] Effets visuels (jus)";
                if (draw_btn(fx_lbl, content_x, y, content_w, BTN_H,
                             C_BLUE, m->opts.fx_effects) && !ui_locked) {
                    m->opts.fx_effects ^= 1;
                    g_fx.enabled = m->opts.fx_effects;   // application immédiate
                }
            }
            y += BTN_H + OPT_GAP;

            draw_text_boxed("Accessibilite", content_x, y, 11, C_GOLD);
            y += 16;
            {
                const char *cb_lbl = m->opts.colorblind
                    ? "[*] Daltonisme (couleurs ennemis distinctes)"
                    : "[ ] Daltonisme (couleurs ennemis distinctes)";
                if (draw_btn(cb_lbl, content_x, y, content_w, BTN_H,
                             C_BLUE, m->opts.colorblind) && !ui_locked)
                    m->opts.colorblind ^= 1;   // appliqué via g_colorblind (app.c)
            }
        }
        break;

    case 3: // Commandes — mode Héros : souris + touches configurables
        {
            int y = content_y;

            /* Mode souris : STANDARD (position absolue recentrée, fiable
               partout) ou BRUT (natif, si le driver le gère bien). */
            {
                const char *cm_lbl = m->opts.hero_mouse_native
                    ? "Souris : BRUTE (Windows natif)  -  clic : STANDARD"
                    : "Souris : STANDARD (recommande)  -  clic : brute";
                if (draw_btn(cm_lbl, content_x, y, content_w, 24,
                             C_BLUE, !m->opts.hero_mouse_native) && !ui_locked)
                    m->opts.hero_mouse_native ^= 1;
            }
            y += 24 + 6;
            {
                const char *iv_lbl = m->opts.hero_invert_y
                    ? "[*] Inverser l'axe vertical (souris)"
                    : "[ ] Inverser l'axe vertical (souris)";
                if (draw_btn(iv_lbl, content_x, y, content_w, 24,
                             C_BLUE, m->opts.hero_invert_y) && !ui_locked)
                    m->opts.hero_invert_y ^= 1;
            }
            y += 24 + 6;

            /* Sensibilités X et Y séparées (50 = normale, mêmes par défaut) */
            {
                int half = content_w / 2 - 6;
                if (draw_volume_slider("Sens. HORIZONTALE (50)",
                                       content_x, y, half,
                                       m->opts.hero_sens_x, &m->opts.hero_sens_x)) {
                    if (m->opts.hero_sens_x < 5) m->opts.hero_sens_x = 5;
                }
                if (draw_volume_slider("Sens. VERTICALE (50)",
                                       content_x + half + 12, y, half,
                                       m->opts.hero_sens_y, &m->opts.hero_sens_y)) {
                    if (m->opts.hero_sens_y < 5) m->opts.hero_sens_y = 5;
                }
            }
            y += 32;
            if (draw_volume_slider("Acceleration (50 = x1.0, lineaire)",
                                   content_x, y, content_w,
                                   m->opts.hero_accel, &m->opts.hero_accel)) {
                if (m->opts.hero_accel < 5) m->opts.hero_accel = 5;
            }
            y += 32;

            static const char *KLBL[HK_COUNT] = {
                "Avancer", "Reculer", "Pas a gauche", "Pas a droite",
                "Saut", "Sprint (maintenu)", "Interagir (ouvrier)",
                "Lancer la vague", "Changer d'arme", "Vue 1re / 3e",
            };
            int row_h = 17, row_sp = 19, key_w = 170;
            for (int i = 0; i < HK_COUNT; i++) {
                dtxt(KLBL[i], content_x, y + 4, 10, C_TEXT);
                char kn[24];
                if (m->opt_rebind == i + 1)
                    snprintf(kn, sizeof(kn), "APPUYEZ...");
                else
                    opts_key_name(m->opts.hero_keys[i], kn, sizeof(kn));
                if (draw_btn(kn, content_x + content_w - key_w, y,
                             key_w, row_h, C_BLUE, m->opt_rebind == i + 1)
                    && !ui_locked) {
                    m->opt_rebind = i + 1;   // prochaine touche = assignation
                }
                y += row_sp;
            }
        }
        break;
    }

    // ── Capture d'une touche (réassignation) ───────────────────
    if (m->opt_rebind > 0) {
        txt_c_boxed("Appuyez sur une touche  (ECHAP : annuler)",
                    cx, cy + ph/2 - M_PAD - BTN_H - 30, 11, C_GOLD);
        int k = GetKeyPressed();
        if (k == KEY_ESCAPE) {
            m->opt_rebind = 0;
        } else if (k > 0) {
            int a = m->opt_rebind - 1;
            /* Touche déjà utilisée par une autre action → échange. */
            for (int i = 0; i < HK_COUNT; i++)
                if (i != a && m->opts.hero_keys[i] == k)
                    m->opts.hero_keys[i] = m->opts.hero_keys[a];
            m->opts.hero_keys[a] = k;
            m->opt_rebind = 0;
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
        }
    }

    // Info résolution actuelle — au-dessus du bouton RETOUR (avant : le
    // texte était dessiné PAR-DESSUS le bouton, cy+208 dans cy+198..234)
    char info[48];
    snprintf(info, sizeof(info), "Fenetre actuelle : %dx%d",
             GetScreenWidth(), GetScreenHeight());
    txt_c(info, cx, cy + ph/2 - M_PAD - BTN_H - fh(9) - 6, 9, C_DIM);

    if (draw_btn("RETOUR", cx - 55, cy + ph/2 - M_PAD - BTN_H,
                 110, BTN_H, C_DIM, 0) && !ui_locked)
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;

    // ════════════════════════════════════════════════
    // LISTES DÉROULANTES — overlay par-dessus le contenu
    // ════════════════════════════════════════════════
    if (m->opt_dropdown_open == 0 && m->opt_tab == 0) {
        /* FPS cible */
        int picked = 0;
        Rectangle bgr = {(float)(dd_x - 4), (float)(dd_y - 4),
                         (float)(content_w + 8), (float)(5*DD_ITEM_SP + 4)};
        DrawRectangleRounded(bgr, 0.06f, 4, (Color){6, 4, 2, 250});
        DrawRectangleRoundedLinesEx(bgr, 0.06f, 4, 1.2f, C_BLUE);
        for (int i = 0; i < 5; i++) {
            if (draw_btn(FPS_LBL[i], dd_x, dd_y + i*DD_ITEM_SP, content_w,
                         DD_ITEM_H, C_TEXT,
                         m->opts.target_fps == FPS_OPTS[i])) {
                m->opts.target_fps = FPS_OPTS[i];
                SetTargetFPS(FPS_OPTS[i]);
                picked = 1;
            }
        }
        /* Clic (item ou extérieur) = fermeture ; hors clic la liste reste */
        if (picked || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m->opt_dropdown_open = -1;
    } else if (m->opt_dropdown_open == 1 && m->opt_tab == 2) {
        /* Résolution */
        int picked = 0;
        Rectangle bgr = {(float)(dd_x - 4), (float)(dd_y - 4),
                         (float)(content_w + 8), (float)(5*DD_ITEM_SP + 4)};
        DrawRectangleRounded(bgr, 0.06f, 4, (Color){6, 4, 2, 250});
        DrawRectangleRoundedLinesEx(bgr, 0.06f, 4, 1.2f, C_BLUE);
        for (int i = 0; i < 5; i++) {
            int cur = (m->opts.win_width == RES[i][0]);
            if (draw_btn(RLBL[i], dd_x, dd_y + i*DD_ITEM_SP, content_w,
                         DD_ITEM_H, C_TEXT, cur)) {
                m->opts.win_width  = RES[i][0];
                m->opts.win_height = RES[i][1];
                act.toggle_fs = 2;
                picked = 1;
            }
        }
        if (picked || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m->opt_dropdown_open = -1;
    } else if (m->opt_dropdown_open >= 0) {
        /* Liste orpheline (onglet changé par clavier, etc.) : fermer */
        m->opt_dropdown_open = -1;
    }

    return act;
}

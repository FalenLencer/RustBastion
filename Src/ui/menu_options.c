/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_options.c ─ Écran des paramètres.
 *
 *  Contient :
 *    draw_options  — Panneau options (onglets : Général, Audio, Graphismes)
 */

#include "menu_internal.h"

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

    // ── Onglets horizontaux ────────────────────────────────────
    const char *tabs[3] = {"General", "Audio", "Graphismes"};
    int tab_w = 120, tab_h = 24, tab_gap = 10;
    int tabs_total = tab_w * 3 + tab_gap * 2;
    int tab_start_x = cx - tabs_total/2;

    for (int i = 0; i < 3; i++) {
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

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m->opt_tab = i;
    }

    py += tab_h + 16;

    // ── Contenu des onglets ────────────────────────────────────
    int content_x = px + M_PAD;
    int content_y = py;
    int content_w = iw;

    switch (m->opt_tab) {
    case 0: // Général
        {
            int y = content_y;
            draw_text_boxed("Plein ecran", content_x, y, 11, C_GOLD);
            y += 16;
            const char *fsl = m->opts.fullscreen ? "[*] Activé" : "[ ] Désactivé";
            if (draw_btn(fsl, content_x, y, content_w, BTN_H, C_BLUE, m->opts.fullscreen)) {
                m->opts.fullscreen ^= 1;
                act.toggle_fs = 1;
            }
            y += BTN_H + 20;

            draw_text_boxed("FPS Cible", content_x, y, 11, C_GOLD);
            y += 16;

            static const int   FPS_OPTS[] = {30, 60, 120, 165, 0};
            static const char *FPS_LBL [] = {"30 FPS","60 FPS","120 FPS","165 FPS","Illimité"};
            char fps_display[32];
            if (m->opts.target_fps == 0)
                snprintf(fps_display, sizeof(fps_display), "Illimité");
            else
                snprintf(fps_display, sizeof(fps_display), "%d FPS", m->opts.target_fps);

            if (draw_btn(fps_display, content_x, y, content_w, BTN_H, C_BLUE, 0))
                m->opt_dropdown_open = (m->opt_dropdown_open == 0) ? -1 : 0;
            draw_dropdown_arrow(content_x, y, content_w, BTN_H, C_BLUE);
            y += BTN_H + 4;

            if (m->opt_dropdown_open == 0) {
                for (int i = 0; i < 5; i++) {
                    if (draw_btn(FPS_LBL[i], content_x, y, content_w, 26, C_TEXT,
                                 m->opts.target_fps == FPS_OPTS[i])) {
                        m->opts.target_fps = FPS_OPTS[i];
                        SetTargetFPS(FPS_OPTS[i]);
                        m->opt_dropdown_open = -1;
                    }
                    y += 30;
                }
            }

            y += 20;
            draw_text_boxed("Compteur FPS", content_x, y, 11, C_GOLD);
            y += 16;
            {
                const char *fps_lbl = m->opts.show_fps ? "[*] Activé" : "[ ] Désactivé";
                if (draw_btn(fps_lbl, content_x, y, content_w, BTN_H,
                             C_BLUE, m->opts.show_fps))
                    m->opts.show_fps ^= 1;
            }
        }
        break;

    case 1: // Audio
        {
            int y = content_y;
            draw_text_boxed("Parametres Audio", content_x, y, 11, C_GOLD);
            y += 22;

            const char *music_toggle = m->opts.music_volume > 0 ? "[*] Musique" : "[ ] Musique";
            if (draw_btn(music_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.music_volume > 0)) {
                m->opts.music_volume = (m->opts.music_volume > 0) ? 0 : 60;
                audio_set_music_volume(m->opts.music_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Musique", content_x, y, content_w, m->opts.music_volume, &m->opts.music_volume))
                audio_set_music_volume(m->opts.music_volume / 100.0f);
            y += 42;

            const char *sfx_toggle = m->opts.sfx_volume > 0 ? "[*] Effets" : "[ ] Effets";
            if (draw_btn(sfx_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.sfx_volume > 0)) {
                m->opts.sfx_volume = (m->opts.sfx_volume > 0) ? 0 : 95;
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Effets", content_x, y, content_w, m->opts.sfx_volume, &m->opts.sfx_volume))
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
            y += 42;

            const char *master_toggle = m->opts.master_volume > 0 ? "[*] Volume général" : "[ ] Volume général";
            if (draw_btn(master_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.master_volume > 0)) {
                m->opts.master_volume = (m->opts.master_volume > 0) ? 0 : 80;
                audio_set_master_volume(m->opts.master_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume général", content_x, y, content_w, m->opts.master_volume, &m->opts.master_volume))
                audio_set_master_volume(m->opts.master_volume / 100.0f);
        }
        break;

    case 2: // Graphismes
        {
            int y = content_y;
            draw_text_boxed("Resolution", content_x, y, 11, C_GOLD);
            y += 16;

            static const int RES[][2] = {
                {1120,770},{1400,962},{1680,1154},{1960,1346},{2240,1540}
            };
            static const char *RLBL[] = {
                "1120 × 770  (1×)","1400 × 962  (1.25×)","1680 × 1154 (1.5×)",
                "1960 × 1346 (1.75×)","2240 × 1540 (2×)",
            };

            char res_display[64];
            snprintf(res_display, sizeof(res_display), "%d × %d",
                     m->opts.win_width, m->opts.win_height);

            if (draw_btn(res_display, content_x, y, content_w, BTN_H, C_BLUE, 0))
                m->opt_dropdown_open = (m->opt_dropdown_open == 1) ? -1 : 1;
            draw_dropdown_arrow(content_x, y, content_w, BTN_H, C_BLUE);
            y += BTN_H + 4;

            if (m->opt_dropdown_open == 1) {
                for (int i = 0; i < 5; i++) {
                    int cur = (m->opts.win_width == RES[i][0]);
                    if (draw_btn(RLBL[i], content_x, y, content_w, 26, C_TEXT, cur)) {
                        m->opts.win_width  = RES[i][0];
                        m->opts.win_height = RES[i][1];
                        act.toggle_fs = 2;
                        m->opt_dropdown_open = -1;
                    }
                    y += 30;
                }
            }
        }
        break;
    }

    // Info résolution actuelle
    char info[48];
    snprintf(info, sizeof(info), "%dx%d", GetScreenWidth(), GetScreenHeight());
    txt_c(info, cx, cy + ph/2 - M_PAD - 26, 9, C_DIM);

    if (draw_btn("RETOUR", cx - 55, cy + ph/2 - M_PAD - BTN_H,
                 110, BTN_H, C_DIM, 0))
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;

    return act;
}

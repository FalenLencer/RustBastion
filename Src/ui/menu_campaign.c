/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_campaign.c ─ Carte de progression de campagne.
 *
 *  Contient :
 *    draw_world_map  — Sélection acte + lancement direct (layout deux colonnes)
 */

#include "menu_internal.h"

// ════════════════════════════════════════════════════
// CARTE DU MONDE / LANCEMENT CAMPAGNE
// ════════════════════════════════════════════════════
MenuAction draw_world_map(MenuState *m, const MetaProgress *meta,
                          int vw, int vh)
{
    MenuAction act = {0};

    draw_bg(m, vw, vh);
    draw_header("CAMPAGNE", vw);
    txt_c_boxed("Cliquez sur un acte ou un chapitre pour le selectionner.",
                vw/2, M_PAD + 86, 10, C_TEXT);

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

    // ── Layout deux colonnes ──────────────────────────────────────
    int content_y = M_PAD + 110;
    int left_x    = M_PAD * 2;
    int left_w    = (vw * 58) / 100;
    int right_x   = left_x + left_w + M_IN * 2;
    int right_w   = vw - right_x - M_PAD * 2;

    // ═══════════════════════════════════════════════
    // COLONNE GAUCHE — Chapitres et actes
    // ═══════════════════════════════════════════════
    int chapter_h = 90;   /* + place pour la ligne "voies alternatives" */
    int chapter_y = content_y;
    int gap       = M_IN;
    int sel_ch    = (m->selected_campaign_act >= 0)
                  ? m->selected_campaign_act / CAMPAIGN_ACTS : -1;

    for (int ch = 0; ch < CAMPAIGN_CHAPTERS; ch++) {
        Color col     = CHAPTER_COLS[ch];
        int   ch_sel  = (ch == sel_ch);

        Rectangle cr = {(float)left_x, (float)chapter_y,
                        (float)left_w, (float)chapter_h};
        int ch_unlocked = meta_act_unlocked(meta, ch * CAMPAIGN_ACTS);
        int hov_ch      = ch_unlocked && vhov_r(cr);

        // ── Fond du chapitre ─────────────────────────────────────
        Color bg = ch_unlocked
            ? ((hov_ch || ch_sel)
                ? (Color){col.r/5, col.g/5, col.b/5, 255}
                : (Color){col.r/8, col.g/8, col.b/8, 255})
            : (Color){10, 8, 5, 255};
        DrawRectangleRounded(cr, 4.0f/chapter_h, 5, bg);
        DrawRectangleRoundedLinesEx(cr, 4.0f/chapter_h, 5,
            (hov_ch || ch_sel) ? 2.5f : 1.5f,
            ch_unlocked ? ((hov_ch || ch_sel) ? col
                                              : (Color){col.r/3, col.g/3, col.b/3, 255})
                        : (Color){40,30,12,255});

        // ── En-tête chapitre ─────────────────────────────────────
        dtxt(TextFormat("CH.%d", ch+1),
             left_x + M_IN, chapter_y + M_IN, 10, C_DIM);
        dtxt(CHAPTER_NAMES[ch],
             left_x + M_IN + 34, chapter_y + M_IN, 14,
             ch_unlocked ? col : C_DIM);

        // ── Boîtes d'acte ────────────────────────────────────────
        int act_w = (left_w - M_IN * 4) / CAMPAIGN_ACTS;
        int act_clicked = 0;
        for (int a = 0; a < CAMPAIGN_ACTS; a++) {
            int stage_idx = ch * CAMPAIGN_ACTS + a;
            const ActData *ad = campaign_act_get(stage_idx);
            int stars    = meta->act_stars[stage_idx];
            int unlocked = meta_act_unlocked(meta, stage_idx);
            int is_sel   = (m->selected_campaign_act == stage_idx);

            int ax = left_x + M_IN + a * (act_w + M_IN);
            int ay = chapter_y + 28;
            int ah = 40;   /* hauteur fixe ; la ligne "voies alt." passe dessous */

            Rectangle ar = {(float)ax, (float)ay, (float)act_w, (float)ah};
            int hov_act = unlocked && vhov_r(ar);

            // Fond : sélectionné > survol > étoiles > vide > verrouillé
            Color abg = unlocked
                ? (is_sel    ? (Color){col.r/3, col.g/3, col.b/3, 255}
                 : hov_act   ? (Color){col.r/4, col.g/4, col.b/4, 255}
                 : stars > 0 ? (Color){col.r/6, col.g/6, col.b/6, 255}
                             : (Color){18,12,4,255})
                : (Color){8,6,3,255};
            DrawRectangleRounded(ar, 3.0f/ah, 4, abg);
            DrawRectangleRoundedLinesEx(ar, 3.0f/ah, 4,
                is_sel ? 2.5f : hov_act ? 2.0f : 1.0f,
                unlocked ? (is_sel  ? col
                          : hov_act ? col
                          : stars > 0 ? col : (Color){60,45,18,255})
                         : (Color){30,24,10,255});

            char abuf[32];
            clip_text(ad->title, act_w - 6, 9, abuf, sizeof(abuf));
            dtxt(abuf, ax+3, ay+2, 9,
                 unlocked ? (stars > 0 ? col : C_TEXT) : C_DIM);

            // Étoiles
            for (int s = 0; s < 2; s++) {
                Color sc = (s < stars) ? (Color){232,200,32,255}
                                       : (Color){40,32,12,255};
                dtxt("*", ax + 3 + s*12, ay + ah - 14, 12, sc);
            }

            // Badges de graphe : CHOIX (bifurcation) / REPLI (repli à la défaite)
            if (unlocked) {
                if (campaign_has_choice(stage_idx)) {
                    int bw = mtxt("CHOIX", 7);
                    dtxt("CHOIX", ax + act_w - bw - 4, ay + 3, 7,
                         (Color){232, 200, 120, 230});
                } else if (campaign_defeat_mode(stage_idx) == DEFEAT_RETREAT) {
                    int bw = mtxt("REPLI", 7);
                    dtxt("REPLI", ax + act_w - bw - 4, ay + 3, 7,
                         (Color){120, 200, 140, 230});
                }
            }

            if (!unlocked)
                dtxt("---", ax + act_w/2 - 8, ay + ah/2 - 5, 10, C_DIM);

            // Clic : sélectionne l'acte (prioritaire sur le chapitre)
            if (unlocked && vclick_r(ar)) {
                m->selected_campaign_act = stage_idx;
                act_clicked = 1;
            }
        }

        // ── Voies alternatives du chapitre (nœuds de branche / repli) ──
        // Listées en clair pour montrer que le chapitre recèle d'autres
        // routes que la trame principale.
        {
            int axp = left_x + M_IN;
            int ayp = chapter_y + 28 + 40 + 4;   // sous les boîtes d'acte
            int shown = 0;
            for (int bn = CAMPAIGN_TOTAL; bn < CAMPAIGN_NODES; bn++) {
                const ActData *bd = campaign_act_get(bn);
                if (!bd->title || !bd->title[0] || bd->chapter != ch) continue;
                if (shown == 0) {
                    dtxt("Voies alt.:", axp, ayp, 8, (Color){100, 85, 60, 220});
                    axp += mtxt("Voies alt.: ", 8);
                }
                char t[40];
                clip_text(bd->title, 150, 8, t, sizeof(t));
                dtxt(t, axp, ayp, 8,
                     ch_unlocked ? (Color){col.r, col.g, col.b, 210}
                                 : (Color){70, 58, 36, 200});
                axp += mtxt(t, 8) + 10;
                shown++;
            }
        }

        // Clic chapitre uniquement si aucun acte n'a capté le clic
        if (!act_clicked && ch_unlocked && vclick_r(cr)) {
            m->selected_campaign_act = ch * CAMPAIGN_ACTS;
        }

        chapter_y += chapter_h + gap;
    }

    // ═══════════════════════════════════════════════
    // COLONNE DROITE — Sélection + LANCER + Sauvegardes
    // ═══════════════════════════════════════════════
    int ry     = content_y;
    int has_sel = (m->selected_campaign_act >= 0);

    // ── Panneau de l'acte sélectionné ────────────────────────────
    int info_h = 112;
    {
        Rectangle info_r = {(float)right_x, (float)ry,
                            (float)right_w, (float)info_h};
        Color info_col = has_sel
            ? CHAPTER_COLS[m->selected_campaign_act / CAMPAIGN_ACTS]
            : C_BORDER;
        DrawRectangleRounded(info_r, (float)PANEL_R/info_h, 5, C_PANEL);
        DrawRectangleRoundedLinesEx(info_r, (float)PANEL_R/info_h, 5,
                                    has_sel ? 1.8f : 1.2f, info_col);

        if (has_sel) {
            int sel    = m->selected_campaign_act;
            int sch    = sel / CAMPAIGN_ACTS;
            Color col  = CHAPTER_COLS[sch];
            const ActData *sel_ad = campaign_act_get(sel);
            int px = right_x + M_IN + 2;
            int py = ry + M_IN;

            // Chapitre
            char chbuf[64];
            snprintf(chbuf, sizeof(chbuf), "Ch.%d — %s", sch+1, CHAPTER_NAMES[sch]);
            char chclip[64];
            clip_text(chbuf, right_w - M_IN*2, 9, chclip, sizeof(chclip));
            dtxt(chclip, px, py, 9,
                 (Color){(unsigned char)(col.r/2+20), (unsigned char)(col.g/2+20),
                         (unsigned char)(col.b/2+20), 200});
            py += fh(9) + 4;

            // Numéro d'acte
            dtxt(TextFormat("Acte %d", sel + 1), px, py, 10, C_DIM);
            py += fh(10) + 2;

            if (sel_ad) {
                // Titre
                char tclip[64];
                clip_text(sel_ad->title, right_w - M_IN*2, 15, tclip, sizeof(tclip));
                dtxt(tclip, px, py, 15, col);
                py += fh(15) + 4;
                // Sous-titre
                char sclip[64];
                clip_text(sel_ad->subtitle, right_w - M_IN*2, 9, sclip, sizeof(sclip));
                dtxt(sclip, px, py, 9, C_DIM);
            }

            // Aperçu du routage de graphe pour cet acte
            {
                const char *rt =
                    campaign_has_choice(sel)    ? "Bifurcation : choix en fin d'acte"
                  : (sel >= CAMPAIGN_TOTAL - 1) ? "Defaite ici : fin de campagne"
                  : (campaign_defeat_mode(sel) == DEFEAT_RETREAT)
                                                ? "Defaite ici : repli vers une autre voie"
                                                : "Defaite ici : reprise affaiblie";
                char rclip[72];
                clip_text(rt, right_w - M_IN*2, 8, rclip, sizeof(rclip));
                dtxt(rclip, right_x + M_IN + 2,
                     ry + info_h - M_IN - fh(9) - 14, 8,
                     (Color){150, 130, 95, 220});
            }

            // Étoiles éventuelles
            int stars_earned = meta->act_stars[sel];
            if (stars_earned > 0) {
                char sbuf[16];
                snprintf(sbuf, sizeof(sbuf), "%d *", stars_earned);
                dtxt(sbuf, right_x + M_IN + 2,
                     ry + info_h - M_IN - fh(9), 9,
                     (Color){232,200,32,255});
            }
        } else {
            txt_c("Selectionnez un acte ou un chapitre",
                  right_x + right_w/2,
                  ry + info_h/2 - fh(10)/2, 10, C_DIM);
        }
    }
    ry += info_h + M_IN;

    // ── Bouton LANCER ─────────────────────────────────────────────
    {
        // Premier emplacement vide
        int first_empty = -1;
        for (int s = 0; s < SAVE_SLOT_COUNT; s++) {
            if (!m->campaign_slots[s].exists) { first_empty = s; break; }
        }
        int can_launch = (has_sel && first_empty >= 0);

        if (draw_btn("LANCER", right_x, ry, right_w, BTN_H + 4,
                     can_launch ? C_GREEN : C_DIM, 0) && can_launch) {
            act.start_campaign      = 1;
            act.new_slot            = first_empty;
            act.campaign_order_seed = 0;
            act.start_campaign_act  = m->selected_campaign_act;
        }

        ry += BTN_H + 4 + 2;

        // Message si tous les emplacements sont occupés
        if (has_sel && !can_launch) {
            txt_c("Effacez une partie ci-dessous",
                  right_x + right_w/2, ry, 9, C_RED);
            ry += fh(9) + 4;
        } else {
            ry += M_IN;
        }
    }

    // ── Section sauvegardes ───────────────────────────────────────
    draw_sep(right_x, ry, right_w, C_BORDER);
    ry += M_IN;
    dtxt("PARTIES EN COURS", right_x, ry, 9, C_DIM);
    ry += fh(9) + M_IN;

    int slot_h   = 52;
    int slot_gap = 4;
    for (int s = 0; s < SAVE_SLOT_COUNT; s++) {
        const SaveInfo *si = &m->campaign_slots[s];
        Rectangle sr = {(float)right_x, (float)ry, (float)right_w, (float)slot_h};
        int hov_s  = si->exists && vhov_r(sr);
        Color sbrd = si->exists ? C_GOLD : C_BORDER;
        Color sbg  = hov_s ? C_HOV : C_PANEL;

        DrawRectangleRounded(sr, (float)PANEL_R/slot_h, 5, sbg);
        DrawRectangleRoundedLinesEx(sr, (float)PANEL_R/slot_h, 5,
                                    hov_s ? 2.0f : 1.2f, sbrd);

        if (si->exists) {
            int bw_rep2 = 82, bw_del2 = 54, btn_gap2 = 4;
            int bx_del2 = right_x + right_w - M_IN - bw_del2;
            int bx_rep2 = bx_del2 - btn_gap2 - bw_rep2;
            int bh2     = 22;
            int by2     = ry + slot_h/2 - bh2/2;
            int txt_avail = bx_rep2 - (right_x + M_IN) - M_IN;

            const ActData *sad = campaign_act_get(si->campaign_stage);
            char hdr[64];
            snprintf(hdr, sizeof(hdr), "Partie %d  —  Ch.%d Acte %d",
                     si->campaign_num+1, sad->chapter+1, sad->act+1);
            char hclip[64];
            clip_text(hdr, txt_avail, 9, hclip, sizeof(hclip));
            dtxt(hclip, right_x + M_IN, ry + M_IN, 9, C_GOLD);

            if (sad) {
                char tclip[48];
                clip_text(sad->title, txt_avail, 9, tclip, sizeof(tclip));
                dtxt(tclip, right_x + M_IN, ry + M_IN + fh(9) + 2, 9, C_DIM);
            }

            if (draw_btn("REPRENDRE", bx_rep2, by2, bw_rep2, bh2, C_GREEN, 0)) {
                act.resume_slot        = s;
                act.resume_is_campaign = 1;
                act.go_game            = 1;
            }
            if (draw_btn("EFFACER", bx_del2, by2, bw_del2, bh2, C_RED, 0)) {
                m->confirm_del_slot = s;
                m->back_screen      = MENU_WORLD_MAP;
                m->screen           = MENU_CONFIRM_DEL;
            }
        } else {
            txt_c(TextFormat("Emplacement %d — vide", s+1),
                  right_x + right_w/2, ry + slot_h/2 - fh(9)/2, 9, C_DIM);
        }
        ry += slot_h + slot_gap;
    }

    // ── Bouton retour ─────────────────────────────────────────────
    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

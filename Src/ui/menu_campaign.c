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
#include "ui_anim.h"

// ── Animation de la carte du monde (PROMPT M) ────────────────────
#define SLOT_WMAP        5       // slot ui_timer (0-4 : titre/pause/hub/…)
#define WM_CH_DUR        0.25f   // entrée d'une carte de chapitre (s)
#define WM_CH_STEP       0.06f   // délai entre chapitres (s)
#define WM_CH_SLIDE      14.0f   // glissement vertical (px)
#define WM_STAR_T0       0.25f   // début de la chute des étoiles (s)
#define WM_STAR_STEP     0.03f   // délai par acte (s)
#define WM_STAR_DUR      0.20f   // durée de la chute d'une étoile (s)
#define WM_INFO_DUR      0.15f   // crossfade du panneau d'info (s)
#define WM_INFO_SLIDE    8.0f    // glissement du contenu info (px)
#define WM_PULSE_DUR     0.8f    // pulse d'attention du bouton LANCER (s)
#define WM_CHEV_SPEED    18.0f   // vitesse de défilement des chevrons (px/s)
#define WM_CHEV_GAP      12.0f   // espacement des chevrons (px)

// Fondu d'une couleur par un facteur k (alpha multiplié).
static Color wm_fade(Color c, float k) {
    c.a = (unsigned char)((float)c.a * k);
    return c;
}

// ── Crossfade du panneau d'info droit : offset + alpha partagés ──
static int   g_wm_dx = 0;
static float g_wm_al = 1.0f;
static void wm_txt(const char *s, int x, int y, int fs, Color c) {
    c.a = (unsigned char)((float)c.a * g_wm_al);
    dtxt(s, x + g_wm_dx, y, fs, c);
}

// ════════════════════════════════════════════════════
// CARTE DU MONDE / LANCEMENT CAMPAGNE
// ════════════════════════════════════════════════════
MenuAction draw_world_map(MenuState *m, const MetaProgress *meta,
                          int vw, int vh)
{
    MenuAction act = {0};

    /* Détection d'arrivée (cf. écran titre : trans_t recule = entrée) */
    {
        static float prev_trans = 1e9f;
        if (m->trans_t < prev_trans) ui_timer(SLOT_WMAP, 1);
        prev_trans = m->trans_t;
    }
    float t_open = ui_timer(SLOT_WMAP, 0);

    /* Horloge continue (respiration sélection, chevrons, LANCER) */
    static float wm_t = 0.0f;
    wm_t += ui_dt();

    draw_bg(m, vw, vh);
    draw_header("CAMPAGNE", vw);
    txt_c_boxed("Cliquez sur un acte ou un chapitre pour le selectionner.",
                vw/2, M_PAD + 86, 10, C_TEXT);

    const Color *CHAPTER_COLS = CHAPTER_COLORS;   // source unique (menu.c)

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

        /* ── Cascade d'ouverture : chapitre ch entre avec ch×0.06 s de
           retard. Hitbox FINALES dès la frame 1 ; seuls le dessin (offset
           dy) et l'alpha (ka) sont animés ; clics ignorés tant que k<1. */
        float k_ch = (t_open - (float)ch * WM_CH_STEP) / WM_CH_DUR;
        k_ch = k_ch < 0.0f ? 0.0f : k_ch > 1.0f ? 1.0f : k_ch;
        if (k_ch <= 0.0f) { chapter_y += chapter_h + gap; continue; }
        float ka = k_ch;
        int   dy = (int)(WM_CH_SLIDE * (1.0f - ea_out_cubic(k_ch)));
        int   ready = (k_ch >= 1.0f);   // clics autorisés

        Rectangle cr = {(float)left_x, (float)chapter_y,
                        (float)left_w, (float)chapter_h};          // hitbox
        Rectangle cr_draw = {(float)left_x, (float)(chapter_y + dy),
                             (float)left_w, (float)chapter_h};     // dessin
        int ch_unlocked = meta_act_unlocked(meta, ch * CAMPAIGN_ACTS);
        int hov_ch      = ch_unlocked && vhov_r(cr);

        // ── Fond du chapitre ─────────────────────────────────────
        Color bg = ch_unlocked
            ? ((hov_ch || ch_sel)
                ? (Color){col.r/5, col.g/5, col.b/5, 255}
                : (Color){col.r/8, col.g/8, col.b/8, 255})
            : (Color){10, 8, 5, 255};
        DrawRectangleRounded(cr_draw, 4.0f/chapter_h, 5, wm_fade(bg, ka));
        DrawRectangleRoundedLinesEx(cr_draw, 4.0f/chapter_h, 5,
            (hov_ch || ch_sel) ? 2.5f : 1.5f,
            wm_fade(ch_unlocked
                        ? ((hov_ch || ch_sel) ? col
                                              : (Color){col.r/3, col.g/3, col.b/3, 255})
                        : (Color){40,30,12,255}, ka));

        // ── En-tête chapitre ─────────────────────────────────────
        dtxt(TextFormat("CH.%d", ch+1),
             left_x + M_IN, chapter_y + dy + M_IN, 10, wm_fade(C_DIM, ka));
        dtxt(CHAPTER_NAMES[ch],
             left_x + M_IN + 34, chapter_y + dy + M_IN, 14,
             wm_fade(ch_unlocked ? col : C_DIM, ka));

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
            Rectangle ar_draw = {(float)ax, (float)(ay + dy),
                                 (float)act_w, (float)ah};
            int hov_act = unlocked && vhov_r(ar);

            // Fond : sélectionné > survol > étoiles > vide > verrouillé
            Color abg = unlocked
                ? (is_sel    ? (Color){col.r/3, col.g/3, col.b/3, 255}
                 : hov_act   ? (Color){col.r/4, col.g/4, col.b/4, 255}
                 : stars > 0 ? (Color){col.r/6, col.g/6, col.b/6, 255}
                             : (Color){18,12,4,255})
                : (Color){8,6,3,255};
            DrawRectangleRounded(ar_draw, 3.0f/ah, 4, wm_fade(abg, ka));
            DrawRectangleRoundedLinesEx(ar_draw, 3.0f/ah, 4,
                is_sel ? 2.0f : hov_act ? 2.0f : 1.0f,
                wm_fade(unlocked ? (is_sel  ? col
                          : hov_act ? col
                          : stars > 0 ? col : (Color){60,45,18,255})
                         : (Color){30,24,10,255}, ka));

            /* Acte sélectionné : anneau externe qui respire */
            if (is_sel) {
                Rectangle sr = {ar_draw.x - 2, ar_draw.y - 2,
                                ar_draw.width + 4, ar_draw.height + 4};
                unsigned char sa =
                    (unsigned char)(120.0f + 80.0f * sinf(wm_t * 2.8f));
                DrawRectangleRoundedLinesEx(sr, 3.0f/ah, 4, 2.0f,
                    wm_fade((Color){col.r, col.g, col.b, sa}, ka));
            }

            // Badge de graphe (haut-droite) : CHOIX (bifurcation) / REPLI
            // (repli à la défaite). Déterminé d'abord pour réserver sa place
            // et éviter que le titre ne le chevauche.
            const char *badge     = NULL;
            Color       badge_col = (Color){0};
            if (unlocked) {
                if (campaign_has_choice(stage_idx)) {
                    badge = "CHOIX"; badge_col = (Color){232, 200, 120, 230};
                } else if (campaign_defeat_mode(stage_idx) == DEFEAT_RETREAT) {
                    badge = "REPLI"; badge_col = (Color){120, 200, 140, 230};
                }
            }
            int badge_w = badge ? mtxt(badge, 7) + 6 : 0;   // +marge

            // Titre (clippé pour laisser la place au badge à droite)
            int title_w = act_w - 6 - badge_w;
            if (title_w < 16) title_w = 16;
            char abuf[32];
            clip_text(ad->title, title_w, 9, abuf, sizeof(abuf));
            dtxt(abuf, ax+3, ay + dy + 2, 9,
                 wm_fade(unlocked ? (stars > 0 ? col : C_TEXT) : C_DIM, ka));

            // Étoiles — les gagnées « tombent » à l'ouverture de l'écran
            for (int s = 0; s < 2; s++) {
                Color sc = (s < stars) ? (Color){232,200,32,255}
                                       : (Color){40,32,12,255};
                int fs = 12;
                if (s < stars) {
                    float ks = (t_open - WM_STAR_T0
                                - (float)stage_idx * WM_STAR_STEP) / WM_STAR_DUR;
                    if (ks <= 0.0f) continue;            // pas encore tombée
                    if (ks < 1.0f)                        // scale 1.8 → 1.0
                        fs = (int)(12.0f * (1.8f - 0.8f * ea_out_back(ks)));
                }
                dtxt("*", ax + 3 + s*12, ay + dy + ah - 14, fs,
                     wm_fade(sc, ka));
            }

            if (badge) {
                int bw = mtxt(badge, 7);
                dtxt(badge, ax + act_w - bw - 4, ay + dy + 3, 7,
                     wm_fade(badge_col, ka));
            }

            if (!unlocked)
                dtxt("---", ax + act_w/2 - 8, ay + dy + ah/2 - 5, 10,
                     wm_fade(C_DIM, ka));

            // Clic : sélectionne l'acte (prioritaire sur le chapitre)
            if (ready && unlocked && vclick_r(ar)) {
                m->selected_campaign_act = stage_idx;
                act_clicked = 1;
            }
        }

        // ── Connecteurs entre actes : la progression se lit d'un trait ──
        if (ch_unlocked) {
            int ah2  = 40;
            int ymid = chapter_y + dy + 28 + ah2/2;
            for (int a = 0; a < CAMPAIGN_ACTS - 1; a++) {
                int st  = ch * CAMPAIGN_ACTS + a;
                int x0  = left_x + M_IN + a * (act_w + M_IN) + act_w;
                int x1  = x0 + M_IN;
                DrawLineEx((Vector2){(float)x0, (float)ymid},
                           (Vector2){(float)x1, (float)ymid}, 2.0f,
                           wm_fade((Color){col.r, col.g, col.b, 90}, ka));
                /* Chevrons défilants UNIQUEMENT sur le front de progression :
                   dernier acte réussi → premier acte non fait. */
                if (meta->act_stars[st] > 0 && meta->act_stars[st+1] == 0 &&
                    meta_act_unlocked(meta, st+1)) {
                    float off = fmodf(wm_t * WM_CHEV_SPEED, WM_CHEV_GAP);
                    for (int j = -1; j <= 1; j++) {
                        float cxx = (float)x0 + off + (float)j * WM_CHEV_GAP;
                        if (cxx < (float)x0 - 2.0f || cxx > (float)x1 - 3.0f)
                            continue;
                        dtxt(">", (int)cxx, ymid - fh(8)/2, 8,
                             wm_fade((Color){col.r, col.g, col.b, 230}, ka));
                    }
                }
            }
        }

        // ── Voies alternatives du chapitre (nœuds de branche / repli) ──
        // Listées en clair pour montrer que le chapitre recèle d'autres
        // routes que la trame principale.
        {
            int axp = left_x + M_IN;
            int ayp = chapter_y + dy + 28 + 40 + 4;   // sous les boîtes d'acte
            int shown = 0;
            for (int bn = CAMPAIGN_TOTAL; bn < CAMPAIGN_NODES; bn++) {
                const ActData *bd = campaign_act_get(bn);
                if (!bd->title || !bd->title[0] || bd->chapter != ch) continue;
                if (shown == 0) {
                    dtxt("Voies alt.:", axp, ayp, 8,
                         wm_fade((Color){100, 85, 60, 220}, ka));
                    axp += mtxt("Voies alt.: ", 8);
                }
                char t[40];
                clip_text(bd->title, 150, 8, t, sizeof(t));
                dtxt(t, axp, ayp, 8,
                     wm_fade(ch_unlocked ? (Color){col.r, col.g, col.b, 210}
                                         : (Color){70, 58, 36, 200}, ka));
                axp += mtxt(t, 8) + 10;
                shown++;
            }
        }

        // Clic chapitre uniquement si aucun acte n'a capté le clic
        if (ready && !act_clicked && ch_unlocked && vclick_r(cr)) {
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
    // Crossfade du contenu quand la sélection change (offset x + alpha).
    {
        static int   prev_sel_act = -999;
        static float info_t       = 1e9f;
        if (m->selected_campaign_act != prev_sel_act) {
            prev_sel_act = m->selected_campaign_act;
            info_t = 0.0f;
        }
        info_t += ui_dt();
        float ik = info_t / WM_INFO_DUR;
        if (ik > 1.0f) ik = 1.0f;
        g_wm_dx = (int)(WM_INFO_SLIDE * (1.0f - ea_out_cubic(ik)));
        g_wm_al = ik;
    }

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
            int px    = right_x + M_IN + 2;
            int py    = ry + M_IN;
            int avail = right_w - M_IN*2 - 2;   // largeur de texte dispo

            // Tout s'enchaîne en flux vertical : pas d'ancrage en bas
            // (sinon le bas chevauche le texte qui descend du haut).

            // Chapitre
            char chbuf[64];
            snprintf(chbuf, sizeof(chbuf), "Ch.%d — %s", sch+1, CHAPTER_NAMES[sch]);
            char chclip[64];
            clip_text(chbuf, avail, 9, chclip, sizeof(chclip));
            wm_txt(chclip, px, py, 9,
                 (Color){(unsigned char)(col.r/2+20), (unsigned char)(col.g/2+20),
                         (unsigned char)(col.b/2+20), 200});
            py += fh(9) + 2;

            // Numéro d'acte (gauche) + étoiles gagnées (droite, même ligne)
            wm_txt(TextFormat("Acte %d", sel + 1), px, py, 10, C_DIM);
            int stars_earned = meta->act_stars[sel];
            if (stars_earned > 0) {
                char sbuf[16];
                snprintf(sbuf, sizeof(sbuf), "%d *", stars_earned);
                int sw = mtxt(sbuf, 10);
                wm_txt(sbuf, right_x + right_w - M_IN - sw, py, 10,
                     (Color){232, 200, 32, 255});
            }
            py += fh(10) + 2;

            if (sel_ad) {
                // Titre
                char tclip[64];
                clip_text(sel_ad->title, avail, 15, tclip, sizeof(tclip));
                wm_txt(tclip, px, py, 15, col);
                py += fh(15) + 2;
                // Sous-titre
                char sclip[64];
                clip_text(sel_ad->subtitle, avail, 9, sclip, sizeof(sclip));
                wm_txt(sclip, px, py, 9, C_DIM);
                py += fh(9) + 3;
            }

            // Aperçu du routage de graphe (sous le sous-titre)
            {
                const char *rt =
                    campaign_has_choice(sel)    ? "Bifurcation : choix en fin d'acte"
                  : (sel >= CAMPAIGN_TOTAL - 1) ? "Defaite ici : fin de campagne"
                  : (campaign_defeat_mode(sel) == DEFEAT_RETREAT)
                                                ? "Defaite ici : repli vers une autre voie"
                                                : "Defaite ici : reprise affaiblie";
                char rclip[72];
                clip_text(rt, avail, 8, rclip, sizeof(rclip));
                wm_txt(rclip, px, py, 8, (Color){150, 130, 95, 220});
            }
        } else {
            txt_c("Selectionnez un acte ou un chapitre",
                  right_x + right_w/2,
                  ry + info_h/2 - fh(10)/2, 10, C_DIM);
        }
    }
    /* Fin du panneau d'info : helpers redevenus neutres */
    g_wm_dx = 0;
    g_wm_al = 1.0f;
    ry += info_h + M_IN;

    // ── Bouton LANCER ─────────────────────────────────────────────
    {
        // Premier emplacement vide
        int first_empty = -1;
        for (int s = 0; s < SAVE_SLOT_COUNT; s++) {
            if (!m->campaign_slots[s].exists) { first_empty = s; break; }
        }
        int can_launch = (has_sel && first_empty >= 0);

        /* Pulse d'attention quand le lancement devient possible,
           puis respiration lente continue (invite à cliquer). */
        static int   prev_can = 0;
        static float pulse_t  = 1e9f;
        if (can_launch && !prev_can) pulse_t = 0.0f;
        prev_can = can_launch;
        pulse_t += ui_dt();

        int launch_y = ry;
        if (draw_btn("LANCER", right_x, ry, right_w, BTN_H + 4,
                     can_launch ? C_GREEN : C_DIM, 0) && can_launch) {
            act.start_campaign      = 1;
            act.new_slot            = first_empty;
            act.campaign_order_seed = 0;
            act.start_campaign_act  = m->selected_campaign_act;
        }
        if (can_launch) {
            unsigned char la;
            if (pulse_t < WM_PULSE_DUR)   /* 2 battements en 0.8 s */
                la = (unsigned char)(fabsf(sinf(pulse_t * PI
                                                / (WM_PULSE_DUR * 0.5f))) * 200.0f);
            else                          /* respiration ±25, période 2 s */
                la = (unsigned char)(25.0f + 25.0f * sinf(wm_t * PI));
            Rectangle lr = {(float)(right_x - 2), (float)(launch_y - 2),
                            (float)(right_w + 4), (float)(BTN_H + 4 + 4)};
            DrawRectangleRoundedLinesEx(lr, (float)BTN_R/(BTN_H+4), 6, 2.0f,
                (Color){C_GREEN.r, C_GREEN.g, C_GREEN.b, la});
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

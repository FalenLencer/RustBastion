/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_campaign_info.c ─ Panneau « acte sélectionné » de la carte du monde.
 *
 *  Extrait de menu_campaign.c (règle projet : fichiers < 500 lignes).
 *  L'acte choisi s'affiche comme au briefing : sa vignette pixel-art (le
 *  lieu clé du récit), en médaillon le portrait de l'adversaire du
 *  chapitre, puis la fiche de route (titre, étoiles, suite d'une défaite).
 */
#include "menu_internal.h"
#include "ui_anim.h"
#include "story_art.h"

#define WM_INFO_DUR    0.15f   // crossfade du panneau d'info (s)
#define WM_INFO_SLIDE  8.0f    // glissement du contenu info (px)
#define WM_VIG_PX      2       // vignette à demi-échelle du briefing (≈ même grille)
#define WM_VIG_INSET   3       // retrait de la vignette dans le cadre arrondi (px)
#define WM_FOE_PX      2       // médaillon : 24 cellules × 2 px = 48 px
#define WM_FOE_MARGIN  6       // marge du médaillon dans la vignette (px)

/* Adversaire de chaque chapitre ; -1 = pas de visage (les pillards et la
   Reine des marais sont des hordes, pas des interlocuteurs). */
static const int CHAPTER_FOE[CAMPAIGN_CHAPTERS] = {
    -1, -1, SPEAKER_GENERAL, SPEAKER_STEEL, SPEAKER_NEXUS,
};

// ── Crossfade du contenu : offset + alpha partagés ───────────────
static int   g_dx = 0;
static float g_al = 1.0f;
static void info_txt(const char *s, int x, int y, int fs, Color c) {
    c.a = (unsigned char)((float)c.a * g_al);
    dtxt(s, x + g_dx, y, fs, c);
}
static Color info_fade(Color c) {
    c.a = (unsigned char)((float)c.a * g_al);
    return c;
}

/* Bandeau illustré : vignette du lieu + médaillon de l'adversaire. */
static void info_vignette(int sel, const ActData *ad, int x, int y, int w, float t) {
    int vx = x + WM_VIG_INSET, vy = y + WM_VIG_INSET;
    int vw = w - WM_VIG_INSET * 2, vh = WM_INFO_VIG_H - WM_VIG_INSET;
    story_vignette_draw(ad->theme, sel, vx, vy, vw, vh, WM_VIG_PX, t);
    /* La vignette n'a pas d'alpha : un voile couleur panneau se lève. */
    if (g_al < 1.0f) {
        Color veil = C_PANEL;
        veil.a = (unsigned char)(255.0f * (1.0f - g_al));
        DrawRectangle(vx, vy, vw, vh, veil);
    }
    Color col = CHAPTER_COLORS[sel / CAMPAIGN_ACTS];
    DrawRectangle(vx, vy + vh - 1, vw, 2, info_fade((Color){col.r, col.g, col.b, 180}));

    int foe = CHAPTER_FOE[sel / CAMPAIGN_ACTS];
    if (foe < 0) return;
    int ps = STORY_PORTRAIT_CELLS * WM_FOE_PX;
    int px = vx + vw - ps - WM_FOE_MARGIN, py = vy + vh - ps - WM_FOE_MARGIN;
    Rectangle fr = {(float)(px - 2), (float)(py - 2), (float)(ps + 4), (float)(ps + 4)};
    DrawRectangleRec(fr, info_fade((Color){0, 0, 0, 200}));
    story_portrait_draw((SpeakerId)foe, px, py, WM_FOE_PX, t, 0, g_al);
    DrawRectangleLinesEx(fr, 1.5f, info_fade(story_speaker_color((SpeakerId)foe)));
}

/* Fiche de route sous la vignette (chapitre, acte, étoiles, titre, défaite). */
static void info_text(const MetaProgress *meta, int sel, const ActData *ad,
                      int x, int y, int w) {
    int sch   = sel / CAMPAIGN_ACTS;
    Color col = CHAPTER_COLORS[sch];
    int px    = x + M_IN + 2;
    int py    = y + M_IN;
    int avail = w - M_IN*2 - 2;   // largeur de texte dispo

    // Flux vertical : pas d'ancrage en bas (sinon chevauchement).
    char chbuf[64], chclip[64];
    snprintf(chbuf, sizeof(chbuf), "Ch.%d — %s", sch + 1, CHAPTER_NAMES[sch]);
    clip_text(chbuf, avail, 10, chclip, sizeof(chclip));
    info_txt(chclip, px, py, 10,
             (Color){(unsigned char)(col.r/2 + 20), (unsigned char)(col.g/2 + 20),
                     (unsigned char)(col.b/2 + 20), 200});
    py += fh(10) + 2;

    // Numéro d'acte (gauche) + étoiles gagnées (droite, même ligne)
    info_txt(TextFormat("Acte %d", sel + 1), px, py, 10, C_DIM);
    for (int s = 0; s < meta->act_stars[sel]; s++) {
        Color stc = {232, 200, 32, (unsigned char)(255.0f * g_al)};
        ui_draw_star((float)(x + w - M_IN - 6 - s * 14 + g_dx),
                     (float)(py + fh(10) / 2), 6.0f, stc);
    }
    py += fh(10) + 2;

    if (ad) {
        char tclip[64], sclip[64];
        clip_text(ad->title, avail, 15, tclip, sizeof(tclip));
        info_txt(tclip, px, py, 15, col);
        py += fh(15) + 2;
        clip_text(ad->subtitle, avail, 10, sclip, sizeof(sclip));
        info_txt(sclip, px, py, 9, C_DIM);
        py += fh(10) + 3;
    }

    // Aperçu du routage de graphe
    const char *rt =
        campaign_has_choice(sel)    ? "Bifurcation : choix en fin d'acte"
      : (sel >= CAMPAIGN_TOTAL - 1) ? "Défaite ici : fin de campagne"
      : (campaign_defeat_mode(sel) == DEFEAT_RETREAT)
                                    ? "Défaite ici : repli vers une autre voie"
                                    : "Défaite ici : reprise affaiblie";
    char rclip[72];
    clip_text(rt, avail, 8, rclip, sizeof(rclip));
    info_txt(rclip, px, py, 8, (Color){150, 130, 95, 220});
}

void wm_draw_act_info(const MenuState *m, const MetaProgress *meta,
                      int x, int y, int w, float t)
{
    /* Crossfade du contenu quand la sélection change (offset x + alpha). */
    static int   prev_sel_act = -999;
    static float info_t       = 1e9f;
    if (m->selected_campaign_act != prev_sel_act) {
        prev_sel_act = m->selected_campaign_act;
        info_t = 0.0f;
    }
    info_t += ui_dt();
    float ik = info_t / WM_INFO_DUR;
    if (ik > 1.0f) ik = 1.0f;
    g_dx = (int)(WM_INFO_SLIDE * (1.0f - ea_out_cubic(ik)));
    g_al = ik;

    int sel = m->selected_campaign_act;
    int has_sel = (sel >= 0 && sel < CAMPAIGN_TOTAL);
    int h = WM_INFO_VIG_H + WM_INFO_TXT_H;
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    DrawRectangleRounded(r, (float)PANEL_R / h, 5, C_PANEL);
    DrawRectangleRoundedLinesEx(r, (float)PANEL_R / h, 5, has_sel ? 1.8f : 1.2f,
                                has_sel ? CHAPTER_COLORS[sel / CAMPAIGN_ACTS] : C_BORDER);

    if (has_sel) {
        const ActData *ad = campaign_act_get(sel);
        if (ad) info_vignette(sel, ad, x, y, w, t);
        info_text(meta, sel, ad, x, y + WM_INFO_VIG_H, w);
    } else {
        txt_c("Sélectionnez un acte ou un chapitre",
              x + w/2, y + h/2 - fh(10)/2, 10, C_DIM);
    }
    g_dx = 0;
    g_al = 1.0f;
}

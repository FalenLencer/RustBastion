/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_journal.c ─ Journal de campagne : relire le récit des actes joués.
 *
 *  Chaque acte TERMINÉ (au moins une étoile méta) s'inscrit au journal : on
 *  y relit son briefing puis son bilan, mis en scène comme en partie
 *  (vignette du lieu, portraits, texte qui défile). Le récit est rejoué
 *  « neutre », sans les doctrines d'une partie précise : c'est la chronique
 *  commune de la campagne, pas l'enregistrement d'un parcours.
 *
 *  Commandes : ESPACE / clic = suite (briefing → bilan → acte suivant),
 *  TAB = tout afficher, flèches = acte précédent / suivant, ÉCHAP = retour.
 */
#include "menu_internal.h"
#include "story_scene.h"

#define JR_PW          820    // largeur de la scène (comme au briefing)
#define JR_VIG_H       208    // bandeau (multiple de STORY_VIGNETTE_PX)
#define JR_TITLE_BAND  76     // dégradé sombre sous le titre du bandeau
#define JR_BOX_H       150    // boîte de dialogue
#define JR_GAP         10
#define JR_NAV_H       30     // barre : acte précédent / onglets / suivant
#define JR_ARROW_W     150
#define JR_TAB_W       120
#define JR_KEY_BASE    1000   // clés de scène distinctes de celles des interludes

static int readable(const MetaProgress *meta, int n) {
    return n >= 0 && n < CAMPAIGN_TOTAL && meta->act_stars[n] > 0;
}
/* Acte lisible suivant dans la direction dir (±1) ; -1 si aucun. */
static int next_readable(const MetaProgress *meta, int from, int dir) {
    for (int n = from + dir; n >= 0 && n < CAMPAIGN_TOTAL; n += dir)
        if (readable(meta, n)) return n;
    return -1;
}

void journal_open(MenuState *m, const MetaProgress *meta, int act) {
    if (!readable(meta, act)) act = next_readable(meta, -1, 1);
    m->journal_act  = act;     // -1 = journal encore vide
    m->journal_part = 0;
}

static void goto_act(MenuState *m, int n) {
    m->journal_act  = n;
    m->journal_part = 0;
}

/* Bandeau illustré + titre de l'acte. */
static void journal_header(Rectangle v, const ActData *ad, int n, int part) {
    story_scene_draw_vignette(v, ad ? ad->theme : THEME_WASTELAND, n);
    int y = (int)(v.y + v.height) - JR_TITLE_BAND;
    DrawRectangleGradientV((int)v.x, y, (int)v.width, JR_TITLE_BAND,
                           (Color){0, 0, 0, 0}, (Color){6, 4, 2, 225});
    dtxt_o(TextFormat("Chapitre %d — Acte %d — %s", n / CAMPAIGN_ACTS + 1, n + 1,
                      part ? "bilan" : "briefing"),
           (int)v.x + 16, y + 14, 9, (Color){214, 186, 128, 240});
    dtxt_o(ad ? ad->title : "", (int)v.x + 16, y + 14 + fh(9) + 1, 22,
           CHAPTER_COLORS[n / CAMPAIGN_ACTS]);
}

MenuAction draw_journal(MenuState *m, const MetaProgress *meta, int vw, int vh)
{
    MenuAction act = {0};
    draw_bg(m, vw, vh);
    draw_header("JOURNAL DE CAMPAGNE", vw);

    int n = m->journal_act;
    if (!readable(meta, n)) {
        txt_c_boxed("Terminez un acte de campagne pour l'inscrire au journal.",
                    vw / 2, vh / 2 - fh(11), 11, C_TEXT);
    } else {
        int pw = (JR_PW < vw - 2 * M_PAD) ? JR_PW : vw - 2 * M_PAD;
        int x  = (vw - pw) / 2;
        int y  = M_PAD + 96;
        int prev = next_readable(meta, n, -1), next = next_readable(meta, n, +1);

        // ── Navigation : acte précédent | BRIEFING  BILAN | acte suivant ──
        if ((draw_btn("« ACTE PRÉCÉDENT", x, y, JR_ARROW_W, JR_NAV_H,
                      prev >= 0 ? C_GOLD : C_DIM, 0) || IsKeyPressed(KEY_LEFT)) && prev >= 0)
            goto_act(m, prev);
        if ((draw_btn("ACTE SUIVANT »", x + pw - JR_ARROW_W, y, JR_ARROW_W, JR_NAV_H,
                      next >= 0 ? C_GOLD : C_DIM, 0) || IsKeyPressed(KEY_RIGHT)) && next >= 0)
            goto_act(m, next);
        if (draw_btn("BRIEFING", vw / 2 - JR_TAB_W - 4, y, JR_TAB_W, JR_NAV_H,
                     C_TEXT, m->journal_part == 0))
            m->journal_part = 0;
        if (draw_btn("BILAN", vw / 2 + 4, y, JR_TAB_W, JR_NAV_H,
                     C_TEXT, m->journal_part == 1))
            m->journal_part = 1;
        y += JR_NAV_H + JR_GAP;

        // ── Scène (la navigation a pu changer d'acte : on relit l'état) ──
        n = m->journal_act;
        int part = m->journal_part;
        const ActData *ad = campaign_act_get(n);
        const char *script = part == 0
            ? campaign_dialog_before(n, 0)
            : campaign_dialog_after(n, 0, n == CAMPAIGN_TOTAL - 1, 1);
        story_scene_sync(JR_KEY_BASE + n * 2 + part, NULL, script);

        Rectangle v   = {(float)x, (float)y, (float)pw, (float)JR_VIG_H};
        journal_header(v, ad, n, part);
        y += JR_VIG_H + JR_GAP;
        Rectangle box = {(float)x, (float)y, (float)pw, (float)JR_BOX_H};
        story_scene_draw_dialogue(box);
        y += JR_BOX_H + JR_GAP;

        // ── Lecture : suite, puis bilan, puis acte suivant ──
        int confirm = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                      IsKeyPressed(KEY_KP_ENTER) || vclick_r(v) || vclick_r(box);
        if (IsKeyPressed(KEY_TAB)) {
            story_scene_skip_all();
        } else if (confirm) {
            if (!story_scene_done())  story_scene_advance();
            else if (part == 0)       m->journal_part = 1;
            else if (next >= 0)       goto_act(m, next);
        }
        /* Pas de glyphe flèche dans la police (Rajdhani) : en toutes lettres. */
        txt_c("ESPACE ou clic : suite      TAB : tout afficher      FLÈCHES : changer d'acte",
              vw / 2, y + 4, 9, C_DIM);
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->back_screen;
        pop_back_screen(m);
    }
    return act;
}

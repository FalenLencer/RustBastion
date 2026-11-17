/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  interlude_story.c ─ Écrans de BRIEFING (avant l'acte) et de BILAN
 *  (après l'acte) de la campagne.
 *
 *  Composition commune, de haut en bas :
 *    1. bandeau illustré du lieu (story_vignettes.c) + titre ;
 *    2. scène dialoguée (story_scene.c) — portrait de celui qui parle ;
 *    3. la DÉCISION : ordre du jour au briefing, doctrine au bilan,
 *       présentées en CARTES cliquables qui affichent leurs conséquences
 *       chiffrées. Une décision informée est une vraie décision.
 *
 *  Les rectangles cliquables sont calculés ici et nulle part ailleurs.
 */
#include "interlude.h"
#include "story_scene.h"
#include "story_art.h"
#include "ui_utils.h"
#include "ui_anim.h"
#include "ui_palette.h"
#include "../game/campaign_data.h"
#include "../game/runperks.h"
#include "../engine/audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── Mise en page (px du canvas virtuel) ─────────────────────────
#define IS_PW_MAX     820   /* largeur max de la scène                   */
#define IS_SIDE       24    /* marge latérale mini sur écran étroit      */
#define IS_VIG_H      208   /* bandeau (multiple de STORY_VIGNETTE_PX)   */
#define IS_BOX_H      150   /* boîte de dialogue                         */
#define IS_GAP        10
#define IS_INFO_H     86    /* briefing : objectif + doctrines           */
#define IS_ORDER_H    76    /* carte d'ordre du jour                     */
#define IS_STRIP_H    30    /* bilan : ferraille / objectif / déblocage  */
#define IS_CHOICE_H   118   /* carte de doctrine                         */
#define IS_BTN_W      250
#define IS_BTN_H      34
#define IS_TITLE_BAND 76    /* dégradé sombre sous le titre du bandeau   */
#define IS_PICK_GUARD 0.30f /* s avant que les cartes soient cliquables  */
#define IS_FADE_IN    0.35f /* apparition du titre (s)                   */
#define IS_HOVER_LIFT 3     /* soulèvement d'une carte survolée (px)     */
#define IS_KEY_BEFORE 0
#define IS_KEY_AFTER  1

#define IS_COL_METHOD (Color){120, 175, 215, 255}   /* doctrine méthodique   */
#define IS_COL_RISK   (Color){225, 165,  70, 255}   /* doctrine imprévisible */
#define IS_COL_GAIN   (Color){ 96, 210, 120, 255}
#define IS_COL_COST   (Color){230, 110,  90, 255}
#define IS_COL_NEXUS  (Color){236,  64,  52, 255}

// ── Petits outils ───────────────────────────────────────────────
static Color fade(Color c, float a) {
    c.a = (unsigned char)((float)c.a * (a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a));
    return c;
}
static int key_pressed_n(int n) {   /* 1 ou 2, clavier principal ou pavé */
    return n == 1 ? (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1))
                  : (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2));
}
static int confirm_pressed(void) {
    return IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
}

static void keycap(int x, int y, const char *lbl, Color col) {
    int w = mtxt(lbl, 9) + 10, h = fh(9) + 4;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         0.3f, 4, (Color){22, 17, 9, 255});
    DrawRectangleRoundedLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h},
                                0.3f, 4, 1.0f, col);
    dtxt(lbl, x + 5, y + 2, 9, col);
}

/* Bandeau illustré + titre (glisse et apparaît à l'ouverture). */
static void header(Rectangle v, ThemeID th, int node, const char *kicker,
                   const char *title, Color tcol, float t) {
    story_scene_draw_vignette(v, th, node);
    int y = (int)(v.y + v.height) - IS_TITLE_BAND;
    DrawRectangleGradientV((int)v.x, y, (int)v.width, IS_TITLE_BAND,
                           (Color){0, 0, 0, 0}, (Color){6, 4, 2, 225});
    float a = t / IS_FADE_IN;
    int   dx = (int)(12.0f * (1.0f - ea_out_cubic(a > 1.0f ? 1.0f : a)));
    dtxt_o(kicker, (int)v.x + 16 + dx, y + 14, 9, fade((Color){214, 186, 128, 240}, a));
    dtxt_o(title,  (int)v.x + 16 + dx, y + 14 + fh(9) + 1, 22, fade(tcol, a));
}

/* Bouton d'action principal (bas-droite). Retourne 1 si cliqué. */
static int action_button(Rectangle b, const char *lbl, const char *key,
                         Vector2 vm, int enabled, int input_ok) {
    int hov = enabled && CheckCollisionPointRec(vm, b);
    Color bc = enabled ? (hov ? UI_ACCENT : UI_ACCENT_DIM) : UI_BORDER;
    DrawRectangleRounded(b, 0.25f, 6, hov ? (Color){40, 26, 8, 255} : (Color){20, 13, 5, 255});
    DrawRectangleRoundedLinesEx(b, 0.25f, 6, hov ? 2.0f : 1.5f, bc);
    int kw = mtxt(key, 9) + 10;
    int lw = mtxt(lbl, 11);
    int x  = (int)(b.x + (b.width - (float)(lw + 10 + kw)) * 0.5f);
    dtxt(lbl, x, (int)(b.y + (b.height - (float)fh(11)) * 0.5f), 11,
         enabled ? UI_TEXT : UI_TEXT_DIM);
    keycap(x + lw + 10, (int)(b.y + (b.height - (float)(fh(9) + 4)) * 0.5f), key, bc);
    return enabled && input_ok && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/* Tous les clics « ailleurs » font avancer le dialogue : avant la fin du
   script, un clic n'importe où = suite. */
static void dialogue_input(int input_ok) {
    if (!input_ok) return;
    if (IsKeyPressed(KEY_TAB)) { story_scene_skip_all(); return; }
    if (!story_scene_done() &&
        (confirm_pressed() || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
        story_scene_advance();
}

// ════════════════════════════════════════════════════════════════
// BRIEFING
// ════════════════════════════════════════════════════════════════
static void draw_info(const GameState *gs, int x, int y, int w) {
    const ActData *act = campaign_act_get(gs->campaign_stage);
    int colw = (w - IS_GAP) / 2;
    char buf[160];

    /* Objectif (+ mutateur du chapitre) */
    dtxt("OBJECTIF", x, y, 9, (Color){90, 170, 90, 255});
    clip_text(act->objective.description, colw, 12, buf, sizeof(buf));
    dtxt(buf, x, y + fh(9) + 2, 12, (Color){160, 228, 160, 255});
    const char *mut = campaign_mutator_name(campaign_mutator_for_stage(
                          campaign_difficulty_stage(gs->campaign_stage)));
    if (mut) {
        clip_text(mut, colw, 9, buf, sizeof(buf));
        dtxt(buf, x, y + fh(9) + 2 + fh(12) + 6, 9, (Color){230, 140, 70, 240});
    }

    /* Doctrines : ce que vos choix passés coûtent et rapportent, RAPPELÉ
       à chaque acte — sinon « Arsenal » ne veut plus rien dire 5 actes
       plus tard. */
    const DoctrineDef *doc[CAMPAIGN_DOCTRINES];
    int nd = campaign_doctrines_active(gs->campaign_flags, doc, CAMPAIGN_DOCTRINES);
    int dx = x + colw + IS_GAP;
    dtxt(nd ? "DOCTRINES EN VIGUEUR" : "AUCUNE DOCTRINE (pas encore de décision)",
         dx, y, 9, (Color){170, 140, 70, 230});
    int dy = y + fh(9) + 2;
    for (int i = 0; i < nd && dy + fh(9) <= y + IS_INFO_H; i++, dy += fh(9) + 2) {
        Color tc = doc[i]->methodical ? IS_COL_METHOD : IS_COL_RISK;
        int nw = mtxt(doc[i]->name, 9);
        dtxt(doc[i]->name, dx, dy, 9, tc);
        clip_text(doc[i]->effect, colw - nw - 10, 9, buf, sizeof(buf));
        dtxt(buf, dx + nw + 10, dy, 9, (Color){160, 142, 108, 230});
    }
}

/* Effet d'une consigne en segments colorés : gains en vert, coûts en rouge. */
static void draw_order_effect(const CampaignOrder *o, int x, int y, float a) {
    struct { int v; const char *u; } p[3] = {
        { o->gold, "or" }, { o->lives, "vies" }, { o->renfort, "Renfort" } };
    for (int i = 0; i < 3; i++) {
        if (!p[i].v) continue;
        char b[32];
        snprintf(b, sizeof(b), "%+d %s", p[i].v, p[i].u);
        dtxt(b, x, y, 11, fade(p[i].v > 0 ? IS_COL_GAIN : IS_COL_COST, a));
        x += mtxt(b, 11) + 18;
    }
}

static void order_card(const GameState *gs, int node, int idx, Rectangle r,
                       Vector2 vm, int clickable, int input_ok) {
    const CampaignOrder *o = campaign_order(node, idx);
    if (!o) return;
    int aff    = campaign_order_affordable(o, gs->gold, gs->lives, gs->run.renfort);
    int picked = (story_scene_pick() == idx);
    int hov    = clickable && aff && CheckCollisionPointRec(vm, r);
    float a    = aff ? 1.0f : 0.45f;
    Rectangle d = r;
    if (hov) d.y -= IS_HOVER_LIFT;

    Color edge = picked ? UI_SUCCESS : hov ? UI_ACCENT : UI_BORDER;
    DrawRectangleRounded(d, 0.12f, 6, picked ? (Color){10, 26, 14, 250} : UI_PANEL_BG);
    DrawRectangleRoundedLinesEx(d, 0.12f, 6, picked || hov ? 2.0f : 1.2f, fade(edge, a));

    char key[12];
    snprintf(key, sizeof(key), "%d", idx + 1);
    keycap((int)d.x + 10, (int)d.y + 9, key, fade(edge, a));
    dtxt(o->title, (int)d.x + 34, (int)d.y + 8, 12, fade(UI_TEXT, a));
    draw_order_effect(o, (int)d.x + 34, (int)d.y + 8 + fh(12) + 4, a);

    const char *who = o->speaker;
    const char *tag = !aff ? "ressources insuffisantes"
                    : picked ? "RETENU — recliquer pour annuler" : who;
    Color tc = !aff ? IS_COL_COST : picked ? UI_SUCCESS
             : story_speaker_color(story_speaker_from_name(who));
    dtxt(tag, (int)d.x + 34, (int)(d.y + d.height) - fh(9) - 7, 9, fade(tc, 0.85f));

    if (!aff) return;
    int hit = input_ok && clickable &&
              ((hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || key_pressed_n(idx + 1));
    if (!hit) return;
    if (picked) {                                   /* 2e clic : annule */
        story_scene_set_pick(-1);
        story_scene_say(NULL, NULL);
    } else {
        story_scene_set_pick(idx);
        story_scene_say(o->speaker, o->reply);      /* le personnage RÉPOND */
        audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
    }
}

int interlude_render_dialog_before(const GameState *gs, Vector2 vm, int input_ok,
                                   int vw, int vh) {
    int node = gs->campaign_stage, flags = gs->campaign_flags;
    const ActData *act = campaign_act_get(node);
    story_scene_sync(node * 2 + IS_KEY_BEFORE, campaign_echo(node, flags),
                     campaign_dialog_before(node, flags));
    float t = story_scene_time();

    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, 228});

    int pw = vw - 2 * IS_SIDE;
    if (pw > IS_PW_MAX) pw = IS_PW_MAX;
    int bottom = IS_INFO_H + IS_GAP + fh(9) + 4 + IS_ORDER_H + IS_GAP + IS_BTN_H;
    int total  = IS_VIG_H + IS_GAP + IS_BOX_H + IS_GAP + bottom;
    int x = (vw - pw) / 2, y = (vh - total) / 2;

    header((Rectangle){(float)x, (float)y, (float)pw, IS_VIG_H}, act->theme, node,
           act->subtitle, act->title, UI_ACCENT, t);
    y += IS_VIG_H + IS_GAP;

    story_scene_draw_dialogue((Rectangle){(float)x, (float)y, (float)pw, IS_BOX_H});
    y += IS_BOX_H + IS_GAP;

    draw_info(gs, x, y, pw);
    y += IS_INFO_H + IS_GAP;

    /* ── Ordre du jour : visible une fois le briefing entendu ── */
    int open      = story_scene_done_once();
    int clickable = open && story_scene_since_open() > IS_PICK_GUARD;
    Rectangle btn = { (float)(x + pw - IS_BTN_W),
                      (float)(y + fh(9) + 4 + IS_ORDER_H + IS_GAP),
                      (float)IS_BTN_W, (float)IS_BTN_H };
    if (open) {
        dtxt("ORDRE DU JOUR  —  une consigne avant l'assaut (facultatif)",
             x, y, 9, (Color){200, 160, 80, 235});
        int cw = (pw - IS_GAP) / 2;
        for (int i = 0; i < CAMPAIGN_ORDER_CHOICES; i++)
            order_card(gs, node, i,
                       (Rectangle){(float)(x + i * (cw + IS_GAP)), (float)(y + fh(9) + 4),
                                   (float)cw, (float)IS_ORDER_H},
                       vm, clickable, input_ok);
        dtxt("1 / 2 : consigne", x, (int)btn.y + (IS_BTN_H - fh(9)) / 2, 9, UI_TEXT_DIM);
    } else {
        dtxt("ESPACE ou clic : suite      TAB : passer le briefing",
             x, (int)btn.y + (IS_BTN_H - fh(9)) / 2, 9, UI_TEXT_DIM);
    }

    int ready   = open && story_scene_done();
    int clicked = action_button(btn, "COMMENCER L'ACTE", "ESPACE", vm,
                                ready && clickable, input_ok);
    if (!input_ok) return 0;
    if (clicked || (ready && confirm_pressed())) return 1;
    /* Un clic sur une carte ou sur le bouton ne doit pas AUSSI faire
       avancer le dialogue : sans cela, choisir une consigne terminait
       instantanément la réplique de réaction qu'elle déclenche. */
    Rectangle zone = { (float)x, (float)y, (float)pw,
                       (float)(fh(9) + 4 + IS_ORDER_H + IS_GAP + IS_BTN_H) };
    if (open && CheckCollisionPointRec(vm, zone) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return 0;
    dialogue_input(input_ok);
    return 0;
}

// ════════════════════════════════════════════════════════════════
// BILAN
// ════════════════════════════════════════════════════════════════
static void draw_stars_row(int x, int y, int stars, int max_stars) {
    const int sw = 22, gap = 6;
    for (int i = 0; i < max_stars; i++)
        ui_draw_star((float)(x + i * (sw + gap) + sw / 2), (float)(y + sw / 2),
                     (float)sw / 2.0f,
                     i < stars ? (Color){240, 204, 40, 255} : (Color){60, 48, 24, 255});
}

static void doctrine_card(const GameState *gs, int node, int idx, Rectangle r,
                          Vector2 vm, int clickable, int input_ok) {
    int flag = campaign_choice_flag(node, idx);
    const DoctrineDef *d = campaign_doctrine_by_flag(flag);
    const char *act_lbl  = campaign_choice_label(node, idx);
    if (!d || !act_lbl) return;

    Color fam = d->methodical ? IS_COL_METHOD : IS_COL_RISK;
    int   hov = clickable && CheckCollisionPointRec(vm, r);
    Rectangle c = r;
    if (hov) c.y -= IS_HOVER_LIFT;

    DrawRectangleRounded(c, 0.08f, 6, hov ? (Color){20, 14, 6, 255} : UI_PANEL_BG);
    DrawRectangleRoundedLinesEx(c, 0.08f, 6, hov ? 2.2f : 1.4f, fade(fam, hov ? 1.0f : 0.6f));
    /* Bande de famille */
    DrawRectangle((int)c.x + 2, (int)c.y + 2, (int)c.width - 4, fh(9) + 6,
                  fade(fam, 0.18f));
    dtxt(d->methodical ? "MÉTHODIQUE  —  vous bâtissez"
                       : "IMPRÉVISIBLE  —  vous jouez le tempo",
         (int)c.x + 10, (int)c.y + 5, 9, fam);
    char key[12];
    snprintf(key, sizeof(key), "%d", idx + 1);
    keycap((int)(c.x + c.width) - 28, (int)c.y + 3, key, fam);

    int ty = (int)c.y + fh(9) + 14;
    dtxt(act_lbl, (int)c.x + 12, ty, 13, UI_TEXT);
    ty += fh(13) + 2;
    char buf[160];
    snprintf(buf, sizeof(buf), "Doctrine : %s", d->name);
    dtxt(buf, (int)c.x + 12, ty, 10, fam);
    ty += fh(10) + 2;
    clip_text(d->effect, (int)c.width - 24, 9, buf, sizeof(buf));
    dtxt(buf, (int)c.x + 12, ty, 9, (Color){200, 180, 140, 240});
    ty += fh(9) + 5;

    /* Le cœur thématique, rendu VISIBLE : ce choix nourrit ou brouille le
       modèle que NEXUS construit de vous — et l'acte final en dépend. */
    int now  = campaign_nexus_prediction(gs->campaign_flags);
    int then = campaign_nexus_prediction(gs->campaign_flags | flag);
    snprintf(buf, sizeof(buf), "Modèle de NEXUS : %d %%  »  %d %%", now, then);
    Color nc = then > now ? IS_COL_COST : IS_COL_GAIN;
    dtxt(buf, (int)c.x + 12, ty, 9, nc);
    int bx = (int)c.x + 12 + mtxt(buf, 9) + 12, bw = (int)(c.x + c.width) - 14 - bx;
    if (bw > 20) {
        int by = ty + fh(9) / 2 - 2;
        DrawRectangle(bx, by, bw, 4, (Color){40, 14, 14, 255});
        DrawRectangle(bx, by, bw * then / 100, 4, fade(IS_COL_NEXUS, 0.85f));
        DrawRectangle(bx + bw * now / 100 - 1, by - 2, 2, 8, UI_TEXT_DIM);  /* repère actuel */
    }

    if (input_ok && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        story_scene_set_pick(idx);
        audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
    }
}

void interlude_render_dialog_after(const GameState *gs, int stars, int scrap_earned,
                                   int last, Vector2 vm, int input_ok, int vw, int vh) {
    int node = gs->campaign_stage, flags = gs->campaign_flags;
    const ActData *act = campaign_act_get(node);
    int has_choice = !last && campaign_has_choice(node);
    story_scene_sync(node * 2 + IS_KEY_AFTER, NULL,
                     campaign_dialog_after(node, flags, last, stars > 0));
    float t = story_scene_time();

    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, 215});

    int pw = vw - 2 * IS_SIDE;
    if (pw > IS_PW_MAX) pw = IS_PW_MAX;
    int bottom = has_choice ? fh(11) + 8 + IS_CHOICE_H : IS_BTN_H;
    int total  = IS_VIG_H + IS_GAP + IS_STRIP_H + IS_GAP + IS_BOX_H + IS_GAP + bottom;
    int x = (vw - pw) / 2, y = (vh - total) / 2;

    /* ── Bandeau + verdict ── */
    Color tcol = last ? UI_ACCENT : UI_SUCCESS;
    char kicker[160];
    snprintf(kicker, sizeof(kicker), "%s  ·  %s", act->subtitle, act->title);
    header((Rectangle){(float)x, (float)y, (float)pw, IS_VIG_H}, act->theme, node,
           kicker, last ? "CAMPAGNE TERMINÉE" : "ACTE TERMINÉ", tcol, t);
    draw_stars_row(x + pw - 16 - 2 * 28, y + IS_VIG_H - 44, stars, 2);
    y += IS_VIG_H + IS_GAP;

    /* ── Bilan : ferraille, objectif (il ORIENTE la suite), déblocage ── */
    {
        char b[96];
        snprintf(b, sizeof(b), "+%d ferraille", scrap_earned);
        int sx = x;
        int sy = y + (IS_STRIP_H - fh(11)) / 2;
        dtxt(b, sx, sy, 11, (Color){130, 200, 60, 255});
        sx += mtxt(b, 11) + 26;
        const char *obj = stars > 0 ? "OBJECTIF ATTEINT" : "OBJECTIF MANQUÉ — la route bifurque";
        dtxt(obj, sx, sy, 11, stars > 0 ? IS_COL_GAIN : UI_WARN);
        sx += mtxt(obj, 11) + 26;
        if (act->unlock_msg) {
            char u[96];
            clip_text(act->unlock_msg, x + pw - sx, 11, u, sizeof(u));
            dtxt(u, sx, sy, 11, (Color){140, 200, 240, 255});
        }
        DrawLine(x, y + IS_STRIP_H, x + pw, y + IS_STRIP_H, (Color){60, 42, 14, 200});
    }
    y += IS_STRIP_H + IS_GAP;

    story_scene_draw_dialogue((Rectangle){(float)x, (float)y, (float)pw, IS_BOX_H});
    y += IS_BOX_H + IS_GAP;

    /* ── Décision / suite ── */
    int open      = story_scene_done_once();
    int clickable = open && story_scene_since_open() > IS_PICK_GUARD;
    if (has_choice && open) {
        const char *q = campaign_choice_prompt(node);
        if (q) dtxt(q, x, y, 11, (Color){236, 204, 128, 255});
        int cw = (pw - IS_GAP) / 2;
        for (int i = 0; i < 2; i++)
            doctrine_card(gs, node, i,
                          (Rectangle){(float)(x + i * (cw + IS_GAP)), (float)(y + fh(11) + 8),
                                      (float)cw, (float)IS_CHOICE_H},
                          vm, clickable, input_ok);
    } else {
        const char *hint = !open ? "ESPACE ou clic : suite      TAB : passer"
                         : last  ? "ESPACE : retour à la carte du monde"
                                 : "ESPACE : acte suivant";
        int hw = mtxt(hint, 10);
        dtxt(hint, x + (pw - hw) / 2, y + (IS_BTN_H - fh(10)) / 2, 10,
             open ? UI_ACCENT : UI_TEXT_DIM);
    }
    /* L'avance du dialogue et la validation sont traitées par
       game_handle_dialog_after (app_flow.c), qui lit aussi la carte
       cliquée via story_scene_take_pick. */
}

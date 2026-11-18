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
#include "ui_plate.h"
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
#define IS_INFO_H     100   /* briefing : plaques objectif + doctrines   */
#define IS_IN_X       12    /* marge intérieure des plaques (px)         */
#define IS_IN_Y       8
#define IS_LINER_A    0.40f /* liseré d'une plaque au repos              */
#define IS_LINE_BG    ((Color){8, 6, 4, 210})   /* fond d'une ligne de titre : masque
                                                   les notifications du jeu dessous */
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

#define IS_HINT_GAP 22   /* entre deux indications de touches (px) */

/* Voile qui grise une plaque entière (carte inaccessible). */
static void plate_veil(Rectangle r) {
    plate_chamfer_fill(r, PLATE_CHAMFER_S, (Color){8, 6, 4, 150});
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

/* Bouton d'action principal (bas-droite), plaque de métal. 1 si cliqué. */
static int action_button(Rectangle b, const char *lbl, const char *key,
                         Vector2 vm, int enabled, int input_ok) {
    int hov = enabled && CheckCollisionPointRec(vm, b);
    plate_button(b, UI_ACCENT, hov, enabled);
    int kw = mtxt(key, 9) + 10;
    int lw = mtxt(lbl, 11);
    int x  = (int)(b.x + (b.width - (float)(lw + 10 + kw)) * 0.5f);
    dtxt(lbl, x, (int)(b.y + (b.height - (float)fh(11)) * 0.5f), 11,
         enabled ? (hov ? UI_ACCENT : UI_TEXT) : UI_TEXT_DIM);
    plate_keycap(x + lw + 10, (int)(b.y + (b.height - (float)(fh(9) + 4)) * 0.5f) - 1, key,
                 enabled ? UI_ACCENT : UI_BORDER);
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
/* Deux plaques côte à côte : l'objectif (+ mutateur du chapitre), et les
   doctrines en vigueur. */
static void draw_info(const GameState *gs, int x, int y, int w) {
    const ActData *act = campaign_act_get(gs->campaign_stage);
    int colw = (w - IS_GAP) / 2, inw = colw - 2 * IS_IN_X;
    char buf[160];

    Rectangle ro = {(float)x, (float)y, (float)colw, (float)IS_INFO_H};
    plate_panel(ro, PLATE_CHAMFER_S, IS_COL_GAIN, IS_LINER_A);
    int cx = x + IS_IN_X, cy = y + IS_IN_Y;
    plate_section(cx, cy, inw, "OBJECTIF", IS_COL_GAIN);
    cy += fh(9) + 8;
    clip_text(act->objective.description, inw, 12, buf, sizeof(buf));
    dtxt(buf, cx, cy, 12, (Color){170, 232, 170, 255});
    const char *mut = campaign_mutator_name(campaign_mutator_for_stage(
                          campaign_difficulty_stage(gs->campaign_stage)));
    if (mut) {
        clip_text(mut, inw, 9, buf, sizeof(buf));
        dtxt(buf, cx, cy + fh(12) + 6, 9, (Color){230, 140, 70, 240});
    }

    /* Doctrines : ce que vos choix passés coûtent et rapportent, RAPPELÉ
       à chaque acte — sinon « Arsenal » ne veut plus rien dire 5 actes
       plus tard. */
    const DoctrineDef *doc[CAMPAIGN_DOCTRINES];
    int nd = campaign_doctrines_active(gs->campaign_flags, doc, CAMPAIGN_DOCTRINES);
    int dx = x + colw + IS_GAP;
    Color gold = {200, 160, 80, 255};
    plate_panel((Rectangle){(float)dx, (float)y, (float)colw, (float)IS_INFO_H},
                PLATE_CHAMFER_S, gold, IS_LINER_A);
    dx += IS_IN_X;
    plate_section(dx, y + IS_IN_Y, inw, nd ? "DOCTRINES EN VIGUEUR" : "AUCUNE DOCTRINE", gold);
    int dy = y + IS_IN_Y + fh(9) + 8;
    if (!nd) dtxt("Pas encore de décision.", dx, dy, 9, UI_TEXT_DIM);
    for (int i = 0; i < nd && dy + fh(9) <= y + IS_INFO_H - IS_IN_Y; i++, dy += fh(9) + 2) {
        Color tc = doc[i]->methodical ? IS_COL_METHOD : IS_COL_RISK;
        int nw = mtxt(doc[i]->name, 9);
        dtxt(doc[i]->name, dx, dy, 9, tc);
        clip_text(doc[i]->effect, inw - nw - 10, 9, buf, sizeof(buf));
        dtxt(buf, dx + nw + 10, dy, 9, (Color){170, 150, 112, 235});
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
    Rectangle d = r;
    if (hov) d.y -= IS_HOVER_LIFT;

    /* Plaque : liseré vert si retenue, couleur d'accent au survol. */
    Color edge = picked ? UI_SUCCESS : hov ? UI_ACCENT : UI_ACCENT_DIM;
    if (hov || picked)
        plate_chamfer_line(plate_inset(d, -2.0f), PLATE_CHAMFER_S + 1.0f, fade(edge, 0.40f));
    plate_panel(d, PLATE_CHAMFER_S, edge, picked || hov ? 1.0f : IS_LINER_A);
    if (picked)                                     /* panneau teinté de vert */
        plate_chamfer_fill(plate_inset(d, PLATE_BORDER + 1.0f), PLATE_CHAMFER_S - 3.0f,
                           (Color){UI_SUCCESS.r, UI_SUCCESS.g, UI_SUCCESS.b, 22});

    char key[12];
    snprintf(key, sizeof(key), "%d", idx + 1);
    int kx = (int)d.x + IS_IN_X, ky = (int)d.y + IS_IN_Y + 2;
    int tx = kx + plate_keycap(kx, ky, key, edge) + 10;
    dtxt(o->title, tx, (int)d.y + IS_IN_Y, 12, UI_TEXT);
    draw_order_effect(o, tx, (int)d.y + IS_IN_Y + fh(12) + 3, 1.0f);

    const char *who = o->speaker;
    const char *tag = !aff ? "ressources insuffisantes"
                    : picked ? "RETENU — recliquer pour annuler" : who;
    Color tc = !aff ? IS_COL_COST : picked ? UI_SUCCESS
             : story_speaker_color(story_speaker_from_name(who));
    dtxt(tag, tx, (int)(d.y + d.height) - fh(9) - IS_IN_Y, 9, fade(tc, 0.9f));

    if (!aff) { plate_veil(d); return; }            /* grisée, non cliquable */
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
    int hy = (int)btn.y + (IS_BTN_H - fh(9) - 4) / 2;
    if (open) {
        DrawRectangle(x, y - 4, pw, fh(9) + 6, IS_LINE_BG);
        plate_section(x, y - 2, pw, "ORDRE DU JOUR  —  une consigne avant l'assaut (facultatif)",
                      (Color){200, 160, 80, 235});
        int cw = (pw - IS_GAP) / 2;
        for (int i = 0; i < CAMPAIGN_ORDER_CHOICES; i++)
            order_card(gs, node, i,
                       (Rectangle){(float)(x + i * (cw + IS_GAP)), (float)(y + fh(9) + 4),
                                   (float)cw, (float)IS_ORDER_H},
                       vm, clickable, input_ok);
        int hx = x + plate_keycap(x, hy, "1", UI_ACCENT_DIM) + 4;
        plate_key_hint(hx, hy, "2", "consigne", 1);
    } else {
        int hx = x + plate_key_hint(x, hy, "ESPACE", "ou clic : suite", 1) + IS_HINT_GAP;
        plate_key_hint(hx, hy, "TAB", "passer le briefing", 1);
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

    if (hov) plate_chamfer_line(plate_inset(c, -2.0f), PLATE_CHAMFER_S + 1.0f, fade(fam, 0.40f));
    plate_panel(c, PLATE_CHAMFER_S, fam, hov ? 1.0f : 0.6f);
    /* Bande de famille (sous l'arête haute de la plaque) */
    int band = fh(9) + 6;
    DrawRectangle((int)(c.x + PLATE_BORDER + 2), (int)(c.y + PLATE_BORDER + 1),
                  (int)(c.width - 2.0f * PLATE_BORDER - 4), band, fade(fam, 0.18f));
    DrawRectangle((int)(c.x + PLATE_BORDER + 2), (int)(c.y + PLATE_BORDER + 1), 3, band, fam);
    dtxt(d->methodical ? "MÉTHODIQUE  —  vous bâtissez"
                       : "IMPRÉVISIBLE  —  vous jouez le tempo",
         (int)c.x + IS_IN_X + 2, (int)(c.y + PLATE_BORDER + 4), 9, fam);
    char key[12];
    snprintf(key, sizeof(key), "%d", idx + 1);
    plate_keycap((int)(c.x + c.width) - 30, (int)(c.y + PLATE_BORDER + 2), key, fam);

    int ty = (int)c.y + fh(9) + 16;
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
        Color oc = stars > 0 ? IS_COL_GAIN : UI_WARN;
        plate_panel((Rectangle){(float)x, (float)y, (float)pw, (float)IS_STRIP_H},
                    PLATE_CHAMFER_S, oc, IS_LINER_A);
        char b[96];
        snprintf(b, sizeof(b), "+%d ferraille", scrap_earned);
        int sx = x + IS_IN_X;
        int sy = y + (IS_STRIP_H - fh(11)) / 2;
        dtxt(b, sx, sy, 11, (Color){130, 200, 60, 255});
        sx += mtxt(b, 11) + 26;
        const char *obj = stars > 0 ? "OBJECTIF ATTEINT" : "OBJECTIF MANQUÉ — la route bifurque";
        dtxt(obj, sx, sy, 11, oc);
        sx += mtxt(obj, 11) + 26;
        if (act->unlock_msg) {
            char u[96];
            clip_text(act->unlock_msg, x + pw - IS_IN_X - sx, 11, u, sizeof(u));
            dtxt(u, sx, sy, 11, (Color){140, 200, 240, 255});
        }
    }
    y += IS_STRIP_H + IS_GAP;

    story_scene_draw_dialogue((Rectangle){(float)x, (float)y, (float)pw, IS_BOX_H});
    y += IS_BOX_H + IS_GAP;

    /* ── Décision / suite ── */
    int open      = story_scene_done_once();
    int clickable = open && story_scene_since_open() > IS_PICK_GUARD;
    if (has_choice && open) {
        const char *q = campaign_choice_prompt(node);
        if (q) {                                     /* la question, avec son repère */
            DrawRectangle(x, y - 3, pw, fh(11) + 6, IS_LINE_BG);
            DrawRectangle(x, y, 3, fh(11), (Color){236, 204, 128, 255});
            dtxt(q, x + 8, y, 11, (Color){236, 204, 128, 255});
        }
        int cw = (pw - IS_GAP) / 2;
        for (int i = 0; i < 2; i++)
            doctrine_card(gs, node, i,
                          (Rectangle){(float)(x + i * (cw + IS_GAP)), (float)(y + fh(11) + 8),
                                      (float)cw, (float)IS_CHOICE_H},
                          vm, clickable, input_ok);
    } else {
        /* Indications de touches, centrées. */
        int hy = y + (IS_BTN_H - fh(9) - 4) / 2;
        if (!open) {
            int w1 = plate_key_hint(0, 0, "ESPACE", "ou clic : suite", 0);
            int w  = w1 + IS_HINT_GAP + plate_key_hint(0, 0, "TAB", "passer", 0);
            int hx = x + (pw - w) / 2;
            plate_key_hint(hx, hy, "ESPACE", "ou clic : suite", 1);
            plate_key_hint(hx + w1 + IS_HINT_GAP, hy, "TAB", "passer", 1);
        } else {
            const char *nxt = last ? "retour à la carte du monde" : "acte suivant";
            int w = plate_key_hint(0, 0, "ESPACE", nxt, 0);
            plate_key_hint(x + (pw - w) / 2, hy, "ESPACE", nxt, 1);
        }
    }
    /* L'avance du dialogue et la validation sont traitées par
       game_handle_dialog_after (app_flow.c), qui lit aussi la carte
       cliquée via story_scene_take_pick. */
}

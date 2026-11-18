/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  interlude.c ─ Écrans de fin de partie et d'extraction (endless).
 *  (Briefing / bilan : interlude_story.c ; butin / boutique : interlude_loot.c.)
 */
#include "interlude.h"
#include "ui_utils.h"
#include "ui_palette.h"   /* palette UI (source unique) */
#include "ui_plate.h"     /* plaques de métal (game over) */
#include "../game/campaign_data.h"
#include "../game/meta.h"
#include "../game/runperks.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <stdio.h>   // snprintf

// ── Constantes layout ────────────────────────────────────────
#define M   16
#define M_S  8

// ── Game over : plaque de métal (ui_plate.h) ────────────────
#define GO_PW          460     /* largeur du panneau                       */
#define GO_PH_DIVERGE  250     /* campagne : repli / reprise               */
#define GO_PH_RECAP    290     /* récapitulatif                            */
#define GO_HDR_H       38      /* bandeau du titre                         */
#define GO_HAZARD_W    30      /* rayures de danger aux bouts du bandeau   */
#define GO_BTN_H       38
#define GO_STAT_W      300     /* bloc des statistiques                    */
#define GO_STAT_FS     11
#define GO_LINER_A     0.65f
#define GO_RED         ((Color){200,  40,  20, 255})   /* défaite             */
#define GO_AMBER       ((Color){214, 140,  30, 255})   /* revers (bifurcation) */
#define GO_CONTINUE    ((Color){120, 210,  90, 255})
#define GO_ABANDON     ((Color){185,  95,  70, 255})
#define GO_NEUTRAL     ((Color){170, 150, 112, 255})

// ── Extraction (endless) ─────────────────────────────────────
#define EX_PW          420
#define EX_PH          260
#define EX_BTN_W       190     /* libellé + touche ESPACE sans toucher les prises */
#define EX_BTN_H       32
#define EX_OUT_H       52      /* case « conséquence » au-dessus d'un bouton */
#define EX_BEACON_HZ   1.2f    /* pulsation des balises (Hz)                */
#define EX_ACC         ((Color){ 90, 170, 220, 255})   /* point d'extraction */
#define EX_GO          ((Color){110, 200,  90, 255})   /* extraire           */
#define EX_STAY        ((Color){ 90, 160, 230, 255})   /* continuer          */

/* Balise d'extraction : boîtier sombre et feu qui pulse. */
static void beacon(int cx, int cy, Color col) {
    float p = 0.5f + 0.5f * sinf((float)GetTime() * EX_BEACON_HZ * 2.0f * PI);
    DrawRectangle(cx - 7, cy - 9, 14, 18, PLATE_FRAME_D);
    DrawRectangle(cx - 6, cy - 8, 12, 16, PLATE_FRAME);
    DrawCircle(cx, cy, 9.0f + 4.0f * p, (Color){col.r, col.g, col.b, (unsigned char)(40.0f + 50.0f * p)});
    DrawCircle(cx, cy, 4.0f, (Color){col.r, col.g, col.b, (unsigned char)(150.0f + 105.0f * p)});
    DrawCircle(cx - 1, cy - 1, 1.5f, (Color){255, 255, 255, (unsigned char)(120.0f + 100.0f * p)});
}

/* Bandeau de titre : teinte, rayures de danger (alerte) OU balises
   (extraction), titre, filet. */
static void go_header(Rectangle pr, const char *title, Color acc, int alert) {
    int x = (int)(pr.x + PLATE_BORDER + 2), w = (int)(pr.width - 2.0f * (PLATE_BORDER + 2));
    int y = (int)(pr.y + PLATE_BORDER + 1);
    DrawRectangle(x, y, w, GO_HDR_H, (Color){(unsigned char)(acc.r / 6), (unsigned char)(acc.g / 6),
                                            (unsigned char)(acc.b / 6), 255});
    if (alert) {
        Color stripe = {(unsigned char)(acc.r * 0.6f), (unsigned char)(acc.g * 0.6f), (unsigned char)(acc.b * 0.6f), 255};
        plate_hazard(x, y, GO_HAZARD_W, GO_HDR_H, stripe);
        plate_hazard(x + w - GO_HAZARD_W, y, GO_HAZARD_W, GO_HDR_H, stripe);
    } else {
        beacon(x + GO_HAZARD_W / 2, y + GO_HDR_H / 2, acc);
        beacon(x + w - GO_HAZARD_W / 2, y + GO_HDR_H / 2, acc);
    }
    dtxt_o(title, (int)(pr.x + pr.width * 0.5f) - mtxt(title, 22) / 2,
           y + (GO_HDR_H - fh(22)) / 2, 22, acc);
    DrawRectangleGradientH(x, y + GO_HDR_H, w / 2, 1, (Color){acc.r, acc.g, acc.b, 0}, acc);
    DrawRectangleGradientH(x + w / 2, y + GO_HDR_H, w - w / 2, 1, acc, (Color){acc.r, acc.g, acc.b, 0});
}

/* Bouton de fin de partie (plaque) ; `key` optionnelle. Retourne le survol. */
static int go_button(Rectangle b, const char *lbl, const char *key, Color acc, Vector2 vm) {
    int hov = CheckCollisionPointRec(vm, b);
    plate_button(b, acc, hov, 1);
    int lw = mtxt(lbl, 11), kw = key ? mtxt(key, 9) + 10 + 8 : 0;
    int x  = (int)(b.x + (b.width - (float)(lw + kw)) * 0.5f);
    dtxt(lbl, x, (int)(b.y + (b.height - (float)fh(11)) * 0.5f), 11, hov ? acc : UI_TEXT);
    if (key) plate_keycap(x + lw + 8, (int)(b.y + (b.height - (float)(fh(9) + 4)) * 0.5f) - 1, key, acc);
    return hov;
}

/* Une ligne du bloc de statistiques : libellé à gauche, valeur à droite. */
static int go_stat(int x, int y, int w, const char *label, const char *value, Color vc,
                   Texture2D icon) {
    int ic = fh(GO_STAT_FS);
    dtxt(label, x + 10, y, GO_STAT_FS, UI_TEXT_DIM);
    int vw = mtxt(value, GO_STAT_FS);
    dtxt(value, x + w - 10 - vw, y, GO_STAT_FS, vc);
    if (icon.id) draw_icon(icon, x + w - 10 - vw - ic - 4, y, ic, WHITE);
    return y + fh(GO_STAT_FS) + 4;
}

// ════════════════════════════════════════════════════
// GAME OVER
// ════════════════════════════════════════════════════
int interlude_render_gameover(const GameState *gs, Vector2 vm,
                              int vw, int vh, int allow_replay) {
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,180});

    /* Campagne : selon le nœud, la défaite peut bifurquer (repli / reprise)
       au lieu d'être une fin sèche. */
    DefeatMode dm = DEFEAT_GAMEOVER;
    int diverge = 0;
    if (gs->is_campaign) {
        dm = campaign_defeat_mode(gs->campaign_stage);
        diverge = (dm != DEFEAT_GAMEOVER);
    }

    /* P0.2 : le panneau non-bifurquant est un RÉCAP (stats + boutons) */
    int pw = GO_PW, ph = diverge ? GO_PH_DIVERGE : GO_PH_RECAP;
    int choice = 0;
    Color acc = diverge ? GO_AMBER : GO_RED;
    Rectangle pr = {cx - pw/2.0f, cy - ph/2.0f, (float)pw, (float)ph};
    plate_panel(pr, PLATE_CHAMFER, acc, GO_LINER_A);
    go_header(pr, diverge ? "POSITION PERDUE" : "BASTION TOMBÉ", acc, 1);

    int py = (int)pr.y + (int)PLATE_BORDER + 1 + GO_HDR_H + 8;
    if (gs->is_campaign) {
        const ActData *ad = campaign_act_get(gs->campaign_stage);
        char buf[80];
        clip_text(ad->subtitle, pw - M*2, 10, buf, sizeof(buf));
        dtxt_c(buf, cx, py, 10, (Color){150, 122, 78, 255});
        py += 18;
    }

    if (diverge) {
        /* Bilan du revers dans une feuillure, puis la porte de sortie. */
        int sw = GO_STAT_W, sx = cx - sw/2, sh = fh(GO_STAT_FS) + 8;
        DrawRectangle(sx, py, sw, sh, PLATE_WELL);
        DrawRectangleLines(sx, py, sw, sh, PLATE_FRAME);
        dtxt_c(TextFormat("Vague %d   ·   Ennemis éliminés : %d",
                          gs->wave_manager.number, gs->kills),
               cx, py + 4, GO_STAT_FS, UI_TEXT_SEC);
        py += sh + 10;
        const char *flavor = (dm == DEFEAT_RETREAT)
            ? "Repli possible : la mission se poursuit ailleurs."
            : "Tout n'est pas perdu — un nouvel assaut est possible.";
        dtxt_c(flavor, cx, py, 10, (Color){160, 186, 160, 255});

        /* Boutons cliquables + raccourcis rappelés dessous (en touches). */
        const char *opt1 = (dm == DEFEAT_RETREAT) ? "SE REPLIER" : "REPRENDRE (AFFAIBLI)";
        int bw2 = 200, gap2 = 12;
        int byb = cy + ph/2 - M - GO_BTN_H - fh(9) - 6;
        Rectangle cb = {(float)(cx - bw2 - gap2/2), (float)byb, (float)bw2, (float)GO_BTN_H};
        Rectangle ab = {(float)(cx + gap2/2),       (float)byb, (float)bw2, (float)GO_BTN_H};
        int hc = go_button(cb, opt1, NULL, GO_CONTINUE, vm);
        int ha = go_button(ab, "ABANDONNER LA RUN", NULL, GO_ABANDON, vm);
        const char *t1 = "continuer", *t2 = "abandonner (sauvegarde effacée)";
        int w1 = plate_key_hint(0, 0, "ESPACE", t1, 0);
        int hx = cx - (w1 + 22 + plate_key_hint(0, 0, "ÉCHAP", t2, 0)) / 2;
        plate_key_hint(hx, byb + GO_BTN_H + 6, "ESPACE", t1, 1);
        plate_key_hint(hx + w1 + 22, byb + GO_BTN_H + 6, "ÉCHAP", t2, 1);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (hc)      choice = GO_CHOICE_CONTINUE;
            else if (ha) choice = GO_CHOICE_ABANDON;
        }
    } else {
        /* ── P0.2 : RÉCAP — sobre et encourageant ────────────────
           « Tenu N vagues » + record, puis le bloc de statistiques
           (kills, or final, perks en campagne) et [REJOUER] / [MENU]
           (campagne : [RETOUR À LA CARTE]). */
        int waves = gs->wave_manager.number;
        dtxt_c(TextFormat("Tenu %d vague%s", waves, waves > 1 ? "s" : ""),
               cx, py, 16, UI_TEXT);
        py += fh(16) + 4;

        if (gs->is_endless) {
            int best = gs->meta.endless_best_wave;
            if (waves >= best && waves > 0)
                dtxt_c("NOUVEAU RECORD !", cx, py, 12, UI_SUCCESS);
            else
                dtxt_c(TextFormat("Record : %d vagues", best),
                       cx, py, 12, UI_TEXT_DIM);
            py += fh(12) + 6;
        }

        int rows = gs->is_campaign ? 3 : 2;
        int sw = GO_STAT_W, sx = cx - sw/2, sh = rows * (fh(GO_STAT_FS) + 4) + 8;
        DrawRectangle(sx, py, sw, sh, PLATE_WELL);
        DrawRectangleLines(sx, py, sw, sh, PLATE_FRAME);
        DrawRectangle(sx, py, 3, sh, acc);                       /* repère */
        int ry = py + 5;
        ry = go_stat(sx, ry, sw, "Ennemis éliminés", TextFormat("%d", gs->kills), UI_TEXT, (Texture2D){0});
        ry = go_stat(sx, ry, sw, "Or final", TextFormat("%d", gs->gold), UI_TEXT, g_icon_gold);
        if (gs->is_campaign) {
            int np = 0;
            for (int i = 0; i < PERK_COUNT; i++)
                if (gs->run.count[i] > 0) np++;
            go_stat(sx, ry, sw, "Perks de la run", TextFormat("%d", np), UI_TEXT, (Texture2D){0});
        }

        /* Boutons (clic détecté ici, action exécutée par app.c) */
        int bw2 = 170;
        int byb = cy + ph/2 - M - GO_BTN_H;
        if (allow_replay) {
            Rectangle rb = {(float)(cx - bw2 - 8), (float)byb, (float)bw2, (float)GO_BTN_H};
            Rectangle mb = {(float)(cx + 8),       (float)byb, (float)bw2, (float)GO_BTN_H};
            int hr = go_button(rb, "REJOUER", NULL, UI_ACCENT, vm);
            int hm = go_button(mb, "MENU", "ESPACE", GO_NEUTRAL, vm);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (hr) choice = GO_CHOICE_REPLAY;
                else if (hm) choice = GO_CHOICE_MENU;
            }
        } else {
            const char *lbl = gs->is_campaign ? "RETOUR À LA CARTE" : "MENU";
            int bwl = 240;
            Rectangle mb = {(float)(cx - bwl/2), (float)byb, (float)bwl, (float)GO_BTN_H};
            if (go_button(mb, lbl, "ESPACE", GO_NEUTRAL, vm) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) choice = GO_CHOICE_MENU;
        }
    }
    return choice;
}

// ════════════════════════════════════════════════════
// ÉCRAN EXTRACTION ENDLESS
// ════════════════════════════════════════════════════
void interlude_extract_buttons(int vw, int vh, Rectangle *extract, Rectangle *cont) {
    int cx = vw/2, cy = vh/2;
    float by = (float)(cy + EX_PH/2 - M_S - EX_BTN_H);
    *extract = (Rectangle){(float)(cx - EX_BTN_W - M_S), by, EX_BTN_W, EX_BTN_H};
    *cont    = (Rectangle){(float)(cx + M_S),            by, EX_BTN_W, EX_BTN_H};
}

/* Case « conséquence » au-dessus d'un bouton : ce que ce choix rapporte. */
static void ex_outcome(Rectangle b, const char *caption, const char *value, Color col,
                       Texture2D icon) {
    int x = (int)b.x, y = (int)b.y - M_S - EX_OUT_H, w = (int)b.width;
    DrawRectangle(x, y, w, EX_OUT_H, PLATE_WELL);
    DrawRectangleLines(x, y, w, EX_OUT_H, PLATE_FRAME);
    DrawRectangle(x, y, 3, EX_OUT_H, col);
    dtxt(caption, x + 10, y + 6, 9, UI_TEXT_DIM);
    int ic = icon.id ? fh(13) + 4 : 0;
    if (icon.id) draw_icon(icon, x + 10, y + 8 + fh(9), fh(13), WHITE);
    dtxt(value, x + 10 + ic, y + 8 + fh(9), 13, col);
}

void interlude_render_extract(const GameState *gs, int vw, int vh,
                              Vector2 vmouse)
{
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,170});

    Rectangle pr = {cx - EX_PW/2.0f, cy - EX_PH/2.0f, (float)EX_PW, (float)EX_PH};
    plate_panel(pr, PLATE_CHAMFER, EX_ACC, GO_LINER_A);
    go_header(pr, "POINT D'EXTRACTION", EX_ACC, 0);

    /* Où en est la run */
    int py = (int)pr.y + (int)PLATE_BORDER + 1 + GO_HDR_H + 10;
    int sw = GO_STAT_W, sx = cx - sw/2, sh = 3 * (fh(GO_STAT_FS) + 4) + 8;
    DrawRectangle(sx, py, sw, sh, PLATE_WELL);
    DrawRectangleLines(sx, py, sw, sh, PLATE_FRAME);
    DrawRectangle(sx, py, 3, sh, EX_ACC);
    int ry = py + 5;
    ry = go_stat(sx, ry, sw, "Série", TextFormat("%d", gs->endless_series + 1), UI_TEXT, (Texture2D){0});
    ry = go_stat(sx, ry, sw, "Vague", TextFormat("%d", gs->wave_manager.number), UI_TEXT, (Texture2D){0});
    go_stat(sx, ry, sw, "Multiplicateur", TextFormat("x%.1f", gs->endless_multiplier), EX_ACC, (Texture2D){0});

    /* Les deux portes : ce que chacune rapporte, puis le bouton. */
    Rectangle eb, cb;
    interlude_extract_buttons(vw, vh, &eb, &cb);
    float next = meta_endless_next_mult(gs->endless_multiplier);
    ex_outcome(eb, "EN EXTRAYANT",
               TextFormat("+%d ferraille", meta_endless_scrap(gs->wave_manager.number,
                                                              gs->endless_multiplier)),
               EX_GO, g_icon_scrap);
    ex_outcome(cb, "EN CONTINUANT",
               next >= ENDLESS_MULT_MAX ? TextFormat("mult. x%.1f (max)", next)
                                        : TextFormat("mult. x%.1f", next),
               EX_STAY, (Texture2D){0});
    go_button(eb, "EXTRAIRE", "E", EX_GO, vmouse);
    go_button(cb, "CONTINUER", "ESPACE", EX_STAY, vmouse);
}

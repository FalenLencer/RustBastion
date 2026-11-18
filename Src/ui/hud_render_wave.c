/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_wave.c ─ Bouton LANCER LA VAGUE et bandeau de fin de vague.
 *
 *  L'aperçu de la prochaine vague (autrefois un panneau flottant posé sur
 *  la carte, au-dessus du bouton) vit dans le panneau droit au repos :
 *  hud_render_intel.c.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "ui_plate.h"                // bouton en plaque de métal
// ── Bouton LANCER LA VAGUE ────────────────────────────────────
#define WAVE_BREATH_FREQ   3.2f   // respiration du halo (rad/s)
#define WAVE_URGENT_FREQ   7.0f   // respiration accélérée (bonus bientôt perdu)
#define WAVE_URGENT_RATIO  0.2f   // seuil d'urgence sur le minuteur de préparation
#define WAVE_RING_DUR      0.30f  // anneau expansif au lancement (s)
#define WAVE_RING_RADIUS   40.0f  // rayon final de l'anneau (px)
#define WAVE_HDR_FS        9      // « VAGUE 4 »
#define WAVE_TITLE_FS      18     // « LANCER »
#define WAVE_BONUS_FS      14     // « +14 »
#define WAVE_BIG_FS        26     // ennemis restants (en vague)
#define WAVE_SMALL_FS      8      // légendes
#define WAVE_BAR_H         6
#define WAVE_GO_COL        ((Color){ 70, 215, 120, 255})   /* prêt à lancer       */
#define WAVE_HURRY_COL     ((Color){243, 156,  18, 255})   /* bonus presque perdu */
#define WAVE_FIGHT_COL     ((Color){215,  85,  60, 255})   /* vague en cours      */
#define WAVE_HDR_COL       ((Color){190, 172, 128, 255})
#define WAVE_GOLD_COL      ((Color){240, 168,  45, 255})

/* Texte centré sur x. */
static void ctxt(const char *s, int cx, int y, int fs, Color c) {
    dtxt(s, cx - mtxt(s, fs) / 2, y, fs, c);
}

/* Bouton LANCER LA VAGUE : en préparation, gros appel à l'action vert
   (orange quand le bonus d'avance s'épuise) avec sa touche, le bonus en or
   et le compte à rebours ; pendant la vague, l'état du combat (ennemis
   restants + progression). */
void hud_render_wave_button(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    if (gs->wave_manager.lock_manual) return;   /* Asym pur : pas de lancement manuel */

    const Rectangle *wb = &ui->wave_btn;
    int   in_prep = (gs->phase == PHASE_PREP);
    float ratio   = gs->wave_manager.prep_timer / PREP_TIME;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    /* ── Horloges locales (accumulées via ui_dt, jamais GetTime) ── */
    static float wave_t   = 0.0f;   // respiration continue
    static float ring_t   = 1e9f;   // anneau de lancement
    static int   was_prep = 0;
    wave_t += ui_dt();
    if (was_prep && !in_prep) ring_t = 0.0f;   // prep → vague : lancement
    was_prep = in_prep;
    ring_t += ui_dt();

    int   whov = CheckCollisionPointRec(virt_mouse(), *wb) && in_prep;
    float whk  = ui_hover_t((int)wb->x, (int)wb->y, whov);
    int   wx   = (int)(wb->x + wb->width / 2);
    int   y    = (int)wb->y + 12;
    int   boss = in_prep && game_next_wave_is_boss(gs);
    int   bar_x = (int)wb->x + 12, bar_w = (int)wb->width - 24;
    int   bar_y = (int)(wb->y + wb->height) - 12 - WAVE_BAR_H;
    char  b[32];

    if (in_prep) {
        int   urgent = (ratio < WAVE_URGENT_RATIO);
        Color acc    = urgent ? WAVE_HURRY_COL : WAVE_GO_COL;
        float p      = (sinf(wave_t * (urgent ? WAVE_URGENT_FREQ : WAVE_BREATH_FREQ)) + 1.0f) * 0.5f;
        plate_chamfer_line(plate_inset(*wb, -3.0f), PLATE_CHAMFER_S + 2.0f,
                           plate_fade(acc, 0.15f + 0.35f * p));          /* halo qui respire */
        plate_button(*wb, acc, whov, 1);
        if (boss) {                                    /* préavis de BOSS : rayures */
            plate_hazard((int)wb->x + 8, (int)wb->y + 6, (int)wb->width - 16, 4, UI_DANGER);
            plate_hazard((int)wb->x + 8, (int)(wb->y + wb->height) - 9, (int)wb->width - 16, 3, UI_DANGER);
        }

        snprintf(b, sizeof(b), boss ? "VAGUE %d : BOSS" : "VAGUE %d", gs->wave_manager.number + 1);
        ctxt(b, wx, y, WAVE_HDR_FS, boss ? UI_DANGER : WAVE_HDR_COL);
        y += fh(WAVE_HDR_FS) + 6;

        int tfs = WAVE_TITLE_FS + (int)(whk + 0.5f);   /* grossit d'1 px au survol */
        Color tc = {(unsigned char)(acc.r + (255 - acc.r) * 0.35f * whk),
                    (unsigned char)(acc.g + (255 - acc.g) * 0.35f * whk),
                    (unsigned char)(acc.b + (255 - acc.b) * 0.35f * whk), 255};
        ctxt("LANCER", wx, y, tfs, tc);
        y += fh(WAVE_TITLE_FS) + 4;

        const char *key = "ENTRÉE";
        int kw = plate_key_hint(0, 0, key, "", 0) - 6;   /* largeur de la touche seule */
        plate_keycap(wx - kw / 2, y, key, UI_ACCENT);
        y += fh(9) + 4 + 2 + 8;

        /* Bonus d'avance : or gagné à lancer MAINTENANT (même calcul que le clic) */
        int bonus = wave_early_launch_bonus(&gs->wave_manager);
        snprintf(b, sizeof(b), "+%d", bonus);
        int gi = fh(WAVE_BONUS_FS) - 7, bw = gi + 3 + mtxt(b, WAVE_BONUS_FS);
        draw_icon(g_icon_gold, wx - bw / 2, y + 4, gi, bonus > 0 ? WHITE : HUD_COL_GREY);
        dtxt(b, wx - bw / 2 + gi + 3, y, WAVE_BONUS_FS, bonus > 0 ? WAVE_GOLD_COL : HUD_COL_GREY);
        y += fh(WAVE_BONUS_FS);
        ctxt("bonus d'avance", wx, y, WAVE_SMALL_FS, HUD_COL_GREY);

        snprintf(b, sizeof(b), "%.0f s", gs->wave_manager.prep_timer);
        ctxt(b, wx, bar_y - fh(10) - 2, 10, urgent ? WAVE_HURRY_COL : WAVE_HDR_COL);
        Color tcol = ratio > 0.5f ? UI_SUCCESS : ratio > WAVE_URGENT_RATIO ? UI_WARN : UI_DANGER;
        hud_draw_bar(bar_x, bar_y, bar_w, WAVE_BAR_H, ratio, tcol, (Color){16, 16, 16, 255});
    } else {
        plate_button(*wb, WAVE_FIGHT_COL, 0, 1);
        int alive    = enemy_pool_alive(&gs->enemies);
        int to_spawn = gs->wave_manager.total_to_spawn - gs->wave_manager.total_spawned;
        if (to_spawn < 0) to_spawn = 0;
        int left = alive + to_spawn;

        snprintf(b, sizeof(b), "VAGUE %d", gs->wave_manager.number);
        ctxt(b, wx, y, WAVE_HDR_FS, WAVE_HDR_COL);
        y += fh(WAVE_HDR_FS) + 6;
        ctxt("EN COURS", wx, y, 13, WAVE_FIGHT_COL);
        y += fh(13) + 6;
        snprintf(b, sizeof(b), "%d", left);
        ctxt(b, wx, y, WAVE_BIG_FS, (Color){240, 225, 200, 255});
        y += fh(WAVE_BIG_FS);
        ctxt(left > 1 ? "ennemis restants" : "ennemi restant", wx, y, WAVE_SMALL_FS, HUD_COL_GREY);

        float prog = 0.0f;
        if (gs->wave_manager.total_to_spawn > 0)
            prog = 1.0f - (float)left / (float)gs->wave_manager.total_to_spawn;
        prog = prog < 0.0f ? 0.0f : prog > 1.0f ? 1.0f : prog;
        snprintf(b, sizeof(b), "%d %%", (int)(prog * 100.0f + 0.5f));
        ctxt(b, wx, bar_y - fh(10) - 2, 10, WAVE_HDR_COL);
        hud_draw_bar(bar_x, bar_y, bar_w, WAVE_BAR_H, prog,
                     prog > 0.7f ? UI_SUCCESS : prog > 0.3f ? UI_WARN : WAVE_FIGHT_COL,
                     (Color){16, 16, 16, 255});
    }

    /* ── Anneau expansif au lancement (transition prep → vague).
       Local au bouton, dans l'espace canvas : pas de conversion
       caméra (fx_burst dessinerait dans le monde, désaligné). ── */
    if (ring_t < WAVE_RING_DUR) {
        float rk = ring_t / WAVE_RING_DUR;
        DrawCircleLines(wx, (int)(wb->y + wb->height / 2), WAVE_RING_RADIUS * rk,
                        (Color){46, 204, 113, (unsigned char)(180.0f * (1.0f - rk))});
    }
}

/* Bandeau de fin de vague (P1.1). */
void hud_render_wave_banner(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // BANDEAU DE FIN DE VAGUE (P1.1)
    // ════════════════════════════════════════════════
    if (ui->wave_banner_t > 0.0f) {
        /* Fondu : entree 0.35 s, sortie 0.5 s ; leger glissement vers le haut */
        float el  = WAVE_BANNER_TIME - ui->wave_banner_t;      /* temps ecoule */
        float a   = 1.0f;
        if (el < 0.35f)                 a = el / 0.35f;
        if (ui->wave_banner_t < 0.5f)   a = ui->wave_banner_t / 0.5f;
        int   cx  = g_map_x_off + g_canvas_virt_w_base / 2;
        int   by  = 46 - (int)(el * 3.0f);                     /* derive douce */
        if (by < 38) by = 38;
        if (ui->top_reserved_y > 38) by += ui->top_reserved_y - 38;  /* sous la bulle tuto */

        char l1[48], l2[64], l3[80];
        snprintf(l1, sizeof(l1), "VAGUE %d REPOUSSÉE !", ui->wave_banner_wave);
        snprintf(l2, sizeof(l2), "%d ennemi%s détruit%s    +%d",
                 ui->wave_banner_kills,
                 ui->wave_banner_kills > 1 ? "s" : "",
                 ui->wave_banner_kills > 1 ? "s" : "",
                 ui->wave_banner_gold);
        int has_adv = hud_counter_advice(gs, l3, (int)sizeof(l3));
        /* Tour MVP de la vague (bilan par tour : dégâts réellement infligés) */
        char l4[80];
        int  mvp = tower_wave_mvp(&gs->towers);
        if (mvp >= 0) {
            const TowerTally *tt = tower_tally(mvp);
            snprintf(l4, sizeof(l4), "Tour MVP : %s  ·  %.0f dégâts  ·  %d kill%s",
                     TOWER_BASE_STATS[gs->towers.towers[mvp].type].name, tt->dmg_wave,
                     tt->kills_wave, tt->kills_wave > 1 ? "s" : "");
        }

        int w1 = mtxt(l1, 20), w2 = mtxt(l2, 12) + fh(12) + 2;
        int w3 = has_adv ? mtxt(l3, 10) : 0;
        int w4 = mvp >= 0 ? mtxt(l4, 10) : 0;
        int pw = w1; if (w2 > pw) pw = w2; if (w3 > pw) pw = w3; if (w4 > pw) pw = w4;
        pw += 36;
        int ph = 8 + fh(20) + 4 + fh(12) + (has_adv ? 3 + fh(10) : 0)
               + (mvp >= 0 ? 3 + fh(10) : 0) + 8;

        /* Plaque de métal (style commun), fondue avec le bandeau */
        plate_alpha(a);
        plate_panel((Rectangle){(float)(cx - pw/2), (float)by, (float)pw, (float)ph},
                    PLATE_CHAMFER, UI_ACCENT, 0.85f);
        plate_alpha(1.0f);

        int ty2 = by + 8;
        Color c1 = UI_ACCENT; c1.a = (unsigned char)(255 * a);
        dtxt(l1, cx - w1/2, ty2, 20, c1);
        ty2 += fh(20) + 4;
        Color c2 = UI_TEXT;   c2.a = (unsigned char)(235 * a);
        int lx = cx - w2/2;
        dtxt(l2, lx, ty2, 12, c2);
        Color gt = WHITE;     gt.a = (unsigned char)(235 * a);
        draw_icon(g_icon_gold, lx + mtxt(l2, 12) + 2, ty2, fh(12), gt);
        ty2 += fh(12) + 3;
        if (mvp >= 0) {
            Color c4 = HUD_COL_TOWER; c4.a = (unsigned char)(235 * a);
            dtxt(l4, cx - w4/2, ty2, 10, c4);
            ty2 += fh(10) + 3;
        }
        if (has_adv) {
            Color c3 = UI_INFO; c3.a = (unsigned char)(220 * a);
            dtxt(l3, cx - w3/2, ty2, 10, c3);
        }
    }
}

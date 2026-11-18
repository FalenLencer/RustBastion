/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_wave.c ─ Bouton LANCER VAGUE (+ aperçu) et bandeau de fin de vague.
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)
// ── Bouton LANCER VAGUE (PROMPT I) ────────────────────────────
#define WAVE_BREATH_FREQ   3.2f   // respiration de la bordure (rad/s)
#define WAVE_URGENT_FREQ   7.0f   // respiration accélérée (timer bonus bas)
#define WAVE_URGENT_RATIO  0.2f   // seuil d'urgence sur le timer bonus
#define WAVE_PV_IN_DUR     0.25f  // apparition du panneau d'aperçu (s)
#define WAVE_PV_RISE       8.0f   // glissement du panneau depuis le bouton (px)
#define WAVE_PV_ICON_STEP  0.05f  // délai entre icônes d'ennemis (s)
#define WAVE_PV_ICON_DUR   0.20f  // pop d'une icône (s)
#define WAVE_RING_DUR      0.30f  // anneau expansif au lancement (s)
#define WAVE_RING_RADIUS   40.0f  // rayon final de l'anneau (px)

/* Bouton LANCER VAGUE, aperçu de la prochaine vague. */
void hud_render_wave_button(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // BOUTON LANCER VAGUE (masqué en Asym pur : pas de lancement manuel)
    // ════════════════════════════════════════════════
    if (!gs->wave_manager.lock_manual) {
        const Rectangle *wb = &ui->wave_btn;
        int   in_prep = (gs->phase == PHASE_PREP);
        float ratio   = gs->wave_manager.prep_timer / PREP_TIME;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        /* ── Horloges locales (accumulées via ui_dt, jamais GetTime) ── */
        static float wave_t   = 0.0f;   // respiration continue
        static float pv_t     = 0.0f;   // apparition de l'aperçu (prep)
        static float ring_t   = 1e9f;   // anneau de lancement
        static int   was_prep = 0;
        wave_t += ui_dt();
        if (in_prep) pv_t += ui_dt(); else pv_t = 0.0f;
        if (was_prep && !in_prep) ring_t = 0.0f;   // prep → vague : lancement
        was_prep = in_prep;
        ring_t += ui_dt();

        /* Survol (lissé) : fond éclairci + texte grossi (seulement en prep) */
        int   whov = CheckCollisionPointRec(virt_mouse(), *wb);
        float whk  = ui_hover_t((int)wb->x, (int)wb->y, whov && in_prep);

        float wrnd = (float)UI_RADIUS / wb->height;
        Color wbg  = in_prep ? (Color){(unsigned char)(5  + whk * 8.0f),
                                       (unsigned char)(20 + whk * 22.0f),
                                       (unsigned char)(8  + whk * 10.0f), 255}
                             : (Color){7, 7, 7, 255};
        DrawRectangleRounded(*wb, wrnd, 6, wbg);

        if (in_prep) {
            /* Bordure qui respire — verte, orange et rapide en urgence */
            int   urgent = (ratio < WAVE_URGENT_RATIO);
            float freq   = urgent ? WAVE_URGENT_FREQ : WAVE_BREATH_FREQ;
            float p      = (sinf(wave_t * freq) + 1.0f) * 0.5f;
            Color bc     = urgent ? (Color){243, 156, 18, 0}
                                  : (Color){ 42, 188, 105, 0};
            bc.a = (unsigned char)(200.0f + 55.0f * p);
            DrawRectangleRoundedLinesEx(*wb, wrnd, 6, 1.5f + 0.8f * p, bc);
            /* Halo : cadre 2 px plus large, alpha pulsé */
            Rectangle hb = {wb->x - 2, wb->y - 2, wb->width + 4, wb->height + 4};
            DrawRectangleRoundedLinesEx(hb, wrnd, 6, 2.0f,
                (Color){bc.r, bc.g, bc.b, (unsigned char)(p * 60.0f)});
        } else {
            DrawRectangleRoundedLinesEx(*wb, wrnd, 6, 1.5f,
                                        (Color){30, 30, 30, 255});
        }

        Color tcol = ratio > 0.5f ? UI_SUCCESS
                   : ratio > 0.2f ? UI_WARN
                                  : UI_DANGER;
        hud_draw_bar((int)wb->x + GAP,
                 (int)wb->y + (int)wb->height - 12,
                 (int)wb->width - GAP*2, 5,
                 ratio, tcol, (Color){16,16,16,255});

        int wx = (int)(wb->x + wb->width/2);
        Color wlbl = in_prep ? (Color){42,188,105,255} : (Color){58,58,58,255};

        if (in_prep) {
            const char *l1 = "LANCER";
            int lfs = 13 + (int)(whk + 0.5f);   // 13 → 14 px au survol
            dtxt(l1, wx - mtxt(l1,lfs)/2, (int)wb->y + M, lfs, wlbl);

            /* Bonus de lancement anticipé : icône or + nombre */
            char b2[14];
            snprintf(b2, sizeof(b2), "+%d", (int)(ratio*15.0f));
            int ic2 = fh(10) - 4;
            int b2w = ic2 + 3 + mtxt(b2, 10);
            draw_icon(g_icon_gold, wx - b2w/2, (int)wb->y+29, ic2, WHITE);
            dtxt(b2, wx - b2w/2 + ic2 + 3, (int)wb->y+27, 10,
                     (Color){225,145,28,255});

            char b3[10];
            snprintf(b3, sizeof(b3), "%.0fs", gs->wave_manager.prep_timer);
            dtxt(b3, wx - mtxt(b3,10)/2, (int)wb->y+40, 10,
                     UI_BORDER);
        } else {
            const char *l1 = "EN COURS";
            dtxt(l1, wx - mtxt(l1,11)/2,
                     (int)(wb->y + wb->height/2 - 8), 11, wlbl);
        }

        // ── Aperçu de la PROCHAINE VAGUE (pendant la prépa) ───────
        // Montre les types susceptibles d'arriver → choix de défense
        // éclairé. En campagne, un type non encore découvert s'affiche "?".
        if (in_prep) {
            EnemyType pv[ENEMY_TYPE_COUNT];
            int max_stage = gs->is_campaign
                ? campaign_difficulty_stage(gs->campaign_stage) - 1 : 9999;
            int npv = wave_preview_types(gs->wave_manager.number + 1,
                                         gs->map.theme, gs->is_campaign, max_stage,
                                         gs->wave_manager.arcade_bias,
                                         pv, ENEMY_TYPE_COUNT);
            if (npv > 6) npv = 6;
            if (npv > 0) {
                /* Apparition : le panneau glisse depuis le bouton en fondu */
                float pk = pv_t / WAVE_PV_IN_DUR;
                if (pk > 1.0f) pk = 1.0f;
                pk = ea_out_cubic(pk);
                int dy = (int)(WAVE_PV_RISE * (1.0f - pk));

                const int ic = 22, ig = 4, ip = 7;
                /* Préavis de BOSS (campagne ET arcade) : le panneau s'élargit
                   déjà à la largeur de l'en-tête (pw dérive de mtxt(hdr)). */
                int boss_next = game_next_wave_is_boss(gs);
                const char *hdr = boss_next ? "PROCHAINE VAGUE : BOSS !"
                                            : "PROCHAINE VAGUE";
                int hw     = mtxt(hdr, 10);
                int stripw = npv*ic + (npv-1)*ig;
                int pw     = (stripw > hw ? stripw : hw) + ip*2;
                /* En-tête à sa hauteur réelle fh(10) : avec 15 px, les
                   vignettes mordaient sur le libellé. */
                const int hdr_h = 4 + fh(10) + 1;
                int ph     = hdr_h + ic + ip;
                int ppx    = wx - pw/2;
                int ppy    = (int)wb->y - ph - 6 + dy;
                DrawRectangleRounded((Rectangle){(float)ppx,(float)ppy,(float)pw,(float)ph},
                                     0.16f, 5,
                                     (Color){10, 7, 3, (unsigned char)(230.0f * pk)});
                DrawRectangleRoundedLinesEx((Rectangle){(float)ppx,(float)ppy,(float)pw,(float)ph},
                                            0.16f, 5, 1.2f,
                                            (Color){70, 48, 16, (unsigned char)(210.0f * pk)});
                /* En-tête rouge si un boss arrive : le préavis doit sauter aux yeux */
                Color hdr_c = boss_next ? UI_DANGER : (Color){150, 120, 70, 255};
                hdr_c.a = (unsigned char)(255.0f * pk);
                dtxt(hdr, ppx + pw/2 - hw/2, ppy + 4, 10, hdr_c);
                int ix = ppx + (pw - stripw)/2, iy = ppy + hdr_h;
                for (int k = 0; k < npv; k++) {
                    /* Cascade : icône k pope avec k×0.05 s de délai */
                    float kj = (pv_t - WAVE_PV_ICON_STEP * (float)k)
                             / WAVE_PV_ICON_DUR;
                    if (kj <= 0.0f) { ix += ic + ig; continue; }
                    float af  = kj > 1.0f ? 1.0f : kj;          // fondu
                    float szf = 0.5f + 0.5f * ea_out_back(kj);  // scale 0.5→1
                    float bs  = (float)ic * szf;
                    float bx0 = (float)ix + ((float)ic - bs) * 0.5f;
                    float by0 = (float)iy + ((float)ic - bs) * 0.5f;

                    EnemyType ty = pv[k];
                    Color ec = renderer_enemy_color(ty);
                    int disc = !gs->is_campaign || gs->meta.bestiary_discovered[ty];
                    Rectangle br = {bx0, by0, bs, bs};
                    DrawRectangleRounded(br, 0.3f, 4,
                        (Color){(unsigned char)(ec.r/5),(unsigned char)(ec.g/5),
                                (unsigned char)(ec.b/5),(unsigned char)(255.0f*af)});
                    DrawRectangleRoundedLinesEx(br, 0.3f, 4, 1.0f,
                        (Color){ec.r, ec.g, ec.b, (unsigned char)(220.0f*af)});
                    if (disc && g_enemy_splash[ty].id != 0) {
                        DrawTexturePro(g_enemy_splash[ty],
                            (Rectangle){0,0,(float)g_enemy_splash[ty].width,(float)g_enemy_splash[ty].height},
                            (Rectangle){bx0 + 2.0f, by0 + 2.0f, bs - 4.0f, bs - 4.0f},
                            (Vector2){0,0}, 0.0f,
                            (Color){255,255,255,(unsigned char)(255.0f*af)});
                    } else if (disc) {
                        DrawCircle(ix+ic/2, iy+ic/2, bs*0.3f,
                                   (Color){ec.r, ec.g, ec.b, (unsigned char)(255.0f*af)});
                    } else {
                        int qw = mtxt("?", 13);
                        dtxt("?", ix+ic/2 - qw/2, iy+ic/2 - fh(13)/2, 13,
                             (Color){ec.r, ec.g, ec.b, (unsigned char)(220.0f*af)});
                    }
                    ix += ic + ig;
                }
            }
        }

        /* ── Anneau expansif au lancement (transition prep → vague).
           Local au bouton, dans l'espace canvas : pas de conversion
           caméra (fx_burst dessinerait dans le monde, désaligné). ── */
        if (ring_t < WAVE_RING_DUR) {
            float rk = ring_t / WAVE_RING_DUR;
            DrawCircleLines(wx, (int)(wb->y + wb->height/2),
                            WAVE_RING_RADIUS * rk,
                            (Color){46, 204, 113,
                                    (unsigned char)(180.0f * (1.0f - rk))});
        }
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

        int w1 = mtxt(l1, 20), w2 = mtxt(l2, 12) + fh(12) + 2;
        int w3 = has_adv ? mtxt(l3, 10) : 0;
        int pw = w1; if (w2 > pw) pw = w2; if (w3 > pw) pw = w3;
        pw += 36;
        int ph = 8 + fh(20) + 4 + fh(12) + (has_adv ? 3 + fh(10) : 0) + 8;

        Color bg = UI_PANEL_BG_SOFT;  bg.a = (unsigned char)(bg.a * a);
        Color bd = UI_ACCENT;         bd.a = (unsigned char)(200 * a);
        DrawRectangleRounded(
            (Rectangle){(float)(cx - pw/2), (float)by, (float)pw, (float)ph},
            0.25f, 6, bg);
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)(cx - pw/2), (float)by, (float)pw, (float)ph},
            0.25f, 6, 1.4f, bd);

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
        if (has_adv) {
            Color c3 = UI_INFO; c3.a = (unsigned char)(220 * a);
            dtxt(l3, cx - w3/2, ty2, 10, c3);
        }
    }
}

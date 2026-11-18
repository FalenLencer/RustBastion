/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_ovl.c ─ Panneaux flottants du HUD sur la carte.
 *
 *  Haut-droit : vague (progression d'acte), kills, vitesse. Bas-gauche : PV
 *  des bases + réparation (seulement s'il y a plusieurs bases ou des
 *  dégâts). L'ancien panneau haut-gauche (or / vies / tours / unités) a
 *  disparu : le panneau bas affiche tout cela, avec ses animations.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "ui_plate.h"                // plaques de métal (style commun)
#include "../game/campaign_data.h"   // campaign_act_get (vagues minimales)

#define OVL_WAVE_FS    12
#define OVL_LINE_FS    10
#define OVL_SLIDE_DUR  0.30f   /* changement de numéro de vague (s) */
#define OVL_SLIDE_PX   10.0f

// ── Chrome partagé : plaque chanfreinée (comme le panneau bas), liseré
//    à la couleur du panneau, poignée de déplacement. Ne décale AUCUN
//    contenu : seulement le fond et le bord — aucun hitbox ne change.
static void draw_overlay_frame(int ox, int oy, int ow, int oh, int dragging, Color accent) {
    plate_panel((Rectangle){(float)ox, (float)oy, (float)ow, (float)oh}, PLATE_CHAMFER_S,
                accent, dragging ? 1.0f : 0.55f);
    for (int d = 0; d < 3; d++)                       /* poignée (3 traits) */
        DrawRectangle(ox + ow / 2 - 9 + d * 7, oy + 5, 4, 2,
                      (Color){accent.r, accent.g, accent.b, (unsigned char)(dragging ? 230 : 150)});
}

/* Overlays carte (vague / kills / vitesse ; bases). */
void hud_render_overlays(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    const int OV_P = OVERLAY_OV_P;

    // ── Haut-droit : Vague / Kills / Vitesse ─────────────────────
    {
        const int ow = OVERLAY_W, oh = OVERLAY_TR_H;
        const int ox = (ui->overlay_tr_pos.x >= 0.0f) ? (int)ui->overlay_tr_pos.x
                                                      : g_map_x_off + g_canvas_virt_w_base - 8 - ow;
        const int oy = (ui->overlay_tr_pos.y >= 0.0f) ? (int)ui->overlay_tr_pos.y : 8;
        draw_overlay_frame(ox, oy, ow, oh, ui->dragging_overlay == 1,
                           gs->phase == PHASE_WAVE ? UI_DANGER : UI_ACCENT_DIM);

        int tx = ox + OV_P + 2, ty = oy + OV_P + OVERLAY_GRIP_H;

        // Vague (X / min en campagne). Au changement de numéro, l'ancien
        // sort vers le haut pendant que le nouveau entre par le bas.
        {
            static int   prev_wave = -1, old_wave = 0;
            static float slide_t   = 1e9f;
            int wave = gs->wave_manager.number;
            if (prev_wave < 0 || wave < prev_wave) { prev_wave = wave; slide_t = 1e9f; }
            else if (wave != prev_wave) { old_wave = prev_wave; prev_wave = wave; slide_t = 0.0f; }
            slide_t += ui_dt();

            int mw = 0;
            if (gs->is_campaign) {
                const ActData *cad = campaign_act_get(gs->campaign_stage);
                mw = cad ? cad->min_waves : 0;
            }
            char wb[28];
            Color wc = {225, 180, 90, 255};
            if (slide_t < OVL_SLIDE_DUR) {
                float k = ea_out_cubic(slide_t / OVL_SLIDE_DUR);
                if (mw) snprintf(wb, sizeof(wb), "Vague  %d / %d", old_wave, mw);
                else    snprintf(wb, sizeof(wb), "Vague  %d", old_wave);
                dtxt(wb, tx, ty - (int)(OVL_SLIDE_PX * k), OVL_WAVE_FS,
                     (Color){wc.r, wc.g, wc.b, (unsigned char)((1.0f - k) * 255.0f)});
                if (mw) snprintf(wb, sizeof(wb), "Vague  %d / %d", wave, mw);
                else    snprintf(wb, sizeof(wb), "Vague  %d", wave);
                dtxt(wb, tx, ty + (int)(OVL_SLIDE_PX * (1.0f - k)), OVL_WAVE_FS,
                     (Color){wc.r, wc.g, wc.b, (unsigned char)(k * 255.0f)});
            } else {
                if (mw) snprintf(wb, sizeof(wb), "Vague  %d / %d", wave, mw);
                else    snprintf(wb, sizeof(wb), "Vague  %d", wave);
                dtxt(wb, tx, ty, OVL_WAVE_FS, wc);
            }
        }
        ty += OVERLAY_TR_ROW_WAVE;

        dtxt(TextFormat("Kills  %d", gs->kills), tx, ty, OVL_LINE_FS, UI_TEXT_DIM);
        ty += OVERLAY_TR_ROW_KILLS;

        // Vitesse — libellé couleur + bouton cycle (rectangle partagé avec le clic)
        {
            static const char *SL[] = {"x1", "x2", "x3"};
            static const Color SC[] = {{110, 170, 110, 255}, {239, 159, 39, 255}, {231, 76, 60, 255}};
            int sidx = (ui->speed_mult >= 1 && ui->speed_mult <= 3) ? ui->speed_mult - 1 : 0;
            dtxt("Vitesse", tx, ty, OVL_LINE_FS, (Color){150, 132, 100, 230});
            dtxt(SL[sidx], tx + mtxt("Vitesse ", OVL_LINE_FS), ty, OVL_LINE_FS, SC[sidx]);
            Rectangle sbr = hud_speed_btn_rect(ui);
            int  bhov = CheckCollisionPointRec(virt_mouse(), sbr);
            const char *nxt = SL[ui->speed_mult % 3];   /* effet du clic */
            plate_chamfer_fill(sbr, 3.0f, bhov ? (Color){55, 40, 14, 255} : (Color){24, 18, 10, 235});
            plate_chamfer_line(sbr, 3.0f, bhov ? SC[ui->speed_mult % 3] : PLATE_FRAME_L);
            dtxt(nxt, (int)(sbr.x + sbr.width / 2) - mtxt(nxt, OVL_LINE_FS) / 2,
                 (int)(sbr.y + sbr.height / 2) - fh(OVL_LINE_FS) / 2, OVL_LINE_FS,
                 bhov ? SC[ui->speed_mult % 3] : (Color){190, 160, 100, 230});
        }
    }

    // ── Bas-gauche : HP des bases + réparation ─────────────
    // P1.2 : masqué tant qu'inutile (1 base intacte) — même prédicat que
    // l'input, aucune hitbox fantôme.
    if (ui->overlay_bl_pos.x >= 0.0f && hud_show_bases_panel(gs)) {
        int  _bh = overlay_bl_h(gs);
        const int _ow = OVERLAY_BL_W;
        const int _ox = (int)ui->overlay_bl_pos.x;
        const int _oy = (int)ui->overlay_bl_pos.y;
        draw_overlay_frame(_ox, _oy, _ow, _bh, ui->dragging_overlay == 2,
                           (Color){110, 200, 130, 255});   // vert = bases

        const int _px  = _ox + OV_P;
        int       _py  = _oy + OV_P + 4;
        const int _iw  = _ow - OV_P * 2;
        float _pu = (sinf((float)GetTime() * 6.0f) + 1.0f) * 0.5f;

        for (int b = 0; b < gs->map.base_count; b++) {
            const BaseInfo *base = &gs->map.bases[b];
            float ratio = (base->max_hp > 0) ? (float)base->hp / (float)base->max_hp : 0.0f;

            /* Couleur = identité de la base (bunker + onde de repérage) */
            Color bc = (!base->active || base->hp <= 0) ? (Color){100, 35, 35, 255}
                                                         : renderer_base_color(b, base->is_primary);
            const char *bname = base->is_primary ? "BASE PRINC." : TextFormat("BASE SEC.%d", b + 1);
            char hp_str[16];
            snprintf(hp_str, sizeof(hp_str), "%d/%d", base->hp > 0 ? base->hp : 0, base->max_hp);

            DrawRectangle(_px - 2, _py - 1, _iw + 4, 9,
                          (Color){(unsigned char)(bc.r / 5), (unsigned char)(bc.g / 5),
                                  (unsigned char)(bc.b / 5), 80});
            draw_icon(g_icon_heart, _px, _py - 2, 14, bc);
            dtxt(bname, _px + 16, _py, 8, bc);
            dtxt(hp_str, _px + _iw - mtxt(hp_str, 8), _py, 8, bc);
            hud_draw_bar(_px, _py + 10, _iw, 6, fmaxf(ratio, 0.0f), bc, (Color){18, 10, 4, 200});

            /* PV critiques : cadre rouge clignotant (sans masquer l'identité) */
            if (base->active && ratio > 0.0f && ratio <= 0.30f)
                DrawRectangleLines(_px - 2, _py - 1, _iw + 4, 18,
                                   (Color){231, 60, 40, (unsigned char)(80 + _pu * 130)});
            if (!base->active || base->hp <= 0)
                dtxt("DÉTRUITE", _px + _iw / 2 - 22, _py + 10, 7, (Color){180, 60, 60, 255});

            _py += 18;   /* label(8) + gap(2) + barre(6) + gap(2) */

            /* Bouton réparation (rectangle calculé par hud_input_layout) */
            if (base->active && base->hp > 0 && base->hp < base->max_hp) {
                int   rc  = base_repair_cost(base->repair_count);
                int   aff = (gs->gold >= rc);
                Rectangle rb2 = ui->repair_base_btn[b];
                int   hov = CheckCollisionPointRec(virt_mouse(), rb2);
                plate_chamfer_fill(rb2, 3.0f, aff ? (hov ? (Color){12, 48, 22, 255} : (Color){6, 26, 12, 240})
                                                  : (Color){18, 14, 10, 220});
                plate_chamfer_line(rb2, 3.0f, aff ? (Color){60, 190, 100, 230} : (Color){70, 56, 30, 180});
                char rbuf2[32];
                snprintf(rbuf2, sizeof(rbuf2), "Rép. +%d PV  %d or", BASE_REPAIR_RESTORE, rc);
                dtxt(rbuf2, (int)(rb2.x + rb2.width / 2) - mtxt(rbuf2, 8) / 2, (int)(rb2.y + 1), 8,
                     aff ? (Color){120, 230, 150, 255} : (Color){110, 90, 60, 255});
                _py += 16;   /* bouton (14) + gap (2) */
            }
            if (b < gs->map.base_count - 1) _py += 4;   /* gap inter-bases */
        }
    }
}

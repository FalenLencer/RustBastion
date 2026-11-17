/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_ovl.c ─ Panneaux flottants du HUD sur la carte.
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

// ── Chrome partagé des panneaux flottants (identité « métal militaire ») ──
// Cadre métal sombre + dégradé de volume, liseré d'accent, équerres d'angle,
// poignée teintée. Ne décale AUCUN contenu : remplace seulement le fond, le
// bord et la poignée d'origine — donc aucun hitbox n'est modifié.
static void draw_overlay_frame(int ox, int oy, int ow, int oh,
                               int dragging, Color accent) {
    Rectangle r = {(float)ox, (float)oy, (float)ow, (float)oh};
    const float rnd = 0.16f;
    // Corps métal + dégradé vertical
    DrawRectangleRounded(r, rnd, 5, (Color){9, 6, 3, 240});
    DrawRectangle(ox + 2, oy + 2,      ow - 4, (oh - 4)/2, (Color){255,255,255,7});
    DrawRectangle(ox + 2, oy + oh/2,   ow - 4, oh/2 - 2,   (Color){0,0,0,38});
    // Liseré d'accent sous la poignée (sépare en-tête / contenu)
    DrawRectangle(ox + 5, oy + 9, ow - 10, 1, (Color){accent.r, accent.g, accent.b, 130});
    // Bordure (vive pendant le glissement)
    Color bd = dragging
        ? (Color){accent.r, accent.g, accent.b, 255}
        : (Color){(unsigned char)(accent.r*0.5f), (unsigned char)(accent.g*0.5f),
                  (unsigned char)(accent.b*0.5f), 210};
    DrawRectangleRoundedLinesEx(r, rnd, 5, dragging ? 2.0f : 1.3f, bd);
    // Équerres d'angle
    const int L = 6;
    Color cc = {accent.r, accent.g, accent.b, 200};
    DrawRectangle(ox+3,      oy+3,      L, 2, cc); DrawRectangle(ox+3,      oy+3,      2, L, cc);
    DrawRectangle(ox+ow-3-L, oy+3,      L, 2, cc); DrawRectangle(ox+ow-5,   oy+3,      2, L, cc);
    DrawRectangle(ox+3,      oy+oh-5,   L, 2, cc); DrawRectangle(ox+3,      oy+oh-3-L, 2, L, cc);
    DrawRectangle(ox+ow-3-L, oy+oh-5,   L, 2, cc); DrawRectangle(ox+ow-5,   oy+oh-3-L, 2, L, cc);
    // Poignée de déplacement (3 points teintés accent)
    for (int d = 0; d < 3; d++)
        DrawRectangle(ox + ow/2 - 9 + d*9, oy + 3, 5, 2,
                      (Color){accent.r, accent.g, accent.b, 165});
}

/* Overlays carte (HUD — or / vague / vitesse). */
void hud_render_overlays(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // OVERLAYS CARTE (HUD — or / vague / vitesse)
    // ════════════════════════════════════════════════
    {
        const int OV_P = 6;   // padding interne des panneaux

        // ── Haut-gauche : OR / Tours / Unités ─────────────────
        {
            const int ow = OVERLAY_W, line1_h = 17, line2_h = 14;
            const int oh = OVERLAY_TL_H;
            const int ox = (ui->overlay_tl_pos.x >= 0.0f)
                         ? (int)ui->overlay_tl_pos.x : g_map_x_off + 8;
            const int oy = (ui->overlay_tl_pos.y >= 0.0f)
                         ? (int)ui->overlay_tl_pos.y : 8;

            int dragging_tl = (ui->dragging_overlay == 0);
            draw_overlay_frame(ox, oy, ow, oh, dragging_tl,
                               (Color){239, 170, 60, 255});   // accent or = ressources

            int tx = ox + OV_P;
            int ty = oy + OV_P + 4;  // +4 pour la poignée

            // ── Or « vivant » : compteur roulant + flash gain/perte ──
            {
                static float disp_gold = -1.0f;  // -1 = snap au 1er passage
                static int   prev_gold = -1;
                static float fx_t      = 1e9f;   // temps depuis le dernier flash
                static int   fx_gain   = 1;      // 1 = gain, 0 = perte

                /* Snap : init ou nouvelle partie (écart aberrant) */
                if (disp_gold < 0.0f ||
                    fabsf((float)gs->gold - disp_gold) > 10000.0f) {
                    disp_gold = (float)gs->gold;
                    prev_gold = gs->gold;
                }
                /* Détection gain (≥ +5) / perte → lance le flash */
                if (gs->gold != prev_gold) {
                    int d = gs->gold - prev_gold;
                    if (d >= 5)     { fx_t = 0.0f; fx_gain = 1; }
                    else if (d < 0) { fx_t = 0.0f; fx_gain = 0; }
                    prev_gold = gs->gold;
                }
                disp_gold += ((float)gs->gold - disp_gold)
                           * fminf(1.0f, ui_dt() * 8.0f);
                fx_t += ui_dt();

                int shown = (int)(disp_gold + 0.5f);
                char gbuf[20];
                if (shown >= 10000)
                    snprintf(gbuf, sizeof(gbuf), "%dk", shown / 1000);
                else
                    snprintf(gbuf, sizeof(gbuf), "%d", shown);

                /* Icône : pop de taille au gain (1.0 → 1.25 → 1.0, 0.20 s) */
                int base = fh(12);
                int isz  = base;
                if (fx_gain && fx_t < 0.20f) {
                    float bump = sinf((fx_t / 0.20f) * PI);   // 0 → 1 → 0
                    isz = (int)((float)base * (1.0f + 0.25f * bump) + 0.5f);
                }
                draw_icon(g_icon_gold, tx - (isz - base)/2, ty - (isz - base)/2,
                          isz, WHITE);

                /* Texte : blanc → or (gain, 0.25 s) ou rouge → or (perte, 0.20 s) */
                Color gcol = {230, 155, 35, 255};
                float fdur = fx_gain ? 0.25f : 0.20f;
                if (fx_t < fdur) {
                    float f = fx_t / fdur;                    // 0 = flash, 1 = normal
                    Color from = fx_gain ? WHITE : (Color){255, 120, 80, 255};
                    gcol = (Color){
                        (unsigned char)(from.r + (gcol.r - from.r) * f),
                        (unsigned char)(from.g + (gcol.g - from.g) * f),
                        (unsigned char)(from.b + (gcol.b - from.b) * f), 255};
                }
                dtxt(gbuf, tx + base + 2, ty, 12, gcol);
            }
            ty += line1_h + 3;

            // Vies — couleur et pulsation selon niveau critique
            {
                float _tp = (float)GetTime();
                float _pp = (sinf(_tp * 5.0f) + 1.0f) * 0.5f;
                Color lc;
                if (gs->lives > 40) {
                    lc = UI_SUCCESS;
                } else if (gs->lives > 15) {
                    lc = UI_WARN;
                } else {
                    unsigned char _lcr = (unsigned char)(160 + (int)(71.0f * _pp));
                    unsigned char _lcg = (unsigned char)(30  + (int)(46.0f * _pp));
                    lc = (Color){_lcr, _lcg, 40, 255};
                }

                /* ── Battement au dégât : scale élastique + anneau ── */
                static int   prev_lives = -1;
                static float hit_t      = 1e9f;
                if (prev_lives < 0 || gs->lives > prev_lives)
                    prev_lives = gs->lives;   // init / regain / nouvelle partie
                if (gs->lives < prev_lives) {
                    hit_t      = 0.0f;
                    prev_lives = gs->lives;
                }
                hit_t += ui_dt();

                int base = fh(12);
                int isz  = base;
                if (hit_t < 0.35f) {   // 1.0 → 1.5 → 1.0, retombée élastique
                    float s = 1.0f + 0.5f * (1.0f - ea_out_elastic(hit_t / 0.35f));
                    isz = (int)((float)base * s + 0.5f);
                }
                draw_icon(g_icon_heart, tx - (isz - base)/2, ty - (isz - base)/2,
                          isz, lc);
                if (hit_t < 0.40f) {   // onde de choc depuis le cœur
                    float pr = hit_t / 0.40f;
                    DrawCircleLines(tx + base/2, ty + base/2,
                                    8.0f + 18.0f * pr,
                                    (Color){lc.r, lc.g, lc.b,
                                            (unsigned char)(200.0f * (1.0f - pr))});
                }
                dtxt(TextFormat("%d", gs->lives),
                     tx + base + 2, ty, 12, lc);
            }
            ty += line1_h + 3;

            // Tours
            {
                Color tc = gs->towers.tower_count >= gs->towers.tower_limit
                    ? UI_DANGER : UI_TEXT_DIM;
                dtxt(TextFormat("Tours  %d / %d",
                             gs->towers.tower_count, gs->towers.tower_limit),
                         tx, ty, 10, tc);
                ty += line2_h + 3;
            }

            // Unités
            {
                Color uc = gs->units.count >= gs->units.unit_limit
                    ? UI_DANGER : UI_TEXT_DIM;
                dtxt(TextFormat("Unités %d / %d",
                             gs->units.count, gs->units.unit_limit),
                         tx, ty, 10, uc);
            }
        }

        // ── Haut-droit : Vague / Kills / Ennemis / Progression / Vitesse ─
        {
            int in_wave  = (gs->phase == PHASE_WAVE);
            int alive    = enemy_pool_alive(&gs->enemies);
            int to_spawn = gs->wave_manager.total_to_spawn
                         - gs->wave_manager.total_spawned;
            if (to_spawn < 0) to_spawn = 0;
            int total_left = alive + to_spawn;

            const int ow = OVERLAY_W;
            const int oh = OVERLAY_TR_H;
            const int ox = (ui->overlay_tr_pos.x >= 0.0f)
                         ? (int)ui->overlay_tr_pos.x
                         : g_map_x_off + g_canvas_virt_w_base - 8 - ow;
            const int oy = (ui->overlay_tr_pos.y >= 0.0f)
                         ? (int)ui->overlay_tr_pos.y : 8;

            int dragging_tr = (ui->dragging_overlay == 1);
            draw_overlay_frame(ox, oy, ow, oh, dragging_tr,
                               UI_DANGER);    // accent rouge = menace

            int tx = ox + OV_P, ty = oy + OV_P + 4;
            int inner_w = ow - OV_P * 2;

            // Vague (X/min en campagne pour indiquer la progression)
            // Au changement de numéro : l'ancien sort vers le haut pendant
            // que le nouveau entre par le bas (slide vertical croisé).
            {
                static int   prev_wave = -1;
                static int   old_wave  = 0;
                static float slide_t   = 1e9f;
                int wave = gs->wave_manager.number;
                if (prev_wave < 0 || wave < prev_wave) {
                    prev_wave = wave;          // init / nouvelle partie : snap
                    slide_t   = 1e9f;
                } else if (wave != prev_wave) {
                    old_wave  = prev_wave;
                    prev_wave = wave;
                    slide_t   = 0.0f;
                }
                slide_t += ui_dt();

                int _mw = 0;
                if (gs->is_campaign) {
                    const ActData *_cad = campaign_act_get(gs->campaign_stage);
                    _mw = _cad ? _cad->min_waves : 0;
                }
                char _wbuf[28];
                Color wc = {185, 145, 60, 255};

                if (slide_t < 0.30f) {
                    float k = ea_out_cubic(slide_t / 0.30f);
                    /* Ancien numéro : monte et s'efface */
                    if (gs->is_campaign)
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d / %d", old_wave, _mw);
                    else
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d", old_wave);
                    dtxt(_wbuf, tx, ty - (int)(10.0f * k), 12,
                         (Color){wc.r, wc.g, wc.b,
                                 (unsigned char)((1.0f - k) * 255.0f)});
                    /* Nouveau numéro : entre par le bas */
                    if (gs->is_campaign)
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d / %d", wave, _mw);
                    else
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d", wave);
                    dtxt(_wbuf, tx, ty + (int)(10.0f * (1.0f - k)), 12,
                         (Color){wc.r, wc.g, wc.b,
                                 (unsigned char)(k * 255.0f)});
                } else {
                    if (gs->is_campaign)
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d / %d", wave, _mw);
                    else
                        snprintf(_wbuf, sizeof(_wbuf), "Vague  %d", wave);
                    dtxt(_wbuf, tx, ty, 12, wc);
                }
            }
            ty += 17 + 3;

            // Kills
            dtxt(TextFormat("Kills  %d", gs->kills),
                     tx, ty, 10, UI_TEXT_DIM);
            ty += 14 + 3;

            // Ennemis restants
            dtxt(in_wave ? TextFormat("Ennemis  %d", total_left)
                             : "Ennemis  --",
                     tx, ty, 10,
                     in_wave ? (Color){218, 90, 70, 255}
                             : (Color){60, 48, 30, 255});
            ty += 14 + 3;

            // Barre de progression de vague
            {
                float prog = 0.0f;
                if (in_wave && gs->wave_manager.total_to_spawn > 0)
                    prog = 1.0f - (float)total_left
                                / (float)gs->wave_manager.total_to_spawn;
                prog = prog < 0.0f ? 0.0f : prog > 1.0f ? 1.0f : prog;
                Color pc = prog > 0.7f ? UI_SUCCESS
                         : prog > 0.3f ? UI_WARN
                                       : (Color){218,  90,  70, 255};
                hud_draw_bar(tx, ty, inner_w, 7,
                         in_wave ? prog : 0.0f,
                         pc, (Color){18, 10, 4, 200});
            }
            ty += 9 + 5;

            // Vitesse — label couleur + bouton cycle [>>]
            {
                const char *sl[] = {"x1", "x2", "x3"};
                const Color sc[] = {
                    {80, 118, 80, 255}, {239, 159, 39, 255}, {231, 76, 60, 255}
                };
                int sidx = (ui->speed_mult >= 1 && ui->speed_mult <= 3)
                         ? ui->speed_mult - 1 : 0;

                // Label "Vitesse x1/x2/x3"
                dtxt("Vitesse", tx, ty, 10, (Color){82, 65, 40, 210});
                dtxt(sl[sidx], tx + mtxt("Vitesse ", 10), ty, 10, sc[sidx]);

                // Bouton [>>] — même position que dans le gestionnaire de clics
                const int bw = 26, bh = 14;
                Rectangle sbr = {
                    (float)(ox + OVERLAY_W - OV_P - bw),
                    (float)ty, (float)bw, (float)bh
                };
                int bhov = CheckCollisionPointRec(virt_mouse(), sbr);
                Color bbg  = bhov ? (Color){55, 40, 12, 255} : (Color){28, 20, 6, 220};
                Color bbrd = bhov ? sc[sidx] : (Color){70, 52, 20, 200};
                DrawRectangleRounded(sbr, 0.3f, 4, bbg);
                DrawRectangleRoundedLinesEx(sbr, 0.3f, 4, bhov ? 1.5f : 1.0f, bbrd);
                // Affiche la prochaine vitesse pour indiquer l'effet du clic
                const char *nxt = sl[ui->speed_mult % 3];
                int nw = mtxt(nxt, 10);
                dtxt(nxt, (int)sbr.x + bw/2 - nw/2,
                     (int)sbr.y + bh/2 - fh(10)/2, 10,
                     bhov ? sc[ui->speed_mult % 3] : (Color){120, 90, 45, 220});
            }
        }
    }

        // ── Bas-gauche : HP des bases + réparation ─────────────
        // P1.2 : masque tant qu'inutile (1 base intacte) — meme predicat
        // que l'input, aucune hitbox fantome.
        if (ui->overlay_bl_pos.x >= 0.0f && hud_show_bases_panel(gs)) {
            int  _bh = overlay_bl_h(gs);
            const int _ow = OVERLAY_BL_W;
            const int _ox = (int)ui->overlay_bl_pos.x;
            const int _oy = (int)ui->overlay_bl_pos.y;

            int dragging_bl = (ui->dragging_overlay == 2);
            draw_overlay_frame(_ox, _oy, _ow, _bh, dragging_bl,
                               (Color){110, 200, 130, 255});  // accent vert = bases

            const int OV_P = OVERLAY_OV_P;   /* alias local (hors portée du bloc parent) */
            const int _px  = _ox + OV_P;
            int       _py  = _oy + OV_P + 4;
            const int _iw  = _ow - OV_P * 2;

            float _t  = (float)GetTime();
            float _pu = (sinf(_t * 6.0f) + 1.0f) * 0.5f;

            for (int b = 0; b < gs->map.base_count; b++) {
                const BaseInfo *base = &gs->map.bases[b];
                float ratio = (base->max_hp > 0)
                    ? (float)base->hp / (float)base->max_hp : 0.0f;

                /* Couleur = identité de la base (différencie les bases entre elles,
                   et correspond au bunker + à l'onde de repérage sur la carte). */
                Color bc = (!base->active || base->hp <= 0)
                         ? (Color){100, 35, 35, 255}
                         : renderer_base_color(b, base->is_primary);

                /* Étiquette base + HP fraction */
                const char *bname = base->is_primary
                    ? "BASE PRINC." : TextFormat("BASE SEC.%d", b + 1);
                char hp_str[16];
                snprintf(hp_str, sizeof(hp_str), "%d/%d",
                         base->hp > 0 ? base->hp : 0, base->max_hp);

                /* Fond de ligne */
                Color _bg_line = {
                    (unsigned char)(bc.r / 5),
                    (unsigned char)(bc.g / 5),
                    (unsigned char)(bc.b / 5), 80
                };
                DrawRectangle(_px - 2, _py - 1, _iw + 4, 9, _bg_line);

                draw_icon(g_icon_heart, _px, _py - 2, 14, bc);
                dtxt(bname, _px + 16, _py, 8, bc);
                int _hw = mtxt(hp_str, 8);
                dtxt(hp_str, _px + _iw - _hw, _py, 8, bc);

                hud_draw_bar(_px, _py + 10, _iw, 6, fmaxf(ratio, 0.0f), bc,
                         (Color){18, 10, 4, 200});

                /* PV critiques : cadre rouge clignotant (sans masquer l'identité) */
                if (base->active && ratio > 0.0f && ratio <= 0.30f) {
                    unsigned char _fa = (unsigned char)(80 + _pu * 130);
                    DrawRectangleLines(_px - 2, _py - 1, _iw + 4, 18,
                                       (Color){231, 60, 40, _fa});
                }

                if (!base->active || base->hp <= 0)
                    dtxt("DÉTRUITE", _px + _iw/2 - 22, _py + 10, 7,
                             (Color){180, 60, 60, 255});

                _py += 18;   /* label(8) + gap(2) + barre(6) + gap(2) */

                /* Bouton réparation */
                if (base->active && base->hp > 0 && base->hp < base->max_hp) {
                    int   rc    = base_repair_cost(base->repair_count);
                    int   aff   = (gs->gold >= rc);
                    Color rbc2  = aff ? (Color){5, 22, 10, 235}  : (Color){10, 8, 5, 200};
                    Color rbrd2 = aff ? (Color){28, 130, 55, 200} : (Color){55, 40, 15, 140};
                    Color rtc2  = aff ? (Color){42, 185, 90, 255} : (Color){55, 42, 22, 255};
                    Rectangle rb2 = ui->repair_base_btn[b];
                    DrawRectangleRounded(rb2, 0.25f, 4, rbc2);
                    DrawRectangleRoundedLinesEx(rb2, 0.25f, 4, 1.0f, rbrd2);
                    char rbuf2[32];
                    snprintf(rbuf2, sizeof(rbuf2), "Rep. +%d PV  %d or",
                             BASE_REPAIR_RESTORE, rc);
                    int rtw2 = mtxt(rbuf2, 8);
                    dtxt(rbuf2,
                         (int)(rb2.x + rb2.width/2 - rtw2/2),
                         (int)(rb2.y + 3), 8, rtc2);
                    _py += 16;   /* bouton (14) + gap (2) */
                }

                if (b < gs->map.base_count - 1) _py += 4;   /* gap inter-bases */
            }
        }
}

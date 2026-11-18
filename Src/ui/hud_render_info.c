/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_info.c ─ Panneau gauche, ruban du build roguelite, infobulle d'outil.
 *
 *  Extrait de hud_render.c (règle projet : fichiers < 500 lignes) ; le code
 *  des sections est inchangé, ui_render() les appelle dans le même ordre.
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)
#include "../game/hints.h"           // HINT_SHOW_TIME (fondu du bandeau CONSEIL)

static void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x + w, y, col);
}

/* Panneau gauche — vies, ferraille, thème, inventaire. */
void hud_render_left_panel(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // PANNEAU GAUCHE — vies, ferraille, thème, inventaire
    // ════════════════════════════════════════════════
    {
        const int px    = M + 2;
        const int bar_x = px + 32;
        const int bar_w = UI_LEFT_PANEL_W - 32 - 30 - M;
        const int val_x = UI_LEFT_PANEL_W - 30;
        int py = HUD_Y + M;

        // FERRAILLE — uniquement en campagne
        if (gs->is_campaign) {
            float sr = fminf((float)gs->meta.scrap / 200.0f, 1.0f);
            draw_icon(g_icon_scrap, px, py - 1, 16, WHITE);
            hud_draw_bar(bar_x, py + 2, bar_w, 7, sr,
                     (Color){127, 200, 50, 255}, (Color){4, 16, 4, 200});
            dtxt(TextFormat("%d", gs->meta.scrap),
                     val_x, py + 1, 10, (Color){127, 200, 50, 255});
            py += 16;
        }

        draw_sep(px, py, UI_LEFT_PANEL_W - M * 2, (Color){40, 25, 6, 130});
        py += 5;

        // Thème
        {
            char tbuf[26];
            clip_text(th->name, UI_LEFT_PANEL_W - M * 2, 10, tbuf, sizeof(tbuf));
            dtxt(tbuf, px, py, 10, (Color){48, 82, 48, 255});
            py += 12;
        }

        // Inventaire
        if (gs->inventory_count > 0) {
            dtxt(TextFormat("Mat: %d", gs->inventory_count),
                     px, py, 9, (Color){62, 165, 185, 255});
        }

        // ── Achats de slots supplémentaires (bas du panneau gauche) ──
        // P1.2 : caches en debut de partie (predicat partage avec l'input) ;
        // a la place, un rappel discret de l'aide pour les nouveaux joueurs.
        if (!hud_show_slot_buys(gs)) {
            dtxt("[H] commandes", px, HUD_Y + HUD_H - 44, 10,
                     (Color){72, 58, 38, 190});
        } else {
            const int bw = UI_LEFT_PANEL_W - M * 2 - 4;

            /* Séparateur + titre */
            draw_sep(px, HUD_Y + HUD_H - 59, bw,
                     (Color){40, 25, 6, 110});
            dtxt("SLOTS", px, HUD_Y + HUD_H - 75, 10,
                     (Color){72, 58, 38, 180});

            const char *slot_labels[2];
            char        slot_bufs[2][40];
            Color       slot_cols[2];
            const Rectangle *slot_rects[2] = {
                &ui->buy_tower_slot_btn,
                &ui->buy_unit_slot_btn,
            };

            /* Tours */
            {
                int _b      = gs->slots_tower_bought;
                int _bl     = tower_active_limit(&gs->bonuses);
                int _at_max = (_b >= SLOT_MAX_BUYS || (_bl + _b) >= MAX_TOWERS_HARD);
                if (_at_max)
                    snprintf(slot_bufs[0], sizeof(slot_bufs[0]), "+1 Tour  [MAX]");
                else
                    snprintf(slot_bufs[0], sizeof(slot_bufs[0]),
                             "+1 Tour  %d or", SLOT_TOWER_COSTS[_b]);
                slot_labels[0] = slot_bufs[0];
                slot_cols[0]   = _at_max
                    ? (Color){55, 42, 18, 200}
                    : (gs->gold >= (_b < SLOT_MAX_BUYS ? SLOT_TOWER_COSTS[_b] : 99999))
                    ? (Color){200, 160, 50, 255}
                    : (Color){100, 75, 25, 200};
            }
            /* Unités */
            {
                int _b      = gs->slots_unit_bought;
                int _ul     = unit_active_limit(&gs->bonuses, gs->map.base_count);
                int _at_max = (_b >= SLOT_MAX_BUYS || (_ul + _b) >= MAX_UNITS);
                if (_at_max)
                    snprintf(slot_bufs[1], sizeof(slot_bufs[1]), "+1 Unité  [MAX]");
                else
                    snprintf(slot_bufs[1], sizeof(slot_bufs[1]),
                             "+1 Unité  %d or", SLOT_UNIT_COSTS[_b]);
                slot_labels[1] = slot_bufs[1];
                slot_cols[1]   = _at_max
                    ? (Color){55, 42, 18, 200}
                    : (gs->gold >= (_b < SLOT_MAX_BUYS ? SLOT_UNIT_COSTS[_b] : 99999))
                    ? (Color){160, 220, 80, 255}
                    : (Color){60, 90, 40, 200};
            }

            Vector2 _vms = virt_mouse();
            for (int _s = 0; _s < 2; _s++) {
                const Rectangle *_sr = slot_rects[_s];
                int _hov = CheckCollisionPointRec(_vms, *_sr);
                DrawRectangleRounded(*_sr, 0.20f, 4,
                    _hov ? (Color){20, 14, 4, 235} : (Color){12, 8, 2, 210});
                DrawRectangleRoundedLinesEx(*_sr, 0.20f, 4, 1.2f,
                    _hov ? (Color){120, 90, 30, 220} : (Color){55, 40, 12, 160});
                int _tw2 = mtxt(slot_labels[_s], 11);
                dtxt(slot_labels[_s],
                     (int)(_sr->x + _sr->width/2 - _tw2/2),
                     (int)(_sr->y + _sr->height/2 - 7),
                     11, slot_cols[_s]);
            }

            (void)bw;
        }
    }
}

/* Build roguelite actif (campagne) — ruban haut-centre. */
void hud_render_build_ribbon(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // BUILD ROGUE-LITE ACTIF (campagne) — ruban haut-centre
    // ════════════════════════════════════════════════
    if (gs->is_campaign) {
        const RunBuild *rb = &gs->run;
        int ids[PERK_COUNT], nt = 0;
        for (int i = 0; i < PERK_COUNT; i++)
            if (rb->count[i] > 0) ids[nt++] = i;

        if (nt > 0 || rb->renfort > 0) {
            const int FS = 9, PAD = 7, GAP = 9, CHIP = 4;
            char rbuf[24];
            snprintf(rbuf, sizeof(rbuf), "Renfort %d", rb->renfort);

            // Mesure de la largeur totale
            char tags[PERK_COUNT][16];
            int  total = mtxt(rbuf, FS) + GAP;
            for (int k = 0; k < nt; k++) {
                int id = ids[k];
                if (rb->count[id] > 1)
                    snprintf(tags[k], sizeof(tags[k]), "%s x%d",
                             RUN_PERKS[id].tag, rb->count[id]);
                else
                    snprintf(tags[k], sizeof(tags[k]), "%s", RUN_PERKS[id].tag);
                total += mtxt(tags[k], FS) + CHIP*2 + GAP;
            }

            int ph  = fh(FS) + 8;
            /* BORNE DE LARGEUR : `total` croît avec le nombre de perks
               possédés (jusqu'à PERK_COUNT). Sans plafond, un build de fin
               de campagne dépasse la largeur du canvas et le ruban déborde
               des DEUX côtés (px0 devient négatif). On borne le ruban et on
               résume le surplus par un « +N ». */
            const int RIB_MAX_W = g_canvas_virt_w_base - 40;
            int pw  = total + PAD*2;
            int truncated = 0;
            if (pw > RIB_MAX_W) { pw = RIB_MAX_W; truncated = 1; }
            int px0 = g_map_x_off + g_canvas_virt_w_base/2 - pw/2;
            int py0 = 6;

            DrawRectangleRounded((Rectangle){(float)px0,(float)py0,(float)pw,(float)ph},
                                 0.5f, 6, (Color){8, 6, 2, 215});
            DrawRectangleRoundedLinesEx((Rectangle){(float)px0,(float)py0,(float)pw,(float)ph},
                                 0.5f, 6, 1.2f, (Color){90, 78, 44, 190});

            int x  = px0 + PAD;
            int ty = py0 + (ph - fh(FS))/2;
            dtxt(rbuf, x, ty, FS, (Color){232, 200, 80, 255});
            x += mtxt(rbuf, FS) + GAP;

            /* Réserve de quoi écrire « +N » si tout ne rentre pas. */
            int x_end = px0 + pw - PAD;
            int drawn = 0;
            for (int k = 0; k < nt; k++) {
                int id = ids[k];
                RunColor rc = runperk_rarity_color(RUN_PERKS[id].rarity);
                int tw = mtxt(tags[k], FS);
                int need = tw + CHIP*2 + GAP;
                int reserve = (truncated && k < nt - 1) ? mtxt("+99", FS) + GAP : 0;
                if (x + need + reserve > x_end) break;
                // Puce colorée selon la rareté
                DrawRectangleRounded(
                    (Rectangle){(float)(x - CHIP), (float)(py0+2),
                                (float)(tw + CHIP*2), (float)(ph-4)},
                    0.5f, 4,
                    (Color){(unsigned char)(rc.r/5),(unsigned char)(rc.g/5),(unsigned char)(rc.b/5),230});
                dtxt(tags[k], x, ty, FS, (Color){rc.r, rc.g, rc.b, 255});
                x += need;
                drawn++;
            }
            if (drawn < nt) {
                char more[12];   /* « + » + int 32 bits complet + NUL */
                snprintf(more, sizeof(more), "+%d", nt - drawn);
                dtxt(more, x, ty, FS, (Color){168, 148, 102, 235});
            }
        }
    }
}

/* Indice contextuel (game/hints.c) : bandeau « CONSEIL » en haut, centré
   sur la carte, sous le ruban du build et l'indicateur de pause tactique. */
#define HINT_Y         76     // bord haut du bandeau (px canvas)
#define HINT_FADE_IN   0.30f  // apparition (s)
#define HINT_FADE_OUT  0.60f  // disparition (s)
#define HINT_FS        10
#define HINT_TAG_FS    8
void hud_render_hint(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    if (ui->hint_t <= 0.0f || !ui->hint_text) return;
    float shown = HINT_SHOW_TIME - ui->hint_t;
    float a = (shown < HINT_FADE_IN) ? shown / HINT_FADE_IN : 1.0f;
    if (ui->hint_t < HINT_FADE_OUT) a = ui->hint_t / HINT_FADE_OUT;
    if (a < 0.0f) a = 0.0f;
    const char *tag = "CONSEIL";
    int tw = mtxt(ui->hint_text, HINT_FS), gw = mtxt(tag, HINT_TAG_FS) + 10;
    int bw = tw + gw + M * 3, bh = fh(HINT_FS) + 12;
    int y  = (ui->top_reserved_y > HINT_Y) ? ui->top_reserved_y : HINT_Y;
    int x  = g_map_x_off + g_canvas_virt_w_base / 2 - bw / 2;
    Rectangle r = {(float)x, (float)y, (float)bw, (float)bh};
    DrawRectangleRounded(r, 0.4f, 6, (Color){10, 16, 24, (unsigned char)(230.0f * a)});
    DrawRectangleRoundedLinesEx(r, 0.4f, 6, 1.5f, (Color){110, 180, 230, (unsigned char)(230.0f * a)});
    int ch = fh(HINT_TAG_FS) + 4, cy = y + (bh - ch) / 2;
    DrawRectangleRounded((Rectangle){(float)(x + M), (float)cy, (float)gw, (float)ch}, 0.4f, 4,
                         (Color){110, 180, 230, (unsigned char)(255.0f * a)});
    dtxt(tag, x + M + 5, cy + 2, HINT_TAG_FS, (Color){10, 16, 24, (unsigned char)(255.0f * a)});
    dtxt(ui->hint_text, x + M * 2 + gw, y + (bh - fh(HINT_FS)) / 2, HINT_FS,
         (Color){215, 232, 245, (unsigned char)(255.0f * a)});
}

/* Infobulle de l'outil survolé (au-dessus du reste). */
void hud_render_tool_tooltip(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    // ════════════════════════════════════════════════
    // TOOLTIP OUTIL (au-dessus de tout le reste)
    // ════════════════════════════════════════════════
    if (ui->hovered_tool != -1) {
        const ToolInfo  *info   = &TOOL_INFO[ui->hovered_tool];
        const Rectangle *rb     = &ui->tool_btns[ui->hovered_tool];
        int              locked = !tool_is_unlocked((ToolID)ui->hovered_tool, gs);

        const int TW = 170;
        int at_limit = !locked &&
            ((ui_tool_is_tower((ToolID)ui->hovered_tool) &&
              gs->towers.tower_count >= gs->towers.tower_limit) ||
             (ui_tool_is_unit((ToolID)ui->hovered_tool) &&
              gs->units.count >= gs->units.unit_limit));
        /* Hauteur au contenu, pas vertical fh() (l'ancien pas de 12-14 px
           pour du texte de fh(10)=16 px superposait les lignes). */
        const int TL = fh(10) + 1;                        /* pas d'une ligne */
        const int TH = locked ? M * 2 + fh(11) + 2 + TL
                              : M * 2 + fh(11) + 2 + TL * 3 + (at_limit ? TL : 0);

        /* Toujours AU-DESSUS du HUD (bas de la carte) : borné dans le HUD,
           il recouvrait le bouton survolé et ses voisins. */
        int tx = (int)rb->x;
        int ty = HUD_Y - TH - GAP;
        if (ty < 2) ty = 2;
        if (tx < UI_LEFT_PANEL_W + M) tx = UI_LEFT_PANEL_W + M;
        if (tx + TW > VIRT_W - UI_PANEL_W - M)
            tx = VIRT_W - UI_PANEL_W - M - TW;

        float trnd = (float)UI_RADIUS / TH;
        Color border = locked ? (Color){80, 55, 20, 200} : TOOL_COLORS[ui->hovered_tool];
        DrawRectangleRounded(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, UI_PANEL_BG);
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, 1.5f, border);

        char dbuf[48];
        clip_text(info->name, TW - M*2, 11, dbuf, sizeof(dbuf));
        dtxt(dbuf, tx+M, ty+M, 11,
             locked ? (Color){80, 62, 35, 255} : TOOL_COLORS[ui->hovered_tool]);

        int ly = ty + M + fh(11) + 2;
        if (locked) {
            TowerType tt = ui_tool_to_tower((ToolID)ui->hovered_tool);
            const char *when = TOWER_UNLOCK_ACT_NAME[tt];
            clip_text(TextFormat("Déblocage : %s", when ? when : "?"),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ly, 10, (Color){130, 95, 40, 255});
        } else {
            clip_text(info->dmg > 0.0f
                          ? TextFormat("Dégâts %.0f  Portée %.1f", info->dmg, info->range)
                          : "Non combatif",
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ly, 10, (Color){145,125,92,255});           ly += TL;
            clip_text(info->dmg > 0.0f
                          ? TextFormat("Cadence %.1f/s  %d or", info->rate, info->cost)
                          : TextFormat("%d or", info->cost),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ly, 10, (Color){145,125,92,255});           ly += TL;
            clip_text(info->desc, TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ly, 10, UI_BORDER);                         ly += TL;

            if (at_limit)
                dtxt(TextFormat("LIMITE (%d bases)", gs->map.base_count),
                     tx + M, ly, 10, UI_DANGER);

            /* P0.3 — Conseil de contre au-dessus du tooltip (tours only) :
               « quelle tour pour la vague qui vient ? » */
            if (ui_tool_is_tower((ToolID)ui->hovered_tool)) {
                char adv[80];
                if (hud_counter_advice(gs, adv, sizeof(adv))) {
                    int aw = mtxt(adv, 10), ah = fh(10) + 6;
                    int ay = ty - ah - 2;
                    DrawRectangle(tx, ay, aw + M * 2, ah, UI_PANEL_BG_SOFT);
                    dtxt(adv, tx + M, ay + 3, 10, UI_INFO);
                }
            }
        }
    }
}

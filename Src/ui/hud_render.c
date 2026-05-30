/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render.c ─ Rendu du HUD en jeu.
 *
 *  Contient :
 *    draw_bar      — Barre de progression générique (static)
 *    draw_sep      — Séparateur horizontal (static)
 *    draw_portrait — Splash art dans un carré avec fond + bordure (static)
 *    draw_tool_btn — Bouton outil (tour / unité) avec splash ou icône (static)
 *    ui_render     — Rendu complet du HUD par frame
 */

#include "hud_internal.h"

// ════════════════════════════════════════════════════
// HELPERS DE RENDU INTERNES
// ════════════════════════════════════════════════════

static void draw_bar(int x, int y, int w, int h,
                     float ratio, Color fill, Color bg) {
    float r = ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f : ratio;
    float rnd = h > 4 ? 0.5f : 0.3f;
    DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)w,(float)h},
                         rnd, 4, bg);
    int fw = (int)(w * r);
    if (fw > 1)
        DrawRectangleRounded((Rectangle){(float)x,(float)y,(float)fw,(float)h},
                             rnd, 4, fill);
}

static void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x + w, y, col);
}

// ── Portrait splash art ───────────────────────────────────────────
// Dessine une texture dans un carré psz×psz avec :
//   • fit proportionnel centré (pas de déformation)
//   • fond sombre + bordure colorée
// Retourne 1 si une image a bien été dessinée, 0 sinon.
static int draw_portrait(Texture2D tex, int px, int py, int psz, Color border) {
    if (tex.id == 0) return 0;

    // Fond sombre
    DrawRectangle(px, py, psz, psz, (Color){6, 3, 1, 220});
    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)psz,(float)psz},
                         1.5f, border);

    // Image redimensionnée proportionnellement
    float scale = fminf((float)psz / (float)tex.width,
                        (float)psz / (float)tex.height);
    int dw = (int)((float)tex.width  * scale);
    int dh = (int)((float)tex.height * scale);
    int ox = px + (psz - dw) / 2;
    int oy = py + (psz - dh) / 2;
    DrawTexturePro(tex,
        (Rectangle){0, 0, (float)tex.width, (float)tex.height},
        (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh},
        (Vector2){0, 0}, 0.0f, WHITE);
    return 1;
}

static void draw_tool_btn(const Rectangle *r, ToolID id,
                           int is_selected, int is_hovered,
                           int can_afford, int is_locked)
{
    const ToolInfo *info = &TOOL_INFO[id];
    Color col = TOOL_COLORS[id];
    float rnd = (float)UI_RADIUS / r->height;

    Color bg = is_selected ? (Color){38, 24,  7, 255} :
               is_hovered  ? (Color){28, 18,  5, 255} :
                             (Color){15,  9,  3, 255};
    DrawRectangleRounded(*r, rnd, 6, bg);

    Color border = is_selected ? col :
                   is_hovered  ? (Color){col.r/2, col.g/2, col.b/2, 200} :
                                 (Color){45, 30, 12, 180};
    float bw = is_selected ? 2.0f : 1.0f;
    DrawRectangleRoundedLinesEx(*r, rnd, 6, bw, border);

    if (!can_afford) col = (Color){65, 50, 32, 255};

    // Splash art si disponible, icône texte sinon

    /* ── Bouton verrouillé : overlay sombre + cadenas ─────────── */
    if (is_locked) {
        DrawRectangleRounded(*r, rnd, 6, (Color){6, 4, 2, 240});
        DrawRectangleRoundedLinesEx(*r, rnd, 6, 1.0f, (Color){40, 28, 10, 160});
        // Symbole cadenas centré
        int cx2 = (int)(r->x + r->width  / 2);
        int cy2 = (int)(r->y + r->height / 2);
        DrawRectangle(cx2 - 7, cy2 - 2, 14, 10, (Color){55, 40, 15, 220});
        DrawRectangleLines(cx2 - 7, cy2 - 2, 14, 10, (Color){90, 65, 20, 255});
        DrawCircleLines(cx2, cy2 - 6, 6, (Color){90, 65, 20, 255});
        // Nom grisé en bas
        int nw2 = mtxt(info->shortname, 9);
        dtxt(info->shortname,
                 cx2 - nw2/2,
                 (int)(r->y + r->height - 13), 9,
                 (Color){55, 42, 22, 255});
        return;
    }

    Texture2D splash = {0};
    if (ui_tool_is_tower(id))
        splash = g_tower_splash[ui_tool_to_tower(id)];
    else if (ui_tool_is_unit(id))
        splash = g_unit_splash[ui_tool_to_unit(id)];

    int name_fs = 9;
    int name_y, cost_y;

    if (splash.id != 0) {
        // Image mise à l'échelle pour remplir le bouton (marge 2px)
        int pad = 2;
        float scale = fminf((float)((int)r->width  - pad*2) / (float)splash.width,
                            (float)((int)r->height - pad*2) / (float)splash.height);
        int dw = (int)(splash.width  * scale);
        int dh = (int)(splash.height * scale);
        int ox = (int)r->x + ((int)r->width  - dw) / 2;
        int oy = (int)r->y + ((int)r->height - dh) / 2;
        Color tint = can_afford ? WHITE : (Color){120, 100, 80, 200};
        DrawTexturePro(splash,
            (Rectangle){0, 0, (float)splash.width, (float)splash.height},
            (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh},
            (Vector2){0, 0}, 0.0f, tint);

        // Bande sombre en bas pour lisibilité du texte (hauteur dynamique)
        int band_h = (int)(r->height * 0.38f);
        if (band_h < 20) band_h = 20;
        DrawRectangle((int)r->x, (int)(r->y + r->height) - band_h,
                      (int)r->width, band_h, (Color){0, 0, 0, 175});

        name_y = (int)(r->y + r->height) - band_h + 2;
        cost_y = (int)(r->y + r->height) - band_h/2 + 1;
    } else {
        // Fallback : icône texte
        int icon_fs = 20;
        int iw = mtxt(info->icon, icon_fs);
        dtxt(info->icon,
                 (int)(r->x + r->width/2 - iw/2),
                 (int)(r->y + 5), icon_fs, col);
        name_y = (int)(r->y + 29);
        cost_y = (int)(r->y + 41);
    }

    // Nom abrégé
    int nw = mtxt(info->shortname, name_fs);
    dtxt(info->shortname,
             (int)(r->x + r->width/2 - nw/2),
             name_y, name_fs,
             can_afford ? (Color){200, 185, 160, 255}
                        : (Color){75, 58, 38, 255});

    // Coût
    char cost_buf[20];
    snprintf(cost_buf, sizeof(cost_buf), "%d or", info->cost);
    int cw = mtxt(cost_buf, name_fs);
    dtxt(cost_buf,
             (int)(r->x + r->width/2 - cw/2),
             cost_y, name_fs,
             can_afford ? (Color){230, 150, 32, 255}
                        : (Color){130, 55, 35, 255});

    if (is_selected)
        DrawRectangleRoundedLinesEx(
            (Rectangle){r->x-2, r->y-2, r->width+4, r->height+4},
            rnd, 6, 1.0f,
            (Color){col.r, col.g, col.b, 60});
}

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

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    const int VIRT_W = g_canvas_virt_w;
    const int HUD_Y  = g_canvas_virt_h - UI_HUD_HEIGHT;
    const int HUD_H  = UI_HUD_HEIGHT;
    const int M      = UI_MARGIN;
    const int GAP    = 6;

    const Theme *th = theme_get(gs->map.theme);

    // ── Fond HUD — plaque métal militaire ─────────────────────
    DrawRectangle(0, HUD_Y, VIRT_W, HUD_H, (Color){12, 8, 4, 255});
    // Ombrage interne (haut + bas) pour le volume
    DrawRectangle(0, HUD_Y + 2, VIRT_W, 16, (Color){0, 0, 0, 55});
    DrawRectangle(0, HUD_Y + HUD_H - 8, VIRT_W, 8, (Color){0, 0, 0, 70});
    // Filet d'accent supérieur (2 px) + ligne sombre dessous
    DrawRectangle(0, HUD_Y, VIRT_W, 2, (Color){150, 96, 20, 255});
    DrawLine(0, HUD_Y + 2, VIRT_W, HUD_Y + 2, (Color){70, 44, 0, 220});
    // Rivets le long de l'arête supérieure
    for (int rvx = 14; rvx < VIRT_W - 8; rvx += 48)
        DrawRectangle(rvx, HUD_Y + 5, 2, 2, (Color){95, 70, 26, 200});

    // Panneaux latéraux (légèrement détachés) + séparateurs d'accent
    DrawRectangle(0,                   HUD_Y, UI_LEFT_PANEL_W, HUD_H, (Color){17, 10, 4, 235});
    DrawRectangle(VIRT_W - UI_PANEL_W, HUD_Y, UI_PANEL_W,      HUD_H, (Color){17, 10, 4, 235});
    DrawRectangle(UI_LEFT_PANEL_W - 1,     HUD_Y + 4, 2, HUD_H - 8, (Color){62, 42, 12, 190});
    DrawRectangle(VIRT_W - UI_PANEL_W - 1, HUD_Y + 4, 2, HUD_H - 8, (Color){62, 42, 12, 190});

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
            draw_bar(bar_x, py + 2, bar_w, 7, sr,
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
            clip_text(th->name, UI_LEFT_PANEL_W - M * 2, 9, tbuf, sizeof(tbuf));
            dtxt(tbuf, px, py, 9, (Color){48, 82, 48, 255});
            py += 12;
        }

        // Inventaire
        if (gs->inventory_count > 0) {
            dtxt(TextFormat("Mat: %d", gs->inventory_count),
                     px, py, 9, (Color){62, 165, 185, 255});
        }

        // ── Achats de slots supplémentaires (bas du panneau gauche) ──
        {
            const int bw = UI_LEFT_PANEL_W - M * 2 - 4;

            /* Séparateur + titre */
            draw_sep(px, HUD_Y + HUD_H - 59, bw,
                     (Color){40, 25, 6, 110});
            dtxt("SLOTS", px, HUD_Y + HUD_H - 75, 9,
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
                    snprintf(slot_bufs[1], sizeof(slot_bufs[1]), "+1 Unite  [MAX]");
                else
                    snprintf(slot_bufs[1], sizeof(slot_bufs[1]),
                             "+1 Unite  %d or", SLOT_UNIT_COSTS[_b]);
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

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons outils
    // ════════════════════════════════════════════════
    {
        /* lx suit la position X dynamique du premier bouton (centré dans la zone) */
        int lx    = (int)ui->tool_btns[TOOL_TOWER_GUN].x;
        int row1y = (int)ui->tool_btns[TOOL_TOWER_GUN].y;
        int row2y = (int)ui->tool_btns[TOOL_UNIT_SOLDIER].y;

        dtxt("TOURS",  lx, row1y - 15, 9, (Color){100, 70, 22, 255});
        dtxt("UNITES", lx, row2y - 15, 9, (Color){38, 100, 38, 255});

        for (int i = 0; i < TOOL_COUNT; i++) {
            int locked     = !tool_is_unlocked((ToolID)i, gs);
            int can_afford = !locked && gs->gold >= TOOL_INFO[i].cost;

            // Griser si limite atteinte
            if (!locked && ui_tool_is_tower((ToolID)i) &&
                gs->towers.tower_count >= gs->towers.tower_limit)
                can_afford = 0;
            if (!locked && ui_tool_is_unit((ToolID)i) &&
                gs->units.count >= gs->units.unit_limit)
                can_afford = 0;

            draw_tool_btn(&ui->tool_btns[i], (ToolID)i,
                          ui->selected_tool == i,
                          ui->hovered_tool  == i,
                          can_afford, locked);
        }
    }

    // ════════════════════════════════════════════════
    // BOUTON LANCER VAGUE
    // ════════════════════════════════════════════════
    {
        const Rectangle *wb = &ui->wave_btn;
        int   in_prep = (gs->phase == PHASE_PREP);
        float ratio   = gs->wave_manager.prep_timer / 20.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        float wrnd = (float)UI_RADIUS / wb->height;
        Color wbg  = in_prep ? (Color){5, 20, 8, 255} : (Color){7, 7, 7, 255};
        Color wbrd = in_prep ? (Color){22, 72, 32, 255} : (Color){30, 30, 30, 255};
        DrawRectangleRounded(*wb, wrnd, 6, wbg);
        DrawRectangleRoundedLinesEx(*wb, wrnd, 6, 1.5f, wbrd);

        Color tcol = ratio > 0.5f ? (Color){46,204,113,255}
                   : ratio > 0.2f ? (Color){243,156,18,255}
                                  : (Color){231,76,60,255};
        draw_bar((int)wb->x + GAP,
                 (int)wb->y + (int)wb->height - 12,
                 (int)wb->width - GAP*2, 5,
                 ratio, tcol, (Color){16,16,16,255});

        int wx = (int)(wb->x + wb->width/2);
        Color wlbl = in_prep ? (Color){42,188,105,255} : (Color){58,58,58,255};

        if (in_prep) {
            const char *l1 = "LANCER";
            dtxt(l1, wx - mtxt(l1,13)/2, (int)wb->y + M, 13, wlbl);

            char b2[14];
            snprintf(b2, sizeof(b2), "+%d or", (int)(ratio*15.0f));
            dtxt(b2, wx - mtxt(b2,10)/2, (int)wb->y+27, 10,
                     (Color){225,145,28,255});

            char b3[10];
            snprintf(b3, sizeof(b3), "%.0fs", gs->wave_manager.prep_timer);
            dtxt(b3, wx - mtxt(b3,10)/2, (int)wb->y+40, 10,
                     (Color){82,65,40,255});
        } else {
            const char *l1 = "EN COURS";
            dtxt(l1, wx - mtxt(l1,11)/2,
                     (int)(wb->y + wb->height/2 - 8), 11, wlbl);
        }
    }

    // ════════════════════════════════════════════════
    // BOUTON PAUSE
    // ════════════════════════════════════════════════
    {
        const Rectangle *pb  = &ui->pause_btn;
        float prnd = (float)UI_RADIUS / pb->height;
        int phov = CheckCollisionPointRec(virt_mouse(), *pb);
        DrawRectangleRounded(*pb, prnd, 6,
            phov ? (Color){28, 22, 8, 255} : (Color){14, 10, 3, 255});
        DrawRectangleRoundedLinesEx(*pb, prnd, 6, 1.5f,
            phov ? (Color){140, 105, 40, 255} : (Color){60, 44, 14, 180});
        // Deux barres verticales (icône pause)
        int pcx = (int)(pb->x + pb->width  / 2);
        int pcy = (int)(pb->y + pb->height / 2);
        DrawRectangle(pcx - 7, pcy - 9, 5, 18, (Color){160, 120, 45, 255});
        DrawRectangle(pcx + 2, pcy - 9, 5, 18, (Color){160, 120, 45, 255});
    }

    // ════════════════════════════════════════════════
    // PANNEAU DROIT — sélection tour / outil / unité
    // ════════════════════════════════════════════════
    {
        const int rx    = VIRT_W - UI_PANEL_W + M;
        const int max_w = UI_PANEL_W - M * 2;
        int py = HUD_Y + M;
        char buf[48];

        if (ui->selection.active) {
            const Tower *tw = &gs->towers.towers[ui->selection.tower_idx];
            if (!tw->active) goto panel_right_empty;

            const TowerStats *st = &TOWER_BASE_STATS[tw->type];
            Color col = TOWER_FILL[tw->type];

            // Portrait splash
            int pdone = draw_portrait(g_tower_splash[tw->type],
                                      rx + max_w - PORTRAIT_SZ, HUD_Y + M, PORTRAIT_SZ, col);
            int text_w = pdone ? max_w - PORTRAIT_SZ - 6 : max_w;

            // Nom (fs=16)
            clip_text(st->name, text_w, 16, buf, sizeof(buf));
            dtxt(buf, rx, py, 16, col);
            py += fh(16) + 4;
            /* Trait s'arrête avant le portrait (text_w = zone de texte sans le splash) */
            DrawLine(rx, py, rx + text_w, py, (Color){44, 28, 8, 140});
            py += 7;

            // Stats — 2 colonnes (fs=13)
            Color sc   = (Color){148,128,95,255};
            Color nlc  = (Color){212,138,25,255};
            Color typc = (Color){82,155,200,255};
            Color matc = (Color){62,172,192,255};
            int   cx2  = rx + max_w / 2;

            dtxt(TextFormat("Dmg   %.0f",   tw->damage),
                     rx,  py, 13, sc);
            dtxt(TextFormat("Port  %.1ft",  tw->range),
                     cx2, py, 13, sc);   py += 17;

            dtxt(TextFormat("Cad   %.1f/s", tw->fire_rate),
                     rx,  py, 13, sc);
            dtxt(TextFormat("Niv   %d",     tw->level),
                     cx2, py, 13, nlc);  py += 17;

            dtxt(TextFormat("Type  %s", DAMAGE_NAMES[tw->dmg_type]),
                     rx, py, 13, typc); py += 17;

            if (tw->material != MAT_NONE) {
                clip_text(TextFormat("Mat: %s", MATERIAL_NAMES[tw->material]),
                          max_w, 13, buf, sizeof(buf));
                dtxt(buf, rx, py, 13, matc);
                py += 16;
            }

            /* ── Boutons d'amélioration (sous le portrait) ─ */
            {
                const int GAP2 = 6;
                const int ubw  = (max_w - GAP2 * 2) / 3;
                int uy = HUD_Y + M + PORTRAIT_SZ + GAP2;

                DrawLine(rx, uy - 3, rx + max_w, uy - 3, (Color){40, 25, 6, 120});

                /* Données des 3 boutons */
                typedef struct { const char *lbl; int cost; int lvl; Color col; } UB;
                UB ubs[3] = {
                    { "DEGATS", tower_upg_next_cost_dmg  (tw), tw->upg_dmg,   {231, 100,  60, 255} },
                    { "PORTEE", tower_upg_next_cost_range(tw), tw->upg_range,  { 82, 155, 200, 255} },
                    { "CADENCE",tower_upg_next_cost_rate (tw), tw->upg_rate,   {155,  89, 182, 255} },
                };
                const Rectangle *upg_rects[3] = {
                    &ui->upg_dmg_btn, &ui->upg_range_btn, &ui->upg_rate_btn
                };
                Vector2 vm2 = virt_mouse();
                for (int _u = 0; _u < 3; _u++) {
                    const Rectangle *ur = upg_rects[_u];
                    int _hov    = CheckCollisionPointRec(vm2, *ur);
                    int _is_max = (ubs[_u].cost < 0);
                    int _afford = !_is_max && (gs->gold >= ubs[_u].cost);
                    Color _bg   = _is_max ? (Color){6, 6, 3, 220}
                                : _hov    ? (Color){20, 12, 4, 235}
                                          : (Color){10, 6, 2, 220};
                    Color _brd  = _is_max ? (Color){50, 40, 15, 120}
                                : _afford ? ubs[_u].col
                                          : (Color){60, 45, 15, 140};
                    DrawRectangleRounded(*ur, 0.15f, 4, _bg);
                    DrawRectangleRoundedLinesEx(*ur, 0.15f, 4, 1.2f, _brd);

                    /* Ligne 1 : étiquette type (y+2, fs=9) */
                    char _ltxt[12];
                    snprintf(_ltxt, sizeof(_ltxt), "%s", ubs[_u].lbl);
                    int _lw = mtxt(_ltxt, 9);
                    Color _lc = _is_max ? (Color){50, 40, 15, 180} : ubs[_u].col;
                    dtxt(_ltxt,
                         (int)(ur->x + ur->width/2 - _lw/2),
                         (int)(ur->y + 2), 9, _lc);

                    if (_is_max) {
                        /* MAX sur 2 lignes fusionnées */
                        int _mw = mtxt("MAX", 9);
                        dtxt("MAX",
                             (int)(ur->x + ur->width/2 - _mw/2),
                             (int)(ur->y + 18), 9,
                             (Color){100, 80, 25, 200});
                    } else {
                        /* Ligne 2 : niveau X/5 (y+16, fs=8) */
                        char _lvltxt[10];
                        snprintf(_lvltxt, sizeof(_lvltxt), "niv %d/%d",
                                 ubs[_u].lvl, TOWER_UPG_MAX);
                        int _nlw = mtxt(_lvltxt, 8);
                        dtxt(_lvltxt,
                             (int)(ur->x + ur->width/2 - _nlw/2),
                             (int)(ur->y + 16), 8,
                             (Color){148, 128, 95, 220});

                        /* Ligne 3 : coût (y+27, fs=8) */
                        char _ctxt[16];
                        snprintf(_ctxt, sizeof(_ctxt), "%d or", ubs[_u].cost);
                        int _cw = mtxt(_ctxt, 8);
                        dtxt(_ctxt,
                             (int)(ur->x + ur->width/2 - _cw/2),
                             (int)(ur->y + 27), 8,
                             _afford ? (Color){220, 145, 30, 255}
                                     : (Color){100, 55, 25, 200});
                    }
                }
                (void)ubw; (void)uy;
            }

            /* ── Bouton RÉPARER TOUR + bouton VENDRE ─────────── */
            {
                /* Réparer */
                Rectangle rb   = ui->repair_tower_btn;
                float     rrnd = (float)UI_RADIUS / rb.height;
                Vector2   vm3  = virt_mouse();
                int _hov_r = CheckCollisionPointRec(vm3, rb);
                int _needs_rep = (tw->hp < TOWER_MAX_HP - 0.5f);
                int _can_rep   = _needs_rep && (gs->gold >= TOWER_REPAIR_COST);
                Color _rbg = _can_rep  ? (_hov_r ? (Color){3, 20, 8, 235} : (Color){3, 12, 6, 210})
                                       : (Color){8, 6, 3, 190};
                Color _rbd = _can_rep  ? (Color){28, 130, 55, 200}
                                       : (Color){45, 35, 12, 120};
                DrawRectangleRounded(rb, rrnd, 4, _rbg);
                DrawRectangleRoundedLinesEx(rb, rrnd, 4, 1.2f, _rbd);
                char _rptxt[24];
                snprintf(_rptxt, sizeof(_rptxt), "Rep. %d or", TOWER_REPAIR_COST);
                int _rpw = mtxt(_rptxt, 10);
                Color _rpc = _can_rep   ? (Color){42, 185, 90, 255}
                           : _needs_rep ? (Color){55, 42, 22, 200}
                                        : (Color){40, 32, 12, 160};
                dtxt(_rptxt,
                     (int)(rb.x + rb.width/2 - _rpw/2),
                     (int)(rb.y + rb.height/2 - 6), 10, _rpc);
                if (!_needs_rep) {
                    int _okw = mtxt("OK", 7);
                    dtxt("OK",
                         (int)(rb.x + rb.width/2 - _okw/2),
                         (int)(rb.y + rb.height - 9), 7,
                         (Color){42, 185, 90, 170});
                }
            }

            // Bouton VENDRE
            {
                Rectangle sb   = ui->sell_btn;
                float     srnd = (float)UI_RADIUS / sb.height;
                Vector2   m    = virt_mouse();
                int hov = CheckCollisionPointRec(m, sb);
                DrawRectangleRounded(sb, srnd, 6,
                    hov ? (Color){48,8,8,255} : (Color){20,4,4,255});
                DrawRectangleRoundedLinesEx(sb, srnd, 6, 1.5f,
                    hov ? (Color){192,48,48,255} : (Color){90,18,18,255});
                int refund = (int)(tower_cost_on_tile(tw->type, &gs->map,
                                       tw->tile_x, tw->tile_y) * TOWER_SELL_REFUND);
                snprintf(buf, sizeof(buf), "Vendre  +%d or", refund);
                int bw = mtxt(buf, 13);
                dtxt(buf, (int)(sb.x + sb.width/2 - bw/2),
                         (int)(sb.y + sb.height/2 - 7),
                         13, (Color){192,58,42,255});
            }

            // Bouton APPLIQUER MATÉRIAU
            if (ui->apply_mat_visible && gs->inventory_count > 0) {
                /* Borne de sécurité sur l'index sélectionné */
                int _sidx = ui->sel_mat_idx;
                if (_sidx < 0 || _sidx >= gs->inventory_count) _sidx = 0;
                MaterialType mat = gs->inventory[_sidx];

                Rectangle ab   = ui->apply_mat_btn;
                float     arnd = (float)UI_RADIUS / ab.height;
                Vector2   mv   = virt_mouse();
                int hov = CheckCollisionPointRec(mv, ab);
                DrawRectangleRounded(ab, arnd, 6,
                    hov ? (Color){4,26,36,255} : (Color){3,14,20,255});
                DrawRectangleRoundedLinesEx(ab, arnd, 6, 1.5f,
                    hov ? (Color){55,165,195,255} : (Color){24,82,100,255});

                /* Label : matériau + index si plusieurs en inventaire */
                if (gs->inventory_count > 1) {
                    /* Flèches de cycle */
                    dtxt("<", (int)(ab.x + 5),
                         (int)(ab.y + ab.height/2 - 8), 14,
                         (Color){62,175,200,200});
                    dtxt(">", (int)(ab.x + ab.width - 12),
                         (int)(ab.y + ab.height/2 - 8), 14,
                         (Color){62,175,200,200});
                    clip_text(TextFormat("[%d/%d] %s",
                                        _sidx + 1, gs->inventory_count,
                                        MATERIAL_NAMES[mat]),
                              max_w - 40, 11, buf, sizeof(buf));
                    int bw2 = mtxt(buf, 11);
                    dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                             (int)(ab.y + ab.height/2 - 7),
                             11, (Color){62,175,200,255});
                } else {
                    clip_text(TextFormat("+ Appliquer  %s", MATERIAL_NAMES[mat]),
                              max_w - M, 13, buf, sizeof(buf));
                    int bw2 = mtxt(buf, 13);
                    dtxt(buf, (int)(ab.x + ab.width/2 - bw2/2),
                             (int)(ab.y + ab.height/2 - 7),
                             13, (Color){62,175,200,255});
                }
            }

        } else if (ui->sell_unit_idx >= 0) {
            const Unit *u = &gs->units.units[ui->sell_unit_idx];
            if (!u->active) goto panel_right_empty;

            // Nom et couleur selon le type
            static const char *UNAMES[UNIT_TYPE_COUNT] = {
                "SOLDAT", "LOURD", "MEDIC", "CHIEN", "OUVRIER"
            };
            static const Color UCOLS[UNIT_TYPE_COUNT] = {
                {39,174,96,255},{41,128,185,255},{231,76,60,255},
                {243,156,18,255},{200,200,50,255}
            };
            Color ucol = (u->type < UNIT_TYPE_COUNT)
                       ? UCOLS[u->type] : (Color){148,128,95,255};
            const char *uname = (u->type < UNIT_TYPE_COUNT)
                              ? UNAMES[u->type] : "UNITE";

            dtxt(uname, rx, py, 16, ucol); py += fh(16) + 4;
            DrawLine(rx, py, rx+max_w, py, (Color){44,44,8,140}); py += 7;

            // HP bar
            float hr = u->max_hp > 0.0f ? u->hp / u->max_hp : 0.0f;
            Color hc = hr > 0.6f ? (Color){46,204,113,255}
                     : hr > 0.3f ? (Color){243,156,18,255}
                                 : (Color){231,76,60,255};
            snprintf(buf, sizeof(buf), "HP   %.0f / %.0f", u->hp, u->max_hp);
            dtxt(buf, rx, py, 13, hc); py += 14;
            draw_bar(rx, py, max_w, 6, fmaxf(hr, 0.0f), hc, (Color){16,16,16,200});
            py += 13;

            // État
            if (u->type == UNIT_WORKER) {
                const char *ss;
                switch (u->state) {
                    case USTATE_GOTO_DEPOSIT: ss = "-> Depot";    break;
                    case USTATE_COLLECT:      ss = "Collecte..."; break;
                    case USTATE_GOTO_BASE:    ss = "<- Base";     break;
                    default:                  ss = "En attente";  break;
                }
                dtxt(ss, rx, py, 13, (Color){148,128,95,255}); py += 17;
                if (!gs->units.mining_enabled)
                    dtxt("Minage : pause vague", rx, py, 11,
                         (Color){200,180,50,200});
                else if (u->state == USTATE_COLLECT) {
                    // Indicateur de ralentissement par ennemis
                    for (int _ej = 0; _ej < MAX_ENEMIES; _ej++) {
                        const Enemy *_e = &gs->enemies.enemies[_ej];
                        if (!_e->active || _e->dead || _e->spawn_delay > 0.0f) continue;
                        float _dx = _e->x - u->x, _dy = _e->y - u->y;
                        float _d2 = UNIT_WORKER_ENEMY_SLOW_RANGE * TILE_SIZE;
                        if (_dx*_dx + _dy*_dy <= _d2*_d2) {
                            dtxt("! Ennemi proche: lent", rx, py, 11,
                                 (Color){231,76,60,220});
                            break;
                        }
                    }
                }
            } else {
                // Unité de combat : état + comportement
                static const char *BNAMES[5] = {
                    "Patrouille", "Garde tourelle", "Escorte", "Manuel", "Suit unite"
                };
                static const Color BCOLS[5] = {
                    {148,128,95,255},{192,57,43,255},{200,200,50,255},
                    {82,155,200,255},{231,76,60,255}
                };
                int beh = (int)u->behavior;
                if (beh < 0 || beh > 4) beh = 0;
                dtxt(BNAMES[beh], rx, py, 13, BCOLS[beh]); py += 17;
            }

            if (u->has_material && u->carried_mat != MAT_NONE) {
                dtxt(TextFormat("Porte  %s", MATERIAL_NAMES[u->carried_mat]),
                         rx, py, 13, (Color){62,175,200,255}); py += 17;
            }

            if (u->type == UNIT_WORKER)
                dtxt("Clic depot = mission", rx, py, 11, (Color){92,92,35,175});

            // ── Boutons de comportement (unités de combat seulement) ──
            if (u->type != UNIT_WORKER) {
                static const char *BLBL[4] = {"PATROL","GARDE","ESCORT","MANUEL"};
                static const Color BCOL[4] = {
                    {100,85,55,255},{192,57,43,255},{200,200,50,255},{82,155,200,255}
                };
                Vector2 _vm4 = virt_mouse();
                for (int _b = 0; _b < 4; _b++) {
                    const Rectangle *_br = &ui->unit_beh_btns[_b];
                    int _active = ((int)u->behavior == _b);
                    int _hov    = CheckCollisionPointRec(_vm4, *_br);
                    Color _bg   = _active ? (Color){20,12,4,240}
                                : _hov   ? (Color){14,8,2,220}
                                         : (Color){8,5,2,200};
                    Color _brd  = _active ? BCOL[_b] : _hov
                                ? (Color){BCOL[_b].r/2,BCOL[_b].g/2,BCOL[_b].b/2,200}
                                : (Color){50,38,14,150};
                    DrawRectangleRounded(*_br, 0.20f, 4, _bg);
                    DrawRectangleRoundedLinesEx(*_br, 0.20f, 4,
                        _active ? 2.0f : 1.0f, _brd);
                    int _tw4 = mtxt(BLBL[_b], 8);
                    dtxt(BLBL[_b],
                         (int)(_br->x + _br->width/2 - _tw4/2),
                         (int)(_br->y + _br->height/2 - 5),
                         8, _active ? BCOL[_b] : (Color){110,90,55,220});
                }
                /* 5e bouton : SUIVRE (médic uniquement) */
                if (u->type == UNIT_MEDIC) {
                    const Rectangle *_br5 = &ui->unit_beh_btns[4];
                    int   _act5 = (u->behavior == UBEH_FOLLOW_UNIT);
                    int   _hov5 = CheckCollisionPointRec(_vm4, *_br5);
                    Color _sc5  = {231, 76, 60, 255};
                    Color _bg5  = _act5 ? (Color){20,12,4,240}
                                : _hov5 ? (Color){14,8,2,220}
                                        : (Color){8,5,2,200};
                    Color _brd5 = _act5 ? _sc5
                                : _hov5 ? (Color){_sc5.r/2,_sc5.g/2,_sc5.b/2,200}
                                        : (Color){50,38,14,150};
                    DrawRectangleRounded(*_br5, 0.20f, 4, _bg5);
                    DrawRectangleRoundedLinesEx(*_br5, 0.20f, 4,
                        _act5 ? 2.0f : 1.0f, _brd5);
                    const char *_lbl5 = "SUIVRE UNITE";
                    int _tw5 = mtxt(_lbl5, 8);
                    dtxt(_lbl5,
                         (int)(_br5->x + _br5->width/2 - _tw5/2),
                         (int)(_br5->y + _br5->height/2 - 5),
                         8, _act5 ? _sc5 : (Color){110,90,55,220});
                }
            }

            // Bouton RENVOYER
            {
                Rectangle ub   = ui->unit_sell_btn;
                float     urnd = (float)UI_RADIUS / ub.height;
                Vector2   m    = virt_mouse();
                int hov = CheckCollisionPointRec(m, ub);
                DrawRectangleRounded(ub, urnd, 6,
                    hov ? (Color){30,10,4,255} : (Color){14,4,2,255});
                DrawRectangleRoundedLinesEx(ub, urnd, 6, 1.5f,
                    hov ? (Color){192,80,48,255} : (Color){80,30,16,255});
                int refund = (int)(UNIT_BASE_STATS[u->type].cost * 0.5f);
                snprintf(buf, sizeof(buf), "Renvoyer  +%d or", refund);
                int bw = mtxt(buf, 13);
                dtxt(buf, (int)(ub.x + ub.width/2 - bw/2),
                         (int)(ub.y + ub.height/2 - 7),
                         13, (Color){192,90,58,255});
            }

        } else if (ui->selected_tool != TOOL_NONE) {
            const ToolInfo *info = &TOOL_INFO[ui->selected_tool];
            Color col = TOOL_COLORS[ui->selected_tool];

            // Portrait splash
            Texture2D ptex = ui_tool_is_tower(ui->selected_tool)
                ? g_tower_splash[ui_tool_to_tower(ui->selected_tool)]
                : g_unit_splash [ui_tool_to_unit (ui->selected_tool)];
            int pdone = draw_portrait(ptex, rx + max_w - PORTRAIT_SZ, HUD_Y + M, PORTRAIT_SZ, col);
            int text_w = pdone ? max_w - PORTRAIT_SZ - 6 : max_w;

            clip_text(info->name, text_w, 16, buf, sizeof(buf));
            dtxt(buf, rx, py, 16, col);
            py += 26;
            DrawLine(rx, py, rx+max_w, py, (Color){44,28,8,140});
            py += 7;

            // Stats 2 colonnes (fs=13)
            Color sc2 = (Color){148,128,95,255};
            int   cx2 = rx + max_w / 2;

            dtxt(TextFormat("Dmg   %.0f",  info->dmg),
                     rx,  py, 13, sc2);
            dtxt(TextFormat("Port  %.1ft", info->range),
                     cx2, py, 13, sc2);  py += 17;
            dtxt(TextFormat("Cad   %.1f/s",info->rate),
                     rx,  py, 13, sc2);  py += 17;

            if (ui_tool_is_tower(ui->selected_tool) &&
                ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
                TowerType tt = ui_tool_to_tower(ui->selected_tool);
                int real_cost = tower_cost_on_tile(tt, &gs->map,
                                    ui->hovered_tile_x,
                                    ui->hovered_tile_y);
                int is_ruin = (gs->map.tiles[ui->hovered_tile_y]
                                            [ui->hovered_tile_x].type
                               == TILE_RUIN);
                if (is_ruin) {
                    dtxt(TextFormat("Cout  %d or (x2)", real_cost),
                             rx, py, 13, (Color){205,108,22,255}); py += 17;
                } else {
                    dtxt(TextFormat("Cout  %d or", real_cost),
                             rx, py, 13, (Color){212,138,25,255}); py += 17;
                }
            } else {
                dtxt(TextFormat("Cout  %d or", info->cost),
                         rx, py, 13, (Color){212,138,25,255}); py += 17;
            }
            py += 3;
            clip_text(info->desc, max_w, 12, buf, sizeof(buf));
            dtxt(buf, rx, py, 12, (Color){82,65,40,255});

        } else {
            panel_right_empty:
            dtxt("Clic sur",    rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("un outil",    rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("ou une tour", rx, py, 13, (Color){50,40,25,255}); py += 17;
            dtxt("posee.",      rx, py, 13, (Color){50,40,25,255});
        }
    }

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

            // Or
            char gbuf[20];
            if (gs->gold >= 10000)
                snprintf(gbuf, sizeof(gbuf), "%dk", gs->gold / 1000);
            else
                snprintf(gbuf, sizeof(gbuf), "%d", gs->gold);
            draw_icon(g_icon_gold, tx, ty, fh(12), WHITE);
            dtxt(gbuf, tx + fh(12) + 2, ty, 12, (Color){230, 155, 35, 255});
            ty += line1_h + 3;

            // Vies — couleur et pulsation selon niveau critique
            {
                float _tp = (float)GetTime();
                float _pp = (sinf(_tp * 5.0f) + 1.0f) * 0.5f;
                Color lc;
                if (gs->lives > 40) {
                    lc = (Color){46, 204, 113, 255};
                } else if (gs->lives > 15) {
                    lc = (Color){243, 156, 18, 255};
                } else {
                    unsigned char _lcr = (unsigned char)(160 + (int)(71.0f * _pp));
                    unsigned char _lcg = (unsigned char)(30  + (int)(46.0f * _pp));
                    lc = (Color){_lcr, _lcg, 40, 255};
                }
                draw_icon(g_icon_heart, tx, ty, fh(12), lc);
                dtxt(TextFormat("%d", gs->lives),
                     tx + fh(12) + 2, ty, 12, lc);
            }
            ty += line1_h + 3;

            // Tours
            {
                Color tc = gs->towers.tower_count >= gs->towers.tower_limit
                    ? (Color){231, 76, 60, 255} : (Color){148, 128, 95, 255};
                dtxt(TextFormat("Tours  %d / %d",
                             gs->towers.tower_count, gs->towers.tower_limit),
                         tx, ty, 10, tc);
                ty += line2_h + 3;
            }

            // Unités
            {
                Color uc = gs->units.count >= gs->units.unit_limit
                    ? (Color){231, 76, 60, 255} : (Color){148, 128, 95, 255};
                dtxt(TextFormat("Unites %d / %d",
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
                               (Color){231, 76, 60, 255});    // accent rouge = menace

            int tx = ox + OV_P, ty = oy + OV_P + 4;
            int inner_w = ow - OV_P * 2;

            // Vague (X/min en campagne pour indiquer la progression)
            {
                char _wbuf[28];
                if (gs->is_campaign) {
                    const ActData *_cad = campaign_act_get(gs->campaign_stage);
                    int _mw = _cad ? _cad->min_waves : 0;
                    snprintf(_wbuf, sizeof(_wbuf), "Vague  %d / %d",
                             gs->wave_manager.number, _mw);
                } else {
                    snprintf(_wbuf, sizeof(_wbuf), "Vague  %d",
                             gs->wave_manager.number);
                }
                dtxt(_wbuf, tx, ty, 12, (Color){185, 145, 60, 255});
            }
            ty += 17 + 3;

            // Kills
            dtxt(TextFormat("Kills  %d", gs->kills),
                     tx, ty, 10, (Color){148, 128, 95, 255});
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
                Color pc = prog > 0.7f ? (Color){46, 204, 113, 255}
                         : prog > 0.3f ? (Color){243, 156,  18, 255}
                                       : (Color){218,  90,  70, 255};
                draw_bar(tx, ty, inner_w, 7,
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
                int nw = mtxt(nxt, 9);
                dtxt(nxt, (int)sbr.x + bw/2 - nw/2,
                     (int)sbr.y + bh/2 - fh(9)/2, 9,
                     bhov ? sc[ui->speed_mult % 3] : (Color){120, 90, 45, 220});
            }
        }
    }

        // ── Bas-gauche : HP des bases + réparation ─────────────
        if (ui->overlay_bl_pos.x >= 0.0f) {
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

                /* Code couleur */
                Color bc;
                if (!base->active || base->hp <= 0) {
                    bc = (Color){100, 35, 35, 255};
                } else if (ratio > 0.60f) {
                    bc = (Color){46, 204, 113, 255};
                } else if (ratio > 0.30f) {
                    bc = (Color){243, 156,  18, 255};
                } else {
                    bc = (Color){231,  76,  60, 255};
                }
                if (ratio > 0.0f && ratio <= 0.30f && base->active) {
                    unsigned char _cr = (unsigned char)(140 + (int)(91.0f * _pu));
                    unsigned char _cg = (unsigned char)(20  + (int)(56.0f * _pu));
                    bc = (Color){_cr, _cg, 40, 255};
                }

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

                draw_bar(_px, _py + 10, _iw, 6, fmaxf(ratio, 0.0f), bc,
                         (Color){18, 10, 4, 200});

                if (!base->active || base->hp <= 0)
                    dtxt("DETRUITE", _px + _iw/2 - 22, _py + 10, 7,
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
                    snprintf(rbuf2, sizeof(rbuf2), "Rep. +%dHP  %d or",
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

    // ════════════════════════════════════════════════
    // OVERLAYS DANS L'ESPACE DE LA CARTE
    // ════════════════════════════════════════════════
    {
        Camera2D map_cam = {0};
        map_cam.offset = (Vector2){(float)g_map_x_off, 0.0f};
        map_cam.zoom   = g_map_render_scale;
        BeginMode2D(map_cam);

        // Prévisualisation placement
        if (ui->selected_tool != TOOL_NONE &&
            ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0) {
            if (ui_tool_is_tower(ui->selected_tool)) {
                render_tower_preview(&gs->map, &gs->towers,
                                     ui_tool_to_tower(ui->selected_tool),
                                     ui->hovered_tile_x,
                                     ui->hovered_tile_y);
            } else {
                for (int b = 0; b < gs->map.base_count; b++) {
                    if (!gs->map.bases[b].active) continue;
                    float bpx = gs->map.bases[b].pos.x * TILE_SIZE + TILE_SIZE/2.0f;
                    float bpy = gs->map.bases[b].pos.y * TILE_SIZE + TILE_SIZE/2.0f;
                    DrawCircleLines((int)bpx, (int)bpy,
                                    5.0f * TILE_SIZE,
                                    (Color){39, 174, 96, 100});
                }
            }
        }

        // Highlight dépôts si ouvrier sélectionné
        if (ui->worker_selected_idx >= 0) {
            float t = (float)GetTime();
            for (int d = 0; d < gs->map.deposit_count; d++) {
                const MaterialDeposit *dep = &gs->map.deposits[d];
                if (!dep->active) continue;
                int cx = dep->tile_x * TILE_SIZE + TILE_SIZE/2;
                int cy = dep->tile_y * TILE_SIZE + TILE_SIZE/2;
                float pulse = (sinf(t * 5.0f + (float)d) + 1.0f) * 0.5f;
                DrawCircleLines(cx, cy, TILE_SIZE/2 + 3,
                    (Color){192,192,42,
                            (unsigned char)(95 + (int)(pulse*100))});
            }
        }

        // Indicateurs d'amélioration : point pulsant sur les tours qu'on peut
        // réellement améliorer MAINTENANT (axe non-max ET or suffisant).
        {
            float t_dot = (float)GetTime();
            float pulse_dot = (sinf(t_dot * 4.0f) + 1.0f) * 0.5f;
            unsigned char pa = (unsigned char)(140 + (int)(pulse_dot * 100.0f));
            for (int i = 0; i < MAX_TOWERS; i++) {
                const Tower *tw = &gs->towers.towers[i];
                if (!tw->active) continue;
                /* Vérifie qu'au moins un axe est abordable (coût > 0 = pas au max) */
                int c_dmg   = tower_upg_next_cost_dmg  (tw);
                int c_range = tower_upg_next_cost_range (tw);
                int c_rate  = tower_upg_next_cost_rate  (tw);
                int can_upg = (c_dmg   > 0 && gs->gold >= c_dmg)  ||
                              (c_range > 0 && gs->gold >= c_range) ||
                              (c_rate  > 0 && gs->gold >= c_rate);
                if (!can_upg) continue;
                /* Point pulsant en haut-droit de la tuile */
                float dot_x = tw->tile_x * TILE_SIZE + TILE_SIZE - 5.0f;
                float dot_y = tw->tile_y * TILE_SIZE + 5.0f;
                DrawCircle((int)dot_x, (int)dot_y, 4,
                           (Color){255, 220, 50, pa});
                DrawCircleLines((int)dot_x, (int)dot_y, 4,
                                (Color){200, 160, 20, 200});
            }
        }

        // Portée au survol d'une tour posée
        if (ui->hovered_tile_x >= 0 && ui->hovered_tile_y >= 0 &&
            ui->selected_tool == TOOL_NONE && !ui->selection.active) {
            int htx = ui->hovered_tile_x, hty = ui->hovered_tile_y;
            for (int i = 0; i < MAX_TOWERS; i++) {
                const Tower *tw = &gs->towers.towers[i];
                if (!tw->active || tw->tile_x != htx || tw->tile_y != hty) continue;
                float cx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                float cy = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                float rad = tw->range * TILE_SIZE;
                Color rc = TOWER_FILL[tw->type];
                DrawCircle((int)cx, (int)cy, rad,
                           (Color){rc.r, rc.g, rc.b, 18});
                DrawCircleLines((int)cx, (int)cy, rad,
                                (Color){rc.r, rc.g, rc.b, 100});
                break;
            }
        }

        // Portée de la tour sélectionnée
        if (ui->selection.active) {
            int tidx = ui->selection.tower_idx;
            if (tidx >= 0 && tidx < MAX_TOWERS) {
                const Tower *tw = &gs->towers.towers[tidx];
                if (tw->active) {
                    float cx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                    float cy = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                    float rad = tw->range * TILE_SIZE;
                    Color rc = TOWER_FILL[tw->type];
                    DrawCircle((int)cx, (int)cy, rad,
                               (Color){rc.r, rc.g, rc.b, 28});
                    DrawCircleLines((int)cx, (int)cy, rad,
                                   (Color){rc.r, rc.g, rc.b, 160});
                }
            }
        }

        EndMode2D();
    }

    // ════════════════════════════════════════════════
    // NOTIFICATIONS FLOTTANTES
    // ════════════════════════════════════════════════
    {
        int nx = g_map_x_off + g_canvas_virt_w_base / 2;
        int base_y = HUD_Y - 12;
        for (int i = 0; i < ui->notif_count; i++) {
            const FloatNotif *n = &ui->notifs[i];
            float alpha_f = n->timer > 0.5f ? 1.0f : n->timer / 0.5f;
            unsigned char alpha = (unsigned char)(alpha_f * 235.0f);
            int ny = base_y - (int)n->y_off - i * 22;
            if (ny < 4) continue;
            int tw2 = mtxt(n->text, 12);
            DrawRectangleRounded(
                (Rectangle){nx - tw2/2 - 9, ny - 4, tw2 + 18, 19},
                0.3f, 4,
                (Color){4, 3, 1, (unsigned char)(alpha_f * 185.0f)});
            dtxt(n->text, nx - tw2/2, ny, 12,
                     (Color){n->col.r, n->col.g, n->col.b, alpha});
        }
    }

    // ════════════════════════════════════════════════
    // TOOLTIP OUTIL (au-dessus de tout le reste)
    // ════════════════════════════════════════════════
    if (ui->hovered_tool != -1) {
        const ToolInfo  *info   = &TOOL_INFO[ui->hovered_tool];
        const Rectangle *rb     = &ui->tool_btns[ui->hovered_tool];
        int              locked = !tool_is_unlocked((ToolID)ui->hovered_tool, gs);

        const int TW = 170;
        const int TH = locked ? 52 : 62;

        int tx = (int)rb->x;
        int ty = (int)rb->y - TH - GAP;
        if (ty < HUD_Y + 2) ty = HUD_Y + 2;
        if (ty + TH > HUD_Y + HUD_H - 2) ty = HUD_Y + HUD_H - TH - 2;
        if (tx < UI_LEFT_PANEL_W + M) tx = UI_LEFT_PANEL_W + M;
        if (tx + TW > VIRT_W - UI_PANEL_W - M)
            tx = VIRT_W - UI_PANEL_W - M - TW;

        float trnd = (float)UI_RADIUS / TH;
        Color border = locked ? (Color){80, 55, 20, 200} : TOOL_COLORS[ui->hovered_tool];
        DrawRectangleRounded(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, (Color){10, 6, 2, 252});
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)tx,(float)ty,(float)TW,(float)TH},
            trnd, 6, 1.5f, border);

        char dbuf[48];
        clip_text(info->name, TW - M*2, 11, dbuf, sizeof(dbuf));
        dtxt(dbuf, tx+M, ty+M, 11,
             locked ? (Color){80, 62, 35, 255} : TOOL_COLORS[ui->hovered_tool]);

        if (locked) {
            TowerType tt = ui_tool_to_tower((ToolID)ui->hovered_tool);
            const char *when = TOWER_UNLOCK_ACT_NAME[tt];
            clip_text(TextFormat("Deblocage : %s", when ? when : "?"),
                      TW-M*2, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+30, 9, (Color){130, 95, 40, 255});
        } else {
            clip_text(TextFormat("Dmg:%.0f  Port:%.1ft", info->dmg, info->range),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+23, 10, (Color){145,125,92,255});
            clip_text(TextFormat("Cad:%.1f/s  Cout:%d or", info->rate, info->cost),
                      TW-M*2, 10, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+35, 10, (Color){145,125,92,255});
            clip_text(info->desc, TW-M*2, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf, tx+M, ty+49, 9, (Color){82,65,40,255});

            int at_tower_limit = ui_tool_is_tower((ToolID)ui->hovered_tool) &&
                                 gs->towers.tower_count >= gs->towers.tower_limit;
            int at_unit_limit  = ui_tool_is_unit((ToolID)ui->hovered_tool) &&
                                 gs->units.count >= gs->units.unit_limit;
            if (at_tower_limit || at_unit_limit) {
                dtxt(TextFormat("LIMITE (%d bases)", gs->map.base_count),
                         tx + M, ty + TH - 14, 9,
                         (Color){231, 76, 60, 255});
            }
        }
    }

    // ════════════════════════════════════════════════
    // FPS (bas-droite)
    // ════════════════════════════════════════════════
    if (ui->show_fps) {
        int fps = GetFPS();
        char fb[14];
        snprintf(fb, sizeof(fb), "%d FPS", fps);
        Color fc = fps >= 150 ? (Color){46,204,113,255}
                 : fps >= 60  ? (Color){243,156,18,255}
                              : (Color){231,76,60,255};
        int fw = mtxt(fb, 11);
        dtxt(fb, VIRT_W - 4 - fw,
             HUD_Y + UI_HUD_HEIGHT - 4 - fh(11), 11, fc);
    }

    // ════════════════════════════════════════════════
    // FICHE DE DÉCOUVERTE
    // ════════════════════════════════════════════════
    if (ui->disc_count > 0) {
        const DiscEntry *de = &ui->disc_queue[0];
        int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
        int cw = 540, ch = 400;
        int card_x = cx - cw/2, card_y = cy - ch/2;

        // Couleur thématique par catégorie
        Color cat_col =
            de->type == DISC_ENEMY ? (Color){210,  70,  50, 255} :
            de->type == DISC_TOWER ? (Color){215, 155,  40, 255} :
                                     (Color){ 70, 170,  80, 255};
        Color cat_dim = (Color){cat_col.r/4, cat_col.g/4, cat_col.b/4, 255};

        // Fond noir semi-transparent plein écran
        DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0,0,0,180});

        // Panneau principal
        DrawRectangleRounded(
            (Rectangle){(float)card_x,(float)card_y,(float)cw,(float)ch},
            0.05f, 6, (Color){8,5,2,252});
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)card_x,(float)card_y,(float)cw,(float)ch},
            0.05f, 6, 2.0f, cat_col);

        // Bandeau d'en-tête
        int hdr_h = 46;
        DrawRectangleRounded(
            (Rectangle){(float)card_x,(float)card_y,(float)cw,(float)hdr_h},
            0.1f, 4, cat_dim);
        DrawRectangleRoundedLinesEx(
            (Rectangle){(float)card_x,(float)card_y,(float)cw,(float)hdr_h},
            0.1f, 4, 1.5f, cat_col);

        // Badge catégorie
        const char *cat_lbl =
            de->type == DISC_ENEMY ? "ENNEMI" :
            de->type == DISC_TOWER ? "TOUR"   : "UNITE";
        int cl = mtxt(cat_lbl, 9);
        int badge_w = cl + 12, badge_h = 18;
        int bx = card_x + 12, by = card_y + hdr_h/2 - badge_h/2;
        DrawRectangleRounded(
            (Rectangle){(float)bx,(float)by,(float)badge_w,(float)badge_h},
            0.4f, 4, cat_col);
        dtxt(cat_lbl, bx + 6, by + badge_h/2 - fh(9)/2, 9, (Color){8,5,2,255});

        // Titre "DÉCOUVERTE !"
        const char *disc_title = "DECOUVERTE !";
        int tw_title = mtxt(disc_title, 16);
        dtxt(disc_title, cx - tw_title/2, card_y + hdr_h/2 - fh(16)/2, 16, cat_col);

        // Bouton [✕]
        int xbw = 28, xbh = 22;
        int xbx = card_x + cw - 10 - xbw, xby = card_y + 10;
        Rectangle xbtn = {(float)xbx,(float)xby,(float)xbw,(float)xbh};
        int xhov = CheckCollisionPointRec(virt_mouse(), xbtn);
        DrawRectangleRounded(xbtn, 0.3f, 4,
            xhov ? (Color){180,50,40,240} : (Color){60,30,20,200});
        DrawRectangleRoundedLinesEx(xbtn, 0.3f, 4, 1.5f,
            xhov ? (Color){255,100,80,255} : cat_col);
        int xw = mtxt("X", 11);
        dtxt("X", xbx + xbw/2 - xw/2, xby + xbh/2 - fh(11)/2, 11,
             xhov ? WHITE : (Color){200,170,130,255});

        // Corps de la fiche
        int body_y = card_y + hdr_h + 12;
        int body_h = ch - hdr_h - 12 - 14 - 34 - 14;
        int pad = 14;

        // Splash art
        int splash_sz = 160;
        Texture2D *spl = NULL;
        if      (de->type == DISC_ENEMY) spl = &g_enemy_splash[de->idx];
        else if (de->type == DISC_TOWER) spl = &g_tower_splash[de->idx];
        else                              spl = &g_unit_splash [de->idx];

        int spl_x = card_x + pad;
        int spl_y = body_y;
        if (spl && spl->id != 0) {
            DrawTexturePro(*spl,
                (Rectangle){0,0,(float)spl->width,(float)spl->height},
                (Rectangle){(float)spl_x,(float)spl_y,
                            (float)splash_sz,(float)splash_sz},
                (Vector2){0,0}, 0.0f, WHITE);
            DrawRectangleLinesEx(
                (Rectangle){(float)spl_x,(float)spl_y,
                            (float)splash_sz,(float)splash_sz},
                1.5f, cat_col);
        } else {
            // Fallback : carré coloré
            DrawRectangleRounded(
                (Rectangle){(float)spl_x,(float)spl_y,
                            (float)splash_sz,(float)splash_sz},
                0.1f, 4, cat_dim);
            DrawRectangleRoundedLinesEx(
                (Rectangle){(float)spl_x,(float)spl_y,
                            (float)splash_sz,(float)splash_sz},
                0.1f, 4, 1.5f, cat_col);
        }

        // Texte à droite du splash
        int txt_x  = spl_x + splash_sz + pad;
        int txt_w  = card_x + cw - pad - txt_x;
        int txt_y  = body_y;

        // Nom de l'entité
        const char *name = NULL;
        if      (de->type == DISC_ENEMY) name = ENEMY_BASE_STATS[de->idx].name;
        else if (de->type == DISC_TOWER) name = TOWER_BASE_STATS[de->idx].name;
        else                              name = UNIT_BASE_STATS [de->idx].name;
        dtxt(name, txt_x, txt_y, 20, cat_col);
        txt_y += fh(20) + 2;

        // Sous-titre type
        const char *sub =
            de->type == DISC_ENEMY ? "Ennemi rencontre" :
            de->type == DISC_TOWER ? "Tour de defense"  : "Unite alliee";
        dtxt(sub, txt_x, txt_y, 9, (Color){100,85,60,220});
        txt_y += fh(9) + 8;

        // Séparateur
        DrawLineEx((Vector2){(float)txt_x,(float)txt_y},
                   (Vector2){(float)(txt_x+txt_w),(float)txt_y},
                   1.2f, (Color){cat_col.r/3,cat_col.g/3,cat_col.b/3,220});
        txt_y += 6;

        // Stats condensées
        char stat_a[64] = {0}, stat_b[64] = {0};
        if (de->type == DISC_ENEMY) {
            snprintf(stat_a, sizeof(stat_a), "PV : %.0f   Vitesse : %.1f   Or : %d",
                     ENEMY_BASE_STATS[de->idx].hp,
                     ENEMY_BASE_STATS[de->idx].speed,
                     ENEMY_BASE_STATS[de->idx].reward);
        } else if (de->type == DISC_TOWER) {
            snprintf(stat_a, sizeof(stat_a), "Cout : %d or   Dgts : %.0f",
                     TOWER_BASE_STATS[de->idx].cost,
                     TOWER_BASE_STATS[de->idx].damage);
            snprintf(stat_b, sizeof(stat_b), "Portee : %.1f   Cad : %.1f/s",
                     TOWER_BASE_STATS[de->idx].range,
                     TOWER_BASE_STATS[de->idx].fire_rate);
        } else {
            snprintf(stat_a, sizeof(stat_a), "Cout : %d or   PV : %.0f",
                     UNIT_BASE_STATS[de->idx].cost,
                     UNIT_BASE_STATS[de->idx].hp);
            snprintf(stat_b, sizeof(stat_b), "Dgts : %.0f   Vitesse : %.1f",
                     UNIT_BASE_STATS[de->idx].damage,
                     UNIT_BASE_STATS[de->idx].speed);
        }
        if (stat_a[0]) { dtxt(stat_a, txt_x, txt_y, 10, (Color){175,155,115,255}); txt_y += fh(10)+3; }
        if (stat_b[0]) { dtxt(stat_b, txt_x, txt_y, 10, (Color){175,155,115,255}); txt_y += fh(10)+6; }

        // Séparateur
        DrawLineEx((Vector2){(float)txt_x,(float)txt_y},
                   (Vector2){(float)(txt_x+txt_w),(float)txt_y},
                   1.0f, (Color){cat_col.r/4,cat_col.g/4,cat_col.b/4,180});
        txt_y += 6;

        // Description (rendu multiligne pour éviter tout chevauchement)
        const char *desc = NULL;
        if      (de->type == DISC_ENEMY) desc = ENEMY_DESC[de->idx];
        else if (de->type == DISC_TOWER) desc = TOWER_BASE_STATS[de->idx].description;
        else                              desc = UNIT_BASE_STATS [de->idx].description;
        if (desc && txt_y < body_y + body_h) {
            char _dl[128]; int _dlen = 0;
            for (const char *_dp = desc; txt_y < body_y + body_h; _dp++) {
                if (*_dp == '\n' || *_dp == '\0') {
                    _dl[_dlen] = '\0';
                    if (_dlen > 0) {
                        char _dc[128];
                        ui_clip_text(_dl, txt_w, 10, _dc, sizeof(_dc));
                        dtxt(_dc, txt_x, txt_y, 10, (Color){190,170,120,255});
                        txt_y += fh(10) + 4;
                    }
                    _dlen = 0;
                    if (*_dp == '\0') break;
                } else if (_dlen < 127) _dl[_dlen++] = *_dp;
            }
            txt_y += 2;
        }

        // Lore (multiline, séparées par \n)
        const char *lore = NULL;
        if      (de->type == DISC_ENEMY) lore = ENEMY_SPEC[de->idx];
        else if (de->type == DISC_TOWER) lore = TOWER_LORE[de->idx];
        else                              lore = UNIT_LORE [de->idx];
        if (lore) {
            char line[128]; int llen = 0;
            for (const char *p = lore; txt_y < body_y + body_h; p++) {
                if (*p == '\n' || *p == '\0') {
                    line[llen] = '\0';
                    if (llen > 0) {
                        char clip[128];
                        ui_clip_text(line, txt_w, 9, clip, sizeof(clip));
                        dtxt(clip, txt_x, txt_y, 9, (Color){145,130,95,200});
                        txt_y += fh(9) + 3;
                    }
                    llen = 0;
                    if (*p == '\0') break;
                } else if (llen < 127) line[llen++] = *p;
            }
        }

        // Compteur si plusieurs fiches en attente
        if (ui->disc_count > 1) {
            char qbuf[32];
            snprintf(qbuf, sizeof(qbuf), "+%d en attente", ui->disc_count - 1);
            int qw = mtxt(qbuf, 9);
            dtxt(qbuf, card_x + cw - pad - qw,
                 card_y + ch - 14 - 34 - 4 - fh(9), 9,
                 (Color){100,85,60,200});
        }

        // Bouton CONTINUER
        int btnw = 160, btnh = 34;
        int btnx = cx - btnw/2, btny = card_y + ch - 14 - btnh;
        Rectangle cont = {(float)btnx,(float)btny,(float)btnw,(float)btnh};
        int chov = CheckCollisionPointRec(virt_mouse(), cont);
        DrawRectangleRounded(cont, 0.3f, 4,
            chov ? cat_col : cat_dim);
        DrawRectangleRoundedLinesEx(cont, 0.3f, 4, 1.5f, cat_col);
        const char *btn_lbl = ui->disc_count > 1 ? "SUIVANT >" : "CONTINUER";
        int blw = mtxt(btn_lbl, 12);
        dtxt(btn_lbl, btnx + btnw/2 - blw/2,
             btny + btnh/2 - fh(12)/2, 12,
             chov ? (Color){8,5,2,255} : cat_col);
    }
}

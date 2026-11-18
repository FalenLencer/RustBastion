/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render.c ─ Rendu du HUD en jeu.
 *
 *  Contient :
 *    hud_draw_bar  — Barre de progression générique (partagée hud_render*.c)
 *    draw_tool_btn — Bouton outil (tour / unité) avec splash ou icône (static)
 *    ui_render     — Orchestration du HUD par frame ; les grandes sections
 *                    vivent dans hud_render_{info,wave,sel,sel_tower,ovl,world}.c
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

// ════════════════════════════════════════════════════
// HELPERS DE RENDU INTERNES
// ════════════════════════════════════════════════════

void hud_draw_bar(int x, int y, int w, int h,
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

    // Coût : icône or + nombre (centrés ensemble)
    char cost_buf[20];
    snprintf(cost_buf, sizeof(cost_buf), "%d", info->cost);
    int ic = fh(name_fs) - 4;
    int cw = ic + 3 + mtxt(cost_buf, name_fs);
    int cx0 = (int)(r->x + r->width/2 - cw/2);
    draw_icon(g_icon_gold, cx0, cost_y + 2, ic,
              can_afford ? WHITE : (Color){140, 110, 80, 255});
    dtxt(cost_buf, cx0 + ic + 3,
             cost_y, name_fs,
             can_afford ? (Color){230, 150, 32, 255}
                        : (Color){130, 55, 35, 255});

    if (is_selected)
        DrawRectangleRoundedLinesEx(
            (Rectangle){r->x-2, r->y-2, r->width+4, r->height+4},
            rnd, 6, 1.0f,
            (Color){col.r, col.g, col.b, 60});
}

/* Base des notifications flottantes, au-dessus de HUD_Y : le panneau
   d'aperçu monte jusqu'à ~HUD_Y-39 (50 px + 6 px d'écart sur LANCER). */
#define NOTIF_ABOVE_PREVIEW 64

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    ui_anim_tick();   // horloge des animations UI (une fois par passe HUD)

    HUD_FRAME_LOCALS;

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

    hud_render_left_panel(ui, gs);       // vies, ferraille, thème, inventaire

    // ════════════════════════════════════════════════
    // PANNEAU CENTRAL — boutons outils
    // ════════════════════════════════════════════════
    {
        /* lx suit la position X dynamique du premier bouton (centré dans la zone) */
        int lx    = (int)ui->tool_btns[TOOL_TOWER_GUN].x;
        int row1y = (int)ui->tool_btns[TOOL_TOWER_GUN].y;
        int row2y = (int)ui->tool_btns[TOOL_UNIT_SOLDIER].y;

        dtxt("TOURS",  lx, row1y - 17, 10, (Color){100, 70, 22, 255});
        dtxt("UNITÉS", lx, row2y - 17, 10, (Color){38, 100, 38, 255});

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

    hud_render_wave_button(ui, gs);      // LANCER VAGUE + aperçu

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

    hud_render_selection(ui, gs);        // panneau droit
    hud_render_overlays(ui, gs);         // panneaux flottants (or / vague…)
    hud_render_world_marks(ui, gs);      // marques dans l'espace carte
    hud_render_build_ribbon(ui, gs);     // build roguelite (campagne)
    hud_render_hint(ui, gs);             // indice contextuel (CONSEIL)
    hud_render_wave_banner(ui, gs);      // fin de vague

    // ════════════════════════════════════════════════
    // NOTIFICATIONS FLOTTANTES
    // ════════════════════════════════════════════════
    {
        int nx = g_map_x_off + g_canvas_virt_w_base / 2;
        /* Démarre AU-DESSUS du panneau « Prochaine vague » (ancré sur le
           bouton LANCER, ~45 px de haut) : partir de HUD_Y-12 le recouvrait. */
        int base_y = HUD_Y - NOTIF_ABOVE_PREVIEW;
        /* Empilement SANS chevauchement : la plus récente en bas, chaque
           plus ancienne au moins un pas au-dessus de la suivante (les
           anciennes ont déjà dérivé vers le haut : un simple i×pas les
           faisait se croiser avec les nouvelles). */
        const int step = fh(12) + 6;
        int ny_below = base_y + step;
        for (int i = ui->notif_count - 1; i >= 0; i--) {
            const FloatNotif *n = &ui->notifs[i];
            float alpha_f = n->timer > 0.5f ? 1.0f : n->timer / 0.5f;
            unsigned char alpha = (unsigned char)(alpha_f * 235.0f);
            int ny = base_y - (int)n->y_off;
            if (ny > ny_below - step) ny = ny_below - step;
            ny_below = ny;
            if (ny < 4) continue;
            int tw2 = mtxt(n->text, 12);
            DrawRectangleRounded(
                (Rectangle){nx - tw2/2 - 9, ny - 3, tw2 + 18, fh(12) + 2},
                0.3f, 4,
                (Color){4, 3, 1, (unsigned char)(alpha_f * 185.0f)});
            dtxt(n->text, nx - tw2/2, ny, 12,
                     (Color){n->col.r, n->col.g, n->col.b, alpha});
        }
    }

    hud_render_tool_tooltip(ui, gs);     // infobulle (au-dessus du reste)

    // ════════════════════════════════════════════════
    // FPS (bas-droite)
    // ════════════════════════════════════════════════
    if (ui->show_fps) {
        int fps = GetFPS();
        char fb[14];
        snprintf(fb, sizeof(fb), "%d FPS", fps);
        Color fc = fps >= 150 ? UI_SUCCESS
                 : fps >= 60  ? UI_WARN
                              : UI_DANGER;
        int fw = mtxt(fb, 11);
        dtxt(fb, VIRT_W - 4 - fw,
             HUD_Y + UI_HUD_HEIGHT - 4 - fh(11), 11, fc);
    }

    // ════════════════════════════════════════════════
    // FICHE DE DÉCOUVERTE (hud_render_disc.c)
    // ════════════════════════════════════════════════
    hud_render_discovery(ui, gs);
}

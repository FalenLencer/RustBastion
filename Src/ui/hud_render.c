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
#include "ui_plate.h"                // plaques / boutons / touches (style commun)

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

// ── Cartes d'outil (tours / unités) du panneau bas ──────────────
#define CARD_BAND_H     15     /* bandeau du nom, en bas de la carte (px)       */
#define CARD_KEY_FS     HUD_KEY_FS   /* touche de raccourci (coin haut-gauche)  */
#define CARD_TAG_FS      8     /* étiquette de prix (coin haut-droit)           */
#define CARD_NAME_FS     9
#define CARD_SEL_PULSE   4.0f  /* respiration du halo de la carte choisie (rad/s) */
#define CARD_HOV_ZOOM    0.06f /* grossissement du portrait au survol           */
#define SEC_TOWER_COL   HUD_COL_TOWER
#define SEC_UNIT_COL    HUD_COL_UNIT
#define CARD_DIM_TXT    HUD_COL_GREY
#define CARD_PRICE_KO   ((Color){210,  80,  55, 255})   /* prix hors de portée   */
#define CARD_PRICE_OK   ((Color){240, 168,  45, 255})
/* Raccourci clavier de chaque outil (hud_input.c : touches 1 à 9). */
static const char *const TOOL_KEYS[TOOL_COUNT] = {"1","2","3","4","5","6","7","8","9"};
_Static_assert(TOOL_COUNT == 9, "TOOL_KEYS : une touche par outil (1 à 9)");

/* Teinte `col` mêlée au fond sombre (k = part de la couleur). */
static Color tint_well(Color col, float k) {
    return (Color){(unsigned char)(PLATE_WELL.r + col.r * k), (unsigned char)(PLATE_WELL.g + col.g * k),
                   (unsigned char)(PLATE_WELL.b + col.b * k), 255};
}

/* Cadenas dessiné (pas de glyphe : la police ne l'a pas). */
static void draw_lock(int cx, int cy, Color c) {
    DrawRectangle(cx - 7, cy - 2, 14, 10, (Color){c.r / 2, c.g / 2, c.b / 2, 230});
    DrawRectangleLines(cx - 7, cy - 2, 14, 10, c);
    DrawCircleLines(cx, cy - 6, 6, c);
    DrawRectangle(cx - 1, cy + 1, 2, 4, c);
}

/* Carte d'outil : plaque chanfreinée, portrait dans une feuillure teintée,
   touche de raccourci, étiquette de prix, nom. États : choisie (halo qui
   respire), survolée, trop chère (portrait éteint, prix rouge), limite
   atteinte (« MAX »), verrouillée (cadenas + rayures). */
static void draw_tool_card(const Rectangle *r, ToolID id, int sel, int hov,
                           int afford, int locked, int at_limit, float t) {
    const ToolInfo  ti   = hud_tool_info(id);   /* chiffres réels (stats du jeu) */
    const ToolInfo *info = &ti;
    Color col = TOOL_COLORS[id];
    float c   = PLATE_CHAMFER_S;

    if (sel) {
        float p = 0.5f + 0.5f * sinf(t * CARD_SEL_PULSE);
        plate_chamfer_line(plate_inset(*r, -3.0f), c + 2.0f, plate_fade(col, 0.20f + 0.35f * p));
        plate_chamfer_line(plate_inset(*r, -2.0f), c + 1.0f, plate_fade(col, 0.90f));
    } else if (hov && !locked) {
        plate_chamfer_line(plate_inset(*r, -2.0f), c + 1.0f, plate_fade(col, 0.45f));
    }
    plate_draw(*r, c);
    Rectangle in = plate_inset(*r, 3.0f);
    float     ic = c - 2.0f;
    plate_chamfer_fill(in, ic, tint_well(col, locked ? 0.0f : sel ? 0.22f : hov ? 0.14f : 0.06f));

    int cx = (int)(in.x + in.width / 2);
    if (locked) {
        plate_hazard((int)in.x + 3, (int)(in.y + in.height) - 7, (int)in.width - 6, 4,
                     (Color){72, 56, 22, 255});
        draw_lock(cx, (int)(in.y + in.height / 2) - 6, (Color){110, 84, 34, 255});
        int nw = mtxt(info->shortname, CARD_NAME_FS);
        dtxt(info->shortname, cx - nw / 2, (int)(in.y + in.height) - 9 - fh(CARD_NAME_FS),
             CARD_NAME_FS, (Color){78, 62, 38, 255});
        return;
    }

    /* Portrait : remplit la feuillure (bandeau et étiquettes par-dessus). */
    Texture2D splash = ui_tool_is_tower(id) ? g_tower_splash[ui_tool_to_tower(id)]
                                            : g_unit_splash[ui_tool_to_unit(id)];
    if (splash.id != 0) {
        float z  = 1.0f + (hov || sel ? CARD_HOV_ZOOM : 0.0f);
        float sc = fminf((in.width - 4.0f) / (float)splash.width,
                         (in.height - 4.0f) / (float)splash.height) * z;
        float dw = (float)splash.width * sc, dh = (float)splash.height * sc;
        DrawTexturePro(splash, (Rectangle){0, 0, (float)splash.width, (float)splash.height},
                       (Rectangle){in.x + (in.width - dw) / 2, in.y + (in.height - dh) / 2, dw, dh},
                       (Vector2){0, 0}, 0.0f, afford ? WHITE : (Color){100, 90, 80, 255});
    } else {
        int iw = mtxt(info->icon, 20);
        dtxt(info->icon, cx - iw / 2, (int)in.y + 8, 20, afford ? col : CARD_DIM_TXT);
    }

    /* Bandeau du nom (marches de 2 rectangles : suit le chanfrein du bas) */
    int bx = (int)in.x, bw = (int)in.width, iic = (int)ic;
    int by = (int)(in.y + in.height) - CARD_BAND_H;
    DrawRectangle(bx + 1, by, bw - 2, CARD_BAND_H - iic, (Color){0, 0, 0, 200});
    DrawRectangle(bx + iic, by + CARD_BAND_H - iic, bw - 2 * iic, iic, (Color){0, 0, 0, 200});
    DrawRectangle(bx + 1, by, bw - 2, 1, plate_fade(col, sel ? 0.95f : 0.45f));
    int nw = mtxt(info->shortname, CARD_NAME_FS);
    dtxt(info->shortname, cx - nw / 2, by + (CARD_BAND_H - fh(CARD_NAME_FS)) / 2 + 1, CARD_NAME_FS,
         sel ? col : afford ? (Color){222, 208, 180, 255} : CARD_DIM_TXT);

    /* Touche de raccourci, coin haut-gauche */
    plate_keycap_fs((int)in.x + 1, (int)in.y + 1, TOOL_KEYS[id],
                    afford ? UI_ACCENT : CARD_DIM_TXT, CARD_KEY_FS);

    /* Prix (ou MAX), coin haut-droit */
    char pb[12];
    int  tag_h = fh(CARD_TAG_FS) + 1, gi = tag_h - 4;
    int  tw;
    if (at_limit) {
        snprintf(pb, sizeof(pb), "MAX");
        tw = mtxt(pb, CARD_TAG_FS) + 6;
    } else {
        snprintf(pb, sizeof(pb), "%d", info->cost);
        tw = gi + 2 + mtxt(pb, CARD_TAG_FS) + 6;
    }
    int tx = (int)(in.x + in.width) - 1 - tw, ty = (int)in.y + 1;
    DrawRectangle(tx, ty, tw, tag_h, (Color){0, 0, 0, 205});
    DrawRectangle(tx, ty + tag_h - 1, tw, 1, plate_fade(at_limit || !afford ? CARD_PRICE_KO
                                                                             : CARD_PRICE_OK, 0.6f));
    if (at_limit) {
        dtxt(pb, tx + 3, ty, CARD_TAG_FS, CARD_PRICE_KO);
    } else {
        draw_icon(g_icon_gold, tx + 3, ty + 2, gi, afford ? WHITE : (Color){150, 120, 90, 255});
        dtxt(pb, tx + 3 + gi + 2, ty, CARD_TAG_FS, afford ? CARD_PRICE_OK : CARD_PRICE_KO);
    }
}

/* Titre de rangée : « TOURS ── 3 / 12 » ; compteur rouge à la limite. */
static void draw_row_title(int x, int y, int w, const char *txt, Color col, int n, int lim) {
    plate_section(x, y, w, txt, col);
    char b[40];   /* « LIMITE  » + 2 entiers 32 bits complets */
    int full = (n >= lim);
    snprintf(b, sizeof(b), full ? "LIMITE  %d / %d" : "%d / %d", n, lim);
    dtxt(b, x + w - mtxt(b, 9), y, 9, full ? UI_DANGER : (Color){175, 160, 130, 255});
}

/* Base des notifications flottantes, au-dessus de HUD_Y (bord de la carte). */
#define NOTIF_ABOVE_HUD 26

// ════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════
void ui_render(const UIState *ui, const GameState *gs) {
    ui_anim_tick();   // horloge des animations UI (une fois par passe HUD)

    HUD_FRAME_LOCALS;

    // ── Fond : trois plaques de métal (statut / commandement / fiche) ──
    {
        static float card_t = 0.0f;          /* respiration de la carte choisie */
        card_t += ui_dt();
        const int g2 = HUD_PLATE_GAP / 2, py0 = HUD_Y + g2, ph = HUD_H - HUD_PLATE_GAP;
        DrawRectangle(0, HUD_Y, VIRT_W, HUD_H, PLATE_WELL);
        DrawRectangleGradientV(0, HUD_Y - 10, VIRT_W, 10, (Color){0, 0, 0, 0}, (Color){0, 0, 0, 120});
        /* Plaque droite : liseré à la couleur du sujet affiché */
        Color racc = UI_ACCENT_DIM;
        if (ui->selected_tool != TOOL_NONE)  racc = TOOL_COLORS[ui->selected_tool];
        else if (ui->sell_unit_idx >= 0)     racc = (Color){110, 200, 110, 255};
        else if (ui->selection.active)       racc = SEC_TOWER_COL;
        else if (gs->phase == PHASE_WAVE || game_next_wave_is_boss(gs)) racc = UI_DANGER;
        plate_panel((Rectangle){(float)g2, (float)py0, (float)(UI_LEFT_PANEL_W - HUD_PLATE_GAP),
                                (float)ph}, PLATE_CHAMFER, UI_ACCENT, 0.45f);
        plate_panel((Rectangle){(float)(UI_LEFT_PANEL_W + g2), (float)py0,
                                (float)(VIRT_W - UI_LEFT_PANEL_W - UI_PANEL_W - HUD_PLATE_GAP),
                                (float)ph}, PLATE_CHAMFER, UI_ACCENT_DIM, 0.35f);
        plate_panel((Rectangle){(float)(VIRT_W - UI_PANEL_W + g2), (float)py0,
                                (float)(UI_PANEL_W - HUD_PLATE_GAP), (float)ph},
                    PLATE_CHAMFER, racc, 0.55f);

        hud_render_left_panel(ui, gs);   // or, bases, ferraille, minerais, slots

        // ── Plaque centrale : titres de rangée + cartes d'outil ──
        int lx    = (int)ui->tool_btns[TOOL_TOWER_GUN].x;
        int row1y = (int)ui->tool_btns[TOOL_TOWER_GUN].y;
        int row2y = (int)ui->tool_btns[TOOL_UNIT_SOLDIER].y;
        int rw    = (int)(ui->tool_btns[TOOL_UNIT_WORKER].x + UI_BTN_W) - lx;
        draw_row_title(lx, row1y - HUD_SEC_H, rw, "TOURS", SEC_TOWER_COL,
                       gs->towers.tower_count, gs->towers.tower_limit);
        draw_row_title(lx, row2y - HUD_SEC_H, rw, "UNITÉS", SEC_UNIT_COL,
                       gs->units.count, gs->units.unit_limit);

        for (int i = 0; i < TOOL_COUNT; i++) {
            int locked   = !tool_is_unlocked((ToolID)i, gs);
            int at_limit = !locked &&
                ((ui_tool_is_tower((ToolID)i) && gs->towers.tower_count >= gs->towers.tower_limit) ||
                 (ui_tool_is_unit((ToolID)i)  && gs->units.count >= gs->units.unit_limit));
            int afford   = !locked && !at_limit && gs->gold >= hud_tool_info((ToolID)i).cost;
            draw_tool_card(&ui->tool_btns[i], (ToolID)i, ui->selected_tool == i,
                           ui->hovered_tool == i, afford, locked, at_limit, card_t);
        }
    }

    hud_render_wave_button(ui, gs);      // LANCER LA VAGUE

    // ════════════════════════════════════════════════
    // BOUTON PAUSE (+ sa touche)
    // ════════════════════════════════════════════════
    {
        const Rectangle *pb = &ui->pause_btn;
        int   phov = CheckCollisionPointRec(virt_mouse(), *pb);
        Color pc   = phov ? (Color){230, 200, 130, 255} : (Color){180, 150, 90, 255};
        plate_button(*pb, (Color){170, 140, 90, 255}, phov, 1);
        int pcx = (int)(pb->x + pb->width  / 2);
        int pcy = (int)(pb->y + pb->height / 2) - 8;
        DrawRectangle(pcx - 7, pcy - 10, 5, 20, pc);
        DrawRectangle(pcx + 2, pcy - 10, 5, 20, pc);
        const char *pk = "ÉCHAP";
        int kw = mtxt(pk, CARD_KEY_FS) + 6;
        plate_keycap_fs(pcx - kw / 2, (int)(pb->y + pb->height) - 10 - fh(CARD_KEY_FS) - 4,
                        pk, UI_ACCENT, CARD_KEY_FS);
    }

    hud_render_selection(ui, gs);        // panneau droit
    hud_render_overlays(ui, gs);         // panneaux flottants (or / vague…)
    hud_render_world_marks(ui, gs);      // marques dans l'espace carte
    hud_render_enemy_tip(ui, gs);        // fiche de l'ennemi survolé
    hud_render_build_ribbon(ui, gs);     // build roguelite (campagne)
    hud_render_hint(ui, gs);             // indice contextuel (CONSEIL)
    hud_render_wave_banner(ui, gs);      // fin de vague

    // ════════════════════════════════════════════════
    // NOTIFICATIONS FLOTTANTES
    // ════════════════════════════════════════════════
    {
        int nx = g_map_x_off + g_canvas_virt_w_base / 2;
        /* Juste au-dessus du panneau bas (l'aperçu de vague flottant qui
           occupait cette place est désormais DANS le panneau droit). */
        int base_y = HUD_Y - NOTIF_ABOVE_HUD;
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
            /* Pastille chanfreinée + repère à la couleur du message */
            Rectangle nr = {(float)(nx - tw2/2 - 9), (float)(ny - 3), (float)(tw2 + 18), (float)(fh(12) + 2)};
            plate_chamfer_fill(nr, 4.0f, (Color){10, 8, 6, (unsigned char)(alpha_f * 200.0f)});
            DrawRectangle((int)nr.x + 3, (int)nr.y + 4, 2, (int)nr.height - 8,
                          (Color){n->col.r, n->col.g, n->col.b, alpha});
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

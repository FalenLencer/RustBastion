/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_enemy.c ─ Fiche d'un ennemi AU SURVOL de la carte.
 *
 *  Le réflexe de tout joueur de tower defense : pointer un ennemi pour voir
 *  ses PV, ses points faibles et ce qu'il résiste — jusqu'ici réservé à la
 *  fiche de première rencontre et au bestiaire du menu. Affichée seulement
 *  sans outil en main (la pose garde son propre aperçu).
 */
#include "hud_internal.h"
#include "ui_palette.h"
#include "ui_plate.h"

#define ETIP_PICK_MIN   10.0f   /* rayon de saisie minimal autour d'un ennemi (px monde) */
#define ETIP_PICK_K      1.6f   /* … ou taille × k si plus grand                         */
#define ETIP_W          210     /* largeur de la fiche (px canvas)                       */
#define ETIP_PAD          9
#define ETIP_NAME_FS     11
#define ETIP_FS           9
#define ETIP_BAR_H        6
#define ETIP_OFF         16     /* décalage depuis le curseur                            */
#define ETIP_WEAK_COL   ((Color){120, 220, 140, 255})
#define ETIP_RES_COL    ((Color){230, 120,  95, 255})
#define ETIP_TXT_COL    ((Color){205, 190, 160, 255})

int hud_enemy_pick(const GameState *gs, Vector2 mouse) {
    Vector2 w = map_screen_to_world(mouse);
    int   best = -1;
    float bd   = 1e9f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &gs->enemies.enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f || e->invisible) continue;
        float r = e->size * ETIP_PICK_K;
        if (r < ETIP_PICK_MIN) r = ETIP_PICK_MIN;
        float d = hypotf(e->x - w.x, e->y - w.y);
        if (d <= r && d < bd) { bd = d; best = i; }
    }
    return best;
}

/* Ligne « Étiquette : texte » coupée à la largeur ; avance y. */
static void tip_line(int x, int *y, int w, const char *lbl, const char *txt, Color c) {
    char b[96], cb[96];
    snprintf(b, sizeof(b), "%s%s", lbl, txt);
    clip_text(b, w, ETIP_FS, cb, sizeof(cb));
    dtxt(cb, x, *y, ETIP_FS, c);
    *y += fh(ETIP_FS);
}

void hud_render_enemy_tip(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    Vector2 m = virt_mouse();
    if (m.y >= HUD_Y || ui->selected_tool != TOOL_NONE || ui->box_dragging ||
        ui->disc_count > 0 || ui->dragging_overlay >= 0) return;
    int idx = hud_enemy_pick(gs, m);
    if (idx < 0) return;
    const Enemy *e = &gs->enemies.enemies[idx];
    Color ec = renderer_enemy_color(e->type);

    char weak[72], res[72], st[64];
    int  nw = hud_enemy_dmg_list(e->type, 1, 1, weak, (int)sizeof(weak));
    int  nr = hud_enemy_dmg_list(e->type, 0, 1, res,  (int)sizeof(res));
    int  off = 0;
    st[0] = '\0';
    if (e->slow_timer   > 0.0f) off += snprintf(st + off, sizeof(st) - (size_t)off, "%sRalenti", off ? ", " : "");
    if (e->poison_timer > 0.0f && off < (int)sizeof(st))
        off += snprintf(st + off, sizeof(st) - (size_t)off, "%sCorrodé", off ? ", " : "");
    if (e->burn_stacks  > 0 && off < (int)sizeof(st))
        off += snprintf(st + off, sizeof(st) - (size_t)off, "%sBrûlure ×%d", off ? ", " : "", e->burn_stacks);
    if (e->boss_shield  > 0.0f && off < (int)sizeof(st))
        snprintf(st + off, sizeof(st) - (size_t)off, "%sBouclier", off ? ", " : "");

    /* Hauteur au contenu (pas vertical = fh) */
    int lines = 3 + (st[0] ? 1 : 0);                     /* vitesse, faible, résiste (+ états) */
    int tip_h = ETIP_PAD * 2 + fh(ETIP_NAME_FS) + 2 + fh(ETIP_FS) + 2 + lines * fh(ETIP_FS);
    int tx = (int)m.x + ETIP_OFF, ty = (int)m.y + ETIP_OFF;
    if (tx + ETIP_W > g_map_x_off + g_canvas_virt_w_base - 4) tx = (int)m.x - ETIP_OFF - ETIP_W;
    if (ty + tip_h > HUD_Y - 4) ty = HUD_Y - 4 - tip_h;
    if (ty < 4) ty = 4;

    plate_panel((Rectangle){(float)tx, (float)ty, (float)ETIP_W, (float)tip_h}, PLATE_CHAMFER_S,
                e->is_boss ? UI_DANGER : ec, 0.9f);
    int x = tx + ETIP_PAD, y = ty + ETIP_PAD, w = ETIP_W - ETIP_PAD * 2;

    /* Nom (+ BOSS) */
    dtxt(ENEMY_BASE_STATS[e->type].name, x, y, ETIP_NAME_FS, ec);
    if (e->is_boss) {
        const char *bt = "BOSS";
        dtxt(bt, x + w - mtxt(bt, ETIP_NAME_FS), y, ETIP_NAME_FS, UI_DANGER);
    }
    y += fh(ETIP_NAME_FS) + 2;

    /* PV : barre + valeurs */
    float hr = e->max_hp > 0.0f ? e->hp / e->max_hp : 0.0f;
    char hb[32];
    snprintf(hb, sizeof(hb), "%.0f / %.0f", e->hp > 0.0f ? e->hp : 0.0f, e->max_hp);
    int hw = mtxt(hb, ETIP_FS);
    hud_draw_bar(x, y + 2 + (fh(ETIP_FS) - ETIP_BAR_H) / 2 - 2, w - hw - 8, ETIP_BAR_H, hr,
                 hr > 0.6f ? UI_SUCCESS : hr > 0.3f ? UI_WARN : UI_DANGER, (Color){24, 16, 12, 255});
    dtxt(hb, x + w - hw, y, ETIP_FS, ETIP_TXT_COL);
    y += fh(ETIP_FS) + 2;

    char sp[48];
    snprintf(sp, sizeof(sp), "%.1f   ·   Or : %d", e->speed, e->reward);
    tip_line(x, &y, w, "Vitesse : ", sp, ETIP_TXT_COL);
    tip_line(x, &y, w, "Faible : ",  nw ? weak : "rien de marqué", nw ? ETIP_WEAK_COL : HUD_COL_GREY);
    tip_line(x, &y, w, "Résiste : ", nr ? res  : "rien de marqué", nr ? ETIP_RES_COL  : HUD_COL_GREY);
    if (st[0]) tip_line(x, &y, w, "État : ", st, UI_INFO);
}

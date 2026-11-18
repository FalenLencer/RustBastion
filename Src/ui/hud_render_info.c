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
#include "ui_plate.h"                // boutons / touches en plaque de métal

// ── Panneau gauche (statut) ──────────────────────────────────────
#define LP_GOLD_FS      20     /* trésor : le chiffre qu'on lit le plus souvent    */
#define LP_LINE_FS       9
#define LP_BAR_H         5
#define LP_INV_SLOT     14     /* case de l'inventaire de minerai (px)             */
#define LP_INV_GAP       3
#define LP_BTN_FS       10
#define LP_GOLD_COL     ((Color){240, 175,  50, 255})
#define LP_LABEL_COL    ((Color){150, 132, 100, 255})
#define LP_SCRAP_COL    ((Color){150, 205,  80, 255})
#define LP_INV_COL      ((Color){ 90, 190, 210, 255})
#define LP_GOLD_SNAP       10000.0f  /* écart = nouvelle partie : pas de défilement */
#define LP_GOLD_ROLL       8.0f      /* vitesse du compteur roulant (1/s)          */
#define LP_GOLD_FLASH_MIN  5         /* gain minimal qui fait flasher le chiffre   */
#define LP_GOLD_FLASH_GAIN 0.25f     /* flash blanc → or (s)                       */
#define LP_GOLD_FLASH_LOSS 0.20f     /* flash rouge → or (s)                       */
#define LP_GOLD_BUMP_DUR   0.20f     /* saut de l'icône au gain (s)                */
#define LP_HIT_DUR         0.35f     /* battement du cœur au dégât (s)             */
#define LP_HIT_RING_DUR    0.40f     /* onde de choc du cœur (s)                   */

/* Bouton d'achat de slot : plaque + « +1 Tour » à gauche, prix à droite. */
static void slot_button(const Rectangle *r, const char *lbl, int cost, int at_max,
                        int afford, Color acc) {
    int hov = CheckCollisionPointRec(virt_mouse(), *r);
    plate_button(*r, acc, hov && !at_max, !at_max);
    int ty = (int)(r->y + (r->height - fh(LP_BTN_FS)) / 2);
    dtxt(lbl, (int)r->x + 16, ty, LP_BTN_FS, at_max ? HUD_COL_GREY : acc);
    char pb[16];
    if (at_max) snprintf(pb, sizeof(pb), "MAX");
    else        snprintf(pb, sizeof(pb), "%d", cost);
    int pw = mtxt(pb, LP_BTN_FS), px = (int)(r->x + r->width) - 16 - pw;
    if (!at_max) {
        int gi = fh(LP_BTN_FS) - 5;
        draw_icon(g_icon_gold, px - gi - 2, ty + 3, gi, afford ? WHITE : HUD_COL_GREY);
    }
    dtxt(pb, px, ty, LP_BTN_FS, at_max ? HUD_COL_GREY : afford ? LP_GOLD_COL : UI_DANGER);
}

/* Panneau gauche — or, bases, ferraille, minerais, achats de slots. */
void hud_render_left_panel(const UIState *ui, const GameState *gs) {
    HUD_FRAME_LOCALS;
    const int x = HUD_LX, w = HUD_LW;
    int y = HUD_Y + HUD_SEC_Y;

    /* ── OR « vivant » : compteur roulant, icône qui saute au gain, flash
          blanc (gain ≥ 5) ou rouge (dépense). Repris de l'ancien panneau
          flottant haut-gauche, supprimé (doublon de ce panneau). ── */
    {
        static float disp = -1.0f, fx_t = 1e9f;
        static int   prev = -1, gain = 1;
        if (disp < 0.0f || fabsf((float)gs->gold - disp) > LP_GOLD_SNAP) {   /* nouvelle partie */
            disp = (float)gs->gold; prev = gs->gold;
        }
        if (gs->gold != prev) {
            int d = gs->gold - prev;
            if (d >= LP_GOLD_FLASH_MIN) { fx_t = 0.0f; gain = 1; }
            else if (d < 0)             { fx_t = 0.0f; gain = 0; }
            prev = gs->gold;
        }
        disp += ((float)gs->gold - disp) * fminf(1.0f, ui_dt() * LP_GOLD_ROLL);
        fx_t += ui_dt();

        int gi = fh(LP_GOLD_FS) - 8, isz = gi;
        if (gain && fx_t < LP_GOLD_BUMP_DUR)
            isz = (int)((float)gi * (1.0f + 0.25f * sinf(fx_t / LP_GOLD_BUMP_DUR * PI)) + 0.5f);
        draw_icon(g_icon_gold, x - (isz - gi) / 2, y + 4 - (isz - gi) / 2, isz, WHITE);
        Color gc = LP_GOLD_COL;
        float fd = gain ? LP_GOLD_FLASH_GAIN : LP_GOLD_FLASH_LOSS;
        if (fx_t < fd) {
            float f = fx_t / fd;
            Color from = gain ? WHITE : (Color){255, 120, 80, 255};
            gc = (Color){(unsigned char)(from.r + (gc.r - from.r) * f),
                         (unsigned char)(from.g + (gc.g - from.g) * f),
                         (unsigned char)(from.b + (gc.b - from.b) * f), 255};
        }
        int shown = (int)(disp + 0.5f);
        char gb[16];
        if (shown >= 100000) snprintf(gb, sizeof(gb), "%dk", shown / 1000);
        else                 snprintf(gb, sizeof(gb), "%d", shown);
        dtxt(gb, x + gi + 6, y, LP_GOLD_FS, gc);
        y += fh(LP_GOLD_FS) + 2;
    }

    /* ── BASES : PV cumulés (= vies) + barre ── */
    {
        int hp = 0, mx = 0;
        for (int b = 0; b < gs->map.base_count && b < MAX_BASES; b++) {
            if (!gs->map.bases[b].active) continue;
            hp += gs->map.bases[b].hp > 0 ? gs->map.bases[b].hp : 0;
            mx += gs->map.bases[b].max_hp;
        }
        float r  = mx > 0 ? (float)hp / (float)mx : 0.0f;
        Color hc = r > 0.6f ? UI_SUCCESS : r > 0.3f ? UI_WARN : UI_DANGER;
        /* Battement au dégât : cœur qui gonfle (retombée élastique) + onde */
        static int   prev_lives = -1;
        static float hit_t      = 1e9f;
        if (prev_lives < 0 || gs->lives > prev_lives) prev_lives = gs->lives;
        if (gs->lives < prev_lives) { hit_t = 0.0f; prev_lives = gs->lives; }
        hit_t += ui_dt();
        int hi = fh(LP_LINE_FS) - 1, hsz = hi;
        if (hit_t < LP_HIT_DUR)
            hsz = (int)((float)hi * (1.0f + 0.5f * (1.0f - ea_out_elastic(hit_t / LP_HIT_DUR))) + 0.5f);
        draw_icon(g_icon_heart, x - 1 - (hsz - hi) / 2, y - (hsz - hi) / 2, hsz, hc);
        if (hit_t < LP_HIT_RING_DUR) {
            float pr = hit_t / LP_HIT_RING_DUR;
            DrawCircleLines(x - 1 + hi / 2, y + hi / 2, 6.0f + 16.0f * pr,
                            (Color){hc.r, hc.g, hc.b, (unsigned char)(200.0f * (1.0f - pr))});
        }
        dtxt(gs->map.base_count > 1 ? "Bases" : "Base", x + hi + 5, y, LP_LINE_FS, LP_LABEL_COL);
        char vb[16];
        snprintf(vb, sizeof(vb), "%d", gs->lives);
        dtxt(vb, x + w - mtxt(vb, LP_LINE_FS), y, LP_LINE_FS, hc);
        y += fh(LP_LINE_FS) + 1;
        hud_draw_bar(x, y, w, LP_BAR_H, r, hc, (Color){20, 14, 10, 255});
        y += LP_BAR_H + 6;
    }

    /* ── FERRAILLE (méta-progression) — campagne seulement ── */
    if (gs->is_campaign) {
        int si = fh(LP_LINE_FS) - 2;
        draw_icon(g_icon_scrap, x - 1, y + 1, si, WHITE);
        dtxt("Ferraille", x + si + 4, y, LP_LINE_FS, LP_LABEL_COL);
        char sb[16];
        snprintf(sb, sizeof(sb), "%d", gs->meta.scrap);
        dtxt(sb, x + w - mtxt(sb, LP_LINE_FS), y, LP_LINE_FS, LP_SCRAP_COL);
        y += fh(LP_LINE_FS) + 4;
    }

    /* ── MINERAIS : une case par place d'inventaire ── */
    {
        int full = (gs->inventory_count >= MAX_INVENTORY);
        dtxt("Minerais", x, y, LP_LINE_FS, LP_LABEL_COL);
        char ib[24];
        snprintf(ib, sizeof(ib), full ? "PLEIN %d/%d" : "%d/%d", gs->inventory_count, MAX_INVENTORY);
        dtxt(ib, x + w - mtxt(ib, LP_LINE_FS), y, LP_LINE_FS, full ? UI_WARN : LP_INV_COL);
        y += fh(LP_LINE_FS) + 1;
        int step = LP_INV_SLOT + LP_INV_GAP;
        int ix   = x + (w - (MAX_INVENTORY * step - LP_INV_GAP)) / 2;
        for (int k = 0; k < MAX_INVENTORY; k++, ix += step) {
            DrawRectangle(ix, y, LP_INV_SLOT, LP_INV_SLOT, PLATE_FRAME_D);
            DrawRectangle(ix + 1, y + 1, LP_INV_SLOT - 2, LP_INV_SLOT - 2, (Color){10, 8, 6, 255});
            if (k >= gs->inventory_count) continue;
            MaterialType m = gs->inventory[k];
            Color mc = (m > MAT_NONE && m < MAT_COUNT) ? MATERIAL_COLORS[m] : LP_INV_COL;
            int cx = ix + LP_INV_SLOT / 2, cy = y + LP_INV_SLOT / 2, rr = LP_INV_SLOT / 2 - 2;
            plate_tri((Vector2){(float)cx, (float)(cy - rr)}, (Vector2){(float)(cx - rr), (float)cy},
                      (Vector2){(float)(cx + rr), (float)cy}, mc);                      /* gemme */
            plate_tri((Vector2){(float)(cx - rr), (float)cy}, (Vector2){(float)cx, (float)(cy + rr)},
                      (Vector2){(float)(cx + rr), (float)cy},
                      (Color){(unsigned char)(mc.r * 0.6f), (unsigned char)(mc.g * 0.6f),
                              (unsigned char)(mc.b * 0.6f), 255});
            DrawRectangle(cx - 1, cy - rr + 2, 2, 2, (Color){255, 255, 255, 170}); /* reflet */
        }
    }

    // ── Achats de slots (bas) ; masqués en début de partie (P1.2,
    // prédicat partagé avec l'input) : rappel de l'aide à la place.
    if (!hud_show_slot_buys(gs)) {
        plate_key_hint(x, (int)ui->buy_unit_slot_btn.y, "H", "Aide et commandes", 1);
        return;
    }
    {
        int b = gs->slots_tower_bought;
        int at_max = (b >= SLOT_MAX_BUYS || tower_active_limit(&gs->bonuses) + b >= MAX_TOWERS_HARD);
        int cost   = at_max ? 0 : SLOT_TOWER_COSTS[b];
        slot_button(&ui->buy_tower_slot_btn, "+1 Tour", cost, at_max, gs->gold >= cost, HUD_COL_TOWER);
    }
    {
        int b = gs->slots_unit_bought;
        int at_max = (b >= SLOT_MAX_BUYS ||
                      unit_active_limit(&gs->bonuses, gs->map.base_count) + b >= MAX_UNITS);
        int cost   = at_max ? 0 : SLOT_UNIT_COSTS[b];
        slot_button(&ui->buy_unit_slot_btn, "+1 Unité", cost, at_max, gs->gold >= cost, HUD_COL_UNIT);
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
#define HINT_BELOW_BANNER_Y 168   // sous le bandeau de fin de vague (haut max ≈ 160 px)
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
    /* Bandeau de fin de vague affiché (même place, centré) : le conseil passe
       dessous au lieu d'être recouvert. */
    if (ui->wave_banner_t > 0.0f && y < HINT_BELOW_BANNER_Y) y = HINT_BELOW_BANNER_Y;
    int x  = g_map_x_off + g_canvas_virt_w_base / 2 - bw / 2;
    Rectangle r = {(float)x, (float)y, (float)bw, (float)bh};
    plate_alpha(a);   /* plaque de métal, liseré bleu « conseil » */
    plate_panel(plate_inset(r, -(float)PLATE_BORDER), PLATE_CHAMFER_S, (Color){110, 180, 230, 255}, 0.9f);
    plate_alpha(1.0f);
    int ch = fh(HINT_TAG_FS) + 4, cy = y + (bh - ch) / 2;
    plate_chamfer_fill((Rectangle){(float)(x + M), (float)cy, (float)gw, (float)ch}, 3.0f,
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
        const ToolInfo   ti     = hud_tool_info((ToolID)ui->hovered_tool);   /* chiffres réels */
        const ToolInfo  *info   = &ti;
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

        Color border = locked ? (Color){80, 55, 20, 200} : TOOL_COLORS[ui->hovered_tool];
        plate_panel((Rectangle){(float)(tx - PLATE_BORDER), (float)(ty - PLATE_BORDER),
                                (float)(TW + 2 * PLATE_BORDER), (float)(TH + 2 * PLATE_BORDER)},
                    PLATE_CHAMFER_S, border, 0.85f);

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
                    int ay = ty - PLATE_BORDER - ah - 2;   /* au-dessus du cadre */
                    DrawRectangle(tx, ay, aw + M * 2, ah, UI_PANEL_BG_SOFT);
                    dtxt(adv, tx + M, ay + 3, 10, UI_INFO);
                }
            }
        }
    }
}

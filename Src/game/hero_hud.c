/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero_hud.c ─ MODE HÉROS : couche 2D par-dessus la scène 3D.
 *  Viseur, bannière d'objectif, stats, arme, invites contextuelles,
 *  toast, aide [H], panneaux pause/défaite, et POPUPS DU MONDE
 *  (dégâts EFFICACE/RESISTE/SYNERGIE + or) projetés dans la vue 3D.
 *  Extrait de hero.c (règle projet : fichiers < 500 lignes).
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"   /* w3d_from_sim                       */
#include "../ui/renderer.h"         /* g_canvas_virt_w/h                  */
#include "../ui/ui_utils.h"         /* dtxt / mtxt / fh / draw_icon       */
#include "../ui/ui_anim.h"          /* ui_anim_tick / ui_dt (pop icônes)  */
#include "../combat/fx.h"           /* g_fx (popups à projeter)           */
#include "rlgl.h"                   /* depth test off (viewmodel)         */
#include <math.h>
#include <stdio.h>

#define HERO_POPUP_BASE_H  1.35f  /* hauteur monde de départ des popups   */
#define HERO_POPUP_RISE_W  1.0f   /* montée des popups (unités monde/s)   */
#define HERO_HPBAR_W       34     /* largeur barre PV ennemie (px canvas) */
#define HERO_HPBAR_BOSS_W  64     /* largeur barre PV de boss             */

/* ════════════════════════════════════════════════════════════════
   POPUPS DU MONDE (dégâts EFFICACE/RESISTE/SYNERGIE, or) EN VUE 3D
   Position sim → monde → canvas via GetWorldToScreenEx aux dimensions
   du CANVAS (= la projection exacte de la scène rendue dedans).
   ════════════════════════════════════════════════════════════════ */
void hero_draw_world_popups(AppContext *ctx, Camera3D cam) {
    Vector3 fw = { cam.target.x - cam.position.x,
                   cam.target.y - cam.position.y,
                   cam.target.z - cam.position.z };

    /* ── Barres de PV ennemies (entamés ou boss), projetées ──────── */
    {
        const GameState *gs = &ctx->gs;
        const HeroState *h  = &ctx->hero;
        float maxd = HERO_HPBAR_TILES * (float)TILE_SIZE;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &gs->enemies.enemies[i];
            if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
            if (!e->is_boss && e->hp >= e->max_hp) continue;  /* intacts : rien */
            float ex = e->x - h->px, ey = e->y - h->py;
            if (ex * ex + ey * ey > maxd * maxd) continue;    /* trop loin      */
            float top = W3D_ENEMY_BODY_H * 2.1f * (e->is_boss ? 1.8f : 1.0f);
            Vector3 p = w3d_from_sim(e->x, e->y, top);
            Vector3 d = { p.x - cam.position.x, p.y - cam.position.y,
                          p.z - cam.position.z };
            if (fw.x * d.x + fw.y * d.y + fw.z * d.z <= 0.0f) continue;
            Vector2 s = GetWorldToScreenEx(p, cam,
                                           g_canvas_virt_w, g_canvas_virt_h);
            float ratio = (e->max_hp > 0.0f) ? e->hp / e->max_hp : 0.0f;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            int bw = e->is_boss ? HERO_HPBAR_BOSS_W : HERO_HPBAR_W;
            int bx = (int)s.x - bw / 2, by = (int)s.y;
            Color fc = (ratio > 0.55f) ? (Color){ 90, 210,  90, 235}
                     : (ratio > 0.25f) ? (Color){235, 190,  60, 235}
                                       : (Color){225,  70,  55, 235};
            DrawRectangle(bx - 1, by - 1, bw + 2, 6, (Color){10, 10, 12, 200});
            DrawRectangle(bx, by, (int)(bw * ratio), 4, fc);
        }
    }

    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        const FxPopup *u = &g_fx.popups[i];
        if (!u->active) continue;
        float el = u->max_life - u->life;      /* temps écoulé (s)        */
        /* Le popup 2D monte en décalant son y ; on reconstruit le point
           d'origine AU SOL et on convertit la montée en hauteur monde. */
        float y0  = u->y - u->vy * el;
        Vector3 p = w3d_from_sim(u->x, y0,
                                 HERO_POPUP_BASE_H + el * HERO_POPUP_RISE_W);
        Vector3 d = { p.x - cam.position.x, p.y - cam.position.y,
                      p.z - cam.position.z };
        if (fw.x * d.x + fw.y * d.y + fw.z * d.z <= 0.0f) continue; /* dos */
        Vector2 s = GetWorldToScreenEx(p, cam,
                                       g_canvas_virt_w, g_canvas_virt_h);
        float a = (u->max_life > 0.0f) ? u->life / u->max_life : 0.0f;
        if (a < 0.0f) a = 0.0f;
        if (a > 1.0f) a = 1.0f;
        Color c = u->col;
        c.a = (unsigned char)(255.0f * a);
        int w = mtxt(u->text, 11);
        dtxt_o(u->text, (int)s.x - w / 2, (int)s.y, 11, c);
    }
}

/* ════════════════════════════════════════════════════════════════
   HOTBAR — cartes de sélection lisibles (tours / recrues)
   ════════════════════════════════════════════════════════════════ */
#define HB_CARD_W  96
#define HB_CARD_H  46
#define HB_GAP      8

static void hotbar_card(int x, int y, char key, const char *name, int cost,
                        Color col, int sel, int enabled, int locked) {
    Rectangle r = {(float)x, (float)y, (float)HB_CARD_W, (float)HB_CARD_H};
    Color bg = locked ? (Color){24, 24, 28, 230}
             : sel    ? (Color){28, 44, 30, 242}
                      : (Color){13, 17, 23, 228};
    DrawRectangleRounded(r, 0.18f, 4, bg);
    Color brd = sel    ? (Color){245, 205, 90, 255}
              : locked ? (Color){70, 70, 78, 220} : col;
    DrawRectangleRoundedLinesEx(r, 0.18f, 4, sel ? 2.4f : 1.4f, brd);

    char kb[6];
    snprintf(kb, sizeof(kb), "[%c]", key);
    dtxt(kb, x + 6, y + 4, 10, (Color){170, 190, 210, 255});

    Color nc = locked ? (Color){110, 110, 118, 255}
             : enabled ? (Color){225, 232, 240, 255}
                       : (Color){140, 140, 148, 255};
    int nw = mtxt(name, 10);
    if (nw > HB_CARD_W - 8) nw = HB_CARD_W - 8;
    dtxt(name, x + HB_CARD_W / 2 - nw / 2, y + 15, 10, nc);

    if (locked) {
        dtxt("VERROUILLE", x + 6, y + HB_CARD_H - fh(10) - 4, 10,
             (Color){220, 90, 70, 255});
    } else {
        /* Coût : icône or + nombre (alignés sur la hauteur du texte) */
        char cb[16];
        snprintf(cb, sizeof(cb), "%d", cost);
        int cy2 = y + HB_CARD_H - fh(10) - 4;
        int ic  = fh(10) - 4;
        draw_icon(g_icon_gold, x + 6, cy2 + 2, ic,
                  enabled ? WHITE : (Color){150, 130, 90, 255});
        dtxt(cb, x + 6 + ic + 3, cy2, 10,
             enabled ? (Color){238, 200, 92, 255} : (Color){150, 120, 70, 255});
    }
}

/* Rangée contextuelle : types de TOUR en mode placement, RECRUES près
   d'une base. y = bord haut de la rangée. */
static void hero_hotbar(AppContext *ctx, int y) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;

    if (h->place_mode) {
        int n = (int)TOWER_TYPE_COUNT;
        int x0 = g_canvas_virt_w / 2 - (n * HB_CARD_W + (n - 1) * HB_GAP) / 2;
        for (int i = 0; i < n; i++) {
            int locked  = !hero_tower_unlocked(ctx, i);
            int cost    = TOWER_BASE_STATS[i].cost;
            int enabled = !locked && gs->gold >= cost;
            hotbar_card(x0 + i * (HB_CARD_W + HB_GAP), y, (char)('1' + i),
                        TOWER_BASE_STATS[i].name, cost,
                        renderer_tower_color((TowerType)i),
                        h->place_type == i, enabled, locked);
        }
    } else if (hero_in_base_zone(ctx)) {
        int n = (int)UNIT_TYPE_COUNT;
        int x0 = g_canvas_virt_w / 2 - (n * HB_CARD_W + (n - 1) * HB_GAP) / 2;
        for (int i = 0; i < n; i++) {
            int cost    = UNIT_BASE_STATS[i].cost;
            int enabled = (gs->gold >= cost) &&
                          (gs->units.count < gs->units.unit_limit);
            hotbar_card(x0 + i * (HB_CARD_W + HB_GAP), y, (char)('1' + i),
                        UNIT_BASE_STATS[i].name, cost,
                        renderer_unit_color((UnitType)i),
                        0, enabled, 0);
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   HUD 2D (par-dessus la scène 3D, dans le canvas)
   ════════════════════════════════════════════════════════════════ */
void hero_hud(AppContext *ctx) {
    HeroState *h = &ctx->hero;
    GameState *gs = &ctx->gs;
    int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;

    /* ── Vignette de dégâts / PV bas (bords rouges) ────────────────── */
    {
        float ratio = (h->hp_max > 0.0f) ? h->hp / h->hp_max : 0.0f;
        float a = (h->hurt_t > 0.0f) ? 0.55f * (h->hurt_t / HERO_HURT_FLASH_T)
                                     : 0.0f;
        if (ratio < 0.35f) {
            float low = (0.35f - ratio) / 0.35f;
            float pulse = 0.25f + 0.15f * sinf((float)GetTime() * 6.0f);
            if (low * pulse > a) a = low * pulse;
        }
        if (a > 0.0f) {
            unsigned char va = (unsigned char)(a * 200.0f);
            Color vc = {180, 30, 25, va}, v0 = {180, 30, 25, 0};
            int th = g_canvas_virt_h / 6, tw2 = g_canvas_virt_w / 8;
            DrawRectangleGradientV(0, 0, g_canvas_virt_w, th, vc, v0);
            DrawRectangleGradientV(0, g_canvas_virt_h - th, g_canvas_virt_w,
                                   th, v0, vc);
            DrawRectangleGradientH(0, 0, tw2, g_canvas_virt_h, vc, v0);
            DrawRectangleGradientH(g_canvas_virt_w - tw2, 0, tw2,
                                   g_canvas_virt_h, v0, vc);
        }
    }

    /* Bannière VUE TACTIQUE */
    if (h->tactical_view) {
        const char *tb = "VUE TACTIQUE  -  [C] revenir au sol";
        int tw3 = mtxt(tb, 12);
        dtxt(tb, cx - tw3 / 2, 14, 12, (Color){150, 220, 255, 255});
    }

    /* Viseur RÉACTIF : blanc au repos ; ROUGE + écarté sur une cible ;
       flash orange au tir. Masqué en vue tactique. */
    int on = h->aim_on_target && !h->tactical_view;
    if (!h->tactical_view) {
    Color cc = (h->fire_flash > 0.0f) ? (Color){255, 170, 80, 255}
             : on                     ? (Color){255,  85, 60, 255}
                                      : (Color){235, 235, 235, 210};
    int g1 = on ? 5 : 3, g2 = on ? 13 : 9;   /* écartement sur cible */
    DrawLine(cx - g2, cy, cx - g1, cy, cc);
    DrawLine(cx + g1, cy, cx + g2, cy, cc);
    DrawLine(cx, cy - g2, cx, cy - g1, cc);
    DrawLine(cx, cy + g1, cx, cy + g2, cc);
    if (on) DrawCircleLines(cx, cy, 3, cc);  /* point central sur cible */

    /* Hit-marker : 4 encoches diagonales quand le tir a touché */
    if (h->trace_t > 0.0f && h->trace_hit) {
        unsigned char a =
            (unsigned char)(255.0f * (h->trace_t / HERO_TRACE_TIME));
        Color hm = {255, 130, 90, a};
        DrawLine(cx - 12, cy - 12, cx - 6, cy - 6, hm);
        DrawLine(cx + 6,  cy - 6,  cx + 12, cy - 12, hm);
        DrawLine(cx - 12, cy + 12, cx - 6, cy + 6, hm);
        DrawLine(cx + 6,  cy + 6,  cx + 12, cy + 12, hm);
    }
    }   /* fin viseur (masqué en vue tactique) */
    /* ── Bannière de VAGUE (transitions de phase) ─────────────────── */
    if (h->wave_banner_t > 0.0f) {
        float a = h->wave_banner_t / HERO_BANNER_TIME;
        if (a > 1.0f) a = 1.0f;
        int bw2 = mtxt(h->wave_banner, 24);
        Color bc = {245, 200, 90, (unsigned char)(255.0f * a)};
        dtxt(h->wave_banner, cx - bw2 / 2, cy - 130, 24, bc);
        /* Recap P1.1 : sous la banniere « repoussee », les stats de vague */
        if (gs->phase == PHASE_PREP && gs->ui.wave_banner_t > 0.0f) {
            char st[64];
            snprintf(st, sizeof(st), "%d ennemis detruits   +%d or",
                     gs->ui.wave_banner_kills, gs->ui.wave_banner_gold);
            int sw = mtxt(st, 12);
            Color sc2 = {235, 225, 205, (unsigned char)(220.0f * a)};
            dtxt(st, cx - sw / 2, cy - 130 + fh(24) + 4, 12, sc2);
        }
    }

    /* ── MENU RADIAL (P1.4) : molette maintenue ─────────────────────
       7 pastilles autour du centre (3 armes en haut, 4 tours) ; le
       trait orange montre le geste, la pastille visee est accentuee. */
    if (h->radial_open) {
        int rr = HERO_RADIAL_R;
        DrawCircle(cx, cy, (float)rr + 52.0f, (Color){6, 4, 2, 130});

        /* Trait du geste (direction de visee du segment) */
        float gl = sqrtf(h->radial_dx * h->radial_dx +
                         h->radial_dy * h->radial_dy);
        if (gl > 1.0f) {
            float ll = gl > (float)rr ? (float)rr : gl;
            DrawLineEx((Vector2){(float)cx, (float)cy},
                       (Vector2){cx + h->radial_dx / gl * ll,
                                 cy + h->radial_dy / gl * ll},
                       2.0f, (Color){232, 152, 32, 170});
        }
        DrawCircle(cx, cy, 4.0f, (Color){232, 152, 32, 220});

        for (int i = 0; i < HERO_RADIAL_N; i++) {
            float a2  = (float)i * (2.0f * PI / (float)HERO_RADIAL_N);
            int   px2 = cx + (int)(sinf(a2) * (float)rr);
            int   py3 = cy - (int)(cosf(a2) * (float)rr);
            int   cost = -1, locked = 0;
            const char *lb = hero_radial_label(ctx, i, &cost, &locked);
            int   sel2   = (h->radial_sel == i);
            int   is_cur = (i < (int)HW_COUNT && (int)h->weapon == i);

            char cb2[16];
            int  has_cost = (cost >= 0);
            if (has_cost) snprintf(cb2, sizeof(cb2), "%d or", cost);

            int lw2 = mtxt(lb, 11);
            int cw2 = has_cost ? mtxt(cb2, 10) : 0;
            int pw2 = (lw2 > cw2 ? lw2 : cw2) + 16;
            int ph2 = 6 + fh(11) + (has_cost ? fh(10) + 2 : 0) + 6;

            Rectangle r = {(float)(px2 - pw2 / 2), (float)(py3 - ph2 / 2),
                           (float)pw2, (float)ph2};
            Color bg  = sel2   ? (Color){44, 30, 10, 245}
                               : (Color){12,  8,  3, 215};
            Color brd = sel2   ? (Color){232, 152, 32, 255}
                      : locked ? (Color){70, 58, 40, 180}
                               : (Color){110, 90, 55, 220};
            DrawRectangleRounded(r, 0.3f, 4, bg);
            DrawRectangleRoundedLinesEx(r, 0.3f, 4, sel2 ? 2.0f : 1.0f, brd);

            Color tc = locked ? (Color){110, 95, 70, 220}
                     : sel2   ? (Color){250, 225, 170, 255}
                              : (Color){225, 210, 180, 240};
            dtxt(lb, px2 - lw2 / 2, (int)r.y + 6, 11, tc);
            if (has_cost)
                dtxt(cb2, px2 - cw2 / 2, (int)r.y + 6 + fh(11) + 2, 10,
                     locked ? (Color){100, 85, 60, 200}
                            : (Color){215, 175, 70, 240});
            /* Petit point : arme actuellement equipee */
            if (is_cur)
                DrawCircle((int)r.x - 6, py3, 3.0f, (Color){120, 220, 130, 255});
        }

        const char *hint = (h->radial_sel >= 0)
            ? "Relachez la molette pour valider"
            : "Poussez la souris vers un choix";
        int hw2 = mtxt(hint, 10);
        dtxt(hint, cx - hw2 / 2, cy + rr + 58, 10, (Color){190, 175, 145, 220});
    }

    /* ── MINIMAP (bas-droit) : la conscience tactique du TD ──────────
       Carte réduite + tours/unités/ennemis/héros en temps réel.       */
    {
        const Map *map = &gs->map;
        int   mw = HERO_MMAP_W;
        float sc = (float)mw / (float)(map->w * TILE_SIZE);
        int   mh = (int)(map->h * (float)TILE_SIZE * sc);
        int   mx = g_canvas_virt_w - mw - 10;
        int   my = g_canvas_virt_h - mh - 10;
        int   ts = (int)((float)TILE_SIZE * sc) + 1;   /* taille tuile mini */

        DrawRectangle(mx - 3, my - 3, mw + 6, mh + 6, (Color){8, 10, 14, 205});
        DrawRectangleLines(mx - 3, my - 3, mw + 6, mh + 6,
                           (Color){90, 110, 130, 220});
        for (int ty = 0; ty < map->h; ty++) {
            for (int tx = 0; tx < map->w; tx++) {
                Color c;
                switch (map->tiles[ty][tx].type) {
                    case TILE_PATH:  c = (Color){ 96,  76,  52, 255}; break;
                    case TILE_WATER: c = (Color){ 30,  55,  92, 255}; break;
                    case TILE_RUIN:  c = (Color){ 72,  72,  78, 255}; break;
                    case TILE_SPAWN: c = (Color){ 96,  40,  40, 255}; break;
                    case TILE_BASE:  c = (Color){ 52,  84,  56, 255}; break;
                    default:         c = (Color){ 38,  40,  36, 255}; break;
                }
                DrawRectangle(mx + (int)(tx * (float)TILE_SIZE * sc),
                              my + (int)(ty * (float)TILE_SIZE * sc), ts, ts, c);
            }
        }
        for (int d = 0; d < map->deposit_count; d++) {   /* minerais */
            const MaterialDeposit *dep = &map->deposits[d];
            if (!dep->active) continue;
            DrawRectangle(mx + (int)(dep->tile_x * (float)TILE_SIZE * sc),
                          my + (int)(dep->tile_y * (float)TILE_SIZE * sc),
                          ts, ts, MATERIAL_COLORS[dep->type]);
        }
        for (int i = 0; i < MAX_TOWERS; i++) {           /* tours */
            const Tower *tw = &gs->towers.towers[i];
            if (!tw->active) continue;
            DrawRectangle(mx + (int)(tw->cx * sc) - 1,
                          my + (int)(tw->cy * sc) - 1, 3, 3,
                          renderer_tower_color(tw->type));
        }
        for (int i = 0; i < MAX_UNITS; i++) {            /* unités */
            const Unit *u = &gs->units.units[i];
            if (!u->active) continue;
            DrawRectangle(mx + (int)(u->x * sc) - 1,
                          my + (int)(u->y * sc) - 1, 2, 2,
                          renderer_unit_color(u->type));
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {          /* ennemis */
            const Enemy *e = &gs->enemies.enemies[i];
            if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
            int sz = e->is_boss ? 4 : 2;
            DrawRectangle(mx + (int)(e->x * sc) - sz / 2,
                          my + (int)(e->y * sc) - sz / 2, sz, sz,
                          renderer_enemy_color(e->type));
        }
        {   /* héros : point blanc + direction du regard */
            int hx = mx + (int)(h->px * sc), hy = my + (int)(h->py * sc);
            DrawRectangle(hx - 1, hy - 1, 3, 3, (Color){250, 250, 250, 255});
            DrawLine(hx, hy, hx + (int)(sinf(h->yaw) * 7.0f),
                     hy + (int)(cosf(h->yaw) * 7.0f),
                     (Color){250, 250, 250, 220});
        }
    }

    /* ── BANNIÈRE D'OBJECTIF (haut-centre) : guide tant qu'aucune tour ──
       n'est posée. Le placement passe par un ouvrier (repère vert). */
    if (!h->place_mode && gs->towers.tower_count == 0) {
        char kn[24];
        opts_key_name(ctx->menu.opts.hero_keys[HK_INTERACT], kn, sizeof(kn));
        char ob[96];
        snprintf(ob, sizeof(ob),
                 "OBJECTIF : rejoins l'OUVRIER (repere vert) puis [%s] pour batir une tour",
                 kn);
        int ow = mtxt(ob, 11);
        float pulse = (sinf((float)GetTime() * 3.0f) + 1.0f) * 0.5f;
        DrawRectangle(cx - ow/2 - 12, 40, ow + 24, fh(11) + 12,
                      (Color){10, 26, 12, 220});
        DrawRectangleLines(cx - ow/2 - 12, 40, ow + 24, fh(11) + 12,
                           (Color){70, 200, 90, (unsigned char)(160 + pulse*90)});
        dtxt(ob, cx - ow/2, 46, 11, (Color){150, 240, 160, 255});
    }

    /* Stats (haut-gauche) : icônes or/vies + nombres, pop au changement */
    ui_anim_tick();   /* horloge UI (inoffensif si déjà tické cette frame) */
    static int   pgold = -1, plives = -1;
    static float tgold = 1e9f, tlives = 1e9f;
    if (pgold < 0 || plives < 0) { pgold = gs->gold; plives = gs->lives; }
    if (gs->gold  != pgold)  { tgold  = 0.0f; pgold  = gs->gold;  }
    if (gs->lives != plives) { tlives = 0.0f; plives = gs->lives; }
    tgold  += ui_dt();
    tlives += ui_dt();

    char gb[16], vb[16], l2[64];
    snprintf(gb, sizeof(gb), "%d", gs->gold);
    snprintf(vb, sizeof(vb), "%d", gs->lives);
    if (gs->phase == PHASE_PREP) {
        snprintf(l2, sizeof(l2), "VAGUE %d  -  prepa %.0fs",
                 gs->wave_manager.number + 1, gs->wave_manager.prep_timer);
    } else {
        snprintf(l2, sizeof(l2), "VAGUE %d  -  ennemis %d",
                 gs->wave_manager.number, enemy_pool_alive(&gs->enemies));
    }
    int ih  = fh(12);
    int sw1 = ih + 4 + mtxt(gb, 12) + 14 + ih + 4 + mtxt(vb, 12);
    int sw2 = mtxt(l2, 10);
    DrawRectangle(8, 10, (sw1 > sw2 ? sw1 : sw2) + 16,
                  fh(12) + fh(10) + 14, (Color){8, 10, 16, 190});

    /* Icône + valeur, avec pop d'échelle 0.22 s quand la valeur change */
    int xx = 16;
    {
        int isz = ih;
        if (tgold < 0.22f)
            isz = (int)((float)ih * (1.0f + 0.3f * sinf((tgold / 0.22f) * PI)));
        draw_icon(g_icon_gold, xx - (isz - ih) / 2, 12 - (isz - ih) / 2,
                  isz, WHITE);
        dtxt(gb, xx + ih + 4, 12, 12, (Color){240, 210, 120, 255});
        xx += ih + 4 + mtxt(gb, 12) + 14;
    }
    {
        int isz = ih;
        if (tlives < 0.22f)
            isz = (int)((float)ih * (1.0f + 0.3f * sinf((tlives / 0.22f) * PI)));
        Color lc = (gs->lives <= 15) ? (Color){231, 76, 60, 255}
                                     : (Color){235, 120, 110, 255};
        draw_icon(g_icon_heart, xx - (isz - ih) / 2, 12 - (isz - ih) / 2,
                  isz, lc);
        dtxt(vb, xx + ih + 4, 12, 12, lc);
    }
    dtxt(l2, 16, 12 + fh(12) + 4, 10, (Color){200, 210, 225, 255});

    /* Barre de PV du héros (le contact ennemi fait mal, la base soigne) */
    {
        float ratio = (h->hp_max > 0.0f) ? h->hp / h->hp_max : 0.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        int bw3 = 150, bx3 = 16, by3 = 8 + fh(12) + fh(10) + 14 + 4;
        Color hc = (ratio > 0.5f)  ? (Color){ 90, 210,  90, 255}
                 : (ratio > 0.25f) ? (Color){235, 190,  60, 255}
                                   : (Color){225,  70,  55, 255};
        DrawRectangle(bx3 - 1, by3 - 1, bw3 + 2, 10, (Color){10, 10, 14, 220});
        DrawRectangle(bx3, by3, (int)(bw3 * ratio), 8, hc);
    }

    /* Arme (haut-droit) */
    char w1[80];
    snprintf(w1, sizeof(w1), "%s  [dgts %.0f | %.1f/s]  amel. %d/%d",
             hero_weapon_name(h->weapon), hero_weapon_dmg(h),
             hero_weapon_rate(h), h->upg_dmg + h->upg_rate, HERO_UPG_MAX * 2);
    int ww = mtxt(w1, 10);
    DrawRectangle(g_canvas_virt_w - ww - 24, 10, ww + 16, fh(10) + 10,
                  (Color){8, 10, 16, 190});
    dtxt(w1, g_canvas_virt_w - ww - 16, 13, 10, (Color){170, 220, 255, 255});

    /* Invites contextuelles (bas-centre) */
    char lines[6][64];
    int nl = hero_prompts(ctx, lines, 6);
    int py = g_canvas_virt_h - 30 - nl * (fh(10) + 4);

    /* Hotbar de sélection au-dessus des invites */
    hero_hotbar(ctx, py - HB_CARD_H - 10);
    for (int i = 0; i < nl; i++) {
        int lw = mtxt(lines[i], 10);
        DrawRectangle(cx - lw / 2 - 8, py - 2, lw + 16, fh(10) + 5,
                      (Color){8, 10, 16, 170});
        dtxt(lines[i], cx - lw / 2, py, 10, (Color){215, 225, 235, 255});
        py += fh(10) + 4;
    }

    /* Toast */
    if (h->toast_t > 0.0f) {
        int tw = mtxt(h->toast, 11);
        dtxt(h->toast, cx - tw / 2, g_canvas_virt_h - 30 - (nl + 1) * (fh(10) + 4) - fh(11),
             11, h->toast_col);
    }

    /* Aide (H) */
    if (h->show_help) {
        static const char *HL[] = {
            "Deplacement / SAUT / sprint / interactions :",
            "  touches configurables dans OPTIONS > COMMANDES",
            "SOURIS     viser            CLIC G tirer (x1.4 si cible <30% PV)",
            "MOLETTE    (maintenir) MENU RADIAL : armes + tours au geste",
            "C          vue TACTIQUE (drone) pour planifier",
            "E          ouvrier: batir / tour: en prendre le CONTROLE",
            "O / P / L  (tour) ameliorer   M / N  appliquer un materiau",
            "1-5        (pres d'une BASE) recruter une unite",
            "ESC        pause    H  fermer l'aide",
        };
        int n = (int)(sizeof(HL) / sizeof(HL[0]));
        int pw = 480, ph = 20 + n * (fh(10) + 4) + 10;
        int px = cx - pw / 2, py2 = cy - ph / 2;
        DrawRectangle(px, py2, pw, ph, (Color){6, 10, 18, 235});
        DrawRectangleLines(px, py2, pw, ph, (Color){90, 150, 210, 235});
        int ly = py2 + 12;
        for (int i = 0; i < n; i++) {
            dtxt(HL[i], px + 20, ly, 10, (Color){210, 220, 232, 255});
            ly += fh(10) + 4;
        }
    }

    if (gs->ui.show_fps) {
        char fb[16];
        snprintf(fb, sizeof(fb), "%d FPS", GetFPS());
        dtxt(fb, 12, g_canvas_virt_h - fh(10) - 8, 10, (Color){120, 220, 120, 220});
    }
}

/* Voile + panneau centré (pause / défaite) ; retourne le y du contenu. */
int hero_overlay_panel(const char *title, Color col) {
    int cx = g_canvas_virt_w / 2;
    DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h, (Color){0, 0, 0, 170});
    int tw = mtxt(title, 30);
    dtxt(title, cx - tw / 2, g_canvas_virt_h / 2 - 70, 30, col);
    return g_canvas_virt_h / 2 - 70 + fh(30) + 14;
}

/* ════════════════════════════════════════════════════════════════
   VIEWMODEL — l'arme en 1re personne (silhouette par archétype,
   recul au tir, flash de bouche). Dessiné DANS la scène 3D, en
   dernier, hors depth-test (ne clippe jamais dans le décor).
   ════════════════════════════════════════════════════════════════ */
#define HERO_VM_FWD    0.52f   /* distance devant la caméra (monde)     */
#define HERO_VM_RIGHT  0.26f   /* décalage à droite                     */
#define HERO_VM_DOWN   0.20f   /* décalage vers le bas                  */
#define HERO_VM_RECOIL 0.10f   /* recul max au tir (monde)              */

void hero_draw_viewmodel(const HeroState *h, Camera3D cam) {
    if (!h->first_person) return;
    if (h->place_mode) return;     /* arme baissée pendant la construction */
    if (h->control_tower >= 0) return;   /* aux commandes d'une tour       */

    Vector3 f = { cam.target.x - cam.position.x,
                  cam.target.y - cam.position.y,
                  cam.target.z - cam.position.z };
    float fl = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
    if (fl < 1e-5f) return;
    f.x /= fl; f.y /= fl; f.z /= fl;
    /* droite = fwd × up(0,1,0) (normalisée dans le plan sol) */
    Vector3 r = { -f.z, 0.0f, f.x };
    float rl = sqrtf(r.x * r.x + r.z * r.z);
    if (rl < 1e-5f) return;
    r.x /= rl; r.z /= rl;

    float recoil = (h->fire_flash > 0.0f) ? h->fire_flash * HERO_VM_RECOIL * 8.0f
                                          : 0.0f;
    float bob = h->moving ? sinf(h->bob_t) * 0.012f : 0.0f;
    Vector3 base = {
        cam.position.x + f.x * (HERO_VM_FWD - recoil) + r.x * HERO_VM_RIGHT,
        cam.position.y + f.y * (HERO_VM_FWD - recoil) - HERO_VM_DOWN + bob,
        cam.position.z + f.z * (HERO_VM_FWD - recoil) + r.z * HERO_VM_RIGHT,
    };
    float yaw_deg = atan2f(f.x, f.z) * RAD2DEG;
    float elev    = asinf(f.y > 1.0f ? 1.0f : (f.y < -1.0f ? -1.0f : f.y));

    rlDisableDepthTest();
    rlPushMatrix();
        rlTranslatef(base.x, base.y, base.z);
        rlRotatef(yaw_deg, 0.0f, 1.0f, 0.0f);
        rlRotatef(-elev * RAD2DEG, 1.0f, 0.0f, 0.0f);
        /* Repère local : +Z = devant. Silhouette par archétype. */
        switch (h->weapon) {
            case HW_CANNON:   /* canon : fût massif + gueule large      */
                DrawCube((Vector3){0, -0.01f, 0.10f}, 0.10f, 0.10f, 0.30f,
                         (Color){96, 66, 44, 255});
                DrawCylinderEx((Vector3){0, 0, 0.20f}, (Vector3){0, 0, 0.46f},
                               0.055f, 0.065f, 10, (Color){70, 52, 38, 255});
                DrawCylinderEx((Vector3){0, 0, 0.44f}, (Vector3){0, 0, 0.47f},
                               0.075f, 0.075f, 10, (Color){50, 38, 28, 255});
                break;
            case HW_TESLA:    /* arc tesla : corps + bobine + orbe      */
                DrawCube((Vector3){0, -0.01f, 0.10f}, 0.07f, 0.08f, 0.26f,
                         (Color){44, 62, 82, 255});
                DrawCylinderEx((Vector3){0, 0, 0.20f}, (Vector3){0, 0, 0.38f},
                               0.045f, 0.03f, 8, (Color){70, 95, 120, 255});
                DrawSphere((Vector3){0, 0, 0.40f}, 0.035f,
                           (Color){120, 210, 245, 255});
                break;
            default:          /* fusil : corps fin + canon long + crosse */
                DrawCube((Vector3){0, -0.015f, 0.06f}, 0.05f, 0.07f, 0.22f,
                         (Color){68, 72, 82, 255});
                DrawCylinderEx((Vector3){0, 0.01f, 0.16f},
                               (Vector3){0, 0.01f, 0.50f},
                               0.018f, 0.018f, 8, (Color){52, 56, 64, 255});
                DrawCube((Vector3){0, -0.05f, -0.05f}, 0.045f, 0.08f, 0.09f,
                         (Color){84, 62, 40, 255});
                break;
        }
        /* Flash de bouche bref */
        if (h->fire_flash > 0.06f)
            DrawSphere((Vector3){0, 0, 0.52f}, 0.045f,
                       (Color){255, 215, 130, 240});
    rlPopMatrix();
    rlEnableDepthTest();
}

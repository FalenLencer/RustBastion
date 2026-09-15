/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero_tower.c ─ MODE HÉROS : interaction avec les TOURS.
 *  - CONTRÔLE MANUEL ([E] près d'une tour) : caméra sur la tourelle, la
 *    souris VISE (tw->angle suit → le modèle 3D pivote), clic gauche TIRE
 *    via projectile_spawn → tous les comportements de la tour s'appliquent
 *    (dégâts typés, matériaux, splash, chaîne, ralenti) ET l'animation de
 *    tir 3D se déclenche (front de fire_timer). Le tir AUTO est suspendu
 *    (g_tower_manual_control) ; le cooldown reste celui de la tour.
 *  - AMÉLIORATIONS ([O]/[P]/[L] près d'une tour ou en contrôle) : mêmes
 *    paliers/coûts que le HUD 2D (tower_upg_*).
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"
#include "../ui/hud_internal.h"     /* hud_counter_advice (conseil P0.3)  */
#include "../combat/projectile.h"
#include "../engine/audio.h"
#include <math.h>
#include <stdio.h>

/* ── Tour active la plus proche du héros ───────────────────────── */
int hero_tower_near(AppContext *ctx) {
    const GameState *gs = &ctx->gs;
    const HeroState *h  = &ctx->hero;
    float best = HERO_TOWER_ZONE_TILES * (float)TILE_SIZE;
    int   idx  = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &gs->towers.towers[i];
        if (!tw->active) continue;
        float dx = h->px - tw->cx, dy = h->py - tw->cy;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d < best) { best = d; idx = i; }
    }
    return idx;
}

/* ── Sortie propre du contrôle ─────────────────────────────────── */
static void hero_tower_release(HeroState *h) {
    h->control_tower       = -1;
    g_tower_manual_control = -1;
}

/* ── Améliorations (mutualisées : proximité ET contrôle) ───────── */
static void hero_tower_upgrades(AppContext *ctx, Tower *tw) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;

    if (IsKeyPressed(KEY_O)) {
        int c = tower_upg_next_cost_dmg(tw);
        if (c < 0) {
            hero_toast(h, "Degats deja au maximum", (Color){180, 180, 190, 255});
        } else if (gs->gold < c) {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        } else {
            gs->gold -= c;
            tower_upgrade_dmg(tw);
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            hero_toast(h, "Tour : degats ameliores !", (Color){120, 220, 130, 255});
        }
    }
    if (IsKeyPressed(KEY_P)) {
        int c = tower_upg_next_cost_rate(tw);
        if (c < 0) {
            hero_toast(h, "Cadence deja au maximum", (Color){180, 180, 190, 255});
        } else if (gs->gold < c) {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        } else {
            gs->gold -= c;
            tower_upgrade_rate(tw);
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            hero_toast(h, "Tour : cadence amelioree !", (Color){120, 220, 130, 255});
        }
    }
    if (IsKeyPressed(KEY_L)) {
        int c = tower_upg_next_cost_range(tw);
        if (c < 0) {
            hero_toast(h, "Portee deja au maximum", (Color){180, 180, 190, 255});
        } else if (gs->gold < c) {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        } else {
            gs->gold -= c;
            tower_upgrade_range(tw);
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            hero_toast(h, "Tour : portee amelioree !", (Color){120, 220, 130, 255});
        }
    }
}

/* ── Matériaux : [N] choisit dans l'inventaire, [M] applique ─────
   Parité avec le 2D : c'est LE système de contres du jeu — il doit
   être jouable depuis le mode héros. */
static void hero_tower_materials(AppContext *ctx, Tower *tw) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    if (gs->inventory_count <= 0) return;
    if (h->mat_sel >= gs->inventory_count) h->mat_sel = 0;

    if (IsKeyPressed(KEY_N)) {
        h->mat_sel = (h->mat_sel + 1) % gs->inventory_count;
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
    }
    if (IsKeyPressed(KEY_M)) {
        MaterialType mt = gs->inventory[h->mat_sel];
        tower_set_material(tw, mt);
        for (int k = h->mat_sel; k < gs->inventory_count - 1; k++)
            gs->inventory[k] = gs->inventory[k + 1];
        gs->inventory_count--;
        if (h->mat_sel >= gs->inventory_count) h->mat_sel = 0;
        audio_play_sfx(AUDIO_SFX_MATERIAL_APPLY);
        char nb[64];
        snprintf(nb, sizeof(nb), "Materiau applique : %s", MATERIAL_NAMES[mt]);
        hero_toast(h, nb, (Color){120, 210, 245, 255});
    }
}

/* ── MODE CONTRÔLE (retourne 1 si actif : l'appelant s'arrête là) ─ */
int hero_tower_update(AppContext *ctx) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    if (h->control_tower < 0) return 0;

    Tower *tw = &gs->towers.towers[h->control_tower];
    if (!tw->active) {                     /* détruite sous nos pieds */
        hero_tower_release(h);
        hero_toast(h, "Tour detruite !", (Color){231, 76, 60, 255});
        return 0;
    }

    /* La souris VISE : tw->angle suit le cap caméra → la sim (portée
       affichée) et le modèle 3D suivent la même visée. */
    tw->angle = atan2f(cosf(h->yaw), sinf(h->yaw));

    /* [E] relâche le contrôle */
    if (IsKeyPressed(ctx->menu.opts.hero_keys[HK_INTERACT])) {
        hero_tower_release(h);
        hero_toast(h, "Controle relache", (Color){170, 215, 255, 255});
        return 1;
    }

    hero_tower_upgrades(ctx, tw);
    hero_tower_materials(ctx, tw);

    /* Réticule : ennemi visé DANS LA PORTÉE de la tour */
    float range = tw->range * (float)TILE_SIZE * HERO_CTRL_RANGE_MULT
                * W3D_PER_PX;              /* → unités monde */
    h->aim_on_target = (hero_aim_pick(ctx, range, NULL, NULL) >= 0);

    /* CLIC G : tir manuel via le pipeline de la tour (cooldown de la tour) */
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && tw->fire_timer <= 0.0f) {
        int hit = hero_aim_pick(ctx, range, NULL, NULL);
        if (hit >= 0) {
            projectile_spawn(&gs->towers, tw, hit, &gs->enemies);
            tw->fire_timer = 1.0f / tw->fire_rate;
            audio_play_sfx(AUDIO_SFX_TOWER_FIRE_GUN + (int)tw->type);
            h->fire_flash = 0.12f;         /* feedback réticule */
        }
    }
    return 1;
}

/* ── Hors contrôle, près d'une tour : entrer + améliorer ────────── */
void hero_tower_interact_near(AppContext *ctx, int tower_idx) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    if (tower_idx < 0) return;
    Tower *tw = &gs->towers.towers[tower_idx];

    if (IsKeyPressed(ctx->menu.opts.hero_keys[HK_INTERACT])) {
        h->control_tower       = tower_idx;
        h->ctrl_px             = tw->cx;
        h->ctrl_py             = tw->cy;
        g_tower_manual_control = tower_idx;
        hero_toast(h, "Controle de la tour - CLIC G pour tirer",
                   (Color){170, 215, 255, 255});
        return;
    }
    hero_tower_upgrades(ctx, tw);
    hero_tower_materials(ctx, tw);
}

/* ── Invites HUD ────────────────────────────────────────────────── */
int hero_tower_prompts(AppContext *ctx, char out[][64], int max, int n) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    char ke[24];
    opts_key_name(ctx->menu.opts.hero_keys[HK_INTERACT], ke, sizeof(ke));

    if (h->control_tower >= 0) {
        Tower *tw = &gs->towers.towers[h->control_tower];
        if (n < max)
            snprintf(out[n++], 64, "TOUR : [CLIC G] tirer   [%s] relacher", ke);
        if (n < max) {
            int cd = tower_upg_next_cost_dmg(tw);
            int cr = tower_upg_next_cost_rate(tw);
            int cp = tower_upg_next_cost_range(tw);
            snprintf(out[n++], 64, "[O]+dgts %s   [P]+cadence %s   [L]+portee %s",
                     cd < 0 ? "MAX" : TextFormat("%dor", cd),
                     cr < 0 ? "MAX" : TextFormat("%dor", cr),
                     cp < 0 ? "MAX" : TextFormat("%dor", cp));
        }
        return n;
    }

    int ti = hero_tower_near(ctx);
    if (ti >= 0 && !h->place_mode) {
        if (n < max)
            snprintf(out[n++], 64, "[%s] CONTROLER la tour   [O/P/L] ameliorer", ke);
        if (gs->inventory_count > 0 && n < max) {
            int sel = (h->mat_sel < gs->inventory_count) ? h->mat_sel : 0;
            snprintf(out[n++], 64, "[M] appliquer %s (%d/%d)   [N] suivant",
                     MATERIAL_NAMES[gs->inventory[sel]],
                     sel + 1, gs->inventory_count);
            /* P0.3 — conseil de contre pour CHOISIR le bon minerai */
            if (n < max) {
                char adv[64];
                if (hud_counter_advice(gs, adv, sizeof(adv)))
                    snprintf(out[n++], 64, "%s", adv);
            }
        }
    }
    return n;
}

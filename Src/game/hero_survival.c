/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero_survival.c ─ MODE HÉROS : le héros n'est PLUS invulnérable.
 *  Les ennemis au CONTACT l'usent (DPS proportionnel à leur puissance),
 *  la zone de base le régénère, et à 0 PV il tombe K.O. quelques
 *  secondes avant de réapparaître à la base primaire — une pénalité de
 *  temps, pas un game-over (la défaite reste la chute des bases).
 *  C'est l'arbitrage central du mode : OÙ mettre son corps ?
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"
#include "../ui/renderer.h"      /* g_canvas_virt_w                       */
#include "../ui/ui_utils.h"      /* dtxt / mtxt                           */
#include "../combat/fx.h"
#include "../engine/audio.h"
#include <math.h>
#include <stdio.h>

/* Tuile praticable la plus proche du centre de la base primaire :
   utilisée au spawn initial ET au respawn après K.O. */
void hero_find_spawn(AppContext *ctx, float *out_px, float *out_py) {
    GameState *gs = &ctx->gs;
    float px = gs->units.base_px;
    float py = gs->units.base_py + (float)TILE_SIZE * 1.5f;

    /* La case visée convient ? (mêmes règles que la collision héros :
       on teste via la carte — eau/ruines/base/tour bloquent) */
    int btx = (int)(gs->units.base_px / TILE_SIZE);
    int bty = (int)(gs->units.base_py / TILE_SIZE);
    int stx = (int)(px / TILE_SIZE), sty = (int)(py / TILE_SIZE);
    if (!move_tile_blocked(&gs->map, &gs->towers, stx, sty, MOVE_F_ALLY)) {
        *out_px = px; *out_py = py;
        return;
    }
    for (int r = 1; r <= 6; r++) {
        for (int oy = -r; oy <= r; oy++) {
            for (int ox = -r; ox <= r; ox++) {
                if (ox > -r && ox < r && oy > -r && oy < r) continue;
                if (!move_tile_blocked(&gs->map, &gs->towers,
                                       btx + ox, bty + oy, MOVE_F_ALLY)) {
                    *out_px = (btx + ox + 0.5f) * (float)TILE_SIZE;
                    *out_py = (bty + oy + 0.5f) * (float)TILE_SIZE;
                    return;
                }
            }
        }
    }
    *out_px = px; *out_py = py;   /* pire cas : l'anti-blocage fera le reste */
}

/* Le héros est-il dans la zone de régénération d'une base active ? */
static int hero_in_regen_zone(const AppContext *ctx) {
    const GameState *gs = &ctx->gs;
    const HeroState *h  = &ctx->hero;
    float zone = HERO_BASE_ZONE_TILES * (float)TILE_SIZE;
    for (int b = 0; b < gs->map.base_count; b++) {
        if (!gs->map.bases[b].active) continue;
        float bx = (gs->map.bases[b].pos.x + 0.5f) * (float)TILE_SIZE;
        float by = (gs->map.bases[b].pos.y + 0.5f) * (float)TILE_SIZE;
        float dx = h->px - bx, dy = h->py - by;
        if (dx * dx + dy * dy < zone * zone) return 1;
    }
    return 0;
}

void hero_survival_update(AppContext *ctx, float dt) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;

    if (h->hurt_t > 0.0f) h->hurt_t -= dt;
    if (h->down) return;              /* K.O. géré par hero_frame        */

    /* ── Dégâts de CONTACT : chaque ennemi collé au héros l'use ────── */
    float dps = 0.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &gs->enemies.enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        float reach = e->size + HERO_RADIUS_PX + HERO_CONTACT_RANGE_PX;
        float dx = e->x - h->px, dy = e->y - h->py;
        if (dx * dx + dy * dy <= reach * reach)
            dps += HERO_CONTACT_BASE_DPS +
                   HERO_CONTACT_DMG_DPS * (float)e->damage;
    }
    if (dps > 0.0f) {
        h->hp     -= dps * dt;
        h->hurt_t  = HERO_HURT_FLASH_T;
    } else if (hero_in_regen_zone(ctx) && h->hp < h->hp_max) {
        /* Régénération uniquement à l'abri d'une base */
        h->hp += HERO_REGEN_BASE_PS * dt;
        if (h->hp > h->hp_max) h->hp = h->hp_max;
    }

    /* ── K.O. : pénalité de temps, pas de game-over ────────────────── */
    if (h->hp <= 0.0f) {
        h->hp    = 0.0f;
        h->down  = 1;
        h->ko_t  = HERO_KO_TIME;
        /* Lâche tout : tour pilotée, placement en cours. */
        h->control_tower       = -1;
        g_tower_manual_control = -1;
        h->place_mode          = 0;
        hero_toast(h, "A TERRE ! Retour a la base...",
                   (Color){231, 76, 60, 255});
        audio_play_sfx(AUDIO_SFX_GAME_OVER);
    }
}

/* Frame complète pendant le K.O. : la SIM CONTINUE (les tours tiennent
   sans toi — c'est la punition), compte à rebours puis respawn. */
void hero_down_frame(AppContext *ctx, float dt) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;

    game_state_update(gs, dt);
    fx_update(dt);
    h->ko_t -= dt;

    Camera3D kcam = hero_camera(h);
    render3d_world_render(ctx, kcam);
    hero_draw_world_popups(ctx, kcam);
    int y = hero_overlay_panel("A TERRE", (Color){231, 76, 60, 255});
    char kb[48];
    snprintf(kb, sizeof(kb), "Retour a la base dans %.0f s...",
             (double)(h->ko_t > 0.0f ? h->ko_t : 0.0f));
    dtxt(kb, g_canvas_virt_w / 2 - mtxt(kb, 12) / 2, y, 12,
         (Color){220, 200, 180, 255});

    if (h->ko_t <= 0.0f) {                  /* respawn à la base */
        hero_find_spawn(ctx, &h->px, &h->py);
        h->hz = 0.0f; h->vz = 0.0f; h->on_ground = 1;
        h->hp   = h->hp_max * HERO_RESPAWN_HP_FRAC;
        h->down = 0;
        hero_toast(h, "De retour au combat !", (Color){120, 220, 130, 255});
    }
}

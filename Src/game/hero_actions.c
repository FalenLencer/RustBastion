/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hero_actions.c ─ MODE HÉROS : arme (tir typé via le chokepoint commun),
 *  zones d'interaction (base : recruter/vague/arme ; ouvrier : construire),
 *  placement de tour et invites contextuelles du HUD.
 */
#include "hero.h"
#include "app.h"
#include "../ui/render3d_world.h"
#include "../combat/projectile.h"   /* combat_apply_damage (chokepoint)   */
#include "../engine/audio.h"
#include <math.h>
#include <stdio.h>

/* ── Armes : 3 archétypes roguelite ────────────────────────────── */
typedef struct {
    const char *name;
    float       dmg;          /* dégâts par coup (base)               */
    float       rate;         /* tirs par seconde (base)              */
    float       range_tiles;  /* portée (tuiles)                      */
    DamageType  dtype;        /* type → résistances/synergies         */
    float       splash_tiles; /* >0 : zone autour de l'impact         */
    int         chain;        /* >0 : rebonds sur ennemis proches     */
    AudioSfxID  sfx;
} HeroWeaponDef;

#define HW_SPLASH_FRAC  0.60f   /* dégâts de zone = 60 % du coup       */
#define HW_CHAIN_FRAC   0.70f   /* dégâts de rebond = 70 % du coup     */
#define HW_CHAIN_TILES  2.2f    /* portée de rebond (tuiles)           */
#define HW_HIT_RADIUS   0.42f   /* marge de visée (unités monde)       */

static const HeroWeaponDef HW_DEFS[HW_COUNT] = {
    [HW_RIFLE]  = { "Fusil",     22.0f, 3.2f, 14.0f, DMG_PHYSICAL,
                    0.0f, 0, AUDIO_SFX_TOWER_FIRE_GUN },
    [HW_CANNON] = { "Canon",     48.0f, 0.9f, 10.0f, DMG_FIRE,
                    1.6f, 0, AUDIO_SFX_TOWER_FIRE_SNIPER },
    [HW_TESLA]  = { "Arc Tesla", 15.0f, 2.1f,  9.0f, DMG_ELECTRIC,
                    0.0f, 2, AUDIO_SFX_TOWER_FIRE_TESLA },
};

const char *hero_weapon_name(HeroWeapon w) {
    return (w >= 0 && w < HW_COUNT) ? HW_DEFS[w].name : "?";
}
float hero_weapon_dmg(const HeroState *h) {
    return HW_DEFS[h->weapon].dmg * (1.0f + HERO_UPG_DMG_STEP * (float)h->upg_dmg);
}
float hero_weapon_rate(const HeroState *h) {
    return HW_DEFS[h->weapon].rate * (1.0f + HERO_UPG_RATE_STEP * (float)h->upg_rate);
}

/* ── Zones d'interaction ───────────────────────────────────────── */
/* Base active la plus proche dans le rayon. Retourne l'index, -1 sinon. */
static int hero_near_base(const AppContext *ctx, float *bpx, float *bpy) {
    const GameState *gs = &ctx->gs;
    const HeroState *h  = &ctx->hero;
    float best = HERO_BASE_ZONE_TILES * (float)TILE_SIZE;
    int   idx  = -1;
    for (int b = 0; b < gs->map.base_count; b++) {
        if (!gs->map.bases[b].active) continue;
        float px = (gs->map.bases[b].pos.x + 0.5f) * (float)TILE_SIZE;
        float py = (gs->map.bases[b].pos.y + 0.5f) * (float)TILE_SIZE;
        float d  = sqrtf((h->px - px) * (h->px - px) + (h->py - py) * (h->py - py));
        if (d < best) { best = d; idx = b; if (bpx) *bpx = px; if (bpy) *bpy = py; }
    }
    return idx;
}

/* Un ouvrier actif est-il à portée ? (thème : c'est LUI qui bâtit) */
static int hero_near_worker(const AppContext *ctx) {
    const GameState *gs = &ctx->gs;
    const HeroState *h  = &ctx->hero;
    float zone = HERO_WORKER_ZONE_TILES * (float)TILE_SIZE;
    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *u = &gs->units.units[i];
        if (!u->active || u->type != UNIT_WORKER) continue;
        float d = sqrtf((h->px - u->x) * (h->px - u->x) +
                        (h->py - u->y) * (h->py - u->y));
        if (d < zone) return 1;
    }
    return 0;
}

/* ── Visée : ennemi sous le réticule (rayon caméra centre-écran) ──
   Réutilisé par le TIR et par le VISEUR RÉACTIF. Retourne l'indice
   d'ennemi (-1 si rien) ; remplit ray/dist si demandés. */
static int hero_aim_pick(AppContext *ctx, float range,
                         Ray *ray_out, float *dist_out) {
    const HeroState *h  = &ctx->hero;
    const GameState *gs = &ctx->gs;

    Camera3D cam = hero_camera(h);
    Vector3 dir = { cam.target.x - cam.position.x,
                    cam.target.y - cam.position.y,
                    cam.target.z - cam.position.z };
    float dl = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (dl < 1e-5f) return -1;
    dir.x /= dl; dir.y /= dl; dir.z /= dl;
    Ray ray = { cam.position, dir };

    int   hit  = -1;
    float best = range;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &gs->enemies.enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        Vector3 c = w3d_from_sim(e->x, e->y, W3D_ENEMY_BODY_H);
        float   r = e->size * W3D_PER_PX + HW_HIT_RADIUS;
        RayCollision rc = GetRayCollisionSphere(ray, c, r);
        if (rc.hit && rc.distance > 0.0f && rc.distance < best) {
            best = rc.distance;
            hit  = i;
        }
    }
    if (ray_out)  *ray_out  = ray;
    if (dist_out) *dist_out = best;
    return hit;
}

/* ── Tir : dégâts typés via le chokepoint commun ────────────────── */
static void hero_fire(AppContext *ctx) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    const HeroWeaponDef *wd = &HW_DEFS[h->weapon];

    float range = wd->range_tiles * W3D_TILE;
    Ray   ray  = { {0, 0, 0}, {0, 0, 1} };   /* défaut sûr si visée dégénérée */
    float best = range;
    int   hit  = hero_aim_pick(ctx, range, &ray, &best);
    Vector3 dir = ray.direction;
    Camera3D cam = hero_camera(h);

    /* Traceur : bouche (devant la caméra) → impact ou portée max */
    h->trace_a = (Vector3){ cam.position.x + dir.x * 0.8f,
                            cam.position.y + dir.y * 0.8f - 0.12f,
                            cam.position.z + dir.z * 0.8f };
    h->trace_b = (Vector3){ cam.position.x + dir.x * best,
                            cam.position.y + dir.y * best,
                            cam.position.z + dir.z * best };
    h->trace_t    = HERO_TRACE_TIME;
    h->trace_hit  = (hit >= 0);
    h->fire_flash = 0.12f;
    h->fire_cd = 1.0f / hero_weapon_rate(h);
    audio_play_sfx(wd->sfx);

    if (hit < 0) return;
    Enemy *e = &gs->enemies.enemies[hit];
    float dmg = hero_weapon_dmg(h);
    combat_apply_damage(e, dmg, wd->dtype);

    /* Canon : zone autour de l'impact */
    if (wd->splash_tiles > 0.0f) {
        float r2 = wd->splash_tiles * (float)TILE_SIZE;
        r2 *= r2;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (i == hit) continue;
            Enemy *e2 = &gs->enemies.enemies[i];
            if (!e2->active || e2->dead || e2->spawn_delay > 0.0f) continue;
            float dx = e2->x - e->x, dy = e2->y - e->y;
            if (dx * dx + dy * dy <= r2)
                combat_apply_damage(e2, dmg * HW_SPLASH_FRAC, wd->dtype);
        }
    }

    /* Tesla : rebonds sur les plus proches */
    if (wd->chain > 0) {
        float cr2 = HW_CHAIN_TILES * (float)TILE_SIZE;
        cr2 *= cr2;
        int   done = 0;
        float fx = e->x, fy = e->y;
        for (int hop = 0; hop < wd->chain; hop++) {
            int   nb = -1;
            float bd = cr2;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (i == hit) continue;
                Enemy *e2 = &gs->enemies.enemies[i];
                if (!e2->active || e2->dead || e2->spawn_delay > 0.0f) continue;
                float dx = e2->x - fx, dy = e2->y - fy;
                float d2 = dx * dx + dy * dy;
                if (d2 < bd && e2->hit_flash <= 0.0f) { bd = d2; nb = i; }
            }
            if (nb < 0) break;
            Enemy *e2 = &gs->enemies.enemies[nb];
            combat_apply_damage(e2, dmg * HW_CHAIN_FRAC, wd->dtype);
            fx = e2->x; fy = e2->y;
            done++;
        }
        (void)done;
    }
}

/* ── Placement : case au sol visée par la caméra ───────────────── */
static void hero_place_update(AppContext *ctx) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;

    Camera3D cam = hero_camera(h);
    Vector3 dir = { cam.target.x - cam.position.x,
                    cam.target.y - cam.position.y,
                    cam.target.z - cam.position.z };
    float dl = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (dl < 1e-5f) { h->place_ok = 0; return; }
    dir.x /= dl; dir.y /= dl; dir.z /= dl;

    /* Intersection avec le plan du sol (y = 0) */
    if (dir.y > -1e-4f) { h->place_ok = 0; return; }   /* on vise le ciel */
    float t  = -cam.position.y / dir.y;
    float wx = cam.position.x + dir.x * t;
    float wz = cam.position.z + dir.z * t;
    float px = wx / W3D_PER_PX, py = wz / W3D_PER_PX;

    /* Portée de placement autour du héros */
    float dx = px - h->px, dy = py - h->py;
    float maxd = HERO_PLACE_DIST_TILES * (float)TILE_SIZE;
    if (dx * dx + dy * dy > maxd * maxd) { h->place_ok = 0; }

    h->place_tx = (int)(px / TILE_SIZE);
    h->place_ty = (int)(py / TILE_SIZE);
    if (h->place_tx < 0 || h->place_ty < 0 ||
        h->place_tx >= gs->map.w || h->place_ty >= gs->map.h) {
        h->place_ok = 0;
        return;
    }
    if (dx * dx + dy * dy > maxd * maxd) return;       /* hors portée */

    int cost = tower_cost_on_tile((TowerType)h->place_type, &gs->map,
                                  h->place_tx, h->place_ty);
    h->place_ok = tower_can_place(&gs->towers, &gs->map,
                                  h->place_tx, h->place_ty)
               && gs->towers.tower_count < gs->towers.tower_limit
               && gs->gold >= cost;
}

/* ── Boucle d'interactions (appelée par hero_input) ────────────── */
void hero_actions_update(AppContext *ctx, float dt) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    const int *hk = ctx->menu.opts.hero_keys;   /* touches configurables */
    (void)dt;

    /* Viseur réactif : un ennemi est-il sous le réticule ce frame ? */
    h->aim_on_target =
        (hero_aim_pick(ctx, HW_DEFS[h->weapon].range_tiles * W3D_TILE,
                       NULL, NULL) >= 0);

    float bpx = 0.0f, bpy = 0.0f;
    int near_base   = hero_near_base(ctx, &bpx, &bpy);
    int near_worker = hero_near_worker(ctx);

    /* ── MODE PLACEMENT (prioritaire sur tout le reste) ─────────── */
    if (h->place_mode) {
        hero_place_update(ctx);
        if (IsKeyPressed(KEY_ONE))   h->place_type = (int)TOWER_GUN;
        if (IsKeyPressed(KEY_TWO))   h->place_type = (int)TOWER_SNIPER;
        if (IsKeyPressed(KEY_THREE)) h->place_type = (int)TOWER_FLAME;
        if (IsKeyPressed(KEY_FOUR))  h->place_type = (int)TOWER_TESLA;
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            h->place_mode = 0;
            return;
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && h->place_ok) {
            if (tower_place(&gs->towers, (TowerType)h->place_type,
                            h->place_tx, h->place_ty,
                            &gs->map, &gs->gold, &gs->bonuses)) {
                audio_play_sfx(AUDIO_SFX_TOWER_PLACE_GUN + h->place_type);
                hero_toast(h, "Tour construite !", (Color){120, 220, 130, 255});
                h->place_mode = 0;
            }
        }
        return;   /* pas de tir ni d'achats pendant le placement */
    }

    /* ── TIR ────────────────────────────────────────────────────── */
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && h->fire_cd <= 0.0f)
        hero_fire(ctx);

    /* ── OUVRIER : entrer en mode construction ──────────────────── */
    if (near_worker && IsKeyPressed(hk[HK_INTERACT])) {
        h->place_mode = 1;
        hero_place_update(ctx);
        return;
    }

    /* ── BASE : vague / recruter / arme / améliorations ─────────── */
    if (near_base < 0) return;

    if (gs->phase == PHASE_PREP && !gs->wave_manager.lock_manual &&
        IsKeyPressed(hk[HK_WAVE])) {
        int bonus = wave_early_launch_bonus(&gs->wave_manager);
        gs->gold += bonus;
        wave_start(&gs->wave_manager);
        gs->phase = PHASE_WAVE;
        char nb[48];
        snprintf(nb, sizeof(nb), "Vague lancee ! +%d or", bonus);
        hero_toast(h, nb, (Color){230, 155, 35, 255});
    }

    /* Recruter (1-5) — dépensé via unit_spawn_at (mêmes règles) */
    int buy = -1;
    if      (IsKeyPressed(KEY_ONE))   buy = (int)UNIT_SOLDIER;
    else if (IsKeyPressed(KEY_TWO))   buy = (int)UNIT_HEAVY;
    else if (IsKeyPressed(KEY_THREE)) buy = (int)UNIT_MEDIC;
    else if (IsKeyPressed(KEY_FOUR))  buy = (int)UNIT_DOG;
    else if (IsKeyPressed(KEY_FIVE))  buy = (int)UNIT_WORKER;
    if (buy >= 0) {
        if (gs->units.count >= gs->units.unit_limit) {
            hero_toast(h, "Limite d'unites atteinte !", (Color){231, 76, 60, 255});
        } else if (buy == (int)UNIT_MEDIC &&
                   unit_medic_count_at_base(&gs->units, bpx, bpy)
                       >= MAX_MEDICS_PER_BASE) {
            hero_toast(h, "Max 4 medics par base !", (Color){231, 76, 60, 255});
        } else if (unit_spawn_at(&gs->units, (UnitType)buy, &gs->gold,
                                 &gs->bonuses, bpx, bpy)) {
            audio_play_sfx(AUDIO_SFX_UNIT_SPAWN);
            char nb[64];
            snprintf(nb, sizeof(nb), "%s recrute !", UNIT_BASE_STATS[buy].name);
            hero_toast(h, nb, (Color){120, 220, 130, 255});
        } else {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        }
    }

    /* Changer d'arme (PREP uniquement : choix de build, pas de swap en combat) */
    if (gs->phase == PHASE_PREP && IsKeyPressed(hk[HK_WEAPON])) {
        h->weapon = (HeroWeapon)(((int)h->weapon + 1) % (int)HW_COUNT);
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        char nb[64];
        snprintf(nb, sizeof(nb), "Arme : %s", hero_weapon_name(h->weapon));
        hero_toast(h, nb, (Color){170, 220, 255, 255});
    }

    /* Améliorations roguelite : [O] dégâts, [P] cadence */
    if (IsKeyPressed(KEY_O) && h->upg_dmg < HERO_UPG_MAX) {
        int cost = HERO_UPG_COST_BASE * (h->upg_dmg + 1);
        if (gs->gold >= cost) {
            gs->gold -= cost;
            h->upg_dmg++;
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            hero_toast(h, "Degats de l'arme ameliores !", (Color){120, 220, 130, 255});
        } else {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        }
    }
    if (IsKeyPressed(KEY_P) && h->upg_rate < HERO_UPG_MAX) {
        int cost = HERO_UPG_COST_BASE * (h->upg_rate + 1);
        if (gs->gold >= cost) {
            gs->gold -= cost;
            h->upg_rate++;
            audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            hero_toast(h, "Cadence de l'arme amelioree !", (Color){120, 220, 130, 255});
        } else {
            hero_toast(h, "Or insuffisant !", (Color){243, 156, 18, 255});
        }
    }
}

/* ── Invites contextuelles pour le HUD ─────────────────────────── */
int hero_prompts(AppContext *ctx, char out[][64], int max) {
    HeroState *h  = &ctx->hero;
    GameState *gs = &ctx->gs;
    int n = 0;

    if (h->place_mode) {
        if (n < max) {
            int cost = tower_cost_on_tile((TowerType)h->place_type, &gs->map,
                                          h->place_tx, h->place_ty);
            snprintf(out[n++], 64, "PLACEMENT : %s (%d or) %s",
                     TOWER_BASE_STATS[h->place_type].name, cost,
                     h->place_ok ? "- CLIC G pour poser" : "- invalide ici");
        }
        if (n < max)
            snprintf(out[n++], 64, "[1-4] type de tour   [CLIC D / ESC] annuler");
        return n;
    }

    const int *hk = ctx->menu.opts.hero_keys;
    char kw[24], ki[24], kb[24];
    opts_key_name(hk[HK_WAVE],     kw, sizeof(kw));
    opts_key_name(hk[HK_INTERACT], ki, sizeof(ki));
    opts_key_name(hk[HK_WEAPON],   kb, sizeof(kb));

    float bpx, bpy;
    if (hero_near_base(ctx, &bpx, &bpy) >= 0) {
        if (gs->phase == PHASE_PREP && !gs->wave_manager.lock_manual && n < max)
            snprintf(out[n++], 64, "[%s] LANCER LA VAGUE (+%d or)", kw,
                     wave_early_launch_bonus(&gs->wave_manager));
        if (n < max)
            snprintf(out[n++], 64,
                     "[1]Soldat %d  [2]Lourd %d  [3]Medic %d  [4]Chien %d  [5]Ouvrier %d",
                     UNIT_BASE_STATS[UNIT_SOLDIER].cost,
                     UNIT_BASE_STATS[UNIT_HEAVY].cost,
                     UNIT_BASE_STATS[UNIT_MEDIC].cost,
                     UNIT_BASE_STATS[UNIT_DOG].cost,
                     UNIT_BASE_STATS[UNIT_WORKER].cost);
        if (n < max) {
            char up[64] = "";
            if (h->upg_dmg  < HERO_UPG_MAX)
                snprintf(up, sizeof(up), "[O] +degats %d or   ",
                         HERO_UPG_COST_BASE * (h->upg_dmg + 1));
            char up2[64] = "";
            if (h->upg_rate < HERO_UPG_MAX)
                snprintf(up2, sizeof(up2), "[P] +cadence %d or",
                         HERO_UPG_COST_BASE * (h->upg_rate + 1));
            snprintf(out[n++], 64, "%s%s", up, up2);
        }
        if (gs->phase == PHASE_PREP && n < max)
            snprintf(out[n++], 64, "[%s] changer d'arme", kb);
    } else if (hero_near_worker(ctx)) {
        if (n < max)
            snprintf(out[n++], 64, "[%s] ouvrier : CONSTRUIRE une tour", ki);
    }
    return n;
}

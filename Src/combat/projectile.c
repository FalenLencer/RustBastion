/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "projectile.h"
#include "combat_math.h"
#include "../engine/audio.h"
#include <math.h>
#include <stddef.h>

static void apply_damage(Enemy *e, float dmg, DamageType dtype) {
    if (!e->active || e->dead) return;

    // Multiplicateur de résistance/faiblesse depuis la table globale
    int dt_idx = (int)dtype;
    if (dt_idx < 0 || dt_idx >= DAMAGE_TYPE_COUNT) dt_idx = 0;
    float mult = ENEMY_DMG_MULT[e->type][dt_idx];

    if (dtype == DMG_POISON) {
        // ── Accumulation brûlure (lance-flammes) ────────────
        // burn_mult = 1.0 au 1er tir, +0.1 par stack suivant (max +100%)
        float burn_mult = 1.0f + e->burn_stacks * 0.10f;
        e->burn_stacks++;
        if (e->burn_stacks > BURN_MAX_STACKS) e->burn_stacks = BURN_MAX_STACKS;
        e->burn_decay_timer = 2.5f;  // réinitialise le timer de décroissance

        e->poison_timer  = 3.0f;
        e->poison_damage = dmg * mult * 0.3f * burn_mult;
        dmg *= 0.5f * mult * burn_mult;
    } else {
        dmg *= mult;
    }

    enemy_damage(e, dmg);
    audio_play_sfx(AUDIO_SFX_ENEMY_HIT);
}

void projectile_spawn(TowerPool *tp, const Tower *tw,
                      int target_idx, const EnemyPool *ep)
{
    if (target_idx < 0 || target_idx >= MAX_ENEMIES) return;
    const Enemy *e = &ep->enemies[target_idx];
    if (!e->active || e->dead) return;

    const TowerStats *st = &TOWER_BASE_STATS[tw->type];

    Projectile *p = NULL;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!tp->projectiles[i].active) {
            p = &tp->projectiles[i];
            break;
        }
    }
    if (!p) return;

    p->x             = tw->cx;
    p->y             = tw->cy;
    p->tx            = e->x;
    p->ty            = e->y;
    p->target_idx    = target_idx;
    p->damage        = tw->damage;
    p->speed         = st->proj_speed * TILE_SIZE;
    p->splash        = st->splash;
    p->splash_radius = st->splash ? TILE_SIZE * 1.5f : 0.0f;
    p->slow_duration = st->slow_duration;
    p->chain_left    = st->chain_count;
    p->origin        = tw->type;
    p->active        = 1;
    p->dmg_type      = tw->dmg_type;
}

void projectile_update(TowerPool *tp, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;

        if (p->target_idx < 0 || p->target_idx >= MAX_ENEMIES) {
            p->active = 0;
            continue;
        }

        const Enemy *tgt = &ep->enemies[p->target_idx];
        if (tgt->active && !tgt->dead) {
            p->tx = tgt->x;
            p->ty = tgt->y;
        }

        float dx   = p->tx - p->x;
        float dy   = p->ty - p->y;
        float dist = sqrtf(dx*dx + dy*dy);
        float step = p->speed * dt;

        if (dist <= step) {
            if (p->splash) {
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    Enemy *e = &ep->enemies[j];
                    if (!e->active || e->dead) continue;
                    float d2 = gdist2(p->tx, p->ty, e->x, e->y);
                    if (d2 <= p->splash_radius * p->splash_radius) {
                        apply_damage(e, p->damage, p->dmg_type);
                        if (p->slow_duration > 0.0f &&
                            e->type != ENEMY_VEHICLE && e->type != ENEMY_PATHBREAKER) {
                            float slow = (p->dmg_type == DMG_CRYO)
                                       ? p->slow_duration * 2.0f
                                       : p->slow_duration;
                            e->slow_timer = slow;
                        }
                    }
                }
            } else {
                Enemy *e = &ep->enemies[p->target_idx];
                apply_damage(e, p->damage, p->dmg_type);
                if (p->slow_duration > 0.0f &&
                    e->type != ENEMY_VEHICLE && e->type != ENEMY_PATHBREAKER) {
                    float slow = (p->dmg_type == DMG_CRYO)
                               ? p->slow_duration * 2.0f
                               : p->slow_duration;
                    e->slow_timer = slow;
                }
            }

            if (p->chain_left > 0) {
                float chain_rng2 = (TILE_SIZE * 2.5f) * (TILE_SIZE * 2.5f);
                int chained = 0;
                for (int j = 0; j < MAX_ENEMIES && !chained; j++) {
                    Enemy *e = &ep->enemies[j];
                    if (!e->active || e->dead || j == p->target_idx) continue;
                    if (gdist2(p->tx, p->ty, e->x, e->y) <= chain_rng2) {
                        p->x          = p->tx;
                        p->y          = p->ty;
                        p->tx         = e->x;
                        p->ty         = e->y;
                        p->target_idx = j;
                        p->chain_left--;
                        chained = 1;
                    }
                }
                if (!chained) p->active = 0;
            } else {
                p->active = 0;
            }
        } else {
            p->x += (dx / dist) * step;
            p->y += (dy / dist) * step;
        }
    }
}

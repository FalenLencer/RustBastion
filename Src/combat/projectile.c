/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "projectile.h"
#include "combat_math.h"
#include "fx.h"
#include "../engine/audio.h"
#include <math.h>
#include <stddef.h>

/* Déplacement max plausible d'une cible entre deux frames (px). Au-delà, son
   slot a été réutilisé par un autre ennemi (le plus rapide : ~3,5 tuiles/s,
   ×3 en avance rapide → < 1 tuile/frame même à bas FPS). */
#define PROJ_TARGET_JUMP_PX  (TILE_SIZE * 2.0f)

static void apply_damage(Enemy *e, float dmg, DamageType dtype) {
    if (!e->active || e->dead) return;

    // Multiplicateur de résistance/faiblesse depuis la table globale
    int dt_idx = (int)dtype;
    if (dt_idx < 0 || dt_idx >= DAMAGE_TYPE_COUNT) dt_idx = 0;
    float mult = ENEMY_DMG_MULT[e->type][dt_idx];

    // ── Synergies d'état (matériaux) ──────────────────────────
    // Cryo : un ennemi ralenti (gelé) est vulnérable à TOUT.
    // Acide : un ennemi empoisonné (corrodé) prend +dégâts phys./feu.
    int syn = 0;   // une synergie d'etat a-t-elle amplifie ce coup ? (feedback)
    if (e->slow_timer > 0.0f) { dmg *= SYN_CRYO_VULN; syn = 1; }
    if (e->poison_timer > 0.0f && (dtype == DMG_PHYSICAL || dtype == DMG_FIRE)) {
        dmg *= SYN_ACID_CORRODE; syn = 1;
    }

    /* Le lance-flammes inflige DMG_FIRE depuis que Feu et Poison ont chacun
       leur ligne de résistances ; ce bloc testait encore DMG_POISON →
       le lance-flammes n'accumulait plus AUCUNE brûlure, et l'acide
       héritait des paliers. Chaque type retrouve sa mécanique. */
    if (dtype == DMG_FIRE) {
        // ── Accumulation brûlure (lance-flammes) ────────────
        // burn_mult = 1.0 au 1er tir, +BURN_STEP par palier (max +100 %)
        float burn_mult = 1.0f + e->burn_stacks * BURN_STEP;
        e->burn_stacks++;
        if (e->burn_stacks > BURN_MAX_STACKS) e->burn_stacks = BURN_MAX_STACKS;
        e->burn_decay_timer = BURN_DECAY_TIME;   // réinitialise la décroissance
        dmg *= mult * burn_mult;
    } else if (dtype == DMG_POISON) {
        // ── Acide : dégâts sur la durée + état « corrodé » (synergie) ──
        e->poison_timer  = POISON_DURATION;
        e->poison_damage = dmg * mult * POISON_DOT_FRAC;
        dmg *= POISON_HIT_FRAC * mult;
    } else {
        dmg *= mult;
    }

    // ── Feedback pedagogique des CONTRES elementaires ────────────
    // Apprend au joueur quel type de degats marche sur quel ennemi : on
    // n'affiche un repere que si l'efficacite DIVERGE nettement du neutre
    // (table de resistance). Throttle gratuit via hit_flash (un pop par cycle
    // d'eclair, ~0.14 s), lu AVANT enemy_damage qui le remet a 1. On ne montre
    // rien si un bouclier de boss bloque (sinon le repere mentirait).
    if (g_fx.enabled && e->hit_flash <= 0.0f &&
        !(e->is_boss && e->boss_shield > 0.0f)) {
        if      (mult >= 1.30f) fx_popup(e->x, e->y - e->size, "EFFICACE", (Color){120, 240, 130, 255});
        else if (mult <= 0.70f) fx_popup(e->x, e->y - e->size, "RESISTE",  (Color){235, 110,  90, 255});
        else if (syn)           fx_popup(e->x, e->y - e->size, "SYNERGIE", (Color){120, 210, 245, 255});
    }

    enemy_damage(e, dmg);
    audio_play_sfx(AUDIO_SFX_ENEMY_HIT);
}

/* Expose le chokepoint aux autres systèmes (arme du mode Héros). */
void combat_apply_damage(Enemy *e, float dmg, DamageType dtype) {
    apply_damage(e, dmg, dtype);
}

int projectile_spawn(TowerPool *tp, const Tower *tw,
                     int target_idx, const EnemyPool *ep)
{
    if (target_idx < 0 || target_idx >= MAX_ENEMIES) return 0;
    const Enemy *e = &ep->enemies[target_idx];
    if (!e->active || e->dead) return 0;

    const TowerStats *st = &TOWER_BASE_STATS[tw->type];

    Projectile *p = NULL;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!tp->projectiles[i].active) {
            p = &tp->projectiles[i];
            break;
        }
    }
    if (!p) return 0;

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
    return 1;
}

void projectile_update(TowerPool *tp, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;

        if (p->target_idx >= MAX_ENEMIES) {
            p->active = 0;
            continue;
        }

        /* Suivi de la cible. Les slots d'ennemis sont RÉUTILISÉS : si la cible
           meurt et qu'un nouvel ennemi prend son slot pendant le vol, le
           projectile le poursuivait à travers la carte (et le frappait à
           distance). Un saut de position impossible = cible perdue
           (target_idx = -1) : le tir finit sa course au dernier point connu. */
        if (p->target_idx >= 0) {
            const Enemy *tgt = &ep->enemies[p->target_idx];
            if (tgt->active && !tgt->dead) {
                if (gdist2(tgt->x, tgt->y, p->tx, p->ty) > PROJ_TARGET_JUMP_PX * PROJ_TARGET_JUMP_PX) {
                    p->target_idx = -1;
                } else {
                    p->tx = tgt->x;
                    p->ty = tgt->y;
                }
            }
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
            } else if (p->target_idx >= 0 &&
                       ep->enemies[p->target_idx].active &&
                       !ep->enemies[p->target_idx].dead) {
                /* Mono-cible : uniquement si la cible est encore là. */
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

            /* FX 3D d'impact (mode héros) : au point touché, typé.
               Émis à chaque coup, rebonds de chaîne compris. */
            if (g_fx3d_impact) g_fx3d_impact(p->tx, p->ty, (int)p->dmg_type);

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

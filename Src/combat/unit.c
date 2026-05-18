/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "unit.h"
#include "../engine/audio.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* ════════════════════════════════════════════════════
   STATS DE BASE
   ════════════════════════════════════════════════════ */
const UnitStats UNIT_BASE_STATS[UNIT_TYPE_COUNT] = {
    [UNIT_SOLDIER] = {
        .name            = "Soldat",
        .cost            = 20,
        .hp              = 80.0f,
        .damage          = 25.0f,
        .speed           = 2.5f,
        .atk_range       = 1.2f,
        .atk_rate        = 1.2f,
        .intercept_range = 6.0f,
        .size            = 5.0f,
        .description     = "Infanterie polyvalente.",
    },
    [UNIT_HEAVY] = {
        .name            = "Lourd",
        .cost            = 35,
        .hp              = 220.0f,
        .damage          = 50.0f,
        .speed           = 1.2f,
        .atk_range       = 1.0f,
        .atk_rate        = 0.6f,
        .intercept_range = 4.0f,
        .size            = 7.0f,
        .description     = "Tank. Protege les autres unites.",
    },
    [UNIT_MEDIC] = {
        .name            = "Medic",
        .cost            = 25,
        .hp              = 60.0f,
        .damage          = 8.0f,
        .speed           = 2.0f,
        .atk_range       = 1.0f,
        .atk_rate        = 0.5f,
        .intercept_range = 3.0f,
        .size            = 5.0f,
        .description     = "Soigne les unites alliees.",
    },
    [UNIT_DOG] = {
        .name            = "Chien",
        .cost            = 10,
        .hp              = 40.0f,
        .damage          = 15.0f,
        .speed           = 4.5f,
        .atk_range       = 0.8f,
        .atk_rate        = 2.0f,
        .intercept_range = 8.0f,
        .size            = 4.0f,
        .description     = "Ultra rapide. Harcel les ennemis.",
    },
    [UNIT_WORKER] = {
        .name            = "Ouvrier",
        .cost            = 15,
        .hp              = 50.0f,
        .damage          = 5.0f,
        .speed           = 2.0f,
        .atk_range       = 0.8f,
        .atk_rate        = 0.5f,
        .intercept_range = 0.0f, // ne combat pas spontanément
        .size            = 5.0f,
        .description     = "Collecte les materiaux sur la carte.",
    },
};

/* ════════════════════════════════════════════════════
   UTILITAIRES
   ════════════════════════════════════════════════════ */
static float udist(float ax, float ay, float bx, float by) {
    float dx = ax-bx, dy = ay-by;
    return sqrtf(dx*dx + dy*dy);
}

int unit_active_limit(const MetaBonuses *bonuses, int base_count) {
    int extra = bonuses ? bonuses->unit_limit_bonus : 0;
    int base  = base_count * MAX_UNITS_PER_BASE;    // 4 par base
    int limit = base + extra * MAX_UNITS_UPGR;
    if (limit > MAX_UNITS) limit = MAX_UNITS;
    return limit;
}

/* ════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════ */
void unit_pool_init(UnitPool *up, float base_px, float base_py) {
    memset(up, 0, sizeof(UnitPool));
    up->base_px      = base_px;
    up->base_py      = base_py;
    up->selected_unit = -1;
    up->unit_limit = MAX_UNITS_PER_BASE;

    for (int i = 0; i < MAX_UNITS; i++) {
        up->units[i].target_idx  = -1;
        up->units[i].deposit_idx = -1;
        up->units[i].carried_mat = MAT_NONE;
    }
}

/* ════════════════════════════════════════════════════
   SPAWN
   ════════════════════════════════════════════════════ */
int unit_spawn_at(UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses,
                  float bpx, float bpy)
{
    if (up->count >= MAX_UNITS || up->count >= up->unit_limit) return 0;

    const UnitStats *st = &UNIT_BASE_STATS[type];
    if (*gold < st->cost) return 0;

    Unit *u = NULL;
    int slot = 0;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!up->units[i].active) { u = &up->units[i]; slot = i; break; }
    }
    if (!u) return 0;

    *gold -= st->cost;
    memset(u, 0, sizeof(Unit));

    u->type            = type;
    u->state           = USTATE_PATROL;
    u->max_hp          = st->hp     * (bonuses ? bonuses->unit_hp_mult  : 1.0f);
    u->hp              = u->max_hp;
    u->damage          = st->damage * (bonuses ? bonuses->unit_dmg_mult : 1.0f);
    u->speed           = st->speed;
    u->atk_range       = st->atk_range;
    u->atk_rate        = st->atk_rate;
    u->intercept_range = st->intercept_range;
    u->size            = st->size;
    u->target_idx      = -1;
    u->deposit_idx     = -1;
    u->carried_mat     = MAT_NONE;
    u->has_material    = 0;
    u->slot            = slot;
    u->active          = 1;
    u->home_base_px    = bpx;
    u->home_base_py    = bpy;

    float max_radius_x  = fminf(bpx, MAP_W * TILE_SIZE - bpx);
    float max_radius_y  = fminf(bpy, MAP_H * TILE_SIZE - bpy);
    float max_radius    = fminf(max_radius_x, max_radius_y) - TILE_SIZE;
    float wanted_radius = (2.0f + (float)(slot % 3)) * TILE_SIZE;
    u->patrol_radius = fminf(wanted_radius, max_radius);
    u->patrol_angle  = ((float)slot / (float)MAX_UNITS) * 2.0f * PI;

    u->x = bpx + cosf(u->patrol_angle) * u->patrol_radius;
    u->y = bpy + sinf(u->patrol_angle) * u->patrol_radius;
    up->count++;
    audio_play_sfx(AUDIO_SFX_UNIT_SPAWN);
    return 1;
}

int unit_spawn(UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses) {
    return unit_spawn_at(up, type, gold, bonuses, up->base_px, up->base_py);
}

/* ════════════════════════════════════════════════════
   ASSIGNER UN DÉPÔT À UN OUVRIER
   ════════════════════════════════════════════════════ */
void unit_assign_deposit(UnitPool *up, int unit_idx, int deposit_idx) {
    if (unit_idx < 0 || unit_idx >= MAX_UNITS) return;
    Unit *u = &up->units[unit_idx];
    if (!u->active || u->type != UNIT_WORKER) return;
    u->deposit_idx = deposit_idx;
    u->state       = USTATE_GOTO_DEPOSIT;
}

/* ════════════════════════════════════════════════════
   DÉGÂTS
   ════════════════════════════════════════════════════ */
void unit_damage(Unit *u, float dmg) {
    // Guard sur hp pour éviter les dégâts multiples sur une unité déjà morte
    if (!u->active || u->hp <= 0.0f) return;
    u->hp -= dmg;
    if (u->hp <= 0.0f) u->hp = 0.0f;
    // Ne pas mettre active=0 ici : unit_pool_update détecte hp=0
    // et gère count-- correctement au début du prochain frame
}

/* ════════════════════════════════════════════════════
   CIBLAGE ENNEMI
   ════════════════════════════════════════════════════ */
static int find_enemy_target(const Unit *u, const EnemyPool *ep,
                              float base_px, float base_py)
{
    // L'ouvrier ne cherche pas de cible spontanément
    if (u->type == UNIT_WORKER) return -1;

    float intercept_px = u->intercept_range * TILE_SIZE;
    int   best         = -1;
    float best_score   = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;

        float d_base = udist(e->x, e->y, base_px, base_py);
        float d_unit = udist(e->x, e->y, u->x,    u->y);

        if (d_base > intercept_px && d_unit > u->atk_range * TILE_SIZE * 3.0f)
            continue;

        float score = -d_base;
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}

static int find_heal_target(const Unit *medic, const UnitPool *up) {
    float heal_range = 3.0f * TILE_SIZE;
    int   best       = -1;
    float worst_hp   = FLT_MAX;

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *a = &up->units[i];
        if (!a->active || a == medic) continue;
        if (a->hp >= a->max_hp)       continue;
        float d = udist(medic->x, medic->y, a->x, a->y);
        if (d > heal_range)           continue;
        if (a->hp < worst_hp) { worst_hp = a->hp; best = i; }
    }
    return best;
}

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void unit_pool_update(UnitPool *up, EnemyPool *ep, Map *map, float dt,
                      MaterialType *inventory, int *inv_count)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &up->units[i];
        if (!u->active) continue;

        // ── Mort de l'ouvrier : perte du matériau ────────
        if (u->hp <= 0.0f) {
            u->active      = 0;
            u->carried_mat = MAT_NONE;
            u->has_material = 0;
            if (up->count > 0) up->count--;
            if (up->selected_unit == i) up->selected_unit = -1;
            continue;
        }

        u->atk_timer  -= dt;
        u->heal_timer -= dt;

        // ════════════════════════════════════════════════
        // OUVRIER — logique de collecte
        // ════════════════════════════════════════════════
        if (u->type == UNIT_WORKER) {
            switch (u->state) {

            case USTATE_GOTO_DEPOSIT: {
                // Vérifie que le dépôt est encore valide
                if (u->deposit_idx < 0 ||
                    u->deposit_idx >= map->deposit_count ||
                    !map->deposits[u->deposit_idx].active) {
                    u->state       = USTATE_PATROL;
                    u->deposit_idx = -1;
                    break;
                }
                MaterialDeposit *dep = &map->deposits[u->deposit_idx];
                float dep_x = dep->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                float dep_y = dep->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                float dist  = udist(u->x, u->y, dep_x, dep_y);

                if (dist <= TILE_SIZE * 0.8f) {
                    // Arrivé au dépôt — commence la collecte
                    u->state            = USTATE_COLLECT;
                    u->collect_duration = 4.0f; // 4 secondes
                    u->collect_timer    = u->collect_duration;
                } else {
                    // Avance vers le dépôt
                    float dx   = dep_x - u->x;
                    float dy   = dep_y - u->y;
                    float step = u->speed * TILE_SIZE * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                }
                break;
            }

            case USTATE_COLLECT: {
                // Vérifie que le dépôt est encore là
                if (u->deposit_idx < 0 ||
                    u->deposit_idx >= map->deposit_count ||
                    !map->deposits[u->deposit_idx].active) {
                    u->state       = USTATE_PATROL;
                    u->deposit_idx = -1;
                    break;
                }
                u->collect_timer -= dt;
                if (u->collect_timer <= 0.0f) {
                    // Collecte terminée
                    MaterialDeposit *dep = &map->deposits[u->deposit_idx];
                    u->carried_mat       = dep->type;
                    u->has_material      = 1;
                    dep->active          = 0; // dépôt épuisé
                    u->deposit_idx       = -1;
                    u->state             = USTATE_GOTO_BASE;
                }
                break;
            }

            case USTATE_GOTO_BASE: {
                float dist = udist(u->x, u->y, u->home_base_px, u->home_base_py);
                if (dist <= TILE_SIZE * 1.0f) {
                    // Arrivé à la base — dépose le matériau
                    if (u->has_material && inv_count &&
                        *inv_count < MAX_INVENTORY) {
                        inventory[*inv_count] = u->carried_mat;
                        (*inv_count)++;
                        audio_play_sfx(AUDIO_SFX_MATERIAL_COLLECT);
                    }
                    u->carried_mat  = MAT_NONE;
                    u->has_material = 0;
                    u->state        = USTATE_PATROL;
                } else {
                    float dx   = u->home_base_px - u->x;
                    float dy   = u->home_base_py - u->y;
                    float step = u->speed * TILE_SIZE * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                }
                break;
            }

            default:
                // PATROL : reste près de la base
                u->state = USTATE_PATROL;
                u->patrol_angle += 0.3f * dt;
                {
                    float tx   = u->home_base_px + cosf(u->patrol_angle) * (TILE_SIZE * 1.5f);
                    float ty   = u->home_base_py + sinf(u->patrol_angle) * (TILE_SIZE * 1.5f);
                    float dist = udist(u->x, u->y, tx, ty);
                    if (dist > 2.0f) {
                        float dx   = tx - u->x;
                        float dy   = ty - u->y;
                        float step = u->speed * TILE_SIZE * 0.5f * dt;
                        u->x += (dx / dist) * step;
                        u->y += (dy / dist) * step;
                    }
                }
                break;
            }

            // Clamp position
            float margin = u->size + 2.0f;
            if (u->x < margin)                     u->x = margin;
            if (u->x > MAP_W * TILE_SIZE - margin) u->x = MAP_W * TILE_SIZE - margin;
            if (u->y < margin)                     u->y = margin;
            if (u->y > MAP_H * TILE_SIZE - margin) u->y = MAP_H * TILE_SIZE - margin;
            continue; // ne pas passer dans la logique combat
        }

        // ════════════════════════════════════════════════
        // UNITÉS DE COMBAT — logique existante
        // ════════════════════════════════════════════════

        // MÉDIC : soigne en priorité
        if (u->type == UNIT_MEDIC && u->heal_timer <= 0.0f) {
            int heal_tgt = find_heal_target(u, up);
            if (heal_tgt != -1) {
                up->units[heal_tgt].hp += 20.0f;
                if (up->units[heal_tgt].hp > up->units[heal_tgt].max_hp)
                    up->units[heal_tgt].hp = up->units[heal_tgt].max_hp;
                u->heal_timer = 1.5f;
                u->state = USTATE_HEAL;
            }
        }

        // Cherche cible ennemie
        int tgt = find_enemy_target(u, ep, u->home_base_px, u->home_base_py);
        u->target_idx = tgt;

        if (tgt != -1) {
            Enemy *e    = &ep->enemies[tgt];
            float  dist = udist(u->x, u->y, e->x, e->y);
            float  atk  = u->atk_range * TILE_SIZE;

            if (dist <= atk) {
                u->state = USTATE_ATTACK;
                if (u->atk_timer <= 0.0f) {
                    enemy_damage(e, u->damage);
                    u->atk_timer = 1.0f / u->atk_rate;
                    unit_damage(u, (float)e->damage * 2.0f);
                }
            } else {
                u->state = USTATE_CHASE;
                float dx   = e->x - u->x;
                float dy   = e->y - u->y;
                float step = u->speed * TILE_SIZE * dt;
                u->x += (dx / dist) * step;
                u->y += (dy / dist) * step;
            }
        } else {
            u->state = USTATE_PATROL;
            u->patrol_angle += 0.5f * dt;
            float target_x = u->home_base_px + cosf(u->patrol_angle) * u->patrol_radius;
            float target_y = u->home_base_py + sinf(u->patrol_angle) * u->patrol_radius;
            float dist     = udist(u->x, u->y, target_x, target_y);
            if (dist > 2.0f) {
                float dx   = target_x - u->x;
                float dy   = target_y - u->y;
                float step = u->speed * TILE_SIZE * 0.6f * dt;
                u->x += (dx / dist) * step;
                u->y += (dy / dist) * step;
            }
        }

        // Limite déplacement hors portée
        float max_dist = (u->intercept_range + 2.0f) * TILE_SIZE;
        float d_base   = udist(u->x, u->y, u->home_base_px, u->home_base_py);
        if (d_base > max_dist && u->target_idx == -1) {
            float dx   = u->home_base_px - u->x;
            float dy   = u->home_base_py - u->y;
            float step = u->speed * TILE_SIZE * dt;
            u->x += (dx / d_base) * step;
            u->y += (dy / d_base) * step;
            u->state = USTATE_RETURN;
        }

        // Clamp position
        float margin = u->size + 2.0f;
        if (u->x < margin)                     u->x = margin;
        if (u->x > MAP_W * TILE_SIZE - margin) u->x = MAP_W * TILE_SIZE - margin;
        if (u->y < margin)                     u->y = margin;
        if (u->y > MAP_H * TILE_SIZE - margin) u->y = MAP_H * TILE_SIZE - margin;
    }
}
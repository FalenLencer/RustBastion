/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "unit.h"
#include "tower.h"
#include "combat_math.h"
#include "../engine/audio.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "../game/meta.h"
_Static_assert(META_UNIT_COUNT == UNIT_TYPE_COUNT,
               "META_UNIT_COUNT dans meta.h ne correspond pas a UNIT_TYPE_COUNT");

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
        .atk_range       = 4.5f,   /* longue portée — tir depuis la distance */
        .atk_rate        = 1.2f,
        .intercept_range = 7.0f,   /* détecte les ennemis plus tôt */
        .size            = 5.0f,
        .description     = "Infanterie longue portee. Tire depuis la distance.",
    },
    [UNIT_HEAVY] = {
        .name            = "Lourd",
        .cost            = 35,
        .hp              = 220.0f,
        .damage          = 50.0f,
        .speed           = 1.2f,
        .atk_range       = 1.5f,   /* corps à corps — courte portée comme le chien */
        .atk_rate        = 0.6f,
        .intercept_range = 4.0f,
        .size            = 7.0f,
        .description     = "Tank corps a corps. Bouclier vivant pour les allies.",
    },
    [UNIT_MEDIC] = {
        .name            = "Medic",
        .cost            = 25,
        .hp              = 60.0f,
        .damage          = 8.0f,
        .speed           = 2.0f,
        .atk_range       = 1.0f,   /* mêlée, uniquement si seul */
        .atk_rate        = 0.5f,
        .intercept_range = 3.0f,
        .size            = 5.0f,
        .description     = "Soigne les allies. N'attaque qu'en dernier recours.",
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

const char *UNIT_LORE[UNIT_TYPE_COUNT] = {
    [UNIT_SOLDIER] =
        "Infanterie equipee de fusils de recuperation.\n"
        "Engage les ennemis depuis la distance (4-5 tiles) sans\n"
        "prendre de degats en retour. Efficace en nombre sur les couloirs\n"
        "ouverts. Fragile au corps a corps — gardez-les derriere un Lourd.",

    [UNIT_HEAVY] =
        "Combattant blinde en armure de recuperation. Tres lent mais\n"
        "capable d'absorber de nombreux coups. Fait office de bouclier\n"
        "pour les unites plus fragiles positionnees derriere lui.",

    [UNIT_MEDIC] =
        "Ancien infirmier militaire reconverti. Soigne automatiquement\n"
        "les allies proches. Ne combat pas tant que d'autres unites\n"
        "de combat sont presentes — mais se defend au corps a corps\n"
        "s'il se retrouve seul face aux ennemis.",

    [UNIT_DOG] =
        "Chien de combat mutant eleve dans les ruines. Extremement rapide\n"
        "et agile. Detecte les ennemis a grande distance et s'y precipite\n"
        "pour les harceler. Fragile — ne le laissez pas seul.",

    [UNIT_WORKER] =
        "Non combatif. L'ouvrier se rend sur les depots de materiaux\n"
        "reperes sur la carte, collecte leur contenu et le rapporte a la\n"
        "base. Les materiaux amplifieront vos tours au combat. Protegez-le.",
};

/* ════════════════════════════════════════════════════
   UTILITAIRES
   ════════════════════════════════════════════════════ */
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
        up->units[i].target_idx      = -1;
        up->units[i].deposit_idx     = -1;
        up->units[i].carried_mat     = MAT_NONE;
        up->units[i].escort_idx      = -1;
        up->units[i].guard_tower_idx = -1;
        up->units[i].behavior        = UBEH_PATROL;
        up->units[i].manual_moving   = 0;
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

    /* Limite de médics par base */
    if (type == UNIT_MEDIC) {
        int mc = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            const Unit *u = &up->units[i];
            if (u->active && u->type == UNIT_MEDIC &&
                u->home_base_px == bpx && u->home_base_py == bpy)
                mc++;
        }
        if (mc >= MAX_MEDICS_PER_BASE) return 0;
    }

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

    u->behavior        = UBEH_PATROL;
    u->escort_idx      = -1;
    u->guard_tower_idx = -1;
    u->manual_x        = bpx;
    u->manual_y        = bpy;
    u->manual_moving   = 0;

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

        float d_base = gdist(e->x, e->y, base_px, base_py);
        float d_unit = gdist(e->x, e->y, u->x,    u->y);

        if (d_base > intercept_px && d_unit > u->atk_range * TILE_SIZE * 3.0f)
            continue;

        float score = -d_base;
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}

static int find_heal_target(const Unit *medic, const UnitPool *up) {
    float heal_range = UNIT_MEDIC_HEAL_RANGE * TILE_SIZE;
    int   best       = -1;
    float worst_hp   = FLT_MAX;

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *a = &up->units[i];
        if (!a->active || a == medic) continue;
        if (a->hp >= a->max_hp)       continue;
        float d = gdist(medic->x, medic->y, a->x, a->y);
        if (d > heal_range)           continue;
        if (a->hp < worst_hp) { worst_hp = a->hp; best = i; }
    }
    return best;
}

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void unit_pool_update(UnitPool *up, EnemyPool *ep, Map *map, float dt,
                      MaterialType *inventory, int *inv_count,
                      const TowerPool *towers)
{
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &up->units[i];
        if (!u->active) continue;

        // ── Mort : note has_material avant désactivation
        //    (game_state.c snapshote les ouvriers pour lâcher la ressource)
        if (u->hp <= 0.0f) {
            u->active       = 0;
            u->carried_mat  = MAT_NONE;
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
                // Figé pendant PHASE_PREP
                if (!up->mining_enabled) break;
                // Dépôt toujours valide ?
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
                float dist  = gdist(u->x, u->y, dep_x, dep_y);
                if (dist <= TILE_SIZE * UNIT_DEPOSIT_ARRIVE_DIST) {
                    u->state            = USTATE_COLLECT;
                    u->collect_duration = UNIT_WORKER_COLLECT_DURATION;
                    u->collect_timer    = u->collect_duration;
                } else {
                    float dx   = dep_x - u->x;
                    float dy   = dep_y - u->y;
                    float step = u->speed * TILE_SIZE * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                }
                break;
            }

            case USTATE_COLLECT: {
                // Figé pendant PHASE_PREP
                if (!up->mining_enabled) break;
                // Dépôt toujours valide ?
                if (u->deposit_idx < 0 ||
                    u->deposit_idx >= map->deposit_count ||
                    !map->deposits[u->deposit_idx].active) {
                    u->state       = USTATE_PATROL;
                    u->deposit_idx = -1;
                    break;
                }
                // Ralentissement si ennemis proches
                float collect_rate = 1.0f;
                if (ep) {
                    float slow_px = UNIT_WORKER_ENEMY_SLOW_RANGE * TILE_SIZE;
                    for (int j = 0; j < MAX_ENEMIES; j++) {
                        const Enemy *e = &ep->enemies[j];
                        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
                        if (gdist(u->x, u->y, e->x, e->y) <= slow_px) {
                            collect_rate = UNIT_WORKER_ENEMY_SLOW_FACTOR;
                            break;
                        }
                    }
                }
                u->collect_timer -= dt * collect_rate;
                if (u->collect_timer <= 0.0f) {
                    MaterialDeposit *dep = &map->deposits[u->deposit_idx];
                    u->carried_mat  = dep->type;
                    u->has_material = 1;
                    dep->active     = 0; // dépôt épuisé
                    u->deposit_idx  = -1;
                    u->state        = USTATE_GOTO_BASE;
                }
                break;
            }

            case USTATE_GOTO_BASE: {
                // Retour à la base autorisé même en PHASE_PREP
                float dist = gdist(u->x, u->y, u->home_base_px, u->home_base_py);
                if (dist <= TILE_SIZE * UNIT_BASE_ARRIVE_DIST) {
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
                // PATROL : tourne autour de la base
                u->state = USTATE_PATROL;
                u->patrol_angle += UNIT_WORKER_PATROL_ANGLE_SPEED * dt;
                {
                    float tx   = u->home_base_px
                                 + cosf(u->patrol_angle) * (TILE_SIZE * UNIT_WORKER_PATROL_RADIUS);
                    float ty   = u->home_base_py
                                 + sinf(u->patrol_angle) * (TILE_SIZE * UNIT_WORKER_PATROL_RADIUS);
                    float dist = gdist(u->x, u->y, tx, ty);
                    if (dist > UNIT_PATROL_SLACK) {
                        float dx   = tx - u->x;
                        float dy   = ty - u->y;
                        float step = u->speed * TILE_SIZE * UNIT_WORKER_PATROL_SPEED_FRAC * dt;
                        u->x += (dx / dist) * step;
                        u->y += (dy / dist) * step;
                    }
                }
                break;
            }

            // Clamp position
            {
                float margin = u->size + 2.0f;
                if (u->x < margin)                      u->x = margin;
                if (u->x > map->w * TILE_SIZE - margin) u->x = map->w * TILE_SIZE - margin;
                if (u->y < margin)                      u->y = margin;
                if (u->y > map->h * TILE_SIZE - margin) u->y = map->h * TILE_SIZE - margin;
            }
            continue; // pas de logique combat pour l'ouvrier
        }

        // ════════════════════════════════════════════════
        // UNITÉS DE COMBAT
        // ════════════════════════════════════════════════

        // ── Résolution du comportement : position "home" effective ──
        float eff_hx = u->home_base_px;
        float eff_hy = u->home_base_py;

        switch (u->behavior) {
        case UBEH_GUARD_TOWER:
            if (u->guard_tower_idx >= 0 && towers &&
                u->guard_tower_idx < MAX_TOWERS &&
                towers->towers[u->guard_tower_idx].active) {
                eff_hx = towers->towers[u->guard_tower_idx].cx;
                eff_hy = towers->towers[u->guard_tower_idx].cy;
            } else {
                // Tour détruite → retour patrouille
                u->behavior        = UBEH_PATROL;
                u->guard_tower_idx = -1;
            }
            break;
        case UBEH_ESCORT_WORKER:
            if (u->escort_idx >= 0 && u->escort_idx < MAX_UNITS &&
                up->units[u->escort_idx].active &&
                up->units[u->escort_idx].type == UNIT_WORKER) {
                eff_hx = up->units[u->escort_idx].x;
                eff_hy = up->units[u->escort_idx].y;
            } else {
                // Ouvrier mort/manquant → retour patrouille
                u->behavior    = UBEH_PATROL;
                u->escort_idx  = -1;
            }
            break;
        case UBEH_FOLLOW_UNIT:
            if (u->escort_idx >= 0 && u->escort_idx < MAX_UNITS &&
                up->units[u->escort_idx].active) {
                eff_hx = up->units[u->escort_idx].x;
                eff_hy = up->units[u->escort_idx].y;
            } else {
                // Cible morte/manquante → retour patrouille
                u->behavior   = UBEH_PATROL;
                u->escort_idx = -1;
            }
            break;
        case UBEH_MANUAL:
            eff_hx = u->manual_x;
            eff_hy = u->manual_y;
            // Déplacement vers la destination manuelle
            if (u->manual_moving) {
                float dist = gdist(u->x, u->y, u->manual_x, u->manual_y);
                if (dist <= TILE_SIZE * 0.5f) {
                    u->manual_moving = 0;
                    u->state = USTATE_PATROL;
                } else {
                    float dx   = u->manual_x - u->x;
                    float dy   = u->manual_y - u->y;
                    float step = u->speed * TILE_SIZE * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                    u->state = USTATE_MOVE_MANUAL;
                }
            }
            break;
        default: // UBEH_PATROL
            break;
        }

        // ── MÉDIC : soigne en priorité ────────────────────────────
        if (u->type == UNIT_MEDIC && u->heal_timer <= 0.0f) {
            int heal_tgt = find_heal_target(u, up);
            if (heal_tgt != -1) {
                up->units[heal_tgt].hp += UNIT_MEDIC_HEAL_AMOUNT;
                if (up->units[heal_tgt].hp > up->units[heal_tgt].max_hp)
                    up->units[heal_tgt].hp = up->units[heal_tgt].max_hp;
                u->heal_timer = UNIT_MEDIC_HEAL_TIMER;
                u->state = USTATE_HEAL;
            }
        }

        // ── Cherche cible ennemie ─────────────────────────────────
        // Médic : n'attaque que s'il est la seule unité de combat active.
        // Dès qu'un Soldat, Lourd ou Chien est présent, il se consacre aux soins.
        int can_attack = 1;
        if (u->type == UNIT_MEDIC) {
            for (int j = 0; j < MAX_UNITS; j++) {
                if (j == i || !up->units[j].active) continue;
                UnitType jt = up->units[j].type;
                if (jt != UNIT_MEDIC && jt != UNIT_WORKER) { can_attack = 0; break; }
            }
        }

        int tgt = can_attack ? find_enemy_target(u, ep, eff_hx, eff_hy) : -1;
        u->target_idx = tgt;

        if (tgt != -1) {
            Enemy *e    = &ep->enemies[tgt];
            float  dist = gdist(u->x, u->y, e->x, e->y);
            float  atk  = u->atk_range * TILE_SIZE;
            if (dist <= atk) {
                u->state = USTATE_ATTACK;
                if (u->atk_timer <= 0.0f) {
                    enemy_damage(e, u->damage);
                    u->atk_timer = 1.0f / u->atk_rate;
                    /* Contre-dégâts seulement en corps à corps :
                       une unité à longue portée ne prend pas de coups en retour
                       — c'est l'ennemi qui appliquera ses dégâts en s'approchant. */
                    if (u->atk_range <= UNIT_MELEE_ATK_THRESHOLD)
                        unit_damage(u, (float)e->damage * UNIT_COUNTER_DMG_MULT);
                }
            } else {
                u->state = USTATE_CHASE;
                float dx   = e->x - u->x;
                float dy   = e->y - u->y;
                float step = u->speed * TILE_SIZE * dt;
                u->x += (dx / dist) * step;
                u->y += (dy / dist) * step;
            }
        } else if (u->behavior != UBEH_MANUAL || !u->manual_moving) {
            // Pas d'ennemi et pas en déplacement manuel actif → patrouille
            u->state = USTATE_PATROL;
            if (u->behavior == UBEH_MANUAL) {
                // En mode manuel : rester au point de destination (pas d'orbite)
                float dist = gdist(u->x, u->y, eff_hx, eff_hy);
                if (dist > UNIT_PATROL_SLACK) {
                    float dx   = eff_hx - u->x;
                    float dy   = eff_hy - u->y;
                    float step = u->speed * TILE_SIZE * UNIT_PATROL_SPEED_FRAC * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                }
            } else {
                // Orbite normale autour de eff_hx/hy
                u->patrol_angle += UNIT_PATROL_ANGLE_SPEED * dt;
                float target_x = eff_hx + cosf(u->patrol_angle) * u->patrol_radius;
                float target_y = eff_hy + sinf(u->patrol_angle) * u->patrol_radius;
                float dist     = gdist(u->x, u->y, target_x, target_y);
                if (dist > UNIT_PATROL_SLACK) {
                    float dx   = target_x - u->x;
                    float dy   = target_y - u->y;
                    float step = u->speed * TILE_SIZE * UNIT_PATROL_SPEED_FRAC * dt;
                    u->x += (dx / dist) * step;
                    u->y += (dy / dist) * step;
                }
            }
        }

        // ── Rappel si trop loin du point home (sauf mode MANUEL) ──
        if (u->behavior != UBEH_MANUAL) {
            float max_dist = (u->intercept_range + 2.0f) * TILE_SIZE;
            float d_base   = gdist(u->x, u->y, eff_hx, eff_hy);
            if (d_base > max_dist && u->target_idx == -1) {
                float dx   = eff_hx - u->x;
                float dy   = eff_hy - u->y;
                float step = u->speed * TILE_SIZE * dt;
                u->x += (dx / d_base) * step;
                u->y += (dy / d_base) * step;
                u->state = USTATE_RETURN;
            }
        }

        // ── Clamp position ────────────────────────────────────────
        {
            float margin = u->size + 2.0f;
            if (u->x < margin)                      u->x = margin;
            if (u->x > map->w * TILE_SIZE - margin) u->x = map->w * TILE_SIZE - margin;
            if (u->y < margin)                      u->y = margin;
            if (u->y > map->h * TILE_SIZE - margin) u->y = map->h * TILE_SIZE - margin;
        }
    }
}
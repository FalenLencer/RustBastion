/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "enemy.h"
#include "unit.h"
#include "tower.h"
#include "../engine/audio.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* ════════════════════════════════════════════════════
   STATS DE BASE PAR TYPE
   ════════════════════════════════════════════════════ */
const EnemyStats ENEMY_BASE_STATS[ENEMY_TYPE_COUNT] = {
    [ENEMY_RAIDER]      = {.hp=60,  .speed=2.0f, .size=6,  .reward=5,  .damage=1, .melee_range=18.0f, .name="Raider"},
    [ENEMY_BRUTE]       = {.hp=200, .speed=0.9f, .size=9,  .reward=12, .damage=3, .melee_range=28.0f, .name="Brute"},
    [ENEMY_RUNNER]      = {.hp=40,  .speed=3.5f, .size=5,  .reward=7,  .damage=1, .melee_range=14.0f, .name="Runner"},
    [ENEMY_VEHICLE]     = {.hp=500, .speed=0.6f, .size=12, .reward=30, .damage=5, .melee_range=36.0f, .name="Blinde"},
    [ENEMY_MUTANT]      = {.hp=120, .speed=1.2f, .size=8,  .reward=10, .damage=2, .melee_range=22.0f, .name="Mutant"},
    [ENEMY_GHOST]       = {.hp=35,  .speed=2.2f, .size=6,  .reward=15, .damage=1, .melee_range=16.0f, .name="Spectre"},
    [ENEMY_PATHBREAKER] = {.hp=150, .speed=2.0f, .size=7,  .reward=18, .damage=2, .melee_range=20.0f, .name="Briseur"},
    [ENEMY_HEALER]      = {.hp=80,  .speed=1.2f, .size=7,  .reward=20, .damage=1, .melee_range=16.0f, .name="Medic"},
    [ENEMY_HUNTER]      = {.hp=90,  .speed=3.0f, .size=6,  .reward=22, .damage=4, .melee_range=20.0f, .name="Chasseur"},
    [ENEMY_ARTILLERY]   = {.hp=300, .speed=0.5f, .size=10, .reward=35, .damage=3, .melee_range=12.0f, .name="Artillerie"},
};

/* ════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════ */
void enemy_pool_init(EnemyPool *pool) {
    memset(pool, 0, sizeof(EnemyPool));
}

/* ════════════════════════════════════════════════════
   UTILITAIRE
   ════════════════════════════════════════════════════ */
static float edist(float ax, float ay, float bx, float by) {
    float dx = ax-bx, dy = ay-by;
    return sqrtf(dx*dx + dy*dy);
}

/* ════════════════════════════════════════════════════
   SPAWN
   ════════════════════════════════════════════════════ */
void enemy_spawn(EnemyPool *pool, EnemyType type,
                 int path_id, const PathSet *paths,
                 float spawn_delay, float wave_scale,
                 float speed_mult)
{
    if (pool->count >= MAX_ENEMIES) return;
    if (path_id < 0 || path_id >= paths->count) return;

    const Path       *path = &paths->paths[path_id];
    if (!path->found || path->len == 0) return;
    const EnemyStats *base = &ENEMY_BASE_STATS[type];

    Enemy *e = NULL;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!pool->enemies[i].active) { e = &pool->enemies[i]; break; }
    }
    if (!e) return;

    memset(e, 0, sizeof(Enemy));

    e->x           = path->steps[0].x * TILE_SIZE + TILE_SIZE / 2.0f;
    e->y           = path->steps[0].y * TILE_SIZE + TILE_SIZE / 2.0f;
    e->type        = type;
    e->max_hp      = base->hp * wave_scale;
    e->hp          = e->max_hp;
    e->speed       = base->speed * speed_mult;
    e->size        = base->size;
    e->reward      = base->reward;
    e->damage      = base->damage;
    e->melee_range = base->melee_range;
    e->path_id     = path_id;
    e->path_index  = 0;
    e->spawn_delay = spawn_delay;
    e->active      = 1;
    e->hunt_target = -1;
    e->arty_target = -1;

    // Ghost
    e->invisible = (type == ENEMY_GHOST) ? 1 : 0;

    // Healer
    if (type == ENEMY_HEALER) {
        e->heal_range  = 2.0f * TILE_SIZE;
        e->heal_amount = 15.0f;
        e->heal_timer  = 0.0f;
    }

    // Hunter
    if (type == ENEMY_HUNTER)
        e->hunt_range = 6.0f * TILE_SIZE;

    // Artillery
    if (type == ENEMY_ARTILLERY)
        e->arty_range = 4.0f * TILE_SIZE;

    // Pathbreaker
    if (type == ENEMY_PATHBREAKER) {
        e->path_broken = 0;
        int break_min = path->len / 3;
        int break_max = path->len / 2;
        if (break_min < 1) break_min = 1;
        if (break_max <= break_min) break_max = break_min + 1;
        e->break_at = break_min + GetRandomValue(0, break_max - break_min);
        e->target_x = path->steps[path->len-1].x * TILE_SIZE + TILE_SIZE / 2.0f;
        e->target_y = path->steps[path->len-1].y * TILE_SIZE + TILE_SIZE / 2.0f;
        e->speed   *= 1.3f;
    }

    pool->count++;
}

/* ════════════════════════════════════════════════════
   DÉGÂTS
   ════════════════════════════════════════════════════ */
void enemy_damage(Enemy *e, float dmg) {
    if (!e->active || e->dead) return;
    if (e->type == ENEMY_BRUTE) dmg *= 0.8f;
    e->hp -= dmg;
    if (e->hp <= 0.0f) { e->hp = 0.0f; e->dead = 1; }
}

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void enemy_pool_update(EnemyPool *pool, const PathSet *paths,
                       UnitPool *units, TowerPool *towers,
                       Map *map,
                       float dt, int *lives, int *gold, int *kills)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &pool->enemies[i];
        if (!e->active) continue;

        // ── Délai de spawn ────────────────────────────────────
        if (e->spawn_delay > 0.0f) {
            e->spawn_delay -= dt;
            if (e->spawn_delay <= 0.0f)
                audio_play_sfx(AUDIO_SFX_ENEMY_SPAWN);
            continue;
        }

        // ── Mort ─────────────────────────────────────────────
        if (e->dead) {
            audio_play_sfx(AUDIO_SFX_ENEMY_DEATH);
            *gold  += e->reward;
            *kills += 1;
            e->active = 0;
            if (pool->count > 0) pool->count--;
            continue;
        }

        // ── Atteint la base ───────────────────────────────────
        if (e->reached_base) {
            // Décrémente la base individuelle selon le chemin emprunté
            // Si la base est déjà tombée, l'ennemi disparaît sans effet
            if (map && e->path_id >= 0 && e->path_id < paths->count) {
                int bid = paths->paths[e->path_id].base_id;
                if (bid >= 0 && bid < map->base_count && map->bases[bid].active) {
                    map->bases[bid].hp -= e->damage;
                    if (map->bases[bid].hp <= 0) {
                        map->bases[bid].hp     = 0;
                        map->bases[bid].active = 0;
                    }
                    *lives -= e->damage;
                    if (*lives < 0) *lives = 0;
                }
            }
            e->active = 0;
            if (pool->count > 0) pool->count--;
            continue;
        }

        // ── Régénération Mutant ───────────────────────────────
        if (e->type == ENEMY_MUTANT && e->hp < e->max_hp) {
            e->hp += 5.0f * dt;
            if (e->hp > e->max_hp) e->hp = e->max_hp;
        }

        // ── Ralentissement ────────────────────────────────────
        if (e->slow_timer > 0.0f) {
            if (e->type != ENEMY_VEHICLE && e->type != ENEMY_PATHBREAKER)
                e->slow_timer -= dt;
            else
                e->slow_timer = 0.0f;
        }

        // ── Poison DoT ────────────────────────────────────────
        if (e->poison_timer > 0.0f) {
            e->poison_timer -= dt;
            enemy_damage(e, e->poison_damage * dt);
        }

        // ── Cooldowns ─────────────────────────────────────────
        if (e->atk_timer  > 0.0f) e->atk_timer  -= dt;
        if (e->heal_timer > 0.0f) e->heal_timer  -= dt;
        if (e->arty_timer > 0.0f) e->arty_timer  -= dt;

        // ════════════════════════════════════════════════
        // HEALER — soigne les ennemis proches en continu
        // ════════════════════════════════════════════════
        if (e->type == ENEMY_HEALER) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (i == j) continue;
                Enemy *other = &pool->enemies[j];
                if (!other->active || other->dead || other->spawn_delay > 0.0f) continue;
                if (edist(e->x, e->y, other->x, other->y) <= e->heal_range) {
                    other->hp += e->heal_amount * dt;
                    if (other->hp > other->max_hp)
                        other->hp = other->max_hp;
                }
            }
        }

        // ════════════════════════════════════════════════
        // HUNTER — traque les unités, ignore le chemin
        // ════════════════════════════════════════════════
        if (e->type == ENEMY_HUNTER && units) {
            float best_d = FLT_MAX;
            int   best_u = -1;
            for (int j = 0; j < MAX_UNITS; j++) {
                const Unit *u = &units->units[j];
                if (!u->active) continue;
                float d = edist(e->x, e->y, u->x, u->y);
                if (d < e->hunt_range && d < best_d) {
                    best_d = d; best_u = j;
                }
            }
            e->hunt_target = best_u;

            if (best_u != -1) {
                const Unit *u = &units->units[best_u];
                float dx   = u->x - e->x;
                float dy   = u->y - e->y;
                float dist = edist(e->x, e->y, u->x, u->y);

                if (dist <= e->melee_range) {
                    // Attaque l'unité — dégâts ×3
                    if (e->atk_timer <= 0.0f) {
                        unit_damage((Unit*)u, (float)e->damage * 3.0f);
                        e->atk_timer = 0.4f;
                    }
                    continue;
                } else {
                    // Fonce sur l'unité en ignorant le chemin
                    float step = e->speed * TILE_SIZE * dt;
                    e->x += (dx / dist) * step;
                    e->y += (dy / dist) * step;
                    continue;
                }
            }
            // Pas d'unité → suit le chemin normalement (pas de continue ici)
        }

        // ════════════════════════════════════════════════
        // ARTILLERY — s'arrête et tire sur les tours
        // ════════════════════════════════════════════════
        if (e->type == ENEMY_ARTILLERY && towers) {
            float best_d = FLT_MAX;
            int   best_t = -1;
            for (int j = 0; j < MAX_TOWERS; j++) {
                Tower *tw = &towers->towers[j];
                // Ignorer les tours inactives OU déjà à 0 HP (en attente de cleanup)
                if (!tw->active || tw->hp <= 0.0f) continue;
                float d = edist(e->x, e->y, tw->cx, tw->cy);
                if (d <= e->arty_range && d < best_d) {
                    best_d = d; best_t = j;
                }
            }
            e->arty_target = best_t;

            if (best_t != -1) {
                Tower *tw = &towers->towers[best_t];
                if (e->arty_timer <= 0.0f) {
                    // Dégâts fixes : 3 tirs de 40 pour détruire (hp=100)
                    tw->hp -= 40.0f;
                    if (tw->hp <= 0.0f) tw->hp = 0.0f;
                    e->arty_timer = 3.0f;
                }
                // S'arrête pendant qu'il tire
                continue;
            }
        }

        // ════════════════════════════════════════════════
        // COMBAT MÊLÉE (tous types sauf Hunter et Artillery
        // qui ont leur propre logique de combat)
        // ════════════════════════════════════════════════
        if (e->type != ENEMY_HUNTER && e->type != ENEMY_ARTILLERY) {
            int   engaged_unit = -1;
            float closest_dist = FLT_MAX;

            if (units) {
                for (int j = 0; j < MAX_UNITS; j++) {
                    const Unit *u = &units->units[j];
                    if (!u->active) continue;
                    float d = edist(e->x, e->y, u->x, u->y);
                    if (d <= e->melee_range && d < closest_dist) {
                        closest_dist = d; engaged_unit = j;
                    }
                }
            }

            if (engaged_unit != -1) {
                e->engage_timer = 0.3f;
                if (e->atk_timer <= 0.0f) {
                    float atk_rate;
                    switch (e->type) {
                        case ENEMY_BRUTE:       atk_rate = 0.6f; break;
                        case ENEMY_RUNNER:      atk_rate = 2.0f; break;
                        case ENEMY_VEHICLE:     atk_rate = 0.4f; break;
                        case ENEMY_GHOST:       atk_rate = 1.5f; break;
                        case ENEMY_PATHBREAKER: atk_rate = 1.2f; break;
                        case ENEMY_HEALER:      atk_rate = 0.5f; break;
                        default:                atk_rate = 1.0f; break;
                    }
                    e->atk_timer = 1.0f / atk_rate;
                    unit_damage((Unit*)&units->units[engaged_unit],
                                (float)e->damage * 3.0f);
                }
                continue;
            } else if (e->engage_timer > 0.0f) {
                e->engage_timer -= dt;
                continue;
            }
        }

        // ════════════════════════════════════════════════
        // DÉPLACEMENT SUR LE CHEMIN
        // ════════════════════════════════════════════════

        // Pathbreaker quitte le chemin
        if (e->type == ENEMY_PATHBREAKER && !e->path_broken) {
            if (e->path_index >= e->break_at) e->path_broken = 1;
        }

        if (e->type == ENEMY_PATHBREAKER && e->path_broken) {
            float dx   = e->target_x - e->x;
            float dy   = e->target_y - e->y;
            float dist = sqrtf(dx*dx + dy*dy);
            float step = e->speed * TILE_SIZE * dt;
            if (dist <= step)
                e->reached_base = 1;
            else {
                e->x += (dx / dist) * step;
                e->y += (dy / dist) * step;
            }
        } else {
            if (e->path_id < 0 || e->path_id >= paths->count) {
                e->active = 0;
                if (pool->count > 0) pool->count--;
                continue;
            }

            const Path *path = &paths->paths[e->path_id];
            if (e->path_index >= path->len - 1) {
                e->reached_base = 1;
                continue;
            }

            float speed = e->speed;
            if (e->slow_timer > 0.0f && e->type != ENEMY_VEHICLE)
                speed *= 0.5f;

            Point next = path->steps[e->path_index + 1];
            float tx   = next.x * TILE_SIZE + TILE_SIZE / 2.0f;
            float ty2  = next.y * TILE_SIZE + TILE_SIZE / 2.0f;
            float dx   = tx  - e->x;
            float dy   = ty2 - e->y;
            float dist = sqrtf(dx*dx + dy*dy);
            float step = speed * TILE_SIZE * dt;

            if (dist <= step) {
                e->x = tx; e->y = ty2;
                e->path_index++;
            } else {
                e->x += (dx / dist) * step;
                e->y += (dy / dist) * step;
            }
        }
    }
}

/* ════════════════════════════════════════════════════
   COMPTE LES ENNEMIS EN VIE
   ════════════════════════════════════════════════════ */
int enemy_pool_alive(const EnemyPool *pool) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (pool->enemies[i].active) n++;
    return n;
}
/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "enemy.h"
#include "unit.h"
#include "tower.h"
#include "combat_math.h"
#include "fx.h"                  // effets de jus (mort, secousse)
#include "../game/runperks.h"   // g_run_mods (build de run)
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
   RÉSISTANCES / FAIBLESSES
   [type][DamageType] : 1.0 = neutre, <1 = résistance, >1 = faiblesse
   Ordre : DMG_PHYSICAL(0), DMG_POISON(1), DMG_ELECTRIC(2), DMG_CRYO(3), DMG_NANO(4), DMG_FIRE(5)
   ════════════════════════════════════════════════════ */
const float ENEMY_DMG_MULT[ENEMY_TYPE_COUNT][DAMAGE_TYPE_COUNT] = {
    [ENEMY_RAIDER]      = { 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.5f }, // organique, très inflammable
    [ENEMY_BRUTE]       = { 0.7f, 1.0f, 1.4f, 1.0f, 1.0f, 1.0f }, // armure lourde, conductive
    [ENEMY_RUNNER]      = { 1.0f, 1.3f, 1.0f, 1.4f, 1.0f, 1.4f }, // léger, brûle vite, gelable
    [ENEMY_VEHICLE]     = { 0.5f, 0.4f, 0.8f, 1.6f, 1.0f, 0.7f }, // blindé, métal résiste aux flammes
    [ENEMY_MUTANT]      = { 1.0f, 0.4f, 1.3f, 1.0f, 1.0f, 0.8f }, // mutation atténue les brûlures
    [ENEMY_GHOST]       = { 0.3f, 1.0f, 1.0f, 1.0f, 2.0f, 0.5f }, // semi-incorporel, le feu le traverse
    [ENEMY_PATHBREAKER] = { 0.8f, 1.4f, 1.0f, 1.0f, 1.0f, 1.2f }, // muscle mais sans armure
    [ENEMY_HEALER]      = { 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f }, // fragile au choc électrique
    [ENEMY_HUNTER]      = { 1.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.2f }, // agile, brûle correctement
    [ENEMY_ARTILLERY]   = { 0.6f, 0.4f, 1.6f, 1.0f, 1.0f, 0.8f }, // machine, partiellement ignifugée
};

/* ════════════════════════════════════════════════════
   DESCRIPTIONS BESTIAIRE
   ════════════════════════════════════════════════════ */
const char *ENEMY_DESC[ENEMY_TYPE_COUNT] = {
    [ENEMY_RAIDER]      = "Pillard humain basique. Suit le chemin et attaque\nau corps a corps. Peu d'armure mais nombreux.",
    [ENEMY_BRUTE]       = "Colosse recouvert de plaques d'acier. Lent mais\nencaisse enormement. Frappe tres fort.",
    [ENEMY_RUNNER]      = "Eclaireur ultra-rapide. Fonce vers la base\nen evitant le gros des combats.",
    [ENEMY_VEHICLE]     = "Vehicule blinde de guerre. Presque impossible a\narreter une fois lance. Immune aux ralentissements.",
    [ENEMY_MUTANT]      = "Creature humanoide mutagene. Se regenere lentement\net resiste aux toxines grace a ses mutations.",
    [ENEMY_GHOST]       = "Entite semi-incorporelle. Invisible aux tourelles,\nseules vos unites peuvent le detecter.",
    [ENEMY_PATHBREAKER] = "Guerrier massif qui quitte le chemin a mi-parcours\npour foncer directement vers la base en ligne droite.",
    [ENEMY_HEALER]      = "Soigneur ennemi. Restaure les PV de tous les\nennemis proches en continu. Priorite critique.",
    [ENEMY_HUNTER]      = "Predateur qui traque vos unites alliees.\nIgnore le chemin et attaque vos soldats en priorite.",
    [ENEMY_ARTILLERY]   = "Machine de guerre qui s'arrete a portee et\ndetruit methodiquement vos tourelles.",
};

const char *ENEMY_SPEC[ENEMY_TYPE_COUNT] = {
    [ENEMY_RAIDER]      = "Aucune specialite. Chair a canon.",
    [ENEMY_BRUTE]       = "Armure lourde. Vulnerable aux decharges electriques.",
    [ENEMY_RUNNER]      = "Vitesse extreme. Peu de PV. Difficile a cibler.",
    [ENEMY_VEHICLE]     = "Immune aux ralentissements. Froid grippe les engrenages.",
    [ENEMY_MUTANT]      = "Regeneration passive. Tres resistant au poison.",
    [ENEMY_GHOST]       = "Invisible aux tours. Nano-armes : vulnerabilite x2.",
    [ENEMY_PATHBREAKER] = "Quitte le chemin. Fonce en ligne droite. Imprevable.",
    [ENEMY_HEALER]      = "Soin de zone continue. Eliminez-le en premier.",
    [ENEMY_HUNTER]      = "Traque les unites. Tres vulnerable aux armes physiques.",
    [ENEMY_ARTILLERY]   = "Siege : bombarde les tours. Tres sensible a l'electricite.",
};

/* ════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════ */
void enemy_pool_init(EnemyPool *pool) {
    memset(pool, 0, sizeof(EnemyPool));
    pool->raider_count = 0;
}

/* ════════════════════════════════════════════════════
   UTILITAIRE
   ════════════════════════════════════════════════════ */
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
    e->melee_range     = base->melee_range;
    e->unit_atk_range  = base->melee_range; /* défaut = mêlée ; surchargé ci-dessous pour les types à distance */
    e->path_id         = path_id;
    e->path_index  = 0;
    e->spawn_delay = spawn_delay;
    e->active      = 1;
    e->hunt_target = -1;
    e->arty_target = -1;
    e->raiding     = 0;
    e->raid_target = -1;
    e->raid_base_x = 0.0f;
    e->raid_base_y = 0.0f;

    // Ghost
    e->invisible = (type == ENEMY_GHOST) ? 1 : 0;

    // Healer
    if (type == ENEMY_HEALER) {
        e->heal_range  = ENEMY_HEALER_HEAL_RANGE  * TILE_SIZE;
        e->heal_amount = ENEMY_HEALER_HEAL_AMOUNT;
        e->heal_timer  = 0.0f;
    }

    // Hunter — longue portée de tir sur les unités
    if (type == ENEMY_HUNTER) {
        e->hunt_range      = ENEMY_HUNTER_HUNT_RANGE      * TILE_SIZE;
        e->unit_atk_range  = ENEMY_HUNTER_UNIT_ATK_RANGE  * TILE_SIZE;
    }

    // Artillery — portée tour + portée secondaire sur unités
    if (type == ENEMY_ARTILLERY) {
        e->arty_range     = ENEMY_ARTY_RANGE          * TILE_SIZE;
        e->unit_atk_range = ENEMY_ARTY_UNIT_ATK_RANGE * TILE_SIZE;
    }

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
        e->speed   *= ENEMY_PATHBREAKER_SPEED_MULT;
    }

    pool->count++;
}

/* ════════════════════════════════════════════════════
   BOSS DE FIN DE CHAPITRE
   Réutilise les résistances/comportement mêlée du Brute comme base,
   mais avec des stats massives, des PV scalés par chapitre, et un
   marqueur is_boss qui active phases (enrage) + rendu spécial.
   ════════════════════════════════════════════════════ */
void enemy_spawn_boss(EnemyPool *pool, int path_id, const PathSet *paths,
                      float hp_scale, int boss_chapter)
{
    if (pool->count >= MAX_ENEMIES) return;
    if (path_id < 0 || path_id >= paths->count) return;
    const Path *path = &paths->paths[path_id];
    if (!path->found || path->len == 0) return;

    Enemy *e = NULL;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (!pool->enemies[i].active) { e = &pool->enemies[i]; break; }
    if (!e) return;

    memset(e, 0, sizeof(Enemy));
    e->x           = path->steps[0].x * TILE_SIZE + TILE_SIZE / 2.0f;
    e->y           = path->steps[0].y * TILE_SIZE + TILE_SIZE / 2.0f;
    e->type        = ENEMY_BRUTE;                 // base résistances + mêlée
    e->is_boss     = 1;
    e->max_hp      = 1200.0f * (hp_scale > 0.0f ? hp_scale : 1.0f);
    e->hp          = e->max_hp;
    e->speed       = 0.85f;
    e->size        = 18.0f;
    e->reward      = 250;
    e->damage      = 12;
    e->melee_range    = 40.0f;
    e->unit_atk_range = 40.0f;   // sans ça, le boss ignorerait les unités (memset → 0)
    e->path_id     = path_id;
    e->path_index  = 0;
    e->spawn_delay = 0.0f;
    e->active      = 1;
    e->hunt_target = -1;
    e->arty_target = -1;
    e->raid_target = -1;

    // Capacité spéciale selon le chapitre (variété entre les boss)
    e->boss_ability  = (boss_chapter <= 1) ? BOSS_SUMMON
                     : (boss_chapter >= 4) ? BOSS_SHIELD
                     : BOSS_STUN;            // chapitres 3 & 4 (index 2,3)
    e->ability_timer = BOSS_ABILITY_PERIOD;
    pool->count++;
}

/* Multiplicateur de vitesse selon la phase (enrage à bas PV). */
static float boss_enrage_mult(const Enemy *e) {
    float r = (e->max_hp > 0.0f) ? e->hp / e->max_hp : 1.0f;
    if (r < 0.33f) return 1.6f;   // phase 3 : furie
    if (r < 0.66f) return 1.3f;   // phase 2 : accélération
    return 1.0f;                  // phase 1
}

/* ════════════════════════════════════════════════════
   DÉGÂTS
   ════════════════════════════════════════════════════ */
void enemy_damage(Enemy *e, float dmg) {
    if (!e->active || e->dead) return;
    // Boss sous bouclier : invulnérable (petit flash pour signaler le blocage)
    if (e->is_boss && e->boss_shield > 0.0f) {
        if (dmg > 0.0f) e->hit_flash = 0.35f;
        return;
    }
    e->hp -= dmg;
    if (dmg > 0.0f) e->hit_flash = 1.0f;   // éclair blanc bref (feedback)
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
    // ── Comptage raiders actifs + limite ──────────────────────────
    int raider_count_now = 0;
    int active_count     = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!pool->enemies[i].active || pool->enemies[i].dead) continue;
        active_count++;
        if (pool->enemies[i].raiding) raider_count_now++;
    }
    pool->raider_count  = raider_count_now;
    int raid_max = (int)((float)active_count * ENEMY_RAID_MAX_FRACTION);
    if (raid_max < 1) raid_max = 1;

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
            int reward = e->reward + g_run_mods.gold_per_kill;   // + perk Pillage/Fortune
            *gold  += reward;
            *kills += 1;
            // Jus : gerbe de débris + pop d'or (boss = plus gros + secousse)
            if (e->is_boss) {
                fx_burst(e->x, e->y, (Color){230, 180, 60, 255}, 30, 250.0f);
                fx_burst(e->x, e->y, (Color){200, 70, 40, 255}, 22, 160.0f);
                fx_shake(12.0f);
            } else {
                int np = (int)(e->size * 0.9f) + 4;
                fx_burst(e->x, e->y, (Color){190, 95, 60, 255}, np, 90.0f + e->size * 6.0f);
            }
            fx_popup_gold(e->x, e->y - e->size, reward);
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
                    fx_shake(3.0f + (float)e->damage);   // jus : impact sur la base
                    if (map->bases[bid].hp <= 0) {
                        map->bases[bid].hp     = 0;
                        map->bases[bid].active = 0;
                        fx_shake(11.0f);                 // base tombée
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
            e->hp += ENEMY_MUTANT_REGEN_RATE * dt;
            if (e->hp > e->max_hp) e->hp = e->max_hp;
        }

        // ── Éclair de dégât (feedback) — decay rapide ─────────
        if (e->hit_flash > 0.0f) {
            e->hit_flash -= dt * 7.0f;
            if (e->hit_flash < 0.0f) e->hit_flash = 0.0f;
        }

        // ── Capacité spéciale de boss (télégraphiée) ──────────
        if (e->is_boss && e->boss_ability != BOSS_ABILITY_NONE) {
            if (e->boss_shield > 0.0f) e->boss_shield -= dt;

            if (e->telegraph_timer > 0.0f) {
                // Préavis en cours → exécute l'effet à l'échéance
                e->telegraph_timer -= dt;
                if (e->telegraph_timer <= 0.0f) {
                    switch (e->boss_ability) {
                        case BOSS_SUMMON:
                            for (int s = 0; s < BOSS_SUMMON_COUNT; s++) {
                                EnemyType ty = (s % 2) ? ENEMY_RUNNER : ENEMY_RAIDER;
                                enemy_spawn(pool, ty, e->path_id, paths,
                                            (float)s * 0.25f, 1.0f, 1.0f);
                            }
                            break;
                        case BOSS_STUN: {
                            float r2 = (BOSS_STUN_RADIUS * TILE_SIZE) *
                                       (BOSS_STUN_RADIUS * TILE_SIZE);
                            if (towers) for (int ti = 0; ti < MAX_TOWERS; ti++) {
                                Tower *tw = &towers->towers[ti];
                                if (!tw->active) continue;
                                float tx = tw->tile_x * TILE_SIZE + TILE_SIZE / 2.0f;
                                float ty = tw->tile_y * TILE_SIZE + TILE_SIZE / 2.0f;
                                if (gdist2(e->x, e->y, tx, ty) <= r2)
                                    tw->stun_timer = BOSS_STUN_DURATION;
                            }
                            fx_shake(8.0f);
                            break;
                        }
                        case BOSS_SHIELD:
                            e->boss_shield = BOSS_SHIELD_DURATION;
                            break;
                        default: break;
                    }
                    e->ability_timer = BOSS_ABILITY_PERIOD;
                }
            } else {
                e->ability_timer -= dt;
                if (e->ability_timer <= 0.0f) {
                    e->telegraph_timer = BOSS_TELEGRAPH_TIME;   // démarre le préavis
                    audio_play_sfx(AUDIO_SFX_ENEMY_SPAWN);
                }
            }
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

        // ── Brûlure (flame stacks) — decay ───────────────
        if (e->burn_stacks > 0) {
            e->burn_decay_timer -= dt;
            if (e->burn_decay_timer <= 0.0f)
                e->burn_stacks = 0;  // plus brûlé depuis >2.5s → stacks perdus
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
                if (gdist(e->x, e->y, other->x, other->y) <= e->heal_range) {
                    other->hp += e->heal_amount * dt;
                    if (other->hp > other->max_hp)
                        other->hp = other->max_hp;
                }
            }
        }

        // ════════════════════════════════════════════════
        // RAIDER — détachement vers un ouvrier
        // ════════════════════════════════════════════════
        if (e->raiding && units) {
            int valid = 0;
            if (e->raid_target >= 0 && e->raid_target < MAX_UNITS) {
                const Unit *wu = &units->units[e->raid_target];
                if (wu->active && wu->type == UNIT_WORKER) {
                    float dist = gdist(e->x, e->y, wu->x, wu->y);
                    if (dist <= ENEMY_RAID_ABANDON_RANGE * TILE_SIZE) {
                        valid = 1;
                        if (dist <= e->melee_range) {
                            // Attaque l'ouvrier
                            if (e->atk_timer <= 0.0f) {
                                unit_damage((Unit*)wu,
                                            (float)e->damage * ENEMY_MELEE_DMG_MULT);
                                e->atk_timer = 1.0f / ENEMY_MELEE_RATE_DEFAULT;
                            }
                        } else {
                            // Fonce sur l'ouvrier
                            float dx   = wu->x - e->x;
                            float dy   = wu->y - e->y;
                            float step = e->speed * TILE_SIZE * dt;
                            e->x += (dx / dist) * step;
                            e->y += (dy / dist) * step;
                        }
                        continue; // ne suit pas le chemin normal
                    }
                }
            }
            if (!valid) {
                // Ouvrier mort ou trop loin : reprend le chemin
                e->raiding     = 0;
                e->raid_target = -1;
                raider_count_now--;
            }
        }

        // ─── Détection d'ouvrier à proximité (peut devenir raider) ──
        if (!e->raiding && e->type != ENEMY_HUNTER && e->type != ENEMY_ARTILLERY
            && units && raider_count_now < raid_max) {
            float detect_px = ENEMY_RAID_DETECT_RANGE * TILE_SIZE;
            for (int j = 0; j < MAX_UNITS; j++) {
                const Unit *wu = &units->units[j];
                if (!wu->active || wu->type != UNIT_WORKER) continue;
                // N'attire que les ouvriers en mission (pas en patrouille de base)
                if (wu->state == USTATE_PATROL) continue;
                float d = gdist(e->x, e->y, wu->x, wu->y);
                if (d <= detect_px) {
                    // Enregistre la position de base cible pour le retour
                    if (e->path_id >= 0 && e->path_id < paths->count) {
                        const Path *p = &paths->paths[e->path_id];
                        e->raid_base_x = p->steps[p->len-1].x * TILE_SIZE + TILE_SIZE/2.0f;
                        e->raid_base_y = p->steps[p->len-1].y * TILE_SIZE + TILE_SIZE/2.0f;
                    }
                    e->raiding = 1;
                    e->raid_target = j;
                    raider_count_now++;
                    break;
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
                float d = gdist(e->x, e->y, u->x, u->y);
                if (d < e->hunt_range && d < best_d) {
                    best_d = d; best_u = j;
                }
            }
            e->hunt_target = best_u;

            if (best_u != -1) {
                const Unit *u = &units->units[best_u];
                float dx   = u->x - e->x;
                float dy   = u->y - e->y;
                float dist = gdist(e->x, e->y, u->x, u->y);

                if (dist <= e->unit_atk_range) {
                    /* À portée : s'arrête et tire (longue portée) */
                    if (e->atk_timer <= 0.0f) {
                        unit_damage((Unit*)u, (float)e->damage * ENEMY_MELEE_DMG_MULT);
                        e->atk_timer = ENEMY_HUNTER_ATK_TIMER;
                    }
                    continue;
                } else {
                    /* Hors portée : se rapproche jusqu'à unit_atk_range */
                    float step = e->speed * TILE_SIZE * dt;
                    e->x += (dx / dist) * step;
                    e->y += (dy / dist) * step;
                    continue;
                }
            }
            // Pas d'unité → suit le chemin normalement (pas de continue ici)
        }

        // ════════════════════════════════════════════════
        // ARTILLERY — priorité tours, puis unités à portée
        // ════════════════════════════════════════════════
        if (e->type == ENEMY_ARTILLERY && towers) {
            float best_d = FLT_MAX;
            int   best_t = -1;
            for (int j = 0; j < MAX_TOWERS; j++) {
                Tower *tw = &towers->towers[j];
                if (!tw->active || tw->hp <= 0.0f) continue;
                float d = gdist(e->x, e->y, tw->cx, tw->cy);
                if (d <= e->arty_range && d < best_d) {
                    best_d = d; best_t = j;
                }
            }
            e->arty_target = best_t;

            if (best_t != -1) {
                /* Tour à portée : tir principal sur la tour */
                Tower *tw = &towers->towers[best_t];
                if (e->arty_timer <= 0.0f) {
                    tw->hp -= ENEMY_ARTY_DAMAGE;
                    if (tw->hp <= 0.0f) tw->hp = 0.0f;
                    e->arty_timer = ENEMY_ARTY_FIRE_TIMER;
                }
                continue; // s'arrête pendant qu'il tire
            }

            /* Pas de tour à portée — tir secondaire sur l'unité la plus proche */
            if (units) {
                float best_du = FLT_MAX;
                int   best_u  = -1;
                for (int j = 0; j < MAX_UNITS; j++) {
                    const Unit *u = &units->units[j];
                    if (!u->active) continue;
                    float d = gdist(e->x, e->y, u->x, u->y);
                    if (d <= e->unit_atk_range && d < best_du) {
                        best_du = d; best_u = j;
                    }
                }
                if (best_u != -1) {
                    if (e->atk_timer <= 0.0f) {
                        unit_damage((Unit*)&units->units[best_u],
                                    (float)e->damage * ENEMY_MELEE_DMG_MULT);
                        e->atk_timer = ENEMY_ARTY_FIRE_TIMER; /* même cadence que pour les tours */
                    }
                    continue; // s'arrête pour tirer
                }
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
                    float d = gdist(e->x, e->y, u->x, u->y);
                    if (d <= e->unit_atk_range && d < closest_dist) {
                        closest_dist = d; engaged_unit = j;
                    }
                }
            }

            if (engaged_unit != -1) {
                e->engage_timer = ENEMY_MELEE_ENGAGE_TIMER;
                if (e->atk_timer <= 0.0f) {
                    float atk_rate;
                    switch (e->type) {
                        case ENEMY_BRUTE:       atk_rate = ENEMY_MELEE_RATE_BRUTE;       break;
                        case ENEMY_RUNNER:      atk_rate = ENEMY_MELEE_RATE_RUNNER;      break;
                        case ENEMY_VEHICLE:     atk_rate = ENEMY_MELEE_RATE_VEHICLE;     break;
                        case ENEMY_GHOST:       atk_rate = ENEMY_MELEE_RATE_GHOST;       break;
                        case ENEMY_PATHBREAKER: atk_rate = ENEMY_MELEE_RATE_PATHBREAKER; break;
                        case ENEMY_HEALER:      atk_rate = ENEMY_MELEE_RATE_HEALER;      break;
                        default:                atk_rate = ENEMY_MELEE_RATE_DEFAULT;     break;
                    }
                    e->atk_timer = 1.0f / atk_rate;
                    unit_damage((Unit*)&units->units[engaged_unit],
                                (float)e->damage * ENEMY_MELEE_DMG_MULT);
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

            // ── Siège d'une base intermédiaire ────────────────────────
            // Si la case où se trouve l'ennemi est une TILE_BASE qui
            // n'est PAS la destination finale du chemin, il s'y arrête
            // et l'attaque jusqu'à destruction avant de reprendre.
            if (map) {
                Point cur = path->steps[e->path_index];
                if (map->tiles[cur.y][cur.x].type == TILE_BASE) {
                    int alive = 0;
                    for (int b = 0; b < map->base_count; b++) {
                        if (map->bases[b].pos.x == cur.x &&
                            map->bases[b].pos.y == cur.y &&
                            map->bases[b].active && map->bases[b].hp > 0) {
                            alive = 1;
                            if (e->atk_timer <= 0.0f) {
                                map->bases[b].hp -= e->damage;
                                if (map->bases[b].hp <= 0) {
                                    map->bases[b].hp     = 0;
                                    map->bases[b].active = 0;
                                }
                                *lives -= e->damage;
                                if (*lives < 0) *lives = 0;
                                e->atk_timer = ENEMY_SIEGE_ATK_TIMER;
                            }
                            break;
                        }
                    }
                    if (alive) continue; // bloqué : attend la destruction
                }
            }

            float speed = e->speed;
            if (e->slow_timer > 0.0f && e->type != ENEMY_VEHICLE)
                speed *= ENEMY_SLOW_SPEED_MULT;
            if (e->is_boss)
                speed *= boss_enrage_mult(e);   // accélère à bas PV (phases)

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
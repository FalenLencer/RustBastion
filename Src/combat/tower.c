/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "tower.h"
#include "projectile.h"
#include "combat_math.h"
#include "../game/runperks.h"   // g_run_mods (build de run)
_Static_assert(META_TOWER_COUNT == TOWER_TYPE_COUNT,
               "META_TOWER_COUNT dans meta.h ne correspond pas a TOWER_TYPE_COUNT");
#include "../engine/audio.h"
#include "../map/pathfinding.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "../game/meta.h"
#include <stdlib.h>

/* Tables LATÉRALES par slot de tour, NON sauvegardées (aucun champ ajouté à
   Tower : la section des tours des sauvegardes reste valide ; après un
   chargement, tout repart à zéro, ce qui est le bon défaut).
   • g_fresh : pose ANNULABLE — 0 = ancienne, sinon rang de pose (1, 2…) ;
   • g_tally : bilan par tour (dégâts infligés, ennemis achevés). */
static int        g_fresh[MAX_TOWERS];
static int        g_fresh_seq = 0;
static TowerTally g_tally[MAX_TOWERS];

/* ════════════════════════════════════════════════════
   STATS DE BASE
   Équilibrage MESURÉ (11/2026, banc headless : bot qui défend 8 actes avec
   une seule composition, même économie) : contrairement à l'intuition
   « DPS/or », les tours MONO-CIBLE étaient loin derrière les tours de zone
   face aux essaims — Tourelle seule 34 vagues tenues sur 96, Sniper 39,
   Flamme 65, Tesla 66. Sniper 0,4 → 0,5 tir/s : 39 → 48 (+9). La Tourelle
   est RESTÉE telle quelle : même +44 % de DPS ne lui donnait que +3 vagues
   (sa limite est la valeur par EMPLACEMENT, la limite de tours étant la
   vraie rareté) ; la gonfler aurait surtout rendu le début de campagne trop
   facile. Tours de zone intouchées (difficulté générale préservée).
   ════════════════════════════════════════════════════ */
const TowerStats TOWER_BASE_STATS[TOWER_TYPE_COUNT] = {
    [TOWER_GUN] = {
        .name="Tourelle", .cost=15, .damage=20.0f, .range=3.5f,
        .fire_rate=1.5f, .proj_speed=10.0f, .splash=0,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=0,
        .description="Polyvalente. Vise l'ennemi le plus proche.",
    },
    [TOWER_SNIPER] = {
        .name="Sniper", .cost=25, .damage=115.0f, .range=6.5f,
        .fire_rate=0.5f, .proj_speed=18.0f, .splash=0,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=0,
        .description="Longue portée. Cible l'ennemi le plus avancé.",
    },
    [TOWER_FLAME] = {
        .name="Lance-flammes", .cost=30, .damage=12.0f, .range=2.0f,
        .fire_rate=3.0f, .proj_speed=5.0f, .splash=1,
        .slow_factor=0.5f, .slow_duration=1.5f, .chain_count=0,
        .description="Zone courte. Ralentit et brûle les ennemis.",
    },
    [TOWER_TESLA] = {
        .name="Tesla", .cost=50, .damage=31.0f, .range=4.0f,
        .fire_rate=0.8f, .proj_speed=20.0f, .splash=1,
        .slow_factor=0.0f, .slow_duration=0.0f, .chain_count=2,
        .description="Chaîne sur 2 ennemis proches. Ignore l'armure.",
    },
};

const char *TOWER_LORE[TOWER_TYPE_COUNT] = {
    [TOWER_GUN] =
        "La tourelle automatique est l'épine dorsale de toute défense.\n"
        "Peu coûteuse, robuste, facile à upgrader. Son canon rotatif\n"
        "vise sans relâche l'ennemi le plus proche de sa zone.\n"
        "Idéale en masse sur les couloirs droits.",

    [TOWER_SNIPER] =
        "Construite à partir de pièces de fusil de précision récupérées.\n"
        "Un seul tir suffisant pour traverser un blindage léger. La balle\n"
        "cherche toujours l'ennemi le plus avancé — celui qui est\n"
        "le plus près de votre base.",

    [TOWER_FLAME] =
        "Alimentée par des bonbonnes de carburant récupérées en zone industrielle.\n"
        "Projette un cône de feu persistant qui inflige des dégâts sur la durée\n"
        "et ralentit les survivants de 50%. Dévastatrice sur les groupes serrés.\n"
        "Portée courte — à placer dans les goulots d'étranglement.",

    [TOWER_TESLA] =
        "Générateur électromagnétique assemblé depuis des équipements militaires.\n"
        "L'arc initial rebondit sur 2 cibles supplémentaires dans un rayon proche.\n"
        "Les dégâts électriques ignorent la résistance physique. Parfaite contre\n"
        "les essaims et les ennemis groupes.",
};

/* ════════════════════════════════════════════════════
   COÛTS DES AMÉLIORATIONS
   ════════════════════════════════════════════════════ */
const int TOWER_UPG_COST_DMG  [TOWER_UPG_MAX] = { 25,  45,  75, 115, 170 };
const int TOWER_UPG_COST_RANGE[TOWER_UPG_MAX] = { 20,  35,  55,  85, 125 };
const int TOWER_UPG_COST_RATE [TOWER_UPG_MAX] = { 30,  55,  90, 140, 210 };

/* ════════════════════════════════════════════════════
   UTILITAIRES
   ════════════════════════════════════════════════════ */
static float px_of(int tile) {
    return tile * TILE_SIZE + TILE_SIZE * 0.5f;
}

/* ════════════════════════════════════════════════════
   LIMITE ACTIVE
   ════════════════════════════════════════════════════ */
int tower_active_limit(const MetaBonuses *bonuses) {
    int extra = bonuses ? bonuses->tower_limit_bonus : 0;
    int limit = MAX_TOWERS_BASE + extra * MAX_TOWERS_UPGR;
    if (limit > MAX_TOWERS_HARD) limit = MAX_TOWERS_HARD;
    return limit;
}

/* ════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════ */
void tower_pool_init(TowerPool *tp) {
    memset(tp, 0, sizeof(TowerPool));
    tp->tower_limit = MAX_TOWERS_BASE;
    tower_fresh_clear();
    memset(g_tally, 0, sizeof(g_tally));   /* bilan par tour : nouvelle partie / chargement */
    projectile_src_reset();
}

/* ════════════════════════════════════════════════════
   VÉRIFICATION PLACEMENT — inclut zone anti-spawn
   ════════════════════════════════════════════════════ */
/* Règles de placement — IMPLÉMENTATION UNIQUE, qui rend en plus la RAISON
   du refus pour que l'UI puisse l'expliquer au joueur. */
TowerPlaceFail tower_place_fail(const TowerPool *tp, const Map *map,
                                int tx, int ty) {
    if (tx < 0 || tx >= map->w || ty < 0 || ty >= map->h) return TPF_TILE;
    if (!map->tiles[ty][tx].buildable)                    return TPF_TILE;

    // Zone d'exclusion autour de chaque spawn
    for (int i = 0; i < map->path_count; i++) {
        if (!map->paths[i].active) continue;
        Point sp   = map->paths[i].spawn;
        int   dist = abs(tx - sp.x) + abs(ty - sp.y);
        if (dist <= SPAWN_EXCLUSION_RADIUS) return TPF_SPAWN;
    }

    // Pas de tour déjà là
    if (tower_at_tile(tp, tx, ty)) return TPF_OCCUPIED;
    return TPF_NONE;
}

int tower_can_place(const TowerPool *tp, const Map *map, int tx, int ty) {
    return tower_place_fail(tp, map, tx, ty) == TPF_NONE;
}

/* 1 si une tour active occupe la tuile — source unique du déblaiement
   du décor (ruine/roche) sous une tour, en 2D comme en 3D. */
int tower_at_tile(const TowerPool *tp, int tx, int ty) {
    if (!tp) return 0;
    for (int i = 0; i < MAX_TOWERS; i++) {
        const Tower *tw = &tp->towers[i];
        if (tw->active && tw->tile_x == tx && tw->tile_y == ty) return 1;
    }
    return 0;
}

/* Tout ce que la tour a coûté : pose (×2 sur ruine) + améliorations payées. */
static int tower_spent(const Tower *t, const Map *map) {
    int spent = tower_cost_on_tile(t->type, map, t->tile_x, t->tile_y);
    for (int i = 0; i < t->upg_dmg   && i < TOWER_UPG_MAX; i++) spent += TOWER_UPG_COST_DMG[i];
    for (int i = 0; i < t->upg_range && i < TOWER_UPG_MAX; i++) spent += TOWER_UPG_COST_RANGE[i];
    for (int i = 0; i < t->upg_rate  && i < TOWER_UPG_MAX; i++) spent += TOWER_UPG_COST_RATE[i];
    return spent;
}

int tower_sell_refund(const Tower *t, const Map *map) {
    if (!t || !map) return 0;
    float frac = TOWER_SELL_REFUND + g_run_mods.sell_refund_add;
    if (frac > 1.0f) frac = 1.0f;          // perk Recyclage : plafond 100 %
    return (int)((float)tower_spent(t, map) * frac);
}

/* ── Pose ANNULABLE (tables en tête de fichier) ── */
void tower_fresh_clear(void) {
    memset(g_fresh, 0, sizeof(g_fresh));
    g_fresh_seq = 0;
}
int tower_is_fresh(const TowerPool *tp, int idx) {
    return tp && idx >= 0 && idx < MAX_TOWERS && tp->towers[idx].active && g_fresh[idx] > 0;
}
int tower_last_fresh(const TowerPool *tp) {
    int best = -1;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (tower_is_fresh(tp, i) && (best < 0 || g_fresh[i] > g_fresh[best])) best = i;
    return best;
}
/* ── Bilan par tour (cf. tower.h) ── */
void tower_tally_add(int idx, float dmg, int kill) {
    if (idx < 0 || idx >= MAX_TOWERS) return;
    if (dmg > 0.0f) { g_tally[idx].dmg += dmg; g_tally[idx].dmg_wave += dmg; }
    if (kill)       { g_tally[idx].kills++;    g_tally[idx].kills_wave++; }
}
const TowerTally *tower_tally(int idx) {
    static const TowerTally none = {0};
    return (idx >= 0 && idx < MAX_TOWERS) ? &g_tally[idx] : &none;
}
int tower_wave_mvp(const TowerPool *tp) {
    int best = -1;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (tp->towers[i].active && g_tally[i].dmg_wave > 0.0f &&
            (best < 0 || g_tally[i].dmg_wave > g_tally[best].dmg_wave)) best = i;
    return best;
}
void tower_wave_tick(int in_wave) {
    static int prev = 0;
    if (in_wave && !prev)
        for (int i = 0; i < MAX_TOWERS; i++) g_tally[i].dmg_wave = 0.0f, g_tally[i].kills_wave = 0;
    if (in_wave) tower_fresh_clear();
    prev = in_wave;
}

int tower_refund(const TowerPool *tp, int idx, const Map *map) {
    if (!tp || !map || idx < 0 || idx >= MAX_TOWERS) return 0;
    const Tower *t = &tp->towers[idx];
    return tower_is_fresh(tp, idx) ? tower_spent(t, map) : tower_sell_refund(t, map);
}

int tower_path_coverage(const Map *map, int tx, int ty, float range_tiles) {
    if (!map || range_tiles <= 0.0f) return 0;
    int   r  = (int)ceilf(range_tiles);
    float r2 = range_tiles * range_tiles;
    int   n  = 0;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int nx = tx + dx, ny = ty + dy;
            if (nx < 0 || ny < 0 || nx >= map->w || ny >= map->h) continue;
            if ((float)(dx * dx + dy * dy) > r2) continue;
            if (map->tiles[ny][nx].type == TILE_PATH) n++;
        }
    }
    return n;
}

/* ════════════════════════════════════════════════════
   COÛT RÉEL SELON LA TUILE
   ════════════════════════════════════════════════════ */
int tower_cost_on_tile(TowerType type, const Map *map, int tile_x, int tile_y) {
    int base_cost = TOWER_BASE_STATS[type].cost;
    if (tile_x < 0 || tile_x >= map->w || tile_y < 0 || tile_y >= map->h)
        return base_cost;
    if (map->tiles[tile_y][tile_x].type == TILE_RUIN)
        return base_cost * TOWER_RUIN_COST_MULT;
    return base_cost;
}

/* ════════════════════════════════════════════════════
   PLACEMENT D'UNE TOUR
   ════════════════════════════════════════════════════ */
int tower_place(TowerPool *tp, TowerType type,
                int tile_x, int tile_y, Map *map,
                int *gold, const MetaBonuses *bonuses)
{
    if (!tower_can_place(tp, map, tile_x, tile_y)) return 0;
    if (tp->tower_count >= tp->tower_limit) return 0;

    int real_cost = tower_cost_on_tile(type, map, tile_x, tile_y);
    if (*gold < real_cost) return 0;

    Tower *tw = NULL;
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!tp->towers[i].active) { tw = &tp->towers[i]; break; }
    }
    if (!tw) return 0;

    *gold -= real_cost;
    g_fresh[tw - tp->towers] = ++g_fresh_seq;   /* annulable jusqu'au lancement */
    g_tally[tw - tp->towers] = (TowerTally){0};  /* bilan propre à cette tour */

    tw->type      = type;
    tw->tile_x    = tile_x;
    tw->tile_y    = tile_y;
    tw->cx        = px_of(tile_x);
    tw->cy        = px_of(tile_y);
    tw->target_mode = TGT_AUTO;
    tw->active    = 1;
    tw->angle     = 0.0f;
    tw->hp        = TOWER_MAX_HP;

    // Modificateurs rogue-lite (build de run) appliqués par-dessus le meta.
    float rl_dmg   = g_run_mods.tdmg_all *
                     (type < TOWER_TYPE_COUNT ? g_run_mods.tdmg_type[type] : 1.0f);
    tw->damage    = TOWER_BASE_STATS[type].damage    * (bonuses ? bonuses->tower_dmg_mult   : 1.0f) * rl_dmg;
    tw->range     = TOWER_BASE_STATS[type].range     * (bonuses ? bonuses->tower_range_mult : 1.0f) * g_run_mods.trange_all;
    tw->fire_rate = TOWER_BASE_STATS[type].fire_rate * (bonuses ? bonuses->tower_rate_mult  : 1.0f) * g_run_mods.trate_all;

    // Sauvegarde des stats de base (avant upgrades individuels)
    tw->base_damage    = tw->damage;
    tw->base_range     = tw->range;
    tw->base_fire_rate = tw->fire_rate;
    tw->upg_dmg        = 0;
    tw->upg_range      = 0;
    tw->upg_rate       = 0;

    tw->dmg_type     = DMG_PHYSICAL;
    tw->material     = MAT_NONE;
    tw->mat_dmg_mult = 1.0f;
    switch (type) {
        case TOWER_FLAME: tw->dmg_type = DMG_FIRE;      break;
        case TOWER_TESLA: tw->dmg_type = DMG_ELECTRIC;  break;
        default:          tw->dmg_type = DMG_PHYSICAL;  break;
    }

    tw->fire_timer = 1.0f / tw->fire_rate;

    map->tiles[tile_y][tile_x].buildable = 0;
    tp->tower_count++;
    return 1;
}

/* ════════════════════════════════════════════════════
   AMÉLIORATIONS INDIVIDUELLES
   ════════════════════════════════════════════════════ */
int tower_upg_next_cost_dmg(const Tower *t) {
    if (t->upg_dmg >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_DMG[t->upg_dmg];
}
int tower_upg_next_cost_range(const Tower *t) {
    if (t->upg_range >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_RANGE[t->upg_range];
}
int tower_upg_next_cost_rate(const Tower *t) {
    if (t->upg_rate >= TOWER_UPG_MAX) return -1;
    return TOWER_UPG_COST_RATE[t->upg_rate];
}

void tower_upgrade_dmg(Tower *t) {
    if (t->upg_dmg >= TOWER_UPG_MAX) return;
    t->upg_dmg++;
    // Inclut le multiplicateur de matériau → le bonus n'est plus effacé par un upgrade.
    t->damage = t->base_damage * (1.0f + t->upg_dmg * TOWER_UPG_DMG_MULT) * t->mat_dmg_mult;
}

void tower_set_material(Tower *t, MaterialType mat) {
    if (!t) return;
    float mult = 1.0f;
    switch (mat) {
        case MAT_IRON:   mult = 1.45f; break;                 // +45% (garde le type de dégâts)
        case MAT_ACID:   t->dmg_type = DMG_POISON;   mult = 1.25f; break;
        case MAT_PLASMA: t->dmg_type = DMG_ELECTRIC; mult = 1.30f; break;
        case MAT_CRYO:   t->dmg_type = DMG_CRYO;     mult = 1.20f; break;
        case MAT_NANO:   t->dmg_type = DMG_NANO;     mult = 1.20f; break;
        default: return;
    }
    t->material     = mat;
    t->mat_dmg_mult = mult;   // remplace proprement (pas d'empilement)
    t->damage = t->base_damage * (1.0f + t->upg_dmg * TOWER_UPG_DMG_MULT) * t->mat_dmg_mult;
}
void tower_upgrade_range(Tower *t) {
    if (t->upg_range >= TOWER_UPG_MAX) return;
    t->upg_range++;
    t->range = t->base_range * (1.0f + t->upg_range * TOWER_UPG_RANGE_MULT);
}
void tower_upgrade_rate(Tower *t) {
    if (t->upg_rate >= TOWER_UPG_MAX) return;
    t->upg_rate++;
    t->fire_rate = t->base_fire_rate * (1.0f + t->upg_rate * TOWER_UPG_RATE_MULT);
}

void tower_do_repair(Tower *t) {
    t->hp = TOWER_MAX_HP;
}

/* ════════════════════════════════════════════════════
   CIBLAGE — priorité choisie (AUTO : Healer d'abord, puis règle du type)
   ════════════════════════════════════════════════════ */
const char *const TARGET_MODE_NAMES[TGT_COUNT] = {
    [TGT_AUTO] = "Auto", [TGT_FIRST] = "Premier", [TGT_LAST] = "Dernier",
    [TGT_STRONG] = "Plus fort", [TGT_NEAR] = "Proche",
};

/* Score d'un ennemi à portée (le plus haut est visé). AUTO reproduit À
   L'IDENTIQUE l'ancienne règle (équilibrage inchangé par défaut). */
static float target_score(const Tower *tw, const Enemy *e, float d2) {
    int mode = tw->target_mode;
    if (mode <= TGT_AUTO || mode >= TGT_COUNT) {
        if (e->type == ENEMY_HEALER) return TOWER_HEALER_PRIORITY;   // priorité absolue
        mode = (tw->type == TOWER_SNIPER) ? TGT_FIRST : TGT_NEAR;
    }
    switch (mode) {
    case TGT_FIRST:  return  (float)e->path_index;   // le plus avancé vers la base
    case TGT_LAST:   return -(float)e->path_index;   // le plus en arrière
    case TGT_STRONG: return  e->hp;                  // le plus de PV restants
    default:         return -d2;                     // le plus proche
    }
}

static int find_target(const Tower *tw, const EnemyPool *ep) {
    float rng  = tw->range * TILE_SIZE;
    float rng2 = rng * rng;

    int   best_idx   = -1;
    float best_score = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;
        if (e->invisible) continue;

        float d2 = gdist2(tw->cx, tw->cy, e->x, e->y);
        if (d2 > rng2) continue;

        float score = target_score(tw, e, d2);
        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
    }
    return best_idx;
}

/* Vitesse de rotation de visée des tours (rad/s) : la tourelle tourne vers
   la cible en CONTINU, pas seulement au tir (sinon elle « saute » à chaque
   coup au lieu de suivre l'ennemi qui bouge). */
#define TOWER_TURN_SPEED  9.0f

/* Fait tourner `cur` vers `tgt` d'au plus `max_d` rad (chemin le plus court). */
/* angle_approach : source unique dans combat_math.h */

/* MODE HÉROS : tour actuellement pilotée à la main (-1 = aucune). */
int g_tower_manual_control = -1;

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void tower_pool_update(TowerPool *tp, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *tw = &tp->towers[i];
        if (!tw->active) continue;

        // Étourdie par une onde EMP de boss → ne tire pas
        if (tw->stun_timer > 0.0f) {
            tw->stun_timer -= dt;
            continue;
        }

        // Pilotée par le HÉROS : visée/tir manuels (hero_tower.c) —
        // seul le cooldown continue de s'écouler ici.
        if (i == g_tower_manual_control) {
            tw->fire_timer -= dt;
            continue;
        }

        // ── VISÉE CONTINUE : on cherche la cible CHAQUE frame et la tourelle
        //    pivote vers elle progressivement (même entre deux tirs). ──
        int target = find_target(tw, ep);
        if (target != -1) {
            const Enemy *e = &ep->enemies[target];
            float desired = atan2f(e->y - tw->cy, e->x - tw->cx);
            tw->angle = angle_approach(tw->angle, desired, TOWER_TURN_SPEED * dt);
        }

        tw->fire_timer -= dt;
        if (tw->fire_timer > 0.0f) continue;
        if (target == -1) continue;

        /* Réserve pleine : on retente à la frame suivante au lieu de
           perdre le tir ET de repartir sur un cycle complet. */
        if (!projectile_spawn(tp, tw, target, ep)) continue;
        switch (tw->type) {
            case TOWER_GUN:    audio_play_sfx(AUDIO_SFX_TOWER_FIRE_GUN);    break;
            case TOWER_SNIPER: audio_play_sfx(AUDIO_SFX_TOWER_FIRE_SNIPER); break;
            case TOWER_FLAME:  audio_play_sfx(AUDIO_SFX_TOWER_FIRE_FLAME);  break;
            case TOWER_TESLA:  audio_play_sfx(AUDIO_SFX_TOWER_FIRE_TESLA);  break;
            default: break;
        }
        tw->fire_timer = 1.0f / tw->fire_rate;
    }

    projectile_update(tp, ep, dt);
}
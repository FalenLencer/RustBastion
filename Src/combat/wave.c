/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "wave.h"
#include "../engine/audio.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define PREP_TIME            20.0f
#define BASE_ENEMIES          6
#define SPAWN_INTERVAL        0.5f
#define WAVE_ENEMIES_PER_WAVE 3      /* ennemis supplémentaires par vague (base linéaire) */
#define WAVE_QUAD_DIV         3      /* diviseur terme quadratique (accélération)         */
#define WAVE_SCALE_GROWTH     1.20f  /* facteur de croissance du scaling HP   */
#define WAVE_SCALE_CAP        6.0f   /* plafond du scaling                    */
#define WAVE_SMART_RATIO      6      /* sur 10 : prob. de ciblage intelligent */
#define WAVE_RAIDER_RESERVE   0.95f  /* prob. cumulée max des non-raiders     */

void wave_init(WaveManager *wm) {
    memset(wm, 0, sizeof(WaveManager));
    wm->state      = WAVE_IDLE;
    wm->number     = 0;
    wm->prep_timer = PREP_TIME;
    wm->scale      = 1.0f;
    wm->scale_cap  = WAVE_SCALE_CAP;
    wm->count_mult = 1.0f;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        wm->arcade_bias[i] = 1.0f;   // neutre par défaut (campagne)
}

/* ════════════════════════════════════════════════════
   BIAIS ALÉATOIRE ARCADE
   Tire 2 types "dominants" (×1.5–2.5) et 2 types "rares" (×0.2–0.4)
   parmi tous les types sauf RAIDER (qui sert de fallback).
   Mélange Fisher-Yates pour garantir une sélection uniforme.
   ════════════════════════════════════════════════════ */
void wave_arcade_bias_init(WaveManager *wm) {
    // Réinitialise tout à 1.0
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        wm->arcade_bias[i] = 1.0f;

    // Pool : tous les types sauf RAIDER
    int pool[ENEMY_TYPE_COUNT];
    int n = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (i != (int)ENEMY_RAIDER) pool[n++] = i;

    // Mélange Fisher-Yates
    for (int i = n - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }

    // 2 dominants : biais 1.5 à 2.5
    for (int i = 0; i < 2 && i < n; i++)
        wm->arcade_bias[pool[i]] = 1.5f + GetRandomValue(0, 100) / 100.0f;

    // 2 rares : biais 0.2 à 0.4
    for (int i = 2; i < 4 && i < n; i++)
        wm->arcade_bias[pool[i]] = 0.2f + GetRandomValue(0, 20) / 100.0f;
}

/* ════════════════════════════════════════════════════
   DÉBLOCAGE DES ENNEMIS EN CAMPAGNE
   Seuil = meta_max_stage_completed() requis (-1 = toujours)
   Ex : threshold=2 → dispo quand ≥2 actes sont finis
   ════════════════════════════════════════════════════ */
static const int ENEMY_UNLOCK_AT[ENEMY_TYPE_COUNT] = {
    [ENEMY_RAIDER]      = -1, // toujours disponible
    [ENEMY_BRUTE]       =  0, // après acte 0  → Ch.1-Acte 2 et après
    [ENEMY_RUNNER]      =  1, // après acte 1  → Ch.1-Acte 3 et après
    [ENEMY_MUTANT]      =  2, // après acte 2  → Ch.2-Acte 1 et après
    [ENEMY_VEHICLE]     =  2, // après acte 2  → Ch.2-Acte 1 et après
    [ENEMY_GHOST]       =  3, // après acte 3  → Ch.2-Acte 2 et après
    [ENEMY_PATHBREAKER] =  4, // après acte 4  → Ch.2-Acte 3 et après
    [ENEMY_HEALER]      =  5, // après acte 5  → Ch.3-Acte 1 et après
    [ENEMY_ARTILLERY]   =  6, // après acte 6  → Ch.3-Acte 2 et après
    [ENEMY_HUNTER]      =  7, // après acte 7  → Ch.3-Acte 3 et après
};

/* ════════════════════════════════════════════════════
   TYPE D'ENNEMI
   bias : tableau arcade_bias du WaveManager (1.0f en campagne)
   max_stage : campaign_stage-1 en campagne (déblocage progressif)
   ════════════════════════════════════════════════════ */
static EnemyType pick_enemy_type(int wave_num, ThemeID theme,
                                  int is_campaign, int max_stage,
                                  const float *bias) {
    // ── Probabilités de base (fonctions de la vague) ──────────────
    float p_vehicle     = fminf(0.05f * (wave_num / 3.0f), 0.12f);
    float p_brute       = fminf(0.10f + wave_num * 0.02f,  0.25f);
    float p_runner      = fminf(0.10f + wave_num * 0.02f,  0.20f);
    float p_mutant      = 0.10f;
    float p_ghost       = fminf(wave_num >= 4 ? 0.05f+(wave_num-4)*0.02f : 0.0f, 0.15f);
    float p_pathbreaker = fminf(wave_num >= 6 ? 0.04f+(wave_num-6)*0.01f : 0.0f, 0.10f);
    float p_healer      = fminf(wave_num >= 5 ? 0.04f+(wave_num-5)*0.01f : 0.0f, 0.08f);
    float p_hunter      = fminf(wave_num >= 7 ? 0.04f+(wave_num-7)*0.01f : 0.0f, 0.10f);
    float p_artillery   = fminf(wave_num >= 8 ? 0.03f+(wave_num-8)*0.005f: 0.0f, 0.06f);

    // ── Modificateurs de thème ────────────────────────────────────
    if (theme == THEME_SWAMP)   { p_mutant += 0.10f; p_ghost       += 0.05f; }
    if (theme == THEME_DESERT)  { p_runner += 0.10f; p_pathbreaker += 0.05f; }
    if (theme == THEME_FACTORY) { p_brute  += 0.08f; p_vehicle     += 0.04f; }
    if (theme == THEME_CITY)    { p_ghost  += 0.05f; p_pathbreaker += 0.05f; }

    // ── Campagne : annule les ennemis non encore introduits ───────
    // max_stage = campaign_stage-1 → l'ennemi s'introduit exactement
    // au bon acte, indépendamment de la progression méta globale.
    if (is_campaign) {
        if (ENEMY_UNLOCK_AT[ENEMY_BRUTE]       > max_stage) p_brute       = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_RUNNER]      > max_stage) p_runner      = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_MUTANT]      > max_stage) p_mutant      = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_VEHICLE]     > max_stage) p_vehicle     = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_GHOST]       > max_stage) p_ghost       = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_PATHBREAKER] > max_stage) p_pathbreaker = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_HEALER]      > max_stage) p_healer      = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_ARTILLERY]   > max_stage) p_artillery   = 0.0f;
        if (ENEMY_UNLOCK_AT[ENEMY_HUNTER]      > max_stage) p_hunter      = 0.0f;
    }

    // ── Arcade : applique les biais aléatoires de la partie ───────
    // Chaque run a 2 types dominants et 2 types rares tirés au sort.
    if (!is_campaign) {
        p_vehicle     *= bias[ENEMY_VEHICLE];
        p_brute       *= bias[ENEMY_BRUTE];
        p_runner      *= bias[ENEMY_RUNNER];
        p_mutant      *= bias[ENEMY_MUTANT];
        p_ghost       *= bias[ENEMY_GHOST];
        p_pathbreaker *= bias[ENEMY_PATHBREAKER];
        p_healer      *= bias[ENEMY_HEALER];
        p_hunter      *= bias[ENEMY_HUNTER];
        p_artillery   *= bias[ENEMY_ARTILLERY];

        // Normalise pour que RAIDER garde ~5% de chance (fallback)
        float total = p_vehicle + p_brute + p_runner + p_mutant + p_ghost
                    + p_pathbreaker + p_healer + p_hunter + p_artillery;
        if (total > WAVE_RAIDER_RESERVE) {
            float scale = WAVE_RAIDER_RESERVE / total;
            p_vehicle     *= scale; p_brute       *= scale;
            p_runner      *= scale; p_mutant       *= scale;
            p_ghost       *= scale; p_pathbreaker  *= scale;
            p_healer      *= scale; p_hunter        *= scale;
            p_artillery   *= scale;
        }
    }

    // ── Sélection cumulative ──────────────────────────────────────
    float r   = (float)GetRandomValue(0, 10000) / 10000.0f;
    float cum = 0.0f;
    cum += p_vehicle;     if (r < cum) return ENEMY_VEHICLE;
    cum += p_brute;       if (r < cum) return ENEMY_BRUTE;
    cum += p_runner;      if (r < cum) return ENEMY_RUNNER;
    cum += p_mutant;      if (r < cum) return ENEMY_MUTANT;
    cum += p_ghost;       if (r < cum) return ENEMY_GHOST;
    cum += p_pathbreaker; if (r < cum) return ENEMY_PATHBREAKER;
    cum += p_healer;      if (r < cum) return ENEMY_HEALER;
    cum += p_hunter;      if (r < cum) return ENEMY_HUNTER;
    cum += p_artillery;   if (r < cum) return ENEMY_ARTILLERY;
    return ENEMY_RAIDER;
}

/* ════════════════════════════════════════════════════
   CHOIX DU CHEMIN — INTELLIGENT ET ALÉATOIRE
   ════════════════════════════════════════════════════
   
   Logique :
   - Chaque ennemi choisit parmi TOUS les chemins disponibles
   - Pondération : les bases les moins attaquées reçoivent plus
     d'ennemis (comportement "intelligent")
   - Un peu d'aléatoire pur pour garder l'incertitude

   Résultat :
   - 1 spawn + 2 bases → les ennemis se répartissent sur les 2
   - 2 spawns + 1 base → les 2 chemins sont utilisés
   - 3 spawns + 2 bases → répartition imprévisible

   wm->base_pressure[b] = nombre d'ennemis déjà envoyés vers base b
   ════════════════════════════════════════════════════ */
/* Retourne 1 si le chemin p est utilisable (A* valide + base active) */
static int path_usable(int p, const PathSet *paths, const Map *map) {
    if (!paths->paths[p].found) return 0;
    int bid = paths->paths[p].base_id;
    if (!map) return 1;
    if (bid < 0 || bid >= map->base_count) return 0;
    return map->bases[bid].active;
}

static int pick_path(WaveManager *wm, const PathSet *paths, const Map *map) {
    if (paths->count == 0) return -1;

    // Ne considérer que les chemins avec un A* valide ET une base active
    {
        int valid = 0;
        int first = -1;
        for (int p = 0; p < paths->count; p++) {
            if (path_usable(p, paths, map)) { valid++; first = p; }
        }
        if (valid == 0) return -1;
        if (valid == 1) return first;
    }

    // Trouve les bases distinctes actives dans le PathSet
    int base_ids[MAX_PATHS];
    int base_count = 0;
    for (int p = 0; p < paths->count; p++) {
        if (!path_usable(p, paths, map)) continue;
        int bid = paths->paths[p].base_id;
        int already = 0;
        for (int i = 0; i < base_count; i++)
            if (base_ids[i] == bid) { already = 1; break; }
        if (!already) base_ids[base_count++] = bid;
    }

    // Mode intelligent : 60% du temps on cible la base la moins attaquée
    // Mode aléatoire  : 40% du temps on choisit n'importe quel chemin
    int use_smart = (GetRandomValue(0, 9) < WAVE_SMART_RATIO);

    if (use_smart && base_count > 1) {
        // Trouve la base avec le moins de pression
        int min_pressure = 999999;
        int target_base  = base_ids[0];
        for (int i = 0; i < base_count; i++) {
            int bid = base_ids[i];
            int pressure = (bid < MAX_BASES) ? wm->base_pressure[bid] : 0;
            if (pressure < min_pressure) {
                min_pressure = pressure;
                target_base  = bid;
            }
        }

        // Parmi les chemins valides qui vont vers cette base, en choisit un au hasard
        int candidates[MAX_PATHS];
        int ncand = 0;
        for (int p = 0; p < paths->count; p++) {
            if (path_usable(p, paths, map) && paths->paths[p].base_id == target_base)
                candidates[ncand++] = p;
        }
        if (ncand > 0) {
            int chosen = candidates[GetRandomValue(0, ncand-1)];
            if (target_base < MAX_BASES)
                wm->base_pressure[target_base]++;
            return chosen;
        }
    }

    // Mode aléatoire pur : n'importe quel chemin valide
    int valid_paths[MAX_PATHS];
    int nvalid = 0;
    for (int p = 0; p < paths->count; p++)
        if (path_usable(p, paths, map)) valid_paths[nvalid++] = p;
    if (nvalid == 0) return -1;
    int chosen = valid_paths[GetRandomValue(0, nvalid - 1)];
    int bid = paths->paths[chosen].base_id;
    if (bid >= 0 && bid < MAX_BASES)
        wm->base_pressure[bid]++;
    return chosen;
}

/* ════════════════════════════════════════════════════
   MISE À JOUR
   ════════════════════════════════════════════════════ */
void wave_update(WaveManager *wm, EnemyPool *pool,
                 const PathSet *paths, const Map *map,
                 const Theme *theme, float dt,
                 int is_campaign, int max_stage)
{
    switch (wm->state) {

        case WAVE_IDLE:
            wm->prep_timer -= dt;
            if (wm->prep_timer <= 0.0f)
                wave_start(wm);
            break;

        case WAVE_SPAWNING: {
            if (paths->count == 0) {
                wm->state = WAVE_COMPLETE;
                break;
            }

            int n = wm->number - 1; // vague 1 = n=0
            int count = (int)((BASE_ENEMIES
                      + n * WAVE_ENEMIES_PER_WAVE
                      + n * n / WAVE_QUAD_DIV)  // accélération quadratique
                      * wm->count_mult + 0.5f);
            if (count < 1)          count = 1;
            if (count > MAX_ENEMIES) count = MAX_ENEMIES;
            if (wm->total_spawned < count) {
                // Choix intelligent du chemin
                int path_id = pick_path(wm, paths, map);
                if (path_id < 0) {
                    // Aucun chemin valide — on saute ce spawn
                    wm->total_spawned++;
                } else {
                    float delay = wm->total_spawned * SPAWN_INTERVAL;
                    EnemyType type = pick_enemy_type(wm->number, theme->id,
                                                     is_campaign, max_stage,
                                                     wm->arcade_bias);
                    enemy_spawn(pool, type, path_id, paths,
                                delay, wm->scale, theme->enemy_speed_mult);
                    wm->total_spawned++;
                }
                wm->total_to_spawn = count;
            } else {
                wm->state = WAVE_ONGOING;
            }
            break;
        }

        case WAVE_ONGOING:
            if (enemy_pool_alive(pool) == 0)
                wm->state = WAVE_COMPLETE;
            break;

        case WAVE_COMPLETE:
            wm->state         = WAVE_IDLE;
            wm->prep_timer    = PREP_TIME;
            wm->total_spawned = 0;
            wm->scale        *= WAVE_SCALE_GROWTH;
            if (wm->scale > wm->scale_cap) wm->scale = wm->scale_cap; // cap — évite la falaise de difficulté
            // Remet la pression à zéro pour la prochaine vague
            for (int i = 0; i < MAX_BASES; i++)
                wm->base_pressure[i] = 0;
            break;
    }
}

int wave_ready(const WaveManager *wm) {
    return wm->state == WAVE_IDLE;
}

void wave_start(WaveManager *wm) {
    wm->number++;
    wm->state         = WAVE_SPAWNING;
    wm->total_spawned = 0;
    audio_play_sfx(AUDIO_SFX_WAVE_START);
}
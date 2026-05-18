#include "wave.h"
#include "../engine/audio.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define PREP_TIME      20.0f
#define BASE_ENEMIES   6
#define SPAWN_INTERVAL 0.5f

void wave_init(WaveManager *wm) {
    memset(wm, 0, sizeof(WaveManager));
    wm->state      = WAVE_IDLE;
    wm->number     = 0;
    wm->prep_timer = PREP_TIME;
    wm->scale      = 1.0f;
}

/* ════════════════════════════════════════════════════
   TYPE D'ENNEMI
   ════════════════════════════════════════════════════ */
static EnemyType pick_enemy_type(int wave_num, ThemeID theme) {
    float r = (float)GetRandomValue(0, 100) / 100.0f;

    float p_vehicle     = fminf(0.05f * (wave_num / 3.0f), 0.12f);
    float p_brute       = fminf(0.10f + wave_num * 0.02f,  0.25f);
    float p_runner      = fminf(0.10f + wave_num * 0.02f,  0.20f);
    float p_mutant      = 0.10f;
    float p_ghost       = fminf(wave_num >= 4 ? 0.05f+(wave_num-4)*0.02f : 0.0f, 0.15f);
    float p_pathbreaker = fminf(wave_num >= 6 ? 0.04f+(wave_num-6)*0.01f : 0.0f, 0.10f);
    float p_healer    = fminf(wave_num >= 5  ? 0.04f+(wave_num-5)*0.01f  : 0.0f, 0.08f);
    float p_hunter    = fminf(wave_num >= 7  ? 0.04f+(wave_num-7)*0.01f  : 0.0f, 0.10f);
    float p_artillery = fminf(wave_num >= 8  ? 0.03f+(wave_num-8)*0.005f : 0.0f, 0.06f);

    if (theme == THEME_SWAMP)   { p_mutant += 0.10f; p_ghost       += 0.05f; }
    if (theme == THEME_DESERT)  { p_runner += 0.10f; p_pathbreaker += 0.05f; }
    if (theme == THEME_FACTORY) { p_brute  += 0.08f; p_vehicle     += 0.04f; }
    if (theme == THEME_CITY)    { p_ghost  += 0.05f; p_pathbreaker += 0.05f; }

    if (r < p_vehicle)                                                            return ENEMY_VEHICLE;
    if (r < p_vehicle + p_brute)                                                  return ENEMY_BRUTE;
    if (r < p_vehicle + p_brute + p_runner)                                       return ENEMY_RUNNER;
    if (r < p_vehicle + p_brute + p_runner + p_mutant)                           return ENEMY_MUTANT;
    if (r < p_vehicle + p_brute + p_runner + p_mutant + p_ghost)                 return ENEMY_GHOST;
    if (r < p_vehicle + p_brute + p_runner + p_mutant + p_ghost + p_pathbreaker) return ENEMY_PATHBREAKER;
    if (r < p_vehicle + p_brute + p_runner + p_mutant + p_ghost + p_pathbreaker + p_healer) return ENEMY_HEALER;
    if (r < p_vehicle + p_brute + p_runner + p_mutant + p_ghost + p_pathbreaker + p_healer + p_hunter) return ENEMY_HUNTER;
    if (r < p_vehicle + p_brute + p_runner + p_mutant + p_ghost + p_pathbreaker + p_healer + p_hunter + p_artillery) return ENEMY_ARTILLERY;
    
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
    if (paths->count == 0) return 0;

    // Ne considérer que les chemins avec un A* valide ET une base active
    {
        int valid = 0;
        int first = -1;
        for (int p = 0; p < paths->count; p++) {
            if (path_usable(p, paths, map)) { valid++; first = p; }
        }
        if (valid == 0) return 0;
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
    int use_smart = (GetRandomValue(0, 9) < 6);

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
    if (nvalid == 0) return 0;
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
                 const Theme *theme, float dt)
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

            int count = BASE_ENEMIES + (wm->number - 1) * 3;
            if (wm->total_spawned < count) {
                // Choix intelligent du chemin
                int path_id = pick_path(wm, paths, map);
                float delay = wm->total_spawned * SPAWN_INTERVAL;

                EnemyType type = pick_enemy_type(wm->number, theme->id);
                enemy_spawn(pool, type, path_id, paths,
                            delay, wm->scale, theme->enemy_speed_mult);

                wm->total_spawned++;
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
            wm->scale        *= 1.20f;
            if (wm->scale > 6.0f) wm->scale = 6.0f;   // cap — évite la falaise de difficulté
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
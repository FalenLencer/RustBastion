#include "wave.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define PREP_TIME      20.0f   // secondes de préparation entre vagues
#define BASE_ENEMIES   6       // ennemis à la vague 1
#define SPAWN_INTERVAL 0.5f    // secondes entre chaque spawn

void wave_init(WaveManager *wm) {
    memset(wm, 0, sizeof(WaveManager));
    wm->state      = WAVE_IDLE;
    wm->number     = 0;
    wm->prep_timer = PREP_TIME;
    wm->scale      = 1.0f;
}

// Choisit le type d'ennemi selon la vague et le thème
static EnemyType pick_enemy_type(int wave_num, ThemeID theme) {
    float r = (float)GetRandomValue(0, 100) / 100.0f;

    // Probabilités évoluent avec le numéro de vague
    float p_vehicle = fminf(0.05f * (wave_num / 3.0f), 0.15f);
    float p_brute   = fminf(0.10f + wave_num * 0.02f,  0.30f);
    float p_runner  = fminf(0.10f + wave_num * 0.02f,  0.25f);
    float p_mutant  = 0.15f;

    // Modificateurs selon le thème
    if (theme == THEME_SWAMP)   { p_mutant += 0.15f; p_runner -= 0.05f; }
    if (theme == THEME_DESERT)  { p_runner += 0.15f; p_mutant -= 0.05f; }
    if (theme == THEME_FACTORY) { p_brute  += 0.10f; p_vehicle+= 0.05f; }
    if (theme == THEME_CITY)    { p_vehicle+= 0.05f; p_runner += 0.05f; }

    if (r < p_vehicle)                          return ENEMY_VEHICLE;
    if (r < p_vehicle + p_brute)                return ENEMY_BRUTE;
    if (r < p_vehicle + p_brute + p_runner)     return ENEMY_RUNNER;
    if (r < p_vehicle + p_brute + p_runner
              + p_mutant)                       return ENEMY_MUTANT;
    return ENEMY_RAIDER;
}

void wave_update(WaveManager *wm, EnemyPool *pool,
                 const PathSet *paths, const Theme *theme,
                 float dt, int *lives, int *gold, int *kills)
{
    // Mise à jour des ennemis dans tous les cas
    // ← CORRECTION bug #3 : passage du pointeur kills
    enemy_pool_update(pool, paths, dt, lives, gold, kills);

    switch (wm->state) {

        case WAVE_IDLE:
            wm->prep_timer -= dt;
            if (wm->prep_timer <= 0.0f)
                wave_start(wm);
            break;

        case WAVE_SPAWNING: {
            // Nombre d'ennemis à spawner cette vague
            int count = BASE_ENEMIES + (wm->number - 1) * 3;
            if (wm->total_spawned < count) {
                // Répartit sur les chemins disponibles en round-robin
                int path_id = wm->total_spawned % paths->count;
                float delay = wm->total_spawned * SPAWN_INTERVAL;

                // ← CORRECTION bug #1 : theme->id au lieu de theme->name[0]
                EnemyType type = pick_enemy_type(wm->number, theme->id);

                // ← CORRECTION bug #2 : wave_scale (HP) et speed_mult (vitesse) séparés
                enemy_spawn(pool, type, path_id, paths,
                            delay,
                            wm->scale,              // wave_scale → HP uniquement
                            theme->enemy_speed_mult); // speed_mult → vitesse uniquement

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
            // Prépare la vague suivante
            wm->state         = WAVE_IDLE;
            wm->prep_timer    = PREP_TIME;
            wm->total_spawned = 0;
            wm->scale        *= 1.15f;  // +15% HP par vague
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
}
#include "enemy.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

// ════════════════════════════════════════════════════
// STATS DE BASE PAR TYPE
// ════════════════════════════════════════════════════
const EnemyStats ENEMY_BASE_STATS[ENEMY_TYPE_COUNT] = {
    [ENEMY_RAIDER]  = {.hp=60,   .speed=2.0f, .size=6,  .reward=10, .damage=1, .name="Raider"},
    [ENEMY_BRUTE]   = {.hp=200,  .speed=0.9f, .size=9,  .reward=25, .damage=3, .name="Brute"},
    [ENEMY_RUNNER]  = {.hp=40,   .speed=3.5f, .size=5,  .reward=15, .damage=1, .name="Runner"},
    [ENEMY_VEHICLE] = {.hp=500,  .speed=0.6f, .size=12, .reward=60, .damage=5, .name="Blinde"},
    [ENEMY_MUTANT]  = {.hp=120,  .speed=1.2f, .size=8,  .reward=20, .damage=2, .name="Mutant"},
};

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void enemy_pool_init(EnemyPool *pool) {
    memset(pool, 0, sizeof(EnemyPool));
}

// ════════════════════════════════════════════════════
// SPAWN
// ════════════════════════════════════════════════════
void enemy_spawn(EnemyPool *pool, EnemyType type,
                 int path_id, const PathSet *paths,
                 float spawn_delay, float wave_scale,
                 float speed_mult)
{
    if (pool->count >= MAX_ENEMIES) return;
    if (path_id >= paths->count)   return;

    const Path       *path = &paths->paths[path_id];
    const EnemyStats *base = &ENEMY_BASE_STATS[type];

    // Cherche un slot libre
    Enemy *e = NULL;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!pool->enemies[i].active) { e = &pool->enemies[i]; break; }
    }
    if (!e) return;

    memset(e, 0, sizeof(Enemy));

    // Position de départ = spawn du chemin
    e->x = path->steps[0].x * TILE_SIZE + TILE_SIZE / 2.0f;
    e->y = path->steps[0].y * TILE_SIZE + TILE_SIZE / 2.0f;

    // wave_scale affecte les HP (difficulté croissante par vague)
    // speed_mult affecte la vitesse (modificateur du thème)
    e->type        = type;
    e->max_hp      = base->hp    * wave_scale;
    e->hp          = e->max_hp;
    e->speed       = base->speed * speed_mult;   // ← CORRECTION bug #2
    e->size        = base->size;
    e->reward      = base->reward;
    e->damage      = base->damage;
    e->path_id     = path_id;
    e->path_index  = 0;
    e->spawn_delay = spawn_delay;
    e->active      = 1;
    e->dead        = 0;
    e->reached_base= 0;

    pool->count++;
}

// ════════════════════════════════════════════════════
// DÉGÂTS
// ════════════════════════════════════════════════════
void enemy_damage(Enemy *e, float dmg) {
    if (!e->active || e->dead) return;
    e->hp -= dmg;
    if (e->hp <= 0.0f) {
        e->hp   = 0.0f;
        e->dead = 1;
    }
}

// ════════════════════════════════════════════════════
// MISE À JOUR
// ════════════════════════════════════════════════════
void enemy_pool_update(EnemyPool *pool, const PathSet *paths,
                       float dt, int *lives, int *gold, int *kills)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &pool->enemies[i];
        if (!e->active) continue;

        // ── Délai de spawn ────────────────────────────────
        if (e->spawn_delay > 0.0f) {
            e->spawn_delay -= dt;
            continue;
        }

        // ── Mort ─────────────────────────────────────────
        if (e->dead) {
            *gold  += e->reward;
            *kills += 1;          // ← CORRECTION bug #3 : comptage direct ici
            e->active = 0;
            pool->count--;
            continue;
        }

        // ── Atteint la base ───────────────────────────────
        if (e->reached_base) {
            *lives -= e->damage;
            if (*lives < 0) *lives = 0;
            e->active = 0;
            pool->count--;
            continue;
        }

        // ── Régénération (Mutant) ─────────────────────────
        if (e->type == ENEMY_MUTANT && e->hp < e->max_hp) {
            e->hp += 5.0f * dt;   // 5 HP/s
            if (e->hp > e->max_hp) e->hp = e->max_hp;
        }

        // ── Décrément ralentissement ──────────────────────
        if (e->slow_timer > 0.0f) e->slow_timer -= dt;

        // ── Déplacement sur le chemin A* ─────────────────
        const Path *path = &paths->paths[e->path_id];
        if (e->path_index >= path->len - 1) {
            e->reached_base = 1;
            continue;
        }

        // Vitesse effective (ralentissement éventuel)
        float speed = e->speed;
        if (e->slow_timer > 0.0f) speed *= 0.5f;

        // Cible : prochaine tuile du chemin
        Point next = path->steps[e->path_index + 1];
        float tx = next.x * TILE_SIZE + TILE_SIZE / 2.0f;
        float ty = next.y * TILE_SIZE + TILE_SIZE / 2.0f;

        float dx   = tx - e->x;
        float dy   = ty - e->y;
        float dist = sqrtf(dx*dx + dy*dy);
        float step = speed * TILE_SIZE * dt;

        if (dist <= step) {
            // Arrive sur la tuile suivante
            e->x = tx;
            e->y = ty;
            e->path_index++;
        } else {
            e->x += (dx / dist) * step;
            e->y += (dy / dist) * step;
        }
    }
}

// ════════════════════════════════════════════════════
// COMPTE LES ENNEMIS ENCORE EN VIE
// ════════════════════════════════════════════════════
int enemy_pool_alive(const EnemyPool *pool) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (pool->enemies[i].active) n++;
    return n;
}
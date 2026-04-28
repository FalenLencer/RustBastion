#include "tower.h"
#include "pathfinding.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "meta.h"

// ════════════════════════════════════════════════════
// STATS DE BASE
// ════════════════════════════════════════════════════
const TowerStats TOWER_BASE_STATS[TOWER_TYPE_COUNT] = {
    [TOWER_GUN] = {
        .name         = "Tourelle",
        .cost         = 15,
        .damage       = 20.0f,
        .range        = 3.5f,
        .fire_rate    = 1.5f,
        .proj_speed   = 10.0f,
        .splash       = 0,
        .slow_factor  = 0.0f,
        .slow_duration= 0.0f,
        .chain_count  = 0,
        .description  = "Polyvalente. Cible le premier ennemi en portee.",
    },
    [TOWER_SNIPER] = {
        .name         = "Sniper",
        .cost         = 25,
        .damage       = 90.0f,
        .range        = 6.5f,
        .fire_rate    = 0.5f,
        .proj_speed   = 18.0f,
        .splash       = 0,
        .slow_factor  = 0.0f,
        .slow_duration= 0.0f,
        .chain_count  = 0,
        .description  = "Longue portee. Cible l'ennemi le plus avance.",
    },
    [TOWER_FLAME] = {
        .name         = "Lance-flammes",
        .cost         = 30,
        .damage       = 12.0f,
        .range        = 2.5f,
        .fire_rate    = 3.0f,
        .proj_speed   = 5.0f,
        .splash       = 1,
        .slow_factor  = 0.5f,
        .slow_duration= 1.5f,
        .chain_count  = 0,
        .description  = "Zone courte. Ralentit et brule les ennemis.",
    },
    [TOWER_TESLA] = {
        .name         = "Tesla",
        .cost         = 40,
        .damage       = 45.0f,
        .range        = 4.0f,
        .fire_rate    = 0.8f,
        .proj_speed   = 20.0f,
        .splash       = 1,
        .slow_factor  = 0.0f,
        .slow_duration= 0.0f,
        .chain_count  = 3,
        .description  = "Chaine sur 3 ennemis proches. Ignore l'armure.",
    },
};

// ════════════════════════════════════════════════════
// UTILITAIRES
// ════════════════════════════════════════════════════
static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax-bx, dy = ay-by;
    return dx*dx + dy*dy;
}

static float px_of(int tile) { return tile * TILE_SIZE + TILE_SIZE * 0.5f; }

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void tower_pool_init(TowerPool *tp) {
    memset(tp, 0, sizeof(TowerPool));
}

// ════════════════════════════════════════════════════
// VÉRIFICATION DE PLACEMENT
// ════════════════════════════════════════════════════
int tower_can_place(const TowerPool *tp, const Map *map,
                    int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;

    const Tile *t = &map->tiles[ty][tx];
    if (!t->buildable) return 0;   // eau, chemin, spawn, base

    // Vérifie qu'aucune tour n'est déjà là
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (tp->towers[i].active &&
            tp->towers[i].tile_x == tx &&
            tp->towers[i].tile_y == ty) return 0;
    }
    return 1;
}

// ════════════════════════════════════════════════════
// PLACEMENT D'UNE TOUR
// ════════════════════════════════════════════════════
int  tower_place(TowerPool *tp, TowerType type,
                 int tile_x, int tile_y, Map *map,
                 int *gold, const MetaBonuses *bonuses)
{
    if (!tower_can_place(tp, map, tile_x, tile_y)) return 0;

    const TowerStats *st = &TOWER_BASE_STATS[type];
    if (*gold < st->cost) return 0;   // pas assez d'or

    // Trouve un slot libre
    Tower *tw = NULL;
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!tp->towers[i].active) { tw = &tp->towers[i]; break; }
    }
    if (!tw) return 0;

    *gold -= st->cost;

    tw->type       = type;
    tw->tile_x     = tile_x;
    tw->tile_y     = tile_y;
    tw->cx         = px_of(tile_x);
    tw->cy         = px_of(tile_y);
    tw->fire_timer = 0.0f;
    tw->level      = 0;
    tw->active     = 1;
    tw->angle      = 0.0f;

    tw->damage    = st->damage    * (bonuses ? bonuses->tower_dmg_mult  : 1.0f);
    tw->range     = st->range     * (bonuses ? bonuses->tower_range_mult: 1.0f);
    tw->fire_rate = st->fire_rate * (bonuses ? bonuses->tower_rate_mult : 1.0f);

    // La tuile n'est plus constructible
    map->tiles[tile_y][tile_x].buildable = 0;

    tp->tower_count++;
    return 1;
}

// ════════════════════════════════════════════════════
// CIBLAGE
// ════════════════════════════════════════════════════

// Retourne l'index de l'ennemi cible selon la stratégie de la tour
// -1 si aucun ennemi en portée
static int find_target(const Tower *tw, const EnemyPool *ep)
{
    const TowerStats *st  = &TOWER_BASE_STATS[tw->type];
    float             rng = st->range * TILE_SIZE;
    float             rng2= rng * rng;

    int   best_idx   = -1;
    float best_score = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;

        float d2 = dist2(tw->cx, tw->cy, e->x, e->y);
        if (d2 > rng2) continue;

        float score;
        if (tw->type == TOWER_SNIPER) {
            // Cible le plus avancé sur son chemin
            score = (float)e->path_index;
        } else {
            // Cible le plus proche
            score = -d2;
        }

        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
    }
    return best_idx;
}

// ════════════════════════════════════════════════════
// SPAWN D'UN PROJECTILE
// ════════════════════════════════════════════════════
static void spawn_projectile(TowerPool *tp, const Tower *tw,
                              int target_idx, const EnemyPool *ep)
{
    const TowerStats *st = &TOWER_BASE_STATS[tw->type];

    Projectile *p = NULL;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!tp->projectiles[i].active) {
            p = &tp->projectiles[i]; break;
        }
    }
    if (!p) return;

    const Enemy *e = &ep->enemies[target_idx];

    p->x            = tw->cx;
    p->y            = tw->cy;
    p->tx           = e->x;
    p->ty           = e->y;
    p->target_idx   = target_idx;
    p->damage       = st->damage;
    p->speed        = st->proj_speed * TILE_SIZE;
    p->splash       = st->splash;
    p->splash_radius= st->splash ? TILE_SIZE * 1.5f : 0.0f;
    p->slow_duration= st->slow_duration;
    p->chain_left   = st->chain_count;
    p->origin       = tw->type;
    p->active       = 1;
}

// ════════════════════════════════════════════════════
// MISE À JOUR DES PROJECTILES
// ════════════════════════════════════════════════════
static void update_projectiles(TowerPool *tp, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &tp->projectiles[i];
        if (!p->active) continue;

        // Suit la cible en mouvement
        const Enemy *tgt = &ep->enemies[p->target_idx];
        if (tgt->active && !tgt->dead) {
            p->tx = tgt->x;
            p->ty = tgt->y;
        }

        float dx   = p->tx - p->x;
        float dy   = p->ty - p->y;
        float dist = sqrtf(dx*dx + dy*dy);
        float step = p->speed * dt;

        if (dist <= step) {
            // Impact
            if (p->splash) {
                // Dégâts de zone
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    Enemy *e = &ep->enemies[j];
                    if (!e->active || e->dead) continue;
                    float d2 = dist2(p->tx, p->ty, e->x, e->y);
                    if (d2 <= p->splash_radius * p->splash_radius) {
                        enemy_damage(e, p->damage);
                        if (p->slow_duration > 0.0f)
                            e->slow_timer = p->slow_duration;
                    }
                }
            } else {
                // Dégâts directs
                Enemy *e = &ep->enemies[p->target_idx];
                enemy_damage(e, p->damage);
                if (p->slow_duration > 0.0f)
                    e->slow_timer = p->slow_duration;
            }

            // Rebond Tesla
            if (p->chain_left > 0) {
                float chain_rng2 = (TILE_SIZE * 2.5f) * (TILE_SIZE * 2.5f);
                int   chained    = 0;
                for (int j = 0; j < MAX_ENEMIES && !chained; j++) {
                    Enemy *e = &ep->enemies[j];
                    if (!e->active || e->dead || j == p->target_idx) continue;
                    if (dist2(p->tx, p->ty, e->x, e->y) <= chain_rng2) {
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

// ════════════════════════════════════════════════════
// MISE À JOUR GLOBALE
// ════════════════════════════════════════════════════
void tower_pool_update(TowerPool *tp, EnemyPool *ep, float dt)
{
    // Mise à jour des tours
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *tw = &tp->towers[i];
        if (!tw->active) continue;

        tw->fire_timer -= dt;
        if (tw->fire_timer > 0.0f) continue;

        int target = find_target(tw, ep);
        if (target == -1) continue;

        // Angle visuel vers la cible
        const Enemy *e = &ep->enemies[target];
        tw->angle = atan2f(e->y - tw->cy, e->x - tw->cx);

        spawn_projectile(tp, tw, target, ep);
        tw->fire_timer = 1.0f / TOWER_BASE_STATS[tw->type].fire_rate;
    }

    // Mise à jour des projectiles
    update_projectiles(tp, ep, dt);
}
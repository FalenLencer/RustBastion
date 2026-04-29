#include "unit.h"
#include <string.h>
#include <math.h>
#include <float.h>

// ════════════════════════════════════════════════════
// STATS DE BASE
// ════════════════════════════════════════════════════
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
};

// ════════════════════════════════════════════════════
// UTILITAIRES
// ════════════════════════════════════════════════════
static float udist(float ax,float ay,float bx,float by){
    float dx=ax-bx,dy=ay-by; return sqrtf(dx*dx+dy*dy);
}

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void unit_pool_init(UnitPool *up, float base_px, float base_py) {
    memset(up, 0, sizeof(UnitPool));
    up->base_px = base_px;
    up->base_py = base_py;
    for (int i = 0; i < MAX_UNITS; i++) up->units[i].target_idx = -1;
}

// ════════════════════════════════════════════════════
// SPAWN
// ════════════════════════════════════════════════════
int unit_spawn(UnitPool *up, UnitType type, int *gold, const MetaBonuses *bonuses) {
    if (up->count >= MAX_UNITS) return 0;

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

    // ── Applique les bonus méta ───────────────────────────
    u->max_hp          = st->hp     * (bonuses ? bonuses->unit_hp_mult  : 1.0f);
    u->hp              = u->max_hp;
    u->damage          = st->damage * (bonuses ? bonuses->unit_dmg_mult : 1.0f);
    // ─────────────────────────────────────────────────────

    u->speed           = st->speed;
    u->atk_range       = st->atk_range;
    u->atk_rate        = st->atk_rate;
    u->intercept_range = st->intercept_range;
    u->size            = st->size;
    u->target_idx      = -1;
    u->slot            = slot;
    u->active          = 1;

    // Rayon de patrouille — limité pour rester dans la carte
    // On calcule la distance max possible depuis la base sans sortir
    float max_radius_x = fminf(up->base_px, MAP_W * TILE_SIZE - up->base_px);
    float max_radius_y = fminf(up->base_py, MAP_H * TILE_SIZE - up->base_py);
    float max_radius   = fminf(max_radius_x, max_radius_y) - TILE_SIZE;

    float wanted_radius = (2.0f + (float)(slot % 3)) * TILE_SIZE;
    u->patrol_radius = fminf(wanted_radius, max_radius);
    u->patrol_angle  = ((float)slot / (float)MAX_UNITS) * 2.0f * PI;

    // Spawn sur le point de patrouille — déjà dans les limites
    u->x = up->base_px + cosf(u->patrol_angle) * u->patrol_radius;
    u->y = up->base_py + sinf(u->patrol_angle) * u->patrol_radius;
    up->count++;
    return 1;
}

// ════════════════════════════════════════════════════
// DÉGÂTS
// ════════════════════════════════════════════════════
void unit_damage(Unit *u, float dmg) {
    if (!u->active) return;
    u->hp -= dmg;
    if (u->hp <= 0.0f) { u->hp = 0.0f; u->active = 0; }
}

// ════════════════════════════════════════════════════
// MISE À JOUR
// ════════════════════════════════════════════════════

// Cherche l'ennemi prioritaire pour cette unité
static int find_enemy_target(const Unit *u, const EnemyPool *ep,
                              float base_px, float base_py)
{
    float intercept_px = u->intercept_range * TILE_SIZE;
    int   best         = -1;
    float best_score   = -FLT_MAX;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &ep->enemies[i];
        if (!e->active || e->dead || e->spawn_delay > 0.0f) continue;

        float d_base = udist(e->x, e->y, base_px, base_py);
        float d_unit = udist(e->x, e->y, u->x,    u->y);

        // Intercepte si l'ennemi est dans la zone ou déjà en combat
        if (d_base > intercept_px && d_unit > u->atk_range * TILE_SIZE * 3.0f)
            continue;

        // Score : privilégie les ennemis proches de la base
        float score = -d_base;
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}

// Cherche un allié blessé à soigner (pour UNIT_MEDIC)
static int find_heal_target(const Unit *medic, const UnitPool *up) {
    float heal_range = 3.0f * TILE_SIZE;
    int   best       = -1;
    float worst_hp   = FLT_MAX;

    for (int i = 0; i < MAX_UNITS; i++) {
        const Unit *a = &up->units[i];
        if (!a->active || a == medic) continue;
        if (a->hp >= a->max_hp)      continue;
        float d = udist(medic->x, medic->y, a->x, a->y);
        if (d > heal_range)          continue;
        if (a->hp < worst_hp) { worst_hp = a->hp; best = i; }
    }
    return best;
}

void unit_pool_update(UnitPool *up, EnemyPool *ep, float dt) {
    for (int i = 0; i < MAX_UNITS; i++) {
        Unit *u = &up->units[i];
        if (!u->active) continue;

        u->atk_timer  -= dt;
        u->heal_timer -= dt;

        // ── MÉDIC : soigne en priorité ────────────────────────
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

        // ── Cherche cible ennemie ─────────────────────────────
        int tgt = find_enemy_target(u, ep, up->base_px, up->base_py);
        u->target_idx = tgt;

        if (tgt != -1) {
            Enemy *e    = &ep->enemies[tgt];
            float  dist = udist(u->x, u->y, e->x, e->y);
            float  atk  = u->atk_range * TILE_SIZE;

            if (dist <= atk) {
                // ── ATTAQUE ───────────────────────────────────
                u->state = USTATE_ATTACK;
                if (u->atk_timer <= 0.0f) {
                    enemy_damage(e, u->damage);
                    u->atk_timer = 1.0f / u->atk_rate;
                    // L'ennemi riposte (dégâts simples)
                    unit_damage(u, 5.0f);
                }
            } else {
                // ── POURSUITE ─────────────────────────────────
                u->state = USTATE_CHASE;
                float dx = e->x - u->x, dy = e->y - u->y;
                float step = u->speed * TILE_SIZE * dt;
                u->x += (dx / dist) * step;
                u->y += (dy / dist) * step;
            }
        } else {
            // ── PATROUILLE autour de la base ──────────────────
            u->state = USTATE_PATROL;

            // Avance l'angle de patrouille
            u->patrol_angle += 0.5f * dt;   // rad/s

            float target_x = up->base_px + cosf(u->patrol_angle) * u->patrol_radius;
            float target_y = up->base_py + sinf(u->patrol_angle) * u->patrol_radius;
            float dist     = udist(u->x, u->y, target_x, target_y);

            if (dist > 2.0f) {
                float dx = target_x - u->x, dy = target_y - u->y;
                float step = u->speed * TILE_SIZE * 0.6f * dt;
                u->x += (dx / dist) * step;
                u->y += (dy / dist) * step;
            }
        }

        // ── Limite le déplacement (ne sort pas trop loin) ─────
        float max_dist = (u->intercept_range + 2.0f) * TILE_SIZE;
        float d_base   = udist(u->x, u->y, up->base_px, up->base_py);
        if (d_base > max_dist && u->target_idx == -1) {
            // Force le retour
            float dx   = up->base_px - u->x;
            float dy   = up->base_py - u->y;
            float step = u->speed * TILE_SIZE * dt;
            u->x += (dx / d_base) * step;
            u->y += (dy / d_base) * step;
            u->state = USTATE_RETURN;
        }
        // ── Clamp position dans les limites de la carte ───────────
        float margin = u->size + 2.0f;
        if (u->x < margin)                        u->x = margin;
        if (u->x > MAP_W * TILE_SIZE - margin)    u->x = MAP_W * TILE_SIZE - margin;
        if (u->y < margin)                        u->y = margin;
        if (u->y > MAP_H * TILE_SIZE - margin)    u->y = MAP_H * TILE_SIZE - margin;
    }
    
}
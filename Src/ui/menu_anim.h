/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim.h ─ Animation cinématique de l'écran titre.
 *
 *  Un ouvrier entre par la gauche, porte une tour sniper au-dessus de la
 *  tête, marche jusqu'à la cible, plante violemment la tour, puis célèbre.
 *  La scène boucle après une pause de 3 s.
 *
 *  Dépendances : Raylib 4.x, C99.
 *  L'OUVRIER, la POUSSIÈRE et les ÉTINCELLES sont 100 % PROCÉDURAUX
 *  (aucune sprite sheet) — seule la tour utilise un splash art (tex_tower).
 *  Tous les timers utilisent un accumulateur dt, jamais GetTime().
 */

#pragma once
#include "raylib.h"

// ════════════════════════════════════════════════════
// PHASES
// ════════════════════════════════════════════════════
typedef enum {
    ANIM_PHASE_WALK_IN   = 0, /* ouvrier entre par la gauche          (2.5 s) */
    ANIM_PHASE_CARRY,         /* ouvrier porte la tour, avance        (1.5 s) */
    ANIM_PHASE_SLAM,          /* plante la tour violemment            (0.3 s) */
    ANIM_PHASE_TOWER_POP,     /* scale pop + dust VFX                 (0.5 s) */
    ANIM_PHASE_VICTORY,       /* ouvrier lève les bras                (1.0 s) */
    ANIM_PHASE_PAUSE,         /* pause immobile avant reset           (3.0 s) */
    ANIM_PHASE_RESET,         /* état transitoire : remet tout à zéro        */
} AnimPhase;

// ════════════════════════════════════════════════════
// PARTICULE D'ÉTINCELLE (soudure) + PUFF DE POUSSIÈRE
// ════════════════════════════════════════════════════
#define WELD_PARTICLE_MAX 28
#define DUST_PARTICLE_MAX 22

typedef struct {
    float x, y;
    float vx, vy;
    float life;      /* vie restante (s)  */
    float max_life;  /* vie initiale (s)  */
    int   active;
} WeldParticle;

// Puff de poussière : disque ocre qui GONFLE, monte et s'estompe.
typedef struct {
    float x, y;
    float vx, vy;
    float r0;        /* rayon initial (px) */
    float life, max_life;
    int   active;
} DustParticle;

// ════════════════════════════════════════════════════
// ÉTAT COMPLET DE L'ANIMATION
// ════════════════════════════════════════════════════
typedef struct {

    /* ── Phase et timers ─────────────────────────── */
    AnimPhase phase;
    float     timer;          /* temps écoulé dans la phase (s)           */
    float     bg_timer;       /* timer indépendant VFX fond (arcs élec.)  */
    float     weld_spawn_t;   /* délai avant prochain spawn d'étincelle   */

    /* ── Ouvrier (procédural, pas de sprite sheet) ─── */
    float     worker_x;
    float     worker_y;
    float     walk_cycle;     /* phase du cycle de marche (0..1, boucle)  */

    /* ── Tour sniper ─────────────────────────────── */
    float     tower_scale;    /* 0.0 → 1.15 → 1.0                        */
    float     tower_x;        /* position X sol (pied de la tour)         */
    float     tower_y;        /* position Y sol                           */
    int       tower_planted;  /* 1 = tour au sol (après le claquement)    */

    /* ── Poussière (procédural) : gerbe émise au slam ── */
    DustParticle dust[DUST_PARTICLE_MAX];

    /* ── Étincelles de soudure (procédural) ────────── */
    WeldParticle weld[WELD_PARTICLE_MAX];

    /* ── Seule texture : la tour (splash art) ──────── */
    Texture2D tex_tower;  /* tower_sniper.png — splash art tour sniper    */

    int       loaded;     /* toujours 1 : l'anim ne dépend d'aucun asset  */

} MenuAnimState;

// ════════════════════════════════════════════════════
// INTERFACE PUBLIQUE
// ════════════════════════════════════════════════════

/* Charge les textures et remet l'état à zéro.
 * À appeler après InitWindow(), depuis menu_init(). */
void menu_anim_init   (MenuAnimState *a);

/* Libère les textures. À appeler depuis menu_cleanup(). */
void menu_anim_cleanup(MenuAnimState *a);

/* Avance la simulation d'un pas dt (secondes).
 * dt doit venir de GetFrameTime() côté appelant.
 * À appeler chaque frame quand l'écran titre est actif. */
void menu_anim_update (MenuAnimState *a, float dt);

/* Saute directement à l'état final (tour plantée + célébration) :
 * clic/touche pendant l'anim, ou retour au titre depuis le jeu —
 * la cinématique complète n'est jouée qu'au lancement du programme. */
void menu_anim_skip   (MenuAnimState *a);

/* Dessine l'animation dans le canvas virtuel (vw × vh).
 * À appeler dans draw_title(), après draw_bg(), avant les boutons UI. */
void menu_anim_render (const MenuAnimState *a, int vw, int vh);

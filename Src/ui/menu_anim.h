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
 *  Toutes les textures sont chargées dans MenuAnimState (pas de globaux).
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
// PARTICULE D'ÉTINCELLE (soudure)
// ════════════════════════════════════════════════════
#define WELD_PARTICLE_MAX 28

typedef struct {
    float x, y;
    float vx, vy;
    float life;      /* vie restante (s)  */
    float max_life;  /* vie initiale (s)  */
    int   active;
} WeldParticle;

// ════════════════════════════════════════════════════
// ÉTAT COMPLET DE L'ANIMATION
// ════════════════════════════════════════════════════
typedef struct {

    /* ── Phase et timers ─────────────────────────── */
    AnimPhase phase;
    float     timer;          /* temps écoulé dans la phase (s)           */
    float     bg_timer;       /* timer indépendant VFX fond (arcs élec.)  */
    float     weld_spawn_t;   /* délai avant prochain spawn d'étincelle   */

    /* ── Ouvrier ─────────────────────────────────── */
    float     worker_x;
    float     worker_y;
    int       worker_frame;   /* index linéaire dans le spritesheet       */
    float     worker_frame_t; /* accumulateur timer frame                 */
    int       worker_flip;    /* 1 = miroir horizontal (DrawTexturePro)   */

    /* ── Tour sniper ─────────────────────────────── */
    float     tower_scale;    /* 0.0 → 1.15 → 1.0                        */
    float     tower_x;        /* position X sol (pied de la tour)         */
    float     tower_y;        /* position Y sol                           */
    int       tower_planted;  /* 1 = tour au sol (après le claquement)    */

    /* ── Dust VFX ────────────────────────────────── */
    int       dust_frame;
    float     dust_frame_t;
    int       dust_active;
    float     dust_x, dust_y;

    /* ── Étincelles de soudure ───────────────────── */
    WeldParticle weld[WELD_PARTICLE_MAX];

    /* ── Textures chargées ───────────────────────── */
    Texture2D tex_worker; /* Anime_worker.png  — spritesheet ouvrier      */
    Texture2D tex_dust;   /* dust_anim.png     — explosion de poussière   */
    Texture2D tex_weld;   /* welding_anime.png — étincelles de soudure    */
    Texture2D tex_tower;  /* tower_sniper.png  — splash art tour sniper   */

    int       loaded;     /* 0 = assets manquants, skip render/update     */

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

/* Dessine l'animation dans le canvas virtuel (vw × vh).
 * À appeler dans draw_title(), après draw_bg(), avant les boutons UI. */
void menu_anim_render (const MenuAnimState *a, int vw, int vh);

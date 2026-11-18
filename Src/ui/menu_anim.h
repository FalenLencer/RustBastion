/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_anim.h ─ Animation cinématique de l'écran titre.
 *
 *  Un ouvrier (pixel art articulé, menu_anim_worker) entre par la gauche,
 *  la caisse-tour sur l'épaule, prend son élan et la LANCE : la caisse
 *  tournoie en cloche, s'écrase au sol et se déploie en tour (poussière,
 *  étincelles, onde de choc). L'ouvrier se protège, s'époussette les mains
 *  puis se tient fièrement, poings sur les hanches. La scène boucle.
 *
 *  Dépendances : Raylib, C99. Seule la tour déployée utilise un splash art
 *  (tex_tower) ; tout le reste est procédural. Les timers utilisent un
 *  accumulateur dt, jamais GetTime().
 */

#pragma once
#include "raylib.h"
#include "menu_anim_worker.h"

// ════════════════════════════════════════════════════
// PHASES
// ════════════════════════════════════════════════════
typedef enum {
    ANIM_PHASE_WALK_IN = 0,   /* entre, caisse sur l'épaule                 */
    ANIM_PHASE_WINDUP,        /* élan : caisse ramenée derrière la tête     */
    ANIM_PHASE_THROW,         /* lancer + vol de la caisse                  */
    ANIM_PHASE_TOWER_POP,     /* impact : la caisse se déploie en tour      */
    ANIM_PHASE_VICTORY,       /* s'époussette les mains, poings aux hanches */
    ANIM_PHASE_PAUSE,         /* attente avant de boucler                   */
    ANIM_PHASE_RESET,         /* état transitoire : remet tout à zéro       */
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
    float     bg_timer;       /* timer indépendant (arcs, respiration)    */
    float     weld_spawn_t;   /* délai avant prochain spawn d'étincelle   */

    /* ── Ouvrier (pixel art articulé) ─────────────── */
    float      worker_x;      /* point au sol (px virtuels)               */
    float      walk_cycle;    /* cycle de marche (calé sur la distance)   */
    WorkerPose pose;          /* pose courante                            */
    WorkerPose pose_from;     /* pose au changement de phase (fondu)      */

    /* ── Caisse-tour lancée ───────────────────────── */
    int       crate_air;      /* 0 en main, 1 en vol, 2 posée (impact)    */
    float     crate_x, crate_y, crate_rot;
    float     rel_x, rel_y, rel_rot;   /* point et angle du lâcher        */

    /* ── Tour déployée ────────────────────────────── */
    float     tower_scale;    /* 0.15 → 1.2 → 1.0 (déploiement)           */
    float     tower_x;        /* position X sol (pied de la tour)         */
    float     tower_y;        /* position Y sol                           */
    int       tower_planted;  /* 1 = tour déployée                        */

    /* ── Poussière (procédural) : gerbe émise à l'impact ── */
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

/* Saute directement à l'état final (tour déployée, ouvrier fier) :
 * clic/touche pendant l'anim, ou retour au titre depuis le jeu —
 * la cinématique complète n'est jouée qu'au lancement du programme. */
void menu_anim_skip   (MenuAnimState *a);

/* 1 tant que la cinématique d'entrée se joue (marche → déploiement). */
int  menu_anim_in_intro(const MenuAnimState *a);

/* Dessine l'animation dans le canvas virtuel (vw × vh).
 * À appeler dans draw_title(), après draw_bg(), avant les boutons UI. */
void menu_anim_render (const MenuAnimState *a, int vw, int vh);

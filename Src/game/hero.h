/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */
#pragma once
/* ════════════════════════════════════════════════════════════════
   game/hero.h — MODE HÉROS : TD incarné, 100 % 3D (bêta).

   Le héros évolue DANS la partie : la sim (game_state_update) tourne
   inchangée ; ce module gère l'avatar (déplacement, caméra 1re/3e
   personne, arme roguelite, interactions) et orchestre la frame du
   mode (input + update + rendu via render3d_world).

   Position du héros en COORDONNÉES SIM (pixels carte, comme les
   unités) → toutes les distances/zones réutilisent TILE_SIZE.
   ════════════════════════════════════════════════════════════════ */
#include "raylib.h"

/* ── Déplacement / caméra (constantes à régler au playtest) ────── */
#define HERO_SPEED_TILES     4.6f    /* vitesse de course (tuiles/s)          */
#define HERO_SPRINT_MULT     1.55f   /* multiplicateur sprint (Shift)         */
#define HERO_JUMP_V          5.4f    /* vitesse verticale du saut (monde/s)   */
#define HERO_GRAVITY        14.0f    /* gravité (monde/s²)                    */
#define HERO_STEP_UP         0.30f   /* marche franchissable sans sauter      */
#define HERO_RADIUS_PX       9.0f    /* rayon de collision (px sim)           */
#define HERO_MOUSE_SENS      0.0026f /* rad/pixel à sensibilité 50 (=1.0×)    */
#define HERO_LOOK_CLAMP      340.0f  /* delta souris max/frame (anti-spike ;
                                        assez large pour un flick rapide)    */
#define HERO_KEY_TURN        2.2f    /* rotation clavier (fleches, rad/s)     */
#define HERO_PITCH_MAX       1.50f   /* inclinaison verticale max (~86°,
                                        norme FPS ; < 90° = pas de gimbal)   */
#define HERO_EYE_H           1.16f   /* hauteur des yeux, 1re pers. (monde)   */
#define HERO_HEAD_H          1.05f   /* point visé par la caméra 3e pers.     */
#define HERO_CAM_DIST        4.6f    /* recul caméra 3e personne (monde)      */
#define HERO_CAM_FOVY        58.0f   /* champ de vision vertical (degrés)     */

/* ── Zones d'interaction (tuiles) ──────────────────────────────── */
#define HERO_BASE_ZONE_TILES   2.6f  /* rayon « près d'une base »             */
#define HERO_WORKER_ZONE_TILES 3.2f  /* rayon « près d'un ouvrier »           */
#define HERO_PLACE_DIST_TILES  4.5f  /* portée max du placement de tour       */
#define HERO_TOWER_ZONE_TILES  1.8f  /* rayon « près d'une tour » (contrôle)  */

/* ── Contrôle manuel d'une tour ────────────────────────────────── */
#define HERO_CTRL_EYE_H        2.35f /* hauteur des yeux sur la tourelle      */
#define HERO_CTRL_RANGE_MULT   1.15f /* bonus de portée en visée manuelle     */

/* ── Survie du héros (fini l'invulnérabilité) ──────────────────── */
#define HERO_MAX_HP           100.0f /* points de vie                         */
#define HERO_CONTACT_RANGE_PX  10.0f /* marge de contact (en plus des tailles)*/
#define HERO_CONTACT_BASE_DPS   4.0f /* PV/s de base par ennemi au contact    */
#define HERO_CONTACT_DMG_DPS    3.0f /* PV/s supplémentaires × e->damage      */
#define HERO_REGEN_BASE_PS      9.0f /* régénération PV/s dans la zone de base*/
#define HERO_KO_TIME            6.0f /* durée du K.O. avant respawn (s)       */
#define HERO_RESPAWN_HP_FRAC    0.7f /* PV au respawn (fraction du max)       */
#define HERO_HURT_FLASH_T       0.3f /* flash rouge à l'impact (s)            */

/* ── Rôle de l'arme : FINISHER (là où les tours sont aveugles) ─── */
#define HERO_EXECUTE_THRESH     0.30f /* seuil PV « achevable » (fraction)    */
#define HERO_EXECUTE_MULT       1.40f /* bonus de dégâts sous le seuil        */

/* ── Vue tactique (drone au-dessus du héros, touche C) ─────────── */
#define HERO_TACT_H            30.0f  /* altitude de la caméra (monde)        */

/* ── Arme du héros (roguelite) ─────────────────────────────────── */
typedef enum { HW_RIFLE = 0, HW_CANNON, HW_TESLA, HW_COUNT } HeroWeapon;

#define HERO_UPG_MAX        8       /* paliers max par branche (dégâts/cadence) */
#define HERO_UPG_DMG_STEP   0.15f   /* +15 % dégâts par palier                  */
#define HERO_UPG_RATE_STEP  0.12f   /* +12 % cadence par palier                 */
#define HERO_UPG_COST_BASE  30      /* coût = BASE × (palier + 1) (or)          */

#define HERO_TRACE_TIME     0.07f   /* durée traceur + flash d'impact (s)       */

/* ── Feel caméra (1re personne) ────────────────────────────────── */
#define HERO_FOV_SPRINT     66.0f   /* FOV élargi en sprint (base = CAM_FOVY)   */
#define HERO_FOV_LERP        8.0f   /* vitesse d'interpolation du FOV (1/s)     */
#define HERO_BOB_FREQ        9.5f   /* fréquence du balancement de marche (rad/s)*/
#define HERO_BOB_AMP         0.035f /* amplitude verticale du balancement (monde)*/

/* ── HUD monde ─────────────────────────────────────────────────── */
#define HERO_MMAP_W          190    /* largeur de la minimap (px canvas)        */
#define HERO_HPBAR_TILES     18.0f  /* distance max d'affichage des barres PV   */
#define HERO_BANNER_TIME     2.2f   /* durée de la bannière de vague (s)        */

/* ── État persistant du mode ───────────────────────────────────── */
typedef struct {
    /* Avatar */
    float px, py;          /* position sim (pixels carte)                */
    float hz, vz;          /* hauteur (unités monde) + vitesse verticale */
    int   on_ground;       /* 1 = au sol (peut sauter)                   */
    float yaw, pitch;      /* orientation caméra (rad)                   */
    int   first_person;    /* 1 = vue 1re personne (V bascule)           */
    int   moving;          /* 1 = en déplacement (anime la marche)       */
    float anim_t;          /* horloge d'animation du modèle héros        */

    /* Arme */
    HeroWeapon weapon;
    int   upg_dmg, upg_rate;
    float fire_cd;         /* temps avant le prochain tir (s)            */
    float fire_flash;      /* >0 : vient de tirer (anim + feedback HUD)  */

    /* Traceur du dernier tir (monde 3D) */
    Vector3 trace_a, trace_b;
    float   trace_t;
    int     trace_hit;     /* 1 = le tir a touché (flash d'impact à trace_b) */
    int     aim_on_target; /* 1 = un ennemi est sous le réticule (viseur réactif) */

    /* Survie */
    float hp, hp_max;      /* points de vie du héros                     */
    float hurt_t;          /* >0 : vient d'encaisser (flash rouge)       */
    float ko_t;            /* >0 : K.O. en cours (respawn à 0)           */
    int   down;            /* 1 = à terre (aucune action possible)       */

    /* Vue tactique (drone, touche C) */
    int   tactical_view;

    /* Sélection de matériau à appliquer ([N] cycle, [M] applique) */
    int   mat_sel;

    /* Contrôle manuel d'une tour ([E] près d'une tour) */
    int   control_tower;   /* indice de la tour pilotée (-1 = aucune)    */
    float ctrl_px, ctrl_py;/* position sim de la tour (caméra tourelle)  */

    /* Placement de tour (via un ouvrier) */
    int   place_mode;      /* 1 = fantôme de placement actif             */
    int   place_type;      /* TowerType en cours de sélection            */
    int   place_tx, place_ty;   /* tuile visée (recalculée par frame)    */
    int   place_ok;        /* 1 = pose valide (constructible + or)       */

    /* Souris : regard par RECENTRAGE MANUEL (chemin virt_mouse, fiable
       partout — DisableCursor/GetMouseDelta cassent sous WSLg/GLFW).   */
    int   mouse_free;      /* 1 = souris libérée (TAB) : pas de regard   */
    int   mouse_settle;    /* frames à ignorer après (re)capture         */
    int   mouse_native;    /* 1 = capture native (GetMouseDelta) ; 0 =
                              mode compatible (recentrage manuel)         */
    float look_ref_x;      /* mode compatible : position virtuelle EXACTE */
    float look_ref_y;      /*   du point de recentrage (auto-calibrée —   */
    int   look_ref_ok;     /*   élimine toute dérive de la vue au repos)  */

    /* Feel caméra */
    int   sprinting;       /* 1 = sprint en cours (élargit le FOV)       */
    float fov_cur;         /* FOV courant (interpolé vers base/sprint)   */
    float bob_t;           /* horloge du balancement de marche           */

    /* Menu radial (P1.4) : molette maintenue → geste directionnel.
       Fonctionne souris VERROUILLÉE (les deltas de regard deviennent le
       geste), donc compatible modes natif ET compatible WSLg. */
    int   radial_open;     /* 1 = menu radial affiché                    */
    float radial_dx;       /* vecteur du geste souris cumulé (px)        */
    float radial_dy;
    int   radial_sel;      /* segment visé (-1 = zone morte)             */

    /* Bannière de vague (VAGUE N / repoussée) */
    float wave_banner_t;
    char  wave_banner[32];
    int   prev_phase;      /* GamePhase du frame précédent (transitions) */

    /* Meta-frame — la pause utilise le MENU CLASSIQUE (ctx->menu.paused) */
    int   show_help;       /* panneau des commandes (H)                  */
    int   fs_forced;       /* 1 = plein écran imposé par le mode (à
                              restaurer en fenêtré à la sortie)          */
    float toast_t;         /* message temporaire bas d'écran             */
    char  toast[96];
    Color toast_col;
} HeroState;

/* ── API (appelée par app.c) ───────────────────────────────────── */
struct AppContext;

/* Prépare l'état héros après game_init_* : spawn près de la base
   primaire, capture le curseur, active le mode. */
void hero_start(struct AppContext *ctx);

/* Frame complète du mode héros (input + sim + rendu).
   Retourne 1 pour continuer, 0 pour quitter l'application. */
int  hero_frame(struct AppContext *ctx, float dt);

/* Caméra 3D courante du héros (partagée rendu / tir / placement). */
Camera3D hero_camera(const HeroState *h);

/* Affiche un message temporaire dans le HUD héros. */
void hero_toast(HeroState *h, const char *msg, Color col);

/* ── hero_hud.c : couche présentation du mode (interne) ────────── */
/* Popups FX (dégâts/or) + barres de PV ennemies projetés en 3D. */
void hero_draw_world_popups(struct AppContext *ctx, Camera3D cam);
/* HUD complet (viseur, hit-marker, minimap, stats, invites, aide…). */
void hero_hud(struct AppContext *ctx);
/* Voile + titre centré (pause/défaite) ; retourne le y du contenu. */
int  hero_overlay_panel(const char *title, Color col);
/* Arme en 1re personne (viewmodel + recul + flash) — à appeler DANS
   la scène 3D (BeginMode3D actif), en dernier. */
void hero_draw_viewmodel(const HeroState *h, Camera3D cam);

/* ── Menu radial (P1.4) : 3 armes + 4 tours à la molette ───────── */
#define HERO_RADIAL_N        7       /* segments : HW_COUNT + 4 tours    */
#define HERO_RADIAL_DEAD_PX  18.0f   /* zone morte du geste (px souris)  */
#define HERO_RADIAL_MAX_PX  140.0f   /* longueur max du vecteur de geste */
#define HERO_RADIAL_R       120      /* rayon d'affichage (px canvas)    */
/* Libellé du segment i ; cost = coût or (-1 si sans coût), locked = 1
   si indisponible dans le contexte courant (affichage grisé du HUD). */
const char *hero_radial_label(struct AppContext *ctx, int i,
                              int *cost, int *locked);
/* Applique le segment sélectionné (à la relâche de la molette). */
void hero_radial_apply(struct AppContext *ctx);

/* ── hero_actions.c : interactions & tir ───────────────────────── */
/* Gère tir, zones base/ouvrier, placement, achats, upgrades.       */
void hero_actions_update(struct AppContext *ctx, float dt);
/* Invites contextuelles pour le HUD (lignes de texte). Retourne n. */
int  hero_prompts(struct AppContext *ctx, char out[][64], int max);
/* 1 = héros dans la zone d'une base (hotbar de recrutement). */
int  hero_in_base_zone(struct AppContext *ctx);
/* 1 = type de tour débloqué (même règle que le HUD 2D). */
int  hero_tower_unlocked(struct AppContext *ctx, int tower_type);
/* Ennemi sous le réticule (rayon caméra). ray/dist optionnels (NULL). */
int  hero_aim_pick(struct AppContext *ctx, float range,
                   Ray *ray_out, float *dist_out);

/* ── hero_survival.c : PV, dégâts de contact, K.O./respawn ─────── */
/* Tuile praticable la plus proche de la base primaire (spawn/respawn). */
void hero_find_spawn(struct AppContext *ctx, float *out_px, float *out_py);
/* Contact ennemi → dégâts ; régén près d'une base ; passage à terre. */
void hero_survival_update(struct AppContext *ctx, float dt);
/* Frame complète pendant le K.O. (sim + rendu + compte à rebours). */
void hero_down_frame(struct AppContext *ctx, float dt);

/* ── hero_tower.c : contrôle manuel + améliorations de tours ───── */
/* Tour active la plus proche dans HERO_TOWER_ZONE_TILES (-1 = aucune). */
int  hero_tower_near(struct AppContext *ctx);
/* Mode CONTRÔLE : visée/tir/upgrades/sortie. Retourne 1 si actif
   (l'appelant ne traite alors rien d'autre). */
int  hero_tower_update(struct AppContext *ctx);
/* Hors contrôle, près d'une tour : [E] entre en contrôle, O/P/L
   améliorent. tower_idx = résultat de hero_tower_near. */
void hero_tower_interact_near(struct AppContext *ctx, int tower_idx);
/* Invites du HUD pour le contrôle / la proximité de tour. */
int  hero_tower_prompts(struct AppContext *ctx, char out[][64],
                        int max, int n);
/* Nom / stats de l'arme courante pour le HUD. */
const char *hero_weapon_name(HeroWeapon w);
float hero_weapon_dmg (const HeroState *h);
float hero_weapon_rate(const HeroState *h);

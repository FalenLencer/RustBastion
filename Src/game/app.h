/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
/* ════════════════════════════════════════════════════════════════
   game/app.h — contexte applicatif global
   Regroupe les états de haut niveau (écran courant, interlude,
   slot actif) et expose les fonctions de boucle principale.
   ════════════════════════════════════════════════════════════════ */
#include "game_state.h"
#include "../ui/menu.h"
#include "../ui/interlude.h"
#include "../net/net_session.h"

// ────────────────────────────────────────────────────────────────
// Écrans principaux
// ────────────────────────────────────────────────────────────────
typedef enum {
    SCREEN_MENU = 0,
    SCREEN_GAME,
} Screen;

// ────────────────────────────────────────────────────────────────
// Contexte global de l'application
// Instancié en static dans main() pour éviter un débordement de pile.
// ────────────────────────────────────────────────────────────────
typedef struct {
    GameState      gs;
    MenuState      menu;

    Screen         screen;
    InterludeState interlude;
    InterludeState interlude_prev;   /* interlude du frame précédent : évite que
                                        l'input traverse une transition (anti-bleed) */

    int            active_slot;

    /* Données temporaires des interludes campagne */
    int            interlude_scrap;
    int            interlude_last;
    int            interlude_stars;

    /* Suivi interne */
    MenuScreen     prev_menu_screen;
    int            applied_fps;
    int            gameover_meta_done;   /* garde : meta_endless_end déjà appelé */

    /* Bannière de carte : affichée 5 s au démarrage puis fondu */
    float          banner_timer;         /* > 0 = visible, fondu sur la dernière 0.5 s */

    /* ── Multijoueur ──────────────────────────────────────── */
    NetSession     session;
    int            mp_active;       /* 1 = session réseau vivante (lobby ou jeu) */
    int            mp_mode;         /* MpMode courant */
    int            mp_in_game;      /* 1 = partie MP en cours (pas juste lobby) */
    NetStatus      mp_peer;         /* dernier statut reçu de l'adversaire */
    int            mp_peer_valid;   /* 1 = au moins un statut reçu */
    int            mp_result;       /* 0 = en cours, 1 = gagné, 2 = perdu */
    float          mp_status_timer; /* throttle d'envoi du statut (s) */
    int            mp_sabotage;     /* Duel/Asym : monnaie d'envoi accumulée */
    int            mp_prev_kills;   /* Duel : kills au tick précédent (delta → sabotage) */
    int            mp_invader;      /* Asym : 1 = ce client est l'ENVAHISSEUR (sim figée) */
    float          mp_budget_t;     /* Asym envahisseur : accumulateur de budget (s) */
    int            mp_wave_compose[ENEMY_TYPE_COUNT]; /* Asym : vague en composition (nb/type) */
    int            mp_wave_budget;  /* Asym envahisseur : budget restant de la vague */
    int            mp_wave_num;     /* Asym envahisseur : nb de vagues envoyées */
    float          mp_wave_cooldown;/* Asym envahisseur : délai avant la prochaine vague (s) */
    float          mp_inject_acc;   /* défenseur : étalement des ennemis injectés (s) */
    int            mp_via_relay;    /* 1 = session courante via serveur relais */
    /* Asym — plateau partagé : snapshot du défenseur vu par l'envahisseur.
       Triplets (tile_x, tile_y, type) packés. */
    unsigned char  mp_board_t[64 * 3]; int mp_board_tn;  /* tours  */
    unsigned char  mp_board_u[32 * 3]; int mp_board_un;  /* unités */
    float          mp_board_timer;   /* défenseur : throttle d'envoi du plateau (s) */
    /* Positions déplaçables des panneaux flottants MP (coords virtuelles ;
       <0 = non initialisé → placé au défaut au 1er rendu). */
    Vector2        mp_pos_rival;     /* HUD rival (haut-centre)       */
    Vector2        mp_pos_duel;      /* barre d'envoi Duel            */
    int            mp_drag;          /* 0=aucun, 1=rival, 2=duel      */
    Vector2        mp_drag_grab;     /* décalage souris→coin au drag  */
    int            tactical_pause;   /* solo : gel pour réfléchir (ESPACE), on peut agir */
    int            tutorial_active;  /* 1 = tutoriel guidé en cours */
    int            tutorial_step;    /* étape courante du tutoriel */
    float          mp_asym_timer;    /* Asym pur : temps de survie écoulé (défenseur) */
    int            show_help;        /* 1 = panneau d'aide des commandes (touche H) */
} AppContext;

// ────────────────────────────────────────────────────────────────
// API publique
// ────────────────────────────────────────────────────────────────

/* Initialise tout le contexte (GameState, MenuState, options, etc.) */
void app_init        (AppContext *ctx);

/* Exécute une frame complète (update + render).
 * Retourne 1 pour continuer, 0 pour quitter la boucle. */
int  app_update      (AppContext *ctx, float dt);

/* Sauvegarde si la partie est en cours (à appeler avant fermeture). */
void app_save_on_exit(const AppContext *ctx);

/* Libère les ressources haut niveau (menu, options). */
void app_cleanup     (AppContext *ctx);

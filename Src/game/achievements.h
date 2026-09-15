/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
/* ════════════════════════════════════════════════════════════════
   game/achievements.h — Succès + méta-déblocages (P1.3)

   Persistance dans un fichier SÉPARÉ du méta (le méta est un fwrite
   de struct brut versionné : y ajouter un champ AURAIT WIPÉ la
   progression de tout le monde). Format tolérant : magic + version
   + count + flags — on pourra AJOUTER des succès sans casser.

   Chaque succès verse une récompense de ferraille UNE seule fois
   (méta-déblocage : alimente la boutique d'améliorations existante).
   ════════════════════════════════════════════════════════════════ */

struct GameState;

typedef enum {
    ACH_FIRST_BLOOD = 0,  /* 1er ennemi detruit                       */
    ACH_KILLS_100,        /* 100 ennemis en une partie                */
    ACH_WAVE_10,          /* atteindre la vague 10                    */
    ACH_WAVE_20,          /* atteindre la vague 20                    */
    ACH_TOWERS_8,         /* 8 tours actives en meme temps            */
    ACH_ALL_TOWERS,       /* les 4 types de tours dans une partie     */
    ACH_RICH,             /* 1000 or en caisse                        */
    ACH_FIRST_ACT,        /* campagne : 1er acte complete             */
    ACH_PERFECT_ACT,      /* campagne : un acte avec objectif bonus   */
    ACH_CAMPAIGN_DONE,    /* campagne complete                        */
    ACH_BOSS_DOWN,        /* un boss de chapitre vaincu               */
    ACH_HERO_MODE,        /* jouer une partie en mode Heros           */
    ACH_MULTI,            /* jouer une partie multijoueur             */
    ACH_ENDLESS_15,       /* endless : atteindre la vague 15          */
    ACH_COUNT
} AchID;

typedef struct {
    const char *name;    /* nom court (affiche menus + notif)         */
    const char *desc;    /* condition en clair (onglet SUCCES)        */
    int         scrap;   /* recompense de ferraille, versee UNE fois  */
} AchDef;

extern const AchDef ACH_DEFS[ACH_COUNT];

/* 1 si le succès `id` est déjà débloqué (chargement paresseux). */
int  ach_unlocked(int id);
/* Nombre total de succès débloqués (pour les compteurs de menus). */
int  ach_unlocked_count(void);
/* Débloque un succès : idempotent. Verse la ferraille dans gs->meta
   (+ meta_save), pousse une notification et persiste le fichier. */
void ach_unlock(int id, struct GameState *gs);
/* Vérifie toutes les conditions POLLABLES sur l'état de jeu courant.
   À appeler une fois par frame de simulation (fin de game_state_update).
   Les succès événementiels (héros, multi) passent par ach_unlock direct. */
void ach_poll_game(struct GameState *gs);

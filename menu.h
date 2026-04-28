#pragma once
#include "raylib.h"
#include "save.h"
#include "theme.h"
#include "meta.h"

// ════════════════════════════════════════════════════
// MACHINE À ÉTATS DU MENU
// ════════════════════════════════════════════════════
typedef enum {
    MENU_MAIN = 0,      // menu principal
    MENU_SLOTS,         // sélection/reprise de partie
    MENU_NEW_GAME,      // création d'une nouvelle partie (choix thème, slot)
    MENU_UPGRADES,      // onglet améliorations méta
    MENU_OPTIONS,       // options (résolution, plein écran)
    MENU_PAUSE,         // menu pause in-game
    MENU_CONFIRM_DEL,   // confirmation effacement d'un slot
} MenuScreen;

// ── Onglets du menu principal ─────────────────────────────────
typedef enum {
    TAB_PLAY = 0,       // jouer / parties
    TAB_UPGRADES,       // arsenal / améliorations
    TAB_OPTIONS,        // options
    TAB_COUNT
} MenuTab;

// ── Options ───────────────────────────────────────────────────
typedef struct {
    int fullscreen;     // 0 = fenêtré, 1 = plein écran
    int win_width;      // résolution fenêtrée
    int win_height;
} AppOptions;

// ── État complet du système de menus ─────────────────────────
typedef struct {
    MenuScreen  screen;
    MenuTab     active_tab;

    // Slots
    SaveInfo    slots[SAVE_SLOT_COUNT];
    int         hovered_slot;      // -1 = aucun
    int         selected_slot;     // slot visé pour new game / delete
    int         confirm_del_slot;  // slot à effacer (confirmation)

    // Création de partie
    ThemeID     new_theme;         // thème choisi (THEME_COUNT = aléatoire)
    int         new_slot;          // slot cible pour la nouvelle partie

    // Améliorations
    int         sel_upg;           // upgrade survolé/sélectionné

    // Options
    AppOptions  opts;

    // Pause
    int         paused;            // 1 si le jeu est en pause

    // Animation / feedback
    float       msg_timer;         // durée d'affichage d'un message temporaire
    char        msg_buf[128];      // texte du message
} MenuState;

// ── API ──────────────────────────────────────────────────────
void menu_init   (MenuState *m, const AppOptions *opts);
void menu_refresh_slots(MenuState *m);   // relit les fichiers de sauvegarde

// Retourne 1 si le menu consomme l'input (ne pas updater le jeu)
// action_* sont des signaux sortants vers main.c :
typedef struct {
    int start_new;      // 1 = lancer new_game avec m->new_theme, m->new_slot
    int resume_slot;    // >= 0 : charger ce slot et jouer
    int go_game;        // aller en SCREEN_GAME (après start ou resume)
    int quit_app;       // quitter l'application
    int save_and_quit;  // sauvegarder puis aller menu principal
    int toggle_fs;      // basculer plein écran
} MenuAction;

MenuAction menu_update(MenuState *m, const MetaProgress *meta);
void       menu_render(const MenuState *m, const MetaProgress *meta,
                       int virt_w, int virt_h);
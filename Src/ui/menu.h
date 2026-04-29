#pragma once
#include "raylib.h"
#include "../core/save.h"
#include "../map/theme.h"
#include "../meta/meta.h"

// ════════════════════════════════════════════════════
// ÉTATS DU MENU
// ════════════════════════════════════════════════════
typedef enum {
    MENU_TITLE       = 0, // écran titre : Jouer / Options / Quitter
    MENU_PLAY_HUB,        // hub : Campagne / Arcade / Améliorations / Retour
    MENU_CAMPAIGN,        // liste des slots de campagne
    MENU_ARCADE,          // liste des slots arcade
    MENU_NEW_CAMPAIGN,    // choisir slot pour nouvelle campagne
    MENU_NEW_ARCADE,      // choisir thème + slot pour arcade
    MENU_UPGRADES,        // arbre d'améliorations
    MENU_OPTIONS,         // options fenêtre/résolution
    MENU_CONFIRM_DEL,     // confirmation suppression slot
    MENU_PAUSE,           // overlay pause in-game
} MenuScreen;

typedef struct {
    int fullscreen;
    int win_width;
    int win_height;
} AppOptions;

typedef struct {
    MenuScreen  screen;
    MenuScreen  back_screen;   // écran cible du bouton Retour

    SaveInfo    slots[SAVE_SLOT_COUNT];
    int         confirm_del_slot;

    ThemeID     new_theme;     // THEME_COUNT = aléatoire
    int         new_slot;

    int         sel_upg;       // upgrade survolé/sélectionné

    AppOptions  opts;
    int         paused;

    float       msg_timer;
    char        msg_buf[128];
} MenuState;

typedef struct {
    int     start_arcade;   // 1 = lancer une arcade
    int     start_campaign; // 1 = lancer une campagne (stage 0)
    int     resume_slot;    // >= 0 : reprendre ce slot
    int     go_game;        // basculer vers SCREEN_GAME
    int     quit_app;
    int     save_and_quit;  // 1=sauver+menu, 2=sauver seulement
    int     toggle_fs;      // 1=fullscreen toggle, 2=resize
    ThemeID new_theme;
    int     new_slot;
} MenuAction;

void       menu_init             (MenuState *m, const AppOptions *opts);
void       menu_refresh_slots    (MenuState *m);
void       menu_set_mouse_offset (float ox, float oy, float scale);
MenuAction menu_update           (MenuState *m, const MetaProgress *meta);
MenuAction menu_render_and_act   (MenuState *m, const MetaProgress *meta,
                                  int virt_w, int virt_h);
void       menu_render           (const MenuState *m, const MetaProgress *meta,
                                  int virt_w, int virt_h);
/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once
#include "raylib.h"
#include "../game/save.h"
#include "../map/theme.h"
#include "../game/meta.h"

// ════════════════════════════════════════════════════
// ÉTATS DU MENU
// ════════════════════════════════════════════════════
typedef enum {
    MENU_TITLE       = 0,
    MENU_PLAY_HUB,
    MENU_CAMPAIGN,
    MENU_WORLD_MAP,
    MENU_ARCADE,
    MENU_NEW_CAMPAIGN,
    MENU_NEW_ARCADE,
    MENU_UPGRADES,
    MENU_OPTIONS,
    MENU_CONFIRM_DEL,
    MENU_PAUSE,
} MenuScreen;

typedef struct {
    int fullscreen;
    int win_width;
    int win_height;
    int target_fps;
    int master_volume;
    int music_volume;
    int sfx_volume;
} AppOptions;

typedef struct {
    MenuScreen  screen;
    MenuScreen  back_screen;
    MenuScreen  back_screen_stack[8];
    int         back_stack_top;

    SaveInfo    slots[SAVE_SLOT_COUNT];           // slots arcade
    SaveInfo    campaign_slots[SAVE_SLOT_COUNT];  // slots campagne (fichiers séparés)
    int         confirm_del_slot;

    ThemeID     new_theme;
    int         new_slot;
    int         campaign_order_seed;

    int         sel_upg;

    AppOptions  opts;
    int         paused;

    float       msg_timer;
    char        msg_buf[128];

    int         opt_tab;            // 0=General, 1=Audio, 2=Graphismes
    int         opt_dropdown_open;  // -1=none, 0=FPS, 1=Resolution, etc
} MenuState;

typedef struct {
    int     start_arcade;
    int     start_campaign;
    int     resume_slot;
    int     resume_is_campaign;   // 1 = le slot à reprendre est une save campagne
    int     go_game;
    int     quit_app;
    int     save_and_quit;
    int     toggle_fs;
    ThemeID new_theme;
    int     new_slot;
    int     campaign_order_seed;
} MenuAction;

// Persistance des options (config/settings.bin)
void       opts_save             (const AppOptions *o);
int        opts_load             (AppOptions *o);

void       menu_init             (MenuState *m, const AppOptions *opts);
void       menu_refresh_slots    (MenuState *m);           // rafraîchit les 2 tableaux
void       menu_set_mouse_offset (float ox, float oy, float sx, float sy);
void       menu_cleanup          (MenuState *m);
MenuAction menu_update           (MenuState *m, const MetaProgress *meta);
MenuAction menu_render_and_act   (MenuState *m, const MetaProgress *meta,
                                  int virt_w, int virt_h);
void       menu_render           (const MenuState *m, const MetaProgress *meta,
                                  int virt_w, int virt_h);
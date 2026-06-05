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
#include "menu_anim.h"

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
    MENU_CUSTOM,         // écran de configuration partie personnalisée
    MENU_UPGRADES,
    MENU_OPTIONS,
    MENU_CONFIRM_DEL,
    MENU_PAUSE,
    MENU_BESTIARY,
    MENU_MP_HUB,         // multijoueur : choix du mode + héberger/rejoindre
    MENU_MP_LOBBY,       // multijoueur : salon (code, ready-up, lancement)
} MenuScreen;

// ── Modes multijoueur (valeur = octet `mode` du protocole réseau) ──
typedef enum {
    MP_NONE = 0,
    MP_COURSE,           // course/score (1er mode implémenté)
    MP_DUEL,             // duel d'envoi de vagues
    MP_ASYM,             // bâtisseur vs envahisseur
    MP_COOP,             // co-op défense partagée
    MP_MODE_COUNT
} MpMode;

// Vue (lecture seule) de l'état de session réseau, remplie par app.c et lue
// par les écrans de lobby. POD : aucun type réseau ici.
typedef struct {
    int  active;          // 1 = une session existe
    int  state;           // SessState (entier) pour l'affichage
    int  is_host;
    int  peer_connected;  // 1 = pair présent
    int  my_ready, peer_ready;
    int  started;         // 1 = partie lancée
    int  failed;
    int  via_relay;       // 1 = session passe par un serveur relais
    char code[20];        // code de session (côté hôte)
    char peer_name[24];
} MpLobbyView;

// ── Configuration d'une partie personnalisée ──────────────────
typedef struct {
    int   theme;             // ThemeID (THEME_COUNT = aléatoire)
    int   min_dist;          // distance min spawn→base (4/6/10/14/18/22)
    int   forced_bases;      // bases alliées (1, 2 ou 3)
    int   forced_spawns;     // spawns ennemis (1, 2 ou 3)
    float scale_cap;         // plafond scaling HP ennemis (2.0→12.0)
    float count_mult;        // multiplicateur nombre d'ennemis (0.5→3.0)
    int   forced_deposits;   // dépôts de minerais (0=aléatoire, 2/4/6=exact)
    int   map_w, map_h;      // taille de la carte en tuiles (0 = MAP_W × MAP_H)
} CustomConfig;

typedef struct {
    int fullscreen;
    int win_width;
    int win_height;
    int target_fps;
    int master_volume;
    int music_volume;
    int sfx_volume;
    int show_fps;      // 1 = afficher le compteur FPS (touche [F] en jeu)
    int fx_effects;    // 1 = effets de jus (particules, secousse, pops) activés
    char player_name[24]; // pseudo affiché en multijoueur
    char relay_addr[32];  // serveur relais "IP:port" (vide = connexion directe)
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
    int         selected_campaign_act;  // acte cliqué sur la carte du monde (0 = début)

    CustomConfig custom_cfg;  // configuration partie personnalisée

    int         sel_upg;

    AppOptions  opts;
    int         paused;

    float       msg_timer;
    char        msg_buf[128];

    int         opt_tab;            // 0=General, 1=Audio, 2=Graphismes
    int         opt_dropdown_open;  // -1=none, 0=FPS, 1=Resolution, etc

    // Bestiaire
    int         sel_bestiary;       // index ennemi sélectionné (0..ENEMY_TYPE_COUNT-1)
    int         bestiary_tab;       // 0=Ennemis, 1=Minerais, 2=Tours, 3=Unites, 4=Butin
    int         sel_material;       // index minerai sélectionné (0..MAT_COUNT-1)
    int         sel_tower;          // index tour sélectionnée (0..TOWER_TYPE_COUNT-1)
    int         sel_unit;           // index unité sélectionnée (0..UNIT_TYPE_COUNT-1)
    int         sel_perk;           // index perk/butin sélectionné (0..PERK_COUNT-1)

    // Animation cinématique de l'écran titre
    MenuAnimState anim;

    // ── Multijoueur ──────────────────────────────────────────
    int          mp_mode;            // MpMode choisi sur le hub
    int          mp_role;            // 0=indéfini, 1=héberger, 2=rejoindre
    char         mp_code_input[20];  // saisie du code (rejoindre)
    int          mp_code_len;
    char         mp_host_ip[16];     // IP de l'hôte encodée dans le code (modifiable)
    int          mp_edit_field;      // champ texte actif : 0=aucun,1=pseudo,2=code,3=ip
    MpLobbyView  mp_view;            // état de session (rempli par app.c)
    char         mp_upnp_msg[160];   // UPNP_HOOK : résultat de l'ouverture auto de port
    int          mp_upnp_ok;         // UPNP_HOOK : 1 = port ouvert (vert), 0 = échec (rouge)
} MenuState;

typedef struct {
    int          start_arcade;
    int          start_campaign;
    int          start_custom;        // 1 = lancer une partie personnalisée
    int          resume_slot;
    int          resume_is_campaign;  // 1 = le slot à reprendre est une save campagne
    int          go_game;
    int          quit_app;
    int          save_and_quit;
    int          toggle_fs;
    ThemeID      new_theme;
    int          new_slot;
    int          campaign_order_seed;
    int          start_campaign_act;  // acte de départ (0 = premier acte)
    CustomConfig custom_cfg;          // paramètres de la partie custom à lancer

    // ── Multijoueur (intentions traitées par app.c) ──────────
    int          mp_host;             // 1 = héberger (mode = mp_mode)
    int          mp_join;             // 1 = rejoindre (code = mp_code)
    int          mp_ready;            // 1 = basculer "prêt"
    int          mp_start;            // 1 = hôte lance la partie
    int          mp_leave;            // 1 = quitter la session
    int          mp_upnp;             // UPNP_HOOK : 1 = tenter l'ouverture auto du port
    int          mp_mode;             // MpMode pour l'hébergement
    char         mp_code[20];         // code saisi pour rejoindre
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
#include "menu.h"
#include "renderer.h"
#include "meta.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ════════════════════════════════════════════════════
// PALETTE INTERNE
// ════════════════════════════════════════════════════
#define C_BG       ((Color){8,  5,  3,  255})
#define C_PANEL    ((Color){14, 9,  4,  255})
#define C_BORDER   ((Color){61, 42, 16, 255})
#define C_GOLD     ((Color){239,159,39, 255})
#define C_GREEN    ((Color){46, 204,113,255})
#define C_RED      ((Color){231,76, 60, 255})
#define C_DIM      ((Color){80, 70, 50, 255})
#define C_TEXT     ((Color){180,160,110,255})
#define C_TITLE    ((Color){239,159,39, 255})
#define C_TAB_ACT  ((Color){40, 25, 8,  255})
#define C_TAB_IDLE ((Color){14, 9,  4,  255})
#define C_HOV      ((Color){30, 20, 6,  255})
#define C_BLUE     ((Color){52, 152,219,255})

// Noms des thèmes pour le menu création
static const char *THEME_LABELS[THEME_COUNT + 1] = {
    "Terres devastees",
    "Marais toxique",
    "Desert irradie",
    "Ville en ruine",
    "Usine abandonnee",
    "Aleatoire",   // index THEME_COUNT
};

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void menu_init(MenuState *m, const AppOptions *opts) {
    memset(m, 0, sizeof(MenuState));
    m->screen        = MENU_MAIN;
    m->active_tab    = TAB_PLAY;
    m->hovered_slot  = -1;
    m->selected_slot = -1;
    m->new_theme     = THEME_COUNT;   // aléatoire par défaut
    m->new_slot      = 0;
    m->paused        = 0;
    if (opts) m->opts = *opts;
    menu_refresh_slots(m);
}

void menu_refresh_slots(MenuState *m) {
    save_scan(m->slots);
}

// ════════════════════════════════════════════════════
// HELPERS DESSIN
// ════════════════════════════════════════════════════

// Bouton centré sur cx,cy — retourne 1 si cliqué
static int btn(const char *label, int cx, int cy, int w, int h,
               Color col, int hovered) {
    Rectangle r = {(float)(cx - w/2), (float)(cy - h/2), (float)w, (float)h};
    Color bg  = hovered ? C_HOV : C_PANEL;
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, hovered ? 2.0f : 1.0f,
                         hovered ? col : C_BORDER);
    int tw = MeasureText(label, 14);
    DrawText(label, cx - tw/2, cy - 7, 14, col);
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// Bouton gauche-aligné
static int btn_left(const char *label, int x, int y, int w, int h,
                    Color col, int hovered) {
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    Color bg  = hovered ? C_HOV : C_PANEL;
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, hovered ? 2.0f : 1.0f,
                         hovered ? col : C_BORDER);
    DrawText(label, x + 8, y + h/2 - 7, 14, col);
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// Retourne 1 si la souris survole le rectangle
static int hov(int x, int y, int w, int h) {
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, (Rectangle){(float)x,(float)y,(float)w,(float)h});
}

// Texte centré
static void txt_center(const char *s, int cx, int y, int size, Color col) {
    int w = MeasureText(s, size);
    DrawText(s, cx - w/2, y, size, col);
}

// Message temporaire centré en bas
static void draw_msg(MenuState *m, int virt_w, int virt_h) {
    if (m->msg_timer <= 0.0f) return;
    int alpha = (int)(fminf(m->msg_timer / 0.5f, 1.0f) * 220.0f);
    Color col = {239,159,39,(unsigned char)alpha};
    txt_center(m->msg_buf, virt_w/2, virt_h - 30, 14, col);
}

static void set_msg(MenuState *m, const char *s) {
    strncpy(m->msg_buf, s, sizeof(m->msg_buf)-1);
    m->msg_buf[sizeof(m->msg_buf)-1] = '\0';
    m->msg_timer = 2.5f;
}

// ════════════════════════════════════════════════════
// RENDU DES ONGLETS
// ════════════════════════════════════════════════════
static void draw_tabs(const MenuState *m, int virt_w) {
    static const char *labels[TAB_COUNT] = {"JOUER","ARSENAL","OPTIONS"};
    int tab_w = 120, tab_h = 30, tab_y = 70, pad = 6;
    int total  = TAB_COUNT * tab_w + (TAB_COUNT - 1) * pad;
    int start_x = virt_w/2 - total/2;

    for (int i = 0; i < TAB_COUNT; i++) {
        int tx = start_x + i * (tab_w + pad);
        int active = (m->active_tab == i);
        Color bg  = active ? C_TAB_ACT : C_TAB_IDLE;
        Color brd = active ? C_GOLD    : C_BORDER;
        DrawRectangle(tx, tab_y, tab_w, tab_h, bg);
        DrawRectangleLinesEx((Rectangle){(float)tx,(float)tab_y,(float)tab_w,(float)tab_h},
                             active ? 2.0f : 1.0f, brd);
        txt_center(labels[i], tx + tab_w/2, tab_y + 8, 13,
                   active ? C_GOLD : C_DIM);
    }
}

// ════════════════════════════════════════════════════
// ONGLET JOUER — liste des slots
// ════════════════════════════════════════════════════
static MenuAction draw_tab_play(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;
    int y  = 120;

    txt_center("VOS PARTIES", cx, y, 16, C_GOLD); y += 30;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        const SaveInfo *si = &m->slots[i];
        int sx = cx - 220, sw2 = 440, sh = 64;
        int is_hov = hov(sx, y, sw2, sh);
        Color bg  = is_hov ? C_HOV : C_PANEL;
        Color brd = si->exists ? C_GOLD : C_BORDER;

        DrawRectangle(sx, y, sw2, sh, bg);
        DrawRectangleLinesEx((Rectangle){(float)sx,(float)y,(float)sw2,(float)sh},
                             1.5f, brd);

        if (si->exists) {
            // Slot occupé
            DrawText(TextFormat("PARTIE %d", i+1), sx+10, y+6,  13, C_GOLD);
            DrawText(si->theme_name,               sx+10, y+22, 11, C_TEXT);
            DrawText(TextFormat("Vague %d  |  %d vies  |  %dor",
                         si->wave, si->lives, si->gold),
                     sx+10, y+36, 11, C_DIM);

            // Bouton Jouer
            if (btn("REPRENDRE", sx+sw2-110, y+sh/2, 90, 28, C_GREEN,
                    hov(sx+sw2-155, y+2, 90, 28))) {
                act.resume_slot = i;
                act.go_game     = 1;
            }
            // Bouton Effacer (petit, rouge)
            int del_x = sx + sw2 - 30;
            int del_y = y + 4;
            DrawRectangle(del_x, del_y, 22, 18, (Color){40,8,8,255});
            DrawRectangleLinesEx((Rectangle){(float)del_x,(float)del_y,22,18},
                                 1.0f, C_RED);
            DrawText("X", del_x+6, del_y+2, 12, C_RED);
            if (hov(del_x, del_y, 22, 18) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m->confirm_del_slot = i;
                m->screen = MENU_CONFIRM_DEL;
            }
        } else {
            // Slot vide
            DrawText(TextFormat("EMPLACEMENT %d — vide", i+1),
                     sx+10, y+24, 12, C_DIM);
            if (btn("NOUVELLE PARTIE", sx+sw2-150, y+sh/2, 130, 28,
                    C_GREEN, hov(sx+sw2-215, y+4, 130, 28))) {
                m->new_slot   = i;
                m->new_theme  = THEME_COUNT;
                m->screen     = MENU_NEW_GAME;
            }
        }
        y += sh + 8;
    }

    // Bouton Quitter en bas
    y = virt_h - 60;
    if (btn("QUITTER LE JEU", cx, y, 160, 32, C_RED,
            hov(cx-80, y-16, 160, 32)))
        act.quit_app = 1;

    (void)virt_h;
    return act;
}

// ════════════════════════════════════════════════════
// ONGLET AMÉLIORATIONS
// ════════════════════════════════════════════════════
static MenuAction draw_tab_upgrades(MenuState *m, const MetaProgress *meta,
                                     int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;
    int y  = 120;
    (void)virt_h;

    // Ferraille
    DrawText(TextFormat("Ferraille disponible : %d", meta->scrap),
             cx - 120, y, 14, C_GREEN); y += 26;
    DrawText(TextFormat("Meilleure vague : %d   Parties : %d",
                 meta->best_wave, meta->runs_completed),
             cx - 130, y, 11, C_DIM); y += 22;

    DrawLine(40, y, virt_w-40, y, C_BORDER); y += 12;

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        int is_sel = (m->sel_upg == i);
        int is_hov = hov(40, y, virt_w-80, 26);
        if (is_hov) m->sel_upg = i;

        Color bg = is_sel ? C_TAB_ACT : (Color){0,0,0,0};
        DrawRectangle(40, y, virt_w-80, 26, bg);

        int cost = meta_upgrade_cost(meta, i);

        // Nom
        Color label_col = is_sel ? C_GOLD : C_TEXT;
        DrawText(UPGRADE_NAMES[i], 54, y+5, 13, label_col);

        // Étoiles
        int lvl = 0, maxlvl = 0;
        switch(i) {
            case UPGRADE_TOWER_DMG:   lvl=meta->lvl_tower_dmg;   maxlvl=MAX_LVL_TOWER_DMG;   break;
            case UPGRADE_TOWER_RANGE: lvl=meta->lvl_tower_range;  maxlvl=MAX_LVL_TOWER_RANGE;  break;
            case UPGRADE_TOWER_RATE:  lvl=meta->lvl_tower_rate;   maxlvl=MAX_LVL_TOWER_RATE;   break;
            case UPGRADE_UNIT_HP:     lvl=meta->lvl_unit_hp;      maxlvl=MAX_LVL_UNIT_HP;      break;
            case UPGRADE_UNIT_DMG:    lvl=meta->lvl_unit_dmg;     maxlvl=MAX_LVL_UNIT_DMG;     break;
            case UPGRADE_START_GOLD:  lvl=meta->lvl_start_gold;   maxlvl=MAX_LVL_START_GOLD;   break;
            case UPGRADE_LIVES:       lvl=meta->lvl_lives;         maxlvl=MAX_LVL_LIVES;        break;
            case UPGRADE_SCRAP_BONUS: lvl=meta->lvl_scrap_bonus;  maxlvl=MAX_LVL_SCRAP_BONUS;  break;
            default: break;
        }
        char stars[16] = {0};
        for (int s=0; s<lvl;    s++) stars[s]   = '*';
        for (int s=lvl; s<maxlvl; s++) stars[s] = '.';
        DrawText(stars, cx - 40, y+5, 13, C_GOLD);

        // Description
        DrawText(UPGRADE_DESC[i], cx + 60, y+7, 10, C_DIM);

        // Coût / MAX
        if (cost > 0) {
            Color cc = (meta->scrap >= cost) ? C_GOLD : C_RED;
            DrawText(TextFormat("%d ferraille", cost), virt_w-200, y+5, 11, cc);
            // Clic = achat
            if (is_hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (meta_upgrade((MetaProgress*)meta, i))
                    set_msg(m, "Amelioration achetee !");
                else
                    set_msg(m, "Ferraille insuffisante.");
            }
        } else {
            DrawText("MAX", virt_w-200, y+5, 11, C_GREEN);
        }

        DrawLine(40, y+26, virt_w-40, y+26, (Color){30,20,8,180});
        y += 28;
    }
    return act;
}

// ════════════════════════════════════════════════════
// ONGLET OPTIONS
// ════════════════════════════════════════════════════
static MenuAction draw_tab_options(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;
    int y  = 130;
    (void)virt_h;

    DrawText("AFFICHAGE", cx - 60, y, 14, C_GOLD); y += 28;

    // Plein écran toggle
    const char *fs_label = m->opts.fullscreen
                         ? "Mode : PLEIN ECRAN" : "Mode : FENETRE";
    if (btn_left(fs_label, cx - 160, y, 320, 30, C_BLUE,
                 hov(cx-160, y, 320, 30))) {
        m->opts.fullscreen ^= 1;
        act.toggle_fs = 1;
    }
    y += 44;

    DrawLine(cx-160, y, cx+160, y, C_BORDER); y += 16;
    DrawText("RESOLUTION FENETRE", cx-90, y, 13, C_GOLD); y += 26;

    // Quelques résolutions prédéfinies
    static const int RESOLUTIONS[][2] = {
        {1120, 770},
        {1400, 962},
        {1680, 1154},
    };
    static const char *RES_LABELS[] = {
        "1120 x 770  (defaut)",
        "1400 x 962",
        "1680 x 1154",
    };
    int nres = 3;
    for (int i = 0; i < nres; i++) {
        int is_cur = (m->opts.win_width == RESOLUTIONS[i][0]);
        Color c = is_cur ? C_GREEN : C_TEXT;
        if (btn_left(RES_LABELS[i], cx - 160, y, 320, 26, c,
                     hov(cx-160, y, 320, 26) && !is_cur)) {
            m->opts.win_width  = RESOLUTIONS[i][0];
            m->opts.win_height = RESOLUTIONS[i][1];
            // Appliqué par main.c via act (ici on n'a pas accès direct)
            // On passe par toggle_fs=0 mais main surveille opts.win_*
            act.toggle_fs = 2;   // signal "resize fenêtre"
        }
        if (is_cur) {
            DrawText("✓", cx + 170, y + 6, 13, C_GREEN);
        }
        y += 30;
    }

    return act;
}

// ════════════════════════════════════════════════════
// ÉCRAN CRÉATION DE PARTIE
// ════════════════════════════════════════════════════
static MenuAction draw_new_game(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;
    int y  = 90;
    (void)virt_h;

    txt_center("NOUVELLE PARTIE", cx, y, 18, C_GOLD); y += 36;
    DrawText(TextFormat("Emplacement : %d", m->new_slot + 1), cx-100, y, 13, C_TEXT); y += 30;

    // Sélection du thème
    DrawText("Choisir le theme de carte :", cx - 130, y, 13, C_GOLD); y += 22;

    for (int i = 0; i <= THEME_COUNT; i++) {
        int is_sel = ((int)m->new_theme == i);
        Color c = is_sel ? C_GREEN : C_TEXT;
        int bx = cx - 170, by = y, bw = 340, bh = 26;
        DrawRectangle(bx, by, bw, bh, is_sel ? C_TAB_ACT : C_PANEL);
        DrawRectangleLinesEx((Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                             is_sel ? 2.0f : 1.0f, is_sel ? C_GREEN : C_BORDER);
        DrawText(THEME_LABELS[i], bx + 10, by + 6, 12, c);
        if (is_sel) DrawText("◀", bx + bw - 20, by + 6, 12, C_GREEN);
        if (hov(bx, by, bw, bh) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m->new_theme = (ThemeID)i;
        y += 30;
    }

    y += 10;

    // Boutons Lancer / Annuler
    if (btn("LANCER", cx - 90, y + 18, 120, 30, C_GREEN,
            hov(cx - 150, y + 3, 120, 30)))
        act.start_new = 1;

    if (btn("ANNULER", cx + 90, y + 18, 120, 30, C_DIM,
            hov(cx + 30, y + 3, 120, 30)))
        m->screen = MENU_MAIN;

    return act;
}

// ════════════════════════════════════════════════════
// ÉCRAN CONFIRMATION SUPPRESSION
// ════════════════════════════════════════════════════
static MenuAction draw_confirm_del(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w/2, cy = virt_h/2;

    // Fond semi-transparent
    DrawRectangle(cx-200, cy-70, 400, 140, (Color){10,5,2,240});
    DrawRectangleLinesEx((Rectangle){(float)(cx-200),(float)(cy-70),400,140},
                         2.0f, C_RED);

    txt_center(TextFormat("Effacer la partie %d ?", m->confirm_del_slot+1),
               cx, cy-45, 15, C_RED);
    txt_center("Cette action est irreversible.", cx, cy-22, 11, C_DIM);

    if (btn("EFFACER", cx - 70, cy + 30, 110, 28, C_RED,
            hov(cx-125, cy+16, 110, 28))) {
        save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        m->screen = MENU_MAIN;
        set_msg(m, "Partie effacee.");
    }
    if (btn("ANNULER", cx + 70, cy + 30, 110, 28, C_TEXT,
            hov(cx+15, cy+16, 110, 28)))
        m->screen = MENU_MAIN;

    return act;
}

// ════════════════════════════════════════════════════
// MENU PAUSE (rendu en overlay sur le jeu)
// ════════════════════════════════════════════════════
static MenuAction draw_pause(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w/2, cy = virt_h/2;

    // Fond semi-opaque
    DrawRectangle(0, 0, virt_w, virt_h, (Color){0,0,0,160});

    int pw = 260, ph = 240;
    int px = cx - pw/2, py = cy - ph/2;
    DrawRectangle(px, py, pw, ph, (Color){10,6,2,245});
    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)pw,(float)ph},
                         2.0f, C_GOLD);

    txt_center("PAUSE", cx, py + 14, 20, C_GOLD);

    int btn_y = py + 55;
    int bw = 180, bh = 30, gap = 38;

    if (btn("REPRENDRE",        cx, btn_y,       bw, bh, C_GREEN, hov(cx-bw/2, btn_y-bh/2,       bw, bh))) m->paused = 0;
    if (btn("OPTIONS",          cx, btn_y+gap,   bw, bh, C_BLUE,  hov(cx-bw/2, btn_y+gap-bh/2,   bw, bh))) m->screen = MENU_OPTIONS;
    if (btn("SAUVEGARDER",      cx, btn_y+gap*2, bw, bh, C_GOLD,  hov(cx-bw/2, btn_y+gap*2-bh/2, bw, bh))) act.save_and_quit = 2; // signal: save only
    if (btn("MENU PRINCIPAL",   cx, btn_y+gap*3, bw, bh, C_DIM,   hov(cx-bw/2, btn_y+gap*3-bh/2, bw, bh))) act.save_and_quit = 1;
    if (btn("QUITTER LE JEU",   cx, btn_y+gap*4, bw, bh, C_RED,   hov(cx-bw/2, btn_y+gap*4-bh/2, bw, bh))) act.quit_app = 1;

    return act;
}

// ════════════════════════════════════════════════════
// OPTIONS (version overlay pause)
// ════════════════════════════════════════════════════
static MenuAction draw_options_overlay(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w/2, cy = virt_h/2;

    DrawRectangle(0, 0, virt_w, virt_h, (Color){0,0,0,160});
    int pw = 340, ph = 260;
    int px = cx-pw/2, py = cy-ph/2;
    DrawRectangle(px, py, pw, ph, (Color){10,6,2,245});
    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)pw,(float)ph},
                         2.0f, C_BORDER);
    txt_center("OPTIONS", cx, py+12, 16, C_GOLD);

    int y = py + 50;
    const char *fs_label = m->opts.fullscreen
                         ? "Mode : PLEIN ECRAN" : "Mode : FENETRE";
    if (btn_left(fs_label, px+20, y, pw-40, 28, C_BLUE,
                 hov(px+20, y, pw-40, 28))) {
        m->opts.fullscreen ^= 1;
        act.toggle_fs = 1;
    }
    y += 44;

    static const int RESOLUTIONS[][2] = {{1120,770},{1400,962},{1680,1154}};
    static const char *RES_LABELS[] = {"1120x770","1400x962","1680x1154"};
    int nres = 3;
    for (int i = 0; i < nres; i++) {
        int is_cur = (m->opts.win_width == RESOLUTIONS[i][0]);
        Color c = is_cur ? C_GREEN : C_TEXT;
        if (btn_left(RES_LABELS[i], px+20, y, pw-40, 24, c,
                     hov(px+20, y, pw-40, 24) && !is_cur)) {
            m->opts.win_width  = RESOLUTIONS[i][0];
            m->opts.win_height = RESOLUTIONS[i][1];
            act.toggle_fs = 2;
        }
        if (is_cur) DrawText("◀", px+pw-30, y+5, 11, C_GREEN);
        y += 28;
    }

    if (btn("RETOUR", cx, py+ph-25, 100, 24, C_DIM,
            hov(cx-50, py+ph-37, 100, 24)))
        m->screen = m->paused ? MENU_PAUSE : MENU_MAIN;

    return act;
}

// ════════════════════════════════════════════════════
// UPDATE PRINCIPAL
// ════════════════════════════════════════════════════
MenuAction menu_update(MenuState *m, const MetaProgress *meta) {
    (void)meta;
    MenuAction act = {0};

    // Timer message
    if (m->msg_timer > 0.0f) m->msg_timer -= GetFrameTime();

    // Navigation clavier dans les upgrades
    if (m->screen == MENU_MAIN && m->active_tab == TAB_UPGRADES) {
        if (IsKeyPressed(KEY_UP))
            m->sel_upg = (m->sel_upg - 1 + UPGRADE_COUNT) % UPGRADE_COUNT;
        if (IsKeyPressed(KEY_DOWN))
            m->sel_upg = (m->sel_upg + 1) % UPGRADE_COUNT;
        if (IsKeyPressed(KEY_ENTER))
            if (meta_upgrade((MetaProgress*)meta, m->sel_upg))
                set_msg(m, "Amelioration achetee !");
    }

    return act;  // le rendu retourne les actions réelles
}

// ════════════════════════════════════════════════════
// RENDU PRINCIPAL (dispatch selon l'écran)
// ════════════════════════════════════════════════════
void menu_render(const MenuState *m_const, const MetaProgress *meta,
                 int virt_w, int virt_h) {
    // On caste pour permettre les modifications internes (sel_upg, msg)
    MenuState *m = (MenuState *)m_const;

    MenuAction act = {0};

    // ── Pause (overlay) ───────────────────────────────────
    if (m->paused && m->screen == MENU_PAUSE) {
        act = draw_pause(m, virt_w, virt_h);
        goto apply;
    }
    if (m->paused && m->screen == MENU_OPTIONS) {
        act = draw_options_overlay(m, virt_w, virt_h);
        goto apply;
    }

    // ── Menu principal (fond + titre) ─────────────────────
    ClearBackground(C_BG);

    // Titre
    txt_center("RUST BASTION", virt_w/2, 18, 28, C_TITLE);
    DrawLine(40, 60, virt_w-40, 60, C_BORDER);

    // Onglets
    draw_tabs(m, virt_w);
    DrawLine(40, 106, virt_w-40, 106, C_BORDER);

    // Clic sur les onglets
    {
        int tab_w = 120, tab_h = 30, tab_y = 70, pad = 6;
        int total  = TAB_COUNT * tab_w + (TAB_COUNT-1) * pad;
        int start_x = virt_w/2 - total/2;
        for (int i = 0; i < TAB_COUNT; i++) {
            int tx = start_x + i*(tab_w+pad);
            if (hov(tx, tab_y, tab_w, tab_h) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                m->active_tab = (MenuTab)i;
        }
    }

    // Confirmation suppression (par-dessus tout)
    if (m->screen == MENU_CONFIRM_DEL) {
        act = draw_confirm_del(m, virt_w, virt_h);
        goto apply;
    }

    // Écran création
    if (m->screen == MENU_NEW_GAME) {
        act = draw_new_game(m, virt_w, virt_h);
        goto apply;
    }

    // Options (plein écran dans le menu principal)
    if (m->screen == MENU_OPTIONS) {
        act = draw_options_overlay(m, virt_w, virt_h);
        goto apply;
    }

    // Contenu de l'onglet actif
    if (m->active_tab == TAB_PLAY)
        act = draw_tab_play(m, virt_w, virt_h);
    else if (m->active_tab == TAB_UPGRADES)
        act = draw_tab_upgrades(m, meta, virt_w, virt_h);
    else if (m->active_tab == TAB_OPTIONS) {
        m->screen = MENU_OPTIONS;
    }

    // Message temporaire
    draw_msg(m, virt_w, virt_h);

apply:
    // Propage les actions vers main.c via la valeur de retour
    // (On utilise une variable globale de dernier recours)
    // En pratique main.c appelle menu_render() et inspecte MenuState après
    (void)act;
}

// ── Variante qui retourne les actions ─────────────────────────
// main.c appelle cette version
MenuAction menu_render_and_act(MenuState *m, const MetaProgress *meta,
                                int virt_w, int virt_h) {
    MenuAction act = {0};

    if (m->paused && m->screen == MENU_PAUSE) {
        act = draw_pause(m, virt_w, virt_h);
        return act;
    }
    if (m->paused && m->screen == MENU_OPTIONS) {
        act = draw_options_overlay(m, virt_w, virt_h);
        return act;
    }

    ClearBackground(C_BG);
    txt_center("RUST BASTION", virt_w/2, 18, 28, C_TITLE);
    DrawLine(40, 60, virt_w-40, 60, C_BORDER);
    draw_tabs(m, virt_w);
    DrawLine(40, 106, virt_w-40, 106, C_BORDER);

    // Clic sur onglets
    {
        int tab_w = 120, tab_h = 30, tab_y = 70, pad = 6;
        int total  = TAB_COUNT * tab_w + (TAB_COUNT-1) * pad;
        int start_x = virt_w/2 - total/2;
        for (int i = 0; i < TAB_COUNT; i++) {
            int tx = start_x + i*(tab_w+pad);
            if (hov(tx, tab_y, tab_w, tab_h) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m->active_tab = (MenuTab)i;
                if (i == TAB_OPTIONS) m->screen = MENU_OPTIONS;
                else                  m->screen = MENU_MAIN;
            }
        }
    }

    if (m->screen == MENU_CONFIRM_DEL) {
        act = draw_confirm_del(m, virt_w, virt_h);
    } else if (m->screen == MENU_NEW_GAME) {
        act = draw_new_game(m, virt_w, virt_h);
    } else if (m->screen == MENU_OPTIONS) {
        act = draw_options_overlay(m, virt_w, virt_h);
    } else {
        if (m->active_tab == TAB_PLAY)
            act = draw_tab_play(m, virt_w, virt_h);
        else if (m->active_tab == TAB_UPGRADES)
            act = draw_tab_upgrades(m, meta, virt_w, virt_h);
    }

    draw_msg(m, virt_w, virt_h);
    return act;
}
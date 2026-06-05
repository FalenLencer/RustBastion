/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu.c ─ Cœur du système de menus.
 *
 *  Ce fichier contient :
 *    • La persistance des options (opts_save / opts_load)
 *    • La souris virtuelle et les helpers de dessin partagés
 *    • L'initialisation et le nettoyage (menu_init, menu_cleanup)
 *    • La mise à jour clavier (menu_update)
 *    • Le dispatch principal (menu_render_and_act / menu_render)
 *
 *  Les fonctions d'écran sont réparties dans :
 *    menu_screens.c   — Title, Play Hub, Slot list, New Arcade/Campaign, Confirm, Pause
 *    menu_campaign.c  — Carte du monde
 *    menu_options.c   — Options (volume, FPS, résolution)
 *    menu_upgrades.c  — Améliorations méta
 *    menu_bestiary.c  — Bestiaire (ennemis, minerais, tours, unités)
 */

#include "menu_internal.h"
#include "../engine/paths.h"

// Couleur d'identité de chaque chapitre — source unique (était dupliquée
// à l'identique dans menu_campaign.c et menu_screens.c).
const Color CHAPTER_COLORS[CAMPAIGN_CHAPTERS] = {
    {200, 150,  80, 255},  // Ch.1 Terres Brulees — ocre
    { 60, 180,  80, 255},  // Ch.2 Marais Toxique — vert
    {220, 180,  80, 255},  // Ch.3 Desert Irradie — jaune
    {100, 140, 200, 255},  // Ch.4 Ville en Ruine — bleu
    {180,  80,  80, 255},  // Ch.5 Usine Abandonnee — rouge
};

// ════════════════════════════════════════════════════
// PERSISTANCE DES OPTIONS
// ════════════════════════════════════════════════════
#define OPTS_MAGIC   0x52424F50u   /* "RBOP" */
#define OPTS_VERSION 5             /* v5 : ajout serveur relais (relay_addr) */

typedef struct { unsigned int magic; int version; AppOptions opts; } OptsFile;

void opts_save(const AppOptions *o) {
    data_mkdir("config");
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "config/settings.bin"), "wb");
    if (!f) return;
    OptsFile hdr = { OPTS_MAGIC, OPTS_VERSION, *o };
    fwrite(&hdr, sizeof(hdr), 1, f);
    fclose(f);
}

int opts_load(AppOptions *o) {
    char path[512];
    FILE *f = fopen(data_path(path, sizeof(path), "config/settings.bin"), "rb");
    if (!f) return 0;
    OptsFile hdr;
    int ok = (fread(&hdr, sizeof(hdr), 1, f) == 1)
          && hdr.magic   == OPTS_MAGIC
          && hdr.version == OPTS_VERSION;
    fclose(f);
    if (ok) *o = hdr.opts;
    return ok;
}

// ════════════════════════════════════════════════════
// SOURIS VIRTUELLE
// ════════════════════════════════════════════════════
static float g_ox = 0.0f, g_oy = 0.0f, g_sx = 1.0f, g_sy = 1.0f;

/* Décalage horizontal de centrage du contenu de menu (écran large).
   Le rendu est translaté de g_menu_x_off via une Camera2D ; la souris doit
   donc soustraire ce même décalage pour rester alignée. Réglé chaque frame
   au début de menu_render_and_act. */
static int g_menu_x_off = 0;

void menu_set_mouse_offset(float ox, float oy, float sx, float sy) {
    g_ox = ox; g_oy = oy;
    g_sx = sx > 0.001f ? sx : 1.0f;
    g_sy = sy > 0.001f ? sy : 1.0f;
}

Vector2 vmouse(void) {
    Vector2 r = GetMousePosition();
    return (Vector2){(r.x - g_ox)/g_sx - (float)g_menu_x_off,
                     (r.y - g_oy)/g_sy};
}

int vhov(int x, int y, int w, int h) {
    Vector2 m = vmouse();
    return m.x >= x && m.x < x+w && m.y >= y && m.y < y+h;
}

int vhov_r(Rectangle r) {
    Vector2 m = vmouse();
    return CheckCollisionPointRec(m, r);
}

int vclick_r(Rectangle r) {
    return vhov_r(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void push_back_screen(MenuState *m) {
    if (m->back_stack_top < (int)(sizeof(m->back_screen_stack)/sizeof(m->back_screen_stack[0])))
        m->back_screen_stack[m->back_stack_top++] = m->back_screen;
}

void pop_back_screen(MenuState *m) {
    if (m->back_stack_top > 0)
        m->back_screen = m->back_screen_stack[--m->back_stack_top];
}

// ════════════════════════════════════════════════════
// HELPERS DE DESSIN
// ════════════════════════════════════════════════════

void txt_c(const char *s, int cx, int y, int fs, Color col) {
    dtxt(s, cx - mtxt(s, fs)/2, y, fs, col);
}

void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x+w, y, col);
}

// Texte centré avec cadre sombre pour lisibilité sur fond d'image
void txt_c_boxed(const char *s, int cx, int y, int fs, Color col) {
    int tw = mtxt(s, fs);
    int rh = fh(fs);
    int px = 8, py = 3;
    DrawRectangleRounded(
        (Rectangle){(float)(cx - tw/2 - px), (float)(y - py),
                    (float)(tw + px*2), (float)(rh + py*2)},
        0.25f, 4, (Color){5, 3, 1, 210});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)(cx - tw/2 - px), (float)(y - py),
                    (float)(tw + px*2), (float)(rh + py*2)},
        0.25f, 4, 1.0f, (Color){55, 36, 12, 120});
    dtxt(s, cx - tw/2, y, fs, col);
}

// Texte aligné à gauche avec cadre sombre
void draw_text_boxed(const char *s, int x, int y, int fs, Color col) {
    int tw = mtxt(s, fs);
    int rh = fh(fs);
    int px = 6, py = 3;
    DrawRectangleRounded(
        (Rectangle){(float)(x - px), (float)(y - py),
                    (float)(tw + px*2), (float)(rh + py*2)},
        0.25f, 4, (Color){5, 3, 1, 210});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)(x - px), (float)(y - py),
                    (float)(tw + px*2), (float)(rh + py*2)},
        0.25f, 4, 1.0f, (Color){55, 36, 12, 120});
    dtxt(s, x, y, fs, col);
}

// ── Système de fond décoratif / images ───────────────────────────
typedef enum {
    MENU_BG_TITLE = 0,
    MENU_BG_PLAY_HUB,
    MENU_BG_CAMPAIGN,
    MENU_BG_ARCADE,
    MENU_BG_UPGRADES,
    MENU_BG_COUNT,
} MenuBgID;

static Texture2D g_menu_bg[MENU_BG_COUNT];
static int g_menu_bg_loaded = 0;

static Texture2D *menu_bg_for_screen(MenuScreen screen) {
    switch (screen) {
        case MENU_TITLE:      return &g_menu_bg[MENU_BG_TITLE];
        case MENU_PLAY_HUB:   return &g_menu_bg[MENU_BG_PLAY_HUB];
        case MENU_CAMPAIGN:   return &g_menu_bg[MENU_BG_CAMPAIGN];
        case MENU_ARCADE:     return &g_menu_bg[MENU_BG_ARCADE];
        case MENU_UPGRADES:   return &g_menu_bg[MENU_BG_UPGRADES];
        default:              return NULL;
    }
}

static void menu_load_bg_textures(void) {
    if (g_menu_bg_loaded) return;
    g_menu_bg_loaded = 1;
    g_menu_bg[MENU_BG_TITLE]    = LoadTexture("assets/textures/Menue/ecran_titre.png");
    g_menu_bg[MENU_BG_PLAY_HUB] = LoadTexture("assets/textures/Menue/menue_hub.png");
    g_menu_bg[MENU_BG_CAMPAIGN] = LoadTexture("assets/textures/Menue/menu_campagne.png");
    g_menu_bg[MENU_BG_ARCADE]   = LoadTexture("assets/textures/Menue/menue_arcade.png");
    g_menu_bg[MENU_BG_UPGRADES] = LoadTexture("assets/textures/Menue/menue_upgrade.png");
}

static void menu_unload_bg_textures(void) {
    if (!g_menu_bg_loaded) return;
    g_menu_bg_loaded = 0;
    for (int i = 0; i < MENU_BG_COUNT; i++) {
        if (IsTextureValid(g_menu_bg[i])) UnloadTexture(g_menu_bg[i]);
    }
}

void draw_bg(const MenuState *m, int vw, int vh) {
    Texture2D *bg = menu_bg_for_screen(m->screen);
    if (g_menu_bg_loaded && bg && IsTextureValid(*bg)) {
        DrawTexturePro(*bg,
            (Rectangle){0, 0, (float)bg->width, (float)bg->height},
            (Rectangle){0, 0, (float)vw, (float)vh},
            (Vector2){0,0}, 0.0f, WHITE);
        return;
    }
    ClearBackground(C_BG);
    /* Grille technique discrète */
    for (int x = 0; x < vw; x += 72)
        DrawLine(x, 0, x, vh, (Color){18, 12, 5, 45});
    for (int y = 0; y < vh; y += 72)
        DrawLine(0, y, vw, y, (Color){18, 12, 5, 45});
    /* Ambiance : assombrissement haut/bas (vignette) + liseré d'accent */
    DrawRectangleGradientV(0, 0,          vw, vh/3, (Color){0,0,0,70}, (Color){0,0,0,0});
    DrawRectangleGradientV(0, vh - vh/3,  vw, vh/3, (Color){0,0,0,0},  (Color){0,0,0,95});
    DrawRectangle(0, 0, vw, 2, (Color){120, 70, 16, 130});
}

// Équerres d'angle (identité « métal militaire ») — partagé.
static void draw_corner_brackets(Rectangle r, Color c, int L) {
    int x0 = (int)r.x + 4, y0 = (int)r.y + 4;
    int x1 = (int)(r.x + r.width) - 4, y1 = (int)(r.y + r.height) - 4;
    Color cc = {c.r, c.g, c.b, 220};
    DrawRectangle(x0,     y0,     L, 2, cc); DrawRectangle(x0,     y0,     2, L, cc);
    DrawRectangle(x1 - L, y0,     L, 2, cc); DrawRectangle(x1 - 2, y0,     2, L, cc);
    DrawRectangle(x0,     y1 - 2, L, 2, cc); DrawRectangle(x0,     y1 - L, 2, L, cc);
    DrawRectangle(x1 - L, y1 - 2, L, 2, cc); DrawRectangle(x1 - 2, y1 - L, 2, L, cc);
}

// En-tête commune : titre RUST BASTION + sous-titre + séparateur
void draw_header(const char *subtitle, int vw) {
    int cx = vw/2;
    txt_c_boxed("RUST BASTION", cx, M_PAD, 28, C_GOLD);
    /* Traits d'accent de part et d'autre du titre */
    int tw = mtxt("RUST BASTION", 28);
    int ly = M_PAD + fh(28)/2;
    Color acc = {C_GOLD.r, C_GOLD.g, C_GOLD.b, 150};
    DrawRectangle(cx - tw/2 - 44, ly, 30, 2, acc);
    DrawRectangle(cx + tw/2 + 14, ly, 30, 2, acc);
    if (subtitle && subtitle[0])
        txt_c_boxed(subtitle, cx, M_PAD + 44, 13, C_TEXT);
    /* Séparateur double (accent + ombre) */
    draw_sep(M_PAD*2, M_PAD + 66, vw - M_PAD*4, C_BORDER);
    DrawRectangle(M_PAD*2, M_PAD + 68, vw - M_PAD*4, 1,
                  (Color){C_GOLD.r/3, C_GOLD.g/3, C_GOLD.b/3, 110});
}

// Panneau centré avec fond et bordure arrondis
void draw_panel(int cx, int cy, int pw, int ph, Color border) {
    Rectangle r = {cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph};
    DrawRectangleRounded(r, (float)PANEL_R/ph, 8, (Color){10, 6, 2, 250});
    /* Volume : léger reflet haut + ombre basse */
    DrawRectangle((int)r.x+2, (int)r.y+2,        pw-4, ph/3,     (Color){255,255,255,6});
    DrawRectangle((int)r.x+2, (int)r.y+ph-ph/4,  pw-4, ph/4 - 2, (Color){0,0,0,45});
    DrawRectangleRoundedLinesEx(r, (float)PANEL_R/ph, 8, 2.0f, border);
    /* Filet d'accent supérieur + équerres d'angle */
    DrawRectangle((int)r.x+10, (int)r.y+5, pw-20, 1,
                  (Color){border.r, border.g, border.b, 160});
    draw_corner_brackets(r, border, 12);
}

// Bouton rectangulaire arrondi — retourne 1 si cliqué
int draw_btn(const char *label, int x, int y, int w, int h,
             Color col, int active)
{
    Rectangle r = {(float)x,(float)y,(float)w,(float)h};
    int hov = vhov_r(r);
    float rnd = (float)BTN_R / h;
    Color bg = active  ? (Color){col.r/5, col.g/5, col.b/5, 255} :
               hov     ? (Color){col.r/7, col.g/7, col.b/7, 255} :
                         C_PANEL;
    float bw = (active || hov) ? 2.0f : 1.2f;
    Color bc = (active || hov) ? col : C_BORDER;
    DrawRectangleRounded(r, rnd, 6, bg);
    DrawRectangleRoundedLinesEx(r, rnd, 6, bw, bc);
    if (active || hov) {
        /* Tick d'accent à gauche + reflet supérieur (tactile) */
        DrawRectangle(x + 3, y + 4, 3, h - 8, col);
        DrawRectangle(x + 7, y + 3, w - 14, 1, (Color){col.r, col.g, col.b, 90});
    }
    int fs = 14;
    int tw = mtxt(label, fs);
    dtxt(label, x + w/2 - tw/2, y + h/2 - fh(fs)/2, fs, col);
    if (vclick_r(r)) {
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        return 1;
    }
    return 0;
}

int draw_volume_slider(const char *label, int x, int y, int w,
                       int value, int *out_value)
{
    char percent_text[16];
    snprintf(percent_text, sizeof(percent_text), "%d%%", value);
    draw_text_boxed(label, x, y, 11, C_GOLD);
    /* Pourcentage aligné à droite sur la même ligne que l'étiquette */
    dtxt(percent_text, x + w - mtxt(percent_text, 11), y, 11, C_TEXT);
    y += 18;
    Rectangle track = {(float)x, (float)y, (float)w, 12.0f};
    Rectangle hit   = {track.x - 2, track.y - 8, track.width + 4, track.height + 16};
    int dragging = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && vhov_r(hit);
    DrawRectangleRec(track, C_DIM);
    float fraction = value / 100.0f;
    Rectangle fill = {track.x, track.y, track.width * fraction, track.height};
    DrawRectangleRec(fill, dragging ? C_GOLD : C_BLUE);
    float knob_x = track.x + (track.width - 18.0f) * fraction;
    Vector2 knob_center = {knob_x + 9.0f, track.y + track.height * 0.5f};
    float knob_r = dragging ? 11.0f : 9.0f;
    DrawCircle((int)knob_center.x, (int)knob_center.y, knob_r, C_GOLD);
    DrawCircleLines((int)knob_center.x, (int)knob_center.y, knob_r, C_BORDER);
    if (dragging) {
        Vector2 mv = vmouse();
        int new_value = (int)(((mv.x - track.x) / track.width) * 100.0f + 0.5f);
        if (new_value < 0)   new_value = 0;
        if (new_value > 100) new_value = 100;
        if (new_value != *out_value) {
            *out_value = new_value;
            return 1;
        }
    }
    return 0;
}

// Petite flèche triangulaire "▼" pour les dropdowns
void draw_dropdown_arrow(int bx, int by, int bw, int bh, Color col) {
    int cx = bx + bw - 16;
    int cy = by + bh / 2 + 1;
    DrawTriangle(
        (Vector2){(float)(cx - 6), (float)(cy - 4)},
        (Vector2){(float)(cx + 6), (float)(cy - 4)},
        (Vector2){(float)cx,       (float)(cy + 4)},
        col);
}

// Grand bouton de navigation avec barre latérale colorée
int draw_nav_btn(const char *icon, const char *title,
                 const char *desc, Color col,
                 int x, int y, int w, int h)
{
    Rectangle r = {(float)x,(float)y,(float)w,(float)h};
    int hov = vhov_r(r);
    float rnd = (float)BTN_R / h;
    Color bg = hov ? (Color){col.r/7, col.g/7, col.b/7, 255} : C_PANEL;
    float bw = hov ? 2.0f : 1.2f;
    Color bc = hov ? col : (Color){col.r/3, col.g/3, col.b/3, 180};
    DrawRectangleRounded(r, rnd, 6, bg);
    DrawRectangleRoundedLinesEx(r, rnd, 6, bw, bc);
    DrawRectangleRounded(
        (Rectangle){(float)x, (float)(y+4), 4, (float)(h-8)},
        0.5f, 4,
        (Color){col.r, col.g, col.b, hov ? 255 : 110});
    dtxt(icon, x + M_PAD + 4, y + h/2 - fh(26)/2, 26, col);
    dtxt(title, x + M_PAD + 36, y + M_IN + 2, 16,
             hov ? col : C_TEXT);
    char dbuf[72];
    clip_text(desc, w - M_PAD - 36 - M_IN, 10, dbuf, sizeof(dbuf));
    dtxt(dbuf, x + M_PAD + 36, y + M_IN + 22, 10,
             hov ? C_TEXT : (Color){130, 110, 72, 255});
    if (vclick_r(r)) {
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        return 1;
    }
    return 0;
}

// Bouton Retour standard
int draw_back_btn(int vw, int vh) {
    (void)vw;
    return draw_btn("< RETOUR", M_PAD, vh - M_PAD - BTN_H,
                    110, BTN_H, C_DIM, 0);
}

// Message temporaire centré en bas
void draw_msg(MenuState *m, int vw, int vh) {
    if (m->msg_timer <= 0.0f) return;
    float a = fminf(m->msg_timer / 0.4f, 1.0f);
    int alpha = (int)(a * 220.0f);
    int tw = mtxt(m->msg_buf, 13);
    int mx = vw/2 - tw/2, my = vh - M_PAD - 20;
    int px = 10, py = 4;
    DrawRectangleRounded(
        (Rectangle){(float)(mx - px), (float)(my - py),
                    (float)(tw + px*2), (float)(13 + py*2)},
        0.3f, 4, (Color){5, 3, 1, (unsigned char)alpha});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)(mx - px), (float)(my - py),
                    (float)(tw + px*2), (float)(13 + py*2)},
        0.3f, 4, 1.2f, (Color){232, 152, 32, (unsigned char)(alpha/2)});
    dtxt(m->msg_buf, mx, my, 13, (Color){232,152,32,(unsigned char)alpha});
}

void set_msg(MenuState *m, const char *s) {
    strncpy(m->msg_buf, s, sizeof(m->msg_buf)-1);
    m->msg_buf[sizeof(m->msg_buf)-1] = '\0';
    m->msg_timer = 2.5f;
}

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void menu_init(MenuState *m, const AppOptions *opts) {
    memset(m, 0, sizeof(MenuState));
    m->screen            = MENU_TITLE;
    m->back_screen       = MENU_TITLE;
    m->new_theme         = THEME_COUNT;
    m->opt_dropdown_open = -1;
    m->new_slot              = -1;
    m->selected_campaign_act = -1;   // -1 = aucun acte choisi sur la carte
    m->paused                = 0;
    m->opts.fullscreen   = 0;
    m->opts.win_width    = GetScreenWidth();
    m->opts.win_height   = GetScreenHeight();
    m->opts.target_fps   = 60;
    m->opts.master_volume = (int)(audio_get_master_volume() * 100.0f + 0.5f);
    m->opts.music_volume  = (int)(audio_get_music_volume()  * 100.0f + 0.5f);
    m->opts.sfx_volume    = (int)(audio_get_sfx_volume()    * 100.0f + 0.5f);
    if (opts) m->opts = *opts;
    if (m->opts.target_fps == 0) m->opts.target_fps = 60;
    if (m->opts.master_volume < 0 || m->opts.master_volume > 100)
        m->opts.master_volume = (int)(audio_get_master_volume() * 100.0f + 0.5f);
    if (m->opts.music_volume < 0 || m->opts.music_volume > 100)
        m->opts.music_volume = (int)(audio_get_music_volume() * 100.0f + 0.5f);
    if (m->opts.sfx_volume < 0 || m->opts.sfx_volume > 100)
        m->opts.sfx_volume = (int)(audio_get_sfx_volume() * 100.0f + 0.5f);
    menu_refresh_slots(m);
    menu_load_bg_textures();
    menu_anim_init(&m->anim);
}

void menu_refresh_slots(MenuState *m) {
    save_scan(m->slots);
    campaign_save_scan(m->campaign_slots);
}

void menu_cleanup(MenuState *m) {
    menu_anim_cleanup(&m->anim);
    menu_unload_bg_textures();
}

// ════════════════════════════════════════════════════
// UPDATE
// ════════════════════════════════════════════════════
MenuAction menu_update(MenuState *m, const MetaProgress *meta) {
    MenuAction act = {0};
    if (m->msg_timer > 0.0f) m->msg_timer -= GetFrameTime();
    if (m->screen == MENU_TITLE)
        menu_anim_update(&m->anim, GetFrameTime());
    if (m->screen == MENU_UPGRADES) {
        if (IsKeyPressed(KEY_UP))
            m->sel_upg = (m->sel_upg - 1 + UPGRADE_COUNT) % UPGRADE_COUNT;
        if (IsKeyPressed(KEY_DOWN))
            m->sel_upg = (m->sel_upg + 1) % UPGRADE_COUNT;
        if (IsKeyPressed(KEY_ENTER))
            if (meta_upgrade((MetaProgress*)meta, m->sel_upg))
                set_msg(m, "Amelioration achetee !");
    }
    return act;
}

// ════════════════════════════════════════════════════
// DISPATCH PRINCIPAL
// ════════════════════════════════════════════════════
MenuAction menu_render_and_act(MenuState *m, const MetaProgress *meta,
                               int vw, int vh)
{
    MenuAction act = {0};

    /* ── Écran large : bornage + centrage du contenu ──────────────
       Les menus sont conçus pour la largeur de base (g_canvas_virt_w_base,
       = MAP_W×TILE). Sur un canvas plus large (16:9, 21:9…), vw dépasse
       cette base et les menus s'étireraient. On borne donc la largeur de
       contenu et on translate le rendu (Camera2D) pour le centrer ; la
       souris compense via g_menu_x_off (cf. vmouse). */
    int menu_w = (vw > g_canvas_virt_w_base) ? g_canvas_virt_w_base : vw;
    g_menu_x_off = (vw - menu_w) / 2;

    /* Bandes latérales : fond sombre uni (sinon on verrait la frame de jeu
       derrière le menu pause, ou la grille déborder). */
    if (g_menu_x_off > 0) {
        DrawRectangle(0, 0, g_menu_x_off, vh, (Color){8, 5, 3, 255});
        DrawRectangle(menu_w + g_menu_x_off, 0,
                      vw - menu_w - g_menu_x_off, vh, (Color){8, 5, 3, 255});
    }

    Camera2D cam = { .offset   = {(float)g_menu_x_off, 0.0f},
                     .target   = {0.0f, 0.0f},
                     .rotation = 0.0f, .zoom = 1.0f };
    BeginMode2D(cam);

    if (m->paused && m->screen == MENU_PAUSE) {
        act = draw_pause(m, menu_w, vh);
    } else if (m->paused && m->screen == MENU_OPTIONS) {
        act = draw_options(m, menu_w, vh);
    } else {
        switch (m->screen) {
            case MENU_TITLE:        act = draw_title(m, menu_w, vh);              break;
            case MENU_PLAY_HUB:     act = draw_play_hub(m, meta, menu_w, vh);     break;
            case MENU_CAMPAIGN:     act = draw_slot_list(m, menu_w, vh, 1);       break;
            case MENU_ARCADE:       act = draw_slot_list(m, menu_w, vh, 0);       break;
            case MENU_NEW_CAMPAIGN: act = draw_new_campaign(m, meta, menu_w, vh); break;
            case MENU_WORLD_MAP:    act = draw_world_map(m, meta, menu_w, vh);    break;
            case MENU_CUSTOM:       act = draw_custom_config(m, menu_w, vh);      break;
            case MENU_NEW_ARCADE:
                m->new_theme = (m->new_theme == (ThemeID)-1) ? THEME_COUNT : m->new_theme;
                act = draw_new_arcade(m, menu_w, vh);
                break;
            case MENU_UPGRADES:     act = draw_upgrades(m, meta, menu_w, vh);     break;
            case MENU_OPTIONS:      act = draw_options(m, menu_w, vh);            break;
            case MENU_BESTIARY:     act = draw_bestiary(m, meta, menu_w, vh);     break;
            case MENU_MP_HUB:       act = draw_mp_hub(m, menu_w, vh);            break;
            case MENU_MP_CONFIG:    act = draw_mp_config(m, menu_w, vh);         break;
            case MENU_MP_CONFIG_ADV:act = draw_custom_config(m, menu_w, vh);     break;
            case MENU_MP_LOBBY:     act = draw_mp_lobby(m, menu_w, vh);          break;
            case MENU_CONFIRM_DEL:
                draw_slot_list(m, menu_w, vh,
                    m->back_screen == MENU_CAMPAIGN ||
                    m->back_screen == MENU_WORLD_MAP);
                act = draw_confirm_del(m, menu_w, vh);
                break;
            default:
                m->screen = MENU_TITLE;
                break;
        }
        draw_msg(m, menu_w, vh);
    }

    EndMode2D();
    return act;
}

void menu_render(const MenuState *m_const, const MetaProgress *meta,
                 int vw, int vh)
{
    menu_render_and_act((MenuState*)m_const, meta, vw, vh);
}

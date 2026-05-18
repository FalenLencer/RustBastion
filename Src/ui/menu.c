#include "menu.h"
#include "renderer.h"
#include "ui_utils.h"
#include "campaign_data.h"
#include "../engine/audio.h"
#include "../engine/paths.h"
#include "../game/meta.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ════════════════════════════════════════════════════
// PERSISTANCE DES OPTIONS
// ════════════════════════════════════════════════════
#define OPTS_MAGIC   0x52424F50u   /* "RBOP" */
#define OPTS_VERSION 1

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
// PALETTE — couleurs communes à tout le menu
// ════════════════════════════════════════════════════
#define C_BG      ((Color){  8,   5,   3, 255})
#define C_PANEL   ((Color){ 14,   9,   4, 255})
#define C_BORDER  ((Color){ 55,  36,  12, 255})
#define C_GOLD    ((Color){232, 152,  32, 255})
#define C_GREEN   ((Color){ 42, 190, 105, 255})
#define C_RED     ((Color){218,  68,  52, 255})
#define C_DIM     ((Color){ 72,  58,  38, 255})
#define C_TEXT    ((Color){168, 148, 102, 255})
#define C_HOV     ((Color){ 26,  17,   5, 255})
#define C_BLUE    ((Color){ 48, 140, 205, 255})
#define C_DARK    ((Color){  9,   5,   2, 255})
#define C_ORANGE  ((Color){215, 118,  28, 255})

// ── Constantes de layout ─────────────────────────────────────
#define M_PAD     16    // marge extérieure des panneaux
#define M_IN       8    // marge intérieure entre éléments
#define M_LINE    12    // espacement entre lignes de texte
#define M_SECT    18    // espacement entre sections
#define BTN_H     36    // hauteur standard des boutons
#define BTN_R      5    // rayon coins arrondis boutons
#define PANEL_R    6    // rayon coins arrondis panneaux

// ════════════════════════════════════════════════════
// SOURIS VIRTUELLE
// ════════════════════════════════════════════════════
static float g_ox = 0.0f, g_oy = 0.0f, g_sx = 1.0f, g_sy = 1.0f;

void menu_set_mouse_offset(float ox, float oy, float sx, float sy) {
    g_ox = ox; g_oy = oy;
    g_sx = sx > 0.001f ? sx : 1.0f;
    g_sy = sy > 0.001f ? sy : 1.0f;
}

static Vector2 vmouse(void) {
    Vector2 r = GetMousePosition();
    return (Vector2){(r.x - g_ox)/g_sx, (r.y - g_oy)/g_sy};
}

static int vhov(int x, int y, int w, int h) {
    Vector2 m = vmouse();
    return m.x >= x && m.x < x+w && m.y >= y && m.y < y+h;
}

static int vhov_r(Rectangle r) {
    Vector2 m = vmouse();
    return CheckCollisionPointRec(m, r);
}

static int vclick_r(Rectangle r) {
    return vhov_r(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void push_back_screen(MenuState *m) {
    if (m->back_stack_top < (int)(sizeof(m->back_screen_stack)/sizeof(m->back_screen_stack[0]))) {
        m->back_screen_stack[m->back_stack_top++] = m->back_screen;
    }
}

static void pop_back_screen(MenuState *m) {
    if (m->back_stack_top > 0) {
        m->back_screen = m->back_screen_stack[--m->back_stack_top];
    }
}

// ════════════════════════════════════════════════════
// HELPERS DE DESSIN
// ════════════════════════════════════════════════════

// Texte centré horizontalement
static void txt_c(const char *s, int cx, int y, int fs, Color col) {
    dtxt(s, cx - mtxt(s, fs)/2, y, fs, col);
}

// Ligne de séparation horizontale
static void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x+w, y, col);
}

// Texte centré avec petit cadre sombre pour lisibilité sur fond d'image
static void txt_c_boxed(const char *s, int cx, int y, int fs, Color col) {
    int tw = mtxt(s, fs);
    int px = 8, py = 3;
    DrawRectangleRounded(
        (Rectangle){(float)(cx - tw/2 - px), (float)(y - py),
                    (float)(tw + px*2), (float)(fs + py*2)},
        0.25f, 4, (Color){5, 3, 1, 210});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)(cx - tw/2 - px), (float)(y - py),
                    (float)(tw + px*2), (float)(fs + py*2)},
        0.25f, 4, 1.0f, (Color){55, 36, 12, 120});
    dtxt(s, cx - tw/2, y, fs, col);
}

// Texte aligné à gauche avec petit cadre sombre
static void draw_text_boxed(const char *s, int x, int y, int fs, Color col) {
    int tw = mtxt(s, fs);
    int px = 6, py = 3;
    DrawRectangleRounded(
        (Rectangle){(float)(x - px), (float)(y - py),
                    (float)(tw + px*2), (float)(fs + py*2)},
        0.25f, 4, (Color){5, 3, 1, 210});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)(x - px), (float)(y - py),
                    (float)(tw + px*2), (float)(fs + py*2)},
        0.25f, 4, 1.0f, (Color){55, 36, 12, 120});
    dtxt(s, x, y, fs, col);
}

// Fond quadrillage décoratif ou image de fond de menu
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

static void draw_bg(const MenuState *m, int vw, int vh) {
    Texture2D *bg = menu_bg_for_screen(m->screen);
    if (g_menu_bg_loaded && bg && IsTextureValid(*bg)) {
        DrawTexturePro(*bg,
            (Rectangle){0, 0, (float)bg->width, (float)bg->height},
            (Rectangle){0, 0, (float)vw, (float)vh},
            (Vector2){0,0}, 0.0f, WHITE);
        return;
    }

    ClearBackground(C_BG);
    for (int x = 0; x < vw; x += 72)
        DrawLine(x, 0, x, vh, (Color){18, 12, 5, 50});
    for (int y = 0; y < vh; y += 72)
        DrawLine(0, y, vw, y, (Color){18, 12, 5, 50});
}

// En-tête commune : titre RUST BASTION + sous-titre + séparateur
static void draw_header(const char *subtitle, int vw) {
    int cx = vw/2;
    txt_c_boxed("RUST BASTION", cx, M_PAD, 28, C_GOLD);
    draw_sep(M_PAD*2, M_PAD + 36, vw - M_PAD*4, C_BORDER);
    if (subtitle && subtitle[0])
        txt_c_boxed(subtitle, cx, M_PAD + 44, 13, C_TEXT);
}

// Panneau centré avec fond et bordure arrondis
static void draw_panel(int cx, int cy, int pw, int ph, Color border) {
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        (float)PANEL_R/ph, 8, (Color){10, 6, 2, 250});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        (float)PANEL_R/ph, 8, 2.0f, border);
}

// Bouton rectangulaire arrondi — retourne 1 si cliqué
static int draw_btn(const char *label, int x, int y, int w, int h,
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

    int fs = 14;
    int tw = mtxt(label, fs);
    dtxt(label, x + w/2 - tw/2, y + h/2 - fs/2, fs, col);
    if (vclick_r(r)) {
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        return 1;
    }
    return 0;
}

static int draw_volume_slider(const char *label, int x, int y, int w,
                              int value, int *out_value)
{
    draw_text_boxed(label, x, y, 11, C_GOLD);
    y += 18;

    Rectangle track = {(float)x, (float)y, (float)w, 12.0f};

    // Zone d'interaction élargie pour attraper le knob plus facilement
    Rectangle hit = {track.x - 2, track.y - 8, track.width + 4, track.height + 16};

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

    char percent_text[16];
    snprintf(percent_text, sizeof(percent_text), "%d%%", value);
    dtxt(percent_text, x + w - mtxt(percent_text, 11),
             y + (int)track.height + 6, 11, C_TEXT);

    if (dragging) {
        Vector2 m = vmouse();
        int new_value = (int)(((m.x - track.x) / track.width) * 100.0f + 0.5f);
        if (new_value < 0)   new_value = 0;
        if (new_value > 100) new_value = 100;
        if (new_value != *out_value) {
            *out_value = new_value;
            return 1;
        }
    }
    return 0;
}

// Grand bouton de navigation avec barre latérale colorée
static int draw_nav_btn(const char *icon, const char *title,
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

    // Barre colorée gauche
    DrawRectangleRounded(
        (Rectangle){(float)x, (float)(y+4), 4, (float)(h-8)},
        0.5f, 4,
        (Color){col.r, col.g, col.b, hov ? 255 : 110});

    // Icône
    dtxt(icon, x + M_PAD + 4, y + h/2 - 13, 26, col);

    // Titre
    dtxt(title, x + M_PAD + 36, y + M_IN + 2, 16,
             hov ? col : C_TEXT);

    // Description clippée
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

// Bouton Retour
static int draw_back_btn(int vw, int vh) {
    (void)vw;
    return draw_btn("< RETOUR", M_PAD, vh - M_PAD - BTN_H,
                    110, BTN_H, C_DIM, 0);
}

// Message temporaire centré en bas
static void draw_msg(MenuState *m, int vw, int vh) {
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

static void set_msg(MenuState *m, const char *s) {
    strncpy(m->msg_buf, s, sizeof(m->msg_buf)-1);
    m->msg_buf[sizeof(m->msg_buf)-1] = '\0';
    m->msg_timer = 2.5f;
}

// ════════════════════════════════════════════════════
// INIT
// ════════════════════════════════════════════════════
void menu_init(MenuState *m, const AppOptions *opts) {
    memset(m, 0, sizeof(MenuState));
    m->screen      = MENU_TITLE;
    m->back_screen = MENU_TITLE;
    m->new_theme   = THEME_COUNT;
    m->new_slot    = 0;
    m->paused      = 0;
    m->opts.fullscreen = 0;
    m->opts.win_width = GetScreenWidth();
    m->opts.win_height = GetScreenHeight();
    m->opts.target_fps = 60;
    m->opts.master_volume = (int)(audio_get_master_volume() * 100.0f + 0.5f);
    m->opts.music_volume = (int)(audio_get_music_volume() * 100.0f + 0.5f);
    m->opts.sfx_volume = (int)(audio_get_sfx_volume() * 100.0f + 0.5f);
    if (opts) m->opts = *opts;
    if (m->opts.target_fps == 0) m->opts.target_fps = 60;
    if (m->opts.master_volume < 0 || m->opts.master_volume > 100) m->opts.master_volume = (int)(audio_get_master_volume() * 100.0f + 0.5f);
    if (m->opts.music_volume < 0 || m->opts.music_volume > 100) m->opts.music_volume = (int)(audio_get_music_volume() * 100.0f + 0.5f);
    if (m->opts.sfx_volume < 0 || m->opts.sfx_volume > 100) m->opts.sfx_volume = (int)(audio_get_sfx_volume() * 100.0f + 0.5f);
    menu_refresh_slots(m);
    menu_load_bg_textures();
}

void menu_refresh_slots(MenuState *m) {
    save_scan(m->slots);
}

void menu_cleanup(MenuState *m) {
    (void)m;
    menu_unload_bg_textures();
}

// ════════════════════════════════════════════════════
// ÉCRAN TITRE
// ════════════════════════════════════════════════════
static MenuAction draw_title(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);

    // Titre principal
    txt_c_boxed("RUST BASTION", cx, vh/2 - 170, 46, C_GOLD);
    txt_c_boxed("Tower Defense Post-Apocalyptique", cx, vh/2 - 114, 13, C_DIM);
    draw_sep(cx - 180, vh/2 - 94, 360, C_BORDER);

    // Boutons
    int bw = 240, bh = BTN_H + 4;
    int bx = cx - bw/2;
    int by = vh/2 - 52;

    if (draw_btn("JOUER",   bx, by, bw, bh, C_GREEN, 0)) {
        m->screen = MENU_PLAY_HUB;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (draw_btn("OPTIONS", bx, by, bw, bh, C_BLUE, 0)) {
        m->screen = MENU_OPTIONS;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (draw_btn("QUITTER", bx, by, bw, bh, C_RED, 0))
        act.quit_app = 1;

    // Version
    dtxt("v0.1", vw - M_PAD - 28, vh - M_PAD - 12, 9, C_DIM);
    return act;
}

// ════════════════════════════════════════════════════
// HUB JOUER
// ════════════════════════════════════════════════════
static MenuAction draw_play_hub(MenuState *m, const MetaProgress *meta,
                                int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("CHOISIR UN MODE", vw);

    int bw = 520, bh = 72, gap = M_IN + 2;
    int bx = cx - bw/2;
    int by = M_PAD + 76;

    if (draw_nav_btn("C", "CAMPAGNE",
                     "Carte de progression — 5 chapitres, 15 actes.",
                     C_GOLD, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_WORLD_MAP;     // ← carte du monde d'abord
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    if (draw_nav_btn("A", "ARCADE",
                     "Choisissez un environnement et jouez librement.",
                     C_BLUE, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_ARCADE;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    char upg_desc[80];
    snprintf(upg_desc, sizeof(upg_desc),
             "Depensez vos %d ferrailles pour ameliorer vos defenses.",
             meta->scrap);
    if (draw_nav_btn("*", "AMELIORATIONS", upg_desc,
                     C_ORANGE, bx, by, bw, bh)) {
        push_back_screen(m);
        m->screen = MENU_UPGRADES;
        m->back_screen = MENU_PLAY_HUB;
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// LISTE DE SLOTS
// ════════════════════════════════════════════════════
static MenuAction draw_slot_list(MenuState *m, int vw, int vh,
                                 int is_campaign)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header(is_campaign ? "CAMPAGNE" : "ARCADE", vw);

    // Sous-titre (décalé sous l'en-tête pour éviter la superposition)
    const char *sub = is_campaign
        ? "5 environnements en sequence — la ferraille se gagne ici"
        : "Mode libre — choisissez votre terrain";
    txt_c_boxed(sub, cx, M_PAD + 62, 10, C_TEXT);

    int sw = 540, sh = 68, sg = M_IN;
    int sx = cx - sw/2;
    int y  = M_PAD + 82;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        const SaveInfo *si = &m->slots[i];
        int slot_matches = !si->exists ||
                           ((int)si->mode == (is_campaign
                                ? SAVE_MODE_CAMPAIGN : SAVE_MODE_ARCADE));

        Rectangle r = {(float)sx,(float)y,(float)sw,(float)sh};
        int hov = vhov_r(r);
        Color brd = (si->exists && slot_matches)
                  ? (is_campaign ? C_GOLD : C_BLUE) : C_BORDER;
        Color bg  = hov ? C_HOV : C_PANEL;

        DrawRectangleRounded(r, (float)PANEL_R/sh, 6, bg);
        DrawRectangleRoundedLinesEx(r, (float)PANEL_R/sh, 6,
                                    1.5f, brd);

        int tx = sx + M_IN + 4;
        int ty = y  + M_IN;

        if (si->exists && slot_matches) {
            if (is_campaign) {
                int themes[CAMPAIGN_STAGES];
                meta_campaign_theme_order(si->campaign_order_seed, themes);
                const Theme *sth = theme_get((ThemeID)themes[si->campaign_stage]);

                char raw[80];
                snprintf(raw, sizeof(raw), "CAMPAGNE %d  —  Stage %d/%d : %s",
                         si->campaign_num+1, si->campaign_stage+1,
                         CAMPAIGN_STAGES, sth->name);
                char cbuf[80];
                int max_cw = sw - 120 - M_IN*2;
                clip_text(raw, max_cw, 12, cbuf, sizeof(cbuf));
                draw_text_boxed(cbuf, tx, ty, 12, C_GOLD);
            } else {
                dtxt(TextFormat("ARCADE  —  %s", si->theme_name),
                         tx, ty, 12, C_BLUE);
            }
            ty += 16;
            dtxt(TextFormat("Vague %d  |  %d vies  |  %d or",
                         si->wave, si->lives, si->gold),
                     tx, ty, 10, C_TEXT);
            ty += M_LINE;
            dtxt(TextFormat("Slot %d", i+1),
                     tx, ty, 9, C_DIM);

            // Bouton REPRENDRE
            int bw2 = 88, bh2 = 26;
            int bx2 = sx + sw - bw2 - M_IN - 22;
            int by2 = y  + sh/2 - bh2/2;
            if (draw_btn("REPRENDRE", bx2, by2, bw2, bh2, C_GREEN, 0)) {
                act.resume_slot = i;
                act.go_game     = 1;
            }

            // Bouton X (supprimer)
            Rectangle xr = {(float)(sx+sw-M_IN-18),
                             (float)(y+M_IN/2), 18, 18};
            int xhov = vhov_r(xr);
            DrawRectangleRounded(xr, 0.3f, 4,
                xhov ? (Color){48,6,6,255} : (Color){28,4,4,255});
            DrawRectangleRoundedLinesEx(xr, 0.3f, 4, 1.2f,
                xhov ? C_RED : (Color){90,16,16,255});
            int xw = mtxt("x", 10);
            dtxt("x", (int)(xr.x + xr.width/2 - xw/2),
                     (int)(xr.y + xr.height/2 - 5), 10, C_RED);
            if (vclick_r(xr)) {
                m->confirm_del_slot = i;
                m->screen = MENU_CONFIRM_DEL;
            }

        } else if (!si->exists) {
            // Slot vide
            dtxt(TextFormat("Emplacement %d — vide", i+1),
                     tx, y + sh/2 - 8, 11, C_DIM);
            int bw2 = 148, bh2 = 26;
            int bx2 = sx + sw - bw2 - M_IN;
            int by2 = y  + sh/2 - bh2/2;
            const char *lbl = is_campaign ? "NOUVELLE CAMPAGNE"
                                          : "NOUVELLE ARCADE";
            Color lc = is_campaign ? C_GOLD : C_BLUE;
            if (draw_btn(lbl, bx2, by2, bw2, bh2, lc, 0)) {
                m->new_slot            = i;
                m->campaign_order_seed = 0;
                m->screen = is_campaign ? MENU_NEW_CAMPAIGN
                                        : MENU_NEW_ARCADE;
            }
        } else {
            dtxt(TextFormat("Emplacement %d — autre mode", i+1),
                     tx, y + sh/2 - 8, 11, C_DIM);
        }

        y += sh + sg;
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// NOUVELLE CAMPAGNE
// ════════════════════════════════════════════════════
static MenuAction draw_new_campaign(MenuState *m,
                                    const MetaProgress *meta,
                                    int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;
    draw_bg(m, vw, vh);
    draw_header("NOUVELLE CAMPAGNE", vw);

    // Génère le seed une seule fois
    if (m->campaign_order_seed == 0)
        m->campaign_order_seed = GetRandomValue(1, 999999);

    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(m->campaign_order_seed, themes);

    int pw = 480, ph = 310;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int py = cy - ph/2 + M_PAD;

    dtxt(TextFormat("Emplacement : %d", m->new_slot+1),
             px, py, 10, C_DIM);
    py += M_LINE + 2;
    dtxt(TextFormat("Campagne n°%d", meta->campaigns_completed+1),
             px, py, 17, C_GOLD);
    py += 22;
    draw_sep(px, py, pw - M_PAD*2, C_BORDER);
    py += M_IN + 2;
    dtxt("Ordre des environnements :", px, py, 10, C_DIM);
    py += M_LINE + 2;

    static const Color stage_cols[CAMPAIGN_STAGES] = {
        {232,152, 32,255},{42,190,105,255},{48,140,205,255},
        {142, 80,168,255},{215,118, 28,255},
    };

    for (int i = 0; i < CAMPAIGN_STAGES; i++) {
        const Theme *th = theme_get((ThemeID)themes[i]);
        Rectangle row = {(float)px,(float)py,(float)(pw - M_PAD*2), 28};
        DrawRectangleRounded(row, (float)PANEL_R/28, 5,
                             (Color){18,10,3,210});
        DrawRectangleRoundedLinesEx(row, (float)PANEL_R/28, 5, 1.0f,
            (Color){stage_cols[i].r/3,stage_cols[i].g/3,
                    stage_cols[i].b/3, 180});

        dtxt(TextFormat("%d.", i+1), px + M_IN, py + 8, 11, C_DIM);

        dtxt(th->name, px + M_IN + 22, py + 8, 11, stage_cols[i]);

        // Description clippée à droite
        int avail = pw - M_PAD*2 - M_IN - 22 -
                    mtxt(th->name, 11) - M_IN*2;
        if (avail > 40) {
            char dbuf[48];
            clip_text(th->description, avail, 9, dbuf, sizeof(dbuf));
            dtxt(dbuf,
                     px + M_IN + 22 + mtxt(th->name,11) + M_IN,
                     py + 10, 9, C_DIM);
        }
        py += 28 + 4;
    }

    // Boutons
    py = cy + ph/2 - M_PAD - BTN_H;
    int half = (pw - M_PAD*2 - M_IN) / 2;

    if (draw_btn("LANCER", px, py, half, BTN_H, C_GREEN, 0)) {
        act.start_campaign      = 1;
        act.new_slot            = m->new_slot;
        act.campaign_order_seed = m->campaign_order_seed;
    }
    if (draw_btn("ANNULER", px + half + M_IN, py,
                 pw - M_PAD*2 - M_IN - half, BTN_H, C_DIM, 0)) {
        m->campaign_order_seed = 0;
        m->screen = MENU_CAMPAIGN;
    }
    return act;
}

// ════════════════════════════════════════════════════
// NOUVELLE ARCADE
// ════════════════════════════════════════════════════
static const char *THEME_LABELS[THEME_COUNT+1] = {
    "Terres devastees","Marais toxique","Desert irradie",
    "Ville en ruine","Usine abandonnee","Aleatoire",
};

static MenuAction draw_new_arcade(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("NOUVELLE PARTIE ARCADE", vw);

    dtxt(TextFormat("Emplacement : %d", m->new_slot+1),
             cx - 100, M_PAD + 62, 11, C_TEXT);

    int bw = 300, bh = 30, gap = 5;
    int bx = cx - bw/2;
    int by = M_PAD + 82;

    draw_text_boxed("Choisir un environnement :", bx, by, 11, C_GOLD);
    by += M_LINE + 4;

    for (int i = 0; i <= THEME_COUNT; i++) {
        int is_sel = ((int)m->new_theme == i);
        Rectangle r = {(float)bx,(float)by,(float)bw,(float)bh};
        float rnd = (float)BTN_R/bh;

        Color bg  = is_sel ? (Color){6,18,32,255} : C_PANEL;
        Color brd = is_sel ? C_BLUE : C_BORDER;
        float lw  = is_sel ? 2.0f : 1.0f;

        DrawRectangleRounded(r, rnd, 5, bg);
        DrawRectangleRoundedLinesEx(r, rnd, 5, lw, brd);
        dtxt(THEME_LABELS[i], bx + M_IN, by + bh/2 - 5, 11,
                 is_sel ? C_BLUE : C_TEXT);
        if (is_sel) {
            const char *chk = "✓";
            dtxt(chk, bx + bw - M_IN - mtxt(chk,11),
                     by + bh/2 - 5, 11, C_BLUE);
        }
        if (vclick_r(r)) m->new_theme = (ThemeID)i;
        by += bh + gap;
    }

    by += M_IN;
    int half = (bw - M_IN) / 2;
    if (draw_btn("LANCER",  bx,            by, half, BTN_H, C_GREEN, 0)) {
        act.start_arcade = 1;
        act.new_theme    = m->new_theme;
        act.new_slot     = m->new_slot;
    }
    if (draw_btn("ANNULER", bx+half+M_IN,  by,
                 bw-M_IN-half, BTN_H, C_DIM, 0))
        m->screen = MENU_ARCADE;

    return act;
}

// ════════════════════════════════════════════════════
// AMÉLIORATIONS
// ════════════════════════════════════════════════════
static MenuAction draw_upgrades(MenuState *m, const MetaProgress *meta,
                                int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("AMELIORATIONS", vw);

    // Sous-titre (décalé sous l'en-tête pour éviter la superposition)
    txt_c_boxed("La ferraille se gagne uniquement en completant des stages de campagne.",
                cx, M_PAD + 62, 10, C_TEXT);

    // Bandeau ferraille
    {
        int fw = 220, fh = 28;
        int fx = cx - fw/2, fy = M_PAD + 80;
        Rectangle fr = {(float)fx,(float)fy,(float)fw,(float)fh};
        DrawRectangleRounded(fr, (float)PANEL_R/fh, 5, (Color){6,20,6,255});
        DrawRectangleRoundedLinesEx(fr, (float)PANEL_R/fh, 5, 1.5f, C_GREEN);
        txt_c(TextFormat("Ferraille : %d", meta->scrap),
              cx, fy + fh/2 - 6, 13, C_GREEN);
    }

    // Stats globales
    int lx = M_PAD * 3;
    int rw = vw - lx * 2;
    int y  = M_PAD + 116;
    draw_text_boxed(TextFormat("Campagnes terminees : %d     Meilleure vague : %d",
                 meta->campaigns_completed, meta->best_wave),
             lx, y, 10, C_TEXT);
    y += M_LINE + 2;
    draw_sep(lx, y, rw, C_BORDER);
    y += M_IN;

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        // Ligne de fond permanente + surlignage au survol/sélection
        int hov = vhov(lx, y, rw, 30);
        if (hov) m->sel_upg = i;
        int is_sel = (m->sel_upg == i);

        Rectangle row = {(float)lx,(float)y,(float)rw, 30};
        DrawRectangleRounded(row, (float)PANEL_R/30, 5,
            is_sel ? (Color){32, 20, 5, 255} : (Color){14, 9, 3, 200});

        // Nom
        draw_text_boxed(UPGRADE_NAMES[i], lx + M_IN, y + 9, 12,
                        is_sel ? C_GOLD : C_TEXT);

        // Niveaux (pastilles)
        int lvl = 0, maxlvl = 0;
        switch (i) {
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

        // Pastilles de niveau — centrées
        int pip_w = 13, pip_h = 9, pip_gap = 3;
        int pip_total = maxlvl * (pip_w + pip_gap) - pip_gap;
        int pip_x = cx - pip_total/2;
        int pip_y = y + 11;
        for (int s = 0; s < maxlvl; s++) {
            Color pc = (s < lvl) ? C_GOLD : (Color){35,25,8,255};
            Rectangle pip = {(float)(pip_x + s*(pip_w+pip_gap)),
                             (float)pip_y, (float)pip_w, (float)pip_h};
            DrawRectangleRounded(pip, 0.4f, 3, pc);
            DrawRectangleRoundedLinesEx(pip, 0.4f, 3, 0.8f,
                (Color){65,48,15,160});
        }

        // Description clippée à droite du centre
        int desc_x  = cx + pip_total/2 + M_IN*2;
        int desc_max = lx + rw - 92 - desc_x - M_IN;
        if (desc_max > 30) {
            char udesc[48];
            clip_text(UPGRADE_DESC[i], desc_max, 9, udesc, sizeof(udesc));
            dtxt(udesc, desc_x, y + 11, 9, C_TEXT);
        }

        // Coût / MAX à droite
        int cost = meta_upgrade_cost(meta, i);
        int cost_x = lx + rw - 82;
        if (cost > 0) {
            int can = meta->scrap >= cost;
            draw_text_boxed(TextFormat("%d ferr.", cost),
                            cost_x, y + 9, 11,
                            can ? C_GOLD : C_RED);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (meta_upgrade((MetaProgress*)meta, i))
                    set_msg(m, "Amelioration achetee !");
                else
                    set_msg(m, "Ferraille insuffisante.");
            }
        } else {
            draw_text_boxed("MAX", cost_x + 10, y + 9, 11, C_GREEN);
        }

        draw_sep(lx, y+30, rw, (Color){25,16,5,140});
        y += 31;
    }

    draw_msg(m, vw, vh);
    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// OPTIONS
// ════════════════════════════════════════════════════
static MenuAction draw_options(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    if (m->paused)
        DrawRectangle(0, 0, vw, vh, (Color){0,0,0,155});
    else
        draw_bg(m, vw, vh);

    int pw = 600, ph = 500;
    draw_panel(cx, cy, pw, ph, C_BORDER);

    int px = cx - pw/2;
    int py = cy - ph/2;
    int iw = pw - M_PAD*2;

    txt_c_boxed("OPTIONS", cx, py + M_PAD, 19, C_GOLD);
    py += 30;
    draw_sep(px + M_PAD, py, iw, C_BORDER);
    py += M_IN + 4;

    // ── Onglets horizontaux ────────────────────────────────────
    const char *tabs[3] = {"General", "Audio", "Graphismes"};
    int tab_w = 120, tab_h = 24;
    int tab_gap = 10;
    int tabs_total = tab_w * 3 + tab_gap * 2;
    int tab_start_x = cx - tabs_total/2;

    for (int i = 0; i < 3; i++) {
        int tx = tab_start_x + i * (tab_w + tab_gap);
        Rectangle tr = {(float)tx, (float)py, (float)tab_w, (float)tab_h};
        int is_sel = (m->opt_tab == i);
        int hov = vhov_r(tr);

        Color bg = is_sel ? (Color){20, 15, 5, 255} : C_PANEL;
        Color bd = is_sel ? C_GOLD : C_BORDER;
        float bw = is_sel ? 2.0f : 1.0f;

        DrawRectangleRounded(tr, 0.3f, 4, bg);
        DrawRectangleRoundedLinesEx(tr, 0.3f, 4, bw, bd);

        int tlen = mtxt(tabs[i], 11);
        dtxt(tabs[i], tx + tab_w/2 - tlen/2, py + tab_h/2 - 6, 11,
                 is_sel ? C_GOLD : C_TEXT);

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m->opt_tab = i;
    }

    py += tab_h + 16;

    // ── Contenu des onglets ────────────────────────────────────
    int content_x = px + M_PAD;
    int content_y = py;
    int content_w = iw;

    switch (m->opt_tab) {
    case 0: // General
        {
            int y = content_y;
            draw_text_boxed("Plein ecran", content_x, y, 11, C_GOLD);
            y += 16;

            const char *fsl = m->opts.fullscreen ? "[*] Activé" : "[ ] Désactivé";
            if (draw_btn(fsl, content_x, y, content_w, BTN_H, C_BLUE, m->opts.fullscreen)) {
                m->opts.fullscreen ^= 1;
                act.toggle_fs = 1;
            }
            y += BTN_H + 20;

            draw_text_boxed("FPS Cible", content_x, y, 11, C_GOLD);
            y += 16;

            // Dropdown FPS
            static const int   FPS_OPTS[] = {30, 60, 120, 165, 0};
            static const char *FPS_LBL [] = {"30 FPS","60 FPS","120 FPS","165 FPS","Illimité"};
            char fps_display[32];
            if (m->opts.target_fps == 0)
                snprintf(fps_display, sizeof(fps_display), "Illimité ▼");
            else
                snprintf(fps_display, sizeof(fps_display), "%d FPS ▼", m->opts.target_fps);

            if (draw_btn(fps_display, content_x, y, content_w, BTN_H, C_BLUE, 0))
                m->opt_dropdown_open = (m->opt_dropdown_open == 0) ? -1 : 0;

            y += BTN_H + 4;

            if (m->opt_dropdown_open == 0) {
                for (int i = 0; i < 5; i++) {
                    if (draw_btn(FPS_LBL[i], content_x, y, content_w, 26, C_TEXT,
                                 m->opts.target_fps == FPS_OPTS[i])) {
                        m->opts.target_fps = FPS_OPTS[i];
                        SetTargetFPS(FPS_OPTS[i]);
                        m->opt_dropdown_open = -1;
                    }
                    y += 30;
                }
            }
        }
        break;

    case 1: // Audio
        {
            int y = content_y;
            draw_text_boxed("Parametres Audio", content_x, y, 11, C_GOLD);
            y += 22;

            const char *music_toggle = m->opts.music_volume > 0 ? "[*] Musique" : "[ ] Musique";
            if (draw_btn(music_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.music_volume > 0)) {
                if (m->opts.music_volume > 0) {
                    m->opts.music_volume = 0;
                } else {
                    m->opts.music_volume = 60;
                }
                audio_set_music_volume(m->opts.music_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Musique", content_x, y, content_w, m->opts.music_volume, &m->opts.music_volume)) {
                audio_set_music_volume(m->opts.music_volume / 100.0f);
            }
            y += 42;

            const char *sfx_toggle = m->opts.sfx_volume > 0 ? "[*] Effets" : "[ ] Effets";
            if (draw_btn(sfx_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.sfx_volume > 0)) {
                if (m->opts.sfx_volume > 0) {
                    m->opts.sfx_volume = 0;
                } else {
                    m->opts.sfx_volume = 95;
                }
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume Effets", content_x, y, content_w, m->opts.sfx_volume, &m->opts.sfx_volume)) {
                audio_set_sfx_volume(m->opts.sfx_volume / 100.0f);
            }
            y += 42;

            const char *master_toggle = m->opts.master_volume > 0 ? "[*] Volume général" : "[ ] Volume général";
            if (draw_btn(master_toggle, content_x, y, content_w, BTN_H, C_BLUE, m->opts.master_volume > 0)) {
                if (m->opts.master_volume > 0) {
                    m->opts.master_volume = 0;
                } else {
                    m->opts.master_volume = 80;
                }
                audio_set_master_volume(m->opts.master_volume / 100.0f);
                audio_play_sfx(AUDIO_SFX_MENU_CONFIRM);
            }
            y += BTN_H + 14;

            if (draw_volume_slider("Volume général", content_x, y, content_w, m->opts.master_volume, &m->opts.master_volume)) {
                audio_set_master_volume(m->opts.master_volume / 100.0f);
            }
        }
        break;

    case 2: // Graphismes
        {
            int y = content_y;
            draw_text_boxed("Resolution", content_x, y, 11, C_GOLD);
            y += 16;

            // Dropdown Résolution
            static const int RES[][2] = {
                {1120,770},{1400,962},{1680,1154},{1960,1346},{2240,1540}
            };
            static const char *RLBL[] = {
                "1120 × 770  (1×)","1400 × 962  (1.25×)","1680 × 1154 (1.5×)",
                "1960 × 1346 (1.75×)","2240 × 1540 (2×)",
            };

            char res_display[64];
            snprintf(res_display, sizeof(res_display), "%d × %d ▼",
                     m->opts.win_width, m->opts.win_height);

            if (draw_btn(res_display, content_x, y, content_w, BTN_H, C_BLUE, 0))
                m->opt_dropdown_open = (m->opt_dropdown_open == 1) ? -1 : 1;

            y += BTN_H + 4;

            if (m->opt_dropdown_open == 1) {
                for (int i = 0; i < 5; i++) {
                    int cur = (m->opts.win_width == RES[i][0]);
                    if (draw_btn(RLBL[i], content_x, y, content_w, 26, C_TEXT, cur)) {
                        m->opts.win_width  = RES[i][0];
                        m->opts.win_height = RES[i][1];
                        act.toggle_fs = 2;
                        m->opt_dropdown_open = -1;
                    }
                    y += 30;
                }
            }
        }
        break;
    }

    // Info résolution actuelle
    char info[48];
    snprintf(info, sizeof(info), "%dx%d",
             GetScreenWidth(), GetScreenHeight());
    txt_c(info, cx, cy + ph/2 - M_PAD - 26, 9, C_DIM);

    // Bouton retour
    if (draw_btn("RETOUR", cx - 55, cy + ph/2 - M_PAD - BTN_H,
                 110, BTN_H, C_DIM, 0))
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// CONFIRMATION SUPPRESSION
// ════════════════════════════════════════════════════
static MenuAction draw_confirm_del(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,140});

    int pw = 360, ph = 120;
    draw_panel(cx, cy, pw, ph, C_RED);

    txt_c(TextFormat("Effacer la partie %d ?", m->confirm_del_slot+1),
          cx, cy - ph/2 + M_PAD, 14, C_RED);
    txt_c("Cette action est irreversible.",
          cx, cy - ph/2 + M_PAD + 22, 10, C_DIM);

    int bw2 = 110, bh2 = BTN_H;
    int by2 = cy + ph/2 - M_PAD - bh2;

    if (draw_btn("EFFACER",  cx - bw2 - M_IN/2, by2, bw2, bh2, C_RED, 0)) {
        save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        m->screen = (m->back_screen == MENU_CAMPAIGN ||
                     m->back_screen == MENU_ARCADE)
                    ? m->back_screen : MENU_PLAY_HUB;
        set_msg(m, "Partie effacee.");
    }
    if (draw_btn("ANNULER",  cx + M_IN/2,         by2, bw2, bh2, C_TEXT, 0))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// MENU PAUSE
// ════════════════════════════════════════════════════
static MenuAction draw_pause(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,155});

    int pw = 260, ph = 340;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int iw = pw - M_PAD*2;
    int py = cy - ph/2 + M_PAD;

    txt_c("PAUSE", cx, py, 22, C_GOLD);
    py += 30;
    draw_sep(px, py, iw, C_BORDER);
    py += M_IN + 4;

    int bh2 = BTN_H, gap = M_IN - 2;

    if (draw_btn("REPRENDRE",     px, py, iw, bh2, C_GREEN, 0))
        { m->paused = 0; m->screen = MENU_TITLE; }
    py += bh2 + gap;

    if (draw_btn("SAUVEGARDER",   px, py, iw, bh2, C_GOLD, 0))
        act.save_and_quit = 2;
    py += bh2 + gap;

    if (draw_btn("OPTIONS",        px, py, iw, bh2, C_BLUE, 0))
        m->screen = MENU_OPTIONS;
    py += bh2 + gap;

    if (draw_btn("MENU PRINCIPAL", px, py, iw, bh2, C_DIM, 0))
        act.save_and_quit = 1;
    py += bh2 + gap;

    if (draw_btn("QUITTER",        px, py, iw, bh2, C_RED, 0))
        act.quit_app = 1;

    // Info résolution
    char sz[48];
    snprintf(sz, sizeof(sz), "%dx%d  %s",
             GetScreenWidth(), GetScreenHeight(),
             IsWindowFullscreen() ? "Plein ecran" : "Fenetre");
    txt_c(sz, cx, cy + ph/2 - M_PAD - 10, 9, C_DIM);

    return act;
}

// ════════════════════════════════════════════════════
// UPDATE
// ════════════════════════════════════════════════════
MenuAction menu_update(MenuState *m, const MetaProgress *meta) {
    MenuAction act = {0};
    if (m->msg_timer > 0.0f) m->msg_timer -= GetFrameTime();

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
// CARTE DU MONDE — progression campagne
// ════════════════════════════════════════════════════
static MenuAction draw_world_map(MenuState *m,
                                 const MetaProgress *meta,
                                 int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;

    draw_bg(m, vw, vh);
    draw_header("PROGRESSION DE CAMPAGNE", vw);

    // Sous-titre (décalé sous l'en-tête pour éviter la superposition)
    txt_c_boxed("Completez chaque acte pour progresser vers la victoire.",
                cx, M_PAD + 62, 10, C_TEXT);

    static const Color CHAPTER_COLS[CAMPAIGN_CHAPTERS] = {
        {200,150, 80,255},  // Wasteland — ocre
        { 60,180, 80,255},  // Swamp     — vert
        {220,180, 80,255},  // Desert    — jaune
        {100,140,200,255},  // City      — bleu
        {180, 80, 80,255},  // Factory   — rouge
    };

    static const char *CHAPTER_NAMES[CAMPAIGN_CHAPTERS] = {
        "Les Terres Brulees",
        "Le Marais Toxique",
        "Le Desert Irradie",
        "La Ville en Ruine",
        "L'Usine Abandonnee",
    };

    int chapter_w = vw - M_PAD * 4;
    int chapter_h = 58;
    int chapter_x = M_PAD * 2;
    int chapter_y = M_PAD + 82;
    int gap       = M_IN;

    for (int ch = 0; ch < CAMPAIGN_CHAPTERS; ch++) {
        Color col = CHAPTER_COLS[ch];

        // Fond du chapitre
        Rectangle cr = {(float)chapter_x, (float)chapter_y,
                        (float)chapter_w, (float)chapter_h};
        int ch_unlocked = meta_act_unlocked(meta, ch * CAMPAIGN_ACTS);
        Color bg = ch_unlocked ? (Color){col.r/8, col.g/8, col.b/8, 255}
                               : (Color){10, 8, 5, 255};
        DrawRectangleRounded(cr, 4.0f/chapter_h, 5, bg);
        DrawRectangleRoundedLinesEx(cr, 4.0f/chapter_h, 5, 1.5f,
            ch_unlocked ? (Color){col.r/3, col.g/3, col.b/3, 255}
                        : (Color){40,30,12,255});

        // Numéro + nom chapitre
        dtxt(TextFormat("CH.%d", ch+1),
                 chapter_x + M_IN, chapter_y + M_IN, 10, C_DIM);
        dtxt(CHAPTER_NAMES[ch],
                 chapter_x + M_IN + 34, chapter_y + M_IN, 14,
                 ch_unlocked ? col : C_DIM);

        // 3 actes
        int act_w = (chapter_w - M_IN * 4) / CAMPAIGN_ACTS;
        for (int a = 0; a < CAMPAIGN_ACTS; a++) {
            int stage_idx = ch * CAMPAIGN_ACTS + a;
            const ActData *ad = campaign_act_get(stage_idx);
            int stars    = meta->act_stars[stage_idx];
            int unlocked = meta_act_unlocked(meta, stage_idx);

            int ax = chapter_x + M_IN + a * (act_w + M_IN);
            int ay = chapter_y + 26;
            int ah = chapter_h - 30;

            Rectangle ar = {(float)ax, (float)ay, (float)act_w, (float)ah};
            Color abg = unlocked ? (stars > 0 ? (Color){col.r/6,col.g/6,col.b/6,255}
                                              : (Color){18,12,4,255})
                                 : (Color){8,6,3,255};
            DrawRectangleRounded(ar, 3.0f/ah, 4, abg);
            DrawRectangleRoundedLinesEx(ar, 3.0f/ah, 4, 1.0f,
                unlocked ? (stars > 0 ? col : (Color){60,45,18,255})
                         : (Color){30,24,10,255});

            // Titre acte clippé
            char abuf[32];
            clip_text(ad->title, act_w - 6, 9, abuf, sizeof(abuf));
            dtxt(abuf, ax+3, ay+2, 9,
                     unlocked ? (stars>0 ? col : C_TEXT) : C_DIM);

            // Étoiles
            for (int s = 0; s < 2; s++) {
                Color sc = (s < stars) ? (Color){232,200,32,255}
                                       : (Color){40,32,12,255};
                dtxt("*", ax + 3 + s*12, ay + ah - 14, 12, sc);
            }

            // Verrou si non débloqué
            if (!unlocked)
                dtxt("[x]", ax + act_w/2 - 8, ay + ah/2 - 5, 10, C_DIM);
        }

        chapter_y += chapter_h + gap;
    }

    // Bouton JOUER — lance une nouvelle campagne ou reprend
    {
        int bw = 200, bh = BTN_H;
        int bx = cx - bw/2;
        int by = vh - M_PAD * 2 - bh;
        if (draw_btn("JOUER UNE CAMPAGNE", bx, by, bw, bh, C_GOLD, 0)) {
            m->screen = MENU_CAMPAIGN;
            m->back_screen = MENU_WORLD_MAP;
        }
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
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

    if (m->paused && m->screen == MENU_PAUSE) {
        act = draw_pause(m, vw, vh);
        return act;
    }
    if (m->paused && m->screen == MENU_OPTIONS) {
        act = draw_options(m, vw, vh);
        return act;
    }

    switch (m->screen) {
        case MENU_TITLE:        act = draw_title(m, vw, vh);              break;
        case MENU_PLAY_HUB:     act = draw_play_hub(m, meta, vw, vh);     break;
        case MENU_CAMPAIGN:     act = draw_slot_list(m, vw, vh, 1);       break;
        case MENU_ARCADE:       act = draw_slot_list(m, vw, vh, 0);       break;
        case MENU_NEW_CAMPAIGN: act = draw_new_campaign(m, meta, vw, vh); break;
        case MENU_WORLD_MAP:    act = draw_world_map(m, meta, vw, vh);    break;
        case MENU_NEW_ARCADE:
            m->new_theme = (m->new_theme == (ThemeID)-1) ? THEME_COUNT : m->new_theme;
            act = draw_new_arcade(m, vw, vh);
            break;
        case MENU_UPGRADES:     act = draw_upgrades(m, meta, vw, vh);     break;
        case MENU_OPTIONS:      act = draw_options(m, vw, vh);            break;
        case MENU_CONFIRM_DEL:
            draw_slot_list(m, vw, vh, m->back_screen == MENU_CAMPAIGN);
            act = draw_confirm_del(m, vw, vh);
            break;
        default:
            m->screen = MENU_TITLE;
            break;
    }

    draw_msg(m, vw, vh);
    return act;
}

void menu_render(const MenuState *m_const, const MetaProgress *meta,
                 int vw, int vh)
{
    menu_render_and_act((MenuState*)m_const, meta, vw, vh);
}
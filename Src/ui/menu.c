#include "menu.h"
#include "renderer.h"
#include "../meta/meta.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ════════════════════════════════════════════════════
// PALETTE
// ════════════════════════════════════════════════════
#define C_BG      ((Color){  8,   5,   3, 255})
#define C_PANEL   ((Color){ 14,   9,   4, 255})
#define C_BORDER  ((Color){ 61,  42,  16, 255})
#define C_GOLD    ((Color){239, 159,  39, 255})
#define C_GREEN   ((Color){ 46, 204, 113, 255})
#define C_RED     ((Color){231,  76,  60, 255})
#define C_DIM     ((Color){ 80,  70,  50, 255})
#define C_TEXT    ((Color){180, 160, 110, 255})
#define C_HOV     ((Color){ 30,  20,   6, 255})
#define C_BLUE    ((Color){ 52, 152, 219, 255})
#define C_DARK    ((Color){ 10,   6,   2, 255})
#define C_ORANGE  ((Color){230, 126,  34, 255})

// ════════════════════════════════════════════════════
// SOURIS VIRTUELLE
// ════════════════════════════════════════════════════
static float g_ox = 0.0f, g_oy = 0.0f, g_sc = 1.0f;

void menu_set_mouse_offset(float ox, float oy, float scale) {
    g_ox = ox; g_oy = oy;
    g_sc = scale > 0.001f ? scale : 1.0f;
}

static Vector2 vmouse(void) {
    Vector2 r = GetMousePosition();
    return (Vector2){(r.x - g_ox) / g_sc, (r.y - g_oy) / g_sc};
}

static int vhov(int x, int y, int w, int h) {
    Vector2 m = vmouse();
    return m.x >= x && m.x < x+w && m.y >= y && m.y < y+h;
}

static int vclick(int x, int y, int w, int h) {
    return vhov(x,y,w,h) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ════════════════════════════════════════════════════
// HELPERS DE DESSIN
// ════════════════════════════════════════════════════
static void txt_c(const char *s, int cx, int y, int fs, Color col) {
    DrawText(s, cx - MeasureText(s, fs)/2, y, fs, col);
}

static void draw_sep(int x, int y, int w, Color col) {
    DrawLine(x, y, x+w, y, col);
}

// Bouton rectangle — retourne 1 si cliqué
static int draw_btn(const char *label, int x, int y, int w, int h,
                    Color col, int active)
{
    int hovered = vhov(x, y, w, h);
    Color bg  = active  ? (Color){col.r/4, col.g/4, col.b/4, 255} :
                hovered ? C_HOV : C_PANEL;
    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLinesEx((Rectangle){(float)x,(float)y,(float)w,(float)h},
                         (active||hovered) ? 2.0f : 1.0f,
                         (active||hovered) ? col : C_BORDER);
    int fs = 15;
    txt_c(label, x + w/2, y + h/2 - fs/2, fs, col);
    return vclick(x, y, w, h);
}

// Grand bouton de navigation (avec icône + description)
static int draw_nav_btn(const char *icon, const char *title,
                        const char *desc, Color col,
                        int x, int y, int w, int h)
{
    int hovered = vhov(x, y, w, h);
    Color bg = hovered ? (Color){col.r/6, col.g/6, col.b/6, 255} : C_PANEL;
    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLinesEx((Rectangle){(float)x,(float)y,(float)w,(float)h},
                         hovered ? 2.5f : 1.5f,
                         hovered ? col : (Color){col.r/2,col.g/2,col.b/2,200});

    // Barre de couleur à gauche
    DrawRectangle(x, y, 4, h, (Color){col.r, col.g, col.b, hovered ? 255 : 120});

    // Icône
    DrawText(icon, x + 20, y + h/2 - 14, 28, col);

    // Titre
    DrawText(title, x + 64, y + 10, 18, hovered ? col : C_TEXT);

    // Description
    DrawText(desc,  x + 64, y + 34, 11, C_DIM);

    return vclick(x, y, w, h);
}

// Fond décoratif du canvas
static void draw_bg(int virt_w, int virt_h) {
    ClearBackground(C_BG);
    // Lignes de grille légères
    for (int x = 0; x < virt_w; x += 80)
        DrawLine(x, 0, x, virt_h, (Color){20, 14, 6, 60});
    for (int y = 0; y < virt_h; y += 80)
        DrawLine(0, y, virt_w, y, (Color){20, 14, 6, 60});
}

// En-tête commun (titre + sous-titre)
static void draw_header(const char *title, const char *subtitle,
                        int virt_w) {
    int cx = virt_w / 2;
    txt_c("RUST BASTION", cx, 18, 30, C_GOLD);
    draw_sep(40, 56, virt_w - 80, C_BORDER);
    if (title && title[0])
        txt_c(title, cx, 66, 16, C_TEXT);
    if (subtitle && subtitle[0])
        txt_c(subtitle, cx, 88, 11, C_DIM);
}

// Panneau centré avec fond et bordure
static void draw_panel(int cx, int cy, int pw, int ph, Color border) {
    DrawRectangle(cx - pw/2, cy - ph/2, pw, ph, (Color){10, 6, 2, 248});
    DrawRectangleLinesEx(
        (Rectangle){(float)(cx-pw/2),(float)(cy-ph/2),(float)pw,(float)ph},
        2.0f, border);
}

// Bouton Retour en bas à gauche
static int draw_back_btn(int virt_h) {
    return draw_btn("< RETOUR", 40, virt_h - 52, 120, 30, C_DIM, 0);
}

// Message temporaire
static void draw_msg(MenuState *m, int virt_w, int virt_h) {
    if (m->msg_timer <= 0.0f) return;
    int alpha = (int)(fminf(m->msg_timer / 0.5f, 1.0f) * 220.0f);
    txt_c(m->msg_buf, virt_w/2, virt_h - 28, 13,
          (Color){239,159,39,(unsigned char)alpha});
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
    m->screen     = MENU_TITLE;
    m->back_screen= MENU_TITLE;
    m->new_theme  = THEME_COUNT;
    m->new_slot   = 0;
    m->paused     = 0;
    if (opts) m->opts = *opts;
    menu_refresh_slots(m);
}

void menu_refresh_slots(MenuState *m) {
    save_scan(m->slots);
}

// ════════════════════════════════════════════════════
// ÉCRAN TITRE  — Jouer / Options / Quitter
// ════════════════════════════════════════════════════
static MenuAction draw_title(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;

    draw_bg(virt_w, virt_h);

    // Titre principal
    txt_c("RUST BASTION", cx, virt_h/2 - 160, 48, C_GOLD);
    txt_c("Tower Defense Post-Apocalyptique", cx, virt_h/2 - 104, 14, C_DIM);
    draw_sep(cx - 200, virt_h/2 - 82, 400, C_BORDER);

    // 3 boutons verticaux centrés
    int bw = 260, bh = 48, gap = 16;
    int bx = cx - bw/2;
    int by = virt_h/2 - 48;

    if (draw_btn("JOUER",   bx, by,          bw, bh, C_GREEN, 0))
        { m->screen = MENU_PLAY_HUB; m->back_screen = MENU_TITLE; }
    by += bh + gap;

    if (draw_btn("OPTIONS", bx, by,          bw, bh, C_BLUE, 0))
        { m->back_screen = MENU_TITLE; m->screen = MENU_OPTIONS; }
    by += bh + gap;

    if (draw_btn("QUITTER", bx, by,          bw, bh, C_RED, 0))
        act.quit_app = 1;

    // Version
    DrawText("v0.1", virt_w - 52, virt_h - 20, 10, C_DIM);

    return act;
}

// ════════════════════════════════════════════════════
// HUB JOUER — Campagne / Arcade / Améliorations / Retour
// ════════════════════════════════════════════════════
static MenuAction draw_play_hub(MenuState *m, const MetaProgress *meta,
                                 int virt_w, int virt_h)
{
    MenuAction act = {0};
    int cx  = virt_w / 2;

    draw_bg(virt_w, virt_h);
    draw_header("CHOISIR UN MODE", NULL, virt_w);

    // 3 grands boutons de navigation
    int bw = 560, bh = 80, gap = 18;
    int bx = cx - bw/2;
    int by = 118;

    // ── CAMPAGNE ─────────────────────────────────────────
    if (draw_nav_btn("C", "CAMPAGNE",
                     "Parcourez les 5 environnements dans l'ordre. Gagnez de la ferraille.",
                     C_GOLD, bx, by, bw, bh)) {
        m->screen      = MENU_CAMPAIGN;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    // ── ARCADE ───────────────────────────────────────────
    if (draw_nav_btn("A", "ARCADE",
                     "Choisissez un environnement et jouez librement.",
                     C_BLUE, bx, by, bw, bh)) {
        m->screen      = MENU_ARCADE;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    // ── AMÉLIORATIONS ────────────────────────────────────
    // Indique la ferraille disponible
    char upg_desc[80];
    snprintf(upg_desc, sizeof(upg_desc),
             "Depensez vos %d ferrailles pour ameliorer vos defenses.",
             meta->scrap);
    if (draw_nav_btn("*", "AMELIORATIONS", upg_desc,
                     C_ORANGE, bx, by, bw, bh)) {
        m->screen      = MENU_UPGRADES;
        m->back_screen = MENU_PLAY_HUB;
    }

    if (draw_back_btn(virt_h)) m->screen = MENU_TITLE;

    return act;
}

// ════════════════════════════════════════════════════
// HELPER COMMUN : liste de slots (campagne ou arcade)
// ════════════════════════════════════════════════════
static MenuAction draw_slot_list(MenuState *m, int virt_w, int virt_h,
                                  int is_campaign)
{
    MenuAction act = {0};
    int cx = virt_w / 2;

    draw_bg(virt_w, virt_h);
    draw_header(is_campaign ? "CAMPAGNE" : "ARCADE",
                is_campaign
                  ? "5 environnements en sequence — la ferraille se gagne ici"
                  : "Mode libre — choisissez votre terrain",
                virt_w);

    int sw  = 560, sh = 72, sg = 10;
    int sx  = cx - sw/2;
    int y   = 112;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        const SaveInfo *si = &m->slots[i];

        // Filtre : n'affiche que les slots du bon mode (ou vides)
        int slot_matches = !si->exists ||
                           ((int)si->mode == (is_campaign
                                              ? SAVE_MODE_CAMPAIGN
                                              : SAVE_MODE_ARCADE));

        int hov = vhov(sx, y, sw, sh);
        Color brd = si->exists && slot_matches ? (is_campaign ? C_GOLD : C_BLUE)
                                               : C_BORDER;
        DrawRectangle(sx, y, sw, sh, hov ? C_HOV : C_PANEL);
        DrawRectangleLinesEx(
            (Rectangle){(float)sx,(float)y,(float)sw,(float)sh},
            1.5f, brd);

        if (si->exists && slot_matches) {
            // Slot occupé du bon type
            if (is_campaign) {
                int themes[CAMPAIGN_STAGES];
                meta_campaign_theme_order(si->campaign_num, themes);
                const Theme *stage_th = theme_get((ThemeID)themes[si->campaign_stage]);
                DrawText(TextFormat("CAMPAGNE %d  —  Stage %d/%d : %s",
                             si->campaign_num + 1,
                             si->campaign_stage + 1, CAMPAIGN_STAGES,
                             stage_th->name),
                         sx+12, y+8, 13, C_GOLD);
            } else {
                DrawText(TextFormat("ARCADE  —  %s", si->theme_name),
                         sx+12, y+8, 13, C_BLUE);
            }
            DrawText(TextFormat("Vague %d  |  %d vies  |  %dor",
                         si->wave, si->lives, si->gold),
                     sx+12, y+26, 11, C_TEXT);
            DrawText(TextFormat("Kills : %d", si->wave * 4),
                     sx+12, y+44, 10, C_DIM);

            // Bouton Reprendre
            int bx = sx + sw - 108, bw = 88, bh = 28;
            int by2 = y + (sh - bh)/2;
            if (draw_btn("REPRENDRE", bx, by2, bw, bh, C_GREEN, 0)) {
                act.resume_slot = i;
                act.go_game     = 1;
            }

            // Bouton Effacer
            int dx = sx + sw - 24, dw = 18, dh = 18, dy = y + 4;
            DrawRectangle(dx, dy, dw, dh, (Color){40,8,8,255});
            DrawRectangleLinesEx((Rectangle){(float)dx,(float)dy,(float)dw,(float)dh},
                                 1.0f, C_RED);
            DrawText("X", dx+4, dy+2, 11, C_RED);
            if (vclick(dx, dy, dw, dh)) {
                m->confirm_del_slot = i;
                m->screen = MENU_CONFIRM_DEL;
            }

        } else if (!si->exists) {
            // Slot vide — proposer de commencer
            DrawText(TextFormat("Emplacement %d — vide", i+1),
                     sx+12, y+26, 12, C_DIM);
            int bx = sx + sw - 168, bw = 148, bh = 28;
            int by2 = y + (sh - bh)/2;
            const char *lbl = is_campaign ? "NOUVELLE CAMPAGNE"
                                          : "NOUVELLE ARCADE";
            Color lc = is_campaign ? C_GOLD : C_BLUE;
            if (draw_btn(lbl, bx, by2, bw, bh, lc, 0)) {
                m->new_slot = i;
                m->screen   = is_campaign ? MENU_NEW_CAMPAIGN
                                          : MENU_NEW_ARCADE;
            }
        } else {
            // Slot d'un autre mode — affiché grisé
            DrawText(TextFormat("Emplacement %d — autre mode", i+1),
                     sx+12, y+26, 12, C_DIM);
        }

        y += sh + sg;
    }

    if (draw_back_btn(virt_h))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// NOUVEAU — CAMPAGNE : confirmation + affichage de l'ordre
// ════════════════════════════════════════════════════
static MenuAction draw_new_campaign(MenuState *m,
                                     const MetaProgress *meta,
                                     int virt_w, int virt_h)
{
    MenuAction act = {0};
    int cx = virt_w / 2, cy = virt_h / 2;
    (void)meta;

    draw_bg(virt_w, virt_h);
    draw_header("NOUVELLE CAMPAGNE", NULL, virt_w);

    // Numéro de campagne = campaigns_completed du meta
    int camp_num = meta->campaigns_completed;
    int themes[CAMPAIGN_STAGES];
    meta_campaign_theme_order(camp_num, themes);

    // Panneau central
    int pw = 500, ph = 320;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    DrawText(TextFormat("Emplacement : %d", m->new_slot + 1),
             cx - pw/2 + 20, cy - ph/2 + 16, 13, C_TEXT);
    DrawText(TextFormat("Campagne n°%d", camp_num + 1),
             cx - pw/2 + 20, cy - ph/2 + 36, 18, C_GOLD);

    DrawText("Ordre des environnements :",
             cx - pw/2 + 20, cy - ph/2 + 68, 12, C_DIM);

    static Color stage_cols[CAMPAIGN_STAGES] = {
        {239,159, 39,255},{46,204,113,255},{52,152,219,255},
        {155, 89,182,255},{230,126, 34,255},
    };

    for (int i = 0; i < CAMPAIGN_STAGES; i++) {
        const Theme *th = theme_get((ThemeID)themes[i]);
        int iy = cy - ph/2 + 90 + i * 36;
        DrawRectangle(cx - pw/2 + 20, iy, pw - 40, 30,
                      (Color){20,12,4,200});
        DrawRectangleLinesEx(
            (Rectangle){(float)(cx-pw/2+20),(float)iy,(float)(pw-40),30},
            1.0f, (Color){stage_cols[i].r/2,stage_cols[i].g/2,stage_cols[i].b/2,200});
        DrawText(TextFormat("%d.", i+1),
                 cx - pw/2 + 30, iy + 7, 13, C_DIM);
        DrawText(th->name,
                 cx - pw/2 + 56, iy + 7, 13, stage_cols[i]);
        DrawText(th->description,
                 cx + 20,        iy + 9, 10, C_DIM);
    }

    // Boutons
    int by = cy + ph/2 - 40;
    if (draw_btn("LANCER",  cx - 80, by, 130, 32, C_GREEN, 0)) {
        act.start_campaign = 1;
        act.new_slot       = m->new_slot;
    }
    if (draw_btn("ANNULER", cx + 80, by, 130, 32, C_DIM, 0))
        m->screen = MENU_CAMPAIGN;

    return act;
}

// ════════════════════════════════════════════════════
// NOUVEAU — ARCADE : choix du thème
// ════════════════════════════════════════════════════
static const char *THEME_LABELS[THEME_COUNT + 1] = {
    "Terres devastees","Marais toxique","Desert irradie",
    "Ville en ruine","Usine abandonnee","Aleatoire",
};

static MenuAction draw_new_arcade(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2;

    draw_bg(virt_w, virt_h);
    draw_header("NOUVELLE PARTIE ARCADE",
                TextFormat("Emplacement %d", m->new_slot + 1), virt_w);

    DrawText("Choisir un environnement :", cx - 160, 110, 13, C_GOLD);

    int bx = cx - 160, bw = 320, bh = 30, by = 132;
    for (int i = 0; i <= THEME_COUNT; i++) {
        int is_sel = ((int)m->new_theme == i);
        Color c = is_sel ? C_BLUE : C_TEXT;
        DrawRectangle(bx, by, bw, bh,
                      is_sel ? (Color){8,20,36,255} : C_PANEL);
        DrawRectangleLinesEx(
            (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
            is_sel ? 2.0f : 1.0f,
            is_sel ? C_BLUE : C_BORDER);
        DrawText(THEME_LABELS[i], bx + 10, by + 8, 12, c);
        if (is_sel) DrawText("◀", bx + bw - 20, by + 8, 12, C_BLUE);
        if (vclick(bx, by, bw, bh)) m->new_theme = (ThemeID)i;
        by += bh + 6;
    }

    by += 8;
    if (draw_btn("LANCER",  cx - 80, by + 18, 130, 32, C_GREEN, 0)) {
        act.start_arcade = 1;
        act.new_theme    = m->new_theme;
        act.new_slot     = m->new_slot;
    }
    if (draw_btn("ANNULER", cx + 80, by + 18, 130, 32, C_DIM, 0))
        m->screen = MENU_ARCADE;

    return act;
}

// ════════════════════════════════════════════════════
// AMÉLIORATIONS
// ════════════════════════════════════════════════════
static MenuAction draw_upgrades(MenuState *m, const MetaProgress *meta,
                                 int virt_w, int virt_h)
{
    MenuAction act = {0};
    int cx = virt_w / 2;

    draw_bg(virt_w, virt_h);
    draw_header("AMELIORATIONS",
                "La ferraille se gagne uniquement en completant des stages de campagne.",
                virt_w);

    // Ferraille disponible — bien visible
    int fx = cx - 120, fy = 102;
    DrawRectangle(fx, fy, 240, 28, (Color){8, 24, 8, 255});
    DrawRectangleLinesEx((Rectangle){(float)fx,(float)fy,240,28},1.5f,C_GREEN);
    txt_c(TextFormat("Ferraille : %d", meta->scrap), cx, fy + 7, 14, C_GREEN);

    int lx  = 80;
    int rw  = virt_w - 160;
    int y   = 142;

    DrawText(TextFormat("Campagnes terminees : %d   Meilleures vague : %d",
                 meta->campaigns_completed, meta->best_wave),
             lx, y, 10, C_DIM); y += 16;
    draw_sep(lx, y, rw, C_BORDER); y += 10;

    for (int i = 0; i < UPGRADE_COUNT; i++) {
        int is_h = vhov(lx, y, rw, 30);
        if (is_h) m->sel_upg = i;
        int is_sel = (m->sel_upg == i);

        DrawRectangle(lx, y, rw, 30,
                      is_sel ? (Color){24,14,4,255} : (Color){0,0,0,0});

        // Nom
        DrawText(UPGRADE_NAMES[i], lx + 10, y + 8, 13,
                 is_sel ? C_GOLD : C_TEXT);

        // Étoiles
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
        // Barre de progression étoiles
        int sx2 = cx - 30;
        for (int s = 0; s < maxlvl; s++) {
            Color sc = (s < lvl) ? C_GOLD : (Color){40,30,10,255};
            DrawRectangle(sx2 + s * 16, y + 10, 12, 10, sc);
            DrawRectangleLines(sx2 + s * 16, y + 10, 12, 10,
                               (Color){80,60,20,180});
        }

        // Description
        DrawText(UPGRADE_DESC[i], cx + 60, y + 10, 10, C_DIM);

        // Coût / MAX
        int cost = meta_upgrade_cost(meta, i);
        if (cost > 0) {
            int can = meta->scrap >= cost;
            DrawText(TextFormat("%d ", cost), lx + rw - 86, y + 8, 11,
                     can ? C_GOLD : C_RED);
            DrawText("ferr.", lx + rw - 50, y + 8, 11, C_DIM);
            if (is_h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (meta_upgrade((MetaProgress*)meta, i))
                    set_msg(m, "Amelioration achetee !");
                else
                    set_msg(m, "Ferraille insuffisante.");
            }
        } else {
            DrawText("MAX", lx + rw - 46, y + 8, 11, C_GREEN);
        }

        draw_sep(lx, y+30, rw, (Color){30,20,8,160});
        y += 32;
    }

    draw_msg(m, virt_w, virt_h);

    if (draw_back_btn(virt_h))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// OPTIONS
// ════════════════════════════════════════════════════
static MenuAction draw_options(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w / 2, cy = virt_h / 2;

    if (m->paused)
        DrawRectangle(0, 0, virt_w, virt_h, (Color){0,0,0,160});
    else
        draw_bg(virt_w, virt_h);

    int pw = 380, ph = 390;
    draw_panel(cx, cy, pw, ph, C_BORDER);
    txt_c("OPTIONS", cx, cy - ph/2 + 16, 22, C_GOLD);
    draw_sep(cx - pw/2 + 24, cy - ph/2 + 46, pw - 48, C_BORDER);

    int y = cy - ph/2 + 62;

    // Plein écran
    txt_c("AFFICHAGE", cx, y, 12, C_DIM); y += 20;
    const char *fsl = m->opts.fullscreen ? "[*] PLEIN ECRAN" : "[ ] PLEIN ECRAN";
    int bw = pw - 48, bx = cx - bw/2;
    if (draw_btn(fsl, bx, y, bw, 30, C_BLUE, m->opts.fullscreen)) {
        m->opts.fullscreen ^= 1;
        act.toggle_fs = 1;
    }
    y += 40;

    // Résolutions
    txt_c("RESOLUTION", cx, y, 12, C_DIM); y += 20;
    static const int RES[][2] = {
        {1120,770},{1400,962},{1680,1154},{1960,1346},{2240,1540}
    };
    static const char *RLBL[] = {
        "1120x770  (1x)","1400x962  (1.25x)","1680x1154 (1.5x)",
        "1960x1346 (1.75x)","2240x1540 (2x)",
    };
    for (int i = 0; i < 5; i++) {
        int cur = (m->opts.win_width == RES[i][0]);
        Color c = cur ? C_GREEN : C_TEXT;
        if (draw_btn(RLBL[i], bx, y, bw, 26, c, cur) && !cur) {
            m->opts.win_width  = RES[i][0];
            m->opts.win_height = RES[i][1];
            act.toggle_fs = 2;
        }
        y += 28;
    }

    char info[48];
    snprintf(info, sizeof(info), "Actuelle : %dx%d",
             GetScreenWidth(), GetScreenHeight());
    txt_c(info, cx, cy + ph/2 - 52, 10, C_DIM);

    if (draw_btn("RETOUR", cx, cy + ph/2 - 24, 120, 26, C_DIM, 0))
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// CONFIRMATION SUPPRESSION
// ════════════════════════════════════════════════════
static MenuAction draw_confirm_del(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w/2, cy = virt_h/2;

    DrawRectangle(0, 0, virt_w, virt_h, (Color){0,0,0,140});

    int pw = 380, ph = 130;
    draw_panel(cx, cy, pw, ph, C_RED);
    txt_c(TextFormat("Effacer la partie %d ?", m->confirm_del_slot+1),
          cx, cy - ph/2 + 18, 15, C_RED);
    txt_c("Cette action est irreversible.", cx, cy - ph/2 + 40, 11, C_DIM);

    if (draw_btn("EFFACER", cx - 70, cy + 26, 110, 28, C_RED, 0)) {
        save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        // Retour à l'écran de liste approprié
        m->screen = (m->back_screen == MENU_CAMPAIGN ||
                     m->back_screen == MENU_ARCADE)
                    ? m->back_screen : MENU_PLAY_HUB;
        set_msg(m, "Partie effacee.");
    }
    if (draw_btn("ANNULER", cx + 70, cy + 26, 110, 28, C_TEXT, 0))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// MENU PAUSE
// ════════════════════════════════════════════════════
static MenuAction draw_pause(MenuState *m, int virt_w, int virt_h) {
    MenuAction act = {0};
    int cx = virt_w/2, cy = virt_h/2;

    DrawRectangle(0, 0, virt_w, virt_h, (Color){0,0,0,160});

    int pw = 300, ph = 340;
    draw_panel(cx, cy, pw, ph, C_GOLD);
    txt_c("PAUSE", cx, cy - ph/2 + 18, 26, C_GOLD);
    draw_sep(cx - pw/2 + 32, cy - ph/2 + 52, pw - 64, C_BORDER);

    int bw = 220, bh = 36, gap = 44;
    int by = cy - ph/2 + 68;

    if (draw_btn("REPRENDRE",     cx, by, bw, bh, C_GREEN, 0))
        { m->paused = 0; m->screen = MENU_TITLE; }
    by += gap;

    if (draw_btn("SAUVEGARDER",   cx, by, bw, bh, C_GOLD, 0))
        act.save_and_quit = 2;
    by += gap;

    if (draw_btn("OPTIONS",        cx, by, bw, bh, C_BLUE, 0))
        m->screen = MENU_OPTIONS;
    by += gap;

    if (draw_btn("MENU PRINCIPAL", cx, by, bw, bh, C_DIM, 0))
        act.save_and_quit = 1;
    by += gap;

    if (draw_btn("QUITTER",        cx, by, bw, bh, C_RED, 0))
        act.quit_app = 1;

    char sz[48];
    snprintf(sz, sizeof(sz), "%dx%d  %s",
             GetScreenWidth(), GetScreenHeight(),
             IsWindowFullscreen() ? "Plein ecran" : "Fenetre");
    txt_c(sz, cx, cy + ph/2 - 18, 10, C_DIM);

    return act;
}

// ════════════════════════════════════════════════════
// UPDATE
// ════════════════════════════════════════════════════
MenuAction menu_update(MenuState *m, const MetaProgress *meta) {
    (void)meta;
    MenuAction act = {0};
    if (m->msg_timer > 0.0f) m->msg_timer -= GetFrameTime();

    // Navigation clavier dans les améliorations
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
                                int virt_w, int virt_h)
{
    MenuAction act = {0};

    // ── Overlays (toujours en premier) ────────────────────────
    if (m->paused && m->screen == MENU_PAUSE) {
        act = draw_pause(m, virt_w, virt_h);
        return act;
    }
    if (m->paused && m->screen == MENU_OPTIONS) {
        act = draw_options(m, virt_w, virt_h);
        return act;
    }

    // ── Écrans principaux ─────────────────────────────────────
    switch (m->screen) {
        case MENU_TITLE:
            act = draw_title(m, virt_w, virt_h);
            break;

        case MENU_PLAY_HUB:
            act = draw_play_hub(m, meta, virt_w, virt_h);
            break;

        case MENU_CAMPAIGN:
            act = draw_slot_list(m, virt_w, virt_h, 1);
            break;

        case MENU_ARCADE:
            act = draw_slot_list(m, virt_w, virt_h, 0);
            break;

        case MENU_NEW_CAMPAIGN:
            act = draw_new_campaign(m, meta, virt_w, virt_h);
            break;

        case MENU_NEW_ARCADE:
            m->new_theme = (m->new_theme == (ThemeID)-1) ? THEME_COUNT : m->new_theme;
            act = draw_new_arcade(m, virt_w, virt_h);
            break;

        case MENU_UPGRADES:
            act = draw_upgrades(m, meta, virt_w, virt_h);
            break;

        case MENU_OPTIONS:
            act = draw_options(m, virt_w, virt_h);
            break;

        case MENU_CONFIRM_DEL:
            // Dessine l'écran précédent en arrière-plan
            draw_slot_list(m, virt_w, virt_h,
                           m->back_screen == MENU_CAMPAIGN);
            act = draw_confirm_del(m, virt_w, virt_h);
            break;

        default:
            m->screen = MENU_TITLE;
            break;
    }

    draw_msg(m, virt_w, virt_h);
    return act;
}

void menu_render(const MenuState *m_const, const MetaProgress *meta,
                 int virt_w, int virt_h)
{
    menu_render_and_act((MenuState*)m_const, meta, virt_w, virt_h);
}
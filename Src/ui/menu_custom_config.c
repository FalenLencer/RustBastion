/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_custom_config.c ─ Écran de configuration du mode personnalisé.
 *
 *  Contient :
 *    draw_custom_config  — Sélection terrain, spawns, bases, distance,
 *                          puissance et croissance des ennemis.
 */

#include "menu_internal.h"
#include "../map/theme.h"

// ─── Helpers locaux ─────────────────────────────────────────────

/* Rangée de boutons radio horizontale.
   Retourne 1 si une valeur a changé, 0 sinon. */
static int radio_row(const char **labels, int n,
                     int x, int y, int btn_w, int btn_h,
                     Color sel_col, int *sel)
{
    int changed = 0;
    int gap = 6;
    for (int i = 0; i < n; i++) {
        int bx  = x + i * (btn_w + gap);
        int active = (*sel == i);
        Rectangle r = {(float)bx, (float)y, (float)btn_w, (float)btn_h};
        int hov = vhov_r(r);
        float rnd = (float)BTN_R / btn_h;

        Color bg  = active  ? (Color){sel_col.r/4, sel_col.g/4, sel_col.b/4, 255}
                  : hov     ? (Color){sel_col.r/8, sel_col.g/8, sel_col.b/8, 255}
                  : C_PANEL;
        float bw2 = active ? 2.2f : hov ? 1.8f : 1.0f;
        Color bc  = active ? sel_col : hov ? (Color){sel_col.r/2,sel_col.g/2,sel_col.b/2,255}
                                            : C_BORDER;
        DrawRectangleRounded(r, rnd, 5, bg);
        DrawRectangleRoundedLinesEx(r, rnd, 5, bw2, bc);

        Color tc = active ? sel_col : hov ? C_TEXT : C_DIM;
        int tw = mtxt(labels[i], 10);
        dtxt(labels[i], bx + btn_w/2 - tw/2, y + btn_h/2 - fh(10)/2, 10, tc);

        if (vclick_r(r)) {
            audio_play_sfx(AUDIO_SFX_MENU_CLICK);
            *sel = i;
            changed = 1;
        }
    }
    return changed;
}

/* Contrôle numérique : [−10] [−] [ N ] [+] [+10]
   Les boutons sont grisés et inactifs quand la valeur est déjà à la limite.
   Retourne 1 si la valeur a changé. */
static int draw_stepper(int *val, int min_v, int max_v,
                        int x, int y, int total_w, int h, Color accent)
{
    const int BIG = 42;   // largeur boutons ×10
    const int SML = 30;   // largeur boutons ×1
    const int GS  =  5;   // gap entre boutons

    int val_w = total_w - 2*(BIG + GS) - 2*(SML + GS);
    if (val_w < 40) val_w = 40;

    int xp = x;
    Rectangle r_m10 = {(float)xp, (float)y, BIG, h}; xp += BIG + GS;
    Rectangle r_m1  = {(float)xp, (float)y, SML, h}; xp += SML + GS;
    Rectangle r_val = {(float)xp, (float)y, val_w, h}; xp += val_w + GS;
    Rectangle r_p1  = {(float)xp, (float)y, SML, h}; xp += SML + GS;
    Rectangle r_p10 = {(float)xp, (float)y, BIG, h};

    float rnd = (float)BTN_R / h;
    int changed = 0;

    int at_min = (*val <= min_v);
    int at_max = (*val >= max_v);

    /* Clics — ignorés si déjà à la limite correspondante */
    if (!at_min && vclick_r(r_m10)) { *val -= 10; if (*val < min_v) *val = min_v; changed = 1; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
    if (!at_min && vclick_r(r_m1))  { *val -=  1; if (*val < min_v) *val = min_v; changed = 1; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
    if (!at_max && vclick_r(r_p1))  { *val +=  1; if (*val > max_v) *val = max_v; changed = 1; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
    if (!at_max && vclick_r(r_p10)) { *val += 10; if (*val > max_v) *val = max_v; changed = 1; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }

    /* Rendu d'un bouton (dis_ = 1 → grisé et non interactif) */
    #define STEP_BTN(r_, lbl_, dis_) \
    { \
        int hv = (dis_) ? 0 : vhov_r(r_); \
        Color _bg = (dis_) ? (Color){18, 18, 18, 220} \
                  : hv     ? (Color){accent.r/6, accent.g/6, accent.b/6, 255} \
                  :           C_PANEL; \
        Color _bc = (dis_) ? (Color){45, 45, 45, 255} \
                  : hv     ? accent : C_BORDER; \
        Color _tc = (dis_) ? (Color){55, 55, 55, 255} \
                  : hv     ? accent : C_DIM; \
        DrawRectangleRounded(r_, rnd, 5, _bg); \
        DrawRectangleRoundedLinesEx(r_, rnd, 5, hv ? 1.8f : 1.0f, _bc); \
        int _tw = mtxt(lbl_, 10); \
        dtxt(lbl_, (int)(r_.x + r_.width/2 - _tw/2), \
             (int)(r_.y + h/2 - fh(10)/2), 10, _tc); \
    }
    STEP_BTN(r_m10, "-10", at_min)
    STEP_BTN(r_m1,  "-",   at_min)
    STEP_BTN(r_p1,  "+",   at_max)
    STEP_BTN(r_p10, "+10", at_max)
    #undef STEP_BTN

    /* Affichage de la valeur */
    DrawRectangleRounded(r_val, rnd, 5, (Color){0, 0, 0, 100});
    DrawRectangleRoundedLinesEx(r_val, rnd, 5, 1.8f, accent);
    const char *sv = TextFormat("%d", *val);
    int tw = mtxt(sv, 13);
    dtxt(sv, (int)(r_val.x + val_w/2 - tw/2),
         (int)(r_val.y + h/2 - fh(13)/2), 13, accent);

    return changed;
}

// ════════════════════════════════════════════════════
// DONNÉES DES PRESETS
// ════════════════════════════════════════════════════
static const char *THEME_LABELS_C[THEME_COUNT + 1] = {
    "Terres devastees",
    "Marais toxique",
    "Desert irradie",
    "Ville en ruine",
    "Usine abandonnee",
    "Aleatoire",
};

/* Distance min spawn→base : 6 valeurs en tuiles manhattan */
static const int MINDIST_VALUES[6] = { 4, 6, 10, 14, 18, 22 };
static const char *MINDIST_LABELS[6] = {
    "Tres proche", "Proche", "Normal", "Eloigne", "Tres eloigne", "Maximum"
};

/* Plafond de scaling HP (scale_cap) — 5 niveaux */
static const float SCALECAP_VALUES[5] = { 2.0f, 4.0f, 6.0f, 9.0f, 12.0f };
static const char *SCALECAP_LABELS[5] = {
    "Facile", "Normal", "Difficile", "Intense", "Extreme"
};

/* Multiplicateur nombre d'ennemis (count_mult) — 5 niveaux */
static const float COUNTMULT_VALUES[5] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
static const char *COUNTMULT_LABELS[5] = {
    "Lente", "Normale", "Rapide", "Dense", "Extreme"
};

/* (dépôts : plus de presets, valeur libre via stepper) */

/* Taille de la carte en tuiles (largeur × hauteur) */
static const int   MAPSIZE_W[3] = { MAP_W, 38, 48 };
static const int   MAPSIZE_H[3] = { MAP_H, 22, 28 };
static const char *MAPSIZE_LABELS[3] = {
    "Standard", "Grande", "Enorme"
};

// ════════════════════════════════════════════════════
// INIT DES VALEURS PAR DÉFAUT (si non encore initialisées)
// ════════════════════════════════════════════════════
static void ensure_defaults(CustomConfig *cfg) {
    if (cfg->forced_bases    < 1)           cfg->forced_bases    = 1;
    if (cfg->forced_bases    > MAX_BASES)   cfg->forced_bases    = MAX_BASES;
    if (cfg->forced_spawns   < 1)           cfg->forced_spawns   = 2;
    if (cfg->forced_spawns   > MAX_PATHS)   cfg->forced_spawns   = MAX_PATHS;
    if (cfg->forced_deposits < 0)           cfg->forced_deposits = 0;   // 0 = aléatoire
    if (cfg->forced_deposits > MAX_MATERIAL_DEPOSITS) cfg->forced_deposits = MAX_MATERIAL_DEPOSITS;
    if (cfg->min_dist        < 4)    cfg->min_dist        = 10;
    if (cfg->scale_cap       < 0.1f) cfg->scale_cap       = 6.0f;
    if (cfg->count_mult      < 0.1f) cfg->count_mult      = 1.0f;
    if (cfg->map_w < 0) cfg->map_w = 0;   // 0 = taille standard
    if (cfg->map_h < 0) cfg->map_h = 0;
    /* theme == 0 correspond à THEME_WASTELAND, choix valide — on ne le force pas. */
    if (cfg->theme < 0 || cfg->theme > THEME_COUNT)
        cfg->theme = THEME_COUNT; // Aleatoire
}

/* Trouve l'index du preset le plus proche d'une valeur float */
static int find_float_preset(float val, const float *arr, int n) {
    int best = 0;
    float best_d = 1e9f;
    for (int i = 0; i < n; i++) {
        float d = val - arr[i];
        if (d < 0.0f) d = -d;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

static int find_int_preset(int val, const int *arr, int n) {
    int best = 0;
    int best_d = 1 << 30;
    for (int i = 0; i < n; i++) {
        int d = val - arr[i]; if (d < 0) d = -d;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

/* Retourne l'index du preset de taille de carte correspondant à map_w.
   0 (ou valeur ≤0) → Standard (index 0). */
static int find_map_preset(int mw) {
    if (mw <= 0) return 0;
    return find_int_preset(mw, MAPSIZE_W, 3);
}

/* ── Limites dynamiques selon la taille de carte ─────────────────────────
   Budget total (bases + spawns) = (périmètre - 8) / 6
     → périmètre utilisable (coins exclus) ÷ 6 tuiles par emplacement
     Énorme (48×28) : (2×76 - 8) / 6 = 24
     Grande (38×22) : (2×60 - 8) / 6 = 18
     Standard(28×16) : (2×44 - 8) / 6 = 13

   Limites individuelles (plafond absolu par type) :
     bases  ≤ bord_court / 3   (elles vont toutes sur 1 seul bord)
     spawns ≤ (W+H) / 4        (répartis sur les 4 bords)

   Dans l'UI, les deux steppers partagent le même budget total :
     max_spawns_effectif = min(ind_max_spawns, budget - bases_actuelles)
     max_bases_effectif  = min(ind_max_bases,  budget - spawns_actuels) */

static int map_budget(int map_w, int map_h) {
    int w = (map_w > 0) ? map_w : MAP_W;
    int h = (map_h > 0) ? map_h : MAP_H;
    int v = (2*(w + h) - 8) / 6;
    if (v < 3) v = 3;
    return v;
}

static int map_ind_max_bases(int map_w, int map_h) {
    int w = (map_w > 0) ? map_w : MAP_W;
    int h = (map_h > 0) ? map_h : MAP_H;
    int min_edge = (w < h) ? w : h;
    int v = min_edge / 3;
    if (v < 1) v = 1;
    if (v > MAX_BASES) v = MAX_BASES;
    return v;
}

static int map_ind_max_spawns(int map_w, int map_h) {
    int w = (map_w > 0) ? map_w : MAP_W;
    int h = (map_h > 0) ? map_h : MAP_H;
    int v = (w + h) / 4;
    if (v < 2) v = 2;
    if (v > MAX_PATHS) v = MAX_PATHS;
    return v;
}

// ════════════════════════════════════════════════════
// ÉCRAN DE CONFIGURATION
// ════════════════════════════════════════════════════
MenuAction draw_custom_config(MenuState *m, int vw, int vh)
{
    MenuAction act = {0};
    int is_mp = (m->screen == MENU_MP_CONFIG_ADV);   // config avancée d'hébergement multijoueur
    draw_bg(m, vw, vh);
    draw_header(is_mp ? "CONFIG DE LA PARTIE (HOTE)" : "CUSTOM GAME", vw);

    CustomConfig *cfg = is_mp ? &m->mp_cfg : &m->custom_cfg;
    ensure_defaults(cfg);

    // ── Constantes de layout ──────────────────────────────────
    const int COL_THEME_X  = M_PAD * 2;          // colonne terrain
    const int COL_THEME_W  = 300;
    const int COL_PARAM_X  = COL_THEME_X + COL_THEME_W + M_PAD * 2;
    const int COL_PARAM_W  = vw - COL_PARAM_X - M_PAD * 2;
    const int CONTENT_Y    = M_PAD + 90;
    const int ROW_H        = 28;      // hauteur des boutons radio
    const int ROW_GAP      = 14;      // espace entre deux sections
    const int LBL_H        = fh(11) + 4;

    // ─────────────────────────────────────────────────────────
    // COLONNE GAUCHE — Terrain
    // ─────────────────────────────────────────────────────────
    {
        int y = CONTENT_Y;
        dtxt("Terrain :", COL_THEME_X, y, 11, C_GOLD);
        y += LBL_H;

        for (int i = 0; i <= THEME_COUNT; i++) {
            int is_sel = (cfg->theme == i);
            Rectangle r = {(float)COL_THEME_X, (float)y,
                           (float)COL_THEME_W,  (float)ROW_H};
            int hov = vhov_r(r);
            float rnd = (float)BTN_R / ROW_H;
            Color col = (i < THEME_COUNT) ? theme_get((ThemeID)i)->palette.path_fill
                                          : C_DIM;
            Color bg  = is_sel ? (Color){col.r/5, col.g/5, col.b/5, 255}
                      : hov    ? (Color){col.r/9, col.g/9, col.b/9, 255}
                      : C_PANEL;
            DrawRectangleRounded(r, rnd, 5, bg);
            DrawRectangleRoundedLinesEx(r, rnd, 5,
                is_sel ? 2.2f : hov ? 1.6f : 1.0f,
                is_sel ? col  : hov ? (Color){col.r/2,col.g/2,col.b/2,255} : C_BORDER);
            dtxt(THEME_LABELS_C[i],
                 COL_THEME_X + M_IN, y + ROW_H/2 - fh(10)/2, 10,
                 is_sel ? col : hov ? C_TEXT : C_DIM);
            if (is_sel) {
                const char *chk = ">";
                dtxt(chk,
                     COL_THEME_X + COL_THEME_W - M_IN - mtxt(chk, 10),
                     y + ROW_H/2 - fh(10)/2, 10, col);
            }
            if (vclick_r(r)) {
                audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                cfg->theme = i;
            }
            y += ROW_H + 4;
        }
    }

    // ─────────────────────────────────────────────────────────
    // COLONNE DROITE — Paramètres de génération et difficulté
    // ─────────────────────────────────────────────────────────
    {
        int y = CONTENT_Y;

        /* Largeur du stepper : occupe la moitié droite de la colonne */
        const int STEP_W = COL_PARAM_W / 2;

        /* ── Budget partagé bases + spawns ───────────────────────
           Calcul des limites effectives :
             - budget total de la carte (bases+spawns réunis)
             - plafond individuel par type (bord court / 4 edges)
             - chaque stepper retire ce que l'autre a déjà utilisé */
        int budget     = map_budget        (cfg->map_w, cfg->map_h);
        int ind_max_s  = map_ind_max_spawns(cfg->map_w, cfg->map_h);
        int ind_max_b  = map_ind_max_bases (cfg->map_w, cfg->map_h);

        /* Priorité aux spawns : on les clamp en premier, puis on recalcule
           le plafond des bases sur le budget restant. */
        int max_spawns = budget - cfg->forced_bases;
        if (max_spawns > ind_max_s) max_spawns = ind_max_s;
        if (max_spawns < 1)         max_spawns = 1;
        if (cfg->forced_spawns > max_spawns) cfg->forced_spawns = max_spawns;

        int max_bases  = budget - cfg->forced_spawns;
        if (max_bases > ind_max_b)  max_bases  = ind_max_b;
        if (max_bases < 1)          max_bases  = 1;
        if (cfg->forced_bases > max_bases)   cfg->forced_bases  = max_bases;

        // ── Spawns ennemis ────────────────────────────────────
        dtxt("Spawns ennemis :",
             COL_PARAM_X, y + ROW_H/2 - fh(11)/2, 11, C_GOLD);
        dtxt(TextFormat("max %d", max_spawns),
             COL_PARAM_X + mtxt("Spawns ennemis :", 11) + 6,
             y + ROW_H/2 - fh(9)/2, 9, C_DIM);
        draw_stepper(&cfg->forced_spawns, 1, max_spawns,
                     COL_PARAM_X + COL_PARAM_W - STEP_W, y, STEP_W, ROW_H, C_RED);
        y += ROW_H + ROW_GAP;

        // ── Bases alliées ─────────────────────────────────────
        dtxt("Bases alliees :",
             COL_PARAM_X, y + ROW_H/2 - fh(11)/2, 11, C_GOLD);
        dtxt(TextFormat("max %d", max_bases),
             COL_PARAM_X + mtxt("Bases alliees :", 11) + 6,
             y + ROW_H/2 - fh(9)/2, 9, C_DIM);
        draw_stepper(&cfg->forced_bases, 1, max_bases,
                     COL_PARAM_X + COL_PARAM_W - STEP_W, y, STEP_W, ROW_H, C_GREEN);
        /* Indicateur de budget utilisé */
        dtxt(TextFormat("%d / %d emplacements", cfg->forced_spawns + cfg->forced_bases, budget),
             COL_PARAM_X + COL_PARAM_W - STEP_W,
             y + ROW_H + 2, 9,
             (cfg->forced_spawns + cfg->forced_bases >= budget) ? C_RED : C_DIM);
        y += ROW_H + ROW_GAP + fh(9) + 4;

        // ── Dépôts de minerais ────────────────────────────────
        dtxt("Depots de minerais :",
             COL_PARAM_X, y + ROW_H/2 - fh(11)/2, 11, C_GOLD);
        dtxt("(0 = aleatoire)",
             COL_PARAM_X + mtxt("Depots de minerais :", 11) + 6,
             y + ROW_H/2 - fh(9)/2, 9, C_DIM);
        draw_stepper(&cfg->forced_deposits, 0, MAX_MATERIAL_DEPOSITS,
                     COL_PARAM_X + COL_PARAM_W - STEP_W, y, STEP_W, ROW_H, C_ORANGE);
        y += ROW_H + ROW_GAP;

        // ── Taille de la carte ────────────────────────────────
        dtxt("Taille de la carte :", COL_PARAM_X, y, 11, C_GOLD);
        y += LBL_H;
        {
            int sel = find_map_preset(cfg->map_w);
            if (radio_row(MAPSIZE_LABELS, 3, COL_PARAM_X, y,
                          COL_PARAM_W / 3 - 4, ROW_H, C_BLUE, &sel)) {
                cfg->map_w = MAPSIZE_W[sel];
                cfg->map_h = MAPSIZE_H[sel];
            }
            /* Affiche les dimensions exactes à droite */
            int eff_w = (cfg->map_w > 0) ? cfg->map_w : MAP_W;
            int eff_h = (cfg->map_h > 0) ? cfg->map_h : MAP_H;
            dtxt(TextFormat("%dx%d tuiles", eff_w, eff_h),
                 COL_PARAM_X + COL_PARAM_W - 70, y + ROW_H/2 - fh(9)/2,
                 9, C_DIM);
        }
        y += ROW_H + ROW_GAP;

        // ── Distance min spawn → base ─────────────────────────
        draw_sep(COL_PARAM_X, y, COL_PARAM_W, C_BORDER);
        y += M_IN;
        dtxt("Distance min spawn/base :", COL_PARAM_X, y, 11, C_GOLD);
        dtxt("(faible = chemins courts = plus difficile)",
             COL_PARAM_X, y + fh(11) + 2, 9, C_DIM);
        y += LBL_H + fh(9) + 4;
        {
            int n = 6;
            int btn_w = (COL_PARAM_W - 5 * 6) / n;
            int sel = find_int_preset(cfg->min_dist, MINDIST_VALUES, n);
            if (radio_row(MINDIST_LABELS, n, COL_PARAM_X, y,
                          btn_w, ROW_H, C_BLUE, &sel))
                cfg->min_dist = MINDIST_VALUES[sel];
        }
        y += ROW_H + ROW_GAP;

        // ── Scalabilité ───────────────────────────────────────
        draw_sep(COL_PARAM_X, y, COL_PARAM_W, C_BORDER);
        y += M_IN;
        dtxt("Scalabilite des ennemis", COL_PARAM_X, y, 12, C_GOLD);
        y += fh(12) + 4;

        // Puissance max
        dtxt("Puissance max (plafond HP/degats) :", COL_PARAM_X, y, 11, C_TEXT);
        y += LBL_H;
        {
            int n = 5;
            int btn_w = (COL_PARAM_W - 4 * 6) / n;
            int sel = find_float_preset(cfg->scale_cap, SCALECAP_VALUES, n);
            if (radio_row(SCALECAP_LABELS, n, COL_PARAM_X, y,
                          btn_w, ROW_H, C_ORANGE, &sel))
                cfg->scale_cap = SCALECAP_VALUES[sel];
            // Valeur numérique en dessous
            dtxt(TextFormat("x%.0f max", cfg->scale_cap),
                 COL_PARAM_X + COL_PARAM_W - 54, y + ROW_H/2 - fh(9)/2,
                 9, C_DIM);
        }
        y += ROW_H + ROW_GAP;

        // Croissance
        dtxt("Croissance (nombre d'ennemis par vague) :", COL_PARAM_X, y, 11, C_TEXT);
        y += LBL_H;
        {
            int n = 5;
            int btn_w = (COL_PARAM_W - 4 * 6) / n;
            int sel = find_float_preset(cfg->count_mult, COUNTMULT_VALUES, n);
            if (radio_row(COUNTMULT_LABELS, n, COL_PARAM_X, y,
                          btn_w, ROW_H, C_RED, &sel))
                cfg->count_mult = COUNTMULT_VALUES[sel];
            dtxt(TextFormat("x%.1f", cfg->count_mult),
                 COL_PARAM_X + COL_PARAM_W - 34, y + ROW_H/2 - fh(9)/2,
                 9, C_DIM);
        }
        y += ROW_H + ROW_GAP + 4;

        // ── Résumé de la configuration ────────────────────────
        draw_sep(COL_PARAM_X, y, COL_PARAM_W, C_BORDER);
        y += M_IN + 2;
        const char *th_name = (cfg->theme == THEME_COUNT)
            ? "Aleatoire"
            : theme_get((ThemeID)cfg->theme)->name;
        const char *dep_str = (cfg->forced_deposits <= 0) ? "dep.alea"
                            : TextFormat("%ddep.", cfg->forced_deposits);
        int eff_w = (cfg->map_w > 0) ? cfg->map_w : MAP_W;
        int eff_h = (cfg->map_h > 0) ? cfg->map_h : MAP_H;
        dtxt(TextFormat(
            "%s  |  %dsp.  |  %dbase(s)  |  %s  |  %dx%d  |  dist%d  |  capx%.0f  |  x%.1fennemis",
            th_name, cfg->forced_spawns, cfg->forced_bases,
            dep_str, eff_w, eff_h, cfg->min_dist, cfg->scale_cap, cfg->count_mult),
             COL_PARAM_X, y, 9, C_DIM);
    }

    // ─────────────────────────────────────────────────────────
    // BOUTONS BAS
    // ─────────────────────────────────────────────────────────
    {
        int bh2 = BTN_H + 4;
        int bw2 = 180;
        int by2 = vh - M_PAD - bh2;
        int bx2 = vw / 2 - bw2 / 2;

        if (is_mp) {
            if (draw_btn("HEBERGER LA PARTIE", bx2, by2, bw2, bh2, C_GREEN, 0)) {
                m->mp_role  = 1;  m->mp_edit_field = 0;
                m->screen   = MENU_MP_LOBBY;
                act.mp_host = 1;  act.mp_mode = m->mp_mode;
            }
        } else if (draw_btn("LANCER LA PARTIE", bx2, by2, bw2, bh2, C_GREEN, 0)) {
            act.start_custom = 1;
            act.custom_cfg   = *cfg;
        }
    }

    if (draw_back_btn(vw, vh)) {
        if (is_mp) {
            m->screen = MENU_MP_CONFIG;   // retour à l'écran simple
        } else {
            m->screen = m->paused ? MENU_PAUSE : m->back_screen;
            if (!m->paused) pop_back_screen(m);
        }
    }
    return act;
}

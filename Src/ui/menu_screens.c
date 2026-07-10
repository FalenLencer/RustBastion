/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_screens.c ─ Écrans principaux du menu.
 *
 *  Contient :
 *    draw_title          — Écran d'accueil (JOUER / OPTIONS / QUITTER)
 *    draw_play_hub       — Hub de sélection de mode
 *    draw_slot_list      — Liste des emplacements de sauvegarde (arcade & campagne)
 *    draw_new_campaign   — Configuration d'une nouvelle campagne
 *    draw_new_arcade     — Sélection de thème pour une partie arcade
 *    draw_confirm_del    — Confirmation de suppression d'une sauvegarde
 *    draw_pause          — Menu pause en jeu
 */

#include "menu_internal.h"
#include "ui_anim.h"

#define GAME_VERSION "v0.1"

// ════════════════════════════════════════════════════
// ÉCRAN TITRE — animation d'entrée + braises de fond
// ════════════════════════════════════════════════════

// ── Réglages de l'entrée en scène ────────────────────────────────
#define SLOT_TITLE        0       // slot ui_timer réservé à l'écran titre
#define SLOT_PAUSE        1       // slot ui_timer réservé au menu pause
#define TITLE_FALL_DUR    0.45f   // chute du titre (s)
#define TITLE_FALL_H      28.0f   // hauteur de chute (px)
#define TITLE_FADE_DUR    0.25f   // fondu du titre (s)
#define TITLE_SUB_DELAY   0.15f   // retard du sous-titre (s)
#define TITLE_SEP_DELAY   0.30f   // retard du séparateur (s)
#define TITLE_SEP_DUR     0.35f   // étirement du séparateur (s)
#define TITLE_SEP_W       360     // largeur finale du séparateur (px)
#define TITLE_BTN_DUR     0.30f   // glissement d'un bouton (s)
#define TITLE_BTN_SLIDE   24.0f   // distance de glissement (px)

// ── Braises dérivantes (cendres du fond peint) ───────────────────
#define EMBER_COUNT       24
#define EMBER_VY_MIN      8.0f    // vitesse de montée min (px/s)
#define EMBER_VY_MAX      18.0f   // vitesse de montée max (px/s)
#define EMBER_DRIFT       6.0f    // amplitude de dérive horizontale (px)
#define EMBER_DRIFT_FREQ  0.8f    // fréquence de la dérive (rad/s)
#define EMBER_FLICK_FREQ  3.0f    // fréquence du scintillement (rad/s)

typedef struct {
    float x, y;      // position (px canvas)
    float vy;        // vitesse de montée (négative)
    float phase;     // déphasage dérive/scintillement
    float hot;       // 0 = braise sombre {200,80,30} … 1 = vive {255,150,60}
    int   size;      // côté du pixel (1–2)
} Ember;

static void ember_respawn(Ember *e, int vw, int y) {
    e->x     = (float)GetRandomValue(0, vw);
    e->y     = (float)y;
    e->vy    = -(EMBER_VY_MIN + (float)GetRandomValue(0, 100) * 0.01f
                               * (EMBER_VY_MAX - EMBER_VY_MIN));
    e->phase = (float)GetRandomValue(0, 628) * 0.01f;
    e->hot   = (float)GetRandomValue(0, 100) * 0.01f;
    e->size  = GetRandomValue(1, 2);
}

static void embers_draw(int vw, int vh) {
    static Ember g_embers[EMBER_COUNT];
    static int   g_init = 0;
    static float g_time = 0.0f;

    if (!g_init) {   // premier passage : semées sur tout l'écran
        g_init = 1;
        for (int i = 0; i < EMBER_COUNT; i++)
            ember_respawn(&g_embers[i], vw, GetRandomValue(0, vh));
    }
    g_time += ui_dt();

    for (int i = 0; i < EMBER_COUNT; i++) {
        Ember *e = &g_embers[i];
        e->y += e->vy * ui_dt();
        if (e->y < -4.0f) ember_respawn(e, vw, vh + 4);

        float dx    = sinf(g_time * EMBER_DRIFT_FREQ + e->phase) * EMBER_DRIFT;
        float flick = (sinf(g_time * EMBER_FLICK_FREQ + e->phase * 2.0f) + 1.0f) * 0.5f;
        unsigned char a = (unsigned char)(40.0f + flick * 80.0f);   // 40..120
        Color col = {(unsigned char)(200.0f + e->hot * 55.0f),
                     (unsigned char)( 80.0f + e->hot * 70.0f),
                     (unsigned char)( 30.0f + e->hot * 30.0f), a};
        DrawRectangle((int)(e->x + dx), (int)e->y, e->size, e->size, col);
    }
}

// Texte centré + cadre sombre, avec facteur de fondu (réplique locale de
// txt_c_boxed : le helper partagé n'expose pas d'alpha global).
static void boxed_txt_fade(const char *s, int cx, int y, int fs,
                           Color col, float af) {
    if (af <= 0.0f) return;
    if (af > 1.0f) af = 1.0f;
    int tw = mtxt(s, fs);
    int rh = fh(fs);
    int px = 8, py = 3;
    Rectangle r = {(float)(cx - tw/2 - px), (float)(y - py),
                   (float)(tw + px*2), (float)(rh + py*2)};
    DrawRectangleRounded(r, 0.25f, 4, (Color){5, 3, 1, (unsigned char)(210 * af)});
    DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.0f,
                                (Color){55, 36, 12, (unsigned char)(120 * af)});
    dtxt(s, cx - tw/2, y, fs,
         (Color){col.r, col.g, col.b, (unsigned char)(col.a * af)});
}

// Bouton à entrée animée : glisse depuis la gauche en fondu, hitbox FINALE
// (mini draw_btn local — on ne modifie pas le helper partagé). Une fois
// installé, délègue au draw_btn normal (hover, press, reflet…).
// click_during_anim : 1 = cliquable dès la 1re frame (écran titre),
//                     0 = cliquable seulement après son délai (pause).
static int fade_btn(const char *label, int x, int y, int w, int h,
                    Color col, float t, float delay, float dur,
                    float slide, int click_during_anim) {
    if (t >= delay + dur)
        return draw_btn(label, x, y, w, h, col, 0);

    float k = (t <= delay) ? 0.0f : ea_out_cubic((t - delay) / dur);
    if (k > 0.0f) {
        int dx = x - (int)((1.0f - k) * slide);
        Rectangle rd  = {(float)dx, (float)y, (float)w, (float)h};
        float     rnd = (float)BTN_R / h;
        unsigned char a = (unsigned char)(k * 255.0f);
        DrawRectangleRounded(rd, rnd, 6, (Color){14, 9, 4, a});        // C_PANEL
        DrawRectangleRoundedLinesEx(rd, rnd, 6, 1.2f,
                                    (Color){55, 36, 12, a});           // C_BORDER
        int fs2 = 14;
        int tw2 = mtxt(label, fs2);
        dtxt(label, dx + w/2 - tw2/2, y + h/2 - fh(fs2)/2, fs2,
             (Color){col.r, col.g, col.b, a});
    }
    if (!click_during_anim && t <= delay) return 0;
    if (vclick_r((Rectangle){(float)x, (float)y, (float)w, (float)h})) {
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        return 1;
    }
    return 0;
}

MenuAction draw_title(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2;

    /* Détection d'arrivée : la transition (menu.c) remet trans_t à 0 à
       chaque entrée d'écran ; si trans_t recule alors que NOUS sommes
       l'écran rendu, une nouvelle entrée sur le titre commence. */
    {
        static float prev_trans = 1e9f;
        if (m->trans_t < prev_trans) ui_timer(SLOT_TITLE, 1);
        prev_trans = m->trans_t;
    }
    float at = ui_timer(SLOT_TITLE, 0);   // temps depuis l'entrée sur l'écran

    draw_bg(m, vw, vh);
    menu_anim_render(&m->anim, vw, vh);

    /* Braises dérivantes : devant la scène, sous le vignettage. */
    embers_draw(vw, vh);

    /* Vignettage des bords : cadre la scène (rendu plus fini) sans masquer
       l'animation centrale. Dégradés transparent→sombre sur chaque bord. */
    {
        int e = 110;                                  /* épaisseur du dégradé   */
        Color dk = (Color){0, 0, 0, 120};
        Color tr = (Color){0, 0, 0, 0};
        DrawRectangleGradientV(0, 0,      vw, e, dk, tr);          /* haut */
        DrawRectangleGradientV(0, vh - e, vw, e, tr, dk);          /* bas  */
        DrawRectangleGradientH(0, 0,      e,  vh, dk, tr);         /* gauche */
        DrawRectangleGradientH(vw - e, 0, e,  vh, tr, dk);         /* droite */
    }

    /* ── Titre : tombe de 28px avec rebond + fondu ──────────────── */
    {
        float fall = ea_out_back(at / TITLE_FALL_DUR);          // entrée clampée
        int   ty   = vh/2 - 170 - (int)((1.0f - fall) * TITLE_FALL_H);
        boxed_txt_fade("RUST BASTION", cx, ty, 46, C_GOLD,
                       at / TITLE_FADE_DUR);
    }
    /* ── Sous-titre : même chute, +0.15s de retard ──────────────── */
    {
        float st   = at - TITLE_SUB_DELAY;
        float fall = ea_out_back(st / TITLE_FALL_DUR);
        int   sy   = vh/2 - 114 - (int)((1.0f - fall) * TITLE_FALL_H);
        boxed_txt_fade("Tower Defense Post-Apocalyptique", cx, sy, 13, C_DIM,
                       st / TITLE_FADE_DUR);
    }
    /* ── Séparateur : s'étire du centre ─────────────────────────── */
    {
        float k = ea_out_cubic((at - TITLE_SEP_DELAY) / TITLE_SEP_DUR);
        int   w = (int)(TITLE_SEP_W * k);
        if (w > 1) draw_sep(cx - w/2, vh/2 - 92, w, C_BORDER);
    }

    /* ── Boutons : cascade glissée depuis la gauche ─────────────── */
    int bw = 240, bh = BTN_H + 4;
    int bx = cx - bw/2;
    int by = vh/2 - 52;

    if (fade_btn("JOUER",   bx, by, bw, bh, C_GREEN, at, 0.45f,
                 TITLE_BTN_DUR, TITLE_BTN_SLIDE, 1)) {
        m->screen = MENU_PLAY_HUB;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (fade_btn("OPTIONS", bx, by, bw, bh, C_BLUE, at, 0.55f,
                 TITLE_BTN_DUR, TITLE_BTN_SLIDE, 1)) {
        m->screen = MENU_OPTIONS;
        m->back_screen = MENU_TITLE;
    }
    by += bh + M_IN;

    if (fade_btn("QUITTER", bx, by, bw, bh, C_RED, at, 0.65f,
                 TITLE_BTN_DUR, TITLE_BTN_SLIDE, 1))
        act.quit_app = 1;

    dtxt(GAME_VERSION, vw - M_PAD - mtxt(GAME_VERSION, 9),
         vh - M_PAD - 12, 9, C_DIM);
    return act;
}

// ════════════════════════════════════════════════════
// HUB JOUER — cascade d'entrée + compteur + badge
// ════════════════════════════════════════════════════
#define SLOT_HUB          2       // slot ui_timer : entrée du hub
#define SLOT_HUB_PULSE    3       // slot ui_timer : pulse du bestiaire
#define HUB_BTN_DUR       0.25f   // glissement d'un nav-bouton (s)
#define HUB_BTN_STEP      0.04f   // décalage entre boutons (s)
#define HUB_BTN_SLIDE     18.0f   // distance de glissement vertical (px)
#define HUB_SCRAP_ROLL    0.4f    // durée du compteur roulant (s)
#define HUB_PULSE_PERIOD  0.8f    // période d'une pulsation bestiaire (s)
#define HUB_PULSE_COUNT   3       // nombre de pulsations

// Nav-bouton à entrée animée : glisse de +18px vers le haut en fondu.
// Réplique simplifiée de draw_nav_btn (qui n'expose pas d'alpha) pendant
// l'anim ; clics IGNORÉS tant que l'entrée n'est pas finie, puis délègue
// au vrai draw_nav_btn (hover, press, reflet…). Hitbox finale intacte.
static int fade_nav_btn(const char *icon, const char *title,
                        const char *desc, Color col,
                        int x, int y, int w, int h,
                        float t, float delay) {
    if (t >= delay + HUB_BTN_DUR)
        return draw_nav_btn(icon, title, desc, col, x, y, w, h);

    float k = (t <= delay) ? 0.0f : ea_out_cubic((t - delay) / HUB_BTN_DUR);
    if (k > 0.0f) {
        int yd = y + (int)((1.0f - k) * HUB_BTN_SLIDE);
        Rectangle rd  = {(float)x, (float)yd, (float)w, (float)h};
        float     rnd = (float)BTN_R / h;
        unsigned char a = (unsigned char)(k * 255.0f);
        DrawRectangleRounded(rd, rnd, 6, (Color){14, 9, 4, a});   // C_PANEL
        DrawRectangleRoundedLinesEx(rd, rnd, 6, 1.2f,
            (Color){col.r/3, col.g/3, col.b/3, (unsigned char)(180.0f * k)});
        DrawRectangleRounded(
            (Rectangle){(float)x, (float)(yd+4), 4, (float)(h-8)},
            0.5f, 4, (Color){col.r, col.g, col.b, (unsigned char)(110.0f * k)});
        dtxt(icon, x + M_PAD + 4, yd + h/2 - fh(26)/2, 26,
             (Color){col.r, col.g, col.b, a});
        dtxt(title, x + M_PAD + 36, yd + M_IN + 2, 16,
             (Color){C_TEXT.r, C_TEXT.g, C_TEXT.b, a});
        char dbuf[72];
        clip_text(desc, w - M_PAD - 36 - M_IN, 10, dbuf, sizeof(dbuf));
        dtxt(dbuf, x + M_PAD + 36, yd + M_IN + 22, 10,
             (Color){130, 110, 72, a});
    }
    return 0;   // pas de clic pendant l'entrée
}

MenuAction draw_play_hub(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2;

    /* Détection d'arrivée (même principe que l'écran titre) */
    {
        static float prev_trans = 1e9f;
        if (m->trans_t < prev_trans) ui_timer(SLOT_HUB, 1);
        prev_trans = m->trans_t;
    }
    float t = ui_timer(SLOT_HUB, 0);

    draw_bg(m, vw, vh);
    draw_header("CHOISIR UN MODE", vw);

    int bw = 520, bh = 72, gap = M_IN + 2;
    int bx = cx - bw/2;
    int by = M_PAD + 76;

    if (fade_nav_btn("C", "CAMPAGNE",
                     "Carte de progression — 5 chapitres, 15 actes.",
                     C_GOLD, bx, by, bw, bh, t, 0*HUB_BTN_STEP)) {
        push_back_screen(m);
        m->selected_campaign_act = -1;   // reset : aucun acte pre-selectionne
        m->screen      = MENU_WORLD_MAP;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    if (fade_nav_btn("A", "ARCADE",
                     "Choisissez un environnement et jouez librement.",
                     C_BLUE, bx, by, bw, bh, t, 1*HUB_BTN_STEP)) {
        push_back_screen(m);
        m->screen = MENU_ARCADE;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    if (fade_nav_btn("T", "TUTORIEL",
                     "Apprenez les bases : tours, vagues, minerais, pause, zoom.",
                     (Color){120, 200, 230, 255}, bx, by, bw, bh,
                     t, 2*HUB_BTN_STEP)) {
        act.start_tutorial = 1;   // lancé par app.c
    }
    by += bh + gap;

    if (fade_nav_btn("H", "MODE HEROS 3D (beta)",
                     "Incarnez un heros sur le terrain : tirez, recrutez, batissez.",
                     (Color){235, 130, 60, 255}, bx, by, bw, bh,
                     t, 3*HUB_BTN_STEP)) {
        act.start_hero = 1;       // lancé par app.c
    }
    by += bh + gap;

    if (fade_nav_btn("M", "MULTIJOUEUR",
                     "Jouez ensemble ou l'un contre l'autre (code de session).",
                     C_GREEN, bx, by, bw, bh, t, 4*HUB_BTN_STEP)) {
        push_back_screen(m);
        m->screen      = MENU_MP_HUB;
        m->back_screen = MENU_PLAY_HUB;
        m->mp_role = 0;
        if (m->mp_mode == MP_NONE) m->mp_mode = MP_COURSE;
    }
    by += bh + gap;

    if (fade_nav_btn("X", "CUSTOM GAME",
                     "Carte, spawns, bases, terrain et difficulte sur mesure.",
                     C_ORANGE, bx, by, bw, bh, t, 5*HUB_BTN_STEP)) {
        push_back_screen(m);
        m->screen = MENU_CUSTOM;
        m->back_screen = MENU_PLAY_HUB;
    }
    by += bh + gap;

    /* Compteur ferraille « roulant » : la valeur affichée court vers la
       vraie valeur en ~HUB_SCRAP_ROLL s (au moins 1 unite/s). */
    {
        static float disp_scrap = -1.0f;
        if (disp_scrap < 0.0f) disp_scrap = (float)meta->scrap;  // 1er passage
        float target = (float)meta->scrap;
        float diff   = target - disp_scrap;
        if (diff != 0.0f) {
            float speed = fabsf(diff) / HUB_SCRAP_ROLL;          // unités/s
            if (speed < 1.0f) speed = 1.0f;
            float step = speed * ui_dt();
            if (fabsf(diff) <= step) disp_scrap = target;
            else disp_scrap += (diff > 0.0f) ? step : -step;
        }
        char upg_desc[80];
        snprintf(upg_desc, sizeof(upg_desc),
                 "Depensez vos %d ferrailles pour ameliorer vos defenses.",
                 (int)(disp_scrap + 0.5f));
        if (fade_nav_btn("*", "AMELIORATIONS", upg_desc,
                         C_ORANGE, bx, by, bw, bh, t, 6*HUB_BTN_STEP)) {
            push_back_screen(m);
            m->screen = MENU_UPGRADES;
            m->back_screen = MENU_PLAY_HUB;
        }
    }
    by += bh + gap;

    int nb_disc = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (meta->bestiary_discovered[i]) nb_disc++;
    char best_desc[80];
    snprintf(best_desc, sizeof(best_desc),
             "%d/%d ennemis identifies. Resistances et faiblesses.",
             nb_disc, ENEMY_TYPE_COUNT);
    if (fade_nav_btn("B", "BESTIAIRE", best_desc,
                     C_RED, bx, by, bw, bh, t, 7*HUB_BTN_STEP)) {
        push_back_screen(m);
        m->screen      = MENU_BESTIARY;
        m->back_screen = MENU_PLAY_HUB;
        if (m->sel_bestiary < 0) m->sel_bestiary = 0;
    }

    /* Badge bestiaire : nouvelles découvertes depuis la dernière visite
       → la bordure pulse HUB_PULSE_COUNT fois puis s'arrête. */
    {
        static int prev_disc = -1;
        static int pulsing   = 0;
        if (prev_disc < 0) prev_disc = nb_disc;          // 1er passage : muet
        if (nb_disc > prev_disc) {
            pulsing   = 1;
            prev_disc = nb_disc;
            ui_timer(SLOT_HUB_PULSE, 1);
        }
        if (pulsing) {
            float pt = ui_timer(SLOT_HUB_PULSE, 0);
            if (pt >= HUB_PULSE_PERIOD * HUB_PULSE_COUNT) {
                pulsing = 0;
            } else {
                /* (1-cos)/2 : part de 0, culmine à mi-période */
                float ph = (1.0f - cosf(pt * 2.0f * PI / HUB_PULSE_PERIOD)) * 0.5f;
                Rectangle br = {(float)bx, (float)by, (float)bw, (float)bh};
                DrawRectangleRoundedLinesEx(br, (float)BTN_R/bh, 6, 2.5f,
                    (Color){C_RED.r, C_RED.g, C_RED.b,
                            (unsigned char)(ph * 200.0f)});
            }
        }
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
MenuAction draw_slot_list(MenuState *m, int vw, int vh, int is_campaign) {
    MenuAction act = {0};
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header(is_campaign ? "CAMPAGNE" : "ARCADE", vw);

    const char *sub = is_campaign
        ? "15 actes en sequence — 5 chapitres — la ferraille se gagne ici"
        : "Mode libre — choisissez votre terrain";
    txt_c_boxed(sub, cx, M_PAD + 86, 10, C_TEXT);

    int sw = 540, sh = 68, sg = M_IN;
    int sx = cx - sw/2;
    int y  = M_PAD + 112;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
        const SaveInfo *si = is_campaign ? &m->campaign_slots[i] : &m->slots[i];

        Rectangle r = {(float)sx,(float)y,(float)sw,(float)sh};
        int hov = vhov_r(r);
        Color brd = si->exists ? (is_campaign ? C_GOLD : C_BLUE) : C_BORDER;
        Color bg  = hov ? C_HOV : C_PANEL;

        DrawRectangleRounded(r, (float)PANEL_R/sh, 6, bg);
        DrawRectangleRoundedLinesEx(r, (float)PANEL_R/sh, 6, 1.5f, brd);

        int tx = sx + M_IN + 4;
        int ty = y  + M_IN;

        /* ── Boutons communs (calculés ici pour aligner le clip du titre) ── */
        const int bh2  = 26;
        const int bw_rep = 88, bw_del = 58, btn_gap = 4;
        const int bx_del = sx + sw - M_IN - bw_del;
        const int bx_rep = bx_del - btn_gap - bw_rep;
        const int by2    = y + sh/2 - bh2/2;
        const int txt_clip_w = bx_rep - tx - M_IN;   /* largeur dispo pour le titre */

        if (si->exists) {
            if (is_campaign) {
                const ActData *ad = campaign_act_get(si->campaign_stage);
                char raw[96];
                snprintf(raw, sizeof(raw),
                         "Campagne %d  —  Acte %d/%d  —  %s",
                         si->campaign_num + 1,
                         si->campaign_stage + 1,
                         CAMPAIGN_TOTAL,
                         ad ? ad->title : "?");
                char cbuf[96];
                clip_text(raw, txt_clip_w, 12, cbuf, sizeof(cbuf));
                draw_text_boxed(cbuf, tx, ty, 12, C_GOLD);
            } else {
                char arcbuf[64];
                clip_text(TextFormat("ARCADE  —  %s", si->theme_name),
                          txt_clip_w, 12, arcbuf, sizeof(arcbuf));
                dtxt(arcbuf, tx, ty, 12, C_BLUE);
            }
            ty += fh(12) + 1;   /* hauteur réelle du titre (taille 12 → ~19 px) */
            {
                char s1[32], s2[24], s3[24];
                snprintf(s1, sizeof(s1), "Vague %d  |  ", si->wave);
                snprintf(s2, sizeof(s2), "  %d  |  ", si->lives);
                snprintf(s3, sizeof(s3), "  %d", si->gold);
                int ix = tx;
                dtxt(s1, ix, ty, 10, C_TEXT);   ix += mtxt(s1, 10);
                draw_icon(g_icon_heart, ix, ty, fh(10), WHITE); ix += fh(10);
                dtxt(s2, ix, ty, 10, C_TEXT);   ix += mtxt(s2, 10);
                draw_icon(g_icon_gold,  ix, ty, fh(10), WHITE); ix += fh(10);
                dtxt(s3, ix, ty, 10, C_TEXT);
            }
            ty += fh(10) + 2;   /* hauteur réelle de la ligne de stats (~16 px) */
            dtxt(TextFormat("Emplacement %d", i+1), tx, ty, 9, C_DIM);

            /* REPRENDRE + EFFACER côte à côte */
            if (draw_btn("REPRENDRE", bx_rep, by2, bw_rep, bh2, C_GREEN, 0)) {
                act.resume_slot        = i;
                act.resume_is_campaign = is_campaign;
                act.go_game            = 1;
            }
            if (draw_btn("EFFACER", bx_del, by2, bw_del, bh2, C_RED, 0)) {
                m->confirm_del_slot = i;
                m->back_screen = is_campaign ? MENU_CAMPAIGN : MENU_ARCADE;
                m->screen = MENU_CONFIRM_DEL;
            }

        } else {
            if (is_campaign) {
                dtxt(TextFormat("Emplacement %d — vide", i+1),
                     tx, y + M_IN, 9, C_DIM);

                if (m->new_slot == i && m->selected_campaign_act >= 0) {
                    /* Acte sélectionné sur la carte → affiche + LANCER */
                    const ActData *sel_ad =
                        campaign_act_get(m->selected_campaign_act);
                    if (sel_ad) {
                        char albl[72];
                        snprintf(albl, sizeof(albl), "Acte %d — %s",
                                 m->selected_campaign_act + 1, sel_ad->title);
                        char aclip[72];
                        int lancer_w = bw_rep + btn_gap + bw_del; /* bouton large */
                        clip_text(albl, bx_rep - tx - M_IN, 11, aclip, sizeof(aclip));
                        dtxt(aclip, tx, y + M_IN + 14, 11, C_GOLD);
                        char sclip[64];
                        clip_text(sel_ad->subtitle, bx_rep - tx - M_IN, 9,
                                  sclip, sizeof(sclip));
                        dtxt(sclip, tx, y + M_IN + 28, 9, C_DIM);
                        if (draw_btn("LANCER", bx_rep, by2, lancer_w, bh2,
                                     C_GOLD, 0)) {
                            act.start_campaign      = 1;
                            act.new_slot            = i;
                            act.campaign_order_seed = 0;
                            act.start_campaign_act  = m->selected_campaign_act;
                        }
                    }
                } else {
                    /* Pas encore d'acte choisi → envoyer sur la carte du monde */
                    int bw_new = 150;
                    int bx_new = sx + sw - M_IN - bw_new;
                    if (draw_btn("NOUVELLE PARTIE", bx_new, by2, bw_new, bh2,
                                 C_GOLD, 0)) {
                        m->new_slot              = i;
                        m->selected_campaign_act = -1;
                        push_back_screen(m);
                        m->screen      = MENU_WORLD_MAP;
                        m->back_screen = MENU_CAMPAIGN;
                    }
                }
            } else {
                dtxt(TextFormat("Emplacement %d — vide", i+1),
                         tx, y + sh/2 - 8, 11, C_DIM);
                if (draw_btn("NOUVELLE ARCADE",
                             sx + sw - M_IN - 154, by2, 154, bh2, C_BLUE, 0)) {
                    m->new_slot            = i;
                    m->campaign_order_seed = 0;
                    m->screen = MENU_NEW_ARCADE;
                }
            }
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
MenuAction draw_new_campaign(MenuState *m, const MetaProgress *meta,
                             int vw, int vh)
{
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;
    draw_bg(m, vw, vh);
    draw_header("NOUVELLE CAMPAGNE", vw);

    const Color *CHAPTER_COLS = CHAPTER_COLORS;   // source unique (menu.c)
    static const char *CHAPTER_NAMES[CAMPAIGN_CHAPTERS] = {
        "Les Terres Brulees",
        "Le Marais Toxique",
        "Le Desert Irradie",
        "La Ville en Ruine",
        "L'Usine Abandonnee",
    };

    int pw = 480, ph = 310;
    draw_panel(cx, cy, pw, ph, C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int py = cy - ph/2 + M_PAD;

    dtxt(TextFormat("Emplacement : %d", m->new_slot+1), px, py, 10, C_DIM);
    py += M_LINE + 2;
    dtxt(TextFormat("Campagne n°%d", meta->campaigns_completed+1), px, py, 17, C_GOLD);
    py += fh(17) + 4;
    draw_sep(px, py, pw - M_PAD*2, C_BORDER);
    py += M_IN + 2;
    /* Acte de départ sélectionné */
    {
        const ActData *sel_ad = campaign_act_get(m->selected_campaign_act);
        if (sel_ad && m->selected_campaign_act > 0) {
            char dbuf[80];
            snprintf(dbuf, sizeof(dbuf), "Depart : Acte %d — %s",
                     m->selected_campaign_act + 1, sel_ad->title);
            char dclip[80];
            clip_text(dbuf, pw - M_PAD*2, 11, dclip, sizeof(dclip));
            dtxt(dclip, px, py, 11, C_GREEN);
            py += M_LINE + 2;
        } else {
            dtxt("Depart : Acte 1 — Premier contact", px, py, 11, C_GREEN);
            py += M_LINE + 2;
        }
    }
    dtxt("15 actes — 5 chapitres, ordre fixe :", px, py, 10, C_DIM);
    py += M_LINE + 2;

    for (int i = 0; i < CAMPAIGN_CHAPTERS; i++) {
        Color col = CHAPTER_COLS[i];
        Rectangle row = {(float)px,(float)py,(float)(pw - M_PAD*2), 28};
        DrawRectangleRounded(row, (float)PANEL_R/28, 5, (Color){18,10,3,210});
        DrawRectangleRoundedLinesEx(row, (float)PANEL_R/28, 5, 1.0f,
            (Color){col.r/3, col.g/3, col.b/3, 180});
        dtxt(TextFormat("Ch.%d", i+1), px + M_IN, py + 8, 11, C_DIM);
        dtxt(CHAPTER_NAMES[i], px + M_IN + 34, py + 8, 11, col);
        py += 28 + 4;
    }

    py = cy + ph/2 - M_PAD - BTN_H;
    int half = (pw - M_PAD*2 - M_IN) / 2;

    if (draw_btn("LANCER", px, py, half, BTN_H, C_GREEN, 0)) {
        act.start_campaign      = 1;
        act.new_slot            = m->new_slot;
        act.campaign_order_seed = 0;
        act.start_campaign_act  = m->selected_campaign_act;
    }
    if (draw_btn("ANNULER", px + half + M_IN, py,
                 pw - M_PAD*2 - M_IN - half, BTN_H, C_DIM, 0)) {
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

MenuAction draw_new_arcade(MenuState *m, int vw, int vh) {
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
            const char *chk = ">";
            dtxt(chk, bx + bw - M_IN - mtxt(chk,11),
                     by + bh/2 - fh(11)/2, 11, C_BLUE);
        }
        if (vclick_r(r)) m->new_theme = (ThemeID)i;
        by += bh + gap;
    }

    by += M_IN;
    int half = (bw - M_IN) / 2;
    if (draw_btn("LANCER",  bx,           by, half, BTN_H, C_GREEN, 0)) {
        act.start_arcade = 1;
        act.new_theme    = m->new_theme;
        act.new_slot     = m->new_slot;
    }
    if (draw_btn("ANNULER", bx+half+M_IN, by,
                 bw-M_IN-half, BTN_H, C_DIM, 0))
        m->screen = MENU_ARCADE;

    return act;
}

// ════════════════════════════════════════════════════
// CONFIRMATION SUPPRESSION
// ════════════════════════════════════════════════════
MenuAction draw_confirm_del(MenuState *m, int vw, int vh) {
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

    if (draw_btn("EFFACER", cx - bw2 - M_IN/2, by2, bw2, bh2, C_RED, 0)) {
        if (m->back_screen == MENU_CAMPAIGN ||
            m->back_screen == MENU_WORLD_MAP)
            campaign_save_delete(m->confirm_del_slot);
        else
            save_delete(m->confirm_del_slot);
        menu_refresh_slots(m);
        m->screen = (m->back_screen == MENU_CAMPAIGN ||
                     m->back_screen == MENU_ARCADE   ||
                     m->back_screen == MENU_WORLD_MAP)
                    ? m->back_screen : MENU_PLAY_HUB;
        set_msg(m, "Partie effacee.");
    }
    if (draw_btn("ANNULER", cx + M_IN/2, by2, bw2, bh2, C_TEXT, 0))
        m->screen = m->back_screen;

    return act;
}

// ════════════════════════════════════════════════════
// MENU PAUSE — ouverture théâtrale
// ════════════════════════════════════════════════════
#define PAUSE_OPEN_DUR   0.28f   // installation du panneau (s)
#define PAUSE_BACK_DUR   0.15f   // montée du voile de fond (s)
#define PAUSE_BTN_DUR    0.20f   // glissement d'un bouton (s)
#define PAUSE_BTN_DELAY0 0.10f   // délai du premier bouton (s)
#define PAUSE_BTN_STEP   0.05f   // décalage entre boutons (s)
#define PAUSE_BTN_SLIDE  12.0f   // distance de glissement (px)
#define PAUSE_RES_DELAY  0.40f   // apparition de la ligne résolution (s)
#define PAUSE_RES_DUR    0.15f   // durée de son fondu (s)

MenuAction draw_pause(MenuState *m, int vw, int vh) {
    MenuAction act = {0};
    int cx = vw/2, cy = vh/2;

    /* Détection d'ouverture : le slot n'a pas été lu pendant au moins
       une frame complète (cf. ui_timer_gap : écart normal ≤ 2 en pause
       à cause du double tick HUD+menu). ESC reste géré par app.c,
       indépendamment de cette animation (actif dès la 1re frame). */
    if (ui_timer_gap(SLOT_PAUSE) > 2) ui_timer(SLOT_PAUSE, 1);
    float t = ui_timer(SLOT_PAUSE, 0);
    float k = ea_out_back(t / PAUSE_OPEN_DUR);   // entrée clampée, overshoot ok

    /* Voile de fond : 0 → 155 en 0.15 s (linéaire) */
    float ba = t / PAUSE_BACK_DUR;
    if (ba > 1.0f) ba = 1.0f;
    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, (unsigned char)(155.0f * ba)});

    int pw = 260, ph = 340;

    /* Panneau : échelle 0.90 → 1.0 (léger dépassement) pour le DESSIN.
       Le layout du CONTENU (boutons, hitbox) reste sur pw/ph finaux. */
    float sc = 0.90f + 0.10f * k;
    draw_panel(cx, cy, (int)(pw * sc), (int)(ph * sc), C_GOLD);

    int px = cx - pw/2 + M_PAD;
    int iw = pw - M_PAD*2;
    int py = cy - (int)(ph * sc)/2 + M_PAD;   // le titre suit le panneau

    /* Titre "PAUSE" : tracking animé — les lettres se resserrent */
    {
        const char *word = "PAUSE";
        int   fs      = 22;
        float spacing = 4.0f + (1.0f - k) * 10.0f;
        float total   = 0.0f;
        for (int i = 0; word[i]; i++) {
            char one[2] = {word[i], '\0'};
            total += (float)mtxt(one, fs);
            if (word[i+1]) total += spacing;
        }
        float lx = (float)cx - total * 0.5f;
        for (int i = 0; word[i]; i++) {
            char one[2] = {word[i], '\0'};
            dtxt(one, (int)lx, py, fs, C_GOLD);
            lx += (float)mtxt(one, fs) + spacing;
        }
    }

    /* Contenu : positions FINALES (hitbox stables dès la 1re frame) */
    py = cy - ph/2 + M_PAD + fh(22) + 3;
    draw_sep(px, py, iw, C_BORDER);
    py += M_IN + 4;

    int bh2 = BTN_H, gap = M_IN - 2;

    if (fade_btn("REPRENDRE",     px, py, iw, bh2, C_GREEN, t,
                 PAUSE_BTN_DELAY0 + 0*PAUSE_BTN_STEP,
                 PAUSE_BTN_DUR, PAUSE_BTN_SLIDE, 0))
        { m->paused = 0; m->screen = MENU_TITLE; }
    py += bh2 + gap;

    if (fade_btn("SAUVEGARDER",   px, py, iw, bh2, C_GOLD, t,
                 PAUSE_BTN_DELAY0 + 1*PAUSE_BTN_STEP,
                 PAUSE_BTN_DUR, PAUSE_BTN_SLIDE, 0))
        act.save_and_quit = 2;
    py += bh2 + gap;

    if (fade_btn("OPTIONS",        px, py, iw, bh2, C_BLUE, t,
                 PAUSE_BTN_DELAY0 + 2*PAUSE_BTN_STEP,
                 PAUSE_BTN_DUR, PAUSE_BTN_SLIDE, 0))
        m->screen = MENU_OPTIONS;
    py += bh2 + gap;

    if (fade_btn("MENU PRINCIPAL", px, py, iw, bh2, C_DIM, t,
                 PAUSE_BTN_DELAY0 + 3*PAUSE_BTN_STEP,
                 PAUSE_BTN_DUR, PAUSE_BTN_SLIDE, 0))
        act.save_and_quit = 1;
    py += bh2 + gap;

    if (fade_btn("QUITTER",        px, py, iw, bh2, C_RED, t,
                 PAUSE_BTN_DELAY0 + 4*PAUSE_BTN_STEP,
                 PAUSE_BTN_DUR, PAUSE_BTN_SLIDE, 0))
        act.quit_app = 1;

    /* Ligne résolution : apparaît en dernier */
    {
        float ra = (t - PAUSE_RES_DELAY) / PAUSE_RES_DUR;
        if (ra > 0.0f) {
            if (ra > 1.0f) ra = 1.0f;
            char sz[48];
            snprintf(sz, sizeof(sz), "%dx%d  %s",
                     GetScreenWidth(), GetScreenHeight(),
                     IsWindowFullscreen() ? "Plein ecran" : "Fenetre");
            txt_c(sz, cx, cy + ph/2 - M_PAD - 10, 9,
                  (Color){C_DIM.r, C_DIM.g, C_DIM.b,
                          (unsigned char)(255.0f * ra)});
        }
    }

    return act;
}

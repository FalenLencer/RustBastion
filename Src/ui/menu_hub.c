/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_hub.c ─ Hub « JOUER » et sous-menu « PARTIE LIBRE ».
 *
 *  Le hub ne propose plus que 5 entrées (il en empilait 8) :
 *    • CAMPAGNE        — grande carte, le mode-roi (progression en badge)
 *    • PARTIE LIBRE    — sous-menu : arcade, partie perso, héros 3D, tutoriel
 *    • MULTIJOUEUR     — à côté de Partie libre
 *    • AMÉLIORATIONS / BESTIAIRE — rangée compacte (méta-progression)
 */

#include "menu_internal.h"
#include "ui_anim.h"

// ── Entrée en cascade + compteur + badge ─────────────────────────
#define SLOT_HUB          2       // slot ui_timer : entrée du hub
#define SLOT_HUB_PULSE    3       // slot ui_timer : pulse du bestiaire
#define SLOT_FREE         7       // slot ui_timer : entrée de PARTIE LIBRE
#define HUB_BTN_DUR       0.25f   // glissement d'une carte (s)
#define HUB_BTN_STEP      0.05f   // décalage entre cartes (s)
#define HUB_SCRAP_ROLL    0.4f    // durée du compteur roulant (s)
#define HUB_PULSE_PERIOD  0.8f    // période d'une pulsation bestiaire (s)
#define HUB_PULSE_COUNT   3       // nombre de pulsations

// ── Gabarit ──────────────────────────────────────────────────────
#define HUB_W             680     // largeur du bloc (descriptions entières en demi-carte)
#define HUB_GAP           12      // écart entre cartes (px)
#define HUB_CAMP_H        112     // carte CAMPAGNE (grande, cf. NAV_BIG_H)
#define HUB_ROW_H         84      // PARTIE LIBRE / MULTIJOUEUR
#define HUB_SMALL_H       58      // AMÉLIORATIONS / BESTIAIRE
#define HUB_V_BIAS        0.40f   // position verticale du bloc dans l'espace libre
#define FREE_CARD_H       70      // cartes du sous-menu PARTIE LIBRE

#define HUB_COL_HERO      ((Color){235, 130,  60, 255})
#define HUB_COL_TUTO      ((Color){120, 200, 230, 255})

// ── Carte de navigation (hub JOUER, PARTIE LIBRE) ───────────────
#define NAV_BIG_H       100   // hauteur à partir de laquelle la carte est « grande »
#define NAV_SLIDE       18.0f // glissement vertical pendant l'entrée (px)
#define NAV_LINE_GAP     3    // écart titre ↔ description (px)
#define NAV_TXT_DX      36    // texte après l'icône (carte normale, px)
#define NAV_TXT_DX_BIG  48    // texte après l'icône (grande carte, px)
#define NAV_DESC_COL    ((Color){130, 110, 72, 255})   // description au repos

// Couleur dont l'alpha est multiplié par k (fondu d'entrée).
static Color col_fade(Color c, float k) {
    c.a = (unsigned char)((float)c.a * k);
    return c;
}

// Carte de navigation : barre latérale colorée, icône, titre et description
// CENTRÉS verticalement, badge optionnel (texte court) aligné à droite.
// k = progression d'entrée (0..1) : < 1 → glisse + fondu, ni survol ni clic
// (hitbox finale inchangée) ; 1 → carte installée, interactive.
static int draw_nav_btn(const char *icon, const char *title, const char *desc,
                 const char *badge, Color col,
                 int x, int y, int w, int h, float k)
{
    if (k <= 0.0f) return 0;
    if (k > 1.0f)  k = 1.0f;
    int live = (k >= 1.0f);
    int yd   = y + (int)((1.0f - k) * NAV_SLIDE);   // position dessinée

    Rectangle r = {(float)x,(float)y,(float)w,(float)h};
    int   hov     = live && vhov_r(r);
    float hk      = live ? ui_hover_t(x, y, hov) : 0.0f;
    float sweep   = live ? ui_hover_sweep(x, y, hov) : 1.0f;
    int   pressed = hov && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    int   dy      = pressed ? 1 : 0;   // enfoncement du contenu
    float rnd     = (float)BTN_R / h;
    int   big     = (h >= NAV_BIG_H);
    int   ifs     = big ? 34 : 26;     // icône
    int   tfs     = big ? 22 : 16;     // titre
    int   dfs     = big ? 11 : 10;     // description / badge

    Color bg = lerp_col(C_PANEL, (Color){col.r/7, col.g/7, col.b/7, 255}, hk);
    if (pressed)
        bg = (Color){(unsigned char)(bg.r * BTN_PRESS_DIM),
                     (unsigned char)(bg.g * BTN_PRESS_DIM),
                     (unsigned char)(bg.b * BTN_PRESS_DIM), bg.a};
    float bw = 1.2f + hk * 0.8f;
    Color bc = lerp_col((Color){col.r/3, col.g/3, col.b/3, 180}, col, hk);
    Rectangle rd = {(float)x, (float)yd, (float)w, (float)h};
    DrawRectangleRounded(rd, rnd, 6, col_fade(bg, k));
    DrawRectangleRoundedLinesEx(rd, rnd, 6, bw, col_fade(bc, k));

    /* Barre latérale : s'allume progressivement */
    DrawRectangleRounded(
        (Rectangle){(float)x, (float)(yd+4), 4, (float)(h-8)},
        0.5f, 4,
        col_fade((Color){col.r, col.g, col.b,
                         (unsigned char)(110.0f + hk * 145.0f)}, k));

    if (live) draw_btn_sweep(x, y, w, h, sweep);

    /* Icône : se décale vers la droite avec rebond au survol */
    int icon_dx = (int)(3.0f * ea_out_back(hk));
    dtxt(icon, x + M_PAD + 4 + icon_dx, yd + h/2 - fh(ifs)/2 + dy, ifs,
         col_fade(col, k));

    /* Badge à droite : réduit la largeur disponible pour le texte */
    int tx    = x + M_PAD + (big ? NAV_TXT_DX_BIG : NAV_TXT_DX);
    int right = x + w - M_PAD;
    if (badge && badge[0]) {
        int bwid = mtxt(badge, dfs + 1);
        dtxt(badge, right - bwid, yd + h/2 - fh(dfs + 1)/2 + dy, dfs + 1,
             col_fade(col, k));
        right -= bwid + M_SECT;
    }
    int ty = yd + h/2 - (fh(tfs) + NAV_LINE_GAP + fh(dfs))/2 + dy;
    char tbuf[72], dbuf[96];
    clip_text(title, right - tx, tfs, tbuf, sizeof(tbuf));
    clip_text(desc,  right - tx, dfs, dbuf, sizeof(dbuf));
    dtxt(tbuf, tx, ty, tfs, col_fade(lerp_col(C_TEXT, col, hk), k));
    dtxt(dbuf, tx, ty + fh(tfs) + NAV_LINE_GAP, dfs,
         col_fade(lerp_col(NAV_DESC_COL, C_TEXT, hk), k));
    if (live && vclick_r(r)) {
        audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        return 1;
    }
    return 0;
}

// Progression d'entrée (0..1) de la carte n° idx d'un écran ouvert depuis t s.
static float hub_k(float t, int idx) {
    float d = t - (float)idx * HUB_BTN_STEP;
    return (d <= 0.0f) ? 0.0f : ea_out_cubic(d / HUB_BTN_DUR);
}

// Remise à zéro du timer d'un écran à son arrivée : la transition (menu.c)
// remet trans_t à 0 à chaque entrée d'écran ; s'il recule, on (ré)entre.
static float screen_timer(int slot, float trans_t, float *prev_trans) {
    if (trans_t < *prev_trans) ui_timer(slot, 1);
    *prev_trans = trans_t;
    return ui_timer(slot, 0);
}

// Ouvre un écran en mémorisant le retour (pile de back_screen).
static void hub_goto(MenuState *m, MenuScreen to, MenuScreen back) {
    push_back_screen(m);
    m->screen      = to;
    m->back_screen = back;
}

// Modale du premier lancement : proposer le tutoriel. Répondre sauve la
// méta (le fichier existe alors → la question ne sera plus jamais posée).
static void hub_first_run(MenuState *m, const MetaProgress *meta,
                          int vw, int vh, MenuAction *act) {
    int cx = vw/2;
    draw_bg(m, vw, vh);
    draw_header("BIENVENUE", vw);
    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, 165});

    int pw2 = 470, ph2 = 190;
    int px2 = cx - pw2/2, py2 = vh/2 - ph2/2 - 30;
    Rectangle pr = {(float)px2, (float)py2, (float)pw2, (float)ph2};
    DrawRectangleRounded(pr, 0.08f, 6, C_PANEL);
    DrawRectangleRoundedLinesEx(pr, 0.08f, 6, 1.6f, C_GOLD);

    const char *q = "PREMIÈRE FOIS ?";
    dtxt(q, cx - mtxt(q, 16)/2, py2 + 18, 16, C_GOLD);
    const char *l1 = "Apprends les bases en 2 minutes :";
    const char *l2 = "tours, vagues, minerais, pause tactique.";
    dtxt(l1, cx - mtxt(l1, 11)/2, py2 + 18 + fh(16) + 10, 11, C_TEXT);
    dtxt(l2, cx - mtxt(l2, 11)/2, py2 + 18 + fh(16) + 10 + fh(11) + 3,
         11, C_TEXT);

    int bw3 = 200, bh3 = 40;
    int byb = py2 + ph2 - bh3 - 16;
    if (draw_btn("OUI, APPRENDRE", px2 + pw2/2 - bw3 - 10, byb,
                 bw3, bh3, C_GOLD, 1)) {
        m->first_run_ask = 0;
        meta_save(meta);          /* marque « a répondu » */
        act->start_tutorial = 1;
    }
    if (draw_btn("JE CONNAIS", px2 + pw2/2 + 10, byb,
                 bw3, bh3, C_DIM, 1)) {
        m->first_run_ask = 0;
        meta_save(meta);
    }
}

// Compteur de ferraille « roulant » : la valeur affichée court vers la
// vraie valeur en ~HUB_SCRAP_ROLL s (au moins 1 unité/s).
static int hub_rolling_scrap(int target_i) {
    static float disp = -1.0f;
    float target = (float)target_i;
    if (disp < 0.0f) disp = target;                 // 1er passage
    float diff = target - disp;
    if (diff != 0.0f) {
        float speed = fabsf(diff) / HUB_SCRAP_ROLL;  // unités/s
        if (speed < 1.0f) speed = 1.0f;
        float step = speed * ui_dt();
        if (fabsf(diff) <= step) disp = target;
        else disp += (diff > 0.0f) ? step : -step;
    }
    return (int)(disp + 0.5f);
}

// Badge bestiaire : nouvelles découvertes depuis la dernière visite →
// la bordure de la carte pulse HUB_PULSE_COUNT fois puis s'arrête.
static void hub_bestiary_pulse(int nb_disc, Rectangle br) {
    static int prev_disc = -1;
    static int pulsing   = 0;
    if (prev_disc < 0) prev_disc = nb_disc;          // 1er passage : muet
    if (nb_disc > prev_disc) {
        pulsing   = 1;
        prev_disc = nb_disc;
        ui_timer(SLOT_HUB_PULSE, 1);
    }
    if (!pulsing) return;
    float pt = ui_timer(SLOT_HUB_PULSE, 0);
    if (pt >= HUB_PULSE_PERIOD * HUB_PULSE_COUNT) { pulsing = 0; return; }
    /* (1-cos)/2 : part de 0, culmine à mi-période */
    float ph = (1.0f - cosf(pt * 2.0f * PI / HUB_PULSE_PERIOD)) * 0.5f;
    DrawRectangleRoundedLinesEx(br, (float)BTN_R / br.height, 6, 2.5f,
        (Color){C_RED.r, C_RED.g, C_RED.b, (unsigned char)(ph * 200.0f)});
}

MenuAction draw_play_hub(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    if (m->first_run_ask) {               /* le hub attend la réponse */
        hub_first_run(m, meta, vw, vh, &act);
        return act;
    }
    static float prev_trans = 1e9f;
    float t = screen_timer(SLOT_HUB, m->trans_t, &prev_trans);

    draw_bg(m, vw, vh);
    draw_header("CHOISIR UN MODE", vw);

    /* Bloc centré horizontalement, un peu au-dessus du centre vertical */
    int total = HUB_CAMP_H + HUB_GAP + HUB_ROW_H + HUB_GAP + HUB_SMALL_H;
    int top   = M_PAD + 70 + M_SECT;
    int free_ = (vh - M_PAD - BTN_H - M_SECT) - top - total;
    int x     = vw/2 - HUB_W/2;
    int y     = top + (free_ > 0 ? (int)((float)free_ * HUB_V_BIAS) : 0);
    int half  = (HUB_W - HUB_GAP) / 2;

    /* ── CAMPAGNE : reprise de la run la plus avancée, actes terminés ── */
    int best_stage = -1;
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        if (m->campaign_slots[i].exists &&
            m->campaign_slots[i].campaign_stage > best_stage)
            best_stage = m->campaign_slots[i].campaign_stage;
    int acts_done = 0;
    for (int i = 0; i < CAMPAIGN_TOTAL; i++)
        if (meta->act_stars[i] > 0) acts_done++;

    char camp_desc[96], camp_badge[24];
    const ActData *ad = (best_stage >= 0) ? campaign_act_get(best_stage) : NULL;
    if (ad)
        snprintf(camp_desc, sizeof(camp_desc), "En cours : acte %d/%d — %s",
                 best_stage + 1, CAMPAIGN_TOTAL, ad->title);
    else
        snprintf(camp_desc, sizeof(camp_desc),
                 "Carte de progression — %d chapitres, %d actes.",
                 CAMPAIGN_CHAPTERS, CAMPAIGN_TOTAL);
    snprintf(camp_badge, sizeof(camp_badge), "%d/%d actes",
             acts_done, CAMPAIGN_TOTAL);
    if (draw_nav_btn("C", ad ? "CONTINUER LA CAMPAGNE" : "CAMPAGNE",
                     camp_desc, camp_badge, C_GOLD,
                     x, y, HUB_W, HUB_CAMP_H, hub_k(t, 0))) {
        m->selected_campaign_act = -1;   // aucun acte pré-sélectionné
        hub_goto(m, MENU_WORLD_MAP, MENU_PLAY_HUB);
    }
    y += HUB_CAMP_H + HUB_GAP;

    /* ── PARTIE LIBRE | MULTIJOUEUR ─────────────────────────────── */
    if (draw_nav_btn("P", "PARTIE LIBRE",
                     "Arcade, partie perso, héros 3D, tutoriel.", NULL,
                     C_BLUE, x, y, half, HUB_ROW_H, hub_k(t, 1)))
        hub_goto(m, MENU_FREE_PLAY, MENU_PLAY_HUB);
    if (draw_nav_btn("M", "MULTIJOUEUR",
                     "Course, duel, asymétrique, co-op.", "bêta",
                     C_GREEN, x + HUB_W - half, y, half, HUB_ROW_H,
                     hub_k(t, 2))) {
        hub_goto(m, MENU_MP_HUB, MENU_PLAY_HUB);
        m->mp_role = 0;
        if (m->mp_mode == MP_NONE) m->mp_mode = MP_COURSE;
    }
    y += HUB_ROW_H + HUB_GAP;

    /* ── AMÉLIORATIONS | BESTIAIRE (méta-progression) ───────────── */
    char upg_badge[24];
    snprintf(upg_badge, sizeof(upg_badge), "%d ferr.",
             hub_rolling_scrap(meta->scrap));
    if (draw_nav_btn("*", "AMÉLIORATIONS", "Bonus permanents.", upg_badge,
                     C_ORANGE, x, y, half, HUB_SMALL_H, hub_k(t, 3)))
        hub_goto(m, MENU_UPGRADES, MENU_PLAY_HUB);

    int nb_disc = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (meta->bestiary_discovered[i]) nb_disc++;
    char best_badge[16];
    snprintf(best_badge, sizeof(best_badge), "%d/%d", nb_disc, ENEMY_TYPE_COUNT);
    int bx = x + HUB_W - half;
    if (draw_nav_btn("B", "BESTIAIRE", "Ennemis, tours, butin, succès.",
                     best_badge, C_RED, bx, y, half, HUB_SMALL_H,
                     hub_k(t, 4))) {
        hub_goto(m, MENU_BESTIARY, MENU_PLAY_HUB);
        if (m->sel_bestiary < 0) m->sel_bestiary = 0;
    }
    hub_bestiary_pulse(nb_disc, (Rectangle){(float)bx, (float)y,
                                            (float)half, (float)HUB_SMALL_H});

    if (draw_back_btn(vw, vh)) {
        m->screen = m->paused ? MENU_PAUSE : m->back_screen;
        if (!m->paused) pop_back_screen(m);
    }
    return act;
}

// ════════════════════════════════════════════════════
// PARTIE LIBRE — les modes hors campagne
// ════════════════════════════════════════════════════
MenuAction draw_free_play(MenuState *m, int vw, int vh)
{
    MenuAction act = {0};
    static float prev_trans = 1e9f;
    float t = screen_timer(SLOT_FREE, m->trans_t, &prev_trans);

    draw_bg(m, vw, vh);
    draw_header("PARTIE LIBRE", vw);

    int total = 4 * FREE_CARD_H + 3 * HUB_GAP;
    int top   = M_PAD + 70 + M_SECT;
    int free_ = (vh - M_PAD - BTN_H - M_SECT) - top - total;
    int x     = vw/2 - HUB_W/2;
    int y     = top + (free_ > 0 ? (int)((float)free_ * HUB_V_BIAS) : 0);

    if (draw_nav_btn("A", "ARCADE",
                     "Choisissez un environnement et jouez sans fin.", NULL,
                     C_BLUE, x, y, HUB_W, FREE_CARD_H, hub_k(t, 0)))
        hub_goto(m, MENU_ARCADE, MENU_FREE_PLAY);
    y += FREE_CARD_H + HUB_GAP;

    if (draw_nav_btn("X", "PARTIE PERSO",
                     "Carte, spawns, bases, terrain et difficulté sur mesure.",
                     NULL, C_ORANGE, x, y, HUB_W, FREE_CARD_H, hub_k(t, 1)))
        hub_goto(m, MENU_CUSTOM, MENU_FREE_PLAY);
    y += FREE_CARD_H + HUB_GAP;

    if (draw_nav_btn("H", "MODE HÉROS 3D",
                     "Incarnez un héros sur le terrain : tirez, recrutez, bâtissez.",
                     "bêta", HUB_COL_HERO, x, y, HUB_W, FREE_CARD_H,
                     hub_k(t, 2)))
        act.start_hero = 1;       // lancé par app.c
    y += FREE_CARD_H + HUB_GAP;

    if (draw_nav_btn("T", "TUTORIEL",
                     "Apprenez les bases : tours, vagues, minerais, pause, zoom.",
                     NULL, HUB_COL_TUTO, x, y, HUB_W, FREE_CARD_H, hub_k(t, 3)))
        act.start_tutorial = 1;   // lancé par app.c

    if (draw_back_btn(vw, vh)) {
        m->screen = m->back_screen;
        pop_back_screen(m);
    }
    return act;
}

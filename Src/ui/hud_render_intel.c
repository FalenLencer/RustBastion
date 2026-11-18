/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_intel.c ─ Panneau droit AU REPOS : renseignement de vague.
 *
 *  Rien de sélectionné → au lieu d'une invite vide, ce qu'il faut savoir
 *  pour décider : en préparation, la composition probable de la PROCHAINE
 *  vague (portraits ; « ? » tant qu'un ennemi n'est pas découvert en
 *  campagne) et le contre conseillé ; pendant la vague, les ennemis encore
 *  en vie par type, et le boss avec ses PV. En bas : les raccourcis.
 *  (Remplace le panneau d'aperçu qui flottait sur la carte au-dessus du
 *  bouton LANCER.)
 */

#include "hud_internal.h"
#include "ui_anim.h"                 // ui_dt / ea_out_back
#include "ui_palette.h"
#include "ui_plate.h"
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

#define INTEL_MAX_CARDS   6
#define INTEL_CARD_GAP    6
#define INTEL_ICON_H     40      /* feuillure du portrait d'ennemi (px)        */
#define INTEL_NAME_FS     8
#define INTEL_LINE_FS     9
#define INTEL_POP_STEP    0.05f  /* cascade d'apparition des cartes (s)        */
#define INTEL_POP_DUR     0.20f
#define INTEL_TAG_FS      8
#define INTEL_BOSS_BAR_H  7
#define INTEL_FIGHT_COL   ((Color){215,  85,  60, 255})
#define INTEL_TAG_COL     ((Color){ 90, 190, 210, 255})
#define INTEL_TXT_COL     ((Color){205, 190, 160, 255})

/* Carte d'ennemi : portrait (ou « ? »), nom, compteur « x5 » si count > 0.
   k : avancement de l'apparition (≤ 0 : pas encore, 1 : en place). */
static void enemy_card(int x, int y, int w, EnemyType t, int known, int count, float k) {
    if (k <= 0.0f) return;
    float a  = k > 1.0f ? 1.0f : k;
    Color ec = renderer_enemy_color(t);
    Rectangle r = {(float)x, (float)y, (float)w, (float)INTEL_ICON_H};
    plate_alpha(a);
    plate_draw(r, PLATE_CHAMFER_S);
    Rectangle in = plate_inset(r, 3.0f);
    plate_chamfer_fill(in, PLATE_CHAMFER_S - 2.0f,
                       (Color){(unsigned char)(PLATE_WELL.r + ec.r * 0.18f),
                               (unsigned char)(PLATE_WELL.g + ec.g * 0.18f),
                               (unsigned char)(PLATE_WELL.b + ec.b * 0.18f), 255});
    plate_alpha(1.0f);
    unsigned char ua = (unsigned char)(255.0f * a);
    float s = 0.6f + 0.4f * ea_out_back(a);            /* pop : 0.6 → 1 */
    if (known && g_enemy_splash[t].id != 0) {
        Texture2D tx = g_enemy_splash[t];
        float sc = fminf((in.width - 2.0f) / (float)tx.width, (in.height - 2.0f) / (float)tx.height) * s;
        float dw = (float)tx.width * sc, dh = (float)tx.height * sc;
        DrawTexturePro(tx, (Rectangle){0, 0, (float)tx.width, (float)tx.height},
                       (Rectangle){in.x + (in.width - dw) / 2, in.y + (in.height - dh) / 2, dw, dh},
                       (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, ua});
    } else if (known) {
        DrawCircle((int)(in.x + in.width / 2), (int)(in.y + in.height / 2), in.height * 0.3f * s,
                   (Color){ec.r, ec.g, ec.b, ua});
    } else {
        int qw = mtxt("?", 16);
        dtxt("?", (int)(in.x + in.width / 2) - qw / 2, (int)(in.y + (in.height - fh(16)) / 2), 16,
             (Color){ec.r, ec.g, ec.b, ua});
    }
    if (count > 0) {                                   /* effectif encore en vie */
        char cb[12];
        snprintf(cb, sizeof(cb), "x%d", count);
        int cw = mtxt(cb, INTEL_TAG_FS) + 6, chh = fh(INTEL_TAG_FS) + 1;
        int cx = (int)(in.x + in.width) - cw, cy = (int)in.y;
        DrawRectangle(cx, cy, cw, chh, (Color){0, 0, 0, (unsigned char)(210.0f * a)});
        dtxt(cb, cx + 3, cy, INTEL_TAG_FS, (Color){240, 225, 200, ua});
    }
    char nb[24];
    clip_text(known ? ENEMY_BASE_STATS[t].name : "Inconnu", w, INTEL_NAME_FS, nb, sizeof(nb));
    Color nc = known ? INTEL_TXT_COL : HUD_COL_GREY;
    nc.a = ua;
    dtxt(nb, x + w / 2 - mtxt(nb, INTEL_NAME_FS) / 2, y + INTEL_ICON_H + 2, INTEL_NAME_FS, nc);
}

/* Étiquette colorée (« CONTRE », « BOSS ») suivie d'un texte ; retourne la
   hauteur de la ligne. */
static int tag_line(int x, int y, int w, const char *tag, Color tc, const char *txt, Color c) {
    int tw = mtxt(tag, INTEL_TAG_FS) + 8, th = fh(INTEL_TAG_FS) + 3;
    DrawRectangle(x, y, tw, th, tc);
    dtxt(tag, x + 4, y + 1, INTEL_TAG_FS, (Color){12, 10, 8, 255});
    if (txt) {
        char cb[80];
        clip_text(txt, w - tw - 8, INTEL_LINE_FS + 1, cb, sizeof(cb));
        dtxt(cb, x + tw + 8, y + (th - fh(INTEL_LINE_FS + 1)) / 2, INTEL_LINE_FS + 1, c);
    }
    return th;
}

/* Raccourcis en bas du panneau : autant que la largeur en permet. */
static void key_hints(int x, int y, int w, int in_prep) {
    static const char *const KP[][2] = {
        {"1-9", "Construire"}, {"ENTRÉE", "Lancer"}, {"ALT", "Portées"}, {"H", "Aide"}, {"ESPACE", "Pause"}};
    static const char *const KW[][2] = {
        {"1-9", "Construire"}, {"X", "Vitesse"}, {"ALT", "Portées"}, {"H", "Aide"}, {"ESPACE", "Pause"}};
    const char *const (*k)[2] = in_prep ? KP : KW;
    const int n = (int)(sizeof(KP) / sizeof(KP[0])), sp = 10, end = x + w;
    for (int i = 0; i < n; i++) {
        int hw = plate_key_hint(0, 0, k[i][0], k[i][1], 0);
        if (x + hw > end) break;
        plate_key_hint(x, y, k[i][0], k[i][1], 1);
        x += hw + sp;
    }
}

void hud_render_intel(const UIState *ui, const GameState *gs, int rx, int max_w) {
    HUD_FRAME_LOCALS;
    int in_prep = (gs->phase == PHASE_PREP);
    int y = HUD_Y + M;
    int cw = (max_w - (INTEL_MAX_CARDS - 1) * INTEL_CARD_GAP) / INTEL_MAX_CARDS;
    char b[96], r[48];

    /* Cascade d'apparition relancée à chaque changement de vague / phase */
    static int   key_prev = -1;
    static float pop_t    = 0.0f;
    int key = gs->wave_manager.number * 2 + in_prep;
    if (key != key_prev) { key_prev = key; pop_t = 0.0f; }
    pop_t += ui_dt();

    if (in_prep && gs->wave_manager.lock_manual) {
        /* Asym : la vague est composée par la horde adverse */
        plate_section(rx, y, max_w, "EN ATTENTE DE LA HORDE", INTEL_FIGHT_COL);
        dtxt("La prochaine vague est choisie par l'adversaire.", rx, y + HUD_SEC_H,
             INTEL_LINE_FS, INTEL_TXT_COL);
    } else if (in_prep) {
        int   boss = game_next_wave_is_boss(gs);
        Color sc   = boss ? UI_DANGER : UI_ACCENT;
        snprintf(b, sizeof(b), boss ? "VAGUE %d  -  BOSS EN APPROCHE" : "PROCHAINE VAGUE : %d",
                 gs->wave_manager.number + 1);
        plate_section(rx, y, max_w, b, sc);
        snprintf(r, sizeof(r), "dans %.0f s", gs->wave_manager.prep_timer);
        dtxt(r, rx + max_w - mtxt(r, INTEL_LINE_FS), y, INTEL_LINE_FS, INTEL_TXT_COL);
        y += HUD_SEC_H;

        EnemyType pv[ENEMY_TYPE_COUNT];
        int max_stage = gs->is_campaign ? campaign_difficulty_stage(gs->campaign_stage) - 1 : 9999;
        int npv = wave_preview_types(gs->wave_manager.number + 1, gs->map.theme, gs->is_campaign,
                                     max_stage, gs->wave_manager.arcade_bias, pv, ENEMY_TYPE_COUNT);
        if (npv > INTEL_MAX_CARDS) npv = INTEL_MAX_CARDS;
        int unknown = 0;
        for (int k = 0; k < npv; k++) {
            int known = !gs->is_campaign || gs->meta.bestiary_discovered[pv[k]];
            unknown |= !known;
            enemy_card(rx + k * (cw + INTEL_CARD_GAP), y, cw, pv[k], known, 0,
                       (pop_t - INTEL_POP_STEP * (float)k) / INTEL_POP_DUR);
        }
        y += INTEL_ICON_H + 2 + fh(INTEL_NAME_FS) + 6;

        /* Contre conseillé (même analyse que l'infobulle de pose) : le
           préfixe « V4 Brute, Raider : » est redondant avec les cartes. */
        char adv[80];
        if (hud_counter_advice(gs, adv, sizeof(adv))) {
            const char *p = strstr(adv, " : ");
            y += tag_line(rx, y, max_w, "CONTRE", INTEL_TAG_COL, p ? p + 3 : adv, UI_INFO) + 6;
        } else {
            y += tag_line(rx, y, max_w, "CONTRE", HUD_COL_GREY,
                          unknown ? "Ennemis inconnus : observez-les au combat"
                                  : "Pas de faiblesse marquée : variez les tours",
                          HUD_COL_GREY) + 6;
        }
        dtxt("Survolez un ennemi : ses faiblesses. Cliquez une tour : ses ordres.",
             rx, y + 2, INTEL_NAME_FS, HUD_COL_GREY);
    } else {
        /* ── Vague en cours : ennemis en vie par type, boss ── */
        int cnt[ENEMY_TYPE_COUNT] = {0}, alive = 0;
        const Enemy *boss = NULL;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &gs->enemies.enemies[i];
            if (!e->active || e->dead) continue;
            if ((unsigned)e->type < (unsigned)ENEMY_TYPE_COUNT) cnt[e->type]++;
            alive++;
            if (e->is_boss && !boss) boss = e;
        }
        int to_spawn = gs->wave_manager.total_to_spawn - gs->wave_manager.total_spawned;
        if (to_spawn < 0) to_spawn = 0;
        snprintf(b, sizeof(b), "VAGUE %d EN COURS", gs->wave_manager.number);
        plate_section(rx, y, max_w, b, INTEL_FIGHT_COL);
        if (to_spawn > 0) snprintf(r, sizeof(r), "%d en vie  +%d à venir", alive, to_spawn);
        else              snprintf(r, sizeof(r), "%d en vie", alive);
        dtxt(r, rx + max_w - mtxt(r, INTEL_LINE_FS), y, INTEL_LINE_FS, INTEL_TXT_COL);
        y += HUD_SEC_H;

        /* Types triés par effectif décroissant */
        int order[ENEMY_TYPE_COUNT], n = 0;
        for (int t = 0; t < ENEMY_TYPE_COUNT; t++)
            if (cnt[t] > 0) order[n++] = t;
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && cnt[order[j]] > cnt[order[j - 1]]; j--) {
                int tmp = order[j]; order[j] = order[j - 1]; order[j - 1] = tmp;
            }
        if (n > INTEL_MAX_CARDS) n = INTEL_MAX_CARDS;
        for (int k = 0; k < n; k++)
            enemy_card(rx + k * (cw + INTEL_CARD_GAP), y, cw, (EnemyType)order[k],
                       !gs->is_campaign || gs->meta.bestiary_discovered[order[k]], cnt[order[k]],
                       (pop_t - INTEL_POP_STEP * (float)k) / INTEL_POP_DUR);
        if (n == 0)
            dtxt(to_spawn > 0 ? "Les premiers ennemis arrivent..." : "Secteur dégagé.",
                 rx, y + INTEL_ICON_H / 2 - fh(INTEL_LINE_FS) / 2, INTEL_LINE_FS + 1, INTEL_TXT_COL);
        y += INTEL_ICON_H + 2 + fh(INTEL_NAME_FS) + 6;

        if (boss && boss->max_hp > 0.0f) {             /* PV du boss */
            float hr = boss->hp / boss->max_hp;
            int th = tag_line(rx, y, max_w, "BOSS", UI_DANGER, NULL, UI_DANGER);
            int bx = rx + mtxt("BOSS", INTEL_TAG_FS) + 16;
            hud_draw_bar(bx, y + (th - INTEL_BOSS_BAR_H) / 2, rx + max_w - bx - 44,
                         INTEL_BOSS_BAR_H, hr, UI_DANGER, (Color){24, 10, 8, 255});
            snprintf(r, sizeof(r), boss->boss_shield > 0.0f ? "BOUCLIER" : "%d %%",
                     (int)(hr * 100.0f + 0.5f));
            dtxt(r, rx + max_w - mtxt(r, INTEL_LINE_FS), y + (th - fh(INTEL_LINE_FS)) / 2,
                 INTEL_LINE_FS, boss->boss_shield > 0.0f ? UI_INFO : INTEL_TXT_COL);
        }
    }

    key_hints(rx, HUD_Y + HUD_H - HUD_SEC_Y - (fh(9) + 6), max_w, in_prep);
}

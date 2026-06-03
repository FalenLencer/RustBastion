/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_bestiary.c ─ Écran Bestiaire.
 *
 *  Contient :
 *    draw_bestiary   — 5 onglets : Ennemis, Minerais, Tours, Unités, Butin
 *                      (le dernier référence les perks de run & la boutique)
 *
 *  Helpers locaux (statiques) :
 *    draw_multiline  — Rendu de texte multiligne séparé par \n
 *    draw_res_bar    — Barre de résistance/faiblesse aux dégâts
 */

#include "menu_internal.h"
#include "../game/runperks.h"   // catalogue de perks (onglet Butin)
#include "perk_art.h"           // emblèmes pixel-art des perks

// Couleurs des minerais : on réutilise la source unique MATERIAL_COLORS
// (renderer.h) au lieu d'une copie divergente.

static const Color TOWER_COLORS[TOWER_TYPE_COUNT] = {
    {215, 155,  40, 255},  // Gun    — or
    { 60, 200, 100, 255},  // Sniper — vert
    {220,  80,  40, 255},  // Flame  — rouge
    { 60, 140, 220, 255},  // Tesla  — bleu
};

static const Color UNIT_COLORS[UNIT_TYPE_COUNT] = {
    { 80, 160,  80, 255},  // Soldier — vert kaki
    {150, 150, 165, 255},  // Heavy   — gris acier
    {220, 215, 200, 255},  // Medic   — blanc ivoire
    {180, 130,  65, 255},  // Dog     — tan
    {200, 140,  40, 255},  // Worker  — orange
};

// ── Helpers locaux ────────────────────────────────────────────────

// Texte multiligne (séparateurs \n) — incrémente *py à chaque ligne
static void draw_multiline(const char *s, int x, int *py, int fs, Color col, int line_h) {
    char buf[256];
    int  bi = 0;
    for (int i = 0; ; i++) {
        if (s[i] == '\n' || s[i] == '\0') {
            buf[bi] = '\0';
            if (bi > 0) dtxt(buf, x, *py, fs, col);
            *py += line_h;
            bi = 0;
            if (s[i] == '\0') break;
        } else if (bi < (int)sizeof(buf)-1) {
            buf[bi++] = s[i];
        }
    }
}

// Barre de résistance : vert = faiblesse, bleu = résistance, gris = neutre
static void draw_res_bar(const char *label, float mult,
                         int x, int y, int bar_w, int bar_h) {
    Color bar_col;
    if      (mult >= 1.25f) bar_col = (Color){220,  80,  50, 255};
    else if (mult >= 1.05f) bar_col = (Color){215, 155,  30, 255};
    else if (mult <= 0.6f)  bar_col = (Color){ 40, 120, 200, 255};
    else if (mult <= 0.9f)  bar_col = (Color){ 80, 160, 220, 255};
    else                    bar_col = (Color){ 80,  70,  45, 255};

    DrawRectangle(x, y, bar_w, bar_h, (Color){20, 14, 5, 200});

    float clamped = mult > 2.0f ? 2.0f : mult;
    int fill_w = (int)(clamped * bar_w / 2.0f);
    if (fill_w > bar_w) fill_w = bar_w;
    DrawRectangle(x, y, fill_w, bar_h, bar_col);

    DrawLine(x + bar_w/2, y, x + bar_w/2, y + bar_h, (Color){120, 100, 55, 180});

    int fs = 9;
    dtxt(label, x + 3, y + bar_h/2 - fh(fs)/2, fs, (Color){200, 180, 120, 255});
    char muls[20];
    if      (mult >= 1.25f) snprintf(muls, sizeof(muls), "x%.1f FAIBLE",  mult);
    else if (mult <= 0.75f) snprintf(muls, sizeof(muls), "x%.1f RESISTE", mult);
    else                    snprintf(muls, sizeof(muls), "x%.1f",          mult);
    int tw = mtxt(muls, fs);
    dtxt(muls, x + bar_w - tw - 3, y + bar_h/2 - fh(fs)/2, fs, (Color){200, 180, 120, 255});
}

// ════════════════════════════════════════════════════
// BESTIAIRE
// ════════════════════════════════════════════════════
MenuAction draw_bestiary(MenuState *m, const MetaProgress *meta,
                         int vw, int vh)
{
    MenuAction act = {0};
    draw_bg(m, vw, vh);
    draw_header("BESTIAIRE", vw);

    // Compteurs de découverte
    int nb_enemy_disc = 0;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (meta->bestiary_discovered[i]) nb_enemy_disc++;
    int nb_mat_disc = 0;
    for (int i = 0; i < MAT_COUNT; i++)
        if (meta->material_discovered[i]) nb_mat_disc++;
    int nb_tower_disc = 0;
    for (int i = 0; i < META_TOWER_COUNT; i++)
        if (meta->tower_discovered[i]) nb_tower_disc++;

    // ── Onglets ──────────────────────────────────────────────────
    int tab_w = 122, tab_h = 26, tab_gap = 6;
    int tabs_total = 5 * tab_w + 4 * tab_gap;
    int tab_x0 = vw/2 - tabs_total/2;
    int tab_y  = M_PAD + 86;

    for (int t = 0; t < 5; t++) {
        int tx  = tab_x0 + t * (tab_w + tab_gap);
        int sel = (m->bestiary_tab == t);
        Rectangle tr = {(float)tx, (float)tab_y, (float)tab_w, (float)tab_h};
        int hov = vhov_r(tr);

        Color bg  = sel ? (Color){28, 18, 5, 255} : (hov ? C_HOV : C_PANEL);
        Color brd = sel ? C_GOLD : (hov ? C_TEXT : C_BORDER);
        float bw  = sel ? 2.0f : 1.0f;
        DrawRectangleRounded(tr, (float)BTN_R/tab_h, 5, bg);
        DrawRectangleRoundedLinesEx(tr, (float)BTN_R/tab_h, 5, bw, brd);

        char lbuf[32];
        switch (t) {
            case 0: snprintf(lbuf, sizeof(lbuf), "ENNEMIS %d/%d",  nb_enemy_disc, ENEMY_TYPE_COUNT); break;
            case 1: snprintf(lbuf, sizeof(lbuf), "MINERAIS %d/%d", nb_mat_disc,   MAT_COUNT);        break;
            case 2: snprintf(lbuf, sizeof(lbuf), "TOURS %d/%d",    nb_tower_disc, META_TOWER_COUNT); break;
            case 3: snprintf(lbuf, sizeof(lbuf), "UNITES");                                           break;
            default:snprintf(lbuf, sizeof(lbuf), "BUTIN %d", PERK_COUNT);                             break;
        }
        int tw2 = mtxt(lbuf, 11);
        dtxt(lbuf, tx + tab_w/2 - tw2/2, tab_y + tab_h/2 - fh(11)/2, 11,
             sel ? C_GOLD : C_TEXT);

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m->bestiary_tab = t;
            audio_play_sfx(AUDIO_SFX_MENU_CLICK);
        }
    }

    // ── Panneaux liste + détails ─────────────────────────────────
    int list_w   = 260;
    int list_x   = M_PAD;
    int list_y   = tab_y + tab_h + M_IN;
    int list_h   = vh - list_y - M_PAD - BTN_H - M_IN;
    int detail_x = list_x + list_w + M_IN;
    int detail_y = list_y;
    int detail_w = vw - detail_x - M_PAD;
    int detail_h = list_h;

    DrawRectangleRounded(
        (Rectangle){(float)list_x, (float)list_y, (float)list_w, (float)list_h},
        4.0f/list_h, 5, (Color){10, 6, 2, 220});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)list_x, (float)list_y, (float)list_w, (float)list_h},
        4.0f/list_h, 5, 1.2f, C_BORDER);
    DrawRectangleRounded(
        (Rectangle){(float)detail_x, (float)detail_y, (float)detail_w, (float)detail_h},
        4.0f/detail_h, 5, (Color){10, 6, 2, 220});
    DrawRectangleRoundedLinesEx(
        (Rectangle){(float)detail_x, (float)detail_y, (float)detail_w, (float)detail_h},
        4.0f/detail_h, 5, 1.2f, C_BORDER);

    // ════════════════════════════════════════════════
    if (m->bestiary_tab == 0) {
    // ── Onglet ENNEMIS ───────────────────────────────────────────

        // Liste gauche
        {
            int entry_h = 34, gap = 4;
            int ey = list_y + M_IN;
            for (int i = 0; i < ENEMY_TYPE_COUNT; i++) {
                int disc = meta->bestiary_discovered[i];
                int sel  = (m->sel_bestiary == i);
                Rectangle r = {(float)(list_x + 4), (float)ey,
                               (float)(list_w - 8), (float)entry_h};
                Color bg  = sel  ? (Color){30, 20, 6, 255} :
                            disc ? (Color){18, 12, 4, 200} : (Color){ 8, 5, 2, 180};
                Color brd = sel  ? C_GOLD :
                            disc ? C_BORDER : (Color){28, 20, 8, 160};
                DrawRectangleRounded(r, 3.0f/entry_h, 4, bg);
                DrawRectangleRoundedLinesEx(r, 3.0f/entry_h, 4, sel ? 2.0f : 1.0f, brd);
                int tx = (int)r.x + M_IN;
                int ty = (int)r.y + entry_h/2 - fh(11)/2;
                if (disc) {
                    dtxt(ENEMY_BASE_STATS[i].name, tx, ty, 11, sel ? C_GOLD : C_TEXT);
                    dtxt("*", tx + list_w - 28, ty, 10,
                         sel ? C_GOLD : (Color){100, 80, 40, 200});
                } else {
                    dtxt("???", tx, ty, 11, C_DIM);
                }
                if (vclick_r(r)) {
                    m->sel_bestiary = i;
                    audio_play_sfx(AUDIO_SFX_MENU_CLICK);
                }
                ey += entry_h + gap;
                if (ey + entry_h > list_y + list_h - M_IN) break;
            }
        }

        // Détails droite
        {
            int si = m->sel_bestiary;
            if (si < 0) si = 0;
            if (si >= ENEMY_TYPE_COUNT) si = ENEMY_TYPE_COUNT - 1;
            int disc = meta->bestiary_discovered[si];
            int px = detail_x + M_IN + 4;
            int py = detail_y + M_IN;
            int pw = detail_w - M_IN * 2 - 4;

            if (!disc) {
                txt_c("???", detail_x + detail_w/2, detail_y + detail_h/2 - 20, 24, C_DIM);
                txt_c("Rencontrez cet ennemi en campagne.",
                      detail_x + detail_w/2, detail_y + detail_h/2 + 10, 10, C_DIM);
            } else {
                int splash_sz = 180;
                Texture2D *spl = &g_enemy_splash[si];
                if (spl->id != 0) {
                    int sx = detail_x + detail_w - splash_sz - M_IN;
                    DrawTexturePro(*spl,
                        (Rectangle){0, 0, (float)spl->width, (float)spl->height},
                        (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                    (float)splash_sz, (float)splash_sz},
                        (Vector2){0,0}, 0.0f, WHITE);
                    DrawRectangleLinesEx(
                        (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                    (float)splash_sz, (float)splash_sz},
                        1.5f, C_BORDER);
                }
                int sep_w = (spl->id != 0) ? (pw - splash_sz - M_IN) : pw;

                dtxt(ENEMY_BASE_STATS[si].name, px, py, 18, C_GOLD);
                py += fh(18) + 4;

                char rew[32];
                snprintf(rew, sizeof(rew), "%d or  |  Vit: %.1f",
                         ENEMY_BASE_STATS[si].reward, ENEMY_BASE_STATS[si].speed);
                dtxt(rew, px, py, 10, C_TEXT);
                py += fh(10) + M_IN;

                {
                    int bar_w = pw - (spl->id != 0 ? splash_sz + M_IN : 0);
                    if (bar_w < 80) bar_w = 80;
                    dtxt("PV de base :", px, py, 9, C_DIM);
                    py += fh(9) + 2;
                    DrawRectangle(px, py, bar_w, 10, (Color){20,14,5,200});
                    DrawRectangle(px, py, bar_w, 10, (Color){60, 180, 80, 200});
                    char hps[24];
                    snprintf(hps, sizeof(hps), "%.0f", ENEMY_BASE_STATS[si].hp);
                    dtxt(hps, px + 3, py - 1, 8, WHITE);
                    py += 14;
                }

                draw_sep(px, py, sep_w, C_BORDER); py += M_IN + 2;
                dtxt("Description :", px, py, 10, C_GOLD); py += fh(10) + 3;
                if (ENEMY_DESC[si])
                    draw_multiline(ENEMY_DESC[si], px, &py, 9,
                                   (Color){160, 145, 100, 255}, fh(9) + 3);

                py += M_IN;
                draw_sep(px, py, sep_w, C_BORDER); py += M_IN + 2;
                dtxt("Specialite :", px, py, 10, C_GOLD); py += fh(10) + 3;
                if (ENEMY_SPEC[si])
                    draw_multiline(ENEMY_SPEC[si], px, &py, 9,
                                   (Color){160, 145, 100, 255}, fh(9) + 3);

                py += M_IN;
                draw_sep(px, py, sep_w, C_BORDER); py += M_IN + 2;
                dtxt("Resistances :", px, py, 10, C_GOLD); py += fh(10) + 4;

                static const char *DMG_LABELS[DAMAGE_TYPE_COUNT] = {
                    "Balles (Pistolet/Sniper)",
                    "Flammes (Lance-flammes)",
                    "Electricite (Tesla)",
                    "Cryogenique",
                    "Nano",
                };
                int bar_w2 = pw - 4;
                if (bar_w2 > 300) bar_w2 = 300;
                int bar_h2 = 18, bar_gap = 5;
                for (int d = 0; d < DAMAGE_TYPE_COUNT; d++) {
                    if (py + bar_h2 > detail_y + detail_h - M_IN) break;
                    draw_res_bar(DMG_LABELS[d], ENEMY_DMG_MULT[si][d],
                                 px, py, bar_w2, bar_h2);
                    py += bar_h2 + bar_gap;
                }
            }
        }

    } else if (m->bestiary_tab == 1) {
    // ── Onglet MINERAIS ──────────────────────────────────────────

        {
            int entry_h = 40, gap = 6;
            int ey = list_y + M_IN;
            for (int i = 0; i < MAT_COUNT; i++) {
                int disc = meta->material_discovered[i];
                int sel  = (m->sel_material == i);
                Color mc = MATERIAL_COLORS[i];
                Rectangle r = {(float)(list_x+4),(float)ey,(float)(list_w-8),(float)entry_h};
                Color bg  = sel  ? (Color){mc.r/8, mc.g/8, mc.b/8, 255} :
                            disc ? (Color){18, 12, 4, 200} : (Color){8,5,2,180};
                Color brd = sel ? mc : disc ? C_BORDER : (Color){28,20,8,160};
                DrawRectangleRounded(r, 3.0f/entry_h, 4, bg);
                DrawRectangleRoundedLinesEx(r, 3.0f/entry_h, 4, sel ? 2.0f : 1.0f, brd);
                int tx = (int)r.x + M_IN, ty = (int)r.y + entry_h/2 - fh(11)/2;
                if (disc) {
                    DrawRectangleRounded(
                        (Rectangle){(float)tx,(float)((int)r.y+entry_h/2-6),12,12},
                        0.3f, 4, (Color){mc.r,mc.g,mc.b,200});
                    dtxt(MATERIAL_NAMES[i], tx+18, ty, 11, sel ? mc : C_TEXT);
                    char dbuf[48];
                    clip_text(MATERIAL_DESC[i], list_w-28, 9, dbuf, sizeof(dbuf));
                    dtxt(dbuf, tx+18, (int)r.y+entry_h/2+3, 9, C_DIM);
                } else { dtxt("???", tx, ty, 11, C_DIM); }
                if (vclick_r(r)) { m->sel_material = i; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
                ey += entry_h + gap;
            }
        }
        {
            int si = m->sel_material < 0 ? 0 : m->sel_material >= MAT_COUNT ? MAT_COUNT-1 : m->sel_material;
            int disc = meta->material_discovered[si];
            int px = detail_x+M_IN+4, py = detail_y+M_IN, pw = detail_w-M_IN*2-4;
            Color mc = MATERIAL_COLORS[si];
            if (!disc) {
                txt_c("???", detail_x+detail_w/2, detail_y+detail_h/2-20, 24, C_DIM);
                txt_c("Collectez ce minerai en campagne.",
                      detail_x+detail_w/2, detail_y+detail_h/2+10, 10, C_DIM);
            } else {
                int icon_sz = 48;
                DrawRectangleRounded((Rectangle){(float)px,(float)py,(float)icon_sz,(float)icon_sz},
                    0.25f,6,(Color){mc.r/5,mc.g/5,mc.b/5,255});
                DrawRectangleRoundedLinesEx((Rectangle){(float)px,(float)py,(float)icon_sz,(float)icon_sz},
                    0.25f,6,2.0f,mc);
                char ini[2] = {MATERIAL_NAMES[si][0],'\0'};
                dtxt(ini, px+icon_sz/2-mtxt(ini,22)/2, py+icon_sz/2-fh(22)/2, 22, mc);
                dtxt(MATERIAL_NAMES[si], px+icon_sz+M_IN, py+4, 20, mc);
                dtxt("Minerai collectible", px+icon_sz+M_IN, py+4+fh(20)+2, 9, C_DIM);
                py += icon_sz+M_IN;
                draw_sep(px,py,pw,C_BORDER); py += M_IN+2;
                dtxt("Effet :", px, py, 10, C_GOLD); py += fh(10)+3;
                dtxt(MATERIAL_DESC[si], px+M_IN, py, 11, (Color){mc.r,mc.g,mc.b,220});
                py += fh(11)+M_IN+2;
                draw_sep(px,py,pw,C_BORDER); py += M_IN+2;
                dtxt("Description :", px, py, 10, C_GOLD); py += fh(10)+3;
                if (MATERIAL_LORE[si])
                    draw_multiline(MATERIAL_LORE[si], px, &py, 9,
                                   (Color){160,145,100,255}, fh(9)+3);
                py += M_IN;
                draw_sep(px,py,pw,C_BORDER); py += M_IN+2;
                dtxt("Type de degats :", px, py, 10, C_GOLD); py += fh(10)+3;
                if (si < 5) dtxt(DAMAGE_NAMES[si], px+M_IN, py, 11, (Color){mc.r,mc.g,mc.b,220});
            }
        }

    } else if (m->bestiary_tab == 2) {
    // ── Onglet TOURS ─────────────────────────────────────────────

        // Liste gauche
        {
            int entry_h = 44, gap = 6;
            int ey = list_y + M_IN;
            for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
                int disc = meta->tower_discovered[i];
                int sel  = (m->sel_tower == i);
                Color tc = TOWER_COLORS[i];
                Rectangle r = {(float)(list_x+4),(float)ey,(float)(list_w-8),(float)entry_h};
                Color bg  = sel  ? (Color){tc.r/8, tc.g/8, tc.b/8, 255} :
                            disc ? (Color){18, 12, 4, 200} : (Color){8,5,2,180};
                Color brd = sel ? tc : disc ? C_BORDER : (Color){28,20,8,160};
                DrawRectangleRounded(r, 3.0f/entry_h, 4, bg);
                DrawRectangleRoundedLinesEx(r, 3.0f/entry_h, 4, sel ? 2.0f : 1.0f, brd);
                int tx = (int)r.x + M_IN, ty = (int)r.y + entry_h/2 - fh(11)/2;
                if (disc) {
                    DrawRectangleRounded(
                        (Rectangle){(float)tx,(float)((int)r.y+entry_h/2-6),12,12},
                        0.3f, 4, (Color){tc.r,tc.g,tc.b,200});
                    dtxt(TOWER_BASE_STATS[i].name, tx+18, ty, 11, sel ? tc : C_TEXT);
                    char dbuf[48];
                    clip_text(TOWER_BASE_STATS[i].description, list_w-28, 9, dbuf, sizeof(dbuf));
                    dtxt(dbuf, tx+18, (int)r.y+entry_h/2+3, 9, C_DIM);
                } else { dtxt("???", tx, ty, 11, C_DIM); }
                if (vclick_r(r)) { m->sel_tower = i; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
                ey += entry_h + gap;
            }
        }

        // Détails droite
        {
            int si = m->sel_tower < 0 ? 0 : m->sel_tower >= TOWER_TYPE_COUNT ? TOWER_TYPE_COUNT-1 : m->sel_tower;
            int disc = meta->tower_discovered[si];
            int px = detail_x+M_IN+4, py = detail_y+M_IN, pw = detail_w-M_IN*2-4;
            Color tc = TOWER_COLORS[si];
            if (!disc) {
                txt_c("???", detail_x+detail_w/2, detail_y+detail_h/2-20, 24, C_DIM);
                txt_c("Posez cette tour en campagne pour la decouvrir.",
                      detail_x+detail_w/2, detail_y+detail_h/2+10, 10, C_DIM);
            } else {
                int splash_sz = 180;
                Texture2D *spl = &g_tower_splash[si];
                if (spl->id != 0) {
                    int sx = detail_x + detail_w - splash_sz - M_IN;
                    DrawTexturePro(*spl,
                        (Rectangle){0, 0, (float)spl->width, (float)spl->height},
                        (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                    (float)splash_sz, (float)splash_sz},
                        (Vector2){0,0}, 0.0f, WHITE);
                    DrawRectangleLinesEx(
                        (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                    (float)splash_sz, (float)splash_sz},
                        1.5f, C_BORDER);
                }
                int sep_w = (spl->id != 0) ? (pw - splash_sz - M_IN) : pw;

                dtxt(TOWER_BASE_STATS[si].name, px, py, 20, tc);
                py += fh(20) + 2;
                dtxt("Tour de defense", px, py, 9, C_DIM);
                py += fh(9) + M_IN;
                draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

                int cx2 = px + sep_w/2;
                dtxt("Cout :", px, py, 9, C_DIM);
                dtxt(TextFormat("%d or", TOWER_BASE_STATS[si].cost), px+50, py, 10, tc);
                dtxt("Degats :", cx2, py, 9, C_DIM);
                dtxt(TextFormat("%.0f", TOWER_BASE_STATS[si].damage), cx2+56, py, 10, tc);
                py += fh(10)+3;
                dtxt("Portee :", px, py, 9, C_DIM);
                dtxt(TextFormat("%.1f cases", TOWER_BASE_STATS[si].range), px+50, py, 10, tc);
                dtxt("Cadence :", cx2, py, 9, C_DIM);
                dtxt(TextFormat("%.1f/s", TOWER_BASE_STATS[si].fire_rate), cx2+60, py, 10, tc);
                py += fh(10)+M_IN;
                draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

                dtxt("Comportement :", px, py, 10, C_GOLD); py += fh(10)+3;
                dtxt(TOWER_BASE_STATS[si].description, px+M_IN, py, 10,
                     (Color){tc.r,tc.g,tc.b,220});
                py += fh(10)+M_IN+2;
                draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

                dtxt("Description :", px, py, 10, C_GOLD); py += fh(10)+3;
                if (TOWER_LORE[si])
                    draw_multiline(TOWER_LORE[si], px, &py, 9,
                                   (Color){160,145,100,255}, fh(9)+3);
            }
        }

    } else if (m->bestiary_tab == 3) {
    // ── Onglet UNITES ────────────────────────────────────────────

        // Liste gauche
        {
            int entry_h = 44, gap = 6;
            int ey = list_y + M_IN;
            for (int i = 0; i < UNIT_TYPE_COUNT; i++) {
                int sel = (m->sel_unit == i);
                Color uc = UNIT_COLORS[i];
                Rectangle r = {(float)(list_x+4),(float)ey,(float)(list_w-8),(float)entry_h};
                Color bg  = sel ? (Color){uc.r/8, uc.g/8, uc.b/8, 255} : (Color){18,12,4,200};
                Color brd = sel ? uc : C_BORDER;
                DrawRectangleRounded(r, 3.0f/entry_h, 4, bg);
                DrawRectangleRoundedLinesEx(r, 3.0f/entry_h, 4, sel ? 2.0f : 1.0f, brd);
                int tx = (int)r.x + M_IN, ty = (int)r.y + entry_h/2 - fh(11)/2;
                DrawRectangleRounded(
                    (Rectangle){(float)tx,(float)((int)r.y+entry_h/2-6),12,12},
                    0.3f, 4, (Color){uc.r,uc.g,uc.b,200});
                dtxt(UNIT_BASE_STATS[i].name, tx+18, ty, 11, sel ? uc : C_TEXT);
                char dbuf[48];
                clip_text(UNIT_BASE_STATS[i].description, list_w-28, 9, dbuf, sizeof(dbuf));
                dtxt(dbuf, tx+18, (int)r.y+entry_h/2+3, 9, C_DIM);
                if (vclick_r(r)) { m->sel_unit = i; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
                ey += entry_h + gap;
            }
        }

        // Détails droite
        {
            int si = m->sel_unit < 0 ? 0 : m->sel_unit >= UNIT_TYPE_COUNT ? UNIT_TYPE_COUNT-1 : m->sel_unit;
            int px = detail_x+M_IN+4, py = detail_y+M_IN, pw = detail_w-M_IN*2-4;
            Color uc = UNIT_COLORS[si];

            int splash_sz = 180;
            Texture2D *spl = &g_unit_splash[si];
            if (spl->id != 0) {
                int sx = detail_x + detail_w - splash_sz - M_IN;
                DrawTexturePro(*spl,
                    (Rectangle){0, 0, (float)spl->width, (float)spl->height},
                    (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                (float)splash_sz, (float)splash_sz},
                    (Vector2){0,0}, 0.0f, WHITE);
                DrawRectangleLinesEx(
                    (Rectangle){(float)sx, (float)(detail_y + M_IN),
                                (float)splash_sz, (float)splash_sz},
                    1.5f, C_BORDER);
            }
            int sep_w = (spl->id != 0) ? (pw - splash_sz - M_IN) : pw;

            dtxt(UNIT_BASE_STATS[si].name, px, py, 20, uc);
            py += fh(20) + 2;
            dtxt("Unite de combat", px, py, 9, C_DIM);
            py += fh(9) + M_IN;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            int cx2 = px + sep_w/2;
            dtxt("Cout :", px, py, 9, C_DIM);
            dtxt(TextFormat("%d or", UNIT_BASE_STATS[si].cost), px+50, py, 10, uc);
            dtxt("PV :", cx2, py, 9, C_DIM);
            dtxt(TextFormat("%.0f", UNIT_BASE_STATS[si].hp), cx2+32, py, 10, uc);
            py += fh(10)+3;
            dtxt("Degats :", px, py, 9, C_DIM);
            dtxt(TextFormat("%.0f", UNIT_BASE_STATS[si].damage), px+50, py, 10, uc);
            dtxt("Vitesse :", cx2, py, 9, C_DIM);
            dtxt(TextFormat("%.1f", UNIT_BASE_STATS[si].speed), cx2+56, py, 10, uc);
            py += fh(10)+3;
            dtxt("Portee att. :", px, py, 9, C_DIM);
            dtxt(TextFormat("%.1f", UNIT_BASE_STATS[si].atk_range), px+76, py, 10, uc);
            dtxt("Cadence :", cx2, py, 9, C_DIM);
            dtxt(TextFormat("%.1f/s", UNIT_BASE_STATS[si].atk_rate), cx2+56, py, 10, uc);
            py += fh(10)+M_IN;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            dtxt("Role :", px, py, 10, C_GOLD); py += fh(10)+3;
            dtxt(UNIT_BASE_STATS[si].description, px+M_IN, py, 10,
                 (Color){uc.r,uc.g,uc.b,220});
            py += fh(10)+M_IN+2;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            dtxt("Description :", px, py, 10, C_GOLD); py += fh(10)+3;
            if (UNIT_LORE[si])
                draw_multiline(UNIT_LORE[si], px, &py, 9,
                               (Color){160,145,100,255}, fh(9)+3);
        }

    } else {
    // ── Onglet BUTIN (perks de run & boutique) ───────────────────
        static const char *RAR_NAMES[3] = {"Commune", "Rare", "Epique"};
        static const char *CAT_NAMES[4] = {"Tour", "Economie", "Unite", "Survie"};

        // Liste gauche — les 19 perks, teintés par rareté
        {
            int entry_h = 28, gap = 2;
            int ey = list_y + M_IN;
            for (int i = 0; i < PERK_COUNT; i++) {
                const PerkDef *pd = &RUN_PERKS[i];
                RunColor rcl = runperk_rarity_color(pd->rarity);
                Color rc = (Color){rcl.r, rcl.g, rcl.b, 255};
                int sel = (m->sel_perk == i);
                Rectangle r = {(float)(list_x+4), (float)ey,
                               (float)(list_w-8), (float)entry_h};
                Color bg  = sel ? (Color){rc.r/8, rc.g/8, rc.b/8, 255}
                                : (Color){16,11,4,200};
                Color brd = sel ? rc : C_BORDER;
                DrawRectangleRounded(r, 3.0f/entry_h, 4, bg);
                DrawRectangleRoundedLinesEx(r, 3.0f/entry_h, 4, sel ? 2.0f : 1.0f, brd);
                int tx = (int)r.x + M_IN, ty = (int)r.y + entry_h/2 - fh(10)/2;
                perk_art_draw(i, tx + 8, (int)r.y + entry_h/2, 9, rc);
                char nb[40];
                clip_text(pd->name, list_w - 52, 10, nb, sizeof(nb));
                dtxt(nb, tx+22, ty, 10, sel ? rc : C_TEXT);
                if (vclick_r(r)) { m->sel_perk = i; audio_play_sfx(AUDIO_SFX_MENU_CLICK); }
                ey += entry_h + gap;
                if (ey + entry_h > list_y + list_h - M_IN) break;
            }
        }

        // Détails droite
        {
            int si = m->sel_perk < 0 ? 0
                   : (m->sel_perk >= PERK_COUNT ? PERK_COUNT-1 : m->sel_perk);
            const PerkDef *pd = &RUN_PERKS[si];
            RunColor rcl = runperk_rarity_color(pd->rarity);
            Color rc = (Color){rcl.r, rcl.g, rcl.b, 255};
            int cat = runperk_category(si);
            int px = detail_x+M_IN+4, py = detail_y+M_IN, pw = detail_w-M_IN*2-4;

            // Emblème procédural : badge de rareté portant le tag
            int em  = 150;
            int ex  = detail_x + detail_w - em - M_IN;
            int ey0 = detail_y + M_IN;
            DrawRectangleRounded(
                (Rectangle){(float)ex,(float)ey0,(float)em,(float)em}, 0.10f, 6,
                (Color){rc.r/6, rc.g/6, rc.b/6, 255});
            DrawRectangleRoundedLinesEx(
                (Rectangle){(float)ex,(float)ey0,(float)em,(float)em}, 0.10f, 6, 2.0f, rc);
            // Emblème pixel-art du perk + tag en petit + rareté en bas
            perk_art_draw(si, ex + em/2, ey0 + em/2 - 8, 40, rc);
            int gtw = mtxt(pd->tag, 12);
            dtxt(pd->tag, ex + em/2 - gtw/2, ey0 + em/2 + 34, 12, rc);
            const char *rn = RAR_NAMES[pd->rarity];
            int rnw = mtxt(rn, 10);
            dtxt(rn, ex + em/2 - rnw/2, ey0 + em - fh(10) - 8, 10,
                 (Color){rc.r, rc.g, rc.b, 200});

            int sep_w = pw - em - M_IN;

            dtxt(pd->name, px, py, 18, rc); py += fh(18)+3;
            dtxt(TextFormat("%s  -  %s", rn, CAT_NAMES[cat]), px, py, 9, C_DIM);
            py += fh(9) + M_IN;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            dtxt("Effet :", px, py, 10, C_GOLD); py += fh(10)+3;
            draw_multiline(pd->desc, px+M_IN, &py, 10,
                           (Color){rc.r, rc.g, rc.b, 225}, fh(10)+3);
            py += M_IN;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            dtxt("Exemplaires max :", px, py, 9, C_DIM);
            dtxt(TextFormat("x%d", pd->max_stack), px+138, py, 10, rc);
            py += fh(10)+3;
            dtxt("Cout boutique :", px, py, 9, C_DIM);
            dtxt(TextFormat("%d Renfort", pd->shop_cost), px+138, py, 10,
                 (Color){232, 200, 90, 255});
            py += fh(10)+M_IN;
            draw_sep(px, py, sep_w, C_BORDER); py += M_IN+2;

            dtxt("Acquisition :", px, py, 10, C_GOLD); py += fh(10)+3;
            draw_multiline(
                "Butin apres chaque acte : 1 choix gratuit parmi 3.\n"
                "Boutique entre chaque chapitre : achat en Renfort.\n"
                "Empiler les perks d'un meme cluster cree des combos.",
                px+M_IN, &py, 9, (Color){160,145,100,255}, fh(9)+3);
        }
    }

    if (draw_back_btn(vw, vh)) {
        m->screen = m->back_screen;
        pop_back_screen(m);
    }
    return act;
}

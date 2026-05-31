/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "interlude.h"
#include "ui_utils.h"
#include "../game/campaign_data.h"
#include "../game/meta.h"
#include "../game/runperks.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <stdio.h>   // snprintf

static const char *rar_name(PerkRarity r) {
    return (r == RAR_EPIC) ? "EPIQUE" : (r == RAR_RARE) ? "RARE" : "COMMUN";
}

// ── Helpers pixel-art partagés (butin / boutique) ─────────────
static void il_rivets_h(int x0, int x1, int y, Color c) {
    for (int x = x0; x <= x1; x += 24) DrawRectangle(x, y, 2, 2, c);
}
static void il_brackets(Rectangle r, Color c, int L) {
    int x0=(int)r.x+4, y0=(int)r.y+4, x1=(int)(r.x+r.width)-4, y1=(int)(r.y+r.height)-4;
    Color cc={c.r,c.g,c.b,225};
    DrawRectangle(x0,y0,L,2,cc); DrawRectangle(x0,y0,2,L,cc);
    DrawRectangle(x1-L,y0,L,2,cc); DrawRectangle(x1-2,y0,2,L,cc);
    DrawRectangle(x0,y1-2,L,2,cc); DrawRectangle(x0,y1-L,2,L,cc);
    DrawRectangle(x1-L,y1-2,L,2,cc); DrawRectangle(x1-2,y1-L,2,L,cc);
}
static void il_keycap(int x, int y, int w, int h, const char *lbl, Color col, int hot) {
    Rectangle k = {(float)x,(float)y,(float)w,(float)h};
    DrawRectangleRounded(k, 0.3f, 4,
        hot ? (Color){(unsigned char)(col.r/2),(unsigned char)(col.g/2),(unsigned char)(col.b/2),255}
            : (Color){22,17,9,255});
    DrawRectangleRoundedLinesEx(k, 0.3f, 4, 1.5f, col);
    int tw = mtxt(lbl, 12);
    dtxt(lbl, x + w/2 - tw/2, y + h/2 - fh(12)/2, 12, hot ? (Color){250,245,230,255} : col);
}
// Glyphe pixel-art de catégorie : 0 tour, 1 éco, 2 unité, 3 survie.
static void il_cat_glyph(int cx, int cy, int cat, Color col) {
    Color sh = {(unsigned char)(col.r*0.55f),(unsigned char)(col.g*0.55f),(unsigned char)(col.b*0.55f),255};
    switch (cat) {
        case 1: // éco : pièce frappée
            DrawCircle(cx, cy, 7, col);
            DrawCircle(cx, cy, 4, sh);
            DrawRectangle(cx-1, cy-3, 2, 6, col);
            break;
        case 2: // unité : casque
            DrawCircle(cx, cy-1, 6, col);
            DrawRectangle(cx-7, cy-1, 14, 3, col);
            DrawRectangle(cx-2, cy-5, 4, 2, sh);
            break;
        case 3: // survie : bouclier
            DrawRectangle(cx-5, cy-6, 10, 6, col);
            DrawRectangle(cx-4, cy,   8, 2, col);
            DrawRectangle(cx-3, cy+2, 6, 2, col);
            DrawRectangle(cx-1, cy+4, 2, 2, col);
            DrawRectangle(cx-2, cy-4, 4, 4, sh);
            break;
        default: // tour : socle + tourelle + canon
            DrawRectangle(cx-6, cy+3, 12, 4, col);
            DrawRectangle(cx-3, cy-4, 6, 7, col);
            DrawRectangle(cx+2, cy-3, 6, 2, sh);   // canon
            break;
    }
}
// Petite caisse de ravitaillement (icône d'en-tête).
static void il_crate(int cx, int cy, Color col) {
    Color sh = {(unsigned char)(col.r/2),(unsigned char)(col.g/2),(unsigned char)(col.b/2),255};
    DrawRectangle(cx-8, cy-7, 16, 14, sh);
    DrawRectangleLines(cx-8, cy-7, 16, 14, col);
    DrawLine(cx-8, cy-7, cx+8, cy+7, col);
    DrawLine(cx+8, cy-7, cx-8, cy+7, col);
}

// ── Constantes layout ────────────────────────────────────────
#define M   16
#define M_S  8

static void txt_c(const char *s, int cx, int y, int fs, Color col) {
    dtxt(s, cx - mtxt(s, fs)/2, y, fs, col);
}

static void draw_stars(int cx, int y, int stars, int max_stars) {
    int sw = 20, gap = 6;
    int total = max_stars * sw + (max_stars-1) * gap;
    int sx = cx - total/2;
    for (int i = 0; i < max_stars; i++) {
        Color c = (i < stars) ? (Color){232,200,32,255}
                               : (Color){50,40,20,255};
        dtxt("*", sx + i*(sw+gap), y, sw, c);
    }
}

// ════════════════════════════════════════════════════
// DIALOGUE AVANT L'ACTE
// ════════════════════════════════════════════════════
void interlude_render_dialog_before(const ActData *act, int node_id, int flags,
                                    int vw, int vh) {
    int cx = vw/2, cy = vh/2;

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,220});

    const char *echo = campaign_echo(node_id, flags);   // rappel d'un choix passé
    int pw = 580, ph = echo ? 350 : 300;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, 2.0f, (Color){80,55,20,255});

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    // Sous-titre chapitre/acte
    txt_c(act->subtitle, cx, py, 9, (Color){100,80,50,255});
    py += 14;

    // Titre de l'acte
    txt_c(act->title, cx, py, 18, (Color){232,152,32,255});
    py += 27;   // fh(18)=25 → séparateur 2px sous le bas du texte (était 24 → 1px DANS le texte)

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){60,40,12,160});
    py += M_S;

    // Objectif
    dtxt("OBJECTIF :", px, py, 10, (Color){80,160,80,255});
    py += 14;
    char obj_buf[80];
    clip_text(act->objective.description, iw, 12, obj_buf, sizeof(obj_buf));
    dtxt(obj_buf, px + M_S, py, 12, (Color){150,220,150,255});
    py += 20;

    // Séparateur
    DrawLine(px, py, px+iw, py, (Color){60,40,12,160});
    py += M_S;

    // Dialogue (texte multi-ligne)
    // On affiche le dialogue ligne par ligne
    {
        const char *d = act->dialog_before;
        char line[96];
        int li = 0, di = 0;
        int dy = py;
        int dlim = cy + ph/2 - (echo ? 80 : 40);   // réserve la place du rappel
        while (d[di] && dy < dlim) {
            li = 0;
            while (d[di] && d[di] != '\n' && li < 95)
                line[li++] = d[di++];
            if (d[di] == '\n') di++;
            line[li] = '\0';
            if (li > 0) {
                char tbuf[96];
                clip_text(line, iw, 10, tbuf, sizeof(tbuf));
                dtxt(tbuf, px, dy, 10, (Color){168,148,102,255});
            }
            dy += 16; // fh(10)=14 + 2px marge
        }
    }

    // Rappel narratif (callback d'un choix/défaite passé)
    if (echo) {
        int ey = cy + ph/2 - M - 50;
        DrawLine(px, ey - 6, px + iw, ey - 6, (Color){90, 70, 26, 150});
        dtxt("» RAPPEL", px, ey - 4, 8, (Color){150, 120, 50, 220});
        ey += 11;
        const char *e = echo; char el[96]; int ei;
        while (*e) {
            ei = 0;
            while (*e && *e != '\n' && ei < 95) el[ei++] = *e++;
            if (*e == '\n') e++;
            el[ei] = '\0';
            char ec[96]; clip_text(el, iw, 9, ec, sizeof(ec));
            dtxt(ec, px, ey, 9, (Color){205, 175, 95, 235});
            ey += 13;
        }
    }

    // Hint
    txt_c("ESPACE -- Commencer l'acte", cx, cy + ph/2 - M - 12, 10,
          (Color){80,65,40,255});
}

// ════════════════════════════════════════════════════
// DIALOGUE APRÈS L'ACTE
// ════════════════════════════════════════════════════
void interlude_render_dialog_after(const ActData *act, int stars,
                                   int scrap_earned, int vw, int vh,
                                   int node_id, int flags)
{
    int cx = vw/2, cy = vh/2;
    int is_last = (act->chapter == CAMPAIGN_CHAPTERS-1 &&
                   act->act == CAMPAIGN_ACTS-1);
    int has_choice = campaign_has_choice(node_id);

    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,200});

    int pw = 580, ph = has_choice ? 372 : 330;
    Color border_col = is_last ? (Color){232,152,32,255}
                               : (Color){42,190,105,255};
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        6.0f/ph, 8, 2.0f, border_col);

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    // Titre
    const char *title = is_last ? "CAMPAGNE TERMINEE !" : "ACTE TERMINE !";
    txt_c(title, cx, py, 20, border_col);
    py += 28;

    // Étoiles
    draw_stars(cx, py, stars, 2);
    py += 28;

    DrawLine(px, py, px+iw, py, (Color){50,35,10,160});
    py += M_S;

    // Stats
    dtxt(TextFormat("Ferraille gagnee : +%d", scrap_earned),
             px, py, 12, (Color){118,188,45,255}); py += 16;

    // Message de débloquage éventuel
    if (act->unlock_msg) {
        dtxt(act->unlock_msg, px, py, 11,
                 (Color){140,200,240,255}); py += 16;
    }

    DrawLine(px, py, px+iw, py, (Color){50,35,10,160});
    py += M_S;

    // Dialogue après (épilogue variant selon le parcours pour le dernier acte)
    {
        const char *d = is_last ? campaign_epilogue(flags) : act->dialog_after;
        char line[96];
        int li = 0, di = 0;
        int dy = py;
        /* Réserve la zone basse pour le bloc de choix (sinon chevauchement). */
        int dlimit = cy + ph/2 - (has_choice ? 78 : 44);
        while (d[di] && dy < dlimit) {
            li = 0;
            while (d[di] && d[di] != '\n' && li < 95)
                line[li++] = d[di++];
            if (d[di] == '\n') di++;
            line[li] = '\0';
            if (li > 0) {
                char tbuf[96];
                clip_text(line, iw, 10, tbuf, sizeof(tbuf));
                dtxt(tbuf, px, dy, 10, (Color){168,148,102,255});
            }
            dy += 16; // fh(10)=14 + 2px marge
        }
    }

    if (has_choice) {
        /* Bifurcation : on présente la question + deux options (touches 1/2). */
        int by = cy + ph/2 - M - 46;
        DrawLine(px, by - 6, px + iw, by - 6, (Color){50,35,10,160});
        const char *prompt = campaign_choice_prompt(node_id);
        if (prompt) {
            char pbuf[96];
            clip_text(prompt, iw, 11, pbuf, sizeof(pbuf));
            txt_c(pbuf, cx, by, 11, (Color){232,200,120,255});
        }
        by += 16;
        for (int o = 0; o < 2; o++) {
            const char *lbl = campaign_choice_label(node_id, o);
            if (!lbl) continue;
            char obuf[110];
            snprintf(obuf, sizeof(obuf), "[%d]  %s", o + 1, lbl);
            char cbuf[120];
            clip_text(obuf, iw, 11, cbuf, sizeof(cbuf));
            dtxt(cbuf, px + M_S, by, 11, (Color){120,210,120,255});
            by += 15;
        }
    } else {
        const char *hint = is_last ? "ESPACE -- Retour au menu"
                                    : "ESPACE -- Stage suivant";
        txt_c(hint, cx, cy + ph/2 - M - 12, 10, (Color){80,65,40,255});
    }
}

// ════════════════════════════════════════════════════
// GAME OVER
// ════════════════════════════════════════════════════
void interlude_render_gameover(const GameState *gs, int vw, int vh) {
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,180});

    /* Campagne : selon le nœud, la défaite peut bifurquer (repli / reprise)
       au lieu d'être une fin sèche. */
    DefeatMode dm = DEFEAT_GAMEOVER;
    int diverge = 0;
    if (gs->is_campaign) {
        dm = campaign_defeat_mode(gs->campaign_stage);
        diverge = (dm != DEFEAT_GAMEOVER);
    }

    int pw = 460, ph = diverge ? 250 : (gs->is_campaign ? 220 : 190);
    float rnd = 5.0f/(float)ph;
    Color border = diverge ? (Color){214, 140, 30, 255} : (Color){200, 40, 20, 255};
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){20, 8, 4, 252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, border);

    int py = cy - ph/2 + M;

    txt_c(diverge ? "POSITION PERDUE" : "BASTION TOMBE", cx, py, 22, border);
    py += 32;

    if (gs->is_campaign) {
        const ActData *ad = campaign_act_get(gs->campaign_stage);
        char buf[80];
        clip_text(ad->subtitle, pw - M*2, 10, buf, sizeof(buf));
        txt_c(buf, cx, py, 10, (Color){100,80,50,255});
        py += 16;
    }

    txt_c(TextFormat("Vague %d  |  Ennemis elimines : %d",
                     gs->wave_manager.number, gs->kills),
          cx, py, 11, (Color){120,80,60,255});
    py += 22;

    if (diverge) {
        const char *flavor = (dm == DEFEAT_RETREAT)
            ? "Repli possible : la mission se poursuit ailleurs."
            : "Tout n'est pas perdu — un nouvel assaut est possible.";
        txt_c(flavor, cx, py, 10, (Color){150, 170, 150, 255});

        const char *opt1 = (dm == DEFEAT_RETREAT)
            ? "[ESPACE]   Se replier"
            : "[ESPACE]   Reprendre (affaibli)";
        txt_c(opt1, cx, cy + ph/2 - M - 30, 13, (Color){120, 210, 90, 255});
        txt_c("[ECHAP]   Abandonner  —  retour a la carte",
              cx, cy + ph/2 - M - 12, 10, (Color){185, 95, 70, 255});
    } else {
        const char *hint = gs->is_campaign
            ? "ESPACE  ou  clic  --  Retour a la carte"
            : "ESPACE  ou  clic  --  Retour au menu";
        txt_c(hint, cx, cy + ph/2 - M - 12, 10, (Color){80, 65, 40, 255});
    }
    (void)py;
}

// ════════════════════════════════════════════════════
// ÉCRAN EXTRACTION ENDLESS
// ════════════════════════════════════════════════════
void interlude_render_extract(const GameState *gs, int vw, int vh,
                              Vector2 vmouse)
{
    int cx = vw/2, cy = vh/2;
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,170});

    int pw = 420, ph = 260;
    float rnd = 5.0f / ph;
    DrawRectangleRounded(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, (Color){10,6,2,252});
    DrawRectangleRoundedLinesEx(
        (Rectangle){cx-pw/2.0f, cy-ph/2.0f, (float)pw, (float)ph},
        rnd, 8, 2.0f, (Color){232,152,32,255});

    int px = cx - pw/2 + M;
    int py = cy - ph/2 + M;
    int iw = pw - M*2;

    /* Titre */
    txt_c("POINT D'EXTRACTION", cx, py, 18, (Color){232,152,32,255});
    py += 24;
    DrawLine(px, py, px+iw, py, (Color){60,40,12,180}); py += 10;

    /* Infos */
    dtxt(TextFormat("Serie          : %d",  gs->endless_series+1),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    dtxt(TextFormat("Vague          : %d",  gs->wave_manager.number),
             px, py, 12, (Color){168,148,102,255}); py += 16;
    dtxt(TextFormat("Multiplicateur : x%.1f", gs->endless_multiplier),
             px, py, 12, (Color){232,152,32,255}); py += 16;

    int score = meta_endless_score(gs->wave_manager.number, gs->endless_multiplier);
    int scrap  = score / 10 > 200 ? 200 : score / 10;
    dtxt(TextFormat("Ferraille si extrait : +%d", scrap),
             px, py, 12, (Color){118,188,45,255}); py += 16;
    dtxt(TextFormat("Continuer -> mult. x%.1f", gs->endless_multiplier * 1.5f),
             px, py, 10, (Color){100,160,220,255}); py += 20;
    DrawLine(px, py, px+iw, py, (Color){40,28,8,140}); py += 10;

    /* Boutons */
    int bw = 160, bh = 32;
    int by2 = cy + ph/2 - M_S - bh;
    int bx1 = cx - bw - M_S;
    int bx2 = cx + M_S;

    /* [E] EXTRAIRE */
    {
        Rectangle r = {(float)bx1,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){8,28,8,255} : (Color){4,16,4,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){42,190,105,255} : (Color){20,80,40,255});
        const char *lbl = "E -- EXTRAIRE";
        dtxt(lbl, bx1+bw/2-mtxt(lbl,13)/2, by2+bh/2-7,
                 13, (Color){42,190,105,255});
    }
    /* [ESPACE] CONTINUER */
    {
        Rectangle r = {(float)bx2,(float)by2,(float)bw,(float)bh};
        int hov = CheckCollisionPointRec(vmouse, r);
        DrawRectangleRounded(r, 5.0f/bh, 6,
            hov ? (Color){6,18,32,255} : (Color){4,12,20,255});
        DrawRectangleRoundedLinesEx(r, 5.0f/bh, 6, 1.5f,
            hov ? (Color){52,140,210,255} : (Color){24,70,110,255});
        const char *lbl = "ESPACE -- CONTINUER";
        dtxt(lbl, bx2+bw/2-mtxt(lbl,11)/2, by2+bh/2-7,
                 11, (Color){52,140,210,255});
    }
    (void)py;
}

// ════════════════════════════════════════════════════
// BUTIN ROGUE-LITE — choisir 1 perk parmi N (après chaque acte)
// ════════════════════════════════════════════════════
// Layout partagé (rendu + détection de clic) — une seule source de vérité.
typedef struct { int cx, cy, pw, ph, px, iw, py0, row, n; } DraftL;
static DraftL draft_layout(const RunBuild *rb, int vw, int vh) {
    DraftL L; L.cx = vw/2; L.cy = vh/2;
    int n = rb->draft_n;
    if (n < 0) n = 0;
    if (n > MAX_DRAFT_OFFER) n = MAX_DRAFT_OFFER;
    L.n  = n;  L.row = 58;  L.pw = 580;  L.ph = 92 + n*L.row + 22;
    L.px = L.cx - L.pw/2 + M;  L.iw = L.pw - M*2;
    L.py0 = (L.cy - L.ph/2 + M) + 28 + 18 + M_S;   // y de la 1re ligne d'offre
    return L;
}

void interlude_render_draft(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    DraftL L = draft_layout(rb, vw, vh);
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,212});
    Color accent = (Color){232,200,80,255};
    Rectangle pr = {L.cx-L.pw/2.0f, L.cy-L.ph/2.0f, (float)L.pw, (float)L.ph};

    // Corps métal + volume
    DrawRectangleRounded(pr, 6.0f/L.ph, 8, (Color){13,10,5,253});
    DrawRectangle((int)pr.x+3, (int)pr.y+3, L.pw-6, L.ph/3, (Color){255,255,255,6});
    DrawRectangle((int)pr.x+3, (int)pr.y+L.ph-L.ph/4, L.pw-6, L.ph/4-3, (Color){0,0,0,45});
    DrawRectangleRoundedLinesEx(pr, 6.0f/L.ph, 8, 2.0f, accent);

    // En-tête : bandeau + caisse + titre + rivets + équerres
    int hy = (int)pr.y;
    DrawRectangle((int)pr.x+3, hy+3, L.pw-6, 30,
                  (Color){(unsigned char)(accent.r/6),(unsigned char)(accent.g/6),10,255});
    DrawRectangle((int)pr.x+3, hy+33, L.pw-6, 1, accent);
    il_rivets_h((int)pr.x+14, (int)pr.x+L.pw-14, hy+9,
                (Color){(unsigned char)(accent.r/2),(unsigned char)(accent.g/2),20,200});
    il_crate(L.px+14, hy+18, accent);
    txt_c("BUTIN DE GUERRE", L.cx, hy+9, 18, accent);
    il_brackets(pr, accent, 12);
    txt_c("Recupere un renfort  —  clic ou touches 1-3",
          L.cx, hy+38, 9, (Color){175,150,100,235});

    float pulse = (sinf((float)GetTime()*4.0f) + 1.0f) * 0.5f;
    for (int i = 0; i < L.n; i++) {
        int id = rb->draft_offer[i];
        if (id < 0 || id >= PERK_COUNT) continue;
        const PerkDef *pd = &RUN_PERKS[id];
        RunColor rc = runperk_rarity_color(pd->rarity);
        Color cc = {rc.r, rc.g, rc.b, 255};
        int ry = L.py0 + i*L.row, ch = L.row - 6;
        Rectangle r = {(float)L.px, (float)ry, (float)L.iw, (float)ch};
        int hov = CheckCollisionPointRec(vm, r);

        DrawRectangleRounded(r, 0.16f, 5,
            hov ? (Color){(unsigned char)(rc.r/4),(unsigned char)(rc.g/4),(unsigned char)(rc.b/4),255}
                : (Color){(unsigned char)(rc.r/9),(unsigned char)(rc.g/9),(unsigned char)(rc.b/9),255});
        DrawRectangleRoundedLinesEx(r, 0.16f, 5, hov ? 2.4f : 1.5f, cc);
        DrawRectangle(L.px+2, ry+2, 4, ch-4, cc);                    // liseré de rareté
        if (pd->rarity == RAR_EPIC)                                   // halo pulsé épique
            DrawRectangleRoundedLinesEx(r, 0.16f, 5, 1.0f,
                (Color){rc.r, rc.g, rc.b, (unsigned char)(60 + pulse*130)});

        il_keycap(L.px+12, ry+ch/2-11, 22, 22, TextFormat("%d", i+1), cc, hov);
        il_cat_glyph(L.px+52, ry+ch/2, runperk_category(id), cc);
        dtxt(pd->name, L.px+70, ry+6, 13, cc);

        const char *rn = rar_name(pd->rarity);
        int rw = mtxt(rn, 8) + 10;
        DrawRectangleRounded(
            (Rectangle){(float)(L.px+L.iw-rw-6), (float)(ry+5), (float)rw, 14.0f},
            0.45f, 4, (Color){(unsigned char)(rc.r/3),(unsigned char)(rc.g/3),(unsigned char)(rc.b/3),255});
        dtxt(rn, L.px+L.iw-rw-1, ry+7, 8, cc);

        char db[96]; clip_text(pd->desc, L.iw-72-rw-10, 9, db, sizeof(db));
        dtxt(db, L.px+70, ry+26, 9, (Color){185,168,135,255});
    }

    txt_c("La rarete depend de la voie choisie.", L.cx, L.cy+L.ph/2-M-11, 8,
          (Color){115,98,62,235});
}

int interlude_draft_pick_at(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    DraftL L = draft_layout(rb, vw, vh);
    for (int i = 0; i < L.n; i++) {
        Rectangle r = {(float)L.px, (float)(L.py0+i*L.row), (float)L.iw, (float)(L.row-6)};
        if (CheckCollisionPointRec(vm, r)) return i;
    }
    return -1;
}

// ════════════════════════════════════════════════════
// BOUTIQUE ROGUE-LITE — dépenser le Renfort (entre chapitres)
// ════════════════════════════════════════════════════
typedef struct { int cx, cy, pw, ph, px, iw, py0, row, n, reroll_y, done_y; } ShopL;
static ShopL shop_layout(const RunBuild *rb, int vw, int vh) {
    ShopL L; L.cx = vw/2; L.cy = vh/2;
    int n = rb->shop_n;
    if (n < 0) n = 0;
    if (n > MAX_SHOP_OFFER) n = MAX_SHOP_OFFER;
    L.n  = n;  L.row = 56;  L.pw = 600;  L.ph = 118 + n*L.row + 28;
    L.px = L.cx - L.pw/2 + M;  L.iw = L.pw - M*2;
    L.py0 = (L.cy - L.ph/2 + M) + 28 + 22 + M_S;
    L.reroll_y = L.cy + L.ph/2 - M - 28;   // bande "relancer"
    L.done_y   = L.cy + L.ph/2 - M - 12;   // bande "partir"
    return L;
}

void interlude_render_shop(const RunBuild *rb, int reroll_cost, Vector2 vm, int vw, int vh) {
    ShopL L = shop_layout(rb, vw, vh);
    DrawRectangle(0, 0, vw, vh, (Color){0,0,0,214});
    Color accent = (Color){120,200,140,255};
    Color gold   = (Color){232,200,80,255};
    Rectangle pr = {L.cx-L.pw/2.0f, L.cy-L.ph/2.0f, (float)L.pw, (float)L.ph};

    DrawRectangleRounded(pr, 6.0f/L.ph, 8, (Color){10,12,6,253});
    DrawRectangle((int)pr.x+3, (int)pr.y+3, L.pw-6, L.ph/3, (Color){255,255,255,6});
    DrawRectangle((int)pr.x+3, (int)pr.y+L.ph-L.ph/4, L.pw-6, L.ph/4-3, (Color){0,0,0,45});
    DrawRectangleRoundedLinesEx(pr, 6.0f/L.ph, 8, 2.0f, accent);

    // En-tête
    int hy = (int)pr.y;
    DrawRectangle((int)pr.x+3, hy+3, L.pw-6, 30,
                  (Color){(unsigned char)(accent.r/7),(unsigned char)(accent.g/7),(unsigned char)(accent.b/7),255});
    DrawRectangle((int)pr.x+3, hy+33, L.pw-6, 1, accent);
    il_rivets_h((int)pr.x+14, (int)pr.x+L.pw-14, hy+9,
                (Color){(unsigned char)(accent.r/2),(unsigned char)(accent.g/2),(unsigned char)(accent.b/2),200});
    il_crate(L.px+14, hy+18, accent);
    txt_c("RAVITAILLEMENT", L.cx, hy+9, 18, accent);
    il_brackets(pr, accent, 12);

    // Compteur de Renfort (pièce + valeur) + aide
    int rny = hy + 40;
    il_cat_glyph(L.px+10, rny+6, 1, gold);
    dtxt(TextFormat("RENFORT  %d", rb->renfort), L.px+24, rny, 12, gold);
    {
        const char *h = "clic / touches 1-4   ·   [R] relancer   ·   [ESPACE] partir";
        dtxt(h, L.px+L.iw - mtxt(h,9), rny+3, 9, (Color){135,130,100,235});
    }

    float pulse = (sinf((float)GetTime()*4.0f) + 1.0f) * 0.5f;
    for (int i = 0; i < L.n; i++) {
        int id = rb->shop_offer[i];
        if (id < 0 || id >= PERK_COUNT) continue;
        const PerkDef *pd = &RUN_PERKS[id];
        RunColor rc = runperk_rarity_color(pd->rarity);
        Color cc = {rc.r, rc.g, rc.b, 255};
        int maxed  = (rb->count[id] >= pd->max_stack);
        int afford = (rb->renfort >= pd->shop_cost) && !maxed;
        Color tc   = afford ? cc : (Color){100,100,100,225};
        int ry = L.py0 + i*L.row, ch = L.row - 6;
        Rectangle r = {(float)L.px, (float)ry, (float)L.iw, (float)ch};
        int hov = CheckCollisionPointRec(vm, r);

        DrawRectangleRounded(r, 0.16f, 5,
            hov ? (Color){(unsigned char)(rc.r/5),(unsigned char)(rc.g/5),(unsigned char)(rc.b/5),255}
                : (Color){(unsigned char)(rc.r/9),(unsigned char)(rc.g/9),(unsigned char)(rc.b/9),255});
        DrawRectangleRoundedLinesEx(r, 0.16f, 5, hov ? 2.2f : 1.3f,
            afford ? cc : (Color){70,70,70,210});
        DrawRectangle(L.px+2, ry+2, 4, ch-4, afford ? cc : (Color){80,80,80,200});
        if (afford && pd->rarity == RAR_EPIC)
            DrawRectangleRoundedLinesEx(r, 0.16f, 5, 1.0f,
                (Color){rc.r, rc.g, rc.b, (unsigned char)(60 + pulse*130)});

        il_keycap(L.px+12, ry+ch/2-10, 20, 20, TextFormat("%d", i+1), tc, hov && afford);
        il_cat_glyph(L.px+48, ry+ch/2, runperk_category(id), tc);
        dtxt(pd->name, L.px+66, ry+5, 12, tc);

        const char *cs = maxed ? "MAX" : TextFormat("%d", pd->shop_cost);
        Color costc = maxed ? (Color){120,120,120,225} : afford ? gold : (Color){150,110,60,225};
        int cw = mtxt(cs, 10) + (maxed ? 0 : mtxt(" RNF", 9));
        DrawRectangleRounded((Rectangle){(float)(L.px+L.iw-cw-16),(float)(ry+5),(float)(cw+12),14.0f},
                             0.45f, 4, (Color){18,18,10,230});
        dtxt(cs, L.px+L.iw-cw-10, ry+6, 10, costc);
        if (!maxed) dtxt(" RNF", L.px+L.iw-cw-10+mtxt(cs,10), ry+7, 9, costc);

        char db[96]; clip_text(pd->desc, L.iw-66-cw-18, 9, db, sizeof(db));
        dtxt(db, L.px+66, ry+25, 9, (Color){165,160,140,255});
    }

    // Boutons Relancer / Partir (pleine largeur des bandes cliquables)
    {
        Rectangle rrr = {(float)L.px, (float)(L.reroll_y-3), (float)L.iw, 16.0f};
        int rrhov = CheckCollisionPointRec(vm, rrr);
        Color rcol = (rb->renfort >= reroll_cost) ? (Color){205,172,92,255} : (Color){110,95,60,220};
        DrawRectangleRounded(rrr, 0.45f, 4, rrhov ? (Color){36,28,10,235} : (Color){18,14,7,190});
        DrawRectangleRoundedLinesEx(rrr, 0.45f, 4, rrhov ? 1.6f : 1.0f, rcol);
        char rr[56]; snprintf(rr, sizeof(rr), "[R]  Relancer les offres  (%d RNF)", reroll_cost);
        txt_c(rr, L.cx, L.reroll_y, 10, rcol);

        Rectangle dnr = {(float)L.px, (float)(L.done_y-3), (float)L.iw, 16.0f};
        int dnhov = CheckCollisionPointRec(vm, dnr);
        DrawRectangleRounded(dnr, 0.45f, 4, dnhov ? (Color){10,40,16,235} : (Color){8,20,10,190});
        DrawRectangleRoundedLinesEx(dnr, 0.45f, 4, dnhov ? 1.6f : 1.0f, accent);
        txt_c("[ESPACE]  Partir au combat", L.cx, L.done_y, 11, accent);
    }
}

int interlude_shop_pick_at(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    ShopL L = shop_layout(rb, vw, vh);
    for (int i = 0; i < L.n; i++) {
        Rectangle r = {(float)L.px, (float)(L.py0+i*L.row), (float)L.iw, (float)(L.row-6)};
        if (CheckCollisionPointRec(vm, r)) return i;
    }
    Rectangle rrr = {(float)L.px, (float)(L.reroll_y-3), (float)L.iw, 16.0f};
    if (CheckCollisionPointRec(vm, rrr)) return -2;
    Rectangle dnr = {(float)L.px, (float)(L.done_y-3), (float)L.iw, 16.0f};
    if (CheckCollisionPointRec(vm, dnr)) return -3;
    return -1;
}


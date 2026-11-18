/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  interlude_loot.c ─ Écrans BUTIN (après chaque acte) et BOUTIQUE (entre
 *  chapitres) de la run roguelite, au style « plaque de métal rivetée » des
 *  écrans du récit (ui_plate.h). Extrait d'interlude.c (règle < 500 lignes).
 *
 *  draft_layout / shop_layout restent la SEULE source des rectangles
 *  cliquables (rendu ET détection de clic) : l'habillage ne déplace rien.
 *  Mise en scène : le panneau glisse et se dévoile, les cartes entrent en
 *  cascade (liseré de rareté qui grandit, contenu qui glisse), reflet qui
 *  balaye les offres épiques.
 */
#include "interlude.h"
#include "ui_utils.h"
#include "ui_anim.h"
#include "ui_palette.h"
#include "ui_plate.h"
#include "perk_art.h"
#include "../game/runperks.h"
#include <math.h>
#include <stdio.h>

// ── Mise en scène ───────────────────────────────────────────────
#define IL_OPEN_DUR    0.25f   // glissement + dévoilement du panneau (s)
#define IL_OPEN_RISE   20.0f   // hauteur du glissement (px)
#define IL_HDR_T0      0.10f   // apparition de l'en-tête (s)
#define IL_HDR_DUR     0.15f
#define IL_CARD_T0     0.15f   // 1re carte (s)
#define IL_CARD_STEP   0.08f   // délai entre cartes (s)
#define IL_CARD_DUR    0.22f   // entrée du contenu d'une carte (s)
#define IL_CARD_FADE   0.10f   // dévoilement des plaques de cartes (s)
#define IL_CARD_SLIDE  14.0f   // glissement du contenu (px)
#define IL_SHIM_PERIOD 2.2f    // reflet épique : période (s)
#define IL_SHIM_CROSS  0.6f    // … durée de la traversée (s)
#define IL_SHIM_W      24      // … largeur de la bande (px)

// ── Mise en page (inchangée : les hitbox en dépendent) ──────────
#define IL_M           16      // marge du panneau
#define IL_MS          8       // petit espacement
#define IL_HDR_H       30      // bandeau de titre
#define SHOP_BTN_H     28      // boutons Relancer / Partir

// ── Habillage ───────────────────────────────────────────────────
#define IL_PANEL_LINER 0.50f   // liseré du grand panneau
#define IL_CARD_LINER  0.55f   // liseré d'une carte au repos
#define IL_TAG_FS      10      // étiquettes (rareté, prix)
#define IL_GOLD        ((Color){232, 200,  80, 255})
#define IL_GREEN       ((Color){120, 200, 140, 255})
#define IL_REROLL      ((Color){205, 172,  92, 255})
#define IL_GREY        ((Color){100, 100, 100, 255})

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static Color with_a(Color c, float a) { return plate_fade(c, a); }
static Color scaled(Color c, float k) {
    return (Color){(unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), c.a};
}

/* Reflet épique : bande claire qui balaye l'intérieur de la carte (clip
   manuel — pas de scissor, il ignore les caméras). */
static void shimmer(Rectangle r, float t) {
    float ph = fmodf(t, IL_SHIM_PERIOD);
    if (ph >= IL_SHIM_CROSS) return;
    Rectangle in = plate_inset(r, PLATE_BORDER + 1.0f);
    float sx  = in.x + (in.width + IL_SHIM_W) * (ph / IL_SHIM_CROSS) - IL_SHIM_W;
    int   sx0 = (int)sx, sx1 = sx0 + IL_SHIM_W;
    if (sx0 < (int)in.x) sx0 = (int)in.x;
    if (sx1 > (int)(in.x + in.width)) sx1 = (int)(in.x + in.width);
    if (sx1 > sx0) DrawRectangle(sx0, (int)in.y, sx1 - sx0, (int)in.height, (Color){255, 255, 255, 18});
}

/* Petite caisse de ravitaillement en métal, bande de couleur (icône d'en-tête). */
static void crate_icon(int cx, int cy, Color acc) {
    DrawRectangle(cx - 9, cy - 7, 18, 14, PLATE_FRAME_D);
    DrawRectangle(cx - 8, cy - 6, 16, 12, PLATE_FRAME);
    DrawRectangle(cx - 8, cy - 6, 16, 1, PLATE_FRAME_L);
    DrawRectangle(cx - 8, cy - 1, 16, 3, acc);
    DrawRectangle(cx - 6, cy - 4, 1, 1, PLATE_FRAME_D); DrawRectangle(cx + 5, cy - 4, 1, 1, PLATE_FRAME_D);
    DrawRectangle(cx - 6, cy + 4, 1, 1, PLATE_FRAME_D); DrawRectangle(cx + 5, cy + 4, 1, 1, PLATE_FRAME_D);
}

/* Grand panneau + bandeau de titre (caisse, titre centré, filet). */
static void panel_with_header(Rectangle pr, const char *title, Color acc, float pa, float ha) {
    plate_alpha(pa);                                           /* le panneau se dévoile */
    plate_panel(pr, PLATE_CHAMFER, acc, IL_PANEL_LINER);
    plate_alpha(1.0f);
    if (ha > 0.0f) {
        int x = (int)(pr.x + PLATE_BORDER + 2), w = (int)(pr.width - 2.0f * (PLATE_BORDER + 2));
        int y = (int)(pr.y + PLATE_BORDER + 1);
        DrawRectangle(x, y, w, IL_HDR_H - 2, with_a(scaled(acc, 0.16f), ha));
        DrawRectangleGradientH(x, y + IL_HDR_H - 2, w / 2, 1, with_a(acc, 0.0f), with_a(acc, 0.8f * ha));
        DrawRectangleGradientH(x + w / 2, y + IL_HDR_H - 2, w - w / 2, 1, with_a(acc, 0.8f * ha), with_a(acc, 0.0f));
        crate_icon((int)pr.x + IL_M + 14, (int)pr.y + 18, with_a(acc, ha));
        dtxt_c(title, (int)(pr.x + pr.width * 0.5f), (int)pr.y + 9, 18, with_a(acc, ha));
    }
}

/* Plaque d'une offre (butin / boutique) : liseré de rareté, survol, reflet. */
static void offer_plate(Rectangle r, Color rc, float hk, int enabled, int epic, float fa, float kc, float t) {
    Color edge = enabled ? rc : (Color){70, 70, 70, 255};
    plate_alpha(fa);                                           /* la carte se dévoile */
    if (hk > 0.0f && enabled)
        plate_chamfer_line(plate_inset(r, -2.0f), PLATE_CHAMFER_S + 1.0f, with_a(rc, 0.45f * hk));
    plate_panel(r, PLATE_CHAMFER_S, edge, IL_CARD_LINER + (1.0f - IL_CARD_LINER) * hk);
    if (enabled && hk > 0.0f)                                  /* éclaircissement au survol */
        plate_chamfer_fill(plate_inset(r, PLATE_BORDER + 1.0f), PLATE_CHAMFER_S - 3.0f,
                           (Color){rc.r, rc.g, rc.b, (unsigned char)(28.0f * hk)});
    int bar_h = (int)((r.height - 2.0f * PLATE_BORDER - 4.0f) * kc);   /* liseré qui grandit */
    DrawRectangle((int)(r.x + PLATE_BORDER + 2), (int)(r.y + PLATE_BORDER + 2), 3, bar_h,
                  with_a(enabled ? rc : (Color){80, 80, 80, 255}, fa));
    if (epic && enabled) shimmer(r, t);
    plate_alpha(1.0f);
}

/* Étiquette à droite de la carte (rareté, prix) ; retourne sa largeur. */
static int tag(int right, int y, const char *txt, Color col, float a) {
    int w = mtxt(txt, IL_TAG_FS) + 10, h = fh(IL_TAG_FS) + 1, x = right - w;
    DrawRectangle(x, y, w, h, with_a(PLATE_WELL, a));
    DrawRectangleLines(x, y, w, h, with_a(scaled(col, 0.7f), a));
    dtxt(txt, x + 5, y, IL_TAG_FS, with_a(col, a));
    return w;
}

/* Flash de sélection (1 frame : app.c ferme l'écran). */
static void pick_flash(Rectangle r, int cy) {
    plate_chamfer_fill(r, PLATE_CHAMFER_S, (Color){255, 255, 255, 120});
    DrawCircleLines((int)(r.x + r.width / 2), cy, 26, (Color){255, 255, 255, 180});
}

/* Horloge d'ouverture : une nouvelle offre rejoue la mise en scène. */
static float open_clock(float *t, int *prev0, int *prevn, int o0, int n) {
    if (o0 != *prev0 || n != *prevn) { *prev0 = o0; *prevn = n; *t = 0.0f; }
    *t += ui_dt();
    return *t;
}

// ════════════════════════════════════════════════════════════════
// BUTIN — choisir 1 perk parmi N (après chaque acte)
// ════════════════════════════════════════════════════════════════
typedef struct { int cx, cy, pw, ph, px, iw, py0, row, n; } DraftL;
static DraftL draft_layout(const RunBuild *rb, int vw, int vh) {
    DraftL L; L.cx = vw/2; L.cy = vh/2;
    int n = rb->draft_n;
    if (n < 0) n = 0;
    if (n > MAX_DRAFT_OFFER) n = MAX_DRAFT_OFFER;
    L.n  = n;  L.row = 58;  L.pw = 580;  L.ph = 92 + n*L.row + 22;
    L.px = L.cx - L.pw/2 + IL_M;  L.iw = L.pw - IL_M*2;
    L.py0 = (L.cy - L.ph/2 + IL_M) + 28 + 18 + IL_MS;   // y de la 1re ligne d'offre
    return L;
}

void interlude_render_draft(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    DraftL L = draft_layout(rb, vw, vh);
    static int   prev_o0 = -999, prev_nn = -999;
    static float open_t  = 1e9f;
    float t  = open_clock(&open_t, &prev_o0, &prev_nn, rb->draft_offer[0], rb->draft_n);
    float pa = clamp01(t / IL_OPEN_DUR);
    float ha = clamp01((t - IL_HDR_T0) / IL_HDR_DUR);
    int   pdy = -(int)(IL_OPEN_RISE * (1.0f - ea_out_cubic(pa)));

    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, (unsigned char)(212.0f * pa)});
    /* Le CADRE glisse (pdy) ; les cartes restent aux positions du layout. */
    Rectangle pr = {L.cx - L.pw/2.0f, L.cy - L.ph/2.0f + (float)pdy, (float)L.pw, (float)L.ph};
    panel_with_header(pr, "BUTIN DE GUERRE", IL_GOLD, pa, ha);
    if (ha > 0.0f)
        dtxt_c("Récupère un renfort  —  clic ou touches 1-3", L.cx, (int)pr.y + 38, 10,
               (Color){175, 150, 100, (unsigned char)(235.0f * ha)});

    for (int i = 0; i < L.n; i++) {
        int id = rb->draft_offer[i];
        if (id < 0 || id >= PERK_COUNT) continue;
        const PerkDef *pd = &RUN_PERKS[id];
        RunColor rcr = runperk_rarity_color(pd->rarity);
        Color rc = {rcr.r, rcr.g, rcr.b, 255};
        int ry = L.py0 + i*L.row, ch = L.row - 6;
        Rectangle r = {(float)L.px, (float)ry, (float)L.iw, (float)ch};
        int hov = CheckCollisionPointRec(vm, r);

        float fa = clamp01((t - IL_CARD_T0) / IL_CARD_FADE);
        float kc = clamp01((t - (IL_CARD_T0 + (float)i * IL_CARD_STEP)) / IL_CARD_DUR);
        if (fa <= 0.0f) continue;
        int   cdx = -(int)(IL_CARD_SLIDE * (1.0f - ea_out_cubic(kc)));
        float hk  = ui_hover_t(L.px, ry, hov);
        offer_plate(r, rc, hk, 1, pd->rarity >= RAR_EPIC, fa, kc, t);

        if (kc > 0.0f) {
            Color cc = with_a(rc, kc);
            plate_keycap(L.px + 14 + cdx, ry + ch/2 - (fh(9) + 4)/2 - 1, TextFormat("%d", i + 1), cc);
            int psz = (int)(11.0f * (1.0f + 0.15f * ea_out_back(hk)) + 0.5f);
            perk_art_draw(id, L.px + 52 + cdx, ry + ch/2, psz, cc);
            dtxt(pd->name, L.px + 70 + cdx, ry + 6, 13, cc);
            int rw = tag(L.px + L.iw - 8 + cdx, ry + 7, runperk_rarity_name(pd->rarity), rc, kc);
            char db[96]; clip_text(pd->desc, L.iw - 72 - rw - 14, 10, db, sizeof(db));
            dtxt(db, L.px + 70 + cdx, ry + 25, 10, (Color){190, 172, 138, (unsigned char)(255.0f * kc)});
        }
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) pick_flash(r, ry + ch/2);
    }

    dtxt_c("La rareté dépend de la voie choisie.", L.cx, L.cy + L.ph/2 - IL_M - 16, 10,
           (Color){125, 106, 68, (unsigned char)(235.0f * pa)});
}

int interlude_draft_pick_at(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    DraftL L = draft_layout(rb, vw, vh);
    for (int i = 0; i < L.n; i++) {
        Rectangle r = {(float)L.px, (float)(L.py0+i*L.row), (float)L.iw, (float)(L.row-6)};
        if (CheckCollisionPointRec(vm, r)) return i;
    }
    return -1;
}

// ════════════════════════════════════════════════════════════════
// BOUTIQUE — dépenser le Renfort (entre chapitres)
// ════════════════════════════════════════════════════════════════
typedef struct { int cx, cy, pw, ph, px, iw, py0, row, n; Rectangle reroll_r, done_r; } ShopL;
static ShopL shop_layout(const RunBuild *rb, int vw, int vh) {
    ShopL L; L.cx = vw/2; L.cy = vh/2;
    int n = rb->shop_n;
    if (n < 0) n = 0;
    if (n > MAX_SHOP_OFFER) n = MAX_SHOP_OFFER;
    L.n  = n;  L.row = 56;  L.pw = 600;  L.ph = 118 + n*L.row + 28;
    L.px = L.cx - L.pw/2 + IL_M;  L.iw = L.pw - IL_M*2;
    L.py0 = (L.cy - L.ph/2 + IL_M) + 28 + 22 + IL_MS;
    /* 2 vrais boutons côte à côte (avant : 2 bandes de 16 px qui se chevauchaient) */
    float by = (float)(L.cy + L.ph/2 - IL_M - SHOP_BTN_H), half = (float)(L.iw - IL_MS) / 2.0f;
    L.reroll_r = (Rectangle){(float)L.px, by, half, SHOP_BTN_H};
    L.done_r   = (Rectangle){(float)L.px + half + IL_MS, by, half, SHOP_BTN_H};
    return L;
}

/* Bouton de la boutique : plaque + [touche] + libellé, centrés. */
static void shop_button(Rectangle b, Color acc, int hov, int enabled, const char *key,
                        const char *lbl, float pa) {
    plate_alpha(pa);                                           /* fondu d'ouverture */
    plate_button(b, acc, hov, enabled);
    int kw = mtxt(key, 9) + 10, lw = mtxt(lbl, 10);
    int x  = (int)(b.x + (b.width - (float)(kw + 8 + lw)) * 0.5f);
    plate_keycap(x, (int)(b.y + (b.height - (float)(fh(9) + 4)) * 0.5f) - 1, key,
                 enabled ? acc : IL_GREY);
    plate_alpha(1.0f);
    dtxt(lbl, x + kw + 8, (int)(b.y + (b.height - (float)fh(10)) * 0.5f), 10,
         with_a(enabled ? (hov ? acc : UI_TEXT) : IL_GREY, pa));
}

void interlude_render_shop(const RunBuild *rb, int reroll_cost, Vector2 vm, int vw, int vh) {
    ShopL L = shop_layout(rb, vw, vh);
    static int   prev_s0 = -999, prev_sn = -999;
    static float open_t  = 1e9f;
    float t  = open_clock(&open_t, &prev_s0, &prev_sn, rb->shop_offer[0], rb->shop_n);
    float pa = clamp01(t / IL_OPEN_DUR);
    float ha = clamp01((t - IL_HDR_T0) / IL_HDR_DUR);
    int   pdy = -(int)(IL_OPEN_RISE * (1.0f - ea_out_cubic(pa)));

    DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, (unsigned char)(214.0f * pa)});
    Rectangle pr = {L.cx - L.pw/2.0f, L.cy - L.ph/2.0f + (float)pdy, (float)L.pw, (float)L.ph};
    panel_with_header(pr, "RAVITAILLEMENT", IL_GREEN, pa, ha);

    /* Solde de Renfort (plaque) + aide */
    int rny = (int)pr.y + 40;
    if (ha > 0.0f) {
        char rv[32]; snprintf(rv, sizeof(rv), "RENFORT  %d", rb->renfort);
        int bw = fh(12) + 10 + mtxt(rv, 12) + 12;
        DrawRectangle(L.px, rny - 3, bw, fh(12) + 6, with_a(PLATE_WELL, ha));
        DrawRectangleLines(L.px, rny - 3, bw, fh(12) + 6, with_a(scaled(IL_GOLD, 0.6f), ha));
        draw_icon(g_icon_scrap, L.px + 5, rny, fh(12), with_a(WHITE, ha));
        dtxt(rv, L.px + 10 + fh(12), rny, 12, with_a(IL_GOLD, ha));
        const char *h = "clic / touches 1-4   ·   [R] relancer   ·   [ESPACE] partir";
        dtxt(h, L.px + L.iw - mtxt(h, 10), rny + 3, 10, (Color){135, 130, 100, (unsigned char)(235.0f * ha)});
    }

    for (int i = 0; i < L.n; i++) {
        int id = rb->shop_offer[i];
        if (id < 0 || id >= PERK_COUNT) continue;
        const PerkDef *pd = &RUN_PERKS[id];
        RunColor rcr = runperk_rarity_color(pd->rarity);
        Color rc = {rcr.r, rcr.g, rcr.b, 255};
        int maxed  = (rb->count[id] >= pd->max_stack);
        int afford = (rb->renfort >= pd->shop_cost) && !maxed;
        int ry = L.py0 + i*L.row, ch = L.row - 6;
        Rectangle r = {(float)L.px, (float)ry, (float)L.iw, (float)ch};
        int hov = CheckCollisionPointRec(vm, r);

        float fa = clamp01((t - IL_CARD_T0) / IL_CARD_FADE);
        float kc = clamp01((t - (IL_CARD_T0 + (float)i * IL_CARD_STEP)) / IL_CARD_DUR);
        if (fa <= 0.0f) continue;
        int   cdx = -(int)(IL_CARD_SLIDE * (1.0f - ea_out_cubic(kc)));
        float hk  = ui_hover_t(L.px, ry, hov);
        offer_plate(r, rc, afford ? hk : 0.0f, afford, pd->rarity >= RAR_EPIC, fa, kc, t);
        if (kc <= 0.0f) continue;

        Color tc = with_a(afford ? rc : IL_GREY, kc);
        plate_keycap(L.px + 14 + cdx, ry + ch/2 - (fh(9) + 4)/2 - 1, TextFormat("%d", i + 1), tc);
        int psz = (int)(11.0f * (1.0f + 0.15f * ea_out_back(hk)) + 0.5f);
        perk_art_draw(id, L.px + 48 + cdx, ry + ch/2, psz, tc);
        dtxt(pd->name, L.px + 66 + cdx, ry + 6, 12, tc);

        const char *cs = maxed ? "MAX" : TextFormat("%d RNF", pd->shop_cost);
        Color costc = maxed ? (Color){120, 120, 120, 255} : afford ? IL_GOLD : (Color){150, 110, 60, 255};
        int cw = tag(L.px + L.iw - 8 + cdx, ry + 7, cs, costc, kc);
        char db[96]; clip_text(pd->desc, L.iw - 66 - cw - 18, 10, db, sizeof(db));
        dtxt(db, L.px + 66 + cdx, ry + 24, 10, (Color){170, 164, 142, (unsigned char)(255.0f * kc)});

        if (afford && hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) pick_flash(r, ry + ch/2);
    }

    /* Boutons Relancer / Partir : positions FIXES (hitbox du layout). */
    char rr[56]; snprintf(rr, sizeof(rr), "Relancer les offres  (%d RNF)", reroll_cost);
    shop_button(L.reroll_r, IL_REROLL, CheckCollisionPointRec(vm, L.reroll_r),
                rb->renfort >= reroll_cost, "R", rr, pa);
    shop_button(L.done_r, IL_GREEN, CheckCollisionPointRec(vm, L.done_r), 1,
                "ESPACE", "Partir au combat", pa);
}

int interlude_shop_pick_at(const RunBuild *rb, Vector2 vm, int vw, int vh) {
    ShopL L = shop_layout(rb, vw, vh);
    for (int i = 0; i < L.n; i++) {
        Rectangle r = {(float)L.px, (float)(L.py0+i*L.row), (float)L.iw, (float)(L.row-6)};
        if (CheckCollisionPointRec(vm, r)) return i;
    }
    if (CheckCollisionPointRec(vm, L.reroll_r)) return -2;
    if (CheckCollisionPointRec(vm, L.done_r))   return -3;
    return -1;
}

/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  hud_render_disc.c ─ Fiche de découverte (ennemi, tour, unité, minerai).
 *  Extraite TELLE QUELLE de hud_render.c, dont ui_render dépassait 2 300
 *  lignes. Ses deux helpers (draw_wrapped, disc_txt) ne servaient QU'À
 *  elle : ils la suivent, sans duplication ni exposition.
 */
#include "hud_internal.h"
#include "ui_anim.h"                 // horloge + hover animés de l'UI
#include "ui_palette.h"              // palette UI (source unique)
#include "../game/campaign_data.h"   // campaign_difficulty_stage (aperçu de vague)

#define SLOT_DISC        4        // slot ui_timer (0-3 : menus)
#define DISC_BG_DUR      0.15f    // montée du fond + pop du panneau (s)
#define DISC_HDR_T0      0.10f    // début du remplissage du bandeau (s)
#define DISC_HDR_DUR     0.25f    // durée du remplissage (s)
#define DISC_SPL_T0      0.20f    // début du slide du splash (s)
#define DISC_SPL_DUR     0.25f    // durée du slide (s)
#define DISC_FLASH_T0    0.45f    // flash blanc de révélation (s)
#define DISC_FLASH_DUR   0.12f
#define DISC_BADGE_T0    0.30f    // tampon du badge catégorie (s)
#define DISC_BADGE_DUR   0.15f
#define DISC_TXT_T0      0.35f    // 1re ligne de texte (s)
#define DISC_TXT_STEP    0.05f    // délai entre lignes (s)
#define DISC_LINE_DUR    0.15f    // fondu d'une ligne (s)
#define DISC_BTN_T0      0.55f    // apparition du bouton CONTINUER (s)

// ── Texte replié sur la largeur (word-wrap) ───────────────────
// Découpe `txt` en mots (les '\n' sont traités comme des espaces) et
// rend des lignes qui tiennent dans `maxw`. Avance *py ; s'arrête à y_limit.
// Évite la troncature mot-au-milieu de l'ancien clip ligne-par-ligne.
static void draw_wrapped(const char *txt, int x, int *py, int maxw,
                         int fs, Color col, int y_limit) {
    if (!txt) return;
    char line[256]; line[0] = '\0'; int ll = 0;
    char word[96];
    int  space_w = mtxt(" ", fs);
    const char *p = txt;
    while (1) {
        while (*p == ' ' || *p == '\n' || *p == '\t') p++;   // saute séparateurs
        int wl = 0;
        while (*p && *p != ' ' && *p != '\n' && *p != '\t' && wl < 95)
            word[wl++] = *p++;
        word[wl] = '\0';
        if (wl == 0) break;   // fin de texte

        /* Le mot dépasse-t-il la largeur si on l'ajoute à la ligne courante ?
           (mesuré sans construire de chaîne combinée → pas de troncature.) */
        if (ll > 0 && mtxt(line, fs) + space_w + mtxt(word, fs) > maxw) {
            if (*py > y_limit) return;
            dtxt(line, x, *py, fs, col);
            *py += fh(fs) + 3;
            ll = 0; line[0] = '\0';
        }
        /* Ajoute le mot à la ligne courante (concaténation bornée). */
        if (ll > 0 && ll < (int)sizeof(line) - 1) line[ll++] = ' ';
        for (int k = 0; word[k] && ll < (int)sizeof(line) - 1; k++)
            line[ll++] = word[k];
        line[ll] = '\0';
    }
    if (ll > 0 && *py <= y_limit) {
        dtxt(line, x, *py, fs, col);
        *py += fh(fs) + 3;
    }
}

// Ligne de texte de la fiche : fondu + petite montée de 6 px.
static void disc_txt(const char *s, int x, int y, int fs, Color c,
                     float t, float delay) {
    float k = (t - delay) / DISC_LINE_DUR;
    if (k <= 0.0f) return;
    if (k > 1.0f) k = 1.0f;
    c.a = (unsigned char)((float)c.a * k);
    dtxt(s, x, y + (int)(6.0f * (1.0f - ea_out_cubic(k))), fs, c);
}

void hud_render_discovery(const UIState *ui, const GameState *gs) {
    (void)gs;
    if (ui->disc_count > 0) {
        const DiscEntry *de = &ui->disc_queue[0];
        int cx = g_canvas_virt_w / 2, cy = g_canvas_virt_h / 2;
        int cw = 540, ch = 400;
        int card_x = cx - cw/2, card_y = cy - ch/2;

        // Couleur thématique par catégorie
        Color cat_col =
            de->type == DISC_ENEMY    ? (Color){210,  70,  50, 255} :
            de->type == DISC_TOWER    ? (Color){215, 155,  40, 255} :
            de->type == DISC_MATERIAL ? MATERIAL_COLORS[de->idx]     :
                                        (Color){ 70, 170,  80, 255};
        Color cat_dim = (Color){cat_col.r/4, cat_col.g/4, cat_col.b/4, 255};

        /* ── Timer d'ouverture : réouverture (gap de rendu) ou fiche
           suivante (le contenu de disc_queue[0] change). ── */
        static int prev_dtype = -1, prev_didx = -1;
        if (ui_timer_gap(SLOT_DISC) > 2 ||
            (int)de->type != prev_dtype || de->idx != prev_didx) {
            ui_timer(SLOT_DISC, 1);
            prev_dtype = (int)de->type;
            prev_didx  = de->idx;
        }
        float dt_open = ui_timer(SLOT_DISC, 0);

        /* Phases de la séquence */
        float k1 = dt_open / DISC_BG_DUR;                      // fond + panneau
        if (k1 > 1.0f) k1 = 1.0f;
        float k2 = (dt_open - DISC_HDR_T0) / DISC_HDR_DUR;     // bandeau + titre
        k2 = k2 < 0.0f ? 0.0f : k2 > 1.0f ? 1.0f : k2;

        // Fond noir semi-transparent plein écran (montée en 0.15 s)
        DrawRectangle(0, 0, g_canvas_virt_w, g_canvas_virt_h,
                      (Color){0,0,0,(unsigned char)(180.0f * k1)});

        // Panneau principal : scale 0.92 → 1.0 autour du centre + fondu.
        // Seul le DESSIN du cadre est mis à l'échelle ; le contenu et les
        // hitbox (X, CONTINUER — gérées par hud_input) restent finaux.
        {
            float sc  = 0.92f + 0.08f * ea_out_cubic(k1);
            float scw = (float)cw * sc, sch = (float)ch * sc;
            Rectangle pr2 = {(float)cx - scw/2.0f, (float)cy - sch/2.0f, scw, sch};
            DrawRectangleRounded(pr2, 0.05f, 6,
                                 (Color){8,5,2,(unsigned char)(252.0f * k1)});
            DrawRectangleRoundedLinesEx(pr2, 0.05f, 6, 2.0f,
                (Color){cat_col.r, cat_col.g, cat_col.b,
                        (unsigned char)(255.0f * k1)});
        }

        // Bandeau d'en-tête : se remplit de gauche à droite (clip manuel —
        // BeginScissorMode opère en coordonnées écran, pas canvas)
        int hdr_h = 46;
        if (k2 > 0.0f) {
            DrawRectangleRounded(
                (Rectangle){(float)card_x,(float)card_y,(float)cw * k2,(float)hdr_h},
                0.1f, 4, cat_dim);
            DrawRectangleRoundedLinesEx(
                (Rectangle){(float)card_x,(float)card_y,(float)cw,(float)hdr_h},
                0.1f, 4, 1.5f,
                (Color){cat_col.r, cat_col.g, cat_col.b,
                        (unsigned char)(255.0f * k2)});
        }

        // Badge catégorie
        const char *cat_lbl =
            de->type == DISC_ENEMY    ? "ENNEMI"  :
            de->type == DISC_TOWER    ? "TOUR"    :
            de->type == DISC_MATERIAL ? "MINERAI" : "UNITE";
        int cl = mtxt(cat_lbl, 10);
        int badge_w = cl + 12, badge_h = 18;
        int bx = card_x + 12, by = card_y + hdr_h/2 - badge_h/2;
        /* Badge : « tamponne » à 0.30 s (scale 1.6 → 1.0, ea_out_back) */
        {
            float kb = (dt_open - DISC_BADGE_T0) / DISC_BADGE_DUR;
            if (kb > 0.0f) {
                float bs = (kb >= 1.0f) ? 1.0f
                         : 1.6f - 0.6f * ea_out_back(kb);
                float bw2 = (float)badge_w * bs, bh2 = (float)badge_h * bs;
                float bcx = (float)bx + badge_w/2.0f, bcy = (float)by + badge_h/2.0f;
                unsigned char ba = (unsigned char)(255.0f * (kb > 1.0f ? 1.0f : kb));
                DrawRectangleRounded(
                    (Rectangle){bcx - bw2/2.0f, bcy - bh2/2.0f, bw2, bh2},
                    0.4f, 4, (Color){cat_col.r, cat_col.g, cat_col.b, ba});
                dtxt(cat_lbl, bx + 6, by + badge_h/2 - fh(10)/2, 10,
                     (Color){8,5,2,ba});
            }
        }

        // Titre "DÉCOUVERTE !" — tracking qui se resserre (8 → 2 px)
        {
            const char *disc_title = "DECOUVERTE !";
            int   fs      = 16;
            float spacing = 2.0f + (1.0f - k2) * 6.0f;
            float total   = 0.0f;
            for (int i = 0; disc_title[i]; i++) {
                char one[2] = {disc_title[i], '\0'};
                total += (float)mtxt(one, fs);
                if (disc_title[i+1]) total += spacing;
            }
            float lx = (float)cx - total * 0.5f;
            int   ly = card_y + hdr_h/2 - fh(fs)/2;
            unsigned char ta = (unsigned char)(255.0f * k2);
            for (int i = 0; disc_title[i]; i++) {
                char one[2] = {disc_title[i], '\0'};
                dtxt(one, (int)lx, ly, fs,
                     (Color){cat_col.r, cat_col.g, cat_col.b, ta});
                lx += (float)mtxt(one, fs) + spacing;
            }
        }

        // Bouton [✕]
        int xbw = 28, xbh = 22;
        int xbx = card_x + cw - 10 - xbw, xby = card_y + 10;
        Rectangle xbtn = {(float)xbx,(float)xby,(float)xbw,(float)xbh};
        int xhov = CheckCollisionPointRec(virt_mouse(), xbtn);
        if (k2 > 0.0f) {   // apparaît avec le bandeau (hitbox : hud_input)
            Color xbg = xhov ? (Color){180,50,40,240} : (Color){60,30,20,200};
            Color xbd = xhov ? (Color){255,100,80,255} : cat_col;
            Color xtc = xhov ? WHITE : (Color){200,170,130,255};
            xbg.a = (unsigned char)((float)xbg.a * k2);
            xbd.a = (unsigned char)((float)xbd.a * k2);
            xtc.a = (unsigned char)((float)xtc.a * k2);
            DrawRectangleRounded(xbtn, 0.3f, 4, xbg);
            DrawRectangleRoundedLinesEx(xbtn, 0.3f, 4, 1.5f, xbd);
            int xw = mtxt("X", 11);
            dtxt("X", xbx + xbw/2 - xw/2, xby + xbh/2 - fh(11)/2, 11, xtc);
        }

        // Corps de la fiche
        int body_y = card_y + hdr_h + 12;
        int pad = 14;

        // Splash art
        int splash_sz = 160;
        Texture2D *spl = NULL;
        if      (de->type == DISC_ENEMY) spl = &g_enemy_splash[de->idx];
        else if (de->type == DISC_TOWER) spl = &g_tower_splash[de->idx];
        else if (de->type == DISC_UNIT)  spl = &g_unit_splash [de->idx];
        // DISC_MATERIAL : pas de splash → le fallback dessine un carre couleur minerai

        int spl_x = card_x + pad;
        int spl_y = body_y;
        /* Slide depuis la gauche (-30 px) + fondu, puis flash de révélation */
        {
            float k3 = (dt_open - DISC_SPL_T0) / DISC_SPL_DUR;
            k3 = k3 < 0.0f ? 0.0f : k3 > 1.0f ? 1.0f : k3;
            int  sxo = spl_x - (int)(30.0f * (1.0f - ea_out_cubic(k3)));
            unsigned char sa = (unsigned char)(255.0f * k3);
            if (k3 > 0.0f) {
                if (spl && spl->id != 0) {
                    DrawTexturePro(*spl,
                        (Rectangle){0,0,(float)spl->width,(float)spl->height},
                        (Rectangle){(float)sxo,(float)spl_y,
                                    (float)splash_sz,(float)splash_sz},
                        (Vector2){0,0}, 0.0f, (Color){255,255,255,sa});
                    DrawRectangleLinesEx(
                        (Rectangle){(float)sxo,(float)spl_y,
                                    (float)splash_sz,(float)splash_sz},
                        1.5f, (Color){cat_col.r, cat_col.g, cat_col.b, sa});
                } else {
                    // Fallback : carré coloré
                    DrawRectangleRounded(
                        (Rectangle){(float)sxo,(float)spl_y,
                                    (float)splash_sz,(float)splash_sz},
                        0.1f, 4,
                        (Color){cat_dim.r, cat_dim.g, cat_dim.b, sa});
                    DrawRectangleRoundedLinesEx(
                        (Rectangle){(float)sxo,(float)spl_y,
                                    (float)splash_sz,(float)splash_sz},
                        0.1f, 4, 1.5f,
                        (Color){cat_col.r, cat_col.g, cat_col.b, sa});
                }
            }
            /* Flash blanc bref qui « révèle » l'image */
            float kf = (dt_open - DISC_FLASH_T0) / DISC_FLASH_DUR;
            if (kf > 0.0f && kf < 1.0f)
                DrawRectangle(spl_x, spl_y, splash_sz, splash_sz,
                              (Color){255,255,255,
                                      (unsigned char)(90.0f * (1.0f - kf))});
        }

        // Texte à droite du splash
        int txt_x  = spl_x + splash_sz + pad;
        int txt_w  = card_x + cw - pad - txt_x;
        int txt_y  = body_y;

        // Nom de l'entité
        const char *name = NULL;
        if      (de->type == DISC_ENEMY)    name = ENEMY_BASE_STATS[de->idx].name;
        else if (de->type == DISC_TOWER)    name = TOWER_BASE_STATS[de->idx].name;
        else if (de->type == DISC_MATERIAL) name = MATERIAL_NAMES  [de->idx];
        else                                name = UNIT_BASE_STATS [de->idx].name;
        disc_txt(name, txt_x, txt_y, 20, cat_col,
                 dt_open, DISC_TXT_T0);
        txt_y += fh(20) + 2;

        // Sous-titre type
        const char *sub =
            de->type == DISC_ENEMY    ? "Ennemi rencontre" :
            de->type == DISC_TOWER    ? "Tour de defense"  :
            de->type == DISC_MATERIAL ? "Minerai - a appliquer sur une tour" :
                                        "Unite alliee";
        disc_txt(sub, txt_x, txt_y, 9, (Color){100,85,60,220},
                 dt_open, DISC_TXT_T0 + DISC_TXT_STEP);
        txt_y += fh(10) + 8;

        // Séparateur (fondu avec les lignes voisines)
        {
            float ks = (dt_open - (DISC_TXT_T0 + DISC_TXT_STEP)) / DISC_LINE_DUR;
            ks = ks < 0.0f ? 0.0f : ks > 1.0f ? 1.0f : ks;
            DrawLineEx((Vector2){(float)txt_x,(float)txt_y},
                       (Vector2){(float)(txt_x+txt_w),(float)txt_y},
                       1.2f, (Color){cat_col.r/3,cat_col.g/3,cat_col.b/3,
                                     (unsigned char)(220.0f * ks)});
        }
        txt_y += 6;

        // Stats condensées
        char stat_a[64] = {0}, stat_b[96] = {0};   // stat_b : liste des faiblesses ennemi
        if (de->type == DISC_ENEMY) {
            snprintf(stat_a, sizeof(stat_a), "PV de base : %.0f   Vitesse : %.1f   Or : %d",
                     ENEMY_BASE_STATS[de->idx].hp,
                     ENEMY_BASE_STATS[de->idx].speed,
                     ENEMY_BASE_STATS[de->idx].reward);
            // Faiblesses elementaires (mult >= 1.3) : apprend le CONTRE a utiliser.
            char wk[64]; int off = 0, wn = 0;
            for (int d = 0; d < DAMAGE_TYPE_COUNT && off < (int)sizeof(wk) - 1; d++) {
                if (ENEMY_DMG_MULT[de->idx][d] >= 1.30f)
                    off += snprintf(wk + off, sizeof(wk) - off, "%s%s",
                                    wn++ ? ", " : "", DAMAGE_NAMES[d]);
            }
            if (wn) snprintf(stat_b, sizeof(stat_b), "Faible : %s", wk);
            else    snprintf(stat_b, sizeof(stat_b), "Peu de faiblesse elementaire");
        } else if (de->type == DISC_TOWER) {
            snprintf(stat_a, sizeof(stat_a), "Cout : %d or   Degats : %.0f",
                     TOWER_BASE_STATS[de->idx].cost,
                     TOWER_BASE_STATS[de->idx].damage);
            snprintf(stat_b, sizeof(stat_b), "Portee : %.1f   Cadence : %.1f/s",
                     TOWER_BASE_STATS[de->idx].range,
                     TOWER_BASE_STATS[de->idx].fire_rate);
        } else if (de->type == DISC_MATERIAL) {
            snprintf(stat_a, sizeof(stat_a), "Applique a une tour : gros bonus");
            snprintf(stat_b, sizeof(stat_b), "Change son type de degats");
        } else {
            snprintf(stat_a, sizeof(stat_a), "Cout : %d or   PV : %.0f",
                     UNIT_BASE_STATS[de->idx].cost,
                     UNIT_BASE_STATS[de->idx].hp);
            if (de->idx == UNIT_WORKER)   /* ne combat jamais (unit.c) : pas de dégâts affichés */
                snprintf(stat_b, sizeof(stat_b), "Non combatif   Vitesse : %.1f",
                         UNIT_BASE_STATS[de->idx].speed);
            else
                snprintf(stat_b, sizeof(stat_b), "Degats : %.0f   Vitesse : %.1f",
                         UNIT_BASE_STATS[de->idx].damage,
                         UNIT_BASE_STATS[de->idx].speed);
        }
        if (stat_a[0]) {
            disc_txt(stat_a, txt_x, txt_y, 10, (Color){175,155,115,255},
                     dt_open, DISC_TXT_T0 + 2*DISC_TXT_STEP);
            txt_y += fh(10)+3;
        }
        if (stat_b[0]) {
            disc_txt(stat_b, txt_x, txt_y, 10, (Color){175,155,115,255},
                     dt_open, DISC_TXT_T0 + 3*DISC_TXT_STEP);
            txt_y += fh(10)+6;
        }

        // Séparateur (fondu)
        {
            float ks = (dt_open - (DISC_TXT_T0 + 3*DISC_TXT_STEP)) / DISC_LINE_DUR;
            ks = ks < 0.0f ? 0.0f : ks > 1.0f ? 1.0f : ks;
            DrawLineEx((Vector2){(float)txt_x,(float)txt_y},
                       (Vector2){(float)(txt_x+txt_w),(float)txt_y},
                       1.0f, (Color){cat_col.r/4,cat_col.g/4,cat_col.b/4,
                                     (unsigned char)(180.0f * ks)});
        }
        txt_y += 6;

        // ── Description + lore : PLEINE LARGEUR sous le splash ─────
        // Word-wrap sur toute la largeur de la fiche (≈512 px) au lieu de
        // tronquer chaque ligne dans la colonne étroite → plus de mots coupés.
        int desc_y = body_y + splash_sz + 12;
        if (desc_y < txt_y) desc_y = txt_y;          // si la colonne droite déborde
        int fx    = card_x + pad;
        int fw    = cw - pad*2;
        int y_lim = card_y + ch - 14 - 34 - 12;      // au-dessus du bouton CONTINUER

        const char *desc = NULL;
        if      (de->type == DISC_ENEMY)    desc = ENEMY_DESC[de->idx];
        else if (de->type == DISC_TOWER)    desc = TOWER_BASE_STATS[de->idx].description;
        else if (de->type == DISC_MATERIAL) desc = MATERIAL_DESC[de->idx];
        else                                desc = UNIT_BASE_STATS [de->idx].description;
        {   /* description puis lore : fondu en cascade (alpha seul) */
            float kd = (dt_open - (DISC_TXT_T0 + 4*DISC_TXT_STEP)) / DISC_LINE_DUR;
            kd = kd < 0.0f ? 0.0f : kd > 1.0f ? 1.0f : kd;
            if (kd > 0.0f)
                draw_wrapped(desc, fx, &desc_y, fw, 11,
                             (Color){190,170,120,(unsigned char)(255.0f*kd)},
                             y_lim);
        }
        desc_y += 5;

        const char *lore = NULL;
        if      (de->type == DISC_ENEMY)    lore = ENEMY_SPEC[de->idx];
        else if (de->type == DISC_TOWER)    lore = TOWER_LORE[de->idx];
        else if (de->type == DISC_MATERIAL) lore = MATERIAL_LORE[de->idx];
        else                                lore = UNIT_LORE [de->idx];
        {
            float kl = (dt_open - (DISC_TXT_T0 + 5*DISC_TXT_STEP)) / DISC_LINE_DUR;
            kl = kl < 0.0f ? 0.0f : kl > 1.0f ? 1.0f : kl;
            if (kl > 0.0f)
                draw_wrapped(lore, fx, &desc_y, fw, 10,
                             (Color){145,130,95,(unsigned char)(200.0f*kl)},
                             y_lim);
        }

        // Compteur si plusieurs fiches en attente — bounce quand N change
        if (ui->disc_count > 1) {
            static int   prev_n   = -1;
            static float bounce_t = 1e9f;
            if (ui->disc_count != prev_n) {
                if (prev_n >= 0) bounce_t = 0.0f;
                prev_n = ui->disc_count;
            }
            bounce_t += ui_dt();
            int qfs = 9;
            if (bounce_t < 0.20f)   /* scale 1.3 → 1.0 via taille de police */
                qfs = 9 + (int)(3.0f * (1.0f - ea_out_cubic(bounce_t / 0.20f)));
            char qbuf[32];
            snprintf(qbuf, sizeof(qbuf), "+%d en attente", ui->disc_count - 1);
            int qw = mtxt(qbuf, qfs);
            dtxt(qbuf, card_x + cw - pad - qw,
                 card_y + ch - 14 - 34 - 4 - fh(10), qfs,
                 (Color){100,85,60,200});
        }

        // Bouton CONTINUER — apparaît à 0.55 s, cliquable dès la frame 1
        // (le clic est géré par hud_input.c, indépendant du dessin).
        {
            float kb2 = (dt_open - DISC_BTN_T0) / DISC_LINE_DUR;
            kb2 = kb2 < 0.0f ? 0.0f : kb2 > 1.0f ? 1.0f : kb2;
            if (kb2 > 0.0f) {
                int btnw = 160, btnh = 34;
                int btnx = cx - btnw/2, btny = card_y + ch - 14 - btnh;
                Rectangle cont = {(float)btnx,(float)btny,(float)btnw,(float)btnh};
                int chov = CheckCollisionPointRec(virt_mouse(), cont);
                Color cbg = chov ? cat_col : cat_dim;
                Color ctc = chov ? (Color){8,5,2,255} : cat_col;
                cbg.a = (unsigned char)((float)cbg.a * kb2);
                ctc.a = (unsigned char)((float)ctc.a * kb2);
                DrawRectangleRounded(cont, 0.3f, 4, cbg);
                DrawRectangleRoundedLinesEx(cont, 0.3f, 4, 1.5f,
                    (Color){cat_col.r, cat_col.g, cat_col.b,
                            (unsigned char)(255.0f * kb2)});
                const char *btn_lbl = ui->disc_count > 1 ? "SUIVANT >" : "CONTINUER";
                int blw = mtxt(btn_lbl, 12);
                dtxt(btn_lbl, btnx + btnw/2 - blw/2,
                     btny + btnh/2 - fh(12)/2, 12, ctc);
            }
        }
    }
}

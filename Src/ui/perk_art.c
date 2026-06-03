/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "perk_art.h"
#include "../game/runperks.h"   // PerkId
#include <math.h>

// Éclaircit/assombrit une couleur (facteur multiplicatif), alpha conservé.
static Color pa_shade(Color c, float f) {
    int r = (int)(c.r * f), g = (int)(c.g * f), b = (int)(c.b * f);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, c.a};
}

// Étincelle 4 branches (deux losanges allongés) — accents d'épiques.
static void pa_sparkle(int cx, int cy, float rad, Color c) {
    DrawTriangle((Vector2){cx, cy - rad}, (Vector2){cx - rad*0.32f, cy},
                 (Vector2){cx, cy + rad}, c);
    DrawTriangle((Vector2){cx, cy + rad}, (Vector2){cx + rad*0.32f, cy},
                 (Vector2){cx, cy - rad}, c);
    DrawTriangle((Vector2){cx - rad, cy}, (Vector2){cx, cy - rad*0.32f},
                 (Vector2){cx + rad, cy}, c);
    DrawTriangle((Vector2){cx + rad, cy}, (Vector2){cx, cy + rad*0.32f},
                 (Vector2){cx - rad, cy}, c);
}

void perk_art_draw(int perk_id, int cx, int cy, int r, Color col) {
    Color sh  = pa_shade(col, 0.50f);
    Color lt  = pa_shade(col, 1.55f);
    Color wht = (Color){255, 255, 255, 230};
    Color gold= (Color){236, 198,  82, 255};
    float u   = (float)r / 8.0f;          // 1 cellule = r/8 px

    #define X(k)  (cx + (int)((k) * u))
    #define Y(k)  (cy + (int)((k) * u))
    #define S(k)  ((int)((k) * u + 0.5f))
    #define RECT(x,y,w,h,c) DrawRectangle(X(x), Y(y), S(w), S(h), (c))
    #define VEC(x,y) (Vector2){(float)X(x), (float)Y(y)}

    switch (perk_id) {

    // ── TOURS : dégâts par type ──────────────────────────────────
    case PERK_GUN:        // tourelle à double canon
        RECT(-6, 2, 12, 4, sh);                 // socle
        RECT(-4,-3,  8, 6, col);                // tourelle
        RECT(-4,-3,  8, 2, lt);                 // arête éclairée
        RECT( 2,-2,  6, 2, sh);                 // canon haut
        RECT( 2, 1,  6, 2, sh);                 // canon bas
        DrawCircle(X(8), Y(-1), S(1.4f), (Color){255, 224, 120, 220});
        DrawCircle(X(8), Y( 2), S(1.4f), (Color){255, 224, 120, 220});
        break;

    case PERK_SNIPER:     // lunette de visée
        DrawCircle(cx, cy, S(7.2f), sh);
        DrawCircle(cx, cy, S(6.0f), col);
        DrawCircle(cx, cy, S(4.2f), (Color){12, 10, 8, 255});
        DrawLine(X(-8), cy, X(8), cy, lt);
        DrawLine(cx, Y(-8), cx, Y(8), lt);
        DrawCircle(cx, cy, S(1.2f), (Color){255, 90, 70, 255});
        break;

    case PERK_FLAME: {    // flamme
        Color fl = (Color){246, 150, 40, 255};
        DrawCircle(X(0), Y(3), S(4.2f), pa_shade(col, 0.9f));
        DrawTriangle(VEC(-4,3), VEC(4,3), VEC(0,-8), pa_shade(col, 0.9f));
        DrawCircle(X(0), Y(4), S(2.4f), fl);
        DrawTriangle(VEC(-2.3f,4), VEC(2.3f,4), VEC(0,-3), fl);
        DrawCircle(X(0), Y(4), S(1.1f), (Color){255, 232, 150, 255});
        break;
    }

    case PERK_TESLA: {    // éclair
        Color spark = (Color){150, 210, 255, 255};
        DrawLineEx(VEC(2,-8), VEC(-2,0), 2.4f*u, col);
        DrawLineEx(VEC(-2,0), VEC(2,0),  2.4f*u, col);
        DrawLineEx(VEC(2,0),  VEC(-2,8), 2.4f*u, col);
        DrawLineEx(VEC(2,-8), VEC(-2,0), 1.0f*u, spark);
        DrawLineEx(VEC(2,0),  VEC(-2,8), 1.0f*u, spark);
        DrawCircle(X(4), Y(-4), S(0.9f), spark);
        DrawCircle(X(-4),Y(5),  S(0.9f), spark);
        break;
    }

    // ── TOURS : globaux ──────────────────────────────────────────
    case PERK_PUISSANCE:  // surcharge : grosse flèche montante + base
        DrawTriangle(VEC(-6,-1), VEC(6,-1), VEC(0,-8), col);
        RECT(-2,-1, 4, 8, col);
        RECT(-2,-1, 4, 2, lt);
        RECT(-5, 7, 10, 2, sh);
        break;

    case PERK_PORTEE:     // portée : anneaux + flèche vers l'extérieur
        DrawCircleLines(X(-3), cy, S(3.0f), sh);
        DrawCircleLines(X(-3), cy, S(5.4f), col);
        DrawLineEx(VEC(-3,0), VEC(6,0), 1.8f*u, col);
        DrawTriangle(VEC(5,-3), VEC(5,3), VEC(9,0), lt);
        break;

    case PERK_CADENCE: {  // servomoteurs : engrenage
        for (int i = 0; i < 8; i++) {
            float a = (float)i * (PI / 4.0f);
            Vector2 c = {cx + cosf(a) * 5.2f * u, cy + sinf(a) * 5.2f * u};
            DrawRectanglePro((Rectangle){c.x, c.y, 2.6f*u, 2.6f*u},
                             (Vector2){1.3f*u, 1.3f*u}, a * RAD2DEG, col);
        }
        DrawCircle(cx, cy, S(4.4f), col);
        DrawCircle(cx, cy, S(3.4f), lt);
        DrawCircle(cx, cy, S(1.8f), (Color){14, 11, 8, 255});
        break;
    }

    case PERK_GLASS:      // canon de verre : gemme fendue
        DrawPoly((Vector2){cx, cy}, 4, r * 0.9f, 45.0f, sh);
        DrawPoly((Vector2){cx, cy}, 4, r * 0.72f, 45.0f, col);
        DrawPoly((Vector2){(float)cx, (float)Y(-2)}, 3, r * 0.42f, 0.0f, lt);
        DrawLine(X(0), Y(-7), X(1), Y(0), wht);
        DrawLine(X(1), Y(0),  X(-1), Y(7), wht);
        DrawPolyLines((Vector2){cx, cy}, 4, r * 0.9f, 45.0f, lt);
        break;

    // ── ÉCONOMIE ─────────────────────────────────────────────────
    case PERK_BUTIN:      // pillage : pièce + réticule
        DrawCircle(cx, cy, S(5.4f), gold);
        DrawCircle(cx, cy, S(4.0f), pa_shade(gold, 0.7f));
        DrawCircleLines(cx, cy, S(5.4f), pa_shade(gold, 1.3f));
        DrawLine(X(-7), cy, X(7), cy, (Color){30, 22, 8, 200});
        DrawLine(cx, Y(-7), cx, Y(7), (Color){30, 22, 8, 200});
        DrawCircle(cx, cy, S(1.0f), (Color){255, 80, 60, 255});
        break;

    case PERK_SUBV:       // subvention : pile de pièces
        DrawEllipse(cx, Y(4),  S(4.4f), S(1.8f), pa_shade(gold, 0.7f));
        DrawEllipse(cx, Y(1),  S(4.4f), S(1.8f), gold);
        DrawEllipse(cx, Y(-2), S(4.4f), S(1.8f), pa_shade(gold, 1.2f));
        DrawEllipseLines(cx, Y(-2), S(4.4f), S(1.8f), pa_shade(gold, 0.6f));
        break;

    case PERK_REVENTE:    // recyclage : flèches circulaires + pièce
        DrawRing((Vector2){cx, cy}, 4.4f*u, 6.2f*u, -30.0f, 130.0f, 18, col);
        DrawRing((Vector2){cx, cy}, 4.4f*u, 6.2f*u, 150.0f, 310.0f, 18, col);
        DrawTriangle(VEC(6.2f,-2), VEC(6.2f,2.4f), VEC(8.6f,0.2f), lt);
        DrawTriangle(VEC(-6.2f,2), VEC(-6.2f,-2.4f), VEC(-8.6f,-0.2f), lt);
        DrawCircle(cx, cy, S(2.6f), gold);
        break;

    case PERK_CAPITAL:    // capitaliste : pièce + flèche montante
        DrawCircle(X(-3), cy, S(4.2f), gold);
        DrawCircle(X(-3), cy, S(2.8f), pa_shade(gold, 0.7f));
        DrawTriangle(VEC(1,-1), VEC(7,-1), VEC(4,-7), lt);
        RECT(3, -1, 2, 7, lt);
        break;

    // ── UNITÉS ───────────────────────────────────────────────────
    case PERK_BLINDAGE:   // blindage : bouclier riveté
        RECT(-5, -6, 10, 7, col);
        DrawTriangle(VEC(-5,1), VEC(5,1), VEC(0,7), col);
        RECT(-5, -6, 10, 2, lt);
        DrawCircle(cx, Y(-2), S(1.4f), lt);
        DrawCircle(cx, Y(-2), S(0.6f), sh);
        break;

    case PERK_FUSIL:      // armement lourd : fusil
        RECT(-7, -1, 13, 2.4f, col);            // canon
        RECT(-7, -1.6f, 3, 5, sh);              // crosse
        RECT(-1, 1.4f, 2, 3.4f, sh);            // chargeur
        RECT(3, -2.4f, 1.6f, 1.6f, lt);         // guidon
        break;

    case PERK_MEDIC:      // trousse de soin
        DrawRectangleRounded((Rectangle){X(-6), Y(-5), S(12), S(10)}, 0.3f, 5, col);
        DrawRectangleRoundedLinesEx((Rectangle){X(-6), Y(-5), S(12), S(10)},
                                    0.3f, 5, 1.5f, lt);
        RECT(-1, -3, 2, 6, wht);
        RECT(-3, -1, 6, 2, wht);
        break;

    // ── SURVIE ───────────────────────────────────────────────────
    case PERK_GARNISON: {  // garnison : cœur (vies)
        Color hb = (Color){226, 76, 72, 255};
        DrawCircle(X(-2.4f), Y(-2), S(2.7f), hb);
        DrawCircle(X(2.4f),  Y(-2), S(2.7f), hb);
        DrawTriangle(VEC(-5,-1), VEC(5,-1), VEC(0,6.5f), hb);
        DrawCircle(X(-2.4f), Y(-3), S(1.0f), (Color){255, 170, 165, 255});
        break;
    }

    // ── ÉPIQUES ──────────────────────────────────────────────────
    case PERK_ARSENAL: {   // arsenal : tourelle dans une étoile
        for (int i = 0; i < 8; i++) {
            float a = (float)i * (PI / 4.0f);
            DrawLineEx((Vector2){cx, cy},
                       (Vector2){cx + cosf(a)*7.4f*u, cy + sinf(a)*7.4f*u},
                       1.2f*u, pa_shade(col, 1.2f));
        }
        DrawCircle(cx, cy, S(4.6f), sh);
        RECT(-3, 1, 6, 3, col);                 // socle
        RECT(-2,-2, 4, 4, col);                 // tourelle
        RECT(1,-1, 4, 1.6f, lt);                // canon
        pa_sparkle(X(5), Y(-5), 2.4f*u, wht);
        break;
    }

    case PERK_FORTUNE:     // fortune : grosse pièce étoilée
        DrawCircle(cx, cy, S(6.2f), gold);
        DrawCircle(cx, cy, S(4.6f), pa_shade(gold, 0.72f));
        DrawCircleLines(cx, cy, S(6.2f), pa_shade(gold, 1.3f));
        RECT(-1, -3, 2, 6, pa_shade(gold, 0.5f));
        pa_sparkle(X(4), Y(-4), 2.6f*u, wht);
        pa_sparkle(X(-5), Y(3), 1.6f*u, wht);
        break;

    case PERK_LEGION: {    // légion : épées croisées
        DrawLineEx(VEC(-6,6), VEC(6,-6), 2.0f*u, pa_shade(col, 1.4f));
        DrawLineEx(VEC(6,6),  VEC(-6,-6),2.0f*u, pa_shade(col, 1.4f));
        DrawLineEx(VEC(-6,6), VEC(6,-6), 0.7f*u, wht);
        DrawLineEx(VEC(6,6),  VEC(-6,-6),0.7f*u, wht);
        RECT(-4, 4, 8, 1.6f, sh);               // gardes (bas)
        DrawCircle(X(-5), Y(6), S(1.2f), gold); // pommeaux
        DrawCircle(X(5),  Y(6), S(1.2f), gold);
        break;
    }

    default:               // repli : pastille générique
        DrawCircle(cx, cy, S(5.0f), col);
        DrawCircle(cx, cy, S(3.0f), sh);
        break;
    }

    #undef X
    #undef Y
    #undef S
    #undef RECT
    #undef VEC
}

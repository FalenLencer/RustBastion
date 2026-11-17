/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_portraits.c ─ Bustes pixel-art des personnages du récit.
 *
 *  Chaque buste est composé de rectangles sur une grille de
 *  STORY_PORTRAIT_CELLS × STORY_PORTRAIT_CELLS cellules (cf. story_art.h).
 *  Dessiner par rectangles — plutôt que par texture — donne des formes
 *  nettes à toute échelle et rend l'animation gratuite : clignement,
 *  bouche qui s'ouvre pendant que la réplique défile, œil de NEXUS.
 *
 *  Règle de lisibilité : une silhouette + UN accessoire distinctif par
 *  personnage (casquette de Voss, lunettes relevées de Kira, lunettes de
 *  soudure de Varela, masque d'acier du Chef, œil de NEXUS, casquette
 *  d'officier en tôle et réticule du général machine). On doit les
 *  reconnaître sans lire le nom.
 */
#include "story_art.h"
#include <math.h>
#include <string.h>

// ── Rythmes d'animation ──────────────────────────────────────────
#define PT_BLINK_PERIOD  4.3f   /* intervalle entre deux clignements (s)   */
#define PT_BLINK_DUR     0.12f  /* durée d'un clignement (s)               */
#define PT_TALK_RATE     9.0f   /* battements de bouche par seconde        */
#define PT_VISOR_RATE    6.0f   /* scintillement de la visée du Chef (rad/s) */
#define PT_EYE_IDLE      3.0f   /* pulsation de l'œil de NEXUS au repos     */
#define PT_EYE_TALK      11.0f  /* … quand il parle                         */
#define PT_SCAN_RATE     7.0f   /* balayage de NEXUS (cellules/s)           */
#define PT_RETICLE_RATE  1.7f   /* va-et-vient du réticule du général (rad/s) */
#define PT_SLIT_W        8      /* largeur de sa fente de visée (cellules)  */

// ── Contexte de dessin (posé par story_portrait_draw) ────────────
static int   g_ox, g_oy, g_px;
static float g_al;

static Color ca(Color c) {
    c.a = (unsigned char)((float)c.a * g_al);
    return c;
}
/* Rectangle en CELLULES (x,y,w,h) → pixels. */
#define P(x, y, w, h, c) \
    DrawRectangle(g_ox + (x) * g_px, g_oy + (y) * g_px, \
                  (w) * g_px, (h) * g_px, ca(c))

static int blinking(float t, float phase) {
    return fmodf(t + phase, PT_BLINK_PERIOD) < PT_BLINK_DUR;
}
static int mouth_open(float t, int talk) {
    return talk && (((int)(t * PT_TALK_RATE)) & 1);
}

/* Paire d'yeux standard (2 cellules de large chacun), ligne `row`. */
static void eyes(int row, int blink, Color white, Color pupil, Color lid) {
    if (blink) {
        P(9, row, 2, 1, lid);  P(13, row, 2, 1, lid);
        return;
    }
    P(9, row, 2, 1, white);   P(10, row, 1, 1, pupil);
    P(13, row, 2, 1, white);  P(13, row, 1, 1, pupil);
}

// ── Couleurs partagées ───────────────────────────────────────────
#define C_DARK  (Color){ 24,  18,  14, 255}
#define C_WHITE (Color){236, 230, 214, 255}

// ════════════════════════════════════════════════════════════════
// VOSS — commandant : casquette d'officier, barbe grise, cicatrice
// ════════════════════════════════════════════════════════════════
static void draw_voss(float t, int talk) {
    const Color skin = {205, 155, 118, 255}, skd  = {162, 116,  86, 255};
    const Color cap  = { 58,  72,  48, 255}, capl = { 88, 106,  72, 255};
    const Color capd = { 30,  36,  26, 255}, amb  = {232, 168,  44, 255};
    const Color gry  = {150, 150, 144, 255}, gryl = {200, 200, 192, 255};
    const Color coat = { 52,  46,  38, 255}, coatl= { 84,  74,  60, 255};
    const Color scar = {150,  74,  62, 255};

    P(2, 19, 20, 5, coat);   P(4, 18, 16, 1, coat);       // manteau
    P(6, 18, 4, 6, coatl);   P(14, 18, 4, 6, coatl);      // revers
    P(4, 20, 2, 1, amb);     P(18, 20, 2, 1, amb);        // galons
    P(10, 16, 4, 3, skd);                                 // cou

    P(7, 6, 10, 11, skin);                                // tête
    P(7, 6, 1, 11, skd);     P(16, 6, 1, 11, skd);
    P(6, 10, 1, 3, skd);     P(17, 10, 1, 3, skd);        // oreilles

    P(7, 1, 10, 1, cap);     P(6, 2, 12, 3, cap);         // casquette
    P(7, 2, 10, 1, capl);
    P(6, 5, 12, 1, capd);    P(11, 3, 2, 2, amb);         // bandeau + insigne
    P(5, 6, 14, 1, C_DARK);  P(6, 7, 12, 1, skd);         // visière + ombre

    P(8, 8, 3, 1, gry);      P(13, 8, 3, 1, gry);         // sourcils
    eyes(9, blinking(t, 0.0f), C_WHITE, C_DARK, skd);
    P(12, 10, 1, 3, skd);                                 // nez
    P(15, 8, 1, 4, scar);                                 // cicatrice

    P(7, 13, 10, 4, gry);    P(8, 17, 8, 1, gry);         // barbe
    P(9, 12, 6, 1, gryl);                                 // moustache
    P(8, 14, 1, 2, gryl);    P(15, 14, 1, 2, gryl);
    P(10, 14, 4, mouth_open(t, talk) ? 2 : 1, C_DARK);    // bouche
}

// ════════════════════════════════════════════════════════════════
// KIRA — sergent : casque, lunettes relevées, peinture de guerre
// ════════════════════════════════════════════════════════════════
static void draw_kira(float t, int talk) {
    const Color skin  = {178, 126,  94, 255}, skd   = {140,  96,  70, 255};
    const Color helm  = { 84,  96,  60, 255}, helml = {116, 130,  86, 255};
    const Color helmd = { 50,  58,  36, 255}, strap = { 36,  34,  28, 255};
    const Color glass = {110, 200, 210, 255}, hair  = {122,  52,  36, 255};
    const Color paint = { 38,  54,  32, 255}, vest  = { 66,  72,  50, 255};
    /* Chèche SABLE, et posé SOUS le menton : en brun-rouge et remonté sur
       la mâchoire, il se lisait comme une barbe rousse à 96 px. */
    const Color scarf = {196, 168, 116, 255}, scarfd= {150, 124,  80, 255};

    P(2, 19, 20, 5, vest);   P(4, 18, 16, 1, vest);       // gilet
    P(5, 19, 2, 5, strap);   P(17, 19, 2, 5, strap);      // bretelles
    P(10, 16, 4, 2, skd);                                 // cou
    P(6, 17, 12, 3, scarf);  P(8, 20, 8, 1, scarf);       // chèche
    P(6, 19, 12, 1, scarfd); P(11, 17, 2, 4, scarfd);     // plis

    P(7, 7, 10, 9, skin);    P(8, 16, 8, 1, skin);        // tête + menton
    P(7, 7, 1, 9, skd);      P(16, 7, 1, 9, skd);

    P(6, 1, 12, 1, helm);    P(5, 2, 14, 5, helm);        // casque
    P(7, 2, 8, 1, helml);    P(5, 7, 14, 1, helmd);
    P(6, 8, 1, 7, strap);    P(17, 8, 1, 7, strap);       // jugulaire

    P(5, 4, 14, 1, C_DARK);                               // lunettes relevées
    P(7, 3, 4, 3, C_DARK);   P(8, 4, 2, 1, glass);
    P(13, 3, 4, 3, C_DARK);  P(14, 4, 2, 1, glass);

    P(7, 8, 2, 1, hair);     P(15, 8, 2, 1, hair);        // mèches
    P(8, 9, 3, 1, hair);     P(13, 9, 3, 1, hair);        // sourcils
    eyes(10, blinking(t, 1.7f), C_WHITE, C_DARK, skd);
    P(8, 12, 3, 1, paint);   P(13, 12, 3, 1, paint);      // peinture de guerre
    P(12, 11, 1, 2, skd);                                 // nez
    P(10, 14, 4, mouth_open(t, talk) ? 2 : 1, C_DARK);    // bouche
}

// ════════════════════════════════════════════════════════════════
// VARELA — ingénieure : lunettes de soudure, chignon, cambouis
// ════════════════════════════════════════════════════════════════
static void draw_varela(float t, int talk) {
    const Color skin  = {224, 174, 136, 255}, skd   = {184, 134, 100, 255};
    const Color hair  = { 46,  34,  30, 255}, hairl = { 88,  66,  54, 255};
    const Color metal = {124, 128, 132, 255}, metald= { 70,  72,  78, 255};
    const Color amb   = {240, 176,  48, 255}, ambl  = {255, 226, 132, 255};
    const Color cov   = { 64,  88, 112, 255}, covl  = { 94, 124, 152, 255};
    const Color tan   = {150, 112,  70, 255}, smudge= { 70,  64,  60, 255};
    const Color lip   = {150,  72,  66, 255};
    /* Les verres captent une lueur d'atelier : léger scintillement. */
    Color lens = (sinf(t * 2.3f) > 0.6f) ? ambl : amb;

    P(2, 19, 20, 5, cov);    P(4, 18, 16, 1, cov);        // combinaison
    P(8, 18, 8, 1, covl);
    P(14, 18, 2, 6, tan);                                 // sangle
    P(5, 21, 3, 2, C_DARK);  P(6, 20, 1, 1, amb);         // poche à outils
    P(10, 16, 4, 2, skd);                                 // cou

    P(7, 6, 10, 11, skin);                                // tête
    P(7, 6, 1, 11, skd);     P(16, 6, 1, 11, skd);

    P(7, 1, 10, 1, hair);    P(6, 2, 12, 4, hair);        // cheveux
    P(6, 6, 1, 8, hair);     P(17, 6, 1, 8, hair);
    P(17, 0, 4, 4, hair);    P(18, 1, 2, 1, hairl);       // chignon
    P(8, 2, 6, 1, hairl);

    P(6, 5, 12, 1, metald);                               // lunettes de soudure
    P(7, 4, 4, 3, metal);    P(8, 5, 2, 1, lens);
    P(13, 4, 4, 3, metal);   P(14, 5, 2, 1, lens);

    P(8, 9, 3, 1, hair);     P(13, 9, 3, 1, hair);        // sourcils
    eyes(10, blinking(t, 2.9f), C_WHITE, C_DARK, skd);
    P(12, 11, 1, 2, skd);                                 // nez
    P(15, 12, 2, 1, smudge);                              // trace de cambouis
    P(10, 14, 4, mouth_open(t, talk) ? 2 : 1, lip);       // bouche
}

// ════════════════════════════════════════════════════════════════
// CHEF DE LA FACTION ACIER — masque intégral, visée rouge
// ════════════════════════════════════════════════════════════════
static void draw_steel(float t, int talk) {
    const Color st  = {132, 138, 146, 255}, stl = {190, 196, 204, 255};
    const Color std = { 78,  82,  90, 255}, blk = { 14,  12,  12, 255};
    const Color cloth = { 40, 34, 34, 255}, crest = {150, 30, 30, 255};
    float flick = 0.72f + 0.28f * sinf(t * PT_VISOR_RATE);
    Color red = {(unsigned char)(226.0f * flick), (unsigned char)(54.0f * flick),
                 (unsigned char)(40.0f * flick), 255};
    /* Il parle derrière sa grille : elle s'éclaire au rythme de la voix. */
    Color grille = mouth_open(t, talk) ? st : std;

    P(1, 17, 7, 7, st);      P(16, 17, 7, 7, st);         // épaulières
    P(1, 17, 7, 1, stl);     P(16, 17, 7, 1, stl);
    P(2, 15, 1, 2, stl);     P(4, 14, 1, 3, stl);         // pointes
    P(19, 14, 1, 3, stl);    P(21, 15, 1, 2, stl);
    P(8, 19, 8, 5, cloth);   P(11, 20, 2, 4, std);        // plastron
    P(3, 20, 1, 1, std);     P(5, 22, 1, 1, std);         // rivets
    P(18, 20, 1, 1, std);    P(20, 22, 1, 1, std);

    P(7, 2, 10, 1, st);      P(6, 3, 12, 14, st);         // casque
    P(7, 17, 10, 1, std);
    P(7, 3, 2, 12, stl);                                  // reflet
    P(16, 3, 2, 14, std);                                 // côté ombre
    P(11, 0, 2, 4, crest);                                // cimier
    P(9, 6, 1, 1, std);      P(14, 6, 1, 1, std);         // rivets de tempe

    P(7, 9, 10, 2, blk);                                  // fente de visée
    P(8, 9, 3, 1, red);      P(13, 9, 3, 1, red);

    P(9, 13, 6, 3, grille);                               // grille respiratoire
    P(10, 13, 1, 3, blk);    P(12, 13, 1, 3, blk);   P(14, 13, 1, 3, blk);
}

// ════════════════════════════════════════════════════════════════
// NEXUS — l'IA : pas de visage, un œil
// ════════════════════════════════════════════════════════════════
static void draw_nexus(float t, int talk) {
    const Color core  = { 22,   8,  10, 255}, ring  = { 96,  18,  20, 255};
    const Color trace = {128,  26,  26, 255}, pit   = {  8,   2,   4, 255};
    float pulse = 0.5f + 0.5f * sinf(t * (talk ? PT_EYE_TALK : PT_EYE_IDLE));
    Color iris  = {(unsigned char)(150.0f + 90.0f * pulse),
                   (unsigned char)( 24.0f + 30.0f * pulse),
                   (unsigned char)( 22.0f + 20.0f * pulse), 255};
    Color pupil = (pulse > 0.55f) ? (Color){255, 214, 186, 255}
                                  : (Color){255, 120,  96, 255};

    P(6, 2, 12, 20, core);   P(4, 4, 16, 16, core);       // boîtier octogonal
    P(2, 6, 20, 12, core);
    P(6, 2, 12, 1, ring);    P(6, 21, 12, 1, ring);       // arêtes
    P(2, 6, 1, 12, ring);    P(21, 6, 1, 12, ring);
    P(4, 4, 2, 1, ring);     P(18, 4, 2, 1, ring);
    P(4, 19, 2, 1, ring);    P(18, 19, 2, 1, ring);

    /* Pistes de circuit : quatre bras SYMÉTRIQUES vers l'orbite. Des
       tronçons épars donnaient un portrait brouillon ; la symétrie dit
       « machine ». */
    P(0, 11, 8, 2, trace);   P(16, 11, 8, 2, trace);
    P(11, 0, 2, 8, trace);   P(11, 16, 2, 8, trace);
    P(4, 11, 1, 2, ring);    P(19, 11, 1, 2, ring);       // nœuds
    P(11, 4, 2, 1, ring);    P(11, 19, 2, 1, ring);

    P(8, 8, 8, 8, ring);     P(9, 9, 6, 6, pit);          // orbite
    P(10, 10, 4, 4, iris);   P(11, 11, 2, 2, pupil);      // iris + pupille

    /* Balayage : une ligne claire descend dans le boîtier. */
    int sy = 4 + (int)fmodf(t * PT_SCAN_RATE, 16.0f);
    P(4, sy, 16, 1, ((Color){255, 70, 56, 38}));
}

// ════════════════════════════════════════════════════════════════
// GÉNÉRAL MACHINE — IA d'artillerie : une machine qui porte l'uniforme.
// Casquette d'officier en tôle, fente de visée où court un réticule.
// ════════════════════════════════════════════════════════════════
static void draw_general(float t, int talk) {
    const Color hull  = {148, 140, 112, 255}, hulll = {194, 186, 150, 255};
    const Color hulld = { 92,  86,  66, 255}, blk   = { 16,  14,  10, 255};
    const Color unif  = { 58,  62,  42, 255}, unifl = { 84,  90,  60, 255};
    const Color cap   = { 64,  70,  46, 255}, capl  = { 94, 102,  68, 255};
    const Color gold  = {230, 186,  70, 255}, red   = {170,  50,  40, 255};
    const Color blue  = { 60,  90, 150, 255};
    const Color amber = {255, 178,  48, 255}, ambd  = {140,  86,  26, 255};
    /* Le réticule balaie la fente : il CHERCHE une cible, même au repos. */
    int sx = 8 + (int)((float)(PT_SLIT_W - 1) * 0.5f * (1.0f + sinf(t * PT_RETICLE_RATE)));
    /* Il parle par sa grille de diffusion : elle s'allume avec la voix. */
    Color grille = mouth_open(t, talk) ? amber : hulld;

    P(2, 19, 20, 5, unif);   P(4, 18, 16, 1, unif);       // vareuse
    P(10, 18, 4, 6, unifl);                               // plastron
    P(1, 18, 5, 2, gold);    P(18, 18, 5, 2, gold);       // épaulettes
    P(1, 20, 1, 1, gold);    P(3, 20, 1, 1, gold);   P(5, 20, 1, 1, gold);
    P(18, 20, 1, 1, gold);   P(20, 20, 1, 1, gold);  P(22, 20, 1, 1, gold);
    P(6, 21, 1, 1, red);     P(7, 21, 1, 1, blue);   P(8, 21, 1, 1, gold);  // barrettes
    P(6, 22, 1, 1, gold);    P(7, 22, 1, 1, red);    P(8, 22, 1, 1, blue);
    P(10, 16, 4, 2, hulld);  P(11, 16, 1, 2, blk);        // cou à vérins

    P(7, 7, 10, 9, hull);    P(8, 16, 8, 1, hull);        // boîtier crânien
    P(7, 7, 1, 9, hulll);    P(16, 7, 1, 9, hulld);
    P(6, 9, 1, 4, hulld);    P(17, 9, 1, 4, hulld);       // capteurs latéraux
    P(8, 12, 1, 1, hulld);   P(15, 12, 1, 1, hulld);      // rivets

    P(7, 1, 10, 1, cap);     P(6, 2, 12, 4, cap);         // casquette d'officier
    P(7, 2, 10, 1, capl);
    P(6, 5, 12, 1, gold);                                 // cordon
    P(11, 3, 2, 2, gold);                                 // insigne
    P(5, 6, 14, 1, blk);                                  // visière
    P(18, 0, 1, 3, hulld);   P(18, 0, 1, 1, talk ? amber : ambd);  // antenne

    P(8, 9, PT_SLIT_W, 2, blk);                           // fente de visée
    if (sx > 8)                 P(sx - 1, 9, 1, 2, ambd);
    if (sx < 8 + PT_SLIT_W - 1) P(sx + 1, 9, 1, 2, ambd);
    P(sx, 9, 1, 2, amber);                                // réticule

    P(9, 13, 6, 2, grille);                               // grille de diffusion
    P(10, 13, 1, 2, blk);    P(12, 13, 1, 2, blk);   P(14, 13, 1, 2, blk);
}

// ════════════════════════════════════════════════════════════════
// RADIO — voix non identifiée : opérateur au casque
// ════════════════════════════════════════════════════════════════
static void draw_radio(float t, int talk) {
    const Color skin = {196, 148, 112, 255}, skd  = {154, 110,  82, 255};
    const Color cap  = { 70,  66,  52, 255}, capl = {100,  94,  74, 255};
    const Color jack = { 60,  64,  50, 255}, jackl= { 86,  92,  70, 255};
    const Color set  = { 30,  28,  26, 255}, led  = {120, 220, 130, 255};

    P(2, 19, 20, 5, jack);   P(4, 18, 16, 1, jack);       // veste
    P(8, 18, 8, 1, jackl);
    P(10, 16, 4, 3, skd);                                 // cou

    P(7, 6, 10, 11, skin);                                // tête
    P(7, 6, 1, 11, skd);     P(16, 6, 1, 11, skd);

    P(7, 2, 10, 1, cap);     P(6, 3, 12, 3, cap);         // bonnet
    P(7, 3, 10, 1, capl);
    P(6, 1, 12, 1, set);                                  // arceau du casque
    P(5, 2, 1, 7, set);      P(18, 2, 1, 7, set);
    P(5, 9, 2, 4, set);      P(17, 9, 2, 4, set);         // écouteurs
    /* Perche du micro : longe la joue et s'arrête AVANT la bouche (posée
       dessus, elle se confondait avec les lèvres). */
    P(6, 13, 1, 3, set);     P(7, 15, 2, 1, set);
    P(8, 14, 1, 2, led);                                  // capsule
    P(5, 10, 1, 1, talk ? led : set);                     // voyant d'émission

    P(8, 8, 3, 1, C_DARK);   P(13, 8, 3, 1, C_DARK);      // sourcils
    eyes(9, blinking(t, 0.9f), C_WHITE, C_DARK, skd);
    P(12, 10, 1, 3, skd);                                 // nez
    P(11, 14, 3, mouth_open(t, talk) ? 2 : 1, C_DARK);    // bouche
}

// ════════════════════════════════════════════════════════════════
// TABLE DES PERSONNAGES — source unique (nom → portrait, couleur, rôle)
// ════════════════════════════════════════════════════════════════
typedef struct {
    const char *key;        /* mot-clé cherché dans l'en-tête de réplique */
    const char *role;
    Color       accent;     /* plaque de nom, cadre                       */
    Color       bg_top;     /* fond du portrait (haut → bas)              */
    Color       bg_bot;
    void      (*draw)(float t, int talk);
    float       voice;      /* hauteur du déclic radio de ses répliques   */
} SpeakerDef;

static const SpeakerDef SPEAKERS[SPEAKER_COUNT] = {
    [SPEAKER_RADIO]  = { NULL,     "Transmission",
        {170, 160, 130, 255}, { 34,  32,  26, 255}, { 16,  15,  12, 255}, draw_radio,   1.00f },
    [SPEAKER_VOSS]   = { "VOSS",   "Commandant du bastion",
        {232, 168,  44, 255}, { 56,  40,  20, 255}, { 22,  15,   8, 255}, draw_voss,    0.80f },
    [SPEAKER_KIRA]   = { "KIRA",   "Sergent éclaireur",
        {150, 196,  92, 255}, { 36,  48,  26, 255}, { 14,  20,  10, 255}, draw_kira,    1.20f },
    [SPEAKER_VARELA] = { "VARELA", "Ingénieure en chef",
        { 96, 180, 220, 255}, { 24,  42,  58, 255}, { 10,  18,  26, 255}, draw_varela,  1.10f },
    [SPEAKER_STEEL]  = { "ACIER",  "Chef de la Faction Acier",
        {206, 210, 218, 255}, { 44,  40,  44, 255}, { 16,  14,  16, 255}, draw_steel,   0.70f },
    [SPEAKER_NEXUS]  = { "NEXUS",  "Intelligence centrale",
        {236,  64,  52, 255}, { 40,  10,  12, 255}, {  8,   2,   4, 255}, draw_nexus,   0.55f },
    [SPEAKER_GENERAL] = { "GÉNÉRAL", "IA d'artillerie du désert",
        {222, 208,  76, 255}, { 46,  44,  20, 255}, { 16,  15,   6, 255}, draw_general, 0.65f },
};

SpeakerId story_speaker_from_name(const char *name) {
    if (!name) return SPEAKER_RADIO;
    /* « CHEF CAPTIF » désigne le même homme que « CHEF DE LA FACTION ACIER ». */
    if (strstr(name, "CHEF")) return SPEAKER_STEEL;
    for (int i = 1; i < SPEAKER_COUNT; i++)
        if (SPEAKERS[i].key && strstr(name, SPEAKERS[i].key)) return (SpeakerId)i;
    return SPEAKER_RADIO;
}

static const SpeakerDef *speaker_def(SpeakerId id) {
    if (id < 0 || id >= SPEAKER_COUNT) id = SPEAKER_RADIO;
    return &SPEAKERS[id];
}

Color       story_speaker_color(SpeakerId id) { return speaker_def(id)->accent; }
const char *story_speaker_role (SpeakerId id) { return speaker_def(id)->role;   }
float       story_speaker_voice(SpeakerId id) { return speaker_def(id)->voice;  }

void story_portrait_draw(SpeakerId id, int x, int y, int px,
                         float t, int talk, float alpha) {
    const SpeakerDef *d = speaker_def(id);
    if (alpha <= 0.0f || px <= 0) return;
    g_ox = x; g_oy = y; g_px = px;
    g_al = alpha > 1.0f ? 1.0f : alpha;

    /* Fond : dégradé en 4 bandes (reste dans l'esthétique pixel). */
    const int N = STORY_PORTRAIT_CELLS, BANDS = 4;
    for (int b = 0; b < BANDS; b++) {
        float k = (float)b / (float)(BANDS - 1);
        Color c = {
            (unsigned char)((float)d->bg_top.r + ((float)d->bg_bot.r - (float)d->bg_top.r) * k),
            (unsigned char)((float)d->bg_top.g + ((float)d->bg_bot.g - (float)d->bg_top.g) * k),
            (unsigned char)((float)d->bg_top.b + ((float)d->bg_bot.b - (float)d->bg_top.b) * k),
            255 };
        P(0, b * (N / BANDS), N, N / BANDS, c);
    }
    d->draw(t, talk);
}

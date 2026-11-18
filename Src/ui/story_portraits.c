/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_portraits.c ─ Portraits des personnages du récit.
 *
 *  Chaque buste est PEINT une fois (story_portraits_paint.c) en
 *  SP_ART × SP_ART pixels d'art, en 4 variantes (neutre, clignement,
 *  parole, parole + clignement), puis mis en scène comme un portrait de
 *  jeu de rôle : fond dégradé tramé, halo de la couleur du personnage
 *  derrière la tête, vignettage, LISERÉ de contre-jour à sa couleur et
 *  contour sombre d'1 px. Le tout dans un atlas (filtre « nearest »),
 *  agrandi d'un facteur ENTIER : pixels nets à toute taille.
 *  Animations continues par-dessus (en px d'art) : visée rouge de l'Acier,
 *  œil et balayage de NEXUS, réticule du général, reflet des lunettes de
 *  Varela.
 */
#include "story_art.h"
#include "story_portraits_internal.h"
#include "terrain_internal.h"   /* tb_mix / tb_scale / tb_bayer (source unique) */
#include <math.h>
#include <string.h>

// ── Rythmes d'animation ──────────────────────────────────────────
#define PT_BLINK_PERIOD  4.3f   /* intervalle entre deux clignements (s)     */
#define PT_BLINK_DUR     0.12f  /* durée d'un clignement (s)                 */
#define PT_BLINK_PHASE   1.37f  /* décalage entre personnages (s)            */
#define PT_TALK_RATE     9.0f   /* battements de bouche par seconde          */
#define PT_VISOR_RATE    6.0f   /* scintillement de la visée de l'Acier      */
#define PT_EYE_IDLE      3.0f   /* pulsation de l'œil de NEXUS au repos      */
#define PT_EYE_TALK      11.0f  /* … quand il parle                          */
#define PT_SCAN_RATE     7.0f   /* balayage de NEXUS (px d'art/s)            */
#define PT_RETICLE_RATE  1.7f   /* va-et-vient du réticule du général        */
#define PT_GLINT_RATE    2.3f   /* reflet des lunettes de Varela             */

// ── Mise en scène ────────────────────────────────────────────────
#define PT_GLOW_X        21.0f  /* halo derrière la tête (px d'art)          */
#define PT_GLOW_Y        15.0f
#define PT_GLOW_R        21.0f
#define PT_GLOW_A        0.30f  /* intensité max du halo                     */
#define PT_RIM_A         0.55f  /* liseré coloré (dos du personnage)         */
#define PT_TOP_A         0.22f  /* reflet des arêtes hautes                  */
#define PT_OUTLINE       ((Color){10, 8, 8, 255})

static int   blinking(float t, float phase) { return fmodf(t + phase, PT_BLINK_PERIOD) < PT_BLINK_DUR; }
static int   mouth_open(float t, int talk)  { return talk && (((int)(t * PT_TALK_RATE)) & 1); }

// ════════════════════════════════════════════════════════════════
// TABLE DES PERSONNAGES — source unique (nom → portrait, couleur, rôle)
// ════════════════════════════════════════════════════════════════
typedef struct {
    const char *key;        /* mot-clé cherché dans l'en-tête de réplique */
    const char *role;
    Color       accent;     /* plaque de nom, cadre, liseré, halo         */
    Color       bg_top;     /* fond du portrait (haut → bas)              */
    Color       bg_bot;
    float       voice;      /* hauteur du déclic radio de ses répliques   */
} SpeakerDef;

static const SpeakerDef SPEAKERS[SPEAKER_COUNT] = {
    [SPEAKER_RADIO]  = { NULL,     "Transmission",
        {170, 160, 130, 255}, { 34,  32,  26, 255}, { 16,  15,  12, 255}, 1.00f },
    [SPEAKER_VOSS]   = { "VOSS",   "Commandant du bastion",
        {232, 168,  44, 255}, { 56,  40,  20, 255}, { 22,  15,   8, 255}, 0.80f },
    [SPEAKER_KIRA]   = { "KIRA",   "Sergent éclaireur",
        {150, 196,  92, 255}, { 36,  48,  26, 255}, { 14,  20,  10, 255}, 1.20f },
    [SPEAKER_VARELA] = { "VARELA", "Ingénieure en chef",
        { 96, 180, 220, 255}, { 24,  42,  58, 255}, { 10,  18,  26, 255}, 1.10f },
    [SPEAKER_STEEL]  = { "ACIER",  "Chef de la Faction Acier",
        {206, 210, 218, 255}, { 44,  40,  44, 255}, { 16,  14,  16, 255}, 0.70f },
    [SPEAKER_NEXUS]  = { "NEXUS",  "Intelligence centrale",
        {236,  64,  52, 255}, { 40,  10,  12, 255}, {  8,   2,   4, 255}, 0.55f },
    [SPEAKER_GENERAL] = { "GÉNÉRAL", "IA d'artillerie du désert",
        {222, 208,  76, 255}, { 46,  44,  20, 255}, { 16,  15,   6, 255}, 0.65f },
};

SpeakerId story_speaker_from_name(const char *name) {
    if (!name) return SPEAKER_RADIO;
    /* « CHEF CAPTIF » désigne le même homme que « CHEF DE LA FACTION ACIER ». */
    if (strstr(name, "CHEF")) return SPEAKER_STEEL;
    for (int i = 1; i < SPEAKER_COUNT; i++)
        if (SPEAKERS[i].key && strstr(name, SPEAKERS[i].key)) return (SpeakerId)i;
    return SPEAKER_RADIO;
}

static SpeakerId valid_id(SpeakerId id) {
    return ((int)id < 0 || (int)id >= SPEAKER_COUNT) ? SPEAKER_RADIO : id;
}
static const SpeakerDef *speaker_def(SpeakerId id) { return &SPEAKERS[valid_id(id)]; }

Color       story_speaker_color(SpeakerId id) { return speaker_def(id)->accent; }
const char *story_speaker_role (SpeakerId id) { return speaker_def(id)->role;   }
float       story_speaker_voice(SpeakerId id) { return speaker_def(id)->voice;  }

// ════════════════════════════════════════════════════════════════
// CUISSON DE L'ATLAS
// ════════════════════════════════════════════════════════════════
static Texture2D g_atlas;
static int       g_atlas_ok;

static Color *cell_px(Image *im, int ox, int oy, int x, int y) {
    return &((Color *)im->data)[(oy + y) * im->width + ox + x];
}

/* Fond : dégradé tramé, halo de la couleur du personnage, vignettage. */
static void paint_bg(Image *im, int ox, int oy, const SpeakerDef *d) {
    for (int y = 0; y < SP_ART; y++)
        for (int x = 0; x < SP_ART; x++) {
            float k  = (float)y / (SP_ART - 1) * 4.0f;            // 5 bandes tramées
            int   i  = (int)k + ((k - floorf(k)) > tb_bayer(x, y));
            Color c  = tb_mix(d->bg_top, d->bg_bot, (float)i / 4.0f);
            float dx = (x + 0.5f - PT_GLOW_X) / PT_GLOW_R, dy = (y + 0.5f - PT_GLOW_Y) / PT_GLOW_R;
            float g  = 1.0f - sqrtf(dx * dx + dy * dy);
            if (g > 0.0f) {                                       // halo en 3 paliers tramés
                float a = PT_GLOW_A * g * g * 3.0f;
                int   s = (int)a + ((a - floorf(a)) > tb_bayer(x + 1, y));
                c = tb_mix(c, d->accent, (float)s / 3.0f * PT_GLOW_A);
            }
            float vx = fabsf(x + 0.5f - SP_ART * 0.5f) / (SP_ART * 0.5f);
            float vy = fabsf(y + 0.5f - SP_ART * 0.5f) / (SP_ART * 0.5f);
            if (vx * vx + vy * vy > 1.15f + tb_bayer(x, y) * 0.3f) c = tb_scale(c, 0.72f);   // vignettage
            c.a = 255;
            *cell_px(im, ox, oy, x, y) = c;
        }
}

/* Liseré de contre-jour (couleur du personnage, côté dos/droite), reflet
   des arêtes hautes, puis contour sombre — sur le calque du personnage. */
static void rim_outline(Image *L, Color accent) {
    static unsigned char m[SP_ART][SP_ART];
    Color *p = (Color *)L->data;
    for (int y = 0; y < SP_ART; y++)
        for (int x = 0; x < SP_ART; x++) m[y][x] = p[y * SP_ART + x].a > 0;
    for (int y = 0; y < SP_ART; y++)
        for (int x = 0; x < SP_ART; x++) {
            if (!m[y][x]) continue;
            Color *c = &p[y * SP_ART + x];
            if (x < SP_ART - 1 && !m[y][x + 1])      *c = tb_mix(*c, accent, PT_RIM_A);
            else if (y > 0 && !m[y - 1][x])          *c = tb_mix(*c, (Color){255, 240, 210, 255}, PT_TOP_A);
            c->a = 255;
        }
    for (int y = 0; y < SP_ART; y++)
        for (int x = 0; x < SP_ART; x++) {
            if (m[y][x]) continue;
            if ((x > 0 && m[y][x - 1]) || (x < SP_ART - 1 && m[y][x + 1]) ||
                (y > 0 && m[y - 1][x]) || (y < SP_ART - 1 && m[y + 1][x]))
                p[y * SP_ART + x] = PT_OUTLINE;
        }
}

static void bake_atlas(void) {
    Image atlas = GenImageColor(SP_ART * SP_FRAMES, SP_ART * SPEAKER_COUNT, BLANK);
    Image layer = GenImageColor(SP_ART, SP_ART, BLANK);
    for (int id = 0; id < SPEAKER_COUNT; id++)
        for (int f = 0; f < SP_FRAMES; f++) {
            const SpeakerDef *d = &SPEAKERS[id];
            int ox = f * SP_ART, oy = id * SP_ART;
            memset(layer.data, 0, (size_t)SP_ART * SP_ART * sizeof(Color));
            sp_paint_figure(&layer, (SpeakerId)id, f);
            rim_outline(&layer, d->accent);
            paint_bg(&atlas, ox, oy, d);
            const Color *lp = (const Color *)layer.data;
            for (int y = 0; y < SP_ART; y++)
                for (int x = 0; x < SP_ART; x++)
                    if (lp[y * SP_ART + x].a) *cell_px(&atlas, ox, oy, x, y) = lp[y * SP_ART + x];
        }
    g_atlas = LoadTextureFromImage(atlas);
    SetTextureFilter(g_atlas, TEXTURE_FILTER_POINT);
    UnloadImage(layer);
    UnloadImage(atlas);
    g_atlas_ok = 1;
}

// ════════════════════════════════════════════════════════════════
// ANIMATIONS CONTINUES (en px d'art, par-dessus la variante)
// ════════════════════════════════════════════════════════════════
static int   g_ox, g_oy, g_k;
static float g_al;

static void ov(int ax, int ay, int aw, int ah, Color c) {
    c.a = (unsigned char)((float)c.a * g_al);
    DrawRectangle(g_ox + ax * g_k, g_oy + ay * g_k, aw * g_k, ah * g_k, c);
}
static void ov_disc(float cx, float cy, float r, Color c) {
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
        for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) ov(x, y, 1, 1, c);
        }
}

static void overlay(SpeakerId id, float t, int talk) {
    switch (id) {
    case SPEAKER_STEEL: {                       /* visée rouge qui palpite */
        float f = 0.72f + 0.28f * sinf(t * PT_VISOR_RATE);
        Color red = {(unsigned char)(226.0f * f), (unsigned char)(54.0f * f), (unsigned char)(40.0f * f), 255};
        ov(15, 14, 3, 1, red); ov(24, 13, 2, 1, red);
        ov(15, 13, 3, 1, (Color){255, 80, 60, (unsigned char)(70.0f * f)});
    } break;
    case SPEAKER_NEXUS: {                       /* l'œil respire, plus vite quand il parle */
        float p = 0.5f + 0.5f * sinf(t * (talk ? PT_EYE_TALK : PT_EYE_IDLE));
        ov_disc(20.0f, 20.0f, 3.2f, (Color){(unsigned char)(150.0f + 90.0f * p),
                                            (unsigned char)(24.0f + 30.0f * p),
                                            (unsigned char)(22.0f + 20.0f * p), 255});
        ov_disc(20.0f, 20.0f, 1.3f, p > 0.55f ? (Color){255, 214, 186, 255} : (Color){255, 120, 96, 255});
        int sy = 3 + (int)fmodf(t * PT_SCAN_RATE, 34.0f);
        ov(5, sy, 30, 1, (Color){255, 70, 56, 40});
    } break;
    case SPEAKER_GENERAL: {                     /* le réticule CHERCHE une cible */
        int sx = 14 + (int)(7.0f * (1.0f + sinf(t * PT_RETICLE_RATE)));
        ov(sx - 1, 15, 1, 2, (Color){140, 86, 26, 255});
        ov(sx + 1, 15, 1, 2, (Color){140, 86, 26, 255});
        ov(sx, 15, 1, 2, (Color){255, 178, 48, 255});
    } break;
    case SPEAKER_VARELA:                        /* lueur d'atelier sur les verres */
        if (sinf(t * PT_GLINT_RATE) > 0.6f) {
            ov(16, 10, 1, 1, (Color){255, 236, 170, 255});
            ov(23, 9, 1, 1, (Color){255, 236, 170, 255});
        }
        break;
    default: break;
    }
}

void story_portrait_draw(SpeakerId id, int x, int y, int px,
                         float t, int talk, float alpha) {
    id = valid_id(id);
    const SpeakerDef *d = speaker_def(id);
    if (alpha <= 0.0f || px <= 0) return;
    if (!g_atlas_ok) bake_atlas();
    g_al = alpha > 1.0f ? 1.0f : alpha;

    /* Facteur ENTIER (pixels nets) ; la marge éventuelle prend le fond. */
    int size = STORY_PORTRAIT_CELLS * px;
    g_k  = size / SP_ART > 0 ? size / SP_ART : 1;
    g_ox = x + (size - SP_ART * g_k) / 2;
    g_oy = y + (size - SP_ART * g_k) / 2;
    if (SP_ART * g_k < size) {
        Color m = d->bg_bot;
        m.a = (unsigned char)(255.0f * g_al);
        DrawRectangle(x, y, size, size, m);
    }
    int fr = (blinking(t, (float)id * PT_BLINK_PHASE) ? SP_BLINK : 0) | (mouth_open(t, talk) ? SP_TALK : 0);
    DrawTexturePro(g_atlas, (Rectangle){(float)(fr * SP_ART), (float)((int)id * SP_ART), SP_ART, SP_ART},
                   (Rectangle){(float)g_ox, (float)g_oy, (float)(SP_ART * g_k), (float)(SP_ART * g_k)},
                   (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, (unsigned char)(255.0f * g_al)});
    overlay(id, t, talk);
}

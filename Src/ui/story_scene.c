/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  story_scene.c ─ Moteur de mise en scène des dialogues (cf. .h).
 *
 *  Le texte est REPLIÉ SUR LA RÉPLIQUE ENTIÈRE avant d'être révélé : les
 *  mots ne sautent jamais d'une ligne à l'autre pendant le défilement.
 *  La révélation compte des octets mais ne coupe jamais un caractère
 *  UTF-8 (les accents font 2 octets).
 */
#include "story_scene.h"
#include "ui_utils.h"
#include "ui_anim.h"
#include "ui_palette.h"
#include "ui_plate.h"
#include "../engine/audio.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SLOT_STORY        6       /* slot ui_timer (0-5 : menus, fiche, carte) */
#define STORY_MAX_BEATS   16      /* répliques max par scène (prélude compris) */
#define STORY_NAME_MAX    40
#define STORY_TEXT_MAX    560
#define STORY_MAX_LINES   12      /* lignes repliées max d'une réplique        */
#define STORY_CPS         52.0f   /* défilement (caractères/s)                 */
#define STORY_FS_TEXT     11
#define STORY_FS_NAME     12
#define STORY_FS_ROLE     8
#define STORY_LINE_GAP    3       /* interligne (px) en plus de fh()           */
#define STORY_PAD         14      /* marge intérieure de la boîte (px)         */
#define STORY_PORTRAIT_IN 0.18f   /* entrée d'un nouveau portrait (s)          */
#define STORY_SLAT_PX     6       /* lames du volet d'entrée du portrait (px)  */

/* ── Cadre de la boîte : plaque de métal rivetée (ui_plate.h) ── */
#define SD_LINER_A        0.45f   /* liseré intérieur à la couleur du perso    */
#define SD_MOUNT          5       /* cadre du médaillon (px)                   */
#define SD_BRACKET        8       /* équerres d'angle du médaillon (px)        */
#define SD_PLATE_PAD      8       /* plaque de nom : marge gauche (px)         */
#define SD_PIP            6       /* pastilles de progression (px)             */
#define SD_PIP_GAP        4
#define SD_BOUNCE_HZ      1.1f    /* rebond de l'indicateur « suite » (Hz)     */
#define SD_BOUNCE_PX      3.0f
#define SD_CURSOR_HZ      2.5f    /* clignement du curseur de frappe (Hz)      */
#define SD_TEXT           ((Color){226, 212, 182, 255})
#define SD_PIP_OFF        ((Color){110,  92,  64, 200})

typedef struct {
    char      name[STORY_NAME_MAX];
    char      text[STORY_TEXT_MAX];
    SpeakerId who;
} Beat;

static struct {
    int   key;
    Beat  beat[STORY_MAX_BEATS];
    int   n, cur;
    int   reply;        /* index de la réplique de réaction (-1 = aucune) */
    float t;            /* horloge de la scène                            */
    float beat_t0;      /* début de la réplique courante                  */
    int   full;         /* réplique courante forcée en entier             */
    float swap_t0;      /* dernier changement de portrait                 */
    int   done_once;
    float open_t0;
    int   pick, pick_new;
} S = { .key = -1, .reply = -1, .pick = -1 };

// ════════════════════════════════════════════════════════════════
// LECTURE DU SCRIPT
// ════════════════════════════════════════════════════════════════
/* Un nom de personnage : pas de minuscule ASCII, ni ponctuation de
   phrase, ni chiffre. Les lettres accentuées (octets ≥ 0x80) sont
   admises : « GÉNÉRAL ». La ponctuation est le garde-fou décisif :
   NEXUS parle en MAJUSCULES, et « COMMANDANT VOSS. MODÈLE… : 96 % »
   passait pour une réplique de Voss. */
static int is_name(const char *s, int len) {
    if (len <= 0 || len >= STORY_NAME_MAX) return 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 0;
        if (c == '"' || c == '.' || c == ',' || c == '!' || c == '?' ||
            c == '%' || c == ';') return 0;
    }
    return 1;
}

static void trim(const char **b, const char **e) {
    while (*b < *e && (**b == ' ' || **b == '\t' || **b == '\r')) (*b)++;
    while (*e > *b && ((*e)[-1] == ' ' || (*e)[-1] == '\t' || (*e)[-1] == '\r')) (*e)--;
}

static Beat *beat_new(const char *name, int len) {
    if (S.n >= STORY_MAX_BEATS) return NULL;
    Beat *b = &S.beat[S.n++];
    if (len >= STORY_NAME_MAX) len = STORY_NAME_MAX - 1;
    memcpy(b->name, name, (size_t)len);
    b->name[len] = '\0';
    b->text[0]   = '\0';
    b->who       = story_speaker_from_name(b->name);
    return b;
}

/* Ajoute un morceau de texte (les guillemets de citation sont retirés :
   le portrait dit déjà qui parle). */
static void beat_append(Beat *b, const char *s, int len) {
    int k = (int)strlen(b->text);
    if (k > 0 && k < STORY_TEXT_MAX - 1) b->text[k++] = ' ';
    for (int i = 0; i < len && k < STORY_TEXT_MAX - 1; i++)
        if (s[i] != '"') b->text[k++] = s[i];
    b->text[k] = '\0';
}

static void parse(const char *script) {
    if (!script) return;
    Beat *open = NULL;
    const char *p = script;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        const char *b = p, *z = e;
        trim(&b, &z);
        int len = (int)(z - b);

        if (len == 0) {
            open = NULL;                                   /* fin de réplique */
        } else if (open && !open->text[0]) {
            beat_append(open, b, len);   /* 1re ligne après « NOM : » = TEXTE,
                                            même si elle a l'air d'un nom */
        } else if (b[len - 1] == ':' && is_name(b, len - 1)) {
            const char *nz = z - 1;                        /* « NOM : »       */
            trim(&b, &nz);
            open = beat_new(b, (int)(nz - b));
        } else {
            const char *sep = strstr(b, " : ");            /* « NOM : texte » */
            if (sep && sep < z && is_name(b, (int)(sep - b))) {
                open = beat_new(b, (int)(sep - b));
                if (open) beat_append(open, sep + 3, (int)(z - sep - 3));
            } else {
                if (!open) open = beat_new("", 0);         /* voix anonyme    */
                if (open) beat_append(open, b, len);
            }
        }
        p = *e ? e + 1 : e;
    }
}

// ════════════════════════════════════════════════════════════════
// ÉTAT
// ════════════════════════════════════════════════════════════════
static int cur_len(void) {
    return (S.n > 0) ? (int)strlen(S.beat[S.cur].text) : 0;
}
static int revealed(void) {
    if (S.full) return 1 << 20;
    float k = (S.t - S.beat_t0) * STORY_CPS;
    return k < 0.0f ? 0 : (int)k;
}
static int cur_complete(void) { return revealed() >= cur_len(); }

static void go_to(int idx) {
    if (idx < 0 || idx >= S.n) return;
    if (S.n > 0 && S.beat[idx].who != S.beat[S.cur].who) S.swap_t0 = S.t;
    S.cur     = idx;
    S.beat_t0 = S.t;
    S.full    = 0;
}

void story_scene_sync(int key, const char *prelude, const char *script) {
    /* Pas rendue à la frame précédente → c'est une NOUVELLE visite. */
    int gap = ui_timer_gap(SLOT_STORY) > 2;
    ui_timer(SLOT_STORY, 0);
    if (key != S.key || gap) {
        memset(&S, 0, sizeof(S));
        S.key   = key;
        S.reply = -1;
        S.pick  = -1;
        parse(prelude);
        parse(script);
        for (int i = 0; i < S.n; ) {                       /* répliques vides */
            if (S.beat[i].text[0]) { i++; continue; }
            memmove(&S.beat[i], &S.beat[i + 1], sizeof(Beat) * (size_t)(S.n - i - 1));
            S.n--;
        }
        S.swap_t0 = -1.0f;   /* 1er portrait : entrée animée */
    }
    S.t += ui_dt();
    if (!S.done_once && story_scene_done()) {
        S.done_once = 1;
        S.open_t0   = S.t;
    }
}

int   story_scene_done(void)      { return S.n == 0 || (S.cur >= S.n - 1 && cur_complete()); }
int   story_scene_done_once(void) { return S.done_once; }
float story_scene_since_open(void){ return S.done_once ? S.t - S.open_t0 : 0.0f; }
float story_scene_time(void)      { return S.t; }

void story_scene_advance(void) {
    if (S.n == 0) return;
    if (!cur_complete()) { S.full = 1; return; }
    if (S.cur < S.n - 1) {
        go_to(S.cur + 1);
        /* Déclic radio à la hauteur de la VOIX du personnage qui prend la
           parole : NEXUS grave, Kira aiguë… on reconnaît qui parle à l'oreille. */
        audio_play_sfx_pitch(AUDIO_SFX_MENU_CLICK, story_speaker_voice(S.beat[S.cur].who));
    }
}

void story_scene_skip_all(void) {
    if (S.n == 0) return;
    if (S.cur != S.n - 1) go_to(S.n - 1);
    S.full = 1;
}

void story_scene_say(const char *name, const char *text) {
    if (!name || !text) {                                  /* retire la réaction */
        if (S.reply >= 0 && S.reply == S.n - 1) {
            S.n--;
            S.reply = -1;
            if (S.cur >= S.n) { S.cur = S.n - 1; S.full = 1; }
        }
        return;
    }
    if (S.reply >= 0 && S.reply == S.n - 1) S.n--;         /* remplace */
    Beat *b = beat_new(name, (int)strlen(name));
    if (!b) return;
    beat_append(b, text, (int)strlen(text));
    S.reply = S.n - 1;
    go_to(S.reply);
}

int  story_scene_pick(void)         { return S.pick; }
void story_scene_set_pick(int idx)  { S.pick = idx; S.pick_new = 1; }
int  story_scene_take_pick(void) {
    if (!S.pick_new) return -1;
    S.pick_new = 0;
    return S.pick;
}

// ════════════════════════════════════════════════════════════════
// RENDU
// ════════════════════════════════════════════════════════════════
typedef struct { int start, len; } Span;

/* Repli au mot près sur maxw ; un mot plus long que la ligne reste seul. */
static int wrap_text(const char *t, int fs, int maxw, Span *out, int max) {
    int  L = (int)strlen(t), n = 0, i = 0;
    char buf[STORY_TEXT_MAX];
    while (i < L && n < max) {
        while (i < L && t[i] == ' ') i++;
        if (i >= L) break;
        int end = i, k = i;
        while (k < L) {
            int w = k;
            while (w < L && t[w] != ' ') w++;
            int len = w - i;
            memcpy(buf, t + i, (size_t)len);
            buf[len] = '\0';
            int too_wide = mtxt(buf, fs) > maxw;
            if (too_wide && end > i) break;                 /* déborde : coupe */
            end = w;
            if (too_wide) break;                            /* mot trop long   */
            k = w;
            while (k < L && t[k] == ' ') k++;
        }
        out[n].start = i;
        out[n].len   = end - i;
        n++;
        i = end;
    }
    return n;
}

static Color with_a(Color c, float a) { return plate_fade(c, a); }

/* Médaillon : cadre biseauté, feuillure sombre, équerres d'angle colorées. */
static void draw_mount(int x, int y, int s, Color acc) {
    int m = SD_MOUNT, w = s + 2 * m;
    DrawRectangle(x - m, y - m, w, w, PLATE_FRAME_D);
    DrawRectangle(x - m + 1, y - m + 1, w - 2, w - 2, PLATE_FRAME);
    DrawRectangle(x - m + 1, y - m + 1, w - 2, 1, PLATE_FRAME_L);
    DrawRectangle(x - m + 1, y - m + 1, 1, w - 2, PLATE_FRAME_L);
    DrawRectangle(x - 2, y - 2, s + 4, s + 4, PLATE_WELL);
    int bx0 = x - m - 1, by0 = y - m - 1, bx1 = x + s + m + 1, by1 = y + s + m + 1, L = SD_BRACKET;
    Color a = with_a(acc, 0.9f);
    DrawRectangle(bx0, by0, L, 2, a);         DrawRectangle(bx0, by0, 2, L, a);
    DrawRectangle(bx1 - L, by0, L, 2, a);     DrawRectangle(bx1 - 2, by0, 2, L, a);
    DrawRectangle(bx0, by1 - 2, L, 2, a);     DrawRectangle(bx0, by1 - L, 2, L, a);
    DrawRectangle(bx1 - L, by1 - 2, L, 2, a); DrawRectangle(bx1 - 2, by1 - L, 2, L, a);
}

void story_scene_draw_dialogue(Rectangle box) {
    if (S.n == 0) return;
    const Beat *b   = &S.beat[S.cur];
    Color       acc = story_speaker_color(b->who);
    int         right = (int)(box.x + box.width) - STORY_PAD;
    plate_panel(box, PLATE_CHAMFER, acc, SD_LINER_A);

    /* ── Portrait dans son médaillon ── */
    int cell = (int)((box.height - 2.0f * STORY_PAD) / (float)STORY_PORTRAIT_CELLS);
    if (cell < 2) cell = 2;
    int   psz = cell * STORY_PORTRAIT_CELLS;
    int   ppx = (int)box.x + STORY_PAD;
    int   ppy = (int)(box.y + (box.height - (float)psz) * 0.5f);
    float kin = (S.t - S.swap_t0) / STORY_PORTRAIT_IN;
    if (kin > 1.0f) kin = 1.0f;
    draw_mount(ppx, ppy, psz, acc);
    story_portrait_draw(b->who, ppx, ppy, cell, S.t, !cur_complete(), 1.0f);
    /* Nouveau personnage : un volet de métal à lames se relève sur lui. */
    int shut = (int)((1.0f - ea_out_cubic(kin)) * (float)psz);
    if (shut > 0) {
        DrawRectangle(ppx, ppy, psz, shut, PLATE_FRAME);
        for (int y = ppy + STORY_SLAT_PX - 1; y < ppy + shut - 2; y += STORY_SLAT_PX)
            DrawRectangle(ppx, y, psz, 1, PLATE_FRAME_D);
        DrawRectangle(ppx, ppy + shut - 2, psz, 2, PLATE_FRAME_L);
    }
    DrawRectangleLines(ppx - 1, ppy - 1, psz + 2, psz + 2, with_a(acc, 0.6f));

    /* ── Plaque de nom (repère coloré) + rôle, filet dégradé dessous ── */
    int tx = ppx + psz + 18;
    int ty = (int)box.y + STORY_PAD - 1;
    const char *nm   = b->name[0] ? b->name : "TRANSMISSION";
    const char *role = story_speaker_role(b->who);
    int nw = mtxt(nm, STORY_FS_NAME), plate_h = fh(STORY_FS_NAME) + 4;
    int plate_w = SD_PLATE_PAD + nw + 10 + mtxt(role, STORY_FS_ROLE) + SD_PLATE_PAD;
    DrawRectangle(tx - SD_PLATE_PAD, ty - 2, plate_w, plate_h,
                  (Color){(unsigned char)(acc.r * 0.20f), (unsigned char)(acc.g * 0.20f),
                          (unsigned char)(acc.b * 0.20f), (unsigned char)(220.0f * kin)});
    DrawRectangle(tx - SD_PLATE_PAD, ty - 2, 3, plate_h, with_a(acc, kin));
    dtxt(nm, tx, ty, STORY_FS_NAME, with_a(acc, kin));
    dtxt(role, tx + nw + 10, ty + fh(STORY_FS_NAME) - fh(STORY_FS_ROLE) - 2, STORY_FS_ROLE,
         with_a(UI_TEXT_DIM, kin));
    DrawRectangleGradientH(tx - SD_PLATE_PAD, ty - 2 + plate_h + 3, right - tx + SD_PLATE_PAD, 1,
                           with_a(acc, 0.55f * kin), (Color){acc.r, acc.g, acc.b, 0});

    /* Progression : une pastille par réplique (passées, courante, à venir). */
    int pips_w = S.n * SD_PIP + (S.n - 1) * SD_PIP_GAP;
    int pxx = right - pips_w, pyy = ty + (plate_h - SD_PIP) / 2 - 2;
    for (int i = 0; i < S.n; i++, pxx += SD_PIP + SD_PIP_GAP) {
        if (i < S.cur)       DrawRectangle(pxx, pyy, SD_PIP, SD_PIP, with_a(acc, 0.45f));
        else if (i == S.cur) DrawRectangle(pxx, pyy, SD_PIP, SD_PIP, acc);
        else                 DrawRectangleLines(pxx, pyy, SD_PIP, SD_PIP, SD_PIP_OFF);
    }
    ty += plate_h + 8;

    /* ── Texte qui défile (+ curseur de frappe tant qu'il défile) ── */
    Span ln[STORY_MAX_LINES];
    int  tw   = right - tx;
    int  nl   = wrap_text(b->text, STORY_FS_TEXT, tw, ln, STORY_MAX_LINES);
    int  rev  = revealed();
    int  lh   = fh(STORY_FS_TEXT) + STORY_LINE_GAP;
    int  ybot = (int)(box.y + box.height) - STORY_PAD;
    int  cx = tx, cy = ty;
    char buf[STORY_TEXT_MAX];
    for (int i = 0; i < nl && ty + fh(STORY_FS_TEXT) <= ybot; i++, ty += lh) {
        int vis = rev - ln[i].start;
        if (vis <= 0) break;
        if (vis > ln[i].len) vis = ln[i].len;
        /* ne jamais couper un caractère UTF-8 (octets 10xxxxxx) */
        while (vis > 0 && vis < ln[i].len &&
               ((unsigned char)b->text[ln[i].start + vis] & 0xC0) == 0x80) vis--;
        memcpy(buf, b->text + ln[i].start, (size_t)vis);
        buf[vis] = '\0';
        dtxt(buf, tx, ty, STORY_FS_TEXT, SD_TEXT);
        cx = tx + mtxt(buf, STORY_FS_TEXT) + 2;
        cy = ty;
    }
    if (!cur_complete() && fmodf(S.t * SD_CURSOR_HZ, 1.0f) < 0.6f)
        DrawRectangle(cx, cy + 3, 5, fh(STORY_FS_TEXT) - 5, with_a(acc, 0.85f));

    /* ── « Suite » : chevron qui rebondit quand la réplique est entière ── */
    if (cur_complete() && S.cur < S.n - 1) {
        float ax = (float)right - 7.0f;
        float ay = box.y + box.height - STORY_PAD - 6.0f
                 + SD_BOUNCE_PX * fabsf(sinf(S.t * SD_BOUNCE_HZ * PI));
        plate_tri((Vector2){ax - 6.0f, ay - 5.0f}, (Vector2){ax, ay + 3.0f}, (Vector2){ax + 6.0f, ay - 5.0f},
                  with_a(acc, 0.9f));
        plate_tri((Vector2){ax - 3.0f, ay - 5.0f}, (Vector2){ax, ay - 1.0f}, (Vector2){ax + 3.0f, ay - 5.0f},
                  PLATE_PANEL);
    }
}

void story_scene_draw_vignette(Rectangle r, ThemeID theme, int node_id) {
    plate_draw(plate_inset(r, -PLATE_BORDER), PLATE_CHAMFER);   /* même plaque que la boîte */
    story_vignette_draw(theme, node_id, (int)r.x, (int)r.y,
                        (int)r.width, (int)r.height, STORY_VIGNETTE_PX, S.t);
    DrawRectangleLines((int)r.x - 1, (int)r.y - 1, (int)r.width + 2, (int)r.height + 2, PLATE_WELL);
}

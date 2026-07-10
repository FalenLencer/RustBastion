/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  ui_anim.c ─ Infrastructure d'animation de l'UI. Voir ui_anim.h.
 *
 *  Robustesse au double tick : pendant le menu pause, ui_render PUIS
 *  menu_render_and_act tournent dans la même frame → ui_anim_tick est
 *  appelée deux fois. Conception pour que ce soit inoffensif :
 *    • g_ui_dt est une AFFECTATION (GetFrameTime est constant sur la
 *      frame) — pas d'accumulation, donc idempotent.
 *    • Les timers n'avancent pas dans le tick : chaque slot avance à
 *      la LECTURE (ui_timer), au plus une fois par tick (numéro de
 *      série). Un slot lu côté HUD et un slot lu côté menu avancent
 *      donc chacun d'exactement un dt par frame.
 *    • Le cache hover n'avance que dans ui_hover_t (une fois par
 *      widget dessiné) ; le tick ne fait que marquer les entrées
 *      comme recyclables.
 */

#include "ui_anim.h"
#include "raylib.h"
#include <math.h>

// ── Réglages (pas de nombres magiques épars) ─────────────────────
#define UIA_DT_MAX        0.05f       // plafond du dt (gros hoquet de frame)
#define UIA_HOVER_IN      0.12f       // durée de montée du survol (s)
#define UIA_HOVER_OUT     0.18f       // durée de descente du survol (s)
#define UIA_HOVER_SLOTS   64          // entrées du cache hover
#define UIA_HOVER_GOLD    0x9E3779B9u // brouilleur de clé (nombre d'or 32 bits)
#define UIA_SWEEP_DUR     0.30f       // durée du reflet balayant (s)
#define UIA_PANEL_DUR     0.30f       // durée d'ouverture des panneaux (s)
#define UIA_BACK_OVERSHOOT 1.70158f   // overshoot standard de ease-out-back
#define UIA_ELASTIC_PERIOD 2.0943951f // (2π)/3 : période de ease-out-elastic

// ── Horloge ──────────────────────────────────────────────────────
static float    g_ui_dt      = 0.0f;  // dt borné de la frame courante
static unsigned g_tick_serial = 0u;   // n° de série du tick (cf. ui_timer)

// ── Ouverture des panneaux ───────────────────────────────────────
// Démarre à 0 : les panneaux se « soudent » aussi au lancement du jeu
// (cohérent avec le fondu d'entrée de la transition d'écran).
static float g_panel_t = 0.0f;

// ── Cache hover ──────────────────────────────────────────────────
typedef struct {
    unsigned key;          // identité du widget (0 = entrée libre)
    float    t;            // progression brute du survol, 0..1
    int      seen;         // vue depuis le dernier tick → non recyclable
    float    sweep;        // progression du reflet balayant, 0..1 (1 = fini)
    int      was_hovered;  // survol de la frame précédente (front montant)
    unsigned sweep_serial; // dernier tick où le balayage a avancé
} HoverEntry;
static HoverEntry g_hover[UIA_HOVER_SLOTS];

// ── Timers génériques ────────────────────────────────────────────
typedef struct {
    float    t;           // temps écoulé depuis le reset (s)
    unsigned last_serial; // dernier tick où le slot a avancé
} TimerSlot;
static TimerSlot g_timers[UIA_TIMER_SLOTS];

// ═════════════════════════════════════════════════════════════════
// EASINGS — entrée clampée sur [0,1]
// ═════════════════════════════════════════════════════════════════
static float uia_clamp01(float t) {
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

float ea_out_cubic(float t) {
    t = uia_clamp01(t);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float ea_in_cubic(float t) {
    t = uia_clamp01(t);
    return t * t * t;
}

// Dépasse 1 avant de se poser (rebond léger) — overshoot voulu.
float ea_out_back(float t) {
    t = uia_clamp01(t);
    float c1 = UIA_BACK_OVERSHOOT;
    float c3 = c1 + 1.0f;
    float u  = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

// Oscille autour de 1 en s'amortissant — overshoot voulu.
float ea_out_elastic(float t) {
    t = uia_clamp01(t);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    // 2^(-10t) × sinusoïde : rebonds amortis vers 1 (2^x = e^(x·ln2)).
    float decay = expf(-10.0f * t * 0.6931472f);   // ln(2)
    return decay * sinf((t * 10.0f - 0.75f) * UIA_ELASTIC_PERIOD) + 1.0f;
}

// ═════════════════════════════════════════════════════════════════
// HORLOGE
// ═════════════════════════════════════════════════════════════════
void ui_anim_tick(void) {
    float dt = GetFrameTime();
    if (dt > UIA_DT_MAX) dt = UIA_DT_MAX;
    g_ui_dt = dt;            // affectation : idempotent si double tick
    g_tick_serial++;

    // Ouverture des panneaux : monte vers UIA_PANEL_DUR puis reste plein.
    // (Le double tick en pause l'accélérerait, mais aucun reset n'a lieu
    // en pause — le progress y est déjà à 1 : sans effet visible.)
    if (g_panel_t < UIA_PANEL_DUR) {
        g_panel_t += dt;
        if (g_panel_t > UIA_PANEL_DUR) g_panel_t = UIA_PANEL_DUR;
    }

    // Marque toutes les entrées hover comme recyclables ; celles encore
    // affichées seront re-marquées par ui_hover_t dans la même frame.
    for (int i = 0; i < UIA_HOVER_SLOTS; i++)
        g_hover[i].seen = 0;
}

float ui_dt(void) {
    return g_ui_dt;
}

// ═════════════════════════════════════════════════════════════════
// CACHE HOVER
// ═════════════════════════════════════════════════════════════════
float ui_hover_t(int x, int y, int hovered) {
    unsigned key = ((unsigned)x << 16) ^ (unsigned)y ^ UIA_HOVER_GOLD;
    if (key == 0u) key = 1u;   // 0 est réservé « entrée libre »

    // Recherche de l'entrée existante.
    HoverEntry *e = 0;
    for (int i = 0; i < UIA_HOVER_SLOTS; i++) {
        if (g_hover[i].key == key) { e = &g_hover[i]; break; }
    }

    // Widget au repos et sans état mémorisé : rien à animer, pas
    // d'allocation (garde la table réservée aux widgets actifs).
    if (!e && !hovered) return 0.0f;

    // Allocation : entrée libre d'abord, sinon recyclage d'une entrée
    // non vue depuis le dernier tick (écran quitté, widget disparu).
    if (!e) {
        for (int i = 0; i < UIA_HOVER_SLOTS; i++) {
            if (g_hover[i].key == 0u) { e = &g_hover[i]; break; }
        }
        if (!e) {
            for (int i = 0; i < UIA_HOVER_SLOTS; i++) {
                if (!g_hover[i].seen) { e = &g_hover[i]; break; }
            }
        }
        if (!e) return hovered ? 1.0f : 0.0f;   // table saturée : repli sans état
        e->key          = key;
        e->t            = 0.0f;
        e->sweep        = 1.0f;   // pas de balayage tant que pas de front montant
        e->was_hovered  = 0;
        e->sweep_serial = g_tick_serial;
    }

    // Front montant du survol → relance le reflet balayant.
    if (hovered && !e->was_hovered) e->sweep = 0.0f;
    e->was_hovered = hovered;

    // Avance du survol (montée rapide, descente plus douce).
    if (hovered) e->t += g_ui_dt / UIA_HOVER_IN;
    else         e->t -= g_ui_dt / UIA_HOVER_OUT;
    e->t = uia_clamp01(e->t);
    e->seen = 1;

    // Entrée retombée au repos : libérée immédiatement.
    if (!hovered && e->t <= 0.0f) {
        e->key = 0u;
        return 0.0f;
    }
    return ea_out_cubic(e->t);
}

float ui_hover_sweep(int x, int y, int hovered) {
    (void)hovered;   // l'état est déjà tenu par ui_hover_t (front montant)
    unsigned key = ((unsigned)x << 16) ^ (unsigned)y ^ UIA_HOVER_GOLD;
    if (key == 0u) key = 1u;

    for (int i = 0; i < UIA_HOVER_SLOTS; i++) {
        HoverEntry *e = &g_hover[i];
        if (e->key != key) continue;
        // Avance au plus une fois par tick (plusieurs lectures possibles).
        if (e->sweep < 1.0f && e->sweep_serial != g_tick_serial) {
            e->sweep += g_ui_dt / UIA_SWEEP_DUR;
            if (e->sweep > 1.0f) e->sweep = 1.0f;
            e->sweep_serial = g_tick_serial;
        }
        return e->sweep;
    }
    return 1.0f;   // aucun état : balayage considéré terminé
}

// ═════════════════════════════════════════════════════════════════
// TIMERS GÉNÉRIQUES
// ═════════════════════════════════════════════════════════════════
float ui_timer(int slot, int reset) {
    if (slot < 0 || slot >= UIA_TIMER_SLOTS) return 0.0f;
    TimerSlot *s = &g_timers[slot];
    if (reset) {
        s->t           = 0.0f;
        s->last_serial = g_tick_serial;
        return 0.0f;
    }
    // Avance à la lecture, au plus une fois par tick : plusieurs
    // lectures du même slot dans la même frame ne le font pas courir.
    if (s->last_serial != g_tick_serial) {
        s->t += g_ui_dt;
        s->last_serial = g_tick_serial;
    }
    return s->t;
}

unsigned ui_timer_gap(int slot) {
    if (slot < 0 || slot >= UIA_TIMER_SLOTS) return 0u;
    return g_tick_serial - g_timers[slot].last_serial;
}

// ═════════════════════════════════════════════════════════════════
// OUVERTURE DES PANNEAUX
// ═════════════════════════════════════════════════════════════════
void ui_panel_reset(void) {
    g_panel_t = 0.0f;
}

float ui_panel_k(void) {
    return g_panel_t / UIA_PANEL_DUR;   // g_panel_t est clampé au tick
}

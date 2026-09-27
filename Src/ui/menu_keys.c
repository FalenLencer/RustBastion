/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  menu_keys.c ─ Noms lisibles des touches (écran Options > Commandes,
 *  invites du mode Héros). Extrait de menu_options.c (limite 500 lignes).
 */
#include "menu.h"
#include "raylib.h"
#include <stdio.h>

/* Nom de touche SELON LA DISPOSITION CLAVIER (GLFW, embarqué dans raylib ;
   les codes KEY_* raylib sont les codes GLFW). raylib ne l'expose pas. */
extern const char *glfwGetKeyName(int key, int scancode);

// ── Nom lisible d'un code de touche raylib (buffer appelant) ─────
void opts_key_name(int key, char *out, int outsz) {
    /* Les codes sont des POSITIONS physiques (US) : KEY_W est la touche Z
       d'un clavier AZERTY. Afficher « W » à un joueur AZERTY était faux →
       on demande le caractère réellement imprimé sur la touche. */
    if ((key >= KEY_A && key <= KEY_Z) || key == KEY_SEMICOLON ||
        key == KEY_COMMA || key == KEY_PERIOD) {
        const char *ln = glfwGetKeyName(key, 0);
        if (ln && ln[0]) {
            snprintf(out, outsz, "%s", ln);
            for (char *c = out; *c; c++)
                if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
            return;
        }
    }
    if (key >= KEY_A && key <= KEY_Z) {
        snprintf(out, outsz, "%c", 'A' + (key - KEY_A));
        return;
    }
    if (key >= KEY_ZERO && key <= KEY_NINE) {
        snprintf(out, outsz, "%c", '0' + (key - KEY_ZERO));
        return;
    }
    if (key >= KEY_F1 && key <= KEY_F12) {
        snprintf(out, outsz, "F%d", key - KEY_F1 + 1);
        return;
    }
    if (key >= KEY_KP_0 && key <= KEY_KP_9) {
        snprintf(out, outsz, "PAVE %d", key - KEY_KP_0);
        return;
    }
    const char *n = NULL;
    switch (key) {
        case KEY_SPACE:         n = "ESPACE";    break;
        case KEY_LEFT_SHIFT:    n = "SHIFT G";   break;
        case KEY_RIGHT_SHIFT:   n = "SHIFT D";   break;
        case KEY_LEFT_CONTROL:  n = "CTRL G";    break;
        case KEY_RIGHT_CONTROL: n = "CTRL D";    break;
        case KEY_LEFT_ALT:      n = "ALT";       break;
        case KEY_TAB:           n = "TAB";       break;
        case KEY_ENTER:         n = "ENTREE";    break;
        case KEY_BACKSPACE:     n = "RETOUR AR"; break;
        case KEY_UP:            n = "FL. HAUT";  break;
        case KEY_DOWN:          n = "FL. BAS";   break;
        case KEY_LEFT:          n = "FL. GAUCHE";break;
        case KEY_RIGHT:         n = "FL. DROITE";break;
        case KEY_SEMICOLON:     n = "; (M azerty)"; break;
        case KEY_COMMA:         n = ",";         break;
        case KEY_PERIOD:        n = ".";         break;
        default: break;
    }
    if (n) snprintf(out, outsz, "%s", n);
    else   snprintf(out, outsz, "CODE %d", key);
}

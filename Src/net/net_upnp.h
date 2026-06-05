/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_upnp.h ─ Ouverture AUTOMATIQUE du port (sans config manuelle).
 *
 *  MODULE ISOLÉ ET RETIRABLE : aucune autre partie du jeu n'en dépend
 *  obligatoirement. Pour le supprimer entièrement :
 *    1. retirer `Src/net/net_upnp.c` de la variable SRC du Makefile ;
 *    2. supprimer les 2 fichiers `net_upnp.{h,c}` ;
 *    3. retirer le petit hook marqué « UPNP_HOOK » dans app.c / menu_mp.c.
 *
 *  But : demander à la box (routeur) d'ouvrir le port TCP du jeu et de
 *  révéler l'IP publique, afin de jouer à distance SANS port-forwarding
 *  manuel et SANS serveur tiers. Deux protocoles tentés, dans l'ordre :
 *    • NAT-PMP / PCP (simple, paquets UDP à la passerelle) ;
 *    • UPnP-IGD (SSDP + SOAP, largement supporté par les box grand public).
 *
 *  LIMITES (assumées) : inopérant si l'UPnP/NAT-PMP est désactivé sur la
 *  box, ou en CGNAT (l'opérateur partage l'IP publique → la box n'a pas
 *  d'IP publique à mapper). Dans ces cas la fonction échoue PROPREMENT
 *  (message explicite) sans bloquer l'interface.
 */
#pragma once
#include <stdint.h>

typedef struct {
    int      mapped;          /* 1 = redirection TCP établie sur la box      */
    uint32_t external_ip_be;  /* IP publique (ordre réseau) ; 0 si inconnue  */
    int      external_public; /* 1 = l'IP externe est routable (pas CGNAT)   */
    char     method[12];      /* "UPnP" | "NAT-PMP" | ""                     */
    char     message[160];    /* texte lisible (succès / cause d'échec)      */
} UpnpResult;

/* Tente d'ouvrir le port TCP `port` sur la box puis de découvrir l'IP
 * publique. Bloquant mais BORNÉ (~6 s max en cas d'échec total ; bien plus
 * court en cas de succès). Retourne 1 si une redirection a été établie,
 * 0 sinon (détails dans out->message). `out` peut être NULL. */
int  net_upnp_map_tcp(uint16_t port, UpnpResult *out);

/* Retire la redirection créée par le dernier net_upnp_map_tcp réussi
 * (best effort, borné). Sans effet si rien n'a été mappé. */
void net_upnp_unmap_tcp(uint16_t port);

/* Demande l'autorisation pare-feu pour le jeu sur `port`.
 * Sous Windows : élève via UAC (invite « autoriser les modifications ? ») et
 * ajoute une règle ENTRANTE « autoriser » pour l'exécutable + le port TCP
 * (couvre aussi les réponses UPnP). Retourne 1 si accordé/ajouté, 0 si refusé.
 * No-op (retourne 1) hors Windows. À appeler AVANT net_upnp_map_tcp. */
int  net_upnp_request_firewall(uint16_t port);

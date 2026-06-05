/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_relay.h ─ Serveur RELAIS minimal (rendez-vous P2P).
 *
 *  MODULE ISOLÉ. Tourne sur un PC toujours allumé (ex. le même que ton
 *  serveur Minecraft), sur un port déjà redirigé par la box → aucune
 *  nouvelle règle. Les DEUX joueurs s'y connectent EN SORTIE (donc ça
 *  marche même derrière CGNAT/NAT côté joueurs) ; le relais apparie les
 *  deux par un CODE DE SALON puis recopie les octets de l'un vers l'autre.
 *
 *  Protocole d'appairage (préambule, 6 octets) envoyé par chaque client :
 *    [ 'R' ][ 'B' ][ room : 4 octets gros-boutiste ]
 *  Après appairage, le relais ne comprend RIEN au reste : il pipe le flux
 *  (messages encadrés du jeu) tel quel.
 *
 *  Lancement headless :  rustbastion --relay [port]   (défaut 47777)
 */
#pragma once

/* Boucle bloquante du relais sur `port` (TCP). Ne retourne qu'en cas
 * d'échec d'initialisation (socket/bind/listen). 0 = jamais (tourne),
 * != 0 = échec d'init. */
int net_relay_run(int port);

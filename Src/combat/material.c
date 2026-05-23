/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#include "material.h"

const char *MATERIAL_NAMES[MAT_COUNT] = {
    [MAT_IRON]  = "Fer",
    [MAT_ACID]  = "Acide",
    [MAT_PLASMA]= "Plasma",
    [MAT_CRYO]  = "Cryo",
    [MAT_NANO]  = "Nano",
};

const char *MATERIAL_DESC[MAT_COUNT] = {
    [MAT_IRON]  = "+10% degats physiques",
    [MAT_ACID]  = "Poison : degats sur la duree",
    [MAT_PLASMA]= "Electrique : chain 2 cibles",
    [MAT_CRYO]  = "Ralentit a -70% au lieu de -50%",
    [MAT_NANO]  = "Regenere les unites proches",
};

const char *MATERIAL_LORE[MAT_COUNT] = {
    [MAT_IRON]  =
        "Eclats de fer recuperes dans les ruines industrielles.\n"
        "Renforce de 10% les degats physiques de toutes les tours.\n"
        "Simple et fiable — constitue la base de tout armement.",

    [MAT_ACID]  =
        "Acide corrosif concentre, vestige de l'ere chimique.\n"
        "Impregneles projectiles d'un poison qui ronge les cibles\n"
        "sur la duree, meme apres qu'elles ont quitte la zone de tir.",

    [MAT_PLASMA]=
        "Plasma electrise instable recolte dans les generateurs detruits.\n"
        "A l'impact, un arc electrique rebondit sur 2 cibles proches.\n"
        "Devastateur dans les groupes serres.",

    [MAT_CRYO]  =
        "Agent cryogenique haute concentration issu des silos frigorifiques.\n"
        "Amplifie le ralentissement des tours Cryo : -70% au lieu de -50%.\n"
        "Les ennemis touches deviennent quasi immobiles.",

    [MAT_NANO]  =
        "Essaim nanobotique autoreplicant recupere dans les labos abandonnes.\n"
        "Repare passivement les unites alliees dans un rayon de 3 cases.\n"
        "Indispensable pour maintenir vos ouvriers en vie.",
};

const char *DAMAGE_NAMES[6] = {
    "Physique", "Poison", "Electrique", "Cryo", "Nano", "Feu",
};
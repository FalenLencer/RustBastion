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
    [MAT_IRON]  = "+10% dégâts physiques",
    [MAT_ACID]  = "Poison : dégâts sur la durée",
    [MAT_PLASMA]= "Électrique : chain 2 cibles",
    [MAT_CRYO]  = "Ralentit à -70% au lieu de -50%",
    [MAT_NANO]  = "Régénère les unités proches",
};

const char *MATERIAL_LORE[MAT_COUNT] = {
    [MAT_IRON]  =
        "Éclats de fer récupérés dans les ruines industrielles.\n"
        "Renforce de 10% les dégâts physiques de toutes les tours.\n"
        "Simple et fiable — constitue la base de tout armement.",

    [MAT_ACID]  =
        "Acide corrosif concentré, vestige de l'ère chimique.\n"
        "Imprègne les projectiles d'un poison qui ronge les cibles\n"
        "sur la durée, même après qu'elles ont quitté la zone de tir.",

    [MAT_PLASMA]=
        "Plasma électrisé instable récolté dans les générateurs détruits.\n"
        "À l'impact, un arc électrique rebondit sur 2 cibles proches.\n"
        "Dévastateur dans les groupes serrés.",

    [MAT_CRYO]  =
        "Agent cryogénique haute concentration issu des silos frigorifiques.\n"
        "Amplifie le ralentissement des tours Cryo : -70% au lieu de -50%.\n"
        "Les ennemis touches deviennent quasi immobiles.",

    [MAT_NANO]  =
        "Essaim nanobotique autoreplicant récupéré dans les labos abandonnés.\n"
        "Répare passivement les unités alliées dans un rayon de 3 cases.\n"
        "Indispensable pour maintenir vos ouvriers en vie.",
};

const char *DAMAGE_NAMES[DAMAGE_TYPE_COUNT] = {
    [DMG_PHYSICAL] = "Physique",
    [DMG_POISON]   = "Poison",
    [DMG_ELECTRIC] = "Électrique",
    [DMG_CRYO]     = "Cryo",
    [DMG_NANO]     = "Nano",
    [DMG_FIRE]     = "Feu",
};
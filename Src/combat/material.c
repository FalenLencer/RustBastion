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

const char *DAMAGE_NAMES[5] = {
    "Physique", "Poison", "Electrique", "Cryo", "Nano",
};
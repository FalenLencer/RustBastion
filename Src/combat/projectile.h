/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

#pragma once

#include "tower.h"
#include "enemy.h"

void projectile_spawn(TowerPool *tp, const Tower *tw,
                      int target_idx, const EnemyPool *ep);

void projectile_update(TowerPool *tp, EnemyPool *ep, float dt);

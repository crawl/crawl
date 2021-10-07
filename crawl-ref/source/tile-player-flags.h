#pragma once

#include "rltiles/tiledef-player.h"

enum tile_player_flags
{
    TILEP_SHOW_EQUIP    = 0x10000000,
};

static_assert(static_cast<int>(TILEP_SHOW_EQUIP) > static_cast<int>(TILEP_PLAYER_MAX),
        "TILEP_SHOW_EQUIP must be distinct from all player tile enums");

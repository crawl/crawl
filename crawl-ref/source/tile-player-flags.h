#pragma once

#include <limits>

#include "rltiles/tiledef-player.h"

const tileidx_t TILEP_SHOW_EQUIP = std::numeric_limits<tileidx_t>::max();

static_assert(TILEP_SHOW_EQUIP > static_cast<tileidx_t>(TILEP_PLAYER_MAX),
        "TILEP_SHOW_EQUIP must be distinct from all player tile enums");

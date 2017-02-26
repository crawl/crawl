#pragma once

#ifndef USE_TILE

#include "libconsole.h"

static inline constexpr bool is_tiles() { return false; }
void w32_insert_escape();

#endif


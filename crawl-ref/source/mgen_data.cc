#include "AppHdr.h"

#include "mgen_data.h"

#include "coord.h"

const std::string TUKIMA_WEAPON = "tukima-weapon";

bool mgen_data::use_position() const
{
    return (in_bounds(pos));
}

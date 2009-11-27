#include "AppHdr.h"

#include "mgen_data.h"

#include "coord.h"

bool mgen_data::use_position() const
{
    return (in_bounds(pos));
}

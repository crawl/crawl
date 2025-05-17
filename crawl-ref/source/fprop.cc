/**
 * @file
 * @brief Functions dealing with env.map_knowledge.
**/

#include "AppHdr.h"

#include "fprop.h"

#include "areas.h"
#include "coord.h"
#include "env.h"
#include "libutil.h"
#include "terrain.h"

bool is_sanctuary(const coord_def& p)
{
    if (!sanctuary_exists() || !map_bounds(p))
        return false;
    const bool sanct = (testbits(env.pgrid(p), FPROP_SANCTUARY_1)
                        || testbits(env.pgrid(p), FPROP_SANCTUARY_2));
    return sanct;
}

bool is_bloodcovered(const coord_def& p)
{
    return testbits(env.pgrid(p), FPROP_BLOODY);
}

bool is_icecovered(const coord_def& p)
{
    return feat_is_wall(env.grid(p)) && testbits(env.pgrid(p), FPROP_ICY);
}

bool is_blasphemy(const coord_def& p)
{
    return testbits(env.pgrid(p), FPROP_BLASPHEMY);
}

bool is_gastronomic(const coord_def& p)
{
    return !cell_is_solid(p) && testbits(env.pgrid(p), FPROP_GASTRONOMY);
}

bool is_tide_immune(const coord_def &p)
{
    return bool(env.pgrid(p) & FPROP_NO_TIDE);
}

feature_property_type str_to_fprop(const string &str)
{
    if (str == "bloody")
        return FPROP_BLOODY;
    if (str == "highlight")
        return FPROP_HIGHLIGHT;
    if (str == "no_cloud_gen")
        return FPROP_NO_CLOUD_GEN;
    if (str == "no_tele_into")
        return FPROP_NO_TELE_INTO;
    if (str == "no_tide")
        return FPROP_NO_TIDE;
    if (str == "no_jiyva")
        return FPROP_NO_JIYVA;
    if (str == "no_automap")
        return FPROP_NO_AUTOMAP;

    return FPROP_NONE;
}

#ifdef USE_TILE
char blood_rotation(const coord_def & p)
{
    return (env.pgrid(p) & FPROP_BLOOD_EAST).flags >> 16;
}
#endif

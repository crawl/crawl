#pragma once

#include "tag-version.h"

/// Bitmasks for area properties that center on actors
enum class area_type
{
    silence       = (1 << 0),
    halo          = (1 << 1),
    umbra         = (1 << 2),
    liquified     = (1 << 3),
    orb           = (1 << 4), ///< The glow of the Orb of Zot
    quad          = (1 << 5),
    disjunction   = (1 << 6),
    awoken_forest = (1 << 7),
};
/// Bit field for the area properties
DEF_BITFIELD(areaprops, area_type);

void invalidate_agrid(bool force = true);

class actor;
void areas_actor_moved(const actor* act);

void create_sanctuary(const coord_def& center, int time);
bool remove_sanctuary();
void decrease_sanctuary_radius();
bool sanctuary_exists();

int shrinking_aoe_range(int dur);

coord_def find_centre_for(const coord_def& f, area_type at);

bool silenced(const coord_def& p);

// Does the given point lie within a halo?
bool haloed(const coord_def& p);

// or is the ground there liquefied?
// @param ledas_only If true, only return true if Leda's Liquefaction was
//                   responsible. This affects descriptions only.
bool liquefied(const coord_def& p, bool ledas_only = false);

// Is it enlightened by the orb?
bool orb_haloed(const coord_def& p);

// ...or by a quad damage?
bool quad_haloed(const coord_def& p);

// ...or by disjunction?
bool disjunction_haloed(const coord_def& p);

// ...or endarkened by an umbra?
bool umbraed(const coord_def& p);

// Is this square within reach of an awakened forest's caster?
bool forest_awoken(const coord_def& p);

#if TAG_MAJOR_VERSION == 34
// ...or is the area hot?
bool heated(const coord_def& p);
#endif

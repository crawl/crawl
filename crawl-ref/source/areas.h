#pragma once

#include "tag-version.h"

enum class area_centre_type
{
    none,
    sanctuary,
    silence,
    halo,
    liquid,
    orb,
    umbra,
    quad,
    disjunction,
#if TAG_MAJOR_VERSION == 34
    hot,
#endif
};

void invalidate_agrid(bool recheck_new = false);

class actor;
void areas_actor_moved(const actor* act, const coord_def& oldpos);

void create_sanctuary(const coord_def& center, int time);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius();
bool sanctuary_exists();

int shrinking_aoe_range(int dur);

coord_def find_centre_for(const coord_def& f,
                          area_centre_type at = area_centre_type::none);

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

#if TAG_MAJOR_VERSION == 34
// ...or is the area hot?
bool heated(const coord_def& p);
#endif

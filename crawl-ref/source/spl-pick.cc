/**
 * @file
 * @brief Functions for picking from lists of spells.
**/

#include "AppHdr.h"

#include "spl-pick.h"

spell_type spell_picker::pick_with_veto(const spell_entry *weights,
                                        int level, spell_type none,
                                        spell_pick_vetoer vetoer)
{
    veto_func = vetoer;
    return pick(weights, level, none);
}

// Veto specialisation for the spell_picker class; this simply calls the
// stored veto function. Can subclass further for more complex veto behaviour.
bool spell_picker::veto(spell_type mon)
{
    return veto_func && veto_func(mon);
}

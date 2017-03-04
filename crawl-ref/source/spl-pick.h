/**
 * @file
 * @brief Functions for picking from lists of spells.
**/

#pragma once

#include "random-pick.h"

#define spell_entry random_pick_entry<spell_type>
typedef bool (*spell_pick_vetoer)(spell_type);

// Subclass a spell picker using the random_picker template
class spell_picker : public random_picker<spell_type, NUM_SPELLS>
{
public:
    spell_picker(spell_pick_vetoer _veto = nullptr) : veto_func(_veto) { };

    spell_type pick_with_veto(const spell_entry *weights, int level,
                              spell_type none,
                              spell_pick_vetoer veto_func = nullptr);

    virtual bool veto(spell_type spell) override;

protected:
    spell_pick_vetoer veto_func;
};

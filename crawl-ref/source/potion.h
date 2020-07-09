/**
 * @file
 * @brief Potion and potion-like effects.
**/

#pragma once

#include "potion-type.h"

bool you_drinkless(bool temp = true);

class PotionEffect
{
private:
    PotionEffect() = delete;
    DISALLOW_COPY_AND_ASSIGN(PotionEffect);
protected:
    PotionEffect(potion_type);
    bool check_known_quaff() const;
public:
    virtual bool can_quaff(string *reason = nullptr) const;

    /**
     * Elsewhere in the code there are things that can have the effect
     * effect of a potion without actually being potions, there
     * are even some legacy 'potions' around in this code that
     * nowadays only provide the potion-like effect.
     *
     * Doesn't currently handle conducts, but possibly it should.
     *
     * @param was_known     Whether the player should be held responsible.
     * @param pow           The 'power' of the effect. Mostly disused.
     * @param is_potion     Whether to apply the effects of MUT_NO_POTION_HEAL.
     * @return              Whether or not the potion had an effect.
     */
    virtual bool effect(bool was_known = true, int pow = 40,
                        bool is_potion = true) const = 0;

    // Quaff also handles god-conduct and potion-specific
    // uncancellability
    // Returns whether or not the potion was actually quaffed
    virtual bool quaff(bool was_known) const;

    const string potion_name;
    const potion_type kind;
};

const PotionEffect* get_potion_effect(potion_type pot);

bool quaff_potion(item_def &potion);
void potionlike_effect(potion_type effect, int pow, bool was_known = true);

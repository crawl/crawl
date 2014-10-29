/**
 * @file
 * @brief Monsters stealth methods.
**/

#include "AppHdr.h"

#include "monster.h"

#include "mon-util.h"

static int _clamp_stealth(int stealth)
{
    if (stealth > 3)
        return 3;
    else if (stealth < -3)
        return -3;
    else
        return stealth;
}

// Monster stealth is a value between:
//
// -3 - Extremely unstealthy
// -2 - Very unstealthy
// -1 - Unstealthy
//  0 - Normal
//  1 - Stealthy
//  2 - Very stealthy
//  3 - Extremely stealthy
//
int monster::stealth() const
{
    if (berserk_or_insane())
        return -3;

    int base_stealth = -(min((int) body_size(), 6) - 3);

    int actual_stealth = base_stealth;

    // Undead are inherently more stealthy.
    if (holiness() == MH_UNDEAD)
    {
        // Zombies are less stealthy.
        if (type == MONS_ZOMBIE)
        {
            // Large zombies even more so.
            actual_stealth -= mons_zombie_size(base_monster);
        }
        // Other undead are otherwise stealthy.
        else
            actual_stealth++;
    }

    // If you're floundering in water, you're unstealthy.
    if (floundering())
        actual_stealth -= 2;

    // Orcs are a noisy bunch and get a penalty here to affect orc wizard
    // invisibility.
    if (mons_genus(type) == MONS_ORC)
        actual_stealth--;

    // Not an issue with invisibility, but glowing or haloes make you
    // unstealthy.
    if (glows_naturally() || halo_radius2() != -1)
        actual_stealth -= 3;

    // Having an umbra makes you more stealthy, on the other hand.
    if (mons_class_flag(type, M_SHADOW) || umbra_radius2() != -1)
        actual_stealth += 3;

    // Some specific overrides
    if (type == MONS_UNSEEN_HORROR)
        actual_stealth = 3;

    return _clamp_stealth(actual_stealth);
}

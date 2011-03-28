/**
 * @file
 * @brief Monsters stealth methods.
**/

#include "AppHdr.h"

#include "monster.h"
#include "mon-util.h"

static int _clamp_stealth (int stealth)
{
    if (stealth > 3)
    {
        return (3);
    }
    else if (stealth < -3)
    {
        return (-3);
    }
    else
    {
        return (stealth);
    }
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
    int base_stealth = -(std::min((int) body_size(), 6) - 3);

    int actual_stealth = base_stealth;

    // Undead are inherently more stealthy.
    if (holiness() == MH_UNDEAD)
    {
        // Zombies are less stealthy.
        if (type == MONS_ZOMBIE_SMALL)
            actual_stealth--;

        // Larger zombies even more so.
        else if (type == MONS_ZOMBIE_LARGE)
            actual_stealth -= 2;

        // Other undead are otherwise stealthy.
        else
            actual_stealth++;
    }

    // If you're floundering in water, you're unstealthy.
    if (floundering())
        actual_stealth -= 2;

    // Orcs are a noisy bunch and get a penalty here to affect orc wizard
    // invisibility.
    if (mons_genus(this->type) == MONS_ORC)
        actual_stealth--;

    // Not an issue with invisibility, but glowing or haloes make you
    // unstealthy.
    if (glows_naturally() || halo_radius2() != -1)
        actual_stealth -= 3;

    // Some specific overrides
    if (type == MONS_UNSEEN_HORROR)
        actual_stealth = 3;

    return _clamp_stealth(actual_stealth);
}

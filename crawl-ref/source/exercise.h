/**
 * @file
 * @brief Collects all calls to skills.cc:exercise for
 *            easier changes to the training modell.
**/

#ifndef EXERCISE_H
#define EXERCISE_H

enum exer_type
{
    EX_BEAM_MAY_HIT,         // beam was not blocked
    EX_BEAM_WILL_HIT,        // beam was not blocked or dodged
    EX_WILL_STAB,
    EX_WILL_HIT,             // melee
    EX_MONSTER_MAY_HIT,      // melee attack was not blocked
    EX_MONSTER_WILL_HIT,     // melee attack was not blocked or dodged
    EX_WILL_LAUNCH,          // launchers
    EX_WILL_THROW_MSL,       // missiles
    EX_WILL_THROW_WEAPON,    // weapons
    EX_WILL_THROW_OTHER,     // junk
    EX_USED_ABIL,
    EX_DID_CAST,
    EX_DID_MISCAST,
    EX_SHIELD_BLOCK,
    EX_DODGE_TRAP,
    EX_SHIELD_BEAM_FAIL,
    EX_SNEAK,
    EX_SNEAK_INVIS,
    EX_DID_EVOKE_ITEM,
    EX_DID_ZAP_WAND,
    EX_WILL_READ_TOME,
    EX_WAIT,
};

void practise(exer_type ex, int param1 = 0);
skill_type abil_skill(ability_type abil);

#endif

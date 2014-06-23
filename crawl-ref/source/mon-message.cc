/**
 * @file
 * @brief Monster message-connected functions.
**/

#include "AppHdr.h"
#include "mon-message.h"
#include <sstream>

#include "defines.h"
#include "env.h"
#include "libutil.h"
#include "monster.h"
#include "mon-util.h"
#include "view.h"

// Note that this function *completely* blocks messaging for monsters
// distant or invisible to the player ... look elsewhere for a function
// permitting output of "It" messages for the invisible {dlb}
// Intentionally avoids info and str_pass now. - bwr
bool simple_monster_message(const monster* mons, const char *event,
                            msg_channel_type channel,
                            int param,
                            description_level_type descrip)
{
    if (mons_near(mons)
        && (channel == MSGCH_MONSTER_SPELL || channel == MSGCH_FRIEND_SPELL
            || mons->visible_to(&you)))
    {
        string msg = mons->name(descrip);
        msg += event;
        msg = apostrophise_fixup(msg);

        if (channel == MSGCH_PLAIN && mons->wont_attack())
            channel = MSGCH_FRIEND_ACTION;

        mprf(channel, param, "%s", msg.c_str());
        return true;
    }

    return false;
}

mon_dam_level_type mons_get_damage_level(const monster* mons)
{
    if (!mons_can_display_wounds(mons))
        return MDAM_OKAY;

    if (mons->hit_points <= mons->max_hit_points / 5)
        return MDAM_ALMOST_DEAD;
    else if (mons->hit_points <= mons->max_hit_points * 2 / 5)
        return MDAM_SEVERELY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points * 3 / 5)
        return MDAM_HEAVILY_DAMAGED;
    else if (mons->hit_points <= mons->max_hit_points * 4 / 5)
        return MDAM_MODERATELY_DAMAGED;
    else if (mons->hit_points < mons->max_hit_points)
        return MDAM_LIGHTLY_DAMAGED;
    else
        return MDAM_OKAY;
}

string get_damage_level_string(mon_holy_type holi, mon_dam_level_type mdam)
{
    ostringstream ss;
    switch (mdam)
    {
    case MDAM_ALMOST_DEAD:
        ss << "almost";
        ss << (wounded_damaged(holi) ? " destroyed" : " dead");
        return ss.str();
    case MDAM_SEVERELY_DAMAGED:
        ss << "severely";
        break;
    case MDAM_HEAVILY_DAMAGED:
        ss << "heavily";
        break;
    case MDAM_MODERATELY_DAMAGED:
        ss << "moderately";
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ss << "lightly";
        break;
    case MDAM_OKAY:
    default:
        ss << "not";
        break;
    }
    ss << (wounded_damaged(holi) ? " damaged" : " wounded");
    return ss.str();
}

void print_wounds(const monster* mons)
{
    if (!mons->alive() || mons->hit_points == mons->max_hit_points)
        return;

    if (!mons_can_display_wounds(mons))
        return;

    mon_dam_level_type dam_level = mons_get_damage_level(mons);
    string desc = get_damage_level_string(mons->holiness(), dam_level);

    desc.insert(0, " is ");
    desc += ".";
    simple_monster_message(mons, desc.c_str(), MSGCH_MONSTER_DAMAGE,
                           dam_level);
}

// (true == 'damaged') [constructs, undead, etc.]
// and (false == 'wounded') [living creatures, etc.] {dlb}
bool wounded_damaged(mon_holy_type holi)
{
    // this schema needs to be abstracted into real categories {dlb}:
    return holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT;
}

bool mons_class_can_display_wounds(monster_type mc)
{
    // Zombified monsters other than spectral things don't show
    // wounds.
    if (mons_class_is_zombified(mc) && mc != MONS_SPECTRAL_THING)
        return false;

    return true;
}

bool mons_can_display_wounds(const monster* mon)
{
    get_tentacle_head(mon);

    return mons_class_can_display_wounds(mon->type);
}

#include <cstring>
#include <string>
#include <sstream>

#include "AppHdr.h"

#include "movement.h"

#include "bloodspatter.h"
#include "message.h"
#include "player.h"
#include "player-reacts.h"
#include "prompt.h"

bool cancel_barbed_move()
{
    if (you.duration[DUR_BARBS] && !you.props.exists(BARBS_MOVE_KEY))
    {
        std::string prompt = "The barbs in your skin will harm you if you move."
                        " Continue?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }

        you.props[BARBS_MOVE_KEY] = true;
    }

    return false;
}

void apply_barbs_damage()
{
    if (you.duration[DUR_BARBS])
    {
        mprf(MSGCH_WARN, "The barbed spikes dig painfully into your body "
                         "as you move.");
        ouch(roll_dice(2, you.attribute[ATTR_BARBS_POW]), KILLED_BY_BARBS);
        bleed_onto_floor(you.pos(), MONS_PLAYER, 2, false);

        // Sometimes decrease duration even when we move.
        if (one_chance_in(3))
            extract_manticore_spikes("The barbed spikes snap loose.");
        // But if that failed to end the effect, duration stays the same.
        if (you.duration[DUR_BARBS])
            you.duration[DUR_BARBS] += you.time_taken;
    }
}

void remove_ice_armour_movement()
{
    if (you.duration[DUR_ICY_ARMOUR])
    {
        mprf(MSGCH_DURATION, "Your icy armour cracks and falls away as "
                             "you move.");
        you.duration[DUR_ICY_ARMOUR] = 0;
        you.redraw_armour_class = true;
    }
}

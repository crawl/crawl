/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-prop.h"
#include "message.h"
#include "player-equip.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"
#include "spl-summoning.h"

spret cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + div_rand_round(random2(1 + power), 5),
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);
    you.props[CONFUSING_TOUCH_KEY] = power;

    return spret::success;
}

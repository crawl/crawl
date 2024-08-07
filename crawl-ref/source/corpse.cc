/**
 * @file
 * @brief Functions for corpse handling.
 **/

#include "AppHdr.h"

#include "corpse.h"

#include "bloodspatter.h"
#include "defines.h"
#include "env.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "makeitem.h"
#include "mon-util.h"

bool is_rottable(const item_def &it)
{
    return it.defined()
        && !is_shop_item(it)
        && !it.props.exists(CORPSE_NEVER_DECAYS)
        && it.is_type(OBJ_CORPSES, CORPSE_BODY);
}

/**
 * Rot a corpse on the floor.
 *
 * @param it            The corpse to rot.
 * @param mitm_index    The slot of the corpse in the floor item array.
 * @param rot_time      The amount of time to rot the corpse for.
 */
static void _maybe_rot_corpse(item_def &it, int mitm_index, int rot_time)
{
    if (!is_rottable(it))
        return;

    it.freshness -= rot_time;
    if (it.freshness > 0)
        return;

    if (!mons_skeleton(it.mon_type))
    {
        item_was_destroyed(it);
        destroy_item(mitm_index);
    }
    else
        turn_corpse_into_skeleton(it);
}

/**
 * Decay corpses
 *
 * @param elapsedTime   The amount of time to rot the corpses for.
 */
void rot_corpses(int elapsedTime)
{
    if (elapsedTime <= 0)
        return;

    const int rot_time = elapsedTime / ROT_TIME_FACTOR;

    for (int mitm_index = 0; mitm_index < MAX_ITEMS; ++mitm_index)
    {
        item_def &it = env.item[mitm_index];
        _maybe_rot_corpse(it, mitm_index, rot_time);
    }
}

/** Skeletonise this corpse.
 *
 *  @param item the corpse to be turned into a skeleton.
 *  @returns whether a valid skeleton could be made.
 */
bool turn_corpse_into_skeleton(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons
    // behind.
    if (!mons_skeleton(item.mon_type))
        return false;

    item.sub_type = CORPSE_SKELETON;
    item.rnd = 1 + random2(255); // not sure this is necessary, but...
    item.props.erase(FORCED_ITEM_COLOUR_KEY);
    return true;
}

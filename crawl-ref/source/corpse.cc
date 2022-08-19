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

/**
 * Check whether an item decays over time. (Corpses, chunks, and blood.)
 *
 * @param item      The item to check.
 * @return          Whether the item changes (rots) over time.
 */
static bool _item_needs_rot_check(const item_def &item)
{
    if (!item.defined())
        return false;

    if (item.props.exists(CORPSE_NEVER_DECAYS))
        return false;

    return item.base_type == OBJ_CORPSES
           && item.sub_type <= CORPSE_SKELETON; // XXX: is this needed?
}

/**
 * Rot a corpse or skeleton lying on the floor.
 *
 * @param it            The corpse or skeleton to rot.
 * @param mitm_index    The slot of the corpse in the floor item array.
 * @param rot_time      The amount of time to rot the corpse for.
 */
static void _rot_corpse(item_def &it, int mitm_index, int rot_time)
{
    ASSERT(it.base_type == OBJ_CORPSES);
    ASSERT(!it.props.exists(CORPSE_NEVER_DECAYS));

    it.freshness -= rot_time;
    if (it.freshness > 0)
        return;

    if (it.sub_type == CORPSE_SKELETON || !mons_skeleton(it.mon_type))
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

        if (is_shop_item(it) || !_item_needs_rot_check(it))
            continue;

        if (it.base_type == OBJ_CORPSES)
            _rot_corpse(it, mitm_index, rot_time);
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
    item.freshness = FRESHEST_CORPSE; // reset rotting counter
    item.rnd = 1 + random2(255); // not sure this is necessary, but...
    item.props.erase(FORCED_ITEM_COLOUR_KEY);
    return true;
}

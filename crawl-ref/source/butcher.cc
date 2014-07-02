/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#include "AppHdr.h"

#include "butcher.h"

#include "bloodspatter.h"
#include "env.h"
#include "food.h"
#include "itemprop.h"
#include "itemprop-enum.h"
#include "items.h"
#include "makeitem.h"
#include "mon-death.h"
#include "random.h"
#include "rot.h"
#include "stuff.h"

static void _create_monster_hide(const item_def corpse)
{
    // kiku_receive_corpses() creates corpses that are easily scummed
    // for hides.  We prevent this by setting "never_hide" as an item
    // property of corpses it creates.
    if (corpse.props.exists("never_hide"))
        return;

    monster_type mons_class = corpse.mon_type;
    armour_type type;

    // These values cannot be set by a reasonable formula: {dlb}
    if (mons_genus(mons_class) == MONS_TROLL)
        mons_class = MONS_TROLL;
    switch (mons_class)
    {
        case MONS_FIRE_DRAGON:    type = ARM_FIRE_DRAGON_HIDE;    break;
        case MONS_TROLL:          type = ARM_TROLL_HIDE;          break;
        case MONS_ICE_DRAGON:     type = ARM_ICE_DRAGON_HIDE;     break;
        case MONS_STEAM_DRAGON:   type = ARM_STEAM_DRAGON_HIDE;   break;
        case MONS_MOTTLED_DRAGON: type = ARM_MOTTLED_DRAGON_HIDE; break;
        case MONS_STORM_DRAGON:   type = ARM_STORM_DRAGON_HIDE;   break;
        case MONS_GOLDEN_DRAGON:  type = ARM_GOLD_DRAGON_HIDE;    break;
        case MONS_SWAMP_DRAGON:   type = ARM_SWAMP_DRAGON_HIDE;   break;
        case MONS_PEARL_DRAGON:   type = ARM_PEARL_DRAGON_HIDE;   break;
        default:
            die("an unknown hide drop");
    }

    int o = items(0, OBJ_ARMOUR, type, true, 0, 0, 0, -1, true);
    if (o == NON_ITEM)
        return;
    item_def& item = mitm[o];

    do_uncurse_item(item, false);

    // Automatically identify the created hide.
    set_ident_flags(item, ISFLAG_IDENT_MASK);

    const monster_type montype =
    static_cast<monster_type>(corpse.orig_monnum);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
        move_item_to_grid(&o, pos);
}

void maybe_drop_monster_hide(const item_def corpse)
{
    if (mons_class_leaves_hide(corpse.mon_type) && !one_chance_in(3))
        _create_monster_hide(corpse);
}

int get_max_corpse_chunks(monster_type mons_class)
{
    return mons_weight(mons_class) / 150;
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
    item.special  = FRESHEST_CORPSE; // reset rotting counter
    item.colour   = LIGHTGREY;
    return true;
}

static void _maybe_bleed_monster_corpse(const item_def corpse)
{
    // Only fresh corpses bleed enough to colour the ground.
    if (!food_is_rotten(corpse))
    {
        const coord_def pos = item_pos(corpse);
        if (!pos.origin())
        {
            const int max_chunks = get_max_corpse_chunks(corpse.mon_type);
            bleed_onto_floor(pos, corpse.mon_type, max_chunks, true);
        }
    }
}

void turn_corpse_into_chunks(item_def &item, bool bloodspatter,
                             bool make_hide)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);
    const item_def corpse = item;
    const int max_chunks = get_max_corpse_chunks(item.mon_type);

    // Only fresh corpses bleed enough to colour the ground.
    if (bloodspatter)
        _maybe_bleed_monster_corpse(corpse);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    bool wants_for_spells = you.has_spell(SPELL_SUBLIMATION_OF_BLOOD);
    // Don't mark it as dropped if we are forcing autopickup of chunks.
    if (you.force_autopickup[OBJ_FOOD][FOOD_CHUNK] <= 0
        && is_bad_food(item) && !wants_for_spells)
    {
        item.flags |= ISFLAG_DROPPED;
    }
    else if (you.species != SP_VAMPIRE)
        clear_item_pickup_flags(item);

    // Happens after the corpse has been butchered.
    if (make_hide)
        maybe_drop_monster_hide(corpse);
}

static void _turn_corpse_into_skeleton_and_chunks(item_def &item, bool prefer_chunks)
{
    item_def copy = item;

    // Complicated logic, but unless we use the original, both could fail if
    // mitm[] is overstuffed.
    if (prefer_chunks)
    {
        turn_corpse_into_chunks(item);
        turn_corpse_into_skeleton(copy);
    }
    else
    {
        turn_corpse_into_chunks(copy);
        turn_corpse_into_skeleton(item);
    }

    copy_item_to_grid(copy, item_pos(item));
}

void butcher_corpse(item_def &item, maybe_bool skeleton, bool chunks)
{
    item_was_destroyed(item);
    if (!mons_skeleton(item.mon_type))
        skeleton = MB_FALSE;
    if (skeleton == MB_TRUE || skeleton == MB_MAYBE && one_chance_in(3))
    {
        if (chunks)
            _turn_corpse_into_skeleton_and_chunks(item, skeleton != MB_TRUE);
        else
        {
            _maybe_bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            turn_corpse_into_skeleton(item);
        }
    }
    else
    {
        if (chunks)
            turn_corpse_into_chunks(item);
        else
        {
            _maybe_bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            destroy_item(item.index());
        }
    }
}

bool check_blood_corpses_on_ground()
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
            && !food_is_rotten(*si)
            && mons_has_blood(si->mon_type))
        {
            return true;
        }
    }
    return false;
}

// Deliberately don't check for rottenness here, so this check
// can also be used to verify whether you *could* have bottled
// a now rotten corpse.
bool can_bottle_blood_from_corpse(monster_type mons_class)
{
    if (you.species != SP_VAMPIRE || you.experience_level < 6
        || !mons_has_blood(mons_class))
    {
        return false;
    }

    int chunk_type = mons_corpse_effect(mons_class);
    if (chunk_type == CE_CLEAN)
        return true;

    return false;
}

int num_blood_potions_from_corpse(monster_type mons_class, int chunk_type)
{
    if (chunk_type == -1)
        chunk_type = mons_corpse_effect(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    const int max_chunks = get_max_corpse_chunks(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    int pot_quantity = max_chunks / 3;
    pot_quantity = stepdown_value(pot_quantity, 2, 2, 6, 6);

    if (pot_quantity < 1)
        pot_quantity = 1;

    return pot_quantity;
}

// If autopickup is active, the potions are auto-picked up after creation.
void turn_corpse_into_blood_potions(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(!food_is_rotten(item));

    const item_def corpse = item;
    const monster_type mons_class = corpse.mon_type;

    ASSERT(can_bottle_blood_from_corpse(mons_class));

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    clear_item_pickup_flags(item);

    item.quantity = num_blood_potions_from_corpse(mons_class);

    // Initialise timer depending on corpse age:
    // almost rotting: age = 100 --> potion timer =  500 --> will coagulate soon
    // freshly killed: age = 200 --> potion timer = 2500 --> fresh !blood
    init_stack_blood_potions(item, (item.special - 100) * 20 + 500);

    // Happens after the blood has been bottled.
    maybe_drop_monster_hide(corpse);
}

void turn_corpse_into_skeleton_and_blood_potions(item_def &item)
{
    item_def blood_potions = item;

    if (mons_skeleton(item.mon_type))
        turn_corpse_into_skeleton(item);

    int o = get_mitm_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_blood_potions(blood_potions);
        copy_item_to_grid(blood_potions, you.pos());
    }
}

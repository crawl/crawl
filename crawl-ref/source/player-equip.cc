#include "AppHdr.h"

#include "player-equip.h"

#include <cmath>

#include "ability.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "database.h"
#include "delay.h"
#include "english.h" // conjugate_verb
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h" // command_to_string
#include "monster.h"
#include "message.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "sound.h"
#include "spl-clouds.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "stringutil.h"
#include "tag-version.h"
#include "transform.h"
#include "xom.h"

static void _mark_unseen_monsters();

/**
 * Returns how many slots of a given type the player character currently has
 * (potentially accounting for additional slots granted by forms, mutations, and
 * items like the Macabre Finger).
 *
 * This does NOT care about whether these slots might be melded by the player's
 * current form. (ie: the player stills has a gloves slot while using Blade Form
 * - they just can't do anything with it.)
 *
 * @param slot              The equipment slot being checked.
 * @param zero_reason[out]  If there are no slots of the given type, and this is
 *                          non-null, it is set to the reason why there are 0.
 *
 * @return The number of slots of the given type the player has.
 */
int get_player_equip_slot_count(equipment_slot slot, string* zero_reason)
{
#define NO_SLOT(x) {if (count == 0) {if (zero_reason) { *zero_reason = x; }; return 0;}}

size_type player_size = you.body_size(PSIZE_TORSO, true);
int count = 0;

    switch (slot)
    {

    case SLOT_WEAPON:
        if (you.has_mutation(MUT_NO_GRASPING))
            NO_SLOT("You can't wield any weapon.")
        else
            return 1;

    case SLOT_OFFHAND:
        if (you.has_mutation(MUT_MISSING_HAND))
        {
            if (you.has_innate_mutation(MUT_TENTACLE_ARMS))
                NO_SLOT("You need the rest of your tentacles for walking.")
            else
            {
                NO_SLOT(make_stringf("You'd need another %s to do that!",
                                        you.hand_name(false).c_str()))
            }
        }
        else if (you.has_mutation(MUT_NO_GRASPING))
            NO_SLOT("You can't hold anything in your paws.")
        // Coglins get a SLOT_WEAPON_OR_OFFHAND instead.
        else if (you.has_mutation(MUT_WIELD_OFFHAND))
            return 0;

        return 1;

    case SLOT_WEAPON_OR_OFFHAND:
        if (you.has_mutation(MUT_WIELD_OFFHAND))
            return 1;

        return 0;

    case SLOT_BODY_ARMOUR:
        if (you.has_mutation(MUT_FORMLESS))
            NO_SLOT("You can't haunt something so large.")
        if (species::is_draconian(you.species))
        {
            NO_SLOT(make_stringf("Your wings%s won't fit in that.",
                                    you.has_mutation(MUT_BIG_WINGS)
                                    ? "" : ", even vestigial as they are,"))
        }
        else if (you.species == SP_OCTOPODE || you.has_mutation(MUT_NO_ARMOUR))
            NO_SLOT("You can't wear armour!");

        return 1;

    // Hats versus helmets is handled elsewhere. If you can wear at least a hat,
    // this should be non-zero.
    case SLOT_HELMET:
        if (you.unrand_equipped(UNRAND_SKULL_OF_ZONGULDROK))
            ++count;

        if (you.has_mutation(MUT_FORMLESS))
            NO_SLOT("You don't have a head.")
        else if (you.has_mutation(MUT_NO_ARMOUR))
            NO_SLOT("That is much too large for your head.")
        else if (you.get_mutation_level(MUT_HORNS, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("You can't wear any headgear with your large horns!")
        else if (you.get_mutation_level(MUT_ANTENNAE, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("You can't wear any headgear with your large antennae!")
        else
            ++count;

        return count;

    case SLOT_GLOVES:
        if (you.unrand_equipped(UNRAND_FISTICLOAK))
            ++count;

        if (you.has_mutation(MUT_QUADRUMANOUS))
            ++count;

        if (you.has_mutation(MUT_FORMLESS))
            NO_SLOT("You don't have hands.")
        else if (player_size <= SIZE_LITTLE)
            NO_SLOT(make_stringf("Those are too big for your %s.", you.hand_name(true).c_str()))
        else if (player_size >= SIZE_LARGE)
            NO_SLOT(make_stringf("Those are too small for your %s.", you.hand_name(true).c_str()))
        else if (you.species == SP_OCTOPODE || you.has_mutation(MUT_NO_ARMOUR))
            NO_SLOT(make_stringf("Those can't fit on your %s.", you.hand_name(true).c_str()))
        else if (you.get_mutation_level(MUT_CLAWS, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT(make_stringf("Those can't fit over your huge %s.", you.hand_name(true).c_str()))
        else if (you.get_mutation_level(MUT_DEMONIC_TOUCH, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("Your demonic touch would destroy those.")
        else
            ++count;

        return count;

    case SLOT_BOOTS:
        if (species::wears_barding(you.species) || you.has_mutation(MUT_FORMLESS))
            NO_SLOT("You don't have any feet!")
        else if (player_size <= SIZE_LITTLE)
            NO_SLOT(make_stringf("Those are too big for your %s.", you.foot_name(true).c_str()))
        else if (player_size >= SIZE_LARGE)
            NO_SLOT(make_stringf("Those are too small for your %s.", you.foot_name(true).c_str()))
        else if (you.species == SP_OCTOPODE || you.has_mutation(MUT_NO_ARMOUR))
            NO_SLOT("You can't wear boots.")
        else if (you.get_mutation_level(MUT_HOOVES, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("Your hooves can't fit into boots.")
        else if (you.get_mutation_level(MUT_TALONS, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("Your talons can't fit into boots.")
        else if (you.has_mutation(MUT_FLOAT))
            NO_SLOT("You have no feet!")

        return 1;

    case SLOT_BARDING:
        if (species::wears_barding(you.species))
            return 1;

        NO_SLOT("You can't fit into that!")

    case SLOT_CLOAK:
        if (you.has_mutation(MUT_FORMLESS))
            NO_SLOT("You don't have any shoulders.")
        else if (you.species == SP_OCTOPODE || you.has_mutation(MUT_NO_ARMOUR))
            NO_SLOT("You can't wear that.")
        else if (you.get_mutation_level(MUT_WEAKNESS_STINGER, mutation_activity_type::INACTIVE) >= 3)
            NO_SLOT("You can't wear that with your sharp stinger!")

        return 1;

    case SLOT_RING:
    {
        if (you.has_mutation(MUT_NO_JEWELLERY))
            NO_SLOT("You can't wear any rings.")

        int ring_count = 2;
        if (you.species == SP_OCTOPODE)
            ring_count = 8;

        if (you.has_mutation(MUT_MISSING_HAND))
            ring_count -= 1;

        if (you.unrand_equipped(UNRAND_FINGER_AMULET))
            ring_count += 1;

        if (you.unrand_equipped(UNRAND_VAINGLORY))
            ring_count += 2;

        return ring_count;
    }

    case SLOT_AMULET:
        if (you.has_mutation(MUT_NO_JEWELLERY))
            NO_SLOT("You can't wear amulets.")

        if (you.unrand_equipped(UNRAND_JUSTICARS_REGALIA))
            return 2;

        return 1;

    case SLOT_GIZMO:
        if (you.species != SP_COGLIN)
            NO_SLOT("You lack an exoframe to install that in.")

        return 1;

    case SLOT_HAUNTED_AUX:
        if (you.has_mutation(MUT_FORMLESS))
            return 6;
        else
            return 0;

    default:
        return 0;

    }
#undef NO_SLOT
}

const static vector<equipment_slot> _flex_slots[] =
{
    {},
    {SLOT_WEAPON, SLOT_WEAPON_OR_OFFHAND},
    {SLOT_OFFHAND, SLOT_WEAPON_OR_OFFHAND},
    {SLOT_BODY_ARMOUR},
    {SLOT_HELMET, SLOT_HAUNTED_AUX},
    {SLOT_GLOVES, SLOT_HAUNTED_AUX},
    {SLOT_BOOTS, SLOT_HAUNTED_AUX},
    {SLOT_BARDING},
    {SLOT_CLOAK, SLOT_HAUNTED_AUX},
    {SLOT_RING},
    {SLOT_AMULET},
    {SLOT_GIZMO},

    {SLOT_WEAPON_OR_OFFHAND, SLOT_WEAPON, SLOT_OFFHAND},
    {SLOT_HAUNTED_AUX, SLOT_HELMET, SLOT_GLOVES, SLOT_BOOTS, SLOT_CLOAK},

    // NUM_EQUIP_SLOTS
    {},

    // 'Fake' slots (for querying purposes)

    // SLOT_LOWER_BODY
    {SLOT_BOOTS, SLOT_BARDING},

    // SLOT_ALL_ARMOUR
    {SLOT_BODY_ARMOUR, SLOT_HELMET, SLOT_GLOVES, SLOT_BOOTS, SLOT_BARDING,
     SLOT_CLOAK, SLOT_HAUNTED_AUX},

    // SLOT_ALL_AUX_ARMOUR
    {SLOT_HELMET, SLOT_GLOVES, SLOT_BOOTS, SLOT_BARDING, SLOT_CLOAK, SLOT_HAUNTED_AUX},

    // SLOT_ALL_JEWELLERY
    {SLOT_RING, SLOT_AMULET},

    // SLOT_ALL_EQUIPMENT
    {SLOT_WEAPON, SLOT_OFFHAND, SLOT_BODY_ARMOUR, SLOT_HELMET, SLOT_GLOVES,
     SLOT_BOOTS, SLOT_BARDING, SLOT_CLOAK, SLOT_RING, SLOT_AMULET, SLOT_GIZMO,
     SLOT_WEAPON_OR_OFFHAND, SLOT_HAUNTED_AUX},

    // SLOT_WEAPON_STRICT
    {SLOT_WEAPON},
};

const vector<equipment_slot>& get_alternate_slots(equipment_slot slot)
{
    ASSERT(slot < END_OF_SLOTS && slot >= SLOT_UNUSED);
    return _flex_slots[slot];
}

player_equip_set::player_equip_set()
{
    num_slots.init(SLOT_UNUSED);
    items.clear();
    unrand_active.init(false);
    artprop_cache.init(0);
    do_unrand_reacts = 0;
    do_unrand_death_effects = 0;
}

int player_equip_set::wearing_ego(object_class_type obj_type, int ego) const
{
    int total = 0;
    for (const player_equip_entry& entry : items)
    {
        if (entry.slot == SLOT_UNUSED || entry.is_overflow || entry.melded)
            continue;

        item_def& item = you.inv[entry.item];
        if (item.base_type == obj_type)
        {
            switch (obj_type)
            {
                case OBJ_WEAPONS:
                    if (get_weapon_brand(item) == ego)
                        ++total;
                    break;

                case OBJ_ARMOUR:
                    if (get_armour_ego_type(item) == ego)
                        ++total;
                    break;

                default:
                    if (item.brand == ego)
                        ++total;
            }
        }
    }

    return total;
}

int player_equip_set::wearing(object_class_type obj_type, int sub_type,
                              bool count_plus, bool check_attunement) const
{
    int total = 0;

    for (const player_equip_entry& entry : items)
    {
        if (entry.slot == SLOT_UNUSED || entry.is_overflow || entry.melded
            || (check_attunement && !entry.attuned))
        {
            continue;
        }

        item_def& item = entry.get_item();
        if (item.base_type != obj_type || item.sub_type != sub_type)
            continue;

        if (count_plus)
            total += item.plus;
        else
            total += 1;
    }

    return total;
}

int player_equip_set::get_artprop(artefact_prop_type prop) const
{
    return artprop_cache[prop];
}

/**
 * Returns an array of exactly how many of each type of equipment slot the
 * player character has (including things like the Macabre Finger, if the player
 * is currently wearing it).
 *
 * This array can be used directly by player_equip_set
 */
FixedVector<int, NUM_EQUIP_SLOTS> get_total_player_equip_slots()
{
    FixedVector<int, NUM_EQUIP_SLOTS> slots;
    for (int i = 0; i < NUM_EQUIP_SLOTS; ++i)
        slots[i] = get_player_equip_slot_count(static_cast<equipment_slot>(i));

    return slots;
}

item_def& player_equip_entry::get_item() const
{
    ASSERT(slot != SLOT_UNUSED);

    return you.inv[item];
}

player_equip_entry::player_equip_entry(item_def& _item, equipment_slot _slot,
                                       bool _is_overflow) :
    slot(_slot), melded(false), attuned(false), is_overflow(_is_overflow)
{
    ASSERT(in_inventory(_item));
    item = _item.link;
}

player_equip_entry::player_equip_entry(int _item, equipment_slot _slot,
                                       bool _melded, bool _attuned,
                                       bool _is_overflow) :
    item(_item), slot(_slot), melded(_melded), attuned(_attuned), is_overflow(_is_overflow)
{}

bool slot_is_melded(equipment_slot slot)
{
    return get_form()->slot_is_blocked(slot)
                || you.fishtail && slot == SLOT_BOOTS;
}

/**
 * Returns whether the player is capable of wearing a given item.
 *
 * This does not consider cursed items occupying the relevant slots (or
 * two-handers blocking the shield slot) but *will* consider new slots gained by
 * whatever form the player is currently in).
 *
 * In general, get_player_equip_slot_count() should account for cases where no
 * item of a given slot can be equipped, while cases where only a subset of them
 * can should go through here.
 *
 * @param item              The item being checked.
 * @param include_form      Whether to veto items that would go into a slot that
 *                          is melded by our current form.
 * @param veto_reason[out]  If the player cannot use this item, and this is
 *                          non-null, it is set to the reason why they can't.
 *
 * @return True if the player is capable of theoretically wearing this item.
 */
bool can_equip_item(const item_def& item, bool include_form, string* veto_reason)
{
#define NO_EQUIP(x) {if (veto_reason) { *veto_reason = x; }; return 0;}

    if (!item_type_is_equipment(item.base_type))
        NO_EQUIP("That isn't an equippable item.")

    vector<equipment_slot> slots = get_all_item_slots(item);

    // For each base slot that this item strictly requires, check if the player
    // has at least one of the equivalent slots.
    for (equipment_slot slot : slots)
    {
        const vector<equipment_slot>& flex = get_alternate_slots(slot);
        bool found_slot = false;
        for (equipment_slot alt_slot : flex)
        {
            // If we don't have this slot, veto_reason will be set here.
            if (get_player_equip_slot_count(alt_slot, veto_reason))
            {
                if (include_form && slot_is_melded(alt_slot))
                {
                    // Note that this slot is blocked due to transformation, in
                    // the likely case that no other compatible slot exists.
                    if (veto_reason)
                        *veto_reason = "You can't equip that in your current form.";
                }
                else
                    found_slot = true;

                break;
            }
        }

        // Didn't find any suitable slot.
        if (!found_slot)
            return false;
    }

    // Now that we know that the player has access to a slot of an appropriate
    // type, is there some *other* reason they cannot wear this item?
    if (item.base_type == OBJ_ARMOUR)
    {
        const size_type player_size = you.body_size(PSIZE_TORSO, !include_form);
        const equipment_slot slot = get_armour_slot(static_cast<armour_type>(item.sub_type));
        if (slot == SLOT_BODY_ARMOUR || slot == SLOT_OFFHAND)
        {
            int bad_size = fit_armour_size(item, player_size);
            if (bad_size != 0)
            {
                NO_EQUIP(make_stringf("That is too %s for you to equip!",
                                            (bad_size > 0) ? "large" : "small"))
            }
        }

        if (is_hard_helmet(item))
        {
            if (player_size >= SIZE_LARGE)
                NO_EQUIP("This helmet is too small for your head.")
            else if (player_size <= SIZE_LITTLE)
                NO_EQUIP("This helmet is too large for your head.")
            else if (species::is_draconian(you.species))
                NO_EQUIP("You can't wear that with your reptilian head.")
            else if (you.species == SP_OCTOPODE)
                NO_EQUIP("Your can't wear that!")
            else if (you.has_mutation(MUT_HORNS))
                NO_EQUIP("You can't fit that over your horns.")
            else if (you.has_mutation(MUT_ANTENNAE))
                NO_EQUIP("You can't fit that over your antennae.")
            else if (you.has_mutation(MUT_BEAK))
                NO_EQUIP("You can't fit that over your beak.")
        }
    }
    else if (item.base_type == OBJ_WEAPONS)
    {
        const size_type bsize = you.body_size(PSIZE_TORSO, !include_form);
        if (is_weapon_too_large(item, bsize)
            && !you.has_mutation(MUT_QUADRUMANOUS))
        {
            NO_EQUIP("That's too large for you to wield.");
        }
    }

    return true;
#undef NO_EQUIP
}

void player_equip_set::update()
{
    unrand_active.reset();
    artprop_cache.init(0);

    artefact_properties_t artprops;
    for (const player_equip_entry& entry : items)
    {
        // Skip overflow items (but we must at least take a peek at melded ones,
        // in case we need to set unrand tracking flags).
        if (entry.is_overflow)
            continue;

        const item_def& item = entry.get_item();

        if (is_artefact(item))
        {
            if (is_unrandom_artefact(item))
            {
                unrand_equipped.set(item.unrand_idx - UNRAND_START);
                if (!entry.melded)
                    unrand_active.set(item.unrand_idx - UNRAND_START);
            }

            if (!entry.melded)
            {
                artefact_properties(item, artprops);

                for (int j = 0; j < (int)artprops.size(); ++j)
                    artprop_cache[j] += artprops[j];
            }
        }
    }

    if (you.active_talisman.defined() && is_artefact(you.active_talisman)
        && you.form == you.default_form)
    {
        artefact_properties(you.active_talisman, artprops);

        for (int j = 0; j < (int)artprops.size(); ++j)
            artprop_cache[j] += artprops[j];
    }

    for (int i = SLOT_UNUSED; i < NUM_EQUIP_SLOTS; ++i)
        num_slots[i] = get_player_equip_slot_count(static_cast<equipment_slot>(i));
}

/**
 * Attempts to find the most specific open slot that a given item could be
 * equipped in, possibly filling in a vector of items that could be swapped out
 * to make room for this.
 *
 * Assumes that the passed item is equippable by the player.
 *
 * @param item             The item we're trying to equip.
 *
 * @param[out] to_replace  If there is no appropriate empty slot, but there are
 *                         slots with items that could be removed to make room
 *                         for this item, the items in those slots will be
 *                         added to this vector.
 *
 *                         For items which occupy more than one slot at a time,
 *                         each vector within this vector represents a distinct
 *                         item (or set of possible items) which must be removed.
 *                         ie: if equipping a two-handed weapon, to_replace may
 *                         contain {{Current Weapon}, {Current Shield}} while
 *                         {{Ring 1, Ring 2}} might result from equpping a ring.
 *
 * @param ignore_curses    If true, treats cursed items as they were removable.
 *                         (Used for equipment preview, so that it can display
 *                         proper stats from swapping them out.)
 *
 * @return  The type of open slot this item could be equipped in. If none exists,
 *          will return SLOT_UNUSED instead.
 *
 *          Important: in cases of items that occupy more than one slot, this
 *          may return a specific slot even though items must still be removed
 *          to be able to equip this! Only a slot *and* an empty to_replace
 *          vector can be interpreted as 'no further action needed to equip
 *          this'. In cases it returns SLOT_UNUSED and there are items in
 *          to_replace, the item should go in the slot of the first one of those
 *          removed.
 */
equipment_slot player_equip_set::find_slot_to_equip_item(const item_def& item,
                                                         vector<vector<item_def*>>& to_replace,
                                                         bool ignore_curses) const
{
    vector<equipment_slot> slots = get_all_item_slots(item);

    FixedVector<int, NUM_EQUIP_SLOTS> slot_count = num_slots;
    equipment_slot ret = SLOT_UNUSED;
    for (size_t i = 0; i < slots.size(); ++i)
    {
        to_replace.emplace_back();
        equipment_slot slot = find_slot_to_equip_item(slots[i], to_replace.back(),
                                                      slot_count,
                                                      ignore_curses);

        // Save this result to return, if it is possible to fill other slots.
        if (i == 0)
            ret = slot;

        // Track that this slot was used (to handle the case of multi-slot
        // items competing for the same flex slot, such as poltergeists wearing
        // the Fungal Fisticloak).
        slot_count[slot] -= 1;

        if (slot == SLOT_UNUSED)
        {
            // If a slot is unavailable and there is nothing the player could
            // remove to change that fact, we must abort completely.
            if (to_replace.back().empty())
            {
                to_replace.clear();
                return SLOT_UNUSED;
            }
        }

        // If this slot has no replacement candidates (presumably because there
        // is actually room), remove this vector so as to not confuse the caller.
        if (to_replace.back().empty())
            to_replace.pop_back();
    }

    return ret;
}

equipment_slot player_equip_set::find_slot_to_equip_item(equipment_slot base_slot,
                                                         vector<item_def*>& to_replace,
                                                         FixedVector<int, NUM_EQUIP_SLOTS>& slot_count,
                                                         bool ignore_curses) const
{
    const vector<equipment_slot>& slots = get_alternate_slots(base_slot);
    for (equipment_slot slot : slots)
    {
        // Skip slots the player doesn't have at all.
        if (slot_count[slot] == 0)
            continue;

        // Otherwise, iterate all slots of this type and see if one is free.
        int count = 0;
        for (const player_equip_entry& entry : items)
        {
            if (entry.slot == slot)
                ++count;
        }

        // The player has more slots than they're currently using, so this one
        // should be fine.
        if (count < slot_count[slot])
            return slot;
    }

    // At this point, we've checked all slots without finding empty ones, so
    // start gathering up items that are taking up those slots, if they exist.
    item_def* cursed_item = nullptr;
    bool found_item = false;
    for (equipment_slot slot : slots)
    {
        for (const player_equip_entry& entry : items)
        {
            if (entry.slot == slot && !entry.melded)
            {
                item_def& item = entry.get_item();

                // Note any cursed item we find, in case *all* the items we find
                // are cursed and we want to report this.
                if (!ignore_curses && item.cursed())
                    cursed_item = &item;
                // Otherwise, add this as a candidate to replace.
                else
                {
                    // If this is an overflow entry, make sure we didn't add
                    // the same item earlier (to prevent asking the player to
                    // choose between [item A] and [item A])
                    bool found_identical = false;
                    if (entry.is_overflow)
                    {
                        for (item_def* check : to_replace)
                            if (check == &item)
                            {
                                found_identical = true;
                                break;
                            }
                    }

                    if (!found_identical)
                    {
                        found_item = true;
                        to_replace.push_back(&item);
                    }
                }
            }
        }
    }

    if (!found_item && cursed_item)
        mprf(MSGCH_PROMPT, "%s is stuck to your body!", cursed_item->name(DESC_YOUR).c_str());

    // We didn't find a useable slot, but may have added removal possibilities
    // to to_replace
    return SLOT_UNUSED;
}

/**
 * Tests whether removing a given item will reduce the player's available
 * equipment slots in a way that requires them to also remove other items.
 *
 * @param item             The item to remove.
 * @param to_replace[out]  If removing the item will cause a slot type to
 *                         overflow, a vector of all removable items in that
 *                         slot type will be added to this vector.
 * @param cursed_okay      True if cursed items are considered okay to remove
 *                         (generally because this is called by someone looking
 *                         to meld rather than remove them).
 *
 * @return How many other items need to be removed before the player could
 *         remove this item. If this is non-zero and to_replace is still empty,
 *         that means that there are *no* valid candidates to remove, despite
 *         needing to do so (almost certainly because every slot of the needed
 *         type contains a cursed item.)
  */
int player_equip_set::needs_chain_removal(const item_def& item,
                                          vector<item_def*>& to_replace,
                                          bool cursed_okay)
{
    if (!item_gives_equip_slots(item))
        return 0;

    // XXX: This likely doesn't properly handle the case where an item gives
    //      slots of *multiple* types, but no such items yet exist. Consider
    //      changing this code if one is ever added.
    unwind_var<player_equip_set> unwind_eq(you.equipment);
    remove(item);

    for (int i = SLOT_UNUSED; i < NUM_EQUIP_SLOTS; ++i)
    {
        int new_num_slots = get_player_equip_slot_count(static_cast<equipment_slot>(i));
        if (new_num_slots < num_slots[i])
        {
            int count = 0;
            for (const player_equip_entry& entry : items)
            {
                if (entry.slot != i)
                    continue;

                ++count;

                if (cursed_okay || !entry.get_item().cursed())
                    to_replace.push_back(&entry.get_item());
            }

            return count - new_num_slots;
        }
    }

    return 0;
}

// Sorter function to be used by get_forced_removal_list()
// ie: prefer to remove items without direct removal penalties, when possible.
static bool _forced_removal_goodness(player_equip_entry* entry1, player_equip_entry* entry2)
{
    const item_def& item1 = entry1->get_item();
    const item_def& item2 = entry2->get_item();

    if (item1.cursed())
        return false;
    else if (item2.cursed())
        return true;
    else if (is_artefact(item1) && artefact_property(item1, ARTP_FRAGILE))
        return false;
    else if (is_artefact(item2) && artefact_property(item2, ARTP_FRAGILE))
        return true;
    else if (is_artefact(item1) && (artefact_property(item1, ARTP_CONTAM)
                                    || artefact_property(item1, ARTP_DRAIN)))
    {
        return false;
    }
    else if (is_artefact(item2) && (artefact_property(item2, ARTP_CONTAM)
                                    || artefact_property(item2, ARTP_DRAIN)))
    {
        return true;
    }

    return true;
}

/**
 * Returns a list of all items that might need to be forced off the player due
 * to a decrease their number of equipment slots (or hat/helmet eligability) for
 * reasons outside their control.
 *
 * If the player has multiple items in a given slot that they've lost, and they
 * do not need to remove all of them, this function will prefer non-cursed items
 * and then items without things like {Contam} or {Fragile}. Otherwise, it will
 * pick the first it sees.
 *
 * @param force_full_check   If true, checks all slots for items that shouldn't
 *                           be able to fit, rather than only looking at slots
 *                           whose capacity has changed since the last call to
 *                           ::update()
 *
 * @return A vector of references to all items that must be removed for the
 *         player's current state to become valid again.
 */
vector<item_def*> player_equip_set::get_forced_removal_list(bool force_full_check)
{
    vector<item_def*> to_remove;

    // Next, calculate our new slot count and compare to see if we need to
    // lose any items for that reason.
    FixedVector<int, NUM_EQUIP_SLOTS> new_num_slots;
    for (int i = SLOT_UNUSED; i < NUM_EQUIP_SLOTS; ++i)
        new_num_slots[i] = get_player_equip_slot_count(static_cast<equipment_slot>(i));

    for (int i = SLOT_UNUSED; i < NUM_EQUIP_SLOTS; ++i)
    {
        if (force_full_check || new_num_slots[i] < num_slots[i])
        {
            vector<player_equip_entry*> maybe_remove;
            for (player_equip_entry& entry : items)
            {
                if (entry.slot == i)
                    maybe_remove.push_back(&entry);
            }

            // Simple case: we don't have to remove anything.
            if ((int)maybe_remove.size() <= new_num_slots[i])
                continue;
            // Another simple case: we have to remove everything.
            if (new_num_slots[i] == 0)
                for (player_equip_entry* entry : maybe_remove)
                    to_remove.push_back(&entry->get_item());
            // We have more items than we need to remove, so try to pick the
            // 'least bad' in a very coarse way.
            else
            {
                const int num_to_remove = maybe_remove.size() - new_num_slots[i];
                sort(maybe_remove.begin(), maybe_remove.end(), _forced_removal_goodness);
                for (int j = 0; j < num_to_remove; ++j)
                    to_remove.push_back(&maybe_remove[j]->get_item());
            }
        }
    }

    // Next, see if any items have become unwearable for non-slot reasons
    // (eg: horns 1 blocking a helmet). These *must* all be removed.
    for (const player_equip_entry& entry : items)
    {
        if (!can_equip_item(entry.get_item()))
        {
            // But check that this isn't a duplicate one removed for slot reasons!
            item_def* item = &entry.get_item();
            if (find(to_remove.begin(), to_remove.end(), item) == to_remove.end())
                to_remove.push_back(&entry.get_item());
        }
    }

    return to_remove;
}

void player_equip_set::add(item_def& item, equipment_slot slot)
{
    ASSERT(slot != SLOT_UNUSED);

    items.emplace_back(item, slot);

    // Any slots past the first must be overflow slots, so place an overflow
    // entry for this item in all of them.
    vector<equipment_slot> slots = get_all_item_slots(item);
    if (slots.size() > 1)
    {
        for (size_t i = 1; i < slots.size(); ++i)
        {
            // If there's multiple choices of overflow, make sure we overflow
            // into a slot the player actually has. (Mostly this is for Coglins
            // wielding two-handers, to make sure it goes in their unique
            // offhand slot.)
            const vector<equipment_slot>& alt_slots = get_alternate_slots(slots[i]);
            for (equipment_slot _slot : alt_slots)
            {
                if (num_slots[_slot] == 0)
                    continue;
                else
                {
                    items.emplace_back(item, _slot, true);
                    break;
                }
            }
        }
    }

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->world_reacts_func)
            ++do_unrand_reacts;
        if (entry->death_effects)
            ++do_unrand_death_effects;
    }
}

void player_equip_set::remove(const item_def& item)
{
    for (int i = (int)items.size() - 1; i >= 0; --i)
    {
        if (items[i].item == item.link)
        {
            items[i] = items[items.size() - 1];
            items.pop_back();
        }
    }

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->world_reacts_func)
            --do_unrand_reacts;
        if (entry->death_effects)
            --do_unrand_death_effects;

        unrand_equipped.set(item.unrand_idx - UNRAND_START, false);
        unrand_active.set(item.unrand_idx - UNRAND_START, false);
    }
}

// Returns the primary slot an item is equipped in (or SLOT_UNUSED if it is not
// equipped at all.)
equipment_slot player_equip_set::find_equipped_slot(const item_def& item) const
{
    for (const player_equip_entry& entry : items)
    {
        if (entry.item == item.link && !entry.is_overflow)
            return entry.slot;
    }

    return SLOT_UNUSED;
}

// Finds a slot occupied by old_item that is a valid place to equip new_item.
// (This is used for overflow items, which may be blocking another piece of gear
// without their own primary slot being suitable for that gear.)
equipment_slot player_equip_set::find_compatible_occupied_slot(const item_def& old_item,
                                                               const item_def& new_item) const
{
    const vector<equipment_slot>& good_slots = get_alternate_slots(get_all_item_slots(new_item)[0]);
    for (const player_equip_entry& entry : items)
        if (entry.item == old_item.link)
            for (equipment_slot slot : good_slots)
                if (entry.slot == slot)
                    return slot;

    return SLOT_UNUSED;
}

bool player_equip_set::has_compatible_slot(equipment_slot slot, bool include_form) const
{
    // Check the exact slot first.
    if (num_slots[slot] > 0 && (!include_form || !slot_is_melded(slot)))
        return true;

    // Now look for compatible alternative slots.
    const vector<equipment_slot>& alt_slots = get_alternate_slots(slot);
    for (const auto& alt_slot : alt_slots)
        if (num_slots[alt_slot] > 0 && (!include_form || !slot_is_melded(alt_slot)))
            return true;

    return false;
}

bool player_equip_set::is_melded(const item_def& item)
{
    for (const player_equip_entry& entry : items)
    {
        if (entry.item == item.link)
            return entry.melded;
    }

    return false;
}

/**
 * Melds a specified set of equipment and then calls unequip_item on each one
 * of them afterward.
 *
 * @param slots        A set of bitflags representing which slots to meld. (See
 *                     form_entry::blocked_slots)
 * @param skip_effects If true, no message will be printed about doing any of
 *                     this, and no unequip effects will be processed.
 */
void player_equip_set::meld_equipment(int slots, bool skip_effects)
{
    vector<item_def*> was_melded;
    for (player_equip_entry& entry : items)
    {
        if ((1 << entry.slot) & slots)
        {
            item_def* item = &entry.get_item();

            if (!entry.melded)
                was_melded.push_back(item);

            entry.melded = true;

            // If this is an item occupying multiple slots, find all the other
            // entries and meld them as well.
            if (get_all_item_slots(*item).size() > 1)
            {
                for (player_equip_entry& overflow : items)
                {
                    if (overflow.item == entry.item)
                        overflow.melded = true;
                }
            }
        }
    }

    // If melding these items will remove slots that contain other items, meld
    // those too (to keep from constantly popping them off during certain
    // transformations).
    int num_melded = was_melded.size();
    handle_chain_removal(was_melded, false);
    if ((int)was_melded.size() > num_melded)
    {
        for (size_t i = num_melded; i < was_melded.size(); ++i)
        {
            for (player_equip_entry& entry : items)
            {
                if (entry.item == was_melded[i]->link)
                {
                    entry.melded = true;
                    if (entry.is_overflow)
                    {
                        for (player_equip_entry& overflow : items)
                        {
                            if (overflow.item == entry.item)
                                overflow.melded = true;
                        }
                    }
                }
            }
        }
    }

    if (was_melded.empty())
        return;

    if (skip_effects)
        return;

    // Print a message.
    vector<string> meld_msg;
    for (item_def* meld_item : was_melded)
        meld_msg.emplace_back(meld_item->name(DESC_PLAIN));

    mprf("Your %s meld%s into your body.",
            comma_separated_line(meld_msg.begin(), meld_msg.end()).c_str(),
            meld_msg.size() > 1 ? "" : "s");

    update();

    // Now, simultaneously do unequip effects for all melded items.
    for (item_def* meld_item : was_melded)
        unequip_effect(meld_item->link, true, true);
}

/**
 * Unmelds equipment in a single slot and does associated bookeeping.
 *
 * @param slot           The slot type to unmeld.
 * @param skip_effects   If true, no message will be printed about doing any of
 *                       this, and no equip effects will be processed.
 */
void player_equip_set::unmeld_slot(equipment_slot slot, bool skip_effects)
{
    vector<item_def*> was_unmelded;
    for (player_equip_entry& entry : items)
    {
        if (entry.slot == slot)
        {
            if (entry.melded)
            {
                if (entry.is_overflow)
                {
                    // If this item takes up multiple slots, verify that none of
                    // them should be melded by our current form before
                    // unmelding any of them.
                    bool keep_melded = false;
                    for (player_equip_entry& overflow : items)
                    {
                        if (overflow.item != entry.item)
                            continue;

                        if (get_form()->slot_is_blocked(overflow.slot))
                        {
                            keep_melded = true;
                            break;
                        }
                    }

                    // If we should actually unmeld this item, make sure to
                    // unmeld every slot of it.
                    if (!keep_melded)
                    {
                        for (player_equip_entry& overflow : items)
                        {
                            if (overflow.item != entry.item)
                                continue;

                            overflow.melded = false;
                        }
                        was_unmelded.push_back(&entry.get_item());
                    }
                }
                else
                {
                    entry.melded = false;
                    was_unmelded.push_back(&entry.get_item());
                }
            }
        }
    }

    handle_unmelding(was_unmelded, skip_effects);
}

/**
 * Unmelds all equipment and does associated bookeeping.
 *
 * @param skip_effects   If true, no message will be printed about doing any of
 *                       this, and no equip effects will be processed.
 */
void player_equip_set::unmeld_all_equipment(bool skip_effects)
{
    vector<item_def*> was_unmelded;
    for (player_equip_entry& entry : items)
    {
        if (entry.melded)
        {
            // If the player is untransforming from a form where fishtail is
            // active, don't unmeld their boots.
            if (you.fishtail)
            {
                if (entry.slot == SLOT_BOOTS)
                    continue;

                // Also skip items filling the boots slot and some other slot.
                vector<equipment_slot> slots = get_all_item_slots(entry.get_item());
                if (find(slots.begin(), slots.end(), SLOT_BOOTS) != slots.end())
                    continue;
            }

            entry.melded = false;
            if (!entry.is_overflow)
                was_unmelded.push_back(&entry.get_item());
        }
    }

    handle_unmelding(was_unmelded, skip_effects);
}

void player_equip_set::handle_unmelding(vector<item_def*>& to_unmeld, bool skip_effects)
{
    if (to_unmeld.empty())
        return;

    // Print a message.
    if (!skip_effects)
    {
        vector<string> unmeld_msg;
        for (item_def* unmeld_item : to_unmeld)
            unmeld_msg.emplace_back(unmeld_item->name(DESC_PLAIN));

        mprf("Your %s unmeld%s from your body.",
                comma_separated_line(unmeld_msg.begin(), unmeld_msg.end()).c_str(),
                unmeld_msg.size() > 1 ? "" : "s");
    }

    update();

    // Now, simultaneously do unequip effects for all melded items.
    if (!skip_effects)
    {
        for (item_def* meld_item : to_unmeld)
            equip_effect(meld_item->link, true, true);
    }
}

/**
 * Finds all items the player has equipped which could fit into a given slot
 * (even if they may not strictly be in that slot - but a flexible slot instead.)
 *
 * @param slot   The slot to scan for. This may be a 'synthetic' slot like
 *               SLOT_ALL_JEWELLERY (which contains both SLOT_RING and SLOT_AMULET).
 * @param include_melded  Whether to also include equipped items which are
 *                        currently melded (defaults to false).
 * @param attuned_only    Whether to only return items which are attuned (ie:
 *                        regen-granting items which have been worn at full HP)
 *
 * @return A vector containing all non-overflow items equipped in a slot
 *         compatible with the specified slot.
 */
vector<item_def*> player_equip_set::get_slot_items(equipment_slot slot,
                                                   bool include_melded,
                                                   bool attuned_only) const
{
    vector<item_def*> found;
    const vector<equipment_slot>& slots = get_alternate_slots(slot);

    // Calculate how many of a given item we could possibly find, so we can
    // abort early.
    int max = 0;
    for (equipment_slot _slot : slots)
        max += num_slots[_slot];

    if (max == 0)
        return found;

    for (player_equip_entry entry : items)
    {
        for (equipment_slot _slot : slots)
        {
            if (entry.slot == _slot && !entry.is_overflow
                && (include_melded || !entry.melded)
                && (!attuned_only || entry.attuned))
            {
                item_def* item = &entry.get_item();

                // While these slots are flexible in what they can contain (for
                // Coglins), if we asked for weapons, we should only get weapons
                // back. Same for shields/orbs.
                if (slot == SLOT_WEAPON && !is_weapon(*item)
                    || slot == SLOT_OFFHAND && item->base_type != OBJ_ARMOUR)
                {
                    continue;
                }

                found.push_back(item);

                // Abort early if we found as many as possible.
                if ((int)found.size() == max)
                    return found;
            }
        }
    }

    return found;
}

// Note: does *not* check 'alternative' slots and *does* return overflow entries,
// unlike get_slot_items(), since this used for chain removal checks.
vector<player_equip_entry> player_equip_set::get_slot_entries(equipment_slot slot) const
{
    vector<player_equip_entry> found;
    for (player_equip_entry entry : items)
    {
        if (entry.slot == slot)
            found.push_back(entry);
    }

    return found;
}

// Similar to get_slot_items(), but directly returns the first item found.
// (Used as a shorthand for things like actor::body_armour(), where the behavior
// of more than one item is unsupported anyway.)
item_def* player_equip_set::get_first_slot_item(equipment_slot slot, bool include_melded) const
{
    const vector<equipment_slot>& slots = get_alternate_slots(slot);
    for (const player_equip_entry& entry : items)
    {
        for (equipment_slot _slot : slots)
            if (entry.slot == _slot && !entry.is_overflow
                && (include_melded || !entry.melded))
            {
                item_def* item = &entry.get_item();

                // While these slots are flexible in what they can contain (for
                // Coglins), if we asked for weapons, we should only get weapons
                // back. Same for shields/orbs.
                if (slot == SLOT_WEAPON && !is_weapon(*item)
                    || slot == SLOT_OFFHAND && item->base_type != OBJ_ARMOUR)
                {
                    continue;
                }

                return item;
            }
    }

    return nullptr;
}

player_equip_entry& player_equip_set::get_entry_for(const item_def& item)
{
    ASSERT(in_inventory(item));

    for (player_equip_entry& entry : items)
    {
        if (entry.is_overflow)
            continue;

        if (entry.item == item.link)
            return entry;
    }

    die("Attempting to access equip entry for unequipped item %s",
            item.name(DESC_PLAIN).c_str());
}

/**
 * Checks whether all of a given slot type is filled with unmelded items.
 * (This is largely used to check if claws are covered by gloves, talons
 * covered by boots, etc.)
 */
bool player_equip_set::slot_is_fully_covered(equipment_slot slot) const
{
    if (num_slots[slot] == 0 || slot_is_melded(slot))
        return false;

    return (int)get_slot_entries(slot).size() == num_slots[slot];
}

/**
 * Recalculate the player's max hp and set the current hp based on the %change
 * of max hp. This has resulted from our having equipped an artefact that
 * changes max hp.
 */
static void _calc_hp_artefact()
{
    calc_hp();
    if (you.hp_max <= 0) // Borgnjor's abusers...
        ouch(0, KILLED_BY_DRAINING);
}

static void _flight_equip()
{
    if (you.airborne()) // already aloft
        mpr("You feel rather light.");
    else
        float_player();
    you.attribute[ATTR_PERM_FLIGHT] = 1;
}

// Attempts to immediately put an item in the first compatable slot, without
// prompts, messages, or equip effects. (Used by new game equipment setup and
// equipment effect previews.)
void autoequip_item(item_def& item)
{
    ASSERT(in_inventory(item));

    vector<vector<item_def*>> dummy;
    equipment_slot slot = you.equipment.find_slot_to_equip_item(item, dummy);
    if (slot != SLOT_UNUSED)
        equip_item(slot, item.link, false, true);
}

// Equip a given item in a given slot, update our equipment info, and possible
// run on-wield effects.
void equip_item(equipment_slot slot, int item_slot, bool msg, bool skip_effects)
{
    ASSERT_RANGE(slot, SLOT_WEAPON, NUM_EQUIP_SLOTS);
    ASSERT_RANGE(item_slot, 0, ENDOFPACK);

    item_def& item = you.inv[item_slot];

    const unsigned int old_talents = your_talents(false).size();

#ifdef USE_SOUND
    if (is_weapon(item))
        parse_sound(WIELD_WEAPON_SOUND);
    else if (item.base_type == OBJ_ARMOUR)
        parse_sound(EQUIP_ARMOUR_SOUND);
    else if (item.base_type == OBJ_JEWELLERY)
        parse_sound(WEAR_JEWELLERY_SOUND);
#endif

    you.equipment.add(item, slot);
    you.equipment.update();

    if (!skip_effects)
        equip_effect(item_slot, false, msg);

    you.gear_change = true;
    update_can_currently_train();

    if (is_weapon(item))
    {
        you.wield_change  = true;
        you.received_weapon_warning = false;
        quiver::on_weapon_changed();
    }

#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
    }
#endif

    check_item_hint(item, old_talents);
}

// Unequip and equipped item (possibly melded).
bool unequip_item(item_def& item, bool msg, bool skip_effects)
{
#ifdef USE_TILE_LOCAL
    const unsigned int old_talents = your_talents(false).size();
#endif

#ifdef USE_SOUND
    parse_sound(item.base_type == OBJ_JEWELLERY
                                        ? REMOVE_JEWELLERY_SOUND :
                    is_weapon(item) ? WIELD_NOTHING_SOUND
                                    : DEQUIP_ARMOUR_SOUND);
#endif

    if (is_weapon(item) && you.has_mutation(MUT_SLOW_WIELD))
        say_farewell_to_weapon(item);

    const int item_slot = item.link;
    you.equipment.remove(item);
    you.equipment.update();

    if (!skip_effects)
        unequip_effect(item_slot, false, msg);

    ash_check_bondage();
    you.last_unequip = item_slot;

#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
    }
#endif

    you.gear_change = true;

    if (is_weapon(item))
    {
        you.wield_change  = true;
        quiver::on_weapon_changed();
    }

    return true;
}

static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld);
static void _unequip_weapon_effect(item_def& item, bool showMsgs, bool meld);
static void _equip_armour_effect(item_def& arm, bool unmeld);
static void _unequip_armour_effect(item_def& item, bool meld);
static void _equip_jewellery_effect(item_def &item, bool unmeld);
static void _unequip_jewellery_effect(item_def &item, bool meld);
static void _equip_use_warning(const item_def& item);
static void _handle_regen_item_equip(const item_def& item);

void equip_effect(int item_slot, bool unmeld, bool msg)
{
    item_def& item = you.inv[item_slot];

    if (msg)
        _equip_use_warning(item);

    const interrupt_block block_unmeld_interrupts(unmeld);

    identify_item(item);

    if (is_artefact(item))
        equip_artefact_effect(item, &msg, unmeld);

    if (is_weapon(item))
        _equip_weapon_effect(item, msg, unmeld);
    else if (item.base_type == OBJ_ARMOUR)
        _equip_armour_effect(item, unmeld);
    else if (item.base_type == OBJ_JEWELLERY)
        _equip_jewellery_effect(item, unmeld);

    if (!unmeld)
        _handle_regen_item_equip(item);

    if (!unmeld && item_gives_equip_slots(item))
        ash_check_bondage();
}

void unequip_effect(int item_slot, bool meld, bool msg)
{
    item_def& item = you.inv[item_slot];

    const interrupt_block block_meld_interrupts(meld);

   if (is_artefact(item))
        unequip_artefact_effect(item, &msg, meld);

    if (is_weapon(item))
        _unequip_weapon_effect(item, msg, meld);
    else if (item.base_type == OBJ_ARMOUR)
        _unequip_armour_effect(item, meld);
    else if (item.base_type == OBJ_JEWELLERY)
        _unequip_jewellery_effect(item, meld);

    // Cursed items should always be destroyed on unequip.
    if (item.cursed() && !meld)
        destroy_item(item);
}

///////////////////////////////////////////////////////////
// Actual equip and unequip effect implementation below
//

void equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld)
{
    ASSERT(is_artefact(item));

    // Call unrandart equip function first, so that it can modify the
    // artefact's properties before they're applied.
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->equip_func)
            entry->equip_func(&item, show_msgs, unmeld);
    }

    const bool msg          = !show_msgs || *show_msgs;

    artefact_properties_t  proprt;
    artefact_properties(item, proprt);

    if (proprt[ARTP_AC] || proprt[ARTP_SHIELDING])
        you.redraw_armour_class = true;

    if (proprt[ARTP_EVASION])
        you.redraw_evasion = true;

    if (proprt[ARTP_SEE_INVISIBLE])
        autotoggle_autopickup(false);

    // Modify ability scores.
    notify_stat_change(STAT_STR, proprt[ARTP_STRENGTH],
                       !(msg && proprt[ARTP_STRENGTH] && !unmeld));
    notify_stat_change(STAT_INT, proprt[ARTP_INTELLIGENCE],
                       !(msg && proprt[ARTP_INTELLIGENCE] && !unmeld));
    notify_stat_change(STAT_DEX, proprt[ARTP_DEXTERITY],
                       !(msg && proprt[ARTP_DEXTERITY] && !unmeld));

    if (proprt[ARTP_FLY])
        _flight_equip();

    if (proprt[ARTP_CONTAM] && msg && !unmeld)
        mpr("You feel a build-up of mutagenic energy.");

    if (proprt[ARTP_RAMPAGING] && msg && !unmeld
        && !you.has_mutation(MUT_ROLLPAGE))
    {
        mpr("You feel ready to rampage towards enemies.");
    }

    if (proprt[ARTP_ARCHMAGI] && msg && !unmeld)
    {
        if (!you.skill(SK_SPELLCASTING))
            mpr("You feel strangely lacking in power.");
        else
            mpr("You feel powerful.");
    }

    if (proprt[ARTP_HP])
        _calc_hp_artefact();

    // Let's try this here instead of up there.
    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();
}

void unequip_artefact_effect(item_def &item,  bool *show_msgs, bool meld)
{
    ASSERT(is_artefact(item));

    artefact_properties_t proprt;
    artefact_properties(item, proprt);
    const bool msg = !show_msgs || *show_msgs;

    if (proprt[ARTP_AC] || proprt[ARTP_SHIELDING])
        you.redraw_armour_class = true;

    if (proprt[ARTP_EVASION])
        you.redraw_evasion = true;

    if (proprt[ARTP_HP])
        _calc_hp_artefact();

    if (proprt[ARTP_MAGICAL_POWER] && !you.has_mutation(MUT_HP_CASTING))
    {
        const bool gives_mp = proprt[ARTP_MAGICAL_POWER] > 0;
        if (msg)
            canned_msg(gives_mp ? MSG_MANA_DECREASE : MSG_MANA_INCREASE);
        if (gives_mp)
            pay_mp(proprt[ARTP_MAGICAL_POWER]);
        calc_mp();
    }

    notify_stat_change(STAT_STR, -proprt[ARTP_STRENGTH],     true);
    notify_stat_change(STAT_INT, -proprt[ARTP_INTELLIGENCE], true);
    notify_stat_change(STAT_DEX, -proprt[ARTP_DEXTERITY],    true);

    if (proprt[ARTP_FLY] != 0 && !meld)
        land_player();

    if (proprt[ARTP_CONTAM] && !meld)
    {
        mpr("Mutagenic energies flood into your body!");
        contaminate_player(7000, true);
    }

    if (proprt[ARTP_RAMPAGING] && msg && !meld
        && !you.rampaging())
    {
        mpr("You no longer feel able to rampage towards enemies.");
    }

    if (proprt[ARTP_ARCHMAGI] && msg && !meld)
        mpr("You feel strangely numb.");

    if (proprt[ARTP_DRAIN] && !meld)
        drain_player(150, true, true);

    if (proprt[ARTP_SEE_INVISIBLE])
        _mark_unseen_monsters();

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->unequip_func)
            entry->unequip_func(&item, show_msgs);
    }

    if (artefact_property(item, ARTP_FRAGILE) && !meld)
    {
        mprf("%s crumbles to dust!", item.name(DESC_THE).c_str());
        dec_inv_item_quantity(item.link, 1);

        // Hide unwield messages for weapons that have already been destroyed.
        if (item.base_type == OBJ_WEAPONS)
            *show_msgs = false;
    }
}

static void _equip_use_warning(const item_def& item)
{
    if (is_holy_item(item) && you_worship(GOD_YREDELEMNUL))
        mpr("You really shouldn't be using a holy item like this.");
    else if (is_evil_item(item) && is_good_god(you.religion))
        mpr("You really shouldn't be using an evil item like this.");
    else if (is_unclean_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using an unclean item like this.");
    else if (is_chaotic_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using a chaotic item like this.");
    else if (is_hasty_item(item) && you_worship(GOD_CHEIBRIADOS))
        mpr("You really shouldn't be using a hasty item like this.");
    else if (is_wizardly_item(item) && you_worship(GOD_TROG))
        mpr("You really shouldn't be using a wizardly item like this.");
}

// Provide a function for handling initial wielding of 'special' weapons
static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld)
{
    you.wield_change = true;
    quiver::on_weapon_changed();

    // message first
    int special = get_weapon_brand(item);
    if (showMsgs && item.base_type != OBJ_STAVES)
    {
        const string item_name = item.name(DESC_YOUR);
        switch (special)
        {
        case SPWPN_FLAMING:
            mprf("%s bursts into flame!", item_name.c_str());
            break;

        case SPWPN_FREEZING:
            mprf("%s %s", item_name.c_str(),
                is_range_weapon(item) ?
                    "is covered in frost." :
                    "glows with a cold blue light!");
            break;

        case SPWPN_HOLY_WRATH:
            if (you.undead_or_demonic())
            {
                mprf("%s sits dull and lifeless in your grasp.",
                        item_name.c_str());
            }
            else
            {
                mprf("%s softly glows with a divine radiance!",
                        item_name.c_str());
            }
            break;

        case SPWPN_FOUL_FLAME:
            if (you.is_holy())
            {
                mprf("%s sits dull and lifeless in your grasp.",
                        item_name.c_str());
            }
            else
            {
                mprf("%s glows horrifically with a foul blackness!",
                        item_name.c_str());
            }
            break;

        case SPWPN_ELECTROCUTION:
            if (!silenced(you.pos()))
            {
                mprf(MSGCH_SOUND,
                        "You hear the crackle of electricity.");
            }
            else
                mpr("You see sparks fly.");
            break;

        case SPWPN_VENOM:
            mprf("%s begins to drip with poison!", item_name.c_str());
            break;

        case SPWPN_PROTECTION:
            mprf("%s hums with potential!", item_name.c_str());
            break;

        case SPWPN_DRAINING:
            mpr("You sense an unholy aura.");
            break;

        case SPWPN_SPEED:
            mpr(you.hands_act("tingle", "!"));
            break;

        case SPWPN_VAMPIRISM:
            mpr("You feel a sense of dread.");
            break;

        case SPWPN_PAIN:
        {
            const string your_arm = you.arm_name(false);
            if (you_worship(GOD_TROG))
            {
                mprf(MSGCH_GOD, "Trog suppresses %s necromantic effect.",
                        apostrophise(item_name).c_str());
            }
            else if (you.skill(SK_NECROMANCY) == 0)
                mpr("You have a feeling of ineptitude.");
            else if (you.skill(SK_NECROMANCY) <= 6)
            {
                mprf("Pain shudders through your %s!",
                        your_arm.c_str());
            }
            else
            {
                mprf("A searing pain shoots up your %s!",
                        your_arm.c_str());
            }
            break;
        }

        case SPWPN_CHAOS:
            mprf("%s is briefly surrounded by a scintillating aura of "
                    "random colours.", item_name.c_str());
            break;

        case SPWPN_PENETRATION:
        {
            // FIXME: make hands_act take a pre-verb adverb so we can
            // use it here.
            bool plural = true;
            string hand = you.hand_name(true, &plural);

            mprf("Your %s briefly %s through it before you manage "
                    "to get a firm grip on it.",
                    hand.c_str(), conjugate_verb("pass", plural).c_str());
            break;
        }

        case SPWPN_REAPING:
            mprf("%s is briefly surrounded by shifting shadows.",
                    item_name.c_str());
            break;

        case SPWPN_ANTIMAGIC:
            if (you.has_mutation(MUT_HP_CASTING))
                mpr("You feel a force failing to suppress your magic.");
            else
            {
                // Even if your maxmp is 0.
                mpr("You feel magic leave you.");
            }
            break;

        case SPWPN_DISTORTION:
            mpr("Space warps around you for a moment!");
            break;

        case SPWPN_ACID:
            mprf("%s begins to ooze corrosive slime!", item_name.c_str());
            break;

        case SPWPN_SPECTRAL:
            mprf("You feel a bond with %s.", item_name.c_str());
            break;

        default:
            break;
        }
    }

    if (special == SPWPN_ANTIMAGIC)
        calc_mp();

    if (!unmeld)
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _unequip_weapon_effect(item_def& item, bool showMsgs, bool meld)
{
    you.wield_change = true;
    quiver::on_weapon_changed();

    if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand(item);

        if (brand != SPWPN_NORMAL)
        {
            const string msg = item.name(DESC_YOUR);

            switch (brand)
            {
            case SPWPN_FLAMING:
                if (showMsgs)
                    mprf("%s stops flaming.", msg.c_str());
                break;

            case SPWPN_HOLY_WRATH:
                if (showMsgs && !you.undead_or_demonic())
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_FOUL_FLAME:
                if (showMsgs && !you.is_holy())
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_FREEZING:
                if (showMsgs)
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_ELECTROCUTION:
                if (showMsgs)
                    mprf("%s stops crackling.", msg.c_str());
                break;

            case SPWPN_VENOM:
                if (showMsgs)
                    mprf("%s stops dripping with poison.", msg.c_str());
                break;

            case SPWPN_PROTECTION:
                if (showMsgs)
                    mprf("%s goes still.", msg.c_str());
                if (you.duration[DUR_SPWPN_PROTECTION])
                {
                    you.duration[DUR_SPWPN_PROTECTION] = 0;
                    you.redraw_armour_class = true;
                }
                break;

            case SPWPN_VAMPIRISM:
                if (showMsgs)
                    mpr("You feel the dreadful sensation subside.");
                break;

            case SPWPN_DISTORTION:
                if (!meld)
                    unwield_distortion();

                break;

            case SPWPN_ANTIMAGIC:
                calc_mp();
                if (!you.has_mutation(MUT_HP_CASTING))
                    mpr("You feel magic returning to you.");
                break;

            case SPWPN_SPECTRAL:
                {
                    monster *spectral_weapon = find_spectral_weapon(item);
                    if (spectral_weapon)
                        end_spectral_weapon(spectral_weapon, false, false);
                }
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect when reading brand weapon in read() in item-use.cc

            case SPWPN_ACID:
                mprf("%s stops oozing corrosive slime.", msg.c_str());
                break;
            }
        }
    }
}

static void _spirit_shield_message(bool unmeld)
{
    if (!unmeld && you.spirit_shield() < 2 && !you.has_mutation(MUT_HP_CASTING))
    {
        mpr("You feel your power drawn to a protective spirit.");
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DEEP_DWARF)
        {
            drain_mp(you.magic_points, true);
            mpr("Now linked to your health, your magic stops regenerating.");
        }
#endif
    }
    else if (!unmeld && (you.get_mutation_level(MUT_MANA_SHIELD)
                         || you.has_mutation(MUT_HP_CASTING)))
    {
        mpr("You feel the presence of a powerless spirit.");
    }
    else if (!you.get_mutation_level(MUT_MANA_SHIELD))
        mpr("You feel spirits watching over you.");
}

static void _zonguldrok_comment_on_hat(const item_def& hat)
{
    // Make sure this is the hat actually *on* Zonguldrok (which for now we
    // consider to be 'your last one'. It's possible in the future, this may
    // become untrue.)
    vector<item_def*> hats = you.equipment.get_slot_items(SLOT_HELMET);
    if ((int)hats.size() != you.equipment.num_slots[SLOT_HELMET]
        || &hat != hats.back())
    {
        return;
    }

    string key;
    if (is_artefact(hat))
        key = "zonguldrok hat good";
    else if (hat.brand)
        key = "zonguldrok hat okay";
    else
        key = "zonguldrok hat bad";

    if (is_unrandom_artefact(hat))
    {
        const unrandart_entry *entry = get_unrand_entry(hat.unrand_idx);
        string unrand_key = "zonguldrok hat " + string(entry->name);

        if (!getSpeakString(unrand_key).empty())
            key = unrand_key;
    }

    const string msg = "A voice whispers, \"" + getSpeakString(key) + "\"";
        mprf(MSGCH_TALK, "%s", msg.c_str());
}

static void _equip_armour_effect(item_def& arm, bool unmeld)
{
    int ego = get_armour_ego_type(arm);
    if (ego != SPARM_NORMAL)
    {
        switch (ego)
        {
        case SPARM_FIRE_RESISTANCE:
            mpr("You feel resistant to fire.");
            break;

        case SPARM_COLD_RESISTANCE:
            mpr("You feel resistant to cold.");
            break;

        case SPARM_POISON_RESISTANCE:
            if (player_res_poison(false, false, false) < 3)
                mpr("You feel resistant to poison.");
            break;

        case SPARM_SEE_INVISIBLE:
            mpr("You feel perceptive.");
            autotoggle_autopickup(false);
            break;

        case SPARM_INVISIBILITY:
            if (!you.duration[DUR_INVIS])
                mpr("You become transparent for a moment.");
            break;

        case SPARM_STRENGTH:
            notify_stat_change(STAT_STR, 3, false);
            break;

        case SPARM_DEXTERITY:
            notify_stat_change(STAT_DEX, 3, false);
            break;

        case SPARM_INTELLIGENCE:
            notify_stat_change(STAT_INT, 3, false);
            break;

        case SPARM_PONDEROUSNESS:
            mpr("You feel rather ponderous.");
            break;

        case SPARM_FLYING:
            _flight_equip();
            break;

        case SPARM_WILLPOWER:
            mpr("You feel strong-willed.");
            break;

        case SPARM_PROTECTION:
            mpr("You feel protected.");
            break;

        case SPARM_STEALTH:
            if (!you.get_mutation_level(MUT_NO_STEALTH))
                mpr("You feel stealthy.");
            break;

        case SPARM_RESISTANCE:
            mpr("You feel resistant to extremes of temperature.");
            break;

        case SPARM_POSITIVE_ENERGY:
            mpr("You feel more protected from negative energy.");
            break;

        case SPARM_ARCHMAGI:
            if (!you.skill(SK_SPELLCASTING))
                mpr("You feel strangely lacking in power.");
            else
                mpr("You feel powerful.");
            break;

        case SPARM_SPIRIT_SHIELD:
            _spirit_shield_message(unmeld);
            break;

        case SPARM_HURLING:
            mpr("You feel that your aim is more steady.");
            break;

        case SPARM_REPULSION:
            mpr("You are surrounded by a repulsion field.");
            break;

        case SPARM_SHADOWS:
            mpr("It gets dark.");
            update_vision_range();
            break;

        case SPARM_RAMPAGING:
            if (!you.has_mutation(MUT_ROLLPAGE))
                mpr("You feel ready to rampage towards enemies.");
            break;

        case SPARM_INFUSION:
            if (you.max_magic_points || you.has_mutation(MUT_HP_CASTING))
            {
                mprf("You feel magic pooling in your %s.",
                     you.hand_name(true).c_str());
            }
            else
            {
                bool can_plural = false;
                const string hands = you.hand_name(true, &can_plural);
                mprf("Your %s feel%s oddly empty.",
                     hands.c_str(), can_plural ? "" : "s");
            }
            break;

        case SPARM_LIGHT:
            invalidate_agrid(true);
            break;

        }

    }

    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    if ((arm.sub_type == ARM_HAT || arm.sub_type == ARM_HELMET)
        && !unmeld && you.unrand_equipped(UNRAND_SKULL_OF_ZONGULDROK))
    {
        _zonguldrok_comment_on_hat(arm);
    }
}

static void _unequip_armour_effect(item_def& item, bool meld)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    switch (get_armour_ego_type(item))
    {
    case SPARM_FIRE_RESISTANCE:
        mpr("You feel less resistant to fire.");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You feel less resistant to cold.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (player_res_poison() <= 0)
            mpr("You no longer feel resistant to poison.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
        {
            mpr("You feel less perceptive.");
            _mark_unseen_monsters();
        }
        break;

    case SPARM_STRENGTH:
        notify_stat_change(STAT_STR, -3, false);
        break;

    case SPARM_DEXTERITY:
        notify_stat_change(STAT_DEX, -3, false);
        break;

    case SPARM_INTELLIGENCE:
        notify_stat_change(STAT_INT, -3, false);
        break;

    case SPARM_PONDEROUSNESS:
    {
        // XX can the noun here be derived from the species walking verb?
        const string noun = you.species == SP_NAGA ? "slither" : "step";
        mprf("That put a bit of spring back into your %s.", noun.c_str());
        break;
    }

    case SPARM_FLYING:
        // XXX: Landing must be deferred until after form is actually changed.
        if (!meld)
            land_player();
        break;

    case SPARM_WILLPOWER:
        mpr("You feel less strong-willed.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        if (!you.get_mutation_level(MUT_NO_STEALTH))
            mpr("You feel less stealthy.");
        break;

    case SPARM_RESISTANCE:
        mpr("You feel hot and cold all over.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("You feel less protected from negative energy.");
        break;

    case SPARM_ARCHMAGI:
        mpr("You feel strangely numb.");
        break;

    case SPARM_SPIRIT_SHIELD:
        if (!you.spirit_shield())
        {
            mpr("You feel strangely alone.");
#if TAG_MAJOR_VERSION == 34
            if (you.species == SP_DEEP_DWARF)
                mpr("Your magic begins regenerating once more.");
#endif
        }
        break;

    case SPARM_HURLING:
        mpr("Your aim is not that steady anymore.");
        break;

    case SPARM_REPULSION:
        mpr("The haze of the repulsion field disappears.");
        break;

    case SPARM_SHADOWS:
        mpr("The dungeon's light returns to normal.");
        update_vision_range();
        break;

    case SPARM_RAMPAGING:
        if (!you.rampaging())
            mpr("You no longer feel able to rampage towards enemies.");
        break;

    case SPARM_INFUSION:
        if (you.max_magic_points || you.has_mutation(MUT_HP_CASTING))
            mprf("You feel magic leave your %s.", you.hand_name(true).c_str());
        break;

    case SPARM_LIGHT:
        invalidate_agrid(true);
        break;

    default:
        break;
    }
}

static void _remove_amulet_of_faith(item_def &item)
{
#ifndef DEBUG_DIAGNOSTICS
    UNUSED(item);
#endif
    if (!faith_has_penalty())
        return;
    if (you_worship(GOD_RU))
    {
        // next sacrifice is going to be delaaaayed.
        ASSERT(you.piety < piety_breakpoint(5));
#ifdef DEBUG_DIAGNOSTICS
        const int cur_delay = you.props[RU_SACRIFICE_DELAY_KEY].get_int();
#endif
        ru_reject_sacrifices(true);
        dprf("prev delay %d, new delay %d", cur_delay,
             you.props[RU_SACRIFICE_DELAY_KEY].get_int());
        return;
    }

    simple_god_message(" seems less interested in you.");

    const int piety_loss = div_rand_round(you.piety, 3);
    // Piety penalty for removing the Amulet of Faith.
    mprf(MSGCH_GOD, "You feel less pious.");
    dprf("%s: piety drain: %d", item.name(DESC_PLAIN).c_str(), piety_loss);
    lose_piety(piety_loss);
}

static void _handle_regen_item_equip(const item_def& item)
{
    const bool regen_hp = is_regen_item(item);
    const bool regen_mp = is_mana_regen_item(item);

    if (!regen_hp && !regen_mp)
        return;

    // The item is providing at least one of these two things, so let's check
    // attunement status.

    equipment_slot eq_slot = item_equip_slot(item);
    bool plural = (eq_slot == SLOT_BOOTS || eq_slot == SLOT_GLOVES);
    string item_name = is_artefact(item) ? get_artefact_name(item)
                                         : eq_slot == SLOT_AMULET
                                         ? "amulet"
                                         : lowercase_string(equip_slot_name(eq_slot, true));

#if TAG_MAJOR_VERSION == 34
    if (regen_hp && !regen_mp && you.get_mutation_level(MUT_NO_REGENERATION))
    {
        mprf("The %s feel%s cold and inert.", item_name.c_str(),
             plural ? "" : "s");
        return;
    }
#endif
    if (regen_mp && !regen_hp && !player_regenerates_mp())
    {
        mprf("The %s feel%s cold and inert.", item_name.c_str(),
             plural ? "" : "s");

        return;
    }

    bool low_mp = regen_mp && you.magic_points < you.max_magic_points;
    bool low_hp = regen_hp && you.hp < you.hp_max;

    if (!low_mp && !low_hp)
    {
        mprf("The %s throb%s to your%s body.", item_name.c_str(),
             plural ? " as they attune themselves" : "s as it attunes itself",
             regen_hp ? " uninjured" : "");
        you.equipment.get_entry_for(item).attuned = true;
        return;
    }

    mprf("The %s cannot attune %s to your%s body.", item_name.c_str(),
         plural ? "themselves" : "itself", low_hp ? " injured" : " exhausted");

    return;
}

bool acrobat_boost_active()
{
    return player_acrobatic()
           && you.duration[DUR_ACROBAT]
           && (!you.caught())
           && (!you.is_constricted());
}

static void _equip_amulet_of_reflection()
{
    you.redraw_armour_class = true;
    mpr("You feel a shielding aura gather around you.");
}

static void _equip_jewellery_effect(item_def &item, bool unmeld)
{
    switch (item.sub_type)
    {
    case RING_FIRE:
        mpr("You feel more attuned to fire.");
        break;

    case RING_ICE:
        mpr("You feel more attuned to ice.");
        break;

    case RING_SEE_INVISIBLE:
        autotoggle_autopickup(false);
        break;

    case RING_FLIGHT:
        _flight_equip();
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        notify_stat_change(STAT_STR, item.plus, false);
        break;

    case RING_DEXTERITY:
        notify_stat_change(STAT_DEX, item.plus, false);
        break;

    case RING_INTELLIGENCE:
        notify_stat_change(STAT_INT, item.plus, false);
        break;

    case RING_MAGICAL_POWER:
        if (you.has_mutation(MUT_HP_CASTING))
        {
            mpr("You repel a surge of foreign magic.");
            break;
        }
        canned_msg(MSG_MANA_INCREASE);
        calc_mp();
        break;

    case AMU_FAITH:
    {
        if (you.has_mutation(MUT_FORLORN))
        {
            mpr("You feel a surge of self-confidence.");
            break;
        }
        if (you.has_mutation(MUT_FAITH))
        {
            mpr("You already have all the faith you need.");
            break;
        }

        const string ignore_reason = ignore_faith_reason();
        if (!ignore_reason.empty())
            simple_god_message(ignore_reason.c_str());
        else
        {
            mprf(MSGCH_GOD, "You feel a %ssurge of divine interest.",
                            you_worship(GOD_NO_GOD) ? "strange " : "");
        }
    }

        break;

    case AMU_ACROBAT:
        if (you.has_mutation(MUT_TENGU_FLIGHT))
            mpr("You feel no more acrobatic than usual.");
        else
            mpr("You feel ready to tumble and roll out of harm's way.");
        break;

    case AMU_REFLECTION:
        _equip_amulet_of_reflection();
        break;

    case AMU_GUARDIAN_SPIRIT:
        _spirit_shield_message(unmeld);
        break;

    // Regeneration and Magic Regeneration amulets handled elsewhere.
    }

    if (!unmeld)
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _unequip_jewellery_effect(item_def &item, bool meld)
{
    // The ring/amulet must already be removed from you.equipment at this point.
    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_ICE:
    case RING_POSITIVE_ENERGY:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_WILLPOWER:
    case RING_SLAYING:
    case RING_STEALTH:
    case RING_WIZARDRY:
        break;

    case RING_SEE_INVISIBLE:
        _mark_unseen_monsters();
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case AMU_REFLECTION:
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        notify_stat_change(STAT_STR, -item.plus, false);
        break;

    case RING_DEXTERITY:
        notify_stat_change(STAT_DEX, -item.plus, false);
        break;

    case RING_INTELLIGENCE:
        notify_stat_change(STAT_INT, -item.plus, false);
        break;

    case RING_FLIGHT:
        // XXX: Landing must be deferred until after form is actually changed.
        if (!meld)
            land_player();
        break;

    case RING_MAGICAL_POWER:
        if (!you.has_mutation(MUT_HP_CASTING))
        {
            canned_msg(MSG_MANA_DECREASE);
            pay_mp(9);
        }
        break;

    case AMU_FAITH:
        if (!meld)
            _remove_amulet_of_faith(item);
        break;

#if TAG_MAJOR_VERSION == 34
    case AMU_GUARDIAN_SPIRIT:
        if (you.species == SP_DEEP_DWARF && player_regenerates_mp())
            mpr("Your magic begins regenerating once more.");
        break;
#endif

    }

    // Must occur after ring is removed. -- bwr
    calc_mp();
}

static void _mark_unseen_monsters()
{

    for (monster_iterator mi; mi; ++mi)
    {
        if (testbits((*mi)->flags, MF_WAS_IN_VIEW) && !you.can_see(**mi))
        {
            (*mi)->went_unseen_this_turn = true;
            (*mi)->unseen_pos = (*mi)->pos();
        }

    }
}

// This brand is supposed to be dangerous because it does large
// bonus damage, as well as banishment and other side effects,
// and you can rely on the occasional spatial bonus to mow down
// some opponents. It's far too powerful without a real risk.
// -- bwr [ed: ebering]
void unwield_distortion(bool brand)
{
    if (have_passive(passive_t::safe_distortion))
    {
        simple_god_message(make_stringf(" absorbs the residual spatial "
                           "distortion as you %s your "
                           "weapon.", brand ? "rebrand" : "unwield").c_str());
        return;
    }
    // Makes no sense to discourage unwielding a temporarily
    // branded weapon since you can wait it out. This also
    // fixes problems with unwield prompts (mantis #793).
    if (coinflip())
        you_teleport_now(false, true, "Space warps around you!");
    else if (coinflip())
    {
        you.banish(nullptr,
                   make_stringf("%sing a weapon of distortion",
                                brand ? "rebrand" : "unwield").c_str(),
                   you.get_experience_level(), true);
    }
    else
    {
        mpr("Space warps into you!");
        contaminate_player(random2avg(18000, 3), true);
    }
}

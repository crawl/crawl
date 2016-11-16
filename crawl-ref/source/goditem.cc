/**
 * @file
 * @brief Gods' attitude towards items.
**/

#include "AppHdr.h"

#include "goditem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "artefact.h"
#include "art-enum.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"

static bool _is_bookrod_type(const item_def& item,
                             bool (*matches)(spell_type spell))
{
    if (!item.defined())
        return false;

    // Return false for item_infos of unknown subtype
    // (== NUM_{BOOKS,RODS} in most cases, OBJ_RANDOM for acquirement)
    if (item.sub_type == get_max_subtype(item.base_type)
        || item.sub_type == OBJ_RANDOM)
    {
        return false;
    }

    if (item.base_type == OBJ_RODS)
        return matches(spell_in_rod(static_cast<rod_type>(item.sub_type)));

    if (!item_is_spellbook(item))
        return false;

    // Book matches only if all the spells match
    for (spell_type spell : spells_in_book(item))
    {
        if (!matches(spell))
            return false;
    }
    return true;
}

bool is_holy_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_HOLY)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = (is_blessed(item)
                  || get_weapon_brand(item) == SPWPN_HOLY_WRATH);
        break;
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_HOLY_WORD);
        break;
    default:
        break;
    }

    return retval;
}

bool is_potentially_evil_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_CHAOS)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    case OBJ_RODS:
        if (item.sub_type == ROD_CLOUDS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

// This is a subset of is_evil_item().
bool is_corpse_violating_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CORPSE_VIOLATING)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_REAPING);
        break;
    }
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_corpse_violating_spell);
        break;
    default:
        break;
    }

    return retval;
}

/**
 * Do good gods always hate use of this item?
 *
 * @param item      The item in question.
 * @return          Whether the Good Gods will always frown on this item's use.
 */
bool is_evil_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_EVIL)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (is_demonic(item))
            return true;
        {
        const int item_brand = get_weapon_brand(item);
        return item_brand == SPWPN_DRAINING
               || item_brand == SPWPN_PAIN
               || item_brand == SPWPN_VAMPIRISM
               || item_brand == SPWPN_REAPING;
        }
        break;
    case OBJ_POTIONS:
        return is_blood_potion(item);
    case OBJ_SCROLLS:
        return item.sub_type == SCR_TORMENT;
    case OBJ_STAVES:
        return item.sub_type == STAFF_DEATH;
    case OBJ_BOOKS:
    case OBJ_RODS:
        return _is_bookrod_type(item, is_evil_spell);
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_HORN_OF_GERYON;
    default:
        return false;
    }
}

bool is_unclean_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_UNCLEAN)
            return true;
    }

    if (item.has_spells())
        return _is_bookrod_type(item, is_unclean_spell);

    return false;
}

bool is_chaotic_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CHAOTIC)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_CHAOS);
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        retval = (item_brand == SPMSL_CHAOS);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_POLYMORPH);
        break;
    case OBJ_POTIONS:
        retval = item.sub_type == POT_MUTATION
                 || item.sub_type == POT_BENEFICIAL_MUTATION
                 || item.sub_type == POT_LIGNIFY;
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_chaotic_spell);
        break;
    case OBJ_MISCELLANY:
        retval = (item.sub_type == MISC_BOX_OF_BEASTS);
        break;
    default:
        break;
    }

    return retval;
}

static bool _is_potentially_hasty_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_CHAOS || item_brand == SPMSL_FRENZY)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_hasty_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_SPEED
                  || item.sub_type == WPN_QUICK_BLADE);
        }
        break;
    case OBJ_ARMOUR:
        {
        const int item_brand = get_armour_ego_type(item);
        retval = (item_brand == SPARM_RUNNING);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_HASTING);
        break;
    case OBJ_JEWELLERY:
        retval = (item.sub_type == AMU_RAGE && !is_artefact(item));
        break;
    case OBJ_POTIONS:
        retval = (item.sub_type == POT_HASTE
                  || item.sub_type == POT_BERSERK_RAGE);
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_hasty_spell);
        break;
    default:
        break;
    }

    return retval;
}

static bool _is_potentially_fiery_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    case OBJ_RODS:
        if (item.sub_type == ROD_CLOUDS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_fiery_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_FLAMING)
            return true;
        }
        break;
#if TAG_MAJOR_VERSION == 34
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_FLAME)
            return true;
        }
        break;
#endif
    case OBJ_WANDS:
        if (item.sub_type == WAND_FLAME)
            return true;
        break;
    case OBJ_SCROLLS:
        if (item.sub_type == SCR_IMMOLATION)
            return true;
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        return _is_bookrod_type(item, is_fiery_spell);
        break;
    case OBJ_STAVES:
        if (item.sub_type == STAFF_FIRE)
            return true;
        break;
    case OBJ_MISCELLANY:
        if (item.sub_type == MISC_LAMP_OF_FIRE)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_channeling_item(const item_def& item)
{
    if (is_unrandom_artefact(item, UNRAND_WUCAD_MU))
        return true;

    return item.base_type == OBJ_STAVES && item.sub_type == STAFF_ENERGY
           || item.base_type == OBJ_MISCELLANY
              && item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY;
}

bool is_corpse_violating_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_CORPSE_VIOLATING;
}

/**
 * Do the good gods hate use of this spell?
 *
 * @param spell     The spell in question; e.g. SPELL_CORPSE_ROT.
 * @return          Whether the Good Gods hate this spell.
 */
bool is_evil_spell(spell_type spell)
{
    const spschools_type disciplines = get_spell_disciplines(spell);
    unsigned int flags = get_spell_flags(spell);

    if (flags & SPFLAG_UNHOLY)
        return true;
    return bool(disciplines & SPTYP_NECROMANCY)
           && !bool(flags & SPFLAG_NOT_EVIL);
}

bool is_unclean_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_UNCLEAN);
}

bool is_chaotic_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_CHAOTIC);
}

bool is_hasty_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_HASTY);
}

bool is_fiery_spell(spell_type spell)
{
    const spschools_type disciplines = get_spell_disciplines(spell);

    return bool(disciplines & SPTYP_FIRE);
}

static bool _your_god_hates_spell(spell_type spell)
{
    return god_hates_spell(spell, you.religion);
}

static bool _your_god_hates_rod_spell(spell_type spell)
{
    return god_hates_spell(spell, you.religion, true);
}

/**
 * Do the good gods dislike players using this item? If so, why?
 *
 * @param item  The item in question.
 * @return      Whether using the item counts as DID_EVIL.
 */
static bool item_handling_is_evil(const item_def &item)
{
    if (is_demonic(item))
        return true;

    if ((item_type_known(item) || is_unrandom_artefact(item))
        && is_evil_item(item))
    {
        return true;
    }

    return false;
}

/**
 * Does the player's god hate them using the given item? If so, why?
 *
 * XXX: We should really be returning a list of all possible conducts for the
 * item and potentially letting callers filter them by the current god; this
 * is duplicating godconduct.cc otherwise.
 *
 * @param item  The item in question.
 * @return      Why the player's god hates the item, e.g. DID_HOLY for holy
 *              wrath items under Yredremnul; else DID_NOTHING.
 */
conduct_type god_hates_item_handling(const item_def &item)
{
    if (is_good_god(you.religion) && item_handling_is_evil(item))
        return DID_EVIL;

    switch (you.religion)
    {
    case GOD_ZIN:
        // Handled here rather than is_forbidden_food() because you can
        // butcher or otherwise desecrate the corpses all you want, just as
        // long as you don't eat the chunks. This check must come before the
        // item_type_known() check because the latter returns false for food
        // (and other item types without identification).
        if (item.is_type(OBJ_FOOD, FOOD_CHUNK) && is_mutagenic(item))
            return DID_DELIBERATE_MUTATING;

        if (!item_type_known(item))
            return DID_NOTHING;

        if (is_unclean_item(item))
            return DID_UNCLEAN;

        if (is_chaotic_item(item))
            return DID_CHAOS;
        break;

    case GOD_YREDELEMNUL:
        if (item_type_known(item) && is_holy_item(item))
            return DID_HOLY;
        break;

    case GOD_TROG:
        if (item_is_spellbook(item))
            return DID_SPELL_MEMORISE;
        if (item.sub_type == BOOK_MANUAL && item_type_known(item)
            && is_harmful_skill((skill_type)item.plus))
        {
            return DID_SPELL_PRACTISE;
        }
        // Only Trog cares about spellbooks vs rods.
        if (item.base_type == OBJ_RODS)
            return DID_NOTHING;
        break;

    case GOD_FEDHAS:
        if (item_type_known(item) && is_corpse_violating_item(item))
            return DID_CORPSE_VIOLATION;
        break;

    case GOD_CHEIBRIADOS:
        if (item_type_known(item) && (_is_potentially_hasty_item(item)
                                      || is_hasty_item(item))
            // Don't need item_type_known for quick blades.
            || item.is_type(OBJ_WEAPONS, WPN_QUICK_BLADE))
        {
            return DID_HASTY;
        }
        break;

    case GOD_DITHMENOS:
        if (item_type_known(item)
            && (_is_potentially_fiery_item(item) || is_fiery_item(item)))
        {
            return DID_FIRE;
        }
        break;

    case GOD_PAKELLAS:
        if (item_type_known(item) && is_channeling_item(item))
            return DID_CHANNEL;
        break;

    default:
        break;
    }

    if (item_type_known(item) && is_potentially_evil_item(item)
        && is_good_god(you.religion))
    {
        return DID_EVIL;
    }

    if (item_type_known(item) && _is_bookrod_type(item,
                                                  item.base_type == OBJ_RODS
                                                  ? _your_god_hates_rod_spell
                                                  : _your_god_hates_spell))
    {
        return NUM_CONDUCTS; // FIXME: Get the specific reason, if it
    }                          // will ever be needed for spellbooks.

    return DID_NOTHING;
}

bool god_hates_item(const item_def &item)
{
    return god_hates_item_handling(item) != DID_NOTHING;
}

/**
 * Does the given god like items of the given kind enough to make artefacts
 * from them? (Thematically.)
 *
 * @param item          The item which may be the basis for an artefact.
 * @param which_god     The god in question.
 * @return              Whether it makes sense for the given god to make an
 *                      artefact out of the given item. Thematically.
 *                      (E.g., Ely shouldn't be forging swords.)
 */
bool god_likes_item_type(const item_def &item, god_type which_god)
{
    // XXX: also check god_hates_item()?
    switch (which_god)
    {
        case GOD_ELYVILON:
            // Peaceful healer god: no weapons, no berserking.
            if (item.base_type == OBJ_WEAPONS)
                return false;

            if (item.is_type(OBJ_JEWELLERY, AMU_RAGE))
                return false;
            break;

        case GOD_SHINING_ONE:
            // Crusader god: holiness, honourable combat.
            if (item.is_type(OBJ_JEWELLERY, RING_STEALTH))
                return false;
            break;

        case GOD_SIF_MUNA:
        case GOD_VEHUMET:
            // The magic gods: no weapons, no preventing spellcasting.
            if (item.base_type == OBJ_WEAPONS)
                return false;
            break;

        case GOD_TROG:
            // Anti-magic god: no spell use, no enhancing magic.
            if (item.base_type == OBJ_BOOKS)
                return false;

            if (item.base_type == OBJ_JEWELLERY
                && (item.sub_type == RING_WIZARDRY
                    || item.sub_type == RING_ICE
                    || item.sub_type == RING_MAGICAL_POWER))
            {
                return false;
            }
            break;

        case GOD_CHEIBRIADOS:
            // Slow god: no quick blades, no berserking.
            if (item.is_type(OBJ_WEAPONS, WPN_QUICK_BLADE))
                return false;

            if (item.is_type(OBJ_JEWELLERY, AMU_RAGE))
                return false;
            break;

        case GOD_DITHMENOS:
            // Shadow god: no reducing stealth.
            if (item.is_type(OBJ_JEWELLERY, RING_LOUDNESS))
                return false;
            break;

        default:
            break;
    }

    return true;
}

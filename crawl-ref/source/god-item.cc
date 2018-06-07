/**
 * @file
 * @brief Gods' attitude towards items.
**/

#include "AppHdr.h"

#include "god-item.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "artefact.h"
#include "art-enum.h"
#include "food.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "potion-type.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"

static bool _is_book_type(const item_def& item,
                          bool (*matches)(spell_type spell))
{
    if (!item.defined())
        return false;

    // Return false for item_infos of unknown subtype
    // (== NUM_BOOKS in most cases, OBJ_RANDOM for acquirement)
    if (item.sub_type == get_max_subtype(item.base_type)
        || item.sub_type == OBJ_RANDOM)
    {
        return false;
    }

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

bool is_holy_item(const item_def& item, bool calc_unid)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_HOLY)
            return true;
    }

    if (item.base_type == OBJ_WEAPONS)
    {
        if (is_blessed(item))
            return true;
        if (calc_unid || item_brand_known(item))
            return get_weapon_brand(item) == SPWPN_HOLY_WRATH;
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_HOLY_WORD);
        break;
    default:
        break;
    }

    return retval;
}

bool is_potentially_evil_item(const item_def& item, bool calc_unid)
{
    if (item.base_type == OBJ_WEAPONS
        && item_brand_known(item)
        && get_weapon_brand(item) == SPWPN_CHAOS)
    {
        return true;
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_CHAOS)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS
            || item.sub_type == WAND_CLOUDS)
        {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

// This is a subset of is_evil_item().
bool is_corpse_violating_item(const item_def& item, bool calc_unid)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CORPSE_VIOLATING)
            return true;
    }

    if (item.base_type == OBJ_WEAPONS
        && (calc_unid || item_brand_known(item))
        && get_weapon_brand(item) == SPWPN_REAPING)
    {
        return true;
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_BOOKS:
        retval = _is_book_type(item, is_corpse_violating_spell);
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
 * @param calc_unid Whether to take into account facts the player does not know.
 * @return          Whether the Good Gods will always frown on this item's use.
 */
bool is_evil_item(const item_def& item, bool calc_unid)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_EVIL)
            return true;
    }

    if (item.base_type == OBJ_WEAPONS)
    {
        if (is_demonic(item))
            return true;
        if (calc_unid || item_brand_known(item))
        {
            const int item_brand = get_weapon_brand(item);
            return item_brand == SPWPN_DRAINING
                   || item_brand == SPWPN_PAIN
                   || item_brand == SPWPN_VAMPIRISM
                   || item_brand == SPWPN_REAPING;
        }
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_POTIONS:
        return is_blood_potion(item);
    case OBJ_SCROLLS:
        return item.sub_type == SCR_TORMENT;
    case OBJ_STAVES:
        return item.sub_type == STAFF_DEATH;
    case OBJ_BOOKS:
        return _is_book_type(item, is_evil_spell);
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_HORN_OF_GERYON;
    default:
        return false;
    }
}

bool is_unclean_item(const item_def& item, bool calc_unid)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_UNCLEAN)
            return true;
    }

    if (item.has_spells() && (item_type_known(item) || calc_unid))
        return _is_book_type(item, is_unclean_spell);

    return false;
}

bool is_chaotic_item(const item_def& item, bool calc_unid)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CHAOTIC)
            return true;
    }

    if (item.base_type == OBJ_WEAPONS
        && (calc_unid || item_brand_known(item)))
    {
        return get_weapon_brand(item) == SPWPN_CHAOS;
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
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
        retval = (item.sub_type == POT_MUTATION
                            && !have_passive(passive_t::cleanse_mut_potions))
                 || item.sub_type == POT_LIGNIFY;
        break;
    case OBJ_BOOKS:
        retval = _is_book_type(item, is_chaotic_spell);
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
    if (item.base_type == OBJ_WEAPONS
        && item_brand_known(item)
        && get_weapon_brand(item) == SPWPN_CHAOS)
    {
        return true;
    }

    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
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

bool is_hasty_item(const item_def& item, bool calc_unid)
{
    bool retval = false;

    if (item.base_type == OBJ_WEAPONS)
    {
        if (item.sub_type == WPN_QUICK_BLADE)
            return true;
        if (calc_unid || item_brand_known(item))
            return get_weapon_brand(item) == SPWPN_SPEED;
    }

    if (!calc_unid && !item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        {
        const int item_brand = get_armour_ego_type(item);
        retval = (item_brand == SPARM_RUNNING);
        }
        break;
    case OBJ_JEWELLERY:
        retval = (item.sub_type == AMU_RAGE && !is_artefact(item));
        break;
    case OBJ_POTIONS:
        retval = (item.sub_type == POT_HASTE
                  || item.sub_type == POT_BERSERK_RAGE);
        break;
    case OBJ_BOOKS:
        retval = _is_book_type(item, is_hasty_spell);
        break;
    default:
        break;
    }

    return retval;
}

bool is_channeling_item(const item_def& item, bool calc_unid)
{
    if (is_unrandom_artefact(item, UNRAND_WUCAD_MU))
        return true;

    if (!calc_unid && !item_type_known(item))
        return false;

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

/**
 * What conducts can one violate using this item?
 * This should only be based on the player's knowledge.
 *
 * @param item  The item in question.
 * @return      List of conducts that can be violated with this; empty if none.
 */
vector<conduct_type> item_conducts(const item_def &item)
{
    vector<conduct_type> conducts;

    if (is_evil_item(item, false))
        conducts.push_back(DID_EVIL);

    if (is_unclean_item(item, false))
        conducts.push_back(DID_UNCLEAN);

    if (is_chaotic_item(item, false))
        conducts.push_back(DID_CHAOS);

    if (is_holy_item(item, false))
        conducts.push_back(DID_HOLY);

    if (item_is_spellbook(item))
        conducts.push_back(DID_SPELL_MEMORISE);

    if (item.sub_type == BOOK_MANUAL && item_type_known(item)
        && is_magic_skill((skill_type)item.plus))
    {
        conducts.push_back(DID_SPELL_PRACTISE);
    }

    if (is_corpse_violating_item(item, false))
        conducts.push_back(DID_CORPSE_VIOLATION);

    if (_is_potentially_hasty_item(item) || is_hasty_item(item, false))
        conducts.push_back(DID_HASTY);

    if (is_channeling_item(item, false))
        conducts.push_back(DID_CHANNEL);

    if (is_potentially_evil_item(item, false))
        conducts.push_back(DID_EVIL);

    return conducts;
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

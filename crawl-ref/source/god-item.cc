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
#include "god-conduct.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
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

    // Return false for item_defs of unknown subtype
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
        if (calc_unid || item.is_identified())
            return get_weapon_brand(item) == SPWPN_HOLY_WRATH;
    }

    return false;
}

bool is_potentially_evil_item(const item_def& item, bool calc_unid)
{
    if (item.base_type == OBJ_WEAPONS
        && item.is_identified()
        && get_weapon_brand(item) == SPWPN_CHAOS)
    {
        return true;
    }

    if (!calc_unid && !item.is_identified())
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
    default:
        break;
    }

    return false;
}

bool is_evil_brand(int brand)
{
    switch (brand)
    {
    case SPWPN_DRAINING:
    case SPWPN_PAIN:
    case SPWPN_VAMPIRISM:
    case SPWPN_REAPING:
    case SPWPN_CHAOS:
    case SPWPN_DISTORTION:
    case SPWPN_FOUL_FLAME:
        return true;
    default:
        return false;
    }
}

bool is_chaotic_brand(int brand)
{
    return brand == SPWPN_CHAOS || brand == SPWPN_DISTORTION;
}

bool is_hasty_brand(int brand)
{
    return brand == SPWPN_CHAOS || brand == SPWPN_SPEED;
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
        if (is_demonic(item) || testbits(item.flags, ISFLAG_CHAOTIC))
            return true;
        if (calc_unid || item.is_identified())
            return is_evil_brand(get_weapon_brand(item));
    }

    if (!calc_unid && !item.is_identified())
        return false;

    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        return item.sub_type == SCR_TORMENT;
    case OBJ_STAVES:
        return item.sub_type == STAFF_DEATH;
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_HORN_OF_GERYON;
    case OBJ_BOOKS:
        return _is_book_type(item, is_evil_spell);
    case OBJ_TALISMANS:
        return item.sub_type == TALISMAN_DEATH;
    default:
        return false;
    }
}

bool is_unclean_item(const item_def& item, bool calc_unid)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if ((entry->flags & (UNRAND_FLAG_EVIL)
            || testbits(item.flags, ISFLAG_CHAOTIC)))
        {
            return true;
        }
    }

    if (item.has_spells() && (item.is_identified() || calc_unid))
        return _is_book_type(item, is_unclean_spell);

    return false;
}

bool is_chaotic_item(const item_def& item, bool calc_unid)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CHAOTIC ||
           (testbits(item.flags, ISFLAG_CHAOTIC)))
        {
            return true;
        }
    }

    if (item.base_type == OBJ_WEAPONS
        && (calc_unid || item.is_identified()))
    {
        return is_chaotic_brand(get_weapon_brand(item));
    }

    if (!calc_unid && !item.is_identified())
        return false;

    switch (item.base_type)
    {
    case OBJ_MISSILES:
        return get_ammo_brand(item) == SPMSL_CHAOS;
    case OBJ_WANDS:
        return item.sub_type == WAND_POLYMORPH;
    case OBJ_POTIONS:
        return (item.sub_type == POT_MUTATION
                            && !have_passive(passive_t::cleanse_mut_potions))
                 || item.sub_type == POT_LIGNIFY;
    case OBJ_BOOKS:
        return item.sub_type == BOOK_MANUAL && item.plus == SK_SHAPESHIFTING
               || _is_book_type(item, is_chaotic_spell);
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_BOX_OF_BEASTS;
    case OBJ_TALISMANS:
        return true;
    default:
        return false;
    }
}

static bool _is_potentially_hasty_item(const item_def& item)
{
    if (item.base_type == OBJ_WEAPONS
        && (item.is_identified() && get_weapon_brand(item) == SPWPN_CHAOS)
        || (testbits(item.flags, ISFLAG_CHAOTIC)))
    {
        return true;
    }

    if (!item.is_identified())
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
    default:
        break;
    }

    return false;
}

bool is_hasty_item(const item_def& item, bool calc_unid)
{

    if (is_artefact(item) && item.base_type != OBJ_BOOKS)
    {
        if ((calc_unid || item.is_identified()))
        {
            if (artefact_property(item, ARTP_RAMPAGING))
                return true;
            // intentionally continue to other hasty checks
        }
    }

    if (item.base_type == OBJ_WEAPONS)
    {
        if (calc_unid || item.is_identified())
            return get_weapon_brand(item) == SPWPN_SPEED;
    }

    if (!calc_unid && !item.is_identified())
        return false;

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return get_armour_rampaging(item, true)
               || get_armour_ego_type(item) == SPARM_MAYHEM
               || is_unrandom_artefact(item, UNRAND_LIGHTNING_SCALES);
    case OBJ_POTIONS:
        return item.sub_type == POT_HASTE;
    case OBJ_BOOKS:
        return _is_book_type(item, is_hasty_spell);
    default:
        break;
    }

    return false;
}

bool is_wizardly_item(const item_def& item, bool calc_unid)
{
    if ((calc_unid || item.is_identified())
        && get_armour_ego_type(item) == SPARM_ENERGY)
    {
        return true;
    }

    if (is_unrandom_artefact(item, UNRAND_WUCAD_MU)
        || is_unrandom_artefact(item, UNRAND_MAGE)
        || is_unrandom_artefact(item, UNRAND_MAJIN)
        || is_unrandom_artefact(item, UNRAND_BATTLE)
        || is_unrandom_artefact(item, UNRAND_ELEMENTAL_STAFF)
        || is_unrandom_artefact(item, UNRAND_OLGREB))
    {
        return true;
    }

    return item.base_type == OBJ_STAVES;
}

/**
 * Do the good gods hate use of this spell?
 *
 * @param spell     The spell in question; e.g. SPELL_PUTREFACTION.
 * @return          Whether the Good Gods hate this spell.
 */
bool is_evil_spell(spell_type spell)
{
    const spschools_type disciplines = get_spell_disciplines(spell);
    spell_flags flags = get_spell_flags(spell);

    if (flags & spflag::unholy)
        return true;
    return bool(disciplines & spschool::necromancy)
           && !bool(flags & spflag::not_evil);
}

bool is_unclean_spell(spell_type spell)
{
    spell_flags flags = get_spell_flags(spell);

    return bool(flags & spflag::unclean);
}

bool is_chaotic_spell(spell_type spell)
{
    spell_flags flags = get_spell_flags(spell);

    return bool(flags & spflag::chaotic);
}

bool is_hasty_spell(spell_type spell)
{
    spell_flags flags = get_spell_flags(spell);

    return bool(flags & spflag::hasty);
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

    if ((item.sub_type == BOOK_MANUAL && is_magic_skill((skill_type)item.plus)))
        conducts.push_back(DID_SPELL_PRACTISE);

    if (is_wizardly_item(item, false))
        conducts.push_back(DID_WIZARDLY_ITEM);

    if (_is_potentially_hasty_item(item) || is_hasty_item(item, false))
        conducts.push_back(DID_HASTY);

    if (is_potentially_evil_item(item, false))
        conducts.push_back(DID_EVIL);

    return conducts;
}

bool god_hates_item(const item_def &item)
{
    return god_hates_item_handling(item) != DID_NOTHING;
}

bool god_despises_item(const item_def &item, god_type which_god)
{
    if (item.base_type != OBJ_TALISMANS)
        return false;
    return item.sub_type == TALISMAN_DEATH && is_good_god(which_god)
           || which_god == GOD_ZIN;
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
    if (god_despises_item(item, which_god))
        return false;
    // XXX: also check god_hates_item()?
    // XXXX: if someone does this, make sure to generalize so that it doesn't
    // use `you.religion`; this code is potentially called in item generation
    // for artefact names
    switch (which_god)
    {
        case GOD_ELYVILON: // Peaceful healer god: no weapons.
        case GOD_SIF_MUNA: // The magic gods: no weapons.
        case GOD_VEHUMET:
            if (item.base_type == OBJ_WEAPONS)
                return false;
            break;

        case GOD_TROG:
            // Berserker god: weapons only.
            if (item.base_type != OBJ_WEAPONS)
                return false;
            break;

        default:
            break;
    }

    return true;
}

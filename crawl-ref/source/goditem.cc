/**
 * @file
 * @brief Gods' attitude towards items.
**/

#include "AppHdr.h"

#include "goditem.h"

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

#include "artefact.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"

static bool _is_bookrod_type(const item_def& item,
                             bool (*suitable)(spell_type spell))
{
    if (!item_is_spellbook(item) && item.base_type != OBJ_RODS)
        return false;

    // Return false for item_infos of unknown subtype
    // (== NUM_{BOOKS,RODS} in most cases, OBJ_RANDOM for acquirement)
    if (item.sub_type == get_max_subtype(item.base_type)
        || item.sub_type == OBJ_RANDOM)
    {
        return false;
    }

    int total       = 0;
    int total_liked = 0;

    for (int i = 0; i < SPELLBOOK_SIZE; ++i)
    {
        spell_type spell = which_spell_in_book(item, i);
        if (spell == SPELL_NO_SPELL)
            continue;

        total++;
        if (suitable(spell))
            total_liked++;
    }

    // If at least half of the available spells are suitable, the whole
    // spellbook or rod is, too.
    return total_liked >= (total / 2) + 1;
}

bool is_holy_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.special);

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

bool is_potentially_unholy_item(const item_def& item)
{
    return false;
}

bool is_unholy_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.special);

        if (entry->flags & UNRAND_FLAG_UNHOLY)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = is_demonic(item);
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_unholy_spell);
        break;
    case OBJ_MISCELLANY:
        retval = (item.sub_type == MISC_BOTTLED_EFREET);
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
        const unrandart_entry* entry = get_unrand_entry(item.special);

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

bool is_evil_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.special);

        if (entry->flags & UNRAND_FLAG_EVIL)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        return item_brand == SPWPN_DRAINING
               || item_brand == SPWPN_PAIN
               || item_brand == SPWPN_VAMPIRICISM
               || item_brand == SPWPN_REAPING;
        }
        break;
    case OBJ_WANDS:
        return item.sub_type == WAND_DRAINING;
    case OBJ_POTIONS:
        return is_blood_potion(item);
    case OBJ_SCROLLS:
        return item.sub_type == SCR_TORMENT;
    case OBJ_STAVES:
        return item.sub_type == STAFF_DEATH;
    case OBJ_BOOKS:
        return _is_bookrod_type(item, is_evil_spell);
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_LANTERN_OF_SHADOWS;
    default:
        return false;
    }
}

bool is_unclean_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.special);

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
        const unrandart_entry* entry = get_unrand_entry(item.special);

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

    if (is_artefact(item) && item.base_type != OBJ_BOOKS
        && artefact_wpn_property(item, ARTP_MUTAGENIC))
    {
        retval = true;
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
        retval = (item.sub_type == AMU_RAGE);
        break;
    case OBJ_POTIONS:
        retval = (item.sub_type == POT_SPEED
                  || item.sub_type == POT_BERSERK_RAGE);
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_hasty_spell);
        break;
    default:
        break;
    }

    if (is_artefact(item) && item.base_type != OBJ_BOOKS
        && (artefact_wpn_property(item, ARTP_ANGRY)
            || artefact_wpn_property(item, ARTP_BERSERK)))
    {
        retval = true;
    }

    return retval;
}

bool is_poisoned_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_VENOM)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_POISONED || item_brand == SPMSL_CURARE)
            return true;
        }
        break;
    case OBJ_STAVES:
        if (item.sub_type == STAFF_POISON)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_unholy_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_UNHOLY;
}

bool is_corpse_violating_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_CORPSE_VIOLATING;
}

bool is_evil_spell(spell_type spell)
{
    unsigned int disciplines = get_spell_disciplines(spell);

    return disciplines & SPTYP_NECROMANCY;
}

bool is_unclean_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_UNCLEAN;
}

bool is_chaotic_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_CHAOTIC;
}

bool is_hasty_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_HASTY;
}

static bool _your_god_hates_spell(spell_type spell)
{
    return god_hates_spell(spell, you.religion);
}

conduct_type good_god_hates_item_handling(const item_def &item)
{
    if (!is_good_god(you.religion)
        || (!is_unholy_item(item) && !is_evil_item(item)))
    {
        return DID_NOTHING;
    }

    if (item_type_known(item) || is_unrandom_artefact(item))
    {
        if (is_evil_item(item))
            return DID_NECROMANCY;
        else
            return DID_UNHOLY;
    }

    if (is_demonic(item))
        return DID_UNHOLY;

    return DID_NOTHING;
}

conduct_type god_hates_item_handling(const item_def &item)
{
    switch (you.religion)
    {
    case GOD_ZIN:
        if (!item_type_known(item))
            return DID_NOTHING;

        if (is_unclean_item(item))
            return DID_UNCLEAN;

        if (is_chaotic_item(item))
            return DID_CHAOS;
        break;

    case GOD_SHINING_ONE:
        if (item_type_known(item) && is_poisoned_item(item))
            return DID_POISON;
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
        // Only Trog cares about spellsbooks vs rods.
        if (item.base_type == OBJ_RODS)
            return DID_NOTHING;
        break;

    case GOD_FEDHAS:
        if (item_type_known(item) && is_corpse_violating_item(item))
            return DID_CORPSE_VIOLATION;
        break;

    case GOD_CHEIBRIADOS:
        if (item_type_known(item)
            && (_is_potentially_hasty_item(item) || is_hasty_item(item)))
        {
            return DID_HASTY;
        }
        break;

    default:
        break;
    }

    if (item_type_known(item) && is_potentially_unholy_item(item)
        && is_good_god(you.religion))
    {
        return DID_UNHOLY;
    }

    if (item_type_known(item) && is_potentially_evil_item(item)
        && is_good_god(you.religion))
    {
        return DID_NECROMANCY;
    }

    if (item_type_known(item) && _is_bookrod_type(item, _your_god_hates_spell))
    {
        return NUM_CONDUCTS; // FIXME: Get the specific reason, if it
    }                          // will ever be needed for spellbooks.

    return DID_NOTHING;
}

bool god_hates_item(const item_def &item)
{
    return (good_god_hates_item_handling(item) != DID_NOTHING)
            || (god_hates_item_handling(item) != DID_NOTHING);
}

bool god_dislikes_spell_type(spell_type spell, god_type god)
{
    if (god_hates_spell(spell, god))
        return true;

    unsigned int flags       = get_spell_flags(spell);
    unsigned int disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_SHINING_ONE:
        // TSO probably wouldn't like spells which would put enemies
        // into a state where attacking them would be unchivalrous.
        if (spell == SPELL_CAUSE_FEAR || spell == SPELL_PARALYSE
            || spell == SPELL_CONFUSE || spell == SPELL_MASS_CONFUSION
            || spell == SPELL_HIBERNATION)
        {
            return true;
        }
        break;

    case GOD_XOM:
        // Ideally, Xom would only like spells which have a random
        // effect, are risky to use, or would otherwise amuse him, but
        // that would be a really small number of spells.

        // Xom would probably find these extra boring.
        if (flags & (SPFLAG_HELPFUL | SPFLAG_NEUTRAL | SPFLAG_ESCAPE
                     | SPFLAG_RECOVERY | SPFLAG_MAPPING))
        {
            return true;
        }

        // Things are more fun for Xom the less the player knows in
        // advance.
        if (disciplines & SPTYP_DIVINATION)
            return true;
        break;

    case GOD_ELYVILON:
        // A peaceful god of healing wouldn't like combat spells.
        if (disciplines & SPTYP_CONJURATION)
            return true;

        // Also doesn't like battle spells of the non-conjuration type.
        if (flags & SPFLAG_BATTLE)
            return true;
        break;

    default:
        break;
    }

    return false;
}

bool god_dislikes_spell_discipline(int discipline, god_type god)
{
    ASSERT(discipline < (1 << (SPTYP_LAST_EXPONENT + 1)));

    if (is_good_god(god) && (discipline & SPTYP_NECROMANCY))
        return true;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return discipline & SPTYP_POISON;

    case GOD_ELYVILON:
        return discipline & (SPTYP_CONJURATION | SPTYP_SUMMONING);

    default:
        break;
    }

    return false;
}

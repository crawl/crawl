/**
 * @file
 * @brief Tracking mercenaries ( General : You bought, or got from Nemelex's card  and Special : Caravan, *TODO* : from minivault )
**/

#include "AppHdr.h"
#include "mercenaries.h"

#include <algorithm>

#include "act-iter.h"
#include "art-enum.h"
#include "artefact.h"
#include "attitude-change.h"
#include "branch.h"
#include "dgn-overview.h"
#include "directn.h"        // DESC_FULL... etc.
#include "colour.h"
#include "god-blessing.h"   // use gift_ammo_to_orc **TODO** make general function : gift_ammo_to
#include "god-companions.h" // to use companion functions
#include "god-item.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "message.h"
#include "mercenary-data.h"
#include "mon-death.h"
#include "mon-place.h" 
#include "mon-util.h"
#include "output.h"
#include "religion.h"
#include "spl-other.h"
#include "showsymb.h"
#include "viewchar.h"


/**
 * JOB_CARAVAN has starting mercenary
 */
void try_to_spawn_mercenary(int merc_type)
{
    const monster_type merctypes[] =
    {
        MONS_MERC_FIGHTER, MONS_MERC_SKALD,
        MONS_MERC_WITCH, MONS_MERC_BRIGAND,
        MONS_MERC_SHAMAN,
    };

    int merc;
    monster* mon;
    monster_type _montype = MONS_MERC_FIGHTER;
    if (merc_type == -1) {
        merc = you.props[CARAVAN_MERCENARY].get_int() > 0 ? you.props[CARAVAN_MERCENARY].get_int() - 1 : random2(4);
        ASSERT(merc < (int)ARRAYSZ(merctypes));
        _montype = merctypes[merc];
    }
    else {
        _montype = (monster_type)merc_type;
    }

    mgen_data mg(_montype, BEH_FRIENDLY,
        you.pos(), MHITYOU, MG_FORCE_BEH, you.religion);

    mg.extra_flags |= (MF_HARD_RESET);

    monster tempmon;
    tempmon.type = _montype;
    if (give_monster_proper_name(tempmon, false))
        mg.mname = tempmon.mname;
    else
        mg.mname = make_name();
    // This is used for giving the merc better stuff in mon-gear.
    mg.props["caravan_mercenary items"] = true;

    mon = create_monster(mg);

    if (!mon)
        return;

    mon->props["dbname"].get_string() = mons_class_name(_montype);
    redraw_screen();

    for (mon_inv_iterator ii(*mon); ii; ++ii)
        ii->flags &= ~ISFLAG_SUMMONED;
    mon->flags &= ~MF_HARD_RESET;
    mon->attitude = ATT_FRIENDLY;
    mons_att_changed(mon);
    mon->props[MERCENARY_FLAG] = true;
    add_companion(mon);

    item_def* weapon = mon->mslot_item(MSLOT_WEAPON);
    if(weapon != nullptr) {
      const bool staff = weapon->base_type == OBJ_STAVES;
      if (staff) {
          mon->spells.clear();
          switch (weapon->sub_type)
          {
          case STAFF_FIRE:
              mon->spells.emplace_back(SPELL_THROW_FLAME, 80, MON_SPELL_WIZARD);
              break;
          case STAFF_COLD:
              mon->spells.emplace_back(SPELL_THROW_FROST, 80, MON_SPELL_WIZARD);
              break;
          case STAFF_AIR:
              mon->spells.emplace_back(SPELL_SHOCK, 80, MON_SPELL_WIZARD);
              break;
          default:
              break;
          }
          mon->props[CUSTOM_SPELLS_KEY] = true;
      }
    }

    simple_monster_message(*mon, " follows you as a mercenary.");
    you.props.erase(CARAVAN_MERCENARY);
}

bool is_caravan_companion(monster mon)
{
    monster_type mon_type = mon.type;
    return (mon_type == MONS_MERC_FIGHTER
               || mon_type == MONS_MERC_KNIGHT
               || mon_type == MONS_MERC_DEATH_KNIGHT
               || mon_type == MONS_MERC_PALADIN
               || mon_type == MONS_MERC_SKALD
               || mon_type == MONS_MERC_INFUSER
               || mon_type == MONS_MERC_TIDEHUNTER
               || mon_type == MONS_MERC_WITCH
               || mon_type == MONS_MERC_SORCERESS
               || mon_type == MONS_MERC_ELEMENTALIST
               || mon_type == MONS_MERC_BRIGAND
               || mon_type== MONS_MERC_ASSASSIN
               || mon_type == MONS_MERC_CLEANER
               || mon_type == MONS_MERC_SHAMAN
               || mon_type == MONS_MERC_SHAMAN_II
               || mon_type == MONS_MERC_SHAMAN_III);
}

/**
 * Checks whether the target monster is a valid target for mercenary item-gifts.
 *
 * @param mons[in]  The monster to consider giving an item to.
 * @param quiet     Whether to print messages if the target is invalid.
 * @return          Whether the player can give an item to the monster.
 */
bool caravan_can_gift_items_to(const monster* mons, bool quiet)
{
    if (!mons || !mons->visible_to(&you))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_THERE);
        return false;
    }

    if (mons->attitude == ATT_FRIENDLY
        // heavy armour melee: able to lots of equipments
        && (mons->type == MONS_MERC_FIGHTER
            || mons->type == MONS_MERC_KNIGHT
            || mons->type == MONS_MERC_DEATH_KNIGHT
            || mons->type == MONS_MERC_PALADIN
        // EV-skald: polearms & light armour
        || mons->type == MONS_MERC_SKALD
            || mons->type == MONS_MERC_INFUSER
            || mons->type == MONS_MERC_TIDEHUNTER
        // pure-wizard: unable to lots of equipments
        || mons->type == MONS_MERC_WITCH
            || mons->type == MONS_MERC_SORCERESS
            || mons->type == MONS_MERC_ELEMENTALIST
        // stealth: short blades & light armour
        || mons->type == MONS_MERC_BRIGAND
            || mons->type == MONS_MERC_ASSASSIN
            || mons->type == MONS_MERC_CLEANER
        // gnoll: able to all equipments after grow
        || mons->type == MONS_MERC_SHAMAN
            || mons->type == MONS_MERC_SHAMAN_II
            || mons->type == MONS_MERC_SHAMAN_III))
    {
        return true;
    }

    return false;
}

/**
 * Return the monster* vector of valid mercenary for gifts in LOS.
 */
static vector<monster* > _valid_caravan_gift_targets_in_sight()
{
    vector<monster* > list_merc;
    for (monster_near_iterator rad(you.pos(), LOS_NO_TRANS); rad; ++rad)
        if (caravan_can_gift_items_to(*rad, true))
            list_merc.push_back(*rad);
    sort(list_merc.begin(), list_merc.end(), 
    [](monster* m1, monster* m2){
        monster_info m1i = monster_info(m1);
        monster_info m2i = monster_info(m2);
        return monster_info::less_than(m1i, m2i, true);});
    return list_merc;
}

bool set_spell_witch(monster* mons, item_def* staff, bool slience) {

    // unrandarts are unique, so set more spells
    if (is_unrandom_artefact(*staff, UNRAND_MAJIN))
    {
        mons->spells.clear();
        switch (mons->type) // Demonology theme: Burn & Fiends
        {// Note: Dose majin will drain health form mercs?
        case MONS_MERC_WITCH:
            mons->spells.emplace_back(SPELL_CALL_IMP, 9, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_SORCERESS:
            mons->spells.emplace_back(SPELL_SUMMON_MINOR_DEMON, 9, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_ELEMENTALIST:
            mons->spells.emplace_back(SPELL_SUMMON_DEMON, 9, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD);
            break;
        default:
            break;
        }
        return true;
    }

    else if (is_unrandom_artefact(*staff, UNRAND_WUCAD_MU))
    {
        mons->spells.clear();
        switch (mons->type) // Wizard theme: Useful & Various
        {
        case MONS_MERC_WITCH:
            mons->spells.emplace_back(SPELL_MAGIC_DART, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_CORONA, 11, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_SORCERESS:
            mons->spells.emplace_back(SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_CONFUSE, 11, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_ELEMENTALIST:
            mons->spells.emplace_back(SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_CONFUSE, 11, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BLINK_RANGE, 33, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY);
            break;
        default:
            break;
        }
        return true;
    }

    else if (is_unrandom_artefact(*staff, UNRAND_ELEMENTAL_STAFF))
    {
        mons->spells.clear();
        switch (mons->type) // Elementalist theme
        {
        case MONS_MERC_WITCH:
            mons->spells.emplace_back(SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_THROW_FROST, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_SANDBLAST, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_SHOCK, 66, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_SORCERESS:
            mons->spells.emplace_back(SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BOLT_OF_COLD, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_STONE_ARROW, 66, MON_SPELL_WIZARD | MON_SPELL_SHORT_RANGE);
            mons->spells.emplace_back(SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
            break;
        case MONS_MERC_ELEMENTALIST:
            mons->spells.emplace_back(SPELL_FIREBALL, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
            mons->spells.emplace_back(SPELL_FREEZING_CLOUD, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
            mons->spells.emplace_back(SPELL_IRON_SHOT, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_CHAIN_LIGHTNING, 66, MON_SPELL_WIZARD);
            break;
        default:
            break;
        }
        return true;
    }

    else if (is_unrandom_artefact(*staff, UNRAND_OLGREB))
    {
        mons->spells.clear();
        switch (mons->type) // +Olgrebs toxic radiance
        {
        case MONS_MERC_WITCH:
            mons->spells.emplace_back(SPELL_STING, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_SORCERESS:
            mons->spells.emplace_back(SPELL_VENOM_BOLT, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_ELEMENTALIST:
            mons->spells.emplace_back(SPELL_POISON_ARROW, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_OLGREBS_TOXIC_RADIANCE, 33, MON_SPELL_WIZARD);
            break;
        default:
            break;
        }
        return true;
    }

    else if (is_unrandom_artefact(*staff, UNRAND_BATTLE))
    {
        mons->spells.clear();
        switch (mons->type) // Iskenderun theme
        {
        case MONS_MERC_WITCH:
            mons->spells.emplace_back(SPELL_MAGIC_DART, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_SORCERESS:
            mons->spells.emplace_back(SPELL_FORCE_LANCE, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD);
            break;
        case MONS_MERC_ELEMENTALIST:
            mons->spells.emplace_back(SPELL_ISKENDERUNS_MYSTIC_BLAST, 66, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_FORCE_LANCE, 33, MON_SPELL_WIZARD);
            mons->spells.emplace_back(SPELL_BATTLESPHERE, 9, MON_SPELL_WIZARD);
            break;
        default:
            break;
        }
        return true;
    }

    else if (staff->base_type == OBJ_STAVES)
    {
        switch (staff->sub_type)
        {
        case STAFF_FIRE:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_THROW_FLAME, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
                mons->spells.emplace_back(SPELL_BOLT_OF_FIRE, 80, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_BOLT_OF_FIRE, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_FIREBALL, 80, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
                break;
            default:
                break;
            }
            break;
        case STAFF_COLD:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_THROW_FROST, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_THROW_FROST, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
                mons->spells.emplace_back(SPELL_BOLT_OF_COLD, 80, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_BOLT_OF_COLD, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_FREEZING_CLOUD, 80, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
                break;
            default:
                break;
            }
            break;
        case STAFF_AIR:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_SHOCK, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_LIGHTNING_BOLT, 66, MON_SPELL_WIZARD | MON_SPELL_LONG_RANGE);
                mons->spells.emplace_back(SPELL_CHAIN_LIGHTNING, 80, MON_SPELL_WIZARD);
                break;
            default:
                break;
            }
            break;
        case STAFF_EARTH:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_STONESKIN, 11, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_SANDBLAST, 80, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_STONESKIN, 11, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_STONE_ARROW, 80, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_STONESKIN, 11, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_IRON_SHOT, 80, MON_SPELL_WIZARD);
                break;
            default:
                break;
            }
            break;
        case STAFF_POISON:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_STING, 33, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_VENOM_BOLT, 33, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_POISON_ARROW, 33, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_MEPHITIC_CLOUD, 33, MON_SPELL_WIZARD);
                break;
            default:
                break;
            }
            break;
        case STAFF_DEATH:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_PAIN, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_AGONY, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_DISPEL_UNDEAD, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_AGONY, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_DISPEL_UNDEAD, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_DEATHS_DOOR, 80, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY);
                break;
            default:
                break;
            }
            break;
        case STAFF_CONJURATION:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_FORCE_LANCE, 80, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_DAZZLING_SPRAY, 66, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_FORCE_LANCE, 80, MON_SPELL_WIZARD);
                mons->spells.emplace_back(SPELL_BATTLESPHERE, 11, MON_SPELL_WIZARD);
                break;
            default:
                break;
            }
            break;
        case STAFF_SUMMONING:
            mons->spells.clear();
            switch (mons->type)
            {
            case MONS_MERC_WITCH:
                mons->spells.emplace_back(SPELL_CALL_CANINE_FAMILIAR, 50, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_SORCERESS:
                mons->spells.emplace_back(SPELL_SUMMON_ICE_BEAST, 50, MON_SPELL_WIZARD);
                break;
            case MONS_MERC_ELEMENTALIST:
                mons->spells.emplace_back(SPELL_MONSTROUS_MENAGERIE, 50, MON_SPELL_WIZARD);
                break;
            default:
                break;
            }
            break;
        default: // wizardry, power, energy
            if (!slience) {
                mprf("This staff isn't useful for %s.",
                    mons->name(DESC_THE, false).c_str());
            }
            return false;
        }
    }
    return true;
}

static void _is_too_heavy(monster* mons, item_def& gift)
{
    mprf("%s is too heavy for %s!",
        gift.name(DESC_THE, false).c_str(), mons->name(DESC_THE, false).c_str());
}

static void _is_unskilled(monster* mons, item_def& gift)
{
    mprf("%s isn't enough skilled to equip %s!",
        mons->name(DESC_THE, false).c_str(), gift.name(DESC_THE, false).c_str());
}

static bool _can_use_shield(monster* mons, item_def& gift)
{
    bool fighter = (mons->type == MONS_MERC_FIGHTER
            || mons->type == MONS_MERC_KNIGHT
            || mons->type == MONS_MERC_DEATH_KNIGHT
            || mons->type == MONS_MERC_PALADIN);

    bool buckler_only = (mons->type == MONS_MERC_SKALD
            || mons->type == MONS_MERC_INFUSER
            || mons->type == MONS_MERC_TIDEHUNTER
            || mons->type == MONS_MERC_SHAMAN);

    bool no_shield = (mons->type == MONS_MERC_WITCH
            || mons->type == MONS_MERC_SORCERESS
            || mons->type == MONS_MERC_ELEMENTALIST);

    if (no_shield)
    {
        _is_too_heavy(mons, gift);
        return false;
    }

    if (buckler_only && gift.sub_type == ARM_BUCKLER)
        return true;

    if (fighter)
    {
        if (mons->type == MONS_MERC_DEATH_KNIGHT
            || mons->type == MONS_MERC_PALADIN)
            return true;
        
        if (mons->type == MONS_MERC_FIGHTER
            || mons->type == MONS_MERC_KNIGHT)
        {
            if (gift.sub_type == ARM_LARGE_SHIELD)
            {
                _is_unskilled(mons, gift);
                return false;
            }
            else return true;
        }
    }

    if (mons->type == MONS_MERC_SHAMAN)
    {
        if (gift.sub_type == ARM_BUCKLER)
            return true;
        else
        {
            _is_unskilled(mons, gift);
            return false;
        }
    }

    if (mons->type == MONS_MERC_SHAMAN_II)
    {
        if (gift.sub_type == ARM_BUCKLER
            || gift.sub_type == ARM_SHIELD)
            return true;
        else
        {
            _is_unskilled(mons, gift);
            return false;
        }
    }

    if (mons->type == MONS_MERC_SHAMAN_III)
        return true;

    _is_too_heavy(mons, gift);
    return false;
}

static bool _can_use_range(monster* mons, item_def& gift)
{
    bool shaman = (mons->type == MONS_MERC_SHAMAN
            || mons->type == MONS_MERC_SHAMAN_II
            || mons->type == MONS_MERC_SHAMAN_III);

    bool range_tier1 = (gift.sub_type == WPN_HUNTING_SLING);

    bool range_tier2 = (range_tier1 || gift.sub_type == WPN_HAND_CROSSBOW
            || gift.sub_type == WPN_FUSTIBALUS || gift.sub_type == WPN_SHORTBOW);

    bool range_tier3 = (range_tier1 || range_tier2
            || gift.sub_type == WPN_ARBALEST || gift.sub_type == WPN_LONGBOW);

    bool range_tier4 = (gift.sub_type == WPN_TRIPLE_CROSSBOW);

    if (!shaman)
    {
        _is_unskilled(mons, gift);
        return false;
    }

    if ((mons->type == MONS_MERC_SHAMAN && range_tier1)
        || (mons->type == MONS_MERC_SHAMAN_II && range_tier2)
        || (mons->type == MONS_MERC_SHAMAN_III && range_tier3))
        return true;

    _is_unskilled(mons, gift);
    return false;
}

static bool _can_use_melee(monster* mons, item_def& gift)
{
    bool melee_tier1 =
       (gift.sub_type == WPN_QUICK_BLADE || gift.sub_type == WPN_DAGGER 
        || gift.sub_type == WPN_WHIP || gift.sub_type == WPN_DEMON_WHIP
        || gift.sub_type == WPN_SACRED_SCOURGE || gift.sub_type == WPN_SPEAR
        || gift.sub_type == WPN_SHORT_SWORD || gift.sub_type == WPN_RAPIER
        || gift.sub_type == WPN_EUDEMON_BLADE || gift.sub_type == WPN_CLUB
        || gift.sub_type == WPN_DIRE_FLAIL || gift.sub_type == WPN_FALCHION
        || gift.sub_type == WPN_DEMON_BLADE || gift.sub_type == WPN_HAND_AXE
        || gift.sub_type == WPN_TRIDENT || gift.sub_type == WPN_DEMON_TRIDENT
        || gift.sub_type == WPN_TRISHULA || gift.sub_type == WPN_QUARTERSTAFF
        || gift.sub_type == WPN_MACE || gift.sub_type == WPN_FLAIL
        || gift.sub_type == WPN_LONG_SWORD || gift.sub_type == WPN_SCIMITAR
        || gift.sub_type == WPN_LAJATANG);

    bool melee_tier2 = (melee_tier1
        || gift.sub_type == WPN_MORNINGSTAR
        || gift.sub_type == WPN_EVENINGSTAR
        || gift.sub_type == WPN_DOUBLE_SWORD
        || gift.sub_type == WPN_WAR_AXE
        || gift.sub_type == WPN_BROAD_AXE
        || gift.sub_type == WPN_HALBERD);

    bool melee_tier3 = (melee_tier1 || melee_tier2 
        || gift.sub_type == WPN_BATTLEAXE
        || gift.sub_type == WPN_GREAT_MACE
        || gift.sub_type == WPN_GREAT_SWORD
        || gift.sub_type == WPN_GLAIVE);

    bool melee_tier4 = (melee_tier1
        || melee_tier2 || melee_tier3
        || gift.sub_type == WPN_TRIPLE_SWORD
        || gift.sub_type == WPN_EXECUTIONERS_AXE
        || gift.sub_type == WPN_SCYTHE
        || gift.sub_type == WPN_BARDICHE);

    if ((mons->type == MONS_MERC_SHAMAN && melee_tier1)
        || (mons->type == MONS_MERC_SHAMAN_II && melee_tier2)
        || (mons->type == MONS_MERC_SHAMAN_III && melee_tier3)
        || (mons->type == MONS_MERC_FIGHTER && melee_tier2)
        || (mons->type == MONS_MERC_KNIGHT && melee_tier3)
        || (mons->type == MONS_MERC_DEATH_KNIGHT && melee_tier4)
        || (mons->type == MONS_MERC_PALADIN && melee_tier4))
        return true;

    _is_unskilled(mons, gift);
    return false;
}

static bool _caravan_gift_items_to(monster* mons, int item_slot)
{
    item_def& gift = you.inv[item_slot];

    const bool shield = is_shield(gift);
    const bool body_armour = gift.base_type == OBJ_ARMOUR
                             && get_armour_slot(gift) == EQ_BODY_ARMOUR;
    const bool weapon = gift.base_type == OBJ_WEAPONS
                             || gift.base_type == OBJ_STAVES;
    const bool range_weapon = weapon && is_range_weapon(gift);
    const item_def* mons_weapon = mons->weapon();
    const item_def* mons_alt_weapon = mons->mslot_item(MSLOT_ALT_WEAPON);

    if (weapon && !mons->could_wield(gift)
        || body_armour && !check_armour_size(gift, mons->body_size())
        || !item_is_selected(gift, OSEL_MERCENARY_GIFT))
    {
        mprf("You can't give that to %s.", mons->name(DESC_THE, false).c_str());
        return false;
    }
    else if (shield
             && (mons_weapon && mons->hands_reqd(*mons_weapon) == HANDS_TWO
                 || mons_alt_weapon
                    && mons->hands_reqd(*mons_alt_weapon) == HANDS_TWO))
    {
        mprf("%s can't equip that with a two-handed weapon.",
             mons->name(DESC_THE, false).c_str());
        return false;
    }

    const bool use_alt_slot = weapon && mons_weapon
                              && is_range_weapon(gift) !=
                                 is_range_weapon(*mons_weapon);

    const auto mslot = body_armour ? MSLOT_ARMOUR :
                                    shield ? MSLOT_SHIELD :
                              use_alt_slot ? MSLOT_ALT_WEAPON :
                                             MSLOT_WEAPON;

    item_def *body_slot = mons->mslot_item(MSLOT_ARMOUR);

    item_def* item_to_drop = mons->mslot_item(mslot);
    if (item_to_drop && item_to_drop->cursed())
    {
        mprf(MSGCH_ERROR, "%s is cursed!", item_to_drop->name(DESC_THE, false).c_str());
        return false;
    }

    if (gift.cursed() || (is_artefact(gift) && artefact_property(gift, ARTP_CURSE)))
    {
        mprf(MSGCH_ERROR, "%s is cursed!", gift.name(DESC_THE, false).c_str());
        return false;
    } 

    bool spellcaster = (mons->type == MONS_MERC_SKALD
            || mons->type == MONS_MERC_INFUSER
            || mons->type == MONS_MERC_TIDEHUNTER
            || mons->type == MONS_MERC_WITCH
            || mons->type == MONS_MERC_SORCERESS
            || mons->type == MONS_MERC_ELEMENTALIST
            || mons->type == MONS_MERC_SHAMAN
            || mons->type == MONS_MERC_SHAMAN_II
            || mons->type == MONS_MERC_SHAMAN_III);

    if (spellcaster && ((is_artefact(gift)
        && artefact_property(gift, ARTP_PREVENT_SPELLCASTING)
            || get_weapon_brand(gift) == SPWPN_ANTIMAGIC)))
    {
        mprf("%s can't use spellcast-disturbing equipments.",
             mons->name(DESC_THE, false).c_str());
        return false;
    }

    if ((mons->type == MONS_MERC_BRIGAND
            || mons->type == MONS_MERC_ASSASSIN
            || mons->type == MONS_MERC_CLEANER)
        && is_artefact(gift)
        && (artefact_property(gift, ARTP_STEALTH) < 0
            || artefact_property(gift, ARTP_ANGRY)
            || artefact_property(gift, ARTP_NOISE)
            || artefact_property(gift, ARTP_CAUSE_TELEPORTATION)))
    {
        mprf("%s can't use stealth-disturbing equipments.",
             mons->name(DESC_THE, false).c_str());
        return false;
    }

    if ((mons->type == MONS_MERC_DEATH_KNIGHT && is_holy_item(gift))
       || (mons->type == MONS_MERC_PALADIN && is_evil_item(gift)))
    {
        mprf("%s rejects %s hand!",
            gift.name(DESC_THE, false).c_str(), mons->name(DESC_THE, false).c_str());
        return false;
    }

    // shields
    if (is_shield(gift)) {

        item_def *shield_slot = mons->mslot_item(MSLOT_SHIELD);
        if ((mslot == MSLOT_WEAPON || mslot == MSLOT_ALT_WEAPON)
            && shield_slot
            && mons->hands_reqd(gift) == HANDS_TWO)
        {
            if (gift.cursed() || (is_artefact(gift) && artefact_property(gift, ARTP_CURSE)))
            {
                mprf(MSGCH_ERROR, "%s is cursed!", gift.name(DESC_THE, false).c_str());
                return false;
            }
        }

        if (!_can_use_shield(mons, gift))
            return false;
    }

    if (weapon) {// weapons.

        // ranged weapon
        if (range_weapon)
        {
            if (!_can_use_range(mons, gift))
                return false;

        } else {

        // polearms for skald.
        if (mons->type == MONS_MERC_SKALD
            || mons->type == MONS_MERC_INFUSER
            || mons->type == MONS_MERC_TIDEHUNTER)
        {
            if (item_attack_skill(gift) != SK_POLEARMS)
            {
                mprf("%s can only equip polearms.",
                mons->name(DESC_THE, false).c_str());
                return false;
            }

            if (mons->type == MONS_MERC_SKALD
                && (gift.sub_type == WPN_SCYTHE
                   || gift.sub_type == WPN_GLAIVE
                   || gift.sub_type == WPN_BARDICHE))
            {
                _is_unskilled(mons, gift);
                return false;
            }

            if (mons->type == MONS_MERC_INFUSER
                && (gift.sub_type == WPN_SCYTHE
                   || gift.sub_type == WPN_BARDICHE))
            {
                _is_unskilled(mons, gift);
                return false;
            }
        }

        // magical staves for witch
        if (mons->type == MONS_MERC_WITCH
            || mons->type == MONS_MERC_SORCERESS
            || mons->type == MONS_MERC_ELEMENTALIST)
        {
            if (gift.base_type == OBJ_STAVES
                || is_unrandom_artefact(gift, UNRAND_MAJIN)
                || is_unrandom_artefact(gift, UNRAND_WUCAD_MU)
                || is_unrandom_artefact(gift, UNRAND_ELEMENTAL_STAFF)
                || is_unrandom_artefact(gift, UNRAND_OLGREB)
                || is_unrandom_artefact(gift, UNRAND_BATTLE))
            {
                if (set_spell_witch(mons, &gift, false) == false)
                {
                    mons->props[CUSTOM_SPELLS_KEY] = true;
                    return false;
                }
            }
            else
            {
                mprf("%s can only equip magical staves.",
                mons->name(DESC_THE, false).c_str());
                return false;
            }
        }

        // stealth weapon for brigand.
        if (mons->type == MONS_MERC_BRIGAND
            || mons->type == MONS_MERC_ASSASSIN
            || mons->type == MONS_MERC_CLEANER)
        {
            if (item_attack_skill(gift) != SK_SHORT_BLADES)
            {
                mprf("%s can only equip short blades.",
                mons->name(DESC_THE, false).c_str());
                return false;
            }
        }

        if (!_can_use_melee(mons, gift))
            return false;
    } }

    if (body_slot && !is_shield(gift)) {// body armours.

        const int ER = -property(gift, PARM_EVASION) / 10;
        if (((mons->type == MONS_MERC_WITCH
            || mons->type == MONS_MERC_SORCERESS
            || mons->type == MONS_MERC_ELEMENTALIST)
            && ER > 0)
        
        || ((mons->type == MONS_MERC_SKALD
            || mons->type == MONS_MERC_INFUSER
            || mons->type == MONS_MERC_TIDEHUNTER
            || mons->type == MONS_MERC_BRIGAND
            || mons->type == MONS_MERC_ASSASSIN
            || mons->type == MONS_MERC_CLEANER
            || mons->type == MONS_MERC_SHAMAN)
            && ER > 4)
        
        || ((mons->type == MONS_MERC_SHAMAN_II)
            && ER > 11)
        
        || ((mons->type == MONS_MERC_SHAMAN_III
            || mons->type == MONS_MERC_FIGHTER)
            && ER > 15)

        || ((mons->type == MONS_MERC_KNIGHT)
            && ER > 18))

        {
            if (mons->type == MONS_MERC_SHAMAN_II
               || mons->type == MONS_MERC_SHAMAN_III)
            {
                _is_unskilled(mons, gift);
                return false;
            }
        
            _is_too_heavy(mons, gift);
            return false;
        }
    }

    item_def *floor_item = mons->take_item(item_slot, mslot);
    if (!floor_item)
    {
        // this probably means move_to_grid in drop_item failed?
        mprf(MSGCH_ERROR, "Gift failed: %s is unable to take %s.",
                                        mons->name(DESC_THE, false).c_str(),
                                        gift.name(DESC_THE, false).c_str());
        return false;
    }
    if (use_alt_slot)
        mons->swap_weapons();

    dprf("is_ranged weap: %d", range_weapon);
    if (range_weapon)
        gift_ammo_to_orc(mons, true); // give a small initial ammo freebie

    return true;
}


/**
 * Allow the caravan player to give an item to a mercenary.
 *
 * @returns whether an item was given.
 */
bool caravan_gift_item()
{
    vector<monster *> list_merc = _valid_caravan_gift_targets_in_sight();
    vector<monster_info *> list_merc_infos;
    if (list_merc.empty())
    {
        mpr("You cannot find mercenary in your sight.");
        return false;
    }

    for (const monster* mercenary : list_merc)
    {
        monster_info* mi = new monster_info(mercenary);
        list_merc_infos.push_back(mi);
    }

    InvMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                        | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);


    string title = "Select a mercenary to give a gift to.";
    desc_menu.set_title(new MenuEntry(title, MEL_TITLE));
    desc_menu.menu_action  = InvMenu::ACT_EXECUTE;
    // Start with hotkey 'a' and count from there.
    menu_letter hotkey;
    // Build menu entries for monsters.
    
    desc_menu.add_entry(new MenuEntry("Mercs", MEL_SUBTITLE));
    for (const monster_info* mi : list_merc_infos)
    {
        // List merc in the form
        string prefix = "";
#ifndef USE_TILE_LOCAL
            cglyph_t g = get_mons_glyph(*mi);
            const string col_string = colour_to_str(g.col);
            prefix = "(<" + col_string + ">"
                    + (g.ch == '<' ? "<<" : stringize_glyph(g.ch))
                    + "</" + col_string + ">) ";
#endif
        string str = get_monster_equipment_desc(*mi, DESC_FULL, DESC_A, false);

#ifndef USE_TILE_LOCAL
            // Wraparound if the description is longer than allowed.
            linebreak_string(str, get_number_of_cols() - 9);
#endif
        vector<formatted_string> fss;
        formatted_string::parse_string_to_multiple(str, fss);
        MenuEntry *me = nullptr;
        for (unsigned int j = 0; j < fss.size(); ++j)
        {
            if (j == 0)
                me = new MonsterMenuEntry(prefix + fss[j].tostring(), mi, hotkey++);
#ifndef USE_TILE_LOCAL
            else
            {
                str = "         " + fss[j].tostring();
                me = new MenuEntry(str, MEL_ITEM, 1);
            }
#endif
            desc_menu.add_entry(me);
        }
    }

    desc_menu.on_single_selection = [&list_merc, &list_merc_infos](const MenuEntry& sel)
    {   
        monster* mons = list_merc[distance(list_merc_infos.begin(), 
                                      find(list_merc_infos.begin(), list_merc_infos.end(), sel.data))];

        // Gift Part
        int item_slot = prompt_invent_item("Give which item?",
                                       menu_type::invlist, OSEL_MERCENARY_GIFT);

        if (item_slot == PROMPT_ABORT || item_slot == PROMPT_NOTHING)
        {
            canned_msg(MSG_OK);
            return false;
        }
        _caravan_gift_items_to(mons, item_slot);
        return false;
    };
    desc_menu.show();
    redraw_screen();
    return true;
}
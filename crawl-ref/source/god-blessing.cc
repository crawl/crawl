/**
 * @file
 * @brief Functions for gods blessing followers.
 **/

#include "AppHdr.h"

#include "god-blessing.h"

#include "artefact.h"
#include "env.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "makeitem.h"
#include "message.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "religion.h"
#include "stringutil.h"
#include "view.h"

/**
 * Given the weapon type, find an upgrade.
 *
 * @param old_type the current weapon_type of the weapon.
 * @param has_shield whether we can go from 1h -> 2h.
 * @param highlevel whether the orc is a strong type.
 * @returns a new weapon type (or sometimes the old one).
 */
// TODO: a less awful way of doing this?
static int _upgrade_weapon_type(int old_type, bool has_shield, bool highlevel)
{
    switch (old_type)
    {
        case WPN_CLUB:
        case WPN_WHIP:
        case WPN_MACE:        return WPN_FLAIL;
        case WPN_FLAIL:       return WPN_MORNINGSTAR;
        case WPN_MORNINGSTAR: return !has_shield ? WPN_DIRE_FLAIL  :
                                     highlevel   ? WPN_EVENINGSTAR :
                                                   WPN_MORNINGSTAR;
        case WPN_DIRE_FLAIL:  return WPN_GREAT_MACE;

        case WPN_DAGGER:      return WPN_FALCHION;
        case WPN_SHORT_SWORD: return WPN_LONG_SWORD;
        case WPN_RAPIER:      return WPN_SCIMITAR;
        case WPN_FALCHION:    return WPN_LONG_SWORD;
        case WPN_LONG_SWORD:  return WPN_SCIMITAR;
        case WPN_SCIMITAR:    return !has_shield ? WPN_GREAT_SWORD   :
                                     highlevel   ? WPN_DOUBLE_SWORD :
                                                   WPN_SCIMITAR;
        case WPN_GREAT_SWORD: return highlevel ? WPN_TRIPLE_SWORD :
                                                 WPN_GREAT_SWORD;

        case WPN_HAND_AXE:    return WPN_WAR_AXE;
            // Low level orcs shouldn't get fairly rare items.
        case WPN_WAR_AXE:     return !highlevel  ? WPN_WAR_AXE   :
                                     has_shield  ? WPN_BROAD_AXE :
                                                   WPN_BATTLEAXE;
        case WPN_BATTLEAXE:   return highlevel ? WPN_EXECUTIONERS_AXE :
                                                 WPN_BATTLEAXE;

        case WPN_SPEAR:       return WPN_TRIDENT;
        case WPN_TRIDENT:     return has_shield ? WPN_TRIDENT : WPN_HALBERD;
        case WPN_HALBERD:     return WPN_GLAIVE;
        case WPN_GLAIVE:      return highlevel ? WPN_BARDICHE : WPN_GLAIVE;

        case WPN_HUNTING_SLING: return WPN_FUSTIBALUS;
        case WPN_HAND_CROSSBOW: return !has_shield ? WPN_ARBALEST :
                                                     WPN_HAND_CROSSBOW;
        case WPN_ARBALEST:      return highlevel ? WPN_TRIPLE_CROSSBOW :
                                                   WPN_ARBALEST;
        case WPN_SHORTBOW:      return highlevel ? WPN_LONGBOW : WPN_SHORTBOW;

        default:              return old_type;
    }
}

/**
 * Try to upgrade an orc's melee weapon or launcher.
 *
 * @param mon The owner of the weapon.
 * @param wpn The weapon to upgrade.
 * @returns True if the weapon was upgraded.
 */
static bool _upgrade_orc_weapon(monster* mon, item_def& wpn)
{
    const int old_weapon_type = wpn.sub_type;

    // lower chance of upgrading to very good weapon types
    bool highlevel_ok = mon->type == MONS_ORC_KNIGHT && one_chance_in(6)
                        || mon->type == MONS_ORC_WARLORD && one_chance_in(3);

    wpn.sub_type = _upgrade_weapon_type(wpn.sub_type,
                                        mon->inv[MSLOT_SHIELD] != NON_ITEM,
                                        highlevel_ok);

    return wpn.sub_type != old_weapon_type;
}

/**
 * Get a weapon for beogh to gift to a weaponless orc.
 *
 * Tries to give something decent, but worse than player gifts.
 *
 * @param[in] mon_type   The type of orc to gift to.
 * @return               A weapon type to gift.
 */
static int _orc_weapon_gift_type(monster_type mon_type)
{
    switch (mon_type)
    {
        case MONS_ORC:
        case MONS_ORC_WIZARD:
        case MONS_ORC_PRIEST:
        case MONS_ORC_HIGH_PRIEST:
        case MONS_ORC_SORCERER:
            return random_choose_weighted(2, WPN_HAND_AXE, // orcs love axes
                                          1, WPN_SHORT_SWORD,
                                          1, WPN_MACE);
        case MONS_ORC_WARRIOR:
        case MONS_ORC_KNIGHT:
        case MONS_ORC_WARLORD:
            return random_choose_weighted(2, WPN_WAR_AXE, // orcs love axes
                                          1, WPN_LONG_SWORD,
                                          1, WPN_FLAIL,
                                          1, WPN_SPEAR);
            // give a lower tier of polearms; reaching is good on followers
        default:
            return WPN_CLUB; // shouldn't ever come up?
    }
}

/**
 * Give a weapon to an orc that doesn't have one.
 *
 * @param[in] orc    The orc to give a weapon to.
 * @param weapon_type  What weapon type to give the orc.
 */
static void _gift_weapon_to_orc(monster* orc, int weapon_type)
{
    ASSERT(weapon_type != NUM_WEAPONS);

    item_def weapon;
    weapon.base_type = OBJ_WEAPONS;
    weapon.sub_type = weapon_type;
    weapon.quantity = 1;
    set_ident_flags(weapon, ISFLAG_IDENT_MASK);
    give_specific_item(orc, weapon);
}

/**
 * Attempt to give a follower appropriate ammo.
 *
 * @param[in] orc               The orc to give ammo to.
 */
void gift_ammo_to_orc(monster* orc)
{
    const item_def* launcher = orc->launcher();

    item_def ammo;
    ammo.base_type = OBJ_MISSILES;

    if (!launcher)
        ammo.sub_type = MI_BOOMERANG;
    else
        ammo.sub_type = fires_ammo_type(*launcher);

    if (ammo.sub_type == MI_STONE)
        ammo.sub_type = MI_SLING_BULLET; // ugly special case

    ammo.quantity = 10 + random2(10);

    const item_def* old_ammo = orc->missiles();
    // don't give a drop message - it'd come before the bless message
    if (old_ammo && !items_stack(*old_ammo, ammo) &&
        !orc->drop_item(MSLOT_MISSILE, false))
    {
        return; // can't force them to drop the ammo, for some reason?
    }

    set_ident_flags(ammo, ISFLAG_IDENT_MASK);

    give_specific_item(orc, ammo);
}

/**
 * Attempt to bless a follower's melee weapon.
 *
 * @param[in] mon      The follower whose weapon should be blessed.
 * @return             The type of blessing given; may be empty.
 */
static string _beogh_bless_melee_weapon(monster* mon)
{
    bool blessed = false;
    item_def* wpn_ptr = mon->melee_weapon();
    ASSERT(wpn_ptr != nullptr);
    item_def& wpn = *wpn_ptr;

    // 50% chance of upgrading weapon type.
    if (!is_artefact(wpn)
        && coinflip()
        && _upgrade_orc_weapon(mon, wpn))
    {
        blessed = true;
    }
    // Enchant it. (Lower odds at high weapon enchantment.)
    if (!x_chance_in_y(wpn.plus, MAX_WPN_ENCHANT)
        && enchant_weapon(wpn, true))
    {
        set_ident_flags(wpn, ISFLAG_KNOW_PLUSES);
        blessed = true;
    }

    if (!blessed)
    {
        dprf("Couldn't bless follower's weapon!");
        return "";
    }

    item_set_appearance(wpn);
    return "superior armament";
}

/**
 * Attempt to give a follower a ranged weapon/ammo.
 *
 * @param[in] mon      The follower who should be blessed.
 * @return             The type of blessing given; may be empty.
 */
static string _beogh_bless_ranged_weapon(monster* mon)
{
    bool blessed = false;
    const bool mon_has_launcher = mon->launcher() != nullptr;

    if (mon_has_launcher)
    {
        item_def& launcher = *mon->launcher();

        // 50% chance of upgrading weapon type.
        if (!is_artefact(launcher)
            && coinflip()
            && _upgrade_orc_weapon(mon, launcher))
        {
           blessed = true;
        }
        // Enchant it. (Lower odds at high weapon enchantment.)
        if (!x_chance_in_y(launcher.plus, MAX_WPN_ENCHANT)
            && enchant_weapon(launcher, true))
        {
            set_ident_flags(launcher, ISFLAG_KNOW_PLUSES);
            blessed = true;
        }

        // Otherwise gift ammunition.
        if (!blessed)
        {
            gift_ammo_to_orc(mon);
            if (mon->missiles() != nullptr)
                return "ammunition";

            dprf("Couldn't give ammo to follower!");
            return ""; // ?
        }
        else
        {
            item_set_appearance(launcher);
            return "superior armament";
        }
    }

    // If they have a shield but no launcher, give boomerangs.
    if (mon->shield() != nullptr)
    {
        gift_ammo_to_orc(mon);
        if (mon->missiles() != nullptr)
            return "ranged armament";

        dprf("Couldn't give ammo to follower!");
        return ""; // ?
    }

    // No launcher, no shield: give them a crossbow & some ammo.
    _gift_weapon_to_orc(mon, WPN_ARBALEST);
    if (mon->launcher() == nullptr)
    {
        dprf("Couldn't give crossbow to follower!");
        return ""; // ?
    }

    gift_ammo_to_orc(mon);
    if (mon->missiles() == nullptr)
        dprf("Couldn't give initial ammo to follower");
    return "ranged armament";
}

/**
 * Attempt to bless a follower's weapon.
 *
 * @param[in] mon      The follower whose weapon should be blessed.
 * @return             The type of blessing given; may be empty.
 */
static string _beogh_bless_weapon(monster* mon)
{
    const item_def* wpn_ptr = mon->melee_weapon();
    if (wpn_ptr == nullptr)
    {
        _gift_weapon_to_orc(mon, _orc_weapon_gift_type(mon->type));
        if (mon->weapon() != nullptr)
            return "armament";

        dprf("Couldn't give a weapon to follower!");
        return ""; // ?
    }

    const item_def* launch_ptr = mon->launcher();
    const item_def* ammo_ptr = mon->missiles();
    if (launch_ptr != nullptr && ammo_ptr == nullptr)
    {
        gift_ammo_to_orc(mon);
        if (mon->missiles() != nullptr)
            return "ammunition";

        dprf("Couldn't give ammo to follower!");
        return ""; // ?
    }

    if (coinflip())
        return _beogh_bless_melee_weapon(mon);
    return _beogh_bless_ranged_weapon(mon);
}

static void _upgrade_shield(item_def &sh)
{
    // Promote from buckler up through tower shield.
    if (sh.sub_type >= ARM_FIRST_SHIELD && sh.sub_type < ARM_LAST_SHIELD)
        sh.sub_type++;
}

static void _upgrade_body_armour(item_def &arm)
{
    const auto type = static_cast<armour_type>(arm.sub_type);

    // Promote from robe up through plate.
    if (type >= ARM_FIRST_MUNDANE_BODY && type < ARM_LAST_MUNDANE_BODY
        // These are supposed to be robe-only.
        && arm.brand != SPARM_ARCHMAGI
        && arm.brand != SPARM_RESISTANCE)
    {
        arm.sub_type++;
    }
}

/**
 * Give armour (a shield or body armour) to an orc that doesn't have one.
 *
 * @param[in] orc       The orc to give armour to.
 * @param[in] shield    Whether to give a shield (default is body armour)
 */
static void _gift_armour_to_orc(monster* orc, bool shield = false)
{
    const bool highlevel = orc->type == MONS_ORC_KNIGHT
                           || orc->type == MONS_ORC_WARLORD;

    item_def armour;
    armour.base_type = OBJ_ARMOUR;
    if (shield)
        armour.sub_type = highlevel ? ARM_KITE_SHIELD : ARM_BUCKLER;
    else
        armour.sub_type = highlevel ? ARM_SCALE_MAIL : ARM_RING_MAIL;
    armour.quantity = 1;
    set_ident_flags(armour, ISFLAG_IDENT_MASK);
    give_specific_item(orc, armour);
}

/**
 * Attempt to bless a follower's armour.
 *
 * @param[in] mon           The follower whose armour should be blessed.
 * @return                  The type of blessing; may be empty.
 */
static string _beogh_bless_armour(monster* mon)
{
    const int armour = mon->inv[MSLOT_ARMOUR];
    const int shield = mon->inv[MSLOT_SHIELD];

    // always give naked orcs armour, if possible
    if (armour == NON_ITEM)
    {
        _gift_armour_to_orc(mon);
        if (mon->inv[MSLOT_ARMOUR] != NON_ITEM)
            return "armour";
        dprf("Couldn't give armour to an orc!"); //?
        return "";
    }

    // Pick either a monster's armour or its shield.
    const item_def* melee_weap = mon->melee_weapon();
    const item_def* launcher = mon->launcher();
    const bool can_use_shield = (melee_weap == nullptr
                                 || mon->hands_reqd(*melee_weap) != HANDS_TWO)
                                && (launcher == nullptr
                                   || mon->hands_reqd(*launcher) != HANDS_TWO);
    const int slot = coinflip() && can_use_shield ? shield : armour;

    if (slot == NON_ITEM)
    {
        ASSERT(slot == shield);
        _gift_armour_to_orc(mon, true);
        if (mon->inv[MSLOT_SHIELD] != NON_ITEM)
            return "a shield";
        dprf("Couldn't give a shield to an orc!"); //?
        return "";
    }

    item_def& arm(env.item[slot]);

    const int old_subtype = arm.sub_type;
    // 50% chance of improving armour/shield type
    if (!is_artefact(arm) && coinflip())
    {
        if (slot == shield)
            _upgrade_shield(arm);
        else
            _upgrade_body_armour(arm);
    }

    // And enchant or uncurse it. (Lower chance for higher enchantment.)
    int ac_change;
    const bool enchanted = !x_chance_in_y(arm.plus, armour_max_enchant(arm))
                           && enchant_armour(ac_change, true, arm);

    if (enchanted)
        set_ident_flags(arm, ISFLAG_KNOW_PLUSES);

    if (!enchanted && old_subtype == arm.sub_type)
    {
        dprf("Couldn't enchant follower's armour!");
        return "";
    }

    item_set_appearance(arm);
    return "improved armour";
}

static bool _blessing_balms(monster* mon)
{
    // Remove poisoning, sickness and confusion, like a potion of
    // curing, but without the healing. Also, remove slowing and
    // fatigue.
    bool success = false;

    if (mon->del_ench(ENCH_POISON, true))
        success = true;

    if (mon->del_ench(ENCH_SICK, true))
        success = true;

    if (mon->del_ench(ENCH_CONFUSION, true))
        success = true;

    if (mon->del_ench(ENCH_SLOW, true))
        success = true;

    if (mon->del_ench(ENCH_FATIGUE, true))
        success = true;

    if (mon->del_ench(ENCH_WRETCHED, true))
        success = true;

    return success;
}

static bool _blessing_healing(monster* mon)
{
    const int healing = mon->max_hit_points / 4 + 1;

    // Heal a monster.
    if (mon->heal(healing + random2(healing + 1)))
    {
        // A high-HP monster might get a unique name.
        if (x_chance_in_y(mon->max_hit_points + 1, 100))
            give_monster_proper_name(*mon);
        return true;
    }

    return false;
}

static bool _increase_ench_duration(monster* mon,
                                    mon_enchant ench,
                                    const int increase)
{
    // Durations are saved as 16-bit signed ints, so clamp at the largest such.
    const int MARSHALL_MAX = (1 << 15) - 1;

    const int newdur = min(ench.duration + increase, MARSHALL_MAX);
    if (ench.duration >= newdur)
        return false;

    ench.duration = newdur;
    mon->update_ench(ench);

    return true;
}

static bool _tso_blessing_extend_stay(monster* mon)
{
    if (!mon->has_ench(ENCH_ABJ))
        return false;

    mon_enchant abj = mon->get_ench(ENCH_ABJ);

    // These numbers are tenths of a player turn. Holy monsters get a
    // much bigger boost than random beasties, which get at most double
    // their current summon duration.
    if (mon->is_holy())
        return _increase_ench_duration(mon, abj, 1100 + random2(1100));
    else
        return _increase_ench_duration(mon, abj, min(abj.duration,
                                                     500 + random2(500)));
}

static bool _tso_blessing_friendliness(monster* mon)
{
    if (!mon->has_ench(ENCH_CHARM))
        return false;

    // [ds] Just increase charm duration, no permanent friendliness.
    const int base_increase = 700;
    return _increase_ench_duration(mon, mon->get_ench(ENCH_CHARM),
                                   base_increase + random2(base_increase));
}

static void _beogh_reinf_callback(const mgen_data &mg, monster *&mon, int placed)
{
    UNUSED(placed);
    ASSERT(mg.god == GOD_BEOGH);

    // Beogh tries a second time to place reinforcements.
    if (!mon)
        mon = create_monster(mg);

    if (!mon)
        return;

    mon->flags |= MF_ATT_CHANGE_ATTEMPT;

    bool high_level = (mon->type == MONS_ORC_PRIEST
                       || mon->type == MONS_ORC_WARRIOR
                       || mon->type == MONS_ORC_KNIGHT);

    // For high level orcs, there's a chance of being named.
    if (high_level && one_chance_in(5))
        give_monster_proper_name(*mon);
}

// If you don't currently have any followers, send a small band to help
// you out.
static void _beogh_blessing_reinforcements()
{
    // Possible reinforcement.
    const monster_type followers[] =
    {
        MONS_ORC, MONS_ORC, MONS_ORC_WIZARD, MONS_ORC_PRIEST
    };

    const monster_type high_xl_followers[] =
    {
        MONS_ORC_PRIEST, MONS_ORC_WARRIOR, MONS_ORC_KNIGHT
    };

    // Send up to four followers.
    int how_many = random2(4) + 1;

    monster_type follower_type;
    for (int i = 0; i < how_many; ++i)
    {
        if (random2(you.experience_level) >= 9 && coinflip())
            follower_type = RANDOM_ELEMENT(high_xl_followers);
        else
            follower_type = RANDOM_ELEMENT(followers);

        delayed_monster(mgen_data(follower_type, BEH_FRIENDLY, you.pos(),
                                   MHITYOU)
                        .set_summoned(&you, 0, 0, GOD_BEOGH),
                        _beogh_reinf_callback);
    }
}

static bool _beogh_blessing_priesthood(monster* mon)
{
    if (mon->type != MONS_ORC)
        return false;

    mon->upgrade_type(MONS_ORC_PRIEST, true, true);
    return true;
}

/**
 * Attempt to bless a follower with curing and/or healing.
 *
 * @param[in] follower      The follower to heal.
 * @return                  The type of healing that occurred; may be empty.
 */
static string _bless_with_healing(monster* follower)
{
    string blessing = "";

    // Maybe try to cure status conditions.
    bool balms = false;
    if (coinflip())
    {
        balms = _blessing_balms(follower);
        if (balms)
            blessing = "divine balms";
        else
            dprf("Couldn't apply balms.");
    }

    // Heal the follower.
    bool healing = _blessing_healing(follower);

    // Maybe heal the follower again.
    if ((!healing || coinflip()) && _blessing_healing(follower))
        healing = true;

    if (healing)
    {
        if (balms)
            blessing += " and ";
        blessing += "healing";
    }
    else
        dprf("Couldn't heal monster.");

    return blessing;
}


/**
 * Print a message for a god blessing a follower.
 *
 * Also flash the screen, in local tiles.
 *
 * @param[in] follower  The follower being blessed.
 * @param god           The god doing the blessing.
 * @param blessing      The blessing being delivered.
 */
static void _display_god_blessing(monster* follower, god_type god,
                                  string blessing)
{
    ASSERT(follower); // XXX: change to monster &follower

    string whom = you.can_see(*follower) ? follower->name(DESC_THE)
    : "a follower";

    simple_god_message(make_stringf(" blesses %s with %s.",
                                    whom.c_str(), blessing.c_str()).c_str(),
                       god);
}

/**
 * Have Beogh attempt to bless the specified follower.
 *
 * If no follower is specified, there is a chance of sending reinforcements.
 *
 * @param[in] follower      The follower to try to bless.
 * @param[in] force         Whether to check follower validity.
 * @return Whether a blessing occurred.
 */
static bool _beogh_bless_follower(monster* follower, bool force)
{
    if (!follower)
    {
        // 1/20 chance of spawning a palband
        if (!one_chance_in(5))
            return false;

        // Don't gift orcs if Toxic radiance is up, to avoid the player being
        // penanced through no fault of their own.
        if (you.duration[DUR_TOXIC_RADIANCE])
            return false;

        // If no follower was found, attempt to send
        // reinforcements.
        _beogh_blessing_reinforcements();

        // Possibly send more reinforcements.
        if (coinflip())
            _beogh_blessing_reinforcements();

        delayed_monster_done("Beogh blesses you with reinforcements.");

        // Return true, even though the reinforcements might
        // not be placed.
        return true;
    }

    string blessing = "";

    // 10% chance of blessing to priesthood.
    if (force || one_chance_in(10))
    {
        if (_beogh_blessing_priesthood(follower))
            blessing = "priesthood";
        else
            dprf("Couldn't promote monster to priesthood");
    }

    // ~15% chance of blessing armament (assume that most priest buffs fail)
    if (blessing.empty() && mons_genus(follower->type) == MONS_ORC
        && (force || one_chance_in(7)))
    {
        blessing = coinflip() ? _beogh_bless_weapon(follower)
                              : _beogh_bless_armour(follower);
    }

    // If they got a good blessing (priesthood or equipment), maybe give them
    // a name.
    if (!blessing.empty())
        give_monster_proper_name(*follower);

    // ~85% chance of trying to heal.
    if (blessing.empty())
        blessing = _bless_with_healing(follower);

    if (blessing.empty())
        return false;

    _display_god_blessing(follower, GOD_BEOGH, blessing);
    return true;
}

/**
 * Attempt to increase the duration of a follower.
 *
 * Their summon duration, if summoned, or charm duration, if charmed.
 *
 * @param[in] follower    The follower to bless.
 * @return                The type of blessing that was given; may be empty.
 */
static string _tso_bless_duration(monster* follower)
{
    // Extend a monster's stay if it's abjurable, or extend charm
    // duration. If neither is possible, deliberately fall through.
    const bool more_time = _tso_blessing_extend_stay(follower);
    const bool friendliness = _tso_blessing_friendliness(follower);

    if (!more_time && !friendliness)
    {
        dprf("Couldn't increase monster's friendliness or summon time.");
        return "";
    }

    string blessing = "";
    if (friendliness)
    {
        blessing += "friendliness";
        if (more_time)
            blessing += " and ";
    }

    if (more_time)
        blessing += "more time in this world";

    return blessing;
}

/**
 * Have The Shining One attempt to bless the specified follower.
 *
 * Blessings can increase duration, or heal & cure.
 *
 * @param[in] follower      The follower to try to bless.
 * @param[in] force         Whether to check follower validity.
 * @return Whether a blessing occurred.
 */
static bool _tso_bless_follower(monster* follower, bool force)
{

    if (!follower || (!force && !is_follower(*follower)))
        return false;

    string blessing = "";
    if (blessing.empty() && coinflip())
        blessing = _bless_with_healing(follower);
    if (blessing.empty())
        blessing = _tso_bless_duration(follower);

    if (blessing.empty())
        return false;

    _display_god_blessing(follower, GOD_SHINING_ONE, blessing);
    return true;
}

static bool _is_friendly_follower(const monster& mon)
{
    return mon.friendly() && is_follower(mon);
}

// Bless the follower indicated in follower, if any. If there isn't
// one, bless a random follower within sight of the player, if any, or
// any follower on the level.
// Blessing can be enforced with a wizard mode command.
bool bless_follower(monster* follower,
                    god_type god,
                    bool force)
{
    // most blessings fail, tragically...
    if (!force && !one_chance_in(4))
        return false;

    // If a follower was specified, and it's suitable, pick it.
    // Otherwise, pick a random follower.
    // XXX: factor out into another function?
    if (!follower || (!force && !is_follower(*follower)))
    {
        // Choose a random follower in LOS, preferably a named or
        // priestly one.
        follower = choose_random_nearby_monster(0, _is_friendly_follower,
                                                god == GOD_BEOGH);
    }

    // Try *again*, on the entire level
    if (!follower)
    {
        follower = choose_random_monster_on_level(0, _is_friendly_follower,
                                                  god == GOD_BEOGH);
    }

    switch (god)
    {
        case GOD_BEOGH: return _beogh_bless_follower(follower, force);
        case GOD_SHINING_ONE:   return _tso_bless_follower(follower, force);
        default: return false; // XXX: print something here?
    }
}

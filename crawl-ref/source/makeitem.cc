/**
 * @file
 * @brief Item creation routines.
**/

#include "AppHdr.h"

#include "makeitem.h"

#include <algorithm>

#include "artefact.h"
#include "colour.h"
#include "decks.h"
#include "describe.h"
#include "dungeon.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "spl-book.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"

static armour_type _get_random_armour_type(int item_level);

int create_item_named(string name, coord_def p, string *error)
{
    trim_string(name);

    item_list ilist;
    const string err = ilist.add_item(name, false);
    if (!err.empty())
    {
        if (error)
            *error = err;
        return NON_ITEM;
    }

    item_spec ispec = ilist.get_item(0);
    int item = dgn_place_item(ispec, you.pos());
    if (item != NON_ITEM)
        link_items();
    else if (error)
        *error = "Failed to create item '" + name + "'";

    return item;
}

bool got_curare_roll(const int item_level)
{
    return one_chance_in(item_level > 27 ? 6   :
                         item_level < 2  ? 15  :
                         (364 - 7 * item_level) / 25);
}

/**
 * Initialize the randomized appearance of a given item.
 *
 * For e.g. wand names, potions colors, helmet tiles...
 *
 * XXX: could this be moved into a constructor or reset method...?
 *
 * @param item  The item to have its appearance initialized.
 */
void item_colour(item_def &item)
{
    // Compute random tile/colour choice.
    item.rnd = 1 + random2(255); // reserve 0 for uninitialized

    if (is_unrandom_artefact(item) && !is_randapp_artefact(item))
        return; // don't stomp on item.special!

    // initialize item appearance.
    // we don't actually *need* to store this now, but we *do* need to store
    // it in appearance at some point, since sub_type isn't public information
    // for un-id'd items, and therefore can't be used to do a lookup at the
    // time item names/colours are calculated.
    // it would probably be better to store this at the time that item_info is
    // generated (get_item_info), but that requires some save compat work (and
    // would be wrong if we ever try to get item colour/names directly...?)
    // possibly a todo for a later date.

    // TODO: move this into a lookup table
    switch (item.base_type)
    {
    case OBJ_WANDS:
        item.appearance = you.item_description[IDESC_WANDS][item.sub_type];
        break;

    case OBJ_POTIONS:
        item.appearance = you.item_description[IDESC_POTIONS][item.sub_type];
        break;

    case OBJ_JEWELLERY:
        item.appearance = you.item_description[IDESC_RINGS][item.sub_type];
        break;

    case OBJ_SCROLLS:
        item.appearance = you.item_description[IDESC_SCROLLS][item.sub_type];
        break;

    case OBJ_STAVES:
        item.appearance = you.item_description[IDESC_STAVES][item.sub_type];
        break;

    default:
        break;
    }
}

// Does Xom consider an item boring?
static bool _is_boring_item(int type, int sub_type)
{
    switch (type)
    {
    case OBJ_POTIONS:
        return sub_type == POT_CURE_MUTATION;
    case OBJ_SCROLLS:
        // These scrolls increase knowledge and thus reduce risk.
        switch (sub_type)
        {
        case SCR_REMOVE_CURSE:
        case SCR_IDENTIFY:
        case SCR_MAGIC_MAPPING:
            return true;
        default:
            break;
        }
        break;
    case OBJ_JEWELLERY:
        return sub_type == RING_TELEPORT_CONTROL
               || sub_type == AMU_RESIST_MUTATION;
    default:
        break;
    }
    return false;
}

static weapon_type _determine_weapon_subtype(int item_level)
{
    if (item_level > 6 && one_chance_in(30)
        && x_chance_in_y(10 + item_level, 100))
    {
        return random_choose(WPN_LAJATANG,
                             WPN_GREATSLING,
                             WPN_TRIPLE_CROSSBOW,
                             WPN_DEMON_WHIP,
                             WPN_DEMON_BLADE,
                             WPN_DEMON_TRIDENT,
                             WPN_DOUBLE_SWORD,
                             WPN_EVENINGSTAR,
                             WPN_EXECUTIONERS_AXE,
                             WPN_QUICK_BLADE,
                             WPN_TRIPLE_SWORD,
                             -1);
    }
    else if (x_chance_in_y(item_level, 20))
    {
        // Pick a weapon based on rarity.
        while (true)
        {
            const int wpntype = random2(NUM_WEAPONS);

            if (x_chance_in_y(weapon_rarity(wpntype), 10))
                return static_cast<weapon_type>(wpntype);
        }
    }
    else if (x_chance_in_y(item_level, item_level+7))
    {
        return random_choose(WPN_QUARTERSTAFF,
                             WPN_FALCHION,
                             WPN_LONG_SWORD,
                             WPN_WAR_AXE,
                             WPN_TRIDENT,
                             WPN_FLAIL,
                             WPN_RAPIER,
                             -1);
    }
    else
    {
        return random_choose(WPN_HUNTING_SLING,
                             WPN_SPEAR,
                             WPN_HAND_AXE,
                             WPN_MACE,
                             // Not worth _weighted for one doubled type.
                             WPN_DAGGER, WPN_DAGGER,
                             WPN_CLUB,
                             WPN_WHIP,
                             WPN_SHORT_SWORD,
                             -1);
    }
}

static bool _try_make_item_unrand(item_def& item, int force_type)
{
    if (player_in_branch(BRANCH_PANDEMONIUM))
        return false;

    int idx = find_okay_unrandart(item.base_type, force_type,
                                  player_in_branch(BRANCH_ABYSS));

    if (idx != -1 && make_item_unrandart(item, idx))
        return true;

    return false;
}

// Return whether we made an artefact.
static bool _try_make_weapon_artefact(item_def& item, int force_type,
                                      int item_level, bool force_randart = false)
{
    if (item_level > 2 && x_chance_in_y(101 + item_level * 3, 4000)
        || force_randart)
    {
        // Make a randart or unrandart.

        // 1 in 20 randarts are unrandarts.
        if (one_chance_in(item_level == MAKE_GOOD_ITEM ? 7 : 20)
            && !force_randart)
        {
            if (_try_make_item_unrand(item, force_type))
                return true;
        }

        // Small clubs are never randarts.
        if (item.sub_type == WPN_CLUB)
            return false;

        // The rest are normal randarts.
        make_item_randart(item);
        // Mean enchantment +6.
        item.plus = 12 - biased_random2(7,2) - biased_random2(7,2) - biased_random2(7,2);

        if (one_chance_in(5))
        {
            do_curse_item(item);
            item.plus = 3 - random2(6);
        }
        else if (item.plus < 0 && !one_chance_in(3))
            do_curse_item(item);

        if (get_weapon_brand(item) == SPWPN_HOLY_WRATH)
            item.flags &= (~ISFLAG_CURSED);
        return true;
    }

    return false;
}

static brand_type _determine_weapon_brand(const item_def& item, int item_level)
{
    // Forced ego.
    if (item.special != 0)
        return static_cast<brand_type>(item.special);

    const bool force_good = item_level >= MAKE_GIFT_ITEM;
    const int tries       = force_good ? 5 : 1;
    brand_type rc         = SPWPN_NORMAL;

    for (int count = 0; count < tries && rc == SPWPN_NORMAL; ++count)
    {
        if (!force_good && !is_demonic(item)
            && !x_chance_in_y(101 + item_level, 300))
        {
            continue;
        }

        // We are not guaranteed to have a special set by the end of
        // this.
        switch (item.sub_type)
        {
        case WPN_EVENINGSTAR:
            if (coinflip())
                rc = SPWPN_DRAINING;
            // **** intentional fall through here ****
        case WPN_MORNINGSTAR:
            if (one_chance_in(4))
                rc = SPWPN_VENOM;

            if (one_chance_in(4))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(20))
                rc = SPWPN_VAMPIRISM;
            // **** intentional fall through here ****
        case WPN_MACE:
        case WPN_GREAT_MACE:
        case WPN_FLAIL:
        case WPN_DIRE_FLAIL:
        case WPN_HAMMER:
            if (one_chance_in(25))
                rc = SPWPN_ANTIMAGIC;

            if (one_chance_in(25))
                rc = SPWPN_PAIN;

            if (one_chance_in(25))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(3) && (rc == SPWPN_NORMAL || one_chance_in(5)))
                rc = SPWPN_VORPAL;

            if (one_chance_in(4))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(3))
                rc = SPWPN_PROTECTION;

            if (one_chance_in(10))
                rc = SPWPN_DRAINING;
            break;

        case WPN_DAGGER:
                                    // total weight 100
            rc = random_choose_weighted(28, SPWPN_VENOM,
                                        20, SPWPN_NORMAL,
                                        10, SPWPN_SPEED,
                                         9, SPWPN_DRAINING,
                                         6, SPWPN_PROTECTION,
                                         6, SPWPN_ELECTROCUTION,
                                         5, SPWPN_HOLY_WRATH,
                                         4, SPWPN_VAMPIRISM,
                                         4, SPWPN_FLAMING,
                                         4, SPWPN_FREEZING,
                                         2, SPWPN_PAIN,
                                         1, SPWPN_DISTORTION,
                                         1, SPWPN_ANTIMAGIC,
                                         0);
            break;

        case WPN_SHORT_SWORD:
        case WPN_RAPIER:
                                    // total weight 100
            rc = random_choose_weighted(33, SPWPN_NORMAL,
                                        17, SPWPN_VENOM,
                                        10, SPWPN_SPEED,
                                         9, SPWPN_DRAINING,
                                         6, SPWPN_PROTECTION,
                                         6, SPWPN_ELECTROCUTION,
                                         5, SPWPN_HOLY_WRATH,
                                         4, SPWPN_VAMPIRISM,
                                         4, SPWPN_FLAMING,
                                         4, SPWPN_FREEZING,
                                         1, SPWPN_DISTORTION,
                                         1, SPWPN_ANTIMAGIC,
                                         0);
            break;

        case WPN_FALCHION:
        case WPN_LONG_SWORD:
        case WPN_SCIMITAR:
        case WPN_GREAT_SWORD:
        case WPN_DOUBLE_SWORD:
        case WPN_TRIPLE_SWORD:
                                    // total weight 100
            rc = random_choose_weighted(23, SPWPN_HOLY_WRATH,
                                        19, SPWPN_NORMAL,
                                        15, SPWPN_VORPAL,
                                        10, SPWPN_ELECTROCUTION,
                                         8, SPWPN_PROTECTION,
                                         5, SPWPN_FREEZING,
                                         5, SPWPN_FLAMING,
                                         5, SPWPN_DRAINING,
                                         4, SPWPN_VAMPIRISM,
                                         2, SPWPN_VENOM,
                                         2, SPWPN_DISTORTION,
                                         1, SPWPN_PAIN,
                                         1, SPWPN_ANTIMAGIC,
                                         0);
            break;

        case WPN_WAR_AXE:
        case WPN_HAND_AXE:
        case WPN_BROAD_AXE:
        case WPN_BATTLEAXE:
        case WPN_EXECUTIONERS_AXE:
                                    // total weight 100
            rc = random_choose_weighted(31, SPWPN_NORMAL,
                                        16, SPWPN_VORPAL,
                                        11, SPWPN_ELECTROCUTION,
                                        10, SPWPN_FREEZING,
                                        10, SPWPN_FLAMING,
                                         8, SPWPN_VENOM,
                                         5, SPWPN_VAMPIRISM,
                                         3, SPWPN_DRAINING,
                                         2, SPWPN_DISTORTION,
                                         2, SPWPN_ANTIMAGIC,
                                         1, SPWPN_PAIN,
                                         1, SPWPN_HOLY_WRATH,
                                         0);
            break;

        case WPN_WHIP:
                                    // total weight 100
            rc = random_choose_weighted(34, SPWPN_NORMAL,
                                        16, SPWPN_VENOM,
                                        16, SPWPN_ELECTROCUTION,
                                         7, SPWPN_DRAINING,
                                         6, SPWPN_FLAMING,
                                         6, SPWPN_FREEZING,
                                         5, SPWPN_VAMPIRISM,
                                         4, SPWPN_PAIN,
                                         3, SPWPN_HOLY_WRATH,
                                         2, SPWPN_DISTORTION,
                                         1, SPWPN_ANTIMAGIC,
                                           0);
            break;

        case WPN_HALBERD:
        case WPN_GLAIVE:
        case WPN_SCYTHE:
        case WPN_TRIDENT:
        case WPN_BARDICHE:
            if (one_chance_in(30))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(4))
                rc = SPWPN_PROTECTION;
            // **** intentional fall through here ****
        case WPN_SPEAR:
            if (one_chance_in(25))
                rc = SPWPN_ANTIMAGIC;

            if (one_chance_in(25))
                rc = SPWPN_PAIN;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRISM;

            if (one_chance_in(25))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5) && (rc == SPWPN_NORMAL || one_chance_in(6)))
                rc = SPWPN_VORPAL;

            if (one_chance_in(6))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(6))
                rc = SPWPN_VENOM;
            break;

        case WPN_HUNTING_SLING:
        case WPN_GREATSLING:
        case WPN_SHORTBOW:
        case WPN_LONGBOW:
        case WPN_HAND_CROSSBOW:
        case WPN_ARBALEST:
            if (coinflip())
                continue;
            // intentionally fallthrough to normal launcher brands
        case WPN_TRIPLE_CROSSBOW:
                                    // total weight 100
            rc = random_choose_weighted(48, SPWPN_FLAMING,
                                        25, SPWPN_FREEZING,
                                        15, SPWPN_EVASION,
                                        11, SPWPN_VORPAL,
                                         1, SPWPN_NORMAL,
                                         0);
            break;

        case WPN_BLOWGUN:
            if (one_chance_in(30))
                rc = SPWPN_EVASION;
            break;

        // Staves
        case WPN_QUARTERSTAFF:
                                  // Total weight 100
        rc = random_choose_weighted(50, SPWPN_NORMAL,
                                    18, SPWPN_PROTECTION,
                                     8, SPWPN_DRAINING,
                                     8, SPWPN_VORPAL,
                                     8, SPWPN_SPEED,
                                     2, SPWPN_DISTORTION,
                                     2, SPWPN_PAIN,
                                     2, SPWPN_HOLY_WRATH,
                                     2, SPWPN_ANTIMAGIC,
                                       0);
            break;

        case WPN_LAJATANG:
                                        // total weight 100
            rc = random_choose_weighted(34, SPWPN_NORMAL,
                                        12, SPWPN_SPEED,
                                        12, SPWPN_ELECTROCUTION,
                                        12, SPWPN_VAMPIRISM,
                                         9, SPWPN_PROTECTION,
                                         7, SPWPN_VENOM,
                                         7, SPWPN_PAIN,
                                         4, SPWPN_ANTIMAGIC,
                                         3, SPWPN_DISTORTION,
                                        0);
            break;

        case WPN_DEMON_WHIP:
        case WPN_DEMON_BLADE:
        case WPN_DEMON_TRIDENT:
                                        // total weight 100
            rc = random_choose_weighted(27, SPWPN_NORMAL,
                                        20, SPWPN_VENOM,
                                        16, SPWPN_ELECTROCUTION,
                                        13, coinflip() ? SPWPN_FLAMING
                                                       : SPWPN_FREEZING,
                                        10, SPWPN_DRAINING,
                                         7, SPWPN_VAMPIRISM,
                                         4, SPWPN_PAIN,
                                         3, SPWPN_ANTIMAGIC,
                                         0);
            break;

        case WPN_EUDEMON_BLADE:      // special gifts of TSO
        case WPN_SACRED_SCOURGE:
        case WPN_TRISHULA:
            rc = SPWPN_HOLY_WRATH;
            break;

        default:
            // unlisted weapons have no associated, standard ego-types {dlb}
            break;
        }
    }

    ASSERT(is_weapon_brand_ok(item.sub_type, rc, true));
    return rc;
}

// Reject brands which are outright bad for the item.  Unorthodox combinations
// are ok, since they can happen on randarts.
bool is_weapon_brand_ok(int type, int brand, bool strict)
{
    item_def item;
    item.base_type = OBJ_WEAPONS;
    item.sub_type = type;

    if (brand <= SPWPN_NORMAL)
        return true;

    if (type == WPN_QUICK_BLADE && brand == SPWPN_SPEED)
        return false;

    if (type == WPN_BLOWGUN)
    {
        switch ((brand_type)brand)
        {
        case SPWPN_NORMAL:
        case SPWPN_PROTECTION:
        case SPWPN_SPEED:
        case SPWPN_EVASION:
            return true;
        default:
            return false;
        }
    }

    switch ((brand_type)brand)
    {
    // Universal brands.
    case SPWPN_NORMAL:
    case SPWPN_VENOM:
    case SPWPN_PROTECTION:
    case SPWPN_SPEED:
    case SPWPN_VORPAL:
    case SPWPN_CHAOS:
    case SPWPN_HOLY_WRATH:
    case SPWPN_ELECTROCUTION:
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:
        break;

    // Melee-only brands.
    case SPWPN_DRAINING:
    case SPWPN_VAMPIRISM:
    case SPWPN_PAIN:
    case SPWPN_DISTORTION:
    case SPWPN_ANTIMAGIC:
    case SPWPN_REAPING:
        if (is_range_weapon(item))
            return false;
        break;

    // Ranged-only brands.
    case SPWPN_PENETRATION:
    case SPWPN_EVASION:
        if (!is_range_weapon(item))
            return false;
        break;

#if TAG_MAJOR_VERSION == 34
    // Removed brands.
    case SPWPN_RETURNING:
    case SPWPN_REACHING:
    case SPWPN_ORC_SLAYING:
    case SPWPN_FLAME:
    case SPWPN_FROST:
    case SPWPN_DRAGON_SLAYING:
        return false;
#endif

    case SPWPN_ACID:
    case SPWPN_CONFUSE:
    case SPWPN_FORBID_BRAND:
    case SPWPN_DEBUG_RANDART:
    case NUM_SPECIAL_WEAPONS:
    case NUM_REAL_SPECIAL_WEAPONS:
        die("invalid brand %d on weapon %d (%s)", brand, type,
            item.name(DESC_PLAIN).c_str());
        break;
    }

    return true;
}

static void _roll_weapon_type(item_def& item, int item_level)
{
    int i;
    for (i = 0; i < 1000; ++i)
    {
        item.sub_type = _determine_weapon_subtype(item_level);
        if (is_weapon_brand_ok(item.sub_type, item.special, true))
            break;
    }
    if (i == 1000)
        item.special = SPWPN_NORMAL; // fall back to no brand
}

static void _generate_weapon_item(item_def& item, bool allow_uniques,
                                  int force_type, int item_level)
{
    // Determine weapon type.
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
        _roll_weapon_type(item, item_level);

    // Forced randart.
    if (item_level == ISPEC_RANDART)
    {
        int i;
        int ego = item.special;
        for (i = 0; i < 100; ++i)
            if (_try_make_weapon_artefact(item, force_type, 0, true) && is_artefact(item))
            {
                if (ego > SPWPN_NORMAL)
                    item.props[ARTEFACT_PROPS_KEY].get_vector()[ARTP_BRAND].get_short() = ego;
                if (randart_is_bad(item)) // recheck, the brand changed
                {
                    force_type = item.sub_type;
                    item.clear();
                    item.base_type = OBJ_WEAPONS;
                    item.sub_type = force_type;
                    continue;
                }
                return;
            }
        // fall back to an ordinary item
        item_level = MAKE_GOOD_ITEM;
    }

    // If we make the unique roll, no further generation necessary.
    if (allow_uniques
        && _try_make_weapon_artefact(item, force_type, item_level))
    {
        return;
    }

    ASSERT(!is_artefact(item));

    // Artefacts handled, let's make a normal item.
    const bool force_good = item_level >= MAKE_GIFT_ITEM;
    const bool forced_ego = item.special > 0;
    const bool no_brand   = item.special == SPWPN_FORBID_BRAND;

    if (no_brand)
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);

    // If it's forced to be a good item, reroll clubs.
    while (force_good && force_type == OBJ_RANDOM && item.sub_type == WPN_CLUB)
        _roll_weapon_type(item, item_level);

    item.plus = 0;

    if (item_level < 0)
    {
        // Thoroughly damaged, could had been good once.
        if (!no_brand && (forced_ego || one_chance_in(4)))
        {
            // Brand is set as for "good" items.
            set_item_ego_type(item, OBJ_WEAPONS,
                _determine_weapon_brand(item, 2 + 2 * env.absdepth0));
        }
        item.plus -= 1 + random2(3);

        if (item_level == ISPEC_BAD)
            do_curse_item(item);
    }
    else if ((force_good || is_demonic(item) || forced_ego
                    || x_chance_in_y(51 + item_level, 200))
                && (!item.is_mundane() || force_good))
    {
        // Make a better item (possibly ego).
        if (!no_brand)
        {
            set_item_ego_type(item, OBJ_WEAPONS,
                              _determine_weapon_brand(item, item_level));
        }

        // if acquired item still not ego... enchant it up a bit.
        if (force_good && item.special == SPWPN_NORMAL)
            item.plus += 2 + random2(3);

        const int chance = (force_good ? 200 : item_level);

        // Odd-looking, but this is how the algorithm compacts {dlb}.
        for (int i = 0; i < 4; ++i)
        {
            item.plus += random2(3);

            if (random2(425) > 35 + chance)
                break;
        }

        // squash boring items.
        if (!force_good && item.special == SPWPN_NORMAL && item.plus < 3)
            item.plus = 0;
    }
    else
    {
        if (one_chance_in(12))
        {
            // Make a cursed item.
            do_curse_item(item);
            item.plus  -= random2(4);
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
        }
    }
}

// Current list is based on dpeg's original post to the Wiki, found at the
// page: <http://crawl.develz.org/wiki/doku.php?id=DCSS%3Aissue%3A41>.
// Remember to update the code in is_missile_brand_ok if adding or altering
// brands that are applied to missiles. {due}
static special_missile_type _determine_missile_brand(const item_def& item,
                                                     int item_level)
{
    // Forced ego.
    if (item.special != 0)
        return static_cast<special_missile_type>(item.special);

    const bool force_good = item_level >= MAKE_GIFT_ITEM;
    special_missile_type rc = SPMSL_NORMAL;

    // "Normal weight" of SPMSL_NORMAL.
    int nw = force_good ? 0 : random2(2000 - 55 * item_level);

    switch (item.sub_type)
    {
    case MI_NEEDLE:
        // Curare is special cased, all the others aren't.
        if (got_curare_roll(item_level))
        {
            rc = SPMSL_CURARE;
            break;
        }

        rc = random_choose_weighted(20, SPMSL_SLEEP,
                                    20, SPMSL_SLOW,
                                    20, SPMSL_CONFUSION,
                                    10, SPMSL_PARALYSIS,
                                    10, SPMSL_FRENZY,
                                    nw, SPMSL_POISONED,
                                    0);
        break;
    case MI_ARROW:
        rc = random_choose_weighted(30, SPMSL_FLAME,
                                    30, SPMSL_FROST,
                                    20, SPMSL_POISONED,
                                    15, SPMSL_DISPERSAL,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
    case MI_BOLT:
        rc = random_choose_weighted(30, SPMSL_FLAME,
                                    30, SPMSL_FROST,
                                    20, SPMSL_POISONED,
                                    15, SPMSL_PENETRATION,
                                    15, SPMSL_SILVER,
                                    10, SPMSL_STEEL,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
    case MI_JAVELIN:
        rc = random_choose_weighted(30, SPMSL_RETURNING,
                                    32, SPMSL_PENETRATION,
                                    32, SPMSL_POISONED,
                                    21, SPMSL_STEEL,
                                    20, SPMSL_SILVER,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
    case MI_TOMAHAWK:
        rc = random_choose_weighted(15, SPMSL_POISONED,
                                    10, SPMSL_SILVER,
                                    10, SPMSL_STEEL,
                                    12, SPMSL_DISPERSAL,
                                    28, SPMSL_RETURNING,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
#if TAG_MAJOR_VERSION == 34
    case MI_DART:
#endif
    case MI_STONE:
        // deliberate fall through
    case MI_LARGE_ROCK:
        // Stones get no brands. Slings may be branded.
        rc = SPMSL_NORMAL;
        break;
    case MI_SLING_BULLET:
        rc = random_choose_weighted(30, SPMSL_FLAME,
                                    30, SPMSL_FROST,
                                    20, SPMSL_POISONED,
                                    15, SPMSL_STEEL,
                                    15, SPMSL_SILVER,
                                    20, SPMSL_EXPLODING,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
    case MI_THROWING_NET:
        rc = random_choose_weighted(30, SPMSL_STEEL,
                                    30, SPMSL_SILVER,
                                    nw, SPMSL_NORMAL,
                                    0);
        break;
    }

    ASSERT(is_missile_brand_ok(item.sub_type, rc, true));

    return rc;
}

bool is_missile_brand_ok(int type, int brand, bool strict)
{
    // Stones can never be branded.
    if ((type == MI_STONE || type == MI_LARGE_ROCK) && brand != SPMSL_NORMAL
        && strict)
    {
        return false;
    }

    // In contrast, needles should always be branded.
    // And all of these brands save poison are unique to needles.
    switch (brand)
    {
    case SPMSL_POISONED:
        if (type == MI_NEEDLE)
            return true;
        break;

    case SPMSL_CURARE:
    case SPMSL_PARALYSIS:
    case SPMSL_SLOW:
    case SPMSL_SLEEP:
    case SPMSL_CONFUSION:
#if TAG_MAJOR_VERSION == 34
    case SPMSL_SICKNESS:
#endif
    case SPMSL_FRENZY:
        return type == MI_NEEDLE;

#if TAG_MAJOR_VERSION == 34
    case SPMSL_BLINDING:
        // possible on ex-pies
        return type == MI_TOMAHAWK && !strict;
#endif

    default:
        if (type == MI_NEEDLE)
            return false;
    }

    // Everything else doesn't matter.
    if (brand == SPMSL_NORMAL)
        return true;

    // In non-strict mode, everything other than needles is mostly ok.
    if (!strict)
        return true;

    // Not a missile?
    if (type == 0)
        return true;

    // Specifics
    switch (brand)
    {
    case SPMSL_FLAME:
        return type == MI_SLING_BULLET || type == MI_ARROW
               || type == MI_BOLT;
    case SPMSL_FROST:
        return type == MI_SLING_BULLET || type == MI_ARROW
               || type == MI_BOLT;
    case SPMSL_POISONED:
        return type == MI_SLING_BULLET || type == MI_ARROW
               || type == MI_BOLT || type == MI_JAVELIN
               || type == MI_TOMAHAWK;
    case SPMSL_RETURNING:
        return type == MI_JAVELIN || type == MI_TOMAHAWK;
    case SPMSL_CHAOS:
        return type == MI_SLING_BULLET || type == MI_ARROW
               || type == MI_BOLT || type == MI_TOMAHAWK
               || type == MI_JAVELIN || type == MI_THROWING_NET;
    case SPMSL_PENETRATION:
        return type == MI_JAVELIN || type == MI_BOLT;
    case SPMSL_DISPERSAL:
        return type == MI_ARROW || type == MI_TOMAHAWK;
    case SPMSL_EXPLODING:
        return type == MI_SLING_BULLET;
    case SPMSL_STEEL: // deliberate fall through
    case SPMSL_SILVER:
        return type == MI_BOLT || type == MI_SLING_BULLET
               || type == MI_JAVELIN || type == MI_TOMAHAWK
               || type == MI_THROWING_NET;
    default: break;
    }

    // Assume yes, if we've gotten this far.
    return false;
}

static void _generate_missile_item(item_def& item, int force_type,
                                   int item_level)
{
    const bool no_brand = (item.special == SPMSL_FORBID_BRAND);
    if (no_brand)
        item.special = SPMSL_NORMAL;

    item.plus = 0;

    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        item.sub_type =
            random_choose_weighted(50, MI_STONE,
                                   20, MI_ARROW,
                                   12, MI_BOLT,
                                   12, MI_SLING_BULLET,
                                   10, MI_NEEDLE,
                                   3,  MI_TOMAHAWK,
                                   2,  MI_JAVELIN,
                                   1,  MI_THROWING_NET,
                                   1,  MI_LARGE_ROCK,
                                   0);
    }

    // No fancy rocks -- break out before we get to special stuff.
    if (item.sub_type == MI_LARGE_ROCK)
    {
        item.quantity = 2 + random2avg(5,2);
        return;
    }
    else if (item.sub_type == MI_STONE)
    {
        item.quantity = 1 + random2(7) + random2(10) + random2(12) + random2(10);
        return;
    }
    else if (item.sub_type == MI_THROWING_NET) // no fancy nets, either
    {
        item.quantity = 1 + one_chance_in(4); // and only one, rarely two
        return;
    }

    if (!no_brand)
    {
        set_item_ego_type(item, OBJ_MISSILES,
                           _determine_missile_brand(item, item_level));
    }

    // Reduced quantity if special.
    if (item.sub_type == MI_JAVELIN || item.sub_type == MI_TOMAHAWK
        || (item.sub_type == MI_NEEDLE && get_ammo_brand(item) != SPMSL_POISONED)
        || get_ammo_brand(item) == SPMSL_RETURNING)
    {
        item.quantity = random_range(2, 8);
    }
    else if (get_ammo_brand(item) != SPMSL_NORMAL)
        item.quantity = 1 + random2(7) + random2(10) + random2(10);
    else
        item.quantity = 1 + random2(7) + random2(10) + random2(10) + random2(12);
}

static bool _try_make_armour_artefact(item_def& item, int force_type,
                                      int item_level, bool force_randart = false)
{
    if (item_level > 2 && x_chance_in_y(101 + item_level * 3, 4000)
        || force_randart)
    {
        // Make a randart or unrandart.

        // 1 in 20 randarts are unrandarts.
        if (one_chance_in(item_level == MAKE_GOOD_ITEM ? 7 : 20)
            && !force_randart)
        {
            if (_try_make_item_unrand(item, force_type))
                return true;
        }

        // The rest are normal randarts.

        // 10% of boots become barding.
        if (item.sub_type == ARM_BOOTS && one_chance_in(10))
        {
            item.sub_type = coinflip() ? ARM_NAGA_BARDING
                                       : ARM_CENTAUR_BARDING;
        }
        else
            hide2armour(item); // No randart hides.

        // Needs to be done after the barding chance else we get randart
        // bardings named Boots of xy.
        make_item_randart(item);

        // Determine enchantment and cursedness.
        if (one_chance_in(5))
        {
            do_curse_item(item);
            item.plus = -random2(6);
        }
        else
        {
            int max_plus = armour_max_enchant(item);
            item.plus = random2(max_plus + 1);

            if (one_chance_in(5))
                item.plus += random2(max_plus + 6) / 2;

            if (one_chance_in(6))
                item.plus -= random2(max_plus + 6);

            if (item.plus < 0 && !one_chance_in(3))
                do_curse_item(item);
        }

        // Don't let quicksilver dragon armour get minuses.
        if (item.sub_type == ARM_QUICKSILVER_DRAGON_ARMOUR)
            item.plus = 0;

        return true;
    }

    return false;
}

static special_armour_type _determine_armour_ego(const item_def& item,
                                                 int force_type, int item_level)
{
    if (item.special != 0)
        return static_cast<special_armour_type>(item.special);

    special_armour_type rc = SPARM_NORMAL;
    switch (item.sub_type)
    {
    case ARM_SHIELD:
    case ARM_LARGE_SHIELD:
    case ARM_BUCKLER:
        rc = random_choose_weighted(40, SPARM_RESISTANCE,
                                   120, SPARM_FIRE_RESISTANCE,
                                   120, SPARM_COLD_RESISTANCE,
                                   120, SPARM_POISON_RESISTANCE,
                                   120, SPARM_POSITIVE_ENERGY,
                                   240, SPARM_REFLECTION,
                                   480, SPARM_PROTECTION,
                                     0);
        break;

    case ARM_CLOAK:
        rc = random_choose(SPARM_POISON_RESISTANCE,
                           SPARM_INVISIBILITY,
                           SPARM_MAGIC_RESISTANCE,
                           -1);
        break;

    case ARM_HAT:
        if (coinflip())
        {
            rc = random_choose_weighted(3, SPARM_MAGIC_RESISTANCE,
                                        2, SPARM_INTELLIGENCE,
                                        2, SPARM_SEE_INVISIBLE,
                                        1, SPARM_SPIRIT_SHIELD,
                                        0);
        }
        break;

    case ARM_HELMET:
        rc = coinflip() ? SPARM_SEE_INVISIBLE : SPARM_INTELLIGENCE;
        break;

    case ARM_GLOVES:
        rc = random_choose(SPARM_DEXTERITY, SPARM_STRENGTH, SPARM_ARCHERY, -1);
        break;

    case ARM_BOOTS:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    {
        const int tmp = random2(600) + 400 * (item.sub_type != ARM_BOOTS);

        rc = (tmp < 200) ? SPARM_RUNNING :
             (tmp < 400) ? SPARM_FLYING :
             (tmp < 600) ? SPARM_STEALTH :
             (tmp < 800) ? SPARM_COLD_RESISTANCE
                          : SPARM_FIRE_RESISTANCE;
        break;
    }

    case ARM_ROBE:
        rc = random_choose_weighted(1, SPARM_RESISTANCE,
                                    2, SPARM_COLD_RESISTANCE,
                                    2, SPARM_FIRE_RESISTANCE,
                                    2, SPARM_POSITIVE_ENERGY,
                                    4, SPARM_MAGIC_RESISTANCE,
                                    4, SPARM_ARCHMAGI,
                                    0);

        // Only ever generate robes of archmagi for random pieces of armour,
        // for whatever reason.
        if (rc == SPARM_ARCHMAGI
            && (force_type != OBJ_RANDOM || !x_chance_in_y(11 + item_level, 50)))
        {
            rc = SPARM_NORMAL;
        }
        break;

    default:
        if (armour_is_hide(item, true)
            || item.sub_type == ARM_ANIMAL_SKIN
            || item.sub_type == ARM_CRYSTAL_PLATE_ARMOUR)
        {
            rc = SPARM_NORMAL;
            break;
        }

        rc = coinflip() ? SPARM_COLD_RESISTANCE : SPARM_FIRE_RESISTANCE;

        if (one_chance_in(9))
            rc = SPARM_POSITIVE_ENERGY;

        if (one_chance_in(5))
            rc = SPARM_MAGIC_RESISTANCE;

        if (one_chance_in(5))
            rc = SPARM_POISON_RESISTANCE;

        if (item.sub_type == ARM_PLATE_ARMOUR && one_chance_in(15))
            rc = SPARM_PONDEROUSNESS;
        break;
    }

    ASSERT(is_armour_brand_ok(item.sub_type, rc, true));
    return rc;
}

bool is_armour_brand_ok(int type, int brand, bool strict)
{
    equipment_type slot = get_armour_slot((armour_type)type);

    // Currently being too restrictive results in asserts, being too
    // permissive will generate such items on "any armour ego:XXX".
    // The latter is definitely so much safer -- 1KB
    switch ((special_armour_type)brand)
    {
    case SPARM_FORBID_EGO:
    case SPARM_NORMAL:
        return true;

    case SPARM_FLYING:
        if (slot == EQ_BODY_ARMOUR)
            return true;
        // deliberate fall-through
    case SPARM_RUNNING:
    case SPARM_STEALTH:
#if TAG_MAJOR_VERSION == 34
    case SPARM_JUMPING:
#endif
        return slot == EQ_BOOTS;

    case SPARM_ARCHMAGI:
        return !strict || type == ARM_ROBE;

    case SPARM_PONDEROUSNESS:
        return true;
#if TAG_MAJOR_VERSION == 34
    case SPARM_PRESERVATION:
        if (type == ARM_PLATE_ARMOUR && !strict)
            return true;
        // deliberate fall-through
#endif
    case SPARM_INVISIBILITY:
        return slot == EQ_CLOAK;

    case SPARM_REFLECTION:
    case SPARM_PROTECTION:
        return slot == EQ_SHIELD;

    case SPARM_STRENGTH:
    case SPARM_DEXTERITY:
        if (!strict)
            return true;
        // deliberate fall-through
    case SPARM_ARCHERY:
        return slot == EQ_GLOVES;

    case SPARM_SEE_INVISIBLE:
    case SPARM_INTELLIGENCE:
        return slot == EQ_HELMET;

    case SPARM_FIRE_RESISTANCE:
    case SPARM_COLD_RESISTANCE:
    case SPARM_RESISTANCE:
        if (type == ARM_FIRE_DRAGON_ARMOUR
            || type == ARM_ICE_DRAGON_ARMOUR
            || type == ARM_GOLD_DRAGON_ARMOUR)
        {
            return false; // contradictory or redundant
        }
        return true; // in portal vaults, these can happen on every slot

    case SPARM_MAGIC_RESISTANCE:
        if (type == ARM_HAT)
            return true;
        // deliberate fall-through
    case SPARM_POISON_RESISTANCE:
    case SPARM_POSITIVE_ENERGY:
        if (type == ARM_PEARL_DRAGON_ARMOUR && brand == SPARM_POSITIVE_ENERGY)
            return false; // contradictory or redundant

        return slot == EQ_BODY_ARMOUR || slot == EQ_SHIELD || slot == EQ_CLOAK
               || !strict;

    case SPARM_SPIRIT_SHIELD:
        return type == ARM_HAT ||
#if TAG_MAJOR_VERSION == 34
               type == ARM_CAP ||
#endif
               slot == EQ_SHIELD || !strict;
    case NUM_SPECIAL_ARMOURS:
    case NUM_REAL_SPECIAL_ARMOURS:
        die("invalid armour brand");
    }

    return true;
}

/**
 * Return the number of plusses required for a type of armour to be noteable.
 * (From plus alone.)
 *
 * @param armour_type   The type of armour being considered.
 * @return              The armour plus value required to be interesting.
 */
static int _armour_plus_threshold(equipment_type armour_type)
{
    switch (armour_type)
    {
            // body armour is very common; squelch most of it
        case EQ_BODY_ARMOUR:
            return 3;
            // shields are fairly common
        case EQ_SHIELD:
            return 2;
            // aux armour is relatively uncommon
        default:
            return 1;
    }
}

static void _generate_armour_item(item_def& item, bool allow_uniques,
                                  int force_type, int item_level)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        int i;
        for (i = 0; i < 1000; ++i)
        {
            item.sub_type = _get_random_armour_type(item_level);
            if (is_armour_brand_ok(item.sub_type, item.special, true))
                break;
        }
    }

    // Forced randart.
    if (item_level == ISPEC_RANDART)
    {
        int i;
        for (i = 0; i < 100; ++i)
        {
            if (_try_make_armour_artefact(item, force_type, 0, true)
                && is_artefact(item))
            {
                return;
            }
        }
        // fall back to an ordinary item
        item_level = MAKE_GOOD_ITEM;
    }

    if (allow_uniques
        && _try_make_armour_artefact(item, force_type, item_level))
    {
        return;
    }

    if (item.sub_type == ARM_BOOTS)
    {
        if (one_chance_in(8))
            item.sub_type = ARM_NAGA_BARDING;
        else if (one_chance_in(7))
            item.sub_type = ARM_CENTAUR_BARDING;
    }

    const bool force_good = item_level >= MAKE_GIFT_ITEM;
    const bool forced_ego = (item.special > 0);
    const bool no_ego     = (item.special == SPARM_FORBID_EGO);

    if (no_ego)
        item.special = SPARM_NORMAL;

    if (item_level < 0)
    {
        // Thoroughly damaged, could have been good once.
        if (!no_ego && (forced_ego || one_chance_in(4)))
        {
            // Brand is set as for "good" items.
            set_item_ego_type(item, OBJ_ARMOUR,
                _determine_armour_ego(item, item.sub_type,
                2 + 2 * env.absdepth0));
        }

        item.plus -= 1 + random2(3);

        if (item_level == ISPEC_BAD)
            do_curse_item(item);
    }
    else if ((forced_ego || item.sub_type == ARM_HAT
                    || x_chance_in_y(51 + item_level, 250))
                && !item.is_mundane() || force_good)
    {
        // Make a good item...
        item.plus += random2(3);

        if (item.sub_type <= ARM_PLATE_ARMOUR
            && x_chance_in_y(21 + item_level, 300))
        {
            item.plus += random2(3);
        }

        if (!no_ego && x_chance_in_y(31 + item_level, 350))
        {
            // ...an ego item, in fact.
            set_item_ego_type(item, OBJ_ARMOUR,
                              _determine_armour_ego(item, force_type,
                                                    item_level));

            if (get_armour_ego_type(item) == SPARM_PONDEROUSNESS)
                item.plus += 3 + random2(8);
        }
    }
    else if (one_chance_in(12))
    {
        // Make a bad (cursed) item.
        do_curse_item(item);

        if (one_chance_in(5))
            item.plus -= random2(3);

        set_item_ego_type(item, OBJ_ARMOUR, SPARM_NORMAL);
    }

    // Make sure you don't get a hide from acquirement (since that
    // would be an enchanted item which somehow didn't get converted
    // into armour).
    if (force_good)
        hide2armour(item);

    // Don't overenchant items.
    if (item.plus > armour_max_enchant(item))
        item.plus = armour_max_enchant(item);

    // Don't let quicksilver dragon armour get minuses.
    if (item.sub_type == ARM_QUICKSILVER_DRAGON_ARMOUR)
        item.plus = 0;

    // squash boring items.
    if (!force_good && item.special == SPARM_NORMAL && item.plus > 0
        && item.plus < _armour_plus_threshold(get_armour_slot(item)))
    {
        item.plus = 0;
    }

    if (armour_is_hide(item))
    {
        do_uncurse_item(item, false);
        item.plus = 0;
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }
}

static monster_type _choose_random_monster_corpse()
{
    for (int count = 0; count < 1000; ++count)
    {
        monster_type spc = mons_species(static_cast<monster_type>(
                                        random2(NUM_MONSTERS)));
        if (mons_class_flag(spc, M_NO_POLY_TO | M_CANT_SPAWN))
            continue;
        if (mons_weight(spc) > 0)        // drops a corpse
            return spc;
    }

    return MONS_RAT;          // if you can't find anything else...
}

static int _random_wand_subtype()
{
    int rc = random2(NUM_WANDS);

    // Adjusted distribution here -- bwr
    // Wands used to be uniform (5.26% each)
    //
    // Now:
    // hasting, heal wounds                                (1.11% each)
    // invisibility, enslavement, fireball, teleportation  (3.74% each)
    // others                                              (6.37% each)
    if (rc == WAND_INVISIBILITY || rc == WAND_HASTING || rc == WAND_HEAL_WOUNDS
        || (rc == WAND_INVISIBILITY || rc == WAND_ENSLAVEMENT
            || rc == WAND_FIREBALL || rc == WAND_TELEPORTATION) && coinflip())
    {
        rc = random2(NUM_WANDS);
    }
    return rc;
}

bool is_high_tier_wand(int type)
{
    switch (type)
    {
    case WAND_PARALYSIS:
    case WAND_FIRE:
    case WAND_COLD:
    case WAND_LIGHTNING:
    case WAND_DRAINING:
    case WAND_DISINTEGRATION:
        return true;
    default:
        return false;
    }
}

static void _generate_wand_item(item_def& item, int force_type, int item_level)
{
    // Determine sub_type.
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
            item.sub_type = _random_wand_subtype();
        while (item_level < 2 && is_high_tier_wand(item.sub_type));
    }

    // Generate charges randomly...
    item.plus = random2avg(wand_max_charges(item.sub_type), 3);

    // ...but 0 charges is silly
    if (item.charges == 0)
        item.charges++;

    item.used_count = 0;
}

static void _generate_food_item(item_def& item, int force_quant, int force_type)
{
    // Determine sub_type:
    if (force_type == OBJ_RANDOM)
    {
        item.sub_type = random_choose_weighted( 30, FOOD_BREAD_RATION,
                                                10, FOOD_FRUIT,
                                                30, FOOD_MEAT_RATION,
                                                15, FOOD_BEEF_JERKY,
                                                10, FOOD_PIZZA,
                                                 5, FOOD_ROYAL_JELLY,
                                                 0);
    }
    else
        item.sub_type = force_type;

    // Happens with ghoul food acquirement -- use place_chunks() outherwise
    if (item.sub_type == FOOD_CHUNK)
    {
        // Set chunk flavour:
        item.plus = _choose_random_monster_corpse();
        item.orig_monnum = item.plus;
        // Set duration.
        item.special = (10 + random2(11)) * 10;
    }

    // Determine quantity.
    if (force_quant > 1)
        item.quantity = force_quant;
    else
    {
        item.quantity = 1;

        if (item.sub_type != FOOD_MEAT_RATION
            && item.sub_type != FOOD_BREAD_RATION)
        {
            if (one_chance_in(80))
                item.quantity += random2(3);

            if (is_fruit(item))
                item.quantity += random2avg(5,2);
        }
    }
}

static void _generate_potion_item(item_def& item, int force_type,
                                  int item_level, int agent)
{
    item.quantity = 1;

    if (one_chance_in(18))
        item.quantity++;

    if (one_chance_in(25))
        item.quantity++;

    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        int stype;
        int tries = 500;
        do
        {
            // total weight is 1075
            stype = random_choose_weighted(191, POT_CURING,
                                            95, POT_HEAL_WOUNDS,
                                            75, POT_RESTORE_ABILITIES,
                                            66, POT_POISON,
                                            66, POT_MIGHT,
                                            66, POT_AGILITY,
                                            66, POT_BRILLIANCE,
                                            63, POT_HASTE,
                                            35, POT_INVISIBILITY,
                                            35, POT_FLIGHT,
                                            35, POT_RESISTANCE,
                                            35, POT_MAGIC,
                                            35, POT_BERSERK_RAGE,
                                            35, POT_MUTATION,
                                            35, POT_LIGNIFY,
                                            34, POT_CANCELLATION,
                                            34, POT_CONFUSION,
                                            25, POT_DEGENERATION,
                                            23, POT_CURE_MUTATION,
                                            12, POT_BENEFICIAL_MUTATION,
                                            10, POT_PORRIDGE,
                                             2, POT_EXPERIENCE,
                                             2, POT_DECAY,
                                             0);
        }
        while (stype == POT_POISON && item_level < 1
               // Too dangerous on monsters early on
               || stype == POT_BERSERK_RAGE && item_level < 2
               || (agent == GOD_XOM && _is_boring_item(OBJ_POTIONS, stype)
                   && --tries > 0));

        item.sub_type = stype;
    }

    if (item.sub_type == POT_BENEFICIAL_MUTATION
#if TAG_MAJOR_VERSION == 34
        || item.sub_type == POT_GAIN_STRENGTH
        || item.sub_type == POT_GAIN_DEXTERITY
        || item.sub_type == POT_GAIN_INTELLIGENCE
#endif
        || item.sub_type == POT_EXPERIENCE
        || item.sub_type == POT_RESTORE_ABILITIES)
    {
        item.quantity = 1;
    }
}

static void _generate_scroll_item(item_def& item, int force_type,
                                  int item_level, int agent)
{
    // determine sub_type:
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        const int depth_mod = random2(1 + item_level);
        int tries = 500;
        do
        {
            // total weight:    784  if depth_mod < 4
            //                  913  otherwise
            //                 -112  in sprint
            item.sub_type = random_choose_weighted(
                200, SCR_IDENTIFY,
                112, SCR_REMOVE_CURSE,
                 // [Cha] don't generate teleportation scrolls if in sprint
                 80, (crawl_state.game_is_sprint() ? NUM_SCROLLS : SCR_TELEPORTATION),
                 40, SCR_ENCHANT_ARMOUR,
                 40, SCR_ENCHANT_WEAPON,
                 40, SCR_RECHARGING,
                 40, SCR_MAGIC_MAPPING,
                 40, SCR_AMNESIA,
                 32, SCR_FEAR,
                 32, SCR_FOG,
                 32, SCR_RANDOM_USELESSNESS,
                 32, SCR_BLINKING,
                 // [Cha] don't generate noise scrolls if in sprint
                 32, (crawl_state.game_is_sprint() ? NUM_SCROLLS : SCR_NOISE),
                 32, SCR_IMMOLATION,
                 // Higher-level scrolls.
                 27, (depth_mod < 4 ? NUM_SCROLLS : SCR_VULNERABILITY),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_ACQUIREMENT),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_SUMMONING),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_SILENCE),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_BRAND_WEAPON),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_TORMENT),
                 17, (depth_mod < 4 ? NUM_SCROLLS : SCR_HOLY_WORD),
                 0);
        }
        while (item.sub_type == NUM_SCROLLS
               || agent == GOD_XOM
               && _is_boring_item(OBJ_SCROLLS, item.sub_type)
               && --tries > 0);
    }

    // determine quantity
    if (item.sub_type == SCR_BRAND_WEAPON
        || item.sub_type == SCR_ACQUIREMENT
        || item.sub_type == SCR_TORMENT
        || item.sub_type == SCR_HOLY_WORD
        || item.sub_type == SCR_SILENCE)
    {
        item.quantity = 1;
    }
    else
    {
        if (one_chance_in(24))
            item.quantity = (coinflip() ? 2 : 3);
        else
            item.quantity = 1;
    }

    item.plus = 0;
}

static void _generate_book_item(item_def& item, bool allow_uniques,
                                int force_type, int item_level)
{
    // determine special (description)
    item.special = random2(5);

    if (one_chance_in(10))
        item.special += random2(NDSC_BOOK_SEC) * NDSC_BOOK_PRI;

    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
        {
            item.sub_type = random2(NUM_FIXED_BOOKS);

            if (book_rarity(item.sub_type) != 100
                && one_chance_in(25))
            {
                item.sub_type = coinflip() ? BOOK_WIZARDRY : BOOK_POWER;
            }

            if (!one_chance_in(100)
                && x_chance_in_y(book_rarity(item.sub_type)-1, item_level+1))
            {
                // If this book is really rare for this depth, continue trying.
                continue;
            }
        }
        while (book_rarity(item.sub_type) == 100);

        // Tome of destruction: rare!
        if (item_level > 10 && x_chance_in_y(21 + item_level, 7000))
            item.sub_type = BOOK_DESTRUCTION;

        // Skill manuals - also rare.
        if (item_level > 6 && x_chance_in_y(21 + item_level, 4000))
            item.sub_type = BOOK_MANUAL;
    }

    // Determine which skill for a manual.
    if (item.sub_type == BOOK_MANUAL)
    {
        if (one_chance_in(4))
        {
            item.skill = static_cast<skill_type>(SK_SPELLCASTING +
                                                 random2(NUM_SKILLS -
                                                         SK_SPELLCASTING));
        }
        else
#if TAG_MAJOR_VERSION == 34
        {
            item.skill = static_cast<skill_type>(random2(SK_UNARMED_COMBAT));
            if (item.skill == SK_STABBING)
                item.skill = SK_UNARMED_COMBAT;
            if (item.skill == SK_TRAPS)
                item.skill = SK_STEALTH;
        }
#else
            item.plus = random2(SK_UNARMED_COMBAT + 1);
#endif
        // Set number of bonus skill points.
        item.skill_points = random_range(2000, 3000);
    }

    // Manuals and books of destruction are rare enough without replacing
    // them with randart books.
    if (item.sub_type == BOOK_MANUAL || item.sub_type == BOOK_DESTRUCTION)
        return;

    // Only randomly generate randart books for OBJ_RANDOM, since randart
    // spellbooks aren't merely of-the-same-type-but-better, but
    // have an entirely different set of spells.
    if (allow_uniques && item_level > 2 && force_type == OBJ_RANDOM
        && x_chance_in_y(101 + item_level * 3, 4000))
    {
        int choice = random_choose_weighted(
            58, BOOK_RANDART_THEME,
             2, BOOK_RANDART_LEVEL, // 1/30
             0);

        item.sub_type = choice;
    }

    if (item.sub_type == BOOK_RANDART_THEME)
        make_book_theme_randart(item, 0, 0, 5 + coinflip(), 20);
    else if (item.sub_type == BOOK_RANDART_LEVEL)
    {
        int max_level  = min(9, max(1, item_level / 3));
        int spl_level  = random_range(1, max_level);
        make_book_level_randart(item, spl_level);
    }
}

static void _generate_staff_item(item_def& item, bool allow_uniques, int force_type, int item_level)
{
    // If we make the unique roll, no further generation necessary.
    // Copied unrand code from _try_make_weapon_artefact since randart enhancer staves
    // can't happen.
    if (allow_uniques
        && one_chance_in(item_level == MAKE_GOOD_ITEM ? 27 : 100))
    {
        // Temporarily fix the base_type to get enhancer staves
        item.base_type = OBJ_WEAPONS;
        if (_try_make_item_unrand(item, WPN_STAFF))
            return;
        item.base_type = OBJ_STAVES;
    }

    if (force_type == OBJ_RANDOM)
    {
#if TAG_MAJOR_VERSION == 34
        do
            item.sub_type = random2(NUM_STAVES);
        while (item.sub_type == STAFF_ENCHANTMENT
               || item.sub_type == STAFF_CHANNELING);
#else
        item.sub_type = random2(NUM_STAVES);
#endif

        // staves of energy are 25% less common, wizardry/power
        // are more common
        if (item.sub_type == STAFF_ENERGY && one_chance_in(4))
            item.sub_type = coinflip() ? STAFF_WIZARDRY : STAFF_POWER;
    }
    else
        item.sub_type = force_type;

    if (one_chance_in(16))
        do_curse_item(item);
}

static void _generate_rod_item(item_def& item, int force_type, int item_level)
{
    if (force_type == OBJ_RANDOM)
#if TAG_MAJOR_VERSION == 34
    {
        do
            item.sub_type = random2(NUM_RODS);
        while (item.sub_type == ROD_WARDING || item.sub_type == ROD_VENOM);
    }
#else
        item.sub_type = random2(NUM_RODS);
#endif
    else
        item.sub_type = force_type;

    init_rod_mp(item, -1, item_level);

    if (one_chance_in(16))
        do_curse_item(item);
}

static bool _try_make_jewellery_unrandart(item_def& item, int force_type,
                                          int item_level)
{
    int type = (force_type == NUM_RINGS)     ? get_random_ring_type() :
               (force_type == NUM_JEWELLERY) ? get_random_amulet_type()
                                             : force_type;
    if (item_level > 2
        && one_chance_in(20)
        && x_chance_in_y(101 + item_level * 3, 2000))
    {
        if (_try_make_item_unrand(item, type))
            return true;
    }

    return false;
}

static int _determine_ring_plus(int subtype)
{
    int rc = 0;

    switch (subtype)
    {
    case RING_PROTECTION:
    case RING_STRENGTH:
    case RING_SLAYING:
    case RING_EVASION:
    case RING_DEXTERITY:
    case RING_INTELLIGENCE:
        if (one_chance_in(5)) // 20% of such rings are cursed {dlb}
        {
            rc = (coinflip() ? -2 : -3);

            if (one_chance_in(3))
                rc -= random2(4);
        }
        else
            rc = 1 + (one_chance_in(3) ? random2(3) : random2avg(6, 2));
        break;

    default:
        break;
    }
    return rc;
}

static void _generate_jewellery_item(item_def& item, bool allow_uniques,
                                     int force_type, int item_level,
                                     int agent)
{
    if (allow_uniques && item_level != ISPEC_RANDART
        && _try_make_jewellery_unrandart(item, force_type, item_level))
    {
        return;
    }

    // Determine subtype.
    // Note: removed double probability reduction for some subtypes
    if (force_type != OBJ_RANDOM
        && force_type != NUM_RINGS
        && force_type != NUM_JEWELLERY)
    {
        item.sub_type = force_type;
    }
    else
    {
        int tries = 500;
        do
        {
            if (force_type == NUM_RINGS)
                item.sub_type = get_random_ring_type();
            else if (force_type == NUM_JEWELLERY)
                item.sub_type = get_random_amulet_type();
            else
                item.sub_type = (one_chance_in(4) ? get_random_amulet_type()
                                                  : get_random_ring_type());
        }
        while (agent == GOD_XOM
               && _is_boring_item(OBJ_JEWELLERY, item.sub_type)
               && --tries > 0);
    }

    item.plus = _determine_ring_plus(item.sub_type);

    if (item.plus < 0)
        do_curse_item(item);

    // All jewellery base types should now work. - bwr
    if (item_level == ISPEC_RANDART
        || allow_uniques && item_level > 2
           && x_chance_in_y(101 + item_level * 3, 4000))
    {
        make_item_randart(item);
    }
    else if (item.sub_type == RING_LOUDNESS
             || item.sub_type == RING_TELEPORTATION
             || item.sub_type == AMU_INACCURACY
             || one_chance_in(50))
    {
        // Bad jewellery is always cursed {dlb}:
        do_curse_item(item);
    }
}

static void _generate_misc_item(item_def& item, int force_type, int force_ego)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
            item.sub_type = random2(NUM_MISCELLANY);
        while
            // never randomly generated
            (item.sub_type == MISC_RUNE_OF_ZOT
             || item.sub_type == MISC_HORN_OF_GERYON
             || item.sub_type == MISC_DECK_OF_PUNISHMENT
             || item.sub_type == MISC_QUAD_DAMAGE
#if TAG_MAJOR_VERSION == 34
             || item.sub_type == MISC_BUGGY_EBONY_CASKET
             || item.sub_type == MISC_BOTTLED_EFREET
             || item.sub_type == MISC_DECK_OF_DUNGEONS
#endif
             // Nemelex' decks are rare in the dungeon.
             || ((item.sub_type == MISC_DECK_OF_WAR
                    || item.sub_type == MISC_DECK_OF_ESCAPE)
                && !one_chance_in(5)));
    }

    // set initial charges
    if (item.sub_type == MISC_BOX_OF_BEASTS
        || item.sub_type == MISC_SACK_OF_SPIDERS)
    {
        item.charges = random_range(5, 15, 2);
        item.used_count = 0;
    }

    if (is_deck(item))
    {
        item.initial_cards = 4 + random2(10);

        if (force_ego >= DECK_RARITY_COMMON
            && force_ego <= DECK_RARITY_LEGENDARY)
        {
            item.deck_rarity = static_cast<deck_rarity_type>(force_ego);
        }
        else
        {
            item.deck_rarity = random_choose_weighted(8, DECK_RARITY_LEGENDARY,
                                                     20, DECK_RARITY_RARE,
                                                     72, DECK_RARITY_COMMON,
                                                      0);
        }
        init_deck(item);
    }
}

/**
 * Alter the inputed item to have no "plusses" (mostly weapon/armour enchantment)
 *
 * @param[in,out] item_slot The item slot of the item to remove "plusses" from.
 */
void squash_plusses(int item_slot)
{
    item_def& item(mitm[item_slot]);

    ASSERT(!is_deck(item));
    item.plus         = 0;
    item.used_count   = 0;
    item.special      = 0;
    set_equip_desc(item, ISFLAG_NO_DESC);
}

/**
 * Create an item. This function does too much.
 *
 * @param allow_uniques Can the item generated be an artefact?
 * @param force_class The desired OBJECTS class (Example: OBJ_ARMOUR)
 * @param force_type The desired SUBTYPE - enum varies by OBJ
 * @param item_level How powerful the item is allowed to be
 * @param force_ego The desired ego/brand
 * @param agent The agent creating the item (Example: Xom) or -1 if NA
 *
 * @return The generated item's item slot or NON_ITEM if it fails.
 */
int items(bool allow_uniques,
          object_class_type force_class,
          int force_type,
          int item_level,
          int force_ego,
          int agent)
{
    ASSERT(force_ego <= 0
           || force_class == OBJ_WEAPONS
           || force_class == OBJ_ARMOUR
           || force_class == OBJ_MISSILES
           || force_class == OBJ_MISCELLANY
              && force_type >= MISC_FIRST_DECK
              && force_type <= MISC_LAST_DECK);

    // Find an empty slot for the item (with culling if required).
    int p = get_mitm_slot(10);
    if (p == NON_ITEM)
        return NON_ITEM;

    item_def& item(mitm[p]);

    const bool force_good = item_level >= MAKE_GIFT_ITEM;

    if (force_ego != 0)
        allow_uniques = false;

    item.special = force_ego;

    // cap item_level unless an acquirement-level item {dlb}:
    if (item_level > 50 && !force_good)
        item_level = 50;

    // determine base_type for item generated {dlb}:
    if (force_class != OBJ_RANDOM)
    {
        ASSERT(force_class < NUM_OBJECT_CLASSES);
        item.base_type = force_class;
    }
    else
    {
        ASSERT(force_type == OBJ_RANDOM);
        // Total weight: 1960
        item.base_type = random_choose_weighted(
                                     1, OBJ_RODS,
                                     9, OBJ_STAVES,
                                    30, OBJ_BOOKS,
                                    50, OBJ_JEWELLERY,
                                    70, OBJ_WANDS,
                                   140, OBJ_FOOD,
                                   212, OBJ_ARMOUR,
                                   212, OBJ_WEAPONS,
                                   176, OBJ_POTIONS,
                                   300, OBJ_MISSILES,
                                   320, OBJ_SCROLLS,
                                   440, OBJ_GOLD,
                                     0);

        // misc items placement wholly dependent upon current depth {dlb}:
        if (item_level > 7 && x_chance_in_y(21 + item_level, 3500))
            item.base_type = OBJ_MISCELLANY;

        if (item_level < 7
            && (item.base_type == OBJ_BOOKS
                || item.base_type == OBJ_STAVES
                || item.base_type == OBJ_RODS
                || item.base_type == OBJ_WANDS)
            && random2(7) >= item_level)
        {
            item.base_type = coinflip() ? OBJ_POTIONS : OBJ_SCROLLS;
        }
    }

    ASSERT(force_type == OBJ_RANDOM
           || item.base_type == OBJ_JEWELLERY && force_type == NUM_JEWELLERY
           || force_type < get_max_subtype(item.base_type));

    item.quantity = 1;          // generally the case

    // make_item_randart() might do things differently based upon the
    // acquirement agent, especially for god gifts.
    if (agent != -1 && !is_stackable_item(item))
        origin_acquired(item, agent);

    if (force_ego < SP_FORBID_EGO)
    {
        force_ego = -force_ego;
        if (get_unique_item_status(force_ego) == UNIQ_NOT_EXISTS)
        {
            make_item_unrandart(mitm[p], force_ego);
            ASSERT(mitm[p].is_valid());
            return p;
        }
        // the base item otherwise
        item.special = SPWPN_NORMAL;
        force_ego = 0;
    }

    // Determine sub_type accordingly. {dlb}
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        _generate_weapon_item(item, allow_uniques, force_type,
                              item_level);
        break;

    case OBJ_MISSILES:
        _generate_missile_item(item, force_type, item_level);
        break;

    case OBJ_ARMOUR:
        _generate_armour_item(item, allow_uniques, force_type, item_level);
        break;

    case OBJ_WANDS:
        _generate_wand_item(item, force_type, item_level);
        break;

    case OBJ_FOOD:
        _generate_food_item(item, allow_uniques, force_type);
        break;

    case OBJ_POTIONS:
        _generate_potion_item(item, force_type, item_level, agent);
        break;

    case OBJ_SCROLLS:
        _generate_scroll_item(item, force_type, item_level, agent);
        break;

    case OBJ_JEWELLERY:
        _generate_jewellery_item(item, allow_uniques, force_type, item_level,
                                 agent);
        break;

    case OBJ_BOOKS:
        _generate_book_item(item, allow_uniques, force_type, item_level);
        break;

    case OBJ_STAVES:
        // Don't generate unrand staves this way except through acquirement,
        // since they also generate as OBJ_WEAPONS.
        _generate_staff_item(item, (agent != -1), force_type, item_level);
        break;

    case OBJ_RODS:
        _generate_rod_item(item, force_type, item_level);
        break;

    case OBJ_ORBS:              // always forced in current setup {dlb}
        item.sub_type = force_type;
        break;

    case OBJ_MISCELLANY:
        _generate_misc_item(item, force_type, force_ego);
        break;

    // that is, everything turns to gold if not enumerated above, so ... {dlb}
    default:
        item.base_type = OBJ_GOLD;
        if (force_good)
            item.quantity = 100 + random2(400);
        else
            item.quantity = 1 + random2avg(19, 2) + random2(item_level);
        break;
    }

    if (item.base_type == OBJ_WEAPONS
          && !is_weapon_brand_ok(item.sub_type, get_weapon_brand(item), false)
        || item.base_type == OBJ_ARMOUR
          && !is_armour_brand_ok(item.sub_type, get_armour_ego_type(item), false)
        || item.base_type == OBJ_MISSILES
          && !is_missile_brand_ok(item.sub_type, item.special, false))
    {
        mprf(MSGCH_ERROR, "Invalid brand on item %s, annulling.",
            item.name(DESC_PLAIN, false, true, false, false, ISFLAG_KNOW_PLUSES
                      | ISFLAG_KNOW_CURSE).c_str());
        item.special = 0;
    }

    // Colour the item.
    item_colour(item);

    // Set brand appearance.
    item_set_appearance(item);

    // Don't place the item just yet.
    item.pos.reset();
    item.link = NON_ITEM;

    // Note that item might be invalidated now, since p could have changed.
    ASSERT(mitm[p].is_valid());
    return p;
}

void reroll_brand(item_def &item, int item_level)
{
    ASSERT(!is_artefact(item));
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        item.special = _determine_weapon_brand(item, item_level);
        break;
    case OBJ_MISSILES:
        item.special = _determine_missile_brand(item, item_level);
        break;
    case OBJ_ARMOUR:
        // Robe of the Archmagi has an ugly hack of unknown purpose,
        // as one of side effects it won't ever generate here.
        item.special = _determine_armour_ego(item, OBJ_ARMOUR, item_level);
        break;
    default:
        die("can't reroll brands of this type");
    }
    item_set_appearance(item);
}

static int _roll_rod_enchant(int item_level)
{
    int value = 0;

    if (one_chance_in(4))
        value -= random_range(1, 3);

    if (item_level >= MAKE_GIFT_ITEM)
        value += 2;

    int pr = 20 + item_level * 2;

    if (pr > 80)
        pr = 80;

    while (random2(100) < pr) value++;

    return stepdown_value(value, 4, 4, 4, 9);
}

void init_rod_mp(item_def &item, int ncharges, int item_level)
{
    ASSERT(item.base_type == OBJ_RODS);

    if (ncharges != -1)
    {
        item.charge_cap = ncharges * ROD_CHARGE_MULT;
        item.rod_plus = 0;
    }
    else
    {
        item.charge_cap = random_range(9, 14) * ROD_CHARGE_MULT;
        item.rod_plus = _roll_rod_enchant(item_level);
    }

    item.charges = item.charge_cap;
}

static bool _weapon_is_visibly_special(const item_def &item)
{
    const int brand = get_weapon_brand(item);
    const bool visibly_branded = (brand != SPWPN_NORMAL);

    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return false;

    if (visibly_branded || is_artefact(item) || item.plus > 0)
        return true;

    if (item.is_mundane())
        return false;

    if (item.flags & ISFLAG_CURSED && one_chance_in(3))
        return true;

    return false;
}

static bool _armour_is_visibly_special(const item_def &item)
{
    const int brand = get_armour_ego_type(item);
    const bool visibly_branded = (brand != SPARM_NORMAL);

    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return false;

    if (visibly_branded || is_artefact(item) || item.plus > 0)
        return true;

    if (item.is_mundane())
        return false;

    if (item.flags & ISFLAG_CURSED && one_chance_in(3))
        return true;

    return false;
}


jewellery_type get_random_amulet_type()
{
#if TAG_MAJOR_VERSION == 34
    int res;
    do
        res = (AMU_FIRST_AMULET + random2(NUM_JEWELLERY - AMU_FIRST_AMULET));
    // Do not generate cFly or Cons
    while (res == AMU_CONTROLLED_FLIGHT || res == AMU_CONSERVATION);

    return jewellery_type(res);
#else
    return static_cast<jewellery_type>(AMU_FIRST_AMULET
           + random2(NUM_JEWELLERY - AMU_FIRST_AMULET));
#endif
}

static jewellery_type _get_raw_random_ring_type()
{
    jewellery_type ring;
    do ring = (jewellery_type)(random_range(RING_FIRST_RING, NUM_RINGS - 1));
        while (ring == RING_TELEPORTATION && crawl_state.game_is_sprint());
    return ring;
}

jewellery_type get_random_ring_type()
{
    const jewellery_type j = _get_raw_random_ring_type();
    // Adjusted distribution here. - bwr
    if ((j == RING_INVISIBILITY
           || j == AMU_REGENERATION
           || j == RING_TELEPORT_CONTROL
           || j == RING_SLAYING)
        && !one_chance_in(3))
    {
        return _get_raw_random_ring_type();
    }

    return j;
}

// FIXME: Need to clean up this mess.
static armour_type _get_random_armour_type(int item_level)
{
    // Default (lowest-level) armours.
    const armour_type defarmours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                       ARM_RING_MAIL };

    int armtype = RANDOM_ELEMENT(defarmours);

    if (x_chance_in_y(11 + item_level, 35))
    {
        // Low-level armours.
        const armour_type lowarmours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                           ARM_RING_MAIL, ARM_SCALE_MAIL,
                                           ARM_CHAIN_MAIL };

        armtype = RANDOM_ELEMENT(lowarmours);
    }

    if (x_chance_in_y(11 + item_level, 60))
    {
        // Medium-level armours.
        const armour_type medarmours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                           ARM_RING_MAIL, ARM_SCALE_MAIL,
                                           ARM_CHAIN_MAIL, ARM_PLATE_ARMOUR };

        armtype = RANDOM_ELEMENT(medarmours);
    }

    if (one_chance_in(20) && x_chance_in_y(11 + item_level, 400))
    {
        // High-level armours, including troll and some dragon armours.
        const armour_type hiarmours[] = { ARM_CRYSTAL_PLATE_ARMOUR,
                                          ARM_TROLL_HIDE,
                                          ARM_TROLL_LEATHER_ARMOUR,
                                          ARM_FIRE_DRAGON_HIDE, ARM_FIRE_DRAGON_ARMOUR,
                                          ARM_ICE_DRAGON_HIDE,
                                          ARM_ICE_DRAGON_ARMOUR };

        armtype = RANDOM_ELEMENT(hiarmours);
    }

    if (one_chance_in(20) && x_chance_in_y(11 + item_level, 500))
    {
        // Animal skins and high-level armours, including the rest of
        // the dragon armours.
        const armour_type morehiarmours[] = { ARM_STEAM_DRAGON_HIDE,
                                              ARM_STEAM_DRAGON_ARMOUR,
                                              ARM_MOTTLED_DRAGON_HIDE,
                                              ARM_MOTTLED_DRAGON_ARMOUR,
                                              ARM_STORM_DRAGON_HIDE,
                                              ARM_STORM_DRAGON_ARMOUR,
                                              ARM_GOLD_DRAGON_HIDE,
                                              ARM_GOLD_DRAGON_ARMOUR,
                                              ARM_SWAMP_DRAGON_HIDE,
                                              ARM_SWAMP_DRAGON_ARMOUR,
                                              ARM_PEARL_DRAGON_HIDE,
                                              ARM_PEARL_DRAGON_ARMOUR,
                                              ARM_SHADOW_DRAGON_HIDE,
                                              ARM_SHADOW_DRAGON_ARMOUR,
                                              ARM_QUICKSILVER_DRAGON_HIDE,
                                              ARM_QUICKSILVER_DRAGON_ARMOUR, };

        armtype = RANDOM_ELEMENT(morehiarmours);

        if (one_chance_in(200))
            armtype = ARM_CRYSTAL_PLATE_ARMOUR;
    }

    // Secondary armours.
    if (one_chance_in(5))
    {
        const armour_type secarmours[] = { ARM_SHIELD, ARM_CLOAK, ARM_HELMET,
                                           ARM_GLOVES, ARM_BOOTS };

        armtype = RANDOM_ELEMENT(secarmours);

        if (armtype == ARM_HELMET && one_chance_in(3))
        {
            const armour_type hats[] = { ARM_HAT, ARM_HELMET };

            armtype = RANDOM_ELEMENT(hats);
        }
        else if (armtype == ARM_SHIELD)
        {
            armtype = random_choose_weighted(333, ARM_SHIELD,
                                             500, ARM_BUCKLER,
                                             167, ARM_LARGE_SHIELD,
                                               0);
        }
    }

    return static_cast<armour_type>(armtype);
}

// Sets item appearance to match brands, if any.
void item_set_appearance(item_def &item)
{
    // Artefact appearance overrides cosmetic flags anyway.
    if (is_artefact(item))
        return;

    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (_weapon_is_visibly_special(item))
        {
            set_equip_desc(item,
                           (coinflip() ? ISFLAG_GLOWING : ISFLAG_RUNED));
        }
        break;

    case OBJ_ARMOUR:
        if (_armour_is_visibly_special(item))
        {
            const item_status_flag_type descs[] = { ISFLAG_GLOWING,
                                                    ISFLAG_RUNED,
                                                    ISFLAG_EMBROIDERED_SHINY };

            set_equip_desc(item, RANDOM_ELEMENT(descs));
        }
        break;

    default:
        break;
    }
}

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
static int _test_item_level()
{
    switch (random2(10))
    {
    case 0:
        return MAKE_GOOD_ITEM;
    case 1:
        return ISPEC_DAMAGED;
    case 2:
        return ISPEC_BAD;
    case 3:
        return ISPEC_RANDART;
    default:
        return random2(50);
    }
}

void makeitem_tests()
{
    int i, level;
    item_def item;

    mpr("Running generate_weapon_item tests.");
    for (i = 0; i < 10000; ++i)
    {
        item.clear();
        level = _test_item_level();
        item.base_type = OBJ_WEAPONS;
        if (coinflip())
            item.special = SPWPN_NORMAL;
        else
            item.special = random2(NUM_REAL_SPECIAL_WEAPONS);
#if TAG_MAJOR_VERSION == 34
        if (item.special == SPWPN_ORC_SLAYING
            || item.special == SPWPN_REACHING
            || item.special == SPWPN_RETURNING
            || item.special == SPWPN_CONFUSE
            || item.special == SPWPN_DRAGON_SLAYING)
        {
            item.special = SPWPN_FORBID_BRAND;
        }
#endif
        _generate_weapon_item(item,
                              coinflip(),
                              coinflip() ? OBJ_RANDOM : random2(NUM_WEAPONS),
                              level);
    }

    mpr("Running generate_armour_item tests.");
    for (i = 0; i < 10000; ++i)
    {
        item.clear();
        level = _test_item_level();
        item.base_type = OBJ_ARMOUR;
        if (coinflip())
            item.special = SPARM_NORMAL;
        else
            item.special = random2(NUM_REAL_SPECIAL_ARMOURS);
        int type = coinflip() ? OBJ_RANDOM : random2(NUM_ARMOURS);
#if TAG_MAJOR_VERSION == 34
        if (type == ARM_CAP)
            type = ARM_HAT;
#endif
        _generate_armour_item(item,
                              coinflip(),
                              type,
                              level);
    }
}
#endif

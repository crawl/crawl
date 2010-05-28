/*
 * File:       makeitem.cc
 * Summary:    Item creation routines.
 * Created by: haranp on Sat Apr 21 11:31:42 2007 UTC
 */

#include "AppHdr.h"

#include <algorithm>

#include "enum.h"
#include "externs.h"
#include "options.h"
#include "makeitem.h"
#include "message.h"

#include "artefact.h"
#include "colour.h"
#include "coord.h"
#include "decks.h"
#include "describe.h"
#include "dungeon.h"
#include "env.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "misc.h"
#include "mon-util.h"
#include "player.h"
#include "random.h"
#include "spl-book.h"
#include "state.h"
#include "travel.h"

bool got_curare_roll(const int item_level)
{
    return one_chance_in(item_level > 27 ? 6   :
                         item_level < 2  ? 15  :
                         (364 - 7 * item_level) / 25);
}

static bool _got_distortion_roll(const int item_level)
{
    return (one_chance_in(25));
}

static int _exciting_colour()
{
    switch (random2(4))
    {
        case 0:  return YELLOW;
        case 1:  return LIGHTGREEN;
        case 2:  return LIGHTRED;
        case 3:  return LIGHTMAGENTA;
        default: return MAGENTA;
    }
}


static int _newwave_weapon_colour(const item_def &item)
{
    int item_colour = BLACK;
    // fixed artefacts get predefined colours

    std::string itname = item.name(DESC_PLAIN);
    lowercase(itname);

    const bool item_runed = itname.find(" runed ") != std::string::npos;
    const bool heav_runed = itname.find(" heavily ") != std::string::npos;

    if ( is_random_artefact(item) && (!item_runed || heav_runed) )
        return _exciting_colour();

    if (is_range_weapon(item))
    {
        switch (range_skill(item))
        {
        case SK_BOWS:
            item_colour = BLUE;
            break;
        case SK_CROSSBOWS:
            item_colour = LIGHTBLUE;
            break;
        case SK_THROWING:
            item_colour = WHITE;
            break;
        case SK_SLINGS:
            item_colour = BROWN;
            break;
        default:
            // huh?
            item_colour = MAGENTA;
            break;
        }
    }
    else
    {
        switch (weapon_skill(item))
        {
        case SK_SHORT_BLADES:
            item_colour = CYAN;
            break;
        case SK_LONG_BLADES:
            item_colour = LIGHTCYAN;
            break;
        case SK_AXES:
            item_colour = DARKGREY;
            break;
        case SK_MACES_FLAILS:
            item_colour = LIGHTGREY;
            break;
        case SK_POLEARMS:
            item_colour = RED;
            break;
        case SK_STAVES:
            item_colour = GREEN;
            break;
        default:
            // huh?
            item_colour = random_colour();
            break;
        }
    }

    return (item_colour);
}

static int _classic_weapon_colour(const item_def &item)
{
    int item_colour = BLACK;

    if (is_range_weapon(item))
        item_colour = BROWN;
    else
    {
        switch (item.sub_type)
        {
        case WPN_CLUB:
        case WPN_GIANT_CLUB:
        case WPN_GIANT_SPIKED_CLUB:
        case WPN_ANKUS:
        case WPN_WHIP:
        case WPN_QUARTERSTAFF:
            item_colour = BROWN;
            break;
        case WPN_QUICK_BLADE:
            item_colour = LIGHTBLUE;
            break;
        case WPN_EXECUTIONERS_AXE:
            item_colour = RED;
            break;
        default:
            item_colour = LIGHTCYAN;
            if (get_equip_race(item) == ISFLAG_DWARVEN)
                item_colour = CYAN;
            break;
        }
    }

    return (item_colour);
}

static int _weapon_colour(const item_def &item)
{
    return (Options.classic_item_colours ? _classic_weapon_colour(item)
                                         : _newwave_weapon_colour(item));
}

static int _newwave_missile_colour(const item_def &item)
{
    int item_colour = BLACK;
    switch (item.sub_type)
    {
    case MI_STONE:
    case MI_SLING_BULLET:
    case MI_LARGE_ROCK:
        item_colour = BROWN;
        break;
    case MI_ARROW:
        item_colour = BLUE;
        break;
    case MI_NEEDLE:
        item_colour = WHITE;
        break;
    case MI_BOLT:
        item_colour = LIGHTBLUE;
        break;
    case MI_DART:
        item_colour = CYAN;
        break;
    case MI_JAVELIN:
        item_colour = RED;
        break;
    case MI_THROWING_NET:
        item_colour = DARKGREY;
        break;
    default:
        // huh?
        item_colour = LIGHTCYAN;
        if (get_equip_race(item) == ISFLAG_DWARVEN)
            item_colour = CYAN;
        break;
    }
    return (item_colour);
}

static int _classic_missile_colour(const item_def &item)
{
    int item_colour = BLACK;
    switch (item.sub_type)
    {
    case MI_STONE:
    case MI_LARGE_ROCK:
    case MI_ARROW:
        item_colour = BROWN;
        break;
    case MI_SLING_BULLET:
        item_colour = BLUE;
        break;
    case MI_NEEDLE:
        item_colour = WHITE;
        break;
    case MI_THROWING_NET:
        item_colour = DARKGREY;
        break;
    default:
        item_colour = LIGHTCYAN;
        if (get_equip_race(item) == ISFLAG_DWARVEN)
            item_colour = CYAN;
        break;
    }
    return (item_colour);
}

static int _missile_colour(const item_def &item)
{
    return (Options.classic_item_colours ? _classic_missile_colour(item)
                                         : _newwave_missile_colour(item));
}

static int _newwave_armour_colour(const item_def &item)
{
    int item_colour = BLACK;
    std::string itname = item.name(DESC_PLAIN);
    lowercase(itname);

    const bool item_runed = itname.find(" runed ") != std::string::npos;
    const bool heav_runed = itname.find(" heavily ") != std::string::npos;

    if (is_random_artefact(item) && (!item_runed || heav_runed))
        return (_exciting_colour());

    switch (item.sub_type)
    {
      case ARM_CLOAK:
        item_colour = WHITE;
        break;
      case ARM_NAGA_BARDING:
      case ARM_CENTAUR_BARDING:
        item_colour = GREEN;
        break;
      case ARM_ROBE:
        item_colour = RED;
        break;
      case ARM_CAP:
      case ARM_WIZARD_HAT:
        item_colour = MAGENTA;
        break;
      case ARM_HELMET:
        item_colour = DARKGREY;
        break;
      case ARM_BOOTS:
        item_colour = BLUE;
        break;
      case ARM_GLOVES:
        item_colour = LIGHTBLUE;
        break;
      case ARM_LEATHER_ARMOUR:
        item_colour = BROWN;
        break;
      case ARM_ANIMAL_SKIN:
        item_colour = LIGHTGREY;
        break;
      case ARM_CRYSTAL_PLATE_MAIL:
        item_colour = WHITE;
        break;
      case ARM_SHIELD:
      case ARM_LARGE_SHIELD:
      case ARM_BUCKLER:
        item_colour = CYAN;
        break;
      default:
        item_colour = LIGHTCYAN;
        break;
    }

    return (item_colour);
}

static int _classic_armour_colour(const item_def &item)
{
    int item_colour = BLACK;
    switch (item.sub_type)
    {
    case ARM_CLOAK:
    case ARM_ROBE:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    case ARM_CAP:
    case ARM_WIZARD_HAT:
        item_colour = random_colour();
        break;
    case ARM_HELMET:
        item_colour = LIGHTCYAN;
        break;
    case ARM_BOOTS: // maybe more interesting boot colours?
    case ARM_GLOVES:
    case ARM_LEATHER_ARMOUR:
        item_colour = BROWN;
        break;
    case ARM_CRYSTAL_PLATE_MAIL:
        item_colour = LIGHTGREY;
        break;
    case ARM_ANIMAL_SKIN:
        item_colour = BROWN;
        break;
    default:
        item_colour = LIGHTCYAN;
        if (get_equip_race(item) == ISFLAG_DWARVEN)
            item_colour = CYAN;
        break;
    }
    return (item_colour);
}

static int _armour_colour(const item_def &item)
{
    return (Options.classic_item_colours ? _classic_armour_colour(item)
                                         : _newwave_armour_colour(item));
}

void item_colour(item_def &item)
{
    int switchnum = 0;
    int temp_value;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact(item))
            break;              // unrandarts already coloured

        if (is_demonic(item))
            item.colour = random_uncommon_colour();
        else
            item.colour = _weapon_colour(item);

        if (is_random_artefact(item) && one_chance_in(5)
            && Options.classic_item_colours)
        {
            item.colour = random_colour();
        }
        break;

    case OBJ_MISSILES:
        item.colour = _missile_colour(item);
        break;

    case OBJ_ARMOUR:
        if (is_unrandom_artefact(item))
            break;              // unrandarts have already been coloured

        switch (item.sub_type)
        {
        case ARM_DRAGON_HIDE:
        case ARM_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_DRAGON);
            break;
        case ARM_TROLL_HIDE:
        case ARM_TROLL_LEATHER_ARMOUR:
            item.colour = mons_class_colour(MONS_TROLL);
            break;
        case ARM_ICE_DRAGON_HIDE:
        case ARM_ICE_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_ICE_DRAGON);
            break;
        case ARM_STEAM_DRAGON_HIDE:
        case ARM_STEAM_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_STEAM_DRAGON);
            break;
        case ARM_MOTTLED_DRAGON_HIDE:
        case ARM_MOTTLED_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_MOTTLED_DRAGON);
            break;
        case ARM_STORM_DRAGON_HIDE:
        case ARM_STORM_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_STORM_DRAGON);
            break;
        case ARM_GOLD_DRAGON_HIDE:
        case ARM_GOLD_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_GOLDEN_DRAGON);
            break;
        case ARM_SWAMP_DRAGON_HIDE:
        case ARM_SWAMP_DRAGON_ARMOUR:
            item.colour = mons_class_colour(MONS_SWAMP_DRAGON);
            break;
        default:
            item.colour = _armour_colour(item);
            break;
        }

        // I don't think this is ever done -- see start of case {dlb}:
        if (is_random_artefact(item) && one_chance_in(5))
            item.colour = random_colour();
        break;

    case OBJ_WANDS:
        item.special = you.item_description[IDESC_WANDS][item.sub_type];

        switch (item.special % 12)
        {
        case 0:         //"iron wand"
            item.colour = CYAN;
            break;
        case 1:         //"brass wand"
        case 5:         //"gold wand"
            item.colour = YELLOW;
            break;
        case 2:         //"bone wand"
        case 8:         //"ivory wand"
        case 9:         //"glass wand"
        case 10:        //"lead wand"
        default:
            item.colour = LIGHTGREY;
            break;
        case 3:         //"wooden wand"
        case 4:         //"copper wand"
        case 7:         //"bronze wand"
            item.colour = BROWN;
            break;
        case 6:         //"silver wand"
            item.colour = WHITE;
            break;
        case 11:        //"fluorescent wand"
            item.colour = random_colour();
            break;
        }

        if (item.special / 12 == 9) // "blackened foo wand"
            item.colour = DARKGREY;

        break;

    case OBJ_POTIONS:
        item.plus = you.item_description[IDESC_POTIONS][item.sub_type];

        switch (item.plus % 14)
        {
        case 0:         //"clear potion"
        default:
            item.colour = LIGHTGREY;
            break;
        case 1:         //"blue potion"
        case 7:         //"inky potion"
            item.colour = BLUE;
            break;
        case 2:         //"black potion"
            item.colour = DARKGREY;
            break;
        case 3:         //"silvery potion"
        case 13:        //"white potion"
            item.colour = WHITE;
            break;
        case 4:         //"cyan potion"
            item.colour = CYAN;
            break;
        case 5:         //"purple potion"
            item.colour = MAGENTA;
            break;
        case 6:         //"orange potion"
            item.colour = LIGHTRED;
            break;
        case 8:         //"red potion"
            item.colour = RED;
            break;
        case 9:         //"yellow potion"
            item.colour = YELLOW;
            break;
        case 10:        //"green potion"
            item.colour = GREEN;
            break;
        case 11:        //"brown potion"
            item.colour = BROWN;
            break;
        case 12:        //"pink potion"
            item.colour = LIGHTMAGENTA;
            break;
        }
        break;

    case OBJ_FOOD:
        switch (item.sub_type)
        {
        case FOOD_BEEF_JERKY:
        case FOOD_BREAD_RATION:
        case FOOD_LYCHEE:
        case FOOD_MEAT_RATION:
        case FOOD_RAMBUTAN:
        case FOOD_SAUSAGE:
        case FOOD_SULTANA:
            item.colour = BROWN;
            break;
        case FOOD_BANANA:
        case FOOD_CHEESE:
        case FOOD_HONEYCOMB:
        case FOOD_LEMON:
        case FOOD_PIZZA:
        case FOOD_ROYAL_JELLY:
            item.colour = YELLOW;
            break;
        case FOOD_PEAR:
            item.colour = LIGHTGREEN;
            break;
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
            item.colour = GREEN;
            break;
        case FOOD_APRICOT:
        case FOOD_ORANGE:
            item.colour = LIGHTRED;
            break;
        case FOOD_STRAWBERRY:
            item.colour = RED;
            break;
        case FOOD_APPLE:
            item.colour = (coinflip() ? RED : GREEN);
            break;
        case FOOD_GRAPE:
            item.colour = (coinflip() ? MAGENTA : GREEN);
            break;
        case FOOD_CHUNK:
            // Set the appropriate colour of the meat:
            temp_value  = mons_class_colour(item.plus);
            item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
            break;
        default:
            item.colour = BROWN;
        }
        break;

    case OBJ_JEWELLERY:
        // unrandarts have already been coloured
        if (is_unrandom_artefact(item))
            break;
        else if (is_random_artefact(item))
        {
            item.colour = random_colour();
            break;
        }

        item.colour  = YELLOW;
        item.special = you.item_description[IDESC_RINGS][item.sub_type];

        switchnum = item.special % 13;

        switch (switchnum)
        {
        case 0:
        case 5:
            item.colour = BROWN;
            break;
        case 1:
        case 8:
        case 11:
            item.colour = LIGHTGREY;
            break;
        case 2:
        case 6:
            item.colour = YELLOW;
            break;
        case 3:
        case 4:
            item.colour = CYAN;
            break;
        case 7:
            item.colour = BROWN;
            break;
        case 9:
        case 10:
            item.colour = WHITE;
            break;
        case 12:
            item.colour = GREEN;
            break;
        case 13:
            item.colour = LIGHTCYAN;
            break;
        }

        if (item.sub_type >= AMU_FIRST_AMULET)
        {
            switch (switchnum)
            {
            case 0:             // "zirconium amulet"
            case 9:             // "ivory amulet"
            case 11:            // "platinum amulet"
                item.colour = WHITE;
                break;
            case 1:             // "sapphire amulet"
                item.colour = LIGHTBLUE;
                break;
            case 2:             // "golden amulet"
            case 6:             // "brass amulet"
                item.colour = YELLOW;
                break;
            case 3:             // "emerald amulet"
                item.colour = GREEN;
                break;
            case 4:             // "garnet amulet"
            case 8:             // "ruby amulet"
                item.colour = RED;
                break;
            case 5:             // "bronze amulet"
            case 7:             // "copper amulet"
                item.colour = BROWN;
                break;
            case 10:            // "bone amulet"
                item.colour = LIGHTGREY;
                break;
            case 12:            // "jade amulet"
                item.colour = GREEN;
                break;
            case 13:            // "fluorescent amulet"
                item.colour = random_colour();
            }
        }

        // blackened - same for both rings and amulets
        if (item.special / 13 == 5)
            item.colour = DARKGREY;
        break;

    case OBJ_SCROLLS:
        item.colour  = LIGHTGREY;
        item.special = you.item_description[IDESC_SCROLLS][item.sub_type];
        item.plus = you.item_description[IDESC_SCROLLS_II][item.sub_type];
        break;

    case OBJ_BOOKS:
        switch (item.special % 10)
        {
        case 0:
        case 1:
        default:
            item.colour = random_colour();
            break;
        case 2:
            item.colour = (one_chance_in(3) ? BROWN : DARKGREY);
            break;
        case 3:
            item.colour = CYAN;
            break;
        case 4:
            item.colour = LIGHTGREY;
            break;
        }
        break;

    case OBJ_STAVES:
        item.colour  = BROWN;
        item.special = you.item_description[IDESC_STAVES][item.sub_type];
        break;

    case OBJ_ORBS:
        item.colour = ETC_MUTAGENIC;
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
            break;

        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
        case MISC_STONE_OF_EARTH_ELEMENTALS:
            item.colour = BROWN;
            break;

        case MISC_AIR_ELEMENTAL_FAN:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
        case MISC_CRYSTAL_BALL_OF_SEEING:
        case MISC_DISC_OF_STORMS:
        case MISC_HORN_OF_GERYON:
        case MISC_LANTERN_OF_SHADOWS:
            item.colour = LIGHTGREY;
            break;

        case MISC_LAMP_OF_FIRE:
            item.colour = YELLOW;
            break;

        case MISC_BOX_OF_BEASTS:
            item.colour = DARKGREY;
            break;

        case MISC_RUNE_OF_ZOT:
            switch (item.plus)
            {
            case RUNE_DIS:                      // iron
                item.colour = ETC_IRON;
                break;

            case RUNE_COCYTUS:                  // icy
                item.colour = ETC_ICE;
                break;

            case RUNE_TARTARUS:                 // bone
                item.colour = ETC_BONE;
                break;

            case RUNE_SLIME_PITS:               // slimy
                item.colour = ETC_SLIME;
                break;

            case RUNE_SNAKE_PIT:                // serpentine
                item.colour = ETC_POISON;
                break;

            case RUNE_ELVEN_HALLS:              // elven
                item.colour = ETC_ELVEN;
                break;

            case RUNE_VAULTS:                   // silver
                item.colour = ETC_SILVER;
                break;

            case RUNE_TOMB:                     // golden
                item.colour = ETC_GOLD;
                break;

            case RUNE_SWAMP:                    // decaying
                item.colour = ETC_DECAY;
                break;

            case RUNE_SHOALS:                   // barnacled
                item.colour = ETC_WATER;
                break;

            // This one is hardly unique, but colour isn't used for
            // stacking, so we don't have to worry too much about this.
            // - bwr
            case RUNE_DEMONIC:                  // random Pandemonium lords
            {
                element_type types[] =
                    {ETC_EARTH, ETC_ELECTRICITY, ETC_ENCHANT, ETC_HEAL,
                     ETC_BLOOD, ETC_DEATH, ETC_UNHOLY, ETC_VEHUMET, ETC_BEOGH,
                     ETC_CRYSTAL, ETC_SMOKE, ETC_DWARVEN, ETC_ORCISH, ETC_GILA,
                     ETC_KRAKEN};

                item.colour = RANDOM_ELEMENT(types);
                break;
            }

            case RUNE_ABYSSAL:             // random in abyss
                item.colour = ETC_RANDOM;
                break;

            case RUNE_MNOLEG:                   // glowing
                item.colour = ETC_MUTAGENIC;
                break;

            case RUNE_LOM_LOBON:                // magical
                item.colour = ETC_MAGIC;
                break;

            case RUNE_CEREBOV:                  // fiery
                item.colour = ETC_FIRE;
                break;

            case RUNE_GEHENNA:                  // obsidian
            case RUNE_GLOORX_VLOQ:              // dark
            default:
                item.colour = ETC_DARK;
                break;
            }
            break;

        case MISC_EMPTY_EBONY_CASKET:
            item.colour = DARKGREY;
            break;

        default:
            item.colour = random_colour();
            break;
        }
        break;

    case OBJ_CORPSES:
        // Set the appropriate colour of the body:
        temp_value  = mons_class_colour(item.plus);
        item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
        break;

    case OBJ_GOLD:
        item.colour = YELLOW;
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
        return (sub_type == POT_CURE_MUTATION);
    case OBJ_SCROLLS:
        // These scrolls increase knowledge and thus reduce risk.
        switch (sub_type)
        {
        case SCR_DETECT_CURSE:
        case SCR_REMOVE_CURSE:
        case SCR_IDENTIFY:
        case SCR_MAGIC_MAPPING:
            return (true);
        default:
            break;
        }
        break;
    case OBJ_JEWELLERY:
        return (sub_type == RING_TELEPORT_CONTROL
                || sub_type == AMU_RESIST_MUTATION);
    default:
        break;
    }
    return (false);
}

static weapon_type _determine_weapon_subtype(int item_level)
{
    weapon_type rc = WPN_UNKNOWN;

    const weapon_type common_subtypes[] = {
        WPN_KNIFE, WPN_QUARTERSTAFF, WPN_SLING,
        WPN_SPEAR, WPN_HAND_AXE, WPN_MACE,
        WPN_DAGGER, WPN_DAGGER, WPN_CLUB,
        WPN_HAMMER, WPN_WHIP, WPN_SABRE
    };

    const weapon_type rare_subtypes[] = {
        WPN_LAJATANG, WPN_DEMON_WHIP, WPN_DEMON_BLADE,
        WPN_DEMON_TRIDENT, WPN_DOUBLE_SWORD, WPN_EVENINGSTAR,
        WPN_EXECUTIONERS_AXE, WPN_KATANA, WPN_QUICK_BLADE,
        WPN_TRIPLE_SWORD
    };

    if (item_level > 6 && one_chance_in(30)
        && x_chance_in_y(10 + item_level, 100))
    {
        rc = RANDOM_ELEMENT(rare_subtypes);
    }
    else if (x_chance_in_y(20 - item_level, 20))
        rc = RANDOM_ELEMENT(common_subtypes);
    else
    {
        // Pick a weapon based on rarity.
        while (true)
        {
            const int wpntype = random2(NUM_WEAPONS);

            if (x_chance_in_y(weapon_rarity(wpntype), 10))
            {
                rc = static_cast<weapon_type>(wpntype);
                break;
            }
        }
    }
    return rc;
}

static bool _try_make_item_special_unrand(item_def& item, int force_type,
                                          int item_level)
{
    dprf("Making special unrand artefact.");

    bool abyss = item_level == level_id(LEVEL_ABYSS).absdepth();
    int idx = find_okay_unrandart(item.base_type, force_type,
                                  UNRANDSPEC_SPECIAL, abyss);

    if (idx != -1 && make_item_unrandart(item, idx))
        return (true);

    return (false);
}

// Return whether we made an artefact.
static bool _try_make_weapon_artefact(item_def& item, int force_type,
                                      int item_level, bool force_randart = false)
{
    if (item.sub_type != WPN_CLUB && item_level > 2
        && x_chance_in_y(101 + item_level * 3, 4000) || force_randart)
    {
        // Make a randart or unrandart.

        // 1 in 50 randarts are unrandarts.
        if (you.level_type != LEVEL_ABYSS
            && you.level_type != LEVEL_PANDEMONIUM
            && one_chance_in(50) && !force_randart)
        {
            const int idx = find_okay_unrandart(OBJ_WEAPONS, force_type,
                                                UNRANDSPEC_NORMAL);
            if (idx != -1)
            {
                make_item_unrandart(item, idx);
                return (true);
            }
        }

        // The other 98% are normal randarts.
        make_item_randart(item);
        item.plus  = random2(7);
        item.plus2 = random2(7);

        if (one_chance_in(3))
            item.plus += random2(7);

        if (one_chance_in(3))
            item.plus2 += random2(7);

        if (one_chance_in(9))
            item.plus -= random2(7);

        if (one_chance_in(9))
            item.plus2 -= random2(7);

        if (one_chance_in(4))
        {
            do_curse_item(item);
            item.plus  = -random2(6);
            item.plus2 = -random2(6);
        }
        else if ((item.plus < 0 || item.plus2 < 0)
                 && !one_chance_in(3))
        {
            do_curse_item(item);
        }

        if (get_weapon_brand(item) == SPWPN_HOLY_WRATH)
            item.flags &= (~ISFLAG_CURSED);
        return (true);
    }

    // If it isn't an artefact yet, try to make a special unrand artefact.
    if (item_level > 6
        && one_chance_in(12)
        && x_chance_in_y(31 + item_level * 3, 3000))
    {
        return (_try_make_item_special_unrand(item, force_type, item_level));
    }

    return (false);
}

static item_status_flag_type _determine_weapon_race(const item_def& item,
                                                    int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;

    if (item.sub_type > WPN_MAX_RACIAL)
        return rc;

    switch (item_race)
    {
    case MAKE_ITEM_ELVEN:
        rc = ISFLAG_ELVEN;
        break;

    case MAKE_ITEM_DWARVEN:
        rc = ISFLAG_DWARVEN;
        break;

    case MAKE_ITEM_ORCISH:
        rc = ISFLAG_ORCISH;
        break;

    case MAKE_ITEM_RANDOM_RACE:
        if (coinflip())
            break;

        switch (item.sub_type)
        {
        case WPN_CLUB:
            if (coinflip())
                rc = ISFLAG_ORCISH;
            break;

        case WPN_WHIP:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_MACE:
        case WPN_FLAIL:
        case WPN_SPIKED_FLAIL:
        case WPN_GREAT_MACE:
        case WPN_DIRE_FLAIL:
            if (one_chance_in(4))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            break;

        case WPN_MORNINGSTAR:
        case WPN_HAMMER:
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(3))
                rc = ISFLAG_DWARVEN;
            break;

        case WPN_EVENINGSTAR:
            if (one_chance_in(5))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(5))
                rc = ISFLAG_DWARVEN;
            break;

        case WPN_DAGGER:
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_SHORT_SWORD:
        case WPN_SABRE:
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(3))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(3))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_FALCHION:
            if (one_chance_in(5))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(3))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_LONG_SWORD:
            if (one_chance_in(6))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (coinflip())
                rc = ISFLAG_ELVEN;
            break;

        case WPN_GREAT_SWORD:
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            break;

        case WPN_SCIMITAR:
            if (one_chance_in(5))
                rc = ISFLAG_ELVEN;
            if (coinflip())
                rc = ISFLAG_ORCISH;
            break;

        case WPN_HAND_AXE:
        case WPN_WAR_AXE:
        case WPN_BROAD_AXE:
        case WPN_BATTLEAXE:
            if (one_chance_in(3))
                rc = ISFLAG_ORCISH;
            if (coinflip())
                rc = ISFLAG_DWARVEN;
            break;

        case WPN_SPEAR:
        case WPN_TRIDENT:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_HALBERD:
        case WPN_GLAIVE:
        case WPN_BARDICHE:
            if (one_chance_in(5))
                rc = ISFLAG_ORCISH;
            break;

        case WPN_EXECUTIONERS_AXE:
            if (one_chance_in(5))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_DWARVEN;
            break;

        case WPN_QUICK_BLADE:
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_BOW:
            if (one_chance_in(6))
                rc = ISFLAG_ORCISH;
            if (coinflip())
                rc = ISFLAG_ELVEN;
            break;

        case WPN_LONGBOW:
            if (one_chance_in(3))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_CROSSBOW:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_DWARVEN;
            break;

        case WPN_BLOWGUN:
            if (one_chance_in(6))
                rc = ISFLAG_ELVEN;
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            break;
        }
        break;
    }
    return rc;
}

static void _weapon_add_racial_modifiers(item_def& item)
{
    switch (get_equip_race(item))
    {
    case ISFLAG_ORCISH:
        if (coinflip())
            item.plus--;
        if (coinflip())
            item.plus2++;
        break;

    case ISFLAG_ELVEN:
        item.plus += random2(3);
        break;

    case ISFLAG_DWARVEN:
        if (coinflip())
            item.plus++;
        if (coinflip())
            item.plus2++;
        break;
    }
}

static brand_type _determine_weapon_brand(const item_def& item, int item_level)
{
    // Forced ego.
    if (item.special != 0)
        return static_cast<brand_type>(item.special);

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
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
                rc = SPWPN_VAMPIRICISM;
            // **** intentional fall through here ****
        case WPN_MACE:
        case WPN_GREAT_MACE:
        case WPN_FLAIL:
        case WPN_SPIKED_FLAIL:
        case WPN_DIRE_FLAIL:
        case WPN_HAMMER:
            if (one_chance_in(25))
                rc = SPWPN_PAIN;

            if (_got_distortion_roll(item_level))
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
            if (one_chance_in(4))
                rc = SPWPN_RETURNING;

            if (one_chance_in(10))
                rc = SPWPN_PAIN;

            if (one_chance_in(3))
                rc = SPWPN_VENOM;
            // **** intentional fall through here ****

        case WPN_SHORT_SWORD:
        case WPN_SABRE:
            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (one_chance_in(8))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(8))
                rc = SPWPN_PROTECTION;

            if (one_chance_in(8))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(12))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(8))
                rc = SPWPN_DRAINING;

            if (one_chance_in(8))
                rc = SPWPN_SPEED;

            if (one_chance_in(6))
                rc = SPWPN_VENOM;
            break;

        case WPN_FALCHION:
        case WPN_LONG_SWORD:
            if (one_chance_in(12))
                rc = SPWPN_VENOM;
            // **** intentional fall through here ****
        case WPN_SCIMITAR:
            if (one_chance_in(25))
                rc = SPWPN_PAIN;
            // **** intentional fall through here ****
        case WPN_GREAT_SWORD:
        case WPN_DOUBLE_SWORD:
        case WPN_TRIPLE_SWORD:
            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(7))
                rc = SPWPN_PROTECTION;

            if (one_chance_in(12))
                rc = SPWPN_DRAINING;

            if (one_chance_in(7))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(4))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(4) && (rc == SPWPN_NORMAL || one_chance_in(3)))
                rc = SPWPN_VORPAL;
            break;

        case WPN_WAR_AXE:
        case WPN_BROAD_AXE:
        case WPN_BATTLEAXE:
        case WPN_EXECUTIONERS_AXE:
            if (one_chance_in(25))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(14))
                rc = SPWPN_DRAINING;
            // **** intentional fall through here ****
        case WPN_HAND_AXE:
            if (one_chance_in(30))
                rc = SPWPN_PAIN;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (item.sub_type == WPN_HAND_AXE && one_chance_in(10))
                rc = SPWPN_RETURNING;

            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(3) && (rc == SPWPN_NORMAL || one_chance_in(5)))
                rc = SPWPN_VORPAL;

            if (one_chance_in(4))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(8))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(12))
                rc = SPWPN_VENOM;
            break;

        case WPN_WHIP:
            if (one_chance_in(12))
                rc = SPWPN_HOLY_WRATH;

            if (one_chance_in(10))
                rc = SPWPN_PAIN;

            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(3))
                rc = SPWPN_REACHING;

            if (one_chance_in(8))
                rc = SPWPN_DRAINING;

            if (one_chance_in(6))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(5))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(6))
                rc = SPWPN_VENOM;
            break;

        case WPN_HALBERD:
        case WPN_GLAIVE:
        case WPN_SCYTHE:
        case WPN_TRIDENT:
        case WPN_BARDICHE:
            if (one_chance_in(30))
                rc = SPWPN_HOLY_WRATH;

            if (item.sub_type == WPN_SCYTHE && one_chance_in(6))
                rc = SPWPN_REAPING;

            if (one_chance_in(4))
                rc = SPWPN_PROTECTION;
            // **** intentional fall through here ****
        case WPN_SPEAR:
            if (one_chance_in(25))
                rc = SPWPN_PAIN;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (item.sub_type == WPN_SPEAR && one_chance_in(6))
                rc = SPWPN_RETURNING;

            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5) && (rc == SPWPN_NORMAL || one_chance_in(6)))
                rc = SPWPN_VORPAL;

            if (one_chance_in(6))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(6))
                rc = SPWPN_VENOM;

            if (one_chance_in(5))
                rc = SPWPN_DRAGON_SLAYING;

            if (one_chance_in(3))
                rc = SPWPN_REACHING;
            break;

        case WPN_SLING:
            if (coinflip())
                break;
            // **** possible intentional fall through here ****
        case WPN_BOW:
        case WPN_LONGBOW:
        case WPN_CROSSBOW:
        {
            const int tmp = random2(1000);
            if (tmp < 480)
                rc = SPWPN_FLAME;
            else if (tmp < 730)
                rc = SPWPN_FROST;
            else if (tmp < 880 && (item.sub_type == WPN_BOW || item.sub_type == WPN_LONGBOW))
                rc = SPWPN_REAPING;
            else if (tmp < 880)
                rc = SPWPN_EVASION;
            else if (tmp < 990)
                rc = SPWPN_VORPAL;

            break;
        }

        // Quarterstaff - not powerful, as this would make the 'staves'
        // skill just too good.
        case WPN_QUARTERSTAFF:
            if (one_chance_in(30))
                rc = SPWPN_PAIN;

            if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5))
                rc = SPWPN_SPEED;

            if (one_chance_in(10))
                rc = SPWPN_VORPAL;

            if (one_chance_in(6))
                rc = SPWPN_PROTECTION;
            break;

        case WPN_LAJATANG:
            if (one_chance_in(8))
                rc = SPWPN_SPEED;
            else if (one_chance_in(12))
                rc = SPWPN_PAIN;
            else if (_got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;
            else if (one_chance_in(9))
                rc = SPWPN_PROTECTION;
            else if (one_chance_in(6))
                rc = SPWPN_ELECTROCUTION;
            else if (one_chance_in(5))
                rc = SPWPN_VAMPIRICISM;
            else if (one_chance_in(6))
                rc = SPWPN_VENOM;
            break;

        case WPN_DEMON_WHIP:
        case WPN_DEMON_BLADE:
        case WPN_DEMON_TRIDENT:
            if (one_chance_in(5))	// 4.9%, 7.3% blades
                rc = SPWPN_VAMPIRICISM;

            if (one_chance_in(10))	// 2.7%, 4.0% blades
                rc = SPWPN_PAIN;

            if (one_chance_in(3)	// 13.6%, 0% blades
                && (item.sub_type == WPN_DEMON_WHIP
                    || item.sub_type == WPN_DEMON_TRIDENT))
            {
                rc = SPWPN_REACHING;
            }

            if (one_chance_in(5))	// 10.2%
                rc = SPWPN_DRAINING;

            if (one_chance_in(5))	// 12.8%
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(5))	// 16%
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(5))	// 20%
                rc = SPWPN_VENOM;
            break;

        case WPN_BLESSED_FALCHION:     // special gifts of TSO
        case WPN_BLESSED_LONG_SWORD:
        case WPN_BLESSED_SCIMITAR:
        case WPN_BLESSED_KATANA:
        case WPN_HOLY_BLADE:
        case WPN_BLESSED_DOUBLE_SWORD:
        case WPN_BLESSED_GREAT_SWORD:
        case WPN_BLESSED_TRIPLE_SWORD:
        case WPN_HOLY_SCOURGE:
            rc = SPWPN_HOLY_WRATH;
            break;

        default:
            // unlisted weapons have no associated, standard ego-types {dlb}
            break;
        }
    }

    ASSERT(is_weapon_brand_ok(item.sub_type, rc));
    return (rc);
}

// Reject brands which are outright bad for the item.  Unorthodox combinations
// are ok, since they can happen on randarts.
bool is_weapon_brand_ok(int type, int brand)
{
    item_def item;
    item.base_type = OBJ_WEAPONS;
    item.sub_type = type;

    int vorp = get_vorpal_type(item);
    int skill = weapon_skill(OBJ_WEAPONS, type);

    if (brand <= SPWPN_NORMAL)
        return (true);

    if (type == WPN_QUICK_BLADE && brand == SPWPN_SPEED)
        return (false);

    if (vorp == DVORP_CRUSHING && brand == SPWPN_VENOM)
        return (false);

    if (skill != SK_POLEARMS && brand == SPWPN_DRAGON_SLAYING)
        return (false);

    switch ((brand_type)brand)
    {
    // Universal brands.
    case SPWPN_NORMAL:
    case SPWPN_VENOM:
    case SPWPN_PROTECTION:
    case SPWPN_SPEED:
    case SPWPN_VORPAL:
    case SPWPN_CHAOS:
    case SPWPN_REAPING:
    case SPWPN_HOLY_WRATH:
    case SPWPN_ELECTROCUTION:
        break;

    // Melee-only brands.
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:
    case SPWPN_ORC_SLAYING:
    case SPWPN_DRAGON_SLAYING:
    case SPWPN_DRAINING:
    case SPWPN_VAMPIRICISM:
    case SPWPN_PAIN:
    case SPWPN_DISTORTION:
    case SPWPN_REACHING:
    case SPWPN_RETURNING:
    case SPWPN_CONFUSE:
        if (is_range_weapon(item))
            return (false);
        break;

    // Ranged-only brands.
    case SPWPN_FLAME:
    case SPWPN_FROST:
    case SPWPN_PENETRATION:
    case SPWPN_EVASION:
        if (!is_range_weapon(item))
            return (false);
        break;

    case SPWPN_ACID:
    case SPWPN_FORBID_BRAND:
    case SPWPN_DEBUG_RANDART:
    case NUM_SPECIAL_WEAPONS:
    case NUM_REAL_SPECIAL_WEAPONS:
    case SPWPN_DUMMY_CRUSHING:
        ASSERT(!"invalid brand");
        break;
    }

    if (brand == SPWPN_RETURNING && !is_throwable(&you, item, true))
        return (false);

    return (true);
}

static void _generate_weapon_item(item_def& item, bool allow_uniques,
                                  int force_type, int item_level,
                                  int item_race)
{
    // Determine weapon type.
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        int i;
        for (i = 0; i < 1000; ++i)
        {
            item.sub_type = _determine_weapon_subtype(item_level);
            if (is_weapon_brand_ok(item.sub_type, item.special))
                goto brand_ok;
        }
        item.sub_type = SPWPN_NORMAL; // fall back to no brand
    }
brand_ok:

    // Forced randart.
    if (item_level == -6)
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
    const bool force_good = (item_level == MAKE_GOOD_ITEM);
    const bool forced_ego = item.special > 0;
    const bool no_brand   = item.special == SPWPN_FORBID_BRAND;

    if (no_brand)
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);

    // If it's forced to be a good item, upgrade the worst weapons.
    if (force_good
        && force_type == OBJ_RANDOM
        && (item.sub_type == WPN_CLUB || item.sub_type == WPN_SLING))
    {
        item.sub_type = WPN_LONG_SWORD;
    }

    item.plus  = 0;
    item.plus2 = 0;

    set_equip_race(item, _determine_weapon_race(item, item_race));

    // If we allow acquirement-type items to be orcish, then there's a
    // good chance that we'll just strip them of their ego type at the
    // bottom of this function. - bwr
    if (force_good && !forced_ego && get_equip_race(item) == ISFLAG_ORCISH)
        set_equip_race(item, ISFLAG_NO_RACE);

    _weapon_add_racial_modifiers(item);

    if (item_level < 0)
    {
        // Thoroughly damaged, could had been good once.
        if (!no_brand && (forced_ego || one_chance_in(4)))
        {
            // Brand is set as for "good" items.
            set_item_ego_type(item, OBJ_WEAPONS,
                              _determine_weapon_brand(item, 2+2*you.absdepth0));
        }
        item.plus  -= 1 + random2(3);
        item.plus2 -= 1 + random2(3);

        if (item_level == -5)
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
        {
            item.plus  += 2 + random2(3);
            item.plus2 += 2 + random2(3);
        }

        const int chance = (force_good ? 200 : item_level);

        // Odd-looking, but this is how the algorithm compacts {dlb}.
        for (int i = 0; i < 4; ++i)
        {
            item.plus += random2(3);

            if (random2(350) > 20 + chance)
                break;
        }

        // Odd-looking, but this is how the algorithm compacts {dlb}.
        for (int i = 0; i < 4; ++i)
        {
            item.plus2 += random2(3);

            if (random2(500) > 50 + chance)
                break;
        }
    }
    else
    {
        if (one_chance_in(12))
        {
            // Make a cursed item.
            do_curse_item(item);
            item.plus  -= random2(4);
            item.plus2 -= random2(4);
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
        }
    }

    if (get_equip_race(item) == ISFLAG_ORCISH
        && !(item_race == MAKE_ITEM_ORCISH && forced_ego))
    {
        // No orc slaying, and no ego at all half the time.
        const int brand = get_weapon_brand(item);
        if (brand == SPWPN_ORC_SLAYING
            || (brand != SPWPN_NORMAL && !forced_ego && coinflip()))
        {
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
        }
    }
}

static item_status_flag_type _determine_missile_race(const item_def& item,
                                                     int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;

    if (item.sub_type > MI_MAX_RACIAL)
        return rc;

    switch (item_race)
    {
    case MAKE_ITEM_ELVEN:
        rc = ISFLAG_ELVEN;
        break;

    case MAKE_ITEM_DWARVEN:
        rc = ISFLAG_DWARVEN;
        break;

    case MAKE_ITEM_ORCISH:
        rc = ISFLAG_ORCISH;
        break;

    case MAKE_ITEM_RANDOM_RACE:
        // Elves don't make bolts, sling bullets, or throwing nets.
        if ((item.sub_type == MI_ARROW
                || item.sub_type == MI_DART
                || item.sub_type == MI_JAVELIN)
            && one_chance_in(4))
        {
            rc = ISFLAG_ELVEN;
        }

        // Orcs don't make sling bullets or throwing nets.
        if ((item.sub_type == MI_ARROW
                || item.sub_type == MI_BOLT
                || item.sub_type == MI_DART
                || item.sub_type == MI_JAVELIN)
            && one_chance_in(4))
        {
            rc = ISFLAG_ORCISH;
        }

        // Dwarves don't make arrows, sling bullets, javelins, or
        // throwing nets.
        if ((item.sub_type == MI_DART || item.sub_type == MI_BOLT)
            && one_chance_in(6))
        {
            rc = ISFLAG_DWARVEN;
        }

        // Dwarves don't make needles.
        if (item.sub_type == MI_NEEDLE)
        {
            if (one_chance_in(10))
                rc = ISFLAG_ELVEN;
            if (one_chance_in(6))
                rc = ISFLAG_ORCISH;
        }
        break;
    }
    return rc;
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

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
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

        rc = static_cast<special_missile_type>(
                         random_choose_weighted(20, SPMSL_SLEEP, 20, SPMSL_SLOW,
                                                20, SPMSL_SICKNESS, 20, SPMSL_CONFUSION,
                                                10, SPMSL_PARALYSIS, 10, SPMSL_RAGE,
                                                nw, SPMSL_POISONED, 0));
        break;
    case MI_DART:
        rc = static_cast<special_missile_type>(
                         random_choose_weighted(30, SPMSL_FLAME, 30, SPMSL_FROST,
                                                20, SPMSL_POISONED, 12, SPMSL_REAPING,
                                                12, SPMSL_SILVER, 12, SPMSL_STEEL,
                                                12, SPMSL_DISPERSAL, 20, SPMSL_EXPLODING,
                                                nw, SPMSL_NORMAL,
                                                0));
        break;
    case MI_ARROW:
        rc = static_cast<special_missile_type>(
                        random_choose_weighted(30, SPMSL_FLAME, 30, SPMSL_FROST,
                                               20, SPMSL_POISONED, 15, SPMSL_REAPING,
                                               15, SPMSL_DISPERSAL,
                                               nw, SPMSL_NORMAL,
                                               0));
        break;
    case MI_BOLT:
        rc = static_cast<special_missile_type>(
                        random_choose_weighted(30, SPMSL_FLAME, 30, SPMSL_FROST,
                                               20, SPMSL_POISONED, 15, SPMSL_PENETRATION,
                                               15, SPMSL_SILVER,
                                               10, SPMSL_STEEL, nw, SPMSL_NORMAL,
                                               0));
        break;
    case MI_JAVELIN:
        rc = static_cast<special_missile_type>(
                        random_choose_weighted(30, SPMSL_RETURNING, 32, SPMSL_PENETRATION,
                                               32, SPMSL_POISONED,
                                               21, SPMSL_STEEL, 20, SPMSL_SILVER,
                                               nw, SPMSL_NORMAL,
                                               0));
        break;
    case MI_STONE:
        // deliberate fall through
    case MI_LARGE_ROCK:
        // Stones get no brands. Slings may be branded.
        rc = SPMSL_NORMAL;
        break;
    case MI_SLING_BULLET:
        rc = static_cast<special_missile_type>(
                        random_choose_weighted(30, SPMSL_FLAME, 30, SPMSL_FROST,
                                               20, SPMSL_POISONED,
                                               15, SPMSL_STEEL, 15, SPMSL_SILVER,
                                               20, SPMSL_EXPLODING, nw, SPMSL_NORMAL,
                                               0));
        break;
    case MI_THROWING_NET:
        rc = static_cast<special_missile_type>(
                        random_choose_weighted(30, SPMSL_STEEL, 30, SPMSL_SILVER,
                                               nw, SPMSL_NORMAL,
                                               0));
        break;
    }

    // Orcish ammo gets poisoned a lot more often.
    if (get_equip_race(item) == ISFLAG_ORCISH && one_chance_in(3))
        rc = SPMSL_POISONED;

    ASSERT(is_missile_brand_ok(item.sub_type, rc));

    return rc;
}

bool is_missile_brand_ok(int type, int brand)
{
    // Stones can never be branded.
    if ((type == MI_STONE || type == MI_LARGE_ROCK) && brand != SPMSL_NORMAL)
        return (false);

    // In contrast, needles should always be branded.
    if (type == MI_NEEDLE)
    {
        switch (brand)
        {
        case SPMSL_POISONED: case SPMSL_CURARE: case SPMSL_PARALYSIS:
        case SPMSL_SLOW: case SPMSL_SLEEP: case SPMSL_CONFUSION:
        case SPMSL_SICKNESS: case SPMSL_RAGE: return (true);
        default: return (false);
        }
    }

    // Everything else doesn't matter.
    if (brand == SPMSL_NORMAL)
        return (true);

    // Not a missile?
    if (type == 0)
        return (true);

    // Specifics
    switch (brand)
    {
    case SPMSL_FLAME:
        return (type == MI_SLING_BULLET || type == MI_ARROW
                || type == MI_BOLT || type == MI_DART);
    case SPMSL_FROST:
        return (type == MI_SLING_BULLET || type == MI_ARROW
                || type == MI_BOLT || type == MI_DART);
    case SPMSL_POISONED:
        return (type == MI_SLING_BULLET || type == MI_ARROW
                || type == MI_BOLT || type == MI_DART
                || type == MI_JAVELIN);
    case SPMSL_RETURNING:
        return (type == MI_JAVELIN);
    case SPMSL_CHAOS:
        return (type == MI_SLING_BULLET || type == MI_ARROW
                || type == MI_BOLT || type == MI_DART
                || type == MI_JAVELIN || type == MI_THROWING_NET);
    case SPMSL_PENETRATION:
        return (type == MI_JAVELIN || type == MI_BOLT);
    case SPMSL_REAPING: // deliberate fall through
    case SPMSL_DISPERSAL:
        return (type == MI_ARROW || type == MI_DART);
    case SPMSL_EXPLODING:
        return (type == MI_SLING_BULLET || type == MI_DART);
    case SPMSL_STEEL: // deliberate fall through
    case SPMSL_SILVER:
        return (type == MI_BOLT || type == MI_SLING_BULLET
                || type == MI_JAVELIN || type == MI_THROWING_NET);
    default: break;
    }

    // Assume yes, if we've gotten this far.
    return (false);
}

static void _generate_missile_item(item_def& item, int force_type,
                                   int item_level, int item_race)
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
            random_choose_weighted(30, MI_STONE,
                                   20, MI_DART,
                                   20, MI_ARROW,
                                   12, MI_BOLT,
                                   12, MI_SLING_BULLET,
                                   10, MI_NEEDLE,
                                   2,  MI_JAVELIN,
                                   1,  MI_THROWING_NET,
                                   0);
    }

    // No fancy rocks -- break out before we get to racial/special stuff.
    if (item.sub_type == MI_LARGE_ROCK)
    {
        item.quantity = 2 + random2avg(5,2);
        return;
    }
    else if (item.sub_type == MI_STONE)
    {
        item.quantity = 1 + random2(9) + random2(12) + random2(15) + random2(12);
        return;
    }
    else if (item.sub_type == MI_THROWING_NET) // no fancy nets, either
    {
        item.quantity = 1 + one_chance_in(4); // and only one, rarely two
        return;
    }

    set_equip_race(item, _determine_missile_race(item, item_race));

    if (!no_brand)
    {
        set_item_ego_type( item, OBJ_MISSILES,
                           _determine_missile_brand(item, item_level) );
    }

    // Reduced quantity if special.
    if (item.sub_type == MI_JAVELIN
        || (item.sub_type == MI_NEEDLE && get_ammo_brand(item) != SPMSL_POISONED)
        || get_ammo_brand(item) == SPMSL_RETURNING
        || (item.sub_type == MI_DART && get_ammo_brand(item) == SPMSL_POISONED))
    {
        item.quantity = random_range(2, 8);
    }
    else if (get_ammo_brand( item ) != SPMSL_NORMAL)
        item.quantity = 1 + random2(9) + random2(12) + random2(12);
    else
        item.quantity = 1 + random2(9) + random2(12) + random2(12) + random2(15);

    if (x_chance_in_y(11 + item_level, 100))
        item.plus += random2(5);

    // Elven arrows and dwarven bolts are quality items.
    if (get_equip_race(item) == ISFLAG_ELVEN && item.sub_type == MI_ARROW
        || get_equip_race(item) == ISFLAG_DWARVEN && item.sub_type == MI_BOLT)
    {
        item.plus += random2(3);
    }
}

static bool _try_make_armour_artefact(item_def& item, int force_type,
                                      int item_level, bool force_randart = false)
{
    if (item_level > 2 && x_chance_in_y(101 + item_level * 3, 4000)
        || force_randart)
    {
        // Make a randart or unrandart.

        // 1 in 50 randarts are unrandarts.
        if (you.level_type != LEVEL_ABYSS
            && you.level_type != LEVEL_PANDEMONIUM
            && one_chance_in(50) && !force_randart)
        {
            // The old generation code did not respect force_type here.
            const int idx = find_okay_unrandart(OBJ_ARMOUR, force_type,
                                                UNRANDSPEC_NORMAL);
            if (idx != -1)
            {
                make_item_unrandart(item, idx);
                return (true);
            }
        }

        // The other 98% are normal randarts.

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
            item.plus = random2(4);

            if (one_chance_in(5))
                item.plus += random2(4);

            if (one_chance_in(6))
                item.plus -= random2(8);

            if (item.plus < 0 && !one_chance_in(3))
                do_curse_item(item);
        }

        return (true);
    }

    // If it isn't an artefact yet, try to make a special unrand artefact.
    if (item_level > 6
        && one_chance_in(12)
        && x_chance_in_y(31 + item_level * 3, 3000))
    {
        return (_try_make_item_special_unrand(item, force_type, item_level));
    }

    return (false);
}

static item_status_flag_type _determine_armour_race(const item_def& item,
                                                    int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;

    if (item.sub_type > ARM_MAX_RACIAL)
        return rc;

    switch (item_race)
    {
    case MAKE_ITEM_ELVEN:
        rc = ISFLAG_ELVEN;
        break;

    case MAKE_ITEM_DWARVEN:
        rc = ISFLAG_DWARVEN;
        break;

    case MAKE_ITEM_ORCISH:
        rc = ISFLAG_ORCISH;
        break;

    case MAKE_ITEM_RANDOM_RACE:
        if (coinflip())
            break;

        switch (item.sub_type)
        {
        case ARM_BUCKLER:
        case ARM_SHIELD:
        case ARM_LARGE_SHIELD:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            if (one_chance_in(3))
                rc = ISFLAG_DWARVEN;
            break;

        case ARM_CLOAK:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case ARM_GLOVES:
        case ARM_BOOTS:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            if (one_chance_in(6))
                rc = ISFLAG_DWARVEN;
            break;

        case ARM_CAP:
            if (one_chance_in(6))
                rc = ISFLAG_ELVEN;
            break;

        case ARM_WIZARD_HAT:
            if (one_chance_in(6))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(6))
                rc = ISFLAG_ELVEN;
            break;

        case ARM_HELMET:
            if (one_chance_in(6))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(5))
                rc = ISFLAG_DWARVEN;
            break;

        case ARM_ROBE:
            if (one_chance_in(6))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(4))
                rc = ISFLAG_ELVEN;
            break;

        case ARM_LEATHER_ARMOUR:
        case ARM_RING_MAIL:
        case ARM_SCALE_MAIL:
        case ARM_CHAIN_MAIL:
        case ARM_SPLINT_MAIL:
        case ARM_BANDED_MAIL:
        case ARM_PLATE_MAIL:
            if (item.sub_type <= ARM_CHAIN_MAIL && one_chance_in(6))
                rc = ISFLAG_ELVEN;
            if (item.sub_type >= ARM_RING_MAIL && one_chance_in(5))
                rc = ISFLAG_DWARVEN;
            if (one_chance_in(5))
                rc = ISFLAG_ORCISH;

        default:
            break;
        }
    }

    return (rc);
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
        rc = static_cast<special_armour_type>(
            random_choose_weighted(40, SPARM_RESISTANCE,
                                   120, SPARM_FIRE_RESISTANCE,
                                   120, SPARM_COLD_RESISTANCE,
                                   120, SPARM_POISON_RESISTANCE,
                                   120, SPARM_POSITIVE_ENERGY,
                                   240, SPARM_REFLECTION,
                                   480, SPARM_PROTECTION,
                                   0));
        break;

    case ARM_CLOAK:
    {
        const special_armour_type cloak_egos[] = {
            SPARM_POISON_RESISTANCE, SPARM_DARKNESS,
            SPARM_MAGIC_RESISTANCE, SPARM_PRESERVATION
        };

        rc = RANDOM_ELEMENT(cloak_egos);
        break;
    }

    case ARM_WIZARD_HAT:
        if (coinflip())
        {
            rc = (one_chance_in(3) ? SPARM_MAGIC_RESISTANCE
                                   : SPARM_INTELLIGENCE);
        }
        break;

    case ARM_CAP:
        if (one_chance_in(10))
        {
            rc = SPARM_SPIRIT_SHIELD;
            break;
        }
    case ARM_HELMET:
        rc = coinflip() ? SPARM_SEE_INVISIBLE : SPARM_INTELLIGENCE;
        break;

    case ARM_GLOVES:
        switch (random2(3))
        {
        case 0:
            rc = SPARM_DEXTERITY;
            break;
        case 1:
            rc = SPARM_STRENGTH;
            break;
        default:
            rc = SPARM_ARCHERY;
        }
        break;

    case ARM_BOOTS:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    {
        const int tmp = random2(600) + 200 * (item.sub_type != ARM_BOOTS);

        rc = (tmp < 200) ? SPARM_RUNNING :
             (tmp < 400) ? SPARM_LEVITATION :
             (tmp < 600) ? SPARM_STEALTH :
             (tmp < 700) ? SPARM_COLD_RESISTANCE
                         : SPARM_FIRE_RESISTANCE;
        break;
    }

    case ARM_ROBE:
        rc = static_cast<special_armour_type>(
                 random_choose_weighted(1, SPARM_RESISTANCE,
                                        2, SPARM_COLD_RESISTANCE,
                                        2, SPARM_FIRE_RESISTANCE,
                                        2, SPARM_POSITIVE_ENERGY,
                                        4, SPARM_MAGIC_RESISTANCE,
                                        4, SPARM_ARCHMAGI,
                                        0));

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
            || item.sub_type == ARM_CRYSTAL_PLATE_MAIL)
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

        if (item.sub_type == ARM_PLATE_MAIL && one_chance_in(15))
            rc = SPARM_PONDEROUSNESS;
        break;
    }

    ASSERT(is_armour_brand_ok(item.sub_type, rc));
    return (rc);
}

bool is_armour_brand_ok(int type, int brand)
{
    equipment_type slot = get_armour_slot((armour_type)type);

    // Currently being too restrictive results in asserts, being too
    // permissive will generate such items on "any armour ego:XXX".
    // The latter is definitely so much safer -- 1KB
    switch((special_armour_type)brand)
    {
    case SPARM_FORBID_EGO:
    case SPARM_NORMAL:
        return (true);

    case SPARM_RUNNING:
    case SPARM_LEVITATION:
    case SPARM_STEALTH:
        return (slot == EQ_BOOTS);

    case SPARM_ARCHMAGI:
        return (type == ARM_ROBE);

    case SPARM_PONDEROUSNESS:
        return (true);

    case SPARM_PRESERVATION:
    case SPARM_DARKNESS:
        return (slot == EQ_CLOAK);

    case SPARM_REFLECTION:
    case SPARM_PROTECTION:
        return (slot == EQ_SHIELD);

    case SPARM_ARCHERY:
    case SPARM_STRENGTH:
    case SPARM_DEXTERITY:
        return (slot == EQ_GLOVES);

    case SPARM_SEE_INVISIBLE:
    case SPARM_INTELLIGENCE:
        return (slot == EQ_HELMET);

    case SPARM_FIRE_RESISTANCE:
    case SPARM_COLD_RESISTANCE:
    case SPARM_RESISTANCE:
        if (type == ARM_DRAGON_ARMOUR
            || type == ARM_ICE_DRAGON_ARMOUR
            || type == ARM_GOLD_DRAGON_ARMOUR)
        {
            return (false); // contradictory or redundant
        }
        return (true); // in portal vaults, these can happen on every slot

    case SPARM_MAGIC_RESISTANCE:
        if (type == ARM_WIZARD_HAT)
            return (true);
    case SPARM_POISON_RESISTANCE:
    case SPARM_POSITIVE_ENERGY:
        return (slot == EQ_BODY_ARMOUR || slot == EQ_SHIELD || slot == EQ_CLOAK);

    case SPARM_SPIRIT_SHIELD:
        return (type == ARM_CAP || slot == EQ_SHIELD);

    case NUM_SPECIAL_ARMOURS:
        ASSERT(!"invalid armour brand");
    }

    return (true);
}

static void _generate_armour_item(item_def& item, bool allow_uniques,
                                  int force_type, int item_level,
                                  int item_race)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        int i;
        for (i=0; i<1000; i++)
        {
            item.sub_type = get_random_armour_type(item_level);
            if (is_armour_brand_ok(item.sub_type, item.special))
                break;
        }
    }

    // Forced randart.
    if (item_level == -6)
    {
        int i;
        for (i=0; i<100; i++)
            if (_try_make_armour_artefact(item, force_type, 0, true) && is_artefact(item))
                return;
        // fall back to an ordinary item
        item_level = MAKE_GOOD_ITEM;
    }

    if (allow_uniques
        && _try_make_armour_artefact(item, force_type, item_level))
    {
        return;
    }

    // If we get here the item is not an artefact.
    if (is_helmet(item) && one_chance_in(3))
        set_helmet_random_desc(item);

    if (item_race == MAKE_ITEM_RANDOM_RACE && item.sub_type == ARM_BOOTS)
    {
        if (one_chance_in(8))
            item.sub_type = ARM_NAGA_BARDING;
        else if (one_chance_in(7))
            item.sub_type = ARM_CENTAUR_BARDING;
    }

    set_equip_race(item, _determine_armour_race(item, item_race));

    // Dwarven armour is high-quality.
    if (get_equip_race(item) == ISFLAG_DWARVEN && coinflip())
        item.plus++;

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
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
                _determine_armour_ego(item, item.sub_type, 2+2*you.absdepth0));
        }

        item.plus -= 1 + random2(3);

        if (item_level == -5)
            do_curse_item(item);
    }
    else if ((force_good || forced_ego || item.sub_type == ARM_WIZARD_HAT
                    || x_chance_in_y(51 + item_level, 250))
                && (!item.is_mundane() || force_good))
    {
        // Make a good item...
        item.plus += random2(3);

        if (item.sub_type <= ARM_PLATE_MAIL
            && x_chance_in_y(21 + item_level, 300))
        {
            item.plus += random2(3);
        }

        if (!no_ego
            && x_chance_in_y(31 + item_level, 350)
            && (force_good
                || forced_ego
                || get_equip_race(item) != ISFLAG_ORCISH
                || item.sub_type <= ARM_PLATE_MAIL && coinflip()))
        {
            // ...an ego item, in fact.
            set_item_ego_type(item, OBJ_ARMOUR,
                              _determine_armour_ego(item, force_type,
                                                    item_level));

            if (get_armour_ego_type(item) == SPARM_PONDEROUSNESS)
                item.plus += 3 + random2(4);
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

    if (armour_is_hide(item))
        item.plus = 0;

    if (item.sub_type == ARM_GLOVES)
        set_gloves_random_desc(item);
}

static monster_type _choose_random_monster_corpse()
{
    for (int count = 0; count < 1000; ++count)
    {
        monster_type spc = mons_species(random2(NUM_MONSTERS));
        if (mons_weight(spc) > 0)        // drops a corpse
            return spc;
    }

    return (MONS_RAT);          // if you can't find anything else...
}

static int _random_wand_subtype()
{
    int rc = random2( NUM_WANDS );

    // Adjusted distribution here -- bwr
    // Wands used to be uniform (5.26% each)
    //
    // Now:
    // invis, hasting, healing  (1.11% each)
    // fireball, teleportaion   (3.74% each)
    // others                   (6.37% each)
    if (rc == WAND_INVISIBILITY || rc == WAND_HASTING || rc == WAND_HEALING
        || (rc == WAND_FIREBALL || rc == WAND_TELEPORTATION) && coinflip())
    {
        rc = random2( NUM_WANDS );
    }
    return rc;
}

static int _wand_max_charges(int subtype)
{
    switch (subtype)
    {
    case WAND_HEALING: case WAND_HASTING: case WAND_INVISIBILITY:
        return 8;

    case WAND_FLAME: case WAND_FROST: case WAND_MAGIC_DARTS:
    case WAND_RANDOM_EFFECTS:
        return 28;

    default:
        return 16;
    }
}

static void _generate_wand_item(item_def& item, int force_type)
{
    // Determine sub_type.
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
        item.sub_type = _random_wand_subtype();

    // Generate charges randomly...
    item.plus = random2avg(_wand_max_charges(item.sub_type), 3);

    // ...but 0 charges is silly
    if (item.plus == 0)
        item.plus++;

    // plus2 tracks how many times the player has zapped it.
    // If it is -1, then the player knows it's empty.
    // If it is -2, then the player has messed with it somehow
    // (presumably by recharging), so don't bother to display
    // the count.
    item.plus2 = 0;
}

static void _generate_food_item(item_def& item, int force_quant, int force_type)
{
    // Determine sub_type:
    if (force_type == OBJ_RANDOM)
    {
        item.sub_type = random_choose_weighted( 250, FOOD_MEAT_RATION,
                                                300, FOOD_BREAD_RATION,
                                                100, FOOD_PEAR,
                                                100, FOOD_APPLE,
                                                100, FOOD_CHOKO,
                                                 10, FOOD_CHEESE,
                                                 10, FOOD_PIZZA,
                                                 10, FOOD_SNOZZCUMBER,
                                                 10, FOOD_APRICOT,
                                                 10, FOOD_ORANGE,
                                                 10, FOOD_BANANA,
                                                 10, FOOD_STRAWBERRY,
                                                 10, FOOD_RAMBUTAN,
                                                 10, FOOD_LEMON,
                                                 10, FOOD_GRAPE,
                                                 10, FOOD_SULTANA,
                                                 10, FOOD_LYCHEE,
                                                 10, FOOD_BEEF_JERKY,
                                                 10, FOOD_SAUSAGE,
                                                  5, FOOD_HONEYCOMB,
                                                  5, FOOD_ROYAL_JELLY,
                                                  0);
    }
    else
        item.sub_type = force_type;

    // Happens with ghoul food acquirement -- use place_chunks() outherwise
    if (item.sub_type == FOOD_CHUNK)
    {
        // Set chunk flavour (default to common dungeon rat steaks):
        item.plus = _choose_random_monster_corpse();
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

            if (item.sub_type == FOOD_STRAWBERRY
                || item.sub_type == FOOD_GRAPE
                || item.sub_type == FOOD_SULTANA)
            {
                item.quantity += 3 + random2avg(15,2);
            }
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
        do
        {
            // total weight is NOT 10000
            stype = random_choose_weighted( 2815, POT_HEALING,
                                            1407, POT_HEAL_WOUNDS,
                                             900, POT_RESTORE_ABILITIES,
                                             648, POT_POISON,
                                             612, POT_SPEED,
                                             612, POT_MIGHT,
                                             612, POT_AGILITY,
                                             612, POT_BRILLIANCE,
                                             340, POT_INVISIBILITY,
                                             340, POT_LEVITATION,
                                             340, POT_RESISTANCE,
                                             340, POT_MAGIC,
                                             324, POT_MUTATION,
                                             324, POT_SLOWING,
                                             324, POT_PARALYSIS,
                                             324, POT_CONFUSION,
                                             278, POT_DEGENERATION,
                                             222, POT_CURE_MUTATION,
                                             162, POT_STRONG_POISON,
                                             136, POT_BERSERK_RAGE,
                                             111, POT_BLOOD,
                                              70, POT_PORRIDGE,
                                              38, POT_GAIN_STRENGTH,
                                              38, POT_GAIN_DEXTERITY,
                                              38, POT_GAIN_INTELLIGENCE,
                                              13, POT_EXPERIENCE,
                                              10, POT_DECAY,
                                               0);
        }
        while (stype == POT_POISON && item_level < 1
               || stype == POT_STRONG_POISON && item_level < 11
               || agent == GOD_XOM && _is_boring_item(OBJ_POTIONS, stype));

        if (stype == POT_GAIN_STRENGTH || stype == POT_GAIN_DEXTERITY
            || stype == POT_GAIN_INTELLIGENCE || stype == POT_EXPERIENCE
            || stype == POT_RESTORE_ABILITIES)
        {
            item.quantity = 1;
        }

        item.sub_type = stype;
    }

    if (is_blood_potion(item))
        init_stack_blood_potions(item);
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

        do
        {
            // total weight: 10000
            item.sub_type = random_choose_weighted(
                1797, SCR_IDENTIFY,
                1305, SCR_REMOVE_CURSE,
                // [Cha] don't generate teleportation scrolls if in sprint
		(crawl_state.game_is_sprint() ? 0 : 802), SCR_TELEPORTATION,
                 642, SCR_DETECT_CURSE,
                 331, SCR_FEAR,
                // [Cha] don't generate noise scrolls if in sprint
                (crawl_state.game_is_sprint() ? 0 : 331), SCR_NOISE,
                 331, SCR_MAGIC_MAPPING,
                 331, SCR_FOG,
                 331, SCR_RANDOM_USELESSNESS,
                 331, SCR_CURSE_WEAPON,
                 331, SCR_CURSE_ARMOUR,
                 331, SCR_RECHARGING,
                 331, SCR_BLINKING,
                 331, SCR_ENCHANT_ARMOUR,
                 331, SCR_ENCHANT_WEAPON_I,
                 331, SCR_ENCHANT_WEAPON_II,

                 // Don't create ?oImmolation at low levels (encourage read-ID).
                 331, (item_level < 4 ? SCR_TELEPORTATION : SCR_IMMOLATION),

                 161, SCR_PAPER,

                 // Medium-level scrolls.
                 140, (depth_mod < 4 ? SCR_TELEPORTATION : SCR_ACQUIREMENT),
                 140, (depth_mod < 4 ? SCR_TELEPORTATION : SCR_ENCHANT_WEAPON_III),
                 140, (depth_mod < 4 ? SCR_DETECT_CURSE  : SCR_SUMMONING),
                 140, (depth_mod < 4 ? SCR_PAPER :         SCR_VULNERABILITY),

                 // High-level scrolls.
                 140, (depth_mod < 7 ? SCR_TELEPORTATION : SCR_VORPALISE_WEAPON),
                 140, (depth_mod < 7 ? SCR_DETECT_CURSE  : SCR_TORMENT),
                 140, (depth_mod < 7 ? SCR_DETECT_CURSE  : SCR_HOLY_WORD),

                 // Balanced by rarity.
                 10, SCR_SILENCE,

                 0);
        }
        while (agent == GOD_XOM && _is_boring_item(OBJ_SCROLLS, item.sub_type));
    }

    // determine quantity
    if (item.sub_type == SCR_VORPALISE_WEAPON
        || item.sub_type == SCR_ENCHANT_WEAPON_III
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

static void _generate_book_item(item_def& item, int allow_uniques,
                                int force_type, int item_level)
{
    // determine special (description)
    item.special = random2(5);

    if (one_chance_in(10))
        item.special += random2(8) * 10;

    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
        {
            item.sub_type = random2(NUM_FIXED_BOOKS);

            if (book_rarity(item.sub_type) != 100
                && one_chance_in(10))
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
            item.plus = SK_SPELLCASTING + random2(NUM_SKILLS - SK_SPELLCASTING);
        else
            item.plus = random2(SK_UNARMED_COMBAT);
        // Set number of reads possible before it "crumbles to dust".
        item.plus2 = 3 + random2(15);
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
        int max_level  = std::min( 9, std::max(1, item_level / 3) );
        int spl_level  = random_range(1, max_level);
        int max_spells = 5 + spl_level/3;
        make_book_level_randart(item, spl_level, max_spells);
    }
}

static void _generate_staff_item(item_def& item, int force_type, int item_level)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        item.sub_type = random2(STAFF_FIRST_ROD);

        // rods are rare (10% of all staves)
        if (one_chance_in(10))
            item.sub_type = random_rod_subtype();

        // staves of energy/channeling are 25% less common, wizardry/power
        // are more common
        if ((item.sub_type == STAFF_ENERGY
                || item.sub_type == STAFF_CHANNELING)
            && one_chance_in(4))
        {
            item.sub_type = coinflip() ? STAFF_WIZARDRY : STAFF_POWER;
        }
    }

    if (item_is_rod(item))
        init_rod_mp(item, -1, item_level);
}

static bool _try_make_jewellery_unrandart(item_def& item, int force_type,
                                          int item_level)
{
    bool rc = false;

    if (item_level > 2
        && you.level_type != LEVEL_ABYSS
        && you.level_type != LEVEL_PANDEMONIUM
        && one_chance_in(20)
        && x_chance_in_y(101 + item_level * 3, 2000))
    {
        // The old generation code did not respect force_type here.
        const int idx = find_okay_unrandart(OBJ_JEWELLERY, force_type,
                                            UNRANDSPEC_NORMAL);
        if (idx != -1)
        {
            make_item_unrandart(item, idx);
            rc = true;
        }
    }

    return (rc);
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
        if (one_chance_in(5))       // 20% of such rings are cursed {dlb}
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
    if (allow_uniques
        && _try_make_jewellery_unrandart(item, force_type, item_level))
    {
        return;
    }

    // Determine subtype.
    // Note: removed double probability reduction for some subtypes
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
        {
            item.sub_type = (one_chance_in(4) ? get_random_amulet_type()
                                              : get_random_ring_type());
        }
        while (agent == GOD_XOM
               && _is_boring_item(OBJ_JEWELLERY, item.sub_type));
    }

    // Everything begins as uncursed, unenchanted jewellery {dlb}:
    item.plus  = 0;
    item.plus2 = 0;

    item.plus = _determine_ring_plus(item.sub_type);
    if (item.plus < 0)
        do_curse_item(item);

    if (item.sub_type == RING_SLAYING ) // requires plus2 too
    {
        if (item.cursed() && !one_chance_in(20))
            item.plus2 = -1 - random2avg(6, 2);
        else
        {
            item.plus2 = 1 + (one_chance_in(3) ? random2(3) : random2avg(6, 2));

            if (x_chance_in_y(9, 25))        // 36% of such rings {dlb}
            {
                // make "ring of damage"
                do_uncurse_item(item);
                item.plus   = 0;
                item.plus2 += 2;
            }
        }
    }

    // All jewellery base types should now work. - bwr
    if (allow_uniques && item_level > 2
        && x_chance_in_y(101 + item_level * 3, 4000))
    {
        make_item_randart(item);
    }
    // If it isn't an artefact yet, try to make a special unrand artefact.
    else if (item_level > 6
             && one_chance_in(12)
             && x_chance_in_y(31 + item_level * 3, 3000))
    {
        _try_make_item_special_unrand(item, force_type, item_level);
    }
    else if (item.sub_type == RING_HUNGER || item.sub_type == RING_TELEPORTATION
             || one_chance_in(50))
    {
        // Rings of hunger and teleportation are always cursed {dlb}:
        do_curse_item(item);
    }
}

static void _generate_misc_item(item_def& item, int force_type, int item_race)
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
             // Pure decks are rare in the dungeon.
             || (item.sub_type == MISC_DECK_OF_ESCAPE
                    || item.sub_type == MISC_DECK_OF_DESTRUCTION
                    || item.sub_type == MISC_DECK_OF_DUNGEONS
                    || item.sub_type == MISC_DECK_OF_SUMMONING
                    || item.sub_type == MISC_DECK_OF_WONDERS)
                 && !one_chance_in(5));

        // filling those silly empty boxes -- bwr
        if (item.sub_type == MISC_EMPTY_EBONY_CASKET && !one_chance_in(20))
            item.sub_type = MISC_BOX_OF_BEASTS;
    }

    if ( is_deck(item) )
    {
        item.plus = 4 + random2(10);
        item.special = random_choose_weighted( 8, DECK_RARITY_LEGENDARY,
                                              20, DECK_RARITY_RARE,
                                              72, DECK_RARITY_COMMON,
                                               0);
        init_deck(item);
    }
}

// Returns item slot or NON_ITEM if it fails.
int items(int allow_uniques,       // not just true-false,
                                   // because of BCR acquirement hack
          object_class_type force_class, // desired OBJECTS class {dlb}
          int force_type,          // desired SUBTYPE - enum varies by OBJ
          bool dont_place,         // don't randomly place item on level
          int item_level,          // level of the item, can differ from global
          int item_race,           // weapon / armour racial categories
                                   // item_race also gives type of rune!
          unsigned mapmask,
          int force_ego,           // desired ego/brand
          int agent)               // acquirement agent, if not -1
{
    ASSERT(force_ego <= 0
           || force_class == OBJ_WEAPONS || force_class == OBJ_ARMOUR
           || force_class == OBJ_MISSILES);

    // Find an empty slot for the item (with culling if required).
    int p = get_item_slot(10);
    if (p == NON_ITEM)
        return (NON_ITEM);

    item_def& item(mitm[p]);

    // make_item_randart() might do things differently based upon the
    // acquirement agent, especially for god gifts.
    if (agent != -1)
        origin_acquired(item, agent);

    const bool force_good = (item_level == MAKE_GOOD_ITEM);

    if (force_ego != 0)
        allow_uniques = false;

    item.special = force_ego;

    // cap item_level unless an acquirement-level item {dlb}:
    if (item_level > 50 && !force_good)
        item_level = 50;

    // determine base_type for item generated {dlb}:
    if (force_class != OBJ_RANDOM)
        item.base_type = force_class;
    else
    {
        item.base_type = static_cast<object_class_type>(
            random_choose_weighted(  5, OBJ_STAVES,
                                    15, OBJ_BOOKS,
                                    25, OBJ_JEWELLERY,
                                    35, OBJ_WANDS,
                                    70, OBJ_FOOD,
                                   100, OBJ_ARMOUR,
                                   100, OBJ_WEAPONS,
                                   100, OBJ_POTIONS,
                                   150, OBJ_MISSILES,
                                   200, OBJ_SCROLLS,
                                   200, OBJ_GOLD,
                                     0));

        // misc items placement wholly dependent upon current depth {dlb}:
        if (item_level > 7 && x_chance_in_y(21 + item_level, 3500))
            item.base_type = OBJ_MISCELLANY;

        if (item_level < 7
            && (item.base_type == OBJ_BOOKS
                || item.base_type == OBJ_STAVES
                || item.base_type == OBJ_WANDS)
            && random2(7) >= item_level)
        {
            item.base_type = coinflip() ? OBJ_POTIONS : OBJ_SCROLLS;
        }
    }

    item.quantity = 1;          // generally the case

    if (force_ego < SP_FORBID_EGO)
    {
        force_ego = -force_ego;
        if (get_unique_item_status(force_ego) == UNIQ_NOT_EXISTS)
        {
            make_item_unrandart(mitm[p], force_ego);
            return (p);
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
                              item_level, item_race);
        break;

    case OBJ_MISSILES:
        _generate_missile_item(item, force_type, item_level, item_race);
        break;

    case OBJ_ARMOUR:
        _generate_armour_item(item, allow_uniques, force_type,
                              item_level, item_race);
        break;

    case OBJ_WANDS:
        _generate_wand_item(item, force_type);
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
        _generate_staff_item(item, force_type, item_level);
        break;

    case OBJ_ORBS:              // always forced in current setup {dlb}
        item.sub_type = force_type;
        break;

    case OBJ_MISCELLANY:
        _generate_misc_item(item, force_type, item_race);
        break;

    // that is, everything turns to gold if not enumerated above, so ... {dlb}
    default:
        item.base_type = OBJ_GOLD;
        if (force_good)
        {
            // New gold acquirement formula from dpeg.
            // Min=220, Max=5520, Mean=1218, Std=911
            item.quantity = 10 * (20
                                  + roll_dice(1, 20)
                                  + (roll_dice(1, 8)
                                     * roll_dice(1, 8)
                                     * roll_dice(1, 8)));
        }
        else
            item.quantity = 1 + random2avg(19, 2) + random2(item_level);
        break;
    }

    if (item.base_type == OBJ_WEAPONS
          && !is_weapon_brand_ok(item.sub_type, item.special)
        || item.base_type == OBJ_ARMOUR
          && !is_armour_brand_ok(item.sub_type, item.special)
        || item.base_type == OBJ_MISSILES
          && !is_missile_brand_ok(item.sub_type, item.special))
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

    if (dont_place)
    {
        item.pos.reset();
        item.link = NON_ITEM;
    }
    else
    {
        coord_def itempos;
        bool found = false;
        for (int i = 0; i < 500 && !found; ++i)
        {
            itempos = random_in_bounds();
            found = (grd(itempos) == DNGN_FLOOR
                     && unforbidden(itempos, mapmask));
        }
        if (!found)
        {
            // Couldn't find a single good spot!
            destroy_item(p);
            return (NON_ITEM);
        }
        move_item_to_grid(&p, itempos);
    }

    // Note that item might be invalidated now, since p could have changed.
    ASSERT(mitm[p].is_valid());
    return (p);
}

void reroll_brand(item_def &item, int item_level)
{
    ASSERT(!is_artefact(item));
    switch(item.base_type)
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
        ASSERT(!"can't reroll brands of this type");
    }
}

static int _roll_rod_enchant(int item_level)
{
    int value = 0;

    if (one_chance_in(4))
        value -= random_range(1, 3);

    if (item_level == MAKE_GOOD_ITEM)
        value += 2;

    int pr = 20 + item_level * 2;

    if (pr > 80)
        pr = 80;

    while (random2(100) < pr) value++;

    return stepdown_value(value, 4, 4, 4, 9);
}

void init_rod_mp(item_def &item, int ncharges, int item_level)
{
    if (!item_is_rod(item))
        return;

    if (ncharges != -1)
    {
        item.plus2 = ncharges * ROD_CHARGE_MULT;
        item.props["rod_enchantment"] = (short)0;
    }
    else
    {
        if (item.sub_type == STAFF_STRIKING)
            item.plus2 = random_range(6, 9) * ROD_CHARGE_MULT;
        else
            item.plus2 = random_range(9, 14) * ROD_CHARGE_MULT;

        item.props["rod_enchantment"] = (short)_roll_rod_enchant(item_level);
    }

    item.plus = item.plus2;
}

static bool _weapon_is_visibly_special(const item_def &item)
{
    const int brand = get_weapon_brand(item);
    const bool visibly_branded = (brand != SPWPN_NORMAL);

    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return (false);

    if (visibly_branded || is_artefact(item))
        return (true);

    if (item.is_mundane())
        return (false);

    if ((item.plus || item.plus2)
        && (one_chance_in(3) || get_equip_race(item) && one_chance_in(7)))
    {
        return (true);
    }

    return (false);
}

static bool _armour_is_visibly_special(const item_def &item)
{
    const int brand = get_armour_ego_type(item);
    const bool visibly_branded = (brand != SPARM_NORMAL);

    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return (false);

    if (visibly_branded || is_artefact(item))
        return (true);

    if (item.is_mundane())
        return (false);

    if (item.plus && !one_chance_in(3))
        return (true);

    return (false);
}

static bool _missile_is_visibly_special(const item_def &item)
{
    const special_missile_type brand = get_ammo_brand(item);

    // Ego missiles are runed if their egos aren't obvious.
    // This applies to all needles.
    if (brand != SPMSL_NORMAL && !missile_brand_obvious(brand))
        return (true);

    // And enchanted do, too. Non-random so we don't
    // get weird stacking behaviour.
    if (item.plus)
        return (true);

    return (false);
}

jewellery_type get_random_amulet_type()
{
    return (jewellery_type)
                (AMU_FIRST_AMULET + random2(NUM_JEWELLERY - AMU_FIRST_AMULET));
}

static jewellery_type _get_raw_random_ring_type()
{
    return (jewellery_type) (RING_REGENERATION + random2(NUM_RINGS));
}

jewellery_type get_random_ring_type()
{
    const jewellery_type j = _get_raw_random_ring_type();
    // Adjusted distribution here. - bwr
    if ((j == RING_INVISIBILITY
           || j == RING_REGENERATION
           || j == RING_TELEPORT_CONTROL
           || j == RING_SLAYING)
        && !one_chance_in(3))
    {
        return (_get_raw_random_ring_type());
    }

    return (j);
}

armour_type get_random_body_armour_type(int item_level)
{
    for (int tries = 100; tries > 0; --tries)
    {
        const armour_type tr = get_random_armour_type(item_level);
        if (get_armour_slot(tr) == EQ_BODY_ARMOUR)
            return (tr);
    }
    return (ARM_ROBE);
}

// FIXME: Need to clean up this mess.
armour_type get_random_armour_type(int item_level)
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

        if (one_chance_in(4))
            armtype = ARM_ANIMAL_SKIN;
    }

    if (x_chance_in_y(11 + item_level, 60))
    {
        // Medium-level armours.
        const armour_type medarmours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                           ARM_RING_MAIL, ARM_SCALE_MAIL,
                                           ARM_CHAIN_MAIL, ARM_SPLINT_MAIL,
                                           ARM_BANDED_MAIL, ARM_PLATE_MAIL };

        armtype = RANDOM_ELEMENT(medarmours);
    }

    if (one_chance_in(20) && x_chance_in_y(11 + item_level, 400))
    {
        // High-level armours, including troll and some dragon armours.
        const armour_type hiarmours[] = { ARM_CRYSTAL_PLATE_MAIL,
                                          ARM_TROLL_HIDE,
                                          ARM_TROLL_LEATHER_ARMOUR,
                                          ARM_DRAGON_HIDE, ARM_DRAGON_ARMOUR,
                                          ARM_ICE_DRAGON_HIDE,
                                          ARM_ICE_DRAGON_ARMOUR };

        armtype = RANDOM_ELEMENT(hiarmours);
    }

    if (one_chance_in(20) && x_chance_in_y(11 + item_level, 500))
    {
        // Animal skins and high-level armours, including the rest of
        // the dragon armours.
        const armour_type morehiarmours[] = { ARM_ANIMAL_SKIN,
                                              ARM_STEAM_DRAGON_HIDE,
                                              ARM_STEAM_DRAGON_ARMOUR,
                                              ARM_MOTTLED_DRAGON_HIDE,
                                              ARM_MOTTLED_DRAGON_ARMOUR,
                                              ARM_STORM_DRAGON_HIDE,
                                              ARM_STORM_DRAGON_ARMOUR,
                                              ARM_GOLD_DRAGON_HIDE,
                                              ARM_GOLD_DRAGON_ARMOUR,
                                              ARM_SWAMP_DRAGON_HIDE,
                                              ARM_SWAMP_DRAGON_ARMOUR };

        armtype = RANDOM_ELEMENT(morehiarmours);

        if (armtype == ARM_ANIMAL_SKIN && one_chance_in(20))
            armtype = ARM_CRYSTAL_PLATE_MAIL;
    }

    // Secondary armours.
    if (one_chance_in(5))
    {
        const armour_type secarmours[] = { ARM_SHIELD, ARM_CLOAK, ARM_HELMET,
                                           ARM_GLOVES, ARM_BOOTS };

        armtype = RANDOM_ELEMENT(secarmours);

        if (armtype == ARM_HELMET && one_chance_in(3))
        {
            const armour_type hats[] = { ARM_CAP, ARM_WIZARD_HAT, ARM_HELMET };

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

    case OBJ_MISSILES:
        if (_missile_is_visibly_special(item))
            set_equip_desc(item, ISFLAG_RUNED);
        break;

    default:
        break;
    }
}

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
static int _test_item_level()
{
    switch(random2(10))
    {
    case 0:
        return MAKE_GOOD_ITEM;
    case 1:
        return -4; // damaged
    case 2:
        return -5; // cursed
    case 3:
        return -6; // force randart
    default:
        return random2(50);
    }
}

void makeitem_tests()
{
    int i, level;
    item_def item;

    mpr("Running generate_weapon_item tests.");
    for (i=0;i<10000;i++)
    {
        item.clear();
        level = _test_item_level();
        item.base_type = OBJ_WEAPONS;
        if (coinflip())
            item.special = SPWPN_NORMAL;
        else
            item.special = random2(MAX_PAN_LORD_BRANDS);
        _generate_weapon_item(item,
                              coinflip(),
                              coinflip() ? OBJ_RANDOM : random2(NUM_WEAPONS),
                              level,
                              MAKE_ITEM_RANDOM_RACE);
    }

    mpr("Running generate_armour_item tests.");
    for (i=0;i<10000;i++)
    {
        item.clear();
        level = _test_item_level();
        item.base_type = OBJ_ARMOUR;
        if (coinflip())
            item.special = SPARM_NORMAL;
        else
            item.special = random2(NUM_SPECIAL_ARMOURS);
        _generate_armour_item(item,
                              coinflip(),
                              coinflip() ? OBJ_RANDOM : random2(NUM_ARMOURS),
                              level,
                              MAKE_ITEM_RANDOM_RACE);
    }
}
#endif

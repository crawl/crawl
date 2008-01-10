/*
 * File:       makeitem.cc
 * Summary:    Item creation routines.
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"

#include "enum.h"
#include "externs.h"
#include "makeitem.h"

#include "decks.h"
#include "describe.h"
#include "food.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "spl-book.h"
#include "stuff.h"
#include "view.h"

static void item_set_appearance(item_def &item);

static bool got_curare_roll(const int item_level)
{
    return one_chance_in(item_level > 27? 6   :
                         item_level < 2 ? 15  :
                         (364 - 7 * item_level) / 25);
}

static bool got_distortion_roll(const int item_level)
{
    return (one_chance_in(25));
}

static int exciting_colour()
{
    switch(random2(4))
    {
        case 0: return YELLOW;
        case 1: return LIGHTGREEN;
        case 2: return LIGHTRED;
        case 3: return LIGHTMAGENTA;
        default: return MAGENTA;
    }
}


static int newwave_weapon_colour(const item_def &item)
{
    int item_colour = BLACK;
    // fixed artefacts get predefined colours

    std::string itname = item.name(DESC_PLAIN);
    lowercase(itname);
    
    const bool item_runed = itname.find(" runed ") != std::string::npos;
    const bool heav_runed = itname.find(" heavily ") != std::string::npos;

    if ( is_random_artefact(item) && (!item_runed || heav_runed) )
        return exciting_colour();

    if (is_range_weapon( item ))
    {
        switch (range_skill(item))
        {
        case SK_BOWS:
            item_colour = BLUE;
            break;
        case SK_CROSSBOWS:
            item_colour = LIGHTBLUE;
            break;
        case SK_DARTS:
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

static int classic_weapon_colour(const item_def &item)
{
    int item_colour = BLACK;
    
    if (is_range_weapon( item ))
        item_colour = BROWN;
    else
    {
        switch (item.sub_type)
        {
        case WPN_CLUB:
        case WPN_GIANT_CLUB:
        case WPN_GIANT_SPIKED_CLUB:
        case WPN_ANCUS:
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

static int weapon_colour(const item_def &item)
{
    return (Options.classic_item_colours?
            classic_weapon_colour(item) : newwave_weapon_colour(item));
}

static int newwave_missile_colour(const item_def &item)
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
        item_colour = DARKGRAY;
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

static int classic_missile_colour(const item_def &item)
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
        item_colour = DARKGRAY;
        break;
    default:
        item_colour = LIGHTCYAN;
        if (get_equip_race(item) == ISFLAG_DWARVEN)
            item_colour = CYAN;
        break;
    }
    return (item_colour);
}

static int missile_colour(const item_def &item)
{
    return (Options.classic_item_colours?
            classic_missile_colour(item) : newwave_missile_colour(item));
}

static int newwave_armour_colour(const item_def &item)
{
    int item_colour = BLACK;
    std::string itname = item.name(DESC_PLAIN);
    lowercase(itname);
    
    const bool item_runed = itname.find(" runed ") != std::string::npos;
    const bool heav_runed = itname.find(" heavily ") != std::string::npos;

    if ( is_random_artefact(item) && (!item_runed || heav_runed) )
        return exciting_colour();

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
      case ARM_HELM:
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
        item_colour = CYAN;
        break;
      default:
        item_colour = LIGHTCYAN;
        break;
    }

    return (item_colour);
}

static int classic_armour_colour(const item_def &item)
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
    case ARM_HELM:
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

static int armour_colour(const item_def &item)
{
    return (Options.classic_item_colours?
            classic_armour_colour(item) : newwave_armour_colour(item));
}

void item_colour( item_def &item )
{
    int switchnum = 0;
    int temp_value;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact( item ))
            break;              // unrandarts already coloured

        if (is_fixed_artefact( item ))
        {
            switch (item.special)   // was: - 180, but that is *wrong* {dlb}
            {
            case SPWPN_SINGING_SWORD:
            case SPWPN_SCEPTRE_OF_TORMENT:
                item.colour = YELLOW;
                break;
            case SPWPN_WRATH_OF_TROG:
            case SPWPN_SWORD_OF_POWER:
                item.colour = RED;
                break;
            case SPWPN_SCYTHE_OF_CURSES:
                item.colour = DARKGREY;
                break;
            case SPWPN_MACE_OF_VARIABILITY:
                item.colour = random_colour();
                break;
            case SPWPN_GLAIVE_OF_PRUNE:
                item.colour = MAGENTA;
                break;
            case SPWPN_SWORD_OF_ZONGULDROK:
                item.colour = LIGHTGREY;
                break;
            case SPWPN_KNIFE_OF_ACCURACY:
                item.colour = LIGHTCYAN;
                break;
            case SPWPN_STAFF_OF_OLGREB:
                item.colour = GREEN;
                break;
            case SPWPN_VAMPIRES_TOOTH:
                item.colour = WHITE;
                break;
            case SPWPN_STAFF_OF_WUCAD_MU:
                item.colour = BROWN;
                break;
            }
            break;
        }

        if (is_demonic( item ))
            item.colour = random_uncommon_colour();
        else
            item.colour = weapon_colour(item);

        if (is_random_artefact( item ) && one_chance_in(5)
              && Options.classic_item_colours)
            item.colour = random_colour();

        break;

    case OBJ_MISSILES:
        item.colour = missile_colour(item);
        break;

    case OBJ_ARMOUR:
        if (is_unrandom_artefact( item ))
            break;              /* unrandarts have already been coloured */

        switch (item.sub_type)
        {
        case ARM_DRAGON_HIDE:
        case ARM_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_DRAGON );
            break;
        case ARM_TROLL_HIDE:
        case ARM_TROLL_LEATHER_ARMOUR:
            item.colour = mons_class_colour( MONS_TROLL );
            break;
        case ARM_ICE_DRAGON_HIDE:
        case ARM_ICE_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_ICE_DRAGON );
            break;
        case ARM_STEAM_DRAGON_HIDE:
        case ARM_STEAM_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_STEAM_DRAGON );
            break;
        case ARM_MOTTLED_DRAGON_HIDE:
        case ARM_MOTTLED_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_MOTTLED_DRAGON );
            break;
        case ARM_STORM_DRAGON_HIDE:
        case ARM_STORM_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_STORM_DRAGON );
            break;
        case ARM_GOLD_DRAGON_HIDE:
        case ARM_GOLD_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_GOLDEN_DRAGON );
            break;
        case ARM_SWAMP_DRAGON_HIDE:
        case ARM_SWAMP_DRAGON_ARMOUR:
            item.colour = mons_class_colour( MONS_SWAMP_DRAGON );
            break;
        default:
            item.colour = armour_colour(item);
            break;
        }

        // I don't think this is ever done -- see start of case {dlb}:
        if (is_random_artefact( item ) && one_chance_in(5))
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
        case 11:                //"plastic wand"
            item.colour = random_colour();
            break;
        }

        if (item.special / 12 == 9)
            item.colour = DARKGREY;

        // rare wands (eg disintegration - these will be very rare):
        // maybe only 1 thing, like: crystal, shining, etc.
        break;

    case OBJ_POTIONS:
        item.special = you.item_description[IDESC_POTIONS][item.sub_type];

        switch (item.special % 14)
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
            // set the appropriate colour of the meat: 
            temp_value = mons_class_colour( item.plus );
            item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
            break;
        default:
            item.colour = BROWN;
        }
        break;

    case OBJ_JEWELLERY:
        /* unrandarts have already been coloured */
        if (is_unrandom_artefact( item ))
            break;
        else if (is_random_artefact( item ))
        {
            item.colour = random_colour();
            break;
        }

        item.colour = YELLOW;
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

        if (item.sub_type >= AMU_RAGE)
        {
            switch (switchnum)
            {
            case 0:             //"zirconium amulet"
            case 9:             //"ivory amulet"
            case 11:            //"platinum amulet"
                item.colour = WHITE;
                break;
            case 1:             //"sapphire amulet"
                item.colour = LIGHTBLUE;
                break;
            case 2:             //"golden amulet"
            case 6:             //"brass amulet"
                item.colour = YELLOW;
                break;
            case 3:             //"emerald amulet"
                item.colour = GREEN;
                break;
            case 4:             //"garnet amulet"
            case 8:             //"ruby amulet"
                item.colour = RED;
                break;
            case 5:             //"bronze amulet"
            case 7:             //"copper amulet"
                item.colour = BROWN;
                break;
            case 10:            //"bone amulet"
                item.colour = LIGHTGREY;
                break;
            case 12:            //"jade amulet"
                item.colour = GREEN;
                break;
            case 13:            //"plastic amulet"
                item.colour = random_colour();
            }
        }

        // blackened - same for both rings and amulets
        if (item.special / 13 == 5)
            item.colour = DARKGREY;
        break;

    case OBJ_SCROLLS:
        item.colour = LIGHTGREY;
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
        item.colour = BROWN;
        break;

    case OBJ_ORBS:
        item.colour = EC_MUTAGENIC;
        break;

    case OBJ_MISCELLANY:
        if ( is_deck(item) )
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
                item.colour = EC_IRON;
                break;

            case RUNE_COCYTUS:                  // icy
                item.colour = EC_ICE;
                break;

            case RUNE_TARTARUS:                 // bone
                item.colour = EC_BONE;
                break;

            case RUNE_SLIME_PITS:               // slimy
                item.colour = EC_SLIME;
                break;

            case RUNE_SNAKE_PIT:                // serpentine
                item.colour = EC_POISON;
                break;
                
            case RUNE_ELVEN_HALLS:              // elven
                item.colour = EC_ELVEN;
                break;

            case RUNE_VAULTS:                   // silver
                item.colour = EC_SILVER;
                break;

            case RUNE_TOMB:                     // golden
                item.colour = EC_GOLD;
                break;

            case RUNE_SWAMP:                    // decaying
                item.colour = EC_DECAY;
                break;
                
            case RUNE_SHOALS:                   // barnacled
                item.colour = EC_WATER;
                break;

            // This one is hardly unique, but colour isn't used for 
            // stacking, so we don't have to worry too much about this. -- bwr
            case RUNE_DEMONIC:             // random pandemonium demonlords
            {
                element_type types[] =
                    {EC_EARTH, EC_ELECTRICITY, EC_ENCHANT, EC_HEAL,
                     EC_BLOOD, EC_DEATH, EC_UNHOLY, EC_VEHUMET, EC_BEOGH,
                     EC_CRYSTAL, EC_SMOKE, EC_DWARVEN, EC_ORCISH, EC_GILA};
                
                item.colour = RANDOM_ELEMENT(types);
                break;
            }

            case RUNE_ABYSSAL:             // random in abyss
                item.colour = EC_RANDOM;
                break;

            case RUNE_MNOLEG:                   // glowing
                item.colour = EC_MUTAGENIC;
                break;

            case RUNE_LOM_LOBON:                // magical
                item.colour = EC_MAGIC;
                break;

            case RUNE_CEREBOV:                  // fiery
                item.colour = EC_FIRE;
                break;

            case RUNE_GEHENNA:                  // obsidian
            case RUNE_GLOORX_VLOQ:              // dark
            default:
                item.colour = EC_DARK;
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
        // set the appropriate colour of the body: 
        temp_value = mons_class_colour( item.plus );
        item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
        break;

    case OBJ_GOLD:
        item.colour = YELLOW;
        break;
    default:
        break;
    }
}                               // end item_colour()

static weapon_type determine_weapon_subtype(int item_level)
{
    weapon_type rc = WPN_UNKNOWN;

    const weapon_type common_subtypes[] = {
        WPN_KNIFE, WPN_QUARTERSTAFF, WPN_SLING,
        WPN_SPEAR, WPN_HAND_AXE, WPN_MACE,
        WPN_DAGGER, WPN_DAGGER, WPN_CLUB,
        WPN_HAMMER, WPN_WHIP, WPN_SABRE
    };

    const weapon_type rare_subtypes[] = {
        WPN_LAJATANG, WPN_DEMON_BLADE, WPN_DEMON_TRIDENT,
        WPN_DEMON_WHIP, WPN_DOUBLE_SWORD, WPN_EVENINGSTAR,
        WPN_EXECUTIONERS_AXE, WPN_KATANA, WPN_QUICK_BLADE,
        WPN_TRIPLE_SWORD
    };

    if (random2(20) < 20 - item_level)
        rc = RANDOM_ELEMENT(common_subtypes);
    else if (item_level > 6 && random2(100) < (10 + item_level)
             && one_chance_in(30))
        rc = RANDOM_ELEMENT(rare_subtypes);
    else
    {
        // pick a weapon based on rarity 
        while (true)
        {
            const int wpntype = random2(NUM_WEAPONS);

            if (weapon_rarity(wpntype) > random2(10))
            {
                rc = static_cast<weapon_type>(wpntype);
                break;
            }
        }
    }
    return rc;
}

// Return whether we made an artefact.
static bool try_make_weapon_artefact(item_def& item, int force_type,
                                     int item_level)
{
    if (item.sub_type != WPN_CLUB && item_level > 2
        && random2(4000) <= 100 + (item_level * 3))
    {
        // Make a randart or unrandart.

        // 1 in 50 randarts are unrandarts.
        if (you.level_type != LEVEL_ABYSS
            && you.level_type != LEVEL_PANDEMONIUM
            && one_chance_in(50))
        {
            const int idx = find_okay_unrandart(OBJ_WEAPONS, force_type);

            if (idx != -1)
            {
                make_item_unrandart( item, idx );
                return true;
            }
        }

        // The other 98% are normal randarts.
        make_item_randart( item );
        item.plus = random2(7);
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
            do_curse_item( item );
            item.plus  = -random2(6);
            item.plus2 = -random2(6);
        }
        else if ((item.plus < 0 || item.plus2 < 0)
                 && !one_chance_in(3))
        {
            do_curse_item( item );
        }
        return true;
    }

    // If it isn't an artefact yet, try to make a fixed artefact.
    if (item_level > 6
        && random2(3000) <= 30 + (item_level * 3)
        && one_chance_in(12))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Making fixed artefact.");
#endif
        if (make_item_fixed_artefact( item, (item_level == 51) ))
            return true;
    }

    return false;
}

static item_status_flag_type determine_weapon_race(const item_def& item,
                                                   int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;
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

        case WPN_ANCUS:
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
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

        case WPN_WAR_AXE:
        case WPN_HAND_AXE:
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

        case WPN_HAND_CROSSBOW:
            if (one_chance_in(3))
                rc = ISFLAG_ELVEN;
            break;

        case WPN_BLOWGUN:
            if (one_chance_in(10))
                rc = ISFLAG_ELVEN;
            if (one_chance_in(4))
                rc = ISFLAG_ORCISH;
            break;
        }
        break;
    }
    return rc;
}

static void weapon_add_racial_modifiers(item_def& item)
{
    switch (get_equip_race( item ))
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

static brand_type determine_weapon_brand(const item_def& item, int item_level)
{
    // Forced ego.
    if (item.special != 0)
        return static_cast<brand_type>(item.special);

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
    const int tries = force_good ? 5 : 1;
    brand_type rc = SPWPN_NORMAL;
    
    for (int count = 0; count < tries && rc == SPWPN_NORMAL; ++count)
    {
        if (!(force_good || is_demonic(item) || random2(300) <= 100+item_level))
            continue;

        // We are not guaranteed to have a special set by the end of this
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

            if (got_distortion_roll(item_level))
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
            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (one_chance_in(8))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(8))
                rc = SPWPN_PROTECTION;

            if (one_chance_in(10))
                rc = SPWPN_ORC_SLAYING;

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

            if (one_chance_in(7))
                rc = SPWPN_SPEED;
            // **** intentional fall through here ****
        case WPN_GREAT_SWORD:
        case WPN_DOUBLE_SWORD:
        case WPN_TRIPLE_SWORD:
            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(7))
                rc = SPWPN_PROTECTION;

            if (one_chance_in(8))
                rc = SPWPN_ORC_SLAYING;

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

            if (item.sub_type == WPN_HAND_AXE &&
                one_chance_in(10))
                rc = SPWPN_RETURNING;

            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(3) && (rc == SPWPN_NORMAL || one_chance_in(5)))
            {
                rc = SPWPN_VORPAL;
            }

            if (one_chance_in(6))
                rc = SPWPN_ORC_SLAYING;

            if (one_chance_in(4))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(8))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(12))
                rc = SPWPN_VENOM;

            break;

        case WPN_WHIP:
            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(6))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(6))
                rc = SPWPN_VENOM;

            if (coinflip())
                rc = SPWPN_REACHING;

            if (one_chance_in(5))
                rc = SPWPN_SPEED;

            if (one_chance_in(5))
                rc = SPWPN_ELECTROCUTION;
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
            if (one_chance_in(5))
                rc = SPWPN_SPEED;
            // **** intentional fall through here ****
        case WPN_SPEAR:
            if (one_chance_in(25))
                rc = SPWPN_PAIN;

            if (one_chance_in(10))
                rc = SPWPN_VAMPIRICISM;

            if (item.sub_type == WPN_SPEAR && one_chance_in(6))
                rc = SPWPN_RETURNING;

            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5) && (rc == SPWPN_NORMAL || one_chance_in(6)))
                rc = SPWPN_VORPAL;

            if (one_chance_in(6))
                rc = SPWPN_ORC_SLAYING;

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
        case WPN_HAND_CROSSBOW:
            if (coinflip())
                break;
            // **** possible intentional fall through here ****
        case WPN_BOW:
        case WPN_LONGBOW:
        case WPN_CROSSBOW:
        {
            const int tmp = random2(1000);
            if ( tmp < 375 )
                rc = SPWPN_FLAME;
            else if ( tmp < 750 )
                rc = SPWPN_FROST;
            else if ( tmp < 920 )
                rc = SPWPN_PROTECTION;
            else if ( tmp < 980 )
                rc = SPWPN_VORPAL;
            else
                rc = SPWPN_SPEED;
            break;
        }

        // quarterstaff - not powerful, as this would make
        // the 'staves' skill just too good
        case WPN_QUARTERSTAFF:
            if (one_chance_in(30))
                rc = SPWPN_PAIN;

            if (got_distortion_roll(item_level))
                rc = SPWPN_DISTORTION;

            if (one_chance_in(5))
                rc = SPWPN_SPEED;

            if (one_chance_in(10))
                rc = SPWPN_VORPAL;

            if (one_chance_in(5))
                rc = SPWPN_PROTECTION;
            break;


        case WPN_DEMON_TRIDENT:
        case WPN_DEMON_WHIP:
        case WPN_DEMON_BLADE:
            if (one_chance_in(10))
                rc = SPWPN_PAIN;

            if (one_chance_in(3)
                && (item.sub_type == WPN_DEMON_WHIP
                    || item.sub_type == WPN_DEMON_TRIDENT))
            {
                rc = SPWPN_REACHING;
            }

            if (one_chance_in(5))
                rc = SPWPN_DRAINING;

            if (one_chance_in(5))
                rc = coinflip() ? SPWPN_FLAMING : SPWPN_FREEZING;

            if (one_chance_in(5))
                rc = SPWPN_ELECTROCUTION;

            if (one_chance_in(5))
                rc = SPWPN_VAMPIRICISM;

            if (one_chance_in(5))
                rc = SPWPN_VENOM;
            break;

        case WPN_BLESSED_BLADE:     // special gift of TSO
            rc = SPWPN_HOLY_WRATH;
            break;

        default:
            // unlisted weapons have no associated, standard ego-types {dlb}
            break;
        }
    }
    return rc;
}

static void generate_weapon_item(item_def& item, bool allow_uniques,
                                 int force_type, int item_level,
                                 int item_race)
{
    // Determine weapon type.
    if ( force_type != OBJ_RANDOM )
        item.sub_type = force_type;
    else
        item.sub_type = determine_weapon_subtype(item_level);

    // If we make the unique roll, no further generation necessary.
    if (allow_uniques && try_make_weapon_artefact(item, force_type, item_level))
        return;

    ASSERT(!is_fixed_artefact(item) && !is_random_artefact(item));

    // Artefacts handled, let's make a normal item.
    const bool force_good = (item_level == MAKE_GOOD_ITEM);
    const bool forced_ego = item.special > 0;
    const bool no_brand   = item.special == SPWPN_FORBID_BRAND;

    if (no_brand)
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );

    // If it's forced to be a good item, upgrade the worst weapons.
    if (force_good
        && force_type == OBJ_RANDOM
        && (item.sub_type == WPN_CLUB || item.sub_type == WPN_SLING))
    {
        item.sub_type = WPN_LONG_SWORD;
    }

    item.plus = 0;
    item.plus2 = 0;

    set_equip_race(item, determine_weapon_race(item, item_race));

    // if we allow acquirement-type items to be orcish, then
    // there's a good chance that we'll just strip them of
    // their ego type at the bottom of this function. -- bwr
    if (force_good && !forced_ego && get_equip_race( item ) == ISFLAG_ORCISH)
        set_equip_race( item, ISFLAG_NO_RACE );

    weapon_add_racial_modifiers(item);

    if ((force_good || is_demonic(item) || forced_ego
         || random2(200) <= 50 + item_level)
        // nobody would bother enchanting a mundane club
        && item.sub_type != WPN_CLUB
        && item.sub_type != WPN_GIANT_CLUB
        && item.sub_type != WPN_GIANT_SPIKED_CLUB)
    {
        // Make a better item (possibly ego)
        if (!no_brand)
            set_item_ego_type(item, OBJ_WEAPONS,
                              determine_weapon_brand(item, item_level));

        // if acquired item still not ego... enchant it up a bit.
        if (force_good && item.special == SPWPN_NORMAL)
        {
            item.plus  += 2 + random2(3);
            item.plus2 += 2 + random2(3);
        }

        const int chance = force_good ? 200 : item_level; 

        // odd-looking, but this is how the algorithm compacts {dlb}:
        for (int i = 0; i < 4; ++i)
        {
            item.plus += random2(3);

            if (random2(350) > 20 + chance)
                break;
        }

        // odd-looking, but this is how the algorithm compacts {dlb}:
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
            // make a cursed item
            do_curse_item( item );
            item.plus  -= random2(4);
            item.plus2 -= random2(4);
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );
        }
    }

    if (get_equip_race(item) == ISFLAG_ORCISH
        && !(item_race == MAKE_ITEM_ORCISH && forced_ego))
    {
        // no holy wrath or slay orc and 1/2 the time no-ego
        const int brand = get_weapon_brand( item );
        if (brand == SPWPN_HOLY_WRATH
            || brand == SPWPN_ORC_SLAYING
            || (brand != SPWPN_NORMAL && !forced_ego && coinflip()))
        {
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );
        }
    }
    // All done!
}

static item_status_flag_type determine_missile_race(const item_def& item,
                                                    int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;
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
        // Elves don't make bolts, sling bullets, or throwing nets
        if ((item.sub_type == MI_ARROW
             || item.sub_type == MI_DART
             || item.sub_type == MI_JAVELIN)
            && one_chance_in(4))
        {
            rc = ISFLAG_ELVEN;
        }

        // Orcs don't make sling bullets or throwing nets
        if ((item.sub_type == MI_ARROW
             || item.sub_type == MI_BOLT
             || item.sub_type == MI_DART
             || item.sub_type == MI_JAVELIN)
            && one_chance_in(4))
        {
            rc = ISFLAG_ORCISH;
        }

        // Dwarves don't make arrows, sling bullets, javelins, or
        // throwing nets
        if ((item.sub_type == MI_DART
             || item.sub_type == MI_BOLT)
            && one_chance_in(6))
        {
            rc = ISFLAG_DWARVEN;
        }

        // Dwarves don't make needles
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

static special_missile_type determine_missile_brand(const item_def& item,
                                                    int item_level)
{
    // Forced ego.
    if (item.special != 0)
        return static_cast<special_missile_type>(item.special);

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
    special_missile_type rc = SPMSL_NORMAL;
    // note that needles can only be poisoned
    //
    // Actually, it'd be really nice if there where
    // some paralysis or slowing poison needles, just
    // so that blowguns have some added utility over
    // the other launchers/throwing weapons. -- bwr

    // All needles are either poison or curare.
    if (item.sub_type == MI_NEEDLE)
        rc = got_curare_roll(item_level) ? SPMSL_CURARE : SPMSL_POISONED;
    else
    {
        const int temp_rand =
            (force_good ? random2(150) : random2(2000 - 55 * item_level));

        if (temp_rand < 60)
            rc = SPMSL_FLAME;
        else if (temp_rand < 120)
            rc = SPMSL_ICE;
        else if (temp_rand < 150)
            rc = SPMSL_POISONED;
        else
            rc = SPMSL_NORMAL;
    }

    // Javelins can be returning.
    if (item.sub_type == MI_JAVELIN && one_chance_in(25))
        rc = SPMSL_RETURNING;

    // orcish ammo gets poisoned a lot more often -- in the original
    // code it was poisoned every time!?
    if (get_equip_race(item) == ISFLAG_ORCISH && one_chance_in(3))
        rc = SPMSL_POISONED;

    // Un-poison sling bullets; un-flame and un-ice javelins; unbrand
    // throwing nets.
    if ((item.sub_type == MI_SLING_BULLET && rc == SPMSL_POISONED)
         || (item.sub_type == MI_JAVELIN
             && (rc == SPMSL_FLAME || rc == SPMSL_ICE))
         || item.sub_type == MI_THROWING_NET)
    {
        rc = SPMSL_NORMAL;
    }

    return rc;
}

static void generate_missile_item(item_def& item, int force_type,
                                  int item_level, int item_race)
{
    const bool no_brand = item.special == SPMSL_FORBID_BRAND;
    if (no_brand)
        item.special = SPMSL_NORMAL;

    item.plus = 0;

    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
        item.sub_type =
            random_choose_weighted(30, MI_STONE,
                                   20, MI_DART,
                                   20, MI_ARROW,
                                   10, MI_NEEDLE,
                                   5,  MI_SLING_BULLET,
                                   2,  MI_JAVELIN,
                                   0);

    // no fancy rocks -- break out before we get to racial/special stuff
    if (item.sub_type == MI_LARGE_ROCK)
    {
        item.quantity = 2 + random2avg(5,2);
        return;
    }
    else if (item.sub_type == MI_STONE)
    {
        item.quantity = 1+random2(9) + random2(12) + random2(15) + random2(12);
        return;
    }

    set_equip_race(item, determine_missile_race(item, item_race));
    if (!no_brand)
        set_item_ego_type( item, OBJ_MISSILES,
                           determine_missile_brand(item, item_level) );

    // reduced quantity if special
    if (item.sub_type == MI_JAVELIN ||
        get_ammo_brand( item ) == SPMSL_CURARE ||
        get_ammo_brand( item ) == SPMSL_RETURNING)
    {
        item.quantity = random_range(2, 8);
    }
    else if (get_ammo_brand( item ) != SPMSL_NORMAL)
        item.quantity = 1+random2(9) + random2(12) + random2(12);
    else
        item.quantity = 1+random2(9) + random2(12) + random2(12) + random2(15);

    if (10 + item_level >= random2(100))
        item.plus += random2(5);

    // elven arrows and dwarven bolts are quality items
    if ((get_equip_race(item) == ISFLAG_ELVEN && item.sub_type == MI_ARROW) ||
        (get_equip_race(item) == ISFLAG_DWARVEN && item.sub_type == MI_BOLT))
    {
        item.plus += random2(3);
    }
}

static bool try_make_armour_artefact(item_def& item, int force_type,
                                     int item_level)
{
    if ( item_level > 2 && random2(4000) <= (100 + item_level * 3))
    {
        // Make a randart or unrandart.

        // 1 in 50 randarts are unrandarts.
        if ((you.level_type != LEVEL_ABYSS
             && you.level_type != LEVEL_PANDEMONIUM)
            && one_chance_in(50))
        {
            // The old generation code did not respect force_type here.
            const int idx = find_okay_unrandart(OBJ_ARMOUR, force_type);
            if (idx != -1)
            {
                make_item_unrandart( item, idx );
                return true;
            }
        }

        // The other 98% are normal randarts.

        // No randart hides.
        hide2armour(item);
        make_item_randart( item );

        // 10% of boots become barding.
        if (item.sub_type == ARM_BOOTS && one_chance_in(10))
            item.sub_type = coinflip() ? ARM_NAGA_BARDING : ARM_CENTAUR_BARDING;

        // Determine enchantment and cursedness.
        if (one_chance_in(5))
        {
            do_curse_item( item );
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
                do_curse_item( item );
        }
        return true;
    }
    return false;
}

static item_status_flag_type determine_armour_race(const item_def& item,
                                                   int item_race)
{
    item_status_flag_type rc = ISFLAG_NO_RACE;
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
        if ( coinflip() )
            break;

        switch (item.sub_type)
        {
        case ARM_SHIELD:
        case ARM_BUCKLER:
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
        case ARM_WIZARD_HAT:
            if (one_chance_in(6))
                rc = ISFLAG_ELVEN;
            break;

        case ARM_HELM:
        case ARM_HELMET:
            if (one_chance_in(8))
                rc = ISFLAG_ORCISH;
            if (one_chance_in(6))
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

        default:    // skins, hides, crystal plate are always plain
            break;
        }
    }
    return rc;
}

static special_armour_type determine_armour_ego(const item_def& item,
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
                                   480, SPARM_PROTECTION,
                                   0));
        break;

    case ARM_CLOAK:
    {
        if (get_equip_race(item) == ISFLAG_DWARVEN)
            break;
        const special_armour_type cloak_egos[] = {
            SPARM_POISON_RESISTANCE, SPARM_DARKNESS,
            SPARM_MAGIC_RESISTANCE, SPARM_PRESERVATION
        };
        rc = RANDOM_ELEMENT(cloak_egos);
        break;
    }

    case ARM_WIZARD_HAT:
        if ( coinflip() )
            rc = (one_chance_in(3) ?
                  SPARM_MAGIC_RESISTANCE : SPARM_INTELLIGENCE);
        break;

    case ARM_HELMET:
    case ARM_HELM:
    case ARM_CAP:
        rc = coinflip() ? SPARM_SEE_INVISIBLE : SPARM_INTELLIGENCE;
        break;

    case ARM_GLOVES:
        rc = coinflip() ? SPARM_DEXTERITY : SPARM_STRENGTH;
        break;

    case ARM_BOOTS:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    {
        const int tmp = random2(600) + 200 * (item.sub_type != ARM_BOOTS);

        rc = (tmp < 200) ? SPARM_RUNNING :
            (tmp < 400) ? SPARM_LEVITATION :
            (tmp < 600) ? SPARM_STEALTH :
            (tmp < 700) ? SPARM_COLD_RESISTANCE : SPARM_FIRE_RESISTANCE;
        break;
    }

    case ARM_ROBE:
        switch (random2(4))
        {
        case 0:
            rc = coinflip() ? SPARM_COLD_RESISTANCE : SPARM_FIRE_RESISTANCE;
            break;

        case 1:
            rc = SPARM_MAGIC_RESISTANCE;
            break;

        case 2:
            rc = coinflip() ? SPARM_POSITIVE_ENERGY : SPARM_RESISTANCE;
            break;
        case 3:
            // This is an odd limitation, but I'm not changing it yet.
            if (force_type == OBJ_RANDOM && random2(50) <= 10 + item_level)
                rc = SPARM_ARCHMAGI;
            break;
        }
        break;

    default:    // other body armours:
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
    return rc;
}


static void generate_armour_item(item_def& item, bool allow_uniques,
                                 int force_type, int item_level, int item_race)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
        item.sub_type = get_random_armour_type(item_level);

    if (allow_uniques && try_make_armour_artefact(item, force_type, item_level))
        return;

    if ( is_helmet(item) && one_chance_in(3) )
        set_helmet_random_desc(item);

    if (item_race == MAKE_ITEM_RANDOM_RACE && item.sub_type == ARM_BOOTS)
    {
        if (one_chance_in(8))
            item.sub_type = ARM_NAGA_BARDING;
        else if (one_chance_in(7))
            item.sub_type = ARM_CENTAUR_BARDING;
    }
    else
        set_equip_race(item, determine_armour_race(item, item_race));

    // Dwarven armour is high-quality.
    if ( get_equip_race(item) == ISFLAG_DWARVEN && coinflip() )
        item.plus++;

    const bool force_good = (item_level == MAKE_GOOD_ITEM);
    const bool forced_ego = item.special > 0;
    const bool no_ego     = item.special == SPARM_FORBID_EGO;

    if (no_ego)
        item.special = SPARM_NORMAL;

    if (force_good || forced_ego || item.sub_type == ARM_WIZARD_HAT
        || 50 + item_level >= random2(250))
    {
        // Make a good item...
        item.plus += random2(3);

        if (item.sub_type <= ARM_PLATE_MAIL && 20 + item_level >= random2(300))
            item.plus += random2(3);

        if (!no_ego && (30 + item_level >= random2(350))
            && (force_good || forced_ego
                || (!get_equip_race(item) == ISFLAG_ORCISH
                    || (item.sub_type <= ARM_PLATE_MAIL && coinflip()))))
        {
            // ...an ego item, in fact.
            set_item_ego_type(item, OBJ_ARMOUR,
                              determine_armour_ego(item, force_type,
                                                   item_level));

            if ( get_armour_ego_type(item) == SPARM_PONDEROUSNESS )
                item.plus += 3 + random2(4);
        }
    }
    else if (one_chance_in(12))
    {
        // Make a bad (cursed) item
        do_curse_item( item );

        if (one_chance_in(5))
            item.plus -= random2(3);

        set_item_ego_type( item, OBJ_ARMOUR, SPARM_NORMAL );
    }

    // Make sure you don't get a hide from acquirement (since that
    // would be an enchanted item which somehow didn't get converted
    // into armour).
    if (force_good)
        hide2armour(item); // what of animal hides? {dlb}

    // skin armours + Crystal PM don't get special enchantments
    // or species, but can be randarts
    if (item.sub_type >= ARM_DRAGON_HIDE
        && item.sub_type <= ARM_SWAMP_DRAGON_ARMOUR)
    {
        if (!forced_ego)
            set_item_ego_type( item, OBJ_ARMOUR, SPARM_NORMAL );
    }
}

static monster_type choose_random_monster_corpse()
{
    for (int count = 0; count < 1000; ++count)
    {
        monster_type spc = mons_species(random2(NUM_MONSTERS));
        if (mons_weight(spc) > 0)        // drops a corpse
            return spc;
    }
    return MONS_RAT;            // if you can't find anything else...
}

static int random_wand_subtype()
{
    int rc = random2( NUM_WANDS );

    // Adjusted distribution here -- bwr
    // Wands used to be uniform (5.26% each)
    //
    // Now:
    // invis, hasting, healing  (1.11% each)
    // fireball, teleportaion   (3.74% each)
    // others                   (6.37% each)
    if ((rc == WAND_INVISIBILITY || rc == WAND_HASTING || rc == WAND_HEALING)
        || ((rc == WAND_FIREBALL || rc == WAND_TELEPORTATION) && coinflip()))
    {
        rc = random2( NUM_WANDS );
    }
    return rc;
}

static int wand_max_charges(int subtype)
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

static void generate_wand_item(item_def& item, int force_type)
{
    // determine sub_type
    if (force_type != OBJ_RANDOM) 
        item.sub_type = force_type;
    else
        item.sub_type = random_wand_subtype();

    // generate charges randomly...
    item.plus = random2avg(wand_max_charges(item.sub_type), 3);

    // ...but 0 charges is silly
    if ( item.plus == 0 )
        item.plus++;

    // plus2 tracks how many times the player has zapped it.
    // If it is -1, then the player knows it's empty.
    // If it is -2, then the player has messed with it somehow
    // (presumably by recharging), so don't bother to display the
    // count.
    item.plus2 = 0;
}

static void generate_food_item(item_def& item, int force_quant, int force_type)
{
    // determine sub_type:
    if (force_type == OBJ_RANDOM)
    {
        item.sub_type = random_choose_weighted( 250, FOOD_MEAT_RATION,
                                                300, FOOD_BREAD_RATION,
                                                100, FOOD_PEAR,
                                                100, FOOD_APPLE,
                                                100, FOOD_CHOKO,
                                                10,  FOOD_CHEESE,
                                                10,  FOOD_PIZZA,
                                                10,  FOOD_SNOZZCUMBER,
                                                10,  FOOD_APRICOT,
                                                10,  FOOD_ORANGE,
                                                10,  FOOD_BANANA,
                                                10,  FOOD_STRAWBERRY,
                                                10,  FOOD_RAMBUTAN,
                                                10,  FOOD_LEMON,
                                                10,  FOOD_GRAPE,
                                                10,  FOOD_SULTANA,
                                                10,  FOOD_LYCHEE,
                                                10,  FOOD_BEEF_JERKY,
                                                10,  FOOD_SAUSAGE,
                                                5,   FOOD_HONEYCOMB,
                                                5,   FOOD_ROYAL_JELLY,
                                                0);
    }
    else
        item.sub_type = force_type;

    // Happens with ghoul food acquirement -- use place_chunks() outherwise
    if (item.sub_type == FOOD_CHUNK)
    {
        // set chunk flavour (default to common dungeon rat steaks):
        item.plus = choose_random_monster_corpse();
        // set duration
        item.special = (10 + random2(11)) * 10;
    }

    // determine quantity
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

static void generate_potion_item(item_def& item, int force_type,
                                 int item_level)
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
        do {
            stype = random_choose_weighted( 1407, POT_HEAL_WOUNDS,
                                            2815, POT_HEALING,
                                            222, POT_CURE_MUTATION,
                                            612, POT_SPEED,
                                            612, POT_MIGHT,
                                            136, POT_BERSERK_RAGE,
                                            340, POT_INVISIBILITY,
                                            340, POT_LEVITATION,
                                            250, POT_RESISTANCE,
                                            70, POT_PORRIDGE,
                                            111, POT_BLOOD,
                                            38, POT_GAIN_STRENGTH,
                                            38, POT_GAIN_DEXTERITY,
                                            38, POT_GAIN_INTELLIGENCE,
                                            13, POT_EXPERIENCE,
                                            14, POT_MAGIC,
                                            972, POT_RESTORE_ABILITIES,
                                            648, POT_POISON,
                                            162, POT_STRONG_POISON,
                                            324, POT_MUTATION,
                                            324, POT_SLOWING,
                                            324, POT_PARALYSIS,
                                            324, POT_CONFUSION,
                                            278, POT_DEGENERATION,
                                            10, POT_DECAY,
                                            0);
        } while ( (stype == POT_POISON && item_level < 1) ||
                  (stype == POT_STRONG_POISON && item_level < 11) ||
                  (stype == POT_SLOWING && item_level < 7) );
        if ( stype == POT_GAIN_STRENGTH || stype == POT_GAIN_DEXTERITY ||
             stype == POT_GAIN_INTELLIGENCE || stype == POT_EXPERIENCE ||
             stype == POT_MAGIC || stype == POT_RESTORE_ABILITIES )
            item.quantity = 1;
        item.sub_type = stype;
    }

    item.plus = 0;
}

static void generate_scroll_item(item_def& item, int force_type,
                                 int item_level)
{
    // determine sub_type:
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        // only used in certain cases {dlb}
        const int depth_mod = random2(1 + item_level);
        const int temp_rand = random2(920);
        
        item.sub_type =
            ((temp_rand > 751) ? SCR_IDENTIFY :          // 18.26%
             (temp_rand > 629) ? SCR_REMOVE_CURSE :      // 13.26%
             (temp_rand > 554) ? SCR_TELEPORTATION :     //  8.15%
             (temp_rand > 494) ? SCR_DETECT_CURSE :      //  6.52%
             (temp_rand > 464) ? SCR_FEAR :              //  3.26%
             (temp_rand > 434) ? SCR_NOISE :             //  3.26%
             (temp_rand > 404) ? SCR_MAGIC_MAPPING :     //  3.26%
             (temp_rand > 374) ? SCR_FOG :               //  3.26%
             (temp_rand > 344) ? SCR_RANDOM_USELESSNESS ://  3.26%
             (temp_rand > 314) ? SCR_CURSE_WEAPON :      //  3.26%
             (temp_rand > 284) ? SCR_CURSE_ARMOUR :      //  3.26%
             (temp_rand > 254) ? SCR_RECHARGING :        //  3.26%
             (temp_rand > 224) ? SCR_BLINKING :          //  3.26%
             (temp_rand > 194) ? SCR_PAPER :             //  3.26%
             (temp_rand > 164) ? SCR_ENCHANT_ARMOUR :    //  3.26%
             (temp_rand > 134) ? SCR_ENCHANT_WEAPON_I :  //  3.26%
             (temp_rand > 104) ? SCR_ENCHANT_WEAPON_II : //  3.26%

             // Crawl is kind to newbie adventurers {dlb}:
             // yes -- these five are messy {dlb}:
             // yes they are a hellish mess of tri-ops and long lines,
             // this formating is somewhat better -- bwr
             (temp_rand > 74) ?
             ((item_level < 4) ? SCR_TELEPORTATION
              : SCR_IMMOLATION) :    //  3.26%
             (temp_rand > 59) ?
             ((depth_mod < 4) ? SCR_TELEPORTATION
              : SCR_ACQUIREMENT) :   //  1.63%
             (temp_rand > 44) ?
             ((depth_mod < 4) ? SCR_DETECT_CURSE
              : SCR_SUMMONING) :     //  1.63%
             (temp_rand > 29) ?
             ((depth_mod < 4) ? SCR_TELEPORTATION    //  1.63%
              : SCR_ENCHANT_WEAPON_III) :
             (temp_rand > 14) ?
             ((depth_mod < 7) ? SCR_DETECT_CURSE
              : SCR_TORMENT)         //  1.63%
             // default:
             : ((depth_mod < 7) ? SCR_TELEPORTATION    //  1.63%
                : SCR_VORPALISE_WEAPON));
    }

    // determine quantity
    if ( item.sub_type == SCR_VORPALISE_WEAPON ||
         item.sub_type == SCR_ENCHANT_WEAPON_III ||
         item.sub_type == SCR_ACQUIREMENT ||
         item.sub_type == SCR_TORMENT )
        item.quantity = 1;
    else
    {
        const int tmp = random2(48);
        if ( tmp == 1 )
            item.quantity = 2;
        else if ( tmp == 0 )
            item.quantity = 3;
        else
            item.quantity = 1;
    }

    item.plus = 0;
}

static void generate_book_item(item_def& item, int force_type,
                               int item_level)
{
    // determine special (description)
    item.special = random2(5);

    if (one_chance_in(10))
        item.special += random2(8) * 10;

    if ( force_type != OBJ_RANDOM )
        item.sub_type = force_type;
    else
    {
        do
        {
            item.sub_type = random2(NUM_BOOKS);
            
            if (item.sub_type != BOOK_DESTRUCTION &&
                item.sub_type != BOOK_MANUAL &&
                book_rarity(item.sub_type) != 100 &&
                one_chance_in(10))
            {
                item.sub_type = coinflip() ? BOOK_WIZARDRY : BOOK_POWER;
            }
            
            if (!one_chance_in(100) &&
                random2(item_level+1) + 1 < book_rarity(item.sub_type))
            {
                item.sub_type = BOOK_DESTRUCTION; // continue trying
            }
        }
        while (item.sub_type == BOOK_DESTRUCTION ||
               item.sub_type == BOOK_MANUAL ||
               book_rarity(item.sub_type) == 100);

        // tome of destruction: rare!
        if ( item_level > 10 && random2(7000) <= item_level + 20 )
            item.sub_type = BOOK_DESTRUCTION;

        // skill manuals - also rare
        if ( item_level >  6 && random2(4000) <= item_level + 20 )
            item.sub_type = BOOK_MANUAL;
    }

    // Determine which skill for a manual
    if ( item.sub_type == BOOK_MANUAL )
    {
        if (one_chance_in(4))
            item.plus = SK_SPELLCASTING + random2(NUM_SKILLS - SK_SPELLCASTING);
        else
        {
            item.plus = random2(SK_UNARMED_COMBAT);

            if (item.plus == SK_UNUSED_1)
                item.plus = SK_UNARMED_COMBAT;
        }
    }
}

static void generate_staff_item(item_def& item, int force_type)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        // assumption: STAFF_SMITING is the first rod
        item.sub_type = random2(STAFF_SMITING);
        
        // rods are rare (10% of all staves)
        if (one_chance_in(10))
            item.sub_type = random_rod_subtype();
        
        // staves of energy/channeling are 25% less common, wizardry/power
        // are more common
        if ((item.sub_type == STAFF_ENERGY 
             || item.sub_type == STAFF_CHANNELING) && one_chance_in(4))
        {
            item.sub_type = coinflip() ? STAFF_WIZARDRY : STAFF_POWER; 
        }
    }
    
    if (item_is_rod( item ))
        init_rod_mp( item );
    
    // add different looks
    item.special = random2(10);
}

static bool try_make_jewellery_unrandart(item_def& item, int force_type,
                                         int item_level)
{
    bool rc = false;

    if (item_level > 2 
        && you.level_type != LEVEL_ABYSS
        && you.level_type != LEVEL_PANDEMONIUM
        && random2(2000) <= 100 + (item_level * 3) 
        && one_chance_in(20))
    {
        // The old generation code did not respect force_type here.
        const int idx = find_okay_unrandart(OBJ_JEWELLERY, force_type);

        if (idx != -1)
        {
            make_item_unrandart( item, idx );
            rc = true;
        }
    }
    return rc;
}

static int determine_ring_plus(int subtype)
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

static void generate_jewellery_item(item_def& item, bool allow_uniques,
                                    int force_type, int item_level)
{
    if (allow_uniques &&
        try_make_jewellery_unrandart(item, force_type, item_level))
        return;

    // Determine subtype.
    // Note: removed double probability reduction for some subtypes
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
        item.sub_type = (one_chance_in(4) ? get_random_amulet_type() :
                         get_random_ring_type());

    // everything begins as uncursed, unenchanted jewellery {dlb}:
    item.plus = 0;
    item.plus2 = 0;

    item.plus = determine_ring_plus(item.sub_type);
    if ( item.plus < 0 )
        do_curse_item(item);

    if (item.sub_type == RING_SLAYING ) // requires plus2 too
    {
        if (item_cursed( item ) && !one_chance_in(20))
            item.plus2 = -1 - random2avg(6, 2);
        else
        {
            item.plus2 = 1 + (one_chance_in(3) ? random2(3) : random2avg(6, 2));

            if (random2(25) < 9)        // 36% of such rings {dlb}
            {
                // make "ring of damage"
                do_uncurse_item( item );
                item.plus = 0;
                item.plus2 += 2;
            }
        }
    }

    // All jewellery base types should now work. -- bwr
    if (allow_uniques && item_level > 2
        && random2(4000) <= 100 + (item_level * 3))
    {
        make_item_randart( item );
    }
    else if (item.sub_type == RING_HUNGER || item.sub_type == RING_TELEPORTATION
             || one_chance_in(50))
    {
        // rings of hunger and teleportation are always cursed {dlb}:
        do_curse_item( item );
    }
}

static void generate_misc_item(item_def& item, int force_type, int item_race)
{
    if (force_type != OBJ_RANDOM)
        item.sub_type = force_type;
    else
    {
        do
        {
            item.sub_type = random2(NUM_MISCELLANY);
        }
        while
            // never randomly generated
            ((item.sub_type == MISC_RUNE_OF_ZOT)
             || (item.sub_type == MISC_HORN_OF_GERYON)
             // pure decks are rare in the dungeon
             || ((item.sub_type == MISC_DECK_OF_ESCAPE ||
                  item.sub_type == MISC_DECK_OF_DESTRUCTION ||
                  item.sub_type == MISC_DECK_OF_DUNGEONS ||
                  item.sub_type == MISC_DECK_OF_SUMMONING ||
                  item.sub_type == MISC_DECK_OF_WONDERS ||
                  item.sub_type == MISC_DECK_OF_PUNISHMENT) &&
                 !one_chance_in(5)));

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

    if (item.sub_type == MISC_RUNE_OF_ZOT)
        item.plus = item_race;
}

// Returns item slot or NON_ITEM if it fails
int items( int allow_uniques,       // not just true-false,
                                    //     because of BCR acquirement hack
           object_class_type force_class, // desired OBJECTS class {dlb}
           int force_type,          // desired SUBTYPE - enum varies by OBJ
           bool dont_place,         // don't randomly place item on level
           int item_level,          // level of the item, can differ from global
           int item_race,           // weapon / armour racial categories
                                    // item_race also gives type of rune!
           unsigned mapmask,
           int force_ego)           // desired ego/brand
{
    // TODO: Allow a combination of force_ego > 0 and
    // force_type == OBJ_RANDOM, so that (for example) you could have
    // force_class = OBJ_WEAPON, force_type = OBJ_RANDOM and
    // force_ego = SPWPN_VORPAL, and a random weapon of a type
    // appropriate for the vorpal brand will be chosen.
    ASSERT(force_ego <= 0 ||
           ((force_class == OBJ_WEAPONS || force_class == OBJ_ARMOUR
             || force_class == OBJ_MISSILES)
            && force_type != OBJ_RANDOM));

    // find an empty slot for the item (with culling if required)
    int p = get_item_slot(10);
    if (p == NON_ITEM)
        return (NON_ITEM);

    item_def& item(mitm[p]);

    const bool force_good = (item_level == MAKE_GOOD_ITEM);

    if (force_ego > 0)
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
            random_choose_weighted(5, OBJ_STAVES,
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
        if (item_level > 7 && (20 + item_level) >= random2(3500))
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

    // determine sub_type accordingly {dlb}:
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        generate_weapon_item(item, allow_uniques, force_type,
                             item_level, item_race);
        break;

    case OBJ_MISSILES:
        generate_missile_item(item, force_type, item_level, item_race);
        break;

    case OBJ_ARMOUR:
        generate_armour_item(item, allow_uniques, force_type,
                             item_level, item_race);
        break;

    case OBJ_WANDS:
        generate_wand_item(item, force_type);
        break;

    case OBJ_FOOD:
        generate_food_item(item, allow_uniques, force_type);
        break;

    case OBJ_POTIONS:
        generate_potion_item(item, force_type, item_level);
        break;

    case OBJ_SCROLLS:
        generate_scroll_item(item, force_type, item_level);
        break;

    case OBJ_JEWELLERY:
        generate_jewellery_item(item, allow_uniques, force_type,  item_level);
        break;

    case OBJ_BOOKS:
        generate_book_item(item, force_type, item_level);
        break;

    case OBJ_STAVES:
        generate_staff_item(item, force_type);
        break;

    case OBJ_ORBS:              // always forced in current setup {dlb}
        item.sub_type = force_type;
        set_unique_item_status( OBJ_ORBS, item.sub_type, UNIQ_EXISTS );
        break;

    case OBJ_MISCELLANY:
        generate_misc_item(item, force_type, item_race);
        break;

    // that is, everything turns to gold if not enumerated above, so ... {dlb}
    default:
        item.base_type = OBJ_GOLD;
        if (force_good)
            item.quantity = 150+random2(150) + random2(random2(random2(2000)));
        else
            item.quantity = 1 + random2avg(19, 2) + random2(item_level);
        break;
    }

    // Colour the item
    item_colour( item );

    // Set brand appearance.
    item_set_appearance( item );

    if (dont_place)
    {
        item.x = 0;
        item.y = 0;
        item.link = NON_ITEM;
    }
    else
    {
        int x_pos, y_pos;
        int tries = 500;
        do
        {
            if (tries-- <= 0)
            {
                destroy_item(p);
                return (NON_ITEM);
            }
            
            x_pos = random2(GXM);
            y_pos = random2(GYM);
        }
        while (grd[x_pos][y_pos] != DNGN_FLOOR
               || !unforbidden(coord_def(x_pos, y_pos), mapmask));

        move_item_to_grid( &p, x_pos, y_pos );
    }

    // Note that item might be invalidated now, since p could have changed.
    ASSERT( is_valid_item(mitm[p]) );
    return p;
}

void init_rod_mp(item_def &item)
{
    if (!item_is_rod(item))
        return;

    if (item.sub_type == STAFF_STRIKING)
        item.plus2 = random_range(6, 9) * ROD_CHARGE_MULT;
    else
        item.plus2 = random_range(9, 14) * ROD_CHARGE_MULT;
    
    item.plus = item.plus2;
}

static bool weapon_is_visibly_special(const item_def &item)
{
    const int brand = get_weapon_brand(item);
    const bool visibly_branded = brand != SPWPN_NORMAL;

    return (is_random_artefact(item) || visibly_branded
            || ((item.plus || item.plus2)
                && (get_equip_race(item)? one_chance_in(7)
                    : one_chance_in(3))))
        // nobody would bother enchanting a mundane club
        && item.sub_type != WPN_CLUB
        && item.sub_type != WPN_GIANT_CLUB
        && item.sub_type != WPN_GIANT_SPIKED_CLUB
        && get_equip_desc(item) == ISFLAG_NO_DESC;
}

static void give_monster_item(
    monsters *mon, int thing,
    bool force_item = false,
    bool (monsters::*pickupfn)(item_def&, int) = NULL)
{
    item_def &mthing = mitm[thing];

    mthing.x = 0;
    mthing.y = 0;
    mthing.link = NON_ITEM;
    unset_ident_flags(mthing, ISFLAG_IDENT_MASK);

    const mon_holy_type mholy = mons_holiness(mon);
    
    if (get_weapon_brand(mthing) == SPWPN_HOLY_WRATH
        && (mholy == MH_UNDEAD || mholy == MH_DEMONIC))
    {
        set_item_ego_type( mthing, OBJ_WEAPONS, SPWPN_NORMAL );
    }

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!(pickupfn? (mon->*pickupfn)(mthing, false)
          : mon->pickup_item(mthing, false, true)))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Destroying %s because %s doesn't want it!",
             mthing.name(DESC_PLAIN).c_str(), mon->name(DESC_PLAIN).c_str());
#endif
        destroy_item(thing);
        return ;
    }
    
    if (!force_item || mthing.colour == BLACK) 
        item_colour( mthing );
}

static void give_scroll(monsters *mon, int level)
{
    //mv - give scroll
    if (mons_is_unique( mon->type ) && one_chance_in(3))
    {
        const int thing_created =
            items(0, OBJ_SCROLLS, OBJ_RANDOM, true, level, 0);
        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        give_monster_item(mon, thing_created);
    }
}

static void give_wand(monsters *mon, int level)
{
    //mv - give wand
    if (mons_is_unique( mon->type ) && one_chance_in(5))
    {
        const int thing_created =
            items(0, OBJ_WANDS, OBJ_RANDOM, true, level, 0);
        if (thing_created == NON_ITEM)
            return;

        // don't give top-tier wands before 5 HD
        if ( mon->hit_dice < 5 )
        {
            // technically these wands will be undercharged, but it
            // doesn't really matter
            if ( mitm[thing_created].sub_type == WAND_FIRE )
                mitm[thing_created].sub_type = WAND_FLAME;
            if ( mitm[thing_created].sub_type == WAND_COLD )
                mitm[thing_created].sub_type = WAND_FROST;
            if ( mitm[thing_created].sub_type == WAND_LIGHTNING )
                mitm[thing_created].sub_type = (coinflip() ?
                                                WAND_FLAME : WAND_FROST);
        }

        mitm[thing_created].flags = 0;
        give_monster_item(mon, thing_created);
    }
}

static void give_potion(monsters *mon, int level)
{
    //mv - give potion
    if (mons_is_unique( mon->type ) && one_chance_in(3))
    {
        const int thing_created =
            items(0, OBJ_POTIONS, OBJ_RANDOM, true, level, 0);
        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        give_monster_item(mon, thing_created);
    }
}

static item_make_species_type give_weapon(monsters *mon, int level,
                                          bool melee_only = false,
                                          bool give_aux_melee = true)
{
    const int bp = get_item_slot();
    bool force_item = false;
    
    if (bp == NON_ITEM)
        return (MAKE_ITEM_RANDOM_RACE);

    item_def &item = mitm[bp];
    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;

    item.base_type = OBJ_UNASSIGNED;

    if (mon->type == MONS_DANCING_WEAPON
        && player_in_branch( BRANCH_HALL_OF_BLADES ))
    {
        level = MAKE_GOOD_ITEM;
    }

    // moved setting of quantity here to keep it in mind {dlb}
    int iquan = 1;
    // I wonder if this is even used, given calls to item() {dlb}

    switch (mon->type)
    {
    case MONS_KOBOLD:
        // a few of the smarter kobolds have blowguns.
        if (one_chance_in(10) && level > 1)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_BLOWGUN;
            break;
        }
        // intentional fallthrough
    case MONS_BIG_KOBOLD:
        if (random2(5) < 3)     // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;

            const int temp_rand = random2(5);
            item.sub_type = ((temp_rand > 2) ? WPN_DAGGER :     // 40%
                                 (temp_rand > 0) ? WPN_SHORT_SWORD  // 40%
                                                 : WPN_CLUB);       // 20%
        }
        else
            return (item_race);
        break;

    case MONS_HOBGOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (random2(5) < 3)     // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_CLUB;
        }
        else
            return (item_race);
        break;

    case MONS_GOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (!melee_only && one_chance_in(12) && level)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_SLING;
            item_race = MAKE_ITEM_NO_RACE;
            break;
        }
        // deliberate fall through {dlb}
    case MONS_JESSICA:
    case MONS_IJYB:
        if (random2(5) < 3)     // < 1 // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = (coinflip() ? WPN_DAGGER : WPN_CLUB);
        }
        else
            return (item_race);
        break;

    case MONS_WIGHT:
    case MONS_NORRIS:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = (one_chance_in(6) ? WPN_WAR_AXE + random2(4)
                                              : WPN_MACE + random2(12));

        if (coinflip())
        {
            force_item = true;
            item_race = MAKE_ITEM_NO_RACE;
            item.plus += 1 + random2(3);
            item.plus2 += 1 + random2(3);

            if (one_chance_in(5))
                set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FREEZING );
        }

        if (one_chance_in(3))
            do_curse_item( item );
        break;

    case MONS_GNOLL:
    case MONS_OGRE_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
    case MONS_EDMUND:
    case MONS_DUANE:
        item_race = MAKE_ITEM_NO_RACE;

        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;

            const int temp_rand = random2(5);
            item.sub_type = ((temp_rand >  2) ? WPN_SPEAR : // 40%
                                 (temp_rand == 2) ? WPN_FLAIL : // 20%
                                 (temp_rand == 1) ? WPN_HALBERD // 20%
                                                  : WPN_CLUB);  // 20%
        }
        break;

    case MONS_ORC:
    case MONS_ORC_PRIEST:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {gdl}

    case MONS_TERENCE:
    case MONS_DRACONIAN:
    case MONS_DRACONIAN_ZEALOT:
        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;

            const int temp_rand = random2(240);
            item.sub_type = ((temp_rand > 209) ? WPN_DAGGER :      //12.50%
                                 (temp_rand > 179) ? WPN_CLUB :        //12.50%
                                 (temp_rand > 152) ? WPN_FLAIL :       //11.25%
                                 (temp_rand > 128) ? WPN_HAND_AXE :    //10.00%
                                 (temp_rand > 108) ? WPN_HAMMER :      // 8.33%
                                 (temp_rand >  88) ? WPN_HALBERD :     // 8.33%
                                 (temp_rand >  68) ? WPN_SHORT_SWORD : // 8.33%
                                 (temp_rand >  48) ? WPN_MACE :        // 8.33%
                                 (temp_rand >  38) ? WPN_WHIP :        // 4.17%
                                 (temp_rand >  28) ? WPN_TRIDENT :     // 4.17%
                                 (temp_rand >  18) ? WPN_FALCHION :    // 4.17%
                                 (temp_rand >   8) ? WPN_MORNINGSTAR : // 4.17%
                                 (temp_rand >   2) ? WPN_WAR_AXE           // 2.50%
                                                   : WPN_SPIKED_FLAIL);// 1.25%
        }
        else
            return (item_race);
        break;

    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
    {
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;

        const int temp_rand = random2(100);
        item.sub_type = ((temp_rand > 79) ? WPN_LONG_SWORD :    // 20%
                         (temp_rand > 59) ? WPN_SHORT_SWORD :   // 20%
                         (temp_rand > 45) ? WPN_SCIMITAR :      // 14%
                         (temp_rand > 31) ? WPN_MACE :          // 14%
                         (temp_rand > 18) ? WPN_BOW :           // 13%
                         (temp_rand >  5) ? WPN_HAND_CROSSBOW   // 13%
                         : WPN_LONGBOW);       //  6%
        break;
    }

    case MONS_DEEP_ELF_BLADEMASTER:
    {
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;

        // If the blademaster already has a weapon, give him the exact same
        // sub_type to match.

        const item_def *weap = mon->mslot_item(MSLOT_WEAPON);
        if (weap && weap->base_type == OBJ_WEAPONS)
            item.sub_type = weap->sub_type;
        else
            item.sub_type = random_choose_weighted(40, WPN_SABRE,
                                                   10, WPN_SHORT_SWORD,
                                                   2,  WPN_QUICK_BLADE,
                                                   0);
        break;
    }

    case MONS_DEEP_ELF_MASTER_ARCHER:
    {
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_LONGBOW;
        break;
    }

    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    {
        if (mons_genus(mon->type) != MONS_DRACONIAN)
            item_race = MAKE_ITEM_ELVEN;
        
        item.base_type = OBJ_WEAPONS;

        const int temp_rand = random2(6);
        item.sub_type = ((temp_rand > 3) ? WPN_LONG_SWORD : // 2 in 6
                             (temp_rand > 2) ? WPN_SHORT_SWORD :// 1 in 6
                             (temp_rand > 1) ? WPN_SABRE :      // 1 in 6
                             (temp_rand > 0) ? WPN_DAGGER       // 1 in 6
                                             : WPN_WHIP);       // 1 in 6
        break;
    }
    case MONS_ORC_WARRIOR:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_BLORK_THE_ORC:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through {dlb}
    case MONS_DANCING_WEAPON:   // give_level may have been adjusted above
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_HAROLD:
    case MONS_JOSEPH:
    case MONS_LOUISE:
    case MONS_MICHAEL:
    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_RUPERT:
    case MONS_SKELETAL_WARRIOR:
    case MONS_WAYNE:
    case MONS_PALE_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_TIAMAT:
    {
        item.base_type = OBJ_WEAPONS;
        const int temp_rand = random2(120);
        item.sub_type = ((temp_rand > 109) ? WPN_LONG_SWORD :   // 8.33%
                             (temp_rand >  99) ? WPN_SHORT_SWORD :  // 8.33%
                             (temp_rand >  89) ? WPN_SCIMITAR :     // 8.33%
                             (temp_rand >  79) ? WPN_BATTLEAXE :    // 8.33%
                             (temp_rand >  69) ? WPN_HAND_AXE :     // 8.33%
                             (temp_rand >  59) ? WPN_HALBERD :      // 8.33%
                             (temp_rand >  49) ? WPN_GLAIVE :       // 8.33%
                             (temp_rand >  39) ? WPN_MORNINGSTAR :  // 8.33%
                             (temp_rand >  29) ? WPN_GREAT_MACE :   // 8.33%
                             (temp_rand >  19) ? WPN_TRIDENT :      // 8.33%
                             (temp_rand >  10) ? WPN_WAR_AXE :          // 7.50%
                             (temp_rand >   1) ? WPN_FLAIL :        // 7.50%
                             (temp_rand >   0) ? WPN_BROAD_AXE      // 0.83%
                                               : WPN_SPIKED_FLAIL); // 0.83%
        break;
    }
    case MONS_ORC_WARLORD:
        // being at the top has its privileges
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall-through
    case MONS_ORC_KNIGHT:
        item_race = MAKE_ITEM_ORCISH;
        // Occasionally get crossbows.
        if (!melee_only && one_chance_in(9))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_CROSSBOW;
            break;
        }
        // deliberate fall-through
    case MONS_URUG:
        item_race = MAKE_ITEM_ORCISH;
        // fall through
        
    case MONS_NORBERT:
    case MONS_JOZEF:
    case MONS_VAULT_GUARD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_DRACONIAN_KNIGHT:
    {
        item.base_type = OBJ_WEAPONS;

        const int temp_rand = random2(25);
        item.sub_type = ((temp_rand > 20) ? WPN_GREAT_SWORD :   // 16%
                             (temp_rand > 16) ? WPN_LONG_SWORD :    // 16%
                             (temp_rand > 12) ? WPN_BATTLEAXE :     // 16%
                             (temp_rand >  8) ? WPN_WAR_AXE :       // 16%
                             (temp_rand >  5) ? WPN_GREAT_MACE :    // 12%
                             (temp_rand >  3) ? WPN_DIRE_FLAIL :    //  8%
                             (temp_rand >  2) ? WPN_BARDICHE :      //  4%
                             (temp_rand >  1) ? WPN_GLAIVE :        //  4%
                             (temp_rand >  0) ? WPN_BROAD_AXE       //  4%
                                              : WPN_HALBERD);       //  4%

        if (one_chance_in(4))
            item.plus += 1 + random2(3);
        break;
    }

    case MONS_POLYPHEMUS:
    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_MISSILES;
        item.sub_type = MI_LARGE_ROCK;
        break;

    case MONS_TWO_HEADED_OGRE:
    case MONS_ETTIN:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                              : WPN_GIANT_CLUB);

        if (one_chance_in(10) || mon->type == MONS_ETTIN)
        {
            item.sub_type = ((one_chance_in(10)) ? WPN_DIRE_FLAIL
                                                     : WPN_GREAT_MACE);
        }
        break;

    case MONS_REAPER:
        level = MAKE_GOOD_ITEM;
        // intentional fall-through...

    case MONS_SIGMUND:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_SCYTHE;
        break;

    case MONS_BALRUG:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_DEMON_WHIP;
        break;

    case MONS_RED_DEVIL:
        if (!one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type = (one_chance_in(3) ? WPN_DEMON_TRIDENT
                                                  : WPN_TRIDENT);
        }
        break;

    case MONS_OGRE:
    case MONS_HILL_GIANT:
    case MONS_EROLCHA:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;

        item.sub_type = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                              : WPN_GIANT_CLUB);

        if (one_chance_in(10))
        {
            item.sub_type = (one_chance_in(10) ? WPN_DIRE_FLAIL
                                                   : WPN_GREAT_MACE);
        }
        break;

    case MONS_MERMAID:
        if (one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_SPEAR;
        }
        break;

    case MONS_MERFOLK:
        if (one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_TRIDENT;
        }
        break;

    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_BOW;
        if (mon->type == MONS_CENTAUR_WARRIOR
                && one_chance_in(3))
            item.sub_type = WPN_LONGBOW;
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_CROSSBOW;
        break;

    case MONS_EFREET:
    case MONS_ERICA:
        force_item = true;
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_SCIMITAR;
        item.plus = random2(5);
        item.plus2 = random2(5);
        item.colour = RED;  // forced by force_item above {dlb}
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );
        break;

    case MONS_ANGEL:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.colour = WHITE;        // forced by force_item above {dlb}

        set_equip_desc( item, ISFLAG_GLOWING );
        if (one_chance_in(3))
        {
            item.sub_type = (one_chance_in(3) ? WPN_GREAT_MACE : WPN_MACE);
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_HOLY_WRATH );
        }
        else
        {
            item.sub_type = WPN_LONG_SWORD;
        }

        item.plus = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        break;

    case MONS_DAEVA:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.colour = WHITE;        // forced by force_item above {dlb}

        item.sub_type = (one_chance_in(4) ? WPN_BLESSED_BLADE
                                              : WPN_LONG_SWORD);

        set_equip_desc( item, ISFLAG_GLOWING );
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_HOLY_WRATH );
        item.plus = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        break;

    case MONS_HELL_KNIGHT:
    case MONS_MAUD:
    case MONS_FREDERICK:
    case MONS_MARGERY:
    {
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_LONG_SWORD + random2(3);

        if (one_chance_in(7))
            item.sub_type = WPN_HALBERD;
        if (one_chance_in(7))
            item.sub_type = WPN_GLAIVE;
        if (one_chance_in(7))
            item.sub_type = WPN_GREAT_MACE;
        if (one_chance_in(7))
            item.sub_type = WPN_BATTLEAXE;
        if (one_chance_in(7))
            item.sub_type = WPN_WAR_AXE;
        if (one_chance_in(7))
            item.sub_type = WPN_BROAD_AXE;
        if (one_chance_in(7))
            item.sub_type = WPN_DEMON_TRIDENT;
        if (one_chance_in(7))
            item.sub_type = WPN_DEMON_BLADE;
        if (one_chance_in(7))
            item.sub_type = WPN_DEMON_WHIP;

        if (one_chance_in(3))
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );
        else if (one_chance_in(3))
        {
            const int temp_rand = random2(5);
            set_item_ego_type( item, OBJ_WEAPONS, 
                                ((temp_rand == 0) ? SPWPN_DRAINING :
                                 (temp_rand == 1) ? SPWPN_VORPAL :
                                 (temp_rand == 2) ? SPWPN_PAIN :
                                 (temp_rand == 3) ? SPWPN_DISTORTION 
                                                  : SPWPN_SPEED) );
        }

        item.plus += random2(6);
        item.plus2 += random2(6);

        item.colour = RED;  // forced by force_item above {dlb}

        if (one_chance_in(3))
            item.colour = DARKGREY;
        if (one_chance_in(5))
            item.colour = CYAN;
        break;
    }
    case MONS_FIRE_GIANT:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_GREAT_SWORD;
        item.plus = 0;
        item.plus2 = 0;
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );

        item.colour = RED;  // forced by force_item above {dlb}
        if (one_chance_in(3))
            item.colour = DARKGREY;
        if (one_chance_in(5))
            item.colour = CYAN;
        break;

    case MONS_FROST_GIANT:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_BATTLEAXE;
        item.plus = 0;
        item.plus2 = 0;
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FREEZING );

        // forced by force_item above {dlb}
        item.colour = (one_chance_in(3) ? WHITE : CYAN);
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_ORC_WIZARD:
    case MONS_ORC_SORCERER:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through, I guess {dlb}
    case MONS_NECROMANCER:
    case MONS_WIZARD:
    case MONS_PSYCHE:
    case MONS_DONALD:
    case MONS_JOSEPHINE:
    case MONS_AGNES:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_DAGGER;
        break;

    case MONS_CEREBOV:
        force_item = true;
        make_item_fixed_artefact( item, false, SPWPN_SWORD_OF_CEREBOV );
        break;

    case MONS_DISPATER:
        force_item = true;
        make_item_fixed_artefact( item, false, SPWPN_STAFF_OF_DISPATER );
        break;

    case MONS_ASMODEUS:
        force_item = true;
        make_item_fixed_artefact( item, false, SPWPN_SCEPTRE_OF_ASMODEUS );
        break;

    case MONS_GERYON: 
        //mv: probably should be moved out of this switch,
        //but it's not worth of it, unless we have more
        //monsters with misc. items
        item.base_type = OBJ_MISCELLANY;
        item.sub_type = MISC_HORN_OF_GERYON;
        break;

    case MONS_SALAMANDER: //mv: new 8 Aug 2001
                          //Yes, they've got really nice items, but
                          //it's almost impossible to get them
    {
        force_item = true;
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        const int temp_rand = random2(6);

        item.sub_type = ((temp_rand == 5) ? WPN_GREAT_SWORD :
                             (temp_rand == 4) ? WPN_TRIDENT :
                             (temp_rand == 3) ? WPN_SPEAR :
                             (temp_rand == 2) ? WPN_GLAIVE :
                             (temp_rand == 1) ? WPN_BOW
                                              : WPN_HALBERD);

        if (is_range_weapon(item))
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAME );
        else
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );

        item.plus = random2(5);
        item.plus2 = random2(5);
        item.colour = RED;  // forced by force_item above {dlb}
        break;
    }
    
    default: break;
    }                           // end "switch(mon->type)"
    
    // Nagas don't get racial stuff.
    if (mons_genus(mon->type) == MONS_NAGA)
        item_race = MAKE_ITEM_NO_RACE;

    // only happens if something in above switch doesn't set it {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return (item_race);

    item.x = 0;
    item.y = 0;
    item.link = NON_ITEM;

    if (force_item)
        item.quantity = iquan;
    else if (mons_is_unique( mon->type ))
    {
        if (random2(100) <= 9 + mon->hit_dice)
            level = MAKE_GOOD_ITEM;
        else if (level != MAKE_GOOD_ITEM)
            level += 5;
    }

    const object_class_type xitc = item.base_type;
    const int xitt = item.sub_type;

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the
    // base and subtypes and create a new item. -- bwr
    const int thing_created =
        ((force_item) ? bp : items( 0, xitc, xitt, true,
                                    level, item_race) );

    if (thing_created == NON_ITEM)
        return (item_race);

    item_def &i = mitm[thing_created];
    if (melee_only && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
    {
        destroy_item(thing_created);
        return (item_race);
    }

    if (force_item)
        item_set_appearance(i);
    
    if (iquan > 1 && !force_item)
        i.quantity = iquan;
        
    give_monster_item(mon, thing_created, force_item);
    
    if (give_aux_melee && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
        give_weapon(mon, level, true, false);

    return (item_race);
}

static void give_ammo(monsters *mon, int level,
                      item_make_species_type item_race)
{
    // mv: gives ammunition
    // note that item_race is not reset for this section
    if (const item_def *launcher = mon->launcher())
    {
        const object_class_type xitc = OBJ_MISSILES;
        int xitt = fires_ammo_type(*launcher);

        if (xitt == MI_STONE && one_chance_in(15))
            xitt = MI_SLING_BULLET;

        const int thing_created = items(0, xitc, xitt, true, level, item_race);
        if (thing_created == NON_ITEM)
            return;

        // monsters will always have poisoned needles -- otherwise
        // they are just going to behave badly --GDL
        if (xitt == MI_NEEDLE)
            set_item_ego_type(mitm[thing_created], OBJ_MISSILES, 
                    got_curare_roll(level)? 
                            SPMSL_CURARE
                          : SPMSL_POISONED);

        // Master archers get double ammo - archery is their only attack.
        if (mon->type == MONS_DEEP_ELF_MASTER_ARCHER)
            mitm[thing_created].quantity *= 2;
        
        give_monster_item(mon, thing_created);
    }                           // end if needs ammo
    else
    {
        // Give some monsters throwing weapons.
        int weap_type = -1;
        object_class_type weap_class = OBJ_WEAPONS;
        int qty = 0;
        switch (mon->type)
        {
        case MONS_KOBOLD:
        case MONS_BIG_KOBOLD:
            if (random2(5) < 2)
            {
                item_race = MAKE_ITEM_NO_RACE;
                weap_class = OBJ_MISSILES;
                weap_type = MI_DART;
                qty = 1 + random2(5);
            }
            break;

        case MONS_ORC_WARRIOR:
            if (one_chance_in(
                    you.where_are_you == BRANCH_ORCISH_MINES? 9 : 20))
            {
                weap_type =
                    random_choose(WPN_HAND_AXE, WPN_SPEAR, -1);
                qty = random_range(4, 8);
                item_race = MAKE_ITEM_ORCISH;
            }
            break;

        case MONS_ORC:
            if (one_chance_in(20))
            {
                weap_type =
                    random_choose(WPN_HAND_AXE, WPN_SPEAR, -1);
                qty = random_range(2, 5);                
                item_race = MAKE_ITEM_ORCISH;
            }
            break;

        case MONS_URUG:
            weap_type  = MI_JAVELIN;
            weap_class = OBJ_MISSILES;
            item_race  = MAKE_ITEM_ORCISH;
            qty = random_range(4, 7);
            break;

        case MONS_MERFOLK:
            if (!one_chance_in(3))
            {
                item_race = MAKE_ITEM_NO_RACE;
                if (coinflip())
                {
                    weap_type = WPN_SPEAR;
                    qty = 1 + random2(3);
                }
                else
                {
                    weap_class = OBJ_MISSILES;
                    weap_type = MI_JAVELIN;
                    qty = 3 + random2(6);
                }
            }
            // fall through

        case MONS_DRACONIAN_KNIGHT:
        case MONS_GNOLL:
        case MONS_HILL_GIANT:
            if (!one_chance_in(20))
                break;
            // fall through
        case MONS_HAROLD: // bounty hunters
        case MONS_JOZEF:
            weap_class = OBJ_MISSILES;
            weap_type = MI_THROWING_NET;
            qty = coinflip() + 1;
            break;
        }

        if (weap_type == -1)
            return ;
        
        const int thing_created =
            items( 0, weap_class, weap_type, true, level, item_race );
        if (thing_created != NON_ITEM)
        {
            mitm[thing_created].quantity = qty;
            give_monster_item(mon, thing_created, false,
                              &monsters::pickup_throwable_weapon);
        }
    }
}

static bool make_item_for_monster(
    monsters *mons,
    object_class_type base,
    int subtype,
    int level,
    item_make_species_type race = MAKE_ITEM_NO_RACE,
    int allow_uniques = 0)
{
    const int bp = get_item_slot();
    if (bp == NON_ITEM)
        return (false);

    const int thing_created =
        items( allow_uniques, base, subtype, true, level, race );

    if (thing_created == NON_ITEM)
        return (false);

    give_monster_item(mons, thing_created);
    return (true);
}

void give_shield(monsters *mon, int level)
{
    switch (mon->type)
    {
    case MONS_DAEVA:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
        
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
        if (one_chance_in(5))
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_ELVEN);
        break;
    case MONS_NAGA_WARRIOR:
    case MONS_VAULT_GUARD:
        if (one_chance_in(3))
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  one_chance_in(3)? ARM_LARGE_SHIELD
                                  : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        break;
    case MONS_DRACONIAN_KNIGHT:
        if (coinflip())
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  coinflip()? ARM_LARGE_SHIELD : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        break;
    case MONS_DEEP_ELF_KNIGHT:
        if (coinflip())
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_ELVEN);
        break;
    case MONS_NORRIS:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
    case MONS_NORBERT:
    case MONS_LOUISE:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
    case MONS_DONALD:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
        
    default: break;
    }
}

void give_armour(monsters *mon, int level)
{
    const int bp = get_item_slot();
    if (bp == NON_ITEM)
        return;

    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;

    int force_colour = 0; //mv: important !!! Items with force_colour = 0
                         //are colored defaultly after following
                         //switch. Others will get force_colour.

    switch (mon->type)
    {
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
        item_race = MAKE_ITEM_ELVEN;
        mitm[bp].base_type = OBJ_ARMOUR;        
        mitm[bp].sub_type = ARM_LEATHER_ARMOUR;
        break;

    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ELVEN;
        // deliberate fall through {dlb}
    case MONS_IJYB:
    case MONS_ORC:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_ORC_PRIEST:
    case MONS_ORC_SORCERER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}
    case MONS_ERICA:
    case MONS_HAROLD:
    case MONS_JOSEPH:
    case MONS_JOSEPHINE:
    case MONS_JOZEF:
    case MONS_NORBERT:
    case MONS_PSYCHE:
    case MONS_TERENCE:
        if (random2(5) < 2)
        {
            mitm[bp].base_type = OBJ_ARMOUR;

            switch (random2(8))
            {
            case 0:
            case 1:
            case 2:
            case 3:
                mitm[bp].sub_type = ARM_LEATHER_ARMOUR;
                break;
            case 4:
            case 5:
                mitm[bp].sub_type = ARM_RING_MAIL;
                break;
            case 6:
                mitm[bp].sub_type = ARM_SCALE_MAIL;
                break;
            case 7:
                mitm[bp].sub_type = ARM_CHAIN_MAIL;
                break;
            }
        }
        else
            return;
        break;

    case MONS_DUANE:
    case MONS_EDMUND:
    case MONS_RUPERT:
    case MONS_URUG:
    case MONS_WAYNE:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_LEATHER_ARMOUR + random2(4);
        break;

    case MONS_ORC_WARLORD:
        // being at the top has its privileges
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall through
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARRIOR:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}
    case MONS_FREDERICK:
    case MONS_HELL_KNIGHT:
    case MONS_LOUISE:
    case MONS_MARGERY:
    case MONS_MAUD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAULT_GUARD:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_CHAIN_MAIL + random2(4);
        break;

    case MONS_ANGEL:
    case MONS_SIGMUND:
    case MONS_WIGHT:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        force_colour = WHITE; //mv: always white
        break;

    // centaurs sometimes wear barding
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        item_race = MAKE_ITEM_NO_RACE;
        if ( one_chance_in( mon->type == MONS_CENTAUR              ? 1000 :
                            mon->type == MONS_CENTAUR_WARRIOR      ?  500 :
                            mon->type == MONS_YAKTAUR              ?  300
                         /* mon->type == MONS_YAKTAUR_CAPTAIN ? */ :  200))
        {
            mitm[bp].base_type = OBJ_ARMOUR;
            mitm[bp].sub_type = ARM_CENTAUR_BARDING;
        }
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
        item_race = MAKE_ITEM_NO_RACE;
        if ( one_chance_in( mon->type == MONS_NAGA         ?  800 :
                            mon->type == MONS_NAGA_WARRIOR ?  300 :
                            mon->type == MONS_NAGA_MAGE    ?  200
                                                           :  100))
        {
            mitm[bp].base_type = OBJ_ARMOUR;
            mitm[bp].sub_type = ARM_NAGA_BARDING;
        }
        else if (mon->type == MONS_GREATER_NAGA || one_chance_in(3))
        {
            mitm[bp].base_type = OBJ_ARMOUR;
            mitm[bp].sub_type = ARM_ROBE;
        }
        break;

    case MONS_DONALD:
    case MONS_JESSICA:
    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_OGRE_MAGE:
    case MONS_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_TIAMAT:
    case MONS_WIZARD:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        break;

    case MONS_ORC_WIZARD:
    case MONS_BLORK_THE_ORC:
        item_race = MAKE_ITEM_ORCISH;
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        break;

    case MONS_BORIS:
        level = MAKE_GOOD_ITEM;
        // fall-through
    case MONS_AGNES:
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        force_colour = DARKGREY; //mv: always darkgrey
        break;

    default:
        return;
    }

    const object_class_type xitc = mitm[bp].base_type;
    const int xitt = mitm[bp].sub_type;

    if (mons_is_unique( mon->type ) && level != MAKE_GOOD_ITEM)
    {
        if (random2(100) < 9 + mon->hit_dice)
            level = MAKE_GOOD_ITEM;
        else 
            level = level * 2 + 5;
    }

    const int thing_created = items( 0, xitc, xitt, true, level, item_race );

    if (thing_created == NON_ITEM)
        return;

    give_monster_item(mon, thing_created);

    //mv: all items with force_colour = 0 are colored via items().
    if (force_colour) 
        mitm[thing_created].colour = force_colour;
}

void give_item(int mid, int level_number) //mv: cleanup+minor changes
{
    monsters *mons = &menv[mid];

    give_scroll(mons, level_number);
    give_wand(mons, level_number);
    give_potion(mons, level_number);
    
    const item_make_species_type item_race = give_weapon(mons, level_number);
    
    give_ammo(mons, level_number, item_race);
    give_armour(mons, 1 + level_number / 2);
    give_shield(mons, 1 + level_number / 2);
}                               // end give_item()

jewellery_type get_random_amulet_type()
{
    return (jewellery_type)
        (AMU_FIRST_AMULET + random2(NUM_JEWELLERY - AMU_FIRST_AMULET));
}

static jewellery_type get_raw_random_ring_type()
{
    return (jewellery_type) (RING_REGENERATION + random2(NUM_RINGS));
}

jewellery_type get_random_ring_type()
{
    const jewellery_type j = get_raw_random_ring_type();
    // Adjusted distribution here -- bwr
    if ((j == RING_INVISIBILITY
         || j == RING_REGENERATION
         || j == RING_TELEPORT_CONTROL
         || j == RING_SLAYING)
        && !one_chance_in(3))
    {
        return get_raw_random_ring_type();
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
    int armtype = random2(3);
    
    if (random2(35) <= item_level + 10)
    {
        armtype = random2(5);
        if (one_chance_in(4))
            armtype = ARM_ANIMAL_SKIN;
    }

    if (random2(60) <= item_level + 10)
        armtype = random2(ARM_SHIELD); // body armour of some kind

    if (10 + item_level >= random2(400) && one_chance_in(20))
        armtype = ARM_DRAGON_HIDE + random2(7); // (ice) dragon/troll/crystal

    if (10 + item_level >= random2(500) && one_chance_in(20))
    {
        // other dragon hides/armour or animal skin
        armtype = ARM_STEAM_DRAGON_HIDE + random2(11);

        if (armtype == ARM_ANIMAL_SKIN && one_chance_in(20))
            armtype = ARM_CRYSTAL_PLATE_MAIL;
    }

    // secondary armours:
    if (one_chance_in(5))
    {
        armtype = ARM_SHIELD + random2(5);
        if ( armtype > ARM_HELMET )
            armtype += 3;

        if ( armtype == ARM_HELMET && one_chance_in(3) )
            armtype = ARM_HELMET + random2(4);

        if (armtype == ARM_SHIELD)                 // 33.3%
        {
            if (coinflip())
                armtype = ARM_BUCKLER;             // 50.0%
            else if (one_chance_in(3))
                armtype = ARM_LARGE_SHIELD;        // 16.7%
        }
    }

    return static_cast<armour_type>(armtype);
}

// Sets item appearance to match brands, if any.
static void item_set_appearance(item_def &item)
{
    if (get_equip_desc(item) != ISFLAG_NO_DESC)
        return;
    
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (weapon_is_visibly_special(item))
            set_equip_desc( item,
                            (coinflip() ? ISFLAG_GLOWING : ISFLAG_RUNED) );
        break;
        
    case OBJ_ARMOUR:
        // if not given a racial type, and special, give shiny/runed/etc desc.
        if (get_armour_ego_type( item ) != SPARM_NORMAL
            || (item.plus != 0 && !one_chance_in(3)))
        {
            const item_status_flag_type descs[] =
            {
                ISFLAG_GLOWING, ISFLAG_RUNED, ISFLAG_EMBROIDERED_SHINY
            };
            
            set_equip_desc( item, RANDOM_ELEMENT(descs) );
        }
        break;

    default:
        break;
    }
}

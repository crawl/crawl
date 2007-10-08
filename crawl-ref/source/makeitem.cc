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

static bool weapon_is_visibly_special(const item_def &item);

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

static bool is_weapon_special(int the_weapon)
{
    return (mitm[the_weapon].special != SPWPN_NORMAL);
}                               // end is_weapon_special()

static void set_weapon_special(int the_weapon, int spwpn)
{
    set_item_ego_type( mitm[the_weapon], OBJ_WEAPONS, spwpn );
}                               // end set_weapon_special()

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
        case SK_LONG_SWORDS:
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
        item_colour = MAGENTA;
        break;
      case ARM_HELMET:
        if (get_helmet_type(item) == THELM_CAP
            || get_helmet_type(item) == THELM_WIZARD_HAT)
        {
            item_colour = MAGENTA;
        } 
        else
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
        item_colour = random_colour();
        break;

    case ARM_HELMET:
        // caps and wizard's hats are random coloured
        if (get_helmet_type(item) == THELM_CAP
            || get_helmet_type(item) == THELM_WIZARD_HAT)
        {
            item_colour = random_colour();
        } 
        else
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
                
            case RUNE_SHOALS:                   // liquid
                item.colour = EC_WATER;
                break;

            // This one is hardly unique, but since colour isn't used for 
            // stacking, so we don't have to worry to much about this. -- bwr
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

// Returns item slot or NON_ITEM if it fails
int items( int allow_uniques,       // not just true-false,
                                    //     because of BCR acquirement hack
           object_class_type force_class, // desired OBJECTS class {dlb}
           int force_type,          // desired SUBTYPE - enum varies by OBJ
           bool dont_place,         // don't randomly place item on level
           int item_level,          // level of the item, can differ from global
           int item_race,           // weapon / armour racial categories
                                    // item_race also gives type of rune!
           unsigned mapmask) 
{
    const bool make_good_item = (item_level == MAKE_GOOD_ITEM);
    
    int temp_rand = 0;             // probability determination {dlb}
    int range_charges = 0;         // for OBJ_WANDS charge count {dlb}
    int loopy = 0;                 // just another loop variable {dlb}
    int count = 0;                 // just another loop variable {dlb}

    int race_plus = 0;
    int race_plus2 = 0;
    int x_pos, y_pos;

    int quant = 0;

    int icky = 0;
    int p = 0;

    // find an empty slot for the item (with culling if required)
    p = get_item_slot(10);
    if (p == NON_ITEM)
        return (NON_ITEM);

    // cap item_level unless an acquirement-level item {dlb}:
    if (item_level > 50 && !make_good_item)
        item_level = 50;

    // determine base_type for item generated {dlb}:
    if (force_class != OBJ_RANDOM)
        mitm[p].base_type = force_class;
    else
    {
        // nice and large for subtle differences {dlb}
        temp_rand = random2(10000);

        mitm[p].base_type =  ((temp_rand <   50) ? OBJ_STAVES :   //  0.50%
                              (temp_rand <  200) ? OBJ_BOOKS :    //  1.50%
                              (temp_rand <  450) ? OBJ_JEWELLERY ://  2.50%
                              (temp_rand <  800) ? OBJ_WANDS :    //  3.50%
                              (temp_rand < 1500) ? OBJ_FOOD :     //  7.00%
                              (temp_rand < 2500) ? OBJ_ARMOUR :   // 10.00%
                              (temp_rand < 3500) ? OBJ_WEAPONS :  // 10.00%
                              (temp_rand < 4500) ? OBJ_POTIONS :  // 10.00%
                              (temp_rand < 6000) ? OBJ_MISSILES : // 15.00%
                              (temp_rand < 8000) ? OBJ_SCROLLS    // 20.00%
                                                 : OBJ_GOLD);     // 20.00%

        // misc items placement wholly dependent upon current depth {dlb}:
        if (item_level > 7 && (20 + item_level) >= random2(3500))
            mitm[p].base_type = OBJ_MISCELLANY;

        if (item_level < 7 
            && (mitm[p].base_type == OBJ_BOOKS 
                || mitm[p].base_type == OBJ_STAVES
                || mitm[p].base_type == OBJ_WANDS)
            && random2(7) >= item_level)
        {
            mitm[p].base_type = coinflip() ? OBJ_POTIONS : OBJ_SCROLLS;
        }
    }

    // determine sub_type accordingly {dlb}:
    switch (mitm[p].base_type)
    {
    case OBJ_WEAPONS:
        // generate initial weapon subtype using weighted function --
        // indefinite loop now more evident and fewer array lookups {dlb}:
        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else 
        {
            if (random2(20) < 20 - item_level)
            {
                // these are the common/low level weapon types
                temp_rand = random2(12);

                mitm[p].sub_type = ((temp_rand ==  0) ? WPN_KNIFE         :
                                    (temp_rand ==  1) ? WPN_QUARTERSTAFF  :
                                    (temp_rand ==  2) ? WPN_SLING         :
                                    (temp_rand ==  3) ? WPN_SPEAR         :
                                    (temp_rand ==  4) ? WPN_HAND_AXE      :
                                    (temp_rand ==  5) ? WPN_DAGGER        :
                                    (temp_rand ==  6) ? WPN_MACE          :
                                    (temp_rand ==  7) ? WPN_DAGGER        :
                                    (temp_rand ==  8) ? WPN_CLUB          :
                                    (temp_rand ==  9) ? WPN_HAMMER        :
                                    (temp_rand == 10) ? WPN_WHIP
                                                      : WPN_SABRE);
            }
            else if (item_level > 6 && random2(100) < (10 + item_level)
                       && one_chance_in(30))
            {
                // place the rare_weapon() == 0 weapons
                //
                // this replaced the infinite loop (wasteful) -- may need
                // to make into its own function to allow ease of tweaking
                // distribution {dlb}:
                temp_rand = random2(10);

                mitm[p].sub_type = ((temp_rand == 9) ? WPN_LAJATANG :
                                    (temp_rand == 8) ? WPN_DEMON_BLADE :
                                    (temp_rand == 7) ? WPN_DEMON_TRIDENT :
                                    (temp_rand == 6) ? WPN_DEMON_WHIP :
                                    (temp_rand == 5) ? WPN_DOUBLE_SWORD :
                                    (temp_rand == 4) ? WPN_EVENINGSTAR :
                                    (temp_rand == 3) ? WPN_EXECUTIONERS_AXE :
                                    (temp_rand == 2) ? WPN_KATANA :
                                    (temp_rand == 1) ? WPN_QUICK_BLADE
                                 /*(temp_rand == 0)*/: WPN_TRIPLE_SWORD);
            }
            else
            {
                // pick a weapon based on rarity 
                for (;;)
                {
                    const int wpntype = random2(NUM_WEAPONS);

                    if (weapon_rarity(wpntype) >= random2(10) + 1)
                    {
                        mitm[p].sub_type = static_cast<unsigned char>(wpntype);
                        break;
                    }
                }
            }
        }

        if (allow_uniques)
        {
            // Note there is nothing to stop randarts being reproduced,
            // except vast improbability.
            if (mitm[p].sub_type != WPN_CLUB && item_level > 2
                && random2(2000) <= 100 + (item_level * 3) && coinflip())
            {
                if (you.level_type != LEVEL_ABYSS
                    && you.level_type != LEVEL_PANDEMONIUM
                    && one_chance_in(50))
                {
                    icky = find_okay_unrandart( OBJ_WEAPONS, force_type );

                    if (icky != -1)
                    {
                        quant = 1;
                        make_item_unrandart( mitm[p], icky );
                        break;
                    }
                }

                make_item_randart( mitm[p] );
                mitm[p].plus = 0;
                mitm[p].plus2 = 0;
                mitm[p].plus += random2(7);
                mitm[p].plus2 += random2(7);

                if (one_chance_in(3))
                    mitm[p].plus += random2(7);

                if (one_chance_in(3))
                    mitm[p].plus2 += random2(7);

                if (one_chance_in(9))
                    mitm[p].plus -= random2(7);

                if (one_chance_in(9))
                    mitm[p].plus2 -= random2(7);

                quant = 1;

                if (one_chance_in(4))
                {
                    do_curse_item( mitm[p] );
                    mitm[p].plus  = -random2(6);
                    mitm[p].plus2 = -random2(6);
                }
                else if ((mitm[p].plus < 0 || mitm[p].plus2 < 0)
                         && !one_chance_in(3))
                {
                    do_curse_item( mitm[p] );
                }
                break;
            }

            if (item_level > 6
                && random2(3000) <= 30 + (item_level * 3) && one_chance_in(12))
            {
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS, "Making fixed artefact.");
#endif
                if (make_item_fixed_artefact( mitm[p], (item_level == 51) ))
                {
                    quant = 1;
                    break;
                }
            }
        }

        ASSERT(!is_fixed_artefact(mitm[p]) && !is_random_artefact(mitm[p]));

        if (make_good_item
            && force_type != OBJ_RANDOM
            && (mitm[p].sub_type == WPN_CLUB || mitm[p].sub_type == WPN_SLING))
        {
            mitm[p].sub_type = WPN_LONG_SWORD;
        }

        quant = 1;

        mitm[p].plus = 0;
        mitm[p].plus2 = 0;
        mitm[p].special = SPWPN_NORMAL;

        if (item_race == MAKE_ITEM_RANDOM_RACE && coinflip())
        {
            switch (mitm[p].sub_type)
            {
            case WPN_CLUB:
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_MACE:
            case WPN_FLAIL:
            case WPN_SPIKED_FLAIL:
            case WPN_GREAT_MACE:
            case WPN_DIRE_FLAIL:
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_MORNINGSTAR:
            case WPN_HAMMER:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_DAGGER:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_SHORT_SWORD:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_FALCHION:
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_LONG_SWORD:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_GREAT_SWORD:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_SCIMITAR:
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_WAR_AXE:
            case WPN_HAND_AXE:
            case WPN_BROAD_AXE:
            case WPN_BATTLEAXE:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_SPEAR:
            case WPN_TRIDENT:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_HALBERD:
            case WPN_GLAIVE:
            case WPN_EXECUTIONERS_AXE:
            case WPN_LOCHABER_AXE:
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_QUICK_BLADE:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_KATANA:
            case WPN_LAJATANG:
            case WPN_KNIFE:
            case WPN_SLING:
                set_equip_race( mitm[p], ISFLAG_NO_RACE );
                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
                break;

            case WPN_BOW:
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_LONGBOW:
                set_equip_race( mitm[p], one_chance_in(3)  ? ISFLAG_ELVEN 
                                                           : ISFLAG_NO_RACE );
                break;

            case WPN_CROSSBOW:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_HAND_CROSSBOW:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_BLOWGUN:
                if (one_chance_in(10))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;
            }
        }

        // fine, but out-of-order relative to mitm[].special ordering {dlb}
        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN:
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            break;

        case MAKE_ITEM_ORCISH:
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;
        }

        // if we allow acquirement-type items to be orcish, then 
        // there's a good chance that we'll just strip them of 
        // their ego type at the bottom of this function. -- bwr
        if (make_good_item
            && get_equip_race( mitm[p] ) == ISFLAG_ORCISH)
        {
            set_equip_race( mitm[p], ISFLAG_NO_RACE );
        }

        switch (get_equip_race( mitm[p] ))
        {
        case ISFLAG_ORCISH:
            if (coinflip())
                race_plus--;
            if (coinflip())
                race_plus2++;
            break;

        case ISFLAG_ELVEN:
            race_plus += random2(3);
            break;

        case ISFLAG_DWARVEN:
            if (coinflip())
                race_plus++;
            if (coinflip())
                race_plus2++;
            break;
        }

        mitm[p].plus  += race_plus;
        mitm[p].plus2 += race_plus2;

        if ((random2(200) <= 50 + item_level
                || make_good_item
                || is_demonic(mitm[p]))
            // nobody would bother enchanting a club
            && mitm[p].sub_type != WPN_CLUB
            && mitm[p].sub_type != WPN_GIANT_CLUB
            && mitm[p].sub_type != WPN_GIANT_SPIKED_CLUB)
        {
            count = 0;

            do
            {
                if (random2(300) <= 100 + item_level
                    || make_good_item
                    || is_demonic( mitm[p] ))
                {
                    // note: this doesn't guarantee special enchantment
                    switch (mitm[p].sub_type)
                    {
                    case WPN_EVENINGSTAR:
                        if (coinflip())
                            set_weapon_special(p, SPWPN_DRAINING);
                        // **** intentional fall through here ****
                    case WPN_MORNINGSTAR:
                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (one_chance_in(4))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(20))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);
                        // **** intentional fall through here ****
                    case WPN_MACE:
                    case WPN_GREAT_MACE:
                        if ((mitm[p].sub_type == WPN_MACE
                                || mitm[p].sub_type == WPN_GREAT_MACE)
                            && one_chance_in(4))
                        {
                            set_weapon_special(p, SPWPN_DISRUPTION);
                        }
                        // **** intentional fall through here ****
                    case WPN_FLAIL:
                    case WPN_SPIKED_FLAIL:
                    case WPN_DIRE_FLAIL:
                    case WPN_HAMMER:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(3) &&
                            (!is_weapon_special(p) || one_chance_in(5)))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_DRAINING);
                        break;


                    case WPN_DAGGER:
                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_RETURNING);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_VENOM);
                        // **** intentional fall through here ****

                    case WPN_SHORT_SWORD:
                    case WPN_SABRE:
                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(8))
                        {
                            set_weapon_special(p,(coinflip() ? SPWPN_FLAMING
                                                             : SPWPN_FREEZING));
                        }

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);
                        break;

                    case WPN_FALCHION:
                    case WPN_LONG_SWORD:
                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_VENOM);
                        // **** intentional fall through here ****
                    case WPN_SCIMITAR:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_SPEED);
                        // **** intentional fall through here ****
                    case WPN_GREAT_SWORD:
                    case WPN_DOUBLE_SWORD:
                    case WPN_TRIPLE_SWORD:
                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5))
                        {
                            set_weapon_special(p,(coinflip() ? SPWPN_FLAMING
                                                             : SPWPN_FREEZING));
                        }

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(4)
                                && (!is_weapon_special(p) || one_chance_in(3)))
                        {
                            set_weapon_special(p, SPWPN_VORPAL);
                        }
                        break;


                    case WPN_WAR_AXE:
                    case WPN_BROAD_AXE:
                    case WPN_BATTLEAXE:
                    case WPN_EXECUTIONERS_AXE:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(14))
                            set_weapon_special(p, SPWPN_DRAINING);
                        // **** intentional fall through here ****
                    case WPN_HAND_AXE:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (mitm[p].sub_type == WPN_HAND_AXE &&
                            one_chance_in(10))
                            set_weapon_special(p, SPWPN_RETURNING);

                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(3)
                                && (!is_weapon_special(p) || one_chance_in(5)))
                        {
                            set_weapon_special(p, SPWPN_VORPAL);
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(4))
                        {
                            set_weapon_special(p,
                                               (coinflip() ? SPWPN_FLAMING
                                                           : SPWPN_FREEZING));
                        }

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_VENOM);

                        break;

                    case WPN_WHIP:
                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(6))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                               : SPWPN_FREEZING));
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (coinflip())
                            set_weapon_special(p, SPWPN_REACHING);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);
                        break;

                    case WPN_HALBERD:
                    case WPN_GLAIVE:
                    case WPN_SCYTHE:
                    case WPN_TRIDENT:
                    case WPN_LOCHABER_AXE:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_PROTECTION);
                        // **** intentional fall through here ****
                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);
                        // **** intentional fall through here ****
                    case WPN_SPEAR:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (mitm[p].sub_type == WPN_SPEAR && one_chance_in(6))
                            set_weapon_special(p, SPWPN_RETURNING);

                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5) &&
                            (!is_weapon_special(p) || one_chance_in(6)))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(6))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_REACHING);
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

                        set_weapon_special( p, (tmp < 375) ? SPWPN_FLAME :
                                               (tmp < 750) ? SPWPN_FROST :
                                               (tmp < 920) ? SPWPN_PROTECTION :
                                               (tmp < 980) ? SPWPN_VORPAL 
                                                           : SPWPN_SPEED );
                        break;
                    }

                    // quarterstaff - not powerful, as this would make
                    // the 'staves' skill just too good
                    case WPN_QUARTERSTAFF:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (got_distortion_roll(item_level))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_PROTECTION);
                        break;


                    case WPN_DEMON_TRIDENT:
                    case WPN_DEMON_WHIP:
                    case WPN_DEMON_BLADE:
                        set_equip_race( mitm[p], ISFLAG_NO_RACE );

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(3)
                            && (mitm[p].sub_type == WPN_DEMON_WHIP
                                || mitm[p].sub_type == WPN_DEMON_TRIDENT))
                        {
                            set_weapon_special(p, SPWPN_REACHING);
                        }

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(5))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_VENOM);
                        break;

                    case WPN_BLESSED_BLADE:     // special gift of TSO
                        set_weapon_special( p, SPWPN_HOLY_WRATH );
                        break;

                    // unlisted weapons have no associated, standard ego-types {dlb}
                    default:
                        break;
                    }
                }                   // end if specially enchanted

                count++;
            }
            while (make_good_item
                    && mitm[p].special == SPWPN_NORMAL 
                    && count < 5);

            // if acquired item still not ego... enchant it up a bit.
            if (make_good_item && mitm[p].special == SPWPN_NORMAL)
            {
                mitm[p].plus  += 2 + random2(3);
                mitm[p].plus2 += 2 + random2(3);
            }

            const int chance = (make_good_item) ? 200 : item_level; 

            // odd-looking, but this is how the algorithm compacts {dlb}:
            for (loopy = 0; loopy < 4; loopy++)
            {
                mitm[p].plus += random2(3);

                if (random2(350) > 20 + chance)
                    break;
            }

            // odd-looking, but this is how the algorithm compacts {dlb}:
            for (loopy = 0; loopy < 4; loopy++)
            {
                mitm[p].plus2 += random2(3);

                if (random2(500) > 50 + chance)
                    break;
            }
        }
        else
        {
            if (one_chance_in(12))
            {
                do_curse_item( mitm[p] );
                mitm[p].plus  -= random2(4);
                mitm[p].plus2 -= random2(4);

                // clear specials {dlb}
                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
            }
        }

        // value was "0" comment said "orc" so I went with comment {dlb}
        if (get_equip_race(mitm[p]) == ISFLAG_ORCISH)
        {
            // no holy wrath or slay orc and 1/2 the time no-ego
            const int brand = get_weapon_brand( mitm[p] );
            if (brand == SPWPN_HOLY_WRATH
                || brand == SPWPN_ORC_SLAYING
                || (brand != SPWPN_NORMAL && coinflip()))
            {
                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
            }
        }

        if (weapon_is_visibly_special(mitm[p]))
        {
            set_equip_desc( mitm[p], (coinflip() ? ISFLAG_GLOWING 
                                                 : ISFLAG_RUNED) );
        }
        break;

    case OBJ_MISSILES:
        quant = 0;
        mitm[p].plus = 0;
        mitm[p].special = SPMSL_NORMAL;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
            mitm[p].sub_type =
                random_choose_weighted(30, MI_STONE,
                                       20, MI_DART,
                                       20, MI_ARROW,
                                       10, MI_NEEDLE,
                                       5,  MI_SLING_BULLET,
                                       2,  MI_JAVELIN,
                                       0);

        // no fancy rocks -- break out before we get to racial/special stuff
        if (mitm[p].sub_type == MI_LARGE_ROCK)
        {
            quant = 2 + random2avg(5,2);
            break;
        }
        else if (mitm[p].sub_type == MI_STONE)
        {
            quant = 1 + random2(9) + random2(12) + random2(15) + random2(12);
            break;
        }

        // set racial type:
        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN: 
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            break;

        case MAKE_ITEM_ORCISH:
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;

        case MAKE_ITEM_RANDOM_RACE:
            if ((mitm[p].sub_type == MI_ARROW
                    || mitm[p].sub_type == MI_DART)
                && one_chance_in(4))
            {
                // elven - not for bolts, though
                set_equip_race( mitm[p], ISFLAG_ELVEN );
            }

            if ((mitm[p].sub_type == MI_ARROW
                    || mitm[p].sub_type == MI_BOLT
                    || mitm[p].sub_type == MI_DART)
                && one_chance_in(4))
            {
                set_equip_race( mitm[p], ISFLAG_ORCISH );
            }

            if ((mitm[p].sub_type == MI_DART
                    || mitm[p].sub_type == MI_BOLT)
                && one_chance_in(6))
            {
                set_equip_race( mitm[p], ISFLAG_DWARVEN );
            }

            if (mitm[p].sub_type == MI_NEEDLE)
            {
                if (one_chance_in(10))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
            }
            break;
        }

        // note that needles can only be poisoned
        //
        // Actually, it'd be really nice if there where
        // some paralysis or slowing poison needles, just
        // so that blowguns have some added utility over
        // the other launchers/throwing weapons. -- bwr
        if (mitm[p].sub_type == MI_NEEDLE)
        {
            const int pois = 
                got_curare_roll(item_level) ? SPMSL_CURARE : SPMSL_POISONED;
            set_item_ego_type( mitm[p], OBJ_MISSILES, pois );
        }
        else
        {
            // decide specials:
            if (make_good_item)
                temp_rand = random2(150);
            else
                temp_rand = random2(2000 - 55 * item_level);

            set_item_ego_type( mitm[p], OBJ_MISSILES, 
                               (temp_rand <  60) ? SPMSL_FLAME :
                               (temp_rand < 120) ? SPMSL_ICE   :
                               (temp_rand < 150) ? SPMSL_POISONED
                                                 : SPMSL_NORMAL );
        }

        // orcish ammo gets poisoned a lot more often -- in the original
        // code it was poisoned every time!?
        if (get_equip_race(mitm[p]) == ISFLAG_ORCISH && one_chance_in(3))
            set_item_ego_type( mitm[p], OBJ_MISSILES, SPMSL_POISONED );

        // reduced quantity if special
        if (mitm[p].sub_type == MI_JAVELIN
            || get_ammo_brand( mitm[p] ) == SPMSL_CURARE)
        {
            quant = random_range(2, 8);
        }
        else if (get_ammo_brand( mitm[p] ) != SPMSL_NORMAL )
            quant = 1 + random2(9) + random2(12) + random2(12);
        else
            quant = 1 + random2(9) + random2(12) + random2(15) + random2(12);

        if (10 + item_level >= random2(100))
            mitm[p].plus += random2(5);

        // elven arrows and dwarven bolts are quality items
        if ((get_equip_race(mitm[p]) == ISFLAG_ELVEN
                && mitm[p].sub_type == MI_ARROW)
            || (get_equip_race(mitm[p]) == ISFLAG_DWARVEN
                && mitm[p].sub_type == MI_BOLT))
        {
            mitm[p].plus += random2(3);
        }
        break;

    case OBJ_ARMOUR:
        quant = 1;

        mitm[p].plus = 0;
        mitm[p].plus2 = 0;
        mitm[p].special = SPARM_NORMAL;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            mitm[p].sub_type = get_random_armour_type(item_level);
        }

        if (mitm[p].sub_type == ARM_HELMET)
        {
            set_helmet_type( mitm[p], THELM_HELMET );
            set_helmet_desc( mitm[p], THELM_DESC_PLAIN );

            if (one_chance_in(3))
                set_helmet_type( mitm[p], random2( THELM_NUM_TYPES ) );

            if (one_chance_in(3))
                set_helmet_random_desc( mitm[p] );
        }

        if (allow_uniques == 1
            && item_level > 2
            && random2(2000) <= (100 + item_level * 3)
            && coinflip())
        {
            if ((you.level_type != LEVEL_ABYSS
                 && you.level_type != LEVEL_PANDEMONIUM)
                && one_chance_in(50))
            {
                icky = find_okay_unrandart(OBJ_ARMOUR);
                if (icky != -1)
                {
                    quant = 1;
                    make_item_unrandart( mitm[p], icky );
                    break;
                }
            }

            hide2armour(mitm[p]);

            // mitm[p].special = SPARM_RANDART_II + random2(4);
            make_item_randart( mitm[p] );
            mitm[p].plus = 0;

            if (mitm[p].sub_type == ARM_BOOTS
                    && one_chance_in(10))
            {
                mitm[p].sub_type = 
                        coinflip()? ARM_NAGA_BARDING
                                  : ARM_CENTAUR_BARDING;
            }

            mitm[p].plus += random2(4);

            if (one_chance_in(5))
                mitm[p].plus += random2(4);

            if (one_chance_in(6))
                mitm[p].plus -= random2(8);

            quant = 1;

            if (one_chance_in(5))
            {
                do_curse_item( mitm[p] );
                mitm[p].plus = -random2(6);
            }
            else if (mitm[p].plus < 0 && !one_chance_in(3))
            {
                do_curse_item( mitm[p] );
            }
            break;
        }

        mitm[p].plus = 0;

        if (item_race == MAKE_ITEM_RANDOM_RACE && coinflip())
        {
            switch (mitm[p].sub_type)
            {
            case ARM_SHIELD:    // shield - must do special things for this!
            case ARM_BUCKLER:
            case ARM_LARGE_SHIELD:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case ARM_CLOAK:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_GLOVES:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_NAGA_BARDING:
            case ARM_CENTAUR_BARDING:
            case ARM_BOOTS:
                if (mitm[p].sub_type == ARM_BOOTS)
                {
                    if (one_chance_in(4))
                    {
                        mitm[p].sub_type = ARM_NAGA_BARDING;
                        break;      
                    }

                    if (one_chance_in(4))
                    {
                        mitm[p].sub_type = ARM_CENTAUR_BARDING;
                        break; 
                    }
                }

                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case ARM_HELMET:
                if (get_helmet_type(mitm[p]) == THELM_CAP
                    || get_helmet_type(mitm[p]) == THELM_WIZARD_HAT)
                {
                    if (one_chance_in(6))
                        set_equip_race( mitm[p], ISFLAG_ELVEN );
                }
                else
                {
                    // helms and helmets
                    if (one_chance_in(8))
                        set_equip_race( mitm[p], ISFLAG_ORCISH );
                    if (one_chance_in(6))
                        set_equip_race( mitm[p], ISFLAG_DWARVEN );
                }
                break;

            case ARM_ROBE:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_RING_MAIL:
            case ARM_SCALE_MAIL:
            case ARM_CHAIN_MAIL:
            case ARM_SPLINT_MAIL:
            case ARM_BANDED_MAIL:
            case ARM_PLATE_MAIL:
                if (mitm[p].sub_type <= ARM_CHAIN_MAIL && one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (mitm[p].sub_type >= ARM_RING_MAIL && one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );

            default:    // skins, hides, crystal plate are always plain
                break;
            }
        }

        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN: 
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            if (coinflip())
                mitm[p].plus++;
            break;

        case MAKE_ITEM_ORCISH: 
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;
        }


        if (50 + item_level >= random2(250)
            || make_good_item
            || (mitm[p].sub_type == ARM_HELMET 
                && get_helmet_type(mitm[p]) == THELM_WIZARD_HAT))
        {
            mitm[p].plus += random2(3);

            if (mitm[p].sub_type <= ARM_PLATE_MAIL && 20 + item_level >= random2(300))
                mitm[p].plus += random2(3);

            if (30 + item_level >= random2(350)
                && (make_good_item
                    || (!get_equip_race(mitm[p]) == ISFLAG_ORCISH
                        || (mitm[p].sub_type <= ARM_PLATE_MAIL && coinflip()))))
            {
                switch (mitm[p].sub_type)
                {
                case ARM_SHIELD:   // shield - must do special things for this!
                case ARM_LARGE_SHIELD:
                case ARM_BUCKLER:
                {
                    const int tmp = random2(1000);

                    set_item_ego_type( mitm[p], OBJ_ARMOUR,
                                       (tmp <  40) ? SPARM_RESISTANCE :
                                       (tmp < 160) ? SPARM_FIRE_RESISTANCE :
                                       (tmp < 280) ? SPARM_COLD_RESISTANCE :
                                       (tmp < 400) ? SPARM_POISON_RESISTANCE :
                                       (tmp < 520) ? SPARM_POSITIVE_ENERGY 
                                                   : SPARM_PROTECTION );
                    break;  // prot
                    //break;
                }
                case ARM_CLOAK:
                    if (get_equip_race(mitm[p]) == ISFLAG_DWARVEN)
                        break;

                    switch (random2(4))
                    {
                    case 0:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POISON_RESISTANCE );
                        break;

                    case 1:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_DARKNESS );
                        break;
                    case 2:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        break;
                    case 3:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_PRESERVATION );
                        break;
                    }
                    break;

                case ARM_HELMET:
                    if (get_helmet_type(mitm[p]) == THELM_WIZARD_HAT && coinflip())
                    {
                        if (one_chance_in(3))
                        {
                            set_item_ego_type( mitm[p], 
                                               OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        }
                        else 
                        {
                            set_item_ego_type( mitm[p], 
                                               OBJ_ARMOUR, SPARM_INTELLIGENCE );
                        }
                    }
                    else
                    {
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_SEE_INVISIBLE
                                                      : SPARM_INTELLIGENCE );
                    }
                    break;

                case ARM_GLOVES:
                    set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                       coinflip() ? SPARM_DEXTERITY
                                                  : SPARM_STRENGTH );
                    break;

                case ARM_BOOTS:
                case ARM_NAGA_BARDING:
                case ARM_CENTAUR_BARDING:
                {
                    const int tmp = random2(600) 
                                    + 200 * (mitm[p].sub_type != ARM_BOOTS);

                    set_item_ego_type( mitm[p], OBJ_ARMOUR,
                                       (tmp < 200) ? SPARM_RUNNING :
                                       (tmp < 400) ? SPARM_LEVITATION :
                                       (tmp < 600) ? SPARM_STEALTH :
                                       (tmp < 700) ? SPARM_COLD_RESISTANCE
                                                   : SPARM_FIRE_RESISTANCE );
                    break;
                }

                case ARM_ROBE:
                    switch (random2(4))
                    {
                    case 0:
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_COLD_RESISTANCE
                                                      : SPARM_FIRE_RESISTANCE );
                        break;

                    case 1:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        break;

                    case 2:
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_POSITIVE_ENERGY
                                                      : SPARM_RESISTANCE );
                        break;
                    case 3:
                        if (force_type != OBJ_RANDOM
                            || is_random_artefact( mitm[p] )
                            || get_armour_ego_type( mitm[p] ) != SPARM_NORMAL
                            || random2(50) > 10 + item_level)
                        {
                            break;
                        }

                        set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_ARCHMAGI );
                        break;
                    }
                    break;

                default:    // other body armours:
                    set_item_ego_type( mitm[p], OBJ_ARMOUR,
                                       coinflip() ? SPARM_COLD_RESISTANCE 
                                                  : SPARM_FIRE_RESISTANCE );

                    if (one_chance_in(9))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POSITIVE_ENERGY );
                    }

                    if (one_chance_in(5))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                    }

                    if (one_chance_in(5))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POISON_RESISTANCE );
                    }

                    if (mitm[p].sub_type == ARM_PLATE_MAIL
                        && one_chance_in(15))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_PONDEROUSNESS );
                        mitm[p].plus += 3 + random2(4);
                    }
                    break;
                }
            }
        }
        else if (one_chance_in(12))
        {
            // mitm[p].plus = (coinflip() ? 99 : 98);   // 98? 99?
            do_curse_item( mitm[p] );

            if (one_chance_in(5))
                mitm[p].plus -= random2(3);

            set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_NORMAL );
        }

        // if not given a racial type, and special, give shiny/runed/etc desc.
        if (get_equip_race(mitm[p]) == 0
            && get_equip_desc(mitm[p]) == 0
            && (((is_random_artefact(mitm[p]) 
                        || get_armour_ego_type( mitm[p] ) != SPARM_NORMAL)
                    && !one_chance_in(10))
                || (mitm[p].plus != 0 && one_chance_in(3))))
        {
            switch (random2(3))
            {
            case 0:
                set_equip_desc( mitm[p], ISFLAG_GLOWING );
                break;

            case 1:
                set_equip_desc( mitm[p], ISFLAG_RUNED );
                break;

            case 2:
            default:
                set_equip_desc( mitm[p], ISFLAG_EMBROIDERED_SHINY );
                break;
            }
        }

        // Make sure you don't get a hide from acquirement (since that
        // would be an enchanted item which somehow didn't get converted
        // into armour).
        if (make_good_item)
            hide2armour(mitm[p]); // what of animal hides? {dlb}

        // skin armours + Crystal PM don't get special enchantments
        //   or species, but can be randarts
        if (mitm[p].sub_type >= ARM_DRAGON_HIDE
            && mitm[p].sub_type <= ARM_SWAMP_DRAGON_ARMOUR)
        {
            set_equip_race( mitm[p], ISFLAG_NO_RACE );
            set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_NORMAL );
        }
        break;

    case OBJ_WANDS:
        // determine sub_type:
        if (force_type != OBJ_RANDOM) 
            mitm[p].sub_type = force_type;
        else
        {
            mitm[p].sub_type = random2( NUM_WANDS );

            // Adjusted distribution here -- bwr
            // Wands used to be uniform (5.26% each)
            //
            // Now:
            // invis, hasting, healing  (1.11% each)
            // fireball, teleportaion   (3.74% each)
            // others                   (6.37% each)
            if ((mitm[p].sub_type == WAND_INVISIBILITY
                    || mitm[p].sub_type == WAND_HASTING
                    || mitm[p].sub_type == WAND_HEALING)
                || ((mitm[p].sub_type == WAND_FIREBALL
                        || mitm[p].sub_type == WAND_TELEPORTATION)
                    && coinflip()))
            {
                mitm[p].sub_type = random2( NUM_WANDS );
            }
        }

        // determine upper bound on charges:
        range_charges = ((mitm[p].sub_type == WAND_HEALING
                          || mitm[p].sub_type == WAND_HASTING
                          || mitm[p].sub_type == WAND_INVISIBILITY)   ? 8 :
                         (mitm[p].sub_type == WAND_FLAME
                          || mitm[p].sub_type == WAND_FROST
                          || mitm[p].sub_type == WAND_MAGIC_DARTS
                          || mitm[p].sub_type == WAND_RANDOM_EFFECTS) ? 28
                                                                      : 16);

        // generate charges randomly:
        mitm[p].plus = random2avg(range_charges, 3);

        // plus2 tracks how many times the player has zapped it.
        // If it is -1, then the player knows it's empty.
        // If it is -2, then the player has messed with it somehow
        // (presumably by recharging), so don't bother to display the
        // count.
        mitm[p].plus2 = 0;

        // set quantity to one:
        quant = 1;
        break;

    case OBJ_FOOD:              // this can be parsed out {dlb}
        // determine sub_type:
        if (force_type == OBJ_RANDOM)
        {
            temp_rand = random2(1000);

            mitm[p].sub_type =
                    ((temp_rand >= 750) ? FOOD_MEAT_RATION : // 25.00% chance
                     (temp_rand >= 450) ? FOOD_BREAD_RATION :// 30.00% chance
                     (temp_rand >= 350) ? FOOD_PEAR :        // 10.00% chance
                     (temp_rand >= 250) ? FOOD_APPLE :       // 10.00% chance
                     (temp_rand >= 150) ? FOOD_CHOKO :       // 10.00% chance
                     (temp_rand >= 140) ? FOOD_CHEESE :      //  1.00% chance
                     (temp_rand >= 130) ? FOOD_PIZZA :       //  1.00% chance
                     (temp_rand >= 120) ? FOOD_SNOZZCUMBER : //  1.00% chance
                     (temp_rand >= 110) ? FOOD_APRICOT :     //  1.00% chance
                     (temp_rand >= 100) ? FOOD_ORANGE :      //  1.00% chance
                     (temp_rand >=  90) ? FOOD_BANANA :      //  1.00% chance
                     (temp_rand >=  80) ? FOOD_STRAWBERRY :  //  1.00% chance
                     (temp_rand >=  70) ? FOOD_RAMBUTAN :    //  1.00% chance
                     (temp_rand >=  60) ? FOOD_LEMON :       //  1.00% chance
                     (temp_rand >=  50) ? FOOD_GRAPE :       //  1.00% chance
                     (temp_rand >=  40) ? FOOD_SULTANA :     //  1.00% chance
                     (temp_rand >=  30) ? FOOD_LYCHEE :      //  1.00% chance
                     (temp_rand >=  20) ? FOOD_BEEF_JERKY :  //  1.00% chance
                     (temp_rand >=  10) ? FOOD_SAUSAGE :     //  1.00% chance
                     (temp_rand >=   5) ? FOOD_HONEYCOMB     //  0.50% chance
                                        : FOOD_ROYAL_JELLY );//  0.50% chance
        }
        else
            mitm[p].sub_type = force_type;

        // Happens with ghoul food acquirement -- use place_chunks() outherwise
        if (mitm[p].sub_type == FOOD_CHUNK)
        {
            for (count = 0; count < 1000; count++) 
            {
                temp_rand = random2( NUM_MONSTERS );     // random monster
                temp_rand = mons_species( temp_rand ); // corpse base type

                if (mons_weight( temp_rand ) > 0)        // drops a corpse
                    break;
            }

            // set chunk flavour (default to common dungeon rat steaks):
            mitm[p].plus = (count == 1000) ? MONS_RAT : temp_rand;
            
            // set duration
            mitm[p].special = (10 + random2(11)) * 10;
        }

        // determine quantity:
        if (allow_uniques > 1)
            quant = allow_uniques;
        else
        {
            quant = 1;

            if (mitm[p].sub_type != FOOD_MEAT_RATION 
                && mitm[p].sub_type != FOOD_BREAD_RATION)
            {
                if (one_chance_in(80))
                    quant += random2(3);

                if (mitm[p].sub_type == FOOD_STRAWBERRY
                    || mitm[p].sub_type == FOOD_GRAPE
                    || mitm[p].sub_type == FOOD_SULTANA)
                {
                    quant += 3 + random2avg(15,2);
                }
            }
        }
        break;

    case OBJ_POTIONS:
        quant = 1;

        if (one_chance_in(18))
            quant++;

        if (one_chance_in(25))
            quant++;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            temp_rand = random2(9);       // general type of potion;

            switch (temp_rand)
            {
            case 0:
            case 1:
            case 2:
            case 8:
                // healing potions
                if (one_chance_in(3))
                    mitm[p].sub_type = POT_HEAL_WOUNDS;         // 14.074%
                else
                    mitm[p].sub_type = POT_HEALING;             // 28.148% 

                if (one_chance_in(20))                         
                    mitm[p].sub_type = POT_CURE_MUTATION;       //  2.222%
                break;

            case 3:
            case 4:
                // enhancements
                if (coinflip())
                    mitm[p].sub_type = POT_SPEED;               //  6.122%
                else
                    mitm[p].sub_type = POT_MIGHT;               //  6.122%

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_BERSERK_RAGE;        //  1.360%

                if (one_chance_in(5))
                    mitm[p].sub_type = POT_INVISIBILITY;        //  3.401%

                if (one_chance_in(6))                           
                    mitm[p].sub_type = POT_LEVITATION;          //  3.401%

                if (one_chance_in(8))
                    mitm[p].sub_type = POT_RESISTANCE;

                if (one_chance_in(30))                         
                    mitm[p].sub_type = POT_PORRIDGE;            //  0.704%

                if (one_chance_in(20))
                    mitm[p].sub_type = POT_BLOOD;               //  1.111%
                break;

            case 5:
                // gain ability
                mitm[p].sub_type = POT_GAIN_STRENGTH + random2(3); // 1.125%
                                                            // or 0.375% each

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_EXPERIENCE;          //  0.125%

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_MAGIC;               //  0.139%

                if (!one_chance_in(8))
                    mitm[p].sub_type = POT_RESTORE_ABILITIES;   //  9.722%

                quant = 1;
                break;

            case 6:
            case 7:
                // bad things
                switch (random2(6))
                {
                case 0:
                case 4:
                    // is this not always the case? - no, level one is 0 {dlb}
                    if (item_level > 0)
                    {
                        mitm[p].sub_type = POT_POISON;          //  6.475%

                        if (item_level > 10 && one_chance_in(4))
                            mitm[p].sub_type = POT_STRONG_POISON;

                        break;
                    }

                /* **** intentional fall through **** */     // ignored for %
                case 5:
                    if (item_level > 6)
                    {
                        mitm[p].sub_type = POT_MUTATION;        //  3.237%
                        break;
                    }

                /* **** intentional fall through **** */     // ignored for %
                case 1:
                    mitm[p].sub_type = POT_SLOWING;             //  3.237%
                    break;

                case 2:
                    mitm[p].sub_type = POT_PARALYSIS;           //  3.237%
                    break;

                case 3:
                    mitm[p].sub_type = POT_CONFUSION;           //  3.237%
                    break;

                }

                if (one_chance_in(8))
                    mitm[p].sub_type = POT_DEGENERATION;        //  2.775%

                if (one_chance_in(1000))                        //  0.022%
                    mitm[p].sub_type = POT_DECAY;

                break;
            }
        }

        mitm[p].plus = 0;
        break;

    case OBJ_SCROLLS:
        // determine sub_type:
        if (force_type == OBJ_RANDOM)
        {
            // only used in certain cases {dlb}
            int depth_mod = random2(1 + item_level);

            temp_rand = random2(920);

            mitm[p].sub_type =
                    ((temp_rand > 751) ? SCR_IDENTIFY :          // 18.26%
                     (temp_rand > 629) ? SCR_REMOVE_CURSE :      // 13.26%
                     (temp_rand > 554) ? SCR_TELEPORTATION :     //  8.15%
                     (temp_rand > 494) ? SCR_DETECT_CURSE :      //  6.52%
                     (temp_rand > 464) ? SCR_FEAR :              //  3.26%
                     (temp_rand > 434) ? SCR_NOISE :             //  3.26%
                     (temp_rand > 404) ? SCR_MAGIC_MAPPING :     //  3.26%
                     (temp_rand > 374) ? SCR_FORGETFULNESS :     //  3.26%
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
        else
            mitm[p].sub_type = force_type;

        // determine quantity:
        temp_rand = random2(48);

        quant = ((temp_rand > 1
                  || mitm[p].sub_type == SCR_VORPALISE_WEAPON
                  || mitm[p].sub_type == SCR_ENCHANT_WEAPON_III
                  || mitm[p].sub_type == SCR_ACQUIREMENT
                  || mitm[p].sub_type == SCR_TORMENT)  ? 1 :    // 95.83%
                                     (temp_rand == 0)  ? 2      //  2.08%
                                                       : 3);    //  2.08%
        mitm[p].plus = 0;
        break;

    case OBJ_JEWELLERY:
        // determine whether an unrandart will be generated {dlb}:
        if (item_level > 2 
            && you.level_type != LEVEL_ABYSS
            && you.level_type != LEVEL_PANDEMONIUM
            && random2(2000) <= 100 + (item_level * 3) 
            && one_chance_in(20))
        {
            icky = find_okay_unrandart(OBJ_JEWELLERY);

            if (icky != -1)
            {
                quant = 1;
                make_item_unrandart( mitm[p], icky );
                break;
            }
        }

        // otherwise, determine jewellery type {dlb}:
        if (force_type == OBJ_RANDOM)
        {
            mitm[p].sub_type =
                (!one_chance_in(4) ? get_random_ring_type()
                                   : get_random_amulet_type());

            // Adjusted distribution here -- bwr
            if ((mitm[p].sub_type == RING_INVISIBILITY
                    || mitm[p].sub_type == RING_REGENERATION
                    || mitm[p].sub_type == RING_TELEPORT_CONTROL
                    || mitm[p].sub_type == RING_SLAYING)
                && !one_chance_in(3))
            {
                mitm[p].sub_type = random2(24);
            }
        }
        else
            mitm[p].sub_type = force_type;

        // quantity is always one {dlb}:
        quant = 1;

        // everything begins as uncursed, unenchanted jewellery {dlb}:
        mitm[p].plus = 0;
        mitm[p].plus2 = 0;

        // set pluses for rings that require them {dlb}:
        switch (mitm[p].sub_type)
        {
        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_SLAYING:
        case RING_EVASION:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
            if (one_chance_in(5))       // 20% of such rings are cursed {dlb}
            {
                do_curse_item( mitm[p] );
                mitm[p].plus = (coinflip() ? -2 : -3);

                if (one_chance_in(3))
                    mitm[p].plus -= random2(4);
            }
            else
            {
                mitm[p].plus += 1 + (one_chance_in(3) ? random2(3)
                                                      : random2avg(6, 2));
            }
            break;

        default:
            break;
        }

        // rings of slaying also require that pluses2 be set {dlb}:
        if (mitm[p].sub_type == RING_SLAYING)
        {
            if (item_cursed( mitm[p] ) && !one_chance_in(20))
                mitm[p].plus2 = -1 - random2avg(6, 2);
            else
            {
                mitm[p].plus2 += 1 + (one_chance_in(3) ? random2(3)
                                                       : random2avg(6, 2));

                if (random2(25) < 9)        // 36% of such rings {dlb}
                {
                    // make "ring of damage"
                    do_uncurse_item( mitm[p] );
                    mitm[p].plus = 0;
                    mitm[p].plus2 += 2;
                }
            }
        }

        // All jewellery base types should now work. -- bwr
        if (allow_uniques == 1 && item_level > 2
            && random2(2000) <= 100 + (item_level * 3) && coinflip())
        {
            make_item_randart( mitm[p] );
            break;
        }

        // rings of hunger and teleportation are always cursed {dlb}:
        if (mitm[p].sub_type == RING_HUNGER
            || mitm[p].sub_type == RING_TELEPORTATION
            || one_chance_in(50))
        {
            do_curse_item( mitm[p] );
        }
        break;

    case OBJ_BOOKS:
        do
        {
            mitm[p].sub_type = random2(NUM_BOOKS);

            if (mitm[p].sub_type != BOOK_DESTRUCTION &&
                mitm[p].sub_type != BOOK_MANUAL &&
                book_rarity(mitm[p].sub_type) != 100 &&
                one_chance_in(10))
            {
                mitm[p].sub_type = coinflip() ? BOOK_WIZARDRY : BOOK_POWER;
            }

            if (!one_chance_in(100) &&
                random2(item_level+1) + 1 < book_rarity(mitm[p].sub_type))
            {
                mitm[p].sub_type = BOOK_DESTRUCTION; // continue trying
            }
        }
        while (mitm[p].sub_type == BOOK_DESTRUCTION ||
               mitm[p].sub_type == BOOK_MANUAL ||
               book_rarity(mitm[p].sub_type) == 100);

        mitm[p].special = random2(5);

        if (one_chance_in(10))
            mitm[p].special += random2(8) * 10;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;

        quant = 1;

        // tome of destruction : rare!
        if (force_type == BOOK_DESTRUCTION
            || (random2(7000) <= item_level + 20 && item_level > 10
                && force_type == OBJ_RANDOM))
        {
            mitm[p].sub_type = BOOK_DESTRUCTION;
        }

        // skill manuals - also rare
        // fixed to generate manuals for *all* extant skills - 14mar2000 {dlb}
        if (force_type == BOOK_MANUAL
            || (random2(4000) <= item_level + 20 && item_level > 6
                && force_type == OBJ_RANDOM))
        {
            mitm[p].sub_type = BOOK_MANUAL;

            if (one_chance_in(4))
            {
                mitm[p].plus = SK_SPELLCASTING
                                    + random2(NUM_SKILLS - SK_SPELLCASTING);
            }
            else
            {
                mitm[p].plus = random2(SK_UNARMED_COMBAT);

                if (mitm[p].plus == SK_UNUSED_1)
                    mitm[p].plus = SK_UNARMED_COMBAT;
            }
        }
        break;

    case OBJ_STAVES:
        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            // assumption: STAFF_SMITING is the first rod
            mitm[p].sub_type = random2(STAFF_SMITING);

            // rods are rare (10% of all staves)
            if (one_chance_in(10))
                mitm[p].sub_type = random_rod_subtype();

            // staves of energy/channeling are less common, wizardry/power
            // are more common
            if ((mitm[p].sub_type == STAFF_ENERGY 
                || mitm[p].sub_type == STAFF_CHANNELING) && one_chance_in(4))
            {
                mitm[p].sub_type = coinflip() ? STAFF_WIZARDRY : STAFF_POWER; 
            }
        }

        if (item_is_rod( mitm[p] ))
            init_rod_mp( mitm[p] );

        // add different looks
        mitm[p].special = random2(10);
        quant = 1;
        break;

    case OBJ_ORBS:              // always forced in current setup {dlb}
        quant = 1;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;

        // I think we only have one type of orb now, so ... {dlb}
        set_unique_item_status( OBJ_ORBS, mitm[p].sub_type, UNIQ_EXISTS );
        break;

    case OBJ_MISCELLANY:
        if (force_type == OBJ_RANDOM)
        {
            do
            {
                mitm[p].sub_type = random2(NUM_MISCELLANY);
            }
            while
                //mv: never generated
               ((mitm[p].sub_type == MISC_RUNE_OF_ZOT)
                || (mitm[p].sub_type == MISC_HORN_OF_GERYON)
                || (mitm[p].sub_type == MISC_DECK_OF_PUNISHMENT)
                // pure decks are rare in the dungeon
                || ((mitm[p].sub_type == MISC_DECK_OF_ESCAPE ||
                     mitm[p].sub_type == MISC_DECK_OF_DESTRUCTION ||
                     mitm[p].sub_type == MISC_DECK_OF_DUNGEONS ||
                     mitm[p].sub_type == MISC_DECK_OF_SUMMONING ||
                     mitm[p].sub_type == MISC_DECK_OF_WONDERS) &&
                    !one_chance_in(5)));

            // filling those silly empty boxes -- bwr
            if (mitm[p].sub_type == MISC_EMPTY_EBONY_CASKET 
                && !one_chance_in(20))
            {
                mitm[p].sub_type = MISC_BOX_OF_BEASTS;
            }
        }
        else
        {
            mitm[p].sub_type = force_type;
        }

        if ( is_deck(mitm[p]) )
        {
            mitm[p].plus = 4 + random2(10);

            mitm[p].special = DECK_RARITY_COMMON;
            if ( one_chance_in(10) )
                mitm[p].special = DECK_RARITY_LEGENDARY;
            if ( one_chance_in(5) )
                mitm[p].special = DECK_RARITY_RARE;

            init_deck(mitm[p]);
        }

        if (mitm[p].sub_type == MISC_RUNE_OF_ZOT)
            mitm[p].plus = item_race;

        quant = 1;
        break;

    // that is, everything turns to gold if not enumerated above, so ... {dlb}
    default:
        mitm[p].base_type = OBJ_GOLD;

        // Acquirement now gives more gold: The base price of a scroll
        // of acquirement is 520 gold. The expected value of the gold
        // it produces is about 480. So you cannot consistently make a
        // profit by buying scrolls of acquirement. However, there is
        // a very low chance you'll get lucky and receive up to 2296!
        // This is quite rare: 50% of the time you'll get less than
        // 360 gold and 90% of the time you'll get less than 900 and
        // 99% of the time you'll get less than 1500. --Zooko
        if (make_good_item)
            quant = 150 + random2(150) + random2(random2(random2(2000)));
        else
            quant = 1 + random2avg(19, 2) + random2(item_level);
        break;
    }

    mitm[p].quantity = quant;

    if (dont_place)
    {
        mitm[p].x = 0;
        mitm[p].y = 0;
        mitm[p].link = NON_ITEM;
    }
    else
    {
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

    item_colour( mitm[p] );

    // Okay, this check should be redundant since the purpose of 
    // this function is to create valid items.  Still, we're adding 
    // this safety for fear that a report of Trog giving a nonexistent 
    // item might symbolize something more serious. -- bwr
    return (is_valid_item( mitm[p] ) ? p : NON_ITEM);
}                               // end items()

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
    const bool visibly_branded =
        brand != SPWPN_NORMAL && brand != SPWPN_DISTORTION;

    return (((is_random_artefact(item) || visibly_branded)
             && !one_chance_in(10))
            || ((item.plus != 0 || item.plus2 != 0)
                && one_chance_in(3)
                && brand != SPWPN_DISTORTION))
        && item.sub_type != WPN_CLUB
        && item.sub_type != WPN_GIANT_CLUB
        && item.sub_type != WPN_GIANT_SPIKED_CLUB
        && get_equip_desc(item) == 0
        && get_equip_race(item) == 0;
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
    
    if (get_weapon_brand(mthing) == SPWPN_DISRUPTION
        && mholy == MH_UNDEAD)
    {
        set_item_ego_type( mthing, OBJ_WEAPONS, SPWPN_NORMAL );
    }
    else if (get_weapon_brand(mthing) == SPWPN_HOLY_WRATH
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
        else if (random2(5) < 2)        // give darts
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_MISSILES;
            item.sub_type = MI_DART;
            iquan = 1 + random2(5);
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
                             (temp_rand >  2) ? WPN_LOCHABER_AXE :  //  4%
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

        int temp_rand = random2(3);
        set_equip_desc( item, (temp_rand == 1) ? ISFLAG_GLOWING :
                                  (temp_rand == 2) ? ISFLAG_RUNED 
                                                   : ISFLAG_NO_DESC );

        if (one_chance_in(3))
            set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );
        else if (one_chance_in(3))
        {
            temp_rand = random2(5);

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
    }                           // end "switch(mon->type)"

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

    const item_def &i = mitm[thing_created];
    if (melee_only && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
    {
        destroy_item(thing_created);
        return (item_race);
    }
    
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

        const int thing_created =
            items( 0, xitc, xitt, true, level, item_race );
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
    case MONS_ORC_WIZARD:
    case MONS_WIZARD:
    case MONS_BLORK_THE_ORC:
        item_race = MAKE_ITEM_NO_RACE;
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
    }                           // end of switch(menv [mid].type)

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
        armtype = random2(8);

    if (10 + item_level >= random2(400) && one_chance_in(20))
        armtype = ARM_DRAGON_HIDE + random2(7);

    if (10 + item_level >= random2(500) && one_chance_in(20))
    {
        armtype = ARM_STEAM_DRAGON_HIDE + random2(11);

        if (armtype == ARM_ANIMAL_SKIN && one_chance_in(20))
            armtype = ARM_CRYSTAL_PLATE_MAIL;
    }

    // secondary armours:
    if (one_chance_in(5))
    {
        armtype = ARM_SHIELD + random2(5);

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

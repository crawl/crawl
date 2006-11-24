/*
 *  File:       misc.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <3>   11/14/99      cdl    evade with random40(ev) vice random2(ev)
 *   <2>    5/20/99      BWR    Multi-user support, new berserk code.
 *   <1>    -/--/--      LRH    Created
 */


#include "AppHdr.h"
#include "misc.h"
#include "notes.h"

#include <string.h>
#if !defined(__IBMCPP__)
#include <unistd.h>
#endif

#ifdef __MINGW32__
#include <io.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "cloud.h"
#include "delay.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "it_use2.h"
#include "items.h"
#include "itemname.h"
#include "lev-pand.h"
#include "macro.h"
#include "monplace.h"
#include "mon-util.h"
#include "monstuff.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells3.h"
#include "spl-cast.h"
#include "stuff.h"
#include "transfor.h"
#include "travel.h"
#include "view.h"

extern FixedVector<char, 10>  Visible_Statue;        // defined in acr.cc

bool scramble(void);
bool trap_item(char base_type, char sub_type, char beam_x, char beam_y);
static void dart_trap(bool trap_known, int trapped, struct bolt &pbolt, bool poison);

// void place_chunks(int mcls, unsigned char rot_status, unsigned char chx,
//                   unsigned char chy, unsigned char ch_col)
void turn_corpse_into_chunks( item_def &item )
{
    const int mons_class = item.plus; 
    const int max_chunks = mons_weight( mons_class ) / 150;

    ASSERT( item.base_type == OBJ_CORPSES );

    item.base_type = OBJ_FOOD;
    item.sub_type = FOOD_CHUNK;
    item.quantity = 1 + random2( max_chunks );

    item.quantity = stepdown_value( item.quantity, 4, 4, 12, 12 );

    // seems to me that this should come about only
    // after the corpse has been butchered ... {dlb}
    if (monster_descriptor( mons_class, MDSC_LEAVES_HIDE ) && !one_chance_in(3))
    {
        int o = get_item_slot( 100 + random2(200) );
        if (o == NON_ITEM)
            return;

        mitm[o].quantity = 1;

        // these values are common to all: {dlb}
        mitm[o].base_type = OBJ_ARMOUR;
        mitm[o].plus = 0;
        mitm[o].plus2 = 0;
        mitm[o].special = 0;
        mitm[o].flags = 0;
        mitm[o].colour = mons_class_colour( mons_class );

        // these values cannot be set by a reasonable formula: {dlb}
        switch (mons_class)
        {
        case MONS_DRAGON:
            mitm[o].sub_type = ARM_DRAGON_HIDE;
            break;
        case MONS_TROLL:
            mitm[o].sub_type = ARM_TROLL_HIDE;
            break;
        case MONS_ICE_DRAGON:
            mitm[o].sub_type = ARM_ICE_DRAGON_HIDE;
            break;
        case MONS_STEAM_DRAGON:
            mitm[o].sub_type = ARM_STEAM_DRAGON_HIDE;
            break;
        case MONS_MOTTLED_DRAGON:
            mitm[o].sub_type = ARM_MOTTLED_DRAGON_HIDE;
            break;
        case MONS_STORM_DRAGON:
            mitm[o].sub_type = ARM_STORM_DRAGON_HIDE;
            break;
        case MONS_GOLDEN_DRAGON:
            mitm[o].sub_type = ARM_GOLD_DRAGON_HIDE;
            break;
        case MONS_SWAMP_DRAGON:
            mitm[o].sub_type = ARM_SWAMP_DRAGON_HIDE;
            break;
        default:
            // future implementation {dlb}
            mitm[o].sub_type = ARM_ANIMAL_SKIN;
            break;
        }

        move_item_to_grid( &o, item.x, item.y ); 
    }
}                               // end place_chunks()

bool grid_is_wall( int grid )
{
    return (grid == DNGN_ROCK_WALL
            || grid == DNGN_STONE_WALL
            || grid == DNGN_METAL_WALL
            || grid == DNGN_GREEN_CRYSTAL_WALL
            || grid == DNGN_WAX_WALL
            || grid == DNGN_PERMAROCK_WALL);
}

bool grid_is_opaque( int grid )
{
    return (grid < MINSEE && grid != DNGN_ORCISH_IDOL);
}

bool grid_is_solid( int grid )
{
    return (grid < MINMOVE);
}

bool grid_is_water( int grid )
{
    return (grid == DNGN_SHALLOW_WATER || grid == DNGN_DEEP_WATER);
}

bool grid_is_watery( int grid )
{
    return (grid_is_water(grid) || grid == DNGN_BLUE_FOUNTAIN);
}

bool grid_destroys_items( int grid )
{
    return (grid == DNGN_LAVA || grid == DNGN_DEEP_WATER);
}

// returns 0 is grid is not an altar, else it returns the GOD_* type
god_type grid_altar_god( unsigned char grid )
{
    if (grid >= DNGN_ALTAR_ZIN && grid <= DNGN_ALTAR_ELYVILON)
        return (static_cast<god_type>( grid - DNGN_ALTAR_ZIN + 1 ));

    return (GOD_NO_GOD);
}

bool grid_is_branch_stairs( unsigned char grid )
{
    return ((grid >= DNGN_ENTER_ORCISH_MINES && grid <= DNGN_ENTER_RESERVED_4)
            || (grid >= DNGN_ENTER_DIS && grid <= DNGN_ENTER_TARTARUS));
}

int grid_secret_door_appearance( int gx, int gy )
{
    int ret = DNGN_FLOOR;

    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            // only considering orthogonal grids
            if ((abs(dx) + abs(dy)) % 2 == 0)
                continue;

            const int targ = grd[gx + dx][gy + dy];

            if (!grid_is_wall( targ ))
                continue;

            if (ret == DNGN_FLOOR)
                ret = targ;
            else if (ret != targ)
                ret = ((ret < targ) ? ret : targ);
        }
    }

    return ((ret == DNGN_FLOOR) ? DNGN_ROCK_WALL 
                                : ret);
}

const char *grid_item_destruction_message( unsigned char grid )
{
    return grid == DNGN_DEEP_WATER? "You hear a splash."
         : grid == DNGN_LAVA      ? "You hear a sizzling splash."
         :                          "You hear an empty echo.";
}

void search_around(void)
{
    char srx = 0;
    char sry = 0;
    int i;

    for (srx = you.x_pos - 1; srx < you.x_pos + 2; srx++)
    {
        for (sry = you.y_pos - 1; sry < you.y_pos + 2; sry++)
        {
            // don't exclude own square; may be levitating
            if (grd[srx][sry] == DNGN_SECRET_DOOR
                && random2(17) <= 1 + you.skills[SK_TRAPS_DOORS])
            {
                grd[srx][sry] = DNGN_CLOSED_DOOR;
                mpr("You found a secret door!");
                exercise(SK_TRAPS_DOORS, ((coinflip())? 2 : 1));
            }

            if (grd[srx][sry] == DNGN_UNDISCOVERED_TRAP
                && random2(17) <= 1 + you.skills[SK_TRAPS_DOORS])
            {
                i = trap_at_xy(srx, sry);

                if (i != -1)
                    grd[srx][sry] = trap_category(env.trap[i].type);

                mpr("You found a trap!");
            }
        }
    }

    return;
}                               // end search_around()

void in_a_cloud(void)
{
    int cl = env.cgrid[you.x_pos][you.y_pos];
    int hurted = 0;
    int resist;

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        mpr("Your icy shield dissipates!", MSGCH_DURATION);
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = 1;
    }

    switch (env.cloud[cl].type)
    {
    case CLOUD_FIRE:
    case CLOUD_FIRE_MON:
        if (you.fire_shield)
            return;

        mpr("You are engulfed in roaring flames!");

        resist = player_res_fire();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(player_AC());

            if (hurted < 1)
                hurted = 0;
            else
                ouch( hurted, cl, KILLED_BY_CLOUD, "flame" );
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= (1 + resist * resist);
            ouch( hurted, cl, KILLED_BY_CLOUD, "flame" );
        }
        expose_player_to_element(BEAM_FIRE, 7);
        break;

    case CLOUD_STINK:
    case CLOUD_STINK_MON:
        // If you don't have to breathe, unaffected
        mpr("You are engulfed in noxious fumes!");
        if (player_res_poison())
            break;

        hurted += (random2(3) * you.time_taken) / 10;
        if (hurted < 1)
            hurted = 0;
        else
            ouch( (hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD, 
                    "noxious fumes" );

        if (1 + random2(27) >= you.experience_level)
        {
            mpr("You choke on the stench!");
            confuse_player( (coinflip() ? 3 : 2) );
        }
        break;

    case CLOUD_COLD:
    case CLOUD_COLD_MON:
        mpr("You are engulfed in freezing vapours!");

        resist = player_res_cold();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(player_AC());
            if (hurted < 0)
                hurted = 0;

            ouch( (hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD, 
                  "freezing vapour" );
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= (1 + resist * resist);
            ouch( hurted, cl, KILLED_BY_CLOUD, "freezing vapour" );
        }
        expose_player_to_element(BEAM_COLD, 7);
        break;

    case CLOUD_POISON:
    case CLOUD_POISON_MON:
        // If you don't have to breathe, unaffected
        mpr("You are engulfed in poison gas!");
        if (!player_res_poison())
        {
            ouch( (random2(10) * you.time_taken) / 10, cl, KILLED_BY_CLOUD, 
                  "poison gas" );
            poison_player(1);
        }
        break;

    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_PURP_SMOKE:
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE_MON:
    case CLOUD_BLUE_SMOKE_MON:
    case CLOUD_PURP_SMOKE_MON:
    case CLOUD_BLACK_SMOKE_MON:
        mpr("You are engulfed in a cloud of smoke!");
        break;

    case CLOUD_STEAM:
    case CLOUD_STEAM_MON:
        mpr("You are engulfed in a cloud of scalding steam!");
        if (player_res_steam() > 0)
        {
            mpr("It doesn't seem to affect you.");
            return;
        }

        hurted += (random2(6) * you.time_taken) / 10;
        if (hurted < 0 || player_res_fire() > 0)
            hurted = 0;

        ouch( (hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD, "poison gas" );
        break;

    case CLOUD_MIASMA:
    case CLOUD_MIASMA_MON:
        mpr("You are engulfed in a dark miasma.");

        if (player_prot_life() > random2(3))
            return;

        poison_player(1);

        hurted += (random2avg(12, 3) * you.time_taken) / 10;    // 3

        if (hurted < 0)
            hurted = 0;

        ouch( hurted, cl, KILLED_BY_CLOUD, "foul pestilence" );
        potion_effect(POT_SLOWING, 5);

        if (you.hp_max > 4 && coinflip())
            rot_hp(1);

        break;
    }

    return;
}                               // end in_a_cloud()

void curare_hits_player(int agent, int degree)
{
    const bool res_poison = player_res_poison();

    poison_player(degree);

    if (!player_res_asphyx())
    {
        int hurted = roll_dice(2, 6);
        // Note that the hurtage is halved by poison resistance.
        if (res_poison)
            hurted /= 2;

        if (hurted)
        {
            mpr("You feel difficulty breathing.");
            ouch( hurted, agent, KILLED_BY_CURARE, "curare-induced apnoea" );
        }
        potion_effect(POT_SLOWING, 2 + random2(4 + degree));
    }
}

void merfolk_start_swimming(void)
{
    FixedVector < char, 8 > removed;

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform();

    for (int i = EQ_WEAPON; i < EQ_RIGHT_RING; i++)
    {
        removed[i] = 0;
    }

    if (you.equip[EQ_BOOTS] != -1)
        removed[EQ_BOOTS] = 1;

    // Perhaps a bit to easy for the player, but we allow merfolk
    // to slide out of heavy body armour freely when entering water,
    // rather than handling emcumbered swimming. -- bwr
    if (!player_light_armour())
    {
        // Can't slide out of just the body armour, cloak comes off -- bwr
        if (you.equip[EQ_CLOAK])
            removed[EQ_CLOAK] = 1;

        removed[EQ_BODY_ARMOUR] = 1;
    }

    remove_equipment(removed);
}

void up_stairs(void)
{
    unsigned char stair_find = grd[you.x_pos][you.y_pos];
    char old_where = you.where_are_you;
    bool was_a_labyrinth = false;

    if (stair_find == DNGN_ENTER_SHOP)
    {
        shop();
        return;
    }

    // probably still need this check here (teleportation) -- bwr
    if ((stair_find < DNGN_STONE_STAIRS_UP_I
            || stair_find > DNGN_ROCK_STAIRS_UP)
        && (stair_find < DNGN_RETURN_FROM_ORCISH_MINES || stair_find >= 150))
    {
        mpr("You can't go up here.");
        return;
    }

    // Since the overloaded message set turn_is_over, I'm assuming that
    // the overloaded character makes an attempt... so we're doing this
    // check before that one. -- bwr
    if (!player_is_levitating()
        && you.conf 
        && (stair_find >= DNGN_STONE_STAIRS_UP_I 
            && stair_find <= DNGN_ROCK_STAIRS_UP)
        && random2(100) > you.dex)
    {
        mpr("In your confused state, you trip and fall back down the stairs.");

        ouch( roll_dice( 3 + you.burden_state, 5 ), 0, 
              KILLED_BY_FALLING_DOWN_STAIRS );

        you.turn_is_over = true;
        return;
    }

    if (you.burden_state == BS_OVERLOADED)
    {
        mpr("You are carrying too much to climb upwards.");
        you.turn_is_over = true;
        return;
    }

    if (you.your_level == 0
          && !yesno("Are you sure you want to leave the Dungeon?", false, 'n'))
    {
        mpr("Alright, then stay!");
        return;
    }

    unsigned char old_level  = you.your_level;

    // Interlevel travel data:
    bool collect_travel_data = you.level_type != LEVEL_LABYRINTH 
        	    && you.level_type != LEVEL_ABYSS 
                && you.level_type != LEVEL_PANDEMONIUM;

    level_id  old_level_id    = level_id::get_current_level_id();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);
    int stair_x = you.x_pos, stair_y = you.y_pos;
    if (collect_travel_data)
        old_level_info.update();

    // Make sure we return to our main dungeon level... labyrinth entrances
    // in the abyss or pandemonium a bit trouble (well the labyrinth does
    // provide a way out of those places, its really not that bad I suppose)
    if (you.level_type == LEVEL_LABYRINTH)
    {
        you.level_type = LEVEL_DUNGEON;
        was_a_labyrinth = true;
    }

    you.your_level--;

    int i = 0;

    if (you.your_level < 0)
    {
        mpr("You have escaped!");

        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] ) 
                && you.inv[i].base_type == OBJ_ORBS)
            {
                ouch(-9999, 0, KILLED_BY_WINNING);
            }
        }

        ouch(-9999, 0, KILLED_BY_LEAVING);
    }

    mpr("Entering...");
    you.prev_targ = MHITNOT;
    you.pet_target = MHITNOT;

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        mpr("Thank you for visiting Hell. Please come again soon.");
        you.where_are_you = BRANCH_MAIN_DUNGEON;
        you.your_level = you.hell_exit;
        stair_find = DNGN_STONE_STAIRS_UP_I;
    }

    if (player_in_hell())
    {
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.your_level = 27;
    }

    switch (stair_find)
    {
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_ZOT:
        mpr("Welcome back to the Dungeon!");
        you.where_are_you = BRANCH_MAIN_DUNGEON;
        break;
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_SWAMP:
        mpr("Welcome back to the Lair of Beasts!");
        you.where_are_you = BRANCH_LAIR;
        break;
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
        mpr("Welcome back to the Vaults!");
        you.where_are_you = BRANCH_VAULTS;
        break;
    case DNGN_RETURN_FROM_TOMB:
        mpr("Welcome back to the Crypt!");
        you.where_are_you = BRANCH_CRYPT;
        break;
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        mpr("Welcome back to the Orcish Mines!");
        you.where_are_you = BRANCH_ORCISH_MINES;
        break;
    }

    unsigned char stair_taken = stair_find;

    if (player_is_levitating())
    {
        if (you.duration[DUR_CONTROLLED_FLIGHT])
            mpr("You fly upwards.");
        else
            mpr("You float upwards... And bob straight up to the ceiling!");
    }
    else
        mpr("You climb upwards.");

    load(stair_taken, LOAD_ENTER_LEVEL, was_a_labyrinth, old_level, old_where);

    you.turn_is_over = true;

    save_game(false);

    new_level();

    viewwindow(1, true);

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    // Tell the travel code that we're now on a new level
    travel_init_new_level();
    if (collect_travel_data)
    {
        // Update stair information for the stairs we just ascended, and the
        // down stairs we're currently on.
        level_id  new_level_id    = level_id::get_current_level_id();

        if (you.level_type != LEVEL_PANDEMONIUM &&
                you.level_type != LEVEL_ABYSS &&
                you.level_type != LEVEL_LABYRINTH)
        {
            LevelInfo &new_level_info = 
                        travel_cache.get_level_info(new_level_id);
            new_level_info.update();

            // First we update the old level's stair.
            level_pos lp;
            lp.id  = new_level_id;
            lp.pos.x = you.x_pos; 
            lp.pos.y = you.y_pos;

            bool guess = false;
            // Ugly hack warning:
            // The stairs in the Vestibule of Hell exhibit special behaviour:
            // they always lead back to the dungeon level that the player
            // entered the Vestibule from. This means that we need to pretend
            // we don't know where the upstairs from the Vestibule go each time
            // we take it. If we don't, interlevel travel may try to use portals
            // to Hell as shortcuts between dungeon levels, which won't work,
            // and will confuse the dickens out of the player (well, it confused
            // the dickens out of me when it happened).
            if (new_level_id.branch == BRANCH_MAIN_DUNGEON &&
                    old_level_id.branch == BRANCH_VESTIBULE_OF_HELL)
            {
                lp.id.depth = -1;
                lp.pos.x = lp.pos.y = -1;
                guess = true;
            }

            old_level_info.update_stair(stair_x, stair_y, lp, guess);

            // We *guess* that going up a staircase lands us on a downstair,
            // and that we can descend that downstair and get back to where we
            // came from. This assumption is guaranteed false when climbing out
            // of one of the branches of Hell.
            if (new_level_id.branch != BRANCH_VESTIBULE_OF_HELL)
            {
                // Set the new level's stair, assuming arbitrarily that going
                // downstairs will land you on the same upstairs you took to
                // begin with (not necessarily true).
                lp.id = old_level_id;
                lp.pos.x = stair_x;
                lp.pos.y = stair_y;
                new_level_info.update_stair(you.x_pos, you.y_pos, lp, true);
            }
        }
    }
}                               // end up_stairs()

void down_stairs( bool remove_stairs, int old_level, bool force )
{
    int i;
    char old_level_type = you.level_type;
    bool was_a_labyrinth = false;
    const unsigned char stair_find = grd[you.x_pos][you.y_pos];

    //int old_level = you.your_level;
    bool leave_abyss_pan = false;
    char old_where = you.where_are_you;

#ifdef SHUT_LABYRINTH
    if (stair_find == DNGN_ENTER_LABYRINTH)
    {
        mpr("Sorry, this section of the dungeon is closed for fumigation.");
        mpr("Try again next release.");
        return;
    }
#endif

    // probably still need this check here (teleportation) -- bwr
    if ((stair_find < DNGN_ENTER_LABYRINTH
            || stair_find > DNGN_ROCK_STAIRS_DOWN)
        && stair_find != DNGN_ENTER_HELL
        && ((stair_find < DNGN_ENTER_DIS
                || stair_find > DNGN_TRANSIT_PANDEMONIUM)
            && stair_find != DNGN_STONE_ARCH)
        && !(stair_find >= DNGN_ENTER_ORCISH_MINES
            && stair_find < DNGN_RETURN_FROM_ORCISH_MINES))
    {
        mpr( "You can't go down here!" );
        return;
    }

    if (stair_find >= DNGN_ENTER_LABYRINTH
        && stair_find <= DNGN_ROCK_STAIRS_DOWN
        && player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        mpr("A mysterious force prevents you from descending the staircase.");
        return;
    }                           /* down stairs in vestibule are one-way */

    if (stair_find == DNGN_STONE_ARCH)
    {
        mpr("You can't go down here!");
        return;
    }

    if (!force && player_is_levitating() 
            && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
    {
        mpr("You're floating high up above the floor!");
        return;
    }

    if (stair_find == DNGN_ENTER_ZOT)
    {
        int num_runes = 0;

        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] )
                && you.inv[i].base_type == OBJ_MISCELLANY
                && you.inv[i].sub_type == MISC_RUNE_OF_ZOT)
            {
                num_runes += you.inv[i].quantity;
            }
        }

        if (num_runes < NUMBER_OF_RUNES_NEEDED)
        {
            switch (NUMBER_OF_RUNES_NEEDED)
            {
            case 1:
                mpr("You need a Rune to enter this place.");
                break;

            default:
                snprintf( info, INFO_SIZE, 
                          "You need at least %d Runes to enter this place.",
                          NUMBER_OF_RUNES_NEEDED );

                mpr(info);
            }
            return;
        }
    }

    // Interlevel travel data:
    bool collect_travel_data = you.level_type != LEVEL_LABYRINTH 
        	    && you.level_type != LEVEL_ABYSS
                && you.level_type != LEVEL_PANDEMONIUM;

    level_id  old_level_id    = level_id::get_current_level_id();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);
    int stair_x = you.x_pos, stair_y = you.y_pos;
    if (collect_travel_data)
        old_level_info.update();

    if (you.level_type == LEVEL_PANDEMONIUM
            && stair_find == DNGN_TRANSIT_PANDEMONIUM)
    {
        was_a_labyrinth = true;
    }
    else
    {
        if (you.level_type != LEVEL_DUNGEON)
            was_a_labyrinth = true;

        you.level_type = LEVEL_DUNGEON;
    }

    mpr("Entering...");
    you.prev_targ = MHITNOT;
    you.pet_target = MHITNOT;

    if (stair_find == DNGN_ENTER_HELL)
    {
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.hell_exit = you.your_level;

        mpr("Welcome to Hell!");
        mpr("Please enjoy your stay.");

        // Kill -more- prompt if we're traveling.
        if (!you.running)
            more();

        you.your_level = 26;    // = 59;
    }

    if ((stair_find >= DNGN_ENTER_DIS 
            && stair_find <= DNGN_ENTER_TARTARUS)
        || (stair_find >= DNGN_ENTER_ORCISH_MINES 
            && stair_find < DNGN_RETURN_FROM_ORCISH_MINES))
    {
        // no idea why such a huge switch and not 100-grd[][]
        // planning ahead for reorganizing grd[][] values - 13jan2000 {dlb}
        strcpy( info, "Welcome to " );
        switch (stair_find)
        {
        case DNGN_ENTER_DIS:
            strcat(info, "the Iron City of Dis!");
            you.where_are_you = BRANCH_DIS;
            you.your_level = 26;
            break;
        case DNGN_ENTER_GEHENNA:
            strcat(info, "Gehenna!");
            you.where_are_you = BRANCH_GEHENNA;
            you.your_level = 26;
            break;
        case DNGN_ENTER_COCYTUS:
            strcat(info, "Cocytus!");
            you.where_are_you = BRANCH_COCYTUS;
            you.your_level = 26;
            break;
        case DNGN_ENTER_TARTARUS:
            strcat(info, "Tartarus!");
            you.where_are_you = BRANCH_TARTARUS;
            you.your_level = 26;
            break;
        case DNGN_ENTER_ORCISH_MINES:
            strcat(info, "the Orcish Mines!");
            you.where_are_you = BRANCH_ORCISH_MINES;
            break;
        case DNGN_ENTER_HIVE:
            strcpy(info, "You hear a buzzing sound coming from all directions.");
            you.where_are_you = BRANCH_HIVE;
            break;
        case DNGN_ENTER_LAIR:
            strcat(info, "the Lair of Beasts!");
            you.where_are_you = BRANCH_LAIR;
            break;
        case DNGN_ENTER_SLIME_PITS:
            strcat(info, "the Pits of Slime!");
            you.where_are_you = BRANCH_SLIME_PITS;
            break;
        case DNGN_ENTER_VAULTS:
            strcat(info, "the Vaults!");
            you.where_are_you = BRANCH_VAULTS;
            break;
        case DNGN_ENTER_CRYPT:
            strcat(info, "the Crypt!");
            you.where_are_you = BRANCH_CRYPT;
            break;
        case DNGN_ENTER_HALL_OF_BLADES:
            strcat(info, "the Hall of Blades!");
            you.where_are_you = BRANCH_HALL_OF_BLADES;
            break;
        case DNGN_ENTER_ZOT:
            strcat(info, "the Hall of Zot!");
            you.where_are_you = BRANCH_HALL_OF_ZOT;
            break;
        case DNGN_ENTER_TEMPLE:
            strcat(info, "the Ecumenical Temple!");
            you.where_are_you = BRANCH_ECUMENICAL_TEMPLE;
            break;
        case DNGN_ENTER_SNAKE_PIT:
            strcat(info, "the Snake Pit!");
            you.where_are_you = BRANCH_SNAKE_PIT;
            break;
        case DNGN_ENTER_ELVEN_HALLS:
            strcat(info, "the Elven Halls!");
            you.where_are_you = BRANCH_ELVEN_HALLS;
            break;
        case DNGN_ENTER_TOMB:
            strcat(info, "the Tomb!");
            you.where_are_you = BRANCH_TOMB;
            break;
        case DNGN_ENTER_SWAMP:
            strcat(info, "the Swamp!");
            you.where_are_you = BRANCH_SWAMP;
            break;
        }

        mpr(info);
    }
    else if (stair_find == DNGN_ENTER_LABYRINTH)
    {
        you.level_type = LEVEL_LABYRINTH;
        grd[you.x_pos][you.y_pos] = DNGN_FLOOR;
    }
    else if (stair_find == DNGN_ENTER_ABYSS)
    {
        you.level_type = LEVEL_ABYSS;
    }
    else if (stair_find == DNGN_ENTER_PANDEMONIUM)
    {
        you.level_type = LEVEL_PANDEMONIUM;
    }

    // When going downstairs into a special level, delete any previous
    // instances of it
    if (you.level_type == LEVEL_LABYRINTH ||
        you.level_type == LEVEL_ABYSS ||
        you.level_type == LEVEL_PANDEMONIUM)
    {
        std::string lname = make_filename(you.your_name, you.your_level,
                                          you.where_are_you,
                                          true, false );
#if DEBUG_DIAGNOSTICS
	snprintf( info, INFO_SIZE, "Deleting: %s", lname.c_str() );
        mpr( info, MSGCH_DIAGNOSTICS );
        more();
#endif
        unlink(lname.c_str());
    }

    if (stair_find == DNGN_EXIT_ABYSS || stair_find == DNGN_EXIT_PANDEMONIUM)
    {
        leave_abyss_pan = true;
        mpr("You pass through the gate, and find yourself at the top of a staircase.");
        more();
    }

    if (!player_is_levitating()
        && you.conf 
        && (stair_find >= DNGN_STONE_STAIRS_DOWN_I 
            && stair_find <= DNGN_ROCK_STAIRS_DOWN)
        && random2(100) > you.dex)
    {
        mpr("In your confused state, you trip and fall down the stairs.");

        // Nastier than when climbing stairs, but you'll aways get to 
        // your destination, -- bwr
        ouch( roll_dice( 6 + you.burden_state, 10 ), 0, 
              KILLED_BY_FALLING_DOWN_STAIRS );
    }

    if (you.level_type == LEVEL_DUNGEON)
        you.your_level++;

    int stair_taken = stair_find;

    //unsigned char save_old = 1;

    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        stair_taken = DNGN_FLOOR;       //81;

    if (you.level_type == LEVEL_PANDEMONIUM)
        stair_taken = DNGN_TRANSIT_PANDEMONIUM;

    if (remove_stairs)
        grd[you.x_pos][you.y_pos] = DNGN_FLOOR;

    switch (you.level_type)
    {
    case LEVEL_LABYRINTH:
        mpr("You enter a dark and forbidding labyrinth.");
        break;

    case LEVEL_ABYSS:
        mpr("You enter the Abyss!");
        mpr("To return, you must find a gate leading back.");
        break;

    case LEVEL_PANDEMONIUM:
        if (old_level_type == LEVEL_PANDEMONIUM)
            mpr("You pass into a different region of Pandemonium.");
        else
        {
            mpr("You enter the halls of Pandemonium!");
            mpr("To return, you must find a gate leading back.");
        }
        break;

    default:
        mpr("You climb downwards.");
        break;
    }

    load(stair_taken, LOAD_ENTER_LEVEL, was_a_labyrinth, old_level, old_where);

    unsigned char pc = 0;
    unsigned char pt = random2avg(28, 3);

    switch (you.level_type)
    {
    case LEVEL_LABYRINTH:
        you.your_level++;
        break;

    case LEVEL_ABYSS:
        grd[you.x_pos][you.y_pos] = DNGN_FLOOR;

        if (old_level_type != LEVEL_PANDEMONIUM)
            you.your_level--;   // Linley-suggested addition 17jan2000 {dlb}

        init_pandemonium();     /* colours only */

        if (player_in_hell())
        {
            you.where_are_you = BRANCH_MAIN_DUNGEON;
            you.your_level = you.hell_exit - 1;
        }
        break;

    case LEVEL_PANDEMONIUM:
        if (old_level_type == LEVEL_PANDEMONIUM)
        {
            init_pandemonium();
            for (pc = 0; pc < pt; pc++)
                pandemonium_mons();
        }
        else
        {
            // Linley-suggested addition 17jan2000 {dlb}
            if (old_level_type != LEVEL_ABYSS)
                you.your_level--;

            init_pandemonium();

            for (pc = 0; pc < pt; pc++)
                pandemonium_mons();

            if (player_in_hell())
            {
                you.where_are_you = BRANCH_MAIN_DUNGEON;
                you.hell_exit = 26;
                you.your_level = 26;
            }
        }
        break;

    default:
        break;
    }

    you.turn_is_over = true;

    save_game(false);

    new_level();

    viewwindow(1, true);

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    travel_init_new_level();
    if (collect_travel_data)
    {
        // Update stair information for the stairs we just descended, and the
        // upstairs we're currently on.
        level_id  new_level_id    = level_id::get_current_level_id();

        if (you.level_type != LEVEL_PANDEMONIUM &&
                you.level_type != LEVEL_ABYSS &&
                you.level_type != LEVEL_LABYRINTH)
        {
            LevelInfo &new_level_info = 
                            travel_cache.get_level_info(new_level_id);
            new_level_info.update();

            // First we update the old level's stair.
            level_pos lp;
            lp.id  = new_level_id;
            lp.pos.x = you.x_pos; 
            lp.pos.y = you.y_pos;

            old_level_info.update_stair(stair_x, stair_y, lp);

            // Then the new level's stair, assuming arbitrarily that going
            // upstairs will land you on the same downstairs you took to begin 
            // with (not necessarily true).
            lp.id = old_level_id;
            lp.pos.x = stair_x;
            lp.pos.y = stair_y;
            new_level_info.update_stair(you.x_pos, you.y_pos, lp, true);
        }
    }
}                               // end down_stairs()

void new_level(void)
{
    int curr_subdungeon_level = you.your_level + 1;

    textcolor(LIGHTGREY);

    // maybe last part better expresssed as <= PIT {dlb}
    if (player_in_hell() || player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
        curr_subdungeon_level = you.your_level - 26;

    /* Remember, must add this to the death_string in ouch */
    if (you.where_are_you >= BRANCH_ORCISH_MINES
        && you.where_are_you <= BRANCH_SWAMP)
    {
        curr_subdungeon_level = you.your_level 
                                    - you.branch_stairs[you.where_are_you - 10];
    }

    gotoxy(46, 12);

#if DEBUG_DIAGNOSTICS
    cprintf( "(%d) ", you.your_level + 1 );
#endif

    env.floor_colour = LIGHTGREY;
    env.rock_colour  = BROWN;

    take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE));
    if (you.level_type == LEVEL_PANDEMONIUM)
    {
        cprintf("- Pandemonium            ");

        env.floor_colour = (mcolour[env.mons_alloc[9]] == BLACK)
                                    ? LIGHTGREY : mcolour[env.mons_alloc[9]];

        env.rock_colour = (mcolour[env.mons_alloc[8]] == BLACK)
                                    ? LIGHTGREY : mcolour[env.mons_alloc[8]];
    }
    else if (you.level_type == LEVEL_ABYSS)
    {
        cprintf("- The Abyss               ");

        env.floor_colour = (mcolour[env.mons_alloc[9]] == BLACK)
                                    ? LIGHTGREY : mcolour[env.mons_alloc[9]];

        env.rock_colour = (mcolour[env.mons_alloc[8]] == BLACK)
                                    ? LIGHTGREY : mcolour[env.mons_alloc[8]];
    }
    else if (you.level_type == LEVEL_LABYRINTH)
    {
        cprintf("- a Labyrinth           ");
    }
    else
    {
        // level_type == LEVEL_DUNGEON
        if (!player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
            cprintf( "%d", curr_subdungeon_level );

        switch (you.where_are_you)
        {
        case BRANCH_MAIN_DUNGEON:
            cprintf(" of the Dungeon           ");
            break;
        case BRANCH_DIS:
            env.floor_colour = CYAN;
            env.rock_colour = CYAN;
            cprintf(" of Dis                   ");
            break;
        case BRANCH_GEHENNA:
            env.floor_colour = DARKGREY;
            env.rock_colour = RED;
            cprintf(" of Gehenna               ");
            break;
        case BRANCH_VESTIBULE_OF_HELL:
            env.floor_colour = LIGHTGREY;
            env.rock_colour = LIGHTGREY;
            cprintf("- the Vestibule of Hell            ");
            break;
        case BRANCH_COCYTUS:
            env.floor_colour = LIGHTBLUE;
            env.rock_colour = LIGHTCYAN;
            cprintf(" of Cocytus                   ");
            break;
        case BRANCH_TARTARUS:
            env.floor_colour = DARKGREY;
            env.rock_colour = DARKGREY;
            cprintf(" of Tartarus                ");
            break;
        case BRANCH_INFERNO:
            env.floor_colour = LIGHTRED;
            env.rock_colour = RED;
            cprintf(" of the Inferno               ");
            break;
        case BRANCH_THE_PIT:
            env.floor_colour = RED;
            env.rock_colour = DARKGREY;
            cprintf(" of the Pit              ");
            break;
        case BRANCH_ORCISH_MINES:
            env.floor_colour = BROWN;
            env.rock_colour = BROWN;
            cprintf(" of the Orcish Mines          ");
            break;
        case BRANCH_HIVE:
            env.floor_colour = YELLOW;
            env.rock_colour = BROWN;
            cprintf(" of the Hive                  ");
            break;
        case BRANCH_LAIR:
            env.floor_colour = GREEN;
            env.rock_colour = BROWN;
            cprintf(" of the Lair                  ");
            break;
        case BRANCH_SLIME_PITS:
            env.floor_colour = GREEN;
            env.rock_colour = LIGHTGREEN;
            cprintf(" of the Slime Pits            ");
            break;
        case BRANCH_VAULTS:
            env.floor_colour = LIGHTGREY;
            env.rock_colour = BROWN;
            cprintf(" of the Vaults                ");
            break;
        case BRANCH_CRYPT:
            env.floor_colour = LIGHTGREY;
            env.rock_colour = LIGHTGREY;
            cprintf(" of the Crypt                 ");
            break;
        case BRANCH_HALL_OF_BLADES:
            env.floor_colour = LIGHTGREY;
            env.rock_colour = LIGHTGREY;
            cprintf(" of the Hall of Blades        ");
            break;

        case BRANCH_HALL_OF_ZOT:
            if (you.your_level - you.branch_stairs[7] <= 1)
            {
                env.floor_colour = LIGHTGREY;
                env.rock_colour = LIGHTGREY;
            }
            else
            {
                switch (you.your_level - you.branch_stairs[7])
                {
                case 2:
                    env.rock_colour = LIGHTGREY;
                    env.floor_colour = BLUE;
                    break;
                case 3:
                    env.rock_colour = BLUE;
                    env.floor_colour = LIGHTBLUE;
                    break;
                case 4:
                    env.rock_colour = LIGHTBLUE;
                    env.floor_colour = MAGENTA;
                    break;
                case 5:
                    env.rock_colour = MAGENTA;
                    env.floor_colour = LIGHTMAGENTA;
                    break;
                }
            }
            cprintf(" of the Realm of Zot          ");
            break;

        case BRANCH_ECUMENICAL_TEMPLE:
            env.floor_colour = LIGHTGREY;
            env.rock_colour = LIGHTGREY;
            cprintf(" of the Temple                ");
            break;
        case BRANCH_SNAKE_PIT:
            env.floor_colour = LIGHTGREEN;
            env.rock_colour = YELLOW;
            cprintf(" of the Snake Pit             ");
            break;
        case BRANCH_ELVEN_HALLS:
            env.floor_colour = DARKGREY;
            env.rock_colour = LIGHTGREY;
            cprintf(" of the Elven Halls           ");
            break;
        case BRANCH_TOMB:
            env.floor_colour = YELLOW;
            env.rock_colour = LIGHTGREY;
            cprintf(" of the Tomb                  ");
            break;
        case BRANCH_SWAMP:
            env.floor_colour = BROWN;
            env.rock_colour = BROWN;
            cprintf(" of the Swamp                 ");
            break;
        }
    }                           // end else
}                               // end new_level()

static void dart_trap( bool trap_known, int trapped, struct bolt &pbolt, 
                       bool poison )
{
    int damage_taken = 0;
    int trap_hit, your_dodge;

    if (random2(10) < 2 || (trap_known && !one_chance_in(4)))
    {
        snprintf( info, INFO_SIZE, "You avoid triggering a%s trap.",
                                    pbolt.name.c_str() );
        mpr(info);
        return;
    }

    if (you.equip[EQ_SHIELD] != -1 && one_chance_in(3))
        exercise( SK_SHIELDS, 1 );

    snprintf( info, INFO_SIZE, "A%s shoots out and ", pbolt.name.c_str() );

    if (random2( 20 + 5 * you.shield_blocks * you.shield_blocks ) 
                                                < player_shield_class())
    {
        you.shield_blocks++;
        strcat( info, "hits your shield." );
        mpr(info);
        goto out_of_trap;
    }

    // note that this uses full ( not random2limit(foo,40) ) player_evasion.
    trap_hit = (20 + (you.your_level * 2)) * random2(200) / 100;

    your_dodge = player_evasion() + random2(you.dex) / 3
                            - 2 + (you.duration[DUR_REPEL_MISSILES] * 10);

    if (trap_hit >= your_dodge && you.duration[DUR_DEFLECT_MISSILES] == 0)
    {
        strcat( info, "hits you!" );
        mpr(info);

        if (poison && random2(100) < 50 - (3 * player_AC()) / 2
                && !player_res_poison())
        {
            poison_player( 1 + random2(3) );
        }

        damage_taken = roll_dice( pbolt.damage );
        damage_taken -= random2( player_AC() + 1 );

        if (damage_taken > 0)
            ouch( damage_taken, 0, KILLED_BY_TRAP, pbolt.name.c_str() );
    }
    else
    {
        strcat( info, "misses you." );
        mpr(info);
    }

    if (player_light_armour() && coinflip())
        exercise( SK_DODGING, 1 );

  out_of_trap:

    pbolt.target_x = you.x_pos;
    pbolt.target_y = you.y_pos;

    if (coinflip())
        itrap( pbolt, trapped );
}                               // end dart_trap()

//
// itrap takes location from target_x, target_y of bolt strcture.
//

void itrap( struct bolt &pbolt, int trapped )
{
    int base_type = OBJ_MISSILES;
    int sub_type = MI_DART;

    switch (env.trap[trapped].type)
    {
    case TRAP_DART:
        base_type = OBJ_MISSILES;
        sub_type = MI_DART;
        break;
    case TRAP_ARROW:
        base_type = OBJ_MISSILES;
        sub_type = MI_ARROW;
        break;
    case TRAP_BOLT:
        base_type = OBJ_MISSILES;
        sub_type = MI_BOLT;
        break;
    case TRAP_SPEAR:
        base_type = OBJ_WEAPONS;
        sub_type = WPN_SPEAR;
        break;
    case TRAP_AXE:
        base_type = OBJ_WEAPONS;
        sub_type = WPN_HAND_AXE;
        break;
    case TRAP_NEEDLE:
        base_type = OBJ_MISSILES;
        sub_type = MI_NEEDLE;
        break;
    default:
        return;
    }

    trap_item( base_type, sub_type, pbolt.target_x, pbolt.target_y );

    return;
}                               // end itrap()

void handle_traps(char trt, int i, bool trap_known)
{
    struct bolt beam;

    switch (trt)
    {
    case TRAP_DART:
        beam.name = " dart";
        beam.damage = dice_def( 1, 4 + (you.your_level / 2) );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_NEEDLE:
        beam.name = " needle";
        beam.damage = dice_def( 1, 0 );
        dart_trap(trap_known, i, beam, true);
        break;

    case TRAP_ARROW:
        beam.name = "n arrow";
        beam.damage = dice_def( 1, 7 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_BOLT:
        beam.name = " bolt";
        beam.damage = dice_def( 1, 13 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_SPEAR:
        beam.name = " spear";
        beam.damage = dice_def( 1, 10 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_AXE:
        beam.name = "n axe";
        beam.damage = dice_def( 1, 15 + you.your_level );
        dart_trap(trap_known, i, beam, false);
        break;

    case TRAP_TELEPORT:
        mpr("You enter a teleport trap!");

        if (scan_randarts(RAP_PREVENT_TELEPORTATION))
            mpr("You feel a weird sense of stasis.");
        else
            you_teleport2( true );
        break;

    case TRAP_AMNESIA:
        mpr("You feel momentarily disoriented.");
        if (!wearing_amulet(AMU_CLARITY))
            forget_map(random2avg(100, 2));
        break;

    case TRAP_BLADE:
        if (trap_known && one_chance_in(3))
            mpr("You avoid triggering a blade trap.");
        else if (random2limit(player_evasion(), 40)
                        + (random2(you.dex) / 3) + (trap_known ? 3 : 0) > 8)
        {
            mpr("A huge blade swings just past you!");
        }
        else
        {
            mpr("A huge blade swings out and slices into you!");
            ouch( (you.your_level * 2) + random2avg(29, 2)
                    - random2(1 + player_AC()), 0, KILLED_BY_TRAP, " blade" );
        }
        break;

    case TRAP_ZOT:
    default:
        mpr((trap_known) ? "You enter the Zot trap."
                         : "Oh no! You have blundered into a Zot trap!");
        miscast_effect( SPTYP_RANDOM, random2(30) + you.your_level,
                        75 + random2(100), 3, "a Zot trap" );
        break;
    }
}                               // end handle_traps()

void disarm_trap( struct dist &disa )
{
    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    int i, j;

    for (i = 0; i < MAX_TRAPS; i++)
    {
        if (env.trap[i].x == you.x_pos + disa.dx
            && env.trap[i].y == you.y_pos + disa.dy)
        {
            break;
        }

        if (i == MAX_TRAPS - 1)
        {
            mpr("Error - couldn't find that trap.");
            return;
        }
    }

    if (trap_category(env.trap[i].type) == DNGN_TRAP_MAGICAL)
    {
        mpr("You can't disarm that trap.");
        return;
    }

    if (random2(you.skills[SK_TRAPS_DOORS] + 2) <= random2(you.your_level + 5))
    {
        mpr("You failed to disarm the trap.");

        you.turn_is_over = true;

        if (random2(you.dex) > 5 + random2(5 + you.your_level))
            exercise(SK_TRAPS_DOORS, 1 + random2(you.your_level / 5));
        else
        {
            handle_traps(env.trap[i].type, i, false);

            if (coinflip())
                exercise(SK_TRAPS_DOORS, 1);
        }

        return;
    }

    mpr("You have disarmed the trap.");

    struct bolt beam;

    beam.target_x = you.x_pos + disa.dx;
    beam.target_y = you.y_pos + disa.dy;

    if (env.trap[i].type != TRAP_BLADE
        && trap_category(env.trap[i].type) == DNGN_TRAP_MECHANICAL)
    {
        for (j = 0; j < 20; j++)
        {
            // places items (eg darts), which will automatically stack
            itrap(beam, i);

            if (j > 10 && one_chance_in(3))
                break;
        }
    }

    grd[you.x_pos + disa.dx][you.y_pos + disa.dy] = DNGN_FLOOR;
    env.trap[i].type = TRAP_UNASSIGNED;
    you.turn_is_over = true;

    // reduced from 5 + random2(5)
    exercise(SK_TRAPS_DOORS, 1 + random2(5) + (you.your_level / 5));
}                               // end disarm_trap()

void weird_writing(char stringy[40])
{
    int temp_rand = 0;          // for probability determinations {dlb}

    temp_rand = random2(15);

    // you'll see why later on {dlb}
    strcpy(stringy, (temp_rand == 0) ? "writhing" :
                    (temp_rand == 1) ? "bold" :
                    (temp_rand == 2) ? "faint" :
                    (temp_rand == 3) ? "spidery" :
                    (temp_rand == 4) ? "blocky" :
                    (temp_rand == 5) ? "angular" :
                    (temp_rand == 6) ? "shimmering" :
                    (temp_rand == 7) ? "glowing" : "");

    if (temp_rand < 8)
        strcat(stringy, " ");   // see above for reasoning {dlb}

    temp_rand = random2(14);

    strcat(stringy, (temp_rand ==  0) ? "yellow" :
                    (temp_rand ==  1) ? "brown" :
                    (temp_rand ==  2) ? "black" :
                    (temp_rand ==  3) ? "purple" :
                    (temp_rand ==  4) ? "orange" :
                    (temp_rand ==  5) ? "lime-green" :
                    (temp_rand ==  6) ? "blue" :
                    (temp_rand ==  7) ? "grey" :
                    (temp_rand ==  8) ? "silver" :
                    (temp_rand ==  9) ? "gold" :
                    (temp_rand == 10) ? "umber" :
                    (temp_rand == 11) ? "charcoal" :
                    (temp_rand == 12) ? "pastel" :
                    (temp_rand == 13) ? "mauve"
                                      : "colourless");

    strcat(stringy, " ");

    temp_rand = random2(14);

    strcat(stringy, (temp_rand == 0) ? "writing" :
                    (temp_rand == 1) ? "scrawl" :
                    (temp_rand == 2) ? "sigils" :
                    (temp_rand == 3) ? "runes" :
                    (temp_rand == 4) ? "hieroglyphics" :
                    (temp_rand == 5) ? "scrawl" :
                    (temp_rand == 6) ? "print-out" :
                    (temp_rand == 7) ? "binary code" :
                    (temp_rand == 8) ? "glyphs" :
                    (temp_rand == 9) ? "symbols"
                                     : "text");

    return;
}                               // end weird_writing()

// must be a better name than 'place' for the first parameter {dlb}
// returns true if we manage to scramble free.
bool fall_into_a_pool( int entry_x, int entry_y, bool allow_shift, 
                       unsigned char terrain )
{
    bool escape = false;
    FixedVector< char, 2 > empty;

    if (you.species == SP_MERFOLK && terrain == DNGN_DEEP_WATER)
    {
        // These can happen when we enter deep water directly -- bwr
        merfolk_start_swimming();
        return (false);
    }

    strcpy(info, "You fall into the ");

    strcat(info, (terrain == DNGN_LAVA)       ? "lava" :
                 (terrain == DNGN_DEEP_WATER) ? "water"
                                              : "programming rift");

    strcat(info, "!");
    mpr(info);

    more();
    mesclr();

    if (terrain == DNGN_LAVA)
    {
        const int resist = player_res_fire();

        if (resist <= 0)
        {
            mpr( "The lava burns you to a cinder!" );
            ouch( INSTANT_DEATH, 0, KILLED_BY_LAVA );
        }
        else
        {
            // should boost # of bangs per damage in the future {dlb}
            mpr( "The lava burns you!" );
            ouch( (10 + roll_dice(2,50)) / resist, 0, KILLED_BY_LAVA );
        }

        expose_player_to_element( BEAM_LAVA, 14 );
    }

    // a distinction between stepping and falling from you.levitation
    // prevents stepping into a thin stream of lava to get to the other side.
    if (scramble())
    {
        if (allow_shift)
        {
            if (empty_surrounds( you.x_pos, you.y_pos, DNGN_FLOOR, 1, 
                                 false, empty ))
            {
                escape = true;
            }
            else
            {
                escape = false;
            }
        }
        else
        {
            // back out the way we came in, if possible
            if (grid_distance( you.x_pos, you.y_pos, entry_x, entry_y ) == 1
                && (entry_x != empty[0] || entry_y != empty[1]))
            {
                escape = true;
                empty[0] = entry_x;
                empty[1] = entry_y;
            }
            else  // zero or two or more squares away, with no way back
            {
                escape = false;
            }
        }
    }
    else
    {
        mpr("You try to escape, but your burden drags you down!");
    }

    if (escape && move_player_to_grid( empty[0], empty[1], false, false, true ))
    {
        mpr("You manage to scramble free!");

        if (terrain == DNGN_LAVA)
            expose_player_to_element( BEAM_LAVA, 14 );

        return (true);
    }

    mpr("You drown...");

    if (terrain == DNGN_LAVA)
        ouch( INSTANT_DEATH, 0, KILLED_BY_LAVA );
    else if (terrain == DNGN_DEEP_WATER)
        ouch( INSTANT_DEATH, 0, KILLED_BY_WATER );

    return (false);
}                               // end fall_into_a_pool()

bool scramble(void)
{
    int max_carry = carrying_capacity();

    if ((max_carry / 2) + random2(max_carry / 2) <= you.burden)
        return false;
    else
        return true;
}                               // end scramble()

void weird_colours(unsigned char coll, char wc[30])
{
    unsigned char coll_div16 = coll / 16; // conceivable max is then 16 {dlb}

    // Must start with a consonant!
    strcpy(wc, (coll_div16 == 0 || coll_div16 ==  7) ? "brilliant" :
               (coll_div16 == 1 || coll_div16 ==  8) ? "pale" :
               (coll_div16 == 2 || coll_div16 ==  9) ? "mottled" :
               (coll_div16 == 3 || coll_div16 == 10) ? "shimmering" :
               (coll_div16 == 4 || coll_div16 == 11) ? "bright" :
               (coll_div16 == 5 || coll_div16 == 12) ? "dark" :
               (coll_div16 == 6 || coll_div16 == 13) ? "shining"
                                                     : "faint");

    strcat(wc, " ");

    while (coll > 17)
        coll -= 10;

    strcat(wc, (coll ==  0) ? "red" :
               (coll ==  1) ? "purple" :
               (coll ==  2) ? "green" :
               (coll ==  3) ? "orange" :
               (coll ==  4) ? "magenta" :
               (coll ==  5) ? "black" :
               (coll ==  6) ? "grey" :
               (coll ==  7) ? "silver" :
               (coll ==  8) ? "gold" :
               (coll ==  9) ? "pink" :
               (coll == 10) ? "yellow" :
               (coll == 11) ? "white" :
               (coll == 12) ? "brown" :
               (coll == 13) ? "aubergine" :
               (coll == 14) ? "ochre" :
               (coll == 15) ? "leaf green" :
               (coll == 16) ? "mauve" :
               (coll == 17) ? "azure"
                            : "colourless");

    return;
}                               // end weird_colours()

bool go_berserk(bool intentional)
{
    if (you.berserker)
    {
        if (intentional)
            mpr("You're already berserk!");
        // or else you won't notice -- no message here.
        return false;
    }

    if (you.exhausted)
    {
        if (intentional)
            mpr("You're too exhausted to go berserk.");
        // or else they won't notice -- no message here
        return false;
    }

    if (you.is_undead)
    {
        if (intentional)
            mpr("You cannot raise a blood rage in your lifeless body.");
        // or else you won't notice -- no message here
        return false;
    }

    mpr("A red film seems to cover your vision as you go berserk!");
    mpr("You feel yourself moving faster!");
    mpr("You feel mighty!");

    you.berserker += 20 + random2avg(19, 2);

    calc_hp();
    you.hp *= 15;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);

    if (!you.might)
        modify_stat( STAT_STRENGTH, 5, true );

    you.might += you.berserker;
    haste_player( you.berserker );

    if (you.berserk_penalty != NO_BERSERK_PENALTY)
        you.berserk_penalty = 0;

    return true;
}                               // end go_berserk()

bool trap_item(char base_type, char sub_type, char beam_x, char beam_y)
{
    item_def  item;

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.plus = 0;
    item.plus2 = 0;
    item.flags = 0;
    item.special = 0;
    item.quantity = 1;
    item.colour = LIGHTCYAN;

    if (base_type == OBJ_MISSILES)
    {
        if (sub_type == MI_NEEDLE)
        {
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );
            item.colour = WHITE;
        }
        else
        {
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_NORMAL );

            if (sub_type == MI_ARROW)
                item.colour = BROWN;
        }
    }
    else
    {
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );
    }

    if (igrd[beam_x][beam_y] != NON_ITEM)
    {
        if (items_stack( item, mitm[ igrd[beam_x][beam_y] ] ))
        {
            inc_mitm_item_quantity( igrd[beam_x][beam_y], 1 );
            return (false);
        }

        // don't want to go overboard here. Will only generate up to three
        // separate trap items, or less if there are other items present.
        if (mitm[ igrd[beam_x][beam_y] ].link != NON_ITEM)
        {
            if (mitm[ mitm[ igrd[beam_x][beam_y] ].link ].link != NON_ITEM)
                return (false);
        }
    }                           // end of if igrd != NON_ITEM

    return (!copy_item_to_grid( item, beam_x, beam_y, 1 ));
}                               // end trap_item()

// returns appropriate trap symbol for a given trap type {dlb}
unsigned char trap_category(unsigned char trap_type)
{
    switch (trap_type)
    {
    case TRAP_TELEPORT:
    case TRAP_AMNESIA:
    case TRAP_ZOT:
        return (DNGN_TRAP_MAGICAL);

    case TRAP_DART:
    case TRAP_ARROW:
    case TRAP_SPEAR:
    case TRAP_AXE:
    case TRAP_BLADE:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
    default:                    // what *would* be the default? {dlb}
        return (DNGN_TRAP_MECHANICAL);
    }
}                               // end trap_category()

// returns index of the trap for a given (x,y) coordinate pair {dlb}
int trap_at_xy(int which_x, int which_y)
{

    for (int which_trap = 0; which_trap < MAX_TRAPS; which_trap++)
    {
        if (env.trap[which_trap].x == which_x
            && env.trap[which_trap].y == which_y)
        {
            return (which_trap);
        }
    }

    // no idea how well this will be handled elsewhere: {dlb}
    return (-1);
}                               // end trap_at_xy()

bool i_feel_safe()
{
    /* This is probably unnecessary, but I'm not sure that
       you're always at least 9 away from a wall */
    int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
    int yend = you.y_pos + 9, xend = you.x_pos + 9;
    if ( xstart < 0 ) xstart = 0;
    if ( ystart < 0 ) ystart = 0;
    if ( xend >= GXM ) xend = 0;
    if ( ystart >= GYM ) yend = 0;

    /* statue check */
    if (you.visible_statue[STATUE_SILVER] ||
             you.visible_statue[STATUE_ORANGE_CRYSTAL] )
	return false;

    /* monster check */
    for ( int y = ystart; y < yend; ++y ) {
	for ( int x = xstart; x < xend; ++x ) {
	    /* if you can see a nonfriendly monster then you feel
	       unsafe */
	    if ( see_grid(x,y) ) {
		const unsigned char targ_monst = mgrd[x][y];
		if ( targ_monst != NON_MONSTER ) {
		    struct monsters *mon = &menv[targ_monst];
		    if ( !mons_friendly(mon) &&
			 player_monster_visible(mon) &&
			 !mons_is_mimic(mon->type) &&
			 (!Options.safe_zero_exp ||
			  !mons_class_flag( mon->type, M_NO_EXP_GAIN ))) {
		      return false;
		    }
		}
	    }
	}
    }
    return true;
}

// Do not attempt to use level_id if level_type != LEVEL_DUNGEON
std::string short_place_name(level_id id)
{
    return short_place_name(
            get_packed_place(id.branch, id.depth, LEVEL_DUNGEON));
}

unsigned short get_packed_place( unsigned char branch, int subdepth,
                                 char level_type )
{
    unsigned short place = (unsigned short)
        ( (branch << 8) | (subdepth & 0xFF) );
    if (level_type == LEVEL_ABYSS || level_type == LEVEL_PANDEMONIUM
            || level_type == LEVEL_LABYRINTH)
        place = (unsigned short) ( (level_type << 8) | 0xFF );
    return place;
}

unsigned short get_packed_place()
{
    return get_packed_place( you.where_are_you,
                      subdungeon_depth(you.where_are_you, you.your_level),
                      you.level_type );
}

std::string place_name( unsigned short place, bool long_name,
                        bool include_number ) {

    unsigned char branch = (unsigned char) ((place >> 8) & 0xFF);
    int lev = place & 0xFF;

    std::string result;
    if (lev == 0xFF)
    {
        switch (branch)
        {
        case LEVEL_ABYSS:
            return ( long_name ? "The Abyss" : "Abyss" );
        case LEVEL_PANDEMONIUM:
            return ( long_name ? "Pandemonium" : "Pan" );
        case LEVEL_LABYRINTH:
            return ( long_name ? "a Labyrinth" : "Lab" );
        default:
            return ( long_name ? "Buggy Badlands" : "Bug" );
        }
    }
    else
    {
        switch (branch)
        {
        case BRANCH_VESTIBULE_OF_HELL:
            return ( long_name ? "The Vestibule of Hell" : "Hell" );
        case BRANCH_HALL_OF_BLADES:
            return ( long_name ? "The Hall of Blades" : "Blade" );
        case BRANCH_ECUMENICAL_TEMPLE:
            return ( long_name ? "The Ecumenical Temple" : "Temple" );
        case BRANCH_DIS:
            result = ( long_name ? "The Iron City of Dis" : "Dis");
            break;
        case BRANCH_GEHENNA:
            result = ( long_name ? "Gehenna" : "Geh" );
            break;            
        case BRANCH_COCYTUS:
            result = ( long_name ? "Cocytus" : "Coc" );
            break;
        case BRANCH_TARTARUS:
            result = ( long_name ? "Tartarus" : "Tar" );
            break;
        case BRANCH_ORCISH_MINES:
            result = ( long_name ? "The Orcish Mines" : "Orc" );
            break;
        case BRANCH_HIVE:
            result = ( long_name ? "The Hive" : "Hive" );
            break;
        case BRANCH_LAIR:
            result = ( long_name ? "The Lair" : "Lair" );
            break;
        case BRANCH_SLIME_PITS:
            result = ( long_name ? "The Slime Pits" : "Slime" );
            break;
        case BRANCH_VAULTS:
            result = ( long_name ? "The Vaults" : "Vault" );
            break;
        case BRANCH_CRYPT:
            result = ( long_name ? "The Crypt" : "Crypt" );
            break;
        case BRANCH_HALL_OF_ZOT:
            result = ( long_name ? "The Hall of Zot" : "Zot" );
            break;
        case BRANCH_SNAKE_PIT:
            result = ( long_name ? "The Snake Pit" : "Snake" );
            break;
        case BRANCH_ELVEN_HALLS:
            result = ( long_name ? "The Elven Halls" : "Elf" );
            break;
        case BRANCH_TOMB:
            result = ( long_name ? "The Tomb" : "Tomb" );
            break;            
        case BRANCH_SWAMP:
            result = ( long_name ? "The Swamp" : "Swamp" );
            break;            
        default:
            result = ( long_name ? "The Dungeon" : "D" );
            break;
        }
    }

    if ( include_number ) {
        char buf[200];
        if ( long_name ) {
            // decapitalize 'the'
            if ( result.find("The") == 0 )
                result[0] = 't';
            snprintf( buf, sizeof buf, "Level %d of %s",
                      lev, result.c_str() );
        }
        else if (lev) {
            snprintf( buf, sizeof buf, "%s:%d",
                      result.c_str(), lev );
        }
        else {
            snprintf( buf, sizeof buf, "%s:$",
                      result.c_str() );
        }
        result = buf;
    }
    return result;
}

// Takes a packed 'place' and returns a compact stringified place name.
// XXX: This is done in several other places; a unified function to
//      describe places would be nice.
std::string short_place_name(unsigned short place)
{
    return place_name( place, false, true );
}

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place)
{
    std::string place = place_name( packed_place, true, true );
    if (place.length() && place != "Pandemonium")
        place[0] = tolower(place[0]);
    return (place.find("level") == 0?
            "on " + place
          : "in " + place);
}

// Use current branch and depth
std::string prep_branch_level_name()
{
    return prep_branch_level_name( get_packed_place() );
}

int absdungeon_depth(unsigned char branch, int subdepth)
{
    int realdepth = subdepth - 1;

    if (branch >= BRANCH_ORCISH_MINES && branch <= BRANCH_SWAMP)
        realdepth = subdepth + you.branch_stairs[branch - 10];

    if (branch >= BRANCH_DIS && branch <= BRANCH_THE_PIT)
        realdepth = subdepth + 26;

    return realdepth;
}

int subdungeon_depth(unsigned char branch, int depth)
{
    int curr_subdungeon_level = depth + 1;

    if (branch >= BRANCH_DIS && branch <= BRANCH_THE_PIT)
        curr_subdungeon_level = depth - 26;

    if (branch >= BRANCH_ORCISH_MINES && branch <= BRANCH_SWAMP)
        curr_subdungeon_level = depth
                                - you.branch_stairs[branch - 10];

    return curr_subdungeon_level;
}

////////////////////////////////////////////////////////////////////////////
// Living breathing dungeon stuff.
//

static std::vector<coord_def> sfx_seeds;

void setup_environment_effects()
{
    sfx_seeds.clear();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            if (!in_bounds(x, y))
                continue;

            const int grid = grd[x][y];
            if (grid == DNGN_LAVA 
                    || (grid == DNGN_SHALLOW_WATER
                        && you.where_are_you == BRANCH_SWAMP))
            {
                coord_def c = { x, y };
                sfx_seeds.push_back(c);
            }
        }
    }
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%u environment effect seeds", sfx_seeds.size());
#endif
}

static void apply_environment_effect(const coord_def &c)
{
    const int grid = grd[c.x][c.y];
    if (grid == DNGN_LAVA)
        check_place_cloud( CLOUD_BLACK_SMOKE_MON, 
                           c.x, c.y, random_range( 4, 8 ) );
    else if (grid == DNGN_SHALLOW_WATER)
        check_place_cloud( CLOUD_MIST, 
                           c.x, c.y, random_range( 2, 5 ) );
}

static const int Base_Sfx_Chance = 5;
void run_environment_effects()
{
    if (!you.time_taken)
        return;

    // Each square in sfx_seeds has this chance of doing something special
    // per turn.
    const int sfx_chance = Base_Sfx_Chance * you.time_taken / 10;
    const int nseeds = sfx_seeds.size();

    // If there is a large number of seeds, speed things up by fudging the
    // numbers.
    if (nseeds > 100)
    {
        int nsels = div_rand_round( sfx_seeds.size() * sfx_chance, 100 );
        if (one_chance_in(5))
            nsels += random2(nsels * 3);

        for (int i = 0; i < nsels; ++i)
            apply_environment_effect( sfx_seeds[ random2(nseeds) ] );
    }
    else
    {
        for (int i = 0; i < nseeds; ++i)
        {
            if (random2(100) >= sfx_chance)
                continue;

            apply_environment_effect( sfx_seeds[i] );
        }
    }
}

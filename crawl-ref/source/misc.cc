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

#include <cstdlib>
#include <cstdio>
#include <cmath>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "abyss.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "delay.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "hiscores.h"
#include "it_use2.h"
#include "items.h"
#include "itemprop.h"
#include "lev-pand.h"
#include "macro.h"
#include "makeitem.h"
#include "monplace.h"
#include "mon-util.h"
#include "monstuff.h"
#include "notes.h"
#include "ouch.h"
#include "overmap.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells3.h"
#include "spl-cast.h"
#include "stuff.h"
#include "transfor.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"

bool scramble(void);
static bool trap_item(object_class_type base_type, char sub_type,
                      char beam_x, char beam_y);
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
    if (monster_descriptor(mons_class, MDSC_LEAVES_HIDE) && !one_chance_in(3))
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
        case MONS_SHEEP:
        case MONS_YAK:
            mitm[o].sub_type = ARM_ANIMAL_SKIN;
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

bool grid_is_solid( int x, int y )
{
    return (grid_is_solid(grd[x][y]));
}

bool grid_is_solid(const coord_def &c)
{
    return (grid_is_solid(grd(c)));
}

bool grid_is_trap(int grid)
{
    return (grid == DNGN_TRAP_MECHANICAL || grid == DNGN_TRAP_MAGICAL
              || grid == DNGN_TRAP_III);
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

// returns 0 if grid is not an altar, else it returns the GOD_* type
god_type grid_altar_god( unsigned char grid )
{
    if (grid >= DNGN_ALTAR_ZIN && grid <= DNGN_ALTAR_LUGONU)
        return (static_cast<god_type>( grid - DNGN_ALTAR_ZIN + 1 ));

    return (GOD_NO_GOD);
}

// returns DNGN_FLOOR for non-gods, otherwise returns the altar for
// the god.
int altar_for_god( god_type god )
{
    if (god == GOD_NO_GOD || god >= NUM_GODS)
        return (DNGN_FLOOR);  // Yeah, lame. Tell me about it.

    return (DNGN_ALTAR_ZIN + god - 1);
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

void search_around( bool only_adjacent )
{
    int i;

    // Traps and doors stepdown skill:
    // skill/(2x-1) for squares at distance x
    int max_dist = (you.skills[SK_TRAPS_DOORS] + 1) / 2;
    if ( max_dist > 5 )
        max_dist = 5;
    if ( max_dist > 1 && only_adjacent )
        max_dist = 1;
    if ( max_dist < 1 )
        max_dist = 1;

    for ( int srx = you.x_pos - max_dist; srx <= you.x_pos + max_dist; ++srx )
    {
        for ( int sry=you.y_pos - max_dist; sry<=you.y_pos + max_dist; ++sry )
        {
            if ( see_grid(srx,sry) ) // must have LOS
            {
                // maybe we want distance() instead of grid_distance()?
                int dist = grid_distance(srx, sry, you.x_pos, you.y_pos);

                // don't exclude own square; may be levitating
                if (dist == 0)
                    ++dist;
                
                // making this harsher by removing the old +1
                int effective = you.skills[SK_TRAPS_DOORS] / (2*dist - 1);
                
                if (grd[srx][sry] == DNGN_SECRET_DOOR &&
                    random2(17) <= effective)
                {
                    grd[srx][sry] = DNGN_CLOSED_DOOR;
                    mpr("You found a secret door!");
                    exercise(SK_TRAPS_DOORS, ((coinflip()) ? 2 : 1));
                }

                if (grd[srx][sry] == DNGN_UNDISCOVERED_TRAP &&
                    random2(17) <= effective)
                {
                    i = trap_at_xy(srx, sry);
                    
                    if (i != -1)
                        grd[srx][sry] = trap_category(env.trap[i].type);
                    
                    mpr("You found a trap!");
                }
            }
        }
    }

    return;
}                               // end search_around()

void in_a_cloud()
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

            ouch( hurted, cl, KILLED_BY_CLOUD, "freezing vapour" );
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
        mpr("You are engulfed in a cloud of smoke!");
        break;

    case CLOUD_STEAM:
        mpr("You are engulfed in a cloud of scalding steam!");
        if (player_res_steam() > 0)
        {
            mpr("It doesn't seem to affect you.");
            return;
        }

        hurted += (random2(6) * you.time_taken) / 10;
        if (hurted < 0 || player_res_fire() > 0)
            hurted = 0;

        ouch( (hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD,
              "steam" );
        break;

    case CLOUD_MIASMA:
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
    default:
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
    int stair_find = grd[you.x_pos][you.y_pos];
    const branch_type old_where = you.where_are_you;
    const bool was_a_labyrinth = you.level_type != LEVEL_DUNGEON;

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

    int old_level  = you.your_level;

    // Interlevel travel data:
    bool collect_travel_data = you.level_type != LEVEL_LABYRINTH 
                    && you.level_type != LEVEL_ABYSS 
                && you.level_type != LEVEL_PANDEMONIUM;

    level_id  old_level_id    = level_id::current();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);
    int stair_x = you.x_pos, stair_y = you.y_pos;
    if (collect_travel_data)
        old_level_info.update();

    // Make sure we return to our main dungeon level... labyrinth entrances
    // in the abyss or pandemonium a bit trouble (well the labyrinth does
    // provide a way out of those places, its really not that bad I suppose)
    if (you.level_type == LEVEL_LABYRINTH)
        you.level_type = LEVEL_DUNGEON;

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
                ouch(INSTANT_DEATH, 0, KILLED_BY_WINNING);
            }
        }

        ouch(INSTANT_DEATH, 0, KILLED_BY_LEAVING);
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

    // did we take a branch stair?
    for ( i = 0; i < NUM_BRANCHES; ++i )
    {
        if ( branches[i].exit_stairs == stair_find )
        {
            mprf("Welcome back to %s!",
                 branches[branches[i].parent_branch].longname);
            you.where_are_you = branches[i].parent_branch;
            break;
        }
    }

    const unsigned char stair_taken = stair_find;

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

    save_game_state();

    new_level();

    viewwindow(1, true);

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);
    
    if (collect_travel_data)
    {
        // Update stair information for the stairs we just ascended, and the
        // down stairs we're currently on.
        level_id  new_level_id    = level_id::current();

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

void down_stairs( bool remove_stairs, int old_level, int force_stair )
{
    int i;
    char old_level_type = you.level_type;
    const bool was_a_labyrinth = you.level_type != LEVEL_DUNGEON;
    const int stair_find =
        force_stair? force_stair : grd[you.x_pos][you.y_pos];

    bool leave_abyss_pan = false;
    branch_type old_where = you.where_are_you;

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

    if (!force_stair && player_is_levitating() 
            && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
    {
        mpr("You're floating high up above the floor!");
        return;
    }

#ifdef DGL_MILESTONES
    if (!force_stair)
    {
        // Not entirely accurate - the player could die before
        // reaching the Abyss.
        if (grd[you.x_pos][you.y_pos] == DNGN_ENTER_ABYSS)
            mark_milestone("abyss.enter", "entered the Abyss!");
        else if (grd[you.x_pos][you.y_pos] == DNGN_EXIT_ABYSS)
            mark_milestone("abyss.exit", "escaped from the Abyss!");
    }
#endif

    // [ds] Descending into the Labyrinth increments your_level. Going
    // downstairs from a labyrinth implies that you've been banished (or been
    // sent to Pandemonium somehow). Decrementing your_level here is needed
    // to fix this buggy sequence: D:n -> Labyrinth -> Abyss -> D:(n+1).
    if (you.level_type == LEVEL_LABYRINTH)
        you.your_level--;

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
                mprf( "You need at least %d Runes to enter this place.",
                      NUMBER_OF_RUNES_NEEDED );
            }
            return;
        }
    }

    // Interlevel travel data:
    bool collect_travel_data = you.level_type != LEVEL_LABYRINTH 
                    && you.level_type != LEVEL_ABYSS
                && you.level_type != LEVEL_PANDEMONIUM;

    level_id  old_level_id    = level_id::current();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);
    int stair_x = you.x_pos, stair_y = you.y_pos;
    if (collect_travel_data)
        old_level_info.update();

    // Preserve abyss uniques now, since this Abyss level will be deleted.
    if (you.level_type == LEVEL_ABYSS)
        save_abyss_uniques();
    
    if (you.level_type != LEVEL_DUNGEON
        && (you.level_type != LEVEL_PANDEMONIUM
            || stair_find != DNGN_TRANSIT_PANDEMONIUM))
    {
        you.level_type = LEVEL_DUNGEON;
    }

    if (!force_stair)
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
        if (!you.running && !force_stair)
            more();

        you.your_level = 26;
    }

    // welcome message
    // try to find a branch stair
    for ( i = 0; i < NUM_BRANCHES; ++i )
    {
        if ( branches[i].entry_stairs == stair_find )
        {
            if ( branches[i].entry_message )
                mpr(branches[i].entry_message);
            else
                mprf("Welcome to %s!", branches[i].longname);
            you.where_are_you = branches[i].id;
            break;
        }
    }

    if (stair_find == DNGN_ENTER_LABYRINTH)
    {
        // no longer a feature
        unnotice_labyrinth_portal();
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
                                          you.level_type, false );
#if DEBUG_DIAGNOSTICS
        mprf( MSGCH_DIAGNOSTICS, "Deleting: %s", lname.c_str() );
#endif
        unlink(lname.c_str());
    }

    if (stair_find == DNGN_EXIT_ABYSS || stair_find == DNGN_EXIT_PANDEMONIUM)
    {
        leave_abyss_pan = true;
        mpr("You pass through the gate.");
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

    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        stair_taken = DNGN_FLOOR;

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
        if (!force_stair)
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

    const bool newlevel =
        load(stair_taken, LOAD_ENTER_LEVEL, was_a_labyrinth,
             old_level, old_where);
    
    if (newlevel)
        xom_is_stimulated(49);

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

    save_game_state();

    new_level();

    viewwindow(1, true);

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    trackers_init_new_level(true);
    if (collect_travel_data)
    {
        // Update stair information for the stairs we just descended, and the
        // upstairs we're currently on.
        level_id  new_level_id    = level_id::current();

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

void trackers_init_new_level(bool transit)
{
    travel_init_new_level();
    if (transit)
        stash_init_new_level();
}

static char fix_black_colour(char incol)
{
    if ( incol == BLACK )
        return LIGHTGREY;
    else
        return incol;
}

void set_colours_from_monsters()
{
    env.floor_colour = fix_black_colour(mcolour[env.mons_alloc[9]]);
    env.rock_colour = fix_black_colour(mcolour[env.mons_alloc[8]]);
}

std::string level_description_string()
{
    if (you.level_type == LEVEL_PANDEMONIUM)
        return "- Pandemonium";

    if (you.level_type == LEVEL_ABYSS)
        return "- The Abyss";

    if (you.level_type == LEVEL_LABYRINTH)
        return "- a Labyrinth";

    // level_type == LEVEL_DUNGEON
    char buf[200];
    const int youbranch = you.where_are_you;
    if ( branches[youbranch].depth == 1 )
        snprintf(buf, sizeof buf, "- %s", branches[youbranch].longname);
    else
    {
        const int curr_subdungeon_level = player_branch_depth();
        snprintf(buf, sizeof buf, "%d of %s", curr_subdungeon_level,
                 branches[youbranch].longname);
    }
    return buf;
}


void new_level(void)
{
    textcolor(LIGHTGREY);

    gotoxy(46, 12);

#if DEBUG_DIAGNOSTICS
    cprintf( "(%d) ", you.your_level + 1 );
#endif

    take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE));
    cprintf("%s", level_description_string().c_str());

    if (you.level_type == LEVEL_PANDEMONIUM || you.level_type == LEVEL_ABYSS)
    {
        set_colours_from_monsters();
    }
    else if (you.level_type == LEVEL_LABYRINTH)
    {
        env.floor_colour = LIGHTGREY;
        env.rock_colour  = BROWN;
    }
    else
    {
        // level_type == LEVEL_DUNGEON
        const int youbranch = you.where_are_you;
        env.floor_colour = branches[youbranch].floor_colour;
        env.rock_colour = branches[youbranch].rock_colour;

        // Zot is multicoloured
        if ( you.where_are_you == BRANCH_HALL_OF_ZOT )
        {
            const char floorcolours_zot[] = { LIGHTGREY, LIGHTGREY, BLUE,
                                              LIGHTBLUE, MAGENTA };
            const char rockcolours_zot[] = { LIGHTGREY, BLUE, LIGHTBLUE,
                                             MAGENTA, LIGHTMAGENTA };

            const int curr_subdungeon_level = player_branch_depth();

            if ( curr_subdungeon_level > 5 || curr_subdungeon_level < 1 )
                mpr("Odd colouring!");
            else
            {
                env.floor_colour = floorcolours_zot[curr_subdungeon_level-1];
                env.rock_colour = rockcolours_zot[curr_subdungeon_level-1];
            }
        }
    }
    clear_to_end_of_line();
#ifdef DGL_WHEREIS
    whereis_record();
#endif
}

static void dart_trap( bool trap_known, int trapped, struct bolt &pbolt, 
                       bool poison )
{
    int damage_taken = 0;
    int trap_hit, your_dodge;

    std::string msg;

    if (random2(10) < 2 || (trap_known && !one_chance_in(4)))
    {
        mprf( "You avoid triggering a%s trap.", pbolt.name.c_str() );
        return;
    }

    if (you.equip[EQ_SHIELD] != -1 && one_chance_in(3))
        exercise( SK_SHIELDS, 1 );

    msg = "A";
    msg += pbolt.name;
    msg += " shoots out and ";

    if (random2( 20 + 5 * you.shield_blocks * you.shield_blocks ) 
                                                < player_shield_class())
    {
        you.shield_blocks++;
        msg += "hits your shield.";
        mpr(msg.c_str());
        goto out_of_trap;
    }

    // note that this uses full ( not random2limit(foo,40) ) player_evasion.
    trap_hit = (20 + (you.your_level * 2)) * random2(200) / 100;

    your_dodge = player_evasion() + random2(you.dex) / 3
                            - 2 + (you.duration[DUR_REPEL_MISSILES] * 10);

    if (trap_hit >= your_dodge && you.duration[DUR_DEFLECT_MISSILES] == 0)
    {
        msg += "hits you!";
        mpr(msg.c_str());

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
        msg += "misses you.";
        mpr(msg.c_str());
    }

    if (player_light_armour(true) && coinflip())
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
    object_class_type base_type = OBJ_MISSILES;
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
            you_teleport_now( true );
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
    learned_something_new(TUT_SEEN_TRAPS);
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
        const int num_to_make = 10 + random2(you.skills[SK_TRAPS_DOORS]);
        for (j = 0; j < num_to_make; j++)
        {           
            // places items (eg darts), which will automatically stack
            itrap(beam, i);
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

    mprf("You fall into the %s!",
         (terrain == DNGN_LAVA)       ? "lava" :
         (terrain == DNGN_DEEP_WATER) ? "water"
                                      : "programming rift");

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
                && (entry_x != empty[0] || entry_y != empty[1])
                && mgrd[entry_x][entry_y] == NON_MONSTER)
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
    if (!you.can_go_berserk(intentional))
        return (false);

    if (Options.tutorial_left)
        Options.tut_berserk_counter++;
    
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

bool trap_item(object_class_type base_type, char sub_type,
               char beam_x, char beam_y)
{
    item_def  item;

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.plus = 0;
    item.plus2 = 0;
    item.flags = 0;
    item.special = 0;
    item.quantity = 1;

    if (base_type == OBJ_MISSILES)
    {
        if (sub_type == MI_NEEDLE)
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );
        else
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_NORMAL );
    }
    else
    {
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );
    }

    item_colour(item);

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
        if (env.trap[which_trap].x == which_x &&
            env.trap[which_trap].y == which_y &&
            env.trap[which_trap].type != TRAP_UNASSIGNED)
        {
            return (which_trap);
        }
    }

    // no idea how well this will be handled elsewhere: {dlb}
    return (-1);
}                               // end trap_at_xy()

bool is_damaging_cloud(cloud_type type)
{
    switch (type)
    {
    case CLOUD_FIRE:
    case CLOUD_STINK:
    case CLOUD_COLD:
    case CLOUD_POISON:
    case CLOUD_STEAM:
    case CLOUD_MIASMA:
        return (true);
    default:
        return (false);
    }
}

std::string cloud_name(cloud_type type)
{
    switch (type)
    {
    case CLOUD_FIRE:
        return "flame";
    case CLOUD_STINK:
        return "noxious fumes";
    case CLOUD_COLD:
        return "freezing vapour";
    case CLOUD_POISON:
        return "poison gases";
    case CLOUD_GREY_SMOKE:
        return "grey smoke";
    case CLOUD_BLUE_SMOKE:
        return "blue smoke";
    case CLOUD_PURP_SMOKE:
        return "purple smoke";
    case CLOUD_STEAM:
        return "steam";
    case CLOUD_MIASMA:
        return "foul pestilence";
    case CLOUD_BLACK_SMOKE:
        return "black smoke";
    case CLOUD_MIST:
        return "thin mist";
    default:
        return "buggy goodness";
    }
}

bool i_feel_safe(bool announce)
{
    /* This is probably unnecessary, but I'm not sure that
       you're always at least 9 away from a wall */
    int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
    int yend = you.y_pos + 9, xend = you.x_pos + 9;
    if ( xstart < 0 ) xstart = 0;
    if ( ystart < 0 ) ystart = 0;
    if ( xend >= GXM ) xend = GXM;
    if ( ystart >= GYM ) yend = GYM;

    if (in_bounds(you.x_pos, you.y_pos)
        && env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
    {
        const cloud_type type =
            env.cloud[ env.cgrid[you.x_pos][you.y_pos] ].type;
        if (is_damaging_cloud(type))
        {
            if (announce)
                mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                     cloud_name(type).c_str());
            return (false);
        }
    }

    std::vector<const monsters *> mons;
    /* monster check */
    for ( int y = ystart; y < yend; ++y )
    {
        for ( int x = xstart; x < xend; ++x )
        {
            /* if you can see a nonfriendly monster then you feel
               unsafe */
            if ( see_grid(x,y) )
            {
                const unsigned char targ_monst = mgrd[x][y];
                if ( targ_monst != NON_MONSTER )
                {
                    struct monsters *mon = &menv[targ_monst];
                    if ( !mons_friendly(mon) &&
                         player_monster_visible(mon) &&
                         !mons_is_submerged(mon) &&
                         !mons_is_mimic(mon->type) &&
                         (!Options.safe_zero_exp ||
                          !mons_class_flag( mon->type, M_NO_EXP_GAIN )))
                    {
                        if (announce)
                            mons.push_back(mon);
                        else
                        {
                            tutorial_first_monster(*mon);
                            return false;
                        }
                    }
                }
            }
        }
    }

    if (announce)
    {
        // Announce the presence of monsters (Eidolos).
        if (mons.size() == 1)
        {
            const monsters &m = *mons[0];
            mprf(MSGCH_WARN, "Not with %s in view!",
                 str_monam(m, DESC_NOCAP_A).c_str());
        }
        else if (mons.size() > 1)
        {
            mprf(MSGCH_WARN, "Not with these monsters around!");
        }
        return (mons.empty());
    }
    
    return true;
}

// Do not attempt to use level_id if level_type != LEVEL_DUNGEON
std::string short_place_name(level_id id)
{
    return id.describe();
}

int place_branch(unsigned short place)
{
    const unsigned branch = (unsigned) ((place >> 8) & 0xFF);
    const int lev = place & 0xFF;
    return lev == 0xFF? -1 : (int) branch;
}

int place_depth(unsigned short place)
{
    const int lev = place & 0xFF;
    return lev == 0xFF? -1 : lev;
}

unsigned short get_packed_place( branch_type branch, int subdepth,
                                 level_area_type level_type )
{
    unsigned short place = (unsigned short)
        ( (static_cast<int>(branch) << 8) | (subdepth & 0xFF) );
    if (level_type == LEVEL_ABYSS || level_type == LEVEL_PANDEMONIUM
            || level_type == LEVEL_LABYRINTH)
        place = (unsigned short) ( (static_cast<int>(level_type) << 8) | 0xFF );
    return place;
}

unsigned short get_packed_place()
{
    return get_packed_place( you.where_are_you,
                      subdungeon_depth(you.where_are_you, you.your_level),
                      you.level_type );
}

bool single_level_branch( branch_type branch )
{
    return
        branch >= 0 && branch < NUM_BRANCHES
        && branches[branch].depth == 1;
}

std::string place_name( unsigned short place, bool long_name,
                        bool include_number )
{

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

    result = (long_name ?
              branches[branch].longname : branches[branch].abbrevname);

    if ( include_number && branches[branch].depth != 1 )
    {
        char buf[200];
        if ( long_name )
        {
            // decapitalize 'the'
            if ( result.find("The") == 0 )
                result[0] = 't';
            snprintf( buf, sizeof buf, "Level %d of %s",
                      lev, result.c_str() );
        }
        else if (lev)
            snprintf( buf, sizeof buf, "%s:%d", result.c_str(), lev );
        else
            snprintf( buf, sizeof buf, "%s:$", result.c_str() );

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

int absdungeon_depth(branch_type branch, int subdepth)
{
    if (branch >= BRANCH_VESTIBULE_OF_HELL && branch <= BRANCH_THE_PIT)
        return subdepth + 27;
    else
    {
        --subdepth;
        while ( branch != BRANCH_MAIN_DUNGEON )
        {
            subdepth += branches[branch].startdepth;
            branch = branches[branch].parent_branch;
        }
    }
    return subdepth;
}

int subdungeon_depth(branch_type branch, int depth)
{
    return depth - absdungeon_depth(branch, 0);
}

int player_branch_depth()
{
    return subdungeon_depth(you.where_are_you, you.your_level);
}

static const char *shop_types[] = {
    "weapon",
    "armour",
    "antique weapon",
    "antique armour",
    "antiques",
    "jewellery",
    "wand",
    "book",
    "food",
    "distillery",
    "scroll",
    "general"
};

int str_to_shoptype(const std::string &s)
{
    if (s == "random" || s == "any")
        return (SHOP_RANDOM);
    
    for (unsigned i = 0; i < sizeof(shop_types) / sizeof (*shop_types); ++i)
    {
        if (s == shop_types[i])
            return (i);
    }
    return (-1);
}

/* Decides whether autoprayer Right Now is a good idea. */
static bool should_autopray()
{
    if ( Options.autoprayer_on == false ||
         you.religion == GOD_NO_GOD ||
         you.duration[DUR_PRAYER] ||
         grid_altar_god( grd[you.x_pos][you.y_pos] ) != GOD_NO_GOD ||
         !i_feel_safe() )
        return false;

    // We already know that we're not praying now. So if you
    // just autoprayed, there's a problem.
    if ( you.just_autoprayed )
    {
        mpr("Autoprayer failed, deactivating.", MSGCH_WARN);
        Options.autoprayer_on = false;
        return false;
    }

    return true;
}

/* Actually performs autoprayer. */
bool do_autopray()
{
    if ( you.turn_is_over )     // can happen with autopickup, I think
        return false;

    if ( should_autopray() )
    {
        pray();
        you.just_autoprayed = true;
        return true;
    }
    else
    {
        you.just_autoprayed = false;
        return false;
    }
}

// general threat = sum_of_logexpervalues_of_nearby_unfriendly_monsters
// highest threat = highest_logexpervalue_of_nearby_unfriendly_monsters
void monster_threat_values(double *general, double *highest)
{
    double sum = 0;
    int highest_xp = -1;

    monsters *monster = NULL;
    for (int it = 0; it < MAX_MONSTERS; it++)
    {
        monster = &menv[it];

        if (monster->alive() && mons_near(monster) && !mons_friendly(monster))
        {
            const int xp = exper_value(monster);
            const double log_xp = log(xp);
            sum += log_xp;
            if (xp > highest_xp)
            {
                highest_xp = xp;
                *highest   = log_xp;
            }
        }
    }

    *general = sum;
}

bool player_in_a_dangerous_place()
{
    const double logexp = log(you.experience);
    double gen_threat = 0.0, hi_threat = 0.0;
    monster_threat_values(&gen_threat, &hi_threat);

    return (gen_threat > logexp * 1.3 || hi_threat > logexp / 2);
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
                const coord_def c(x, y);
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
        check_place_cloud( CLOUD_BLACK_SMOKE, 
                           c.x, c.y, random_range( 4, 8 ), KC_OTHER );
    else if (grid == DNGN_SHALLOW_WATER)
        check_place_cloud( CLOUD_MIST, 
                           c.x, c.y, random_range( 2, 5 ), KC_OTHER );
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

    // If there are a large number of seeds, speed things up by fudging the
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

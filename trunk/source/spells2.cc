/*
 *  File:       spells2.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *     <4>    03jan1999    jmf     Changed summon_small_mammals so at
 *                                 higher levels it indeed summons in plural.
 *                                 Removed some IMHO unnecessary failure msgs.
 *                                 (from e.g. animate_dead).
 *                                 Added protection by special deities.
 *     <3>     10/11/99    BCR     fixed range bug in burn_freeze,
 *                                 vamp_drain, and summon_elemental
 *     <2>     5/26/99     JDJ     detect_items uses '~' instead of '*'.
 *     <1>     -/--/--     LRH     Created
 */

#include "AppHdr.h"
#include "spells2.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "direct.h"
#include "effects.h"
#include "itemname.h"
#include "items.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "spells4.h"
#include "spl-cast.h"
#include "stuff.h"
#include "view.h"
#include "wpn-misc.h"

int raise_corpse( int corps, int corx, int cory, int corps_beh,
                  int corps_hit, int actual );

unsigned char detect_traps( int pow )
{
    unsigned char traps_found = 0;

    if (pow > 50)
        pow = 50;

    const int range = 8 + random2(8) + pow;

    for (int count_x = 0; count_x < MAX_TRAPS; count_x++)
    {
        const int etx = env.trap[ count_x ].x;
        const int ety = env.trap[ count_x ].y;

        // Used to just be visible screen:
        // if (etx > you.x_pos - 15 && etx < you.x_pos + 15
        //     && ety > you.y_pos - 8 && ety < you.y_pos + 8)

        if (grid_distance( you.x_pos, you.y_pos, etx, ety ) < range)
        {
            if (grd[ etx ][ ety ] == DNGN_UNDISCOVERED_TRAP)
            {
                traps_found++;

                grd[ etx ][ ety ] = trap_category( env.trap[count_x].type );
                env.map[etx - 1][ety - 1] = '^';
            }
        }
    }

    return (traps_found);
}                               // end detect_traps()

unsigned char detect_items( int pow )
{
    if (pow > 50)
        pow = 50;

    unsigned char items_found = 0;
    const int     map_radius = 8 + random2(8) + pow;

    mpr("You detect items!");

    for (int i = you.x_pos - map_radius; i < you.x_pos + map_radius; i++)
    {
        for (int j = you.y_pos - map_radius; j < you.y_pos + map_radius; j++)
        {
            if (i < 5 || j < 5 || i > (GXM - 5) || j > (GYM - 5))
                continue;

            if (igrd[i][j] != NON_ITEM)
            {
                env.map[i - 1][j - 1] = '~';
            }
        }
    }

    return (items_found);
}                               // end detect_items()

unsigned char detect_creatures( int pow )
{
    if (pow > 50)
        pow = 50;

    unsigned char creatures_found = 0;
    const int     map_radius = 8 + random2(8) + pow;

    mpr("You detect creatures!");

    for (int i = you.x_pos - map_radius; i < you.x_pos + map_radius; i++)
    {
        for (int j = you.y_pos - map_radius; j < you.y_pos + map_radius; j++)
        {
            if (i < 5 || j < 5 || i > (GXM - 5) || j > (GYM - 5))
                continue;

            if (mgrd[i][j] != NON_MONSTER)
            {
                struct monsters *mon = &menv[ mgrd[i][j] ];

                env.map[i - 1][j - 1] = mons_char( mon->type );

                // Assuming that highly intelligent spellcasters can
                // detect scyring. -- bwr
                if (mons_intel( mon->type ) == I_HIGH
                    && mons_flag( mon->type, M_SPELLCASTER ))
                {
                    behaviour_event( mon, ME_DISTURB, MHITYOU, 
                                     you.x_pos, you.y_pos );
                }
            }
        }
    }

    return (creatures_found);
}                               // end detect_creatures()

int corpse_rot(int power)
{
    UNUSED( power );

    char adx = 0;
    char ady = 0;

    char minx = you.x_pos - 6;
    char maxx = you.x_pos + 7;
    char miny = you.y_pos - 6;
    char maxy = you.y_pos + 6;
    char xinc = 1;
    char yinc = 1;

    if (coinflip())
    {
        minx = you.x_pos + 6;
        maxx = you.x_pos - 7;
        xinc = -1;
    }

    if (coinflip())
    {
        miny = you.y_pos + 6;
        maxy = you.y_pos - 7;
        yinc = -1;
    }

    for (adx = minx; adx != maxx; adx += xinc)
    {
        if (adx == 7 || adx == -7)
            return 0;

        for (ady = miny; ady != maxy; ady += yinc)
        {
            if (see_grid(adx, ady))
            {
                if (igrd[adx][ady] == NON_ITEM
                    || env.cgrid[adx][ady] != EMPTY_CLOUD)
                {
                    continue;
                }

                int objl = igrd[adx][ady];
                int hrg = 0;

                while (objl != NON_ITEM)
                {
                    if (mitm[objl].base_type == OBJ_CORPSES
                        && mitm[objl].sub_type == CORPSE_BODY)
                    {
                        if (!mons_skeleton(mitm[objl].plus))
                            destroy_item(objl);
                        else
                        {
                            mitm[objl].sub_type = CORPSE_SKELETON;
                            mitm[objl].special = 200;
                            mitm[objl].colour = LIGHTGREY;
                        }

                        place_cloud(CLOUD_MIASMA, adx, ady,
                                    4 + random2avg(16, 3));

                        goto out_of_raise;
                    }
                    hrg = mitm[objl].link;
                    objl = hrg;
                }

              out_of_raise:
                objl = 1;
            }
        }
    }

    if (you.species != SP_MUMMY)   // josh declares mummies cannot smell {dlb}
        mpr("You smell decay.");

    // should make zombies decay into skeletons

    return 0;
}                               // end corpse_rot()

int animate_dead( int power, int corps_beh, int corps_hit, int actual )
{
    UNUSED( power );

    int adx = 0;
    int ady = 0;

    int minx = you.x_pos - 6;
    int maxx = you.x_pos + 7;
    int miny = you.y_pos - 6;
    int maxy = you.y_pos + 6;
    int xinc = 1;
    int yinc = 1;

    int number_raised = 0;

    if (coinflip())
    {
        minx = you.x_pos + 6;
        maxx = you.x_pos - 7;
        xinc = -1;
    }

    if (coinflip())
    {
        miny = you.y_pos + 6;
        maxy = you.y_pos - 7;
        yinc = -1;
    }

    for (adx = minx; adx != maxx; adx += xinc)
    {
        if ((adx == 7) || (adx == -7))
            return 0;

        for (ady = miny; ady != maxy; ady += yinc)
        {
            if (see_grid(adx, ady))
            {
                if (igrd[adx][ady] != NON_ITEM)
                {
                    int objl = igrd[adx][ady];
                    int hrg = 0;

                    //this searches all the items on the ground for a corpse
                    while (objl != NON_ITEM)
                    {
                        if (mitm[objl].base_type == OBJ_CORPSES)
                        {
                            number_raised += raise_corpse(objl, adx, ady,
                                                corps_beh, corps_hit, actual);
                            break;
                        }

                        hrg = mitm[objl].link;
                        objl = hrg;
                    }

                    objl = 1;
                }
            }
        }
    }

    if (actual == 0)
        return number_raised;

    if (number_raised > 0)
    {
        mpr("The dead are walking!");
        //else
        //  mpr("The dark energy consumes the dead!"); - no, this
        // means that no corpses were found. Better to say:
        // mpr("You receive no reply.");
        //jmf: Why do I have to get an uninformative message when some random
        //jmf: monster fails to do something?
        // IMHO there's too much noise already.
    }

    return number_raised;
}                               // end animate_dead()

int animate_a_corpse( int axps,  int ayps, int corps_beh, int corps_hit,
                      int class_allowed )
{
    if (igrd[axps][ayps] == NON_ITEM)
        return 0;
    else if (mitm[igrd[axps][ayps]].base_type != OBJ_CORPSES)
        return 0;
    else if (class_allowed == CORPSE_SKELETON
             && mitm[igrd[axps][ayps]].sub_type != CORPSE_SKELETON)
        return 0;
    else
        if (raise_corpse( igrd[axps][ayps], axps, ayps, 
                          corps_beh, corps_hit, 1 ) > 0)
    {
        mpr("The dead are walking!");
    }

    return 0;
}                               // end animate_a_corpse()

int raise_corpse( int corps, int corx, int cory, 
                  int corps_beh, int corps_hit, int actual )
{
    int returnVal = 1;

    if (!mons_zombie_size(mitm[corps].plus))
        returnVal = 0;
    else if (actual != 0)
    {
        int type;
        if (mitm[corps].sub_type == CORPSE_BODY)
        {
            if (mons_zombie_size(mitm[corps].plus) == Z_SMALL)
                type = MONS_ZOMBIE_SMALL;
            else
                type = MONS_ZOMBIE_LARGE;
        }
        else
        {
            if (mons_zombie_size(mitm[corps].plus) == Z_SMALL)
                type = MONS_SKELETON_SMALL;
            else
                type = MONS_SKELETON_LARGE;
        }

        create_monster( type, 0, corps_beh, corx, cory, corps_hit,
                        mitm[corps].plus );

        destroy_item(corps);
    }

    return returnVal;
}                               // end raise_corpse()

void cast_twisted(int power, int corps_beh, int corps_hit)
{
    int total_mass = 0;
    int num_corpses = 0;
    int type_resurr = MONS_ABOMINATION_SMALL;
    char colour;

    unsigned char rotted = 0;

    if (igrd[you.x_pos][you.y_pos] == NON_ITEM)
    {
        mpr("There's nothing here!");
        return;
    }

    int objl = igrd[you.x_pos][you.y_pos];
    int next;

    while (objl != NON_ITEM)
    {
        next = mitm[objl].link;

        if (mitm[objl].base_type == OBJ_CORPSES
                && mitm[objl].sub_type == CORPSE_BODY)
        {
            total_mass += mons_weight( mitm[objl].plus );

            num_corpses++;
            if (mitm[objl].special < 100)
                rotted++;

            destroy_item( objl );
        }

        objl = next;
    }

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Mass for abomination: %d", total_mass);
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    // This is what the old statement pretty much boils down to, 
    // the average will be approximately 10 * power (or about 1000
    // at the practical maximum).  That's the same as the mass
    // of a hippogriff, a spiny frog, or a steam dragon.  Thus, 
    // material components are far more important to this spell. -- bwr
    total_mass += roll_dice( 20, power );

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Mass including power bonus: %d", total_mass);
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (total_mass < 400 + roll_dice( 2, 500 )
        || num_corpses < (coinflip() ? 3 : 2))
    {
        mpr("The spell fails.");
        mpr("The corpses collapse into a pulpy mess.");
        return;
    }

    if (total_mass > 500 + roll_dice( 3, 1000 ))
        type_resurr = MONS_ABOMINATION_LARGE;

    if (rotted == num_corpses)
        colour = BROWN;
    else if (rotted >= random2( num_corpses ))
        colour = RED;
    else
        colour = LIGHTRED;

    int mon = create_monster( type_resurr, 0, corps_beh, you.x_pos, you.y_pos,
                              corps_hit, colour );

    if (mon == -1)
        mpr("The corpses collapse into a pulpy mess.");
    else
    {
        mpr("The heap of corpses melds into an agglomeration of writhing flesh!");
        if (type_resurr == MONS_ABOMINATION_LARGE)
        {
            menv[mon].hit_dice = 8 + total_mass / ((colour == LIGHTRED) ? 500 :
                                                   (colour == RED)      ? 1000 
                                                                        : 2500);

            if (menv[mon].hit_dice > 30)
                menv[mon].hit_dice = 30;

            // XXX: No convenient way to get the hit dice size right now.
            menv[mon].hit_points = hit_points( menv[mon].hit_dice, 2, 5 );
            menv[mon].max_hit_points = menv[mon].hit_points;

            if (colour == LIGHTRED)
                menv[mon].armour_class += total_mass / 1000;
        }
    }
}                               // end cast_twisted()

bool brand_weapon(int which_brand, int power)
{
    int temp_rand;              // probability determination {dlb}
    int duration_affected = 0;  //jmf: NB: now HOW LONG, not WHICH BRAND.

    const int wpn = you.equip[EQ_WEAPON];

    if (you.duration[DUR_WEAPON_BRAND])
        return false;

    if (wpn == -1)
        return false;

    if (you.inv[wpn].base_type != OBJ_WEAPONS
        || launches_things(you.inv[wpn].sub_type))
    {
        return false;
    }

    if (is_fixed_artefact( you.inv[wpn] )
        || is_random_artefact( you.inv[wpn] )
        || get_weapon_brand( you.inv[wpn] ) != SPWPN_NORMAL )
    {
        return false;
    }

    char str_pass[ ITEMNAME_SIZE ];
    in_name( wpn, DESC_CAP_YOUR, str_pass );
    strcpy( info, str_pass );

    const int wpn_type = damage_type( you.inv[wpn].base_type,
                                      you.inv[wpn].sub_type );

    switch (which_brand)        // use SPECIAL_WEAPONS here?
    {
    case SPWPN_FLAMING:
        strcat(info, " bursts into flame!");
        duration_affected = 7;
        break;

    case SPWPN_FREEZING:
        strcat(info, " glows blue.");
        duration_affected = 7;
        break;

    case SPWPN_VENOM:
        if (wpn_type == DVORP_CRUSHING)
            return false;

        strcat(info, " starts dripping with poison.");
        duration_affected = 15;
        break;

    case SPWPN_DRAINING:
        strcat(info, " crackles with unholy energy.");
        duration_affected = 12;
        break;

    case SPWPN_VORPAL:
        if (wpn_type != DVORP_SLICING)
            return false;

        strcat(info, " glows silver and looks extremely sharp.");
        duration_affected = 10;
        break;

    case SPWPN_DISTORTION:      //jmf: added for Warp Weapon
        strcat(info, " seems to ");

        temp_rand = random2(6);
        strcat(info, (temp_rand == 0) ? "twist" :
                     (temp_rand == 1) ? "bend" :
                     (temp_rand == 2) ? "vibrate" :
                     (temp_rand == 3) ? "flex" :
                     (temp_rand == 4) ? "wobble"
                                      : "twang");

        strcat( info, coinflip() ? " oddly." : " strangely." );
        duration_affected = 5;

        // This brand is insanely powerful, this isn't even really
        // a start to balancing it, but it needs something. -- bwr
        miscast_effect(SPTYP_TRANSLOCATION, 9, 90, 100, "a distortion effect");
        break;

    case SPWPN_DUMMY_CRUSHING:  //jmf: added for Maxwell's Silver Hammer
        if (wpn_type != DVORP_CRUSHING)
            return false;

        which_brand = SPWPN_VORPAL;
        strcat(info, " glows silver and feels heavier.");
        duration_affected = 7;
        break;
    }

    set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, which_brand );

    mpr(info);
    you.wield_change = true;

    int dur_change = duration_affected + roll_dice( 2, power );

    you.duration[DUR_WEAPON_BRAND] += dur_change;

    if (you.duration[DUR_WEAPON_BRAND] > 50)
        you.duration[DUR_WEAPON_BRAND] = 50;

    return true;
}                               // end brand_weapon()

bool restore_stat(unsigned char which_stat, bool suppress_msg)
{
    bool statRestored = false;

    // a bit hackish, but cut me some slack, man! --
    // besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        for (unsigned char loopy = STAT_STRENGTH; loopy < NUM_STATS; loopy++)
        {
            if (restore_stat(loopy, suppress_msg) == true)
                statRestored = true;
        }
        return statRestored;                 // early return {dlb}
    }

    // the real function begins here {dlb}:
    char *ptr_stat = 0;         // NULL {dlb}
    char *ptr_stat_max = 0;     // NULL {dlb}
    char *ptr_redraw = 0;       // NULL {dlb}

    if (!suppress_msg)
        strcpy(info, "You feel your ");

    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STRENGTH:
        if (!suppress_msg)
            strcat(info, "strength");

        ptr_stat = &you.strength;
        ptr_stat_max = &you.max_strength;
        ptr_redraw = &you.redraw_strength;
        break;

    case STAT_DEXTERITY:
        if (!suppress_msg)
            strcat(info, "dexterity");

        ptr_stat = &you.dex;
        ptr_stat_max = &you.max_dex;
        ptr_redraw = &you.redraw_dexterity;
        break;

    case STAT_INTELLIGENCE:
        if (!suppress_msg)
            strcat(info, "intelligence");

        ptr_stat = &you.intel;
        ptr_stat_max = &you.max_intel;
        ptr_redraw = &you.redraw_intelligence;
        break;
    }

    if (*ptr_stat < *ptr_stat_max)
    {
        if (!suppress_msg)
        {
            strcat(info, " returning.");
            mpr(info);
        }

        *ptr_stat = *ptr_stat_max;
        *ptr_redraw = 1;
        statRestored = true;

        if (ptr_stat == &you.strength)
            burden_change();
    }

    return statRestored;
}                               // end restore_stat()

void turn_undead(int pow)
{
    struct monsters *monster;

    mpr("You attempt to repel the undead.");

    for (int tu = 0; tu < MAX_MONSTERS; tu++)
    {
        monster = &menv[tu];

        if (monster->type == -1 || !mons_near(monster))
            continue;

        // used to inflict random2(5) + (random2(pow) / 20) damage,
        // in addition {dlb}
        if (mons_holiness(monster->type) == MH_UNDEAD)
        {
            if (check_mons_resist_magic( monster, pow ))
            {
                simple_monster_message( monster, " resists." );
                continue;
            }

            if (!mons_add_ench(monster, ENCH_FEAR))
                continue;

            simple_monster_message( monster, " is repelled!" );

            //mv: must be here to work
            behaviour_event( monster, ME_SCARE, MHITYOU ); 

            // reduce power based on monster turned
            pow -= monster->hit_dice * 3;
            if (pow <= 0)
                break;

        }                       // end "if mons_holiness"
    }                           // end "for tu"
}                               // end turn_undead()

void holy_word(int pow)
{
    struct monsters *monster;

    mpr("You speak a Word of immense power!");

    // doubt this will ever happen, but it's here as a safety -- bwr
    if (pow > 300)
        pow = 300;

    for (int tu = 0; tu < MAX_MONSTERS; tu++)
    {
        monster = &menv[tu];

        if (monster->type == -1 || !mons_near(monster))
            continue;

        if (mons_holiness(monster->type) == MH_UNDEAD
                || mons_holiness(monster->type) == MH_DEMONIC)
        {
            simple_monster_message(monster, " convulses!");

            hurt_monster( monster, roll_dice( 2, 15 ) + (random2(pow) / 3) );

            if (monster->hit_points < 1)
            {
                monster_die(monster, KILL_YOU, 0);
                continue;
            }

            if (monster->speed_increment >= 25)
                monster->speed_increment -= 20;

            mons_add_ench(monster, ENCH_FEAR);
        }                       // end "if mons_holiness"
    }                           // end "for tu"
}                               // end holy_word()

// poisonous light passes right through invisible players
// and monsters, and so, they are unaffected by this spell --
// assumes only you can cast this spell (or would want to)
void cast_toxic_radiance(void)
{
    struct monsters *monster;

    mpr("You radiate a sickly green light!");

    show_green = GREEN;
    viewwindow(1, false);
    more();
    mesclr();

    // determine whether the player is hit by the radiance: {dlb}
    if (you.invis)
    {
        mpr("The light passes straight through your body.");
    }
    else if (!player_res_poison())
    {
        mpr("You feel rather sick.");
        poison_player(2);
    }

    // determine which monsters are hit by the radiance: {dlb}
    for (int toxy = 0; toxy < MAX_MONSTERS; toxy++)
    {
        monster = &menv[toxy];

        if (monster->type != -1 && mons_near(monster))
        {
            if (!mons_has_ench(monster, ENCH_INVIS))
            {
                poison_monster(monster, true);

                if (coinflip()) // 50-50 chance for a "double hit" {dlb}
                    poison_monster(monster, true);

            }
            else if (player_see_invis())
            {
                // message player re:"miss" where appropriate {dlb}
                strcpy(info, "The light passes through ");
                strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
                strcat(info, ".");
                mpr(info);
            }
        }
    }
}                               // end cast_toxic_radiance()

void cast_refrigeration(int pow)
{
    struct monsters *monster = 0;       // NULL {dlb}
    int hurted = 0;
    struct bolt beam;
    int toxy;

    beam.flavour = BEAM_COLD;

    const dice_def  dam_dice( 3, 5 + pow / 10 );

    mpr("The heat is drained from your surroundings.");

    show_green = LIGHTCYAN;
    viewwindow(1, false);
    more();
    mesclr();

    // Do the player:
    hurted = roll_dice( dam_dice );
    hurted = check_your_resists( hurted, beam.flavour );

    if (hurted > 0)
    {
        mpr("You feel very cold.");
        ouch( hurted, 0, KILLED_BY_FREEZING );

        // Note: this used to be 12!... and it was also applied even if 
        // the player didn't take damage from the cold, so we're being 
        // a lot nicer now.  -- bwr
        scrolls_burn( 5, OBJ_POTIONS );
    }

    // Now do the monsters:
    for (toxy = 0; toxy < MAX_MONSTERS; toxy++)
    {
        monster = &menv[toxy];

        if (monster->type == -1)
            continue;

        if (mons_near(monster))
        {
            snprintf( info, INFO_SIZE, "You freeze %s.", 
                      ptr_monam( monster, DESC_NOCAP_THE ));

            mpr(info);

            hurted = roll_dice( dam_dice );
            hurted = mons_adjust_flavoured( monster, beam, hurted );

            if (hurted > 0)
            {
                hurt_monster( monster, hurted );

                if (monster->hit_points < 1)
                    monster_die(monster, KILL_YOU, 0);
                else
                {
                    print_wounds(monster);

                    //jmf: "slow snakes" finally available
                    if (mons_flag( monster->type, M_COLD_BLOOD ) && coinflip())
                        mons_add_ench(monster, ENCH_SLOW);
                }
            }
        }
    }
}                               // end cast_refrigeration()

void drain_life(int pow)
{
    int hp_gain = 0;
    int hurted = 0;
    struct monsters *monster = 0;       // NULL {dlb}

    mpr("You draw life from your surroundings.");

    // Incoming power to this function is skill in INVOCATIONS, so
    // we'll add an assert here to warn anyone who tries to use
    // this function with spell level power.
    ASSERT( pow <= 27 );

    show_green = DARKGREY;
    viewwindow(1, false);
    more();
    mesclr();

    for (int toxy = 0; toxy < MAX_MONSTERS; toxy++)
    {
        monster = &menv[toxy];

        if (monster->type == -1)
            continue;

        if (mons_holiness( monster->type ) != MH_NATURAL)
            continue;

        if (mons_res_negative_energy( monster ))
            continue;

        if (mons_near(monster))
        {
            strcpy(info, "You draw life from ");
            strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
            strcat(info, ".");
            mpr(info);

            hurted = 3 + random2(7) + random2(pow);

            hurt_monster(monster, hurted);

            hp_gain += hurted;

            if (monster->hit_points < 1)
                monster_die(monster, KILL_YOU, 0);
            else
                print_wounds(monster);
        }
    }

    hp_gain /= 2;

    if (hp_gain > pow * 2)
        hp_gain = pow * 2;

    if (hp_gain)
    {
        mpr( "You feel life flooding into your body." );
        inc_hp( hp_gain, false );
    }
}                               // end drain_life()

int vampiric_drain(int pow)
{
    int inflicted = 0;
    int mgr = 0;
    struct monsters *monster = 0;       // NULL
    struct dist vmove;

  dirc:
    mpr("Which direction?", MSGCH_PROMPT);
    direction( vmove, DIR_DIR, TARG_ENEMY );

    if (!vmove.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return -1;
    }

    mgr = mgrd[you.x_pos + vmove.dx][you.y_pos + vmove.dy];

    if (vmove.dx == 0 && vmove.dy == 0)
    {
        mpr("You can't do that.");
        goto dirc;
    }

    if (mgr == NON_MONSTER)
    {
        mpr("There isn't anything there!");
        return -1;
    }

    monster = &menv[mgr];

    const int holy = mons_holiness(monster->type);

    if (holy == MH_UNDEAD || holy == MH_DEMONIC)
    {
        mpr("Aaaarggghhhhh!");
        dec_hp(random2avg(39, 2) + 10, false);
        return -1;
    }

    if (mons_res_negative_energy( monster ))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return -1;
    }

    // The practical maximum of this is about 25 (pow @ 100).  -- bwr
    inflicted = 3 + random2avg( 9, 2 ) + random2(pow) / 7;

    if (inflicted >= monster->hit_points)
        inflicted = monster->hit_points;

    if (inflicted >= you.hp_max - you.hp)
        inflicted = you.hp_max - you.hp;

    if (inflicted == 0)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return -1;
    }

    hurt_monster(monster, inflicted);

    strcpy(info, "You feel life coursing from ");
    strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
    strcat(info, " into your body!");
    mpr(info);

    print_wounds(monster);

    if (monster->hit_points < 1)
        monster_die(monster, KILL_YOU, 0);

    inc_hp(inflicted / 2, false);

    return 1;
}                               // end vampiric_drain()

// Note: this function is currently only used for Freeze. -- bwr
char burn_freeze(int pow, char flavour)
{
    int mgr = NON_MONSTER;
    struct monsters *monster = 0;       // NULL {dlb}
    struct dist bmove;

    if (pow > 25)
        pow = 25;

    while (mgr == NON_MONSTER)
    {
        mpr("Which direction?", MSGCH_PROMPT);
        direction( bmove, DIR_DIR, TARG_ENEMY );

        if (!bmove.isValid)
        {
            canned_msg(MSG_SPELL_FIZZLES);
            return -1;
        }

        if (bmove.isMe)
        {
            canned_msg(MSG_UNTHINKING_ACT);
            return -1;
        }

        mgr = mgrd[you.x_pos + bmove.dx][you.y_pos + bmove.dy];

        // Yes, this is strange, but it does maintain the original behaviour
        if (mgr == NON_MONSTER)
        {
            mpr("There isn't anything close enough!");
            return -1;
        }
    }

    monster = &menv[mgr];

    strcpy(info, "You ");
    strcat(info, (flavour == BEAM_FIRE)        ? "burn" :
                 (flavour == BEAM_COLD)        ? "freeze" :
                 (flavour == BEAM_MISSILE)     ? "crush" :
                 (flavour == BEAM_ELECTRICITY) ? "zap"
                                               : "______");

    strcat(info, " ");
    strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
    strcat(info, ".");
    mpr(info);

    int hurted = roll_dice( 1, 3 + pow / 3 ); 

    struct bolt beam;

    beam.flavour = flavour;

    if (flavour != BEAM_MISSILE)
        hurted = mons_adjust_flavoured(monster, beam, hurted);

    if (hurted)
    {
        hurt_monster(monster, hurted);

        if (monster->hit_points < 1)
            monster_die(monster, KILL_YOU, 0);
        else
        {
            print_wounds(monster);

            if (flavour == BEAM_COLD)
            {
                if (mons_flag( monster->type, M_COLD_BLOOD ) && coinflip())
                    mons_add_ench(monster, ENCH_SLOW);

                const int cold_res = mons_res_cold( monster );
                if (cold_res <= 0)
                {
                    const int stun = (1 - cold_res) * random2( 2 + pow / 5 );
                    monster->speed_increment -= stun;
                }
            }
        }
    }

    return 1;
}                               // end burn_freeze()

// 'unfriendly' is percentage chance summoned elemental goes
//              postal on the caster (after taking into account
//              chance of that happening to unskilled casters
//              anyway)
int summon_elemental(int pow, unsigned char restricted_type,
                     unsigned char unfriendly)
{
    int type_summoned = MONS_PROGRAM_BUG;       // error trapping {dlb}
    char summ_success = 0;
    struct dist smove;

    int dir_x;
    int dir_y;
    int targ_x;
    int targ_y;

    int numsc = ENCH_ABJ_II + (random2(pow) / 5);

    if (numsc > ENCH_ABJ_VI)
        numsc = ENCH_ABJ_VI;

    for (;;) 
    {
        mpr("Summon from material in which direction?", MSGCH_PROMPT);

        direction( smove, DIR_DIR );

        if (!smove.isValid)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return (-1);
        }

        dir_x  = smove.dx;
        dir_y  = smove.dy;
        targ_x = you.x_pos + dir_x;
        targ_y = you.y_pos + dir_y;

        if (mgrd[ targ_x ][ targ_y ] != NON_MONSTER)
            mpr("Not there!");
        else if (dir_x == 0 && dir_y == 0)
            mpr("You can't summon an elemental from yourself!");
        else 
            break;
    }

    if (grd[ targ_x ][ targ_y ] == DNGN_ROCK_WALL
        && (restricted_type == 0 || restricted_type == MONS_EARTH_ELEMENTAL))
    {
        type_summoned = MONS_EARTH_ELEMENTAL;

        if (targ_x > 6 && targ_x < 74 && targ_y > 6 && targ_y < 64)
            grd[ targ_x ][ targ_y ] = DNGN_FLOOR;
    }
    else if ((env.cgrid[ targ_x ][ targ_y ] != EMPTY_CLOUD
            && (env.cloud[env.cgrid[ targ_x ][ targ_y ]].type == CLOUD_FIRE
             || env.cloud[env.cgrid[ targ_x ][ targ_y ]].type == CLOUD_FIRE_MON))
        && (restricted_type == 0 || restricted_type == MONS_FIRE_ELEMENTAL))
    {
        type_summoned = MONS_FIRE_ELEMENTAL;
        delete_cloud( env.cgrid[ targ_x ][ targ_y ] );
    }
    else if ((grd[ targ_x ][ targ_y ] == DNGN_LAVA)
        && (restricted_type == 0 || restricted_type == MONS_FIRE_ELEMENTAL))
    {
        type_summoned = MONS_FIRE_ELEMENTAL;
    }
    else if ((grd[ targ_x ][ targ_y ] == DNGN_DEEP_WATER
            || grd[ targ_x ][ targ_y ] == DNGN_SHALLOW_WATER
            || grd[ targ_x ][ targ_y ] == DNGN_BLUE_FOUNTAIN)
        && (restricted_type == 0 || restricted_type == MONS_WATER_ELEMENTAL))
    {
        type_summoned = MONS_WATER_ELEMENTAL;
    }
    else if ((grd[ targ_x ][ targ_y ] >= DNGN_FLOOR
            && env.cgrid[ targ_x ][ targ_y ] == EMPTY_CLOUD)
        && (restricted_type == 0 || restricted_type == MONS_AIR_ELEMENTAL))
    {
        type_summoned = MONS_AIR_ELEMENTAL;
    }

    // found something to summon
    if (type_summoned == MONS_PROGRAM_BUG)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (-1);
    }

    // silly - ice for water? 15jan2000 {dlb}
    // little change here to help with the above... and differentiate
    // elements a bit... {bwr}
    // - Water elementals are now harder to be made reliably friendly
    // - Air elementals are harder because they're more dynamic/dangerous
    // - Earth elementals are more static and easy to tame (as before)
    // - Fire elementals fall in between the two (10 is still fairly easy)
    if ((type_summoned == MONS_FIRE_ELEMENTAL
            && random2(10) >= you.skills[SK_FIRE_MAGIC])

        || (type_summoned == MONS_WATER_ELEMENTAL
            && random2((you.species == SP_MERFOLK) ? 5 : 15)
                           >= you.skills[SK_ICE_MAGIC])

        || (type_summoned == MONS_AIR_ELEMENTAL
            && random2(15) >= you.skills[SK_AIR_MAGIC])

        || (type_summoned == MONS_EARTH_ELEMENTAL
            && random2(5)  >= you.skills[SK_EARTH_MAGIC])

        || random2(100) < unfriendly)
    {
        summ_success = create_monster( type_summoned, numsc, BEH_HOSTILE,
                                       targ_x, targ_y, MHITYOU, 250 );

        if (summ_success >= 0)
            mpr( "The elemental doesn't seem to appreciate being summoned." );
    }
    else
    {
        summ_success = create_monster( type_summoned, numsc, BEH_FRIENDLY,
                                       targ_x, targ_y, you.pet_target, 250 );
    }

    return (summ_success >= 0);
}                               // end summon_elemental()

//jmf: beefed up higher-level casting of this (formerly lame) spell
void summon_small_mammals(int pow)
{
    int thing_called = MONS_PROGRAM_BUG;        // error trapping{dlb}

    int pow_spent = 0;
    int pow_left = pow + 1;
    int summoned = 0;
    int summoned_max = pow / 16;

    if (summoned_max > 5)
        summoned_max = 5;
    if (summoned_max < 1)
        summoned_max = 1;

    while (pow_left > 0 && summoned < summoned_max)
    {
        summoned++;
        pow_spent = 1 + random2(pow_left);
        pow_left -= pow_spent;

        switch (pow_spent)
        {
        case 75:
        case 74:
        case 38:
            thing_called = MONS_ORANGE_RAT;
            break;

        case 65:
        case 64:
        case 63:
        case 27:
        case 26:
        case 25:
            thing_called = MONS_GREEN_RAT;
            break;

        case 57:
        case 56:
        case 55:
        case 54:
        case 53:
        case 52:
        case 20:
        case 18:
        case 16:
        case 14:
        case 12:
        case 10:
            thing_called = coinflip() ? MONS_QUOKKA : MONS_GREY_RAT;
            break;

        default:
            thing_called = coinflip() ? MONS_GIANT_BAT : MONS_RAT;
            break;
        }

        create_monster( thing_called, ENCH_ABJ_III, BEH_FRIENDLY,
                        you.x_pos, you.y_pos, you.pet_target, 250 );
    }
}                               // end summon_small_mammals()

void summon_scorpions(int pow)
{
    int numsc = 1 + random2(pow) / 10 + random2(pow) / 10;

    numsc = stepdown_value(numsc, 2, 2, 6, 8);  //see stuff.cc - 12jan2000 {dlb}

    for (int scount = 0; scount < numsc; scount++)
    {
        if (random2(pow) <= 3)
        {
            if (create_monster( MONS_SCORPION, ENCH_ABJ_III, BEH_HOSTILE,
                                you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
            {
                mpr("A scorpion appears. It doesn't look very happy.");
            }
        }
        else
        {
            if (create_monster( MONS_SCORPION, ENCH_ABJ_III, BEH_FRIENDLY,
                                you.x_pos, you.y_pos, 
                                you.pet_target, 250 ) != -1)
            {
                mpr("A scorpion appears.");
            }
        }
    }
}                               // end summon_scorpions()

void summon_ice_beast_etc(int pow, int ibc)
{
    int numsc = ENCH_ABJ_II + (random2(pow) / 4);
    int beha = BEH_FRIENDLY;

    if (numsc > ENCH_ABJ_VI)
        numsc = ENCH_ABJ_VI;

    switch (ibc)
    {
    case MONS_ICE_BEAST:
        mpr("A chill wind blows around you.");
        break;

    case MONS_IMP:
        mpr("A beastly little devil appears in a puff of flame.");
        break;

    case MONS_WHITE_IMP:
        mpr("A beastly little devil appears in a puff of frigid air.");
        break;

    case MONS_SHADOW_IMP:
        mpr("A shadowy apparition takes form in the air.");
        break;

    case MONS_ANGEL:
        mpr("You open a gate to the realm of Zin!");
        break;

    case MONS_DAEVA:
        mpr("You are momentarily dazzled by a brilliant golden light.");
        break;

    default:
        mpr("A demon appears!");
        if (random2(pow) < 4)
        {
            beha = BEH_HOSTILE;
            mpr("It doesn't look very happy.");
        }
        break;

    }

    create_monster( ibc, numsc, beha, you.x_pos, you.y_pos, MHITYOU, 250 );
}                               // end summon_ice_beast_etc()

bool summon_swarm( int pow, bool unfriendly, bool god_gift )
{
    int thing_called = MONS_PROGRAM_BUG;        // error trapping {dlb}
    int numsc = 2 + random2(pow) / 10 + random2(pow) / 25;
    bool summoned = false;

    // see stuff.cc - 12jan2000 {dlb}
    numsc = stepdown_value( numsc, 2, 2, 6, 8 );

    for (int scount = 0; scount < numsc; scount++)
    {
        switch (random2(14))
        {
        case 0:
        case 1:         // prototypical swarming creature {dlb}
            thing_called = MONS_KILLER_BEE;
            break;

        case 2:         // comment said "larva", code read scorpion {dlb}
            thing_called = MONS_SCORPION;
            break;              // think: "The Arrival" {dlb}

        case 3:         //jmf: technically not insects but still cool
            thing_called = MONS_WORM;
            break;              // but worms kinda "swarm" so s'ok {dlb}

        case 4:         // comment read "larva", code was for scorpion
            thing_called = MONS_GIANT_MOSQUITO;
            break;              // changed into giant mosquito 12jan2000 {dlb}

        case 5:         // think: scarabs in "The Mummy" {dlb}
            thing_called = MONS_GIANT_BEETLE;
            break;

        case 6:         //jmf: blowfly instead of queen bee
            thing_called = MONS_GIANT_BLOWFLY;
            break;

            // queen bee added if more than x bees in swarm? {dlb}
            // the above would require code rewrite - worth it? {dlb}

        case 8:         //jmf: changed to red wasp; was wolf spider
            thing_called = MONS_WOLF_SPIDER;    //jmf: spiders aren't insects
            break;              // think: "Kingdom of the Spiders" {dlb}
            // not just insects!!! - changed back {dlb}

        case 9:
            thing_called = MONS_BUTTERFLY;      // comic relief? {dlb}
            break;

        case 10:                // change into some kind of snake -- {dlb}
            thing_called = MONS_YELLOW_WASP;    // do wasps swarm? {dlb}
            break;              // think: "Indiana Jones" and snakepit? {dlb}

        default:                // 3 in 14 chance, 12jan2000 {dlb}
            thing_called = MONS_GIANT_ANT;
            break;
        }                       // end switch

        int behaviour = BEH_HOSTILE;  // default to unfriendly

        // Note: friendly, non-god_gift means spell.
        if (god_gift)
            behaviour = BEH_GOD_GIFT;
        else if (!unfriendly && random2(pow) > 7)
            behaviour = BEH_FRIENDLY;

        if (create_monster( thing_called, ENCH_ABJ_III, behaviour, 
                            you.x_pos, you.y_pos, MHITYOU, 250 ))
        {
            summoned = true;
        }
    }

    return (summoned);
}                               // end summon_swarm()

void summon_undead(int pow)
{
    int temp_rand = 0;
    int thing_called = MONS_PROGRAM_BUG;        // error trapping {dlb}

    int numsc = 1 + random2(pow) / 30 + random2(pow) / 30;
    numsc = stepdown_value(numsc, 2, 2, 6, 8);  //see stuff.cc {dlb}

    mpr("You call on the undead to aid you!");

    for (int scount = 0; scount < numsc; scount++)
    {
        temp_rand = random2(25);

        thing_called = ((temp_rand > 8) ? MONS_WRAITH :          // 64%
                        (temp_rand > 3) ? MONS_SPECTRAL_WARRIOR  // 20%
                                        : MONS_FREEZING_WRAITH); // 16%

        if (random2(pow) < 6)
        {
            if (create_monster( thing_called, ENCH_ABJ_V, BEH_HOSTILE,
                                you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
            {
                mpr("You sense a hostile presence.");
            }
        }
        else
        {
            if (create_monster( thing_called, ENCH_ABJ_V, BEH_FRIENDLY,
                                you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
            {
                mpr("An insubstantial figure forms in the air.");
            }
        }
    }                           // end for loop

    //jmf: Kiku sometimes deflects this
    if (!you.is_undead
        && !(you.religion == GOD_KIKUBAAQUDGHA
             && (!player_under_penance()
                 && you.piety >= 100 && random2(200) <= you.piety)))
    {
        disease_player( 25 + random2(50) );
    }
}                               // end summon_undead()

void summon_things( int pow )
{
    int big_things = 0;
    int numsc = 2 + (random2(pow) / 10) + (random2(pow) / 10);

    if (one_chance_in(3) && !lose_stat( STAT_INTELLIGENCE, 1, true ))
        mpr("Your call goes unanswered.");
    else
    {
        numsc = stepdown_value( numsc, 2, 2, 6, -1 );

        while (numsc > 2)
        {
            if (one_chance_in(4))
                break;

            numsc -= 2;
            big_things++;
        }

        if (numsc > 8)
            numsc = 8;

        if (big_things > 8)
            big_things = 8;

        while (big_things > 0)
        {
            create_monster( MONS_TENTACLED_MONSTROSITY, 0, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 );
            big_things--;
        }

        while (numsc > 0)
        {
            create_monster( MONS_ABOMINATION_LARGE, 0, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 );
            numsc--;
        }

        snprintf( info, INFO_SIZE, "Some Thing%s answered your call!", 
                  (numsc + big_things > 1) ? "s" : "" );

        mpr(info);
    }

    return;
}

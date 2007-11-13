/*
 *  File:       it_use3.cc
 *  Summary:    Functions for using some of the wackier inventory items.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>     6/13/99         BWR             Auto ID Channel staff
 *      <3>     5/22/99         BWR             SPWLD_POWER is now HP/13 - 3
 *      <2>     5/20/99         BWR             Capped SPWLD_POWER to +20
 *      <1>     -/--/--         LRH             Created
 */

#include "AppHdr.h"
#include "it_use3.h"

#include <cstdlib>
#include <string.h>

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "database.h"
#include "decks.h"
#include "direct.h"
#include "effects.h"
#include "fight.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "overmap.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "view.h"
#include "xom.h"

static bool ball_of_energy(void);
static bool ball_of_fixation(void);
static bool ball_of_seeing(void);
static bool box_of_beasts(void);
static bool disc_of_storms(void);
static bool efreet_flask(void);

void special_wielded()
{
    const int wpn = you.equip[EQ_WEAPON];
    const int old_plus = you.inv[wpn].plus;
    const int old_plus2 = you.inv[wpn].plus2;
    const char old_colour = you.inv[wpn].colour;
    bool makes_noise = false;

    switch (you.special_wield)
    {
    case SPWLD_SING:
    case SPWLD_NOISE:
    {
        makes_noise = (one_chance_in(20) && !silenced(you.x_pos, you.y_pos));
          
        if (makes_noise)
        {
            std::string msg;

            if (you.special_wield == SPWLD_SING)
            {
                msg = getSpeakString("Singing Sword");
                if (msg != "")
                {
                    // "Your Singing Sword" sounds disrespectful
                    // (as if there could be more than one!)
                    msg = replace_all(msg, "@Your_weapon@", "@The_weapon@");
                    msg = replace_all(msg, "@your_weapon@", "@the_weapon@");
                }
            }
            else // SPWLD_NOISE
            {
                msg = getSpeakString("noisy weapon");
                if (msg != "")
                {
                    msg = replace_all(msg, "@Your_weapon@", "Your @weapon@");
                    msg = replace_all(msg, "@your_weapon@", "your @weapon@");
                }
            }
            
            // set appropriate channel (will usually be TALK)
            msg_channel_type channel = MSGCH_TALK;

            // disallow anything with VISUAL in it;
            if (msg != "" && msg.find("VISUAL") != std::string::npos)
                msg = "";
                
            if (msg != "")
            {
                std::string param = "";
                std::string::size_type pos = msg.find(":");

                if (pos != std::string::npos)
                    param = msg.substr(0, pos);

                if (!param.empty())
                {
                    bool match = true;

                    if (param == "DANGER")
                        channel = MSGCH_DANGER;
                    else if (param == "WARN")
                        channel = MSGCH_WARN;
                    else if (param == "SOUND")
                        channel = MSGCH_SOUND;
                    else if (param == "PLAIN")
                        channel = MSGCH_PLAIN;
                    else if (param == "SPELL" || param == "ENCHANT")
                        msg = ""; // disallow these as well, channel stays TALK
                    else if (param != "TALK")
                        match = false;

                    if (match && msg != "")
                        msg = msg.substr(pos + 1);
                }
            }
            
            if (msg == "") // give default noises
            {
                if (you.special_wield == SPWLD_SING)
                    msg = "@The_weapon@ sings.";
                else
                {
                    channel = MSGCH_SOUND;
                    msg = "You hear a strange noise.";
                }
            }
            
            // replace weapon references
            msg = replace_all(msg, "@The_weapon@", "The @weapon@");
            msg = replace_all(msg, "@the_weapon@", "the @weapon@");
            msg = replace_all(msg, "@weapon@", you.inv[wpn].name(DESC_BASENAME));

            mpr(msg.c_str(), channel);

        } // makes_noise
        break;
    } 
    
    case SPWLD_CURSE:
        if (one_chance_in(30))
            curse_an_item(false);
        break;

    case SPWLD_VARIABLE:
        do_uncurse_item( you.inv[wpn] );

        if (random2(5) < 2)     // 40% chance {dlb}
            you.inv[wpn].plus  += (coinflip() ? +1 : -1);

        if (random2(5) < 2)     // 40% chance {dlb}
            you.inv[wpn].plus2 += (coinflip() ? +1 : -1);

        if (you.inv[wpn].plus < -4)
            you.inv[wpn].plus = -4;
        else if (you.inv[wpn].plus > 16)
            you.inv[wpn].plus = 16;

        if (you.inv[wpn].plus2 < -4)
            you.inv[wpn].plus2 = -4;
        else if (you.inv[wpn].plus2 > 16)
            you.inv[wpn].plus2 = 16;

        you.inv[wpn].colour = random_colour();
        break;

    case SPWLD_TORMENT:
        if (one_chance_in(200))
        {
            torment( TORMENT_SPWLD, you.x_pos, you.y_pos );
            did_god_conduct( DID_UNHOLY, 1 );
        }
        break;

    case SPWLD_ZONGULDROK:
        if (one_chance_in(5))
        {
            animate_dead( 1 + random2(3), BEH_HOSTILE, MHITYOU, 1 );
            did_god_conduct( DID_NECROMANCY, 1 );
        }
        break;

    case SPWLD_POWER:
        you.inv[wpn].plus = stepdown_value( -4 + (you.hp / 5), 4, 4, 4, 20 );
        you.inv[wpn].plus2 = you.inv[wpn].plus;
        break;

    case SPWLD_OLGREB:
        // Giving Olgreb's staff a little lift since staves of poison have
        // been made better. -- bwr
        you.inv[wpn].plus = you.skills[SK_POISON_MAGIC] / 3;
        you.inv[wpn].plus2 = you.inv[wpn].plus;
        break;

    case SPWLD_WUCAD_MU:
        you.inv[wpn].plus  = ((you.intel > 25) ? 22 : you.intel - 3);
        you.inv[wpn].plus2 = ((you.intel > 25) ? 13 : you.intel / 2);
        break;

    case SPWLD_SHADOW:
        if (random2(8) <= player_spec_death())
        {
            did_god_conduct( DID_NECROMANCY, 1 );
            create_monster( MONS_SHADOW, 2, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 );
        }

        show_green = DARKGREY;
        break;

    //case SPWLD_PRUNE:
    default:
        return;
    }

    if (makes_noise)
        noisy( 25, you.x_pos, you.y_pos );

    if (old_plus != you.inv[wpn].plus
        || old_plus2 != you.inv[wpn].plus2
        || old_colour != you.inv[wpn].colour)
    {
        you.wield_change = true;
    }

    return;
}                               // end special_wielded()

static bool reaching_weapon_attack(const item_def& wpn)
{
    dist beam;

    mpr("Attack whom?", MSGCH_PROMPT);

    direction(beam, DIR_TARGET, TARG_ENEMY);

    if (!beam.isValid)
        return false;

    if (beam.isMe)
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    const int x_distance = abs(beam.tx - you.x_pos);
    const int y_distance = abs(beam.ty - you.y_pos);

    if (x_distance > 2 || y_distance > 2)
    {
        mpr("Your weapon cannot reach that far!");
        return false;
    }
    else if (mgrd[beam.tx][beam.ty] == NON_MONSTER)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters. Maybe we shouldn't do practice
        // here to prevent scumming...but that would just encourage
        // finding popcorn monsters.
        mpr("You attack empty space.");
        return true;
    }

    /* BCR - Added a check for monsters in the way.  Only checks cardinal
     *       directions.  Knight moves are ignored.  Assume the weapon
     *       slips between the squares.
     */

    // if we're attacking more than a space away
    if ((x_distance > 1) || (y_distance > 1))
    {
        const int x_middle = MAX(beam.tx, you.x_pos) - (x_distance / 2);
        const int y_middle = MAX(beam.ty, you.y_pos) - (y_distance / 2);

        // if either the x or the y is the same, we should check for
        // a monster:
        if (((beam.tx == you.x_pos) || (beam.ty == you.y_pos))
            && (mgrd[x_middle][y_middle] != NON_MONSTER))
        {
            const int skill = weapon_skill( wpn.base_type, wpn.sub_type );

            if ((5 + (3 * skill)) > random2(100))
            {
                mpr("You reach to attack!");
                you_attack(mgrd[beam.tx][beam.ty], false);
            }
            else
            {
                mpr("You could not reach far enough!");
                you_attack(mgrd[x_middle][y_middle], false);
            }
        }
        else
        {
            mpr("You reach to attack!");
            you_attack(mgrd[beam.tx][beam.ty], false);
        }
    }
    else
        you_attack(mgrd[beam.tx][beam.ty], false);

    return true;
}                               // end reaching_weapon_attack()

static bool evoke_horn_of_geryon()
{
    // Note: This assumes that the Vestibule has not been changed.
    bool rc = false;

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        mpr("You produce a weird and mournful sound.");

        for (int count_x = 0; count_x < GXM; count_x++)
        {
            for (int count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] == DNGN_STONE_ARCH)
                {
                    rc = true;

                    map_marker *marker =
                        env.markers.find(coord_def(count_x, count_y),
                                         MAT_FEATURE);

                    if (marker)
                    {
                        map_feature_marker *featm =
                            dynamic_cast<map_feature_marker*>(marker);
                        grd[count_x][count_y] = featm->feat;
                        env.markers.remove(marker);
                    }
                }
            }
        }

        if (rc)
            mpr("Your way has been unbarred.");
    }
    else
    {
        mpr("You produce a hideous howling noise!");
        create_monster( MONS_BEAST, 4, BEH_HOSTILE, you.x_pos, you.y_pos,
                        MHITYOU, 250, false, false, false, true );
    }
    return rc;
}

static bool evoke_sceptre_of_asmodeus()
{
    bool rc = true;
    if ( one_chance_in(21) )
        rc = false;
    else if (one_chance_in(2))
    {
        // summon devils, maybe a Fiend
        const monster_type mtype = (one_chance_in(4) ? MONS_FIEND :
                                    summon_any_demon(DEMON_COMMON));
        const bool good_summon = (create_monster( mtype, 6, BEH_HOSTILE,
                                                  you.x_pos, you.y_pos, 
                                                  MHITYOU, 250) != -1);

        if (good_summon)
        {
            if (mtype == MONS_FIEND)
                mpr("\"Your arrogance condemns you, mortal!\"");
            else
                mpr("The Sceptre summons one of its servants.");
        }
        else
            mpr("The air shimmers briefly.");
    }
    else
    {
        // Cast a destructive spell
        const spell_type spl = static_cast<spell_type>(
            random_choose_weighted( 114, SPELL_BOLT_OF_FIRE,
                                    57,  SPELL_LIGHTNING_BOLT,
                                    57,  SPELL_BOLT_OF_DRAINING,
                                    12,  SPELL_HELLFIRE,
                                    0 ));
        your_spells( spl, you.skills[SK_EVOCATIONS] * 8, false );
    }
    return true;
}

// returns true if item successfully evoked.
bool evoke_wielded()
{
    int power = 0;

    int pract = 0;
    bool did_work = false;  // used for default "nothing happens" message

    const int wield = you.equip[EQ_WEAPON];

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg( MSG_TOO_BERSERK );
        return (false);
    }
    else if (wield == -1)
    {
        mpr("You aren't wielding anything!");
        crawl_state.zero_turns_taken();
        return (false);
    }

    item_def& wpn = you.inv[wield];
    bool unevokable = false;

    // Check inscriptions.
    if ( !check_warning_inscriptions(wpn, OPER_EVOKE) )
        return false;

    switch (wpn.base_type)
    {
    case OBJ_WEAPONS:
        if (get_weapon_brand(wpn) == SPWPN_REACHING && enough_mp(1, false))
        {
            if ( reaching_weapon_attack(wpn) )
            {
                // needed a cost to prevent evocation training abuse -- bwr
                dec_mp(1);
                make_hungry( 50, false );  
                pract = (one_chance_in(5) ? 1 : 0);
                did_work = true;
            }
            else
                return false;
        }
        else if (is_fixed_artefact( wpn ))
        {
            switch (wpn.special)
            {
            case SPWPN_STAFF_OF_DISPATER:
                if (you.duration[DUR_DEATHS_DOOR] || !enough_hp(11, true) 
                    || !enough_mp(5, true))
                {
                    break;
                }

                mpr("You feel the staff feeding on your energy!");

                dec_hp( 5 + random2avg(19, 2), false, "Staff of Dispater" );
                dec_mp( 2 + random2avg(5, 2) );
                make_hungry( 100, false );

                power = you.skills[SK_EVOCATIONS] * 8;
                your_spells( SPELL_HELLFIRE, power, false );
                pract = (coinflip() ? 2 : 1);
                did_work = true;
                break;

            case SPWPN_SCEPTRE_OF_ASMODEUS:
                if ( evoke_sceptre_of_asmodeus() )
                {
                    make_hungry(200, false);
                    did_work = true;
                    pract = 1;
                }
                break;

            case SPWPN_STAFF_OF_OLGREB:
                if (!enough_mp( 4, true ) 
                    || you.skills[SK_EVOCATIONS] < random2(6))
                {
                    break;
                }

                dec_mp(4);
                make_hungry( 50, false );
                pract = 1;
                did_work = true;

                power = 10 + you.skills[SK_EVOCATIONS] * 8;

                your_spells( SPELL_OLGREBS_TOXIC_RADIANCE, power, false );

                if (you.skills[SK_EVOCATIONS] >= random2(10))
                    your_spells( SPELL_VENOM_BOLT, power, false );
                break;

            case SPWPN_STAFF_OF_WUCAD_MU:
                if (you.magic_points == you.max_magic_points
                    || you.skills[SK_EVOCATIONS] < random2(25))
                {
                    break;
                }

                mpr("Magical energy flows into your mind!");

                inc_mp( 3 + random2(5) + you.skills[SK_EVOCATIONS] / 3, false );
                make_hungry( 50, false );
                pract = 1;
                did_work = true;

                if (one_chance_in(3))
                {
                    miscast_effect( SPTYP_DIVINATION, random2(9), 
                                    random2(70), 100, "the Staff of Wucad Mu" );
                }
                break;

            default:
                unevokable = true;
                break;
            }
        }
        else
        {
            unevokable = true;
        }
        break;

    case OBJ_STAVES:
        if (item_is_rod( wpn ))
        {
            pract = staff_spell( wield );
            // [ds] Early exit, no turns are lost.
            if (pract == -1)
                return (false);

            did_work = true;  // staff_spell() will handle messages
        }
        else if (wpn.sub_type == STAFF_CHANNELING)
        {
            if (you.magic_points < you.max_magic_points 
                && you.skills[SK_EVOCATIONS] >= random2(30)) 
            {
                mpr("You channel some magical energy.");
                inc_mp( 1 + random2(3), false );
                make_hungry( 50, false );
                pract = (one_chance_in(5) ? 1 : 0);
                did_work = true;

                if (!item_type_known(wpn))
                {
                    set_ident_flags( wpn, ISFLAG_KNOW_TYPE );

                    mprf("You are wielding %s.",
                         wpn.name(DESC_NOCAP_A).c_str());
                         
                    more();

                    you.wield_change = true;
                }
            }
        }
        else
        {
            unevokable = true;
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items
        
        if ( is_deck(wpn) )
        {
            evoke_deck(wpn);
            pract = 1;
            break;
        }

        switch (wpn.sub_type)
        {
        case MISC_BOTTLED_EFREET:
            if (efreet_flask())
                pract = 2;
            break;

        case MISC_CRYSTAL_BALL_OF_SEEING:
            if (ball_of_seeing())
                pract = 1;
            break;

        case MISC_AIR_ELEMENTAL_FAN:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                summon_elemental(100, MONS_AIR_ELEMENTAL, 4);
                pract = (one_chance_in(5) ? 1 : 0);
            }
            break;

        case MISC_LAMP_OF_FIRE:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                summon_elemental(100, MONS_FIRE_ELEMENTAL, 4);
                pract = (one_chance_in(5) ? 1 : 0);
            }
            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                summon_elemental(100, MONS_EARTH_ELEMENTAL, 4);
                pract = (one_chance_in(5) ? 1 : 0);
            }
            break;

        case MISC_HORN_OF_GERYON:
            if ( evoke_horn_of_geryon() )
                pract = 1;
            break;

        case MISC_BOX_OF_BEASTS:
            if (box_of_beasts())
                pract = 1;
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (ball_of_energy())
                pract = 1;
            break;

        case MISC_CRYSTAL_BALL_OF_FIXATION:
            if (ball_of_fixation())
                pract = 1;
            break;

        case MISC_DISC_OF_STORMS:
            if (disc_of_storms())
                pract = (coinflip() ? 2 : 1);
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (pract > 0)
        exercise( SK_EVOCATIONS, pract );

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return (did_work);
}                               // end evoke_wielded()

static bool efreet_flask(void)
{
    const beh_type behaviour =
        ((you.skills[SK_EVOCATIONS] / 3 + 10 > random2(20)) 
         ? BEH_FRIENDLY : BEH_HOSTILE);

    mpr("You open the flask...");

    const int efreet = create_monster( MONS_EFREET, 0, behaviour, 
                                       you.x_pos, you.y_pos, MHITYOU, 250,
                                       false, false, true );
    if (efreet != -1)
    {
        monsters *mon = &menv[efreet];
        
        mpr( "...and a huge efreet comes out." );
        player_angers_monster(mon);
        mpr( (mon->attitude == ATT_FRIENDLY)?
             "\"Thank you for releasing me!\""
             : "It howls insanely!" );
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );    

    return (true);
}                               // end efreet_flask()

static bool ball_of_seeing(void)
{
    int use = 0;
    bool ret = false;

    mpr("You gaze into the crystal ball.");

    use = ((!you.duration[DUR_CONF]) ?
           random2(you.skills[SK_EVOCATIONS] * 6) : random2(5));

    if (use < 2)
    {
        lose_stat( STAT_INTELLIGENCE, 1, false, "using a ball of seeing");
    }
    else if (use < 5 && enough_mp(1, true))
    {
        mpr("You feel power drain from you!");
        set_mp(0, false);
    }
    else if (use < 10 || you.level_type == LEVEL_LABYRINTH)
    {
        if (you.level_type == LEVEL_LABYRINTH)
            mpr("You see a maze of twisty little passages, all alike.");
        confuse_player( 10 + random2(10), false );
    }
    else if (use < 15 || coinflip())
    {
        mpr("You see nothing.");
    }
    else if (magic_mapping( 15, 50 + random2( you.skills[SK_EVOCATIONS]), true))
    {
        mpr("You see a map of your surroundings!");
        ret = true;
    }
    else
    {
        mpr("You see nothing.");
    }

    return (ret);
}                               // end ball_of_seeing()

static bool disc_of_storms(void)
{
    int temp_rand = 0;          // probability determination {dlb}
    int disc_count = 0;

    const int fail_rate = (30 - you.skills[SK_EVOCATIONS]);
    bool ret = false;

    if (player_res_electricity() || (random2(100) < fail_rate))
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (random2(100) < fail_rate)
        mpr("The disc glows for a moment, then fades.");
    else if (random2(100) < fail_rate)
        mpr("Little bolts of electricity crackle over the disc.");
    else
    {
        mpr("The disc erupts in an explosion of electricity!");

        disc_count = roll_dice( 2, 1 + you.skills[SK_EVOCATIONS] / 7 );

        while (disc_count)
        {
            bolt beam;

            temp_rand = random2(3);

            zap_type which_zap = ((temp_rand > 1) ? ZAP_LIGHTNING :
                                  (temp_rand > 0) ? ZAP_ELECTRICITY
                                                  : ZAP_ORB_OF_ELECTRICITY);

            beam.source_x = you.x_pos;
            beam.source_y = you.y_pos;
            beam.target_x = you.x_pos + random2(13) - 6;
            beam.target_y = you.y_pos + random2(13) - 6;

            zapping( which_zap, 30 + you.skills[SK_EVOCATIONS] * 2, beam );

            disc_count--;
        }

        ret = true;
    }

    return (ret);
}                               // end disc_of_storms()

void tome_of_power(int slot)
{
    int powc = 5 + you.skills[SK_EVOCATIONS] 
                 + roll_dice( 5, you.skills[SK_EVOCATIONS] ); 

    msg::stream << "The book opens to a page covered in "
                << weird_writing() << '.' << std::endl;

    you.turn_is_over = true;

    if (!yesno("Read it?"))
        return;

    set_ident_flags( you.inv[slot], ISFLAG_KNOW_TYPE );

    if (you.mutation[MUT_BLURRY_VISION] > 0
        && random2(4) < you.mutation[MUT_BLURRY_VISION])
    {
        mpr("The page is too blurry for you to read.");
        return;
    }

    mpr("You find yourself reciting the magical words!");
    exercise( SK_EVOCATIONS, 1 ); 

    if ( random2(50) < 7 )
    {
        mpr("A cloud of weird smoke pours from the book's pages!");
        big_cloud( random_smoke_type(), KC_YOU,
                   you.x_pos, you.y_pos, 20, 10 + random2(8) );
        xom_is_stimulated(16);
    }
    else if ( random2(43) < 2 )
    {
        mpr("A cloud of choking fumes pours from the book's pages!");
        big_cloud(CLOUD_POISON, KC_YOU,
                  you.x_pos, you.y_pos, 20, 7 + random2(5));
        xom_is_stimulated(64);
    }
    else if ( random2(41) < 2 )
    {
        mpr("A cloud of freezing gas pours from the book's pages!");
        big_cloud(CLOUD_COLD, KC_YOU, you.x_pos, you.y_pos, 20, 8 + random2(5));
        xom_is_stimulated(64);
    }
    else if ( random2(39) < 3 )
    {
        if (one_chance_in(5))
        {
            mpr("The book disappears in a mighty explosion!");
            dec_inv_item_quantity( slot, 1 );
        }

        bolt beam;
        beam.type = SYM_BURST;
        beam.damage = dice_def( 3, 15 );
        // unsure about this    // BEAM_EXPLOSION instead? [dlb]
        beam.flavour = BEAM_FIRE;
        beam.target_x = you.x_pos;
        beam.target_y = you.y_pos;
        beam.name = "fiery explosion";
        beam.colour = RED;
        // your explosion, (not someone else's explosion)
        beam.beam_source = NON_MONSTER;
        beam.thrower = KILL_YOU;
        beam.aux_source = "an exploding tome of Destruction";
        beam.ex_size = 2;
        beam.is_tracer = false;
        beam.is_explosion = true;

        explosion(beam);
        xom_is_stimulated(255);
    }
    else if (one_chance_in(36))
    {
        if (create_monster( MONS_ABOMINATION_SMALL, 6, BEH_HOSTILE,
                            you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
        {
            mpr("A horrible Thing appears!");
            mpr("It doesn't look too friendly.");
        }
        xom_is_stimulated(255);
    }
    else
    {
        viewwindow(1, false);

        int temp_rand = random2(23) + random2(you.skills[SK_EVOCATIONS] / 3);
        
        if (temp_rand > 25)
            temp_rand = 25;

        const spell_type spell_casted =
            ((temp_rand > 24) ? SPELL_LEHUDIBS_CRYSTAL_SPEAR :
             (temp_rand > 21) ? SPELL_BOLT_OF_FIRE :
             (temp_rand > 18) ? SPELL_BOLT_OF_COLD :
             (temp_rand > 16) ? SPELL_LIGHTNING_BOLT :
             (temp_rand > 10) ? SPELL_FIREBALL :
             (temp_rand >  9) ? SPELL_VENOM_BOLT :
             (temp_rand >  8) ? SPELL_BOLT_OF_DRAINING :
             (temp_rand >  7) ? SPELL_BOLT_OF_INACCURACY :
             (temp_rand >  6) ? SPELL_STICKY_FLAME :
             (temp_rand >  5) ? SPELL_TELEPORT_SELF :
             (temp_rand >  4) ? SPELL_CIGOTUVIS_DEGENERATION :
             (temp_rand >  3) ? SPELL_POLYMORPH_OTHER :
             (temp_rand >  2) ? SPELL_MEPHITIC_CLOUD :
             (temp_rand >  1) ? SPELL_THROW_FLAME :
             (temp_rand >  0) ? SPELL_THROW_FROST
                              : SPELL_MAGIC_DART);

        your_spells( spell_casted, powc, false );
    }
}

void skill_manual(int slot)
{
    // Removed confirmation request because you know it's
    // a manual in advance.
    you.turn_is_over = true;
    item_def& manual(you.inv[slot]);
    set_ident_flags( manual, ISFLAG_KNOW_TYPE );
    const int skill = manual.plus;

    mprf("You read about %s.", skill_name(skill));

    exercise(skill, 500);

    if (one_chance_in(10))
    {
        mpr("The manual crumbles into dust.");
        dec_inv_item_quantity( slot, 1 );
    }
    else
    {
        mpr("The manual looks somewhat more worn.");
    }
    xom_is_stimulated(14);
}

static bool box_of_beasts()
{
    int beasty = MONS_PROGRAM_BUG;      // error trapping {dlb}
    int temp_rand = 0;          // probability determination {dlb}

    int ret = false;

    mpr("You open the lid...");

    if (random2(100) < 60 + you.skills[SK_EVOCATIONS])
    {
        temp_rand = random2(11);

        beasty = ((temp_rand == 0) ? MONS_GIANT_BAT :
                  (temp_rand == 1) ? MONS_HOUND :
                  (temp_rand == 2) ? MONS_JACKAL :
                  (temp_rand == 3) ? MONS_RAT :
                  (temp_rand == 4) ? MONS_ICE_BEAST :
                  (temp_rand == 5) ? MONS_SNAKE :
                  (temp_rand == 6) ? MONS_YAK :
                  (temp_rand == 7) ? MONS_BUTTERFLY :
                  (temp_rand == 8) ? MONS_HELL_HOUND :
                  (temp_rand == 9) ? MONS_BROWN_SNAKE 
                                   : MONS_GIANT_LIZARD);

        beh_type beh = (one_chance_in(you.skills[SK_EVOCATIONS] + 5)
                        ? BEH_HOSTILE : BEH_FRIENDLY);

        if (create_monster( beasty, 2 + random2(4), beh,
                            you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
        {
            mpr("...and something leaps out!");
            xom_is_stimulated(14);
            ret = true;
        }
    }
    else
    {
        if (!one_chance_in(6))
            mpr("...but nothing happens.");
        else
        {
            mpr("...but the box appears empty.");
            you.inv[you.equip[EQ_WEAPON]].sub_type = MISC_EMPTY_EBONY_CASKET;
        }
    }

    return (ret);
}                               // end box_of_beasts()

static bool ball_of_energy(void)
{
    int use = 0;
    int proportional = 0;

    bool ret = false;

    mpr("You gaze into the crystal ball.");

    use = ((!you.duration[DUR_CONF]) ? random2(you.skills[SK_EVOCATIONS] * 6) : random2(6));

    if (use < 2 || you.max_magic_points == 0)
    {
        lose_stat(STAT_INTELLIGENCE, 1, false, "using a ball of energy");
    }
    else if ((use < 4 && enough_mp(1, true))
             || you.magic_points == you.max_magic_points)
    {
        mpr( "You feel your power drain away!" );
        set_mp( 0, false );
    }
    else if (use < 6)
    {
        confuse_player( 10 + random2(10), false );
    }
    else
    {
        proportional = you.magic_points * 100;
        proportional /= you.max_magic_points;

        if (random2avg(77 - you.skills[SK_EVOCATIONS] * 2, 4) > proportional 
            || one_chance_in(25))
        {
            mpr( "You feel your power drain away!" );
            set_mp( 0, false );
        }
        else
        {
            mpr( "You are suffused with power!" );
            inc_mp( 6 + roll_dice( 2, you.skills[SK_EVOCATIONS] ), false );

            ret = true;
        }
    }

    return (ret);
}

static bool ball_of_fixation(void)
{
    mpr("You gaze into the crystal ball.");
    mpr("You are mesmerised by a rainbow of scintillating colours!");

    const int duration = random_range(15, 40);
    you.duration[DUR_PARALYSIS] = duration;
    you.duration[DUR_SLOW] = duration;

    return (true);
}

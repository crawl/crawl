/*
 *  File:       spells1.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>      06-mar-2000    bwr             confusing_touch, sure_blade
 *      <3>      9/11/99        LRH             Can't blink in the Abyss
 *      <3>      6/22/99        BWR             Removed teleport control from
 *                                              random_blink().
 *      <2>      5/20/99        BWR             Increased greatest healing.
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "spells1.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "cloud.h"
#include "direct.h"
#include "effects.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"

static bool abyss_blocks_teleport(bool cblink)
{
    // Lugonu worshippers get their perks.
    if (you.religion == GOD_LUGONU)
        return (false);

    // Controlled Blink (the spell) works quite reliably in the Abyss.
    return (cblink? one_chance_in(3) : !one_chance_in(3));
}

// If wizard_blink is set, all restriction are ignored (except for
// a monster being at the target spot), and the player gains no
// contamination.
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink)
{
    dist beam;

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return(1);
    }

    // yes, there is a logic to this ordering {dlb}:
    if (scan_randarts(RAP_PREVENT_TELEPORTATION) && !wizard_blink)
        mpr("You feel a weird sense of stasis.");
    else if (you.level_type == LEVEL_ABYSS
             && abyss_blocks_teleport(high_level_controlled_blink)
             && !wizard_blink)
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    else if (you.duration[DUR_CONF] && !wizard_blink)
        random_blink(false);
    else if (!allow_control_teleport(true) && !wizard_blink)
    {
        mpr("A powerful magic interferes with your control of the blink.");
        if (high_level_controlled_blink)
            return (cast_semi_controlled_blink(pow));
        random_blink(false);
    }
    else
    {
        // query for location {dlb}:
        for (;;)
        {
            direction(beam, DIR_TARGET, TARG_ANY, false, false, "Blink to where?");

            if (!beam.isValid || coord_def(beam.tx, beam.ty) == you.pos())
            {
                if (!wizard_blink &&
                    !yesno("Are you sure you want to cancel this blink?",
                           false, 'n'))
                {
                    mesclr();
                    continue;
                }
                canned_msg(MSG_OK);
                return (-1);         // early return {dlb}
            }

            // Wizard blink can move past translucent walls.
            if (see_grid_no_trans(beam.tx, beam.ty))
                break;
            else if (trans_wall_blocking( beam.tx, beam.ty ))
            {
                // Wizard blink can move past translucent walls.
                if (wizard_blink)
                    break;

                mesclr();
                mpr("You can't blink through translucent walls.");
            }
            else
            {
                mesclr();
                mpr("You can only blink to visible locations.");
            }
        }

        // Allow wizard blink to send player into walls, in case
        // the user wants to alter that grid to something else.
        if (grid_is_solid(grd[beam.tx][beam.ty]) && wizard_blink)
            grd[beam.tx][beam.ty] = DNGN_FLOOR;

        if (grid_is_solid(grd[beam.tx][beam.ty])
            || mgrd[beam.tx][beam.ty] != NON_MONSTER)
        {
            mpr("Oops! Maybe something was there already.");
            random_blink(false);
        }
        else if (you.level_type == LEVEL_ABYSS && !wizard_blink)
        {
            abyss_teleport( false );
            you.pet_target = MHITNOT;
        }
        else
        {
            // no longer held in net
            clear_trapping_net();
            
            move_player_to_grid(beam.tx, beam.ty, false, true, true);

            // controlling teleport contaminates the player -- bwr
            if (!wizard_blink)
                contaminate_player( 1 );
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 0 && !wizard_blink)
        {
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = 1;
        }
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return (1);
}                               // end blink()

void random_blink(bool allow_partial_control, bool override_abyss)
{
    int tx, ty;
    bool succ = false;

    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
        mpr("You feel a weird sense of stasis.");
    else if (you.level_type == LEVEL_ABYSS
             && !override_abyss && !one_chance_in(3))
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    else if (!random_near_space(you.x_pos, you.y_pos, tx, ty)
             && !random_near_space(you.x_pos, you.y_pos, tx, ty, true))
    {
        mpr("You feel jittery for a moment.");
    }

#ifdef USE_SEMI_CONTROLLED_BLINK
    //jmf: add back control, but effect is cast_semi_controlled_blink(pow)
    else if (player_control_teleport() && !you.duration[DUR_CONF]
             && allow_partial_control && allow_control_teleport())
    {
        mpr("You may select the general direction of your translocation.");
        cast_semi_controlled_blink(100);
        succ = true;
    }
#endif

    else
    {
        mpr("You blink.");
        
        // no longer held in net
        clear_trapping_net();

        succ = true;
        you.moveto(tx, ty);

        if (you.level_type == LEVEL_ABYSS)
        {
            abyss_teleport( false );
            you.pet_target = MHITNOT;
        }
    }

    if (succ && you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = 1;
    }

    return;
}                               // end random_blink()

int fireball(int power, bolt &beam)
{
    zapping(ZAP_FIREBALL, power, beam);
    return (1);
}                               // end fireball()

int cast_fire_storm(int powc, bolt &beam)
{
    beam.ex_size = 2 + (random2(powc) > 75);
    beam.flavour = BEAM_LAVA;
    beam.type = SYM_ZAP;
    beam.colour = RED;
    beam.beam_source = MHITYOU;
    beam.thrower = KILL_YOU_MISSILE;
    beam.aux_source.clear();
    beam.obvious_effect = false;
    beam.is_beam = false;
    beam.is_tracer = false;
    beam.is_explosion = true;
    beam.ench_power = powc;     // used for radius
    beam.name = "great blast of fire";
    beam.hit = 20 + powc / 10;
    beam.damage = calc_dice( 9, 20 + powc );

    explosion( beam );
    mpr("A raging storm of fire appears!");

    viewwindow(1, false);

    return (1);
}                               // end cast_fire_storm()


void cast_chain_lightning( int powc )
{
    struct bolt beam;

    // initialize beam structure
    beam.name = "lightning arc";
    beam.aux_source = "chain lightning";
    beam.beam_source = MHITYOU;
    beam.thrower = KILL_YOU_MISSILE;
    beam.range = 8;
    beam.rangeMax = 8;
    beam.hit = AUTOMATIC_HIT;
    beam.type = SYM_ZAP;
    beam.flavour = BEAM_ELECTRICITY;
    beam.obvious_effect = true;
    beam.is_beam = false;            // since we want to stop at our target
    beam.is_explosion = false;
    beam.is_tracer = false;

    int sx, sy;
    int tx, ty;
    int i;

    for (sx = you.x_pos, sy = you.y_pos; 
         powc > 0; 
         powc -= 8 + random2(13), sx = tx, sy = ty)
    {
        // infinity as far as this spell is concerned
        // (Range - 1) is used because the distance is randomized and
        // may be shifted by one.
        int min_dist = MONSTER_LOS_RANGE - 1;

        int dist;
        int count = 0;

        tx = -1;
        ty = -1;

        for (i = 0; i < MAX_MONSTERS; i++)
        {
            struct monsters *monster = &menv[i];

            if (monster->type == -1)
                continue;

            dist = grid_distance( sx, sy, monster->x, monster->y );

            // check for the source of this arc
            if (!dist) 
                continue;
                
            // randomize distance (arcs don't care about a couple of feet)
            dist += (random2(3) - 1);

            // always ignore targets further than current one 
            if (dist > min_dist)
                continue;

            if (!check_line_of_sight( sx, sy, monster->x, monster->y ))
                continue;

            count++;

            if (dist < min_dist)
            {
                // switch to looking for closer targets (but not always)
                if (!one_chance_in(10))
                {
                    min_dist = dist;
                    tx = monster->x;
                    ty = monster->y;
                    count = 0;
                }
            }
            else if (tx == -1 || one_chance_in( count ))
            {
                // either first target, or new selected target at min_dist
                tx = monster->x;
                ty = monster->y;
                
                // need to set min_dist for first target case
                if (dist < min_dist)
                    min_dist = dist;
            }
        }

        // now check if the player is a target:
        dist = grid_distance( sx, sy, you.x_pos, you.y_pos );

        if (dist)       // ie player was not the source
        {
            // distance randomized (as above)
            dist += (random2(3) - 1);

            // select player if only, closest, or randomly selected
            if ((tx == -1 
                    || dist < min_dist
                    || (dist == min_dist && one_chance_in( count + 1 )))
                && check_line_of_sight( sx, sy, you.x_pos, you.y_pos ))
            {
                tx = you.x_pos;
                ty = you.y_pos;
            }
        }

        const bool see_source = see_grid( sx, sy );
        const bool see_targ   = see_grid( tx, ty );

        if (tx == -1)
        {
            if (see_source)
                mpr( "The lightning grounds out." );

            break;
        }

        // Trying to limit message spamming here so we'll only mention
        // the thunder when it's out of LoS.
        if (noisy( 25, sx, sy ) && !see_source)
            mpr( "You hear a mighty clap of thunder!", MSGCH_SOUND );

        if (see_source && !see_targ)
            mpr( "The lightning arcs out of your line of sight!" );
        else if (!see_source && see_targ)
            mpr( "The lightning arc suddenly appears!" );

        if (!see_grid_no_trans( tx, ty ))
        {
            // It's no longer in the caster's LOS and influence.
            powc = powc / 2 + 1;
        }

        beam.source_x = sx;
        beam.source_y = sy;
        beam.target_x = tx;
        beam.target_y = ty;
        beam.colour = LIGHTBLUE;
        beam.damage = calc_dice( 5, 12 + powc * 2 / 3 );

        // Be kinder to the player
        if (tx == you.x_pos && ty == you.y_pos)
        {
            if (!(beam.damage.num /= 2))
                beam.damage.num = 1;
            if ((beam.damage.size /= 2) < 3)
                beam.damage.size = 3;
        }
        fire_beam( beam );
    }

    more();
}

void identify(int power)
{
    int id_used = 1;
    int item_slot;

    // scrolls of identify *may* produce "extra" identifications {dlb}:
    if (power == -1 && one_chance_in(5))
        id_used += (coinflip()? 1 : 2);

    do
    {
        item_slot = prompt_invent_item( "Identify which item?", MT_INVLIST,
                                        OSEL_UNIDENT, true, true, false );
        if (item_slot == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            return;
        }

        item_def& item(you.inv[item_slot]);

        if ( fully_identified(item) )
        {
            mpr("Choose an unidentified item, or Esc to abort.");
            if ( Options.auto_list )
                more();
            continue;
        }

        if ( !is_artefact(item) )
            set_ident_type( item.base_type, item.sub_type, ID_KNOWN_TYPE );

        set_ident_flags( item, ISFLAG_IDENT_MASK );
        
        // output identified item
        mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
        if (item_slot == you.equip[EQ_WEAPON])
            you.wield_change = true;

        id_used--;

        if (Options.auto_list && id_used > 0)
            more();
    }
    while (id_used > 0);
}                               // end identify()

// return whether the spell was actually cast
bool conjure_flame(int pow)
{
    struct dist spelld;

    bool done_first_message = false;

    for (;;)
    {
        if (done_first_message)
            mpr("Where would you like to place the cloud?", MSGCH_PROMPT);
        else 
        {
            mpr("You cast a flaming cloud spell! But where?", MSGCH_PROMPT);
            done_first_message = true;
        }

        direction( spelld, DIR_TARGET, TARG_ENEMY, false, false );

        if (!spelld.isValid)
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (trans_wall_blocking(spelld.tx, spelld.ty))
        {
            mpr("A translucent wall is in the way.");
            return false;
        }
        else if (!see_grid(spelld.tx, spelld.ty))
        {
            mpr("You can't see that place!");
            continue;
        }
        
        if (spelld.tx == you.x_pos && spelld.ty == you.y_pos)
        {
            mpr("You can't place the cloud here!");
            continue;
        }

        const int cloud = env.cgrid[spelld.tx][spelld.ty];

        if (grid_is_solid(grd[ spelld.tx ][ spelld.ty ]) ||
            mgrd[ spelld.tx ][ spelld.ty ] != NON_MONSTER ||
            (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE))
        {
            mpr( "There's already something there!" );
            continue;
        }
        else if ( cloud != EMPTY_CLOUD )
        {
            // reinforce the cloud - but not too much
            mpr( "The fire roars with new energy!" );
            const int extra_dur = 2 + std::min(random2(pow) / 2, 20);
            env.cloud[cloud].decay += extra_dur * 5;
            env.cloud[cloud].whose = KC_YOU;
            return true;
        }

        break;
    }

    int durat = 5 + (random2(pow) / 2) + (random2(pow) / 2);

    if (durat > 23)
        durat = 23;

    place_cloud( CLOUD_FIRE, spelld.tx, spelld.ty, durat, KC_YOU );
    return true;
}

int stinking_cloud( int pow, bolt &beem )
{
    beem.name = "ball of vapour";
    beem.colour = GREEN;
    beem.range = 6;
    beem.rangeMax = 6;
    beem.damage = dice_def( 1, 0 );
    beem.hit = 20;
    beem.type = SYM_ZAP;
    beem.flavour = BEAM_MMISSILE;
    beem.ench_power = pow;
    beem.beam_source = MHITYOU;
    beem.thrower = KILL_YOU;
    beem.aux_source.clear();
    beem.is_beam = false;
    beem.is_tracer = false;

    fire_beam(beem);

    return (1);
}                               // end stinking_cloud()

int cast_big_c(int pow, cloud_type cty, kill_category whose, bolt &beam)
{
    big_cloud( cty, whose,
               beam.target_x, beam.target_y, pow, 8 + random2(3) );
    return (1);
}                               // end cast_big_c()

void big_cloud(cloud_type cl_type, kill_category whose,
               int cl_x, int cl_y, int pow, int size, int spread_rate)
{
    apply_area_cloud(make_a_normal_cloud, cl_x, cl_y, pow, size,
                     cl_type, whose, spread_rate);
}                               // end big_cloud()

static int healing_spell( int healed )
{
    int mgr = 0;
    struct monsters *monster = 0;       // NULL {dlb}
    struct dist bmove;

    mpr("Which direction?", MSGCH_PROMPT);
    direction( bmove, DIR_DIR, TARG_FRIEND );

    if (!bmove.isValid)
    {
        canned_msg( MSG_OK );
        return 0;
    }

    mgr = mgrd[you.x_pos + bmove.dx][you.y_pos + bmove.dy];

    if (bmove.dx == 0 && bmove.dy == 0)
    {
        mpr("You are healed.");
        inc_hp(healed, false);
        return 1;
    }

    if (mgr == NON_MONSTER)
    {
        mpr("There isn't anything there!");
        return -1;
    }

    monster = &menv[mgr];

    if (heal_monster(monster, healed, false))
    {
        mprf("You heal %s.", monster->name(DESC_NOCAP_THE).c_str());

        if (monster->hit_points == monster->max_hit_points)
            simple_monster_message( monster, " is completely healed." );
        else
        {
            const monsters *mons = static_cast<const monsters*>(monster);
            print_wounds(mons);
        }
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return 1;
}                               // end healing_spell()

#if 0
char cast_lesser_healing( int pow )
{
    return healing_spell(5 + random2avg(7, 2));
}                               // end lesser healing()

char cast_greater_healing( int pow )
{
    return healing_spell(15 + random2avg(29, 2));
}                               // end cast_greater_healing()

char cast_greatest_healing( int pow )
{
    return healing_spell(50 + random2avg(49, 2));
}                               // end cast_greatest_healing()
#endif 

int cast_healing( int pow )
{
    if (pow > 50)
        pow = 50;

    return (healing_spell( pow + roll_dice( 2, pow ) - 2 )); 
}

bool cast_revivification(int power)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;
    int loss = 0;

    if (you.hp == you.hp_max)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (you.hp_max < 21)
        mpr("You lack the resilience to cast this spell.");
    else
    {
        mpr("Your body is healed in an amazingly painful way.");

        loss = 2;
        for (loopy = 0; loopy < 9; loopy++)
        {
            if (random2(power) < 8)
                loss++;
        }

        dec_max_hp( loss );
        set_hp( you.hp_max, false );
        success = true;
    }

    return (success);
}                               // end cast_revivification()

void cast_cure_poison(int mabil)
{
    if (!you.duration[DUR_POISONING])
        canned_msg(MSG_NOTHING_HAPPENS);
    else
        reduce_poison_player( 2 + random2(mabil) + random2(3) );

    return;
}                               // end cast_cure_poison()

void purification(void)
{
    mpr("You feel purified!");

    you.duration[DUR_POISONING] = 0;
    you.rotting = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.disease = 0;
    you.duration[DUR_PARALYSIS] = 0;          // can't currently happen -- bwr
}                               // end purification()

int allowed_deaths_door_hp(void)
{
    int hp = you.skills[SK_NECROMANCY] / 2;

    if (you.religion == GOD_KIKUBAAQUDGHA && !player_under_penance())
        hp += you.piety / 15;

    return (hp);
}

void cast_deaths_door(int pow)
{
    if (you.is_undead)
        mpr("You're already dead!");
    else if (you.duration[DUR_DEATHS_DOOR])
        mpr("Your appeal for an extension has been denied.");
    else
    {
        mpr("You feel invincible!");
        mpr("You seem to hear sand running through an hourglass...");

        set_hp( allowed_deaths_door_hp(), false );
        deflate_hp( you.hp_max, false );

        you.duration[DUR_DEATHS_DOOR] = 10 + random2avg(13, 3) + (random2(pow) / 10);

        if (you.duration[DUR_DEATHS_DOOR] > 25)
            you.duration[DUR_DEATHS_DOOR] = 23 + random2(5);
    }

    return;
}

// can't use beam variables here, because of monster_die and the puffs of smoke
void abjuration(int pow)
{
    struct monsters *monster = 0;       // NULL {dlb}

    mpr("Send 'em back where they came from!");

    // Scale power into something comparable to summon lifetime.
    const int abjdur = pow * 12;

    for (int ab = 0; ab < MAX_MONSTERS; ab++)
    {
        monster = &menv[ab];

        if (monster->type == -1 || !mons_near(monster))
            continue;

        if (mons_friendly(monster))
            continue;

        mon_enchant abj = monster->get_ench(ENCH_ABJ);
        if (abj.ench != ENCH_NONE)
        {
            const int sockage = std::max(fuzz_value(abjdur, 60, 30), 40);
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "%s abj: dur: %d, abj: %d",
                 monster->name(DESC_PLAIN).c_str(), abj.duration, sockage);
#endif
            if (!monster->lose_ench_duration(abj, sockage))
                simple_monster_message(monster, " shudders.");
        }
    }
}                               // end abjuration()

// Antimagic is sort of an anti-extension... it sets a lot of magical
// durations to 1 so it's very nasty at times (and potentially lethal,
// that's why we reduce levitation to 2, so that the player has a chance
// to stop insta-death... sure the others could lead to death, but that's
// not as direct as falling into deep water) -- bwr
void antimagic()
{
    duration_type dur_list[] = {
        DUR_INVIS, DUR_CONF, DUR_PARALYSIS, DUR_SLOW, DUR_HASTE,
        DUR_MIGHT, DUR_FIRE_SHIELD, DUR_ICY_ARMOUR, DUR_REPEL_MISSILES,
        DUR_REGENERATION, DUR_SWIFTNESS, DUR_STONEMAIL, DUR_CONTROL_TELEPORT,
        DUR_TRANSFORMATION, DUR_DEATH_CHANNEL, DUR_DEFLECT_MISSILES,
        DUR_FORESCRY, DUR_SEE_INVISIBLE, DUR_WEAPON_BRAND, DUR_SILENCE,
        DUR_CONDENSATION_SHIELD, DUR_STONESKIN, DUR_BARGAIN,
        DUR_INSULATION, DUR_RESIST_POISON, DUR_RESIST_FIRE, DUR_RESIST_COLD,
        DUR_SLAYING, DUR_STEALTH, DUR_MAGIC_SHIELD
    };
        
    if (you.duration[DUR_LEVITATION] > 2 && you.duration[DUR_LEVITATION] < 100)
        you.duration[DUR_LEVITATION] = 2;

    if (you.duration[DUR_CONTROLLED_FLIGHT] < 100 &&
        you.duration[DUR_CONTROLLED_FLIGHT] > 1) // don't mess with permaflight
        you.duration[DUR_CONTROLLED_FLIGHT] = 1;
    for ( unsigned int i = 0; i < ARRAYSIZE(dur_list); ++i )
        if ( you.duration[dur_list[i]] > 1 )
            you.duration[dur_list[i]] = 1;

    contaminate_player( -1 * (1+random2(5)));
}                               // end antimagic()

void extension(int pow)
{
    int contamination = random2(2);

    if (you.duration[DUR_HASTE])
    {
        potion_effect(POT_SPEED, pow);
        contamination++;
    }

    if (you.duration[DUR_SLOW])
        potion_effect(POT_SLOWING, pow);

#if 0
    if (you.duration[DUR_PARALYSIS])
        potion_effect(POT_PARALYSIS, pow);  // how did you cast extension?

    if (you.duration[DUR_CONF])
        potion_effect(POT_CONFUSION, pow);  // how did you cast extension?
#endif

    if (you.duration[DUR_MIGHT])
    {
        potion_effect(POT_MIGHT, pow);
        contamination++;  
    }

    if (you.duration[DUR_LEVITATION] && !you.duration[DUR_CONTROLLED_FLIGHT])
        potion_effect(POT_LEVITATION, pow);

    if (you.duration[DUR_INVIS])
    {
        potion_effect(POT_INVISIBILITY, pow);
        contamination++;
    }

    if (you.duration[DUR_ICY_ARMOUR])
        ice_armour(pow, true);

    if (you.duration[DUR_REPEL_MISSILES])
        missile_prot(pow);

    if (you.duration[DUR_REGENERATION])
        cast_regen(pow);

    if (you.duration[DUR_DEFLECT_MISSILES])
        deflection(pow);

    if (you.duration[DUR_FIRE_SHIELD])
    {
        you.duration[DUR_FIRE_SHIELD] += random2(pow / 20);

        if (you.duration[DUR_FIRE_SHIELD] > 50)
            you.duration[DUR_FIRE_SHIELD] = 50;

        mpr("Your ring of flames roars with new vigour!");
    }

    if ( !(you.duration[DUR_WEAPON_BRAND] < 1
                 || you.duration[DUR_WEAPON_BRAND] > 80) )
    {
        you.duration[DUR_WEAPON_BRAND] += 5 + random2(8);
    }

    if (you.duration[DUR_SWIFTNESS])
        cast_swiftness(pow);

    if (you.duration[DUR_INSULATION])
        cast_insulation(pow);

    if (you.duration[DUR_STONEMAIL])
        stone_scales(pow);

    if (you.duration[DUR_CONTROLLED_FLIGHT])
        cast_fly(pow);

    if (you.duration[DUR_CONTROL_TELEPORT])
        cast_teleport_control(pow);

    if (you.duration[DUR_RESIST_POISON])
        cast_resist_poison(pow);

    if (you.duration[DUR_TRANSFORMATION])
    {
        mpr("Your transformation has been extended.");
        you.duration[DUR_TRANSFORMATION] += random2(pow);
        if (you.duration[DUR_TRANSFORMATION] > 100)
            you.duration[DUR_TRANSFORMATION] = 100;
    }

    //jmf: added following
    if (you.duration[DUR_STONESKIN])
        cast_stoneskin(pow);

    if (you.duration[DUR_FORESCRY])
        cast_forescry(pow);

    if (you.duration[DUR_SEE_INVISIBLE])
        cast_see_invisible(pow);

    if (you.duration[DUR_SILENCE])   //how precisely did you cast extension?
        cast_silence(pow);

    if (you.duration[DUR_CONDENSATION_SHIELD])
        cast_condensation_shield(pow);

    if (contamination)
        contaminate_player( contamination );
}                               // end extension()

void ice_armour(int pow, bool extending)
{
    if (!player_light_armour())
    {
        if (!extending)
            mpr("You are wearing too much armour.");

        return;
    }

    if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
    {
        if (!extending)
            mpr("The spell conflicts with another spell still in effect.");

        return;
    }

    if (you.duration[DUR_ICY_ARMOUR])
        mpr( "Your icy armour thickens." );
    else 
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST)
            mpr( "Your icy body feels more resilient." );
        else
            mpr( "A film of ice covers your body!" );

        you.redraw_armour_class = 1;
    }     

    you.duration[DUR_ICY_ARMOUR] += 20 + random2(pow) + random2(pow);

    if (you.duration[DUR_ICY_ARMOUR] > 50)
        you.duration[DUR_ICY_ARMOUR] = 50;
}                               // end ice_armour()

void stone_scales(int pow)
{
    int dur_change = 0;

    if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
    {
        mpr("The spell conflicts with another spell still in effect.");
        return;
    }

    if (you.duration[DUR_STONEMAIL])
        mpr("Your scaly armour looks firmer.");
    else 
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
            mpr( "Your stone body feels more resilient." );
        else
            mpr( "A set of stone scales covers your body!" );

        you.redraw_evasion = 1;
        you.redraw_armour_class = 1;
    }     

    dur_change = 20 + random2(pow) + random2(pow);

    if (dur_change + you.duration[DUR_STONEMAIL] >= 100)
        you.duration[DUR_STONEMAIL] = 100;
    else
        you.duration[DUR_STONEMAIL] += dur_change;

    burden_change();
}                               // end stone_scales()

void missile_prot(int pow)
{
    mpr("You feel protected from missiles.");

    you.duration[DUR_REPEL_MISSILES] += 8 + roll_dice( 2, pow );

    if (you.duration[DUR_REPEL_MISSILES] > 100)
        you.duration[DUR_REPEL_MISSILES] = 100;
}                               // end missile_prot()

void deflection(int pow)
{
    mpr("You feel very safe from missiles.");

    you.duration[DUR_DEFLECT_MISSILES] += 15 + random2(pow);

    if (you.duration[DUR_DEFLECT_MISSILES] > 100)
        you.duration[DUR_DEFLECT_MISSILES] = 100;
}                               // end cast_deflection()

void cast_regen(int pow)
{
    //if (pow > 150) pow = 150;
    mpr("Your skin crawls.");

    you.duration[DUR_REGENERATION] += 5 + roll_dice( 2, pow / 3 + 1 );

    if (you.duration[DUR_REGENERATION] > 100)
        you.duration[DUR_REGENERATION] = 100;
}                               // end cast_regen()

void cast_berserk(void)
{
    go_berserk(true);
}                               // end cast_berserk()

void cast_swiftness(int power)
{
    int dur_incr = 0;

    if (player_in_water())
    {
        mpr("The water foams!");
        return;
    }

    if (!you.duration[DUR_SWIFTNESS] && player_movement_speed() <= 6)
    {
        mpr( "You can't move any more quickly." );
        return;
    }

    // Reduced the duration:  -- bwr
    // dur_incr = random2(power) + random2(power) + 20;
    dur_incr = 20 + random2( power );

    // [dshaligram] Removed the on-your-feet bit. Sounds odd when you're
    // levitating, for instance.
    mpr("You feel quick.");

    if (dur_incr + you.duration[DUR_SWIFTNESS] > 100)
        you.duration[DUR_SWIFTNESS] = 100;
    else
        you.duration[DUR_SWIFTNESS] += dur_incr;
}                               // end cast_swiftness()

void cast_fly(int power)
{
    int dur_change = 25 + random2(power) + random2(power);

    const bool was_levitating = player_is_levitating();

    if (you.duration[DUR_LEVITATION] + dur_change > 100)
        you.duration[DUR_LEVITATION] = 100;
    else
        you.duration[DUR_LEVITATION] += dur_change;

    if (you.duration[DUR_CONTROLLED_FLIGHT] + dur_change > 100)
        you.duration[DUR_CONTROLLED_FLIGHT] = 100;
    else
        you.duration[DUR_CONTROLLED_FLIGHT] += dur_change;

    burden_change();

    if (!was_levitating)
    {
        if (you.light_flight())
            mpr("You swoop lightly up into the air.");
        else
            mpr("You fly up into the air.");
    }
    else
        mpr("You feel more buoyant.");
}

void cast_insulation(int power)
{
    int dur_incr = 10 + random2(power);

    mpr("You feel insulated.");

    if (dur_incr + you.duration[DUR_INSULATION] > 100)
        you.duration[DUR_INSULATION] = 100;
    else
        you.duration[DUR_INSULATION] += dur_incr;
}                               // end cast_insulation()

void cast_resist_poison(int power)
{
    int dur_incr = 10 + random2(power);

    mpr("You feel resistant to poison.");

    if (dur_incr + you.duration[DUR_RESIST_POISON] > 100)
        you.duration[DUR_RESIST_POISON] = 100;
    else
        you.duration[DUR_RESIST_POISON] += dur_incr;
}                               // end cast_resist_poison()

void cast_teleport_control(int power)
{
    int dur_incr = 10 + random2(power);

    mpr("You feel in control.");

    if (dur_incr + you.duration[DUR_CONTROL_TELEPORT] >= 50)
        you.duration[DUR_CONTROL_TELEPORT] = 50;
    else
        you.duration[DUR_CONTROL_TELEPORT] += dur_incr;
}                               // end cast_teleport_control()

void cast_ring_of_flames(int power)
{
    you.duration[DUR_FIRE_SHIELD] += 5 + (power / 10) + (random2(power) / 5);

    if (you.duration[DUR_FIRE_SHIELD] > 50)
        you.duration[DUR_FIRE_SHIELD] = 50;

    mpr("The air around you leaps into flame!");

    manage_fire_shield();
}                               // end cast_ring_of_flames()

void cast_confusing_touch(int power)
{
    msg::stream << "Your " << your_hand(true) << " begin to glow "
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << std::endl;

    you.duration[DUR_CONFUSING_TOUCH] += 5 + (random2(power) / 5);

    if (you.duration[DUR_CONFUSING_TOUCH] > 50)
        you.duration[DUR_CONFUSING_TOUCH] = 50;

}                               // end cast_confusing_touch()

bool cast_sure_blade(int power)
{
    bool success = false;

    if (you.equip[EQ_WEAPON] == -1)
        mpr("You aren't wielding a weapon!");
    else if (weapon_skill( you.inv[you.equip[EQ_WEAPON]].base_type,
                     you.inv[you.equip[EQ_WEAPON]].sub_type) != SK_SHORT_BLADES)
    {
        mpr("You cannot bond with this weapon.");
    }
    else
    {
        if (!you.duration[DUR_SURE_BLADE])
            mpr("You become one with your weapon.");
        else if (you.duration[DUR_SURE_BLADE] < 25)
            mpr("Your bond becomes stronger.");

        you.duration[DUR_SURE_BLADE] += 8 + (random2(power) / 10);

        if (you.duration[DUR_SURE_BLADE] > 25)
            you.duration[DUR_SURE_BLADE] = 25;

        success = true;
    }

    return (success);
}                               // end cast_sure_blade()

void manage_fire_shield(void)
{
    you.duration[DUR_FIRE_SHIELD]--;

    if (!you.duration[DUR_FIRE_SHIELD])
        return;

    char stx = 0, sty = 0;

    for (stx = -1; stx < 2; stx++)
    {
        for (sty = -1; sty < 2; sty++)
        {
            if (sty == 0 && stx == 0)
                continue;

            //if ( one_chance_in(3) ) beam.range ++;

            if (!grid_is_solid(grd[you.x_pos + stx][you.y_pos + sty])
                && env.cgrid[you.x_pos + stx][you.y_pos + sty] == EMPTY_CLOUD)
            {
                place_cloud( CLOUD_FIRE, you.x_pos + stx, you.y_pos + sty,
                             1 + random2(6), KC_YOU );
            }
        }
    }
}                               // end manage_fire_shield()

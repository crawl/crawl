/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *      <2>     9/11/99        LRH    Teleportation takes longer in the Abyss
 *      <2>     8/05/99        BWR    Added allow_control_teleport
 *      <1>     -/--/--        LRH    Created
 */

#include "AppHdr.h"
#include "spells3.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "cloud.h"
#include "direct.h"
#include "debug.h"
#include "delay.h"
#include "itemname.h"
#include "items.h"
#include "it_use2.h"
#include "misc.h"
#include "monplace.h"
#include "mon-pick.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "spells1.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "view.h"
#include "wpn-misc.h"

static bool monster_on_level(int monster);

void cast_selective_amnesia(bool force)
{
    char ep_gain = 0;
    unsigned char keyin = 0;

    if (you.spell_no == 0)
        mpr("You don't know any spells.");      // re: sif muna {dlb}
    else
    {
        // query - conditional ordering is important {dlb}:
        for (;;)
        {
            mpr( "Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT );

            keyin = (unsigned char) get_ch();

            if (keyin == ESCAPE)
                return;         // early return {dlb}

            if (keyin == '?' || keyin == '*')
            {
                // this reassignment is "key" {dlb}
                keyin = (unsigned char) list_spells();

                redraw_screen();
            }

            if (!isalpha( keyin ))
                mesclr( true );
            else
                break;
        }

        // actual handling begins here {dlb}:
        const int spell = get_spell_by_letter( keyin );
        const int slot  = get_spell_slot_by_letter( keyin );

        if (spell == SPELL_NO_SPELL)
            mpr( "You don't know that spell." );
        else
        {
            if (!force
                 && (you.religion != GOD_SIF_MUNA
                     && random2(you.skills[SK_SPELLCASTING])
                         < random2(spell_difficulty( spell ))))
            {
                mpr("Oops! This spell sure is a blunt instrument.");
                forget_map(20 + random2(50));
            }
            else
            {
                ep_gain = spell_mana( spell );
                del_spell_from_memory_by_slot( slot );

                if (ep_gain > 0)
                {
                    inc_mp(ep_gain, false);
                    mpr( "The spell releases its latent energy back to you as "
                         "it unravels." );
                }
            }
        }
    }

    return;
}                               // end cast_selective_amnesia()

bool remove_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not curse(s) removed {dlb}

    // special "wield slot" case - see if you can figure out why {dlb}:
    // because only cursed weapons in hand only count as cursed -- bwr
    if (you.equip[EQ_WEAPON] != -1
                && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS)
    {
        if (item_cursed( you.inv[you.equip[EQ_WEAPON]] ))
        {
            do_uncurse_item( you.inv[you.equip[EQ_WEAPON]] );
            success = true;
            you.wield_change = true;
        }
    }

    // everything else uses the same paradigm - are we certain?
    // what of artefact rings and amulets? {dlb}:
    for (loopy = EQ_CLOAK; loopy < NUM_EQUIP; loopy++)
    {
        if (you.equip[loopy] != -1 && item_cursed(you.inv[you.equip[loopy]]))
        {
            do_uncurse_item( you.inv[you.equip[loopy]] );
            success = true;
        }
    }

    // messaging output {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You feel as if something is helping you.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end remove_curse()

bool detect_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not any curses found {dlb}

    for (loopy = 0; loopy < ENDOFPACK; loopy++)
    {
        if (you.inv[loopy].quantity
            && (you.inv[loopy].base_type == OBJ_WEAPONS
                || you.inv[loopy].base_type == OBJ_ARMOUR
                || you.inv[loopy].base_type == OBJ_JEWELLERY))
        {
            if (item_not_ident( you.inv[loopy], ISFLAG_KNOW_CURSE ))
                success = true;

            set_ident_flags( you.inv[loopy], ISFLAG_KNOW_CURSE );
        }
    }

    // messaging output {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You sense the presence of curses on your possessions.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end detect_curse()

bool cast_smiting(int power)
{
    bool success = false;
    struct dist beam;
    struct monsters *monster = 0;       // NULL {dlb}

    mpr("Smite whom?", MSGCH_PROMPT);

    direction( beam, DIR_TARGET, TARG_ENEMY );

    if (!beam.isValid
        || mgrd[beam.tx][beam.ty] == NON_MONSTER
        || beam.isMe)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else
    {
        monster = &menv[mgrd[beam.tx][beam.ty]];

        strcpy(info, "You smite ");
        strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
        strcat(info, "!");
        mpr(info);

        hurt_monster(monster, random2(8) + (random2(power) / 3));

        if (monster->hit_points < 1)
            monster_die(monster, KILL_YOU, 0);
        else
            print_wounds(monster);

        success = true;
    }

    return (success);
}                               // end cast_smiting()

bool airstrike(int power)
{
    bool success = false;
    struct dist beam;
    struct monsters *monster = 0;       // NULL {dlb}
    int hurted = 0;

    mpr("Strike whom?", MSGCH_PROMPT);

    direction( beam, DIR_TARGET, TARG_ENEMY );

    if (!beam.isValid
        || mgrd[beam.tx][beam.ty] == NON_MONSTER
        || beam.isMe)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else
    {
        monster = &menv[mgrd[beam.tx][beam.ty]];

        strcpy(info, "The air twists around and strikes ");
        strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
        strcat(info, "!");
        mpr(info);

        hurted = random2( random2(12) + (random2(power) / 6)
                                      + (random2(power) / 7) );
        hurted -= random2(1 + monster->armour_class);

        if (hurted < 0)
            hurted = 0;
        else
        {
            hurt_monster(monster, hurted);

            if (monster->hit_points < 1)
                monster_die(monster, KILL_YOU, 0);
            else
                print_wounds(monster);
        }

        success = true;
    }

    return (success);
}                               // end airstrike()

bool cast_bone_shards(int power)
{
    bool success = false;
    struct bolt beam;
    struct dist spelld;

    if (you.equip[EQ_WEAPON] == -1
                    || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_CORPSES)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else if (you.inv[you.equip[EQ_WEAPON]].sub_type != CORPSE_SKELETON)
        mpr("The corpse collapses into a mass of pulpy flesh.");
    else if (spell_direction(spelld, beam) != -1)
    {
        // practical max of 100 * 15 + 3000 = 4500
        // actual max of    200 * 15 + 3000 = 6000
        power *= 15;
        power += mons_weight( you.inv[you.equip[EQ_WEAPON]].plus );

        mpr("The skeleton explodes into sharp fragments of bone!");

        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
        zapping(ZAP_BONE_SHARDS, power, beam);

        success = true;
    }

    return (success);
}                               // end cast_bone_shards()

void sublimation(int power)
{
    unsigned char loopy = 0;    // general purpose loop variable {dlb}

    if (you.equip[EQ_WEAPON] == -1
        || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_FOOD
        || you.inv[you.equip[EQ_WEAPON]].sub_type != FOOD_CHUNK)
    {
        if (you.deaths_door)
        {
            mpr( "A conflicting enchantment prevents the spell from "
                 "coming into effect." );
        }
        else if (!enough_hp( 2, true ))
        {
             mpr("Your attempt to draw power from your own body fails.");
        }
        else
        {
            mpr("You draw magical energy from your own body!");

            while (you.magic_points < you.max_magic_points && you.hp > 1)
            {
                inc_mp(1, false);
                dec_hp(1, false);

                for (loopy = 0; loopy < (you.hp > 1 ? 3 : 0); loopy++)
                {
                    if (random2(power) < 6)
                        dec_hp(1, false);
                }

                if (random2(power) < 6)
                    break;
            }
        }
    }
    else
    {
        mpr("The chunk of flesh you are holding crumbles to dust.");
        mpr("A flood of magical energy pours into your mind!");

        inc_mp( 7 + random2(7), false );

        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
    }

    return;
}                               // end sublimation()

// Simulacrum
//
// This spell extends creating undead to Ice mages, as such it's high
// level, requires wielding of the material component, and the undead 
// aren't overly powerful (they're also vulnerable to fire).  I've put
// back the abjuration level in order to keep down the army sizes again.
//
// As for what it offers necromancers considering all the downsides
// above... it allows the turning of a single corpse into an army of
// monsters (one per food chunk)... which is also a good reason for
// why it's high level.
//
// Hides and other "animal part" items are intentionally left out, it's
// unrequired complexity, and fresh flesh makes more "sense" for a spell
// reforming the original monster out of ice anyways.
void simulacrum(int power)
{
    int max_num = 4 + random2(power) / 20;
    if (max_num > 8)
        max_num = 8;

    const int chunk = you.equip[EQ_WEAPON];

    if (chunk != -1
        && is_valid_item( you.inv[ chunk ] )
        && (you.inv[ chunk ].base_type == OBJ_CORPSES
            || (you.inv[ chunk ].base_type == OBJ_FOOD
                && you.inv[ chunk ].sub_type == FOOD_CHUNK)))
    {
        const int mons_type = you.inv[ chunk ].plus;

        // Can't create more than the available chunks
        if (you.inv[ chunk ].quantity < max_num)
            max_num = you.inv[ chunk ].quantity;

        dec_inv_item_quantity( chunk, max_num );

        int summoned = 0;

        for (int i = 0; i < max_num; i++)
        {
            if (create_monster( MONS_SIMULACRUM_SMALL, ENCH_ABJ_VI,
                                BEH_FRIENDLY, you.x_pos, you.y_pos,
                                you.pet_target, mons_type ) != -1)
            {
                summoned++;
            }
        }

        if (summoned)
        {
            strcpy( info, (summoned == 1) ? "An icy figure forms "
                                          : "Some icy figures form " );
            strcat( info, "before you!" );
            mpr( info );
        }
        else
            mpr( "You feel cold for a second." );
    }
    else
    {
        mpr( "You need to wield a piece of raw flesh for this spell "
             "to be effective!" );
    }
}                               // end sublimation()

void dancing_weapon(int pow, bool force_hostile)
{
    int numsc = ENCH_ABJ_II + (random2(pow) / 5);
    char str_pass[ ITEMNAME_SIZE ];

    if (numsc > ENCH_ABJ_VI)
        numsc = ENCH_ABJ_VI;

    int i;
    int summs = 0;
    char behavi = BEH_FRIENDLY;

    const int wpn = you.equip[EQ_WEAPON];

    // See if weilded item is appropriate:
    if (wpn == -1
        || you.inv[wpn].base_type != OBJ_WEAPONS
        || launches_things( you.inv[wpn].sub_type )
        || is_fixed_artefact( you.inv[wpn] ))
    {
        goto failed_spell;
    }

    // See if we can get an mitm for the dancing weapon:
    i = get_item_slot();
    if (i == NON_ITEM)
        goto failed_spell;

    // cursed weapons become hostile
    if (item_cursed( you.inv[wpn] ) || force_hostile)
        behavi = BEH_HOSTILE; 

    summs = create_monster( MONS_DANCING_WEAPON, numsc, behavi, 
                            you.x_pos, you.y_pos, you.pet_target, 1 );

    if (summs < 0)
    {
        // must delete the item before failing!
        mitm[i].base_type = OBJ_UNASSIGNED;
        mitm[i].quantity = 0;
        goto failed_spell;
    }

    // We are successful:
    unwield_item( wpn );                        // remove wield effects

    // copy item (done here after any wield effects are removed)
    mitm[i] = you.inv[wpn];
    mitm[i].quantity = 1;
    mitm[i].x = 0;
    mitm[i].y = 0;
    mitm[i].link = NON_ITEM;

    in_name( wpn, DESC_CAP_YOUR, str_pass );
    strcpy( info, str_pass );
    strcat( info, " dances into the air!" );
    mpr( info );

    you.inv[ wpn ].quantity = 0;
    you.equip[EQ_WEAPON] = -1;

    menv[summs].inv[MSLOT_WEAPON] = i;
    menv[summs].number = mitm[i].colour;

    return;

failed_spell:
    mpr("Your weapon vibrates crazily for a second.");
}                               // end dancing_weapon()

static bool monster_on_level(int monster)
{
    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].type == monster)
            return true;
    }

    return false;
}                               // end monster_on_level()

//
// This function returns true if the player can use controlled
// teleport here.
//
bool allow_control_teleport( bool silent )
{
    bool ret = true;

    if (you.level_type == LEVEL_ABYSS || you.level_type == LEVEL_LABYRINTH)
        ret = false;
    else
    {
        switch (you.where_are_you)
        {
        case BRANCH_TOMB:
            // The tomb is a laid out maze, it'd be a shame if the player
            // just teleports through any of it... so we only allow 
            // teleport once they have the rune.
            ret = false;
            for (int i = 0; i < ENDOFPACK; i++)
            {
                if (is_valid_item( you.inv[i] )
                    && you.inv[i].base_type == OBJ_MISCELLANY
                    && you.inv[i].sub_type == MISC_RUNE_OF_ZOT
                    && you.inv[i].plus == BRANCH_TOMB)
                {
                    ret = true;
                    break;
                }
            }
            break;

        case BRANCH_SLIME_PITS:
            // Cannot teleport into the slime pit vaults until
            // royal jelly is gone.
            if (monster_on_level(MONS_ROYAL_JELLY))
                ret = false;
            break;

        case BRANCH_ELVEN_HALLS:
            // Cannot raid the elven halls vaults until fountain drained
            if (you.branch_stairs[STAIRS_ELVEN_HALLS] +
                    branch_depth(STAIRS_ELVEN_HALLS) == you.your_level)
            {
                for (int x = 5; x < GXM - 5; x++) 
                {
                    for (int y = 5; y < GYM - 5; y++) 
                    {
                        if (grd[x][y] == DNGN_SPARKLING_FOUNTAIN)
                            ret = false;
                    }
                }
            }
            break;

        case BRANCH_HALL_OF_ZOT:
            // Cannot control teleport until the Orb is picked up
            if (you.branch_stairs[STAIRS_HALL_OF_ZOT] +
                    branch_depth(STAIRS_HALL_OF_ZOT) == you.your_level
                && you.char_direction != DIR_ASCENDING)
            {
                ret = false;
            }
            break;
        }
    }

    // Tell the player why if they have teleport control.
    if (!ret && you.attribute[ATTR_CONTROL_TELEPORT] && !silent)
        mpr("A powerful magic prevents control of your teleportation.");

    return ret;
}                               // end allow_control_teleport()

void you_teleport(void)
{
    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
        mpr("You feel a weird sense of stasis.");
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        you.duration[DUR_TELEPORT] = 3 + random2(3);

        if (you.level_type == LEVEL_ABYSS && !one_chance_in(5))
        {
            mpr("You have a feeling this translocation may take a while to kick in...");
            you.duration[DUR_TELEPORT] += 5 + random2(10);
        }
    }

    return;
}                               // end you_teleport()

void you_teleport2( bool allow_control, bool new_abyss_area )
{
    bool is_controlled = (allow_control && !you.conf
                              && you.attribute[ATTR_CONTROL_TELEPORT]
                              && allow_control_teleport());

    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
    {
        mpr("You feel a strange sense of stasis.");
        return;
    }

    // after this point, we're guaranteed to teleport. Turn off auto-butcher.
    // corpses still get butchered,  but at least we don't get a silly message.
    if (current_delay_action() == DELAY_BUTCHER)
        stop_delay();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = 1;
    }

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( new_abyss_area );
        you.pet_target = MHITNOT;
        return;
    }

    FixedVector < int, 2 > plox;

    plox[0] = 1;
    plox[1] = 0;

    if (is_controlled)
    {
        mpr("You may choose your destination (press '.' or delete to select).");
        mpr("Expect minor deviation.");
        more();

        show_map(plox);

        redraw_screen();

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "Target square (%d,%d)", plox[0], plox[1] );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        plox[0] += random2(3) - 1;
        plox[1] += random2(3) - 1;

        if (one_chance_in(4))
        {
            plox[0] += random2(3) - 1;
            plox[1] += random2(3) - 1;
        }

        if (plox[0] < 6 || plox[1] < 6 || plox[0] > (GXM - 5)
                || plox[1] > (GYM - 5))
        {
            mpr("Nearby solid objects disrupt your rematerialisation!");
            is_controlled = false;
        }

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "Scattered target square (%d,%d)", plox[0], plox[1] );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        if (is_controlled)
        {
            you.x_pos = plox[0];
            you.y_pos = plox[1];

            if ((grd[you.x_pos][you.y_pos] != DNGN_FLOOR
                    && grd[you.x_pos][you.y_pos] != DNGN_SHALLOW_WATER)
                || mgrd[you.x_pos][you.y_pos] != NON_MONSTER
                || env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
            {
                is_controlled = false;
            }
            else
            {
                // controlling teleport contaminates the player -- bwr
                contaminate_player(1);
            }
        }
    }                           // end "if is_controlled"

    if (!is_controlled)
    {
        mpr("Your surroundings suddenly seem different.");

        do
        {
            you.x_pos = 5 + random2( GXM - 10 );
            you.y_pos = 5 + random2( GYM - 10 );
        }
        while ((grd[you.x_pos][you.y_pos] != DNGN_FLOOR
                   && grd[you.x_pos][you.y_pos] != DNGN_SHALLOW_WATER)
               || mgrd[you.x_pos][you.y_pos] != NON_MONSTER
               || env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD);
    }
}                               // end you_teleport()

bool entomb(void)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool proceed = false;       // loop management varaiable {dlb}
    int which_trap = 0;         // used in clearing out certain traps {dlb}
    char srx = 0, sry = 0;
    char number_built = 0;

    FixedVector < unsigned char, 7 > safe_to_overwrite;

    // hack - passing chars through '...' promotes them to ints, which
    // barfs under gcc in fixvec.h.  So don't.
    safe_to_overwrite[0] = DNGN_FLOOR;
    safe_to_overwrite[1] = DNGN_SHALLOW_WATER;
    safe_to_overwrite[2] = DNGN_OPEN_DOOR;
    safe_to_overwrite[3] = DNGN_TRAP_MECHANICAL;
    safe_to_overwrite[4] = DNGN_TRAP_MAGICAL;
    safe_to_overwrite[5] = DNGN_TRAP_III;
    safe_to_overwrite[6] = DNGN_UNDISCOVERED_TRAP;


    for (srx = you.x_pos - 1; srx < you.x_pos + 2; srx++)
    {
        for (sry = you.y_pos - 1; sry < you.y_pos + 2; sry++)
        {
            proceed = false;

            // tile already occupied by monster or yourself {dlb}:
            if (mgrd[srx][sry] != NON_MONSTER
                    || (srx == you.x_pos && sry == you.y_pos))
            {
                continue;
            }

            // the break here affects innermost containing loop {dlb}:
            for (loopy = 0; loopy < 7; loopy++)
            {
                if (grd[srx][sry] == safe_to_overwrite[loopy])
                {
                    proceed = true;
                    break;
                }
            }

            // checkpoint one - do we have a legitimate tile? {dlb}
            if (!proceed)
                continue;

            int objl = igrd[srx][sry];
            int hrg = 0;

            while (objl != NON_ITEM)
            {
                // hate to see the orb get  detroyed by accident {dlb}:
                if (mitm[objl].base_type == OBJ_ORBS)
                {
                    proceed = false;
                    break;
                }

                hrg = mitm[objl].link;
                objl = hrg;
            }

            // checkpoint two - is the orb resting in the tile? {dlb}:
            if (!proceed)
                continue;

            objl = igrd[srx][sry];
            hrg = 0;

            while (objl != NON_ITEM)
            {
                hrg = mitm[objl].link;
                destroy_item(objl);
                objl = hrg;
            }

            // deal with clouds {dlb}:
            if (env.cgrid[srx][sry] != EMPTY_CLOUD)
                delete_cloud( env.cgrid[srx][sry] );

            // mechanical traps are destroyed {dlb}:
            if ((which_trap = trap_at_xy(srx, sry)) != -1)
            {
                if (trap_category(env.trap[which_trap].type)
                                                    == DNGN_TRAP_MECHANICAL)
                {
                    env.trap[which_trap].type = TRAP_UNASSIGNED;
                    env.trap[which_trap].x = 1;
                    env.trap[which_trap].y = 1;
                }
            }

            // finally, place the wall {dlb}:
            grd[srx][sry] = DNGN_ROCK_WALL;
            number_built++;
        }                       // end "for srx,sry"
    }

    if (number_built > 0)
        mpr("Walls emerge from the floor!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (number_built > 0);
}                               // end entomb()

void cast_poison_ammo(void)
{
    const int ammo = you.equip[EQ_WEAPON];
    char str_pass[ ITEMNAME_SIZE ];

    if (ammo == -1
        || you.inv[ammo].base_type != OBJ_MISSILES
        || get_ammo_brand( you.inv[ammo] ) != SPMSL_NORMAL
        || you.inv[ammo].sub_type == MI_STONE
        || you.inv[ammo].sub_type == MI_LARGE_ROCK)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    if (set_item_ego_type( you.inv[ammo], OBJ_MISSILES, SPMSL_POISONED ))
    {
        in_name(ammo, DESC_CAP_YOUR, str_pass);
        strcpy(info, str_pass);
        strcat(info, (you.inv[ammo].quantity == 1) ? " is" : " are");
        strcat(info, " covered in a thin film of poison.");
        mpr(info);

        you.wield_change = true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}                               // end cast_poison_ammo()

bool project_noise(void)
{
    bool success = false;
    FixedVector < int, 2 > plox;

    plox[0] = 1;
    plox[1] = 0;

    mpr( "Choose the noise's source (press '.' or delete to select)." );
    more();
    show_map(plox);

    redraw_screen();

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Target square (%d,%d)", plox[0], plox[1] );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (!silenced( plox[0], plox[1] ))
    {
        // player can use this spell to "sound out" the dungeon -- bwr
        if (plox[0] > 1 && plox[0] < (GXM - 2) 
            && plox[1] > 1 && plox[1] < (GYM - 2)
            && grd[ plox[0] ][ plox[1] ] > DNGN_LAST_SOLID_TILE)
        {
            noisy( 30, plox[0], plox[1] );
            success = true;
        }

        if (!silenced( you.x_pos, you.y_pos ))
        {
            if (!success)
                mpr("You hear a dull thud.");
            else
            {
                snprintf( info, INFO_SIZE, "You hear a %svoice call your name.",
                          (see_grid( plox[0], plox[1] ) ? "distant " : "") );
                mpr( info );
            }
        }
    }

    return (success);
}                               // end project_noise()

/*
   Type recalled:
   0 = anything
   1 = undead only (Kiku religion ability)
 */
bool recall(char type_recalled)
{
    int loopy = 0;              // general purpose looping variable {dlb}
    bool success = false;       // more accurately: "apparent success" {dlb}
    int start_count = 0;
    int step_value = 1;
    int end_count = (MAX_MONSTERS - 1);
    FixedVector < char, 2 > empty;
    struct monsters *monster = 0;       // NULL {dlb}

    empty[0] = empty[1] = 0;

// someone really had to make life difficult {dlb}:
// sometimes goes through monster list backwards
    if (coinflip())
    {
        start_count = (MAX_MONSTERS - 1);
        end_count = 0;
        step_value = -1;
    }

    for (loopy = start_count; loopy != end_count; loopy += step_value)
    {
        monster = &menv[loopy];

        if (monster->type == -1)
            continue;

        if (!mons_friendly(monster))
            continue;

        if (monster_habitat(monster->type) != DNGN_FLOOR)
            continue;

        if (type_recalled == 1)
        {
            /* abomin created by twisted res, although it gets others too */
            if ( !((monster->type == MONS_ABOMINATION_SMALL
                            || monster->type == MONS_ABOMINATION_LARGE)
                        && (monster->number == BROWN
                            || monster->number == RED
                            || monster->number == LIGHTRED)) )
            {
                if (monster->type != MONS_REAPER
                        && mons_holiness(monster->type) != MH_UNDEAD)
                {
                    continue;
                }
            }
        }

        if (empty_surrounds(you.x_pos, you.y_pos, DNGN_FLOOR, false, empty))
        {
            // clear old cell pointer -- why isn't there a function for moving a monster?
            mgrd[monster->x][monster->y] = NON_MONSTER;
            // set monster x,y to new value
            monster->x = empty[0];
            monster->y = empty[1];
            // set new monster grid pointer to this monster.
            mgrd[monster->x][monster->y] = monster_index(monster);

            // only informed if monsters recalled are visible {dlb}:
            if (simple_monster_message(monster, " is recalled."))
                success = true;
        }
        else
        {
            break;              // no more room to place monsters {dlb}
        }
    }

    if (!success)
        mpr("Nothing appears to have answered your call.");

    return (success);
}                               // end recall()

void portal(void)
{
    char dir_sign = 0;
    unsigned char keyi;
    int target_level = 0;
    int old_level = you.your_level;

    if (!player_in_branch( BRANCH_MAIN_DUNGEON ))
    {
        mpr("This spell doesn't work here.");
    }
    else if (grd[you.x_pos][you.y_pos] != DNGN_FLOOR)
    {
        mpr("You must find a clear area in which to cast this spell.");
    }
    else
    {
        // the first query {dlb}:
        mpr("Which direction ('<' for up, '>' for down, 'x' to quit)?", MSGCH_PROMPT);

        for (;;)
        {
            keyi = (unsigned char) get_ch();

            if (keyi == '<')
            {
                if (you.your_level == 0)
                    mpr("You can't go any further upwards with this spell.");
                else
                {
                    dir_sign = -1;
                    break;
                }
            }

            if (keyi == '>')
            {
                if (you.your_level == 35)
                    mpr("You can't go any further downwards with this spell.");
                else
                {
                    dir_sign = 1;
                    break;
                }
            }

            if (keyi == 'x')
            {
                canned_msg(MSG_OK);
                return;         // an early return {dlb}
            }
        }

        // the second query {dlb}:
        mpr("How many levels (1 - 9, 'x' to quit)?", MSGCH_PROMPT);

        for (;;)
        {
            keyi = (unsigned char) get_ch();

            if (keyi == 'x')
            {
                canned_msg(MSG_OK);
                return;         // another early return {dlb}
            }

            if (!(keyi < '1' || keyi > '9'))
            {
                target_level = you.your_level + ((keyi - '0') * dir_sign);
                break;
            }
        }

        // actual handling begins here {dlb}:
        if (player_in_branch( BRANCH_MAIN_DUNGEON ))
        {
            if (target_level < 0)
                target_level = 0;
            else if (target_level > 26)
                target_level = 26;
        }

        mpr( "You fall through a mystic portal, and materialise at the "
             "foot of a staircase." );
        more();

        you.your_level = target_level - 1;
        grd[you.x_pos][you.y_pos] = DNGN_STONE_STAIRS_DOWN_I;

        down_stairs( true, old_level );
        untag_followers();
    }

    return;
}                               // end portal()

bool cast_death_channel(int power)
{
    bool success = false;

    if (you.duration[DUR_DEATH_CHANNEL] < 30)
    {
        mpr("Malign forces permeate your being, awaiting release.");

        you.duration[DUR_DEATH_CHANNEL] += 15 + random2(1 + (power / 3));

        if (you.duration[DUR_DEATH_CHANNEL] > 100)
            you.duration[DUR_DEATH_CHANNEL] = 100;

        success = true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}                               // end cast_death_channel()

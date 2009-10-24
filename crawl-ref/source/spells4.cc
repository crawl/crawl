/*
 *  File:       spells4.cc
 *  Summary:    new spells, focusing on transmigration, divination and
 *              other neglected areas of Crawl magic ;^)
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <2> 29jul2000  jdj  Made a zillion functions static.
 *   <1> 06jan2000  jmf  Created
 */

#include "AppHdr.h"

#include <string>
#include <iostream>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "ouch.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "spells1.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "view.h"

enum DEBRIS                 // jmf: add for shatter, dig, and Giants to throw
{
    DEBRIS_METAL,           //    0
    DEBRIS_ROCK,
    DEBRIS_STONE,
    DEBRIS_WOOD,
    DEBRIS_CRYSTAL,
    NUM_DEBRIS
};          // jmf: ...and I'll actually implement the items Real Soon Now...

static int _make_a_rot_cloud(int x, int y, int pow, cloud_type ctype);
static int _quadrant_blink(int x, int y, int pow, int garbage);

void do_monster_rot(int mon);

// Just to avoid typing this over and over.
// Returns true if monster died. -- bwr
inline bool player_hurt_monster(int monster, int damage)
{
    ASSERT( monster != NON_MONSTER );

    if (damage > 0)
    {
        hurt_monster( &menv[monster], damage );

        if (menv[monster].hit_points > 0)
        {
            const monsters *mons = static_cast<const monsters*>(&menv[monster]);
            print_wounds(mons);
        }
        else
        {
            monster_die( &menv[monster], KILL_YOU, 0 );
            return (true);
        }
    }

    return (false);
}

// Here begin the actual spells:
static int _shatter_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    dice_def   dam_dice( 0, 5 + pow / 3 );  // number of dice set below
    const int  monster = mgrd[x][y];

    if (monster == NON_MONSTER)
        return (0);

    // Removed a lot of silly monsters down here... people, just because
    // it says ice, rock, or iron in the name doesn't mean it's actually
    // made out of the substance. -- bwr
    switch (menv[monster].type)
    {
    case MONS_ICE_BEAST:        // 3/2 damage
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
    case MONS_SILVER_STATUE:
        dam_dice.num = 4;
        break;

    case MONS_SKELETON_SMALL: // double damage
    case MONS_SKELETON_LARGE:
    case MONS_CURSE_SKULL:
    case MONS_CLAY_GOLEM:
    case MONS_STONE_GOLEM:
    case MONS_IRON_GOLEM:
    case MONS_CRYSTAL_GOLEM:
    case MONS_ORANGE_STATUE:
    case MONS_EARTH_ELEMENTAL:
    case MONS_GARGOYLE:
    case MONS_SKELETAL_DRAGON:
    case MONS_SKELETAL_WARRIOR:
        dam_dice.num = 6;
        break;

    case MONS_VAPOUR:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_AIR_ELEMENTAL:
    case MONS_FIRE_ELEMENTAL:
    case MONS_WATER_ELEMENTAL:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_FREEZING_WRAITH:
    case MONS_WRAITH:
    case MONS_PHANTOM:
    case MONS_PLAYER_GHOST:
    case MONS_SHADOW:
    case MONS_HUNGRY_GHOST:
    case MONS_FLAYED_GHOST:
    case MONS_SMOKE_DEMON:      //jmf: I hate these bastards...
        dam_dice.num = 0;
        break;

    case MONS_PULSATING_LUMP:
    case MONS_JELLY:
    case MONS_SLIME_CREATURE:
    case MONS_BROWN_OOZE:
    case MONS_AZURE_JELLY:
    case MONS_DEATH_OOZE:
    case MONS_ACID_BLOB:
    case MONS_ROYAL_JELLY:
    case MONS_OOZE:
    case MONS_SPECTRAL_THING:
    case MONS_JELLYFISH:
        dam_dice.num = 1;
        dam_dice.size /= 2;
        break;

    case MONS_DANCING_WEAPON:     // flies, but earth based
    case MONS_MOLTEN_GARGOYLE:
    case MONS_QUICKSILVER_DRAGON:
        // Soft, earth creatures... would normally resist to 1 die, but
        // are sensitive to this spell. -- bwr
        dam_dice.num = 2;
        break;

    default:                    // normal damage
        if (mons_flies( &menv[monster] ))
            dam_dice.num = 1;
        else
            dam_dice.num = 3;
        break;
    }

    int damage = roll_dice( dam_dice ) - random2( menv[monster].ac );

    if (damage > 0)
        player_hurt_monster( monster, damage );
    else
        damage = 0;

    return (damage);
}

static int _shatter_items(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int broke_stuff = 0, next, obj = igrd[x][y];

    if (obj == NON_ITEM)
        return 0;

    while (obj != NON_ITEM)
    {
        next = mitm[obj].link;

        switch (mitm[obj].base_type)
        {
        case OBJ_POTIONS:
            if (!one_chance_in(10))
            {
                broke_stuff++;
                destroy_item(obj);
            }
            break;

        default:
            break;
        }

        obj = next;
    }

    if (broke_stuff)
    {
        if (!silenced(x, y) && !silenced(you.x_pos, you.y_pos))
            mpr("You hear glass break.", MSGCH_SOUND);

        return 1;
    }

    return 0;
}

static int _shatter_walls(int x, int y, int pow, int garbage)
{
    UNUSED(garbage);

    int chance = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (x <= 5 || x >= GXM - 5 || y <= 5 || y >= GYM - 5)
        return (0);

    const dungeon_feature_type grid = grd[x][y];

    switch (grid)
    {
    case DNGN_SECRET_DOOR:
        if (see_grid(x, y))
            mpr("A secret door shatters!");
        chance = 100;
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_OPEN_DOOR:
        if (see_grid(x, y))
            mpr("A door shatters!");
        chance = 100;
        break;

    case DNGN_METAL_WALL:
        chance = pow / 10;
        break;

    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        chance = 50;
        break;

    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        chance = pow / 6;
        break;

    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_ROCK_WALL:
        chance = pow / 4;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        chance = 50;
        break;

    default:
        break;
    }

    if (x_chance_in_y(chance, 100))
    {
        noisy(30, x, y);

        grd[x][y] = DNGN_FLOOR;

        if (grid == DNGN_ORCISH_IDOL)
            beogh_idol_revenge();

        return (1);
    }

    return (0);
}

void cast_shatter(int pow)
{
    int damage = 0;
    const bool silence = silenced(you.x_pos, you.y_pos);

    if (silence)
        mpr("The dungeon shakes!");
    else
    {
        noisy(30, you.x_pos, you.y_pos);
        mpr("The dungeon rumbles!", MSGCH_SOUND);
    }

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_NONE:
    case TRAN_SPIDER:
    case TRAN_LICH:
    case TRAN_DRAGON:
    case TRAN_AIR:
    case TRAN_SERPENT_OF_HELL:
    case TRAN_BAT:
        break;

    case TRAN_STATUE:           // full damage
        damage = 15 + random2avg( (pow / 5), 4 );
        break;

    case TRAN_ICE_BEAST:        // 1/2 damage
        damage = 10 + random2avg( (pow / 5), 4 ) / 2;
        break;

    case TRAN_BLADE_HANDS:      // 2d3 damage
        mpr("Your scythe-like blades vibrate painfully!");
        damage = 2 + random2avg(5, 2);
        break;

    default:
        mpr("cast_shatter(): unknown transformation in spells4.cc");
    }

    if (damage)
        ouch(damage, 0, KILLED_BY_TARGETTING);

    int rad = 3 + (you.skills[SK_EARTH_MAGIC] / 5);

    apply_area_within_radius(_shatter_items, you.x_pos, you.y_pos,
                             pow, rad, 0);
    apply_area_within_radius(_shatter_monsters, you.x_pos, you.y_pos,
                             pow, rad, 0);
    int dest = apply_area_within_radius( _shatter_walls, you.x_pos, you.y_pos,
                                         pow, rad, 0 );

    if (dest && !silence)
        mpr("Ka-crash!", MSGCH_SOUND);
}

// Cast_forescry: raises evasion (by 8 currently) via Divinations.
void cast_forescry(int pow)
{
    if (!you.duration[DUR_FORESCRY])
        mpr("You begin to receive glimpses of the immediate future...");
    else
        mpr("Your vision of the future intensifies.");

    you.duration[DUR_FORESCRY] += 5 + random2(pow);

    if (you.duration[DUR_FORESCRY] > 30)
        you.duration[DUR_FORESCRY] = 30;

    you.redraw_evasion = true;
}

void cast_see_invisible(int pow)
{
    if (player_see_invis())
        mpr("Nothing seems to happen.");
    else
        mpr("Your vision seems to sharpen.");

    // No message if you already are under the spell.
    you.duration[DUR_SEE_INVISIBLE] += 10 + random2(2 + (pow / 2));

    if (you.duration[DUR_SEE_INVISIBLE] > 100)
        you.duration[DUR_SEE_INVISIBLE] = 100;
}

// The description idea was okay, but this spell just isn't that exciting.
// So I'm converting it to the more practical expose secret doors. -- bwr
void cast_detect_secret_doors(int pow)
{
    int found = 0;

    for (int x = you.x_pos - 8; x <= you.x_pos + 8; x++)
    {
        for (int y = you.y_pos - 8; y <= you.y_pos + 8; y++)
        {
            if (x < 5 || x > GXM - 5 || y < 5 || y > GYM - 5)
                continue;

            if (!see_grid(x, y))
                continue;

            if (grd[x][y] == DNGN_SECRET_DOOR && random2(pow) > random2(15))
            {
                reveal_secret_door(x, y);
                found++;
            }
        }
    }

    if (found)
        redraw_screen();

    mprf("You detect %s", (found > 0) ? "secret doors!" : "nothing.");
}

static int _sleep_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );
    const int mnstr = mgrd[x][y];

    if (mnstr == NON_MONSTER)
        return 0;

    monsters& mon = menv[mnstr];

    if (mons_holiness(&mon) != MH_NATURAL)
        return 0;
    if (check_mons_resist_magic( &mon, pow ))
        return 0;

    // Works on friendlies too, so no check for that.

    //jmf: Now that sleep == hibernation:
    if (mons_res_cold( &mon ) > 0 && coinflip())
        return 0;
    if (mon.has_ench(ENCH_SLEEP_WARY))
        return 0;

    mon.put_to_sleep();

    if (mons_class_flag( mon.type, M_COLD_BLOOD ) && coinflip())
        mon.add_ench(ENCH_SLOW);

    return 1;
}

void cast_mass_sleep(int pow)
{
    apply_area_visible(_sleep_monsters, pow);
}

// This is a hack until we set an is_beast flag in the monster data
// (which we might never do, this is sort of minor.)
// It's a list of monster types which can be affected by beast taming.
static bool _is_domesticated_animal(int type)
{
    const monster_type types[] = {
        MONS_GIANT_BAT, MONS_HOUND, MONS_JACKAL, MONS_RAT,
        MONS_YAK, MONS_WYVERN, MONS_HIPPOGRIFF, MONS_GRIFFON,
        MONS_DEATH_YAK, MONS_WAR_DOG, MONS_GREY_RAT,
        MONS_GREEN_RAT, MONS_ORANGE_RAT, MONS_SHEEP,
        MONS_HOG, MONS_GIANT_FROG, MONS_GIANT_BROWN_FROG,
        MONS_SPINY_FROG, MONS_BLINK_FROG, MONS_WOLF, MONS_WARG,
        MONS_BEAR, MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR, MONS_BLACK_BEAR
    };

    for (unsigned int i = 0; i < ARRAYSZ(types); ++i)
        if (types[i] == type)
            return (true);

    return (false);
}

static int _tame_beast_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );
    const int which_mons = mgrd[x][y];

    if (which_mons == NON_MONSTER)
        return 0;

    monsters *monster = &menv[which_mons];

    if (!_is_domesticated_animal(monster->type) || mons_friendly(monster)
        || player_will_anger_monster(monster))
    {
        return 0;
    }

    // 50% bonus for dogs
    if (monster->type == MONS_HOUND || monster->type == MONS_WAR_DOG )
        pow += (pow / 2);

    if (you.species == SP_HILL_ORC && monster->type == MONS_WARG)
        pow += (pow / 2);

    if (check_mons_resist_magic(monster, pow))
        return 0;

    simple_monster_message(monster, " is tamed!");

    if (random2(100) < random2(pow / 10))
        monster->attitude = ATT_FRIENDLY;  // permanent
    else
        monster->add_ench(ENCH_CHARM);     // temporary

    return 1;
}

void cast_tame_beasts(int pow)
{
    apply_area_visible(_tame_beast_monsters, pow);
}

static int _ignite_poison_objects(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int obj = igrd[x][y], next, strength = 0;

    if (obj == NON_ITEM)
        return (0);

    while (obj != NON_ITEM)
    {
        next = mitm[obj].link;
        if (mitm[obj].base_type == OBJ_POTIONS)
        {
            switch (mitm[obj].sub_type)
            {
                // intentional fall-through all the way down
            case POT_STRONG_POISON:
                strength += 20;
            case POT_DEGENERATION:
                strength += 10;
            case POT_POISON:
                strength += 10;
                destroy_item(obj);
            default:
                break;
            }
        }

        // FIXME: implement burning poisoned ammo
        // else if ( it's ammo that's poisoned) {
        //   strength += number_of_ammo;
        //   destroy_item(ammo);
        //  }
        obj = next;
    }

    if (strength > 0)
    {
        place_cloud(CLOUD_FIRE, x, y, strength + roll_dice(3, strength / 4),
                    KC_YOU);
    }

    return (strength);
}

static int _ignite_poison_clouds( int x, int y, int pow, int garbage )
{
    UNUSED( pow );
    UNUSED( garbage );

    bool did_anything = false;

    const int cloud = env.cgrid[x][y];

    if (cloud != EMPTY_CLOUD)
    {
        if (env.cloud[ cloud ].type == CLOUD_STINK)
        {
            did_anything = true;
            env.cloud[ cloud ].type = CLOUD_FIRE;

            env.cloud[ cloud ].decay /= 2;

            if (env.cloud[ cloud ].decay < 1)
                env.cloud[ cloud ].decay = 1;
        }
        else if (env.cloud[ cloud ].type == CLOUD_POISON)
        {
            did_anything = true;
            env.cloud[ cloud ].type = CLOUD_FIRE;
        }
    }

    return did_anything;
}

static int _ignite_poison_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    struct bolt beam;
    beam.flavour = BEAM_FIRE;   // This is dumb, only used for adjust!

    dice_def  dam_dice( 0, 5 + pow / 7 );  // Dice added below if applicable.

    const int mon_index = mgrd[x][y];
    if (mon_index == NON_MONSTER)
        return (0);

    struct monsters *const mon = &menv[ mon_index ];

    // Monsters which have poison corpses or poisonous attacks.
    if (mons_is_poisoner(mon))
        dam_dice.num = 3;

    // Monsters which are poisoned:
    int strength = 0;

    // First check for player poison.
    mon_enchant ench = mon->get_ench(ENCH_POISON);
    if (ench.ench != ENCH_NONE)
        strength += ench.degree;

    // Strength is now the sum of both poison types
    // (although only one should actually be present at a given time).
    dam_dice.num += strength;

    int damage = roll_dice( dam_dice );
    if (damage > 0)
    {
        damage = mons_adjust_flavoured( mon, beam, damage );

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Dice: %dd%d; Damage: %d",
             dam_dice.num, dam_dice.size, damage );
#endif

        if (!player_hurt_monster( mon_index, damage ))
        {
            // Monster survived, remove any poison.
            mon->del_ench(ENCH_POISON);
            behaviour_event( mon, ME_ALERT );
        }

        return (1);
    }

    return (0);
}

void cast_ignite_poison(int pow)
{
    int damage = 0, strength = 0, pcount = 0, acount = 0, totalstrength = 0;
    char item;
    bool wasWielding = false;

    // Temp weapon of venom => temp fire brand.
    const int wpn = you.equip[EQ_WEAPON];

    if (wpn != -1
        && you.duration[DUR_WEAPON_BRAND]
        && get_weapon_brand( you.inv[wpn] ) == SPWPN_VENOM)
    {
        if (set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, SPWPN_FLAMING ))
        {
            mprf("%s bursts into flame!",
                 you.inv[wpn].name(DESC_CAP_YOUR).c_str());

            you.wield_change = true;
            you.duration[DUR_WEAPON_BRAND] += 1 + you.duration[DUR_WEAPON_BRAND] / 2;
            if (you.duration[DUR_WEAPON_BRAND] > 80)
                you.duration[DUR_WEAPON_BRAND] = 80;
        }
    }

    totalstrength = 0;

    for (item = 0; item < ENDOFPACK; item++)
    {
        if (!you.inv[item].quantity)
            continue;

        strength = 0;

        if (you.inv[item].base_type == OBJ_MISSILES)
        {
            if (you.inv[item].special == 3)
            {
                // Burn poison ammo.
                strength = you.inv[item].quantity;
                acount  += you.inv[item].quantity;
            }
        }

        if (you.inv[item].base_type == OBJ_POTIONS)
        {
            switch (you.inv[item].sub_type)
            {
            case POT_STRONG_POISON:
                strength += 20 * you.inv[item].quantity;
                break;
            case POT_DEGENERATION:
            case POT_POISON:
                strength += 10 * you.inv[item].quantity;
                break;
            default:
                break;
            } // end switch

            if (strength)
                pcount += you.inv[item].quantity;
        }

        if (strength)
        {
            you.inv[item].quantity = 0;
            if (item == you.equip[EQ_WEAPON])
            {
                you.equip[EQ_WEAPON] = -1;
                wasWielding = true;
            }
        }

        totalstrength += strength;
    }

    if (acount > 0)
        mpr("Some ammunition you are carrying burns!");

    if (pcount > 0)
    {
        mprf("%s potion%s you are carrying explode%s!",
             pcount > 1 ? "Some" : "A",
             pcount > 1 ? "s" : "",
             pcount > 1 ? "" : "s");
    }

    if (wasWielding == true)
        canned_msg( MSG_EMPTY_HANDED );

    if (totalstrength)
    {
        place_cloud(
            CLOUD_FIRE, you.x_pos, you.y_pos,
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) +
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) + 1,
            KC_YOU);
    }

    // Player is poisonous.
    if (player_mutation_level(MUT_SPIT_POISON)
        || player_mutation_level(MUT_STINGER)
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER // poison attack
        || (!player_is_shapechanged()
            && (you.species == SP_GREEN_DRACONIAN       // poison breath
                || you.species == SP_KOBOLD             // poisonous corpse
                || you.species == SP_NAGA)))            // spit poison
    {
        damage = roll_dice( 3, 5 + pow / 7 );
    }

    // Player is poisoned.
    damage += roll_dice( you.duration[DUR_POISONING], 6 );

    if (damage)
    {
        const int resist = player_res_fire();

        if (resist > 0)
        {
            mpr("You feel like your blood is boiling!");
            damage = damage / 3;
        }
        else if (resist < 0)
        {
            damage *= 3;
            mpr("The poison in your system burns terribly!");
        }
        else
        {
            mpr("The poison in your system burns!");
        }

        ouch( damage, 0, KILLED_BY_TARGETTING );

        if (you.duration[DUR_POISONING] > 0)
        {
            mpr( "You feel that the poison has left your system." );
            you.duration[DUR_POISONING] = 0;
        }
    }

    apply_area_visible(_ignite_poison_clouds, pow);
    apply_area_visible(_ignite_poison_objects, pow);
    apply_area_visible(_ignite_poison_monsters, pow);
}                               // end cast_ignite_poison()

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.duration[DUR_SILENCE] += 10 + random2avg( pow, 2 );

    if (you.duration[DUR_SILENCE] > 100)
        you.duration[DUR_SILENCE] = 100;

    if (you.duration[DUR_BEHELD])
    {
        mpr("You break out of your daze!", MSGCH_RECOVERY);
        you.duration[DUR_BEHELD] = 0;
        you.beheld_by.clear();
    }
}

static int _discharge_monsters( int x, int y, int pow, int garbage )
{
    UNUSED( garbage );

    const int mon = mgrd[x][y];
    int damage = 0;

    struct bolt beam;
    beam.flavour = BEAM_ELECTRICITY; // used for mons_adjust_flavoured

    if (x == you.x_pos && y == you.y_pos)
    {
        mpr( "You are struck by lightning." );
        damage = 3 + random2( 5 + pow / 10 );
        damage = check_your_resists( damage, BEAM_ELECTRICITY );
        if ( player_is_airborne() )
            damage /= 2;
        ouch( damage, 0, KILLED_BY_WILD_MAGIC );
    }
    else if (mon == NON_MONSTER)
        return (0);
    else if (mons_res_elec(&menv[mon]) > 0 || mons_flies(&menv[mon]))
        return (0);
    else
    {
        damage = 3 + random2( 5 + pow / 10 );
        damage = mons_adjust_flavoured( &menv[mon], beam, damage );

        if (damage)
        {
            mprf( "%s is struck by lightning.",
                  menv[mon].name(DESC_CAP_THE).c_str());
            player_hurt_monster( mon, damage );
        }
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(3)) || (pow >= 3 && one_chance_in(10)))
    {
        mpr( "The lightning arcs!" );
        pow /= (coinflip() ? 2 : 3);
        damage += apply_random_around_square( _discharge_monsters, x, y,
                                              true, pow, 1 );
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in
        // cast_discharge() are clean. -- bwr
        mpr( "The lightning grounds out." );
    }

    return (damage);
}

void cast_discharge( int pow )
{
    int num_targs = 1 + random2( 1 + pow / 25 );
    int dam;

    dam = apply_random_around_square( _discharge_monsters, you.x_pos, you.y_pos,
                                      true, pow, num_targs );

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Arcs: %d Damage: %d", num_targs, dam );
#endif

    if (dam == 0)
    {
        if (coinflip())
            mpr("The air around you crackles with electrical energy.");
        else
        {
            const bool plural = coinflip();
            mprf("%s blue arc%s ground%s harmlessly %s you.",
                 plural ? "Some" : "A",
                 plural ? "s" : "",
                 plural ? " themselves" : "s itself",
                 plural ? "around" : (coinflip() ? "beside" :
                                      coinflip() ? "behind" : "before"));
        }
    }
}

// NB: this must be checked against the same effects
// in fight.cc for all forms of attack !!! {dlb}
// This function should be currently unused (the effect is too powerful).
static int _distortion_monsters(int x, int y, int pow, int message)
{
    int specdam = 0;
    int monster_attacked = mgrd[x][y];

    if (monster_attacked == NON_MONSTER)
        return 0;

    struct monsters *defender = &menv[monster_attacked];

    if (pow > 100)
        pow = 100;

    if (x == you.x_pos && y == you.y_pos)
    {
        if (you.skills[SK_TRANSLOCATIONS] < random2(8))
        {
            miscast_effect( SPTYP_TRANSLOCATION, pow / 9 + 1, pow, 100,
                            "cast bend on self" );
        }
        else
        {
            miscast_effect( SPTYP_TRANSLOCATION, 1, 1, 100,
                            "cast bend on self" );
        }

        return 1;
    }

    if (defender->type == MONS_BLINK_FROG)      // any others resist?
    {
        int hp = defender->hit_points;
        int max_hp = defender->max_hit_points;

        mpr("The blink frog basks in the translocular energy.");

        if (hp < max_hp)
            hp += 1 + random2(1 + pow / 4) + random2(1 + pow / 7);

        if (hp > max_hp)
            hp = max_hp;

        defender->hit_points = hp;
        return 1;
    }
    else if (coinflip())
    {
        mprf("Space bends around %s.",
             defender->name(DESC_NOCAP_THE).c_str());
        specdam += 1 + random2avg( 7, 2 ) + random2(pow) / 40;
    }
    else if (coinflip())
    {
        mprf("Space warps horribly around %s!",
             defender->name(DESC_NOCAP_THE).c_str());
        specdam += 3 + random2avg( 12, 2 ) + random2(pow) / 25;
    }
    else if (one_chance_in(3))
    {
        monster_blink(defender);
        return 1;
    }
    else if (one_chance_in(3))
    {
        monster_teleport(defender, coinflip());
        return 1;
    }
    else if (one_chance_in(3))
    {
        defender->banish();
        return 1;
    }
    else if (message)
    {
        mpr("Nothing seems to happen.");
        return 1;
    }

    player_hurt_monster(monster_attacked, specdam);

    return (specdam);
}

void cast_bend(int pow)
{
    apply_one_neighbouring_square( _distortion_monsters, pow );
}

// Really this is just applying the best of Band/Warp weapon/Warp field
// into a spell that gives the "make monsters go away" benefit without
// the insane damage potential.  -- bwr
int disperse_monsters(int x, int y, int pow, int message)
{
    UNUSED( message );

    const int monster_attacked = mgrd[x][y];

    if (monster_attacked == NON_MONSTER)
        return 0;

    struct monsters *defender = &menv[monster_attacked];

    if (defender->type == MONS_BLINK_FROG)
    {
        simple_monster_message(defender, " resists.");
        return 1;
    }
    else if (check_mons_resist_magic(defender, pow))
    {
        // XXX Note that this might affect magic-immunes!
        if (coinflip())
        {
            simple_monster_message(defender, " partially resists.");
            monster_blink(defender);
        }
        else
            simple_monster_message(defender, " resists.");

        return 1;
    }
    else
    {
        monster_teleport( defender, true );
        return 1;
    }

    return 0;
}

void cast_dispersal(int pow)
{
    if (apply_area_around_square( disperse_monsters,
                                  you.x_pos, you.y_pos, pow ) == 0)
    {
        mpr( "The air shimmers briefly around you." );
    }
}

static int _spell_swap_func(int x, int y, int pow, int message)
{
    UNUSED( message );

    int monster_attacked = mgrd[x][y];

    if (monster_attacked == NON_MONSTER)
        return 0;

    struct monsters *defender = &menv[monster_attacked];

    if (defender->type == MONS_BLINK_FROG
        || check_mons_resist_magic( defender, pow ))
    {
        simple_monster_message( defender, mons_immune_magic(defender) ?
                                " is unaffected." : " resists." );
    }
    else
    {
        // Swap doesn't seem to actually swap, but just sets the
        // monster's location equal to the players... this being because
        // the acr.cc call is going to move the player afterwards (for
        // the regular friendly monster swap).  So we'll go through
        // standard swap procedure here... since we really want to apply
        // the same swap_places function as with friendly monsters...
        // see note over there. -- bwr
        int old_x = defender->x;
        int old_y = defender->y;

        if (swap_places( defender ))
            you.moveto(old_x, old_y);
    }

    return 1;
}

void cast_swap(int pow)
{
    apply_one_neighbouring_square( _spell_swap_func, pow );
}

static int _make_a_rot_cloud(int x, int y, int pow, cloud_type ctype)
{
    int next = 0, obj = mgrd[x][y];

    if (obj == NON_MONSTER)
        return 0;

    while (obj != NON_ITEM)
    {
        next = mitm[obj].link;

        if (mitm[obj].base_type == OBJ_CORPSES
            && mitm[obj].sub_type == CORPSE_BODY)
        {
            if (!mons_skeleton(mitm[obj].plus))
                destroy_item(obj);
            else
                turn_corpse_into_skeleton(mitm[obj]);

            place_cloud(ctype, x, y,
                        (3 + random2(pow / 4) + random2(pow / 4) +
                         random2(pow / 4)),
                        KC_YOU);
            return 1;
        }

        obj = next;
    }

    return 0;
}

int make_a_normal_cloud(int x, int y, int pow, int spread_rate,
                        cloud_type ctype, kill_category whose)
{
    place_cloud( ctype, x, y,
                 (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                 whose, spread_rate );

    return 1;
}

static int _passwall(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    int dx, dy, nx = x, ny = y;
    int howdeep = 0;
    bool done = false;
    int shallow = 1 + (you.skills[SK_EARTH_MAGIC] / 8);
    bool non_rock_barriers = false;

    // Irony: you can start on a secret door but not a door.
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls... I'm not sure we should
    // even allow statues (should be contiguous rock). -- bwr
    // XXX: Allow statues as entry points?
    if (grd[x][y] != DNGN_ROCK_WALL && grd[x][y] != DNGN_CLEAR_ROCK_WALL)
    {
        mpr("That's not a passable wall.");
        return 0;
    }

    dx = x - you.x_pos;
    dy = y - you.y_pos;

    while (!done)
    {
        if (!in_bounds(nx, ny))
        {
            mpr("You sense an overwhelming volume of rock.");
            return 0;
        }

        switch (grd[nx][ny])
        {
        default:
            if (grid_is_solid(grd[nx][ny]))
                non_rock_barriers = true;
            done = true;
            break;

        case DNGN_ROCK_WALL:
        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_ORCISH_IDOL:
        case DNGN_GRANITE_STATUE:
        case DNGN_SECRET_DOOR:
            nx += dx;
            ny += dy;
            howdeep++;
            break;
        }
    }

    int range = shallow + random2(pow) / 25;

    if (howdeep > shallow || non_rock_barriers)
    {
        mprf("This rock feels %sdeep.",
             non_rock_barriers || (howdeep > range)? "extremely " : "");

        if (yesno("Try anyway?"))
        {
            if (howdeep > range || non_rock_barriers)
            {
                ouch(1 + you.hp, 0, KILLED_BY_PETRIFICATION);
                //jmf: not return; if wizard, successful transport is option
            }
        }
        else
        {
            if (one_chance_in(30))
                mpr("Wuss.");
            else
                canned_msg(MSG_OK);
            return 1;
        }
    }

    // Note that the delay was (1 + howdeep * 2), but now that the
    // delay is stopped when the player is attacked it can be much
    // shorter since its harder to use for quick escapes. -- bwr
    start_delay( DELAY_PASSWALL, 2 + howdeep, nx, ny );

    return 1;
}                               // end passwall()

void cast_passwall(int pow)
{
    apply_one_neighbouring_square(_passwall, pow);
}

static int _intoxicate_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER
        || mons_intel(menv[mon].type) < I_NORMAL
        || mons_holiness(&menv[mon]) != MH_NATURAL
        || mons_res_poison(&menv[mon]) > 0)
    {
        return 0;
    }

    menv[mon].add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU));
    return 1;
}

void cast_intoxicate(int pow)
{
    potion_effect( POT_CONFUSION, 10 + (100 - pow) / 10);

    if (one_chance_in(20)
        && lose_stat( STAT_INTELLIGENCE, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow);
}

bool backlight_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)
        return (false);

    switch (menv[mon].type)
    {
    case MONS_FIRE_VORTEX:
    case MONS_ANGEL:
    case MONS_FIEND:
    case MONS_SHADOW:
    case MONS_EFREET:
    case MONS_HELLION:
    case MONS_GLOWING_SHAPESHIFTER:
    case MONS_FIRE_ELEMENTAL:
    case MONS_AIR_ELEMENTAL:
    case MONS_SHADOW_FIEND:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_ORANGE_RAT:
    case MONS_BALRUG:
    case MONS_SPATIAL_VORTEX:
    case MONS_PIT_FIEND:
    case MONS_SHINING_EYE:
    case MONS_DAEVA:
    case MONS_SPECTRAL_THING:
    case MONS_ORB_OF_FIRE:
    case MONS_EYE_OF_DEVASTATION:
        return (false);               // Already glowing or invisible.
    default:
        break;
    }

    mon_enchant bklt = menv[mon].get_ench(ENCH_BACKLIGHT);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (menv[mon].has_ench(ENCH_INVIS))
    {
        if (!menv[mon].has_ench(ENCH_BACKLIGHT))
        {
            menv[mon].add_ench(
                mon_enchant(ENCH_BACKLIGHT, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message( &menv[mon], " is lined in light." );
        }
        return (true);
    }

    menv[mon].add_ench(mon_enchant(ENCH_BACKLIGHT, 1));

    if (lvl == 0)
        simple_monster_message( &menv[mon], " is outlined in light." );
    else if (lvl == 4)
        simple_monster_message( &menv[mon], " glows brighter for a moment." );
    else
        simple_monster_message( &menv[mon], " glows brighter." );

    return (true);
}

bool cast_evaporate(int pow, bolt& beem, int potion)
{
    if (potion == -1)
        return (false);
    else if (you.inv[potion].base_type != OBJ_POTIONS)
    {
        mpr( "This spell works only on potions!" );
        canned_msg(MSG_SPELL_FIZZLES);
        return (false);
    }

    beem.name        = "potion";
    beem.colour      = you.inv[potion].colour;
    beem.range       = 9;
    beem.rangeMax    = 9;
    beem.type        = dchar_glyph(DCHAR_FIRED_FLASK);
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU_MISSILE;
    beem.is_beam     = false;
    beem.aux_source.clear();

    beem.hit        = you.dex / 2 + roll_dice( 2, you.skills[SK_THROWING] / 2 + 1 );
    beem.damage     = dice_def( 1, 0 );  // no damage, just producing clouds
    beem.ench_power = pow;               // used for duration only?

    beem.flavour    = BEAM_POTION_STINKING_CLOUD;
    beam_type tracer_flavour = BEAM_MMISSILE;

    switch (you.inv[potion].sub_type)
    {
    case POT_STRONG_POISON:
        beem.flavour   = BEAM_POTION_POISON;
        tracer_flavour = BEAM_POISON;
        beem.ench_power *= 2;
        break;

    case POT_DEGENERATION:
        beem.effect_known = false;
        beem.flavour   = (coinflip() ? BEAM_POTION_POISON : BEAM_POTION_MIASMA);
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_POISON:
        beem.flavour   = BEAM_POTION_POISON;
        tracer_flavour = BEAM_POISON;
        break;

    case POT_DECAY:
        beem.flavour   = BEAM_POTION_MIASMA;
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_PARALYSIS:
        beem.ench_power *= 2;
        // fall through
    case POT_CONFUSION:
    case POT_SLOWING:
        tracer_flavour = beem.flavour = BEAM_POTION_STINKING_CLOUD;
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        tracer_flavour = beem.flavour = BEAM_POTION_STEAM;
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        if (one_chance_in(3))
            break; // stinking cloud
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beem.effect_known = false;
        beem.flavour = (coinflip() ? BEAM_POTION_FIRE : BEAM_POTION_STEAM);
        if (you.inv[potion].sub_type == POT_BERSERK_RAGE)
            tracer_flavour = BEAM_FIRE;
        else
            tracer_flavour = BEAM_RANDOM;
        break;

    case POT_MUTATION:
    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beem.effect_known = false;
        switch (random2(5))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;   break;
        case 1:   beem.flavour = BEAM_POTION_COLD;   break;
        case 2:   beem.flavour = BEAM_POTION_POISON; break;
        case 3:   beem.flavour = BEAM_POTION_MIASMA; break;
        default:  beem.flavour = BEAM_POTION_RANDOM; break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;

    default:
        beem.effect_known = false;
        switch (random2(12))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;            break;
        case 1:   beem.flavour = BEAM_POTION_STINKING_CLOUD;  break;
        case 2:   beem.flavour = BEAM_POTION_COLD;            break;
        case 3:   beem.flavour = BEAM_POTION_POISON;          break;
        case 4:   beem.flavour = BEAM_POTION_RANDOM;          break;
        case 5:   beem.flavour = BEAM_POTION_BLUE_SMOKE;      break;
        case 6:   beem.flavour = BEAM_POTION_BLACK_SMOKE;     break;
        case 7:   beem.flavour = BEAM_POTION_PURP_SMOKE;      break;
        default:  beem.flavour = BEAM_POTION_STEAM;           break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;
    }

    // Fire tracer.
    beem.source_x       = you.x_pos;
    beem.source_y       = you.y_pos;
    beem.can_see_invis  = player_see_invis();
    beem.smart_monster  = true;
    beem.attitude       = ATT_FRIENDLY;
    beem.fr_count       = 0;
    beem.beam_cancelled = false;
    beem.is_tracer      = true;

    beam_type real_flavour = beem.flavour;
    beem.flavour           = tracer_flavour;
    fire_beam(beem);

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies or at ourselves.
        canned_msg(MSG_OK);
        return (false);
    }

    if (coinflip())
        exercise( SK_THROWING, 1 );

    // Really fire.
    beem.flavour = real_flavour;
    beem.is_tracer = false;
    fire_beam(beem);

    // Use up a potion.
    if (is_blood_potion(you.inv[potion]))
        remove_oldest_blood_potion(you.inv[potion]);

    dec_inv_item_quantity( potion, 1 );

    return (true);
}                               // end cast_evaporate()

// The intent of this spell isn't to produce helpful potions
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not
// using up the corpse might also lead to game balance problems. -- bwr
void cast_fulsome_distillation( int powc )
{
    if (powc > 50)
        powc = 50;

    int corpse = -1;

    // Search items at the player's location for corpses.
    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            snprintf( info, INFO_SIZE, "Distill a potion from %s?",
                      si->name(DESC_NOCAP_THE).c_str() );

            if (yesno( info, true, 0, false ))
            {
                corpse = si->index();
                break;
            }
        }
    }

    if (corpse == -1)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    const bool rotten      = food_is_rotten(mitm[corpse]);
    const bool big_monster = (mons_type_hit_dice( mitm[corpse].plus ) >= 5);
    const bool power_up    = (rotten && big_monster);

    potion_type pot_type = POT_WATER;

    switch (mitm[corpse].plus)
    {
    case MONS_GIANT_BAT:             // extracting batty behaviour : 1
    case MONS_UNSEEN_HORROR:         // extracting batty behaviour : 7
    case MONS_GIANT_BLOWFLY:         // extracting batty behaviour : 5
        pot_type = POT_CONFUSION;
        break;

    case MONS_RED_WASP:              // paralysis attack : 8
    case MONS_YELLOW_WASP:           // paralysis attack : 4
        pot_type = POT_PARALYSIS;
        break;

    case MONS_SNAKE:                 // clean meat, but poisonous attack : 2
    case MONS_GIANT_ANT:             // clean meat, but poisonous attack : 3
        pot_type = (power_up ? POT_POISON : POT_CONFUSION);
        break;

    case MONS_ORANGE_RAT:            // poisonous meat, but draining attack : 3
        pot_type = (power_up ? POT_DECAY : POT_POISON);
        break;

    case MONS_SPINY_WORM:            // 12
        pot_type = (power_up ? POT_DECAY : POT_STRONG_POISON);
        break;

    default:
        switch (mons_corpse_effect( mitm[corpse].plus ))
        {
        case CE_CLEAN:
            pot_type = (power_up ? POT_CONFUSION : POT_WATER);
            break;

        case CE_CONTAMINATED:
            pot_type = (power_up ? POT_DEGENERATION : POT_POISON);
            break;

        case CE_POISONOUS:
            pot_type = (power_up ? POT_STRONG_POISON : POT_POISON);
            break;

        case CE_MUTAGEN_RANDOM:
        case CE_MUTAGEN_GOOD:   // unused
        case CE_RANDOM:         // unused
            pot_type = POT_MUTATION;
            break;

        case CE_MUTAGEN_BAD:    // unused
        case CE_ROTTEN:         // actually this only occurs via mangling
        case CE_HCL:            // necrophage
            pot_type = (power_up ? POT_DECAY : POT_STRONG_POISON);
            break;

        case CE_NOCORPSE:       // shouldn't occur
        default:
            break;
        }
        break;
    }

    // If not powerful enough, we downgrade the potion.
    if (random2(50) > powc + 10 * rotten)
    {
        switch (pot_type)
        {
        case POT_DECAY:
        case POT_DEGENERATION:
        case POT_STRONG_POISON:
            pot_type = POT_POISON;
            break;

        case POT_MUTATION:
        case POT_POISON:
            pot_type = POT_CONFUSION;
            break;

        case POT_PARALYSIS:
            pot_type = POT_SLOWING;
            break;

        case POT_CONFUSION:
        case POT_SLOWING:
        default:
            pot_type = POT_WATER;
            break;
        }
    }

    // We borrow the corpse's object to make our potion.
    mitm[corpse].base_type = OBJ_POTIONS;
    mitm[corpse].sub_type  = pot_type;
    mitm[corpse].quantity  = 1;
    mitm[corpse].plus      = 0;
    mitm[corpse].plus2     = 0;
    mitm[corpse].flags     = 0;
    mitm[corpse].inscription.clear();
    item_colour( mitm[corpse] );  // sets special as well

    mprf("You extract %s from the corpse.",
         mitm[corpse].name(DESC_NOCAP_A).c_str());

    // Try to move the potion to the player (for convenience).
    if (move_item_to_player( corpse, 1 ) != 1)
        mpr( "Unfortunately, you can't carry it right now!" );
}

static int _rot_living(int x, int y, int pow, int message)
{
    UNUSED( message );

    int mon = mgrd[x][y];
    int ench;

    if (mon == NON_MONSTER)
        return 0;

    if (mons_holiness(&menv[mon]) != MH_NATURAL)
        return 0;

    if (check_mons_resist_magic(&menv[mon], pow))
        return 0;

    ench = ((random2(pow) + random2(pow) + random2(pow) + random2(pow)) / 4);
    ench = 1 + (ench >= 20) + (ench >= 35) + (ench >= 50);

    menv[mon].add_ench( mon_enchant(ENCH_ROT, ench, KC_YOU) );

    return 1;
}

static int _rot_undead(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    int mon = mgrd[x][y];
    int ench;

    if (mon == NON_MONSTER)
        return 0;

    if (mons_holiness(&menv[mon]) != MH_UNDEAD)
        return 0;

    if (check_mons_resist_magic(&menv[mon], pow))
        return 0;

    // This does not make sense -- player mummies are
    // immune to rotting (or have been) -- so what is
    // the schema in use here to determine rotting??? {dlb}

    //jmf: Up for discussion. it is clearly unfair to
    //     rot player mummies.
    //     the `schema' here is: corporeal non-player undead
    //     rot, discorporeal undead don't rot. if you wanna
    //     insist that monsters get the same treatment as
    //     players, I demand my player mummies get to worship
    //     the evil mummy & orc gods.
    switch (menv[mon].type)
    {
    case MONS_ZOMBIE_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_MUMMY:
    case MONS_GUARDIAN_MUMMY:
    case MONS_GREATER_MUMMY:
    case MONS_MUMMY_PRIEST:
    case MONS_GHOUL:
    case MONS_NECROPHAGE:
    case MONS_VAMPIRE:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
    case MONS_LICH:
    case MONS_ANCIENT_LICH:
    case MONS_WIGHT:
    case MONS_BORIS:
        break;
    case MONS_ROTTING_HULK:
    default:
        return 0;               // Immune (no flesh) or already rotting.
    }

    ench = ((random2(pow) + random2(pow) + random2(pow) + random2(pow)) / 4);
    ench = 1 + (ench >= 20) + (ench >= 35) + (ench >= 50);

    menv[mon].add_ench( mon_enchant(ENCH_ROT, ench, KC_YOU) );

    return 1;
}

static int _rot_corpses(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    return _make_a_rot_cloud(x, y, pow, CLOUD_MIASMA);
}

void cast_rotting(int pow)
{
    apply_area_visible(_rot_living, pow);
    apply_area_visible(_rot_undead, pow);
    apply_area_visible(_rot_corpses, pow);
    return;
}

void do_monster_rot(int mon)
{
    int damage = 1 + random2(3);

    if (mons_holiness(&menv[mon]) == MH_UNDEAD && !one_chance_in(5))
    {
        apply_area_cloud(make_a_normal_cloud, menv[mon].x, menv[mon].y,
                         10, 1, CLOUD_MIASMA, KC_YOU);
    }

    player_hurt_monster( mon, damage );
    return;
}

static int _snake_charm_monsters(int x, int y, int pow, int message)
{
    UNUSED( message );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER
        || one_chance_in(4)
        || mons_friendly(&menv[mon])
        || mons_char(menv[mon].type) != 'S'
        || check_mons_resist_magic(&menv[mon], pow))
    {
        return 0;
    }

    menv[mon].attitude = ATT_FRIENDLY;
    mprf("%s sways back and forth.", menv[mon].name(DESC_CAP_THE).c_str());

    return 1;
}

void cast_snake_charm(int pow)
{
    // powc = (you.experience_level * 2) + (you.skills[SK_INVOCATIONS] * 3);
    apply_one_neighbouring_square(_snake_charm_monsters, pow);
}

void cast_fragmentation(int pow)        // jmf: ripped idea from airstrike
{
    struct dist beam;
    struct bolt blast;
    int debris = 0;
    int trap;
    bool explode = false;
    bool hole = true;
    const char *what = NULL;

    mpr("Fragment what (e.g. a wall or monster)?", MSGCH_PROMPT);
    direction( beam, DIR_TARGET, TARG_ENEMY );

    if (!beam.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    //FIXME: If (player typed '>' to attack floor) goto do_terrain;
    blast.beam_source = MHITYOU;
    blast.thrower     = KILL_YOU;
    blast.ex_size     = 1;              // default
    blast.type        = '#';
    blast.colour      = 0;
    blast.source_x    = you.x_pos;
    blast.source_y    = you.y_pos;
    blast.flavour     = BEAM_FRAG;
    blast.hit         = AUTOMATIC_HIT;
    blast.is_tracer   = false;
    blast.set_target(beam);
    blast.aux_source.clear();

    // Number of dice vary... 3 is easy/common, but it can get as high as 6.
    blast.damage = dice_def( 0, 5 + pow / 10 );

    const int grid = grd[beam.tx][beam.ty];
    const int mon  = mgrd[beam.tx][beam.ty];

    const bool okay_to_dest = in_bounds(beam.target());

    if (mon != NON_MONSTER)
    {
        // This needs its own hand_buff... we also need to do it first
        // in case the target dies. -- bwr
        char explode_msg[80];

        snprintf( explode_msg, sizeof( explode_msg ), "%s explodes!",
                  menv[mon].name(DESC_CAP_THE).c_str() );

        switch (menv[mon].type)
        {
        case MONS_ICE_STATUE:           // blast of ice fragments
        case MONS_ICE_BEAST:
        case MONS_SIMULACRUM_SMALL:
        case MONS_SIMULACRUM_LARGE:
            explode          = true;
            blast.name       = "icy blast";
            blast.colour     = WHITE;
            blast.damage.num = 2;
            blast.flavour    = BEAM_ICE;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 1;
            break;

        case MONS_FLYING_SKULL:         // blast of bone
        case MONS_SKELETON_SMALL:
        case MONS_SKELETON_LARGE:
        case MONS_SKELETAL_DRAGON:
        case MONS_SKELETAL_WARRIOR:
            explode = true;

            mprf("The %s explodes into sharp fragments of bone!",
                 (menv[mon].type == MONS_FLYING_SKULL) ? "skull" : "skeleton");

            blast.name = "blast of bone shards";

            blast.colour = LIGHTGREY;

            if (x_chance_in_y(pow / 5, 50))        // potential insta-kill
            {
                monster_die(&menv[mon], KILL_YOU, 0);
                blast.damage.num = 4;
            }
            else
            {
                blast.damage.num = 2;
                if (player_hurt_monster(mon, roll_dice( blast.damage )))
                    blast.damage.num = 4;
            }
            goto all_done;      // i.e. no "Foo Explodes!"

        case MONS_WOOD_GOLEM:
            explode = false;
            simple_monster_message(&menv[mon], " shudders violently!");

            // We use blast.damage not only for inflicting damage here,
            // but so that later on we'll know that the spell didn't
            // fizzle (since we don't actually explode wood golems). -- bwr
            blast.damage.num = 2;
            player_hurt_monster( mon, roll_dice( blast.damage ) );
            break;

        case MONS_IRON_GOLEM:
        case MONS_METAL_GARGOYLE:
            explode          = true;
            blast.name       = "blast of metal fragments";
            blast.colour     = CYAN;
            blast.damage.num = 4;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 2;
            break;

        case MONS_CLAY_GOLEM:   // Assume baked clay and not wet loam.
        case MONS_STONE_GOLEM:
        case MONS_EARTH_ELEMENTAL:
        case MONS_GARGOYLE:
            explode          = true;
            blast.ex_size    = 2;
            blast.name       = "blast of rock fragments";
            blast.colour     = BROWN;
            blast.damage.num = 3;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 1;
            break;

        case MONS_SILVER_STATUE:
        case MONS_ORANGE_STATUE:
            explode       = true;
            blast.ex_size = 2;
            if (menv[mon].type == MONS_SILVER_STATUE)
            {
                blast.name       = "blast of silver fragments";
                blast.colour     = WHITE;
                blast.damage.num = 3;
            }
            else
            {
                blast.name       = "blast of orange crystal shards";
                blast.colour     = LIGHTRED;
                blast.damage.num = 6;
            }

            {
                int statue_damage = roll_dice(blast.damage) * 2;
                if (pow >= 50 && one_chance_in(10))
                    statue_damage = menv[mon].hit_points;

                if (player_hurt_monster(mon, statue_damage))
                    blast.damage.num += 2;
            }
            break;

        case MONS_CRYSTAL_GOLEM:
            explode          = true;
            blast.ex_size    = 2;
            blast.name       = "blast of crystal shards";
            blast.colour     = WHITE;
            blast.damage.num = 4;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 2;
            break;

        default:
            blast.damage.num = 1;  // to mark that a monster was targetted

            // Yes, this spell does lousy damage if the
            // monster isn't susceptible. -- bwr
            player_hurt_monster( mon, roll_dice( 1, 5 + pow / 25 ) );
            goto do_terrain;
        }

        mpr( explode_msg );
        goto all_done;
    }

  do_terrain:
    // FIXME: do nothing in Abyss & Pandemonium?

    switch (grid)
    {
    //
    // Stone and rock terrain
    //
    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        blast.colour = env.rock_colour;
        // fall-through
    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        what = "wall";
        if (player_in_branch( BRANCH_HALL_OF_ZOT ))
            blast.colour = env.rock_colour;
        // fall-through
    case DNGN_ORCISH_IDOL:
        if (what == NULL)
            what = "stone idol";
        if (blast.colour == 0)
            blast.colour = DARKGREY;
        // fall-through
    case DNGN_GRANITE_STATUE:   // normal rock -- big explosion
        if (what == NULL)
            what = "statue";

        explode = true;

        blast.name       = "blast of rock fragments";
        blast.damage.num = 3;
        if (blast.colour == 0)
            blast.colour = LIGHTGREY;

        if (okay_to_dest
            && (grid == DNGN_ORCISH_IDOL
                || grid == DNGN_GRANITE_STATUE
                || pow >= 40 && grid == DNGN_ROCK_WALL && one_chance_in(3)
                || pow >= 40 && grid == DNGN_CLEAR_ROCK_WALL
                   && one_chance_in(3)
                || pow >= 60 && grid == DNGN_STONE_WALL && one_chance_in(10)
                || pow >= 60 && grid == DNGN_CLEAR_STONE_WALL
                   && one_chance_in(10)))
        {
            // terrain blew up real good:
            blast.ex_size         = 2;
            grd[beam.tx][beam.ty] = DNGN_FLOOR;
            debris                = DEBRIS_ROCK;
        }
        break;

    //
    // Metal -- small but nasty explosion
    //

    case DNGN_METAL_WALL:
        what             = "metal wall";
        blast.colour     = CYAN;
        explode          = true;
        blast.name       = "blast of metal fragments";
        blast.damage.num = 4;

        if (okay_to_dest && pow >= 80 && x_chance_in_y(pow / 5, 500))
        {
            blast.damage.num += 2;
            grd[beam.tx][beam.ty] = DNGN_FLOOR;
            debris = DEBRIS_METAL;
        }
        break;

    //
    // Crystal
    //

    case DNGN_GREEN_CRYSTAL_WALL:       // crystal -- large & nasty explosion
        what             = "crystal wall";
        blast.colour     = GREEN;
        explode          = true;
        blast.ex_size    = 2;
        blast.name       = "blast of crystal shards";
        blast.damage.num = 5;

        if (okay_to_dest && grid == DNGN_GREEN_CRYSTAL_WALL && coinflip())
        {
            blast.ex_size = coinflip() ? 3 : 2;
            grd[beam.tx][beam.ty] = DNGN_FLOOR;
            debris = DEBRIS_CRYSTAL;
        }
        break;

    //
    // Traps
    //

    case DNGN_UNDISCOVERED_TRAP:
    case DNGN_TRAP_MECHANICAL:
        trap = trap_at_xy( beam.tx, beam.ty );
        if (trap != -1
            && trap_category( env.trap[trap].type ) != DNGN_TRAP_MECHANICAL)
        {
            // Non-mechanical traps don't explode with this spell. -- bwr
            break;
        }

        // Undiscovered traps appear as exploding from the floor. -- bwr
        what = ((grid == DNGN_UNDISCOVERED_TRAP) ? "floor" : "trap");

        explode          = true;
        hole             = false;    // to hit monsters standing on traps
        blast.name       = "blast of fragments";
        blast.colour     = env.floor_colour;  // in order to blend in
        blast.damage.num = 2;

        // Exploded traps are nonfunctional, ammo is also ruined -- bwr
        if (okay_to_dest)
        {
            grd[beam.tx][beam.ty] = DNGN_FLOOR;
            env.trap[trap].type = TRAP_UNASSIGNED;
        }
        break;

    //
    // Stone doors and arches
    //

    case DNGN_OPEN_DOOR:
    case DNGN_CLOSED_DOOR:
        // Doors always blow up, stone arches never do (would cause problems).
        if (okay_to_dest)
            grd[beam.tx][beam.ty] = DNGN_FLOOR;

        // fall-through
    case DNGN_STONE_ARCH:          // Floor -- small explosion.
        explode          = true;
        hole             = false;  // to hit monsters standing on doors
        blast.name       = "blast of rock fragments";
        blast.colour     = LIGHTGREY;
        blast.damage.num = 2;
        break;

    //
    // Permarock and floor are unaffected -- bwr
    //
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
    case DNGN_FLOOR:
        explode = false;
        mprf("%s seems to be unnaturally hard.",
             (grid == DNGN_FLOOR) ? "The dungeon floor"
                                  : "That wall");
        break;

    default:
        // FIXME: cute message for water?
        break;
    }

  all_done:
    if (explode && blast.damage.num > 0)
    {
        if (what != NULL)
            mprf("The %s explodes!", what);

        explosion( blast, hole, true );

        if (grid == DNGN_ORCISH_IDOL)
            beogh_idol_revenge();
    }
    else if (blast.damage.num == 0)
    {
        // If damage dice are zero we assume that nothing happened at all.
        canned_msg(MSG_SPELL_FIZZLES);
    }
}                               // end cast_fragmentation()

void cast_twist(int pow)
{
    struct dist targ;
    struct bolt tmp;    // used, but ignored

    // level one power cap -- bwr
    if (pow > 25)
        pow = 25;

    // Get target, using DIR_TARGET for targetting only,
    // since we don't use fire_beam() for this spell.
    if (!spell_direction(targ, tmp, DIR_TARGET))
        return;

    const int mons = mgrd[ targ.tx ][ targ.ty ];

    // Anything there?
    if (mons == NON_MONSTER || targ.isMe)
    {
        mpr("There is no monster there!");
        return;
    }

    // Monster can magically save vs attack.
    if (check_mons_resist_magic( &menv[ mons ], pow * 2 ))
    {
        simple_monster_message( &menv[mons], mons_immune_magic(&menv[mons]) ?
                                " is unaffected." : " resists." );
        return;
    }

    // Roll the damage... this spell is pretty low on damage, because
    // it can target any monster in LOS (high utility).  This is
    // similar to the damage done by Magic Dart (although, the
    // distribution is much more uniform). -- bwr
    int damage = 1 + random2( 3 + pow / 5 );

    // Inflict the damage.
    player_hurt_monster( mons, damage );
}

bool cast_portal_projectile(int pow)
{
    if (pow > 50)
        pow = 50;

    dist target;
    int item = get_ammo_to_shoot(-1, target, true);
    if (item == -1)
        return (false);

    if (grid_is_solid(target.tx, target.ty))
    {
        mpr("You can't shoot at gazebos.");
        return (false);
    }

    // Can't use portal through walls. (That'd be just too cheap!)
    if (trans_wall_blocking( target.tx, target.ty ))
    {
        mpr("A translucent wall is in the way.");
        return (false);
    }

    if (!check_warning_inscriptions(you.inv[item], OPER_FIRE))
        return (false);

    bolt beam;
    throw_it( beam, item, true, random2(pow/4), &target );

    return (true);
}

//
// This version of far strike is a bit too creative for level one, in
// order to make it work we needed to put a lot of restrictions on it
// (like the damage limitation), which wouldn't be necessary if it were
// a higher level spell.  This code might come back as a high level
// translocation spell later (maybe even with special effects if it's
// using some of Josh's ideas about occasionally losing the weapon).
// Would potentially make a good high-level, second book Warper spell
// (since Translocations is a utility school, it should be higher level
// that usual... especially if it turns into a flavoured smiting spell).
// This can all wait until after the next release (as it would be better
// to have a proper way to do a single weapon strike here (you_attack
// does far more than we need or want here)). --bwr
//
void cast_far_strike(int pow)
{
    dist targ;
    bolt tmp;    // used, but ignored

    // Get target, using DIR_TARGET for targetting only,
    // since we don't use fire_beam() for this spell.
    if (!spell_direction(targ, tmp, DIR_TARGET))
        return;

    // Get the target monster...
    if (mgrd[targ.tx][targ.ty] == NON_MONSTER || targ.isMe)
    {
        mpr("There is no monster there!");
        return;
    }

    if (trans_wall_blocking( targ.tx, targ.ty ))
    {
        mpr("A translucent wall is in the way.");
        return;
    }

    //  Start with weapon base damage...

    int damage = 3;     // default unarmed damage
    int speed  = 10;    // default unarmed time

    if (you.weapon())   // if not unarmed
    {
        const item_def& wpn(*you.weapon());
        // Look up the base damage.
        if (wpn.base_type == OBJ_WEAPONS)
        {
            damage = property( wpn, PWPN_DAMAGE );
            speed  = property( wpn, PWPN_SPEED );

            if (get_weapon_brand(wpn) == SPWPN_SPEED)
                speed /= 2;
        }
        else if (item_is_staff(wpn))
        {
            damage = property(wpn, PWPN_DAMAGE );
            speed = property(wpn, PWPN_SPEED );
        }
    }

    // Because we're casting a spell (and don't want to make this level
    // one spell too good), we're not applying skill speed bonuses and at
    // the very least guaranteeing one full turn (speed == 10) like the
    // other spells (if any thing else related to speed is changed, at
    // least leave this right before the application to you.time_taken).
    // Leaving skill out of the speed bonus is an important part of
    // keeping this spell from becoming a "better than actual melee"
    // spell... although, it's fine if that's the case for early Warpers,
    // Fighter types, and such that pick up this trivial first level spell,
    // shouldn't be using it instead of melee (but rather as an accessory
    // long range plinker).  Therefore, we tone things down to try and
    // guarantee that the spell is never begins to approach real combat
    // (although the magic resistance check might end up with a higher
    // hit rate than attacking against EV for high level Warpers). -- bwr
    if (speed < 10)
        speed = 10;

    you.time_taken *= speed;
    you.time_taken /= 10;

    // Apply strength only to damage (since we're only interested in
    // force here, not finesse... the dex/to-hit part of combat is
    // instead handled via magical ability).  This part could probably
    // just be removed, as it's unlikely to make any real difference...
    // if it is, the Warper stats in newgame.cc should be changed back
    // to the standard 6 int-4 dex of spellcasters. -- bwr
    int dammod = 78;
    const int dam_stat_val = you.strength;

    if (dam_stat_val > 11)
        dammod += (random2(dam_stat_val - 11) * 2);
    else if (dam_stat_val < 9)
        dammod -= (random2(9 - dam_stat_val) * 3);

    damage *= dammod;
    damage /= 78;

    monsters *monster = &menv[ mgrd[targ.tx][targ.ty] ];

    // Apply monster's AC.
    if (monster->ac > 0)
        damage -= random2( 1 + monster->ac );

    // Roll the damage...
    damage = 1 + random2( damage );

    // Monster can magically save vs attack (this could be replaced or
    // augmented with an EV check).
    if (check_mons_resist_magic( monster, pow * 2 ))
    {
        simple_monster_message( monster, mons_immune_magic(monster) ?
                                " is unaffected." : " resists." );
        return;
    }

    // Inflict the damage.
    hurt_monster( monster, damage );
    if (monster->hit_points < 1)
        monster_die( monster, KILL_YOU, 0 );
    else
        print_wounds( monster );

    return;
}

int cast_apportation(int pow)
{
    dist beam;

    mpr("Pull items from where?");

    direction( beam, DIR_TARGET, TARG_ANY );

    if (!beam.isValid)
    {
        canned_msg(MSG_OK);
        return (-1);
    }

    // it's already here!
    if (beam.isMe)
    {
        mpr( "That's just silly." );
        return (-1);
    }

    if (trans_wall_blocking( beam.tx, beam.ty ))
    {
        mpr("A translucent wall is in the way.");
        return (0);
    }

    // Protect the player from destroying the item
    const dungeon_feature_type grid = grd[ you.x_pos ][ you.y_pos ];

    if (grid_destroys_items(grid))
    {
        mpr( "That would be silly while over this terrain!" );
        return (0);
    }

    // If this is ever changed to allow moving objects that can't
    // be seen, it should at least only allow moving from squares
    // that have been phyisically (and maybe magically) seen and
    // should probably have a range check as well.  In these cases
    // the spell should probably be upped to at least two, or three
    // if magic mapped squares are allowed.  Right now it's okay
    // at one... it has a few uses, but you still have to get line
    // of sight to the object first so it will only help a little
    // with snatching runes or the orb (although it can be quite
    // useful for getting items out of statue rooms or the abyss). -- bwr
    if (!see_grid( beam.tx, beam.ty ))
    {
        mpr( "You cannot see there!" );
        return (0);
    }

    if ((beam.tx == (find_floor_item(OBJ_ORBS,ORB_ZOT)->x)) && (beam.ty == (find_floor_item(OBJ_ORBS,ORB_ZOT)->y)))
    {
        mpr( "You cannot apport the sacred Orb." );
        return(0);
    }

    // Let's look at the top item in that square...
    const int item = igrd[ beam.tx ][ beam.ty ];
    if (item == NON_ITEM)
    {
        const int  mon = mgrd[ beam.tx ][ beam.ty ];

        if (mon == NON_MONSTER)
            mpr( "There are no items there." );
        else if (mons_is_mimic( menv[ mon ].type ))
        {
            mprf("%s twitches.", menv[mon].name(DESC_CAP_THE).c_str());
        }
        else
            mpr( "This spell does not work on creatures." );

        return (0);
    }

    // Mass of one unit.
    const int unit_mass = item_mass( mitm[ item ] );
    // Assume we can pull everything.
    int max_units = mitm[ item ].quantity;

    // Item has mass: might not move all of them.
    if (unit_mass > 0)
    {
        const int max_mass = pow * 30 + random2( pow * 20 );

        // Most units our power level will allow.
        max_units = max_mass / unit_mass;
    }

    if (max_units <= 0)
    {
        mpr( "The mass is resisting your pull." );
        return (0);
    }

    int done = 0;

    // Failure should never really happen after all the above checking,
    // but we'll handle it anyways...
    if (move_top_item( beam.target(), you.pos() ))
    {
        if (max_units < mitm[ item ].quantity)
        {
            mitm[ item ].quantity = max_units;
            mpr( "You feel that some mass got lost in the cosmic void." );
        }
        else
        {
            mpr( "Yoink!" );
            mprf("You pull the item%s to yourself.",
                 (mitm[ item ].quantity > 1) ? "s" : "" );
        }
        done = 1;

        // If we apport a net, free the monster under it.
        if (mitm[item].base_type == OBJ_MISSILES
            && mitm[item].sub_type == MI_THROWING_NET
            && item_is_stationary(mitm[item]))
        {
           const int mon = mgrd[ beam.tx ][ beam.ty ];
           remove_item_stationary(mitm[item]);

           if (mon != NON_MONSTER)
               (&menv[mon])->del_ench(ENCH_HELD, true);
        }
    }
    else
        mpr( "The spell fails." );

    return (done);
}

bool cast_sandblast(int pow, bolt &beam)
{
    bool big = false;

    if (you.weapon())
    {
        const item_def& wpn(*you.weapon());
        big = (wpn.base_type == OBJ_MISSILES
               && (wpn.sub_type == MI_STONE || wpn.sub_type == MI_LARGE_ROCK));
    }

    bool success = zapping(big ? ZAP_SANDBLAST
                               : ZAP_SMALL_SANDBLAST, pow, beam, true);

    if (big && success)
        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );

    return (success);
}

void cast_condensation_shield(int pow)
{
    if (you.shield() || you.duration[DUR_FIRE_SHIELD])
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr("The disc of vapour around you crackles some more.");
            you.duration[DUR_CONDENSATION_SHIELD] += 5 + roll_dice(2, 3);
        }
        else
        {
            mpr("A crackling disc of dense vapour forms in the air!");
            you.redraw_armour_class = true;

            you.duration[DUR_CONDENSATION_SHIELD] = 10 + roll_dice(2, pow / 5);
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 30)
            you.duration[DUR_CONDENSATION_SHIELD] = 30;
    }

    return;
}

void remove_divine_shield()
{
    mpr("Your divine shield disappears!", MSGCH_DURATION);
    you.duration[DUR_DIVINE_SHIELD] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    you.redraw_armour_class = true;
}

// shield bonus = attribute for duration turns, then decreasing by 1
//                every two out of three turns
// overall shield duration = duration + attribute
// recasting simply resets those two values (to better values, presumably)
void cast_divine_shield()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
    {
        you.redraw_armour_class = true;
        if (you.shield() || you.duration[DUR_FIRE_SHIELD]
            || you.duration[DUR_CONDENSATION_SHIELD])
        {
            mprf("Your shield is strengthened by %s's divine power.",
                 god_name(you.religion).c_str());
        }
        else
            mpr("A divine shield forms around you!");
    }

    // duration of complete shield bonus from 35 to 80 turns
    you.duration[DUR_DIVINE_SHIELD]
       = 35 + (you.skills[SK_SHIELDS] + you.skills[SK_INVOCATIONS]*4)/3;

    // shield bonus up to 8
    you.attribute[ATTR_DIVINE_SHIELD] = 3 + you.skills[SK_SHIELDS]/5;

    you.redraw_armour_class = true;
}

static int _quadrant_blink(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    if (x == you.x_pos && y == you.y_pos)
        return (0);

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( false );
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;
        return (1);
    }

    if (pow > 100)
        pow = 100;

    const int dist = random2(6) + 2;  // 2-7

    // This is where you would *like* to go.
    const int base_x = you.x_pos + (x - you.x_pos) * dist;
    const int base_y = you.y_pos + (y - you.y_pos) * dist;

    // This can take a while if pow is high and there's lots of translucent
    // walls nearby.
    int tx, ty;
    bool found = false;
    for ( int i = 0; i < (pow*pow) / 500 + 1; ++i )
    {
        // Find a space near our base point...
        // First try to find a random square not adjacent to the basepoint,
        // then one adjacent if that fails.
        if (!random_near_space(base_x, base_y, tx, ty)
            && !random_near_space(base_x, base_y, tx, ty, true))
        {
            return 0;
        }

        // ... which is close enough, and also far enough from us.
        if (distance(base_x, base_y, tx, ty) > 10
            || distance(you.x_pos, you.y_pos, tx, ty) < 8)
        {
            continue;
        }

        if (!see_grid_no_trans(tx, ty))
            continue;

        found = true;
        break;
    }

    if (!found)
        return(0);

    you.moveto(tx, ty);
    return 1;
}

int cast_semi_controlled_blink(int pow)
{
    return apply_one_neighbouring_square(_quadrant_blink, pow);
}

void cast_stoneskin(int pow)
{
    if (you.is_undead
        && (you.species != SP_VAMPIRE || you.hunger_state < HS_SATIATED))
    {
        mpr("This spell does not affect your undead flesh.");
        return;
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_STATUE
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_BLADE_HANDS)
    {
        mpr("This spell does not affect your current form.");
        return;
    }

    if (you.duration[DUR_STONEMAIL] || you.duration[DUR_ICY_ARMOUR])
    {
        mpr("This spell conflicts with another spell still in effect.");
        return;
    }

    if (you.duration[DUR_STONESKIN])
        mpr( "Your skin feels harder." );
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
            mpr( "Your stone body feels more resilient." );
        else
            mpr( "Your skin hardens." );

        you.redraw_armour_class = true;
    }

    you.duration[DUR_STONESKIN] += 10 + random2(pow) + random2(pow);

    if (you.duration[DUR_STONESKIN] > 50)
        you.duration[DUR_STONESKIN] = 50;
}

/*
 *  File:       spells4.cc
 *  Summary:    new spells, focusing on transmigration, divination and
 *              other neglected areas of Crawl magic ;^)
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 *
 *  Change History (most recent first):
 *
 *   <2> 29jul2000  jdj  Made a zillion functions static.
 *   <1> 06jan2000  jmf  Created
 */

#include "AppHdr.h"

#include <string>
#include <stdio.h>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "direct.h"
#include "dungeon.h"
#include "effects.h"
#include "it_use2.h"
#include "itemname.h"
#include "items.h"
#include "invent.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "spells1.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
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

static bool mons_can_host_shuggoth(int type);
// static int make_a_random_cloud(int x, int y, int pow, int ctype);
static int make_a_rot_cloud(int x, int y, int pow, int ctype);
static int quadrant_blink(int x, int y, int pow, int garbage);

//void cast_animate_golem(int pow); // see actual function for reasoning {dlb}
//void cast_detect_magic(int pow);  //jmf: as above...
//void cast_eringyas_surprising_bouquet(int powc);
void do_monster_rot(int mon);

//jmf: FIXME: put somewhere else (misc.cc?)
// A feeble attempt at Nethack-like completeness for cute messages.
const char *your_hand( bool plural )
{
    static char hand_buff[80];

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    default:
        mpr("ERROR: unknown transformation in your_hand() (spells4.cc)");
    case TRAN_NONE:
    case TRAN_STATUE:
        if (you.species == SP_TROLL || you.species == SP_GHOUL)
        {
            strcpy(hand_buff, "claw");
            break;
        }
        // or fall-through
    case TRAN_ICE_BEAST:
    case TRAN_LICH:
        strcpy(hand_buff, "hand");
        break;
    case TRAN_SPIDER:
        strcpy(hand_buff, "front leg");
        break;
    case TRAN_SERPENT_OF_HELL:
    case TRAN_DRAGON:
        strcpy(hand_buff, "foreclaw");
        break;
    case TRAN_BLADE_HANDS:
        strcpy(hand_buff, "scythe-like blade");
        break;
    case TRAN_AIR:
        strcpy(hand_buff, "misty tendril");
        break;
    }

    if (plural)
        strcat(hand_buff, "s");

    return (hand_buff);
}

// I need to make some debris for metal, crystal and stone.
// They could go in OBJ_MISSILES, but I think I'd rather move
// MI_LARGE_ROCK into OBJ_DEBRIS and code giants to throw any
// OBJ_DEBRIS they get their meaty mits on.
static void place_debris(int x, int y, int debris_type)
{
#ifdef USE_DEBRIS_CODE
    switch (debris_type)
    {
    // hate to say this, but the first parameter only allows specific quantity
    // for *food* and nothing else -- and I would hate to see that parameter
    // (force_unique) abused any more than it already has been ... {dlb}:
    case DEBRIS_STONE:
        large = items( random2(3), OBJ_MISSILES, MI_LARGE_ROCK, true, 1, 250 );
        small = items( 3 + random2(6) + random2(6) + random2(6),
                       OBJ_MISSILES, MI_STONE, true, 1, 250 );
        break;
    case DEBRIS_METAL:
    case DEBRIS_WOOD:
    case DEBRIS_CRYSTAL:
        break;
    }

    if (small != NON_ITEM)
        move_item_to_grid( &small, x, y );

    if (large != NON_ITEM)
        move_item_to_grid( &large, x, y );

#else
    UNUSED( x );
    UNUSED( y );
    UNUSED( debris_type );
    return;
#endif
}                               // end place_debris()

// just to avoid typing this over and over
// now returns true if monster died -- bwr
inline bool player_hurt_monster(int monster, int damage)
{
    ASSERT( monster != NON_MONSTER );

    if (damage > 0)
    {
        hurt_monster( &menv[monster], damage );

        if (menv[monster].hit_points > 0)
            print_wounds( &menv[monster] );
        else
        {
            monster_die( &menv[monster], KILL_YOU, 0 );
            return (true);
        }
    }

    return (false);
}                               // end player_hurt_monster()


// Here begin the actual spells:
static int shatter_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    dice_def   dam_dice( 0, 5 + pow / 4 );  // number of dice set below
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
        dam_dice.num = 4;
        break;

    case MONS_SKELETON_SMALL: // double damage
    case MONS_SKELETON_LARGE:
    case MONS_CURSE_SKULL:
    case MONS_CLAY_GOLEM:
    case MONS_STONE_GOLEM:
    case MONS_IRON_GOLEM:
    case MONS_CRYSTAL_GOLEM:
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

    int damage = roll_dice( dam_dice ) - random2( menv[monster].armour_class );

    if (damage > 0)
        player_hurt_monster( monster, damage );
    else 
        damage = 0;

    return (damage);
}                               // end shatter_monsters()

static int shatter_items(int x, int y, int pow, int garbage)
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
            mpr("You hear glass break.");

        return 1;
    }

    return 0;
}                               // end shatter_items()

static int shatter_walls(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    int  chance = 0;
    int  stuff = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (x <= 5 || x >= GXM - 5 || y <= 5 || y >= GYM - 5)
        return (0);

    switch (grd[x][y])
    {
    case DNGN_SECRET_DOOR:
        if (see_grid(x, y))
            mpr("A secret door shatters!");
        grd[x][y] = DNGN_FLOOR;
        stuff = DEBRIS_WOOD;
        chance = 100;
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_OPEN_DOOR:
        if (see_grid(x, y))
            mpr("A door shatters!");
        grd[x][y] = DNGN_FLOOR;
        stuff = DEBRIS_WOOD;
        chance = 100;
        break;

    case DNGN_METAL_WALL:
    case DNGN_SILVER_STATUE:
        stuff = DEBRIS_METAL;
        chance = pow / 10;
        break;

    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        chance = 50;
        stuff = DEBRIS_STONE;
        break;

    case DNGN_STONE_WALL:
        chance = pow / 6;
        stuff = DEBRIS_STONE;
        break;

    case DNGN_ROCK_WALL:
        chance = pow / 4;
        stuff = DEBRIS_ROCK;
        break;

    case DNGN_ORANGE_CRYSTAL_STATUE:
        chance = pow / 6;
        stuff = DEBRIS_CRYSTAL;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        chance = 50;
        stuff = DEBRIS_CRYSTAL;
        break;

    default:
        break;
    }

    if (stuff && random2(100) < chance)
    {
        if (!silenced( x, y ))
            noisy( 30, x, y );

        grd[x][y] = DNGN_FLOOR;
        place_debris(x, y, stuff);
        return (1);
    }

    return (0);
}                               // end shatter_walls()

void cast_shatter(int pow)
{
    int damage = 0;
    const bool sil = silenced( you.x_pos, you.y_pos );

    if (!sil)
        noisy( 30, you.x_pos, you.y_pos );

    snprintf(info, INFO_SIZE, "The dungeon %s!", (sil ? "shakes" : "rumbles"));
    mpr(info);

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_NONE:
    case TRAN_SPIDER:
    case TRAN_LICH:
    case TRAN_DRAGON:
    case TRAN_AIR:
    case TRAN_SERPENT_OF_HELL:
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

    apply_area_within_radius(shatter_items, you.x_pos, you.y_pos, pow, rad, 0);
    apply_area_within_radius(shatter_monsters, you.x_pos, you.y_pos, pow, rad, 0);
    int dest = apply_area_within_radius( shatter_walls, you.x_pos, you.y_pos, 
                                         pow, rad, 0 );

    if (dest && !sil)
        mpr("Ka-crash!");
}                               // end cast_shatter()

// cast_forescry: raises evasion (by 8 currently) via divination
void cast_forescry(int pow)
{
    if (!you.duration[DUR_FORESCRY])
        mpr("You begin to receive glimpses of the immediate future...");

    you.duration[DUR_FORESCRY] += 5 + random2(pow);

    if (you.duration[DUR_FORESCRY] > 30)
        you.duration[DUR_FORESCRY] = 30;

    you.redraw_evasion = 1;
}                               // end cast_forescry()

void cast_see_invisible(int pow)
{
    if (player_see_invis())
        mpr("Nothing seems to happen.");
    else
        mpr("Your vision seems to sharpen.");

    // no message if you already are under the spell
    you.duration[DUR_SEE_INVISIBLE] += 10 + random2(2 + (pow / 2));

    if (you.duration[DUR_SEE_INVISIBLE] > 100)
        you.duration[DUR_SEE_INVISIBLE] = 100;
}                               // end cast_see_invisible()

#if 0
// FIXME: This would be kinda cool if implemented right.
//        The idea is that, like detect_secret_doors, the spell gathers all
//        sorts of information about a thing and then tells the caster a few
//        cryptic hints. So for a (+3,+5) Mace of Flaming, one might detect
//        "enchantment and heat", but for a cursed ring of hunger, one might
//        detect "enchantment and ice" (since it gives you a 'deathly cold'
//        feeling when you put it on) or "necromancy" (since it's evil).
//        A weapon of Divine Wrath and a randart that makes you angry might
//        both give similar messages. The key would be to not tell more than
//        hints about whether an item is benign or cursed, but give info
//        on how strong its enchantment is (and therefore how valuable it
//        probably is).
static void cast_detect_magic(int pow)
{
    struct dist bmove;
    int x, y;
    int monster = 0, item = 0, next;    //int max;
    FixedVector < int, NUM_SPELL_TYPES > found;
    int strong = 0;             // int curse = 0;

    for (next = 0; next < NUM_SPELL_TYPES; next++)
    {
        found[next] = 0;
    }

    mpr("Which direction?", MSGCH_PROMPT);
    direction( bmove, DIR_DIR );

    if (!bmove.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    if (bmove.dx == 0 && bmove.dy == 0)
    {
        mpr("You detect a divination in progress.");
        return;
    }

    x = you.x_pos + bmove.dx;
    y = you.y_pos + bmove.dy;

    monster = mgrd[x][y];
    if (monster == NON_MONSTER)
        goto do_items;
    else
        goto all_done;

  do_items:
    item = igrd[x][y];

    if (item == NON_ITEM)
        goto all_done;

    while (item != NON_ITEM)
    {
        next = mitm[item].link;
        if (is_dumpable_artifact
            (mitm[item].base_type, mitm[item].sub_type, mitm[item].plus,
             mitm[item].plus2, mitm[item].special, 0, 0))
        {
            strong++;
            //FIXME: do checks for randart properties
        }
        else
        {
            switch (mitm[item].base_type)
            {
            case OBJ_WEAPONS:
                found[SPTYP_ENCHANTMENT] += (mitm[item].plus > 50);
                found[SPTYP_ENCHANTMENT] += (mitm[item].plus2 > 50);
                break;

            case OBJ_MISSILES:
                found[SPTYP_ENCHANTMENT] += (mitm[item].plus > 50);
                found[SPTYP_ENCHANTMENT] += (mitm[item].plus2 > 50);
                break;

            case OBJ_ARMOUR:
                found[SPTYP_ENCHANTMENT] += mitm[item].plus;
            }
        }
    }

  all_done:
    if (monster)
    {
        mpr("You detect a morphogenic field, such as a monster might have.");
    }
    if (strong)
    {
        mpr("You detect very strong enchantments.");
        return;
    }
    else
    {
        //FIXME:
    }
    return;
}
#endif

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
                grd[x][y] = DNGN_CLOSED_DOOR;
                found++;
            }
        }
    }

    if (found)
    {
        redraw_screen();

        snprintf( info, INFO_SIZE, "You detect %s secret door%s.", 
                 (found > 1) ? "some" : "a", (found > 1) ? "s" : "" );
        mpr( info );
    }
}                               // end cast_detect_secret_doors()

void cast_summon_butterflies(int pow)
{
    // explicitly limiting the number
    int num = 4 + random2(3) + random2( pow ) / 10;
    if (num > 16)
        num = 16;

    for (int scount = 1; scount < num; scount++)
    {
        create_monster( MONS_BUTTERFLY, ENCH_ABJ_III, BEH_FRIENDLY,
                        you.x_pos, you.y_pos, MHITYOU, 250 );
    }
}

void cast_summon_large_mammal(int pow)
{
    int mon;
    int temp_rand = random2(pow);

    if (temp_rand < 10)
        mon = MONS_JACKAL;
    else if (temp_rand < 15)
        mon = MONS_HOUND;
    else
    {
        switch (temp_rand % 7)
        {
        case 0:
            if (you.species == SP_HILL_ORC && one_chance_in(3))
                mon = MONS_WARG;
            else
                mon = MONS_WOLF;
            break;
        case 1:
        case 2:
            mon = MONS_WAR_DOG;
            break;
        case 3:
        case 4:
            mon = MONS_HOUND;
            break;
        default:
            mon = MONS_JACKAL;
            break;
        }
    }

    create_monster( mon, ENCH_ABJ_III, BEH_FRIENDLY, you.x_pos, you.y_pos, 
                    you.pet_target, 250 );
}

void cast_sticks_to_snakes(int pow)
{
    int mon, i, behaviour;

    int how_many = 0;

    int max = 1 + random2( 1 + you.skills[SK_TRANSMIGRATION] ) / 4;

    int dur = ENCH_ABJ_III + random2(pow) / 20;
    if (dur > ENCH_ABJ_V)
        dur = ENCH_ABJ_V;

    const int weapon = you.equip[EQ_WEAPON];

    if (weapon == -1)
    {
        snprintf( info, INFO_SIZE, "Your %s feel slithery!", your_hand(true));
        mpr(info);
        return;
    }

    behaviour = item_cursed( you.inv[ weapon ] ) ? BEH_HOSTILE
                                                 : BEH_FRIENDLY;

    if ((you.inv[ weapon ].base_type == OBJ_MISSILES
         && (you.inv[ weapon ].sub_type == MI_ARROW)))
    {
        if (you.inv[ weapon ].quantity < max)
            max = you.inv[ weapon ].quantity;

        for (i = 0; i <= max; i++)
        {
            //jmf: perhaps also check for poison ammo?
            if (pow > 50 || (pow > 25 && one_chance_in(3)))
                mon = MONS_SNAKE;
            else
                mon = MONS_SMALL_SNAKE;

            if (create_monster( mon, dur, behaviour, you.x_pos, you.y_pos, 
                                MHITYOU, 250 ) != -1)
            {
                how_many++;
            }
        }
    }

    if (you.inv[ weapon ].base_type == OBJ_WEAPONS
        && (you.inv[ weapon ].sub_type == WPN_CLUB
            || you.inv[ weapon ].sub_type == WPN_SPEAR
            || you.inv[ weapon ].sub_type == WPN_QUARTERSTAFF
            || you.inv[ weapon ].sub_type == WPN_SCYTHE
            || you.inv[ weapon ].sub_type == WPN_GIANT_CLUB
            || you.inv[ weapon ].sub_type == WPN_GIANT_SPIKED_CLUB
            || you.inv[ weapon ].sub_type == WPN_BOW
            || you.inv[ weapon ].sub_type == WPN_ANCUS
            || you.inv[ weapon ].sub_type == WPN_HALBERD
            || you.inv[ weapon ].sub_type == WPN_GLAIVE
            || you.inv[ weapon ].sub_type == WPN_BLOWGUN))
    {
        how_many = 1;

        // Upsizing Snakes to Brown Snakes as the base class for using
        // the really big sticks (so bonus applies really only to trolls,
        // ogres, and most importantly ogre magi).  Still it's unlikely
        // any character is strong enough to bother lugging a few of
        // these around.  -- bwr
        if (mass_item( you.inv[ weapon ] ) < 500)
            mon = MONS_SNAKE;
        else
            mon = MONS_BROWN_SNAKE;

        if (pow > 90 && one_chance_in(3))
            mon = MONS_GREY_SNAKE;

        if (pow > 70 && one_chance_in(3))
            mon = MONS_BLACK_SNAKE;

        if (pow > 40 && one_chance_in(3))
            mon = MONS_YELLOW_SNAKE;

        if (pow > 20 && one_chance_in(3))
            mon = MONS_BROWN_SNAKE;

        create_monster(mon, dur, behaviour, you.x_pos, you.y_pos, MHITYOU, 250);
    }

#ifdef USE_DEBRIS_CODE
    if (you.inv[ weapon ].base_type == OBJ_DEBRIS
        && (you.inv[ weapon ].sub_type == DEBRIS_WOOD))
    {
        // this is how you get multiple big snakes
        how_many = 1;
        mpr("FIXME: implement OBJ_DEBRIS conversion! (spells4.cc)");
    }
#endif // USE_DEBRIS_CODE

    if (how_many > you.inv[you.equip[EQ_WEAPON]].quantity)
        how_many = you.inv[you.equip[EQ_WEAPON]].quantity;

    if (how_many)
    {
        dec_inv_item_quantity( you.equip[EQ_WEAPON], how_many );

        snprintf( info, INFO_SIZE, "You create %s snake%s!",
                how_many > 1 ? "some" : "a", how_many > 1 ? "s" : "");
    }
    else
    {
        snprintf( info, INFO_SIZE, "Your %s feel slithery!", your_hand(true));
    }

    mpr(info);
    return;
}                               // end cast_sticks_to_snakes()

void cast_summon_dragon(int pow)
{
    int happy;

    // Removed the chance of multiple dragons... one should be more
    // than enough, and if it isn't, the player can cast again...
    // especially since these aren't on the Abjuration plan... they'll
    // last until they die (maybe that should be changed, but this is
    // a very high level spell so it might be okay).  -- bwr
    happy = (random2(pow) > 5);

    if (create_monster( MONS_DRAGON, ENCH_ABJ_III,
                        (happy ? BEH_FRIENDLY : BEH_HOSTILE),
                        you.x_pos, you.y_pos, MHITYOU, 250 ) != -1)
    {
        strcpy(info, "A dragon appears.");

        if (!happy)
            strcat(info, " It doesn't look very happy.");
    }
    else
        strcpy(info, "Nothing happens.");

    mpr(info);
}                               // end cast_summon_dragon()

void cast_conjure_ball_lightning( int pow )
{
    int num = 3 + random2( 2 + pow / 50 );

    // but restricted so that the situation doesn't get too gross.
    // Each of these will explode for 3d20 damage. -- bwr
    if (num > 8)
        num = 8;

    bool summoned = false;

    for (int i = 0; i < num; i++)
    {
        int tx = -1, ty = -1;

        for (int j = 0; j < 10; j++)
        {
            if (!random_near_space( you.x_pos, you.y_pos, tx, ty, true, true)
                && distance( you.x_pos, you.y_pos, tx, ty ) <= 5)
            {
                break;
            }
        }

        // if we fail, we'll try the ol' summon next to player trick.
        if (tx == -1 || ty == -1)
        {
            tx = you.x_pos;
            ty = you.y_pos;
        }

        int mon = mons_place( MONS_BALL_LIGHTNING, BEH_FRIENDLY, MHITNOT,
                              true, tx, ty );

        // int mon = create_monster( MONS_BALL_LIGHTNING, 0, BEH_FRIENDLY, 
        //                           tx, ty, MHITNOT, 250 );

        if (mon != -1)
        {
            mons_add_ench( &menv[mon], ENCH_SHORT_LIVED );
            summoned = true;
        }
    }

    if (summoned)
        mpr( "You create some ball lightning!" );
    else
        canned_msg( MSG_NOTHING_HAPPENS );
}

static int sleep_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );
    int mnstr = mgrd[x][y];

    if (mnstr == NON_MONSTER)                                   return 0;
    if (mons_holiness( menv[mnstr].type ) != MH_NATURAL)        return 0;
    if (check_mons_resist_magic( &menv[mnstr], pow ))           return 0;

    // Why shouldn't we be able to sleep friendly monsters? -- bwr
    // if (mons_friendly( &menv[mnstr] ))                          return 0;

    //jmf: now that sleep == hibernation:
    if (mons_res_cold( &menv[mnstr] ) > 0 && coinflip())        return 0;
    if (mons_has_ench( &menv[mnstr], ENCH_SLEEP_WARY ))         return 0;

    menv[mnstr].behaviour = BEH_SLEEP;
    mons_add_ench( &menv[mnstr], ENCH_SLEEP_WARY );

    if (mons_flag( menv[mnstr].type, M_COLD_BLOOD ) && coinflip())
        mons_add_ench( &menv[mnstr], ENCH_SLOW );

    return 1;
}                               // end sleep_monsters()

void cast_mass_sleep(int pow)
{
    apply_area_visible(sleep_monsters, pow);
}                               // end cast_mass_sleep()

static int tame_beast_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );
    int which_mons = mgrd[x][y];

    if (which_mons == NON_MONSTER)                             return 0;

    struct monsters *monster = &menv[which_mons];

    if (mons_holiness(monster->type) != MH_NATURAL)            return 0;
    if (mons_intel_type(monster->type) != I_ANIMAL)            return 0;
    if (mons_friendly(monster))                                return 0;

    // 50% bonus for dogs, add cats if they get implemented
    if (monster->type == MONS_HOUND || monster->type == MONS_WAR_DOG
		 || monster->type == MONS_BLACK_BEAR)
    {
        pow += (pow / 2);
    }

    if (you.species == SP_HILL_ORC && monster->type == MONS_WARG)
        pow += (pow / 2);

    if (check_mons_resist_magic(monster, pow))
        return 0;

    // I'd like to make the monsters affected permanently, but that's
    // pretty powerful. Maybe a small (pow/10) chance of being permanently
    // tamed, large chance of just being enslaved.
    simple_monster_message(monster, " is tamed!");

    if (random2(100) < random2(pow / 10))
        monster->attitude = ATT_FRIENDLY;       // permanent, right?
    else
        mons_add_ench(monster, ENCH_CHARM);

    return 1;
}                               // end tame_beast_monsters()

void cast_tame_beasts(int pow)
{
    apply_area_visible(tame_beast_monsters, pow);
}                               // end cast_tame_beasts()

static int ignite_poison_objects(int x, int y, int pow, int garbage)
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

        // FIXME: impliment burning poisoned ammo
        // else if ( it's ammo that's poisoned) {
        //   strength += number_of_ammo;
        //   destroy_item(ammo);
        //  }
        obj = next;
    }

    if (strength > 0)
        place_cloud(CLOUD_FIRE, x, y, strength + roll_dice(3, strength / 4) );

    return (strength);
}                               // end ignite_poison_objects()

static int ignite_poison_clouds( int x, int y, int pow, int garbage )
{
    UNUSED( pow );
    UNUSED( garbage );

    bool did_anything = false;

    const int cloud = env.cgrid[x][y];

    if (cloud != EMPTY_CLOUD)
    {
        if (env.cloud[ cloud ].type == CLOUD_STINK
            || env.cloud[ cloud ].type == CLOUD_STINK_MON)
        {
            did_anything = true;
            env.cloud[ cloud ].type = CLOUD_FIRE;

            env.cloud[ cloud ].decay /= 2;

            if (env.cloud[ cloud ].decay < 1)
                env.cloud[ cloud ].decay = 1;
        }
        else if (env.cloud[ cloud ].type == CLOUD_POISON
                 || env.cloud[ cloud ].type == CLOUD_POISON_MON)
        {
            did_anything = true;
            env.cloud[ cloud ].type = CLOUD_FIRE;
        }
    }

    return ((int) did_anything);
}                               // end ignite_poison_clouds()

static int ignite_poison_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    struct bolt beam;
    beam.flavour = BEAM_FIRE;   // this is dumb, only used for adjust!

    dice_def  dam_dice( 0, 5 + pow / 7 );  // dice added below if applicable

    const int mon_index = mgrd[x][y];
    if (mon_index == NON_MONSTER)
        return (0);

    struct monsters *const mon = &menv[ mon_index ];

    // Monsters which have poison corpses or poisonous attacks:
    if (mons_corpse_thingy( mon->type ) == CE_POISONOUS
        || mon->type == MONS_GIANT_ANT
        || mon->type == MONS_SMALL_SNAKE
        || mon->type == MONS_SNAKE
        || mon->type == MONS_JELLYFISH
        || mons_is_mimic( mon->type ))
    {
        dam_dice.num = 3;
    }

    // Monsters which are poisoned:
    int strength = 0;

    // first check for player poison:
    int ench = mons_has_ench( mon, ENCH_YOUR_POISON_I, ENCH_YOUR_POISON_IV );
    if (ench != ENCH_NONE)
        strength += ench - ENCH_YOUR_POISON_I + 1;

    // ... now monster poison:
    ench = mons_has_ench( mon, ENCH_POISON_I, ENCH_POISON_IV );
    if (ench != ENCH_NONE)
        strength += ench - ENCH_POISON_I + 1;
    
    // strength is now the sum of both poison types (although only 
    // one should actually be present at a given time):
    dam_dice.num += strength;

    int damage = roll_dice( dam_dice );
    if (damage > 0)
    {
        damage = mons_adjust_flavoured( mon, beam, damage );

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "Dice: %dd%d; Damage: %d", 
                  dam_dice.num, dam_dice.size, damage );    

        mpr( info, MSGCH_DIAGNOSTICS );
#endif

        if (!player_hurt_monster( mon_index, damage ))
        {
            // Monster survived, remove any poison.
            mons_del_ench( mon, ENCH_POISON_I, ENCH_POISON_IV );
            mons_del_ench( mon, ENCH_YOUR_POISON_I, ENCH_YOUR_POISON_IV );
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
    char str_pass[ ITEMNAME_SIZE ];

    // temp weapon of venom => temp fire brand
    const int wpn = you.equip[EQ_WEAPON];

    if (wpn != -1 
        && you.duration[DUR_WEAPON_BRAND]
        && get_weapon_brand( you.inv[wpn] ) == SPWPN_VENOM)
    {
        if (set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, SPWPN_FLAMING ))
        {
            in_name( wpn, DESC_CAP_YOUR, str_pass );
            strcpy( info, str_pass );
            strcat( info, " bursts into flame!" );
            mpr(info);

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
            {                   // burn poison ammo
                strength = you.inv[item].quantity;
                acount += you.inv[item].quantity;
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
        mpr("Some ammo you are carrying burns!");

    if (pcount > 0)
    {
        snprintf( info, INFO_SIZE, "%s potion%s you are carrying explode%s!",
            pcount > 1 ? "Some" : "A",
            pcount > 1 ? "s" : "",
            pcount > 1 ? "" : "s");
        mpr(info);
    }

    if (wasWielding == true)
        canned_msg( MSG_EMPTY_HANDED );

    if (totalstrength)
    {
        place_cloud(CLOUD_FIRE, you.x_pos, you.y_pos,
                    random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) +
                    random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) + 1);
    }

    // player is poisonous
    if (you.mutation[MUT_SPIT_POISON] || you.mutation[MUT_STINGER]
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER // poison attack
        || (!player_is_shapechanged()
            && (you.species == SP_GREEN_DRACONIAN       // poison breath
                || you.species == SP_KOBOLD             // poisonous corpse
                || you.species == SP_NAGA)))            // spit poison
    {
        damage = roll_dice( 3, 5 + pow / 7 );
    }

    // player is poisoned
    damage += roll_dice( you.poison, 6 );

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

        if (you.poison > 0)
        {
            mpr( "You feel that the poison has left your system." );
            you.poison = 0;
        }
    }

    apply_area_visible(ignite_poison_clouds, pow);
    apply_area_visible(ignite_poison_objects, pow);
    apply_area_visible(ignite_poison_monsters, pow);
}                               // end cast_ignite_poison()

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.duration[DUR_SILENCE] += 10 + random2avg( pow, 2 );

    if (you.duration[DUR_SILENCE] > 100)
        you.duration[DUR_SILENCE] = 100;
}                               // end cast_silence()


/* ******************************************************************
// no hooks for this anywhere {dlb}:

void cast_animate_golem(int pow)
{
  // must have more than 20 max_hitpoints

  // must be wielding a Scroll of Paper (for chem)

  // must be standing on a pile of <foo> (for foo in: wood, metal, rock, stone)

  // Will cost you 5-10% of max_hitpoints, or 20 + some, whichever is more
  mpr("You imbue the inanimate form with a portion of your life force.");

  naughty(NAUGHTY_CREATED_LIFE, 10);
}

****************************************************************** */

static int discharge_monsters( int x, int y, int pow, int garbage )
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
            strcpy( info, ptr_monam( &(menv[mon]), DESC_CAP_THE ) );
            strcat( info, " is struck by lightning." );
            mpr( info );

            player_hurt_monster( mon, damage );
        }
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(3)) || (pow >= 3 && one_chance_in(10)))
    {
        mpr( "The lightning arcs!" );
        pow /= (coinflip() ? 2 : 3);
        damage += apply_random_around_square( discharge_monsters, x, y, 
                                              true, pow, 1 ); 
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in 
        // cast_discharge() are clean. -- bwr
        mpr( "The lightning grounds out." );
    }

    return (damage);
}                               // end discharge_monsters() 

void cast_discharge( int pow )
{
    int num_targs = 1 + random2( 1 + pow / 25 );
    int dam;

    dam = apply_random_around_square( discharge_monsters, you.x_pos, you.y_pos,
                                      true, pow, num_targs );

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Arcs: %d Damage: %d", num_targs, dam );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (dam == 0) 
    {
        if (coinflip())
            mpr("The air around you crackles with electrical energy.");
        else 
        {
            bool plural = coinflip();
            snprintf( info, INFO_SIZE, "%s blue arc%s ground%s harmlessly %s you.",
                plural ? "Some" : "A",
                plural ? "s" : "",
                plural ? " themselves" : "s itself",
                plural ? "around" : (coinflip() ? "beside" :
                                     coinflip() ? "behind" : "before")
                );

            mpr(info);
        }
    }
}                               // end cast_discharge()

// NB: this must be checked against the same effects
// in fight.cc for all forms of attack !!! {dlb}
// This function should be currently unused (the effect is too powerful)
static int distortion_monsters(int x, int y, int pow, int message)
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
                            "a distortion effect" );
        }
        else
        {
            miscast_effect( SPTYP_TRANSLOCATION, 1, 1, 100, 
                            "a distortion effect" );
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
        strcpy(info, "Space bends around ");
        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
        strcat(info, ".");
        mpr(info);
        specdam += 1 + random2avg( 7, 2 ) + random2(pow) / 40;
    }
    else if (coinflip())
    {
        strcpy(info, "Space warps horribly around ");
        strcat(info, ptr_monam( defender, DESC_NOCAP_THE ));
        strcat(info, "!");
        mpr(info);

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
        monster_die(defender, KILL_RESET, 0);
        return 1;
    }
    else if (message)
    {
        mpr("Nothing seems to happen.");
        return 1;
    }

    player_hurt_monster(monster_attacked, specdam);

    return (specdam);
}                               // end distortion_monsters()

void cast_bend(int pow)
{
    apply_one_neighbouring_square( distortion_monsters, pow );
}                               // end cast_bend()

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
        mpr( "There is a brief shimmering in the air around you." );
    }
}

static int spell_swap_func(int x, int y, int pow, int message)
{
    UNUSED( message );

    int monster_attacked = mgrd[x][y];

    if (monster_attacked == NON_MONSTER)
        return 0;

    struct monsters *defender = &menv[monster_attacked];

    if (defender->type == MONS_BLINK_FROG
        || check_mons_resist_magic( defender, pow ))
    {
        simple_monster_message( defender, " resists." );
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
        {
            you.x_pos = old_x;
            you.y_pos = old_y;
        }
    }

    return 1;
}

void cast_swap(int pow)
{
    apply_one_neighbouring_square( spell_swap_func, pow );
}

static int make_a_rot_cloud(int x, int y, int pow, int ctype)
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
            {
                mitm[obj].sub_type = CORPSE_SKELETON;
                mitm[obj].special = 200;
                mitm[obj].colour = LIGHTGREY;
            }

            place_cloud(ctype, x, y,
                        (3 + random2(pow / 4) + random2(pow / 4) +
                         random2(pow / 4)));
            return 1;
        }

        obj = next;
    }

    return 0;
}                               // end make_a_rot_cloud()

int make_a_normal_cloud(int x, int y, int pow, int ctype)
{
    place_cloud( ctype, x, y, 
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)) );

    return 1;
}                               // end make_a_normal_cloud()

#if 0

static int make_a_random_cloud(int x, int y, int pow, int ctype)
{
    if (ctype == CLOUD_NONE)
        ctype = CLOUD_BLACK_SMOKE;

    unsigned char cloud_material;

    switch (random2(9))
    {
    case 0:
        cloud_material = CLOUD_FIRE;
        break;
    case 1:
        cloud_material = CLOUD_STINK;
        break;
    case 2:
        cloud_material = CLOUD_COLD;
        break;
    case 3:
        cloud_material = CLOUD_POISON;
        break;
    case 4:
        cloud_material = CLOUD_BLUE_SMOKE;
        break;
    case 5:
        cloud_material = CLOUD_STEAM;
        break;
    case 6:
        cloud_material = CLOUD_PURP_SMOKE;
        break;
    default:
        cloud_material = ctype;
        break;
    }

    // that last bit is equivalent to "random2(pow/4) + random2(pow/4)
    // + random2(pow/4)" {dlb}
    // can you see the pattern? {dlb}
    place_cloud(cloud_material, x, y, 3 + random2avg(3 * (pow / 4) - 2, 3));

    return 1;
}                               // end make_a_random_cloud()

#endif

static int passwall(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    char dx, dy, nx = x, ny = y;
    int howdeep = 0;
    bool done = false;
    int shallow = 1 + (you.skills[SK_EARTH_MAGIC] / 8);

    // allow statues as entry points?
    if (grd[x][y] != DNGN_ROCK_WALL)
        // Irony: you can start on a secret door but not a door.
        // Worked stone walls are out, they're not diggable and
        // are used for impassable walls... I'm not sure we should
        // even allow statues (should be contiguous rock) -- bwr
    {
        mpr("That's not a passable wall.");
        return 0;
    }

    dx = x - you.x_pos;
    dy = y - you.y_pos;

    while (!done)
    {
        // I'm trying to figure proper borders out {dlb}
        // FIXME: dungeon border?
        if (nx > (GXM - 1) || ny > (GYM - 1) || nx < 2 || ny < 2)
        {
            mpr("You sense an overwhelming volume of rock.");
            return 0;
        }

        switch (grd[nx][ny])
        {
        default:
            done = true;
            break;
        case DNGN_ROCK_WALL:
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

    if (howdeep > shallow)
    {
        mpr("This rock feels deep.");

        if (yesno("Try anyway?"))
        {
            if (howdeep > range)
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
    apply_one_neighbouring_square(passwall, pow);
}                               // end cast_passwall()

static int intoxicate_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)
        return 0;
    if (mons_intel(menv[mon].type) < I_NORMAL)
        return 0;
    if (mons_holiness(menv[mon].type) != MH_NATURAL)
        return 0;
    if (mons_res_poison(&menv[mon]) > 0)
        return 0;

    mons_add_ench(&menv[mon], ENCH_CONFUSION);
    return 1;
}                               // end intoxicate_monsters()

void cast_intoxicate(int pow)
{
    potion_effect( POT_CONFUSION, 10 + (100 - pow) / 10);

    if (one_chance_in(20) && lose_stat( STAT_INTELLIGENCE, 1 + random2(3) ))
        mpr("Your head spins!");

    apply_area_visible(intoxicate_monsters, pow);
}                               // end cast_intoxicate()

// intended as a high-level Elven (a)bility
static int glamour_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    int mon = mgrd[x][y];

    // Power in this function is already limited by a function of
    // experience level (10 + level / 2) since it's only an ability,
    // never an actual spell. -- bwr

    if (mon == NON_MONSTER)
        return (0);

    if (one_chance_in(5))
        return (0);

    if (mons_intel(menv[mon].type) < I_NORMAL)
        return (0);

    if (mons_holiness(mon) != MH_NATURAL)
        return (0);

    if (!mons_is_humanoid( menv[mon].type ))
        return (0);

    const char show_char = mons_char( menv[mon].type );

    // gargoyles are immune.
    if (menv[mon].type == MONS_GARGOYLE
        || menv[mon].type == MONS_METAL_GARGOYLE
        || menv[mon].type == MONS_MOLTEN_GARGOYLE)
    {
        return (0);
    }

    // orcs resist thru hatred of elves
    // elves resist cause they're elves
    // boggarts are malevolent highly magical wee-folk
    if (show_char == 'o' || show_char == 'e' || menv[mon].type == MONS_BOGGART)
        pow = (pow / 2) + 1;

    if (check_mons_resist_magic(&menv[mon], pow))
        return (0);

    switch (random2(6))
    {
    case 0:
        mons_add_ench(&menv[mon], ENCH_FEAR);
        break;
    case 1:
    case 4:
        mons_add_ench(&menv[mon], ENCH_CONFUSION);
        break;
    case 2:
    case 5:
        mons_add_ench(&menv[mon], ENCH_CHARM);
        break;
    case 3:
        menv[mon].behaviour = BEH_SLEEP;
        break;
    }

    // why no, there's no message as to which effect happened >:^)
    if (!one_chance_in(4))
    {
        strcpy(info, ptr_monam( &(menv[mon]), DESC_CAP_THE));

        switch (random2(4))
        {
        case 0:
            strcat(info, " looks dazed.");
            break;
        case 1:
            strcat(info, " blinks several times.");
            break;
        case 2:
            strcat(info, " rubs its eye");
            if (menv[mon].type != MONS_CYCLOPS)
                strcat(info, "s");
            strcat(info, ".");
            break;
        case 4:
            strcat(info, " tilts its head.");
            break;
        }

        mpr(info);
    }

    return (1);
}                               // end glamour_monsters()

void cast_glamour(int pow)
{
    apply_area_visible(glamour_monsters, pow);
}                               // end cast_glamour()

bool backlight_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)
        return (false);

    switch (menv[mon].type)
    {
    //case MONS_INSUBSTANTIAL_WISP: //jmf: I'm not sure if these glow or not
    //case MONS_VAPOUR:
    case MONS_UNSEEN_HORROR:    // consider making this visible? probably not.
        return (false);

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
        return (false);               // already glowing or invisible
    default:
        break;
    }

    int lvl = mons_has_ench( &menv[mon], ENCH_BACKLIGHT_I, ENCH_BACKLIGHT_IV );

    if (lvl == ENCH_NONE)
        simple_monster_message( &menv[mon], " is outlined in light." );
    else if (lvl == ENCH_BACKLIGHT_IV)
        simple_monster_message( &menv[mon], " glows brighter for a moment." );
    else 
    {
        // remove old level
        mons_del_ench( &menv[mon], ENCH_BACKLIGHT_I, ENCH_BACKLIGHT_III, true );
        simple_monster_message( &menv[mon], " glows brighter." );
    }

    // this enchantment wipes out invisibility (neat)
    mons_del_ench( &menv[mon], ENCH_INVIS );
    mons_add_ench( &menv[mon], ENCH_BACKLIGHT_IV );

    return (true);
}                               // end backlight_monsters()

void cast_evaporate(int pow)
{
    // experimenting with allowing the potion to be thrown... we're
    // still making it have to be "in hands" at this point. -- bwr
    struct dist spelld;
    struct bolt beem;

    const int potion = prompt_invent_item( "Throw which potion?", OBJ_POTIONS );

    if (potion == -1)
    {
        snprintf( info, INFO_SIZE, "Wisps of steam play over your %s!", 
                  your_hand(true) );

        mpr(info);
        return;
    } 
    else if (you.inv[potion].base_type != OBJ_POTIONS)
    {
        mpr( "This spell works only on potions!" );
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    mpr( STD_DIRECTION_PROMPT, MSGCH_PROMPT );

    message_current_target();

    direction( spelld, DIR_NONE, TARG_ENEMY );

    if (!spelld.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    beem.target_x = spelld.tx;
    beem.target_y = spelld.ty;

    beem.source_x = you.x_pos;
    beem.source_y = you.y_pos;

    strcpy( beem.beam_name, "potion" );
    beem.colour = you.inv[potion].colour;
    beem.range = 9;
    beem.rangeMax = 9;
    beem.type = SYM_FLASK;
    beem.beam_source = MHITYOU;
    beem.thrower = KILL_YOU_MISSILE;
    beem.aux_source = NULL;
    beem.isBeam = false;
    beem.isTracer = false;

    beem.hit = you.dex / 2 + roll_dice( 2, you.skills[SK_THROWING] / 2 + 1 );
    beem.damage = dice_def( 1, 0 );  // no damage, just producing clouds
    beem.ench_power = pow;           // used for duration only?

    beem.flavour = BEAM_POTION_STINKING_CLOUD;

    switch (you.inv[potion].sub_type)
    {
    case POT_STRONG_POISON:
        beem.flavour = BEAM_POTION_POISON;
        beem.ench_power *= 2;
        break;

    case POT_DEGENERATION:
        beem.flavour = (coinflip() ? BEAM_POTION_POISON : BEAM_POTION_MIASMA);
        beem.ench_power *= 2;
        break;
            
    case POT_POISON:
        beem.flavour = BEAM_POTION_POISON;
        break;

    case POT_DECAY:
        beem.flavour = BEAM_POTION_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_PARALYSIS:
        beem.ench_power *= 2;
        // fall through
    case POT_CONFUSION:
    case POT_SLOWING:
        beem.flavour = BEAM_POTION_STINKING_CLOUD;
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        beem.flavour = BEAM_POTION_STEAM;
        break;

    case POT_BERSERK_RAGE:
        beem.flavour = (coinflip() ? BEAM_POTION_FIRE : BEAM_POTION_STEAM);
        break;

    case POT_MUTATION:
    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        switch (random2(5))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;            break;
        case 1:   beem.flavour = BEAM_POTION_COLD;            break;
        case 2:   beem.flavour = BEAM_POTION_POISON;          break;
        case 3:   beem.flavour = BEAM_POTION_MIASMA;          break;
        default:  beem.flavour = BEAM_POTION_RANDOM;          break;
        }
        break;

    default:
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
        break;
    }

    if (coinflip())
        exercise( SK_THROWING, 1 );

    fire_beam(beem);

    // both old and new code use up a potion:
    dec_inv_item_quantity( potion, 1 );

    return;
}                               // end cast_evaporate()

// The intent of this spell isn't to produce helpful potions 
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.  
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not 
// using up the corpse might also lead to game balance problems. -- bwr
void cast_fulsome_distillation( int powc )
{
    char str_pass[ ITEMNAME_SIZE ];

    if (powc > 50)
        powc = 50;

    int corpse = -1;

    // Search items at the players location for corpses.
    // XXX: Turn this into a separate function and merge with 
    // the messes over in butchery, animating, and maybe even 
    // item pickup from stacks (which would make it easier to 
    // create a floor stack menu system later) -- bwr
    for (int curr_item = igrd[you.x_pos][you.y_pos]; 
             curr_item != NON_ITEM; 
             curr_item = mitm[curr_item].link) 
    {
        if (mitm[curr_item].base_type == OBJ_CORPSES
            && mitm[curr_item].sub_type == CORPSE_BODY)
        {
            it_name( curr_item, DESC_NOCAP_THE, str_pass );
            snprintf( info, INFO_SIZE, "Distill a potion from %s?", str_pass );

            if (yesno( info, true, false ))
            {
                corpse = curr_item;
                break;
            }
        }
    }

    if (corpse == -1)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    const bool rotten = (mitm[corpse].special < 100);
    const bool big_monster = (mons_type_hit_dice( mitm[corpse].plus ) >= 5);
    const bool power_up = (rotten && big_monster);

    int potion_type = POT_WATER;

    switch (mitm[corpse].plus)
    {
    case MONS_GIANT_BAT:             // extracting batty behaviour : 1
    case MONS_UNSEEN_HORROR:         // extracting batty behaviour : 7
    case MONS_GIANT_BLOWFLY:         // extracting batty behaviour : 5
        potion_type = POT_CONFUSION;
        break;

    case MONS_RED_WASP:              // paralysis attack : 8
    case MONS_YELLOW_WASP:           // paralysis attack : 4
        potion_type = POT_PARALYSIS;
        break;

    case MONS_SNAKE:                 // clean meat, but poisonous attack : 2
    case MONS_GIANT_ANT:             // clean meat, but poisonous attack : 3
        potion_type = (power_up ? POT_POISON : POT_CONFUSION);
        break;

    case MONS_ORANGE_RAT:            // poisonous meat, but draining attack : 3
        potion_type = (power_up ? POT_DECAY : POT_POISON);
        break;

    case MONS_SPINY_WORM:            // 12
        potion_type = (power_up ? POT_DECAY : POT_STRONG_POISON);
        break;

    default:
        switch (mons_corpse_thingy( mitm[corpse].plus ))
        {
        case CE_CLEAN:
            potion_type = (power_up ? POT_CONFUSION : POT_WATER);
            break;

        case CE_CONTAMINATED:
            potion_type = (power_up ? POT_DEGENERATION : POT_POISON);
            break;

        case CE_POISONOUS:
            potion_type = (power_up ? POT_STRONG_POISON : POT_POISON);
            break;

        case CE_MUTAGEN_RANDOM: 
        case CE_MUTAGEN_GOOD:   // unused
        case CE_RANDOM:         // unused
            potion_type = POT_MUTATION;
            break;

        case CE_MUTAGEN_BAD:    // unused
        case CE_ROTTEN:         // actually this only occurs via mangling 
        case CE_HCL:            // necrophage
            potion_type = (power_up ? POT_DECAY : POT_STRONG_POISON);
            break;

        case CE_NOCORPSE:       // shouldn't occur
        default:
            break;
        }
        break;
    }

    // If not powerful enough, we downgrade the potion
    if (random2(50) > powc + 10 * rotten)
    {
        switch (potion_type)
        {
        case POT_DECAY: 
        case POT_DEGENERATION: 
        case POT_STRONG_POISON: 
            potion_type = POT_POISON;
            break;

        case POT_MUTATION: 
        case POT_POISON: 
            potion_type = POT_CONFUSION;
            break;

        case POT_PARALYSIS: 
            potion_type = POT_SLOWING;
            break;

        case POT_CONFUSION: 
        case POT_SLOWING: 
        default:
            potion_type = POT_WATER;
            break;
        }
    }

    // We borrow the corpse's object to make our potion:
    mitm[corpse].base_type = OBJ_POTIONS;
    mitm[corpse].sub_type = potion_type;
    mitm[corpse].quantity = 1;
    mitm[corpse].plus = 0;
    mitm[corpse].plus2 = 0;
    item_colour( mitm[corpse] );  // sets special as well

    it_name( corpse, DESC_NOCAP_A, str_pass );
    snprintf( info, INFO_SIZE, "You extract %s from the corpse.",
              str_pass );
    mpr( info );

    // try to move the potion to the player (for convenience)
    if (move_item_to_player( corpse, 1 ) != 1)
    {
        mpr( "Unfortunately, you can't carry it right now!" );
    }
}

void make_shuggoth(int x, int y, int hp)
{
    int mon = create_monster( MONS_SHUGGOTH, 100 + random2avg(58, 3),
                              BEH_HOSTILE, x, y, MHITNOT, 250 );

    if (mon != -1)
    {
        menv[mon].hit_points = hp;
        menv[mon].max_hit_points = hp;
    }

    return;
}                               // end make_shuggoth()

static int rot_living(int x, int y, int pow, int message)
{
    UNUSED( message );

    int mon = mgrd[x][y];
    int ench;

    if (mon == NON_MONSTER)
        return 0;

    if (mons_holiness(menv[mon].type) != MH_NATURAL)
        return 0;

    if (check_mons_resist_magic(&menv[mon], pow))
        return 0;

    ench = ((random2(pow) + random2(pow) + random2(pow) + random2(pow)) / 4);

    if (ench >= 50)
        ench = ENCH_YOUR_ROT_IV;
    else if (ench >= 35)
        ench = ENCH_YOUR_ROT_III;
    else if (ench >= 20)
        ench = ENCH_YOUR_ROT_II;
    else
        ench = ENCH_YOUR_ROT_I;

    mons_add_ench(&menv[mon], ench);

    return 1;
}                               // end rot_living()

static int rot_undead(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    int mon = mgrd[x][y];
    int ench;

    if (mon == NON_MONSTER)
        return 0;

    if (mons_holiness(menv[mon].type) != MH_UNDEAD)
        return 0;

    if (check_mons_resist_magic(&menv[mon], pow))
        return 0;

    // this does not make sense -- player mummies are
    // immune to rotting (or have been) -- so what is
    // the schema in use here to determine rotting??? {dlb}

    //jmf: up for discussion. it is clearly unfair to
    //     rot player mummies.
    //     the `shcema' here is: corporeal non-player undead
    //     rot, discorporeal undead don't rot. if you wanna
    //     insist that monsters get the same treatment as
    //     players, I demand my player mummies get to worship
    //     the evil mummy & orc god.
    switch (menv[mon].type)
    {
    case MONS_NECROPHAGE:
    case MONS_ZOMBIE_SMALL:
    case MONS_LICH:
    case MONS_MUMMY:
    case MONS_VAMPIRE:
    case MONS_ZOMBIE_LARGE:
    case MONS_WIGHT:
    case MONS_GHOUL:
    case MONS_BORIS:
    case MONS_ANCIENT_LICH:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAMPIRE_MAGE:
    case MONS_GUARDIAN_MUMMY:
    case MONS_GREATER_MUMMY:
    case MONS_MUMMY_PRIEST:
        break;
    case MONS_ROTTING_HULK:
    default:
        return 0;               // immune (no flesh) or already rotting
    }

    ench = ((random2(pow) + random2(pow) + random2(pow) + random2(pow)) / 4);

    if (ench >= 50)
        ench = ENCH_YOUR_ROT_IV;
    else if (ench >= 35)
        ench = ENCH_YOUR_ROT_III;
    else if (ench >= 20)
        ench = ENCH_YOUR_ROT_II;
    else
        ench = ENCH_YOUR_ROT_I;

    mons_add_ench(&menv[mon], ench);

    return 1;
}                               // end rot_undead()

static int rot_corpses(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    return make_a_rot_cloud(x, y, pow, CLOUD_MIASMA);
}                               // end rot_corpses()

void cast_rotting(int pow)
{
    apply_area_visible(rot_living, pow);
    apply_area_visible(rot_undead, pow);
    apply_area_visible(rot_corpses, pow);
    return;
}                               // end cast_rotting()

void do_monster_rot(int mon)
{
    int damage = 1 + random2(3);

    if (mons_holiness(menv[mon].type) == MH_UNDEAD && random2(5))
    {
        apply_area_cloud(make_a_normal_cloud, menv[mon].x, menv[mon].y,
                         10, 1, CLOUD_MIASMA);
    }

    player_hurt_monster( mon, damage );
    return;
}                               // end do_monster_rot()

static int snake_charm_monsters(int x, int y, int pow, int message)
{
    UNUSED( message );

    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)                             return 0;
    if (mons_friendly(&menv[mon]))                      return 0;
    if (one_chance_in(4))                               return 0;
    if (mons_char(menv[mon].type) != 'S')               return 0;
    if (check_mons_resist_magic(&menv[mon], pow))       return 0;

    menv[mon].attitude = ATT_FRIENDLY;
    snprintf( info, INFO_SIZE, "%s sways back and forth.", ptr_monam( &(menv[mon]), DESC_CAP_THE ));
    mpr(info);

    return 1;
}

void cast_snake_charm(int pow)
{
    // powc = (you.experience_level * 2) + (you.skills[SK_INVOCATIONS] * 3);
    apply_one_neighbouring_square(snake_charm_monsters, pow);
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

    mpr("Fragment what (e.g. a wall)?", MSGCH_PROMPT);
    direction( beam, DIR_TARGET, TARG_ENEMY );

    if (!beam.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    //FIXME: if (player typed '>' to attack floor) goto do_terrain;
    blast.beam_source = MHITYOU;
    blast.thrower = KILL_YOU;
    blast.aux_source = NULL;
    blast.ex_size = 1;              // default
    blast.type = '#';
    blast.colour = 0;
    blast.target_x = beam.tx;
    blast.target_y = beam.ty;
    blast.isTracer = false;
    blast.flavour = BEAM_FRAG;

    // Number of dice vary... 3 is easy/common, but it can get as high as 6.
    blast.damage = dice_def( 0, 5 + pow / 10 ); 

    const int grid = grd[beam.tx][beam.ty];
    const int mon  = mgrd[beam.tx][beam.ty];

    const bool okay_to_dest = ((beam.tx > 5 && beam.tx < GXM - 5)
                                && (beam.ty > 5 && beam.ty < GYM - 5));

    if (mon != NON_MONSTER)
    {
        // This needs its own hand_buff... we also need to do it first
        // in case the target dies. -- bwr
        char explode_msg[80];

        snprintf( explode_msg, sizeof( explode_msg ), "%s explodes!",
                  ptr_monam( &(menv[mon]), DESC_CAP_THE ) );

        switch (menv[mon].type)
        {
        case MONS_ICE_BEAST: // blast of ice fragments
        case MONS_SIMULACRUM_SMALL:
        case MONS_SIMULACRUM_LARGE:
            explode = true;
            strcpy(blast.beam_name, "icy blast");
            blast.colour = WHITE;
            blast.damage.num = 2;
            blast.flavour = BEAM_ICE;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 1;
            break;

        case MONS_FLYING_SKULL:
        case MONS_SKELETON_SMALL:
        case MONS_SKELETON_LARGE:       // blast of bone
            explode = true;

            snprintf( info, INFO_SIZE, "The sk%s explodes into sharp fragments of bone!",
                    (menv[mon].type == MONS_FLYING_SKULL) ? "ull" : "eleton");

            strcpy(blast.beam_name, "blast of bone shards");

            blast.colour = LIGHTGREY;

            if (random2(50) < (pow / 5))        // potential insta-kill
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
            explode = true;
            strcpy( blast.beam_name, "blast of metal fragments" );
            blast.colour = CYAN;
            blast.damage.num = 4;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 2;
            break;

        case MONS_CLAY_GOLEM:   // assume baked clay and not wet loam
        case MONS_STONE_GOLEM:
        case MONS_EARTH_ELEMENTAL:
        case MONS_GARGOYLE:
            explode = true;
            blast.ex_size = 2;
            strcpy(blast.beam_name, "blast of rock fragments");
            blast.colour = BROWN;
            blast.damage.num = 3;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 1;
            break;

        case MONS_CRYSTAL_GOLEM:
            explode = true;
            blast.ex_size = 2;
            strcpy(blast.beam_name, "blast of crystal shards");
            blast.colour = WHITE;
            blast.damage.num = 4;
            if (player_hurt_monster(mon, roll_dice( blast.damage )))
                blast.damage.num += 2;
            break;

        default:
            blast.damage.num = 1;  // to mark that a monster was targetted

            // Yes, this spell does lousy damage if the 
            // monster isn't susceptable. -- bwr 
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
    case DNGN_SECRET_DOOR:
        blast.colour = env.rock_colour;
        // fall-through
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

        strcpy(blast.beam_name, "blast of rock fragments");
        blast.damage.num = 3;
        if (blast.colour == 0)
            blast.colour = LIGHTGREY;

        if (okay_to_dest
            && (grid == DNGN_ORCISH_IDOL 
                || grid == DNGN_GRANITE_STATUE
                || (pow >= 40 && grid == DNGN_ROCK_WALL && one_chance_in(3))
                || (pow >= 60 && grid == DNGN_STONE_WALL && one_chance_in(10))))
        {
            // terrain blew up real good:
            blast.ex_size = 2;
            grd[beam.tx][beam.ty] = DNGN_FLOOR;
            debris = DEBRIS_ROCK;
        }
        break;

    //
    // Metal -- small but nasty explosion
    //

    case DNGN_METAL_WALL:       
        what = "metal wall";
        blast.colour = CYAN;
        // fallthru
    case DNGN_SILVER_STATUE:
        if (what == NULL)
        {
            what = "silver statue";
            blast.colour = WHITE;
        }

        explode = true;
        strcpy( blast.beam_name, "blast of metal fragments" );
        blast.damage.num = 4;

        if (okay_to_dest && pow >= 80 && random2(500) < pow / 5)
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
        what = "crystal wall";
        blast.colour = GREEN;
        // fallthru
    case DNGN_ORANGE_CRYSTAL_STATUE:
        if (what == NULL)
        {
            what = "crystal statue";
            blast.colour = LIGHTRED; //jmf: == orange, right?
        }

        explode = true;
        blast.ex_size = 2;
        strcpy(blast.beam_name, "blast of crystal shards");
        blast.damage.num = 5;

        if (okay_to_dest
            && ((grid == DNGN_GREEN_CRYSTAL_WALL && coinflip())
                || (grid == DNGN_ORANGE_CRYSTAL_STATUE 
                    && pow >= 50 && one_chance_in(10))))
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
            // non-mechanical traps don't explode with this spell -- bwr 
            break;
        }

        // undiscovered traps appear as exploding from the floor -- bwr
        what = ((grid == DNGN_UNDISCOVERED_TRAP) ? "floor" : "trap");

        explode = true;
        hole = false;           // to hit monsters standing on traps
        strcpy( blast.beam_name, "blast of fragments" );
        blast.colour = env.floor_colour;  // in order to blend in
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
        // Doors always blow up, stone arches never do (would cause problems)
        if (okay_to_dest)
            grd[beam.tx][beam.ty] = DNGN_FLOOR;

        // fall-through
    case DNGN_STONE_ARCH:       // floor -- small explosion
        explode = true;
        hole = false;           // to hit monsters standing on doors
        strcpy( blast.beam_name, "blast of rock fragments" );
        blast.colour = LIGHTGREY;
        blast.damage.num = 2;
        break;

    //
    // Permarock and floor are unaffected -- bwr
    //
    case DNGN_PERMAROCK_WALL:
    case DNGN_FLOOR:
        explode = false;
        snprintf( info, INFO_SIZE, "%s seems to be unnaturally hard.",
                  (grid == DNGN_PERMAROCK_WALL) ? "That wall" 
                                                : "The dungeon floor" );
        explode = false;
        break;

    case DNGN_TRAP_III: // What are these? Should they explode? -- bwr
    default:
        // FIXME: cute message for water?
        break;
    }

  all_done:
    if (explode && blast.damage.num > 0)
    {
        if (what != NULL)
        {
            snprintf( info, INFO_SIZE, "The %s explodes!", what);
            mpr(info);
        }

        explosion( blast, hole );
    }
    else if (blast.damage.num == 0)
    {
        // if damage dice are zero we assume that nothing happened at all.
        canned_msg(MSG_SPELL_FIZZLES);
    }

    if (debris)
        place_debris(beam.tx, beam.ty, debris);
}                               // end cast_fragmentation()

void cast_twist(int pow)
{
    struct dist targ;
    struct bolt tmp;    // used, but ignored

    // level one power cap -- bwr
    if (pow > 25)
        pow = 25;

    // Get target,  using DIR_TARGET for targetting only,
    // since we don't use fire_beam() for this spell.
    if (spell_direction(targ, tmp, DIR_TARGET) == -1)
        return;

    const int mons = mgrd[ targ.tx ][ targ.ty ];

    // anything there?
    if (mons == NON_MONSTER || targ.isMe)
    {
        mpr("There is no monster there!");
        return;
    }

    // Monster can magically save vs attack.
    if (check_mons_resist_magic( &menv[ mons ], pow * 2 ))
    {
        simple_monster_message( &menv[ mons ], " resists." );
        return;
    }

    // Roll the damage... this spell is pretty low on damage, because
    // it can target any monster in LOS (high utility).  This is
    // similar to the damage done by Magic Dart (although, the
    // distribution is much more uniform). -- bwr
    int damage = 1 + random2( 3 + pow / 5 );

    // Inflict the damage
    player_hurt_monster( mons, damage );
    return;
}                               // end cast_twist()

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
    struct dist targ;
    struct bolt tmp;    // used, but ignored

    // Get target,  using DIR_TARGET for targetting only,
    // since we don't use fire_beam() for this spell.
    if (spell_direction(targ, tmp, DIR_TARGET) == -1)
        return;

    // Get the target monster...
    if (mgrd[targ.tx][targ.ty] == NON_MONSTER
        || targ.isMe)
    {
        mpr("There is no monster there!");
        return;
    }

    //  Start with weapon base damage...
    const int weapon = you.equip[ EQ_WEAPON ];

    int damage = 3;     // default unarmed damage
    int speed  = 10;    // default unarmed time

    if (weapon != -1)   // if not unarmed
    {
        // look up the damage base
        if (you.inv[ weapon ].base_type == OBJ_WEAPONS)
        {
            damage = property( you.inv[ weapon ], PWPN_DAMAGE );
            speed = property( you.inv[ weapon ], PWPN_SPEED );

            if (get_weapon_brand( you.inv[ weapon ] ) == SPWPN_SPEED)
            {
                speed *= 5;
                speed /= 10;
            }
        }
        else if (item_is_staff( you.inv[ weapon ] ))
        {
            damage = property( you.inv[ weapon ], PWPN_DAMAGE );
            speed = property( you.inv[ weapon ], PWPN_SPEED );
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

    struct monsters *monster = &menv[ mgrd[targ.tx][targ.ty] ];

    // apply monster's AC
    if (monster->armour_class > 0)
        damage -= random2( 1 + monster->armour_class );

#if 0
    // Removing damage limiter since it's categorized at level 4 right now.

    // Force transmitted is limited by skill...
    const int limit = (you.skills[SK_TRANSLOCATIONS] + 1) / 2 + 3;
    if (damage > limit)
        damage = limit;
#endif

    // Roll the damage...
    damage = 1 + random2( damage );

    // Monster can magically save vs attack (this could be replaced or
    // augmented with an EV check).
    if (check_mons_resist_magic( monster, pow * 2 ))
    {
        simple_monster_message( monster, " resists." );
        return;
    }

    // Inflict the damage
    hurt_monster( monster, damage );
    if (monster->hit_points < 1)
        monster_die( monster, KILL_YOU, 0 );
    else
        print_wounds( monster );

    return;
}                               // end cast_far_strike()

void cast_apportation(int pow)
{
    struct dist beam;

    mpr("Pull items from where?");

    direction( beam, DIR_TARGET );

    if (!beam.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return;
    }

    // it's already here!
    if (beam.isMe)
    {
        mpr( "That's just silly." );
        return;
    }

    // Protect the player from destroying the item
    const int grid = grd[ you.x_pos ][ you.y_pos ];

    if (grid == DNGN_LAVA || grid == DNGN_DEEP_WATER)
    {
        mpr( "That would be silly while over this terrain!" );
        return;
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
        return;
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
            snprintf( info, INFO_SIZE, "%s twitches.",
                      ptr_monam( &(menv[ mon ]), DESC_CAP_THE ) );
            mpr( info );
        }
        else 
            mpr( "This spell does not work on creatures." );

        return;
    }

    // mass of one unit
    const int unit_mass = mass_item( mitm[ item ] );
    // assume we can pull everything
    int max_units = mitm[ item ].quantity;

    // item has mass: might not move all of them
    if (unit_mass > 0)
    {
        const int max_mass = pow * 30 + random2( pow * 20 );

        // most units our power level will allow
        max_units = max_mass / unit_mass;
    }

    if (max_units <= 0)
    {
        mpr( "The mass is resisting your pull." );
        return;
    }

    // Failure should never really happen after all the above checking,
    // but we'll handle it anyways...
    if (move_top_item( beam.tx, beam.ty, you.x_pos, you.y_pos ))
    {
        if (max_units < mitm[ item ].quantity)
        {
            mitm[ item ].quantity = max_units;
            mpr( "You feel that some mass got lost in the cosmic void." );
        }
        else
        {
            mpr( "Yoink!" );
            snprintf( info, INFO_SIZE, "You pull the item%s to yourself.",
                                 (mitm[ item ].quantity > 1) ? "s" : "" );
            mpr( info );
        }
    }
    else
        mpr( "The spell fails." );
}

void cast_sandblast(int pow)
{
    bool big = true;
    struct dist spd;
    struct bolt beam;

    // this type of power manipulation should be done with the others,
    // currently over in it_use2.cc (ack) -- bwr
    // int hurt = 2 + random2(5) + random2(4) + random2(pow) / 20;

    big = false;

    if (you.equip[EQ_WEAPON] != -1)
    {
        int wep = you.equip[EQ_WEAPON];
        if (you.inv[wep].base_type == OBJ_MISSILES
           && (you.inv[wep].sub_type == MI_STONE || you.inv[wep].sub_type == MI_LARGE_ROCK))
           big = true;
    }

    if (spell_direction(spd, beam) == -1)
        return;

    if (spd.isMe)
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return;
    }

    if (big)
    {
        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
        zapping(ZAP_SANDBLAST, pow, beam);
    }
    else
    {
        zapping(ZAP_SMALL_SANDBLAST, pow, beam);
    }
}                               // end cast_sandblast()

static bool mons_can_host_shuggoth(int type)    //jmf: simplified
{
    if (mons_holiness(type) != MH_NATURAL)
        return false;
    if (mons_flag(type, M_WARM_BLOOD))
        return true;

    return false;
}

void cast_shuggoth_seed(int powc)
{
    struct dist beam;
    int i;

    mpr("Sow seed in whom?", MSGCH_PROMPT);

    direction( beam, DIR_TARGET, TARG_ENEMY );

    if (!beam.isValid)
    {
        mpr("You feel a distant frustration.");
        return;
    }

    if (beam.isMe)
    {
        if (!you.is_undead)
        {
            you.duration[DUR_INFECTED_SHUGGOTH_SEED] = 10;
            mpr("A deathly dread twitches in your chest.");
        }
        else
            mpr("You feel a distant frustration.");
    }

    i = mgrd[beam.tx][beam.ty];

    if (i == NON_MONSTER)
    {
        mpr("You feel a distant frustration.");
        return;
    }

    if (mons_can_host_shuggoth(menv[i].type))
    {
        if (random2(powc) > 100)
            mons_add_ench(&menv[i], ENCH_YOUR_SHUGGOTH_III);
        else
            mons_add_ench(&menv[i], ENCH_YOUR_SHUGGOTH_IV);

        simple_monster_message(&menv[i], " twitches.");
    }

    return;
}

void cast_condensation_shield(int pow)
{
    if (you.equip[EQ_SHIELD] != -1 || you.fire_shield)
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
            you.duration[DUR_CONDENSATION_SHIELD] += 5 + roll_dice(2, 3);
        else
        {
            mpr("A crackling disc of dense vapour forms in the air!");
            you.redraw_armour_class = 1;

            you.duration[DUR_CONDENSATION_SHIELD] = 10 + roll_dice(2, pow / 5);
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 30)
            you.duration[DUR_CONDENSATION_SHIELD] = 30;
    }

    return;
}                               // end cast_condensation_shield()

static int quadrant_blink(int x, int y, int pow, int garbage)
{
    UNUSED( garbage );

    if (x == you.x_pos && y == you.y_pos)
        return (0);

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( false );
        you.pet_target = MHITNOT;
        return (1);
    }

    if (pow > 100)
        pow = 100;

    // setup: Brent's new algorithm
    // we are interested in two things: distance of a test point from
    // the ideal 'line',  and the distance of a test point from two
    // actual points,  one in the 'correct' direction and one in the
    // 'incorrect' direction.

    // scale distance by 10 for more interesting numbers.
    int l,m;        // for line equation lx + my = 0
    l = (x - you.x_pos);
    m = (you.y_pos - y);

    int tx, ty;         // test x,y
    int rx, ry;         // x,y relative to you.
    int sx, sy;         // test point in the correct direction
    int bx = x;         // best x
    int by = y;         // best y

    int best_dist = 10000;

    sx = l;
    sy = -m;

    // for each point (a,b), distance from the line is | la + mb |

    for(int tries = pow * pow / 500 + 1; tries > 0; tries--)
    {
        if (!random_near_space(you.x_pos, you.y_pos, tx, ty))
            return 0;

        rx = tx - you.x_pos;
        ry = ty - you.y_pos;

        int dist = l * rx + m * ry;
        dist *= 10 * dist;      // square and multiply by 10

        // check distance to test points
        int dist1 = distance(rx, ry, sx, sy) * 10;
        int dist2 = distance(rx, ry, -sx, -sy) * 10;

        // 'good' points will always be closer to test point 1
        if (dist2 < dist1)
            dist += 80;          // make the point less attractive

        if (dist < best_dist)
        {
            best_dist = dist;
            bx = tx;
            by = ty;
        }
    }

    you.x_pos = bx;
    you.y_pos = by;

    return (1);
}

void cast_semi_controlled_blink(int pow)
{
    apply_one_neighbouring_square(quadrant_blink, pow);
    return;
}

void cast_stoneskin(int pow)
{
    if (you.is_undead)
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

        you.redraw_armour_class = 1;
    }

    you.duration[DUR_STONESKIN] += 10 + random2(pow) + random2(pow);

    if (you.duration[DUR_STONESKIN] > 50)
        you.duration[DUR_STONESKIN] = 50;
}

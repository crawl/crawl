/*
 *  File:       spells4.cc
 *  Summary:    new spells, focusing on Transmutations, Divinations,
 *              and other neglected areas of Crawl magic ;^)
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

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
#include "spl-mis.h"
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

// Just to avoid typing this over and over.
// Returns true if monster died. -- bwr
static bool _player_hurt_monster(monsters& m, int damage)
{
    if (damage > 0)
    {
        m.hurt(&you, damage, BEAM_MISSILE, false);

        if (m.alive())
        {
            print_wounds(&m);
            behaviour_event(&m, ME_WHACK, MHITYOU);
        }
        else
        {
            monster_die(&m, KILL_YOU, NON_MONSTER);
            return (true);
        }
    }

    return (false);
}

// Here begin the actual spells:
static int _shatter_monsters(coord_def where, int pow, int, actor *)
{
    dice_def   dam_dice( 0, 5 + pow / 3 );  // number of dice set below
    monsters *monster = monster_at(where);

    if (monster == NULL)
        return (0);

    // Removed a lot of silly monsters down here... people, just because
    // it says ice, rock, or iron in the name doesn't mean it's actually
    // made out of the substance. -- bwr
    switch (monster->type)
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
        if (mons_flies(monster))
            dam_dice.num = 1;
        else
            dam_dice.num = 3;
        break;
    }

    int damage = dam_dice.roll() - random2(monster->ac);

    if (damage > 0)
        _player_hurt_monster(*monster, damage);
    else
        damage = 0;

    return (damage);
}

static int _shatter_items(coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    int broke_stuff = 0;

    for ( stack_iterator si(where); si; ++si )
    {
        if (si->base_type == OBJ_POTIONS && !one_chance_in(10))
        {
            broke_stuff++;
            destroy_item(si->index());
        }
    }

    if (broke_stuff)
    {
        if (player_can_hear(where))
            mpr("You hear glass break.", MSGCH_SOUND);

        return 1;
    }

    return 0;
}

static int _shatter_walls(coord_def where, int pow, int, actor *)
{
    int chance = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (!in_bounds(where))
        return 0;

    const dungeon_feature_type grid = grd(where);

    switch (grid)
    {
    case DNGN_SECRET_DOOR:
        if (see_grid(where))
            mpr("A secret door shatters!");
        chance = 100;
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_OPEN_DOOR:
        if (see_grid(where))
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
        noisy(30, where);

        grd(where) = DNGN_FLOOR;

        if (grid == DNGN_ORCISH_IDOL)
            did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);

        return (1);
    }

    return (0);
}

void cast_shatter(int pow)
{
    int damage = 0;
    const bool silence = silenced(you.pos());

    if (silence)
        mpr("The dungeon shakes!");
    else
    {
        noisy(30, you.pos());
        mpr("The dungeon rumbles!", MSGCH_SOUND);
    }

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_NONE:
    case TRAN_SPIDER:
    case TRAN_LICH:
    case TRAN_DRAGON:
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

    if (damage > 0)
        ouch(damage, NON_MONSTER, KILLED_BY_TARGETTING);

    int rad = 3 + (you.skills[SK_EARTH_MAGIC] / 5);

    apply_area_within_radius(_shatter_items, you.pos(), pow, rad, 0);
    apply_area_within_radius(_shatter_monsters, you.pos(), pow, rad, 0);
    int dest = apply_area_within_radius( _shatter_walls, you.pos(),
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

    for ( radius_iterator ri(you.pos(), 8); ri; ++ri )
    {
        if (grd(*ri) == DNGN_SECRET_DOOR && random2(pow) > random2(15))
        {
            reveal_secret_door(*ri);
            found++;
        }
    }

    if (found)
        redraw_screen();

    mprf("You detect %s", (found > 0) ? "secret doors!" : "nothing.");
}

static int _sleep_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (0);

    if (mons_holiness(monster) != MH_NATURAL)
        return (0);
    if (check_mons_resist_magic(monster, pow))
        return (0);

    // Works on friendlies too, so no check for that.

    //jmf: Now that sleep == hibernation:
    const int res = mons_res_cold(monster);
    if (res > 0 && one_chance_in(std::max(4 - res, 1)))
        return (0);
    if (monster->has_ench(ENCH_SLEEP_WARY) && !one_chance_in(3))
        return (0);

    monster->put_to_sleep();

    if (mons_class_flag( monster->type, M_COLD_BLOOD ) && coinflip())
        monster->add_ench(ENCH_SLOW);

    return (1);
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

static int _tame_beast_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return 0;

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

static int _ignite_poison_objects(coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    int strength = 0;

    for ( stack_iterator si(where); si; ++si )
    {
        if (si->base_type == OBJ_POTIONS)
        {
            switch (si->sub_type)
            {
                // intentional fall-through all the way down
            case POT_STRONG_POISON:
                strength += 20;
            case POT_DEGENERATION:
                strength += 10;
            case POT_POISON:
                strength += 10;
                destroy_item(si->index());
            default:
                break;
            }
        }

        // FIXME: implement burning poisoned ammo
    }

    if (strength > 0)
    {
        place_cloud(CLOUD_FIRE, where,
                    strength + roll_dice(3, strength / 4), KC_YOU);
    }

    return (strength);
}

static int _ignite_poison_clouds( coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    bool did_anything = false;

    const int i = env.cgrid(where);
    if (i != EMPTY_CLOUD)
    {
        cloud_struct& cloud = env.cloud[i];
        if (cloud.type == CLOUD_STINK)
        {
            did_anything = true;
            cloud.type = CLOUD_FIRE;

            cloud.decay /= 2;

            if (cloud.decay < 1)
                cloud.decay = 1;
        }
        else if (cloud.type == CLOUD_POISON)
        {
            did_anything = true;
            cloud.type = CLOUD_FIRE;
        }
    }

    return did_anything;
}

static int _ignite_poison_monsters(coord_def where, int pow, int, actor *)
{
    bolt beam;
    beam.flavour = BEAM_FIRE;   // This is dumb, only used for adjust!

    dice_def dam_dice(0, 5 + pow/7);  // Dice added below if applicable.

    monsters *mon = monster_at(where);
    if (mon == NULL)
        return (0);

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

    int damage = dam_dice.roll();
    if (damage > 0)
    {
        damage = mons_adjust_flavoured( mon, beam, damage );

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Dice: %dd%d; Damage: %d",
             dam_dice.num, dam_dice.size, damage );
#endif

        if (!_player_hurt_monster(*mon, damage))
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
    // Poison branding becomes fire branding.
    if (you.weapon()
        && you.duration[DUR_WEAPON_BRAND]
        && get_weapon_brand(*you.weapon()) == SPWPN_VENOM)
    {
        if (set_item_ego_type(*you.weapon(), OBJ_WEAPONS, SPWPN_FLAMING))
        {
            mprf("%s bursts into flame!",
                 you.weapon()->name(DESC_CAP_YOUR).c_str());

            you.wield_change = true;
            you.duration[DUR_WEAPON_BRAND] +=
                1 + you.duration[DUR_WEAPON_BRAND] / 2;

            if (you.duration[DUR_WEAPON_BRAND] > 80)
                you.duration[DUR_WEAPON_BRAND] = 80;
        }
    }

    int totalstrength = 0;
    int ammo_burnt = 0;
    int potions_burnt = 0;
    bool was_wielding = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (!is_valid_item(item))
            continue;

        int strength = 0;

        if (item.base_type == OBJ_MISSILES && item.special == SPMSL_POISONED)
        {
            // Burn poison ammo.
            strength = item.quantity;
            ammo_burnt += item.quantity;
        }
        else if (item.base_type == OBJ_POTIONS)
        {
            // Burn poisonous potions.
            switch (item.sub_type)
            {
            case POT_STRONG_POISON:
                strength = 20 * item.quantity;
                break;
            case POT_DEGENERATION:
            case POT_POISON:
                strength = 10 * item.quantity;
                break;
            default:
                break;
            }

            if (strength)
                potions_burnt += item.quantity;
        }

        if (strength)
        {
            if (i == you.equip[EQ_WEAPON])
            {
                unwield_item();
                was_wielding = true;
            }

            item_was_destroyed(item);
            destroy_item(item);
        }

        totalstrength += strength;
    }

    if (ammo_burnt)
        mpr("Some ammunition you are carrying burns!");

    if (potions_burnt)
    {
        mprf("%s potion%s you are carrying explode%s!",
             potions_burnt > 1 ? "Some" : "A",
             potions_burnt > 1 ? "s" : "",
             potions_burnt > 1 ? "" : "s");
    }

    if (was_wielding)
        canned_msg(MSG_EMPTY_HANDED);

    if (totalstrength)
    {
        place_cloud(
            CLOUD_FIRE, you.pos(),
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) +
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) + 1,
            KC_YOU);
    }

    int damage = 0;
    // Player is poisonous.
    if (player_mutation_level(MUT_SPIT_POISON)
        || player_mutation_level(MUT_STINGER)
        || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER // poison attack
        || (!player_is_shapechanged()
            && (you.species == SP_GREEN_DRACONIAN       // poison breath
                || you.species == SP_KOBOLD             // poisonous corpse
                || you.species == SP_NAGA)))            // spit poison
    {
        damage = roll_dice(3, 5 + pow / 7);
    }

    // Player is poisoned.
    damage += roll_dice(you.duration[DUR_POISONING], 6);

    if (damage)
    {
        const int resist = player_res_fire();
        if (resist > 0)
        {
            mpr("You feel like your blood is boiling!");
            damage /= 3;
        }
        else if (resist < 0)
        {
            mpr("The poison in your system burns terribly!");
            damage *= 3;
        }
        else
        {
            mpr("The poison in your system burns!");
        }

        ouch(damage, NON_MONSTER, KILLED_BY_TARGETTING);

        if (you.duration[DUR_POISONING] > 0)
        {
            mpr( "You feel that the poison has left your system." );
            you.duration[DUR_POISONING] = 0;
        }
    }

    apply_area_visible(_ignite_poison_clouds, pow);
    apply_area_visible(_ignite_poison_objects, pow);
    apply_area_visible(_ignite_poison_monsters, pow);
}

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.duration[DUR_SILENCE] += 10 + random2avg( pow, 2 );

    if (you.duration[DUR_SILENCE] > 100)
        you.duration[DUR_SILENCE] = 100;

    if (you.duration[DUR_MESMERISED])
    {
        mpr("You break out of your daze!", MSGCH_RECOVERY);
        you.duration[DUR_MESMERISED] = 0;
        you.mesmerised_by.clear();
    }
}

static int _discharge_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    int damage = 0;

    bolt beam;
    beam.flavour = BEAM_ELECTRICITY; // used for mons_adjust_flavoured

    if (where == you.pos())
    {
        mpr("You are struck by lightning.");
        damage = 3 + random2(5 + pow / 10);
        damage = check_your_resists( damage, BEAM_ELECTRICITY );
        if (player_is_airborne())
            damage /= 2;
        ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC);
    }
    else if (monster == NULL)
        return (0);
    else if (mons_res_elec(monster) > 0 || mons_flies(monster))
        return (0);
    else
    {
        damage = 3 + random2(5 + pow/10);
        damage = mons_adjust_flavoured(monster, beam, damage );

        if (damage)
        {
            mprf("%s is struck by lightning.",
                 monster->name(DESC_CAP_THE).c_str());
            _player_hurt_monster(*monster, damage);
        }
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(3)) || (pow >= 3 && one_chance_in(10)))
    {
        mpr("The lightning arcs!");
        pow /= (coinflip() ? 2 : 3);
        damage += apply_random_around_square(_discharge_monsters, where,
                                             true, pow, 1);
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in
        // cast_discharge() are clean. -- bwr
        mpr("The lightning grounds out.");
    }

    return (damage);
}

void cast_discharge(int pow)
{
    int num_targs = 1 + random2(1 + pow / 25);
    int dam;

    dam = apply_random_around_square(_discharge_monsters, you.pos(),
                                     true, pow, num_targs);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Arcs: %d Damage: %d", num_targs, dam);
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

// Really this is just applying the best of Band/Warp weapon/Warp field
// into a spell that gives the "make monsters go away" benefit without
// the insane damage potential.  -- bwr
int disperse_monsters(coord_def where, int pow, int, actor *)
{
    monsters *defender = monster_at(where);
    if (defender == NULL)
        return 0;

    if (defender->type == MONS_BLINK_FROG
        || defender->type == MONS_PRINCE_RIBBIT)
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
    if (apply_area_around_square(disperse_monsters, you.pos(), pow) == 0)
        mpr("The air shimmers briefly around you.");
}

int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, kill_category whose,
                        killer_type killer)
{
    if (killer == KILL_NONE)
        killer = cloud_struct::whose_to_killer(whose);

    place_cloud( ctype, where,
                 (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                 whose, killer, spread_rate );

    return 1;
}

static int _passwall(coord_def where, int pow, int, actor *)
{
    int howdeep = 0;
    bool done = false;
    int shallow = 1 + (you.skills[SK_EARTH_MAGIC] / 8);
    bool non_rock_barriers = false;

    // Irony: you can start on a secret door but not a door.
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls... I'm not sure we should
    // even allow statues (should be contiguous rock). -- bwr
    // XXX: Allow statues as entry points?
    if (grd(where) != DNGN_ROCK_WALL && grd(where) != DNGN_CLEAR_ROCK_WALL)
    {
        mpr("That's not a passable wall.");
        return 0;
    }

    coord_def delta = where - you.pos();
    coord_def n = where;

    while (!done)
    {
        if (!in_bounds(n))
        {
            mpr("You sense an overwhelming volume of rock.");
            return 0;
        }

        switch (grd(n))
        {
        default:
            if (grid_is_solid(grd(n)))
                non_rock_barriers = true;
            done = true;
            break;

        case DNGN_ROCK_WALL:
        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_ORCISH_IDOL:
        case DNGN_GRANITE_STATUE:
        case DNGN_SECRET_DOOR:
            n += delta;
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
                ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_PETRIFICATION);
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

    // Passwall delay is reduced, and the delay cannot be interrupted.
    start_delay( DELAY_PASSWALL, 1 + howdeep, n.x, n.y );

    return 1;
}

void cast_passwall(int pow)
{
    apply_one_neighbouring_square(_passwall, pow);
}

static int _intoxicate_monsters(coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    monsters *monster = monster_at(where);
    if (monster == NULL
        || mons_intel(monster) < I_NORMAL
        || mons_holiness(monster) != MH_NATURAL
        || mons_res_poison(monster) > 0)
    {
        return 0;
    }

    monster->add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU));
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

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (false);

    // Already glowing.
    if (mons_class_flag(monster->type, M_GLOWS))
        return (false);

    mon_enchant bklt = monster->get_ench(ENCH_BACKLIGHT);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (monster->has_ench(ENCH_INVIS))
    {
        if (!monster->has_ench(ENCH_BACKLIGHT))
        {
            monster->add_ench(
                mon_enchant(ENCH_BACKLIGHT, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message(monster, " is lined in light.");
        }
        return (true);
    }

    monster->add_ench(mon_enchant(ENCH_BACKLIGHT, 1));

    if (lvl == 0)
        simple_monster_message(monster, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(monster, " glows brighter for a moment.");
    else
        simple_monster_message(monster, " glows brighter.");

    return (true);
}

bool cast_evaporate(int pow, bolt& beem, int pot_idx)
{
    ASSERT(you.inv[pot_idx].base_type == OBJ_POTIONS);
    item_def& potion = you.inv[pot_idx];

    beem.name        = "potion";
    beem.colour      = potion.colour;
    beem.type        = dchar_glyph(DCHAR_FIRED_FLASK);
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU_MISSILE;
    beem.is_beam     = false;
    beem.aux_source.clear();

    beem.hit        = you.dex / 2 + roll_dice( 2, you.skills[SK_THROWING] / 2 + 1 );
    beem.damage     = dice_def( 1, 0 );  // no damage, just producing clouds
    beem.ench_power = pow;               // used for duration only?
    beem.is_explosion = true;
    beem.range      = 8;

    beem.flavour    = BEAM_POTION_STINKING_CLOUD;
    beam_type tracer_flavour = BEAM_MMISSILE;

    switch (potion.sub_type)
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
        if (potion.sub_type == POT_BERSERK_RAGE)
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

    // Fire tracer. FIXME: use player_tracer() here!
    beem.source         = you.pos();
    beem.can_see_invis  = player_see_invis();
    beem.smart_monster  = true;
    beem.attitude       = ATT_FRIENDLY;
    beem.beam_cancelled = false;
    beem.is_tracer      = true;
    beem.friend_info.reset();

    beam_type real_flavour = beem.flavour;
    beem.flavour           = tracer_flavour;
    beem.fire();

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
    beem.fire();

    // Use up a potion.
    if (is_blood_potion(potion))
        remove_oldest_blood_potion(potion);

    dec_inv_item_quantity( pot_idx, 1 );

    return (true);
}

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

bool cast_fragmentation(int pow, const dist& spd)
{
    int debris = 0;
    bool explode = false;
    bool hole = true;
    const char *what = NULL;

    ray_def ray;
    if (!find_ray(you.pos(), spd.target, false, ray))
    {
        mpr("There's a wall in the way!");
        return (false);
    }

    //FIXME: If (player typed '>' to attack floor) goto do_terrain;

    bolt beam;

    beam.flavour     = BEAM_FRAG;
    beam.type        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.beam_source = MHITYOU;
    beam.thrower     = KILL_YOU;
    beam.ex_size     = 1;
    beam.source      = you.pos();
    beam.hit         = AUTOMATIC_HIT;

    beam.set_target(spd);
    beam.aux_source.clear();

    // Number of dice vary... 3 is easy/common, but it can get as high as 6.
    beam.damage = dice_def(0, 5 + pow / 10);

    const dungeon_feature_type grid = grd(spd.target);

    if (monsters *mon = monster_at(spd.target))
    {
        // Save the monster's name in case it isn't available later.
        const std::string name_cap_the = mon->name(DESC_CAP_THE);

        switch (mon->type)
        {
        case MONS_WOOD_GOLEM:
            simple_monster_message(mon, " shudders violently!");

            // We use beam.damage not only for inflicting damage here,
            // but so that later on we'll know that the spell didn't
            // fizzle (since we don't actually explode wood golems). -- bwr
            explode         = false;
            beam.damage.num = 2;
            _player_hurt_monster(*mon, beam.damage.roll());
            break;

        case MONS_IRON_GOLEM:
        case MONS_METAL_GARGOYLE:
            explode         = true;
            beam.name       = "blast of metal fragments";
            beam.colour     = CYAN;
            beam.damage.num = 4;
            if (_player_hurt_monster(*mon, beam.damage.roll()))
                beam.damage.num += 2;
            break;

        case MONS_CLAY_GOLEM:   // Assume baked clay and not wet loam.
        case MONS_STONE_GOLEM:
        case MONS_EARTH_ELEMENTAL:
        case MONS_GARGOYLE:
            explode         = true;
            beam.ex_size    = 2;
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = 3;
            if (_player_hurt_monster(*mon, beam.damage.roll()))
                beam.damage.num++;
            break;

        case MONS_SILVER_STATUE:
        case MONS_ORANGE_STATUE:
            explode         = true;
            beam.ex_size    = 2;
            if (mon->type == MONS_SILVER_STATUE)
            {
                beam.name       = "blast of silver fragments";
                beam.colour     = WHITE;
                beam.damage.num = 3;
            }
            else
            {
                beam.name       = "blast of orange crystal shards";
                beam.colour     = LIGHTRED;
                beam.damage.num = 6;
            }

            {
                int statue_damage = beam.damage.roll() * 2;
                if (pow >= 50 && one_chance_in(10))
                    statue_damage = mon->hit_points;

                if (_player_hurt_monster(*mon, statue_damage))
                    beam.damage.num += 2;
            }
            break;

        case MONS_CRYSTAL_GOLEM:
            explode         = true;
            beam.ex_size    = 2;
            beam.name       = "blast of crystal shards";
            beam.colour     = WHITE;
            beam.damage.num = 4;
            if (_player_hurt_monster(*mon, beam.damage.roll()))
                beam.damage.num += 2;
            break;

        default:
            if (mons_is_icy(mon->type)) // blast of ice
            {
                explode         = true;
                beam.name       = "icy blast";
                beam.colour     = WHITE;
                beam.damage.num = 2;
                    beam.flavour    = BEAM_ICE;
                if (_player_hurt_monster(*mon, beam.damage.roll()))
                    beam.damage.num++;
                break;
            }
            else if (mons_is_skeletal(mon->type)
                || mon->type == MONS_FLYING_SKULL) // blast of bone
            {
                mprf("The %s explodes into sharp fragments of bone!",
                     (mon->type == MONS_FLYING_SKULL) ? "skull" : "skeleton");

                explode     = true;
                beam.name   = "blast of bone shards";
                beam.colour = LIGHTGREY;

                if (x_chance_in_y(pow / 5, 50)) // potential insta-kill
                {
                    monster_die(mon, KILL_YOU, NON_MONSTER);
                    beam.damage.num = 4;
                }
                else
                {
                      beam.damage.num = 2;
                      if (_player_hurt_monster(*mon, beam.damage.roll()))
                          beam.damage.num += 2;
                  }
                  goto all_done; // i.e., no "Foo Explodes!"
            }
            else
            {
                const bool petrifying = mons_is_petrifying(mon);
                const bool petrified = mons_is_petrified(mon) && !petrifying;

                // Petrifying or petrified monsters can be exploded.
                if (petrifying || petrified)
                {
                    explode         = true;
                    beam.ex_size    = petrifying ? 1 : 2;
                    beam.name       = "blast of petrified fragments";
                    beam.colour     = mons_class_colour(mon->type);
                    beam.damage.num = petrifying ? 2 : 3;
                    if (_player_hurt_monster(*mon, beam.damage.roll()))
                        beam.damage.num++;
                    break;
                }
            }

            // Mark that a monster was targetted.
            beam.damage.num = 1;

            // Yes, this spell does lousy damage if the monster
            // isn't susceptible. -- bwr
            _player_hurt_monster(*mon, roll_dice(1, 5 + pow / 25));
            goto do_terrain;
        }

        mprf("%s shatters!", name_cap_the.c_str());
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
        beam.colour = env.rock_colour;
        // fall-through
    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        what = "wall";
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            beam.colour = env.rock_colour;
        // fall-through
    case DNGN_ORCISH_IDOL:
        if (what == NULL)
            what = "stone idol";
        if (beam.colour == 0)
            beam.colour = DARKGREY;
        // fall-through
    case DNGN_GRANITE_STATUE:   // normal rock -- big explosion
        if (what == NULL)
            what = "statue";

        explode = true;

        beam.name       = "blast of rock fragments";
        beam.damage.num = 3;
        if (beam.colour == 0)
            beam.colour = LIGHTGREY;

        if ((grid == DNGN_ORCISH_IDOL
             || grid == DNGN_GRANITE_STATUE
             || pow >= 40 && grid == DNGN_ROCK_WALL && one_chance_in(3)
             || pow >= 40 && grid == DNGN_CLEAR_ROCK_WALL
                 && one_chance_in(3)
             || pow >= 60 && grid == DNGN_STONE_WALL && one_chance_in(10)
             || pow >= 60 && grid == DNGN_CLEAR_STONE_WALL
                 && one_chance_in(10)))
        {
            // terrain blew up real good:
            beam.ex_size        = 2;
            grd(spd.target)     = DNGN_FLOOR;
            debris              = DEBRIS_ROCK;
        }
        break;

    //
    // Metal -- small but nasty explosion
    //

    case DNGN_METAL_WALL:
        what            = "metal wall";
        beam.colour     = CYAN;
        explode         = true;
        beam.name       = "blast of metal fragments";
        beam.damage.num = 4;

        if (pow >= 80 && x_chance_in_y(pow / 5, 500))
        {
            beam.damage.num += 2;
            grd(spd.target)  = DNGN_FLOOR;
            debris           = DEBRIS_METAL;
        }
        break;

    //
    // Crystal
    //

    case DNGN_GREEN_CRYSTAL_WALL:       // crystal -- large & nasty explosion
        what            = "crystal wall";
        beam.colour     = GREEN;
        explode         = true;
        beam.ex_size    = 2;
        beam.name       = "blast of crystal shards";
        beam.damage.num = 5;

        if (grid == DNGN_GREEN_CRYSTAL_WALL && coinflip())
        {
            beam.ex_size    = coinflip() ? 3 : 2;
            grd(spd.target) = DNGN_FLOOR;
            debris          = DEBRIS_CRYSTAL;
        }
        break;

    //
    // Traps
    //

    case DNGN_UNDISCOVERED_TRAP:
    case DNGN_TRAP_MECHANICAL:
    {
        trap_def* ptrap = find_trap(spd.target);
        if (ptrap && ptrap->category() != DNGN_TRAP_MECHANICAL)
        {
            // Non-mechanical traps don't explode with this spell. -- bwr
            break;
        }

        // Undiscovered traps appear as exploding from the floor. -- bwr
        what = ((grid == DNGN_UNDISCOVERED_TRAP) ? "floor" : "trap");

        explode         = true;
        hole            = false;    // to hit monsters standing on traps
        beam.name       = "blast of fragments";
        beam.colour     = env.floor_colour;  // in order to blend in
        beam.damage.num = 2;

        // Exploded traps are nonfunctional, ammo is also ruined -- bwr
        ptrap->destroy();
        break;
    }

    //
    // Stone doors and arches
    //

    case DNGN_OPEN_DOOR:
    case DNGN_CLOSED_DOOR:
        // Doors always blow up, stone arches never do (would cause problems).
        grd(spd.target) = DNGN_FLOOR;

        // fall-through
    case DNGN_STONE_ARCH:          // Floor -- small explosion.
        explode         = true;
        hole            = false;  // to hit monsters standing on doors
        beam.name       = "blast of rock fragments";
        beam.colour     = LIGHTGREY;
        beam.damage.num = 2;
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
    if (explode && beam.damage.num > 0)
    {
        if (what != NULL)
            mprf("The %s shatters!", what);

        beam.explode(true, hole);

        if (grid == DNGN_ORCISH_IDOL)
            did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);
    }
    else if (beam.damage.num == 0)
    {
        // If damage dice are zero, assume that nothing happened at all.
        canned_msg(MSG_SPELL_FIZZLES);
    }

    return (true);
}

bool cast_portal_projectile(int pow)
{
    dist target;
    int item = get_ammo_to_shoot(-1, target, true);
    if (item == -1)
        return (false);

    if (grid_is_solid(target.target))
    {
        mpr("You can't shoot at gazebos.");
        return (false);
    }

    // Can't use portal through walls. (That'd be just too cheap!)
    if (trans_wall_blocking( target.target ))
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

bool cast_apportation(int pow, const coord_def& where)
{
    // Protect the player from destroying the item.
    if (grid_destroys_items(grd(you.pos())))
    {
        mpr( "That would be silly while over this terrain!" );
        return (false);
    }

    if (trans_wall_blocking(where))
    {
        mpr("A translucent wall is in the way.");
        return (false);
    }

    // Let's look at the top item in that square...
    const int item_idx = igrd(where);
    if (item_idx == NON_ITEM)
    {
        // Maybe the player *thought* there was something there (a mimic.)
        if (monsters* m = monster_at(where))
        {
            if (mons_is_mimic(m->type) && you.can_see(m))
            {
                mprf("%s twitches.", m->name(DESC_CAP_THE).c_str());
                // Nothing else gives this message, so identify the mimic.
                m->flags |= MF_KNOWN_MIMIC;
                return (true);  // otherwise you get free mimic ID
            }
        }

        mpr("There are no items there.");
        return (false);
    }

    item_def& item = mitm[item_idx];

    // Mass of one unit.
    const int unit_mass = item_mass(item);
    const int max_mass = pow * 30 + random2(pow * 20);

    int max_units = item.quantity;
    if (unit_mass > 0)
        max_units = max_mass / unit_mass;

    if (max_units <= 0)
    {
        mpr("The mass is resisting your pull.");
        return (true);
    }

    // We need to modify the item *before* we move it, because
    // move_top_item() might change the location, or merge
    // with something at our position.
    mprf("Yoink! You pull the item%s to yourself.",
         (item.quantity > 1) ? "s" : "");

    if (max_units < item.quantity)
    {
        item.quantity = max_units;
        mpr("You feel that some mass got lost in the cosmic void.");
    }

    // If we apport a net, free the monster under it.
    if (item.base_type == OBJ_MISSILES
        && item.sub_type == MI_THROWING_NET
        && item_is_stationary(item))
    {
        remove_item_stationary(item);
        if (monsters *monster = monster_at(where))
            monster->del_ench(ENCH_HELD, true);
    }

    // Actually move the item.
    move_top_item(where, you.pos());

    return (true);
}

bool wielding_rocks()
{
    bool rc = false;
    if (you.weapon())
    {
        const item_def& wpn(*you.weapon());
        rc = (wpn.base_type == OBJ_MISSILES
              && (wpn.sub_type == MI_STONE || wpn.sub_type == MI_LARGE_ROCK));
    }
    return (rc);
}

bool cast_sandblast(int pow, bolt &beam)
{
    const bool big = wielding_rocks();
    const bool success = zapping(big ? ZAP_SANDBLAST
                                     : ZAP_SMALL_SANDBLAST, pow, beam, true);

    if (big && success)
        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );

    return (success);
}

void remove_condensation_shield()
{
    mpr("Your icy shield evaporates.", MSGCH_DURATION);
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
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
            you.duration[DUR_CONDENSATION_SHIELD] = 10 + roll_dice(2, pow / 5);
            you.redraw_armour_class = true;
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 30)
            you.duration[DUR_CONDENSATION_SHIELD] = 30;
    }
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
        if (you.shield()
            || you.duration[DUR_FIRE_SHIELD]
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

static int _quadrant_blink(coord_def where, int pow, int, actor *)
{
    if (where == you.pos())
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
    const coord_def base = you.pos() + (where - you.pos()) * dist;

    // This can take a while if pow is high and there's lots of translucent
    // walls nearby.
    coord_def target;
    bool found = false;
    for ( int i = 0; i < (pow*pow) / 500 + 1; ++i )
    {
        // Find a space near our base point...
        // First try to find a random square not adjacent to the basepoint,
        // then one adjacent if that fails.
        if (!random_near_space(base, target)
            && !random_near_space(base, target, true))
        {
            return 0;
        }

        // ... which is close enough, but also far enough from us.
        if (distance(base, target) > 10 || distance(you.pos(), target) < 8)
            continue;

        if (!see_grid_no_trans(target))
            continue;

        found = true;
        break;
    }

    if (!found)
        return(0);

    you.moveto(target);
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

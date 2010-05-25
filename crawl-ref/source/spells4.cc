/*
 *  File:       spells4.cc
 *  Summary:    new spells, focusing on Transmutations, Divinations,
 *              and other neglected areas of Crawl magic ;^)
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 */

#include "AppHdr.h"

#include <string>
#include <iostream>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "abyss.h"
#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "debug.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "los.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "quiver.h"
#include "religion.h"
#include "godconduct.h"
#include "skills.h"
#include "spells1.h"
#include "spells4.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stuff.h"
#include "areas.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"

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
static bool _player_hurt_monster(monsters& m, int damage,
        beam_type flavour = BEAM_MISSILE)
{
    if (damage > 0)
    {
        m.hurt(&you, damage, flavour, false);

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
    dice_def dam_dice(0, 5 + pow / 3); // number of dice set below
    monsters *mon = monster_at(where);

    if (mon == NULL)
        return (0);

    // Removed a lot of silly monsters down here... people, just because
    // it says ice, rock, or iron in the name doesn't mean it's actually
    // made out of the substance. - bwr
    switch (mon->type)
    {
    case MONS_SILVER_STATUE: // 3/2 damage
        dam_dice.num = 4;
        break;

    case MONS_CURSE_SKULL:      // double damage
    case MONS_CLAY_GOLEM:
    case MONS_STONE_GOLEM:
    case MONS_IRON_GOLEM:
    case MONS_CRYSTAL_GOLEM:
    case MONS_ORANGE_STATUE:
    case MONS_STATUE:
    case MONS_EARTH_ELEMENTAL:
    case MONS_GARGOYLE:
        dam_dice.num = 6;
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
    case MONS_JELLYFISH:
    case MONS_WATER_ELEMENTAL:
        dam_dice.num = 1;
        dam_dice.size /= 2;
        break;

    case MONS_DANCING_WEAPON:     // flies, but earth based
    case MONS_MOLTEN_GARGOYLE:
    case MONS_QUICKSILVER_DRAGON:
        // Soft, earth creatures... would normally resist to 1 die, but
        // are sensitive to this spell. - bwr
        dam_dice.num = 2;
        break;

    default:
        if (mon->is_insubstantial()) // normal damage
            dam_dice.num = 0;
        else if (mon->is_icy())      // 3/2 damage
            dam_dice.num = 4;
        else if (mon->is_skeletal()) // double damage
            dam_dice.num = 6;
        else if (mons_flies(mon))    // 1/3 damage
            dam_dice.num = 1;
        else
        {
            const bool petrifying = mon->petrifying();
            const bool petrified = mon->petrified() && !petrifying;

            // Petrifying or petrified monsters can be shattered.
            if (petrifying || petrified)
                dam_dice.num = petrifying ? 4 : 6; // 3/2 or double damage
            else
                dam_dice.num = 3;
        }
        break;
    }

    int damage = std::max(0, dam_dice.roll() - random2(mon->ac));

    if (damage > 0)
        _player_hurt_monster(*mon, damage);

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

    if (env.markers.property_at(where, MAT_ANY, "veto_shatter") == "veto")
        return 0;

    const dungeon_feature_type grid = grd(where);

    switch (grid)
    {
    case DNGN_SECRET_DOOR:
        if (you.see_cell(where))
            mpr("A secret door shatters!");
        chance = 100;
        break;

    case DNGN_CLOSED_DOOR:
    case DNGN_DETECTED_SECRET_DOOR:
    case DNGN_OPEN_DOOR:
        if (you.see_cell(where))
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
    case DNGN_SLIMY_WALL:
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
        set_terrain_changed(where);

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
        ouch(damage, NON_MONSTER, KILLED_BY_TARGETING);

    int rad = 3 + (you.skills[SK_EARTH_MAGIC] / 5);

    apply_area_within_radius(_shatter_items, you.pos(), pow, rad, 0);
    apply_area_within_radius(_shatter_monsters, you.pos(), pow, rad, 0);
    int dest = apply_area_within_radius(_shatter_walls, you.pos(),
                                        pow, rad, 0);

    if (dest && !silence)
        mpr("Ka-crash!", MSGCH_SOUND);
}

// Cast_phase_shift: raises evasion (by 8 currently) via Translocations.
void cast_phase_shift(int pow)
{
    if (!you.duration[DUR_PHASE_SHIFT])
        mpr("You feel the strange sensation of being on two planes at once.");
    else
        mpr("Your feel the material plane grow further away.");

    you.increase_duration(DUR_PHASE_SHIFT, 5 + random2(pow), 30);
    you.redraw_evasion = true;
}

void cast_see_invisible(int pow)
{
    if (you.can_see_invisible())
        mpr("Nothing seems to happen.");
    else
    {
        mpr("Your vision seems to sharpen.");

        // We might have to turn autopickup back on again.
        // TODO: Once the spell times out we might want to check all monsters
        //       in LOS for invisibility and turn autopickup off again, if
        //       needed.
        autotoggle_autopickup(false);
    }

    // No message if you already are under the spell.
    you.increase_duration(DUR_SEE_INVISIBLE, 10 + random2(2 + pow/2), 100);
}

// The description idea was okay, but this spell just isn't that exciting.
// So I'm converting it to the more practical expose secret doors. -- bwr
void cast_detect_secret_doors(int pow)
{
    int found = 0;

    for (radius_iterator ri(you.get_los()); ri; ++ri )
        if (grd(*ri) == DNGN_SECRET_DOOR && random2(pow) > random2(15))
        {
            reveal_secret_door(*ri);
            found++;
        }

    if (found)
        redraw_screen();

    mprf("You detect %s", (found > 0) ? "secret doors!" : "nothing.");
}

static int _sleep_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (!monster)
        return (0);

    if (!monster->can_hibernate(true))
        return (0);

    if (monster->check_res_magic(pow))
        return (0);

    const int res = monster->res_cold();
    if (res > 0 && one_chance_in(std::max(4 - res, 1)))
        return (0);

    if (monster->has_ench(ENCH_SLEEP_WARY) && !one_chance_in(3))
        return (0);

    monster->hibernate();
    monster->expose_to_element(BEAM_COLD, 2);
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
        MONS_HOG, MONS_GIANT_FROG, MONS_GIANT_TOAD,
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

    if (!_is_domesticated_animal(monster->type) || monster->friendly()
        || player_will_anger_monster(monster))
    {
        return 0;
    }

    // 50% bonus for dogs
    if (monster->type == MONS_HOUND || monster->type == MONS_WAR_DOG )
        pow += (pow / 2);

    if (you.species == SP_HILL_ORC && monster->type == MONS_WARG)
        pow += (pow / 2);

    if (monster->check_res_magic(pow))
        return 0;

    simple_monster_message(monster, " is tamed!");

    if (random2(100) < random2(pow / 10))
        monster->attitude = ATT_FRIENDLY;  // permanent
    else
        monster->add_ench(ENCH_CHARM);     // temporary
    mons_att_changed(monster);

    return 1;
}

void cast_tame_beasts(int pow)
{
    apply_area_visible(_tame_beast_monsters, pow);
}

static int _ignite_poison_objects(coord_def where, int pow, int, actor *)
{
    UNUSED(pow);

    int strength = 0;

    for (stack_iterator si(where); si; ++si)
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
    UNUSED(pow);

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
        damage = mons_adjust_flavoured(mon, beam, damage);
        simple_monster_message(mon, " seems to burn from within!");

        dprf("Dice: %dd%d; Damage: %d", dam_dice.num, dam_dice.size, damage);

        if (!_player_hurt_monster(*mon, damage))
        {
            // Monster survived, remove any poison.
            mon->del_ench(ENCH_POISON);
            behaviour_event(mon, ME_ALERT);
        }

        return (1);
    }

    return (0);
}

void cast_ignite_poison(int pow)
{
    flash_view(RED);

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

            int increase = 1 + you.duration[DUR_WEAPON_BRAND]
                               /(2 * BASELINE_DELAY);

            you.increase_duration(DUR_WEAPON_BRAND, increase, 80);
        }
    }

    int totalstrength = 0;
    int ammo_burnt = 0;
    int potions_burnt = 0;
    bool was_wielding = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (!item.is_valid())
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
            mpr("The poison in your system burns!");

        ouch(damage, NON_MONSTER, KILLED_BY_TARGETING);

        if (you.duration[DUR_POISONING] > 0)
        {
            mpr("You feel that the poison has left your system.");
            you.duration[DUR_POISONING] = 0;
        }
    }

    apply_area_visible(_ignite_poison_clouds, pow);
    apply_area_visible(_ignite_poison_objects, pow);
    apply_area_visible(_ignite_poison_monsters, pow);

#ifndef USE_TILE
    delay(100); // show a brief flash
#endif
    flash_view(0);
}

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.increase_duration(DUR_SILENCE, 10 + random2avg(pow, 2), 100);
    invalidate_agrid();

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
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
        damage = check_your_resists(damage, BEAM_ELECTRICITY,
                                    "static discharge");
        if (you.airborne())
            damage /= 2;
        ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC);
    }
    else if (monster == NULL)
        return (0);
    else if (monster->res_elec() > 0 || mons_flies(monster))
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

    dprf("Arcs: %d Damage: %d", num_targs, dam);

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
// the insane damage potential. - bwr
int disperse_monsters(coord_def where, int pow, int, actor *)
{
    monsters *mon = monster_at(where);
    if (!mon)
        return (0);

    if (mons_genus(mon->type) == MONS_BLINK_FROG)
    {
        simple_monster_message(mon, " resists.");
        return (1);
    }
    else if (mon->check_res_magic(pow))
    {
        // XXX: Note that this might affect magic-immunes!
        if (coinflip())
        {
            simple_monster_message(mon, " partially resists.");
            monster_blink(mon);
        }
        else
            simple_monster_message(mon, " resists.");
        return (1);
    }
    else
    {
        monster_teleport(mon, true);
        return (1);
    }

    return (0);
}

void cast_dispersal(int pow)
{
    if (apply_area_around_square(disperse_monsters, you.pos(), pow) == 0)
        mpr("The air shimmers briefly around you.");
}

int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, kill_category whose,
                        killer_type killer, int colour, std::string name,
                        std::string tile)
{
    if (killer == KILL_NONE)
        killer = cloud_struct::whose_to_killer(whose);

    place_cloud( ctype, where,
                 (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                 whose, killer, spread_rate, colour, name, tile );

    return 1;
}

bool _feat_is_passwallable(dungeon_feature_type feat)
{
    // Irony: you can passwall through a secret door but not a door.
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls...
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        return (true);
    default:
        return (false);
    }
}

bool cast_passwall(const coord_def& delta, int pow)
{
    int shallow = 1 + (you.skills[SK_EARTH_MAGIC] / 8);
    int range = shallow + random2(pow) / 25;
    int maxrange = shallow + pow / 25;

    coord_def dest;
    for (dest = you.pos() + delta;
         in_bounds(dest) && _feat_is_passwallable(grd(dest));
         dest += delta) ;

    int walls = (dest - you.pos()).rdist() - 1;
    if (walls == 0)
    {
        mpr("That's not a passable wall.");
        return (false);
    }

    // Below here, failing to cast yields information to the
    // player, so we don't make the spell abort (return true).
    if (!in_bounds(dest))
        mpr("You sense an overwhelming volume of rock.");
    else if (feat_is_solid(grd(dest)))
        mpr("Something is blocking your path through the rock.");
    else if (walls > maxrange)
        mpr("This rock feels extremely deep.");
    else if (walls > range)
        mpr("You fail to penetrate the rock.");
    else
    {
        // Passwall delay is reduced, and the delay cannot be interrupted.
        start_delay(DELAY_PASSWALL, 1 + walls, dest.x, dest.y);
    }
    return (true);
}

static int _intoxicate_monsters(coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    monsters *monster = monster_at(where);
    if (monster == NULL
        || mons_intel(monster) < I_NORMAL
        || monster->holiness() != MH_NATURAL
        || monster->res_poison() > 0)
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
        && lose_stat( STAT_INT, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow);
}

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED(pow);
    UNUSED(garbage);

    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (false);

    // Already glowing.
    if (mons_class_flag(monster->type, M_GLOWS))
        return (false);

    mon_enchant bklt = monster->get_ench(ENCH_CORONA);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (monster->has_ench(ENCH_INVIS))
    {
        if (!monster->has_ench(ENCH_CORONA))
        {
            monster->add_ench(
                mon_enchant(ENCH_CORONA, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message(monster, " is lined in light.");
        }
        return (true);
    }

    monster->add_ench(mon_enchant(ENCH_CORONA, 1));

    if (lvl == 0)
        simple_monster_message(monster, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(monster, " glows brighter for a moment.");
    else
        simple_monster_message(monster, " glows brighter.");

    return (true);
}

// Returns a vector of cloud types created by this potion type.
// FIXME: Heavily duplicated code.
static std::vector<int> _get_evaporate_result(int potion)
{
    std::vector <int> beams;
    bool random_potion = false;
    switch (potion)
    {
    case POT_STRONG_POISON:
    case POT_POISON:
        beams.push_back(BEAM_POTION_POISON);
        break;

    case POT_DEGENERATION:
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_DECAY:
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_PARALYSIS:
    case POT_CONFUSION:
    case POT_SLOWING:
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_MUTATION:
        beams.push_back(BEAM_POTION_MUTAGENIC);
        // deliberate fall-through
    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        random_potion = true;
        break;

    default:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_STINKING_CLOUD);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_BLUE_SMOKE);
        beams.push_back(BEAM_POTION_STEAM);
        random_potion = true;
        break;
    }

    std::vector<int> clouds;
    for (unsigned int k = 0; k < beams.size(); ++k)
        clouds.push_back(beam2cloud((beam_type) beams[k]));

    if (random_potion)
    {
        // handled in beam.cc
        clouds.push_back(CLOUD_FIRE);
        clouds.push_back(CLOUD_STINK);
        clouds.push_back(CLOUD_COLD);
        clouds.push_back(CLOUD_POISON);
        clouds.push_back(CLOUD_BLUE_SMOKE);
        clouds.push_back(CLOUD_STEAM);
    }

    return (clouds);
}

// Returns a comma-separated list of all cloud types potentially created
// by this potion type. Doesn't respect the different probabilities.
std::string get_evaporate_result_list(int potion)
{
    std::vector<int> clouds = _get_evaporate_result(potion);
    std::sort(clouds.begin(), clouds.end());

    std::vector<std::string> clouds_list;

    int old_cloud = -1;
    for (unsigned int k = 0; k < clouds.size(); ++k)
    {
        const int new_cloud = clouds[k];
        if (new_cloud == old_cloud)
            continue;

        // This relies on all smoke types being handled as blue.
        if (new_cloud == CLOUD_BLUE_SMOKE)
            clouds_list.push_back("coloured smoke");
        else
            clouds_list.push_back(cloud_name((cloud_type) new_cloud));

        old_cloud = new_cloud;
    }

    return comma_separated_line(clouds_list.begin(), clouds_list.end(),
                                " or ", ", ");
}


// Assumes beem.range is already set -cao
bool cast_evaporate(int pow, bolt& beem, int pot_idx)
{
    ASSERT(you.inv[pot_idx].base_type == OBJ_POTIONS);
    item_def& potion = you.inv[pot_idx];

    beem.name        = "potion";
    beem.colour      = potion.colour;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_FLASK);
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU_MISSILE;
    beem.is_beam     = false;
    beem.aux_source.clear();

    beem.hit        = you.dex() / 2 + roll_dice( 2, you.skills[SK_THROWING] / 2 + 1 );
    beem.damage     = dice_def( 1, 0 );  // no damage, just producing clouds
    beem.ench_power = pow;               // used for duration only?
    beem.is_explosion = true;

    beem.flavour    = BEAM_POTION_STINKING_CLOUD;
    beam_type tracer_flavour = BEAM_MMISSILE;

    switch (potion.sub_type)
    {
    case POT_STRONG_POISON:
        beem.ench_power *= 2;
        // deliberate fall-through
    case POT_POISON:
        beem.flavour   = BEAM_POTION_POISON;
        tracer_flavour = BEAM_POISON;
        break;

    case POT_DEGENERATION:
        beem.effect_known = false;
        beem.flavour   = (coinflip() ? BEAM_POTION_POISON : BEAM_POTION_MIASMA);
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
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
        // Maybe we'll get a mutagenic cloud.
        if (coinflip())
        {
            beem.effect_known = true;
            tracer_flavour = beem.flavour = BEAM_POTION_MUTAGENIC;
            break;
        }
        // if not, deliberate fall through for something random

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
        default:  beem.flavour = BEAM_POTION_STEAM;           break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;
    }

    // Fire tracer. FIXME: use player_tracer() here!
    beem.source         = you.pos();
    beem.can_see_invis  = you.can_see_invisible();
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
        exercise(SK_THROWING, 1);

    // Really fire.
    beem.flavour = real_flavour;
    beem.is_tracer = false;
    beem.fire();

    // Use up a potion.
    if (is_blood_potion(potion))
        remove_oldest_blood_potion(potion);

    dec_inv_item_quantity(pot_idx, 1);

    return (true);
}

// The intent of this spell isn't to produce helpful potions
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not
// using up the corpse might also lead to game balance problems. - bwr
bool cast_fulsome_distillation(int /*pow*/)
{
    int num_corpses = 0;
    int corpse = -1;

    // Determine how many corpses are available.
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            corpse = si->index();
            ++num_corpses;
        }
    }

    // If there is only one corpse, distill it; otherwise, ask the player
    // which corpse to use.
    switch (num_corpses)
    {
        case 0:
            mpr("There aren't any corpses here!");
            return (false);
        case 1:
            // Use the only corpse available without prompting.
            break;
        default:
            // Search items at the player's location for corpses.
            // The last corpse detected earlier is irrelevant.
            corpse = -1;
            for (stack_iterator si(you.pos(), true); si; ++si)
            {
                if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
                {
                    snprintf(info, INFO_SIZE, "Distill a potion from %s?",
                             get_menu_colour_prefix_tags(*si, DESC_NOCAP_THE).c_str());

                    if (yesno(info, true, 0, false))
                    {
                        corpse = si->index();
                        break;
                    }
                }
            }
    }

    if (corpse == -1)
    {
        canned_msg(MSG_OK);
        return (false);
    }

    potion_type pot_type = POT_WATER;

    switch (mons_corpse_effect(mitm[corpse].plus))
    {
    case CE_CLEAN:
        pot_type = POT_WATER;
        break;

    case CE_CONTAMINATED:
        pot_type = (mons_weight(mitm[corpse].plus) >= 900)
            ? POT_DEGENERATION : POT_CONFUSION;
        break;

    case CE_POISONOUS:
        pot_type = POT_POISON;
        break;

    case CE_MUTAGEN_RANDOM:
    case CE_MUTAGEN_GOOD:   // unused
    case CE_RANDOM:         // unused
        pot_type = POT_MUTATION;
        break;

    case CE_MUTAGEN_BAD:    // unused
    case CE_ROTTEN:         // actually this only occurs via mangling
    case CE_HCL:            // necrophage
        pot_type = POT_DECAY;
        break;

    case CE_NOCORPSE:       // shouldn't occur
    default:
        break;
    }

    switch (mitm[corpse].plus)
    {
    case MONS_RED_WASP:              // paralysis attack
        pot_type = POT_PARALYSIS;
        break;

    case MONS_YELLOW_WASP:           // slowing attack
        pot_type = POT_SLOWING;
        break;

    default:
        break;
    }

    struct monsterentry* smc = get_monster_data(mitm[corpse].plus);

    for (int nattk = 0; nattk < 4; ++nattk)
    {
        if (smc->attack[nattk].flavour == AF_POISON_MEDIUM
            || smc->attack[nattk].flavour == AF_POISON_STRONG
            || smc->attack[nattk].flavour == AF_POISON_STR
            || smc->attack[nattk].flavour == AF_POISON_INT
            || smc->attack[nattk].flavour == AF_POISON_DEX
            || smc->attack[nattk].flavour == AF_POISON_STAT)
        {
            pot_type = POT_STRONG_POISON;
        }
    }

    const bool was_orc = (mons_species(mitm[corpse].plus) == MONS_ORC);

    // We borrow the corpse's object to make our potion.
    mitm[corpse].base_type = OBJ_POTIONS;
    mitm[corpse].sub_type  = pot_type;
    mitm[corpse].quantity  = 1;
    mitm[corpse].plus      = 0;
    mitm[corpse].plus2     = 0;
    mitm[corpse].flags     = 0;
    mitm[corpse].inscription.clear();
    item_colour(mitm[corpse]); // sets special as well

    // Always identify said potion.
    set_ident_type(mitm[corpse], ID_KNOWN_TYPE);

    mprf("You extract %s from the corpse.",
         mitm[corpse].name(DESC_NOCAP_A).c_str());

    // Try to move the potion to the player (for convenience).
    if (move_item_to_player(corpse, 1) != 1)
        mpr("Unfortunately, you can't carry it right now!");

    if (was_orc)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);

    return (true);
}

bool cast_fragmentation(int pow, const dist& spd)
{
    int debris = 0;
    bool explode = false;
    bool hole    = true;
    const char *what = NULL;

    if (!exists_ray(you.pos(), spd.target))
    {
        mpr("There's a wall in the way!");
        return (false);
    }

    //FIXME: If (player typed '>' to attack floor) goto do_terrain;

    bolt beam;

    beam.flavour     = BEAM_FRAG;
    beam.glyph       = dchar_glyph(DCHAR_FIRED_BURST);
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
            // fizzle (since we don't actually explode wood golems). - bwr
            explode         = false;
            beam.damage.num = 2;
            _player_hurt_monster(*mon, beam.damage.roll(),
                                 BEAM_DISINTEGRATION);
            break;

        case MONS_IRON_GOLEM:
        case MONS_METAL_GARGOYLE:
            explode         = true;
            beam.name       = "blast of metal fragments";
            beam.colour     = CYAN;
            beam.damage.num = 4;
            if (_player_hurt_monster(*mon, beam.damage.roll(),
                                     BEAM_DISINTEGRATION))
                beam.damage.num += 2;
            break;

        case MONS_CLAY_GOLEM:   // Assume baked clay and not wet loam.
        case MONS_STONE_GOLEM:
        case MONS_EARTH_ELEMENTAL:
        case MONS_GARGOYLE:
        case MONS_STATUE:
            explode         = true;
            beam.ex_size    = 2;
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = 3;
            if (_player_hurt_monster(*mon, beam.damage.roll(),
                                     BEAM_DISINTEGRATION))
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

                if (_player_hurt_monster(*mon, statue_damage,
                                         BEAM_DISINTEGRATION))
                    beam.damage.num += 2;
            }
            break;

        case MONS_CRYSTAL_GOLEM:
            explode         = true;
            beam.ex_size    = 2;
            beam.name       = "blast of crystal shards";
            beam.colour     = WHITE;
            beam.damage.num = 4;
            if (_player_hurt_monster(*mon, beam.damage.roll(),
                                     BEAM_DISINTEGRATION))
                beam.damage.num += 2;
            break;

        default:
            if (mon->is_icy()) // blast of ice
            {
                explode         = true;
                beam.name       = "icy blast";
                beam.colour     = WHITE;
                beam.damage.num = 2;
                beam.flavour    = BEAM_ICE;
                if (_player_hurt_monster(*mon, beam.damage.roll()),
                                         BEAM_DISINTEGRATION)
                    beam.damage.num++;
                break;
            }
            else if (mon->is_skeletal()
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
                      if (_player_hurt_monster(*mon, beam.damage.roll(),
                                               BEAM_DISINTEGRATION))
                          beam.damage.num += 2;
                  }
                  goto all_done; // i.e., no "Foo Explodes!"
            }
            else
            {
                const bool petrifying = mon->petrifying();
                const bool petrified = mon->petrified() && !petrifying;

                // Petrifying or petrified monsters can be exploded.
                if (petrifying || petrified)
                {
                    explode         = true;
                    beam.ex_size    = petrifying ? 1 : 2;
                    beam.name       = "blast of petrified fragments";
                    beam.colour     = mons_class_colour(mon->type);
                    beam.damage.num = petrifying ? 2 : 3;
                    if (_player_hurt_monster(*mon, beam.damage.roll(),
                                             BEAM_DISINTEGRATION))
                        beam.damage.num++;
                    break;
                }
            }

            // Mark that a monster was targeted.
            beam.damage.num = 1;

            // Yes, this spell does lousy damage if the monster isn't
            // susceptible. - bwr
            _player_hurt_monster(*mon, roll_dice(1, 5 + pow / 25),
                                 BEAM_DISINTEGRATION);
            goto do_terrain;
        }

        mprf("%s shatters!", name_cap_the.c_str());
        goto all_done;
    }

    for (stack_iterator si(spd.target, true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES)
        {
            std::string nm = si->name(DESC_CAP_THE);
            if (si->sub_type == CORPSE_BODY)
            {
                if (!explode_corpse(*si, spd.target))
                {
                    mprf("%s seems to be exceptionally well connected.",
                         nm.c_str());

                    goto all_done;
                }
            }

            mprf("%s explodes!", nm.c_str());
            destroy_item(si.link());
            // si invalid now!
            goto all_done;
        }
    }

    if (env.markers.property_at(spd.target, MAT_ANY,
                                "veto_fragmentation") == "veto")
    {
        mprf("%s seems to be unnaturally hard.",
             feature_description(spd.target, false, DESC_CAP_THE, false).c_str());
        canned_msg(MSG_SPELL_FIZZLES);
        return (true);
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
            set_terrain_changed(spd.target);
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
            set_terrain_changed(spd.target);
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
            set_terrain_changed(spd.target);
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
    case DNGN_DETECTED_SECRET_DOOR:
        // Doors always blow up, stone arches never do (would cause problems).
        grd(spd.target) = DNGN_FLOOR;
        set_terrain_changed(spd.target);

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

    if (cell_is_solid(target.target))
    {
        mpr("You can't shoot at gazebos.");
        return (false);
    }

    // Can't use portal through walls. (That'd be just too cheap!)
    if (you.trans_wall_blocking( target.target ))
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
    if (you.trans_wall_blocking(where))
    {
        mpr("Something is in the way.");
        return (false);
    }

    // Letting mostly-melee characters spam apport after every Shoals
    // fight seems like it has too much grinding potential.  We could
    // weaken this for high power.
    if (grd(where) == DNGN_DEEP_WATER || grd(where) == DNGN_LAVA)
    {
        mpr("The density of the terrain blocks your spell.");
        return (false);
    }

    // Let's look at the top item in that square...
    // And don't allow apporting from shop inventories.
    const int item_idx = igrd(where);
    if (item_idx == NON_ITEM || !in_bounds(where))
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

    // Protect the player from destroying the item.
    if (feat_destroys_item(grd(you.pos()), item))
    {
        mpr( "That would be silly while over this terrain!" );
        return (false);
    }

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
    const bool big     = wielding_rocks();
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
            you.increase_duration(DUR_CONDENSATION_SHIELD,
                                  5 + roll_dice(2,3), 30);
        }
        else
        {
            mpr("A crackling disc of dense vapour forms in the air!");
            you.increase_duration(DUR_CONDENSATION_SHIELD,
                                  10 + roll_dice(2, pow / 5), 30);
            you.redraw_armour_class = true;
        }
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
    else
        mpr("Your divine shield is renewed.");

    you.redraw_armour_class = true;

    // duration of complete shield bonus from 35 to 80 turns
    you.set_duration(DUR_DIVINE_SHIELD,
                     35 + (you.skills[SK_INVOCATIONS] * 4) / 3);

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
    for (int i = 0; i < (pow*pow) / 500 + 1; ++i)
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

        if (!you.see_cell_no_trans(target))
            continue;

        found = true;
        break;
    }

    if (!found)
        return(0);

    coord_def origin = you.pos();
    int res = move_player_to_grid(target, false, true, true);

    if (res)
    {
        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), KC_YOU);
    }

    return res;
}

int cast_semi_controlled_blink(int pow)
{
    int result = apply_one_neighbouring_square(_quadrant_blink, pow);

    // Controlled blink causes glowing.
    if (result)
        contaminate_player(1, true);

    return (result);
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

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);
}

bool do_slow_monster(monsters* mon, kill_category whose_kill)
{
    // Try to remove haste, if monster is hasted.
    if (mon->del_ench(ENCH_HASTE, true))
    {
        if (simple_monster_message(mon, " is no longer moving quickly."))
            return (true);
    }

    // Not hasted, slow it.
    if (!mon->has_ench(ENCH_SLOW)
        && !mons_is_stationary(mon)
        && mon->add_ench(mon_enchant(ENCH_SLOW, 0, whose_kill)))
    {
        if (!mon->paralysed() && !mon->petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return (true);
        }
    }

    return (false);
}

/*
 *  File:       monstuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <8>      7 Aug  2001   MV      Inteligent monsters now pick up gold
 *      <7>     26 Mar  2001   GDL     Fixed monster reaching
 *      <6>     13 Mar  2001   GDL     Rewrite of monster AI
 *      <5>     31 July 2000   GDL     More Manticore fixes.
 *      <4>     29 July 2000   JDJ     Fixed a bunch of places in handle_pickup where MSLOT_WEAPON
 *                                     was being erroneously used.
 *      <3>     25 July 2000   GDL     Fixed Manticores
 *      <2>     11/23/99       LRH     Upgraded monster AI
 *      <1>     -/--/--        LRH     Created
 */

#include "AppHdr.h"
#include "monstuff.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "fight.h"
#include "files.h"
#include "ghost.h"
#include "hiscores.h"
#include "it_use2.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "Kills.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "spells2.h"
#include "spells4.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "stash.h"
#include "xom.h"

static bool _wounded_damaged(int wound_class);
static bool _handle_special_ability(monsters *monster, bolt & beem);
static bool _handle_pickup(monsters *monster);
static void _handle_behaviour(monsters *monster);
static void _set_nearest_monster_foe(monsters *monster);
static void _mons_in_cloud(monsters *monster);
static bool _monster_move(monsters *monster);
static bool _plant_spit(monsters *monster, bolt &pbolt);
static spell_type _map_wand_to_mspell(int wand_type);

// [dshaligram] Doesn't need to be extern.
static int mmov_x, mmov_y;

static int compass_x[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
static int compass_y[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };

static bool immobile_monster[MAX_MONSTERS];

#define FAR_AWAY    1000000         // used in monster_move()

// This function creates an artificial item to represent a mimic's appearance.
// Eventually, mimics could be redone to be more like dancing weapons...
// there'd only be one type and it would look like the item it carries. -- bwr
void get_mimic_item( const monsters *mimic, item_def &item )
{
    ASSERT( mimic != NULL && mons_is_mimic( mimic->type ) );

    item.base_type = OBJ_UNASSIGNED;
    item.sub_type = 0;
    item.special = 0;
    item.colour = 0;
    item.flags = 0;
    item.quantity = 1;
    item.plus = 0;
    item.plus2 = 0;
    item.x = mimic->x;
    item.y = mimic->y;
    item.link = NON_ITEM;

    int prop = 127 * mimic->x + 269 * mimic->y;

    switch (mimic->type)
    {
    case MONS_WEAPON_MIMIC:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = (59 * mimic->x + 79 * mimic->y) % NUM_WEAPONS;

        prop %= 100;

        if (prop < 20)
        {
            make_item_randart(item);
            // Override special - this could cause a "bad" (no-properties)
            // randart, but we only need the name anyway. We have to
            // do this in order to get a consistent name for the mimic item.
            item.special = (((mimic->x << 8) + mimic->y) & RANDART_SEED_MASK);
        }
        else if (prop < 50)
            set_equip_desc( item, ISFLAG_GLOWING );
        else if (prop < 80)
            set_equip_desc( item, ISFLAG_RUNED );
        else if (prop < 85)
            set_equip_race( item, ISFLAG_ORCISH );
        else if (prop < 90)
            set_equip_race( item, ISFLAG_DWARVEN );
        else if (prop < 95)
            set_equip_race( item, ISFLAG_ELVEN );
        break;

    case MONS_ARMOUR_MIMIC:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = (59 * mimic->x + 79 * mimic->y) % NUM_ARMOURS;

        prop %= 100;

        if (prop < 20)
        {
            make_item_randart(item);
            // See comment above for randart weapon mimics.
            item.special = (((mimic->x << 8) + mimic->y) & RANDART_SEED_MASK);
        }
        else if (prop < 40)
            set_equip_desc( item, ISFLAG_GLOWING );
        else if (prop < 60)
            set_equip_desc( item, ISFLAG_RUNED );
        else if (prop < 80)
            set_equip_desc( item, ISFLAG_EMBROIDERED_SHINY );
        else if (prop < 85)
            set_equip_race( item, ISFLAG_ORCISH );
        else if (prop < 90)
            set_equip_race( item, ISFLAG_DWARVEN );
        else if (prop < 95)
            set_equip_race( item, ISFLAG_ELVEN );
        break;

    case MONS_SCROLL_MIMIC:
        item.base_type = OBJ_SCROLLS;
        item.sub_type = prop % NUM_SCROLLS;
        break;

    case MONS_POTION_MIMIC:
        item.base_type = OBJ_POTIONS;
        item.sub_type = prop % NUM_POTIONS;
        break;

    case MONS_GOLD_MIMIC:
    default:
        item.base_type = OBJ_GOLD;
        item.quantity = 5 + prop % 30;
        break;
    }

    item_colour( item ); // also sets special vals for scrolls/potions
}

// Sets the colour of a mimic to match its description... should be called
// whenever a mimic is created or teleported. -- bwr
int get_mimic_colour( const monsters *mimic )
{
    ASSERT( mimic != NULL && mons_is_mimic( mimic->type ) );

    if (mimic->type == MONS_SCROLL_MIMIC)
        return (LIGHTGREY);
    else if (mimic->type == MONS_GOLD_MIMIC)
        return (YELLOW);

    item_def  item;
    get_mimic_item( mimic, item );

    return (item.colour);
}

// monster curses a random player inventory item:
bool curse_an_item( bool decay_potions, bool quiet )
{
    int count = 0;
    int item  = ENDOFPACK;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (!is_valid_item( you.inv[i] ))
            continue;

        if (you.inv[i].base_type == OBJ_WEAPONS
            || you.inv[i].base_type == OBJ_ARMOUR
            || you.inv[i].base_type == OBJ_JEWELLERY
            || you.inv[i].base_type == OBJ_POTIONS)
        {
            if (item_cursed( you.inv[i] ))
                continue;

            if (you.inv[i].base_type == OBJ_POTIONS
                && (!decay_potions || you.inv[i].sub_type == POT_DECAY))
            {
                continue;
            }

            // item is valid for cursing, so we'll give it a chance
            count++;
            if (one_chance_in( count ))
                item = i;
        }
    }

    // any item to curse?
    if (item == ENDOFPACK)
        return (false);

    // curse item:
    if (decay_potions && !quiet) // just for mummies
        mpr("You feel nervous for a moment...", MSGCH_MONSTER_SPELL);

    /* don't change you.inv_special (just for fun) */
    if (you.inv[item].base_type == OBJ_POTIONS)
    {
        int amount;
        // decay at least two of the stack
        if (you.inv[item].quantity <= 2)
            amount = you.inv[item].quantity;
        else
            amount = 2 + random2(you.inv[item].quantity - 1);

        split_potions_into_decay(item, amount);

        if (item_value(you.inv[item], true) / amount > 2)
            xom_is_stimulated(32 * amount);
    }
    else
    {
        do_curse_item( you.inv[item], false );
    }

    return (true);
}

static void _monster_drop_ething(monsters *monster,
                                 bool mark_item_origins = false)
{
    const bool hostile_grid = grid_destroys_items(grd(monster->pos()));
    const int midx = (int) monster_index(monster);

    bool destroyed = false;

    /* drop weapons & missiles last (ie on top) so others pick up */
    for (int i = NUM_MONSTER_SLOTS - 1; i >= 0; i--)
    {
        int item = monster->inv[i];

        if (item != NON_ITEM)
        {
            const bool summoned_item =
                testbits(mitm[item].flags, ISFLAG_SUMMONED);
            if (hostile_grid || summoned_item)
            {
                item_was_destroyed(mitm[item], midx);
                destroy_item( item );
                if (!summoned_item)
                    destroyed = true;
            }
            else
            {
                move_item_to_grid( &item, monster->x, monster->y );
                if (mark_item_origins && is_valid_item(mitm[item]))
                {
                    origin_set_monster(mitm[item], monster);
                }
            }

            monster->inv[i] = NON_ITEM;
        }
    }

    if (destroyed)
        mprf(MSGCH_SOUND, grid_item_destruction_message(grd(monster->pos())));
}

static void _place_monster_corpse(const monsters *monster)
{
    int corpse_class = mons_species(monster->type);
    if (corpse_class == MONS_DRACONIAN)
        corpse_class = draco_subspecies(monster);

    if (monster->has_ench(ENCH_SHAPESHIFTER))
        corpse_class = MONS_SHAPESHIFTER;
    else if (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        corpse_class = MONS_GLOWING_SHAPESHIFTER;

    if (mons_weight(corpse_class) == 0 || coinflip())
        return;

    int o = get_item_slot();
    if (o == NON_ITEM)
        return;

    mitm[o].flags     = 0;
    mitm[o].base_type = OBJ_CORPSES;
    mitm[o].plus      = corpse_class;
    mitm[o].plus2     = 0;    // butcher work done
    mitm[o].sub_type  = CORPSE_BODY;
    mitm[o].special   = 210;  // rot time
    mitm[o].colour    = mons_class_colour(corpse_class);
    mitm[o].quantity  = 1;

    if (mitm[o].colour == BLACK)
        mitm[o].colour = monster->colour;

    if (grid_destroys_items(grd[monster->x][monster->y]))
    {
        item_was_destroyed(mitm[o]);
        mitm[o].base_type = OBJ_UNASSIGNED;
        return;
    }

    // Don't care if 'o' is changed, and it shouldn't be (corpses don't stack)
    move_item_to_grid( &o, monster->x, monster->y );
    if (see_grid(monster->x, monster->y))
    {
        tutorial_dissection_reminder(
            mons_corpse_effect(monster->type) != CE_POISONOUS
             || player_res_poison() > 0 );
    }
}                               // end place_monster_corpse()

static void _tutorial_inspect_kill()
{
    if (Options.tutorial_events[TUT_KILLED_MONSTER])
        learned_something_new(TUT_KILLED_MONSTER);
}

#ifdef DGL_MILESTONES
static std::string _milestone_kill_verb(killer_type killer)
{
    return (killer == KILL_RESET? "banished " : "killed ");
}

static void _check_kill_milestone(const monsters *mons,
                                 killer_type killer, int i)
{
    if (mons->type == MONS_PLAYER_GHOST)
    {
        std::string milestone = _milestone_kill_verb(killer) + "the ghost of ";
        milestone += ghost_description(*mons, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    else if (mons_is_unique(mons->type))
    {
        mark_milestone("unique",
                       _milestone_kill_verb(killer)
                       + mons->name(DESC_NOCAP_THE, true)
                       + ".");
    }
}
#endif // DGL_MILESTONES

static void _give_monster_experience( monsters *victim,
                                     int killer_index, int experience,
                                     bool victim_was_born_friendly )
{
    if (invalid_monster_index(killer_index))
        return;

    monsters *mon = &menv[killer_index];
    if (!mon->alive())
        return;

    if ((!victim_was_born_friendly || !mons_friendly(mon))
        && !mons_aligned(killer_index, monster_index(victim)))
    {
        if (mon->gain_exp(experience))
        {
            // Randomly bless the follower who gained experience.
            if ((((you.religion == GOD_SHINING_ONE
                && random2(you.piety) >= piety_breakpoint(0))
                    || (you.religion == GOD_BEOGH
                        && random2(you.piety) >= piety_breakpoint(2)))
                && !player_under_penance())
                && !one_chance_in(3))
            {
                bless_follower(mon);
            }
        }
    }
}

static void _give_adjusted_experience(monsters *monster, killer_type killer,
                                     bool pet_kill, int killer_index,
                                     unsigned int *exp_gain,
                                     unsigned int *avail_gain)
{
    const int experience = exper_value(monster);

    const bool created_friendly =
        testbits(monster->flags, MF_CREATED_FRIENDLY);
    const bool was_neutral = testbits(monster->flags, MF_WAS_NEUTRAL);
    const bool no_xp = monster->has_ench(ENCH_ABJ);
    const bool already_got_half_xp = testbits(monster->flags, MF_GOT_HALF_XP);

    if (created_friendly || was_neutral || no_xp)
        ; // No experience if monster was created friendly or summoned.
    else if (YOU_KILL(killer))
    {
        int old_lev = you.experience_level;
        if (already_got_half_xp)
            gain_exp( experience / 2, exp_gain, avail_gain );
        else
            gain_exp( experience, exp_gain, avail_gain );

        // Give a message for monsters dying out of sight
        if (exp_gain > 0 && !mons_near(monster)
            && you.experience_level == old_lev)
        {
            mpr("You feel a bit more experienced.");
        }
    }
    else if (pet_kill && !already_got_half_xp)
        gain_exp( experience / 2 + 1, exp_gain, avail_gain );

    if (MON_KILL(killer) && !no_xp)
        _give_monster_experience( monster, killer_index, experience,
                                  created_friendly );
}

static bool _is_pet_kill(killer_type killer, int i)
{
    if (!MON_KILL(killer))
        return (false);

    if (i == ANON_FRIENDLY_MONSTER)
        return (true);

    if (i < 0 || i >= MAX_MONSTERS)
        return (false);

    const monsters *m = &menv[i];
    if (mons_friendly(m)) // this includes enslaved monsters
        return (true);

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    return (me.ench == ENCH_CONFUSION
            && (me.who == KC_YOU || me.who == KC_FRIENDLY));
}

// Elyvilon will occasionally (5% chance) protect the life of
// one of your allies.
static bool _ely_protects_ally(monsters *monster)
{
    ASSERT(you.religion == GOD_ELYVILON);

    if (mons_holiness(monster) != MH_NATURAL
         && mons_holiness(monster) != MH_HOLY
        || !mons_friendly(monster)
        || !mons_near(monster)
        || !player_monster_visible(monster) // for simplicity
        || !one_chance_in(20))
    {
        return (false);
    }

    monster->hit_points = 1;
    snprintf(info, INFO_SIZE, " protects %s%s from harm!%s",
             mons_is_unique(monster->type) ? "" : "your ",
             monster->name(DESC_PLAIN).c_str(),
             coinflip() ? "" : "  You feel responsible.");
    simple_god_message(info);
    lose_piety(1);

    return (true);
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heals_monster(monsters *monster, killer_type killer, int i)
{
    ASSERT(you.religion != GOD_ELYVILON);
    god_type god = GOD_ELYVILON;

    if (!you.penance[god] || !is_evil_god(you.religion))
        return false;

    const int ely_penance = you.penance[god];

    if (mons_friendly(monster) || !one_chance_in(10))
        return (false);

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monsters *mon = &menv[i];
        if (!mons_friendly(mon) || !one_chance_in(3))
            return (false);

        if (!mons_near(monster))
            return (false);
    }
    else if (!YOU_KILL(killer))
        return (false);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "monster hp: %d, max hp: %d",
         monster->hit_points, monster->max_hit_points);
#endif

    monster->hit_points = std::min(1 + random2(ely_penance/3),
                                   monster->max_hit_points);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "new hp: %d, ely penance: %d",
         monster->hit_points, ely_penance);
#endif

    snprintf(info, INFO_SIZE, "%s heals %s%s",
             god_name(god, false).c_str(),
             monster->name(DESC_NOCAP_THE).c_str(),
             monster->hit_points * 2 <= monster->max_hit_points ? "." : "!");

    god_speaks(god, info);
    dec_penance(god, 1 + random2(monster->hit_points/2));

    return (true);
}

static bool _monster_avoided_death(monsters *monster, killer_type killer, int i)
{
    if (monster->hit_points < -25
        || monster->hit_points < -monster->max_hit_points
        || monster->max_hit_points <= 0
        || monster->hit_dice < 1)
    {
        return (false);
    }

    // Elyvilon specials
    if (you.religion == GOD_ELYVILON && _ely_protects_ally(monster))
        return (true);

    if (you.religion != GOD_ELYVILON && _ely_heals_monster(monster, killer, i))
        return (true);

    // Beogh special
    bool convert = false;

    if (you.religion == GOD_BEOGH
        && mons_species(monster->type) == MONS_ORC
        && !player_under_penance() && you.piety >= piety_breakpoint(2)
        && mons_near(monster))
    {
        if (YOU_KILL(killer))
            convert = true;
        else if (MON_KILL(killer) && !invalid_monster_index(i))
        {
            monsters *mon = &menv[i];
            if (is_orcish_follower(mon) && !one_chance_in(3))
                convert = true;
        }
    }

    // Orcs may convert to Beogh under threat of death, either from you
    // or, less often, your followers.  In both cases, the checks are
    // made against your stats.  You're the potential messiah, after
    // all.
    if (convert)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Death convert attempt on %s, HD: %d, "
             "your xl: %d",
             monster->name(DESC_PLAIN).c_str(),
             monster->hit_dice,
             you.experience_level);
#endif
        if (random2(you.piety) >= piety_breakpoint(0)
            && random2(you.experience_level) >= random2(monster->hit_dice)
            // Bias beaten-up-conversion towards the stronger orcs.
            && random2(monster->hit_dice) > 2)
        {
            beogh_convert_orc(monster, true, MON_KILL(killer));
            return (true);
        }
    }

    return (false);
}

static bool _is_mons_mutator_or_rotter(monsters *mons)
{
    if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER, ENCH_SHAPESHIFTER))
        return true;

    if (mons->has_spell(SPELL_POLYMORPH_OTHER))
        return true;

    const int attk_flavour = mons_attack_spec(mons, 0).flavour;
    return (attk_flavour == AF_MUTATE || attk_flavour == AF_ROT);
}

static bool _slime_pit_unlock(bool silent)
{
    unset_level_flags(LFLAG_NO_TELE_CONTROL, silent);

    bool in_los = false;
    if (!silent)
    {
        for (int x = 0; x < GXM && !in_los; ++x)
            for (int y = 0; y < GYM; ++y)
                if (grd[x][y] == DNGN_STONE_WALL
                    && see_grid(x, y))
                {
                    in_los = true;
                    break;
                }
    }

    replace_area_wrapper(DNGN_STONE_WALL, DNGN_CLEAR_ROCK_WALL);

    if (!silent)
    {
        if (in_los)
            mpr("Suddenly, all colour oozes out of the stone walls.",
                MSGCH_MONSTER_ENCHANT);
        else
            mpr("You feel a strange vibration for a moment.",
                MSGCH_MONSTER_ENCHANT);
    }

    return (true);
}

static bool _slime_pit_unlock_offlevel()
{
    return _slime_pit_unlock(true);
}
static bool _slime_pit_unlock_onlevel()
{
    return _slime_pit_unlock(false);
}

static void _fire_monster_death_event(monsters *monster,
                                      killer_type killer,
                                      int i)
{
    // Banished monsters aren't technically dead, so no death event
    // for them.
    if (killer != KILL_RESET)
    {
        dungeon_events.fire_event(
            dgn_event(DET_MONSTER_DIED, monster->pos(), 0,
                      monster_index(monster), killer));

        if (monster->type == MONS_ROYAL_JELLY)
        {
            const level_id target(BRANCH_SLIME_PITS, 6);
            if (is_existing_level(target))
            {
                apply_to_level(
                    target,
                    true,
                    target == level_id::current()?
                    _slime_pit_unlock_onlevel : _slime_pit_unlock_offlevel );
            }
        }
    }
}

void monster_die(monsters *monster, killer_type killer, int i, bool silent)
{
    if (monster->type == -1)
        return;

    if (!silent && _monster_avoided_death(monster, killer, i))
        return;

    if (mons_is_caught(monster))
        mons_clear_trapping_net(monster);

    // update list of monsters beholding player
    update_beholders(monster, true);

    const int monster_killed = monster_index(monster);
    bool death_message
        = !silent && mons_near(monster) && player_monster_visible(monster);
    bool in_transit = false;
    const bool hard_reset = testbits(monster->flags, MF_HARD_RESET);
    bool drop_items = !hard_reset;
    const bool gives_xp = !monster->has_ench(ENCH_ABJ);

#ifdef DGL_MILESTONES
    _check_kill_milestone(monster, killer, i);
#endif

    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == i)
    {
        const mon_enchant me = monster->get_ench(ENCH_CONFUSION);
        if (me.ench == ENCH_CONFUSION && me.who == KC_YOU)
            killer = KILL_YOU_CONF;
    }
    else if (MON_KILL(killer) && monster->has_ench(ENCH_CHARM))
        killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)

    // Take note!
    if (killer != KILL_RESET && killer != KILL_DISMISSED)
    {
        if ( MONST_INTERESTING(monster)
             // XXX yucky hack
             || monster->type == MONS_PLAYER_GHOST
             || monster->type == MONS_PANDEMONIUM_DEMON )
        {
            take_note(Note(NOTE_KILL_MONSTER,
                           monster->type, mons_friendly(monster),
                           monster->name(DESC_NOCAP_A, true).c_str()));
        }
    }

    // From time to time Trog gives you a little bonus
    if (killer == KILL_YOU && you.duration[DUR_BERSERKER])
    {
        if (you.religion == GOD_TROG
            && (!player_under_penance() && you.piety > random2(1000)))
        {
            int bonus = 3 + random2avg( 10, 2 );

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player( bonus );

            mpr( "You feel the power of Trog in you as your rage grows.",
                 MSGCH_GOD, GOD_TROG );
        }
        else if (wearing_amulet( AMU_RAGE ) && one_chance_in(30))
        {
            int bonus = 2 + random2(4);

            you.duration[DUR_BERSERKER] += bonus;
            you.duration[DUR_MIGHT] += bonus;
            haste_player( bonus );

            mpr( "Your amulet glows a violent red." );
        }
    }

    if (you.prev_targ == monster_killed)
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }

    if (killer == KILL_YOU)
        crawl_state.cancel_cmd_repeat();

    const bool pet_kill = _is_pet_kill(killer, i);

    if (monster->type == MONS_GIANT_SPORE
        || monster->type == MONS_BALL_LIGHTNING)
    {
        if (monster->hit_points < 1 && monster->hit_points > -15)
            return;
    }
    else if (monster->type == MONS_FIRE_VORTEX
             || monster->type == MONS_SPATIAL_VORTEX)
    {
        if (!silent)
        {
            simple_monster_message( monster, " dissipates!",
                                    MSGCH_MONSTER_DAMAGE, MDAM_DEAD );
            death_message = false;
        }

        if (monster->type == MONS_FIRE_VORTEX)
        {
            place_cloud(CLOUD_FIRE, monster->x, monster->y, 2 + random2(4),
                        monster->kill_alignment());
        }
    }
    else if (monster->type == MONS_SIMULACRUM_SMALL
             || monster->type == MONS_SIMULACRUM_LARGE)
    {
        if (!silent)
        {
            simple_monster_message( monster, " vapourises!",
                                    MSGCH_MONSTER_DAMAGE,  MDAM_DEAD );
            death_message = false;
        }

        place_cloud(CLOUD_COLD, monster->x, monster->y, 2 + random2(4),
                    monster->kill_alignment());
    }
    else if (monster->type == MONS_DANCING_WEAPON)
    {
        if (!silent)
        {
            if (!hard_reset)
            {
                simple_monster_message( monster, " falls from the air.",
                                        MSGCH_MONSTER_DAMAGE, MDAM_DEAD );
                death_message = false;
            }
        }

        if (!hard_reset)
        {
            if (killer == KILL_RESET)
                killer = KILL_DISMISSED;
        }
    }

    switch (killer)
    {
        case KILL_YOU:          /* You kill in combat. */
        case KILL_YOU_MISSILE:  /* You kill by missile or beam. */
        case KILL_YOU_CONF:     /* You kill by confusion */
        {
            const bool created_friendly =
                testbits(monster->flags, MF_CREATED_FRIENDLY);
            const bool was_neutral =
                testbits(monster->flags, MF_WAS_NEUTRAL);

            if (death_message)
            {
                mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                     _wounded_damaged(monster->type) ? "destroy" : "kill",
                     monster->name(DESC_NOCAP_THE).c_str());

                if ((created_friendly || was_neutral) && gives_xp)
                    mpr("That felt strangely unrewarding.");
            }

            // killing triggers tutorial lesson
            _tutorial_inspect_kill();

            // prevent summoned creatures from being done_good kills,
            if (!created_friendly && gives_xp)
            {
                if (mons_holiness(monster) == MH_NATURAL)
                    did_god_conduct(DID_KILL_LIVING,
                                    monster->hit_dice, true, monster);

                if (mons_holiness(monster) == MH_UNDEAD)
                    did_god_conduct(DID_KILL_UNDEAD,
                                    monster->hit_dice, true, monster);

                if (mons_holiness(monster) == MH_DEMONIC)
                    did_god_conduct(DID_KILL_DEMON,
                                    monster->hit_dice, true, monster);

                if (mons_class_flag(monster->type, M_EVIL)
                    && mons_holiness(monster) == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_NATURAL_EVIL,
                                    monster->hit_dice, true, monster);
                }

                if (_is_mons_mutator_or_rotter(monster))
                    did_god_conduct(DID_KILL_MUTATOR_OR_ROTTER,
                                    monster->hit_dice, true, monster);

                // jmf: Trog hates wizards
                if (mons_is_magic_user(monster))
                    did_god_conduct(DID_KILL_WIZARD,
                                    monster->hit_dice, true, monster);

                // Beogh hates priests of other gods.
                if (mons_class_flag(monster->type, M_PRIEST))
                    did_god_conduct(DID_KILL_PRIEST,
                                    monster->hit_dice, true, monster);

                // Holy beings take their gear with them when they die.
                if (mons_is_holy(monster))
                {
                    did_god_conduct(DID_KILL_HOLY, monster->hit_dice,
                                    true, monster);
                    drop_items = false;
                }
            }

            // Divine health and mp restoration doesn't happen when killing
            // born-friendly monsters. The mutation still applies, however.
            if (player_mutation_level(MUT_DEATH_STRENGTH)
                || (!created_friendly && gives_xp
                    && (you.religion == GOD_MAKHLEB
                        || you.religion == GOD_SHINING_ONE
                           && mons_is_evil_or_unholy(monster))
                    && (!player_under_penance() &&
                        random2(you.piety) >= piety_breakpoint(0))))
            {
                if (you.hp < you.hp_max)
                {
                    mpr("You feel a little better.");
                    inc_hp(monster->hit_dice + random2(monster->hit_dice),
                           false);
                }
            }

            if (!created_friendly && gives_xp
                && (you.religion == GOD_MAKHLEB || you.religion == GOD_VEHUMET
                    || you.religion == GOD_SHINING_ONE
                       && mons_is_evil_or_unholy(monster))
                && (!player_under_penance()
                    && random2(you.piety) >= piety_breakpoint(0)))
            {
                if (you.magic_points < you.max_magic_points)
                {
                    mpr("You feel your power returning.");
                    inc_mp( 1 + random2(monster->hit_dice / 2), false );
                }
            }

            // Randomly bless a follower.
            if (!created_friendly && gives_xp
                && (((you.religion == GOD_SHINING_ONE
                    && mons_is_evil_or_unholy(monster)
                    && random2(you.piety) >= piety_breakpoint(0))
                        || (you.religion == GOD_BEOGH
                            && mons_holiness(monster) == MH_NATURAL
                            && random2(you.piety) >= piety_breakpoint(2)))
                    && !player_under_penance()))
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL]
                && gives_xp
                && mons_holiness(monster) == MH_NATURAL
                && mons_weight(mons_species(monster->type)))
            {
                if (create_monster( MONS_SPECTRAL_THING, 0, BEH_FRIENDLY,
                                    monster->x, monster->y, you.pet_target,
                                    mons_species(monster->type)) != -1)
                {
                    if (death_message)
                        mpr("A glowing mist starts to gather...");
                }
            }
            break;
        }

        case KILL_MON:          /* Monster kills in combat */
        case KILL_MON_MISSILE:  /* Monster kills by missile or beam */
            if (!silent)
                simple_monster_message(monster,
                                       _wounded_damaged(monster->type) ?
                                       " is destroyed!" : " dies!",
                                       MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);

            // no piety loss if god gifts killed by other monsters
            if (mons_friendly(monster) && !testbits(monster->flags,MF_GOD_GIFT))
                did_god_conduct(DID_FRIEND_DIES, 1 + (monster->hit_dice / 2),
                                true, monster);

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills. Only
            // affects creatures which were friendly when summoned.
            if (!testbits(monster->flags, MF_CREATED_FRIENDLY) && gives_xp
                && pet_kill)
            {
                bool notice = false;
                const bool anon = (i == ANON_FRIENDLY_MONSTER);

                const mon_holy_type targ_holy  = mons_holiness(monster),
                    attacker_holy = anon? MH_NATURAL : mons_holiness(&menv[i]);

                if (attacker_holy == MH_UNDEAD)
                {
                    if (targ_holy == MH_NATURAL)
                        notice |=
                            did_god_conduct(DID_LIVING_KILLED_BY_UNDEAD_SLAVE,
                                        monster->hit_dice);
                }
                else if (you.religion == GOD_VEHUMET
                         || you.religion == GOD_MAKHLEB
                         || you.religion == GOD_SHINING_ONE
                         || (!anon && testbits(menv[i].flags, MF_GOD_GIFT)))
                {
                    // Yes, we are splitting undead pets from the others
                    // as a way to focus Necromancy vs Summoning (ignoring
                    // Summon Wraith here)... at least we're being nice and
                    // putting the natural creature Summons together with
                    // the Demon ones.  Note that Vehumet gets a free
                    // pass here since those followers are assumed to
                    // come from Summoning spells...  the others are
                    // from invocations (Zin, TSO, Makh, Kiku). -- bwr

                    if (targ_holy == MH_NATURAL)
                    {
                        notice |= did_god_conduct( DID_LIVING_KILLED_BY_SERVANT,
                                                   monster->hit_dice );

                        if (mons_class_flag( monster->type, M_EVIL ))
                        {
                            notice |=
                                did_god_conduct(
                                        DID_NATURAL_EVIL_KILLED_BY_SERVANT,
                                        monster->hit_dice );
                        }
                    }
                    else if (targ_holy == MH_DEMONIC)
                    {
                        notice |= did_god_conduct( DID_DEMON_KILLED_BY_SERVANT,
                                                   monster->hit_dice );
                    }
                    else if (targ_holy == MH_UNDEAD)
                    {
                        notice |= did_god_conduct( DID_UNDEAD_KILLED_BY_SERVANT,
                                                   monster->hit_dice );
                    }
                }

                // Holy kills are always noticed.
                if (targ_holy == MH_HOLY)
                {
                    notice |= did_god_conduct( DID_HOLY_KILLED_BY_SERVANT,
                                               monster->hit_dice );
                }

                if (you.religion == GOD_VEHUMET
                    && notice
                    && (!player_under_penance()
                        && random2(you.piety) >= piety_breakpoint(0)))
                {
                    // Vehumet - only for non-undead servants (coding
                    // convenience, no real reason except that Vehumet
                    // prefers demons)
                    if (you.magic_points < you.max_magic_points)
                    {
                        mpr("You feel your power returning.");
                        inc_mp( 1 + random2(monster->hit_dice / 2), false );
                    }
                }

                if (you.religion == GOD_SHINING_ONE
                        && mons_is_evil_or_unholy(monster)
                    && (!player_under_penance()
                        && random2(you.piety) >= piety_breakpoint(0))
                    && !invalid_monster_index(i))
                {
                    monsters *mon = &menv[i];

                    // Randomly bless the follower who killed.
                    if (!one_chance_in(3) && bless_follower(mon))
                        break;

                    if (mon->alive() && mon->hit_points < mon->max_hit_points)
                    {
                        simple_monster_message(mon, " looks invigorated.");
                        heal_monster( mon, 1 + random2(monster->hit_dice / 4),
                                      false );
                    }
                }

                // Randomly bless the follower who killed.
                if (((you.religion == GOD_BEOGH
                    && mons_holiness(monster) == MH_NATURAL
                    && random2(you.piety) >= piety_breakpoint(2))
                    && !player_under_penance())
                    && !one_chance_in(3)
                    && !invalid_monster_index(i))
                {
                    monsters *mon = &menv[i];

                    bless_follower(mon);
                }

            }
            break;

        /* Monster killed by trap/inanimate thing/itself/poison not from you */
        case KILL_MISC:
            if (!silent)
                simple_monster_message(monster,
                                       _wounded_damaged(monster->type) ?
                                       " is destroyed!" : " dies!",
                                       MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            break;

        case KILL_RESET:
        /* Monster doesn't die, just goes back to wherever it came from
           This must only be called by monsters running out of time (or
           abjuration), because it uses the beam variables! Or does it??? */
            if (!silent)
                simple_monster_message( monster,
                                        " disappears in a puff of smoke!" );

            place_cloud( random_smoke_type(),
                         monster->x, monster->y, 1 + random2(3),
                         monster->kill_alignment() );

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (!monster->needs_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons_is_summoned(monster))
                    monster->destroy_inventory();
                break;
            }

            // Monster goes to the Abyss.
            monster->flags |= MF_BANISHED;
            monster->set_transit( level_id(LEVEL_ABYSS) );
            in_transit = true;
            monster->destroy_inventory();
            break;

        case KILL_DISMISSED:
            break;

        default:
            monster->destroy_inventory();
            break;
    }

    if (monster->type == MONS_MUMMY)
    {
        if (YOU_KILL(killer) && killer != KILL_YOU_CONF)
            curse_an_item(true);
    }
    else if (monster->type == MONS_GUARDIAN_MUMMY
             || monster->type == MONS_GREATER_MUMMY
             || monster->type == MONS_MUMMY_PRIEST)
    {
        if (YOU_KILL(killer) && killer != KILL_YOU_CONF)
        {
            mpr("You feel extremely nervous for a moment...",
                MSGCH_MONSTER_SPELL);

            miscast_effect( SPTYP_NECROMANCY,
                            3 + (monster->type == MONS_GREATER_MUMMY) * 8
                              + (monster->type == MONS_MUMMY_PRIEST) * 5,
                            random2avg(88, 3), 100, "a mummy death curse" );
        }
    }
    else if (monster->type == MONS_BORIS && !in_transit)
    {
        // XXX: actual blood curse effect for Boris? -- bwr

        if (one_chance_in(5))
            mons_speaks( monster );
        else
        {
            // Provide the player with an ingame clue to Boris' return.  -- bwr
            const int tmp = random2(6);
            simple_monster_message( monster,
                    (tmp == 0) ? " says, \"You haven't seen the last of me!\"" :
                    (tmp == 1) ? " says, \"I'll get you next time!\"" :
                    (tmp == 2) ? " says, \"This isn't over yet!\"" :
                    (tmp == 3) ? " says, \"I'll be back!\"" :
                    (tmp == 4) ? " says, \"This isn't the end, it's only just beginning!\"" :
                    (tmp == 5) ? " says, \"Kill me?  I think not!\""
                               : " says, \"You cannot defeat me so easily!\"",
                                    MSGCH_TALK );
        }

        // Now that Boris is dead, he's a valid target for monster
        // creation again. -- bwr
        you.unique_creatures[ monster->type ] = false;
    }

    if (killer != KILL_RESET && killer != KILL_DISMISSED)
    {
        you.kills->record_kill(monster, killer, pet_kill);

        kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)?                                          KC_FRIENDLY :
                                                                 KC_OTHER;

        unsigned int exp_gain = 0, avail_gain = 0;
        _give_adjusted_experience(monster, killer, pet_kill, i,
                                  &exp_gain, &avail_gain);

        PlaceInfo& curr_PlaceInfo = you.get_place_info();
        PlaceInfo  delta;

        delta.mon_kill_num[kc]++;
        delta.mon_kill_exp       += exp_gain;
        delta.mon_kill_exp_avail += avail_gain;

        you.global_info += delta;
        you.global_info.assert_validity();

        curr_PlaceInfo += delta;
        curr_PlaceInfo.assert_validity();

        if (monster->has_ench(ENCH_ABJ))
        {
            if (mons_weight(mons_species(monster->type)))
            {
                if (monster->type == MONS_SIMULACRUM_SMALL
                    || monster->type == MONS_SIMULACRUM_LARGE)
                {
                    simple_monster_message( monster, " vapourises!" );

                    place_cloud( CLOUD_COLD, monster->x, monster->y,
                                 1 + random2(3), monster->kill_alignment() );
                }
                else
                {
                    simple_monster_message(monster,
                                "'s corpse disappears in a puff of smoke!");

                    place_cloud( random_smoke_type(),
                                 monster->x, monster->y, 1 + random2(3),
                                 monster->kill_alignment() );
                }
            }
        }
        else
        {
            // have to add case for disintegration effect here? {dlb}
            _place_monster_corpse(monster);
        }
    }

    _fire_monster_death_event(monster, killer, i);

    const coord_def mwhere = monster->pos();
    if (drop_items)
        _monster_drop_ething(monster, YOU_KILL(killer) || pet_kill);
    monster_cleanup(monster);

    // Force redraw for monsters that die.
    if (see_grid(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }
}

void monster_cleanup(monsters *monster)
{
    unsigned int monster_killed = monster_index(monster);
    monster->reset();

    for (int dmi = 0; dmi < MAX_MONSTERS; dmi++)
    {
        if (menv[dmi].foe == monster_killed)
            menv[dmi].foe = MHITNOT;
    }

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;
}                               // end monster_cleanup()

static bool _jelly_divide(monsters * parent)
{
    int jex = 0, jey = 0;       // loop variables {dlb}
    bool foundSpot = false;     // to rid code of hideous goto {dlb}
    monsters *child = NULL;     // value determined with loop {dlb}

    if (!mons_class_flag( parent->type, M_SPLITS ) || parent->hit_points == 1)
        return (false);

    // first, find a suitable spot for the child {dlb}:
    for (jex = -1; jex < 3; jex++)
    {
        // loop moves beyond those tiles contiguous to parent {dlb}:
        if (jex > 1)
            return (false);

        for (jey = -1; jey < 2; jey++)
        {
            // 10-50 for now - must take clouds into account:
            if (mgrd[parent->x + jex][parent->y + jey] == NON_MONSTER
                && parent->can_pass_through(parent->x + jex, parent->y + jey)
                && (parent->x + jex != you.x_pos || parent->y + jey != you.y_pos))
            {
                foundSpot = true;
                break;
            }
        }

        if (foundSpot)
            break;
    }                           /* end of for jex */

    int k = 0;                  // must remain outside loop that follows {dlb}

    // now that we have a spot, find a monster slot {dlb}:
    for (k = 0; k < MAX_MONSTERS; k++)
    {
        child = &menv[k];

        if (child->type == -1)
            break;
        else if (k == MAX_MONSTERS - 1)
            return (false);
    }

    // handle impact of split on parent {dlb}:
    parent->max_hit_points /= 2;

    if (parent->hit_points > parent->max_hit_points)
        parent->hit_points = parent->max_hit_points;

    parent->init_experience();
    parent->experience = parent->experience * 3 / 5 + 1;

    // create child {dlb}:
    // this is terribly partial and really requires
    // more thought as to generation ... {dlb}
    *child = *parent;
    child->max_hit_points = child->hit_points;
    child->speed_increment = 70 + random2(5);
    child->x = parent->x + jex;
    child->y = parent->y + jey;

    mgrd[child->x][child->y] = k;

    if (!simple_monster_message(parent, " splits in two!"))
    {
        if (!silenced(parent->x, parent->y) || !silenced(child->x, child->y))
            mpr("You hear a squelching noise.", MSGCH_SOUND);
    }

    return (true);
}                               // end jelly_divde()

// if you're invis and throw/zap whatever, alerts menv to your position
void alert_nearby_monsters(void)
{
    monsters *monster = 0;       // NULL {dlb}

    for (int it = 0; it < MAX_MONSTERS; it++)
    {
        monster = &menv[it];

        // Judging from the above comment, this function isn't
        // intended to wake up monsters, so we're only going to
        // alert monsters that aren't sleeping.  For cases where an
        // event should wake up monsters and alert them, I'd suggest
        // calling noisy() before calling this function. -- bwr
        if (monster->type != -1
            && monster->behaviour != BEH_SLEEP
            && mons_near(monster))
        {
            behaviour_event( monster, ME_ALERT, MHITYOU );
        }
    }
}                               // end alert_nearby_monsters()

static bool _valid_morph( monsters *monster, int new_mclass )
{
    const dungeon_feature_type current_tile = grd[monster->x][monster->y];

    // morph targets are _always_ "base" classes, not derived ones.
    new_mclass = mons_species(new_mclass);

    // [ds] Non-base draconians are much more trouble than their HD
    // suggests.
    if (mons_genus(new_mclass) == MONS_DRACONIAN
        && new_mclass != MONS_DRACONIAN
        && !player_in_branch(BRANCH_HALL_OF_ZOT)
        && !one_chance_in(10))
    {
        return (false);
    }

    /* various inappropriate polymorph targets */
    if (mons_class_holiness( new_mclass ) != mons_holiness( monster )
        || mons_class_flag( new_mclass, M_NO_EXP_GAIN )        // not helpless
        || new_mclass == mons_species( monster->type ) // must be different
        || new_mclass == MONS_PROGRAM_BUG
        || new_mclass == MONS_SHAPESHIFTER
        || new_mclass == MONS_GLOWING_SHAPESHIFTER

        // These require manual setting of mons.number to indicate
        // what they are a skeleton/zombie/simulacrum/spectral thing of,
        // which we currently aren't smart enough to handle.
        || mons_class_is_zombified(new_mclass)

        // These shouldn't happen anyways (demons unaffected + holiness check),
        // but if we ever do have polydemon, these will be needed:
        || new_mclass == MONS_PLAYER_GHOST
        || new_mclass == MONS_PANDEMONIUM_DEMON

        // Other poly-unsuitable things
        || new_mclass == MONS_ROYAL_JELLY
        || new_mclass == MONS_ORB_GUARDIAN
        || new_mclass == MONS_ORANGE_STATUE
        || new_mclass == MONS_SILVER_STATUE
        || new_mclass == MONS_ICE_STATUE
        || (new_mclass >= MONS_GERYON && new_mclass <= MONS_ERESHKIGAL))
    {
        return (false);
    }

    // Determine if the monster is happy on current tile
    return (monster_habitable_grid(new_mclass, current_tile));
}        // end valid_morph()

static bool _is_poly_power_unsuitable(
    poly_power_type power,
    int src_pow,
    int tgt_pow,
    int relax)
{
    switch (power)
    {
    case PPT_LESS:
        return (tgt_pow > src_pow - 3 + (relax * 3) / 2)
            || (power == PPT_LESS && (tgt_pow < src_pow - (relax / 2)));
    case PPT_MORE:
        return (tgt_pow < src_pow + 2 - relax)
            || (power == PPT_MORE && (tgt_pow > src_pow + relax));
    default:
    case PPT_SAME:
        return (tgt_pow < src_pow - relax)
            || (tgt_pow > src_pow + (relax * 3) / 2);
    }
}

/*
 * if targetc == RANDOM_MONSTER then relpower indicates the desired
 * power of the new monster relative to the current monster.
 * Relaxation still takes effect when needed no matter what relpower
 * says.
 */
bool monster_polymorph( monsters *monster, monster_type targetc,
                        poly_power_type power )
{
    std::string str_polymon;
    int source_power, target_power, relax;
    int tries = 1000;

    // Used to be mons_power, but that just returns hit_dice
    // for the monster class.  By using the current hit dice
    // the player gets the opportunity to use draining more
    // effectively against shapeshifters. -- bwr
    source_power = monster->hit_dice;
    relax = 1;

    if (targetc == RANDOM_MONSTER)
    {
        do
        {
            // Pick a monster that's guaranteed happy at this grid
            targetc = random_monster_at_grid(monster->x, monster->y);

            // valid targets are always base classes ([ds] which is unfortunate
            // in that well-populated monster classes will dominate polymorphs)
            targetc = mons_species( targetc );

            target_power = mons_power( targetc );

            if (one_chance_in(200))
                relax++;

            if (relax > 50)
                return (simple_monster_message( monster, " shudders." ));
        }
        while (tries-- && (!_valid_morph( monster, targetc )
                           || _is_poly_power_unsuitable(power, source_power,
                                                        target_power, relax)));
    }

    if (!_valid_morph( monster, targetc ))
        return simple_monster_message(monster, " looks momentarily different.");

    // If old monster is visible to the player, and is interesting,
    // then note why the interesting monster went away.
    if (player_monster_visible(monster) && mons_near(monster)
        && MONST_INTERESTING(monster))
    {
        take_note(Note(NOTE_POLY_MONSTER, monster->type, 0,
                       monster->name(DESC_CAP_A, true).c_str()));
    }

    // messaging: {dlb}
    bool invis = (mons_class_flag( targetc, M_INVIS )
                  || monster->invisible()) && !player_see_invis();

    if (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER, ENCH_SHAPESHIFTER))
        str_polymon = " changes into ";
    else if (targetc == MONS_PULSATING_LUMP)
        str_polymon = " degenerates into ";
    else
        str_polymon = " evaporates and reforms as ";

    if (invis)
        str_polymon += "something you cannot see!";
    else
    {
        str_polymon += mons_type_name(targetc, DESC_NOCAP_A);

        if (targetc == MONS_PULSATING_LUMP)
            str_polymon += " of flesh";

        str_polymon += "!";
    }

    bool player_messaged = simple_monster_message(monster, str_polymon.c_str());

    // Even if the monster transforms from one type that can behold the
    // player into a different type which can also behold the player,
    // the polymorph disrupts the beholding process.  Do this before
    // changing monster->type, since unbeholding can only happen while
    // the monster is still a mermaid.
    update_beholders(monster, true);

    // the actual polymorphing:
    const int old_hp     = monster->hit_points;
    const int old_hp_max = monster->max_hit_points;
    const bool old_mon_shifter = monster->has_ench(ENCH_GLOWING_SHAPESHIFTER,
                                                   ENCH_SHAPESHIFTER);
    const bool old_mon_caught = mons_is_caught(monster);
    const char old_ench_countdown = monster->ench_countdown;

    // deal with mons_sec
    monster->type   = targetc;
    monster->number = MONS_PROGRAM_BUG;

    mon_enchant abj     = monster->get_ench(ENCH_ABJ);
    mon_enchant shifter = monster->get_ench(ENCH_GLOWING_SHAPESHIFTER,
                                            ENCH_SHAPESHIFTER);
    mon_enchant charm   = monster->get_ench(ENCH_CHARM);

    // Note: define_monster() will clear out all enchantments! -- bwr
    define_monster( monster_index(monster) );

    monster->add_ench(abj);
    monster->add_ench(shifter);
    // Only shapeshifters retain the charm enchantment.
    if (old_mon_shifter)
        monster->add_ench(charm);

    monster->ench_countdown = old_ench_countdown;

    if (mons_class_flag( monster->type, M_INVIS ))
        monster->add_ench(ENCH_INVIS);

    if (!player_messaged && mons_near(monster)
        && player_monster_visible(monster))
    {
        mprf("%s appears out of thin air!", monster->name(DESC_CAP_A).c_str());
        player_messaged = true;
    }

    monster->hit_points = monster->max_hit_points
                                * ((old_hp * 100) / old_hp_max) / 100
                                + random2(monster->max_hit_points);

    if (monster->hit_points > monster->max_hit_points)
        monster->hit_points = monster->max_hit_points;

    monster->speed_increment = 67 + random2(6);

    _monster_drop_ething(monster);

    // New monster type might be interesting
    mark_interesting_monst(monster);

    // If new monster is visible to player, then we've seen it
    if (player_monster_visible(monster) && mons_near(monster))
        seen_monster(monster);

    if (old_mon_caught)
    {
        if (monster->body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            int net = get_trapping_net(monster->x, monster->y);
            if (net != NON_ITEM)
                destroy_item(net);

            if (see_grid(monster->x, monster->y))
            {
                if (player_monster_visible(monster))
                {
                    mprf("The net rips apart, and %s comes free!",
                         monster->name(DESC_NOCAP_THE).c_str());
                }
                else
                {
                    mpr("All of a sudden the net rips apart!");
                }
            }
        }
        else if (mons_is_insubstantial(monster->type)
                 || monster->type == MONS_OOZE
                 || monster->type == MONS_PULSATING_LUMP)
        {
            const int net = get_trapping_net(monster->x, monster->y);
            if (net != NON_ITEM)
                remove_item_stationary(mitm[net]);

            if (mons_is_insubstantial(monster->type))
            {
                simple_monster_message(monster,
                                       " drifts right through the net!");
            }
            else
            {
                simple_monster_message(monster,
                                       " oozes right through the net!");
            }
        }
        else
            monster->add_ench(ENCH_HELD);
    }

    return (player_messaged);
}                                        // end monster_polymorph()

bool monster_blink(monsters *monster)
{
    int nx, ny;

    {
        unwind_var<env_show_grid> visible_grid( env.show );
        losight(env.show, grd, monster->x, monster->y, true);

        if (!random_near_space(monster->x, monster->y, nx, ny,
                               false, true))
            return (false);
    }

    if (mons_is_caught(monster))
        mons_clear_trapping_net(monster);

    mgrd[monster->x][monster->y] = NON_MONSTER;

    const coord_def oldplace = monster->pos();
    monster->x = nx;
    monster->y = ny;

    mgrd[nx][ny] = monster_index(monster);

    if (player_monster_visible(monster) && mons_near(monster))
        seen_monster(monster);

    monster->check_redraw(oldplace);
    monster->apply_location_effects();

    return (true);
}

// allow_adjacent:  allow target to be adjacent to origin
// restrict_LOS:    restict target to be within PLAYER line of sight
bool random_near_space(int ox, int oy, int &tx, int &ty, bool allow_adjacent,
                       bool restrict_LOS)
{

    // This might involve ray tracing (via num_feats_between()), so
    // cache results to avoid duplicating ray traces.
    FixedArray<bool, 14, 14> tried;
    tried.init(false);

    // Is the monster on the other side of a tranparent wall?
    bool trans_wall_block  = trans_wall_blocking(ox, oy);
    bool origin_is_player  = (you.pos() == coord_def(ox, oy));
    int  min_walls_between = 0;

    // Skip ray tracing if possible.
    if (trans_wall_block)
        min_walls_between = num_feats_between(ox, oy, you.x_pos, you.y_pos,
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);
    int  tries = 0;
    while (tries++ < 150)
    {
        int dx = random2(14);
        int dy = random2(14);

        tx = ox - 6 + dx;
        ty = oy - 6 + dy;

        // origin is not 'near'
        if (tx == ox && ty == oy)
            continue;

        if (tried[dx][dy])
            continue;

        tried[dx][dy] = true;

        if (!in_bounds(tx, ty)
            || (!see_grid(tx, ty) && restrict_LOS)
            || grd[tx][ty] < DNGN_SHALLOW_WATER
            || mgrd[tx][ty] != NON_MONSTER
            || (tx == you.x_pos && ty == you.y_pos)
            || (!allow_adjacent && distance(ox, oy, tx, ty) <= 2))
            continue;

        if (!trans_wall_block && !origin_is_player)
            return (true);

        // If the monster is on a visible square which is on the other
        // side of one or more translucent from the player, then it
        // can only blink through translucent walls if the end point
        // is either not visible to the player, or there are at least
        // as many translucent walls between the player and the end
        // point as between the player and the start point.  However,
        // monsters can still blink through translucent walls to get
        // away from the player, since in the absence of tranlucent
        // walls monsters can blink to places which are not in either
        // the monster's nor the player's LOS.
        if (!see_grid(tx, ty) && !origin_is_player)
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
        {
            if (see_grid_no_trans(tx, ty))
                return (true);

            continue;
        }

        int walls_passed = num_feats_between(tx, ty, ox, oy,
                                             DNGN_CLEAR_ROCK_WALL,
                                             DNGN_CLEAR_PERMAROCK_WALL);
        if (walls_passed == 0)
            return (true);

        // Player can't randomly pass through translucent walls.
        if (origin_is_player)
            continue;

        int walls_between = num_feats_between(tx, ty, you.x_pos, you.y_pos,
                                              DNGN_CLEAR_ROCK_WALL,
                                              DNGN_CLEAR_PERMAROCK_WALL);

        if (walls_between >= min_walls_between)
            return (true);
    }

    return (false);
}                               // end random_near_space()

static bool _habitat_okay( const monsters *monster, dungeon_feature_type targ )
{
    return (monster_habitable_grid(monster, targ));
}

// This doesn't really swap places, it just sets the monster's
// position equal to the player (the player has to be moved afterwards).
// It also has a slight problem with the fact that if the player is
// levitating over an inhospitable habitat for the monster the monster
// will be put in a place it normally couldn't go (this could be a
// feature because it prevents insta-killing).  In order to prevent
// that little problem, we go looking for a square for the monster
// to "scatter" to instead... and if we can't find one the monster
// just refuses to be swapped (not a bug, this is intentionally
// avoiding the insta-kill).  Another option is to look a bit
// wider for a vaild square (either by a last attempt blink, or
// by looking at a wider radius)...  insta-killing should be a
// last resort in this function (especially since Tome, Dig, and
// Summoning can be used to set up death traps).  If worse comes
// to worse, at least consider making the Swap spell not work
// when the player is over lava or water (if the player wants to
// swap pets to their death, we can let that go). -- bwr
bool swap_places(monsters *monster)
{
    int loc_x = you.x_pos;
    int loc_y = you.y_pos;

    const int mgrid = grd[monster->x][monster->y];

    if (monster->has_ench(ENCH_HELD))
    {
        simple_monster_message(monster, " is held in a net!");
        return (false);
    }

    const bool mon_dest_okay = _habitat_okay( monster, grd[loc_x][loc_y] );
    const bool you_dest_okay =
        !is_grid_dangerous(mgrid)
        || yesno("Do you really want to step there?", false, 'n');

    if (!you_dest_okay)
        return (false);

    bool swap = mon_dest_okay;

    // Choose an appropriate habitat square at random around the target.
    if (!swap)
    {
        int num_found = 0;
        int temp_x, temp_y;

        for (int x = -1; x <= 1; x++)
        {
            temp_x = you.x_pos + x;
            if (temp_x < 0 || temp_x >= GXM)
                continue;

            for (int y = -1; y <= 1; y++)
            {
                if (x == 0 && y == 0)
                    continue;

                temp_y = you.y_pos + y;
                if (temp_y < 0 || temp_y >= GYM)
                    continue;

                if (mgrd[temp_x][temp_y] == NON_MONSTER
                    && _habitat_okay( monster, grd[temp_x][temp_y] ))
                {
                    // Found an appropiate space... check if we
                    // switch the current choice to this one.
                    num_found++;
                    if (one_chance_in(num_found))
                    {
                        loc_x = temp_x;
                        loc_y = temp_y;
                    }
                }
            }
        }

        if (num_found)
            swap = true;
    }

    if (swap)
    {
        mpr("You swap places.");

        mgrd[monster->x][monster->y] = NON_MONSTER;

        monster->x = loc_x;
        monster->y = loc_y;

        mgrd[monster->x][monster->y] = monster_index(monster);
    }
    else
    {
        // Might not be ideal, but it's better than insta-killing
        // the monster... maybe try for a short blink instead? -- bwr
        simple_monster_message( monster, " resists." );
        // FIXME: AI_HIT_MONSTER isn't ideal.
        interrupt_activity( AI_HIT_MONSTER, monster );
    }

    return (swap);
}                               // end swap_places()

void mons_get_damage_level(
    const monsters* monster,
    std::string& desc, mon_dam_level_type& dam_level)
{
    if (monster->hit_points <= monster->max_hit_points / 6)
    {
        desc += "almost ";
        desc += _wounded_damaged(monster->type) ? "destroyed" : "dead";
        dam_level = MDAM_ALMOST_DEAD;
        return;
    }

    if (monster->hit_points <= monster->max_hit_points / 6)
    {
        desc += "horribly ";
        dam_level = MDAM_HORRIBLY_DAMAGED;
    }
    else if (monster->hit_points <= monster->max_hit_points / 3)
    {
        desc += "heavily ";
        dam_level = MDAM_HEAVILY_DAMAGED;
    }
    else if (monster->hit_points <= (monster->max_hit_points * 3) / 4)
    {
        desc += "moderately ";
        dam_level = MDAM_MODERATELY_DAMAGED;
    }
    else if (monster->hit_points < monster->max_hit_points)
    {
        desc += "lightly ";
        dam_level = MDAM_LIGHTLY_DAMAGED;
    }
    else
    {
        desc += "not ";
        dam_level = MDAM_OKAY;
    }

    desc += _wounded_damaged(monster->type) ? "damaged" : "wounded";
}

void print_wounds(const monsters *monster)
{
    if (monster->type == -1)
        return;

    if (monster->hit_points == monster->max_hit_points
        || monster->hit_points < 1)
    {
        return;
    }

    if (monster_descriptor(monster->type, MDSC_NOMSG_WOUNDS))
        return;

    std::string desc;
    mon_dam_level_type dam_level;
    mons_get_damage_level(monster, desc, dam_level);

    desc.insert(0, " is ");
    desc += ".";
    simple_monster_message(monster, desc.c_str(), MSGCH_MONSTER_DAMAGE, dam_level);
}                               // end print_wounds()

// (true == 'damaged') [constructs, undead, etc.]
// and (false == 'wounded') [living creatures, etc.] {dlb}
static bool _wounded_damaged(int monster_type)
{
    // this schema needs to be abstracted into real categories {dlb}:
    const mon_holy_type holy = mons_class_holiness(monster_type);
    if (holy == MH_UNDEAD || holy == MH_NONLIVING || holy == MH_PLANT)
        return (true);

    return (false);
}                               // end _wounded_damaged()

//---------------------------------------------------------------
//
// behaviour_event
//
// 1. Change any of: monster state, foe, and attitude
// 2. Call handle_behaviour to re-evaluate AI state and target x,y
//
//---------------------------------------------------------------
void behaviour_event( monsters *mon, int event, int src,
                      int src_x, int src_y )
{
    bool isSmart          = (mons_intel(mon->type) > I_ANIMAL);
    bool wontAttack       = mons_wont_attack(mon);
    bool sourceWontAttack = false;
    bool setTarget        = false;
    bool breakCharm       = false;

    if (src == MHITYOU)
        sourceWontAttack = true;
    else if (src != MHITNOT)
        sourceWontAttack = mons_wont_attack( &menv[src] );

    switch(event)
    {
    case ME_DISTURB:
        // assumes disturbed by noise...
        if (mon->behaviour == BEH_SLEEP)
            mon->behaviour = BEH_WANDER;

        // A bit of code to make Project Noise actually do
        // something again.  Basically, dumb monsters and
        // monsters who aren't otherwise occupied will at
        // least consider the (apparent) source of the noise
        // interesting for a moment. -- bwr
        if (!isSmart || mon->foe == MHITNOT || mon->behaviour == BEH_WANDER)
        {
            mon->target_x = src_x;
            mon->target_y = src_y;
        }
        break;

    case ME_WHACK:
    case ME_ANNOY:
        // will turn monster against <src>, unless they
        // are BOTH friendly or good neutral and stupid,
        // or else fleeing anyway. Hitting someone over
        // the head, of course, always triggers this code.
        if (event == ME_WHACK
            || ((wontAttack != sourceWontAttack || isSmart)
               && mon->behaviour != BEH_FLEE && mon->behaviour != BEH_PANIC))
        {
            // (plain) plants and fungi cannot flee or fight back
            if (mon->type == MONS_FUNGUS || mon->type == MONS_PLANT)
                return;

            mon->foe = src;

            if (mon->behaviour != BEH_CORNERED)
                mon->behaviour = BEH_SEEK;

            if (src == MHITYOU)
            {
                mon->attitude = ATT_HOSTILE;
                breakCharm    = true;
            }
        }

        // now set target x,y so that monster can whack
        // back (once) at an invisible foe
        if (event == ME_WHACK)
            setTarget = true;
        break;

    case ME_ALERT:
        // will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // it won't turn friends hostile either.
        if (mon->behaviour != BEH_CORNERED && mon->behaviour != BEH_PANIC
            && mon->behaviour != BEH_FLEE)
        {
            mon->behaviour = BEH_SEEK;
        }

        if (mon->foe == MHITNOT)
            mon->foe = src;
        break;

    case ME_SCARE:
        // Berserking monsters don't flee.
        if (mon->has_ench(ENCH_BERSERK))
            break;

        // Neither do wandering mushrooms.
        if (mon->type == MONS_WANDERING_MUSHROOM)
            break;

        mon->foe = src;
        mon->behaviour = BEH_FLEE;
        // assume monsters know where to run from, even
        // if player is invisible.
        setTarget = true;
        if (see_grid(mon->x, mon->y))
            learned_something_new(TUT_FLEEING_MONSTER);
        break;

    case ME_CORNERED:
        if (mon->type == MONS_WANDERING_MUSHROOM)
            break;

        // just set behaviour.. foe doesn't change.
        if (mon->behaviour != BEH_CORNERED && !mon->has_ench(ENCH_FEAR))
            simple_monster_message(mon, " turns to fight!");

        mon->behaviour = BEH_CORNERED;
        break;

    case ME_EVAL:
    default:
        break;
    }

    if (setTarget)
    {
        if (src == MHITYOU)
        {
            mon->target_x = you.x_pos;
            mon->target_y = you.y_pos;
            mon->attitude = ATT_HOSTILE;
        }
        else if (src != MHITNOT)
        {
            mon->target_x = menv[src].x;
            mon->target_y = menv[src].y;
        }
    }

    // now, break charms if appropriate
    if (breakCharm)
        mon->del_ench(ENCH_CHARM);

    // do any resultant foe or state changes
    _handle_behaviour( mon );
}

//---------------------------------------------------------------
//
// handle_behaviour
//
// 1. Evalutates current AI state
// 2. Sets monster targetx,y based on current foe
//
// XXX: Monsters of I_NORMAL or above should select a new target
// if their current target is another monster which is sitting in
// a wall and is immune to most attacks while in a wall, unless
// the monster has a spell or special/nearby ability which isn't
// affected by the wall.
//---------------------------------------------------------------
static void _handle_behaviour(monsters *mon)
{
    bool changed = true;
    bool isFriendly = mons_friendly(mon);
    bool isNeutral  = mons_neutral(mon);
    bool wontAttack = mons_wont_attack(mon);
    bool proxPlayer = mons_near(mon);
    bool proxFoe;
    bool isHurt    = (mon->hit_points <= mon->max_hit_points / 4 - 1);
    bool isHealthy = (mon->hit_points > mon->max_hit_points / 2);
    bool isSmart   = (mons_intel(mon->type) > I_ANIMAL);
    bool isScared  = mon->has_ench(ENCH_FEAR);
    bool isMobile  = !mons_is_stationary(mon);

    // check for confusion -- early out.
    if (mon->has_ench(ENCH_CONFUSION))
    {
        mon->target_x = 10 + random2(GXM - 10);
        mon->target_y = 10 + random2(GYM - 10);
        return;
    }

    // validate current target exists
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        if (menv[mon->foe].type == -1)
            mon->foe = MHITNOT;
    }

    // change proxPlayer depending on invisibility and standing
    // in shallow water
    if (proxPlayer && you.invisible())
    {
        if (!mons_player_visible( mon ))
            proxPlayer = false;

        // must be able to see each other
        if (!see_grid(mon->x, mon->y))
            proxPlayer = false;

        const int intel = mons_intel(mon->type);
        // now, the corollary to that is that sometimes, if a
        // player is right next to a monster, they will 'see'
        if (grid_distance( you.x_pos, you.y_pos, mon->x, mon->y ) == 1
            && one_chance_in(3))
        {
            proxPlayer = true;
        }

        // [dshaligram] Very smart monsters have a chance of cluing in to
        // invisible players in various ways.
        else if (intel == I_NORMAL && one_chance_in(13)
                 || intel == I_HIGH && one_chance_in(6))
        {
            proxPlayer = true;
        }
    }

    // set friendly target, if they don't already have one
    // berserking allies ignore your commands
    if (isFriendly
        && you.pet_target != MHITNOT
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->has_ench(ENCH_BERSERK))
    {
        mon->foe = you.pet_target;
    }

    // instead berserkers attack nearest monsters.
    if (mon->has_ench(ENCH_BERSERK)
        && (mon->foe == MHITNOT || isFriendly && mon->foe == MHITYOU))
    {
        // intelligent monsters prefer to attack the player,
        // even when berserking
        if (!isFriendly && proxPlayer && mons_intel(mon->type) >= I_NORMAL)
        {
            mon->foe = MHITYOU;
        }
        else
        {
            _set_nearest_monster_foe(mon);
        }
    }

    if (isNeutral && mon->foe == MHITNOT)
        _set_nearest_monster_foe(mon);

    // monsters do not attack themselves {dlb}
    if (mon->foe == monster_index(mon))
        mon->foe = MHITNOT;

    // friendly and good neutral monsters do not attack other friendly
    // and good neutral monsters
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        if (wontAttack && mons_wont_attack(&menv[mon->foe]))
            mon->foe = MHITNOT;
    }

    // neutral monsters prefer not to attack players, or other neutrals
    if (isNeutral && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || mons_neutral(&menv[mon->foe])))
    {
        mon->foe = MHITNOT;
    }

    // unfriendly monsters fighting other monsters will usually
    // target the player, if they're healthy
    if (!isFriendly && !isNeutral
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !(mon->has_ench(ENCH_BERSERK)) && isHealthy
        && !one_chance_in(3))
    {
        mon->foe = MHITYOU;
    }

    // validate target again
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        if (menv[mon->foe].type == -1)
            mon->foe = MHITNOT;
    }

    while (changed)
    {
        int foe_x = you.x_pos;
        int foe_y = you.y_pos;

        // evaluate these each time; they may change
        if (mon->foe == MHITNOT)
            proxFoe = false;
        else
        {
            if (mon->foe == MHITYOU)
            {
                foe_x = you.x_pos;
                foe_y = you.y_pos;
                proxFoe = proxPlayer;   // take invis into account
            }
            else
            {
                proxFoe = mons_near(mon, mon->foe);

                if (!mons_monster_visible( mon, &menv[mon->foe] ))
                    proxFoe = false;

                // XXX monsters will rely on player LOS -- GDL
                if (!see_grid(menv[mon->foe].x, menv[mon->foe].y))
                    proxFoe = false;

                foe_x = menv[mon->foe].x;
                foe_y = menv[mon->foe].y;
            }
        }

        // track changes to state; attitude never changes here.
        beh_type new_beh = mon->behaviour;
        unsigned int new_foe = mon->foe;

        // take care of monster state changes
        switch(mon->behaviour)
        {
        case BEH_SLEEP:
            // default sleep state
            mon->target_x = mon->x;
            mon->target_y = mon->y;
            new_foe = MHITNOT;
            break;

        case BEH_SEEK:
            // no foe? then wander or seek the player
            if (mon->foe == MHITNOT)
            {
                if (!proxPlayer || isNeutral)
                    new_beh = BEH_WANDER;
                else
                {
                    new_foe = MHITYOU;
                    mon->target_x = you.x_pos;
                    mon->target_y = you.y_pos;
                }

                break;
            }

            // foe gone out of LOS?
            if (!proxFoe)
            {
                if (isFriendly)
                {
                    new_foe = MHITYOU;
                    mon->target_x = foe_x;
                    mon->target_y = foe_y;
                    break;
                }

                if (mon->foe_memory > 0 && mon->foe != MHITNOT)
                {
                    // if we've arrived at our target x,y
                    // do a stealth check.  If the foe
                    // fails, monster will then start
                    // tracking foe's CURRENT position,
                    // but only for a few moves (smell and
                    // intuition only go so far)

                    if (mon->x == mon->target_x &&
                        mon->y == mon->target_y)
                    {
                        if (mon->foe == MHITYOU)
                        {
                            if (check_awaken(mon))
                            {
                                mon->target_x = you.x_pos;
                                mon->target_y = you.y_pos;
                            }
                            else
                                mon->foe_memory = 1;
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                            {
                                mon->target_x = menv[mon->foe].x;
                                mon->target_y = menv[mon->foe].y;
                            }
                            else
                                mon->foe_memory = 1;
                        }
                    }

                    // either keep chasing, or start
                    // wandering.
                    if (mon->foe_memory < 2)
                    {
                        mon->foe_memory = 0;
                        new_beh = BEH_WANDER;
                    }
                    break;
                }

                // hack: smarter monsters will
                // tend to pursue the player longer.
                int memory = 0;
                switch (mons_intel(monster_index(mon)))
                {
                case I_HIGH:
                    memory = 100 + random2(200);
                    break;
                case I_NORMAL:
                    memory = 50 + random2(100);
                    break;
                case I_ANIMAL:
                case I_INSECT:
                    memory = 25 + random2(75);
                    break;
                case I_PLANT:
                    memory = 10 + random2(50);
                    break;
                }

                mon->foe_memory = memory;
                break;      // from case
            }

            // monster can see foe: continue 'tracking'
            // by updating target x,y
            if (mon->foe == MHITYOU)
            {
                // sometimes, your friends will wander a bit.
                if (isFriendly && one_chance_in(8))
                {
                    mon->target_x = 10 + random2(GXM - 10);
                    mon->target_y = 10 + random2(GYM - 10);
                    mon->foe = MHITNOT;
                    new_beh = BEH_WANDER;
                }
                else
                {
                    mon->target_x = you.x_pos;
                    mon->target_y = you.y_pos;
                }
            }
            else
            {
                mon->target_x = menv[mon->foe].x;
                mon->target_y = menv[mon->foe].y;
            }

            if (isHurt && !isSmart && isMobile)
                new_beh = BEH_FLEE;
            break;

        case BEH_WANDER:
            // is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if (proxFoe && !testbits( mon->flags, MF_BATTY ))
            {
                new_beh = BEH_SEEK;
                break;
            }

            // default wander behaviour
            //
            // XXX: This is really dumb wander behaviour... instead of
            // changing the goal square every turn, better would be to
            // have the monster store a direction and have the monster
            // head in that direction for a while, then shift the
            // direction to the left or right.  We're changing this so
            // wandering monsters at least appear to have some sort of
            // attention span.  -- bwr
            if ((mon->x == mon->target_x && mon->y == mon->target_y)
                || one_chance_in(20)
                || testbits( mon->flags, MF_BATTY ))
            {
                mon->target_x = 10 + random2(GXM - 10);
                mon->target_y = 10 + random2(GYM - 10);
            }

            // during their wanderings, monsters will
            // eventually relax their guard (stupid
            // ones will do so faster, smart monsters
            // have longer memories
            if (!proxFoe && mon->foe != MHITNOT)
            {
                if (one_chance_in( isSmart ? 60 : 20 ))
                    new_foe = MHITNOT;
            }
            break;

        case BEH_FLEE:
            // check for healed
            if (isHealthy && !isScared)
                new_beh = BEH_SEEK;
            // smart monsters flee until they can
            // flee no more...  possible to get a
            // 'CORNERED' event, at which point
            // we can jump back to WANDER if the foe
            // isn't present.

            // XXX: If a monster can move through solid grids, then it
            // should preferentially flee towards the nearest solid grid
            // it can move through.  It will be (mostly) safe as soon as
            // it enters the wall, and even if it isn't, once it moves
            // again it will be on the other side of the wall and likely
            // beyond the reach of the player.

            if (isFriendly)
            {
                // Special-cased below so that it will flee *towards* you
                mon->target_x = you.x_pos;
                mon->target_y = you.y_pos;
            }
            else if (proxFoe)
            {
                // try to flee _from_ the correct position
                mon->target_x = foe_x;
                mon->target_y = foe_y;
            }
            break;

        case BEH_CORNERED:
            if (isHealthy)
                new_beh = BEH_SEEK;

            // foe gone out of LOS?
            if (!proxFoe)
            {
                if ((isFriendly || proxPlayer) && !isNeutral)
                    new_foe = MHITYOU;
                else
                    new_beh = BEH_WANDER;
            }
            else
            {
                mon->target_x = foe_x;
                mon->target_y = foe_y;
            }
            break;

        default:
            return;     // uh oh
        }

        changed = (new_beh != mon->behaviour || new_foe != mon->foe);
        mon->behaviour = new_beh;

        if (mon->foe != new_foe)
            mon->foe_memory = 0;

        mon->foe = new_foe;
    }
}                               // end handle_behaviour()

static bool _mons_check_set_foe(monsters *mon, int x, int y,
                                bool friendly, bool neutral)
{
    if (!in_bounds(x, y))
        return (false);

    if (!friendly && !neutral && x == you.x_pos && y == you.y_pos
        && mons_player_visible(mon))
    {
        mon->foe = MHITYOU;
        return (true);
    }

    if (mgrd[x][y] != NON_MONSTER)
    {
        monsters *foe = &menv[mgrd[x][y]];

        if (foe != mon
            && mons_monster_visible(mon, foe)
            && (mons_friendly(foe) != friendly
                || (neutral && !mons_neutral(foe))))
        {
            mon->foe = mgrd[x][y];
            return (true);
        }
    }
    return (false);
}

// choose nearest monster as a foe
// (used for berserking monsters)
void _set_nearest_monster_foe(monsters *mon)
{
    const bool friendly = mons_friendly(mon);
    const bool neutral  = mons_neutral(mon);

    const int mx = mon->x;
    const int my = mon->y;

    for (int k = 1; k <= LOS_RADIUS; k++)
    {
        for (int x = mx - k; x <= mx + k; ++x)
            if (_mons_check_set_foe(mon, x, my - k, friendly, neutral)
                || _mons_check_set_foe(mon, x, my + k, friendly, neutral))
            {
                return;
            }

        for (int y = my - k + 1; y < my + k; ++y)
            if (_mons_check_set_foe(mon, mx - k, y, friendly, neutral)
                || _mons_check_set_foe(mon, mx + k, y, friendly, neutral))
            {
                return;
            }
    }
}

// The default suitable() function for choose_random_nearby_monster().
bool choose_any_monster(const monsters* mon)
{
    return true;
}

// Find a nearby monster and return its index, including you as a
// possibility with probability weight.  suitable() should return true
// for the type of monster wanted.
int choose_random_nearby_monster(int weight,
                                 bool (*suitable)(const monsters* mon))
{
    int mons_count = weight;
    int result = NON_MONSTER;

    int ystart = you.y_pos - 9, xstart = you.x_pos - 9;
    int yend = you.y_pos + 9, xend = you.x_pos + 9;
    if ( xstart < 0 ) xstart = 0;
    if ( ystart < 0 ) ystart = 0;
    if ( xend >= GXM ) xend = GXM;
    if ( yend >= GYM ) yend = GYM;

    // monster check
    for ( int y = ystart; y < yend; ++y )
        for ( int x = xstart; x < xend; ++x )
            if ( see_grid(x,y) && mgrd[x][y] != NON_MONSTER )
            {
                result = mgrd[x][y];

                if ( suitable(&menv[result]) && one_chance_in(++mons_count) )
                    break;

                result = NON_MONSTER;
            }

    return result;
}

// note that this function *completely* blocks messaging for monsters
// distant or invisible to the player ... look elsewhere for a function
// permitting output of "It" messages for the invisible {dlb}
// Intentionally avoids info and str_pass now. -- bwr
bool simple_monster_message(const monsters *monster, const char *event,
                            msg_channel_type channel, int param,
                            description_level_type descrip)
{

    if (mons_near( monster )
        && (channel == MSGCH_MONSTER_SPELL || player_monster_visible(monster)))
    {
        char buff[INFO_SIZE];

        std::string name = get_unique_monster_name(monster);
        if (name.empty())
            name = monster->name(descrip);

        snprintf( buff, sizeof(buff), "%s%s", name.c_str(), event );

        mpr( buff, channel, param );
        return (true);
    }

    return (false);
}                               // end simple_monster_message()

//---------------------------------------------------------------
//
// handle_movement
//
// Move the monster closer to its target square.
//
//---------------------------------------------------------------
static void _handle_movement(monsters *monster)
{
    int dx, dy;

    // monsters will try to flee out of a sanctuary
    if (is_sanctuary(monster->x, monster->y) && !mons_friendly(monster)
        && !mons_is_fleeing(monster)
        && monster->add_ench(mon_enchant(ENCH_FEAR, 0, KC_YOU)))
    {
        behaviour_event(monster, ME_SCARE, MHITNOT, monster->x, monster->y);
    }
    else if (mons_is_fleeing(monster) && inside_level_bounds(env.sanctuary_pos)
             && !is_sanctuary(monster->x, monster->y)
             && monster->target_x == env.sanctuary_pos.x
             && monster->target_y == env.sanctuary_pos.y)
    {   // once outside there's a chance they'll regain their courage
        if (random2(5) > 2)
            monster->del_ench(ENCH_FEAR);
    }

    // some calculations
    if (monster->type == MONS_BORING_BEETLE && monster->foe == MHITYOU)
    {
        dx = you.x_pos - monster->x;
        dy = you.y_pos - monster->y;
    }
    else
    {
        dx = monster->target_x - monster->x;
        dy = monster->target_y - monster->y;
    }

    // move the monster:
    mmov_x = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    mmov_y = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);

    if (monster->behaviour == BEH_FLEE &&
        (!mons_friendly(monster) ||
         monster->target_x != you.x_pos || monster->target_y != you.y_pos))
    {
        mmov_x *= -1;
        mmov_y *= -1;
    }

    // don't allow monsters to enter a sanctuary
    // or attack you inside a sanctuary even if it's right next to them
    if (is_sanctuary(monster->x + mmov_x, monster->y + mmov_y)
        && (!is_sanctuary(monster->x, monster->y)
            || monster->x + mmov_x == you.x_pos
               && monster->y + mmov_y == you.y_pos))
    {
        mmov_x = 0;
        mmov_y = 0;
    }

    // bounds check: don't let fleeing monsters try to run
    // off the map
    if (monster->target_x + mmov_x < 0 || monster->target_x + mmov_x >= GXM)
        mmov_x = 0;

    if (monster->target_y + mmov_y < 0 || monster->target_y + mmov_y >= GYM)
        mmov_y = 0;

    // now quit if we can't move
    if (mmov_x == 0 && mmov_y == 0)
        return;

    // reproduced here is some semi-legacy code that makes monsters
    // move somewhat randomly along oblique paths.  It is an exceedingly
    // good idea, given crawl's unique line of sight properties.
    //
    // Added a check so that oblique movement paths aren't used when
    // close to the target square. -- bwr
    if (grid_distance( dx, dy, 0, 0 ) > 3)
    {
        if (abs(dx) > abs(dy))
        {
            // sometimes we'll just move parallel the x axis
            if (coinflip())
                mmov_y = 0;
        }

        if (abs(dy) > abs(dx))
        {
            // sometimes we'll just move parallel the y axis
            if (coinflip())
                mmov_x = 0;
        }
    }
}                               // end handle_movement()

//---------------------------------------------------------------
//
// handle_nearby_ability
//
// Gives monsters a chance to use a special ability when they're
// next to the player.
//
//---------------------------------------------------------------
static void _handle_nearby_ability(monsters *monster)
{
    if (!mons_near( monster )
        || monster->behaviour == BEH_SLEEP
        || monster->has_ench(ENCH_SUBMERGED))
    {
        return;
    }

#define MON_SPEAK_CHANCE 21

    if (mons_class_flag(monster->type, M_SPEAKS)
        && (monster->behaviour != BEH_WANDER || monster->attitude == ATT_NEUTRAL)
        && one_chance_in(MON_SPEAK_CHANCE))
    {
        mons_speaks(monster);
    }
    else if (get_mon_shape(monster) >= MON_SHAPE_QUADRUPED)
    {
        // Non-humanoid-ish monsters have a low chance of speaking
        // without the M_SPEAKS flag, to give the dungeon some
        // atmosphere/flavor.
        int chance = MON_SPEAK_CHANCE * 4;

        // Band members are a lot less likely to speak, since there's
        // a lot of them.
        if (testbits(monster->flags, MF_BAND_MEMBER))
            chance *= 10;

        // However, confused and fleeing monsters are more interesting.
        if (monster->behaviour == BEH_FLEE)
            chance /= 2;
        if (monster->has_ench(ENCH_CONFUSION))
            chance /= 2;

        if (one_chance_in(chance))
            mons_speaks(monster);
    }

    if (monster_can_submerge(monster, grd[monster->x][monster->y])
        && !player_beheld_by(monster) // no submerging if player entranced
        && (one_chance_in(5)
            || ((grid_distance( monster->x, monster->y,
                                you.x_pos, you.y_pos ) > 1
                 // FIXME This is better expressed as a
                 // function such as
                 // monster_has_ranged_attack:
                 && monster->type != MONS_ELECTRICAL_EEL
                 && monster->type != MONS_LAVA_SNAKE
                 && (monster->type != MONS_MERMAID
                     || you.species == SP_MERFOLK)
                 // Don't submerge if we just unsubmerged for
                 // the sake of shouting.
                 && monster->seen_context != "bursts forth shouting"
                 && !one_chance_in(20)) )
            || monster->hit_points <= monster->max_hit_points / 2
            || env.cgrid[monster->x][monster->y] != EMPTY_CLOUD))
    {
        monster->add_ench(ENCH_SUBMERGED);
        update_beholders(monster);
        return;
    }

    switch (monster->type)
    {
    case MONS_SPATIAL_VORTEX:
    case MONS_KILLER_KLOWN:
        // used for colour (butterflies too, but they don't change)
        monster->colour = random_colour();
        break;

    case MONS_GIANT_EYEBALL:
        if (coinflip() && !mons_friendly(monster)
            && monster->behaviour != BEH_WANDER)
        {
            simple_monster_message(monster, " stares at you.");

            int &paralysis(you.duration[DUR_PARALYSIS]);
            if (!paralysis || (paralysis < 10 && one_chance_in(1 + paralysis)))
                paralysis += 2 + random2(3);
        }
        break;

    case MONS_EYE_OF_DRAINING:
        if (coinflip() && !mons_friendly(monster)
            && monster->behaviour != BEH_WANDER)
        {
            simple_monster_message(monster, " stares at you.");

            dec_mp(5 + random2avg(13, 3));

            heal_monster(monster, 10, true); // heh heh {dlb}
        }
        break;

    case MONS_AIR_ELEMENTAL:
        if (one_chance_in(5))
            monster->add_ench(ENCH_SUBMERGED);
        break;

    case MONS_PANDEMONIUM_DEMON:
        if (monster->ghost->cycle_colours)
            monster->colour = random_colour();
        break;

    default: break;
    }
}                               // end handle_nearby_ability()

//---------------------------------------------------------------
//
// handle_special_ability
//
// $$$ not sure what to say here...
//
//---------------------------------------------------------------
static bool _handle_special_ability(monsters *monster, bolt & beem)
{
    bool used = false;

    FixedArray < unsigned int, 19, 19 > show;

    const monster_type mclass = (mons_genus( monster->type ) == MONS_DRACONIAN)
                                  ? draco_subspecies( monster )
                                  : static_cast<monster_type>( monster->type );

    if (!mons_near( monster )
        || monster->behaviour == BEH_SLEEP
        || monster->has_ench(ENCH_SUBMERGED))
    {
        return (false);
    }

    switch (mclass)
    {
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
        if (is_sanctuary(monster->x, monster->y))
            break;

        used = orc_battle_cry(monster);
        break;

    case MONS_ORANGE_STATUE:
        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }
        used = orange_statue_effects(monster);
        break;

    case MONS_SILVER_STATUE:
        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }
        used = silver_statue_effects(monster);
        break;

    case MONS_BALL_LIGHTNING:
        if (is_sanctuary(monster->x, monster->y))
            break;

        if (monster->attitude == ATT_HOSTILE
            && distance( you.x_pos, you.y_pos, monster->x, monster->y ) <= 5)
        {
            monster->hit_points = -1;
            used = true;
            break;
        }

        for (int i = 0; i < MAX_MONSTERS; i++)
        {
            monsters *targ = &menv[i];

            if (targ->type == -1 || targ->type == NON_MONSTER)
                continue;

            if (distance( monster->x, monster->y, targ->x, targ->y ) >= 5)
                continue;

            if (monster->attitude == targ->attitude)
                continue;

            // faking LOS by checking the neighbouring square
            int dx = sgn(targ->x - monster->x);
            int dy = sgn(targ->y - monster->y);

            const int tx = monster->x + dx;
            const int ty = monster->y + dy;

            if (tx < 0 || tx > GXM || ty < 0 || ty > GYM)
                continue;

            if (!grid_is_solid(grd[tx][ty]))
            {
                monster->hit_points = -1;
                used = true;
                break;
            }
        }
        break;

    case MONS_LAVA_SNAKE:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        if (coinflip())
            break;

        // setup tracer
        beem.name = "glob of lava";
        beem.range = 4;
        beem.rangeMax = 13;
        beem.damage = dice_def( 3, 10 );
        beem.colour = RED;
        beem.type = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour = BEAM_LAVA;
        beem.hit = 20;
        beem.beam_source = monster_index(monster);
        beem.thrower = KILL_MON;
        beem.aux_source = "glob of lava";

        // fire tracer
        fire_tracer(monster, beem);

        // good idea?
        if (mons_should_fire(beem))
        {
            simple_monster_message(monster, " spits lava!");
            fire_beam(beem);
            used = true;
        }
        break;

    case MONS_ELECTRICAL_EEL:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!mons_player_visible( monster ))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        if (coinflip())
            break;

        // setup tracer
        beem.name = "bolt of electricity";
        beem.damage = dice_def( 3, 6 );
        beem.colour = LIGHTCYAN;
        beem.type = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour = BEAM_ELECTRICITY;
        beem.hit = 50;
        beem.beam_source = monster_index(monster);
        beem.thrower = KILL_MON;
        beem.aux_source = "bolt of electricity";
        beem.range = 4;
        beem.rangeMax = 13;
        beem.is_beam = true;

        // fire tracer
        fire_tracer(monster, beem);

        // good idea?
        if (mons_should_fire(beem))
        {
            simple_monster_message(monster, " shoots out a bolt of electricity!");
            fire_beam(beem);
            used = true;
        }
        break;

    case MONS_ACID_BLOB:
    case MONS_OKLOB_PLANT:
    case MONS_YELLOW_DRACONIAN:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        if (one_chance_in(3))
            used = _plant_spit(monster, beem);

        break;

    case MONS_MOTH_OF_WRATH:
        if (one_chance_in(3))
            used = moth_incite_monsters(monster);
        break;

    case MONS_PIT_FIEND:
        if (one_chance_in(3))
            break;
        // deliberate fall through
    case MONS_FIEND:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        // friendly fiends won't use torment, preferring hellfire
        // (right now there is no way a monster can predict how
        // badly they'll damage the player with torment) -- GDL
        if (one_chance_in(4))
        {
            spell_type spell_cast = SPELL_NO_SPELL;

            switch (random2(4))
            {
            case 0:
                if (!mons_friendly(monster))
                {
                    spell_cast = SPELL_SYMBOL_OF_TORMENT;
                    mons_cast(monster, beem, spell_cast);
                    used = true;
                    break;
                }
                // deliberate fallthrough -- see above
            case 1:
            case 2:
            case 3:
                spell_cast = SPELL_HELLFIRE;
                setup_mons_cast(monster, beem, spell_cast);

                // fire tracer
                fire_tracer(monster, beem);

                // good idea?
                if (mons_should_fire(beem))
                {
                    simple_monster_message( monster, " makes a gesture!",
                                            MSGCH_MONSTER_SPELL );

                    mons_cast(monster, beem, spell_cast);
                    used = true;
                }
                break;
            }

            mmov_x = 0;
            mmov_y = 0;
        }
        break;

    case MONS_IMP:
    case MONS_PHANTOM:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_BLINK_FROG:
    case MONS_KILLER_KLOWN:
        if (one_chance_in(7) || mons_is_caught(monster) && one_chance_in(3))
        {
            simple_monster_message(monster, " blinks.");
            monster_blink(monster);
        }
        break;

    case MONS_MANTICORE:
        if (!mons_player_visible( monster ))
            break;

        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        if (!mons_near(monster))
            break;

        // the fewer spikes the manticore has left, the less
        // likely it will use them.
        if (random2(16) >= static_cast<int>(monster->number))
            break;

        // do the throwing right here, since the beam is so
        // easy to set up and doesn't involve inventory.

        // set up the beam
        beem.name = "volley of spikes";
        beem.range = 9;
        beem.rangeMax = 9;
        beem.hit = 14;
        beem.damage = dice_def( 2, 10 );
        beem.beam_source = monster_index(monster);
        beem.type = dchar_glyph(DCHAR_FIRED_MISSILE);
        beem.colour = LIGHTGREY;
        beem.flavour = BEAM_MISSILE;
        beem.thrower = KILL_MON;
        beem.aux_source = "volley of spikes";
        beem.is_beam = false;

        // fire tracer
        fire_tracer(monster, beem);

        // good idea?
        if (mons_should_fire(beem))
        {
            simple_monster_message(monster, " flicks its tail!");
            fire_beam(beem);
            used = true;
            // decrement # of volleys left
            monster->number--;
        }
        break;

    case MONS_PLAYER_GHOST:
    {
        const ghost_demon &ghost = *(monster->ghost);

        if (ghost.species < SP_RED_DRACONIAN
            || ghost.species == SP_GREY_DRACONIAN
            || ghost.species >= SP_BASE_DRACONIAN
            || ghost.xl < 7
            || one_chance_in(ghost.xl - 5))
        {
            break;
        }
    }
    // dragon breath weapon:
    case MONS_DRAGON:
    case MONS_HELL_HOUND:
    case MONS_ICE_DRAGON:
    case MONS_LINDWURM:
    case MONS_FIREDRAKE:
    case MONS_XTAHUA:
    case MONS_WHITE_DRACONIAN:
    case MONS_RED_DRACONIAN:
        if (!mons_player_visible( monster ))
            break;

        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (is_sanctuary(you.x_pos, you.y_pos)
            || is_sanctuary(monster->x, monster->y))
        {
            break;
        }

        if ((monster->type != MONS_HELL_HOUND && random2(13) < 3)
            || one_chance_in(10))
        {
            setup_dragon(monster, beem);

            // fire tracer
            fire_tracer(monster, beem);

            // good idea?
            if (mons_should_fire(beem))
            {
                simple_monster_message(monster, " breathes.",
                        MSGCH_MONSTER_SPELL);
                fire_beam(beem);
                mmov_x = 0;
                mmov_y = 0;
                used = true;
            }
        }
        break;

    case MONS_MERMAID:
    {
        // Don't behold player already half down or up the stairs
	if ( !you.delay_queue.empty() )
        {
	    delay_queue_item delay = you.delay_queue.front();

	    if (delay.type == DELAY_ASCENDING_STAIRS
	        || delay.type == DELAY_DESCENDING_STAIRS)
	    {
#ifdef DEBUG_DIAGNOSTICS
                mpr("Taking stairs, don't behold.", MSGCH_DIAGNOSTICS);
#endif
                break;
	    }
        }

        // won't sing if either of you silenced, or it's friendly or confused
        if (monster->has_ench(ENCH_CONFUSION) || mons_friendly(monster)
            || silenced(monster->x, monster->y) || silenced(you.x_pos, you.y_pos))
        {
            break;
        }

        // reduce probability because of spamminess
        if (you.species == SP_MERFOLK && !one_chance_in(4))
            break;

        // a wounded invisible mermaid is less likely to give away her position
        if (monster->invisible()
            && monster->hit_points <= monster->max_hit_points / 2
            && !one_chance_in(3))
        {
            break;
        }

        bool already_beheld = player_beheld_by(monster);

        if (one_chance_in(5)
            || monster->foe == MHITYOU && !already_beheld && coinflip())
        {
            if (player_monster_visible(monster))
            {
                simple_monster_message(monster,
                    make_stringf(" chants %s song.",
                    already_beheld? "her luring" : "a haunting").c_str(),
                    MSGCH_MONSTER_SPELL);
            }
            else
            {
                // if you're already beheld by an invisible mermaid she can
                // still prolong the enchantment; otherwise you "resist"
                if (already_beheld)
                    mpr("You hear a luring song.", MSGCH_SOUND);
                else
                {
                    if (one_chance_in(4)) // reduce spamminess
                    {
                        if (coinflip())
                            mpr("You hear a haunting song.", MSGCH_SOUND);
                        else
                            mpr("You hear an eerie melody.", MSGCH_SOUND);

                        canned_msg(MSG_YOU_RESIST); // flavour only
                    }
                    break;
                }
            }

            // once beheld by a particular monster, cannot resist anymore
            if (!already_beheld
                && (you.species == SP_MERFOLK || you_resist_magic(100)))
            {
                canned_msg(MSG_YOU_RESIST);
                break;
            }

            if (!you.duration[DUR_BEHELD])
            {
                you.duration[DUR_BEHELD] = 7;
                you.beheld_by.push_back(monster_index(monster));
                mpr("You are beheld!", MSGCH_WARN);
            }
            else
            {
                you.duration[DUR_BEHELD] += 5;
                if (!already_beheld)
                    you.beheld_by.push_back(monster_index(monster));
            }
            used = true;

            if (you.duration[DUR_BEHELD] > 12)
                you.duration[DUR_BEHELD] = 12;
        }
        break;
    }
    default:
        break;
    }

    if (used)
        monster->lose_energy(EUT_SPECIAL);

    return (used);
}                               // end handle_special_ability()

//---------------------------------------------------------------
//
// handle_potion
//
// Give the monster a chance to quaff a potion. Returns true if
// the monster imbibed.
//
//---------------------------------------------------------------
static bool _handle_potion(monsters *monster, bolt & beem)
{

    // yes, there is a logic to this ordering {dlb}:
    if (monster->behaviour == BEH_SLEEP)
        return (false);
    else if (monster->inv[MSLOT_POTION] == NON_ITEM)
        return (false);
    else if (!one_chance_in(3))
        return (false);
    else
    {
        bool                    imbibed     = false;
        item_type_id_state_type ident       = ID_UNKNOWN_TYPE;
        bool                    was_visible =
            mons_near(monster) && player_monster_visible(monster);

        const int potion_type = mitm[monster->inv[MSLOT_POTION]].sub_type;
        switch (potion_type)
        {
        case POT_HEALING:
        case POT_HEAL_WOUNDS:
            if (monster->hit_points <= monster->max_hit_points / 2
                && mons_holiness(monster) != MH_UNDEAD
                && mons_holiness(monster) != MH_NONLIVING
                && mons_holiness(monster) != MH_PLANT)
            {
                simple_monster_message(monster, " drinks a potion.");

                if (heal_monster(monster, 5 + random2(7), false))
                {
                    simple_monster_message(monster, " is healed!");
                    ident = ID_MON_TRIED_TYPE;
                }

                if (mitm[monster->inv[MSLOT_POTION]].sub_type
                                                    == POT_HEAL_WOUNDS)
                {
                    heal_monster(monster, 10 + random2avg(28, 3), false);
                }

                if (potion_type == POT_HEALING)
                {
                    monster->del_ench(ENCH_POISON);
                    monster->del_ench(ENCH_SICK);
                    if (monster->del_ench(ENCH_CONFUSION))
                        ident = ID_KNOWN_TYPE;
                    if (monster->del_ench(ENCH_ROT))
                        ident = ID_KNOWN_TYPE;
                }

                imbibed = true;
            }
            break;

        case POT_BLOOD:
        case POT_BLOOD_COAGULATED:
            if (mons_species(monster->type) == MONS_VAMPIRE
                && monster->hit_points <= monster->max_hit_points / 2)
            {
                simple_monster_message(monster, " drinks a potion.");

                if (heal_monster(monster, 10 + random2avg(28, 3), false))
                {
                    simple_monster_message(monster, " is healed!");
                    ident = ID_MON_TRIED_TYPE;
                }

                imbibed = true;
            }
            break;

        case POT_SPEED:
            // notice that these are the same odd colours used in
            // mons_ench_f2() {dlb}
            if (monster->has_ench(ENCH_HASTE))
                break;

            beem.colour = BLUE;
            // intentional fall through
        case POT_INVISIBILITY:
            if (mitm[monster->inv[MSLOT_POTION]].sub_type == POT_INVISIBILITY)
            {
                if (monster->has_ench(ENCH_INVIS))
                    break;

                beem.colour = MAGENTA;
                // Friendly monsters won't go invisible if the player
                // can't see invisible. We're being nice.
                if ( mons_friendly(monster) && !player_see_invis(false) )
                    break;
            }

            // allow monsters to drink these when player in sight (jpeg)
            simple_monster_message(monster, " drinks a potion.");
            mons_ench_f2(monster, beem);
            imbibed = true;
            ident = ID_KNOWN_TYPE;
            break;
        }

        if (imbibed)
        {
            if (dec_mitm_item_quantity( monster->inv[MSLOT_POTION], 1 ))
                monster->inv[MSLOT_POTION] = NON_ITEM;
            else if (mitm[monster->inv[MSLOT_POTION]].sub_type == POT_BLOOD
                     || mitm[monster->inv[MSLOT_POTION]].sub_type
                            == POT_BLOOD_COAGULATED)
            {
                remove_oldest_blood_potion(mitm[monster->inv[MSLOT_POTION]]);
            }

            if (ident != ID_UNKNOWN_TYPE && was_visible)
                set_ident_type(OBJ_POTIONS, potion_type, ident);

            monster->lose_energy(EUT_ITEM);
        }

        return (imbibed);
    }
}                               // end handle_potion()

static bool _handle_reaching(monsters *monster)
{
    bool       ret = false;
    const int  wpn = monster->inv[MSLOT_WEAPON];

    if (mons_aligned(monster_index(monster), monster->foe))
        return (false);

    if (monster->has_ench(ENCH_SUBMERGED))
        return (false);

    if (wpn != NON_ITEM && get_weapon_brand( mitm[wpn] ) == SPWPN_REACHING )
    {
        if (monster->foe == MHITYOU)
        {
            // this check isn't redundant -- player may be invisible.
            if (monster->target_pos() == you.pos()
                && see_grid_no_trans(monster->pos()))
            {
                int dx = abs(monster->x - you.x_pos);
                int dy = abs(monster->y - you.y_pos);

                if ((dx == 2 && dy <= 2) || (dy == 2 && dx <= 2))
                {
                    ret = true;
                    monster_attack( monster_index(monster) );
                }
            }
        }
        else if (monster->foe != MHITNOT)
        {
            // same comments as to invisibility as above.
            if (monster->target_x == menv[monster->foe].x
                && monster->target_y == menv[monster->foe].y)
            {
                int dx = abs(monster->x - menv[monster->foe].x);
                int dy = abs(monster->y - menv[monster->foe].y);
                if ((dx == 2 && dy <= 2) || (dy == 2 && dx <= 2))
                {
                    ret = true;
                    monsters_fight( monster_index(monster), monster->foe );
                }
            }
        }
    }

    // Player saw the item reach
    if (ret && !is_artefact(mitm[wpn]) && mons_near(monster)
        && player_monster_visible(monster))
    {
        set_ident_flags(mitm[wpn], ISFLAG_KNOW_TYPE);
    }

    return ret;
}                               // end handle_reaching()

//---------------------------------------------------------------
//
// handle_scroll
//
// Give the monster a chance to read a scroll. Returns true if
// the monster read something.
//
//---------------------------------------------------------------
static bool _handle_scroll(monsters *monster)
{
    // yes, there is a logic to this ordering {dlb}:
    if (monster->has_ench(ENCH_CONFUSION)
        || monster->behaviour == BEH_SLEEP
        || monster->has_ench(ENCH_SUBMERGED))
    {
        return (false);
    }
    else if (monster->inv[MSLOT_SCROLL] == NON_ITEM)
        return (false);
    else if (!one_chance_in(3))
        return (false);
    else
    {
        bool                    read        = false;
        item_type_id_state_type ident       = ID_UNKNOWN_TYPE;
        bool                    was_visible =
            mons_near(monster) && player_monster_visible(monster);

        // notice how few cases are actually accounted for here {dlb}:
        const int scroll_type = mitm[monster->inv[MSLOT_SCROLL]].sub_type;
        switch (scroll_type)
        {
        case SCR_TELEPORTATION:
            if (!monster->has_ench(ENCH_TP))
            {
                if (monster->behaviour == BEH_FLEE || monster->has_ench(ENCH_HELD))
                {
                    simple_monster_message(monster, " reads a scroll.");
                    monster_teleport(monster, false);
                    read  = true;
                    ident = ID_KNOWN_TYPE;
                }
            }
            break;

        case SCR_BLINKING:
            if (monster->behaviour == BEH_FLEE || monster->has_ench(ENCH_HELD))
            {
                if (mons_near(monster))
                {
                    simple_monster_message(monster, " reads a scroll.");
                    simple_monster_message(monster, " blinks!");
                    monster_blink(monster);
                    read  = true;
                    ident = ID_KNOWN_TYPE;
                }
            }
            break;

        case SCR_SUMMONING:
            if (mons_near(monster))
            {
                simple_monster_message(monster, " reads a scroll.");
                create_monster( MONS_ABOMINATION_SMALL, 2,
                                SAME_ATTITUDE(monster), monster->x, monster->y,
                                monster->foe, MONS_PROGRAM_BUG );
                read  = true;
                ident = ID_KNOWN_TYPE;
            }
            break;
        }

        if (read)
        {
            if (dec_mitm_item_quantity( monster->inv[MSLOT_SCROLL], 1 ))
                monster->inv[MSLOT_SCROLL] = NON_ITEM;

            if (ident != ID_UNKNOWN_TYPE && was_visible)
                set_ident_type(OBJ_SCROLLS, scroll_type, ident);

            monster->lose_energy(EUT_ITEM);
        }

        return read;
    }
}                               // end handle_scroll()

//---------------------------------------------------------------
//
// handle_wand
//
// Give the monster a chance to zap a wand. Returns true if the
// monster zapped.
//
//---------------------------------------------------------------
static bool _handle_wand(monsters *monster, bolt &beem)
{
    // yes, there is a logic to this ordering {dlb}:
    if (monster->behaviour == BEH_SLEEP)
        return (false);
    else if (!mons_near(monster))
        return (false);
    else if (monster->has_ench(ENCH_SUBMERGED))
        return (false);
    else if (monster->inv[MSLOT_WAND] == NON_ITEM
             || mitm[monster->inv[MSLOT_WAND]].plus <= 0)
    {
        return (false);
    }
    else if (coinflip())
    {
        bool niceWand    = false;
        bool zap         = false;
        bool was_visible
                 = mons_near(monster) && player_monster_visible(monster);

        item_def &wand(mitm[monster->inv[MSLOT_WAND]]);

        // map wand type to monster spell type
        const spell_type mzap = _map_wand_to_mspell(wand.sub_type);
        if (mzap == SPELL_NO_SPELL)
            return (false);

        // set up the beam
        int power    = 30 + monster->hit_dice;
        bolt theBeam = mons_spells(mzap, power);

        beem.name         = theBeam.name;
        beem.beam_source  = monster_index(monster);
        beem.source_x     = monster->x;
        beem.source_y     = monster->y;
        beem.colour       = theBeam.colour;
        beem.range        = theBeam.range;
        beem.rangeMax     = theBeam.rangeMax;
        beem.damage       = theBeam.damage;
        beem.ench_power   = theBeam.ench_power;
        beem.hit          = theBeam.hit;
        beem.type         = theBeam.type;
        beem.flavour      = theBeam.flavour;
        beem.thrower      = theBeam.thrower;
        beem.is_beam      = theBeam.is_beam;
        beem.is_explosion = theBeam.is_explosion;

#if HISCORE_WEAPON_DETAIL
        beem.aux_source =
            wand.name(DESC_QUALNAME, false, true, false, false);
#else
        beem.aux_source =
            wand.name(DESC_QUALNAME, false, true, false, false,
                      ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
#endif

        const int wand_type = wand.sub_type;
        switch (wand_type)
        {
        case WAND_DISINTEGRATION:
            // Dial down damage from wands of disintegration, since
            // disintegration beams can do large amounts of damage.
            beem.damage.size = beem.damage.size * 2 / 3;
            break;

            // these have been deemed "too tricky" at this time {dlb}:
        case WAND_ENSLAVEMENT:
        case WAND_DIGGING:
        case WAND_RANDOM_EFFECTS:
            return (false);

        case WAND_POLYMORPH_OTHER:
            // Monsters can be very trigger happy with wands, reduce this
            // for polymorph.
            if (!one_chance_in(5))
                return false;
            break;

        // these are wands that monsters will aim at themselves {dlb}:
        case WAND_HASTING:
            if (!monster->has_ench(ENCH_HASTE))
            {
                beem.target_x = monster->x;
                beem.target_y = monster->y;

                niceWand = true;
                break;
            }
            return (false);

        case WAND_HEALING:
            if (monster->hit_points <= monster->max_hit_points / 2)
            {
                beem.target_x = monster->x;
                beem.target_y = monster->y;

                niceWand = true;
                break;
            }
            return (false);

        case WAND_INVISIBILITY:
            if (!monster->has_ench(ENCH_INVIS)
                && !monster->has_ench(ENCH_SUBMERGED)
                && (!mons_friendly(monster) || player_see_invis(false)))
            {
                beem.target_x = monster->x;
                beem.target_y = monster->y;

                niceWand = true;
                break;
            }
            return (false);

        case WAND_TELEPORTATION:
            if (monster->hit_points <= monster->max_hit_points / 2
                || mons_is_caught(monster))
            {
                if (!monster->has_ench(ENCH_TP)
                    && !one_chance_in(20))
                {
                    beem.target_x = monster->x;
                    beem.target_y = monster->y;

                    niceWand = true;
                    break;
                }
                // this break causes the wand to be tried on the player:
                break;
            }
            return (false);
        }

        // fire tracer, if necessary
        if (!niceWand)
        {
            fire_tracer( monster, beem );

            // good idea?
            zap = mons_should_fire( beem );
        }

        if (niceWand || zap)
        {
            if (!simple_monster_message(monster, " zaps a wand."))
            {
                if (!silenced(you.x_pos, you.y_pos))
                    mpr("You hear a zap.", MSGCH_SOUND);
            }

            // charge expenditure {dlb}
            wand.plus--;
            beem.is_tracer = false;
            fire_beam( beem );

            if (was_visible)
            {
                if (niceWand || beem.name != "0" || beem.obvious_effect)
                    set_ident_type(OBJ_WANDS, wand_type, ID_KNOWN_TYPE);
                else
                    set_ident_type(OBJ_WANDS, wand_type, ID_MON_TRIED_TYPE);

                // increment zap count
                if ( wand.plus2 >= 0 )
                    wand.plus2++;
            }

            monster->lose_energy(EUT_ITEM);

            return (true);
        }
    }

    return (false);
}                               // end handle_wand()

// Returns a suitable breath weapon for the draconian; does not handle all
// draconians, does fire a tracer.
static spell_type _get_draconian_breath_spell( const monsters *monster )
{
    spell_type draco_breath = SPELL_NO_SPELL;

    if (mons_genus( monster->type ) == MONS_DRACONIAN)
    {
        switch (draco_subspecies( monster ))
        {
        case MONS_BLACK_DRACONIAN:
            draco_breath = SPELL_LIGHTNING_BOLT;
            break;

        case MONS_PALE_DRACONIAN:
            draco_breath = SPELL_STEAM_BALL;
            break;

        case MONS_GREEN_DRACONIAN:
            draco_breath = SPELL_POISONOUS_CLOUD;
            break;

        case MONS_PURPLE_DRACONIAN:
            draco_breath = SPELL_ISKENDERUNS_MYSTIC_BLAST;
            break;

        case MONS_MOTTLED_DRACONIAN:
            draco_breath = SPELL_STICKY_FLAME;
            break;

        case MONS_DRACONIAN:
        case MONS_YELLOW_DRACONIAN:     // already handled as ability
        case MONS_RED_DRACONIAN:        // already handled as ability
        case MONS_WHITE_DRACONIAN:      // already handled as ability
        default:
            break;
        }
    }


    if (draco_breath != SPELL_NO_SPELL)
    {
        // [ds] Check line-of-fire here. It won't happen elsewhere.
        bolt beem;
        setup_mons_cast(monster, beem, draco_breath);

        fire_tracer(monster, beem);

        if (!mons_should_fire(beem))
            draco_breath = SPELL_NO_SPELL;
    }

    return (draco_breath);
}

static bool _is_emergency_spell(const monster_spells &msp, int spell)
{
    // If the emergency spell appears early, it's probably not a dedicated
    // escape spell.
    for (int i = 0; i < 5; ++i)
        if (msp[i] == spell)
            return (false);

    return (msp[5] == spell);
}

static const char *_orb_of_fire_glow()
{
    static const char *orb_glows[] =
    {
        " glows yellow.",
        " glows bright magenta.",
        " glows deep purple.",       // Smoke on the Water
        " glows red.",
        " emits a lurid red light.",
    };
    return RANDOM_ELEMENT(orb_glows);
}

static bool _mons_announce_cast(monsters *monster, bool nearby,
                                spell_type spell_cast,
                                spell_type draco_breath)
{
    if (nearby)      // handle monsters within range of player
    {
        if (monster->type == MONS_GERYON)
        {
            if (silenced(monster->x, monster->y))
                return (false);

            simple_monster_message( monster, " winds a great silver horn.",
                                    MSGCH_MONSTER_SPELL );
        }
        else if (mons_is_demon( monster->type ))
        {
            simple_monster_message( monster, " gestures.",
                                    MSGCH_MONSTER_SPELL );
        }
        else
        {
            switch (monster->type)
            {
            default:
                if (spell_cast == draco_breath)
                {
                    if (!simple_monster_message(monster, " breathes.",
                                                MSGCH_MONSTER_SPELL))
                    {
                        if (!silenced(monster->x, monster->y)
                            && !silenced(you.x_pos, you.y_pos))
                        {
                            mpr("You hear a roar.", MSGCH_SOUND);
                        }
                    }
                    break;
                }

                if (silenced(monster->x, monster->y))
                    return (false);

                if (mons_class_flag(monster->type, M_PRIEST))
                {
                    switch (random2(3))
                    {
                    case 0:
                        simple_monster_message( monster,
                                                " prays.",
                                                MSGCH_MONSTER_SPELL );
                        break;
                    case 1:
                        simple_monster_message( monster,
                                                " mumbles some strange prayers.",
                                                MSGCH_MONSTER_SPELL );
                        break;
                    case 2:
                    default:
                        simple_monster_message( monster,
                                                " utters an invocation.",
                                                MSGCH_MONSTER_SPELL );
                        break;
                    }
                }
                else
                {
                    switch (random2(3))
                    {
                    case 0:
                        // XXX: could be better, chosen to match the
                        // ones in monspeak.cc... has the problem
                        // that it doesn't suggest a vocal component. -- bwr
                        if (player_monster_visible(monster))
                        {
                            simple_monster_message( monster,
                                                    " gestures wildly.",
                                                    MSGCH_MONSTER_SPELL );
                        }
                        break;
                    case 1:
                        simple_monster_message( monster,
                                                " mumbles some strange words.",
                                                MSGCH_MONSTER_SPELL );
                        break;
                    case 2:
                    default:
                        simple_monster_message( monster,
                                                " casts a spell.",
                                                MSGCH_MONSTER_SPELL );
                        break;
                    }
                }
                break;

            case MONS_BALL_LIGHTNING:
                monster->hit_points = -1;
                break;

            case MONS_STEAM_DRAGON:
            case MONS_MOTTLED_DRAGON:
            case MONS_STORM_DRAGON:
            case MONS_GOLDEN_DRAGON:
            case MONS_SHADOW_DRAGON:
            case MONS_SWAMP_DRAGON:
            case MONS_SWAMP_DRAKE:
            case MONS_DEATH_DRAKE:
            case MONS_HELL_HOG:
            case MONS_SERPENT_OF_HELL:
            case MONS_QUICKSILVER_DRAGON:
            case MONS_IRON_DRAGON:
                if (!simple_monster_message(monster, " breathes.",
                                            MSGCH_MONSTER_SPELL))
                {
                    if (!silenced(monster->x, monster->y)
                        && !silenced(you.x_pos, you.y_pos))
                    {
                        mpr("You hear a roar.", MSGCH_SOUND);
                    }
                }
                break;

            case MONS_VAPOUR:
                monster->add_ench(ENCH_SUBMERGED);
                break;

            case MONS_BRAIN_WORM:
            case MONS_ELECTRIC_GOLEM:
            case MONS_ICE_STATUE:
                // These don't show any signs that they're casting a spell.
                break;

            case MONS_GREAT_ORB_OF_EYES:
            case MONS_SHINING_EYE:
            case MONS_EYE_OF_DEVASTATION:
                simple_monster_message(monster, " gazes.", MSGCH_MONSTER_SPELL);
                break;

            case MONS_GIANT_ORANGE_BRAIN:
                simple_monster_message(monster, " pulsates.",
                                       MSGCH_MONSTER_SPELL);
                break;

            case MONS_NAGA:
            case MONS_NAGA_WARRIOR:
                simple_monster_message(monster, " spits poison.",
                                       MSGCH_MONSTER_SPELL);
                break;

            case MONS_ORB_OF_FIRE:
                simple_monster_message(monster, _orb_of_fire_glow(),
                                       MSGCH_MONSTER_SPELL);
                break;
            }
        }
    }
    else                    // handle far-away monsters
    {
        if (monster->type == MONS_GERYON
            && !silenced(you.x_pos, you.y_pos))
        {
            mpr("You hear a weird and mournful sound.", MSGCH_SOUND);
        }
    }

    return (true);
}

//---------------------------------------------------------------
//
// handle_spell
//
// Give the monster a chance to cast a spell. Returns true if
// a spell was cast.
//
//---------------------------------------------------------------
static bool _handle_spell( monsters *monster, bolt & beem )
{
    bool monsterNearby = mons_near(monster);
    bool finalAnswer   = false;   // as in: "Is that your...?" {dlb}
    const spell_type draco_breath = _get_draconian_breath_spell(monster);

    // yes, there is a logic to this ordering {dlb}:
    if (monster->behaviour == BEH_SLEEP
        || (!mons_class_flag(monster->type, M_SPELLCASTER)
                && draco_breath == SPELL_NO_SPELL)
        || monster->has_ench(ENCH_SUBMERGED))
    {
        return (false);
    }

    if ((mons_class_flag(monster->type, M_ACTUAL_SPELLS)
            || mons_class_flag(monster->type, M_PRIEST))
        && (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER, ENCH_SHAPESHIFTER)))
    {
        return (false);           //jmf: shapeshifters don't get spells, just
                                  //     physical powers.
    }
    else if (monster->has_ench(ENCH_CONFUSION)
             && !mons_class_flag(monster->type, M_CONFUSED))
    {
        return (false);
    }
    else if (monster->type == MONS_PANDEMONIUM_DEMON
             && !monster->ghost->spellcaster)
    {
        return (false);
    }
    else if (random2(200) > monster->hit_dice + 50
             || (monster->type == MONS_BALL_LIGHTNING && coinflip()))
    {
        return (false);
    }
    else
    {
        spell_type spell_cast = SPELL_NO_SPELL;
        monster_spells hspell_pass(monster->spells);

        if (!mon_enemies_around(monster))
        {
            // forces the casting of dig when player not visible - this is EVIL!
            if (monster->has_spell(SPELL_DIG)
                && monster->behaviour == BEH_SEEK)
            {
                spell_cast = SPELL_DIG;
                finalAnswer = true;
            }
            else if ((monster->has_spell(SPELL_LESSER_HEALING)
                      || monster->has_spell(SPELL_GREATER_HEALING))
                     && monster->hit_points < monster->max_hit_points)
            {
                // The player's out of sight!
                // Quick, let's take a turn to heal ourselves. -- bwr
                spell_cast =
                    monster->has_spell(SPELL_GREATER_HEALING)?
                    SPELL_GREATER_HEALING : SPELL_LESSER_HEALING;
                finalAnswer = true;
            }
            else if (monster->behaviour == BEH_FLEE)
            {
                // Since the player isn't around, we'll extend the monster's
                // normal fleeing choices to include the self-enchant slot.

                int foundcount = 0;
                for (int i = NUM_MONSTER_SPELL_SLOTS - 1; i >= 0; --i)
                {
                    if (ms_useful_fleeing_out_of_sight(monster, hspell_pass[i])
                        && one_chance_in(++foundcount))
                    {
                        spell_cast = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
            else if (monster->foe == MHITYOU && !monsterNearby)
                return (false);
        }

        // monsters caught in a net try to get away
        // this is only urgent if enemies are around
        if (!finalAnswer && mon_enemies_around(monster)
            && mons_is_caught(monster) && one_chance_in(4))
        {
            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            {
                if (ms_quick_get_away( monster, hspell_pass[i] ))
                {
                    spell_cast = hspell_pass[i];
                    finalAnswer = true;
                    break;
                }
            }
        }

        // Promote the casting of useful spells for low-HP monsters.
        if (!finalAnswer
            && monster->hit_points < monster->max_hit_points / 4
            && !one_chance_in(4))
        {
            // Note: There should always be at least some chance we don't
            // get here... even if the monster is on its last HP.  That
            // way we don't have to worry about monsters infinitely casting
            // Healing on themselves (e.g. orc high priests).
            if (monster->behaviour == BEH_FLEE
                && ms_low_hitpoint_cast( monster, hspell_pass[5] ))
            {
                spell_cast = hspell_pass[5];
                finalAnswer = true;
            }

            if (!finalAnswer)
            {
                int found_spell = 0;
                for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
                {
                    if (ms_low_hitpoint_cast( monster, hspell_pass[i] )
                        && one_chance_in(++found_spell))
                    {
                        spell_cast = hspell_pass[i];
                        finalAnswer = true;
                    }
                }
            }
        }

        if (!finalAnswer)
        {
            // if nothing found by now, safe friendlies will rarely cast
            if (mons_friendly(monster) && !mon_enemies_around(monster)
                && !one_chance_in(8))
            {
                return (false);
            }

            // remove healing/invis/haste if we don't need them
            int num_no_spell = 0;

            for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            {
                if (hspell_pass[i] == SPELL_NO_SPELL)
                    num_no_spell++;
                else if (ms_waste_of_time( monster, hspell_pass[i] )
                // should monster not have selected dig by now, it never will:
                         || hspell_pass[i] == SPELL_DIG)
                {
                    hspell_pass[i] = SPELL_NO_SPELL;
                    num_no_spell++;
                }
            }

            // If no useful spells... cast no spell.
            if (num_no_spell == NUM_MONSTER_SPELL_SLOTS
                && draco_breath == SPELL_NO_SPELL)
            {
                return (false);
            }

            // up to four tries to pick a spell.
            for (int loopy = 0; loopy < 4; loopy ++)
            {
                bool spellOK = false;

                // setup spell - fleeing monsters will always try to
                // choose their emergency spell.
                if (monster->behaviour == BEH_FLEE)
                {
                    spell_cast = (one_chance_in(5) ? SPELL_NO_SPELL
                                                   : hspell_pass[5]);
                }
                else if (is_sanctuary(you.x_pos, you.y_pos)
                         || is_sanctuary(monster->x, monster->y))
                {
                    return (false);
                }
                else
                {
                    // Randomly picking one of the non-emergency spells:
                    spell_cast = hspell_pass[random2(5)];
                }

                if (spell_cast == SPELL_NO_SPELL)
                    continue;

                // setup the spell
                setup_mons_cast(monster, beem, spell_cast);

                // beam-type spells requiring tracers
                if (ms_requires_tracer(spell_cast))
                {
                    fire_tracer(monster, beem);
                    // good idea?
                    if (mons_should_fire(beem))
                        spellOK = true;
                }
                else
                {
                    // all direct-effect/summoning/self-enchantments/etc.
                    spellOK = true;

                    if (is_sanctuary(you.x_pos, you.y_pos)
                        || is_sanctuary(monster->x, monster->y))
                    {
                        spellOK = false;
                    }
                    else if (ms_direct_nasty(spell_cast)
                             && mons_aligned(monster_index(monster), monster->foe))
                    {
                        spellOK = false;
                    }
                    else if (monster->foe == MHITYOU || monster->foe == MHITNOT)
                    {
                        // XXX: Note the crude hack so that monsters can
                        // use ME_ALERT to target (we should really have
                        // a measure of time instead of peeking to see
                        // if the player is still there). -- bwr
                        if (!mons_player_visible( monster )
                            && (monster->target_x != you.x_pos
                                || monster->target_y != you.y_pos
                                || coinflip()))
                        {
                            spellOK = false;
                        }
                    }
                    else if (!mons_monster_visible( monster, &menv[monster->foe] ))
                    {
                        spellOK = false;
                    }
                }

                // if not okay, then maybe we'll cast a defensive spell
                if (!spellOK)
                {
                    if (is_sanctuary(you.x_pos, you.y_pos)
                        || is_sanctuary(monster->x, monster->y))
                    {
                        spell_cast = SPELL_NO_SPELL;
                    }
                    else
                        spell_cast = (coinflip() ? hspell_pass[2] : SPELL_NO_SPELL);
                }

                if (spell_cast != SPELL_NO_SPELL)
                    break;
            }
        }

        // If there's otherwise no ranged attack use the breath weapon.
        // The breath weapon is also occasionally used.
        if (draco_breath != SPELL_NO_SPELL
                && (spell_cast == SPELL_NO_SPELL
                    || (!_is_emergency_spell(hspell_pass, spell_cast)
                        && one_chance_in(4)))
            && !is_sanctuary(you.x_pos, you.y_pos)
            && !is_sanctuary(monster->x, monster->y))
        {
            spell_cast = draco_breath;
            finalAnswer = true;
        }

        // should the monster *still* not have a spell, well, too bad {dlb}:
        if (spell_cast == SPELL_NO_SPELL)
            return (false);

        // Try to animate dead: if nothing rises, pretend we didn't cast it
        if (spell_cast == SPELL_ANIMATE_DEAD
            && !animate_dead( monster, 100, SAME_ATTITUDE(monster),
                              monster->foe, 0 ))
        {
            return (false);
        }

        if (!_mons_announce_cast(monster, monsterNearby,
                                 spell_cast, draco_breath))
        {
            return (false);
        }

        // FINALLY! determine primary spell effects {dlb}:
        if (spell_cast == SPELL_BLINK)
        {
            // why only cast blink if nearby? {dlb}
            if (monsterNearby)
            {
                simple_monster_message(monster, " blinks!");
                monster_blink(monster);

                monster->lose_energy(EUT_SPELL);
            }
            else
                return (false);
        }
        else
        {
            mons_cast(monster, beem, spell_cast);
            mmov_x = 0;
            mmov_y = 0;
            monster->lose_energy(EUT_SPELL);
        }
    } // end "if mons_class_flag(monster->type, M_SPELLCASTER) ...

    return (true);
}                               // end handle_spell()

// Returns a rough estimate of damage from throwing the wielded weapon.
int mons_thrown_weapon_damage(const item_def *weap)
{
    if (!weap || get_weapon_brand(*weap) != SPWPN_RETURNING)
        return (0);
    return std::max(0, (property(*weap, PWPN_DAMAGE) + weap->plus2 / 2));
}

// Returns a rough estimate of damage from firing/throwing missile.
int mons_missile_damage(const item_def *launch,
                        const item_def *missile)
{
    if (!missile || (!launch && !is_throwable(*missile)))
        return (0);

    const int missile_damage = property(*missile, PWPN_DAMAGE) / 2 + 1;
    const int launch_damage  = launch? property(*launch, PWPN_DAMAGE) : 0;
    return std::max(0, launch_damage + missile_damage);
}

int mons_weapon_damage_rating(const item_def &launcher)
{
    return (property(launcher, PWPN_DAMAGE) + launcher.plus2);
}

// Given the monster's current weapon and alt weapon (either or both of
// which may be NULL), works out whether using missiles or throwing the
// main weapon (with returning brand) is better. If using missiles that
// need a launcher, sets *launcher to the launcher.
//
// If the monster has no ranged weapon attack, returns NON_ITEM.
//
int mons_pick_best_missile(monsters *mons, item_def **launcher,
                           bool ignore_melee)
{
    *launcher = NULL;
    item_def *melee = NULL, *launch = NULL;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                launch = item;
            else if (!ignore_melee)
                melee = item;
        }
    }

    const item_def *missiles = mons->missiles();
    if (launch && missiles && !missiles->launched_by(*launch))
        launch = NULL;

    const int tdam = mons_thrown_weapon_damage(melee);
    const int fdam = mons_missile_damage(launch, missiles);

    if (!tdam && !fdam)
        return (NON_ITEM);
    else if (tdam >= fdam)
        return (melee->index());
    else
    {
        *launcher = launch;
        return (missiles->index());
    }
}

//---------------------------------------------------------------
//
// handle_throw
//
// Give the monster a chance to throw something. Returns true if
// the monster hurled.
//
//---------------------------------------------------------------
static bool _handle_throw(monsters *monster, bolt & beem)
{
    // yes, there is a logic to this ordering {dlb}:
    if (monster->incapacitated()
        || monster->asleep()
        || monster->submerged())
    {
        return (false);
    }

    if (mons_itemuse(monster->type) < MONUSE_OPEN_DOORS)
        return (false);

    const bool archer = mons_class_flag(monster->type, M_ARCHER);
    // Highly-specialised archers are more likely to shoot than talk.
    if (one_chance_in(archer? 9 : 5))
        return (false);

    // don't allow offscreen throwing for now.
    if (monster->foe == MHITYOU && !mons_near(monster))
        return (false);

    // Monsters won't shoot in melee range, largely for balance reasons.
    // Specialist archers are an exception to this rule.
    if (!archer
        && adjacent( beem.target_x, beem.target_y, monster->x, monster->y ))
    {
        return (false);
    }

    item_def *launcher = NULL;
    const item_def *weapon = NULL;
    const int mon_item = mons_pick_best_missile(monster, &launcher);

    if (mon_item == NON_ITEM || !is_valid_item(mitm[mon_item]))
        return (false);

    if (is_sanctuary(monster->x, monster->y)
        || is_sanctuary(beem.target_x, beem.target_y))
    {
        return (false);
    }

    // throwing a net at a target that is already caught would be
    // completely useless, so bail out
    if (mitm[mon_item].base_type == OBJ_MISSILES
        && mitm[mon_item].sub_type == MI_THROWING_NET
        && (beem.target_x == you.x_pos && beem.target_y == you.y_pos
        && you.caught()))
    {
        return (false);
    }

    // If the attack needs a launcher that we can't wield, bail out.
    if (launcher)
    {
        weapon = monster->mslot_item(MSLOT_WEAPON);
        if (weapon && weapon != launcher && weapon->cursed())
            return (false);
    }

    // ok, we'll try it.
    setup_generic_throw( monster, beem );

    // set fake damage for the tracer.
    beem.damage = dice_def(10, 10);

    // fire tracer
    fire_tracer( monster, beem );

    // clear fake damage (will be set correctly in mons_throw).
    beem.damage = 0;

    // good idea?
    if (mons_should_fire( beem ))
    {
        if (launcher && launcher != weapon)
            monster->swap_weapons();

        beem.name.clear();
        return (mons_throw( monster, beem, mon_item ));
    }

    return (false);
}                               // end handle_throw()

static bool _handle_monster_spell(monsters *monster, bolt &beem)
{
    // shapeshifters don't get spells
    if (!monster->has_ench( ENCH_GLOWING_SHAPESHIFTER,
                            ENCH_SHAPESHIFTER )
        || !mons_class_flag( monster->type, M_ACTUAL_SPELLS ))
    {
        if (_handle_spell(monster, beem))
            return (true);
    }
    return (false);
}

// Give the monster its action energy (aka speed_increment).
static void _monster_add_energy(monsters *monster)
{
    int energy_gained = (monster->speed * you.time_taken) / 10;

    // Slow monsters might get 0 here. Maybe we should factor in
    // *how* slow it is...but a 10-to-1 move ratio seems more than
    // enough.
    if ( energy_gained == 0 && monster->speed != 0 )
        energy_gained = 1;

    monster->speed_increment += energy_gained;
}

int mons_natural_regen_rate(monsters *monster)
{
    // A HD divider ranging from 3 (at 1 HD) to 1 (at 8 HD).
    int divider =
        std::max(div_rand_round(15 - monster->hit_dice, 4), 1);

    // The undead have a harder time regenerating.
    switch (monster->holiness())
    {
    // The undead don't regenerate easily.
    case MH_UNDEAD:
        divider *= 4;
        break;

    // And golems have it worse.
    case MH_NONLIVING:
        divider *= 5;
        break;

    default:
        break;
    }

    return (std::max(div_rand_round(monster->hit_dice, divider), 1));
}

static inline bool _mons_natural_regen_roll(monsters *monster)
{
    const int regen_rate = mons_natural_regen_rate(monster);
    return (random2(25) < regen_rate);
}

// Do natural regeneration for monster.
static void _monster_regenerate(monsters *monster)
{
    if (monster->has_ench(ENCH_SICK))
        return;

    // Non-land creatures out of their element cannot regenerate.
    if (mons_habitat(monster) != HT_LAND
        && !monster_habitable_grid(monster, grd(monster->pos())))
    {
        return;
    }

    if (monster_descriptor(monster->type, MDSC_REGENERATES)
        || (monster->type == MONS_FIRE_ELEMENTAL
            && (grd(monster->pos()) == DNGN_LAVA
                || cloud_type_at(monster->pos()) == CLOUD_FIRE))

        || (monster->type == MONS_WATER_ELEMENTAL
            && grid_is_watery(grd(monster->pos())))

        || (monster->type == MONS_AIR_ELEMENTAL
            && env.cgrid(monster->pos()) == EMPTY_CLOUD
            && one_chance_in(3))

        || _mons_natural_regen_roll(monster))
    {
        heal_monster(monster, 1, false);
    }
}

static void _swim_or_move_energy(monsters *mon)
{
    const dungeon_feature_type feat = grd[mon->x][mon->y];
    // FIXME: Replace check with mons_is_swimming()?
    mon->lose_energy(
        feat >= DNGN_LAVA && feat <= DNGN_SHALLOW_WATER
        && !mon->airborne() ? EUT_SWIM : EUT_MOVE );
}

#if DEBUG
#    define DEBUG_ENERGY_USE(problem) \
         if (monster->speed_increment == old_energy) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  monster->name(DESC_PLAIN).c_str(), true);
#else
#    define DEBUG_ENERGY_USE(problem) ((void) 0)
#endif

static void _handle_monster_move(int i, monsters *monster)
{
    bool brkk = false;
    bolt beem;
    FixedArray <unsigned int, 19, 19> show;

    if (monster->hit_points > monster->max_hit_points)
        monster->hit_points = monster->max_hit_points;

    // monster just summoned (or just took stairs), skip this action
    if (testbits( monster->flags, MF_JUST_SUMMONED ))
    {
        monster->flags &= ~MF_JUST_SUMMONED;
        return;
    }

    _monster_add_energy(monster);

    // Handle clouds on nonmoving monsters:
    if (monster->speed == 0)
    {
        if (env.cgrid[monster->x][monster->y] != EMPTY_CLOUD
            && !monster->has_ench(ENCH_SUBMERGED))
        {
            _mons_in_cloud( monster );
        }
    }

    // Apply monster enchantments once for every normal-speed
    // player turn.
    monster->ench_countdown -= you.time_taken;
    while (monster->ench_countdown < 0)
    {
        monster->ench_countdown += 10;
        monster->apply_enchantments();

        // If the monster *merely* died just break from the loop
        // rather than quit altogether, since we have to deal with
        // giant spores and ball lightning exploding at the end of the
        // function, but do return if the monster's data has been
        // reset, since then the monster type is invalid.
        if (monster->type == -1)
            return;
        else if (monster->hit_points < 1)
            break;
    }

    // memory is decremented here for a reason -- we only want it
    // decrementing once per monster "move"
    if (monster->foe_memory > 0)
        monster->foe_memory--;

    if (monster->type == MONS_GLOWING_SHAPESHIFTER)
        monster->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    // otherwise there are potential problems with summonings
    if (monster->type == MONS_SHAPESHIFTER)
        monster->add_ench(ENCH_SHAPESHIFTER);

    // We reset batty monsters from wander to seek here, instead
    // of in handle_behaviour() since that will be called with
    // every single movement, and we want these monsters to
    // hit and run. -- bwr
    if (monster->foe != MHITNOT
        && monster->behaviour == BEH_WANDER
        && testbits( monster->flags, MF_BATTY ))
    {
        monster->behaviour = BEH_SEEK;
    }

    monster->check_speed();

    monsterentry* entry = get_monster_data(monster->type);
    if (!entry)
        return;

    int old_energy      = INT_MAX;
    int non_move_energy = std::min(entry->energy_usage.move,
                                   entry->energy_usage.swim);

    while (monster->has_action_energy())
    {                   // The continues & breaks are WRT this.
        if (!monster->alive())
            break;

        if (monster->speed_increment >= old_energy)
        {
#if DEBUG
            if (monster->speed_increment == old_energy)
                mprf(MSGCH_DIAGNOSTICS, "'%s' has same energy as last loop",
                     monster->name(DESC_PLAIN, true).c_str());
            else
                mprf(MSGCH_DIAGNOSTICS, "'%s' has MORE energy than last loop",
                     monster->name(DESC_PLAIN, true).c_str());
#endif
            monster->speed_increment = old_energy - 10;
            old_energy               = monster->speed_increment;
            continue;
        }
        old_energy = monster->speed_increment;

        monster->shield_blocks = 0;

        if (env.cgrid[monster->x][monster->y] != EMPTY_CLOUD)
        {
            if (monster->has_ench(ENCH_SUBMERGED))
            {
                monster->speed_increment -= entry->energy_usage.swim;
                break;
            }

            if (monster->type == -1)
            {
                monster->speed_increment -= entry->energy_usage.move;
                break;  // problem with vortices
            }

            _mons_in_cloud(monster);

            if (monster->type == -1)
            {
                monster->speed_increment = 1;
                break;
            }
        }

        if (monster->type == MONS_TIAMAT && one_chance_in(3))
        {
            int cols[] = { RED, WHITE, DARKGREY, GREEN, MAGENTA };
            int newcol = cols[random2(sizeof(cols) / sizeof(cols[0]))];
            monster->colour = newcol;
        }

        _monster_regenerate(monster);

        if (mons_is_paralysed(monster))
        {
            monster->speed_increment -= non_move_energy;
            continue;
        }

        _handle_behaviour(monster);

        // submerging monsters will hide from clouds
        if (monster_can_submerge(monster, grd[monster->x][monster->y])
            && env.cgrid[monster->x][monster->y] != EMPTY_CLOUD)
        {
            monster->add_ench(ENCH_SUBMERGED);
        }

        if (monster->speed >= 100)
        {
            monster->speed_increment -= non_move_energy;
            continue;
        }

        if (mons_is_zombified(monster)
            && monster->type != MONS_SPECTRAL_THING)
        {
            monster->max_hit_points = monster->hit_points;
        }

        if (igrd[monster->x][monster->y] != NON_ITEM
            && (mons_itemuse(monster->type) == MONUSE_WEAPONS_ARMOUR
                || mons_itemuse(monster->type) == MONUSE_EATS_ITEMS
                || monster->type == MONS_NECROPHAGE
                || monster->type == MONS_GHOUL))
        {
            // keep permanent friendlies from picking up stuff
            if (monster->attitude != ATT_FRIENDLY)
            {
                if (_handle_pickup(monster))
                {
                    DEBUG_ENERGY_USE("handle_pickup()");
                    continue;
                }
            }
        }

        if (mons_is_caught(monster))
            // Struggling against the net takes time.
            _swim_or_move_energy(monster);
        else
        {
            // calculates mmov_x, mmov_y based on monster target.
            _handle_movement(monster);

            brkk = false;

             if (mons_is_confused( monster )
                 || (monster->type == MONS_AIR_ELEMENTAL
                     && monster->has_ench(ENCH_SUBMERGED)))
             {
                 std::vector<coord_def> moves;

                 int pfound = 0;
                 for (int yi = -1; yi <= 1; ++yi)
                     for (int xi = -1; xi <= 1; ++xi)
                     {
                         coord_def c = monster->pos() + coord_def(xi, yi);
                         if (in_bounds(c) && monster->can_pass_through(c)
                             && one_chance_in(++pfound))
                         {
                             mmov_x = xi;
                             mmov_y = yi;
                         }
                     }

                 if (random2(2 + pfound) < 2)
                     mmov_x = mmov_y = 0;

                 // bounds check: don't let confused monsters try to run
                 // off the map
                 if (monster->x + mmov_x < 0
                    || monster->x + mmov_x >= GXM)
                 {
                     mmov_x = 0;
                 }

                 if (monster->y + mmov_y < 0
                         || monster->y + mmov_y >= GYM)
                 {
                     mmov_y = 0;
                 }

                 if (!monster->can_pass_through(monster->x + mmov_x,
                                                monster->y + mmov_y))
                 {
                     mmov_x = mmov_y = 0;
                 }

                 if (mgrd[monster->x + mmov_x][monster->y + mmov_y] != NON_MONSTER
                     && !is_sanctuary(monster->x, monster->y)
                     && (mmov_x != 0 || mmov_y != 0))
                 {
                     monsters_fight(i,
                         mgrd[monster->x + mmov_x][monster->y + mmov_y]);

                     brkk = true;
                     mmov_x = 0;
                     mmov_y = 0;
                     DEBUG_ENERGY_USE("monsters_fight()");
                 }
             }

             if (brkk)
                 continue;
        }
        _handle_nearby_ability( monster );

        beem.target_x = monster->target_x;
        beem.target_y = monster->target_y;

        if (monster->behaviour != BEH_SLEEP
            && monster->behaviour != BEH_WANDER

            // berserking monsters are limited to running up and
            // hitting their foes.
            && !monster->has_ench(ENCH_BERSERK))
        {
            // prevents unfriendlies from nuking you from offscreen.
            // How nice!
            if (mons_friendly(monster) || mons_near(monster))
            {
                // [ds] Special abilities shouldn't overwhelm spellcasting
                // in monsters that have both. This aims to give them both
                // roughly the same weight.

                if (coinflip()?
                        _handle_special_ability(monster, beem)
                            || _handle_monster_spell(monster, beem)
                    :   _handle_monster_spell(monster, beem)
                            || _handle_special_ability(monster, beem))
                {
                    DEBUG_ENERGY_USE("spell or special");
                    continue;
                }

                if (_handle_potion(monster, beem))
                {
                    DEBUG_ENERGY_USE("handle_potion()");
                    continue;
                }

                if (_handle_scroll(monster))
                {
                    DEBUG_ENERGY_USE("handle_scroll()");
                    continue;
                }

                if (_handle_wand(monster, beem))
                {
                    DEBUG_ENERGY_USE("handle_wand()");
                    continue;
                }

                if (_handle_reaching(monster))
                {
                    DEBUG_ENERGY_USE("handle_reaching()");
                    continue;
                }
            }

            if (_handle_throw(monster, beem))
            {
                DEBUG_ENERGY_USE("handle_throw()");
                continue;
            }
        }

        if (!mons_is_caught(monster))
        {
            // see if we move into (and fight) an unfriendly monster
            int targmon = mgrd[monster->x + mmov_x][monster->y + mmov_y];
            if (targmon != NON_MONSTER
                && targmon != i
                && !mons_aligned(i, targmon))
            {
                // figure out if they fight
                if (monsters_fight(i, targmon))
                {
                    if (testbits(monster->flags, MF_BATTY))
                    {
                        monster->behaviour = BEH_WANDER;
                        monster->target_x = 10 + random2(GXM - 10);
                        monster->target_y = 10 + random2(GYM - 10);
                        // monster->speed_increment -= monster->speed;
                    }

                    mmov_x = 0;
                    mmov_y = 0;
                    brkk = true;
                    DEBUG_ENERGY_USE("monsters_fight()");
                }
            }

            if (brkk)
                continue;

            if (monster->x + mmov_x == you.x_pos
                && monster->y + mmov_y == you.y_pos)
            {
                bool isFriendly = mons_friendly(monster);
                bool attacked = false;

                if (!isFriendly)
                {
                    monster_attack(i);
                    attacked = true;

                    if (testbits(monster->flags, MF_BATTY))
                    {
                        monster->behaviour = BEH_WANDER;
                        monster->target_x = 10 + random2(GXM - 10);
                        monster->target_y = 10 + random2(GYM - 10);
                    }
                    DEBUG_ENERGY_USE("monster_attack()");
                }

                if ((monster->type == MONS_GIANT_SPORE
                        || monster->type == MONS_BALL_LIGHTNING)
                    && monster->hit_points < 1)
                {

                    // detach monster from the grid first, so it
                    // doesn't get hit by its own explosion (GDL)
                    mgrd[monster->x][monster->y] = NON_MONSTER;

                    spore_goes_pop(monster);
                    monster_cleanup(monster);
                    continue;
                }

                if (attacked)
                {
                    mmov_x = 0;
                    mmov_y = 0;
                    continue;   //break;
                }
            }

            if (invalid_monster(monster) || mons_is_stationary(monster))
            {
                if (monster->speed_increment == old_energy)
                    monster->speed_increment -= non_move_energy;
                continue;
            }

            if (!_monster_move(monster))
                monster->speed_increment -= non_move_energy;
        }
        update_beholders(monster);

        // reevaluate behaviour, since the monster's
        // surroundings have changed (it may have moved,
        // or died for that matter.  Don't bother for
        // dead monsters.  :)
        if (monster->type != -1)
            _handle_behaviour(monster);

    }                   // end while

    if (monster->type != -1 && monster->hit_points < 1)
    {
        if (monster->type == MONS_GIANT_SPORE
            || monster->type == MONS_BALL_LIGHTNING)
        {
            // detach monster from the grid first, so it
            // doesn't get hit by its own explosion (GDL)
            mgrd[monster->x][monster->y] = NON_MONSTER;

            spore_goes_pop( monster );
            monster_cleanup( monster );
            return;
        }
        else
        {
            monster_die( monster, KILL_MISC, 0 );
        }
    }
}

//---------------------------------------------------------------
//
// handle_monsters
//
// This is the routine that controls monster AI.
//
//---------------------------------------------------------------
void handle_monsters(void)
{
    // Keep track of monsters that have already moved and don't allow
    // them to move again.
    memset(immobile_monster, 0, sizeof immobile_monster);

    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        monsters *monster = &menv[i];

        if (monster->type == -1 || immobile_monster[i])
            continue;

        const int mx = monster->x,
                  my = monster->y;
        _handle_monster_move(i, monster);

        if (!invalid_monster(monster)
            && (monster->x != mx || monster->y != my))
        {
            immobile_monster[i] = true;
        }

        // If the player got banished, discard pending monster actions.
        if (you.banished)
        {
            // clear list of beholding monsters
            if (you.duration[DUR_BEHELD])
            {
                you.beheld_by.clear();
                you.duration[DUR_BEHELD] = 0;
            }
            break;
        }
    }                           // end of for loop

    // Clear any summoning flags so that lower indiced
    // monsters get their actions in the next round.
    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        menv[i].flags &= ~MF_JUST_SUMMONED;
    }
}                               // end handle_monster()

static bool _is_item_jelly_edible(const item_def &item)
{
    // don't eat artefacts (note that unrandarts are randarts)
    if (is_fixed_artefact(item) || is_random_artefact(item))
        return (false);

    // shouldn't eat stone things
    //    - but what about wands and rings?
    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return (false);
    }

    // don't eat special game items
    if (item.base_type == OBJ_ORBS
        || (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_HORN_OF_GERYON)))
    {
        return (false);
    }

    return (true);
}

//---------------------------------------------------------------
//
// handle_pickup
//
// Returns false if monster doesn't spend any time pickup up
//
//---------------------------------------------------------------
static bool _handle_pickup(monsters *monster)
{
    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);
    int  item = NON_ITEM;

    if (monster->has_ench(ENCH_SUBMERGED))
        return (false);

    if (monster->behaviour == BEH_SLEEP)
        return (false);

    if (mons_itemuse(monster->type) == MONUSE_EATS_ITEMS)
    {
        int  midx       = monster_index(monster);
        int  hps_gained = 0;
        int  max_eat    = roll_dice( 1, 10 );
        int  eaten      = 0;
        bool eaten_net  = false;

        for (item = igrd[monster->x][monster->y];
             item != NON_ITEM && eaten < max_eat && hps_gained < 50;
             item = mitm[item].link)
        {
            int quant = mitm[item].quantity;

            if (!_is_item_jelly_edible(mitm[item]))
                continue;

            if (mitm[igrd[monster->x][monster->y]].base_type != OBJ_GOLD)
            {
                if (quant > max_eat - eaten)
                    quant = max_eat - eaten;

                hps_gained += (quant * item_mass( mitm[item] )) / 20 + quant;
                eaten += quant;

                if (mons_is_caught(monster)
                    && mitm[item].base_type == OBJ_MISSILES
                    && mitm[item].sub_type == MI_THROWING_NET
                    && item_is_stationary(mitm[item]))
                {
                    monster->del_ench(ENCH_HELD, true);
                    eaten_net = true;
                }
            }
            else
            {
                // shouldn't be much trouble to digest a huge pile of gold!
                if (quant > 500)
                    quant = 500 + roll_dice( 2, (quant - 500) / 2 );

                hps_gained += quant / 10 + 1;
                eaten++;
            }

            if (quant >= mitm[item].quantity)
                item_was_destroyed(mitm[item], midx);

            dec_mitm_item_quantity( item, quant );
        }

        if (eaten)
        {
            if (hps_gained < 1)
                hps_gained = 1;
            else if (hps_gained > 50)
                hps_gained = 50;

            // This is done manually instead of using heal_monster(),
            // because that function doesn't work quite this way.  -- bwr
            monster->hit_points += hps_gained;

            if (monster->max_hit_points < monster->hit_points)
                monster->max_hit_points = monster->hit_points;

            if (!silenced(you.x_pos, you.y_pos)
                && !silenced(monster->x, monster->y))
            {
                mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
                     monsterNearby ? "" : " distant");
            }
            if (eaten_net)
                simple_monster_message(monster, " devours the net!");

            if (mons_class_flag( monster->type, M_SPLITS ))
            {
                const int reqd = (monster->hit_dice <= 6)
                                            ? 50 : monster->hit_dice * 8;

                if (monster->hit_points >= reqd)
                    _jelly_divide(monster);
            }
        }

        return (false);
    }                           // end "if jellies"

    // Note: Monsters only look at stuff near the top of stacks.

    for (item = igrd[monster->x][monster->y]; item != NON_ITEM; )
    {
        item_def &topickup = mitm[item];
        item = topickup.link;
        if (monster->pickup_item(topickup, monsterNearby))
            return (true);
        if (coinflip())
            break;
    }
    return (false);
}                               // end handle_pickup()

static void _jelly_grows(monsters *monster)
{
    if (!silenced(you.x_pos, you.y_pos)
        && !silenced(monster->x, monster->y))
    {
        mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
             mons_near(monster) ? "" : " distant");
    }

    monster->hit_points += 5;

    // note here, that this makes jellies "grow" {dlb}:
    if (monster->hit_points > monster->max_hit_points)
        monster->max_hit_points = monster->hit_points;

    if (mons_class_flag( monster->type, M_SPLITS ))
    {
        // and here is where the jelly might divide {dlb}
        const int reqd = (monster->hit_dice < 6) ? 50
                                                 : monster->hit_dice * 8;

        if (monster->hit_points >= reqd)
            _jelly_divide(monster);
    }
}

static bool _mons_can_displace(const monsters *mpusher, const monsters *mpushee)
{
    if (invalid_monster(mpusher) || invalid_monster(mpushee))
        return (false);

    const int ipushee = monster_index(mpushee);
    if (invalid_monster_index(ipushee))
        return (false);

    if (immobile_monster[ipushee])
        return (false);

    // Confused monsters can't be pushed past, sleeping monsters
    // can't push. Note that sleeping monsters can't be pushed
    // past, either, but they may be woken up by a crowd trying to
    // elbow past them, and the wake-up check happens downstream.
    if (mons_is_confused(mpusher) || mons_is_confused(mpushee)
        || mons_is_paralysed(mpusher) || mons_is_paralysed(mpushee)
        || mons_is_sleeping(mpusher) || mons_is_stationary(mpusher)
        || mons_is_stationary(mpushee))
    {
        return (false);
    }

    // Batty monsters are unpushable
    if (mons_is_batty(mpusher) || mons_is_batty(mpushee))
        return (false);

    if (!monster_shover(mpusher))
        return (false);

    if (!monster_senior(mpusher, mpushee))
        return (false);

    return (true);
}

static bool _monster_swaps_places( monsters *mon, int mx, int my )
{
    if (!mx && !my)
        return (false);

    int targmon = mgrd[mon->x + mx][mon->y + my];
    if (targmon == MHITNOT || targmon == MHITYOU)
        return (false);

    monsters *m2 = &menv[targmon];
    if (!_mons_can_displace(mon, m2))
        return (false);

    if (mons_is_sleeping(m2))
    {
        if (one_chance_in(2))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "Alerting monster %s at (%d,%d)",
                 m2->name(DESC_PLAIN).c_str(), m2->x, m2->y);
#endif
            behaviour_event( m2, ME_ALERT, MHITNOT );
        }
        return (false);
    }

    // Check that both monsters will be happy at their proposed new locations.
    const int cx = mon->x, cy = mon->y,
              nx = mon->x + mx, ny = mon->y + my;
    if (!_habitat_okay(mon, grd[nx][ny])
        || !_habitat_okay(m2, grd[cx][cy]))
    {
        return (false);
    }

    // Okay, do the swap!
    _swim_or_move_energy(mon);

    mon->x = nx;
    mon->y = ny;
    mgrd[nx][ny] = monster_index(mon);

    m2->x  = cx;
    m2->y  = cy;
    const int m2i = monster_index(m2);
    ASSERT(m2i >= 0 && m2i < MAX_MONSTERS);
    mgrd[cx][cy] = m2i;
    immobile_monster[m2i] = true;

    mon->check_redraw(coord_def(cx, cy));
    mon->apply_location_effects();
    m2->apply_location_effects();

    return (false);
}

static bool _do_move_monster(monsters *monster, int xi, int yi)
{
    const int fx = monster->x + xi,
        fy = monster->y + yi;

    if (!in_bounds(fx, fy))
        return false;

    if (fx == you.x_pos && fy == you.y_pos)
    {
        monster_attack( monster_index(monster) );
        return true;
    }

    if (!xi && !yi)
    {
        const int mx = monster_index(monster);
        monsters_fight( mx, mx );
        return true;
    }

    if (mgrd[fx][fy] != NON_MONSTER)
    {
        monsters_fight( monster_index(monster), mgrd[fx][fy] );
        return true;
    }

    if (!xi && !yi)
        return false;

    // this appears to be the real one, ie where the movement occurs:
    _swim_or_move_energy(monster);

    mgrd[monster->x][monster->y] = NON_MONSTER;

    monster->x = fx;
    monster->y = fy;

    // need to put in something so that monster picks up multiple
    // items (eg ammunition) identical to those it's carrying.
    mgrd[monster->x][monster->y] = monster_index(monster);

    monster->check_redraw(monster->pos() - coord_def(xi, yi));
    monster->apply_location_effects();

    return true;
}

void mons_check_pool(monsters *mons, killer_type killer, int killnum)
{
    // Levitating/flying monsters don't make contact with the terrain.
    if (mons->airborne())
        return;

    dungeon_feature_type grid = grd(mons->pos());
    if ((grid == DNGN_LAVA || grid == DNGN_DEEP_WATER)
        && !monster_habitable_grid(mons, grid))
    {
        const bool message = mons_near(mons);

        // don't worry about invisibility - you should be able to
        // see if something has fallen into the lava
        if (message)
            mprf("%s falls into the %s!",
                 mons->name(DESC_CAP_THE).c_str(),
                 (grid == DNGN_LAVA ? "lava" : "water"));

        if (grid == DNGN_LAVA && mons_res_fire(mons) >= 2)
            grid = DNGN_DEEP_WATER;

        // Even fire resistant monsters perish in lava, but undead can survive
        // deep water.
        if (grid == DNGN_LAVA || mons->can_drown())
        {
            if (message)
            {
                if (grid == DNGN_LAVA)
                {
                    simple_monster_message( mons, " is incinerated.",
                                            MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else if (mons_genus(mons->type) == MONS_MUMMY)
                {
                    simple_monster_message( mons, " falls apart.",
                                            MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
                else
                {
                    simple_monster_message( mons, " drowns.",
                                            MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                }
            }

            if (killer == KILL_NONE)
            {
                // Self-kill.
                killer  = KILL_MON;
                killnum = monster_index(mons);
            }
            monster_die(mons, killer, killnum, true);
        }
    }
}

// returns true for monsters that obviously (to the player) feel
// "thematically at home" in a branch
// currently used for native monsters recognizing traps
static bool _is_native_in_branch(const monsters *monster,
                                 const branch_type branch)
{
    switch (branch)
    {
    case BRANCH_ELVEN_HALLS:
        return (mons_species(monster->type) == MONS_ELF);

    case BRANCH_ORCISH_MINES:
        return (mons_species(monster->type) == MONS_ORC);

    case BRANCH_SHOALS:
        return (mons_species(monster->type) == MONS_CYCLOPS
                || mons_species(monster->type) == MONS_MERFOLK
                || mons_species(monster->type) == MONS_MERMAID);

    case BRANCH_SLIME_PITS:
        return (mons_species(monster->type) == MONS_JELLY);

    case BRANCH_SNAKE_PIT:
        return (mons_species(monster->type) == MONS_NAGA
                || mons_species(monster->type) == MONS_SNAKE);

    case BRANCH_HALL_OF_ZOT:
        return (mons_species(monster->type) == MONS_DRACONIAN);

    case BRANCH_TOMB:
        return (mons_species(monster->type) == MONS_MUMMY);

    case BRANCH_HIVE:
        return (monster->type == MONS_KILLER_BEE_LARVA
                || monster->type == MONS_KILLER_BEE
                || monster->type == MONS_QUEEN_BEE);

    case BRANCH_HALL_OF_BLADES:
        return (monster->type == MONS_DANCING_WEAPON);

    default:
        return false;
    }
}

// randomize potential damage
static int _estimated_trap_damage(trap_type trap)
{
    switch (trap)
    {
        case TRAP_BLADE:
           return (10 + random2(30));
        case TRAP_DART:
           return (random2(4));
        case TRAP_ARROW:
           return (random2(7));
        case TRAP_SPEAR:
           return (random2(10));
        case TRAP_BOLT:
           return (random2(13));
        case TRAP_AXE:
           return (random2(15));
        default:
           return (0);
    }
}

static bool _mon_can_move_to_pos(const monsters *monster, const int count_x,
                                 const int count_y, bool just_check = false);

// Check whether a given trap (described by trap position) can be
// regarded as safe. Takes in account monster intelligence and allegiance.
// (just_check is used for intelligent monsters trying to avoid traps.)
static bool _is_trap_safe(const monsters *monster, const int trap_x,
                          const int trap_y, bool just_check = false)
{
    const int intel = mons_intel(monster->type);

    // Dumb monsters don't care at all.
    if (intel == I_PLANT)
        return (true);

    const trap_struct &trap = env.trap[trap_at_xy(trap_x,trap_y)];

    if (trap.type == TRAP_SHAFT && monster->will_trigger_shaft())
    {
        if (mons_is_fleeing(monster) && intel >= I_NORMAL)
            return (true);
        return (false);
    }

    // Monsters are not afraid of non-mechanical traps. XXX: If we add
    // any non-mechanical traps that can damage monsters, must add
    // check here.
    const bool mechanical = trap_category(trap.type) == DNGN_TRAP_MECHANICAL;

    const bool player_knows_trap = (grd[trap_x][trap_y] != DNGN_UNDISCOVERED_TRAP);

    // Smarter trap handling for intelligent monsters
    // * monsters native to a branch can be assumed to know the trap
    //   locations and thus be able to avoid them
    // * permanent friendlies can be assumed to have been warned by the
    //   player about all traps s/he knows about
    // * very intelligent monsters can be assumed to have a high T&D skill
    //   (or have memorised part of the dungeon layout ;) )
    if (intel >= I_NORMAL && mechanical
        && (_is_native_in_branch(monster, you.where_are_you)
            || monster->attitude == ATT_FRIENDLY
               && player_knows_trap
            || intel >= I_HIGH && one_chance_in(3)))
    {
        if (just_check)
            return false; // square is blocked
        else
        {
            // test for corridor-like environment
            const int x = trap_x - monster->x;
            const int y = trap_y - monster->y;

            // The question is whether the monster (m) can easily reach its
            // presumable destination (x) without stepping on the trap. Traps
            // in corridors do not allow this. See e.g
            //  #x#        ##
            //  #^#   or  m^x
            //   m         ##
            //
            // The same problem occurs if paths are blocked by monsters,
            // hostile terrain or other traps rather than walls.
            // What we do is check whether the squares with the relative
            // positions (-1,0)/(+1,0) or (0,-1)/(0,+1) form a "corridor"
            // (relative to the _current_ monster position rather than the
            // trap one) form a corridor-like environment. If they don't
            // the trap square is marked as "unsafe", otherwise the decision
            // will be made according to later tests (monster hp, trap type, ...)
            // If a monster still gets stuck in a corridor it will usually be
            // because it has less than half its maximum hp

            if ((_mon_can_move_to_pos(monster, x-1, y, true)
                   || _mon_can_move_to_pos(monster, x+1,y, true))
                && (_mon_can_move_to_pos(monster, x,y-1, true)
                    || _mon_can_move_to_pos(monster, x,y+1, true)))
            {
                return (false);
            }
        }
    }

    // friendlies will try not to be parted from you
    if (intelligent_ally(monster) && trap.type == TRAP_TELEPORT
        && player_knows_trap && mons_near(monster))
    {
        return (false);
    }

    // Healthy monsters don't mind a little pain.
    if (mechanical && monster->hit_points >= monster->max_hit_points / 2
        && (intel == I_ANIMAL
            || monster->hit_points > _estimated_trap_damage(trap.type)))
    {
        return (true);
    }

    // Friendly monsters don't enjoy Zot trap perks, handle accordingly.
    if (mons_friendly(monster))
        return (mechanical? mons_flies(monster) : trap.type != TRAP_ZOT);
    else
        return (!mechanical || mons_flies(monster));
}

static void _mons_open_door(monsters* monster, const coord_def &pos)
{
    dungeon_feature_type grid = grd(pos);
    const char *adj = "", *noun = "door";

    bool was_secret = false;

    if (grid == DNGN_SECRET_DOOR)
    {
        grid = grid_secret_door_appearance(pos.x, pos.y);
        grd(pos) = DNGN_OPEN_DOOR; // just a simple door, no gates etc.

        was_secret = true;
        if (!see_grid(pos))
            set_terrain_changed(pos);
    }
    else // maybe several connected doors -> gate
    {
        std::set<coord_def> all_door;
        find_connected_identical(pos, grd(pos), all_door);
        get_door_description(all_door.size(), &adj, &noun);

        for (std::set<coord_def>::iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
             const coord_def& dc = *i;
             grd[dc.x][dc.y] = DNGN_OPEN_DOOR;
        }
    }

    if (see_grid(pos))
    {
        viewwindow(true, false);

        if (was_secret)
        {
            mprf("%s was actually a secret door!",
                 feature_description(grid, NUM_TRAPS, false,
                                     DESC_CAP_THE, false).c_str());
        }

        if (!you.can_see(monster))
        {
            mprf("Something unseen opens the %s%s.", adj, noun);
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
    }

    monster->lose_energy(EUT_MOVE);
}

// Check whether a monster can move to given square (described by its relative
// coordinates to the current monster position). just_check is true only for
// calls from is_trap_safe when checking the surrounding squares of a trap.
bool _mon_can_move_to_pos(const monsters *monster, const int count_x,
                         const int count_y, bool just_check)
{
    const int targ_x = monster->x + count_x;
    const int targ_y = monster->y + count_y;

    // bounds check - don't consider moving out of grid!
    if (!inside_level_bounds(targ_x, targ_y))
        return false;

    // non-friendly and non-good neutral monsters won't enter
    // sanctuaries
    if (!mons_wont_attack(monster)
        && is_sanctuary(targ_x, targ_y)
        && !is_sanctuary(monster->x, monster->y))
    {
        return (false);
    }
    // inside a sanctuary don't attack anything!
    if (is_sanctuary(monster->x, monster->y)
        && (targ_x == you.x_pos && targ_y == you.y_pos
            || mgrd[targ_x][targ_y] != NON_MONSTER))
    {
        return (false);
    }

    const dungeon_feature_type target_grid = grd[targ_x][targ_y];
    const habitat_type habitat = mons_habitat(monster);

    // effectively slows down monster movement across water.
    // Fire elementals can't cross at all.
    bool no_water = false;
    if (monster->type == MONS_FIRE_ELEMENTAL || one_chance_in(5))
        //okmove = DNGN_WATER_STUCK;
        no_water = true;

    const int targ_cloud_num  = env.cgrid[ targ_x ][ targ_y ];
    const int targ_cloud_type =
        targ_cloud_num == EMPTY_CLOUD? CLOUD_NONE
                                     : env.cloud[targ_cloud_num].type;

    const int curr_cloud_num = env.cgrid[ monster->x ][ monster->y ];
    const int curr_cloud_type =
        curr_cloud_num == EMPTY_CLOUD? CLOUD_NONE
                                     : env.cloud[curr_cloud_num].type;

    if (monster->type == MONS_BORING_BEETLE
        && (target_grid == DNGN_ROCK_WALL
            || target_grid == DNGN_CLEAR_ROCK_WALL))
    {
        // don't burrow out of bounds
        if (targ_x <= 7 || targ_x >= (GXM - 8)
            || targ_y <= 7 || targ_y >= (GYM - 8))
        {
            return false;
        }

        // don't burrow at an angle (legacy behaviour)
        if (count_x != 0 && count_y != 0)
        {
            return false;
        }
    }
    else if (!monster->can_pass_through_feat(target_grid)
             || (no_water && target_grid >= DNGN_DEEP_WATER
                 && target_grid <= DNGN_WATER_STUCK))
    {
        return false;
    }
    else if (!_habitat_okay( monster, target_grid ))
    {
        return false;
    }

    if (monster->type == MONS_WANDERING_MUSHROOM
        && see_grid(targ_x, targ_y))
    {
        return false;
    }

    // Water elementals avoid fire and heat
    if (monster->type == MONS_WATER_ELEMENTAL
        && (target_grid == DNGN_LAVA
            || targ_cloud_type == CLOUD_FIRE
            || targ_cloud_type == CLOUD_STEAM))
    {
        return false;
    }

    // Fire elementals avoid water and cold
    if (monster->type == MONS_FIRE_ELEMENTAL
        && (grid_is_watery(target_grid)
            || targ_cloud_type == CLOUD_COLD))
    {
        return false;
    }

    // Submerged water creatures avoid the shallows where
    // they would be forced to surface. -- bwr
    // [dshaligram] Monsters now prefer to head for deep water only if
    // they're low on hitpoints. No point in hiding if they want a
    // fight.
    if (habitat == HT_WATER
        && (targ_x != you.x_pos || targ_y != you.y_pos)
        && target_grid != DNGN_DEEP_WATER
        && grd[monster->x][monster->y] == DNGN_DEEP_WATER
        && monster->hit_points < (monster->max_hit_points * 3) / 4)
    {
        return false;
    }

    // smacking the player is always a good move if we're
    // hostile (even if we're heading somewhere else)
    // also friendlies want to keep close to the player
    // so it's okay as well

    // smacking another monster is good, if the monsters
    // are aligned differently
    if (mgrd[targ_x][targ_y] != NON_MONSTER)
    {
        if (just_check)
        {
            if (targ_x == monster->x && targ_y == monster->y)
                return true;

            return false; // blocks square
        }

        const int thismonster = monster_index(monster),
                  targmonster = mgrd[targ_x][targ_y];
        if (mons_aligned(thismonster, targmonster)
            && targmonster != MHITNOT
            && targmonster != MHITYOU
            && !_mons_can_displace(monster, &menv[targmonster]))
        {
            return false;
        }
    }

    // friendlies shouldn't try to move onto the player's
    // location, if they are aiming for some other target
    if (mons_wont_attack(monster)
        && monster->foe != MHITNOT
        && monster->foe != MHITYOU
        && targ_x == you.x_pos
        && targ_y == you.y_pos)
    {
        return false;
    }

    // wandering through a trap is OK if we're pretty healthy,
    // really stupid, or immune to the trap
    const int which_trap = trap_at_xy(targ_x,targ_y);
    if (which_trap >= 0 && !_is_trap_safe(monster, targ_x, targ_y, just_check))
    {
        return false;
    }

    if (targ_cloud_num != EMPTY_CLOUD)
    {
        if (curr_cloud_num != EMPTY_CLOUD
            && targ_cloud_type == curr_cloud_type)
        {
            return true;
        }

        switch (targ_cloud_type)
        {
        case CLOUD_MIASMA:
            // Even the dumbest monsters will avoid miasma if they can.
            return (mons_res_miasma(monster) > 0);

        case CLOUD_FIRE:
            if (mons_res_fire(monster) > 1)
                return true;

            if (monster->hit_points >= 15 + random2avg(46, 5))
                return true;
            break;

        case CLOUD_STINK:
            if (mons_res_poison(monster) > 0)
                return true;
            if (1 + random2(5) < monster->hit_dice)
                return true;
            if (monster->hit_points >= random2avg(19, 2))
                return true;
            break;

        case CLOUD_COLD:
            if (mons_res_cold(monster) > 1)
                return true;

            if (monster->hit_points >= 15 + random2avg(46, 5))
                return true;
            break;

        case CLOUD_POISON:
            if (mons_res_poison(monster) > 0)
                return true;

            if (monster->hit_points >= random2avg(37, 4))
                return true;
            break;

            // this isn't harmful, but dumb critters might think so.
        case CLOUD_GREY_SMOKE:
            if (mons_intel(monster->type) > I_ANIMAL || coinflip())
                return true;

            if (mons_res_fire(monster) > 0)
                return true;

            if (monster->hit_points >= random2avg(19, 2))
                return true;
            break;

        default:
            return true;   // harmless clouds
        }

        // if we get here, the cloud is potentially harmful.
        // exceedingly dumb creatures will still wander in.
        if (mons_intel(monster->type) != I_PLANT)
            return false;
    }

    // if we end up here the monster can safely move
    return true;
}

static bool _monster_move(monsters *monster)
{
    FixedArray < bool, 3, 3 > good_move;
    int count_x, count_y, count;
    int okmove = DNGN_SHALLOW_WATER; // what does this actually do?

    const habitat_type habitat = mons_habitat(monster);
    bool deep_water_available = false;

    // Berserking monsters make a lot of racket
    if (monster->has_ench(ENCH_BERSERK))
    {
        int noise_level = get_shout_noise_level(mons_shouts(monster->type));
        if (noise_level > 0)
        {
            if (mons_near(monster) && player_monster_visible(monster))
            {
                if (one_chance_in(10))
                {
                    mprf(MSGCH_TALK_VISUAL, "%s rages.",
                         monster->name(DESC_CAP_THE).c_str());
                }
                noisy( noise_level, monster->x, monster->y );
            }
            else if (one_chance_in(5))
            {
                handle_monster_shouts(monster, true);
            }
            else // just be noisy without messaging the player
                noisy( noise_level, monster->x, monster->y );
        }
    }

    if (monster->confused())
    {
        if (mmov_x || mmov_y || one_chance_in(15))
        {
            coord_def newpos = monster->pos() + coord_def(mmov_x, mmov_y);
            if (in_bounds(newpos)
                && (habitat == HT_LAND
                    || monster_habitable_grid(monster, grd(newpos))))
            {
                return _do_move_monster(monster, mmov_x, mmov_y);
            }
        }
        return false;
    }

    // let's not even bother with this if mmov_x and mmov_y are zero.
    if (mmov_x == 0 && mmov_y == 0)
        return false;

    if (mons_flies(monster) != FL_NONE
        || habitat != HT_LAND
        || mons_amphibious(monster->type))
    {
        okmove = DNGN_MINMOVE;
    }

    for (count_x = 0; count_x < 3; count_x++)
        for (count_y = 0; count_y < 3; count_y++)
        {
             const int targ_x = monster->x + count_x - 1;
             const int targ_y = monster->y + count_y - 1;

             // [ds] Bounds check was after grd[targ_x][targ_y] which would
             // trigger an ASSERT. Moved it up.

             // bounds check - don't consider moving out of grid!
             if (targ_x < 0 || targ_x >= GXM || targ_y < 0 || targ_y >= GYM)
             {
                 good_move[count_x][count_y] = false;
                 continue;
             }
             dungeon_feature_type target_grid = grd[targ_x][targ_y];

             if (target_grid == DNGN_DEEP_WATER)
                 deep_water_available = true;

             const monsters* mons = dynamic_cast<const monsters*>(monster);
             good_move[count_x][count_y] =
                 _mon_can_move_to_pos(mons, count_x-1, count_y-1);
        } // now we know where we _can_ move.

    const coord_def newpos = monster->pos() + coord_def(mmov_x, mmov_y);
    // normal/smart monsters know about secret doors (they _live_ in the
    // dungeon!)
    if (grd(newpos) == DNGN_CLOSED_DOOR
        || (grd(newpos) == DNGN_SECRET_DOOR
            && (mons_intel(monster_index(monster)) == I_HIGH
                || mons_intel(monster_index(monster)) == I_NORMAL)))
    {
        if (mons_is_zombified(monster))
        {
            // for zombies, monster type is kept in mon->number
            if (mons_itemuse(monster->number) >= MONUSE_OPEN_DOORS)
            {
                _mons_open_door(monster, newpos);
                return true;
            }
        }
        else if (mons_itemuse(monster->type) >= MONUSE_OPEN_DOORS)
        {
            _mons_open_door(monster, newpos);
            return true;
        }
    } // endif - secret/closed doors

    // jellies eat doors.  Yum!
    if ((grd[monster->x + mmov_x][monster->y + mmov_y] == DNGN_CLOSED_DOOR
            || grd[monster->x + mmov_x][monster->y + mmov_y] == DNGN_OPEN_DOOR)
        && mons_itemuse(monster->type) == MONUSE_EATS_ITEMS)
    {
        grd[monster->x + mmov_x][monster->y + mmov_y] = DNGN_FLOOR;

        _jelly_grows(monster);

        if (see_grid(monster->x + mmov_x, monster->y + mmov_y))
        {
            viewwindow(true, false);

            if (!you.can_see(monster))
            {
                mpr("The door mysteriously vanishes.");
                interrupt_activity( AI_FORCE_INTERRUPT );
            }
        }
    } // done door-eating jellies


    // water creatures have a preference for water they can hide in -- bwr
    // [ds] Weakened the powerful attraction to deep water if the monster
    // is in good health.
    if (habitat == HT_WATER
        && deep_water_available
        && grd[monster->x][monster->y] != DNGN_DEEP_WATER
        && grd[monster->x + mmov_x][monster->y + mmov_y] != DNGN_DEEP_WATER
        && (monster->x + mmov_x != you.x_pos
            || monster->y + mmov_y != you.y_pos)
        && (one_chance_in(3)
            || monster->hit_points <= (monster->max_hit_points * 3) / 4))
    {
        count = 0;

        for (count_x = 0; count_x < 3; count_x++)
        {
            for (count_y = 0; count_y < 3; count_y++)
            {
                if (good_move[count_x][count_y]
                    && grd[monster->x + count_x - 1][monster->y + count_y - 1]
                            == DNGN_DEEP_WATER)
                {
                    count++;

                    if (one_chance_in( count ))
                    {
                        mmov_x = count_x - 1;
                        mmov_y = count_y - 1;
                    }
                }
            }
        }
    }

    // now, if a monster can't move in its intended direction, try
    // either side.  If they're both good, move in whichever dir
    // gets it closer(farther for fleeing monsters) to its target.
    // If neither does, do nothing.
    if (good_move[mmov_x + 1][mmov_y + 1] == false)
    {
        int current_distance = grid_distance( monster->x, monster->y,
                                              monster->target_x,
                                              monster->target_y );

        int dir = -1;
        int i, mod, newdir;

        for (i = 0; i < 8; i++)
        {
            if (compass_x[i] == mmov_x && compass_y[i] == mmov_y)
            {
                dir = i;
                break;
            }
        }

        if (dir < 0)
            goto forget_it;

        int dist[2];

        // first 1 away, then 2 (3 is silly)
        for (int j = 1; j <= 2; j++)
        {
            int sdir, inc;

            if (coinflip())
            {
                sdir = -j;
                inc = 2*j;
            }
            else
            {
                sdir = j;
                inc = -2*j;
            }

            // try both directions
            for (mod = sdir, i = 0; i < 2; mod += inc, i++)
            {
                newdir = (dir + 8 + mod) % 8;
                if (good_move[compass_x[newdir] + 1][compass_y[newdir] + 1])
                {
                    dist[i] = grid_distance( monster->x + compass_x[newdir],
                                             monster->y + compass_y[newdir],
                                             monster->target_x,
                                             monster->target_y );
                }
                else
                {
                    dist[i] = (monster->behaviour == BEH_FLEE) ? (-FAR_AWAY)
                                                               : FAR_AWAY;
                }
            }

            // now choose
            if (dist[0] == dist[1] && abs(dist[0]) == FAR_AWAY)
                continue;

            // which one was better? -- depends on FLEEING or not
            if (monster->behaviour == BEH_FLEE)
            {
                if (dist[0] >= dist[1] && dist[0] >= current_distance)
                {
                    mmov_x = compass_x[((dir+8)+sdir)%8];
                    mmov_y = compass_y[((dir+8)+sdir)%8];
                    break;
                }
                if (dist[1] >= dist[0] && dist[1] >= current_distance)
                {
                    mmov_x = compass_x[((dir+8)-sdir)%8];
                    mmov_y = compass_y[((dir+8)-sdir)%8];
                    break;
                }
            }
            else
            {
                if (dist[0] <= dist[1] && dist[0] <= current_distance)
                {
                    mmov_x = compass_x[((dir+8)+sdir)%8];
                    mmov_y = compass_y[((dir+8)+sdir)%8];
                    break;
                }
                if (dist[1] <= dist[0] && dist[1] <= current_distance)
                {
                    mmov_x = compass_x[((dir+8)-sdir)%8];
                    mmov_y = compass_y[((dir+8)-sdir)%8];
                    break;
                }
            }
        }
    } // end - try to find good alternate move

forget_it:

    // ------------------------------------------------------------------
    // if we haven't found a good move by this point, we're not going to.
    // ------------------------------------------------------------------

    // take care of beetle burrowing
    if (monster->type == MONS_BORING_BEETLE)
    {
        dungeon_feature_type feat =
            grd[monster->x + mmov_x][monster->y + mmov_y];
        if ((feat == DNGN_ROCK_WALL || feat == DNGN_ROCK_WALL)
            && good_move[mmov_x + 1][mmov_y + 1] == true)
        {
            grd[monster->x + mmov_x][monster->y + mmov_y] = DNGN_FLOOR;
            set_terrain_changed(monster->pos() + coord_def(mmov_x, mmov_y));

            if (!silenced(you.x_pos, you.y_pos))
                mpr("You hear a grinding noise.", MSGCH_SOUND);
        }
    }

    bool ret = false;
    if (good_move[mmov_x + 1][mmov_y + 1] && !(mmov_x == 0 && mmov_y == 0))
    {
        // check for attacking player
        if (monster->x + mmov_x == you.x_pos
            && monster->y + mmov_y == you.y_pos)
        {
            ret    = monster_attack( monster_index(monster) );
            mmov_x = 0;
            mmov_y = 0;
        }

        // If we're following the player through stairs, the only valid
        // movement is towards the player. -- bwr
        if (testbits( monster->flags, MF_TAKING_STAIRS ))
        {
            const delay_type delay = current_delay_action();
            if (delay != DELAY_ASCENDING_STAIRS
                && delay != DELAY_DESCENDING_STAIRS)
            {
                monster->flags &= ~MF_TAKING_STAIRS;

#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "BUG: %s was marked as follower when not following!",
                     monster->name(DESC_PLAIN).c_str(), true);
#endif
            }
            else
            {
                ret    = true;
                mmov_x = 0;
                mmov_y = 0;

#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "%s is skipping movement in order to follow.",
                     monster->name(DESC_CAP_THE).c_str(), true );
#endif
            }
        }

        // check for attacking another monster
        int targmon = mgrd[monster->x + mmov_x][monster->y + mmov_y];
        if (targmon != NON_MONSTER)
        {
            if (mons_aligned(monster_index(monster), targmon))
                ret = _monster_swaps_places(monster, mmov_x, mmov_y);
            else
            {
                monsters_fight(monster_index(monster), targmon);
                ret = true;
            }

            // If the monster swapped places, the work's already done.
            mmov_x = 0;
            mmov_y = 0;
        }

        if (monster->type == MONS_EFREET
            || monster->type == MONS_FIRE_ELEMENTAL)
        {
            place_cloud( CLOUD_FIRE, monster->x, monster->y,
                         2 + random2(4), monster->kill_alignment() );
        }

        if (monster->type == MONS_ROTTING_DEVIL
            || monster->type == MONS_CURSE_TOE)
        {
            place_cloud( CLOUD_MIASMA, monster->x, monster->y,
                         2 + random2(3), monster->kill_alignment() );
        }
    }
    else
    {
        mmov_x = mmov_y = 0;

        // fleeing monsters that can't move will panic and possibly
        // turn to face their attacker
        if (monster->behaviour == BEH_FLEE)
            behaviour_event(monster, ME_CORNERED);
    }

    if (mmov_x || mmov_y || (monster->confused() && one_chance_in(6)))
        return _do_move_monster(monster, mmov_x, mmov_y);

    return ret;
}                               // end monster_move()

static void _setup_plant_spit(monsters *monster, bolt &pbolt)
{
    pbolt.name = "acid";
    pbolt.type = dchar_glyph(DCHAR_FIRED_ZAP);
    pbolt.range = 9;
    pbolt.rangeMax = 9;
    pbolt.colour = YELLOW;
    pbolt.flavour = BEAM_ACID;
    pbolt.beam_source = monster_index(monster);
    pbolt.damage = dice_def( 3, 7 );
    pbolt.hit = 20 + (3 * monster->hit_dice);
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();
}

static bool _plant_spit(monsters *monster, bolt &pbolt)
{
    bool did_spit = false;

    char spit_string[INFO_SIZE];

    _setup_plant_spit(monster, pbolt);

    // fire tracer
    fire_tracer(monster, pbolt);

    if (mons_should_fire(pbolt))
    {
        strcpy( spit_string, " spits" );
        if (pbolt.target_x == you.x_pos && pbolt.target_y == you.y_pos)
            strcat( spit_string, " at you" );

        strcat( spit_string, "." );
        simple_monster_message( monster, spit_string );

        fire_beam( pbolt );
        did_spit = true;
    }

    return (did_spit);
}                               // end plant_spit()

static void _mons_in_cloud(monsters *monster)
{
    int wc = env.cgrid[monster->x][monster->y];
    int hurted = 0;
    bolt beam;

    const int speed = ((monster->speed > 0) ? monster->speed : 10);
    bool wake = false;

    if (mons_is_mimic( monster->type ))
    {
        mimic_alert(monster);
        return;
    }

    const cloud_struct &cloud(env.cloud[wc]);
    switch (cloud.type)
    {
    case CLOUD_DEBUGGING:
        end(1, false, "Fatal error: monster steps on nonexistent cloud!");
        return;

    case CLOUD_FIRE:
        if (monster->type == MONS_FIRE_VORTEX
            || monster->type == MONS_EFREET
            || monster->type == MONS_FIRE_ELEMENTAL)
        {
            return;
        }

        simple_monster_message(monster, " is engulfed in flame!");

        hurted +=
            resist_adjust_damage( monster,
                                  BEAM_FIRE,
                                  monster->res_fire(),
                                  ((random2avg(16, 3) + 6) * 10) / speed );

        hurted -= random2(1 + monster->ac);
        break;                  // to damage routine at end {dlb}

    case CLOUD_STINK:
        simple_monster_message(monster, " is engulfed in noxious gasses!");

        if (mons_res_poison(monster) > 0)
            return;

        beam.flavour = BEAM_CONFUSION;
        beam.thrower = cloud.beam_thrower();

        if (cloud.whose == KC_FRIENDLY)
            beam.beam_source = ANON_FRIENDLY_MONSTER;

        if (mons_class_is_confusable(monster->type)
            && 1 + random2(27) >= monster->hit_dice)
        {
            mons_ench_f2(monster, beam);
        }

        hurted += (random2(3) * 10) / speed;
        break;                  // to damage routine at end {dlb}

    case CLOUD_COLD:
        simple_monster_message(monster, " is engulfed in freezing vapours!");

        hurted +=
            resist_adjust_damage( monster,
                                  BEAM_COLD,
                                  monster->res_cold(),
                                  ((6 + random2avg(16, 3)) * 10) / speed );

        hurted -= random2(1 + monster->ac);
        break;                  // to damage routine at end {dlb}

    case CLOUD_POISON:
        simple_monster_message(monster, " is engulfed in a cloud of poison!");

        if (mons_res_poison(monster) > 0)
            return;

        poison_monster(monster, cloud.whose);
        // If the monster got poisoned, wake it up.
        wake = true;

        hurted += (random2(8) * 10) / speed;

        if (mons_res_poison(monster) < 0)
            hurted += (random2(4) * 10) / speed;
        break;                  // to damage routine at end {dlb}

    case CLOUD_STEAM:
    {
        // couldn't be bothered coding for armour of res fire

        simple_monster_message(monster, " is engulfed in steam!");

        const int steam_base_damage = steam_cloud_damage(cloud);
        hurted +=
            resist_adjust_damage(
                monster,
                BEAM_STEAM,
                monster->res_steam(),
                (random2avg(steam_base_damage, 2) * 10) / speed);

        hurted -= random2(1 + monster->ac);
        break;                  // to damage routine at end {dlb}
    }

    case CLOUD_MIASMA:
        simple_monster_message(monster, " is engulfed in a dark miasma!");

        if (mons_res_miasma(monster) > 0)
            return;

        poison_monster(monster, cloud.whose);

        if (monster->max_hit_points > 4 && coinflip())
            monster->max_hit_points--;

        beam.flavour = BEAM_SLOW;

        if (one_chance_in(3))
            mons_ench_f2(monster, beam);

        hurted += (10 * random2avg(12, 3)) / speed;    // 3
        break;              // to damage routine at end {dlb}

    default:                // 'harmless' clouds -- colored smoke, etc {dlb}.
        return;
    }

    // A sleeping monster that sustains damage will wake up.
    if ((wake || hurted > 0) && monster->behaviour == BEH_SLEEP)
    {
        // We have no good coords to give the monster as the source of the
        // disturbance other than the cloud itself.
        behaviour_event(monster, ME_DISTURB, MHITNOT, monster->x, monster->y);
    }

    if (hurted < 0)
        hurted = 0;
    else if (hurted > 0)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s takes %d damage from cloud.",
             monster->name(DESC_CAP_THE).c_str(),
             hurted);
#endif
        hurt_monster(monster, hurted);

        if (monster->hit_points < 1)
        {
            mon_enchant death_ench( ENCH_NONE, 0, cloud.whose );
            monster_die(monster, death_ench.killer(), death_ench.kill_agent());
        }
    }
}                               // end mons_in_cloud()

bool monster_descriptor(int which_class, unsigned char which_descriptor)
{
    if (which_descriptor == MDSC_LEAVES_HIDE)
    {
        switch (which_class)
        {
        case MONS_DRAGON:
        case MONS_TROLL:
        case MONS_ICE_DRAGON:
        case MONS_STEAM_DRAGON:
        case MONS_MOTTLED_DRAGON:
        case MONS_STORM_DRAGON:
        case MONS_GOLDEN_DRAGON:
        case MONS_SWAMP_DRAGON:
        case MONS_YAK:
        case MONS_SHEEP:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_REGENERATES)
    {
        switch (which_class)
        {
        case MONS_CACODEMON:
        case MONS_DEEP_TROLL:
        case MONS_HELLWING:
        case MONS_IMP:
        case MONS_IRON_TROLL:
        case MONS_LEMURE:
        case MONS_ROCK_TROLL:
        case MONS_SLIME_CREATURE:
        case MONS_SNORG:
        case MONS_TROLL:
        case MONS_HYDRA:
        case MONS_KILLER_KLOWN:
            return (true);
        default:
            return (false);
        }
    }

    if (which_descriptor == MDSC_NOMSG_WOUNDS)
    {
        if (mons_class_is_zombified(which_class)
            && which_class != MONS_SPECTRAL_THING)
        {
            return (true);
        }

        switch (which_class)
        {
        case MONS_RAKSHASA:
        case MONS_RAKSHASA_FAKE:
            return (true);
        default:
            return (false);
        }
    }
    return (false);
}                               // end monster_descriptor()

bool message_current_target()
{
    if (crawl_state.is_replaying_keys())
    {
        if (you.prev_targ == MHITNOT || you.prev_targ == MHITYOU)
            return false;

        const monsters *montarget = &menv[you.prev_targ];
        return (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU
                && mons_near(montarget) && player_monster_visible(montarget));
    }

    if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
    {
        const monsters *montarget = &menv[you.prev_targ];

        if (mons_near(montarget) && player_monster_visible(montarget))
        {
            mprf( MSGCH_PROMPT, "Current target: %s "
                  "(use p or f to fire at it again.)",
                  montarget->name(DESC_PLAIN).c_str() );
            return (true);
        }

        mpr("You have no current target.");
    }

    return (false);
}

// aaah, the simple joys of pointer arithmetic! {dlb}:
unsigned int monster_index(const monsters *monster)
{
    return (monster - menv.buffer());
}

int hurt_monster(monsters * victim, int damage_dealt)
{
    damage_dealt = std::max(std::min(damage_dealt, victim->hit_points), 0);

    return (victim->hurt(NULL, damage_dealt));
}

bool heal_monster(monsters * patient, int health_boost,
                  bool permit_growth)
{
    if (mons_is_statue(patient->type))
        return (false);

    if (health_boost < 1)
        return (false);
    else if (!permit_growth && patient->hit_points == patient->max_hit_points)
        return (false);

    patient->hit_points += health_boost;

    bool success = true;

    if (patient->hit_points > patient->max_hit_points)
    {
        if (permit_growth)
        {
            const monsterentry* m = get_monster_data(patient->type);
            const int maxhp =
                m->hpdice[0] * (m->hpdice[1] + m->hpdice[2]) + m->hpdice[3];

            // Limit HP growth.
            if (random2(3 * maxhp) > 2 * patient->max_hit_points)
                patient->max_hit_points++;
            else
                success = false;
        }

        patient->hit_points = patient->max_hit_points;
    }

    return (success);
}                               // end heal_monster()

static spell_type _map_wand_to_mspell(int wand_type)
{
    switch (wand_type)
    {
    case WAND_FLAME:
        return SPELL_THROW_FLAME;
    case WAND_FROST:
        return SPELL_THROW_FROST;
    case WAND_SLOWING:
        return SPELL_SLOW;
    case WAND_HASTING:
        return SPELL_HASTE;
    case WAND_MAGIC_DARTS:
        return SPELL_MAGIC_DART;
    case WAND_HEALING:
        return SPELL_LESSER_HEALING;
    case WAND_PARALYSIS:
        return SPELL_PARALYSE;
    case WAND_FIRE:
        return SPELL_BOLT_OF_FIRE;
    case WAND_COLD:
        return SPELL_BOLT_OF_COLD;
    case WAND_CONFUSION:
        return SPELL_CONFUSE;
    case WAND_INVISIBILITY:
        return SPELL_INVISIBILITY;
    case WAND_TELEPORTATION:
        return SPELL_TELEPORT_OTHER;
    case WAND_LIGHTNING:
        return SPELL_LIGHTNING_BOLT;
    case WAND_DRAINING:
        return SPELL_BOLT_OF_DRAINING;
    case WAND_DISINTEGRATION:
        return SPELL_DISINTEGRATE;
    case WAND_POLYMORPH_OTHER:
        return SPELL_POLYMORPH_OTHER;
    default:
        return SPELL_NO_SPELL;
    }
}

void seen_monster(monsters *monster)
{
    if ( monster->flags & MF_SEEN )
        return;

    // First time we've seen this particular monster
    monster->flags |= MF_SEEN;

    if ( !mons_is_mimic(monster->type)
         && MONST_INTERESTING(monster)
         && monster->type != MONS_PANDEMONIUM_DEMON
         && monster->type != MONS_PLAYER_GHOST )
    {
        take_note(
            Note(NOTE_SEEN_MONSTER, monster->type, 0,
                 monster->name(DESC_NOCAP_A).c_str()));
    }
}

//---------------------------------------------------------------
//
// shift_monster
//
// Moves a monster to approximately (x,y) and returns true
// if monster was moved.
//
//---------------------------------------------------------------
bool shift_monster( monsters *mon, int x, int y )
{
    bool found_move = false;

    int i, j;
    int tx, ty;
    int nx = 0, ny = 0;

    int count = 0;

    if (x == 0 && y == 0)
    {
        // try and find a random floor space some distance away
        for (i = 0; i < 50; i++)
        {
            tx = 5 + random2( GXM - 10 );
            ty = 5 + random2( GYM - 10 );

            int dist = grid_distance(x, y, tx, ty);
            if (grd[tx][ty] == DNGN_FLOOR && dist > 10)
                break;
        }

        if (i == 50)
            return (false);
    }

    for (i = -1; i <= 1; i++)
    {
        for (j = -1; j <= 1; j++)
        {
            tx = x + i;
            ty = y + j;

            if (!in_bounds(tx, ty))
                continue;

            // won't drop on anything but vanilla floor right now
            if (grd[tx][ty] != DNGN_FLOOR)
                continue;

            if (mgrd[tx][ty] != NON_MONSTER)
                continue;

            if (tx == you.x_pos && ty == you.y_pos)
                continue;

            if (one_chance_in(++count))
            {
                nx = tx;
                ny = ty;
                found_move = true;
            }
        }
    }

    if (found_move)
    {
        const int mon_index = mgrd[mon->x][mon->y];
        mgrd[mon->x][mon->y] = NON_MONSTER;
        mgrd[nx][ny] = mon_index;
        mon->x = nx;
        mon->y = ny;
    }

    return (found_move);
}

/*
 *  File:       godabil.cc
 *  Summary:    God-granted abilities.
 */

#include "AppHdr.h"

#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "effects.h"
#include "files.h"
#include "godabil.h"
#include "invent.h"
#include "items.h"
#include "kills.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

bool yred_injury_mirror(bool actual)
{
    return (you.religion == GOD_YREDELEMNUL && !player_under_penance()
            && you.piety >= piety_breakpoint(1)
            && (!actual || you.duration[DUR_PRAYER]));
}

bool beogh_water_walk()
{
    return (you.religion == GOD_BEOGH && !player_under_penance()
            && you.piety >= piety_breakpoint(4));
}

bool jiyva_grant_jelly(bool actual)
{
    return (you.religion == GOD_JIYVA && !player_under_penance()
            && you.piety >= piety_breakpoint(2)
            && (!actual || you.duration[DUR_PRAYER]));
}

bool jiyva_remove_bad_mutation()
{
    if (!how_mutated())
    {
        mpr("You have no bad mutations to be cured!");
        return (false);
    }

    // Ensure that only bad mutations are removed.
    if (!delete_mutation(RANDOM_BAD_MUTATION, true, false, true, true))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (false);
    }

    mpr("You feel cleansed.");
    return (true);
}

bool vehumet_supports_spell(spell_type spell)
{
    if (spell_typematch(spell, SPTYP_CONJURATION | SPTYP_SUMMONING))
        return (true);

    if (spell == SPELL_SHATTER
        || spell == SPELL_FRAGMENTATION
        || spell == SPELL_SANDBLAST)
    {
        return (true);
    }

    return (false);
}

// Returns false if the invocation fails (no spellbooks in sight, etc.).
bool trog_burn_spellbooks()
{
    if (you.religion != GOD_TROG)
        return (false);

    god_acting gdact;

    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_BOOKS
            && si->sub_type != BOOK_MANUAL
            && si->sub_type != BOOK_DESTRUCTION)
        {
            mpr("Burning your own feet might not be such a smart idea!");
            return (false);
        }
    }

    int totalpiety = 0;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, true, true, true); ri; ++ri)
    {
        // If a grid is blocked, books lying there will be ignored.
        // Allow bombing of monsters.
        const unsigned short cloud = env.cgrid(*ri);
        if (feat_is_solid(grd(*ri))
            || cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
        {
            continue;
        }

        int count = 0;
        int rarity = 0;
        for (stack_iterator si(*ri); si; ++si)
        {
            if (si->base_type != OBJ_BOOKS
                || si->sub_type == BOOK_MANUAL
                || si->sub_type == BOOK_DESTRUCTION)
            {
                continue;
            }

            // Ignore {!D} inscribed books.
            if (!check_warning_inscriptions(*si, OPER_DESTROY))
            {
                mpr("Won't ignite {!D} inscribed book.");
                continue;
            }

            rarity += book_rarity(si->sub_type);
            // Piety increases by 2 for books never cracked open, else 1.
            // Conversely, rarity influences the duration of the pyre.
            if (!item_type_known(*si))
                totalpiety += 2;
            else
                totalpiety++;

#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Burned book rarity: %d", rarity);
#endif
            destroy_item(si.link());
            count++;
        }

        if (count)
        {
            if (cloud != EMPTY_CLOUD)
            {
                // Reinforce the cloud.
                mpr("The fire roars with new energy!");
                const int extra_dur = count + random2(rarity / 2);
                env.cloud[cloud].decay += extra_dur * 5;
                env.cloud[cloud].set_whose(KC_YOU);
                continue;
            }

            const int duration = std::min(4 + count + random2(rarity/2), 23);
            place_cloud(CLOUD_FIRE, *ri, duration, KC_YOU);

            mprf(MSGCH_GOD, "The book%s burst%s into flames.",
                 count == 1 ? ""  : "s",
                 count == 1 ? "s" : "");
        }
    }

    if (!totalpiety)
    {
         mpr("You cannot see a spellbook to ignite!");
         return (false);
    }
    else
    {
         simple_god_message(" is delighted!", GOD_TROG);
         gain_piety(totalpiety);
    }

    return (true);
}

static bool _is_yred_enslaved_soul(const monsters* mon)
{
    return (mon->alive() && mons_enslaved_soul(mon));
}

static bool _yred_enslaved_souls_on_level_disappear()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (_is_yred_enslaved_soul(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead soul disappearing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            simple_monster_message(monster, " is freed.");

            // The monster disappears.
            monster_die(monster, KILL_DISMISSED, NON_MONSTER);

            success = true;
        }
    }

    return (success);
}

static bool _yred_souls_disappear()
{
    return (apply_to_all_dungeons(_yred_enslaved_souls_on_level_disappear));
}

void yred_make_enslaved_soul(monsters *mon, bool force_hostile,
                             bool quiet, bool unrestricted)
{
    if (!unrestricted)
        _yred_souls_disappear();

    const int type = mon->type;
    monster_type soul_type = mons_species(type);
    const std::string whose =
        you.can_see(mon) ? apostrophise(mon->name(DESC_CAP_THE))
                         : mon->pronoun(PRONOUN_CAP_POSSESSIVE);
    const bool twisted =
        !unrestricted ? !x_chance_in_y(you.skills[SK_INVOCATIONS] * 20 / 9 + 20,
                                       100)
                      : false;
    int corps = -1;

    // If the monster's held in a net, get it out.
    mons_clear_trapping_net(mon);

    const monsters orig = *mon;

    if (twisted)
    {
        mon->type = mons_zombie_size(soul_type) == Z_BIG ?
            MONS_ABOMINATION_LARGE : MONS_ABOMINATION_SMALL;
        mon->base_monster = MONS_NO_MONSTER;
    }
    else
    {
        // Drop the monster's corpse, so that it can be properly
        // re-equipped below.
        corps = place_monster_corpse(mon, true, true);
    }

    // Drop the monster's equipment.
    monster_drop_ething(mon);

    // Recreate the monster as an abomination, or as itself before
    // turning it into a spectral thing below.
    define_monster(*mon);

    mon->colour = ETC_UNHOLY;

    mon->flags |= MF_CREATED_FRIENDLY;
    mon->flags |= MF_ENSLAVED_SOUL;

    if (twisted)
        // Mark abominations as undead.
        mon->flags |= MF_HONORARY_UNDEAD;
    else if (corps != -1)
    {
        // Turn the monster into a spectral thing, minus the usual
        // adjustments for zombified monsters.
        mon->type = MONS_SPECTRAL_THING;
        mon->base_monster = soul_type;

        // Re-equip the spectral thing.
        equip_undead(mon->pos(), corps, monster_index(mon),
                     mon->base_monster);

        // Destroy the monster's corpse, as it's no longer needed.
        destroy_item(corps);
    }

    name_zombie(mon, &orig);

    mons_make_god_gift(mon, GOD_YREDELEMNUL);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, !force_hostile ? MHITNOT : MHITYOU);

    if (!quiet)
    {
        mprf("%s soul %s, and %s.", whose.c_str(),
             twisted        ? "becomes twisted" : "remains intact",
             !force_hostile ? "is now yours"    : "fights you");
    }
}

static void _print_converted_orc_speech(const std::string key,
                                        monsters *mon,
                                        msg_channel_type channel)
{
    std::string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

// Orcs may turn friendly when encountering followers of Beogh, and be
// made gifts of Beogh.
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower)
{
    ASSERT(mons_species(orc->type) == MONS_ORC);

    if (you.can_see(orc)) // show reaction
    {
        if (emergency || !orc->alive())
        {
            if (converted_by_follower)
            {
                _print_converted_orc_speech("reaction_battle_follower", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle_follower", orc,
                                            MSGCH_TALK);
            }
            else
            {
                _print_converted_orc_speech("reaction_battle", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
            }
        }
        else
        {
            _print_converted_orc_speech("reaction_sight", orc,
                                        MSGCH_FRIEND_ENCHANT);

            if (!one_chance_in(3))
                _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        }
    }

    orc->attitude = ATT_FRIENDLY;

    // The monster is not really *created* friendly, but should it
    // become hostile later on, it won't count as a good kill.
    orc->flags |= MF_CREATED_FRIENDLY;

    // Prevent assertion if the orc was previously worshipping a
    // different god, rather than already worshipping Beogh or being an
    // atheist.
    orc->god = GOD_NO_GOD;

    mons_make_god_gift(orc, GOD_BEOGH);

    if (orc->is_patrolling())
    {
        // Make orcs stop patrolling and forget their patrol point,
        // they're supposed to follow you now.
        orc->patrol_point = coord_def(0, 0);
    }

    if (!orc->alive())
        orc->hit_points = std::min(random_range(1, 4), orc->max_hit_points);

    // Avoid immobile "followers".
    behaviour_event(orc, ME_ALERT, MHITNOT);
}

void feawn_neutralise_plant(monsters *plant)
{
    if (!plant
        || !feawn_neutralises(plant)
        || plant->attitude != ATT_HOSTILE
        || testbits(plant->flags, MF_ATT_CHANGE_ATTEMPT))
    {
        return;
    }

    plant->attitude = ATT_GOOD_NEUTRAL;
    plant->flags   |= MF_WAS_NEUTRAL;
}

// Feawn allows worshipers to walk on top of stationary plants and
// fungi.
bool feawn_passthrough(const monsters * target)
{
    return (target && you.religion == GOD_FEAWN
            && mons_is_plant(target)
            && mons_is_stationary(target)
            && (target->type != MONS_OKLOB_PLANT
                || target->attitude != ATT_HOSTILE));
}

void jiyva_convert_slime(monsters* slime)
{
    ASSERT(mons_is_slime(slime));

    if (you.can_see(slime))
    {
        if (mons_genus(slime->type) == MONS_GIANT_EYEBALL)
        {
            mprf(MSGCH_GOD, "%s stares at you suspiciously for a moment, "
                            "then relaxes.",

            slime->name(DESC_CAP_THE).c_str());
        }
        else
        {
            mprf(MSGCH_GOD, "%s trembles before you.",
                 slime->name(DESC_CAP_THE).c_str());
        }
    }

    slime->attitude = ATT_STRICT_NEUTRAL;
    slime->flags   |= MF_WAS_NEUTRAL;

    if (!mons_eats_items(slime))
    {
        slime->add_ench(ENCH_EAT_ITEMS);

        mprf(MSGCH_MONSTER_ENCHANT, "%s looks hungrier.",
             slime->name(DESC_CAP_THE).c_str());
    }

    // Prevent assertion if the slime was previously worshipping a
    // different god, rather than already worshipping Jiyva or being an
    // atheist.
    slime->god = GOD_NO_GOD;

    mons_make_god_gift(slime, GOD_JIYVA);
}

bool ponderousify_armour()
{
    int item_slot = -1;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Make which item ponderous?",
                            MT_INVLIST, OSEL_PONDER_ARM, true, true, false);
        }

        if (prompt_failed(item_slot))
            return (false);

        item_def& arm(you.inv[item_slot]);

        if (!is_enchantable_armour(arm, true, true)
            || get_armour_ego_type(arm) != SPARM_NORMAL
            || get_armour_slot(arm) != EQ_BODY_ARMOUR)
        {
            mpr("Choose some type of armour to enchant, or Esc to abort.");
            if (Options.auto_list)
                more();

            item_slot = -1;
            mpr("You can't enchant that."); //does not appear
            continue;
        }

        //make item desc runed if desc was vanilla?

        set_item_ego_type(arm, OBJ_ARMOUR, SPARM_PONDEROUSNESS);

        you.redraw_armour_class = true;
        you.redraw_evasion = true;

        simple_god_message(" says: Use this wisely!");

        return (true);
    }
    while (true);

    return true;
}

static int _slouch_monsters(coord_def where, int pow, int, actor* agent)
{
    monsters* mon = monster_at(where);
    if (mon == NULL)
        return (0);

    int dmg = (mon->speed - 1000/player_movement_speed()/player_speed());
    dmg = (dmg > 0 ? roll_dice(dmg*4, 3)/2 : 0);

    mon->hurt(agent, dmg, BEAM_MMISSILE, true);
    return (1);
}

int cheibriados_slouch(int pow)
{
    return (apply_area_visible(_slouch_monsters, pow));
}

////////////////////////////////////////////////////////////////////////////

static int _lugonu_warp_monster(coord_def where, int pow, int, actor *)
{
    if (!in_bounds(where))
        return (0);

    monsters* mon = monster_at(where);
    if (mon == NULL)
        return (0);

    if (!mons_friendly(mon))
        behaviour_event(mon, ME_ANNOY, MHITYOU);

    if (check_mons_resist_magic(mon, pow * 2))
    {
        mprf("%s %s.",
             mon->name(DESC_CAP_THE).c_str(), mons_resist_string(mon));
        return (1);
    }

    const int damage = 1 + random2(pow / 6);
    if (mon->type == MONS_BLINK_FROG)
        mon->heal(damage, false);
    else if (!check_mons_resist_magic(mon, pow))
    {
        mon->hurt(&you, damage);
        if (!mon->alive())
            return (1);
    }

    mon->blink();

    return (1);
}

static void _lugonu_warp_area(int pow)
{
    apply_area_around_square( _lugonu_warp_monster, you.pos(), pow );
}

void lugonu_bends_space()
{
    const int pow = 4 + skill_bump(SK_INVOCATIONS);
    const bool area_warp = random2(pow) > 9;

    mprf("Space bends %saround you!", area_warp? "sharply " : "");

    if (area_warp)
        _lugonu_warp_area(pow);

    random_blink(false, true);

    const int damage = roll_dice(1, 4);
    ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC, "a spatial distortion");
}

////////////////////////////////////////////////////////////////////////

void cheibriados_time_bend(int pow)
{
    mpr("The flow of time bends around you.");

    for (adjacent_iterator ai; ai; ++ai)
    {
        monsters* mon = monster_at(*ai);
        if (mon != NULL && !mons_is_stationary(mon))
        {
            if (roll_dice(mon->hit_dice, 3) > random2avg(pow, 2))
            {
                mprf("%s %s.",
                 mon->name(DESC_CAP_THE).c_str(), mons_resist_string(mon));
                continue;
            }

            simple_god_message(
                make_stringf(" rebukes %s.",
                             mon->name(DESC_NOCAP_THE).c_str()).c_str(),
                             GOD_CHEIBRIADOS);
            do_slow_monster(mon, KC_YOU);
        }
    }
}

void cheibriados_time_step(int pow) // pow is the number of turns to skip
{
    coord_def old_pos = you.pos();

    mpr("You step out of the flow of time.");
    you.flash_colour = LIGHTBLUE;
    viewwindow(true, true);
    you.moveto(coord_def(0, 0));
    you.duration[DUR_TIME_STEP] = pow;

    you.time_taken = 10;
    do
    {
        run_environment_effects();
        handle_monsters();
        manage_clouds();
    }
    while (--you.duration[DUR_TIME_STEP] > 0);
    // Update corpses, etc.  This does also shift monsters, but only by a tiny bit.
    update_level(pow*10);

#ifndef USE_TILE
    delay(1000);
#endif

    you.flash_colour = 0;
    you.moveto(old_pos);
    you.duration[DUR_TIME_STEP] = 0;
    viewwindow(true, false);
    mpr("You return into the normal time flow.");
}

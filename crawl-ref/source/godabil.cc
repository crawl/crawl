/*
 *  File:       godabil.cc
 *  Summary:    God-granted abilities.
 */

#include "AppHdr.h"

#include <queue>

#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-actions.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "files.h"
#include "fprop.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "skills2.h"
#include "shopping.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"
#include "viewgeom.h"

#ifdef USE_TILE
#include "tiledef-main.h"
#endif

bool zin_sustenance(bool actual)
{
    return (you.piety >= piety_breakpoint(0)
            && (!actual || you.hunger_state == HS_STARVING));
}

// Monsters cannot be affected in these states.
// (All results of Recite, plus stationary and friendly + stupid;
// note that berserk monsters are also hasted.)
static bool _zin_recite_mons_useless(const monster* mon)
{
    const mon_holy_type holiness = mon->holiness();

    return (mons_intel(mon) < I_NORMAL
            || !mon->is_holy()
               && holiness != MH_NATURAL
               && holiness != MH_UNDEAD
               && holiness != MH_DEMONIC
            || mons_is_stationary(mon)
            || mons_is_fleeing(mon)
            || mon->asleep()
            || mon->wont_attack()
            || mon->neutral()
            || mons_is_confused(mon)
            || mon->paralysed()
            || mon->has_ench(ENCH_BATTLE_FRENZY)
            || mon->has_ench(ENCH_HASTE));
}

// Check whether this monster might be influenced by Recite.
// Returns 0, if no monster found.
// Returns 1, if eligible monster found.
// Returns -1, if monster already affected or too dumb to understand.
int zin_check_recite_to_single_monster(const coord_def& where)
{
    monster* mon = monster_at(where);

    if (mon == NULL)
        return (0);

    if (!_zin_recite_mons_useless(mon))
        return (1);

    return (-1);
}

// Check whether there are monsters who might be influenced by Recite.
// Returns 0, if no monsters found.
// Returns 1, if eligible audience found.
// Returns -1, if entire audience already affected or too dumb to understand.
int zin_check_recite_to_monsters()
{
    bool found_monsters = false;

    for (radius_iterator ri(you.pos(), 8); ri; ++ri)
    {
        const int retval = zin_check_recite_to_single_monster(*ri);

        if (retval == -1)
            found_monsters = true;

        // Check if audience can listen.
        if (retval == 1)
            return (1);
    }

    if (!found_monsters)
        dprf("No audience found!");
    else
        dprf("No sensible audience found!");

   // No use preaching to the choir, nor to common animals.
   if (found_monsters)
       return (-1);

   // Sorry, no audience found!
   return (0);
}

// Power is maximum 50.
int zin_recite_to_single_monster(const coord_def& where,
                                 bool imprisoned, int pow)
{
    if (you.religion != GOD_ZIN)
        return (0);

    monster* mon = monster_at(where);

    if (mon == NULL)
        return (0);

    if (_zin_recite_mons_useless(mon))
        return (0);

    // nothing happens
    if (!imprisoned && coinflip())
        return (0);

    // up to (60 + 40)/2 = 50
    if (pow == -1)
        pow = (2 * skill_bump(SK_INVOCATIONS) + you.piety / 5) / 2;

    const mon_holy_type holiness = mon->holiness();
    bool impressible = true;
    int resist;

    if (mon->is_holy())
        resist = std::max(0, 7 - random2(you.skills[SK_INVOCATIONS]));
    else
    {
        resist = mon->res_magic();

        if (holiness == MH_UNDEAD)
        {
            impressible = false;
            pow -= 2 + random2(3);
        }
        else if (holiness == MH_DEMONIC)
        {
            impressible = false;
            pow -= 3 + random2(5);
        }

        if (mon->is_unclean() || mon->is_chaotic())
            impressible = false;
    }

    pow -= resist;

    if (pow > 0)
        pow = random2avg(pow, 2);

    if (pow <= 0) // Uh oh...
    {
        if (!imprisoned && one_chance_in(resist + 1))
            return (0);  // nothing happens, whew!

        if (!one_chance_in(4)
             && mon->add_ench(mon_enchant(ENCH_HASTE, 0, KC_YOU,
                                          (16 + random2avg(13, 2)) * 10)))
        {

            simple_monster_message(mon, " speeds up in annoyance!");
        }
        else if (!one_chance_in(3)
                 && mon->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1, KC_YOU,
                                              (16 + random2avg(13, 2)) * 10)))
        {
            simple_monster_message(mon, " goes into a battle-frenzy!");
        }
        else if (!one_chance_in(3)
                 && mons_shouts(mon->type, false) != S_SILENT)
        {
            force_monster_shout(mon);
        }
        else
            return (0); // nothing happens

        // Bad effects stop the recital.
        stop_delay();
        return (1);
    }

    switch (pow)
    {
        case 0:
            return (0); // handled above
        case 1:
        case 2:
        case 3:
        case 4:
            if (!mons_class_is_confusable(mon->type)
                || !mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU,
                                              (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mon, " looks confused.");
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            if (!mon->can_hibernate())
                return (0);
            mon->hibernate();
            simple_monster_message(mon, " falls asleep!");
            break;
        case 9:
        case 10:
        case 11:
        case 12:
            if (!impressible
                || !mon->add_ench(mon_enchant(ENCH_TEMP_PACIF, 0, KC_YOU,
                                  (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mon, " seems impressed!");
            break;
        case 13:
        case 14:
        case 15:
            if (!mon->add_ench(ENCH_FEAR))
                return (0);
            simple_monster_message(mon, " turns to flee.");
            break;
        case 16:
        case 17:
            if (!mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, KC_YOU,
                               (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mon, " freezes in fright!");
            break;
        default:
            if (mon->is_holy())
                good_god_holy_attitude_change(mon);
            else
            {
                if (!impressible)
                    return (0);
                simple_monster_message(mon, " seems fully impressed!");
                mons_pacify(mon);
            }
            break;
    }

    return (1);
}

static bool _kill_duration(duration_type dur)
{
    const bool rc = (you.duration[dur] > 0);
    you.duration[dur] = 0;
    return (rc);
}

bool zin_vitalisation()
{
    bool success = false;
    int type = 0;

    // Remove negative afflictions.
    if (you.disease || you.rotting || you.confused()
        || you.duration[DUR_PARALYSIS] || you.duration[DUR_POISONING]
        || you.petrified())
    {
        do
        {
            switch (random2(6))
            {
            case 0:
                if (you.disease)
                {
                    success = true;
                    you.disease = 0;
                }
                break;
            case 1:
                if (you.rotting)
                {
                    success = true;
                    you.rotting = 0;
                }
                break;
            case 2:
                success = _kill_duration(DUR_CONF);
                break;
            case 3:
                success = _kill_duration(DUR_PARALYSIS);
                break;
            case 4:
                success = _kill_duration(DUR_POISONING);
                break;
            case 5:
                success = _kill_duration(DUR_PETRIFIED);
                break;
            }
        }
        while (!success);
    }
    // Restore stats.
    else if (you.strength() < you.max_strength()
             || you.intel() < you.max_intel()
             || you.dex() < you.max_dex())
    {
        type = 1;
        restore_stat(STAT_RANDOM, 0, true);
        success = true;
    }
    else
    {
        // Add divine stamina.
        if (!you.duration[DUR_DIVINE_STAMINA])
        {
            success = true;
            type = 2;

            mprf("%s grants you divine stamina.",
                 god_name(GOD_ZIN).c_str());

            const int stamina_amt = 3;
            you.attribute[ATTR_DIVINE_STAMINA] = stamina_amt;
            you.set_duration(DUR_DIVINE_STAMINA,
                             40 + (you.skills[SK_INVOCATIONS]*5)/2);

            notify_stat_change(STAT_STR, stamina_amt, true, "");
            notify_stat_change(STAT_INT, stamina_amt, true, "");
            notify_stat_change(STAT_DEX, stamina_amt, true, "");
        }
    }

    // If vitalisation has succeeded, display an appropriate message.
    if (success)
    {
        mprf("You feel %s.", (type == 0) ? "better" :
                             (type == 1) ? "renewed"
                                         : "powerful");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

void zin_remove_divine_stamina()
{
    mpr("Your divine stamina fades away.", MSGCH_DURATION);
    notify_stat_change(STAT_STR, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_INT, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_DEX, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    you.duration[DUR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
}

bool zin_remove_all_mutations()
{
    if (!how_mutated())
    {
        mpr("You have no mutations to be cured!");
        return (false);
    }

    you.num_gifts[GOD_ZIN]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    simple_god_message(" draws all chaos from your body!");
    delete_all_mutations();

    return (true);
}

bool zin_sanctuary()
{
    // Casting is disallowed while previous sanctuary in effect.
    // (Checked in abl-show.cc.)
    if (env.sanctuary_time)
        return (false);

    // Yes, shamelessly stolen from NetHack...
    if (!silenced(you.pos())) // How did you manage that?
        mpr("You hear a choir sing!", MSGCH_SOUND);
    else
        mpr("You are suddenly bathed in radiance!");

    flash_view(WHITE);

    holy_word(100, HOLY_WORD_ZIN, you.pos(), true);

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    // Pets stop attacking and converge on you.
    you.pet_target = MHITYOU;

    create_sanctuary(you.pos(), 7 + you.skills[SK_INVOCATIONS] / 2);

    return (true);
}

// shield bonus = attribute for duration turns, then decreasing by 1
//                every two out of three turns
// overall shield duration = duration + attribute
// recasting simply resets those two values (to better values, presumably)
void tso_divine_shield()
{
    if (!you.duration[DUR_DIVINE_SHIELD])
    {
        if (you.shield()
            || you.duration[DUR_FIRE_SHIELD]
            || you.duration[DUR_CONDENSATION_SHIELD])
        {
            mprf("Your shield is strengthened by %s's divine power.",
                 god_name(GOD_SHINING_ONE).c_str());
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

void tso_remove_divine_shield()
{
    mpr("Your divine shield disappears!", MSGCH_DURATION);
    you.duration[DUR_DIVINE_SHIELD] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    you.redraw_armour_class = true;
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Unholy and evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // Artefacts, including most randarts.
    if (random2(value) >= random2(250))
        return (true);

    // Medium valuable items are more likely to net piety at low piety,
    // more so for missiles, since they're worth less as single items.
    if (random2(value) >= random2((type == OBJ_MISSILES) ? 10 : 100)
        && one_chance_in(1 + you.piety / 50))
    {
        return (true);
    }

    // If not for the above, missiles shouldn't yield piety.
    if (type == OBJ_MISSILES)
        return (false);

    // Weapons, on the other hand, are always acceptable to boost low
    // piety.
    if (you.piety < piety_breakpoint(0) || player_under_penance())
        return (true);

    return (false);
}

bool elyvilon_destroy_weapons()
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    god_acting gdact;

    bool success = false;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        item_def& item(*si);
        if (item.base_type != OBJ_WEAPONS
                && item.base_type != OBJ_MISSILES
            || item_is_stationary(item)) // Held in a net?
        {
            continue;
        }

        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't destroy {!D} inscribed item.");
            continue;
        }

        // item_value() multiplies by quantity.
        const int value = item_value(item, true) / item.quantity;
        dprf("Destroyed weapon value: %d", value);

        piety_gain_t pgain = PIETY_NONE;
        const bool unholy_weapon = is_unholy_item(item);
        const bool evil_weapon = is_evil_item(item);

        if (unholy_weapon
            || evil_weapon
            || _destroyed_valuable_weapon(value, item.base_type))
        {
            pgain = PIETY_SOME;
        }

        if (get_weapon_brand(item) == SPWPN_HOLY_WRATH)
        {
            // Weapons blessed by TSO don't get destroyed but are instead
            // returned whence they came. (jpeg)
            simple_god_message(
                make_stringf(" %sreclaims %s.",
                             pgain == PIETY_SOME ? "gladly " : "",
                             item.name(DESC_NOCAP_THE).c_str()).c_str(),
                GOD_SHINING_ONE);
        }
        else
        {
            // Elyvilon doesn't care about item sacrifices at altars, so
            // I'm stealing _Sacrifice_Messages.
            print_sacrifice_message(GOD_ELYVILON, item, pgain);
            if (unholy_weapon || evil_weapon)
            {
                const char *desc_weapon = evil_weapon ? "evil" : "unholy";

                // Print this in addition to the above!
                simple_god_message(
                    make_stringf(" welcomes the destruction of %s %s "
                                 "weapon%s.",
                                 item.quantity == 1 ? "this" : "these",
                                 desc_weapon,
                                 item.quantity == 1 ? ""     : "s").c_str(),
                    GOD_ELYVILON);
            }
        }

        if (pgain == PIETY_SOME)
            gain_piety(1);

        destroy_item(si.link());
        success = true;
    }

    if (!success)
        mpr("There are no weapons here to destroy!");

    return (success);
}

void elyvilon_purification()
{
    mpr("You feel purified!");

    you.disease = 0;
    you.rotting = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PARALYSIS] = 0;          // can't currently happen -- bwr
    you.duration[DUR_PETRIFIED] = 0;
}

bool elyvilon_divine_vigour()
{
    bool success = false;

    if (!you.duration[DUR_DIVINE_VIGOUR])
    {
        mprf("%s grants you divine vigour.",
             god_name(GOD_ELYVILON).c_str());

        const int vigour_amt = 1 + (you.skills[SK_INVOCATIONS]/3);
        const int old_hp_max = you.hp_max;
        const int old_mp_max = you.max_magic_points;
        you.attribute[ATTR_DIVINE_VIGOUR] = vigour_amt;
        you.set_duration(DUR_DIVINE_VIGOUR,
                         40 + (you.skills[SK_INVOCATIONS]*5)/2);

        calc_hp();
        inc_hp(you.hp_max - old_hp_max, false);
        calc_mp();
        inc_mp(you.max_magic_points - old_mp_max, false);

        success = true;
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

void elyvilon_remove_divine_vigour()
{
    mpr("Your divine vigour fades away.", MSGCH_DURATION);
    you.duration[DUR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    calc_hp();
    calc_mp();
}

bool vehumet_supports_spell(spell_type spell)
{
    if (spell_typematch(spell, SPTYP_CONJURATION | SPTYP_SUMMONING))
        return (true);

    // Conjurations work by conjuring up a chunk of short-lived matter and
    // propelling it towards the victim.  This is the most popular way, but
    // by no means it has a monopoly for being destructive.
    // Vehumet loves all direct physical destruction.
    if (spell == SPELL_SHATTER
        || spell == SPELL_FRAGMENTATION // LRD
        || spell == SPELL_SANDBLAST
        || spell == SPELL_AIRSTRIKE
        || spell == SPELL_IGNITE_POISON
        || spell == SPELL_OZOCUBUS_REFRIGERATION
        // Toxic Radiance does no direct damage
        || spell == SPELL_BONE_SHARDS)
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
    int totalblocked = 0;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, true, true, true); ri; ++ri)
    {
        const unsigned short cloud = env.cgrid(*ri);
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

            // If a grid is blocked, books lying there will be ignored.
            // Allow bombing of monsters.
            if (feat_is_solid(grd(*ri))
                || cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
            {
                totalblocked++;
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

            dprf("Burned book rarity: %d", rarity);
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

    if (totalpiety)
    {
        simple_god_message(" is delighted!", GOD_TROG);
        gain_piety(totalpiety);
    }
    else if (totalblocked)
    {
        if (totalblocked == 1)
            mpr("The spellbook fails to ignite!");
        else
            mpr("The spellbooks fail to ignite!");
        return (false);
    }
    else
    {
        mpr("You cannot see a spellbook to ignite!");
        return (false);
    }

    return (true);
}

bool beogh_water_walk()
{
    return (you.religion == GOD_BEOGH && !player_under_penance()
            && you.piety >= piety_breakpoint(4));
}

bool jiyva_can_paralyse_jellies()
{
    return (you.religion == GOD_JIYVA && !player_under_penance()
            && you.piety >= piety_breakpoint(2));
}

void jiyva_paralyse_jellies()
{
    int jelly_count = 0;
    for (radius_iterator ri(you.pos(), 9); ri; ++ri)
    {
        monster* mon = monster_at(*ri);

        if (mon != NULL && mons_is_slime(mon))
        {
            mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0,
                                      KC_OTHER, 200));
            jelly_count++;
        }
    }

    if (jelly_count > 0)
    {
        mprf(MSGCH_PRAY, "%s.",
             jelly_count > 1 ? "The nearby slimes join your prayer"
                             : "A nearby slime joins your prayer");
        lose_piety(5);
    }
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

bool yred_injury_mirror(bool actual)
{
    return (you.religion == GOD_YREDELEMNUL && !player_under_penance()
            && you.piety >= piety_breakpoint(1)
            && (!actual || you.duration[DUR_PRAYER]));
}

bool yred_can_animate_dead()
{
    return (you.piety >= piety_breakpoint(2));
}

void yred_animate_remains_or_dead()
{
    if (yred_can_animate_dead())
    {
        mpr("You call on the dead to rise...");

        animate_dead(&you, you.skills[SK_INVOCATIONS] + 1, BEH_FRIENDLY,
                     MHITYOU, &you, "", GOD_YREDELEMNUL);
    }
    else
    {
        mpr("You attempt to give life to the dead...");

        if (animate_remains(you.pos(), CORPSE_BODY, BEH_FRIENDLY,
                            MHITYOU, &you, "", GOD_YREDELEMNUL) < 0)
        {
            mpr("There are no remains here to animate!");
        }
    }
}

void yred_drain_life()
{
    mpr("You draw life from your surroundings.");

    flash_view(DARKGREY);
    more();
    mesclr();

    const int pow = you.skills[SK_INVOCATIONS];
    const int hurted = 3 + random2(7) + random2(pow);
    int hp_gain = 0;

    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (mi->holiness() != MH_NATURAL
            || mi->res_negative_energy())
        {
            continue;
        }

        mprf("You draw life from %s.",
             mi->name(DESC_NOCAP_THE).c_str());

        behaviour_event(*mi, ME_WHACK, MHITYOU, you.pos());

        mi->hurt(&you, hurted);

        if (mi->alive())
            print_wounds(*mi);

        if (!mi->is_summoned())
            hp_gain += hurted;
    }

    hp_gain /= 2;

    hp_gain = std::min(pow * 2, hp_gain);

    if (hp_gain)
    {
        mpr("You feel life flooding into your body.");
        inc_hp(hp_gain, false);
    }
}

void yred_make_enslaved_soul(monster* mon, bool force_hostile)
{
    add_daction(DACT_OLD_ENSLAVED_SOULS_POOF);

    const monster_type soul_type = mons_species(mon->type);
    const std::string whose =
        you.can_see(mon) ? apostrophise(mon->name(DESC_CAP_THE))
                         : mon->pronoun(PRONOUN_CAP_POSSESSIVE);

    // If the monster's held in a net, get it out.
    mons_clear_trapping_net(mon);

    // Drop the monster's holy equipment, and keep wielding the rest.
    monster_drop_things(mon, false, is_holy_item);

    const monster orig = *mon;

    // Turn the monster into a spectral thing, minus the usual
    // adjustments for zombified monsters.
    mon->type = MONS_SPECTRAL_THING;
    mon->base_monster = soul_type;

    // Recreate the monster as a spectral thing.
    define_monster(mon);

    mon->colour = ETC_UNHOLY;

    mon->flags |= MF_NO_REWARD;
    mon->flags |= MF_ENSLAVED_SOUL;

    // If the original monster type has melee, spellcasting or priestly
    // abilities, make sure its spectral thing has them as well.
    mon->flags |= orig.flags & (MF_MELEE_MASK | MF_SPELL_MASK);
    mon->spells = orig.spells;

    name_zombie(mon, &orig);

    mons_make_god_gift(mon, GOD_YREDELEMNUL);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, !force_hostile ? MHITNOT : MHITYOU);

    mprf("%s soul %s.", whose.c_str(),
         !force_hostile ? "is now yours" : "fights you");
}

bool kiku_receive_corpses(int pow, coord_def where)
{
    // pow = invocations * 4, ranges from 0 to 108
    dprf("kiku_receive_corpses() power: %d", pow);

    // Kiku gives branch-appropriate corpses (like shadow creatures).
    int expected_extra_corpses = 3 + pow / 18; // 3 at 0 Inv, 9 at 27 Inv.
    int corpse_delivery_radius = 1;

    // We should get the same number of corpses
    // in a hallway as in an open room.
    int spaces_for_corpses = 0;
    for (radius_iterator ri(where, corpse_delivery_radius, C_ROUND,
                            you.get_los(), true);
         ri; ++ri)
    {
        if (mons_class_can_pass(MONS_HUMAN, grd(*ri)))
            spaces_for_corpses++;
    }

    int percent_chance_a_square_receives_extra_corpse = // can be > 100
        int(float(expected_extra_corpses) / float(spaces_for_corpses) * 100.0);

    int corpses_created = 0;

    for (radius_iterator ri(where, corpse_delivery_radius, C_ROUND,
                            you.get_los());
         ri; ++ri)
    {
        bool square_is_walkable = mons_class_can_pass(MONS_HUMAN, grd(*ri));
        bool square_is_player_square = (*ri == where);
        bool square_gets_corpse =
            (random2(100) < percent_chance_a_square_receives_extra_corpse)
            || (square_is_player_square && random2(100) < 97);

        if (!square_is_walkable || !square_gets_corpse)
            continue;

        corpses_created++;

        // Find an appropriate monster corpse for level and power.
        monster_type mon_type = MONS_PROGRAM_BUG;
        int adjusted_power = 0;
        for (int i = 0; i < 200 && !mons_class_can_be_zombified(mon_type); ++i)
        {
            adjusted_power = std::min(pow / 4, random2(random2(pow)));
            mon_type = pick_local_zombifiable_monster(adjusted_power);
        }

        // Create corpse object.
        monster dummy;
        dummy.type = mon_type;
        int index_of_corpse_created = get_item_slot();

        if (index_of_corpse_created == NON_ITEM)
            break;

        if (mons_genus(mon_type) == MONS_HYDRA)
            dummy.number = random2(20) + 1;

        int valid_corpse = fill_out_corpse(&dummy,
                                           dummy.type,
                                           mitm[index_of_corpse_created],
                                           false);
        if (valid_corpse == -1)
        {
            mitm[index_of_corpse_created].clear();
            continue;
        }

        mitm[index_of_corpse_created].props["DoNotDropHide"] = true;

        ASSERT(valid_corpse >= 0);

        // Higher piety means fresher corpses.  One out of ten corpses
        // will always be rotten.
        int rottedness = 200 -
            (!one_chance_in(10) ? random2(200 - you.piety)
                                : random2(100 + random2(75)));
        mitm[index_of_corpse_created].special = rottedness;

        // Place the corpse.
        move_item_to_grid(&index_of_corpse_created, *ri);
    }

    if (corpses_created)
    {
        if (you.religion == GOD_KIKUBAAQUDGHA)
        {
            simple_god_message(corpses_created > 1 ? " delivers you corpses!"
                                                   : " delivers you a corpse!");
        }
        maybe_update_stashes();
        return (true);
    }
    else
    {
        if (you.religion == GOD_KIKUBAAQUDGHA)
            simple_god_message(" can find no cadavers for you!");
        return (false);
    }
}

bool fedhas_passthrough_class(const monster_type mc)
{
    return (you.religion == GOD_FEDHAS
            && mons_class_is_plant(mc)
            && mons_class_is_stationary(mc));
}

// Fedhas allows worshipers to walk on top of stationary plants and
// fungi.
bool fedhas_passthrough(const monster* target)
{
    return (target
            && fedhas_passthrough_class(target->type)
            && (target->type != MONS_OKLOB_PLANT
                || target->attitude != ATT_HOSTILE));
}

bool fedhas_passthrough(const monster_info* target)
{
    return (target
            && fedhas_passthrough_class(target->type)
            && (target->type != MONS_OKLOB_PLANT
                || target->attitude != ATT_HOSTILE));
}

// Fedhas worshipers can shoot through non-hostile plants, can a
// particular beam go through a particular monster?
bool fedhas_shoot_through(const bolt & beam, const monster* victim)
{
    actor * originator = beam.agent();
    if (!victim || !originator)
        return (false);

    bool origin_worships_fedhas;
    mon_attitude_type origin_attitude;
    if (originator->atype() == ACT_PLAYER)
    {
        origin_worships_fedhas = you.religion == GOD_FEDHAS;
        origin_attitude = ATT_FRIENDLY;
    }
    else
    {
        monster* temp = originator->as_monster();
        if (!temp)
            return (false);
        origin_worships_fedhas = temp->god == GOD_FEDHAS;
        origin_attitude = temp->attitude;
    }

    return (origin_worships_fedhas
            && fedhas_protects(victim)
            && !beam.is_enchantment()
            && !(beam.is_explosion && beam.in_explosion_phase)
            && (mons_atts_aligned(victim->attitude, origin_attitude)
                || victim->neutral()));
}

// Turns corpses in LOS into skeletons and grows toadstools on them.
// Can also turn zombies into skeletons and destroy ghoul-type monsters.
// Returns the number of corpses consumed.
int fedhas_fungal_bloom()
{
    int seen_mushrooms  = 0;
    int seen_corpses    = 0;

    int processed_count = 0;
    bool kills = false;

    for (radius_iterator i(you.pos(), LOS_RADIUS); i; ++i)
    {
        monster* target = monster_at(*i);
        if (target && target->is_summoned())
            continue;

        if (!is_harmless_cloud(cloud_type_at(*i)))
            continue;

        if (target && target->mons_species() != MONS_TOADSTOOL)
        {
            switch (mons_genus(target->mons_species()))
            {
            case MONS_ZOMBIE_SMALL:
                // Maybe turn a zombie into a skeleton.
                if (mons_skeleton(mons_zombie_base(target)))
                {
                    processed_count++;

                    monster_type skele_type = MONS_SKELETON_LARGE;
                    if (mons_zombie_size(mons_zombie_base(target)) == Z_SMALL)
                        skele_type = MONS_SKELETON_SMALL;

                    // Killing and replacing the zombie since upgrade_type
                    // doesn't get skeleton speed right (and doesn't
                    // reduce the victim's HP). This is awkward. -cao
                    mgen_data mg(skele_type, target->behaviour, NULL, 0, 0,
                                 target->pos(),
                                 target->foe,
                                 MG_FORCE_BEH | MG_FORCE_PLACE,
                                 target->god,
                                 mons_zombie_base(target),
                                 target->number);

                    unsigned monster_flags = target->flags;
                    int current_hp = target->hit_points;
                    mon_enchant_list ench = target->enchantments;

                    simple_monster_message(target, "'s flesh rots away.");

                    monster_die(target, KILL_MISC, NON_MONSTER, true);
                    int mons = create_monster(mg);
                    env.mons[mons].flags = monster_flags;
                    env.mons[mons].enchantments = ench;

                    if (env.mons[mons].hit_points > current_hp)
                        env.mons[mons].hit_points = current_hp;

                    behaviour_event(&env.mons[mons], ME_ALERT, MHITYOU);

                    continue;
                }
                // Else fall through and destroy the zombie.
                // Ghoul-type monsters are always destroyed.
            case MONS_GHOUL:
            {
                simple_monster_message(target, " rots away and dies.");

                coord_def pos = target->pos();
                int colour    = target->colour;
                int corpse    = monster_die(target, KILL_MISC, NON_MONSTER, true);
                kills = true;

                // If a corpse didn't drop, create a toadstool.
                // If one did drop, we will create toadstools from it as usual
                // later on.
                if (corpse < 0)
                {
                    const int mushroom = create_monster(
                                mgen_data(MONS_TOADSTOOL,
                                          BEH_GOOD_NEUTRAL,
                                          &you,
                                          0,
                                          0,
                                          pos,
                                          MHITNOT,
                                          MG_FORCE_PLACE,
                                          GOD_NO_GOD,
                                          MONS_NO_MONSTER,
                                          0,
                                          colour),
                                          false);

                    if (mushroom != -1)
                        seen_mushrooms++;

                    processed_count++;

                    continue;
                }
                break;
            }

            default:
                continue;
            }
        }

        for (stack_iterator j(*i); j; ++j)
        {
            bool corpse_on_pos = false;
            if (j->base_type == OBJ_CORPSES && j->sub_type == CORPSE_BODY)
            {
                corpse_on_pos  = true;
                int trial_prob = mushroom_prob(*j);

                processed_count++;
                int target_count = 1 + binomial_generator(20, trial_prob);

                int seen_per;
                spawn_corpse_mushrooms(*j, target_count, seen_per,
                                       BEH_GOOD_NEUTRAL, true);

                seen_mushrooms += seen_per;

                // Either turn this corpse into a skeleton or destroy it.
                if (mons_skeleton(j->plus))
                    turn_corpse_into_skeleton(*j);
                else
                    destroy_item(j->index());
            }

            if (corpse_on_pos && you.see_cell(*i))
                seen_corpses++;
        }
    }

    if (seen_mushrooms > 0)
        mushroom_spawn_message(seen_mushrooms, seen_corpses);

    if (kills)
        mprf("That felt like a moral victory.");

    return (processed_count);
}

static int _create_plant(coord_def & target, int hp_adjust = 0)
{
    if (actor_at(target) || !mons_class_can_pass(MONS_PLANT, grd(target)))
        return (0);

    const int plant = create_monster(mgen_data
                                     (MONS_PLANT,
                                      BEH_FRIENDLY,
                                      &you,
                                      0,
                                      0,
                                      target,
                                      MHITNOT,
                                      MG_FORCE_PLACE, GOD_FEDHAS));


    if (plant != -1)
    {
        env.mons[plant].flags |= MF_ATT_CHANGE_ATTEMPT;
        env.mons[plant].max_hit_points += hp_adjust;
        env.mons[plant].hit_points += hp_adjust;

        if (you.see_cell(target))
        {
            if (hp_adjust)
            {
                mprf("A plant, strengthened by %s, grows up from the ground.",
                     god_name(GOD_FEDHAS).c_str());
            }
            else
                mpr("A plant grows up from the ground.");
        }
    }

    return (plant != -1);
}

bool fedhas_sunlight()
{
    const int c_size = 5;
    const int x_offset[] = {-1, 0, 0, 0, 1};
    const int y_offset[] = { 0,-1, 0, 1, 0};

    dist spelld;

    bolt temp_bolt;
    temp_bolt.colour = YELLOW;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE_SUBMERGED;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.may_target_monster = false;
    args.top_prompt = "Select sunlight destination.";
    direction(spelld, args);

    if (!spelld.isValid)
        return (false);

    const coord_def base = spelld.target;

    int evap_count  = 0;
    int plant_count = 0;
    int processed_count = 0;

    // This is dealt with outside of the main loop.
    int cloud_count = 0;

    // FIXME: Uncomfortable level of code duplication here but the explosion
    // code in bolt subjects the input radius to r*(r+1) for the threshold and
    // since r is an integer we can never get just the 4-connected neighbours.
    // Anyway the bolt code doesn't seem to be well set up to handle the
    // 'occasional plant' gimmick.
    for (int i = 0; i < c_size; ++i)
    {
        coord_def target = base;
        target.x += x_offset[i];
        target.y += y_offset[i];

        if (!in_bounds(target) || feat_is_solid(grd(target)))
            continue;

        temp_bolt.explosion_draw_cell(target);

        actor *victim = actor_at(target);

        // If this is a water square we will evaporate it.
        dungeon_feature_type ftype = grd(target);
        dungeon_feature_type orig_type = ftype;

        switch (ftype)
        {
        case DNGN_SHALLOW_WATER:
            ftype = DNGN_FLOOR;
            break;

        case DNGN_DEEP_WATER:
            ftype = DNGN_SHALLOW_WATER;
            break;

        default:
            break;
        }

        if (orig_type != ftype)
        {
            dungeon_terrain_changed(target, ftype);

            if (you.see_cell(target))
                evap_count++;

            // This is a little awkward but if we evaporated all the way to
            // the dungeon floor that may have given a monster
            // ENCH_AQUATIC_LAND, and if that happened the player should get
            // credit if the monster dies. The enchantment is inflicted via
            // the dungeon_terrain_changed call chain and that doesn't keep
            // track of what caused the terrain change. -cao
            monster* mons = monster_at(target);
            if (mons && ftype == DNGN_FLOOR
                && mons->has_ench(ENCH_AQUATIC_LAND))
            {
                mon_enchant temp = mons->get_ench(ENCH_AQUATIC_LAND);
                temp.who = KC_YOU;
                mons->add_ench(temp);
            }

            processed_count++;
        }

        monster* mons = monster_at(target);

        if (victim)
        {
            if (!mons)
                you.backlight();
            else
            {
                backlight_monsters(target, 1, 0);
                behaviour_event(mons, ME_ALERT, MHITYOU);
            }

            processed_count++;
        }
        else if (one_chance_in(100)
                 && ftype >= DNGN_FLOOR_MIN
                 && ftype <= DNGN_FLOOR_MAX
                 && orig_type == DNGN_SHALLOW_WATER)
        {
            // Create a plant.
            const int plant = create_monster(mgen_data(MONS_PLANT,
                                                       BEH_HOSTILE,
                                                       &you,
                                                       0,
                                                       0,
                                                       target,
                                                       MHITNOT,
                                                       MG_FORCE_PLACE,
                                                       GOD_FEDHAS));

            if (plant != -1 && you.see_cell(target))
                plant_count++;

            processed_count++;
        }
    }

    // We damage clouds for a large radius, though.
    for (radius_iterator ai(base, 7); ai; ++ai)
    {
        if (env.cgrid(*ai) != EMPTY_CLOUD)
        {
            const int cloudidx = env.cgrid(*ai);
            if (env.cloud[cloudidx].type == CLOUD_GLOOM)
            {
                cloud_count++;
                delete_cloud(cloudidx);
            }
        }
    }

#ifndef USE_TILE
    // Move the cursor out of the way (it looks weird).
    coord_def temp = grid2view(base);
    cgotoxy(temp.x, temp.y, GOTO_DNGN);
#endif
    delay(200);

    if (plant_count)
    {
        mprf("%s grow%s in the sunlight.",
             (plant_count > 1 ? "Some plants": "A plant"),
             (plant_count > 1 ? "": "s"));
    }

    if (evap_count)
        mprf("Some water evaporates in the bright sunlight.");

    if (cloud_count)
        mprf("Sunlight penetrates the thick gloom.");

    return (true);
}

template<typename T>
bool less_second(const T & left, const T & right)
{
    return (left.second < right.second);
}

typedef std::pair<coord_def, int> point_distance;

// Find the distance from origin to each of the targets, those results
// are stored in distances (which is the same size as targets). Exclusion
// is a set of points which are considered disconnected for the search.
static void _path_distance(const coord_def & origin,
                           const std::vector<coord_def> & targets,
                           std::set<int> exclusion,
                           std::vector<int> & distances)
{
    std::queue<point_distance> fringe;
    fringe.push(point_distance(origin,0));
    distances.clear();
    distances.resize(targets.size(), INT_MAX);

    while (!fringe.empty())
    {
        point_distance current = fringe.front();
        fringe.pop();

        // did we hit a target?
        for (unsigned i = 0; i < targets.size(); ++i)
        {
            if (current.first == targets[i])
            {
                distances[i] = current.second;
                break;
            }
        }

        for (adjacent_iterator adj_it(current.first); adj_it; ++adj_it)
        {
            int idx = adj_it->x + adj_it->y * X_WIDTH;
            if (you.see_cell(*adj_it)
                && !feat_is_solid(env.grid(*adj_it))
                && *adj_it != you.pos()
                && exclusion.insert(idx).second)
            {
                monster* temp = monster_at(*adj_it);
                if (!temp || (temp->attitude == ATT_HOSTILE
                              && !mons_is_stationary(temp)))
                {
                    fringe.push(point_distance(*adj_it, current.second+1));
                }
            }
        }
    }
}


// Find the minimum distance from each point of origin to one of the targets
// The distance is stored in 'distances', which is the same size as origins.
static void _point_point_distance(const std::vector<coord_def> & origins,
                                  const std::vector<coord_def> & targets,
                                  std::vector<int> & distances)
{
    distances.clear();
    distances.resize(origins.size(), INT_MAX);

    // Consider all points of origin as blocked (you can search outward
    // from one, but you can't form a path across a different one).
    std::set<int> base_exclusions;
    for (unsigned i = 0; i < origins.size(); ++i)
    {
        int idx = origins[i].x + origins[i].y * X_WIDTH;
        base_exclusions.insert(idx);
    }

    std::vector<int> current_distances;
    for (unsigned i = 0; i < origins.size(); ++i)
    {
        // Find the distance from the point of origin to each of the targets.
        _path_distance(origins[i], targets, base_exclusions,
                       current_distances);

        // Find the smallest of those distances
        int min_dist = current_distances[0];
        for (unsigned j = 1; j < current_distances.size(); ++j)
            if (current_distances[j] < min_dist)
                min_dist = current_distances[j];

        distances[i] = min_dist;
    }
}

// So the idea is we want to decide which adjacent tiles are in the most 'danger'
// We claim danger is proportional to the minimum distances from the point to a
// (hostile) monster. This function carries out at most 7 searches to calculate
// the distances in question.
bool prioritise_adjacent(const coord_def &target, std::vector<coord_def> & candidates)
{
    radius_iterator los_it(target, LOS_RADIUS, true, true, true);

    std::vector<coord_def> mons_positions;
    // collect hostile monster positions in LOS
    for (; los_it; ++los_it)
    {
        monster* hostile = monster_at(*los_it);

        if (hostile && hostile->attitude == ATT_HOSTILE
            && you.can_see(hostile))
        {
            mons_positions.push_back(hostile->pos());
        }
    }

    if (mons_positions.empty())
    {
        std::random_shuffle(candidates.begin(), candidates.end());
        return (true);
    }

    std::vector<int> distances;

    _point_point_distance(candidates, mons_positions, distances);

    std::vector<point_distance> possible_moves(candidates.size());

    for (unsigned i = 0; i < possible_moves.size(); ++i)
    {
        possible_moves[i].first  = candidates[i];
        possible_moves[i].second = distances[i];
    }

    std::sort(possible_moves.begin(), possible_moves.end(),
              less_second<point_distance>);

    for (unsigned i = 0; i < candidates.size(); ++i)
        candidates[i] = possible_moves[i].first;

    return (true);
}

static bool _prompt_amount(int max, int& selected, const std::string& prompt)
{
    selected = max;
    while (true)
    {
        msg::streams(MSGCH_PROMPT) << prompt << " (" << max << " max) "
                                   << std::endl;

        const unsigned char keyin = get_ch();

        // Cancel
        if (key_is_escape(keyin) || keyin == ' ' || keyin == '0')
        {
            canned_msg(MSG_OK);
            return (false);
        }

        // Default is max
        if (keyin == '\n'  || keyin == '\r')
            return (true);

        // Otherwise they should enter a digit
        if (isadigit(keyin))
        {
            selected = keyin - '0';
            if (selected > 0 && selected <= max)
                return (true);
        }
        // else they entered some garbage?
    }

    return (max);
}

static int _collect_fruit(std::vector<std::pair<int,int> >& available_fruit)
{
    int total = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined() && is_fruit(you.inv[i]))
        {
            total += you.inv[i].quantity;
            available_fruit.push_back(std::make_pair(you.inv[i].quantity, i));
        }
    }
    std::sort(available_fruit.begin(), available_fruit.end());

    return (total);
}

static void _decrease_amount(std::vector<std::pair<int, int> >& available,
                             int amount)
{
    const int total_decrease = amount;
    for (unsigned int i = 0; i < available.size() && amount > 0; i++)
    {
        const int decrease_amount = std::min(available[i].first, amount);
        amount -= decrease_amount;
        dec_inv_item_quantity(available[i].second, decrease_amount);
    }
    if (total_decrease > 1)
        mprf("%d pieces of fruit are consumed!", total_decrease);
    else
        mpr("A piece of fruit is consumed!");
}

// Create a ring or partial ring around the caster.  The user is
// prompted to select a stack of fruit, and then plants are placed on open
// squares adjacent to the user.  Of course, one piece of fruit is
// consumed per plant, so a complete ring may not be formed.
bool fedhas_plant_ring_from_fruit()
{
    // How much fruit is available?
    std::vector<std::pair<int, int> > collected_fruit;
    int total_fruit = _collect_fruit(collected_fruit);

    // How many adjacent open spaces are there?
    std::vector<coord_def> adjacent;
    for (adjacent_iterator adj_it(you.pos()); adj_it; ++adj_it)
    {
        if (mons_class_can_pass(MONS_PLANT, env.grid(*adj_it))
            && !actor_at(*adj_it))
        {
            adjacent.push_back(*adj_it);
        }
    }

    const int max_use = std::min(total_fruit,
                                 static_cast<int>(adjacent.size()));

    // Don't prompt if we can't do anything (due to having no fruit or
    // no squares to place plants on).
    if (max_use == 0)
    {
        if (adjacent.size() == 0)
            mprf("No empty adjacent squares.");
        else
            mprf("No fruit available.");

        return (false);
    }

    prioritise_adjacent(you.pos(), adjacent);

    // Screwing around with display code I don't really understand. -cao
    crawl_state.darken_range = 1;
    viewwindow(false);

    for (int i = 0; i < max_use; ++i)
    {
#ifndef USE_TILE
        coord_def temp = grid2view(adjacent[i]);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);
        put_colour_ch(GREEN, '1' + i);
#else
        tiles.add_overlay(adjacent[i], TILE_INDICATOR + i);
#endif
    }

    // And how many plants does the user want to create?
    int target_count;
    if (!_prompt_amount(max_use, target_count,
                        "How many plants will you create?"))
    {
        // User canceled at the prompt.
        crawl_state.darken_range = -1;
        viewwindow(false);
        return (false);
    }

    const int hp_adjust = you.skills[SK_INVOCATIONS] * 10;

    // The user entered a number, remove all number overlays which
    // are higher than that number.
#ifndef USE_TILE
    unsigned not_used = adjacent.size() - unsigned(target_count);
    for (unsigned i = adjacent.size() - not_used;
         i < adjacent.size();
         i++)
    {
        view_update_at(adjacent[i]);
    }
#else
    // For tiles we have to clear all overlays and redraw the ones
    // we want.
    tiles.clear_overlays();
    for (int i = 0; i < target_count; ++i)
        tiles.add_overlay(adjacent[i], TILE_INDICATOR + i);
#endif

    int created_count = 0;
    for (int i = 0; i < target_count; ++i)
    {
        if (_create_plant(adjacent[i], hp_adjust))
            created_count++;

        // Clear the overlay and draw the plant we just placed.
        // This is somewhat more complicated in tiles.
        view_update_at(adjacent[i]);
#ifdef USE_TILE
        tiles.clear_overlays();
        for (int j = i + 1; j < target_count; ++j)
            tiles.add_overlay(adjacent[j], TILE_INDICATOR + j);
        viewwindow(false);
#endif
        delay(200);
    }

    _decrease_amount(collected_fruit, created_count);

    crawl_state.darken_range = -1;
    viewwindow(false);

    return (created_count);
}

// Create a circle of water around the target, with a radius of
// approximately 2.  This turns normal floor tiles into shallow water
// and turns (unoccupied) shallow water into deep water.  There is a
// chance of spawning plants or fungus on unoccupied dry floor tiles
// outside of the rainfall area.  Return the number of plants/fungi
// created.
int fedhas_rain(const coord_def &target)
{
    int spawned_count = 0;
    int processed_count = 0;

    for (radius_iterator rad(target, LOS_RADIUS, true, true, true); rad; ++rad)
    {
        // Adjust the shape of the rainfall slightly to make it look
        // nicer.  I want a threshold of 2.5 on the euclidean distance,
        // so a threshold of 6 prior to the sqrt is close enough.
        int rain_thresh = 6;
        coord_def local = *rad - target;

        dungeon_feature_type ftype = grd(*rad);

        if (local.abs() > rain_thresh)
        {
            // Maybe spawn a plant on (dry, open) squares that are in
            // LOS but outside the rainfall area.  In open space, there
            // are 213 squares in LOS, and we are going to drop water on
            // (25-4) of those, so if we want x plants to spawn on
            // average in open space, the trial probability should be
            // x/192.
            if (x_chance_in_y(5, 192)
                && !actor_at(*rad)
                && ftype >= DNGN_FLOOR_MIN
                && ftype <= DNGN_FLOOR_MAX)
            {
                const int plant = create_monster(mgen_data
                                     (coinflip() ? MONS_PLANT : MONS_FUNGUS,
                                      BEH_GOOD_NEUTRAL,
                                      &you,
                                      0,
                                      0,
                                      *rad,
                                      MHITNOT,
                                      MG_FORCE_PLACE, GOD_FEDHAS));

                if (plant != -1)
                    spawned_count++;

                processed_count++;
            }

            continue;
        }

        // Turn regular floor squares only into shallow water.
        if (ftype >= DNGN_FLOOR_MIN && ftype <= DNGN_FLOOR_MAX)
        {
            dungeon_terrain_changed(*rad, DNGN_SHALLOW_WATER);

            processed_count++;
        }
        // We can also turn shallow water into deep water, but we're
        // just going to skip cases where there is something on the
        // shallow water.  Destroying items will probably be annoying,
        // and insta-killing monsters is clearly out of the question.
        else if (!actor_at(*rad)
                 && igrd(*rad) == NON_ITEM
                 && ftype == DNGN_SHALLOW_WATER)
        {
            dungeon_terrain_changed(*rad, DNGN_DEEP_WATER);

            processed_count++;
        }

        if (ftype >= DNGN_MINMOVE)
        {
            // Maybe place a raincloud.
            //
            // The rainfall area is 20 (5*5 - 4 (corners) - 1 (center));
            // the expected number of clouds generated by a fixed chance
            // per tile is 20 * p = expected.  Say an Invocations skill
            // of 27 gives expected 5 clouds.
            int max_expected = 5;
            int expected = div_rand_round(max_expected
                                          * you.skills[SK_INVOCATIONS], 27);

            if (x_chance_in_y(expected, 20))
            {
                place_cloud(CLOUD_RAIN, *rad, 10, KC_YOU);

                processed_count++;
            }
        }
    }

    if (spawned_count > 0)
    {
        mprf("%s grow%s in the rain.",
             (spawned_count > 1 ? "Some plants" : "A plant"),
             (spawned_count > 1 ? "" : "s"));
    }

    return (processed_count);
}

// Destroy corpses in the player's LOS (first corpse on a stack only)
// and make 1 giant spore per corpse.  Spores are given the input as
// their starting behavior; the function returns the number of corpses
// processed.
int fedhas_corpse_spores(beh_type behavior, bool interactive)
{
    int count = 0;
    std::vector<stack_iterator> positions;

    for (radius_iterator rad(you.pos(), LOS_RADIUS, true, true, true); rad;
         ++rad)
    {
        if (actor_at(*rad))
            continue;

        for (stack_iterator stack_it(*rad); stack_it; ++stack_it)
        {
            if (stack_it->base_type == OBJ_CORPSES
                && stack_it->sub_type == CORPSE_BODY)
            {
                positions.push_back(stack_it);
                count++;
                break;
            }
        }
    }

    if (count == 0)
        return (count);

    crawl_state.darken_range = 0;
    viewwindow(false);
    for (unsigned i = 0; i < positions.size(); ++i)
    {
#ifndef USE_TILE

        coord_def temp = grid2view(positions[i]->pos);
        cgotoxy(temp.x, temp.y, GOTO_DNGN);

        unsigned color = GREEN | COLFLAG_FRIENDLY_MONSTER;
        color = real_colour(color);

        unsigned character = mons_char(MONS_GIANT_SPORE);
        put_colour_ch(color, character);
#else
        tiles.add_overlay(positions[i]->pos, TILE_SPORE_OVERLAY);
#endif
    }

    if (interactive && yesnoquit("Will you create these spores?",
                                 true, 'y') <= 0)
    {
        crawl_state.darken_range = -1;
        viewwindow(false);
        return (-1);
    }

    for (unsigned i = 0; i < positions.size(); ++i)
    {
        count++;
        int rc = create_monster(mgen_data(MONS_GIANT_SPORE,
                                          behavior,
                                          &you,
                                          0,
                                          0,
                                          positions[i]->pos,
                                          MHITNOT,
                                          MG_FORCE_PLACE));

        if (rc != -1)
        {
            env.mons[rc].flags |= MF_ATT_CHANGE_ATTEMPT;
            if (behavior == BEH_FRIENDLY)
            {
                env.mons[rc].behaviour = BEH_WANDER;
                env.mons[rc].foe = MHITNOT;
            }
        }

        if (mons_skeleton(positions[i]->plus))
            turn_corpse_into_skeleton(*positions[i]);
        else
            destroy_item(positions[i]->index());
    }

    crawl_state.darken_range = -1;
    viewwindow(false);

    return (count);
}

struct monster_conversion
{
    monster_conversion() :
        base_monster(NULL),
        piety_cost(0),
        fruit_cost(0)
    {
    }

    monster* base_monster;
    int piety_cost;
    int fruit_cost;
    monster_type new_type;
};


// Given a monster (which should be a plant/fungus), see if
// fedhas_evolve_flora() can upgrade it, and set up a monster_conversion
// structure for it.  Return true (and fill in possible_monster) if the
// monster can be upgraded, and return false otherwise.
static bool _possible_evolution(const monster* input,
                                monster_conversion & possible_monster)
{
    switch (input->type)
    {
    case MONS_PLANT:
    case MONS_BUSH:
        possible_monster.new_type = MONS_OKLOB_PLANT;
        possible_monster.fruit_cost = 1;
        break;

    case MONS_FUNGUS:
        possible_monster.new_type = MONS_WANDERING_MUSHROOM;
        possible_monster.piety_cost = 1;
        break;

    case MONS_TOADSTOOL:
        possible_monster.new_type = MONS_WANDERING_MUSHROOM;
        possible_monster.piety_cost = 2;
        break;

    case MONS_BALLISTOMYCETE:
        possible_monster.new_type = MONS_HYPERACTIVE_BALLISTOMYCETE;
        possible_monster.piety_cost = 4;
        break;

    default:
        return (false);
    }

    return (true);
}

bool mons_is_evolvable(const monster* mon)
{
    monster_conversion temp;
    return (_possible_evolution(mon, temp));
}

static bool _place_ballisto(const coord_def & pos)
{
    const int ballisto = create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                                  BEH_FRIENDLY,
                                                  &you,
                                                  0,
                                                  0,
                                                  pos,
                                                  MHITNOT,
                                                  MG_FORCE_PLACE,
                                                  GOD_FEDHAS));

    if (ballisto != -1)
    {
        remove_mold(pos);
        mprf("The mold grows into a ballistomycete.");
        mprf("Your piety has decreased.");
        lose_piety(1);
        return (true);
    }

    // Monster placement failing should be quite unusual, but it could happen.
    // Not entirely sure what to say about it, but a more informative message
    // might be good. -cao
    canned_msg(MSG_NOTHING_HAPPENS);
    return (false);
}

bool fedhas_evolve_flora()
{
    monster_conversion upgrade;

    // This is a little sloppy, but cancel early if nothing useful is in
    // range.
    bool in_range = false;
    for (radius_iterator rad(you.get_los()); rad; ++rad)
    {
        const monster* temp = monster_at(*rad);
        if (is_moldy(*rad) && mons_class_can_pass(MONS_BALLISTOMYCETE,
                                                  env.grid(*rad))
            || temp && mons_is_evolvable(temp))
        {
            in_range = true;
            break;
        }
    }

    if (!in_range)
    {
        mprf("No evolvable flora in sight.");
        return (false);
    }

    dist spelld;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_EVOLVABLE_PLANTS;
    args.range = LOS_RADIUS;
    args.needs_path = false;
    args.may_target_monster = false;
    args.show_floor_desc = true;
    args.top_prompt = "Select plant or fungus to evolve.";

    direction(spelld, args);

    if (!spelld.isValid)
    {
        // Check for user cancel.
        canned_msg(MSG_OK);
        return (false);
    }

    monster* const target = monster_at(spelld.target);

    if (!target)
    {
        if (!is_moldy(spelld.target)
            || !mons_class_can_pass(MONS_BALLISTOMYCETE,
                                    env.grid(spelld.target)))
        {
            if (env.grid(spelld.target) == DNGN_TREE)
                mprf("The tree has already reached the pinnacle of evolution.");
            else
                mprf("You must target a plant or fungus.");
            return (false);
        }
        return (_place_ballisto(spelld.target));

    }

    if (!_possible_evolution(target, upgrade))
    {
        if (mons_is_plant(target))
            simple_monster_message(target, " has already reached "
                                   "the pinnacle of evolution.");
        else
            mprf("Only plants or fungi may be upgraded.");

        return (false);
    }

    std::vector<std::pair<int, int> > collected_fruit;
    if (upgrade.fruit_cost)
    {
        const int total_fruit = _collect_fruit(collected_fruit);

        if (total_fruit < upgrade.fruit_cost)
        {
            mprf("Not enough fruit available.");
            return (false);
        }
    }

    if (upgrade.piety_cost && upgrade.piety_cost > you.piety)
    {
        mprf("Not enough piety.");
        return (false);
    }

    switch (target->type)
    {
    case MONS_PLANT:
    case MONS_BUSH:
    {
        std::string evolve_desc = " can now spit acid";
        if (you.skills[SK_INVOCATIONS] >= 20)
            evolve_desc += " continuously";
        else if (you.skills[SK_INVOCATIONS] >= 15)
            evolve_desc += " quickly";
        else if (you.skills[SK_INVOCATIONS] >= 10)
            evolve_desc += " rather quickly";
        else if (you.skills[SK_INVOCATIONS] >= 5)
            evolve_desc += " somewhat quickly";
        evolve_desc += ".";

        simple_monster_message(target, evolve_desc.c_str());
        break;
    }

    case MONS_FUNGUS:
    case MONS_TOADSTOOL:
        simple_monster_message(target,
                               " can now pick up its mycelia and move.");
        break;

    case MONS_BALLISTOMYCETE:
        simple_monster_message(target, " appears agitated.");
        break;

    default:
        break;
    }

    target->upgrade_type(upgrade.new_type, true, true);
    target->god = GOD_FEDHAS;
    target->attitude = ATT_FRIENDLY;
    target->flags |= MF_NO_REWARD;
    target->flags |= MF_ATT_CHANGE_ATTEMPT;
    behaviour_event(target, ME_ALERT);
    mons_att_changed(target);

    // Try to remove slowly dying in case we are upgrading a
    // toadstool, and spore production in case we are upgrading a
    // ballistomycete.
    target->del_ench(ENCH_SLOWLY_DYING);
    target->del_ench(ENCH_SPORE_PRODUCTION);

    if (target->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        target->add_ench(ENCH_EXPLODING);

    target->hit_dice += you.skills[SK_INVOCATIONS];

    if (upgrade.fruit_cost)
        _decrease_amount(collected_fruit, upgrade.fruit_cost);

    if (upgrade.piety_cost)
    {
        lose_piety(upgrade.piety_cost);
        mprf("Your piety has decreased.");
    }

    return (true);
}

static int _lugonu_warp_monster(monster* mon, int pow)
{
    if (mon == NULL)
        return (0);

    if (!mon->friendly())
        behaviour_event(mon, ME_ANNOY, MHITYOU);

    if (mon->check_res_magic(pow * 2))
    {
        mprf("%s %s.",
             mon->name(DESC_CAP_THE).c_str(), mons_resist_string(mon));
        return (1);
    }

    const int damage = 1 + random2(pow / 6);
    if (mons_genus(mon->type) == MONS_BLINK_FROG)
        mon->heal(damage, false);
    else if (!mon->check_res_magic(pow))
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
    apply_monsters_around_square(_lugonu_warp_monster, you.pos(), pow);
}

void lugonu_bend_space()
{
    const int pow = 4 + skill_bump(SK_INVOCATIONS);
    const bool area_warp = random2(pow) > 9;

    mprf("Space bends %saround you!", area_warp ? "sharply " : "");

    if (area_warp)
        _lugonu_warp_area(pow);

    random_blink(false, true);

    const int damage = roll_dice(1, 4);
    ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC, "a spatial distortion");
}

bool is_ponderousifiable(const item_def& item)
{
    return (item.base_type == OBJ_ARMOUR
            && you_tran_can_wear(item)
            && !is_shield(item)
            && !is_artefact(item)
            && get_armour_ego_type(item) != SPARM_RUNNING
            && get_armour_ego_type(item) != SPARM_PONDEROUSNESS);
}

bool ponderousify_armour()
{
    const int item_slot = prompt_invent_item("Make which item ponderous?",
                                             MT_INVLIST, OSEL_PONDER_ARM,
                                             true, true, false);

    if (prompt_failed(item_slot))
        return (false);

    item_def& arm(you.inv[item_slot]);
    if (!is_ponderousifiable(arm)) // player pressed '*' and made a bad choice
    {
        mpr("That item can't be made ponderous.");
        return (false);
    }

    const int old_ponder = player_ponderousness();
    cheibriados_make_item_ponderous(arm);

    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    simple_god_message(" says: Use this wisely!");

    const int new_ponder = player_ponderousness();
    if (new_ponder > old_ponder)
    {
        mprf("You feel %s ponderous.",
             old_ponder? "even more" : "rather");
        che_handle_change(CB_PONDEROUS, new_ponder - old_ponder);
    }

    return (true);
}

void cheibriados_time_bend(int pow)
{
    mpr("The flow of time bends around you.");

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon && !mons_is_stationary(mon))
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

static int _slouch_monsters(coord_def where, int pow, int, actor* agent)
{
    monster* mon = monster_at(where);
    if (mon == NULL || mons_is_stationary(mon) || mon->cannot_move()
        || mons_is_projectile(mon->type)
        || mon->asleep() && !mons_is_confused(mon))
    {
        return (0);
    }

    int dmg = (mon->speed - 1000/player_movement_speed()/player_speed());
    dmg = (dmg > 0 ? roll_dice(dmg*4, 3)/2 : 0);

    mon->hurt(agent, dmg, BEAM_MMISSILE, true);
    return (1);
}

int cheibriados_slouch(int pow)
{
    return (apply_area_visible(_slouch_monsters, pow, false, &you));
}

void cheibriados_time_step(int pow) // pow is the number of turns to skip
{
    const coord_def old_pos = you.pos();

    mpr("You step out of the flow of time.");
    flash_view(LIGHTBLUE);
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
    // Update corpses, etc.  This does also shift monsters, but only by
    // a tiny bit.
    update_level(pow * 10);

#ifndef USE_TILE
    delay(1000);
#endif

    monster* mon;
    if (mon = monster_at(old_pos))
    {
        mon->blink();
        if (mon = monster_at(old_pos))
            mon->teleport(true);
    }

    you.moveto(old_pos);
    you.duration[DUR_TIME_STEP] = 0;

    flash_view(0);
    mpr("You return to the normal time flow.");
}

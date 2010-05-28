/*
 *  File:       effects.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "effects.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <queue>
#include <set>
#include <cmath>

#include "externs.h"
#include "options.h"

#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "decks.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "dungeon.h"
#include "directn.h"
#include "dgnevent.h"
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "food.h"
#include "hiscores.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "map_knowledge.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-pathfind.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells2.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "xom.h"

int holy_word_player(int pow, int caster, actor *attacker)
{
    if (!you.undead_or_demonic())
        return (0);

    int hploss;

    // Holy word won't kill its user.
    if (attacker == &you)
        hploss = std::max(0, you.hp / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 3);

    if (!hploss)
        return (0);

    mpr("You are blasted by holy energy!");

    const char *aux = "holy word";

    kill_method_type type = KILLED_BY_MONSTER;
    if (invalid_monster_index(caster))
    {
        type = KILLED_BY_SOMETHING;
        if (crawl_state.is_god_acting())
            type = KILLED_BY_DIVINE_WRATH;

        switch (caster)
        {
        case HOLY_WORD_SCROLL:
            aux = "scroll of holy word";
            break;

        case HOLY_WORD_ZIN:
            aux = "Zin's holy word";
            break;

        case HOLY_WORD_TSO:
            aux = "the Shining One's holy word";
            break;
        }
    }

    ouch(hploss, caster, type, aux);

    return (1);
}

int holy_word_monsters(coord_def where, int pow, int caster,
                       actor *attacker)
{
    pow = std::min(300, pow);

    int retval = 0;

    // Is the player in this cell?
    if (where == you.pos())
        retval = holy_word_player(pow, caster, attacker);

    // Is a monster in this cell?
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (retval);

    if (!monster->alive() || !monster->undead_or_demonic())
        return (retval);

    int hploss;

    // Holy word won't kill its user.
    if (attacker == monster)
        hploss = std::max(0, monster->hit_points / 2 - 1);
    else
        hploss = roll_dice(3, 15) + (random2(pow) / 3);

    if (hploss)
        simple_monster_message(monster, " convulses!");

    monster->hurt(attacker, hploss, BEAM_MISSILE, false);

    if (hploss)
    {
        retval = 1;

        if (monster->alive())
        {
            // Holy word won't annoy, slow, or frighten its user.
            if (attacker != monster)
            {
                // Currently, holy word annoys the monsters it affects
                // because it can kill them, and because hostile
                // monsters don't use it.
                if (attacker != NULL)
                    behaviour_event(monster, ME_ANNOY, attacker->mindex());

                if (monster->speed_increment >= 25)
                    monster->speed_increment -= 20;

                monster->add_ench(ENCH_FEAR);
            }
        }
        else
            monster->hurt(attacker, INSTANT_DEATH);
    }

    return (retval);
}

int holy_word(int pow, int caster, const coord_def& where, bool silent,
              actor *attacker)
{
    if (!silent && attacker)
    {
        mprf("%s %s a Word of immense power!",
             attacker->name(DESC_CAP_THE).c_str(),
             attacker->conj_verb("speak").c_str());
    }

    // We could use actor.get_los(), but maybe it's NULL.
    los_def los(where);
    los.update();
    int r = 0;
    for (radius_iterator ri(&los); ri; ++ri)
        r += holy_word_monsters(*ri, 0, caster, attacker);
    return (r);
}

int torment_player(int pow, int caster)
{
    ASSERT(!crawl_state.game_is_arena());

    UNUSED(pow);

    // [dshaligram] Switched to using ouch() instead of dec_hp() so that
    // notes can also track torment and activities can be interrupted
    // correctly.
    int hploss = 0;

    if (!player_res_torment(false))
    {
        // Negative energy resistance can alleviate torment.
        hploss = std::max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
    }

    // Kiku protects you from torment to a degree.
    bool kiku_shielding_player =
        (you.religion == GOD_KIKUBAAQUDGHA
        && !player_under_penance()
        && you.piety > 80
        && you.gift_timeout == 0); // no protection during pain branding weapon

    if (kiku_shielding_player)
    {
        if (hploss > 0)
        {
            if (random2(600) < you.piety) // 13.33% to 33.33% chance
            {
                hploss = 0;
                simple_god_message(" shields you from torment!");
            }
            else if (random2(250) < you.piety) // 24% to 80% chance
            {
                hploss -= random2(hploss - 1);
                simple_god_message(" partially shields you from torment!");
            }
        }
    }


    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return (0);
    }

    mpr("Your body is wracked with pain!");

    const char *aux = "torment";

    kill_method_type type = KILLED_BY_MONSTER;
    if (invalid_monster_index(caster))
    {
        type = KILLED_BY_SOMETHING;
        if (crawl_state.is_god_acting())
            type = KILLED_BY_DIVINE_WRATH;

        switch (caster)
        {
        case TORMENT_CARDS:
        case TORMENT_SPELL:
            aux = "Symbol of Torment";
            break;

        case TORMENT_SPWLD:
            // XXX: If we ever make any other weapon / randart eligible
            // to torment, this will be incorrect.
            aux = "Sceptre of Torment";
            break;

        case TORMENT_SCROLL:
            aux = "scroll of torment";
            break;

        case TORMENT_XOM:
            type = KILLED_BY_XOM;
            aux = "Xom's torment";
            break;

        case TORMENT_KIKUBAAQUDGHA:
            aux = "Kikubaaqudgha's torment";
            break;
        }
    }

    ouch(hploss, caster, type, aux);

    return (1);
}

// torment_monsters() is called with power 0 because torment is
// UNRESISTABLE except for having torment resistance!  Even if we used
// maximum power of 1000, high level monsters and characters would save
// too often.  (GDL)

int torment_monsters(coord_def where, int pow, int caster, actor *attacker)
{
    UNUSED(pow);

    int retval = 0;

    // Is the player in this cell?
    if (where == you.pos())
        retval = torment_player(0, caster);

    // Is a monster in this cell?
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (retval);

    if (!monster->alive() || monster->res_torment())
        return (retval);

    int hploss = std::max(0, monster->hit_points / 2 - 1);

    if (hploss)
    {
        simple_monster_message(monster, " convulses!");

        // Currently, torment doesn't annoy the monsters it affects
        // because it can't kill them, and because hostile monsters use
        // it.  It does alert them, though.
        // XXX: attacker isn't passed through "int torment()".
        behaviour_event(monster, ME_ALERT,
                        attacker ? attacker->mindex() : MHITNOT);
    }

    monster->hurt(NULL, hploss, BEAM_TORMENT_DAMAGE);

    if (hploss)
        retval = 1;

    return (retval);
}

int torment(int caster, const coord_def& where)
{
    actor* attacker = NULL;

    if (!invalid_monster_index(caster))
        attacker = &menv[caster];
    else if (caster < 0 && you.turn_is_over && where == you.pos()
             && !(crawl_state.is_god_acting()
                  && crawl_state.is_god_retribution()))
    {
        // Maybe monsters should still consider it your fault if it's
        // caused by divine retribution?
        attacker = &you;
    }

    los_def los(where);
    los.update();
    int r = 0;
    for (radius_iterator ri(&los); ri; ++ri)
        r += torment_monsters(*ri, 0, caster, attacker);
    return (r);
}

void immolation(int pow, int caster, coord_def where, bool known,
                actor *attacker)
{
    ASSERT(!crawl_state.game_is_arena());

    const char *aux = "immolation";

    bolt beam;

    if (caster < 0)
    {
        switch (caster)
        {
        case IMMOLATION_SCROLL:
            aux = "scroll of immolation";
            break;

        case IMMOLATION_SPELL:
            aux = "a fiery explosion";
            break;

        case IMMOLATION_TOME:
            aux = "an exploding Tome of Destruction";
            break;
        }
    }

    beam.flavour       = BEAM_FIRE;
    beam.glyph         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage        = dice_def(3, pow);
    beam.target        = where;
    beam.name          = "fiery explosion";
    beam.colour        = RED;
    beam.aux_source    = aux;
    beam.ex_size       = 2;
    beam.is_explosion  = true;
    beam.effect_known  = known;
    beam.affects_items = (caster != IMMOLATION_SCROLL);

    if (caster == IMMOLATION_GENERIC)
    {
        beam.thrower     = KILL_MISC;
        beam.beam_source = NON_MONSTER;
    }
    else if (attacker == &you)
    {
        beam.thrower     = KILL_YOU;
        beam.beam_source = NON_MONSTER;
    }
    else
    {
        beam.thrower     = KILL_MON;
        beam.beam_source = attacker->mindex();
    }

    beam.explode();
}

static bool _conduct_electricity_hit(bolt& beam, actor* victim,
                                     int dmg, int corpse)
{
    if (!victim->alive() || victim->res_elec() > 0 || victim->airborne())
        return (false);

    return (true);
}

static bool _conduct_electricity_damage(bolt &beam, actor* victim,
                                        int &dmg, std::string &dmg_msg)
{
    dmg = (10 + random2(15)) / 2;

    return (false);
}

static bool _conduct_electricity_aoe(bolt& beam, const coord_def& target)
{
    if (feat_is_water(grd(target)))
        return (true);

    return (false);
}

void conduct_electricity(coord_def where, actor *attacker)
{
    bolt beam;

    beam.flavour       = BEAM_ELECTRICITY;
    beam.glyph         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage        = dice_def(1, 15);
    beam.target        = where;
    beam.name          = "electric current";
    beam.hit_verb      = "shocks";
    beam.colour        = ETC_ELECTRICITY;
    beam.aux_source    = "arcing electricity";
    beam.ex_size       = 1;
    beam.is_explosion  = true;
    beam.effect_known  = true;
    beam.affects_items = false;
    beam.aoe_funcs.push_back(_conduct_electricity_aoe);
    beam.hit_funcs.push_back(_conduct_electricity_hit);
    beam.damage_funcs.push_back(_conduct_electricity_damage);

    if (attacker == &you)
    {
        beam.thrower     = KILL_YOU;
        beam.beam_source = NON_MONSTER;
    }
    else
    {
        beam.thrower     = KILL_MON;
        beam.beam_source = attacker->mindex();
    }

    beam.explode(false, true);
}

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker)
{
    ASSERT(!crawl_state.game_is_arena());

    const char *aux = "cleansing flame";

    bolt beam;

    if (caster < 0)
    {
        switch (caster)
        {
        case CLEANSING_FLAME_TSO:
            aux = "the Shining One's cleansing flame";
            break;
        }
    }

    beam.flavour      = BEAM_HOLY;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.damage       = dice_def(2, pow);
    beam.target       = you.pos();
    beam.name         = "golden flame";
    beam.colour       = YELLOW;
    beam.aux_source   = aux;
    beam.ex_size      = 2;
    beam.is_explosion = true;

    if (caster == CLEANSING_FLAME_GENERIC || caster == CLEANSING_FLAME_TSO)
    {
        beam.thrower     = KILL_MISC;
        beam.beam_source = NON_MONSTER;
    }
    else if (attacker == &you)
    {
        beam.thrower     = KILL_YOU;
        beam.beam_source = NON_MONSTER;
    }
    else
    {
        beam.thrower     = KILL_MON;
        beam.beam_source = attacker->mindex();
    }

    beam.explode();
}

static std::string _who_banished(const std::string &who)
{
    return (who.empty() ? who : " (" + who + ")");
}

void banished(dungeon_feature_type gate_type, const std::string &who)
{
    ASSERT(!crawl_state.game_is_arena());

#ifdef DGL_MILESTONES
    if (gate_type == DNGN_ENTER_ABYSS)
    {
        mark_milestone("abyss.enter",
                       "is cast into the Abyss!" + _who_banished(who));
    }
    else if (gate_type == DNGN_EXIT_ABYSS)
    {
        mark_milestone("abyss.exit",
                       "escaped from the Abyss!" + _who_banished(who));
    }
#endif

    std::string cast_into;

    switch (gate_type)
    {
    case DNGN_ENTER_ABYSS:
        if (you.level_type == LEVEL_ABYSS)
        {
            mpr("You feel trapped.");
            return;
        }
        cast_into = "the Abyss";
        break;

    case DNGN_EXIT_ABYSS:
        if (you.level_type != LEVEL_ABYSS)
        {
            mpr("You feel dizzy for a moment.");
            return;
        }
        break;

    case DNGN_ENTER_PANDEMONIUM:
        if (you.level_type == LEVEL_PANDEMONIUM)
        {
            mpr("You feel trapped.");
            return;
        }
        cast_into = "Pandemonium";
        break;

    case DNGN_TRANSIT_PANDEMONIUM:
        if (you.level_type != LEVEL_PANDEMONIUM)
        {
            banished(DNGN_ENTER_PANDEMONIUM, who);
            return;
        }
        break;

    case DNGN_EXIT_PANDEMONIUM:
        if (you.level_type != LEVEL_PANDEMONIUM)
        {
            mpr("You feel dizzy for a moment.");
            return;
        }
        break;

    case DNGN_ENTER_LABYRINTH:
        if (you.level_type == LEVEL_LABYRINTH)
        {
            mpr("You feel trapped.");
            return;
        }
        cast_into = "a Labyrinth";
        break;

    case DNGN_ENTER_HELL:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
        if (player_in_hell() || player_in_branch(BRANCH_VESTIBULE_OF_HELL))
        {
            mpr("You feel dizzy for a moment.");
            return;
        }
        cast_into = "Hell";
        break;

    default:
        mprf(MSGCH_DIAGNOSTICS, "Invalid banished() gateway %d",
             static_cast<int>(gate_type));
        ASSERT(false);
    }

    // Now figure out how we got here.
    if (crawl_state.is_god_acting())
    {
        // down_stairs() will take care of setting things.
        you.entry_cause = EC_UNKNOWN;
    }
    else if (who.find("self") != std::string::npos || who == you.your_name
             || who == "you" || who == "You")
    {
        you.entry_cause = EC_SELF_EXPLICIT;
    }
    else if (who.find("distortion") != std::string::npos)
    {
        if (who.find("wield") != std::string::npos)
        {
            if (who.find("unknowing") != std::string::npos)
                you.entry_cause = EC_SELF_ACCIDENT;
            else
                you.entry_cause = EC_SELF_RISKY;
        }
        else if (who.find("affixation") != std::string::npos)
            you.entry_cause = EC_SELF_ACCIDENT;
        else if (who.find("branding")  != std::string::npos)
            you.entry_cause = EC_SELF_RISKY;
        else
            you.entry_cause = EC_MONSTER;
    }
    else if (who == "drawing a card")
        you.entry_cause = EC_SELF_RISKY;
    else if (who.find("you miscast") != std::string::npos)
        you.entry_cause = EC_MISCAST;
    else if (who == "wizard command")
        you.entry_cause = EC_SELF_EXPLICIT;
    else if (who.find("effects of Hell") != std::string::npos)
        you.entry_cause = EC_ENVIRONMENT;
    else if (who.find("Zot") != std::string::npos)
        you.entry_cause = EC_TRAP;
    else if (who.find("trap") != std::string::npos)
        you.entry_cause = EC_TRAP;
    else
        you.entry_cause = EC_MONSTER;

    if (!crawl_state.is_god_acting())
        you.entry_cause_god = GOD_NO_GOD;

    if (!cast_into.empty() && you.entry_cause != EC_SELF_EXPLICIT)
    {
        const std::string what = "Cast into " + cast_into + _who_banished(who);
        take_note(Note(NOTE_MESSAGE, 0, 0, what.c_str()), true);
    }

    down_stairs(you.absdepth0, gate_type, you.entry_cause);  // heh heh
}

bool forget_spell(void)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.spell_no)
        return (false);

    // find a random spell to forget:
    int slot = -1;
    int num  = 0;

    for (int i = 0; i < 25; i++)
    {
        if (you.spells[i] != SPELL_NO_SPELL)
        {
            num++;
            if (one_chance_in( num ))
                slot = i;
        }
    }

    if (slot == -1)              // should never happen though
        return (false);

    mprf("Your knowledge of %s becomes hazy all of a sudden, and you forget "
         "the spell!", spell_title(you.spells[slot]));

    del_spell_from_memory_by_slot( slot );
    return (true);
}

void direct_effect(monsters *source, spell_type spell,
                   bolt &pbolt, actor *defender)
{
    monsters *def = defender->as_monster();

    if (def)
    {
        // annoy the target
        behaviour_event(def, ME_ANNOY, source->mindex());
    }

    int damage_taken = 0;

    switch (spell)
    {
    case SPELL_SMITING:
        if (def)
            simple_monster_message(def, " is smitten.");
        else
            mpr("Something smites you!");

        pbolt.name       = "smiting";
        pbolt.flavour    = BEAM_MISSILE;
        pbolt.aux_source = "by divine providence";
        damage_taken     = 7 + random2avg(11, 2);
        break;

    case SPELL_AIRSTRIKE:
        // Damage averages 14 for 5HD, 18 for 10HD, 28 for 20HD.
        if (def)
            simple_monster_message(def, " is struck by the twisting air!");
        else
            mpr("The air twists around and strikes you!");

        pbolt.name       = "airstrike";
        pbolt.flavour    = BEAM_MISSILE;
        pbolt.aux_source = "by the air";

        damage_taken     = 10 + 2 * source->hit_dice;

        // Apply "bonus" against flying/levitating characters after AC
        // has been checked.
        if (defender->flight_mode() != FL_NONE)
        {
            damage_taken *= 3;
            damage_taken /= 2;
        }

        // Previous method of damage calculation (in line with player
        // airstrike) had absurd variance.
        damage_taken = random2avg(damage_taken, 3);
        damage_taken -= random2(defender->armour_class());
        break;

    case SPELL_BRAIN_FEED:
        if (!def)
        {
            // lose_stat() must come last {dlb}
            if (one_chance_in(3)
                && lose_stat(STAT_INT, 1, source))
            {
                mpr("Something feeds on your intellect!");
                xom_is_stimulated(50);
            }
            else
                mpr("Something tries to feed on your intellect!");
        }
        break;

    case SPELL_HAUNT:
        if (!def)
            mpr("You feel haunted.");
        else
            mpr("You sense an evil presence.");
        mons_cast_haunt(source);
        break;

    case SPELL_MISLEAD:
        if (!def)
            mons_cast_mislead(source);
        else
            defender->confuse(source, source->hit_dice * 12);
        break;

    default:
        ASSERT(false);
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
    {
        if (def)
            def->hurt(source, damage_taken);
        else
            ouch(damage_taken, pbolt.beam_source, KILLED_BY_BEAM,
                 pbolt.aux_source.c_str());
    }
}

void random_uselessness(int scroll_slot)
{
    ASSERT(!crawl_state.game_is_arena());

    int temp_rand = random2(8);

    // If this isn't from a scroll, skip the first two possibilities.
    if (scroll_slot == -1)
        temp_rand = 2 + random2(6);

    switch (temp_rand)
    {
    case 0:
        mprf("The dust glows %s!", weird_glowing_colour().c_str());
        break;

    case 1:
        mprf("The scroll reassembles itself in your %s!",
             your_hand(true).c_str());
        inc_inv_item_quantity(scroll_slot, 1);
        break;

    case 2:
        if (you.weapon())
        {
            mprf("%s glows %s for a moment.",
                 you.weapon()->name(DESC_CAP_YOUR).c_str(),
                 weird_glowing_colour().c_str());
        }
        else
        {
            mprf("Your %s glow %s for a moment.",
                 your_hand(true).c_str(), weird_glowing_colour().c_str());
        }
        break;

    case 3:
        if (you.species == SP_MUMMY)
            mpr("Your bandages flutter.");
        else // if (you.can_smell())
            mprf("You smell %s.", weird_smell().c_str());
        break;

    case 4:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 5:
        temp_rand = random2(3);
        if (player_mutation_level(MUT_BEAK) || one_chance_in(3))
            mpr("Your brain hurts!");
        else if (you.species == SP_MUMMY || coinflip())
            mpr("Your ears itch!");
        else
            mpr("Your nose twitches suddenly!");
        break;

    case 6:
        mpr("You hear the tinkle of a tiny bell.", MSGCH_SOUND);
        noisy(2, you.pos());
        cast_summon_butterflies(100);
        break;

    case 7:
        mprf(MSGCH_SOUND, "You hear %s.", weird_sound().c_str());
        noisy(2, you.pos());
        break;
    }
}

static armour_type _random_nonbody_armour_type()
{
    const armour_type at =
        static_cast<armour_type>(
            random_choose(ARM_SHIELD, ARM_CLOAK, ARM_HELMET,
                          ARM_GLOVES, ARM_BOOTS, -1));
    return (at);
}

const int max_has_value = 100;
typedef FixedVector<int, max_has_value> has_vector;

static armour_type _pick_wearable_armour(const armour_type arm)
{
    armour_type result = arm;

    // Some species specific fitting problems.
    // FIXME: switch to the cleaner logic in can_wear_armour()
    switch (you.species)
    {
    case SP_OGRE:
    case SP_TROLL:
    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:
    case SP_SPRIGGAN:
        if (arm == ARM_GLOVES
            || arm == ARM_BOOTS
            || arm == ARM_CENTAUR_BARDING
            || arm == ARM_NAGA_BARDING)
        {
            result = ARM_ROBE;  // no heavy armour
        }
        else if (arm == ARM_SHIELD)
        {
            if (you.species == SP_SPRIGGAN)
                result = ARM_BUCKLER;
            else if (coinflip()) // giant races: 50/50 shield/large shield
                result = ARM_LARGE_SHIELD;
        }
        else if (arm == NUM_ARMOURS)
        {
            result = ARM_ROBE;  // no heavy armour, see below
        }
        break;

    case SP_NAGA:
        if (arm == ARM_BOOTS || arm == ARM_CENTAUR_BARDING)
            result = ARM_NAGA_BARDING;
        break;

    case SP_CENTAUR:
        if (arm == ARM_BOOTS || arm == ARM_NAGA_BARDING)
            result = ARM_CENTAUR_BARDING;
        break;

    default:
        if (arm == ARM_CENTAUR_BARDING || arm == ARM_NAGA_BARDING)
            result = ARM_BOOTS;
        break;
    }

    // Mutation specific problems (horns allow caps).
    if (result == ARM_BOOTS && !player_has_feet()
        || result == ARM_GLOVES && you.has_claws(false) >= 3)
    {
        result = NUM_ARMOURS;
    }

    // Do this here, before acquirement()'s call to can_wear_armour(),
    // so that caps will be just as common as helmets for those
    // that can't wear helmets.
    // We check for the mutation directly to avoid acquirement fiddles
    // with vampires.
    if (arm == ARM_HELMET
        && (!you_can_wear(EQ_HELMET)
            || you.mutation[MUT_HORNS]
            || you.mutation[MUT_ANTENNAE]))
    {
        result = coinflip() ? ARM_CAP : ARM_WIZARD_HAT;
    }

    return (result);
}

static armour_type _acquirement_armour_subtype(bool divine)
{
    // Increasing the representation of the non-body armour
    // slots here to make up for the fact that there's only
    // one type of item for most of them. -- bwr
    //
    // NUM_ARMOURS is body armour and handled below
    armour_type result = NUM_ARMOURS;

    if (divine)
    {
        if (coinflip())
            result = _random_nonbody_armour_type();
    }
    else
    {
        static const equipment_type armour_slots[] =
            {  EQ_SHIELD, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS   };

        equipment_type picked = EQ_BODY_ARMOUR;
        const int num_slots = ARRAYSZ(armour_slots);
        // Start count at 1, for body armour (already picked).
        for (int i = 0, count = 1; i < num_slots; ++i)
            if (you_can_wear(armour_slots[i], true) && one_chance_in(++count))
                picked = armour_slots[i];

        switch (picked)
        {
        case EQ_SHIELD:
            result = ARM_SHIELD; break;
        case EQ_CLOAK:
            result = ARM_CLOAK;  break;
        case EQ_HELMET:
            result = ARM_HELMET; break;
        case EQ_GLOVES:
            result = ARM_GLOVES; break;
        case EQ_BOOTS:
            result = ARM_BOOTS;  break;
        default:
        case EQ_BODY_ARMOUR:
            result = NUM_ARMOURS; break;
        }
    }

    result = _pick_wearable_armour(result);

    // Now we'll randomly pick a body armour up to plate mail (light
    // only in the case of robes or animal skins).  Unlike before, now
    // we're only giving out the finished products here, never the
    // hides. - bwr
    if (result == NUM_ARMOURS || result == ARM_ROBE)
    {
        // Start with normal base armour.
        if (result == ARM_ROBE)
        {
            // Animal skins don't get egos, so make them less likely.
            result = (one_chance_in(4) ? ARM_ANIMAL_SKIN : ARM_ROBE);

            // Armour-restricted species get a bonus chance at
            // troll/dragon armour.  (In total, the chance is almost
            // 10%.)
            if (one_chance_in(20))
            {
                result = static_cast<armour_type>(
                    random_choose_weighted(3, ARM_TROLL_LEATHER_ARMOUR,
                                           3, ARM_STEAM_DRAGON_ARMOUR,
                                           1, ARM_SWAMP_DRAGON_ARMOUR,
                                           1, ARM_DRAGON_ARMOUR,
                                           0));
            }

            // Non-god acquirement not only has a much better chance, but
            // can give high-end ones as well.
            if (!divine && one_chance_in(5))
            {
                static_cast<armour_type>(random_choose(
                        ARM_DRAGON_ARMOUR,
                        ARM_ICE_DRAGON_ARMOUR,
                        ARM_STEAM_DRAGON_ARMOUR,
                        ARM_MOTTLED_DRAGON_ARMOUR,
                        ARM_STORM_DRAGON_ARMOUR,
                        ARM_GOLD_DRAGON_ARMOUR,
                        ARM_SWAMP_DRAGON_ARMOUR,
                        -1));
            }
        }
        else
        {
            if (divine)
            {
                const armour_type armours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                                ARM_RING_MAIL, ARM_SCALE_MAIL,
                                                ARM_CHAIN_MAIL, ARM_SPLINT_MAIL,
                                                ARM_BANDED_MAIL, ARM_PLATE_MAIL };

                result = static_cast<armour_type>(RANDOM_ELEMENT(armours));

                if (one_chance_in(10) && you.skills[SK_ARMOUR] >= 10)
                    result = ARM_CRYSTAL_PLATE_MAIL;

                if (one_chance_in(12))
                    result = ARM_ANIMAL_SKIN;
            }
            else
            {
                const armour_type armours[] =
                    { ARM_ANIMAL_SKIN, ARM_ROBE, ARM_LEATHER_ARMOUR,
                      ARM_RING_MAIL, ARM_SCALE_MAIL, ARM_CHAIN_MAIL,
                      ARM_BANDED_MAIL, ARM_SPLINT_MAIL, ARM_PLATE_MAIL,
                      ARM_CRYSTAL_PLATE_MAIL };

                const int num_arms = ARRAYSZ(armours);

                // Weight sub types relative to (armour skill + 3).
                // Actually, the AC improvement is not linear, and we
                // might also want to take into account Dodging/Stealth
                // and Strength, but this is definitely better than the
                // random chance above.

                // This formula makes sense only for casters.
                const int skill = std::min(27, you.skills[SK_ARMOUR] + 3);
                int total = 0;
                for (int i = 0; i < num_arms; ++i)
                {
                    int weight = std::max(1, 27 - abs(skill - i*3));
                    weight = weight * weight * weight;
                    total += weight;
                    if (x_chance_in_y(weight, total))
                        result = armours[i];
                }
                // ... so we override it for heavy meleers, who get 50% plates.
                // (Should it be more?  A scale mail is wasted acquirement, even
                // if it's any but most über randart).
                if (random2(you.skills[SK_SPELLCASTING] + you.skills[SK_DODGING])
                    < random2(you.skills[SK_ARMOUR]) && coinflip())
                {
                    result = one_chance_in(4) ? ARM_CRYSTAL_PLATE_MAIL :
                                                ARM_PLATE_MAIL;
                }
            }
        }

        // Everyone can wear things made from hides.
        if (one_chance_in(20))
        {
            result = static_cast<armour_type>(
                random_choose_weighted(20, ARM_TROLL_LEATHER_ARMOUR,
                                       20, ARM_STEAM_DRAGON_ARMOUR,
                                       15, ARM_MOTTLED_DRAGON_ARMOUR,
                                       15, ARM_SWAMP_DRAGON_ARMOUR,
                                       10, ARM_DRAGON_ARMOUR,
                                       10, ARM_ICE_DRAGON_ARMOUR,
                                        5, ARM_STORM_DRAGON_ARMOUR,
                                        5, ARM_GOLD_DRAGON_ARMOUR,
                                        0));
        }
    }

    return (result);
}

// If armour acquirement turned up a non-ego non-artefact armour item,
// see whether the player has any unfilled equipment slots.  If so,
// hand out a plain (and possibly negatively enchanted) item of that
// type.  Otherwise, keep the original armour.
static bool _try_give_plain_armour(item_def &arm)
{
    static const equipment_type armour_slots[] =
        {  EQ_SHIELD, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS  };

    equipment_type picked = EQ_BODY_ARMOUR;
    const int num_slots = ARRAYSZ(armour_slots);
    for (int i = 0, count = 0; i < num_slots; ++i)
    {
        if (!you_can_wear(armour_slots[i], true))
            continue;

        if (you.equip[armour_slots[i]] != -1)
            continue;

        // Consider shield slot filled in some cases.
        if (armour_slots[i] == EQ_SHIELD)
        {
            const item_def* weapon = you.weapon();

            // Unarmed fighters don't need shields.
            if (!weapon && you.skills[SK_UNARMED_COMBAT] > random2(8))
                continue;

            // Two-handed weapons and ranged weapons conflict with shields.
            if (weapon
                && (hands_reqd(*weapon, you.body_size()) == HANDS_TWO
                    || is_range_weapon(*weapon)))
            {
                continue;
            }
        }

        if (one_chance_in(++count))
            picked = armour_slots[i];
    }

    // All available secondary slots already filled.
    if (picked == EQ_BODY_ARMOUR)
        return (false);

    armour_type result = NUM_ARMOURS;
    switch (picked)
    {
    case EQ_SHIELD:
        result = ARM_SHIELD; break;
    case EQ_CLOAK:
        result = ARM_CLOAK;  break;
    case EQ_HELMET:
        result = ARM_HELMET; break;
    case EQ_GLOVES:
        result = ARM_GLOVES; break;
    case EQ_BOOTS:
        result = ARM_BOOTS;  break;
    default:
        return (false);
    }
    // Clear the description flag.
    set_equip_desc(arm, ISFLAG_NO_DESC);
    arm.sub_type = _pick_wearable_armour(result);
    arm.plus = random2(5) - 2;

    const int max_ench = armour_max_enchant(arm);
    if (arm.plus > max_ench)
        arm.plus = max_ench;
    else if (arm.plus < -max_ench)
        arm.plus = -max_ench;

    return (true);
}

// Write results into arguments.
void _acquirement_determine_food(int& type_wanted, int& quantity,
                                 const has_vector& already_has)
{
    // Food is a little less predictable now. - bwr
    if (you.species == SP_GHOUL)
        type_wanted = one_chance_in(10) ? FOOD_ROYAL_JELLY : FOOD_CHUNK;
    else if (you.species == SP_VAMPIRE)
    {
        // Vampires really don't want any OBJ_FOOD but OBJ_CORPSES
        // but it's easier to just give them a potion of blood
        // class type is set elsewhere
        type_wanted = POT_BLOOD;
    }
    else if (you.religion == GOD_FEDHAS)
    {
        // Fedhas worshippers get fruit to use for growth and evolution
        type_wanted = one_chance_in(3) ? FOOD_BANANA : FOOD_ORANGE;
    }
    else
    {
        // Meat is better than bread (except for herbivores), and
        // by choosing it as the default we don't have to worry
        // about special cases for carnivorous races (e.g. kobolds)
        type_wanted = FOOD_MEAT_RATION;

        if (player_mutation_level(MUT_HERBIVOROUS))
            type_wanted = FOOD_BREAD_RATION;

        // If we have some regular rations, then we're probably more
        // interested in faster foods (especially royal jelly)...
        // otherwise the regular rations should be a good enough offer.
        if (already_has[FOOD_MEAT_RATION]
            + already_has[FOOD_BREAD_RATION] >= 2 || coinflip())
        {
            type_wanted = one_chance_in(5) ? FOOD_HONEYCOMB
                : FOOD_ROYAL_JELLY;
        }
    }

    quantity = 3 + random2(5);

    if (type_wanted == FOOD_BANANA || type_wanted == FOOD_ORANGE)
    {
        quantity = 8 + random2avg(15, 2);
    }
    // giving more of the lower food value items
    else if (type_wanted == FOOD_HONEYCOMB || type_wanted == FOOD_CHUNK)
    {
        quantity += random2avg(10, 2);
    }
    else if (type_wanted == POT_BLOOD)
    {
    // this was above in the vampire block, but gets overwritten by line 1371
    // so moving here {due}
        quantity = 2 + random2(4);
    }
}

static int _acquirement_weapon_subtype(bool divine)
{
    // Asking for a weapon is biased towards your skills.
    // First pick a skill, weighting towards those you have.
    int count = 0;
    int skill = SK_FIGHTING;
    int best_sk = 0;

    for (int i = SK_SHORT_BLADES; i <= SK_CROSSBOWS; i++)
    {
        if (is_invalid_skill(i))
            continue;

        // Adding a small constant allows for the occasional
        // weapon in an untrained skill.

        const int weight = you.skills[i] + 1;
        count += weight;

        if (you.skills[i] > best_sk)
            best_sk = you.skills[i];

        if (x_chance_in_y(weight, count))
            skill = i;
    }
    if (you.skills[SK_UNARMED_COMBAT] > best_sk)
        best_sk = you.skills[SK_UNARMED_COMBAT];

    // Now choose a subtype which uses that skill.
    int result = OBJ_RANDOM;
    count = 0;
    item_def item_considered;
    item_considered.base_type = OBJ_WEAPONS;
    // Let's guess the percentage of shield use the player did, this is
    // based on empirical data where pure-shield MDs get skills like 17 sh 25 m&f
    // and pure-shield Spriggans 7 sh 18 m&f.
    int shield_sk = you.skills[SK_SHIELDS] * species_skills(SK_SHIELDS, you.species) / 100;
    int want_shield = std::min(2 * shield_sk, best_sk) + 10;
    int dont_shield = std::max(best_sk - shield_sk, 0) + 10;
    // At XL 10, weapons of the handedness you want get weight *2, those of
    // opposite handedness 1/2, assuming your shields usage is respectively
    // 0% or 100% in the above formula.  At skill 25 that's *3.5 .
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        item_considered.sub_type = i;

        // Can't get blessed blades through acquirement, only from TSO
        if (is_blessed(item_considered))
            continue;

        int acqweight = property(item_considered, PWPN_ACQ_WEIGHT);

        if (!acqweight)
            continue;

        // HANDS_DOUBLE > HANDS_TWO
        const bool two_handed = hands_reqd(item_considered, you.body_size()) >= HANDS_TWO;

        // For non-Trog/Okawaru acquirements, give a boost to high-end items.
        if (!divine && !is_range_weapon(item_considered))
        {
            if (acqweight < 5)
                acqweight = 5;
            int damage = property(item_considered, PWPN_DAMAGE);
            if (!two_handed)
                damage = damage * 3 / 2;
            damage *= damage * damage;
            acqweight *= damage / property(item_considered, PWPN_SPEED);
        }

        if (two_handed)
            acqweight = acqweight * dont_shield / want_shield;
        else
            acqweight = acqweight * want_shield / dont_shield;

        if (!you.seen_weapon[i])
            acqweight *= 5; // strong emphasis on type variety, brands go only second

        int wskill = range_skill(OBJ_WEAPONS, i);
        if (wskill == SK_THROWING)
            wskill = weapon_skill(OBJ_WEAPONS, i);

        if (wskill == skill && x_chance_in_y(acqweight, count += acqweight))
            result = i;
    }
    return (result);
}

static bool _have_item_with_types(object_class_type basetype, int subtype)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def& item = you.inv[i];
        if (item.is_valid()
            && item.base_type == basetype && item.sub_type == subtype)
        {
            return (true);
        }
    }
    return (false);
}

static missile_type _acquirement_missile_subtype()
{
    int count = 0;
    int skill = SK_THROWING;

    for (int i = SK_SLINGS; i <= SK_THROWING; i++)
    {
        if (you.skills[i])
        {
            count += you.skills[i];
            if (x_chance_in_y(you.skills[i], count))
                skill = i;
        }
    }

    missile_type result = MI_DART;

    switch (skill)
    {
    case SK_SLINGS:    result = MI_SLING_BULLET; break;
    case SK_BOWS:      result = MI_ARROW; break;
    case SK_CROSSBOWS: result = MI_BOLT; break;

    case SK_THROWING:
        // Assuming that blowgun in inventory means that they
        // may want needles for it (but darts might also be
        // wanted).  Maybe expand this... see above comment.
        result =
            (_have_item_with_types(OBJ_WEAPONS, WPN_BLOWGUN) && coinflip())
            ? MI_NEEDLE : MI_DART;
        break;

    default:
        break;
    }
    return (result);
}

static int _acquirement_jewellery_subtype()
{
    int result = 0;

    // Try ten times to give something the player hasn't seen.
    for (int i = 0; i < 10; i++)
    {
        // 1/3 amulets, 2/3 rings.
        result = (one_chance_in(3) ? get_random_amulet_type()
                                   : get_random_ring_type());

        // If we haven't seen this yet, we're done.
        if (get_ident_type(OBJ_JEWELLERY, result) == ID_UNKNOWN_TYPE)
            break;
    }

    return (result);
}

static int _acquirement_staff_subtype(const has_vector& already_has)
{
    int result = random2(STAFF_FIRST_ROD);

    // Elemental preferences -- bwr
    if (result == STAFF_FIRE || result == STAFF_COLD)
    {
        if (you.skills[SK_FIRE_MAGIC] > you.skills[SK_ICE_MAGIC])
            result = STAFF_FIRE;
        if (you.skills[SK_FIRE_MAGIC] < you.skills[SK_ICE_MAGIC])
            result = STAFF_COLD;
    }
    else if (result == STAFF_AIR || result == STAFF_EARTH)
    {
        if (you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
            result = STAFF_AIR;
        if (you.skills[SK_AIR_MAGIC] < you.skills[SK_EARTH_MAGIC])
            result = STAFF_EARTH;
    }

    skill_type best_spell_skill = best_skill(SK_SPELLCASTING, NUM_SKILLS - 1);

#define TRY_GIVE(x) { if (!already_has[x]) result = x; }
    // If we're going to give out an enhancer staff,
    // we should at least bias things towards the
    // best spell skill. -- bwr
    switch (best_spell_skill)
    {
    case SK_FIRE_MAGIC:   TRY_GIVE(STAFF_FIRE);        break;
    case SK_ICE_MAGIC:    TRY_GIVE(STAFF_COLD);        break;
    case SK_AIR_MAGIC:    TRY_GIVE(STAFF_AIR);         break;
    case SK_EARTH_MAGIC:  TRY_GIVE(STAFF_EARTH);       break;
    case SK_POISON_MAGIC: TRY_GIVE(STAFF_POISON);      break;
    case SK_NECROMANCY:   TRY_GIVE(STAFF_DEATH);       break;
    case SK_CONJURATIONS: TRY_GIVE(STAFF_CONJURATION); break;
    case SK_ENCHANTMENTS: TRY_GIVE(STAFF_ENCHANTMENT); break;
    case SK_SUMMONINGS:   TRY_GIVE(STAFF_SUMMONING);   break;
#undef TRY_GIVE

    case SK_EVOCATIONS:
        if (!one_chance_in(4))
            result = random_rod_subtype();
        break;

    default: // Invocations and leftover spell schools.
        switch (random2(5))
        {
        case 0: result = STAFF_WIZARDRY;   break;
        case 1: result = STAFF_POWER;      break;
        case 2: result = STAFF_ENERGY;     break;
        case 3: result = STAFF_CHANNELING; break;
        case 4: break;          // keep the original random staff
        }
        break;
    }

    int spell_skills = 0;
    for (int i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
        spell_skills += you.skills[i];

    // Increased chance of getting a rod for new or
    // non-spellcasters.  -- bwr
    if (one_chance_in(20)
        || (spell_skills <= 1               // short on spells
            && result < STAFF_FIRST_ROD
            && !one_chance_in(4)))
    {
        result = coinflip() ? STAFF_STRIKING : random_rod_subtype();
    }

    return (result);
}

static int _acquirement_misc_subtype()
{
    int result = NUM_MISCELLANY;
    do
    {
        result = random2(NUM_MISCELLANY);
    }
    while (result == MISC_HORN_OF_GERYON
           || result == MISC_RUNE_OF_ZOT
           || result == MISC_CRYSTAL_BALL_OF_FIXATION
           || result == MISC_EMPTY_EBONY_CASKET
           || result == MISC_DECK_OF_PUNISHMENT);

    return (result);
}

static int _acquirement_wand_subtype()
{
    int picked = NUM_WANDS;

    int total = 0;
    for (int type = 0; type < NUM_WANDS; ++type)
    {
        int w = 0;

        // First, weight according to usefulness.
        switch (type)
        {
        case WAND_HASTING:          // each 17.9%, group unknown each 26.3%
        case WAND_HEALING:
            w = 25; break;
        case WAND_TELEPORTATION:    // each 10.7%, group unknown each 17.6%
        case WAND_INVISIBILITY:
            w = 15; break;
        case WAND_FIRE:             // each 5.7%, group unknown each 9.3%
        case WAND_COLD:
        case WAND_LIGHTNING:
        case WAND_DRAINING:
            w = 8; break;
        case WAND_DIGGING:          // each 3.6%, group unknown each 6.25%
        case WAND_FIREBALL:
        case WAND_DISINTEGRATION:
        case WAND_POLYMORPH_OTHER:
            w = 5; break;
        case WAND_FLAME:            // each 0.7%, group unknown each 1.4%
        case WAND_FROST:
        case WAND_CONFUSION:
        case WAND_PARALYSIS:
        case WAND_SLOWING:
        case WAND_ENSLAVEMENT:
        case WAND_MAGIC_DARTS:
        case WAND_RANDOM_EFFECTS:
        default:
            w = 1; break;
        }

        // Unknown wands get another huge weight bonus.
        if (get_ident_type(OBJ_WANDS, type) == ID_UNKNOWN_TYPE)
            w *= 2;

        total += w;
        if (x_chance_in_y(w, total))
            picked = type;
    }

    return (picked);
}

static int _find_acquirement_subtype(object_class_type class_wanted,
                                     int &quantity, bool divine,
                                     int agent = -1)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    int type_wanted = OBJ_RANDOM;

    // Write down what the player is carrying.
    has_vector already_has;
    already_has.init(0);
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        const item_def& item = you.inv[i];
        if (item.is_valid() && item.base_type == class_wanted)
        {
            ASSERT(item.sub_type < max_has_value);
            already_has[item.sub_type] += item.quantity;
        }
    }

    bool try_again = (class_wanted == OBJ_JEWELLERY
                      || class_wanted == OBJ_STAVES
                      || class_wanted == OBJ_MISCELLANY);
    int useless_count = 0;

    do
    {
        again:
        switch (class_wanted)
        {
        case OBJ_FOOD:
            // set type_wanted and quantity
            _acquirement_determine_food(type_wanted, quantity, already_has);
            break;

        case OBJ_WEAPONS:    type_wanted = _acquirement_weapon_subtype(divine);  break;
        case OBJ_MISSILES:   type_wanted = _acquirement_missile_subtype(); break;
        case OBJ_ARMOUR:     type_wanted = _acquirement_armour_subtype(divine); break;
        case OBJ_MISCELLANY: type_wanted = _acquirement_misc_subtype(); break;
        case OBJ_WANDS:      type_wanted = _acquirement_wand_subtype(); break;
        case OBJ_STAVES:     type_wanted = _acquirement_staff_subtype(already_has);
            break;
        case OBJ_JEWELLERY:  type_wanted = _acquirement_jewellery_subtype();
            break;
        default: break;         // gold, books
        }

        item_def dummy;
        dummy.base_type = class_wanted;
        dummy.sub_type = type_wanted;
        dummy.plus = 1; // empty wands would be useless
        dummy.flags |= ISFLAG_IDENT_MASK;

        if (is_useless_item(dummy, false) && useless_count++ < 200)
            goto again;

        if (try_again)
        {
            ASSERT(type_wanted < max_has_value);
            if (!already_has[type_wanted])
                try_again = false;
            if (one_chance_in(200))
                try_again = false;
        }
    }
    while (try_again);

    return (type_wanted);
}

// The weight of a spell takes into account its disciplines' skill levels
// and the spell difficulty.
static int _spell_weight(spell_type spell)
{
    ASSERT(spell != SPELL_NO_SPELL);

    int weight = 0;
    unsigned int disciplines = get_spell_disciplines(spell);
    int count = 0;
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        int disc = 1 << i;
        if (disciplines & disc)
        {
            int skill = you.skills[spell_type2skill(disc)];

            weight += skill;
            count++;
        }
    }
    ASSERT(count > 0);

    // Particularly difficult spells _reduce_ the overall weight.
    int leveldiff = 5 - spell_difficulty(spell);

    return std::max(0, 2 * weight/count + leveldiff);

}

// When randomly picking a book for acquirement, use the sum of the
// weights of all unknown spells in the book.
static int _book_weight(book_type book)
{
    ASSERT(book >= 0 && book <= MAX_FIXED_BOOK);

    int total_weight = 0;
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type stype = which_spell_in_book(book, i);
        if (stype == SPELL_NO_SPELL)
            continue;

        // Skip over spells already seen.
        if (you.seen_spell[stype])
            continue;

        total_weight += _spell_weight(stype);
    }

    return (total_weight);
}

static bool _is_magic_skill(int skill)
{
    return (skill >= SK_SPELLCASTING && skill < SK_INVOCATIONS);
}

static bool _skill_useless_with_god(int skill)
{
    switch (you.religion)
    {
    case GOD_TROG:
        return (_is_magic_skill(skill) || skill == SK_INVOCATIONS);
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
    case GOD_FEDHAS:
        return (skill == SK_NECROMANCY);
    case GOD_XOM:
    case GOD_NEMELEX_XOBEH:
        return (skill == SK_INVOCATIONS);
    default:
        return (false);
    }
}

static bool _do_book_acquirement(item_def &book, int agent)
{
    // items() shouldn't make book a randart for acquirement items.
    ASSERT(!is_random_artefact(book));

    int          level       = (you.skills[SK_SPELLCASTING] + 2) / 3;
    unsigned int seen_levels = you.attribute[ATTR_RND_LVL_BOOKS];

    level = std::max(1, level);

    if (agent == GOD_XOM)
        level = random_range(1, 9);
    else if (seen_levels & (1 << level))
    {
        // Give a book of a level not seen before, preferably one with
        // spells of a low enough level for the player to cast, or the
        // lowest aviable level if all levels which the player can cast
        // have already been given.
        int max_level = std::min(9, you.get_experience_level());

        std::vector<int> vec;
        for (int i = 1; i <= 9 && (vec.empty() || i <= max_level); i++)
            if (!(seen_levels & (1 << i)))
                vec.push_back(i);

        if (vec.size() > 0)
            level = vec[random2(vec.size())];
        else
            level = -1;
    }

    int choice = NUM_BOOKS;

    bool knows_magic = false;
    // Manuals are too useful for Xom, and useless when gifted from Sif Muna.
    if (agent != GOD_XOM && agent != GOD_SIF_MUNA)
    {
        int magic_weights = 0;
        int other_weights = 0;

        for (int i = 0; i < NUM_SKILLS; i++)
        {
            if (is_invalid_skill(i))
                continue;

            int weight = you.skills[i];

            // Anyone can get Spellcasting 1. Doesn't prove anything.
            if (i == SK_SPELLCASTING && weight >= 1)
                weight--;

            if (_is_magic_skill(i))
                magic_weights += weight;
            else
                other_weights += weight;
        }

        // If someone has 25% or more magic skills, never give manuals.
        // Otherwise, count magic skills double to bias against manuals
        // for magic users.
        if (magic_weights * 3 < other_weights
            && x_chance_in_y(other_weights, 2*magic_weights + other_weights))
        {
            choice = BOOK_MANUAL;
            if (magic_weights > 0)
                knows_magic = true;
        }
    }

    if (choice == NUM_BOOKS)
    {
        choice = random_choose_weighted(
                                        30, BOOK_RANDART_THEME,
           agent == GOD_SIF_MUNA ? 10 : 40, NUM_BOOKS, // normal books
                     level == -1 ?  0 :  1, BOOK_RANDART_LEVEL, 0);
    }

    // Acquired randart books have a chance of being named after the player.
    std::string owner = "";
    if (agent == AQ_SCROLL && one_chance_in(12)
        || agent == AQ_CARD_GENIE && one_chance_in(6))
    {
        owner = you.your_name;
    }

    switch (choice)
    {
    default:
    case NUM_BOOKS:
    {
        int total_weights = 0;

        // Pick a random spellbook according to unknown spells contained.
        int weights[MAX_FIXED_BOOK+1];
        for (int bk = 0; bk <= MAX_FIXED_BOOK; bk++)
        {
            if (bk > MAX_NORMAL_BOOK && agent == GOD_SIF_MUNA)
            {
                weights[bk] = 0;
                continue;
            }
            weights[bk]    = _book_weight(static_cast<book_type>(bk));
            total_weights += weights[bk];
        }

        if (total_weights > 0)
        {
            book.sub_type = choose_random_weighted(weights,
                                                   weights + ARRAYSZ(weights));
            break;
        }
        // else intentional fall-through
    }
    case BOOK_RANDART_THEME:
        book.sub_type = BOOK_RANDART_THEME;
        if (!make_book_theme_randart(book, 0, 0, 5 + coinflip(), 20,
                                     SPELL_NO_SPELL, owner))
        {
            return (false);
        }
        break;

    case BOOK_RANDART_LEVEL:
    {
        book.sub_type  = BOOK_RANDART_LEVEL;
        int max_spells = 5 + level/3;
        if (!make_book_level_randart(book, level, max_spells, owner))
            return (false);
        break;
    }

    case BOOK_MANUAL:
    {
        // The Tome of Destruction is rare enough we won't change this.
        if (book.sub_type == BOOK_DESTRUCTION)
            return (true);

        int weights[NUM_SKILLS];
        int total_weights = 0;

        for (int i = 0; i < NUM_SKILLS; i++)
        {
            if (is_invalid_skill(i))
            {
                weights[i] = 0;
                continue;
            }

            int skill = you.skills[i];

            if (skill == 27 || you.species == SP_DEMIGOD && i == SK_INVOCATIONS)
            {
                weights[i] = 0;
                continue;
            }

            int w = (skill < 12) ? skill + 3
                                 : std::max(0, 25 - skill);

            // Give a bonus for some highly sought after skills.
            if (i == SK_FIGHTING || i == SK_ARMOUR || i == SK_SPELLCASTING
                || i == SK_INVOCATIONS || i == SK_EVOCATIONS)
            {
                w += 5;
            }

            // Greatly reduce the chances of getting a manual for a skill
            // you couldn't use unless you switched your religion.
            if (_skill_useless_with_god(i))
                w /= 2;

            // If we don't have any magic skills, make non-magic skills
            // more likely.
            if (!knows_magic && !_is_magic_skill(i))
                w *= 2;

            weights[i] = w;
            total_weights += w;
        }

        // Are we too skilled to get any manuals?
        if (total_weights == 0)
            return _do_book_acquirement(book, agent);

        book.sub_type = BOOK_MANUAL;
        book.plus     = choose_random_weighted(weights, weights + NUM_SKILLS);
        // Set number of reads possible before it "crumbles to dust".
        book.plus2    = 3 + random2(15);
        break;
    } // manuals
    } // switch book choice
    return (true);
}

static int _failed_acquirement(bool quiet)
{
    if (!quiet)
        mpr("The demon of the infinite void smiles upon you.");
    return (NON_ITEM);
}

int _weapon_brand_quality(int brand, bool range)
{
    switch(brand)
    {
    case SPWPN_SPEED:
        return range ? 3 : 5;
    case SPWPN_PENETRATION:
        return 4;
    case SPWPN_ELECTROCUTION:
    case SPWPN_DISTORTION:
    case SPWPN_HOLY_WRATH:
    case SPWPN_REAPING:
        return 3;
    case SPWPN_CHAOS:
        return 2;
    default:
        return 1;
    case SPWPN_NORMAL:
        return 0;
    case SPWPN_PAIN:
        return you.skills[SK_NECROMANCY] / 2;
    case SPWPN_VORPAL:
        return range ? 5 : 1;
    }
}

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos, bool debug)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    const bool divine = (agent == GOD_OKAWARU || agent == GOD_XOM || agent == GOD_TROG);
    int thing_created = NON_ITEM;
    int quant = 1;
    for (int item_tries = 0; item_tries < 40; item_tries++)
    {
        int type_wanted = _find_acquirement_subtype(class_wanted, quant, divine, agent);

        // Clobber class_wanted for vampires.
        if (you.species == SP_VAMPIRE && class_wanted == OBJ_FOOD)
            class_wanted = OBJ_POTIONS;

        // Don't generate randart books in items(), we do that
        // ourselves.
        int want_arts = (class_wanted == OBJ_BOOKS ? 0 : 1);

        thing_created = items( want_arts, class_wanted, type_wanted, true,
                               MAKE_GOOD_ITEM, MAKE_ITEM_RANDOM_RACE,
                               0, 0, agent );

        if (thing_created == NON_ITEM)
            continue;

        item_def &doodad(mitm[thing_created]);

        // Not a god, prefer better brands.
        if (!divine && !is_artefact(doodad) && doodad.base_type == OBJ_WEAPONS)
        {
            while(_weapon_brand_quality(get_weapon_brand(doodad),
                                        is_range_weapon(doodad)) < random2(6))
            {
                reroll_brand(doodad, MAKE_GOOD_ITEM);
            }
        }

        // Try to not generate brands that were already seen, although unlike
        // jewelry and books, this is not absolute.
        while (!is_artefact(doodad)
               && (doodad.base_type == OBJ_WEAPONS
                     && you.seen_weapon[doodad.sub_type] & (1<<get_weapon_brand(doodad))
                   || doodad.base_type == OBJ_ARMOUR
                     && you.seen_armour[doodad.sub_type] & (1<<get_weapon_brand(doodad)))
               && !one_chance_in(5))
        {
            reroll_brand(doodad, MAKE_GOOD_ITEM);
        }

        // For plain armour, try to change the subtype to something
        // matching a currently unfilled equipment slot.
        if (doodad.base_type == OBJ_ARMOUR && !is_artefact(doodad))
        {
            const equipment_type eq = get_armour_slot(doodad);

            // Don't try to replace an item if it would already fill a
            // currently unfilled secondary armour slot.
            if (eq == EQ_BODY_ARMOUR || you.equip[eq] != -1)
            {
                const special_armour_type sparm = get_armour_ego_type(doodad);
                bool is_plain     = (sparm == SPARM_NORMAL);
                bool is_redundant = false;

                // If the created item is an ego item, check whether we're
                // already wearing an item with this ego in the same slot, and
                // whether the enchantment is worse than that of the current
                // item. (For armour, only consider items of the same subtype.)
                // If so, try filling an unfilled equipment slot after all.
                if (!is_plain && agent != GOD_XOM)
                {
                    if (you.equip[eq] != -1
                        && (eq != EQ_BODY_ARMOUR
                            || doodad.sub_type == you.inv[you.equip[eq]].sub_type)
                        && sparm == get_armour_ego_type(you.inv[you.equip[eq]])
                        && doodad.plus <= you.inv[you.equip[eq]].plus)
                    {
                        // Ego type is cleared later.
                        is_redundant = true;
                    }
                }

                if (is_plain || is_redundant)
                {
                    if (_try_give_plain_armour(doodad))
                    {
                        // Make sure the item is plain.
                        doodad.special = SPARM_NORMAL;

                        // Okawaru shouldn't hand out negatively enchanted
                        // plain items.
                        if (agent == GOD_OKAWARU && doodad.plus < 0)
                            doodad.plus = 0;
                        else if (agent == GOD_XOM && doodad.plus > 0)
                            doodad.plus *= -1;
                    }
                    else if (is_plain && agent != GOD_XOM && one_chance_in(3))
                    {
                        // If the item is plain and there aren't any
                        // unfilled slots, we might want to roll again.
                        destroy_item(thing_created, true);
                        thing_created = NON_ITEM;
                        continue;
                    }
                }
            }
        }

        if (doodad.base_type == OBJ_WEAPONS
               && !can_wield(&doodad, false, true)
            || doodad.base_type == OBJ_ARMOUR
               && !can_wear_armour(doodad, false, true))
        {
            destroy_item(thing_created, true);
            thing_created = NON_ITEM;
            continue;
        }

        // Only TSO gifts blessed weapons, and currently not through
        // acquirement, but make sure of this anyway.
        if (agent != GOD_SHINING_ONE && is_blessed(doodad))
        {
            destroy_item(thing_created, true);
            thing_created = NON_ITEM;
            continue;
        }

        // Trog does not gift the Wrath of Trog, nor weapons of pain
        // (which work together with Necromantic magic).
        if (agent == GOD_TROG)
        {
            int brand = get_weapon_brand(doodad);
            if (brand == SPWPN_PAIN
                || is_unrandom_artefact(doodad)
                   && (doodad.special == UNRAND_TROG
                       || doodad.special == UNRAND_WUCAD_MU))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }
        }

        // MT - Check: god-gifted weapons and armour shouldn't kill you.
        // Except Xom.
        if ((agent == GOD_TROG || agent == GOD_OKAWARU)
            && is_artefact(doodad))
        {
            artefact_properties_t  proprt;
            artefact_wpn_properties( doodad, proprt );

            // Check vs. stats. positive stats will automatically fall
            // through.  As will negative stats that won't kill you.
            if (-proprt[ARTP_STRENGTH] >= you.strength()
                || -proprt[ARTP_INTELLIGENCE] >= you.intel()
                || -proprt[ARTP_DEXTERITY] >= you.dex())
            {
                // Try again.
                destroy_item(thing_created);
                thing_created = NON_ITEM;
                continue;
            }
        }

        // Sif Muna shouldn't gift Vehumet or Kiku's special books.
        // (The spells therein are still fair game for randart books.)
        if (agent == GOD_SIF_MUNA
            && doodad.sub_type >= MIN_GOD_ONLY_BOOK
            && doodad.sub_type <= MAX_GOD_ONLY_BOOK)
        {
            ASSERT(doodad.base_type == OBJ_BOOKS);

            // Try again.
            destroy_item(thing_created);
            thing_created = NON_ITEM;
            continue;
        }
        break;
    }

    if (thing_created == NON_ITEM)
        return _failed_acquirement(quiet);

    // Easier to read this way.
    item_def& thing(mitm[thing_created]);

    if (class_wanted == OBJ_WANDS)
        thing.plus = std::max((int) thing.plus, 3 + random2(3));
    else if (quant > 1)
        thing.quantity = quant;

    if (is_blood_potion(thing))
        init_stack_blood_potions(thing);

    // Remove curse flag from item.
    do_uncurse_item(thing);

    if (thing.base_type == OBJ_BOOKS)
    {
        if (!_do_book_acquirement(thing, agent))
        {
            destroy_item(thing, true);
            return _failed_acquirement(quiet);
        }
        // Don't mark books as seen if only generated for the
        // acquirement statistics.
        if (!debug)
            mark_had_book(thing);
    }
    else if (thing.base_type == OBJ_JEWELLERY)
    {
        switch (thing.sub_type)
        {
        case RING_SLAYING:
            // Make sure plus to damage is >= 1.
            thing.plus2 = std::max(abs(thing.plus2), 1);
            // fall through...

        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_INTELLIGENCE:
        case RING_DEXTERITY:
        case RING_EVASION:
            // Make sure plus is >= 1.
            thing.plus = std::max(abs(thing.plus), 1);
            break;

        case RING_HUNGER:
        case AMU_INACCURACY:
            // These are the only truly bad pieces of jewellery.
            if (!one_chance_in(9))
                make_item_randart(thing);
            break;

        default:
            break;
        }
    }
    else if (thing.base_type == OBJ_WEAPONS
             && !is_unrandom_artefact(thing))
    {
        // HACK: Make unwieldable weapons wieldable.
        // Note: messing with fixed artefacts is probably very bad.
        switch (you.species)
        {
        case SP_DEMONSPAWN:
        case SP_MUMMY:
        case SP_GHOUL:
        case SP_VAMPIRE:
        {
            int brand = get_weapon_brand(thing);
            if (brand == SPWPN_HOLY_WRATH)
            {
                if (is_random_artefact(thing))
                {
                    // Keep resetting seed until it's good.
                    for (; brand == SPWPN_HOLY_WRATH;
                         brand = get_weapon_brand(thing))
                    {
                        make_item_randart(thing);
                    }
                }
                else
                {
                    set_item_ego_type(thing, OBJ_WEAPONS, SPWPN_VORPAL);
                }
            }
            break;
        }

        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
            switch (thing.sub_type)
            {
            case WPN_LONGBOW:
                thing.sub_type = WPN_BOW;
                break;

            case WPN_GREAT_SWORD:
            case WPN_TRIPLE_SWORD:
                thing.sub_type = (coinflip() ? WPN_FALCHION : WPN_LONG_SWORD);
                break;

            case WPN_GREAT_MACE:
            case WPN_DIRE_FLAIL:
                thing.sub_type = (coinflip() ? WPN_MACE : WPN_FLAIL);
                break;

            case WPN_BATTLEAXE:
            case WPN_EXECUTIONERS_AXE:
                thing.sub_type = (coinflip() ? WPN_HAND_AXE : WPN_WAR_AXE);
                break;

            case WPN_HALBERD:
            case WPN_GLAIVE:
            case WPN_SCYTHE:
            case WPN_BARDICHE:
                thing.sub_type = (coinflip() ? WPN_SPEAR : WPN_TRIDENT);
                break;
            }
            break;

        default:
            break;
        }

        // These can never get egos, and mundane versions are quite common, so
        // guarantee artifact status.  Rarity is a bit low to compensate.
        if (thing.sub_type == WPN_GIANT_CLUB
            || thing.sub_type == WPN_GIANT_SPIKED_CLUB)
        {
            if (!one_chance_in(25))
                make_item_randart(thing, true);
        }

        int plusmod = random2(4);
        if (agent == GOD_TROG)
        {
            // More damage, less accuracy.
            thing.plus  -= plusmod;
            thing.plus2 += plusmod;
            if (!is_artefact(thing))
                thing.plus = std::max(static_cast<int>(thing.plus), 0);
        }
        else if (agent == GOD_OKAWARU)
        {
            // More accuracy, less damage.
            thing.plus  += plusmod;
            thing.plus2 -= plusmod;
            if (!is_artefact(thing))
                thing.plus2 = std::max(static_cast<int>(thing.plus2), 0);
        }
    }

    if (agent > GOD_NO_GOD && agent < NUM_GODS && agent == you.religion)
        thing.inscription = "god gift";

    // Moving this above the move since it might not exist after falling.
    if (thing_created != NON_ITEM && !quiet)
        canned_msg(MSG_SOMETHING_APPEARS);

    // If a god wants to give you something but the floor doesn't want it,
    // it counts as a failed acquirement - no piety, etc cost.
    if (feat_destroys_item(grd(pos), thing) && (agent > GOD_NO_GOD) &&
        (agent < NUM_GODS))
    {
        if (agent == GOD_XOM)
            simple_god_message(" snickers.", GOD_XOM);
        else
            return _failed_acquirement(quiet);
    }

    move_item_to_grid( &thing_created, pos );

    return (thing_created);
}

bool acquirement(object_class_type class_wanted, int agent,
                 bool quiet, int* item_index, bool debug)
{
    ASSERT(!crawl_state.game_is_arena());

    int thing_created = NON_ITEM;

    if (item_index == NULL)
        item_index = &thing_created;

    *item_index = NON_ITEM;

    while (class_wanted == OBJ_RANDOM)
    {
        ASSERT(!quiet);
        mesclr();
        mpr ("[a] Weapon  [b] Armour  [c] Jewellery      [d] Book");
        mprf("[e] Staff   [f] Wand    [g] Miscellaneous  [h] %s [i] Gold",
            (you.religion == GOD_FEDHAS ? "Fruit" : "Food "));
        mpr("What kind of item would you like to acquire? ", MSGCH_PROMPT);

        const int keyin = tolower( get_ch() );
        switch (keyin)
        {
        case 'a': case ')':            class_wanted = OBJ_WEAPONS;    break;
        case 'b': case '[':  case ']': class_wanted = OBJ_ARMOUR;     break;
        case 'c': case '=':  case '"': class_wanted = OBJ_JEWELLERY;  break;
        case 'd': case '+':  case ':': class_wanted = OBJ_BOOKS;      break;
        case 'e': case '\\': case '|': class_wanted = OBJ_STAVES;     break;
        case 'f': case '/':            class_wanted = OBJ_WANDS;      break;
        case 'g': case '}':  case '{': class_wanted = OBJ_MISCELLANY; break;
        case 'h': case '%':            class_wanted = OBJ_FOOD;       break;
        case 'i': case '$':            class_wanted = OBJ_GOLD;       break;
        default:
            // Lets wizards escape out of accidently choosing acquirement.
            if (agent == AQ_WIZMODE)
            {
                canned_msg(MSG_OK);
                return (false);
            }

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
            // If we've gotten a HUP signal then the player will be unable
            // to make a selection.
            if (crawl_state.seen_hups)
            {
                mpr("Acquirement interrupted by HUP signal.", MSGCH_ERROR);
                you.turn_is_over = false;
                return (false);
            }
#endif
           break;
        }
    }

    *item_index = acquirement_create_item(class_wanted, agent, quiet,
                                          you.pos(), debug);

    return (true);
}

bool recharge_wand(int item_slot)
{
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item( "Charge which item?", MT_INVLIST,
                                            OSEL_RECHARGE, true, true, false );
        }
        if (prompt_failed(item_slot))
            return (false);

        item_def &wand = you.inv[ item_slot ];

        if (!item_is_rechargeable(wand, true, true))
        {
            mpr("Choose an item to recharge, or Esc to abort.");
            if (Options.auto_list)
                more();

            // Try again.
            item_slot = -1;
            continue;
        }

        // Weapons of electrocution can be "charged", i.e. gain +1 damage.
        if (wand.base_type == OBJ_WEAPONS)
        {
            if (get_weapon_brand(wand) == SPWPN_ELECTROCUTION)
            {
                // Might fail because of already high enchantment.
                if (enchant_weapon( ENCHANT_TO_DAM, false, wand ))
                {
                    you.wield_change = true;

                    if (!item_ident(wand, ISFLAG_KNOW_TYPE))
                        set_ident_flags(wand, ISFLAG_KNOW_TYPE);

                    return (true);
                }
                return (false);
            }
            else
                canned_msg( MSG_NOTHING_HAPPENS );
        }

        if (wand.base_type != OBJ_WANDS && !item_is_rod(wand))
            return (false);

        int charge_gain = 0;
        if (wand.base_type == OBJ_WANDS)
        {
            charge_gain = wand_charge_value(wand.sub_type);

            const int new_charges =
                std::max<int>(
                    wand.plus,
                    std::min(charge_gain * 3,
                             wand.plus +
                             1 + random2avg(((charge_gain - 1) * 3) + 1, 3)));

            const bool charged = (new_charges > wand.plus);

            std::string desc;

            if (charged && item_ident(wand, ISFLAG_KNOW_PLUSES))
            {
                snprintf(info, INFO_SIZE, " and now has %d charge%s",
                         new_charges, new_charges == 1 ? "" : "s");
                desc = info;
            }

            mprf("%s %s for a moment%s.",
                 wand.name(DESC_CAP_YOUR).c_str(),
                 charged ? "glows" : "flickers",
                 desc.c_str());

            // Reinitialise zap counts.
            wand.plus  = new_charges;
            wand.plus2 = (charged ? ZAPCOUNT_RECHARGED : ZAPCOUNT_MAX_CHARGED);
        }
        else // It's a rod.
        {
            bool work = false;

            if (wand.plus2 < MAX_ROD_CHARGE * ROD_CHARGE_MULT)
            {
                wand.plus2 += ROD_CHARGE_MULT * random_range(1,2);

                if (wand.plus2 > MAX_ROD_CHARGE * ROD_CHARGE_MULT)
                    wand.plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;

                work = true;
            }

            if (wand.plus < wand.plus2)
            {
                wand.plus = wand.plus2;
                work = true;
            }

            if (short(wand.props["rod_enchantment"]) < MAX_WPN_ENCHANT)
            {
                static_cast<short&>(wand.props["rod_enchantment"])
                    += random_range(1,2);

                if (short(wand.props["rod_enchantment"]) > MAX_WPN_ENCHANT)
                    wand.props["rod_enchantment"] = short(MAX_WPN_ENCHANT);

                work = true;
            }

            if (!work)
                return (false);

            mprf("%s glows for a moment.", wand.name(DESC_CAP_YOUR).c_str());
        }

        you.wield_change = true;
        return (true);
    }
    while (true);

    return (false);
}

// Berserking monsters cannot be ordered around.
static bool _follows_orders(monsters* mon)
{
    return (mon->friendly() && mon->type != MONS_GIANT_SPORE
        && !mon->berserk());
}

// Sets foe target of friendly monsters.
// If allow_patrol is true, patrolling monsters get MHITNOT instead.
static void _set_friendly_foes(bool allow_patrol = false)
{
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        mi->foe = (allow_patrol && mi->is_patrolling() ? MHITNOT
                                                         : you.pet_target);
    }
}

static void _set_allies_patrol_point(bool clear = false)
{
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (!_follows_orders(*mi))
            continue;
        mi->patrol_point = (clear ? coord_def(0, 0) : mi->pos());
        if (!clear)
            mi->behaviour = BEH_WANDER;
    }
}

void yell(bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    bool targ_prev = false;
    int mons_targd = MHITNOT;
    dist targ;

    const std::string shout_verb = you.shout_verb();
    std::string cap_shout = shout_verb;
    cap_shout[0] = toupper(cap_shout[0]);

    int noise_level = 12; // "shout"

    // Tweak volume for different kinds of vocalisation.
    if (shout_verb == "roar")
        noise_level = 18;
    else if (shout_verb == "hiss")
        noise_level = 8;
    else if (shout_verb == "squeak")
        noise_level = 4;
    else if (shout_verb == "__NONE")
        noise_level = 0;
    else if (shout_verb == "yell")
        noise_level = 14;
    else if (shout_verb == "scream")
        noise_level = 16;

    if (silenced(you.pos()) || you.cannot_speak())
        noise_level = 0;

    if (noise_level == 0)
    {
        if (force)
        {
            if (shout_verb == "__NONE" || you.paralysed())
            {
                mprf("You feel a strong urge to %s, but "
                     "you are unable to make a sound!",
                     shout_verb == "__NONE" ? "scream"
                                            : shout_verb.c_str());
            }
            else
            {
                mprf("You feel a %s rip itself from your throat, "
                     "but you make no sound!",
                     shout_verb.c_str());
            }
        }
        else
            mpr("You are unable to make a sound!");

        return;
    }

    if (force)
    {
        mprf("A %s rips itself from your throat!", shout_verb.c_str());
        noisy(noise_level, you.pos());
        return;
    }

    mpr("What do you say?", MSGCH_PROMPT);
    mprf(" t - %s!", cap_shout.c_str());

    if (!you.berserk())
    {
        std::string previous;
        if (!(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU))
        {
            const monsters *target = &menv[you.prev_targ];
            if (target->alive() && you.can_see(target))
            {
                previous = "   p - Attack previous target.";
                targ_prev = true;
            }
        }

        mprf("Orders for allies: a - Attack new target.%s", previous.c_str());
        mpr( "                   s - Stop attacking.");
        mpr( "                   w - Wait here.           f - Follow me.");
    }
    mprf(" Anything else - Stay silent%s.",
         one_chance_in(20) ? " (and be thought a fool)" : "");

    unsigned char keyn = get_ch();
    mesclr();

    switch (keyn)
    {
    case '!':    // for players using the old keyset
    case 't':
        mprf(MSGCH_SOUND, "You %s for attention!", shout_verb.c_str());
        noisy(noise_level, you.pos());
        you.turn_is_over = true;
        return;

    case 'f':
    case 's':
        mons_targd = MHITYOU;
        if (keyn == 'f')
        {
            // Don't reset patrol points for 'Stop fighting!'
            _set_allies_patrol_point(true);
            mpr("Follow me!");
        }
        else
            mpr("Stop fighting!");
        break;

    case 'w':
        mpr("Wait here!");
        mons_targd = MHITNOT;
        _set_allies_patrol_point();
        break;

    case 'p':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        if (targ_prev)
        {
            mons_targd = you.prev_targ;
            break;
        }

    // fall through
    case 'a':
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        if (env.sanctuary_time > 0)
        {
            if (!yesno("An ally attacking under your orders might violate "
                       "sanctuary; order anyway?", false, 'n'))
            {
                canned_msg(MSG_OK);
                return;
            }
        }

        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_HOSTILE;
            args.needs_path = false;
            args.top_prompt = "Gang up on whom?";
            direction(targ, args);
        }

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        {
            bool cancel = !targ.isValid;
            if (!cancel)
            {
                const monsters* m = monster_at(targ.target);
                cancel = (m == NULL || !you.can_see(m));
                if (!cancel)
                    mons_targd = m->mindex();
            }

            if (cancel)
            {
                mpr("Yeah, whatever.");
                return;
            }
        }
        break;

    default:
        mpr("Okely-dokely.");
        return;
    }

    you.turn_is_over = true;
    you.pet_target = mons_targd;
    // Allow patrolling for "Stop fighting!" and "Wait here!"
    _set_friendly_foes(keyn == 's' || keyn == 'w');

    if (mons_targd != MHITNOT && mons_targd != MHITYOU)
        mpr("Attack!");

    noisy(10, you.pos());
}

bool forget_inventory(bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    int items_forgotten = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item(you.inv[i]);
        if (!item.is_valid() || item_is_equipped(item))
            continue;

        unsigned long orig_flags = item.flags;

        unset_ident_flags(item, ISFLAG_KNOW_CURSE);

        // Don't forget times used or uses left for wands or decks.
        if (item.base_type != OBJ_WANDS && item.base_type != OBJ_MISCELLANY)
            unset_ident_flags(item, ISFLAG_KNOW_PLUSES);

        if (!is_artefact(item))
        {
            switch (item.base_type)
            {
            case OBJ_WEAPONS:
            case OBJ_ARMOUR:
            case OBJ_BOOKS:
            case OBJ_STAVES:
            case OBJ_MISCELLANY:
                // Don't forget identity of decks if the player has
                // used any of its cards, or knows how many are left.
                if (!is_deck(item) || item.plus2 == 0)
                    unset_ident_flags(item, ISFLAG_KNOW_TYPE);
                break;

            default:
                break;
            }
        }
        // Non-jewellery artefacts can easily be re-identified by
        // equipping them.
        else if (item.base_type != OBJ_JEWELLERY)
            unset_ident_flags(item, ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);

        if (item.flags != orig_flags)
            items_forgotten++;
    }

    if (items_forgotten > 0)
        mpr("Wait, did you forget something?");

    return (items_forgotten > 0);
}

// Returns true if there was a visible change.
bool vitrify_area(int radius)
{
    if (radius < 2)
        return (false);

    // This hinges on clear wall types having the same order as non-clear ones!
    const int clear_plus = DNGN_CLEAR_ROCK_WALL - DNGN_ROCK_WALL;
    bool something_happened = false;
    for (radius_iterator ri(you.pos(), radius, C_POINTY); ri; ++ri)
    {
        const dungeon_feature_type grid = grd(*ri);

        if (grid == DNGN_ROCK_WALL
            || grid == DNGN_STONE_WALL
            || grid == DNGN_PERMAROCK_WALL)
        {
            grd(*ri) = static_cast<dungeon_feature_type>(grid + clear_plus);
            set_terrain_changed(ri->x, ri->y);
            something_happened = true;
        }
    }
    return (something_happened);
}

static void _hell_effects()
{
    if (is_sanctuary(you.pos()))
    {
        mpr("Zin's power protects you from Hell's scourges!", MSGCH_GOD);
        return;
    }

    int temp_rand = random2(17);
    spschool_flag_type which_miscast = SPTYP_RANDOM;
    bool summon_instead = false;
    monster_type which_beastie = MONS_NO_MONSTER;

    mpr((temp_rand ==  0) ? "\"You will not leave this place.\"" :
        (temp_rand ==  1) ? "\"Die, mortal!\"" :
        (temp_rand ==  2) ? "\"We do not forgive those who trespass against us!\"" :
        (temp_rand ==  3) ? "\"Trespassers are not welcome here!\"" :
        (temp_rand ==  4) ? "\"You do not belong in this place!\"" :
        (temp_rand ==  5) ? "\"Leave now, before it is too late!\"" :
        (temp_rand ==  6) ? "\"We have you now!\"" :
        // plain messages
        (temp_rand ==  7) ? (you.can_smell()) ? "You smell brimstone."
                                                 : "Brimstone rains from above." :
        (temp_rand ==  8) ? "You feel lost and a long, long way from home..." :
        (temp_rand ==  9) ? "You shiver with fear." :
        // warning
        (temp_rand == 10) ? "You feel a terrible foreboding..." :
        (temp_rand == 11) ? "Something frightening happens." :
        (temp_rand == 12) ? "You sense an ancient evil watching you..." :
        (temp_rand == 13) ? "You suddenly feel all small and vulnerable." :
        (temp_rand == 14) ? "You sense a hostile presence." :
        // sounds
        (temp_rand == 15) ? "A gut-wrenching scream fills the air!" :
        (temp_rand == 16) ? "You hear words spoken in a strange and terrible language..."
                          : "You hear diabolical laughter!",
        (temp_rand <  7 ? MSGCH_TALK :
         temp_rand < 10 ? MSGCH_PLAIN :
         temp_rand < 15 ? MSGCH_WARN
                        : MSGCH_SOUND));

    if (temp_rand >= 15)
        noisy(15, you.pos());

    temp_rand = random2(27);

    if (temp_rand > 17)     // 9 in 27 odds {dlb}
    {
        temp_rand = random2(8);

        if (temp_rand > 3)  // 4 in 8 odds {dlb}
            which_miscast = SPTYP_NECROMANCY;
        else if (temp_rand > 1)     // 2 in 8 odds {dlb}
            which_miscast = SPTYP_SUMMONING;
        else if (temp_rand > 0)     // 1 in 8 odds {dlb}
            which_miscast = SPTYP_CONJURATION;
        else                // 1 in 8 odds {dlb}
            which_miscast = SPTYP_ENCHANTMENT;

        MiscastEffect(&you, MISC_KNOWN_MISCAST, which_miscast,
                      4 + random2(6), random2avg(97, 3),
                      "the effects of Hell");
    }
    else if (temp_rand > 7) // 10 in 27 odds {dlb}
    {
        // 60:40 miscast:summon split {dlb}
        summon_instead = x_chance_in_y(2, 5);

        switch (you.where_are_you)
        {
        case BRANCH_DIS:
            if (summon_instead)
                which_beastie = summon_any_demon(DEMON_GREATER);
            else
                which_miscast = SPTYP_EARTH;
            break;

        case BRANCH_GEHENNA:
            if (summon_instead)
                which_beastie = MONS_FIEND;
            else
                which_miscast = SPTYP_FIRE;
            break;

        case BRANCH_COCYTUS:
            if (summon_instead)
                which_beastie = MONS_ICE_FIEND;
            else
                which_miscast = SPTYP_ICE;
            break;

        case BRANCH_TARTARUS:
            if (summon_instead)
                which_beastie = MONS_SHADOW_FIEND;
            else
                which_miscast = SPTYP_NECROMANCY;
            break;

        default:
            // This is to silence gcc compiler warnings. {dlb}
            if (summon_instead)
                which_beastie = MONS_FIEND;
            else
                which_miscast = SPTYP_NECROMANCY;
            break;
        }

        if (summon_instead)
        {
            create_monster(
                mgen_data::hostile_at(which_beastie, "the effects of Hell",
                    true, 0, 0, you.pos()));
        }
        else
        {
            MiscastEffect(&you, MISC_KNOWN_MISCAST, which_miscast,
                          4 + random2(6), random2avg(97, 3),
                          "the effects of Hell");
        }
    }

    // NB: No "else" - 8 in 27 odds that nothing happens through
    //                 first chain. {dlb}
    // Also note that the following is distinct from and in
    // addition to the above chain.

    // Try to summon at least one and up to five random monsters. {dlb}
    if (one_chance_in(3))
    {
        mgen_data mg;
        mg.pos = you.pos();
        mg.foe = MHITYOU;
        mg.non_actor_summoner = "the effects of Hell";
        create_monster(mg);

        for (int i = 0; i < 4; ++i)
            if (one_chance_in(3))
                create_monster(mg);
    }
}

// This function checks whether we can turn a wall into a floor space and
// still keep a corridor-like environment. The wall in position x is a
// a candidate for switching if it's flanked by floor grids to two sides
// and by walls (any type) to the remaining cardinal directions.
//
//   .        #          2
//  #x#  or  .x.   ->   0x1
//   .        #          3
static bool _feat_is_flanked_by_walls(const coord_def &p)
{
    const coord_def adjs[] = { coord_def(p.x-1,p.y),
                               coord_def(p.x+1,p.y),
                               coord_def(p.x  ,p.y-1),
                               coord_def(p.x  ,p.y+1) };

    // paranoia!
    for (unsigned int i = 0; i < ARRAYSZ(adjs); ++i)
        if (!in_bounds(adjs[i]))
            return (false);

    return (feat_is_wall(grd(adjs[0])) && feat_is_wall(grd(adjs[1]))
               && feat_has_solid_floor(grd(adjs[2])) && feat_has_solid_floor(grd(adjs[3]))
            || feat_has_solid_floor(grd(adjs[0])) && feat_has_solid_floor(grd(adjs[1]))
               && feat_is_wall(grd(adjs[2])) && feat_is_wall(grd(adjs[3])));
}

// Sometimes if a floor is turned into a wall, a dead-end will be created.
// If this is the case, we need to make sure that it is at least two grids
// deep.
//
// Example: If a wall is built at X (A), two dead-ends are created, a short
//          and a long one. The latter is perfectly fine, but the former
//          looks a bit odd. If Y is chosen, this looks much better (B).
//
// #######    (A)  #######    (B)  #######
// ...XY..         ...#...         ....#..
// #.#####         #.#####         #.#####
//
// What this function does is check whether the neighbouring floor grids
// are flanked by walls on both sides, and if so, the grids following that
// also have to be floor flanked by walls.
//
//   czd
//   a.b   -> if (a, b == walls) then (c, d == walls) or return (false)
//   #X#
//    .
//
// Grid z may be floor or wall, either way we have a corridor of at least
// length 2.
static bool _deadend_check_wall(const coord_def &p)
{
    // The grids to the left and right of p are walls. (We already know that
    // they are symmetric, so only need to check one side. We also know that
    // the other direction, here up/down must then be non-walls.)
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        // Run the check twice, once in either direction.
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x-1, p.y+i);
            const coord_def b(p.x+1, p.y+i);
            const coord_def c(p.x-1, p.y+2*i);
            const coord_def d(p.x+1, p.y+2*i);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return (false);
            }
        }
    }
    else // The grids above and below p are walls.
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+i  , p.y-1);
            const coord_def b(p.x+i  , p.y+1);
            const coord_def c(p.x+2*i, p.y-1);
            const coord_def d(p.x+2*i, p.y+1);

            if (in_bounds(a) && in_bounds(b)
                && feat_is_wall(grd(a)) && feat_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !feat_is_wall(grd(c)) || !feat_is_wall(grd(d))))
            {
                return (false);
            }
        }
    }

    return (true);
}

// Similar to the above, checks whether turning a wall grid into floor
// would create a short "dead-end" of only 1 grid.
//
// In the example below, X would create miniature dead-ends at positions
// a and b, but both Y and Z avoid this, and the resulting mini-mazes
// look much better.
//
// ########   (A)  ########     (B)  ########     (C)  ########
// #.....#.        #....a#.          #.....#.          #.....#.
// #.#YXZ#.        #.##.##.          #.#.###.          #.###.#.
// #.#.....        #.#b....          #.#.....          #.#.....
//
// In general, if a floor grid horizontally or vertically adjacent to the
// change target has a floor neighbour diagonally adjacent to the change
// target, the next neighbour in the same direction needs to be floor,
// as well.
static bool _deadend_check_floor(const coord_def &p)
{
    if (feat_is_wall(grd[p.x-1][p.y]))
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x, p.y+2*i);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+2*j, p.y+i);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+j, p.y+i);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return (false);
            }
        }
    }
    else
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x+2*i, p.y);
            if (!in_bounds(a) || feat_has_solid_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+i, p.y+2*j);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+i, p.y+j);
                if (feat_has_solid_floor(grd(c)) && !feat_has_solid_floor(grd(b)))
                    return (false);
            }
        }
    }

    return (true);
}

// Changes a small portion of a labyrinth by exchanging wall against floor
// grids in such a way that connectivity remains guaranteed.
void change_labyrinth(bool msg)
{
    int size = random_range(12, 24); // size of the shifted area (square)
    coord_def c1, c2; // upper left, lower right corners of the shifted area

    std::vector<coord_def> targets;

    // Try 10 times for an area that is little mapped.
    for (int tries = 10; tries > 0; --tries)
    {
        targets.clear();

        int x = random_range(LABYRINTH_BORDER, GXM - LABYRINTH_BORDER - size);
        int y = random_range(LABYRINTH_BORDER, GYM - LABYRINTH_BORDER - size);
        c1 = coord_def(x, y);
        c2 = coord_def(x + size, y + size);

        int count_known = 0;
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
            if (is_terrain_seen(*ri))
                count_known++;

        if (tries > 1 && count_known > size * size / 6)
            continue;

        // Fill a vector with wall grids that are potential targets for
        // swapping against floor, i.e. are flanked by walls to two cardinal
        // directions, and by floor on the two remaining sides.
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
        {
            if (is_terrain_seen(*ri) || !feat_is_wall(grd(*ri)))
                continue;

            // Skip on grids inside vaults so as not to disrupt them.
            if (testbits(env.pgrid(*ri), FPROP_VAULT))
                continue;

            // Make sure we don't accidentally create "ugly" dead-ends.
            if (_feat_is_flanked_by_walls(*ri) && _deadend_check_floor(*ri))
                targets.push_back(*ri);
        }

        if (targets.size() >= 8)
            break;
    }

    if (targets.empty())
    {
        if (msg)
            mpr("No unexplored wall grids found!");
        return;
    }

    if (msg)
    {
        mprf(MSGCH_DIAGNOSTICS, "Changing labyrinth from (%d, %d) to (%d, %d)",
             c1.x, c1.y, c2.x, c2.y);
    }

    if (msg)
    {
        std::string path_str = "";
        mpr("Here's the list of targets: ", MSGCH_DIAGNOSTICS);
        for (unsigned int i = 0; i < targets.size(); i++)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", targets[i].x, targets[i].y);
            path_str += info;
        }
        mpr(path_str.c_str(), MSGCH_DIAGNOSTICS);
        mprf(MSGCH_DIAGNOSTICS, "-> #targets = %d", targets.size());
    }

#ifdef WIZARD
    // Remove old highlighted areas to make place for the new ones.
    for (rectangle_iterator ri(1); ri; ++ri)
        env.pgrid(*ri) &= ~(FPROP_HIGHLIGHT);
#endif

    // How many switches we'll be doing.
    const int max_targets = random_range(std::min((int) targets.size(), 12),
                                         std::min((int) targets.size(), 45));

    // Shuffle the targets, then pick the max_targets first ones.
    std::random_shuffle(targets.begin(), targets.end(), random2);

    // For each of the chosen wall grids, calculate the path connecting the
    // two floor grids to either side, and block off one floor grid on this
    // path to close the circle opened by turning the wall into floor.
    for (int count = 0; count < max_targets; count++)
    {
        const coord_def c(targets[count]);
        // Maybe not valid anymore...
        if (!feat_is_wall(grd(c)) || !_feat_is_flanked_by_walls(c))
            continue;

        // Use the adjacent floor grids as source and destination.
        coord_def src(c.x-1,c.y);
        coord_def dst(c.x+1,c.y);
        if (!feat_has_solid_floor(grd(src)) || !feat_has_solid_floor(grd(dst)))
        {
            src = coord_def(c.x, c.y-1);
            dst = coord_def(c.x, c.y+1);
        }

        // Pathfinding from src to dst...
        monster_pathfind mp;
        bool success = mp.init_pathfind(src, dst, false, msg);
        if (!success)
        {
            if (msg)
            {
                mpr("Something went badly wrong - no path found!",
                    MSGCH_DIAGNOSTICS);
            }
            continue;
        }

        // Get the actual path.
        const std::vector<coord_def> path = mp.backtrack();

        // Replace the wall with floor, but preserve the old grid in case
        // we find no floor grid to swap with.
        // It's better if the change is done now, so the grid can be
        // treated as floor rather than a wall, and we don't need any
        // special cases.
        dungeon_feature_type old_grid = grd(c);
        grd(c) = DNGN_FLOOR;
        los_terrain_changed(c);

        // Add all floor grids meeting a couple of conditions to a vector
        // of potential switch points.
        std::vector<coord_def> points;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def p(path[i]);
            // The point must be inside the changed area.
            if (p.x < c1.x || p.x > c2.x || p.y < c1.y || p.y > c2.y)
                continue;

            // Only replace plain floor.
            if (grd(p) != DNGN_FLOOR)
                continue;

            // Don't change any grids we remember.
            if (is_terrain_seen(p.x, p.y))
                continue;

            // We don't want to deal with monsters being shifted around.
            if (monster_at(p))
                continue;

            // Do not pick a grid right next to the original wall.
            if (std::abs(p.x-c.x) + std::abs(p.y-c.y) <= 1)
                continue;

            if (_feat_is_flanked_by_walls(p) && _deadend_check_wall(p))
                points.push_back(p);
        }

        if (points.empty())
        {
            // Take back the previous change.
            grd(c) = old_grid;
            los_terrain_changed(c);
            continue;
        }

        // Randomly pick one floor grid from the vector and replace it
        // with an adjacent wall type.
        const int pick = random_range(0, (int) points.size() - 1);
        const coord_def p(points[pick]);
        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS, "Switch %d (%d, %d) with %d (%d, %d).",
                 (int) old_grid, c.x, c.y, (int) grd(p), p.x, p.y);
        }
#ifdef WIZARD
        // Highlight the switched grids.
        env.pgrid(c) |= FPROP_HIGHLIGHT;
        env.pgrid(p) |= FPROP_HIGHLIGHT;
#endif

        // Shift blood some of the time.
        if (is_bloodcovered(c))
        {
            if (one_chance_in(4))
            {
                int wall_count = 0;
                coord_def old_adj(c);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_is_wall(grd(*ai)) && one_chance_in(++wall_count))
                        old_adj = *ai;

                if (old_adj != c && maybe_bloodify_square(old_adj))
                    env.pgrid(c) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(500))
        {
            // Rarely add blood randomly, accumulating with time...
            maybe_bloodify_square(c);
        }

        // Rather than use old_grid directly, replace with an adjacent
        // wall type, preferably stone, rock, or metal.
        old_grid = grd[p.x-1][p.y];
        if (!feat_is_wall(old_grid))
        {
            old_grid = grd[p.x][p.y-1];
            if (!feat_is_wall(old_grid))
            {
                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS,
                         "No adjacent walls at pos (%d, %d)?", p.x, p.y);
                }
                old_grid = DNGN_STONE_WALL;
            }
            else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                     && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
            {
                old_grid = grd[p.x][p.y+1];
            }
        }
        else if (old_grid != DNGN_ROCK_WALL && old_grid != DNGN_STONE_WALL
                 && old_grid != DNGN_METAL_WALL && !one_chance_in(3))
        {
            old_grid = grd[p.x+1][p.y];
        }
        grd(p) = old_grid;
        los_terrain_changed(p);

        // Shift blood some of the time.
        if (is_bloodcovered(p))
        {
            if (one_chance_in(4))
            {
                int floor_count = 0;
                coord_def new_adj(p);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (feat_has_solid_floor(grd(*ai)) && one_chance_in(++floor_count))
                        new_adj = *ai;

                if (new_adj != p && maybe_bloodify_square(new_adj))
                    env.pgrid(p) &= (~FPROP_BLOODY);
            }
        }
        else if (one_chance_in(100))
        {
            // Occasionally add blood randomly, accumulating with time...
            maybe_bloodify_square(p);
        }
    }

    // The directions are used to randomly decide where to place items that
    // have ended up in walls during the switching.
    std::vector<coord_def> dirs;
    dirs.push_back(coord_def(-1,-1));
    dirs.push_back(coord_def( 0,-1));
    dirs.push_back(coord_def( 1,-1));
    dirs.push_back(coord_def(-1, 0));

    dirs.push_back(coord_def( 1, 0));
    dirs.push_back(coord_def(-1, 1));
    dirs.push_back(coord_def( 0, 1));
    dirs.push_back(coord_def( 1, 1));

    // Search the entire shifted area for stacks of items now stuck in walls
    // and move them to a random adjacent non-wall grid.
    for (rectangle_iterator ri(c1, c2); ri; ++ri)
    {
        if (!feat_is_wall(grd(*ri)) || igrd(*ri) == NON_ITEM)
            continue;

        if (msg)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Need to move around some items at pos (%d, %d)...",
                 ri->x, ri->y);
        }
        // Search the eight possible directions in random order.
        std::random_shuffle(dirs.begin(), dirs.end(), random2);
        for (unsigned int i = 0; i < dirs.size(); i++)
        {
            const coord_def p = *ri + dirs[i];
            if (!in_bounds(p))
                continue;

            if (feat_has_solid_floor(grd(p)))
            {
                // Once a valid grid is found, move all items from the
                // stack onto it.
                move_items(*ri, p);

                if (msg)
                {
                    mprf(MSGCH_DIAGNOSTICS, "Moved items over to (%d, %d)",
                         p.x, p.y);
                }
                break;
            }
        }
    }

    // Recheck item coordinates, to make totally sure.
    fix_item_coordinates();

    // Finally, give the player a clue about what just happened.
    const int which = (silenced(you.pos()) ? 2 + random2(2)
                                           : random2(4));
    switch (which)
    {
    case 0: mpr("You hear an odd grinding sound!"); break;
    case 1: mpr("You hear the creaking of ancient gears!"); break;
    case 2: mpr("The floor suddenly vibrates beneath you!"); break;
    case 3: mpr("You feel a sudden draft!"); break;
    }
}

static bool _food_item_needs_time_check(item_def &item)
{
    if (!item.is_valid())
        return (false);

    if (item.base_type != OBJ_CORPSES
        && item.base_type != OBJ_FOOD
        && item.base_type != OBJ_POTIONS)
    {
        return (false);
    }

    if (item.base_type == OBJ_CORPSES
        && item.sub_type > CORPSE_SKELETON)
    {
        return (false);
    }

    if (item.base_type == OBJ_FOOD && item.sub_type != FOOD_CHUNK)
        return (false);

    if (item.base_type == OBJ_POTIONS && !is_blood_potion(item))
        return (false);

    return (true);
}

#define ROTTING_WARNED_KEY "rotting_warned"

static void _rot_inventory_food(long time_delta)
{
    // Update all of the corpses and food chunks in the player's
    // inventory. {should be moved elsewhere - dlb}
    bool burden_changed_by_rot = false;
    std::vector<char> rotten_items;

    int num_chunks         = 0;
    int num_chunks_gone    = 0;
    int num_bones          = 0;
    int num_bones_gone     = 0;
    int num_corpses        = 0;
    int num_corpses_rotted = 0;
    int num_corpses_gone   = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (item.quantity < 1)
            continue;

        if (!_food_item_needs_time_check(item))
            continue;

        if (item.base_type == OBJ_POTIONS)
        {
            // Also handles messaging.
            if (maybe_coagulate_blood_potions_inv(item))
                burden_changed_by_rot = true;
            continue;
        }

        if (item.base_type == OBJ_FOOD)
            num_chunks++;
        else if (item.sub_type == CORPSE_SKELETON)
            num_bones++;
        else
            num_corpses++;

        // Food item timed out -> make it disappear.
        if ((time_delta / 20) >= item.special)
        {
            if (item.base_type == OBJ_FOOD)
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                // In case time_delta >= 220
                if (!item.props.exists(ROTTING_WARNED_KEY))
                    num_chunks_gone++;

                destroy_item(item);
                burden_changed_by_rot = true;

                continue;
            }

            // The item is of type carrion.
            if (item.sub_type == CORPSE_SKELETON
                || !mons_skeleton(item.plus))
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                if (item.sub_type == CORPSE_SKELETON)
                    num_bones_gone++;
                else
                    num_corpses_gone++;

                destroy_item(item);
                burden_changed_by_rot = true;
                continue;
            }

            turn_corpse_into_skeleton(item);
            if (you.equip[EQ_WEAPON] == i)
                you.wield_change = true;
            burden_changed_by_rot = true;

            num_corpses_rotted++;
            continue;
        }

        // If it hasn't disappeared, reduce the rotting timer.
        item.special -= (time_delta / 20);

        if (food_is_rotten(item)
            && (item.special + (time_delta / 20) >= 100))
        {
            rotten_items.push_back(index_to_letter(i));
            if (you.equip[EQ_WEAPON] == i)
                you.wield_change = true;
        }
    }

    //mv: messages when chunks/corpses become rotten
    if (!rotten_items.empty())
    {
        std::string msg = "";

        // Races that can't smell don't care, and trolls are stupid and
        // don't care.
        if (you.can_smell() && you.species != SP_TROLL)
        {
            int temp_rand = 0; // Grr.
            int level = player_mutation_level(MUT_SAPROVOROUS);
            if (!level && you.species == SP_VAMPIRE)
                level = 1;

            switch (level)
            {
            // level 1 and level 2 saprovores, as well as vampires, aren't so touchy
            case 1:
            case 2:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "You smell rotting flesh." :
                      (temp_rand == 6) ? "You smell decay."
                                       : "There is something rotten in your inventory.";
                break;

            // level 3 saprovores like it
            case 3:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you hungry." :
                      (temp_rand == 6) ? "You smell decay. Yum-yum."
                                       : "Wow! There is something tasty in your inventory.";
                break;

            default:
                temp_rand = random2(8);
                msg = (temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you sick." :
                      (temp_rand == 6) ? "You smell decay. Yuck!"
                                       : "Ugh! There is something really disgusting in your inventory.";
                break;
            }
        }
        else if (Options.list_rotten)
            msg = "Something in your inventory has become rotten.";

        if (Options.list_rotten)
        {
            mprf(MSGCH_ROTTEN_MEAT, "%s (slot%s %s)",
                 msg.c_str(),
                 rotten_items.size() > 1 ? "s" : "",
                 comma_separated_line(rotten_items.begin(),
                                      rotten_items.end()).c_str());
        }
        else if (!msg.empty())
            mpr(msg.c_str(), MSGCH_ROTTEN_MEAT);

        learned_something_new(HINT_ROTTEN_FOOD);
    }

    if (burden_changed_by_rot)
    {
        if ((num_chunks_gone + num_bones_gone + num_corpses_gone
             + num_corpses_rotted) > 0)
        {
            std::string msg;
            if (num_chunks_gone == num_chunks
                && num_bones_gone == num_bones
                && (num_corpses_gone + num_corpses_rotted) == num_corpses)
            {
                msg = "All of the ";
            }
            else
                msg = "Some of the ";

            std::vector<std::string> strs;
            if (num_chunks_gone > 0)
                strs.push_back("chunks of flesh");
            if (num_bones_gone > 0)
                strs.push_back("skeletons");
            if ((num_corpses_gone + num_corpses_rotted) > 0)
                strs.push_back("corpses");

            msg += comma_separated_line(strs.begin(), strs.end());
            msg += " in your inventory have ";

            if (num_corpses_rotted == 0)
                msg += "completely ";
            else if ((num_chunks_gone + num_bones_gone
                      + num_corpses_gone) == 0)
            {
                msg += "partially ";
            }
            else
                msg += "completely or partially ";

            msg += "rotted away.";
            mprf(MSGCH_ROTTEN_MEAT, "%s", msg.c_str());
        }
        burden_change();
    }
}

// Get around C++ dividing integers towards 0.
static int _div(int num, int denom)
{
    div_t res = div(num, denom);
    return (res.rem >= 0 ? res.quot : res.quot - 1);
}

// Do various time related actions...
void handle_time()
{
    int base_time = you.elapsed_time % 200;
    int old_time = base_time - you.time_taken;

    // The checks below assume the function is called at least
    // once every 50 elapsed time units.

    // Every 5 turns, spawn random monsters.
    if (_div(base_time, 50) > _div(old_time, 50))
        spawn_random_monsters();

    // Every 20 turns, a variety of other effects.
    if (! (_div(base_time, 200) > _div(old_time, 200)))
        return;

    int time_delta = 200;

    // Update all of the corpses, food chunks, and potions of blood on
    // the floor.
    update_corpses(time_delta);

    if (crawl_state.game_is_arena())
        return;

    // Nasty things happen to people who spend too long in Hell.
    if (player_in_hell() && coinflip())
        _hell_effects();

    // Adjust the player's stats if s/he's diseased (or recovering).
    if (!you.disease)
    {
        bool recovery = true;

        // The better-fed you are, the faster your stat recovery.
        if (you.species == SP_VAMPIRE)
        {
            if (you.hunger_state == HS_STARVING)
                // No stat recovery for starving vampires.
                recovery = false;
            else if (you.hunger_state <= HS_HUNGRY)
                // Halved stat recovery for hungry vampires.
                recovery = coinflip();
        }

        // Slow heal mutation.  Applied last.
        // Each level reduces your stat recovery by one third.
        if (player_mutation_level(MUT_SLOW_HEALING) > 0
            && x_chance_in_y(player_mutation_level(MUT_SLOW_HEALING), 3))
        {
            recovery = false;
        }

        // Rate of recovery equals one level of MUT_DETERIORATION.
        if (recovery && x_chance_in_y(4, 200))
            restore_stat(STAT_RANDOM, 1, false, true);
    }
    else
    {
        // If Cheibriados has slowed your biology, disease might
        // not actually do anything.
        if (one_chance_in(30)
            && !(you.religion == GOD_CHEIBRIADOS
                 && you.piety >= piety_breakpoint(0)
                 && coinflip()))
        {
            mpr("Your disease is taking its toll.", MSGCH_WARN);
            lose_stat(STAT_RANDOM, 1, false, "disease");
        }
    }

    // Adjust the player's stats if s/he has the deterioration mutation.
    if (player_mutation_level(MUT_DETERIORATION)
        && x_chance_in_y(player_mutation_level(MUT_DETERIORATION) * 5 - 1, 200))
    {
        lose_stat(STAT_RANDOM, 1, false, "deterioration mutation");
    }

    int added_contamination = 0;

    // Account for mutagenic radiation.  Invis and haste will give the
    // player about .1 points per turn, mutagenic randarts will give
    // about 1.5 points on average, so they can corrupt the player
    // quite quickly.  Wielding one for a short battle is OK, which is
    // as things should be.   -- GDL
    if (you.duration[DUR_INVIS] && x_chance_in_y(6, 10))
        added_contamination++;

    if (you.duration[DUR_HASTE] && !you.berserk() && x_chance_in_y(6, 10))
        added_contamination++;

    bool mutagenic_randart = false;
    if (const int artefact_glow = scan_artefacts(ARTP_MUTAGENIC))
    {
        // Reduced randart glow. Note that one randart will contribute
        // 2 - 5 units of glow to artefact_glow. A randart with a mutagen
        // index of 2 does about 0.58 points of contamination per turn.
        // A randart with a mutagen index of 5 does about 0.7 points of
        // contamination per turn.

        const int mean_glow   = 500 + artefact_glow * 40;
        const int actual_glow = mean_glow / 2 + random2(mean_glow);
        added_contamination += div_rand_round(actual_glow, 1000);
        mutagenic_randart = true;
    }

    // We take off about .5 points per turn.
    if (!you.duration[DUR_INVIS] && !you.duration[DUR_HASTE] && coinflip())
        added_contamination--;

    // Only punish if contamination caused by mutagenic randarts.
    // (Haste and invisibility already penalised earlier.)
    contaminate_player( added_contamination, mutagenic_randart );

    // Only check for badness once every other turn.
    if (coinflip())
    {
        // [ds] Be less harsh with glow mutation; Brent and Mark Mackey note
        // that the commented out random2(X) <= MC check was a bug. I've
        // uncommented it but dropped the roll sharply from 150. (Brent used
        // the original roll of 150 for 4.1.2, but I think players are
        // sufficiently used to beta 26's unkindness that we can use a lower
        // roll.)
        if (is_sanctuary(you.pos())
            && you.magic_contamination > 5
            && x_chance_in_y(you.magic_contamination + 1, 25))
        {
            mpr("Your body momentarily shudders from a surge of wild "
                "energies until Zin's power calms it.", MSGCH_GOD);
        }
        else if (you.magic_contamination > 5
                 && x_chance_in_y(you.magic_contamination + 1, 25))
        {
            mpr("Your body shudders with the violent release "
                "of wild energies!", MSGCH_WARN);

            // For particularly violent releases, make a little boom.
            // Undead enjoy extra contamination explosion damage because
            // the magical contamination has a harder time dissipating
            // through non-living flesh. :-)
            if (you.magic_contamination > 10 && coinflip())
            {
                bolt beam;

                beam.flavour      = BEAM_RANDOM;
                beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
                beam.damage       = dice_def(3, you.magic_contamination
                                             * (you.is_undead ? 4 : 2) / 4);
                beam.target       = you.pos();
                beam.name         = "magical storm";
                beam.beam_source  = NON_MONSTER;
                beam.aux_source   = "a magical explosion";
                beam.ex_size      = std::max(1, std::min(9,
                                        you.magic_contamination / 15));
                beam.ench_power   = you.magic_contamination * 5;
                beam.is_explosion = true;

                beam.explode();
            }

            // We want to warp the player, not do good stuff!
            if (one_chance_in(5))
                mutate(RANDOM_MUTATION);
            else
                give_bad_mutation(true, coinflip());

            // we're meaner now, what with explosions and whatnot, but
            // we dial down the contamination a little faster if its actually
            // mutating you.  -- GDL
            contaminate_player(-(random2(you.magic_contamination / 4) + 1));
        }
    }

    // Random chance to identify staff in hand based off of Spellcasting
    // and an appropriate other spell skill... is 1/20 too fast?
    if (you.weapon()
        && you.weapon()->base_type == OBJ_STAVES
        && !item_type_known(*you.weapon())
        && one_chance_in(20))
    {
        int total_skill = you.skills[SK_SPELLCASTING];

        switch (you.weapon()->sub_type)
        {
        case STAFF_WIZARDRY:
        case STAFF_ENERGY:
            total_skill += you.skills[SK_SPELLCASTING];
            break;
        case STAFF_FIRE:
            if (you.skills[SK_FIRE_MAGIC] > you.skills[SK_ICE_MAGIC])
                total_skill += you.skills[SK_FIRE_MAGIC];
            else
                total_skill += you.skills[SK_ICE_MAGIC];
            break;
        case STAFF_COLD:
            if (you.skills[SK_ICE_MAGIC] > you.skills[SK_FIRE_MAGIC])
                total_skill += you.skills[SK_ICE_MAGIC];
            else
                total_skill += you.skills[SK_FIRE_MAGIC];
            break;
        case STAFF_AIR:
            if (you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
                total_skill += you.skills[SK_AIR_MAGIC];
            else
                total_skill += you.skills[SK_EARTH_MAGIC];
            break;
        case STAFF_EARTH:
            if (you.skills[SK_EARTH_MAGIC] > you.skills[SK_AIR_MAGIC])
                total_skill += you.skills[SK_EARTH_MAGIC];
            else
                total_skill += you.skills[SK_AIR_MAGIC];
            break;
        case STAFF_POISON:
            total_skill += you.skills[SK_POISON_MAGIC];
            break;
        case STAFF_DEATH:
            total_skill += you.skills[SK_NECROMANCY];
            break;
        case STAFF_CONJURATION:
            total_skill += you.skills[SK_CONJURATIONS];
            break;
        case STAFF_ENCHANTMENT:
            total_skill += you.skills[SK_ENCHANTMENTS];
            break;
        case STAFF_SUMMONING:
            total_skill += you.skills[SK_SUMMONINGS];
            break;
        }

        if (x_chance_in_y(total_skill, 100))
        {
            item_def& item = *you.weapon();

            set_ident_type(OBJ_STAVES, item.sub_type, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_IDENT_MASK);

            mprf("You are wielding %s.", item.name(DESC_NOCAP_A).c_str());
            more();

            you.wield_change = true;
        }
    }

    // Check to see if an upset god wants to do something to the player.
    handle_god_time();

    if (player_mutation_level(MUT_SCREAM)
        && x_chance_in_y(3 + player_mutation_level(MUT_SCREAM) * 3, 100))
    {
        yell(true);
    }

    _rot_inventory_food(time_delta);

    // Exercise armour *xor* stealth skill: {dlb}
    if (one_chance_in(6) && you.check_train_armour())
    {
        // Armour trained in check_train_armour
    }
    // Exercise stealth skill:
    else if (you.burden_state == BS_UNENCUMBERED
             && !you.berserk()
             && !you.attribute[ATTR_SHADOWS])
    {
        const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const int armour_mass = body_armour? item_mass(*body_armour) : 0;
        if (!x_chance_in_y(armour_mass, 1000)
            // Diminishing returns for stealth training by waiting.
            && you.skills[SK_STEALTH] <= 2 + random2(3)
            && one_chance_in(18))
        {
            exercise(SK_STEALTH, 1);
        }
    }

    if (you.level_type == LEVEL_LABYRINTH)
    {
        // Now that the labyrinth can be automapped, apply map rot as
        // a counter-measure. (Those mazes sure are easy to forget.)
        forget_map(you.species == SP_MINOTAUR ? 25 : 45);

        // From time to time change a section of the labyrinth.
        if (one_chance_in(10))
            change_labyrinth();
    }

    if (you.religion == GOD_JIYVA && one_chance_in(10))
    {
        int total_jellies = 1 + random2(5);
        bool success = false;
        for (int num_jellies = total_jellies; num_jellies > 0; num_jellies--)
        {
            // Spread jellies around the level.
            coord_def newpos;
            do
                newpos = random_in_bounds();
            while (grd(newpos) != DNGN_FLOOR
                       && grd(newpos) != DNGN_SHALLOW_WATER
                   || monster_at(newpos)
                   || env.cgrid(newpos) != EMPTY_CLOUD);

            mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0, 0, newpos,
                         MHITNOT, 0, GOD_JIYVA);
            mg.non_actor_summoner = "Jiyva";

            if (create_monster(mg) != -1)
                success = true;
        }

        if (success && !silenced(you.pos()))
        {
            switch (random2(3))
            {
                case 0:
                    simple_god_message(" gurgles merrily.");
                    break;
                case 1:
                    mprf(MSGCH_SOUND, "You hear %s splatter%s.",
                         total_jellies > 1 ? "a series of" : "a",
                         total_jellies > 1 ? "s" : "");
                    break;
                case 2:
                    simple_god_message(" says: Divide and consume!");
                    break;
            }
        }
    }

    if (you.religion == GOD_JIYVA && x_chance_in_y(you.piety / 4, MAX_PIETY)
        && !player_under_penance() && one_chance_in(4))
    {
        jiyva_stat_action();
    }
}

// Move monsters around to fake them walking around while player was
// off-level. Also let them go back to sleep eventually.
static void _catchup_monster_moves(monsters *mon, int turns)
{
    // Summoned monsters might have disappeared.
    if (!mon->alive())
        return;

    // Don't move non-land or stationary monsters around.
    if (mons_primary_habitat(mon) != HT_LAND
        || mons_is_zombified(mon)
           && mons_class_primary_habitat(mon->base_monster) != HT_LAND
        || mons_is_stationary(mon))
    {
        return;
    }

    // Don't shift giant spores since that would disrupt their trail.
    if (mon->type == MONS_GIANT_SPORE)
        return;

    // Let sleeping monsters lie.
    if (mon->asleep() || mon->paralysed())
        return;

    const int range = (turns * mon->speed) / 10;
    const int moves = (range > 50) ? 50 : range;

    const bool ranged_attack = (mons_has_ranged_spell(mon, true)
                                || mons_has_ranged_attack(mon));

#ifdef DEBUG_DIAGNOSTICS
    // probably too annoying even for DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "mon #%d: range %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %ld",
         mon->mindex(), range, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags );
#endif

    if (range <= 0)
        return;

    // After x turns, half of the monsters will have forgotten about the
    // player, and a quarter has gone to sleep. A given monster has a
    // 95% chance of forgetting the player after 4*x turns, and going to
    // sleep after 10*x turns.
    int x = 0; // Quiet unitialized variable compiler warning.
    switch (mons_intel(mon))
    {
    case I_HIGH:
        x = 1000;
        break;
    case I_NORMAL:
        x = 500;
        break;
    case I_ANIMAL:
    case I_INSECT:
        x = 250;
        break;
    case I_PLANT:
        x = 125;
        break;
    }

    bool changed = 0;
    for  (int i = 0; i < range/x; i++)
    {
        if (mon->behaviour == BEH_SLEEP)
            break;

        if (coinflip())
        {
            changed = 1;
            if (coinflip())
                mon->behaviour = BEH_SLEEP;
            else
            {
                mon->behaviour = BEH_WANDER;
                mon->foe = MHITNOT;
                mon->target = random_in_bounds();
            }
        }
    }

    if (ranged_attack && !changed)
    {
        // If we're doing short time movement and the monster has a
        // ranged attack (missile or spell), then the monster will
        // flee to gain distance if it's "too close", else it will
        // just shift its position rather than charge the player. -- bwr
        if (grid_distance(mon->pos(), mon->target) < 3)
        {
            mon->behaviour = BEH_FLEE;

            // If the monster is on the target square, fleeing won't
            // work.
            if (mon->pos() == mon->target)
            {
                if (you.pos() != mon->pos())
                {
                    // Flee from player's position if different.
                    mon->target = you.pos();
                }
                else
                {
                    coord_def mshift(random2(3) - 1, random2(3) - 1);

                    // Bounds check: don't let fleeing monsters try to
                    // run off the grid.
                    const coord_def s = mon->target + mshift;
                    if (!in_bounds_x(s.x))
                        mshift.x = 0;
                    if (!in_bounds_y(s.y))
                        mshift.y = 0;

                    // Randomise the target so we have a direction to
                    // flee.
                    mon->target.x += mshift.x;
                    mon->target.y += mshift.y;
                }
            }

            dprf("backing off...");
        }
        else
        {
            shift_monster(mon, mon->pos());

#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "shifted to (%d, %d)",
                 mon->pos().x, mon->pos().y);
#endif
            return;
        }
    }

    coord_def pos(mon->pos());

    // Dirt simple movement.
    for (int i = 0; i < moves; ++i)
    {
        coord_def inc(mon->target - pos);
        inc = coord_def(sgn(inc.x), sgn(inc.y));

        if (mons_is_fleeing(mon))
            inc *= -1;

        // Bounds check: don't let shifting monsters try to run off the
        // grid.
        const coord_def s = pos + inc;
        if (!in_bounds_x(s.x))
            inc.x = 0;
        if (!in_bounds_y(s.y))
            inc.y = 0;

        if (inc.origin())
            break;

        const coord_def next(pos + inc);
        const dungeon_feature_type feat = grd(next);
        if (feat_is_solid(feat)
            || monster_at(next)
            || !monster_habitable_grid(mon, feat))
        {
            break;
        }

        pos = next;
    }

    if (!shift_monster(mon, pos))
        shift_monster(mon, mon->pos());

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "moved to (%d, %d)", mon->pos().x, mon->pos().y);
#endif
}

//---------------------------------------------------------------
//
// update_level
//
// Update the level when the player returns to it.
//
//---------------------------------------------------------------
void update_level(long elapsedTime)
{
    ASSERT(!crawl_state.game_is_arena());

    const int turns = elapsedTime / 10;

#ifdef DEBUG_DIAGNOSTICS
    int mons_total = 0;

    mprf(MSGCH_DIAGNOSTICS, "turns: %d", turns );
#endif

    update_corpses(elapsedTime);
    shoals_apply_tides(turns, true);
    recharge_rods(turns, true);

    if (env.sanctuary_time)
    {
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
          env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (monster_iterator mi; mi; ++mi)
    {
#ifdef DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        // Pacified monsters often leave the level now.
        if (mi->pacified() && turns > random2(40) + 21)
        {
            make_mons_leave_level(*mi);
            continue;
        }

        // Following monsters don't get movement.
        if (mi->flags & MF_JUST_SUMMONED)
            continue;

        // XXX: Allow some spellcasting (like Healing and Teleport)? - bwr
        // const bool healthy = (mi->hit_points * 2 > mi->max_hit_points);

        // This is the monster healing code, moved here from tag.cc:
        if (mons_can_regenerate(*mi))
        {
            if (monster_descriptor(mi->type, MDSC_REGENERATES)
                || mi->type == MONS_PLAYER_GHOST)
            {
                mi->heal(turns);
            }
            else
            {
                // Set a lower ceiling of 0.1 on the regen rate.
                const int regen_rate =
                    std::max(mons_natural_regen_rate(*mi) * 2, 5);

                mi->heal(div_rand_round(turns * regen_rate, 50));
            }
        }

        // Handle nets specially to remove the trapping property of the net.
        if (mi->caught())
            mi->del_ench(ENCH_HELD, true);

        _catchup_monster_moves(*mi, turns);

        if (turns >= 10 && mi->alive())
            mi->timeout_enchantments(turns / 10);
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "total monsters on level = %d", mons_total );
#endif

    for (int i = 0; i < MAX_CLOUDS; i++)
        delete_cloud(i);
}

static void _maybe_restart_fountain_flow(const coord_def& where,
                                         const int tries)
{
    dungeon_feature_type grid = grd(where);

    if (grid < DNGN_DRY_FOUNTAIN_BLUE || grid > DNGN_DRY_FOUNTAIN_BLOOD)
        return;


    for (int i = 0; i < tries; ++i)
    {
        if (!one_chance_in(100))
            continue;

        // Make it start flowing again.
        grd(where) = static_cast<dungeon_feature_type> (grid
                        - (DNGN_DRY_FOUNTAIN_BLUE - DNGN_FOUNTAIN_BLUE));

        if (is_terrain_seen(where))
            set_map_knowledge_obj(where, grd(where));

        // Clean bloody floor.
        if (is_bloodcovered(where))
            env.pgrid(where) &= ~(FPROP_BLOODY);

        // Chance of cleaning adjacent squares.
        for (adjacent_iterator ai(where); ai; ++ai)
            if (is_bloodcovered(*ai) && one_chance_in(5))
                env.pgrid(*ai) &= ~(FPROP_BLOODY);

        break;
   }
}


// A comparison struct for use in an stl priority queue.
template<typename T>
struct greater_second
{
    // The stl priority queue is a max queue and uses < as the default
    // comparison.  We want a min queue so we have to use a > operation
    // here.
    bool operator()(const T & left, const T & right)
    {
        return (left.second > right.second);
    }
};

// Basically we want to break a circle into n_arcs equal sized arcs and find
// out which arc the input point pos falls on.
static int _arc_decomposition(const coord_def & pos, int n_arcs)
{
    float theta = atan2((float)pos.y, (float)pos.x);

    if (pos.x == 0 && pos.y != 0)
        theta = pos.y > 0 ? PI / 2 : -PI / 2;

    if (theta < 0)
        theta += 2 * PI;

    float arc_angle = 2 * PI / n_arcs;

    theta += arc_angle / 2.0f;

    if (theta >= 2 * PI)
        theta -= 2 * PI;

    return static_cast<int> (theta / arc_angle);
}

int place_ring(std::vector<coord_def> &ring_points,
               const coord_def &origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int &seen_count)
{
    std::random_shuffle(ring_points.begin(),
                        ring_points.end());

    int target_amount = ring_points.size();
    int spawned_count = 0;
    seen_count = 0;

    std::vector<int> arc_counts(n_arcs, arc_occupancy);

    for (unsigned i = 0;
         spawned_count < target_amount && i < ring_points.size();
         i++)
    {
        int direction = _arc_decomposition(ring_points.at(i)
                                           - origin, n_arcs);

        if (arc_counts[direction]-- <= 0)
            continue;

        prototype.pos = ring_points.at(i);

        const int mushroom = create_monster(prototype, false);

        if (mushroom != -1)
        {
            spawned_count++;
            if (you.see_cell(ring_points.at(i)))
                seen_count++;
        }
    }

    return (spawned_count);
}

// Collect lists of points that are within LOS (under the given env map),
// unoccupied, and not solid (walls/statues).
void collect_radius_points(std::vector<std::vector<coord_def> > &radius_points,
                           const coord_def &origin, const los_base* los)
{

    radius_points.clear();
    radius_points.resize(LOS_RADIUS);

    // Just want to associate a point with a distance here for convenience.
    typedef std::pair<coord_def, int> coord_dist;

    // Using a priority queue because squares don't make very good circles at
    // larger radii.  We will visit points in order of increasing euclidean
    // distance from the origin (not path distance).
    std::priority_queue<coord_dist,
                        std::vector<coord_dist>,
                        greater_second<coord_dist> > fringe;

    fringe.push(coord_dist(origin, 0));

    std::set<int> visited_indices;

    int current_r = 1;
    int current_thresh = current_r * (current_r + 1);

    int max_distance = LOS_RADIUS * LOS_RADIUS + 1;

    while (!fringe.empty())
    {
        coord_dist current = fringe.top();
        // We're done here once we hit a point that is farther away from the
        // origin than our maximum permissible radius.
        if (current.second > max_distance)
            break;

        fringe.pop();


        int idx = current.first.x + current.first.y * X_WIDTH;
        if (!visited_indices.insert(idx).second)
            continue;

        while (current.second > current_thresh)
        {
            current_r++;
            current_thresh = current_r * (current_r + 1);
        }

        // We don't include radius 0.  This is also a good place to check if
        // the squares are already occupied since we want to search past
        // occupied squares but don't want to consider them valid targets.
        if (current.second && !actor_at(current.first))
            radius_points[current_r - 1].push_back(current.first);

        for (adjacent_iterator i(current.first); i; ++i)
        {
            coord_dist temp(*i, current.second);

            // If the grid is out of LOS, skip it.
            if (!los->see_cell(temp.first))
                continue;

            coord_def local = temp.first - origin;

            temp.second = local.abs();

            idx = temp.first.x + temp.first.y * X_WIDTH;

            if (visited_indices.find(idx) == visited_indices.end()
                && in_bounds(temp.first)
                && !cell_is_solid(temp.first))
            {
                fringe.push(temp);
            }
        }

    }
}

// Place a partial ring of toadstools around the given corpse.  Returns
// the number of mushrooms spawned.  A return of 0 indicates no
// mushrooms were placed -> some sort of failure mode was reached.
static int _mushroom_ring(item_def &corpse, int & seen_count,
                          beh_type toadstool_behavior)
{
    // minimum number of mushrooms spawned on a given ring
    unsigned min_spawn = 2;

    seen_count = 0;

    std::vector<std::vector<coord_def> > radius_points;

    los_def los(corpse.pos, opc_solid);

    collect_radius_points(radius_points, corpse.pos, &los);

    // So what we have done so far is collect the set of points at each radius
    // reachable from the origin with (somewhat constrained) 8 connectivity,
    // now we will choose one of those radii and spawn mushrooms at some
    // of the points along it.
    int chosen_idx = random2(LOS_RADIUS);

    unsigned max_size = 0;
    for (unsigned i = 0; i < LOS_RADIUS; ++i)
    {
        if (radius_points[i].size() >= max_size)
        {
            max_size = radius_points[i].size();
            chosen_idx = i;
        }
    }

    chosen_idx = random2(chosen_idx + 1);

    // Not enough valid points?
    if (radius_points[chosen_idx].size() < min_spawn)
        return (0);

    mgen_data temp(MONS_TOADSTOOL,
                   toadstool_behavior, 0, 0, 0,
                   coord_def(),
                   MHITNOT,
                   MG_FORCE_PLACE,
                   GOD_NO_GOD,
                   MONS_NO_MONSTER,
                   0,
                   corpse.colour);

    float target_arc_len = 2 * sqrtf(2.0f);

    int n_arcs = static_cast<int> (ceilf(2 * PI * (chosen_idx + 1)
                                   / target_arc_len));

    int spawned_count = place_ring(radius_points[chosen_idx], corpse.pos, temp,
                                   n_arcs, 1, seen_count);

    return (spawned_count);
}

// Try to spawn 'target_count' mushrooms around the position of
// 'corpse'.  Returns the number of mushrooms actually spawned.
// Mushrooms radiate outwards from the corpse following bfs with
// 8-connectivity.  Could change the expansion pattern by using a
// priority queue for sequencing (priority = distance from origin under
// some metric).
int spawn_corpse_mushrooms(item_def &corpse,
                           int target_count,
                           int & seen_targets,
                           beh_type toadstool_behavior,
                           bool distance_as_time)

{
    seen_targets = 0;
    if (target_count == 0)
        return (0);

    int c_size = 8;
    int permutation[] = {0, 1, 2, 3, 4, 5, 6, 7};

    int placed_targets = 0;

    std::queue<coord_def> fringe;
    std::set<int> visited_indices;

    // Slight chance of spawning a ring of mushrooms around the corpse (and
    // skeletonising it) if the corpse square is unoccupied.
    if (!actor_at(corpse.pos) && one_chance_in(100))
    {
        int ring_seen;
        // It's possible no reasonable ring can be found, in that case we'll
        // give up and just place a toadstool on top of the corpse (probably).
        int res = _mushroom_ring(corpse, ring_seen, toadstool_behavior);

        if (res)
        {
            corpse.special = 0;

            if (you.see_cell(corpse.pos))
                mpr("A ring of toadstools grows before your very eyes.");
            else if (ring_seen > 1)
                mpr("Some toadstools grow in a peculiar arc.");
            else if (ring_seen > 0)
                mpr("A toadstool grows.");

            seen_targets = -1;

            return (res);
        }
    }

    visited_indices.insert(X_WIDTH * corpse.pos.y + corpse.pos.x);
    fringe.push(corpse.pos);

    while (!fringe.empty())
    {
        coord_def current = fringe.front();

        fringe.pop();

        monsters * monster = monster_at(current);

        bool player_occupant = you.pos() == current;

        // Is this square occupied by a non mushroom?
        if (monster && monster->mons_species() != MONS_TOADSTOOL
            || player_occupant && you.religion != GOD_FEDHAS
            || !is_harmless_cloud(cloud_type_at(current)))
        {
            continue;
        }

        if (!monster)
        {
            const int mushroom = create_monster(
                        mgen_data(MONS_TOADSTOOL,
                                  toadstool_behavior,
                                  0,
                                  0,
                                  0,
                                  current,
                                  MHITNOT,
                                  MG_FORCE_PLACE,
                                  GOD_NO_GOD,
                                  MONS_NO_MONSTER,
                                  0,
                                  corpse.colour),
                                  false);

            if (mushroom != -1)
            {
                // Going to explicitly override the die-off timer in
                // this case (this condition means we got called from
                // fungal_bloom() or similar, and are creating a lot of
                // toadstools at once that should die off quickly).
                if (distance_as_time)
                {
                    coord_def offset = corpse.pos - current;

                    int dist = static_cast<int>(sqrtf(offset.abs()) + 0.5);

                    // Trying a longer base duration...
                    int time_left = random2(8) + dist * 8 + 8;

                    time_left *= 10;

                    mon_enchant temp_en(ENCH_SLOWLY_DYING, 1, KC_OTHER,
                                        time_left);

                    env.mons[mushroom].update_ench(temp_en);
                }

                placed_targets++;
                if (current == you.pos())
                {
                    mprf("A toadstool grows at your feet.");
                    current=  env.mons[mushroom].pos();
                }
                else if (you.see_cell(current))
                    seen_targets++;
            }
            else
                continue;
        }

        // We're done here if we placed the desired number of mushrooms.
        if (placed_targets == target_count)
            break;

        // Wish adjacent_iterator had a random traversal.
        std::random_shuffle(permutation, permutation+c_size);

        for (int count = 0; count < c_size; ++count)
        {
            coord_def temp = current + Compass[permutation[count]];

            int index = temp.x + temp.y * X_WIDTH;

            if (visited_indices.find(index) == visited_indices.end()
                && in_bounds(temp)
                && mons_class_can_pass(MONS_TOADSTOOL, grd(temp)))
            {

                visited_indices.insert(index);
                fringe.push(temp);
            }
        }
    }

    return (placed_targets);
}

int mushroom_prob(item_def & corpse)
{
    int low_threshold = 5;
    int high_threshold = FRESHEST_CORPSE - 5;

    // Expect this many trials over a corpse's lifetime since this function
    // is called once for every 10 units of rot_time.
    int step_size = 10;
    float total_trials = (high_threshold - low_threshold) / step_size;

    // Chance of producing no mushrooms (not really because of weight_factor
    // below).
    float p_failure = 0.5f;

    float trial_prob_f = 1 - powf(p_failure, 1.0f / total_trials);

    // The chance of producing mushrooms depends on the weight of the
    // corpse involved.  Humans weigh 550 so we will take that as the
    // base factor here.
    float weight_factor = item_mass(corpse) / 550.0f;

    trial_prob_f *= weight_factor;

    int trial_prob = static_cast<int>(100 * trial_prob_f);

    return (trial_prob);
}

bool mushroom_spawn_message(int seen_targets, int seen_corpses)
{
    if (seen_targets > 0)
    {
        std::string what  = seen_targets  > 1 ? "Some toadstools"
                                              : "A toadstool";
        std::string where = seen_corpses  > 1 ? "nearby corpses" :
                            seen_corpses == 1 ? "a nearby corpse"
                                              : "the ground";
        mprf("%s grow%s from %s.",
             what.c_str(), seen_targets > 1 ? "" : "s", where.c_str());

        return (true);
    }

    return (false);
}

// Randomly decide whether or not to spawn a mushroom over the given
// corpse.  Assumption: this is called before the rotting away logic in
// update_corpses.  Some conditions in this function may set the corpse
// timer to 0, assuming that the corpse will be turned into a
// skeleton/destroyed on this update.
static void _maybe_spawn_mushroom(item_def & corpse, int rot_time)
{
    // We won't spawn a mushroom within 10 turns of the corpse's being created
    // or rotting away.
    int low_threshold  = 5;
    int high_threshold = FRESHEST_CORPSE - 15;

    if (corpse.special < low_threshold || corpse.special > high_threshold)
        return;

    int spawn_time = (rot_time > corpse.special ? corpse.special : rot_time);

    if (spawn_time > high_threshold)
        spawn_time = high_threshold;

    int step_size = 10;

    int current_trials = spawn_time / step_size;
    int trial_prob     = mushroom_prob(corpse);
    int success_count  = binomial_generator(current_trials, trial_prob);

    int seen_spawns;
    spawn_corpse_mushrooms(corpse, success_count, seen_spawns);
    mushroom_spawn_message(seen_spawns, you.see_cell(corpse.pos) ? 1 : 0);
}

//---------------------------------------------------------------
//
// update_corpses
//
// Update all of the corpses and food chunks on the floor.
//
//---------------------------------------------------------------
void update_corpses(long elapsedTime)
{
    if (elapsedTime <= 0)
        return;

    const long rot_time = elapsedTime / 20;

    for (int c = 0; c < MAX_ITEMS; ++c)
    {
        item_def &it = mitm[c];

        if (!_food_item_needs_time_check(it))
            continue;

        if (it.base_type == OBJ_POTIONS)
        {
            maybe_coagulate_blood_potions_floor(c);
            continue;
        }

        if (it.sub_type == CORPSE_BODY)
            _maybe_spawn_mushroom(it, rot_time);

        if (rot_time >= it.special && !is_being_butchered(it))
        {
            if (it.base_type == OBJ_FOOD)
                destroy_item(c);
            else
            {
                if (it.sub_type == CORPSE_SKELETON
                    || !mons_skeleton(it.plus))
                {
                    destroy_item(c);
                }
                else
                    turn_corpse_into_skeleton(it);
            }
        }
        else
            it.special -= rot_time;
    }

    int fountain_checks = elapsedTime / 1000;
    if (x_chance_in_y(elapsedTime % 1000, 1000))
        fountain_checks += 1;

    // Dry fountains may start flowing again.
    if (fountain_checks > 0)
    {
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_DRY_FOUNTAIN_BLUE
                && grd(*ri) < DNGN_PERMADRY_FOUNTAIN)
            {
                _maybe_restart_fountain_flow(*ri, fountain_checks);
            }
        }
    }
}

static void _recharge_rod( item_def &rod, long aut, bool in_inv )
{
    if (!item_is_rod(rod) || rod.plus >= rod.plus2)
        return;

    long rate = 4 + short(rod.props["rod_enchantment"]);

    rate *= (10 + skill_bump( SK_EVOCATIONS ));
    rate *= aut;
    rate = div_rand_round( rate, 100 );

    if (rate > rod.plus2 - rod.plus) // Prevent overflow
        rate = rod.plus2 - rod.plus;

    // With this, a +0 rod with no skill gets 1 mana per 25.0 turns

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        if (item_is_equipped( rod, true ))
            you.wield_change = true;
    }

    rod.plus += rate;

    if (in_inv && rod.plus == rod.plus2)
    {
        msg::stream << "Your " << rod.name(DESC_QUALNAME) << " has recharged."
                    << std::endl;
        if (is_resting())
            stop_running();
    }

    return;
}

void recharge_rods(long aut, bool level_only)
{
    if (!level_only)
    {
        for (int item = 0; item < ENDOFPACK; ++item)
        {
            _recharge_rod( you.inv[item], aut, true );
        }
    }

    for (int item = 0; item < MAX_ITEMS; ++item)
    {
        _recharge_rod( mitm[item], aut, false );
    }
}

void slime_wall_damage(actor* act, int delay)
{
    const int depth = player_in_branch(BRANCH_SLIME_PITS)
                      ? player_branch_depth()
                      : 1;

    int walls = 0;
    for (adjacent_iterator ai(act->pos()); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            walls++;

    if (!walls)
        return;

    // Up to 1d6 damage per wall per slot.
    const int strength = div_rand_round(depth * walls * BASELINE_DELAY, delay);

    if (act->atype() == ACT_PLAYER)
    {
        ASSERT(act == &you);
        splash_with_acid(strength, true,
                         (walls > 1) ? "The walls burn you!"
                                     : "The wall burns you!");
    }
    else
    {
        monsters* mon = act->as_monster();

        // Slime native monsters are immune to slime walls.
        if (mons_is_slime(mon))
            return;

         const int dam = resist_adjust_damage(mon, BEAM_ACID, mon->res_acid(),
                                              roll_dice(2, strength));
         if (dam > 0 && you.can_see(mon))
         {
             mprf((walls > 1) ? "The walls burn %s!" : "The wall burns %s!",
                  mon->name(DESC_NOCAP_THE).c_str());
         }
         corrode_monster(mon);
         mon->hurt(NULL, dam, BEAM_ACID);
    }
}

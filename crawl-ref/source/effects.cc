/*
 *  File:       effects.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "effects.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dgnevent.h"
#include "food.h"
#include "hiscores.h"
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
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells2.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

int holy_word_player(int pow, int caster, actor *attacker)
{
    if (!player_is_unholy())
        return 0;

    int hploss;

    // Holy word won't kill its user.
    if (attacker == &you)
        hploss = std::max(0, you.hp / 2 - 1);
    else
        hploss = roll_dice(2, 15) + (random2(pow) / 3);

    if (!hploss)
        return 0;

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

    return 1;
}

int holy_word_monsters(coord_def where, int pow, int caster,
                       actor *attacker)
{
    int retval = 0;

    // doubt this will ever happen, but it's here as a safety -- bwr
    pow = std::min(300, pow);

    // Is the player in this cell?
    if (where == you.pos())
        retval = holy_word_player(pow, caster, attacker);

    // Is a monster in this cell?
    const int mon = mgrd(where);

    if (mon == NON_MONSTER)
        return retval;

    monsters *monster = &menv[mon];

    if (!monster->alive() || !mons_is_unholy(monster))
        return retval;

    int hploss;

    // Holy word won't kill its user.
    if (attacker == monster)
        hploss = std::max(0, monster->hit_points / 2 - 1);
    else
        hploss = roll_dice(2, 15) + (random2(pow) / 3);

    if (hploss)
        simple_monster_message(monster, " convulses!");

    monster->hurt(attacker, hploss);

    if (hploss)
    {
        retval = 1;

        // Holy word won't annoy, slow, or frighten its user.
        if (monster->alive() && attacker != monster)
        {
            // Currently, holy word annoys the monsters it affects
            // because it can kill them, and because hostile monsters
            // don't use it.
            behaviour_event(monster, ME_ANNOY, MHITYOU);

            if (monster->speed_increment >= 25)
                monster->speed_increment -= 20;

            monster->add_ench(ENCH_FEAR);
        }
    }

    return retval;
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

    return apply_area_within_radius(holy_word_monsters, where, pow, 8, caster,
                                    attacker);
}

int torment_player(int pow, int caster)
{
    ASSERT(!crawl_state.arena);

    UNUSED(pow);

    // [dshaligram] Switched to using ouch() instead of dec_hp() so that
    // notes can also track torment and activities can be interrupted
    // correctly.
    int hploss = 0;

    if (!player_res_torment())
    {
        // Negative energy resistance can alleviate torment.
        hploss = std::max(0, you.hp * (50 - player_prot_life() * 5) / 100 - 1);
    }

    if (!hploss)
    {
        mpr("You feel a surge of unholy energy.");
        return 0;
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
            aux  = "Xom's torment";
            break;
        }
    }

    ouch(hploss, caster, type, aux);

    return 1;
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
    const int mon = mgrd(where);

    if (mon == NON_MONSTER)
        return retval;

    monsters *monster = &menv[mon];

    if (!monster->alive() || mons_res_negative_energy(monster) == 3)
        return retval;

    int hploss = std::max(0, monster->hit_points / 2 - 1);

    if (hploss)
        simple_monster_message(monster, " convulses!");

    // Currently, torment doesn't annoy the monsters it affects because
    // it can't kill them, and because hostile monsters use it.
    monster->hurt(NULL, hploss, BEAM_TORMENT_DAMAGE);

    if (hploss)
        retval = 1;

    return retval;
}

int torment(int caster, const coord_def& where)
{
    return apply_area_within_radius(torment_monsters, where, 0, 8, caster);
}

void immolation(int pow, int caster, coord_def where, bool known,
                actor *attacker)
{
    ASSERT(!crawl_state.arena);

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
    beam.type          = dchar_glyph(DCHAR_FIRED_BURST);
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

void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker)
{
    ASSERT(!crawl_state.arena);

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
    beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
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
    ASSERT(!crawl_state.arena);

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

    // No longer held in net.
    clear_trapping_net();

    down_stairs(you.your_level, gate_type, you.entry_cause);  // heh heh
}

bool forget_spell(void)
{
    ASSERT(!crawl_state.arena);

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

    del_spell_from_memory_by_slot( slot );

    return (true);
}                               // end forget_spell()

// use player::decrease_stats() instead iff:
// (a) player_sust_abil() should not factor in; and
// (b) there is no floor to the final stat values {dlb}
bool lose_stat(unsigned char which_stat, unsigned char stat_loss, bool force,
               const char *cause, bool see_source)
{
    bool statLowered = false;   // must initialize to false {dlb}
    char *ptr_stat = NULL;
    bool *ptr_redraw = NULL;
    char newValue = 0;          // holds new value, for comparison to old {dlb}

    kill_method_type kill_type = NUM_KILLBY;

    // begin outputing message: {dlb}
    std::string msg = "You feel ";

    // set pointers to appropriate variables: {dlb}
    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STRENGTH:
        msg       += "weakened";
        ptr_stat   = &you.strength;
        ptr_redraw = &you.redraw_strength;
        kill_type  = KILLED_BY_WEAKNESS;
        break;

    case STAT_DEXTERITY:
        msg       += "clumsy";
        ptr_stat   = &you.dex;
        ptr_redraw = &you.redraw_dexterity;
        kill_type  = KILLED_BY_CLUMSINESS;
        break;

    case STAT_INTELLIGENCE:
        msg       += "dopey";
        ptr_stat   = &you.intel;
        ptr_redraw = &you.redraw_intelligence;
        kill_type  = KILLED_BY_STUPIDITY;
        break;
    }

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    // newValue is current value less modifier: {dlb}
    newValue = *ptr_stat - stat_loss;

    // conceivable that stat was already *at* three
    // or stat_loss zeroed by player_sust_abil(): {dlb}
    //
    // Actually, that code was somewhat flawed.  Several race-class combos
    // can start with a stat lower than three, and this block (which
    // used to say '!=' would actually cause stat gain with the '< 3'
    // check that used to be above.  Crawl has stat-death code and I
    // don't see why we shouldn't be using it here.  -- bwr
    if (newValue < *ptr_stat)
    {
        *ptr_stat = newValue;
        *ptr_redraw = 1;

        // handle burden change, where appropriate: {dlb}
        if (ptr_stat == &you.strength)
            burden_change();

        statLowered = true;  // that is, stat was lowered (not just changed)
    }

    // a warning to player that s/he cut it close: {dlb}
    if (!statLowered)
        msg += " for a moment";

    // finish outputting message: {dlb}
    msg += ".";
    mpr(msg.c_str());

    if (newValue < 1)
    {
        if (cause == NULL)
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type);
        else
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
    }


    return (statLowered);
}                               // end lose_stat()

bool lose_stat(unsigned char which_stat, unsigned char stat_loss, bool force,
               const std::string cause, bool see_source)
{
    return lose_stat(which_stat, stat_loss, force, cause.c_str(), see_source);
}

bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const monsters* cause, bool force)
{
    if (cause == NULL || invalid_monster(cause))
        return lose_stat(which_stat, stat_loss, force, NULL, true);

    bool        vis  = mons_near(cause) && player_monster_visible(cause);
    std::string name = cause->name(DESC_NOCAP_A, true);

    if (cause->has_ench(ENCH_SHAPESHIFTER))
        name += " (shapeshifter)";
    else if (cause->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        name += " (glowing shapeshifter)";

    return lose_stat(which_stat, stat_loss, force, name, vis);
}

bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
               const item_def &cause, bool removed, bool force)
{
    std::string name = cause.name(DESC_NOCAP_THE, false, true, false, false,
                                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
    std::string verb;

    switch(cause.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (removed)
            verb = "removing";
        else
            verb = "wearing";
        break;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (removed)
            verb = "unwielding";
        else
            verb = "wielding";
        break;

    case OBJ_WANDS:   verb = "zapping";  break;
    case OBJ_FOOD:    verb = "eating";   break;
    case OBJ_SCROLLS: verb = "reading";  break;
    case OBJ_POTIONS: verb = "drinking"; break;
    default:          verb = "using";
    }

    return lose_stat(which_stat, stat_loss, force, verb + " " + name, true);
}

void direct_effect(monsters *source, spell_type spell,
                   bolt &pbolt, actor *defender)
{
    monsters *def =
        (defender->atype() == ACT_MONSTER) ? dynamic_cast<monsters*>(defender)
                                           : NULL;

    if (def)
    {
        // annoy the target
        behaviour_event(def, ME_ANNOY, monster_index(source));
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

    case SPELL_BRAIN_FEED:
        if (!def)
        {
            // lose_stat() must come last {dlb}
            if (one_chance_in(3)
                && lose_stat(STAT_INTELLIGENCE, 1, source))
            {
                mpr("Something feeds on your intellect!");
                xom_is_stimulated(50);
            }
            else
                mpr("Something tries to feed on your intellect!");
        }
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
    ASSERT(!crawl_state.arena);

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
        mpr("The scroll reassembles itself in your hands!");
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
        if (player_can_smell())
            mprf("You smell %s.", weird_smell().c_str());
        else if (you.species == SP_MUMMY)
            mpr("Your bandages flutter.");
        else // currently not ever used
            canned_msg(MSG_NOTHING_HAPPENS);
        break;

    case 4:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 5:
        temp_rand = random2(3);
        mprf("Your %s",
             (temp_rand == 0) ? "ears itch!" :
             (temp_rand == 1) ? "brain hurts!"
                              : "nose twitches suddenly!");
        break;

    case 6:
        mpr("You hear the tinkle of a tiny bell.", MSGCH_SOUND);
        cast_summon_butterflies(100);
        break;

    case 7:
        mprf(MSGCH_SOUND, "You hear %s.", weird_sound().c_str());
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

static int _find_acquirement_subtype(object_class_type class_wanted,
                                     int &quantity)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    int type_wanted = OBJ_RANDOM;
    int iteration = 0;

    const int max_has_value = 100;
    FixedVector< int, max_has_value > already_has;

    skill_type best_spell = SK_NONE;
    skill_type best_any   = SK_NONE;

    already_has.init(0);

    int spell_skills = 0;
    for (int i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
        spell_skills += you.skills[i];

    for (int acqc = 0; acqc < ENDOFPACK; acqc++)
    {
        if (is_valid_item( you.inv[acqc] )
            && you.inv[acqc].base_type == class_wanted)
        {
            ASSERT( you.inv[acqc].sub_type < max_has_value );
            already_has[you.inv[acqc].sub_type] += you.inv[acqc].quantity;
        }
    }

    if (class_wanted == OBJ_FOOD)
    {
        // food is a little less predictable now -- bwr

        if (you.species == SP_GHOUL)
        {
            type_wanted = one_chance_in(10) ? FOOD_ROYAL_JELLY
                                            : FOOD_CHUNK;
        }
        else if (you.species == SP_VAMPIRE)
        {
            // Vampires really don't want any OBJ_FOOD but OBJ_CORPSES
            // but it's easier to just give them a potion of blood
            // class type is set elsewhere
            type_wanted = POT_BLOOD;
            quantity = 2 + random2(4);
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

        // giving more of the lower food value items
        if (type_wanted == FOOD_HONEYCOMB || type_wanted == FOOD_CHUNK)
        {
            quantity += random2avg(10, 2);
        }
    }
    else if (class_wanted == OBJ_WEAPONS)
    {
        // Now asking for a weapon is biased towards your skills,
        // although launchers are right out for now. -- bwr
        int count = 0;
        int skill = SK_FIGHTING;

        for (int i = SK_SHORT_BLADES; i <= SK_DARTS; i++)
        {
            if (i == SK_UNUSED_1)
                continue;

            // Adding a small constant allows for the occasional
            // weapon in an untrained skill.

            const int weight = you.skills[i] + 1;
            count += weight;

            if (x_chance_in_y(weight, count))
                skill = i;
        }

        count = 0;

        item_def item_considered;
        item_considered.base_type = OBJ_WEAPONS;
        for (int i = 0; i < NUM_WEAPONS; ++i)
        {
            item_considered.sub_type = i;

            const int acqweight = property(item_considered, PWPN_ACQ_WEIGHT);

            if (!acqweight)
                continue;

            int wskill = range_skill(OBJ_WEAPONS, i);
            if (wskill == SK_THROWING)
                wskill = weapon_skill(OBJ_WEAPONS, i);

            if (wskill == skill && x_chance_in_y(acqweight, count += acqweight))
                type_wanted = i;
        }
    }
    else if (class_wanted == OBJ_MISSILES)
    {
        int count = 0;
        int skill = SK_THROWING;

        for (int i = SK_SLINGS; i <= SK_DARTS; i++)
        {
            if (you.skills[i])
            {
                count += you.skills[i];
                if (x_chance_in_y(you.skills[i], count))
                    skill = i;
            }
        }

        switch (skill)
        {
        case SK_SLINGS:
            type_wanted = MI_STONE;
            break;

        case SK_BOWS:
            type_wanted = MI_ARROW;
            break;

        case SK_CROSSBOWS:
            type_wanted = MI_DART;
            for (int i = 0; i < ENDOFPACK; i++)
            {
                // Assuming that crossbow in inventory means that they
                // want bolts for it (not darts for a hand crossbow)...
                // perhaps we should check for both and compare ammo
                // amounts on hand?
                if (is_valid_item( you.inv[i] )
                    && you.inv[i].base_type == OBJ_WEAPONS
                    && you.inv[i].sub_type == WPN_CROSSBOW)
                {
                    type_wanted = MI_BOLT;
                    break;
                }
            }
            break;

        case SK_DARTS:
            type_wanted = MI_DART;
            for (int i = 0; i < ENDOFPACK; i++)
            {
                if (is_valid_item( you.inv[i] )
                    && you.inv[i].base_type == OBJ_WEAPONS
                    && you.inv[i].sub_type == WPN_BLOWGUN)
                {
                    // Assuming that blowgun in inventory means that they
                    // may want needles for it (but darts might also be
                    // wanted).  Maybe expand this... see above comment.
                    if (coinflip())
                        type_wanted = MI_NEEDLE;
                    break;
                }
            }
            break;

        default:
            type_wanted = MI_DART;
            break;
        }
    }
    else if (class_wanted == OBJ_ARMOUR)
    {
        // Increasing the representation of the non-body armour
        // slots here to make up for the fact that there's one
        // one type of item for most of them. -- bwr
        //
        // OBJ_RANDOM is body armour and handled below
        type_wanted = (coinflip())? OBJ_RANDOM :
            static_cast<int>(_random_nonbody_armour_type());

        // Some species specific fitting problems.
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
            if (type_wanted == ARM_GLOVES || type_wanted == ARM_BOOTS
                || type_wanted == ARM_CENTAUR_BARDING
                || type_wanted == ARM_NAGA_BARDING)
            {
                type_wanted = ARM_ROBE;  // no heavy armour
            }
            else if (type_wanted == ARM_SHIELD)
            {
                if (you.species == SP_SPRIGGAN)
                    type_wanted = ARM_BUCKLER;
                else if (coinflip()) // giant races: 50/50 shield/large shield
                    type_wanted = ARM_LARGE_SHIELD;
            }
            else if (type_wanted == OBJ_RANDOM)
            {
                type_wanted = ARM_ROBE;  // no heavy armour, see below
            }
            break;

        case SP_NAGA:
            if (type_wanted == ARM_BOOTS || type_wanted == ARM_CENTAUR_BARDING)
                type_wanted = ARM_NAGA_BARDING;
            break;

        case SP_CENTAUR:
            if (type_wanted == ARM_BOOTS || type_wanted == ARM_NAGA_BARDING)
                type_wanted = ARM_CENTAUR_BARDING;
            break;

        default:
            if (type_wanted == ARM_CENTAUR_BARDING
                || type_wanted == ARM_NAGA_BARDING)
            {
                type_wanted = ARM_BOOTS;
            }
            break;
        }

        // Mutation specific problems (horns allow caps).
        if (type_wanted == ARM_BOOTS && !player_has_feet()
            || type_wanted == ARM_GLOVES && you.has_claws(false) >= 3)
        {
            type_wanted = OBJ_RANDOM;
        }

        // Do this here, before acquirement()'s call to can_wear_armour(),
        // so that caps will be just as common as helmets for those
        // that can't wear helmets.
        // We check for the mutation directly to avoid acquirement fiddles
        // with vampires.
        if (type_wanted == ARM_HELMET
            && (!you_can_wear(EQ_HELMET) || you.mutation[MUT_HORNS]))
        {
            type_wanted = coinflip()? ARM_CAP : ARM_WIZARD_HAT;
        }

        // Now we'll randomly pick a body armour (light only in the
        // case of ARM_ROBE).  Unlike before, now we're only giving
        // out the finished products here, never the hides.  -- bwr
        if (type_wanted == OBJ_RANDOM || type_wanted == ARM_ROBE)
        {
            // start with normal base armour
            if (type_wanted == ARM_ROBE)
                type_wanted = coinflip() ? ARM_ROBE : ARM_ANIMAL_SKIN;
            else
            {
                type_wanted = ARM_ROBE + random2(8);

                if (one_chance_in(10) && you.skills[SK_ARMOUR] >= 10)
                    type_wanted = ARM_CRYSTAL_PLATE_MAIL;

                if (one_chance_in(10))
                    type_wanted = ARM_ANIMAL_SKIN;
            }

            // everyone can wear things made from hides
            if (one_chance_in(20))
            {
                int rnd = random2(20);

                type_wanted = (rnd <  4) ? ARM_TROLL_LEATHER_ARMOUR  :  // 20%
                              (rnd <  8) ? ARM_STEAM_DRAGON_ARMOUR   :  // 20%
                              (rnd < 11) ? ARM_MOTTLED_DRAGON_ARMOUR :  // 15%
                              (rnd < 14) ? ARM_SWAMP_DRAGON_ARMOUR   :  // 15%
                              (rnd < 16) ? ARM_DRAGON_ARMOUR         :  // 10%
                              (rnd < 18) ? ARM_ICE_DRAGON_ARMOUR     :  // 10%
                              (rnd < 19) ? ARM_STORM_DRAGON_ARMOUR      //  5%
                                         : ARM_GOLD_DRAGON_ARMOUR;      //  5%
            }
        }
    }
    else if (class_wanted != OBJ_GOLD)
    {
        do
        {
            unsigned char i;

            switch (class_wanted)
            {
            case OBJ_JEWELLERY:
                // Try for a base type the player hasn't identified
                for (i = 0; i < 10; i++)
                {
                    type_wanted = random2(24);

                    if (one_chance_in(3))
                        type_wanted = AMU_RAGE + random2(10);

                    if (get_ident_type(OBJ_JEWELLERY, type_wanted) ==
                        ID_UNKNOWN_TYPE)
                    {
                        break;
                    }
                }
                break;

            case OBJ_BOOKS:
                // Remember, put rarer books higher in the list.
                iteration = 1;
                type_wanted = NUM_BOOKS;

                best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS - 1),
                                         best_spell );

              which_book:
#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "acquirement: iteration = %d, best_spell = %d",
                     iteration, best_spell );
#endif

                switch (best_spell)
                {
                default:
                case SK_SPELLCASTING:
                    if (you.skills[SK_SPELLCASTING] <= 3
                        && !you.had_book[BOOK_CANTRIPS])
                    {
                        // Handful of level one spells, very useful for the
                        // new spellcaster who's asking for a book -- bwr
                        type_wanted = BOOK_CANTRIPS;
                    }
                    else if (!you.had_book[BOOK_MINOR_MAGIC_I])
                        type_wanted = BOOK_MINOR_MAGIC_I + random2(3);
                    else if (!you.had_book[BOOK_WIZARDRY])
                        type_wanted = BOOK_WIZARDRY;
                    else if (!you.had_book[BOOK_CONTROL])
                        type_wanted = BOOK_CONTROL;
                    else if (!you.had_book[BOOK_POWER])
                        type_wanted = BOOK_POWER;
                    break;

                case SK_POISON_MAGIC:
                    if (!you.had_book[BOOK_YOUNG_POISONERS])
                        type_wanted = BOOK_YOUNG_POISONERS;
                    else if (!you.had_book[BOOK_ENVENOMATIONS])
                        type_wanted = BOOK_ENVENOMATIONS;
                    break;

                case SK_EARTH_MAGIC:
                    if (!you.had_book[BOOK_GEOMANCY])
                        type_wanted = BOOK_GEOMANCY;
                    else if (!you.had_book[BOOK_EARTH])
                        type_wanted = BOOK_EARTH;
                    break;

                case SK_AIR_MAGIC:
                    // removed the book of clouds... all the other elements
                    // (and most other spell skills) only get two.
                    if (!you.had_book[BOOK_AIR])
                        type_wanted = BOOK_AIR;
                    else if (!you.had_book[BOOK_SKY])
                        type_wanted = BOOK_SKY;
                    break;

                case SK_ICE_MAGIC:
                    if (!you.had_book[BOOK_FROST])
                        type_wanted = BOOK_FROST;
                    else if (!you.had_book[BOOK_ICE])
                        type_wanted = BOOK_ICE;
                    break;

                case SK_FIRE_MAGIC:
                    if (!you.had_book[BOOK_FLAMES])
                        type_wanted = BOOK_FLAMES;
                    else if (!you.had_book[BOOK_FIRE])
                        type_wanted = BOOK_FIRE;
                    break;

                case SK_SUMMONINGS:
                    if (!you.had_book[BOOK_CALLINGS])
                        type_wanted = BOOK_CALLINGS;
                    else if (!you.had_book[BOOK_SUMMONINGS])
                        type_wanted = BOOK_SUMMONINGS;

                    // now a Vehumet special -- bwr
                    // else if (!you.had_book[BOOK_DEMONOLOGY])
                    //     type_wanted = BOOK_DEMONOLOGY;
                    break;

                case SK_ENCHANTMENTS:
                    best_any = best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);

                    // So many enchantment books!  I really can't feel
                    // guilty at all for dividing out the fighting
                    // books and forcing the player to raise a fighting
                    // skill (or enchantments in the case of Crusaders)
                    // to get the remaining books... enchantments are
                    // much too good (most spells, lots of books here,
                    // id wand charges, gives magic resistance),
                    // something will eventually have to be done.  -- bwr
                    if (best_any >= SK_FIGHTING && best_any <= SK_STAVES)
                    {
                        // Fighter mages get the fighting enchantment books
                        if (!you.had_book[BOOK_WAR_CHANTS])
                            type_wanted = BOOK_WAR_CHANTS;
                        else if (!you.had_book[BOOK_TUKIMA])
                            type_wanted = BOOK_TUKIMA;
                    }
                    else if (!you.had_book[BOOK_CHARMS])
                        type_wanted = BOOK_CHARMS;
                    else if (!you.had_book[BOOK_HINDERANCE])
                        type_wanted = BOOK_HINDERANCE;
                    else if (!you.had_book[BOOK_ENCHANTMENTS])
                        type_wanted = BOOK_ENCHANTMENTS;
                    break;

                case SK_CONJURATIONS:
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        type_wanted = give_first_conjuration_book();
                    else if (!you.had_book[BOOK_TEMPESTS])
                        type_wanted = BOOK_TEMPESTS;

                    // now a Vehumet special -- bwr
                    // else if (!you.had_book[BOOK_ANNIHILATIONS])
                    //     type_wanted = BOOK_ANNIHILATIONS;
                    break;

                case SK_NECROMANCY:
                    if (!you.had_book[BOOK_NECROMANCY])
                        type_wanted = BOOK_NECROMANCY;
                    else if (!you.had_book[BOOK_DEATH])
                        type_wanted = BOOK_DEATH;
                    else if (!you.had_book[BOOK_UNLIFE])
                        type_wanted = BOOK_UNLIFE;

                    // now a Kikubaaqudgha special -- bwr
                    // else if (!you.had_book[BOOK_NECRONOMICON])
                    //    type_wanted = BOOK_NECRONOMICON;
                    break;

                case SK_TRANSLOCATIONS:
                    if (!you.had_book[BOOK_SPATIAL_TRANSLOCATIONS])
                        type_wanted = BOOK_SPATIAL_TRANSLOCATIONS;
                    else if (!you.had_book[BOOK_WARP])
                        type_wanted = BOOK_WARP;
                    break;

                case SK_TRANSMUTATION:
                    if (!you.had_book[BOOK_CHANGES])
                        type_wanted = BOOK_CHANGES;
                    else if (!you.had_book[BOOK_TRANSFIGURATIONS])
                        type_wanted = BOOK_TRANSFIGURATIONS;
                    else if (!you.had_book[BOOK_MUTATIONS])
                        type_wanted = BOOK_MUTATIONS;
                    break;

                case SK_DIVINATIONS:    //jmf: added 24mar2000
                    if (!you.had_book[BOOK_SURVEYANCES])
                        type_wanted = BOOK_SURVEYANCES;
                    else if (!you.had_book[BOOK_DIVINATIONS])
                        type_wanted = BOOK_DIVINATIONS;
                    break;
                }
/*
                if (type_wanted == 99 && glof == best_skill(SK_SPELLCASTING, (NUM_SKILLS - 1), 99))
*/
                if (type_wanted == NUM_BOOKS && iteration == 1)
                {
                    best_spell = best_skill( SK_SPELLCASTING, NUM_SKILLS - 1,
                                             best_skill(SK_SPELLCASTING,
                                                        NUM_SKILLS - 1, 99) );
                    iteration++;
                    goto which_book;
                }

                // If we don't have a book, try and get a new one.
                if (type_wanted > MAX_FIXED_BOOK)
                {
                    do
                    {
                        type_wanted = random2(NUM_NORMAL_BOOKS);
                        if (one_chance_in(500))
                            break;
                    }
                    while (you.had_book[type_wanted]);
                }

                // If the book is invalid find any valid one.
                while (type_wanted == BOOK_HEALING)
                    type_wanted = random2(NUM_NORMAL_BOOKS);
                break;

            case OBJ_STAVES:
                type_wanted = random2(13);

                if (type_wanted >= 10)
                    type_wanted = STAFF_AIR + type_wanted - 10;

                // Elemental preferences -- bwr
                if (type_wanted == STAFF_FIRE || type_wanted == STAFF_COLD)
                {
                    if (you.skills[SK_FIRE_MAGIC] > you.skills[SK_ICE_MAGIC])
                        type_wanted = STAFF_FIRE;
                    else if (you.skills[SK_FIRE_MAGIC] != you.skills[SK_ICE_MAGIC])
                        type_wanted = STAFF_COLD;
                }
                else if (type_wanted == STAFF_AIR || type_wanted == STAFF_EARTH)
                {
                    if (you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
                        type_wanted = STAFF_AIR;
                    else if (you.skills[SK_AIR_MAGIC] != you.skills[SK_EARTH_MAGIC])
                        type_wanted = STAFF_EARTH;
                }

                best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS-1), 99 );

                // If we're going to give out an enhancer staff,
                // we should at least bias things towards the
                // best spell skill. -- bwr
                switch (best_spell)
                {
                case SK_FIRE_MAGIC:
                    if (!already_has[STAFF_FIRE])
                        type_wanted = STAFF_FIRE;
                    break;

                case SK_ICE_MAGIC:
                    if (!already_has[STAFF_COLD])
                        type_wanted = STAFF_COLD;
                    break;

                case SK_AIR_MAGIC:
                    if (!already_has[STAFF_AIR])
                        type_wanted = STAFF_AIR;
                    break;

                case SK_EARTH_MAGIC:
                    if (!already_has[STAFF_EARTH])
                        type_wanted = STAFF_EARTH;
                    break;

                case SK_POISON_MAGIC:
                    if (!already_has[STAFF_POISON])
                        type_wanted = STAFF_POISON;
                    break;

                case SK_NECROMANCY:
                    if (!already_has[STAFF_DEATH])
                        type_wanted = STAFF_DEATH;
                    break;

                case SK_CONJURATIONS:
                    if (!already_has[STAFF_CONJURATION])
                        type_wanted = STAFF_CONJURATION;
                    break;

                case SK_ENCHANTMENTS:
                    if (!already_has[STAFF_ENCHANTMENT])
                        type_wanted = STAFF_ENCHANTMENT;
                    break;

                case SK_SUMMONINGS:
                    if (!already_has[STAFF_SUMMONING])
                        type_wanted = STAFF_SUMMONING;
                    break;

                case SK_EVOCATIONS:
                    if (!one_chance_in(4))
                        type_wanted = random_rod_subtype();
                    break;

                default: // Invocations and leftover spell schools.
                    switch (random2(5))
                    {
                    case 0:
                        type_wanted = STAFF_WIZARDRY;
                        break;

                    case 1:
                        type_wanted = STAFF_POWER;
                        break;

                    case 2:
                        type_wanted = STAFF_ENERGY;
                        break;

                    case 3:
                        type_wanted = STAFF_CHANNELING;
                        break;

                    case 4:
                        break;
                    }
                    break;
                }

                // Increased chance of getting a rod for new or
                // non-spellcasters.  -- bwr
                if (one_chance_in(20)
                    || (spell_skills <= 1               // short on spells
                        && type_wanted < STAFF_SMITING
                        && !one_chance_in(4)))
                {
                    type_wanted = coinflip() ? STAFF_STRIKING :
                        random_rod_subtype();
                }
                break;

            case OBJ_MISCELLANY:
                do
                {
                    type_wanted = random2(NUM_MISCELLANY);
                }
                while (type_wanted == MISC_HORN_OF_GERYON
                       || type_wanted == MISC_RUNE_OF_ZOT
                       || type_wanted == MISC_CRYSTAL_BALL_OF_FIXATION
                       || type_wanted == MISC_EMPTY_EBONY_CASKET
                       || type_wanted == MISC_DECK_OF_PUNISHMENT);
                break;
            default:
                break;
            }

            ASSERT( type_wanted < max_has_value );
        }
        while (already_has[type_wanted] && !one_chance_in(200));
    }

    return (type_wanted);
}

static void _do_book_acquirement(item_def &book, int agent)
{
    // items() shouldn't make book a randart for acquirement items.
    ASSERT(!is_random_artefact(book));

    // Non-normal books (i.e. manuals/book of destruction) are rare enough
    // without turning them into randart books.
    if (book.sub_type > MAX_NORMAL_BOOK)
        return;

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

    int choice = random_choose_weighted(
            55, BOOK_RANDART_THEME,
            24, book.sub_type,
            level == -1      ? 0 :
       agent == GOD_SIF_MUNA ? 6 : 3, BOOK_RANDART_LEVEL,
       agent == GOD_XOM      ? 0 : 6, BOOK_MANUAL, // too useful for Xom
            0);

    // No changes.
    if (choice == book.sub_type)
        return;

    book.sub_type = choice;

    // Acquired randart books have a chance of being named after the player.
    std::string owner = "";
    if (agent == AQ_SCROLL && one_chance_in(10)
        || agent == AQ_CARD_GENIE && one_chance_in(5))
    {
        owner = you.your_name;
    }

    switch (choice)
    {
    case BOOK_RANDART_THEME:
        make_book_theme_randart(book, 0, 0, 7, 22, SPELL_NO_SPELL, owner);
        break;

    case BOOK_RANDART_LEVEL:
    {
        int num_spells = 7 - (level + 1) / 2 + random_range(1, 2);
        make_book_level_randart(book, level, num_spells, owner);
        break;
    }

    // Spell discipline manual
    case BOOK_MANUAL:
    {
        int weights[SK_POISON_MAGIC - SK_CONJURATIONS + 1];
        int total_weights = 0;

        for (int i = SK_CONJURATIONS; i <= SK_POISON_MAGIC; i++)
        {
            int w = std::max(0, 24 - you.skills[i])
                * 100 / species_skills(i, you.species);

            weights[i - SK_CONJURATIONS] = w;
            total_weights += w;
        }

        // Are we too skilled to get any manuals?
        if (total_weights == 0)
        {
            _do_book_acquirement(book, agent);
            return;
        }

        int skill = choose_random_weighted(weights,
                                           weights + ARRAYSZ(weights));
        skill += SK_CONJURATIONS;

        book.sub_type = BOOK_MANUAL;
        book.plus = skill;
        // Set number of reads possible before it "crumbles to dust".
        book.plus2 = 3 + random2(15);
        break;
    }

    default:
        ASSERT(false);
    }
}

bool acquirement(object_class_type class_wanted, int agent,
                 bool quiet, int* item_index)
{
    ASSERT(!crawl_state.arena);

    int thing_created = NON_ITEM;

    if (item_index == NULL)
        item_index = &thing_created;

    *item_index = NON_ITEM;

    while (class_wanted == OBJ_RANDOM)
    {
        ASSERT(!quiet);
        mesclr();
        mpr( "[a] Weapon  [b] Armour  [c] Jewellery      [d] Book" );
        mpr( "[e] Staff   [f] Food    [g] Miscellaneous  [h] Gold" );
        mpr("What kind of item would you like to acquire? ", MSGCH_PROMPT);

        const int keyin = tolower( get_ch() );
        switch (keyin)
        {
        case 'a': class_wanted = OBJ_WEAPONS;    break;
        case 'b': class_wanted = OBJ_ARMOUR;     break;
        case 'c': class_wanted = OBJ_JEWELLERY;  break;
        case 'd': class_wanted = OBJ_BOOKS;      break;
        case 'e': class_wanted = OBJ_STAVES;     break;
        case 'f': class_wanted = OBJ_FOOD;       break;
        case 'g': class_wanted = OBJ_MISCELLANY; break;
        case 'h': class_wanted = OBJ_GOLD;       break;
        default:
            // Lets wizards escape out of accidently choosing acquirement.
            if (agent == AQ_WIZMODE)
            {
                canned_msg(MSG_OK);
                return (false);
            }
            break;
        }
    }

    if (grid_destroys_items(grd(you.pos())))
    {
        // How sad (and stupid).
        if (!silenced(you.pos()) && !quiet)
            mprf(MSGCH_SOUND, grid_item_destruction_message(grd(you.pos())));

        if (agent > GOD_NO_GOD && agent < NUM_GODS)
        {
            if (agent == GOD_XOM)
                simple_god_message(" snickers.", GOD_XOM);
            else
            {
                ASSERT(!"God gave gift item while player was on grid which "
                        "destroys items.");
                mprf(MSGCH_ERROR, "%s gave a god gift while you were on "
                                  "terrain which destroys items.",
                     god_name((god_type) agent).c_str());
            }
        }

        *item_index = NON_ITEM;
    }
    else
    {
        int quant = 1;
        for (int item_tries = 0; item_tries < 40; item_tries++)
        {
            int type_wanted = _find_acquirement_subtype(class_wanted, quant);

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
            if (doodad.base_type == OBJ_WEAPONS
                   && !can_wield(&doodad, false, true)
                || doodad.base_type == OBJ_ARMOUR
                   && !can_wear_armour(doodad, false, true))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }

            // Only TSO gifts blessed blades, and currently not through
            // acquirement, but make sure of this anyway.
            if (agent != GOD_SHINING_ONE && is_blessed_blade(doodad))
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
                    || is_fixed_artefact(doodad)
                       && (doodad.special == SPWPN_WRATH_OF_TROG
                           || doodad.special == SPWPN_STAFF_OF_WUCAD_MU))
                {
                    destroy_item(thing_created, true);
                    thing_created = NON_ITEM;
                    continue;
                }
            }

            // MT - Check: god-gifted weapons and armour shouldn't kill you.
            // Except Xom.
            if ((agent == GOD_TROG || agent == GOD_OKAWARU)
                && is_random_artefact(doodad))
            {
                randart_properties_t  proprt;
                randart_wpn_properties( doodad, proprt );

                // Check vs. stats. positive stats will automatically fall
                // through.  As will negative stats that won't kill you.
                if (-proprt[RAP_STRENGTH] >= you.strength
                    || -proprt[RAP_INTELLIGENCE] >= you.intel
                    || -proprt[RAP_DEXTERITY] >= you.dex)
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
        {
            if (!quiet)
                mpr("The demon of the infinite void smiles upon you.");
            *item_index = NON_ITEM;
            return (false);
        }

        // Easier to read this way.
        item_def& thing(mitm[thing_created]);

        // Give some more gold.
        if (class_wanted == OBJ_GOLD)
            thing.quantity += 150;
        else if (quant > 1)
            thing.quantity = quant;

        if (is_blood_potion(thing))
            init_stack_blood_potions(thing);

        // Remove curse flag from item.
        do_uncurse_item(thing);

        if (thing.base_type == OBJ_BOOKS)
        {
            _do_book_acquirement(thing, agent);
            mark_had_book(thing);
        }
        else if (thing.base_type == OBJ_JEWELLERY)
        {
            switch (thing.sub_type)
            {
            case RING_SLAYING:
                // Make sure plus to damage is >= 1.
                thing.plus2 = abs( thing.plus2 );
                if (thing.plus2 == 0)
                    thing.plus2 = 1;
                // fall through...

            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
                // Make sure plus is >= 1.
                thing.plus = abs( thing.plus );
                if (thing.plus == 0)
                    thing.plus = 1;
                break;

            case RING_HUNGER:
            case AMU_INACCURACY:
                // These are the only truly bad pieces of jewellery.
                if (!one_chance_in(9))
                    make_item_randart( thing );
                break;

            default:
                break;
            }
        }
        else if (thing.base_type == OBJ_WEAPONS
                 && !is_fixed_artefact(thing)
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
                    int brand = get_weapon_brand( thing );
                    if (brand == SPWPN_HOLY_WRATH)
                    {
                        if (!is_random_artefact( thing ))
                        {
                            set_item_ego_type( thing,
                                               OBJ_WEAPONS, SPWPN_VORPAL );
                        }
                        else
                        {
                            // Keep resetting seed until it's good.
                            for (; brand == SPWPN_HOLY_WRATH;
                                  brand = get_weapon_brand(thing))
                            {
                                make_item_randart( thing );
                            }
                        }
                    }
                }
                break;

            case SP_HALFLING:
            case SP_GNOME:
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

            int plusmod = random2(4);
            if (agent == GOD_TROG)
            {
                // More damage, less accuracy.
                thing.plus  -= plusmod;
                thing.plus2 += plusmod;
                if (!is_random_artefact(thing))
                    thing.plus = std::max(static_cast<int>(thing.plus), 0);
            }
            else if (agent == GOD_OKAWARU)
            {
                // More accuracy, less damage.
                thing.plus  += plusmod;
                thing.plus2 -= plusmod;
                if (!is_random_artefact(thing))
                    thing.plus2 = std::max(static_cast<int>(thing.plus2), 0);
            }
        }

        if (agent > GOD_NO_GOD && agent < NUM_GODS && agent == you.religion)
        {
            thing.inscription = "god gift";
            if (is_random_artefact(thing))
            {
                if (!is_unrandom_artefact(thing)
                    && thing.base_type != OBJ_BOOKS)
                {
                    // Give another name that takes god gift into account;
                    // artefact books already do that.
                    thing.props["randart_name"].get_string() =
                        artefact_name(thing, false);
                }
            }
        }
        move_item_to_grid( &thing_created, you.pos() );

        // This should never actually be NON_ITEM because of the way
        // move_item_to_grid works (doesn't create a new item ever),
        // but we're checking it anyways. -- bwr
        if (thing_created != NON_ITEM)
        {
            if (!quiet)
                canned_msg(MSG_SOMETHING_APPEARS);
        }
        *item_index = thing_created;
    }

    // Well, the item may have fallen in the drink, but the intent is
    // that acquirement happened. -- bwr
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
        if (wand.base_type == OBJ_WEAPONS
            && get_weapon_brand(wand) == SPWPN_ELECTROCUTION)
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

        if (wand.base_type != OBJ_WANDS && !item_is_rod(wand))
            return (false);

        int charge_gain = 0;
        if (wand.base_type == OBJ_WANDS)
        {
            charge_gain = wand_charge_value(wand.sub_type);

            // Reinitialize zap counts.
            wand.plus2 = ZAPCOUNT_RECHARGED;

            const int new_charges =
                std::max<int>(
                    wand.plus,
                    std::min(charge_gain * 3,
                             wand.plus +
                             1 + random2avg( ((charge_gain - 1) * 3) + 1, 3 )));

            const bool charged = (new_charges > wand.plus);

            std::string desc;
            if (charged && item_ident(wand, ISFLAG_KNOW_PLUSES))
            {
                snprintf(info, INFO_SIZE, " and now has %d charges", new_charges);
                desc = info;
            }
            mprf("%s %s for a moment%s.",
                 wand.name(DESC_CAP_YOUR).c_str(),
                 charged? "glows" : "flickers",
                 desc.c_str());

            wand.plus = new_charges;

            if (!charged)
                wand.plus2 = ZAPCOUNT_MAX_CHARGED;
        }
        else // It's a rod.
        {
            bool work = false;

            if (wand.plus2 < MAX_ROD_CHARGE * ROD_CHARGE_MULT)
            {
                wand.plus2 += ROD_CHARGE_MULT;

                if (wand.plus2 > MAX_ROD_CHARGE * ROD_CHARGE_MULT)
                    wand.plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;

                work = true;
            }

            if (wand.plus < wand.plus2)
            {
                wand.plus = wand.plus2;
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

// Sets foe target of friendly monsters.
// If allow_patrol is true, patrolling monsters get MHITNOT instead.
static void _set_friendly_foes(bool allow_patrol = false)
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *mon(&menv[i]);
        if (!mon->alive() || !mons_near(mon) || !mons_friendly_real(mon))
            continue;

        mon->foe = (allow_patrol && mon->is_patrolling() ? MHITNOT
                                                         : you.pet_target);
    }
}

static void _set_allies_patrol_point(bool clear = false)
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *mon(&menv[i]);
        if (!mon->alive() || !mons_near(mon) || !mons_friendly_real(mon))
            continue;

        mon->patrol_point = (clear ? coord_def(0, 0) : mon->pos());

        if (!clear)
            mon->behaviour = BEH_WANDER;
    }
}

void yell(bool force)
{
    ASSERT(!crawl_state.arena);

    bool targ_prev = false;
    int mons_targd = MHITNOT;
    struct dist targ;

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

    if (!you.duration[DUR_BERSERKER])
    {
        std::string previous;
        if (!(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU))
        {
            monsters *target = &menv[you.prev_targ];
            if (target->alive() && mons_near(target)
                && player_monster_visible(target))
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
         one_chance_in(20)? " (and be thought a fool)" : "");

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
        if (you.duration[DUR_BERSERKER])
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
        if (you.duration[DUR_BERSERKER])
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

        mpr("Gang up on whom?", MSGCH_PROMPT);
        direction( targ, DIR_TARGET, TARG_ENEMY, -1, false, false );

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        if (!targ.isValid || mgrd(targ.target) == NON_MONSTER
            || !player_monster_visible(&env.mons[mgrd(targ.target)]))
        {
            mpr("Yeah, whatever.");
            return;
        }

        mons_targd = mgrd(targ.target);
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
}                               // end yell()

bool forget_inventory(bool quiet)
{
    ASSERT(!crawl_state.arena);

    int items_forgotten = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item(you.inv[i]);
        if (!is_valid_item(item) || item_is_equipped(item))
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
    for ( radius_iterator ri(you.pos(), radius, false, false); ri; ++ri )
    {
        const dungeon_feature_type grid = grd(*ri);

        if (grid == DNGN_ROCK_WALL
            || grid == DNGN_STONE_WALL
            || grid == DNGN_PERMAROCK_WALL )
        {
            grd(*ri)
                = static_cast<dungeon_feature_type>(grid + clear_plus);
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
    monster_type which_beastie = MONS_PROGRAM_BUG;

    mpr((temp_rand ==  0) ? "\"You will not leave this place.\"" :
        (temp_rand ==  1) ? "\"Die, mortal!\"" :
        (temp_rand ==  2) ? "\"We do not forgive those who trespass against us!\"" :
        (temp_rand ==  3) ? "\"Trespassers are not welcome here!\"" :
        (temp_rand ==  4) ? "\"You do not belong in this place!\"" :
        (temp_rand ==  5) ? "\"Leave now, before it is too late!\"" :
        (temp_rand ==  6) ? "\"We have you now!\"" :
        // plain messages
        (temp_rand ==  7) ? (player_can_smell()) ? "You smell brimstone."
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
                mgen_data::hostile_at(which_beastie,
                    you.pos(), 0, 0, true));
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
        create_monster(mg);

        for (int i = 0; i < 4; ++i)
            if (one_chance_in(3))
                create_monster(mg);
    }
}

static bool _is_floor(const dungeon_feature_type feat)
{
    return (!grid_is_solid(feat) && !grid_destroys_items(feat));
}

// This function checks whether we can turn a wall into a floor space and
// still keep a corridor-like environment. The wall in position x is a
// a candidate for switching if it's flanked by floor grids to two sides
// and by walls (any type) to the remaining cardinal directions.
//
//   .        #          2
//  #x#  or  .x.   ->   0x1
//   .        #          3
static bool _grid_is_flanked_by_walls(const coord_def &p)
{
    const coord_def adjs[] = { coord_def(p.x-1,p.y),
                               coord_def(p.x+1,p.y),
                               coord_def(p.x  ,p.y-1),
                               coord_def(p.x  ,p.y+1) };

    // paranoia!
    for (unsigned int i = 0; i < ARRAYSZ(adjs); ++i)
        if (!in_bounds(adjs[i]))
            return (false);

    return (grid_is_wall(grd(adjs[0])) && grid_is_wall(grd(adjs[1]))
               && _is_floor(grd(adjs[2])) && _is_floor(grd(adjs[3]))
            || _is_floor(grd(adjs[0])) && _is_floor(grd(adjs[1]))
               && grid_is_wall(grd(adjs[2])) && grid_is_wall(grd(adjs[3])));
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
//   c.d
//   a.b   -> if (a, b == walls) then (c, d == walls) or return (false)
//   #X#
//    .
static bool _deadend_check_wall(const coord_def &p)
{
    if (grid_is_wall(grd[p.x-1][p.y]))
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x-1, p.y+i);
            const coord_def b(p.x+1, p.y+i);
            const coord_def c(p.x-1, p.y+2*i);
            const coord_def d(p.x+1, p.y+2*i);

            if (in_bounds(a) && in_bounds(b)
                && grid_is_wall(grd(a)) && grid_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !grid_is_wall(grd(c)) || !grid_is_wall(grd(d))))
            {
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

            const coord_def a(p.x+i  , p.y-1);
            const coord_def b(p.x+i  , p.y+1);
            const coord_def c(p.x+2*i, p.y-1);
            const coord_def d(p.x+2*i, p.y+1);

            if (in_bounds(a) && in_bounds(b)
                && grid_is_wall(grd(a)) && grid_is_wall(grd(b))
                && (!in_bounds(c) || !in_bounds(d)
                    || !grid_is_wall(grd(c)) || !grid_is_wall(grd(d))))
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
// In the example below, X would create dead-ends at positions a and b,
// but both Y and Z avoid this, and the resulting mini-mazes looks better.
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
    if (grid_is_wall(grd[p.x-1][p.y]))
    {
        for (int i = -1; i <= 1; i++)
        {
            if (i == 0)
                continue;

            const coord_def a(p.x, p.y+2*i);
            if (!in_bounds(a) || _is_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+2*j, p.y+i);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+j, p.y+i);
                if (_is_floor(grd(c)) && !_is_floor(grd(b)))
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
            if (!in_bounds(a) || _is_floor(grd(a)))
                continue;

            for (int j = -1; j <= 1; j++)
            {
                if (j == 0)
                    continue;

                const coord_def b(p.x+i, p.y+2*j);
                if (!in_bounds(b))
                    continue;

                const coord_def c(p.x+i, p.y+j);
                if (_is_floor(grd(c)) && !_is_floor(grd(b)))
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

        if (tries > 1 && count_known > size*size/6)
            continue;

        // Fill a vector with wall grids that are potential targets for
        // swapping against floor, i.e. are flanked by walls to two cardinal
        // directions, and by floor on the two remaining sides.
        for (rectangle_iterator ri(c1, c2); ri; ++ri)
        {
            if (is_terrain_seen(*ri) || !grid_is_wall(grd(*ri)))
                continue;

            // Skip on grids inside vaults so as not to disrupt them.
            if (testbits(env.map(*ri).property, FPROP_VAULT))
                continue;

            if (_grid_is_flanked_by_walls(*ri) && _deadend_check_floor(*ri))
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
        env.map(*ri).property &= ~(FPROP_HIGHLIGHT);
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
        if (!_grid_is_flanked_by_walls(c))
            continue;

        // Use the adjacent floor grids as source and destination.
        coord_def src(c.x-1,c.y);
        coord_def dst(c.x+1,c.y);
        if (!_is_floor(grd(src)) || !_is_floor(grd(dst)))
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

        // Add all floor grids meeting a couple of conditions to a vector
        // of potential switch points.
        std::vector<coord_def> points;
        for (unsigned int i = 0; i < path.size(); i++)
        {
            const coord_def p(path[i]);
            if (p.x < c1.x || p.x > c2.x || p.y < c1.y || p.y > c2.y)
                continue;

            if (is_terrain_seen(p.x, p.y))
                continue;

            // We don't want to deal with monsters being shifted around.
            if (mgrd(p) != NON_MONSTER)
                continue;

            // Do not pick a grid right next to the original wall.
            if (std::abs(p.x-c.x) + std::abs(p.y-c.y) <= 1)
                continue;

            if (_grid_is_flanked_by_walls(p) && _deadend_check_wall(p))
                points.push_back(p);
        }

        if (points.empty())
        {
            // Take back the previous change.
            grd(c) = old_grid;
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
        env.map(c).property |= FPROP_HIGHLIGHT;
        env.map(p).property |= FPROP_HIGHLIGHT;
#endif

        // Shift blood some most of the time.
        if (is_bloodcovered(c))
        {
            if (one_chance_in(4))
            {
                int wall_count = 0;
                coord_def old_adj(c);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (grid_is_wall(grd(*ai)) && one_chance_in(++wall_count))
                        old_adj = *ai;

                if (old_adj != c)
                {
                    if (!is_bloodcovered(old_adj))
                        env.map(old_adj).property |= FPROP_BLOODY;
                    env.map(c).property &= (~FPROP_BLOODY);
                }
            }
        }
        else if (one_chance_in(500))
        {
            // Sometimes (rarely) add blood randomly, accumulating with time...
            env.map(p).property |= FPROP_BLOODY;
        }

        // Rather than use old_grid directly, replace with an adjacent
        // wall type, preferably stone, rock, or metal.
        old_grid = grd[p.x-1][p.y];
        if (!grid_is_wall(old_grid))
        {
            old_grid = grd[p.x][p.y-1];
            if (!grid_is_wall(old_grid))
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

        // Shift blood some of the time.
        if (is_bloodcovered(p))
        {
            if (one_chance_in(4))
            {
                int floor_count = 0;
                coord_def new_adj(p);
                for (adjacent_iterator ai(c); ai; ++ai)
                    if (_is_floor(grd(*ai)) && one_chance_in(++floor_count))
                        new_adj = *ai;

                if (new_adj != p)
                {
                    if (!is_bloodcovered(new_adj))
                        env.map(new_adj).property |= FPROP_BLOODY;
                    env.map(p).property &= (~FPROP_BLOODY);
                }
            }
        }
        else if (one_chance_in(100))
        {
            // Occasionally add blood randomly, accumulating with time...
            env.map(p).property |= FPROP_BLOODY;
        }
    }

    // The directions are used to randomly decide where to place items that
    // have ended up in walls during the switching.
    std::vector<coord_def> dirs;
    dirs.push_back(coord_def(-1,-1));
    dirs.push_back(coord_def( 0,-1));
    dirs.push_back(coord_def( 1,-1));
    dirs.push_back(coord_def(-1, 0));
//    dirs.push_back(coord_def( 0, 0));
    dirs.push_back(coord_def( 1, 0));
    dirs.push_back(coord_def(-1, 1));
    dirs.push_back(coord_def( 0, 1));
    dirs.push_back(coord_def( 1, 1));

    // Search the entire shifted area for stacks of items now stuck in walls
    // and move them to a random adjacent non-wall grid.
    for (rectangle_iterator ri(c1, c2); ri; ++ri)
    {
        if (!grid_is_wall(grd(*ri)) || igrd(*ri) == NON_ITEM)
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

            if (_is_floor(grd(p)))
            {
                // Once a valid grid is found, move all items from the
                // stack onto it.
                int it = igrd(*ri);
                while (it != NON_ITEM)
                {
                    mitm[it].pos.x = p.x;
                    mitm[it].pos.y = p.y;
                    if (mitm[it].link == NON_ITEM)
                    {
                        // Link to the stack on the target grid p,
                        // or NON_ITEM, if empty.
                        mitm[it].link = igrd(p);
                        break;
                    }
                    it = mitm[it].link;
                }

                // Move entire stack over to p.
                igrd(p) = igrd(*ri);
                igrd(*ri) = NON_ITEM;

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
    if (!is_valid_item(item))
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

static void _rot_inventory_food(long time_delta)
{
    // Update all of the corpses and food chunks in the player's
    // inventory. {should be moved elsewhere - dlb}
    bool burden_changed_by_rot = false;
    std::vector<char> rotten_items;
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].quantity < 1)
            continue;

        if (!_food_item_needs_time_check(you.inv[i]))
            continue;

        if (you.inv[i].base_type == OBJ_POTIONS)
        {
            // also handles messaging
            if (maybe_coagulate_blood_potions_inv(you.inv[i]))
                burden_changed_by_rot = true;
            continue;
        }

        // food item timed out -> make it disappear
        if ((time_delta / 20) >= you.inv[i].special)
        {
            if (you.inv[i].base_type == OBJ_FOOD)
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                destroy_item(you.inv[i]);
                burden_changed_by_rot = true;
                continue;
            }

            if (you.inv[i].sub_type == CORPSE_SKELETON)
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                destroy_item(you.inv[i]);
                burden_changed_by_rot = true;
                continue;
            }

            if (!mons_skeleton(you.inv[i].plus))
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                destroy_item(you.inv[i]);
                burden_changed_by_rot = true;
                continue;
            }

            turn_corpse_into_skeleton(you.inv[i]);
            you.wield_change      = true;
            burden_changed_by_rot = true;
            continue;
        }

        // if it hasn't disappeared, reduce the rotting timer
        you.inv[i].special -= (time_delta / 20);

        if (food_is_rotten(you.inv[i])
            && (you.inv[i].special + (time_delta / 20) >= 100))
        {
            rotten_items.push_back(index_to_letter(i));
        }
    }

    //mv: messages when chunks/corpses become rotten
    if (!rotten_items.empty())
    {
        std::string msg = "";

        // Races that can't smell don't care, and trolls are stupid and
        // don't care.
        if (player_can_smell() && you.species != SP_TROLL)
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

        learned_something_new(TUT_ROTTEN_FOOD);
    }

    if (burden_changed_by_rot)
    {
        mpr("Your equipment suddenly weighs less.", MSGCH_ROTTEN_MEAT);
        burden_change();
    }
}

// Do various time related actions...
// This function is called about every 20 turns.
void handle_time(long time_delta)
{
    // Update all of the corpses, food chunks and potions of blood on
    // the floor.
    update_corpses(time_delta);

    spawn_random_monsters();

    if (crawl_state.arena)
        return;

    // Nasty things happen to people who spend too long in Hell.
    if (player_in_hell() && coinflip())
        _hell_effects();

    // Adjust the player's stats if s/he's diseased (or recovering).
    if (!you.disease)
    {
        if (you.strength < you.max_strength && one_chance_in(100))
            restore_stat(STAT_STRENGTH, 0, false, true);

        if (you.intel < you.max_intel && one_chance_in(100))
            restore_stat(STAT_INTELLIGENCE, 0, false, true);

        if (you.dex < you.max_dex && one_chance_in(100))
            restore_stat(STAT_DEXTERITY, 0, false, true);
    }
    else
    {
        if (one_chance_in(30))
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

    if (you.duration[DUR_HASTE] && !you.duration[DUR_BERSERKER]
        && x_chance_in_y(6, 10))
    {
        added_contamination++;
    }

    bool mutagenic_randart = false;
    if (const int randart_glow = scan_randarts(RAP_MUTAGENIC))
    {
        // Reduced randart glow. Note that one randart will contribute
        // 2 - 5 units of glow to randart_glow. A randart with a mutagen
        // index of 2 does about 0.58 points of contamination per turn.
        // A randart with a mutagen index of 5 does about 0.7 points of
        // contamination per turn.

        const int mean_glow   = 500 + randart_glow * 40;
        const int actual_glow = mean_glow / 2 + random2(mean_glow);
        added_contamination += div_rand_round(actual_glow, 1000);
        mutagenic_randart = true;
    }

    // we take off about .5 points per turn
    if (!you.duration[DUR_INVIS] && !you.duration[DUR_HASTE] && coinflip())
        added_contamination--;

    // only punish if contamination caused by mutagenic randarts
    // (haste and invisibility already penalized earlier)
    contaminate_player( added_contamination, mutagenic_randart );

    // only check for badness once every other turn
    if (coinflip())
    {
        // [ds] Be less harsh with glow mutation; Brent and Mark Mackey note
        // that the commented out random2(X) <= MC check was a bug. I've
        // uncommented it but dropped the roll sharply from 150. (Brent used
        // the original roll of 150 for 4.1.2, but I think players are
        // sufficiently used to beta 26's unkindness that we can use a lower
        // roll.)
        if (is_sanctuary(you.pos())
            && you.magic_contamination >= 5
            && x_chance_in_y(you.magic_contamination + 1, 25))
        {
            mpr("Your body momentarily shudders from a surge of wild "
                "energies until Zin's power calms it.", MSGCH_GOD);
        }
        else if (you.magic_contamination >= 5
                 && x_chance_in_y(you.magic_contamination + 1, 25))
        {
            mpr("Your body shudders with the violent release "
                "of wild energies!", MSGCH_WARN);

            // For particularly violent releases, make a little boom.
            // Undead enjoy extra contamination explosion damage because
            // the magical contamination has a harder time dissipating
            // through non-living flesh. :-)
            if (you.magic_contamination >= 10 && coinflip())
            {
                bolt beam;

                beam.flavour      = BEAM_RANDOM;
                beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
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

            // we want to warp the player, not do good stuff!
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
    if (!player_light_armour(true))
    {
        // lowered random roll from 7 to 6 -- bwross
        if (random2(1000) > item_mass(you.inv[you.equip[EQ_BODY_ARMOUR]])
            && one_chance_in(6))
        {
            exercise(SK_ARMOUR, 1);
        }
    }
    // Exercise stealth skill:
    else if (you.burden_state == BS_UNENCUMBERED
             && !you.duration[DUR_BERSERKER]
             && you.special_wield != SPWLD_SHADOW)
    {
        // Diminishing returns for stealth training by waiting.
        if ((you.equip[EQ_BODY_ARMOUR] == -1
            || you.equip[EQ_BODY_ARMOUR] != -1
                && random2(item_mass(you.inv[you.equip[EQ_BODY_ARMOUR]])) < 100)
            && you.skills[SK_STEALTH] <= 2 + random2(3) && one_chance_in(18))
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
}

// Move monsters around to fake them walking around while player was
// off-level.
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

    // Let sleeping monsters lie.
    if (mons_is_sleeping(mon) || mons_is_paralysed(mon))
        return;

    const int range = (turns * mon->speed) / 10;
    const int moves = (range > 50) ? 50 : range;

    // const bool short_time = (range >= 5 + random2(10));
    const bool long_time = (range >= (500 + roll_dice(2, 500)));

    const bool ranged_attack = (mons_has_ranged_spell(mon, true)
                                || mons_has_ranged_attack(mon));

#if DEBUG_DIAGNOSTICS
    // probably too annoying even for DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "mon #%d: range %d; long %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %ld",
         monster_index(mon), range, long_time, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags );
#endif

    if (range <= 0)
        return;

    if (long_time
        && (mons_is_fleeing(mon)
            || mons_is_cornered(mon)
            || mons_is_batty(mon)
            || ranged_attack
            || coinflip()))
    {
        if (!mons_is_wandering(mon))
        {
            mon->behaviour = BEH_WANDER;
            mon->foe = MHITNOT;
            mon->target = random_in_bounds();
        }
        else
        {
            // The monster will be sleeping after we move it.
            mon->behaviour = BEH_SLEEP;
        }
    }
    else if (ranged_attack)
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

#if DEBUG_DIAGNOSTICS
            mpr("backing off...", MSGCH_DIAGNOSTICS);
#endif
        }
        else
        {
            shift_monster(mon, mon->pos());

#if DEBUG_DIAGNOSTICS
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
        if (grid_is_solid(feat)
            || mgrd(next) != NON_MONSTER
            || !monster_habitable_grid(mon, feat))
        {
            break;
        }

        pos = next;
    }

    if (!shift_monster(mon, pos))
        shift_monster(mon, mon->pos());

#if DEBUG_DIAGNOSTICS
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
void update_level(double elapsedTime)
{
    ASSERT(!crawl_state.arena);

    const int turns = static_cast<int>(elapsedTime / 10.0);

#if DEBUG_DIAGNOSTICS
    int mons_total = 0;

    mprf(MSGCH_DIAGNOSTICS, "turns: %d", turns );
#endif

    update_corpses(elapsedTime);

    if (env.sanctuary_time)
    {
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
          env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (int m = 0; m < MAX_MONSTERS; m++)
    {
        monsters *mon = &menv[m];

        if (!mon->alive())
            continue;

#if DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        // Pacified monsters often leave the level now.
        if (mons_is_pacified(mon) && turns > random2(40) + 21)
        {
            make_mons_leave_level(mon);
            continue;
        }

        // Following monsters don't get movement.
        if (mon->flags & MF_JUST_SUMMONED)
            continue;

        // XXX: Allow some spellcasting (like Healing and Teleport)? -- bwr
        // const bool healthy = (mon->hit_points * 2 > mon->max_hit_points);

        // This is the monster healing code, moved here from tag.cc:
        if (monster_descriptor(mon->type, MDSC_REGENERATES)
            || mon->type == MONS_PLAYER_GHOST)
        {
            heal_monster(mon, turns, false);
        }
        else if (mons_can_regenerate(mon))
        {
            // Set a lower ceiling of 0.1 on the regen rate.
            const int regen_rate =
                std::max(mons_natural_regen_rate(mon) * 2, 5);

            heal_monster(mon, div_rand_round(turns * regen_rate, 50),
                         false);
        }

        // Handle nets specially to remove the trapping property of the net.
        if (mons_is_caught(mon))
            mon->del_ench(ENCH_HELD, true);

        _catchup_monster_moves(mon, turns);

        if (turns >= 10 && mon->alive())
            mon->timeout_enchantments(turns / 10);
    }

#if DEBUG_DIAGNOSTICS
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
            set_envmap_obj(where, grd(where));

        // Clean bloody floor.
        if (is_bloodcovered(where))
            env.map(where).property &= ~(FPROP_BLOODY);

        // Chance of cleaning adjacent squares.
        for (adjacent_iterator ai(where); ai; ++ai)
            if (is_bloodcovered(*ai) && one_chance_in(5))
                env.map(*ai).property &= ~(FPROP_BLOODY);

        break;
   }
}

//---------------------------------------------------------------
//
// update_corpses
//
// Update all of the corpses and food chunks on the floor. (The
// elapsed time is a double because this is called when we re-
// enter a level and a *long* time may have elapsed).
//
//---------------------------------------------------------------
void update_corpses(double elapsedTime)
{
    if (elapsedTime <= 0.0)
        return;

    const long rot_time = static_cast<long>(elapsedTime / 20.0);

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

    int fountain_checks = static_cast<int>(elapsedTime / 1000.0);
    if (x_chance_in_y(static_cast<int>(elapsedTime) % 1000, 1000))
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

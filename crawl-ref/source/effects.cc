/*
 *  File:       effects.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "effects.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "decks.h"
#include "delay.h"
#include "direct.h"
#include "dgnevent.h"
#include "food.h"
#include "hiscores.h"
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
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

// torment_monsters is called with power 0 because torment is
// UNRESISTABLE except for being undead or having torment
// resistance!  Even if we used maximum power of 1000, high
// level monsters and characters would save too often.  (GDL)

int torment_monsters(int x, int y, int pow, int caster)
{
    UNUSED( pow );

    // is player?
    if (x == you.x_pos && y == you.y_pos)
    {
        // [dshaligram] Switched to using ouch() instead of dec_hp so that
        // notes can also track torment and activities can be interrupted
        // correctly.
        int hploss = 0;
        if (!player_res_torment())
        {
            // negative energy resistance can alleviate torment
            hploss = you.hp * (50 - player_prot_life() * 5) / 100 - 1;
            if (hploss >= you.hp)
                hploss = you.hp - 1;
            if (hploss < 0)
                hploss = 0;
        }

        if (!hploss)
            mpr("You feel a surge of unholy energy.");
        else
        {
            mpr("Your body is wracked with pain!");

            const char *aux = "torment";
            if (caster < 0)
            {
                switch (caster)
                {
                case TORMENT_CARDS:
                case TORMENT_SPELL:
                    aux = "Symbol of Torment";
                    break;
                case TORMENT_SPWLD:
                    // XXX: If we ever make any other weapon / randart
                    // eligible to torment, this will be incorrect.
                    aux = "Sceptre of Torment";
                    break;
                case TORMENT_SCROLL:
                    aux = "scroll of torment";
                    break;
                }
                caster = TORMENT_GENERIC;
            }
            ouch(hploss, caster, 
                    caster != TORMENT_GENERIC? KILLED_BY_MONSTER 
                                             : KILLED_BY_SOMETHING, 
                    aux);
        }

        return 1;
    }

    // check for monster in cell
    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)
        return 0;

    struct monsters *monster = &menv[mon];

    if (monster->type == -1)
        return 0;

    if (mons_res_negative_energy( monster ) >= 3)
        return 0;

    monster->hit_points = 1 + (monster->hit_points / 2);
    simple_monster_message(monster, " convulses!");

    return 1;
}

void torment(int caster, int tx, int ty)
{
    apply_area_within_radius(torment_monsters, tx, ty, 0, 8, caster);
}                               // end torment()

static std::string who_banished(const std::string &who)
{
    return (who.empty()? who : " (" + who + ")");
}

void banished(dungeon_feature_type gate_type, const std::string &who)
{
#ifdef DGL_MILESTONES
    if (gate_type == DNGN_ENTER_ABYSS)
        mark_milestone("abyss.enter",
                       "is cast into the Abyss!" + who_banished(who));
    else if (gate_type == DNGN_EXIT_ABYSS)
        mark_milestone("abyss.exit",
                       "escaped from the Abyss!" + who_banished(who));
#endif

    std::string cast_into = "";

    switch(gate_type)
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
    else if (who.find("miscast") != std::string::npos)
        you.entry_cause = EC_MISCAST;
    else if (who == "wizard command")
        you.entry_cause = EC_SELF_EXPLICIT;
    else
        you.entry_cause = EC_MONSTER;

    if (!crawl_state.is_god_acting())
        you.entry_cause_god = GOD_NO_GOD;

    if (cast_into != "" && you.entry_cause != EC_SELF_EXPLICIT)
    {
        const std::string what = "Cast into " + cast_into + who_banished(who);
        take_note(Note(NOTE_MESSAGE, 0, 0, what.c_str()), true);
    }

    // no longer held in net
    clear_trapping_net();

    down_stairs(you.your_level, gate_type, you.entry_cause);  // heh heh
}

bool forget_spell(void)
{
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
            ouch(INSTANT_DEATH, 0, kill_type);
        else
            ouch(INSTANT_DEATH, 0, kill_type, cause, see_source);
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

void direct_effect(struct bolt &pbolt)
{
    int damage_taken = 0;

    monsters* source = NULL;

    if (pbolt.beam_source != NON_MONSTER)
        source = &menv[pbolt.beam_source];

    switch (pbolt.type)
    {
    case DMNBM_HELLFIRE:
        pbolt.name = "hellfire";
        pbolt.ex_size = 1;
        pbolt.flavour = BEAM_HELLFIRE;
        pbolt.is_explosion = true;
        pbolt.type = dchar_glyph(DCHAR_FIRED_ZAP);
        pbolt.colour = RED;
        pbolt.thrower = KILL_MON_MISSILE;
        pbolt.aux_source.clear();
        pbolt.is_beam = false;
        pbolt.is_tracer = false;
        pbolt.hit = 20;
        pbolt.damage = dice_def( 3, 20 );
        pbolt.aux_source = "burst of hellfire";
        explosion( pbolt );
        break;

    case DMNBM_SMITING:
        mpr( "Something smites you!" );
        pbolt.name = "smiting";
        pbolt.aux_source = "by divine providence";
        damage_taken = 7 + random2avg(11, 2);
        break;

    case DMNBM_BRAIN_FEED:
        // lose_stat() must come last {dlb}
        if (one_chance_in(3) &&
            lose_stat(STAT_INTELLIGENCE, 1, source))
        {
            mpr("Something feeds on your intellect!");
            xom_is_stimulated(50);
        }
        else
        {
            mpr("Something tries to feed on your intellect!");
        }
        break;
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
        ouch(damage_taken, pbolt.beam_source, KILLED_BY_BEAM, 
             pbolt.aux_source.c_str());

    return;
}                               // end direct_effect()

// monster-to-monster
void mons_direct_effect(struct bolt &pbolt, int i)
{
    // note the translation here - important {dlb}
    int o = menv[i].foe;
    struct monsters *monster = &menv[o];
    int damage_taken = 0;

    // annoy the target
    behaviour_event(monster, ME_ANNOY, i);

    switch (pbolt.type)
    {
    case DMNBM_HELLFIRE:
        simple_monster_message(monster, " is engulfed in hellfire.");
        pbolt.name = "hellfire";
        pbolt.flavour = BEAM_LAVA;

        damage_taken = 5 + random2(10) + random2(5);
        damage_taken = mons_adjust_flavoured(monster, pbolt, damage_taken);
        break;

    case DMNBM_SMITING:
        simple_monster_message(monster, " is smitten.");
        pbolt.name = "smiting";
        pbolt.flavour = BEAM_MISSILE;

        damage_taken += 7 + random2avg(11, 2);
        break;

    case DMNBM_BRAIN_FEED:      // not implemented here (nor, probably, can be)
        break;

    case DMNBM_MUTATION:
        if (mons_holiness(monster) != MH_NATURAL ||
            mons_immune_magic(monster))
            simple_monster_message(monster, " is unaffected.");
        else if (check_mons_resist_magic( monster, pbolt.ench_power ))
            simple_monster_message(monster, " resists.");
        else
            monster_polymorph(monster, RANDOM_MONSTER);
        break;
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
    {
        hurt_monster(monster, damage_taken);

        if (monster->hit_points < 1)
            monster_die(monster, KILL_MON_MISSILE, i);
    }

    return;
}                               // end mons_direct_effect()

void random_uselessness(unsigned char ru, unsigned char sc_read_2)
{
    int temp_rand = 0;          // probability determination {dlb}

    switch (ru)
    {
    case 0:
        msg::stream << "The dust glows a " << weird_glow_colour()
                    << " colour!" << std::endl;
        break;

    case 1:
        mpr("The scroll reassembles itself in your hand!");
        inc_inv_item_quantity( sc_read_2, 1 );
        break;

    case 2:
        if (you.equip[EQ_WEAPON] != -1)
        {
            msg::stream << you.inv[you.equip[EQ_WEAPON]].name(DESC_CAP_YOUR)
                        << " glows " << weird_glow_colour()
                        << " for a moment." << std::endl;
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }
        break;

    case 3:
        temp_rand = random2(8);
        mprf("You hear the distant roaring of an enraged %s!",
             (temp_rand == 0) ? "frog"          :
             (temp_rand == 1) ? "pill bug"      :
             (temp_rand == 2) ? "millipede"     :
             (temp_rand == 3) ? "eggplant"      :
             (temp_rand == 4) ? "albino dragon" :
             (temp_rand == 5) ? "dragon"        :
             (temp_rand == 6) ? "human"
                              : "slug");
        break;

    case 4:
        if (player_can_smell())
        {
            temp_rand = random2(8);
            mprf("You smell %s",
                 (temp_rand == 0) ? "coffee."          :
                 (temp_rand == 1) ? "salt."            :
                 (temp_rand == 2) ? "burning hair!"    :
                 (temp_rand == 3) ? "baking bread."    :
                 (temp_rand == 4) ? "something weird." :
                 (temp_rand == 5) ? "wet wool."        :
                 (temp_rand == 6) ? "sulphur."
                                  : "fire and brimstone!");
        }
        break;

    case 5:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 6:
        temp_rand = random2(3);
        mprf("Your %s",
             (temp_rand == 0) ? "ears itch."   :
             (temp_rand == 1) ? "brain hurts!"
                              : "nose twitches suddenly!");
        break;

    case 7:
        mpr("You hear the tinkle of a tiny bell.");
        cast_summon_butterflies( 100 );
        break;

    case 8:
        temp_rand = random2(9);
        mprf("You hear %s.",
             (temp_rand == 0) ? "snatches of song"                 :
             (temp_rand == 1) ? "a voice call someone else's name" :
             (temp_rand == 2) ? "a very strange noise"             :
             (temp_rand == 3) ? "roaring flame"                    :
             (temp_rand == 4) ? "a very strange noise indeed"      :
             (temp_rand == 5) ? "the chiming of a distant gong"    :
             (temp_rand == 6) ? "the bellowing of a yak"           :
             (temp_rand == 7) ? "a crunching sound"
                              : "the tinkle of an enormous bell");
        break;
    }

    return;
}                               // end random_uselessness()

static armour_type random_nonbody_armour_type()
{
    const armour_type at =
        static_cast<armour_type>( 
            random_choose(ARM_SHIELD, ARM_CLOAK, ARM_HELMET,
                          ARM_GLOVES, ARM_BOOTS, -1));
    return (at);
}

static int find_acquirement_subtype(object_class_type class_wanted,
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

            if (you.mutation[MUT_HERBIVOROUS])
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

        // Can't do much with launchers, so we'll avoid them for now -- bwr
        for (int i = SK_SHORT_BLADES; i < SK_DARTS; i++)
        {
            if (i == SK_UNUSED_1)
                continue;

            // Adding a small constant allows for the occasional
            // weapon in an untrained skill.

            const int weight = you.skills[i] + 1;
            count += weight;

            if (random2(count) < weight)
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

            if (wskill == skill && random2(count += acqweight) < acqweight)
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
                if (random2(count) < you.skills[i])
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
            static_cast<int>(random_nonbody_armour_type());

        // some species specific fitting problems
        switch (you.species)
        {
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
        case SP_RED_DRACONIAN:
        case SP_WHITE_DRACONIAN:
        case SP_GREEN_DRACONIAN:
        case SP_GOLDEN_DRACONIAN:
        case SP_GREY_DRACONIAN:
        case SP_BLACK_DRACONIAN:
        case SP_PURPLE_DRACONIAN:
        case SP_MOTTLED_DRACONIAN:
        case SP_PALE_DRACONIAN:
        case SP_UNK0_DRACONIAN:
        case SP_UNK1_DRACONIAN:
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

        // mutation specific problems (horns allow caps)
        if ((type_wanted == ARM_BOOTS && !player_has_feet())
            || (you.has_claws(false) >= 3 && type_wanted == ARM_GLOVES))
        {
            type_wanted = OBJ_RANDOM;
        }

        // Do this here, before acquirement()'s call to can_wear_armour(),
        // so that caps will be just as common as helmets for those
        // that can't wear helmets.
        if (type_wanted == ARM_HELMET
            && ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
                || player_genus(GENPC_DRACONIAN)
                || you.species == SP_KENKU
                || you.species == SP_SPRIGGAN
                || you.mutation[MUT_HORNS]))
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
                // remember, put rarer books higher in the list
                iteration = 1;
                type_wanted = NUM_BOOKS;

                best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS - 1), 
                                         best_spell );

              which_book:
#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "acquirement: iteration = %d, best_spell = %d",
                     iteration, best_spell );
#endif //jmf: debugging

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
                    if (best_any >= SK_FIGHTING
                                && best_any <= SK_STAVES)
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

                case SK_TRANSMIGRATION:
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
                    best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS - 1),
                                             best_skill(SK_SPELLCASTING,
                                                        (NUM_SKILLS - 1), 99) );
                    iteration++;
                    goto which_book;
                }

                // if we don't have a book, try and get a new one.
                if (type_wanted == NUM_BOOKS)
                {
                    do
                    {
                        type_wanted = random2(NUM_BOOKS);
                        if (one_chance_in(500))
                            break;
                    }
                    while (you.had_book[type_wanted]);
                }

                // if the book is invalid find any valid one.
                while (book_rarity(type_wanted) == 100
                                       || type_wanted == BOOK_DESTRUCTION
                                       || type_wanted == BOOK_MANUAL)
                {
                    type_wanted = random2(NUM_BOOKS);
                }
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

                // If we're going to give out an enhancer stave, 
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

                default: // invocations and leftover spell schools
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
                    type_wanted = random2(NUM_MISCELLANY);
                while (type_wanted == MISC_HORN_OF_GERYON
                    || type_wanted == MISC_RUNE_OF_ZOT
                    || type_wanted == MISC_CRYSTAL_BALL_OF_FIXATION
                    || type_wanted == MISC_EMPTY_EBONY_CASKET);
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

bool acquirement(object_class_type class_wanted, int agent,
                 bool quiet, int* item_index)
{
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
        switch ( keyin )
        {
        case 'a': class_wanted = OBJ_WEAPONS;    break;
        case 'b': class_wanted = OBJ_ARMOUR;     break;
        case 'c': class_wanted = OBJ_JEWELLERY;  break;
        case 'd': class_wanted = OBJ_BOOKS;      break;
        case 'e': class_wanted = OBJ_STAVES;     break;
        case 'f': class_wanted = OBJ_FOOD;       break;
        case 'g': class_wanted = OBJ_MISCELLANY; break;
        case 'h': class_wanted = OBJ_GOLD;       break;
        default: break;
        }
    }

    if (grid_destroys_items(grd[you.x_pos][you.y_pos]))
    {
        // how sad (and stupid)
        if (!silenced(you.pos()) && !quiet)
            mprf(MSGCH_SOUND, 
                 grid_item_destruction_message(grd[you.x_pos][you.y_pos]));

        item_was_destroyed(mitm[igrd[you.x_pos][you.y_pos]], NON_MONSTER);
        *item_index = NON_ITEM;
    }
    else
    {
        int quant = 1;
        for (int item_tries = 0; item_tries < 40; item_tries++)
        {
            int type_wanted = find_acquirement_subtype(class_wanted, quant);
            
            // clobber class_wanted for vampires
            if (you.species == SP_VAMPIRE && class_wanted == OBJ_FOOD)
                class_wanted = OBJ_POTIONS;

            thing_created = items( 1, class_wanted, type_wanted, true, 
                                   MAKE_GOOD_ITEM, 250 );

            if (thing_created == NON_ITEM)
                continue;

            const item_def &doodad(mitm[thing_created]);
            if ((doodad.base_type == OBJ_WEAPONS
                 && !can_wield(&doodad, false, true))
                || (doodad.base_type == OBJ_ARMOUR
                    && !can_wear_armour(doodad, false, true))

                // Trog does not gift the Wrath of Trog.
                || (agent == GOD_TROG && is_fixed_artefact(doodad)
                    && doodad.special == SPWPN_WRATH_OF_TROG))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }

            // MT - Check: god-gifted weapons and armor shouldn't kill you.
            // Except Xom.
            if ((agent == GOD_TROG || agent == GOD_OKAWARU)
                && is_random_artefact(doodad))
            {
                randart_properties_t  proprt;
                randart_wpn_properties( doodad, proprt );

                // check vs stats. positive stats will automatically fall
                // through.  As will negative stats that won't kill you.
                if (-proprt[RAP_STRENGTH] >= you.strength ||
                    -proprt[RAP_INTELLIGENCE] >= you.intel ||
                    -proprt[RAP_DEXTERITY] >= you.dex)
                {
                    // try again
                    destroy_item(thing_created);
                    thing_created = NON_ITEM;
                    continue;
                }
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

        // easier to read this way
        item_def& thing(mitm[thing_created]);

        // give some more gold
        if ( class_wanted == OBJ_GOLD )
            thing.quantity += 150;
        else if (quant > 1)
            thing.quantity = quant;
    
        // remove curse flag from item
        do_uncurse_item( thing );

        if (thing.base_type == OBJ_BOOKS)
        {
            mark_had_book(thing.sub_type);
        }
        else if (thing.base_type == OBJ_JEWELLERY)
        {
            switch (thing.sub_type)
            {
            case RING_SLAYING:
                // make sure plus to damage is >= 1
                thing.plus2 = abs( thing.plus2 );
                if (thing.plus2 == 0)
                    thing.plus2 = 1;
                // fall through... 

            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
                // make sure plus is >= 1
                thing.plus = abs( thing.plus );
                if (thing.plus == 0)
                    thing.plus = 1;
                break;

            case RING_HUNGER:
            case AMU_INACCURACY:
                // these are the only truly bad pieces of jewellery
                if (!one_chance_in(9))
                    make_item_randart( thing );
                break;

            default:
                break;
            }
        }
        else if (thing.base_type == OBJ_WEAPONS
                 && !is_fixed_artefact( thing )
                 && !is_unrandom_artefact( thing ))
        {
            // HACK: make unwieldable weapons wieldable 
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
                            // keep resetting seed until it's good:
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
                    thing.sub_type = 
                            (coinflip() ? WPN_FALCHION : WPN_LONG_SWORD);
                    break;

                case WPN_GREAT_MACE:
                case WPN_DIRE_FLAIL:
                    thing.sub_type = 
                            (coinflip() ? WPN_MACE : WPN_FLAIL);
                    break;

                case WPN_BATTLEAXE:
                case WPN_EXECUTIONERS_AXE:
                    thing.sub_type = 
                            (coinflip() ? WPN_HAND_AXE : WPN_WAR_AXE);
                    break;

                case WPN_HALBERD:
                case WPN_GLAIVE:
                case WPN_SCYTHE:
                case WPN_BARDICHE:
                    thing.sub_type = 
                            (coinflip() ? WPN_SPEAR : WPN_TRIDENT);
                    break;
                }
                break;

            default:
                break;
            }
            
            int plusmod = random2(4);
            // more damage, less accuracy
            if (agent == GOD_TROG)
            {
                thing.plus  -= plusmod;
                thing.plus2 += plusmod;
                if (!is_random_artefact(thing))
                    thing.plus = std::max(static_cast<int>(thing.plus), 0);
            }
            // more accuracy, less damage
            else if (agent == GOD_OKAWARU)
            {
                thing.plus  += plusmod;
                thing.plus2 -= plusmod;
                if (!is_random_artefact(thing))
                    thing.plus2 = std::max(static_cast<int>(thing.plus2), 0);
            }
        }

        move_item_to_grid( &thing_created, you.x_pos, you.y_pos );

        // This should never actually be NON_ITEM because of the way
        // move_item_to_grid works (doesn't create a new item ever), 
        // but we're checking it anyways. -- bwr
        if (thing_created != NON_ITEM)
        {
            if (!quiet)
                canned_msg(MSG_SOMETHING_APPEARS);
            origin_acquired(mitm[thing_created], agent);
        }
        *item_index = thing_created;
    }

    // Well, the item may have fallen in the drink, but the intent is 
    // that acquirement happened. -- bwr
    return (true);
}                               // end acquirement()

bool recharge_wand(void)
{
    if (you.equip[EQ_WEAPON] == -1)
        return (false);

    item_def &wand = you.inv[ you.equip[EQ_WEAPON] ];

    if (wand.base_type != OBJ_WANDS && !item_is_rod(wand))
        return (false);

    int charge_gain = 0;
    if (wand.base_type == OBJ_WANDS)
    {
        switch (wand.sub_type)
        {
        case WAND_INVISIBILITY:
        case WAND_FIREBALL:
        case WAND_HEALING:
        case WAND_HASTING:
            charge_gain = 3;
            break;

        case WAND_LIGHTNING:
        case WAND_DRAINING:
            charge_gain = 4;
            break;

        case WAND_FIRE:
        case WAND_COLD:
            charge_gain = 5;
            break;

        default:
            charge_gain = 8;
            break;
        }

        // don't display zap counts any more
        wand.plus2 = ZAPCOUNT_UNKNOWN;

        mprf("%s glows for a moment.", wand.name(DESC_CAP_YOUR).c_str());

        wand.plus += 1 + random2avg( ((charge_gain - 1) * 3) + 1, 3 );

        if (wand.plus > charge_gain * 3)
            wand.plus = charge_gain * 3;
    }
    else 
    {
        // This is a rod.
        bool work = false;

        if (wand.plus2 <= MAX_ROD_CHARGE * ROD_CHARGE_MULT)
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
}                               // end recharge_wand()

void yell(bool force)
{
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

    if (silenced(you.x_pos, you.y_pos) || you.cannot_speak())
        noise_level = 0;

    if (noise_level == 0)
    {
        if (force)
        {
            if (shout_verb == "__NONE" || you.paralysed())
                mprf("You feel a strong urge to %s, but you are unable to make a sound!",
                     shout_verb == "__NONE" ? "scream" : shout_verb.c_str());
            else
                mprf("You feel a %s rip itself from your throat, but you make no sound!",
                     shout_verb.c_str());
        }
        else
            mpr("You are unable to make a sound!");

        return;
    }

    if (force)
    {
        mprf("A %s rips itself from your throat!", shout_verb.c_str());
        noisy( noise_level, you.x_pos, you.y_pos );
        return;
    }

    mpr("What do you say?", MSGCH_PROMPT);
    mprf(" ! - %s", cap_shout.c_str());

    if (!you.duration[DUR_BERSERKER])
    {
        mpr(" a - Order allies to attack a monster");

        if (!(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU))
        {
            struct monsters *target = &menv[you.prev_targ];

            if (mons_near(target) && player_monster_visible(target))
            {
                mpr(" p - Order allies to attack your previous target");
                targ_prev = true;
            }
        }
   }

    mprf(" Anything else - Stay silent%s",
         one_chance_in(20)? " (and be thought a fool)" : "");

    unsigned char keyn = get_ch();

    switch (keyn)
    {
    case '!':
        mprf(MSGCH_SOUND, "You %s for attention!", shout_verb.c_str());
        you.turn_is_over = true;
        noisy( noise_level, you.x_pos, you.y_pos );
        return;

    case 'a':
        if (you.duration[DUR_BERSERKER])
        {
            canned_msg(MSG_TOO_BERSERK);
            return;
        }

        mpr("Gang up on whom?", MSGCH_PROMPT);
        direction( targ, DIR_TARGET, TARG_ENEMY, false, false );

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        if (!targ.isValid || mgrd[targ.tx][targ.ty] == NON_MONSTER ||
            !player_monster_visible(&env.mons[mgrd[targ.tx][targ.ty]]))
        {
            mpr("Yeah, whatever.");
            return;
        }

        mons_targd = mgrd[targ.tx][targ.ty];
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
        /* fall through... */
    default:
        mpr("Okely-dokely.");
        return;
    }

    you.pet_target = mons_targd;

    noisy( 10, you.x_pos, you.y_pos );
    mpr("Attack!");
}                               // end yell()

bool forget_inventory(bool quiet)
{
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
                // Don't forget identity of decks if it the player has
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

///////////////////////////////////////////////////////////////////////

static void hell_effects()
{
    int temp_rand = random2(17);
    spschool_flag_type which_miscast = SPTYP_RANDOM;
    bool summon_instead = false;
    monster_type which_beastie = MONS_PROGRAM_BUG;

    mpr((temp_rand == 0)  ? "\"You will not leave this place.\"" :
        (temp_rand == 1)  ? "\"Die, mortal!\"" :
        (temp_rand == 2)  ? "\"We do not forgive those who trespass against us!\"" :
        (temp_rand == 3)  ? "\"Trespassers are not welcome here!\"" :
        (temp_rand == 4)  ? "\"You do not belong in this place!\"" :
        (temp_rand == 5)  ? "\"Leave now, before it is too late!\"" :
        (temp_rand == 6)  ? "\"We have you now!\"" :
        // plain messages
        (temp_rand == 7)  ? (player_can_smell()) ? "You smell brimstone." :
        "Brimstone rains from above." :
        (temp_rand == 8)  ? "You feel lost and a long, long way from home..." :
        (temp_rand == 9)  ? "You shiver with fear." :
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
        (temp_rand < 7  ? MSGCH_TALK :
         temp_rand < 10 ? MSGCH_PLAIN :
         temp_rand < 15 ? MSGCH_WARN
         : MSGCH_SOUND) );

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

        miscast_effect( which_miscast, 4 + random2(6), random2avg(97, 3),
                        100, "the effects of Hell" );
    }
    else if (temp_rand > 7) // 10 in 27 odds {dlb}
    {
        // 60:40 miscast:summon split {dlb}
        summon_instead = (random2(5) > 2);

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
        default:        // this is to silence gcc compiler warnings {dlb}
            if (summon_instead)
                which_beastie = MONS_FIEND;
            else
                which_miscast = SPTYP_NECROMANCY;
            break;
        }

        if (summon_instead)
        {
            create_monster( which_beastie, 0, BEH_HOSTILE, you.x_pos,
                            you.y_pos, MHITYOU, 250 );
        }
        else
        {
            miscast_effect( which_miscast, 4 + random2(6),
                            random2avg(97, 3), 100, "the effects of Hell" );
        }
    }

    // NB: no "else" - 8 in 27 odds that nothing happens through
    // first chain {dlb}
    // also note that the following is distinct from and in
    // addition to the above chain:

    // try to summon at least one and up to five random monsters {dlb}
    if (one_chance_in(3))
    {
        create_monster( RANDOM_MONSTER, 0, BEH_HOSTILE, 
                        you.x_pos, you.y_pos, MHITYOU, 250 );

        for (int i = 0; i < 4; i++)
        {
            if (one_chance_in(3))
            {
                create_monster( RANDOM_MONSTER, 0, BEH_HOSTILE,
                                you.x_pos, you.y_pos, MHITYOU, 250 );
            }
        }
    }
}

static void rot_inventory_food(long time_delta)
{
    // Update all of the corpses and food chunks in the player's
    // inventory {should be moved elsewhere - dlb}

    bool burden_changed_by_rot = false;
    bool new_rotting_item = false;
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].quantity < 1)
            continue;

        if (you.inv[i].base_type != OBJ_CORPSES && you.inv[i].base_type != OBJ_FOOD)
            continue;

        if (you.inv[i].base_type == OBJ_CORPSES
            && you.inv[i].sub_type > CORPSE_SKELETON)
        {
            continue;
        }

        if (you.inv[i].base_type == OBJ_FOOD && you.inv[i].sub_type != FOOD_CHUNK)
            continue;

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
                continue;       // carried skeletons are not destroyed

            if (!mons_skeleton( you.inv[i].plus ))
            {
                if (you.equip[EQ_WEAPON] == i)
                    unwield_item();

                destroy_item(you.inv[i]);
                burden_changed_by_rot = true;
                continue;
            }

            you.inv[i].sub_type = CORPSE_SKELETON;
            you.inv[i].special = 0;
            you.inv[i].colour = LIGHTGREY;
            you.wield_change = true;
            burden_changed_by_rot = true;
            continue;
        }

        you.inv[i].special -= (time_delta / 20);

        if (you.inv[i].special < 100 && (you.inv[i].special + (time_delta / 20)>=100))
        {
            new_rotting_item = true; 
        }
    }

    //mv: messages when chunks/corpses become rotten
    if (new_rotting_item)
    {
        // XXX: should probably still notice?
        // Races that can't smell don't care, and trolls are stupid and
        // don't care.
        if (player_can_smell() && you.species != SP_TROLL)
        {
            int temp_rand = 0; // Grr.
            switch (you.mutation[MUT_SAPROVOROUS])
            {
            // level 1 and level 2 saprovores aren't so touchy
            case 1:
            case 2:
                temp_rand = random2(8);
                mpr( ((temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "You smell rotting flesh." :
                      (temp_rand == 6) ? "You smell decay."
                                       : "There is something rotten in your inventory."),
                    MSGCH_ROTTEN_MEAT );
                break;

            // level 3 saprovores like it
            case 3:
                temp_rand = random2(8);
                mpr( ((temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you hungry." :
                      (temp_rand == 6) ? "You smell decay. Yum-yum."
                                       : "Wow! There is something tasty in your inventory."),
                    MSGCH_ROTTEN_MEAT );
                break;

            default:
                temp_rand = random2(8);
                mpr( ((temp_rand  < 5) ? "You smell something rotten." :
                      (temp_rand == 5) ? "The smell of rotting flesh makes you sick." :
                      (temp_rand == 6) ? "You smell decay. Yuck!"
                                       : "Ugh! There is something really disgusting in your inventory."), 
                    MSGCH_ROTTEN_MEAT );
                break;
            }
        }
        learned_something_new(TUT_ROTTEN_FOOD);
    }
    if (burden_changed_by_rot)
    {
        mpr("Your equipment suddenly weighs less.", MSGCH_ROTTEN_MEAT);
        burden_change();
    }
}

//---------------------------------------------------------------
//
// handle_time
//
// Do various time related actions... 
// This function is called about every 20 turns.
//
//---------------------------------------------------------------
void handle_time( long time_delta )
{
    // BEGIN - Nasty things happen to people who spend too long in Hell:
    if (player_in_hell() && coinflip())
        hell_effects();

    // Adjust the player's stats if s/he's diseased (or recovering).
    if (!you.disease)
    {
        if (you.strength < you.max_strength && one_chance_in(100))
        {
            mpr("You feel your strength returning.", MSGCH_RECOVERY);
            you.strength++;
            you.redraw_strength = 1;
        }

        if (you.dex < you.max_dex && one_chance_in(100))
        {
            mpr("You feel your dexterity returning.", MSGCH_RECOVERY);
            you.dex++;
            you.redraw_dexterity = 1;
        }

        if (you.intel < you.max_intel && one_chance_in(100))
        {
            mpr("You feel your intelligence returning.", MSGCH_RECOVERY);
            you.intel++;
            you.redraw_intelligence = 1;
        }
    }
    else
    {
        if (one_chance_in(30))
        {
            mpr("Your disease is taking its toll.", MSGCH_WARN);
            lose_stat(STAT_RANDOM, 1, false, "disease");
        }
    }

    // Adjust the player's stats if s/he has the deterioration mutation
    if (you.mutation[MUT_DETERIORATION]
        && random2(200) <= you.mutation[MUT_DETERIORATION] * 5 - 2)
    {
        lose_stat(STAT_RANDOM, 1, false, "deterioration mutation");
    }

    int added_contamination = 0;

    // Account for mutagenic radiation.  Invis and haste will give the
    // player about .1 points per turn, mutagenic randarts will give
    // about 1.5 points on average, so they can corrupt the player
    // quite quickly.  Wielding one for a short battle is OK, which is
    // as things should be.   -- GDL
    if (you.duration[DUR_INVIS] && random2(10) < 6)
        added_contamination++;

    if (you.duration[DUR_HASTE] && !you.duration[DUR_BERSERKER]
        && random2(10) < 6)
    {
        added_contamination++;
    }

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
    }

    // we take off about .5 points per turn
    if (!you.duration[DUR_INVIS] && !you.duration[DUR_HASTE] && coinflip())
        added_contamination -= 1;

    contaminate_player( added_contamination );

    // only check for badness once every other turn
    if (coinflip())
    {
        // [ds] Be less harsh with glow mutation; Brent and Mark Mackey note
        // that the commented out random2(X) <= MC check was a bug. I've
        // uncommented it but dropped the roll sharply from 150. (Brent used
        // the original roll of 150 for 4.1.2, but I think players are
        // sufficiently used to beta 26's unkindness that we can use a lower
        // roll.)
        if (you.magic_contamination >= 5
            && random2(25) <= you.magic_contamination)
        {
            mpr("Your body shudders with the violent release of wild energies!", MSGCH_WARN);

            // for particularly violent releases, make a little boom
            if (you.magic_contamination >= 10 && coinflip())
            {
                struct bolt boom;
                boom.type = dchar_glyph(DCHAR_FIRED_BURST);
                boom.colour = BLACK;
                boom.flavour = BEAM_RANDOM;
                boom.target_x = you.x_pos;
                boom.target_y = you.y_pos;
                // Undead enjoy extra contamination explosion damage because
                // the magical contamination has a harder time dissipating
                // through non-living flesh. :-)
                boom.damage =
                    dice_def( 3,
                              you.magic_contamination
                              * (you.is_undead? 4 : 2)
                              / 4 );
                boom.thrower = KILL_MISC;
                boom.aux_source = "a magical explosion";
                boom.beam_source = NON_MONSTER;
                boom.is_beam = false;
                boom.is_tracer = false;
                boom.is_explosion = true;
                boom.name = "magical storm";

                boom.ench_power = (you.magic_contamination * 5);
                boom.ex_size = (you.magic_contamination / 15);
                if (boom.ex_size > 9)
                    boom.ex_size = 9;

                explosion(boom);
            }

            // we want to warp the player, not do good stuff!
            if (one_chance_in(5))
                mutate(RANDOM_MUTATION);
            else
                give_bad_mutation(coinflip());

            // we're meaner now, what with explosions and whatnot, but
            // we dial down the contamination a little faster if its actually
            // mutating you.  -- GDL
            contaminate_player( -(random2(you.magic_contamination / 4) + 1) );
        }
    }

    // Random chance to identify staff in hand based off of Spellcasting
    // and an appropriate other spell skill... is 1/20 too fast?
    if (you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_STAVES
        && !item_type_known( you.inv[you.equip[EQ_WEAPON]] )
        && one_chance_in(20))
    {
        int total_skill = you.skills[SK_SPELLCASTING];

        switch (you.inv[you.equip[EQ_WEAPON]].sub_type)
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

        if (random2(100) < total_skill)
        {
            item_def& item = you.inv[you.equip[EQ_WEAPON]];
            set_ident_flags( item, ISFLAG_IDENT_MASK );

            mprf("You are wielding %s.", item.name(DESC_NOCAP_A).c_str());
            more();

            you.wield_change = true;
        }
    }

    // Check to see if an upset god wants to do something to the player
    // jmf: moved huge thing to religion.cc
    handle_god_time();

    if (you.mutation[MUT_SCREAM]
        && (random2(100) <= 2 + you.mutation[MUT_SCREAM] * 3) )
    {
        yell(true);
    }
    else if (you.mutation[MUT_SLEEPINESS]
             && random2(100) < you.mutation[MUT_SLEEPINESS] * 5)
    {
        you.put_to_sleep();
    }

    // Update all of the corpses and food chunks on the floor
    update_corpses(time_delta);

    rot_inventory_food(time_delta);

    // exercise armour *xor* stealth skill: {dlb}
    if (!player_light_armour(true))
    {
        if (random2(1000) <= item_mass( you.inv[you.equip[EQ_BODY_ARMOUR]] ))
            return;

        if (one_chance_in(6))   // lowered random roll from 7 to 6 -- bwross
            exercise(SK_ARMOUR, 1);
    }
    else                        // exercise stealth skill:
    {
        if (you.burden_state != BS_UNENCUMBERED || you.duration[DUR_BERSERKER])
            return;

        if (you.special_wield == SPWLD_SHADOW)
            return;

        if (you.equip[EQ_BODY_ARMOUR] != -1
            && random2( item_mass( you.inv[you.equip[EQ_BODY_ARMOUR]] )) >= 100)
        {
            return;
        }

        if (one_chance_in(18))
            exercise(SK_STEALTH, 1);
    }
}

//---------------------------------------------------------------
//
// update_level
//
// Update the level when the player returns to it.
//
//---------------------------------------------------------------
void update_level( double elapsedTime )
{
    int m, i;
    const int turns = static_cast<int>(elapsedTime / 10.0);

#if DEBUG_DIAGNOSTICS
    int mons_total = 0;

    mprf(MSGCH_DIAGNOSTICS, "turns: %d", turns );
#endif

    update_corpses( elapsedTime );
     
    if (env.sanctuary_time)
    {
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
          env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (m = 0; m < MAX_MONSTERS; m++)
    {
        struct monsters *mon = &menv[m];

        if (mon->type == -1)
            continue;

#if DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        // following monsters don't get movement
        if (mon->flags & MF_JUST_SUMMONED)
            continue;

        // XXX: Allow some spellcasting (like Healing and Teleport)? -- bwr
        // const bool healthy = (mon->hit_points * 2 > mon->max_hit_points);

        // This is the monster healing code, moved here from tag.cc:
        if (monster_descriptor( mon->type, MDSC_REGENERATES )
            || mon->type == MONS_PLAYER_GHOST)
        {   
            heal_monster( mon, turns, false );
        }
        else
        {   
            heal_monster( mon, (turns / 10), false );
        }

        if (turns >= 10)
            mon->timeout_enchantments( turns / 10 );

        // Summoned monsters might have disappeared
        if (mon->type == -1)
            continue;

        // Don't move non-land or stationary monsters around
        if (mons_habitat( mon->type ) != HT_LAND
            || mons_is_stationary( mon ))
        {
            continue;
        }

        // Let sleeping monsters lie
        if (mon->behaviour == BEH_SLEEP || mons_is_paralysed(mon))
            continue;

        const int range = (turns * mon->speed) / 10;
        const int moves = (range > 50) ? 50 : range;

        // const bool short_time = (range >= 5 + random2(10));
        const bool long_time  = (range >= (500 + roll_dice( 2, 500 )));

        const bool ranged_attack = (mons_has_ranged_spell( mon ) 
                                    || mons_has_ranged_attack( mon )); 

#if DEBUG_DIAGNOSTICS
        // probably too annoying even for DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "mon #%d: range %d; long %d; "
             "pos (%d,%d); targ %d(%d,%d); flags %ld", 
             m, range, long_time, mon->x, mon->y, 
             mon->foe, mon->target_x, mon->target_y, mon->flags );
#endif 

        if (range <= 0)
            continue;

        if (long_time 
            && (mon->behaviour == BEH_FLEE 
                || mon->behaviour == BEH_CORNERED
                || testbits( mon->flags, MF_BATTY )
                || ranged_attack
                || coinflip()))
        {
            if (mon->behaviour != BEH_WANDER)
            {
                mon->behaviour = BEH_WANDER;
                mon->foe = MHITNOT;
                mon->target_x = 10 + random2( GXM - 10 ); 
                mon->target_y = 10 + random2( GYM - 10 ); 
            }
            else 
            {
                // monster will be sleeping after we move it
                mon->behaviour = BEH_SLEEP; 
            }
        }
        else if (ranged_attack)
        {
            // if we're doing short time movement and the monster has a 
            // ranged attack (missile or spell), then the monster will
            // flee to gain distance if its "too close", else it will 
            // just shift its position rather than charge the player. -- bwr
            if (grid_distance(mon->x, mon->y, mon->target_x, mon->target_y) < 3)
            {
                mon->behaviour = BEH_FLEE;

                // if the monster is on the target square, fleeing won't work
                if (mon->x == mon->target_x && mon->y == mon->target_y)
                {
                    if (you.x_pos != mon->x || you.y_pos != mon->y)
                    {
                        // flee from player's position if different
                        mon->target_x = you.x_pos;
                        mon->target_y = you.y_pos;
                    }
                    else
                    {
                        // randomize the target so we have a direction to flee
                        mon->target_x += (random2(3) - 1);
                        mon->target_y += (random2(3) - 1);
                    }
                }

#if DEBUG_DIAGNOSTICS
                mpr( "backing off...", MSGCH_DIAGNOSTICS );
#endif
            }
            else
            {
                shift_monster( mon, mon->x, mon->y );

#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS, "shifted to (%d,%d)", mon->x, mon->y);
#endif
                continue;
            }
        }

        coord_def pos(mon->pos());
        // dirt simple movement:
        for (i = 0; i < moves; i++)
        {
            coord_def inc(mon->target_pos() - pos);
            inc = coord_def(sgn(inc.x), sgn(inc.y));

            if (mon->behaviour == BEH_FLEE)
                inc *= -1;

            if (pos.x + inc.x < 0 || pos.x + inc.x >= GXM)
                inc.x = 0;

            if (pos.y + inc.y < 0 || pos.y + inc.y >= GYM)
                inc.y = 0;

            if (inc.origin())
                break;

            const coord_def next(pos + inc);
            const dungeon_feature_type feat = grd(next);
            if (grid_is_solid(feat)
                || mgrd(next) != NON_MONSTER
                || !monster_habitable_grid(mon, feat))
                break;

            pos = next;
        }

        if (!shift_monster( mon, pos.x, pos.y ))
            shift_monster( mon, mon->x, mon->y );

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "moved to (%d,%d)", mon->x, mon->y );
#endif
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "total monsters on level = %d", mons_total );
#endif

    for (i = 0; i < MAX_CLOUDS; i++)
        delete_cloud( i );
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
    int cx, cy;

    if (elapsedTime <= 0.0)
        return;

    const long rot_time = static_cast<long>(elapsedTime / 20.0);

    for (int c = 0; c < MAX_ITEMS; c++)
    {
        item_def &it = mitm[c];
        
        if (!is_valid_item(it))
            continue;

        if (it.base_type != OBJ_CORPSES && it.base_type != OBJ_FOOD)
        {
            continue;
        }

        if (it.base_type == OBJ_CORPSES
            && it.sub_type > CORPSE_SKELETON)
        {
            continue;
        }

        if (it.base_type == OBJ_FOOD && it.sub_type != FOOD_CHUNK)
        {
            continue;
        }

        if (rot_time >= it.special && !is_being_butchered(it))
        {
            if (it.base_type == OBJ_FOOD)
            {
                destroy_item(c);
            }
            else
            {
                if (it.sub_type == CORPSE_SKELETON
                    || !mons_skeleton( it.plus ))
                {
                    destroy_item(c);
                }
                else
                {
                    it.sub_type = CORPSE_SKELETON;
                    it.special = 200;
                    it.colour = LIGHTGREY;
                }
            }
        }
        else
        {
            ASSERT(rot_time < 256);
            it.special -= rot_time;
        }
    }

    int fountain_checks = static_cast<int>(elapsedTime / 1000.0);
    if (random2(1000) < static_cast<int>(elapsedTime) % 1000)
        fountain_checks += 1;

    // dry fountains may start flowing again
    if (fountain_checks > 0)
    {
        for (cx=0; cx<GXM; cx++)
        {
            for (cy=0; cy<GYM; cy++)
            {
                if (grd[cx][cy] > DNGN_SPARKLING_FOUNTAIN
                    && grd[cx][cy] < DNGN_PERMADRY_FOUNTAIN)
                {
                    for (int i=0; i<fountain_checks; i++)
                    {
                        if (one_chance_in(100))
                        {
                            if (grd[cx][cy] > DNGN_SPARKLING_FOUNTAIN)
                                grd[cx][cy] =
                                    static_cast<dungeon_feature_type>(
                                        grd[cx][cy] - 1);
                                        
                            // clean bloody floor
                            if (is_bloodcovered(cx,cy))
                                env.map[cx][cy].property = FPROP_NONE;
                            // chance of cleaning adjacent squares
                            for (int k=-1; k<=1; k++)
                            {
                                 for (int l=-1; l<=1; l++)
                                      if (is_bloodcovered(cx+k,cy+l)
                                          && one_chance_in(5))
                                      {
                                          env.map[cx+k][cy+l].property = FPROP_NONE;
                                      }
                            }
                        }
                    }
                }
            }
        }
    }
}

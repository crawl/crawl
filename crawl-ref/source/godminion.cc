/**
 * @file
 * @brief Minions used for Demigods "abstract worshippers" mechanic.
**/

#include "AppHdr.h"

#include "godminion.h"
#include "mgen_data.h"

#include "externs.h"

#include "cloud.h"
#include "database.h"
#include "dungeon.h"
#include "godabil.h"
#include "godconduct.h"
#include "goditem.h"
#include "libutil.h"
#include "mapdef.h"
#include "maps.h"
#include "mgen_data.h"
#include "mon-cast.h"
#include "mon-util.h"
#include "mon-place.h"
#include "terrain.h"
#include "random.h"
#include "random-weight.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-util.h"
#include "spl-clouds.h"

#define DEMIGOD_BUILD_NUM_RACES 22

/**
 * There are two code paths to build minions. We can either pull a database string from monbuild.txt which must be in a standard MONS spec format;
 * or there is a purely code-based approach which is implemented here, using a list of player races and blacklists/whitelists to determine
 * the final composition.
 *
 * Note: When adding new gods there will always have to be some added support for the Demigod code. This should be kept to a minimum. Additional
 *       speech should be added in database/godminions.txt and minion speech in database/minionspeak.txt. These should both be optional but the
 *       generic god speech won't be very thematic.
 *       The key functions to which stuff must be added in this file are demigod_weight_race_for_god (if the god has specifically recommended races)
 *       and demigod_fixup_monster_for_god.
 *       You may also wish to make some of the god's conducts relevant (but not implemented yet).
 */
monster* demigod_build_minion(god_type which_god, int level) {

    // Make a list of valid minion races
    monster_type player_races[DEMIGOD_BUILD_NUM_RACES] = {
        MONS_HUMAN,
        MONS_ELF,  // Nearly a high elf
        MONS_DEEP_ELF_MAGE, // Will do for Deep Elf
        // MONS_SLUDGE_ELF - no equiv
        MONS_DEEP_DWARF,
        MONS_ORC,  // Rotate between other types of orc?
        MONS_MERFOLK,
        MONS_HALFLING,
        MONS_KOBOLD,
        MONS_SPRIGGAN,
        MONS_NAGA,
        MONS_CENTAUR,
        MONS_OGRE,  // Also ogre mage?
        MONS_TROLL,
        MONS_MINOTAUR,
        MONS_TENGU,
        MONS_DRACONIAN,
        MONS_DEMONSPAWN,
        MONS_MUMMY,
        MONS_GHOUL,
        MONS_VAMPIRE,
        MONS_FELID,
        MONS_OCTOPODE
    };

    // Get a weight value for each race based on the acting god
    std::vector<std::pair<monster_type, int> > mon_weights;
    for (int i = 0; i < DEMIGOD_BUILD_NUM_RACES; ++i) {
        mon_weights.push_back(std::make_pair(static_cast<monster_type>(player_races[i]), demigod_weight_race_for_god(which_god,player_races[i])));
    }
    // Randomly pick a race based on the weights
    monster_type chosen_race = *random_choose_weighted(mon_weights);

    // Perform a similar process for spells (with some extra adjustment based on minion level)
    std::vector<std::pair<spell_type, int> > spell_weights;
    int available_spells = 0;
    for (int i = 0; i < NUM_SPELLS; i++) {
        spell_type spell = static_cast<spell_type>(i);
        if (is_valid_mon_spell(spell)) {

            int w = demigod_weight_spell_for_god(which_god, chosen_race, spell);
            int l = spell_difficulty(spell);
            // TODO: Instead of this hard cutoff, adjust the weights smoothly so that high level spells are more likely for high level chars
            if (level < (l*3)) w = 0;
            // if (level > l*8) w = 0;
            if (w>0) {
                available_spells++;
                spell_weights.push_back(std::make_pair(spell, w));
            }
        }
    }
    // TODO: Adjust number of spells based on a) background b) god c) level
    int num_spells = random_range(3,6);

    // Generate the monster
    mgen_data mg(
        chosen_race,
        BEH_HOSTILE,
        0,
        0, 0,
        you.pos(), // TODO: move it a little farther from the player (does PROX_AWAY_FROM_PLAYER do this?)
        MHITNOT,
        0,
        which_god,
        MONS_NO_MONSTER,
        0,
        god_colour(which_god),
        level,
        PROX_CLOSE_TO_PLAYER,
        level_id::current(),
        0,
        0,
        0,
        "Foo",
        god_name(which_god,false),
        RANDOM_MONSTER);

    // demigod_fixup_monster_for_god(mg,which_god);

    // Create the minion
    monster *mon = create_monster(mg);
    if (mon) {
        // Setup monster details so we can check conducts for dying and fleeing
        mon->flags = mon->flags | MF_MINION;
        you.minion_mid = mon->mid;
        you.minion_timer_long = DEMIGOD_MINION_TIMER_LONG;
        you.minion_timer_short = DEMIGOD_MINION_TIMER_SHORT;

        monster_spells &spells(mon->spells);
        if (available_spells>0 && num_spells>0) {
            for (int i = 0; i < num_spells; ++i) {
                // TODO: Examine the spell - if it's this race's breath spell, skip and fill another slot
                // spell_type sp = spells[i];
                spells[i] = *random_choose_weighted(spell_weights);;
            }
        }

        // TODO: Add 1 or 2 cantrips
        // TODO: Give items

        // Also place cloud
        // TODO: make_a_normal_cloud no longer exists, is there a better way to create this cloud?
        // apply_area_cloud(make_a_normal_cloud, where, random_range(2,5), random_range(2,5), CLOUD_TLOC_ENERGY, &mons, 20, god_colour(which_god));
        return mon;
    }
    else {
        mpr("Error creating Minion!");
        return 0;
    }
}

/**
 * Return a weighting value for a given race to be a minion of a specified god
 */
int demigod_weight_race_for_god(god_type god, monster_type mon) {

    // This first part should be analogous to player_can_join_god in religion.cc

    // Good gods refuse evil races
    if (is_good_god(god) &&
        // monster_type == MONS_DEMONSPAWN ||
        (mons_class_holiness(mon) == MH_UNDEAD || mons_class_holiness(mon) == MH_DEMONIC))
        return 0;

    if (god == GOD_YREDELEMNUL && mons_class_flag(mon, M_ARTIFICIAL))
        return 0;

    // Fedhas hates undead, but will accept demonspawn.
    if (god == GOD_FEDHAS && mons_class_holiness(mon) == MH_UNDEAD)
        return 0;

    // TODO: Adjust preference for some god/race combos
    switch (god) {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_XOM:
        case GOD_VEHUMET:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_SIF_MUNA:
        case GOD_TROG:
        case GOD_NEMELEX_XOBEH:
        case GOD_ELYVILON:
        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
        case GOD_ASHENZARI:
            return 10;
        default:
            // TODO: ASSERT error?
            return 0;
    }

    return 10;
}

int demigod_weight_spell_for_god(god_type god, monster_type mon, spell_type spell) {

    switch (god) {
        case GOD_ZIN:
            // TODO: Modify monster speech when casting these to do something similar to Recite
            switch (spell) {
                case SPELL_CONFUSE:
                case SPELL_PARALYSE:
                case SPELL_SLEEP:
                    return 10;
                case SPELL_SILENCE:
                    return 5;
                case SPELL_TOMB_OF_DOROKLOHE: // Imprison / Sanctuary ...?
                    return 1;
                default:
                    return 0;
            }
        case GOD_SHINING_ONE:
            // TODO: Halo
            switch (spell) {
                case SPELL_HOLY_LIGHT:
                case SPELL_HOLY_FLAMES:
                case SPELL_HOLY_BREATH:
                    return 10;
                default:
                    return 0;
            }
        case GOD_KIKUBAAQUDGHA:
            return spell_typematch(spell, SPTYP_NECROMANCY) ? 10 : 0;
        case GOD_YREDELEMNUL:
            return (spell_typematch(spell, SPTYP_NECROMANCY) && spell_typematch(spell, SPTYP_SUMMONING)) ? 10 : 0;
        case GOD_XOM:
            // Xom likes all spells, but some even more so
            switch (spell) {
                case SPELL_SUMMON_BUTTERFLIES:
                case SPELL_SUMMON_TWISTER:
                case SPELL_PORKALATOR:
                case SPELL_CHAOS_BREATH:
                    return 50;
                default:
                    return 10;
            }
        case GOD_VEHUMET:
            return (is_player_spell(spell) && vehumet_supports_spell(spell)) ? 10 : 0;
        case GOD_OKAWARU:
            return 0;
        case GOD_MAKHLEB:
            switch (spell) {
                // TODO: Minor/Major destruction
                case SPELL_SUMMON_GREATER_DEMON:
                case SPELL_SUMMON_DEMON:
                case SPELL_DEMONIC_HORDE:
                    return 10;
                default:
                    return 0;
            }
        case GOD_SIF_MUNA:
            // Siffies can still use Conj but it's weighted lower. Otherwise they only use player spells.
            // TODO: Maybe Vehumet spells should use player_spell check too?
            return (vehumet_supports_spell(spell) ? 2 : (is_player_spell(spell) ? 10 : 0));
        case GOD_TROG:
            switch (spell) {
                case SPELL_TROGS_HAND:
                case SPELL_BERSERKER_RAGE:
                case SPELL_BROTHERS_IN_ARMS:
                    return 10;
                default:
                    return 0;
            }
        case GOD_NEMELEX_XOBEH:
            // TODO: Any spells with card-like effects. Or, can monsters use evokables anyway?
            return 0;
        case GOD_ELYVILON:
            switch (spell) {
                case SPELL_MINOR_HEALING:
                case SPELL_MAJOR_HEALING:
                case SPELL_HEAL_OTHER:
                    return 10;
                default:
                    return 0;
            }
        case GOD_FEDHAS:
            switch (spell) {
                // case SPELL_AWAKEN_FOREST:  -- if there were any trees around...
                case SPELL_SUNRAY:
                case SPELL_SUMMON_CANIFORMS:
                case SPELL_SUMMON_MUSHROOMS:
                    return 10;
                default:
                    return 0;
            }
            // TODO: Fedhasians summon oklobs
            return 0;
        case GOD_CHEIBRIADOS:
            // A selection of slowing spells
            // TODO: Slow the minion
            switch (spell) {
                case SPELL_SLOW:
                case SPELL_LEDAS_LIQUEFACTION:
                case SPELL_ENSNARE:
                    return 20;
                case SPELL_PARALYSE:
                    return 5;
                default:
                    // Low chance of other spells
                    return (is_player_spell(spell) && !is_hasty_spell(spell)) ? 2 : 0;
            }
        case GOD_ASHENZARI:
            // TODO: Ash minions should get player detection
            return is_player_spell(spell) ? 10 : 0;
        default:
            // TODO: ASSERT error?
            return 0;
    }

    return 0;
}
/*
void demigod_fixup_monster_for_god(mgen_data mg, god_type god) {

    switch (god) {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_XOM:
        case GOD_VEHUMET:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_SIF_MUNA:
        case GOD_TROG:
        case GOD_NEMELEX_XOBEH:
        case GOD_ELYVILON:
        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
        case GOD_ASHENZARI:
            break;
        default:
            break;
    }

}*/

// Minions dispatched after Demigods
bool demigod_dispatch_minion(god_type which_god, int level)
{
    monster* mons = demigod_build_minion(which_god,level);
    if (!mons) return false;

    you.minions_dispatched[which_god]++;

    // Generate a key for the minion lookup
    /*
    bool found = false;
    int find_level = level;
    std::string build = "";
    while(!found) {
        char buf[80];
        sprintf(buf, " minion %d", find_level);
        std::string god_name_key = god_name(which_god);
        // lowercase(god_name_key);
        // Get the build string
        build = getRandBuildString(god_name_key,buf);
        mprf("build string returned: %s", build.c_str());

        if (build.length() > 0) {
            found = true;
        }
        else {
            find_level--;
            if (find_level==0) return false;
        }
    }

    // Parse build string as monster list
    mons_list mlist;
    mlist.set_mons(0, build);

    // Get spec and fix some values
    mons_spec mspec = mlist.get_monster(0);
    mspec.mlevel = level;
    mspec.god = which_god;
    mspec.non_actor_summoner = god_name(which_god);

    // TODO: Some HD / HP scaling with level if they're set in the spec
    if (mspec.hd==0)
        mspec.hd = level;
    // TODO: Not a good way to place
    coord_def where = find_newmons_square_contiguous(mspec.monbase, you.pos(), 5);
    if (where.x==-1 || where.y==-1) return false;
    monster* mons = dgn_place_monster(mspec, level, where, true, true, false);
    if (!mons) return false;

    mons->foe = MHITYOU;

            */


    // God message ("Zin minion dispatched low/mid/high" or just "Zin minion dispatched")
    // TODO: Might want to spawn minions out of LOS and deliver this message when they're seen
    std::string which_god_name = god_name(which_god);
    std::string speak_key = which_god_name + " minion dispatched";
    mprf("god speech lookup for %s", speak_key.c_str());

    // Base message level on piety
    int prank = piety_rank() - 1;
    std::string tier = " low";
    if (prank>2) tier = " mid";
    if (prank>4) tier = " high";

    std::string speak_text = getSpeakString(speak_key + tier);
    if (speak_text.empty())
        speak_text = getSpeakString(speak_key);

    // Perform substitutions
    if (!speak_text.empty())
    {
        speak_text = replace_all(speak_text, "@The_monster@", mons->name(DESC_THE));
        speak_text = replace_all(speak_text, "@The_player@", you.name(DESC_PLAIN));
        speak_text = replace_all(speak_text, "@The_god@", god_name(which_god,true));
        // Speak
        god_speaks(which_god, speak_text.c_str());
    }

    return true;
    /* Following code will probably be deleted. But I'm keeping it around in case I end up using the switch.
     * TODO: Possible vault usage:
    std::string name = god_name(which_god);
    name = replace_all(name, " ", "_");
    lowercase(name);

    std::string maptag = make_stringf("minion_%s", name.c_str());
    const map_def *map = random_map_for_tag(maptag.c_str());

    const mons_spec *mspec = (map->mons).get_monster(0);
    */
    /*
    int id = create_monster(mg,true);
    // Name semi-unique
    if (id != -1)
        give_monster_proper_name(&menv[id],true);
    */
}

void demigod_handle_notoriety()
{
    if (you.minion_mid > 0) {
        // Already a minion in play; adjust timers
        you.minion_timer_long--;
        you.minion_timer_short--;

        mprf("Minion timers: %d %d",you.minion_timer_short,you.minion_timer_long);
        if (you.minion_timer_long<=0) {
            mprf("Demigod long timer expired");
            demigod_minion_timer_expired();
        }
        else if ( you.minion_timer_short<=0) {
            mprf("Demigod short timer expired");
            demigod_minion_timer_expired();
        }
        // TODO: Taunt messages when timer at low
    }
    else if (one_chance_in(20))
    {
        std::vector<std::pair<god_type, int> > wrath_weights;
        int total_notoriety = 0;
        for (int i = GOD_NO_GOD + 1; i < NUM_GODS; ++i)
        {
            if (you.notoriety[i] > 0 && i != GOD_SELF && i != GOD_JIYVA && i != GOD_BEOGH && i != GOD_LUGONU)
            {
                mprf("%s notoriety: %d",god_name(static_cast<god_type>(i)).c_str(),you.notoriety[i]);
                wrath_weights.push_back(std::make_pair(static_cast<god_type>(i), you.notoriety[i]));
                total_notoriety += you.notoriety[i];
            }
        }

        // With 20 notoriety with each god, gets a minion roughly every 100 god_time calls (which don't equate strictly to turns but I'm not sure how often)
        if (total_notoriety > 0 && wrath_weights.size() > 0 && x_chance_in_y(total_notoriety,2000)) {
            // Decide on a god to act and send a minion
            god_type which_god = *random_choose_weighted(wrath_weights);

            // Equiv level plus a modifier based on notoriety and piety (up to six levels)
            const int level = you.experience_level + (you.notoriety[which_god] * you.piety / 7400) + (random2(you.minions_dispatched[which_god]));
            if (demigod_dispatch_minion(which_god,level))
                // Reset penance
                dec_notoriety(which_god, you.notoriety[which_god]);
        }
    }
}

// Player has taken too long to kill the minion.
void demigod_minion_timer_expired()
{
    // If minion is on this level, remove it
    monster* minion = monster_by_mid(you.minion_mid);
    if (minion) {
        // Clouds
        // TODO: make_a_normal_cloud no longer exists, is there a better way to create this cloud?
        // apply_area_cloud(make_a_normal_cloud, minion->pos(), random_range(2,5), random_range(2,5), CLOUD_TLOC_ENERGY, minion, 20, god_colour(minion->god));
        monster_die(minion, KILL_RESET, NON_MONSTER);
    }

    // Reset mid anyway (if the minion was off-level, it'll be destroyed
    // instantly when you return to that level, based on its mid not matching)
    you.minion_mid = 0;

    // Conduct and response from gods
    did_god_conduct(DID_FLEE_GOD_MINION, 0, true, minion);
}

void dec_notoriety(god_type god, int val)
{
    if (val <= 0 || you.notoriety[god] <= 0)
        return;

    // TODO: dec_penance writes a message like "the god is mollified", but if we've had god speech when the minion lost we probably don't need this?
    you.notoriety[god] -= val;
    mprf("%s notoriety decreased: %d",god_name(god).c_str(),you.notoriety[god]);
}

void inc_notoriety(god_type god, int val)
{
    if (val <= 0)
        return;
    // TODO: messages on notoriety increasing?
    you.notoriety[god] += val;
    you.notoriety[god] = min((uint8_t)MAX_PENANCE, you.notoriety[god]);
    mprf("%s notoriety increased: %d",god_name(god).c_str(),you.notoriety[god]);
}

bool demigod_incur_wrath(god_type which_god, int amount)
{
    inc_notoriety(which_god, amount);
    // TODO: Flavour messages as particular gods become angry with you
    return true;
}

bool demigod_incur_wrath_all(int amount)
{
    for (int i=GOD_NO_GOD+1;i<NUM_GODS;i++) {
        god_type which_god = static_cast<god_type>(i);
        if (which_god != GOD_SELF)
            demigod_incur_wrath(which_god, amount);
    }
    return true;
}

/**
 * @file
 * @brief Divine retribution.
**/

#include "AppHdr.h"

#include "god-wrath.h"

#include <cmath>
#include <queue>
#include <sstream>

#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cleansing-flame-source-type.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "death-curse.h"
#include "decks.h"
#include "env.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-passive.h" // shadow_monster
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "losglobal.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "notes.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "traps.h" // is_valid_shaft_level
#include "transform.h"
#include "view.h"
#include "xom.h"

static void _god_smites_you(god_type god, const char *message = nullptr,
                            kill_method_type death_type = NUM_KILLBY);
static void _tso_blasts_cleansing_flame(const char *message = nullptr);

static const char *_god_wrath_adjectives[] =
{
    "bugginess",        // NO_GOD
    "wrath",            // Zin
    "wrath",            // the Shining One (unused)
    "malice",           // Kikubaaqudgha
    "anger",            // Yredelemnul
    "capriciousness",   // Xom
    "wrath",            // Vehumet
    "fury",             // Okawaru
    "fury",             // Makhleb
    "will",             // Sif Muna
    "fiery rage",       // Trog
    "wrath",            // Nemelex
    "displeasure",      // Elyvilon
    "touch",            // Lugonu
    "wrath",            // Beogh
    "all-consuming vengeance",  // Jiyva
    "enmity",           // Fedhas Madash
    "meddling",         // Cheibriados
    "doom",             // Ashenzari (unused)
    "darkness",         // Dithmenos
    "greed",            // Gozag (unused)
    "adversity",        // Qazlal
    "disappointment",   // Ru
#if TAG_MAJOR_VERSION == 34
    "progress",         // Pakellas
#endif
    "fury",             // Uskayaw
    "memory",           // Hepliaklqana (unused)
    "rancor",           // Wu Jian
    "fiery vengeance",  // Ignis
};
COMPILE_CHECK(ARRAYSZ(_god_wrath_adjectives) == NUM_GODS);

/**
 * Return a name associated with the given god's wrath.
 *
 * E.g., "the darkness of Dithmenos".
 * Used for cause of death messages (e.g. 'killed by a titan (summoned by the
 * wrath of Okawaru)')
 *
 * @param god   The god in question.
 * @return      A string name for the god's wrath.
 */
static string _god_wrath_name(god_type god)
{
    const bool use_full_name = god == GOD_FEDHAS      // fedhas is very formal.
                               || god == GOD_WU_JIAN; // apparently.

    return make_stringf("the %s of %s",
                        _god_wrath_adjectives[god],
                        god_name(god, use_full_name).c_str());
}

static mgen_data _wrath_mon_data(monster_type mtyp, god_type god)
{
    mgen_data mg = mgen_data::hostile_at(mtyp, true, you.pos())
                    .set_summoned(nullptr, 0, 0, god)
                    .set_non_actor_summoner(_god_wrath_name(god));
    mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
    return mg;
}

static bool _yred_random_zombified_hostile()
{
    const bool skel = one_chance_in(4);

    monster_type z_base;

    do
    {
        // XXX: better zombie selection?
        level_id place(BRANCH_DUNGEON,
                       min(27, you.experience_level + 5));
        z_base = pick_local_zombifiable_monster(place, RANDOM_MONSTER,
                                                you.pos());
    }
    while (skel && !mons_skeleton(z_base));

    mgen_data temp = _wrath_mon_data(skel ? MONS_SKELETON : MONS_ZOMBIE,
                                     GOD_YREDELEMNUL)
                     .set_base(z_base);

    return create_monster(temp, false);
}

static const vector<pop_entry> _okawaru_servants =
{ // warriors
  {  1,  3,   3, FALL, MONS_ORC },
  {  1,  3,   3, FALL, MONS_GNOLL },
  {  2,  6,   3, PEAK, MONS_OGRE },
  {  2,  6,   2, PEAK, MONS_GNOLL_SERGEANT },
  {  3,  7,   1, FLAT, MONS_TWO_HEADED_OGRE },
  {  3, 13,   3, PEAK, MONS_ORC_WARRIOR },
  {  5, 15,   3, PEAK, MONS_ORC_KNIGHT },
  {  5, 15,   2, PEAK, MONS_CYCLOPS },
  {  7, 21,   2, PEAK, MONS_CENTAUR_WARRIOR },
  {  7, 21,   2, PEAK, MONS_NAGA_WARRIOR },
  {  7, 21,   2, PEAK, MONS_TENGU_WARRIOR },
  {  7, 21,   1, FLAT, MONS_MERFOLK_IMPALER },
  {  7, 21,   1, FLAT, MONS_MERFOLK_JAVELINEER },
  {  7, 21,   1, FLAT, MONS_MINOTAUR },
  {  9, 27,   2, FLAT, MONS_STONE_GIANT },
  {  9, 27,   1, FLAT, MONS_DEEP_ELF_KNIGHT },
  {  9, 27,   1, FLAT, MONS_DEEP_ELF_ARCHER },
  { 11, 21,   1, FLAT, MONS_ORC_WARLORD },
  { 11, 27,   2, FLAT, MONS_FIRE_GIANT },
  { 11, 27,   2, FLAT, MONS_FROST_GIANT },
  { 13, 27,   1, FLAT, MONS_DEEP_ELF_BLADEMASTER },
  { 13, 27,   1, FLAT, MONS_DEEP_ELF_MASTER_ARCHER },
  { 13, 27,   1, FLAT, RANDOM_BASE_DRACONIAN },
  { 15, 27,   2, FLAT, MONS_TITAN },
};

static bool _okawaru_random_servant()
{
    monster_type mon_type = pick_monster_from(_okawaru_servants,
                                              you.experience_level);

    mgen_data temp = _wrath_mon_data(mon_type, GOD_OKAWARU);

    // Don't send dream sheep into battle, but otherwise let bands in.
    // This makes sure you get multiple orcs/gnolls early on.
    if (mon_type != MONS_CYCLOPS)
        temp.flags |= MG_PERMIT_BANDS;

    return create_monster(temp, false);
}

static bool _dithmenos_random_shadow(const int count, const int tier)
{
    monster_type mon_type = MONS_SHADOW;
    if (tier >= 2 && count == 0 && coinflip())
        mon_type = MONS_TZITZIMITL;
    else if (tier >= 1 && count < 3 && coinflip())
        mon_type = MONS_SHADOW_DEMON;

    return create_monster(_wrath_mon_data(mon_type, GOD_DITHMENOS), false);
}

/**
 * Summon divine warriors of the Shining One to punish the player.
 */
static void _tso_summon_warriors()
{
    bool success = false;
    int how_many = 1 + random2(you.experience_level / 5) + random2(3);

    for (; how_many > 0; --how_many)
    {
        if (summon_holy_warrior(100, true))
            success = true;
    }

    simple_god_message(success ? " sends the divine host to punish "
                       "you for your evil ways!"
                       : "'s divine host fails to appear.", GOD_SHINING_ONE);

}

/**
 * The Shining One shouts angrily to alert the player's foes!
 */
static void _tso_shouts()
{
    simple_god_message(" booms out: "
                       "\"Take the path of righteousness! REPENT!\"",
                       GOD_SHINING_ONE);
    noisy(25, you.pos()); // same as scroll of noise
}

/**
 * The Shining One silences the player!!
 */
static void _tso_squelches()
{
    god_speaks(GOD_SHINING_ONE,
               "You feel the Shining One's silent rage upon you!");
    cast_silence(25);
}

/**
 * Call down the wrath of the Shining One upon the player!
 *
 * Holy warriors/cleansing theme.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _tso_retribution()
{
    switch (random2(7))
    {
    case 0:
    case 1:
    case 2:
        _tso_summon_warriors();
        break;
    case 3:
    case 4:
        _tso_blasts_cleansing_flame();
        break;
    case 5:
        _tso_shouts();
        break;
    case 6:
        _tso_squelches();
        break;
    }
    return true;
}

static void _zin_remove_good_mutations()
{
    if (!you.how_mutated())
        return;

    const god_type god = GOD_ZIN;
    bool success = false;

    simple_god_message(" draws some chaos from your body!", god);

    bool failMsg = true;

    for (int i = 7; i >= 0; --i)
    {
        // Ensure that only good mutations are removed.
        if (i <= random2(10)
            && delete_mutation(RANDOM_GOOD_MUTATION, _god_wrath_name(god),
                               failMsg, false, true))
        {
            success = true;
        }
        else
            failMsg = false;
    }

    if (success && !you.how_mutated())
        simple_god_message(" rids your body of chaos!", god);
}

static bool _zin_retribution()
{
    // preaching/creeping doom theme
    const god_type god = GOD_ZIN;

    // If not mutated, do something else instead.
    const int punishment = you.how_mutated() ? random2(6) : random2(4);

    switch (punishment)
    {
    case 0:
    case 1:
    case 2: // recital
        simple_god_message(" recites the Axioms of Law to you!", god);
        switch (random2(3))
        {
        case 0:
            confuse_player(5 + random2(3));
            break;
        case 1:
            you.put_to_sleep(nullptr, 30 + random2(20));
            break;
        case 2:
            paralyse_player(_god_wrath_name(god));
            return false;
        }
        break;
    case 3: // noisiness
        simple_god_message(" booms out: \"Turn to the light! REPENT!\"", god);
        noisy(25, you.pos()); // same as scroll of noise
        break;
    case 4:
    case 5: // remove good mutations
        _zin_remove_good_mutations();
        break;
    }
    return true;
}

static bool _cheibriados_retribution()
{
    // time god/slowness theme
    const god_type god = GOD_CHEIBRIADOS;

    // Chei retribution might only make sense in combat.
    // We can crib some Xom code for this. {bh}
    int tension = get_tension(GOD_CHEIBRIADOS);
    int wrath_value = random2(tension);

    // Determine the level of wrath
    int wrath_type = 0;
    if (wrath_value < 2)
        wrath_type = 0;
    else if (wrath_value < 4)
        wrath_type = 1;
    else if (wrath_value < 8)
        wrath_type = 2;
    else if (wrath_value < 16)
        wrath_type = 3;
    else
        wrath_type = 4;

    // Strip away extra speed
    dec_haste_player(10000);

    switch (wrath_type)
    {
    // Very high tension wrath.
    // Add noise then start sleeping and slow the player with 2/3 chance.
    case 4:
        simple_god_message(" strikes the hour.", god);
        noisy(40, you.pos());
        dec_penance(god, 1); // and fall-through.
    // High tension wrath
    // Sleep the player and slow the player with 50% chance.
    case 3:
        mpr("You lose track of time.");
        you.put_to_sleep(nullptr, 30 + random2(20));
        if (one_chance_in(wrath_type - 1))
            break;
        else
            dec_penance(god, 1); // and fall-through.
    // Medium tension
    case 2:
        if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
        {
            mprf(MSGCH_WARN, "You feel the world leave you behind!");
            slow_player(100);
        }
        break;
    // Low/no tension; lose stats.
    case 1:
    case 0:
        mpr("Time shudders.");
        lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
        break;

    default:
        break;
    }

    return true;
}

static void _spell_retribution(monster* avatar, spell_type spell, god_type god,
                               const char* message = nullptr)
{
    simple_god_message(message ? message : " rains destruction down upon you!",
                       god);
    bolt beam;
    beam.source = you.pos();
    beam.target = you.pos();
    beam.aimed_at_feet = true;
    beam.source_name = avatar->mname;
    mons_cast(avatar, beam, spell, MON_SPELL_PRIEST, false);
}

/**
 * Choose a type of destruction with which to punish the player.
 *
 * @return A spell type to hurl at the player.
 */
static spell_type _makhleb_destruction_type()
{
    const int severity = min(random_range(you.experience_level / 14,
                                          you.experience_level / 9),
                             2);
    switch (severity)
    {
        case 0:
        default:
            // minor destruction
            return random_choose(SPELL_THROW_FLAME,
                                 SPELL_PAIN,
                                 SPELL_STONE_ARROW,
                                 SPELL_SHOCK,
                                 SPELL_SPIT_ACID);
        case 1:
            // major destruction
            return random_choose(SPELL_BOLT_OF_FIRE,
                                 SPELL_FIREBALL,
                                 SPELL_LIGHTNING_BOLT,
                                 SPELL_STICKY_FLAME,
                                 SPELL_IRON_SHOT,
                                 SPELL_BOLT_OF_DRAINING,
                                 SPELL_ORB_OF_ELECTRICITY);
        case 2:
            // legendary destruction (no IOOD because it doesn't really
            // work here)
            return random_choose(SPELL_FIREBALL,
                                 SPELL_LEHUDIBS_CRYSTAL_SPEAR,
                                 SPELL_ORB_OF_ELECTRICITY,
                                 SPELL_FLASH_FREEZE,
                                 SPELL_GHOSTLY_FIREBALL);
    }
}

/**
 * Create a fake 'avatar' monster representing a god, with which to hurl
 * destructive magic at foolish players.
 *
 * @param god           The god doing the wrath-hurling.
 * @return              An avatar monster, or nullptr if none could be set up.
 */
static monster* get_avatar(god_type god)
{
    // TODO: it would be better to abstract the fake monster code from both
    // this and shadow monster and possibly use different monster types --
    // doing it this way makes it easier for bugs where the two are conflated
    // to creep in
    monster* avatar = shadow_monster(false);
    if (!avatar)
        return nullptr;

    // shadow_monster() has the player's mid, which is no good here.
    avatar->set_new_monster_id();

    avatar->mname = _god_wrath_name(god);
    avatar->flags |= MF_NAME_REPLACE;
    avatar->attitude = ATT_HOSTILE;
    avatar->set_hit_dice(you.experience_level);

    return avatar;
}

/// Cleanup a temporary 'avatar' monster.
static void _reset_avatar(monster &avatar)
{
    env.mid_cache.erase(avatar.mid);
    shadow_monster_reset(&avatar);
}

/**
 * Rain down Makhleb's destruction upon the player!
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _makhleb_call_down_destruction()
{
    const god_type god = GOD_MAKHLEB;

    monster* avatar = get_avatar(god);
    // can't be const because mons_cast() doesn't accept const monster*

    if (avatar == nullptr)
    {
        simple_god_message(" has no time to deal with you just now.", god);
        return false; // not a very dazzling divine experience...
    }

    _spell_retribution(avatar, _makhleb_destruction_type(), god);
    _reset_avatar(*avatar);
    return true;
}

/**
 * Figure out how many greater servants (2s) an instance of Makhleb's wrath
 * should summon.
 *
 * @return The number of greater servants to be summoned by Makhleb's wrath.
 */
static int _makhleb_num_greater_servants()
{
    const int severity = 1 + you.experience_level / 2
                           + random2(you.experience_level / 2);

    if (severity > 13)
        return 2 + random2(you.experience_level / 5 - 2); // up to 6 at XL27
    else if (severity > 7 && !one_chance_in(5))
        return 1;
    return 0;
}

/**
 * Attempt to summon one of Makhleb's diabolical servants to punish the player.
 *
 * @param servant   The type of servant to be summoned.
 * @return          Whether the summoning was successful.
 */
static bool _makhleb_summon_servant(monster_type servant)
{
    return create_monster(_wrath_mon_data(servant, GOD_MAKHLEB), false);
}

/**
 * Unleash Makhleb's fiendish minions on the player!
 *
 * @return Whether to take further divine wrath actions afterward. (true.)
 */
static bool _makhleb_summon_servants()
{
    const int greater_servants = _makhleb_num_greater_servants();

    // up to 6 at XL25+
    const int total_servants = max(greater_servants,
                               1 + (random2(you.experience_level)
                                 + random2(you.experience_level)) / 10);
    const int lesser_servants = total_servants - greater_servants;

    int summoned = 0;

    for (int i = 0; i < greater_servants; i++)
    {
        const monster_type servant = random_choose(MONS_EXECUTIONER,
                                                   MONS_GREEN_DEATH,
                                                   MONS_BLIZZARD_DEMON,
                                                   MONS_BALRUG,
                                                   MONS_CACODEMON);
        if (_makhleb_summon_servant(servant))
            summoned++;
    }

    for (int i = 0; i < lesser_servants; i++)
    {
        const monster_type servant = random_choose(MONS_HELLWING,
                                                   MONS_NEQOXEC,
                                                   MONS_ORANGE_DEMON,
                                                   MONS_SMOKE_DEMON,
                                                   MONS_YNOXINUL);
        if (_makhleb_summon_servant(servant))
            summoned++;
    }

    simple_god_message(summoned > 1 ? " sends minions to punish you." :
                       summoned > 0 ? " sends a minion to punish you."
                       : "'s minions fail to arrive.", GOD_MAKHLEB);

    return true;

}

/**
 * Call down the wrath of Makhleb upon the player!
 *
 * Demonic servant theme.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _makhleb_retribution()
{
    if (coinflip())
        return _makhleb_call_down_destruction();
    else
        return _makhleb_summon_servants();
}

static int _count_corpses_in_los(vector<stack_iterator> *positions)
{
    int count = 0;

    for (radius_iterator rad(you.pos(), LOS_NO_TRANS, true); rad;
         ++rad)
    {
        if (actor_at(*rad))
            continue;

        for (stack_iterator stack_it(*rad); stack_it; ++stack_it)
        {
            if (stack_it->is_type(OBJ_CORPSES, CORPSE_BODY))
            {
                if (positions)
                    positions->push_back(stack_it);
                count++;
                break;
            }
        }
    }

    return count;
}

static bool _kikubaaqudgha_retribution()
{
    // death/necromancy theme
    const god_type god = GOD_KIKUBAAQUDGHA;

    god_speaks(god, coinflip() ? "You hear Kikubaaqudgha cackling."
                               : "Kikubaaqudgha's malice focuses upon you.");

    if (x_chance_in_y(you.experience_level, 27))
    {
        // torment, or 3 death curses of maximum power
        if (!you.res_torment())
            torment(nullptr, TORMENT_KIKUBAAQUDGHA, you.pos());
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                death_curse(you, nullptr,
                            _god_wrath_name(god), you.experience_level);
            }
        }
    }
    else if (random2(you.experience_level) >= 4)
    {
        // death curse, 25% chance of additional curse
        const int num_curses = one_chance_in(4) ? 2 : 1;
        for (int i = 0; i < num_curses; i++)
        {
                death_curse(you, nullptr,
                            _god_wrath_name(god), you.experience_level);
        }
    }

    return true;
}

static bool _yredelemnul_retribution()
{
    // undead theme
    const god_type god = GOD_YREDELEMNUL;

    if (coinflip())
    {
        if (you_worship(god) && coinflip() && yred_reclaim_souls())
            ;
        else
        {
            int how_many = 1 + random2avg(1 + (you.experience_level / 5), 2);
            int count = 0;

            for (; how_many > 0; --how_many)
            {
                if (one_chance_in(you.experience_level))
                {
                    if (_yred_random_zombified_hostile())
                        ++count;
                }
                else
                {
                    if (yred_random_servant(0, true))
                        ++count;
                    else
                        ++how_many;
                }
            }

            simple_god_message(count > 1 ? " sends servants to punish you." :
                               count > 0 ? " sends a servant to punish you."
                                         : "'s servants fail to arrive.", god);
        }
    }
    else
    {
        monster* avatar = get_avatar(god);
        // can't be const because mons_cast() doesn't accept const monster*

        if (avatar == nullptr)
        {
            simple_god_message(" has no time to deal with you just now.", god);
            return false;
        }

        _spell_retribution(avatar, SPELL_BOLT_OF_DRAINING, god,
                           "'s anger turns toward you for a moment.");
        _reset_avatar(*avatar);
    }

    return true;
}

static bool _trog_retribution()
{
    // physical/berserk theme
    const god_type god = GOD_TROG;

    if (coinflip())
    {
        int count = 0;
        int points = 3 + you.experience_level * 3;

        {
            msg::suppress msg;

            while (points > 0)
            {
                int cost =
                    min(min(random2avg((1 + you.experience_level / 3), 2) + 3,
                            10),
                        points);

                // quick reduction for large values
                if (points > 20 && coinflip())
                {
                    points -= 10;
                    cost = min(1 + div_rand_round(you.experience_level, 2), 10);
                }

                points -= cost;

                if (summon_berserker(cost * 20, 0))
                    count++;
            }
        }

        simple_god_message(count > 1 ? " sends monsters to punish you." :
                           count > 0 ? " sends a monster to punish you."
                                     : " has no time to punish you... now.",
                           god);
    }
    else if (!one_chance_in(3))
    {
        simple_god_message("'s voice booms out, \"Feel my wrath!\"", god);

        // A collection of physical effects that might be better
        // suited to Trog than wild fire magic... messages could
        // be better here... something more along the lines of apathy
        // or loss of rage to go with the anti-berserk effect-- bwr
        switch (random2(6))
        {
        case 0:
        case 1:
        case 2:
            lose_stat(STAT_STR, 1 + random2(you.strength() / 5));
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                mprf(MSGCH_WARN, "You suddenly pass out!");
                const int turns = 2 + random2(6);
                take_note(Note(NOTE_PARALYSIS, min(turns, 13), 0, "Trog"));
                you.increase_duration(DUR_PARALYSIS, turns, 13);
            }
            return false;

        case 4:
        case 5:
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                mprf(MSGCH_WARN, "You suddenly feel lethargic!");
                slow_player(100);
            }
            break;
        }
    }
    else
    {
        // A fireball is magic when used by a mortal but just a manifestation
        // of pure rage when used by a god. --ebering

        monster* avatar = get_avatar(god);
        // can't be const because mons_cast() doesn't accept const monster*

        if (avatar == nullptr)
        {
            simple_god_message(" has no time to deal with you just now.", god);
            return false; // not a very dazzling divine experience...
        }

        _spell_retribution(avatar, SPELL_FIREBALL,
                           god, " hurls fiery rage upon you!");
        _reset_avatar(*avatar);
    }

    return true;
}

static bool _beogh_retribution()
{
    // orcish theme
    const god_type god = GOD_BEOGH;

    switch (random2(8))
    {
    case 0: // smiting (25%)
    case 1:
        _god_smites_you(god);
        break;

    case 2: // send out one or two dancing weapons (12.5%)
    {
        int num_created = 0;
        int num_to_create = random_range(1, 2);

        for (int i = 0; i < num_to_create; ++i)
        {
            const int wpn_type =
                random_choose(WPN_CLUB,        WPN_MACE,      WPN_FLAIL,
                              WPN_MORNINGSTAR, WPN_DAGGER,    WPN_SHORT_SWORD,
                              WPN_LONG_SWORD,  WPN_SCIMITAR,  WPN_GREAT_SWORD,
                              WPN_HAND_AXE,    WPN_BATTLEAXE, WPN_SPEAR,
                              WPN_HALBERD);

            // Now create monster.
            if (monster *mon =
                create_monster(_wrath_mon_data(MONS_DANCING_WEAPON, god)))
            {
                ASSERT(mon->weapon() != nullptr);
                item_def& wpn(*mon->weapon());

                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_ELECTROCUTION);

                wpn.plus  = random2(3);
                wpn.sub_type = wpn_type;

                set_ident_flags(wpn, ISFLAG_KNOW_TYPE);

                item_colour(wpn);

                ghost_demon newstats;
                newstats.init_dancing_weapon(wpn,
                                             you.experience_level * 50 / 9);

                mon->set_ghost(newstats);
                mon->ghost_demon_init();

                num_created++;
            }
        }

        if (num_created > 0)
        {
            ostringstream msg;
            msg << " throws "
                << (num_created == 1 ? "an implement" : "implements")
                << " of electrocution at you.";
            simple_god_message(msg.str().c_str(), god);
            break;
        } // else fall through
    }
    case 3: // 25%, relatively harmless
    case 4: // in effect, only for penance
        if (you_worship(god) && beogh_followers_abandon_you())
            break;
        // else fall through
    default: // send orcs after you (3/8 to 5/8)
    {
        const int points = you.experience_level + 3
                           + random2(you.experience_level * 3);

        monster_type punisher;
        // "natural" bands
        if (points >= 30) // min: lvl 7, always: lvl 27
            punisher = MONS_ORC_WARLORD;
        else if (points >= 24) // min: lvl 6, always: lvl 21
            punisher = MONS_ORC_HIGH_PRIEST;
        else if (points >= 18) // min: lvl 4, always: lvl 15
            punisher = MONS_ORC_KNIGHT;
        else if (points > 10) // min: lvl 3, always: lvl 8
            punisher = MONS_ORC_WARRIOR;
        else
            punisher = MONS_ORC;

        mgen_data temp = _wrath_mon_data(punisher, god);
        temp.flags |=  MG_PERMIT_BANDS;

        monster *mons = create_monster(temp, false);

        // sometimes name band leader
        if (mons && one_chance_in(3))
            give_monster_proper_name(*mons);

        simple_god_message(
            mons ? " sends forth an army of orcs."
                 : " is still gathering forces against you.", god);
    }
    }

    return true;
}

static bool _okawaru_retribution()
{
    // warrior theme
    const god_type god = GOD_OKAWARU;

    int how_many = 1 + (you.experience_level / 5);
    int count = 0;

    for (; how_many > 0; --how_many)
        count += _okawaru_random_servant();

    simple_god_message(count > 0 ? " sends forces against you!"
                                 : "'s forces are busy with other wars.", god);

    return true;
}

static bool _sif_muna_retribution()
{
    // magic/intelligence theme
    const god_type god = GOD_SIF_MUNA;

    simple_god_message("'s wrath finds you.", god);

    switch (random2(10))
    {
    case 0:
    case 1:
    case 2:
        lose_stat(STAT_INT, 1 + random2(you.intel() / 5));
        break;

    case 3:
    case 4:
    case 5:
        confuse_player(5 + random2(3));
        break;

    case 6:
    case 7:
    case 8:
        if (you.magic_points > 0)
        {
            drain_mp(you.magic_points);
            canned_msg(MSG_MAGIC_DRAIN);
        }
        break;

    case 9:
        // This will set all the extendable duration spells to
        // a duration of one round, thus potentially exposing
        // the player to real danger.
        debuff_player();
        break;
    }

    return true;
}

/**
 * Perform translocation-flavored Lugonu retribution.
 *
 * 25% banishment; 50% teleport near monsters.
 */
static void _lugonu_transloc_retribution()
{
    const god_type god = GOD_LUGONU;

    if (coinflip())
    {
        // Give extra opportunities for embarrassing teleports.
        simple_god_message("'s wrath scatters you!", god);
        you_teleport_now(false, true, "Space warps around you!");
    }
    else if (coinflip())
    {
        simple_god_message(" draws you home!", god);
        you.banish(nullptr, "Lugonu's touch", you.get_experience_level(), true);
    }
}

/**
 * Summon Lugonu's minions to punish the player.
 *
 * Possibly a major minion with pals, possibly just some riff-raff, depending
 * on level & chance.
 */
static void _lugonu_minion_retribution()
{
    // abyssal servant theme
    const god_type god = GOD_LUGONU;

    // should we summon more & higher-tier lugonu minions?
    // linear chance, from 0% at xl 4 to 80% at xl 16
    const bool major = (you.experience_level > (4 + random2(12))
                        && !one_chance_in(5));

    // how many lesser minions should we try to summon?
    // if this is major wrath, summon a few minions; 0 below xl9, 0-3 at xl 27.
    // otherwise, summon exactly (!) 1 + xl/7 minions, maxing at 4 at xl 21.
    const int how_many = (major ? random2(you.experience_level / 9 + 1)
                                : 1 + you.experience_level / 7);

    // did we successfully summon any minions? (potentially set true below)
    bool success = false;

    for (int i = 0; i < how_many; ++i)
    {
        // try to summon a few minor minions...
        // weight toward large abominations, and away from small ones, at
        // higher levels
        const monster_type to_summon =
            random_choose_weighted(
                15 - (you.experience_level/2),  MONS_ABOMINATION_SMALL,
                you.experience_level/2,         MONS_ABOMINATION_LARGE,
                6,                              MONS_THRASHING_HORROR,
                3,                              MONS_ANCIENT_ZYME
            );

        if (create_monster(_wrath_mon_data(to_summon, god), false))
            success = true;
    }

    if (major)
    {
        // try to summon one nasty monster.
        const monster_type to_summon = random_choose(MONS_TENTACLED_STARSPAWN,
                                                     MONS_WRETCHED_STAR,
                                                     MONS_STARCURSED_MASS);

        if (create_monster(_wrath_mon_data(to_summon, god), false))
            success = true;
    }

    simple_god_message(success ? " sends minions to punish you."
                               : "'s minions fail to arrive.", god);
}

/**
 * Call down the wrath of Lugonu upon the player!
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _lugonu_retribution()
{
    _lugonu_transloc_retribution();
    _lugonu_minion_retribution();

    return true;
}



/**
 * Choose a type of destruction with which to punish the player.
 *
 * @return A spell type to hurl at the player.
 */
static spell_type _vehumet_wrath_type()
{
    const int severity = min(random_range(1 + you.experience_level / 5,
                                          1 + you.experience_level / 3),
                             9);
    // Mostly player-castable conjurations with a couple of additions.
    switch (severity)
    {
        case 1:
            return random_choose(SPELL_MAGIC_DART,
                                 SPELL_STING,
                                 SPELL_SHOCK,
                                 SPELL_FLAME_TONGUE);
        case 2:
            return random_choose(SPELL_THROW_FLAME,
                                 SPELL_THROW_FROST);
        case 3:
            return random_choose(SPELL_MEPHITIC_CLOUD,
                                 SPELL_STONE_ARROW);
        case 4:
            return random_choose(SPELL_STICKY_FLAME,
                                 SPELL_THROW_ICICLE,
                                 SPELL_ENERGY_BOLT);
        case 5:
            return random_choose(SPELL_FIREBALL,
                                 SPELL_LIGHTNING_BOLT,
                                 SPELL_BOLT_OF_MAGMA,
                                 SPELL_VENOM_BOLT,
                                 SPELL_BOLT_OF_DRAINING,
                                 SPELL_QUICKSILVER_BOLT,
                                 SPELL_METAL_SPLINTERS);
        case 6:
            return random_choose(SPELL_BOLT_OF_FIRE,
                                 SPELL_BOLT_OF_COLD,
                                 SPELL_CORROSIVE_BOLT,
                                 SPELL_FREEZING_CLOUD,
                                 SPELL_POISONOUS_CLOUD,
                                 SPELL_POISON_ARROW,
                                 SPELL_IRON_SHOT,
                                 SPELL_CONJURE_BALL_LIGHTNING);
        case 7:
            return random_choose(SPELL_ORB_OF_ELECTRICITY,
                                 SPELL_FLASH_FREEZE);
        case 8:
            return SPELL_LEHUDIBS_CRYSTAL_SPEAR;
        case 9:
            return SPELL_FIRE_STORM;
        default:
            return SPELL_NO_SPELL;
    }
}

/**
 * Call down the wrath of Vehumpet upon the player!
 *
 * Conjuration theme.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _vehumet_retribution()
{
    const god_type god = GOD_VEHUMET;

    monster* avatar = get_avatar(god);
    if (!avatar)
    {
        simple_god_message(" has no time to deal with you just now.", god);
        return false;
    }

    const spell_type spell = _vehumet_wrath_type();
    if (spell == SPELL_NO_SPELL)
    {
        simple_god_message(" has no time to deal with you just now.", god);
        _reset_avatar(*avatar);
        return false;
    }

    _spell_retribution(avatar, spell, god);
    _reset_avatar(*avatar);
    return true;
}

static bool _nemelex_retribution()
{
    // card theme
    const god_type god = GOD_NEMELEX_XOBEH;

    simple_god_message(" makes you draw from the deck of Punishment.", god);
    draw_from_deck_of_punishment();
    return true;
}

/**
 * Let Jiyva throw a few malmutations the player's way.
 */
static void _jiyva_mutate_player()
{
    const god_type god = GOD_JIYVA;
    god_speaks(god, "You feel Jiyva alter your body.");

    const int mutations = 1 + random2(3);
    for (int i = 0; i < mutations; ++i)
        mutate(RANDOM_BAD_MUTATION, _god_wrath_name(god), true, false, true);
}

static void _jiyva_remove_slime_mutation()
{
    bool slimy = false;
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (is_slime_mutation(static_cast<mutation_type>(i))
            && you.has_mutation(static_cast<mutation_type>(i)))
        {
            slimy = true;
        }
    }

    if (!slimy)
        return;

    const god_type god = GOD_JIYVA;
    simple_god_message("'s gift of slime is revoked.", god);
    delete_mutation(RANDOM_SLIME_MUTATION, _god_wrath_name(god),
                    true, false, true);
}

/**
 * Make Jiyva polymorph the player into a bad form.
 */
static void _jiyva_transform()
{
    const god_type god = GOD_JIYVA;
    god_speaks(god, "Mutagenic energy floods into your body!");

    const transformation form = random_choose(transformation::bat,
                                              transformation::fungus,
                                              transformation::pig,
                                              transformation::tree,
                                              transformation::wisp);

    if (transform(random2(you.penance[god]) * 2, form, true))
        you.transform_uncancellable = true;
}
/**
 * Make Jiyva contaminate tha player.
 */
static void _jiyva_contaminate()
{
    const god_type god = GOD_JIYVA;
    god_speaks(god, "Mutagenic energy floods into your body!");
    contaminate_player(random2(you.penance[god] * 500));
}

static void _jiyva_summon_slimes()
{
    const god_type god = GOD_JIYVA;

    const monster_type slimes[] =
    {
        MONS_FLOATING_EYE,
        MONS_EYE_OF_DEVASTATION,
        MONS_GREAT_ORB_OF_EYES,
        MONS_SHINING_EYE,
        MONS_GLOWING_ORANGE_BRAIN,
        MONS_JELLY,
        MONS_ROCKSLIME,
        MONS_QUICKSILVER_OOZE,
        MONS_ACID_BLOB,
        MONS_AZURE_JELLY,
        MONS_SLIME_CREATURE,
    };

    const int how_many = 1 + (you.experience_level / 10) + random2(3);
    bool success = false;

    for (int i = 0; i < how_many; i++)
    {
        const monster_type slime = RANDOM_ELEMENT(slimes);

        if (create_monster(_wrath_mon_data(slime, god), false))
            success = true;
    }

    god_speaks(god, success ? "Some slimes ooze up out of the ground!"
                            : "The ground quivers slightly.");
}

/**
 * Call down the wrath of Jiyva upon the player!
 *
 * Mutations and slime theme.
 *
 * @return Whether to take further divine wrath actions afterward. (true.)
 */
static bool _jiyva_retribution()
{
    const god_type god = GOD_JIYVA;

    if (you.can_safely_mutate() && one_chance_in(7))
        _jiyva_mutate_player();
    else if (one_chance_in(3) && !you.transform_uncancellable)
        _jiyva_transform();
    else if (!one_chance_in(3) || you_worship(god))
        _jiyva_contaminate();
    else
        _jiyva_summon_slimes();

    if (coinflip())
        _jiyva_remove_slime_mutation();

    return true;
}

/**
 * Let Fedhas call down the enmity of nature upon the player!
 * Equal chance corrosive bolt, primal wave (a throwback to rain),
 * or thorn volley
 */
static void _fedhas_nature_retribution()
{
    const god_type god = GOD_FEDHAS;

    monster* avatar = get_avatar(god);
    // can't be const because mons_cast() doesn't accept const monster*

    if (avatar == nullptr)
    {
        simple_god_message(" has no time to deal with you just now.", god);
        return;
    }

    spell_type spell = random_choose(SPELL_CORROSIVE_BOLT,
                                     SPELL_PRIMAL_WAVE,
                                     SPELL_THORN_VOLLEY);

    _spell_retribution(avatar, spell, god, " invokes nature against you.");
    _reset_avatar(*avatar);
}

// Collect lists of points that are within LOS (under the given env map),
// unoccupied, and not solid (walls/statues).
static void _collect_radius_points(vector<vector<coord_def> > &radius_points,
                                   const coord_def &origin, los_type los)
{
    radius_points.clear();
    radius_points.resize(LOS_RADIUS);

    // Just want to associate a point with a distance here for convenience.
    typedef pair<coord_def, int> coord_dist;

    // Using a priority queue because squares don't make very good circles at
    // larger radii. We will visit points in order of increasing euclidean
    // distance from the origin (not path distance). We want a min queue
    // based on the distance, so we use greater_second as the comparator.
    priority_queue<coord_dist, vector<coord_dist>,
                   greater_second<coord_dist> > fringe;

    fringe.push(coord_dist(origin, 0));

    set<int> visited_indices;

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

        // We don't include radius 0. This is also a good place to check if
        // the squares are already occupied since we want to search past
        // occupied squares but don't want to consider them valid targets.
        if (current.second && !actor_at(current.first))
            radius_points[current_r - 1].push_back(current.first);

        for (adjacent_iterator i(current.first); i; ++i)
        {
            coord_dist temp(*i, current.second);

            // If the grid is out of LOS, skip it.
            if (!cell_see_cell(origin, temp.first, los))
                continue;

            coord_def local = temp.first - origin;

            temp.second = local.abs();

            idx = temp.first.x + temp.first.y * X_WIDTH;

            if (!visited_indices.count(idx)
                && in_bounds(temp.first)
                && !cell_is_solid(temp.first))
            {
                fringe.push(temp);
            }
        }

    }
}

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

static int _place_ring(vector<coord_def> &ring_points,
                       const coord_def &origin, mgen_data prototype,
                       int n_arcs, int arc_occupancy, int &seen_count)
{
    shuffle_array(ring_points);

    int target_amount = ring_points.size();
    int spawned_count = 0;
    seen_count = 0;

    vector<int> arc_counts(n_arcs, arc_occupancy);

    for (unsigned i = 0;
         spawned_count < target_amount && i < ring_points.size();
         i++)
    {
        int direction = _arc_decomposition(ring_points.at(i)
                                           - origin, n_arcs);

        if (arc_counts[direction]-- <= 0)
            continue;

        prototype.pos = ring_points.at(i);

        if (create_monster(prototype, false))
        {
            spawned_count++;
            if (you.see_cell(ring_points.at(i)))
                seen_count++;
        }
    }

    return spawned_count;
}

template<typename T>
static bool less_second(const T & left, const T & right)
{
    return left.second < right.second;
}

typedef pair<coord_def, int> point_distance;

// Find the distance from origin to each of the targets, those results
// are stored in distances (which is the same size as targets). Exclusion
// is a set of points which are considered disconnected for the search.
static void _path_distance(const coord_def& origin,
                           const vector<coord_def>& targets,
                           set<int> exclusion,
                           vector<int>& distances)
{
    queue<point_distance> fringe;
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
                              && !temp->is_stationary()))
                {
                    fringe.push(point_distance(*adj_it, current.second+1));
                }
            }
        }
    }
}

// Find the minimum distance from each point of origin to one of the targets
// The distance is stored in 'distances', which is the same size as origins.
static void _point_point_distance(const vector<coord_def>& origins,
                                  const vector<coord_def>& targets,
                                  vector<int>& distances)
{
    distances.clear();
    distances.resize(origins.size(), INT_MAX);

    // Consider all points of origin as blocked (you can search outward
    // from one, but you can't form a path across a different one).
    set<int> base_exclusions;
    for (coord_def c : origins)
    {
        int idx = c.x + c.y * X_WIDTH;
        base_exclusions.insert(idx);
    }

    vector<int> current_distances;
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

// So the idea is we want to decide which adjacent tiles are in the most
// 'danger' We claim danger is proportional to the minimum distances from the
// point to a (hostile) monster. This function carries out at most 7 searches
// to calculate the distances in question.
static bool _prioritise_adjacent(const coord_def &target,
                                 vector<coord_def>& candidates)
{
    radius_iterator los_it(target, LOS_NO_TRANS, true);

    vector<coord_def> mons_positions;
    // collect hostile monster positions in LOS
    for (; los_it; ++los_it)
    {
        monster* hostile = monster_at(*los_it);

        if (hostile && hostile->attitude == ATT_HOSTILE
            && you.can_see(*hostile))
        {
            mons_positions.push_back(hostile->pos());
        }
    }

    if (mons_positions.empty())
    {
        shuffle_array(candidates);
        return true;
    }

    vector<int> distances;

    _point_point_distance(candidates, mons_positions, distances);

    vector<point_distance> possible_moves(candidates.size());

    for (unsigned i = 0; i < possible_moves.size(); ++i)
    {
        possible_moves[i].first  = candidates[i];
        possible_moves[i].second = distances[i];
    }

    sort(possible_moves.begin(), possible_moves.end(),
              less_second<point_distance>);

    for (unsigned i = 0; i < candidates.size(); ++i)
        candidates[i] = possible_moves[i].first;

    return true;
}

/**
 * Summon Fedhas's oklobs & mushrooms around the player.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _fedhas_summon_plants()
{
    const god_type god = GOD_FEDHAS;
    bool success = false;

    // We are going to spawn some oklobs but first we need to find
    // out a little about the situation.
    vector<vector<coord_def> > radius_points;
    _collect_radius_points(radius_points, you.pos(), LOS_NO_TRANS);

    int max_idx = 3;
    unsigned max_points = radius_points[max_idx].size();

    for (unsigned i = max_idx + 1; i < radius_points.size(); i++)
    {
        if (radius_points[i].size() > max_points)
        {
            max_points = radius_points[i].size();
            max_idx = i;
        }
    }

    mgen_data temp = _wrath_mon_data(MONS_OKLOB_PLANT, god);

    // If we have a lot of space to work with we can do something
    // flashy.
    if (radius_points[max_idx].size() > 24)
    {
        int seen_count;

        temp.cls = MONS_PLANT;

        _place_ring(radius_points[0], you.pos(), temp, 1,
                radius_points[0].size(), seen_count);

        if (seen_count > 0)
            success = true;

        temp.cls = MONS_OKLOB_PLANT;

        _place_ring(radius_points[max_idx], you.pos(), temp,
                random_range(3, 8), 1, seen_count);

        if (seen_count > 0)
            success = true;
    }
    // Otherwise we do something with the nearest neighbors
    // (assuming the player isn't already surrounded).
    else if (!radius_points[0].empty())
    {
        unsigned target_count = random_range(2, 8);
        if (target_count < radius_points[0].size())
            _prioritise_adjacent(you.pos(), radius_points[0]);
        else
            target_count = radius_points[0].size();

        for (unsigned i = radius_points[0].size() - target_count;
             i < radius_points[0].size(); ++i)
        {
            temp.pos = radius_points[0].at(i);
            temp.cls = coinflip() ? MONS_WANDERING_MUSHROOM
                                  : MONS_OKLOB_PLANT;

            if (create_monster(temp, false))
                success = true;
        }
    }

    if (success)
    {
        god_speaks(god, "Plants grow around you in an ominous manner.");
        return false;
    }

    return true;
}

static int _fedhas_corpse_spores(beh_type attitude)
{
    vector<stack_iterator> positions;
    int count = _count_corpses_in_los(&positions);
    ASSERT(attitude != BEH_FRIENDLY || count > 0);

    if (count == 0)
        return count;

    for (const stack_iterator &si : positions)
    {
        count++;

        if (monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE_SPORE,
                                               attitude,
                                               si->pos,
                                               MHITNOT,
                                               MG_FORCE_PLACE,
                                               GOD_FEDHAS)
                                            .set_summoned(&you, 0, 0)))
        {
            plant->flags |= MF_NO_REWARD;

            if (attitude == BEH_FRIENDLY)
            {
                plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                mons_make_god_gift(*plant, GOD_FEDHAS);

                plant->behaviour = BEH_WANDER;
                plant->foe = MHITNOT;
            }
        }

        if (mons_skeleton(si->mon_type))
            turn_corpse_into_skeleton(*si);
        else
        {
            item_was_destroyed(*si);
            destroy_item(si->index());
        }
    }

    viewwindow(false);
    update_screen();

    return count;
}

/**
 * Call down the wrath of Fedhas upon the player!
 *
 * Plants and plant/nature themed attacks.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _fedhas_retribution()
{
    const god_type god = GOD_FEDHAS;

    // We have 3 forms of retribution, but players under penance will be
    // spared the 'you are now surrounded by oklob plants, please die' one.
    const int retribution_options = you_worship(god) ? 2 : 3;

    switch (random2(retribution_options))
    {
    case 0:
        // Try and spawn some hostile ballistomycete spores, if none are created
        // fall through to the elemental miscast effects.
        if (_fedhas_corpse_spores(BEH_HOSTILE))
        {
            simple_god_message(" produces spores.", god);
            return true;
        }

    case 1:
    default:
        _fedhas_nature_retribution();
        return true;

    case 2:
        return _fedhas_summon_plants();
    }
}

static bool _dithmenos_retribution()
{
    // shadow theme
    const god_type god = GOD_DITHMENOS;

    switch (random2(4))
    {
    case 0:
    {
        int count = 0;
        int how_many = 3 + random2avg(div_rand_round(you.experience_level, 3),
                                      2);
        const int tier = div_rand_round(you.experience_level, 9);
        while (how_many-- > 0)
        {
            if (_dithmenos_random_shadow(count, tier))
                count++;
        }
        simple_god_message(count ? " calls forth shadows to punish you."
                                 : " fails to incite the shadows against you.",
                           god);
        break;
    }
    case 1:
    {
        int count = 0;
        int how_many = 2 + random2avg(div_rand_round(you.experience_level, 4),
                                      4);
        for (int i = 0; i < how_many; ++i)
        {
            if (create_monster(
                    mgen_data(
                        RANDOM_MOBILE_MONSTER, BEH_HOSTILE, you.pos(), MHITYOU,
                              MG_NONE, god)
                            .set_place(level_id(BRANCH_DUNGEON,
                                                min(27,
                                                    you.experience_level + 5)))
                            .set_summoned(nullptr, 4, MON_SUMM_WRATH)
                            .set_non_actor_summoner(_god_wrath_name(god))))
            {
                count++;
            }
        }
        simple_god_message(count ? " weaves monsters from the shadows."
                                 : " fails to weave monsters from the shadows.",
                           god);
        break;
    }
    case 2:
    {
        // This is possibly kind of underwhelming?
        god_speaks(god, "You feel overwhelmed by the shadows around you.");
        you.put_to_sleep(nullptr, 30 + random2(20));
        break;
    }
    case 3:
        simple_god_message(" tears the shadows away from you.", god);
        you.sentinel_mark();
        break;
    }
    return true;
}

static const vector<pop_entry> pop_qazlal_wrath =
{
  {  0, 12, 25, SEMI, MONS_AIR_ELEMENTAL },
  {  4, 12, 50, FLAT, MONS_WIND_DRAKE },
  { 10, 22, 50, SEMI, MONS_SPARK_WASP },
  { 18, 27, 50, RISE, MONS_STORM_DRAGON },

  {  0, 12, 25, SEMI, MONS_FIRE_ELEMENTAL },
  {  4, 12, 50, FLAT, MONS_FIRE_CRAB },
  {  8, 16, 30, FLAT, MONS_LINDWURM },
  { 12, 27, 50, SEMI, MONS_FIRE_DRAGON },

  {  0, 12, 25, SEMI, MONS_WATER_ELEMENTAL },
  {  2, 10, 50, FLAT, MONS_RIME_DRAKE },
  { 12, 27, 50, SEMI, MONS_ICE_DRAGON },
  { 20, 27, 30, RISE, MONS_SHARD_SHRIKE },

  {  0, 12, 25, SEMI, MONS_EARTH_ELEMENTAL },
  {  2, 10, 50, FLAT, MONS_BASILISK },
  {  4, 14, 30, FLAT, MONS_BOULDER_BEETLE },
  { 18, 27, 50, RISE, MONS_IRON_DRAGON },
};

/**
 * Summon elemental creatures to destroy the player!
 */
static void _qazlal_summon_elementals()
{
    const god_type god = GOD_QAZLAL;

    const int how_many = 1 + div_rand_round(you.experience_level, 7);
    bool success = false;

    for (int i = 0; i < how_many; i++)
    {
        monster_type mon = pick_monster_from(pop_qazlal_wrath,
                                             you.experience_level);

        if (create_monster(_wrath_mon_data(mon, god), false))
            success = true;
    }

    if (success)
        simple_god_message(" incites the elements against you!", god);
    else
        simple_god_message(" fails to incite the elements against you.", god);
}

/**
 * Give the player temporary elemental-vulnerability mutations.
 */
static void _qazlal_elemental_vulnerability()
{
    const god_type god = GOD_QAZLAL;

    if (mutate(RANDOM_QAZLAL_MUTATION, _god_wrath_name(god), false,
               false, true, false, MUTCLASS_TEMPORARY))
    {
        simple_god_message(" strips away your elemental protection.",
                           god);
    }
    else
    {
        simple_god_message(" fails to strip away your elemental protection.",
                           god);
    }
}

/**
 * Call down the wrath of Qazlal upon the player!
 *
 * Disaster/elemental theme.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _qazlal_retribution()
{
    if (coinflip())
    {
        simple_god_message(" causes a mighty clap of thunder!",
                           GOD_QAZLAL);
        noisy(25, you.pos());
    }

    if (coinflip())
        _qazlal_summon_elementals();
    else
        _qazlal_elemental_vulnerability();

    return true;
}

static bool _choose_hostile_monster(const monster& mon)
{
    return mon.attitude == ATT_HOSTILE;
}

static int _wu_jian_summon_weapons()
{
    god_type god = GOD_WU_JIAN;
    const int num = 3 + random2(3);
    int created = 0;

    for (int i = 0; i < num; ++i)
    {
        const int subtype = random_choose(WPN_DIRE_FLAIL, WPN_QUARTERSTAFF,
                                          WPN_BROAD_AXE, WPN_GREAT_SWORD,
                                          WPN_RAPIER, WPN_GLAIVE);
        const int ego = random_choose(SPWPN_VORPAL, SPWPN_FLAMING,
                                      SPWPN_FREEZING, SPWPN_ELECTROCUTION,
                                      SPWPN_SPEED);

        if (monster *mon =
            create_monster(_wrath_mon_data(MONS_DANCING_WEAPON, god)))
        {
            ASSERT(mon->weapon() != nullptr);
            item_def& wpn(*mon->weapon());

            set_item_ego_type(wpn, OBJ_WEAPONS, ego);

            wpn.plus = random2(5);
            wpn.sub_type = subtype;

            set_ident_flags(wpn, ISFLAG_KNOW_TYPE);

            item_colour(wpn);

            ghost_demon newstats;
            newstats.init_dancing_weapon(wpn, you.experience_level * 50 / 9);

            mon->set_ghost(newstats);
            mon->ghost_demon_init();

            created++;
        }
    }

    return created;
}

static bool _wu_jian_retribution()
{
    god_type god = GOD_WU_JIAN;

    if (_wu_jian_summon_weapons())
    {
        switch (random2(4))
        {
        case 0:
            wu_jian_sifu_message(" says: Die by a thousand cuts!");
            you.set_duration(DUR_BARBS, random_range(5, 10));
            break;
        case 1:
            wu_jian_sifu_message(" whispers: Nowhere to run...");
            you.set_duration(DUR_SLOW, random_range(5, 10));
            break;
        case 2:
            wu_jian_sifu_message(" whispers: These will loosen your tongue!");
            you.increase_duration(DUR_SILENCE, 5 + random2(11), 50);
            invalidate_agrid(true);
            break;
        case 3:
            wu_jian_sifu_message(" says: Suffer, mortal!");
            you.corrode_equipment(_god_wrath_name(god).c_str(), 2);
            break;
        }
    }
    else
        simple_god_message("'s divine weapons fail to arrive.", god);

    return true;
}

static void _summon_ignis_elementals()
{
    const god_type god = GOD_IGNIS;
    const int how_many = random_range(2, 3);
    bool success = false;
    for (int i = 0; i < how_many; i++)
        if (create_monster(_wrath_mon_data(MONS_FIRE_ELEMENTAL, god), false))
            success = true;

    if (success)
    {
        const string msg = getSpeakString("Ignis elemental wrath");
        god_speaks(god, msg.c_str());
    }
    else
        simple_god_message("' divine wrath fails to arrive.", god);
}

static bool _ignis_shaft()
{
    // Would it be interesting if ignis could shaft you into other branches,
    // e.g. d -> orc, orc -> elf..?
    if (!you.shaftable())
        return false;
    simple_god_message(" burns the ground from beneath your feet!", GOD_IGNIS);
    ASSERT(you.do_shaft());
    return true;
}

/**
 *  Finds the highest HD monster in sight that would make a suitable vessel
 *  for Ignis's vengeance.
*/
static monster* _ignis_champion_target()
{
    monster* best_mon = nullptr;
    // Never pick monsters that are incredibly weak. That's just sad.
    int min_xl = you.get_experience_level() / 2;
    int seen = 0;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        // Some of these cases are redundant. TODO: cleanup
        if (!mon
            || mons_is_firewood(*mon)
            || !mons_can_use_stairs(*mon, DNGN_STONE_STAIRS_DOWN_I)
            || mons_is_tentacle_or_tentacle_segment(mon->type)
            || mon->is_stationary()
            || mons_is_conjured(mon->type)
            || mon->is_perm_summoned()
            || mon->wont_attack()
            // no stealing another god's pals :P
            || mon->is_priest())
        {
            continue;
        }

        // Don't anoint monsters with no melee attacks or with an
        // existing attack flavour.
        const mon_attack_def atk = mons_attack_spec(*mon, 0, true);
        if (atk.type == AT_NONE || atk.flavour != AF_PLAIN)
            continue;

        // Don't pick something weaker than a different mon we've already seen.
        const int hd = mon->get_experience_level();
        if (hd < min_xl)
            continue;

        // Is this the new strongest thing we've seen? If so, reset the
        // HD floor & the seen count.
        if (hd > min_xl)
        {
            min_xl = hd;
            seen = 0;
        }

        // Reservoir sampling among monsters of this HD.
        ++seen;
        if (one_chance_in(seen))
            best_mon = mon;
    }

    return best_mon;
}

static bool _ignis_champion()
{
    monster *mon = _ignis_champion_target();
    if (!mon)
        return false;
    // Message ordering is a bit touchy here.
    // First, we say what we're doing. TODO: more fun messages
    simple_god_message(make_stringf(" anoints %s as an instrument of vengeance!",
                                    mon->name(DESC_THE).c_str()).c_str(), GOD_IGNIS);
    // Then, we add the ench. This makes it visible if it wasn't, since it's both
    // confusing and unfun for players otherwise.
    mon->add_ench(mon_enchant(ENCH_FIRE_CHAMPION, 1));
    // Then we fire off update_monsters_in_view() to make the next messages make more
    // sense. This triggers 'comes into view' if needed.
    update_monsters_in_view();
    // Then we explain what 'fire champion' does, for those who don't go through xv
    // with a fine-toothed comb afterward.
    simple_monster_message(*mon, " is coated in flames, covering ground quickly"
                                 " and attacking fiercely!");
    // Then we alert it last. It's just reacting, after all.
    behaviour_event(mon, ME_ALERT, &you);
    // Assign blame (so we can look up funny deaths)
    mons_add_blame(mon, "anointed by " + god_name(GOD_IGNIS));
    return true;
}

static bool _ignis_retribution()
{
    if (one_chance_in(3) && _ignis_shaft())
        return true;
    if (coinflip() && _ignis_champion())
        return true;
    _summon_ignis_elementals();
    return true;
}

static bool _uskayaw_retribution()
{
    const god_type god = GOD_USKAYAW;

    // check if we have monsters around
    monster* mon = nullptr;
    mon = choose_random_nearby_monster(0, _choose_hostile_monster);

    switch (random2(5))
    {
    case 0:
    case 1:
        if (mon && mon->can_go_berserk())
        {
            simple_god_message(make_stringf(" drives %s into a dance frenzy!",
                                     mon->name(DESC_THE).c_str()).c_str(), god);
            mon->go_berserk(true);
            return true;
        }
        // else we intentionally fall through

    case 2:
    case 3:
        if (mon)
        {
            simple_god_message(" booms out, \"Time for someone else to take a solo\"",
                                    god);
            paralyse_player(_god_wrath_name(god));
            dec_penance(god, 1);
            return false;
        }
        // else we intentionally fall through

    case 4:
        simple_god_message(" booms out: \"Revellers, it's time to dance!\"", god);
        noisy(35, you.pos());
        break;
    }
    return true;
}

bool divine_retribution(god_type god, bool no_bonus, bool force)
{
    ASSERT(god != GOD_NO_GOD);

    if (is_unavailable_god(god))
        return false;

    // Good gods don't use divine retribution on their followers, and
    // gods don't use divine retribution on followers of gods they don't
    // hate.
    if (!force && ((god == you.religion && is_good_god(god))
        || (god != you.religion && !god_hates_your_god(god))))
    {
        return false;
    }

    god_acting gdact(god, true);

    bool do_more    = true;
    switch (god)
    {
    // One in ten chance that Xom might do something good...
    case GOD_XOM:
        xom_acts(abs(you.piety - HALF_MAX_PIETY),
                 frombool(one_chance_in(10)));
        break;
    case GOD_SHINING_ONE:   do_more = _tso_retribution(); break;
    case GOD_ZIN:           do_more = _zin_retribution(); break;
    case GOD_MAKHLEB:       do_more = _makhleb_retribution(); break;
    case GOD_KIKUBAAQUDGHA: do_more = _kikubaaqudgha_retribution(); break;
    case GOD_YREDELEMNUL:   do_more = _yredelemnul_retribution(); break;
    case GOD_TROG:          do_more = _trog_retribution(); break;
    case GOD_BEOGH:         do_more = _beogh_retribution(); break;
    case GOD_OKAWARU:       do_more = _okawaru_retribution(); break;
    case GOD_LUGONU:        do_more = _lugonu_retribution(); break;
    case GOD_VEHUMET:       do_more = _vehumet_retribution(); break;
    case GOD_NEMELEX_XOBEH: do_more = _nemelex_retribution(); break;
    case GOD_SIF_MUNA:      do_more = _sif_muna_retribution(); break;
    case GOD_JIYVA:         do_more = _jiyva_retribution(); break;
    case GOD_FEDHAS:        do_more = _fedhas_retribution(); break;
    case GOD_CHEIBRIADOS:   do_more = _cheibriados_retribution(); break;
    case GOD_DITHMENOS:     do_more = _dithmenos_retribution(); break;
    case GOD_QAZLAL:        do_more = _qazlal_retribution(); break;
    case GOD_USKAYAW:       do_more = _uskayaw_retribution(); break;
    case GOD_WU_JIAN:       do_more = _wu_jian_retribution(); break;
    case GOD_IGNIS:         do_more = _ignis_retribution(); break;

    case GOD_ASHENZARI:
    case GOD_ELYVILON:
    case GOD_GOZAG:
    case GOD_RU:
    case GOD_HEPLIAKLQANA:
#if TAG_MAJOR_VERSION == 34
    case GOD_PAKELLAS:
#endif
        // No reduction with time.
        return false;

    default:
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_RELIGION)
        mprf(MSGCH_DIAGNOSTICS, "No retribution defined for %s.",
             god_name(god).c_str());
#endif
        return false;
    }

    if (no_bonus)
        return true;

    // Sometimes divine experiences are overwhelming...
    if (do_more && one_chance_in(5) && you.experience_level < random2(37))
    {
        if (coinflip())
        {
            if (!you.confused())
            {
                mprf(MSGCH_WARN, "The divine experience confuses you!");
                confuse_player(5 + random2(3));
            }
        }
        else
        {
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                mprf(MSGCH_WARN, "The divine experience drains your vigour!");

                slow_player(random2(20));
            }
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    dec_penance(god, 1 + random2(3));

    return true;
}

static void _tso_blasts_cleansing_flame(const char *message)
{
    // If there's a message, display it before firing.
    if (message)
        god_speaks(GOD_SHINING_ONE, message);

    simple_god_message(" blasts you with cleansing flame!",
                       GOD_SHINING_ONE);

    // damage is 2d(pow), *3/2 for undead and demonspawn
    cleansing_flame(5 + (you.experience_level * 7) / 12,
                    cleansing_flame_source::tso, you.pos());
}

static void _god_smites_you(god_type god, const char *message,
                            kill_method_type death_type)
{
    ASSERT(god != GOD_NO_GOD);

    if (death_type == NUM_KILLBY)
    {
        switch (god)
        {
            case GOD_BEOGH:     death_type = KILLED_BY_BEOGH_SMITING; break;
            case GOD_SHINING_ONE: death_type = KILLED_BY_TSO_SMITING; break;
            default:            death_type = KILLED_BY_DIVINE_WRATH;  break;
        }
    }

    string aux;

    if (death_type != KILLED_BY_BEOGH_SMITING
        && death_type != KILLED_BY_TSO_SMITING)
    {
        aux = "smitten by " + god_name(god);
    }

    // If there's a message, display it before smiting.
    if (message)
        god_speaks(god, message);

    int divine_hurt = 10 + random2(10);

    for (int i = 0; i < 5; ++i)
        divine_hurt += random2(you.experience_level);

    simple_god_message(" smites you!", god);
    ouch(divine_hurt, death_type, MID_NOBODY, aux.c_str());
}

void reduce_xp_penance(god_type god, int amount)
{
    if (!you.penance[god] || !you.exp_docked_total[god])
        return;

    int lost = min(amount / 2, you.exp_docked[god]);
    you.exp_docked[god] -= lost;

    int new_pen = (((int64_t)you.exp_docked[god] * 50)
                   + you.exp_docked_total[god] - 1)
                / you.exp_docked_total[god];
    if (new_pen < you.penance[god])
        dec_penance(god, you.penance[god] - new_pen);
}

void gozag_incite(monster *mon)
{
    ASSERT(!mon->wont_attack());

    behaviour_event(mon, ME_ALERT, &you);

    bool success = false;

    if (mon->needs_berserk(true, true))
    {
        mon->go_berserk(true);
        success = true;
    }
    else if (!mon->has_ench(ENCH_HASTE))
    {
        enchant_actor_with_flavour(mon, mon, BEAM_HASTE);
        success = true;
    }

    if (success)
    {
        mon->add_ench(ENCH_GOZAG_INCITE);
        view_update_at(mon->pos());
    }
}

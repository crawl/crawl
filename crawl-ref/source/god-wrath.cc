/**
 * @file
 * @brief Divine retribution.
**/

#include "AppHdr.h"

#include "god-wrath.h"

#include <cmath>
#include <queue>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "cleansing-flame-source-type.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "death-curse.h"
#include "decks.h"
#include "env.h"
#include "fineff.h"
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
#include "spl-monench.h"
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
                    .set_summoned(nullptr, MON_SUMM_WRATH)
                    .set_non_actor_summoner(_god_wrath_name(god))
                    .set_range(2, you.current_vision);
    mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
    mg.god = god;
    return mg;
}

/**
 * A standardized tension check to make sure wrath effects that have no
 * effects outside of combat happen in a randomized fashion that also isn't
 * too horribly lethal to escape.
 * @param check_hp If requested, also check player HP to be above 66%.
 */
static bool wrath_tension_check(bool check_hp)
{
    int tens = get_tension(GOD_NO_GOD);
    bool safe_tense = (tens > random_range(4, 6) && tens < 27);

    if (check_hp)
        return (you.hp > you.hp_max * 2 / 3) && safe_tense;
    else
        return safe_tense;
}

static const vector<pop_entry> _okawaru_servants =
{ // warriors
  {  1,  4,  30, FALL, MONS_ORC },
  {  1,  4,  30, FALL, MONS_GNOLL },
  {  2,  6,  30, PEAK, MONS_OGRE },
  {  3,  6,  20, PEAK, MONS_GNOLL_SERGEANT },
  {  3,  9,  30, PEAK, MONS_ORC_WARRIOR },
  {  5, 10,  10, FLAT, MONS_TWO_HEADED_OGRE },
  {  7, 13,  20, PEAK, MONS_CYCLOPS },
  {  7, 16,  20, PEAK, MONS_TENGU_WARRIOR },
  {  8, 16,  20, PEAK, MONS_NAGA_WARRIOR },
  {  9, 16,  20, PEAK, MONS_CENTAUR_WARRIOR },
  {  9, 18,  30, PEAK, MONS_ORC_KNIGHT },
  { 10, 20,  10, FLAT, MONS_DEEP_ELF_KNIGHT },
  { 10, 20,  10, FLAT, MONS_DEEP_ELF_ARCHER },
  { 11, 21,  10, FLAT, MONS_MINOTAUR },
  { 11, 22,  10, FLAT, MONS_MERFOLK_IMPALER },
  { 12, 23,  10, FLAT, MONS_MERFOLK_JAVELINEER },
  { 13, 24,  10, FLAT, MONS_ORC_WARLORD },
  { 13, 24,  20, FLAT, MONS_YAKTAUR_CAPTAIN },
  { 14, 25,  20, FLAT, MONS_STONE_GIANT },
  { 14, 26,  15, FLAT, MONS_FIRE_GIANT },
  { 14, 26,  15, FLAT, MONS_FROST_GIANT },
  { 15, 26,  10, FLAT, RANDOM_BASE_DRACONIAN },
  { 16, 27,  20, FLAT, MONS_SPRIGGAN_DEFENDER },
  { 17, 27,  15, FLAT, MONS_TITAN },
  { 18, 27,  20, FLAT, MONS_WAR_GARGOYLE },
  { 19, 27,  15, FLAT, MONS_TENGU_REAVER },
  { 19, 37,  15, SEMI, MONS_DRACONIAN_KNIGHT },
  { 20, 37,  20, SEMI, MONS_DEEP_ELF_BLADEMASTER },
  { 20, 37,  20, SEMI, MONS_DEEP_ELF_MASTER_ARCHER },
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

    if (success)
    {
        simple_god_message(" sends the divine host to punish you for your evil "
                           "ways!", false, GOD_SHINING_ONE);
    }
    else
    {
        simple_god_message(" divine host fails to appear.",
                           true, GOD_SHINING_ONE);
    }
}

/**
 * The Shining One shouts angrily to alert the player's foes!
 */
static void _tso_shouts()
{
    simple_god_message(" booms out: Take the path of righteousness! REPENT!",
                       false, GOD_SHINING_ONE);
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

static bool _zin_remove_good_mutations()
{
    if (!you.how_mutated())
        return true; // This was checked in _zin_retribution().

    const god_type god = GOD_ZIN;
    bool success = false;

    simple_god_message(" draws some chaos from your body!", false, god);

    bool failMsg = true;

    int how_many = binomial(7, 75, 100); // same avg, var as old bernoullis
    for (int i = how_many; i >= 0; --i)
    {
        // Ensure that only good mutations are removed.
        if (delete_mutation(RANDOM_GOOD_MUTATION, _god_wrath_name(god),
                            failMsg, false, true))
        {
            success = true;
        }
        else
            failMsg = false;
    }

    if (success && !you.how_mutated())
        simple_god_message(" rids your body of chaos!", false, god);
    return success;
}

static bool _zin_retribution()
{
    // preaching/creeping doom theme
    const god_type god = GOD_ZIN;

    // If not mutated, do something else instead.
    const int punishment = you.how_mutated() ? random2(6) : random2(4) + 2;

    switch (punishment)
    {
    case 0:
    case 1: // remove good mutations or deliberately fall through
        if (_zin_remove_good_mutations())
            break;
    case 2:
    case 3:
    case 4: // recital
        simple_god_message(" recites the Axioms of Law to you!", false, god);
        switch (random2(4))
        {
        case 0:
            confuse_player(5 + random2(3));
            break;
        case 1:
            you.put_to_sleep(nullptr, random_range(5, 10) * BASELINE_DELAY);
            break;
        case 2:
            paralyse_player(_god_wrath_name(god));
            return false;
        case 3:
            blind_player(20 + random2(15), ETC_SILVER);
            return false;
        }
        break;
    case 5: // noisiness
        simple_god_message(" booms out: Turn to the light! REPENT!", false, god);
        noisy(25, you.pos()); // same as scroll of noise
        break;
    }
    return true;
}

static bool _xom_retribution()
{
    const int severity = abs(you.piety - HALF_MAX_PIETY);
    const bool good = one_chance_in(10);
    return xom_acts(severity, good) != XOM_DID_NOTHING;
}

static bool _cheibriados_retribution()
{
    // time god theme
    const god_type god = GOD_CHEIBRIADOS;

    // Several of the main effects should care only about present enemies.
    int tension = get_tension(GOD_CHEIBRIADOS);

    // Almost any tension: sleep if >66% HP, otherwise remove haste and give
    // slow. Next to / no tension: noise.
    if (tension > random_range(5, 8))
    {
        if (you.hp >= (you.hp_max * 3 / 4))
        {
            mprf(MSGCH_DANGER, "You lose track of time!");
            you.put_to_sleep(nullptr, random_range(5, 10) * BASELINE_DELAY);
            dec_penance(god, 1);
        }
        else
        {
            mprf(MSGCH_DANGER, "The world leaves you behind!");
            dec_haste_player(10000);
            slow_player(81 + random2(10));
        }
    }
    else
    {
        simple_god_message(" strikes the hour, and time shudders.", false, god);
        noisy(40, you.pos());
    }

    return true;
}

void lucy_check_meddling()
{
    if (!have_passive(passive_t::wrath_banishment))
        return;

    vector<monster*> potential_banishees;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        monster *mon = *mi;
        if (!mon || mon->attitude != ATT_HOSTILE || mon->is_peripheral())
            continue;
        potential_banishees.push_back(mon);
    }
    if (potential_banishees.empty())
        return;

    bool banished = false;
    shuffle_array(begin(potential_banishees), end(potential_banishees));
    for (monster *mon : potential_banishees)
    {
        // We might have banished a summoner and poofed its summons, etc.
        if (invalid_monster(mon) || !mon->alive())
            continue;
        // 80% chance of banishing god wrath summons, 30% chance of banishing
        // other creatures nearby.
        if (x_chance_in_y(mon->was_created_by(MON_SUMM_WRATH) ? 8 : 3, 10))
        {
            if (!banished)
            {
                simple_god_message(" does not welcome meddling.");
                banished = true;
            }
            mon->banish(&you);
        }
    }
}

static void _spell_retribution(monster* avatar, spell_type spell, god_type god,
                               const char* message = nullptr)
{
    simple_god_message(message ? message : " rains destruction down upon you!",
                       false, god);
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
static monster* _get_wrath_avatar(god_type god)
{
    if (monster_at(you.pos()))
        return nullptr;

    monster* avatar = get_free_monster();
    if (!avatar)
        return nullptr;

    avatar->type       = MONS_GOD_WRATH_AVATAR;
    avatar->behaviour  = BEH_SEEK;
    avatar->attitude   = ATT_HOSTILE;
    avatar->flags      = MF_NO_REWARD | MF_JUST_SUMMONED | MF_SEEN
                         | MF_WAS_IN_VIEW | MF_HARD_RESET | MF_NAME_REPLACE;
    avatar->god        = god;
    avatar->set_position(you.pos());
    avatar->set_new_monster_id();
    env.mgrid(you.pos()) = avatar->mindex();
    avatar->mname = _god_wrath_name(god);
    avatar->set_hit_dice(you.experience_level);

    return avatar;
}

/// Cleanup a temporary 'avatar' monster.
static void _reset_avatar(monster &avatar)
{
    env.mid_cache.erase(avatar.mid);
    avatar.reset();
}

/**
 * Rain down Makhleb's destruction upon the player!
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _makhleb_call_down_destruction()
{
    const god_type god = GOD_MAKHLEB;

    monster* avatar = _get_wrath_avatar(god);
    // can't be const because mons_cast() doesn't accept const monster*

    if (avatar == nullptr)
    {
        simple_god_message(" has no time to deal with you just now.", false,
                           god);
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
        return 2 + random2((you.experience_level - 2) / 5); // up to 6 at XL27
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

    if (summoned > 0)
    {
        simple_god_message(summoned > 1 ? " sends minions to punish you." :
                                          " sends a minion to punish you.",
                           false, GOD_MAKHLEB);
    }
    else
        simple_god_message(" minions fail to arrive.", true, GOD_MAKHLEB);

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

static bool _kikubaaqudgha_retribution()
{
    // death/necromancy theme
    const god_type god = GOD_KIKUBAAQUDGHA;
    god_speaks(god, coinflip() ? "You hear Kikubaaqudgha cackling."
                               : "Kikubaaqudgha's malice focuses upon you.");

    if (wrath_tension_check(true))
    {
        int xl = you.experience_level;
        int length = min(5, random_range(xl * 3, xl * 5));
        monster* avatar = _get_wrath_avatar(god);
        cast_sign_of_ruin(*avatar, you.pos(), length * BASELINE_DELAY);
        _reset_avatar(*avatar);
    }
    else
        drain_player(random_range(125, 225), false, true, false);

    return true;
}

static bool _yredelemnul_retribution()
{
    const god_type god = GOD_YREDELEMNUL;

    int how_many = random_range(2, 4);
    int count = 0;
    for (int i = 0; i < how_many; ++i)
    {
        if (yred_random_servant(you.experience_level, true), true)
            ++count;
    }

    if (count > 0)
    {
        simple_god_message(count > 1 ? " sends servants to punish you." :
                                       " sends a servant to punish you.",
                           false, god);
    }
    else
        simple_god_message(" servants fail to arrive.", true, god);

    if (coinflip())
    {
        simple_god_message(" binds you in chains!", false, god);
        you.increase_duration(DUR_NO_MOMENTUM, random_range(3, 8));
    }

    return true;
}

static bool _trog_retribution()
{
    // berserk theme
    const god_type god = GOD_TROG;
    if (wrath_tension_check(true))
    {
        // If the player is healthy and in a reasonable tension range,
        // make the player do a cruel mockery of berserk:
        // weakly thrashing in place, regularly hitting walls and floors.
        simple_god_message(" tears away your strength and self-control!", false, god);
        int vex_max = max(4, you.experience_level / 4);
        you.weaken(nullptr, 25);
        you.vex(nullptr, random_range(3, vex_max), "Trog's wrath");
    }
    else
    {
        // If the other effect could kill you, tension is low enough we can
        // safely interrupt you, or tension's so high they're not making things
        // much worse, summon berserkers from the Brothers In Arms monster set.
        int count = 0;
        int points = 2 + you.experience_level * 3;

        {
            msg::suppress msg;

            while (points > 0)
            {
                int cost =
                    min(min(random2avg((1 + you.experience_level / 4), 2) + 3,
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
                           false, god);
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

                identify_item(wpn);

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
            simple_god_message(msg.str().c_str(), false, god);
            break;
        } // else fall through
    }
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
                 : " is still gathering forces against you.", false, god);
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

    if (count > 0)
        simple_god_message(" sends forces against you!", false, god);
    else
        simple_god_message(" forces are busy with other wars.", true, god);

    return true;
}

static bool _sif_muna_retribution()
{
    // magic/intelligence theme
    const god_type god = GOD_SIF_MUNA;

    simple_god_message(" wrath finds you.", true, god);

    switch (random2(10))
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        confuse_player(5 + random2(3));
        break;

    case 5:
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
        simple_god_message(" wrath scatters you!", true, god);
        you_teleport_now(false, true, "Space warps around you!");
    }
    else if (coinflip())
    {
        simple_god_message(" draws you home!", false, god);
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
    // otherwise, summon around 1 + xl/7 minions, maxing at 6 at xl 21.
    const int how_many = (major ? random2(you.experience_level / 9 + 1)
                                : max(random2(3) + you.experience_level / 7,
                                      1));

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

    if (success)
        simple_god_message(" sends minions to punish you.", false, god);
    else
        simple_god_message(" minions fail to arrive.", true, god);
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
    switch (severity)
    {
        case 1:
            return random_choose(SPELL_MAGIC_DART,
                                 SPELL_STING,
                                 SPELL_SHOCK);
        case 2:
            return random_choose(SPELL_THROW_FLAME,
                                 SPELL_THROW_FROST);
        case 3:
            return random_choose(SPELL_MEPHITIC_CLOUD,
                                 SPELL_STONE_ARROW);
        case 4:
            return random_choose(SPELL_STICKY_FLAME,
                                 SPELL_THROW_ICICLE);
        case 5:
            return random_choose(SPELL_FIREBALL,
                                 SPELL_LIGHTNING_BOLT,
                                 SPELL_BOLT_OF_MAGMA,
                                 SPELL_VENOM_BOLT,
                                 SPELL_BOLT_OF_DRAINING,
                                 SPELL_BOLT_OF_DEVASTATION,
                                 SPELL_QUICKSILVER_BOLT,
                                 SPELL_FREEZING_CLOUD,
                                 SPELL_POISONOUS_CLOUD,
                                 SPELL_METAL_SPLINTERS);
        case 6:
            return random_choose(SPELL_BOLT_OF_FIRE,
                                 SPELL_BOLT_OF_COLD,
                                 SPELL_CORROSIVE_BOLT,
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

    monster* avatar = _get_wrath_avatar(god);
    if (!avatar)
    {
        simple_god_message(" has no time to deal with you just now.", false,
                           god);
        return false;
    }

    const spell_type spell = _vehumet_wrath_type();
    if (spell == SPELL_NO_SPELL)
    {
        simple_god_message(" has no time to deal with you just now.", false,
                           god);
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

    simple_god_message(" makes you draw from the deck of Punishment.", false,
                       god);
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

/**
 * Make Jiyva contaminate that player.
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
        MONS_GLASS_EYE,
        MONS_EYE_OF_DEVASTATION,
        MONS_GREAT_ORB_OF_EYES,
        MONS_SHINING_EYE,
        MONS_GLOWING_ORANGE_BRAIN,
        MONS_ROCKSLIME,
        MONS_VOID_OOZE,
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
    else if (!one_chance_in(3) || you_worship(god))
        _jiyva_contaminate();
    else
        _jiyva_summon_slimes();

    return true;
}

/**
 * Make Fedhas polymorph the player into the trees and fungi they betrayed.
 */
static void _fedhas_transform()
{
    god_speaks(GOD_FEDHAS, "Fedhas booms out: Become one with the cycle of life!");

    const transformation form = random_choose(transformation::fungus,
                                              transformation::tree);

    if (transform(random_range(40, 60), form, true))
        you.transform_uncancellable = true;
}

/**
 * Summon Fedhas's oklobs around the player, plus some surrounding briars.
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _fedhas_summon_plants()
{
    const god_type god = GOD_FEDHAS;
    int xl = you.experience_level;
    int oklob_count = 0;
    int oklob_cap = random_range(1 + div_rand_round(xl, 9),
                                 2 + div_rand_round(xl, 4));
    int radius = random_choose_weighted(54 - you.experience_level, 2,
                                        13, 3);

    // First, find places to place oklobs- not adjacent, but not on the edge
    // of LOS either to let people decide whether or not to kill the oklobs.
    vector<coord_def> oklob_pos;
    for (radius_iterator ri(you.pos(), 5, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (grid_distance(you.pos(), *ri) < radius)
            continue;

        if (!actor_at(*ri) && monster_habitable_grid(MONS_OKLOB_PLANT, *ri))
            oklob_pos.push_back(*ri);
    }

    if (oklob_pos.empty())
        return false;

    // Place them randomly in the spaces viable to place oklobs.
    shuffle_array(oklob_pos);
    for (int i = 0; i < (int)oklob_pos.size() && oklob_count <= oklob_cap; ++i)
    {
        mgen_data mg(MONS_OKLOB_PLANT, BEH_HOSTILE, oklob_pos[i], MHITYOU,
                     MG_FORCE_BEH | MG_FORCE_PLACE, GOD_FEDHAS);
        mg.set_summoned(nullptr, MON_SUMM_WRATH);
        mg.hd = mons_class_hit_dice(MONS_OKLOB_PLANT) + you.experience_level;
        mg.non_actor_summoner = "Fedhas Madash";
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        if (create_monster(mg))
            oklob_count++;
    }

    // Place briars around the player after we've placed oklobs,
    // guaranteed if adjacent but randomly otherwise.
    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (!actor_at(*ri) && monster_habitable_grid(MONS_BRIAR_PATCH, *ri))
        {
            mgen_data mg(MONS_BRIAR_PATCH, BEH_HOSTILE, *ri, MHITYOU,
                         MG_FORCE_BEH | MG_FORCE_PLACE, GOD_FEDHAS);
            mg.set_summoned(nullptr, MON_SUMM_WRATH);
            mg.non_actor_summoner = "Fedhas Madash";
            mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

            if (grid_distance(*ri, you.pos()) == 1 || x_chance_in_y(2, 3))
                create_monster(mg);
        }
    }

    if (oklob_count > 1)
    {
        god_speaks(god, "Plants grow around you in an ominous manner.");
        return false;
    }

    return true;
}

/**
 * Call down the wrath of Fedhas upon the player!
 * Plants and plant/nature themed attacks.
 *
 * @return Whether to take further divine wrath actions afterward.
 */
static bool _fedhas_retribution()
{
    if (wrath_tension_check(true) && !you.transform_uncancellable)
    {
        _fedhas_transform();
        return true;
    }
    else
        return _fedhas_summon_plants();
}

static spell_type _get_hostile_shadow_spell()
{
    vector<spell_type> spells;

    for (spell_type player_spell : you.spells)
    {
        const spschools_type schools = get_spell_disciplines(player_spell);

        // Including only the spells that would work fine for an independent
        // player shadow for now.
        if (schools & spschool::fire)
            spells.push_back(SPELL_SHADOW_BALL);
        if (schools & spschool::ice)
            spells.push_back(SPELL_CREEPING_SHADOW);
        if (schools & spschool::earth)
            spells.push_back(SPELL_SHADOW_SHARD);
        if (schools & spschool::conjuration)
            spells.push_back(SPELL_SHADOW_BEAM);
        if (schools & spschool::hexes)
            spells.push_back(SPELL_SHADOW_TORPOR);
        if (schools & spschool::summoning)
            spells.push_back(SPELL_SHADOW_PUPPET);
    }

    if (spells.empty())
        return SPELL_NO_SPELL;
    else
        return spells[random2(spells.size())];
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
        int how_many = random_range(3, 5);
        mgen_data mg = _wrath_mon_data(MONS_SHADOW_PUPPET, GOD_DITHMENOS);
        mg.hd = 3 + div_rand_round(you.experience_level * 2, 3);
        while (how_many-- > 0)
        {
            if (create_monster(mg, false))
                count++;
        }
        simple_god_message(count ? " weaves the shadows around you into monsters."
                                 : " fails to incite the shadows against you.",
                           false, god);

        if (coinflip())
        {
            mpr("You feel lethargic.");
            you.increase_duration(DUR_SLOW, random_range(10, 20), 100);
        }

        break;
    }
    case 1:
    {
        spell_type spell = _get_hostile_shadow_spell();
        coord_def pos = find_newmons_square(MONS_PLAYER_SHADOW, you.pos());
        if (!pos.origin())
        {
            monster* shadow = create_player_shadow(pos, false, spell);
            simple_god_message(shadow ? " turns your shadow against you."
                                      : " fails to turn your shadows against you.",
                               false, god);
        }

        break;
    }
    case 2:
    {
        // This is possibly kind of underwhelming?
        god_speaks(god, "You feel overwhelmed by the shadows around you.");
        you.put_to_sleep(nullptr, random_range(5, 10) * BASELINE_DELAY);
        break;
    }
    case 3:
        simple_god_message(" tears the darkness away from you.", false, god);
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
    {
        simple_god_message(" incites the elements against you!", false,
                           god);
    }
    else
    {
        simple_god_message(" fails to incite the elements against you.", false,
                           god);
    }
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
                           false, god);
    }
    else
    {
        simple_god_message(" fails to strip away your elemental protection.",
                           false, god);
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
        simple_god_message(" causes a mighty clap of thunder!", false,
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
        const int ego = random_choose(SPWPN_HEAVY, SPWPN_FLAMING,
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

            identify_item(wpn);

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
            barb_player(random_range(5, 10), 5);
            break;
        case 1:
            wu_jian_sifu_message(" whispers: Nowhere to run...");
            slow_player(random_range(5, 10));
            break;
        case 2:
            wu_jian_sifu_message(" whispers: These will loosen your tongue!");
            you.increase_duration(DUR_SILENCE, 5 + random2(11), 50);
            invalidate_agrid(true);
            break;
        case 3:
            wu_jian_sifu_message(" says: Suffer, mortal!");
            you.corrode(nullptr, _god_wrath_name(god).c_str(), 8);
            break;
        }
    }
    else
        simple_god_message(" divine weapons fail to arrive.", true, god);

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
        simple_god_message(" divine wrath fails to arrive.", true, god);
}

static bool _ignis_shaft()
{
    // Would it be interesting if ignis could shaft you into other branches,
    // e.g. d -> orc, orc -> elf..?
    if (!you.shaftable())
        return false;

    simple_god_message(" burns the ground from beneath your feet!", false,
                       GOD_IGNIS);

    // player::do_shaft() already checks resist_dislodge, but the message is a
    // bit worse.
    if (!you.resists_dislodge("falling"))
        you.do_shaft();
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
            || mon->is_peripheral()
            || !mons_can_use_stairs(*mon, DNGN_STONE_STAIRS_DOWN_I)
            || mon->is_stationary()
            || mon->wont_attack()
            // no stealing another god's pals :P
            || mon->is_priest()
            || mon->god != GOD_NO_GOD)
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
    simple_god_message(make_stringf(" anoints %s as an instrument of "
                                    "vengeance!", mon->name(DESC_THE).c_str()).c_str(),
                                    false, GOD_IGNIS);
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
    mon = choose_random_nearby_monster(_choose_hostile_monster);

    switch (random2(5))
    {
    case 0:
    case 1:
        if (mon && mon->can_go_berserk())
        {
            simple_god_message(make_stringf(" drives %s into a dance frenzy!",
                                     mon->name(DESC_THE).c_str()).c_str(),
                                     false, god);
            mon->go_berserk(true);
            return true;
        }
        // else we intentionally fall through

    case 2:
    case 3:
        if (mon)
        {
            simple_god_message(" booms out: Time for someone else to take a "
                               "solo!", false, god);
            paralyse_player(_god_wrath_name(god));
            dec_penance(god, 1);
            return false;
        }
        // else we intentionally fall through

    case 4:
        simple_god_message(" booms out: Revellers, it's time to dance!",
                           false, god);
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

    // Good gods (and Beogh) don't use divine retribution on their followers,
    // and gods don't use divine retribution on followers of gods they don't
    // hate.
    if (!force && ((god == you.religion && (is_good_god(god) || god == GOD_BEOGH))
        || (god != you.religion && !god_hates_your_god(god))))
    {
        return false;
    }

    god_acting gdact(god, true);

    bool do_more    = true;
    switch (god)
    {
    case GOD_XOM:           do_more = _xom_retribution(); break;
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

    lucy_check_meddling();

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
            mprf(MSGCH_WARN, "The divine experience drains your vigour!");
            slow_player(10 + random2(5));
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    // TODO: reverse this philosophy entirely: refactor this all to actually
    // check if the wrath actually did anything.
    dec_penance(god, 1 + random2(3));

    return true;
}

static void _tso_blasts_cleansing_flame(const char *message)
{
    // If there's a message, display it before firing.
    if (message)
        god_speaks(GOD_SHINING_ONE, message);

    simple_god_message(" blasts you with cleansing flame!", false,
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

    simple_god_message(" smites you!", false, god);
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

void gozag_abandon_shops_on_level()
{
    vector<map_marker *> markers = env.markers.get_all(MAT_FEATURE);
    for (const auto marker : markers)
    {
        map_feature_marker *feat =
            dynamic_cast<map_feature_marker *>(marker);
        ASSERT(feat);
        if (feat->feat == DNGN_ABANDONED_SHOP)
        {
            // TODO: clear shop data out?
            env.grid(feat->pos) = DNGN_ABANDONED_SHOP;
            view_update_at(feat->pos);
            env.markers.remove(feat);
        }
    }
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
        lugonu_meddle_fineff::schedule();
    }
}

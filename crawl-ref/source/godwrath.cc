/**
 * @file
 * @brief Divine retribution.
**/

#include "AppHdr.h"

#include "godwrath.h"

#include <sstream>

#include "artefact.h"
#include "attitude-change.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
#include "godpassive.h" // shadow_monster
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mutation.h"
#include "notes.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
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
    "vengeance",        // Jiyva
    "enmity",           // Fedhas Madhash
    "meddling",         // Cheibriados
    "doom",             // Ashenzari (unused)
    "darkness",         // Dithmenos
    "greed",            // Gozag (unused)
    "adversity",        // Qazlal
    "disappointment",   // Ru
    "progress",         // Pakellas
    "fury",             // Uskayaw
    "memory",           // Hepliaklqana (unused)
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
    const bool use_full_name = god == GOD_FEDHAS; // fedhas is very formal.
                                                  // apparently.
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

static const pop_entry _okawaru_servants[] =
{ // warriors
  {  1,  3,   3, FALL, MONS_ORC },
  {  1,  3,   3, FALL, MONS_GNOLL },
  {  2,  6,   3, PEAK, MONS_OGRE },
  {  2,  6,   2, PEAK, MONS_GNOLL_SERGEANT },
  {  3,  7,   1, FLAT, MONS_TWO_HEADED_OGRE },
  {  3, 13,   3, PEAK, MONS_ORC_WARRIOR },
  {  5, 15,   3, PEAK, MONS_ORC_KNIGHT },
  {  5, 15,   1, PEAK, MONS_HILL_GIANT },
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
  { 0,0,0,FLAT,MONS_0 }
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
 * @return Whether to take further divine wrath actions afterward. (false.)
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
    return false;
}

static void _zin_remove_good_mutations()
{
    if (!how_mutated())
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
                               failMsg, false, true, true))
        {
            success = true;
        }
        else
            failMsg = false;
    }

    if (success && !how_mutated())
    {
        simple_god_message(" rids your body of chaos!", god);
        dec_penance(god, 1);
    }
}

static bool _zin_retribution()
{
    // preaching/creeping doom theme
    const god_type god = GOD_ZIN;

    int punishment = random2(8);

    // If not mutated, do something else instead.
    if (punishment > 7 && !how_mutated())
        punishment = random2(6);

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
            break;
        }
        break;
    case 3:
    case 4: // famine
        simple_god_message(" sends a famine down upon you!", god);
        make_hungry(you.hunger / 2, false);
        break;
    case 5: // noisiness
        simple_god_message(" booms out: \"Turn to the light! REPENT!\"", god);
        noisy(25, you.pos()); // same as scroll of noise
        break;
    case 6:
    case 7: // remove good mutations
        _zin_remove_good_mutations();
        break;
    }
    return false;
}

static void _ely_dull_inventory_weapons()
{
    int chance = 50;
    int num_dulled = 0;

    for (auto &item : you.inv)
    {
        if (!item.defined())
            continue;

        if (item.base_type == OBJ_WEAPONS)
        {
            // Don't dull artefacts at all, or weapons below -1.
            if (is_artefact(item) || item.plus <= -1)
                continue;

            // 2/3 of the time, don't do anything.
            if (!one_chance_in(3))
                continue;

            bool wielded = false;

            if (item.link == you.equip[EQ_WEAPON])
                wielded = true;

            // Dull the weapon.
            if (item.plus > -1)
                item.plus--;

            // Update the weapon display, if necessary.
            if (wielded)
                you.wield_change = true;

            chance += item_value(item, true) / 50;
            num_dulled++;
        }
    }

    if (num_dulled > 0)
    {
        if (x_chance_in_y(chance + 1, 100))
            dec_penance(GOD_ELYVILON, 1);

        simple_god_message(
            make_stringf(" dulls %syour weapons.",
                         num_dulled == 1 ? "one of " : "").c_str(),
            GOD_ELYVILON);
    }
}

static bool _elyvilon_retribution()
{
    // healing/interference with fighting theme
    const god_type god = GOD_ELYVILON;

    simple_god_message("'s displeasure finds you.", god);

    switch (random2(5))
    {
    case 0:
    case 1:
        confuse_player(5 + random2(3));
        break;

    case 2: // mostly flavour messages
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_POISON,
                      one_chance_in(3) ? 1 : 0, _god_wrath_name(god));
        break;

    case 3:
    case 4: // Dull weapons in your inventory.
        _ely_dull_inventory_weapons();
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

    bool glammer = false;

    // Determine the level of wrath
    int wrath_type = 0;
    if (wrath_value < 2)       { wrath_type = 0; }
    else if (wrath_value < 4)  { wrath_type = 1; }
    else if (wrath_value < 8)  { wrath_type = 2; }
    else if (wrath_value < 16) { wrath_type = 3; }
    else                       { wrath_type = 4; }

    // Strip away extra speed
    if (one_chance_in(5 - wrath_type))
        dec_haste_player(10000);

    // Chance to be overwhelmed by the divine experience
    if (one_chance_in(5 - wrath_type))
        glammer = true;

    switch (wrath_type)
    {
    // Very high tension wrath
    case 4:
        simple_god_message(" adjusts the clock.", god);
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_RANDOM, 8, 90,
                      _god_wrath_name(god));
        if (one_chance_in(wrath_type - 1))
            break;
    // High tension wrath
    case 3:
        mpr("You lose track of time.");
        you.put_to_sleep(nullptr, 30 + random2(20));
        dec_penance(god, 1);
        if (one_chance_in(wrath_type - 2))
            break;
    // Medium tension
    case 2:
        if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
        {
            mprf(MSGCH_WARN, "You feel the world leave you behind!");
            you.set_duration(DUR_EXHAUSTED, 200);
            slow_player(100);
        }

        if (one_chance_in(wrath_type - 2))
            break;
    // Low tension
    case 1:
        mpr("Time shudders.");
        cheibriados_time_step(2+random2(4));
        if (one_chance_in(3))
            break;
    // No tension wrath.
    case 0:
        if (curse_an_item())
            simple_god_message(" makes up for lost time.", god);
        else
            glammer = true;

    default:
        break;
    }

    if (wrath_type > 2)
        dec_penance(god, 1 + random2(wrath_type));
    return glammer;
}

static void _spell_retribution(monster* avatar, spell_type spell, god_type god)
{
    simple_god_message(" rains destruction down upon you!", god);
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

static bool _kikubaaqudgha_retribution()
{
    // death/necromancy theme
    const god_type god = GOD_KIKUBAAQUDGHA;

    god_speaks(god, coinflip() ? "You hear Kikubaaqudgha cackling."
                               : "Kikubaaqudgha's malice focuses upon you.");

    if (!count_corpses_in_los(nullptr) || random2(you.experience_level) > 4)
    {
        // Either zombies, or corpse rot + skeletons.
        kiku_receive_corpses(you.experience_level * 4);

        if (coinflip())
            corpse_rot(nullptr);
    }

    if (x_chance_in_y(you.experience_level, 27))
    {
        // torment, or 3 necromancy miscasts
        if (!player_res_torment(false))
            torment(nullptr, TORMENT_KIKUBAAQUDGHA, you.pos());
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                MiscastEffect(&you, nullptr, GOD_MISCAST + god,
                              SPTYP_NECROMANCY,
                              2 + div_rand_round(you.experience_level, 9),
                              random2avg(88, 3), _god_wrath_name(god));
            }
        }
    }
    else if (random2(you.experience_level) >= 4)
    {
        // necromancy miscast, 20% chance of additional miscast
        do
        {
            MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_NECROMANCY,
                          2 + div_rand_round(you.experience_level, 9),
                          random2avg(88, 3), _god_wrath_name(god));
        }
        while (one_chance_in(5));
    }

    // Every act of retribution causes corpses in view to rise against
    // you.
    animate_dead(&you, 1 + random2(3), BEH_HOSTILE, MHITYOU, 0,
                 _god_wrath_name(god), god);

    return true;
}

static bool _yredelemnul_retribution()
{
    // undead theme
    const god_type god = GOD_YREDELEMNUL;

    if (coinflip())
    {
        if (you_worship(god) && coinflip() && yred_slaves_abandon_you())
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
                        count++;
                }
                else
                {
                    const int num = yred_random_servants(0, true);
                    if (num >= 0)
                        count += num;
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
        simple_god_message("'s anger turns toward you for a moment.", god);
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_NECROMANCY,
                      2 + div_rand_round(you.experience_level, 9),
                      random2avg(88, 3), _god_wrath_name(god));
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
            no_messages msg;

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
            you.rot(nullptr, 3 + random2(3));
            break;

        case 1:
        case 2:
            lose_stat(STAT_STR, 1 + random2(you.strength() / 5));
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                dec_penance(god, 3);
                mprf(MSGCH_WARN, "You suddenly pass out!");
                const int turns = 2 + random2(6);
                take_note(Note(NOTE_PARALYSIS, min(turns, 13), 0, "Trog"));
                you.increase_duration(DUR_PARALYSIS, turns, 13);
            }
            break;

        case 4:
        case 5:
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                dec_penance(god, 1);
                mprf(MSGCH_WARN, "You suddenly feel exhausted!");
                you.set_duration(DUR_EXHAUSTED, 200);
                slow_player(100);
            }
            break;
        }
    }
    else
    {
        //jmf: returned Trog's old Fire damage
        // -- actually, this function partially exists to remove that,
        //    we'll leave this effect in, but we'll remove the wild
        //    fire magic. -- bwr
        dec_penance(god, 2);
        mprf(MSGCH_WARN, "You feel Trog's fiery rage upon you!");
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_FIRE,
                      8 + you.experience_level, random2avg(98, 3),
                      _god_wrath_name(god));
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
        int num_to_create = (coinflip()) ? 1 : 2;

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
    dec_penance(god, 1);

    switch (random2(10))
    {
    case 0:
    case 1:
        lose_stat(STAT_INT, 1 + random2(you.intel() / 5));
        break;

    case 2:
    case 3:
    case 4:
        confuse_player(5 + random2(3));
        break;

    case 5:
    case 6:
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_DIVINATION, 9, 90,
                      _god_wrath_name(god));
        break;

    case 7:
    case 8:
        if (you.magic_points > 0
#if TAG_MAJOR_VERSION == 34
                 || you.species == SP_DJINNI
#endif
                )
        {
            drain_mp(100);  // This should zero it.
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
 * 50% chance of tloc miscasts; failing that, 50% chance of teleports/blinks.
 */
static void _lugonu_transloc_retribution()
{
    const god_type god = GOD_LUGONU;

    if (coinflip())
    {
        simple_god_message("'s wrath finds you!", god);
        MiscastEffect(&you, nullptr, GOD_MISCAST + god, SPTYP_TRANSLOCATION, 9,
                      90, "Lugonu's touch");
    }
    else if (coinflip())
    {
        // Give extra opportunities for embarrassing teleports.
        simple_god_message("'s wrath finds you!", god);
        mpr("Space warps around you!");
        if (!one_chance_in(3))
            you_teleport_now();
        else
            uncontrolled_blink();
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
 * @return Whether to take further divine wrath actions afterward. (false.)
 */
static bool _lugonu_retribution()
{
    _lugonu_transloc_retribution();
    _lugonu_minion_retribution();

    return false;
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
            return random_choose(SPELL_ISKENDERUNS_MYSTIC_BLAST,
                                 SPELL_STICKY_FLAME,
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
            && you.mutation[i] > 0)
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

    const transformation_type form = random_choose(TRAN_BAT,
                                                   TRAN_FUNGUS,
                                                   TRAN_PIG,
                                                   TRAN_TREE,
                                                   TRAN_WISP);

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
        MONS_EYE_OF_DRAINING,
        MONS_EYE_OF_DEVASTATION,
        MONS_GREAT_ORB_OF_EYES,
        MONS_SHINING_EYE,
        MONS_GLOWING_ORANGE_BRAIN,
        MONS_JELLY,
        MONS_ACID_BLOB,
        MONS_AZURE_JELLY,
        MONS_DEATH_OOZE,
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
 */
static void _fedhas_elemental_miscast()
{
    const god_type god = GOD_FEDHAS;
    simple_god_message(" invokes the elements against you.", god);

    const spschool_flag_type stype = random_choose(SPTYP_ICE, SPTYP_FIRE,
                                                   SPTYP_EARTH, SPTYP_AIR);
    MiscastEffect(&you, nullptr, GOD_MISCAST + god, stype,
                  5 + you.experience_level, random2avg(88, 3),
                  _god_wrath_name(god));
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
    collect_radius_points(radius_points, you.pos(), LOS_NO_TRANS);

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

        place_ring(radius_points[0],
                   you.pos(),
                   temp,
                   1, radius_points[0].size(),
                   seen_count);

        if (seen_count > 0)
            success = true;

        temp.cls = MONS_OKLOB_PLANT;

        place_ring(radius_points[max_idx],
                   you.pos(),
                   temp,
                   random_range(3, 8), 1,
                   seen_count);

        if (seen_count > 0)
            success = true;
    }
    // Otherwise we do something with the nearest neighbors
    // (assuming the player isn't already surrounded).
    else if (!radius_points[0].empty())
    {
        unsigned target_count = random_range(2, 8);
        if (target_count < radius_points[0].size())
            prioritise_adjacent(you.pos(), radius_points[0]);
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

/**
 * Call down the wrath of Fedhas upon the player!
 *
 * Plants and elemental miscasts.
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
        if (fedhas_corpse_spores(BEH_HOSTILE))
        {
            simple_god_message(" produces spores.", god);
            return true;
        }

    case 1:
    default:
        _fedhas_elemental_miscast();
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

static const pop_entry pop_qazlal_wrath[] =
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
  {  4, 14, 30, FLAT, MONS_CATOBLEPAS },
  { 18, 27, 50, RISE, MONS_IRON_DRAGON },

  { 0,0,0,FLAT,MONS_0 }
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
               false, true, false, MUTCLASS_TEMPORARY, true))
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

bool drain_wands()
{
    vector<string> wands;
    for (auto &wand : you.inv)
    {
        if (!wand.defined() || wand.base_type != OBJ_WANDS)
            continue;

        const int charges = wand.plus;
        if (charges > 0 && coinflip())
        {
            const int charge_val = wand_charge_value(wand.sub_type);
            wand.plus -= min(1 + random2(charge_val), charges);
            // Display new number of charges when messaging.
            wands.push_back(wand.name(DESC_PLAIN));
        }
    }
    if (wands.empty())
        return false;

    mpr_comma_separated_list("Magical energy is drained from your ", wands);
    return true;
}

static bool _choose_hostile_monster(const monster& mon)
{
    return mon.attitude == ATT_HOSTILE;
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
    case GOD_ELYVILON:      do_more = _elyvilon_retribution(); break;
    case GOD_JIYVA:         do_more = _jiyva_retribution(); break;
    case GOD_FEDHAS:        do_more = _fedhas_retribution(); break;
    case GOD_CHEIBRIADOS:   do_more = _cheibriados_retribution(); break;
    case GOD_DITHMENOS:     do_more = _dithmenos_retribution(); break;
    case GOD_QAZLAL:        do_more = _qazlal_retribution(); break;
    case GOD_USKAYAW:       do_more = _uskayaw_retribution(); break;

    case GOD_ASHENZARI:
    case GOD_GOZAG:
    case GOD_RU:
    case GOD_HEPLIAKLQANA:
    case GOD_PAKELLAS:
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
            mprf(MSGCH_WARN, "The divine experience confuses you!");
            confuse_player(5 + random2(3));
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
                    CLEANSING_FLAME_TSO, you.pos());
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
    dec_penance(god, 1);
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

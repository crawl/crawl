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
#include "effects.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
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
#include "player-stats.h"
#include "potion.h"
#include "random-weight.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"
#include "xom.h"

static void _god_smites_you(god_type god, const char *message = NULL,
                            kill_method_type death_type = NUM_KILLBY);
static void _tso_blasts_cleansing_flame(const char *message = NULL);

static const char *_god_wrath_adjectives[NUM_GODS] =
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
};

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

    mgen_data temp = mgen_data::hostile_at(skel ? MONS_SKELETON : MONS_ZOMBIE,
                                           _god_wrath_name(GOD_YREDELEMNUL),
                                           true, 0, 0, you.pos(), 0,
                                           GOD_YREDELEMNUL, z_base);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    return create_monster(temp, false);
}

static const pop_entry _okawaru_servants[] =
{ // warriors
  {  1,  3,   3, DOWN, MONS_ORC },
  {  1,  3,   3, DOWN, MONS_GNOLL },
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

    mgen_data temp = mgen_data::hostile_at(mon_type,
                                           _god_wrath_name(GOD_OKAWARU),
                                           true, 0, 0, you.pos(), 0,
                                           GOD_OKAWARU);

    // Don't send sheep into battle, but otherwise let bands in.
    // This makes sure you get multiple orcs/gnolls early on.
    if (mon_type != MONS_CYCLOPS)
        temp.flags |= MG_PERMIT_BANDS;

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    return create_monster(temp, false);
}

static bool _dithmenos_random_shadow(const int count, const int tier)
{
    monster_type mon_type = MONS_SHADOW;
    if (tier >= 2 && count == 0 && coinflip())
        mon_type = MONS_SHADOW_FIEND;
    else if (tier >= 1 && count < 3 && coinflip())
        mon_type = MONS_SHADOW_DEMON;

    mgen_data temp = mgen_data::hostile_at(mon_type,
                                           _god_wrath_name(GOD_DITHMENOS),
                                           true, 0, 0, you.pos(), 0,
                                           GOD_DITHMENOS);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

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

    bool success = false;

    simple_god_message(" draws some chaos from your body!", GOD_ZIN);

    bool failMsg = true;

    for (int i = 7; i >= 0; --i)
    {
        // Ensure that only good mutations are removed.
        if (i <= random2(10)
            && delete_mutation(RANDOM_GOOD_MUTATION, "Zin's wrath",
                               failMsg, false, true, true))
        {
            success = true;
        }
        else
            failMsg = false;
    }

    if (success && !how_mutated())
    {
        simple_god_message(" rids your body of chaos!", GOD_ZIN);
        dec_penance(GOD_ZIN, 1);
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
            confuse_player(3 + random2(10));
            break;
        case 1:
            you.put_to_sleep(NULL, 30 + random2(20));
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
    int quiver_link;

    you.m_quiver->get_desired_item(NULL, &quiver_link);

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        if (you.inv[i].base_type == OBJ_WEAPONS)
        {
            // Don't dull artefacts at all, or weapons below -1.
            if (is_artefact(you.inv[i]) || you.inv[i].plus <= -1)
                continue;

            // 2/3 of the time, don't do anything.
            if (!one_chance_in(3))
                continue;

            bool wielded = false;

            if (you.inv[i].link == you.equip[EQ_WEAPON])
                wielded = true;

            // Dull the weapon.
            if (you.inv[i].plus > -1)
                you.inv[i].plus--;

            // Update the weapon display, if necessary.
            if (wielded)
                you.wield_change = true;

            chance += item_value(you.inv[i], true) / 50;
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
        confuse_player(3 + random2(10));
        break;

    case 2: // mostly flavour messages
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_POISON,
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
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_RANDOM, 8, 90,
                      _god_wrath_name(god));
        if (one_chance_in(wrath_type - 1))
            break;
    // High tension wrath
    case 3:
        mpr("You lose track of time.");
        you.put_to_sleep(NULL, 30 + random2(20));
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
                                 SPELL_SPIT_ACID,
                                 -1);
        case 1:
            // major destruction
            return random_choose(SPELL_BOLT_OF_FIRE,
                                 SPELL_FIREBALL,
                                 SPELL_LIGHTNING_BOLT,
                                 SPELL_STICKY_FLAME,
                                 SPELL_IRON_SHOT,
                                 SPELL_BOLT_OF_DRAINING,
                                 SPELL_ORB_OF_ELECTRICITY,
                                 -1);
        case 2:
            // legendary destruction (no IOOD because it doesn't really
            // work here)
            return random_choose(SPELL_FIREBALL,
                                 SPELL_LEHUDIBS_CRYSTAL_SPEAR,
                                 SPELL_ORB_OF_ELECTRICITY,
                                 SPELL_FLASH_FREEZE,
                                 SPELL_GHOSTLY_FIREBALL,
                                 -1);
    }
}

/**
 * Create a fake 'avatar' monster representing a god, with which to hurl
 * destructive magic at foolish players.
 *
 * @param god           The god doing the wrath-hurling.
 * @return              An avatar monster, or NULL if none could be set up.
 */
static monster* get_avatar(god_type god)
{
    monster* avatar = shadow_monster(false);
    if (!avatar)
        return NULL;

    avatar->mname = _god_wrath_name(god);
    avatar->flags |= MF_NAME_REPLACE;
    avatar->attitude = ATT_HOSTILE;
    avatar->set_hit_dice(you.experience_level);

    return avatar;
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

    if (avatar == NULL)
    {
        simple_god_message("has no time to deal with you just now.", god);
        return false; // not a very dazzling divine experience...
    }

    _spell_retribution(avatar, _makhleb_destruction_type(), god);
    shadow_monster_reset(avatar);
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

    mgen_data temp = mgen_data::hostile_at(servant,
                                           _god_wrath_name(GOD_MAKHLEB),
                                           true, 0, 0, you.pos(), 0,
                                           GOD_MAKHLEB);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    return create_monster(temp, false);
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
                                                   MONS_CACODEMON,
                                                   -1);
        if (_makhleb_summon_servant(servant))
            summoned++;
    }

    for (int i = 0; i < lesser_servants; i++)
    {
        const monster_type servant = random_choose(MONS_HELLWING,
                                                   MONS_NEQOXEC,
                                                   MONS_ORANGE_DEMON,
                                                   MONS_SMOKE_DEMON,
                                                   MONS_YNOXINUL,
                                                   -1);
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

    if (!count_corpses_in_los(NULL) || random2(you.experience_level) > 4)
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
            torment(NULL, TORMENT_KIKUBAAQUDGHA, you.pos());
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_NECROMANCY,
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
            MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_NECROMANCY,
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
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_NECROMANCY,
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
            potion_effect(POT_DECAY, 100);
            break;

        case 1:
        case 2:
            lose_stat(STAT_STR, 1 + random2(you.strength() / 5), false,
                      "divine retribution from Trog");
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                dec_penance(god, 3);
                mprf(MSGCH_WARN, "You suddenly pass out!");
                you.duration[DUR_PARALYSIS] = 2 + random2(6);
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
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_FIRE,
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
                              WPN_HALBERD,     -1);

            // Now create monster.
            if (monster *mon =
                create_monster(
                    mgen_data::hostile_at(MONS_DANCING_WEAPON,
                        _god_wrath_name(god),
                        true, 0, 0, you.pos(), 0, god)))
            {
                ASSERT(mon->weapon() != NULL);
                item_def& wpn(*mon->weapon());

                set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_ELECTROCUTION);

                wpn.plus  = random2(3);
                wpn.sub_type = wpn_type;

                set_ident_flags(wpn, ISFLAG_KNOW_TYPE);

                item_colour(wpn);

                mon->flags |= (MF_NO_REWARD | MF_HARD_RESET);

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

        mgen_data temp = mgen_data::hostile_at(punisher,
                                               _god_wrath_name(god),
                                               true, 0, 0, you.pos(),
                                               MG_PERMIT_BANDS, god);

        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        monster *mons = create_monster(temp, false);

        // sometimes name band leader
        if (mons && one_chance_in(3))
            give_monster_proper_name(mons);

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
        lose_stat(STAT_INT, 1 + random2(you.intel() / 5), false,
                  "divine retribution from Sif Muna");
        break;

    case 2:
    case 3:
    case 4:
        confuse_player(3 + random2(10));
        break;

    case 5:
    case 6:
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_DIVINATION, 9, 90,
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
            mprf(MSGCH_WARN, "You suddenly feel drained of magical energy!");
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
        MiscastEffect(&you, NULL, GOD_MISCAST + god, SPTYP_TRANSLOCATION, 9,
                      90, "Lugonu's touch");
    }
    else if (coinflip())
    {
        // Give extra opportunities for embarrassing teleports.
        simple_god_message("'s wrath finds you!", god);
        mpr("Space warps around you!");
        if (!one_chance_in(3))
            you_teleport_now(false);
        else
            random_blink(false);
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
                3,                              MONS_ANCIENT_ZYME,
                0
            );

        mgen_data temp = mgen_data::hostile_at(to_summon,
                                               _god_wrath_name(god), true, 0,
                                               0, you.pos(), 0, god);
        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        if (create_monster(temp, false))
            success = true;
    }

    if (major)
    {
        // try to summon one nasty monster.
        const monster_type to_summon = random_choose(MONS_TENTACLED_STARSPAWN,
                                                     MONS_WRETCHED_STAR,
                                                     MONS_STARCURSED_MASS,
                                                     -1);

        mgen_data temp = mgen_data::hostile_at(to_summon,
                                               _god_wrath_name(god), true, 0,
                                               0, you.pos(), 0, god);
        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        if (create_monster(temp, false))
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
                                 SPELL_FLAME_TONGUE,
                                 -1);
        case 2:
            return random_choose(SPELL_THROW_FLAME,
                                 SPELL_THROW_FROST,
                                 -1);
        case 3:
            return random_choose(SPELL_MEPHITIC_CLOUD,
                                 SPELL_STONE_ARROW,
                                 -1);
        case 4:
            return random_choose(SPELL_ISKENDERUNS_MYSTIC_BLAST,
                                 SPELL_STICKY_FLAME,
                                 SPELL_THROW_ICICLE,
                                 SPELL_ENERGY_BOLT,
                                 -1);
        case 5:
            return random_choose(SPELL_FIREBALL,
                                 SPELL_LIGHTNING_BOLT,
                                 SPELL_BOLT_OF_MAGMA,
                                 SPELL_VENOM_BOLT,
                                 SPELL_BOLT_OF_DRAINING,
                                 SPELL_QUICKSILVER_BOLT,
                                 SPELL_METAL_SPLINTERS,
                                 -1);
        case 6:
            return random_choose(SPELL_BOLT_OF_FIRE,
                                 SPELL_BOLT_OF_COLD,
                                 SPELL_CORROSIVE_BOLT,
                                 SPELL_FREEZING_CLOUD,
                                 SPELL_POISONOUS_CLOUD,
                                 SPELL_POISON_ARROW,
                                 SPELL_IRON_SHOT,
                                 SPELL_CONJURE_BALL_LIGHTNING,
                                 -1);
        case 7:
            return random_choose(SPELL_ORB_OF_ELECTRICITY,
                                 SPELL_FLASH_FREEZE,
                                 -1);
        case 8:
            return random_choose(SPELL_LEHUDIBS_CRYSTAL_SPEAR,
                                 -1);
        case 9:
            return random_choose(SPELL_FIRE_STORM,
                                 SPELL_HELLFIRE, // let it end...
                                 -1);
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
        simple_god_message("has no time to deal with you just now.", god);
        return false;
    }

    const spell_type spell = _vehumet_wrath_type();
    if (spell == SPELL_NO_SPELL)
    {
        simple_god_message("has no time to deal with you just now.", god);
        shadow_monster_reset(avatar);
        return false;
    }

    _spell_retribution(avatar, spell, god);
    shadow_monster_reset(avatar);
    return true;
}

static bool _nemelex_retribution()
{
    // card theme
    const god_type god = GOD_NEMELEX_XOBEH;

    // like Xom, this might actually help the player -- bwr
    simple_god_message(" makes you draw from the Deck of Punishment.", god);
    draw_from_deck_of_punishment();
    return true;
}

/**
 * Let Jiyva throw a few malmutations the player's way.
 */
static void _jiyva_mutate_player()
{
    god_speaks(GOD_JIYVA, "You feel Jiyva alter your body.");

    const int mutations = 1 + random2(3);
    for (int i = 0; i < mutations; ++i)
        mutate(RANDOM_BAD_MUTATION, "Jiyva's wrath", true, false, true);
}

/**
 * Make Jiyva slmify a nearby enemy.
 */
static void _jiyva_slimify()
{
    monster* mon = NULL;

    const int max_tries = 10;
    bool success = false;
    for (int i = 0; i < max_tries; i++)
    {
        mon = choose_random_nearby_monster(0);

        if (mon && mon_can_be_slimified(mon) && mon->attitude == ATT_HOSTILE)
        {
            success = true;
            break;
        }
    }

    if (success)
        return;

    simple_god_message(make_stringf("'s putrescence saturates %s!",
                                    mon->name(DESC_THE).c_str()).c_str(),
                       GOD_JIYVA);
    slimify_monster(mon, true);
}

/**
 * Transmutation-miscast-themed wrath; make Jiyva contaminate tha player,
 * and possibly polymorph them into a bad (?) form.
 */
static void _jiyva_tmut()
{
    const god_type god = GOD_JIYVA;
    god_speaks(god, "Mutagenic energy floods into your body!");
    contaminate_player(random2(you.penance[god] * 500));

    if (coinflip())
        return;

    // XXX: someone should probably rethink this list...
    const transformation_type form = random_choose(TRAN_BAT, TRAN_STATUE,
                                                   TRAN_SPIDER, -1);

    if (transform(random2(you.penance[god]) * 2, form, true))
        you.transform_uncancellable = true;
}

static void _jiyva_summon_slimes()
{
    const god_type god = GOD_JIYVA;

    const monster_type slimes[] =
    {
        MONS_GIANT_EYEBALL,
        MONS_EYE_OF_DRAINING,
        MONS_EYE_OF_DEVASTATION,
        MONS_GREAT_ORB_OF_EYES,
        MONS_SHINING_EYE,
        MONS_GIANT_ORANGE_BRAIN,
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

        mgen_data temp =
            mgen_data::hostile_at(static_cast<monster_type>(slime),
                                  _god_wrath_name(god),
                                  true, 0, 0, you.pos(), 0, god);

        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        if (create_monster(temp, false))
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
    // Don't create hostile slimes while under penance.
    else if (!you_worship(god) && there_are_monsters_nearby() && coinflip())
        _jiyva_slimify();
    else if (!one_chance_in(3) || you_worship(god))
        _jiyva_tmut();
    else
        _jiyva_summon_slimes();

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
                                                   SPTYP_EARTH, SPTYP_AIR,
                                                   -1);
    MiscastEffect(&you, NULL, GOD_MISCAST + god, stype,
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

    mgen_data temp =
        mgen_data::hostile_at(MONS_OKLOB_PLANT,
                              _god_wrath_name(god),
                              false, 0, 0,
                              coord_def(-1, -1),
                              MG_FORCE_PLACE, god);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

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
        // Try and spawn some hostile giant spores, if none are created
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
                        RANDOM_MOBILE_MONSTER, BEH_HOSTILE, 0,
                        4, MON_SUMM_WRATH, you.pos(), MHITYOU, 0, god,
                        MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE,
                        level_id(BRANCH_DUNGEON,
                                 min(27, you.experience_level + 5)),
                        0, 0, 0, "", _god_wrath_name(god))))
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
        you.put_to_sleep(NULL, 30 + random2(20));
        break;
    }
    case 3:
        simple_god_message(" tears the shadows away from you.", god);
        you.sentinel_mark();
        break;
    }
    return true;
}

/**
 * Summon Qazlal's elemental minions to destroy the player!
 */
static void _qazlal_summon_elementals()
{
    const god_type god = GOD_QAZLAL;

    mgen_data temp =
        mgen_data::hostile_at(MONS_NO_MONSTER,
                              _god_wrath_name(god),
                              true, 0, 0, you.pos(), 0, god);

    temp.hd = you.experience_level;
    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    const int how_many = 1 + (you.experience_level / 5);
    bool success = false;

    for (int i = 0; i < how_many; i++)
    {
        temp.cls = random_choose(MONS_FIRE_ELEMENTAL,
                                 MONS_WATER_ELEMENTAL,
                                 MONS_AIR_ELEMENTAL,
                                 MONS_EARTH_ELEMENTAL,
                                 -1);
        if (create_monster(temp, false))
            success = true;
    }

    if (success)
        simple_god_message(" incites the elements against you!", god);
    else
        simple_god_message(" fails to incite the elements against you.", god);
}

/**
 * Surround the player with dangerous terrain! (Currently just lava!)
 */
static void _qazlal_deform_terrain()
{
    // TODO: think of terrain-ish effects for the other elements

    const god_type god = GOD_QAZLAL;

    vector<coord_weight> candidates;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (grd(*ri) != DNGN_FLOOR || actor_at(*ri) || igrd(*ri) != NON_ITEM)
            continue;

        const int weight = LOS_RADIUS*LOS_RADIUS - distance2(you.pos(), *ri);
        candidates.push_back(coord_weight(*ri, weight));
    }

    const int how_many = min((int)candidates.size(),
                             3 + (you.experience_level / 2));
    int deforms = 0;
    while (deforms < how_many)
    {
        const coord_def* pos = random_choose_weighted(candidates);
        if (!pos)
            break;

        deforms++;
        temp_change_terrain(*pos, DNGN_LAVA,
                            random2(you.experience_level * BASELINE_DELAY),
                            TERRAIN_CHANGE_FLOOD);

        for (auto it = candidates.begin(); it != candidates.end(); ++it)
        {
            if (it->first == *pos)
            {
                candidates.erase(it);
                break;
            }
        }
    }

    if (deforms)
    {
        mprf(MSGCH_GOD, god,
             "The ground around you shudders, and lava spills forth!");
    }
    else
    {
        mprf(MSGCH_GOD, god,
             "The ground around you shudders for a moment.");
    }
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
    switch (random2(3))
    {
    case 0:
        _qazlal_summon_elementals();
        break;
    case 1:
        _qazlal_deform_terrain();
        break;
    case 2:
        _qazlal_elemental_vulnerability();
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
        xom_acts(one_chance_in(10), abs(you.piety - HALF_MAX_PIETY));
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

    case GOD_ASHENZARI:
    case GOD_GOZAG:
    case GOD_RU:
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
            confuse_player(3 + random2(10));
        }
        else
        {
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                mprf(MSGCH_WARN, "The divine experience leaves you feeling exhausted!");

                slow_player(random2(20));
            }
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    dec_penance(god, 1 + random2(3));

    return true;
}

// Currently only used when orcish idols have been destroyed.
static string _get_beogh_speech(const string key)
{
    string result = getSpeakString("Beogh " + key);

    if (result.empty())
        return "Beogh is angry!";

    return result;
}

// Destroying orcish idols (a.k.a. idols of Beogh) may anger Beogh.
void beogh_idol_revenge()
{
    god_acting gdact(GOD_BEOGH, true);

    // Beogh watches his charges closely, but for others doesn't always
    // notice.
    if (!you_worship(GOD_BEOGH)
        && (!player_genus(GENPC_ORCISH) || coinflip())
        && x_chance_in_y(2, 3))
    {
        return;
    }

    const char *revenge;

    if (you_worship(GOD_BEOGH))
        revenge = _get_beogh_speech("idol follower").c_str();
    else if (player_genus(GENPC_ORCISH))
        revenge = _get_beogh_speech("idol orc").c_str();
    else
        revenge = _get_beogh_speech("idol other").c_str();

    _god_smites_you(GOD_BEOGH, revenge);
}

static void _tso_blasts_cleansing_flame(const char *message)
{
    // TSO won't protect you from his own cleansing flame, and Xom is too
    // capricious to protect you from it.
    if (!you_worship(GOD_SHINING_ONE) && !you_worship(GOD_XOM)
        && !player_under_penance() && x_chance_in_y(you.piety, MAX_PIETY * 2))
    {
        god_speaks(you.religion,
                   make_stringf("\"Mortal, I have averted the wrath of %s... "
                                "this time.\"",
                                god_name(GOD_SHINING_ONE).c_str()).c_str());
    }
    else
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
}

static void _god_smites_you(god_type god, const char *message,
                            kill_method_type death_type)
{
    ASSERT(god != GOD_NO_GOD);

    // Your god won't protect you from his own smiting, and Xom is too
    // capricious to protect you from any god's smiting.
    if (!you_worship(god) && !you_worship(GOD_XOM)
        && !player_under_penance() && x_chance_in_y(you.piety, MAX_PIETY * 2))
    {
        god_speaks(you.religion,
                   make_stringf("\"Mortal, I have averted the wrath of %s... "
                                "this time.\"", god_name(god).c_str()).c_str());
    }
    else
    {
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
}

void ash_reduce_penance(int amount)
{
    if (!you.penance[GOD_ASHENZARI] || !you.exp_docked_total)
        return;

    int lost = min(amount / 2, you.exp_docked);
    you.exp_docked -= lost;

    int new_pen = (((int64_t)you.exp_docked * 50) + you.exp_docked_total - 1)
                / you.exp_docked_total;
    if (new_pen < you.penance[GOD_ASHENZARI])
        dec_penance(GOD_ASHENZARI, you.penance[GOD_ASHENZARI] - new_pen);
}

void gozag_incite(monster *mon)
{
    ASSERT(!mon->wont_attack());

    behaviour_event(mon, ME_ALERT, &you);

    bool success = false;

    if (coinflip() && mon->needs_berserk(false))
    {
        mon->go_berserk(false);
        success = true;
    }
    else
    {
        int tries = 3;
        do
        {
            switch (random2(3))
            {
                case 0:
                    if (mon->has_ench(ENCH_MIGHT))
                        break;
                    enchant_actor_with_flavour(mon, mon, BEAM_MIGHT);
                    success = true;
                    break;
                case 1:
                    if (mon->has_ench(ENCH_HASTE))
                        break;
                    enchant_actor_with_flavour(mon, mon, BEAM_HASTE);
                    success = true;
                    break;
                case 2:
                    if (mon->invisible() || you.can_see_invisible())
                        break;
                    enchant_actor_with_flavour(mon, mon, BEAM_INVISIBILITY);
                    success = true;
                    break;
            }
        }
        while (!success && --tries > 0);
    }

    if (success)
        view_update_at(mon->pos());
}

/**
 * Invoke the CURSE OF GOZAG by goldifying some part of a stack that the player
 * is trying to pick up.
 * @param it            The item being picked up.
 * @param quant_got     The number of items being picked up. (May be only part
 * of the stack.)
 * @param quiet         Whether to suppress messages.
 * @return              How much of the stack was goldified.
 */
int gozag_goldify(const item_def &it, int quant_got, bool quiet)
{
    if (it.base_type != OBJ_POTIONS
        && it.base_type != OBJ_SCROLLS
        && it.base_type != OBJ_FOOD)
    {
        return 0;
    }


    const int val = item_value(it, true) / it.quantity;
    double prob = (double)(val - 20) / 100.0;
    if (prob > 1.0)
        prob = 1.0;

    int goldified_count = 0;
    for (int i = 0; i < quant_got; i++)
        if (decimal_chance(prob))
            goldified_count++;

    if (!goldified_count)
        return goldified_count;

    if (!quiet)
    {
        string msg = get_desc_quantity(goldified_count, quant_got, "the")
                     + " " + it.name(DESC_PLAIN);
        msg += goldified_count > 1 ? " turn" : " turns";
        msg += " to gold as you touch ";
        msg += goldified_count > 1 ? "them." : "it.";

        mprf(MSGCH_GOD, GOD_GOZAG, "%s", msg.c_str());
    }

    get_gold(it, goldified_count, quiet);
    dec_penance(GOD_GOZAG, goldified_count);

    return goldified_count;
}

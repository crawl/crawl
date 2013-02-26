/**
 * @file
 * @brief Divine retribution.
**/

#include "AppHdr.h"

#include "godwrath.h"

#include "externs.h"

#include "artefact.h"
#include "attitude-change.h"
#include "database.h"
#include "decks.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
#include "itemprop.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "makeitem.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "ouch.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "shopping.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "state.h"
#include "transform.h"
#include "shout.h"
#include "xom.h"

#include <sstream>

static void _god_smites_you(god_type god, const char *message = NULL,
                            kill_method_type death_type = NUM_KILLBY);
static bool _beogh_idol_revenge();
static void _tso_blasts_cleansing_flame(const char *message = NULL);
static bool _tso_holy_revenge();
static bool _ely_holy_revenge(const monster *victim);

static bool _yred_random_zombified_hostile()
{
    const bool skel = one_chance_in(4);

    monster_type z_base;
    monster_type z_type;

    do
        z_base = pick_random_zombie();
    while (skel && !mons_skeleton(z_base));

    if (mons_zombie_size(z_base) == Z_BIG)
        z_type = skel ? MONS_SKELETON_LARGE : MONS_ZOMBIE_LARGE;
    else
        z_type = skel ? MONS_SKELETON_SMALL : MONS_ZOMBIE_SMALL;

    mgen_data temp = mgen_data::hostile_at(z_type, "the anger of Yredelemnul",
                                           true, 0, 0, you.pos(), 0,
                                           GOD_YREDELEMNUL, z_base);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    return create_monster(temp, false);
}

static bool _okawaru_random_servant()
{
    monster_type mon_type;
    const int temp_rand = random2(100);

    // warriors
    mon_type = ((temp_rand < 15) ? MONS_ORC_WARRIOR :      // 15%
                (temp_rand < 30) ? MONS_ORC_KNIGHT :       // 15%
                (temp_rand < 40) ? MONS_NAGA_WARRIOR :     // 10%
                (temp_rand < 50) ? MONS_CENTAUR_WARRIOR :  // 10%
                (temp_rand < 60) ? MONS_STONE_GIANT :      // 10%
                (temp_rand < 70) ? MONS_FIRE_GIANT :       // 10%
                (temp_rand < 80) ? MONS_FROST_GIANT :      // 10%
                (temp_rand < 90) ? MONS_CYCLOPS :          // 10%
                (temp_rand < 95) ? MONS_HILL_GIANT         //  5%
                                 : MONS_TITAN);            //  5%

    mgen_data temp = mgen_data::hostile_at(mon_type, "the fury of Okawaru",
                                           true, 0, 0, you.pos(), 0,
                                           GOD_OKAWARU);

    temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    return create_monster(temp, false);
}

static bool _tso_retribution()
{
    // holy warriors/cleansing theme
    const god_type god = GOD_SHINING_ONE;

    int punishment = random2(7);

    switch (punishment)
    {
    case 0:
    case 1:
    case 2: // summon holy warriors (3/7)
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
                                   : "'s divine host fails to appear.", god);

        break;
    }
    case 3:
    case 4: // cleansing flame (2/7)
        _tso_blasts_cleansing_flame();
        break;
    case 5:
    case 6: // either noisiness or silence (2/7)
        if (coinflip())
        {
            simple_god_message(" booms out: \"Take the path of righteousness! REPENT!\"", god);
            noisy(25, you.pos()); // same as scroll of noise
        }
        else
        {
            god_speaks(god, "You feel the Shining One's silent rage upon you!");
            cast_silence(25);
        }
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
            confuse_player(3 + random2(10), false);
            break;
        case 1:
            you.put_to_sleep(NULL, 30 + random2(20));
            break;
        case 2:
            paralyse_player("the wrath of Zin");
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
            // Don't dull artefacts at all, or weapons below -1/-1.
            if (is_artefact(you.inv[i])
                || you.inv[i].plus <= -1 && you.inv[i].plus2 <= -1)
            {
                continue;
            }

            // 2/3 of the time, don't do anything.
            if (!one_chance_in(3))
                continue;

            bool wielded = false;

            if (you.inv[i].link == you.equip[EQ_WEAPON])
                wielded = true;

            // Dull the weapon.
            if (you.inv[i].plus > -1)
                you.inv[i].plus--;
            if (you.inv[i].plus2 > -1)
                you.inv[i].plus2--;

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
        confuse_player(3 + random2(10), false);
        break;

    case 2: // mostly flavour messages
        MiscastEffect(&you, -god, SPTYP_POISON, one_chance_in(3) ? 1 : 0,
                      "the displeasure of Elyvilon");
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
        MiscastEffect(&you, -god, SPTYP_RANDOM, 8, 90,
                      "the meddling of Cheibriados");
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
            mpr("You feel the world leave you behind!", MSGCH_WARN);
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

static bool _makhleb_retribution()
{
    // demonic servant theme
    const god_type god = GOD_MAKHLEB;

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        mgen_data temp =
            mgen_data::hostile_at(random_choose(MONS_EXECUTIONER, MONS_GREEN_DEATH,
                                  MONS_BLIZZARD_DEMON, MONS_BALRUG, MONS_CACODEMON, -1),
                                  "the fury of Makhleb",
                                  true, 0, 0, you.pos(), 0, god);

        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        bool success = create_monster(temp, false);

        simple_god_message(success ? " sends a greater servant after you!"
                                   : "'s greater servant is unavoidably "
                                     "detained.", god);
    }
    else
    {
        int how_many = 1 + (you.experience_level / 7);
        int count = 0;

        for (; how_many > 0; --how_many)
        {
            mgen_data temp =
                mgen_data::hostile_at(random_choose(MONS_HELLWING, MONS_NEQOXEC,
                                      MONS_ORANGE_DEMON, MONS_SMOKE_DEMON,
                                      MONS_YNOXINUL, -1),
                                      "the fury of Makhleb",
                                      true, 0, 0, you.pos(), 0, god);

            temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

            if (create_monster(temp, false))
                count++;
        }

        simple_god_message(count > 1 ? " sends minions to punish you." :
                           count > 0 ? " sends a minion to punish you."
                                     : "'s minions fail to arrive.", god);
    }

    return true;
}

static bool _kikubaaqudgha_retribution()
{
    // death/necromancy theme
    const god_type god = GOD_KIKUBAAQUDGHA;

    god_speaks(god, coinflip() ? "You hear Kikubaaqudgha cackling."
                               : "Kikubaaqudgha's malice focuses upon you.");

    if (random2(you.experience_level) > 4)
    {
        // Either zombies, or corpse rot + skeletons.
        kiku_receive_corpses(you.experience_level * 4, you.pos());

        if (coinflip())
            corpse_rot(&you);
    }

    if (coinflip())
    {
        // necromancy miscast, 20% chance of additional miscast
        do
        {
            MiscastEffect(&you, -god, SPTYP_NECROMANCY,
                          5 + you.experience_level,
                          random2avg(88, 3), "the malice of Kikubaaqudgha");
        }
        while (one_chance_in(5));
    }
    else if (one_chance_in(10))
    {
        // torment, or 3 necromancy miscasts
        if (!player_res_torment(false))
            torment(NULL, TORMENT_KIKUBAAQUDGHA, you.pos());
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                MiscastEffect(&you, -god, SPTYP_NECROMANCY,
                              5 + you.experience_level,
                              random2avg(88, 3), "the malice of Kikubaaqudgha");
            }
        }
    }

    // Every act of retribution causes corpses in view to rise against
    // you.
    animate_dead(&you, 1 + random2(3), BEH_HOSTILE, MHITYOU, 0,
                 "the malice of Kikubaaqudgha", GOD_KIKUBAAQUDGHA);

    return true;
}

static bool _yredelemnul_retribution()
{
    // undead theme
    const god_type god = GOD_YREDELEMNUL;

    if (random2(you.experience_level) > 4)
    {
        if (you.religion == god && coinflip() && yred_slaves_abandon_you())
            ;
        else
        {
            const bool zombified = one_chance_in(4);

            int how_many = 1 + random2(1 + (you.experience_level / 5));
            int count = 0;

            for (; how_many > 0; --how_many)
            {
                if (zombified)
                {
                    if (_yred_random_zombified_hostile())
                        count++;
                }
                else
                    count += yred_random_servants(0, true);
            }

            simple_god_message(count > 1 ? " sends servants to punish you." :
                               count > 0 ? " sends a servant to punish you."
                                         : "'s servants fail to arrive.", god);
        }
    }
    else
    {
        simple_god_message("'s anger turns toward you for a moment.", god);
        MiscastEffect(&you, -god, SPTYP_NECROMANCY, 5 + you.experience_level,
                      random2avg(88, 3), "the anger of Yredelemnul");
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
                int cost = min(random2(8) + 3, points);

                // quick reduction for large values
                if (points > 20 && coinflip())
                {
                    points -= 10;
                    cost = 10;
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
            lose_stat(STAT_STR, 1 + random2(you.strength() / 5), true,
                      "divine retribution from Trog");
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                dec_penance(god, 3);
                mpr("You suddenly pass out!", MSGCH_WARN);
                you.duration[DUR_PARALYSIS] = 2 + random2(6);
            }
            break;

        case 4:
        case 5:
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                dec_penance(god, 1);
                mpr("You suddenly feel exhausted!", MSGCH_WARN);
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
        mpr("You feel Trog's fiery rage upon you!", MSGCH_WARN);
        MiscastEffect(&you, -god, SPTYP_FIRE, 8 + you.experience_level,
                      random2avg(98, 3), "the fiery rage of Trog");
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
        _god_smites_you(GOD_BEOGH);
        break;

    case 2: // send out one or two dancing weapons (12.5%)
    {
        int num_created = 0;
        int num_to_create = (coinflip()) ? 1 : 2;

        // Need a species check, in case this retribution is a result of
        // drawing the Wrath card.
        const bool am_orc = (you.species == SP_HILL_ORC);

        for (int i = 0; i < num_to_create; ++i)
        {
            const int temp_rand = random2(13);
            const int wpn_type = ((temp_rand ==  0) ? WPN_CLUB :
                                  (temp_rand ==  1) ? WPN_MACE :
                                  (temp_rand ==  2) ? WPN_FLAIL :
                                  (temp_rand ==  3) ? WPN_MORNINGSTAR :
                                  (temp_rand ==  4) ? WPN_DAGGER :
                                  (temp_rand ==  5) ? WPN_SHORT_SWORD :
                                  (temp_rand ==  6) ? WPN_LONG_SWORD :
                                  (temp_rand ==  7) ? WPN_SCIMITAR :
                                  (temp_rand ==  8) ? WPN_GREAT_SWORD :
                                  (temp_rand ==  9) ? WPN_HAND_AXE :
                                  (temp_rand == 10) ? WPN_BATTLEAXE :
                                  (temp_rand == 11) ? WPN_SPEAR
                                                    : WPN_HALBERD);

            // Now create monster.
            if (monster *mon =
                create_monster(
                    mgen_data::hostile_at(MONS_DANCING_WEAPON,
                        "the wrath of Beogh",
                        true, 0, 0, you.pos(), 0, god)))
            {
                ASSERT(mon->weapon() != NULL);
                item_def& wpn(*mon->weapon());

                if (am_orc)
                {
                    set_equip_race(wpn, ISFLAG_NO_RACE);
                    set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_ORC_SLAYING);
                }
                else
                {
                    set_equip_race(wpn, ISFLAG_ORCISH);
                    set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_ELECTROCUTION);
                }

                if (coinflip())
                    wpn.flags |= ISFLAG_CURSED;

                wpn.plus  = random2(3);
                wpn.plus2 = random2(3);
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
                << " of " << (am_orc ? "orc slaying" : "electrocution")
                << " at you.";
            simple_god_message(msg.str().c_str(), god);
            break;
        } // else fall through
    }
    case 3: // 25%, relatively harmless
    case 4: // in effect, only for penance
        if (you.religion == god && beogh_followers_abandon_you())
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

        mgen_data temp = mgen_data::hostile_at(punisher, "the wrath of Beogh",
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
        lose_stat(STAT_INT, 1 + random2(you.intel() / 5), true,
                  "divine retribution from Sif Muna");
        break;

    case 2:
    case 3:
    case 4:
        confuse_player(3 + random2(10), false);
        break;

    case 5:
    case 6:
        MiscastEffect(&you, -god, SPTYP_DIVINATION, 9, 90,
                      "the will of Sif Muna");
        break;

    case 7:
        if (!forget_spell())
            mpr("You get a splitting headache.");
        break;

    case 8:
        if (you.magic_points > 0)
        {
            dec_mp(100);  // This should zero it.
            mpr("You suddenly feel drained of magical energy!", MSGCH_WARN);
        }
        break;

    case 9:
        // This will set all the extendable duration spells to
        // a duration of one round, thus potentially exposing
        // the player to real danger.
        antimagic();
        break;
    }

    return true;
}

static bool _lugonu_retribution()
{
    // abyssal servant theme
    const god_type god = GOD_LUGONU;

    if (coinflip())
    {
        simple_god_message("'s wrath finds you!", god);
        MiscastEffect(&you, -god, SPTYP_TRANSLOCATION, 9, 90, "Lugonu's touch");
        // No return - Lugonu's touch is independent of other effects.
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

        // No return.
    }

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        mgen_data temp =
            mgen_data::hostile_at(random_choose(MONS_GREEN_DEATH,
                                  MONS_BLIZZARD_DEMON, MONS_BALRUG, -1),
                                  "the touch of Lugonu",
                                  true, 0, 0, you.pos(), 0, god);

        temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        bool success = create_monster(temp, false);
        simple_god_message(success ? " sends a demon after you!"
                                   : "'s demon is unavoidably detained.", god);
    }
    else
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 7);

        for (; how_many > 0; --how_many)
        {
            mgen_data temp =
                mgen_data::hostile_at(random_choose(MONS_HELLWING, MONS_NEQOXEC,
                                      MONS_ORANGE_DEMON, MONS_SMOKE_DEMON,
                                      MONS_YNOXINUL, -1),
                                      "the touch of Lugonu",
                                      true, 0, 0, you.pos(), 0, god);

            temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

            if (create_monster(temp, false))
                success = true;
        }

        simple_god_message(success ? " sends minions to punish you."
                                   : "'s minions fail to arrive.", god);
    }

    return false;
}

static bool _vehumet_retribution()
{
    // conjuration theme
    const god_type god = GOD_VEHUMET;

    simple_god_message("'s vengeance finds you.", god);
    MiscastEffect(&you, -god, SPTYP_CONJURATION,
                   8 + you.experience_level, random2avg(98, 3),
                   "the wrath of Vehumet");
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

static bool _jiyva_retribution()
{
    const god_type god = GOD_JIYVA;

    if (you.can_safely_mutate() && one_chance_in(7))
    {
        const int mutat = 1 + random2(3);

        god_speaks(god, "You feel Jiyva alter your body.");

        for (int i = 0; i < mutat; ++i)
            mutate(RANDOM_BAD_MUTATION, "Jiyva's wrath", true, false, true);
    }
    else if (there_are_monsters_nearby() && coinflip())
    {
        int tries = 0;
        bool found_one = false;
        monster* mon;

        while (tries < 10)
        {
            mon = choose_random_nearby_monster(0);

            if (!mon || !mon_can_be_slimified(mon)
                || mon->attitude != ATT_HOSTILE)
            {
                tries++;
                continue;
            }
            else
            {
                found_one = true;
                break;
            }
        }

        if (found_one)
        {
            simple_god_message(
                make_stringf("'s putrescence saturates %s!",
                             mon->name(DESC_THE).c_str()).c_str(), god);
            slimify_monster(mon, true);
        }
    }
    else if (!one_chance_in(3))
    {
            god_speaks(god, "Mutagenic energy floods into your body!");
            contaminate_player(random2(you.penance[GOD_JIYVA]) / 2);

            if (coinflip())
            {
                transformation_type form = TRAN_NONE;

                switch (random2(3))
                {
                    case 0:
                        form = TRAN_BAT;
                        break;
                    case 1:
                        form = TRAN_STATUE;
                        break;
                    case 2:
                        form = TRAN_SPIDER;
                        break;
                }

                if (transform(random2(you.penance[GOD_JIYVA]) * 2, form, true))
                    you.transform_uncancellable = true;
            }
    }
    else
    {
        const monster_type slimes[] = {
                MONS_GIANT_EYEBALL, MONS_EYE_OF_DRAINING,
                MONS_EYE_OF_DEVASTATION, MONS_GREAT_ORB_OF_EYES,
                MONS_SHINING_EYE, MONS_GIANT_ORANGE_BRAIN,
                MONS_JELLY, MONS_BROWN_OOZE, MONS_ACID_BLOB, MONS_AZURE_JELLY,
                MONS_DEATH_OOZE, MONS_SLIME_CREATURE
            };

        int how_many = 1 + (you.experience_level / 10) + random2(3);
        bool success = false;

        for (; how_many > 0; --how_many)
        {
            const monster_type slime = RANDOM_ELEMENT(slimes);

            mgen_data temp =
                mgen_data::hostile_at(static_cast<monster_type>(slime),
                                      "the vengeance of Jiyva",
                                      true, 0, 0, you.pos(), 0, god);

            temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

            if (create_monster(temp, false))
                success = true;
        }

        god_speaks(god, success ? "Some slimes ooze up out of the ground!"
                                : "The ground quivers slightly.");
    }

    return true;
}

static bool _fedhas_retribution()
{
    const god_type god = GOD_FEDHAS;

    // We have 3 forms of retribution, but players under penance will be
    // spared the 'you are now surrounded by oklob plants, please die' one.
    const int retribution_options = you.religion == GOD_FEDHAS ? 2 : 3;

    switch (random2(retribution_options))
    {
    case 0:
        // Try and spawn some hostile giant spores, if none are created
        // fall through to the elemental miscast effects.
        if (fedhas_corpse_spores(BEH_HOSTILE, false))
        {
            simple_god_message(" produces spores.", GOD_FEDHAS);
            break;
        }

    case 1:
    {
        // Elemental miscast effects.
        simple_god_message(" invokes the elements against you.", GOD_FEDHAS);

        spschool_flag_type stype = SPTYP_NONE;
        switch (random2(4))
        {
        case 0:
            stype= SPTYP_ICE;
            break;
        case 1:
            stype = SPTYP_EARTH;
            break;
        case 2:
            stype = SPTYP_FIRE;
            break;
        case 3:
            stype = SPTYP_AIR;
            break;
        };
        MiscastEffect(&you, -god, stype, 5 + you.experience_level,
                      random2avg(88, 3), "the enmity of Fedhas Madash");
        break;
    }

    case 2:
    {
        bool success = false;

        // We are going to spawn some oklobs but first we need to find
        // out a little about the situation.
        vector<vector<coord_def> > radius_points;
        collect_radius_points(radius_points, you.pos(),
                              you.get_los_no_trans());

        unsigned free_thresh = 24;

        int max_idx = 3;
        unsigned max_points = radius_points[max_idx].size();

        for (unsigned i=max_idx + 1; i<radius_points.size(); i++)
        {
            if (radius_points[i].size() > max_points)
            {
                max_points = radius_points[i].size();
                max_idx = i;
            }
        }

        mgen_data temp =
            mgen_data::hostile_at(MONS_OKLOB_PLANT,
                                  "the enmity of Fedhas Madash",
                                  false,
                                  0,
                                  0,
                                  coord_def(-1, -1),
                                  MG_FORCE_PLACE,
                                  GOD_FEDHAS);

       temp.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        // If we have a lot of space to work with we can do something
        // flashy.
        if (radius_points[max_idx].size() > free_thresh)
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

            unsigned i = radius_points[0].size() - target_count;

            for (; i < radius_points[0].size(); ++i)
            {
                temp.pos = radius_points[0].at(i);
                temp.cls = coinflip() ?
                           MONS_WANDERING_MUSHROOM : MONS_OKLOB_PLANT;

                if (create_monster(temp, false))
                    success = true;
            }
        }

        if (success)
        {
            god_speaks(god, "Plants grow around you in an ominous manner.");
            return false;
        }

        break;
    }
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

    case GOD_ASHENZARI:
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
            mpr("The divine experience confuses you!", MSGCH_WARN);
            confuse_player(3 + random2(10));
        }
        else
        {
            if (you.duration[DUR_SLOW] < 180 * BASELINE_DELAY)
            {
                mpr("The divine experience leaves you feeling exhausted!",
                     MSGCH_WARN);

                slow_player(random2(20));
            }
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    dec_penance(god, 1 + random2(3));

    return true;
}

bool do_god_revenge(conduct_type thing_done, const monster *victim)
{
    bool retval = false;

    switch (thing_done)
    {
    case DID_DESTROY_ORCISH_IDOL:
        retval = _beogh_idol_revenge();
        break;
    case DID_KILL_HOLY:
    case DID_HOLY_KILLED_BY_UNDEAD_SLAVE:
    case DID_HOLY_KILLED_BY_SERVANT:
        // It's TSO who does the smiting and war stuff so he handles revenge
        // for his allies as well -- unless another god has some special ties.
        if (victim && victim->god == GOD_ELYVILON)
            retval = _ely_holy_revenge(victim);
        else
            retval = _tso_holy_revenge();
        break;
    default:
        break;
    }

    return retval;
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
static bool _beogh_idol_revenge()
{
    god_acting gdact(GOD_BEOGH, true);

    // Beogh watches his charges closely, but for others doesn't always
    // notice.
    if (you.religion == GOD_BEOGH
        || (you.species == SP_HILL_ORC && coinflip())
        || one_chance_in(3))
    {
        const char *revenge;

        if (you.religion == GOD_BEOGH)
            revenge = _get_beogh_speech("idol follower").c_str();
        else if (you.species == SP_HILL_ORC)
            revenge = _get_beogh_speech("idol hill orc").c_str();
        else
            revenge = _get_beogh_speech("idol other").c_str();

        _god_smites_you(GOD_BEOGH, revenge);

        return true;
    }

    return false;
}

static void _tso_blasts_cleansing_flame(const char *message)
{
    // TSO won't protect you from his own cleansing flame, and Xom is too
    // capricious to protect you from it.
    if (you.religion != GOD_SHINING_ONE && you.religion != GOD_XOM
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

// Currently only used when holy beings have been killed.
static string _get_tso_speech(const string key)
{
    string result = getSpeakString("the Shining One " + key);

    if (result.empty())
        return "The Shining One is angry!";

    return result;
}

// Killing holy beings may anger TSO.
static bool _tso_holy_revenge()
{
    god_acting gdact(GOD_SHINING_ONE, true);

    // TSO watches evil god worshippers more closely.
    if (!is_good_god(you.religion)
        && ((is_evil_god(you.religion) && one_chance_in(6))
            || one_chance_in(8)))
    {
        string revenge;

        if (is_evil_god(you.religion))
            revenge = _get_tso_speech("holy evil");
        else
            revenge = _get_tso_speech("holy other");

        _tso_blasts_cleansing_flame(revenge.c_str());

        return true;
    }

    return false;
}

// Killing apises may make Elyvilon sad.  She'll sulk and stuff.
static bool _ely_holy_revenge(const monster *victim)
{
    // It's a mild effect, a relatively big chance is ok.  Keeping it small
    // though -- we don't want gods to be omniscient.
    if (!one_chance_in(3))
        return false;

    god_acting gdact(GOD_ELYVILON, true);

    string msg = getSpeakString("Elyvilon holy");
    if (msg.empty())
        msg = "Elyvilon is displeased.";
    mpr(msg.c_str(), MSGCH_GOD, GOD_ELYVILON);

    vector<monster*> patients;
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        // healer not necromancer
        if (!mi->alive())
            continue;
        // hates undead -- would she heal demons out of spite for you?
        if (mi->is_evil(false) || mi->is_unholy(false))
            continue;
        // your associates are presumed guilty
        if (mi->wont_attack())
            continue;
        if (mi->hit_points >= mi->max_hit_points)
            continue;
        patients.push_back(*mi);
    }
    if (patients.empty())
        return false;

    mpr("Elyvilon touches your foes with healing grace.");
    for (vector<monster*>::const_iterator mi = patients.begin();
         mi != patients.end(); ++mi)
    {
        simple_monster_message(*mi, " is healed.");
        (*mi)->heal(10 + random2(10), false);
    }

    return true;
}

static void _god_smites_you(god_type god, const char *message,
                            kill_method_type death_type)
{
    ASSERT(god != GOD_NO_GOD);

    // Your god won't protect you from his own smiting, and Xom is too
    // capricious to protect you from any god's smiting.
    if (you.religion != god && you.religion != GOD_XOM
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
        ouch(divine_hurt, NON_MONSTER, death_type, aux.c_str());
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

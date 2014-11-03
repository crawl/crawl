/**
 * @file
 * @brief Summoning spells and other effects creating monsters.
**/

#include "AppHdr.h"

#include "spl-summoning.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "butcher.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "ghost.h"
#include "godconduct.h"
#include "goditem.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "melee_attack.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "options.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "spl-zap.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "unwind.h"
#include "viewchar.h"
#include "xom.h"

static void _monster_greeting(monster *mons, const string &key)
{
    string msg = getSpeakString(key);
    if (msg == "__NONE")
        msg.clear();
    mons_speaks_msg(mons, msg, MSGCH_TALK, silenced(mons->pos()));
}

spret_type cast_summon_butterflies(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = min(8, 3 + random2(3) + random2(pow) / 10);

    for (int i = 0; i < how_many; ++i)
    {
        if (create_monster(
                mgen_data(MONS_BUTTERFLY, BEH_FRIENDLY, &you,
                          3, SPELL_SUMMON_BUTTERFLIES,
                          you.pos(), MHITYOU,
                          0, god)))
        {
            success = true;
        }
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_small_mammal(int pow, god_type god, bool fail)
{
    fail_check();

    monster_type mon = MONS_PROGRAM_BUG;

    if (x_chance_in_y(10, pow + 1))
        mon = coinflip() ? MONS_BAT : MONS_RAT;
    else
        mon = MONS_QUOKKA;

    if (!create_monster(
            mgen_data(mon, BEH_FRIENDLY, &you,
                      3, SPELL_SUMMON_SMALL_MAMMAL,
                      you.pos(), MHITYOU,
                      MG_AUTOFOE, god)))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return SPRET_SUCCESS;
}

bool item_is_snakable(const item_def& item)
{
    return item.base_type == OBJ_MISSILES
           && (item.sub_type == MI_ARROW || item.sub_type == MI_JAVELIN)
           && item.special != SPMSL_SILVER
           && item.special != SPMSL_STEEL;
}

spret_type cast_sticks_to_snakes(int pow, god_type god, bool fail)
{
    if (!you.weapon())
    {
        mprf("Your %s feel slithery!", you.hand_name(true).c_str());
        return SPRET_ABORT;
    }

    const item_def& wpn = *you.weapon();
    const string abort_msg = make_stringf("%s feel%s slithery for a moment!",
                                          wpn.name(DESC_YOUR).c_str(),
                                          wpn.quantity > 1 ? "" : "s");

    // Don't enchant sticks marked with {!D}.
    if (!check_warning_inscriptions(wpn, OPER_DESTROY))
    {
        mprf("%s", abort_msg.c_str());
        return SPRET_ABORT;
    }

    const int dur = min(3 + random2(pow) / 20, 5);
    int how_many_max = 1 + min(6, random2(pow) / 15);
    const bool friendly = (!wpn.cursed());
    const beh_type beha = (friendly) ? BEH_FRIENDLY : BEH_HOSTILE;

    int count = 0;

    if (!item_is_snakable(wpn))
    {
        mprf("%s", abort_msg.c_str());
        return SPRET_ABORT;
    }
    else
    {
        fail_check();
        if (wpn.quantity < how_many_max)
            how_many_max = wpn.quantity;

        for (int i = 0; i < how_many_max; i++)
        {
            monster_type mon;

            if (one_chance_in(5 - min(4, div_rand_round(pow * 2, 25))))
            {
                mon = x_chance_in_y(pow / 3, 100) ? MONS_WATER_MOCCASIN
                                                  : MONS_ADDER;
            }
            else
                mon = MONS_BALL_PYTHON;

            if (monster *snake = create_monster(mgen_data(mon, beha, &you,
                                      0, SPELL_STICKS_TO_SNAKES, you.pos(),
                                      MHITYOU, MG_AUTOFOE, god), false))
            {
                count++;
                snake->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, dur));
            }
        }
    }

    if (!count)
    {
        mprf("%s", abort_msg.c_str());
        return SPRET_SUCCESS;
    }

    dec_inv_item_quantity(you.equip[EQ_WEAPON], count);
    mpr((count > 1) ? "You create some snakes!" : "You create a snake!");
    return SPRET_SUCCESS;
}

// Creates a mixed swarm of typical swarming animals.
// Number and duration depend on spell power.
spret_type cast_summon_swarm(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;
    const int dur = min(2 + (random2(pow) / 4), 6);
    const int how_many = stepdown_value(2 + random2(pow)/10 + random2(pow)/25,
                                        2, 2, 6, 8);

    for (int i = 0; i < how_many; ++i)
    {

        monster_type mon = MONS_NO_MONSTER;

        // If you worship a good god, don't summon an evil/unclean
        // swarmer (in this case, the vampire mosquito).
        const int MAX_TRIES = 100;
        int tries = 0;
        do
        {
            mon = random_choose_weighted(1, MONS_BUTTERFLY,
                                         1, MONS_WORM,
                                         3, MONS_WORKER_ANT,
                                         1, MONS_SCORPION,
                                         1, MONS_SPIDER,
                                         1, MONS_GOLIATH_BEETLE,
                                         3, MONS_KILLER_BEE,
                                         1, MONS_VAMPIRE_MOSQUITO,
                                         1, MONS_YELLOW_WASP,
                                         0);
        }
        while (player_will_anger_monster(mon) && ++tries < MAX_TRIES);

        // If a hundred tries wasn't enough, it's never going to work.
        if (tries >= MAX_TRIES)
            break;

        if (create_monster(
                mgen_data(mon, BEH_FRIENDLY, &you,
                          dur, SPELL_SUMMON_SWARM,
                          you.pos(),
                          MHITYOU,
                          MG_AUTOFOE, god)))
        {
            success = true;
        }
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_call_canine_familiar(int pow, god_type god, bool fail)
{
    fail_check();
    monster_type mon = MONS_PROGRAM_BUG;

    const int chance = pow + random_range(-10, 10);

    if (chance > 59)
        mon = MONS_WARG;
    else if (chance > 39)
        mon = MONS_WOLF;
    else
        mon = MONS_HOUND;

    const int dur = min(2 + (random2(pow) / 4), 6);

    if (!create_monster(
            mgen_data(mon, BEH_FRIENDLY, &you,
                      dur, SPELL_CALL_CANINE_FAMILIAR,
                      you.pos(),
                      MHITYOU,
                      MG_AUTOFOE, god)))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return SPRET_SUCCESS;
}

spret_type cast_summon_ice_beast(int pow, god_type god, bool fail)
{
    fail_check();
    const int dur = min(2 + (random2(pow) / 4), 4);

    mgen_data ice_beast = mgen_data(MONS_ICE_BEAST, BEH_FRIENDLY, &you,
                                    dur, SPELL_SUMMON_ICE_BEAST,
                                    you.pos(), MHITYOU,
                                    MG_AUTOFOE, god);
    ice_beast.hd = (3 + div_rand_round(pow, 13));

    if (create_monster(ice_beast))
        mpr("A chill wind blows around you.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_monstrous_menagerie(actor* caster, int pow, god_type god, bool fail)
{
    fail_check();
    monster_type type = MONS_PROGRAM_BUG;

    if (random2(pow) > 60 && coinflip())
        type = MONS_SPHINX;
    else
        type = random_choose(MONS_HARPY, MONS_MANTICORE, MONS_LINDWURM, -1);

    int num = (type == MONS_HARPY ? 1 + x_chance_in_y(pow, 80)
                                      + x_chance_in_y(pow - 75, 100)
                                  : 1);
    const bool plural = (num > 1);

    const int hd_bonus = div_rand_round(pow - 50, 25);

    mgen_data mdata = mgen_data(type, BEH_COPY, caster, 4,
                                SPELL_MONSTROUS_MENAGERIE, caster->pos(),
                                (caster->is_player()) ?
                                    MHITYOU : caster->as_monster()->foe,
                                MG_AUTOFOE | MG_DONT_CAP, god);

    if (caster->is_player())
        mdata.hd = get_monster_data(type)->hpdice[0] +  hd_bonus;

    bool seen = false;
    bool first = true;
    int mid = -1;
    while (num-- > 0)
    {
        if (monster* beast = create_monster(mdata))
        {
            if (you.can_see(beast))
                seen = true;

            // Link the harpies together as one entity as far as the summon
            // cap is concerned.
            if (type == MONS_HARPY)
            {
                if (mid == -1)
                    mid = beast->mid;

                beast->props["summon_id"].get_int() = mid;
            }

            // Handle cap only for the first of the batch being summoned
            if (first)
                summoned_monster(beast, &you, SPELL_MONSTROUS_MENAGERIE);

            first = false;
        }
    }

    if (seen)
    {
        mprf("%s %s %s %s!", caster->name(DESC_THE).c_str(),
                             caster->conj_verb("summon").c_str(),
                             plural ? "some" : "a",
                             plural ? pluralise(mons_type_name(type, DESC_PLAIN)).c_str()
                                    : mons_type_name(type, DESC_PLAIN).c_str());
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_hydra(actor *caster, int pow, god_type god, bool fail)
{
    fail_check();
    // Power determines number of heads. Minimum 4 heads, maximum 12.
    // Rare to get more than 8.
    int heads;

    // Small chance to create a huge hydra (if spell power is high enough)
    if (one_chance_in(6))
        heads = min((random2(pow) / 6), 12);
    else
        heads = min((random2(pow) / 6), 8);

    if (heads < 4)
        heads = 4;

    // Duration is always very short - just 1.
    if (monster *hydra = create_monster(
            mgen_data(MONS_HYDRA, BEH_COPY, caster,
                      1, SPELL_SUMMON_HYDRA, caster->pos(),
                      (caster->is_player()) ?
                          MHITYOU : caster->as_monster()->foe,
                      MG_AUTOFOE, (god == GOD_NO_GOD) ? caster->deity() : god,
                      MONS_HYDRA, heads)))
    {
        if (you.see_cell(hydra->pos()))
            mpr("A hydra appears.");
    }
    else if (caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static monster_type _choose_dragon_type(int pow, god_type god, bool player)
{
    monster_type mon = MONS_PROGRAM_BUG;

    const int chance = random2(pow);

    if (chance >= 80 || one_chance_in(6))
        mon = (coinflip()) ? MONS_GOLDEN_DRAGON : MONS_QUICKSILVER_DRAGON;
    else if (chance >= 40 || one_chance_in(6))
        switch (random2(3))
        {
        case 0:
            mon = MONS_IRON_DRAGON;
            break;
        case 1:
            mon = MONS_SHADOW_DRAGON;
            break;
        default:
            mon = MONS_STORM_DRAGON;
            break;
        }
    else
        mon = (coinflip()) ? MONS_FIRE_DRAGON : MONS_ICE_DRAGON;

    // For good gods, switch away from shadow dragons (and, for TSO,
    // golden dragons, since they poison) to storm/iron dragons.
    if (player && player_will_anger_monster(mon)
        || (god == GOD_SHINING_ONE && mon == MONS_GOLDEN_DRAGON))
    {
        mon = (coinflip()) ? MONS_STORM_DRAGON : MONS_IRON_DRAGON;
    }

    return mon;
}

spret_type cast_dragon_call(int pow, bool fail)
{
    if (you.duration[DUR_DRAGON_CALL_COOLDOWN])
    {
        mpr("You cannot issue another dragon's call so soon.");
        return SPRET_ABORT;
    }

    fail_check();

    mpr("You call out to the draconic realm, and the dragon horde roars back!");
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    you.duration[DUR_DRAGON_CALL] = (15 + pow / 5 + random2(15)) * BASELINE_DELAY;

    return SPRET_SUCCESS;
}

static bool _place_dragon()
{
    const int pow = calc_spell_power(SPELL_DRAGON_CALL, true);
    monster_type mon = _choose_dragon_type(pow, you.religion, true);

    vector<monster*> targets;

    // Pick a random hostile in sight
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(&you, *mi))
            targets.push_back(*mi);
    }

    shuffle_array(targets);

    // Attempt to place adjacent to the first chosen hostile. If there is no
    // valid spot, move on to the next one.
    for (unsigned int i = 0; i < targets.size(); ++i)
    {
        // Chose a random viable adjacent spot to the select target
        vector<coord_def> spots;
        for (adjacent_iterator ai(targets[i]->pos()); ai; ++ai)
        {
            if (monster_habitable_grid(MONS_FIRE_DRAGON, grd(*ai)) && !actor_at(*ai))
                spots.push_back(*ai);
        }

        // Now try to create the actual dragon
        if (spots.size() > 0)
        {
            const coord_def pos = spots[random2(spots.size())];

            if (monster *dragon = create_monster(mgen_data(mon, BEH_COPY, &you,
                                                           2, SPELL_DRAGON_CALL,
                                                           pos, MHITYOU,
                                                           MG_FORCE_PLACE | MG_AUTOFOE)))
            {
                dec_mp(random_range(2, 3));

                if (you.see_cell(dragon->pos()))
                    mpr("A dragon arrives to answer your call!");

                // The dragon is allowed to act immediately here
                dragon->flags &= ~MF_JUST_SUMMONED;

                if (!enough_mp(2, true))
                {
                    mprf(MSGCH_DURATION, "Having expended the last of your "
                                         "magical power, your connection to the "
                                         "dragon horde fades.");
                    you.duration[DUR_DRAGON_CALL] = 0;
                    you.duration[DUR_DRAGON_CALL_COOLDOWN] = random_range(150, 250);
                }

                return true;
            }
        }
    }

    return false;
}

void do_dragon_call(int time)
{
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    while (time > you.attribute[ATTR_NEXT_DRAGON_TIME]
           && you.duration[DUR_DRAGON_CALL])
    {
        time -= you.attribute[ATTR_NEXT_DRAGON_TIME];
        _place_dragon();
        you.attribute[ATTR_NEXT_DRAGON_TIME] = 3 + random2(5)
                                               + count_summons(&you, SPELL_DRAGON_CALL) * 5;
    }
    you.attribute[ATTR_NEXT_DRAGON_TIME] -= time;
}

spret_type cast_summon_dragon(actor *caster, int pow, god_type god, bool fail)
{
    // Dragons are always friendly. Dragon type depends on power and
    // random chance, with two low-tier dragons possible at high power.
    // Duration fixed at 6.

    fail_check();
    bool success = false;

    if (god == GOD_NO_GOD)
        god = caster->deity();

    int how_many = 1;
    monster_type mon = _choose_dragon_type(pow, god, caster->is_player());

    if (pow >= 100 && (mon == MONS_FIRE_DRAGON || mon == MONS_ICE_DRAGON))
        how_many = 2;

    // For good gods, switch away from shadow dragons (and, for TSO,
    // golden dragons, since they poison) to storm/iron dragons.
    if (player_will_anger_monster(mon)
        || (god == GOD_SHINING_ONE && mon == MONS_GOLDEN_DRAGON))
    {
        mon = (coinflip()) ? MONS_STORM_DRAGON : MONS_IRON_DRAGON;
    }

    for (int i = 0; i < how_many; ++i)
    {
        if (monster *dragon = create_monster(
                mgen_data(mon, BEH_COPY, caster,
                          6, SPELL_SUMMON_DRAGON,
                          caster->pos(),
                          (caster->is_player()) ? MHITYOU
                            : caster->as_monster()->foe,
                          MG_AUTOFOE, god)))
        {
            if (you.see_cell(dragon->pos()))
                mpr("A dragon appears.");
            success = true;
        }
    }

    if (!success && caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_mana_viper(int pow, god_type god, bool fail)
{
    fail_check();

    mgen_data viper = mgen_data(MONS_MANA_VIPER, BEH_FRIENDLY, &you,
                                2, SPELL_SUMMON_MANA_VIPER,
                                you.pos(), MHITYOU,
                                MG_AUTOFOE, god);
    viper.hd = (5 + div_rand_round(pow, 12));

    // Don't scale hp at the same time as their antimagic power
    viper.hp = hit_points(9, 3, 5);

    if (create_monster(viper))
        mpr("A mana viper appears with a sibilant hiss.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

// This assumes that the specified monster can go berserk.
static void _make_mons_berserk_summon(monster* mon)
{
    mon->go_berserk(false);
    mon_enchant berserk = mon->get_ench(ENCH_BERSERK);
    mon_enchant abj = mon->get_ench(ENCH_ABJ);

    // Let Trog's gifts berserk longer, and set the abjuration timeout
    // to the berserk timeout.
    berserk.duration = berserk.duration * 3 / 2;
    berserk.maxduration = berserk.duration;
    abj.duration = abj.maxduration = berserk.duration;
    mon->update_ench(berserk);
    mon->update_ench(abj);
}

// This is actually one of Trog's wrath effects.
bool summon_berserker(int pow, actor *caster, monster_type override_mons)
{
    monster_type mon = MONS_PROGRAM_BUG;

    const int dur = min(2 + (random2(pow) / 4), 6);

    if (override_mons != MONS_PROGRAM_BUG)
        mon = override_mons;
    else
    {
        if (pow <= 100)
        {
            // bears
            mon = (coinflip()) ? MONS_BLACK_BEAR : MONS_POLAR_BEAR;
        }
        else if (pow <= 140)
        {
            // ogres
            mon = (one_chance_in(3) ? MONS_TWO_HEADED_OGRE : MONS_OGRE);
        }
        else if (pow <= 180)
        {
            // trolls
            mon = random_choose_weighted(3, MONS_TROLL,
                                         3, MONS_DEEP_TROLL,
                                         2, MONS_IRON_TROLL,
                                         0);
        }
        else
        {
            // giants
            mon = (coinflip()) ? MONS_HILL_GIANT : MONS_STONE_GIANT;
        }
    }

    mgen_data mg(mon, caster ? BEH_COPY : BEH_HOSTILE, caster, dur, 0,
                 caster ? caster->pos() : you.pos(),
                 (caster && caster->is_monster())
                     ? ((monster*)caster)->foe : MHITYOU,
                 MG_AUTOFOE, GOD_TROG);

    if (!caster)
        mg.non_actor_summoner = "the rage of " + god_name(GOD_TROG, false);

    monster *mons = create_monster(mg);

    if (!mons)
        return false;

    _make_mons_berserk_summon(mons);
    return true;
}

// Not a spell. Rather, this is TSO's doing.
bool summon_holy_warrior(int pow, bool punish)
{
    mgen_data mg(random_choose(MONS_ANGEL, MONS_DAEVA, -1),
                 punish ? BEH_HOSTILE : BEH_FRIENDLY,
                 punish ? 0 : &you,
                 punish ? 0 : min(2 + (random2(pow) / 4), 6),
                 0, you.pos(), MHITYOU,
                 MG_FORCE_BEH | MG_AUTOFOE, GOD_SHINING_ONE);

    if (punish)
    {
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
        mg.non_actor_summoner = god_name(GOD_SHINING_ONE, false);
    }

    monster *summon = create_monster(mg);

    if (!summon)
        return false;

    summon->flags |= MF_ATT_CHANGE_ATTEMPT;

    if (!punish)
        mpr("You are momentarily dazzled by a brilliant light.");

    player_angers_monster(summon);
    return true;
}

/**
 * Essentially a macro to allow for a generic fail pattern to avoid leaking
 * information about invisible enemies. (Not implemented as a macro because I
 * find they create unreadable code.)
 *
 * @return SPRET_SUCCESS
 **/
static bool _fail_tukimas()
{
    mprf("You can't see a target there!");
    return false; // Waste the turn - no anti-invis tech
}

/**
 * Gets an item description for use in Tukima's Dance messages.
 **/
static string _get_item_desc(item_def* wpn, bool target_is_player)
{
    return wpn->name(target_is_player ? DESC_YOUR : DESC_THE);
}

/**
 * Checks if Tukima's Dance can actually affect the target (and anger them)
 *
 * @param mon     The targeted monster
 * @return        Whether the target can be affected by Tukima's Dance
 **/
bool tukima_affects(const monster *mon)
{
    ASSERT(mon);

    item_def* wpn = mon->weapon();
    return wpn
           && is_weapon(*wpn)
           && !is_range_weapon(*wpn)
           && !is_special_unrandom_artefact(*wpn)
           && !mons_class_is_animated_weapon(mon->type);
}

/**
 * Checks if Tukima's Dance is being cast on a valid target.
 *
 * TODO: reduce redundancy with tukima_affects()
 *
 * @param target     The spell's target.
 * @return           Whether the target is valid.
 **/
static bool _check_tukima_validity(const actor *target)
{
    bool target_is_player = target == &you;
    item_def* wpn = target->weapon();
    bool can_see_target = target_is_player || target->visible_to(&you);

    // See if the wielded item is appropriate.
    if (!wpn)
    {
        if (!can_see_target)
            return _fail_tukimas();

        if (target_is_player)
            mprf("Your %s twitch.", you.hand_name(true).c_str());
        else
        {
            mprf("%s %s twitch.",
                 apostrophise(target->name(DESC_THE)).c_str(),
                 target->hand_name(true).c_str());
        }
        return false;
    }

    if (!is_weapon(*wpn)
        || is_range_weapon(*wpn)
        || is_special_unrandom_artefact(*wpn)
        || mons_class_is_animated_weapon(target->type))
    {
        if (!can_see_target)
            return _fail_tukimas();

        if (mons_class_is_animated_weapon(target->type))
        {
            simple_monster_message((monster*)target,
                                   " is already dancing.");
        }
        else
        {
            mprf("%s vibrate%s crazily for a second.",
                 _get_item_desc(wpn, target_is_player).c_str(),
                 wpn->quantity > 1 ? "" : "s");
        }
        return false;
    }

    return true;
}


/**
 * Actually animates the weapon of the target creature (no checks).
 *
 * @param pow               Spellpower.
 * @param target            The spell's target (monster or player)
 * @param force_friendly    Whether the weapon should always be pro-player.
 **/
static void _animate_weapon(int pow, actor* target, bool force_friendly)
{
    bool target_is_player = target == &you;
    item_def* wpn = target->weapon();
    if (target_is_player)
    {
        // Clear temp branding so we don't change the brand permanently.
        if (you.duration[DUR_WEAPON_BRAND])
        {
            ASSERT(you.weapon());
            end_weapon_brand(*wpn);
        }

        // Mark weapon as "thrown", so we'll autopickup it later.
        wpn->flags |= ISFLAG_THROWN;
    }
    item_def cp = *wpn;
    // If sac love, the weapon will go after you, not the target.
    const bool sac_love = player_mutation_level(MUT_NO_LOVE);
    // Self-casting haunts yourself! MUT_NO_LOVE overrides force friendly.
    const bool friendly = (force_friendly || !target_is_player) && !sac_love;
    const int dur = min(2 + (random2(pow) / 5), 6);

    mgen_data mg(MONS_DANCING_WEAPON,
                 friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                 &you,
                 dur, SPELL_TUKIMAS_DANCE,
                 target->pos(),
                 (target_is_player || sac_love) ? MHITYOU : target->mindex(),
                 sac_love ? 0 : MG_FORCE_BEH);
    mg.props[TUKIMA_WEAPON] = cp;
    mg.props[TUKIMA_POWER] = pow;

    monster *mons = create_monster(mg);

    if (!mons)
    {
        mprf("%s twitches for a moment.",
             _get_item_desc(wpn, target_is_player).c_str());
        return;
    }

    // Don't haunt yourself if the weapon is friendly or if sac love.
    if (!force_friendly && !sac_love)
    {
        mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                   INFINITE_DURATION));
        mons->foe = target->mindex();
    }

    // We are successful.  Unwield the weapon, removing any wield effects.
    mprf("%s dances into the air!",
         _get_item_desc(wpn, target_is_player).c_str());
    if (target_is_player)
        unwield_item();
    else
    {
        monster* montarget = (monster*)target;
        const int primary_weap = montarget->inv[MSLOT_WEAPON];
        const mon_inv_type wp_slot = (primary_weap != NON_ITEM
                                      && &mitm[primary_weap] == wpn) ?
                                         MSLOT_WEAPON : MSLOT_ALT_WEAPON;
        ASSERT(montarget->inv[wp_slot] != NON_ITEM);
        ASSERT(&mitm[montarget->inv[wp_slot]] == wpn);

        montarget->unequip(*(montarget->mslot_item(wp_slot)),
                           wp_slot, 0, true);
        montarget->inv[wp_slot] = NON_ITEM;
    }

    // Find out what our god thinks before killing the item.
    conduct_type why = good_god_hates_item_handling(*wpn);
    if (!why)
        why = god_hates_item_handling(*wpn);

    wpn->clear();

    if (why)
    {
        simple_god_message(" booms: How dare you animate that foul thing!");
        did_god_conduct(why, 10, true, mons);
    }
}

/**
 * Casts Tukima's Dance, animating the weapon of the target creature (if valid)
 *
 * @param pow               Spellpower.
 * @param where             The target grid.
 * @param force_friendly    Whether the weapon should always be pro-player.
 **/
void cast_tukimas_dance(int pow, actor* target, bool force_friendly)
{
    ASSERT(target);

    if (!_check_tukima_validity(target))
        return;

    _animate_weapon(pow, target, force_friendly);
}

spret_type cast_conjure_ball_lightning(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = min(5, 2 + pow / 100 + random2(pow / 50 + 1));

    mgen_data cbl(MONS_BALL_LIGHTNING, BEH_FRIENDLY, &you, 0,
                  SPELL_CONJURE_BALL_LIGHTNING, you.pos(), MHITNOT, 0, god);
    cbl.hd = 5 + div_rand_round(pow, 20);

    for (int i = 0; i < how_many; ++i)
    {
        if (monster *ball = create_monster(cbl))
        {
            success = true;
            ball->add_ench(ENCH_SHORT_LIVED);

            // Avoid ball lightnings without targets always moving towards (0,0)
            set_random_target(ball);
        }
    }

    if (success)
        mpr("You create some ball lightning!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_lightning_spire(int pow, const coord_def& where, god_type god, bool fail)
{
    const int dur = 2;

    if (distance2(where, you.pos()) > dist_range(spell_range(SPELL_SUMMON_LIGHTNING_SPIRE,
                                                      pow))
        || !in_bounds(where))
    {
        mpr("That's too far away.");
        return SPRET_ABORT;
    }

    if (!monster_habitable_grid(MONS_HUMAN, grd(where)))
    {
        mpr("You can't construct there.");
        return SPRET_ABORT;
    }

    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(mons))
        {
            mpr("That space is already occupied.");
            return SPRET_ABORT;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see is blocking your construction!");
        return SPRET_SUCCESS;
    }

    fail_check();

    mgen_data spire = mgen_data(MONS_LIGHTNING_SPIRE, BEH_FRIENDLY, &you, dur,
                                SPELL_SUMMON_LIGHTNING_SPIRE, where, MHITYOU,
                                MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE, god);
    spire.hd = max(1, div_rand_round(pow, 10));

    if (create_monster(spire))
    {
        if (!silenced(where))
            mpr("An electric hum fills the air.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;

}

spret_type cast_summon_guardian_golem(int pow, god_type god, bool fail)
{
    fail_check();

    mgen_data golem = mgen_data(MONS_GUARDIAN_GOLEM, BEH_FRIENDLY, &you, 3,
                                SPELL_SUMMON_GUARDIAN_GOLEM, you.pos(), MHITYOU,
                                MG_FORCE_BEH, god);
    golem.hd = 4 + div_rand_round(pow, 16);

    monster* mons = (create_monster(golem));

    if (mons)
    {
        // Immediately apply injury bond
        guardian_golem_bond(mons);

        mpr("A guardian golem appears, shielding your allies.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}


spret_type cast_call_imp(int pow, god_type god, bool fail)
{
    fail_check();
    monster_type mon = MONS_PROGRAM_BUG;

    if (random2(pow) >= 46 || one_chance_in(6))
        mon = one_chance_in(3) ? MONS_IRON_IMP : MONS_SHADOW_IMP;
    else
        mon = one_chance_in(3) ? MONS_WHITE_IMP : MONS_CRIMSON_IMP;

    const int dur = min(2 + (random2(pow) / 4), 6);

    if (monster *imp = create_monster(
            mgen_data(mon, BEH_FRIENDLY, &you, dur, SPELL_CALL_IMP,
                      you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, god)))
    {
        mpr((mon == MONS_WHITE_IMP)  ? "A beastly little devil appears in a puff of frigid air." :
            (mon == MONS_IRON_IMP)   ? "A metallic apparition takes form in the air." :
            (mon == MONS_SHADOW_IMP) ? "A shadowy apparition takes form in the air."
                                     : "A beastly little devil appears in a puff of flame.");

        if (!player_angers_monster(imp))
            _monster_greeting(imp, "_friendly_imp_greeting");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  monster_type mon, int dur, bool friendly,
                                  bool charmed, bool quiet)
{
    bool success = false;

    if (monster *demon = create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY :
                          charmed ? BEH_CHARMED : BEH_HOSTILE, &you,
                      dur, spell, you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, god)))
    {
        success = true;

        mpr("A demon appears!");

        if (!player_angers_monster(demon) && !friendly)
        {
            mpr(charmed ? "You don't feel so good about this..."
                        : "It doesn't seem very happy.");
        }
        else if (friendly)
        {
            if (mon == MONS_CRIMSON_IMP || mon == MONS_WHITE_IMP
                || mon == MONS_IRON_IMP || mon == MONS_SHADOW_IMP)
            {
                _monster_greeting(demon, "_friendly_imp_greeting");
            }
        }

        if (charmed && !friendly)
        {
            int charm_dur = random_range(15 + pow / 14, 27 + pow / 11)
                            * BASELINE_DELAY;

            mon_enchant charm = demon->get_ench(ENCH_CHARM);
            charm.duration = charm_dur;
            demon->update_ench(charm);

            // Ensure that temporarily-charmed demons will outlast their charm
            mon_enchant abj = demon->get_ench(ENCH_ABJ);
            if (charm.duration + 100 > abj.duration)
            {
                abj.duration = charm.duration + 100;
                demon->update_ench(abj);
            }

            // Affects messaging, and stuns demon a turn upon charm wearing off
            demon->props["charmed_demon"].get_bool() = true;
        }
    }

    return success;
}

static bool _summon_common_demon(int pow, god_type god, int spell, bool quiet)
{
    const int chance = 70 - (pow / 3);
    monster_type type = MONS_PROGRAM_BUG;

    if (x_chance_in_y(chance, 100))
    {
        // tier 4
        type = random_choose(MONS_BLUE_DEVIL,    MONS_RUST_DEVIL,
                             MONS_ORANGE_DEMON,  MONS_RED_DEVIL,
                             MONS_SIXFIRHY,      MONS_HELLWING,
                             -1);
    }
    else
    {
        // tier 3
        type = random_choose(MONS_SUN_DEMON,     MONS_SOUL_EATER,
                             MONS_ICE_DEVIL,     MONS_SMOKE_DEMON,
                             MONS_NEQOXEC,       MONS_YNOXINUL,
                             MONS_CHAOS_SPAWN,
                             -1);
    }

    return _summon_demon_wrapper(pow, god, spell, type,
                                 min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

static bool _summon_greater_demon(int pow, god_type god, int spell, bool quiet)
{
    monster_type mon = summon_any_demon(RANDOM_DEMON_GREATER);

    const bool charmed = (random2(pow) > 5);
    const bool friendly = (charmed && mons_demon_tier(mon) == 2);

    return _summon_demon_wrapper(pow, god, spell, mon,
                                 4, friendly, charmed, quiet);
}

bool summon_demon_type(monster_type mon, int pow, god_type god,
                       int spell)
{
    return _summon_demon_wrapper(pow, god, spell, mon,
                                 min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, false);
}

spret_type cast_summon_demon(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You open a gate to Pandemonium!");

    if (!_summon_common_demon(pow, god, SPELL_SUMMON_DEMON, false))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_greater_demon(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("You open a gate to Pandemonium!");

    if (!_summon_greater_demon(pow, god, SPELL_SUMMON_GREATER_DEMON, false))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static monster_type _zotdef_shadow()
{
    for (int tries = 0; tries < 100; tries++)
    {
        monster_type mc = env.mons_alloc[random2(MAX_MONS_ALLOC)];
        if (!invalid_monster_type(mc) && !mons_is_unique(mc))
            return mc;
    }

    return RANDOM_COMPATIBLE_MONSTER;
}

spret_type cast_shadow_creatures(int st, god_type god, level_id place,
                                 bool fail)
{
    fail_check();
    const bool scroll = (st == MON_SUMM_SCROLL);
    mpr("Wisps of shadow whirl around you...");

    monster_type critter = RANDOM_COMPATIBLE_MONSTER;
    if (crawl_state.game_is_zotdef())
        critter = _zotdef_shadow();

    int num = (scroll ? roll_dice(2, 2) : 1);
    int num_created = 0;

    for (int i = 0; i < num; ++i)
    {
        if (monster *mons = create_monster(
            mgen_data(critter, BEH_FRIENDLY, &you,
                      (scroll ? 2 : 1), // This duration is only used for band members.
                      st, you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, god,
                      MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE, place), false))
        {
            // In the rare cases that a specific spell set of a monster will
            // cause anger, even if others do not, try rerolling
            int tries = 0;
            while (player_will_anger_monster(mons) && ++tries <= 20)
            {
                // Save the enchantments, particularly ENCH_SUMMONED etc.
                mon_enchant_list ench = mons->enchantments;
                FixedBitVector<NUM_ENCHANTMENTS> cache = mons->ench_cache;
                define_monster(mons);
                mons->enchantments = ench;
                mons->ench_cache = cache;
            }

            // If we didn't find a valid spell set yet, just give up
            if (tries > 20)
                monster_die(mons, KILL_RESET, NON_MONSTER);
            else
            {
                // Choose a new duration based on HD.
                int x = max(mons->get_experience_level() - 3, 1);
                int d = div_rand_round(17,x);
                if (scroll)
                    d++;
                if (d < 1)
                    d = 1;
                if (d > 4)
                    d = 4;
                mon_enchant me = mon_enchant(ENCH_ABJ, d);
                me.set_duration(mons, &me);
                mons->update_ench(me);

                // Set summon ID, to share summon cap with its band members
                mons->props["summon_id"].get_int() = mons->mid;
            }

            // Remove any band members that would turn hostile, and link their
            // summon IDs
            for (monster_iterator mi; mi; ++mi)
            {
                if (testbits(mi->flags, MF_BAND_MEMBER)
                    && (mid_t) mi->props["band_leader"].get_int() == mons->mid)
                {
                    if (player_will_anger_monster(*mi))
                        monster_die(*mi, KILL_RESET, NON_MONSTER);

                    mi->props["summon_id"].get_int() = mons->mid;
                }
            }

            num_created++;
        }
    }

    if (!num_created)
        mpr("The shadows disperse without effect.");

    return SPRET_SUCCESS;
}

bool can_cast_malign_gateway()
{
    timeout_malign_gateways(0);

    return count_malign_gateways() < 1;
}

coord_def find_gateway_location(actor* caster)
{
    vector<coord_def> points;

    bool xray = you.xray_vision;
    you.xray_vision = false;

    for (unsigned i = 0; i < 8; ++i)
    {
        coord_def delta = Compass[i];
        coord_def test = coord_def(-1, -1);

        for (int t = 0; t < 11; t++)
        {
            test = caster->pos() + (delta * (2+t));
            if (!in_bounds(test) || !feat_is_malign_gateway_suitable(grd(test))
                || actor_at(test)
                || count_neighbours_with_func(test, &feat_is_solid) != 0
                || !caster->see_cell_no_trans(test))
            {
                continue;
            }

            points.push_back(test);
        }
    }

    you.xray_vision = xray;

    if (points.empty())
        return coord_def(0, 0);

    return points[random2(points.size())];
}

spret_type cast_malign_gateway(actor * caster, int pow, god_type god, bool fail)
{
    coord_def point = find_gateway_location(caster);
    bool success = (point != coord_def(0, 0));

    bool is_player = (caster->is_player());

    if (success)
    {
        fail_check();

        const int malign_gateway_duration = BASELINE_DELAY * (random2(3) + 2);
        env.markers.add(new map_malign_gateway_marker(point,
                                malign_gateway_duration,
                                is_player,
                                is_player ? ""
                                    : caster->as_monster()->full_name(DESC_A, true),
                                is_player ? BEH_FRIENDLY
                                    : attitude_creation_behavior(
                                      caster->as_monster()->attitude),
                                god,
                                pow));
        env.markers.clear_need_activate();
        env.grid(point) = DNGN_MALIGN_GATEWAY;
        set_terrain_changed(point);

        noisy(spell_effect_noise(SPELL_MALIGN_GATEWAY), point);
        mprf(MSGCH_WARN, "The dungeon shakes, a horrible noise fills the air, "
                         "and a portal to some otherworldly place is opened!");

        if (one_chance_in(5) && caster->is_player())
        {
            // if someone deletes the db, no message is ok
            mpr(getMiscString("SHT_int_loss").c_str());
            // Messages the same as for SHT, as they are currently (10/10) generic.
            lose_stat(STAT_INT, 1 + random2(3), false, "opening a malign portal");
        }

        return SPRET_SUCCESS;
    }
    // We don't care if monsters fail to cast it.
    if (is_player)
        mpr("A gateway cannot be opened in this cramped space!");

    return SPRET_ABORT;
}

spret_type cast_summon_horrible_things(int pow, god_type god, bool fail)
{
    fail_check();
    if (one_chance_in(5))
    {
        // if someone deletes the db, no message is ok
        mpr(getMiscString("SHT_int_loss").c_str());
        lose_stat(STAT_INT, 1 + random2(2), false, "summoning horrible things");
    }

    int num_abominations = random_range(2, 4) + x_chance_in_y(pow, 200);
    int num_tmons = random2(pow) > 120 ? 2 : random2(pow) > 50 ? 1 : 0;

    if (num_tmons == 0)
        num_abominations++;

    int count = 0;

    while (num_abominations-- > 0)
    {
        if (monster *mons = create_monster(
               mgen_data(MONS_ABOMINATION_LARGE, BEH_FRIENDLY, &you,
                         3, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH | MG_AUTOFOE, god)))
        {
            count++;
            player_angers_monster(mons);
        }
    }

    while (num_tmons-- > 0)
    {
        if (monster *mons = create_monster(
               mgen_data(MONS_TENTACLED_MONSTROSITY, BEH_FRIENDLY, &you,
                         3, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH | MG_AUTOFOE, god)))
        {
            count++;
            player_angers_monster(mons);
        }
    }

    if (!count)
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static bool _water_adjacent(coord_def p)
{
    for (orth_adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_water(grd(*ai)))
            return true;
    }

    return false;
}

/**
 * Cast summon forest.
 *
 * @param caster The caster.
 * @param pow    The spell power.
 * @param god    The god of the summoned dryad (usually the caster's).
 * @param fail   Did this spell miscast? If true, abort the cast.
 * @return       SPRET_ABORT if a summoning area couldn't be found,
 *               SPRET_FAIL if one could be found but we miscast, and
 *               SPRET_SUCCESS if the spell was succesfully cast.
*/
spret_type cast_summon_forest(actor* caster, int pow, god_type god, bool fail)
{
    const int duration = random_range(120 + pow, 200 + pow * 3 / 2);

    // Is this area open enough to summon a forest?
    bool success = false;
    for (adjacent_iterator ai(caster->pos(), false); ai; ++ai)
    {
        if (count_neighbours_with_func(*ai, &feat_is_solid) == 0)
        {
            success = true;
            break;
        }
    }

    if (success)
    {
        fail_check();
        // Replace some rock walls with trees, then scatter a smaller number
        // of trees on unoccupied floor (such that they do not break connectivity)
        for (distance_iterator di(caster->pos(), false, true, LOS_RADIUS); di; ++di)
        {
           if ((grd(*di) == DNGN_ROCK_WALL && x_chance_in_y(pow, 150))
               || ((grd(*di) == DNGN_FLOOR && x_chance_in_y(pow, 1250)
                    && !actor_at(*di) && !plant_forbidden_at(*di, true))))
           {
                temp_change_terrain(*di, DNGN_TREE, duration,
                                    TERRAIN_CHANGE_FORESTED);
           }
        }

        // Maybe make a pond
        if (coinflip())
        {
            coord_def pond = find_gateway_location(caster);
            int num = random_range(10, 22);
            int deep = (!one_chance_in(3) ? div_rand_round(num, 3) : 0);

            for (distance_iterator di(pond, true, false, 4); di && num > 0; ++di)
            {
                if (grd(*di) == DNGN_FLOOR
                    && (di.radius() == 0 || _water_adjacent(*di))
                    && x_chance_in_y(4, di.radius() + 3))
                {
                    num--;
                    deep--;

                    dungeon_feature_type feat = DNGN_SHALLOW_WATER;
                    if (deep > 0 && *di != you.pos())
                    {
                        monster* mon = monster_at(*di);
                        if (!mon || mon->is_habitable_feat(DNGN_DEEP_WATER))
                            feat = DNGN_DEEP_WATER;
                    }

                    temp_change_terrain(*di, feat, duration, TERRAIN_CHANGE_FORESTED);
                }
            }
        }

        mpr("A forested plane collides here with a resounding crunch!");
        noisy(spell_effect_noise(SPELL_SUMMON_FOREST), caster->pos());

        mgen_data dryad_data = mgen_data(MONS_DRYAD, BEH_FRIENDLY, &you, 1,
                                         SPELL_SUMMON_FOREST, caster->pos(),
                                         MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, god);
        dryad_data.hd = 5 + div_rand_round(pow, 18);

        if (monster *dryad = create_monster(dryad_data))
        {
            player_angers_monster(dryad);
            mon_enchant abj = dryad->get_ench(ENCH_ABJ);
            abj.duration = duration - 10;
            dryad->update_ench(abj);

            // Pre-awaken the forest just summoned.
            bolt dummy;
            mons_cast(dryad, dummy, SPELL_AWAKEN_FOREST,
                      dryad->spell_slot_flags(SPELL_AWAKEN_FOREST));
        }

        you.duration[DUR_FORESTED] = duration;

        return SPRET_SUCCESS;
    }

    mpr("You need more open space to cast this spell.");
    return SPRET_ABORT;
}

static bool _animatable_remains(const item_def& item)
{
    return item.base_type == OBJ_CORPSES
        && mons_class_can_be_zombified(item.mon_type)
        // the above allows spectrals/etc
        && (mons_zombifiable(item.mon_type)
            || mons_skeleton(item.mon_type));
}

// Try to equip the skeleton/zombie with the objects it died with.  This
// excludes holy items, items which were dropped by the player onto the
// corpse, and corpses which were picked up and moved by the player, so
// the player can't equip their undead slaves with items of their
// choice.
//
// The item selection logic has one problem: if a first monster without
// any items dies and leaves a corpse, and then a second monster with
// items dies on the same spot but doesn't leave a corpse, then the
// undead can be equipped with the second monster's items if the second
// monster is either of the same type as the first, or if the second
// monster wasn't killed by the player or a player's pet.
static void _equip_undead(const coord_def &a, int corps, monster *mon, monster_type monnum)
{
    if (mons_class_itemuse(monnum) < MONUSE_STARTING_EQUIPMENT)
        return;

    // If the player picked up and dropped the corpse, then all its
    // original equipment fell off.
    if (mitm[corps].flags & ISFLAG_DROPPED)
        return;

    // A monster's corpse is last in the linked list after its items,
    // so (for example) the first item after the second-to-last corpse
    // is the first item belonging to the last corpse.
    int objl      = igrd(a);
    int first_obj = NON_ITEM;

    while (objl != NON_ITEM && objl != corps)
    {
        item_def item(mitm[objl]);

        if (item.base_type != OBJ_CORPSES && first_obj == NON_ITEM)
            first_obj = objl;

        objl = item.link;
    }

    // If the corpse was being drained when it was raised the item is
    // already destroyed.
    ASSERT(objl == corps || objl == NON_ITEM);

    if (first_obj == NON_ITEM)
        return;

    // Iterate backwards over the list, since the items earlier in the
    // linked list were dropped most recently and hence more likely to
    // be items the monster didn't die with.
    vector<int> item_list;
    objl = first_obj;
    while (objl != NON_ITEM && objl != corps)
    {
        item_list.push_back(objl);
        objl = mitm[objl].link;
    }

    for (int i = item_list.size() - 1; i >= 0; --i)
    {
        objl = item_list[i];
        item_def &item(mitm[objl]);

        // Stop equipping monster if the item probably didn't originally
        // belong to the monster.
        if ((origin_known(item) && item.orig_monnum != monnum)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN))
            || item.base_type == OBJ_CORPSES)
        {
            return;
        }

        // Don't equip the undead with holy items.
        if (is_holy_item(item))
            continue;

        mon_inv_type mslot;

        switch (item.base_type)
        {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_RODS:
        {
            const bool weapon = mon->inv[MSLOT_WEAPON] != NON_ITEM;
            const bool alt_weapon = mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM;

            if ((weapon && !alt_weapon) || (!weapon && alt_weapon))
                mslot = !weapon ? MSLOT_WEAPON : MSLOT_ALT_WEAPON;
            else
                mslot = MSLOT_WEAPON;

            // Stupid undead can't use ranged weapons.
            if (!is_range_weapon(item))
                break;

            continue;
        }

        case OBJ_ARMOUR:
            mslot = equip_slot_to_mslot(get_armour_slot(item));

            // A piece of armour which can't be worn indicates that this
            // and further items weren't the equipment the monster died
            // with.
            if (mslot == NUM_MONSTER_SLOTS)
                return;
            break;

        // Stupid undead can't use missiles.
        case OBJ_MISSILES:
            continue;

        // Stupid undead can't use gold.
        case OBJ_GOLD:
            continue;

        // Stupid undead can't use wands.
        case OBJ_WANDS:
            continue;

        // Stupid undead can't use scrolls.
        case OBJ_SCROLLS:
            continue;

        // Stupid undead can't use potions.
        case OBJ_POTIONS:
            continue;

        // Stupid undead can't use jewellery.
        case OBJ_JEWELLERY:
            continue;

        // Stupid undead can't use miscellaneous objects.
        case OBJ_MISCELLANY:
            continue;

        default:
            continue;
        }

        // Two different items going into the same slot indicate that
        // this and further items weren't equipment the monster died
        // with.
        if (mon->inv[mslot] != NON_ITEM)
            return;

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->pickup_item(mitm[objl], false, true);
    }
}

// Displays message when raising dead with Animate Skeleton or Animate Dead.
static void _display_undead_motions(int motions)
{
    vector<string> motions_list;

    // Check bitfield from _raise_remains for types of corpse(s) being animated.
    if (motions & DEAD_ARE_WALKING)
        motions_list.push_back("walking");
    if (motions & DEAD_ARE_HOPPING)
        motions_list.push_back("hopping");
    if (motions & DEAD_ARE_SWIMMING)
        motions_list.push_back("swimming");
    if (motions & DEAD_ARE_FLYING)
        motions_list.push_back("flying");
    if (motions & DEAD_ARE_SLITHERING)
        motions_list.push_back("slithering");
    if (motions & DEAD_ARE_CRAWLING)
        motions_list.push_back("crawling");

    // Prevents the message from getting too long and spammy.
    if (motions_list.size() > 3)
        mpr("The dead have arisen!");
    else
    {
        mprf("The dead are %s!", comma_separated_line(motions_list.begin(),
             motions_list.end()).c_str());
    }
}

static int _corpse_number(const item_def &item)
{
    return item.props.exists(MONSTER_NUMBER)
           ? item.props[MONSTER_NUMBER].get_short(): 0;
}

static bool _raise_remains(const coord_def &pos, int corps, beh_type beha,
                           unsigned short hitting, actor *as, string nas,
                           god_type god, bool actual, bool force_beh,
                           monster **raised, int* motions_r)
{
    if (raised)
        *raised = 0;

    const item_def& item = mitm[corps];

    if (!_animatable_remains(item))
        return false;

    if (!actual)
        return true;

    monster_type zombie_type = item.mon_type;
    // hack: don't re-froggify poor prince ribbit after death
    if (zombie_type == MONS_PRINCE_RIBBIT)
        zombie_type = MONS_HUMAN;

    const int hd     = (item.props.exists(MONSTER_HIT_DICE)) ?
                           item.props[MONSTER_HIT_DICE].get_short() : 0;
    const int number = _corpse_number(item);

    // Save the corpse name before because it can get destroyed if it is
    // being drained and the raising interrupts it.
    uint64_t name_type = 0;
    string name;
    if (is_named_corpse(item))
        name = get_corpse_name(item, &name_type);

    // Headless hydras cannot be raised, sorry.
    if (zombie_type == MONS_HYDRA && number == 0)
    {
        if (you.see_cell(pos))
        {
            mprf("The headless hydra %s sways and immediately collapses!",
                 item.sub_type == CORPSE_BODY ? "corpse" : "skeleton");
        }
        return false;
    }

    monster_type mon = item.sub_type == CORPSE_BODY ? MONS_ZOMBIE
                                                    : MONS_SKELETON;
    monster_type monnum = static_cast<monster_type>(item.orig_monnum);
    // hack: don't re-froggify poor prince ribbit after death
    if (monnum == MONS_PRINCE_RIBBIT)
        monnum = MONS_HUMAN;

    if (mon == MONS_ZOMBIE && !mons_zombifiable(zombie_type))
    {
        ASSERT(mons_skeleton(zombie_type));
        if (as == &you)
        {
            mpr("The flesh is too rotten for a proper zombie; "
                "only a skeleton remains.");
        }
        mon = MONS_SKELETON;
    }

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    mgen_data mg(mon, beha, as, 0, 0, pos, hitting,
                 MG_FORCE_BEH|MG_FORCE_PLACE|MG_AUTOFOE,
                 god, monnum, number);

    // No experience for monsters animated by god wrath or the Sword of
    // Zonguldrok.
    if (nas != "")
        mg.extra_flags |= MF_NO_REWARD;

    mg.non_actor_summoner = nas;

    monster *mons = create_monster(mg);

    if (raised)
        *raised = mons;

    if (!mons)
        return false;

    // If the original monster has been drained or levelled up, its HD
    // might be different from its class HD, in which case its HP should
    // be rerolled to match.
    if (mons->get_experience_level() != hd)
    {
        mons->set_hit_dice(max(hd, 1));
        roll_zombie_hp(mons);
    }

    if (!name.empty()
        && (name_type == 0 || (name_type & MF_NAME_MASK) == MF_NAME_REPLACE))
    {
        name_zombie(mons, monnum, name);
    }

    // Re-equip the zombie.
    _equip_undead(pos, corps, mons, monnum);

    // Destroy the monster's corpse, as it's no longer needed.
    item_was_destroyed(item);
    destroy_item(corps);

    if (!force_beh)
        player_angers_monster(mons);

    // Bitfield for motions - determines text displayed when animating dead.
    if (mons_class_primary_habitat(zombie_type)    == HT_WATER
        || mons_class_primary_habitat(zombie_type) == HT_LAVA)
    {
        *motions_r |= DEAD_ARE_SWIMMING;
    }
    else if (mons_class_flies(zombie_type))
        *motions_r |= DEAD_ARE_FLYING;
    else if (mons_genus(zombie_type)    == MONS_SNAKE
             || mons_genus(zombie_type) == MONS_NAGA
             || mons_genus(zombie_type) == MONS_GUARDIAN_SERPENT
             || mons_genus(zombie_type) == MONS_ELEPHANT_SLUG
             || mons_genus(zombie_type) == MONS_GIANT_LEECH
             || mons_genus(zombie_type) == MONS_WORM)
    {
        *motions_r |= DEAD_ARE_SLITHERING;
    }
    else if (mons_genus(zombie_type)    == MONS_GIANT_FROG
             || mons_genus(zombie_type) == MONS_BLINK_FROG
             || mons_genus(zombie_type) == MONS_QUOKKA)
    {
        *motions_r |= DEAD_ARE_HOPPING;
    }
    else if (mons_genus(zombie_type)    == MONS_WORKER_ANT
             || mons_genus(zombie_type) == MONS_GOLIATH_BEETLE
             || mons_base_char(zombie_type) == 's') // many genera
    {
        *motions_r |= DEAD_ARE_CRAWLING;
    }
    else
        *motions_r |= DEAD_ARE_WALKING;

    return true;
}

// Note that quiet will *not* suppress the message about a corpse
// you are butchering being animated.
// This is called for Animate Skeleton and from animate_dead.
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as, string nas,
                    god_type god, bool actual,
                    bool quiet, bool force_beh,
                    monster** mon, int* motions_r)
{
    if (is_sanctuary(a))
        return 0;

    int number_found = 0;
    bool success = false;
    int motions = 0;

    // Search all the items on the ground for a corpse.
    for (stack_iterator si(a, true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES
            && (class_allowed == CORPSE_BODY
                || si->sub_type == CORPSE_SKELETON))
        {
            number_found++;

            if (!_animatable_remains(*si))
                continue;

            const bool was_draining = is_being_drained(*si);
            const bool was_butchering = is_being_butchered(*si);

            success = _raise_remains(a, si.link(), beha, hitting, as, nas,
                                     god, actual, force_beh, mon,
                                     &motions);

            if (actual && success)
            {
                // Ignore quiet.
                if (was_butchering || was_draining)
                {
                    mprf("The corpse you are %s rises to %s!",
                         was_draining ? "drinking from"
                                      : "butchering",
                         beha == BEH_FRIENDLY ? "join your ranks"
                                              : "attack");
                }

                if (!quiet && you.see_cell(a))
                    _display_undead_motions(motions);

                if (was_butchering)
                    xom_is_stimulated(200);
            }

            break;
        }
    }

    if (motions_r && you.see_cell(a))
        *motions_r |= motions;

    if (number_found == 0)
        return -1;

    if (!success)
        return 0;

    return 1;
}

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as, string nas, god_type god, bool actual)
{
    UNUSED(pow);

    int number_raised = 0;
    int number_seen   = 0;
    int motions       = 0;

    for (radius_iterator ri(caster->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        // There may be many corpses on the same spot.
        while (animate_remains(*ri, CORPSE_BODY, beha, hitting, as, nas, god,
                               actual, true, 0, 0, &motions) > 0)
        {
            number_raised++;
            if (you.see_cell(*ri))
                number_seen++;

            // For the tracer, we don't care about exact count (and the
            // corpse is not gone).
            if (!actual)
                break;
        }
    }

    if (actual && number_seen > 0)
        _display_undead_motions(motions);

    return number_raised;
}

spret_type cast_animate_skeleton(god_type god, bool fail)
{
    bool found = false;

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES
            && mons_class_can_be_zombified(si->mon_type)
            && mons_skeleton(si->mon_type))
        {
            found = true;
        }
    }

    if (!found)
    {
        mpr("There is nothing here that can be animated!");
        return SPRET_ABORT;
    }

    fail_check();
    canned_msg(MSG_ANIMATE_REMAINS);

    // First, we try to animate a skeleton if there is one.
    if (animate_remains(you.pos(), CORPSE_SKELETON, BEH_FRIENDLY,
                        MHITYOU, &you, "", god) != -1)
    {
        return SPRET_SUCCESS;
    }

    // If not, look for a corpse and butcher it.
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
            && mons_skeleton(si->mon_type)
            && mons_class_can_be_zombified(si->mon_type))
        {
            butcher_corpse(*si, MB_TRUE);
            mpr("Before your eyes, flesh is ripped from the corpse!");
            if (Options.chunks_autopickup)
                request_autopickup();
            // Only convert the top one.
            break;
        }
    }

    // Now we try again to animate a skeleton.
    if (animate_remains(you.pos(), CORPSE_SKELETON, BEH_FRIENDLY,
                        MHITYOU, &you, "", god) < 0)
    {
        mpr("There is no skeleton here to animate!");
    }

    return SPRET_SUCCESS;
}

spret_type cast_animate_dead(int pow, god_type god, bool fail)
{
    fail_check();
    canned_msg(MSG_CALL_DEAD);

    if (!animate_dead(&you, pow + 1, BEH_FRIENDLY, MHITYOU, &you, "", god))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

/**
 * Have the player cast simulacrum.
 *
 * @param pow The spell power.
 * @param god The god casting the spell.
 * @param fail If true, return SPRET_FAIL unless the spell is aborted.
 * @returns SPRET_ABORT if no viable corpse was at the player's location,
 *          otherwise SPRET_TRUE or SPRET_FAIL based on fail.
 */
spret_type cast_simulacrum(int pow, god_type god, bool fail)
{
    bool found = false;
    int co = -1;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES
            && si->sub_type == CORPSE_BODY
            && mons_class_can_be_zombified(si->mon_type))
        {
            found = true;
            co = si->index();
        }
    }

    if (!found)
    {
        mpr("There is nothing here that can be animated!");
        return SPRET_ABORT;
    }

    fail_check();
    canned_msg(MSG_ANIMATE_REMAINS);

    item_def& corpse = mitm[co];
    const int mon_number = _corpse_number(corpse);
    // How many simulacra can this particular monster give at maximum.
    int num_sim  = 1 + random2(mons_weight(corpse.mon_type) / 150);
    num_sim  = stepdown_value(num_sim, 4, 4, 12, 12);

    mgen_data mg(MONS_SIMULACRUM, BEH_FRIENDLY, &you, 0, SPELL_SIMULACRUM,
                 you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE, god,
                 corpse.mon_type, mon_number);

    // Can't create more than the max for the monster.
    int how_many = min(8, 4 + random2(pow) / 20);
    how_many = min<int>(how_many, num_sim);

    // Avoid headless hydras. Unlike Animate Dead, still consume the flesh.
    if (corpse.mon_type == MONS_HYDRA && mon_number == 0)
    {
        // No monster to conj_verb with :(
        mprf("The headless hydra simulacr%s immediately collapse%s into snow!",
             how_many == 1 ? "um" : "a", how_many == 1 ? "s" : "");
        if (!turn_corpse_into_skeleton(corpse))
            butcher_corpse(corpse, MB_FALSE, false);
        return SPRET_SUCCESS;
    }

    int count = 0;
    for (int i = 0; i < how_many; ++i)
    {
        // Use the original monster type as the zombified type here,
        // to get the proper stats from it.
        if (monster *sim = create_monster(mg))
        {
            count++;
            player_angers_monster(sim);
            sim->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
        }
    }

    if (count)
    {
        if (!turn_corpse_into_skeleton(corpse))
            butcher_corpse(corpse, MB_FALSE, false);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

/**
 * Have a monster cast simulacrum.
 *
 * @param mon The monster casting the spell.
 * @param actual If false, return true if the spell would have succeeded on at
 *               least one corpse, but don't cast.
 * @returns True if at least one simulacrum was created, or if actual is true,
 *          if one would have been created.
 */
bool monster_simulacrum(monster *mon, bool actual)
{
    // You can see the spell being cast, not necessarily the caster.
    const bool cast_visible = you.see_cell(mon->pos());
    bool did_creation = false;
    int num_seen = 0;

    dprf("trying to cast simulacrum");
    for (radius_iterator ri(mon->pos(), LOS_NO_TRANS); ri; ++ri)
    {

        // Search all the items on the ground for a corpse.
        for (stack_iterator si(*ri, true); si; ++si)
        {
            // Safe on non-corpses.
            const int mon_number = _corpse_number(*si);

            if (si->base_type != OBJ_CORPSES
                || si->sub_type != CORPSE_BODY
                || !mons_class_can_be_zombified(si->mon_type)
                || si->mon_type == MONS_HYDRA && mon_number == 0)
            {
                continue;
            }

            if (!actual)
                return true;

            int co = si->index();
            // Create half as many as the player version.
            int how_many = 1 + random2(mons_weight(mitm[co].mon_type) / 300);
            how_many  = stepdown_value(how_many, 2, 2, 6, 6);
            bool was_draining = is_being_drained(*si);
            bool was_butchering = is_being_butchered(*si);
            bool was_successful = false;
            for (int i = 0; i < how_many; ++i)
            {
                // Use the original monster type as the zombified type here,
                // to get the proper stats from it.
                if (monster *sim = create_monster(
                        mgen_data(MONS_SIMULACRUM, SAME_ATTITUDE(mon), mon, 0,
                                  SPELL_SIMULACRUM, *ri, mon->foe,
                                  MG_FORCE_BEH
                                  | (cast_visible ? MG_DONT_COME : 0),
                                  mon->god,
                                  mitm[co].mon_type, mon_number)))
                {
                    was_successful = true;
                    player_angers_monster(sim);
                    sim->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
                    if (you.can_see(sim))
                        num_seen++;
                }
            }

            if (was_successful)
            {
                did_creation = true;
                turn_corpse_into_skeleton(mitm[co]);
                // Ignore quiet.
                if (was_butchering || was_draining)
                {
                    mprf("The flesh of the corpse you are %s vaporises!",
                         was_draining ? "drinking from" : "butchering");
                    xom_is_stimulated(200);
                }

            }
        }
    }

    if (num_seen > 1)
        mprf("Some icy apparitions appear!");
    else if (num_seen == 1)
        mprf("An icy apparition appears!");

    return did_creation;
}

// Return a definite/indefinite article for (number) things.
static const char *_count_article(int number, bool definite)
{
    if (number == 0)
        return "No";
    else if (definite)
        return "The";
    else if (number == 1)
        return "A";
    else
        return "Some";
}

bool twisted_resurrection(actor *caster, int pow, beh_type beha,
                          unsigned short foe, god_type god, bool actual)
{
    int num_orcs = 0;
    int num_holy = 0;

    // In a tracer (actual == false), num_crawlies counts the number of
    // affected corpses. When actual == true, these count the number of
    // crawling corpses, macabre masses, and lost corpses, respectively.
    int num_crawlies = 0;
    int num_masses = 0;
    int num_lost = 0;

    // ...and the number of each that were seen by the player.
    int seen_crawlies = 0;
    int seen_masses = 0;
    int seen_lost = 0;
    int seen_lost_piles = 0;

    for (radius_iterator ri(caster->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        int num_corpses = 0;
        int total_mass = 0;
        const bool visible = you.see_cell(*ri);

        // Count up number/size of corpses at this location.
        for (stack_iterator si(*ri); si; ++si)
        {
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
            {
                if (!actual)
                {
                    ++num_crawlies;
                    continue;
                }

                if (mons_genus(si->mon_type) == MONS_ORC)
                    num_orcs++;
                if (mons_class_holiness(si->mon_type) == MH_HOLY)
                    num_holy++;

                total_mass += mons_weight(si->mon_type);

                ++num_corpses;
                item_was_destroyed(*si);
                destroy_item(si->index());
            }
        }

        if (!actual || num_corpses == 0)
            continue;

        // 20 aum per HD at max power; 30 at 100 power; and 60 at 0 power.
        // 10 aum per HD at 500 power (monster version).
        int hd = div_rand_round((pow + 100) * total_mass, (200*300));

        if (hd <= 0)
        {
            num_lost += num_corpses;
            if (visible)
            {
                seen_lost += num_corpses;
                seen_lost_piles++;
            }
            continue;
        }

        // Getting a huge abomination shouldn't be too easy.
        if (hd > 15)
            hd = 15 + (hd - 15) / 2;

        hd = min(hd, 30);

        monster_type montype;

        if (hd >= 11 && num_corpses > 2)
            montype = MONS_ABOMINATION_LARGE;
        else if (hd >= 6 && num_corpses > 1)
            montype = MONS_ABOMINATION_SMALL;
        else if (num_corpses > 1)
            montype = MONS_MACABRE_MASS;
        else
            montype = MONS_CRAWLING_CORPSE;

        mgen_data mg(montype, beha, caster, 0, 0, *ri, foe,
                     MG_FORCE_BEH | MG_AUTOFOE, god);
        if (monster *mons = create_monster(mg))
        {
            // Set hit dice, AC, and HP.
            init_abomination(mons, hd);

            mons->god = god;

            if (num_corpses > 1)
            {
                ++num_masses;
                if (visible)
                    ++seen_masses;
            }
            else
            {
                ++num_crawlies;
                if (visible)
                    ++seen_crawlies;
            }
        }
        else
        {
            num_lost += num_corpses;
            if (visible)
            {
                seen_lost += num_corpses;
                seen_lost_piles++;
            }
        }
    }

    // Monsters shouldn't bother casting Twisted Res for just a single corpse.
    if (!actual)
        return num_crawlies >= (caster->is_player() ? 1 : 2);

    if (num_lost + num_crawlies + num_masses == 0)
        return false;

    if (seen_lost)
    {
        mprf("%s %s into %s!",
             _count_article(seen_lost, seen_crawlies + seen_masses == 0),
             seen_lost == 1 ? "corpse collapses" : "corpses collapse",
             seen_lost_piles == 1 ? "a pulpy mess" : "pulpy messes");
    }

    if (seen_crawlies > 0)
    {
        mprf("%s %s to drag %s along the ground!",
             _count_article(seen_crawlies, seen_lost + seen_masses == 0),
             seen_crawlies == 1 ? "corpse begins" : "corpses begin",
             seen_crawlies == 1 ? "itself" : "themselves");
    }

    if (seen_masses > 0)
    {
        mprf("%s corpses meld into %s of writhing flesh!",
             _count_article(2, seen_crawlies + seen_lost == 0),
             seen_masses == 1 ? "an agglomeration" : "agglomerations");
    }

    if (num_orcs > 0 && caster->is_player())
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2 * num_orcs);
    if (num_holy > 0 && caster->is_player())
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2 * num_holy);

    return true;
}

spret_type cast_twisted_resurrection(int pow, god_type god, bool fail)
{
    if (twisted_resurrection(&you, pow, BEH_FRIENDLY, MHITYOU, god, !fail))
        return fail ? SPRET_FAIL : SPRET_SUCCESS;
    else
    {
        mpr("There are no corpses nearby!");
        return SPRET_ABORT;
    }
}

spret_type cast_haunt(int pow, const coord_def& where, god_type god, bool fail)
{
    monster* m = monster_at(where);

    if (m == NULL)
    {
        fail_check();
        mpr("An evil force gathers, but it quickly dissipates.");
        return SPRET_SUCCESS; // still losing a turn
    }
    else if (m->wont_attack())
    {
        mpr("You cannot haunt those who bear you no hostility.");
        return SPRET_ABORT;
    }

    int mi = m->mindex();
    ASSERT(!invalid_monster_index(mi));

    if (stop_attack_prompt(m, false, you.pos()))
        return SPRET_ABORT;

    fail_check();

    bool friendly = true;
    int success = 0;
    int to_summon = stepdown_value(2 + (random2(pow) / 10) + (random2(pow) / 10),
                                   2, 2, 6, -1);

    while (to_summon--)
    {
        const monster_type mon =
            random_choose_weighted(1, MONS_PHANTOM,
                                   1, MONS_HUNGRY_GHOST,
                                   1, MONS_SHADOW_WRAITH,
                                   5, MONS_WRAITH,
                                   2, MONS_FREEZING_WRAITH,
                                   2, MONS_PHANTASMAL_WARRIOR,
                                   0);

        if (monster *mons = create_monster(
                mgen_data(mon,
                          BEH_FRIENDLY, &you,
                          3, SPELL_HAUNT,
                          where, mi, MG_FORCE_BEH, god)))
        {
            success++;

            if (player_angers_monster(mons))
                friendly = false;
            else
            {
                mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, m, INFINITE_DURATION));
                mons->foe = mi;
            }
        }
    }

    if (success > 1)
    {
        mpr(friendly ? "Insubstantial figures form in the air."
                     : "You sense hostile presences.");
    }
    else if (success)
    {
        mpr(friendly ? "An insubstantial figure forms in the air."
                     : "You sense a hostile presence.");
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    //jmf: Kiku sometimes deflects this
    if (!in_good_standing(GOD_KIKUBAAQUDGHA, 3)
        || !x_chance_in_y(you.piety, MAX_PIETY))
    {
        you.sicken(25 + random2(50));
    }

    return SPRET_SUCCESS;
}

void init_servitor(monster* servitor, actor* caster)
{
    ASSERT(servitor->ghost.get());
    servitor->ghost->init_spellforged_servitor(caster);
    servitor->ghost_demon_init();
    servitor->props["custom_spells"].get_bool() = true;

    if (you.can_see(caster))
    {
        mprf("%s %s a servant imbued with %s destructive magic!",
             caster->name(DESC_THE).c_str(),
             caster->conj_verb("summon").c_str(),
             caster->pronoun(PRONOUN_POSSESSIVE).c_str());
    }
    else if (servitor)
        simple_monster_message(servitor, " appears!");

    int shortest_range = LOS_RADIUS + 1;
    for (size_t i = 0; i < servitor->spells.size(); ++i)
    {
        if (servitor->spells[i].spell == SPELL_NO_SPELL)
            continue;

        int range = spell_range(servitor->spells[i].spell, 100, false);
        if (range < shortest_range)
            shortest_range = range;
    }
    servitor->props["ideal_range"].get_int() = shortest_range;
}

spret_type cast_spellforged_servitor(int pow, god_type god, bool fail)
{
    fail_check();

    mgen_data mdata(MONS_SPELLFORGED_SERVITOR, BEH_FRIENDLY, &you,
                    4, SPELL_SPELLFORGED_SERVITOR, you.pos(), MHITYOU,
                    MG_AUTOFOE, god);

    if (monster* mon = create_monster(mdata))
        init_servitor(mon, &you);
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

static int _abjuration(int pow, monster *mon)
{
    // Scale power into something comparable to summon lifetime.
    const int abjdur = pow * 12;

    // XXX: make this a prompt
    if (mon->wont_attack())
        return false;

    int duration;
    if (mon->is_summoned(&duration))
    {
        int sockage = max(fuzz_value(abjdur, 60, 30), 40);
        dprf("%s abj: dur: %d, abj: %d",
             mon->name(DESC_PLAIN).c_str(), duration, sockage);

        bool shielded = false;
        // TSO and Trog's abjuration protection.
        if (mons_is_god_gift(mon, GOD_SHINING_ONE))
        {
            sockage = sockage * (30 - mon->get_hit_dice()) / 45;
            if (sockage < duration)
            {
                simple_god_message(" protects a fellow warrior from your evil magic!",
                                   GOD_SHINING_ONE);
                shielded = true;
            }
        }
        else if (mons_is_god_gift(mon, GOD_TROG))
        {
            sockage = sockage * 8 / 15;
            if (sockage < duration)
            {
                simple_god_message(" shields an ally from your puny magic!",
                                   GOD_TROG);
                shielded = true;
            }
        }

        mon_enchant abj = mon->get_ench(ENCH_ABJ);
        if (!mon->lose_ench_duration(abj, sockage) && !shielded)
            simple_monster_message(mon, " shudders.");
    }

    return true;
}

spret_type cast_abjuration(int pow, const coord_def& where, bool fail)
{
    fail_check();

    monster* mon = monster_at(where);

    if (mon)
    {
        mpr("Send 'em back where they came from!");
        _abjuration(pow, mon);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_aura_of_abjuration(int pow, bool fail)
{
    fail_check();

    if (!you.duration[DUR_ABJURATION_AURA])
        mpr("You begin to abjure the creatures around you!");
    else
        mpr("You extend your aura of abjuration.");

    you.increase_duration(DUR_ABJURATION_AURA,  6 + roll_dice(2, pow / 12), 50);
    you.props["abj_aura_pow"].get_int() = pow;

    return SPRET_SUCCESS;
}

void do_aura_of_abjuration(int delay)
{
    const int pow = you.props["abj_aura_pow"].get_int() * delay / 10;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        _abjuration(pow / 2, *mi);
}

spret_type cast_forceful_dismissal(int pow, bool fail)
{
    fail_check();
    vector<pair<coord_def, int> > explode;

    // Gather the list of summons to be dismissed
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->friendly() && mi->is_summoned() && mi->summoner == you.mid
            && you.see_cell_no_trans(mi->pos()))
        {
            explode.push_back(make_pair(mi->pos(), mi->get_hit_dice()));
        }
    }

    // Abort if there are no valid summons nearby
    if (!explode.size())
    {
        mpr("You have no nearby summons to unbind.");
        return SPRET_ABORT;
    }

    // Now prompt if we would harm something (other than those creatures
    // that are about to disappear anyway)
    vector<coord_def> affected;
    bool harm_player = false;
    for (unsigned int i = 0; i < explode.size(); ++i)
    {
        for (adjacent_iterator ai(explode[i].first, false); ai; ++ai)
        {
            monster* mon = monster_at(*ai);
            if (mon && mon->friendly()
                 && !(mon->is_summoned() && mon->summoner == you.mid))
            {
                affected.push_back(*ai);
            }

            if (*ai == you.pos())
                harm_player = true;
        }
    }
    targetter_list hitfunc(affected, you.pos());

    if (harm_player
        && !yesno("Really unbind your summons while standing next to them?",
                  false, 'n'))
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    if (stop_attack_prompt(hitfunc, "harm"))
        return SPRET_ABORT;

    // Dismiss the summons
    for (unsigned int i = 0; i < explode.size(); ++i)
    {
        monster_die(monster_at(explode[i].first), KILL_DISMISSED, NON_MONSTER,
                    true);
    }

    // Now do the explosions
    mpr("You violently release the magic binding your summons!");
    for (unsigned int i = 0; i < explode.size(); ++i)
    {
        bolt explosion;
        explosion.thrower = KILL_YOU;
        explosion.source = explode[i].first;
        explosion.target = explode[i].first;
        explosion.name = "magical backlash";
        explosion.is_explosion = true;
        explosion.auto_hit = true;
        explosion.damage = dice_def(1 + div_rand_round(explode[i].second, 4),
                                    7 + pow / 8);
        explosion.flavour = BEAM_MMISSILE;
        explosion.colour  = ETC_MAGIC;
        explosion.ex_size = 1;
        explosion.draw_delay = 1;
        explosion.explode_delay = 20;

        explosion.explode(true, false);
    }

    return SPRET_SUCCESS;
}

monster* find_battlesphere(const actor* agent)
{
    if (agent->props.exists("battlesphere"))
        return monster_by_mid(agent->props["battlesphere"].get_int());
    else
        return NULL;
}

spret_type cast_battlesphere(actor* agent, int pow, god_type god, bool fail)
{
    fail_check();

    monster* battlesphere;
    if (agent->is_player() && (battlesphere = find_battlesphere(&you)))
    {
        bool recalled = false;
        if (!you.can_see(battlesphere))
        {
            coord_def empty;
            if (find_habitable_spot_near(agent->pos(), MONS_BATTLESPHERE, 3, false, empty)
                && battlesphere->move_to_pos(empty))
            {
                recalled = true;
            }
        }

        if (recalled)
        {
            mpr("You recall your battlesphere and imbue it with additional"
                " charge.");
        }
        else
            mpr("You imbue your battlesphere with additional charge.");

        battlesphere->battlecharge = min(20, (int) battlesphere->battlecharge
                                              + 4 + random2(pow + 10) / 10);

        // Increase duration
        mon_enchant abj = battlesphere->get_ench(ENCH_FAKE_ABJURATION);
        abj.duration = min(abj.duration + (7 + roll_dice(2, pow)) * 10, 500);
        battlesphere->update_ench(abj);
    }
    else
    {
        ASSERT(!find_battlesphere(agent));
        mgen_data mg (MONS_BATTLESPHERE,
                      agent->is_player() ? BEH_FRIENDLY
                                         : SAME_ATTITUDE(agent->as_monster()),
                      agent,
                      0, SPELL_BATTLESPHERE,
                      agent->pos(),
                      agent->mindex(),
                      0, god,
                      MONS_NO_MONSTER, 0, BLACK);

        mg.hd = 1 + div_rand_round(pow, 11);
        battlesphere = create_monster(mg);

        if (battlesphere)
        {
            int dur = min((7 + roll_dice(2, pow)) * 10, 500);
            battlesphere->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 1, 0, dur));
            battlesphere->summoner = agent->mid;
            agent->props["battlesphere"].get_int() = battlesphere->mid;

            if (agent->is_player())
                mpr("You conjure a globe of magical energy.");
            else
            {
                if (you.can_see(agent) && you.can_see(battlesphere))
                {
                    simple_monster_message(agent->as_monster(),
                                           " conjures a globe of magical energy!");
                }
                else if (you.can_see(battlesphere))
                    simple_monster_message(battlesphere, " appears!");
                battlesphere->props["band_leader"].get_int() = agent->mid;
            }
            battlesphere->battlecharge = 4 + random2(pow + 10) / 10;
            battlesphere->foe = agent->mindex();
            battlesphere->target = agent->pos();
        }
        else if (agent->is_player() || you.can_see(agent))
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return SPRET_SUCCESS;
}

void end_battlesphere(monster* mons, bool killed)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor* agent = actor_by_mid(mons->summoner);
    if (agent)
        agent->props.erase("battlesphere");

    if (!killed)
    {
        if (agent && agent->is_player())
        {
            if (you.can_see(mons))
            {
                if (mons->battlecharge == 0)
                {
                    mpr("Your battlesphere expends the last of its energy"
                        " and dissipates.");
                }
                else
                    mpr("Your battlesphere wavers and loses cohesion.");
            }
            else
                mpr("You feel your bond with your battlesphere wane.");
        }
        else if (you.can_see(mons))
            simple_monster_message(mons, " dissipates.");

        if (!cell_is_solid(mons->pos()))
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);

        monster_die(mons, KILL_RESET, NON_MONSTER);
    }
}

bool battlesphere_can_mirror(spell_type spell)
{
    return (spell_typematch(spell, SPTYP_CONJURATION)
           && spell_to_zap(spell) != NUM_ZAPS)
           || spell == SPELL_FREEZE
           || spell == SPELL_STICKY_FLAME
           || spell == SPELL_SANDBLAST
           || spell == SPELL_AIRSTRIKE
           || spell == SPELL_DAZZLING_SPRAY
           || spell == SPELL_SEARING_RAY;
}

bool aim_battlesphere(actor* agent, spell_type spell, int powc, bolt& beam)
{
    //Is this spell something that will trigger the battlesphere?
    if (battlesphere_can_mirror(spell))
    {
        monster* battlesphere = find_battlesphere(agent);

        // If we've somehow gotten separated from the battlesphere (ie:
        // abyss level teleport), bail out and cancel the battlesphere bond
        if (!battlesphere)
        {
            agent->props.erase("battlesphere");
            return false;
        }

        // In case the battlesphere was in the middle of a (failed)
        // target-seeking action, cancel it so that it can focus on a new
        // target
        reset_battlesphere(battlesphere);

        // Don't try to fire at ourselves
        if (beam.target == battlesphere->pos())
            return false;

        // If the player beam is targeted at a creature, aim at this creature.
        // Otherwise, aim at the furthest creature in the player beam path
        bolt testbeam = beam;

        if (agent->is_player())
            testbeam.thrower = KILL_YOU_MISSILE;
        else
        {
            testbeam.thrower = KILL_MON_MISSILE;
            testbeam.source_id = agent->mid;
        }

        testbeam.is_tracer = true;
        zap_type ztype = spell_to_zap(spell);

        // Fallback for non-standard spell zaps
        if (ztype == NUM_ZAPS)
            ztype = ZAP_MAGIC_DART;

        // This is so that reflection and pathing rules for the parent beam
        // will be obeyed when figuring out what is being aimed at
        zappy(ztype, powc, testbeam);

        battlesphere->props["firing_target"] = beam.target;
        battlesphere->props.erase("foe");
        if (!actor_at(beam.target))
        {
            testbeam.fire();

            for (size_t i = 0; i < testbeam.path_taken.size(); i++)
            {
                const coord_def c = testbeam.path_taken[i];
                if (c != battlesphere->pos() && monster_at(c))
                {
                    battlesphere->props["firing_target"] = c;
                    battlesphere->foe = actor_at(c)->mindex();
                    battlesphere->props["foe"] = battlesphere->foe;
                    break;
                }
            }

            // If we're firing at empty air, lose any prior target lock
            if (!battlesphere->props.exists("foe"))
                battlesphere->foe = agent->mindex();
        }
        else
        {
            battlesphere->foe = actor_at(beam.target)->mindex();
            battlesphere->props["foe"] = battlesphere->foe;
        }

        battlesphere->props["ready"] = true;

        return true;
    }

    return false;
}

bool trigger_battlesphere(actor* agent, bolt& beam)
{
    monster* battlesphere = find_battlesphere(agent);
    if (!battlesphere)
        return false;

    if (battlesphere->props.exists("ready"))
    {
        // If the battlesphere is aiming at empty air but the triggering
        // conjuration is an explosion, try to find something to shoot within
        // the blast
        if (!battlesphere->props.exists("foe") && beam.is_explosion)
        {
            explosion_map exp_map;
            exp_map.init(INT_MAX);
            beam.determine_affected_cells(exp_map, coord_def(), 0,
                                          beam.ex_size, true, true);

            for (radius_iterator ri(beam.target, beam.ex_size, C_ROUND);
                 ri; ++ri)
            {
                if (exp_map(*ri - beam.target + coord_def(9,9)) < INT_MAX)
                {
                    const actor *targ = actor_at(*ri);
                    if (targ && targ != battlesphere)
                    {
                        battlesphere->props["firing_target"] = *ri;
                        battlesphere->foe = targ->mindex();
                        battlesphere->props["foe"] = battlesphere->foe;
                        continue;
                    }
                }
            }
        }

        battlesphere->props.erase("ready");
        battlesphere->props["firing"] = true;

        // Since monsters may be acting out of sequence, give the battlesphere
        // enough energy to attempt to fire this round, and requeue if it's
        // already passed its turn
        if (agent->is_monster())
        {
            battlesphere->speed_increment = 100;
            queue_monster_for_action(battlesphere);
        }

        return true;
    }

    return false;
}

// Called at the start of each round. Cancels firing orders given in the
// previous round, if the battlesphere was not able to execute them fully
// before the next player action
void reset_battlesphere(monster* mons)
{
    if (!mons || mons->type != MONS_BATTLESPHERE)
        return;

    mons->props.erase("ready");

    if (mons->props.exists("tracking"))
    {
        mons->props.erase("tracking");
        mons->props.erase("firing");
        if (mons->props.exists("foe"))
            mons->foe = mons->props["foe"].get_int();
        mons->behaviour = BEH_SEEK;
    }
}

bool fire_battlesphere(monster* mons)
{
    if (!mons || mons->type != MONS_BATTLESPHERE)
        return false;

    actor* agent = actor_by_mid(mons->summoner);

    if (!agent || !agent->alive())
    {
        end_battlesphere(mons, false);
        return false;
    }

    bool used = false;

    if (mons->props.exists("firing") && mons->battlecharge > 0)
    {
        if (mons->props.exists("tracking"))
        {
            if (mons->pos() == mons->props["tracking_target"].get_coord())
            {
                mons->props.erase("tracking");
                if (mons->props.exists("foe"))
                    mons->foe = mons->props["foe"].get_int();
                mons->behaviour = BEH_SEEK;
            }
            else // Currently tracking, but have not reached target pos
            {
                mons->target = mons->props["tracking_target"].get_coord();
                return false;
            }
        }
        else
        {
            // If the battlesphere forgot its foe (due to being out of los),
            // remind it
            if (mons->props.exists("foe"))
                mons->foe = mons->props["foe"].get_int();
        }

        // Set up the beam.
        bolt beam;
        beam.source_name = "battlesphere";

        // If we are locked onto a foe, use its current position
        if (!invalid_monster_index(mons->foe) && menv[mons->foe].alive())
            beam.target = menv[mons->foe].pos();
        else
            beam.target = mons->props["firing_target"].get_coord();

        // Sanity check: if we have somehow ended up targetting ourselves, bail
        if (beam.target == mons->pos())
        {
            mprf(MSGCH_ERROR, "Battlesphere targeting itself? Fixing.");
            mons->props.erase("firing");
            mons->props.erase("firing_target");
            mons->props.erase("foe");
            return false;
        }

        beam.name       = "barrage of energy";
        beam.range      = LOS_RADIUS;
        beam.hit        = AUTOMATIC_HIT;
        beam.damage     = dice_def(2, 5 + mons->get_hit_dice());
        beam.glyph      = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.colour     = MAGENTA;
        beam.flavour    = BEAM_MMISSILE;
        beam.is_beam    = false;

        // Fire tracer.
        fire_tracer(mons, beam);

        // Never fire if we would hurt the caster, and ensure that the beam
        // would hit at least SOMETHING, unless it was targeted at empty space
        // in the first place
        if (beam.friend_info.count == 0
            && (monster_at(beam.target) ? beam.foe_info.count > 0 :
                find(beam.path_taken.begin(), beam.path_taken.end(),
                     beam.target)
                    != beam.path_taken.end()))
        {
            beam.thrower = (agent->is_player()) ? KILL_YOU : KILL_MON;
            simple_monster_message(mons, " fires!");
            beam.fire();

            used = true;
            // Decrement # of volleys left and possibly expire the battlesphere.
            if (--mons->battlecharge == 0)
                end_battlesphere(mons, false);

            mons->props.erase("firing");
        }
        // If we are firing at something, try to find a nearby position
        // from which we could safely fire at it
        else
        {
            const bool empty_beam = (beam.foe_info.count == 0);
            for (distance_iterator di(mons->pos(), true, true, 2); di; ++di)
            {
                if (*di == beam.target || actor_at(*di)
                    || cell_is_solid(*di)
                    || !agent->see_cell(*di))
                {
                    continue;
                }

                beam.source = *di;
                beam.is_tracer = true;
                beam.friend_info.reset();
                beam.foe_info.reset();
                beam.fire();
                if (beam.friend_info.count == 0
                    && (beam.foe_info.count > 0 || empty_beam))
                {
                    if (empty_beam
                        && find(beam.path_taken.begin(), beam.path_taken.end(),
                                beam.target) == beam.path_taken.end())
                    {
                        continue;
                    }

                    mons->firing_pos = coord_def(0, 0);
                    mons->target = *di;
                    mons->behaviour = BEH_WANDER;
                    mons->props["foe"] = mons->foe;
                    mons->props["tracking"] = true;
                    mons->foe = MHITNOT;
                    mons->props["tracking_target"] = *di;
                    break;
                }
            }

            // If we didn't find a better firing position nearby, cancel firing
            if (!mons->props.exists("tracking"))
                mons->props.erase("firing");
        }
    }

    // If our last target is dead, or the player wandered off, resume
    // following the player
    if ((mons->foe == MHITNOT || !mons->can_see(agent)
         || (!invalid_monster_index(mons->foe)
             && !agent->can_see(&menv[mons->foe])))
        && !mons->props.exists("tracking"))
    {
        mons->foe = agent->mindex();
    }

    return used;
}

spret_type cast_fulminating_prism(int pow, const coord_def& where, bool fail)
{
    if (distance2(where, you.pos()) > dist_range(spell_range(SPELL_FULMINANT_PRISM,
                                                 pow)))
    {
        mpr("That's too far away.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(where))
    {
        mpr("You can't conjure that within a solid object!");
        return SPRET_ABORT;
    }

    // Note that self-targeting is handled by SPFLAG_NOT_SELF.
    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(mons))
        {
            mpr("You can't place the prism on a creature.");
            return SPRET_ABORT;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        mpr("You see a ghostly outline there, and the spell fizzles.");
        return SPRET_SUCCESS;      // Don't give free detection!
    }

    fail_check();

    int hd = div_rand_round(pow, 10);

    mgen_data prism_data = mgen_data(MONS_FULMINANT_PRISM, BEH_FRIENDLY, &you,
                                     3, SPELL_FULMINANT_PRISM,
                                     where, MHITYOU, MG_FORCE_PLACE);
    prism_data.hd = hd;
    monster *prism = create_monster(prism_data);

    if (prism)
        mpr("You conjure a prism of explosive energy!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

monster* find_spectral_weapon(const actor* agent)
{
    if (agent->props.exists("spectral_weapon"))
        return monster_by_mid(agent->props["spectral_weapon"].get_int());
    else
        return NULL;
}

spret_type cast_spectral_weapon(actor *agent, int pow, god_type god, bool fail)
{
    ASSERT(agent);

    const int dur = min(2 + random2(1 + div_rand_round(pow, 25)), 4);
    item_def* wpn = agent->weapon();

    // If the wielded weapon should not be cloned, abort
    if (!wpn || !is_weapon(*wpn) || is_range_weapon(*wpn)
        || is_special_unrandom_artefact(*wpn))
    {
        if (agent->is_player())
        {
            if (wpn)
            {
                mprf("%s vibrate%s crazily for a second.",
                     wpn->name(DESC_YOUR).c_str(),
                     wpn->quantity > 1 ? "" : "s");
            }
            else
                mprf("Your %s twitch.", you.hand_name(true).c_str());
        }

        return SPRET_ABORT;
    }

    fail_check();

    item_def cp = *wpn;

    // Remove any existing spectral weapons. Only one should be alive at any
    // given time.
    monster *old_mons = find_spectral_weapon(agent);
    if (old_mons)
        end_spectral_weapon(old_mons, false);

    mgen_data mg(MONS_SPECTRAL_WEAPON,
            agent->is_player() ? BEH_FRIENDLY
                               : SAME_ATTITUDE(agent->as_monster()),
            agent,
            dur, SPELL_SPECTRAL_WEAPON,
            agent->pos(),
            agent->mindex(),
            0, god);

    int skill_with_weapon = agent->skill(item_attack_skill(*wpn), 10, false);

    mg.props[TUKIMA_WEAPON] = cp;
    mg.props[TUKIMA_POWER] = pow;
    mg.props[TUKIMA_SKILL] = skill_with_weapon;

    monster *mons = create_monster(mg);
    if (!mons)
    {
        //if (agent->is_player())
            canned_msg(MSG_NOTHING_HAPPENS);

        return SPRET_SUCCESS;
    }

    if (agent->is_player())
        mpr("You draw out your weapon's spirit!");
    else
    {
        if (you.can_see(agent) && you.can_see(mons))
        {
            string buf = " draws out ";
            buf += agent->pronoun(PRONOUN_POSSESSIVE);
            buf += " weapon's spirit!";
            simple_monster_message(agent->as_monster(), buf.c_str());
        }
        else if (you.can_see(mons))
            simple_monster_message(mons, " appears!");

        mons->props["band_leader"].get_int() = agent->mid;
        mons->foe = agent->mindex();
        mons->target = agent->pos();
    }

    mons->summoner = agent->mid;
    agent->props["spectral_weapon"].get_int() = mons->mid;

    return SPRET_SUCCESS;
}

void end_spectral_weapon(monster* mons, bool killed, bool quiet)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor *owner = actor_by_mid(mons->summoner);

    if (owner)
        owner->props.erase("spectral_weapon");

    if (!quiet)
    {
        if (you.can_see(mons))
        {
            simple_monster_message(mons, " fades away.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        }
        else if (owner && owner->is_player())
            mpr("You feel your bond with your spectral weapon wane.");
    }

    if (!killed)
        monster_die(mons, KILL_RESET, NON_MONSTER);
}

bool trigger_spectral_weapon(actor* agent, const actor* target)
{
    monster *spectral_weapon = find_spectral_weapon(agent);

    // Don't try to attack with a nonexistant spectral weapon
    if (!spectral_weapon || !spectral_weapon->alive())
    {
        agent->props.erase("spectral_weapon");
        return false;
    }

    // Likewise if the target is the spectral weapon itself
    if (target->as_monster() == spectral_weapon)
        return false;

    // Clear out any old orders.
    reset_spectral_weapon(spectral_weapon);

    spectral_weapon->props[SW_TARGET_MID].get_int() = target->mid;
    spectral_weapon->props[SW_READIED] = true;

    return true;
}

// Called at the start of each round. Cancels attack order given in the
// previous round, if the weapon was not able to execute them fully
// before the next player action
void reset_spectral_weapon(monster* mons)
{
    if (!mons || mons->type != MONS_SPECTRAL_WEAPON)
        return;

    if (mons->props.exists(SW_TRACKING))
    {
        mons->props.erase(SW_TRACKING);
        mons->props.erase(SW_READIED);
        mons->props.erase(SW_TARGET_MID);

        return;
    }

    // If an attack has been readied, begin tracking.
    if (mons->props.exists(SW_READIED))
        mons->props[SW_TRACKING] = true;
    else
        mons->props.erase(SW_TARGET_MID);
}

/* Confirms the spectral weapon can and will attack the given defender.
 *
 * Checks the target, and that we haven't attacked yet.
 * Then consumes our ready state, preventing further attacks.
 */
bool confirm_attack_spectral_weapon(monster* mons, const actor *defender)
{
    // No longer tracking towards the target.
    mons->props.erase(SW_TRACKING);

    // Is the defender our target?
    if (mons->props.exists(SW_TARGET_MID)
        && (mid_t)mons->props[SW_TARGET_MID].get_int() == defender->mid
        && mons->props.exists(SW_READIED))
    {
        // Consume our ready state and attack
        mons->props.erase(SW_READIED);
        return true;
    }

    // Expend the weapon's energy, as it can't attack
    int energy = mons->action_energy(EUT_ATTACK);
    ASSERT(energy > 0);

    mons->speed_increment -= energy;

    return false;
}

void grand_avatar_reset(monster* mons)
{
    ASSERT(mons);
    ASSERT(mons->type == MONS_GRAND_AVATAR);
    actor* agent = actor_by_mid(mons->summoner);
    if (!agent || !agent->alive())
    {
        monster_die(mons, KILL_TIMEOUT, NON_MONSTER);
        return;
    }
    mons->props.erase(GA_TARGET_MID);
    mons->props.erase(GA_MELEE);
    mons->props.erase(GA_SPELL);
    actor* foe = mons->get_foe();
    if (foe)
        mons->target = foe->pos();
    else
    {
        mons->foe = agent->mindex();
        mons->target = agent->pos();
    }
}

bool grand_avatar_check_melee(monster* mons, actor* target)
{
    if (mons->props.exists(GA_TARGET_MID)
        && mons->props.exists(GA_MELEE))
    {
        actor* desired_target =
            actor_by_mid(mons->props[GA_TARGET_MID].get_int());
        if (target == desired_target)
        {
            grand_avatar_reset(mons);
            return true;
        }
    }
    mons->lose_energy(EUT_ATTACK);
    return false;
}

void trigger_grand_avatar(monster* mons, actor* victim, spell_type spell,
                          const int old_hp)
{
    const bool melee = (spell == SPELL_MELEE);
    ASSERT(mons->has_ench(ENCH_GRAND_AVATAR));

    actor* avatar = mons->get_ench(ENCH_GRAND_AVATAR).agent();
    if (!avatar)
    {
        mons->del_ench(ENCH_GRAND_AVATAR);
        return;
    }

    ASSERT(avatar->is_monster());
    monster* av = avatar->as_monster();
    monster* owner = monster_by_mid(av->summoner);
    if (!owner || !mons_aligned(mons, owner))
    {
        mons->del_ench(ENCH_GRAND_AVATAR);
        return;
    }

    if (!victim
        || !victim->alive()
        || (!melee && !battlesphere_can_mirror(spell))
        || old_hp - victim->stat_hp() < random2(GRAND_AVATAR_DAMAGE))
    {
        return;
    }

    av->props[GA_TARGET_MID].get_int() = victim->mid;
    if (melee)
    {
        av->props[GA_MELEE].get_bool() = true;
        av->props.erase(GA_SPELL);
    }
    else
    {
        av->props[GA_SPELL].get_bool() = true;
        av->props.erase(GA_MELEE);
    }
}

void end_grand_avatar(monster* mons, bool killed)
{
    if (!mons)
        return;

    actor* agent = actor_by_mid(mons->summoner);
    if (agent)
    {
        monster* owner = agent->as_monster();
        owner->del_ench(ENCH_GRAND_AVATAR);
    }

    if (!killed)
    {
        if (!cell_is_solid(mons->pos()))
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);
        monster_die(mons, KILL_RESET, NON_MONSTER);
    }
}

spell_type summons_index::map(const summons_desc* val)
{
    return val->which;
}

// spell type, cap, timeout
static const summons_desc summonsdata[] =
{
    // Beasts
    { SPELL_SUMMON_BUTTERFLIES,         8, 5 },
    { SPELL_SUMMON_SMALL_MAMMAL,        4, 2 },
    { SPELL_CALL_CANINE_FAMILIAR,       1, 2 },
    { SPELL_SUMMON_ICE_BEAST,           3, 3 },
    { SPELL_SUMMON_HYDRA,               3, 2 },
    { SPELL_SUMMON_MANA_VIPER,          2, 2 },
    // Demons
    { SPELL_CALL_IMP,                   3, 3 },
    { SPELL_SUMMON_DEMON,               3, 2 },
    { SPELL_SUMMON_GREATER_DEMON,       3, 2 },
    // General monsters
    { SPELL_MONSTROUS_MENAGERIE,        4, 2 },
    { SPELL_SUMMON_HORRIBLE_THINGS,     8, 8 },
    { SPELL_SHADOW_CREATURES,           4, 2 },
    { SPELL_SUMMON_LIGHTNING_SPIRE,     1, 2 },
    { SPELL_SUMMON_GUARDIAN_GOLEM,      1, 2 },
    { SPELL_SPELLFORGED_SERVITOR,       1, 2 },
    // Monster spells
    { SPELL_SUMMON_UFETUBUS,            8, 2 },
    { SPELL_SUMMON_HELL_BEAST,          8, 2 },
    { SPELL_SUMMON_UNDEAD,              8, 2 },
    { SPELL_SUMMON_DRAKES,              4, 2 },
    { SPELL_SUMMON_MUSHROOMS,           8, 2 },
    { SPELL_SUMMON_EYEBALLS,            4, 2 },
    { SPELL_WATER_ELEMENTALS,           3, 2 },
    { SPELL_FIRE_ELEMENTALS,            3, 2 },
    { SPELL_EARTH_ELEMENTALS,           3, 2 },
    { SPELL_AIR_ELEMENTALS,             3, 2 },
    { SPELL_IRON_ELEMENTALS,            3, 2 },
    { SPELL_SUMMON_SPECTRAL_ORCS,       3, 2 },
    { SPELL_FIRE_SUMMON,                4, 2 },
    { SPELL_SUMMON_MINOR_DEMON,         3, 3 },
    { SPELL_CALL_LOST_SOUL,             3, 2 },
    { SPELL_SUMMON_VERMIN,              5, 2 },
    { SPELL_FORCEFUL_INVITATION,        3, 1 },
    { SPELL_PLANEREND,                  6, 1 },
    { SPELL_SUMMON_DRAGON,              4, 8 },
    { SPELL_PHANTOM_MIRROR,             4, 1 },
    { SPELL_FAKE_MARA_SUMMON,           2, 1 },
    { SPELL_SUMMON_EMPEROR_SCORPIONS,   6, 2 },
    { SPELL_SUMMON_SCARABS,             8, 1 },
    // Rod specials
    { SPELL_SUMMON_SWARM,              12, 2 },
    { SPELL_WEAVE_SHADOWS,              4, 2 },
    { SPELL_NO_SPELL,                   0, 0 }
};

static summons_index summonsindex(summonsdata);

bool summons_are_capped(spell_type spell)
{
    ASSERT_RANGE(spell, 0, NUM_SPELLS);
    return summonsindex.contains(spell);
}

int summons_limit(spell_type spell)
{
    if (!summons_are_capped(spell))
        return 0;
    const summons_desc *desc = summonsindex[spell];
    return desc->type_cap;
}

static bool _spell_has_variable_cap(spell_type spell)
{
    return spell == SPELL_SHADOW_CREATURES
           || spell == SPELL_WEAVE_SHADOWS
           || spell == SPELL_MONSTROUS_MENAGERIE;
}

static void _expire_capped_summon(monster* mon, int delay, bool recurse)
{
    // Timeout the summon
    mon_enchant abj = mon->get_ench(ENCH_ABJ);
    abj.duration = delay;
    mon->update_ench(abj);
    // Mark our cap abjuration so we don't keep abjuring the same
    // one if creating multiple summons (also, should show a status light).
    mon->add_ench(ENCH_SUMMON_CAPPED);

    if (recurse && mon->props.exists("summon_id"))
    {
        const int summon_id = mon->props["summon_id"].get_int();
        for (monster_iterator mi; mi; ++mi)
        {
            // Summoner check should be technically unnecessary, but saves
            // scanning props for all monsters on the level.
            if (mi->summoner == mon->summoner
                && mi->props.exists("summon_id")
                && mi->props["summon_id"].get_int() == summon_id
                && !mi->has_ench(ENCH_SUMMON_CAPPED))
            {
                _expire_capped_summon(*mi, delay, false);
            }
        }
    }
}

// Call when a monster has been summoned, to manage this summoner's caps.
void summoned_monster(const monster *mons, const actor *caster,
                      spell_type spell)
{
    if (!summons_are_capped(spell))
        return;

    const summons_desc *desc = summonsindex[spell];

    int max_this_time = desc->type_cap;

    // Cap large abominations and tentacled monstrosities separately
    if (spell == SPELL_SUMMON_HORRIBLE_THINGS)
    {
        max_this_time = (mons->type == MONS_ABOMINATION_LARGE ? max_this_time * 3 / 4
                                                              : max_this_time * 1 / 4);
    }

    monster* oldest_summon = 0;
    int oldest_duration = 0;

    // Linked summons that have already been counted once
    set<int> seen_ids;

    int count = 1;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons == *mi)
            continue;

        int duration = 0;
        int stype    = 0;
        const bool summoned = mi->is_summoned(&duration, &stype);
        if (summoned && stype == spell && caster->mid == mi->summoner
            && mons_aligned(caster, *mi))
        {
            // Count large abominations and tentacled monstrosities separately
            if (spell == SPELL_SUMMON_HORRIBLE_THINGS && mi->type != mons->type)
                continue;

            if (_spell_has_variable_cap(spell) && mi->props.exists("summon_id"))
            {
                const int id = mi->props["summon_id"].get_int();

                // Skip any linked summon whose set we have seen already,
                // otherwise add it to the list of seen summon IDs
                if (seen_ids.find(id) == seen_ids.end())
                    seen_ids.insert(id);
                else
                    continue;
            }

            count++;

            // If this summon is the oldest (well, the closest to expiry)
            // remember it (unless already expiring due to a cap)
            if (!mi->has_ench(ENCH_SUMMON_CAPPED)
                && (!oldest_summon || duration < oldest_duration))
            {
                oldest_summon = *mi;
                oldest_duration = duration;
            }
        }
    }

    if (oldest_summon && count > max_this_time)
        _expire_capped_summon(oldest_summon, desc->timeout * 5, true);
}

int count_summons(const actor *summoner, spell_type spell)
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (summoner == *mi)
            continue;

        int stype    = 0;
        const bool summoned = mi->is_summoned(NULL, &stype);
        if (summoned && stype == spell && summoner->mid == mi->summoner
            && mons_aligned(summoner, *mi))
        {
            count++;
        }
    }

    return count;
}

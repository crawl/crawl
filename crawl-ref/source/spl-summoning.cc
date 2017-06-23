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
#include "god-conduct.h"
#include "god-item.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "melee-attack.h"
#include "message.h"
#include "mgen-data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h" // MON_SPELL_WIZARD
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-movetarget.h"
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
#include "timed-effects.h"
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

static mgen_data _summon_data(const actor &caster, monster_type mtyp,
                              int dur, god_type god, spell_type spell)
{
    return mgen_data(mtyp, BEH_COPY, caster.pos(),
                     caster.is_player() ? MHITYOU : caster.as_monster()->foe,
                     MG_AUTOFOE)
                     .set_summoned(&caster, dur, spell, god);
}

static mgen_data _pal_data(monster_type pal, int dur, god_type god,
                           spell_type spell)
{
    return _summon_data(you, pal, dur, god, spell);
}

spret_type cast_summon_butterflies(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = min(8, 3 + random2(3) + random2(pow) / 10);

    for (int i = 0; i < how_many; ++i)
    {
        if (create_monster(_pal_data(MONS_BUTTERFLY, 3, god,
                                     SPELL_SUMMON_BUTTERFLIES)))
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
        mon = random_choose(MONS_BAT, MONS_RAT);
    else
        mon = MONS_QUOKKA;

    if (!create_monster(_pal_data(mon, 3, god, SPELL_SUMMON_SMALL_MAMMAL)))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_sticks_to_snakes(int pow, god_type god, bool fail)
{
    // The first items placed into this list will be the first
    // to be converted; for players with bow skill we prefer
    // plain arrows.
    // For players without bow skill, we prefer arrows with the
    // smallest quantity, in order to free up inventory sooner
    list<item_def*> valid_sticks;
    int num_sticks = 0;
    for (item_def& i : you.inv)
        if (i.is_type(OBJ_MISSILES, MI_ARROW)
            && check_warning_inscriptions(i, OPER_DESTROY))
        {
            // If the player has bow skill, assume that they
            // would prefer that their regular ammo would be
            // used first
            if (get_ammo_brand(i) == SPMSL_NORMAL)
                valid_sticks.push_front(&i);
            else
                valid_sticks.push_back(&i);
            num_sticks += i.quantity;
        }

    if (valid_sticks.empty())
    {
		mpr("뱀으로 바꿀만한 물건이 없다.");
        return SPRET_ABORT;
    }
    // Sort by the quantity if the player has no bow skill; this will
    // put arrows with the smallest quantity first in line
    // If the player has bow skill, we will already have plain arrows
    // in the first element, so skip this
    if (you.skills[SK_BOWS] < 1)
    {
        valid_sticks.sort([](const item_def* a, const item_def* b) -> bool
                             {
                                 return a->quantity < b->quantity;
                             }
                         );
    }
    const int dur = min(3 + random2(pow) / 20, 5);
    int how_many_max = 1 + min(6, random2(pow) / 15);

    int count = 0;

    fail_check();
    if (num_sticks < how_many_max)
        how_many_max = num_sticks;
    item_def *stick = nullptr;
    for (int i = 0; i < how_many_max; i++)
    {
        monster_type mon;
        if (!stick || stick->quantity == 0)
        {
            stick = valid_sticks.front();
            valid_sticks.pop_front();
        }
        if (one_chance_in(5 - min(4, div_rand_round(pow * 2, 25))))
        {
            mon = x_chance_in_y(pow / 3, 100) ? MONS_WATER_MOCCASIN
                                              : MONS_ADDER;
        }
        else
            mon = MONS_BALL_PYTHON;
        if (monster *snake = create_monster(_pal_data(mon, 0, god,
                                                      SPELL_STICKS_TO_SNAKES),
                                            false))
        {
            count++;
            dec_inv_item_quantity(letter_to_index(stick->slot), 1);
            snake->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, dur));
        }
    }
    if (count)
    {
        int sticks_left = num_sticks - count;

        if (count > 1)
            mprf("You create %d snakes!", count);
        else
			mpr("뱀을 만들었다!");

        if (sticks_left)
        {
            mprf("You now have %d arrow%s.", sticks_left,
                                             sticks_left > 1 ? "s" : "");
        }
        else
			mpr("남아있는 화살이 없다.");
    }
    else
		mpr("당신은 뱀 만들기에 실패했다.");

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

    if (!create_monster(_pal_data(mon, dur, god, SPELL_CALL_CANINE_FAMILIAR)))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_ice_beast(int pow, god_type god, bool fail)
{
    fail_check();
    const int dur = min(2 + (random2(pow) / 4), 4);

    mgen_data ice_beast = _pal_data(MONS_ICE_BEAST, dur, god,
                                    SPELL_SUMMON_ICE_BEAST);
    ice_beast.hd = (3 + div_rand_round(pow, 13));

    if (create_monster(ice_beast))
		mpr("당신 주위로 한기를 품은 바람이 몰아쳤다.");
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
        type = random_choose(MONS_HARPY, MONS_MANTICORE, MONS_LINDWURM);

    if (player_will_anger_monster(type))
        type = MONS_MANTICORE;

    int num = (type == MONS_HARPY ? 1 + x_chance_in_y(pow, 80)
                                      + x_chance_in_y(pow - 75, 100)
                                  : 1);
    const bool plural = (num > 1);

    mgen_data mdata = _summon_data(*caster, type, 4, god,
                                   SPELL_MONSTROUS_MENAGERIE);
    mdata.flags |= MG_DONT_CAP;
    if (caster->is_player())
        mdata.hd = get_monster_data(type)->HD + div_rand_round(pow - 50, 25);

    bool seen = false;
    bool first = true;
    int mid = -1;
    while (num-- > 0)
    {
        if (monster* beast = create_monster(mdata))
        {
            if (you.can_see(*beast))
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
                             plural ? pluralise_monster(mons_type_name(type, DESC_PLAIN)).c_str()
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
    const int maxheads = one_chance_in(6) ? 12 : 8;
    const int heads = max(4, min(random2(pow) / 6, maxheads));

    // Duration is always very short - just 1.
    mgen_data mg = _summon_data(*caster, MONS_HYDRA, 1, god,
                                SPELL_SUMMON_HYDRA);
    mg.props[MGEN_NUM_HEADS] = heads;
    if (monster *hydra = create_monster(mg))
    {
        if (you.see_cell(hydra->pos()))
            mprf("%s appears.", hydra->name(DESC_A).c_str());
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
        mon = random_choose(MONS_GOLDEN_DRAGON, MONS_QUICKSILVER_DRAGON);
    else if (chance >= 40 || one_chance_in(6))
        mon = random_choose(MONS_IRON_DRAGON, MONS_SHADOW_DRAGON, MONS_STORM_DRAGON);
    else
        mon = random_choose(MONS_FIRE_DRAGON, MONS_ICE_DRAGON);

    // For good gods, switch away from shadow dragons to storm/iron dragons.
    if (player && player_will_anger_monster(mon))
        mon = random_choose(MONS_STORM_DRAGON, MONS_IRON_DRAGON);

    return mon;
}

spret_type cast_dragon_call(int pow, bool fail)
{
    if (you.duration[DUR_DRAGON_CALL]
        || you.duration[DUR_DRAGON_CALL_COOLDOWN])
    {
		mpr("그렇게 빨리 다시 용을 부를 수는 없다.");
        return SPRET_ABORT;
    }

    fail_check();

	mpr("당신은 용의 왕국에 소환 요청을 했고, 용 무리가 울부짖으며 답했다!");
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    you.duration[DUR_DRAGON_CALL] = (15 + pow / 5 + random2(15)) * BASELINE_DELAY;

    return SPRET_SUCCESS;
}

static void _place_dragon()
{

    const int pow = calc_spell_power(SPELL_DRAGON_CALL, true);
    monster_type mon = _choose_dragon_type(pow, you.religion, true);
    int mp_cost = random_range(2, 3);

    vector<monster*> targets;

    // Pick a random hostile in sight
    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
        if (!mons_aligned(&you, *mi) && mons_is_threatening(**mi))
            targets.push_back(*mi);

    shuffle_array(targets);

    // Attempt to place adjacent to the first chosen hostile. If there is no
    // valid spot, move on to the next one.
    for (monster *target : targets)
    {
        // Chose a random viable adjacent spot to the select target
        vector<coord_def> spots;
        for (adjacent_iterator ai(target->pos()); ai; ++ai)
        {
            if (monster_habitable_grid(MONS_FIRE_DRAGON, grd(*ai))
                && !actor_at(*ai))
            {
                spots.push_back(*ai);
            }
        }

        // Now try to create the actual dragon
        if (spots.size() <= 0)
            continue;

        // Abort if we lack sufficient MP, but the dragon call duration
        // remains, as the player might soon have enough again.
        if (!enough_mp(mp_cost, true))
        {
			mpr("용이 당신의 부름에 답하려 했지만, 당신의 마력이 부족하다!");
            return;
        }

        const coord_def pos = spots[random2(spots.size())];
        monster *dragon = create_monster(
            mgen_data(mon, BEH_COPY, pos, MHITYOU, MG_FORCE_PLACE | MG_AUTOFOE)
            .set_summoned(&you, 2, SPELL_DRAGON_CALL));
        if (!dragon)
            continue;

        dec_mp(mp_cost);
        if (you.see_cell(dragon->pos()))
			mpr("용이 당신의 부름에 응답했다!");

        // The dragon is allowed to act immediately here
        dragon->flags &= ~MF_JUST_SUMMONED;
        return;
    }

    return;
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

/**
 * Handle the Doom Howl status effect, possibly summoning hostile nasties
 * around the player.
 *
 * @param time      The number of aut that the howling has been going on for
 *                  since the last doom_howl call.
 */
void doom_howl(int time)
{
    // TODO: pull hound-count generation into a helper function
    int howlcalled_count = 0;
    if (!you.props.exists(NEXT_DOOM_HOUND_KEY))
        you.props[NEXT_DOOM_HOUND_KEY] = random_range(20, 40);
    // 1 nasty beast every 2-4 turns
    while (time > 0)
    {
        const int time_to_call = you.props[NEXT_DOOM_HOUND_KEY].get_int();
        if (time_to_call <= time)
        {
            you.props[NEXT_DOOM_HOUND_KEY] = random_range(20, 40);
            ++howlcalled_count;
        }
        else
            you.props[NEXT_DOOM_HOUND_KEY].get_int() -= time;
        time -= time_to_call;
    }

    if (!howlcalled_count)
        return;

    const actor *target = &you;

    for (int i = 0; i < howlcalled_count; ++i)
    {
        const monster_type howlcalled = random_choose(
                MONS_BONE_DRAGON, MONS_SHADOW_DRAGON, MONS_SHADOW_DEMON,
                MONS_REAPER, MONS_TORMENTOR, MONS_TZITZIMITL
        );
        vector<coord_def> spots;
        for (adjacent_iterator ai(target->pos()); ai; ++ai)
        {
            if (monster_habitable_grid(howlcalled, grd(*ai))
                && !actor_at(*ai))
            {
                spots.push_back(*ai);
            }
        }
        if (spots.size() <= 0)
            continue;

        const coord_def pos = spots[random2(spots.size())];

        monster *mons = create_monster(mgen_data(howlcalled, BEH_HOSTILE,
                                                 pos, target->mindex(),
                                                 MG_FORCE_BEH));
        if (mons)
        {
            mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                       INFINITE_DURATION));
            mons->behaviour = BEH_SEEK;
            mons_add_blame(mons, "called by a doom hound"); // assumption!
            check_place_cloud(CLOUD_BLACK_SMOKE, mons->pos(),
                              random_range(1,2), mons);
        }
    }
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

    for (int i = 0; i < how_many; ++i)
    {
        if (monster *dragon = create_monster(
                _summon_data(*caster, mon, 6, god, SPELL_SUMMON_DRAGON)))
        {
            if (you.see_cell(dragon->pos()))
				mpr("용이 나타났다.");
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

    mgen_data viper = _pal_data(MONS_MANA_VIPER, 2, god,
                                SPELL_SUMMON_MANA_VIPER);
    viper.hd = (5 + div_rand_round(pow, 12));

    // Don't scale hp at the same time as their antimagic power
    viper.hp = hit_points(495); // avg 50

    if (create_monster(viper))
		mpr("마력 독사가 쉿쉿하는 소리를 내며 나타났다.");
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
            mon = random_choose(MONS_BLACK_BEAR, MONS_POLAR_BEAR);
        }
        else if (pow <= 140)
        {
            // ogres
            mon = random_choose_weighted(1, MONS_TWO_HEADED_OGRE, 2, MONS_OGRE);
        }
        else if (pow <= 180)
        {
            // trolls
            mon = random_choose_weighted(3, MONS_TROLL,
                                         3, MONS_DEEP_TROLL,
                                         2, MONS_IRON_TROLL);
        }
        else
        {
            // giants
            mon = random_choose(MONS_CYCLOPS, MONS_STONE_GIANT);
        }
    }

    mgen_data mg(mon, caster ? BEH_COPY : BEH_HOSTILE,
                 caster ? caster->pos() : you.pos(),
                 (caster && caster->is_monster()) ? ((monster*)caster)->foe
                                                  : MHITYOU,
                 MG_AUTOFOE);
    mg.set_summoned(caster, caster ? dur : 0, SPELL_NO_SPELL, GOD_TROG);

    if (!caster)
    {
        mg.non_actor_summoner = "the rage of " + god_name(GOD_TROG, false);
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
    }

    monster *mons = create_monster(mg);

    if (!mons)
        return false;

    _make_mons_berserk_summon(mons);
    return true;
}

// Not a spell. Rather, this is TSO's doing.
bool summon_holy_warrior(int pow, bool punish)
{
    mgen_data mg(random_choose(MONS_ANGEL, MONS_DAEVA),
                 punish ? BEH_HOSTILE : BEH_FRIENDLY,
                 you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE);
    mg.set_summoned(punish ? 0 : &you,
                    punish ? 0 : min(2 + (random2(pow) / 4), 6),
                    SPELL_NO_SPELL, GOD_SHINING_ONE);

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
		mpr("당신은 밝은 빛에 순간적으로 눈이 멀었다.");

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
static string _get_item_desc(const item_def* wpn, bool target_is_player)
{
    return wpn->name(target_is_player ? DESC_YOUR : DESC_THE);
}

/**
 * Checks if Tukima's Dance can actually affect the target (and anger them)
 *
 * @param target  The targeted monster (or player).
 * @return        Whether the target can be affected by Tukima's Dance.
 **/
bool tukima_affects(const actor &target)
{
    const item_def* wpn = target.weapon();
    return wpn
           && is_weapon(*wpn)
           && !is_range_weapon(*wpn)
           && !is_special_unrandom_artefact(*wpn)
           && !mons_class_is_animated_weapon(target.type)
           && !mons_is_hepliaklqana_ancestor(target.type);
}

/**
 * Checks if Tukima's Dance is being cast on a valid target.
 *
 * @param target     The spell's target.
 * @return           Whether the target is valid.
 **/
static bool _check_tukima_validity(const actor *target)
{
    bool target_is_player = target == &you;
    const item_def* wpn = target->weapon();
    bool can_see_target = target_is_player || target->visible_to(&you);

    // See if the wielded item is appropriate.
    if (!wpn)
    {
        if (!can_see_target)
            return _fail_tukimas();

        if (target_is_player)
            mpr(you.hands_act("휘익", "."));
        else
        {
            // FIXME: maybe move hands_act to class actor?
            bool plural = true;
            const string hand = target->hand_name(true, &plural);

            mprf("%s %s %s.",
                 apostrophise(target->name(DESC_THE)).c_str(),
                 hand.c_str(), conjugate_verb("twitch", plural).c_str());
        }
        return false;
    }

    if (!tukima_affects(*target))
    {
        if (!can_see_target)
            return _fail_tukimas();

        if (mons_class_is_animated_weapon(target->type))
        {
            simple_monster_message(*(monster*)target,
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
 **/
static void _animate_weapon(int pow, actor* target)
{
    bool target_is_player = target == &you;
    item_def * const wpn = target->weapon();
    ASSERT(wpn);
    if (target_is_player)
    {
        // Clear temp branding so we don't change the brand permanently.
        if (you.duration[DUR_EXCRUCIATING_WOUNDS])
            end_weapon_brand(*wpn);

        // Mark weapon as "thrown", so we'll autopickup it later.
        wpn->flags |= ISFLAG_THROWN;
    }
    // If sac love, the weapon will go after you, not the target.
    const bool sac_love = you.get_mutation_level(MUT_NO_LOVE);
    // Self-casting haunts yourself! MUT_NO_LOVE overrides force friendly.
    const bool friendly = !target_is_player && !sac_love;
    const int dur = min(2 + (random2(pow) / 5), 6);

    mgen_data mg(MONS_DANCING_WEAPON,
                 friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                 target->pos(),
                 (target_is_player || sac_love) ? MHITYOU : target->mindex(),
                 sac_love ? MG_NONE : MG_FORCE_BEH);
    mg.set_summoned(&you, dur, SPELL_TUKIMAS_DANCE);
    mg.props[TUKIMA_WEAPON] = *wpn;
    mg.props[TUKIMA_POWER] = pow;

    monster * const mons = create_monster(mg);

    if (!mons)
    {
        mprf("%s twitches for a moment.",
             _get_item_desc(wpn, target_is_player).c_str());
        return;
    }

    // Don't haunt yourself under sac love.
    if (!sac_love)
    {
        mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                   INFINITE_DURATION));
        mons->foe = target->mindex();
    }

    // We are successful. Unwield the weapon, removing any wield effects.
    mprf("%s dances into the air!",
         _get_item_desc(wpn, target_is_player).c_str());
    if (target_is_player)
        unwield_item();
    else
    {
        monster * const montarget = target->as_monster();
        const int primary_weap = montarget->inv[MSLOT_WEAPON];
        const mon_inv_type wp_slot = (primary_weap != NON_ITEM
                                      && &mitm[primary_weap] == wpn) ?
                                         MSLOT_WEAPON : MSLOT_ALT_WEAPON;
        ASSERT(montarget->inv[wp_slot] != NON_ITEM);
        ASSERT(&mitm[montarget->inv[wp_slot]] == wpn);

        montarget->unequip(*(montarget->mslot_item(wp_slot)), false, true);
        montarget->inv[wp_slot] = NON_ITEM;
    }

    // Find out what our god thinks before killing the item.
    conduct_type why = god_hates_item_handling(*wpn);

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
 **/
void cast_tukimas_dance(int pow, actor* target)
{
    ASSERT(target);

    if (!_check_tukima_validity(target))
        return;

    _animate_weapon(pow, target);
}

spret_type cast_conjure_ball_lightning(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    const int how_many = min(5, 2 + pow / 100 + random2(pow / 50 + 1));

    mgen_data cbl(MONS_BALL_LIGHTNING, BEH_FRIENDLY, you.pos());
    cbl.set_summoned(&you, 0, SPELL_CONJURE_BALL_LIGHTNING, god);
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
		mpr("당신은 전기 구체를 생성했다!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_lightning_spire(int pow, const coord_def& where, god_type god, bool fail)
{
    const int dur = 2;

    if (grid_distance(where, you.pos()) > spell_range(SPELL_SUMMON_LIGHTNING_SPIRE,
                                                      pow)
        || !in_bounds(where))
    {
		mpr("너무 멀리 떨어져있다.");
        return SPRET_ABORT;
    }

    if (!monster_habitable_grid(MONS_HUMAN, grd(where)))
    {
		mpr("그 지점에 설치할 수 없다.");
        return SPRET_ABORT;
    }

    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(*mons))
        {
			mpr("그 장소는 이미 꽉 차있다.");
            return SPRET_ABORT;
        }

        fail_check();

        // invisible monster
		mpr("당신이 볼 수 없는 무언가가 당신의 설치를 방해하고 있다!");
        return SPRET_SUCCESS;
    }

    fail_check();

    mgen_data spire(MONS_LIGHTNING_SPIRE, BEH_FRIENDLY, where, MHITYOU,
                    MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);
    spire.set_summoned(&you, dur, SPELL_SUMMON_LIGHTNING_SPIRE,  god);
    spire.hd = max(1, div_rand_round(pow, 10));

    if (create_monster(spire))
    {
        if (!silenced(where))
			mpr("전하가 웅웅거리는 소리가 대기를 가득 채웠다.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;

}

spret_type cast_summon_guardian_golem(int pow, god_type god, bool fail)
{
    fail_check();

    mgen_data golem = _pal_data(MONS_GUARDIAN_GOLEM, 3, god,
                                SPELL_SUMMON_GUARDIAN_GOLEM);
    golem.flags &= ~MG_AUTOFOE; // !!!
    golem.hd = 4 + div_rand_round(pow, 16);

    monster* mons = (create_monster(golem));

    if (mons)
    {
        // Immediately apply injury bond
        guardian_golem_bond(*mons);

		mpr("당신의 아군 대신 피해를 입는 가디언 골렘이 나타났다.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

/**
 * Choose a type of imp to summon with Call Imp.
 *
 * @param pow   The power with which the spell is being cast.
 * @return      An appropriate imp type.
 */
static monster_type _get_imp_type(int pow)
{
    // Proportion of white imps is independent of spellpower.
    if (x_chance_in_y(5, 18))
        return MONS_WHITE_IMP;

    // 3/13 * 13/18 = 1/6 chance of one of these two at 0-46 spellpower,
    // increasing up to about 4/9 at max spellpower.
    if (random2(pow) >= 46 || x_chance_in_y(3, 13))
        return one_chance_in(3) ? MONS_IRON_IMP : MONS_SHADOW_IMP;

    // 5/9 crimson at 0-46 spellpower, about half that at max power.
    return MONS_CRIMSON_IMP;
}

static map<monster_type, const char*> _imp_summon_messages = {
    { MONS_WHITE_IMP,
        "A beastly little devil appears in a puff of frigid air." },
    { MONS_IRON_IMP, "A metallic apparition takes form in the air." },
    { MONS_SHADOW_IMP, "A shadowy apparition takes form in the air." },
    { MONS_CRIMSON_IMP, "A beastly little devil appears in a puff of flame." },
};

/**
 * Cast the spell Call Imp, summoning a friendly imp nearby.
 *
 * @param pow   The spellpower at which the spell is being cast.
 * @param god   The god of the caster.
 * @param fail  Whether the caster (you) failed to cast the spell.
 * @return      SPRET_FAIL if fail is true; SPRET_SUCCESS otherwise.
 */
spret_type cast_call_imp(int pow, god_type god, bool fail)
{
    fail_check();

    const monster_type imp_type = _get_imp_type(pow);

    const int dur = min(2 + (random2(pow) / 4), 6);

    mgen_data imp_data = _pal_data(imp_type, dur, god, SPELL_CALL_IMP);
    imp_data.flags |= MG_FORCE_BEH; // disable player_angers_monster()
    if (monster *imp = create_monster(imp_data))
    {
        mpr(_imp_summon_messages[imp_type]);

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
                       charmed ? BEH_CHARMED
                               : BEH_HOSTILE,
                      you.pos(), MHITYOU, MG_FORCE_BEH | MG_AUTOFOE)
            .set_summoned(&you, dur, spell, god)))
    {
        success = true;

		mpr("악마가 나타났다!");

        if (!player_angers_monster(demon) && !friendly)
        {
			mpr(charmed ? "당신은 좋지 않은 예감이 들었다..."
                        : "그것은 행복하지않은 것 같다. ...매우.");
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
        type = random_demon_by_tier(4);
    else
        type = random_demon_by_tier(3);

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
                       int spell, bool friendly)
{
    return _summon_demon_wrapper(pow, god, spell, mon,
                                 min(2 + (random2(pow) / 4), 6),
                                 friendly, false, false);
}

spret_type cast_summon_demon(int pow, god_type god, bool fail)
{
    fail_check();
	mpr("판데모니엄으로 통하는 문을 열었다!");

    if (!_summon_common_demon(pow, god, SPELL_SUMMON_DEMON, false))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_summon_greater_demon(int pow, god_type god, bool fail)
{
    fail_check();
	mpr("판데모니엄으로 통하는 문을 열었다!");

    if (!_summon_greater_demon(pow, god, SPELL_SUMMON_GREATER_DEMON, false))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

spret_type cast_shadow_creatures(int st, god_type god, level_id place,
                                 bool fail)
{
    fail_check();
    const bool scroll = (st == MON_SUMM_SCROLL);
	mpr("그림자의 조각이 당신 주위를 회전하기 시작했다...");

    int num = (scroll ? roll_dice(2, 2) : 1);
    int num_created = 0;

    for (int i = 0; i < num; ++i)
    {
        if (monster *mons = create_monster(
            mgen_data(RANDOM_COMPATIBLE_MONSTER, BEH_FRIENDLY, you.pos(),
                      MHITYOU, MG_FORCE_BEH | MG_AUTOFOE | MG_NO_OOD)
                      // This duration is only used for band members.
                      .set_summoned(&you, scroll ? 2 : 1, st, god)
                      .set_place(place),
            false))
        {
            // In the rare cases that a specific spell set of a monster will
            // cause anger, even if others do not, try rerolling
            int tries = 0;
            while (player_will_anger_monster(*mons) && ++tries <= 20)
            {
                // Save the enchantments, particularly ENCH_SUMMON etc.
                mon_enchant_list ench = mons->enchantments;
                FixedBitVector<NUM_ENCHANTMENTS> cache = mons->ench_cache;
                if (mons_class_is_zombified(mons->type))
                    define_zombie(mons, mons->base_monster, mons->type);
                else
                    define_monster(*mons);
                mons->enchantments = ench;
                mons->ench_cache = cache;
            }

            // If we didn't find a valid spell set yet, just give up
            if (tries > 20)
                monster_die(*mons, KILL_RESET, NON_MONSTER);
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
                    if (player_will_anger_monster(**mi))
                        monster_die(**mi, KILL_RESET, NON_MONSTER);

                    mi->props["summon_id"].get_int() = mons->mid;
                }
            }

            num_created++;
        }
    }

    if (!num_created)
		mpr("그림자가 아무 효과도 없이 흩어져 사라졌다.");

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

    for (coord_def delta : Compass)
    {
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
                                    : caster->as_monster()->full_name(DESC_A),
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

        return SPRET_SUCCESS;
    }
    // We don't care if monsters fail to cast it.
    if (is_player)
		mpr("이런 비좁은 장소에서는 문을 열 수 없다!");

    return SPRET_ABORT;
}

spret_type cast_summon_horrible_things(int pow, god_type god, bool fail)
{
    fail_check();
    if (god == GOD_NO_GOD && one_chance_in(5))
    {
        // if someone deletes the db, no message is ok
        mpr(getMiscString("SHT_int_loss"));
        lose_stat(STAT_INT, 1);
    }

    int num_abominations = random_range(2, 4) + x_chance_in_y(pow, 200);
    int num_tmons = random2(pow) > 120 ? 2 : random2(pow) > 50 ? 1 : 0;

    if (num_tmons == 0)
        num_abominations++;

    int count = 0;

    while (num_abominations-- > 0)
    {
        const mgen_data abom = _pal_data(MONS_ABOMINATION_LARGE, 3, god,
                                         SPELL_SUMMON_HORRIBLE_THINGS);
        if (create_monster(abom))
            ++count;
    }

    while (num_tmons-- > 0)
    {
        const mgen_data tmons = _pal_data(MONS_TENTACLED_MONSTROSITY, 3, god,
                                          SPELL_SUMMON_HORRIBLE_THINGS);
        if (create_monster(tmons))
            ++count;
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
 *               SPRET_SUCCESS if the spell was successfully cast.
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
        for (distance_iterator di(caster->pos(), false, true,
                                  LOS_DEFAULT_RANGE); di; ++di)
        {
            if ((feat_is_wall(grd(*di)) && !feat_is_permarock(grd(*di))
                 && x_chance_in_y(pow, 150))
                || (grd(*di) == DNGN_FLOOR && x_chance_in_y(pow, 1250)
                    && !actor_at(*di) && !plant_forbidden_at(*di, true)))
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

		mpr("숲으로 뒤덮인 평원이 엄청난 소리와 함께 충돌했다!");
        noisy(spell_effect_noise(SPELL_SUMMON_FOREST), caster->pos());

        mgen_data dryad_data = _pal_data(MONS_DRYAD, 1, god,
                                         SPELL_SUMMON_FOREST);
        dryad_data.hd = 5 + div_rand_round(pow, 18);

        if (monster *dryad = create_monster(dryad_data))
        {
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

	mpr("이 주문을 사용하기 위해선 더 넓은 공간이 필요하다.");
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

/**
 * Equip the dearly departed with its ex-possessions.
 *
 * This excludes holy items (not wieldable by the undead, and items which were
 * dropped onto the square, so the player can't equip their undead slaves with
 * items of their choice.
 *
 * @param a      where to pull items from
 * @param corpse the corpse of the dead monster.
 * @param mon    the zombie/skeleton/etc. being reequipped.
 */
static void _equip_undead(const coord_def &a, const item_def& corpse, monster *mon)
{
    if (!corpse.props.exists(MONSTER_MID))
        return;
    for (stack_iterator si(a); si; ++si)
    {
        if (!si->props.exists(DROPPER_MID_KEY)
            || si->props[DROPPER_MID_KEY].get_int()
               != corpse.props[MONSTER_MID].get_int())
        {
            continue;
        }

        // Don't equip the undead with holy items.
        if (is_holy_item(*si))
            continue;

        // Stupid undead can't use most items.
        if (si->base_type != OBJ_WEAPONS
            && si->base_type != OBJ_STAVES
            && si->base_type != OBJ_ARMOUR
            || is_range_weapon(*si))
        {
            continue;
        }

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->pickup_item(*si, false, true);
    }
}

// Displays message when raising dead with Animate Skeleton or Animate Dead.
static void _display_undead_motions(int motions)
{
    vector<const char *> motions_list;

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
		mpr("시체들이 일어났다!");
    else
    {
        mprf("The dead are %s!", comma_separated_line(motions_list.begin(),
             motions_list.end()).c_str());
    }
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

    const int hd = (item.props.exists(MONSTER_HIT_DICE)) ?
                    item.props[MONSTER_HIT_DICE].get_short() : 0;

    // Save the corpse name before because it can get destroyed if it is
    // being drained and the raising interrupts it.
    monster_flags_t name_type;
    string name;
    if (is_named_corpse(item))
        name = get_corpse_name(item, &name_type);

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
			mpr("그 좀비를 일으키기엔 살이 너무 썩어있다; "
                "오직 스켈레톤으로만 일으킬 수 있다.");
        }
        mon = MONS_SKELETON;
    }

    // Use the original monster type as the zombified type here, to get
    // the proper stats from it.
    mgen_data mg(mon, beha, pos, hitting,
                 MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);
    mg.set_summoned(as, 0, 0, god);
    mg.set_base(monnum);

    if (item.props.exists(CORPSE_HEADS))
    {
        // Headless hydras cannot be raised, sorry.
        if (item.props[CORPSE_HEADS].get_short() == 0)
        {
            if (you.see_cell(pos))
            {
                mprf("The headless hydra %s sways and immediately collapses!",
                     item.sub_type == CORPSE_BODY ? "corpse" : "skeleton");
            }
            return false;
        }
        mg.props[MGEN_NUM_HEADS] = item.props[CORPSE_HEADS].get_short();
    }

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

    if (god == GOD_NO_GOD) // only Yred dead-raising lasts forever.
        mons->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));

    // If the original monster has been levelled up, its HD might be different
    // from its class HD, in which case its HP should be rerolled to match.
    if (mons->get_experience_level() != hd)
    {
        mons->set_hit_dice(max(hd, 1));
        roll_zombie_hp(mons);
    }

    if (!name.empty()
        && (!name_type || (name_type & MF_NAME_MASK) == MF_NAME_REPLACE))
    {
        name_zombie(*mons, monnum, name);
    }

    // Re-equip the zombie.
    if (mons_class_itemuse(monnum) >= MONUSE_STARTING_EQUIPMENT)
        _equip_undead(pos, item, mons);

    // Destroy the monster's corpse, as it's no longer needed.
    item_was_destroyed(item);
    destroy_item(corps);

    if (!force_beh)
        player_angers_monster(mons);

    // Bitfield for motions - determines text displayed when animating dead.
    // XXX: could this use monster shape in some way?
    if (mons_class_primary_habitat(zombie_type)    == HT_WATER
        || mons_class_primary_habitat(zombie_type) == HT_LAVA)
    {
        *motions_r |= DEAD_ARE_SWIMMING;
    }
    else if (mons_class_flag(zombie_type, M_FLIES))
        *motions_r |= DEAD_ARE_FLYING;
    else if (mons_genus(zombie_type)    == MONS_SNAKE
             || mons_genus(zombie_type) == MONS_NAGA
             || mons_genus(zombie_type) == MONS_SALAMANDER
             || mons_genus(zombie_type) == MONS_GUARDIAN_SERPENT
             || mons_genus(zombie_type) == MONS_ELEPHANT_SLUG
             || mons_genus(zombie_type) == MONS_TYRANT_LEECH
             || mons_genus(zombie_type) == MONS_WORM)
    {
        *motions_r |= DEAD_ARE_SLITHERING;
    }
    else if (mons_genus(zombie_type)    == MONS_FROG
             || mons_genus(zombie_type) == MONS_QUOKKA)
    {
        *motions_r |= DEAD_ARE_HOPPING;
    }
    else if (mons_genus(zombie_type)    == MONS_WORKER_ANT
             || mons_genus(zombie_type) == MONS_BEETLE
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

    if (grd(a) == DNGN_DEEP_WATER)
        return 0; // trapped in davy jones' locker...

    int number_found = 0;
    bool any_success = false;
    int motions = 0;

    // Search all the items on the ground for a corpse.
    for (stack_iterator si(a, true); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES)
            continue;

        if (class_allowed != CORPSE_BODY && si->sub_type != CORPSE_SKELETON)
            continue;

        number_found++;

        if (!_animatable_remains(*si))
            continue;

        const bool was_draining = is_being_drained(*si);
        const bool was_butchering = is_being_butchered(*si);

        const bool success = _raise_remains(a, si.index(), beha, hitting,
                                            as, nas, god, actual,
                                            force_beh, mon, &motions);

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

        any_success |= success;

        break;
    }

    if (motions_r && you.see_cell(a))
        *motions_r |= motions;

    if (number_found == 0)
        return -1;

    if (!any_success)
        return 0;

    return 1;
}

int animate_dead(actor *caster, int /*pow*/, beh_type beha,
                 unsigned short hitting, actor *as, string nas, god_type god,
                 bool actual)
{
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
		mpr("일으킬 수 있는 시체가 하나도 없다!");
        return SPRET_ABORT;
    }

    fail_check();
    canned_msg(MSG_ANIMATE_REMAINS);

    const char* no_space = "...but the skeleton had no space to rise!";

    // First, we try to animate a skeleton if there is one.
    const int animate_skel_result = animate_remains(you.pos(), CORPSE_SKELETON,
                                                    BEH_FRIENDLY, MHITYOU,
                                                    &you, "", god);
    if (animate_skel_result != -1)
    {
        if (animate_skel_result == 0)
            mpr(no_space);
        return SPRET_SUCCESS;
    }

    // If not, look for a corpse and butcher it.
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->is_type(OBJ_CORPSES, CORPSE_BODY)
            && mons_skeleton(si->mon_type)
            && mons_class_can_be_zombified(si->mon_type))
        {
            butcher_corpse(*si, MB_TRUE);
			mpr("당신의 눈 앞에서, 살점이 시체로부터 떨어져 나왔다!");
            request_autopickup();
            // Only convert the top one.
            break;
        }
    }

    // Now we try again to animate a skeleton.
    // this return type is insanely stupid
    const int animate_result = animate_remains(you.pos(), CORPSE_SKELETON,
                                               BEH_FRIENDLY, MHITYOU, &you, "",
                                               god);
    dprf("result: %d", animate_result);
    switch (animate_result)
    {
        case -1:
			mpr("이 곳엔 일으킬 스켈레톤이 없다!");
            break;
        case 0:
            mpr(no_space);
            break;
        default:
            // success, messages already printed
            break;
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
        if (si->is_type(OBJ_CORPSES, CORPSE_BODY)
            && mons_class_can_be_zombified(si->mon_type))
        {
            found = true;
            co = si->index();
        }
    }

    if (!found)
    {
		mpr("일으킬만한 것이 없다!");
        return SPRET_ABORT;
    }

    fail_check();
    canned_msg(MSG_ANIMATE_REMAINS);

    item_def& corpse = mitm[co];
    // How many simulacra can this particular monster give at maximum.
    int num_sim  = 1 + random2(max_corpse_chunks(corpse.mon_type));
    num_sim  = stepdown_value(num_sim, 4, 4, 12, 12);

    mgen_data mg = _pal_data(MONS_SIMULACRUM, 0, god, SPELL_SIMULACRUM);
    mg.set_base(corpse.mon_type);

    // Can't create more than the max for the monster.
    int how_many = min(8, 4 + random2(pow) / 20);
    how_many = min<int>(how_many, num_sim);

    if (corpse.props.exists(CORPSE_HEADS))
    {
        // Avoid headless hydras. Unlike Animate Dead, still consume the flesh.
        if (corpse.props[CORPSE_HEADS].get_short() == 0)
        {
            // No monster to conj_verb with :(
            mprf("The headless hydra simulacr%s immediately collapse%s into snow!",
                 how_many == 1 ? "um" : "a", how_many == 1 ? "s" : "");
            if (!turn_corpse_into_skeleton(corpse))
                butcher_corpse(corpse, MB_FALSE, false);
            return SPRET_SUCCESS;
        }
        mg.props[MGEN_NUM_HEADS] = corpse.props[CORPSE_HEADS].get_short();
    }

    int count = 0;
    for (int i = 0; i < how_many; ++i)
    {
        if (monster *sim = create_monster(mg))
        {
            count++;
            sim->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 4));
        }
    }

    if (!count)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (!turn_corpse_into_skeleton(corpse))
        butcher_corpse(corpse, MB_FALSE, false);

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
            if (si->base_type != OBJ_CORPSES
                || si->sub_type != CORPSE_BODY
                || !mons_class_can_be_zombified(si->mon_type))
            {
                continue;
            }

            mgen_data mg(MONS_SIMULACRUM, SAME_ATTITUDE(mon), *ri, mon->foe,
                         MG_FORCE_BEH
                         | (cast_visible ? MG_DONT_COME : MG_NONE));
            mg.set_base(si->mon_type);
            mg.set_summoned(mon, 0, SPELL_SIMULACRUM, mon->god);
            if (si->props.exists(CORPSE_HEADS))
            {
                if (si->props[CORPSE_HEADS].get_short() == 0)
                    continue;
                else
                    mg.props[MGEN_NUM_HEADS] = si->props[CORPSE_HEADS].get_short();
            }

            if (!actual)
                return true;

            // Create half as many as the player version.
            int how_many = 1 + random2(
                                  div_rand_round(
                                    max_corpse_chunks(si->mon_type), 2));
            how_many  = stepdown_value(how_many, 2, 2, 6, 6);
            bool was_draining = is_being_drained(*si);
            bool was_butchering = is_being_butchered(*si);
            bool was_successful = false;
            for (int i = 0; i < how_many; ++i)
            {
                // Use the original monster type as the zombified type here,
                // to get the proper stats from it.
                if (monster *sim = create_monster(mg))
                {
                    was_successful = true;
                    player_angers_monster(sim);
                    sim->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 4));
                    if (you.can_see(*sim))
                        num_seen++;
                }
            }

            if (was_successful)
            {
                did_creation = true;
                turn_corpse_into_skeleton(*si);
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
        int total_max_chunks = 0;
        const bool visible = you.see_cell(*ri);

        // Count up number/size of corpses at this location.
        for (stack_iterator si(*ri); si; ++si)
        {
            if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
            {
                if (!actual)
                {
                    ++num_crawlies;
                    continue;
                }

                if (mons_genus(si->mon_type) == MONS_ORC)
                    num_orcs++;
                if (mons_class_holiness(si->mon_type) & MH_HOLY)
                    num_holy++;

                total_max_chunks += max_corpse_chunks(si->mon_type);

                ++num_corpses;
                item_was_destroyed(*si);
                destroy_item(si->index());
            }
        }

        if (!actual || num_corpses == 0)
            continue;

        // 3 HD per 2 max chunks at 500 power.
        int hd = div_rand_round((pow + 100) * total_max_chunks, 400);

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

        mgen_data mg(montype, beha, *ri, foe, MG_FORCE_BEH | MG_AUTOFOE);
        mg.set_summoned(caster, 0, 0, god);
        if (monster *mons = create_monster(mg))
        {
            // Set hit dice, AC, and HP.
            init_abomination(*mons, hd);

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

monster_type pick_random_wraith()
{
    return random_choose_weighted(1, MONS_PHANTOM,
                                  1, MONS_HUNGRY_GHOST,
                                  1, MONS_SHADOW_WRAITH,
                                  5, MONS_WRAITH,
                                  2, MONS_FREEZING_WRAITH,
                                  2, MONS_PHANTASMAL_WARRIOR);
}

spret_type cast_haunt(int pow, const coord_def& where, god_type god, bool fail)
{
    monster* m = monster_at(where);

    if (m == nullptr)
    {
        fail_check();
		mpr("사악한 기운이 모여들었지만, 금새 흩어져버렸다.");
        return SPRET_SUCCESS; // still losing a turn
    }
    else if (m->wont_attack())
    {
		mpr("당신에게 적대감을 표하지 않는 존재에게 유령을 출몰시킬 순 없다.");
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
        const monster_type mon = pick_random_wraith();

        if (monster *mons = create_monster(
                mgen_data(mon, BEH_FRIENDLY, where, mi, MG_FORCE_BEH)
                .set_summoned(&you, 3, SPELL_HAUNT, god)))
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
		mpr(friendly ? "실체없는 물체들이 허공에 나타났다."
                     : "당신은 적대적인 존재감을 감지했다.");
    }
    else if (success)
    {
		mpr(friendly ? "실체없는 물체가 허공에 나타났다."
                     : "당신은 적대적인 존재감을 감지했다.");
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    return SPRET_SUCCESS;
}



static spell_type servitor_spells[] =
{
    // primary spells
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_IOOD,
    SPELL_IRON_SHOT,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_POISON_ARROW,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,
    SPELL_BOLT_OF_DRAINING,
    SPELL_VENOM_BOLT,
    SPELL_THROW_ICICLE,
    SPELL_STONE_ARROW,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    // secondary spells
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_FIREBALL,
    SPELL_AIRSTRIKE,
    SPELL_LRD,
    SPELL_FREEZING_CLOUD,
    SPELL_POISONOUS_CLOUD,
    SPELL_FORCE_LANCE,
    SPELL_DAZZLING_SPRAY,
    SPELL_MEPHITIC_CLOUD,
    // fallback spells
    SPELL_STICKY_FLAME,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_FREEZE,
    SPELL_FLAME_TONGUE,
    SPELL_STING,
    SPELL_SANDBLAST,
    SPELL_MAGIC_DART,
};

/**
 * Initialize the given spellforged servitor's HD and spellset, based on the
 * caster's spellpower and castable attack spells.
 *
 * @param mon       The spellforged servitor to be initialized.
 * @param caster    The entity summoning the servitor; may be the player.
 */
static void _init_servitor_monster(monster &mon, const actor& caster)
{
    const monster* caster_mon = caster.as_monster();
    const int pow = caster_mon ?
                        6 * caster_mon->spell_hd(SPELL_SPELLFORGED_SERVITOR) :
                        calc_spell_power(SPELL_SPELLFORGED_SERVITOR, true);

    mon.set_hit_dice(9 + div_rand_round(pow, 14));
    mon.max_hit_points = mon.hit_points = 60 + roll_dice(7, 5); // 67-95
                                            // mhp doesn't vary with HD
    int spell_levels = 0;

    for (const spell_type spell : servitor_spells)
    {
        if (caster.has_spell(spell)
            && (caster_mon || raw_spell_fail(spell) < 50))
        {
            mon.spells.emplace_back(spell, 0, MON_SPELL_WIZARD);
            spell_levels += spell_difficulty(spell);
        }
    }

    // Fix up frequencies now that we know the total number of spell levels.
    const int base_freq = caster_mon ? 67 : 200;
    for (auto& slot : mon.spells)
    {
        slot.freq = max(1, div_rand_round(spell_difficulty(slot.spell)
                                          * base_freq,
                                          spell_levels));
    }
    mon.props[CUSTOM_SPELLS_KEY].get_bool() = true;
}

void init_servitor(monster* servitor, actor* caster)
{
    ASSERT(servitor); // XXX: change to monster &servitor
    ASSERT(caster); // XXX: change to actor &caster
    _init_servitor_monster(*servitor, *caster);

    if (you.can_see(*caster))
    {
        mprf("%s %s a servant imbued with %s destructive magic!",
             caster->name(DESC_THE).c_str(),
             caster->conj_verb("summon").c_str(),
             caster->pronoun(PRONOUN_POSSESSIVE).c_str());
    }
    else
        simple_monster_message(*servitor, " appears!");

    int shortest_range = LOS_RADIUS + 1;
    for (const mon_spell_slot &slot : servitor->spells)
    {
        if (slot.spell == SPELL_NO_SPELL)
            continue;

        int range = spell_range(slot.spell, 100, false);
        if (range < shortest_range)
            shortest_range = range;
    }
    servitor->props["ideal_range"].get_int() = shortest_range;
}

spret_type cast_spellforged_servitor(int pow, god_type god, bool fail)
{
    fail_check();

    mgen_data mdata = _pal_data(MONS_SPELLFORGED_SERVITOR, 4, god,
                                SPELL_SPELLFORGED_SERVITOR);

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
        if (mons_is_god_gift(*mon, GOD_SHINING_ONE))
        {
            sockage = sockage * (30 - mon->get_hit_dice()) / 45;
            if (sockage < duration)
            {
                simple_god_message(" protects a fellow warrior from your evil magic!",
                                   GOD_SHINING_ONE);
                shielded = true;
            }
        }
        else if (mons_is_god_gift(*mon, GOD_TROG))
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
            simple_monster_message(*mon, " shudders.");
    }

    return true;
}

spret_type cast_aura_of_abjuration(int pow, bool fail)
{
    fail_check();

    if (!you.duration[DUR_ABJURATION_AURA])
		mpr("당신은 주위의 소환물을 돌려보내기 시작했다!");
    else
		mpr("당신은 송환의 오라를 내뿜기 시작했다.");

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

monster* find_battlesphere(const actor* agent)
{
    if (agent->props.exists("battlesphere"))
        return monster_by_mid(agent->props["battlesphere"].get_int());
    else
        return nullptr;
}

spret_type cast_battlesphere(actor* agent, int pow, god_type god, bool fail)
{
    fail_check();

    monster* battlesphere;
    if (agent->is_player() && (battlesphere = find_battlesphere(&you)))
    {
        bool recalled = false;
        if (!you.can_see(*battlesphere))
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
			mpr("당신은 당신의 전투 구체를 다시 불러냈고, 마력을 "
                " 추가로 충전하였다.");
        }
        else
			mpr("당신은 전투 구체의 충전량을 채웠다.");

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
                      agent->pos(), agent->mindex());
        mg.set_summoned(agent, 0, SPELL_BATTLESPHERE, god);
        mg.hd = 1 + div_rand_round(pow, 11);
        battlesphere = create_monster(mg);

        if (battlesphere)
        {
            int dur = min((7 + roll_dice(2, pow)) * 10, 500);
            battlesphere->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 1, 0, dur));
            battlesphere->summoner = agent->mid;
            agent->props["battlesphere"].get_int() = battlesphere->mid;

            if (agent->is_player())
				mpr("당신은 마력으로 이루어진 구를 소환했다.");
            else
            {
                if (you.can_see(*agent) && you.can_see(*battlesphere))
                {
                    simple_monster_message(*agent->as_monster(),
                                           " conjures a globe of magical energy!");
                }
                else if (you.can_see(*battlesphere))
                    simple_monster_message(*battlesphere, " appears!");
                battlesphere->props["band_leader"].get_int() = agent->mid;
            }
            battlesphere->battlecharge = 4 + random2(pow + 10) / 10;
            battlesphere->foe = agent->mindex();
            battlesphere->target = agent->pos();
        }
        else if (agent->is_player() || you.can_see(*agent))
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
            if (you.can_see(*mons))
            {
                if (mons->battlecharge == 0)
                {
					mpr("당신의 전투 구체가 마지막 에너지를 소모하고 소멸하였다.");
                }
                else
					mpr("당신의 전투 구체가 불안정하게 흔들리더니 결속력을 잃었다.");
            }
            else
				mpr("당신과 전투 구체 간의 결속이 약해진 것이 느껴졌다.");
        }
        else if (you.can_see(*mons))
            simple_monster_message(*mons, " dissipates.");

        if (!cell_is_solid(mons->pos()))
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);

        monster_die(*mons, KILL_RESET, NON_MONSTER);
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
        zappy(ztype, powc, false, testbeam);

        battlesphere->props["firing_target"] = beam.target;
        battlesphere->props.erase("foe");
        if (!actor_at(beam.target))
        {
            testbeam.fire();

            for (const coord_def c : testbeam.path_taken)
            {
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

            for (radius_iterator ri(beam.target, beam.ex_size, C_SQUARE);
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

        // Sanity check: if we have somehow ended up targeting ourselves, bail
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
        beam.pierce     = false;

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
            simple_monster_message(*mons, " fires!");
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
    if ((mons->foe == MHITNOT || !mons->can_see(*agent)
         || (!invalid_monster_index(mons->foe)
             && !agent->can_see(menv[mons->foe])))
        && !mons->props.exists("tracking"))
    {
        mons->foe = agent->mindex();
    }

    return used;
}

spret_type cast_fulminating_prism(actor* caster, int pow,
                                  const coord_def& where, bool fail)
{
    if (grid_distance(where, caster->pos())
        > spell_range(SPELL_FULMINANT_PRISM, pow))
    {
        if (caster->is_player())
			mpr("너무 멀리 떨어져있다.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(where))
    {
        if (caster->is_player())
			mpr("딱딱한 물체 내부에 시전할 수는 없다!");
        return SPRET_ABORT;
    }

    actor* victim = monster_at(where);
    if (victim)
    {
        if (caster->can_see(*victim))
        {
            if (caster->is_player())
				mpr("당신은 생물 위에 프리즘을 설치할 수는 없다.");
            return SPRET_ABORT;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        if (caster->is_player()
            || (you.can_see(*caster) && you.see_cell(where)))
        {
            if (you.can_see(*victim))
            {
                mprf("%s %s.", victim->name(DESC_THE).c_str(),
                               victim->conj_verb("twitch").c_str());
            }
            else
                canned_msg(MSG_GHOSTLY_OUTLINE);
        }
        return SPRET_SUCCESS;      // Don't give free detection!
    }

    fail_check();

    int hd = div_rand_round(pow, 10);

    mgen_data prism_data = mgen_data(MONS_FULMINANT_PRISM,
                                     caster->is_player()
                                     ? BEH_FRIENDLY
                                     : SAME_ATTITUDE(caster->as_monster()),
                                     where, MHITNOT, MG_FORCE_PLACE);
    prism_data.set_summoned(caster, 0, SPELL_FULMINANT_PRISM);
    prism_data.hd = hd;
    monster *prism = create_monster(prism_data);

    if (prism)
    {
        if (caster->observable())
        {
            mprf("%s %s a prism of explosive energy!",
                 caster->name(DESC_THE).c_str(),
                 caster->conj_verb("conjure").c_str());
        }
        else if (you.can_see(*prism))
            mprf("A prism of explosive energy appears from nowhere!");
    }
    else if (you.can_see(*caster))
        canned_msg(MSG_NOTHING_HAPPENS);

    return SPRET_SUCCESS;
}

monster* find_spectral_weapon(const actor* agent)
{
    if (agent->props.exists("spectral_weapon"))
        return monster_by_mid(agent->props["spectral_weapon"].get_int());
    else
        return nullptr;
}

bool weapon_can_be_spectral(const item_def *wpn)
{
    return wpn && is_weapon(*wpn) && !is_range_weapon(*wpn)
        && !is_special_unrandom_artefact(*wpn);
}

spret_type cast_spectral_weapon(actor *agent, int pow, god_type god, bool fail)
{
    ASSERT(agent);

    const int dur = min(2 + random2(1 + div_rand_round(pow, 25)), 4);
    item_def* wpn = agent->weapon();

    // If the wielded weapon should not be cloned, abort
    if (!weapon_can_be_spectral(wpn))
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
                mpr(you.hands_act("휘익", "."));
        }

        return SPRET_ABORT;
    }

    fail_check();

    // Remove any existing spectral weapons. Only one should be alive at any
    // given time.
    monster *old_mons = find_spectral_weapon(agent);
    if (old_mons)
        end_spectral_weapon(old_mons, false);

    mgen_data mg(MONS_SPECTRAL_WEAPON,
                 agent->is_player() ? BEH_FRIENDLY
                                    : SAME_ATTITUDE(agent->as_monster()),
                 agent->pos(),
                 agent->mindex());
    mg.set_summoned(agent, dur, SPELL_SPECTRAL_WEAPON, god);
    mg.props[TUKIMA_WEAPON] = *wpn;
    mg.props[TUKIMA_POWER] = pow;

    monster *mons = create_monster(mg);
    if (!mons)
    {
        //if (agent->is_player())
            canned_msg(MSG_NOTHING_HAPPENS);

        return SPRET_SUCCESS;
    }

    if (agent->is_player())
		mpr("당신은 무기의 영혼을 이끌어냈다!");
    else
    {
        if (you.can_see(*agent) && you.can_see(*mons))
        {
            string buf = " draws out ";
            buf += agent->pronoun(PRONOUN_POSSESSIVE);
            buf += " weapon's spirit!";
            simple_monster_message(*agent->as_monster(), buf.c_str());
        }
        else if (you.can_see(*mons))
            simple_monster_message(*mons, " appears!");

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
        if (you.can_see(*mons))
        {
            simple_monster_message(*mons, " fades away.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        }
        else if (owner && owner->is_player())
			mpr("당신과 혼령 무기 간의 결속이 약해진 것이 느껴졌다.");
    }

    if (!killed)
        monster_die(*mons, KILL_RESET, NON_MONSTER);
}

bool trigger_spectral_weapon(actor* agent, const actor* target)
{
    monster *spectral_weapon = find_spectral_weapon(agent);

    // Don't try to attack with a nonexistent spectral weapon
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

static void _setup_infestation(bolt &beam, int pow)
{
    beam.name         = "infestation";
    beam.aux_source   = "infestation";
    beam.flavour      = BEAM_INFESTATION;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour       = GREEN;
    beam.source_id    = MID_PLAYER;
    beam.thrower      = KILL_YOU;
    beam.is_explosion = true;
    beam.ex_size      = 2;
    beam.ench_power   = pow;
    beam.origin_spell = SPELL_INFESTATION;
}

spret_type cast_infestation(int pow, bolt &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_SOMETHING_IN_WAY);
        return SPRET_ABORT;
    }

    fail_check();

    _setup_infestation(beam, pow);
	mpr("당신은 풍뎅이 떼를 불러냈다!");
    beam.explode();

    return SPRET_SUCCESS;
}

struct summon_cap
{
    int type_cap;
    int timeout;
};

// spell type, cap, timeout
static const map<spell_type, summon_cap> summonsdata =
{
    // Beasts
    { SPELL_SUMMON_BUTTERFLIES,         { 8, 5 } },
    { SPELL_SUMMON_SMALL_MAMMAL,        { 4, 2 } },
    { SPELL_CALL_CANINE_FAMILIAR,       { 1, 2 } },
    { SPELL_SUMMON_ICE_BEAST,           { 3, 3 } },
    { SPELL_SUMMON_HYDRA,               { 3, 2 } },
    { SPELL_SUMMON_MANA_VIPER,          { 2, 2 } },
    // Demons
    { SPELL_CALL_IMP,                   { 3, 3 } },
    { SPELL_SUMMON_DEMON,               { 3, 2 } },
    { SPELL_SUMMON_GREATER_DEMON,       { 3, 2 } },
    // General monsters
    { SPELL_MONSTROUS_MENAGERIE,        { 3, 2 } },
    { SPELL_SUMMON_HORRIBLE_THINGS,     { 8, 8 } },
    { SPELL_SHADOW_CREATURES,           { 4, 2 } },
    { SPELL_SUMMON_LIGHTNING_SPIRE,     { 1, 2 } },
    { SPELL_SUMMON_GUARDIAN_GOLEM,      { 1, 2 } },
    { SPELL_SPELLFORGED_SERVITOR,       { 1, 2 } },
    // Monster spells
    { SPELL_SUMMON_UFETUBUS,            { 8, 2 } },
    { SPELL_SUMMON_HELL_BEAST,          { 8, 2 } },
    { SPELL_SUMMON_UNDEAD,              { 8, 2 } },
    { SPELL_SUMMON_DRAKES,              { 4, 2 } },
    { SPELL_SUMMON_MUSHROOMS,           { 8, 2 } },
    { SPELL_SUMMON_EYEBALLS,            { 4, 2 } },
    { SPELL_WATER_ELEMENTALS,           { 3, 2 } },
    { SPELL_FIRE_ELEMENTALS,            { 3, 2 } },
    { SPELL_EARTH_ELEMENTALS,           { 3, 2 } },
    { SPELL_AIR_ELEMENTALS,             { 3, 2 } },
#if TAG_MAJOR_VERSION == 34
    { SPELL_IRON_ELEMENTALS,            { 3, 2 } },
#endif
    { SPELL_SUMMON_SPECTRAL_ORCS,       { 3, 2 } },
    { SPELL_FIRE_SUMMON,                { 4, 2 } },
    { SPELL_SUMMON_MINOR_DEMON,         { 3, 3 } },
    { SPELL_CALL_LOST_SOUL,             { 3, 2 } },
    { SPELL_SUMMON_VERMIN,              { 5, 2 } },
    { SPELL_FORCEFUL_INVITATION,        { 3, 1 } },
    { SPELL_PLANEREND,                  { 6, 1 } },
    { SPELL_SUMMON_DRAGON,              { 4, 8 } },
    { SPELL_PHANTOM_MIRROR,             { 4, 1 } },
    { SPELL_FAKE_MARA_SUMMON,           { 2, 1 } },
    { SPELL_SUMMON_EMPEROR_SCORPIONS,   { 6, 2 } },
    { SPELL_SUMMON_SCARABS,             { 8, 1 } },
    { SPELL_SUMMON_HOLIES,              { 4, 2 } },
    { SPELL_SUMMON_EXECUTIONERS,        { 3, 1 } },
    { SPELL_AWAKEN_EARTH,               { 9, 2 } },
    { SPELL_GREATER_SERVANT_MAKHLEB,    { 1, 2 } },
};

bool summons_are_capped(spell_type spell)
{
    ASSERT_RANGE(spell, 0, NUM_SPELLS);
    return summonsdata.count(spell);
}

int summons_limit(spell_type spell)
{
    const summon_cap *cap = map_find(summonsdata, spell);
    return cap ? cap->type_cap : 0;
}

static bool _spell_has_variable_cap(spell_type spell)
{
    return spell == SPELL_SHADOW_CREATURES
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
    const summon_cap *cap = map_find(summonsdata, spell);
    if (!cap) // summons aren't capped
        return;

    int max_this_time = cap->type_cap;

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
        _expire_capped_summon(oldest_summon, cap->timeout * 5, true);
}

int count_summons(const actor *summoner, spell_type spell)
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (summoner == *mi)
            continue;

        int stype    = 0;
        const bool summoned = mi->is_summoned(nullptr, &stype);
        if (summoned && stype == spell && summoner->mid == mi->summoner
            && mons_aligned(summoner, *mi))
        {
            count++;
        }
    }

    return count;
}

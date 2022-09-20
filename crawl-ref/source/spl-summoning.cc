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
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "god-conduct.h"
#include "god-item.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h" // MON_SPELL_WIZARD
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-movetarget.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "place.h" // absdungeon_depth
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-util.h"
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

// combine with MG_AUTOFOE
static int _auto_autofoe(const actor *caster)
{
    return (!caster || caster->is_player())
        ? int{MHITYOU}
        : caster->as_monster()->foe;
}

static mgen_data _summon_data(const actor &caster, monster_type mtyp,
                              int dur, god_type god, spell_type spell)
{
    return mgen_data(mtyp, BEH_COPY, caster.pos(),
                     _auto_autofoe(&caster),
                     MG_AUTOFOE)
                     .set_summoned(&caster, dur, spell, god);
}

static mgen_data _pal_data(monster_type pal, int dur, god_type god,
                           spell_type spell)
{
    return _summon_data(you, pal, dur, god, spell);
}

spret cast_summon_small_mammal(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt())
        return spret::abort;

    fail_check();

    monster_type mon = MONS_PROGRAM_BUG;

    if (x_chance_in_y(10, pow + 1))
        mon = random_choose(MONS_BAT, MONS_RAT);
    else
        mon = MONS_QUOKKA;

    if (!create_monster(_pal_data(mon, 3, god, SPELL_SUMMON_SMALL_MAMMAL)))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_call_canine_familiar(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt())
        return spret::abort;

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

    return spret::success;
}

spret cast_summon_cactus(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data mg = _pal_data(MONS_CACTUS_GIANT, 3, god, SPELL_SUMMON_CACTUS);
    mg.hp = hit_points(pow + 27, 1);
    if (!create_monster(mg))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_armour_spirit(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    const item_def *armour = you.slot_item(EQ_BODY_ARMOUR);
    if (armour == nullptr)
    {
        // I don't think we can ever reach this line, but let's be safe.
        mpr("You aren't wearing any armour!");
        return spret::abort;
    }

    int mitm_slot = get_mitm_slot(10);
    if (mitm_slot == NON_ITEM)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::abort;
    }

    fail_check();

    mgen_data mg = _pal_data(MONS_ANIMATED_ARMOUR, 2, god,
                             SPELL_ANIMATE_ARMOUR);
    mg.hd = 15 + div_rand_round(pow, 10);
    monster* spirit = create_monster(mg);
    if (!spirit)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    item_def &fake_armour = env.item[mitm_slot];
    fake_armour.clear();
    fake_armour.base_type = OBJ_ARMOUR;
    fake_armour.sub_type = armour->sub_type;
    fake_armour.quantity = 1;
    fake_armour.rnd = armour->rnd ? armour->rnd : 1; // unrands have no rnd; hackily add one
    fake_armour.flags |= ISFLAG_SUMMONED | ISFLAG_KNOW_PLUSES;
    item_set_appearance(fake_armour);

    spirit->pickup_item(fake_armour, false, true);

    return spret::success;
}



spret cast_summon_ice_beast(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();
    const int dur = min(2 + (random2(pow) / 4), 4);

    mgen_data ice_beast = _pal_data(MONS_ICE_BEAST, dur, god,
                                    SPELL_SUMMON_ICE_BEAST);
    ice_beast.hd = (3 + div_rand_round(pow, 13));

    if (create_monster(ice_beast))
        mpr("A chill wind blows around you.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_monstrous_menagerie(actor* caster, int pow, god_type god, bool fail)
{
    if (caster->is_player() && stop_summoning_prompt())
        return spret::abort;

    fail_check();
    monster_type type = MONS_PROGRAM_BUG;

    if (random2(pow) > 60 && coinflip())
        type = MONS_SPHINX;
    else
        type = coinflip() ? MONS_MANTICORE : MONS_LINDWURM;

    mgen_data mdata = _summon_data(*caster, type, 4, god,
                                   SPELL_MONSTROUS_MENAGERIE);
    if (caster->is_player())
        mdata.hd = get_monster_data(type)->HD + div_rand_round(pow - 50, 25);

    monster* beast = create_monster(mdata);
    if (!beast)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (you.can_see(*beast))
    {
        mprf("%s %s %s!", caster->name(DESC_THE).c_str(),
                          caster->conj_verb("summon").c_str(),
                          mons_type_name(type, DESC_A).c_str());
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_hydra(actor *caster, int pow, god_type god, bool fail)
{
    if (caster->is_player() && stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

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

    return spret::success;
}

static monster_type _choose_dragon_type(int pow, god_type /*god*/, bool player)
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
    if (player && god_hates_monster(mon))
        mon = random_choose(MONS_STORM_DRAGON, MONS_IRON_DRAGON);

    return mon;
}

spret cast_dragon_call(int pow, bool fail)
{
    // Quicksilver and storm dragons don't have rPois, but that's fine.
    if (stop_summoning_prompt(MR_RES_POISON, "call dragons"))
        return spret::abort;

    fail_check();

    mpr("You call out to the draconic realm, and the dragon horde roars back!");
    noisy(spell_effect_noise(SPELL_DRAGON_CALL), you.pos());

    you.duration[DUR_DRAGON_CALL] = (15 + pow / 5 + random2(15)) * BASELINE_DELAY;
    you.props[DRAGON_CALL_POWER_KEY].get_int() = pow;

    return spret::success;
}

static void _place_dragon()
{
    const int pow = you.props[DRAGON_CALL_POWER_KEY].get_int();
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
            if (monster_habitable_grid(MONS_FIRE_DRAGON, env.grid(*ai))
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
            mpr("A dragon tries to answer your call, but you don't have enough "
                "magical power!");
            return;
        }

        const coord_def pos = spots[random2(spots.size())];
        monster *dragon = create_monster(
            mgen_data(mon, BEH_COPY, pos, MHITYOU, MG_FORCE_PLACE | MG_AUTOFOE)
            .set_summoned(&you, 2, SPELL_DRAGON_CALL));
        if (!dragon)
            continue;

        pay_mp(mp_cost);
        if (you.see_cell(dragon->pos()))
            mpr("A dragon arrives to answer your call!");
        finalize_mp_cost();

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
                MONS_BONE_DRAGON, MONS_REAPER, MONS_TORMENTOR, MONS_TZITZIMITL,
                MONS_PUTRID_MOUTH
        );
        vector<coord_def> spots;
        for (adjacent_iterator ai(target->pos()); ai; ++ai)
        {
            if (monster_habitable_grid(howlcalled, env.grid(*ai))
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

spret cast_summon_dragon(actor *caster, int pow, god_type god, bool fail)
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
                mpr("A dragon appears.");
            success = true;
        }
    }

    if (!success && caster->is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_mana_viper(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data viper = _pal_data(MONS_MANA_VIPER, 2, god,
                                SPELL_SUMMON_MANA_VIPER);
    viper.hd = (5 + div_rand_round(pow, 12));

    // Don't scale hp at the same time as their antimagic power
    viper.hp = hit_points(495); // avg 50

    if (create_monster(viper))
        mpr("A mana viper appears with a sibilant hiss.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
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
                 _auto_autofoe(caster),
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
        mpr("You are momentarily dazzled by a brilliant light.");

    return true;
}

/**
 * Essentially a macro to allow for a generic fail pattern to avoid leaking
 * information about invisible enemies. (Not implemented as a macro because I
 * find they create unreadable code.)
 *
 * @return spret::success
 **/
static bool _fail_tukimas()
{
    mprf("You can't see a target there!");
    return false; // Waste the turn - no anti-invis tech
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
           && !target.is_player()
           && !is_special_unrandom_artefact(*wpn)
           && !mons_class_is_animated_weapon(target.type)
           // XX use god_protects here. But, need to know the caster too...
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
    const item_def* wpn = target->weapon();
    bool can_see_target = target->visible_to(&you);

    // See if the wielded item is appropriate.
    if (!wpn)
    {
        if (!can_see_target)
            return _fail_tukimas();

        // FIXME: maybe move hands_act to class actor?
        bool plural = true;
        const string hand = target->hand_name(true, &plural);

        mprf("%s %s %s.",
             apostrophise(target->name(DESC_THE)).c_str(),
             hand.c_str(), conjugate_verb("twitch", plural).c_str());

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
                 wpn->name(DESC_THE).c_str(),
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
 * @param target            The spell's target.
 **/
static void _animate_weapon(int pow, actor* target)
{
    item_def * const wpn = target->weapon();
    ASSERT(wpn);
    // If sac love, the weapon will go after you, not the target.
    const bool hostile = you.allies_forbidden();
    const int dur = min(2 + (random2(pow) / 5), 6);

    mgen_data mg(MONS_DANCING_WEAPON,
                 hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                 target->pos(),
                 hostile ? MHITYOU : target->mindex(),
                 hostile ? MG_NONE : MG_FORCE_BEH);
    mg.set_summoned(&you, dur, SPELL_TUKIMAS_DANCE);
    mg.props[TUKIMA_WEAPON] = *wpn;
    mg.props[TUKIMA_POWER] = pow;

    monster * const mons = create_monster(mg);

    if (!mons)
    {
        mprf("%s twitches for a moment.", wpn->name(DESC_THE).c_str());
        return;
    }

    // Don't haunt yourself under sac love.
    if (!hostile)
    {
        mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target,
                                   INFINITE_DURATION));
        mons->foe = target->mindex();
    }

    // We are successful. Unwield the weapon, removing any wield effects.
    mprf("%s dances into the air!", wpn->name(DESC_THE).c_str());

    monster * const montarget = target->as_monster();
    const int primary_weap = montarget->inv[MSLOT_WEAPON];
    const mon_inv_type wp_slot = (primary_weap != NON_ITEM
                                  && &env.item[primary_weap] == wpn) ?
                                     MSLOT_WEAPON : MSLOT_ALT_WEAPON;
    ASSERT(montarget->inv[wp_slot] != NON_ITEM);
    ASSERT(&env.item[montarget->inv[wp_slot]] == wpn);

    montarget->unequip(*(montarget->mslot_item(wp_slot)), false, true);
    montarget->inv[wp_slot] = NON_ITEM;

    // Also steal ammo for launchers.
    if (is_range_weapon(*wpn))
    {
        const int ammo = montarget->inv[MSLOT_MISSILE];
        if (ammo != NON_ITEM)
        {
            ASSERT(mons->inv[MSLOT_MISSILE] == NON_ITEM);
            mons->inv[MSLOT_MISSILE] = ammo;
            montarget->inv[MSLOT_MISSILE] = NON_ITEM;
            env.item[ammo].set_holding_monster(*mons);
        }
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

/// When the player conjures ball lightning with the given spellpower, what
/// HD will the resulting lightning have?
int ball_lightning_hd(int pow, bool random)
{
    if (random)
        return max(1, div_rand_round(pow, 6) - 6);
    return max(1, pow / 6 - 6);
}

int mons_ball_lightning_hd(int pow, bool random)
{
    // We love players, don't we? Let's be nice.
    return ball_lightning_hd(pow, random) / 2;
}

spret cast_conjure_ball_lightning(int pow, god_type god, bool fail)
{
    fail_check();
    bool success = false;

    mgen_data cbl =_pal_data(MONS_BALL_LIGHTNING, 0, god,
                             SPELL_CONJURE_BALL_LIGHTNING);
    cbl.hd = ball_lightning_hd(pow);

    for (int i = 0; i < 3; ++i)
    {
        if (monster *ball = create_monster(cbl))
        {
            success = true;
            ball->add_ench(ENCH_SHORT_LIVED);

            // Avoid ball lightnings without targets always moving towards (0,0)
            if (!(ball->get_foe() && ball->get_foe()->is_monster()))
                set_random_target(ball);
        }
    }

    if (success)
        mpr("You create some ball lightning!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_lightning_spire(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data spire = _pal_data(MONS_LIGHTNING_SPIRE, 2, god,
                                SPELL_SUMMON_LIGHTNING_SPIRE);
    spire.hd = max(1, div_rand_round(pow, 10));

    monster* mons = create_monster(spire);

    if (mons && !silenced(mons->pos()))
        mpr("An electric hum fills the air.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret cast_summon_guardian_golem(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

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

        mpr("A guardian golem appears, shielding your allies.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

/**
 * Choose a type of imp to summon with Call Imp.
 *
 * @return      An appropriate imp type.
 */
static monster_type _get_imp_type()
{
    if (x_chance_in_y(5, 18))
        return MONS_WHITE_IMP;

    // 3/13 * 13/18 = 1/6 chance of one of these two.
    if (x_chance_in_y(3, 13))
        return one_chance_in(3) ? MONS_IRON_IMP : MONS_SHADOW_IMP;

    // 5/9 chance of getting, regrettably, a crimson imp.
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
 * @return      spret::fail if fail is true; spret::success otherwise.
 */
spret cast_call_imp(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    const monster_type imp_type = _get_imp_type();

    const int dur = min(2 + (random2(pow) / 4), 6);

    mgen_data imp_data = _pal_data(imp_type, dur, god, SPELL_CALL_IMP);
    if (monster *imp = create_monster(imp_data))
    {
        mpr(_imp_summon_messages[imp_type]);
        _monster_greeting(imp, "_friendly_imp_greeting");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  monster_type mon, int dur, bool friendly,
                                  bool charmed)
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

        mpr("A demon appears!");

        if (!friendly)
        {
            mpr(charmed ? "You don't feel so good about this..."
                        : "It doesn't seem very happy.");
        }
        else if (mon == MONS_CRIMSON_IMP || mon == MONS_WHITE_IMP
                || mon == MONS_IRON_IMP || mon == MONS_SHADOW_IMP)
        {
            _monster_greeting(demon, "_friendly_imp_greeting");
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
            demon->props[CHARMED_DEMON_KEY].get_bool() = true;
        }
    }

    return success;
}

static bool _summon_common_demon(int pow, god_type god, int spell)
{
    const int chance = 70 - (pow / 3);
    monster_type type = MONS_PROGRAM_BUG;

    if (x_chance_in_y(chance, 100))
        type = random_demon_by_tier(4);
    else
        type = random_demon_by_tier(3);

    return _summon_demon_wrapper(pow, god, spell, type,
                                 min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false);
}

bool summon_demon_type(monster_type mon, int pow, god_type god,
                       int spell, bool friendly)
{
    return _summon_demon_wrapper(pow, god, spell, mon,
                                 min(2 + (random2(pow) / 4), 6),
                                 friendly, false);
}

spret cast_summon_demon(int pow)
{
    // Don't prompt here, since this is invoked automatically by the
    // obsidian axe. The player shouldn't have control.

    mpr("You open a gate to Pandemonium!");

    if (!_summon_common_demon(pow, GOD_NO_GOD, SPELL_SUMMON_DEMON))
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret summon_butterflies()
{
    // Just fizzle instead of creating hostile butterflies.
    if (you.allies_forbidden())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    const string reason = stop_summoning_reason(MR_NO_FLAGS, M_FLIES);
    if (reason != "")
    {
        string prompt = make_stringf("Really summon butterflies while emitting a %s?",
                                     reason.c_str());

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    // XXX: dedup with Xom, or change number?

    // place some in a tight cluster, distance 2. Max 24 squares, so this is
    // always at least 2/3 density.
    const int how_many_inner = random_range(16, 22);
    bool success = false;
    for (int i = 0; i < how_many_inner; ++i)
    {
        mgen_data butterfly(MONS_BUTTERFLY, BEH_FRIENDLY, you.pos(), MHITYOU,
                            MG_AUTOFOE);
        butterfly.set_summoned(&you, 3, MON_SUMM_BUTTERFLIES);

        if (create_monster(butterfly))
            success = true;
    }
    // place another set more sparsely. These will try to find a placement
    // within range 3 of the player. If that place is already filled, they will
    // go as far as 2 from that original spot. This can backfill the inner
    // zone.
    const int how_many_outer = random_range(12, 28);
    for (int i = 0; i < how_many_outer; ++i)
    {
        coord_def pos(-1,-1);
        if (!find_habitable_spot_near(you.pos(), MONS_BUTTERFLY, 3, false, pos))
            break;
        mgen_data butterfly(MONS_BUTTERFLY, BEH_FRIENDLY, pos, MHITYOU,
                            MG_AUTOFOE);
        butterfly.set_summoned(&you, 3, MON_SUMM_BUTTERFLIES);

        if (create_monster(butterfly))
            success = true;
    }


    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (silenced(you.pos()))
        mpr("The fluttering of tiny wings stirs the air.");
    else
        mpr("You hear the tinkle of a tiny bell.");

    return spret::success;
}

spret summon_shadow_creatures()
{
    // Hard to predict what resistances might come from this.
    if (stop_summoning_prompt())
        return spret::abort;

    mpr("Wisps of shadow whirl around you...");

    int num = roll_dice(2, 2);
    int num_created = 0;

    for (int i = 0; i < num; ++i)
    {
        if (monster *mons = create_monster(
            mgen_data(RANDOM_COMPATIBLE_MONSTER, BEH_FRIENDLY, you.pos(),
                      MHITYOU, MG_FORCE_BEH | MG_AUTOFOE | MG_NO_OOD)
                      // This duration is only used for band members.
                      .set_summoned(&you, 2, MON_SUMM_SCROLL)
                      .set_place(level_id::current()),
            false))
        {
            // Choose a new duration based on HD.
            int x = max(mons->get_experience_level() - 3, 1);
            int d = min(4, 1 + div_rand_round(17, x));
            mon_enchant me = mon_enchant(ENCH_ABJ, d);
            me.set_duration(mons, &me);
            mons->update_ench(me);

            // Set summon ID, to share summon cap with its band members
            mons->props[SUMMON_ID_KEY].get_int() = mons->mid;

            // Remove any band members that would turn hostile, and link their
            // summon IDs
            for (monster_iterator mi; mi; ++mi)
            {
                if (testbits(mi->flags, MF_BAND_MEMBER)
                    && (mid_t) mi->props[BAND_LEADER_KEY].get_int() == mons->mid)
                {
                    if (god_hates_monster(**mi))
                        monster_die(**mi, KILL_RESET, NON_MONSTER);

                    mi->props[SUMMON_ID_KEY].get_int() = mons->mid;
                }
            }

            num_created++;
        }
    }

    if (!num_created)
        mpr("The shadows disperse without effect.");

    return spret::success;
}

bool can_cast_malign_gateway()
{
    timeout_malign_gateways(0);

    return count_malign_gateways() < 1;
}

coord_def find_gateway_location(actor* caster)
{
    vector<coord_def> points;

    for (coord_def delta : Compass)
    {
        coord_def test = coord_def(-1, -1);

        for (int t = 0; t < 11; t++)
        {
            test = caster->pos() + (delta * (2+t));
            if (!in_bounds(test) || !feat_is_malign_gateway_suitable(env.grid(test))
                || actor_at(test)
                || count_neighbours_with_func(test, &feat_is_solid) != 0
                || !caster->see_cell_no_trans(test))
            {
                continue;
            }

            points.push_back(test);
        }
    }

    if (points.empty())
        return coord_def(0, 0);

    return points[random2(points.size())];
}

void create_malign_gateway(coord_def point, beh_type beh, string cause,
                           int pow, god_type god, bool is_player)
{
    const int malign_gateway_duration = BASELINE_DELAY * (random2(2) + 1);
    env.markers.add(new map_malign_gateway_marker(point,
                            malign_gateway_duration,
                            is_player,
                            is_player ? "" : cause,
                            beh,
                            god,
                            pow));
    env.markers.clear_need_activate();
    env.grid(point) = DNGN_MALIGN_GATEWAY;
    set_terrain_changed(point);

    noisy(spell_effect_noise(SPELL_MALIGN_GATEWAY), point);
    mprf(MSGCH_WARN, "The dungeon shakes, a horrible noise fills the air, "
                     "and a portal to some otherworldly place is opened!");
}

spret cast_malign_gateway(actor * caster, int pow, god_type god,
                          bool fail, bool test)
{
    if (!test && caster->is_player() && stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    coord_def point = find_gateway_location(caster);
    bool success = point != coord_def(0, 0);
    if (test)
        return success ? spret::success : spret::abort;

    bool is_player = caster->is_player();

    if (success)
    {
        fail_check();

        create_malign_gateway(
            point,
            is_player ? BEH_FRIENDLY
                      : attitude_creation_behavior(
                          caster->as_monster()->attitude),
            is_player ? ""
                      : caster->as_monster()->full_name(DESC_A),
            pow,
            god,
            is_player);

        return spret::success;
    }

    return spret::abort;
}

spret cast_summon_horrible_things(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

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

    return spret::success;
}

static bool _water_adjacent(coord_def p)
{
    for (orth_adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_water(env.grid(*ai)))
            return true;
    }

    return false;
}


// Is this area open enough to summon a forest?
static bool _can_summon_forest(actor &caster)
{
    for (adjacent_iterator ai(caster.pos(), false); ai; ++ai)
        if (count_neighbours_with_func(*ai, &feat_is_solid) == 0)
            return true;
    return false;
}

/**
 * Cast summon forest.
 *
 * @param caster The caster.
 * @param pow    The spell power.
 * @param god    The god of the summoned dryad (usually the caster's).
 * @param fail   Did this spell miscast? If true, abort the cast.
 * @return       spret::abort if a summoning area couldn't be found,
 *               spret::fail if one could be found but we miscast, and
 *               spret::success if the spell was successfully cast.
*/
spret cast_summon_forest(actor* caster, int pow, god_type god, bool fail, bool test)
{
    if (!_can_summon_forest(*caster))
        return spret::abort;

    if (test)
        return spret::success;

    const int duration = random_range(120 + pow, 200 + pow * 3 / 2);

    // Hm, should dryads have rPois?
    if (stop_summoning_prompt(MR_NO_FLAGS, "summon a forest"))
        return spret::abort;

    fail_check();
    // Replace some rock walls with trees, then scatter a smaller number
    // of trees on unoccupied floor (such that they do not break connectivity)
    for (distance_iterator di(caster->pos(), false, true,
                              LOS_DEFAULT_RANGE); di; ++di)
    {
        if ((feat_is_wall(env.grid(*di)) && !feat_is_permarock(env.grid(*di))
             && x_chance_in_y(pow, 150))
            || (env.grid(*di) == DNGN_FLOOR && x_chance_in_y(pow, 1250)
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
            if (env.grid(*di) == DNGN_FLOOR
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

    return spret::success;
}

monster_type pick_random_wraith()
{
    return random_choose_weighted(1, MONS_SHADOW_WRAITH,
                                  5, MONS_WRAITH,
                                  2, MONS_FREEZING_WRAITH,
                                  2, MONS_PHANTASMAL_WARRIOR);
}

spret cast_haunt(int pow, const coord_def& where, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON, "haunt your foes"))
        return spret::abort;

    monster* m = monster_at(where);

    if (m == nullptr)
    {
        fail_check();
        mpr("An evil force gathers, but it quickly dissipates.");
        return spret::success; // still losing a turn
    }
    else if (m->wont_attack())
    {
        mpr("You cannot haunt those who bear you no hostility.");
        return spret::abort;
    }

    int mi = m->mindex();
    ASSERT(!invalid_monster_index(mi));

    if (stop_attack_prompt(m, false, you.pos()))
        return spret::abort;

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
            mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, m, INFINITE_DURATION));
            mons->foe = mi;
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
        return spret::success;
    }

    return spret::success;
}

static spell_type servitor_spells[] =
{
    // primary spells
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_IOOD,
    SPELL_IRON_SHOT,
    SPELL_BOLT_OF_COLD, // left in for frederick
    SPELL_LIGHTNING_BOLT,
    SPELL_FIREBALL,
    SPELL_ARCJOLT,
    SPELL_STONE_ARROW,
    SPELL_LRD,
    SPELL_AIRSTRIKE,
    SPELL_FORCE_LANCE, // left in for frederick
    // less desirable
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,
    SPELL_STICKY_FLAME,
};

/**
 * Return the spell a player spellforged servitor would use, for the spell
 * description.
 *
 * @return spell_type  The spell a player servitor would use if cast now
 */
spell_type player_servitor_spell()
{
    for (const spell_type spell : servitor_spells)
        if (you.has_spell(spell) && raw_spell_fail(spell) < 50)
            return spell;
    return SPELL_NO_SPELL;
}

bool spell_servitorable(spell_type to_serve)
{
    for (const spell_type spell : servitor_spells)
        if (spell == to_serve)
            return true;
    return false;
}

/**
 * Initialize the given spellforged servitor's HD and spellset, based on the
 * caster's spellpower and castable attack spells.
 *
 * @param mon       The spellforged servitor to be initialized.
 * @param caster    The entity summoning the servitor; may be the player.
 * @param pow       The caster's spellpower.
 */
static void _init_servitor_monster(monster &mon, const actor& caster, int pow)
{
    const monster* caster_mon = caster.as_monster();

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

            // Player servitors take a single spell
            if (!caster_mon)
                break;
        }
    }

    // Fix up frequencies now that we know the total number of spell levels.
    const int base_freq = caster_mon ? 67 : 150;
    for (auto& slot : mon.spells)
    {
        slot.freq = max(1, div_rand_round(spell_difficulty(slot.spell)
                                          * base_freq,
                                          spell_levels));
    }
    mon.props[CUSTOM_SPELLS_KEY].get_bool() = true;
}

void init_servitor(monster* servitor, actor* caster, int pow)
{
    ASSERT(servitor); // XXX: change to monster &servitor
    ASSERT(caster); // XXX: change to actor &caster
    _init_servitor_monster(*servitor, *caster, pow);

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
    servitor->props[IDEAL_RANGE_KEY].get_int() = shortest_range;
}

spret cast_spellforged_servitor(int pow, god_type god, bool fail)
{
    if (stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    mgen_data mdata = _pal_data(MONS_SPELLFORGED_SERVITOR, 4, god,
                                SPELL_SPELLFORGED_SERVITOR);

    if (monster* mon = create_monster(mdata))
        init_servitor(mon, &you, pow);
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

monster* find_battlesphere(const actor* agent)
{
    if (agent->props.exists(BATTLESPHERE_KEY))
        return monster_by_mid(agent->props[BATTLESPHERE_KEY].get_int());
    else
        return nullptr;
}

static int _battlesphere_hd(int pow, bool random = true)
{
    if (random)
        return 1 + div_rand_round(pow, 11);
    return 1 + pow / 11;
}

static dice_def _battlesphere_damage(int hd)
{
    return dice_def(2, 5 + hd);
}

dice_def battlesphere_damage(int pow)
{
    return _battlesphere_damage(_battlesphere_hd(pow, false));
}

spret cast_battlesphere(actor* agent, int pow, god_type god, bool fail)
{
    if (agent->is_player() && stop_summoning_prompt(MR_RES_POISON))
        return spret::abort;

    fail_check();

    monster* battlesphere;
    if (agent->is_player() && (battlesphere = find_battlesphere(&you)))
    {
        bool recalled = false;
        if (!you.can_see(*battlesphere))
        {
            coord_def empty;
            if (find_habitable_spot_near(agent->pos(), MONS_BATTLESPHERE, 2,
                                         false, empty)
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
                      agent->pos(), agent->mindex());
        mg.set_summoned(agent, 0, SPELL_BATTLESPHERE, god);
        mg.hd = _battlesphere_hd(pow);
        battlesphere = create_monster(mg);

        if (battlesphere)
        {
            int dur = min((7 + roll_dice(2, pow)) * 10, 500);
            battlesphere->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 1, 0, dur));
            battlesphere->summoner = agent->mid;
            agent->props[BATTLESPHERE_KEY].get_int() = battlesphere->mid;

            if (agent->is_player())
                mpr("You conjure a globe of magical energy.");
            else
            {
                if (you.can_see(*agent) && you.can_see(*battlesphere))
                {
                    simple_monster_message(*agent->as_monster(),
                                           " conjures a globe of magical energy!");
                }
                else if (you.can_see(*battlesphere))
                    simple_monster_message(*battlesphere, " appears!");
                battlesphere->props[BAND_LEADER_KEY].get_int() = agent->mid;
            }
            battlesphere->battlecharge = 4 + random2(pow + 10) / 10;
            battlesphere->foe = agent->mindex();
            battlesphere->target = agent->pos();
        }
        else if (agent->is_player() || you.can_see(*agent))
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return spret::success;
}

void end_battlesphere(monster* mons, bool killed)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor* agent = actor_by_mid(mons->summoner);
    if (agent)
        agent->props.erase(BATTLESPHERE_KEY);

    if (!killed)
    {
        if (agent && agent->is_player())
        {
            if (you.can_see(*mons))
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
        else if (you.can_see(*mons))
            simple_monster_message(*mons, " dissipates.");

        if (!cell_is_solid(mons->pos()))
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);

        monster_die(*mons, KILL_RESET, NON_MONSTER);
    }
}

bool battlesphere_can_mirror(spell_type spell)
{
    return spell_typematch(spell, spschool::conjuration)
           && spell != SPELL_BATTLESPHERE
           && spell != SPELL_SPELLFORGED_SERVITOR;
}

bool aim_battlesphere(actor* agent, spell_type spell)
{
    //Is this spell something that will trigger the battlesphere?
    if (battlesphere_can_mirror(spell))
    {
        monster* battlesphere = find_battlesphere(agent);

        // If we've somehow gotten separated from the battlesphere (ie:
        // abyss level teleport), bail out and cancel the battlesphere bond
        if (!battlesphere)
        {
            agent->props.erase(BATTLESPHERE_KEY);
            return false;
        }

        // In case the battlesphere was in the middle of a (failed)
        // target-seeking action, cancel it so that it can focus on a new
        // target
        reset_battlesphere(battlesphere);
        battlesphere->props.erase(MON_FOE_KEY);

        // Pick a random baddie in LOS
        vector<actor *> targets;
        for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
        {
            if (battlesphere->can_see(**ai)
                && !mons_aligned(agent, *ai)
                && (ai->is_player() || !mons_is_firewood(*ai->as_monster())))
            {
                targets.push_back(*ai);
            }
        }

        if (targets.empty())
            return false;

        const actor * target = *random_iterator(targets);
        battlesphere->foe = target->mindex();
        battlesphere->props[MON_FOE_KEY] = battlesphere->foe;
        battlesphere->props["ready"] = true;

        return true;
    }

    return false;
}

bool trigger_battlesphere(actor* agent)
{
    monster* battlesphere = find_battlesphere(agent);
    if (!battlesphere)
        return false;

    if (battlesphere->props.exists("ready"))
    {
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
        if (mons->props.exists(MON_FOE_KEY))
            mons->foe = mons->props[MON_FOE_KEY].get_int();
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
            if (mons->pos() == mons->props[TRACKING_TARGET_KEY].get_coord())
            {
                mons->props.erase("tracking");
                if (mons->props.exists(MON_FOE_KEY))
                    mons->foe = mons->props[MON_FOE_KEY].get_int();
                mons->behaviour = BEH_SEEK;
            }
            else // Currently tracking, but have not reached target pos
            {
                mons->target = mons->props[TRACKING_TARGET_KEY].get_coord();
                return false;
            }
        }
        else
        {
            // If the battlesphere forgot its foe (due to being out of los),
            // remind it
            if (mons->props.exists(MON_FOE_KEY))
                mons->foe = mons->props[MON_FOE_KEY].get_int();
        }

        // Set up the beam.
        bolt beam;
        beam.source_name = BATTLESPHERE_KEY;

        // If we are locked onto a foe, use its current position
        if (!invalid_monster_index(mons->foe) && env.mons[mons->foe].alive())
            beam.target = env.mons[mons->foe].pos();

        // Sanity check: if we have somehow ended up targeting ourselves, bail
        if (beam.target == mons->pos())
        {
            mprf(MSGCH_ERROR, "Battlesphere targeting itself? Fixing.");
            mons->props.erase("firing");
            mons->props.erase(MON_FOE_KEY);
            return false;
        }

        beam.name       = "barrage of energy";
        beam.range      = LOS_RADIUS;
        beam.hit        = AUTOMATIC_HIT;
        beam.damage     = _battlesphere_damage(mons->get_hit_dice());
        beam.glyph      = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.colour     = MAGENTA;
        beam.flavour    = BEAM_MMISSILE;
        beam.pierce     = false;

        // Fire tracer.
        fire_tracer(mons, beam);

        // Never fire if we would hurt the caster, and ensure that the beam
        // would hit at least SOMETHING, unless it was targeted at empty space
        // in the first place
        if (beam.friend_info.count == 0 && beam.foe_info.count > 0)
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
                    mons->props[MON_FOE_KEY] = mons->foe;
                    mons->props["tracking"] = true;
                    mons->foe = MHITNOT;
                    mons->props[TRACKING_TARGET_KEY] = *di;
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
             && !agent->can_see(env.mons[mons->foe])))
        && !mons->props.exists("tracking"))
    {
        mons->foe = agent->mindex();
    }

    return used;
}

int prism_hd(int pow, bool random)
{
    if (random)
        return div_rand_round(pow, 10);
    return pow / 10;
}

spret cast_fulminating_prism(actor* caster, int pow,
                                  const coord_def& where, bool fail)
{
    if (grid_distance(where, caster->pos())
        > spell_range(SPELL_FULMINANT_PRISM, pow))
    {
        if (caster->is_player())
            mpr("That's too far away.");
        return spret::abort;
    }

    if (cell_is_solid(where))
    {
        if (caster->is_player())
            mpr("You can't conjure that within a solid object!");
        return spret::abort;
    }

    actor* victim = monster_at(where);
    if (victim)
    {
        if (caster->can_see(*victim))
        {
            if (caster->is_player())
                mpr("You can't place the prism on a creature.");
            return spret::abort;
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
        return spret::success;      // Don't give free detection!
    }

    fail_check();

    const int hd = prism_hd(pow);

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

    return spret::success;
}

monster* find_spectral_weapon(const actor* agent)
{
    if (agent->props.exists(SPECTRAL_WEAPON_KEY))
        return monster_by_mid(agent->props[SPECTRAL_WEAPON_KEY].get_int());
    else
        return nullptr;
}

void end_spectral_weapon(monster* mons, bool killed, bool quiet)
{
    // Should only happen if you dismiss it in wizard mode, I think
    if (!mons)
        return;

    actor *owner = actor_by_mid(mons->summoner);

    if (owner)
        owner->props.erase(SPECTRAL_WEAPON_KEY);

    if (!quiet)
    {
        if (you.can_see(*mons))
        {
            simple_monster_message(*mons, " fades away.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        }
        else if (owner && owner->is_player())
            mpr("You feel your bond with your spectral weapon wane.");
    }

    if (!killed)
        monster_die(*mons, KILL_RESET, NON_MONSTER);
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

spret cast_infestation(int pow, bolt &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_SOMETHING_IN_WAY);
        return spret::abort;
    }

    fail_check();

    _setup_infestation(beam, pow);
    mpr("You call forth a plague of scarabs!");
    beam.explode();

    return spret::success;
}

struct summon_cap
{
    int player_cap;
    int monster_cap;
};

// spell type, player cap, monster cap
static const map<spell_type, summon_cap> summonsdata =
{
    // Player- and monster-castable spells
    { SPELL_SUMMON_SMALL_MAMMAL,      { 2, 4 } },
    { SPELL_CALL_CANINE_FAMILIAR,     { 1, 1 } },
    { SPELL_SUMMON_ICE_BEAST,         { 1, 3 } },
    { SPELL_SUMMON_HYDRA,             { 2, 3 } },
    { SPELL_SUMMON_MANA_VIPER,        { 1, 3 } },
    { SPELL_CALL_IMP,                 { 1, 3 } },
    { SPELL_MONSTROUS_MENAGERIE,      { 2, 3 } },
    { SPELL_SUMMON_HORRIBLE_THINGS,   { 8, 8 } },
    { SPELL_SUMMON_LIGHTNING_SPIRE,   { 1, 1 } },
    { SPELL_SUMMON_GUARDIAN_GOLEM,    { 1, 1 } },
    { SPELL_SPELLFORGED_SERVITOR,     { 1, 1 } },
    { SPELL_ANIMATE_ARMOUR,           { 1, 1 } },
    { SPELL_HAUNT,                    { 8, 8 } },
    { SPELL_SUMMON_CACTUS,            { 1, 1 } },
    // Monster-only spells
    { SPELL_SHADOW_CREATURES,         { 0, 4 } },
    { SPELL_SUMMON_SPIDERS,           { 0, 5 } },
    { SPELL_SUMMON_UFETUBUS,          { 0, 8 } },
    { SPELL_SUMMON_HELL_BEAST,        { 0, 8 } },
    { SPELL_SUMMON_UNDEAD,            { 0, 8 } },
    { SPELL_SUMMON_DRAKES,            { 0, 4 } },
    { SPELL_SUMMON_MUSHROOMS,         { 0, 8 } },
    { SPELL_SUMMON_EYEBALLS,          { 0, 4 } },
    { SPELL_WATER_ELEMENTALS,         { 0, 3 } },
    { SPELL_FIRE_ELEMENTALS,          { 0, 3 } },
    { SPELL_EARTH_ELEMENTALS,         { 0, 3 } },
    { SPELL_AIR_ELEMENTALS,           { 0, 3 } },
    { SPELL_SUMMON_SPECTRAL_ORCS,     { 0, 3 } },
    { SPELL_FIRE_SUMMON,              { 0, 4 } },
    { SPELL_SUMMON_MINOR_DEMON,       { 0, 3 } },
    { SPELL_CALL_LOST_SOUL,           { 0, 3 } },
    { SPELL_SUMMON_VERMIN,            { 0, 5 } },
    { SPELL_FORCEFUL_INVITATION,      { 0, 3 } },
    { SPELL_PLANEREND,                { 0, 6 } },
    { SPELL_SUMMON_DRAGON,            { 0, 4 } },
    { SPELL_PHANTOM_MIRROR,           { 0, 4 } },
    { SPELL_FAKE_MARA_SUMMON,         { 0, 2 } },
    { SPELL_SUMMON_EMPEROR_SCORPIONS, { 0, 6 } },
    { SPELL_SUMMON_SCARABS,           { 0, 8 } },
    { SPELL_SUMMON_HOLIES,            { 0, 4 } },
    { SPELL_SUMMON_EXECUTIONERS,      { 0, 3 } },
    { SPELL_AWAKEN_EARTH,             { 0, 9 } },
    { SPELL_GREATER_SERVANT_MAKHLEB,  { 0, 1 } },
    { SPELL_SUMMON_GREATER_DEMON,     { 0, 3 } },
    { SPELL_SUMMON_DEMON,             { 0, 3 } },
    { SPELL_SUMMON_TZITZIMITL,        { 0, 3 } },
    { SPELL_SUMMON_HELL_SENTINEL,     { 0, 3 } },
    { SPELL_CONJURE_LIVING_SPELLS,    { 0, 6 } },
    { SPELL_SHEZAS_DANCE,             { 0, 6 } },
};

bool summons_are_capped(spell_type spell)
{
    ASSERT_RANGE(spell, 0, NUM_SPELLS);
    return summonsdata.count(spell);
}

int summons_limit(spell_type spell, bool player)
{
    const summon_cap *cap = map_find(summonsdata, spell);
    if (!cap)
        return 0;
    else
        return player ? cap->player_cap : cap->monster_cap;
}

static bool _spell_has_variable_cap(spell_type spell)
{
    return spell == SPELL_SHADOW_CREATURES;
}

static void _expire_capped_summon(monster* mon, bool recurse)
{
    // Timeout the summon
    mon_enchant abj = mon->get_ench(ENCH_ABJ);
    abj.duration = 10;
    mon->update_ench(abj);
    // Mark our cap abjuration so we don't keep abjuring the same
    // one if creating multiple summons (also, should show a status light).
    mon->add_ench(ENCH_SUMMON_CAPPED);

    if (recurse && mon->props.exists(SUMMON_ID_KEY))
    {
        const int summon_id = mon->props[SUMMON_ID_KEY].get_int();
        for (monster_iterator mi; mi; ++mi)
        {
            // Summoner check should be technically unnecessary, but saves
            // scanning props for all monsters on the level.
            if (mi->summoner == mon->summoner
                && mi->props.exists(SUMMON_ID_KEY)
                && mi->props[SUMMON_ID_KEY].get_int() == summon_id
                && !mi->has_ench(ENCH_SUMMON_CAPPED))
            {
                _expire_capped_summon(*mi, false);
            }
        }
    }
}

// Call when a monster has been summoned, to manage this summoner's caps.
void summoned_monster(const monster *mons, const actor *caster,
                      spell_type spell)
{
    int cap = summons_limit(spell, caster->is_player());
    if (!cap) // summons aren't capped
        return;

    // Cap large abominations and tentacled monstrosities separately
    if (spell == SPELL_SUMMON_HORRIBLE_THINGS)
    {
        cap = (mons->type == MONS_ABOMINATION_LARGE ? cap * 3 / 4
                                                    : cap * 1 / 4);
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

            if (_spell_has_variable_cap(spell) && mi->props.exists(SUMMON_ID_KEY))
            {
                const int id = mi->props[SUMMON_ID_KEY].get_int();

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

    if (oldest_summon && count > cap)
        _expire_capped_summon(oldest_summon, true);
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

static bool _create_briar_patch(coord_def& target)
{
    mgen_data mgen = mgen_data(MONS_BRIAR_PATCH, BEH_FRIENDLY, target,
            MHITNOT, MG_FORCE_PLACE, GOD_FEDHAS);
    mgen.hd = mons_class_hit_dice(MONS_BRIAR_PATCH) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, min(2 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6),
            SPELL_NO_SPELL);

    if (create_monster(mgen))
    {
        mpr("A briar patch grows up from the ground.");
        return true;
    }

    return false;
}

vector<coord_def> find_briar_spaces(bool just_check)
{
    vector<coord_def> result;

    for (adjacent_iterator adj_it(you.pos()); adj_it; ++adj_it)
    {
        if (monster_habitable_grid(MONS_BRIAR_PATCH, env.grid(*adj_it))
            && (!actor_at(*adj_it)
                || just_check && !you.can_see(*actor_at(*adj_it))))
        {
            result.push_back(*adj_it);
        }
    }

    return result;
}

void fedhas_wall_of_briars()
{
    // How many adjacent open spaces are there?
    vector<coord_def> adjacent = find_briar_spaces();

    if (adjacent.empty())
    {
        mpr("Something you can't see blocks your briars from growing!");
        return;
    }

    int created_count = 0;
    for (auto p : adjacent)
    {
        if (_create_briar_patch(p))
            created_count++;
    }

    if (!created_count)
        canned_msg(MSG_NOTHING_HAPPENS);

    return;
}

static void _overgrow_wall(const coord_def &pos)
{
    const dungeon_feature_type feat = env.grid(pos);
    const string what = feature_description(feat, NUM_TRAPS, "", DESC_THE);

    if (monster_at(pos))
    {
        mprf("Something unseen blocks growth in %s.", what.c_str());
        return;
    }

    destroy_wall(pos);

    const monster_type mon = random_choose_weighted(4, MONS_OKLOB_SAPLING,
                                                    4, MONS_BURNING_BUSH,
                                                    4, MONS_WANDERING_MUSHROOM,
                                                    1, MONS_BALLISTOMYCETE,
                                                    1, MONS_OKLOB_PLANT);
    mgen_data mgen(mon, BEH_FRIENDLY, pos, MHITYOU, MG_FORCE_PLACE);
    mgen.hd = mons_class_hit_dice(mon) + you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6),
            SPELL_NO_SPELL);
    if (const monster* const plant = create_monster(mgen))
    {
        mprf("%s is torn apart as %s grows in its place.", what.c_str(),
                plant->name(DESC_A).c_str());
    }
    // XXX: Maybe try to make this revert the terrain if a monster isn't placed.
    else
        mprf("%s falls apart, but nothing grows.", what.c_str());
}

spret fedhas_overgrow(bool fail)
{
    targeter_overgrow tgt;
    direction_chooser_args args;
    args.hitfunc = &tgt;
    args.restricts = DIR_TARGET;
    args.mode = TARG_ANY;
    args.range = LOS_RADIUS;
    args.just_looking = false;
    args.needs_path = false;
    args.top_prompt = "Aiming: <white>Overgrow</white>";
    dist sdirect;
    direction(sdirect, args);
    if (!sdirect.isValid)
        return spret::abort;

    fail_check();

    for (auto site : tgt.affected_positions)
        _overgrow_wall(site);

    return spret::success;
}

spret fedhas_grow_ballistomycete(const coord_def& target, bool fail)
{
    if (grid_distance(target, you.pos()) > 2 || !in_bounds(target))
    {
        mpr("That's too far away.");
        return spret::abort;
    }

    if (!monster_habitable_grid(MONS_BALLISTOMYCETE, env.grid(target)))
    {
        mpr("You can't grow a ballistomycete there.");
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons)
    {
        if (you.can_see(*mons))
        {
            mpr("That space is already occupied.");
            return spret::abort;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see occupies that space!");
        return spret::success;
    }

    fail_check();

    mgen_data mgen(MONS_BALLISTOMYCETE, BEH_FRIENDLY, target, MHITYOU,
            MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);
    mgen.hd = mons_class_hit_dice(MONS_BALLISTOMYCETE) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6),
            SPELL_NO_SPELL);

    if (create_monster(mgen))
        mpr("A ballistomycete grows from the ground.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret fedhas_grow_oklob(const coord_def& target, bool fail)
{
    if (grid_distance(target, you.pos()) > 2 || !in_bounds(target))
    {
        mpr("That's too far away.");
        return spret::abort;
    }

    if (!monster_habitable_grid(MONS_OKLOB_PLANT, env.grid(target)))
    {
        mpr("You can't grow an oklob plant there.");
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons)
    {
        if (you.can_see(*mons))
        {
            mpr("That space is already occupied.");
            return spret::abort;
        }

        fail_check();

        // invisible monster
        mpr("Something you can't see is occupying that space!");
        return spret::success;
    }

    fail_check();

    mgen_data mgen(MONS_OKLOB_PLANT, BEH_FRIENDLY, target, MHITYOU,
            MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);
    mgen.hd = mons_class_hit_dice(MONS_OKLOB_PLANT) +
        you.skill_rdiv(SK_INVOCATIONS);
    mgen.set_summoned(&you, min(3 + you.skill_rdiv(SK_INVOCATIONS, 1, 5), 6),
            SPELL_NO_SPELL);

    if (create_monster(mgen))
        mpr("An oklob plant grows from the ground.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

void kiku_unearth_wretches()
{
    const int pow = you.skill(SK_NECROMANCY, 6);
    const int min_wretches = 2;
    const int max_wretches = min_wretches + div_rand_round(pow, 27); // 8 max
    const int wretches = random_range(min_wretches, max_wretches);
    bool created = false;
    for (int i = 0; i < wretches; ++i)
    {
        // choose a type
        const int typ_pow = you.skill(SK_NECROMANCY, 4);
        const int adjusted_power = min(typ_pow / 4, random2(random2(typ_pow)));
        const level_id lev(you.where_are_you, adjusted_power
                           - absdungeon_depth(you.where_are_you, 0));
        const monster_type mon_type = pick_local_corpsey_monster(lev);
        ASSERT(mons_class_can_be_zombified(mons_species(mon_type)));
        // place a monster
        mgen_data mg(mon_type,
                     BEH_HOSTILE, // so players don't have attack prompts
                     you.pos(),
                     MHITNOT);
        mg.extra_flags = MF_NO_REWARD // ?
                       | MF_NO_REGEN;
        mg.props[KIKU_WRETCH_KEY] = true;

        monster *mon = create_monster(mg);
        if (!mon)
            continue;

        created = true;
        mons_add_blame(mon, "unearthed by the player character");
        mon->hit_points = 1;
        mon->props[ALWAYS_CORPSE_KEY] = true;
        mon->props[KIKU_WRETCH_KEY] = true;

        // Die in 2-3 turns.
        mon->add_ench(mon_enchant(ENCH_SLOWLY_DYING, 1, nullptr,
                                   20 + random2(10)));
        mon->add_ench(mon_enchant(ENCH_PARALYSIS, 0, &you, 9999));
    }
    if (!created)
        simple_god_message(" has no space to call forth the wretched!");
    else
        simple_god_message(" calls piteous wretches from the earth!");
}

static bool _create_foxfire(const actor &agent, coord_def pos,
                            god_type god, int pow)
{
    const auto att = agent.is_player() ? BEH_FRIENDLY
                                       : SAME_ATTITUDE(agent.as_monster());
    mgen_data fox(MONS_FOXFIRE, att,
                  pos, MHITNOT, MG_FORCE_PLACE | MG_AUTOFOE);
    fox.set_summoned(&agent, 0, SPELL_FOXFIRE, god);
    fox.hd = pow;
    monster *foxfire;

    if (cell_is_solid(pos) || actor_at(pos))
        return false;

    foxfire = create_monster(fox);
    if (!foxfire)
        return false;

    foxfire->add_ench(ENCH_SHORT_LIVED);
    foxfire->steps_remaining = you.current_vision + 2;

    // Avoid foxfire without targets always moving towards (0,0)
    if (!foxfire->get_foe()
        || !foxfire->get_foe()->is_monster() && !agent.is_monster())
    {
        set_random_target(foxfire);
    }
    return true;
}

spret cast_foxfire(actor &agent, int pow, god_type god, bool fail)
{
    bool see_space = false;
    for (adjacent_iterator ai(agent.pos()); ai; ++ai)
    {
        if (cell_is_solid(*ai))
            continue;
        if (actor_at(*ai) && agent.can_see(*actor_at(*ai)))
            continue;
        see_space = true;
        break;
    }

    if (agent.is_player() && !see_space)
    {
        mpr("There is not enough space to conjure foxfire!");
        return spret::abort;
    }

    fail_check();

    int created = 0;

    for (fair_adjacent_iterator ai(agent.pos()); ai; ++ai)
    {
        if (!_create_foxfire(agent, *ai, god, pow))
            continue;
        ++created;
        if (created == 2)
            break;
    }

    if (created && you.see_cell(agent.pos()))
    {
        mprf("%s conjure%s some foxfire!",
             agent.name(DESC_THE).c_str(),
             agent.is_monster() ? "s" : "");
    }
    else if (agent.is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

spret foxfire_swarm()
{
    bool created = false;
    bool unknown_unseen = false;
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (_create_foxfire(you, *ri, GOD_NO_GOD, 10))
        {
            created = true;
            continue;
        }
        const actor* agent = actor_at(*ri);
        if (agent && !you.can_see(*agent))
            unknown_unseen = true;
    }
    if (created)
    {
        mpr("Flames leap up around you!");
        return spret::success;
    }
    if (!unknown_unseen)
    {
        mpr("There's no space to create foxfire here.");
        return spret::abort;
    }
    canned_msg(MSG_NOTHING_HAPPENS);
    return spret::fail; // don't spend piety, do spend a turn
}

bool summon_spider(const actor &agent, coord_def pos, god_type god,
                        spell_type spell, int pow)
{
    monster_type mon = random_choose_weighted(100, MONS_REDBACK,
                                              100, MONS_JUMPING_SPIDER,
                                               75, MONS_TARANTELLA,
                                               50, MONS_CULICIVORA,
                                               50, MONS_ORB_SPIDER,
                                               pow / 2, MONS_WOLF_SPIDER);

    monster *mons = create_monster(
            mgen_data(mon, BEH_COPY, pos, _auto_autofoe(&agent), MG_AUTOFOE)
                      .set_summoned(&agent, 3, spell, god));
    if (mons)
        return true;

    return false;
}

spret summon_spiders(actor &agent, int pow, god_type god, bool fail)
{
    // Can't happen at present, but why not check just to be sure.
    if (agent.is_player() && stop_summoning_prompt())
        return spret::abort;

    fail_check();

    int created = 0;

    for (int i = 0; i < 1 + div_rand_round(random2(pow), 80); i++)
    {
        if (summon_spider(agent, agent.pos(), god, SPELL_SUMMON_SPIDERS, pow))
            created++;
    }

    if (created && you.see_cell(agent.pos()))
    {
        mprf("%s %s %s!",
             agent.name(DESC_THE).c_str(),
             agent.conj_verb("summon").c_str(),
             created > 1 ? "spiders" : "a spider");
    }
    else if (agent.is_player())
        canned_msg(MSG_NOTHING_HAPPENS);

    return spret::success;
}

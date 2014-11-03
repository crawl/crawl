/**
 * @file
 * @brief Contains monster death functionality, including unique code.
**/

#include "AppHdr.h"

#include "mon-death.h"

#include "act-iter.h"
#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "butcher.h"
#include "cloud.h"
#include "cluautil.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "effects.h"
#include "env.h"
#include "fineff.h"
#include "food.h"
#include "godabil.h"
#include "godblessing.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "hints.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "notes.h"
#include "religion.h"
#include "rot.h"
#include "spl-damage.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "traps.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"

// Initialises a corpse item using the given monster and monster type.
// The monster pointer is optional; you may pass in NULL to bypass
// per-monster checks.
//
// force_corpse forces creation of the corpse item even if the monster
// would not otherwise merit a corpse.
monster_type fill_out_corpse(const monster* mons,
                             monster_type mtype,
                             item_def& corpse,
                             bool force_corpse)
{
    ASSERT(!invalid_monster_type(mtype));
    corpse.clear();

    int summon_type;
    if (mons && (mons->is_summoned(NULL, &summon_type)
                    || (mons->flags & (MF_BANISHED | MF_HARD_RESET))))
    {
        return MONS_NO_MONSTER;
    }

    monster_type corpse_class = mons_species(mtype);

    if (mons)
    {
        // If this was a corpse that was temporarily animated then turn the
        // monster back into a corpse.
        if (mons_class_is_zombified(mons->type)
            && (summon_type == SPELL_ANIMATE_DEAD
                || summon_type == SPELL_ANIMATE_SKELETON
                || summon_type == MON_SUMM_ANIMATE))
        {
            corpse_class = mons_zombie_base(mons);
        }

        if (mons
            && (mons_genus(mtype) == MONS_DRACONIAN
                || mons_genus(mtype) == MONS_DEMONSPAWN))
        {
            if (mons->type == MONS_TIAMAT)
                corpse_class = MONS_DRACONIAN;
            else
                corpse_class = draco_or_demonspawn_subspecies(mons);
        }

        if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            mtype = corpse_class = MONS_GLOWING_SHAPESHIFTER;
        else if (mons->has_ench(ENCH_SHAPESHIFTER))
            mtype = corpse_class = MONS_SHAPESHIFTER;
    }

    // Doesn't leave a corpse.
    if (!mons_class_can_leave_corpse(corpse_class) && !force_corpse)
        return MONS_NO_MONSTER;

    corpse.flags          = 0;
    corpse.base_type      = OBJ_CORPSES;
    corpse.mon_type       = corpse_class;
    corpse.butcher_amount = 0;    // butcher work done
    corpse.sub_type       = CORPSE_BODY;
    corpse.freshness      = FRESHEST_CORPSE;  // rot time
    corpse.quantity       = 1;
    corpse.orig_monnum    = mtype;

    if (mons)
    {
        corpse.props[MONSTER_HIT_DICE] = short(mons->get_experience_level());
        corpse.props[MONSTER_NUMBER]   = short(mons->number);
        // XXX: Appears to be a safe conversion?
        corpse.props[MONSTER_MID]      = int(mons->mid);
        if (mons->props.exists(NEVER_HIDE_KEY))
            corpse.props[NEVER_HIDE_KEY] = true;
    }

    const colour_t class_colour = mons_class_colour(corpse_class);
    if (class_colour == BLACK)
    {
        if (mons)
            corpse.props[FORCED_ITEM_COLOUR_KEY] = mons->colour;
        else
        {
            // [ds] Ick: no easy way to get a monster's colour
            // otherwise:
            monster m;
            m.type = mtype;
            define_monster(&m);
            corpse.props[FORCED_ITEM_COLOUR_KEY] = m.colour;
        }
    }

    if (mons && !mons->mname.empty() && !(mons->flags & MF_NAME_NOCORPSE))
    {
        corpse.props[CORPSE_NAME_KEY] = mons->mname;
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = mons->flags;
    }
    else if (mons_is_unique(mtype))
    {
        corpse.props[CORPSE_NAME_KEY] = mons_type_name(mtype, DESC_PLAIN);
        corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = 0;
    }

    return corpse_class;
}

bool explode_corpse(item_def& corpse, const coord_def& where)
{
    // Don't want chunks to show up behind the player.
    los_def ld(where, opc_no_actor);

    if (mons_class_leaves_hide(corpse.mon_type)
        && mons_genus(corpse.mon_type) == MONS_DRAGON)
    {
        // Uh... dragon hide is tough stuff and it keeps the monster in
        // one piece?  More importantly, it prevents a flavour feature
        // from becoming a trap for the unwary.

        return false;
    }

    ld.update();

    const int max_chunks = get_max_corpse_chunks(corpse.mon_type);

    int nchunks = 1;
    if (corpse.base_type == OBJ_GOLD)
    {
        nchunks = corpse.quantity;
        corpse.quantity = 1;
        // Don't give a bonus from *all* the gold chunks...
        corpse.special = 0;
    }
    else
    {
        nchunks += random2(max_chunks);
        nchunks = stepdown_value(nchunks, 4, 4, 12, 12);
    }

    // spray some blood
    if (corpse.base_type != OBJ_GOLD)
        blood_spray(where, corpse.mon_type, nchunks * 3);

    // Don't let the player evade food conducts by using OOD (!) or /disint
    // Spray blood, but no chunks. (The mighty hand of your God squashes them
    // in mid-flight...!)
    if (is_forbidden_food(corpse))
        return true;

    // turn the corpse into chunks
    if (corpse.base_type != OBJ_GOLD)
    {
        corpse.base_type = OBJ_FOOD;
        corpse.sub_type  = FOOD_CHUNK;
        if (is_bad_food(corpse))
            corpse.flags |= ISFLAG_DROPPED;
    }

    // spray chunks everywhere!
    for (int ntries = 0; nchunks > 0 && ntries < 10000; ++ntries)
    {
        coord_def cp = where;
        cp.x += random_range(-LOS_RADIUS, LOS_RADIUS);
        cp.y += random_range(-LOS_RADIUS, LOS_RADIUS);

        dprf("Trying to scatter chunk to %d, %d...", cp.x, cp.y);

        if (!in_bounds(cp))
            continue;

        if (!ld.see_cell(cp))
            continue;

        dprf("Cell is visible...");

        if (cell_is_solid(cp) || actor_at(cp))
            continue;

        --nchunks;

        dprf("Success");

        copy_item_to_grid(corpse, cp);
    }

    return true;
}

static int _calc_monster_experience(monster* victim, killer_type killer,
                                    int killer_index)
{
    const int experience = exper_value(victim);
    const bool no_xp = victim->has_ench(ENCH_ABJ) || !experience || victim->has_ench(ENCH_FAKE_ABJURATION);
    const bool created_friendly = testbits(victim->flags, MF_NO_REWARD);

    if (no_xp || !MON_KILL(killer) || invalid_monster_index(killer_index))
        return 0;

    monster* mon = &menv[killer_index];
    if (!mon->alive())
        return 0;

    if ((created_friendly && mon->friendly())
        || mons_aligned(mon, victim))
    {
        return 0;
    }

    return experience;
}

static void _give_monster_experience(int experience, int killer_index)
{
    if (experience <= 0 || invalid_monster_index(killer_index))
        return;

    monster* mon = &menv[killer_index];
    if (!mon->alive())
        return;

    if (mon->gain_exp(experience))
    {
        if (!you_worship(GOD_SHINING_ONE) && !you_worship(GOD_BEOGH)
            || player_under_penance()
            || !one_chance_in(3))
        {
            return;
        }

        // Randomly bless the follower who gained experience.
        if (you_worship(GOD_SHINING_ONE)
                && random2(you.piety) >= piety_breakpoint(0)
            || you_worship(GOD_BEOGH)
                && random2(you.piety) >= piety_breakpoint(2))
        {
            bless_follower(mon);
        }
    }
}

static void _beogh_spread_experience(int exp)
{
    int total_hd = 0;

    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (is_orcish_follower(*mi))
            total_hd += mi->get_experience_level();
    }

    if (total_hd <= 0)
        return;

    for (monster_near_iterator mi(&you); mi; ++mi)
        if (is_orcish_follower(*mi))
        {
            _give_monster_experience(exp * mi->get_experience_level() / total_hd,
                                         mi->mindex());
        }
}

static int _calc_player_experience(const monster* mons)
{
    int experience = exper_value(mons);

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
    const bool was_neutral = testbits(mons->flags, MF_WAS_NEUTRAL);
    const bool no_xp = mons->has_ench(ENCH_ABJ) || !experience || mons->has_ench(ENCH_FAKE_ABJURATION);
    const bool already_got_half_xp = testbits(mons->flags, MF_GOT_HALF_XP);
    const int half_xp = (experience + 1) / 2;

    if (created_friendly || was_neutral || no_xp)
        return 0; // No xp if monster was created friendly or summoned.

    if (!mons->damage_total)
    {
        mprf(MSGCH_WARN, "Error, exp for monster with no damage: %s",
             mons->name(DESC_PLAIN, true).c_str());
        return 0;
    }

    dprf("Damage ratio: %1.1f/%d (%d%%)",
         0.5 * mons->damage_friendly, mons->damage_total,
         50 * mons->damage_friendly / mons->damage_total);
    experience = (experience * mons->damage_friendly / mons->damage_total
                  + 1) / 2;
    ASSERT(mons->damage_friendly <= 2 * mons->damage_total);

    // All deaths of hostiles grant at least 50% XP in ZotDef.
    if (crawl_state.game_is_zotdef() && experience < half_xp)
        experience = half_xp;

    // Note: This doesn't happen currently since monsters with
    //       MF_GOT_HALF_XP have always gone through pacification,
    //       hence also have MF_WAS_NEUTRAL. [rob]
    if (already_got_half_xp)
    {
        experience -= half_xp;
        if (experience < 0)
            experience = 0;
    }

    return experience;
}

static void _give_player_experience(int experience, killer_type killer,
                                    bool pet_kill, bool was_visible)
{
    if (experience <= 0 || crawl_state.game_is_arena())
        return;

    unsigned int exp_gain = 0;
    gain_exp(experience, &exp_gain);

    kill_category kc =
            (killer == KILL_YOU || killer == KILL_YOU_MISSILE) ? KC_YOU :
            (pet_kill)                                         ? KC_FRIENDLY :
                                                                 KC_OTHER;
    PlaceInfo& curr_PlaceInfo = you.get_place_info();
    PlaceInfo  delta;

    delta.mon_kill_num[kc]++;
    delta.mon_kill_exp       += exp_gain;

    you.global_info += delta;
    you.global_info.assert_validity();

    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();

    // Give a message for monsters dying out of sight.
    if (exp_gain > 0 && !was_visible)
        mpr("You feel a bit more experienced.");

    if (kc == KC_YOU && you_worship(GOD_BEOGH))
        _beogh_spread_experience(experience / 2);
}

static void _give_experience(int player_exp, int monster_exp,
                             killer_type killer, int killer_index,
                             bool pet_kill, bool was_visible)
{
    _give_player_experience(player_exp, killer, pet_kill, was_visible);
    _give_monster_experience(monster_exp, killer_index);
}

// Returns the item slot of a generated corpse, or -1 if no corpse.
int place_monster_corpse(const monster* mons, bool silent, bool force)
{
    // The game can attempt to place a corpse for an out-of-bounds monster
    // if a shifter turns into a giant spore and explodes.  In this
    // case we place no corpse since the explosion means anything left
    // over would be scattered, tiny chunks of shifter.
    if (!in_bounds(mons->pos()))
        return -1;

    // Don't attempt to place corpses within walls, either.
    if (feat_is_wall(grd(mons->pos())))
        return -1;

    // If we were told not to leave a corpse, don't.
    if (mons->props.exists(NEVER_CORPSE_KEY))
        return -1;

    item_def corpse;
    const monster_type corpse_class = fill_out_corpse(mons, mons->type,
                                                      corpse);

    bool vault_forced = false;

    // Don't place a corpse?  If a zombified monster is somehow capable
    // of leaving a corpse, then always place it.
    if (mons_class_is_zombified(mons->type))
        force = true;

    // "always_corpse" forces monsters to always generate a corpse upon
    // their deaths.
    if (mons->props.exists("always_corpse")
        || mons_class_flag(mons->type, M_ALWAYS_CORPSE)
        || mons_is_demonspawn(mons->type)
           && mons_class_flag(draco_or_demonspawn_subspecies(mons),
                              M_ALWAYS_CORPSE))
    {
        vault_forced = true;
    }

    if (corpse_class == MONS_NO_MONSTER
        || (!force && !vault_forced && coinflip()))
    {
        return -1;
    }

    if (!force && in_good_standing(GOD_GOZAG))
    {
        const monsterentry* me = get_monster_data(corpse_class);
        const int base_gold = max(3, (me->weight - 200) / 27);
        corpse.clear();
        corpse.base_type = OBJ_GOLD;
        corpse.quantity = base_gold / 2 + random2avg(base_gold, 2);
        corpse.special = corpse.quantity;
        item_colour(corpse);
    }

    int o = get_mitm_slot();

    // Zotdef corpse creation forces cleanup, otherwise starvation
    // kicks in. The magic number 9 is less than the magic number of
    // 10 in get_mitm_slot which indicates that a cull will be initiated
    // if a free slot can't be found.
    if (o == NON_ITEM && crawl_state.game_is_zotdef())
        o = get_mitm_slot(9);

    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return -1;
    }

    mitm[o] = corpse;

    origin_set_monster(mitm[o], mons);

    if ((mons->flags & MF_EXPLODE_KILL) && explode_corpse(corpse, mons->pos()))
    {
        // We already have a spray of chunks.
        item_was_destroyed(mitm[o]);
        destroy_item(o);
        return -1;
    }

    move_item_to_grid(&o, mons->pos(), !mons->swimming());
    if (o != NON_ITEM && mitm[o].base_type == OBJ_GOLD)
    {
        invalidate_agrid(true);
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }

    if (you.see_cell(mons->pos()))
    {
        if (force && !silent)
        {
            if (you.can_see(mons))
                simple_monster_message(mons, " turns back into a corpse!");
            else
            {
                mprf("%s appears out of nowhere!",
                     mitm[o].name(DESC_A).c_str());
            }
        }
        if (o != NON_ITEM && !silent)
        {
            const bool poison =
                (carrion_is_poisonous(corpse) && player_res_poison() <= 0);
            hints_dissection_reminder(!poison);
        }
    }

    return o == NON_ITEM ? -1 : o;
}

static void _hints_inspect_kill()
{
    if (Hints.hints_events[HINT_KILLED_MONSTER])
        learned_something_new(HINT_KILLED_MONSTER);
}

static string _milestone_kill_verb(killer_type killer)
{
    return killer == KILL_BANISHED ? "banished" :
           killer == KILL_PACIFIED ? "pacified" :
           killer == KILL_ENSLAVED ? "enslaved" : "killed";
}

void record_monster_defeat(monster* mons, killer_type killer)
{
    if (crawl_state.game_is_arena())
        return;
    if (killer == KILL_RESET || killer == KILL_DISMISSED)
        return;
    if (mons->has_ench(ENCH_FAKE_ABJURATION))
        return;
    if (MONST_INTERESTING(mons))
    {
        take_note(Note(NOTE_DEFEAT_MONSTER, mons->type, mons->friendly(),
                       mons->full_name(DESC_A).c_str(),
                       _milestone_kill_verb(killer).c_str()));
    }
    // XXX: See comment in monster_polymorph.
    bool is_unique = mons_is_unique(mons->type);
    if (mons->props.exists("original_was_unique"))
        is_unique = mons->props["original_was_unique"].get_bool();
    if (mons->type == MONS_PLAYER_GHOST)
    {
        monster_info mi(mons);
        string milestone = _milestone_kill_verb(killer) + " the ghost of ";
        milestone += get_ghost_description(mi, true);
        milestone += ".";
        mark_milestone("ghost", milestone);
    }
    // Or summoned uniques, which a summoned ghost is treated as {due}
    else if (is_unique && !mons->is_summoned())
    {
        mark_milestone("uniq",
                       _milestone_kill_verb(killer)
                       + " "
                       + mons->name(DESC_THE, true)
                       + ".");
    }
}

static bool _is_pet_kill(killer_type killer, int i)
{
    if (!MON_KILL(killer))
        return false;

    if (i == ANON_FRIENDLY_MONSTER)
        return true;

    if (invalid_monster_index(i))
        return false;

    const monster* m = &menv[i];
    if (m->friendly()) // This includes enslaved monsters.
        return true;

    // Check if the monster was confused by you or a friendly, which
    // makes casualties to this monster collateral kills.
    const mon_enchant me = m->get_ench(ENCH_CONFUSION);
    const mon_enchant me2 = m->get_ench(ENCH_INSANE);
    return me.ench == ENCH_CONFUSION
           && (me.who == KC_YOU || me.who == KC_FRIENDLY)
           || me2.ench == ENCH_INSANE
              && (me2.who == KC_YOU || me2.who == KC_FRIENDLY);
}

int exp_rate(int killer)
{
    // Damage by the spectral weapon is considered to be the player's damage ---
    // so the player does not lose any exp from dealing damage with a spectral weapon summon
    if (!invalid_monster_index(killer)
        && menv[killer].type == MONS_SPECTRAL_WEAPON
        && menv[killer].summoner == MID_PLAYER)
    {
        return 2;
    }

    if (killer == MHITYOU || killer == YOU_FAULTLESS)
        return 2;

    if (_is_pet_kill(KILL_MON, killer))
        return 1;

    return 0;
}

// Elyvilon will occasionally (5% chance) protect the life of one of
// your allies.
static bool _ely_protect_ally(monster* mons, killer_type killer)
{
    if (!you_worship(GOD_ELYVILON))
        return false;

    if (!MON_KILL(killer) && !YOU_KILL(killer))
        return false;

    if (!mons->is_holy()
            && mons->holiness() != MH_NATURAL
        || !mons->friendly()
        || !you.can_see(mons) // for simplicity
        || !one_chance_in(20))
    {
        return false;
    }

    mons->hit_points = 1;

    snprintf(info, INFO_SIZE, " protects %s from harm!",
             mons->name(DESC_THE).c_str());

    simple_god_message(info);

    return true;
}

// Elyvilon retribution effect: Heal hostile monsters that were about to
// be killed by you or one of your friends.
static bool _ely_heal_monster(monster* mons, killer_type killer, int i)
{
    god_type god = GOD_ELYVILON;

    if (!you.penance[god] || !god_hates_your_god(god))
        return false;

    const int ely_penance = you.penance[god];

    if (mons->friendly() || !one_chance_in(10))
        return false;

    if (MON_KILL(killer) && !invalid_monster_index(i))
    {
        monster* mon = &menv[i];
        if (!mon->friendly() || !one_chance_in(3))
            return false;

        if (!mons_near(mons))
            return false;
    }
    else if (!YOU_KILL(killer))
        return false;

    dprf("monster hp: %d, max hp: %d", mons->hit_points, mons->max_hit_points);

    mons->hit_points = min(1 + random2(ely_penance/3), mons->max_hit_points);

    dprf("new hp: %d, ely penance: %d", mons->hit_points, ely_penance);

    snprintf(info, INFO_SIZE, "%s heals %s%s",
             god_name(god, false).c_str(),
             mons->name(DESC_THE).c_str(),
             mons->hit_points * 2 <= mons->max_hit_points ? "." : "!");

    god_speaks(god, info);
    dec_penance(god, 1);

    return true;
}

static bool _yred_enslave_soul(monster* mons, killer_type killer)
{
    if (you_worship(GOD_YREDELEMNUL) && mons_enslaved_body_and_soul(mons)
        && mons_near(mons) && killer != KILL_RESET
        && killer != KILL_DISMISSED
        && killer != KILL_BANISHED)
    {
        record_monster_defeat(mons, killer);
        record_monster_defeat(mons, KILL_ENSLAVED);
        yred_make_enslaved_soul(mons, player_under_penance());
        return true;
    }

    return false;
}

static bool _beogh_forcibly_convert_orc(monster* mons, killer_type killer,
                                        int i)
{
    if (in_good_standing(GOD_BEOGH, 2)
        && mons_genus(mons->type) == MONS_ORC
        && !mons->is_summoned() && !mons->is_shapeshifter()
        && mons_near(mons) && !mons_is_god_gift(mons))
    {
        bool convert = false;

        if (YOU_KILL(killer))
            convert = true;
        else if (MON_KILL(killer) && !invalid_monster_index(i))
        {
            monster* mon = &menv[i];
            if (is_follower(mon) && !one_chance_in(3))
                convert = true;
        }

        // Orcs may convert to Beogh under threat of death, either from
        // you or, less often, your followers.  In both cases, the
        // checks are made against your stats.  You're the potential
        // messiah, after all.
        if (convert)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Death convert attempt on %s, HD: %d, "
                 "your xl: %d",
                 mons->name(DESC_PLAIN).c_str(),
                 mons->get_hit_dice(),
                 you.experience_level);
#endif
            if (random2(you.piety) >= piety_breakpoint(0)
                && random2(you.experience_level) >=
                   random2(mons->get_hit_dice())
                // Bias beaten-up-conversion towards the stronger orcs.
                && random2(mons->get_experience_level()) > 2)
            {
                beogh_convert_orc(mons, true, MON_KILL(killer));
                return true;
            }
        }
    }

    return false;
}

static bool _lost_soul_nearby(const coord_def pos)
{
    for (monster_near_iterator mi(pos, LOS_NO_TRANS); mi; ++mi)
        if (mi->type == MONS_LOST_SOUL)
            return true;

    return false;
}

static bool _monster_avoided_death(monster* mons, killer_type killer, int i)
{
    if (mons->max_hit_points <= 0 || mons->get_hit_dice() < 1)
        return false;

    // Before the hp check since this should not care about the power of the
    // finishing blow
    if (!mons_is_zombified(mons)
        && (mons->holiness() == MH_UNDEAD || mons->holiness() == MH_NATURAL)
        && !testbits(mons->flags, MF_SPECTRALISED)
        && killer != KILL_RESET
        && killer != KILL_DISMISSED
        && killer != KILL_BANISHED
        && _lost_soul_nearby(mons->pos()))
    {
        if (lost_soul_revive(mons))
            return true;
    }

    if (mons->hit_points < -25 || mons->hit_points < -mons->max_hit_points)
        return false;

    // Elyvilon specials.
    if (_ely_protect_ally(mons, killer))
        return true;
    if (_ely_heal_monster(mons, killer, i))
        return true;

    // Yredelemnul special.
    if (_yred_enslave_soul(mons, killer))
        return true;

    // Beogh special.
    if (_beogh_forcibly_convert_orc(mons, killer, i))
        return true;

    return false;
}

static void _jiyva_died()
{
    if (you_worship(GOD_JIYVA))
        return;

    add_daction(DACT_REMOVE_JIYVA_ALTARS);

    if (!player_in_branch(BRANCH_SLIME))
        return;

    if (silenced(you.pos()))
    {
        god_speaks(GOD_JIYVA, "With an infernal shudder, the power ruling "
                   "this place vanishes!");
    }
    else
    {
        god_speaks(GOD_JIYVA, "With infernal noise, the power ruling this "
                   "place vanishes!");
    }
}

void fire_monster_death_event(monster* mons,
                              killer_type killer,
                              int i, bool polymorph)
{
    int type = mons->type;

    // Treat whatever the Royal Jelly polymorphed into as if it were still
    // the Royal Jelly (but if a player chooses the character name
    // "shaped Royal Jelly" don't unlock the vaults when the player's
    // ghost is killed).
    if (mons->mname == "shaped Royal Jelly"
        && !mons_is_pghost(mons->type))
    {
        type = MONS_ROYAL_JELLY;
    }

    if (!polymorph)
    {
        dungeon_events.fire_event(
            dgn_event(DET_MONSTER_DIED, mons->pos(), 0,
                      mons->mid, killer));
    }

    if (killer == KILL_BANISHED)
        return;

    los_monster_died(mons);

    if (type == MONS_ROYAL_JELLY && !polymorph)
    {
        you.royal_jelly_dead = true;

        if (jiyva_is_dead())
            _jiyva_died();
    }
}

static void _mummy_curse(monster* mons, killer_type killer, int index)
{
    int pow;

    switch (killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_MISC:
        case KILL_RESET:
        case KILL_DISMISSED:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_BANISHED:
            return;

        default:
            break;
    }

    switch (mons->type)
    {
        case MONS_MENKAURE:
        case MONS_MUMMY:          pow = 1; break;
        case MONS_GUARDIAN_MUMMY: pow = 3; break;
        case MONS_MUMMY_PRIEST:   pow = 8; break;
        case MONS_GREATER_MUMMY:  pow = 11; break;
        case MONS_KHUFU:          pow = 15; break;

        default:
            mprf(MSGCH_DIAGNOSTICS, "Unknown mummy type.");
            return;
    }

    actor* target;

    if (YOU_KILL(killer))
        target = &you;
    // Killed by a Zot trap, a god, etc, or suicide.
    else if (invalid_monster_index(index) || index == mons->mindex())
        return;
    else
        target = &menv[index];

    // Mummy was killed by a giant spore or ball lightning?
    if (!target->alive())
        return;

    // Mummies are smart enough not to waste curses on summons or allies.
    if (target->is_monster() && target->as_monster()->friendly()
        && !crawl_state.game_is_arena())
    {
        target = &you;
    }

    if ((mons->type == MONS_MUMMY || mons->type == MONS_MENKAURE)
        && target->is_player())
    {
        // Kiku protects you from ordinary mummy curses.
        if (in_good_standing(GOD_KIKUBAAQUDGHA, 1))
        {
            simple_god_message(" averts the curse.");
            return;
        }

        mprf(MSGCH_MONSTER_SPELL, "You feel nervous for a moment...");
        curse_an_item();
    }
    else
    {
        if (target->is_player())
            mprf(MSGCH_MONSTER_SPELL, "You feel extremely nervous for a moment...");
        else if (you.can_see(target))
        {
            mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
                 target->name(DESC_THE).c_str());
        }
        MiscastEffect(target, mons->mindex(), SPTYP_NECROMANCY,
                      pow, random2avg(88, 3), "a mummy death curse");
    }
}

template<typename valid_T, typename connect_T>
static void _search_dungeon(const coord_def & start,
                    valid_T & valid_target,
                    connect_T & connecting_square,
                    set<position_node> & visited,
                    vector<set<position_node>::iterator> & candidates,
                    bool exhaustive = true,
                    int connect_mode = 8)
{
    if (connect_mode < 1 || connect_mode > 8)
        connect_mode = 8;

    // Ordering the default compass index this way gives us the non
    // diagonal directions as the first four elements - so by just
    // using the first 4 elements instead of the whole array we
    // can have 4-connectivity.
    int compass_idx[] = {0, 2, 4, 6, 1, 3, 5, 7};

    position_node temp_node;
    temp_node.pos = start;
    temp_node.last = NULL;

    queue<set<position_node>::iterator > fringe;

    set<position_node>::iterator current = visited.insert(temp_node).first;
    fringe.push(current);

    bool done = false;
    while (!fringe.empty())
    {
        current = fringe.front();
        fringe.pop();

        shuffle_array(compass_idx, connect_mode);

        for (int i=0; i < connect_mode; ++i)
        {
            coord_def adjacent = current->pos + Compass[compass_idx[i]];
            if (in_bounds(adjacent))
            {
                temp_node.pos = adjacent;
                temp_node.last = &(*current);
                pair<set<position_node>::iterator, bool > res;
                res = visited.insert(temp_node);

                if (!res.second)
                    continue;

                if (valid_target(adjacent))
                {
                    candidates.push_back(res.first);
                    if (!exhaustive)
                    {
                        done = true;
                        break;
                    }

                }

                if (connecting_square(adjacent))
                {
//                    if (res.second)
                    fringe.push(res.first);
                }
            }
        }
        if (done)
            break;
    }
}

static bool _ballisto_at(const coord_def & target)
{
    monster* mons = monster_at(target);
    return mons && mons->type == MONS_BALLISTOMYCETE
           && mons->alive();
}

static bool _player_at(const coord_def & target)
{
    return you.pos() == target;
}

static bool _mold_connected(const coord_def & target)
{
    return is_moldy(target) || _ballisto_at(target);
}

// If 'monster' is a ballistomycete or spore, activate some number of
// ballistomycetes on the level.
static void _activate_ballistomycetes(monster* mons, const coord_def& origin,
                                      bool player_kill)
{
    if (!mons || mons->is_summoned()
              || mons->mons_species() != MONS_BALLISTOMYCETE
                 && mons->type != MONS_GIANT_SPORE)
    {
        return;
    }

    // If a spore or inactive ballisto died we will only activate one
    // other ballisto. If it was an active ballisto we will distribute
    // its count to others on the level.
    int activation_count = 1;
    if (mons->type == MONS_BALLISTOMYCETE)
        activation_count += mons->number;
    if (mons->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        activation_count = 0;

    int non_activable_count = 0;
    int ballisto_count = 0;

    bool any_friendly = mons->attitude == ATT_FRIENDLY;
    bool fedhas_mode  = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->mindex() != mons->mindex() && mi->alive())
        {
            if (mi->type == MONS_BALLISTOMYCETE)
                ballisto_count++;
            else if (mi->type == MONS_GIANT_SPORE
                     || mi->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
            {
                non_activable_count++;
            }

            if (mi->attitude == ATT_FRIENDLY)
                any_friendly = true;
        }
    }

    bool exhaustive = true;
    bool (*valid_target)(const coord_def &) = _ballisto_at;
    bool (*connecting_square) (const coord_def &) = _mold_connected;

    set<position_node> visited;
    vector<set<position_node>::iterator > candidates;

    if (you_worship(GOD_FEDHAS))
    {
        if (non_activable_count == 0
            && ballisto_count == 0
            && any_friendly
            && mons->type == MONS_BALLISTOMYCETE)
        {
            mpr("Your fungal colony was destroyed.");
            dock_piety(5, 0);
        }

        fedhas_mode = true;
        activation_count = 1;
        exhaustive = false;
        valid_target = _player_at;
    }

    _search_dungeon(origin, valid_target, connecting_square, visited,
                    candidates, exhaustive);

    if (candidates.empty())
    {
        if (!fedhas_mode
            && non_activable_count == 0
            && ballisto_count == 0
            && mons->attitude == ATT_HOSTILE)
        {
            if (player_kill)
                mpr("The fungal colony is destroyed.");

            // Get rid of the mold, so it'll be more useful when new fungi
            // spawn.
            for (rectangle_iterator ri(1); ri; ++ri)
                remove_mold(*ri);
        }

        return;
    }

    // A (very) soft cap on colony growth, no activations if there are
    // already a lot of ballistos on level.
    if (candidates.size() > 25)
        return;

    shuffle_array(candidates);

    int index = 0;

    for (int i = 0; i < activation_count; ++i)
    {
        index = i % candidates.size();

        monster* spawner = monster_at(candidates[index]->pos);

        // This may be the players position, in which case we don't
        // have to mess with spore production on anything
        if (spawner && !fedhas_mode)
        {
            spawner->number++;

            // Change color and start the spore production timer if we
            // are moving from 0 to 1.
            if (spawner->number == 1)
            {
                spawner->colour = LIGHTMAGENTA;
                // Reset the spore production timer.
                spawner->del_ench(ENCH_SPORE_PRODUCTION, false);
                spawner->add_ench(ENCH_SPORE_PRODUCTION);
            }
        }

        const position_node* thread = &(*candidates[index]);
        while (thread)
        {
            if (!one_chance_in(3))
                env.pgrid(thread->pos) |= FPROP_GLOW_MOLD;

            thread = thread->last;
        }
        env.level_state |= LSTATE_GLOW_MOLD;
    }
}

static void _setup_base_explosion(bolt & beam, const monster& origin)
{
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.source_id    = origin.mid;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source       = origin.pos();
    beam.source_name  = origin.base_name(DESC_BASENAME, true);
    beam.target       = origin.pos();
    beam.noise_msg    = "You hear an explosion!";

    if (!crawl_state.game_is_arena() && origin.attitude == ATT_FRIENDLY
        && !origin.is_summoned())
    {
        beam.thrower = KILL_YOU;
    }
    else
        beam.thrower = KILL_MON;

    beam.aux_source.clear();
    beam.attitude = origin.attitude;
}

void setup_spore_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_SPORE;
    beam.damage  = dice_def(3, 15);
    beam.name    = "explosion of spores";
    beam.colour  = LIGHTGREY;
    beam.ex_size = 2;
}

static void _setup_lightning_explosion(bolt & beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour   = BEAM_ELECTRICITY;
    beam.damage    = dice_def(3, 5 + origin.get_hit_dice() * 5 / 4);
    beam.name      = "blast of lightning";
    beam.noise_msg = "You hear a clap of thunder!";
    beam.colour    = LIGHTCYAN;
    beam.ex_size   = x_chance_in_y(origin.get_hit_dice(), 24) ? 3 : 2;
    // Don't credit the player for ally-summoned ball lightning explosions.
    if (origin.summoner && origin.summoner != MID_PLAYER)
        beam.thrower = KILL_MON;
}

static void _setup_prism_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_MMISSILE;
    beam.damage  = (origin.number == 2 ?
                        dice_def(3, 6 + origin.get_hit_dice() * 7 / 4)
                        : dice_def(2, 6 + origin.get_hit_dice() * 7 / 4));
    beam.name    = "blast of energy";
    beam.colour  = MAGENTA;
    beam.ex_size = origin.number;
}

static void _setup_bennu_explosion(bolt& beam, const monster& origin)
{
    _setup_base_explosion(beam, origin);
    beam.flavour = BEAM_GHOSTLY_FLAME;
    beam.damage  = dice_def(3, 5 + origin.get_hit_dice() * 5 / 4);
    beam.name    = "pyre of ghostly fire";
    beam.noise_msg = "You hear an otherworldly crackling!";
    beam.colour  = CYAN;
    beam.ex_size = 2;
}

static void _setup_inner_flame_explosion(bolt & beam, const monster& origin,
                                         actor* agent)
{
    _setup_base_explosion(beam, origin);
    const int size   = origin.body_size(PSIZE_BODY);
    beam.flavour     = BEAM_FIRE;
    beam.damage      = (size > SIZE_BIG)  ? dice_def(3, 25) :
                       (size > SIZE_TINY) ? dice_def(3, 20) :
                                            dice_def(3, 15);
    beam.name        = "fiery explosion";
    beam.colour      = RED;
    beam.ex_size     = (size > SIZE_BIG) ? 2 : 1;
    beam.source_name = origin.name(DESC_A, true);
    beam.thrower     = (agent && agent->is_player()) ? KILL_YOU_MISSILE
                                                     : KILL_MON_MISSILE;
}

static bool _explode_monster(monster* mons, killer_type killer,
                             int killer_index, bool pet_kill, bool wizard)
{
    if (mons->hit_points > 0 || mons->hit_points <= -15 || wizard
        || killer == KILL_RESET || killer == KILL_DISMISSED
        || killer == KILL_BANISHED)
    {
        if (killer != KILL_TIMEOUT)
            return false;
    }

    bolt beam;
    const int type = mons->type;
    const char* sanct_msg = NULL;
    actor* agent = mons;

    if (type == MONS_GIANT_SPORE)
    {
        setup_spore_explosion(beam, *mons);
        sanct_msg    = "By Zin's power, the giant spore's explosion is "
                       "contained.";
    }
    else if (type == MONS_BALL_LIGHTNING)
    {
        _setup_lightning_explosion(beam, *mons);
        sanct_msg    = "By Zin's power, the ball lightning's explosion "
                       "is contained.";
    }
    else if (type == MONS_LURKING_HORROR)
        sanct_msg = "The lurking horror fades away harmlessly.";
    else if (type == MONS_FULMINANT_PRISM)
    {
        _setup_prism_explosion(beam, *mons);
        sanct_msg = "By Zin's power, the prism's explosion is contained.";
    }
    else if (type == MONS_BENNU)
    {
        _setup_bennu_explosion(beam, *mons);
        sanct_msg = "By Zin's power, the bennu's fires are quelled.";
    }
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        mon_enchant i_f = mons->get_ench(ENCH_INNER_FLAME);
        ASSERT(i_f.ench == ENCH_INNER_FLAME);
        agent = actor_by_mid(i_f.source);
        _setup_inner_flame_explosion(beam, *mons, agent);
        // This might need to change if monsters ever get the ability to cast
        // Inner Flame...
        if (agent && agent->is_player())
            mons_add_blame(mons, "hexed by the player character");
        else if (agent)
            mons_add_blame(mons, "hexed by " + agent->name(DESC_A, true));
        mons->flags    |= MF_EXPLODE_KILL;
        sanct_msg       = "By Zin's power, the fiery explosion "
                          "is contained.";
        beam.aux_source = "exploding inner flame";
    }
    else
    {
        msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                        << static_cast<int>(type)
                                        << endl;
        return false;
    }

    if (beam.aux_source.empty())
    {
        if (type == MONS_BENNU)
        {
            if (YOU_KILL(killer))
                beam.aux_source = "ignited by themself";
            else if (pet_kill)
                beam.aux_source = "ignited by their pet";
        }
        else
        {
            if (YOU_KILL(killer))
                beam.aux_source = "set off by themself";
            else if (pet_kill)
                beam.aux_source = "set off by their pet";
        }
    }

    bool saw = false;
    if (you.can_see(mons))
    {
        saw = true;
        viewwindow();
        if (is_sanctuary(mons->pos()))
            mprf(MSGCH_GOD, "%s", sanct_msg);
        else if (type == MONS_BENNU)
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s blazes out!",
                 mons->full_name(DESC_THE).c_str());
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s explodes!",
                 mons->full_name(DESC_THE).c_str());
    }

    if (is_sanctuary(mons->pos()))
        return false;

    // Explosion side-effects.
    if (type == MONS_LURKING_HORROR)
        torment(mons, mons->mindex(), mons->pos());
    else if (mons->has_ench(ENCH_INNER_FLAME))
    {
        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
            if (!cell_is_solid(*ai) && env.cgrid(*ai) == EMPTY_CLOUD
                && !one_chance_in(5))
            {
                place_cloud(CLOUD_FIRE, *ai, 10 + random2(10), agent);
            }
    }

    // Detach monster from the grid first, so it doesn't get hit by
    // its own explosion. (GDL)
    // Unless it's a phoenix, where this isn't much of a concern.
    mgrd(mons->pos()) = NON_MONSTER;

    // The explosion might cause a monster to be placed where the bomb
    // used to be, so make sure that mgrd() doesn't get cleared a second
    // time (causing the new monster to become floating) when
    // mons->reset() is called.
    if (type == MONS_GIANT_SPORE)
        mons->set_position(coord_def(0,0));

    // Exploding kills the monster a bit earlier than normal.
    mons->hit_points = -16;
    if (saw)
        viewwindow();

    // FIXME: show_more == mons_near(mons)
    if (type == MONS_LURKING_HORROR)
    {
        targetter_los hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, DARKGRAY, 300, &hitfunc);
    }
    else
        beam.explode();

    _activate_ballistomycetes(mons, beam.target, YOU_KILL(beam.killer()));
    // Monster died in explosion, so don't re-attach it to the grid.
    return true;
}

static void _monster_die_cloud(const monster* mons, bool corpse, bool silent,
                        bool summoned)
{
    // Chaos spawn always leave behind a cloud of chaos.
    if (mons->type == MONS_CHAOS_SPAWN)
    {
        summoned = true;
        corpse   = false;
    }

    if (!summoned)
        return;

    if (cell_is_solid(mons->pos()))
        return;

    string prefix = " ";
    if (corpse)
    {
        if (mons_weight(mons_species(mons->type)) == 0)
            return;

        prefix = "'s corpse ";
    }

    string msg = summoned_poof_msg(mons) + "!";

    cloud_type cloud = CLOUD_NONE;
    if (msg.find("smoke") != string::npos)
        cloud = random_smoke_type();
    else if (msg.find("chaos") != string::npos)
        cloud = CLOUD_CHAOS;

    if (!silent)
        simple_monster_message(mons, (prefix + msg).c_str());

    if (cloud != CLOUD_NONE)
        place_cloud(cloud, mons->pos(), 1 + random2(3), mons);
}

static string _killer_type_name(killer_type killer)
{
    switch (killer)
    {
    case KILL_NONE:
        return "none";
    case KILL_YOU:
        return "you";
    case KILL_MON:
        return "mon";
    case KILL_YOU_MISSILE:
        return "you_missile";
    case KILL_MON_MISSILE:
        return "mon_missile";
    case KILL_YOU_CONF:
        return "you_conf";
    case KILL_MISCAST:
        return "miscast";
    case KILL_MISC:
        return "misc";
    case KILL_RESET:
        return "reset";
    case KILL_DISMISSED:
        return "dismissed";
    case KILL_BANISHED:
        return "banished";
    case KILL_UNSUMMONED:
        return "unsummoned";
    case KILL_TIMEOUT:
        return "timeout";
    case KILL_PACIFIED:
        return "pacified";
    case KILL_ENSLAVED:
        return "enslaved";
    }
    die("invalid killer type");
}

static void _make_spectral_thing(monster* mons, bool quiet)
{
    if (mons->holiness() == MH_NATURAL && mons_can_be_zombified(mons))
    {
        const monster_type spectre_type = mons_species(mons->type);
        enchant_type shapeshift = ENCH_NONE;
        if (mons->has_ench(ENCH_SHAPESHIFTER))
            shapeshift = ENCH_SHAPESHIFTER;
        else if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            shapeshift = ENCH_GLOWING_SHAPESHIFTER;

        // Headless hydras cannot be made spectral hydras, sorry.
        if (spectre_type == MONS_HYDRA && mons->heads() == 0)
        {
            mpr("A glowing mist gathers momentarily, then fades.");
            return;
        }

        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        if (monster *spectre = create_monster(
                mgen_data(MONS_SPECTRAL_THING, BEH_FRIENDLY, &you,
                    0, SPELL_DEATH_CHANNEL, mons->pos(), MHITYOU,
                    0, static_cast<god_type>(you.attribute[ATTR_DIVINE_DEATH_CHANNEL]),
                    mons->type, mons->number)))
        {
            if (!quiet)
                mpr("A glowing mist starts to gather...");

            // If the original monster has been levelled up, its HD might be
            // different from its class HD, in which case its HP should be
            // rerolled to match.
            if (spectre->get_experience_level() != mons->get_experience_level())
            {
                spectre->set_hit_dice(max(mons->get_experience_level(), 1));
                roll_zombie_hp(spectre);
            }

            name_zombie(spectre, mons);

            spectre->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 6));
            if (shapeshift)
                spectre->add_ench(shapeshift);
        }
    }
}

static void _druid_final_boon(const monster* mons)
{
    vector<monster*> beasts;
    for (monster_near_iterator mi(mons); mi; ++mi)
    {
        if (mons_is_beast(mons_base_type(*mi)) && mons_aligned(mons, *mi))
            beasts.push_back(*mi);
    }

    if (!beasts.size())
        return;

    if (you.can_see(mons))
    {
        mprf(MSGCH_MONSTER_SPELL, "With its final breath, %s offers up its power "
                                  "to the beasts of the wild!",
                                  mons->name(DESC_THE).c_str());
    }

    shuffle_array(beasts);
    int num = min((int)beasts.size(),
                  random_range(mons->get_hit_dice() / 3,
                               mons->get_hit_dice() / 2 + 1));

    // Healing and empowering done in two separate loops for tidier messages
    for (int i = 0; i < num; ++i)
    {
        if (beasts[i]->heal(roll_dice(3, mons->get_hit_dice()))
            && you.can_see(beasts[i]))
        {
            mprf("%s %s healed.", beasts[i]->name(DESC_THE).c_str(),
                                  beasts[i]->conj_verb("are").c_str());
        }
    }

    for (int i = 0; i < num; ++i)
    {
        simple_monster_message(beasts[i], " seems to grow more fierce.");
        beasts[i]->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1, mons,
                                        random_range(120, 200)));
    }
}

static bool _mons_reaped(actor *killer, monster* victim)
{
    beh_type beh;
    unsigned short hitting;

    if (killer->is_player())
    {
        hitting = MHITYOU;
        beh     = BEH_FRIENDLY;
    }
    else
    {
        monster* mon = killer->as_monster();

        beh = SAME_ATTITUDE(mon);

        // Get a new foe for the zombie to target.
        behaviour_event(mon, ME_EVAL);
        hitting = mon->foe;
    }

    monster *zombie = 0;
    if (animate_remains(victim->pos(), CORPSE_BODY, beh, hitting, killer, "",
                        GOD_NO_GOD, true, true, true, &zombie) <= 0)
    {
        return false;
    }

    if (you.can_see(victim))
        mprf("%s turns into a zombie!", victim->name(DESC_THE).c_str());
    else if (you.can_see(zombie))
        mprf("%s appears out of thin air!", zombie->name(DESC_THE).c_str());

    player_angers_monster(zombie);

    return true;
}

static bool _reaping(monster *mons)
{
    if (!mons->props.exists("reaping_damage"))
        return false;

    int rd = mons->props["reaping_damage"].get_int();
    dprf("Reaping chance: %d/%d", rd, mons->damage_total);
    if (!x_chance_in_y(rd, mons->damage_total))
        return false;

    actor *killer = actor_by_mid(mons->props["reaper"].get_int());
    if (killer)
        return _mons_reaped(killer, mons);
    return false;
}

int monster_die(monster* mons, const actor *killer, bool silent,
                bool wizard, bool fake)
{
    killer_type ktype = KILL_YOU;
    int kindex = NON_MONSTER;

    if (!killer)
        ktype = KILL_NONE;
    else if (killer->is_monster())
    {
        const monster *kmons = killer->as_monster();
        ktype = kmons->confused_by_you() ? KILL_YOU_CONF : KILL_MON;
        kindex = kmons->mindex();
    }

    return monster_die(mons, ktype, kindex, silent, wizard, fake);
}

// Returns the slot of a possibly generated corpse or -1.
int monster_die(monster* mons, killer_type killer,
                int killer_index, bool silent, bool wizard, bool fake)
{
    if (invalid_monster(mons))
        return -1;

    const bool was_visible = you.can_see(mons);

    // If a monster was banished to the Abyss and then killed there,
    // then its death wasn't a banishment.
    if (player_in_branch(BRANCH_ABYSS))
        mons->flags &= ~MF_BANISHED;

    if (!silent && !fake
        && _monster_avoided_death(mons, killer, killer_index))
    {
        mons->flags &= ~MF_EXPLODE_KILL;
        return -1;
    }

    // If the monster was calling the tide, let go now.
    mons->del_ench(ENCH_TIDE);

    // Same for silencers.
    mons->del_ench(ENCH_SILENCE);

    // ... and liquefiers.
    mons->del_ench(ENCH_LIQUEFYING);

    // Clean up any blood from the flayed effect
    if (mons->has_ench(ENCH_FLAYED))
        heal_flayed_effect(mons, true, true);

    crawl_state.inc_mon_acting(mons);

    ASSERT(!(YOU_KILL(killer) && crawl_state.game_is_arena()));

    if (mons->props.exists("monster_dies_lua_key"))
    {
        lua_stack_cleaner clean(dlua);

        dlua_chunk &chunk = mons->props["monster_dies_lua_key"];

        if (!chunk.load(dlua))
        {
            push_monster(dlua, mons);
            clua_pushcxxstring(dlua, _killer_type_name(killer));
            dlua.callfn(NULL, 2, 0);
        }
        else
        {
            mprf(MSGCH_ERROR,
                 "Lua death function for monster '%s' didn't load: %s",
                 mons->full_name(DESC_PLAIN).c_str(),
                 dlua.error.c_str());
        }
    }

    mons_clear_trapping_net(mons);
    mons->stop_constricting_all(false);
    mons->stop_being_constricted();

    you.remove_beholder(mons);
    you.remove_fearmonger(mons);
    // Uniques leave notes and milestones, so this information is already leaked.
    remove_unique_annotation(mons);

    // Clear auto exclusion now the monster is killed - if we know about it.
    if (mons_near(mons) || wizard || mons_is_unique(mons->type))
        remove_auto_exclude(mons);

          int  duration      = 0;
    const bool summoned      = mons->is_summoned(&duration);
    const int monster_killed = mons->mindex();
    const bool hard_reset    = testbits(mons->flags, MF_HARD_RESET);
    bool unsummoned          = killer == KILL_UNSUMMONED;
    bool timeout             = killer == KILL_TIMEOUT;
    const bool gives_xp      = (!summoned && !mons_class_flag(mons->type,
                                M_NO_EXP_GAIN));

    bool drop_items    = !hard_reset;

    const bool mons_reset(killer == KILL_RESET || killer == KILL_DISMISSED);

    bool fake_abjuration = (mons->has_ench(ENCH_FAKE_ABJURATION));

    const bool submerged     = mons->submerged();

    bool in_transit          = false;
    bool was_banished        = (killer == KILL_BANISHED);

    // Award experience for suicide if the suicide was caused by the
    // player.
    if (MON_KILL(killer) && monster_killed == killer_index)
    {
        if (mons->confused_by_you())
        {
            ASSERT(!crawl_state.game_is_arena());
            killer = KILL_YOU_CONF;
        }
    }
    else if (MON_KILL(killer) && mons->has_ench(ENCH_CHARM))
    {
        ASSERT(!crawl_state.game_is_arena());
        killer = KILL_YOU_CONF; // Well, it was confused in a sense... (jpeg)
    }

    // Kills by the spectral weapon are considered as kills by the player instead
    // Ditto Dithmenos shadow kills.
    if ((killer == KILL_MON || killer == KILL_MON_MISSILE)
        && !invalid_monster_index(killer_index)
        && ((menv[killer_index].type == MONS_SPECTRAL_WEAPON
             && menv[killer_index].summoner == MID_PLAYER)
            || mons_is_player_shadow(&menv[killer_index])))
    {
        killer = (killer == KILL_MON_MISSILE) ? KILL_YOU_MISSILE
                                              : KILL_YOU;
        killer_index = you.mindex();
    }

    // Take notes and mark milestones.
    record_monster_defeat(mons, killer);

    // Various sources of berserk extension on kills.
    if (killer == KILL_YOU && you.berserk())
    {
        if (in_good_standing(GOD_TROG) && you.piety > random2(1000))
        {
            const int bonus = (3 + random2avg(10, 2)) / 2;

            you.increase_duration(DUR_BERSERK, bonus);

            mprf(MSGCH_GOD, GOD_TROG,
                 "You feel the power of Trog in you as your rage grows.");
        }
        else if (player_equip_unrand(UNRAND_BLOODLUST) && coinflip())
        {
            const int bonus = (2 + random2(4)) / 2;
            you.increase_duration(DUR_BERSERK, bonus);
            mpr("The necklace of Bloodlust glows a violent red.");
        }
    }

    if (you.prev_targ == monster_killed)
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }

    if (killer == KILL_YOU)
        crawl_state.cancel_cmd_repeat();

    const bool pet_kill = _is_pet_kill(killer, killer_index);

    bool did_death_message = false;

    if (mons->type == MONS_GIANT_SPORE
        || mons->type == MONS_BALL_LIGHTNING
        || mons->type == MONS_LURKING_HORROR
        || (mons->type == MONS_FULMINANT_PRISM && mons->number > 0)
        || mons->type == MONS_BENNU
        || mons->has_ench(ENCH_INNER_FLAME))
    {
        did_death_message =
            _explode_monster(mons, killer, killer_index, pet_kill, wizard);
    }
    else if (mons->type == MONS_FULMINANT_PRISM && mons->number == 0)
    {
        if (!silent && !hard_reset && !was_banished)
        {
            simple_monster_message(mons, " detonates feebly.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }
    }
    else if (mons->type == MONS_FIRE_VORTEX
             || mons->type == MONS_SPATIAL_VORTEX
             || mons->type == MONS_TWISTER)
    {
        if (!silent && !mons_reset && !was_banished)
        {
            simple_monster_message(mons, " dissipates!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (mons->type == MONS_FIRE_VORTEX && !wizard && !mons_reset
            && !submerged && !was_banished && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_FIRE, mons->pos(), 2 + random2(4), mons);
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_SIMULACRUM)
    {
        if (!silent && !mons_reset && !was_banished)
        {
            simple_monster_message(mons, " vapourises!",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
            silent = true;
        }

        if (!wizard && !mons_reset && !submerged && !was_banished
            && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_COLD, mons->pos(), 2 + random2(4), mons);
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_DANCING_WEAPON)
    {
        if (!hard_reset)
        {
            if (killer == KILL_RESET)
                killer = KILL_DISMISSED;
        }

        int w_idx = mons->inv[MSLOT_WEAPON];
        ASSERT(w_idx != NON_ITEM);

        // XXX: This can probably become mons->is_summoned(): there's no
        // feasible way for a dancing weapon to "drop" it's weapon and somehow
        // gain a summoned one, or vice versa.
        bool summoned_it = mitm[w_idx].flags & ISFLAG_SUMMONED;

        if (!silent && !hard_reset && !was_banished)
        {
            if (!summoned_it)
            {
                simple_monster_message(mons, " falls from the air.",
                                       MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                silent = true;
            }
            else
                killer = KILL_RESET;
        }

        if (was_banished && !summoned_it && !hard_reset && mons->has_ench(ENCH_ABJ))
        {
            if (is_unrandom_artefact(mitm[w_idx]))
                set_unique_item_status(mitm[w_idx], UNIQ_LOST_IN_ABYSS);

            destroy_item(w_idx);
        }
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE)
    {
        if (!silent && !mons_reset && !mons->has_ench(ENCH_SEVERED) && !was_banished)
        {
            if (you.can_see(mons))
            {
                mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, silenced(mons->pos()) ?
                    "The tentacle is hauled back through the portal!" :
                    "With a roar, the tentacle is hauled back through the portal!");
            }
            silent = true;
        }

        if (killer == KILL_RESET)
            killer = KILL_DISMISSED;
    }
    else if (mons->type == MONS_BATTLESPHERE)
    {
        if (!wizard && !mons_reset && !was_banished
            && !cell_is_solid(mons->pos()))
        {
            place_cloud(CLOUD_MAGIC_TRAIL, mons->pos(), 3 + random2(3), mons);
        }
        end_battlesphere(mons, true);
    }
    else if (mons->type == MONS_BRIAR_PATCH)
    {
        if (timeout && !silent)
            simple_monster_message(mons, " crumbles away.");
    }
    else if (mons->type == MONS_SPECTRAL_WEAPON)
    {
        end_spectral_weapon(mons, true, killer == KILL_RESET);
        silent = true;
    }
    else if (mons->type == MONS_GRAND_AVATAR)
    {
        if (!silent)
        {
            simple_monster_message(mons, " fades into the ether.",
                                   MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        }
        silent = true;
    }
    else if (mons->type == MONS_DROWNED_SOUL)
    {
        // Suppress death message if 'killed' by touching something
        if (mons->hit_points == -1000)
            silent = true;
    }
    else if (mons->type == MONS_SPRIGGAN_DRUID && !silent && !was_banished
             && !wizard && !mons_reset)
    {
        _druid_final_boon(mons);
    }

    else if (mons->type == MONS_ELEMENTAL_WELLSPRING
             && mons->mindex() == killer_index)
    {
        if (!silent)
            simple_monster_message(mons, " exhausts itself and dries up.");
        silent = true;
    }

    const bool death_message = !silent && !did_death_message
                               && mons_near(mons)
                               && mons->visible_to(&you);
    const bool exploded      = mons->flags & MF_EXPLODE_KILL;

    const bool created_friendly = testbits(mons->flags, MF_NO_REWARD);
    const bool was_neutral = testbits(mons->flags, MF_WAS_NEUTRAL);
          bool anon = (killer_index == ANON_FRIENDLY_MONSTER);
    mon_holy_type targ_holy = mons->holiness();

    // Dual holiness, Trog and Kiku like dead demons but not undead.
    if ((mons->type == MONS_ABOMINATION_SMALL
         || mons->type == MONS_ABOMINATION_LARGE
         || mons->type == MONS_CRAWLING_CORPSE
         || mons->type == MONS_MACABRE_MASS)
        && (you_worship(GOD_TROG)
         || you_worship(GOD_KIKUBAAQUDGHA)))
    {
        targ_holy = MH_DEMONIC;
    }

    // Adjust song of slaying bonus
    // Kills by the spectral weapon should be adjusted by this point to be
    // kills by the player --- so kills by the spectral weapon are considered
    // here as well
    if (killer == KILL_YOU && you.duration[DUR_SONG_OF_SLAYING] && gives_xp
        && !mons->has_ench(ENCH_ABJ) && !fake_abjuration && !created_friendly
        && !was_neutral)
    {
        int sos_bonus = you.props["song_of_slaying_bonus"].get_int();
        mon_threat_level_type threat = mons_threat_level(mons, true);
        // Only certain kinds of threats at different sos levels will increase the bonus
        if (threat == MTHRT_TRIVIAL && sos_bonus < 3
            || threat == MTHRT_EASY && sos_bonus < 5
            || threat == MTHRT_TOUGH && sos_bonus < 7
            || threat == MTHRT_NASTY)
        {
            you.props["song_of_slaying_bonus"] = sos_bonus + 1;
        }
    }

    switch (killer)
    {
        case KILL_YOU:          // You kill in combat.
        case KILL_YOU_MISSILE:  // You kill by missile or beam.
        case KILL_YOU_CONF:     // You kill by confusion.
        {
            const bool bad_kill    = god_hates_killing(you.religion, mons)
                                     && killer_index != YOU_FAULTLESS;
            const bool good_kill   = gives_xp && !created_friendly;

            if (death_message)
            {
                if (killer == KILL_YOU_CONF
                    && (anon || !invalid_monster_index(killer_index)))
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s is %s!",
                         mons->name(DESC_THE).c_str(),
                         exploded                        ? "blown up" :
                         wounded_damaged(targ_holy)     ? "destroyed"
                                                         : "killed");
                }
                else
                {
                    mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "You %s %s!",
                         exploded                        ? "blow up" :
                         wounded_damaged(targ_holy)     ? "destroy"
                                                         : "kill",
                         mons->name(DESC_THE).c_str());
                }

                if ((created_friendly || was_neutral) && gives_xp)
                    mpr("That felt strangely unrewarding.");
            }

            // Killing triggers hints mode lesson.
            if (gives_xp)
                _hints_inspect_kill();

            // Prevent summoned creatures from being good kills.
            if (bad_kill || good_kill)
            {
                if (targ_holy == MH_DEMONIC || mons_is_demonspawn(mons->type))
                {
                    did_god_conduct(DID_KILL_DEMON,
                                    mons->get_experience_level(), true, mons);
                }
                else if (targ_holy == MH_NATURAL)
                {
                    did_god_conduct(DID_KILL_LIVING,
                                    mons->get_experience_level(), true, mons);

                    // TSO hates natural evil and unholy beings.
                    if (mons->is_unholy())
                    {
                        did_god_conduct(DID_KILL_NATURAL_UNHOLY,
                                        mons->get_experience_level(), true, mons);
                    }
                    else if (mons->is_evil())
                    {
                        did_god_conduct(DID_KILL_NATURAL_EVIL,
                                        mons->get_experience_level(), true, mons);
                    }
                }
                else if (targ_holy == MH_UNDEAD)
                {
                    did_god_conduct(DID_KILL_UNDEAD,
                                    mons->get_experience_level(), true, mons);
                }

                // Zin hates unclean and chaotic beings.
                if (mons->how_unclean())
                {
                    did_god_conduct(DID_KILL_UNCLEAN,
                                    mons->get_experience_level(), true, mons);
                }
                else if (mons->how_chaotic())
                {
                    did_god_conduct(DID_KILL_CHAOTIC,
                                    mons->get_experience_level(), true, mons);
                }

                // jmf: Trog hates wizards.
                if (mons->is_actual_spellcaster())
                {
                    did_god_conduct(DID_KILL_WIZARD,
                                    mons->get_experience_level(), true, mons);
                }

                // Beogh hates priests of other gods.
                if (mons->is_priest())
                {
                    did_god_conduct(DID_KILL_PRIEST,
                                    mons->get_experience_level(), true, mons);
                }

                // Jiyva hates you killing slimes, but eyeballs
                // mutation can confuse without you meaning it.
                if (mons_is_slime(mons) && killer != KILL_YOU_CONF && bad_kill)
                {
                    did_god_conduct(DID_KILL_SLIME, mons->get_experience_level(),
                                    true, mons);
                }

                if (fedhas_protects(mons))
                {
                    did_god_conduct(DID_KILL_PLANT, mons->get_experience_level(),
                                    true, mons);
                }

                // Cheibriados hates fast monsters.
                if (cheibriados_thinks_mons_is_fast(mons)
                    && !mons->cannot_move())
                {
                    did_god_conduct(DID_KILL_FAST, mons->get_experience_level(),
                                    true, mons);
                }

                // Yredelemnul hates artificial beings.
                if (mons->is_artificial())
                {
                    did_god_conduct(DID_KILL_ARTIFICIAL, mons->get_experience_level(),
                                    true, mons);
                }

                // Holy kills are always noticed.
                if (mons->is_holy())
                {
                    did_god_conduct(DID_KILL_HOLY, mons->get_experience_level(),
                                    true, mons);
                }

                // Dithmenos hates sources of fire.
                // (This is *after* the holy so that the right order of
                //  messages appears.)
                if (mons_is_fiery(mons))
                {
                    did_god_conduct(DID_KILL_FIERY, mons->get_experience_level(),
                                    true, mons);
                }
            }

            // Divine health and mana restoration doesn't happen when
            // killing born-friendly monsters.
            if (good_kill
                && (you_worship(GOD_MAKHLEB)
                    || you_worship(GOD_VEHUMET)
                    || you_worship(GOD_SHINING_ONE)
                       && (mons->is_evil() || mons->is_unholy()))
                && !mons_is_object(mons->type)
                && !player_under_penance()
                && random2(you.piety) >= piety_breakpoint(0))
            {
                int hp_heal = 0, mp_heal = 0;

                switch (you.religion)
                {
                case GOD_MAKHLEB:
                    hp_heal = mons->get_experience_level()
                              + random2(mons->get_experience_level());
                    break;
                case GOD_SHINING_ONE:
                    hp_heal = random2(1 + 2 * mons->get_experience_level());
                    mp_heal = random2(2 + mons->get_experience_level() / 3);
                    break;
                case GOD_VEHUMET:
                    mp_heal = 1 + random2(mons->get_experience_level() / 2);
                    break;
                default:
                    die("bad kill-on-healing god!");
                }

#if TAG_MAJOR_VERSION == 34
                if (you.species == SP_DJINNI)
                    hp_heal = max(hp_heal, mp_heal * 2), mp_heal = 0;
#endif
                if (hp_heal && you.hp < you.hp_max
                    && !you.duration[DUR_DEATHS_DOOR])
                {
                    mpr("You feel a little better.");
                    inc_hp(hp_heal);
                }

                if (mp_heal && you.magic_points < you.max_magic_points)
                {
                    mpr("You feel your power returning.");
                    inc_mp(mp_heal);
                }
            }

            if (good_kill && you_worship(GOD_RU) && you.piety < 200
                    && one_chance_in(2))
            {
                ASSERT(you.props.exists("ru_progress_to_next_sacrifice"));
                ASSERT(you.props.exists("available_sacrifices"));
                int sacrifice_count =
                    you.props["available_sacrifices"].get_vector().size();
                if (sacrifice_count == 0)
                {
                    int current_progress =
                            you.props["ru_progress_to_next_sacrifice"]
                                .get_int();
                    you.props["ru_progress_to_next_sacrifice"] =
                            current_progress + 1;
                }
            }

            // Randomly bless a follower.
            if (!created_friendly
                && gives_xp
                && in_good_standing(GOD_BEOGH)
                && random2(you.piety) >= piety_breakpoint(2)
                && !mons_is_object(mons->type))
            {
                bless_follower();
            }

            if (you.duration[DUR_DEATH_CHANNEL] && gives_xp)
                _make_spectral_thing(mons, !death_message);
            break;
        }

        case KILL_MON:          // Monster kills in combat.
        case KILL_MON_MISSILE:  // Monster kills by missile or beam.
            if (death_message)
            {
                const char* msg =
                    exploded                     ? " is blown up!" :
                    wounded_damaged(targ_holy)  ? " is destroyed!"
                                                 : " dies!";
                simple_monster_message(mons, msg, MSGCH_MONSTER_DAMAGE,
                                       MDAM_DEAD);
            }

            if (crawl_state.game_is_arena())
                break;

            // No piety loss if god gifts killed by other monsters.
            // Also, dancing weapons aren't really friendlies.
            if (mons->friendly() && !mons_is_object(mons->type))
            {
                const int mon_intel = mons_class_intel(mons->type) - I_ANIMAL;

                did_god_conduct(mon_intel > 0 ? DID_SOULED_FRIEND_DIED
                                              : DID_FRIEND_DIED,
                                1 + (mons->get_experience_level() / 2),
                                true, mons);
            }

            if (pet_kill && fedhas_protects(mons))
            {
                did_god_conduct(DID_PLANT_KILLED_BY_SERVANT,
                                1 + (mons->get_experience_level() / 2),
                                true, mons);
            }

            // Trying to prevent summoning abuse here, so we're trying to
            // prevent summoned creatures from being done_good kills.  Only
            // affects creatures which were friendly when summoned.
            if (!created_friendly && gives_xp && pet_kill
                && (anon || !invalid_monster_index(killer_index)))
            {
                monster* killer_mon = NULL;
                if (!anon)
                {
                    killer_mon = &menv[killer_index];

                    // If the killer is already dead, treat it like an
                    // anonymous monster.
                    if (killer_mon->type == MONS_NO_MONSTER)
                        anon = true;
                }

                const mon_holy_type killer_holy =
                    anon ? MH_NATURAL : killer_mon->holiness();

                if (you_worship(GOD_ZIN)
                    || you_worship(GOD_SHINING_ONE)
                    || you_worship(GOD_YREDELEMNUL)
                    || you_worship(GOD_KIKUBAAQUDGHA)
                    || you_worship(GOD_MAKHLEB)
                    || you_worship(GOD_LUGONU)
                    || you_worship(GOD_QAZLAL)
                    || !anon && mons_is_god_gift(killer_mon))
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, these are hacks, but they make sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that e.g. Yredelemnul ignores kills done
                        // by confused monsters as opposed to enslaved
                        // or friendly ones. (jpeg)
                        if (targ_holy == MH_DEMONIC
                            || mons_is_demonspawn(mons->type))
                        {
                            did_god_conduct(
                                !confused ? DID_DEMON_KILLED_BY_UNDEAD_SLAVE :
                                            DID_DEMON_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }
                        else if (targ_holy == MH_NATURAL)
                        {
                            did_god_conduct(
                                !confused ? DID_LIVING_KILLED_BY_UNDEAD_SLAVE :
                                            DID_LIVING_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }
                        else if (targ_holy == MH_UNDEAD)
                        {
                            did_god_conduct(
                                !confused ? DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE :
                                            DID_UNDEAD_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }

                        if (mons->how_unclean())
                        {
                            did_god_conduct(DID_UNCLEAN_KILLED_BY_SERVANT,
                                            mons->get_experience_level());
                        }

                        if (mons->how_chaotic())
                        {
                            did_god_conduct(DID_CHAOTIC_KILLED_BY_SERVANT,
                                            mons->get_experience_level());
                        }

                        if (mons->is_artificial())
                        {
                            did_god_conduct(
                                !confused ? DID_ARTIFICIAL_KILLED_BY_UNDEAD_SLAVE :
                                            DID_ARTIFICIAL_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }
                    }
                    // Yes, we are splitting undead pets from the others
                    // as a way to focus Necromancy vs. Summoning
                    // (ignoring Haunt here)... at least we're being
                    // nice and putting the natural creature summons
                    // together with the demonic ones.  Note that
                    // Vehumet gets a free pass here since those
                    // followers are assumed to come from summoning
                    // spells...  the others are from invocations (TSO,
                    // Makhleb, Kiku). - bwr
                    else if (targ_holy == MH_DEMONIC
                             || mons_is_demonspawn(mons->type))
                    {
                        did_god_conduct(DID_DEMON_KILLED_BY_SERVANT,
                                        mons->get_experience_level());
                    }
                    else if (targ_holy == MH_NATURAL)
                    {
                        did_god_conduct(DID_LIVING_KILLED_BY_SERVANT,
                                        mons->get_experience_level());

                        // TSO hates natural evil and unholy beings.
                        if (mons->is_unholy())
                        {
                            did_god_conduct(
                                DID_NATURAL_UNHOLY_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }
                        else if (mons->is_evil())
                        {
                            did_god_conduct(DID_NATURAL_EVIL_KILLED_BY_SERVANT,
                                mons->get_experience_level());
                        }
                    }
                    else if (targ_holy == MH_UNDEAD)
                    {
                        did_god_conduct(DID_UNDEAD_KILLED_BY_SERVANT,
                                        mons->get_experience_level());
                    }

                    if (mons->how_unclean())
                    {
                        did_god_conduct(DID_UNCLEAN_KILLED_BY_SERVANT,
                                        mons->get_experience_level());
                    }

                    if (mons->how_chaotic())
                    {
                        did_god_conduct(DID_CHAOTIC_KILLED_BY_SERVANT,
                                        mons->get_experience_level());
                    }

                    if (mons->is_artificial())
                    {
                        did_god_conduct(DID_ARTIFICIAL_KILLED_BY_SERVANT,
                                        mons->get_experience_level());
                    }
                }

                // Holy kills are always noticed.
                if (mons->is_holy())
                {
                    if (killer_holy == MH_UNDEAD)
                    {
                        const bool confused =
                            anon ? false : !killer_mon->friendly();

                        // Yes, this is a hack, but it makes sure that
                        // confused monsters doing kills are not
                        // referred to as "slaves", and I think it's
                        // okay that Yredelemnul ignores kills done by
                        // confused monsters as opposed to enslaved or
                        // friendly ones. (jpeg)
                        did_god_conduct(
                            !confused ? DID_HOLY_KILLED_BY_UNDEAD_SLAVE :
                                        DID_HOLY_KILLED_BY_SERVANT,
                            mons->get_experience_level(), true, mons);
                    }
                    else
                    {
                        did_god_conduct(DID_HOLY_KILLED_BY_SERVANT,
                                        mons->get_experience_level(),
                                        true, mons);
                    }
                }

                if (in_good_standing(GOD_SHINING_ONE)
                    && (mons->is_evil() || mons->is_unholy())
                    && random2(you.piety) >= piety_breakpoint(0)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    if (!one_chance_in(3) && killer_mon->alive()
                        && bless_follower(killer_mon))
                    {
                        break;
                    }

                    if (killer_mon->alive()
                        && killer_mon->hit_points < killer_mon->max_hit_points)
                    {
                        simple_monster_message(killer_mon,
                                               " looks invigorated.");
                        killer_mon->heal(1 + random2(mons->get_experience_level() / 4));
                    }
                }

                if (in_good_standing(GOD_BEOGH)
                    && random2(you.piety) >= piety_breakpoint(2)
                    && !one_chance_in(3)
                    && !invalid_monster_index(killer_index))
                {
                    // Randomly bless the follower who killed.
                    bless_follower(killer_mon);
                }

                if (you.duration[DUR_DEATH_CHANNEL] && gives_xp && was_visible)
                    _make_spectral_thing(mons, !death_message);
            }
            break;

        // Monster killed by trap/inanimate thing/itself/poison not from you.
        case KILL_MISC:
        case KILL_MISCAST:
            if (death_message)
            {
                if (fake_abjuration)
                {
                    if (mons_genus(mons->type) == MONS_SNAKE)
                    {
                        // Sticks to Snake
                        simple_monster_message(mons, " withers and dies!",
                            MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
                    }
                    else if (mons->type == MONS_SPECTRAL_THING)
                    {
                        // Death Channel
                        simple_monster_message(mons, " fades into mist!");
                    }
                    else
                    {
                        string msg = " " + summoned_poof_msg(mons) + "!";
                        simple_monster_message(mons, msg.c_str());
                    }
                }
                else
                {
                    const char* msg =
                        exploded                     ? " is blown up!" :
                        wounded_damaged(targ_holy)  ? " is destroyed!"
                                                     : " dies!";
                    simple_monster_message(mons, msg, MSGCH_MONSTER_DAMAGE,
                                           MDAM_DEAD);
                }
            }
            break;

        case KILL_BANISHED:
            // Monster doesn't die, just goes back to wherever it came from.
            // This must only be called by monsters running out of time (or
            // abjuration), because it uses the beam variables!  Or does it???
            // Pacified monsters leave the level when this happens.

            // KILL_RESET monsters no longer lose their whole inventory, only
            // items they were generated with.
            if (mons->pacified() || !mons->needs_abyss_transit())
            {
                // A banished monster that doesn't go on the transit list
                // loses all items.
                if (!mons->is_summoned())
                    drop_items = false;
                break;
            }

            // Monster goes to the Abyss.
            mons->flags |= MF_BANISHED;
            {
                unwind_var<int> dt(mons->damage_total, 0);
                unwind_var<int> df(mons->damage_friendly, 0);
                mons->set_transit(level_id(BRANCH_ABYSS));
            }
            set_unique_annotation(mons, BRANCH_ABYSS);
            in_transit = true;
            drop_items = false;
            mons->firing_pos.reset();
            // Make monster stop patrolling and/or travelling.
            mons->patrol_point.reset();
            mons->travel_path.clear();
            mons->travel_target = MTRAV_NONE;
            break;

        case KILL_RESET:
            drop_items = false;
            break;

        case KILL_TIMEOUT:
        case KILL_UNSUMMONED:
        case KILL_DISMISSED:
            break;

        default:
            drop_items = false;
            break;
    }

    // Make sure Boris has a foe to address.
    if (mons->foe == MHITNOT)
    {
        if (!mons->wont_attack() && !crawl_state.game_is_arena())
            mons->foe = MHITYOU;
        else if (!invalid_monster_index(killer_index))
            mons->foe = killer_index;
    }

    // Make sure that the monster looks dead.
    if (mons->alive() && (!summoned || duration > 0))
    {
        dprf("Granting xp for non-damage kill.");
        if (YOU_KILL(killer))
            mons->damage_friendly += mons->hit_points * 2;
        else if (pet_kill)
            mons->damage_friendly += mons->hit_points;
        mons->damage_total += mons->hit_points;

        if (!in_transit) // banishment only
            mons->hit_points = -1;
    }

    if (!silent && !wizard && you.see_cell(mons->pos()))
    {
        // Make sure that the monster looks dead.
        if (mons->alive() && !in_transit && (!summoned || duration > 0))
            mons->hit_points = -1;
        // Hack: with cleanup_dead=false, a tentacle [segment] of a dead
        // [malign] kraken has no valid head reference.
        if (!mons_is_tentacle_or_tentacle_segment(mons->type))
            mons_speaks(mons);
    }

    if (mons->type == MONS_BORIS && !in_transit && !mons->pacified())
    {
        // XXX: Actual blood curse effect for Boris? - bwr

        // Now that Boris is dead, he's a valid target for monster
        // creation again. - bwr
        you.unique_creatures.set(mons->type, false);
        // And his vault can be placed again.
        you.uniq_map_names.erase("uniq_boris");
    }
    if (mons->type == MONS_JORY && !in_transit)
        blood_spray(mons->pos(), MONS_JORY, 50);
    else if (mons_is_kirke(mons)
             && !in_transit
             && !testbits(mons->flags, MF_WAS_NEUTRAL))
    {
        hogs_to_humans();
    }
    else if ((mons_is_natasha(mons) || mons_genus(mons->type) == MONS_FELID)
             && !in_transit && !mons->pacified() && mons_felid_can_revive(mons))
    {
        drop_items = false;

        // Like Boris, but her vault can't come back
        if (mons_is_natasha(mons))
            you.unique_creatures.set(MONS_NATASHA, false);
        mons_felid_revive(mons);
    }
    else if (mons_is_pikel(mons))
    {
        // His slaves don't care if he's dead or not, just whether or not
        // he goes away.
        pikel_band_neutralise();
    }
    else if (mons->is_named() && mons->friendly() && killer != KILL_RESET)
        take_note(Note(NOTE_ALLY_DEATH, 0, 0, mons->mname.c_str()));
    else if (mons_is_tentacle_head(mons_base_type(mons)))
    {
        if (destroy_tentacles(mons)
            && !in_transit
            && you.see_cell(mons->pos()))
        {
            if (mons_base_type(mons) == MONS_KRAKEN)
                mpr("The dead kraken's tentacles slide back into the water.");
            else if (mons->type == MONS_TENTACLED_STARSPAWN)
                mpr("The starspawn's tentacles wither and die.");
        }
    }
    else if (mons_is_tentacle_or_tentacle_segment(mons->type)
             && killer != KILL_MISC
                 || mons->type == MONS_ELDRITCH_TENTACLE
                 || mons->type == MONS_SNAPLASHER_VINE)
    {
        if (mons->type == MONS_SNAPLASHER_VINE)
        {
            if (mons->props.exists("vine_awakener"))
            {
                monster* awakener =
                        monster_by_mid(mons->props["vine_awakener"].get_int());
                if (awakener)
                    awakener->props["vines_awakened"].get_int()--;
            }
        }
        destroy_tentacle(mons);
    }
    else if (mons->type == MONS_ELDRITCH_TENTACLE_SEGMENT
             && killer != KILL_MISC)
    {
        if (!invalid_monster_index(mons->number)
             && mons_base_type(&menv[mons->number]) == MONS_ELDRITCH_TENTACLE
             && menv[mons->number].alive())
        {
            monster_die(&menv[mons->number], killer, killer_index, silent,
                        wizard, fake);
        }
    }
    else if (mons_is_elven_twin(mons))
        elven_twin_died(mons, in_transit, killer, killer_index);
    else if (mons->type == MONS_VAULT_WARDEN)
        timeout_terrain_changes(0, true);
    else if (mons->type == MONS_FLAYED_GHOST)
        end_flayed_effect(mons);
    // Give the treant a last chance to release its wasps if it is killed in a
    // single blow from above half health
    else if (mons->type == MONS_SHAMBLING_MANGROVE && !was_banished
             && !mons->pacified() && (!summoned || duration > 0) && !wizard
             && !mons_reset)
    {
        treant_release_fauna(mons);
    }
    else if (mons->type == MONS_BENNU && !in_transit && !was_banished
             && !mons_reset && !mons->pacified()
             && (!summoned || duration > 0) && !wizard
             && mons_bennu_can_revive(mons))
    {
        mons_bennu_revive(mons);
    }
    else if (!mons->is_summoned())
    {
        if (mons_genus(mons->type) == MONS_MUMMY)
            _mummy_curse(mons, killer, killer_index);
    }

    if (mons->mons_species() == MONS_BALLISTOMYCETE)
    {
        _activate_ballistomycetes(mons, mons->pos(),
                                 YOU_KILL(killer) || pet_kill);
    }

    if (!wizard && !submerged && !was_banished)
    {
        _monster_die_cloud(mons, !mons_reset && !fake_abjuration && !unsummoned
                                 && !timeout, silent, summoned);
    }

    int corpse = -1;
    if (!mons_reset && !summoned && !fake_abjuration && !unsummoned
        && !timeout && !was_banished)
    {
        // Have to add case for disintegration effect here? {dlb}
        int corpse2 = -1;

        if (mons->type == MONS_SPRIGGAN_RIDER)
        {
            corpse2 = mounted_kill(mons, MONS_YELLOW_WASP, killer, killer_index);
            mons->type = MONS_SPRIGGAN;
        }
        corpse = place_monster_corpse(mons, silent);
        if (corpse == -1)
            corpse = corpse2;
    }

    if ((killer == KILL_YOU
         || killer == KILL_YOU_MISSILE
         || killer == KILL_YOU_CONF
         || pet_kill)
             && corpse >= 0
             && mitm[corpse].base_type != OBJ_GOLD
             && player_mutation_level(MUT_POWERED_BY_DEATH))
    {
        const int pbd_dur = player_mutation_level(MUT_POWERED_BY_DEATH) * 8
                            + roll_dice(2, 8);
        if (pbd_dur > you.duration[DUR_POWERED_BY_DEATH])
            you.set_duration(DUR_POWERED_BY_DEATH, pbd_dur);
    }

    if (corpse >= 0 && mitm[corpse].base_type != OBJ_GOLD)
    {
        // Powered by death.
        // Find nearby putrid demonspawn.
        for (monster_near_iterator mi(mons->pos()); mi; ++mi)
        {
            monster* mon = *mi;
            if (mon->alive()
                && mons_is_demonspawn(mon->type)
                && draco_or_demonspawn_subspecies(mon)
                   == MONS_PUTRID_DEMONSPAWN)
            {
                // Rather than regen over time, the expected 24 + 2d8 duration
                // is given as an instant health bonus.
                // These numbers may need to be adjusted.
                if (mon->heal(random2avg(24, 2) + roll_dice(2, 8)))
                {
                    simple_monster_message(mon,
                                           " regenerates before your eyes!");
                }
            }
        }
    }

    unsigned int player_exp = 0, monster_exp = 0;
    if (!mons_reset && !fake_abjuration && !timeout && !unsummoned)
    {
        player_exp = _calc_player_experience(mons);
        monster_exp = _calc_monster_experience(mons, killer, killer_index);
    }

    if (!mons_reset && !fake_abjuration && !crawl_state.game_is_arena()
        && !unsummoned && !timeout && !in_transit)
    {
        you.kills->record_kill(mons, killer, pet_kill);
    }

    if (fake)
    {
        if (corpse != -1 && _reaping(mons))
            corpse = -1;

        _give_experience(player_exp, monster_exp, killer, killer_index,
                         pet_kill, was_visible);
        crawl_state.dec_mon_acting(mons);

        return corpse;
    }

    mons_remove_from_grid(mons);
    fire_monster_death_event(mons, killer, killer_index, false);

    if (crawl_state.game_is_arena())
        arena_monster_died(mons, killer, killer_index, silent, corpse);

    // Monsters haloes should be removed when they die.
    if (mons->halo_radius2()
        || mons->umbra_radius2()
        || mons->silence_radius2())
    {
        invalidate_agrid();
    }

    const coord_def mwhere = mons->pos();
    if (drop_items)
        monster_drop_things(mons, YOU_KILL(killer) || pet_kill);
    else
    {
        // Destroy the items belonging to MF_HARD_RESET monsters so they
        // don't clutter up mitm[].
        mons->destroy_inventory();
    }

    if (!silent && !wizard && !mons_reset && corpse != -1
        && !fake_abjuration
        && !timeout
        && !unsummoned
        && !(mons->flags & MF_KNOWN_SHIFTER)
        && mons->is_shapeshifter())
    {
        simple_monster_message(mons, "'s shape twists and changes as "
                               "it dies.");
    }

    if (mons->is_divine_companion()
        && killer != KILL_RESET
        && !(mons->flags & MF_BANISHED))
    {
        remove_companion(mons);
    }

    // If we kill an invisible monster reactivate autopickup.
    // We need to check for actual invisibility rather than whether we
    // can see the monster. There are several edge cases where a monster
    // is visible to the player but we still need to turn autopickup
    // back on, such as TSO's halo or sticky flame. (jpeg)
    if (mons_near(mons) && mons->has_ench(ENCH_INVIS))
        autotoggle_autopickup(false);

    if (corpse != -1 && _reaping(mons))
        corpse = -1;

    crawl_state.dec_mon_acting(mons);
    monster_cleanup(mons);

    // Force redraw for monsters that die.
    if (in_bounds(mwhere) && you.see_cell(mwhere))
    {
        view_update_at(mwhere);
        update_screen();
    }

    if (gives_xp)
    {
        _give_experience(player_exp, monster_exp, killer, killer_index,
                         pet_kill, was_visible);
    }

    return corpse;
}

void unawaken_vines(const monster* mons, bool quiet)
{
    int vines_seen = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SNAPLASHER_VINE
            && mi->props.exists("vine_awakener")
            && monster_by_mid(mi->props["vine_awakener"].get_int()) == mons)
        {
            if (you.can_see(*mi))
                ++vines_seen;
            monster_die(*mi, KILL_RESET, NON_MONSTER);
        }
    }

    if (!quiet && vines_seen)
    {
        mprf("The vine%s fall%s limply to the ground.",
              (vines_seen > 1 ? "s" : ""), (vines_seen == 1 ? "s" : ""));
    }
}

void heal_flayed_effect(actor* act, bool quiet, bool blood_only)
{
    if (!blood_only)
    {
        if (act->is_player())
            you.duration[DUR_FLAYED] = 0;
        else
            act->as_monster()->del_ench(ENCH_FLAYED, true, false);

        if (you.can_see(act) && !quiet)
        {
            mprf("The terrible wounds on %s body vanish.",
                 act->name(DESC_ITS).c_str());
        }

        act->heal(act->props["flay_damage"].get_int());
        act->props.erase("flay_damage");
    }

    CrawlVector &blood = act->props["flay_blood"].get_vector();

    for (int i = 0; i < blood.size(); ++i)
        env.pgrid(blood[i].get_coord()) &= ~FPROP_BLOODY;
    act->props.erase("flay_blood");
}

void end_flayed_effect(monster* ghost)
{
    if (you.duration[DUR_FLAYED] && !ghost->wont_attack())
        heal_flayed_effect(&you);

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->has_ench(ENCH_FLAYED) && !mons_aligned(ghost, *mi))
            heal_flayed_effect(*mi);
    }
}

// Clean up after a dead monster.
void monster_cleanup(monster* mons)
{
    crawl_state.mon_gone(mons);

    if (mons->has_ench(ENCH_AWAKEN_FOREST))
    {
        forest_message(mons->pos(), "The forest abruptly stops moving.");
        env.forest_awoken_until = 0;
    }

    if (mons->has_ench(ENCH_AWAKEN_VINES))
        unawaken_vines(mons, false);

    // So that a message is printed for the effect ending
    if (mons->has_ench(ENCH_CONTROL_WINDS))
        mons->del_ench(ENCH_CONTROL_WINDS);

    // So proper messages are printed
    if (mons->has_ench(ENCH_GRASPING_ROOTS_SOURCE))
        mons->del_ench(ENCH_GRASPING_ROOTS_SOURCE);

    // May have been constricting something. No message because that depends
    // on the order in which things are cleaned up: If the constrictee is
    // cleaned up first, we wouldn't get a message anyway.
    mons->stop_constricting_all(false, true);

    if (mons_is_tentacle_head(mons_base_type(mons)))
        destroy_tentacles(mons);

    env.mid_cache.erase(mons->mid);
    unsigned int monster_killed = mons->mindex();
    mons->reset();

    for (monster_iterator mi; mi; ++mi)
        if (mi->foe == monster_killed)
            mi->foe = MHITNOT;

    if (you.pet_target == monster_killed)
        you.pet_target = MHITNOT;
}

int mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                int killer_index)
{
    monster mon;
    mon.type = mc;
    mon.moveto(daddy->pos());
    define_monster(&mon);
    mon.flags = daddy->flags;
    mon.attitude = daddy->attitude;
    mon.damage_friendly = daddy->damage_friendly;
    mon.damage_total = daddy->damage_total;
    // Keep the rider's name, if it had one (Mercenary card).
    if (!daddy->mname.empty() && mon.type == MONS_SPRIGGAN)
        mon.mname = daddy->mname;
    if (daddy->props.exists("reaping_damage"))
    {
        dprf("Mounted kill: marking the other monster as reaped as well.");
        mon.props["reaping_damage"].get_int() = daddy->props["reaping_damage"].get_int();
        mon.props["reaper"].get_int() = daddy->props["reaper"].get_int();
    }

    return monster_die(&mon, killer, killer_index, false, false, true);
}

/**
 * Applies harmful environmental effects from the current tile to monsters.
 *
 * @param mons      The monster to maybe drown/incinerate.
 * @param oldpos    Their previous tile, before landing up here.
 * @param killer    Who's responsible for killing them, if they die here.
 * @param killnum   Not sure; *probably* the mindex of the killer, if any?
 */
void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer, int killnum)
{
    // Flying/clinging monsters don't make contact with the terrain.
    if (!mons->ground_level())
        return;

    dungeon_feature_type grid = grd(mons->pos());
    if (grid != DNGN_LAVA && grid != DNGN_DEEP_WATER
        || monster_habitable_grid(mons, grid))
    {
        return;
    }


    // Don't worry about invisibility.  You should be able to see if
    // something has fallen into the lava.
    if (mons_near(mons) && (oldpos == mons->pos() || grd(oldpos) != grid))
    {
         mprf("%s falls into the %s!",
             mons->name(DESC_THE).c_str(),
             grid == DNGN_LAVA ? "lava" : "water");
    }

    // Even fire resistant monsters perish in lava.
    if (grid == DNGN_LAVA && mons->res_fire() < 2)
    {
        simple_monster_message(mons, " is incinerated.",
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else if (mons->can_drown())
    {
        simple_monster_message(mons, " drowns.",
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }
    else
    {
        simple_monster_message(mons, " falls apart.",
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
    }

    if (killer == KILL_NONE)
    {
        // Self-kill.
        killer  = KILL_MON;
        killnum = mons->mindex();
    }

    // Yredelemnul special, redux: It's the only one that can
    // work on drowned monsters.
    if (!_yred_enslave_soul(mons, killer))
        monster_die(mons, killer, killnum, true);
}

// Make all of the monster's original equipment disappear, unless it's a fixed
// artefact or unrand artefact.
static void _vanish_orig_eq(monster* mons)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (mons->inv[i] == NON_ITEM)
            continue;

        item_def &item(mitm[mons->inv[i]]);

        if (!item.defined())
            continue;

        if (origin_known(item) || item.orig_monnum != 0
            || !item.inscription.empty()
            || is_unrandom_artefact(item)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN
                              | ISFLAG_NOTED_GET)))
        {
            continue;
        }
        item.flags |= ISFLAG_SUMMONED;
    }
}

int dismiss_monsters(string pattern)
{
    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    const bool keep_item = strip_tag(pattern, "keepitem");
    const bool harmful = pattern == "harmful";
    const bool mobile  = pattern == "mobile";

    // Dismiss by regex.
    text_pattern tpat(pattern);
    int ndismissed = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->alive()
            && (mobile ? !mons_class_is_stationary(mi->type) :
                harmful ? !mons_is_firewood(*mi) && !mi->wont_attack()
                : tpat.empty() || tpat.matches(mi->name(DESC_PLAIN, true))))
        {
            if (!keep_item)
                _vanish_orig_eq(*mi);
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER, false, true);
            ++ndismissed;
        }
    }

    return ndismissed;
}

string summoned_poof_msg(const monster* mons, bool plural)
{
    int  summon_type = 0;
    bool valid_mon   = false;
    if (mons != NULL && !invalid_monster(mons))
    {
        (void) mons->is_summoned(NULL, &summon_type);
        valid_mon = true;
    }

    string msg      = "disappear%s in a puff of smoke";
    bool   no_chaos = false;

    switch (summon_type)
    {
    case SPELL_SHADOW_CREATURES:
    case SPELL_WEAVE_SHADOWS:
    case MON_SUMM_SCROLL:
        msg      = "dissolve%s into shadows";
        no_chaos = true;
        break;

    case MON_SUMM_CHAOS:
        msg = "degenerate%s into a cloud of primal chaos";
        break;

    case MON_SUMM_WRATH:
    case MON_SUMM_AID:
        if (valid_mon && is_good_god(mons->god))
        {
            msg      = "dissolve%s into sparkling lights";
            no_chaos = true;
        }
        break;

    case SPELL_GHOSTLY_FLAMES:
    case SPELL_CALL_LOST_SOUL:
        msg = "fades away";
        break;
    }

    if (valid_mon)
    {
        if (mons->god == GOD_XOM && !no_chaos && one_chance_in(10)
            || mons->type == MONS_CHAOS_SPAWN)
        {
            msg = "degenerate%s into a cloud of primal chaos";
        }

        if (mons->is_holy()
            && summon_type != SPELL_SHADOW_CREATURES
            && summon_type != MON_SUMM_CHAOS)
        {
            msg = "dissolve%s into sparkling lights";
        }

        if (mons_is_slime(mons)
            && mons->god == GOD_JIYVA)
        {
            msg = "dissolve%s into a puddle of slime";
        }

        if (mons->type == MONS_DROWNED_SOUL)
            msg = "returns to the deep";

        if (mons->has_ench(ENCH_PHANTOM_MIRROR))
            msg = "shimmers and vanishes";
    }

    // Conjugate.
    msg = make_stringf(msg.c_str(), plural ? "" : "s");

    return msg;
}

string summoned_poof_msg(const int midx, const item_def &item)
{
    if (midx == NON_MONSTER)
        return summoned_poof_msg(static_cast<const monster* >(NULL), item);
    else
        return summoned_poof_msg(&menv[midx], item);
}

string summoned_poof_msg(const monster* mons, const item_def &item)
{
    ASSERT(item.flags & ISFLAG_SUMMONED);

    return summoned_poof_msg(mons, item.quantity > 1);
}

/**
 * Determine if a specified monster is Pikel.
 *
 * Checks both the monster type and the "original_name" prop, thus allowing
 * Pikelness to be transferred through polymorph.
 *
 * @param mons    The monster to be checked.
 * @return        True if the monster is Pikel, otherwise false.
**/
bool mons_is_pikel(monster* mons)
{
    return mons->type == MONS_PIKEL
           || (mons->props.exists("original_name")
               && mons->props["original_name"].get_string() == "Pikel");
}

/**
 * Perform neutralisation for members of Pikel's band upon Pikel's 'death'.
 *
 * This neutralisation occurs in multiple instances: when Pikel is neutralised,
 * enslaved, when Pikel dies, when Pikel is banished.
 * It is handled by a daction (as a fineff) to preserve across levels.
 **/
void pikel_band_neutralise()
{
    int visible_slaves = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SLAVE
            && testbits(mi->flags, MF_BAND_MEMBER)
            && mi->props.exists("pikel_band")
            && mi->mname != "freed slave"
            && mi->observable())
        {
            visible_slaves++;
        }
    }
    const char* final_msg = nullptr;
    if (visible_slaves == 1)
        final_msg = "With Pikel's spell broken, the former slave thanks you for freedom.";
    else if (visible_slaves > 1)
        final_msg = "With Pikel's spell broken, the former slaves thank you for their freedom.";

    delayed_action_fineff::schedule(DACT_PIKEL_SLAVES, final_msg);
}

/**
 * Determine if a monster is Kirke.
 *
 * As with mons_is_pikel, tracks Kirke via type and original name, thus allowing
 * tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @return        True if Kirke, false otherwise.
**/
bool mons_is_kirke(monster* mons)
{
    return mons->type == MONS_KIRKE
           || (mons->props.exists("original_name")
               && mons->props["original_name"].get_string() == "Kirke");
}

/**
 * Revert porkalated hogs.
 *
 * Called upon Kirke's death. Hogs either from Kirke's band or
 * subsequently porkalated should be reverted to their original form.
 * This takes place as a daction to preserve behaviour across levels;
 * this function simply checks if any are visible and raises a fineff
 * containing an appropriate message. The fineff raises the actual
 * daction.
 */
void hogs_to_humans()
{
    int any = 0, human = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_genus(mi->type) != MONS_HOG)
            continue;

        if (!mi->props.exists("kirke_band")
            && !mi->props.exists(ORIG_MONSTER_KEY))
        {
            continue;
        }

        // Shapeshifters will stop being a hog when they feel like it.
        if (mi->is_shapeshifter())
            continue;

        const bool could_see = you.can_see(*mi);

        if (could_see) any++;

        if (!mi->props.exists(ORIG_MONSTER_KEY) && could_see)
            human++;
    }

    const char* final_msg = nullptr;

    if (any == 1)
    {
        if (any == human)
            final_msg = "No longer under Kirke's spell, the hog turns into a human!";
        else
            final_msg = "No longer under Kirke's spell, the hog returns to its "
                        "original form!";
    }
    else if (any > 1)
    {
        if (any == human)
        {
            final_msg = "No longer under Kirke's spell, the hogs revert to their "
                        "human forms!";
        }
        else
            final_msg = "No longer under Kirke's spell, the hogs revert to their "
                        "original forms!";
    }

    kirke_death_fineff::schedule(final_msg);
}

/**
 * Determine if a monster is Dowan.
 *
 * Tracks through type and original_name, thus tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @return        True if Dowan, otherwise false.
**/
bool mons_is_dowan(const monster* mons)
{
    return mons->type == MONS_DOWAN
           || (mons->props.exists("original_name")
               && mons->props["original_name"].get_string() == "Dowan");
}

/**
 * Determine if a monster is Duvessa.
 *
 * Tracks through type and original_name, thus tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @return        True if Duvessa, otherwise false.
**/
bool mons_is_duvessa(const monster* mons)
{
    return mons->type == MONS_DUVESSA
           || (mons->props.exists("original_name")
               && mons->props["original_name"].get_string() == "Duvessa");
}

/**
 * Determine if a monster is either Dowan or Duvessa.
 *
 * Tracks through type and original_name, thus tracking through polymorph. A
 * wrapper around mons_is_dowan and mons_is_duvessa. Used to determine if a
 * death function should be called for the monster in question.
 *
 * @param mons    The monster to check.
 * @return        True if either Dowan or Duvessa, otherwise false.
**/
bool mons_is_elven_twin(const monster* mons)
{
    return mons_is_dowan(mons) || mons_is_duvessa(mons);
}

/**
 * Perform functional changes Dowan or Duvessa upon the other's death.
 *
 * This functional is called when either Dowan or Duvessa are killed or
 * banished. It performs a variety of changes in both attitude, spells, flavour,
 * speech, etc.
 *
 * @param twin          The monster who died.
 * @param in_transit    True if banished, otherwise false.
 * @param killer        The kill-type related to twin.
 * @param killer_index  The index of the actor who killed twin.
**/
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index)
{
    // Sometimes, if you pacify one twin near a staircase, they leave
    // in the same turn. Convert, in those instances.
    if (twin->neutral())
    {
        elven_twins_pacify(twin);
        return;
    }

    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if ((*mi)->good_neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    // Okay, let them climb stairs now.
    mons->props["can_climb"] = true;
    if (!in_transit)
        mons->props["speech_prefix"] = "twin_died";
    else
        mons->props["speech_prefix"] = "twin_banished";

    // If you've stabbed one of them, the other one is likely asleep still.
    if (mons->asleep())
        behaviour_event(mons, ME_DISTURB, 0, mons->pos());

    // Will generate strings such as 'Duvessa_Duvessa_dies' or, alternately
    // 'Dowan_Dowan_dies', but as neither will match, these can safely be
    // ignored.
    string key = mons->name(DESC_THE, true) + "_"
                 + twin->name(DESC_THE, true) + "_dies_";

    if (mons_near(mons) && !mons->observable())
        key += "invisible_";
    else if (!mons_near(mons))
        key += "distance_";

    bool i_killed = ((killer == KILL_MON || killer == KILL_MON_MISSILE)
                      && mons->mindex() == killer_index);

    if (i_killed)
    {
        key += "bytwin_";
        mons->props["speech_prefix"] = "twin_ikilled";
    }

    // Drop the final '_'.
    key.erase(key.length() - 1);

    string death_message = getSpeakString(key);

    // Check if they can speak or not: they may have been polymorphed.
    if (mons_near(mons) && !death_message.empty() && mons->can_speak())
        mons_speaks_msg(mons, death_message, MSGCH_TALK, silenced(you.pos()));
    else if (mons->can_speak())
        mprf("%s", death_message.c_str());

    // Upgrade the spellbook here, as elven_twin_energize
    // may not be called due to lack of visibility.
    if (mons_is_dowan(mons))
    {
        mons->spells[0].spell = SPELL_THROW_ICICLE;
        mons->spells[1].spell = SPELL_BLINK;
        mons->spells[3].spell = SPELL_STONE_ARROW;
        mons->spells[4].spell = SPELL_HASTE;
        // Nothing with 6.

        // Indicate that he has an updated spellbook.
        mons->props["custom_spells"] = true;
    }

    // Finally give them new energy
    if (mons_near(mons))
        elven_twin_energize(mons);
    else
        mons->props[ELVEN_ENERGIZE_KEY] = true;
}

void elven_twin_energize(monster* mons)
{
    if (mons_is_duvessa(mons))
        mons->go_berserk(true);
    else
    {
        ASSERT(mons_is_dowan(mons));
        if (mons->observable())
            simple_monster_message(mons, " seems to find hidden reserves of power!");

        mons->add_ench(ENCH_HASTE);
    }

    mons->props[ELVEN_IS_ENERGIZED_KEY] = true;
}

/**
 * Pacification effects for Dowan and Duvessa.
 *
 * As twins, pacifying one pacifies the other.
 *
 * @param twin    The original monster pacified.
**/
void elven_twins_pacify(monster* twin)
{
    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if ((*mi)->neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    ASSERT(!mons->neutral());

    if (you_worship(GOD_ELYVILON))
        gain_piety(random2(mons->max_hit_points / (2 + you.piety / 20)), 2);

    if (mons_near(mons))
        simple_monster_message(mons, " likewise turns neutral.");

    record_monster_defeat(mons, KILL_PACIFIED);
    mons_pacify(mons, ATT_NEUTRAL);
}

/**
 * Unpacification effects for Dowan and Duvessa.
 *
 * If they are both pacified and you attack one, the other will not remain
 * neutral. This is both for flavour (they do things together), and
 * functionality (so Dowan does not begin beating on Duvessa, etc).
 *
 * @param twin    The monster attacked.
**/
void elven_twins_unpacify(monster* twin)
{
    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if (!(*mi)->neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    behaviour_event(mons, ME_WHACK, &you, you.pos(), false);
}

bool mons_is_natasha(const monster* mons)
{
    return mons->type == MONS_NATASHA
           || (mons->props.exists("original_name")
               && mons->props["original_name"].get_string() == "Natasha");
}

bool mons_felid_can_revive(const monster* mons)
{
    return !mons->props.exists("felid_revives")
           || mons->props["felid_revives"].get_byte() < 2;
}

void mons_felid_revive(monster* mons)
{
    // Mostly adapted from bring_to_safety()
    coord_def revive_place;
    int tries = 10000;
    while (tries > 0) // Don't try too hard.
    {
        revive_place.x = random2(GXM);
        revive_place.y = random2(GYM);
        if (!in_bounds(revive_place)
            || grd(revive_place) != DNGN_FLOOR
            || env.cgrid(revive_place) != EMPTY_CLOUD
            || monster_at(revive_place)
            || env.pgrid(revive_place) & FPROP_NO_TELE_INTO
            || distance2(revive_place, mons->pos()) < dist_range(10))
        {
            tries--;
            continue;
        }
        else
            break;
    }
    if (tries == 0)
        return;

    // XXX: this will need to be extended if we get more types of enemy
    // felids
    monster_type type = (mons_is_natasha(mons)) ? MONS_NATASHA
                                                : MONS_FELID;
    monsterentry* me = get_monster_data(type);
    ASSERT(me);

    const int revives = (mons->props.exists("felid_revives"))
                        ? mons->props["felid_revives"].get_byte() + 1
                        : 1;

    const int hd = me->hpdice[0] - revives;

    monster *newmons =
        create_monster(
            mgen_data(type, (mons->has_ench(ENCH_CHARM) ? BEH_HOSTILE
                             : SAME_ATTITUDE(mons)),
                      0, 0, 0, revive_place, mons->foe, 0, GOD_NO_GOD,
                      MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE,
                      level_id::current(), hd));

    if (newmons)
    {
        for (int i = NUM_MONSTER_SLOTS - 1; i >= 0; --i)
            if (mons->inv[i] != NON_ITEM)
            {
                int item = mons->inv[i];
                give_specific_item(newmons, mitm[item]);
                destroy_item(item);
                mons->inv[i] = NON_ITEM;
            }

        newmons->props["felid_revives"].get_byte() = revives;
    }
}

bool mons_bennu_can_revive(const monster* mons)
{
    return !mons->props.exists("bennu_revives")
           || mons->props["bennu_revives"].get_byte() < 1;
}

void mons_bennu_revive(monster* mons)
{
    // Bennu only resurrect once and immediately in the same spot,
    // so this is rather abbreviated compared to felids.
    // XXX: Maybe generalize felid_revives and merge the two anyway?
    monster_type type = MONS_BENNU;
    monsterentry* me = get_monster_data(type);
    ASSERT(me);

    const int revives = (mons->props.exists("bennu_revives"))
                        ? mons->props["bennu_revives"].get_byte() + 1
                        : 1;
    bool res_visible = you.see_cell(mons->pos());

    mgen_data mg(type, (mons->has_ench(ENCH_CHARM) ? BEH_HOSTILE
                        : SAME_ATTITUDE(mons)),
                        0, 0, 0, (mons->pos()), mons->foe,
                        (res_visible ? MG_DONT_COME : 0), GOD_NO_GOD,
                        MONS_NO_MONSTER, 0, BLACK, PROX_ANYWHERE,
                        level_id::current());

    mons->set_position(coord_def(0,0));

    monster *newmons = create_monster(mg);
    if (newmons)
    {
        newmons->props["bennu_revives"].get_byte() = revives;

    }
}

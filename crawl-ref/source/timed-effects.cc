/**
 * @file
 * @brief Gametime related functions.
**/

#include "AppHdr.h"

#include "timed-effects.h"

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "dgn-event.h"
#include "env.h"
#include "exercise.h"
#include "externs.h"
#include "fprop.h"
#include "god-passive.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "mgen-data.h"
#include "monster.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mutation.h"
#include "notes.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "shout.h"
#include "state.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "stringutil.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#include "tileview.h"
#include "throw.h"
#include "travel.h"
#include "viewchar.h"
#include "unwind.h"

static void _apply_contam_over_time()
{
    int added_contamination = 0;

    //If not invisible, normal dissipation
    if (!you.duration[DUR_INVIS])
        added_contamination -= 75;

    // Scaling to turn length
    added_contamination = div_rand_round(added_contamination * you.time_taken,
                                         BASELINE_DELAY);

    contaminate_player(added_contamination, false);
}

// Bad effects from magic contamination.
static void _magic_contamination_effects()
{
    mprf(MSGCH_WARN, "Your body shudders with the violent release "
                     "of wild energies!");

    const int contam = you.magic_contamination;

    // For particularly violent releases, make a little boom.
    if (contam > 10000 && coinflip())
    {
        bolt beam;

        beam.flavour      = BEAM_RANDOM;
        beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage       = dice_def(3, div_rand_round(contam, 2000));
        beam.target       = you.pos();
        beam.name         = "magical storm";
        //XXX: Should this be MID_PLAYER?
        beam.source_id    = MID_NOBODY;
        beam.aux_source   = "a magical explosion";
        beam.ex_size      = max(1, min(LOS_RADIUS,
                                       div_rand_round(contam, 15000)));
        beam.ench_power   = div_rand_round(contam, 200);
        beam.is_explosion = true;

        beam.explode();
    }

    const mutation_permanence_class mutclass = MUTCLASS_NORMAL;

    // We want to warp the player, not do good stuff!
    mutate(one_chance_in(5) ? RANDOM_MUTATION : RANDOM_BAD_MUTATION,
           "mutagenic glow", true, coinflip(), false, false, mutclass);

    // we're meaner now, what with explosions and whatnot, but
    // we dial down the contamination a little faster if its actually
    // mutating you.  -- GDL
    contaminate_player(-(random2(contam / 4) + 1000));
}
// Checks if the player should be hit with magic contaimination effects,
// then actually does it if they should be.
static void _check_contamination_effects(int /*time_delta*/)
{
    const bool glow_effect = player_severe_contamination()
                             && x_chance_in_y(you.magic_contamination, 12000);

    if (glow_effect)
    {
        if (is_sanctuary(you.pos()))
        {
            mprf(MSGCH_GOD, "Your body momentarily shudders from a surge of wild "
                            "energies until Zin's power calms it.");
        }
        else
            _magic_contamination_effects();
    }
}

// Exercise armour *xor* stealth skill: {dlb}
static void _wait_practice(int /*time_delta*/)
{
    practise_waiting();
}

// Update the abyss speed. This place is unstable and the speed can
// fluctuate. It's not a constant increase.
static void _abyss_speed(int /*time_delta*/)
{
    if (!player_in_branch(BRANCH_ABYSS))
        return;

    if (have_passive(passive_t::slow_abyss) && coinflip())
        ; // Speed change less often for Chei.
    else if (coinflip() && you.abyss_speed < 100)
        ++you.abyss_speed;
    else if (one_chance_in(5) && you.abyss_speed > 0)
        --you.abyss_speed;
}

static void _jiyva_effects(int /*time_delta*/)
{
    if (have_passive(passive_t::jellies_army) && one_chance_in(10))
    {
        int total_jellies = 1 + random2(5);
        bool success = false;
        for (int num_jellies = total_jellies; num_jellies > 0; num_jellies--)
        {
            // Spread jellies around the level.
            coord_def newpos;
            do
            {
                newpos = random_in_bounds();
            }
            while (env.grid(newpos) != DNGN_FLOOR
                       && env.grid(newpos) != DNGN_SHALLOW_WATER
                   || monster_at(newpos)
                   || cloud_at(newpos)
                   || testbits(env.pgrid(newpos), FPROP_NO_JIYVA));

            mgen_data mg(MONS_JELLY, BEH_GOOD_NEUTRAL, newpos);
            mg.god = GOD_JIYVA;
            mg.non_actor_summoner = "Jiyva";

            if (create_monster(mg))
                success = true;
        }

        if (success && !silenced(you.pos()))
        {
            switch (random2(3))
            {
                case 0:
                    simple_god_message(" gurgles merrily.");
                    break;
                case 1:
                    mprf(MSGCH_SOUND, "You hear %s splatter%s.",
                         total_jellies > 1 ? "a series of" : "a",
                         total_jellies > 1 ? "s" : "");
                    break;
                case 2:
                    simple_god_message(" says: Divide and consume!");
                    break;
            }
        }
    }

    if (have_passive(passive_t::jelly_eating) && one_chance_in(25))
        jiyva_eat_offlevel_items();
}

static void _evolve(int /*time_delta*/)
{
    if (!you.can_safely_mutate())
        return;

    const bool malignant = you.has_mutation(MUT_DEVOLUTION);
    if (!malignant && !you.has_mutation(MUT_EVOLUTION))
        return;

    if (you.attribute[ATTR_EVOL_XP] > 0)
        return;
    set_evolution_mut_xp(malignant);

    mpr("You feel a genetic drift.");
    const mutation_type typ = malignant ? RANDOM_BAD_MUTATION : RANDOM_GOOD_MUTATION;
    const char* const reason = malignant ? "hidden defects" : "hidden potential";
    if (!mutate(typ, reason, false, false, false, false, MUTCLASS_NORMAL))
        return;

    int &muts = you.props[EVOLUTION_MUTS_KEY].get_int();
    ++muts;
    if (muts >= 2)
    {
        muts -= 2;
        if (malignant)
            delete_mutation(MUT_DEVOLUTION, "hidden defects expressed", false);
        else
            delete_mutation(MUT_EVOLUTION, "hidden potential expressed", false);
    }
    more();
}

// Get around C++ dividing integers towards 0.
static int _div(int num, int denom)
{
    div_t res = div(num, denom);
    return res.rem >= 0 ? res.quot : res.quot - 1;
}

struct timed_effect
{
    void              (*trigger)(int);
    int               min_time;
    int               max_time;
    bool              arena;
};

// If you add an entry to this list, remember to add a matching entry
// to timed_effect_type in timed-effect-type.h!
static struct timed_effect timed_effects[] =
{
    { rot_corpses,               200,   200, true  },
#if TAG_MAJOR_VERSION == 34
    { nullptr,                         0,     0, false },
    { nullptr,                         0,     0, false },
#endif
    { _check_contamination_effects,   70,   200, false },
#if TAG_MAJOR_VERSION == 34
    { nullptr,                         0,     0, false },
#endif
    { handle_god_time,               100,   300, false },
#if TAG_MAJOR_VERSION == 34
    { nullptr,                                0,     0, false },
    { nullptr,            0,   0, false },
#endif
    { _wait_practice,                100,   300, false },
#if TAG_MAJOR_VERSION == 34
    { nullptr,                         0,     0, false },
#endif
    { _abyss_speed,                  100,   300, false },
    { _jiyva_effects,                100,   300, false },
    { _evolve,                       100,   300, false },
#if TAG_MAJOR_VERSION == 34
    { nullptr,                         0,     0, false },
#endif
};

// Do various time related actions...
void handle_time()
{
    int base_time = you.elapsed_time % 200;
    int old_time = base_time - you.time_taken;

    // The checks below assume the function is called at least
    // once every 50 elapsed time units.

    // Every 5 turns, spawn random monsters
    if (_div(base_time, 50) > _div(old_time, 50))
    {
        spawn_random_monsters();
        if (player_in_branch(BRANCH_ABYSS))
        {
            // Ramp up spawn rate dramatically after Abyss:5.
            const int chances = you.depth < 5 ? you.depth
                                              : 5 + (you.depth - 5) * 5;
            for (int i = 1; i < chances; ++i)
                if (x_chance_in_y(i, 5))
                    spawn_random_monsters();
        }
    }

    // Abyss maprot.
    if (player_in_branch(BRANCH_ABYSS))
        forget_map(true);

    // Magic contamination from spells and Orb.
    if (!crawl_state.game_is_arena())
        _apply_contam_over_time();

    for (unsigned int i = 0; i < ARRAYSZ(timed_effects); i++)
    {
        if (crawl_state.game_is_arena() && !timed_effects[i].arena)
            continue;

        if (!timed_effects[i].trigger)
        {
            if (you.next_timer_effect[i] < INT_MAX)
                you.next_timer_effect[i] = INT_MAX;
            continue;
        }

        if (you.elapsed_time >= you.next_timer_effect[i])
        {
            int time_delta = you.elapsed_time - you.last_timer_effect[i];
            (timed_effects[i].trigger)(time_delta);
            you.last_timer_effect[i] = you.next_timer_effect[i];
            you.next_timer_effect[i] =
                you.last_timer_effect[i]
                + random_range(timed_effects[i].min_time,
                               timed_effects[i].max_time);
        }
    }
}

/**
 * Make ranged monsters flee from the player during their time offlevel.
 *
 * @param mon           The monster in question.
 */
static void _monster_flee(monster *mon)
{
    mon->behaviour = BEH_FLEE;
    dprf("backing off...");

    if (mon->pos() != mon->target)
        return;
    // If the monster is on the target square, fleeing won't work.

    if (in_bounds(env.old_player_pos) && env.old_player_pos != mon->pos())
    {
        // Flee from player's old position if different.
        mon->target = env.old_player_pos;
        return;
    }

    // Randomise the target so we have a direction to flee.
    coord_def mshift;
    mshift.x = random2(3) - 1;
    mshift.y = random2(3) - 1;

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = mon->target + mshift;
    if (!in_bounds_x(s.x))
        mshift.x = 0;
    if (!in_bounds_y(s.y))
        mshift.y = 0;

    mon->target.x += mshift.x;
    mon->target.y += mshift.y;

    return;
}

/**
 * Make a monster take a number of moves toward (or away from, if fleeing)
 * their current target, very crudely.
 *
 * @param mon       The mon in question.
 * @param moves     The number of moves to take.
 */
static void _catchup_monster_move(monster* mon, int moves)
{
    coord_def pos(mon->pos());

    // Dirt simple movement.
    for (int i = 0; i < moves; ++i)
    {
        coord_def inc(mon->target - pos);
        inc = coord_def(sgn(inc.x), sgn(inc.y));

        if (mons_is_retreating(*mon))
            inc *= -1;

        // Bounds check: don't let shifting monsters try to run off the
        // grid.
        const coord_def s = pos + inc;
        if (!in_bounds_x(s.x))
            inc.x = 0;
        if (!in_bounds_y(s.y))
            inc.y = 0;

        if (inc.origin())
            break;

        const coord_def next(pos + inc);
        const dungeon_feature_type feat = env.grid(next);
        if (feat_is_solid(feat)
            || monster_at(next)
            || !monster_habitable_grid(mon, feat))
        {
            break;
        }

        pos = next;
    }

    if (!mon->shift(pos))
        mon->shift(mon->pos());
}

/**
 * Move monsters around to fake them walking around while player was
 * off-level.
 *
 * Does not account for monster move speeds.
 *
 * Also make them forget about the player over time.
 *
 * @param mon       The monster under consideration
 * @param turns     The number of offlevel player turns to simulate.
 */
static void _catchup_monster_moves(monster* mon, int turns)
{
    // Summoned monsters might have disappeared.
    if (!mon->alive())
        return;

    // Ball lightning dissipates harmlessly out of LOS
    if (mon->type == MONS_BALL_LIGHTNING && mon->summoner == MID_PLAYER)
    {
        monster_die(*mon, KILL_RESET, NON_MONSTER);
        return;
    }

    // Expire friendly summons and temporary allies
    if (mon->friendly()
        && (mon->is_summoned() || mon->has_ench(ENCH_FAKE_ABJURATION))
        && !mon->is_perm_summoned())
    {
        // You might still see them disappear if you were quick
        if (turns > 2)
            monster_die(*mon, KILL_DISMISSED, NON_MONSTER);
        else
        {
            enchant_type abj_type = mon->has_ench(ENCH_ABJ) ? ENCH_ABJ
                                    : ENCH_FAKE_ABJURATION;
            mon_enchant abj  = mon->get_ench(abj_type);
            abj.duration = 0;
            mon->update_ench(abj);
        }
        return;
    }

    // Yred & animate dead zombies crumble on floor change
    if (mon->friendly()
        && ((is_yred_undead_follower(*mon) && mon->type != MONS_BOUND_SOUL)
            || (mon->has_ench(ENCH_SUMMON)
                && mon->get_ench(ENCH_SUMMON).degree == SPELL_ANIMATE_DEAD)))
    {
        if (turns > 2)
            monster_die(*mon, KILL_DISMISSED, NON_MONSTER);
        else
        {
            // handle expiration messages if the player was quick
            // doing it this way so the messages are kept consistent with
            // corresponding non-yred derived undead
            mon_enchant abj(ENCH_FAKE_ABJURATION, 0, 0, 1);
            mon->add_ench(abj);
            abj.duration = 0;
            mon->update_ench(abj);
        }
        return;
    }

    // Don't move non-land or stationary monsters around.
    if (mons_primary_habitat(*mon) != HT_LAND
        || mons_is_zombified(*mon)
           && mons_class_primary_habitat(mon->base_monster) != HT_LAND
        || mon->is_stationary())
    {
        return;
    }

    // special movement code for ioods
    if (mons_is_projectile(*mon))
    {
        iood_catchup(mon, turns);
        return;
    }

    // Let sleeping monsters lie.
    if (mon->asleep() || mon->paralysed())
        return;

    // Don't shift towards timestepped players.
    if (mon->target.origin())
        return;

    const int mon_turns = (turns * mon->speed) / 10;
    const int moves = min(mon_turns, 50);

    // probably too annoying even for DEBUG_DIAGNOSTICS
    dprf("mon #%d: range %d; "
         "pos (%d,%d); targ %d(%d,%d); flags %" PRIx64,
         mon->mindex(), mon_turns, mon->pos().x, mon->pos().y,
         mon->foe, mon->target.x, mon->target.y, mon->flags.flags);

    if (mon_turns <= 0)
        return;

    // restore behaviour later if we start fleeing
    unwind_var<beh_type> saved_beh(mon->behaviour);

    if (mons_has_ranged_attack(*mon))
    {
        // If we're doing short time movement and the monster has a
        // ranged attack (missile or spell), then the monster will
        // flee to gain distance if it's "too close", else it will
        // just shift its position rather than charge the player. -- bwr
        if (grid_distance(mon->pos(), mon->target) >= 3)
        {
            mon->shift(mon->pos());
            dprf("shifted to (%d, %d)", mon->pos().x, mon->pos().y);
            return;
        }

        _monster_flee(mon);
    }

    _catchup_monster_move(mon, moves);

    dprf("moved to (%d, %d)", mon->pos().x, mon->pos().y);
}

/**
 * Update a monster's enchantments when the player returns
 * to the level.
 *
 * Management for enchantments... problems with this are the oddities
 * (monster dying from poison several thousands of turns later), and
 * game balance.
 *
 * Consider: Poison/Sticky Flame a monster at range and leave, monster
 * dies but can't leave level to get to player (implied game balance of
 * the delayed damage is that the monster could be a danger before
 * it dies). This could be fixed by keeping some monsters active
 * off level and allowing them to take stairs (a very serious change).
 *
 * Compare this to the current abuse where the player gets
 * effectively extended duration of these effects (although only
 * the actual effects only occur on level, the player can leave
 * and heal up without having the effect disappear).
 *
 * This is a simple compromise between the two... the enchantments
 * go away, but the effects don't happen off level.  -- bwr
 *
 * @param levels XXX: sometimes the missing aut/10, sometimes aut/100
 */
void monster::timeout_enchantments(int levels)
{
    if (enchantments.empty())
        return;

    const mon_enchant_list ec = enchantments;
    for (auto &entry : ec)
    {
        if (entry.second.duration >= INFINITE_DURATION)
            continue;

        switch (entry.first)
        {
        case ENCH_POISON: case ENCH_CORONA: case ENCH_CONTAM:
        case ENCH_STICKY_FLAME: case ENCH_ABJ: case ENCH_SHORT_LIVED:
        case ENCH_HASTE: case ENCH_MIGHT: case ENCH_FEAR:
        case ENCH_CHARM: case ENCH_SLEEP_WARY: case ENCH_SICK:
        case ENCH_PARALYSIS: case ENCH_PETRIFYING:
        case ENCH_PETRIFIED: case ENCH_SWIFT: case ENCH_SILENCE:
        case ENCH_LOWERED_WL: case ENCH_SOUL_RIPE: case ENCH_ANTIMAGIC:
        case ENCH_REGENERATION: case ENCH_STRONG_WILLED:
        case ENCH_MIRROR_DAMAGE: case ENCH_LIQUEFYING:
        case ENCH_SILVER_CORONA: case ENCH_DAZED: case ENCH_FAKE_ABJURATION:
        case ENCH_BREATH_WEAPON: case ENCH_WRETCHED:
        case ENCH_SCREAMED: case ENCH_BLIND: case ENCH_WORD_OF_RECALL:
        case ENCH_INJURY_BOND: case ENCH_FLAYED: case ENCH_BARBS:
        case ENCH_AGILE: case ENCH_FROZEN: case ENCH_VITRIFIED:
        case ENCH_SIGN_OF_RUIN: case ENCH_SAP_MAGIC: case ENCH_NEUTRAL_BRIBED:
        case ENCH_FRIENDLY_BRIBED: case ENCH_CORROSION: case ENCH_GOLD_LUST:
        case ENCH_RESISTANCE: case ENCH_HEXED: case ENCH_IDEALISED:
        case ENCH_BOUND_SOUL: case ENCH_STILL_WINDS: case ENCH_DRAINED:
        case ENCH_ANGUISH: case ENCH_FIRE_VULN: case ENCH_SPELL_CHARGED:
        case ENCH_SLOW:
            lose_ench_levels(entry.second, levels);
            break;

        case ENCH_INVIS:
            if (!mons_class_flag(type, M_INVIS))
                lose_ench_levels(entry.second, levels);
            break;

        case ENCH_FRENZIED:
        case ENCH_BERSERK:
        case ENCH_INNER_FLAME:
        case ENCH_ROLLING:
        case ENCH_MERFOLK_AVATAR_SONG:
        case ENCH_INFESTATION:
        case ENCH_HELD:
        case ENCH_BULLSEYE_TARGET:
            del_ench(entry.first);
            break;

        case ENCH_FATIGUE:
            del_ench(entry.first);
            del_ench(ENCH_SLOW);
            break;

        case ENCH_TP:
            teleport(true);
            del_ench(entry.first);
            break;

        case ENCH_CONFUSION:
            if (!mons_class_flag(type, M_CONFUSED))
                del_ench(entry.first);
            // That triggered a behaviour_event, which could have made a
            // pacified monster leave the level.
            if (alive() && !is_stationary())
                monster_blink(this, true);
            break;

        case ENCH_TIDE:
        {
            const int actdur = speed_to_duration(speed) * levels;
            lose_ench_duration(entry.first, actdur);
            break;
        }

        case ENCH_SLOWLY_DYING:
        {
            const int actdur = speed_to_duration(speed) * levels;
            if (lose_ench_duration(entry.first, actdur))
                monster_die(*this, KILL_MISC, NON_MONSTER, true);
            break;
        }

        default:
            break;
        }

        if (!alive())
            break;
    }
}

/**
 * Update the level upon the player's return.
 *
 * @param elapsedTime how long the player was away.
 */
void update_level(int elapsedTime)
{
    ASSERT(!crawl_state.game_is_arena());

    const int turns = elapsedTime / 10;

#ifdef DEBUG_DIAGNOSTICS
    int mons_total = 0;

    dprf("turns: %d", turns);
#endif

    rot_corpses(elapsedTime);
    shoals_apply_tides(turns, true);
    timeout_tombs(turns);
    timeout_terrain_changes(elapsedTime);

    if (env.sanctuary_time)
    {
        // XX this doesn't guarantee that the final FPROP will be removed?
        if (turns >= env.sanctuary_time)
            remove_sanctuary();
        else
            env.sanctuary_time -= turns;
    }

    dungeon_events.fire_event(
        dgn_event(DET_TURN_ELAPSED, coord_def(0, 0), turns * 10));

    for (monster_iterator mi; mi; ++mi)
    {
#ifdef DEBUG_DIAGNOSTICS
        mons_total++;
#endif

        if (!update_monster(**mi, turns))
            continue;
    }

#ifdef DEBUG_DIAGNOSTICS
    dprf("total monsters on level = %d", mons_total);
#endif

    delete_all_clouds();
}

/**
 * Update the monster upon the player's return
 *
 * @param mon   The monster to update.
 * @param turns How many turns (not auts) since the monster left the player
 * @returns     Returns nullptr if monster was destroyed by the update;
 *              Returns the updated monster if it still exists.
 */
monster* update_monster(monster& mon, int turns)
{
    // Pacified monsters often leave the level now.
    if (mon.pacified() && turns > random2(40) + 21)
    {
        make_mons_leave_level(&mon);
        return nullptr;
    }

    // Ignore monsters flagged to skip their next action
    if (mon.flags & MF_JUST_SUMMONED)
        return &mon;

    // XXX: Allow some spellcasting (like Healing and Teleport)? - bwr
    // const bool healthy = (mon->hit_points * 2 > mon->max_hit_points);

    mon.heal(div_rand_round(turns * mon.off_level_regen_rate(), 100));

    // Handle nets specially to remove the trapping property of the net.
    if (mon.caught())
        mon.del_ench(ENCH_HELD, true);

    _catchup_monster_moves(&mon, turns);

    mon.foe_memory = max(mon.foe_memory - turns, 0);

    // FIXME:  Convert literal string 10 to constant to convert to auts
    if (turns >= 10 && mon.alive())
        mon.timeout_enchantments(turns / 10);

    return &mon;
}

static void _drop_tomb(const coord_def& pos, bool premature, bool zin)
{
    int count = 0;
    monster* mon = monster_at(pos);

    // Don't wander on duty!
    if (mon)
        mon->behaviour = BEH_SEEK;

    bool seen_change = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        // "Normal" tomb (card or monster spell)
        if (!zin && revert_terrain_change(*ai, TERRAIN_CHANGE_TOMB))
        {
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
        // Zin's Imprison.
        else if (zin && revert_terrain_change(*ai, TERRAIN_CHANGE_IMPRISON))
        {
            for (map_marker *mark : env.markers.get_markers_at(*ai))
            {
                if (mark->property("feature_description")
                    == "a gleaming silver wall")
                {
                    env.markers.remove(mark);
                }
            }

            env.grid_colours(*ai) = 0;
            tile_clear_flavour(*ai);
            tile_init_flavour(*ai);
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
    }

    if (count)
    {
        if (seen_change && !zin)
            mprf("The walls disappear%s!", premature ? " prematurely" : "");
        else if (seen_change && zin)
        {
            mprf("Zin %s %s %s.",
                 (mon) ? "releases"
                       : "dismisses",
                 (mon) ? mon->name(DESC_THE).c_str()
                       : "the silver walls,",
                 (mon) ? make_stringf("from %s prison",
                             mon->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str()
                       : "but there is nothing inside them");
        }
        else
        {
            if (!silenced(you.pos()))
                mprf(MSGCH_SOUND, "You hear a deep rumble.");
            else
                mpr("You feel the ground shudder.");
        }
    }
}

static vector<map_malign_gateway_marker*> _get_malign_gateways()
{
    vector<map_malign_gateway_marker*> mm_markers;

    for (map_marker *mark : env.markers.get_all(MAT_MALIGN))
    {
        if (mark->get_type() != MAT_MALIGN)
            continue;

        map_malign_gateway_marker *mmark = dynamic_cast<map_malign_gateway_marker*>(mark);

        mm_markers.push_back(mmark);
    }

    return mm_markers;
}

int count_malign_gateways()
{
    return _get_malign_gateways().size();
}

void timeout_malign_gateways(int duration)
{
    // Passing 0 should allow us to just touch the gateway and see
    // if it should decay. This, in theory, should resolve the one
    // turn delay between it timing out and being recastable. -due
    for (map_malign_gateway_marker *mmark : _get_malign_gateways())
    {
        if (duration)
            mmark->duration -= duration;

        if (mmark->duration > 0)
        {
            const int pow = 3 + random2(10);
            const int size = 2 + random2(5);
            big_cloud(CLOUD_TLOC_ENERGY, 0, mmark->pos, pow, size);
        }
        else
        {
            monster* mons = monster_at(mmark->pos);
            if (mmark->monster_summoned && !mons)
            {
                // The marker hangs around until later.
                if (env.grid(mmark->pos) == DNGN_MALIGN_GATEWAY)
                    env.grid(mmark->pos) = DNGN_FLOOR;

                env.markers.remove(mmark);
            }
            else if (!mmark->monster_summoned && !mons)
            {
                bool is_player = mmark->is_player;
                actor* caster = 0;
                if (is_player)
                    caster = &you;

                mgen_data mg = mgen_data(MONS_ELDRITCH_TENTACLE,
                                         mmark->behaviour,
                                         mmark->pos,
                                         MHITNOT,
                                         MG_FORCE_PLACE);
                mg.set_summoned(caster, 0, 0, mmark->god);
                if (!is_player)
                    mg.non_actor_summoner = mmark->summoner_string;

                if (monster *tentacle = create_monster(mg))
                {
                    tentacle->flags |= MF_NO_REWARD;
                    tentacle->add_ench(ENCH_PORTAL_TIMER);
                    int dur = random2avg(mmark->power, 6);
                    dur -= random2(4); // sequence point between random calls
                    dur *= 10;
                    mon_enchant kduration = mon_enchant(ENCH_PORTAL_PACIFIED, 4,
                        caster, dur);
                    tentacle->props[BASE_POSITION_KEY].get_coord()
                                        = tentacle->pos();
                    tentacle->add_ench(kduration);

                    mmark->monster_summoned = true;
                }
            }
        }
    }
}

void timeout_tombs(int duration)
{
    if (!duration)
        return;

    for (map_marker *mark : env.markers.get_all(MAT_TOMB))
    {
        if (mark->get_type() != MAT_TOMB)
            continue;

        map_tomb_marker *cmark = dynamic_cast<map_tomb_marker*>(mark);
        cmark->duration -= duration;

        // Empty tombs disappear early.
        monster* mon_entombed = monster_at(cmark->pos);
        bool empty_tomb = !(mon_entombed || you.pos() == cmark->pos);
        bool zin = (cmark->source == -GOD_ZIN);

        if (cmark->duration <= 0 || empty_tomb)
        {
            _drop_tomb(cmark->pos, empty_tomb, zin);

            monster* mon_src =
                !invalid_monster_index(cmark->source) ? &env.mons[cmark->source]
                                                      : nullptr;
            // A monster's Tomb of Doroklohe spell.
            if (mon_src
                && mon_src == mon_entombed)
            {
                mon_src->lose_energy(EUT_SPELL);
            }

            env.markers.remove(cmark);
        }
    }
}

void timeout_binding_sigils()
{
    int num_seen = 0;
    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(mark);
        if (marker->change_type == TERRAIN_CHANGE_BINDING_SIGIL)
        {
            if (you.see_cell(marker->pos))
                num_seen++;
            revert_terrain_change(marker->pos, TERRAIN_CHANGE_BINDING_SIGIL);
        }
    }

    if (num_seen > 1)
        mprf(MSGCH_DURATION, "Your binding sigils disappear.");
    else if (num_seen > 0)
        mprf(MSGCH_DURATION, "Your binding sigil disappears.");
}

// Force-cancel the player's toxic bog (in cases of !cancellation or quicksilver)
void end_toxic_bog()
{
    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
            dynamic_cast<map_terrain_change_marker*>(mark);

        if (marker->change_type == TERRAIN_CHANGE_BOG)
            revert_terrain_change(marker->pos, TERRAIN_CHANGE_BOG);
    }
}

void timeout_terrain_changes(int duration, bool force)
{
    if (!duration && !force)
        return;

    int num_seen[NUM_TERRAIN_CHANGE_TYPES] = {0};
    // n.b. unordered_set doesn't work here because pair isn't hashable
    set<pair<coord_def, terrain_change_type>> revert;

    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(mark);

        if (marker->duration != INFINITE_DURATION)
            marker->duration -= duration;

        if (marker->change_type == TERRAIN_CHANGE_DOOR_SEAL
            && !feat_is_sealed(env.grid(marker->pos)))
        {
            // TODO: could this be done inside `revert_terrain_change`? The
            // two things to test are corrupting sealed doors, and destroying
            // sealed doors. See 7aedcd24e1be3ed58fef9542786c1a194e4c07d0 and
            // 6c286a4f22bcba4cfcb36053eb066367874be752.
            if (marker->duration <= 0)
                env.markers.remove(marker); // deletes `marker`
            continue;
        }

        if ((marker->change_type == TERRAIN_CHANGE_BOG
             || marker->change_type == TERRAIN_CHANGE_BINDING_SIGIL)
            && !you.see_cell(marker->pos))
        {
            marker->duration = 0;
        }

        actor* src = actor_by_mid(marker->mon_num);
        if (marker->duration <= 0
            || (marker->mon_num != 0
                && (!src || !src->alive()
                    || (src->is_monster() && src->as_monster()->pacified()))))
        {
            if (you.see_cell(marker->pos))
                num_seen[marker->change_type]++;
            revert.insert(pair<coord_def, terrain_change_type>(marker->pos,
                                                        marker->change_type));
        }
    }
    // finally, revert the changes and delete the markers
    for (const auto &m_pos : revert)
        revert_terrain_change(m_pos.first, m_pos.second);

    if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 1)
        mpr("The runic seals fade away.");
    else if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 0)
        mpr("The runic seal fades away.");

    if (num_seen[TERRAIN_CHANGE_BINDING_SIGIL] > 1)
        mprf(MSGCH_DURATION, "Your binding sigils disappear.");
    else if (num_seen[TERRAIN_CHANGE_BINDING_SIGIL] > 0)
        mprf(MSGCH_DURATION, "Your binding sigil disappears.");
}

////////////////////////////////////////////////////////////////////////////
// Living breathing dungeon stuff.
//

static vector<coord_def> sfx_seeds;

void setup_environment_effects()
{
    sfx_seeds.clear();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            if (!in_bounds(x, y))
                continue;

            const int grid = env.grid[x][y];
            if (grid == DNGN_LAVA
                    || (grid == DNGN_SHALLOW_WATER
                        && player_in_branch(BRANCH_SWAMP)))
            {
                const coord_def c(x, y);
                sfx_seeds.push_back(c);
            }
        }
    }
    dprf("%u environment effect seeds", (unsigned int)sfx_seeds.size());
}

static void apply_environment_effect(const coord_def &c)
{
    const dungeon_feature_type grid = env.grid(c);
    // Don't apply if if the feature doesn't want it.
    if (testbits(env.pgrid(c), FPROP_NO_CLOUD_GEN))
        return;
    if (grid == DNGN_LAVA)
        check_place_cloud(CLOUD_BLACK_SMOKE, c, random_range(4, 8), 0);
    else if (one_chance_in(3) && grid == DNGN_SHALLOW_WATER)
        check_place_cloud(CLOUD_MIST,        c, random_range(2, 5), 0);
}

static const int Base_Sfx_Chance = 5;
void run_environment_effects()
{
    if (!you.time_taken)
        return;

    dungeon_events.fire_event(DET_TURN_ELAPSED);

    // Each square in sfx_seeds has this chance of doing something special
    // per turn.
    const int sfx_chance = Base_Sfx_Chance * you.time_taken / 10;
    const int nseeds = sfx_seeds.size();

    // If there are a large number of seeds, speed things up by fudging the
    // numbers.
    if (nseeds > 50)
    {
        int nsels = div_rand_round(sfx_seeds.size() * sfx_chance, 100);
        if (one_chance_in(5))
            nsels += random2(nsels * 3);

        for (int i = 0; i < nsels; ++i)
            apply_environment_effect(sfx_seeds[ random2(nseeds) ]);
    }
    else
    {
        for (int i = 0; i < nseeds; ++i)
        {
            if (random2(100) >= sfx_chance)
                continue;

            apply_environment_effect(sfx_seeds[i]);
        }
    }

    run_corruption_effects(you.time_taken);
    shoals_apply_tides(div_rand_round(you.time_taken, BASELINE_DELAY),
                       false);
    timeout_tombs(you.time_taken);
    timeout_malign_gateways(you.time_taken);
    timeout_terrain_changes(you.time_taken);
    run_cloud_spreaders(you.time_taken);
}

// Converts a movement speed to a duration. i.e., answers the
// question: if the monster is so fast, how much time has it spent in
// its last movement?
//
// If speed is 10 (normal),    one movement is a duration of 10.
// If speed is 1  (very slow), each movement is a duration of 100.
// If speed is 15 (50% faster than normal), each movement is a duration of
// 6.6667.
int speed_to_duration(int speed)
{
    if (speed < 1)
        speed = 10;
    else if (speed > 100)
        speed = 100;

    return div_rand_round(100, speed);
}

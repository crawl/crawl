#include "AppHdr.h"

#include "stairs.h"

#include <sstream>

#include "ability.h"
#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "bloodspatter.h"
#include "branch.h"
#include "chardump.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h" // place_specific_trap
#include "env.h"
#include "files.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-passive.h" // passive_t::slow_abyss
#include "hints.h"
#include "hiscores.h"
#include "item-name.h"
#include "items.h"
#include "level-state-type.h"
#include "losglobal.h"
#include "mapmark.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-transit.h" // untag_followers
#include "movement.h"
#include "mutation.h"
#include "notes.h"
#include "orb-type.h"
#include "output.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-other.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#ifdef USE_TILE_LOCAL
 #include "tilepick.h"
#endif
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"
#include "zot.h" // bezotted

static string _annotation_exclusion_warning(level_id next_level_id)
{
    if (level_annotation_has("!", next_level_id)
        && next_level_id != level_id::current()
        && is_connected_branch(next_level_id))
    {
        crawl_state.level_annotation_shown = true;
        return make_stringf("Warning, next level annotated: <yellow>%s</yellow>",
                            get_level_annotation(next_level_id).c_str());
    }

    if (is_exclude_root(you.pos())
             && feat_is_travelable_stair(env.grid(you.pos()))
             && !strstr(get_exclusion_desc(you.pos()).c_str(), "cloud"))
    {
        return "This staircase is marked as excluded!";
    }

    return "";
}

static string _target_exclusion_warning()
{
    if (!feat_is_travelable_stair(env.grid(you.pos())))
        return "";

    LevelInfo *li = travel_cache.find_level_info(level_id::current());
    if (!li)
        return "";

    const stair_info *si = li->get_stair(you.pos());
    if (!si)
        return "";

    if (stairs_destination_is_excluded(*si))
        return "This staircase leads to a travel-excluded area!";

    return "";
}

static string _bezotting_warning(branch_type branch)
{
    if (branch == you.where_are_you || !bezotted_in(branch))
        return "";

    const int turns = turns_until_zot_in(branch);
    return make_stringf("You have just %d turns in %s to find a new floor before Zot consumes you.",
                        turns, branches[branch].longname);
}

bool check_next_floor_warning()
{
    level_id  next_level_id = level_id::get_next_level_id(you.pos());

    crawl_state.level_annotation_shown = false;
    const string annotation_warning = _annotation_exclusion_warning(next_level_id);

    const string target_warning = _target_exclusion_warning();
    const string bezotting_warning = _bezotting_warning(next_level_id.branch);

    if (annotation_warning != "")
        mprf(MSGCH_PROMPT, "%s", annotation_warning.c_str());
    if (target_warning != "")
        mprf(MSGCH_PROMPT, "%s", target_warning.c_str());
    if (bezotting_warning != "")
        mprf(MSGCH_PROMPT, "%s", bezotting_warning.c_str());

    const bool might_be_dangerous = annotation_warning != ""
                                 || target_warning != ""
                                 || bezotting_warning != "";

    if (might_be_dangerous
        && !yesno("Enter next level anyway?", true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        interrupt_activity(activity_interrupt::force);
        crawl_state.level_annotation_shown = false;
        return false;
    }
    return true;
}

static void _player_change_level_reset()
{
    you.prev_targ  = MID_NOBODY;
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

    you.prev_grd_targ.reset();
}

static void _player_change_level(level_id lev)
{
    you.depth         = lev.depth;
    you.where_are_you = lev.branch;
}

static void _maybe_destroy_shaft(const coord_def &p)
{
    trap_def* trap = trap_at(p);
    if (trap && trap->type == TRAP_SHAFT)
        trap->destroy(true);
}

static bool _stair_moves_pre(dungeon_feature_type stair)
{
    if (crawl_state.prev_cmd == CMD_WIZARD)
        return false;

    if (stair != env.grid(you.pos()))
        return false;

    if (feat_stair_direction(stair) == CMD_NO_CMD)
        return false;

    if (!you.duration[DUR_REPEL_STAIRS_CLIMB])
        return false;

    int pct;
    if (you.duration[DUR_REPEL_STAIRS_MOVE])
        pct = 29;
    else
        pct = 50;

    // When the effect is still strong, the chance to actually catch a stair
    // is smaller. (Assuming the duration starts out at 500.)
    const int dur = max(0, you.duration[DUR_REPEL_STAIRS_CLIMB] - 200);
    pct += dur/20;

    if (!x_chance_in_y(pct, 100))
        return false;

    // Get feature name before sliding stair over.
    string stair_str = feature_description_at(you.pos(), false, DESC_THE);

    if (!slide_feature_over(you.pos()))
        return false;

    string verb = stair_climb_verb(stair);

    mprf("%s moves away as you attempt to %s it!", stair_str.c_str(),
         verb.c_str());

    you.turn_is_over = true;

    return true;
}

static void _climb_message(dungeon_feature_type stair, bool going_up,
                           branch_type old_branch)
{
    if (!is_connected_branch(old_branch))
        return;

    if (feat_is_portal(stair))
    {
        if (stair != DNGN_ENTER_CRUCIBLE)
            mpr("The world spins around you as you enter the gateway.");
    }
    else if (feat_is_escape_hatch(stair))
    {
        if (going_up)
            mpr("A mysterious force pulls you upwards.");
        else
        {
            mprf("You %s downwards.",
                 you.airborne() ? "fly" : "slide");
        }
        mpr("The hatch slams shut behind you.");
    }
    else if (feat_is_gate(stair))
    {
        mprf("You %s %s through the gate.",
             you.airborne() ? "fly" : "go",
             going_up ? "up" : "down");
    }
    else if (stair != DNGN_ALTAR_IGNIS)
    {
        mprf("You %s %swards.",
             you.airborne() ? "fly" : "climb",
             going_up ? "up" : "down");
    }
}

static void _clear_golubria_traps()
{
    for (auto c : find_golubria_on_level())
    {
        trap_def *trap = trap_at(c);
        if (trap && trap->type == TRAP_GOLUBRIA)
            trap->destroy();
    }
}

static void _remove_unstable_monsters()
{
    for (auto &mons : menv_real)
    {
        if (mons_class_flag(mons.type, M_UNSTABLE) && mons.is_summoned())
            mons.reset();
    }
}

static void _complete_zig()
{
    if (!zot_immune())
        mpr("You have passed through the Ziggurat. Zot will hunt you nevermore.");
    you.zigs_completed++;
}

void leaving_level_now(dungeon_feature_type stair_used)
{
    if (stair_used == DNGN_EXIT_ZIGGURAT)
    {
        if (you.depth == 27)
            _complete_zig();
        mark_milestone("zig.exit", make_stringf("left a ziggurat at level %d.",
                       you.depth));
    }

    if (stair_used == DNGN_EXIT_ABYSS)
    {
#ifdef DEBUG
        auto &vault_list =  you.vault_list[level_id::current()];
        vault_list.push_back("[exit]");
#endif
        clear_abyssal_rune_knowledge();
    }

    dungeon_events.fire_position_event(DET_PLAYER_CLIMBS, you.pos());
    dungeon_events.fire_event(DET_LEAVING_LEVEL);

    _clear_golubria_traps();
    _remove_unstable_monsters();
}

static void _update_travel_cache(const level_id& old_level,
                                 const coord_def& stair_pos)
{
    // If the old level is gone, nothing to save.
    if (!you.save || !you.save->has_chunk(old_level.describe()))
        return;

    // Update stair information for the stairs we just ascended, and the
    // down stairs we're currently on.
    level_id  new_level_id    = level_id::current();

    if (can_travel_interlevel())
    {
        LevelInfo &old_level_info =
                    travel_cache.get_level_info(old_level);
        LevelInfo &new_level_info =
                    travel_cache.get_level_info(new_level_id);
        new_level_info.update();

        // First we update the old level's stair.
        level_pos lp;
        lp.id  = new_level_id;
        lp.pos = you.pos();

        bool guess = false;
        // Ugly hack warning:
        // The stairs in the Vestibule of Hell exhibit special behaviour:
        // they always lead back to the dungeon level that the player
        // entered the Vestibule from. This means that we need to pretend
        // we don't know where the upstairs from the Vestibule go each time
        // we take it. If we don't, interlevel travel may try to use portals
        // to Hell as shortcuts between dungeon levels, which won't work,
        // and will confuse the dickens out of the player (well, it confused
        // the dickens out of me when it happened).
        if ((new_level_id == BRANCH_DUNGEON || new_level_id == BRANCH_DEPTHS)
            && old_level == BRANCH_VESTIBULE)
        {
            old_level_info.clear_stairs(DNGN_EXIT_HELL);
        }
        else
            old_level_info.update_stair(stair_pos, lp, guess);

        // We *guess* that going up a staircase lands us on a downstair,
        // and that we can descend that downstair and get back to where we
        // came from. This assumption is guaranteed false when climbing out
        // of one of the branches of Hell.
        if (new_level_id != BRANCH_VESTIBULE
            || !is_hell_subbranch(old_level.branch))
        {
            // Set the new level's stair, assuming arbitrarily that going
            // downstairs will land you on the same upstairs you took to
            // begin with (not necessarily true).
            lp.id = old_level;
            lp.pos = stair_pos;
            new_level_info.update_stair(you.pos(), lp, true);
        }
    }
}

// These checks are probably unnecessary.
static bool _check_stairs(const dungeon_feature_type ftype, bool going_up)
{
    // If it's not bidirectional, check that the player is headed
    // in the right direction.
    if (!feat_is_bidirectional_portal(ftype))
    {
        if (feat_stair_direction(ftype) != (going_up ? CMD_GO_UPSTAIRS
                                                     : CMD_GO_DOWNSTAIRS))
        {
            if (ftype == DNGN_STONE_ARCH)
                mpr("There is nothing on the other side of the stone arch.");
            else if (ftype == DNGN_ABANDONED_SHOP)
                mpr("This shop has been abandoned, nothing of value remains.");
            else if (going_up)
                mpr("You can't go up here!");
            else
                mpr("You can't go down here!");
            return false;
        }
    }

    return true;
}

static bool _check_fall_down_stairs(const dungeon_feature_type ftype, bool going_up)
{
    if (!you.airborne()
        && you.confused()
        && !feat_is_escape_hatch(ftype)
        && !crawl_state.game_is_descent()
        && coinflip())
    {
        const char* fall_where = "down the stairs";
        if (!feat_is_staircase(ftype))
            fall_where = "through the gate";

        mprf("In your confused state, you trip and fall %s%s.",
             going_up ? "back " : "", fall_where);
        if (!feat_is_staircase(ftype))
            ouch(1, KILLED_BY_FALLING_THROUGH_GATE);
        else
            ouch(1, KILLED_BY_FALLING_DOWN_STAIRS);

        // Note that if going downstairs, this only does damage.
        // It doesn't cancel the level transition.
        if (going_up)
        {
            you.turn_is_over = true;
            return true;
        }
    }

    return false;
}

static void _rune_effect(dungeon_feature_type ftype)
{
    vector<int> runes;
    for (int i = 0; i < NUM_RUNE_TYPES; i++)
        if (you.runes[i])
            runes.push_back(i);

    ASSERT(runes.size() >= 1);
    shuffle_array(runes);

    // Zot is extra flashy.
    if (ftype == DNGN_ENTER_ZOT)
    {
        ASSERT(runes.size() >= 3);

        mprf("You insert the %s rune into the lock.", rune_type_name(runes[2]));
#ifdef USE_TILE_LOCAL
        view_add_tile_overlay(you.pos(), tileidx_zap(rune_colour(runes[2])));
        viewwindow(false);
        update_screen();
#else
        flash_view(UA_BRANCH_ENTRY, rune_colour(runes[2]));
#endif
        mpr("The lock glows eerily!");
        // included in default force_more_message

        mprf("You insert the %s rune into the lock.", rune_type_name(runes[1]));
        big_cloud(CLOUD_BLUE_SMOKE, &you, you.pos(), 20, 7 + random2(7));
        viewwindow();
        update_screen();
        mpr("Heavy smoke blows from the lock!");
        // included in default force_more_message
    }

    mprf("You insert the %s rune into the lock.", rune_type_name(runes[0]));

    if (silenced(you.pos()))
        mpr("The gate opens wide!");
    else
        mpr("With a soft hiss the gate opens wide!");
    // these are included in default force_more_message
}

static void _maybe_use_runes(dungeon_feature_type ftype)
{
    switch (ftype)
    {
    case DNGN_ENTER_ZOT:
        if (!you.level_visited(level_id(BRANCH_ZOT, 1)) && !crawl_state.game_is_descent())
            _rune_effect(ftype);
        break;
    case DNGN_EXIT_VAULTS:
        if (vaults_is_locked())
        {
            unlock_vaults();
            _rune_effect(ftype);
        }
        break;
    default:
        break;
    }
}

static void _gauntlet_effect()
{
    // already doomed
    if (you.stasis())
        return;

    mprf(MSGCH_WARN, "The nature of this place prevents you from teleporting.");

    if (you.get_base_mutation_level(MUT_TELEPORT))
        mpr("You feel stable on this floor.");
}

static void _hell_effects()
{

    // 50% chance at max piety
    if (have_passive(passive_t::resist_hell_effects)
        && x_chance_in_y(you.piety, MAX_PIETY * 2) || is_sanctuary(you.pos()))
    {
        simple_god_message(" power protects you from the chaos of Hell!", true);
        return;
    }

    const bool loud = one_chance_in(6) && !silenced(you.pos());
    string msg = getMiscString(loud ? "hell_effect_noisy"
                                    : "hell_effect_quiet");
    if (msg.empty())
        msg = "Something hellishly buggy happens.";

    mprf(MSGCH_HELL_EFFECT, "%s", msg.c_str());
    if (loud)
        noisy(15, you.pos());

    switch (random2(3))
    {
        case 0:
            temp_mutate(RANDOM_BAD_MUTATION, "hell effect");
            break;
        case 1:
            drain_player(85, true, true);
            break;
        default:
            break;
    }
}

static void _vainglory_arrival()
{
    vector<monster*> mons;
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (!mi->is_firewood() && !mi->wont_attack())
            mons.push_back(*mi);
    }

    if (!mons.empty())
    {
        mprf(MSGCH_WARN, "You announce your regal presence to all who would look upon you.");
        for (monster* mon : mons)
            behaviour_event(mon, ME_ANNOY, &you);

        you.duration[DUR_VAINGLORY] = random_range(120, 220);
    }
}

static void _new_level_amuses_xom(dungeon_feature_type feat,
                                  dungeon_feature_type old_feat,
                                  bool shaft, int shaft_depth, bool voluntary)
{
    switch (you.where_are_you)
    {
    default:
        // Xom thinks it's funny if you enter a new level via shaft
        // or escape hatch, for shafts it's funnier the deeper you fell.
        if (shaft || feat_is_escape_hatch(feat))
            xom_is_stimulated(shaft_depth * 50);
        else if (!is_connected_branch(you.where_are_you))
            xom_is_stimulated(25);
        else
            xom_is_stimulated(10);
        break;

    case BRANCH_ZIGGURAT:
        // The best way to die currently.
        xom_is_stimulated(50);
        break;

    case BRANCH_PANDEMONIUM:
        xom_is_stimulated(100);
        break;

    case BRANCH_ABYSS:
        if (voluntary && old_feat == DNGN_ENTER_ABYSS)
            xom_is_stimulated(100, XM_INTRIGUED);
        else
            xom_is_stimulated(200);
        break;
    }
}

/**
 * Determine destination level.
 *
 * @param how         How the player is trying to travel.
 *                    (e.g. stairs, traps, portals, etc)
 * @param forced      True if the player is forcing the traveling attempt.
 *                    (e.g. forcibly exiting the abyss, etc)
 * @param going_up    True if the player is going upstairs.
 * @param known_shaft True if the player is intentionally shafting themself.
 * @return            The destination level, if valid. Note the default value
 *                    of dest is not valid (since depth = -1) and this is
 *                    generally what is returned for invalid destinations.
 *                    But note the special case when failing to climb stairs
 *                    when attempting to leave the dungeon, depth = 1.
 */
static level_id _travel_destination(const dungeon_feature_type how,
                                    bool forced, bool going_up,
                                    bool known_shaft)
{
    const bool shaft = known_shaft || how == DNGN_TRAP_SHAFT;
    level_id shaft_dest;
    level_id dest;
    if (shaft)
    {
        if (!is_valid_shaft_level(false))
        {
            if (known_shaft)
                mpr("The shaft disappears in a puff of logic!");
            _maybe_destroy_shaft(you.pos());
            return dest;
        }

        shaft_dest = you.shaft_dest();
    }
    // How far down you fall via a shaft or hatch.
    const int shaft_depth = (shaft ? shaft_dest.depth - you.depth : 1);

    // Only check the current position for a legal stair traverse.
    // Check that we're going the right way (if we're not falling through
    // a shaft or being forced).
    if (!shaft && !forced && !_check_stairs(how, going_up))
        return dest;

    // Up and down both work for some portals.
    // Canonicalize the direction: hell exits into the vestibule are considered
    // going up; everything else is going down. This mostly affects which way you
    // fall if confused.
    if (feat_is_bidirectional_portal(how))
        going_up = feat_is_hell_subbranch_exit(how);

    if (_stair_moves_pre(how))
        return dest;

    // Falling down is checked before the transition if going upstairs, since
    // it might prevent the transition itself.
    if (going_up && _check_fall_down_stairs(how, true))
    {
        // TODO: This probably causes an obscure bug where confused players
        // going 'down' into the vestibule are twice as likely to fall, because
        // they have to pass a check here, and later in floor_transition
        // Right solution is probably to use the canonicalized direction everywhere

        // If player falls down the stairs trying to leave the dungeon, we set
        // the destination depth to 1 (i.e. D:1)
        if (how == DNGN_EXIT_DUNGEON)
            dest.depth = 1;
        return dest;
    }

    if (shaft)
    {
        if (shaft_dest == level_id::current())
        {
            if (known_shaft)
            {
                mpr("Strange, the shaft seems to lead back to this level.");
                mpr("The strain on the space-time continuum destroys the "
                    "shaft!");
            }
            _maybe_destroy_shaft(you.pos());
            return dest;
        }

        if (!known_shaft)
        {
            mark_milestone("shaft", "fell down a shaft to "
                                    + shaft_dest.describe() + ".");
        }

        mprf("You %s into a shaft and drop %d floor%s!",
             you.airborne() ? "are sucked" : "fall",
             shaft_depth,
             shaft_depth > 1 ? "s" : "");

        // Shafts are one-time-use.
        mpr("The shaft crumbles and collapses.");
        _maybe_destroy_shaft(you.pos());
    }

    // Maybe perform the entry sequence (we check that they have enough runes
    // in main.cc: _can_take_stairs())
    _maybe_use_runes(how);

    // Markers might be deleted when removing portals.
    const string dst = env.markers.property_at(you.pos(), MAT_ANY, "dst");

    if (shaft)
        return shaft_dest;
    else
        return stair_destination(how, dst, true);
}

/**
 * Check to see if transition will actually move the player.
 *
 * @param dest      The destination level (branch and depth).
 * @param feat      The dungeon feature the player is standing on.
 * @param going_up  True if the player is trying to go up stairs.
 * @return          True if the level transition should happen.
 */
static bool _level_transition_moves_player(level_id dest,
                                           dungeon_feature_type feat,
                                           bool going_up)
{
    bool trying_to_exit = feat == DNGN_EXIT_DUNGEON && going_up;

    // When exiting the dungeon, dest is not valid (depth = -1)
    // So the player can transition with an invalid dest ONLY when exiting.
    // Otherwise (i.e. not exiting) dest must be valid.
    return dest.is_valid() != trying_to_exit;
}

level_id level_above()
{
    if (crawl_state.game_is_sprint())
        return level_id();
    // can't re-enter old zig floors; they get regenerated
    if (player_in_branch(BRANCH_ZIGGURAT))
        return level_id();
    if (you.depth > 1)
        return level_id(you.where_are_you, you.depth - 1);
    if (!is_connected_branch(you.where_are_you))
        return level_id(); // no rocketing out of the abyss, portals, pan...
    const level_id entry = brentry[you.where_are_you];
    if (entry.is_valid())
        return entry;
    return level_id();
}

void rise_through_ceiling()
{
    const level_id whither = level_above();
    if (you.where_are_you == BRANCH_DUNGEON
        && you.depth == 1
        && player_has_orb())
    {
        mpr("With a burst of heat and light, you rocket upward!");
        floor_transition(DNGN_EXIT_DUNGEON, DNGN_EXIT_DUNGEON,
                         level_id(BRANCH_DUNGEON, 0), true, true, false, false);
    }
    if (!whither.is_valid())
    {
        mpr("In a burst of heat and light, you rocket briefly upward... "
            "but you can't rise from here.");
        return;
    }

    mpr("With a burst of heat and light, you rocket upward!");
    untag_followers(); // XXX: is this needed?
    stop_delay(true);
    floor_transition(DNGN_ALTAR_IGNIS /*hack*/, DNGN_ALTAR_IGNIS,
                     whither, true, true, false, false);
    you.clear_far_engulf();

    // flavour! blow a hole through the floor
    if (env.grid(you.pos()) == DNGN_FLOOR
        && !trap_at(you.pos()) /*needed?*/
        && is_valid_shaft_level())
    {
        place_specific_trap(you.pos(), TRAP_SHAFT);
    }
}

/**
 * Transition to a different level.
 *
 * @param how The type of stair/portal tile the player is being conveyed through
 * @param whence The tile the player was on at the beginning of the transition
 *               (likely the same as how, unless forced is true)
 * @param whither The destination level
 * @param shaft Is the player going down a shaft?
 */
void floor_transition(dungeon_feature_type how,
                      const dungeon_feature_type whence, level_id whither,
                      bool forced, bool going_up, bool shaft,
                      bool update_travel_cache)
{
    const level_id old_level = level_id::current();

    // Clean up fake blood.
    heal_flayed_effect(&you, true, true);

    // We "stepped".
    if (!forced)
        player_did_deliberate_movement();

    // Magical level changes (which currently only exist "downwards") need this.
    clear_trapping_net();
    stop_channelling_spells();
    you.stop_constricting_all();
    you.stop_being_constricted();
    you.clear_beholders();
    you.clear_fearmongers();
    dec_frozen_ramparts(you.duration[DUR_FROZEN_RAMPARTS]);
    if (you.duration[DUR_OOZEMANCY])
        jiyva_end_oozemancy();
    if (you.duration[DUR_NOXIOUS_BOG])
        you.duration[DUR_NOXIOUS_BOG] = 0;
    if (you.duration[DUR_DIMENSIONAL_BULLSEYE])
    {
        monster* targ = monster_by_mid(you.props[BULLSEYE_TARGET_KEY].get_int());
        if (targ)
            targ->del_ench(ENCH_BULLSEYE_TARGET);
    }
    if (yred_torch_is_raised())
        yred_end_conquest();
    if (you.duration[DUR_FATHOMLESS_SHACKLES])
    {
        you.duration[DUR_FATHOMLESS_SHACKLES] = 0;
        yred_end_blasphemy();
    }

    if (you.duration[DUR_BEOGH_DIVINE_CHALLENGE])
        flee_apostle_challenge();

    if (you.duration[DUR_BLOOD_FOR_BLOOD])
        beogh_end_blood_for_blood();

    if (you.duration[DUR_BEOGH_CAN_RECRUIT])
    {
        you.duration[DUR_BEOGH_CAN_RECRUIT] = 0;
        end_beogh_recruit_window();
    }

    if (you.duration[DUR_CACOPHONY])
    {
        you.duration[DUR_CACOPHONY] = 0;
        mprf(MSGCH_DURATION, "Your cacophony subsides as you depart the area.");
        for (monster_iterator mi; mi; ++mi)
            if (mi->was_created_by(MON_SUMM_CACOPHONY))
                monster_die(**mi, KILL_RESET, NON_MONSTER, true);
    }

    // Fire level-leaving trigger.
    leaving_level_now(how);

    // Fix this up now so the milestones and notes report the correct
    // destination floor.
    if (whither.branch == BRANCH_ABYSS)
    {
        if (!you.props.exists(ABYSS_MIN_DEPTH_KEY))
            you.props[ABYSS_MIN_DEPTH_KEY] = 1;

        whither.depth = max(you.props[ABYSS_MIN_DEPTH_KEY].get_int(),
                            whither.depth);
        you.props[ABYSS_MIN_DEPTH_KEY] = whither.depth;
    }

    // Not entirely accurate - the player could die before
    // reaching the Abyss.
    if (!forced && whence == DNGN_ENTER_ABYSS)
    {
        mark_milestone("abyss.enter", "entered the Abyss!");
        take_note(Note(NOTE_MESSAGE, 0, 0, "Voluntarily entered the Abyss."), true);
    }
    else if (!forced && whence == DNGN_EXIT_THROUGH_ABYSS)
    {
        mark_milestone("abyss.enter", "escaped (hah) into the Abyss!");
        take_note(Note(NOTE_MESSAGE, 0, 0, "Took an exit into the Abyss."), true);
    }
    else if (how == DNGN_EXIT_ABYSS)
    {
        mark_milestone("abyss.exit", "escaped from the Abyss!");
        you.attribute[ATTR_BANISHMENT_IMMUNITY] = you.elapsed_time + 100
                                                  + random2(100);
        you.banished_by = "";
        you.banished_power = 0;
    }

    // Interlevel travel data.
    const bool collect_travel_data = can_travel_interlevel();
    if (collect_travel_data)
    {
        LevelInfo &old_level_info = travel_cache.get_level_info(old_level);
        old_level_info.update();
    }

    const coord_def stair_pos = you.pos();

    // Note down whether we knew where we were going for descent timing.
    const bool dest_known = !shaft && travel_cache.know_stair(stair_pos);

    if (how == DNGN_EXIT_DUNGEON)
    {
        you.depth = 0;
        mpr("You have escaped!");
        ouch(INSTANT_DEATH, player_has_orb() ? KILLED_BY_WINNING
                                             : KILLED_BY_LEAVING);
    }

    if (how == DNGN_ENTER_ZIGGURAT)
        dungeon_terrain_changed(you.pos(), DNGN_STONE_ARCH);

    if (how == DNGN_ENTER_PANDEMONIUM
        || how == DNGN_ENTER_ABYSS
        || feat_is_portal_entrance(how))
    {
        you.level_stack.push_back(level_pos::current());
    }

    // Actually change the player's branch and depth, along with some cleanup.
    _player_change_level_reset();
    _player_change_level(whither);

    // Some branch specific messages.
    if (old_level.branch == BRANCH_VESTIBULE
        && !is_hell_subbranch(you.where_are_you))
    {
        mpr("Thank you for visiting Hell. Please come again soon.");
    }

    if (how == DNGN_EXIT_ABYSS
        || how == DNGN_EXIT_PANDEMONIUM
        || how == DNGN_EXIT_THROUGH_ABYSS)
    {
        mpr("You pass through the gate.");
        take_note(Note(NOTE_MESSAGE, 0, 0,
            how == DNGN_EXIT_ABYSS ? "Escaped the Abyss" :
            how == DNGN_EXIT_PANDEMONIUM ? "Escaped Pandemonium" :
            how == DNGN_EXIT_THROUGH_ABYSS ? "Escaped into the Abyss" :
            "Buggered into bugdom"), true);

        if (!you.wizard || !crawl_state.is_replaying_keys())
            more();
    }

    // Fixup exits from the Hell branches.
    if (player_in_branch(BRANCH_VESTIBULE)
        && is_hell_subbranch(old_level.branch))
    {
        how = branches[old_level.branch].entry_stairs;
    }

    // Check for falling down the stairs or portal.
    if (!going_up && !shaft && !forced)
        _check_fall_down_stairs(how, false);

    if (shaft)
        how = DNGN_TRAP_SHAFT;

    switch (you.where_are_you)
    {
    case BRANCH_ABYSS:
        // There are no abyssal stairs that go up, so this whole case is only
        // when going down.
        // -- unless you're a rocketeer!
        you.props.erase(ABYSS_SPAWNED_XP_EXIT_KEY);
        if (old_level.depth > you.depth)
            break;
        if (old_level.branch == BRANCH_ABYSS)
        {
            mprf(MSGCH_BANISHMENT, "You plunge deeper into the Abyss.");
            if (!you.runes[RUNE_ABYSSAL] && you.depth >= ABYSSAL_RUNE_MIN_LEVEL)
                mpr("The abyssal rune of Zot can be found at this depth.");
            break;
        }
        if (!forced)
            mpr("You enter the Abyss!");

        mpr("To return, you must find a gate leading back.");
        mpr("Killing monsters will force the Abyss to allow you passage.");
        if (have_passive(passive_t::slow_abyss))
        {
            mprf(MSGCH_GOD, you.religion,
                 "You feel %s slowing down the madness of this place.",
                 god_name(you.religion).c_str());
        }

        you.props[ABYSS_STAIR_XP_KEY] = EXIT_XP_COST;

        // Re-entering the Abyss halves accumulated speed.
        you.abyss_speed /= 2;
        learned_something_new(HINT_ABYSS);
        break;

    case BRANCH_PANDEMONIUM:
        if (old_level.branch == BRANCH_PANDEMONIUM)
            mpr("You pass into a different region of Pandemonium.");
        break;

    default:
        // This hits both cases.
        if (!shaft)
            _climb_message(how, going_up, old_level.branch);
        break;
    }

    // Did we enter a different branch?
    if (!player_in_branch(old_level.branch))
    {
        const branch_type branch = you.where_are_you;
        if (branch_entered(branch))
            mprf("Welcome back to %s!", branches[branch].longname);
        else if (how == branches[branch].entry_stairs)
        {
            if (branches[branch].entry_message)
                mpr(branches[branch].entry_message);
            else if (branch != BRANCH_ABYSS) // too many messages...
                mprf("Welcome to %s!", branches[branch].longname);
        }
        const bool was_bezotted = bezotted_in(old_level.branch);
        if (bezotted())
        {
            if (was_bezotted)
                mpr("Zot already knows this place too well. Descend or flee this branch!");
            else
                mpr("Zot's attention fixes on you again. Descend or flee this branch!");
#if TAG_MAJOR_VERSION == 34
            if (you.species == SP_METEORAN)
                update_vision_range();
#endif
        }
        else if (was_bezotted)
        {
            if (branch == BRANCH_ABYSS)
                mpr("Zot has no power in the Abyss.");
            else
                mpr("You feel Zot lose track of you.");
#if TAG_MAJOR_VERSION == 34
            if (you.species == SP_METEORAN)
                update_vision_range();
#endif
        }
        print_gem_warnings(gem_for_branch(branch), 0);

        if (how == DNGN_ENTER_VAULTS && !runes_in_pack())
        {
            lock_vaults();
            mpr("The door slams shut behind you.");
        }

        if (branch == BRANCH_GAUNTLET)
            _gauntlet_effect();

        if (branch == BRANCH_ARENA)
            okawaru_duel_healing();

        const set<branch_type> boring_branch_exits = {
            BRANCH_TEMPLE,
            BRANCH_BAZAAR,
            BRANCH_TROVE
        };

        // Did we leave a notable branch for the first time?
        if (boring_branch_exits.count(old_level.branch) == 0
            && !you.branches_left[old_level.branch])
        {
            string old_branch_string = branches[old_level.branch].longname;
            if (starts_with(old_branch_string, "The "))
                old_branch_string[0] = tolower_safe(old_branch_string[0]);
            mark_milestone("br.exit", "left " + old_branch_string + ".",
                           old_level.describe());
            you.branches_left.set(old_level.branch);
        }
        if (how == branches[branch].entry_stairs)
        {
            const string noise_desc = branch_noise_desc(branch);
            if (!noise_desc.empty())
                mpr(noise_desc);

            const string rune_msg = branch_rune_desc(branch, true);
            if (!rune_msg.empty())
                mpr(rune_msg);
        }

        // Entered a branch from its parent.
        if (parent_branch(branch) == old_level.branch)
            enter_branch(branch, old_level);
    }

    // Warn Formicids if they cannot shaft here
    if (player_has_ability(ABIL_SHAFT_SELF, true)
                                && !is_valid_shaft_level(false))
    {
        mpr("Beware, you cannot shaft yourself on this level.");
    }

    const auto speed = dest_known ? LOAD_ENTER_LEVEL : LOAD_ENTER_LEVEL_FAST;
    const bool newlevel = load_level(how, speed, old_level);

    if (newlevel)
    {
        _new_level_amuses_xom(how, whence, shaft,
                              (shaft ? whither.depth - old_level.depth : 1),
                              !forced);

        // scary hack!
        if (crawl_state.game_is_descent() && !env.properties.exists(DESCENT_STAIRS_KEY))
            load_level(how, LOAD_RESTART_GAME, old_level);
    }

    // This should maybe go in load_level?
    if (you.where_are_you == BRANCH_ABYSS)
        generate_random_blood_spatter_on_level();

    you.turn_is_over = true;

    // This save point is somewhat odd, in that it corresponds to
    // a time when the player didn't have control. So, force a save after the
    // next world_reacts. XX should this be later in this function? Or not
    // here at all? (the hup check in save_game_state should be, though)
    crawl_state.save_after_turn = true;
    save_game_state();

    new_level();

    moveto_location_effects(whence);
    if (is_hell_subbranch(you.where_are_you))
        _hell_effects();

    if (you.unrand_equipped(UNRAND_VAINGLORY))
        _vainglory_arrival();

    trackers_init_new_level();

    if (update_travel_cache && !shaft)
        _update_travel_cache(old_level, stair_pos);

    // Preventing obvious finding of stairs at your position.
    env.map_seen.set(you.pos());

    viewwindow();
    update_screen();

    // There's probably a reason for this. I don't know it.
    if (going_up)
        seen_monsters_react();
    else
        maybe_update_stashes();

    autotoggle_autopickup(false);
    request_autopickup();
}

/**
 * Try to go up or down stairs.
 *
 * @param force_stair         The type of stair/portal to take. By default,
 *      use whatever tile is under the player. But this can be overridden
 *      (e.g. passing DNGN_EXIT_ABYSS forces the player out of the abyss)
 * @param going_up            True if the player is going upstairs
 * @param force_known_shaft   True if player is shafting themselves via ability.
 * @param update_travel_cache True if travel cache should be updated.
 */
void take_stairs(dungeon_feature_type force_stair, bool going_up,
                 bool force_known_shaft, bool update_travel_cache)
{
    const dungeon_feature_type old_feat = orig_terrain(you.pos());
    dungeon_feature_type how = force_stair ? force_stair : old_feat;

    // Taking a shaft manually (stepping on a known shaft, or using shaft ability)
    const bool known_shaft = (!force_stair
                              && get_trap_type(you.pos()) == TRAP_SHAFT)
                             || (force_stair == DNGN_TRAP_SHAFT
                                 && force_known_shaft);
    // Latter case is falling down a shaft.
    const bool shaft = known_shaft || force_stair == DNGN_TRAP_SHAFT;

    level_id whither = _travel_destination(how, bool(force_stair), going_up,
                                           known_shaft);

    if (!_level_transition_moves_player(whither, old_feat, going_up))
        return;

    // The transition is "forced" for the purpose of floor_transition if
    // a force_stair feature is specified and force_known_shaft is not set
    // (in the latter case, the player 'moved').
    floor_transition(how, old_feat, whither,
                     bool(force_stair) && !force_known_shaft,
                     going_up, shaft, update_travel_cache);
}

void up_stairs(dungeon_feature_type force_stair, bool update_travel_cache)
{
    take_stairs(force_stair, true, false, update_travel_cache);
}

// Find the other end of the stair or portal at location pos on the current
// level. for_real is true if we are actually traversing the feature rather
// than merely asking what is on the other side.
level_id stair_destination(coord_def pos, bool for_real)
{
    return stair_destination(orig_terrain(pos),
                             env.markers.property_at(pos, MAT_ANY, "dst"),
                             for_real);
}

// Find the other end of a stair or portal on the current level. feat is the
// type of feature (DNGN_EXIT_ABYSS, for example), dst is the target of a
// portal vault entrance (and is ignored for other types of features), and
// for_real is true if we are actually traversing the feature rather than
// merely asking what is on the other side.
level_id stair_destination(dungeon_feature_type feat, const string &dst,
                           bool for_real)
{
#if TAG_MAJOR_VERSION == 34
    if (feat == DNGN_ESCAPE_HATCH_UP && player_in_branch(BRANCH_LABYRINTH))
        feat = DNGN_EXIT_LABYRINTH;
#else
    UNUSED(dst); // see below in the switch
#endif
    if (branches[you.where_are_you].exit_stairs == feat
        && parent_branch(you.where_are_you) < NUM_BRANCHES
        && feat != DNGN_EXIT_ZIGGURAT)
    {
        level_id lev = brentry[you.where_are_you];
        if (!lev.is_valid())
        {
            // Wizmode, the branch wasn't generated this game.
            // Pick the middle of the range instead.
            lev = level_id(branches[you.where_are_you].parent_branch,
                           (branches[you.where_are_you].mindepth
                            + branches[you.where_are_you].maxdepth) / 2);
            ASSERT(lev.is_valid());
        }

        return lev;
    }

    if (feat_is_portal_exit(feat))
        feat = DNGN_EXIT_PANDEMONIUM;

    switch (feat)
    {
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        if (you.depth <= 1)
        {
            if (you.wizard && !for_real)
                return level_id();
            die("upstairs from top of a branch");
        }
        return level_id(you.where_are_you, you.depth - 1);

    case DNGN_EXIT_HELL:
        // If set, it would be found as a branch exit.
        if (you.wizard)
        {
            if (for_real)
            {
                mprf(MSGCH_ERROR, "Error: no Hell exit level, how in the "
                                  "Vestibule did you get here? Let's go to D:1.");
            }
            return level_id(BRANCH_DUNGEON, 1);
        }
        else
            die("hell exit without return destination");

    case DNGN_ABYSSAL_STAIR:
        ASSERT(player_in_branch(BRANCH_ABYSS));
        push_features_to_abyss();
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    {
        ASSERT(!at_branch_bottom());
        level_id lev = level_id::current();
        lev.depth++;
        return lev;
    }

    case DNGN_TRANSIT_PANDEMONIUM:
        return level_id(BRANCH_PANDEMONIUM);

    case DNGN_EXIT_THROUGH_ABYSS:
        return level_id(BRANCH_ABYSS);

#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_PORTAL_VAULT:
        if (dst.empty())
        {
            if (for_real)
                die("portal without a destination");
            else
                return level_id();
        }
        try
        {
            return level_id::parse_level_id(dst);
        }
        catch (const bad_level_id &err)
        {
            die("Invalid destination for portal: %s", err.what());
        }
#endif

    case DNGN_ENTER_HELL:
        if (for_real)
            brentry[BRANCH_VESTIBULE] = level_id::current();
        return level_id(BRANCH_VESTIBULE);

    case DNGN_EXIT_DIS:
    case DNGN_EXIT_GEHENNA:
    case DNGN_EXIT_COCYTUS:
    case DNGN_EXIT_TARTARUS:
        return level_id(BRANCH_VESTIBULE);

    case DNGN_EXIT_ABYSS:
#if TAG_MAJOR_VERSION == 34
        if (you.char_class == JOB_ABYSSAL_KNIGHT && you.level_stack.empty())
            return level_id(BRANCH_DUNGEON, 1);
    case DNGN_EXIT_PORTAL_VAULT:
#endif
    case DNGN_EXIT_PANDEMONIUM:
        if (you.level_stack.empty())
        {
            if (you.wizard)
            {
                if (for_real)
                {
                    mprf(MSGCH_ERROR, "Error: no return path. You did create "
                         "the exit manually, didn't you? Let's go to D:1.");
                }
                return level_id(BRANCH_DUNGEON, 1);
            }
            die("no return path from a portal (%s)",
                level_id::current().describe().c_str());
        }
        return you.level_stack.back().id;
    case DNGN_ENTER_ABYSS:
        push_features_to_abyss();
        break;
    default:
        break;
    }

    // Try to find a branch stair.
    for (branch_iterator it; it; ++it)
    {
        if (it->entry_stairs == feat)
            return level_id(it->id);
    }

    return level_id();
}

void down_stairs(dungeon_feature_type force_stair, bool force_known_shaft, bool update_travel_cache)
{
    take_stairs(force_stair, false, force_known_shaft, update_travel_cache);
}

static void _update_level_state()
{
    env.level_state = 0;

    vector<coord_def> golub = find_golubria_on_level();
    if (!golub.empty())
        env.level_state |= LSTATE_GOLUBRIA;

    for (monster_iterator mon_it; mon_it; ++mon_it)
    {
        if (mons_offers_beogh_conversion(**mon_it))
            env.level_state |= LSTATE_BEOGH;
        if (mon_it->has_ench(ENCH_STILL_WINDS))
            env.level_state |= LSTATE_STILL_WINDS;
        if (mon_it->has_ench(ENCH_AWAKEN_FOREST))
        {
            env.forest_awoken_until
                = you.elapsed_time
                  + mon_it->get_ench(ENCH_AWAKEN_FOREST).duration;
        }
    }

#if TAG_MAJOR_VERSION == 34
    const bool have_ramparts = you.duration[DUR_FROZEN_RAMPARTS];
    const auto &ramparts_pos = you.props[FROZEN_RAMPARTS_KEY].get_coord();
#endif
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_SLIMY_WALL)
            env.level_state |= LSTATE_SLIMY_WALL;

        if (is_icecovered(*ri))
#if TAG_MAJOR_VERSION == 34
        {
            // Buggy versions of Frozen Ramparts didn't properly clear
            // FPROP_ICY from walls in some cases, so we detect invalid walls
            // and remove the flag.
            if (have_ramparts
                && ramparts_pos.distance_from(*ri) <= 3
                && cell_see_cell(*ri, ramparts_pos, LOS_NO_TRANS))
            {
#endif
            env.level_state |= LSTATE_ICY_WALL;
#if TAG_MAJOR_VERSION == 34
            }
            else
                env.pgrid(*ri) &= ~FPROP_ICY;
        }
#endif
    }

    env.orb_pos = coord_def();
    if (item_def* orb = find_floor_item(OBJ_ORBS, ORB_ZOT))
        env.orb_pos = orb->pos;
    else if (player_has_orb() || you.unrand_equipped(UNRAND_CHARLATANS_ORB))
    {
        if (player_has_orb())
            env.orb_pos = you.pos();
        invalidate_agrid(true);
    }
}

void new_level(bool restore)
{
    print_stats_level();
    update_whereis();

    _update_level_state();

    if (restore)
        return;

    cancel_polar_vortex();

    if (player_in_branch(BRANCH_ZIGGURAT))
        you.zig_max = max(you.zig_max, you.depth);
}

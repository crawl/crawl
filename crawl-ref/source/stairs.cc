#include "AppHdr.h"

#include "stairs.h"

#include <sstream>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "chardump.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-passive.h" // passive_t::slow_abyss
#include "hints.h"
#include "hiscores.h"
#include "item-name.h"
#include "items.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-death.h"
#include "notes.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-other.h"
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#ifdef USE_TILE_LOCAL
 #include "tilepick.h"
#endif
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

bool check_annotation_exclusion_warning()
{
    level_id  next_level_id = level_id::get_next_level_id(you.pos());

    crawl_state.level_annotation_shown = false;
    bool might_be_dangerous = false;

    if (level_annotation_has("!", next_level_id)
        && next_level_id != level_id::current()
        && is_connected_branch(next_level_id))
    {
        mprf(MSGCH_PROMPT, "Warning, next level annotated: <yellow>%s</yellow>",
             get_level_annotation(next_level_id).c_str());
        might_be_dangerous = true;
        crawl_state.level_annotation_shown = true;
    }
    else if (is_exclude_root(you.pos())
             && feat_is_travelable_stair(grd(you.pos()))
             && !strstr(get_exclusion_desc(you.pos()).c_str(), "cloud"))
    {
        mprf(MSGCH_WARN, "This staircase is marked as excluded!");
        might_be_dangerous = true;
    }

    if (might_be_dangerous
        && !yesno("Enter next level anyway?", true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        interrupt_activity(AI_FORCE_INTERRUPT);
        crawl_state.level_annotation_shown = false;
        return false;
    }
    return true;
}

static void _player_change_level_reset()
{
    you.prev_targ  = MHITNOT;
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

    if (stair != grd(you.pos()))
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
    string stair_str = feature_description_at(you.pos(), false, DESC_THE, false);

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
        mpr("The world spins around you as you enter the gateway.");
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
    else
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

static void _clear_prisms()
{
    for (auto &mons : menv_real)
        if (mons.type == MONS_FULMINANT_PRISM)
            mons.reset();
}

void leaving_level_now(dungeon_feature_type stair_used)
{
    process_sunlights(true);

    if (stair_used == DNGN_EXIT_ZIGGURAT)
    {
        if (you.depth == 27)
            you.zigs_completed++;
        mark_milestone("zig.exit", make_stringf("left a ziggurat at level %d.",
                       you.depth));
    }

    dungeon_events.fire_position_event(DET_PLAYER_CLIMBS, you.pos());
    dungeon_events.fire_event(DET_LEAVING_LEVEL);

    _clear_golubria_traps();
    _clear_prisms();

    end_recall();
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
                mpr("This shop appears to be closed.");
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
        // Tt doesn't cancel the level transition.
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
    // Nothing even remotely flashy for Zig.
    if (ftype != DNGN_ENTER_ZIGGURAT)
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
            tiles.add_overlay(you.pos(), tileidx_zap(rune_colour(runes[2])));
            update_screen();
#else
            flash_view(UA_BRANCH_ENTRY, rune_colour(runes[2]));
#endif
            mpr("The lock glows eerily!");
            // included in default force_more_message

            mprf("You insert the %s rune into the lock.", rune_type_name(runes[1]));
            big_cloud(CLOUD_BLUE_SMOKE, &you, you.pos(), 20, 7 + random2(7));
            viewwindow();
            mpr("Heavy smoke blows from the lock!");
            // included in default force_more_message
        }

        mprf("You insert the %s rune into the lock.", rune_type_name(runes[0]));

        if (silenced(you.pos()))
            mpr("The gate opens wide!");
        else
            mpr("With a loud hiss the gate opens wide!");
        // these are included in default force_more_message
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

    case BRANCH_LABYRINTH:
        // Finding the way out of a labyrinth interests Xom.
        xom_is_stimulated(75);
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

static level_id _travel_destination(const dungeon_feature_type how,
                                    const dungeon_feature_type whence,
                                    bool forced, bool going_up,
                                    bool known_shaft)
{
    const bool shaft = known_shaft || how == DNGN_TRAP_SHAFT;
    level_id shaft_dest;
    level_id dest;
    if (shaft)
    {
        if (!is_valid_shaft_level())
        {
            if (known_shaft)
                mpr("The shaft disappears in a puff of logic!");
            _maybe_destroy_shaft(you.pos());
            return dest;
        }

        shaft_dest = you.shaft_dest(known_shaft);
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
        going_up = (how == DNGN_ENTER_HELL && player_in_hell());

    if (_stair_moves_pre(how))
        return dest;

    // Falling down is checked before the transition if going upstairs, since
    // it might prevent the transition itself.
    if (going_up && _check_fall_down_stairs(how, true))
        // TODO: This probably causes an obscure bug where confused players
        // going 'down' into the vestibule are twice as likely to fall, because
        // they have to pass a check here, and later in floor_transition 
        // Right solution is probably to use the canonicalized direction everywhere
        return dest;

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

        string howfar;
        if (shaft_depth > 1)
            howfar = make_stringf(" for %d floors", shaft_depth);

        mprf("You %s a shaft%s!", you.airborne() ? "are sucked into"
                                                 : "fall through",
                                  howfar.c_str());

        // Shafts are one-time-use.
        mpr("The shaft crumbles and collapses.");
        _maybe_destroy_shaft(you.pos());
    }

    // Maybe perform the entry sequence (we check that they have enough runes
    // in main.cc: _can_take_stairs())
    for (branch_iterator it; it; ++it)
    {
        if (how != it->entry_stairs)
            continue;

        if (!is_existing_level(level_id(it->id, 1))
            && runes_for_branch(it->id) > 0)
        {
            _rune_effect(how);
        }

        break;
    }

    // Markers might be deleted when removing portals.
    const string dst = env.markers.property_at(you.pos(), MAT_ANY, "dst");

    if (shaft)
        return shaft_dest;
    else
        return stair_destination(how, dst, true);
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
                      bool forced, bool going_up, bool shaft)
{
    const level_id old_level = level_id::current();

    // Clean up fake blood.
    heal_flayed_effect(&you, true, true);

    // Magical level changes (which currently only exist "downwards") need this.
    clear_trapping_net();
    end_searing_ray();

    // Fire level-leaving trigger.
    leaving_level_now(how);

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
    else if (how == DNGN_EXIT_ABYSS
             && you.chapter != CHAPTER_POCKET_ABYSS)
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

    if (how == DNGN_EXIT_DUNGEON)
    {
        you.depth = 0;
        mpr("You have escaped!");

        if (player_has_orb())
            ouch(INSTANT_DEATH, KILLED_BY_WINNING);

        ouch(INSTANT_DEATH, KILLED_BY_LEAVING);
    }

    if (how == DNGN_ENTER_LABYRINTH || how == DNGN_ENTER_ZIGGURAT)
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

    // Check for falling down the stairs or portal. (Why can't you fall in the abyss?)
    if (!going_up && !shaft
        && how != DNGN_ENTER_ABYSS
        && how != DNGN_ABYSSAL_STAIR
        && how != DNGN_EXIT_ABYSS)
    {
        _check_fall_down_stairs(how, false);
    }

    if (shaft)
        how = DNGN_TRAP_SHAFT;

    switch (you.where_are_you)
    {
    case BRANCH_ABYSS:
        // There are no abyssal stairs that go up, so this whole case is only
        // when going down.
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
        you.props.erase(ABYSS_SPAWNED_XP_EXIT_KEY);

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

        // Did we leave a notable branch for the first time?
        if ((brdepth[old_level.branch] > 1
             || old_level.branch == BRANCH_VESTIBULE)
            && !you.branches_left[old_level.branch])
        {
            string old_branch_string = branches[old_level.branch].longname;
            if (starts_with(old_branch_string, "The "))
                old_branch_string[0] = tolower(old_branch_string[0]);
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

        // Entered a regular (non-portal) branch from above.
        if (!going_up && parent_branch(branch) == old_level.branch)
            enter_branch(branch, old_level);
    }

    // Warn Formicids if they cannot shaft here
    if (you.species == SP_FORMICID && !is_valid_shaft_level())
        mpr("Beware, you cannot shaft yourself on this level.");

    const bool newlevel = load_level(how, LOAD_ENTER_LEVEL, old_level);

    if (newlevel)
    {
        _new_level_amuses_xom(how, whence, shaft,
                              (shaft ? whither.depth - old_level.depth : 1),
                              !forced);
    }

    // This should maybe go in load_level?
    if (you.where_are_you == BRANCH_ABYSS)
        generate_random_blood_spatter_on_level();

    you.turn_is_over = true;

    save_game_state();

    new_level();

    // Dunno why this is on going down only.
    if (!going_up)
    {
        moveto_location_effects(whence);

        // Clear list of beholding and constricting/constricted monsters.
        you.clear_beholders();
        you.stop_constricting_all();
        you.stop_being_constricted();

        trackers_init_new_level(true);
    }

    you.clear_fearmongers();

    if (!you.wizard && !shaft)
        _update_travel_cache(old_level, stair_pos);

    // Preventing obvious finding of stairs at your position.
    env.map_seen.set(you.pos());

    viewwindow();

    // There's probably a reason for this. I don't know it.
    if (going_up)
        seen_monsters_react();
    else
        maybe_update_stashes();

    request_autopickup();
}

/**
 * Try to go up or down stairs.
 *
 * @param force_stair The type of stair/portal to take. By default, use whatever
 *      tile is under the player. But this can be overridden (e.g. passing
 *      DNGN_EXIT_ABYSS forces the player out of the abyss)
 * @param force_known_shaft true if the player is shafting themselves via ability
 */
void take_stairs(dungeon_feature_type force_stair, bool going_up,
                 bool force_known_shaft)
{
    const dungeon_feature_type old_feat = orig_terrain(you.pos());
    dungeon_feature_type how = force_stair ? force_stair : old_feat;

    // Taking a shaft manually (stepping on a known shaft, or using shaft ability)
    const bool known_shaft = (!force_stair
                              && get_trap_type(you.pos()) == TRAP_SHAFT
                              && how != DNGN_UNDISCOVERED_TRAP)
                             || (force_stair == DNGN_TRAP_SHAFT
                                 && force_known_shaft);
    // Latter case is falling down a shaft.
    const bool shaft = known_shaft || force_stair == DNGN_TRAP_SHAFT;

    level_id whither = _travel_destination(how, old_feat,
                           bool(force_stair), going_up, known_shaft);

    if (!whither.is_valid() && !(old_feat == DNGN_EXIT_DUNGEON && going_up))
        return;

    floor_transition(how, old_feat, whither,
                     bool(force_stair), going_up, shaft);
}

void up_stairs(dungeon_feature_type force_stair)
{
    take_stairs(force_stair, true, false);
}

// Find the other end of the stair or portal at location pos on the current
// level.  for_real is true if we are actually traversing the feature rather
// than merely asking what is on the other side.
level_id stair_destination(coord_def pos, bool for_real)
{
    return stair_destination(orig_terrain(pos),
                             env.markers.property_at(pos, MAT_ANY, "dst"),
                             for_real);
}

// Find the other end of a stair or portal on the current level.  feat is the
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
        if (for_real && !player_in_hell())
            brentry[BRANCH_VESTIBULE] = level_id::current();
        return level_id(BRANCH_VESTIBULE);

    case DNGN_EXIT_ABYSS:
        if (you.chapter == CHAPTER_POCKET_ABYSS)
            return level_id(BRANCH_DUNGEON, 1);
#if TAG_MAJOR_VERSION == 34
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

// TODO(Zannick): Fully merge with up_stairs into take_stairs.
void down_stairs(dungeon_feature_type force_stair, bool force_known_shaft)
{
    take_stairs(force_stair, false, force_known_shaft);
}

static bool _any_glowing_mold()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        if (glowing_mold(*ri))
            return true;
    for (monster_iterator mon_it; mon_it; ++mon_it)
        if (mon_it->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
            return true;

    return false;
}

static void _update_level_state()
{
    env.level_state = 0;

    vector<coord_def> golub = find_golubria_on_level();
    if (!golub.empty())
        env.level_state |= LSTATE_GOLUBRIA;

    if (_any_glowing_mold())
        env.level_state |= LSTATE_GLOW_MOLD;
    for (monster_iterator mon_it; mon_it; ++mon_it)
    {
        if (mons_allows_beogh(**mon_it))
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
    for (rectangle_iterator ri(0); ri; ++ri)
        if (grd(*ri) == DNGN_SLIMY_WALL)
            env.level_state |= LSTATE_SLIMY_WALL;

    env.orb_pos = coord_def();
    if (item_def* orb = find_floor_item(OBJ_ORBS, ORB_ZOT))
        env.orb_pos = orb->pos;
    else if (player_has_orb())
    {
        env.orb_pos = you.pos();
        invalidate_agrid(true);
    }
}

void new_level(bool restore)
{
    print_stats_level();
#ifdef DGL_WHEREIS
    whereis_record();
#endif

    _update_level_state();

    if (restore)
        return;

    cancel_tornado();

    if (player_in_branch(BRANCH_ZIGGURAT))
        you.zig_max = max(you.zig_max, you.depth);
}

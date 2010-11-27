#include "AppHdr.h"

#include "stairs.h"

#include "abyss.h"
#include "areas.h"
#include "branch.h"
#include "chardump.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "hints.h"
#include "hiscores.h"
#include "itemname.h"
#include "items.h"
#include "lev-pand.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "place.h"
#include "random.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-transloc.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "tagstring.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

bool check_annotation_exclusion_warning()
{
    // Players might not realise the implications of teleport mutations
    // in the labyrinth.
    if (grd(you.pos()) == DNGN_ENTER_LABYRINTH
        && player_mutation_level(MUT_TELEPORT))
    {
        mpr("Within the labyrinth you'll only be able to teleport away from "
            "the exit!");
        if (!yesno("Continue anyway?", false, 'N', true, false))
        {
            canned_msg(MSG_OK);
            interrupt_activity(AI_FORCE_INTERRUPT);
            return (false);
        }
        return (true);
    }

    level_id  next_level_id = level_id::get_next_level_id(you.pos());

    crawl_state.level_annotation_shown = false;
    bool might_be_dangerous = false;

    if (level_annotation_has("!", next_level_id)
        && next_level_id != level_id::current()
        && next_level_id.level_type == LEVEL_DUNGEON)
    {
        mpr("Warning, next level annotated: " +
            colour_string(get_level_annotation(next_level_id), YELLOW),
            MSGCH_PROMPT);
        might_be_dangerous = true;
        crawl_state.level_annotation_shown = true;
    }
    else if (is_exclude_root(you.pos()) &&
             feat_is_travelable_stair(grd(you.pos())))
    {
        mpr("This staircase is marked as excluded!", MSGCH_WARN);
        might_be_dangerous = true;
    }

    if (might_be_dangerous
        && !yesno("Enter next level anyway?", true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        interrupt_activity(AI_FORCE_INTERRUPT);
        crawl_state.level_annotation_shown = false;
        return (false);
    }
    return (true);
}

static void _player_change_level_reset()
{
    you.prev_targ  = MHITNOT;
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

    you.prev_grd_targ.reset();
}

static level_id _stair_destination_override()
{
    const std::string force_place =
        env.markers.property_at(you.pos(), MAT_ANY, "dstplace");
    if (!force_place.empty())
    {
        try
        {
            const level_id place = level_id::parse_level_id(force_place);
            return (place);
        }
        catch (const std::string &err)
        {
            end(1, false, "Marker set with invalid level name: %s",
                force_place.c_str());
        }
    }

    const level_id invalid;
    return (invalid);
}

static bool _stair_force_destination(const level_id &override)
{
    if (override.is_valid())
    {
        if (override.level_type == LEVEL_DUNGEON)
        {
            you.where_are_you = override.branch;
            you.absdepth0    = override.absdepth();
            you.level_type    = override.level_type;
        }
        else
        {
            you.level_type = override.level_type;
        }
        return (true);
    }
    return (false);
}

static void _player_change_level_upstairs(dungeon_feature_type stair_find,
                                          const level_id &place_override)
{
    if (_stair_force_destination(place_override))
        return;

    if (you.level_type == LEVEL_DUNGEON)
        you.absdepth0--;

    // Make sure we return to our main dungeon level... labyrinth entrances
    // in the abyss or pandemonium are a bit trouble (well the labyrinth does
    // provide a way out of those places, its really not that bad I suppose).
    if (level_type_exits_up(you.level_type))
        you.level_type = LEVEL_DUNGEON;

    if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        you.where_are_you = you.hell_branch;
        you.absdepth0 = you.hell_exit;
    }

    if (player_in_hell())
    {
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.absdepth0 = 27;
    }

    // Did we take a branch stair?
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].exit_stairs == stair_find
            && you.where_are_you == i)
        {
            you.where_are_you = branches[i].parent_branch;

            // If leaving a branch which wasn't generated in this
            // particular game (like the Swamp or Shoals), then
            // its startdepth is set to -1; compensate for that,
            // so we don't end up on "level -1".
            if (branches[i].startdepth == -1)
                you.absdepth0 += 2;
            break;
        }
    }
}

static bool _marker_vetoes_level_change()
{
    return (marker_vetoes_operation("veto_level_change"));
}

static bool _stair_moves_pre(dungeon_feature_type stair)
{
    if (crawl_state.prev_cmd == CMD_WIZARD)
        return (false);

    if (stair != grd(you.pos()))
        return (false);

    if (feat_stair_direction(stair) == CMD_NO_CMD)
        return (false);

    if (!you.duration[DUR_REPEL_STAIRS_CLIMB])
        return (false);

    int pct;
    if (you.duration[DUR_REPEL_STAIRS_MOVE])
        pct = 29;
    else
        pct = 50;

    // When the effect is still strong, the chance to actually catch a stair
    // is smaller. (Assuming the duration starts out at 500.)
    const int dur = std::max(0, you.duration[DUR_REPEL_STAIRS_CLIMB] - 200);
    pct += dur/20;

    if (!x_chance_in_y(pct, 100))
        return (false);

    // Get feature name before sliding stair over.
    std::string stair_str =
        feature_description(you.pos(), false, DESC_CAP_THE, false);

    if (!slide_feature_over(you.pos(), coord_def(-1, -1), false))
        return (false);

    std::string verb = stair_climb_verb(stair);

    mprf("%s moves away as you attempt to %s it!", stair_str.c_str(),
         verb.c_str());

    you.turn_is_over = true;

    return (true);
}

static bool _check_carrying_orb()
{
    // We never picked up the Orb, no problem.
    if (you.char_direction != GDT_ASCENDING)
        return (true);

    // So we did pick up the Orb. Now check whether we're carrying it.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined()
            && you.inv[i].base_type == OBJ_ORBS
            && you.inv[i].sub_type == ORB_ZOT)
        {
            return (true);
        }
    }
    return (yes_or_no("You're not carrying the Orb! Leave anyway"));
}

// Adds a dungeon marker at the point of the level where returning from
// a labyrinth or portal vault should drop the player.
static void _mark_portal_return_point(const coord_def &pos)
{
    // First toss all markers of this type. Stale markers are possible
    // if the player goes to the Abyss from a portal vault /
    // labyrinth, thus returning to this level without activating a
    // previous portal vault exit marker.
    const std::vector<map_marker*> markers = env.markers.get_all(MAT_FEATURE);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        if (dynamic_cast<map_feature_marker*>(markers[i])->feat ==
            DNGN_EXIT_PORTAL_VAULT)
        {
            env.markers.remove(markers[i]);
        }
    }

    if (!env.markers.find(pos, MAT_FEATURE))
    {
        map_feature_marker *mfeat =
            new map_feature_marker(pos, DNGN_EXIT_PORTAL_VAULT);
        env.markers.add(mfeat);
    }
}

static void _exit_stair_message(dungeon_feature_type stair, bool /* going_up */)
{
    if (feat_is_escape_hatch(stair))
        mpr("The hatch slams shut behind you.");
}

static void _climb_message(dungeon_feature_type stair, bool going_up,
                           level_area_type old_level_type)
{
    if (old_level_type != LEVEL_DUNGEON)
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
                 you.flight_mode() == FL_FLY ? "fly" :
                     (you.airborne() ? "float" : "slide"));
        }
    }
    else if (feat_is_gate(stair))
    {
        mprf("You %s %s through the gate.",
             you.flight_mode() == FL_FLY ? "fly" :
                   (you.airborne() ? "float" : "go"),
             going_up ? "up" : "down");
    }
    else
    {
        mprf("You %s %swards.",
             you.flight_mode() == FL_FLY ? "fly" :
                   (you.airborne() ? "float" : "climb"),
             going_up ? "up" : "down");
    }
}

static void _clear_golubria_traps()
{
    std::vector<coord_def> traps = find_golubria_on_level();
    for (std::vector<coord_def>::const_iterator it = traps.begin(); it != traps.end(); ++it)
    {
        trap_def *trap = find_trap(*it);
        if (trap && trap->type == TRAP_GOLUBRIA)
        {
            trap->destroy();
        }
    }
}

static void _leaving_level_now()
{
    // Note the name ahead of time because the events may cause markers
    // to be discarded.
    const std::string newtype =
        env.markers.property_at(you.pos(), MAT_ANY, "dst");

    // Extension to use for bones files.
    const std::string newext =
        env.markers.property_at(you.pos(), MAT_ANY, "dstext");

    const std::string oldname = you.level_type_name;
    std::string newname =
        env.markers.property_at(you.pos(), MAT_ANY, "dstname");

    std::string neworigin =
        env.markers.property_at(you.pos(), MAT_ANY, "dstorigin");

    const std::string oldname_abbrev = you.level_type_name_abbrev;
    std::string newname_abbrev =
        env.markers.property_at(you.pos(), MAT_ANY, "dstname_abbrev");

    dungeon_events.fire_position_event(DET_PLAYER_CLIMBS, you.pos());
    dungeon_events.fire_event(DET_LEAVING_LEVEL);

    // Lua scripts explicitly set level_type_name, so use that.
    if (you.level_type_name != oldname)
        newname = you.level_type_name;

    // Lua scripts explicitly set level_type_name_abbrev, so use that.
    if (you.level_type_name_abbrev != oldname_abbrev)
        newname_abbrev = you.level_type_name_abbrev;

    if (newname_abbrev.length() > MAX_NOTE_PLACE_LEN)
    {
        mprf(MSGCH_ERROR, "'%s' is too long for a portal vault name "
                          "abbreviation, truncating");
        newname_abbrev = newname_abbrev.substr(0, MAX_NOTE_PLACE_LEN);
    }

    you.level_type_origin = "";
    // Lua scripts explicitly set level_type_origin, so use that.
    if (!you.level_type_origin.empty())
        neworigin = you.level_type_origin;

    // Don't clobber level_type_name for stairs in portal vaults.
    if (you.level_type_name.empty() || !newname.empty()
        || you.level_type != LEVEL_PORTAL_VAULT)
    {
        you.level_type_name = newname;
    }

    // Don't clobber level_type_name_abbrev for stairs in portal vaults.
    if (you.level_type_name_abbrev.empty() || !newname_abbrev.empty()
        || you.level_type != LEVEL_PORTAL_VAULT)
    {
        you.level_type_name_abbrev = newname_abbrev;
    }

    if (you.level_type_tag.empty() || !newtype.empty()
        || you.level_type != LEVEL_PORTAL_VAULT)
    {
        you.level_type_tag = newtype;
    }

    const std::string spaced_tag = replace_all(you.level_type_tag, "_", " ");

    if (!you.level_type_tag.empty() && you.level_type_name.empty())
        you.level_type_name = spaced_tag;

    if (!you.level_type_name.empty() && you.level_type_name_abbrev.empty())
    {
        if (you.level_type_name.length() <= MAX_NOTE_PLACE_LEN)
            you.level_type_name_abbrev = you.level_type_name;
        else if (you.level_type_tag.length() <= MAX_NOTE_PLACE_LEN)
            you.level_type_name_abbrev = spaced_tag;
        else
        {
            const std::string shorter =
                you.level_type_name.length() < you.level_type_tag.length() ?
                    you.level_type_name : spaced_tag;

            you.level_type_name_abbrev = shorter.substr(0, MAX_NOTE_PLACE_LEN);
        }
    }

    if (!newext.empty())
        you.level_type_ext = newext;
    else if (!you.level_type_tag.empty())
        you.level_type_ext = lowercase_string(you.level_type_tag);

    if (you.level_type_ext.length() > 3)
        you.level_type_ext = you.level_type_ext.substr(0, 3);

    if (!neworigin.empty())
        you.level_type_origin = neworigin;
    else if (!you.level_type_name.empty())
    {
        std::string lname = lowercase_string(you.level_type_name);
        std::string article, prep;

        if (starts_with(lname, "level ")
            || lname.find(":") != std::string::npos)
        {
            prep = "on ";
        }
        else
            prep = "in ";

        if (starts_with(lname, "a ") || starts_with(lname, "an ")
            || starts_with(lname, "the ") || starts_with(lname, "level ")
            || lname.find(":") != std::string::npos)
        {
            ; // Doesn't need an article
        }
        else
        {
            char letter = you.level_type_name[0];
            if (isupper(letter))
                article = "the ";
            else if (is_vowel(letter))
                article = "an ";
            else
                article = "a ";
        }

        you.level_type_origin  = prep + article + you.level_type_name;
    }

    _clear_golubria_traps();
}

static void _set_entry_cause(entry_cause_type default_cause,
                             level_area_type old_level_type)
{
    ASSERT(default_cause != NUM_ENTRY_CAUSE_TYPES);

    if (old_level_type == you.level_type && you.entry_cause != EC_UNKNOWN)
        return;

    if (crawl_state.is_god_acting())
    {
        if (crawl_state.is_god_retribution())
            you.entry_cause = EC_GOD_RETRIBUTION;
        else
            you.entry_cause = EC_GOD_ACT;

        you.entry_cause_god = crawl_state.which_god_acting();
    }
    else if (default_cause != EC_UNKNOWN)
    {
        you.entry_cause     = default_cause;
        you.entry_cause_god = GOD_NO_GOD;
    }
    else
    {
        you.entry_cause     = EC_SELF_EXPLICIT;
        you.entry_cause_god = GOD_NO_GOD;
    }
}

static void _update_travel_cache(bool collect_travel_data,
                                 const level_id& old_level,
                                 const coord_def& stair_pos)
{
    if (collect_travel_data)
    {
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
            if (new_level_id == BRANCH_MAIN_DUNGEON
                && old_level == BRANCH_VESTIBULE_OF_HELL)
            {
                old_level_info.clear_stairs(DNGN_EXIT_HELL);
            }
            else
            {
                old_level_info.update_stair(stair_pos, lp, guess);
            }

            // We *guess* that going up a staircase lands us on a downstair,
            // and that we can descend that downstair and get back to where we
            // came from. This assumption is guaranteed false when climbing out
            // of one of the branches of Hell.
            if (new_level_id != BRANCH_VESTIBULE_OF_HELL
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
    else // !collect_travel_data
    {
        travel_cache.erase_level_info(old_level);
    }
}

void up_stairs(dungeon_feature_type force_stair,
               entry_cause_type entry_cause)
{
    dungeon_feature_type stair_find = (force_stair ? force_stair
                                       : grd(you.pos()));
    const level_id  old_level = level_id::current();

    // Up and down both work for shops.
    if (stair_find == DNGN_ENTER_SHOP)
    {
        shop();
        return;
    }

    // Up and down both work for portals.
    if (feat_is_bidirectional_portal(stair_find))
    {
        if (!(stair_find == DNGN_ENTER_HELL && player_in_hell())) {
            down_stairs(force_stair, entry_cause);
            return;
        }
    }
    // Probably still need this check here (teleportation) -- bwr
    else if (feat_stair_direction(stair_find) != CMD_GO_UPSTAIRS)
    {
        if (stair_find == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else if (stair_find == DNGN_ABANDONED_SHOP)
            mpr("This shop appears to be closed.");
        else
            mpr("You can't go up here.");
        return;
    }

    if (_stair_moves_pre(stair_find))
        return;

    // Since the overloaded message set turn_is_over, I'm assuming that
    // the overloaded character makes an attempt... so we're doing this
    // check before that one. -- bwr
    if (!you.airborne()
        && you.confused()
        && old_level.level_type == LEVEL_DUNGEON
        && !feat_is_escape_hatch(stair_find)
        && coinflip())
    {
        const char* fall_where = "down the stairs";
        if (!feat_is_staircase(stair_find))
            fall_where = "through the gate";

        mprf("In your confused state, you trip and fall back %s.", fall_where);
        if (!feat_is_staircase(stair_find))
            ouch(1, NON_MONSTER, KILLED_BY_FALLING_THROUGH_GATE);
        else
            ouch(1, NON_MONSTER, KILLED_BY_FALLING_DOWN_STAIRS);
        you.turn_is_over = true;
        return;
    }

    if (you.burden_state == BS_OVERLOADED && !feat_is_escape_hatch(stair_find)
        && !feat_is_gate(stair_find))
    {
        mpr("You are carrying too much to climb upwards.");
        you.turn_is_over = true;
        return;
    }

    const level_id destination_override(_stair_destination_override());
    const bool leaving_dungeon =
        level_id::current() == level_id(BRANCH_MAIN_DUNGEON, 1)
        && !destination_override.is_valid();

    if (leaving_dungeon)
    {
        bool stay = (!yesno("Are you sure you want to leave the Dungeon?",
                            false, 'n') || !_check_carrying_orb());

        if (!stay && Hints.hints_left)
        {
            if (!yesno("Are you *sure*?  Doing so will end the game!", false,
                       'n'))
            {
                stay = true;
            }
        }

        if (stay)
        {
            mpr("Alright, then stay!");
            return;
        }
    }

    // Bail if any markers veto the move.
    if (_marker_vetoes_level_change())
        return;

    // Magical level changes (don't exist yet in this direction)
    // need this.
    clear_trapping_net();

    // Checks are done, the character is committed to moving between levels.
    _leaving_level_now();

    // Interlevel travel data.
    const bool collect_travel_data = can_travel_interlevel();
    if (collect_travel_data)
    {
        LevelInfo &old_level_info = travel_cache.get_level_info(old_level);
        old_level_info.update();
    }

    _player_change_level_reset();
    _player_change_level_upstairs(stair_find, destination_override);

    if (you.absdepth0 < 0)
    {
        mpr("You have escaped!");

        for (int i = 0; i < ENDOFPACK; i++)
        {
            if (you.inv[i].defined()
                && you.inv[i].base_type == OBJ_ORBS)
            {
                ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_WINNING);
            }
        }

        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_LEAVING);
    }

    if (old_level.branch == BRANCH_VESTIBULE_OF_HELL
        && !player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        mpr("Thank you for visiting Hell. Please come again soon.");
    }

    // Fixup exits from the Hell branches.
    if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        switch (old_level.branch)
        {
        case BRANCH_COCYTUS:  stair_find = DNGN_ENTER_COCYTUS;  break;
        case BRANCH_DIS:      stair_find = DNGN_ENTER_DIS;      break;
        case BRANCH_GEHENNA:  stair_find = DNGN_ENTER_GEHENNA;  break;
        case BRANCH_TARTARUS: stair_find = DNGN_ENTER_TARTARUS; break;
        default: break;
        }
    }

    const dungeon_feature_type stair_taken = stair_find;

    if (you.flight_mode() == FL_LEVITATE && !feat_is_gate(stair_find))
        mpr("You float upwards... And bob straight up to the ceiling!");
    else if (you.flight_mode() == FL_FLY && !feat_is_gate(stair_find))
        mpr("You fly upwards.");
    else
        _climb_message(stair_find, true, old_level.level_type);

    _exit_stair_message(stair_find, true);

    if (old_level.branch != you.where_are_you && you.level_type == LEVEL_DUNGEON)
    {
        mprf("Welcome back to %s!",
             branches[you.where_are_you].longname);
    }

    const coord_def stair_pos = you.pos();

    load(stair_taken, LOAD_ENTER_LEVEL, old_level);

    _set_entry_cause(entry_cause, old_level.level_type);
    entry_cause = you.entry_cause;

    you.turn_is_over = true;

    save_game_state();

    new_level();

    _update_travel_cache(collect_travel_data, old_level, stair_pos);

    viewwindow();
    seen_monsters_react();

    // Left Zot without enough runes to get back in (because they were
    // destroyed), but need to get back in Zot to get the Orb?
    // Xom finds that funny.
    if (stair_find == DNGN_RETURN_FROM_ZOT
        && branches[BRANCH_HALL_OF_ZOT].branch_flags & BFLAG_HAS_ORB)
    {
        int runes_avail = you.attribute[ATTR_UNIQUE_RUNES]
                          + you.attribute[ATTR_DEMONIC_RUNES]
                          + you.attribute[ATTR_ABYSSAL_RUNES];

        if (runes_avail < NUMBER_OF_RUNES_NEEDED)
            xom_is_stimulated(255, "Xom snickers loudly.", true);
    }

    if (!allow_control_teleport(true))
        mpr("You sense a powerful magical force warping space.", MSGCH_WARN);

    request_autopickup();
}

// All changes to you.level_type, you.where_are_you and you.absdepth0
// for descending stairs should happen here.
static void _player_change_level_downstairs(dungeon_feature_type stair_find,
                                            const level_id &place_override,
                                            bool shaft,
                                            int shaft_level,
                                            const level_id &shaft_dest)
{
    if (_stair_force_destination(place_override))
        return;

    const level_area_type original_level_type(you.level_type);

    if (you.level_type != LEVEL_DUNGEON
        && (you.level_type != LEVEL_PANDEMONIUM
            || stair_find != DNGN_TRANSIT_PANDEMONIUM)
        && (you.level_type != LEVEL_PORTAL_VAULT
            || !feat_is_stone_stair(stair_find)))
    {
        you.level_type = LEVEL_DUNGEON;
    }

    if (stair_find == DNGN_ENTER_HELL)
    {
        you.hell_branch = you.where_are_you;
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.hell_exit = you.absdepth0;

        you.absdepth0 = 26;
    }

    // Welcome message.
    // Try to find a branch stair.
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].entry_stairs == stair_find)
        {
            you.where_are_you = branches[i].id;
            break;
        }
    }

    if (stair_find == DNGN_ENTER_LABYRINTH)
        you.level_type = LEVEL_LABYRINTH;
    else if (stair_find == DNGN_ENTER_ABYSS)
        you.level_type = LEVEL_ABYSS;
    else if (stair_find == DNGN_ENTER_PANDEMONIUM)
        you.level_type = LEVEL_PANDEMONIUM;
    else if (stair_find == DNGN_ENTER_PORTAL_VAULT)
        you.level_type = LEVEL_PORTAL_VAULT;

    if (shaft)
    {
        you.absdepth0    = shaft_level;
        you.where_are_you = shaft_dest.branch;
    }
    else if (original_level_type == LEVEL_DUNGEON
             && you.level_type == LEVEL_DUNGEON)
    {
        you.absdepth0++;
    }
}

static void _maybe_destroy_trap(const coord_def &p)
{
    trap_def* trap = find_trap(p);
    if (trap)
        trap->destroy();
}

int runes_in_pack(std::vector<int> &runes)
{
    int num_runes = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined()
            && you.inv[i].base_type == OBJ_MISCELLANY
            && you.inv[i].sub_type == MISC_RUNE_OF_ZOT)
        {
            num_runes += you.inv[i].quantity;
            for (int q = 1;
                 runes.size() < 3 && q <= you.inv[i].quantity; ++q)
            {
                runes.push_back(i);
            }
        }
    }

    return num_runes;
}

void down_stairs(dungeon_feature_type force_stair,
                 entry_cause_type entry_cause, const level_id* force_dest)
{
    const level_id old_level = level_id::current();
    const dungeon_feature_type stair_find =
        force_stair? force_stair : grd(you.pos());

    const bool shaft = (!force_stair
                            && get_trap_type(you.pos()) == TRAP_SHAFT
                        || force_stair == DNGN_TRAP_NATURAL);
    level_id shaft_dest;
    int      shaft_level = -1;

    // Up and down both work for shops.
    if (stair_find == DNGN_ENTER_SHOP)
    {
        shop();
        return;
    }

    // Up and down both work for portals.
    if (feat_is_bidirectional_portal(stair_find))
    {
        if (stair_find == DNGN_ENTER_HELL && player_in_hell()) {
            up_stairs(force_stair, entry_cause);
            return;
        }
    }
    // Probably still need this check here (teleportation) -- bwr
    else if (feat_stair_direction(stair_find) != CMD_GO_DOWNSTAIRS && !shaft)
    {
        if (stair_find == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else if (stair_find == DNGN_ABANDONED_SHOP)
            mpr("This shop appears to be closed.");
        else
            mpr("You can't go down here!");
        return;
    }

    if (stair_find == DNGN_ENTER_HELL && you.level_type != LEVEL_DUNGEON)
    {
        mpr("You can't enter Hell from outside the dungeon!",
            MSGCH_ERROR);
        return;
    }

    if (stair_find > DNGN_ENTER_LABYRINTH
        && stair_find <= DNGN_ESCAPE_HATCH_DOWN
        && player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        // Down stairs in vestibule are one-way!
        // This doesn't make any sense. Why would there be any down stairs
        // in the Vestibule? {due, 9/2010}
        mpr("A mysterious force prevents you from descending the staircase.");
        return;
    }

    if (stair_find == DNGN_STONE_ARCH)
    {
        mpr("There is nothing on the other side of the stone arch.");
        return;
    }

    if (!force_stair && you.flight_mode() == FL_LEVITATE
        && !feat_is_gate(stair_find))
    {
        mpr("You're floating high up above the floor!");
        return;
    }

    if (_stair_moves_pre(stair_find))
        return;

    if (shaft)
    {
        const bool known_trap = (grd(you.pos()) != DNGN_UNDISCOVERED_TRAP
                                 && !force_stair);

        if (you.flight_mode() == FL_LEVITATE && !force_stair)
        {
            if (known_trap)
                mpr("You can't fall through a shaft while levitating.");
            return;
        }

        if (!is_valid_shaft_level())
        {
            if (known_trap)
                mpr("The shaft disappears in a puff of logic!");
            _maybe_destroy_trap(you.pos());
            return;
        }

        shaft_dest = you.shaft_dest(known_trap);
        if (shaft_dest == level_id::current())
        {
            if (known_trap)
            {
                mpr("Strange, the shaft seems to lead back to this level.");
                mpr("The strain on the space-time continuum destroys the "
                    "shaft!");
            }
            _maybe_destroy_trap(you.pos());
            return;
        }
        shaft_level = absdungeon_depth(shaft_dest.branch, shaft_dest.depth);

        if (!known_trap && shaft_level - you.absdepth0 > 1)
            mark_milestone("shaft", "fell down a shaft to " +
                                    short_place_name(shaft_dest) + ".");

        if (you.flight_mode() != FL_FLY || force_stair)
            mpr("You fall through a shaft!");
        if (you.flight_mode() == FL_FLY && !force_stair)
            mpr("You dive down through the shaft.");

        // Shafts are one-time-use.
        mpr("The shaft crumbles and collapses.");
        _maybe_destroy_trap(you.pos());
    }

    if (stair_find == DNGN_ENTER_ZOT && !you.opened_zot)
    {
        std::vector<int> runes;
        const int num_runes = runes_in_pack(runes);

        if (num_runes < NUMBER_OF_RUNES_NEEDED)
        {
            switch (NUMBER_OF_RUNES_NEEDED)
            {
            case 1:
                mpr("You need a rune to enter this place.");
                break;

            default:
                mprf("You need at least %d runes to enter this place.",
                     NUMBER_OF_RUNES_NEEDED);
            }
            return;
        }

        ASSERT(runes.size() >= 3);

        mprf("You insert %s into the lock.",
             you.inv[runes[0]].name(DESC_NOCAP_THE).c_str());
#ifdef USE_TILE
        tiles.add_overlay(you.pos(), tileidx_zap(GREEN));
        update_screen();
#else
        flash_view(LIGHTGREEN);
#endif
        mpr("The lock glows an eerie green colour!");
        more();

        mprf("You insert %s into the lock.",
             you.inv[runes[1]].name(DESC_NOCAP_THE).c_str());
        big_cloud(CLOUD_BLUE_SMOKE, KC_YOU, you.pos(), 20, 7 + random2(7));
        viewwindow();
        mpr("Heavy smoke blows from the lock!");
        more();

        mprf("You insert %s into the lock.",
             you.inv[runes[2]].name(DESC_NOCAP_THE).c_str());

        if (silenced(you.pos()))
            mpr("The gate opens wide!");
        else
            mpr("With a loud hiss the gate opens wide!");
        more();

        you.opened_zot = true;
    }

    // Bail if any markers veto the move.
    if (_marker_vetoes_level_change())
        return;

    level_id destination_override = _stair_destination_override();
    if (force_dest)
        destination_override = *force_dest;

    // All checks are done, the player is on the move now.

    // Magical level changes (Portal, Banishment) need this.
    clear_trapping_net();

    // Fire level-leaving trigger.
    _leaving_level_now();

    if (!force_stair && !crawl_state.game_is_arena())
    {
        // Not entirely accurate - the player could die before
        // reaching the Abyss.
        if (grd(you.pos()) == DNGN_ENTER_ABYSS)
            mark_milestone("abyss.enter", "entered the Abyss!");
        else if (grd(you.pos()) == DNGN_EXIT_ABYSS
                 && you.char_direction != GDT_GAME_START)
        {
            mark_milestone("abyss.exit", "escaped from the Abyss!");
        }
    }

    // Interlevel travel data.
    const bool collect_travel_data = can_travel_interlevel();
    if (collect_travel_data)
    {
        LevelInfo &old_level_info = travel_cache.get_level_info(old_level);
        old_level_info.update();
    }
    const coord_def stair_pos = you.pos();

    // Preserve abyss uniques now, since this Abyss level will be deleted.
    if (you.level_type == LEVEL_ABYSS)
        save_abyss_uniques();

    // XXX: Obsolete, now that labyrinth entrances are only placed via Lua
    //      with timed markes. Leaving in to reduce the chance of an
    //      accidental permanent labyrinth entry. [rob]
    if (stair_find == DNGN_ENTER_LABYRINTH)
        dungeon_terrain_changed(you.pos(), DNGN_STONE_ARCH);

    if (stair_find == DNGN_ENTER_LABYRINTH
        || stair_find == DNGN_ENTER_PORTAL_VAULT)
    {
        _mark_portal_return_point(you.pos());
    }

    const int shaft_depth = (shaft ? shaft_level - you.absdepth0 : 1);
    _player_change_level_reset();
    _player_change_level_downstairs(stair_find, destination_override, shaft,
                                    shaft_level, shaft_dest);

    // When going downstairs into a special level, delete any previous
    // instances of it.
    if (you.level_type != LEVEL_DUNGEON)
    {
        std::string lname = get_level_filename(level_id::current());
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Deleting: %s", lname.c_str());
#endif
        you.save->delete_chunk(lname);
    }

    // Did we enter a new branch.
    const bool entered_branch(
        you.where_are_you != old_level.branch
        && branches[you.where_are_you].parent_branch == old_level.branch);

    if (stair_find == DNGN_EXIT_ABYSS || stair_find == DNGN_EXIT_PANDEMONIUM)
    {
        mpr("You pass through the gate.");
        if (!you.wizard || !crawl_state.is_replaying_keys())
            more();
    }

    if (old_level.level_type != you.level_type && you.level_type == LEVEL_DUNGEON)
        mprf("Welcome back to %s!", branches[you.where_are_you].longname);

    if (!you.airborne()
        && you.confused()
        && !feat_is_escape_hatch(stair_find)
        && force_stair != DNGN_ENTER_ABYSS
        && coinflip())
    {
        const char* fall_where = "down the stairs";
        if (!feat_is_staircase(stair_find))
            fall_where = "through the gate";

        mprf("In your confused state, you trip and fall %s.", fall_where);
        // Note that this only does damage; it doesn't cancel the level
        // transition.
        if (!feat_is_staircase(stair_find))
            ouch(1, NON_MONSTER, KILLED_BY_FALLING_THROUGH_GATE);
        else
            ouch(1, NON_MONSTER, KILLED_BY_FALLING_DOWN_STAIRS);
    }

    dungeon_feature_type stair_taken = stair_find;

    if (you.level_type == LEVEL_ABYSS)
        stair_taken = DNGN_FLOOR;

    if (you.level_type == LEVEL_PANDEMONIUM)
        stair_taken = DNGN_TRANSIT_PANDEMONIUM;

    if (shaft)
        stair_taken = DNGN_ESCAPE_HATCH_DOWN;

    switch (you.level_type)
    {
    case LEVEL_LABYRINTH:
        // XXX: Ideally, we want to hint at the wall rule (rock > metal),
        //      and that the walls can shift occasionally.
        // Are these too long?
        mpr("As you enter the labyrinth, previously moving walls settle noisily into place.");
        mpr("You hear the metallic echo of a distant snort before it fades into the rock.");
        mark_milestone("br.enter", "entered a Labyrinth.");
        break;

    case LEVEL_ABYSS:
        if (!force_stair)
            mpr("You enter the Abyss!");

        mpr("To return, you must find a gate leading back.");
        if (you.religion == GOD_CHEIBRIADOS)
        {
            mpr("You feel Cheibriados slowing down the madness of this place.",
                MSGCH_GOD, GOD_CHEIBRIADOS);
        }
        learned_something_new(HINT_ABYSS);
        break;

    case LEVEL_PANDEMONIUM:
        if (old_level.level_type == LEVEL_PANDEMONIUM)
            mpr("You pass into a different region of Pandemonium.");
        else
        {
            mpr("You enter the halls of Pandemonium!");
            mpr("To return, you must find a gate leading back.");
        }
        break;

    default:
        if (shaft)
            handle_items_on_shaft(you.pos(), false);
        else
            _climb_message(stair_find, false, old_level.level_type);
        break;
    }

    if (!shaft)
        _exit_stair_message(stair_find, false);

    if (entered_branch)
    {
        if (branches[you.where_are_you].entry_message)
            mpr(branches[you.where_are_you].entry_message);
        else
            mprf("Welcome to %s!", branches[you.where_are_you].longname);
    }

    if (stair_find == DNGN_ENTER_HELL)
    {
        mpr("Welcome to Hell!");
        mpr("Please enjoy your stay.");

        // Kill -more- prompt if we're traveling.
        if (!you.running && !force_stair)
            more();
    }

    const bool newlevel = load(stair_taken, LOAD_ENTER_LEVEL, old_level);

    _set_entry_cause(entry_cause, old_level.level_type);
    entry_cause = you.entry_cause;

    if (newlevel)
    {
        // When entering a new level, reset friendly_pickup to default.
        you.friendly_pickup = Options.default_friendly_pickup;

        switch (you.level_type)
        {
        case LEVEL_DUNGEON:
            // Xom thinks it's funny if you enter a new level via shaft
            // or escape hatch, for shafts it's funnier the deeper you fell.
            if (shaft || feat_is_escape_hatch(stair_find))
                xom_is_stimulated(shaft_depth * 50);
            else
                xom_is_stimulated(14);
            break;

        case LEVEL_PORTAL_VAULT:
            // Portal vaults aren't as interesting.
            xom_is_stimulated(25);
            break;

        case LEVEL_LABYRINTH:
            // Finding the way out of a labyrinth interests Xom,
            // but less so for Minotaurs. (though not now, as they cannot
            // map the labyrinth any more {due})
            xom_is_stimulated(98);
            break;

        case LEVEL_ABYSS:
        case LEVEL_PANDEMONIUM:
        {
            // Paranoia
            if (old_level.level_type == you.level_type)
                break;

            PlaceInfo &place_info = you.get_place_info();
            generate_random_blood_spatter_on_level();

            // Entering voluntarily only stimulates Xom if you've never
            // been there before
            if ((place_info.num_visits == 1 && place_info.levels_seen == 1)
                || entry_cause != EC_SELF_EXPLICIT)
            {
                if (crawl_state.is_god_acting())
                    xom_is_stimulated(255);
                else if (entry_cause == EC_SELF_EXPLICIT)
                {
                    // Entering Pandemonium or the Abyss for the first
                    // time *voluntarily* stimulates Xom much more than
                    // entering a normal dungeon level for the first time.
                    xom_is_stimulated(128, XM_INTRIGUED);
                }
                else if (entry_cause == EC_SELF_RISKY)
                    xom_is_stimulated(128);
                else
                    xom_is_stimulated(255);
            }

            break;
        }

        default:
            ASSERT(false);
        }
    }

    switch (you.level_type)
    {
    case LEVEL_ABYSS:
        grd(you.pos()) = DNGN_FLOOR;

        init_pandemonium();     // colours only
        break;

    case LEVEL_PANDEMONIUM:
        init_pandemonium();

        for (int pc = random2avg(28, 3); pc > 0; pc--)
            pandemonium_mons();
        break;

    default:
        break;
    }

    you.turn_is_over = true;

    save_game_state();

    new_level();

    // Clear list of beholding monsters.
    you.clear_beholders();
    you.clear_fearmongers();

    if (!allow_control_teleport(true))
        mpr("You sense a powerful magical force warping space.", MSGCH_WARN);

    trackers_init_new_level(true);

    // XXX: Using force_dest to decide whether to save stair info.
    //      Currently it's only used for Portal, where we don't
    //      want to mark the destination known.
    if (!force_dest)
        _update_travel_cache(collect_travel_data, old_level, stair_pos);

    // Preventing obvious finding of stairs at your position.
    env.map_shadow(you.pos()).flags |= MAP_SEEN_FLAG;

    viewwindow();

    // Checking new squares for interesting features.
    if (!you.running)
        check_for_interesting_features();

    maybe_update_stashes();

    request_autopickup();

    // Zotdef: returning from portals (e.g. bazaar) paralyses the player in place for 5 moves. Nasty,
    // but punishes players for using portals as quick-healing stopovers.
    if (crawl_state.game_is_zotdef()) start_delay(DELAY_UNINTERRUPTIBLE, 5);

}

void new_level(void)
{
    cancel_tornado();

    if (you.level_type == LEVEL_PORTAL_VAULT)
    {
        // This here because place_name can't find the name of a level that you
        // *are no longer on* when it spits out the new notes list.
        std::string desc = "Entered " + place_name(get_packed_place(), true, true);
        take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE, 0, 0, NULL,
                      desc.c_str()));
    }
    else
        take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE));

    print_stats_level();
#ifdef DGL_WHEREIS
    whereis_record();
#endif
}

// Returns a hatch or stair (up or down)
dungeon_feature_type random_stair()
{
    return (static_cast<dungeon_feature_type>(
        DNGN_STONE_STAIRS_DOWN_I+random2(
            DNGN_ESCAPE_HATCH_UP-DNGN_STONE_STAIRS_DOWN_I+1)));
}

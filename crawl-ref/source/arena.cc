/**
 * @file
 * @brief Functions related to the monster arena (stage and watch fights).
**/

#include "AppHdr.h"

#include "arena.h"

#include <stdexcept>

#include "act-iter.h"
#include "colour.h"
#include "command.h"
#include "dungeon.h"
#include "end.h"
#include "food.h"
#include "item-name.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "los.h"
#include "macro.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mgen-data.h"
#include "mon-death.h"
#include "mon-pick.h"
#include "mon-tentacle.h"
#include "ng-init.h"
#include "spl-miscast.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "unicode.h"
#include "version.h"
#include "view.h"

#define ARENA_VERBOSE

extern void world_reacts();

namespace arena
{
    static void write_error(const string &error);

    struct arena_error : public runtime_error
    {
        explicit arena_error(const string &msg) : runtime_error(msg) {}
        explicit arena_error(const char *msg) : runtime_error(msg) {}
    };
#define arena_error_f(...) arena_error(make_stringf(__VA_ARGS__))

    // A faction is just a big list of monsters. Monsters will be dropped
    // around the appropriate marker.
    struct faction
    {
        string desc;
        mons_list   members;
        bool        friendly;
        int         active_members;
        bool        won;

        vector<int>       respawn_list;
        vector<coord_def> respawn_pos;

        faction(bool fr) : members(), friendly(fr), active_members(0),
                           won(false) { }

        void place_at(const coord_def &pos);

        void reset()
        {
            active_members = 0;
            won            = false;

            respawn_list.clear();
            respawn_pos.clear();
        }

        void clear()
        {
            reset();
            members.clear();
        }
    };

    static string teams;

    static int total_trials = 0;

    static bool contest_cancelled = false;

    static bool is_respawning = false;

    static int trials_done = 0;
    static int team_a_wins = 0;
    static int ties        = 0;

    static int turns       = 0;

    static bool allow_summons       = true;
    static bool allow_animate       = true;
    static bool allow_chain_summons = true;
    static bool allow_zero_xp       = false;
    static bool allow_immobile      = true;
    static bool allow_bands         = true;
    static bool name_monsters       = false;
    static bool random_uniques      = false;
    static bool real_summons        = false;
    static bool move_summons        = false;
    static bool respawn             = false;
    static bool move_respawns       = false;

    static bool miscasts            = false;

    static int  summon_throttle     = INT_MAX;

    static vector<monster_type> uniques_list;
    static vector<int> a_spawners;
    static vector<int> b_spawners;
    static int8_t           to_respawn[MAX_MONSTERS];

    static int item_drop_times[MAX_ITEMS];

    static bool banned_glyphs[128];

    static string arena_type = "";
    static faction faction_a(true);
    static faction faction_b(false);
    static coord_def place_a, place_b;

    static bool cycle_random     = false;
    static uint32_t cycle_random_pos = 0;

    static FILE *file = nullptr;
    static level_id place(BRANCH_DEPTHS, 1);

    static void adjust_spells(monster* mons, bool no_summons, bool no_animate)
    {
        monster_spells &spells(mons->spells);
        erase_if(spells, [&](const mon_spell_slot &t) {
            return (no_summons && spell_typematch(t.spell, SPTYP_SUMMONING))
                || (no_animate && t.spell == SPELL_ANIMATE_DEAD);
        });
    }

    static void adjust_monsters()
    {
        for (monster_iterator mon; mon; ++mon)
        {
            const bool friendly = mon->friendly();
            // Set target to the opposite faction's home base.
            mon->target = friendly ? place_b : place_a;
        }
    }

    static void list_eq(const monster *mon)
    {
        if (!Options.arena_list_eq || file == nullptr)
            return;

        vector<int> items;
        for (short it : mon->inv)
            if (it != NON_ITEM)
                items.push_back(it);

        if (items.empty())
            return;

        fprintf(file, "%s:\n", mon->name(DESC_PLAIN, true).c_str());

        for (int iidx : items)
        {
            item_def &item = mitm[iidx];
            fprintf(file, "        %s\n",
                    item.name(DESC_PLAIN, false, true).c_str());
        }
    }

    void faction::place_at(const coord_def &pos)
    {
        ASSERT_IN_BOUNDS(pos);
        for (int i = 0, size = members.size(); i < size; ++i)
        {
            mons_spec spec = members.get_monster(i);

            if (friendly)
                spec.attitude = ATT_FRIENDLY;

            for (int q = 0; q < spec.quantity; ++q)
            {
                const coord_def loc = pos;
                if (!in_bounds(loc))
                    break;

                const monster* mon = dgn_place_monster(spec,
                                                       loc, false, true, false);
                if (!mon)
                {
                    game_ended_with_error(
                        make_stringf(
                            "Failed to create monster at (%d,%d) grd: %s",
                            loc.x, loc.y, dungeon_feature_name(grd(loc))));
                }
                list_eq(mon);
                to_respawn[mon->mindex()] = i;
            }
        }
    }

    static void center_print(unsigned sz, string text, int number = -1)
    {
        if (number >= 0)
            text = make_stringf("(%d) %s", number, text.c_str());

        unsigned len = strwidth(text);
        if (len > sz)
            text = chop_string(text, len = sz);

        cprintf("%s%s", string((sz - len) / 2, ' ').c_str(), text.c_str());
    }

    static void setup_level()
    {
        turns = 0;

        a_spawners.clear();
        b_spawners.clear();
        memset(item_drop_times, 0, sizeof(item_drop_times));

        if (place.is_valid())
        {
            you.where_are_you = place.branch;
            you.depth         = place.depth;
        }

        dgn_reset_level();

        for (int x = 0; x < GXM; ++x)
            for (int y = 0; y < GYM; ++y)
                grd[x][y] = DNGN_ROCK_WALL;

        unwind_bool gen(crawl_state.generating_level, true);

        typedef unwind_var< set<string> > unwind_stringset;

        const unwind_stringset mtags(you.uniq_map_tags);
        const unwind_stringset mnames(you.uniq_map_names);

        string map_name = "arena_" + arena_type;
        const map_def *map = random_map_for_tag(map_name);

        if (!map)
        {
            throw arena_error_f("No arena maps named \"%s\"",
                                arena_type.c_str());
        }

#ifdef USE_TILE
        // Arena is never saved, so we can skip this.
        tile_init_default_flavour();
        tile_clear_flavour();
#endif

        ASSERT(map);
        bool success = dgn_place_map(map, false, true);
        if (!success)
        {
            throw arena_error_f("Failed to create arena named \"%s\"",
                                arena_type.c_str());
        }
        link_items();

        if (!env.rock_colour)
            env.rock_colour = CYAN;
        if (!env.floor_colour)
            env.floor_colour = LIGHTGREY;

#ifdef USE_TILE
        tile_new_level(true);
#endif
        los_changed();
        env.markers.activate_all();
    }

    static string find_monster_spec()
    {
        if (!teams.empty())
            return teams;
        else
            return "random v random";
    }

    /// @throws arena_error if a monster specification is invalid.
    static void parse_faction(faction &fact, string spec)
    {
        fact.clear();
        fact.desc = spec;

        for (const string &monster : split_string(",", spec))
        {
            const string err = fact.members.add_mons(monster, false);
            if (!err.empty())
                throw arena_error(err);
        }
    }

    /// @throws arena_error if the monster specification is invalid.
    static void parse_monster_spec()
    {
        string spec = find_monster_spec();

        allow_chain_summons = !strip_tag(spec, "no_chain_summons");

        allow_summons   = !strip_tag(spec, "no_summons");
        allow_animate   = !strip_tag(spec, "no_animate");
        allow_immobile  = !strip_tag(spec, "no_immobile");
        allow_bands     = !strip_tag(spec, "no_bands");
        allow_zero_xp   =  strip_tag(spec, "allow_zero_xp");
        real_summons    =  strip_tag(spec, "real_summons");
        move_summons    =  strip_tag(spec, "move_summons");
        miscasts        =  strip_tag(spec, "miscasts");
        respawn         =  strip_tag(spec, "respawn");
        move_respawns   =  strip_tag(spec, "move_respawns");
        summon_throttle = strip_number_tag(spec, "summon_throttle:");

        if (real_summons && respawn)
            throw arena_error("Can't set real_summons and respawn at same time.");

        if (summon_throttle <= 0)
            summon_throttle = INT_MAX;

        cycle_random   = strip_tag(spec, "cycle_random");
        name_monsters  = strip_tag(spec, "names");
        random_uniques = strip_tag(spec, "random_uniques");

        const int ntrials = strip_number_tag(spec, "t:");
        if (ntrials != TAG_UNFOUND && ntrials >= 1 && ntrials <= 99
            && !total_trials)
        {
            total_trials = ntrials;
        }

        arena_type = strip_tag_prefix(spec, "arena:");

        if (arena_type.empty())
            arena_type = "default";

        const int arena_delay = strip_number_tag(spec, "delay:");
        if (arena_delay >= 0 && arena_delay < 2000)
            Options.view_delay = arena_delay;

        string arena_place = strip_tag_prefix(spec, "arena_place:");
        if (!arena_place.empty())
        {
            try
            {
                place = level_id::parse_level_id(arena_place);
            }
            catch (const bad_level_id &err)
            {
                throw arena_error_f("Bad place '%s': %s",
                                    arena_place.c_str(),
                                    err.what());
            }
        }

        for (unsigned char gly : strip_tag_prefix(spec, "ban_glyphs:"))
            if (gly < ARRAYSZ(banned_glyphs))
                banned_glyphs[gly] = true;

        vector<string> factions = split_string(" v ", spec);

        if (factions.size() == 1)
            factions = split_string(" vs ", spec);

        if (factions.size() != 2)
        {
            throw arena_error_f("Expected arena monster spec \"xxx v yyy\", "
                                "but got \"%s\"", spec.c_str());
        }

        try
        {
            parse_faction(faction_a, factions[0]);
            parse_faction(faction_b, factions[1]);
        }
        catch (const arena_error &err)
        {
            throw arena_error_f("Bad monster spec \"%s\": %s",
                                spec.c_str(),
                                err.what());
        }

        if (faction_a.desc == faction_b.desc)
        {
            faction_a.desc += " (A)";
            faction_b.desc += " (B)";
        }
    }

    static void setup_monsters()
    {
        faction_a.reset();
        faction_b.reset();

        for (int i = 0; i < MAX_MONSTERS; i++)
            to_respawn[i] = -1;

        unwind_var< FixedBitVector<NUM_MONSTERS> >
            uniq(you.unique_creatures);

        place_a = dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I);
        place_b = dgn_find_feature_marker(DNGN_STONE_STAIRS_DOWN_I);

        // Place the different factions in different orders on
        // alternating rounds so that one side doesn't get the
        // first-move advantage for all rounds.
        if (trials_done & 1)
        {
            faction_a.place_at(place_a);
            faction_b.place_at(place_b);
        }
        else
        {
            faction_b.place_at(place_b);
            faction_a.place_at(place_a);
        }

        adjust_monsters();
    }

    static void show_fight_banner(bool after_fight = false)
    {
        int line = 1;

        cgotoxy(1, line++, GOTO_STAT);
        textcolour(WHITE);
        center_print(crawl_view.hudsz.x, string("Crawl ") + Version::Long);
        line++;

        cgotoxy(1, line++, GOTO_STAT);
        textcolour(YELLOW);
        center_print(crawl_view.hudsz.x, faction_a.desc,
                     total_trials ? team_a_wins : -1);
        cgotoxy(1, line++, GOTO_STAT);
        textcolour(LIGHTGREY);
        center_print(crawl_view.hudsz.x, "vs");
        cgotoxy(1, line++, GOTO_STAT);
        textcolour(YELLOW);
        center_print(crawl_view.hudsz.x, faction_b.desc,
                     total_trials ? trials_done - team_a_wins - ties : -1);

        if (total_trials > 1 && trials_done < total_trials)
        {
            cgotoxy(1, line++, GOTO_STAT);
            textcolour(BROWN);
            center_print(crawl_view.hudsz.x,
                         make_stringf("Round %d of %d",
                                      after_fight ? trials_done
                                                  : trials_done + 1,
                                      total_trials));
        }
        else
        {
            cgotoxy(1, line++, GOTO_STAT);
            textcolour(BROWN);
            clear_to_end_of_line();
        }
    }

    static void setup_others()
    {
        you.species = SP_HUMAN;
        you.char_class = JOB_FIGHTER;
        you.experience_level = 27;

        you.position.y = -1;
        coord_def yplace(dgn_find_feature_marker(DNGN_ESCAPE_HATCH_UP));
        crawl_view.set_player_at(yplace);

        you.mutation[MUT_ACUTE_VISION] = 3;

        you.your_name = "Arena";

        you.hp = you.hp_max = 99;

        for (int i = 0; i < NUM_STATS; ++i)
            you.base_stats[i] = 20;

        show_fight_banner();
    }

    static void expand_mlist(int exp)
    {
        crawl_view.mlistp.y  -= exp;
        crawl_view.mlistsz.y += exp;
    }

    /// @throws arena_error if the specification was invalid.
    static void setup_fight()
    {
        //no_messages mx;
        parse_monster_spec();
        setup_level();

        // Monster setup may block waiting for matchups.
        setup_monsters();

        setup_others();
    }

    static void count_foes()
    {
        int orig_a = faction_a.active_members;
        int orig_b = faction_b.active_members;

        if (orig_a < 0)
            mprf(MSGCH_ERROR, "Book-keeping says faction_a has negative active members.");

        if (orig_b < 0)
            mprf(MSGCH_ERROR, "Book-keeping says faction_b has negative active members.");

        faction_a.active_members = 0;
        faction_b.active_members = 0;

        for (monster_iterator mons; mons; ++mons)
        {
            if (mons_is_tentacle_or_tentacle_segment(mons->type))
                continue;
            if (mons->attitude == ATT_FRIENDLY)
                faction_a.active_members++;
            else if (mons->attitude == ATT_HOSTILE)
                faction_b.active_members++;
        }

        if (orig_a != faction_a.active_members
            || orig_b != faction_b.active_members)
        {
            mprf(MSGCH_ERROR, "Book-keeping error in faction member count: "
                              "%d:%d instead of %d:%d",
                              orig_a, orig_b,
                              faction_a.active_members, faction_b.active_members);

            if (faction_a.active_members > 0
                && faction_b.active_members <= 0)
            {
                faction_a.won = true;
                faction_b.won = false;
            }
            else if (faction_b.active_members > 0
                     && faction_a.active_members <= 0)
            {
                faction_b.won = true;
                faction_a.won = false;
            }
        }
    }

    // Returns true as long as at least one member of each faction is alive.
    static bool fight_is_on()
    {
        if (faction_a.active_members > 0 && faction_b.active_members > 0)
        {
            if (faction_a.won || faction_b.won)
            {
                mprf(MSGCH_ERROR, "Both factions alive but one declared the winner.");
                faction_a.won = false;
                faction_b.won = false;
            }
            return true;
        }

        // Sync up our book-keeping with the actual state, and report
        // any inconsistencies.
        count_foes();

        return faction_a.active_members > 0 && faction_b.active_members > 0;
    }

    static void dump_messages()
    {
        if (!Options.arena_dump_msgs || file == nullptr)
            return;

        vector<string> messages;
        vector<msg_channel_type> channels;
        get_recent_messages(messages, channels);

        for (unsigned int i = 0; i < messages.size(); i++)
        {
            string msg  = messages[i];
            int         chan = channels[i];

            string prefix;
            switch (chan)
            {
                case MSGCH_DIAGNOSTICS:
                    prefix = "DIAG: ";
                    if (Options.arena_dump_msgs_all)
                        break;
                    continue;

                // Ignore messages generated while the user examines
                // the arnea.
                case MSGCH_PROMPT:
                case MSGCH_MONSTER_TARGET:
                case MSGCH_FLOOR_ITEMS:
                case MSGCH_EXAMINE:
                case MSGCH_EXAMINE_FILTER:
                    continue;

                // If a monster-damage message ends with '!' it's a
                // death message, otherwise it's an examination message
                // and should be skipped.
                case MSGCH_MONSTER_DAMAGE:
                    if (msg[msg.length() - 1] != '!')
                        continue;
                    break;

                case MSGCH_ERROR: prefix = "ERROR: "; break;
                case MSGCH_WARN: prefix = "WARN: "; break;
                case MSGCH_SOUND: prefix = "SOUND: "; break;

                case MSGCH_TALK_VISUAL:
                case MSGCH_TALK: prefix = "TALK: "; break;
            }
            msg = prefix + msg;

            fprintf(file, "%s\n", msg.c_str());
        }
    }

    // Try to prevent random luck from letting one spawner fill up the
    // arena with so many monsters that the other spawner can never get
    // back on even footing.
    static void balance_spawners()
    {
        if (a_spawners.empty() || b_spawners.empty())
            return;

        if (faction_a.active_members == 0 || faction_b.active_members == 0)
        {
            mprf(MSGCH_ERROR, "ERROR: Both sides have spawners, but the active "
                 "member count of one side has been reduced to zero!");
            return;
        }

        for (int idx : a_spawners)
        {
            menv[idx].speed_increment *= faction_b.active_members;
            menv[idx].speed_increment /= faction_a.active_members;
        }

        for (int idx : b_spawners)
        {
            menv[idx].speed_increment *= faction_a.active_members;
            menv[idx].speed_increment /= faction_b.active_members;
        }
    }

    static void do_miscasts()
    {
        if (!miscasts)
            return;

        for (monster_iterator mon; mon; ++mon)
        {
            if (mon->type == MONS_TEST_SPAWNER)
                continue;

            MiscastEffect(*mon, *mon, WIZARD_MISCAST, SPTYP_RANDOM,
                          random_range(1, 3), "arena miscast", NH_NEVER);
        }
    }

    static void handle_keypress(int ch)
    {
        if (key_is_escape(ch) || toalower(ch) == 'q')
        {
            contest_cancelled = true;
            mpr("Canceled contest at user request");
            return;
        }

        const command_type cmd = key_to_command(ch, KMC_DEFAULT);

        // We only allow a short list of commands to be used in the arena.
        switch (cmd)
        {
        case CMD_LOOK_AROUND:
        case CMD_SUSPEND_GAME:
        case CMD_REPLAY_MESSAGES:
            break;

        default:
            return;
        }

        if (file != nullptr)
            fflush(file);

        cursor_control coff(true);

        unwind_var<game_type> type(crawl_state.type, GAME_TYPE_NORMAL);
        unwind_bool ar_susp(crawl_state.arena_suspended, true);
        coord_def yplace(dgn_find_feature_marker(DNGN_ESCAPE_HATCH_UP));
        unwind_var<coord_def> pos(you.position);
        you.position = yplace;
        process_command(cmd);
    }

    static void do_respawn(faction &fac)
    {
        is_respawning = true;
        for (unsigned int _i = fac.respawn_list.size(); _i > 0; _i--)
        {
            unsigned int i = _i - 1;

            coord_def pos      = fac.respawn_pos[i];
            int       spec_idx = fac.respawn_list[i];
            mons_spec spec     = fac.members.get_monster(spec_idx);

            if (fac.friendly)
                spec.attitude = ATT_FRIENDLY;

            monster *mon = dgn_place_monster(spec, pos, false, true);

            if (!mon && fac.active_members == 0 && monster_at(pos))
            {
                // We have no members left, so to prevent the round
                // from ending attempt to displace whatever is in
                // our position.
                monster* other = monster_at(pos);

                if (to_respawn[other->mindex()] == -1)
                {
                    // The other monster isn't a respawner itself, so
                    // just get rid of it.
                    mprf(MSGCH_DIAGNOSTICS,
                         "Dismissing non-respawner %s to make room for "
                         "respawner whose side has 0 active members.",
                         other->name(DESC_PLAIN, true).c_str());
                    monster_die(other, KILL_DISMISSED, NON_MONSTER);
                }
                else
                {
                    // Other monster is a respawner, try to move it.
                    mprf(MSGCH_DIAGNOSTICS,
                         "Teleporting respawner %s to make room for "
                         "other respawner whose side has 0 active members.",
                         other->name(DESC_PLAIN, true).c_str());
                    monster_teleport(other, true);
                }

                mon = dgn_place_monster(spec, pos, false, true);
            }

            if (mon)
            {
                // We succeeded, so remove from list.
                fac.respawn_list.erase(fac.respawn_list.begin() + i);
                fac.respawn_pos.erase(fac.respawn_pos.begin() + i);

                to_respawn[mon->mindex()] = spec_idx;

                if (move_respawns)
                    monster_teleport(mon, true, true);
            }
            else
            {
                // Couldn't respawn, so leave it on the list; hopefully
                // space will open up later.
            }
        }
        is_respawning = false;
    }

    static void do_fight()
    {
        viewwindow();
        clear_messages(true);
        {
            cursor_control coff(false);
            while (fight_is_on())
            {
                if (kbhit())
                {
                    const int ch = getchm();
                    handle_keypress(ch);
                    ASSERT(crawl_state.game_is_arena());
                    ASSERT(!crawl_state.arena_suspended);
                    if (contest_cancelled)
                        return;
                }

#ifdef ARENA_VERBOSE
                mprf("---- Turn #%d ----", turns);
#endif

                // Check the consistency of our book-keeping every 100 turns.
                if ((turns++ % 100) == 0)
                    count_foes();

                viewwindow();
                you.time_taken = 10;
                // Make sure we don't starve.
                you.hunger = HUNGER_MAXIMUM;
                //report_foes();
                world_reacts();
                do_miscasts();
                do_respawn(faction_a);
                do_respawn(faction_b);
                balance_spawners();
                delay(Options.view_delay);
                clear_messages();
                dump_messages();
                ASSERT(you.pet_target == MHITNOT);
            }
            viewwindow();
        }

        clear_messages();

        trials_done++;

        // We bother with all this to properly deal with ties, and with
        // ball lightning or ballistomycete spores winning the fight via suicide.
        // The sanity checking is probably just paranoia.
        bool was_tied = false;
        if (!faction_a.won && !faction_b.won)
        {
            if (faction_a.active_members > 0)
            {
                mprf(MSGCH_ERROR, "Tie declared, but faction_a won.");
                team_a_wins++;
                faction_a.won = true;
            }
            else if (faction_b.active_members > 0)
            {
                mprf(MSGCH_ERROR, "Tie declared, but faction_b won.");
                faction_b.won = true;
            }
            else
            {
                ties++;
                was_tied = true;
            }
        }
        else if (faction_a.won && faction_b.won)
        {
            faction_a.won = false;
            faction_b.won = false;

            mprf(MSGCH_ERROR, "*BOTH* factions won?!");
            if (faction_a.active_members > 0)
            {
                mprf(MSGCH_ERROR, "Faction_a real winner.");
                team_a_wins++;
                faction_a.won = true;
            }
            else if (faction_b.active_members > 0)
            {
                mprf(MSGCH_ERROR, "Faction_b real winner.");
                faction_b.won = true;
            }
            else
            {
                mprf(MSGCH_ERROR, "Both sides dead.");
                ties++;
                was_tied = true;
            }
        }
        else if (faction_a.won)
            team_a_wins++;

        show_fight_banner(true);

        string msg;
        if (was_tied)
            msg = "Tie";
        else
            msg = "Winner: %s!";

        if (Options.arena_dump_msgs || Options.arena_list_eq)
            msg = "---------- " + msg + " ----------";

        if (was_tied)
            mpr(msg);
        else
            mprf(msg.c_str(),
                 faction_a.won ? faction_a.desc.c_str()
                               : faction_b.desc.c_str());
        dump_messages();
    }

    static void global_setup(const string& arena_teams)
    {
        // Clear some things that shouldn't persist across restart_after_game.
        // parse_monster_spec and setup_fight will clear the rest.
        total_trials = trials_done = team_a_wins = ties = 0;
        contest_cancelled = false;
        is_respawning = false;
        uniques_list.clear();
        memset(banned_glyphs, 0, sizeof(banned_glyphs));
        arena_type = "";
        place = level_id(BRANCH_DEPTHS, 1);

        // [ds] Turning off view_lock crashes arena.
        Options.view_lock_x = Options.view_lock_y = true;

        teams = arena_teams;
        // Set various options from the arena spec's tags
        try
        {
            parse_monster_spec();
        }
        catch (const arena_error &error)
        {
            write_error(error.what());
            game_ended_with_error(error.what());
        }

        if (file != nullptr)
            end(0, false, "Results file already open");
        file = fopen("arena.result", "w");

        if (file != nullptr)
        {
            string spec = find_monster_spec();
            fprintf(file, "%s\n", spec.c_str());

            if (Options.arena_dump_msgs || Options.arena_list_eq)
                fprintf(file, "========================================\n");
        }

        expand_mlist(5);

        for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
        {
            if (i == MONS_PLAYER_GHOST)
                continue;

            if (mons_is_unique(i) && !arena_veto_random_monster(i))
                uniques_list.push_back(i);
        }
    }

    static void global_shutdown()
    {
        if (file != nullptr)
            fclose(file);

        file = nullptr;
    }

    static void write_results()
    {
        if (file != nullptr)
        {
            if (Options.arena_dump_msgs || Options.arena_list_eq)
                fprintf(file, "========================================\n");
            fprintf(file, "%d-%d", team_a_wins,
                    trials_done - team_a_wins - ties);
            if (ties > 0)
                fprintf(file, "-%d", ties);
            fprintf(file, "\n");
        }
    }

    static void write_error(const string &error)
    {
        if (file != nullptr)
        {
            fprintf(file, "err: %s\n", error.c_str());
            fclose(file);
        }
        file = nullptr;
    }

    static void simulate()
    {
        init_level_connectivity();
        do
        {
            try
            {
                setup_fight();
            }
            catch (const arena_error &error)
            {
                write_error(error.what());
                game_ended_with_error(error.what());
            }
            do_fight();

            if (trials_done < total_trials)
                delay(Options.view_delay * 5);
        }
        while (!contest_cancelled && trials_done < total_trials);

        if (total_trials > 0)
        {
            mprf("Final score: %s (%d); %s (%d) [%d ties]",
                 faction_a.desc.c_str(), team_a_wins,
                 faction_b.desc.c_str(), trials_done - team_a_wins - ties,
                 ties);
        }
        delay(Options.view_delay * 5);

        write_results();
    }
}

/////////////////////////////////////////////////////////////////////////////

// Various arena callbacks

monster_type arena_pick_random_monster(const level_id &place)
{
    if (arena::random_uniques)
    {
        const vector<monster_type> &uniques = arena::uniques_list;

        const monster_type type = uniques[random2(uniques.size())];
        you.unique_creatures.set(type, false);

        return type;
    }

    if (!arena::cycle_random)
        return RANDOM_MONSTER;

    for (int tries = 0; tries <= NUM_MONSTERS; tries++)
    {
        monster_type mons = pick_monster_by_hash(place.branch,
            ++arena::cycle_random_pos);

        if (arena_veto_random_monster(mons))
            continue;

        return mons;
    }

    game_ended_with_error(
        make_stringf("No random monsters for place '%s'",
                     arena::place.describe().c_str()));
}

bool arena_veto_random_monster(monster_type type)
{
    if (mons_is_tentacle_or_tentacle_segment(type))
        return true;
    if (!arena::allow_immobile && mons_class_is_stationary(type))
        return true;
    if (!arena::allow_zero_xp && !mons_class_gives_xp(type))
        return true;
    if (!(mons_char(type) & !127) && arena::banned_glyphs[mons_char(type)])
        return true;

    return false;
}

bool arena_veto_place_monster(const mgen_data &mg, bool first_band_member,
                              const coord_def& pos)
{
    // If the first band member makes it past the summon throttle cut,
    // let all of the rest of its band in too regardless of the summon
    // throttle.
    if (mg.abjuration_duration > 0 && first_band_member)
    {
        if (mg.behaviour == BEH_FRIENDLY
            && arena::faction_a.active_members > arena::summon_throttle)
        {
            return true;
        }
        else if (mg.behaviour == BEH_HOSTILE
                 && arena::faction_b.active_members > arena::summon_throttle)
        {
            return true;
        }

    }
    return !arena::allow_bands && !first_band_member
           || !(mons_char(mg.cls) & !127)
              && arena::banned_glyphs[mons_char(mg.cls)];
}

// XXX: Still having some trouble with book-keeping if a slime creature
// is placed via splitting.
void arena_placed_monster(monster* mons)
{
    if (mons_is_tentacle_or_tentacle_segment(mons->type))
        ; // we don't count tentacles or tentacle segments, even free-standing
    else if (mons->attitude == ATT_FRIENDLY)
    {
        arena::faction_a.active_members++;
        arena::faction_b.won = false;
    }
    else if (mons->attitude == ATT_HOSTILE)
    {
        arena::faction_b.active_members++;
        arena::faction_a.won = false;
    }

    if (!arena::allow_summons || !arena::allow_animate)
    {
        arena::adjust_spells(mons, !arena::allow_summons,
                             !arena::allow_animate);
    }

    if (mons->type == MONS_TEST_SPAWNER)
    {
        if (mons->attitude == ATT_FRIENDLY)
            arena::a_spawners.push_back(mons->mindex());
        else if (mons->attitude == ATT_HOSTILE)
            arena::b_spawners.push_back(mons->mindex());
    }

    const bool summoned = mons->is_summoned();

#ifdef ARENA_VERBOSE
    mprf("%s %s!",
         mons->full_name(DESC_A).c_str(),
         arena::is_respawning                ? "respawns" :
         (summoned && ! arena::real_summons) ? "is summoned"
                                             : "enters the arena");
#endif

    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        ii->flags |= ISFLAG_IDENT_MASK;

        // Set the "drop" time here in case the monster drops the
        // item without dying, like being polymorphed.
        arena::item_drop_times[ii->index()] = arena::turns;
    }

    if (arena::name_monsters && !mons->is_named())
        mons->mname = make_name();

    if (summoned)
    {
        // Real summons drop corpses and items.
        if (arena::real_summons)
        {
            mons->del_ench(ENCH_ABJ, true, false);
            for (mon_inv_iterator ii(*mons); ii; ++ii)
                ii->flags &= ~ISFLAG_SUMMONED;
        }

        if (arena::move_summons)
            monster_teleport(mons, true, true);

        if (!arena::allow_chain_summons)
            arena::adjust_spells(mons, true, false);
    }
}

// Take care of respawning slime creatures merging and then splitting.
void arena_split_monster(monster* split_from, monster* split_to)
{
    if (!arena::respawn)
        return;

    const int from_idx   = split_from->mindex();
    const int member_idx = arena::to_respawn[from_idx];

    if (member_idx == -1)
        return;

    arena::to_respawn[split_to->mindex()] = member_idx;
}

void arena_monster_died(monster* mons, killer_type killer,
                        int killer_index, bool silent, const item_def* corpse)
{
    if (mons_is_tentacle_or_tentacle_segment(mons->type))
        ; // part of a monster, or a spell
    else if (mons->attitude == ATT_FRIENDLY)
        arena::faction_a.active_members--;
    else if (mons->attitude == ATT_HOSTILE)
        arena::faction_b.active_members--;

    if (arena::faction_a.active_members > 0
        && arena::faction_b.active_members <= 0)
    {
        arena::faction_a.won = true;
    }
    else if (arena::faction_b.active_members > 0
             && arena::faction_a.active_members <= 0)
    {
        arena::faction_b.won = true;
    }
    // Everyone is dead. Is it a tie, or something else?
    else if (arena::faction_a.active_members <= 0
             && arena::faction_b.active_members <= 0)
    {
        if (mons->flags & MF_HARD_RESET && !MON_KILL(killer))
            mpr("Last arena monster was dismissed.");
        // If all monsters are dead, and the last one to die is a giant
        // spore or ball lightning, then that monster's faction is the
        // winner, since self-destruction is their purpose. But if a
        // trap causes the spore to explode, and that kills everything,
        // it's a tie, since it counts as the trap killing everyone.
        else if (mons_self_destructs(*mons) && MON_KILL(killer))
        {
            if (mons->attitude == ATT_FRIENDLY)
                arena::faction_a.won = true;
            else if (mons->attitude == ATT_HOSTILE)
                arena::faction_b.won = true;
        }
    }

    // Only respawn those monsters which were initially placed in the
    // arena.
    const int midx = mons->mindex();
    if (arena::respawn && arena::to_respawn[midx] != -1
        // Don't respawn when a slime 'dies' from merging with another
        // slime.
        && !(mons->type == MONS_SLIME_CREATURE && silent
             && killer == KILL_MISC
             && killer_index == NON_MONSTER))
    {
        arena::faction *fac = nullptr;
        if (mons->attitude == ATT_FRIENDLY)
            fac = &arena::faction_a;
        else if (mons->attitude == ATT_HOSTILE)
            fac = &arena::faction_b;

        if (fac)
        {
            int member_idx = arena::to_respawn[midx];
            fac->respawn_list.push_back(member_idx);
            fac->respawn_pos.push_back(mons->pos());

            // Un-merge slime when it respawns, but only if it's
            // specifically a slime, and not a random monster which
            // happens to be a slime.
            if (mons->type == MONS_SLIME_CREATURE
                && (fac->members.get_monster(member_idx).type
                    == MONS_SLIME_CREATURE))
            {
                for (int i = 1; i < mons->blob_size; i++)
                {
                    fac->respawn_list.push_back(member_idx);
                    fac->respawn_pos.push_back(mons->pos());
                }
            }

            arena::to_respawn[midx] = -1;
        }
    }

    if (corpse)
        arena::item_drop_times[corpse->index()] = arena::turns;

    // Won't be dropping any items.
    if (mons->flags & MF_HARD_RESET)
        return;

    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        if (ii->flags & ISFLAG_SUMMONED)
            continue;

        arena::item_drop_times[ii->index()] = arena::turns;
    }
}

static bool _sort_by_age(int a, int b)
{
    return arena::item_drop_times[a] < arena::item_drop_times[b];
}

#define DESTROY_ITEM(i) \
{ \
    destroy_item(i, true); \
    arena::item_drop_times[i] = 0; \
    cull_count++; \
    if (first_avail == NON_ITEM) \
        first_avail = i; \
}

// Culls the items which have been on the floor the longest, culling the
// newest items last. Items which a monster dropped voluntarily or
// because of being polymorphed, rather than because of dying, are
// culled earlier than they should be, but it's not like we have to be
// fair to the arena monsters.
int arena_cull_items()
{
    vector<int> items;

    int first_avail = NON_ITEM;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        // All items in mitm[] are valid when we're called.
        const item_def &item(mitm[i]);

        // We want floor items.
        if (!in_bounds(item.pos))
            continue;

        items.push_back(i);
    }

    // Cull half of items on the floor.
    const int cull_target = items.size() / 2;
          int cull_count  = 0;

    sort(items.begin(), items.end(), _sort_by_age);

    vector<int> ammo;

    for (int idx : items)
    {
        const item_def &item(mitm[idx]);

        // If the drop time is 0 then this is probably thrown ammo.
        if (arena::item_drop_times[idx] == 0)
        {
            // We know it's at least this old.
            arena::item_drop_times[idx] = arena::turns;

            // Arrows/needles/etc on the floor is just clutter.
            if (item.base_type != OBJ_MISSILES
               || item.sub_type == MI_JAVELIN
               || item.sub_type == MI_TOMAHAWK
               || item.sub_type == MI_THROWING_NET)
            {
                ammo.push_back(idx);
                continue;
            }
        }
        DESTROY_ITEM(idx);
        if (cull_count >= cull_target)
            break;
    }

    if (cull_count >= cull_target)
    {
        dprf("On turn #%d culled %d items dropped by monsters, done.",
             arena::turns, cull_count);
        return first_avail;
    }

    dprf("On turn #%d culled %d items dropped by monsters, culling some more.",
         arena::turns, cull_count);

#ifdef DEBUG_DIAGNOSTICS
    const int count1 = cull_count;
#endif
    for (int idx : ammo)
    {
        DESTROY_ITEM(idx);
        if (cull_count >= cull_target)
            break;
    }

    if (cull_count >= cull_target)
    {
        dprf("Culled %d (probably) ammo items, done.",
             cull_count - count1);
        return first_avail;
    }

    dprf("Culled %d items total, short of target %d.",
         cull_count, cull_target);
    return first_avail;
} // arena_cull_items

/////////////////////////////////////////////////////////////////////////////

static void _init_arena()
{
    initialise_branch_depths();
    run_map_global_preludes();
    run_map_local_preludes();
    initialise_item_descriptions();
}

NORETURN void run_arena(const string& teams)
{
    _init_arena();

    ASSERT(!crawl_state.arena_suspended);

#ifdef WIZARD
    // The player has wizard powers for the duration of the arena.
    unwind_bool wiz(you.wizard, true);
#endif

    arena::global_setup(teams);
    arena::simulate();
    arena::global_shutdown();
    game_ended();
}

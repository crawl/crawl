/*
 *  File:       arena.cc
 *  Summary:    Functions related to the monster arena (stage and watch fights).
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "externs.h"
#include "arena.h"
#include "chardump.h"
#include "cio.h"
#include "command.h"
#include "dungeon.h"
#include "initfile.h"
#include "items.h"
#include "itemname.h" // for make_name()
#include "libutil.h"
#include "macro.h"
#include "maps.h"
#include "message.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "monstuff.h"
#include "monplace.h"
#include "mstuff2.h"
#include "output.h"
#include "randart.h"
#include "skills2.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "version.h"
#include "view.h"

#define DEBUG_DIAGNOSTICS 1

extern void world_reacts();

namespace arena
{
    void write_error(const std::string &error);

    // A faction is just a big list of monsters. Monsters will be dropped
    // around the appropriate marker.
    struct faction
    {
        std::string desc;
        mons_list   members;
        bool        friendly;
        int         active_members;
        bool        won;

        faction(bool fr) : members(), friendly(fr), active_members(0),
                           won(false) { }

        void place_at(const coord_def &pos);

        void reset()
        {
            active_members = 0;
            won            = false;
        }

        void clear()
        {
            reset();
            members.clear();
        }
    };

    int total_trials = 0;

    bool contest_canceled = false;

    int trials_done = 0;
    int team_a_wins = 0;
    int ties        = 0;

    int turns       = 0;

    bool allow_summons       = true;
    bool allow_animate       = true;
    bool allow_chain_summons = true;
    bool allow_zero_xp       = false;
    bool allow_immobile      = true;
    bool allow_bands         = true;
    bool name_monsters       = false;
    bool random_uniques      = false;
    bool real_summons        = false;
    bool move_summons        = false;

    bool miscasts            = false;

    int  summon_throttle     = INT_MAX;

    std::vector<int> uniques_list;
    std::vector<int> a_spawners;
    std::vector<int> b_spawners;

    int item_drop_times[MAX_ITEMS];

    bool banned_glyphs[256];

    std::string arena_type = "";
    faction faction_a(true);
    faction faction_b(false);
    coord_def place_a, place_b;

    bool cycle_random     = false;
    int  cycle_random_pos = -1;

    FILE *file = NULL;
    int message_pos = 0;
    level_id place(BRANCH_MAIN_DUNGEON, 20);

    void adjust_spells(monsters* mons, bool no_summons, bool no_animate)
    {
        monster_spells &spells(mons->spells);
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            spell_type sp = spells[i];
            if (no_summons && spell_typematch(sp, SPTYP_SUMMONING))
                spells[i] = SPELL_NO_SPELL;
            else if (no_animate && sp == SPELL_ANIMATE_DEAD)
                spells[i] = SPELL_NO_SPELL;
        }
    }

    void adjust_monsters()
    {
        if (!allow_summons || !allow_animate)
        {
            for (int m = 0; m < MAX_MONSTERS; ++m)
            {
                monsters *mons(&menv[m]);
                if (!mons->alive())
                    continue;
                adjust_spells(mons, !allow_summons, !allow_animate);
            }
        }

        for (int i = 0; i < MAX_MONSTERS; i++)
        {
            monsters *mon = &menv[i];
            if (!mon->alive())
                continue;

            const bool friendly = mons_friendly_real(mon);
            // Set target to the opposite faction's home base.
            mon->target = friendly ? place_b : place_a;
        }
    }

    void list_eq(int imon)
    {
        if (!Options.arena_list_eq || file == NULL)
            return;

        const monsters* mon = &menv[imon];

        std::vector<int> items;

        for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
            if (mon->inv[i] != NON_ITEM)
                items.push_back(mon->inv[i]);

        if (items.size() == 0)
            return;

        fprintf(file, "%s:\n", mon->name(DESC_PLAIN, true).c_str());

        for (unsigned int i = 0; i < items.size(); i++)
        {
            item_def &item = mitm[items[i]];
            fprintf(file, "        %s\n",
                    item.name(DESC_PLAIN, false, true).c_str());
        }
    }

    void faction::place_at(const coord_def &pos)
    {
        ASSERT(in_bounds(pos));
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

                const int imon = dgn_place_monster(spec, you.your_level,
                                                   loc, false, true, false);
                if (imon == -1)
                    end(1, false, "Failed to create monster at (%d,%d) grd: %s",
                        loc.x, loc.y, dungeon_feature_name(grd(loc)));
                list_eq(imon);
            }
        }
    }

    void center_print(unsigned sz, std::string text, int number = -1)
    {
        if (number >= 0)
            text = make_stringf("(%d) %s", number, text.c_str());

        if (text.length() > sz)
            text = text.substr(0, sz);

        int padding = (sz - text.length()) / 2 + text.length();
        cprintf("%*s", padding, text.c_str());
    }

    void setup_level()
    {
        turns = 0;

        a_spawners.clear();
        b_spawners.clear();
        memset(item_drop_times, 0, sizeof(item_drop_times));

        if (place.is_valid())
        {
            you.level_type    = place.level_type;
            you.where_are_you = place.branch;
            you.your_level    = place.absdepth();
        }

        dgn_reset_level();

        for (int x = 0; x < GXM; ++x)
            for (int y = 0; y < GYM; ++y)
                grd[x][y] = DNGN_ROCK_WALL;

        unwind_bool gen(Generating_Level, true);

        typedef unwind_var< std::set<std::string> > unwind_stringset;

        const unwind_stringset mtags(you.uniq_map_tags);
        const unwind_stringset mnames(you.uniq_map_names);

        std::string map_name = "arena_" + arena_type;
        const map_def *map = random_map_for_tag(map_name.c_str());

        if (!map)
            throw make_stringf("No arena maps named \"%s\"", arena_type.c_str());

#ifdef USE_TILE
        tile_init_default_flavour();
        tile_clear_flavour();
#endif

        ASSERT(map);
        bool success = dgn_place_map(map, true, true);
        if (!success)
            throw make_stringf("Failed to create arena named \"%s\"",
                               arena_type.c_str());
        link_items();

        if (!env.rock_colour)
            env.rock_colour = CYAN;
        if (!env.floor_colour)
            env.floor_colour = LIGHTGREY;

#ifdef USE_TILE
        TileNewLevel(true);
#endif
    }

    std::string find_monster_spec()
    {
        if (!SysEnv.arena_teams.empty())
            return (SysEnv.arena_teams);
        throw std::string("No monsters specified for the arena.");
    }

    void parse_faction(faction &fact, std::string spec)
        throw (std::string)
    {
        fact.clear();
        fact.desc = spec;

        std::vector<std::string> monsters = split_string(",", spec);
        for (int i = 0, size = monsters.size(); i < size; ++i)
        {
            const std::string err = fact.members.add_mons(monsters[i], false);
            if (!err.empty())
                throw err;
        }
    }

    void parse_monster_spec()
        throw (std::string)
    {
        std::string spec = find_monster_spec();

        Options.arena_force_ai =
            strip_bool_tag(spec, "force_ai", Options.arena_force_ai);

        allow_chain_summons = !strip_tag(spec, "no_chain_summons");

        allow_summons   = !strip_tag(spec, "no_summons");
        allow_animate   = !strip_tag(spec, "no_animate");
        allow_immobile  = !strip_tag(spec, "no_immobile");
        allow_bands     = !strip_tag(spec, "no_bands");
        allow_zero_xp   =  strip_tag(spec, "allow_zero_xp");
        real_summons    =  strip_tag(spec, "real_summons");
        move_summons    =  strip_tag(spec, "move_summons");
        miscasts        =  strip_tag(spec, "miscasts");
        summon_throttle = strip_number_tag(spec, "summon_throttle:");

        if (summon_throttle <= 0)
            summon_throttle = INT_MAX;

        cycle_random   = strip_tag(spec, "cycle_random");
        name_monsters  = strip_tag(spec, "names");
        random_uniques = strip_tag(spec, "random_uniques");

        const int ntrials = strip_number_tag(spec, "t:");
        if (ntrials != TAG_UNFOUND && ntrials >= 1 && ntrials <= 99
            && !total_trials)
            total_trials = ntrials;

        arena_type = strip_tag_prefix(spec, "arena:");

        if (arena_type.empty())
            arena_type = "default";

        const int arena_delay = strip_number_tag(spec, "delay:");
        if (arena_delay >= 0 && arena_delay < 2000)
            Options.arena_delay = arena_delay;

        std::string arena_place = strip_tag_prefix(spec, "arena_place:");
        if (!arena_place.empty())
        {
            try {
                place = level_id::parse_level_id(arena_place);
            }
            catch (const std::string &err)
            {
                throw make_stringf("Bad place '%s': %s",
                                   arena_place.c_str(),
                                   err.c_str());
            }
            if (place.level_type == LEVEL_LABYRINTH)
                throw (std::string("Can't set arena place to the "
                                   "labyrinth."));
            else if (place.level_type == LEVEL_PORTAL_VAULT)
                throw (std::string("Can't set arena place to a portal "
                                   "vault."));
        }

        std::string glyphs = strip_tag_prefix(spec, "ban_glyphs:");
        for (unsigned int i = 0; i < glyphs.size(); i++)
            banned_glyphs[(int)glyphs[i]] = true;

        std::vector<std::string> factions = split_string(" v ", spec);

        if (factions.size() == 1)
            factions = split_string(" vs ", spec);

        if (factions.size() != 2)
            throw make_stringf("Expected arena monster spec \"xxx v yyy\", "
                               "but got \"%s\"", spec.c_str());

        try
        {
            parse_faction(faction_a, factions[0]);
            parse_faction(faction_b, factions[1]);
        }
        catch (const std::string &err)
        {
            throw make_stringf("Bad monster spec \"%s\": %s",
                               spec.c_str(),
                               err.c_str());
        }

        if (faction_a.desc == faction_b.desc)
        {
            faction_a.desc += " (A)";
            faction_b.desc += " (B)";
        }
    }

    void setup_monsters()
        throw (std::string)
    {
        faction_a.reset();
        faction_b.reset();

        unwind_var< FixedVector<bool, NUM_MONSTERS> >
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

    void show_fight_banner(bool after_fight = false)
    {
        int line = 1;

        cgotoxy(1, line++, GOTO_STAT);
        textcolor(WHITE);
        center_print(crawl_view.hudsz.x,
                     "Crawl " VER_NUM VER_QUAL " " VERSION_DETAIL);
        line++;

        cgotoxy(1, line++, GOTO_STAT);
        textcolor(YELLOW);
        center_print(crawl_view.hudsz.x, faction_a.desc,
                     total_trials ? team_a_wins : -1);
        cgotoxy(1, line++, GOTO_STAT);
        textcolor(LIGHTGREY);
        center_print(crawl_view.hudsz.x, "vs");
        cgotoxy(1, line++, GOTO_STAT);
        textcolor(YELLOW);
        center_print(crawl_view.hudsz.x, faction_b.desc,
                     total_trials ? trials_done - team_a_wins - ties : -1);

        if (total_trials > 1 && trials_done < total_trials)
        {
            cgotoxy(1, line++, GOTO_STAT);
            textcolor(BROWN);
            center_print(crawl_view.hudsz.x,
                         make_stringf("Round %d of %d",
                                      after_fight ? trials_done
                                                  : trials_done + 1,
                                      total_trials));
        }
        else
        {
            cgotoxy(1, line++, GOTO_STAT);
            textcolor(BROWN);
            clear_to_end_of_line();
        }
    }

    void setup_others()
    {
        you.species = SP_HUMAN;
        you.char_class = JOB_FIGHTER;
        you.experience_level = 27;

        you.mutation[MUT_ACUTE_VISION] = 3;

        coord_def yplace(dgn_find_feature_marker(DNGN_ESCAPE_HATCH_UP));
        // Fix up the viewport.  Temporarily unset arena mode to avoid
        // assertion.
        crawl_state.arena = false;
        you.moveto(yplace);
        crawl_state.arena = true;

        strcpy(you.your_name, "Arena");

        you.hp = you.hp_max = 99;

        Options.show_gold_turns = false;

        show_fight_banner();
    }

    void expand_mlist(int exp)
    {
        crawl_view.mlistp.y -= exp;
        crawl_view.mlistsz.y += exp;
    }

    void setup_fight()
        throw (std::string)
    {
        //no_messages mx;
        parse_monster_spec();
        setup_level();

        // Monster set up may block waiting for matchups.
        setup_monsters();

        setup_others();
    }

    // Temporarily unset craw_state.arena to force a --more-- to happen.
    void more()
    {
        unwind_bool state(crawl_state.arena, false);

        ::more();
    }

    void count_foes()
    {
        int orig_a = faction_a.active_members;
        int orig_b = faction_b.active_members;

        if (orig_a < 0)
        {
            mpr("Book-keeping says faction_a has negative active members.",
                MSGCH_ERROR);
            more();
        }

        if (orig_b < 0)
        {
            mpr("Book-keeping says faction_b has negative active members.",
                MSGCH_ERROR);
            more();
        }

        faction_a.active_members = 0;
        faction_b.active_members = 0;

        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            const monsters *mons(&menv[i]);
            if (mons->alive())
            {
                if (mons->attitude == ATT_FRIENDLY)
                    faction_a.active_members++;
                else if (mons->attitude == ATT_HOSTILE)
                    faction_b.active_members++;
            }
        }

        if (orig_a != faction_a.active_members
            || orig_b != faction_b.active_members)
        {
            mpr("Book-keeping error in faction member count.", MSGCH_ERROR);
            more();

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
    bool fight_is_on()
    {
        if (faction_a.active_members > 0 && faction_b.active_members > 0)
        {
            if (faction_a.won || faction_b.won)
            {
                mpr("Both factions alive but one declared the winner.",
                    MSGCH_ERROR);
                more();
                faction_a.won = false;
                faction_b.won = false;
            }
            return (true);
        }

        // Sync up our book-keeping with the actual state, and report
        // any inconsistencies.
        count_foes();

        return (faction_a.active_members > 0 && faction_b.active_members > 0);
    }

    void report_foes()
    {
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mons(&menv[i]);
            if (mons->alive())
            {
                if (mons->type == MONS_SIGMUND)
                {
                    coord_def where;
                    if (mons->get_foe())
                        where = mons->get_foe()->pos();
                    mprf("%s (%d,%d) foe: %s (%d,%d)",
                         mons->name(DESC_PLAIN).c_str(),
                         mons->pos().x, mons->pos().y,
                         mons->get_foe()? mons->get_foe()->name(DESC_PLAIN).c_str()
                         : "(none)",
                         where.x, where.y);
                }
            }
        }
    }

    void fixup_foes()
    {
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mons(&menv[i]);
            if (mons->alive())
                behaviour_event(mons, ME_DISTURB, MHITNOT, mons->pos());
        }
    }

    void dump_messages()
    {
        if (!Options.arena_dump_msgs || file == NULL)
            return;

        std::vector<int> channels;
        std::vector<std::string> messages =
            get_recent_messages(message_pos,
                                !Options.arena_dump_msgs_all,
                                &channels);

        for (unsigned int i = 0; i < messages.size(); i++)
        {
            std::string msg  = messages[i];
            int         chan = channels[i];

            std::string prefix;
            switch(chan)
            {
                case MSGCH_ERROR: prefix = "ERROR: "; break;
                case MSGCH_WARN: prefix = "WARN: "; break;
                case MSGCH_DIAGNOSTICS: prefix = "DIAG: "; break;
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
    void balance_spawners()
    {
        if (a_spawners.size() == 0 || b_spawners.size() == 0)
            return;

        for (unsigned int i = 0; i < a_spawners.size(); i++)
        {
            int idx = a_spawners[i];
            menv[idx].speed_increment *= faction_b.active_members;
            menv[idx].speed_increment /= faction_a.active_members;
        }

        for (unsigned int i = 0; i < b_spawners.size(); i++)
        {
            int idx = b_spawners[i];
            menv[idx].speed_increment *= faction_a.active_members;
            menv[idx].speed_increment /= faction_b.active_members;
        }
    }

    void do_miscasts()
    {
        if (!miscasts)
            return;

        for (int i = 0; i < MAX_MONSTERS; i++)
        {
            monsters* mon = &menv[i];

            if (!mon->alive() || mon->type == MONS_TEST_SPAWNER)
                continue;

            MiscastEffect(mon, i, SPTYP_RANDOM, random_range(1, 3),
                          "arena miscast", NH_NEVER);
        }
    }

    void handle_keypress(int ch)
    {
        if (ch == ESCAPE || tolower(ch) == 'q' || ch == CONTROL('G'))
        {
            contest_canceled = true;
            mpr("Canceled contest at user request");
            return;
        }

        const command_type cmd = key_to_command(ch, KC_DEFAULT);

        // We only allow a short list of commands to be used in the arena.
        switch(cmd)
        {
        case CMD_LOOK_AROUND:
        case CMD_SUSPEND_GAME:
        case CMD_REPLAY_MESSAGES:
            break;

        default:
            return;
        }

        if (file != NULL)
            fflush(file);

        cursor_control coff(true);

        unwind_bool  ar     (crawl_state.arena,           false);
        unwind_bool  ar_susp(crawl_state.arena_suspended, true);

        unwind_var<coord_def> pos(you.position);
        coord_def yplace(dgn_find_feature_marker(DNGN_ESCAPE_HATCH_UP));
        you.moveto(yplace);

        process_command(cmd);
    }

    void do_fight()
    {
        mesclr(true);
        {
            cursor_control coff(false);
            while (fight_is_on())
            {
                if (kbhit())
                {
                    const int ch = getch();
                    handle_keypress(ch);
                    ASSERT(crawl_state.arena && !crawl_state.arena_suspended);
                    if (contest_canceled)
                        return;
                }

#ifdef DEBUG_DIAGNOSTICS
                mprf("---- Turn #%d ----", turns);
#endif

                // Check the consistency of our book-keeping every 100 turns.
                if ((turns++ % 100) == 0)
                    count_foes();

                viewwindow(true, false);
                unwind_var<coord_def> pos(you.position);
                // Move hero offscreen.
                you.position.y = -1;
                you.time_taken = 10;
                // Make sure we don't starve.
                you.hunger = 10999;
                //report_foes();
                world_reacts();
                do_miscasts();
                balance_spawners();
                delay(Options.arena_delay);
                mesclr();
                dump_messages();
                ASSERT(you.pet_target == MHITNOT);
            }
            viewwindow(true, false);
        }

        mesclr();

        trials_done++;

        // We bother with all this to properly deal with ties, and with
        // ball lightning or giant spores winning the fight via suicide.
        // The sanity checking is probably just paranoia.
        bool was_tied = false;
        if (!faction_a.won && !faction_b.won)
        {
            if (faction_a.active_members > 0)
            {
                mpr("Tie declared, but faction_a won.", MSGCH_ERROR);
                more();
                team_a_wins++;
                faction_a.won = true;
            }
            else if (faction_b.active_members > 0)
            {
                mpr("Tie declared, but faction_b won.", MSGCH_ERROR);
                more();
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

            mpr("*BOTH* factions won?!", MSGCH_ERROR);
            if (faction_a.active_members > 0)
            {
                mpr("Faction_a real winner.", MSGCH_ERROR);
                team_a_wins++;
                faction_a.won = true;
            }
            else if (faction_b.active_members > 0)
            {
                mpr("Faction_b real winner.", MSGCH_ERROR);
                faction_b.won = true;
            }
            else
            {
                mpr("Both sides dead.", MSGCH_ERROR);
                ties++;
                was_tied = true;
            }
            more();
        }
        else if (faction_a.won)
            team_a_wins++;

        show_fight_banner(true);

        std::string msg;
        if (was_tied)
            msg = "Tie";
        else
            msg = "Winner: %s!";

        if (Options.arena_dump_msgs || Options.arena_list_eq)
            msg = "---------- " + msg + " ----------";

        if (was_tied)
            mprf(msg.c_str());
        else
            mprf(msg.c_str(),
                 faction_a.won ? faction_a.desc.c_str()
                               : faction_b.desc.c_str());
        dump_messages();
    }

    void global_setup()
    {
        // Set various options from the arena spec's tags
        try
        {
            parse_monster_spec();
        }
        catch (const std::string &error)
        {
            write_error(error);
            end(0, false, "%s", error.c_str());
        }

        if (file != NULL)
            end(0, false, "Results file already open");
        file = fopen("arena.result", "w");

        if (file != NULL)
        {
            std::string spec = find_monster_spec();
            fprintf(file, "%s\n", spec.c_str());

            if (Options.arena_dump_msgs || Options.arena_list_eq)
                fprintf(file, "========================================\n");
        }

        expand_mlist(5);

        for (int i = 0; i < NUM_MONSTERS; i++)
        {
            if (i == MONS_PLAYER_GHOST)
                continue;

            if (mons_is_unique(i)
                && !arena_veto_random_monster( (monster_type) i))
            {
                uniques_list.push_back(i);
            }
        }
    }

    void global_shutdown()
    {
        if (file != NULL)
            fclose(file);

        file = NULL;
    }

    void write_results()
    {
        if (file != NULL)
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

    void write_error(const std::string &error)
    {
        if (file != NULL)
        {
            fprintf(file, "err: %s\n", error.c_str());
            fclose(file);
        }
        file = NULL;
    }

    void simulate()
    {
        init_level_connectivity();
        do
        {
            try
            {
                setup_fight();
            }
            catch (const std::string &error)
            {
                write_error(error);
                end(0, false, "%s", error.c_str());
            }
            do_fight();

            if (trials_done < total_trials)
                delay(Options.arena_delay * 5);
        } while (!contest_canceled && trials_done < total_trials);

        if (total_trials > 0)
        {
            mprf("Final score: %s (%d); %s (%d) [%d ties]",
                 faction_a.desc.c_str(), team_a_wins,
                 faction_b.desc.c_str(), trials_done - team_a_wins - ties,
                 ties);
        }
        delay(Options.arena_delay * 5);

        write_results();
    }
}

/////////////////////////////////////////////////////////////////////////////

// Various arena callbacks

monster_type arena_pick_random_monster(const level_id &place, int power,
                                       int &lev_mons)
{
    if (arena::random_uniques)
    {
        const std::vector<int> &uniques = arena::uniques_list;

        monster_type type = (monster_type) uniques[random2(uniques.size())];
        you.unique_creatures[type] = false;

        return (type);
    }

    if (!arena::cycle_random)
        return (RANDOM_MONSTER);

    for (int tries = 0; tries <= NUM_MONSTERS; tries++)
    {
        arena::cycle_random_pos++;
        if (arena::cycle_random_pos >= NUM_MONSTERS)
            arena::cycle_random_pos = 0;

        const monster_type type = (monster_type) arena::cycle_random_pos;

        if (mons_rarity(type, place) == 0)
            continue;

        if (arena_veto_random_monster(type))
            continue;

        return (type);
    }

    end(1, false, "No random monsters for place '%s'",
        arena::place.describe().c_str());
    return (NUM_MONSTERS);
}

bool arena_veto_random_monster(monster_type type)
{
    if (!arena::allow_immobile && mons_class_is_stationary(type))
        return (true);
    if (!arena::allow_zero_xp && mons_class_flag(type, M_NO_EXP_GAIN))
        return (true);
    if (arena::banned_glyphs[mons_char(type)])
        return (true);

    return (false);
}

bool arena_veto_place_monster(const mgen_data &mg, bool first_band_member,
                              const coord_def& pos)
{
    if (mg.abjuration_duration > 0)
    {
        if (mg.behaviour == BEH_FRIENDLY
            && arena::faction_a.active_members > arena::summon_throttle)
        {
            return (true);
        }
        else if (mg.behaviour == BEH_HOSTILE
                 && arena::faction_b.active_members > arena::summon_throttle)
        {
            return (true);
        }

    }
    return (!arena::allow_bands && !first_band_member
            || arena::banned_glyphs[mons_char(mg.cls)]);
}

void arena_placed_monster(monsters *monster)
{
    if (monster->attitude == ATT_FRIENDLY)
    {
        arena::faction_a.active_members++;
        arena::faction_b.won = false;
    }
    else if (monster->attitude == ATT_HOSTILE)
    {
        arena::faction_b.active_members++;
        arena::faction_a.won = false;
    }

    if (monster->type == MONS_TEST_SPAWNER)
    {
        if (monster->attitude == ATT_FRIENDLY)
            arena::a_spawners.push_back(monster->mindex());
        else if (monster->attitude == ATT_HOSTILE)
            arena::b_spawners.push_back(monster->mindex());
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf("%s enters the arena!", monster->full_name(DESC_CAP_A, true).c_str());
#endif

    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        short it = monster->inv[i];
        if (it != NON_ITEM)
        {
            item_def &item(mitm[it]);
            item.flags |= ISFLAG_IDENT_MASK;

            // Don't leak info on wands or potions.
            if (item.base_type == OBJ_WANDS
                || item.base_type == OBJ_POTIONS)
            {
                item.colour = random_colour();
            }
            // Set the "drop" time here in case the monster drops the
            // item without dying, like being polymorphed.
            arena::item_drop_times[it] = arena::turns;
        }
    }

    if (arena::name_monsters && !monster->is_named())
        monster->mname = make_name(random_int(), false);

    if (monster->is_summoned())
    {
        // Real summons drop corpses and items.
        if (arena::real_summons)
        {
            monster->del_ench(ENCH_ABJ, true, false);
            for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
            {
                short it = monster->inv[i];
                if (it != NON_ITEM)
                    mitm[it].flags &= ~ISFLAG_SUMMONED;
            }
        }

        if (arena::move_summons)
            monster_teleport(monster, true, true);

        if (!arena::allow_chain_summons || !arena::allow_animate)
            arena::adjust_spells(monster, !arena::allow_chain_summons,
                                 !arena::allow_animate);
    }
}

void arena_monster_died(monsters *monster, killer_type killer,
                        int killer_index, bool silent, int corpse)
{
    if (monster->attitude == ATT_FRIENDLY)
        arena::faction_a.active_members--;
    else if (monster->attitude == ATT_HOSTILE)
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
    // Everyone is dead.  Is it a tie, or something else?
    else if (arena::faction_a.active_members <= 0
             && arena::faction_b.active_members <= 0)
    {
        if (monster->flags & MF_HARD_RESET && !MON_KILL(killer))
            end(1, false, "Last arena monster was dismissed.");
        // If all monsters are dead, and the last one to die is a giant
        // spore or ball lightning, then that monster's faction is the
        // winner, since self-destruction is their purpose.  But if a
        // trap causes the spore to explode, and that kills everything,
        // it's a tie, since it counts as the trap killing everyone.
        else if (mons_self_destructs(monster) && MON_KILL(killer))
        {
            if (monster->attitude == ATT_FRIENDLY)
                arena::faction_a.won = true;
            else if (monster->attitude == ATT_HOSTILE)
                arena::faction_b.won = true;
        }
    }

    if (corpse != -1 && corpse != NON_ITEM)
        arena::item_drop_times[corpse] = arena::turns;

    // Won't be dropping any items.
    if (monster->flags & MF_HARD_RESET)
        return;

    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
    {
        int idx = monster->inv[i];
        if (idx == NON_ITEM)
            continue;

        if (mitm[idx].flags & ISFLAG_SUMMONED)
            continue;

        arena::item_drop_times[idx] = arena::turns;
    }
}

static bool _sort_by_age(int a, int b)
{
    return (arena::item_drop_times[a] < arena::item_drop_times[b]);
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
// newest items last.  Items which a monster dropped voluntarily or
// because of being polymorphed, rather than because of dying, are
// culled earlier than they should be, but it's not like we have to be
// fair to the arena monsters.
int arena_cull_items()
{
    std::vector<int> items;

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

    std::sort(items.begin(), items.end(), _sort_by_age);

    std::vector<int> ammo;

    for (unsigned int i = 0, end = items.size(); i < end; i++)
    {
        const int      idx = items[i];
        const item_def &item(mitm[idx]);

        // If the drop time is 0 then this is probably thrown ammo.
        if (arena::item_drop_times[idx] == 0)
        {
            // We know it's at least this old.
            arena::item_drop_times[idx] = arena::turns;

            // Arrows/needles/etc on the floor is just clutter.
            if (item.base_type != OBJ_MISSILES
               || item.sub_type == MI_JAVELIN
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
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "On turn #%d culled %d items dropped by "
                                "monsters, done.",
             arena::turns, cull_count);
#endif
        return (first_avail);
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "On turn #%d culled %d items dropped by "
                            "monsters, culling some more.",
         arena::turns, cull_count);
#endif

    const int count1 = cull_count;
    for (unsigned int i = 0; i < ammo.size(); i++)
    {
        DESTROY_ITEM(ammo[i]);
        if (cull_count >= cull_target)
            break;
    }

    if (cull_count >= cull_target)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Culled %d (probably) ammo items, done.",
             cull_count - count1);
#endif
        return (first_avail);
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Culled %d items total, short of target %d.",
         cull_count, cull_target);
#endif
    return (first_avail);
} // arena_cull_items

/////////////////////////////////////////////////////////////////////////////

void run_arena()
{
    ASSERT(!crawl_state.arena_suspended);

#ifdef WIZARD
    // The playe has wizard powers for the duration of the arena.
    unwind_bool wiz(you.wizard, true);
#endif

    arena::global_setup();
    arena::simulate();
    arena::global_shutdown();
}

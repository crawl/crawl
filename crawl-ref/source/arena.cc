#include "AppHdr.h"

#include "externs.h"
#include "arena.h"
#include "chardump.h"
#include "cio.h"
#include "dungeon.h"
#include "initfile.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-util.h"
#include "monstuff.h"
#include "monplace.h"
#include "output.h"
#include "skills2.h"
#include "spl-util.h"
#include "state.h"
#include "version.h"
#include "view.h"

extern void world_reacts();

namespace arena
{
    // A faction is just a big list of monsters. Monsters will be dropped
    // around the appropriate marker.
    struct faction
    {
        std::string desc;
        mons_list members;
        bool friendly;

        faction(bool fr) : members(), friendly(fr) { }

        void place_at(const coord_def &pos);

        void clear()
        {
            members.clear();
        }
    };

    int total_trials = 0;

    int trials_done = 0;
    int team_a_wins = 0;
    bool allow_summons = true;
    std::string arena_type = "";
    faction faction_a(true);
    faction faction_b(false);
    FILE *file = NULL;
    int message_pos = 0;

    void adjust_monsters()
    {
        if (!allow_summons)
        {
            for (int m = 0; m < MAX_MONSTERS; ++m)
            {
                monsters *mons(&menv[m]);
                if (!mons->alive())
                    continue;

                monster_spells &spells(mons->spells);
                for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
                {
                    spell_type sp = spells[i];
                    if (spell_typematch(sp, SPTYP_SUMMONING))
                        spells[i] = SPELL_NO_SPELL;
                }
            }
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
                const coord_def place =
                    find_newmons_square_contiguous(MONS_GIANT_BAT, pos, 6);
                if (!in_bounds(place))
                    break;

                const int imon = dgn_place_monster(spec, you.your_level,
                                                   place, false, true, false);
                if (imon == -1)
                    end(1, false, "Failed to create monster at (%d,%d)",
                        place.x, place.y);
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
        dgn_reset_level();

        for (int x = 0; x < GXM; ++x)
            for (int y = 0; y < GYM; ++y)
                grd[x][y] = DNGN_ROCK_WALL;

        unwind_bool gen(Generating_Level, true);

        typedef unwind_var< std::set<std::string> > unwind_stringset;

        const unwind_stringset mtags(you.uniq_map_tags);
        const unwind_stringset mnames(you.uniq_map_names);

        std::string map_name = "arena_" + arena_type;
        const map_def *map = random_map_for_tag(map_name.c_str(), false);

        if (!map)
            throw make_stringf("No arena maps named \"%s\"", arena_type.c_str());

        ASSERT(map);
        dgn_place_map(map, true, true);

        if (!env.rock_colour)
            env.rock_colour = CYAN;
        if (!env.floor_colour)
            env.floor_colour = LIGHTGREY;
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

        allow_summons = !strip_tag(spec, "no_summons");

        const int ntrials = strip_number_tag(spec, "t:");
        if (ntrials != TAG_UNFOUND && ntrials >= 1 && ntrials <= 99
            && !total_trials)
            total_trials = ntrials;

        arena_type = strip_tag_prefix(spec, "arena:");

        if (arena_type.empty())
            arena_type = "default";

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
        unwind_var< FixedVector<bool, NUM_MONSTERS> >
            uniq(you.unique_creatures);

        coord_def place_a(dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I));
        coord_def place_b(dgn_find_feature_marker(DNGN_STONE_STAIRS_DOWN_I));
        faction_a.place_at(place_a);
        faction_b.place_at(place_b);
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
                     total_trials ? trials_done - team_a_wins : -1);

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
        // Fix up the viewport.
        you.moveto(yplace);

        strcpy(you.your_name, "Arena");

        you.hp = you.hp_max = 99;
        you.your_level = 20;

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

    // Returns true as long as at least one member of each faction is alive.
    bool fight_is_on()
    {
        bool found_friend = false;
        bool found_enemy = false;
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            const monsters *mons(&menv[i]);
            if (mons->alive())
            {
                if (mons->attitude == ATT_FRIENDLY)
                    found_friend = true;
                else if (mons->attitude == ATT_HOSTILE)
                    found_enemy = true;
                if (found_friend && found_enemy)
                    return (true);
            }
        }
        return (false);
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
            {
                behaviour_event(mons, ME_DISTURB, MHITNOT, mons->pos());
            }
        }
    }

    bool friendlies_win()
    {
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mons(&menv[i]);
            if (mons->alive())
                return (mons->attitude == ATT_FRIENDLY);
        }
        return (false);
    }


    void dump_messages()
    {
        if (!Options.arena_dump_msgs || file == NULL)
            return;

        std::vector<std::string> messages =
            get_recent_messages(message_pos,
                                !Options.arena_dump_msgs_all);

        for (unsigned int i = 0; i < messages.size(); i++)
            fprintf(file, "%s\n", messages[i].c_str());
    }

    void do_fight()
    {
        mesclr(true);
        {
            cursor_control coff(false);
            while (fight_is_on())
            {
                if (kbhit() && getch() == ESCAPE)
                    end(0, false, "Canceled contest at user request");

                viewwindow(true, false);
                unwind_var<coord_def> pos(you.position);
                // Move hero offscreen.
                you.position.y = -1;
                you.time_taken = 10;
                //report_foes();
                world_reacts();
                delay(Options.arena_delay);
                mesclr();
                dump_messages();
            }
            viewwindow(true, false);
        }

        mesclr();

        const bool team_a_won = friendlies_win();

        trials_done++;

        if (team_a_won)
            team_a_wins++;

        show_fight_banner(true);

        const char *msg;
        if (Options.arena_dump_msgs || Options.arena_list_eq)
            msg = "---------- Winner: %s! ----------";
        else
            msg = "Winner: %s!";

        mprf(msg,
             team_a_won ? faction_a.desc.c_str() : faction_b.desc.c_str());
        dump_messages();
    }

    void global_setup()
    {
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
            fprintf(file, "%d-%d\n", team_a_wins, trials_done - team_a_wins);
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
                delay(Options.arena_delay * 8);
        } while (trials_done < total_trials);

        if (total_trials > 0)
        {
            mprf("Final score: %s (%d); %s (%d)",
                 faction_a.desc.c_str(), team_a_wins,
                 faction_b.desc.c_str(), trials_done - team_a_wins);
            delay(Options.arena_delay * 8);
        }

        write_results();
    }
}

void run_arena()
{
    arena::global_setup();
    arena::simulate();
    arena::global_shutdown();
}

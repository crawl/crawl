/**
 * @file
 * @brief Map generation statistics/testing.
**/

#include "AppHdr.h"

#include "dbg-maps.h"

#include "branch.h"
#include "chardump.h"
#include "crash.h"
#include "dbg-objstat.h"
#include "dungeon.h"
#include "env.h"
#include "initfile.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "ng-init.h"
#include "player.h"
#include "shopping.h"
#include "state.h"
#include "stringutil.h"
#include "view.h"

#ifdef DEBUG_STATISTICS
// Map statistics generation.

static map<string, int> try_count;
static map<string, int> use_count;
static map<string, int> success_count;
static vector<level_id> generated_levels;
static int branch_count;
static map<level_id, int> level_mapcounts;
static map< level_id, pair<int,int> > map_builds;
static map< level_id, set<string> > level_mapsused;

typedef map< string, set<level_id> > mapname_place_map;
static mapname_place_map map_levelsused;
static map<string, string> errors;
static string last_error;

static int levels_tried = 0, levels_failed = 0;
static int build_attempts = 0, level_vetoes = 0;
// Map from message to counts.
static map<string, int> veto_messages;

void mapstat_report_map_build_start()
{
    build_attempts++;
    map_builds[level_id::current()].first++;
}

void mapstat_report_map_veto(const string &message)
{
    level_vetoes++;
    ++veto_messages[message];
    map_builds[level_id::current()].second++;
}

static bool _is_disconnected_level()
{
    // Don't care about non-Dungeon levels.
    if (!player_in_connected_branch()
        || (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED))
    {
        return false;
    }

    return dgn_count_disconnected_zones(true);
}

static bool _do_build_level()
{
    clear_messages();
    mprf("On %s; %d g, %d fail, %u err%s, %u uniq, "
         "%d try, %d (%.2f%%) vetos",
         level_id::current().describe().c_str(), levels_tried, levels_failed,
         (unsigned int)errors.size(), last_error.empty() ? ""
         : (" (" + last_error + ")").c_str(), (unsigned int) use_count.size(),
         build_attempts, level_vetoes,
         build_attempts ? level_vetoes * 100.0 / build_attempts : 0.0);

    watchdog();

    no_messages mx;
    if (kbhit() && key_is_escape(getchk()))
    {
        mprf(MSGCH_WARN, "User requested cancel");
        return false;
    }

    ++levels_tried;
    if (!builder())
    {
        ++levels_failed;
        // Abort level build failure in objstat since the statistics will be
        // off.
        // XXX - Maybe try the level build some small number of times instead
        // of aborting on first fail.
        if (crawl_state.obj_stat_gen)
        {
            fprintf(stderr, "Level build failed on %s. Aborting.\n",
                    level_id::current().describe().c_str());
            return false;
        }
        return true;
    }

    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            if (grd[x][y] == DNGN_RUNED_DOOR)
                grd[x][y] = DNGN_CLOSED_DOOR;
            // objstat tallying of monsters and shop items.
            if (crawl_state.obj_stat_gen)
            {
                coord_def pos(x, y);
                monster *mons = monster_at(pos);
                if (mons)
                    objstat_record_monster(mons);

                const shop_struct * const shop = shop_at(pos);
                if (shop && shop->defined())
                {
                    for (const auto &item : shop->stock)
                        if (item.defined())
                            objstat_record_item(item);
                }
            }
        }


    // Record floor items for objstat.
    if (crawl_state.obj_stat_gen)
        for (auto &item : mitm)
            if (item.defined())
                objstat_record_item(item);

    {
        unwind_bool wiz(you.wizard, true);
        magic_mapping(1000, 100, true, true, false,
                      coord_def(GXM/2, GYM/2));
    }

    // Dump the map of any disconnected level if this CLO is set.
    if (_is_disconnected_level() && crawl_state.map_stat_dump_disconnect)
    {
        string vaults = comma_separated_fn(
                begin(env.level_vaults), end(env.level_vaults),
                [](unique_ptr<vault_placement> &lp) { return lp->map.name; },
                ", ", ", ");

        if (!vaults.empty())
            vaults = " (" + vaults + ")";

        FILE *fp = fopen("map.dump", "w");
        fprintf(fp, "Bad (disconnected) level (%s) on %s%s.\n\n",
                env.level_build_method.c_str(),
                level_id::current().describe().c_str(),
                vaults.c_str());

        dump_map(fp, true);
        fclose(fp);

        mprf(MSGCH_ERROR,
             "Bad (disconnected) level on %s%s",
             level_id::current().describe().c_str(),
             vaults.c_str());

        return false;
    }
    return true;
}

static void _dungeon_places()
{
    generated_levels.clear();
    branch_count = 0;
    for (branch_iterator it; it; ++it)
    {
        if (brdepth[it->id] == -1)
            continue;
#if TAG_MAJOR_VERSION == 34
        // Don't want to include Forest since it doesn't generate
        if (it->id == BRANCH_FOREST)
            continue;
#endif

        bool new_branch = true;
        for (int depth = 1; depth <= brdepth[it->id]; ++depth)
        {
            level_id l(it->id, depth);
            if (SysEnv.map_gen_range.get() && !SysEnv.map_gen_range->is_usable_in(l))
                continue;
            generated_levels.push_back(l);
            if (new_branch)
                ++branch_count;
            new_branch = false;
        }
    }
}

static bool _build_dungeon()
{
    for (const level_id lid : generated_levels)
    {
        you.where_are_you = lid.branch;
        you.depth = lid.depth;

#if TAG_MAJOR_VERSION == 34
        // An unholy hack, FIXME!
        if (!brentry[BRANCH_FOREST].is_valid()
            && lid.branch == BRANCH_FOREST && lid.depth == 5)
        {
            you.unique_creatures.set(MONS_THE_ENCHANTRESS, false);
        }
#endif
        if (!_do_build_level())
            return false;
    }
    return true;
}

/**
 * Build dungeon levels for mapstat or objstat.
 *
 * The exact branches/levels built and number of build iterations is set by the
 * command-line options for mapstat/objstat.

 * @returns True if all iterations built successfully. For mapstat, this can
 * return false if an iteration produced a disconnected level, since for
 * diagnostic purposes we record the map in detail to a file and exit. For
 * objstat, this only returns false if the primary dungeon generation function
 * builder() fails, as the level may be in an invalid state and any object
 * statistics erroneous.
*/
bool mapstat_build_levels()
{
    if (!generated_levels.size())
        _dungeon_places();
    printf("Iteration: ");
    fflush(stdout);
    for (int i = 0; i < SysEnv.map_gen_iters; ++i)
    {
        clear_messages();
        mprf("On %d of %d; %d g, %d fail, %u err%s, %u uniq, "
             "%d try, %d (%.2f%%) vetoes",
             i, SysEnv.map_gen_iters, levels_tried, levels_failed,
             (unsigned int)errors.size(),
             last_error.empty() ? "" : (" (" + last_error + ")").c_str(),
             (unsigned int)use_count.size(), build_attempts, level_vetoes,
             build_attempts ? level_vetoes * 100.0 / build_attempts : 0.0);
        printf("%d..", i + 1);
        fflush(stdout);
        dlua.callfn("dgn_clear_data", "");
        you.uniq_map_tags.clear();
        you.uniq_map_names.clear();
        you.unique_creatures.reset();
        initialise_branch_depths();
        init_level_connectivity();
        if (!_build_dungeon())
            return false;
        if (crawl_state.obj_stat_gen)
            objstat_iteration_stats();
    }
    printf("Finished.\n");
    fflush(stdout);
    return true;
}

void mapstat_report_map_try(const map_def &map)
{
    try_count[map.name]++;
}

void mapstat_report_map_use(const map_def &map)
{
    use_count[map.name]++;
    level_mapcounts[level_id::current()]++;
    level_mapsused[level_id::current()].insert(map.name);
    map_levelsused[map.name].insert(level_id::current());
}

void mapstat_report_map_success(const string &map_name)
{
    success_count[map_name]++;
}

void mapstat_report_error(const map_def &map, const string &err)
{
    last_error = err;
}

static void _report_available_random_vaults(FILE *outf)
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();

    fprintf(outf, "\n\nRandom vaults available by dungeon level:\n");
    for (auto lvl : generated_levels)
    {
        // The watchdog has already been activated by _do_build_level.
        // Reporting all the vaults could take a while.
        watchdog();
        fprintf(outf, "\n%s -------------\n", lvl.describe().c_str());
        clear_messages();
        mprf("Examining random maps at %s", lvl.describe().c_str());
        mapstat_report_random_maps(outf, lvl);
        if (kbhit() && key_is_escape(getchk()))
            break;
        fprintf(outf, "---------------------------------\n");
    }
}

static void _check_mapless(const level_id &lid, vector<level_id> &mapless)
{
    if (!level_mapsused.count(lid))
        mapless.push_back(lid);
}

static void _write_map_stats()
{
    const char *out_file = "mapstat.log";
    FILE *outf = fopen(out_file, "w");
    printf("Writing map stats to %s...", out_file);
    fflush(stdout);
    fprintf(outf, "Map Generation Stats\n\n");
    fprintf(outf, "Levels attempted: %d, built: %d, failed: %d\n",
            levels_tried, levels_tried - levels_failed,
            levels_failed);
    if (!errors.empty())
    {
        fprintf(outf, "\n\nMap errors:\n");
        for (const auto &err : errors)
            fprintf(outf, "%s: %s\n", err.first.c_str(), err.second.c_str());
    }

    vector<level_id> mapless;
    for (branch_iterator it; it; ++it)
    {
        if (brdepth[it->id] == -1)
            continue;

        for (int dep = 1; dep <= brdepth[it->id]; ++dep)
        {
            const level_id lid(it->id, dep);
            if (SysEnv.map_gen_range.get()
                && !SysEnv.map_gen_range->is_usable_in(lid))
            {
                continue;
            }
            _check_mapless(lid, mapless);
        }
    }

    if (!mapless.empty())
    {
        fprintf(outf, "\n\nLevels with no maps:\n");
        for (int i = 0, size = mapless.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, mapless[i].describe().c_str());
    }

    _report_available_random_vaults(outf);

    vector<string> unused_maps;
    for (int i = 0, size = map_count(); i < size; ++i)
    {
        const map_def *map = map_by_index(i);
        if (!try_count.count(map->name)
            && !map->has_tag("dummy"))
        {
            unused_maps.push_back(map->name);
        }
    }

    if (level_vetoes)
    {
        fprintf(outf, "\n\nMost vetoed levels:\n");
        multimap<int, level_id> sortedvetos;
        for (const auto &entry : map_builds)
        {
            if (!entry.second.second)
                continue;

            sortedvetos.insert(make_pair(entry.second.second, entry.first));
        }

        int count = 0;
        for (auto i = sortedvetos.rbegin(); i != sortedvetos.rend(); ++i)
        {
            const int vetoes = i->first;
            const int tries  = map_builds[i->second].first;
            fprintf(outf, "%3d) %s (%d of %d vetoed, %.2f%%)\n",
                    ++count, i->second.describe().c_str(),
                    vetoes, tries, vetoes * 100.0 / tries);
        }

        fprintf(outf, "\n\nVeto reasons:\n");
        multimap<int, string> sortedreasons;
        for (const auto &entry : veto_messages)
            sortedreasons.insert(make_pair(entry.second, entry.first));

        for (auto i = sortedreasons.rbegin(); i != sortedreasons.rend(); ++i)
            fprintf(outf, "%3d) %s\n", i->first, i->second.c_str());
    }

    if (!unused_maps.empty() && !SysEnv.map_gen_range.get())
    {
        fprintf(outf, "\n\nUnused maps:\n\n");
        for (int i = 0, size = unused_maps.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, unused_maps[i].c_str());
    }

    fprintf(outf, "\n\nMaps by level:\n\n");
    for (const auto &entry : level_mapsused)
    {
        string line =
            make_stringf("%s ------------\n", entry.first.describe().c_str());
        const set<string> &maps = entry.second;
        for (auto j = maps.begin(); j != maps.end(); ++j)
        {
            if (j != maps.begin())
                line += ", ";
            if (line.length() + j->length() > 79)
            {
                fprintf(outf, "%s\n", line.c_str());
                line = *j;
            }
            else
                line += *j;
        }

        if (!line.empty())
            fprintf(outf, "%s\n", line.c_str());

        fprintf(outf, "------------\n\n");
    }

    fprintf(outf, "\n\nMaps used (successful, placed incl. vetoed, tried):\n\n");
    multimap<int, string> usedmaps;
    for (const auto &entry : try_count)
        usedmaps.insert(make_pair(entry.second, entry.first));

    for (const auto &entry : usedmaps)
    {
        const int tries = entry.first;
        const int uses = lookup(use_count, entry.second, 0);
        const int succ = lookup(success_count, entry.second, 0);
        fprintf(outf, "%4d, %4d, %4d: %s\n",
                succ, uses, tries, entry.second.c_str());
    }

    fprintf(outf, "\n\nMaps and where used:\n\n");
    for (const auto &entry : map_levelsused)
    {
        fprintf(outf, "%s ============\n", entry.first.c_str());
        string line;
        for (auto lvl : entry.second)
        {
            if (!line.empty())
                line += ", ";
            string level = lvl.describe();
            if (line.length() + level.length() > 79)
            {
                fprintf(outf, "%s\n", line.c_str());
                line = level;
            }
            else
                line += level;
        }
        if (!line.empty())
            fprintf(outf, "%s\n", line.c_str());

        fprintf(outf, "==================\n\n");
    }
    fclose(outf);
    printf("\n");
}

void mapstat_generate_stats()
{
    // Warn assertions about possible oddities like the artefact list being
    // cleared.
    you.wizard = true;
    // Let "acquire foo" have skill aptitudes to work with.
    you.species = SP_HUMAN;

    initialise_item_descriptions();
    initialise_branch_depths();
    // We have to run map preludes ourselves.
    run_map_global_preludes();
    run_map_local_preludes();

    _dungeon_places();
    clear_messages();
    mpr("던전의 지도를 생성한다");
    printf("Generating map stats for %d iteration(s) of %d level(s) over "
           "%d branch(es).\n", SysEnv.map_gen_iters,
           (int) generated_levels.size(), branch_count);
    fflush(stdout);
    // We write mapstats even if the iterations were aborted due to a bad level
    // build.
    mapstat_build_levels();
    _write_map_stats();
    printf("Map stats complete.\n");
}

#endif // DEBUG_STATISTICS

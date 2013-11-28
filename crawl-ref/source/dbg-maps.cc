/**
 * @file
 * @brief Map generation statistics/testing.
**/

#include "AppHdr.h"

#include "dbg-maps.h"

#include "branch.h"
#include "chardump.h"
#include "crash.h"
#include "dungeon.h"
#include "env.h"
#include "initfile.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "ng-init.h"
#include "player.h"
#include "view.h"

#ifdef DEBUG_DIAGNOSTICS
// Map statistics generation.

static map<string, int> mapgen_try_count;
static map<string, int> mapgen_use_count;
static map<level_id, int> mapgen_level_mapcounts;
static map< level_id, pair<int,int> > mapgen_map_builds;
static map< level_id, set<string> > mapgen_level_mapsused;

typedef map< string, set<level_id> > mapname_place_map;
static mapname_place_map mapgen_map_levelsused;
static map<string, string> mapgen_errors;
static string mapgen_last_error;

static int mg_levels_tried = 0, mg_levels_failed = 0;
static int mg_build_attempts = 0, mg_vetoes = 0;

void mapgen_report_map_build_start()
{
    mg_build_attempts++;
    mapgen_map_builds[level_id::current()].first++;
}

void mapgen_report_map_veto()
{
    mg_vetoes++;
    mapgen_map_builds[level_id::current()].second++;
}

static bool _mg_is_disconnected_level()
{
    // Don't care about non-Dungeon levels.
    if (!player_in_connected_branch()
        || (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED))
    {
        return false;
    }

    return dgn_count_disconnected_zones(true);
}

static bool mg_do_build_level(int niters)
{
    mesclr();
    mprf("On %s (%d); %d g, %d fail, %u err%s, %u uniq, "
         "%d try, %d (%.2lf%%) vetos",
         level_id::current().describe().c_str(), niters,
         mg_levels_tried, mg_levels_failed,
         (unsigned int)mapgen_errors.size(),
         mapgen_last_error.empty()? ""
         : (" (" + mapgen_last_error + ")").c_str(),
         (unsigned int)mapgen_use_count.size(),
         mg_build_attempts, mg_vetoes,
         mg_build_attempts? mg_vetoes * 100.0 / mg_build_attempts : 0.0);

    watchdog();

    no_messages mx;
    for (int i = 0; i < niters; ++i)
    {
        if (kbhit() && key_is_escape(getchk()))
        {
            mpr("User requested cancel", MSGCH_WARN);
            return false;
        }

        ++mg_levels_tried;
        if (!builder())
        {
            ++mg_levels_failed;
            continue;
        }

        for (int y = 0; y < GYM; ++y)
            for (int x = 0; x < GXM; ++x)
            {
                switch (grd[x][y])
                {
                case DNGN_RUNED_DOOR:
                    grd[x][y] = DNGN_CLOSED_DOOR;
                    break;
                default:
                    break;
                }
            }

        {
            unwind_bool wiz(you.wizard, true);
            magic_mapping(1000, 100, true, true, false,
                          coord_def(GXM/2, GYM/2));
        }
        if (_mg_is_disconnected_level())
        {
            string vaults;
            for (int j = 0, size = env.level_vaults.size(); j < size; ++j)
            {
                if (j && !vaults.empty())
                    vaults += ", ";
                vaults += env.level_vaults[j]->map.name;
            }

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
    }
    return true;
}

static vector<level_id> mg_dungeon_places()
{
    vector<level_id> places;

    for (int br = BRANCH_DUNGEON; br < NUM_BRANCHES; ++br)
    {
        if (brdepth[br] == -1)
            continue;

        const branch_type branch = static_cast<branch_type>(br);
        for (int depth = 1; depth <= brdepth[br]; ++depth)
        {
            level_id l(branch, depth);
            if (SysEnv.map_gen_range.get() && !SysEnv.map_gen_range->is_usable_in(l))
                continue;
            places.push_back(l);
        }
    }
    return places;
}

static bool mg_build_dungeon()
{
    const vector<level_id> places = mg_dungeon_places();

    for (int i = 0, size = places.size(); i < size; ++i)
    {
        const level_id &lid = places[i];
        you.where_are_you = lid.branch;
        you.depth = lid.depth;

        // An unholy hack, FIXME!
        if (!brentry[BRANCH_FOREST].is_valid()
            && lid.branch == BRANCH_FOREST && lid.depth == 5)
        {
            you.unique_creatures.set(MONS_THE_ENCHANTRESS, false);
        }

        if (!mg_do_build_level(1))
            return false;
    }
    return true;
}

static void mg_build_levels(int niters)
{
    mesclr();
    mpr("Generating dungeon map stats");

    for (int i = 0; i < niters; ++i)
    {
        mesclr();
        mprf("On %d of %d; %d g, %d fail, %u err%s, %u uniq, "
             "%d try, %d (%.2lf%%) vetoes",
             i, niters,
             mg_levels_tried, mg_levels_failed,
             (unsigned int)mapgen_errors.size(),
             mapgen_last_error.empty()? ""
             : (" (" + mapgen_last_error + ")").c_str(),
             (unsigned int)mapgen_use_count.size(),
             mg_build_attempts, mg_vetoes,
             mg_build_attempts? mg_vetoes * 100.0 / mg_build_attempts : 0.0);

        dlua.callfn("dgn_clear_data", "");
        you.uniq_map_tags.clear();
        you.uniq_map_names.clear();
        you.unique_creatures.reset();
        initialise_branch_depths();
        init_level_connectivity();
        if (!mg_build_dungeon())
            break;
    }
}

void mapgen_report_map_try(const map_def &map)
{
    mapgen_try_count[map.name]++;
}

void mapgen_report_map_use(const map_def &map)
{
    mapgen_use_count[map.name]++;
    mapgen_level_mapcounts[level_id::current()]++;
    mapgen_level_mapsused[level_id::current()].insert(map.name);
    mapgen_map_levelsused[map.name].insert(level_id::current());
}

void mapgen_report_error(const map_def &map, const string &err)
{
    mapgen_last_error = err;
}

static void _mapgen_report_available_random_vaults(FILE *outf)
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();

    const vector<level_id> places = mg_dungeon_places();
    fprintf(outf, "\n\nRandom vaults available by dungeon level:\n");

    for (vector<level_id>::const_iterator i = places.begin();
         i != places.end(); ++i)
    {
        fprintf(outf, "\n%s -------------\n", i->describe().c_str());
        mesclr();
        mprf("Examining random maps at %s", i->describe().c_str());
        mg_report_random_maps(outf, *i);
        if (kbhit() && key_is_escape(getchk()))
            break;
        fprintf(outf, "---------------------------------\n");
    }
}

static void _check_mapless(const level_id &lid, vector<level_id> &mapless)
{
    if (!mapgen_level_mapsused.count(lid))
        mapless.push_back(lid);
}

static void _write_mapgen_stats()
{
    FILE *outf = fopen("mapgen.log", "w");
    fprintf(outf, "Map Generation Stats\n\n");
    fprintf(outf, "Levels attempted: %d, built: %d, failed: %d\n",
            mg_levels_tried, mg_levels_tried - mg_levels_failed,
            mg_levels_failed);

    if (!mapgen_errors.empty())
    {
        fprintf(outf, "\n\nMap errors:\n");
        for (map<string, string>::const_iterator i = mapgen_errors.begin();
             i != mapgen_errors.end(); ++i)
        {
            fprintf(outf, "%s: %s\n",
                    i->first.c_str(), i->second.c_str());
        }
    }

    vector<level_id> mapless;
    for (int i = BRANCH_DUNGEON; i < NUM_BRANCHES; ++i)
    {
        if (brdepth[i] == -1)
            continue;

        const branch_type br = static_cast<branch_type>(i);
        for (int dep = 1; dep <= brdepth[i]; ++dep)
        {
            const level_id lid(br, dep);
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

    _mapgen_report_available_random_vaults(outf);

    vector<string> unused_maps;
    for (int i = 0, size = map_count(); i < size; ++i)
    {
        const map_def *map = map_by_index(i);
        if (!mapgen_try_count.count(map->name)
            && !map->has_tag("dummy"))
        {
            unused_maps.push_back(map->name);
        }
    }

    if (mg_vetoes)
    {
        fprintf(outf, "\n\nMost vetoed levels:\n");
        multimap<int, level_id> sortedvetos;
        for (map< level_id, pair<int, int> >::const_iterator
                 i = mapgen_map_builds.begin(); i != mapgen_map_builds.end();
             ++i)
        {
            if (!i->second.second)
                continue;

            sortedvetos.insert(pair<int, level_id>(i->second.second, i->first));
        }

        int count = 0;
        for (multimap<int, level_id>::reverse_iterator
                 i = sortedvetos.rbegin(); i != sortedvetos.rend(); ++i)
        {
            const int vetoes = i->first;
            const int tries  = mapgen_map_builds[i->second].first;
            fprintf(outf, "%3d) %s (%d of %d vetoed, %.2f%%)\n",
                    ++count, i->second.describe().c_str(),
                    vetoes, tries, vetoes * 100.0 / tries);
        }
    }

    if (!unused_maps.empty() && !SysEnv.map_gen_range.get())
    {
        fprintf(outf, "\n\nUnused maps:\n\n");
        for (int i = 0, size = unused_maps.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, unused_maps[i].c_str());
    }

    fprintf(outf, "\n\nMaps by level:\n\n");
    for (map<level_id, set<string> >::const_iterator i =
             mapgen_level_mapsused.begin(); i != mapgen_level_mapsused.end();
         ++i)
    {
        string line =
            make_stringf("%s ------------\n", i->first.describe().c_str());
        const set<string> &maps = i->second;
        for (set<string>::const_iterator j = maps.begin(); j != maps.end(); ++j)
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

    fprintf(outf, "\n\nMaps used:\n\n");
    multimap<int, string> usedmaps;
    for (map<string, int>::const_iterator i = mapgen_try_count.begin();
         i != mapgen_try_count.end(); ++i)
    {
        usedmaps.insert(pair<int, string>(i->second, i->first));
    }

    for (multimap<int, string>::reverse_iterator i = usedmaps.rbegin();
         i != usedmaps.rend(); ++i)
    {
        const int tries = i->first;
        map<string, int>::const_iterator iuse = mapgen_use_count.find(i->second);
        const int uses = iuse == mapgen_use_count.end()? 0 : iuse->second;
        if (tries == uses)
            fprintf(outf, "%4d       : %s\n", tries, i->second.c_str());
        else
            fprintf(outf, "%4d (%4d): %s\n", uses, tries, i->second.c_str());
    }

    fprintf(outf, "\n\nMaps and where used:\n\n");
    for (mapname_place_map::iterator i = mapgen_map_levelsused.begin();
         i != mapgen_map_levelsused.end(); ++i)
    {
        fprintf(outf, "%s ============\n", i->first.c_str());
        string line;
        for (set<level_id>::const_iterator j = i->second.begin();
             j != i->second.end(); ++j)
        {
            if (!line.empty())
                line += ", ";
            string level = j->describe();
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
}

void generate_map_stats()
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
    mg_build_levels(SysEnv.map_gen_iters);
    _write_mapgen_stats();
}

#endif // DEBUG_DIAGNOSTICS

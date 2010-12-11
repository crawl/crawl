/*
 *  File:       ctest.cc
 *  Summary:    Crawl Lua test cases
 *  Written by: Darshan Shaligram
 *
 *  ctest runs Lua tests found in the test directory. The intent here
 *  is to test parts of Crawl that can be easily tested from within Crawl
 *  itself (such as LOS). As a side-effect, writing Lua bindings to support
 *  tests will expand the available Lua bindings. :-)
 *
 *  Tests will run only with Crawl built in its source tree without
 *  DATA_DIR_PATH set.
 */

#include "AppHdr.h"

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)

#include "clua.h"
#include "cluautil.h"
#include "dlua.h"
#include "files.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "ng-init.h"
#include "state.h"
#include "stuff.h"
#include "zotdef.h"

#include <algorithm>
#include <vector>

namespace crawl_tests
{
    const std::string test_dir = "test";
    const std::string script_dir = "scripts";
    const std::string test_player_name = "Superbug99";
    const species_type test_player_species = SP_HUMAN;
    const job_type test_player_job = JOB_FIGHTER;
    const char *activity = "test";

    int ntests = 0;
    int nsuccess = 0;

    typedef std::pair<std::string, std::string> file_error;
    std::vector<file_error> failures;

    void reset_test_data()
    {
        ntests = 0;
        nsuccess = 0;
        failures.clear();
        you.your_name = test_player_name;
        you.species = test_player_species;
        you.char_class = test_player_job;
    }

    int crawl_begin_test(lua_State *ls)
    {
        mprf(MSGCH_PROMPT, "Starting %s: %s",
             activity,
             luaL_checkstring(ls, 1));
        lua_pushnumber(ls, ++ntests);
        return (1);
    }

    int crawl_test_success(lua_State *ls)
    {
        if (!crawl_state.script)
            mprf(MSGCH_PROMPT, "Test success: %s", luaL_checkstring(ls, 1));
        lua_pushnumber(ls, ++nsuccess);
        return (1);
    }

    int crawl_script_args(lua_State *ls)
    {
        return clua_stringtable(ls, crawl_state.script_args);
    }

    static const struct luaL_reg crawl_test_lib[] =
    {
        { "begin_test", crawl_begin_test },
        { "test_success", crawl_test_success },
        { "script_args", crawl_script_args },
        { NULL, NULL }
    };

    void init_test_bindings()
    {
        lua_stack_cleaner clean(dlua);
        luaL_openlib(dlua, "crawl", crawl_test_lib, 0);
        dlua.execfile("clua/test.lua", true, true);
        initialise_branch_depths();
    }

    bool is_test_selected(const std::string &testname)
    {
        if (crawl_state.tests_selected.empty())
            return (true);
        for (int i = 0, size = crawl_state.tests_selected.size();
             i < size; ++i)
        {
            const std::string &phrase(crawl_state.tests_selected[i]);
            if (testname.find(phrase) != std::string::npos)
                return (true);
        }
        return (false);
    }

    void run_test(const std::string &file)
    {
        if (!is_test_selected(file))
            return;

        ++ntests;
        mprf(MSGCH_DIAGNOSTICS, "Running %s %d: %s",
             activity, ntests, file.c_str());
        flush_prev_message();

        const std::string path(
            catpath(crawl_state.script? script_dir : test_dir, file));
        dlua.execfile(path.c_str(), true, false);
        if (dlua.error.empty())
            ++nsuccess;
        else
            failures.push_back(file_error(file, dlua.error));
    }

    static bool _has_test(const std::string& test)
    {
        if (crawl_state.script)
            return false;
        if (crawl_state.tests_selected.empty())
            return true;
        return crawl_state.tests_selected[0].find(test) != std::string::npos;
    }

    // Assumes curses has already been initialized.
    bool run_tests(bool exit_on_complete)
    {
        if (crawl_state.script)
            activity = "script";

        flush_prev_message();

        run_map_preludes();
        reset_test_data();

        init_test_bindings();

        if (_has_test("makeitem"))
            makeitem_tests();
        if (_has_test("zotdef_wave"))
            debug_waves();

        // Get a list of Lua files in test. Order of execution of
        // tests should be irrelevant.
        const std::vector<std::string> tests(
            get_dir_files_ext(crawl_state.script? script_dir : test_dir,
                              ".lua"));
        std::for_each(tests.begin(), tests.end(),
                      run_test);

        if (failures.empty() && !ntests && crawl_state.script)
            failures.push_back(
                file_error(
                    "Script setup",
                    "No scripts found matching " +
                    comma_separated_line(crawl_state.tests_selected.begin(),
                                         crawl_state.tests_selected.end(),
                                         ", ",
                                         ", ")));

        if (exit_on_complete)
        {
            cio_cleanup();
            for (int i = 0, size = failures.size(); i < size; ++i)
            {
                const file_error &fe(failures[i]);
                fprintf(stderr, "%s error: %s\n",
                        activity, fe.second.c_str());
            }
            const int code = failures.empty() ? 0 : 1;
            end(code, false, "%d %ss, %d succeeded, %d failed",
                ntests, activity, nsuccess, failures.size());
        }
        return (failures.empty());
    }
}

#endif // DEBUG_DIAGNOSTICS

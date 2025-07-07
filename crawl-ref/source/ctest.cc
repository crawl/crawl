/**
 * @file
 * @brief Crawl Lua test cases
 *
 * ctest runs Lua tests found in the test directory. The intent here
 * is to test parts of Crawl that can be easily tested from within Crawl
 * itself (such as LOS). As a side-effect, writing Lua bindings to support
 * tests will expand the available Lua bindings. :-)
 *
 * Tests will run only with Crawl built in its source tree without
 * DATA_DIR_PATH set.
**/

#include "AppHdr.h"

#include "ctest.h"

#include <algorithm>
#include <vector>

#include "clua.h"
#include "cluautil.h"
#include "coordit.h"
#include "dlua.h"
#include "end.h"
#include "errors.h"
#include "files.h"
#include "fixedp.h"
#include "item-name.h"
#include "jobs.h"
#include "libutil.h"
#include "mapdef.h"
#include "maps.h"
#include "message.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-util.h"
#include "ng-init.h"
#include "state.h"
#include "stringutil.h"
#include "xom.h"

static const string test_dir = "test";
static const string script_dir = "scripts";
static const char *activity = "test";

static int ntests = 0;
static int nsuccess = 0;

typedef pair<string, string> file_error;
static vector<file_error> failures;

static void _reset_test_data()
{
    ntests = 0;
    nsuccess = 0;
    failures.clear();
    you.your_name = "Superbug99";
    you.species = SP_HUMAN;
    you.char_class = JOB_FIGHTER;
}

static int crawl_begin_test(lua_State *ls)
{
    mprf(MSGCH_PROMPT, "Starting %s: %s",
         activity,
         luaL_checkstring(ls, 1));
    lua_pushnumber(ls, ++ntests);
    return 1;
}

static int crawl_test_success(lua_State *ls)
{
    if (!crawl_state.script)
        mprf(MSGCH_PROMPT, "Test success: %s", luaL_checkstring(ls, 1));
    lua_pushnumber(ls, ++nsuccess);
    return 1;
}

static int crawl_script_args(lua_State *ls)
{
    return clua_stringtable(ls, crawl_state.script_args);
}

static const struct luaL_reg crawl_test_lib[] =
{
    { "begin_test", crawl_begin_test },
    { "test_success", crawl_test_success },
    { "script_args", crawl_script_args },
    { nullptr, nullptr }
};

static void _init_test_bindings()
{
    lua_stack_cleaner clean(dlua);
    luaL_openlib(dlua, "crawl", crawl_test_lib, 0);
    dlua.execfile("dlua/test.lua", true, true);
    initialise_branch_depths();
    initialise_item_descriptions();
}

static bool _is_test_selected(const string &testname)
{
    if (crawl_state.test_list)
    {
        ASSERT(ends_with(testname, ".lua"));
        printf("%s\n", testname.substr(0, testname.length() - 4).c_str());
        return false;
    }

    if (crawl_state.tests_selected.empty() && !starts_with(testname, "big/"))
        return true;
    for (const string& phrase : crawl_state.tests_selected)
    {
        if (testname == phrase || testname == phrase + ".lua")
            return true;
    }
    return false;
}

static void run_test(const string &file)
{
    if (!_is_test_selected(file))
        return;

    // halt immediately if there are HUPs. TODO: interrupt tests?
    if (crawl_state.seen_hups)
        end(0);

    ++ntests;
    if (!crawl_state.script)
        fprintf(stderr, "Running test #%d: '%s'.\n", ntests, file.c_str());
    mprf(MSGCH_DIAGNOSTICS, "Running %s %d: %s",
         activity, ntests, file.c_str());
    flush_prev_message();

    // XXX: We should probably reset more things between tests
    you.position.reset();
    you.on_current_level = true;

    const string path(catpath(crawl_state.script? script_dir : test_dir, file));
    dlua.execfile(path.c_str(), true, false);
    if (dlua.error.empty())
        ++nsuccess;
    else
        failures.emplace_back(file, dlua.error);
}

#ifdef DEBUG_TESTS
static bool _has_test(const string& test)
{
    if (crawl_state.script)
        return false;
    if (crawl_state.tests_selected.empty())
        return true;
    return crawl_state.tests_selected[0].find(test) != string::npos;
}

static void _run_test(const string &name, void (*func)())
{
    // halt immediately if there are HUPs. TODO: interrupt tests?
    if (crawl_state.seen_hups)
        end(0);
    if (crawl_state.test_list)
        return (void)printf("%s\n", name.c_str());

    if (!_has_test(name))
        return;
    if (!crawl_state.script)
        fprintf(stderr, "Running test #%d: '%s'.\n", ntests, name.c_str());

    ++ntests;
    try
    {
        (*func)();
        ++nsuccess;
    }
    catch (const ext_fail_exception &E)
    {
        failures.emplace_back(name, E.what());
    }
}
#endif

// Assumes curses has already been initialized.
void run_tests()
{
    if (crawl_state.script)
        activity = "script";

    flush_prev_message();

    run_map_global_preludes();
    run_map_local_preludes();
    _reset_test_data();

    _init_test_bindings();

#ifdef DEBUG_TESTS
    if (!crawl_state.script)
    {
        _run_test("makeitem", makeitem_tests);
        _run_test("mon-pick", debug_monpick);
        _run_test("mon-data", debug_mondata);
        _run_test("mon-spell", debug_monspells);
        _run_test("coordit", coordit_tests);
        _run_test("makename", make_name_tests);
        _run_test("job-data", debug_jobdata);
        _run_test("mon-bands", debug_bands);
        _run_test("xom-data", validate_xom_events);
        _run_test("maybe-bool", maybe_bool::test_cases);
        _run_test("fixedp", fixedp<>::test_cases);
    }
#else
    ASSERT(crawl_state.script);
#endif

    // Get a list of Lua files in test.
    {
        const string &dir = crawl_state.script ? script_dir : test_dir;
        vector<string> tests = get_dir_files_recursive(dir, ".lua");

        // Make the order consistent from one run to the next, for
        // reproducibility.
        sort(begin(tests), end(tests));

        for_each(tests.begin(), tests.end(), run_test);

        if (failures.empty() && !ntests && crawl_state.script)
        {
            failures.emplace_back("Script setup",
                    "No scripts found matching "
                    + comma_separated_line(crawl_state.tests_selected.begin(),
                                           crawl_state.tests_selected.end(),
                                           ", ", ", "));
        }
    }

#ifdef DEBUG_TAG_PROFILING
    tag_profile_out();
#endif

    if (crawl_state.test_list)
        end(0);
    cio_cleanup();
    for (const file_error &fe : failures)
        fprintf(stderr, "%s error: %s\n", activity, fe.second.c_str());

    const int code = failures.empty() ? 0 : 1;
    // scripts are responsible for printing their own errors
    if (crawl_state.script && ntests == 1)
        end(code, false);
    else
    {
        end(code, false, "%d %ss, %d succeeded, %d failed",
            ntests, activity, nsuccess, (int)failures.size());
    }
}

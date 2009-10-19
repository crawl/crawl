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

#if DEBUG_DIAGNOSTICS

#include "clua.h"
#include "dlua.h"
#include "files.h"
#include "maps.h"
#include "state.h"
#include "stuff.h"

#include <algorithm>
#include <vector>

namespace crawl_tests
{
    const std::string test_dir = "test";
    const std::string test_player_name = "Superbug99";

    int ntests = 0;
    int nsuccess = 0;

    typedef std::pair<std::string, std::string> file_error;
    std::vector<file_error> failures;

    void reset_test_data()
    {
        ntests = 0;
        nsuccess = 0;
        failures.clear();
        // XXX: Good grief, you.your_name is still not a C++ string?!
        strncpy(you.your_name, test_player_name.c_str(), kNameLen);
        you.your_name[kNameLen - 1] = 0;
    }

    int crawl_begin_test(lua_State *ls)
    {
        mprf(MSGCH_PROMPT, "Starting tests: %s", luaL_checkstring(ls, 1));
        lua_pushnumber(ls, ++ntests);
        return (1);
    }

    int crawl_test_success(lua_State *ls)
    {
        mprf(MSGCH_PROMPT, "Test success: %s", luaL_checkstring(ls, 1));
        lua_pushnumber(ls, ++nsuccess);
        return (1);
    }

    static const struct luaL_reg crawl_test_lib[] =
    {
        { "begin_test", crawl_begin_test },
        { "test_success", crawl_test_success },
        { NULL, NULL }
    };

    void init_test_bindings()
    {
        lua_stack_cleaner clean(dlua);
        luaL_openlib(dlua, "crawl", crawl_test_lib, 0);
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
        const std::string path(catpath(test_dir, file));
        dlua.execfile(path.c_str(), true, false);
        if (dlua.error.empty())
            ++nsuccess;
        else
            failures.push_back(file_error(file, dlua.error));
    }

    // Assumes curses has already been initialized.
    bool run_tests(bool exit_on_complete)
    {
        run_map_preludes();
        reset_test_data();

        init_test_bindings();

        // Get a list of Lua files in test. Order of execution of
        // tests should be irrelevant.
        const std::vector<std::string> tests(
            get_dir_files_ext(test_dir, ".lua"));
        std::for_each(tests.begin(), tests.end(),
                      run_test);

        if (exit_on_complete)
        {
            cio_cleanup();
            for (int i = 0, size = failures.size(); i < size; ++i)
            {
                const file_error &fe(failures[i]);
                fprintf(stderr, "Test error: %s\n",
                        fe.second.c_str());
            }
            const int code = failures.empty() ? 0 : 1;
            end(code, false, "%d tests, %d succeeded, %d failed",
                ntests, nsuccess, failures.size());
        }
        return (failures.empty());
    }
}

#endif // DEBUG_DIAGNOSTICS

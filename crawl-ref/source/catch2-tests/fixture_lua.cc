/// \file
/// \brief Fixture for using dlua (dungeon builder Lua) and clua commands
/// \details This is a cut-down version of the initialization of crawl, up to
/// the point of the usual lua tests in the source/test directory. See the
/// warnings for limitation of this fixture.
/// \warning The initialization of crawl can change. We are probably building
/// on a shifting foundation. Please check if a scenario meets your
/// expectations before using the scenario to test anything.
/// \warning No display. We can't run the abyss_shift.lua test.
/// \warning Assume not using tiles, android, or hint mode.
/// \warning Don't run the tests concurrently because all tests use the
/// same game state.
/// \warning This fixture probably can't reset the key binding to macros.
/// Changes in one test may persist to another test.

#include "fixture_lua.h"

namespace details_fixture_lua
{
    void _original_main(int argc, char* argv[]);

    void _init_lua_test();

/// \brief Recreate the global lua interpreters and global game states
/// \details Replace the game state and lua interpreter to ensure they are
/// new and instantiated in the right order.
/// \note Since all tests use the same game states, we can't run the tests
/// concurrently.
    static void _recreate_global_game_states()
    {
        clua = CLua(true);
        dlua = CLua(false);  // Lua interpreter for the dungeon builder.
        env = crawl_environment();
        you = player();
        crawl_state = game_state();
    }

/// \brief Adaptation of the main function in main.cc
/// \details The modifications ensure keybings & element_colours are run
/// only once across all tests.
/// \note This will try to read the init file.
    void _original_main(int argc, char* argv[])
    {
        setlocale(LC_ALL, "");
        msg::force_stderr echo(
                MB_MAYBE); // Avoid static initialization order woes
        init_crash_handler();  // Get reasons instead of core dump for crashes

        // -- Run the guarded steps once across all tests. -- //
        // Hardcoded initial keybindings.
        using namespace init_guards;
        auto& guards = get_init_guards_instance();
        if (guards[keybindings]) init_keybindings();
        // No public methods to reset the maps instantiated in color.h
        // Need this to read the maps with lua
        if (guards[element_colours]) init_element_colours();

        // System environment and basedir etc. required for dlua
        get_system_environment();
        parse_args(argc, argv, true);  // Sets up data directory for dlua.

        // Init monsters up front - needed to handle the mon_glyph option right.
        init_char_table(CSET_ASCII);
        init_monsters();

        // Init name cache. Currently unused, but item_glyph will need these
        // once implemented.
        init_properties();
        init_item_name_cache();
    }

    static void _startup_initialize()
    {
        Options.fixup_options();

        you.symbol = MONS_PLAYER;
        msg::initialise_mpr_streams();

        rng::seed(); // don't use any chosen seed yet

        clua.init_libraries();

        init_char_table(Options.char_set);
        init_show_table();
        init_monster_symbols();
        init_spell_descs();        // This needs to be way up top. {dlb}
        init_zap_index();
        init_mut_index();
        init_sac_index();
        init_duration_index();
        init_mon_name_cache();
        init_mons_spells();

        // init_item_name_cache() needs to be redone after init_char_table()
        // and init_show_table() have been called, so that the glyphs will
        // be set to use with item_names_by_glyph_cache.
        init_item_name_cache();

        unwind_bool no_more(crawl_state.show_more_prompt, false);

        // Init item array.
        for (int i = 0; i < MAX_ITEMS; ++i)
            init_item(i);

        reset_all_monsters();
        init_anon();

        env.igrid.init(NON_ITEM);
        env.mgrid.init(NON_MONSTER);
        env.map_knowledge.init(map_cell());
        env.pgrid.init(terrain_property_t{});

        you.unique_creatures.reset();
        you.unique_items.init(UNIQ_NOT_EXISTS);

        // Set up the Lua interpreter for the dungeon builder.
        init_dungeon_lua();

        // Initialise internal databases.
        databaseSystemInit();

        init_feat_desc_cache();
        init_spell_name_cache();
        init_spell_rarities();

        // Read special levels and vaults.
        read_maps();
        run_map_global_preludes();
        you.game_seed = crawl_state.seed;

        // don't run cio_init() since we don't have a screen (e.g. ncurses)
    }

/// \brief Initializations for lua
/// \details Adapted from _init_test_bindings() in ctest.cc
    void _init_lua_test()
    {
        lua_stack_cleaner clean(dlua);  // clean lua stack upon destruction
        // dlua/test.lua contains helper functions for testing
        dlua.execfile("dlua/test.lua", true, true);
        initialise_branch_depths();
        initialise_item_descriptions();
    }

    void _setup_fixture_lua()
    {
        _recreate_global_game_states();
        int argc = 1;
        char* argv[] = {(char*) "crawl", (char*) "\0"};
        _original_main(argc, argv);
        _startup_initialize();
        flush_prev_message();
        run_map_global_preludes();
        run_map_local_preludes();
        _init_lua_test();
    }

/// \brief Reset claw
/// \details Adapted from _reset_game() in main.cc
/// \warning Can't reset the keybinding to macros.
    void _teardown_fixture_lua()
    {
        crawl_state.reset_game();
        clear_message_store();
        // No public methods to reset the name_to_cmd_map and cmd_to_name_map
        // in macro.h ...
        macro_clear_buffers();

        the_lost_ones.clear();
        shopping_list = ShoppingList();
        you = player();
        reset_hud();
        StashTrack = StashTracker();
        travel_cache = TravelCache();
        // no hints mode
        clear_level_target();
        overview_clear();
        clear_message_window();
        note_list.clear();
        msg::deinitialise_mpr_streams();
    }
}

FixtureLua::FixtureLua()
{
    details_fixture_lua::_setup_fixture_lua();
}

FixtureLua::~FixtureLua()
{
    details_fixture_lua::_teardown_fixture_lua();
}

/// \brief Run dlua command and require no error
void require_execstring(const std::string& cmd, int nresults,
                        const std::string& context)
{
    const int err = dlua.execstring(cmd.c_str(), context.c_str(), nresults);
    INFO("Lua command: " << cmd.c_str());
    INFO("Lua error: " << dlua.error);
    REQUIRE(err == 0);
    REQUIRE(dlua.error == "");
}

/*
 *  File:       startup.cc
 *  Summary:    Collection of startup related functions and objects
 *  Written by:
 */

#include "AppHdr.h"

#include "arena.h"
#include "cio.h"
#include "command.h"
#include "ctest.h"
#include "initfile.h"
#include "itemname.h"
#include "items.h"
#include "database.h"
#include "defines.h"
#include "dbg-maps.h"
#include "dlua.h"
#include "dungeon.h"
#include "env.h"
#include "exclude.h"
#include "files.h"
#include "food.h"
#include "lev-pand.h"
#include "macro.h"
#include "maps.h"
#include "message.h"
#include "menu.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-cast.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-input.h"
#include "ng-init.h"
#include "ng-setup.h"
#include "notes.h"
#include "output.h"
#include "stairs.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "startup.h"
#include "state.h"
#include "stuff.h"
#include "options.h"
#include "terrain.h"
#include "tutorial.h"
#include "view.h"
#include "viewchar.h"

#include "jobs.h"

// Initialise a whole lot of stuff...
void _initialize()
{
    Options.fixup_options();

    // Read the options the player used last time they created a new
    // character.
    read_startup_prefs();

    you.symbol = '@';
    you.colour = LIGHTGREY;

    if (Options.seed)
        seed_rng(Options.seed);
    else
        seed_rng();
    get_typeid_array().init(ID_UNKNOWN_TYPE);
    init_char_table(Options.char_set);
    init_show_table();
    init_monster_symbols();
    init_spell_descs();        // This needs to be way up top. {dlb}
    init_zap_index();
    init_mut_index();
    init_mon_name_cache();
    init_mons_spells();

    // init_item_name_cache() needs to be redone after init_char_table()
    // and init_show_table() have been called, so that the glyphs will
    // be set to use with item_names_by_glyph_cache.
    init_item_name_cache();

    msg::initialise_mpr_streams();

    // Init item array.
    for (int i = 0; i < MAX_ITEMS; ++i)
        init_item(i);

    // Empty messaging string.
    info[0] = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
        menv[i].reset();

    igrd.init(NON_ITEM);
    mgrd.init(NON_MONSTER);
    env.map_knowledge.init(map_cell());
    env.pgrid.init(0);

    you.unique_creatures.init(false);
    you.unique_items.init(UNIQ_NOT_EXISTS);

    // Set up the Lua interpreter for the dungeon builder.
    init_dungeon_lua();

#ifdef USE_TILE
    // Draw the splash screen before the database gets initialised as that
    // may take awhile and it's better if the player can look at a pretty
    // screen while this happens.
    if (!crawl_state.map_stat_gen && !crawl_state.test
        && Options.tile_title_screen)
    {
        tiles.draw_title();
        tiles.update_title_msg("Loading Databases...");
    }
#endif

    // Initialise internal databases.
    databaseSystemInit();
#ifdef USE_TILE
    if (Options.tile_title_screen)
        tiles.update_title_msg("Loading Spells and Features...");
#endif

    init_feat_desc_cache();
    init_spell_name_cache();
    init_spell_rarities();
#ifdef USE_TILE
    if (Options.tile_title_screen)
        tiles.update_title_msg("Loading maps...");
#endif

    // Read special levels and vaults.
    read_maps();

    if (crawl_state.build_db)
        end(0);

    cio_init();

    // System initialisation stuff.
    textbackground(0);
#ifdef USE_TILE
    if (Options.tile_title_screen)
    {
        tiles.update_title_msg("Loading complete, press any key to start.");
        tiles.hide_title();
    }
#endif

    clrscr();

#ifdef DEBUG_DIAGNOSTICS
    if (crawl_state.map_stat_gen)
    {
        generate_map_stats();
        end(0, false);
    }
#endif

    if (crawl_state.test)
    {
#if defined(DEBUG_TESTS) && !defined(DEBUG)
#error "DEBUG must be defined if DEBUG_TESTS is defined"
#endif

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
#ifdef USE_TILE
        init_player_doll();
        tiles.initialise_items();
#endif
        crawl_state.show_more_prompt = false;
        crawl_tests::run_tests(true);
        // Superfluous, just to make it clear that this is the end of
        // the line.
        end(0, false);
#else
        end(1, false, "Non-debug Crawl cannot run tests. "
            "Please use a debug build (defined FULLDEBUG, DEBUG_DIAGNOSTIC "
            "or DEBUG_TESTS)");
#endif
    }
}

/**
 * Performs all the common steps that normal, tutorial and sprint share
 */
bool _setup_generic(const std::string& name)
{
    newgame_def ng;
    ng.name = name;
    // Defines a new game, or returns false if a save game
    // with that name exists.
    const bool newc = choose_game(&ng);
    if (newc)
        setup_game(ng);
    else
        restore_game(ng.name);

    // Fix the mutation definitions for the species we're playing.
    fixup_mutations();

    // Load macros
    macro_init();

    crawl_state.need_save = true;

    calc_hp();
    calc_mp();

    run_map_preludes();

    if (newc && you.char_direction == GDT_GAME_START)
    {
        // Chaos Knights of Lugonu start out in the Abyss.
        you.level_type  = LEVEL_ABYSS;
        you.entry_cause = EC_UNKNOWN;
    }

    load(you.entering_level ? you.transit_stair : DNGN_STONE_STAIRS_DOWN_I,
         you.entering_level ? LOAD_ENTER_LEVEL :
         newc               ? LOAD_START_GAME : LOAD_RESTART_GAME,
         NUM_LEVEL_AREA_TYPES, -1, you.where_are_you);

    if (newc && you.char_direction == GDT_GAME_START)
    {
        // Randomise colours properly for the Abyss.
        init_pandemonium();
    }

#ifdef DEBUG_DIAGNOSTICS
    // Debug compiles display a lot of "hidden" information, so we auto-wiz.
    you.wizard = true;
#endif

    init_properties();
    burden_change();

    you.redraw_stats.init(true);
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.redraw_quiver       = true;
    you.wield_change        = true;

    // Start timer on session.
    you.start_time = time(NULL);

#ifdef CLUA_BINDINGS
    clua.runhook("chk_startgame", "b", newc);
    std::string yname = you.your_name; // XXX: what's this for?
    read_init_file(true);
    Options.fixup_options();
    you.your_name = yname;

    // In case Lua changed the character set.
    init_char_table(Options.char_set);
    init_show_table();
    init_monster_symbols();
#endif

#ifdef USE_TILE
    // Override inventory weights options for tiled menus.
    if (Options.tile_menu_icons && Options.show_inventory_weights)
        Options.show_inventory_weights = false;

    init_player_doll();

    tiles.resize();
#endif

    draw_border();
    new_level();
    update_turn_count();

    // Set vision radius to player's current vision.
    set_los_radius(you.current_vision);
    init_exclusion_los();

    trackers_init_new_level(false);

    if (newc) // start a new game
    {
        you.friendly_pickup = Options.default_friendly_pickup;

        // Mark items in inventory as of unknown origin.
        origin_set_inventory(origin_set_unknown);

        // For a new game, wipe out monsters in LOS, and
        // for new tutorial games also the items.
        zap_los_monsters(Tutorial.tutorial_events[TUT_SEEN_FIRST_OBJECT]);

        // For a newly started tutorial, turn secret doors into normal ones.
        if (Tutorial.tutorial_left)
            tutorial_zap_secret_doors();
    }

#ifdef USE_TILE
    tiles.initialise_items();
    // Must re-run as the feature table wasn't initialised yet.
    TileNewLevel(newc);
#endif

    // This just puts the view up for the first turn.
    viewwindow(false);

    activate_notes(true);

    // XXX: And run Lua map postludes for D:1. Kinda hacky, it shouldn't really
    // be here.
    run_map_epilogues();

    return (newc);
}

/**
 * Special steps that normal game needs;
 */
void _setup_normal_game()
{
    make_hungry(0, true);
}

/**
 * Special steps that tutorial game needs;
 */
void _setup_tutorial()
{
    make_hungry(0, true);
}

/**
 * Special steps that arena needs;
 */
void _setup_arena()
{
    run_map_preludes();
    initialise_item_descriptions();
#ifdef USE_TILE
    tiles.initialise_items();
#endif

    run_arena();
    end(0, false);
}

/**
 * Special steps that sprint needs;
 */
void _setup_sprint()
{

}

/**
 * Helpor for show_startup_menu()
 * constructs the game modes section
 */
void _construct_game_modes_menu(MenuScroller* menu)
{
    TextItem* tmp = NULL;
    std::string text;

    tmp = new TextItem();
    text = "Dungeon Crawl";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_NORMAL);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Dungeon Crawl: The main game: full of monsters,"
                              " items, gods and danger!");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    text = "Tutorial for Dungeon Crawl";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_TUTORIAL);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Tutorial that covers the basics of Dungeon crawl"
                              " survival.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    text = "Instructions";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id('?');
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Help menu.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    text = "Dungeon Sprint";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_SPRINT);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Hard, fixed single level game mode.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    text = "The Arena";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_ARENA);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Pit computer controlled teams versus each other!");
    menu->attach_item(tmp);
    tmp->set_visible(true);
}

static void _construct_save_games_menu(MenuScroller* menu,
                       const std::vector<player_save_info>& chars)
{
    if (chars.size() == 0)
    {
        // no saves
        return;
    }

    std::string text;

    std::vector<player_save_info>::iterator it;
    for (unsigned int i = 0; i < chars.size(); ++i)
    {
#ifdef USE_TILE
        SaveMenuItem* tmp = new SaveMenuItem();
#else
        TextItem* tmp = new TextItem();
#endif
        tmp->set_text(chars.at(i).short_desc());
        tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
        tmp->set_fg_colour(WHITE);
        tmp->set_highlight_colour(WHITE);
        // unique id
        tmp->set_id(NUM_GAME_TYPE + i);
#ifdef USE_TILE
        tmp->set_doll(chars.at(i).doll);
#endif
        //tmp->set_description_text("...");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

// Should probably use some std::find invocation instead.
static int _find_save(const std::vector<player_save_info>& chars,
                      const std::string& name)
{
    for (int i = 0; i < static_cast<int>(chars.size()); ++i)
        if (chars[i].name == name)
            return (i);
    return (-1);
}

static const int SCROLLER_MARGIN_X = 18;
static const int GAME_MODES_START_Y = 7;
static const int SAVE_GAMES_START_Y = GAME_MODES_START_Y + 2 + NUM_GAME_TYPE;
static const int MISC_TEXT_START_Y = 19;
static const int GAME_MODES_WIDTH = 60;
/**
 * Setups the game mode and returns the wanted player name
 */
static std::string show_startup_menu(const std::string& default_name)
{
    clrscr();
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1, 1), coord_def(get_number_of_cols() - 1,
                   get_number_of_lines()) - 1, "freeform");
    // This freeform will only containt unfocusable texts
    freeform->allow_focus(false);
    MenuScroller* game_modes = new MenuScroller();
    game_modes->init(coord_def(SCROLLER_MARGIN_X, GAME_MODES_START_Y),
                     coord_def(GAME_MODES_WIDTH,
                     GAME_MODES_START_Y + NUM_GAME_TYPE + 1),
                     "game modes");

    MenuScroller* save_games = new MenuScroller();
    save_games->init(coord_def(SCROLLER_MARGIN_X, SAVE_GAMES_START_Y),
                     coord_def(get_number_of_cols() - 1, MISC_TEXT_START_Y - 1),
                     "save games");
    _construct_game_modes_menu(game_modes);
    std::vector<player_save_info> chars = find_saved_characters();
    _construct_save_games_menu(save_games, chars);

    NoSelectTextItem* tmp = new NoSelectTextItem();
    tmp->set_text("Enter your name:");
    tmp->set_bounds(coord_def(1, GAME_MODES_START_Y - 2),
                    coord_def(SCROLLER_MARGIN_X, GAME_MODES_START_Y - 1));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new NoSelectTextItem();
    tmp->set_text("Choices:");
    tmp->set_bounds(coord_def(1, GAME_MODES_START_Y),
                    coord_def(SCROLLER_MARGIN_X, GAME_MODES_START_Y + 1));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new NoSelectTextItem();
    tmp->set_text("Saved games:");
    tmp->set_bounds(coord_def(1, SAVE_GAMES_START_Y),
                    coord_def(SCROLLER_MARGIN_X, SAVE_GAMES_START_Y + 1));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new NoSelectTextItem();
    std::string text = "Use the up/down keys to select the type of game "
                       "or load a character.\n"
                       "You can type your name; if you leave it blank "
                       "you will be asked later.\n"
                       "Press Enter to start.\n";
    tmp->set_text(text);
    tmp->set_bounds(coord_def(1, MISC_TEXT_START_Y),
                    coord_def(get_number_of_cols() - 2, MISC_TEXT_START_Y + 4));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    menu.attach_object(freeform);
    menu.attach_object(game_modes);
    menu.attach_object(save_games);

    MenuDescriptor* descriptor = new MenuDescriptor(&menu);
    descriptor->init(coord_def(1, MISC_TEXT_START_Y + 4),
                     coord_def(get_number_of_cols() - 1, MISC_TEXT_START_Y + 5),
                     "descriptor");
    menu.attach_object(descriptor);

#ifdef USE_TILE
    // Black and White highlighter looks kinda bad on tiles
    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
#else
    BlackWhiteHighlighter* highlighter = new BlackWhiteHighlighter(&menu);
#endif
    highlighter->init(coord_def(-1, -1), coord_def(-1, -1), "highlighter");
    menu.attach_object(highlighter);

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    game_modes->set_visible(true);
    save_games->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    // Draw legal info etc
    opening_screen();

    std::string input_string = default_name;

    // If the game filled in a complete name, the user will
    // usually want to enter a new name instead of adding
    // to the current one.
    bool full_name = !input_string.empty();

    if (!chars.empty())
    {
        menu.set_active_object(save_games);
        int i = _find_save(chars, input_string);
        if (i == -1)
        {
            save_games->activate_first_item();
            input_string = chars.at(0).name;
        }
        else
        {
            save_games->set_active_item(i);
        }
    }
    else
    {
        menu.set_active_object(game_modes);
        game_modes->activate_first_item();
    }

    while (true)
    {
        menu.draw_menu();
        textcolor(WHITE);
        cgotoxy(SCROLLER_MARGIN_X ,GAME_MODES_START_Y - 2);
        clear_to_end_of_line();
        cgotoxy(SCROLLER_MARGIN_X ,GAME_MODES_START_Y - 2);
        cprintf("%s", input_string.c_str());

        int keyn = getch_ck();

        if (keyn == CK_ESCAPE)
        {
            // End the game
            end(0);
        }

        if (!menu.process_key(keyn))
        {
            // handle the non-action keys by hand to poll input
            // Only consider alphanumeric keys and -_ .
            bool changed_name = false;
            if (std::isalnum(keyn) || keyn == '-' || keyn == '.'
                || keyn == '_' || keyn == ' ')
            {
                if (full_name)
                {
                    full_name = false;
                    input_string = "";
                }
                input_string += static_cast<char> (keyn);
                changed_name = true;
            }
            else if (keyn == CK_BKSP)
            {
                if (!input_string.empty())
                {
                    if (full_name)
                        input_string = "";
                    else
                        input_string.erase(input_string.size() - 1);
                    changed_name = true;
                    full_name = false;
                }
            }
            // clear the "That's a silly name line"
            cgotoxy(SCROLLER_MARGIN_X, GAME_MODES_START_Y - 1);
            clear_to_end_of_line();

            // Depending on whether the current name occurs
            // in the saved games, update the active object.
            // We want enter to start a new game if no character
            // with the given name exists, or load the corresponding
            // game.
            if (changed_name)
            {
                int i = _find_save(chars, input_string);
                if (i == -1)
                {
                    menu.set_active_object(game_modes);
                }
                else
                {
                    menu.set_active_object(save_games);
                    save_games->set_active_item(i);
                }
            }
            else
            {
                // Menu might have changed selection -- sync name.
                int id = menu.get_active_item()->get_id();
                switch (id)
                {
                case GAME_TYPE_NORMAL:
                case GAME_TYPE_TUTORIAL:
                case GAME_TYPE_SPRINT:
                case GAME_TYPE_ARENA:
                    // If a game type is chosen, the user expects
                    // to start a new game. Just blanking the name
                    // it it clashes for now.
                    if (_find_save(chars, input_string) != -1)
                        input_string = "";
                    break;

                case '?':
                    break;

                default:
                    int save_number = id - NUM_GAME_TYPE;
                    input_string = chars.at(save_number).name;
                    full_name = true;
                    break;
                }
            }
        }
        // we had a significant action!
        std::vector<MenuItem*> selected = menu.get_selected_items();
        if (selected.size() == 0)
        {
            // Uninteresting action, poll a new key
            continue;
        }

        int id = selected.at(0)->get_id();
        switch (id)
        {
        case GAME_TYPE_NORMAL:
        case GAME_TYPE_TUTORIAL:
        case GAME_TYPE_SPRINT:
            if (is_good_name(input_string, true, false))
            {
                crawl_state.type = static_cast<game_type>(id);
                return input_string;
            }
            else
            {
                // bad name
                textcolor(RED);
                cgotoxy(SCROLLER_MARGIN_X ,GAME_MODES_START_Y - 1);
                clear_to_end_of_line();
                cprintf("That's a silly name");
            }
            continue;

        case GAME_TYPE_ARENA:
            // Do we need to set anything else? :D
            crawl_state.type = GAME_TYPE_ARENA;
            return "";

        case '?':
            list_commands();
            // recursive escape because help messes up CRTRegion
            return show_startup_menu(default_name);

        default:
            // It was a savegame instead
            int save_number = id - NUM_GAME_TYPE;
            // Return the savegame character name
            return chars.at(save_number).name;
        }
    }
    // this should never happen
    return "";
}

bool startup_step()
{
    bool new_character = false;
    std::string name;

    _initialize();

    if (!Options.game.name.empty())
        name = Options.game.name;

    // Copy name into you.your_name if set from environment --
    // note that Options.player_name could already be set from init.txt.
    // This, clearly, will overwrite such information. {dlb}
    if (!SysEnv.crawl_name.empty())
        name = SysEnv.crawl_name;

    // If a name is specified through options (command line),
    // we don't show the startup menu. It's unclear that is ideal,
    // but is how DGL installs work currently.
    // We could also check whether game type has been set here,
    // but it's probably not necessary to choose non-default game
    // types while specifying a name externally.
    if (!is_good_name(name, false, false))
    {
        if (Options.prev_game.name.length() && Options.remember_name)
            name = Options.prev_game.name;
        name = show_startup_menu(name);
    }

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
        _setup_normal_game();
        new_character = _setup_generic(name);
        break;
    case GAME_TYPE_TUTORIAL:
        _setup_tutorial();
        new_character = _setup_generic(name);
        break;
    case GAME_TYPE_SPRINT:
        _setup_sprint();
        new_character = _setup_generic(name);
        break;
    case GAME_TYPE_ARENA:
        _setup_arena();
        // this will never happen
        return false;
    default:
        ASSERT(!"Bad game type");
        end(-1);
    }

    return new_character;
}

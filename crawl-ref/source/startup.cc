/**
 * @file
 * @brief Collection of startup related functions and objects
**/

#include "AppHdr.h"

#include "startup.h"

#include "abyss.h"
#include "arena.h"
#include "branch.h"
#include "command.h"
#include "coordit.h"
#include "ctest.h"
#include "database.h"
#include "dbg-maps.h"
#include "dbg-objstat.h"
#include "dungeon.h"
#include "end.h"
#include "exclude.h"
#include "files.h"
#include "food.h"
#include "god-abil.h"
#include "god-passive.h"
#include "hints.h"
#include "initfile.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "loading-screen.h"
#include "macro.h"
#include "maps.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-input.h"
#include "ng-setup.h"
#include "notes.h"
#include "output.h"
#include "player-save-info.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "tileview.h"
#include "viewchar.h"
#include "view.h"
#ifdef USE_TILE_LOCAL
 #include "windowmanager.h"
#endif
#include "ui.h"

using namespace ui;

static void _cio_init();

// Initialise a whole lot of stuff...
static void _initialize()
{
    Options.fixup_options();

    you.symbol = MONS_PLAYER;
    msg::initialise_mpr_streams();

    seed_rng();

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

    igrd.init(NON_ITEM);
    mgrd.init(NON_MONSTER);
    env.map_knowledge.init(map_cell());
    env.pgrid.init(terrain_property_t{});

    you.unique_creatures.reset();
    you.unique_items.init(UNIQ_NOT_EXISTS);

    // Set up the Lua interpreter for the dungeon builder.
    init_dungeon_lua();

#ifdef USE_TILE_LOCAL
    // Draw the splash screen before the database gets initialised as that
    // may take awhile and it's better if the player can look at a pretty
    // screen while this happens.
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
    {
        loading_screen_open();
        loading_screen_update_msg("Loading databases...");
    }
#endif

    // Initialise internal databases.
    databaseSystemInit();
#ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_update_msg("Loading spells and features...");
#endif

    init_feat_desc_cache();
    init_spell_name_cache();
    init_spell_rarities();
#ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_update_msg("Loading maps...");
#endif

    // Read special levels and vaults.
    read_maps();
    run_map_global_preludes();

    if (crawl_state.build_db)
        end(0);

#ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_close();
#endif

    if (Options.seed)
        seed_rng(Options.seed);

#ifdef DEBUG_STATISTICS
    if (crawl_state.map_stat_gen)
    {
        release_cli_signals();
        mapstat_generate_stats();
        end(0, false);
    }
    else if (crawl_state.obj_stat_gen)
    {
        release_cli_signals();
        objstat_generate_stats();
        end(0, false);
    }
#endif

    if (!crawl_state.test_list)
    {
        if (!crawl_state.io_inited)
            _cio_init();
        clrscr();
    }

    if (crawl_state.test)
    {
#if defined(DEBUG_TESTS) && !defined(DEBUG)
#error "DEBUG must be defined if DEBUG_TESTS is defined"
#endif

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
#ifdef USE_TILE
        init_player_doll();
#endif
        dgn_reset_level();
        crawl_state.show_more_prompt = false;
        run_tests();
        // doesn't return
#else
        end(1, false, "Non-debug Crawl cannot run tests. "
            "Please use a debug build (defined FULLDEBUG, DEBUG_DIAGNOSTIC "
            "or DEBUG_TESTS)");
#endif
    }

    mpr(opening_screen().c_str());
}

/** KILL_RESETs all monsters in LOS.
*
*  Doesn't affect monsters behind glass, only those that would
*  immediately have line-of-fire.
*
*  @param items_also whether to zap items as well as monsters.
*/
static void _zap_los_monsters(bool items_also)
{
    for (radius_iterator ri(you.pos(), LOS_SOLID); ri; ++ri)
    {
        if (items_also)
        {
            int item = igrd(*ri);

            if (item != NON_ITEM && mitm[item].defined())
                destroy_item(item);
        }

        // If we ever allow starting with a friendly monster,
        // we'll have to check here.
        monster* mon = monster_at(*ri);
        if (mon == nullptr || !mons_is_threatening(*mon))
            continue;

        dprf("Dismissing %s",
             mon->name(DESC_PLAIN, true).c_str());

        // Do a hard reset so the monster's items will be discarded.
        mon->flags |= MF_HARD_RESET;
        // Do a silent, wizard-mode monster_die() just to be extra sure the
        // player sees nothing.
        monster_die(*mon, KILL_DISMISSED, NON_MONSTER, true, true);
    }
}

static void _post_init(bool newc)
{
    ASSERT(strwidth(you.your_name) <= MAX_NAME_LENGTH);

    clua.load_persist();

    // Load macros
    macro_init();

    crawl_state.need_save = true;
    crawl_state.last_type = crawl_state.type;
    crawl_state.marked_as_won = false;

    destroy_abyss();

    calc_hp();
    calc_mp();
    if (you.form != transformation::lich)
        food_change(true);
    shopping_list.refresh();

    run_map_local_preludes();

    // Abyssal Knights start out in the Abyss.
    if (newc && you.chapter == CHAPTER_POCKET_ABYSS)
        you.where_are_you = BRANCH_ABYSS;
    else if (newc)
        you.where_are_you = root_branch;

    // XXX: Any invalid level_id should do.
    level_id old_level;
    old_level.branch = NUM_BRANCHES;

    handle_terminal_resize(false); // resize HUD now that we know player
                                   // species and game mode

    load_level(you.entering_level ? you.transit_stair : DNGN_STONE_STAIRS_DOWN_I,
               you.entering_level ? LOAD_ENTER_LEVEL :
               newc               ? LOAD_START_GAME : LOAD_RESTART_GAME,
               old_level);

    if (newc && you.chapter == CHAPTER_POCKET_ABYSS)
        generate_abyss();

#ifdef DEBUG_DIAGNOSTICS
    // Debug compiles display a lot of "hidden" information, so we auto-wiz.
    you.wizard = true;
#endif
#ifdef WIZARD
    // Save-less games are pointless except for tests.
    if (Options.no_save)
        you.wizard = true;
#endif

    init_properties();

    you.redraw_stats.init(true);
    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.redraw_quiver       = true;
    you.wield_change        = true;

    // Start timer on session.
    you.last_keypress_time = chrono::system_clock::now();

#ifdef CLUA_BINDINGS
    clua.runhook("chk_startgame", "b", newc);

    read_init_file(true);
    Options.fixup_options();

    // In case Lua changed the character set.
    init_char_table(Options.char_set);
    init_show_table();
    init_monster_symbols();
#endif

#ifdef USE_TILE
    init_player_doll();

    tiles.resize();
#endif
    update_player_symbol();

    draw_border();
    new_level(!newc);
    update_turn_count();
    update_vision_range();
    you.xray_vision = !!you.duration[DUR_SCRYING];
    init_exclusion_los();
    ash_check_bondage(false);

    trackers_init_new_level(false);
    tile_new_level(newc);

    if (newc) // start a new game
    {
        // For a new game, wipe out monsters in LOS, and
        // for new hints mode games also the items.
        _zap_los_monsters(Hints.hints_events[HINT_SEEN_FIRST_OBJECT]);
    }

    // This just puts the view up for the first turn.
    you.redraw_title = true;
    textcolour(LIGHTGREY);
    you.redraw_status_lights = true;
    print_stats();
    viewwindow();

    activate_notes(true);

    // XXX: And run Lua map postludes for D:1. Kinda hacky, it shouldn't really
    // be here.
    if (newc)
        run_map_epilogues();

    // Sanitize skills, init can_train[].
    fixup_skills();
}

#ifndef DGAMELAUNCH
/**
 * Helper for show_startup_menu()
 * constructs the game modes section
 */
static void _construct_game_modes_menu(MenuScroller* menu)
{
#ifdef USE_TILE_LOCAL
    TextTileItem* tmp = nullptr;
#else
    TextItem* tmp = nullptr;
#endif
    string text;

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_NORMAL), TEX_GUI));
#else
    tmp = new TextItem();
#endif
    text = "Dungeon Crawl";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_NORMAL);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Dungeon Crawl: The main game: full of monsters, "
                              "items, gods and danger!");
    menu->attach_item(tmp);
    tmp->set_visible(true);

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_TUTORIAL), TEX_GUI));
#else
    tmp = new TextItem();
#endif
    text = "Tutorial for Dungeon Crawl";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_TUTORIAL);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Tutorial that covers the basics of "
                              "Dungeon Crawl survival.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_HINTS), TEX_GUI));
#else
    tmp = new TextItem();
#endif
    text = "Hints Mode for Dungeon Crawl";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_HINTS);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("A mostly normal game that provides more "
                              "advanced hints than the tutorial.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_SPRINT), TEX_GUI));
#else
    tmp = new TextItem();
#endif
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

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_INSTRUCTIONS), TEX_GUI));
#else
    tmp = new TextItem();
#endif
    text = "Instructions";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_INSTRUCTIONS);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("Help menu.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_ARENA), TEX_GUI));
#else
    tmp = new TextItem();
#endif
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

#ifdef USE_TILE_LOCAL
    tmp = new TextTileItem();
    tmp->add_tile(tile_def(tileidx_gametype(GAME_TYPE_HIGH_SCORES), TEX_GUI));
#else
    tmp = new TextItem();
#endif
    text = "High Scores";
    tmp->set_text(text);
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    tmp->set_id(GAME_TYPE_HIGH_SCORES);
    // Scroller does not care about x-coordinates and only cares about
    // item height obtained from max.y - min.y
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_description_text("View the high score list.");
    menu->attach_item(tmp);
    tmp->set_visible(true);
}

static void _add_newgame_button(MenuScroller* menu, int num_chars)
{
    // XXX: duplicates a lot of _construct_save_games_menu code. not ideal.
#ifdef USE_TILE_LOCAL
    SaveMenuItem* tmp = new SaveMenuItem();
#else
    TextItem* tmp = new TextItem();
#endif
    tmp->set_text("New Game");
    tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);
    // unique id
    tmp->set_id(NUM_GAME_TYPE + num_chars);
    menu->attach_item(tmp);
    tmp->set_visible(true);
}

static void _construct_save_games_menu(MenuScroller* menu,
                                       const vector<player_save_info>& chars)
{
    for (unsigned int i = 0; i < chars.size(); ++i)
    {
#ifdef USE_TILE_LOCAL
        SaveMenuItem* tmp = new SaveMenuItem();
#else
        TextItem* tmp = new TextItem();
#endif
        tmp->set_text(chars.at(i).short_desc());
        tmp->set_bounds(coord_def(1, 1), coord_def(1, 2));
        tmp->set_fg_colour(chars.at(i).save_loadable ? WHITE : RED);
        tmp->set_highlight_colour(WHITE);
        // unique id
        tmp->set_id(NUM_GAME_TYPE + i);
#ifdef USE_TILE_LOCAL
        tmp->set_doll(chars.at(i).doll);
#endif
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }

    if (!chars.empty())
        _add_newgame_button(menu, chars.size());
}

// Should probably use some find invocation instead.
static int _find_save(const vector<player_save_info>& chars, const string& name)
{
    for (int i = 0; i < static_cast<int>(chars.size()); ++i)
        if (chars[i].name == name)
            return i;
    return -1;
}

static bool _game_defined(const newgame_def& ng)
{
    return ng.type != NUM_GAME_TYPE
           && ng.species != SP_UNKNOWN
           && ng.job != JOB_UNKNOWN;
}

static const int SCROLLER_MARGIN_X  = 18;
static const int NAME_START_Y       = 5;
static const int GAME_MODES_START_Y = 7;
static const int NUM_HELP_LINES     = 3;
static const int NUM_MISC_LINES     = 5;

// TODO: should be game_type. Also, does this really need to be static?
// maybe part of crawl_state?
static int startup_menu_game_type = GAME_TYPE_UNSPECIFIED;

class UIStartupMenu : public Widget
{
public:
    UIStartupMenu(newgame_def& _ng_choice, const newgame_def &_defaults) : done(false), end_game(false), ng_choice(_ng_choice), defaults(_defaults) {
        chars = find_all_saved_characters();
        num_saves = chars.size();
        input_string = crawl_state.default_startup_name;
    };

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

    PrecisionMenu menu;
    bool done;
    bool end_game;
private:
    newgame_def& ng_choice;
    const newgame_def &defaults;
    string input_string;
    bool full_name;
    TextItem *input_text;
    vector<player_save_info> chars;
    int num_saves;
    MenuScroller *game_modes, *save_games;
    MenuDescriptor *descriptor;
};

SizeReq UIStartupMenu::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    SizeReq ret;
    if (!dim)
        ret = { 80, 90 }; // seems to work well empirically
    else
    {
        // duplicate of calculations below in _allocate_region()
        const int save_lines = num_saves + (num_saves ? 1 : 0); // add 1 for New Game
        const int help_start = GAME_MODES_START_Y + num_to_lines(save_lines + NUM_GAME_TYPE) + 2;
        const int help_end   = help_start + NUM_HELP_LINES + 1;
        ret = { 20, help_end };
    }

    const FontWrapper* font = tiles.get_crt_font();
    const int f = !dim ? font->char_width() : font->char_height();
    ret.min *= f;
    ret.nat *= f;

    return ret;
#else
    if (!dim)
        return { 80, 80 };
    else
        return { 24, 24 };
#endif
}

void UIStartupMenu::_render()
{
#ifdef USE_TILE_LOCAL
    GLW_3VF t = {(float)m_region[0], (float)m_region[1], 0}, s = {1, 1, 1};
    glmanager->set_transform(t, s);
#endif
    ui::push_scissor(m_region);
    menu.draw_menu();
    ui::pop_scissor();
#ifdef USE_TILE_LOCAL
    glmanager->reset_transform();
#endif
}

void UIStartupMenu::_allocate_region()
{
    menu.clear();

#ifdef USE_TILE_LOCAL
    const FontWrapper* font = tiles.get_crt_font();
    const int max_col = m_region[2]/font->char_width();
    const int max_line = m_region[3]/font->char_height();
#else
    const int max_col = m_region[2], max_line = m_region[3];
#endif

    const int num_modes = NUM_GAME_TYPE;

    int save_lines = num_saves + (num_saves ? 1 : 0); // add 1 for New Game
    const int help_start = min(GAME_MODES_START_Y + num_to_lines(save_lines + num_modes) + 2,
                               max_line - NUM_MISC_LINES + 1);
    const int help_end   = help_start + NUM_HELP_LINES + 1;

    const int game_mode_bottom = GAME_MODES_START_Y + num_to_lines(num_modes);
    const int game_save_top = help_start - 2 - num_to_lines(min(2, save_lines));
    const int save_games_start_y = min<int>(game_mode_bottom, game_save_top);

    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1, 1), coord_def(max_col, max_line), "freeform");
    // This freeform will only containt unfocusable texts
    freeform->allow_focus(false);
    game_modes = new MenuScroller();
    game_modes->init(coord_def(SCROLLER_MARGIN_X, GAME_MODES_START_Y),
                     coord_def(max_col, save_games_start_y),
                     "game modes");

    save_games = new MenuScroller();
    save_games->init(coord_def(SCROLLER_MARGIN_X, save_games_start_y),
                     coord_def(max_col, help_start - 1),
                     "save games");
    _construct_game_modes_menu(game_modes);
    _construct_save_games_menu(save_games, chars);

    NoSelectTextItem* tmp = new NoSelectTextItem();
    tmp->set_text("Enter your name:");
    tmp->set_bounds(coord_def(1, NAME_START_Y),
                    coord_def(SCROLLER_MARGIN_X, NAME_START_Y + 1));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new NoSelectTextItem();
    tmp->set_text("Choices:");
    tmp->set_bounds(coord_def(1, GAME_MODES_START_Y),
                    coord_def(SCROLLER_MARGIN_X, GAME_MODES_START_Y + 1));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    if (num_saves)
    {
        tmp = new NoSelectTextItem();
        tmp->set_text("Saved games:");
        tmp->set_bounds(coord_def(1, save_games_start_y),
                        coord_def(SCROLLER_MARGIN_X, save_games_start_y + 1));
        freeform->attach_item(tmp);
        tmp->set_visible(true);
    }

    tmp = new NoSelectTextItem();

    string text = "Use the up/down keys to select the type of game or load a "
                  "character.";
#ifdef USE_TILE_LOCAL
    if (tiles.is_using_small_layout())
        text += " ";
    else
#endif
        text += "\n";
    text +=       "You can type your name; if you leave it blank you will be "
                  "asked later.\n"
                  "Press Enter to start";
    // TODO: this should include a description of that character.
    if (_game_defined(defaults))
        text += ", Tab to repeat the last game's choice";
    text += ".\n";
    tmp->set_text(text);
    tmp->set_bounds(coord_def(1, help_start), coord_def(max_col - 1, help_end));
    freeform->attach_item(tmp);
    tmp->set_visible(true);

    menu.attach_object(freeform);
    menu.attach_object(game_modes);
    menu.attach_object(save_games);

    descriptor = new MenuDescriptor(&menu);
    descriptor->init(coord_def(1, help_end), coord_def(max_col, help_end + 1),
                     "descriptor");
    menu.attach_object(descriptor);

#ifdef USE_TILE_LOCAL
    // Black and White highlighter looks kinda bad on tiles
    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
#else
    BlackWhiteHighlighter* highlighter = new BlackWhiteHighlighter(&menu);
#endif
    highlighter->init(coord_def(-1, -1), coord_def(-1, -1), "highlighter");
    menu.attach_object(highlighter);

    freeform->set_visible(true);
    game_modes->set_visible(true);
    save_games->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    // Draw legal info etc
    auto intro_text = new FormattedTextItem();
    intro_text->set_text(opening_screen());
    intro_text->set_bounds(coord_def(1, 1), coord_def(max_col, 5));
    intro_text->set_visible(true);
    freeform->attach_item(intro_text);

    input_text = new TextItem();
    input_text->set_text(input_string);
    input_text->set_fg_colour(WHITE);
    input_text->set_bounds(coord_def(SCROLLER_MARGIN_X, NAME_START_Y),
            coord_def(max_col, NAME_START_Y+1));
    input_text->set_visible(true);
    freeform->attach_item(input_text);

    // If the game filled in a complete name, the user will
    // usually want to enter a new name instead of adding
    // to the current one.
    full_name = !input_string.empty();

    int save = _find_save(chars, input_string);
    // don't use non-enum game_type values across restarts, as the list of
    // saves may have changed on restart.
    if (startup_menu_game_type >= NUM_GAME_TYPE)
        startup_menu_game_type = GAME_TYPE_UNSPECIFIED;

    if (startup_menu_game_type != GAME_TYPE_UNSPECIFIED)
    {
        menu.set_active_object(game_modes);
        game_modes->set_active_item(startup_menu_game_type);
    }
    else if (save != -1)
    {
        menu.set_active_object(save_games);
        // save game id is offset by NUM_GAME_TYPE
        save_games->set_active_item(NUM_GAME_TYPE + save);
    }
    else if (defaults.type != NUM_GAME_TYPE)
    {
        menu.set_active_object(game_modes);
        game_modes->set_active_item(defaults.type);
    }
    else if (!chars.empty())
    {
        menu.set_active_object(save_games);
        save_games->activate_first_item();
    }
    else
    {
        menu.set_active_object(game_modes);
        game_modes->activate_first_item();
    }

    descriptor->render();
    if (recent_error_messages())
    {
        descriptor->override_description(
            "Errors during initialization; press ctrl-p to view.");
    }
    else
        descriptor->override_description(crawl_state.last_game_exit.message);

}

bool UIStartupMenu::on_event(const wm_event& ev)
{
#ifdef USE_TILE_LOCAL
    if (ev.type == WME_MOUSEMOTION
     || ev.type == WME_MOUSEBUTTONDOWN
     || ev.type == WME_MOUSEWHEEL)
    {
        MouseEvent mouse_ev = ev.mouse_event;
        mouse_ev.px -= m_region[0];
        mouse_ev.py -= m_region[1];

        int key = menu.handle_mouse(mouse_ev);
        if (key && key != CK_NO_KEY)
        {
            wm_event fake_key = {0};
            fake_key.type = WME_KEYDOWN;
            fake_key.key.keysym.sym = key;
            on_event(fake_key);
        }

        if (ev.type == WME_MOUSEMOTION)
            _expose();
        return true;
    }
#endif

    if (ev.type != WME_KEYDOWN)
        return false;
    int keyn = ev.key.keysym.sym;

    // XXX: these keys are effectively broken on the main menu; there's a new
    // menu implementation on the way, but until it's ready, disable them
    if (keyn == CK_PGUP || keyn == CK_PGDN)
        return true;

    _expose();

    if (key_is_escape(keyn) || keyn == CK_MOUSE_CMD)
    {
        // End the game
        return done = end_game = true;
    }
    else if (keyn == '\t' && _game_defined(defaults))
    {
        ng_choice = defaults;
        return done = true;
    }
    else if (keyn == '?')
    {
        show_help();

        // If we had a save selected, reset type so that the save
        // will continue to be selected when we restart the menu.
        MenuItem *active = menu.get_active_item();
        if (active && active->get_id() >= NUM_GAME_TYPE)
            startup_menu_game_type = GAME_TYPE_UNSPECIFIED;

        return true;
    }
    else if (keyn == CONTROL('P'))
    {
        replay_messages();
        MenuItem *active = menu.get_active_item();
        if (active && active->get_id() >= NUM_GAME_TYPE)
            startup_menu_game_type = GAME_TYPE_UNSPECIFIED;
        return true;
    }

    if (!menu.process_key(keyn))
    {
        // handle the non-action keys by hand to poll input
        // Only consider alphanumeric keys and -_ .
        bool changed_name = false;
        if (iswalnum(keyn) || keyn == '-' || keyn == '.'
            || keyn == '_' || keyn == ' ')
        {
            if (full_name)
            {
                full_name = false;
                input_string = "";
            }
            if (strwidth(input_string) < MAX_NAME_LENGTH)
            {
                input_string += stringize_glyph(keyn);
                changed_name = true;
            }
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

        // Depending on whether the current name occurs
        // in the saved games, update the active object.
        // We want enter to start a new game if no character
        // with the given name exists, or load the corresponding
        // game.
        if (changed_name)
        {
            int i = _find_save(chars, input_string);
            if (i == -1)
                menu.set_active_object(game_modes);
            else
            {
                menu.set_active_object(save_games);
                // save game ID is offset by NUM_GAME_TYPE
                save_games->set_active_item(NUM_GAME_TYPE + i);
            }
        }
        else
        {
            // Menu might have changed selection -- sync name.
            // TODO: the mapping from menu item to game_type is alarmingly
            // brittle here
            startup_menu_game_type = menu.get_active_item()->get_id();
            switch (startup_menu_game_type)
            {
            case GAME_TYPE_ARENA:
                input_string = "";
                break;
            case GAME_TYPE_NORMAL:
            case GAME_TYPE_TUTORIAL:
            case GAME_TYPE_SPRINT:
            case GAME_TYPE_HINTS:
                // If a game type is chosen, the user expects
                // to start a new game. Just blanking the name
                // it it clashes for now.
                if (_find_save(chars, input_string) != -1)
                    input_string = "";
                break;
            case GAME_TYPE_HIGH_SCORES:
                break;

            case GAME_TYPE_INSTRUCTIONS:
                break;

            default:
                int save_number = startup_menu_game_type - NUM_GAME_TYPE;
                if (save_number < num_saves)
                    input_string = chars.at(save_number).name;
                else // new game
                    input_string = "";
                full_name = true;
                break;
            }
        }
        input_text->set_text(input_string);
    }
    // we had a significant action!
    vector<MenuItem*> selected = menu.get_selected_items();
    menu.clear_selections();
    if (selected.empty())
    {
        // Uninteresting action, poll a new key
        return true;
    }

    int id = selected.at(0)->get_id();
    switch (id)
    {
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_TUTORIAL:
    case GAME_TYPE_SPRINT:
    case GAME_TYPE_HINTS:
        trim_string(input_string);
        if (is_good_name(input_string, true))
        {
            ng_choice.type = static_cast<game_type>(id);
            ng_choice.name = input_string;
            return done = true;
        }
        else
        {
            // bad name
            descriptor->override_description("That's a silly name.");
            // Don't make the next key re-enter the game.
            menu.clear_selections();
        }
        return true;

    case GAME_TYPE_ARENA:
        ng_choice.type = GAME_TYPE_ARENA;
        return done = true;

    case GAME_TYPE_INSTRUCTIONS:
        show_help();
        return true;

    case GAME_TYPE_HIGH_SCORES:
        show_hiscore_table();
        return true;

    default:
        // It was a savegame instead
        const int save_number = id - NUM_GAME_TYPE;
        if (save_number < num_saves) // actual save
        {
            // Save the savegame character name
            ng_choice.name = chars.at(save_number).name;
            ng_choice.type = chars.at(save_number).saved_game_type;
            ng_choice.filename = chars.at(save_number).filename;
        }
        else // "new game"
        {
            ng_choice.name = "";
            ng_choice.type = GAME_TYPE_NORMAL;
            ng_choice.filename = ""; // ?
        }
        return done = true;
    }
}

/**
 * Saves game mode and player name to ng_choice.
 */
static void _show_startup_menu(newgame_def& ng_choice,
                               const newgame_def& defaults)
{
    unwind_bool no_more(crawl_state.show_more_prompt, false);

#if defined(USE_TILE_LOCAL) && defined(TOUCH_UI)
    wm->show_keyboard();
#elif defined(USE_TILE_WEB)
    tiles_crt_control show_as_menu(CRT_MENU);
#endif


    auto startup_ui = make_shared<UIStartupMenu>(ng_choice, defaults);
    auto popup = make_shared<ui::Popup>(startup_ui);

    ui::run_layout(move(popup), startup_ui->done);

    if (startup_ui->end_game)
    {
#ifdef USE_TILE_WEB
        tiles.send_exit_reason("cancel");
#endif
        end(0);
    }
}
#endif

static void _choose_arena_teams(newgame_def& choice,
                                const newgame_def& defaults)
{
#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU);
#endif

    if (!choice.arena_teams.empty())
        return;
    clear_message_store();

    char buf[80];
    resumable_line_reader reader(buf, sizeof(buf));
    bool done = false, cancel;
    auto prompt_ui = make_shared<Text>();

    prompt_ui->on(Widget::slots.event, [&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        int key = ev.key.keysym.sym;
        key = reader.putkey(key);
        if (key == -1)
            return true;
        cancel = !!key;
        return done = true;
    });

    auto popup = make_shared<ui::Popup>(prompt_ui);
    ui::push_layout(move(popup));
    while (!done && !crawl_state.seen_hups)
    {
        string hlbuf = formatted_string(buf).to_colour_string();
        if (hlbuf.find(" v ") != string::npos)
            hlbuf = "<w>" + replace_all(hlbuf, " v ", "</w> v <w>") + "</w>";

        formatted_string prompt;
        prompt.cprintf("Enter your choice of teams:\n\n  ");
        prompt += formatted_string::parse_string(hlbuf);

        prompt.cprintf("\n\n");
        if (!defaults.arena_teams.empty())
            prompt.cprintf("Enter - %s\n", defaults.arena_teams.c_str());
        prompt.cprintf("\n");
        prompt.cprintf("Examples:\n");
        prompt.cprintf("  Sigmund v Jessica\n");
        prompt.cprintf("  99 orc v the Royal Jelly\n");
        prompt.cprintf("  20-headed hydra v 10 kobold ; scimitar ego:flaming");
        prompt_ui->set_text(prompt);

        ui::pump_events();
    }
    ui::pop_layout();

    if (cancel || crawl_state.seen_hups)
        game_ended(game_exit::abort);
    choice.arena_teams = buf;
    if (choice.arena_teams.empty())
        choice.arena_teams = defaults.arena_teams;
}

#ifndef DGAMELAUNCH
static bool _exit_type_allows_menu_bypass(game_exit exit)
{
    // restart with last game saved, crashed, or aborted: don't bypass
    // restart with last game died, won, or left: bypass if other settings allow
    // it. If quit, bypass only if the relevant option is set.
    // unknown corresponds to no previous game in this crawl
    // session.
    return exit == game_exit::death
        || exit == game_exit::win
        || exit == game_exit::unknown
        || exit == game_exit::leave
        || (exit == game_exit::quit && Options.newgame_after_quit);
}
#endif

bool startup_step()
{
    _initialize();

    newgame_def choice   = Options.game;

    // Setup base game type *before* reading startup prefs -- the prefs file
    // may be in a game-specific subdirectory.
    crawl_state.type = choice.type;

    newgame_def defaults = read_startup_prefs();
    if (crawl_state.default_startup_name.size() == 0)
        crawl_state.default_startup_name = defaults.name;

    // Set the crawl_state gametype to the requested game type. This must
    // be done before looking for the savegame or the startup prefs file.
    if (crawl_state.type == GAME_TYPE_UNSPECIFIED
        && defaults.type != GAME_TYPE_UNSPECIFIED)
    {
        crawl_state.type = defaults.type;
    }

    // Name from environment overwrites the one from command line.
    if (!SysEnv.crawl_name.empty())
        choice.name = SysEnv.crawl_name;


#ifndef DGAMELAUNCH

    // startup

    // These conditions are ignored for tutorial or sprint, which always trigger
    // the relevant submenu. Arena never triggers a menu.
    const bool can_bypass_menu =
            _exit_type_allows_menu_bypass(crawl_state.last_game_exit.exit_reason)
         && crawl_state.last_type != GAME_TYPE_ARENA
         && Options.name_bypasses_menu
         && is_good_name(choice.name, false);

    if (crawl_state.last_type == GAME_TYPE_TUTORIAL
        || crawl_state.last_type == GAME_TYPE_SPRINT)
    {
        // this counts as showing the startup menu
        crawl_state.bypassed_startup_menu = false;
        choice.type = crawl_state.last_type;
        crawl_state.type = crawl_state.last_type;
        crawl_state.last_type = GAME_TYPE_UNSPECIFIED;
        choice.name = defaults.name;
        if (choice.type == GAME_TYPE_TUTORIAL)
            choose_tutorial_character(choice);
    }
    else if (!can_bypass_menu && choice.type != GAME_TYPE_ARENA)
    {
        crawl_state.bypassed_startup_menu = false;
        _show_startup_menu(choice, defaults);
        // [ds] Must set game type here, or we won't be able to load
        // Sprint saves.
        crawl_state.type = choice.type;
    }
    else
        crawl_state.bypassed_startup_menu = true;
#endif

    // TODO: integrate arena better with
    //       choose_game and setup_game
    if (choice.type == GAME_TYPE_ARENA)
    {
        _choose_arena_teams(choice, defaults);
        write_newgame_options_file(choice);
        crawl_state.last_type = GAME_TYPE_ARENA;
        run_arena(choice.arena_teams); // this is NORETURN
    }

    bool newchar = false;
    newgame_def ng;
    if (choice.filename.empty() && !choice.name.empty())
        choice.filename = get_save_filename(choice.name);

    if (save_exists(choice.filename) && restore_game(choice.filename))
        save_player_name();
    else if (choose_game(ng, choice, defaults)
             && restore_game(ng.filename))
    {
        save_player_name();
    }
    else
    {
        setup_game(ng);
        newchar = true;
        write_newgame_options_file(choice);
    }
    crawl_state.default_startup_name = you.your_name;

    _post_init(newchar);

    return newchar;
}



static void _cio_init()
{
    crawl_state.io_inited = true;
    console_startup();
    set_cursor_enabled(false);
    crawl_view.init_geometry();
    textbackground(0);
}

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
#include "outer-menu.h"
#include "message.h"
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
#include "traps.h" // set_shafted
#include "viewchar.h"
#include "view.h"
#ifdef USE_TILE_LOCAL
 #include "windowmanager.h"
#endif
#include "ui.h"
#include "version.h"

using namespace ui;

static void _loading_message(string m)
{
    mpr(m.c_str());
#ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_update_msg(m.c_str());
#endif
}

// Initialise a whole lot of stuff...
static void _initialize()
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

#ifdef USE_TILE_LOCAL
    // Draw the splash screen before the database gets initialised as that
    // may take awhile and it's better if the player can look at a pretty
    // screen while this happens.
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_open();
#endif

    // Initialise internal databases.
    _loading_message("Loading databases...");
    databaseSystemInit();

    _loading_message("Loading spells and features...");
    init_feat_desc_cache();
    init_spell_name_cache();
#ifdef DEBUG
    validate_spellbooks();
#endif

    // Read special levels and vaults.
    _loading_message("Loading maps...");
    read_maps();
    run_map_global_preludes();

    if (crawl_state.build_db)
        end(0);

#ifdef USE_TILE_LOCAL
    if (!crawl_state.tiles_disabled && crawl_state.title_screen)
        loading_screen_close();
#endif

    you.game_seed = crawl_state.seed;

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
            cio_init();
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

    mpr(opening_screen().tostring().c_str());
    mpr(options_read_status().tostring().c_str());
}

/** KILL_RESETs all monsters in LOS.
*
*  Doesn't affect monsters behind glass, only those that would
*  immediately have line-of-fire.
*/
static void _zap_los_monsters()
{
    const bool items_also = Hints.hints_events[HINT_SEEN_FIRST_OBJECT];
    for (radius_iterator ri(you.pos(), LOS_SOLID); ri; ++ri)
    {
        if (items_also)
        {
            int item = env.igrid(*ri);

            if (item != NON_ITEM && env.item[item].defined())
                destroy_item(item);
        }

        monster* mon = monster_at(*ri);
        if (mon == nullptr || !mons_is_threatening(*mon) || mon->friendly())
            continue;

        dprf("Dismissing %s",
             mon->name(DESC_PLAIN, true).c_str());

        if (mons_is_or_was_unique(*mon))
        {
            if (mons_is_elven_twin(mon))
            {
                if (monster* sibling = mons_find_elven_twin_of(mon))
                {
                    sibling->flags |=MF_HARD_RESET;
                    monster_die(*sibling, KILL_DISMISSED, NON_MONSTER, true, true);
                }
            }
        }
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

    // XXX: now that the player is loaded, do a layout.
    // This is necessary to ensure that the message window is positioned, in
    // case there are any early game warning messages to be logged.
#ifdef USE_TILE
    tiles.resize();

#ifdef USE_TILE_WEB
    if (!newc)
        sync_last_milestone();
#endif
#endif

    clua.load_persist();

    // Load macros
    macro_init();

    crawl_state.need_save = crawl_state.game_started = true;
    crawl_state.last_type = crawl_state.type;
    crawl_state.marked_as_won = false;

    destroy_abyss();

    calc_hp();
    calc_mp();
    shopping_list.refresh();
    populate_sets_by_obj_type();

    run_map_local_preludes();

    if (newc)
    {
        // n.b. temple already generated in setup_game at this point
        if (Options.pregen_dungeon == level_gen_type::full
            && crawl_state.game_standard_levelgen())
        {
            pregen_dungeon(level_id(NUM_BRANCHES, -1));
        }

        you.entering_level = false;
        you.transit_stair = DNGN_UNSEEN;
        you.depth = starting_absdepth() + 1;
        you.where_are_you = root_branch;
        if (you.depth > 1)
            set_shafted();
    }

    // XXX: Any invalid level_id should do.
    level_id old_level;
    old_level.branch = NUM_BRANCHES;

#ifdef USE_TILE_WEB
    if (tiles.get_ui_state() == UI_CRT)
        tiles.set_ui_state(UI_NORMAL);
#endif

    load_level(you.entering_level ? you.transit_stair :
               you.char_class == JOB_DELVER ? DNGN_STONE_STAIRS_UP_I : DNGN_STONE_STAIRS_DOWN_I,
               you.entering_level ? LOAD_ENTER_LEVEL :
               newc               ? LOAD_START_GAME : LOAD_RESTART_GAME,
               old_level);

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
    you.redraw_noise        = true;
    you.wield_change        = true;
    you.gear_change         = true;
    quiver::set_needs_redraw();


    // Start timer on session.
    you.last_keypress_time = chrono::system_clock::now();

    // in principle everything here might be skippable if CLUA_BINDINGS is not
    // defined, but do it anyways for consistency with normal builds.
    clua.runhook("chk_startgame", "b", newc);

    read_init_file(true);
    Options.fixup_options();
    read_startup_prefs();
#ifdef USE_TILE_WEB
    tiles.send_options();
#endif

    // In case Lua changed the character set.
    init_char_table(Options.char_set);
    init_show_table();
    init_monster_symbols();

#ifdef USE_TILE
    init_player_doll();

    tiles.resize();
#endif
    update_player_symbol();

    if (newc)
        quiver::on_newchar(); // needs to happen after init file is read

    draw_border();
    new_level(!newc);
    update_turn_count();
    update_vision_range();
    init_exclusion_los();
    ash_check_bondage();
    if (you.prev_save_version != Version::Long)
        check_if_everything_is_identified();

    // XX why is this run now in addition to a related call in load_level?
    // (There this function is only called on level change, and instead
    // we run travel_init_load_level; this function is just a call to
    // travel_init_new_level, which from the comments shouldn't be run on load?)
    trackers_init_new_level();

    if (newc) // start a new game
    {
        // For a new game, wipe out monsters in LOS, and
        // for new hints mode games also the items.
        _zap_los_monsters();
    }

    // This just puts the view up for the first turn.
    you.redraw_title = true;
    you.redraw_status_lights = true;
    print_stats();
    update_screen();
    viewwindow();
    update_screen();

    activate_notes(true);

    // XXX: And run Lua map postludes for D:1. Kinda hacky, it shouldn't really
    // be here.
    if (newc)
        run_map_epilogues();

    // Sanitize skills, init can_currently_train[].
    fixup_skills();
}

#ifndef DGAMELAUNCH

struct game_modes_menu_item
{
    game_type id;
    const char *label;
    const char *description;
};

static const game_modes_menu_item entries[] =
{
    {GAME_TYPE_NORMAL, "Dungeon Crawl",
        "Dungeon Crawl: The main game: full of monsters, items, "
        "gods and danger!" },
    {GAME_TYPE_CUSTOM_SEED, "Choose Game Seed",
        "Play with a chosen custom dungeon seed." },
    {GAME_TYPE_TUTORIAL, "Tutorial for Dungeon Crawl",
        "Tutorial that covers the basics of Dungeon Crawl survival." },
    {GAME_TYPE_HINTS, "Hints Mode for Dungeon Crawl",
        "A mostly normal game that provides more advanced hints "
        "than the tutorial."},
    {GAME_TYPE_SPRINT, "Dungeon Sprint",
        "Hard, fixed single level game mode." },
    {GAME_TYPE_INSTRUCTIONS, "Instructions", "Help menu." },
    {GAME_TYPE_ARENA, "The Arena",
        "Pit computer controlled teams versus each other!" },
    {GAME_TYPE_HIGH_SCORES, "High Scores",
        "View the high score list." },
};

static void _construct_game_modes_menu(shared_ptr<OuterMenu>& container)
{
    for (unsigned int i = 0; i < ARRAYSZ(entries); ++i)
    {
        const auto& entry = entries[i];
        auto label = make_shared<Text>();

#ifdef USE_TILE_LOCAL
        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->set_cross_alignment(Widget::Align::CENTER);
        auto tile = make_shared<Image>();
        tile->set_tile(tile_def(tileidx_gametype(entry.id)));
        tile->set_margin_for_sdl(0, 6, 0, 0);
        hbox->add_child(move(tile));
        hbox->add_child(label);
#endif

        label->set_text(formatted_string(entry.label, WHITE));

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE_LOCAL
        hbox->set_margin_for_sdl(2, 10, 2, 2);
        btn->set_child(move(hbox));
#else
        btn->set_child(move(label));
#endif
        btn->id = entry.id;
        btn->description = entry.description;
        btn->highlight_colour = LIGHTGREY;
        container->add_button(move(btn), 0, i);
    }
}

static shared_ptr<MenuButton> _make_newgame_button(int num_chars)
{
    auto label = make_shared<Text>(formatted_string("New Game", WHITE));

#ifdef USE_TILE_LOCAL
    auto hbox = make_shared<Box>(Box::HORZ);
    hbox->set_cross_alignment(Widget::Align::CENTER);
    hbox->add_child(label);
    label->set_margin_for_sdl(0,0,0,TILE_Y+6);
    hbox->min_size().height = TILE_Y;
#endif

    auto btn = make_shared<MenuButton>();
#ifdef USE_TILE_LOCAL
    btn->set_child(move(hbox));
#else
    btn->set_child(move(label));
#endif
    btn->get_child()->set_margin_for_sdl(2, 10, 2, 2);
    btn->id = NUM_GAME_TYPE + num_chars;
    btn->highlight_colour = LIGHTGREY;
    return btn;
}

static void _construct_save_games_menu(shared_ptr<OuterMenu>& container,
                                       const vector<player_save_info>& chars)
{
    for (unsigned int i = 0; i < chars.size(); ++i)
    {
        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->set_cross_alignment(Widget::Align::CENTER);

#ifdef USE_TILE_LOCAL
        auto tile = make_shared<ui::PlayerDoll>(chars.at(i).doll);
        tile->set_margin_for_sdl(0, 6, 0, 0);
        hbox->add_child(move(tile));
#endif

        const COLOURS fg = chars.at(i).save_loadable ? WHITE : RED;
        auto text = chars.at(i).short_desc();
        bool wiz = strip_suffix(text, " (WIZ)");
        auto label = make_shared<Text>(formatted_string(text, fg));
        label->set_ellipsize(true);
#ifdef USE_TILE_LOCAL
        label->max_size().height = tiles.get_crt_font()->char_height();
#else
        label->max_size().height = 1;
#endif
        hbox->add_child(label);

        if (wiz)
        {
            const COLOURS wiz_bg = chars.at(i).save_loadable ? LIGHTMAGENTA : RED;
            auto wiz_text = formatted_string(" (WIZ)", wiz_bg);
            hbox->add_child(make_shared<Text>(wiz_text));
        }

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE_LOCAL
        btn->set_child(move(hbox));
#else
        btn->set_child(move(label));
#endif
        btn->get_child()->set_margin_for_sdl(2, 10, 2, 2);
        btn->id = NUM_GAME_TYPE + i;
        btn->highlight_colour = LIGHTGREY;
        container->add_button(move(btn), 0, i);
    }

    if (!chars.empty())
    {
        auto btn = _make_newgame_button(chars.size());
        container->add_button(move(btn), 0, (int)chars.size());
    }
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

class UIStartupMenu : public Widget
{
public:
    UIStartupMenu(newgame_def& _ng_choice, const newgame_def &_defaults)
                : done(false), end_game(false), ng_choice(_ng_choice),
                  defaults(_defaults),
                  selected_game_type(crawl_state.last_type)
    {
        chars = find_all_saved_characters();
        num_saves = chars.size();
        input_string = crawl_state.default_startup_name;

        m_root = make_shared<Box>(Box::VERT);
        add_internal_child(m_root);
        m_root->set_cross_alignment(Widget::Align::STRETCH);

        auto about = make_shared<Text>(opening_screen());
        about->set_margin_for_crt(0, 0, 1, 0);
        about->set_margin_for_sdl(0, 0, 10, 0);

        m_root->add_child(move(about));

        auto grid = make_shared<Grid>();
        grid->set_margin_for_crt(0, 0, 1, 0);

        auto name_prompt = make_shared<Text>("Enter your name:");
        name_prompt->set_margin_for_crt(0, 1, 1, 0);
        name_prompt->set_margin_for_sdl(0, 0, 10, 0);

        // If the game filled in a complete name, the user will
        // usually want to enter a new name instead of adding
        // to the current one.
        input_text = make_shared<Text>(formatted_string(input_string, WHITE));
        input_text->set_margin_for_crt(0, 0, 1, 0);
        input_text->set_margin_for_sdl(0, 0, 10, 10);

        grid->add_child(move(name_prompt), 0, 0);
        grid->add_child(input_text, 1, 0);

        descriptions = make_shared<Switcher>();

        auto mode_prompt = make_shared<Text>("Choices:");
        mode_prompt->set_margin_for_crt(0, 1, 1, 0);
        mode_prompt->set_margin_for_sdl(0, 0, 10, 0);
        game_modes_menu = make_shared<OuterMenu>(true, 1, ARRAYSZ(entries));
        game_modes_menu->set_margin_for_sdl(0, 0, 10, 10);
        game_modes_menu->set_margin_for_crt(0, 0, 1, 0);
        game_modes_menu->descriptions = descriptions;
        _construct_game_modes_menu(game_modes_menu);

#ifdef USE_TILE_LOCAL
        game_modes_menu->min_size().height = TILE_Y*3;
#else
        game_modes_menu->min_size().height = 2;
#endif

        grid->add_child(move(mode_prompt), 0, 1);
        grid->add_child(game_modes_menu, 1, 1);

        save_games_menu = make_shared<OuterMenu>(num_saves > 1, 1, num_saves + 1);
        if (num_saves > 0)
        {
            auto save_prompt = make_shared<Text>("Saved games:");
            save_prompt->set_margin_for_crt(0, 1, 1, 0);
            save_prompt->set_margin_for_sdl(0, 0, 10, 0);
            save_games_menu->set_margin_for_sdl(0, 0, 10, 10);
#ifdef USE_TILE_LOCAL
            save_prompt->min_size().height = TILE_Y + 2;
            save_games_menu->min_size().height = TILE_Y * min(num_saves, 3);
#else
            save_games_menu->min_size().height = 2;
#endif
            save_games_menu->descriptions = descriptions;

            _construct_save_games_menu(save_games_menu, chars);
            grid->add_child(move(save_prompt), 0, 2);
            grid->add_child(save_games_menu, 1, 2);

            game_modes_menu->linked_menus[2] = save_games_menu;
            save_games_menu->linked_menus[0] = game_modes_menu;

            grid->row_flex_grow(1) = 1000;
            grid->row_flex_grow(2) = 1;
        }

        m_root->on_activate_event([this](const ActivateEvent& event) {
            const auto button = static_pointer_cast<const MenuButton>(event.target());
            this->menu_item_activated(button->id);
            return true;
        });

        // TODO: focus events should probably not bubble, but there should be
        // some way to capture them...
        for (auto &w : game_modes_menu->get_buttons())
        {
            w->on_focusin_event([w, this](const FocusEvent&) {
                return this->on_button_focusin(*w);
            });
        }
        for (auto &w : save_games_menu->get_buttons())
        {
            w->on_focusin_event([w, this](const FocusEvent&) {
                return this->on_button_focusin(*w);
            });
        }

        grid->column_flex_grow(0) = 1;
        grid->column_flex_grow(1) = 10;

        m_root->add_child(move(grid));

        string instructions_text;
        // TODO: these can overflow on console 80x24 and won't line-wrap, is
        // there any good solution to this? e.g.
        // `long name long name the Vine Stalker Earth Elementalist`
        if (defaults.name.size() > 0 && _find_save(chars, defaults.name) != -1)
        {
            auto save = _find_save(chars, defaults.name);
            instructions_text +=
                    "<white>[tab]</white> quick-load last game: "
                    + chars[save].really_short_desc() + "\n";
        }
        else if (_game_defined(defaults))
        {
            instructions_text +=
                    "<white>[tab]</white> quick-start last combo: "
                    + (defaults.name.size() ? (defaults.name + " the ") : "")
                    + newgame_char_description(defaults) + "\n";
        }
        instructions_text +=
            "<white>[ctrl-p]</white> view rc file information and log";
        if (recent_error_messages())
            instructions_text += " (<red>Errors during initialization!</red>)";

        m_root->add_child(make_shared<Text>(
                        formatted_string::parse_string(instructions_text)));

        descriptions->set_margin_for_crt(1, 0, 0, 0);
        descriptions->set_margin_for_sdl(10, 0, 0, 0);
        m_root->add_child(descriptions);
    };

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;

    bool has_allocated = false;

    bool done;
    bool end_game;
    virtual shared_ptr<Widget> get_child_at_offset(int, int) override {
        return m_root;
    }

private:
    newgame_def& ng_choice;
    const newgame_def &defaults;
    string input_string;
    vector<player_save_info> chars;
    int num_saves;
    bool first_action = true;

    bool on_button_focusin(const MenuButton& btn)
    {
        selected_game_type = btn.id;
        switch (selected_game_type)
        {
        case GAME_TYPE_NORMAL:
        case GAME_TYPE_CUSTOM_SEED:
        case GAME_TYPE_TUTORIAL:
        case GAME_TYPE_SPRINT:
        case GAME_TYPE_HINTS:
            // If a game type is chosen, the user expects to start a new game.
            // Just blanking the name it it clashes for now.
            if (_find_save(chars, input_string) != -1)
                input_string = "";
            break;

        case GAME_TYPE_ARENA:
        case GAME_TYPE_HIGH_SCORES:
        case GAME_TYPE_INSTRUCTIONS:
            break;

        default:
            int save_number = selected_game_type - NUM_GAME_TYPE;
            if (save_number < num_saves)
                input_string = chars.at(save_number).name;
            else // new game
                input_string = "";
            break;
        }
        input_text->set_text(formatted_string(input_string, WHITE));
        return false;
    }

    void on_show();
    void menu_item_activated(int id);

    shared_ptr<Box> m_root;
    shared_ptr<Text> input_text;
    shared_ptr<Switcher> descriptions;
    shared_ptr<OuterMenu> game_modes_menu;
    shared_ptr<OuterMenu> save_games_menu;
    // not a `game_type` because it is used for save #s as well
    int selected_game_type;
};

SizeReq UIStartupMenu::_get_preferred_size(Direction dim, int prosp_width)
{
    return m_root->get_preferred_size(dim, prosp_width);
}

void UIStartupMenu::_render()
{
    m_root->render();
}

void UIStartupMenu::_allocate_region()
{
    m_root->allocate_region(m_region);

    if (!has_allocated)
    {
        has_allocated = true;
        on_show();
    }
}

void UIStartupMenu::on_show()
{
    int save = _find_save(chars, input_string);
    // don't use non-enum game_type values across restarts, as the list of
    // saves may have changed on restart.
    if (selected_game_type >= NUM_GAME_TYPE)
        selected_game_type = GAME_TYPE_UNSPECIFIED;

    int id;
    if (selected_game_type != GAME_TYPE_UNSPECIFIED)
        id = selected_game_type;
    else if (save != -1)
    {
        // save game id is offset by NUM_GAME_TYPE
        id = NUM_GAME_TYPE + save;
    }
    else if (defaults.type != NUM_GAME_TYPE)
        id = defaults.type;
    else if (!chars.empty())
        id = NUM_GAME_TYPE + 0;
    else
        id = 0;

    if (auto focus = game_modes_menu->get_button_by_id(id))
        game_modes_menu->scroll_button_into_view(focus);
    else if (auto focus2 = save_games_menu->get_button_by_id(id))
        save_games_menu->scroll_button_into_view(focus2);

    on_hotkey_event([this](const KeyEvent& ev) {
        const auto keyn = ev.key();
        bool changed_name = false;

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
            menu_item_activated(GAME_TYPE_INSTRUCTIONS);
            return true;
        }
        else if (keyn == CONTROL('P'))
        {
            replay_messages_during_startup();
            return true;
        }
        else if (keyn == '*')
        {
            input_string = newgame_random_name();
            changed_name = true;
        }
        else if (keyn == CONTROL('U'))
        {
            input_string = "";
            changed_name = true;
        }

        if (keyn == ' ' && first_action)
        {
            first_action = false;
            input_string = "";
            changed_name = true;
        }
        // handle the non-action keys by hand to poll input
        // Only consider alphanumeric keys and -_ .
        else if (iswalnum(keyn) || keyn == '-' || keyn == '.'
            || keyn == '_' || keyn == ' ')
        {
            first_action = false;
            if (strwidth(input_string) < MAX_NAME_LENGTH)
            {
                input_string += stringize_glyph(keyn);
                changed_name = true;
            }
        }
        else if (keyn == CK_BKSP)
        {
            if (first_action)
            {
                first_action = false;
                input_string = "";
                changed_name = true;
            }
            if (!input_string.empty())
            {
                input_string.erase(input_string.size() - 1);
                changed_name = true;
            }
        }

        if (!changed_name)
            return false;

        input_text->set_text(formatted_string(input_string, WHITE));

        // Depending on whether the current name occurs
        // in the saved games, update the active object.
        // We want enter to start a new game if no character
        // with the given name exists, or load the corresponding
        // game.
        int i = _find_save(chars, input_string);
        auto menu = i == -1 ? game_modes_menu : save_games_menu;
        auto btn = menu->get_button(0, i == -1 ? 0 : i);
        menu->scroll_button_into_view(btn);
        return true;
    });
}

void UIStartupMenu::menu_item_activated(int id)
{
    switch (id)
    {
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_CUSTOM_SEED:
    case GAME_TYPE_TUTORIAL:
    case GAME_TYPE_SPRINT:
    case GAME_TYPE_HINTS:
        trim_string(input_string);
        // XXX: is it ever not a good name?
        if (is_good_name(input_string, true))
        {
            ng_choice.type = static_cast<game_type>(id);
            ng_choice.name = input_string;
            done = true;
        }
        return;

    case GAME_TYPE_ARENA:
        ng_choice.type = GAME_TYPE_ARENA;
        done = true;
        return;

    case GAME_TYPE_INSTRUCTIONS:
        show_help();
        return;

    case GAME_TYPE_HIGH_SCORES:
        show_hiscore_table();
        return;

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
        done = true;
        return;
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
    tiles_crt_popup show_as_popup;
#endif


    auto startup_ui = make_shared<UIStartupMenu>(ng_choice, defaults);
    auto popup = make_shared<ui::Popup>(startup_ui);

    ui::run_layout(move(popup), startup_ui->done);

    if (startup_ui->end_game || crawl_state.seen_hups)
    {
#ifdef USE_TILE_WEB
        tiles.send_exit_reason("cancel");
#endif
        end(0);
    }
}
#endif

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
    if (crawl_state.default_startup_name.size() == 0 && Options.remember_name)
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

    if (recent_error_messages() && !Options.suppress_startup_errors)
        replay_messages_during_startup();

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
        crawl_state.last_type = GAME_TYPE_ARENA;
        run_arena(choice, defaults.arena_teams); // this is NORETURN
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
        clear_message_store();
        setup_game(ng);
        newchar = true;
        choice.seed = Options.seed; // kind of ugly, but may be changed during
                                    // setup_game.
        write_newgame_options_file(choice);
    }
    if (Options.remember_name)
        crawl_state.default_startup_name = you.your_name;

    _post_init(newchar);

    return newchar;
}



void cio_init()
{
    crawl_state.io_inited = true;
    console_startup();
    set_cursor_enabled(false);
    crawl_view.init_geometry();
    textbackground(0);
}

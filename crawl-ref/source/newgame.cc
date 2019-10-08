/**
 * @file
 * @brief Functions used when starting a new game.
**/

#include "AppHdr.h"

#include "newgame.h"

#include "cio.h"
#include "command.h"
#include "database.h"
#include "end.h"
#include "english.h"
#include "files.h"
#include "filter-enum.h"
#include "hints.h"
#include "initfile.h"
#include "item-name.h" // make_name
#include "item-prop.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "maps.h"
#include "menu.h"
#include "ng-input.h"
#include "ng-restr.h"
#include "options.h"
#include "prompt.h"
#include "skills.h"
#include "species-groups.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
#include "tilereg-crt.h"
#include "tilepick.h"
#include "tilepick-p.h"
#include "tilefont.h"
#include "tiledef-main.h"
#include "tiledef-feat.h"
#endif
#include "version.h"
#include "ui.h"
#include "outer-menu.h"

using namespace ui;

static void _choose_gamemode_map(newgame_def& ng, newgame_def& ng_choice,
                                 const newgame_def& defaults);
static bool _choose_weapon(newgame_def& ng, newgame_def& ng_choice,
                          const newgame_def& defaults);

#ifdef USE_TILE_LOCAL
#  define STARTUP_HIGHLIGHT_NORMAL LIGHTGRAY
#  define STARTUP_HIGHLIGHT_BAD LIGHTGRAY
#  define STARTUP_HIGHLIGHT_CONTROL LIGHTGRAY
#  define STARTUP_HIGHLIGHT_GOOD LIGHTGREEN
#else
#  define STARTUP_HIGHLIGHT_NORMAL BLUE
#  define STARTUP_HIGHLIGHT_BAD LIGHTGRAY
#  define STARTUP_HIGHLIGHT_CONTROL BLUE
#  define STARTUP_HIGHLIGHT_GOOD GREEN
#endif

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

newgame_def::newgame_def()
    : name(), type(GAME_TYPE_NORMAL),
      seed(0), pregenerate(false),
      species(SP_UNKNOWN), job(JOB_UNKNOWN),
      weapon(WPN_UNKNOWN),
      fully_random(false)
{
}

void newgame_def::clear_character()
{
    species  = SP_UNKNOWN;
    job      = JOB_UNKNOWN;
    weapon   = WPN_UNKNOWN;
}

enum MenuOptions
{
    M_QUIT = -1,
    M_ABORT = -2,
    M_APTITUDES  = -3,
    M_HELP = -4,
    M_VIABLE = -5,
    M_RANDOM = -6,
    M_VIABLE_CHAR = -7,
    M_RANDOM_CHAR = -8,
    M_DEFAULT_CHOICE = -9,
};

enum MenuChoices
{
    C_SPECIES = 0,
    C_JOB = 1,
};

enum MenuItemStatuses
{
    ITEM_STATUS_UNKNOWN,
    ITEM_STATUS_RESTRICTED,
    ITEM_STATUS_ALLOWED
};

static bool _is_random_species(species_type sp)
{
    return sp == SP_RANDOM || sp == SP_VIABLE;
}

static bool _is_random_job(job_type job)
{
    return job == JOB_RANDOM || job == JOB_VIABLE;
}

static bool _is_random_choice(const newgame_def& choice)
{
    return _is_random_species(choice.species)
           && _is_random_job(choice.job);
}

static bool _is_random_viable_choice(const newgame_def& choice)
{
    return _is_random_choice(choice) &&
           (choice.job == JOB_VIABLE || choice.species == SP_VIABLE);
}

static bool _char_defined(const newgame_def& ng)
{
    return ng.species != SP_UNKNOWN && ng.job != JOB_UNKNOWN;
}

string newgame_char_description(const newgame_def& ng)
{
    if (_is_random_viable_choice(ng))
        return "Recommended character";
    else if (_is_random_choice(ng))
        return "Random character";
    else if (_is_random_job(ng.job))
    {
        const string j = (ng.job == JOB_RANDOM ? "Random " : "Recommended ");
        return j + species_name(ng.species);
    }
    else if (_is_random_species(ng.species))
    {
        const string s = (ng.species == SP_RANDOM ? "Random " : "Recommended ");
        return s + get_job_name(ng.job);
    }
    else
        return species_name(ng.species) + " " + get_job_name(ng.job);
}

static string _welcome(const newgame_def& ng)
{
    string text;
    if (ng.species != SP_UNKNOWN)
        text = species_name(ng.species);
    if (ng.job != JOB_UNKNOWN)
    {
        if (!text.empty())
            text += " ";
        text += get_job_name(ng.job);
    }
    if (!ng.name.empty())
    {
        if (!text.empty())
            text = " the " + text;
        text = ng.name + text;
    }
    else if (!text.empty())
        text = "unnamed " + text;
    if (!text.empty())
        text = ", " + text;
    text = "Welcome" + text + ".";
    return text;
}

void choose_tutorial_character(newgame_def& ng_choice)
{
    ng_choice.species = SP_HUMAN;
    ng_choice.job = JOB_FIGHTER;
    ng_choice.weapon = WPN_FLAIL;
}

static void _resolve_species(newgame_def& ng, const newgame_def& ng_choice)
{
    // Don't overwrite existing species.
    if (ng.species != SP_UNKNOWN)
        return;

    switch (ng_choice.species)
    {
    case SP_UNKNOWN:
        ng.species = SP_UNKNOWN;
        return;

    case SP_VIABLE:
    case SP_RANDOM:
    {
        vector<species_type> candidate_species;
        if (is_starting_job(ng.job))
        {
            switch (ng_choice.species)
            {
            case SP_VIABLE:
                candidate_species =
                    filter_enum(NUM_SPECIES, [&](species_type species) {
                        return is_starting_species(species)
                            && job_recommends_species(ng.job, species);
                    });
                // Note: we know the array has at least one element because all
                // starting jobs must recommend at least one species.
                ASSERT(candidate_species.size() > 0);
                break;
            case SP_RANDOM:
                candidate_species =
                    filter_enum(NUM_SPECIES, [&](species_type species) {
                        return is_starting_species(species)
                            && character_is_allowed(species, ng.job);
                    });
                if (candidate_species.size() == 0)
                    die("Selected job allows no species?!");
                break;

            default:
                die("How did we get here?");
            }
            ng.species = candidate_species[random2(candidate_species.size())];
        }
        else
            ng.species = random_starting_species();
        return;
    }

    default:
        ng.species = ng_choice.species;
        return;
    }
}

static void _resolve_job(newgame_def& ng, const newgame_def& ng_choice)
{
    if (ng.job != JOB_UNKNOWN)
        return;

    switch (ng_choice.job)
    {
    case JOB_UNKNOWN:
        ng.job = JOB_UNKNOWN;
        return;

    case JOB_VIABLE:
    case JOB_RANDOM:
    {
        vector<job_type> candidate_jobs;
        if (is_starting_species(ng.species))
        {
            switch (ng_choice.job)
            {
            case JOB_VIABLE:
                candidate_jobs =
                    filter_enum(NUM_JOBS, [&](job_type job) {
                        return is_starting_job(job)
                            && species_recommends_job(ng.species, job);
                    });
                // Note: we know the array has at least one element because all
                // starting species must recommend at least one job.
                ASSERT(candidate_jobs.size() > 0);
                break;
            case JOB_RANDOM:
                candidate_jobs =
                    filter_enum(NUM_JOBS, [&](job_type job) {
                        return is_starting_job(job)
                            && character_is_allowed(ng.species, job);
                    });
                if (candidate_jobs.size() == 0)
                    die("Selected species allows no jobs?!");
                break;

            default:
                die("How did we get here?");
            }

            ng.job = candidate_jobs[random2(candidate_jobs.size())];
        }
        else
            ng.job = random_starting_job();
        return;
    }

    default:
        ng.job = ng_choice.job;
        return;
    }
}

static void _resolve_species_job(newgame_def& ng, const newgame_def& ng_choice)
{
    // Since recommendations are no longer bidirectional, pick one of
    // species or job to start. If one but not the other was specified
    // as "viable", always choose that one last; otherwise use a random
    // order.
    const bool spfirst  = ng_choice.species != SP_VIABLE
                          && ng_choice.job == JOB_VIABLE;
    const bool jobfirst = ng_choice.species == SP_VIABLE
                          && ng_choice.job != JOB_VIABLE;
    if (spfirst || !jobfirst && coinflip())
    {
        _resolve_species(ng, ng_choice);
        _resolve_job(ng, ng_choice);
    }
    else
    {
        _resolve_job(ng, ng_choice);
        _resolve_species(ng, ng_choice);
    }
}

static string _highlight_pattern(const newgame_def& ng)
{
    if (ng.species != SP_UNKNOWN)
        return species_name(ng.species) + "  ";

    if (ng.job == JOB_UNKNOWN)
        return "";

    string ret;
    species_type temp;
    for (int sp = 0; sp < NUM_SPECIES; ++sp)
    {
        temp = static_cast<species_type>(sp);
        if (is_good_combination(temp, ng.job, false, true))
            ret += species_name(temp) + "  |";
    }

    if (!ret.empty())
        ret.resize(ret.size() - 1);
    return ret;
}

static void _prompt_choice(int choice_type, newgame_def& ng, newgame_def& ng_choice,
                        const newgame_def& defaults);

static void _choose_species_job(newgame_def& ng, newgame_def& ng_choice,
                                const newgame_def& defaults)
{
    _resolve_species_job(ng, ng_choice);

    while (ng_choice.species == SP_UNKNOWN || ng_choice.job == JOB_UNKNOWN)
    {
        // Slightly non-obvious behaviour here is due to the fact that
        // both types of _prompt_choice can ask for an entirely
        // random character to be rolled. They will reset relevant fields
        // in ng for this purpose.
        if (ng_choice.species == SP_UNKNOWN)
            _prompt_choice(C_SPECIES, ng, ng_choice, defaults);
        _resolve_species_job(ng, ng_choice);
        if (ng_choice.job == JOB_UNKNOWN)
            _prompt_choice(C_JOB, ng, ng_choice, defaults);
        _resolve_species_job(ng, ng_choice);
    }

    if (!job_allowed(ng.species, ng.job))
    {
        // Either an invalid combination was passed in through options,
        // or we messed up.
        end(1, false, "Incompatible species and background (%s) selected.",
                                newgame_char_description(ng).c_str());
    }
}

// For completely random combinations (!, #, or Options.game.fully_random)
// reroll characters until the player accepts one of them or quits.
static bool _reroll_random(newgame_def& ng)
{
    string specs = chop_string(species_name(ng.species), 79, false);

    formatted_string prompt;
    prompt.cprintf("You are a%s %s %s.",
            (is_vowel(specs[0])) ? "n" : "", specs.c_str(),
            get_job_name(ng.job));

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    dolls_data doll;
    fill_doll_for_newgame(doll, ng);
#ifdef USE_TILE_LOCAL
    auto tile = make_shared<ui::PlayerDoll>(doll);
    tile->set_margin_for_sdl({0, 10, 0, 0});
    title_hbox->add_child(move(tile));
#endif
#endif
    title_hbox->add_child(make_shared<Text>(prompt));
    title_hbox->align_items = Widget::CENTER;
    title_hbox->set_margin_for_sdl({0, 0, 20, 0});
    title_hbox->set_margin_for_crt({0, 0, 1, 0});

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->add_child(move(title_hbox));
    vbox->add_child(make_shared<Text>("Do you want to play this combination? [Y/n/q]"));
    auto popup = make_shared<ui::Popup>(move(vbox));

    bool done = false;
    char c;
    popup->on(Widget::slots.event, [&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        c = ev.key.keysym.sym;
        return done = true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("prompt", prompt.to_colour_string());
    tiles.send_doll(doll, false, false);
    tiles.push_ui_layout("newgame-random-combo", 0);
#endif
    ui::run_layout(move(popup), done);
#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif

    if (key_is_escape(c) || toalower(c) == 'q' || crawl_state.seen_hups)
        game_ended(game_exit::abort);
    return toalower(c) == 'n' || c == '\t' || c == '!' || c == '#';
}

static void _choose_char(newgame_def& ng, newgame_def& choice,
                         newgame_def defaults)
{
    const newgame_def ng_reset = ng;

    if (ng.type == GAME_TYPE_TUTORIAL)
    {
        choose_tutorial_character(choice);
        choice.allowed_jobs.clear();
        choice.allowed_species.clear();
        choice.allowed_weapons.clear();
    }
    else if (ng.type == GAME_TYPE_HINTS)
    {
        pick_hints(choice);
        choice.allowed_jobs.clear();
        choice.allowed_species.clear();
        choice.allowed_weapons.clear();
    }

#if defined(DGAMELAUNCH) && defined(TOURNEY)
    // Apologies to non-public servers.
    if (ng.type == GAME_TYPE_NORMAL)
    {
        if (!yesno("Trunk games don't count for the tournament, you want "
                   TOURNEY ". Play trunk anyway? (Y/N)", false, 'n'))
        {
            game_ended(game_exit::abort);
        }
    }
#endif

    while (true)
    {
        if (choice.allowed_combos.size())
        {
            choice.species = SP_UNKNOWN;
            choice.job = JOB_UNKNOWN;
            choice.weapon = WPN_UNKNOWN;
            string combo =
                choice.allowed_combos[random2(choice.allowed_combos.size())];

            vector<string> parts = split_string(".", combo);
            if (parts.size() > 0)
            {
                string character = trim_string(parts[0]);

                if (character.length() == 4)
                {
                    choice.species =
                        get_species_by_abbrev(
                            character.substr(0, 2).c_str());
                    choice.job =
                        get_job_by_abbrev(
                            character.substr(2, 2).c_str());
                }
                else
                {
                    species_type sp = SP_UNKNOWN;;
                    // XXX: This is awkward; find a better way?
                    for (int i = 0; i < NUM_SPECIES; ++i)
                    {
                        sp = static_cast<species_type>(i);
                        if (character.length() >= species_name(sp).length()
                            && character.substr(0, species_name(sp).length())
                               == species_name(sp))
                        {
                            choice.species = sp;
                            break;
                        }
                    }
                    if (sp != SP_UNKNOWN)
                    {
                        string temp =
                            character.substr(species_name(sp).length());
                        choice.job = str_to_job(trim_string(temp));
                    }
                }

                if (parts.size() > 1)
                {
                    string weapon = trim_string(parts[1]);
                    choice.weapon = str_to_weapon(weapon);
                }
            }
        }
        else
        {
            if (choice.allowed_species.size())
            {
                choice.species =
                    choice.allowed_species[
                        random2(choice.allowed_species.size())];
            }
            if (choice.allowed_jobs.size())
            {
                choice.job =
                    choice.allowed_jobs[random2(choice.allowed_jobs.size())];
            }
            if (choice.allowed_weapons.size())
            {
                choice.weapon =
                    choice.allowed_weapons[
                        random2(choice.allowed_weapons.size())];
            }
        }

        _choose_species_job(ng, choice, defaults);

        if (choice.fully_random && _reroll_random(ng))
        {
            ng = ng_reset;
            continue;
        }

        if (_choose_weapon(ng, choice, defaults))
        {
            // We're done!
            return;
        }

        // Else choose again, name and type stays same.
        defaults = choice;
        ng = ng_reset;
        choice = ng_reset;
    }
}

static void _add_menu_sub_item(shared_ptr<OuterMenu>& menu, int x, int y, const string& text, const string& description, char letter, int id)
{
    auto tmp = make_shared<Text>();
    tmp->set_text(formatted_string(text, BROWN));
    tmp->set_margin_for_sdl({4,8,4,8});
    tmp->set_margin_for_crt({0,2,0,0});

    auto btn = make_shared<MenuButton>();
    btn->set_child(move(tmp));
    btn->id = id;
    btn->description = description;
    btn->hotkey = letter;
    btn->highlight_colour = STARTUP_HIGHLIGHT_CONTROL;
    menu->add_button(move(btn), x, y);
}

#ifndef DGAMELAUNCH
/**
 * Attempt to generate a random name for a character that doesn't collide with
 * an existing save name.
 *
 * @return  A random name, or the empty string if no good name could be
 *          generated after several tries.
 */
static string _random_name()
{
    for (int i = 0; i < 100; ++i)
    {
        const string name = make_name();
        const string filename = get_save_filename(name);
        if (!save_exists(filename))
            return name;
    }

    return "";
}

static void _choose_name(newgame_def& ng, newgame_def& choice)
{
    char buf[MAX_NAME_LENGTH + 1]; // FIXME: make line_reader handle widths
    buf[0] = '\0';
    resumable_line_reader reader(buf, sizeof(buf));

    bool done = false;
    bool overwrite_prompt = false;
    bool good_name = true;
    bool cancel = false;

    string specs = chop_string(species_name(ng.species), 79, false);

    formatted_string title;
    title.cprintf("You are a%s %s %s.",
            (is_vowel(specs[0])) ? "n" : "", specs.c_str(),
            get_job_name(ng.job));

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    dolls_data doll;
    fill_doll_for_newgame(doll, ng);
#ifdef USE_TILE_LOCAL
    auto tile = make_shared<ui::PlayerDoll>(doll);
    tile->set_margin_for_sdl({0, 10, 0, 0});
    title_hbox->add_child(move(tile));
#endif
#endif
    title_hbox->add_child(make_shared<Text>(title));
    title_hbox->align_items = Widget::CENTER;
    title_hbox->set_margin_for_sdl({0, 0, 20, 0});
    title_hbox->set_margin_for_crt({0, 0, 1, 0});

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->add_child(move(title_hbox));
    auto prompt_ui = make_shared<Text>();
    vbox->add_child(prompt_ui);

    auto sub_items = make_shared<OuterMenu>(false, 3, 1);
    vbox->add_child(sub_items);

    _add_menu_sub_item(sub_items, 0, 0,
            "Esc - Quit", "", CK_ESCAPE, CK_ESCAPE);
    _add_menu_sub_item(sub_items, 1, 0,
            "* - Random name", "", '*', '*');

    auto ok_switcher = make_shared<Switcher>();
    ok_switcher->align_y = Widget::STRETCH;
    {
        auto tmp = make_shared<Text>();
        tmp->set_text(formatted_string("Enter - Begin!", BROWN));

        auto btn = make_shared<MenuButton>();
        tmp->set_margin_for_sdl({4,8,4,8});
        tmp->set_margin_for_crt({0,2,0,0});
        btn->set_child(move(tmp));
        btn->id = CK_ENTER;
        btn->description = "";
        btn->hotkey = CK_ENTER;
        btn->highlight_colour = STARTUP_HIGHLIGHT_CONTROL;

        auto err = make_shared<Text>(
                formatted_string("That's a silly name!", LIGHTRED));
        err->set_margin_for_sdl({0,0,0,10});
        auto box = make_shared<Box>(Box::HORZ);
        box->align_items = Widget::CENTER;
        box->add_child(err);

        ok_switcher->add_child(btn);
        ok_switcher->add_child(box);
        sub_items->add_button(btn, 2, 0);
    }

    auto popup = make_shared<ui::Popup>(move(vbox));

    sub_items->on_button_activated = [&](int id) {
        switch (id)
        {
            case '*':
                reader.putkey(CK_END);
                reader.putkey(CONTROL('U'));
                for (char ch : _random_name())
                    reader.putkey(ch);
                return;
            case CK_ENTER:
                choice.name = buf;
                trim_string(choice.name);
                if (choice.name.empty())
                    choice.name = _random_name();
                if (good_name)
                {
                    ng.name = choice.name;
                    ng.filename = get_save_filename(choice.name);
                    overwrite_prompt = save_exists(ng.filename);
                    if (!overwrite_prompt)
                        done = true;
                }
                return;
            case CK_ESCAPE:
                done = cancel = true;
                return;
        }

    };

    popup->on(Widget::slots.event, [&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        int key = ev.key.keysym.sym;

        if (!overwrite_prompt)
        {
            key = reader.putkey(key);
            good_name = is_good_name(buf, true);
            ok_switcher->current() = good_name ? 0 : 1;
        }
        else
        {
            overwrite_prompt = false;
            if (key == 'Y')
                return done = true;
        }
        return true;
    });

    ui::push_layout(move(popup));
    ui::set_focused_widget(sub_items.get());
    while (!done && !crawl_state.seen_hups)
    {
        formatted_string prompt;
        prompt.textcolour(CYAN);
        prompt.cprintf("What is your name today? ");
        prompt.textcolour(LIGHTGREY);
        prompt.cprintf("%s\n", buf);
        prompt.textcolour(LIGHTRED);
        if (overwrite_prompt)
            prompt.cprintf("You have an existing game under this name; really overwrite? [Y/n]");
        prompt_ui->set_text(prompt);

        ui::pump_events();
    }
    ui::pop_layout();

    if (cancel || crawl_state.seen_hups)
        game_ended(game_exit::abort);
}

#endif

static keyfun_action _keyfun_seed_input(int &ch)
{
    if (ch == CONTROL('K') || ch == CONTROL('D') || ch == CONTROL('W') ||
            ch == CONTROL('U') || ch == CONTROL('A') || ch == CONTROL('E') ||
            ch == CK_ENTER || ch == CK_BKSP || ch == CK_ESCAPE ||
            ch < 0 || // this should get all other special keys
            isadigit(ch))
    {
        return KEYFUN_PROCESS;
    }
    return KEYFUN_IGNORE;
}

static void _choose_seed(newgame_def& ng, newgame_def& choice,
    const newgame_def& defaults)
{
    char buf[21]; // max unsigned 64 bit integer is 20 chars in decimal,
                  // specifically 18446744073709551615
    buf[0] = '\0';
    resumable_line_reader reader(buf, sizeof(buf));
    if (Options.seed)
        choice.seed = Options.seed;
    else if (Options.seed_from_rc)
        choice.seed = Options.seed_from_rc;
    reader.set_text(make_stringf("%" PRIu64, choice.seed));
    reader.set_keyproc(_keyfun_seed_input);
    const bool show_pregen_toggle =
#ifdef DEBUG
                        true;
#else
                        false;
#endif

    bool done = false;
    bool cancel = false;
    choice.pregenerate = Options.pregen_dungeon;

    auto prompt_ui = make_shared<Text>();
    prompt_ui->on(Widget::slots.event, [&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        int key = ev.key.keysym.sym;

        if (show_pregen_toggle && key == CONTROL('I'))
        {
            choice.pregenerate = !choice.pregenerate;
            return done = false;
        }
        else if (key == 'd' || key == 'D')
        {
            time_t now;
            struct tm * timeinfo;

            time(&now);
            timeinfo = localtime(&now);
            char timebuf[9];
            strftime(timebuf, sizeof(timebuf), "%Y%m%d", timeinfo);
            reader.set_text(timebuf);
            return done = false;
        }
        else if (key == '?')
            show_help('D', "Seeded play"); // TODO: scroll to section
        else if (key == '-')
        {
            reader.set_text("");
            return done = false;
        }
#ifdef USE_TILE_LOCAL
        else if ((key == 'p' || key == 'P') && wm->has_clipboard())
        {
            string clip = wm->get_clipboard();
            // try to avoid pasting in crazy garbage, by parsing as uint64
            // this could be done better
            uint64_t clip_seed;
            int clip_found = sscanf(clip.c_str(), "%" SCNu64, &clip_seed);
            if (clip_found)
                reader.set_text(make_stringf("%" PRIu64, clip_seed));
            return done = false;
        }
#endif
        // TODO: digits only
        key = reader.putkey(key);

        if (key != -1)
        {
            if (key_is_escape(key))
                return done = cancel = true;
            if (reader.get_text().size() > 0)
                return done = true;
        }
        return true;
    });

    auto box = make_shared<ui::Box>(ui::Widget::VERT);
    box->add_child(prompt_ui);
    auto pregen_choice = make_shared<ui::Text>("");
    box->add_child(pregen_choice);
    if (show_pregen_toggle)
    {
        pregen_choice->set_text(
            "Pregenerate the dungeon ([tab] to switch)? Yes | No");
    }

    auto popup = make_shared<ui::Popup>(box);
    ui::push_layout(move(popup));
    ui::set_focused_widget(prompt_ui.get());
    while (!done && !crawl_state.seen_hups)
    {
        formatted_string prompt;
        prompt.textcolour(CYAN);
        prompt.cprintf("Play a game with a custom seed for version %s.\n\n",
            Version::Long);
        prompt.textcolour(LIGHTGREY);
        prompt.cprintf(
            "Choose 0 for a random seed. Press [d] for today's daily seed.\n"
#ifdef USE_TILE_LOCAL
            "Press [p] to paste a seed from the clipboard (overwriting the\n"
            "current value).\n"
#endif
            "\n");
        prompt.cprintf("Seed ([-] to clear):");
        string seed_text = make_stringf("%-20s", buf);
        prompt.cprintf("\n%s\n\n", seed_text.c_str());

        prompt.cprintf(
            "The seed will determine the dungeon layout, monsters, and items\n"
            "that you discover, relative to this version of crawl. (See the \n"
            "manual for more details.)\n\n");
#ifdef SEEDING_UNRELIABLE
            prompt.cprintf(
                "Warning: your build of crawl does not support stable seeding!\n"
                "Levels may differ from 'official' seeded games.\n\n");
#endif
        prompt_ui->set_text(prompt);
        // yes this appalling, some day we will have real buttons and text
        // input. The seed highlight doesn't do much on console, but makes
        // tiles look a lot better. N.b. the newline before the seed above
        // is really so that an empty seed string won't get multiple highlights.
        prompt_ui->set_highlight_pattern(seed_text, false);
        if (show_pregen_toggle)
        {
            if (choice.pregenerate)
                pregen_choice->set_highlight_pattern("Yes", false);
            else
                pregen_choice->set_highlight_pattern("No", false);
        }
        ui::pump_events();
    }
    ui::pop_layout();

    string result = reader.get_text();
    uint64_t tmp_seed = 0;
    // TODO: if the user types in a number that exceeds the max value, sscanf
    // will give back the max value. Probably better to print an error?
    int found = sscanf(result.c_str(), "%" SCNu64, &tmp_seed);

    if (cancel || crawl_state.seen_hups || !found || result.size() == 0)
        game_ended(game_exit::abort);

    Options.seed = choice.seed = tmp_seed;
    Options.pregen_dungeon = choice.pregenerate;
}

// Read a choice of game into ng.
// Returns false if a game (with name ng.name) should
// be restored instead of starting a new character.
bool choose_game(newgame_def& ng, newgame_def& choice,
                 const newgame_def& defaults)
{
#ifdef USE_TILE_WEB
    tiles.set_ui_state(UI_CRT);
#endif

    clrscr();

    textcolour(LIGHTGREY);

    ng.name = choice.name;
    ng.type = choice.type;
    ng.map  = choice.map;

    if (ng.type == GAME_TYPE_SPRINT
        || ng.type == GAME_TYPE_TUTORIAL)
    {
        _choose_gamemode_map(ng, choice, defaults);
    }
    else if (ng.type == GAME_TYPE_CUSTOM_SEED)
        _choose_seed(ng, choice, defaults);

    _choose_char(ng, choice, defaults);

    // Set these again, since _mark_fully_random may reset ng.
    ng.name = choice.name;
    ng.type = choice.type;
    ng.seed = choice.seed;
    ng.pregenerate = choice.pregenerate;

#ifndef DGAMELAUNCH
    // New: pick name _after_ character choices.
    if (choice.name.empty())
        _choose_name(ng, choice);
#endif

    if (ng.name.empty())
        end(1, false, "No player name specified.");

    ASSERT(is_good_name(ng.name, false)
           && job_allowed(ng.species, ng.job)
           && ng.type != NUM_GAME_TYPE);

    write_newgame_options_file(choice);

    return false;
}

// Set ng_choice to defaults without overwriting name and game type.
static void _set_default_choice(newgame_def& ng, newgame_def& ng_choice,
                                const newgame_def& defaults)
{
    // Reset ng so _resolve_species_job will work properly.
    ng.clear_character();

    const string name = ng_choice.name;
    const game_type type   = ng_choice.type;
    ng_choice = defaults;
    ng_choice.name = name;
    ng_choice.type = type;
}

static void _mark_fully_random(newgame_def& ng, newgame_def& ng_choice,
                               bool viable)
{
    // Reset ng so _resolve_species_job will work properly.
    ng.clear_character();

    ng_choice.fully_random = true;
    if (viable)
    {
        ng_choice.species = SP_VIABLE;
        ng_choice.job = JOB_VIABLE;
    }
    else
    {
        ng_choice.species = SP_RANDOM;
        ng_choice.job = JOB_RANDOM;
    }
}

class UINewGameMenu;

static void _construct_species_menu(const newgame_def& ng,
                                    const newgame_def& defaults,
                                    UINewGameMenu* ng_menu)
{
    menu_letter letter = 'a';
    // Add entries for any species groups with at least one playable species.
    for (species_group& group : species_groups)
    {
        if (ng.job == JOB_UNKNOWN
            ||  any_of(begin(group.species_list),
                      end(group.species_list),
                      [&ng](species_type species)
                      { return species_allowed(ng.job, species) != CC_BANNED; }
                )
        )
        {
            group.attach(ng, defaults, ng_menu, letter);
        }
    }
}

static job_group jobs_order[] =
{
    {
        "Warrior",
        coord_def(0, 0), 20,
        { JOB_FIGHTER, JOB_GLADIATOR, JOB_MONK, JOB_HUNTER, JOB_ASSASSIN }
    },
    {
        "Adventurer",
        coord_def(0, 7), 20,
        { JOB_ARTIFICER, JOB_WANDERER }
    },
    {
        "Zealot",
        coord_def(1, 0), 25,
        { JOB_BERSERKER, JOB_ABYSSAL_KNIGHT, JOB_CHAOS_KNIGHT }
    },
    {
        "Warrior-mage",
        coord_def(1, 5), 26,
        { JOB_SKALD, JOB_TRANSMUTER, JOB_WARPER, JOB_ARCANE_MARKSMAN,
          JOB_ENCHANTER }
    },
    {
        "Mage",
        coord_def(2, 0), 22,
        { JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER, JOB_NECROMANCER,
          JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST,
          JOB_AIR_ELEMENTALIST, JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE }
    }
};

/**
 * Helper for _choose_job
 * constructs the menu used and highlights the previous job if there is one
 */
static void _construct_backgrounds_menu(const newgame_def& ng,
                                        const newgame_def& defaults,
                                        UINewGameMenu* ng_menu)
{
    menu_letter letter = 'a';
    // Add entries for any job groups with at least one playable background.
    for (job_group& group : jobs_order)
    {
        if (ng.species == SP_UNKNOWN
            || any_of(begin(group.jobs), end(group.jobs), [&ng](job_type job)
                      { return job_allowed(ng.species, job) != CC_BANNED; }))
        {
            group.attach(ng, defaults, ng_menu, letter);
        }
    }
}

class UINewGameMenu : public Widget
{
public:
    UINewGameMenu(int _choice_type, newgame_def& _ng, newgame_def& _ng_choice, const newgame_def& _defaults) : m_choice_type(_choice_type), m_ng(_ng), m_ng_choice(_ng_choice), m_defaults(_defaults)
    {
        m_vbox = make_shared<Box>(Box::VERT);
        m_vbox->_set_parent(this);
        m_vbox->align_items = Widget::Align::STRETCH;

        welcome.textcolour(BROWN);
        welcome.cprintf("%s", _welcome(m_ng).c_str());
        welcome.textcolour(YELLOW);
        welcome.cprintf(" Please select your ");
        welcome.cprintf(m_choice_type == C_JOB ? "background." : "species.");
        m_vbox->add_child(make_shared<Text>(welcome));

        descriptions = make_shared<Switcher>();

        m_main_items = make_shared<OuterMenu>(true, 3, 20);
        m_main_items->menu_id = m_choice_type == C_JOB ?
            "background-main" : "species-main";
        m_main_items->set_margin_for_crt({1, 0, 1, 0});
        m_main_items->set_margin_for_sdl({15, 0, 15, 0});
        m_main_items->descriptions = descriptions;
        m_vbox->add_child(m_main_items);

#ifndef USE_TILE_LOCAL
        m_vbox->expand_h = true;
        max_size() = { 80, INT_MAX };
#endif

        descriptions->set_margin_for_crt({1, 0, 1, 0});
        descriptions->set_margin_for_sdl({0, 0, 15, 0});
        descriptions->current() = -1;
        descriptions->shrink_h = true;
        m_vbox->add_child(descriptions);

        if (m_choice_type == C_JOB)
            _construct_backgrounds_menu(m_ng, m_defaults, this);
        else
            _construct_species_menu(m_ng, m_defaults, this);

        m_sub_items = make_shared<OuterMenu>(false, 2, 4);
        m_sub_items->menu_id = m_choice_type == C_JOB ?
            "background-sub" : "species-sub";
        m_sub_items->descriptions = descriptions;
        m_vbox->add_child(m_sub_items);
        _add_choice_menu_options(m_choice_type, m_ng, m_defaults);

        m_main_items->linked_menus[2] = m_sub_items;
        m_sub_items->linked_menus[0] = m_main_items;

        m_main_items->on_button_activated = m_sub_items->on_button_activated =
            [this](int id) { this->menu_item_activated(id); };

        for (auto &w : m_main_items->get_buttons())
        {
            w->on(Widget::slots.event, [w, this](wm_event ev) {
                return this->button_event_hook(ev, w);
            });
        }
        for (auto &w : m_sub_items->get_buttons())
        {
            w->on(Widget::slots.event, [w, this](wm_event ev) {
                return this->button_event_hook(ev, w);
            });
        }
    };

    virtual shared_ptr<Widget> get_child_at_offset(int x, int y) override {
        return static_pointer_cast<Widget>(m_vbox);
    }

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    virtual bool on_event(const wm_event& event) override;

#ifdef USE_TILE_WEB
    void serialize();
#endif

    void menu_item_activated(int id);

    bool done = false;
    bool end_game = false;
    bool cancel = false;

protected:
    friend struct job_group;
    friend struct species_group;
    void _add_group_item(menu_letter &letter,
                                int id,
                                int item_status,
                                string item_name,
#ifdef USE_TILE
                                tile_def item_tile,
#endif
                                bool is_active_item,
                                coord_def position)

    {
        auto label = make_shared<Text>();

#ifdef USE_TILE
        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->align_items = Widget::Align::CENTER;
        hbox->justify_items = Widget::Align::STRETCH;
        auto tile = make_shared<Image>();
        tile->set_tile(item_tile);
        tile->set_margin_for_sdl({0, 6, 0, 0});
        tile->flex_grow = 0;
        hbox->add_child(move(tile));
        hbox->add_child(label);
#endif

        COLOURS fg, hl;

        if (item_status == ITEM_STATUS_UNKNOWN)
        {
            fg = LIGHTGRAY;
            hl = STARTUP_HIGHLIGHT_NORMAL;
        }
        else if (item_status == ITEM_STATUS_RESTRICTED)
        {
            fg = DARKGRAY;
            hl = STARTUP_HIGHLIGHT_BAD;
        }
        else
        {
            fg = WHITE;
            hl = STARTUP_HIGHLIGHT_GOOD;
        }

        string text;
        text += letter;
        text += " - ";
        text += item_name;
        label->set_text(formatted_string(text, fg));

        string desc = unwrap_desc(getGameStartDescription(item_name));
        trim_string(desc);

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE
        hbox->set_margin_for_sdl({2,10,2,2});
        btn->set_child(move(hbox));
#else
        btn->set_child(move(label));
#endif
        btn->id = id;
        btn->description = desc;
        btn->hotkey = letter;
        btn->highlight_colour = hl;
        btn->set_margin_for_crt({0,1,0,0});

        m_main_items->add_button(btn, position.x, position.y);

        if (is_active_item || position == coord_def(0, 1))
            m_main_items->set_initial_focus(btn.get());
    }

    void _add_group_title(const char* name, coord_def position)
    {
        auto text = make_shared<Text>(formatted_string(name, LIGHTBLUE));
        text->set_margin_for_sdl({7, 0, 7, 32+2+6});
        m_main_items->add_label(move(text), position.x, position.y);
    }

    void _add_choice_menu_option(int x, int y, const string& text, char letter,
            int id, const string& desc)
    {
        _add_menu_sub_item(m_sub_items, x, y, text, desc, letter, id);
    }

    void _add_choice_menu_options(int choice_type,
                                        const newgame_def& ng,
                                        const newgame_def& defaults)
    {
        string choice_name = choice_type == C_JOB ? "background" : "species";
        string other_choice_name = choice_type == C_JOB ? "species" : "background";

        string text, desc;

        if (choice_type == C_SPECIES)
            text = "+ - Recommended species";
        else
            text = "+ - Recommended background";

        int id;
        // If the player has species chosen, use VIABLE, otherwise use RANDOM
        if ((choice_type == C_SPECIES && ng.job != JOB_UNKNOWN)
        || (choice_type == C_JOB && ng.species != SP_UNKNOWN))
        {
            id = M_VIABLE;
        }
        else
            id = M_RANDOM;
        desc = "Picks a random recommended " + other_choice_name + " based on your current " + choice_name + " choice.";

        _add_choice_menu_option(0, 0,
                text, '+', id, desc);

        _add_choice_menu_option(0, 1,
                "# - Recommended character", '#', M_VIABLE_CHAR,
                "Shuffles through random recommended character combinations "
                "until you accept one.");

        _add_choice_menu_option(0, 2,
                "% - List aptitudes", '%', M_APTITUDES,
                "Lists the numerical skill train aptitudes for all races.");

        _add_choice_menu_option(0, 3,
                "? - Help", '?', M_HELP,
                "Opens the help screen.");

        _add_choice_menu_option(1, 0,
                "    * - Random " + choice_name, '*', M_RANDOM,
                "Picks a random " + choice_name + ".");

        _add_choice_menu_option(1, 1,
                "    ! - Random character", '!', M_RANDOM_CHAR,
                "Shuffles through random character combinations "
                "until you accept one.");

        if ((choice_type == C_JOB && ng.species != SP_UNKNOWN)
            || (choice_type == C_SPECIES && ng.job != JOB_UNKNOWN))
        {
            text = "Space - Change " + other_choice_name;
            desc = "Lets you change your " + other_choice_name + " choice.";
        }
        else
        {
            text = "Space - Pick " + other_choice_name + " first";
            desc = "Lets you pick your " + other_choice_name + " first.";
        }
        _add_choice_menu_option(1, 2,
                text, ' ', M_ABORT, desc);

        if (_char_defined(defaults))
        {
            _add_choice_menu_option(1, 3,
                    "  Tab - " + newgame_char_description(defaults), '\t',
                    M_DEFAULT_CHOICE,
                    "Play a new game with your previous choice.");
        }
    }

private:
    bool button_event_hook(const wm_event& ev, MenuButton* btn)
    {
        if (ev.type == WME_KEYDOWN)
            return on_event(ev);
        return false;
    }

    formatted_string welcome;
    int m_choice_type;
    newgame_def& m_ng;
    newgame_def& m_ng_choice;
    const newgame_def& m_defaults;

    shared_ptr<Box> m_vbox;
    shared_ptr<OuterMenu> m_main_items;
    shared_ptr<OuterMenu> m_sub_items;
    shared_ptr<Text> description;
    shared_ptr<Switcher> descriptions;
};

void UINewGameMenu::_render()
{
    m_vbox->render();
}

SizeReq UINewGameMenu::_get_preferred_size(Direction dim, int prosp_width)
{
    return m_vbox->get_preferred_size(dim, prosp_width);
}

void UINewGameMenu::_allocate_region()
{
    m_vbox->allocate_region(m_region);
}

bool UINewGameMenu::on_event(const wm_event& ev)
{
    if (ev.type != WME_KEYDOWN)
        return false;
    int keyn = ev.key.keysym.sym;

    // First process all the menu entries available
    if (keyn != CK_ENTER)
    {
        // Process all the other keys that are not assigned to the menu
        switch (keyn)
        {
        case 'X':
        case CONTROL('Q'):
#ifdef USE_TILE_WEB
            tiles.send_exit_reason("cancel");
#endif
            return done = end_game = true;
        CASE_ESCAPE
        case CK_MOUSE_CMD:
            return done = cancel = true;
        case CK_BKSP:
            if (m_choice_type == C_JOB)
                m_ng_choice.job = JOB_UNKNOWN;
            else
                m_ng_choice.species = SP_UNKNOWN;
            return done = true;
        default:
            break;
        }
    }

    return false;
}


#ifdef USE_TILE_WEB
void UINewGameMenu::serialize()
{
    tiles.json_write_string("title", welcome.to_colour_string());
    m_main_items->serialize("main-items");
    m_sub_items->serialize("sub-items");
}
#endif

void UINewGameMenu::menu_item_activated(int id)
{
    bool viable = false;
    switch (id)
    {
    case M_VIABLE_CHAR:
        viable = true;
        // intentional fall-through
    case M_RANDOM_CHAR:
        _mark_fully_random(m_ng, m_ng_choice, viable);
        done = true;
        return;
    case M_DEFAULT_CHOICE:
        if (_char_defined(m_defaults))
        {
            _set_default_choice(m_ng, m_ng_choice, m_defaults);
            done = true;
        }
            // ignore default because we don't have previous start options
        return;
    case M_ABORT:
        m_ng.species = m_ng_choice.species = SP_UNKNOWN;
        m_ng.job     = m_ng_choice.job     = JOB_UNKNOWN;
        done = true;
        return;
    case M_HELP:
        show_help(m_choice_type == C_JOB ? '2' : '1');
        return;
    case M_APTITUDES:
        show_help('%', _highlight_pattern(m_ng));
        return;
    case M_VIABLE:
        if (m_choice_type == C_JOB)
            m_ng_choice.job = JOB_VIABLE;
        else
            m_ng_choice.species = SP_VIABLE;
        done = true;
        return;
    case M_RANDOM:
        if (m_choice_type == C_JOB)
            m_ng_choice.job = JOB_RANDOM;
        else
            m_ng_choice.species = SP_RANDOM;
        done = true;
        return;
    default:
        // we have a selection
        if (m_choice_type == C_JOB)
        {
            job_type job = static_cast<job_type> (id);
            if (m_ng.species == SP_UNKNOWN
                || job_allowed(m_ng.species, job) != CC_BANNED)
            {
                m_ng_choice.job = job;
                done = true;
            }
        }
        else
        {
            species_type species = static_cast<species_type> (id);
            if (m_ng.job == JOB_UNKNOWN
                || species_allowed(m_ng.job, species) != CC_BANNED)
            {
                m_ng_choice.species = species;
                done = true;
            }
        }
        return;
    }
}

void job_group::attach(const newgame_def& ng, const newgame_def& defaults,
                       UINewGameMenu* ng_menu, menu_letter &letter)
{
    ng_menu->_add_group_title(name, position);

    coord_def pos(position);

    for (job_type &job : jobs)
    {
        if (job == JOB_UNKNOWN)
            break;

        if (ng.species != SP_UNKNOWN
            && job_allowed(ng.species, job) == CC_BANNED)
        {
            continue;
        }

        int item_status;
        if (ng.species == SP_UNKNOWN)
            item_status = ITEM_STATUS_UNKNOWN;
        else if (job_allowed(ng.species, job) == CC_RESTRICTED)
            item_status = ITEM_STATUS_RESTRICTED;
        else
            item_status = ITEM_STATUS_ALLOWED;

        const bool is_active_item = defaults.job == job;

        ++pos.y;

        ng_menu->_add_group_item(
            letter,
            job,
            item_status,
            get_job_name(job),
#ifdef USE_TILE
            tile_def(tileidx_player_job(job,
                    item_status != ITEM_STATUS_RESTRICTED), TEX_GUI),
#endif
            is_active_item,
            pos
        );

        ++letter;
    }
}

void species_group::attach(const newgame_def& ng, const newgame_def& defaults,
                       UINewGameMenu* ng_menu, menu_letter &letter)
{
    ng_menu->_add_group_title(name, position);

    coord_def pos(position);

    for (species_type &this_species : species_list)
    {
        if (this_species == SP_UNKNOWN)
            break;

        if (ng.job == JOB_UNKNOWN && !is_starting_species(this_species))
            continue;

        if (ng.job != JOB_UNKNOWN
            && species_allowed(ng.job, this_species) == CC_BANNED)
        {
            continue;
        }

        int item_status;
        if (ng.job == JOB_UNKNOWN)
            item_status = ITEM_STATUS_UNKNOWN;
        else if (species_allowed(ng.job, this_species) == CC_RESTRICTED)
            item_status = ITEM_STATUS_RESTRICTED;
        else
            item_status = ITEM_STATUS_ALLOWED;

        const bool is_active_item = defaults.species == this_species;

        ++pos.y;

        ng_menu->_add_group_item(
            letter,
            this_species,
            item_status,
            species_name(this_species),
#ifdef USE_TILE
            tile_def(tileidx_player_species(this_species,
                    item_status != ITEM_STATUS_RESTRICTED), TEX_GUI),
#endif
            is_active_item,
            pos
        );

        ++letter;
    }
}


/**
 * Prompt for job or species menu
 * Saves the choice to ng_choice, doesn't resolve random choices.
 *
 * ng should be const, but we need to reset it for _resolve_species_job
 * to work correctly in view of fully random characters.
 */
static void _prompt_choice(int choice_type, newgame_def& ng, newgame_def& ng_choice,
                        const newgame_def& defaults)
{
    auto newgame_ui = make_shared<UINewGameMenu>(choice_type, ng, ng_choice, defaults);
    auto popup = make_shared<ui::Popup>(newgame_ui);

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    newgame_ui->serialize();
    tiles.push_ui_layout("newgame-choice", 1);
#endif

    ui::push_layout(move(popup));
    ui::set_focused_widget(newgame_ui.get());
    while (!newgame_ui->done && !crawl_state.seen_hups)
        ui::pump_events();
    ui::pop_layout();

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif

    if (newgame_ui->end_game)
        end(0);
    if (newgame_ui->cancel || crawl_state.seen_hups)
        game_ended(game_exit::abort);
}

typedef pair<weapon_type, char_choice_restriction> weapon_choice;

static weapon_type _fixup_weapon(weapon_type wp,
                                 const vector<weapon_choice>& weapons)
{
    if (wp == WPN_UNKNOWN || wp == WPN_RANDOM || wp == WPN_VIABLE)
        return wp;
    for (weapon_choice choice : weapons)
        if (wp == choice.first)
            return wp;
    return WPN_UNKNOWN;
}

static void _construct_weapon_menu(const newgame_def& ng,
                                   const weapon_type& defweapon,
                                   const vector<weapon_choice>& weapons,
                                   shared_ptr<OuterMenu>& main_items,
                                   shared_ptr<OuterMenu>& sub_items)
{
    struct weapon_menu_item {
        skill_type skill;
        string label;
        tileidx_t tile;
        weapon_menu_item(skill_type _skill, string _label, tileidx_t _tile)
            : skill(move(_skill)), label(move(_label)), tile(move(_tile)) {};
        weapon_menu_item(skill_type _skill, string _label)
            : skill(move(_skill)), label(move(_label)), tile(0) {};
    };
    vector<weapon_menu_item> choices;

    string thrown_name;

    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        weapon_type wpn_type = weapons[i].first;

        switch (wpn_type)
        {
        case WPN_UNARMED:
            choices.emplace_back(SK_UNARMED_COMBAT, species_has_claws(ng.species) ? "claws" : "unarmed");
            break;
        case WPN_THROWN:
        {
            // We don't support choosing among multiple thrown weapons.
            tileidx_t tile = 0;
            if (species_can_throw_large_rocks(ng.species))
            {
                thrown_name = "large rocks";
#ifdef USE_TILE
                tile = TILE_MI_LARGE_ROCK;
#endif
            }
            else if (species_size(ng.species, PSIZE_TORSO) <= SIZE_SMALL)
            {
                thrown_name = "boomerangs";
#ifdef USE_TILE
                tile = TILE_MI_BOOMERANG;
#endif
            }
            else
            {
                thrown_name = "javelins";
#ifdef USE_TILE
                tile = TILE_MI_JAVELIN;
#endif
            }
            choices.emplace_back(SK_THROWING,
                    thrown_name + " and throwing nets", tile);
            break;
        }
        default:
            string text = weapon_base_name(wpn_type);
            item_def dummy;
            dummy.base_type = OBJ_WEAPONS;
            dummy.sub_type = wpn_type;
            if (is_ranged_weapon_type(wpn_type))
            {
                text += " and ";
                text += wpn_type == WPN_HUNTING_SLING ? ammo_name(MI_SLING_BULLET)
                                                      : ammo_name(wpn_type);
                text += "s";
            }
            choices.emplace_back(item_attack_skill(dummy), text
#ifdef USE_TILE
                    , tileidx_item(dummy)
#endif
            );
            break;
        }
    }

    int max_text_width = 0;
    for (const auto& choice : choices)
        max_text_width = max(max_text_width, strwidth(choice.label));

    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        const auto& choice = choices[i];

        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->align_items = Widget::Align::CENTER;
        hbox->set_margin_for_sdl({2,10,2,2});

#ifdef USE_TILE
        auto tile_stack = make_shared<Stack>();
        tile_stack->set_margin_for_sdl({0, 6, 0, 0});
        tile_stack->flex_grow = 0;
        hbox->add_child(tile_stack);

        if (choice.skill == SK_THROWING)
        {
            tile_stack->add_child(make_shared<Image>(
                    tile_def(TILE_MI_THROWING_NET, TEX_DEFAULT)));
        }
        if (choice.skill == SK_UNARMED_COMBAT)
        {
            tile_stack->min_size() =
#ifdef USE_TILE_WEB
                            // these dimensions are apparently unused for
                            // webtiles, we do this so they're not interpreted
                            // as characters for webtiles console.
                            0;
#else
                            TILE_Y;
#endif
        }
        else
        {
            tile_stack->add_child(make_shared<Image>(
                    tile_def(choice.tile, TEX_DEFAULT)));
        }
#endif

        auto label = make_shared<Text>();
        hbox->add_child(label);

        const char letter = 'a' + i;

        string text = make_stringf(" %c - %s", letter,
                chop_string(choice.label, max_text_width, true).c_str()
                );

        weapon_type wpn_type = weapons[i].first;
        char_choice_restriction wpn_restriction = weapons[i].second;

        const auto fg = wpn_restriction == CC_UNRESTRICTED ? WHITE : LIGHTGREY;
        const auto bg = wpn_restriction == CC_UNRESTRICTED ?
            STARTUP_HIGHLIGHT_GOOD : STARTUP_HIGHLIGHT_BAD;

        label->set_text(formatted_string(text, fg));

        hbox->justify_items = Widget::Align::STRETCH;
        string apt_text = make_stringf("(%+d apt)",
                species_apt(choice.skill, ng.species));
        auto suffix = make_shared<Text>(formatted_string(apt_text, fg));
        hbox->add_child(suffix);

        auto btn = make_shared<MenuButton>();
        btn->set_child(move(hbox));
        btn->id = wpn_type;
        btn->hotkey = letter;
        btn->highlight_colour = bg;

        // Is this item our default weapon?
        if (wpn_type == defweapon || (defweapon == WPN_UNKNOWN && i == 0))
            main_items->set_initial_focus(btn.get());
        main_items->add_button(move(btn), 0, i);
    }

    _add_menu_sub_item(sub_items, 0, 0, "+ - Recommended random choice",
            "Picks a random recommended weapon", '+', M_VIABLE);
    _add_menu_sub_item(sub_items, 0, 1, "% - List aptitudes",
            "Lists the numerical skill train aptitudes for all races", '%',
            M_APTITUDES);
    _add_menu_sub_item(sub_items, 0, 2, "? - Help",
            "Opens the help screen", '?', M_HELP);
    _add_menu_sub_item(sub_items, 1, 0, "* - Random weapon",
            "Picks a random weapon", '*', WPN_RANDOM);
    _add_menu_sub_item(sub_items, 1, 1, "Bksp - Return to character menu",
            "Lets you return back to Character choice menu", CK_BKSP, M_ABORT);

    if (defweapon != WPN_UNKNOWN)
    {
        string text = "Tab - ";

        ASSERT(defweapon != WPN_THROWN || thrown_name != "");
        text += defweapon == WPN_RANDOM  ? "Random" :
                defweapon == WPN_VIABLE  ? "Recommended" :
                defweapon == WPN_UNARMED ? "unarmed" :
                defweapon == WPN_THROWN  ? thrown_name :
                weapon_base_name(defweapon);

        _add_menu_sub_item(sub_items, 1, 2, text,
                "Select your old weapon", '\t', M_DEFAULT_CHOICE);
    }
}

/**
 * Returns false if user escapes
 */
static bool _prompt_weapon(const newgame_def& ng, newgame_def& ng_choice,
                           const newgame_def& defaults,
                           const vector<weapon_choice>& weapons)
{
    weapon_type defweapon = _fixup_weapon(defaults.weapon, weapons);

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    dolls_data doll;
    fill_doll_for_newgame(doll, ng);
#ifdef USE_TILE_LOCAL
    auto tile = make_shared<ui::PlayerDoll>(doll);
    tile->set_margin_for_sdl({0, 10, 0, 0});
    title_hbox->add_child(move(tile));
#endif
#endif
    auto title = make_shared<Text>(formatted_string(_welcome(ng), BROWN));
    title_hbox->add_child(title);
    title_hbox->align_items = Widget::CENTER;
    title_hbox->set_margin_for_sdl({0, 0, 20, 0});
    title_hbox->set_margin_for_crt({0, 0, 1, 0});

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->align_items = Widget::Align::STRETCH;
    vbox->add_child(title_hbox);
    auto prompt = make_shared<Text>(formatted_string("You have a choice of weapons.", CYAN));
    vbox->add_child(prompt);

    auto main_items = make_shared<OuterMenu>(true, 1, weapons.size());
    main_items->menu_id = "weapon-main";
    main_items->set_margin_for_sdl({15, 0, 15, 0});
    main_items->set_margin_for_crt({1, 0, 1, 0});
    vbox->add_child(main_items);

    auto sub_items = make_shared<OuterMenu>(false, 2, 3);
    sub_items->menu_id = "weapon-sub";
    vbox->add_child(sub_items);

    main_items->linked_menus[2] = sub_items;
    sub_items->linked_menus[0] = main_items;

    _construct_weapon_menu(ng, defweapon, weapons, main_items, sub_items);

    bool done = false, ret = false;

    auto menu_item_activated = [&](int id) {
        switch (id)
        {
            case M_ABORT:
                ret = false;
                done = true;
                return;
            case M_APTITUDES:
                show_help('%', _highlight_pattern(ng));
                return;
            case M_HELP:
                show_help('?');
                return;
            case M_DEFAULT_CHOICE:
                if (defweapon != WPN_UNKNOWN)
                {
                    ng_choice.weapon = defweapon;
                    break;
                }
                // No default weapon defined.
                // This case should never happen in those cases but just in case
                return;
            case M_VIABLE:
                ng_choice.weapon = WPN_VIABLE;
                break;
            case M_RANDOM:
                ng_choice.weapon = WPN_RANDOM;
                break;
            default:
                ng_choice.weapon = static_cast<weapon_type> (id);
                break;
        }
        ret = done = true;
    };
    main_items->on_button_activated = menu_item_activated;
    sub_items->on_button_activated = menu_item_activated;

    auto popup = make_shared<ui::Popup>(vbox);
    popup->add_event_filter([&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        int key = ev.key.keysym.sym;

        switch (key)
        {
            case 'X':
            case CONTROL('Q'):
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
            case CK_MOUSE_CMD:
                ret = false;
                return done = true;
            default:
                break;
        }

        return false;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("title", title->get_text().to_colour_string());
    tiles.json_write_string("prompt", prompt->get_text().to_colour_string());
    main_items->serialize("main-items");
    sub_items->serialize("sub-items");
    tiles.send_doll(doll, false, false);
    tiles.push_ui_layout("newgame-choice", 1);
#endif
    ui::run_layout(move(popup), done);
#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif

    return ret;
}

static weapon_type _starting_weapon_upgrade(weapon_type wp, job_type job,
                                            species_type species)
{
    const bool fighter = job == JOB_FIGHTER;
    const size_type size = species_size(species, PSIZE_TORSO);

    // TODO: actually query itemprop for one-handedness.
    switch (wp)
    {
    case WPN_SHORT_SWORD:
        return WPN_RAPIER;
    case WPN_MACE:
        return WPN_FLAIL;
    case WPN_HAND_AXE:
        return WPN_WAR_AXE;
    case WPN_SPEAR:
        // Small fighters can't use tridents with a shield.
        return fighter && size <= SIZE_SMALL  ? wp : WPN_TRIDENT;
    case WPN_FALCHION:
        return WPN_LONG_SWORD;
    default:
        return wp;
    }
}

static vector<weapon_choice> _get_weapons(const newgame_def& ng)
{
    vector<weapon_choice> weapons;
    if (job_gets_ranged_weapons(ng.job))
    {
        weapon_type startwep[4] = { WPN_THROWN, WPN_HUNTING_SLING,
                                    WPN_SHORTBOW, WPN_HAND_CROSSBOW };

        for (int i = 0; i < 4; i++)
        {
            weapon_choice wp;
            wp.first = startwep[i];

            wp.second = weapon_restriction(wp.first, ng);
            if (wp.second != CC_BANNED)
                weapons.push_back(wp);
        }
    }
    else
    {
        weapon_type startwep[7] = { WPN_SHORT_SWORD, WPN_MACE, WPN_HAND_AXE,
                                    WPN_SPEAR, WPN_FALCHION, WPN_QUARTERSTAFF,
                                    WPN_UNARMED };
        for (int i = 0; i < 7; ++i)
        {
            weapon_choice wp;
            wp.first = startwep[i];
            if (job_gets_good_weapons(ng.job))
            {
                wp.first = _starting_weapon_upgrade(wp.first, ng.job,
                                                    ng.species);
            }

            wp.second = weapon_restriction(wp.first, ng);
            if (wp.second != CC_BANNED)
                weapons.push_back(wp);
        }
    }
    return weapons;
}

static void _resolve_weapon(newgame_def& ng, newgame_def& ng_choice,
                            const vector<weapon_choice>& weapons)
{
    int weapon = ng_choice.weapon;

    if (ng_choice.allowed_weapons.size())
    {
        weapon =
            ng_choice.allowed_weapons[
                random2(ng_choice.allowed_weapons.size())];
    }

    switch (weapon)
    {
    case WPN_VIABLE:
    {
        int good_choices = 0;
        for (weapon_choice choice : weapons)
        {
            if (choice.second == CC_UNRESTRICTED
                && one_chance_in(++good_choices))
            {
                ng.weapon = choice.first;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case WPN_RANDOM:
        ng.weapon = weapons[random2(weapons.size())].first;
        return;

    default:
        // _fixup_weapon will return WPN_UNKNOWN, allowing the player
        // to select the weapon, if the weapon option is incompatible.
        ng.weapon = _fixup_weapon(ng_choice.weapon, weapons);
        return;
    }
}

// Returns false if aborted, else an actual weapon choice
// is written to ng.weapon for the jobs that call
// _update_weapon() later.
static bool _choose_weapon(newgame_def& ng, newgame_def& ng_choice,
                           const newgame_def& defaults)
{
    // No weapon use at all. The actual item will be removed later.
    if (ng.species == SP_FELID)
        return true;

    if (!job_has_weapon_choice(ng.job))
        return true;

    vector<weapon_choice> weapons = _get_weapons(ng);

    ASSERT(!weapons.empty());
    if (weapons.size() == 1)
    {
        ng.weapon = ng_choice.weapon = weapons[0].first;
        return true;
    }

    _resolve_weapon(ng, ng_choice, weapons);
    if (ng.weapon == WPN_UNKNOWN)
    {
        if (!_prompt_weapon(ng, ng_choice, defaults, weapons))
            return false;
        _resolve_weapon(ng, ng_choice, weapons);
    }

    return true;
}

#ifdef USE_TILE
static tile_def tile_for_map_name(string name)
{
    if (starts_with(name, "Lesson "))
    {
        const int i = name[7]-'1';
        ASSERT_RANGE(i, 0, 5);
        constexpr tileidx_t tutorial_tiles[5] = {
            TILEG_TUT_MOVEMENT,
            TILEG_TUT_COMBAT,
            TILEG_CMD_DISPLAY_INVENTORY,
            TILEG_CMD_CAST_SPELL,
            TILEG_CMD_USE_ABILITY,
        };
        return tile_def(tutorial_tiles[i], TEX_GUI);
    }

    if (name == "Sprint I: \"Red Sonja\"")
        return tile_def(TILEP_MONS_SONJA, TEX_PLAYER);
    if (name == "Sprint II: \"The Violet Keep of Menkaure\"")
        return tile_def(TILEP_MONS_MENKAURE, TEX_PLAYER);
    if (name == "Sprint III: \"The Ten Rune Challenge\"")
        return tile_def(TILE_MISC_RUNE_OF_ZOT, TEX_DEFAULT);
    if (name == "Sprint IV: \"Fedhas' Mad Dash\"")
        return tile_def(TILE_DNGN_ALTAR_FEDHAS, TEX_FEAT);
    if (name == "Sprint V: \"Ziggurat Sprint\"")
        return tile_def(TILE_DNGN_PORTAL_ZIGGURAT, TEX_FEAT);
    if (name == "Sprint VI: \"Thunderdome\"")
        return tile_def(TILE_GOLD16, TEX_DEFAULT);
    if (name == "Sprint VII: \"The Pits\"")
        return tile_def(TILE_WALL_CRYPT_METAL + 2, TEX_WALL);
    if (name == "Sprint VIII: \"Arena of Blood\"")
        return tile_def(TILE_UNRAND_WOE, TEX_DEFAULT);
    if (name == "Sprint IX: \"|||||||||||||||||||||||||||||\"")
        return tile_def(TILE_WALL_LAB_METAL + 2, TEX_WALL);
    return tile_def(0, TEX_GUI);
}
#endif

static void _construct_gamemode_map_menu(const mapref_vector& maps,
                                         const newgame_def& defaults,
                                         shared_ptr<OuterMenu>& main_items,
                                         shared_ptr<OuterMenu>& sub_items)
{
    string text;
    bool activate_next = defaults.map == "";

    for (int i = 0; i < static_cast<int> (maps.size()); i++)
    {
        auto label = make_shared<Text>();

        const char letter = 'a' + i;

        const string map_name = maps[i]->desc_or_name();
        text = " ";
        text += letter;
        text += " - ";
        text += map_name;

#ifdef USE_TILE
        auto hbox = make_shared<Box>(Box::HORZ);
        hbox->align_items = Widget::Align::CENTER;
        auto tile = make_shared<Image>();
        tile->set_tile(tile_for_map_name(map_name));
        tile->set_margin_for_sdl({0, 6, 0, 0});
        hbox->add_child(move(tile));
        hbox->add_child(label);
#endif

        label->set_text(formatted_string(text, LIGHTGREY));

        auto btn = make_shared<MenuButton>();
#ifdef USE_TILE
        hbox->set_margin_for_sdl({2,10,2,2});
        btn->set_child(move(hbox));
#else
        btn->set_child(move(label));
#endif
        btn->id = i; // ID corresponds to location in vector
        btn->hotkey = letter;
        main_items->add_button(btn, 0, i);

        if (activate_next)
        {
            main_items->set_initial_focus(btn.get());
            activate_next = false;
        }
        // Is this item our default map?
        else if (defaults.map == maps[i]->name)
        {
            if (crawl_state.last_game_exit.exit_reason == game_exit::win)
                activate_next = true;
            else
                main_items->set_initial_focus(btn.get());
        }
    }

    // Don't overwhelm new players with aptitudes or the full list of commands!
    if (!crawl_state.game_is_tutorial())
    {
        _add_menu_sub_item(sub_items, 0, 0, "% - List aptitudes",
                "Lists the numerical skill train aptitudes for all races",
                '%', M_APTITUDES);
        _add_menu_sub_item(sub_items, 0, 1, "? - Help",
                "Opens the help screen", '?', M_HELP);
        _add_menu_sub_item(sub_items, 1, 0, "* - Random map",
                "Picks a random sprint map", '*', M_RANDOM);
    }

    // TODO: let players escape back to first screen menu
    // Adjust the end marker to align the - because Bksp text is longer by 3
    //tmp = new TextItem();
    //tmp->set_text("Bksp - Return to character menu");
    //tmp->set_description_text("Lets you return back to Character choice menu");
    //tmp->set_fg_colour(BROWN);
    //tmp->add_hotkey(CK_BKSP);
    //tmp->set_id(M_ABORT);
    //tmp->set_highlight_colour(LIGHTGRAY);
    //menu->attach_item(tmp);

    // Only add tab entry if we have a previous map choice
    if (crawl_state.game_is_sprint() && !defaults.map.empty()
        && defaults.type == GAME_TYPE_SPRINT && _char_defined(defaults))
    {
        text.clear();
        text += "Tab - ";
        text += defaults.map;
        _add_menu_sub_item(sub_items, 1, 1, text,
                "Select your previous sprint map and character", '\t',
                M_DEFAULT_CHOICE);
    }
}

// Compare two maps by their ORDER: header, falling back to desc or name if equal.
static bool _cmp_map_by_order(const map_def* m1, const map_def* m2)
{
    return m1->order < m2->order
           || m1->order == m2->order && m1->desc_or_name() < m2->desc_or_name();
}

static void _prompt_gamemode_map(newgame_def& ng, newgame_def& ng_choice,
                                 const newgame_def& defaults,
                                 mapref_vector maps)
{
    formatted_string welcome;
    welcome.textcolour(BROWN);
    welcome.cprintf("%s\n", _welcome(ng).c_str());
    if (Options.seed_from_rc)
        welcome.cprintf("Custom seed: %" PRIu64 "\n", Options.seed_from_rc);

    welcome.textcolour(CYAN);
    welcome.cprintf("\nYou have a choice of %s:",
            ng_choice.type == GAME_TYPE_TUTORIAL ? "lessons" : "maps");

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->align_items = Widget::Align::STRETCH;
    vbox->add_child(make_shared<Text>(welcome));

    auto main_items = make_shared<OuterMenu>(true, 1, maps.size());
    main_items->menu_id = "map-main";
    main_items->set_margin_for_sdl({15, 0, 15, 0});
    main_items->set_margin_for_crt({1, 0, 1, 0});
    vbox->add_child(main_items);

    auto sub_items = make_shared<OuterMenu>(false, 2, 2);
    sub_items->menu_id = "map-sub";
    vbox->add_child(sub_items);

    main_items->linked_menus[2] = sub_items;
    sub_items->linked_menus[0] = main_items;

    sort(maps.begin(), maps.end(), _cmp_map_by_order);
    _construct_gamemode_map_menu(maps, defaults, main_items, sub_items);

    bool done = false, cancel = false;

    auto menu_item_activated = [&](int id) {
        switch (id)
        {
            case M_ABORT:
                // TODO: fix
                break;
            case M_APTITUDES:
                show_help('%', _highlight_pattern(ng));
                return;
            case M_HELP:
                show_help('?');
                return;
            case M_DEFAULT_CHOICE:
                _set_default_choice(ng, ng_choice, defaults);
                break;
            case M_RANDOM:
                // FIXME setting this to "random" is broken
                ng_choice.map.clear();
                break;
            default:
                // We got an item selection
                ng_choice.map = maps.at(id)->name;
                break;
        }
        done = true;
    };
    main_items->on_button_activated = menu_item_activated;
    sub_items->on_button_activated = menu_item_activated;

    auto popup = make_shared<ui::Popup>(vbox);
    popup->add_event_filter([&](wm_event ev)  {
        if (ev.type != WME_KEYDOWN)
            return false;
        int keyn = ev.key.keysym.sym;

        switch (keyn)
        {
            case 'X':
            case CONTROL('Q'):
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                end(0);
                break;
            CASE_ESCAPE
                return done = cancel = true;
                break;
            case ' ':
                return done = true;
            default:
                break;
        }

        return false;
    });
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("title", welcome.to_colour_string());
    main_items->serialize("main-items");
    sub_items->serialize("sub-items");
    tiles.push_ui_layout("newgame-choice", 1);
#endif
    ui::run_layout(move(popup), done);
#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif

    if (cancel || crawl_state.seen_hups)
        game_ended(game_exit::abort);
}

static void _resolve_gamemode_map(newgame_def& ng, const newgame_def& ng_choice,
                                  const mapref_vector& maps)
{
    if (ng_choice.map == "random" || ng_choice.map.empty())
        ng.map = maps[random2(maps.size())]->name;
    else
        ng.map = ng_choice.map;
}

static void _choose_gamemode_map(newgame_def& ng, newgame_def& ng_choice,
                                 const newgame_def& defaults)
{
    // Sprint, otherwise Tutorial.
    const bool is_sprint = (ng_choice.type == GAME_TYPE_SPRINT);

    const string type_name = gametype_to_str(ng_choice.type);

    const mapref_vector maps = find_maps_for_tag(type_name);

    if (maps.empty())
        end(1, true, "No %s maps found.", type_name.c_str());

    if (ng_choice.map.empty())
    {
        if (is_sprint
            && ng_choice.type == !crawl_state.sprint_map.empty())
        {
            ng_choice.map = crawl_state.sprint_map;
        }
        else if (maps.size() > 1)
            _prompt_gamemode_map(ng, ng_choice, defaults, maps);
        else
            ng_choice.map = maps[0]->name;
    }

    _resolve_gamemode_map(ng, ng_choice, maps);
}

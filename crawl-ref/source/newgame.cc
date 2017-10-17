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
#include "hints.h"
#include "initfile.h"
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
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE_LOCAL
#include "tilereg-crt.h"
#endif
#include "version.h"

static void _choose_gamemode_map(newgame_def& ng, newgame_def& ng_choice,
                                 const newgame_def& defaults);
static bool _choose_weapon(newgame_def& ng, newgame_def& ng_choice,
                          const newgame_def& defaults);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

newgame_def::newgame_def()
    : name(), type(GAME_TYPE_NORMAL),
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

static string _char_description(const newgame_def& ng)
{
    if (_is_random_viable_choice(ng))
        return "Viable character";
    else if (_is_random_choice(ng))
        return "Random character";
    else if (_is_random_job(ng.job))
    {
        const string j = (ng.job == JOB_RANDOM ? "Random " : "Viable ");
        return j + species_name(ng.species);
    }
    else if (_is_random_species(ng.species))
    {
        const string s = (ng.species == SP_RANDOM ? "Random " : "Viable ");
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

static void _print_character_info(const newgame_def& ng)
{
    clrscr();
    textcolour(BROWN);
    cprintf("%s\n", _welcome(ng).c_str());
}

void choose_tutorial_character(newgame_def& ng_choice)
{
    ng_choice.species = SP_HUMAN;
    ng_choice.job = JOB_FIGHTER;
    ng_choice.weapon = WPN_FLAIL;
}

// March 2008: change order of species and jobs on character selection
// screen as suggested by Markus Maier.
// We have subsequently added a few new categories.
// Replacing this with named groups, but leaving because a bunch of code
// still depends on it and I don't want to unwind that now. -2/24/2017 CBH
static const species_type species_order[] =
{
    // comparatively human-like looks
    SP_HUMAN,          SP_DEEP_ELF,
    SP_DEEP_DWARF,     SP_HILL_ORC,
    // small species
    SP_HALFLING,       SP_KOBOLD,
    SP_SPRIGGAN,
    // large species
    SP_OGRE,           SP_TROLL,
    // significantly different body type from human ("monstrous")
    SP_NAGA,           SP_CENTAUR,
    SP_MERFOLK,        SP_MINOTAUR,
    SP_TENGU,          SP_BASE_DRACONIAN,
    SP_GARGOYLE,       SP_FORMICID,
    SP_BARACHI,        SP_GNOLL,
    // mostly human shape but made of a strange substance
    SP_VINE_STALKER,
    // celestial species
    SP_DEMIGOD,        SP_DEMONSPAWN,
    // undead species
    SP_MUMMY,          SP_GHOUL,
    SP_VAMPIRE,
    // not humanoid at all
    SP_FELID,          SP_OCTOPODE,
};
COMPILE_CHECK(ARRAYSZ(species_order) <= NUM_SPECIES);

bool is_starting_species(species_type species)
{
    // Trunk-only until we finish the species.
    if (species == SP_GNOLL && Version::ReleaseType != VER_ALPHA)
        return false;

    return find(species_order, species_order + ARRAYSZ(species_order),
                species) != species_order + ARRAYSZ(species_order);
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
    {
        int good_choices = 0;
        for (const species_type sp : species_order)
        {
            if (is_good_combination(sp, ng.job, false, true)
                && one_chance_in(++good_choices))
            {
                ng.species = sp;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case SP_RANDOM:
        // any valid species will do
        if (ng.job == JOB_UNKNOWN)
        {
            do {
                ng.species = RANDOM_ELEMENT(species_order);
            } while (!is_starting_species(ng.species));
        }
        else
        {
            // Pick a random legal character.
            int good_choices = 0;
            for (const species_type sp : species_order)
            {
                if (is_good_combination(sp, ng.job, false, false)
                    && one_chance_in(++good_choices))
                {
                    ng.species = sp;
                }
            }
            if (!good_choices)
                end(1, false, "Failed to find legal species.");
        }
        return;

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
    {
        int good_choices = 0;
        for (int i = 0; i < NUM_JOBS; i++)
        {
            job_type job = job_type(i);
            if (is_good_combination(ng.species, job, true, true)
                && one_chance_in(++good_choices))
            {
                ng.job = job;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case JOB_RANDOM:
        if (ng.species == SP_UNKNOWN)
        {
            // any valid job will do
            do
            {
                ng.job = job_type(random2(NUM_JOBS));
            }
            while (!is_starting_job(ng.job));
        }
        else
        {
            // Pick a random legal character.
            int good_choices = 0;
            for (int i = 0; i < NUM_JOBS; i++)
            {
                job_type job = job_type(i);
                if (is_good_combination(ng.species, job, true, false)
                    && one_chance_in(++good_choices))
                {
                    ASSERT(is_starting_job(job));
                    ng.job = job;
                }
            }
            if (!good_choices)
                end(1, false, "Failed to find legal background.");
        }
        return;

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
    for (const species_type species : species_order)
        if (is_good_combination(species, ng.job, false, true))
            ret += species_name(species) + "  |";

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
                                _char_description(ng).c_str());
    }
}

// For completely random combinations (!, #, or Options.game.fully_random)
// reroll characters until the player accepts one of them or quits.
static bool _reroll_random(newgame_def& ng)
{
    clrscr();

    string specs = chop_string(species_name(ng.species), 79, false);

    cprintf("You are a%s %s %s.\n",
            (is_vowel(specs[0])) ? "n" : "", specs.c_str(),
            get_job_name(ng.job));

    cprintf("\nDo you want to play this combination? (ynq) [y]");
    char c = getchm();
    if (key_is_escape(c) || toalower(c) == 'q')
    {
#ifdef USE_TILE_WEB
        tiles.send_exit_reason("cancel");
#endif
        game_ended(game_exit::abort);
    }
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
#ifdef USE_TILE_WEB
            tiles.send_exit_reason("cancel");
#endif
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

// Read a choice of game into ng.
// Returns false if a game (with name ng.name) should
// be restored instead of starting a new character.
bool choose_game(newgame_def& ng, newgame_def& choice,
                 const newgame_def& defaults)
{
    clrscr();

    // XXX: this should be somewhere else.
    if (!crawl_state.startup_errors.empty()
        && !Options.suppress_startup_errors)
    {
        crawl_state.show_startup_errors();
        clrscr();
    }

    textcolour(LIGHTGREY);

    ng.name = choice.name;
    ng.type = choice.type;
    ng.map  = choice.map;

    if (ng.type == GAME_TYPE_SPRINT
        || ng.type == GAME_TYPE_TUTORIAL)
    {
        _choose_gamemode_map(ng, choice, defaults);
    }

    _choose_char(ng, choice, defaults);

    // Set these again, since _mark_fully_random may reset ng.
    ng.name = choice.name;
    ng.type = choice.type;

#ifndef DGAMELAUNCH
    // New: pick name _after_ character choices.
    if (choice.name.empty())
    {
        clrscr();

        string specs = chop_string(species_name(ng.species), 79, false);

        cprintf("You are a%s %s %s.\n",
                (is_vowel(specs[0])) ? "n" : "", specs.c_str(),
                get_job_name(ng.job));

        enter_player_name(choice);
        ng.name = choice.name;
        ng.filename = get_save_filename(choice.name);

        if (save_exists(ng.filename))
        {
            cprintf("\nDo you really want to overwrite your old game? ");
            char c = getchm();
            if (c != 'Y' && c != 'y')
                return true;
        }
    }
#endif

    if (ng.name.empty())
        end(1, false, "No player name specified.");

    ASSERT(is_good_name(ng.name, false, false)
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


/**
 * Helper function for _choose_species
 * Constructs the menu screen
 */
static const int COLUMN_WIDTH = 25;
static const int X_MARGIN = 4;
static const int CHAR_DESC_START_Y = 16;
static const int CHAR_DESC_HEIGHT = 3;
static const int SPECIAL_KEYS_START_Y = CHAR_DESC_START_Y
                                        + CHAR_DESC_HEIGHT + 1;

static void _add_choice_menu_options(int choice_type,
                                     const newgame_def& ng,
                                     const newgame_def& defaults,
                                     MenuFreeform* menu)
{
    string choice_name = choice_type == C_JOB ? "background" : "species";
    string other_choice_name = choice_type == C_JOB ? "species" : "background";

    // Add all the special button entries
    TextItem* tmp = new TextItem();
    if (choice_type == C_SPECIES)
        tmp->set_text("+ - Viable species");
    else
        tmp->set_text("+ - Viable background");
    coord_def min_coord = coord_def(X_MARGIN, SPECIAL_KEYS_START_Y);
    coord_def max_coord = coord_def(min_coord.x + tmp->get_text().size(),
                                    min_coord.y + 1);
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('+');
    // If the player has species chosen, use VIABLE, otherwise use RANDOM
    if ((choice_type == C_SPECIES && ng.job != JOB_UNKNOWN)
       || (choice_type == C_JOB && ng.species != SP_UNKNOWN))
    {
        tmp->set_id(M_VIABLE);
    }
    else
        tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Picks a random viable " + other_choice_name + " based on your current " + choice_name + " choice.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("# - Viable character");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('#');
    tmp->set_id(M_VIABLE_CHAR);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Shuffles through random viable character combinations "
                              "until you accept one.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("% - List aptitudes");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 2;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('%');
    tmp->set_id(M_APTITUDES);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Lists the numerical skill train aptitudes for all races.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("? - Help");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 3;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('?');
    tmp->set_id(M_HELP);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Opens the help screen.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("* - Random " + choice_name);
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Picks a random " + choice_name + ".");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("! - Random character");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('!');
    tmp->set_id(M_RANDOM_CHAR);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Shuffles through random character combinations "
                              "until you accept one.");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    if ((choice_type == C_JOB && ng.species != SP_UNKNOWN)
        || (choice_type == C_SPECIES && ng.job != JOB_UNKNOWN))
    {
        tmp->set_text("Space - Change " + other_choice_name);
        tmp->set_description_text("Lets you change your " + other_choice_name +
            " choice.");
    }
    else
    {
        tmp->set_text("Space - Pick " + other_choice_name + " first");
        tmp->set_description_text("Lets you pick your " + other_choice_name +
            " first.");
    }
    // Adjust the end marker to align the - because Space text is longer by 4
    min_coord.x = X_MARGIN + COLUMN_WIDTH - 4;
    min_coord.y = SPECIAL_KEYS_START_Y + 2;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey(' ');
    tmp->set_id(M_ABORT);
    tmp->set_highlight_colour(BLUE);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    if (_char_defined(defaults))
    {
        tmp = new TextItem();
        tmp->set_text("Tab - " + _char_description(defaults));
        // Adjust the end marker to align the - because Tab text is longer by 2
        min_coord.x = X_MARGIN + COLUMN_WIDTH - 2;
        min_coord.y = SPECIAL_KEYS_START_Y + 3;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('\t');
        tmp->set_id(M_DEFAULT_CHOICE);
        tmp->set_highlight_colour(BLUE);
        tmp->set_description_text("Play a new game with your previous choice.");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

static void _add_group_title(MenuFreeform* menu,
                             const char* name,
                             coord_def position,
                             int width)
{
    TextItem* tmp = new NoSelectTextItem();
    string text;
    tmp->set_text(name);
    tmp->set_fg_colour(LIGHTBLUE);
    coord_def min_coord(2 + position.x, 3 + position.y);
    coord_def max_coord(min_coord.x + width, min_coord.y + 1);
    tmp->set_bounds(min_coord, max_coord);
    menu->attach_item(tmp);
    tmp->set_visible(true);
}

static void _attach_group_item(MenuFreeform* menu,
                               menu_letter &letter,
                               int id,
                               int item_status,
                               string item_name,
                               bool is_active_item,
                               coord_def min_coord,
                               coord_def max_coord)

{
    TextItem* tmp = new TextItem();

    if (item_status == ITEM_STATUS_UNKNOWN)
    {
        tmp->set_fg_colour(LIGHTGRAY);
        tmp->set_highlight_colour(BLUE);
    }
    else if (item_status == ITEM_STATUS_RESTRICTED)
    {
        tmp->set_fg_colour(DARKGRAY);
        tmp->set_highlight_colour(BLUE);
    }
    else
    {
        tmp->set_fg_colour(WHITE);
        tmp->set_highlight_colour(GREEN);
    }

    string text;
    text += letter;
    text += " - ";
    text += item_name;
    tmp->set_text(text);
    tmp->set_bounds(min_coord, max_coord);
    tmp->add_hotkey(letter);
    tmp->set_id(id);
    tmp->set_description_text(unwrap_desc(getGameStartDescription(item_name)));
    menu->attach_item(tmp);
    tmp->set_visible(true);
    if (is_active_item)
        menu->set_active_item(tmp);
}

void species_group::attach(const newgame_def& ng, const newgame_def& defaults,
                       MenuFreeform* menu, menu_letter &letter)
{
    _add_group_title(menu, name, position, width);

    coord_def min_coord(2 + position.x, 3 + position.y);
    coord_def max_coord(min_coord.x + width, min_coord.y + 1);

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

        ++min_coord.y;
        ++max_coord.y;

        _attach_group_item(
            menu,
            letter,
            this_species,
            item_status,
            species_name(this_species),
            is_active_item,
            min_coord,
            max_coord
        );

        ++letter;
    }
}

static species_group species_groups[] =
{
    {
        "Simple",
        coord_def(0, 0),
        20,
        {
            SP_HILL_ORC,
            SP_MINOTAUR,
            SP_MERFOLK,
            SP_GARGOYLE,
            SP_BASE_DRACONIAN,
            SP_HALFLING,
            SP_TROLL,
            SP_GHOUL,
        }
    },
    {
        "Intermediate",
        coord_def(25, 0),
        20,
        {
            SP_HUMAN,
            SP_KOBOLD,
            SP_DEMONSPAWN,
            SP_CENTAUR,
            SP_SPRIGGAN,
            SP_TENGU,
            SP_DEEP_ELF,
            SP_OGRE,
            SP_DEEP_DWARF,
            SP_GNOLL,
        }
    },
    {
        "Advanced",
        coord_def(50, 0),
        20,
        {
            SP_VINE_STALKER,
            SP_VAMPIRE,
            SP_DEMIGOD,
            SP_FORMICID,
            SP_NAGA,
            SP_OCTOPODE,
            SP_FELID,
            SP_BARACHI,
            SP_MUMMY,
        }
    },
};

static void _construct_species_menu(const newgame_def& ng,
                                    const newgame_def& defaults,
                                    MenuFreeform* menu)
{
    ASSERT(menu != nullptr);

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
            group.attach(ng, defaults, menu, letter);
        }
    }

    _add_choice_menu_options(C_SPECIES, ng, defaults, menu);
}

void job_group::attach(const newgame_def& ng, const newgame_def& defaults,
                       MenuFreeform* menu, menu_letter &letter)
{
    _add_group_title(menu, name, position, width);

    coord_def min_coord(2 + position.x, 3 + position.y);
    coord_def max_coord(min_coord.x + width, min_coord.y + 1);

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

        string job_name = get_job_name(job);
        const bool is_active_item = defaults.job == job;

        ++min_coord.y;
        ++max_coord.y;

        _attach_group_item(
            menu,
            letter,
            job,
            item_status,
            job_name,
            is_active_item,
            min_coord,
            max_coord
        );

        ++letter;
    }
}

static job_group jobs_order[] =
{
    {
        "Warrior",
        coord_def(0, 0), 15,
        { JOB_FIGHTER, JOB_GLADIATOR, JOB_MONK, JOB_HUNTER, JOB_ASSASSIN }
    },
    {
        "Adventurer",
        coord_def(0, 7), 15,
        { JOB_ARTIFICER, JOB_WANDERER }
    },
    {
        "Zealot",
        coord_def(15, 0), 20,
        { JOB_BERSERKER, JOB_ABYSSAL_KNIGHT, JOB_CHAOS_KNIGHT }
    },
    {
        "Warrior-mage",
        coord_def(35, 0), 21,
        { JOB_SKALD, JOB_TRANSMUTER, JOB_WARPER, JOB_ARCANE_MARKSMAN,
          JOB_ENCHANTER }
    },
    {
        "Mage",
        coord_def(56, 0), 22,
        { JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER, JOB_NECROMANCER,
          JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST,
          JOB_AIR_ELEMENTALIST, JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE }
    }
};

bool is_starting_job(job_type job)
{
    for (const job_group& group : jobs_order)
        for (const job_type job_ : group.jobs)
            if (job == job_)
                return true;
    return false;
}

/**
 * Helper for _choose_job
 * constructs the menu used and highlights the previous job if there is one
 */
static void _construct_backgrounds_menu(const newgame_def& ng,
                                        const newgame_def& defaults,
                                        MenuFreeform* menu)
{
    menu_letter letter = 'a';
    // Add entries for any job groups with at least one playable background.
    for (job_group& group : jobs_order)
    {
        if (ng.species == SP_UNKNOWN
            || any_of(begin(group.jobs), end(group.jobs), [&ng](job_type job)
                      { return job_allowed(ng.species, job) != CC_BANNED; }))
        {
            group.attach(ng, defaults, menu, letter);
        }
    }

    _add_choice_menu_options(C_JOB, ng, defaults, menu);
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
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(0,0), coord_def(get_number_of_cols() + 1,
                   get_number_of_lines() + 1), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    int keyn;

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolour(BROWN);
    cprintf("%s", _welcome(ng).c_str());

    textcolour(YELLOW);

    if (choice_type == C_JOB)
    {
        cprintf(" Please select your background.");
        _construct_backgrounds_menu(ng, defaults, freeform);
    }
    else
    {
        cprintf(" Please select your species.");
        _construct_species_menu(ng, defaults, freeform);
    }

    MenuDescriptor* descriptor = new MenuDescriptor(&menu);
    descriptor->init(
        coord_def(X_MARGIN, CHAR_DESC_START_Y),
        coord_def(get_number_of_cols(),
        CHAR_DESC_START_Y + CHAR_DESC_HEIGHT),
        "descriptor"
    );
    menu.attach_object(descriptor);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous background?
    if (menu.get_active_item() == nullptr)
        freeform->activate_first_item();

#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    textcolour(LIGHTGREY);

    // Poll input until we have a conclusive escape or pick
    while (true)
    {
        menu.draw_menu();

        keyn = getch_ck();

        // First process all the menu entries available
        if (!menu.process_key(keyn))
        {
            // Process all the other keys that are not assigned to the menu
            switch (keyn)
            {
            case 'X':
            case CONTROL('Q'):
                cprintf("\nGoodbye!");
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                end(0);
                return;
            CASE_ESCAPE
            case CK_MOUSE_CMD:
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                game_ended(game_exit::abort);
            case CK_BKSP:
                if (choice_type == C_JOB)
                    ng_choice.job = JOB_UNKNOWN;
                else
                    ng_choice.species = SP_UNKNOWN;
                return;
            default:
                // if we get this far, we did not get a significant selection
                // from the menu, nor did we get an escape character
                // continue the while loop from the beginning and poll a new key
                continue;
            }
        }
        // We have had a significant input key event
        // construct the return vector
        vector<MenuItem*> selection = menu.get_selected_items();
        if (!selection.empty())
        {
            // we have a selection!
            // we only care about the first selection (there should be only one)
            int selection_key = selection.at(0)->get_id();

            bool viable = false;
            switch (selection_key)
            {
            case M_VIABLE_CHAR:
                viable = true;
                // intentional fall-through
            case M_RANDOM_CHAR:
                _mark_fully_random(ng, ng_choice, viable);
                return;
            case M_DEFAULT_CHOICE:
                if (_char_defined(defaults))
                {
                    _set_default_choice(ng, ng_choice, defaults);
                    return;
                }
                else
                {
                    // ignore default because we don't have previous start options
                    continue;
                }
            case M_ABORT:
                ng.species = ng_choice.species = SP_UNKNOWN;
                ng.job     = ng_choice.job     = JOB_UNKNOWN;
                return;
            case M_HELP:
                 // access to the help files
                if (choice_type == C_JOB)
                    list_commands('2');
                else
                    list_commands('1');

                return _prompt_choice(choice_type, ng, ng_choice, defaults);
            case M_APTITUDES:
                list_commands('%', false, _highlight_pattern(ng));
                return _prompt_choice(choice_type, ng, ng_choice, defaults);
            case M_VIABLE:
                if (choice_type == C_JOB)
                    ng_choice.job = JOB_VIABLE;
                else
                    ng_choice.species = SP_VIABLE;
                return;
            case M_RANDOM:
                if (choice_type == C_JOB)
                    ng_choice.job = JOB_RANDOM;
                else
                    ng_choice.species = SP_RANDOM;
                return;
            default:
                // we have a selection
                if (choice_type == C_JOB)
                {
                    job_type job = static_cast<job_type> (selection_key);
                    if (ng.species == SP_UNKNOWN
                        || job_allowed(ng.species, job) != CC_BANNED)
                    {
                        ng_choice.job = job;
                        return;
                    }
                    else
                    {
                        selection.at(0)->select(false);
                        continue;
                    }
                }
                else
                {
                    species_type species = static_cast<species_type> (selection_key);
                    if (ng.job == JOB_UNKNOWN
                        || species_allowed(ng.job, species) != CC_BANNED)
                    {
                        ng_choice.species = species;
                        return;
                    }
                    else
                        continue;
                }
            }
        }
    }
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

static const int WEAPON_COLUMN_WIDTH = 36;
static void _construct_weapon_menu(const newgame_def& ng,
                                   const weapon_type& defweapon,
                                   const vector<weapon_choice>& weapons,
                                   MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    TextItem* tmp = nullptr;
    string text;
    const char *thrown_name = nullptr;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        weapon_type wpn_type = weapons[i].first;
        char_choice_restriction wpn_restriction = weapons[i].second;
        tmp = new TextItem();
        text.clear();

        if (wpn_restriction == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(WHITE);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(LIGHTGRAY);
            tmp->set_highlight_colour(BLUE);
        }
        const char letter = 'a' + i;
        tmp->add_hotkey(letter);
        tmp->set_id(wpn_type);

        text += letter;
        text += " - ";
        switch (wpn_type)
        {
        case WPN_UNARMED:
            text += species_has_claws(ng.species) ? "claws" : "unarmed";
            break;
        case WPN_THROWN:
            // We don't support choosing among multiple thrown weapons.
            ASSERT(!thrown_name);
            if (species_can_throw_large_rocks(ng.species))
                thrown_name = "large rocks";
            else if (species_size(ng.species, PSIZE_TORSO) <= SIZE_SMALL)
                thrown_name = "tomahawks";
            else
                thrown_name = "javelins";
            text += thrown_name;
            text += " and throwing nets";
            break;
        default:
            text += weapon_base_name(wpn_type);
            if (is_ranged_weapon_type(wpn_type))
            {
                text += " and ";
                text += wpn_type == WPN_HUNTING_SLING ? ammo_name(MI_SLING_BULLET)
                                                      : ammo_name(wpn_type);
                text += "s";
            }
            break;
        }
        // Fill to column width to give extra padding for the highlight
        text.append(WEAPON_COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);
        // Is this item our default weapon?
        if (wpn_type == defweapon)
            menu->set_active_item(tmp);
    }
    // Add all the special button entries
    tmp = new TextItem();
    tmp->set_text("+ - Viable random choice");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('+');
    tmp->set_id(M_VIABLE);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Picks a random viable weapon");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("% - List aptitudes");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('%');
    tmp->set_id(M_APTITUDES);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Lists the numerical skill train aptitudes for all races");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("? - Help");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 2;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('?');
    tmp->set_id(M_HELP);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Opens the help screen");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("* - Random weapon");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(WPN_RANDOM);
    tmp->set_highlight_colour(BLUE);
    tmp->set_description_text("Picks a random weapon");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Adjust the end marker to align the - because Bksp text is longer by 3
    tmp = new TextItem();
    tmp->set_text("Bksp - Return to character menu");
    tmp->set_description_text("Lets you return back to Character choice menu");
    min_coord.x = X_MARGIN + COLUMN_WIDTH - 3;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey(CK_BKSP);
    tmp->set_id(M_ABORT);
    tmp->set_highlight_colour(BLUE);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    if (defweapon != WPN_UNKNOWN)
    {
        text.clear();
        text = "Tab - ";

        ASSERT(defweapon != WPN_THROWN || thrown_name);
        text += defweapon == WPN_RANDOM  ? "Random" :
                defweapon == WPN_VIABLE  ? "Viable" :
                defweapon == WPN_UNARMED ? "unarmed" :
                defweapon == WPN_THROWN  ? thrown_name :
                weapon_base_name(defweapon);

        // Adjust the end marker to aling the - because
        // Tab text is longer by 2
        tmp = new TextItem();
        tmp->set_text(text);
        min_coord.x = X_MARGIN + COLUMN_WIDTH - 2;
        min_coord.y = SPECIAL_KEYS_START_Y + 2;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('\t');
        tmp->set_id(M_DEFAULT_CHOICE);
        tmp->set_highlight_colour(BLUE);
        tmp->set_description_text("Select your old weapon");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

/**
 * Returns false if user escapes
 */
static bool _prompt_weapon(const newgame_def& ng, newgame_def& ng_choice,
                           const newgame_def& defaults,
                           const vector<weapon_choice>& weapons)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    weapon_type defweapon = _fixup_weapon(defaults.weapon, weapons);

    _construct_weapon_menu(ng, defweapon, weapons, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous weapon?
    if (menu.get_active_item() == nullptr)
        freeform->activate_first_item();
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolour(CYAN);
    cprintf("\nYou have a choice of weapons:  ");

    while (true)
    {
        menu.draw_menu();

        int keyn = getch_ck();

        // First process menu entries
        if (!menu.process_key(keyn))
        {
            // Process all the keys that are not attached to items
            switch (keyn)
            {
            case 'X':
            case CONTROL('Q'):
                cprintf("\nGoodbye!");
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
            case CK_MOUSE_CMD:
                return false;
            default:
                // if we get this far, we did not get a significant selection
                // from the menu, nor did we get an escape character
                // continue the while loop from the beginning and poll a new key
                continue;
            }
        }
        // We have a significant key input!
        // Construct selection vector
        vector<MenuItem*> selection = menu.get_selected_items();
        // There should only be one selection, otherwise something broke
        if (selection.size() != 1)
        {
            // poll a new key
            continue;
        }

        // Get the stored id from the selection
        int selection_ID = selection.at(0)->get_id();
        switch (selection_ID)
        {
        case M_ABORT:
            return false;
        case M_APTITUDES:
            list_commands('%', false, _highlight_pattern(ng));
            return _prompt_weapon(ng, ng_choice, defaults, weapons);
        case M_HELP:
            list_commands('?');
            return _prompt_weapon(ng, ng_choice, defaults, weapons);
        case M_DEFAULT_CHOICE:
            if (defweapon != WPN_UNKNOWN)
            {
                ng_choice.weapon = defweapon;
                return true;
            }
            // No default weapon defined.
            // This case should never happen in those cases but just in case
            continue;
        case M_VIABLE:
            ng_choice.weapon = WPN_VIABLE;
            return true;
        case M_RANDOM:
            ng_choice.weapon = WPN_RANDOM;
            return true;
        default:
            // We got an item selection
            ng_choice.weapon = static_cast<weapon_type> (selection_ID);
            return true;
        }
    }
    // This should never happen
    return false;
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

static void _construct_gamemode_map_menu(const mapref_vector& maps,
                                         const newgame_def& defaults,
                                         MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    static const int MENU_COLUMN_WIDTH = get_number_of_cols();
    TextItem* tmp = nullptr;
    string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);
    bool activate_next = false;

    unsigned int padding_width = 0;
    for (int i = 0; i < static_cast<int> (maps.size()); i++)
    {
        padding_width = max<int>(padding_width,
                                 strwidth(maps.at(i)->desc_or_name()));
    }
    padding_width += 4; // Count the letter and " - "
    padding_width = min<int>(padding_width, MENU_COLUMN_WIDTH - 1);

    for (int i = 0; i < static_cast<int> (maps.size()); i++)
    {
        tmp = new TextItem();
        text.clear();

        tmp->set_fg_colour(LIGHTGREY);
        tmp->set_highlight_colour(GREEN);

        const char letter = 'a' + i;
        text += letter;
        text += " - ";

        text += maps[i]->desc_or_name();
        text = chop_string(text, padding_width);

        tmp->set_text(text);
        tmp->add_hotkey(letter);
        tmp->set_id(i); // ID corresponds to location in vector

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);

        if (activate_next)
        {
            menu->set_active_item(tmp);
            activate_next = false;
        }
        // Is this item our default map?
        else if (defaults.map == maps[i]->name)
        {
            if (crawl_state.last_game_exit == game_exit::win)
                activate_next = true;
            else
                menu->set_active_item(tmp);
        }
    }

    // Don't overwhelm new players with aptitudes or the full list of commands!
    if (!crawl_state.game_is_tutorial())
    {
        tmp = new TextItem();
        tmp->set_text("% - List aptitudes");
        min_coord.x = X_MARGIN;
        min_coord.y = SPECIAL_KEYS_START_Y + 1;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('%');
        tmp->set_id(M_APTITUDES);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Lists the numerical skill train aptitudes for all races");
        menu->attach_item(tmp);
        tmp->set_visible(true);

        tmp = new TextItem();
        tmp->set_text("? - Help");
        min_coord.x = X_MARGIN;
        min_coord.y = SPECIAL_KEYS_START_Y + 2;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('?');
        tmp->set_id(M_HELP);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Opens the help screen");
        menu->attach_item(tmp);
        tmp->set_visible(true);

        tmp = new TextItem();
        tmp->set_text("* - Random map");
        min_coord.x = X_MARGIN + COLUMN_WIDTH;
        min_coord.y = SPECIAL_KEYS_START_Y + 1;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('*');
        tmp->set_id(M_RANDOM);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Picks a random sprint map");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }

    // TODO: let players escape back to first screen menu
    // Adjust the end marker to align the - because Bksp text is longer by 3
    //tmp = new TextItem();
    //tmp->set_text("Bksp - Return to character menu");
    //tmp->set_description_text("Lets you return back to Character choice menu");
    //min_coord.x = X_MARGIN + COLUMN_WIDTH - 3;
    //min_coord.y = SPECIAL_KEYS_START_Y + 1;
    //max_coord.x = min_coord.x + tmp->get_text().size();
    //max_coord.y = min_coord.y + 1;
    //tmp->set_bounds(min_coord, max_coord);
    //tmp->set_fg_colour(BROWN);
    //tmp->add_hotkey(CK_BKSP);
    //tmp->set_id(M_ABORT);
    //tmp->set_highlight_colour(LIGHTGRAY);
    //menu->attach_item(tmp);
    //tmp->set_visible(true);

    // Only add tab entry if we have a previous map choice
    if (crawl_state.game_is_sprint() && !defaults.map.empty()
        && defaults.type == GAME_TYPE_SPRINT && _char_defined(defaults))
    {
        tmp = new TextItem();
        text.clear();
        text += "Tab - ";
        text += defaults.map;

        // Adjust the end marker to align the - because
        // Tab text is longer by 2
        tmp->set_text(text);
        min_coord.x = X_MARGIN + COLUMN_WIDTH - 2;
        min_coord.y = SPECIAL_KEYS_START_Y + 2;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('\t');
        tmp->set_id(M_DEFAULT_CHOICE);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Select your previous sprint map and character");
        menu->attach_item(tmp);
        tmp->set_visible(true);
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
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    sort(maps.begin(), maps.end(), _cmp_map_by_order);
    _construct_gamemode_map_menu(maps, defaults, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous sprint map?
    if (menu.get_active_item() == nullptr)
        freeform->activate_first_item();

    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolour(CYAN);
    cprintf("\nYou have a choice of %s:\n\n",
            ng_choice.type == GAME_TYPE_TUTORIAL ? "lessons"
                                                  : "maps");

    while (true)
    {
        menu.draw_menu();

        int keyn = getch_ck();

        // First process menu entries
        if (!menu.process_key(keyn))
        {
            // Process all the keys that are not attached to items
            switch (keyn)
            {
            case 'X':
            case CONTROL('Q'):
                cprintf("\nGoodbye!");
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                end(0);
                break;
            CASE_ESCAPE
#ifdef USE_TILE_WEB
                tiles.send_exit_reason("cancel");
#endif
                game_ended(game_exit::abort);
                break;
            case ' ':
                return;
            default:
                // if we get this far, we did not get a significant selection
                // from the menu, nor did we get an escape character
                // continue the while loop from the beginning and poll a new key
                continue;
            }
        }
        // We have a significant key input!
        // Construct selection vector
        vector<MenuItem*> selection = menu.get_selected_items();
        // There should only be one selection, otherwise something broke
        if (selection.size() != 1)
        {
            // poll a new key
            continue;
        }

        // Get the stored id from the selection
        int selection_ID = selection.at(0)->get_id();
        switch (selection_ID)
        {
        case M_ABORT:
            // TODO: fix
            return;
        case M_APTITUDES:
            list_commands('%', false, _highlight_pattern(ng));
            return _prompt_gamemode_map(ng, ng_choice, defaults, maps);
        case M_HELP:
            list_commands('?');
            return _prompt_gamemode_map(ng, ng_choice, defaults, maps);
        case M_DEFAULT_CHOICE:
            _set_default_choice(ng, ng_choice, defaults);
            return;
        case M_RANDOM:
            // FIXME setting this to "random" is broken
            ng_choice.map.clear();
            return;
        default:
            // We got an item selection
            ng_choice.map = maps.at(selection_ID)->name;
            return;
        }
    }
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

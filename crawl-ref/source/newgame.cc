/*
 *  File:       newgame.cc
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "newgame.h"

#include "cio.h"
#include "command.h"
#include "database.h"
#include "files.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "jobs.h"
#include "macro.h"
#include "makeitem.h"
#include "maps.h"
#include "menu.h"
#include "ng-input.h"
#include "ng-restr.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "species.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"
#include "hints.h"

#ifdef USE_TILE
#include "tilereg-crt.h"
#endif

static void _choose_sprint_map(newgame_def* ng, newgame_def* ng_choice,
                               const newgame_def& defaults);
static bool _choose_weapon(newgame_def* ng, newgame_def* ng_choice,
                          const newgame_def& defaults);
static bool _choose_book(newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults);
static bool _choose_god(newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults);
static bool _choose_wand(newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

newgame_def::newgame_def()
    : name(), type(GAME_TYPE_NORMAL),
      species(SP_UNKNOWN), job(JOB_UNKNOWN),
      weapon(WPN_UNKNOWN), book(SBT_NONE),
      religion(GOD_NO_GOD), wand(SWT_NO_SELECTION),
      fully_random(false)
{
}

void newgame_def::clear_character()
{
    species  = SP_UNKNOWN;
    job      = JOB_UNKNOWN;
    weapon   = WPN_UNKNOWN;
    book     = SBT_NONE;
    religion = GOD_NO_GOD;
    wand     = SWT_NO_SELECTION;
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

static bool _is_random_species(species_type sp)
{
    return (sp == SP_RANDOM || sp == SP_VIABLE);
}

static bool _is_random_job(job_type job)
{
    return (job == JOB_RANDOM || job == JOB_VIABLE);
}

static bool _is_random_choice(const newgame_def& choice)
{
    return (_is_random_species(choice.species)
            && _is_random_job(choice.job));
}

static bool _is_random_viable_choice(const newgame_def& choice)
{
    return (_is_random_choice(choice) &&
            (choice.job == JOB_VIABLE || choice.species == SP_VIABLE));
}

static bool _char_defined(const newgame_def& ng)
{
    return (ng.species != SP_UNKNOWN && ng.job != JOB_UNKNOWN);
}

static std::string _char_description(const newgame_def& ng)
{
    if (_is_random_viable_choice(ng))
        return "Viable character";
    else if (_is_random_choice(ng))
        return "Random character";
    else if (_is_random_job(ng.job))
    {
        const std::string j = (ng.job == JOB_RANDOM ? "Random " : "Viable ");
        return (j + species_name(ng.species));
    }
    else if (_is_random_species(ng.species))
    {
        const std::string s = (ng.species == SP_RANDOM ? "Random "
                                                       : "Viable ");
        return (s + get_job_name(ng.job));
    }
    else
        return (species_name(ng.species) + " " + get_job_name(ng.job));
}

static std::string _welcome(const newgame_def* ng)
{
    std::string text;
    if (ng->species != SP_UNKNOWN)
        text = species_name(ng->species);
    if (ng->job != JOB_UNKNOWN)
    {
        if (!text.empty())
            text += " ";
        text += get_job_name(ng->job);
    }
    if (!ng->name.empty())
    {
        if (!text.empty())
            text = " the " + text;
        text = ng->name + text;
    }
    else if (!text.empty())
        text = "unnamed " + text;
    if (!text.empty())
        text = ", " + text;
    text = "Welcome" + text + ".";
    return (text);
}

static void _print_character_info(const newgame_def* ng)
{
    clrscr();
    textcolor(BROWN);
    cprintf("%s\n", _welcome(ng).c_str());
}

// Determines if a species is valid. If 'display' is true, returns if
// the species is displayable in the new game screen - this is
// primarily used to suppress the display of the draconian variants.
static bool _is_species_valid_choice(species_type species)
{
    if (species < 0 || species > NUM_SPECIES)
        return (false);

    if (species >= SP_ELF) // These are all invalid.
        return (false);

    // Non-base draconians cannot be selected either.
    if (species >= SP_RED_DRACONIAN && species < SP_BASE_DRACONIAN)
        return (false);

    return (true);
}

#ifdef ASSERTS
static bool _species_is_undead( const species_type speci )
{
    return (speci == SP_MUMMY || speci == SP_GHOUL || speci == SP_VAMPIRE);
}
#endif

undead_state_type get_undead_state(const species_type sp)
{
    switch (sp)
    {
    case SP_MUMMY:
        return US_UNDEAD;
    case SP_GHOUL:
        return US_HUNGRY_DEAD;
    case SP_VAMPIRE:
        return US_SEMI_UNDEAD;
    default:
        ASSERT(!_species_is_undead(sp));
        return (US_ALIVE);
    }
}

static void _choose_tutorial_character(newgame_def* ng_choice)
{
    ng_choice->species = SP_HIGH_ELF;
    ng_choice->job = JOB_FIGHTER;
    ng_choice->weapon = WPN_MACE;
}

static void _resolve_species(newgame_def* ng, const newgame_def* ng_choice)
{
    // Don't overwrite existing species.
    if (ng->species != SP_UNKNOWN)
        return;

    switch (ng_choice->species)
    {
    case SP_UNKNOWN:
        ng->species = SP_UNKNOWN;
        return;

    case SP_VIABLE:
    {
        int good_choices = 0;
        for (int i = 0; i < ng_num_species(); i++)
        {
            species_type sp = get_species(i);
            if (is_good_combination(sp, ng->job, true)
                && one_chance_in(++good_choices))
            {
                ng->species = sp;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case SP_RANDOM:
        if (ng->job == JOB_UNKNOWN)
        {
            // any species will do
            ng->species = get_species(random2(ng_num_species()));
        }
        else
        {
            // Pick a random legal character.
            int good_choices = 0;
            for (int i = 0; i < ng_num_species(); i++)
            {
                species_type sp = get_species(i);
                if (is_good_combination(sp, ng->job, false)
                    && one_chance_in(++good_choices))
                {
                    ng->species = sp;
                }
            }
            if (!good_choices)
                end(1, false, "Failed to find legal species.");
        }
        return;

    default:
        ng->species = ng_choice->species;
        return;
    }
}

static void _resolve_job(newgame_def* ng, const newgame_def* ng_choice)
{
    if (ng->job != JOB_UNKNOWN)
        return;

    switch (ng_choice->job)
    {
    case JOB_UNKNOWN:
        ng->job = JOB_UNKNOWN;
        return;

    case JOB_VIABLE:
    {
        int good_choices = 0;
        for (int i = 0; i < ng_num_jobs(); i++)
        {
            job_type job = get_job(i);
            if (is_good_combination(ng->species, job, true)
                && one_chance_in(++good_choices))
            {
                ng->job = job;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case JOB_RANDOM:
        if (ng->species == SP_UNKNOWN)
        {
            // any job will do
            ng->job = get_job(random2(ng_num_jobs()));
        }
        else
        {
            // Pick a random legal character.
            int good_choices = 0;
            for (int i = 0; i < ng_num_jobs(); i++)
            {
                job_type job = get_job(i);
                if (is_good_combination(ng->species, job, false)
                    && one_chance_in(++good_choices))
                {
                    ng->job = job;
                }
            }
            if (!good_choices)
                end(1, false, "Failed to find legal background.");
        }
        return;

    default:
        ng->job = ng_choice->job;
        return;
    }
}

static void _resolve_species_job(newgame_def* ng, const newgame_def* ng_choice)
{
    _resolve_species(ng, ng_choice);
    _resolve_job(ng, ng_choice);
}

static void _prompt_species(newgame_def* ng, newgame_def* ng_choice,
                            const newgame_def& defaults);
static void _prompt_job(newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults);

static void _choose_species_job(newgame_def* ng, newgame_def* ng_choice,
                                const newgame_def& defaults)
{
    _resolve_species_job(ng, ng_choice);

    while (ng_choice->species == SP_UNKNOWN || ng_choice->job == JOB_UNKNOWN)
    {
        // Slightly non-obvious behaviour here is due to the fact that
        // both _prompt_species and _prompt_job can ask for an entirely
        // random character to be rolled. They will reset relevant fields
        // in *ng for this purpose.
        if (ng_choice->species == SP_UNKNOWN)
            _prompt_species(ng, ng_choice, defaults);
        _resolve_species_job(ng, ng_choice);
        if (ng_choice->job == JOB_UNKNOWN)
            _prompt_job(ng, ng_choice, defaults);
        _resolve_species_job(ng, ng_choice);
    }

    if (!job_allowed(ng->species, ng->job))
    {
        // Either an invalid combination was passed in through options,
        // or we messed up.
        end(1, false,
            "Incompatible species and background specified in options file.");
    }
}

// For completely random combinations (!, #, or Options.game.fully_random)
// reroll characters until the player accepts one of them or quits.
static bool _reroll_random(newgame_def* ng)
{
    clrscr();

    std::string specs = species_name(ng->species);
    if (specs.length() > 79)
        specs = specs.substr(0, 79);

    cprintf("You are a%s %s %s.\n",
            (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
            get_job_name(ng->job));

    cprintf("\nDo you want to play this combination? (ynq) [y]");
    char c = getchm();
    if (key_is_escape(c) || tolower(c) == 'q')
        end(0);
    return (tolower(c) == 'n');
}

static void _choose_char(newgame_def* ng, newgame_def* choice,
                         newgame_def defaults)
{
    const newgame_def ng_reset = *ng;

    if (ng->type == GAME_TYPE_TUTORIAL)
        _choose_tutorial_character(choice);
    else if (ng->type == GAME_TYPE_HINTS)
        pick_hints(choice);

    while (true)
    {
        _choose_species_job(ng, choice, defaults);

        if (choice->fully_random && _reroll_random(ng))
        {
            *ng = ng_reset;
            continue;
        }

        if (_choose_weapon(ng, choice, defaults)
            && _choose_book(ng, choice, defaults)
            && _choose_god(ng, choice, defaults)
            && _choose_wand(ng, choice, defaults))
        {
            // We're done!
            return;
        }

        // Else choose again, name and type stays same.
        defaults = *choice;
        *ng = ng_reset;
        *choice = ng_reset;
    }
}

// Read a choice of game into ng.
// Returns false if a game (with name ng->name) should
// be restored instead of starting a new character.
bool choose_game(newgame_def* ng, newgame_def* choice,
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

    textcolor(LIGHTGREY);

    ng->name = choice->name;
    ng->type = choice->type;
    ng->map  = choice->map;

    if (ng->type == GAME_TYPE_SPRINT)
        _choose_sprint_map(ng, choice, defaults);

    _choose_char(ng, choice, defaults);

    // Set these again, since _mark_fully_random may reset *ng.
    ng->name = choice->name;
    ng->type = choice->type;

#ifndef DGAMELAUNCH
    // New: pick name _after_ character choices.
    if (choice->name.empty())
    {
        clrscr();

        std::string specs = species_name(ng->species);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf("You are a%s %s %s.\n",
                (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                get_job_name(ng->job));

        enter_player_name(choice);
        ng->name = choice->name;

        if (save_exists(choice->name))
        {
            cprintf("\nDo you really want to overwrite your old game? ");
            char c = getchm();
            if (c != 'Y' && c != 'y')
                return (true);
        }
    }
#endif

    if (ng->name.empty())
        end(1, false, "No player name specified.");

    ASSERT(is_good_name(ng->name, false, false)
           && job_allowed(ng->species, ng->job)
           && ng->type != NUM_GAME_TYPE);

    write_newgame_options_file(*choice);

    return (false);
}

int start_to_book(int firstbook, int booktype)
{
    switch (firstbook)
    {
    case BOOK_MINOR_MAGIC_I:
        switch (booktype)
        {
        case SBT_FIRE:
            return (BOOK_MINOR_MAGIC_I);

        case SBT_COLD:
            return (BOOK_MINOR_MAGIC_II);

        case SBT_SUMM:
            return (BOOK_MINOR_MAGIC_III);

        default:
            return (-1);
        }

    case BOOK_CONJURATIONS_I:
        switch (booktype)
        {
        case SBT_FIRE:
            return (BOOK_CONJURATIONS_I);

        case SBT_COLD:
            return (BOOK_CONJURATIONS_II);

        case SP_CAT:
            dec_max_hp(3);

        default:
            return (-1);
        }

    default:
        return (-1);
    }
}

int claws_level(species_type sp)
{
    switch (sp)
    {
    case SP_GHOUL:
        return 1;
    case SP_TROLL:
        return 3;
    default:
        return 0;
    }
}

void make_rod(item_def &item, stave_type rod_type, int ncharges)
{
    item.base_type = OBJ_STAVES;
    item.sub_type  = rod_type;
    item.quantity  = 1;
    item.special   = you.item_description[IDESC_STAVES][rod_type];
    item.colour    = BROWN;

    init_rod_mp(item, ncharges);
}

// Set ng_choice to defaults without overwriting name and game type.
static void _set_default_choice(newgame_def* ng_choice,
                                const newgame_def& defaults)
{
    const std::string name = ng_choice->name;
    const game_type type   = ng_choice->type;
    *ng_choice = defaults;
    ng_choice->name = name;
    ng_choice->type = type;
}

static void _mark_fully_random(newgame_def* ng, newgame_def* ng_choice,
                               bool viable)
{
    // Reset *ng so _resolve_species_job will work properly.
    ng->clear_character();

    ng_choice->fully_random = true;
    if (viable)
    {
        ng_choice->species = SP_VIABLE;
        ng_choice->job = JOB_VIABLE;
    }
    else
    {
        ng_choice->species = SP_RANDOM;
        ng_choice->job = JOB_RANDOM;
    }
}

/**
 * Helper function for _choose_species
 * Constructs the menu screen
 */
static const int COLUMN_WIDTH = 25;
static const int X_MARGIN = 4;
static const int CHAR_DESC_START_Y = 17;
static const int SPECIAL_KEYS_START_Y = CHAR_DESC_START_Y + 3;
static void _construct_species_menu(const newgame_def* ng,
                                    const newgame_def& defaults,
                                    MenuFreeform* menu)
{
    ASSERT(menu != NULL);
    static const int ITEMS_IN_COLUMN = 8;
    // Construct the menu, 3 columns
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    for (int i = 0; i < ng_num_species(); ++i)
    {
        const species_type species = get_species(i);
        if (!_is_species_valid_choice(species))
            continue;

        tmp = new TextItem();
        text.clear();

        if (ng->job == JOB_UNKNOWN
            || job_allowed(species, ng->job) == CC_UNRESTRICTED)
          {
              tmp->set_fg_colour(LIGHTGRAY);
              tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(YELLOW);
        }
        if (ng->job != JOB_UNKNOWN
            && job_allowed(species, ng->job) == CC_BANNED)
        {
            text = "    ";
            text += species_name(species);
            text += " N/A";
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(RED);
        }
        else
        {
            text = index_to_letter(i);
            text += " - ";
            text += species_name(species);
        }
        // Fill to column width - 1
        text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);
        min_coord.x = X_MARGIN + (i / ITEMS_IN_COLUMN) * COLUMN_WIDTH;
        min_coord.y = 3 + i % ITEMS_IN_COLUMN;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        tmp->add_hotkey(index_to_letter(i));
        tmp->set_id(species);
        tmp->set_description_text(getGameStartDescription(species_name(species)));
        menu->attach_item(tmp);
        tmp->set_visible(true);
        if (defaults.species == species)
        {
            menu->set_active_item(tmp);
        }
    }

    // Add all the special button entries
    tmp = new TextItem();
    tmp->set_text("+ - Viable Species");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('+');
    // If the player has a job chosen, use VIABLE, otherwise use RANDOM
    if (ng->job != JOB_UNKNOWN)
    {
        tmp->set_id(M_VIABLE);
    }
    else
    {
        tmp->set_id(M_RANDOM);
    }
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random viable species based on your current job choice");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Shuffles through random viable character combinations "
                              "until you accept one");
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
    tmp->set_description_text("Lists the numerical skill train aptitudes for all races");
    tmp->set_highlight_colour(LIGHTGRAY);
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Opens the help screen");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("* - Random species");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random species");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Shuffles through random character combinations "
                              "until you accept one");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Adjust the end marker to align the - because Space text is longer by 4
    tmp = new TextItem();
    if (ng->job != JOB_UNKNOWN)
    {
        tmp->set_text("Space - Change background");
        tmp->set_description_text("Lets you change your background choice");
    }
    else
    {
        tmp->set_text("Space - Pick background first");
        tmp->set_description_text("Lets you pick your background first");
    }
    min_coord.x = X_MARGIN + COLUMN_WIDTH - 4;
    min_coord.y = SPECIAL_KEYS_START_Y + 2;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey(' ');
    tmp->set_id(M_ABORT);
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    if (_char_defined(defaults))
    {
        std::string tmp_string = "Tab - ";
        tmp_string += _char_description(defaults).c_str();
        // Adjust the end marker to aling the - because
        // Tab text is longer by 2
        tmp = new TextItem();
        tmp->set_text(tmp_string);
        min_coord.x = X_MARGIN + COLUMN_WIDTH - 2;
        min_coord.y = SPECIAL_KEYS_START_Y + 3;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('\t');
        tmp->set_id(M_DEFAULT_CHOICE);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Play a new game with your previous choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

// Prompt the player for a choice of species.
// ng should be const, but we need to reset it for _resolve_species_job
// to work correctly in view of fully random characters.
static void _prompt_species(newgame_def* ng, newgame_def* ng_choice,
                            const newgame_def& defaults)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(0,0), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    int keyn;

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor(BROWN);
    cprintf("%s", _welcome(ng).c_str());

    textcolor(YELLOW);
    cprintf(" Please select your species.");

    _construct_species_menu(ng, defaults, freeform);
    MenuDescriptor* descriptor = new MenuDescriptor(&menu);
    descriptor->init(coord_def(X_MARGIN, CHAR_DESC_START_Y),
                     coord_def(get_number_of_cols(), CHAR_DESC_START_Y + 2),
                     "descriptor");
    menu.attach_object(descriptor);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous background?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    textcolor( LIGHTGREY );
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
            CASE_ESCAPE
                cprintf("\nGoodbye!");
                end(0);
                return;
            case CK_BKSP:
                ng_choice->species = SP_UNKNOWN;
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
        if (selection.size() > 0)
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
                    _set_default_choice(ng_choice, defaults);
                    return;
                }
                else
                {
                    // ignore Tab because we don't have previous start options
                    continue;
                }
            case M_ABORT:
                ng->species = ng_choice->species = SP_UNKNOWN;
                ng->job     = ng_choice->job     = JOB_UNKNOWN;
                return;
            case M_HELP:
                 // access to the help files
                list_commands('1');
                return _prompt_species(ng, ng_choice, defaults);
            case M_APTITUDES:
                list_commands('%');
                return _prompt_species(ng, ng_choice, defaults);
            case M_VIABLE:
                ng_choice->species = SP_VIABLE;
                return;
            case M_RANDOM:
                ng_choice->species = SP_RANDOM;
                return;
            default:
                // we have a species selection
                species_type species = static_cast<species_type> (selection_key);
                if (ng->job == JOB_UNKNOWN
                    || job_allowed(species, ng->job) != CC_BANNED)
                {
                    ng_choice->species = species;
                    return;
                }
                else
                {
                    continue;
                }
            }
        }
    }
}

/**
 * Helper for _choose_job
 * constructs the menu used and highlights the previous job if there is one
 */
static void _construct_backgrounds_menu(const newgame_def* ng,
                                        const newgame_def& defaults,
                                        MenuFreeform* menu)
{
    static const int ITEMS_IN_COLUMN = 10;
    // Construct the menu, 3 columns
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    for (int i = 0; i < ng_num_jobs(); ++i)
    {
        const job_type job = get_job(i);
        tmp = new TextItem();
        text.clear();

        if (ng->species == SP_UNKNOWN
            || job_allowed(ng->species, job) == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(LIGHTGRAY);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(YELLOW);
        }
        if (ng->species != SP_UNKNOWN
            && job_allowed(ng->species, job) == CC_BANNED)
        {
            text = "    ";
            text += get_job_name(job);
            text += " N/A";
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(RED);
        }
        else
        {
            text = index_to_letter(i);
            text += " - ";
            text += get_job_name(job);
        }
        // fill the text entry to end of column - 1
        text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);
        min_coord.x = X_MARGIN + (i / ITEMS_IN_COLUMN) * COLUMN_WIDTH;
        min_coord.y = 3 + i % ITEMS_IN_COLUMN;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        tmp->add_hotkey(index_to_letter(i));
        tmp->set_id(job);
        tmp->set_description_text(getGameStartDescription(get_job_name(job)));

        menu->attach_item(tmp);
        tmp->set_visible(true);
        if (defaults.job == job)
        {
            menu->set_active_item(tmp);
        }
    }

    // Add all the special button entries
    tmp = new TextItem();
    tmp->set_text("+ - Viable background");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('+');
    // If the player has species chosen, use VIABLE, otherwise use RANDOM
    if (ng->species != SP_UNKNOWN)
    {
        tmp->set_id(M_VIABLE);
    }
    else
    {
        tmp->set_id(M_RANDOM);
    }
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random viable background based on your current species choice");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Shuffles through random viable character combinations "
                              "until you accept one");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Lists the numerical skill train aptitudes for all races");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Opens the help screen");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    tmp = new TextItem();
    tmp->set_text("* - Random background");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random background");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Shuffles through random character combinations "
                              "until you accept one");
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Adjust the end marker to align the - because Space text is longer by 4
    tmp = new TextItem();
    if (ng->species != SP_UNKNOWN)
    {
        tmp->set_text("Space - Change species");
        tmp->set_description_text("Lets you change your species choice");
    }
    else
    {
        tmp->set_text("Space - Pick species first");
        tmp->set_description_text("Lets you pick your species first");

    }
    min_coord.x = X_MARGIN + COLUMN_WIDTH - 4;
    min_coord.y = SPECIAL_KEYS_START_Y + 2;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey(' ');
    tmp->set_id(M_ABORT);
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    if (_char_defined(defaults))
    {
        text.clear();
        text = "Tab - ";
        text += _char_description(defaults).c_str();
        // Adjust the end marker to aling the - because
        // Tab text is longer by 2
        tmp = new TextItem();
        tmp->set_text(text);
        min_coord.x = X_MARGIN + COLUMN_WIDTH - 2;
        min_coord.y = SPECIAL_KEYS_START_Y + 3;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('\t');
        tmp->set_id(M_DEFAULT_CHOICE);
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Play a new game with your previous choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

/**
 * _prompt_job menu
 * Saves the choice to ng_choice, doesn't resolve random choices.
 *
 * ng should be const, but we need to reset it for _resolve_species_job
 * to work correctly in view of fully random characters.
 */
static void _prompt_job(newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(0,0), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    int keyn;

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor(BROWN);
    cprintf("%s", _welcome(ng).c_str());

    textcolor( YELLOW );
    cprintf(" Please select your background.");

    _construct_backgrounds_menu(ng, defaults, freeform);
    MenuDescriptor* descriptor = new MenuDescriptor(&menu);
    descriptor->init(coord_def(X_MARGIN, CHAR_DESC_START_Y),
                     coord_def(get_number_of_cols(), CHAR_DESC_START_Y + 2),
                     "descriptor");
    menu.attach_object(descriptor);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous background?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    textcolor( LIGHTGREY );

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
            CASE_ESCAPE
                cprintf("\nGoodbye!");
                end(0);
                return;
            case CK_BKSP:
                ng_choice->job = JOB_UNKNOWN;
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
        if (selection.size() > 0)
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
                    _set_default_choice(ng_choice, defaults);
                    return;
                }
                else
                {
                    // ignore default because we don't have previous start options
                    continue;
                }
            case M_ABORT:
                ng->species = ng_choice->species = SP_UNKNOWN;
                ng->job     = ng_choice->job     = JOB_UNKNOWN;
                return;
            case M_HELP:
                 // access to the help files
                list_commands('1');
                return _prompt_job(ng, ng_choice, defaults);
            case M_APTITUDES:
                list_commands('%');
                return _prompt_job(ng, ng_choice, defaults);
            case M_VIABLE:
                ng_choice->job = JOB_VIABLE;
                return;
            case M_RANDOM:
                ng_choice->job = JOB_RANDOM;
                return;
            default:
                // we have a job selection
                job_type job = static_cast<job_type> (selection_key);
                if (ng->species == SP_UNKNOWN
                    || job_allowed(ng->species, job) != CC_BANNED)
                {
                    ng_choice->job = job;
                    return;
                }
                else
                {
                    continue;
                }
            }
        }
    }
}

typedef std::pair<weapon_type, char_choice_restriction> weapon_choice;

static weapon_type _fixup_weapon(weapon_type wp,
                                 const std::vector<weapon_choice>& weapons)
{
    if (wp == WPN_UNKNOWN || wp == WPN_RANDOM || wp == WPN_VIABLE)
        return (wp);
    for (unsigned int i = 0; i < weapons.size(); ++i)
        if (wp == weapons[i].first)
            return (wp);
    return (WPN_UNKNOWN);
}

static void _construct_weapon_menu(const weapon_type& defweapon,
                                   const std::vector<weapon_choice>& weapons,
                                   MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        tmp = new TextItem();
        text.clear();

        if (weapons[i].second == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(LIGHTGRAY);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(YELLOW);
        }
        const char letter = 'a' + i;
        tmp->add_hotkey(letter);
        tmp->set_id(weapons[i].first);

        text += letter;
        text += " - ";
        text += weapons[i].first == WPN_UNARMED
                ? "claws" : weapon_base_name(weapons[i].first);
        // Fill to column width to give extra padding for the highlight
        text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);
        // Is this item our default weapon?
        if (weapons[i].first == defweapon)
        {
            menu->set_active_item(tmp);
        }
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
    tmp->set_highlight_colour(LIGHTGRAY);
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
    tmp->set_text("* - Random weapon");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(WPN_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
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
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    if (defweapon != WPN_UNKNOWN)
    {
        text.clear();
        text = "Tab - ";

        text += defweapon == WPN_RANDOM  ? "Random" :
                defweapon == WPN_VIABLE  ? "Viable" :
                defweapon == WPN_UNARMED ? "claws"  :
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
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Select your old weapon");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

/**
 * Returns false if user escapes
 */
static bool _prompt_weapon(const newgame_def* ng, newgame_def* ng_choice,
                           const newgame_def& defaults,
                           const std::vector<weapon_choice>& weapons)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    weapon_type defweapon = _fixup_weapon(defaults.weapon, weapons);

    _construct_weapon_menu(defweapon, weapons, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous weapon?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolor( CYAN );
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
                cprintf("\nGoodbye!");
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
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
            list_commands('%');
            return _prompt_weapon(ng, ng_choice, defaults, weapons);
        case M_HELP:
            list_commands('?');
            return _prompt_weapon(ng, ng_choice, defaults, weapons);
        case M_DEFAULT_CHOICE:
            if (defweapon != WPN_UNKNOWN)
            {
                ng_choice->weapon = defweapon;
                return true;
            }
            // No default weapon defined.
            // This case should never happen in those cases but just in case
            continue;
        case M_VIABLE:
            ng_choice->weapon = WPN_VIABLE;
            return true;
        case M_RANDOM:
            ng_choice->weapon = WPN_RANDOM;
            return true;
        default:
            // We got an item selection
            ng_choice->weapon = static_cast<weapon_type> (selection_ID);
            return true;
        }
    }
    // This should never happen
    return false;
}

static std::vector<weapon_choice> _get_weapons(const newgame_def* ng)
{
    std::vector<weapon_choice> weapons;

    weapon_type startwep[5] = { WPN_UNARMED, WPN_SHORT_SWORD, WPN_MACE,
                                WPN_HAND_AXE, WPN_SPEAR };
    for (int i = 0; i < 5; ++i)
    {
        weapon_choice wp;
        wp.first = startwep[i];

        switch (wp.first)
        {
        case WPN_UNARMED:
            if (ng->job == JOB_GLADIATOR || !claws_level(ng->species))
                continue;
            break;
        case WPN_SPEAR:
            // Non-small gladiators and merfolk get tridents.
            if (ng->job == JOB_GLADIATOR
                  && species_size(ng->species, PSIZE_BODY) >= SIZE_MEDIUM
                || ng->species == SP_MERFOLK)
            {
                wp.first = WPN_TRIDENT;
            }
            break;
        case WPN_MACE:
            if (ng->species == SP_OGRE)
                wp.first = WPN_ANKUS;
            break;
        default:
            break;
        }
        wp.second = weapon_restriction(wp.first, *ng);
        if (wp.second != CC_BANNED)
            weapons.push_back(wp);
    }
    return weapons;
}

static void _resolve_weapon(newgame_def* ng, newgame_def* ng_choice,
                            const std::vector<weapon_choice>& weapons)
{
    switch (ng_choice->weapon)
    {
    case WPN_UNKNOWN:
        ng->weapon = WPN_UNKNOWN;
        return;

    case WPN_VIABLE:
    {
        int good_choices = 0;
        for (unsigned int i = 0; i < weapons.size(); i++)
        {
            if (weapons[i].second == CC_UNRESTRICTED
                && one_chance_in(++good_choices))
            {
                ng->weapon = weapons[i].first;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case WPN_RANDOM:
        ng->weapon = weapons[random2(weapons.size())].first;
        return;

    default:
        // Check this is a legal choice, in case it came
        // through command line options.
        ng->weapon = _fixup_weapon(ng_choice->weapon, weapons);
        if (ng->weapon == WPN_UNKNOWN)
        {
            // Either an invalid combination was passed in through options,
            // or we messed up.
            end(1, false,
                "Incompatible weapon specified in options file.");
        }
        return;
    }
}

// Returns false if aborted, else an actual weapon choice
// is written to ng->weapon for the jobs that call
// _update_weapon() later.
static bool _choose_weapon(newgame_def* ng, newgame_def* ng_choice,
                           const newgame_def& defaults)
{
    // No weapon use at all.  The actual item will be removed later.
    if (ng->species == SP_CAT)
        return (true);

    switch (ng->job)
    {
    case JOB_FIGHTER:
    case JOB_GLADIATOR:
    case JOB_CHAOS_KNIGHT:
    case JOB_CRUSADER:
    case JOB_REAVER:
    case JOB_WARPER:
        break;
    default:
        return (true);
    }

    std::vector<weapon_choice> weapons = _get_weapons(ng);

    ASSERT(!weapons.empty());
    if (weapons.size() == 1)
    {
        ng->weapon = ng_choice->weapon = weapons[0].first;
        return (true);
    }

    if (ng_choice->weapon == WPN_UNKNOWN)
        if (!_prompt_weapon(ng, ng_choice, defaults, weapons))
            return (false);

    _resolve_weapon(ng, ng_choice, weapons);
    return (true);
}

static startup_book_type _fixup_book(startup_book_type book, int numbooks)
{
    if (book == SBT_RANDOM)
        return (SBT_RANDOM);
    else if (book == SBT_VIABLE)
        return (SBT_VIABLE);
    else if (book >= 0 && book < numbooks)
        return (book);
    else
        return (SBT_NONE);
}

static std::string _startup_book_name(startup_book_type book)
{
    switch (book)
    {
    case SBT_FIRE:
        return "Fire";
    case SBT_COLD:
        return "Cold";
    case SBT_SUMM:
        return "Summoning";
    case SBT_RANDOM:
        return "Random";
    case SBT_VIABLE:
        return "Viable";
    default:
        return "Buggy";
    }
}

static void _construct_book_menu(const newgame_def& ng,
                                 const book_type& firstbook,
                                 const startup_book_type& defbook,
                                 int numbooks,
                                 MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    item_def book;
    book.base_type = OBJ_BOOKS;
    book.quantity  = 1;
    book.plus      = 0;
    book.plus2     = 0;
    book.special   = 1;

    for (int i = 0; i < numbooks; ++i)
    {
        tmp = new TextItem();
        text.clear();

        book.sub_type = firstbook + i;
        startup_book_type sb = static_cast<startup_book_type>(i);
        if (book_restriction(sb, ng) == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(LIGHTGREY);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGREY);
            tmp->set_highlight_colour(YELLOW);
        }
        const char letter = 'a' + i;
        text += letter;
        text += " - ";
        text += book.name(DESC_PLAIN, false, true);

        //book name is longer than COLUMN_WIDTH
        //text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);

        tmp->add_hotkey(letter);
        tmp->set_id(sb);

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);
        // Is this item our default book?
        if (sb == defbook)
        {
            menu->set_active_item(tmp);
        }

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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random viable book");
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
    tmp->set_text("* - Random book");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random book");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Only add tab entry if we have a previous book choice
    if (defbook != SBT_NONE)
    {
        tmp = new TextItem();
        text.clear();
        text = "Tab - ";

        text +=  _startup_book_name(defbook);

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
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Select your previous book choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

// Returns false if the user aborted.
static bool _prompt_book(const newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults,
                         book_type firstbook, int numbooks)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    startup_book_type defbook = _fixup_book(defaults.book, numbooks);
    _construct_book_menu(*ng, firstbook, defbook, numbooks, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous weapon?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolor(CYAN);
    cprintf("\nYou have a choice of books:");

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
                cprintf("\nGoodbye!");
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
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
            list_commands('%');
            return _prompt_book(ng, ng_choice, defaults, firstbook, numbooks);
        case M_HELP:
            list_commands('?');
            return _prompt_book(ng, ng_choice, defaults, firstbook, numbooks);
        case M_DEFAULT_CHOICE:
            if (defbook != SBT_NONE)
            {
                ng_choice->book = defbook;
                return true;
            }
            // This case should not happen if defbook == SBT_NONE
            continue;
        case M_VIABLE:
            ng_choice->book = SBT_VIABLE;
            return true;
        case M_RANDOM:
            ng_choice->book = SBT_RANDOM;
            return true;
        default:
            // We got an item selection
            ng_choice->book = static_cast<startup_book_type> (selection_ID);
            return true;
        }
    }
    return false;
}

static void _resolve_book(newgame_def* ng, const newgame_def* ng_choice,
                          int numbooks)
{
    switch (ng_choice->book)
    {
    case SBT_NONE:
        ng->book = SBT_NONE;
        return;

    case SBT_VIABLE:
    {
        int good_choices = 0;
        for (int i = 0; i < numbooks; i++)
        {
            startup_book_type sb = static_cast<startup_book_type>(i);
            if (book_restriction(sb, *ng) == CC_UNRESTRICTED
                && one_chance_in(++good_choices))
            {
                ng->book = sb;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case SBT_RANDOM:
        ng->book = static_cast<startup_book_type>(random2(numbooks));
        return;

    default:
        if (ng_choice->book >= 0 && ng_choice->book < numbooks)
            ng->book = ng_choice->book;
        else
        {
            // Either an invalid combination was passed in through options,
            // or we messed up.
            end(1, false,
                "Incompatible book specified in options file.");
        }
        return;
    }
}

static bool _choose_book(newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults,
                         book_type firstbook, int numbooks)
{
    if (ng_choice->book == SBT_NONE
        && !_prompt_book(ng, ng_choice, defaults, firstbook, numbooks))
    {
        return (false);
    }
    _resolve_book(ng, ng_choice, numbooks);
    return (true);
}

static bool _choose_book(newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults)
{
    switch (ng->job)
    {
    case JOB_REAVER:
    case JOB_CONJURER:
        return (_choose_book(ng, ng_choice, defaults, BOOK_CONJURATIONS_I, 2));
    case JOB_WIZARD:
        return (_choose_book(ng, ng_choice, defaults, BOOK_MINOR_MAGIC_I, 3));
    default:
        return (true);
    }
}

// Covers both chaos knight and priest choices.
static std::string _god_text(god_type god)
{
    switch (god)
    {
    case GOD_ZIN:
        return "Zin (for traditional priests)";
    case GOD_YREDELEMNUL:
        return "Yredelemnul (for priests of death)";
    case GOD_BEOGH:
        return "Beogh (for priests of Orcs)";
    case GOD_XOM:
        return "Xom of Chaos";
    case GOD_MAKHLEB:
        return "Makhleb the Destroyer";
    case GOD_LUGONU:
        return "Lugonu the Unformed";
    default:
        ASSERT(false);
        return "";
    }
}

typedef std::pair<god_type, char_choice_restriction> god_choice;

static god_type _fixup_god(god_type god, const std::vector<god_choice>& gods)
{
    if (god == GOD_NO_GOD || god == GOD_RANDOM || god == GOD_VIABLE)
        return (god);
    for (unsigned int i = 0; i < gods.size(); ++i)
        if (god == gods[i].first)
            return (god);
    return (GOD_NO_GOD);
}

static void _construct_god_menu(const god_type& defgod,
                                const std::vector<god_choice>& gods,
                                MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    for (unsigned int i = 0; i < gods.size(); ++i)
    {
        tmp = new TextItem();
        text.clear();

        if (gods[i].second == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(LIGHTGREY);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGREY);
            tmp->set_highlight_colour(YELLOW);

        }
        const char letter = 'a' + i;
        text += letter;
        text += " - ";
        text += _god_text(gods[i].first);
        //god text is longer than COLUMN_WIDTH
        //text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);

        tmp->add_hotkey(letter);
        tmp->set_id(gods[i].first);

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);
        // Is this item our default god?
        if (gods[i].first == defgod)
        {
            menu->set_active_item(tmp);
        }
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
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random viable god");
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
    tmp->set_text("* - Random god");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random god");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Only add tab entry if we have a previous book choice
    if (defgod != GOD_NO_GOD)
    {
        tmp = new TextItem();
        text.clear();
        text = "Tab - ";

        text += defgod == GOD_RANDOM ? "Random" :
                defgod == GOD_VIABLE ? "Viable" :
                god_name(defgod);

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
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Select your previous book choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

static bool _prompt_god(const newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults,
                        const std::vector<god_choice>& gods)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    const god_type defgod = _fixup_god(defaults.religion, gods);
    _construct_god_menu(defgod, gods, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous god?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolor(CYAN);
    cprintf("\nWhich god do you wish to serve?");

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
                cprintf("\nGoodbye!");
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
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
            list_commands('%');
            return _prompt_god(ng, ng_choice, defaults, gods);
        case M_HELP:
            list_commands('?');
            return _prompt_god(ng, ng_choice, defaults, gods);
        case M_DEFAULT_CHOICE:
            if (defgod != GOD_NO_GOD)
            {
                ng_choice->religion = defgod;
                return true;
            }
            // This case should not happen when defgod == god_no_god
            continue;
        case M_VIABLE:
            ng_choice->religion = GOD_VIABLE;
            return true;
        case M_RANDOM:
            ng_choice->religion = GOD_RANDOM;
            return true;
        default:
            // We got an item selection
            ng_choice->religion = static_cast<god_type> (selection_ID);
            return true;
        }
    }
    return false;
}

static void _resolve_god(newgame_def* ng, const newgame_def* ng_choice,
                         const std::vector<god_choice>& gods)
{
    switch (ng_choice->religion)
    {
    case GOD_NO_GOD:
        ng->religion = GOD_NO_GOD;
        return;

    case GOD_VIABLE:
    {
        int good_choices = 0;
        for (unsigned int i = 0; i < gods.size(); i++)
        {
            if (gods[i].second == CC_UNRESTRICTED
                && one_chance_in(++good_choices))
            {
                ng->religion = gods[i].first;
            }
        }
        if (good_choices)
            return;
    }
        // intentional fall-through
    case GOD_RANDOM:
        ng->religion = gods[random2(gods.size())].first;
        return;

    default:
        // Check this is a legal choice, in case it came
        // through command line options.
        ng->religion = _fixup_god(ng_choice->religion, gods);
        if (ng->religion == GOD_NO_GOD)
        {
            // Either an invalid combination was passed in through options,
            // or we messed up.
            end(1, false,
                "Incompatible book specified in options file.");
        }
        return;
    }
}

static bool _choose_god(newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults)
{
    if (ng->job != JOB_PRIEST && ng->job != JOB_CHAOS_KNIGHT)
        return (true);

    std::vector<god_choice> gods;
    for (unsigned int i = 0; i < NUM_GODS; ++i)
    {
        god_choice god;
        god.first = static_cast<god_type>(i);
        god.second = religion_restriction(god.first, *ng);
        if (god.second != CC_BANNED)
            gods.push_back(god);
    }

    ASSERT(!gods.empty());
    if (gods.size() == 1)
    {
        ng->religion = ng_choice->religion = gods[0].first;
        return (true);
    }

    // XXX: assumes we can never choose between a god and no god.
    if (ng_choice->religion == GOD_NO_GOD)
        if (!_prompt_god(ng, ng_choice, defaults, gods))
            return (false);

    _resolve_god(ng, ng_choice, gods);
    return (true);
}

int start_to_wand(int wandtype, bool& is_rod)
{
    is_rod = false;

    switch (wandtype)
    {
    case SWT_ENSLAVEMENT:
        return (WAND_ENSLAVEMENT);

    case SWT_CONFUSION:
        return (WAND_CONFUSION);

    case SWT_MAGIC_DARTS:
        return (WAND_MAGIC_DARTS);

    case SWT_FROST:
        return (WAND_FROST);

    case SWT_FLAME:
        return (WAND_FLAME);

    case SWT_STRIKING:
        is_rod = true;
        return (STAFF_STRIKING);

    default:
        return (-1);
    }
}

static void _construct_wand_menu(const startup_wand_type& defwand,
                                MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);


    for (int i = 0; i < NUM_STARTUP_WANDS; i++)
    {
        tmp = new TextItem();
        text.clear();
        startup_wand_type sw = static_cast<startup_wand_type>(i);

        tmp->set_fg_colour(LIGHTGREY);
        tmp->set_highlight_colour(GREEN);

        const char letter = 'a' + i;
        text += letter;
        text += " - ";

        if (sw == SWT_STRIKING)
        {
            item_def rod;
            make_rod(rod, STAFF_STRIKING, 8);
            text += rod.name(DESC_QUALNAME, false, true);
        }
        else
        {
            bool dummy;
            wand_type w = static_cast<wand_type>(start_to_wand(sw, dummy));
            text += wand_type_name(w);
        }
        // Add padding
        text.append(COLUMN_WIDTH - text.size() - 1 , ' ');
        tmp->set_text(text);

        tmp->add_hotkey(letter);
        tmp->set_id(sw);

        min_coord.x = X_MARGIN;
        min_coord.y = ITEMS_START_Y + i;
        max_coord.x = min_coord.x + text.size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);

        menu->attach_item(tmp);
        tmp->set_visible(true);
        // Is this item our default god?
        if (sw == defwand)
        {
            menu->set_active_item(tmp);
        }
    }

    // Add all the special button entries
    // Wands do not have unviable choices
    //tmp = new TextItem();
    //tmp->set_text("+ - Viable random choice");
    //min_coord.x = X_MARGIN;
    //min_coord.y = SPECIAL_KEYS_START_Y;
    //max_coord.x = min_coord.x + tmp->get_text().size();
    //max_coord.y = min_coord.y + 1;
    //tmp->set_bounds(min_coord, max_coord);
    //tmp->set_fg_colour(BROWN);
    //tmp->add_hotkey('+');
    //tmp->set_id(SBT_VIABLE);
    //tmp->set_highlight_colour(LIGHTGRAY);
    //tmp->set_description_text("Picks a random viable god");
    //menu->attach_item(tmp);
    //tmp->set_visible(true);

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
    tmp->set_text("* - Random wand");
    min_coord.x = X_MARGIN + COLUMN_WIDTH;
    min_coord.y = SPECIAL_KEYS_START_Y;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('*');
    tmp->set_id(M_RANDOM);
    tmp->set_highlight_colour(LIGHTGRAY);
    tmp->set_description_text("Picks a random wand");
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
    tmp->set_highlight_colour(LIGHTGRAY);
    menu->attach_item(tmp);
    tmp->set_visible(true);

    // Only add tab entry if we have a previous wand choice
    if (defwand != SWT_NO_SELECTION)
    {
        tmp = new TextItem();
        text.clear();
        text = "Tab - ";

        text += defwand == SWT_ENSLAVEMENT ? "Enslavement" :
                defwand == SWT_CONFUSION   ? "Confusion"   :
                defwand == SWT_MAGIC_DARTS ? "Magic Darts" :
                defwand == SWT_FROST       ? "Frost"       :
                defwand == SWT_FLAME       ? "Flame"       :
                defwand == SWT_STRIKING    ? "Striking"    :
                defwand == SWT_RANDOM      ? "Random"      :
                                             "Buggy";

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
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Select your previous wand choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }
}

static bool _prompt_wand(const newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1,1), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    const startup_wand_type defwand = defaults.wand;

    _construct_wand_menu(defwand, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous god?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolor(CYAN);
    cprintf("\nYou have a choice of tools:");

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
                cprintf("\nGoodbye!");
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
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
            list_commands('%');
            return _prompt_wand(ng, ng_choice, defaults);
        case M_HELP:
            list_commands('?');
            return _prompt_wand(ng, ng_choice, defaults);
        case M_DEFAULT_CHOICE:
            if (defwand != SWT_NO_SELECTION)
            {
                ng_choice->wand = defwand;
                return true;
            }
            // This case should not happen if defwand == swt_no_selection
            continue;
        case M_RANDOM:
            ng_choice->wand = SWT_RANDOM;
            return true;
        default:
            // We got an item selection
            ng_choice->wand = static_cast<startup_wand_type> (selection_ID);
            return true;
        }
    }
    return false;
}

static void _resolve_wand(newgame_def* ng, const newgame_def* ng_choice)
{
    switch (ng_choice->wand)
    {
    case SWT_RANDOM:
        ng->wand = static_cast<startup_wand_type>(random2(NUM_STARTUP_WANDS));
        return;
    default:
        ng->wand = ng_choice->wand;
        return;
    }
}

static bool _choose_wand(newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults)
{
    if (ng->job != JOB_ARTIFICER)
        return (true);

    if (ng_choice->wand == SWT_NO_SELECTION)
        if (!_prompt_wand(ng, ng_choice, defaults))
            return (false);

    _resolve_wand(ng, ng_choice);
    return (true);
}

static void _construct_sprint_map_menu(const mapref_vector& maps,
                                       const newgame_def& defaults,
                                       MenuFreeform* menu)
{
    static const int ITEMS_START_Y = 5;
    static const int SPRINT_COLUMN_WIDTH = get_number_of_cols();
    TextItem* tmp = NULL;
    std::string text;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    unsigned int padding_width = 0;
    for (int i = 0; i < static_cast<int> (maps.size()); i++)
    {
        if (padding_width < maps.at(i)->desc_or_name().length())
        {
            padding_width = maps.at(i)->desc_or_name().length();
        }
    }
    padding_width += 4; // Count the letter and " - "

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
        if (static_cast<int>(text.length()) > SPRINT_COLUMN_WIDTH - 1)
            text = text.substr(0, SPRINT_COLUMN_WIDTH - 1);

        // Add padding
        if (padding_width > text.size())
        {
            text.append(padding_width - text.size(), ' ');
        }
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
        // Is this item our default sprint map?
        if (defaults.map == maps[i]->name)
        {
            menu->set_active_item(tmp);
        }
    }

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

    // Only add tab entry if we have a previous wand choice
    if (defaults.type == GAME_TYPE_SPRINT
        && !defaults.map.empty() && _char_defined(defaults))
    {
        tmp = new TextItem();
        text.clear();
        text += "Tab - ";
        text += defaults.map;

        // Adjust the end marker to aling the - because
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

static bool _cmp_map_by_name(const map_def* m1, const map_def* m2)
{
    return (m1->desc_or_name() < m2->desc_or_name());
}

static void _prompt_sprint_map(const newgame_def* ng, newgame_def* ng_choice,
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

    std::sort(maps.begin(), maps.end(), _cmp_map_by_name);
    _construct_sprint_map_menu(maps, defaults, freeform);

    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
    highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
    menu.attach_object(highlighter);

    // Did we have a previous sprint map?
    if (menu.get_active_item() == NULL)
    {
        freeform->activate_first_item();
    }
    _print_character_info(ng); // calls clrscr() so needs to be before attach()

#ifdef USE_TILE
    tiles.get_crt()->attach_menu(&menu);
#endif

    freeform->set_visible(true);
    highlighter->set_visible(true);

    textcolor(CYAN);
    cprintf("\nYou have a choice of maps:\n\n");

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
                cprintf("\nGoodbye!");
                end(0);
                break;
            case ' ':
            CASE_ESCAPE
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
        std::vector<MenuItem*> selection = menu.get_selected_items();
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
            list_commands('%');
            return _prompt_sprint_map(ng, ng_choice, defaults, maps);
        case M_HELP:
            list_commands('?');
            return _prompt_sprint_map(ng, ng_choice, defaults, maps);
        case M_DEFAULT_CHOICE:
            _set_default_choice(ng_choice, defaults);
            return;
        case M_RANDOM:
            // FIXME setting this to "random" is broken
            ng_choice->map.clear();
            return;
        default:
            // We got an item selection
            ng_choice->map = maps.at(selection_ID)->name;
            return;
        }
    }
}

static void _resolve_sprint_map(newgame_def* ng, const newgame_def* ng_choice,
                                const mapref_vector& maps)
{
    if (ng_choice->map == "random" || ng_choice->map.empty())
        ng->map = maps[random2(maps.size())]->name;
    else
        ng->map = ng_choice->map;
}

static void _choose_sprint_map(newgame_def* ng, newgame_def* ng_choice,
                               const newgame_def& defaults)
{
    const mapref_vector maps = get_sprint_maps();
    if (maps.empty())
        end(1, true, "No sprint maps found.");

    if (ng_choice->map.empty())
    {
        if (!crawl_state.sprint_map.empty())
            ng_choice->map = crawl_state.sprint_map;
        else if (maps.size() > 1)
            _prompt_sprint_map(ng, ng_choice, defaults, maps);
        else
            ng_choice->map = maps[0]->name;
    }

    _resolve_sprint_map(ng, ng_choice, maps);
}

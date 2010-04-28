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
#include "menu.h"
#include "ng-init.h"
#include "ng-input.h"
#include "ng-restr.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "species.h"
#include "state.h"
#include "stuff.h"
#include "tutorial.h"

#ifdef USE_TILE
#include "tilereg-crt.h"
#endif

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
        return (j + species_name(ng.species, 1));
    }
    else if (_is_random_species(ng.species))
    {
        const std::string s = (ng.species == SP_RANDOM ? "Random "
                                                       : "Viable ");
        return (s + get_job_name(ng.job));
    }
    else
        return (species_name(ng.species, 1) + " " + get_job_name(ng.job));
}

static std::string _welcome(const newgame_def* ng)
{
    std::string text;
    if (ng->species != SP_UNKNOWN)
        text = species_name(ng->species, 1);
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

    // No problem with these.
    if (species <= SP_RED_DRACONIAN || species > SP_BASE_DRACONIAN)
        return (true);

    // Draconians other than red return false.
    return (false);
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

    bool viable = false;
    switch (ng_choice->species)
    {
    case SP_UNKNOWN:
        ng->species = SP_UNKNOWN;
        return;

    case SP_VIABLE:
        viable = true;
        // intentional fall-through
    case SP_RANDOM:
        if (ng->job == JOB_UNKNOWN)
        {
            // any species will do
            ng->species = get_species(random2(ng_num_species()));
        }
        else
        {
            // depending on viable flag, pick either a
            // viable combo or a random combo
            do
            {
                ng->species = get_species(random2(ng_num_species()));
            }
            while (!is_good_combination(ng->species, ng->job, viable));
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

    bool viable = false;
    switch (ng_choice->job)
    {
    case JOB_UNKNOWN:
        ng->job = JOB_UNKNOWN;
        return;

    case JOB_VIABLE:
        viable = true;
        // intentional fall-through
    case JOB_RANDOM:
        if (ng->species == SP_UNKNOWN)
        {
            // any job will do
            ng->job = get_job(random2(ng_num_jobs()));
        }
        else
        {
            // depending on viable flag, pick either a
            // viable combo or a random combo
            do
            {
                ng->job = get_job(random2(ng_num_jobs()));
            }
            while (!is_good_combination(ng->species, ng->job, viable));
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

    std::string specs = species_name(ng->species, 1);
    if (specs.length() > 79)
        specs = specs.substr(0, 79);

    cprintf("You are a%s %s %s.\n",
            (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
            get_job_name(ng->job));

    cprintf("\nDo you want to play this combination? (ynq) [y]");
    char c = getchm();
    if (c == ESCAPE || tolower(c) == 'q')
        end(0);
    return (tolower(c) == 'n');
}

static void _choose_char(newgame_def* ng, newgame_def* choice,
                         newgame_def defaults)
{
    ng->name = choice->name;
    ng->type = choice->type;

    if (ng->type == GAME_TYPE_TUTORIAL)
        _choose_tutorial_character(choice);

    while (true)
    {
        _choose_species_job(ng, choice, defaults);

        if (choice->fully_random && _reroll_random(ng))
        {
            *ng = newgame_def();
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

        // Else choose again, name stays same.
        const std::string old_name = ng->name;

        defaults = *choice;
        *ng = newgame_def();
        *choice = newgame_def();

        ng->name = old_name;
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

    _choose_char(ng, choice, defaults);

    // New: pick name _after_ character choices.
    if (choice->name.empty())
    {
        clrscr();

        std::string specs = species_name(ng->species, 1);
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

static void _mark_fully_random(newgame_def* ng, newgame_def* ng_choice,
                               bool viable)
{
    // Reset *ng so _resolve_species_job will work properly.
    *ng = newgame_def();

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
static const int SPECIAL_KEYS_START_Y = 21;
static const int CHAR_DESC_START_Y = 18;
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

    for (int i = 0; i < NUM_SPECIES; ++i) {
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
            text += species_name(species, 1);
            text += " N/A";
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(RED);
        }
        else
        {
            text = index_to_letter(i);
            text += " - ";
            text += species_name(species, 1);
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
        tmp->set_description_text(getGameStartDescription(species_name(species, 1)));
        menu->attach_item(tmp);
        tmp->set_visible(true);
        if (defaults.species == species)
        {
            menu->set_active_item(tmp);
        }
    }

    // Add all the special button entries
    if (ng->job != JOB_UNKNOWN)
    {
        tmp = new TextItem();
        tmp->set_text("+ - Viable Species");
        min_coord.x = X_MARGIN;
        min_coord.y = SPECIAL_KEYS_START_Y;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('+');
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Picks a random viable species based on your current job choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }

    tmp = new TextItem();
    tmp->set_text("# - Viable character");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('#');
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
        freeform->set_active_item(0);
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
            case ESCAPE:
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
            // we only add one hotkey per entry, if at any point we want to add
            // multiples, you would need to process them all
            if (selection.at(0)->get_hotkeys().size() == 0)
            {
                // the selection is uninteresting
                continue;
            }

            int selection_key = selection.at(0)->get_hotkeys().at(0);

            bool viable = false;
            switch (selection_key)
            {
            case '#':
                viable = true;
                // intentional fall-through
            case '!':
                _mark_fully_random(ng, ng_choice, viable);
                return;
            case '\t':
                if (_char_defined(defaults))
                {
                    *ng_choice = defaults;
                    return;
                }
                else
                {
                    // ignore Tab because we don't have previous start options
                    continue;
                }
            case ' ':
                ng->species = ng_choice->species = SP_UNKNOWN;
                ng->job     = ng_choice->job     = JOB_UNKNOWN;
                return;
            case '?':
                 // access to the help files
                list_commands('1');
                return _prompt_species(ng, ng_choice, defaults);
            case '%':
                list_commands('%');
                return _prompt_species(ng, ng_choice, defaults);
            case '+':
                ng_choice->species = SP_VIABLE;
                return;
            case '*':
                ng_choice->species = SP_RANDOM;
                return;
            default:
                // we have a species selection
                species_type species = get_species(letter_to_index(selection_key));
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

    for (int i = 0; i < NUM_JOBS; ++i) {
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
        tmp->set_description_text(getGameStartDescription(get_job_name(job)));

        menu->attach_item(tmp);
        tmp->set_visible(true);
        if (defaults.job == job)
        {
            menu->set_active_item(tmp);
        }
    }

    // Add all the special button entries
    if (ng->species != SP_UNKNOWN)
    {
        tmp = new TextItem();
        tmp->set_text("+ - Viable background");
        min_coord.x = X_MARGIN;
        min_coord.y = SPECIAL_KEYS_START_Y;
        max_coord.x = min_coord.x + tmp->get_text().size();
        max_coord.y = min_coord.y + 1;
        tmp->set_bounds(min_coord, max_coord);
        tmp->set_fg_colour(BROWN);
        tmp->add_hotkey('+');
        tmp->set_highlight_colour(LIGHTGRAY);
        tmp->set_description_text("Picks a random viable background based on your current species choice");
        menu->attach_item(tmp);
        tmp->set_visible(true);
    }

    tmp = new TextItem();
    tmp->set_text("# - Viable character");
    min_coord.x = X_MARGIN;
    min_coord.y = SPECIAL_KEYS_START_Y + 1;
    max_coord.x = min_coord.x + tmp->get_text().size();
    max_coord.y = min_coord.y + 1;
    tmp->set_bounds(min_coord, max_coord);
    tmp->set_fg_colour(BROWN);
    tmp->add_hotkey('#');
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
        freeform->set_active_item(0);
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
            case ESCAPE:
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
            // we only add one hotkey per entry, if at any point we want to add
            // multiples, you would need to process them all
            if (selection.at(0)->get_hotkeys().size() == 0)
            {
                // the selection is uninteresting
                continue;
            }
            int selection_key = selection.at(0)->get_hotkeys().at(0);

            bool viable = false;
            switch (selection_key)
            {
            case '#':
                viable = true;
                // intentional fall-through
            case '!':
                _mark_fully_random(ng, ng_choice, viable);
                return;
            case '\t':
                if (_char_defined(defaults))
                {
                    *ng_choice = defaults;
                    return;
                }
                else
                {
                    // ignore Tab because we don't have previous start options
                    continue;
                }
            case ' ':
                ng->species = ng_choice->species = SP_UNKNOWN;
                ng->job     = ng_choice->job     = JOB_UNKNOWN;
                return;
            case '?':
                 // access to the help files
                list_commands('1');
                return _prompt_job(ng, ng_choice, defaults);
            case '%':
                list_commands('%');
                return _prompt_job(ng, ng_choice, defaults);
            case '+':
                ng_choice->job = JOB_VIABLE;
                return;
            case '*':
                ng_choice->job = JOB_RANDOM;
                return;
            default:
                // we have a job selection
                job_type job = get_job(letter_to_index(selection_key));
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

static bool _prompt_weapon(const newgame_def* ng, newgame_def* ng_choice,
                           const newgame_def& defaults,
                           const std::vector<weapon_choice>& weapons)
{
    _print_character_info(ng);

    textcolor( CYAN );
    cprintf("\nYou have a choice of weapons:  "
                "(Press %% for a list of aptitudes)\n");

    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        if (weapons[i].second == CC_UNRESTRICTED)
            textcolor(LIGHTGREY);
        else
            textcolor(DARKGREY);

        const char letter = 'a' + i;
        cprintf("%c - %s\n", letter,
                weapons[i].first == WPN_UNARMED
                  ? "claws" : weapon_base_name(weapons[i].first));
    }

    weapon_type defweapon = _fixup_weapon(defaults.weapon, weapons);

    textcolor(BROWN);
    cprintf("\n* - Random choice; + - Viable random choice; "
                "Bksp - Back to species and background selection; "
                "X - Quit\n");

    if (defweapon != WPN_UNKNOWN)
    {
        cprintf("; Enter - %s",
                defweapon == WPN_RANDOM  ? "Random" :
                defweapon == WPN_VIABLE  ? "Viable" :
                defweapon == WPN_UNARMED ? "claws"  :
                weapon_base_name(defweapon));
    }
    cprintf("\n");

    while (true)
    {
        textcolor(CYAN);
        cprintf("\nWhich weapon? ");
        textcolor(LIGHTGREY);

        int keyin = getch_ck();

        switch (keyin)
        {
        case 'X':
            cprintf("\nGoodbye!");
            end(0);
            break;
        case CK_BKSP:
        case CK_ESCAPE:
        case ' ':
            return (false);
        case '\r':
        case '\n':
            if (defweapon != WPN_UNKNOWN)
            {
                ng_choice->weapon = defweapon;
                return (true);
            }
            else
                continue;
        case '+':
            ng_choice->weapon = WPN_VIABLE;
            return (true);
        case '*':
            ng_choice->weapon = WPN_RANDOM;
            return (true);
        case '%':
            list_commands('%');
            return _prompt_weapon(ng, ng_choice, defaults, weapons);
        default:
            if (keyin - 'a' >= 0 && keyin - 'a' < (int)weapons.size())
            {
                ng_choice->weapon = weapons[keyin - 'a'].first;
                return (true);
            }
            else
                continue;
       }
    }
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
    switch (ng->job)
    {
    case JOB_FIGHTER:
    case JOB_GLADIATOR:
    case JOB_CHAOS_KNIGHT:
    case JOB_DEATH_KNIGHT:
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

startup_book_type _fixup_book(startup_book_type book, int numbooks)
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

std::string _startup_book_name(startup_book_type book)
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

// Returns false if the user aborted.
static bool _prompt_book(const newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults,
                         book_type firstbook, int numbooks)
{
    clrscr();

    _print_character_info(ng);

    textcolor(CYAN);
    cprintf("\nYou have a choice of books:  "
                "(Press %% for a list of aptitudes)\n");

    item_def book;
    book.base_type = OBJ_BOOKS;
    book.quantity  = 1;
    book.plus      = 0;
    book.plus2     = 0;
    book.special   = 1;
    for (int i = 0; i < numbooks; ++i)
    {
        book.sub_type = firstbook + i;
        startup_book_type sb = static_cast<startup_book_type>(i);
        if (book_restriction(sb, *ng) == CC_UNRESTRICTED)
            textcolor(LIGHTGREY);
        else
            textcolor(DARKGREY);

        cprintf("%c - %s\n", 'a' + i,
                book.name(DESC_PLAIN, false, true).c_str());
    }

    textcolor(BROWN);
    cprintf("\n* - Random choice; + - Viable random choice; "
                "Bksp - Back to species and background selection; "
                "X - Quit\n");

    startup_book_type defbook = _fixup_book(defaults.book, numbooks);

    if (defbook != SBT_NONE)
        cprintf("; Enter - %s", _startup_book_name(defbook).c_str());
    cprintf("\n");

    while (true)
    {
        textcolor(CYAN);
        cprintf("\nWhich book? ");
        textcolor(LIGHTGREY);

        int keyin = getch_ck();

        switch (keyin)
        {
        case 'X':
            cprintf("\nGoodbye!");
            end(0);
            break;
        case CK_BKSP:
        case ESCAPE:
        case ' ':
            return (false);
        case '\r':
        case '\n':
            if (defbook != SBT_NONE)
            {
                ng_choice->book = defbook;
                return (true);
            }
            else
                continue;
        case '%':
            list_commands('%');
            return _prompt_book(ng, ng_choice, defaults, firstbook, numbooks);
        case '+':
            ng_choice->book = SBT_VIABLE;
            return (true);
        case '*':
            ng_choice->book = SBT_RANDOM;
            return (true);
        default:
            if (keyin >= 'a' && keyin < 'a' + numbooks)
            {
                ng_choice->book = static_cast<startup_book_type>(keyin - 'a');
                return (true);
            }
            else
                continue;
        }
    }
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

static bool _prompt_god(const newgame_def* ng, newgame_def* ng_choice,
                        const newgame_def& defaults,
                        const std::vector<god_choice>& gods)
{
    _print_character_info(ng);

    textcolor(CYAN);
    cprintf("\nWhich god do you wish to serve?\n");

    for (unsigned int i = 0; i < gods.size(); ++i)
    {
        if (gods[i].second == CC_UNRESTRICTED)
            textcolor(LIGHTGREY);
        else
            textcolor(DARKGREY);

        const char letter = 'a' + i;
        cprintf("%c - %s\n", letter, _god_text(gods[i].first).c_str());
    }

    textcolor(BROWN);
    cprintf("\n* - Random choice; + - Good random choice\n"
                "Bksp - Back to species and background selection; "
                "X - Quit\n");

    const god_type defgod = _fixup_god(defaults.religion, gods);
    if (defgod != GOD_NO_GOD)
    {
        textcolor(BROWN);
        cprintf("\nEnter - %s\n", defgod == GOD_RANDOM ? "Random" :
                                  defgod == GOD_VIABLE ? "Viable" :
                                  god_name(defgod).c_str());
        textcolor(LIGHTGREY);
    }

    while (true)
    {
        int keyn = getch_ck();

        switch (keyn)
        {
        case 'X':
            cprintf("\nGoodbye!");
            end(0);
            break;
        case CK_BKSP:
        case CK_ESCAPE:
        case ' ':
            return (false);
        case '\r':
        case '\n':
            if (defgod != GOD_NO_GOD)
            {
                ng_choice->religion = defgod;
                return (true);
            }
            else
                continue;
        case '+':
            ng_choice->religion = GOD_VIABLE;
            return (true);
        case '*':
            ng_choice->religion = GOD_RANDOM;
            return (true);
        default:
            if (keyn - 'a' >= 0 && keyn - 'a' < (int)gods.size())
            {
                ng_choice->religion = gods[keyn - 'a'].first;
                return (true);
            }
            else
                continue;
        }
    }
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

static bool _prompt_wand(const newgame_def* ng, newgame_def* ng_choice,
                         const newgame_def& defaults)
{
    _print_character_info(ng);

    textcolor(CYAN);
    cprintf("\nYou have a choice of tools:\n"
                "(Press %% for a list of aptitudes)\n");

    for (int i = 0; i < NUM_STARTUP_WANDS; i++)
    {
        startup_wand_type sw = static_cast<startup_wand_type>(i);
        textcolor(LIGHTGREY);

        const char letter = 'a' + i;

        if (sw == SWT_STRIKING)
        {
            item_def rod;
            make_rod(rod, STAFF_STRIKING, 8);
            cprintf("%c - %s\n", letter,
                    rod.name(DESC_QUALNAME, false, true).c_str());
        }
        else
        {
            bool dummy;
            wand_type w = static_cast<wand_type>(start_to_wand(sw, dummy));
            cprintf("%c - %s\n", letter, wand_type_name(w));
        }
    }

    textcolor(BROWN);
    cprintf("\n* - Random choice; "
                "Bksp - Back to species and background selection; "
                "X - Quit\n");

    startup_wand_type defwand = defaults.wand;

    if (defwand != SWT_NO_SELECTION)
    {
        cprintf("; Enter - %s",
                defwand == SWT_ENSLAVEMENT ? "Enslavement" :
                defwand == SWT_CONFUSION   ? "Confusion"   :
                defwand == SWT_MAGIC_DARTS ? "Magic Darts" :
                defwand == SWT_FROST       ? "Frost"       :
                defwand == SWT_FLAME       ? "Flame"       :
                defwand == SWT_STRIKING    ? "Striking"    :
                defwand == SWT_RANDOM      ? "Random"      :
                                             "Buggy");
    }

    cprintf("\n");

    while (true)
    {
        textcolor(CYAN);
        cprintf("\nWhich tool? ");
        textcolor(LIGHTGREY);

        int keyin = getch_ck();

        switch (keyin)
        {
        case 'X':
            cprintf("\nGoodbye!");
            end(0);
            break;
        case CK_BKSP:
        case CK_ESCAPE:
        case ' ':
            return (false);
        case '\r':
        case '\n':
            if (defwand != SWT_NO_SELECTION)
            {
                ng_choice->wand = defwand;
                return (true);
            }
            else
                continue;
        case '%':
            list_commands('%');
            return _prompt_wand(ng, ng_choice, defaults);
        case '*':
            ng_choice->wand = SWT_RANDOM;
            return (true);
        default:
            if (keyin - 'a' >= 0 && keyin - 'a' < NUM_STARTUP_WANDS)
            {
                ng_choice->wand = static_cast<startup_wand_type>(keyin - 'a');
                return (true);
            }
            else
                continue;
        }
    }
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

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
#include "species.h"
#include "state.h"
#include "stuff.h"
#include "tutorial.h"

static bool _choose_species(newgame_def* ng);
static bool _choose_job(newgame_def* ng);
static bool _choose_weapon(newgame_def* ng);
static bool _choose_book(newgame_def* ng);
static bool _choose_god(newgame_def* ng);
static bool _choose_wand(newgame_def* ng);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

newgame_def::newgame_def()
    : name(), species(SP_UNKNOWN), job(JOB_UNKNOWN),
      weapon(NUM_WEAPONS), book(SBT_NO_SELECTION),
      religion(GOD_NO_GOD), wand(SWT_NO_SELECTION)
{
}

static char ng_race, ng_cls;
static bool ng_random;
static int  ng_ck;
static int  ng_weapon;
static int  ng_book;
static int  ng_wand;
static god_type ng_pr;

static void _reset_newgame_options(void)
{
    ng_race   = ng_cls = 0;
    ng_random = false;
    ng_ck     = GOD_NO_GOD;
    ng_pr     = GOD_NO_GOD;
    ng_weapon = WPN_UNKNOWN;
    ng_book   = SBT_NO_SELECTION;
    ng_wand   = SWT_NO_SELECTION;
}

static void _save_newgame_options(void)
{
    // Note that we store species and job directly here, whereas up until
    // now we've been storing the hotkey.
    Options.prev_race       = ng_race;
    Options.prev_cls        = ng_cls;
    Options.prev_randpick   = ng_random;
    Options.prev_ck         = ng_ck;
    Options.prev_pr         = ng_pr;
    Options.prev_weapon     = ng_weapon;
    Options.prev_book       = ng_book;
    Options.prev_wand       = ng_wand;

    write_newgame_options_file();
}

static void _set_startup_options(void)
{
    Options.race         = Options.prev_race;
    Options.cls          = Options.prev_cls;
    Options.chaos_knight = Options.prev_ck;
    Options.priest       = Options.prev_pr;
    Options.weapon       = Options.prev_weapon;
    Options.book         = Options.prev_book;
    Options.wand         = Options.prev_wand;
}

static bool _prev_startup_options_set(void)
{
    // Are these two enough? They should be, in theory, since we'll
    // remember the player's weapon and god choices.
    return (Options.prev_race && Options.prev_cls);
}

static std::string _get_opt_species_name(char species)
{
    species_type pspecies = get_species(letter_to_index(species));
    return (pspecies == SP_UNKNOWN ? "Random" : species_name(pspecies, 1));
}

static std::string _get_opt_job_name(char ojob)
{
    job_type pjob = get_job(letter_to_index(ojob));
    return (pjob == JOB_UNKNOWN ? "Random" : get_job_name(pjob));
}

static std::string _prev_startup_description(void)
{
    if (Options.prev_race == '*' && Options.prev_cls == '*')
        Options.prev_randpick = true;

    if (Options.prev_randpick)
        return "Random character";

    if (Options.prev_cls == '?')
        return "Random " + _get_opt_species_name(Options.prev_race);

    return _get_opt_species_name(Options.prev_race) + " " +
           _get_opt_job_name(Options.prev_cls);
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
static bool _is_species_valid_choice(species_type species, bool display = true)
{
    if (species < 0 || species > NUM_SPECIES)
        return (false);

    if (species >= SP_ELF) // These are all invalid.
        return (false);

    // No problem with these.
    if (species <= SP_RED_DRACONIAN || species > SP_BASE_DRACONIAN)
        return (true);

    // Draconians other than red return false if display == true.
    return (!display);
}

static void _pick_random_species_and_job(newgame_def* ng,
                                         bool unrestricted_only)
{
    // We pick both species and job at the same time to give each
    // valid possibility a fair chance.  For proof that this will
    // work correctly see the proof in religion.cc:handle_god_time().
    int job_count = 0;

    species_type species = SP_UNKNOWN;
    job_type job = JOB_UNKNOWN;

    // For each valid (species, job) choose one randomly.
    for (int sp = 0; sp < NUM_SPECIES; sp++)
    {
        // We only want draconians counted once in this loop...
        // We'll add the variety lower down -- bwr
        if (!_is_species_valid_choice(static_cast<species_type>(sp)))
            continue;

        for (int cl = JOB_FIGHTER; cl < NUM_JOBS; cl++)
        {
            if (is_good_combination(static_cast<species_type>(sp),
                                    static_cast<job_type>(cl),
                                    unrestricted_only))
            {
                job_count++;
                if (one_chance_in(job_count))
                {
                    species = static_cast<species_type>(sp);
                    job = static_cast<job_type>(cl);
                }
            }
        }
    }

    // At least one job must exist in the game, else we're in big trouble.
    ASSERT(species != SP_UNKNOWN && job != JOB_UNKNOWN);

    // Return draconian variety here.
    if (species == SP_RED_DRACONIAN)
        ng->species = random_draconian_player_species();
    else
        ng->species = species;

    ng->job = job;
}

// Returns true if a save game exists with given name.
static bool _check_saved_game(const std::string& name)
{
    FILE *handle;

    std::string basename = get_savedir_filename(name, "", "");
    std::string savename = basename + ".chr";

#ifdef LOAD_UNPACKAGE_CMD
    std::string zipname = basename + PACKAGE_SUFFIX;
    handle = fopen(zipname.c_str(), "rb+");
    if (handle != NULL)
    {
        fclose(handle);

        // Create command.
        char cmd_buff[1024];

        std::string directory = get_savedir();

        escape_path_spaces(basename);
        escape_path_spaces(directory);
        snprintf( cmd_buff, sizeof(cmd_buff), LOAD_UNPACKAGE_CMD,
                  basename.c_str(), directory.c_str() );

        if (system( cmd_buff ) != 0)
        {
            cprintf( "\nWarning: Zip command (LOAD_UNPACKAGE_CMD) "
                         "returned non-zero value!\n" );
        }

        // Remove save game package.
        unlink(zipname.c_str());
    }
#endif

    handle = fopen(savename.c_str(), "rb+");

    if (handle != NULL)
    {
        fclose(handle);
        return (true);
    }
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

static void _setup_tutorial_character(newgame_def* ng)
{
    ng->species = SP_HIGH_ELF;
    ng->job = JOB_FIGHTER;

    Options.weapon = WPN_MACE;
}

static void _choose_species_job(newgame_def* ng)
{
    if (Options.random_pick)
    {
        _pick_random_species_and_job(ng, Options.good_random);
        ng_random = true;
    }
    else if (crawl_state.game_is_tutorial())
    {
        _setup_tutorial_character(ng);
    }
    else
    {
        if (Options.race != 0 && Options.cls != 0
            && Options.race != '*' && Options.cls != '*'
            && !job_allowed(get_species(letter_to_index(Options.race)),
                            get_job(letter_to_index(Options.cls))))
        {
            end(1, false,
                "Incompatible species and background specified in options file.");
        }
        // Repeat until valid species/background combination found.
        while (_choose_species(ng) && _choose_job(ng));
    }
}

// For completely random combinations (!, #, or Options.random_pick)
// reroll characters until the player accepts one of them or quits.
static bool _reroll_random(newgame_def* ng)
{
    if (Options.random_pick)
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
        if (tolower(c) == 'n')
            return (true);
    }
    return (false);
}

static void _choose_char(newgame_def* ng)
{
    while (true)
    {
        _reset_newgame_options();

        _choose_species_job(ng);

        if (_reroll_random(ng))
            continue;

        if (_choose_weapon(ng) && _choose_book(ng)
            && _choose_god(ng) && _choose_wand(ng))
        {
            // We're done!
            return;
        }

        // Else choose again, name stays same.
        const std::string old_name = ng->name;

        Options.prev_randpick = false;
        Options.prev_race     = ng_race;
        Options.prev_cls      = ng_cls;
        Options.prev_weapon   = ng_weapon;
        // ck, pr and book are asked last --> don't need to be changed

        *ng = newgame_def();

        Options.reset_startup_options();

        ng->name = old_name;
    }
}

// Read a choice of game into ng.
// Returns false if a game (with name ng->name) should
// be restored instead of starting a new character.
bool choose_game(newgame_def* ng)
{
    clrscr();

    if (!crawl_state.startup_errors.empty()
        && !Options.suppress_startup_errors)
    {
        crawl_state.show_startup_errors();
        clrscr();
    }

    textcolor(LIGHTGREY);

    if (!ng->name.empty())
    {
        if (_check_saved_game(ng->name))
        {
            Options.prev_name = ng->name;
            save_player_name();
            return (false);
        }
    }

    _choose_char(ng);

    // New: pick name _after_ character choices.
    if (ng->name.empty())
    {
        clrscr();

        std::string specs = species_name(ng->species, 1);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf("You are a%s %s %s.\n",
                (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                get_job_name(ng->job));

        enter_player_name(ng);

        if (_check_saved_game(ng->name))
        {
            cprintf("\nDo you really want to overwrite your old game? ");
            char c = getchm();
            if (c != 'Y' && c != 'y')
                return (false);
        }
    }

    _save_newgame_options();
    return (true);
}

static startup_book_type _book_to_start(int book)
{
    switch (book)
    {
    case BOOK_MINOR_MAGIC_I:
    case BOOK_CONJURATIONS_I:
        return (SBT_FIRE);

    case BOOK_MINOR_MAGIC_II:
    case BOOK_CONJURATIONS_II:
        return (SBT_COLD);

    case BOOK_MINOR_MAGIC_III:
        return (SBT_SUMM);

    default:
        return (SBT_NO_SELECTION);
    }
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

/**
 * Helper function for _choose_species
 * Constructs the menu screen
 */
static const int COLUMN_WIDTH = 25;
static const int X_MARGIN = 4;
static const int SPECIAL_KEYS_START_Y = 21;
static const int CHAR_DESC_START_Y = 18;
static void _construct_species_menu(const newgame_def* ng, MenuFreeform* menu)
{
    ASSERT(menu != NULL);
    static const int ITEMS_IN_COLUMN = 8;
    species_type prev_specie = get_species(letter_to_index(Options.prev_race));
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
        if (prev_specie == species)
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

    if (Options.prev_race)
    {
        if (_prev_startup_options_set())
        {
            std::string tmp_string = "Tab - ";
            tmp_string += _prev_startup_description().c_str();
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
}

// choose_species returns true if the player should also pick a background.
// This is done because of the '!' option which will pick a random
// character, obviating the necessity of choosing a class.
static bool _choose_species(newgame_def* ng)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(0,0), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    int keyn;

    if (Options.cls)
    {
        ng->job = get_job(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor(BROWN);
    cprintf("%s", _welcome(ng).c_str());

    textcolor(YELLOW);
    cprintf(" Please select your species.");

    _construct_species_menu(ng, freeform);
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
    bool loop_flag = true;
    while (loop_flag)
    {
        menu.draw_menu();

        if (Options.race != 0)
        {
            keyn = Options.race;
        }
        else
        {
            keyn = getch_ck();
        }

        bool good_random = false;
        int random_index = -1;

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
                loop_flag = false;
                break;
            case CK_BKSP:
                ng->species  = SP_UNKNOWN;
                Options.race = 0;
                return true;
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

            switch (selection_key)
            {
            case '#':
                good_random = true;
                // intentional fall-through
            case '!':
                _pick_random_species_and_job(ng, good_random);
                Options.random_pick = true; // used to give random weapon/god as well
                ng_random = true;
                if (good_random)
                    Options.good_random = true;
                return false;
            case '\t':
                if (_prev_startup_options_set())
                {
                    if (Options.prev_randpick
                        || Options.prev_race == '*' && Options.prev_cls == '*')
                    {
                        Options.random_pick = true;
                        ng_random = true;
                        _pick_random_species_and_job(ng, Options.good_random);
                        return (false);
                    }
                    _set_startup_options();
                    ng->species = SP_UNKNOWN;
                    ng->job = JOB_UNKNOWN;
                    return true;
                }
                // ignore Tab because we don't have previous start options set
                continue;
            case ' ':
                ng->species  = SP_UNKNOWN;
                Options.race = 0;
                return true;
            case '?':
                 // access to the help files
                list_commands('1');
                return _choose_species(ng);
            case '%':
                list_commands('%');
                return _choose_species(ng);
            case CONTROL('t'):
                // intentional fallthrough
                // TODO: remove these when we have a new start menu
            case 'T':
                if (!crawl_state.game_is_sprint())
                {
                    return !pick_tutorial();
                }
                break;
            case '+':
                good_random = true;
                // intentional fallthrough
            case '*':
                // pick a random allowed specie
                if (ng->job == JOB_UNKNOWN)
                {
                    // pick any specie
                    random_index = random2(ng_num_species());
                    ng->species = get_species(random_index);
                    ng_race = index_to_letter(random_index);
                    return true;
                }
                else
                {
                    // depending on good_random flag, pick either a
                    // viable combo or a random combo
                    do
                    {
                        random_index = random2(ng_num_species());
                    }
                    while (!is_good_combination(get_species(random_index),
                                                ng->job, good_random));
                    ng->species = get_species(random_index);
                    ng_race = index_to_letter(random_index);
                    return true;
                }
            default:
                // we have a species selection
                if (ng->job == JOB_UNKNOWN)
                {
                    // we have no restrictions!
                    ng->species = get_species(letter_to_index(selection_key));
                    // this is probably used for... something
                    ng_race = selection_key;
                    return true; // pick also background
                }
                else
                {
                    // Can we allow this selection?
                    if (job_allowed(get_species(letter_to_index(selection_key)),
                                    ng->job) == CC_BANNED)
                    {
                        // we cannot, repoll for key
                        continue;
                    }
                    else
                    {
                        // we have a valid choice!
                        ng->species = get_species(letter_to_index(selection_key));
                        // this is probably used for... something
                        ng_race = selection_key;
                        return false; // no need to pick background
                    }
                }
                break;
            }
        }
    }
    // we will never reach here
    return true;
}

/**
 * Helper for _choose_job
 * constructs the menu used and highlights the previous job if there is one
 */
static void _construct_backgrounds_menu(const newgame_def* ng,
                                        MenuFreeform* menu)
{
    static const int ITEMS_IN_COLUMN = 10;
    job_type prev_job = get_job(letter_to_index(Options.prev_cls));
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
        if (prev_job == job)
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

    if (Options.prev_race)
    {
        if (_prev_startup_options_set())
        {
            text.clear();
            text = "Tab - ";
            text += _prev_startup_description().c_str();
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
}

/**
 * _choose_job menu
 * returns true if we should also pick a species
 * false if we already have the specie selected
 */
static bool _choose_job(newgame_def* ng)
{
    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(0,0), coord_def(get_number_of_cols(),
                   get_number_of_lines()), "freeform");
    menu.attach_object(freeform);
    menu.set_active_object(freeform);

    int keyn;
    ng_cls = 0;

    if (ng->species != SP_UNKNOWN && ng->job != JOB_UNKNOWN)
        return false;

    if (Options.cls)
    {
        ng->job = get_job(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor(BROWN);
    cprintf("%s", _welcome(ng).c_str());

    textcolor( YELLOW );
    cprintf(" Please select your background.");

    _construct_backgrounds_menu(ng, freeform);
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
    bool loop_flag = true;
    while (loop_flag)
    {
        menu.draw_menu();

        if (Options.cls != 0)
        {
            keyn = Options.cls;
        }
        else
        {
            keyn = getch_ck();
        }

        bool good_random = false;
        int random_index = -1;

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
                loop_flag = false;
                break;
            case CK_BKSP:
                ng->job = JOB_UNKNOWN;
                Options.cls = 0;
                return true;
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

            switch (selection_key)
            {
            case '#':
                good_random = true;
                // intentional fall-through
            case '!':
                _pick_random_species_and_job(ng, good_random);
                Options.random_pick = true; // used to give random weapon/god as well
                ng_random = true;
                if (good_random)
                    Options.good_random = true;
                return false;
            case '\t':
                if (_prev_startup_options_set())
                {
                    if (Options.prev_randpick
                        || Options.prev_race == '*' && Options.prev_cls == '*')
                    {
                        Options.random_pick = true;
                        ng_random = true;
                        _pick_random_species_and_job(ng, Options.good_random);
                        return false;
                    }
                    _set_startup_options();
                    ng->species = SP_UNKNOWN;
                    ng->job = JOB_UNKNOWN;
                    return true;
                }
                // ignore Tab because we don't have previous start options set
                continue;
            case ' ':
                ng->job = JOB_UNKNOWN;
                Options.cls = 0;
                return true;
            case '?':
                 // access to the help files
                list_commands('1');
                return _choose_job(ng);
            case '%':
                list_commands('%');
                return _choose_job(ng);
            case CONTROL('t'):
                // intentional fallthrough
                // TODO: remove these when we have a new start menu
            case 'T':
                if (!crawl_state.game_is_sprint())
                {
                    return !pick_tutorial();
                }
                break;
            case '+':
                good_random = true;
                // intentional fallthrough
            case '*':
                // pick a random allowed background
                if (ng->species == SP_UNKNOWN)
                {
                    // pick any specie
                    random_index = random2(ng_num_jobs());
                    ng->job = get_job(random_index);
                    ng_cls = index_to_letter(random_index);
                    return true;
                }
                else
                {
                    // depending on good_random flag, pick either a
                    // viable combo or a random combo
                    do
                    {
                        random_index = random2(ng_num_species());
                    }
                    while (!is_good_combination(ng->species,
                                                get_job(random_index),
                                                good_random));
                    ng->job = get_job(random_index);
                    ng_cls = index_to_letter(random_index);
                    return true;
                }
            default:
                // we have a background selection
                if (ng->species == SP_UNKNOWN)
                {
                    // we have no restrictions!
                    ng->job = get_job(letter_to_index(selection_key));
                    // this is probably used for... something
                    ng_cls = selection_key;
                    return true; // pick also specie
                }
                else
                {
                    // Can we allow this selection?
                    if (job_allowed(ng->species,
                        get_job(letter_to_index(selection_key)))
                        == CC_BANNED)
                    {
                        // we cannot, repoll for key
                        continue;
                    }
                    else
                    {
                        // we have a valid choice!
                        ng->job = get_job(letter_to_index(selection_key));
                        // this is probably used for... something
                        ng_cls = selection_key;
                        return false; // no need to pick specie
                    }
                }
                break;
            }
        }
    }
    // we will never reach here
    return true;
}

static bool _do_choose_weapon(newgame_def *ng)
{
    weapon_type startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
                                WPN_HAND_AXE, WPN_SPEAR, WPN_UNKNOWN };

    // Gladiators that are at least medium sized get to choose a trident
    // rather than a spear.
    if (ng->job == JOB_GLADIATOR
        && species_size(ng->species, PSIZE_BODY) >= SIZE_MEDIUM)
    {
        startwep[3] = WPN_TRIDENT;
    }

    const bool claws_allowed =
        (ng->job != JOB_GLADIATOR && claws_level(ng->species));

    if (claws_allowed)
    {
        for (int i = 3; i >= 0; --i)
            startwep[i + 1] = startwep[i];

        startwep[0] = WPN_UNARMED;
    }
    else
    {
        switch (ng->species)
        {
        case SP_OGRE:
            startwep[1] = WPN_ANKUS;
            break;
        case SP_MERFOLK:
            startwep[3] = WPN_TRIDENT;
            break;
        default:
            break;
        }
    }

    char_choice_restriction startwep_restrictions[5];
    const int num_choices = (claws_allowed ? 5 : 4);

    for (int i = 0; i < num_choices; i++)
        startwep_restrictions[i] = weapon_restriction(startwep[i], *ng);

    if (Options.weapon == WPN_UNARMED && claws_allowed)
    {
        ng_weapon = Options.weapon;
        ng->weapon = static_cast<weapon_type>(Options.weapon);
        return (true);
    }

    if (Options.weapon != WPN_UNKNOWN && Options.weapon != WPN_RANDOM
        && Options.weapon != WPN_UNARMED)
    {
        // If Options.weapon is available, then use it.
        for (int i = 0; i < num_choices; i++)
        {
            if (startwep[i] == Options.weapon
                && startwep_restrictions[i] != CC_BANNED)
            {
                ng_weapon = Options.weapon;
                ng->weapon = static_cast<weapon_type>(Options.weapon);
                return (true);
            }
        }
    }

    int keyin = 0;
    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        _print_character_info(ng);

        textcolor( CYAN );
        cprintf("\nYou have a choice of weapons:  "
                    "(Press %% for a list of aptitudes)\n");

        bool prevmatch = false;
        for (int i = 0; i < num_choices; i++)
        {
            ASSERT(startwep[i] != WPN_UNKNOWN);

            if (startwep_restrictions[i] == CC_BANNED)
                continue;

            if (startwep_restrictions[i] == CC_UNRESTRICTED)
                textcolor(LIGHTGREY);
            else
                textcolor(DARKGREY);

            const char letter = 'a' + i;
            cprintf("%c - %s\n", letter,
                    startwep[i] == WPN_UNARMED ? "claws"
                                               : weapon_base_name(startwep[i]));

            if (Options.prev_weapon == startwep[i])
                prevmatch = true;
        }

        if (!prevmatch && Options.prev_weapon != WPN_RANDOM)
            Options.prev_weapon = WPN_UNKNOWN;

        textcolor(BROWN);
        cprintf("\n* - Random choice; + - Good random choice; "
                    "Bksp - Back to species and background selection; "
                    "X - Quit\n");

        if (prevmatch || Options.prev_weapon == WPN_RANDOM)
        {
            cprintf("; Enter - %s",
                    Options.prev_weapon == WPN_RANDOM  ? "Random" :
                    Options.prev_weapon == WPN_UNARMED ? "claws"  :
                    weapon_base_name(Options.prev_weapon));
        }
        cprintf("\n");

        do
        {
            textcolor( CYAN );
            cprintf("\nWhich weapon? ");
            textcolor( LIGHTGREY );

            keyin = getch_ck();

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
                if (Options.prev_weapon != WPN_UNKNOWN)
                {
                    if (Options.prev_weapon == WPN_RANDOM)
                        keyin = '*';
                    else
                    {
                        for (int i = 0; i < num_choices; ++i)
                             if (startwep[i] == Options.prev_weapon)
                                 keyin = 'a' + i;
                    }
                }
                break;
            case '%':
                list_commands('%');
                return _do_choose_weapon(ng);
            default:
                break;
           }
        }
        while (keyin != '*' && keyin != '+'
               && (keyin < 'a' || keyin >= ('a' + num_choices)
                   || startwep_restrictions[keyin - 'a'] == CC_BANNED));
    }

    if (Options.random_pick || Options.weapon == WPN_RANDOM
        || keyin == '*' || keyin == '+')
    {
        Options.weapon = WPN_RANDOM;
        ng_weapon = WPN_RANDOM;

        int good_choices = 0;
        if (keyin == '+' || Options.good_random && keyin != '*')
        {
            for (int i = 0; i < num_choices; i++)
            {
                if (weapon_restriction(startwep[i], *ng) == CC_UNRESTRICTED
                    && one_chance_in(++good_choices))
                {
                    keyin = i;
                }
            }
        }

        if (!good_choices)
            keyin = random2(num_choices);

        keyin += 'a';
    }
    else
        ng_weapon = startwep[keyin - 'a'];

    ng->weapon = startwep[keyin - 'a'];

    return (true);
}

// Returns false if aborted, else an actual weapon choice
// is written to ng->weapon for the jobs that call
// _update_weapon() later.
static bool _choose_weapon(newgame_def* ng)
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
        return (_do_choose_weapon(ng));
    default:
        return (true);
    }
}

// firstbook/numbooks required to prompt with the actual book
// names.
// Returns false if aborted, else an actual book choice
// is written to ng->book.
static bool _choose_book(newgame_def* ng, int firstbook, int numbooks)
{
    clrscr();

    item_def book;
    book.base_type = OBJ_BOOKS;
    book.sub_type  = firstbook;
    book.quantity  = 1;
    book.plus      = 0;
    book.plus2     = 0;
    book.special   = 1;

    // Assume a choice of no more than three books, and that all book
    // choices are contiguous.
    ASSERT(numbooks >= 1 && numbooks <= 3);
    char_choice_restriction book_restrictions[3];
    for (int i = 0; i < numbooks; i++)
    {
        book_restrictions[i] = book_restriction(
                                   _book_to_start(firstbook + i), *ng);
    }

    if (Options.book)
    {
        const int opt_book = start_to_book(firstbook, Options.book);
        if (opt_book != -1)
        {
            book.sub_type = opt_book;
            ng_book = Options.book;
            ng->book = static_cast<startup_book_type>(Options.book);
            return (true);
        }
    }

    if (Options.prev_book)
    {
        if (start_to_book(firstbook, Options.prev_book) == -1
            && Options.prev_book != SBT_RANDOM)
        {
            Options.prev_book = SBT_NO_SELECTION;
        }
    }

    int keyin = 0;
    if (!Options.random_pick && Options.book != SBT_RANDOM)
    {
        _print_character_info(ng);

        textcolor( CYAN );
        cprintf("\nYou have a choice of books:  "
                    "(Press %% for a list of aptitudes)\n");

        for (int i = 0; i < numbooks; ++i)
        {
            book.sub_type = firstbook + i;

            if (book_restrictions[i] == CC_UNRESTRICTED)
                textcolor(LIGHTGREY);
            else
                textcolor(DARKGREY);

            cprintf("%c - %s\n", 'a' + i,
                    book.name(DESC_PLAIN, false, true).c_str());
        }

        textcolor(BROWN);
        cprintf("\n* - Random choice; + - Good random choice; "
                    "Bksp - Back to species and background selection; "
                    "X - Quit\n");

        if (Options.prev_book != SBT_NO_SELECTION)
        {
            cprintf("; Enter - %s",
                    Options.prev_book == SBT_FIRE   ? "Fire"      :
                    Options.prev_book == SBT_COLD   ? "Cold"      :
                    Options.prev_book == SBT_SUMM   ? "Summoning" :
                    Options.prev_book == SBT_RANDOM ? "Random"
                                                    : "Buggy Book");
        }
        cprintf("\n");

        do
        {
            textcolor( CYAN );
            cprintf("\nWhich book? ");
            textcolor( LIGHTGREY );

            keyin = getch_ck();

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
                if (Options.prev_book != SBT_NO_SELECTION)
                {
                    if (Options.prev_book == SBT_RANDOM)
                        keyin = '*';
                    else
                    {
                        keyin = 'a'
                                + start_to_book(firstbook, Options.prev_book)
                                - firstbook;
                    }
                }
                break;
            case '%':
                list_commands('%');
                return _choose_book(ng, firstbook, numbooks);
            default:
                break;
            }
        }
        while (keyin != '*' && keyin != '+'
               && (keyin < 'a' || keyin >= ('a' + numbooks)));
    }

    if (Options.random_pick || Options.book == SBT_RANDOM || keyin == '*'
        || keyin == '+')
    {
        ng_book = SBT_RANDOM;

        int good_choices = 0;
        if (keyin == '+'
            || Options.good_random
               && (Options.random_pick || Options.book == SBT_RANDOM))
        {
            for (int i = 0; i < numbooks; i++)
            {
                if (book_restriction(_book_to_start(firstbook + i), *ng)
                            == CC_UNRESTRICTED
                        && one_chance_in(++good_choices))
                {
                    keyin = i;
                }
            }
        }

        if (!good_choices)
            keyin = random2(numbooks);

        keyin += 'a';
    }
    else
        ng_book = _book_to_start(keyin - 'a' + firstbook);

    ng->book = _book_to_start(firstbook + keyin - 'a');
    return (true);
}

static bool _choose_book(newgame_def* ng)
{
    switch (ng->job)
    {
    case JOB_REAVER:
    case JOB_CONJURER:
        return (_choose_book(ng, BOOK_CONJURATIONS_I, 2));
    case JOB_WIZARD:
        return (_choose_book(ng, BOOK_MINOR_MAGIC_I, 3));
    default:
        return (true);
    }
}

static bool _choose_priest(newgame_def* ng)
{
    if (ng->species == SP_DEMONSPAWN || ng->species == SP_MUMMY
        || ng->species == SP_GHOUL || ng->species == SP_VAMPIRE)
    {
        ng->religion = GOD_YREDELEMNUL;
    }
    else
    {
        const god_type gods[3] = { GOD_ZIN, GOD_YREDELEMNUL, GOD_BEOGH };

        // Disallow invalid choices.
        if (religion_restriction(Options.priest, *ng) == CC_BANNED)
            Options.priest = GOD_NO_GOD;

        if (Options.priest != GOD_NO_GOD && Options.priest != GOD_RANDOM)
            ng_pr = ng->religion = static_cast<god_type>(Options.priest);
        else if (Options.random_pick || Options.priest == GOD_RANDOM)
        {
            bool did_chose = false;
            if (Options.good_random)
            {
                int count = 0;
                for (int i = 0; i < 3; i++)
                {
                    if (religion_restriction(gods[i], *ng) == CC_BANNED)
                        continue;

                    if (religion_restriction(gods[i], *ng) == CC_UNRESTRICTED
                        && one_chance_in(++count))
                    {
                        ng->religion = gods[i];
                        did_chose = true;
                    }
                }
            }

            if (!did_chose)
            {
                ng->religion = (coinflip() ? GOD_YREDELEMNUL : GOD_ZIN);

                // For orcs 50% chance of Beogh instead.
                if (ng->species == SP_HILL_ORC && coinflip())
                    ng->religion = GOD_BEOGH;
            }
            ng_pr = GOD_RANDOM;
        }
        else
        {
            _print_character_info(ng);

            textcolor(CYAN);
            cprintf("\nWhich god do you wish to serve?\n");

            const char* god_name[3] = {"Zin (for traditional priests)",
                                       "Yredelemnul (for priests of death)",
                                       "Beogh (for priests of Orcs)"};

            for (int i = 0; i < 3; i++)
            {
                if (religion_restriction(gods[i], *ng) == CC_BANNED)
                    continue;

                if (religion_restriction(gods[i], *ng) == CC_UNRESTRICTED)
                    textcolor(LIGHTGREY);
                else
                    textcolor(DARKGREY);

                const char letter = 'a' + i;
                cprintf("%c - %s\n", letter, god_name[i]);
            }

            textcolor(BROWN);
            cprintf("\n* - Random choice; + - Good random choice\n"
                        "Bksp - Back to species and background selection; "
                        "X - Quit\n");

            if (religion_restriction(Options.prev_pr, *ng) == CC_BANNED)
                Options.prev_pr = GOD_NO_GOD;

            if (Options.prev_pr != GOD_NO_GOD)
            {
                textcolor(BROWN);
                cprintf("\nEnter - %s\n",
                        Options.prev_pr == GOD_ZIN         ? "Zin" :
                        Options.prev_pr == GOD_YREDELEMNUL ? "Yredelemnul" :
                        Options.prev_pr == GOD_BEOGH       ? "Beogh"
                                                           : "Random");
            }

            int keyn;
            do
            {
                keyn = getch_ck();

                switch (keyn)
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
                    if (Options.prev_pr == GOD_NO_GOD
                        || Options.prev_pr == GOD_BEOGH
                           && ng->species != SP_HILL_ORC)
                    {
                        break;
                    }
                    if (Options.prev_pr != GOD_RANDOM)
                    {
                        Options.prev_pr
                                 = static_cast<god_type>(Options.prev_pr);
                        ng->religion = Options.prev_pr;
                        break;
                    }
                    keyn = '*'; // for ng_pr setting
                    // fall-through for random
                case '+':
                    if (keyn == '+')
                    {
                        int count = 0;
                        for (int i = 0; i < 3; i++)
                        {
                            if (religion_restriction(gods[i], *ng) == CC_BANNED)
                                continue;

                            if (religion_restriction(gods[i], *ng)
                                    == CC_UNRESTRICTED
                                && one_chance_in(++count))
                            {
                                ng->religion = gods[i];
                            }
                        }
                        if (count > 0)
                            break;
                    }
                    // intentional fall-through
                case '*':
                    ng->religion = coinflip() ? GOD_ZIN : GOD_YREDELEMNUL;
                    if (ng->species == SP_HILL_ORC && coinflip())
                        ng->religion = GOD_BEOGH;
                    break;
                case 'a':
                    ng->religion = GOD_ZIN;
                    break;
                case 'b':
                    ng->religion = GOD_YREDELEMNUL;
                    break;
                case 'c':
                    if (ng->species == SP_HILL_ORC)
                    {
                        ng->religion = GOD_BEOGH;
                        break;
                    } // else fall through
                default:
                    break;
                }
            }
            while (ng->religion == GOD_NO_GOD);

            ng_pr = (keyn == '*' ? GOD_RANDOM : ng->religion);
        }
    }
    return (true);
}

static bool _choose_chaos_knight(newgame_def* ng)
{
    const god_type gods[3] = { GOD_XOM, GOD_MAKHLEB, GOD_LUGONU };

    if (Options.chaos_knight != GOD_NO_GOD
        && Options.chaos_knight != GOD_RANDOM)
    {
        ng_ck = ng->religion =
            static_cast<god_type>(Options.chaos_knight);
    }
    else if (Options.random_pick || Options.chaos_knight == GOD_RANDOM)
    {
        bool did_chose = false;
        if (Options.good_random)
        {
            int count = 0;
            for (int i = 0; i < 3; i++)
            {
                if (religion_restriction(gods[i], *ng) == CC_BANNED)
                    continue;

                if (religion_restriction(gods[i], *ng) == CC_UNRESTRICTED
                    && one_chance_in(++count))
                {
                    ng->religion = gods[i];
                    did_chose = true;
                }
            }
        }

        if (!did_chose)
        {
            ng->religion = (one_chance_in(3) ? GOD_XOM :
                           coinflip()       ? GOD_MAKHLEB
                                            : GOD_LUGONU);
        }
        ng_ck = GOD_RANDOM;
    }
    else
    {
        _print_character_info(ng);

        textcolor(CYAN);
        cprintf("\nWhich god of chaos do you wish to serve?\n");

        const char* god_name[3] = {"Xom of Chaos",
                                   "Makhleb the Destroyer",
                                   "Lugonu the Unformed"};

        for (int i = 0; i < 3; i++)
        {
            if (religion_restriction(gods[i], *ng) == CC_BANNED)
                continue;

            if (religion_restriction(gods[i], *ng) == CC_UNRESTRICTED)
                textcolor(LIGHTGREY);
            else
                textcolor(DARKGREY);

            const char letter = 'a' + i;
            cprintf("%c - %s\n", letter, god_name[i]);
        }

        textcolor(BROWN);
        cprintf("\n* - Random choice; + - Good random choice\n"
                    "Bksp - Back to species and background selection; "
                    "X - Quit\n");

        if (Options.prev_ck != GOD_NO_GOD)
        {
            textcolor(BROWN);
            cprintf("\nEnter - %s\n",
                    Options.prev_ck == GOD_XOM     ? "Xom" :
                    Options.prev_ck == GOD_MAKHLEB ? "Makhleb" :
                    Options.prev_ck == GOD_LUGONU  ? "Lugonu"
                                                   : "Random");
            textcolor(LIGHTGREY);
        }

        int keyn;
        do
        {
            keyn = getch_ck();

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
                if (Options.prev_ck == GOD_NO_GOD)
                    break;

                if (Options.prev_ck != GOD_RANDOM)
                {
                    ng->religion = static_cast<god_type>(Options.prev_ck);
                    break;
                }
                keyn = '*'; // for ng_ck setting
                // fall-through for random
            case '+':
                if (keyn == '+')
                {
                    int count = 0;
                    for (int i = 0; i < 3; i++)
                    {
                        if (religion_restriction(gods[i], *ng) == CC_BANNED)
                            continue;

                        if (religion_restriction(gods[i], *ng)
                                == CC_UNRESTRICTED
                            && one_chance_in(++count))
                        {
                            ng->religion = gods[i];
                        }
                    }
                    if (count > 0)
                        break;
                }
                // intentional fall-through
            case '*':
                ng->religion = (one_chance_in(3) ? GOD_XOM :
                               coinflip()       ? GOD_MAKHLEB
                                                : GOD_LUGONU);
                break;
            case 'a':
                ng->religion = GOD_XOM;
                break;
            case 'b':
                ng->religion = GOD_MAKHLEB;
                break;
            case 'c':
                ng->religion = GOD_LUGONU;
                break;
            default:
                break;
            }
        }
        while (ng->religion == GOD_NO_GOD);

        ng_ck = (keyn == '*') ? GOD_RANDOM
                              : ng->religion;
    }
    return (true);
}

// For jobs with god choice, fill in ng->religion.
static bool _choose_god(newgame_def* ng)
{
    switch (ng->job)
    {
    case JOB_CHAOS_KNIGHT:
        return (_choose_chaos_knight(ng));
    case JOB_PRIEST:
        return (_choose_priest(ng));
    default:
        return (true);
    }
}

static startup_wand_type _wand_to_start(int wand, bool is_rod)
{
    if (!is_rod)
    {
        switch (wand)
        {
        case WAND_ENSLAVEMENT:
            return (SWT_ENSLAVEMENT);

        case WAND_CONFUSION:
            return (SWT_CONFUSION);

        case WAND_MAGIC_DARTS:
            return (SWT_MAGIC_DARTS);

        case WAND_FROST:
            return (SWT_FROST);

        case WAND_FLAME:
            return (SWT_FLAME);

        default:
            return (SWT_NO_SELECTION);
        }
    }
    else
    {
        switch (wand)
        {
        case STAFF_STRIKING:
            return (SWT_STRIKING);

        default:
            return (SWT_NO_SELECTION);
        }
    }
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

static bool _choose_wand(newgame_def* ng)
{
    if (ng->job != JOB_ARTIFICER)
        return (true);

    // Wand-choosing interface for Artificers -- Greenberg/Bane

    const wand_type startwand[5] = { WAND_ENSLAVEMENT, WAND_CONFUSION,
                                     WAND_MAGIC_DARTS, WAND_FROST, WAND_FLAME };

    item_def wand;
    make_rod(wand, STAFF_STRIKING, 8);
    const int num_choices = 6;

    int keyin = 0;
    int wandtype;
    bool is_rod;
    if (Options.wand)
    {
        if (start_to_wand(Options.wand, is_rod) != -1)
        {
            keyin = 'a' + Options.wand - 1;
            ng_wand = Options.wand;
            ng->wand = static_cast<startup_wand_type>(Options.wand);
            return (true);
        }
    }

    if (Options.prev_wand)
    {
        if (start_to_wand(Options.prev_wand, is_rod) == -1
            && Options.prev_wand != SWT_RANDOM)
        {
            Options.prev_wand = SWT_NO_SELECTION;
        }
    }

    if (!Options.random_pick && Options.wand != SWT_RANDOM)
    {
        _print_character_info(ng);

        textcolor( CYAN );
        cprintf("\nYou have a choice of tools:\n"
                    "(Press %% for a list of aptitudes)\n");

        bool prevmatch = false;
        for (int i = 0; i < num_choices; i++)
        {
            textcolor(LIGHTGREY);

            const char letter = 'a' + i;
            if (i == num_choices - 1)
            {
                cprintf("%c - %s\n", letter,
                        wand.name(DESC_QUALNAME, false, true).c_str());
                wandtype = wand.sub_type;
                is_rod = true;
            }
            else
            {
                cprintf("%c - %s\n", letter, wand_type_name(startwand[i]));
                wandtype = startwand[i];
                is_rod = false;
            }

            if (Options.prev_wand == _wand_to_start(wandtype, is_rod))
                prevmatch = true;
        }

        if (!prevmatch && Options.prev_wand != SWT_RANDOM)
            Options.prev_wand = SWT_NO_SELECTION;

        textcolor(BROWN);
        cprintf("\n* - Random choice; "
                    "Bksp - Back to species and background selection; "
                    "X - Quit\n");

        if (prevmatch || Options.prev_wand == SWT_RANDOM)
        {
            cprintf("; Enter - %s",
                    Options.prev_wand == SWT_ENSLAVEMENT ? "Enslavement" :
                    Options.prev_wand == SWT_CONFUSION   ? "Confusion"   :
                    Options.prev_wand == SWT_MAGIC_DARTS ? "Magic Darts" :
                    Options.prev_wand == SWT_FROST       ? "Frost"       :
                    Options.prev_wand == SWT_FLAME       ? "Flame"       :
                    Options.prev_wand == SWT_STRIKING    ? "Striking"    :
                    Options.prev_wand == SWT_RANDOM      ? "Random"
                                                         : "Buggy Tool");
        }
        cprintf("\n");

        do
        {
            textcolor( CYAN );
            cprintf("\nWhich tool? ");
            textcolor( LIGHTGREY );

            keyin = getch_ck();

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
                if (Options.prev_wand != SWT_NO_SELECTION)
                {
                    if (Options.prev_wand == SWT_RANDOM)
                        keyin = '*';
                    else
                    {
                        for (int i = 0; i < num_choices; ++i)
                        {
                            if (i == num_choices - 1)
                            {
                                wandtype = wand.sub_type;
                                is_rod = true;
                            }
                            else
                            {
                                wandtype = startwand[i];
                                is_rod = false;
                            }

                            if (Options.prev_wand ==
                                _wand_to_start(wandtype, is_rod))
                            {
                                 keyin = 'a' + i;
                            }
                        }
                    }
                }
                break;
            case '%':
                list_commands('%');
                return _choose_wand(ng);
            default:
                break;
           }
        }
        while (keyin != '*'  && (keyin < 'a' || keyin >= ('a' + num_choices)));
    }

    if (Options.random_pick || Options.wand == SWT_RANDOM || keyin == '*')
    {
        Options.wand = WPN_RANDOM;
        ng_wand = SWT_RANDOM;

        keyin = random2(num_choices);
        keyin += 'a';
    }
    else
        ng_wand = keyin - 'a' + 1;

    ng->wand = static_cast<startup_wand_type>(keyin - 'a' + 1);
    return (true);
}

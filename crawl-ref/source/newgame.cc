/*
 *  File:       newgame.cc
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "newgame.h"
#include "ng-init.h"
#include "ng-input.h"
#include "ng-restr.h"
#include "jobs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <algorithm>

#ifdef UNIX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "externs.h"
#include "options.h"
#include "species.h"

#include "abl-show.h"
#include "artefact.h"
#include "cio.h"
#include "command.h"
#include "describe.h"
#include "dungeon.h"
#include "files.h"
#include "food.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "menu.h"
#include "mutation.h"
#include "misc.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tutorial.h"

#define MIN_START_STAT       3

static void _give_basic_knowledge(job_type which_job);
static void _give_basic_spells(job_type which_job);
static void _give_last_paycheck(job_type which_job);
static void _init_player(void);
static void _jobs_stat_init(job_type which_job);
static void _species_stat_init(species_type which_species);

static void _create_wanderer(void);
static bool _give_items_skills(void);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

static newgame_def ng;

// XXX: temporary to get data in and out of
// purely rewritten functions.
void newgame_def::init(const player &p)
{
    name = p.your_name;
    species = p.species;
    job = p.char_class;
}

void newgame_def::save(player &p)
{
    p.your_name = name;
    p.species = species;
    p.char_class = job;
}

static char ng_race, ng_cls;
static bool ng_random;
static int  ng_ck, ng_dk;
static int  ng_weapon;
static int  ng_book;
static int  ng_wand;
static god_type ng_pr;

static void _reset_newgame_options(void)
{
    ng_race   = ng_cls = 0;
    ng_random = false;
    ng_ck     = GOD_NO_GOD;
    ng_dk     = DK_NO_SELECTION;
    ng_pr     = GOD_NO_GOD;
    ng_weapon = WPN_UNKNOWN;
    ng_book   = SBT_NO_SELECTION;
    ng_wand   = SWT_NO_SELECTION;
}

static void _save_newgame_options(void)
{
    // Note that we store race and class directly here, whereas up until
    // now we've been storing the hotkey.
    Options.prev_race       = ng_race;
    Options.prev_cls        = ng_cls;
    Options.prev_randpick   = ng_random;
    Options.prev_ck         = ng_ck;
    Options.prev_dk         = ng_dk;
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
    Options.death_knight = Options.prev_dk;
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

static std::string _get_opt_race_name(char race)
{
    species_type prace = get_species(letter_to_index(race));
    return (prace == SP_UNKNOWN ? "Random" : species_name(prace, 1));
}

static std::string _get_opt_class_name(char oclass)
{
    job_type pclass = get_class(letter_to_index(oclass));
    return (pclass == JOB_UNKNOWN ? "Random" : get_class_name(pclass));
}

static std::string _prev_startup_description(void)
{
    if (Options.prev_race == '*' && Options.prev_cls == '*')
        Options.prev_randpick = true;

    if (Options.prev_randpick)
        return "Random character";

    if (Options.prev_cls == '?')
        return "Random " + _get_opt_race_name(Options.prev_race);

    return _get_opt_race_name(Options.prev_race) + " " +
           _get_opt_class_name(Options.prev_cls);
}

// Output full character info when weapons/books/religion are chosen.
static void _print_character_info()
{
    clrscr();

    // At this point all of name, species and class should be decided.
    if (!you.your_name.empty()
        && you.char_class != JOB_UNKNOWN && you.species != SP_UNKNOWN)
    {
        cprintf("Welcome, ");
        textcolor( YELLOW );
        cprintf("%s the %s %s." EOL, you.your_name.c_str(), species_name(you.species, 1).c_str(),
                get_class_name(you.char_class));
    }
}

int give_first_conjuration_book()
{
    // Assume the fire/earth book, as conjurations is strong with fire. -- bwr
    int book = BOOK_CONJURATIONS_I;

    // Conjuration books are largely Fire or Ice, so we'll use
    // that as the primary condition, and air/earth to break ties. -- bwr
    if (you.skills[SK_ICE_MAGIC] > you.skills[SK_FIRE_MAGIC]
        || you.skills[SK_FIRE_MAGIC] == you.skills[SK_ICE_MAGIC]
           && you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
    {
        book = BOOK_CONJURATIONS_II;
    }
    else if (you.skills[SK_FIRE_MAGIC] == 0 && you.skills[SK_EARTH_MAGIC] == 0)
    {
        // If we're here it's because we were going to default to the
        // fire/earth book... but we don't have those skills.  So we
        // choose randomly based on the species weighting, again
        // ignoring air/earth which are secondary in these books. - bwr
        if (random2(species_skills(SK_ICE_MAGIC, you.species)) <
            random2(species_skills(SK_FIRE_MAGIC, you.species)))
        {
            book = BOOK_CONJURATIONS_II;
        }
    }

    return (book);
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

static void _pick_random_species_and_class( bool unrestricted_only )
{
    // We pick both species and class at the same time to give each
    // valid possibility a fair chance.  For proof that this will
    // work correctly see the proof in religion.cc:handle_god_time().
    int job_count = 0;

    species_type species = SP_UNKNOWN;
    job_type job = JOB_UNKNOWN;

    // For each valid (species, class) choose one randomly.
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
        you.species = random_draconian_player_species();
    else
        you.species = species;

    you.char_class = job;
}

static bool _check_saved_game(void)
{
    FILE *handle;

    std::string basename = get_savedir_filename(you.your_name, "", "");
    std::string savename = basename + ".sav";

#ifdef LOAD_UNPACKAGE_CMD
    std::string zipname = basename + PACKAGE_SUFFIX;
    handle = fopen(zipname.c_str(), "rb+");
    if (handle != NULL)
    {
        fclose(handle);
        cprintf(EOL "Loading game..." EOL);

        // Create command.
        char cmd_buff[1024];

        std::string directory = get_savedir();

        escape_path_spaces(basename);
        escape_path_spaces(directory);
        snprintf( cmd_buff, sizeof(cmd_buff), LOAD_UNPACKAGE_CMD,
                  basename.c_str(), directory.c_str() );

        if (system( cmd_buff ) != 0)
        {
            cprintf( EOL "Warning: Zip command (LOAD_UNPACKAGE_CMD) "
                         "returned non-zero value!" EOL );
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

static void _give_starting_food()
{
    // These undead start with no food.
    if (you.species == SP_MUMMY || you.species == SP_GHOUL)
        return;

    item_def item;
    item.quantity = 1;
    if (you.species == SP_SPRIGGAN)
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type  = POT_PORRIDGE;
    }
    else if (you.species == SP_VAMPIRE)
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type  = POT_BLOOD;
        init_stack_blood_potions(item);
    }
    else
    {
        item.base_type = OBJ_FOOD;
        if (you.species == SP_HILL_ORC || you.species == SP_KOBOLD
            || player_genus(GENPC_OGRE) || you.species == SP_TROLL)
        {
            item.sub_type = FOOD_MEAT_RATION;
        }
        else
            item.sub_type = FOOD_BREAD_RATION;
    }

    // Give another one for hungry species.
    if (player_mutation_level(MUT_FAST_METABOLISM))
        item.quantity = 2;

    const int slot = find_free_slot(item);
    you.inv[slot] = item;       // will ASSERT if couldn't find free slot
}

static void _mark_starting_books()
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].is_valid() && you.inv[i].base_type == OBJ_BOOKS)
            mark_had_book(you.inv[i]);
}

static void _racialise_starting_equipment()
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (you.inv[i].is_valid())
        {
            // Don't change object type modifier unless it starts plain.
            if ((you.inv[i].base_type == OBJ_ARMOUR
                    || you.inv[i].base_type == OBJ_WEAPONS
                    || you.inv[i].base_type == OBJ_MISSILES)
                && get_equip_race(you.inv[i]) == ISFLAG_NO_RACE)
            {
                // Now add appropriate species type mod.
                // Fighters don't get elven body armour.
                if (player_genus(GENPC_ELVEN)
                    && (you.char_class != JOB_FIGHTER
                        || you.inv[i].base_type != OBJ_ARMOUR
                        || get_armour_slot(you.inv[i]) != EQ_BODY_ARMOUR))
                {
                    set_equip_race(you.inv[i], ISFLAG_ELVEN);
                }
                else if (player_genus(GENPC_DWARVEN))
                    set_equip_race(you.inv[i], ISFLAG_DWARVEN);
                else if (you.species == SP_HILL_ORC)
                    set_equip_race(you.inv[i], ISFLAG_ORCISH);
            }
        }
    }
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
static void _reassess_starting_skills()
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.skills[i] == 0
            && (you.species != SP_VAMPIRE || i != SK_UNARMED_COMBAT))
        {
            continue;
        }

        // Grant the amount of skill points required for a human.
        const int points = skill_exp_needed(you.skills[i]);
        you.skill_points[i] = (points * species_skills(i, SP_HUMAN)) / 100 + 1;

        // Find out what level that earns this character.
        const int sp_diff = species_skills(i, you.species);
        you.skills[i] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[i] > (skill_exp_needed(lvl) * sp_diff) / 100)
                you.skills[i] = lvl;
            else
                break;
        }

        // Vampires should always have Unarmed Combat skill.
        if (you.species == SP_VAMPIRE && i == SK_UNARMED_COMBAT
            && you.skills[i] < 1)
        {
            you.skill_points[i] = (skill_exp_needed(1) * sp_diff) / 100;
            you.skills[i] = 1;
        }

        // Wanderers get at least 1 level in their skills.
        if (you.char_class == JOB_WANDERER && you.skills[i] < 1)
        {
            you.skill_points[i] = (skill_exp_needed(1) * sp_diff) / 100;
            you.skills[i] = 1;
        }

        // Spellcasters should always have Spellcasting skill.
        if (i == SK_SPELLCASTING && you.skills[i] < 1)
        {
            you.skill_points[i] = (skill_exp_needed(1) * sp_diff) / 100;
            you.skills[i] = 1;
        }
    }
}

// Make sure no stats are unacceptably low
// (currently possible only for GhBe -- 1KB)
static void _unfocus_stats()
{
    int needed;

    if ((needed = MIN_START_STAT - you.strength) > 0)
    {
        if (you.intel > you.dex)
            you.intel -= needed;
        else
            you.dex -= needed;

        you.strength = MIN_START_STAT;
    }
    if ((needed = MIN_START_STAT - you.intel) > 0)
    {
        if (you.strength > you.dex)
            you.strength -= needed;
        else
            you.dex -= needed;

        you.intel = MIN_START_STAT;
    }
    if ((needed = MIN_START_STAT - you.dex) > 0)
    {
        if (you.strength > you.intel)
            you.strength -= needed;
        else
            you.intel -= needed;

        you.dex = MIN_START_STAT;
    }
}

// Randomly boost stats a number of times.
static void _wanderer_assign_remaining_stats(int points_left)
{
    while (points_left > 0)
    {
        // Stats that are already high will be chosen half as often.
        switch (random2(NUM_STATS))
        {
        case STAT_STRENGTH:
            if (you.strength > 17 && coinflip())
                continue;

            you.strength++;
            break;

        case STAT_DEXTERITY:
            if (you.dex > 17 && coinflip())
                continue;

            you.dex++;
            break;

        case STAT_INTELLIGENCE:
            if (you.intel > 17 && coinflip())
                continue;

            you.intel++;
            break;
        }

        points_left--;
    }
}

static void _give_species_bonus_hp()
{
    if (player_genus(GENPC_DRACONIAN) || player_genus(GENPC_DWARVEN))
        inc_max_hp(1);
    else
    {
        switch (you.species)
        {
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
            inc_max_hp(3);
            break;

        case SP_GHOUL:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_DEMIGOD:
            inc_max_hp(2);
            break;

        case SP_HILL_ORC:
        case SP_MUMMY:
        case SP_MERFOLK:
            inc_max_hp(1);
            break;

        case SP_HIGH_ELF:
        case SP_VAMPIRE:
            dec_max_hp(1);
            break;

        case SP_DEEP_ELF:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
            dec_max_hp(2);
            break;

        default:
            break;
        }
    }
}

// Adjust max_magic_points by species. {dlb}
static void _give_species_bonus_mp()
{
    switch (you.species)
    {
    case SP_VAMPIRE:
    case SP_SPRIGGAN:
    case SP_DEMIGOD:
    case SP_DEEP_ELF:
        inc_max_mp(1);
        break;

    default:
        break;
    }
}

#ifdef DEBUG
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

// For items that get a random colour, give them a more thematic one.
static void _apply_job_colour(item_def &item)
{
    if (!Options.classic_item_colours)
        return;

    if (item.base_type != OBJ_ARMOUR)
        return;

    switch (item.sub_type)
    {
    case ARM_CLOAK:
    case ARM_ROBE:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    case ARM_CAP:
    case ARM_WIZARD_HAT:
        break;
    default:
        return;
    }

    switch (you.char_class)
    {
    case JOB_THIEF:
    case JOB_NECROMANCER:
    case JOB_ASSASSIN:
        item.colour = DARKGREY;
        break;
    case JOB_FIRE_ELEMENTALIST:
        item.colour = RED;
        break;
    case JOB_ICE_ELEMENTALIST:
        item.colour = BLUE;
        break;
    case JOB_AIR_ELEMENTALIST:
        item.colour = LIGHTBLUE;
        break;
    case JOB_EARTH_ELEMENTALIST:
        item.colour = BROWN;
        break;
    case JOB_VENOM_MAGE:
        item.colour = MAGENTA;
        break;
    default:
        break;
    }
}

bool new_game(void)
{
    clrscr();
    _init_player();

    if (!crawl_state.startup_errors.empty()
        && !Options.suppress_startup_errors)
    {
        crawl_state.show_startup_errors();
        clrscr();
    }

    if (!Options.player_name.empty())
        you.your_name = Options.player_name;

    textcolor(LIGHTGREY);

    // Copy name into you.your_name if set from environment --
    // note that you.your_name could already be set from init.txt.
    // This, clearly, will overwrite such information. {dlb}
    if (!SysEnv.crawl_name.empty())
        you.your_name = SysEnv.crawl_name;

    opening_screen();
    ng.init(you);
    enter_player_name(ng, true);
    ng.save(you);

    if (!you.your_name.empty())
    {
        if (_check_saved_game())
        {
            save_player_name();
            return (false);
        }
    }

game_start:
    _reset_newgame_options();
    if (Options.random_pick)
    {
        _pick_random_species_and_class(Options.good_random);
        ng_random = true;
    }
    else
    {
        if (Options.race != 0 && Options.cls != 0
            && Options.race != '*' && Options.cls != '*'
            && !class_allowed(get_species(letter_to_index(Options.race)),
                               get_class(letter_to_index(Options.cls))))
        {
            end(1, false,
                "Incompatible species and job specified in options file.");
        }
        // Repeat until valid race/class combination found.
        while (choose_race() && !choose_class());
    }

    // Pick random draconian type.
    if (you.species == SP_RED_DRACONIAN)
        you.species = random_draconian_player_species();

    strcpy( you.class_name, get_class_name(you.char_class) );

    if (Options.random_pick)
    {
        // For completely random combinations (!, #, or Options.random_pick)
        // reroll characters until the player accepts one of them or quits.
        clrscr();

        std::string specs = species_name(you.species, you.experience_level);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf( "You are a%s %s %s." EOL,
                 (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                 you.class_name );

        cprintf(EOL "Do you want to play this combination? (ynq) [y]");
        char c = getch();
        if (c == ESCAPE || tolower(c) == 'q')
            end(0);
        if (tolower(c) == 'n')
            goto game_start;
    }

    // New: pick name _after_ race and class choices.
    if (you.your_name.empty())
    {
        clrscr();

        std::string specs = species_name(you.species, you.experience_level);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf( "You are a%s %s %s." EOL,
                 (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                 you.class_name );

        ng.init(you);
        enter_player_name(ng, false);
        ng.save(you);

        if (_check_saved_game())
        {
            cprintf(EOL "Do you really want to overwrite your old game? ");
            char c = getch();
            if (c != 'Y' && c != 'y')
            {
                textcolor( BROWN );
                cprintf(EOL EOL "Welcome back, ");
                textcolor( YELLOW );
                cprintf("%s!", you.your_name.c_str());
                textcolor( LIGHTGREY );

                return (false);
            }
        }
    }


// ************ Round-out character statistics and such. ************

    _species_stat_init( you.species );     // must be down here {dlb}

    you.is_undead = get_undead_state(you.species);

    // Before we get into the inventory init, set light radius based
    // on species vision. Currently, all species see out to 8 squares.
    you.normal_vision  = LOS_RADIUS;
    you.current_vision = LOS_RADIUS;

    _jobs_stat_init( you.char_class );
    _give_last_paycheck( you.char_class );

    _unfocus_stats();

    // Needs to be done before handing out food.
    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    // Returns false if Backspace on god/weapon/... selection.
    if (!_give_items_skills())
    {
        // Now choose again, name stays same.
        const std::string old_name = you.your_name;

        Options.prev_randpick = false;
        Options.prev_race     = ng_race;
        Options.prev_cls      = ng_cls;
        Options.prev_weapon   = ng_weapon;
        // ck, dk, pr and book are asked last --> don't need to be changed

        // Reset stats.
        _init_player();

        Options.reset_startup_options();

        you.your_name = old_name;

        // Choose new character.
        goto game_start;
    }

    _give_species_bonus_hp();
    _give_species_bonus_mp();

    if (you.species == SP_DEMONSPAWN)
        roll_demonspawn_mutations();

    // XXX: These need to be set above using functions!!! {dlb}
    you.max_dex      = you.dex;
    you.max_strength = you.strength;
    you.max_intel    = you.intel;

    _give_starting_food();
    _mark_starting_books();
    _racialise_starting_equipment();

    _give_basic_spells(you.char_class);
    _give_basic_knowledge(you.char_class);

    initialise_item_descriptions();

    _reassess_starting_skills();
    calc_total_skill_points();

    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].is_valid())
        {
            // XXX: Why is this here? Elsewhere it's only ever used for runes.
            you.inv[i].flags |= ISFLAG_BEEN_IN_INV;

            // Identify all items in pack.
            set_ident_type(you.inv[i], ID_KNOWN_TYPE);
            set_ident_flags(you.inv[i], ISFLAG_IDENT_MASK);

            // link properly
            you.inv[i].pos.set(-1, -1);
            you.inv[i].link = i;
            you.inv[i].slot = index_to_letter(you.inv[i].link);
            item_colour(you.inv[i]);  // set correct special and colour
            _apply_job_colour(you.inv[i]);
        }

    // Apply autoinscribe rules to inventory.
    request_autoinscribe();
    autoinscribe();

    // Brand items as original equipment.
    origin_set_inventory(origin_set_startequip);

    // We calculate hp and mp here; all relevant factors should be
    // finalised by now. (GDL)
    calc_hp();
    calc_mp();

    // Make sure the starting player is fully charged up.
    set_hp( you.hp_max, false );
    set_mp( you.max_magic_points, false );

    // tmpfile purging removed in favour of marking
    Generated_Levels.clear();

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();

    // Generate the second name of Jiyva
    fix_up_jiyva_name();

    _save_newgame_options();

    // Pretend that a savefile was just loaded, in order to
    // get things setup properly.
    SavefileCallback::post_restore();

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

static int _start_to_book(int firstbook, int booktype)
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

static bool _choose_book( int slot, int firstbook, int numbooks )
{
    clrscr();

    ng.init(you); // XXX

    item_def &book(you.inv[slot]);
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
                                   _book_to_start(firstbook + i), ng);
    }

    if (Options.book)
    {
        const int opt_book = _start_to_book(firstbook, Options.book);
        if (opt_book != -1)
        {
            book.sub_type = opt_book;
            ng_book = Options.book;
            return (true);
        }
    }

    if (Options.prev_book)
    {
        if (_start_to_book(firstbook, Options.prev_book) == -1
            && Options.prev_book != SBT_RANDOM)
        {
            Options.prev_book = SBT_NO_SELECTION;
        }
    }

    int keyin = 0;
    if (!Options.random_pick && Options.book != SBT_RANDOM)
    {
        _print_character_info();

        textcolor( CYAN );
        cprintf(EOL "You have a choice of books:  "
                    "(Press %% for a list of aptitudes)" EOL);

        for (int i = 0; i < numbooks; ++i)
        {
            book.sub_type = firstbook + i;

            if (book_restrictions[i] == CC_UNRESTRICTED)
                textcolor(LIGHTGREY);
            else
                textcolor(DARKGREY);

            cprintf("%c - %s" EOL, 'a' + i,
                    book.name(DESC_PLAIN, false, true).c_str());
        }

        textcolor(BROWN);
        cprintf(EOL "* - Random choice; + - Good random choice; "
                    "Bksp - Back to species and class selection; "
                    "X - Quit" EOL);

        if (Options.prev_book != SBT_NO_SELECTION)
        {
            cprintf("; Enter - %s",
                    Options.prev_book == SBT_FIRE   ? "Fire"      :
                    Options.prev_book == SBT_COLD   ? "Cold"      :
                    Options.prev_book == SBT_SUMM   ? "Summoning" :
                    Options.prev_book == SBT_RANDOM ? "Random"
                                                    : "Buggy Book");
        }
        cprintf(EOL);

        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which book? ");
            textcolor( LIGHTGREY );

            keyin = c_getch();

            switch (keyin)
            {
            case 'X':
                cprintf(EOL "Goodbye!");
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
                                + _start_to_book(firstbook, Options.prev_book)
                                - firstbook;
                    }
                }
                break;
            case '%':
                list_commands('%');
                return _choose_book(slot, firstbook, numbooks);
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
                if (book_restriction(_book_to_start(firstbook + i), ng)
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

    book.sub_type = firstbook + keyin - 'a';
    return (true);
}

static bool _choose_weapon()
{
    weapon_type startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
                                WPN_HAND_AXE, WPN_SPEAR, WPN_UNKNOWN };

    ng.init(you);

    // Gladiators that are at least medium sized get to choose a trident
    // rather than a spear
    if (you.char_class == JOB_GLADIATOR
        && you.body_size(PSIZE_BODY) >= SIZE_MEDIUM)
    {
        startwep[3] = WPN_TRIDENT;
    }

    const bool claws_allowed =
        (you.char_class != JOB_GLADIATOR && you.has_claws());

    if (claws_allowed)
    {
        for (int i = 3; i >= 0; --i)
            startwep[i + 1] = startwep[i];

        startwep[0] = WPN_UNARMED;
    }
    else
    {
        switch (you.species)
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
        startwep_restrictions[i] = weapon_restriction(startwep[i], ng);

    if (Options.weapon == WPN_UNARMED && claws_allowed)
    {
        you.inv[0].quantity = 0; // no weapon
        ng_weapon = Options.weapon;
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
                you.inv[0].sub_type = Options.weapon;
                ng_weapon = Options.weapon;
                return (true);
            }
        }
    }

    int keyin = 0;
    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        _print_character_info();

        textcolor( CYAN );
        cprintf(EOL "You have a choice of weapons:  "
                    "(Press %% for a list of aptitudes)" EOL);

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
            cprintf("%c - %s" EOL, letter,
                    startwep[i] == WPN_UNARMED ? "claws"
                                               : weapon_base_name(startwep[i]));

            if (Options.prev_weapon == startwep[i])
                prevmatch = true;
        }

        if (!prevmatch && Options.prev_weapon != WPN_RANDOM)
            Options.prev_weapon = WPN_UNKNOWN;

        textcolor(BROWN);
        cprintf(EOL "* - Random choice; + - Good random choice; "
                    "Bksp - Back to species and class selection; "
                    "X - Quit" EOL);

        if (prevmatch || Options.prev_weapon == WPN_RANDOM)
        {
            cprintf("; Enter - %s",
                    Options.prev_weapon == WPN_RANDOM  ? "Random" :
                    Options.prev_weapon == WPN_UNARMED ? "claws"  :
                    weapon_base_name(Options.prev_weapon));
        }
        cprintf(EOL);

        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which weapon? ");
            textcolor( LIGHTGREY );

            keyin = c_getch();

            switch (keyin)
            {
            case 'X':
                cprintf(EOL "Goodbye!");
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
                return _choose_weapon();
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
                if (weapon_restriction(startwep[i], ng) == CC_UNRESTRICTED
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

    if (startwep[keyin - 'a'] == WPN_UNARMED)
        you.inv[0].quantity = 0; // no weapon
    else
        you.inv[0].sub_type = startwep[keyin - 'a'];

    return (true);
}

static bool _necromancy_okay()
{
    switch (you.species)
    {
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
    case SP_DEEP_DWARF:
    case SP_DEMONSPAWN:
    case SP_KENKU:
    case SP_MUMMY:
    case SP_VAMPIRE:
        return (true);

    default:
        if (player_genus(GENPC_DRACONIAN))
            return (true);
        return (false);
    }
}

static void _init_player(void)
{
    you.init();
}

static void _give_last_paycheck(job_type which_job)
{
    switch (which_job)
    {
    case JOB_HEALER:
    case JOB_THIEF:
        you.gold = 100;
        break;

    case JOB_WANDERER:
    case JOB_WARPER:
    case JOB_ARCANE_MARKSMAN:
    case JOB_ASSASSIN:
        you.gold = 50;
        break;

    default:
        you.gold = 20;
        break;

    case JOB_PALADIN:
    case JOB_MONK:
        you.gold = 0;
        break;
    }
}

// Requires stuff::modify_all_stats() and works because
// stats zeroed out by newgame::init_player()... Recall
// that demonspawn & demigods get more later on. {dlb}
static void _species_stat_init(species_type which_species)
{
    int sb = 0; // strength base
    int ib = 0; // intelligence base
    int db = 0; // dexterity base

    // Note: The stats in in this list aren't intended to sum the same
    // for all races.  The fact that Mummies and Ghouls are really low
    // is considered acceptable (Mummies don't have to eat, and Ghouls
    // are supposed to be a really hard race).  -- bwr
    switch (which_species)
    {
    default:                    sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_HUMAN:              sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_DEMIGOD:            sb =  9; ib = 10; db =  9;      break;  // 28
    case SP_DEMONSPAWN:         sb =  6; ib =  7; db =  6;      break;  // 19

    case SP_HIGH_ELF:           sb =  5; ib =  9; db =  8;      break;  // 22
    case SP_DEEP_ELF:           sb =  3; ib = 10; db =  8;      break;  // 21
    case SP_SLUDGE_ELF:         sb =  6; ib =  7; db =  7;      break;  // 20

    case SP_MOUNTAIN_DWARF:     sb =  9; ib =  4; db =  5;      break;  // 18
    case SP_DEEP_DWARF:         sb =  9; ib =  6; db =  6;      break;  // 21

    case SP_TROLL:              sb = 13; ib =  2; db =  3;      break;  // 18
    case SP_OGRE:               sb = 10; ib =  5; db =  3;      break;  // 18

    case SP_MINOTAUR:           sb = 10; ib =  3; db =  3;      break;  // 16
    case SP_HILL_ORC:           sb =  9; ib =  3; db =  4;      break;  // 16
    case SP_CENTAUR:            sb =  8; ib =  5; db =  2;      break;  // 15
    case SP_NAGA:               sb =  8; ib =  6; db =  4;      break;  // 18

    case SP_MERFOLK:            sb =  6; ib =  5; db =  7;      break;  // 18
    case SP_KENKU:              sb =  6; ib =  6; db =  7;      break;  // 19

    case SP_KOBOLD:             sb =  5; ib =  4; db =  8;      break;  // 17
    case SP_HALFLING:           sb =  3; ib =  6; db =  9;      break;  // 18
    case SP_SPRIGGAN:           sb =  2; ib =  7; db =  9;      break;  // 18

    case SP_MUMMY:              sb =  7; ib =  3; db =  3;      break;  // 13
    case SP_GHOUL:              sb =  9; ib =  1; db =  2;      break;  // 13
    case SP_VAMPIRE:            sb =  5; ib =  8; db =  7;      break;  // 20

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:     sb =  9; ib =  6; db =  2;      break;  // 17
    }

    modify_all_stats( sb+2, ib+2, db+2 );
}

static void _jobs_stat_init(job_type which_job)
{
    int s = 0;   // strength mod
    int i = 0;   // intelligence mod
    int d = 0;   // dexterity mod
    int hp = 0;  // HP base
    int mp = 0;  // MP base

    // Note: Wanderers are correct, they're a challenging class. -- bwr
    switch (which_job)
    {
    case JOB_FIGHTER:           s =  8; i =  0; d =  4; hp = 15; mp = 0; break;
    case JOB_BERSERKER:         s =  9; i = -1; d =  4; hp = 15; mp = 0; break;
    case JOB_GLADIATOR:         s =  7; i =  0; d =  5; hp = 14; mp = 0; break;
    case JOB_PALADIN:           s =  7; i =  2; d =  3; hp = 14; mp = 0; break;

    case JOB_CRUSADER:          s =  4; i =  4; d =  4; hp = 13; mp = 1; break;
    case JOB_DEATH_KNIGHT:      s =  5; i =  3; d =  4; hp = 13; mp = 1; break;
    case JOB_CHAOS_KNIGHT:      s =  4; i =  4; d =  4; hp = 13; mp = 1; break;

    case JOB_REAVER:            s =  5; i =  5; d =  2; hp = 13; mp = 1; break;
    case JOB_HEALER:            s =  5; i =  5; d =  2; hp = 13; mp = 1; break;
    case JOB_PRIEST:            s =  5; i =  4; d =  3; hp = 12; mp = 1; break;

    case JOB_ASSASSIN:          s =  3; i =  3; d =  6; hp = 12; mp = 0; break;
    case JOB_THIEF:             s =  4; i =  2; d =  6; hp = 13; mp = 0; break;
    case JOB_STALKER:           s =  2; i =  4; d =  6; hp = 12; mp = 1; break;

    case JOB_HUNTER:            s =  4; i =  3; d =  5; hp = 13; mp = 0; break;
    case JOB_WARPER:            s =  3; i =  5; d =  4; hp = 12; mp = 1; break;
    case JOB_ARCANE_MARKSMAN:   s =  3; i =  5; d =  4; hp = 12; mp = 1; break;

    case JOB_MONK:              s =  3; i =  2; d =  7; hp = 13; mp = 0; break;
    case JOB_TRANSMUTER:        s =  2; i =  5; d =  5; hp = 12; mp = 1; break;

    case JOB_WIZARD:            s = -1; i = 10; d =  3; hp =  8; mp = 5; break;
    case JOB_CONJURER:          s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_ENCHANTER:         s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_FIRE_ELEMENTALIST: s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_ICE_ELEMENTALIST:  s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_AIR_ELEMENTALIST:  s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_EARTH_ELEMENTALIST:s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_SUMMONER:          s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_VENOM_MAGE:        s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_NECROMANCER:       s =  0; i =  7; d =  5; hp = 10; mp = 3; break;

    case JOB_WANDERER:
    {
        // Wanderers get their stats randomly distributed.
        _wanderer_assign_remaining_stats(12);
                                                        hp = 11; mp = 1; break;
    }

    case JOB_ARTIFICER:         s =  3; i =  4; d =  5; hp = 13; mp = 1; break;
    default:                    s =  0; i =  0; d =  0; hp = 10; mp = 0; break;
    }

    modify_all_stats( s, i, d );

    // Used for Jiyva's stat swapping if the player has not reached
    // experience level 3.
    you.last_chosen = (stat_type) random2(NUM_STATS);

    set_hp( hp, true );
    set_mp( mp, true );
}

void give_basic_mutations(species_type speci)
{
    // We should switch over to a size-based system
    // for the fast/slow metabolism when we get around to it.
    switch (speci)
    {
    case SP_HILL_ORC:
        you.mutation[MUT_SAPROVOROUS] = 1;
        break;
    case SP_OGRE:
        you.mutation[MUT_TOUGH_SKIN]      = 1;
        you.mutation[MUT_FAST_METABOLISM] = 1;
        you.mutation[MUT_SAPROVOROUS]     = 1;
        break;
    case SP_HALFLING:
        you.mutation[MUT_SLOW_METABOLISM] = 1;
        break;
    case SP_MINOTAUR:
        you.mutation[MUT_HORNS] = 2;
        break;
    case SP_SPRIGGAN:
        you.mutation[MUT_ACUTE_VISION]    = 1;
        you.mutation[MUT_FAST]            = 3;
        you.mutation[MUT_HERBIVOROUS]     = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 3;
        break;
    case SP_CENTAUR:
        you.mutation[MUT_TOUGH_SKIN]      = 3;
        you.mutation[MUT_FAST]            = 2;
        you.mutation[MUT_DEFORMED]        = 1;
        you.mutation[MUT_FAST_METABOLISM] = 2;
        you.mutation[MUT_HOOVES]          = 1;
        break;
    case SP_NAGA:
        you.mutation[MUT_ACUTE_VISION]      = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_DEFORMED]          = 1;
        break;
    case SP_MUMMY:
        you.mutation[MUT_TORMENT_RESISTANCE]         = 1;
        you.mutation[MUT_POISON_RESISTANCE]          = 1;
        you.mutation[MUT_COLD_RESISTANCE]            = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        break;
    case SP_DEEP_DWARF:
        you.mutation[MUT_SLOW_HEALING]    = 3;
        you.mutation[MUT_PASSIVE_MAPPING] = 1;
        break;
    case SP_GHOUL:
        you.mutation[MUT_TORMENT_RESISTANCE]         = 1;
        you.mutation[MUT_POISON_RESISTANCE]          = 1;
        you.mutation[MUT_COLD_RESISTANCE]            = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        you.mutation[MUT_SAPROVOROUS]                = 3;
        you.mutation[MUT_CARNIVOROUS]                = 3;
        you.mutation[MUT_SLOW_HEALING]               = 1;
        break;
    case SP_KENKU:
        you.mutation[MUT_BEAK]   = 1;
        you.mutation[MUT_TALONS] = 1;
        break;
    case SP_TROLL:
        you.mutation[MUT_TOUGH_SKIN]      = 2;
        you.mutation[MUT_REGENERATION]    = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        you.mutation[MUT_SAPROVOROUS]     = 2;
        you.mutation[MUT_GOURMAND]        = 1;
        you.mutation[MUT_SHAGGY_FUR]      = 1;
        break;
    case SP_KOBOLD:
        you.mutation[MUT_SAPROVOROUS] = 2;
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_VAMPIRE:
        you.mutation[MUT_FANGS]        = 3;
        you.mutation[MUT_ACUTE_VISION] = 1;
        break;
    default:
        break;
    }

    // Starting mutations are unremovable.
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        you.demon_pow[i] = you.mutation[i];
}

// Give knowledge of things that aren't in the starting inventory.
static void _give_basic_knowledge(job_type which_job)
{
    if (you.species == SP_VAMPIRE)
        set_ident_type(OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE);

    switch (which_job)
    {
    case JOB_STALKER:
    case JOB_ASSASSIN:
    case JOB_VENOM_MAGE:
        set_ident_type(OBJ_POTIONS, POT_POISON, ID_KNOWN_TYPE);
        break;

    case JOB_TRANSMUTER:
        set_ident_type(OBJ_POTIONS, POT_WATER, ID_KNOWN_TYPE);
        break;

    case JOB_ARTIFICER:
        if (!item_is_rod(you.inv[2]))
            set_ident_type(OBJ_SCROLLS, SCR_RECHARGING, ID_KNOWN_TYPE);
        break;

    default:
        break;
    }
}

static void _give_basic_spells(job_type which_job)
{
    // Wanderers may or may not already have a spell. -- bwr
    if (which_job == JOB_WANDERER)
        return;

    spell_type which_spell = SPELL_NO_SPELL;

    switch (which_job)
    {
    case JOB_WIZARD:
        if (!you.skills[SK_CONJURATIONS])
        {
            // Wizards who start with Minor Magic III (summoning) have no
            // Conjurations skill, and thus get another starting spell.
            which_spell = SPELL_SUMMON_SMALL_MAMMALS;
            break;
        }
        // intentional fall-through
    case JOB_CONJURER:
    case JOB_REAVER:
        which_spell = SPELL_MAGIC_DART;
        break;
    case JOB_STALKER:
    case JOB_VENOM_MAGE:
        which_spell = SPELL_STING;
        break;
    case JOB_SUMMONER:
        which_spell = SPELL_SUMMON_SMALL_MAMMALS;
        break;
    case JOB_NECROMANCER:
        which_spell = SPELL_PAIN;
        break;
    case JOB_ENCHANTER:
        which_spell = SPELL_CORONA;
        break;
    case JOB_FIRE_ELEMENTALIST:
        which_spell = SPELL_FLAME_TONGUE;
        break;
    case JOB_ICE_ELEMENTALIST:
        which_spell = SPELL_FREEZE;
        break;
    case JOB_AIR_ELEMENTALIST:
        which_spell = SPELL_SHOCK;
        break;
    case JOB_EARTH_ELEMENTALIST:
        which_spell = SPELL_SANDBLAST;
        break;
    case JOB_DEATH_KNIGHT:
        if (you.species == SP_DEMIGOD || you.religion != GOD_YREDELEMNUL)
            which_spell = SPELL_PAIN;
        break;

    default:
        break;
    }

    if (which_spell != SPELL_NO_SPELL)
        add_spell_to_memory( which_spell );

    return;
}

static void _make_rod(item_def &item, stave_type rod_type, int ncharges)
{
    item.base_type = OBJ_STAVES;
    item.sub_type  = rod_type;
    item.quantity  = 1;
    item.special   = you.item_description[IDESC_STAVES][rod_type];
    item.colour    = BROWN;

    init_rod_mp(item, ncharges);
}

// Creates an item of a given base and sub type.
// replacement is used when handing out armour that is not wearable for
// some species; otherwise use -1.
static void _newgame_make_item(int slot, equipment_type eqslot,
                               object_class_type base,
                               int sub_type, int replacement = -1,
                               int qty = 1, int plus = 0, int plus2 = 0)
{
    if (slot == -1)
    {
        // If another of the item type is already there, add to the
        // stack instead.
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            item_def& item = you.inv[i];
            if (item.base_type == base && item.sub_type == sub_type
                && is_stackable_item(item))
            {
                item.quantity += qty;
                return;
            }
        }

        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!you.inv[i].is_valid())
            {
                slot = i;
                break;
            }
        }
        ASSERT(slot != -1);
    }

    item_def &item(you.inv[slot]);
    item.base_type = base;
    item.sub_type  = sub_type;
    item.quantity  = qty;
    item.plus      = plus;
    item.plus2     = plus2;
    item.special   = 0;

    // If the character is restricted in wearing armour of equipment
    // slot eqslot, hand out replacement instead.
    if (item.base_type == OBJ_ARMOUR && replacement != -1
        && !you_can_wear(eqslot))
    {
        // Don't replace shields with bucklers for large races or
        // draconians.
        if (sub_type != ARM_SHIELD
            || you.body_size(PSIZE_TORSO) < SIZE_LARGE
               && !player_genus(GENPC_DRACONIAN))
        {
            item.sub_type = replacement;
        }
    }

    if (eqslot != EQ_NONE && you.equip[eqslot] == -1)
        you.equip[eqslot] = slot;
}

// Returns true if a "good" weapon is given.
static bool _give_wanderer_weapon(int & slot, int wpn_skill, int plus)
{
    // Darts skill also gets you some needles.
    if (wpn_skill == SK_DARTS)
    {
        // Plus is set if we are getting a good item.  In that case, we
        // get curare here.
        if (plus)
        {
            _newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1,
                               1 + random2(4));
            set_item_ego_type(you.inv[slot], OBJ_MISSILES, SPMSL_CURARE);
            slot++;
        }
        // Otherwise, we just get some poisoned needles.
        else
        {
            _newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1,
                               5 + roll_dice(2, 5));
            set_item_ego_type(you.inv[slot], OBJ_MISSILES, SPMSL_POISONED);
            slot++;
        }
    }

    _newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, WPN_KNIFE);

    // We'll also re-fill the template, all for later possible safe
    // reuse of code in the future.
    you.inv[slot].quantity  = 1;
    you.inv[slot].plus      = 0;
    you.inv[slot].plus2     = 0;
    you.inv[slot].special   = 0;

    // Now fill in the type according to the random wpn_skill.
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
        you.inv[slot].sub_type = WPN_SHORT_SWORD;
        break;

    case SK_MACES_FLAILS:
        you.inv[slot].sub_type = WPN_MACE;
        break;

    case SK_AXES:
        you.inv[slot].sub_type = WPN_HAND_AXE;
        break;

    case SK_POLEARMS:
        you.inv[slot].sub_type = WPN_SPEAR;
        break;

    case SK_STAVES:
        you.inv[slot].sub_type = WPN_QUARTERSTAFF;
        break;

    case SK_DARTS:
        you.inv[slot].sub_type = WPN_BLOWGUN;
        break;

    case SK_BOWS:
        you.inv[slot].sub_type = WPN_BOW;
        break;

    case SK_CROSSBOWS:
        you.inv[slot].sub_type = WPN_HAND_CROSSBOW;
        break;
    }

    int offset = plus ? 1 : 0;
    you.inv[slot].plus  = random2(plus) + offset;
    you.inv[slot].plus2 = random2(plus) + offset;

    return (true);
}

static void _newgame_clear_item(int slot)
{
    you.inv[slot] = item_def();

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        if (you.equip[i] == slot)
            you.equip[i] = -1;
}

// The overall role choice for wanderers is a weighted chance based on
// stats.
static stat_type _wanderer_choose_role()
{
    int total_stats = 0;

    total_stats += you.strength;
    total_stats += you.dex;
    total_stats += you.intel;

    int target = random2(total_stats);

    stat_type role;

    if (target < you.strength)
        role = STAT_STRENGTH;
    else if (target < (you.dex + you.strength))
        role = STAT_DEXTERITY;
    else
        role = STAT_INTELLIGENCE;

    return (role);
}

static skill_type _apt_weighted_choice(const skill_type * skill_array,
                                       unsigned arr_size)
{
    int total_apt = 0;

    for (unsigned i = 0; i < arr_size; ++i)
    {
        int reciprocal_apt = (100 * 100) /
                             species_skills(skill_array[i], you.species);
        total_apt += reciprocal_apt;
    }

    unsigned probe = random2(total_apt);
    unsigned region_covered = 0;

    for (unsigned i = 0; i < arr_size; ++i)
    {
        int reciprocal_apt = (100 * 100) /
                             species_skills(skill_array[i], you.species);
        region_covered += reciprocal_apt;

        if (probe < region_covered)
            return (skill_array[i]);
    }

    return (NUM_SKILLS);
}

static skill_type _wanderer_role_skill_select(stat_type selected_role,
                                              skill_type sk_1,
                                              skill_type sk_2)
{
    skill_type selected_skill = SK_NONE;

    switch((int)selected_role)
    {
    case STAT_DEXTERITY:
        switch (random2(6))
        {
        case 0:
        case 1:
            selected_skill = SK_FIGHTING;
            break;
        case 2:
            selected_skill = SK_DODGING;
            break;
        case 3:
            selected_skill = SK_STEALTH;
            break;
        case 4:
        case 5:
            selected_skill = sk_1;
            break;
        }
        break;

    case STAT_STRENGTH:
    {
        int options = 3;
        if (!you_can_wear(EQ_BODY_ARMOUR))
            options--;

        switch (random2(options))
        {
        case 0:
            selected_skill = SK_FIGHTING;
            break;
        case 1:
            selected_skill = sk_1;
            break;
        case 2:
            selected_skill = SK_ARMOUR;
            break;
        }
        break;
    }

    case STAT_INTELLIGENCE:
        switch (random2(3))
        {
        case 0:
            selected_skill = SK_SPELLCASTING;
            break;
        case 1:
            selected_skill = sk_1;
            break;
        case 2:
            selected_skill = sk_2;
            break;
        }
        break;
    }

    return (selected_skill);
}

static skill_type _wanderer_role_weapon_select(stat_type role)
{
    skill_type skill = NUM_SKILLS;
    const skill_type str_weapons[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS };

    int str_size = sizeof(str_weapons) / sizeof(skill_type);

    const skill_type dex_weapons[] =
        { SK_SHORT_BLADES, SK_STAVES, SK_UNARMED_COMBAT, SK_POLEARMS };

    int dex_size = sizeof(dex_weapons) / sizeof(skill_type);

    const skill_type casting_schools[] =
        { SK_SUMMONINGS, SK_NECROMANCY, SK_TRANSLOCATIONS,
          SK_TRANSMUTATIONS, SK_POISON_MAGIC, SK_CONJURATIONS,
          SK_ENCHANTMENTS, SK_FIRE_MAGIC, SK_ICE_MAGIC,
          SK_AIR_MAGIC, SK_EARTH_MAGIC };

    int casting_size = sizeof(casting_schools) / sizeof(skill_type);

    switch ((int)role)
    {
    case STAT_STRENGTH:
        skill = _apt_weighted_choice(str_weapons, str_size);
        break;

    case STAT_DEXTERITY:
        skill = _apt_weighted_choice(dex_weapons, dex_size);
        break;

    case STAT_INTELLIGENCE:
        skill = _apt_weighted_choice(casting_schools, casting_size);
        break;
    }

    return (skill);
}

static void _wanderer_role_skill(stat_type role, int levels)
{
    skill_type weapon_type = NUM_SKILLS;
    skill_type spell2 = NUM_SKILLS;

    weapon_type = _wanderer_role_weapon_select(role);
    if (role == STAT_INTELLIGENCE)
       spell2 = _wanderer_role_weapon_select(role);

    skill_type selected_skill = NUM_SKILLS;
    for (int i = 0; i < levels; ++i)
    {
        selected_skill = _wanderer_role_skill_select(role, weapon_type,
                                                     spell2);
        you.skills[selected_skill]++;
    }

}

// Select a random skill from all skills we have at least 1 level in.
static skill_type _weighted_skill_roll()
{
    int total_skill = 0;

    for (unsigned i = 0; i < NUM_SKILLS; ++i)
        total_skill += you.skills[i];

    int probe = random2(total_skill);
    int covered_region = 0;

    for (unsigned i = 0; i < NUM_SKILLS; ++i)
    {
        covered_region += you.skills[i];
        if (probe < covered_region)
            return (skill_type(i));
    }

    return (NUM_SKILLS);
}

static void _give_wanderer_book(skill_type skill, int & slot)
{
    int book_type = BOOK_MINOR_MAGIC_I;
    switch((int)skill)
    {
    case SK_SPELLCASTING:
        switch (random2(3))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC_I;
            break;
        case 1:
            book_type = BOOK_MINOR_MAGIC_II;
            break;
        case 2:
            book_type = BOOK_MINOR_MAGIC_III;
            break;
        }
        break;

    case SK_CONJURATIONS:
        switch (random2(6))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC_I;
            break;

        case 1:
            book_type = BOOK_MINOR_MAGIC_II;
            break;
        case 2:
            book_type = BOOK_CONJURATIONS_I;
            break;
        case 3:
            book_type = BOOK_CONJURATIONS_II;
            break;
        case 4:
            book_type = BOOK_YOUNG_POISONERS;
            break;
        case 5:
            book_type = BOOK_STALKING;
            break;
        }
        break;

    case SK_SUMMONINGS:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC_III;
            break;
        case 1:
            book_type = BOOK_CALLINGS;
            break;
        }
        break;

    case SK_NECROMANCY:
        book_type = BOOK_NECROMANCY;
        break;

    case SK_TRANSLOCATIONS:
        book_type = BOOK_SPATIAL_TRANSLOCATIONS;
        break;

    case SK_TRANSMUTATIONS:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_GEOMANCY;
            break;
        case 1:
            book_type = BOOK_CHANGES;
            break;
        }
        break;

    case SK_FIRE_MAGIC:
        switch (random2(3))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC_I;
            break;
        case 1:
            book_type = BOOK_FLAMES;
            break;
        case 2:
            book_type = BOOK_CONJURATIONS_I;
            break;
        }
        break;

    case SK_ICE_MAGIC:
        switch (random2(3))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC_II;
            break;
        case 1:
            book_type = BOOK_FROST;
            break;
        case 2:
            book_type = BOOK_CONJURATIONS_II;
            break;
        }
        break;

    case SK_AIR_MAGIC:
        book_type = BOOK_AIR;
        break;

    case SK_EARTH_MAGIC:
        book_type = BOOK_GEOMANCY;
        break;

    case SK_POISON_MAGIC:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_STALKING;
            break;
        case 1:
            book_type = BOOK_YOUNG_POISONERS;
            break;
        }
        break;

    case SK_ENCHANTMENTS:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_WAR_CHANTS;
            break;
        case 1:
            book_type = BOOK_CHARMS;
            break;
        }
        break;
    }

    _newgame_make_item(slot, EQ_NONE, OBJ_BOOKS, book_type);
}

// Players can get some consumables as a "good item".
static void _good_potion_or_scroll(int & slot)
{
    int base_rand = 5;
    // No potions for mummies.
    if (you.is_undead == US_UNDEAD)
        base_rand -= 3;
    // No berserk rage for ghouls.
    else if (you.is_undead && you.is_undead != US_SEMI_UNDEAD)
        base_rand--;

    you.inv[slot].quantity = 1;
    you.inv[slot].plus     = 0;
    you.inv[slot].plus2    = 0;

    switch (random2(base_rand))
    {
    case 0:
        you.inv[slot].base_type = OBJ_SCROLLS;
        you.inv[slot].sub_type  = SCR_FEAR;
        break;

    case 1:
        you.inv[slot].base_type = OBJ_SCROLLS;
        you.inv[slot].sub_type  = SCR_BLINKING;
        break;

    case 2:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_HEAL_WOUNDS;
        break;

    case 3:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_SPEED;
        break;

    case 4:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_BERSERK_RAGE;
        break;
    }

    slot++;
}

// Make n random attempts at identifying healing or teleportation.
static void _healing_or_teleport(int n)
{
    int temp_rand = 2;

    // No potions for mummies.
    if (you.is_undead == US_UNDEAD)
        temp_rand--;

    for (int i = 0; i < n; ++i)
    {
        switch (random2(temp_rand))
        {
        case 0:
            set_ident_type(OBJ_SCROLLS, SCR_TELEPORTATION, ID_KNOWN_TYPE);
            break;
        case 1:
            set_ident_type(OBJ_POTIONS, POT_HEALING, ID_KNOWN_TYPE);
            break;
        }
    }
}

// Create a random wand/rod of striking in the inventory.
static void _wanderer_random_evokable(int & slot)
{
    wand_type selected_wand = WAND_ENSLAVEMENT;

    switch (random2(6))
    {
    case 0:
        selected_wand = WAND_ENSLAVEMENT;
        break;

    case 1:
        selected_wand = WAND_CONFUSION;
        break;

    case 2:
        selected_wand = WAND_FLAME;
        break;

    case 3:
        selected_wand = WAND_FROST;
        break;

    case 4:
        selected_wand = WAND_MAGIC_DARTS;
        break;

    case 5:
        _make_rod(you.inv[slot], STAFF_STRIKING, 8);
        slot++;
        return;
    }

    _newgame_make_item(slot, EQ_NONE, OBJ_WANDS, selected_wand, -1, 1,
                       15);
    slot++;
}

void _wanderer_good_equipment(skill_type & skill, int & slot)
{
    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS,
          SK_SHORT_BLADES, SK_STAVES, SK_UNARMED_COMBAT, SK_POLEARMS };


    int total_weapons = sizeof(combined_weapon_skills) / sizeof(skill_type);

    // Normalise the input type.
    if (skill == SK_FIGHTING)
    {
        int max_sklev = 0;
        skill_type max_skill = SK_NONE;

        for (int i = 0; i < total_weapons; ++i)
        {
            if (you.skills[combined_weapon_skills[i]] >= max_sklev)
            {
                max_skill = combined_weapon_skills[i];
                max_sklev = you.skills[max_skill];
            }
        }
        skill = max_skill;
    }

    switch((int)skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_BOWS:
    case SK_CROSSBOWS:
    case SK_DARTS:
    case SK_STAVES:
    case SK_SHORT_BLADES:
        _give_wanderer_weapon(slot, skill, 3);
        slot++;
        break;

    case SK_ARMOUR:
        // Deformed races aren't given armor skill, so there's no need
        // to worry about scale mail's not fitting.
        _newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL);
        slot++;
        break;

    case SK_SHIELDS:
        _newgame_make_item(slot, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD,
                           ARM_BUCKLER);
        slot++;
        break;

    case SK_SPELLCASTING:
    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
    case SK_ENCHANTMENTS:
        _give_wanderer_book(skill, slot);
        slot++;
        break;

    case SK_DODGING:
    case SK_STEALTH:
    case SK_TRAPS_DOORS:
    case SK_STABBING:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    {
        // Random consumables: 2x attempts to ID healing/teleportation
        // 1 good potion/scroll.
        _healing_or_teleport(2);
        _good_potion_or_scroll(slot);
        break;
    }

    case SK_EVOCATIONS:
        // Random wand/rod of striking.
        _wanderer_random_evokable(slot);
        break;
    }
}

// The "decent" spell type item puts a spell in the player's memory.
static void _give_wanderer_spell(skill_type skill)
{
    spell_type spell = SPELL_NO_SPELL;

    // Doing a rejection loop for this because I am lazy.
    while (skill == SK_SPELLCASTING)
    {
        int value = SK_POISON_MAGIC-SK_CONJURATIONS + 1;
        skill = skill_type(SK_CONJURATIONS + random2(value));
    }

    switch ((int)skill)
    {
    case SK_CONJURATIONS:
        spell = SPELL_MAGIC_DART;
        break;

    case SK_SUMMONINGS:
        spell = SPELL_SUMMON_SMALL_MAMMALS;
        break;

    case SK_NECROMANCY:
        spell = SPELL_PAIN;
        break;

    case SK_TRANSLOCATIONS:
        spell = SPELL_APPORTATION;
        break;

    case SK_TRANSMUTATIONS:
        spell = SPELL_SANDBLAST;
        break;

    case SK_FIRE_MAGIC:
        spell = SPELL_FLAME_TONGUE;
        break;

    case SK_ICE_MAGIC:
        spell = SPELL_FREEZE;
        break;

    case SK_AIR_MAGIC:
        spell = SPELL_SHOCK;
        break;

    case SK_EARTH_MAGIC:
        spell = SPELL_SANDBLAST;
        break;

    case SK_POISON_MAGIC:
        spell = SPELL_STING;
        break;

    case SK_ENCHANTMENTS:
        spell = SPELL_CORONA;
        break;
    }

    add_spell_to_memory(spell);
}

void _wanderer_decent_equipment(skill_type & skill,
                                std::set<skill_type> & gift_skills,
                                int & slot)
{
    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS,
          SK_SHORT_BLADES, SK_STAVES, SK_UNARMED_COMBAT, SK_POLEARMS };

    int total_weapons = sizeof(combined_weapon_skills) / sizeof(skill_type);

    // If we already gave an item for this type, just give the player
    // a consumable.
    if ((skill == SK_DODGING || skill == SK_STEALTH)
        && gift_skills.find(SK_ARMOUR) != gift_skills.end())
    {
        skill = SK_TRAPS_DOORS;
    }

    // Give the player knowledge of only one spell.
    if (skill >= SK_SPELLCASTING && skill <= SK_POISON_MAGIC)
    {
        for (unsigned i = 0; i < you.spells.size(); ++i)
        {
            if (you.spells[i] != SPELL_NO_SPELL)
            {
                skill = SK_TRAPS_DOORS;
                break;
            }
        }
    }

    // If fighting comes up, give something from the highest weapon
    // skill.
    if (skill == SK_FIGHTING)
    {
        int max_sklev = 0;
        skill_type max_skill = SK_NONE;

        for (int i = 0; i < total_weapons; ++i)
        {
            if (you.skills[combined_weapon_skills[i]] >= max_sklev)
            {
                max_skill = combined_weapon_skills[i];
                max_sklev = you.skills[max_skill];
            }
        }

        skill = max_skill;
    }

    // Don't give a gift from the same skill twice; just default to
    // a healing potion/teleportation scroll.
    if (gift_skills.find(skill) != gift_skills.end())
        skill = SK_TRAPS_DOORS;

    switch((int)skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_BOWS:
    case SK_CROSSBOWS:
    case SK_DARTS:
    case SK_STAVES:
    case SK_SHORT_BLADES:
        _give_wanderer_weapon(slot, skill, 0);
        slot++;
        break;

    case SK_ARMOUR:
        _newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_RING_MAIL);
        slot++;
        break;

    case SK_SHIELDS:
        _newgame_make_item(slot, EQ_SHIELD, OBJ_ARMOUR, ARM_BUCKLER,
                           ARM_SHIELD);
        slot++;
        break;

    case SK_DODGING:
    case SK_STEALTH:
        _newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        slot++;
        break;

    case SK_SPELLCASTING:
    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
        _give_wanderer_spell(skill);
        break;

    case SK_TRAPS_DOORS:
    case SK_STABBING:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
        _healing_or_teleport(1);
        break;
    }
}

// We don't actually want to send adventurers wandering naked into the
// dungeon.
static void _wanderer_cover_equip_holes(int & slot)
{
    // We are going to cover any glaring holes (no armor/no weapon) that
    // occurred during equipment generation.
    if (you.equip[EQ_BODY_ARMOUR] == -1)
    {
        _newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        slot++;
    }

    if (you.equip[EQ_WEAPON] == -1)
    {
        weapon_type weapon = WPN_CLUB;
        if (you.dex > you.strength || you.skills[SK_STABBING])
            weapon = WPN_DAGGER;

        _newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, weapon);
        slot++;
    }

    // Give a dagger if you have stabbing skill.  Maybe this is
    // unnecessary?
    if (you.skills[SK_STABBING])
    {
        bool has_dagger = false;

        for (int i = 0; i < slot; ++i)
        {
            if (you.inv[i].base_type == OBJ_WEAPONS
                && you.inv[i].sub_type == WPN_DAGGER)
            {
                has_dagger = true;
                break;
            }
        }

        if (!has_dagger)
        {
            _newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
            slot++;
        }
    }

    // The player gets a stack of darts if they have a hand
    // crossbow/darts skill but no blowgun.
    bool need_darts = false;

    for (int i = 0; i < slot; ++i)
    {
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == WPN_HAND_CROSSBOW)
        {
            need_darts = true;
            break;
        }
    }

    if (!need_darts && you.skills[SK_DARTS])
    {
        need_darts = true;

        for (int i = 0; i < slot; ++i)
        {
            if (you.inv[i].base_type == OBJ_WEAPONS
                && you.inv[i].sub_type == WPN_BLOWGUN)
            {
                need_darts = false;
                break;
            }
        }
    }

    if (need_darts)
    {
        _newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_DART, -1,
                           8 + roll_dice(2, 8));
        slot++;
    }

    // And the player needs arrows if they have a bow.
    bool needs_arrows = false;

    for (int i = 0; i < slot; ++i)
    {
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == WPN_BOW)
        {
            needs_arrows = true;
            break;
        }
    }

    if (needs_arrows)
    {
        _newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1,
                           15 + random2avg(21, 5));
        slot++;
    }
}

// New style wanderers are supposed to be decent in terms of skill
// levels/equipment, but pretty randomised.
static void _create_wanderer(void)
{
    // Decide what our character roles are.
    stat_type primary_role   = _wanderer_choose_role();
    stat_type secondary_role = _wanderer_choose_role();

    // Regardless of roles, players get a couple levels in these skills.
    const skill_type util_skills[] =
        { SK_DARTS, SK_STABBING, SK_TRAPS_DOORS, SK_STEALTH,
          SK_SHIELDS, SK_EVOCATIONS, SK_INVOCATIONS };

    int util_size = sizeof(util_skills) / sizeof(skill_type);

    // No Invocations for demigods.
    if (you.species == SP_DEMIGOD)
        util_size--;

    // Maybe too many skill levels, given the level 1 floor on skill
    // levels for wanderers?
    int primary_skill_levels   = 5;
    int secondary_skill_levels = 3;

    // Allocate main skill levels.
    _wanderer_role_skill(primary_role, primary_skill_levels);
    _wanderer_role_skill(secondary_role, secondary_skill_levels);

    skill_type util_skill1 = _apt_weighted_choice(util_skills, util_size);
    skill_type util_skill2 = _apt_weighted_choice(util_skills, util_size);

    // And a couple levels of utility skills.
    you.skills[util_skill1]++;
    you.skills[util_skill2]++;

    // Caster types maybe need more MP?
    int mp_adjust = 0;
    if (primary_role == STAT_INTELLIGENCE)
        mp_adjust++;
    if (secondary_role == STAT_INTELLIGENCE)
        mp_adjust++;
    set_mp(you.magic_points + mp_adjust, true);

    // Keep track of what skills we got items from, mostly to prevent
    // giving a good and then a normal version of the same weapon.
    std::set<skill_type> gift_skills;

    // Wanderers get 1 good thing, a couple average things, and then
    // 1 last stage to fill any glaring equipment holes (no clothes,
    // etc.).
    skill_type good_equipment = _weighted_skill_roll();

    // The first of these goes through the whole role/aptitude weighting
    // thing again.  It's quite possible that this will give something
    // we have no skill in.
    stat_type selected_role = one_chance_in(3) ? secondary_role : primary_role;
    skill_type sk_1 = SK_NONE;
    skill_type sk_2 = SK_NONE;

    sk_1 = _wanderer_role_weapon_select(selected_role);
    if (selected_role == STAT_INTELLIGENCE)
        sk_2 = _wanderer_role_weapon_select(selected_role);

    skill_type decent_1 = _wanderer_role_skill_select(selected_role,
                                                      sk_1, sk_2);
    skill_type decent_2 = _weighted_skill_roll();

    // Not even trying to put things in the same slot from game to game.
    int equip_slot = 0;

    _wanderer_good_equipment(good_equipment, equip_slot);
    gift_skills.insert(good_equipment);

    _wanderer_decent_equipment(decent_1, gift_skills, equip_slot);
    gift_skills.insert(decent_1);
    _wanderer_decent_equipment(decent_2, gift_skills, equip_slot);
    gift_skills.insert(decent_2);

    _wanderer_cover_equip_holes(equip_slot);
}

// choose_race returns true if the player should also pick a class.
// This is done because of the '!' option which will pick a random
// character, obviating the necessity of choosing a class.
bool choose_race()
{
    char keyn;

    bool printed = false;

    if (Options.cls)
    {
        you.char_class = get_class(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    if (Options.race != 0)
        printed = true;

spec_query:
    bool prevraceok = (Options.prev_race == '*');
    if (!printed)
    {
        clrscr();

        if (you.char_class != JOB_UNKNOWN)
        {
            textcolor( BROWN );
            bool shortgreet = false;
            if (!you.your_name.empty() || you.char_class != JOB_UNKNOWN)
                cprintf("Welcome, ");
            else
            {
                cprintf("Welcome.");
                shortgreet = true;
            }

            textcolor( YELLOW );
            if (!you.your_name.empty())
            {
                cprintf("%s", you.your_name.c_str());
                if (you.char_class != JOB_UNKNOWN)
                    cprintf(" the ");
            }
            if (you.char_class != JOB_UNKNOWN)
                cprintf("%s", get_class_name(you.char_class));

            if (!shortgreet)
                cprintf(".");

            textcolor( WHITE ); // for the tutorial
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press Ctrl-T to enter a tutorial.)");
        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:  "
                "(Press ? for more information, %% for a list of aptitudes)");
        cprintf(EOL);

        textcolor( LIGHTGREY );

        int i = -1;
        for (i = 0; i < ng_num_species(); ++i)
        {
            const species_type si = get_species(i);

            if (!_is_species_valid_choice(si))
                continue;

            // Dim text for restricted species
            if (you.char_class == JOB_UNKNOWN
                || class_allowed(si, you.char_class) == CC_UNRESTRICTED)
            {
                textcolor(LIGHTGREY);
            }
            else
                textcolor(DARKGREY);

            // Show banned races as "unavailable".
            if (you.char_class != JOB_UNKNOWN
                && class_allowed(si, you.char_class) == CC_BANNED)
            {
                cprintf("    %s N/A", species_name(si, 1).c_str());
            }
            else
            {
                char sletter = index_to_letter(i);

                if (sletter == Options.prev_race)
                    prevraceok = true;

                cprintf("%c - %s", sletter, species_name(si, 1).c_str());
            }

            if (i % 2)
                cprintf(EOL);
            else
                cgotoxy(31, wherey());

            textcolor(LIGHTGREY); // Reset text colour.
        }

        if (i % 2)
            cprintf(EOL);

        textcolor( BROWN );
        cprintf(EOL EOL);
        if (you.char_class == JOB_UNKNOWN)
        {
            cprintf("Space - Choose job first; * - Random species" EOL
                    "! - Random character; # - Good random character; X - Quit"
                    EOL);
        }
        else
        {
            cprintf("* - Random; + - Good random; "
                    "Bksp - Back to job selection; X - Quit"
                    EOL);
        }

        if (Options.prev_race)
        {
            if (prevraceok)
            {
                cprintf("Enter - %s",
                        _get_opt_race_name(Options.prev_race).c_str());
            }
            if (_prev_startup_options_set())
            {
                cprintf("%sTab - %s",
                        prevraceok? "; " : "",
                        _prev_startup_description().c_str());
            }
            cprintf(EOL);
        }

        textcolor( CYAN );
        cprintf(EOL "Which one? ");
        textcolor( LIGHTGREY );

        printed = true;
    }

    if (Options.race != 0)
        keyn = Options.race;
    else
        keyn = c_getch();

    bool good_random = false;
    switch (keyn)
    {
    case 'X':
    case ESCAPE:
        cprintf(EOL "Goodbye!");
        end(0);
        break;
    case CK_BKSP:
    case ' ':
        you.species  = SP_UNKNOWN;
        Options.race = 0;
        return (true);
    case '#':
        good_random = true;
        // intentional fall-through
    case '!':
        _pick_random_species_and_class(good_random);
        Options.random_pick = true; // used to give random weapon/god as well
        ng_random = true;
        if (good_random)
            Options.good_random = true;
        return (false);
    case '\r':
    case '\n':
        if (Options.prev_race && prevraceok)
            keyn = Options.prev_race;
        break;
    case '\t':
        if (_prev_startup_options_set())
        {
            if (Options.prev_randpick
                || Options.prev_race == '*' && Options.prev_cls == '*')
            {
                Options.random_pick = true;
                ng_random = true;
                _pick_random_species_and_class(Options.good_random);
                return (false);
            }
            _set_startup_options();
            you.species = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;
            return (true);
        }
        break;
    // access to the help files
    case '?':
        list_commands('1');
        return choose_race();
    case '%':
        list_commands('%');
        return choose_race();
    default:
        break;
    }

    // These are handled specially as they _could_ be set
    // using Options.race or prev_race.
    if (keyn == CONTROL('T') || keyn == 'T') // easy to set in init.txt
        return !pick_tutorial();

    bool good_randrace = (keyn == '+');
    bool randrace = (good_randrace || keyn == '*');
    if (randrace)
    {
        if (you.char_class == JOB_THIEF || you.char_class == JOB_WANDERER)
            good_randrace = false;

        int index;
        do
        {
            index = random2(ng_num_species());
        }
        while (!_is_species_valid_choice(get_species(index), false)
               || you.char_class != JOB_UNKNOWN
                  && !is_good_combination(get_species(index), you.char_class,
                                           good_randrace));

        keyn = index_to_letter(index);
    }

    if (keyn >= 'a' && keyn <= 'z' || keyn >= 'A' && keyn <= 'Z')
        you.species = get_species(letter_to_index(keyn));

    if (!_is_species_valid_choice( you.species ))
    {
        if (Options.race != 0)
        {
            Options.race = 0;
            printed = false;
        }
        goto spec_query;
    }

    if (you.species != SP_UNKNOWN && you.char_class != JOB_UNKNOWN
        && !class_allowed(you.species, you.char_class))
    {
        goto spec_query;
    }

    // Set to 0 in case we come back from choose_class().
    Options.race = 0;
    ng_race = (randrace ? (good_randrace? '+' : '*')
                        : keyn);

    return (true);
}

// Returns true if a class was chosen, false if we should go back to
// race selection.
bool choose_class(void)
{
    char keyn;

    bool printed = false;
    if (Options.cls != 0)
        printed = true;

    if (you.species != SP_UNKNOWN && you.char_class != JOB_UNKNOWN)
        return (true);

    ng_cls = 0;

job_query:
    bool prevclassok = (Options.prev_cls == '*');
    if (!printed)
    {
        clrscr();

        if (you.species != SP_UNKNOWN)
        {
            textcolor( BROWN );
            cprintf("Welcome, ");
            textcolor( YELLOW );
            if (!you.your_name.empty())
                cprintf("%s the ", you.your_name.c_str());
            cprintf("%s.", species_name(you.species, 1).c_str());
            textcolor( WHITE );
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press Ctrl-T to enter a tutorial.)");

        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:  "
                "(Press ? for more information, %% for a list of aptitudes)" EOL);
        textcolor( LIGHTGREY );

        int j = 0;
        job_type which_job;
        for (int i = 0; i < ng_num_classes(); i++)
        {
            which_job = get_class(i);

            // Dim text for restricted classes.
            // Thief and wanderer are general challenge classes in that there's
            // no species that's unrestricted in combination with them.
            if (you.species == SP_UNKNOWN
                   && which_job != JOB_THIEF && which_job != JOB_WANDERER
                || you.species != SP_UNKNOWN
                   && class_allowed(you.species, which_job) == CC_UNRESTRICTED)
            {
                textcolor(LIGHTGREY);
            }
            else
                textcolor(DARKGREY);

            // Show banned races as "unavailable".
            if (class_allowed(you.species, which_job) == CC_BANNED)
            {
                cprintf("    %s N/A", get_class_name(which_job));
            }
            else
            {
                char sletter = index_to_letter(i);

                if (sletter == Options.prev_cls)
                    prevclassok = true;

                cprintf("%c - %s", sletter, get_class_name(which_job));
            }

            if (j % 2)
                cprintf(EOL);
            else
                cgotoxy(31, wherey());

            textcolor(LIGHTGREY); // Reset text colour.

            j++;
        }

        if (j % 2)
            cprintf(EOL);

        textcolor( BROWN );
        if (you.species == SP_UNKNOWN)
        {
            cprintf(EOL
                    "Space - Choose species first; * - Random job; "
                    "+ - Good random job" EOL
                    "! - Random character; # - Good random character; X - Quit"
                    EOL);
        }
        else
        {
            cprintf(EOL
                    "* - Random; + - Good random; "
                    "Bksp - Back to species selection; X - Quit"
                    EOL);
        }

        if (Options.prev_cls)
        {
            if (prevclassok)
            {
                cprintf("Enter - %s",
                        _get_opt_class_name(Options.prev_cls).c_str());
            }
            if (_prev_startup_options_set())
            {
                cprintf("%sTab - %s",
                        prevclassok? "; " : "",
                        _prev_startup_description().c_str());
            }
            cprintf(EOL);
        }

        textcolor( CYAN );
        cprintf(EOL "Which one? ");
        textcolor( LIGHTGREY );

        printed = true;
    }

    if (Options.cls != 0)
        keyn = Options.cls;
    else
        keyn = c_getch();

    bool good_random = false;
    switch (keyn)
    {
    case 'X':
        cprintf(EOL "Goodbye!");
        end(0);
        break;
     case CK_BKSP:
     case ESCAPE:
     case ' ':
        if (keyn != ' ' || you.species == SP_UNKNOWN)
        {
            you.char_class = JOB_UNKNOWN;
            return (false);
        }
    case 'T':
    case CONTROL('T'):
        return pick_tutorial();
    case '#':
        good_random = true;
        // intentional fall-through
    case '!':
        _pick_random_species_and_class(good_random);
        // used to give random weapon/god as well
        Options.random_pick = true;
        ng_random = true;
        if (good_random)
            Options.good_random = true;
        return (true);
    case '\r':
    case '\n':
        if (Options.prev_cls && prevclassok)
            keyn = Options.prev_cls;
        break;
    case '\t':
        if (_prev_startup_options_set())
        {
            if (Options.prev_randpick
                || Options.prev_race == '*' && Options.prev_cls == '*')
            {
                Options.random_pick = true;
                ng_random = true;
                _pick_random_species_and_class(Options.good_random);
                return (true);
            }
            _set_startup_options();

            // Toss out old species selection, if any.
            you.species = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;
            return (false);
        }
        break;
    // help files
    case '?':
        list_commands('2');
        return choose_class();
    case '%':
        list_commands('%');
        return choose_class();
    default:
        break;
    }

    job_type chosen_job = JOB_UNKNOWN;
    good_random = (keyn == '+');
    if (good_random || keyn == '*')
    {
        // Pick a job at random... see god retribution for proof this
        // is uniform. -- bwr
        int job_count = 0;

        job_type job;
        for (int i = 0; i < NUM_JOBS; i++)
        {
            job = static_cast<job_type>(i);
            if (good_random && (job == JOB_THIEF || job == JOB_WANDERER))
                continue;

            if (you.species == SP_UNKNOWN
                || is_good_combination(you.species, job, good_random))
            {
                job_count++;
                if (one_chance_in( job_count ))
                    chosen_job = job;
            }
        }
        ASSERT( chosen_job != JOB_UNKNOWN );
    }
    else if (keyn >= 'a' && keyn <= 'z' || keyn >= 'A' && keyn <= 'Z')
        chosen_job = get_class(letter_to_index(keyn));

    if (chosen_job == JOB_UNKNOWN)
    {
        if (Options.cls != 0)
        {
            Options.cls = 0;
            printed = false;
        }
        goto job_query;
    }

    if (you.species != SP_UNKNOWN
        && !class_allowed(you.species, chosen_job))
    {
        if (Options.cls != 0)
        {
            Options.cls = 0;
            printed = false;
        }
        goto job_query;
    }

    ng_cls = keyn;

    you.char_class = chosen_job;
    return (you.char_class != JOB_UNKNOWN && you.species != SP_UNKNOWN);
}

bool _needs_butchering_tool()
{
    // Mummies/Vampires don't eat.
    // Ghouls have claws (see below).
    if (you.is_undead)
        return (false);

    // Trolls have claws.
    if (you.has_claws())
        return (false);

    // Spriggans don't eat meat.
    if (you.mutation[MUT_HERBIVOROUS] == 3)
        return (false);

    return (true);
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

static int _start_to_wand(int wandtype, bool& is_rod)
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

static bool _choose_wand()
{
    // Wand-choosing interface for Artificers -- Greenberg/Bane

    const wand_type startwand[5] = { WAND_ENSLAVEMENT, WAND_CONFUSION,
                                     WAND_MAGIC_DARTS, WAND_FROST, WAND_FLAME };
    _make_rod(you.inv[2], STAFF_STRIKING, 8);
    const int num_choices = 6;

    int keyin = 0;
    int wandtype;
    bool is_rod;
    if (Options.wand)
    {
        if (_start_to_wand(Options.wand, is_rod) != -1)
        {
            keyin = 'a' + Options.wand - 1;
            ng_wand = Options.wand;
            goto wand_done;
        }
    }

    if (Options.prev_wand)
    {
        if (_start_to_wand(Options.prev_wand, is_rod) == -1
            && Options.prev_wand != SWT_RANDOM)
        {
            Options.prev_wand = SWT_NO_SELECTION;
        }
    }

    if (!Options.random_pick && Options.wand != SWT_RANDOM)
    {
        _print_character_info();

        textcolor( CYAN );
        cprintf(EOL "You have a choice of tools:" EOL
                    "(Press %% for a list of aptitudes)" EOL);

        bool prevmatch = false;
        for (int i = 0; i < num_choices; i++)
        {
            textcolor(LIGHTGREY);

            const char letter = 'a' + i;
            if (i == num_choices - 1)
            {
                cprintf("%c - %s" EOL, letter,
                        you.inv[2].name(DESC_QUALNAME, false, true).c_str());
                wandtype = you.inv[2].sub_type;
                is_rod = true;
            }
            else
            {
                cprintf("%c - %s" EOL, letter, wand_type_name(startwand[i]));
                wandtype = startwand[i];
                is_rod = false;
            }

            if (Options.prev_wand == _wand_to_start(wandtype, is_rod))
                prevmatch = true;
        }

        if (!prevmatch && Options.prev_wand != SWT_RANDOM)
            Options.prev_wand = SWT_NO_SELECTION;

        textcolor(BROWN);
        cprintf(EOL "* - Random choice; "
                    "Bksp - Back to species and class selection; "
                    "X - Quit" EOL);

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
        cprintf(EOL);

        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which tool? ");
            textcolor( LIGHTGREY );

            keyin = c_getch();

            switch (keyin)
            {
            case 'X':
                cprintf(EOL "Goodbye!");
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
                                wandtype = you.inv[2].sub_type;
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
                return _choose_wand();
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

wand_done:

    if (keyin - 'a' == num_choices - 1)
    {
        // Choose the rod; we're all done.
        return (true);
    }

    // 1 wand of random effects and one chosen lesser wand
    const wand_type choice = startwand[keyin - 'a'];
    const int ncharges = 15;
    _newgame_make_item(2, EQ_NONE, OBJ_WANDS, WAND_RANDOM_EFFECTS, -1, 1,
                       ncharges, 0);
    _newgame_make_item(3, EQ_NONE, OBJ_WANDS, choice, -1, 1, ncharges, 0);

    return (true);
}

bool _give_items_skills()
{
    char keyn;
    int weap_skill = 0;
    int choice;                 // used for third-screen choices

    ng.init(you); // XXX

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return (false);

        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        // Skills.
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_SHIELDS]  = 2;

        weap_skill = 2;

        you.skills[(player_light_armour() ? SK_DODGING : SK_ARMOUR)] = 3;

        break;

    case JOB_GLADIATOR:
    {
        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        // No unarmed for Trolls/Ghouls this time.
        if (!_choose_weapon())
            return (false);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        int curr = 3;
        if (you_can_wear(EQ_HELMET))
        {
            _newgame_make_item(3, EQ_HELMET, OBJ_ARMOUR, ARM_HELMET);
            curr++;
        }

        // Small races get stones, the others nets.
        if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM)
            _newgame_make_item(curr, EQ_NONE, OBJ_MISSILES, MI_STONE, -1, 20);
        else
        {
            _newgame_make_item(curr, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               4);
        }

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_SHIELDS]  = 2;
        you.skills[SK_UNARMED_COMBAT] = 2;
        weap_skill = 3;
        break;
    }

    case JOB_MONK:
        you.equip[EQ_WEAPON] = -1; // Monks fight unarmed.

        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        you.skills[SK_FIGHTING]       = 3;
        you.skills[SK_UNARMED_COMBAT] = 4;
        you.skills[SK_DODGING]        = 3;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        // WEAPONS
        if (you.has_claws())
            you.equip[EQ_WEAPON] = -1; // Trolls/Ghouls fight unarmed.
        else
        {
            // Races skilled with maces/flails get one, the others axes.
            weapon_type startwep = WPN_HAND_AXE;
            if (species_skills(SK_MACES_FLAILS, you.species) <
                species_skills(SK_AXES, you.species))
            {
                startwep = (player_genus(GENPC_OGRE)) ? WPN_ANKUS : WPN_MACE;
            }

            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, startwep);
        }

        // ARMOUR
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ANIMAL_SKIN);

        // SKILLS
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_DODGING]  = 2;
        weap_skill = 3;

        if (you_can_wear(EQ_BODY_ARMOUR))
            you.skills[SK_ARMOUR] = 2;
        else
        {
            you.skills[SK_DODGING]++;
            you.skills[SK_ARMOUR] = 1; // for the eventual dragon scale mail :)
        }
        break;

    case JOB_PALADIN:
        you.religion = GOD_SHINING_ONE;
        you.piety = 28;

        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_FALCHION);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_RING_MAIL,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);
        _newgame_make_item(3, EQ_NONE, OBJ_POTIONS, POT_HEALING);

        // Skills.
        you.skills[(player_light_armour() ? SK_DODGING : SK_ARMOUR)] = 2;
        you.skills[SK_FIGHTING]    = 2;
        you.skills[SK_SHIELDS]     = 2;
        you.skills[SK_LONG_BLADES] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_PRIEST:
        you.piety = 45;

        // Set gods.
        if (you.species == SP_DEMONSPAWN || you.species == SP_MUMMY
            || you.species == SP_GHOUL || you.species == SP_VAMPIRE)
        {
            you.religion = GOD_YREDELEMNUL;
        }
        else
        {
            const god_type gods[3] = { GOD_ZIN, GOD_YREDELEMNUL, GOD_BEOGH };

            // Disallow invalid choices.
            if (religion_restriction(Options.priest, ng) == CC_BANNED)
                Options.priest = GOD_NO_GOD;

            if (Options.priest != GOD_NO_GOD && Options.priest != GOD_RANDOM)
                ng_pr = you.religion = static_cast<god_type>(Options.priest);
            else if (Options.random_pick || Options.priest == GOD_RANDOM)
            {
                bool did_chose = false;
                if (Options.good_random)
                {
                    int count = 0;
                    for (int i = 0; i < 3; i++)
                    {
                        if (religion_restriction(gods[i], ng) == CC_BANNED)
                            continue;

                        if (religion_restriction(gods[i], ng) == CC_UNRESTRICTED
                            && one_chance_in(++count))
                        {
                            you.religion = gods[i];
                            did_chose = true;
                        }
                    }
                }

                if (!did_chose)
                {
                    you.religion = (coinflip() ? GOD_YREDELEMNUL : GOD_ZIN);

                    // For orcs 50% chance of Beogh instead.
                    if (you.species == SP_HILL_ORC && coinflip())
                        you.religion = GOD_BEOGH;
                }
                ng_pr = GOD_RANDOM;
            }
            else
            {
                _print_character_info();

                textcolor( CYAN );
                cprintf(EOL "Which god do you wish to serve?" EOL);

                const char* god_name[3] = {"Zin (for traditional priests)",
                                           "Yredelemnul (for priests of death)",
                                           "Beogh (for priests of Orcs)"};

                for (int i = 0; i < 3; i++)
                {
                    if (religion_restriction(gods[i], ng) == CC_BANNED)
                        continue;

                    if (religion_restriction(gods[i], ng) == CC_UNRESTRICTED)
                        textcolor(LIGHTGREY);
                    else
                        textcolor(DARKGREY);

                    const char letter = 'a' + i;
                    cprintf("%c - %s" EOL, letter, god_name[i]);
                }

                textcolor( BROWN );
                cprintf(EOL "* - Random choice; + - Good random choice" EOL
                            "Bksp - Back to species and class selection; "
                            "X - Quit" EOL);

                if (religion_restriction(Options.prev_pr, ng) == CC_BANNED)
                    Options.prev_pr = GOD_NO_GOD;

                if (Options.prev_pr != GOD_NO_GOD)
                {
                    textcolor(BROWN);
                    cprintf(EOL "Enter - %s" EOL,
                            Options.prev_pr == GOD_ZIN         ? "Zin" :
                            Options.prev_pr == GOD_YREDELEMNUL ? "Yredelemnul" :
                            Options.prev_pr == GOD_BEOGH       ? "Beogh"
                                                               : "Random");
                }

                do
                {
                    keyn = c_getch();

                    switch (keyn)
                    {
                    case 'X':
                        cprintf(EOL "Goodbye!");
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
                               && you.species != SP_HILL_ORC)
                        {
                            break;
                        }
                        if (Options.prev_pr != GOD_RANDOM)
                        {
                            Options.prev_pr
                                     = static_cast<god_type>(Options.prev_pr);
                            you.religion = Options.prev_pr;
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
                                if (religion_restriction(gods[i], ng) == CC_BANNED)
                                    continue;

                                if (religion_restriction(gods[i], ng)
                                        == CC_UNRESTRICTED
                                    && one_chance_in(++count))
                                {
                                    you.religion = gods[i];
                                }
                            }
                            if (count > 0)
                                break;
                        }
                        // intentional fall-through
                    case '*':
                        you.religion = coinflip() ? GOD_ZIN : GOD_YREDELEMNUL;
                        if (you.species == SP_HILL_ORC && coinflip())
                            you.religion = GOD_BEOGH;
                        break;
                    case 'a':
                        you.religion = GOD_ZIN;
                        break;
                    case 'b':
                        you.religion = GOD_YREDELEMNUL;
                        break;
                    case 'c':
                        if (you.species == SP_HILL_ORC)
                        {
                            you.religion = GOD_BEOGH;
                            break;
                        } // else fall through
                    default:
                        break;
                    }
                }
                while (you.religion == GOD_NO_GOD);

                ng_pr = (keyn == '*' ? GOD_RANDOM : you.religion);
            }
        }

        if (you.religion == GOD_BEOGH)
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_HAND_AXE);
        else
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_QUARTERSTAFF);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        if (you.religion == GOD_ZIN)
            _newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEALING, -1, 2);

        you.skills[SK_FIGHTING]    = 2;
        you.skills[SK_INVOCATIONS] = 5;
        you.skills[SK_DODGING]     = 1;
        weap_skill = 3;
        break;

    case JOB_CHAOS_KNIGHT:
    {
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1,
                           2);

        if (!_choose_weapon())
            return (false);

        const god_type gods[3] = { GOD_XOM, GOD_MAKHLEB, GOD_LUGONU };

        if (Options.chaos_knight != GOD_NO_GOD
            && Options.chaos_knight != GOD_RANDOM)
        {
            ng_ck = you.religion =
                static_cast<god_type>( Options.chaos_knight );
        }
        else if (Options.random_pick || Options.chaos_knight == GOD_RANDOM)
        {
            bool did_chose = false;
            if (Options.good_random)
            {
                int count = 0;
                for (int i = 0; i < 3; i++)
                {
                    if (religion_restriction(gods[i], ng) == CC_BANNED)
                        continue;

                    if (religion_restriction(gods[i], ng) == CC_UNRESTRICTED
                        && one_chance_in(++count))
                    {
                        you.religion = gods[i];
                        did_chose = true;
                    }
                }
            }

            if (!did_chose)
            {
                you.religion = (one_chance_in(3) ? GOD_XOM :
                                coinflip()       ? GOD_MAKHLEB
                                                 : GOD_LUGONU);
            }
            ng_ck = GOD_RANDOM;
        }
        else
        {
            _print_character_info();

            textcolor( CYAN );
            cprintf(EOL "Which god of chaos do you wish to serve?" EOL);

            const char* god_name[3] = {"Xom of Chaos",
                                       "Makhleb the Destroyer",
                                       "Lugonu the Unformed"};

            for (int i = 0; i < 3; i++)
            {
                if (religion_restriction(gods[i], ng) == CC_BANNED)
                    continue;

                if (religion_restriction(gods[i], ng) == CC_UNRESTRICTED)
                    textcolor(LIGHTGREY);
                else
                    textcolor(DARKGREY);

                const char letter = 'a' + i;
                cprintf("%c - %s" EOL, letter, god_name[i]);
            }

            textcolor( BROWN );
            cprintf(EOL "* - Random choice; + - Good random choice" EOL
                        "Bksp - Back to species and class selection; "
                        "X - Quit" EOL);

            if (Options.prev_ck != GOD_NO_GOD)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_ck == GOD_XOM     ? "Xom" :
                        Options.prev_ck == GOD_MAKHLEB ? "Makhleb" :
                        Options.prev_ck == GOD_LUGONU  ? "Lugonu"
                                                       : "Random");
                textcolor(LIGHTGREY);
            }

            do
            {
                keyn = c_getch();

                switch (keyn)
                {
                case 'X':
                    cprintf(EOL "Goodbye!");
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
                        you.religion = static_cast<god_type>(Options.prev_ck);
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
                            if (religion_restriction(gods[i], ng) == CC_BANNED)
                                continue;

                            if (religion_restriction(gods[i], ng)
                                    == CC_UNRESTRICTED
                                && one_chance_in(++count))
                            {
                                you.religion = gods[i];
                            }
                        }
                        if (count > 0)
                            break;
                    }
                    // intentional fall-through
                case '*':
                    you.religion = (one_chance_in(3) ? GOD_XOM :
                                    coinflip()       ? GOD_MAKHLEB
                                                     : GOD_LUGONU);
                    break;
                case 'a':
                    you.religion = GOD_XOM;
                    break;
                case 'b':
                    you.religion = GOD_MAKHLEB;
                    break;
                case 'c':
                    you.religion = GOD_LUGONU;
                    break;
                default:
                    break;
                }
            }
            while (you.religion == GOD_NO_GOD);

            ng_ck = (keyn == '*') ? GOD_RANDOM
                                  : you.religion;
        }

        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);
        else
            you.inv[0].plus2 = 4 - you.inv[0].plus;

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE, 1, you.religion == GOD_XOM ? 2 : 0);

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR]   = 1;
        you.skills[SK_DODGING]  = 1;
        if (species_skills(SK_ARMOUR, you.species) >
            species_skills(SK_DODGING, you.species))
        {
            you.skills[SK_DODGING]++;
        }
        else
            you.skills[SK_ARMOUR]++;
        weap_skill = 2;

        if (you.religion == GOD_XOM)
        {
            you.skills[SK_FIGHTING]++;
            // The new (piety-aware) Xom uses piety in his own special way...
            // (Namely, 100 is neutral.)
            you.piety = 100;

            // The new Xom also uses gift_timeout in his own special way...
            // (Namely, a countdown to becoming bored.)
            you.gift_timeout = std::max(5, random2(40) + random2(40));
        }
        else // Makhleb or Lugonu
        {
            you.skills[SK_INVOCATIONS] = 2;

            if (you.religion == GOD_LUGONU)
            {
                // Chaos Knights of Lugonu start in the Abyss.  We need
                // to mark this unusual occurrence, so the player
                // doesn't get early access to OOD items, etc.
                you.char_direction = GDT_GAME_START;
                you.piety = 38;
            }
            else
                you.piety = 25;
        }
        break;
    }
    case JOB_DEATH_KNIGHT:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return (false);

        choice = DK_NO_SELECTION;

        // Order is important here. -- bwr
        if (you.species == SP_DEMIGOD)
            choice = DK_NECROMANCY;
        else if (Options.death_knight != DK_NO_SELECTION
                 && Options.death_knight != DK_RANDOM)
        {
            ng_dk = choice = Options.death_knight;
        }
        else if (Options.random_pick || Options.death_knight == DK_RANDOM)
        {
            ng_dk = DK_RANDOM;

            bool did_chose = false;
            if (Options.good_random)
            {
                if (_necromancy_okay())
                {
                    choice = DK_NECROMANCY;
                    did_chose = true;
                }

                if (religion_restriction(GOD_YREDELEMNUL, ng) == CC_UNRESTRICTED)
                {
                    if (!did_chose || coinflip())
                        choice = DK_YREDELEMNUL;
                    did_chose = true;
                }
            }

            if (!did_chose)
                choice = (coinflip() ? DK_NECROMANCY : DK_YREDELEMNUL);
        }
        else
        {
            _print_character_info();

            textcolor(CYAN);
            cprintf(EOL "From where do you draw your power?" EOL);
            textcolor(_necromancy_okay() ? LIGHTGREY : DARKGREY);
            cprintf("a - Necromantic magic" EOL);

            // Yredelemnul is an okay choice for everyone.
            if (religion_restriction(GOD_YREDELEMNUL, ng) == CC_UNRESTRICTED)
                textcolor(LIGHTGREY);
            else
                textcolor(DARKGREY);

            cprintf("b - the god Yredelemnul" EOL);

            textcolor( BROWN );
            cprintf(EOL "* - Random choice; + - Good random choice " EOL
                        "Bksp - Back to species and class selection; "
                        "X - Quit" EOL);

            if (Options.prev_dk != DK_NO_SELECTION)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_dk == DK_NECROMANCY  ? "Necromancy" :
                        Options.prev_dk == DK_YREDELEMNUL ? "Yredelemnul"
                                                          : "Random");
            }

            do
            {
                keyn = c_getch();

                switch (keyn)
                {
                case 'X':
                    cprintf(EOL "Goodbye!");
                    end(0);
                    break;
                case CK_BKSP:
                case ESCAPE:
                case ' ':
                    return (false);
                case '\r':
                case '\n':
                    if (Options.prev_dk == DK_NO_SELECTION)
                        break;

                    if (Options.prev_dk != DK_RANDOM)
                    {
                        choice = Options.prev_dk;
                        break;
                    }
                    keyn = '*'; // for ng_dk setting
                    // fall-through for random
                case '+':
                    if (keyn == '+')
                    {
                        bool did_chose = false;
                        if (_necromancy_okay())
                        {
                            choice = DK_NECROMANCY;
                            did_chose = true;
                        }

                        if (religion_restriction(GOD_YREDELEMNUL, ng)
                                == CC_UNRESTRICTED)
                        {
                            if (!did_chose || coinflip())
                                choice = DK_YREDELEMNUL;
                            did_chose = true;
                        }
                        if (did_chose)
                            break;
                    }
                    // fall-through for random
                case '*':
                    choice = coinflip()? DK_NECROMANCY : DK_YREDELEMNUL;
                    break;
                case 'a':
                    cprintf(EOL "Very well.");
                    choice = DK_NECROMANCY;
                    break;
                case 'b':
                    choice = DK_YREDELEMNUL;
                default:
                    break;
                }
            }
            while (choice == DK_NO_SELECTION);

            ng_dk = (keyn == '*'? DK_RANDOM : choice);
        }

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                              ARM_ROBE);

        switch (choice)
        {
        default:  // This shouldn't happen anyways. -- bwr
        case DK_NECROMANCY:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

            you.skills[SK_SPELLCASTING] = 1;
            you.skills[SK_NECROMANCY]   = 2;
            break;
        case DK_YREDELEMNUL:
            you.religion = GOD_YREDELEMNUL;
            you.piety = 28;
            you.inv[0].plus  = 2;
            you.inv[0].plus2 = 2;
            you.skills[SK_INVOCATIONS] = 3;
            break;
        }

        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR]   = 1;
        you.skills[SK_DODGING]  = 1;
        weap_skill = 2;
        break;

    case JOB_HEALER:
        you.religion = GOD_ELYVILON;
        you.piety = 55;

        you.equip[EQ_WEAPON] = -1; // Healers fight unarmed.

        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_POTIONS, POT_HEALING);
        _newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEAL_WOUNDS);

        you.skills[SK_FIGHTING]       = 2;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_DODGING]        = 1;
        you.skills[SK_INVOCATIONS]    = 4;
        break;

    case JOB_CRUSADER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return (false);

        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_WAR_CHANTS);

        you.skills[SK_FIGHTING]     = 3;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_ENCHANTMENTS] = 2;
        weap_skill = 2;
        break;

    case JOB_REAVER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return (false);

        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);

        if (!_choose_book(2, BOOK_CONJURATIONS_I, 2))
            return (false);

        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_CONJURATIONS] = 2;
        weap_skill = 3;
        break;

    case JOB_WARPER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
 
        if (!_choose_weapon())
            return (false);
 
        if (you.inv[0].quantity < 1)
            _newgame_clear_item(0);
 
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_SPATIAL_TRANSLOCATIONS);

        // One free escape.
        _newgame_make_item(3, EQ_NONE, OBJ_SCROLLS, SCR_BLINKING);
        _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 20);

        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_ARMOUR]         = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSLOCATIONS] = 3;
        you.skills[SK_DARTS]          = 1;
        weap_skill = 3;
    break;

    case JOB_ARCANE_MARKSMAN:

        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        switch (you.species)
        {
        case SP_SLUDGE_ELF:
        case SP_HILL_ORC:
        case SP_MERFOLK:
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, -1, 6);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               2);
            break;

        case SP_TROLL:
        case SP_OGRE:
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_LARGE_ROCK, -1, 5);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               3);
            break;

        case SP_HALFLING:
        case SP_KOBOLD:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1,
                               30);

            // Wield the sling instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_CROSSBOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 25);

            // Wield the crossbow instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        default:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 25);

            // Wield the bow instead.
            you.equip[EQ_WEAPON] = 1;
            break;
        }

        if (is_range_weapon(you.inv[1]))
            you.skills[range_skill(you.inv[1])] = 3;
        else
            you.skills[SK_THROWING] = 3;

        if (!_choose_book(3, BOOK_ELEMENTAL_MISSILES, 2))
            return (false);

        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        
        switch (you.inv[3].sub_type)
        {
        case BOOK_ELEMENTAL_MISSILES:
            you.skills[SK_ENCHANTMENTS] = 3;
            break;
        case BOOK_WARPED_MISSILES:
            you.skills[SK_TRANSLOCATIONS] = 3;
            break;
        }
        break;

    case JOB_WIZARD:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_HELMET, OBJ_ARMOUR, ARM_WIZARD_HAT);

        if (!_choose_book(2, BOOK_MINOR_MAGIC_I, 3))
            return (false);

        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        you.skills[SK_SPELLCASTING]   = 3;
        // All three starting books contain Translocations spells.
        you.skills[SK_TRANSLOCATIONS] = 1;

        // The other two schools depend on the chosen book.
        switch (you.inv[2].sub_type)
        {
        case BOOK_MINOR_MAGIC_I:
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_FIRE_MAGIC]   = 1;
            break;
        case BOOK_MINOR_MAGIC_II:
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_ICE_MAGIC]    = 1;
            break;
        case BOOK_MINOR_MAGIC_III:
            you.skills[SK_SUMMONINGS]   = 1;
            you.skills[SK_ENCHANTMENTS] = 1;
            break;
        }
        break;

    case JOB_CONJURER:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        if (!_choose_book(1, BOOK_CONJURATIONS_I, 2))
            return (false);

        you.skills[SK_CONJURATIONS] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ENCHANTER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1, 1,
                           1);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE, -1, 1, 1);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_CHARMS);

        // Gets some darts - this class is difficult to start off with.
        _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 16, 1);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)
        if (you.species == SP_SPRIGGAN)
            you.inv[0].sub_type = WPN_DAGGER;
        if (you.species == SP_OGRE || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 1;
        you.skills[SK_DARTS]        = 1;
        you.skills[SK_ENCHANTMENTS] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_SUMMONER:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_CALLINGS);

        you.skills[SK_SUMMONINGS]   = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_NECROMANCER:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_NECROMANCY]   = 4;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_TRANSMUTER:
        you.equip[EQ_WEAPON] = -1; // Transmuters fight unarmed.

        // Some sticks for sticks to snakes.
        _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 12);
        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_CHANGES);

        // A little bit of starting ammo for evaporate... don't need too
        // much now that the character can make their own. - bwr
        _newgame_make_item(4, EQ_NONE, OBJ_POTIONS, POT_CONFUSION, -1, 2);
        _newgame_make_item(5, EQ_NONE, OBJ_POTIONS, POT_POISON);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)

        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSMUTATIONS] = 2;
        break;

    case JOB_FIRE_ELEMENTALIST:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_FLAMES);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_FIRE_MAGIC]   = 3;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ICE_ELEMENTALIST:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_FROST);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_ICE_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_AIR_ELEMENTALIST:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_AIR);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_AIR_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_EARTH_ELEMENTALIST:
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_GEOMANCY);
        _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_STONE, -1, 20);

        you.skills[SK_TRANSMUTATIONS] = 1;
        you.skills[SK_EARTH_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING]   = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_VENOM_MAGE:
        // Venom Mages don't need a starting weapon since acquiring a weapon
        // to poison should be easy, and Sting is *powerful*.
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_YOUNG_POISONERS);

        you.skills[SK_POISON_MAGIC] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_STALKER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, 2, 2);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);
        _newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_STALKING);

        if (you.species == SP_OGRE || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        you.skills[SK_FIGHTING]     = 1;
        weap_skill = 1;
        you.skills[SK_POISON_MAGIC] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        you.skills[SK_STABBING]     = 2;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_ENCHANTMENTS] = 1;
        break;

    case JOB_THIEF:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_HAND_CROSSBOW);

        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);

        _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 20);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)
        if (you.species == SP_SPRIGGAN)
            you.inv[0].sub_type = WPN_DAGGER;
        if (you.species == SP_OGRE || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 2;
        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 2;
        you.skills[SK_STABBING] = 2;
        you.skills[SK_TRAPS_DOORS] = 2;
        you.skills[SK_CROSSBOWS] = 1;
        break;

    case JOB_ASSASSIN:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, 2, 2);
        _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BLOWGUN);

        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);

        _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 10);
        set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_POISONED);
        _newgame_make_item(5, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 3);
        set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_CURARE);

        if (you.species == SP_OGRE || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 2;
        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_STEALTH]      = 3;
        you.skills[SK_STABBING]     = 2;
        you.skills[SK_DARTS]        = 2;
        break;

    case JOB_HUNTER:
        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);

        switch (you.species)
        {
        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_CENTAUR:
        case SP_OGRE:
            you.inv[0].sub_type = WPN_HAND_AXE;
            break;
        case SP_GHOUL:
        case SP_TROLL:
            _newgame_clear_item(0);
            break;
        default:
            break;
        }

        switch (you.species)
        {
        case SP_SLUDGE_ELF:
        case SP_HILL_ORC:
        case SP_MERFOLK:
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, -1, 6);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               2);
            break;

        case SP_TROLL:
        case SP_OGRE:
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_LARGE_ROCK, -1, 5);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               3);
            break;

        case SP_HALFLING:
        case SP_KOBOLD:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1,
                               30);

            // Wield the sling instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_CROSSBOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 25);

            // Wield the crossbow instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        default:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 25);

            // Wield the bow instead.
            you.equip[EQ_WEAPON] = 1;
            break;
        }

        _newgame_make_item(3, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 1;
        weap_skill = 1;

        if (is_range_weapon(you.inv[1]))
            you.skills[range_skill(you.inv[1])] = 4;
        else
            you.skills[SK_THROWING] = 4;
        break;

    case JOB_WANDERER:
        _create_wanderer();
        break;

    case JOB_ARTIFICER:
        // Equipment. Dagger, and armour or robe.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR,
                           ARM_LEATHER_ARMOUR, ARM_ROBE);

        // Choice of lesser wands, 15 charges plus wand of random
        // effects: confusion, enslavement, slowing, magic dart, frost,
        // flame; OR a rod of striking, 8 charges and no random effects.
        if (!_choose_wand())
            return (false);

        // If an offensive wand or the rod of striking was chosen,
        // don't hand out a weapon.
        if (you.inv[3].base_type != OBJ_WANDS
            || you.inv[3].sub_type != WAND_CONFUSION
               && you.inv[3].sub_type != WAND_ENSLAVEMENT)
        {
            _newgame_clear_item(0);
        }

        // Skills
        you.skills[SK_EVOCATIONS]  = 4;
        you.skills[SK_TRAPS_DOORS] = 3;
        you.skills[SK_DODGING]     = 2;
        you.skills[SK_FIGHTING]    = 1;
        you.skills[SK_STEALTH]     = 1;
        break;

    default:
        break;
    }

    // Deep Dwarves get healing potions and wand of healing (3).
    if (you.species == SP_DEEP_DWARF)
    {
        _newgame_make_item(-1, EQ_NONE, OBJ_POTIONS, POT_HEALING, -1, 2);
        _newgame_make_item(-1, EQ_NONE, OBJ_POTIONS, POT_HEAL_WOUNDS, -1, 2);
        _newgame_make_item(-1, EQ_NONE, OBJ_WANDS, WAND_HEALING, -1, 1, 3);
    }

    if (weap_skill)
    {
        if (!you.weapon())
            you.skills[SK_UNARMED_COMBAT] = weap_skill;
        else
            you.skills[weapon_skill(*you.weapon())] = weap_skill;
    }

    init_skill_order();

    if (you.religion != GOD_NO_GOD)
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
    }

    return (true);
}

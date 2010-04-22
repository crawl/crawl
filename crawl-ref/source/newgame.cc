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
#include "database.h"
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
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "sprint.h"
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
bool _choose_species(void);
bool _choose_job(void);
static bool _choose_weapon();
static bool _choose_book();
static bool _choose_god();
static bool _choose_wand();

static void _create_wanderer(void);
static bool _give_items_skills(void);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

static newgame_def ng;

newgame_def::newgame_def()
    : name(), species(NUM_SPECIES), job(NUM_JOBS),
      weapon(NUM_WEAPONS), book(SBT_NO_SELECTION),
      religion(GOD_NO_GOD), wand(SWT_NO_SELECTION)
{
}

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

// Output full character info when weapons/books/religion are chosen.
static void _print_character_info()
{
    clrscr();

    // At this point all of name, species and job should be decided.
    if (!you.your_name.empty()
        && you.char_class != JOB_UNKNOWN && you.species != SP_UNKNOWN)
    {
        cprintf("Welcome, ");
        textcolor( YELLOW );
        cprintf("%s the %s %s.\n", you.your_name.c_str(),
                species_name(you.species, 1).c_str(),
                get_job_name(you.char_class));
    }
}

static void _print_character_info_ng()
{
    clrscr();

    // At this point all of name, species and job should be decided.
    if (!ng.name.empty()
        && ng.job != JOB_UNKNOWN && ng.species != SP_UNKNOWN)
    {
        cprintf("Welcome, ");
        textcolor( YELLOW );
        cprintf("%s the %s %s.\n", ng.name.c_str(),
                species_name(ng.species, 1).c_str(),
                get_job_name(ng.job));
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

static void _pick_random_species_and_job( bool unrestricted_only )
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
        you.species = random_draconian_player_species();
    else
        you.species = species;

    you.char_class = job;
}

// Returns true if a save game exists with name you.your_name.
static bool _check_saved_game(void)
{
    FILE *handle;

    std::string basename = get_savedir_filename(you.your_name, "", "");
    std::string savename = basename + ".chr";

#ifdef LOAD_UNPACKAGE_CMD
    std::string zipname = basename + PACKAGE_SUFFIX;
    handle = fopen(zipname.c_str(), "rb+");
    if (handle != NULL)
    {
        fclose(handle);
        cprintf("\nLoading game...\n");

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
            || player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
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

    for (int i = 0; i < NUM_STATS; ++i)
    {
        int j = (i + 1) % NUM_STATS;
        int k = (i + 2) % NUM_STATS;
        if ((needed = MIN_START_STAT - you.base_stats[i]) > 0)
        {
            if (you.base_stats[j] > you.base_stats[k])
                you.base_stats[j] -= needed;
            else
                you.base_stats[k] -= needed;
            you.base_stats[i] = MIN_START_STAT;
        }
    }
}

// Randomly boost stats a number of times.
static void _wanderer_assign_remaining_stats(int points_left)
{
    while (points_left > 0)
    {
        // Stats that are already high will be chosen half as often.
        stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
        if (you.base_stats[stat] > 17 && coinflip())
            continue;

        you.base_stats[stat]++;
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

void _setup_tutorial_character()
{
    you.species = SP_HIGH_ELF;
    you.char_class = JOB_FIGHTER;

    Options.weapon = WPN_MACE;
}

static void _newgame_make_item(int, equipment_type, object_class_type base,
                               int, int, int, int, int);
void _setup_tutorial_miscs()
{
    // Give him spellcasting
    you.skills[SK_SPELLCASTING] = 3;
    you.skills[SK_CONJURATIONS] = 1;
    // Give him some mana to play around with
    inc_max_mp(2);
    // Get rid of the starting items!
    you.equip[EQ_WEAPON] = -1;
    you.equip[EQ_BODY_ARMOUR] = -1;
    you.equip[EQ_SHIELD] = -1;
    _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE, -1,
                       1, 0, 0);
    _newgame_make_item(1, EQ_BOOTS, OBJ_ARMOUR, ARM_BOOTS, -1, 1, 0, 0);
    _newgame_make_item(2, EQ_WEAPON, OBJ_WEAPONS, WPN_CLUB, -1, 1, 0, 0);
    // Make him hungry for the butchering tutorial
    you.hunger = 2650;
}

bool new_game(const std::string& name)
{
    clrscr();
    _init_player();

    if (!crawl_state.startup_errors.empty()
        && !Options.suppress_startup_errors)
    {
        crawl_state.show_startup_errors();
        clrscr();
    }

    textcolor(LIGHTGREY);

    opening_screen();

    you.your_name = name;

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
        _pick_random_species_and_job(Options.good_random);
        ng_random = true;
    }
    else if (crawl_state.game_is_tutorial())
    {
        _setup_tutorial_character();
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
        while (_choose_species() && _choose_job());
    }

    // TODO: choose weapon and god here. Get rid of the goto.
    //       Not sure all of the resetting is necessary.
    if (!_choose_weapon() || !_choose_book()
        || !_choose_god() || !_choose_wand())
    {
        // Now choose again, name stays same.
        const std::string old_name = you.your_name;

        Options.prev_randpick = false;
        Options.prev_race     = ng_race;
        Options.prev_cls      = ng_cls;
        Options.prev_weapon   = ng_weapon;
        // ck, pr and book are asked last --> don't need to be changed

        // Reset stats.
        _init_player();

        Options.reset_startup_options();

        you.your_name = old_name;

        // Choose new character.
        goto game_start;
    }

    // Pick random draconian type.
    if (you.species == SP_RED_DRACONIAN)
        you.species = random_draconian_player_species();

    strlcpy(you.class_name, get_job_name(you.char_class), sizeof(you.class_name));

    if (Options.random_pick)
    {
        // For completely random combinations (!, #, or Options.random_pick)
        // reroll characters until the player accepts one of them or quits.
        clrscr();

        std::string specs = species_name(you.species, you.experience_level);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf( "You are a%s %s %s.\n",
                 (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                 you.class_name );

        cprintf("\nDo you want to play this combination? (ynq) [y]");
        char c = getchm();
        if (c == ESCAPE || tolower(c) == 'q')
            end(0);
        if (tolower(c) == 'n')
            goto game_start;
    }

    // New: pick name _after_ species and background choices.
    if (you.your_name.empty())
    {
        clrscr();

        std::string specs = species_name(you.species, you.experience_level);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf( "You are a%s %s %s.\n",
                 (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                 you.class_name );

        ng.init(you);
        enter_player_name(ng);
        ng.save(you);

        if (_check_saved_game())
        {
            cprintf("\nDo you really want to overwrite your old game? ");
            char c = getchm();
            if (c != 'Y' && c != 'y')
            {
                textcolor( BROWN );
                cprintf("\n\nWelcome back, ");
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
        // ck, pr and book are asked last --> don't need to be changed

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

    _give_starting_food();
    if (crawl_state.game_is_sprint()) {
        sprint_give_items();
    }
    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
    {
        _setup_tutorial_miscs();
    }
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

// Recall that demonspawn & demigods get more later on. {dlb}
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

    case SP_MUMMY:              sb =  9; ib =  5; db =  5;      break;  // 19
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

    you.base_stats[STAT_STR] = sb + 2;
    you.base_stats[STAT_INT] = ib + 2;
    you.base_stats[STAT_DEX] = db + 2;
}

static void _jobs_stat_init(job_type which_job)
{
    int s = 0;   // strength mod
    int i = 0;   // intelligence mod
    int d = 0;   // dexterity mod
    int hp = 0;  // HP base
    int mp = 0;  // MP base

    // Note: Wanderers are correct, they've got a challenging background. -- bwr
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

    you.base_stats[STAT_STR] += s;
    you.base_stats[STAT_INT] += i;
    you.base_stats[STAT_DEX] += d;

    // Used for Jiyva's stat swapping if the player has not reached
    // experience level 3.
    you.last_chosen = (stat_type) random2(NUM_STATS);

    set_hp( hp, true );
    set_mp( mp, true );
}

static int _claws_level(species_type sp)
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
        you.mutation[MUT_HOOVES]          = 3;
        break;
    case SP_NAGA:
        you.mutation[MUT_ACUTE_VISION]      = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_DEFORMED]          = 1;
        you.mutation[MUT_SLOW]              = 2;
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
        you.mutation[MUT_TALONS] = 3;
        break;
    case SP_TROLL:
        you.mutation[MUT_TOUGH_SKIN]      = 2;
        you.mutation[MUT_REGENERATION]    = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        you.mutation[MUT_CLAWS]           = 3;
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

    // Some mutations out-sourced because they're
    // relevant during character choice.
    you.mutation[MUT_CLAWS] = _claws_level(speci);

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
    if (wpn_skill == SK_THROWING)
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

    case SK_THROWING:
        you.inv[slot].sub_type = WPN_BLOWGUN;
        break;

    case SK_BOWS:
        you.inv[slot].sub_type = WPN_BOW;
        break;

    case SK_CROSSBOWS:
        you.inv[slot].sub_type = WPN_CROSSBOW;
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
    for (int i = 0; i < NUM_STATS; ++i)
        total_stats += you.stat(static_cast<stat_type>(i));

    int target = random2(total_stats);

    stat_type role;

    if (target < you.strength())
        role = STAT_STR;
    else if (target < (you.dex() + you.strength()))
        role = STAT_DEX;
    else
        role = STAT_INT;

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
    case STAT_DEX:
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

    case STAT_STR:
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

    case STAT_INT:
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
    case STAT_STR:
        skill = _apt_weighted_choice(str_weapons, str_size);
        break;

    case STAT_DEX:
        skill = _apt_weighted_choice(dex_weapons, dex_size);
        break;

    case STAT_INT:
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
    if (role == STAT_INT)
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
    case SK_THROWING:
    case SK_STAVES:
    case SK_SHORT_BLADES:
        _give_wanderer_weapon(slot, skill, 3);
        slot++;
        break;

    case SK_ARMOUR:
        // Deformed species aren't given armor skill, so there's no need
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
    case SK_THROWING:
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
        if (you.dex() > you.strength() || you.skills[SK_STABBING])
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

    // The player needs a stack of bolts if they have a crossbow.
    bool need_bolts = false;

    for (int i = 0; i < slot; ++i)
    {
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == WPN_CROSSBOW)
        {
            need_bolts = true;
            break;
        }
    }

    if (need_bolts)
    {
        _newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1,
                           15 + random2avg(21, 5));
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
        { SK_THROWING, SK_STABBING, SK_TRAPS_DOORS, SK_STEALTH,
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
    if (primary_role == STAT_INT)
        mp_adjust++;
    if (secondary_role == STAT_INT)
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
    if (selected_role == STAT_INT)
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

/**
 * Helper function for _choose_species
 * Constructs the menu screen
 */
static const int COLUMN_WIDTH = 25;
static const int X_MARGIN = 4;
static const int SPECIAL_KEYS_START_Y = 21;
static const int CHAR_DESC_START_Y = 18;
void _construct_species_menu(MenuFreeform* menu)
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

        if (you.char_class == JOB_UNKNOWN
            || job_allowed(species, you.char_class) == CC_UNRESTRICTED)
          {
              tmp->set_fg_colour(LIGHTGRAY);
              tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(YELLOW);
        }
        if (you.char_class != JOB_UNKNOWN
            && job_allowed(species, you.char_class) == CC_BANNED)
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
    if (you.char_class != JOB_UNKNOWN)
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
    if (you.char_class != JOB_UNKNOWN)
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
bool _choose_species()
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
        you.char_class = get_job(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor( BROWN );
    if (you.your_name.empty() && you.char_class == JOB_UNKNOWN)
    {
        cprintf("Welcome. ");
    }
    else if (you.your_name.empty() && you.char_class != JOB_UNKNOWN)
    {
        cprintf("Welcome, the %s. ", get_job_name(you.char_class));
    }
    else if (you.char_class != JOB_UNKNOWN)
    {
        cprintf("Welcome, %s the %s. ", you.your_name.c_str(),
                get_job_name(you.char_class));
    }
    else
    {
        cprintf("Welcome, %s. ", you.your_name.c_str());
    }

    textcolor( YELLOW );
    cprintf("Please select your species");

    _construct_species_menu(freeform);
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
                you.species  = SP_UNKNOWN;
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
                _pick_random_species_and_job(good_random);
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
                        _pick_random_species_and_job(Options.good_random);
                        return (false);
                    }
                    _set_startup_options();
                    you.species = SP_UNKNOWN;
                    you.char_class = JOB_UNKNOWN;
                    return true;
                }
                // ignore Tab because we don't have previous start options set
                continue;
            case ' ':
                you.species  = SP_UNKNOWN;
                Options.race = 0;
                return true;
            case '?':
                 // access to the help files
                list_commands('1');
                return _choose_species();
            case '%':
                list_commands('%');
                return _choose_species();
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
                if (you.char_class == JOB_UNKNOWN)
                {
                    // pick any specie
                    random_index = random2(ng_num_species());
                    you.species = get_species(random_index);
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
                                                you.char_class, good_random));
                    you.species = get_species(random_index);
                    ng_race = index_to_letter(random_index);
                    return true;
                }
            default:
                // we have a species selection
                if (you.char_class == JOB_UNKNOWN)
                {
                    // we have no restrictions!
                    you.species = get_species(letter_to_index(selection_key));
                    // this is probably used for... something
                    ng_race = selection_key;
                    return true; // pick also background
                }
                else
                {
                    // Can we allow this selection?
                    if (job_allowed(get_species(letter_to_index(selection_key)),
                                    you.char_class) == CC_BANNED)
                    {
                        // we cannot, repoll for key
                        continue;
                    }
                    else
                    {
                        // we have a valid choice!
                        you.species = get_species(letter_to_index(selection_key));
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
void _construct_backgrounds_menu(MenuFreeform* menu)
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

        if (you.species == SP_UNKNOWN
            || job_allowed(you.species, job) == CC_UNRESTRICTED)
        {
            tmp->set_fg_colour(LIGHTGRAY);
            tmp->set_highlight_colour(GREEN);
        }
        else
        {
            tmp->set_fg_colour(DARKGRAY);
            tmp->set_highlight_colour(YELLOW);
        }
        if (you.species != SP_UNKNOWN
            && job_allowed(you.species, job) == CC_BANNED)
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
    if (you.species != SP_UNKNOWN)
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
    if (you.species != SP_UNKNOWN)
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
bool _choose_job(void)
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

    if (you.species != SP_UNKNOWN && you.char_class != JOB_UNKNOWN)
        return false;

    if (Options.cls)
    {
        you.char_class = get_job(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    clrscr();

    // TODO: attach these to the menu in a NoSelectTextItem
    textcolor( BROWN );
    if (you.your_name.empty() && you.species == SP_UNKNOWN)
    {
        cprintf("Welcome. ");
    }
    else if (you.your_name.empty() && you.species != SP_UNKNOWN)
    {
        cprintf("Welcome, the %s. ", species_name(you.species, 1).c_str());
    }
    else if (you.species != SP_UNKNOWN)
    {
        cprintf("Welcome, %s the %s. ", you.your_name.c_str(),
                species_name(you.species, 1).c_str());
    }
    else
    {
        cprintf("Welcome, %s. ", you.your_name.c_str());
    }

    textcolor( YELLOW );
    cprintf("Please select your background");

    _construct_backgrounds_menu(freeform);
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
                you.char_class  = JOB_UNKNOWN;
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
                _pick_random_species_and_job(good_random);
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
                        _pick_random_species_and_job(Options.good_random);
                        return false;
                    }
                    _set_startup_options();
                    you.species = SP_UNKNOWN;
                    you.char_class = JOB_UNKNOWN;
                    return true;
                }
                // ignore Tab because we don't have previous start options set
                continue;
            case ' ':
                you.char_class  = JOB_UNKNOWN;
                Options.cls = 0;
                return true;
            case '?':
                 // access to the help files
                list_commands('1');
                return _choose_job();
            case '%':
                list_commands('%');
                return _choose_job();
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
                if (you.species == SP_UNKNOWN)
                {
                    // pick any specie
                    random_index = random2(ng_num_jobs());
                    you.char_class = get_job(random_index);
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
                    while (!is_good_combination(you.species, get_job(random_index),
                                                good_random));
                    you.char_class = get_job(random_index);
                    ng_cls = index_to_letter(random_index);
                    return true;
                }
            default:
                // we have a background selection
                if (you.species == SP_UNKNOWN)
                {
                    // we have no restrictions!
                    you.char_class = get_job(letter_to_index(selection_key));
                    // this is probably used for... something
                    ng_cls = selection_key;
                    return true; // pick also specie
                }
                else
                {
                    // Can we allow this selection?
                    if (job_allowed(you.species,
                        get_job(letter_to_index(selection_key)))
                        == CC_BANNED)
                    {
                        // we cannot, repoll for key
                        continue;
                    }
                    else
                    {
                        // we have a valid choice!
                        you.char_class = get_job(letter_to_index(selection_key));
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

static int _claws_level(species_type sp);

static bool _do_choose_weapon()
{
    weapon_type startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
                                WPN_HAND_AXE, WPN_SPEAR, WPN_UNKNOWN };

    ng.init(you);

    // Gladiators that are at least medium sized get to choose a trident
    // rather than a spear.
    if (ng.job == JOB_GLADIATOR
        && species_size(ng.species, PSIZE_BODY) >= SIZE_MEDIUM)
    {
        startwep[3] = WPN_TRIDENT;
    }

    const bool claws_allowed =
        (ng.job != JOB_GLADIATOR && _claws_level(ng.species));

    if (claws_allowed)
    {
        for (int i = 3; i >= 0; --i)
            startwep[i + 1] = startwep[i];

        startwep[0] = WPN_UNARMED;
    }
    else
    {
        switch (ng.species)
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
        ng_weapon = Options.weapon;
        ng.weapon = static_cast<weapon_type>(Options.weapon);
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
                ng.weapon = static_cast<weapon_type>(Options.weapon);
                return (true);
            }
        }
    }

    int keyin = 0;
    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        _print_character_info_ng();

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
                return _do_choose_weapon();
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

    ng.weapon = startwep[keyin - 'a'];

    return (true);
}

// Returns false if aborted, else an actual weapon choice
// is written to ng.weapon for the jobs that call
// _update_weapon() later.
static bool _choose_weapon()
{
    ng.init(you);
    switch (ng.job)
    {
    case JOB_FIGHTER:
    case JOB_GLADIATOR:
    case JOB_CHAOS_KNIGHT:
    case JOB_DEATH_KNIGHT:
    case JOB_CRUSADER:
    case JOB_REAVER:
    case JOB_WARPER:
        return (_do_choose_weapon());
    default:
        return (true);
    }
}

// firstbook/numbooks required to prompt with the actual book
// names.
// Returns false if aborted, else an actual book choice
// is written to ng.book.
static bool _choose_book(int firstbook, int numbooks)
{
    clrscr();

    ng.init(you); // XXX

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
                                   _book_to_start(firstbook + i), ng);
    }

    if (Options.book)
    {
        const int opt_book = _start_to_book(firstbook, Options.book);
        if (opt_book != -1)
        {
            book.sub_type = opt_book;
            ng_book = Options.book;
            ng.book = static_cast<startup_book_type>(Options.book);
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
        _print_character_info_ng();

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
                                + _start_to_book(firstbook, Options.prev_book)
                                - firstbook;
                    }
                }
                break;
            case '%':
                list_commands('%');
                return _choose_book(firstbook, numbooks);
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

    ng.book = _book_to_start(firstbook + keyin - 'a');
    return (true);
}

static bool _choose_book()
{
    ng.init(you);
    switch (ng.job)
    {
    case JOB_REAVER:
    case JOB_CONJURER:
        return (_choose_book(BOOK_CONJURATIONS_I, 2));
    case JOB_WIZARD:
        return (_choose_book(BOOK_MINOR_MAGIC_I, 3));
    default:
        return (true);
    }
}

static bool _choose_priest()
{
    if (ng.species == SP_DEMONSPAWN || ng.species == SP_MUMMY
        || ng.species == SP_GHOUL || ng.species == SP_VAMPIRE)
    {
        ng.religion = GOD_YREDELEMNUL;
    }
    else
    {
        const god_type gods[3] = { GOD_ZIN, GOD_YREDELEMNUL, GOD_BEOGH };

        // Disallow invalid choices.
        if (religion_restriction(Options.priest, ng) == CC_BANNED)
            Options.priest = GOD_NO_GOD;

        if (Options.priest != GOD_NO_GOD && Options.priest != GOD_RANDOM)
            ng_pr = ng.religion = static_cast<god_type>(Options.priest);
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
                        ng.religion = gods[i];
                        did_chose = true;
                    }
                }
            }

            if (!did_chose)
            {
                ng.religion = (coinflip() ? GOD_YREDELEMNUL : GOD_ZIN);

                // For orcs 50% chance of Beogh instead.
                if (ng.species == SP_HILL_ORC && coinflip())
                    ng.religion = GOD_BEOGH;
            }
            ng_pr = GOD_RANDOM;
        }
        else
        {
            _print_character_info_ng();

            textcolor(CYAN);
            cprintf("\nWhich god do you wish to serve?\n");

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
                cprintf("%c - %s\n", letter, god_name[i]);
            }

            textcolor(BROWN);
            cprintf("\n* - Random choice; + - Good random choice\n"
                        "Bksp - Back to species and background selection; "
                        "X - Quit\n");

            if (religion_restriction(Options.prev_pr, ng) == CC_BANNED)
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
                           && you.species != SP_HILL_ORC)
                    {
                        break;
                    }
                    if (Options.prev_pr != GOD_RANDOM)
                    {
                        Options.prev_pr
                                 = static_cast<god_type>(Options.prev_pr);
                        ng.religion = Options.prev_pr;
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
                                ng.religion = gods[i];
                            }
                        }
                        if (count > 0)
                            break;
                    }
                    // intentional fall-through
                case '*':
                    ng.religion = coinflip() ? GOD_ZIN : GOD_YREDELEMNUL;
                    if (ng.species == SP_HILL_ORC && coinflip())
                        ng.religion = GOD_BEOGH;
                    break;
                case 'a':
                    ng.religion = GOD_ZIN;
                    break;
                case 'b':
                    ng.religion = GOD_YREDELEMNUL;
                    break;
                case 'c':
                    if (ng.species == SP_HILL_ORC)
                    {
                        ng.religion = GOD_BEOGH;
                        break;
                    } // else fall through
                default:
                    break;
                }
            }
            while (ng.religion == GOD_NO_GOD);

            ng_pr = (keyn == '*' ? GOD_RANDOM : ng.religion);
        }
    }
    return (true);
}

static bool _choose_chaos_knight()
{
    const god_type gods[3] = { GOD_XOM, GOD_MAKHLEB, GOD_LUGONU };

    if (Options.chaos_knight != GOD_NO_GOD
        && Options.chaos_knight != GOD_RANDOM)
    {
        ng_ck = ng.religion =
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
                if (religion_restriction(gods[i], ng) == CC_BANNED)
                    continue;

                if (religion_restriction(gods[i], ng) == CC_UNRESTRICTED
                    && one_chance_in(++count))
                {
                    ng.religion = gods[i];
                    did_chose = true;
                }
            }
        }

        if (!did_chose)
        {
            ng.religion = (one_chance_in(3) ? GOD_XOM :
                           coinflip()       ? GOD_MAKHLEB
                                            : GOD_LUGONU);
        }
        ng_ck = GOD_RANDOM;
    }
    else
    {
        _print_character_info_ng();

        textcolor(CYAN);
        cprintf("\nWhich god of chaos do you wish to serve?\n");

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
                    ng.religion = static_cast<god_type>(Options.prev_ck);
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
                            ng.religion = gods[i];
                        }
                    }
                    if (count > 0)
                        break;
                }
                // intentional fall-through
            case '*':
                ng.religion = (one_chance_in(3) ? GOD_XOM :
                               coinflip()       ? GOD_MAKHLEB
                                                : GOD_LUGONU);
                break;
            case 'a':
                ng.religion = GOD_XOM;
                break;
            case 'b':
                ng.religion = GOD_MAKHLEB;
                break;
            case 'c':
                ng.religion = GOD_LUGONU;
                break;
            default:
                break;
            }
        }
        while (ng.religion == GOD_NO_GOD);

        ng_ck = (keyn == '*') ? GOD_RANDOM
                              : ng.religion;
    }
    return (true);
}

// For jobs with god choice, fill in ng.religion.
static bool _choose_god()
{
    switch (ng.job)
    {
    case JOB_CHAOS_KNIGHT:
        return (_choose_chaos_knight());
    case JOB_PRIEST:
        return (_choose_priest());
    default:
        return (true);
    }
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
    if (ng.job != JOB_ARTIFICER)
        return (true);

    // Wand-choosing interface for Artificers -- Greenberg/Bane

    const wand_type startwand[5] = { WAND_ENSLAVEMENT, WAND_CONFUSION,
                                     WAND_MAGIC_DARTS, WAND_FROST, WAND_FLAME };

    item_def wand;
    _make_rod(wand, STAFF_STRIKING, 8);
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
            ng.wand = static_cast<startup_wand_type>(Options.wand);
            return (true);
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

    ng.wand = static_cast<startup_wand_type>(keyin - 'a' + 1);
    return (true);
}

static void _give_wand()
{
    bool is_rod;
    int wand = _start_to_wand(ng.wand, is_rod);
    ASSERT(wand != -1);

    if (is_rod)
        _make_rod(you.inv[2], STAFF_STRIKING, 8);
    else
    {
        // 1 wand of random effects and one chosen lesser wand
        const wand_type choice = static_cast<wand_type>(wand);
        const int ncharges = 15;
        _newgame_make_item(2, EQ_NONE, OBJ_WANDS, WAND_RANDOM_EFFECTS,
                           -1, 1, ncharges, 0);
        _newgame_make_item(3, EQ_NONE, OBJ_WANDS, choice,
                           -1, 1, ncharges, 0);
    }
}

static void _update_weapon()
{
    ASSERT(ng.weapon != NUM_WEAPONS);

    if (ng.weapon == WPN_UNARMED)
        _newgame_clear_item(0);
    else
        you.inv[0].sub_type = ng.weapon;
}

bool _give_items_skills()
{
    int weap_skill = 0;

    ng.init(you); // XXX

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon();

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        // Skills.
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_SHIELDS]  = 2;

        weap_skill = 2;

        you.skills[(player_effectively_in_light_armour()
                    ? SK_DODGING : SK_ARMOUR)] = 3;

        break;

    case JOB_GLADIATOR:
    {
        // Equipment.
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon();

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        int curr = 3;
        if (you_can_wear(EQ_HELMET))
        {
            _newgame_make_item(3, EQ_HELMET, OBJ_ARMOUR, ARM_HELMET);
            curr++;
        }

        // Small species get stones, the others nets.
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
            // Species skilled with maces/flails get one, the others axes.
            weapon_type startwep = WPN_HAND_AXE;
            if (species_skills(SK_MACES_FLAILS, you.species) <
                species_skills(SK_AXES, you.species))
            {
                startwep = (player_genus(GENPC_OGREISH)) ? WPN_ANKUS : WPN_MACE;
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
        you.skills[(player_effectively_in_light_armour()
                    ? SK_DODGING : SK_ARMOUR)] = 2;
        you.skills[SK_FIGHTING]    = 2;
        you.skills[SK_SHIELDS]     = 2;
        you.skills[SK_LONG_BLADES] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_PRIEST:
        you.religion = ng.religion;
        you.piety = 45;

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
                           2, 2);
        _update_weapon();

        you.religion = ng.religion;

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
        _update_weapon();

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                              ARM_ROBE);

        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_NECROMANCY]   = 2;

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
        _update_weapon();

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
        _update_weapon();

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           _start_to_book(BOOK_CONJURATIONS_I, ng.book));

        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_CONJURATIONS] = 2;
        weap_skill = 3;
        break;

    case JOB_WARPER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon();

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
        you.skills[SK_THROWING]       = 1;
        weap_skill = 3;
    break;

    case JOB_ARCANE_MARKSMAN:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        switch (you.species)
        {
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

        // And give them a book
        _newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_BRANDS);

        you.skills[range_skill(you.inv[1])] = 2;
        you.skills[SK_DODGING]              = 1;
        you.skills[SK_SPELLCASTING]         = 2;
        you.skills[SK_ENCHANTMENTS]         = 2;
        break;

    case JOB_WIZARD:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(1, EQ_HELMET, OBJ_ARMOUR, ARM_WIZARD_HAT);

        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           _start_to_book(BOOK_MINOR_MAGIC_I, ng.book));

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

        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           _start_to_book(BOOK_CONJURATIONS_I, ng.book));

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

        // Gets some darts - this job is difficult to start off with.
        _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 16, 1);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)
        if (you.species == SP_SPRIGGAN)
            you.inv[0].sub_type = WPN_DAGGER;

        if (player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 1;
        you.skills[SK_THROWING]     = 1;
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

        if (player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 1;
        you.skills[SK_FIGHTING]     = 1;
        you.skills[SK_POISON_MAGIC] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        you.skills[SK_STABBING]     = 2;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_ENCHANTMENTS] = 1;
        break;

    case JOB_THIEF:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);
        _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 20);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)
        if (you.species == SP_SPRIGGAN)
            you.inv[0].sub_type = WPN_DAGGER;

        if (player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 2;
        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 2;
        you.skills[SK_STABBING] = 2;
        you.skills[SK_TRAPS_DOORS] = 2;
        you.skills[SK_THROWING] = 2;
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
        you.skills[SK_THROWING]     = 2;
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
            you.inv[0].sub_type = WPN_HAND_AXE;
            break;
        case SP_OGRE:
            you.inv[0].sub_type = WPN_CLUB;
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
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, -1, 6, 1);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               2);
            break;

        case SP_OGRE:
            // Give ogres a knife for butchering, as they now start with
            // a club instead of an axe.
            _newgame_make_item(4, EQ_NONE, OBJ_WEAPONS, WPN_KNIFE);
        case SP_TROLL:
            _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_LARGE_ROCK, -1, 5,
                               1);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               3);
            break;

        case SP_HALFLING:
        case SP_KOBOLD:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1,
                               30, 1);

            // Give them a buckler, as well; sling users + bucklers is an ideal
            // combination.
            _newgame_make_item(4, EQ_SHIELD, OBJ_ARMOUR, ARM_BUCKLER);
            you.skills[SK_SHIELDS] = 2;

            // Wield the sling instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_CROSSBOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 25, 1);

            // Wield the crossbow instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        default:
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 25, 1);

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
        _give_wand();

        // If an offensive wand or the rod of striking was chosen,
        // don't hand out a weapon.
        if (item_is_rod(you.inv[2]))
        {
            // If the rod of striking was chosen, put it in the first
            // slot and wield it.
            you.inv[0] = you.inv[2];
            you.equip[EQ_WEAPON] = 0;
            _newgame_clear_item(2);
        }
        else if (you.inv[3].base_type != OBJ_WANDS
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

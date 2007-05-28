/*
 *  File:       newgame.cc
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *     <16>      19-Jun-2000    GDL   changed handle to FILE *
 *     <15>      06-Mar-2000    bwr   changes to berserer, paladin, enchanter
 *     <14>      10-Jan-2000    DLB   class_allowed() lists excluded
 *                                       species for all but hunters
 *                                    some clean-up of init_player()
 *     <13>      1/10/2000      BCR   Made ogre berserkers get club
 *                                    skill, Trolls get unarmed skill
 *                                    Halflings can be assasins and
 *                                    warpers
 *     <12>      12/4/99        jmf   Gave Paladins more armour skill + a
 *                                    long sword (to compensate for
 *                                    their inability to use poison).
 *                                    Allowed Spriggan Stalkers (since
 *                                    that's basically just a venom mage
 *                                    + assassin, both of which are now
 *                                    legal).
 *     <11>      11/22/99       LRH   Er, re-un-capitalised class
 *                                    names (makes them distinguish-
 *                                    able in score list)
 *     <10>      10/31/99       CDL   Allow Spriggan Assassins
 *                                    Remove some old comments
 *      <9>      10/12/99       BCR   Made sure all the classes are
 *                                    capitalized correctly.
 *      <8>      9/09/99        BWR   Changed character selection
 *                                    screens look (added sub-species
 *                                    menus from Dustin Ragan)
 *      <7>      7/13/99        BWR   Changed assassins to use
 *                                    hand crossbows, changed
 *                                    rangers into hunters.
 *      <6>      6/22/99        BWR   Added new rangers/slingers
 *      <5>      6/17/99        BCR   Removed some Linux/Mac filename
 *                                    weirdness
 *      <4>      6/13/99        BWR   SysEnv support
 *      <3>      6/11/99        DML   Removed tmpfile purging.
 *      <2>      5/20/99        BWR   CRAWL_NAME, new berserk, upped
 *                                    troll food consumption, added
 *                                    demonspawn transmuters.
 *      <1>      -/--/--        LRH   Created
 */

#include "AppHdr.h"
#include "newgame.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef DOS
#include <conio.h>
#include <dos.h>
#endif

#ifdef UNIX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "branch.h"
#include "command.h"
#include "files.h"
#include "fight.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "menu.h"
#include "player.h"
#include "randart.h"
#include "skills.h"
#include "skills2.h"
#include "spl-util.h"
#include "stuff.h"
#include "tutorial.h"
#include "version.h"
#include "view.h"

extern std::string init_file_location;

#define MIN_START_STAT       1

static bool class_allowed(unsigned char speci, int char_class);
static bool validate_player_name(bool verbose);
static void choose_weapon(void);
static void enter_player_name(bool blankOK);
static void give_basic_knowledge(int which_job);
static void give_basic_spells(int which_job);
static void give_basic_mutations(unsigned char speci);
static void give_last_paycheck(int which_job);
static void init_player(void);
static void jobs_stat_init(int which_job);
static void openingScreen(void);
static void species_stat_init(unsigned char which_species);

#ifdef USE_SPELLCASTER_AND_RANGER_WANDERER_TEMPLATES
static void give_random_wand( int slot );
static void give_random_scroll( int slot );
#endif

static void give_random_potion( int slot );
static void give_random_secondary_armour( int slot );
static bool give_wanderer_weapon( int slot, int wpn_skill );
static void create_wanderer(void);
static void give_items_skills(void);
static char letter_to_species(int keyn);
static char letter_to_class(int keyn);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

static char ng_race, ng_cls;
static bool ng_random;
static int ng_ck, ng_dk, ng_pr;
static int ng_weapon;
static int ng_book;

static void reset_newgame_options(void)
{
    ng_race = ng_cls = 0;
    ng_random = false;
    ng_ck = GOD_NO_GOD;
    ng_dk = DK_NO_SELECTION;
    ng_pr = GOD_NO_GOD;
    ng_weapon = WPN_UNKNOWN;
    ng_book = SBT_NO_SELECTION;
}

static void save_newgame_options(void)
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

    write_newgame_options_file();
}

static void set_startup_options(void)
{
    Options.race    = Options.prev_race;
    Options.cls     = Options.prev_cls;
    Options.weapon  = Options.prev_weapon;
    Options.death_knight = Options.prev_dk;
    Options.chaos_knight = Options.prev_ck;
    Options.priest       = Options.prev_pr;
    Options.book = Options.prev_book;
}

static bool prev_startup_options_set(void)
{
    // Are these two enough? They should be, in theory, since we'll
    // remember the player's weapon and god choices.
    return Options.prev_race && Options.prev_cls;
}

static std::string get_opt_race_name(char race)
{
    int prace = letter_to_species(race);
    return prace? species_name(prace, 1) : "Random";
}

static std::string get_opt_class_name(char oclass)
{
    int pcls  = letter_to_class(oclass);
    return pcls != JOB_UNKNOWN? get_class_name(pcls) : "Random";
}

static std::string prev_startup_description(void)
{
    if (Options.prev_race == '?' && Options.prev_cls == '?')
        Options.prev_randpick = true;

    if (Options.prev_randpick)
        return "Random character";

    if (Options.prev_cls == '?')
        return "Random " + get_opt_race_name(Options.prev_race);
    return get_opt_race_name(Options.prev_race) + " " +
           get_opt_class_name(Options.prev_cls);
}

int give_first_conjuration_book()
{
    // Assume the fire/earth book, as conjurations is strong with fire -- bwr
    int book = BOOK_CONJURATIONS_I;

    // Conjuration books are largely Fire or Ice, so we'll use
    // that as the primary condition, and air/earth to break ties. -- bwr
    if (you.skills[SK_ICE_MAGIC] > you.skills[SK_FIRE_MAGIC]
        || (you.skills[SK_FIRE_MAGIC] == you.skills[SK_ICE_MAGIC]
            && you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC]))
    {
        book = BOOK_CONJURATIONS_II;
    }
    else if (you.skills[SK_FIRE_MAGIC] == 0 && you.skills[SK_EARTH_MAGIC] == 0)
    {
        // If we're here its because we were going to default to the
        // fire/earth book... but we don't have those skills.  So we
        // choose randomly based on the species weighting, again
        // ignoring air/earth which are secondary in these books.  -- bwr
        if (random2( species_skills( SK_ICE_MAGIC, you.species ) )
                < random2( species_skills( SK_FIRE_MAGIC, you.species ) )) 
        {
            book = BOOK_CONJURATIONS_II;
        }
    }

    return (book);
}

static void pick_random_species_and_class( void )
{
    //
    // We pick both species and class at the same time to give each
    // valid possibility a fair chance.  For proof that this will
    // work correctly see the proof in religion.cc:handle_god_time().
    //
    int job_count = 0;

    int species = -1;
    int job = -1;

    // for each valid (species, class) choose one randomly
    for (int sp = SP_HUMAN; sp < NUM_SPECIES; sp++)
    {
        // we only want draconians counted once in this loop...
        // we'll add the variety lower down -- bwr
        if (sp >= SP_WHITE_DRACONIAN && sp <= SP_BASE_DRACONIAN)
            continue;

        for (int cl = JOB_FIGHTER; cl < NUM_JOBS; cl++)
        {
            if (class_allowed(sp, cl))
            {
                job_count++;
                if (one_chance_in( job_count ))
                {
                    species = sp;
                    job = cl;
                }
            }
        }
    }

    // at least one job must exist in the game else we're in big trouble
    ASSERT( species != -1 && job != -1 );

    // return draconian variety here
    if (species == SP_RED_DRACONIAN)
        you.species = SP_RED_DRACONIAN + random2(9);
    else
        you.species = species;

    you.char_class = job;
}

static bool check_saved_game(void)
{
    FILE *handle;

    std::string basename = get_savedir_filename( you.your_name, "", "" );
    std::string savename = basename + ".sav";

#ifdef LOAD_UNPACKAGE_CMD
    std::string zipname = basename + PACKAGE_SUFFIX;
    handle = fopen(zipname.c_str(), "rb+");
    if (handle != NULL)
    {
        fclose(handle);
        cprintf(EOL "Loading game..." EOL);

        // Create command
        char cmd_buff[1024];

        const std::string directory = get_savedir();

        snprintf( cmd_buff, sizeof(cmd_buff), LOAD_UNPACKAGE_CMD,
                  basename.c_str(), directory.c_str() );

        if (system( cmd_buff ) != 0)
        {
            cprintf( EOL "Warning: Zip command (LOAD_UNPACKAGE_CMD) "
                         "returned non-zero value!" EOL );
        }

        // Remove save game package
        unlink(zipname.c_str());
    }
#endif

    handle = fopen(savename.c_str(), "rb+");

    if (handle != NULL)
    {
        fclose(handle);
        return true;
    }
    return false;
}

static unsigned char random_potion_description()
{
    int desc, nature, colour;

    do
    {
        desc = random2( PDQ_NQUALS * PDC_NCOLOURS );

        if (coinflip())
            desc %= PDC_NCOLOURS;

        nature = PQUAL(desc);
        colour = PCOLOUR(desc);

        // nature and colour correspond to primary and secondary
        // in itemname.cc; this check ensures clear potions don't
        // get odd qualifiers.
    } while ((colour == PDC_CLEAR && nature > PDQ_VISCOUS) 
            || desc == PDESCS(PDC_CLEAR)
            || desc == PDESCQ(PDQ_GLUGGY, PDC_WHITE));

    return static_cast<unsigned char>(desc);
}

// Determine starting depths of branches
static void initialise_branch_depths()
{
    branches[BRANCH_ECUMENICAL_TEMPLE].startdepth = random_range(4, 7);
    branches[BRANCH_ORCISH_MINES].startdepth = random_range(6, 11);
    branches[BRANCH_ELVEN_HALLS].startdepth = random_range(3, 4);
    branches[BRANCH_LAIR].startdepth = random_range(8, 13);
    branches[BRANCH_HIVE].startdepth = random_range(11, 16);
    branches[BRANCH_SLIME_PITS].startdepth = random_range(3, 9);
    if ( coinflip() )
    {
        branches[BRANCH_SWAMP].startdepth = random_range(2, 7);
        branches[BRANCH_SHOALS].startdepth = -1;
    }
    else
    {
        branches[BRANCH_SWAMP].startdepth = -1;
        branches[BRANCH_SHOALS].startdepth = random_range(2, 7);
    }
    branches[BRANCH_SNAKE_PIT].startdepth = random_range(3, 8);
    branches[BRANCH_VAULTS].startdepth = random_range(14, 19);
    branches[BRANCH_CRYPT].startdepth = random_range(2, 4);
    branches[BRANCH_HALL_OF_BLADES].startdepth = random_range(4, 6);
    branches[BRANCH_TOMB].startdepth = random_range(2, 3);
}

static void initialise_item_descriptions()
{
    // must remember to check for already existing colours/combinations
    you.item_description.init(255);

    you.item_description[IDESC_POTIONS][POT_PORRIDGE] =
        PDESCQ(PDQ_GLUGGY, PDC_WHITE);

    you.item_description[IDESC_POTIONS][POT_WATER] = PDESCS(PDC_CLEAR);

    for (int i = 0; i < NUM_IDESC; i++)
    {
        // We really should only loop until NUM_WANDS, etc., here
        for (int j = 0; j < you.item_description.height(); j++)
        {
            // Don't override predefines
            if (you.item_description[i][j] != 255)
                continue;

            // pick a new description until it's good
            while (true)
            {

                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2( 16 * 12 );
                    if (coinflip())
                        you.item_description[i][j] %= 12;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = random_potion_description();
                    break;

                case IDESC_SCROLLS: // scrolls
                case IDESC_SCROLLS_II:
                    you.item_description[i][j] = random2(151);
                    break;

                case IDESC_RINGS: // rings
                    you.item_description[i][j] = random2( 13 * 13 );
                    if (coinflip())
                        you.item_description[i][j] %= 13;
                    break;
                }

                bool is_ok = true;

                // test whether we've used this description before
                // don't have p < j because some are preassigned
                for (int p = 0; p < you.item_description.height(); p++)
                {
                    if ( p == j )
                        continue;

                    if (you.item_description[i][p]==you.item_description[i][j])
                    {
                        is_ok = false;
                        break;
                    }
                }
                if ( is_ok )
                    break;
            }
        }
    }
}

static void give_starting_food()
{
    // the undead start with no food
    if (you.is_undead != US_ALIVE)
        return;

    item_def item;
    if ( you.species == SP_SPRIGGAN )
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type = POT_PORRIDGE;
    }
    else
    {
        item.base_type = OBJ_FOOD;
        if (you.species == SP_HILL_ORC || you.species == SP_KOBOLD ||
            you.species == SP_OGRE || you.species == SP_TROLL)
            item.sub_type = FOOD_MEAT_RATION;
        else
            item.sub_type = FOOD_BREAD_RATION;
    }
    item.quantity = 1;

    const int slot = find_free_slot(item);
    you.inv[slot] = item;       // will ASSERT if couldn't find free slot
}

static void mark_starting_books()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]) && you.inv[i].base_type == OBJ_BOOKS)
        {
            const int subtype = you.inv[i].sub_type;

            you.had_book[subtype] = true;

            // one for all, all for one
            if (subtype == BOOK_MINOR_MAGIC_I ||
                subtype == BOOK_MINOR_MAGIC_II ||
                subtype == BOOK_MINOR_MAGIC_III)
            {
                you.had_book[BOOK_MINOR_MAGIC_I] = true;
                you.had_book[BOOK_MINOR_MAGIC_II] = true;
                you.had_book[BOOK_MINOR_MAGIC_III] = true;
            }

            if (subtype == BOOK_CONJURATIONS_I ||
                subtype == BOOK_CONJURATIONS_II)
            {
                you.had_book[BOOK_CONJURATIONS_I] = true;
                you.had_book[BOOK_CONJURATIONS_II] = true;
            }
        }
    }
}

static void racialise_starting_equipment()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]))
        {
            // don't change object type modifier unless it starts plain
            if ((you.inv[i].base_type == OBJ_ARMOUR ||
                 you.inv[i].base_type == OBJ_WEAPONS ||
                 you.inv[i].base_type == OBJ_MISSILES)
                && get_equip_race(you.inv[i]) == ISFLAG_NO_RACE )
            {
                // now add appropriate species type mod
                switch (you.species)
                {
                case SP_ELF:
                case SP_HIGH_ELF:
                case SP_GREY_ELF:
                case SP_DEEP_ELF:
                case SP_SLUDGE_ELF:
                    set_equip_race( you.inv[i], ISFLAG_ELVEN );
                    break;

                case SP_HILL_DWARF:
                case SP_MOUNTAIN_DWARF:
                    set_equip_race( you.inv[i], ISFLAG_DWARVEN );
                    break;

                case SP_HILL_ORC:
                    set_equip_race( you.inv[i], ISFLAG_ORCISH );
                    break;
                }
            }
        }
    }
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
static void reassess_starting_skills()
{
    for (int i = 0; i < NUM_SKILLS; i++)
    {
        if (!you.skills[i])
            continue;

        // Grant the amount of skill points required for a human
        const int points = skill_exp_needed( you.skills[i] + 1 );
        you.skill_points[i] = (points * species_skills(i, SP_HUMAN))/100 + 1;

        // Find out what level that earns this character.
        const int sp_diff = species_skills( i, you.species );
        you.skills[i] = 0;

        for (int lvl = 1; lvl <= 8; lvl++) 
        {
            if (you.skill_points[i] > (skill_exp_needed(lvl+1) * sp_diff)/100)
                you.skills[i] = lvl;
            else
                break;
        }
    }
}

// randomly boost stats a number of times
static void assign_remaining_stats( int points_left )
{
    // first spend points to get us to the minimum allowed value -- bwr
    if (you.strength < MIN_START_STAT)
    {
        points_left -= (MIN_START_STAT - you.strength);
        you.strength = MIN_START_STAT;
    }

    if (you.intel < MIN_START_STAT)
    {
        points_left -= (MIN_START_STAT - you.intel);
        you.intel = MIN_START_STAT;
    }

    if (you.dex < MIN_START_STAT)
    {
        points_left -= (MIN_START_STAT - you.dex);
        you.dex = MIN_START_STAT;
    }

    // now randomly assign the remaining points --bwr
    while (points_left > 0)
    {
        // stats that are already high will be chosen half as often
        switch (random2( NUM_STATS ))
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

static void give_species_bonus_hp()
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
        case SP_OGRE_MAGE:
        case SP_DEMIGOD:
            inc_max_hp(2);
            break;

        case SP_HILL_ORC:
        case SP_MUMMY:
        case SP_MERFOLK:
            inc_max_hp(1);
            break;

        case SP_ELF:
        case SP_GREY_ELF:
        case SP_HIGH_ELF:
            dec_max_hp(1);
            break;

        case SP_DEEP_ELF:
        case SP_GNOME:
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

static void give_species_bonus_mp()
{
    // adjust max_magic_points by species {dlb}
    switch (you.species)
    {
    case SP_SPRIGGAN:
    case SP_DEMIGOD:
    case SP_GREY_ELF:
    case SP_DEEP_ELF:
        inc_max_mp(1);
        break;

    default:
        break;
    }
}

bool new_game(void)
{
    //jmf: NEW ASSERTS: we ought to do a *lot* of these
    ASSERT(NUM_SPELLS < SPELL_NO_SPELL);
    ASSERT(NUM_JOBS < JOB_UNKNOWN);
    ASSERT(NUM_ATTRIBUTES >= 30);

    init_player();

    if (!Options.player_name.empty())
    {
        strncpy(you.your_name, Options.player_name.c_str(), kNameLen);
        you.your_name[kNameLen - 1] = 0;
    }

    textcolor(LIGHTGREY);

    // copy name into you.your_name if set from environment --
    // note that you.your_name could already be set from init.txt
    // this, clearly, will overwrite such information {dlb}
    if (!SysEnv.crawl_name.empty())
    {
        strncpy( you.your_name, SysEnv.crawl_name.c_str(), kNameLen );
        you.your_name[ kNameLen - 1 ] = 0;
    }

    openingScreen();
    enter_player_name(true);

    if (you.your_name[0] != 0)
    {
        if (check_saved_game())
        {
            textcolor( BROWN );
            cprintf( EOL "Welcome back, " );
            textcolor( YELLOW );
            cprintf( "%s!", you.your_name );
            textcolor( LIGHTGREY );

            save_player_name();
            return (false);
        }
    }

    reset_newgame_options();
    if (Options.random_pick)
    {
        pick_random_species_and_class();
        ng_random = true;
    }
    else
    {
        while (choose_race() && !choose_class());
    }

    strcpy( you.class_name, get_class_name( you.char_class ) );

    // new: pick name _after_ race and class choices
    if (you.your_name[0] == 0)
    {
        clrscr();

        char spec_buff[80];
        strncpy(spec_buff,
                species_name(you.species, you.experience_level), 80);

        cprintf( "You are a%s %s %s." EOL, 
                 (is_vowel( spec_buff[0] )) ? "n" : "", spec_buff,
                 you.class_name );

        enter_player_name(false);

        if (check_saved_game())
        {
            cprintf(EOL "Do you really want to overwrite your old game?");
            char c = getch();
            if (!(c == 'Y' || c == 'y'))
            {
                textcolor( BROWN );
                cprintf(EOL EOL "Welcome back, ");
                textcolor( YELLOW );
                cprintf("%s!", you.your_name);
                textcolor( LIGHTGREY );

                return (false);
            }
        }
    }


// ************ round-out character statistics and such ************

    species_stat_init( you.species );     // must be down here {dlb}

    you.is_undead = ((you.species == SP_MUMMY) ? US_UNDEAD :
                     (you.species == SP_GHOUL) ? US_HUNGRY_DEAD : US_ALIVE);

    // before we get into the inventory init, set light radius based
    // on species vision. currently, all species see out to 8 squares.
    you.normal_vision = 8;
    you.current_vision = 8;

    jobs_stat_init( you.char_class );
    give_last_paycheck( you.char_class );

    assign_remaining_stats((you.species == SP_DEMIGOD ||
                            you.species == SP_DEMONSPAWN) ? 15 : 8);

    // this function depends on stats being finalized
    give_items_skills();

    give_species_bonus_hp();
    give_species_bonus_mp();

    // these need to be set above using functions!!! {dlb}
    you.max_dex = you.dex;
    you.max_strength = you.strength;
    you.max_intel = you.intel;

    give_starting_food();
    mark_starting_books();
    racialise_starting_equipment();

    initialise_item_descriptions();

    reassess_starting_skills();
    calc_total_skill_points();

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]))
        {
            // identify all items in pack
            set_ident_type( you.inv[i].base_type, 
                            you.inv[i].sub_type, ID_KNOWN_TYPE );
            // link properly
            you.inv[i].x = -1;
            you.inv[i].y = -1;
            you.inv[i].link = i;
            you.inv[i].slot = index_to_letter(you.inv[i].link);
            item_colour( you.inv[i] );  // set correct special and colour
        }
    }

    // Brand items as original equipment.
    origin_set_inventory(origin_set_startequip);

    // we calculate hp and mp here; all relevant factors should be
    // finalized by now (GDL)
    calc_hp();
    calc_mp();

    // make sure the starting player is fully charged up
    set_hp( you.hp_max, false );
    set_mp( you.max_magic_points, false ); 

    give_basic_spells(you.char_class);
    give_basic_knowledge(you.char_class);

    // tmpfile purging removed in favour of marking
    tmp_file_pairs.init(false);
    
    give_basic_mutations(you.species);
    initialise_branch_depths();

    save_newgame_options();
    return (true);
}                               // end of new_game()

static bool species_is_undead( unsigned char speci )
{
    return (speci == SP_MUMMY || speci == SP_GHOUL);
}

static bool class_allowed( unsigned char speci, int char_class )
{
    switch (char_class)
    {
    case JOB_FIGHTER:
        switch (speci)
        {
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
            return false;
        }
        return true;

    case JOB_WIZARD:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_HILL_DWARF:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_PRIEST:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_THIEF:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_GLADIATOR:
        if (player_genus(GENPC_ELVEN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_NECROMANCER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_ELF:
        case SP_GHOUL:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_PALADIN:
        switch (speci)
        {
        case SP_HUMAN:
        case SP_MOUNTAIN_DWARF:
        case SP_HIGH_ELF:
            return true;
        }
        return false;

    case JOB_ASSASSIN:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GHOUL:
        case SP_GNOME:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_BERSERKER:
        if (player_genus(GENPC_ELVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MOUNTAIN_DWARF:
        case SP_NAGA:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_HUNTER:
        if (player_genus(GENPC_DRACONIAN, speci))   // use bows
            return true;
        if (player_genus(GENPC_DWARVEN, speci))     // use xbows
            return true;

        switch (speci)
        {
            // bows --
        case SP_CENTAUR:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_ELF:
        case SP_GREY_ELF:
        case SP_HIGH_ELF:
        case SP_HUMAN:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_SLUDGE_ELF:
            // xbows --
        case SP_HILL_ORC:
            // slings --
        case SP_GNOME:
        case SP_HALFLING:
            // spear
        case SP_MERFOLK:
            return true;
        }
        return false;

    case JOB_CONJURER:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
        case SP_SLUDGE_ELF:
            return false;
        }
        return true;

    case JOB_ENCHANTER:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_SLUDGE_ELF:
            return false;
        }
        return true;

    case JOB_FIRE_ELEMENTALIST:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_ICE_ELEMENTALIST:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_SUMMONER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_AIR_ELEMENTALIST:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_EARTH_ELEMENTALIST:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_ELF:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_CRUSADER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        //case SP_HALFLING: //jmf: they're such good enchanters...
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
        case SP_SLUDGE_ELF:
            return false;
        }
        return true;

    case JOB_DEATH_KNIGHT:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;

        switch (speci)
        {
        case SP_ELF:
        case SP_GHOUL:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        // case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_VENOM_MAGE:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_ELF:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_CHAOS_KNIGHT:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_TRANSMUTER:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_HALFLING:
        case SP_HILL_DWARF:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_HEALER:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_REAVER:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HILL_DWARF:
        case SP_MINOTAUR:
        case SP_MOUNTAIN_DWARF:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
        case SP_SLUDGE_ELF:
            return false;
        }
        return true;

    case JOB_STALKER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_MONK:
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HILL_DWARF:
        case SP_KOBOLD:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        }
        return true;

    case JOB_WARPER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HILL_ORC:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        }
        return true;

    case JOB_WANDERER:
        switch (speci)
        {
        case SP_HUMAN:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_GHOUL:
            return true;
        }
        return false;

    case JOB_QUITTER:   // shouldn't happen since 'x' is handled specially
    default:
        return false;
    }
}                               // end class_allowed()


static void choose_book( item_def& book, int firstbook, int numbooks )
{
    int keyin = 0;
    clrscr();
    book.base_type = OBJ_BOOKS;
    book.quantity = 1;
    book.plus = 0;
    book.special = 1;

    // using the fact that CONJ_I and MINOR_MAGIC_I are both
    // fire books, CONJ_II and MINOR_MAGIC_II are both ice books
    if ( Options.book && Options.book <= numbooks )
    {
        book.sub_type = firstbook + Options.book - 1;
        ng_book = Options.book;
        return;
    }

    if ( Options.prev_book > numbooks && Options.prev_book != SBT_RANDOM )
        Options.prev_book = SBT_NO_SELECTION;

    if ( !Options.random_pick )
    {
        textcolor( CYAN );
        cprintf(EOL " You have a choice of books:" EOL);
        textcolor( LIGHTGREY );

        for (int i=0; i < numbooks; ++i)
        {
            book.sub_type = firstbook + i;
            cprintf("%c - %s" EOL, 'a' + i, book.name(DESC_PLAIN).c_str());
        }

        textcolor(BROWN);
        cprintf(EOL "* - Random" );
        if ( Options.prev_book != SBT_NO_SELECTION )
        {
            cprintf("; Enter - %s",
                    Options.prev_book == SBT_FIRE   ? "Fire"      :
                    Options.prev_book == SBT_COLD   ? "Cold"      :
                    Options.prev_book == SBT_SUMM   ? "Summoning" :
                    Options.prev_book == SBT_RANDOM ? "Random"    :
                    "Buggy Book");
        }
        cprintf(EOL);
            
        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which book? ");
            textcolor( LIGHTGREY );

            keyin = get_ch();
        } while (keyin != '*' && 
                 ((keyin != '\r' && keyin != '\n') ||
                  Options.prev_book == SBT_NO_SELECTION ) &&
                 (keyin < 'a' || keyin >= ('a' + numbooks)));

        if ( keyin == '\r' || keyin == '\n' )
        {
            if ( Options.prev_book == SBT_RANDOM )
                keyin = '*';
            else
                keyin = ('a' +  Options.prev_book - 1);
        }
    }

    if (Options.random_pick || Options.book == SBT_RANDOM || keyin == '*')
        ng_book = SBT_RANDOM;
    else
        ng_book = keyin - 'a' + 1;
    
    if ( Options.random_pick || keyin == '*' )
        keyin = random2(numbooks) + 'a';

    book.sub_type = firstbook + keyin - 'a';
}
    

static const weapon_type startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
    WPN_HAND_AXE, WPN_SPEAR, WPN_TRIDENT };

static void choose_weapon( void )
{
    unsigned char keyin = 0;
    int num_choices = 4;
    int temp_rand;              // probability determination {dlb}

    if (you.char_class == JOB_CHAOS_KNIGHT)
    {
        temp_rand = random2(4);

        you.inv[0].sub_type = ((temp_rand == 0) ? WPN_SHORT_SWORD :
                           (temp_rand == 1) ? WPN_MACE :
                           (temp_rand == 2) ? WPN_HAND_AXE : WPN_SPEAR);
        return;
    }

    if (you.char_class == JOB_GLADIATOR || you.species == SP_MERFOLK)
        num_choices = 5;

    if (Options.weapon != WPN_UNKNOWN && Options.weapon != WPN_RANDOM
        && (Options.weapon != WPN_TRIDENT || num_choices == 5))
    {
        you.inv[0].sub_type = Options.weapon;
        ng_weapon = Options.weapon;
        return;
    }

    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        clrscr();

        textcolor( CYAN );
        cprintf(EOL " You have a choice of weapons:" EOL);
        textcolor( LIGHTGREY );

        bool prevmatch = false;
        for(int i=0; i<num_choices; i++)
        {
            int x = effective_stat_bonus(startwep[i]);
            cprintf("%c - %s%s" EOL, 'a' + i,
                    weapon_base_name(startwep[i]),
                    (x <= -4) ? " (not ideal)" : "" );

            if (Options.prev_weapon == startwep[i])
                prevmatch = true;
        }
        if (!prevmatch && Options.prev_weapon != WPN_RANDOM)
            Options.prev_weapon = WPN_UNKNOWN;

        textcolor(BROWN);
        cprintf(EOL "* - Random" );
        if (Options.prev_weapon != WPN_UNKNOWN)
        {
            cprintf("; Enter - %s",
                    Options.prev_weapon == WPN_RANDOM ? "Random" :
                    weapon_base_name(Options.prev_weapon));
        }
        cprintf(EOL);
            
        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which weapon? ");
            textcolor( LIGHTGREY );

            keyin = get_ch();
        }
        while (keyin != '*' && 
                ((keyin != '\r' && keyin != '\n') 
                    || Options.prev_weapon == WPN_UNKNOWN) &&
                (keyin < 'a' || keyin > ('a' + num_choices)));

        if (keyin == '\r' || keyin == '\n')
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

        if (keyin != '*' && effective_stat_bonus(startwep[keyin-'a']) > -4)
        {
            cprintf(EOL "A fine choice. " EOL);
            delay(1000);
        }
    }

    if (Options.random_pick || Options.weapon == WPN_RANDOM || keyin == '*')
    {
        Options.weapon = WPN_RANDOM;
        // try to choose a decent weapon
        for(int times=0; times<50; times++)
        {
            keyin = random2(num_choices);
            int x = effective_stat_bonus(startwep[keyin]);
            if (x > -2)
                break;
        }
        keyin += 'a';
    }

    you.inv[0].sub_type = startwep[keyin-'a'];
    ng_weapon = (Options.random_pick || 
                Options.weapon == WPN_RANDOM || 
                keyin == '*')
        ? WPN_RANDOM : you.inv[0].sub_type;
}

static void init_player(void)
{
    you.init();
}

static void give_last_paycheck(int which_job)
{
    switch (which_job)
    {
    case JOB_HEALER:
    case JOB_THIEF:
        you.gold = roll_dice( 2, 100 );
        break;

    case JOB_WANDERER:
    case JOB_WARPER:
    case JOB_ASSASSIN:
        you.gold = roll_dice( 2, 50 );
        break;

    default:
        you.gold = roll_dice( 2, 20 );
        break;

    case JOB_PALADIN:
    case JOB_MONK:
        you.gold = 0;
        break;
    }
}

// requires stuff::modify_all_stats() and works because
// stats zeroed out by newgame::init_player()... recall
// that demonspawn & demigods get more later on {dlb}
static void species_stat_init(unsigned char which_species)
{
    int sb = 0; // strength base
    int ib = 0; // intelligence base
    int db = 0; // dexterity base

    // Note: The stats in in this list aren't intended to sum the same 
    // for all races.  The fact that Mummies and Ghouls are really low 
    // is considered acceptable (Mummies don't have to eat, and Ghouls
    // are supposted to be a really hard race).  Also note that Demigods
    // and Demonspawn get seven more random points added later. -- bwr
    switch (which_species)
    {
    default:                    sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_HUMAN:              sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_DEMIGOD:            sb =  7; ib =  7; db =  7;      break;  // 21+7
    case SP_DEMONSPAWN:         sb =  4; ib =  4; db =  4;      break;  // 12+7

    case SP_ELF:                sb =  5; ib =  8; db =  8;      break;  // 21
    case SP_HIGH_ELF:           sb =  5; ib =  9; db =  8;      break;  // 22
    case SP_GREY_ELF:           sb =  4; ib =  9; db =  8;      break;  // 21
    case SP_DEEP_ELF:           sb =  3; ib = 10; db =  8;      break;  // 21
    case SP_SLUDGE_ELF:         sb =  6; ib =  7; db =  7;      break;  // 20

    case SP_HILL_DWARF:         sb = 10; ib =  3; db =  4;      break;  // 17
    case SP_MOUNTAIN_DWARF:     sb =  9; ib =  4; db =  5;      break;  // 18

    case SP_TROLL:              sb = 13; ib =  2; db =  3;      break;  // 18
    case SP_OGRE:               sb = 12; ib =  3; db =  3;      break;  // 18
    case SP_OGRE_MAGE:          sb =  9; ib =  7; db =  3;      break;  // 19

    case SP_MINOTAUR:           sb = 10; ib =  3; db =  3;      break;  // 16
    case SP_HILL_ORC:           sb =  9; ib =  3; db =  4;      break;  // 16
    case SP_CENTAUR:            sb =  8; ib =  5; db =  2;      break;  // 15
    case SP_NAGA:               sb =  8; ib =  6; db =  4;      break;  // 18

    case SP_GNOME:              sb =  6; ib =  6; db =  7;      break;  // 19
    case SP_MERFOLK:            sb =  6; ib =  5; db =  7;      break;  // 18
    case SP_KENKU:              sb =  6; ib =  6; db =  7;      break;  // 19

    case SP_KOBOLD:             sb =  5; ib =  4; db =  8;      break;  // 17
    case SP_HALFLING:           sb =  3; ib =  6; db =  9;      break;  // 18
    case SP_SPRIGGAN:           sb =  2; ib =  7; db =  9;      break;  // 18

    case SP_MUMMY:              sb =  7; ib =  3; db =  3;      break;  // 13
    case SP_GHOUL:              sb =  9; ib =  1; db =  2;      break;  // 13

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_GOLDEN_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_UNK0_DRACONIAN:
    case SP_UNK1_DRACONIAN:
    case SP_BASE_DRACONIAN:     sb =  9; ib =  6; db =  2;      break;  // 17
    }

    modify_all_stats( sb, ib, db );
}

static void jobs_stat_init(int which_job)
{
    int s = 0;   // strength mod
    int i = 0;   // intelligence mod
    int d = 0;   // dexterity mod
    int hp = 0;  // HP base
    int mp = 0;  // MP base

    // Note:  Wanderers are correct, they're a challenging class. -- bwr
    switch (which_job)
    {
    case JOB_FIGHTER:           s =  7; i =  0; d =  3; hp = 15; mp = 0; break;
    case JOB_BERSERKER:         s =  7; i = -1; d =  4; hp = 15; mp = 0; break;
    case JOB_GLADIATOR:         s =  6; i =  0; d =  4; hp = 14; mp = 0; break;
    case JOB_PALADIN:           s =  6; i =  2; d =  2; hp = 14; mp = 0; break;

    case JOB_CRUSADER:          s =  4; i =  3; d =  3; hp = 13; mp = 1; break;
    case JOB_DEATH_KNIGHT:      s =  4; i =  3; d =  3; hp = 13; mp = 1; break;
    case JOB_CHAOS_KNIGHT:      s =  4; i =  3; d =  3; hp = 13; mp = 1; break;

    case JOB_REAVER:            s =  4; i =  4; d =  2; hp = 13; mp = 1; break;
    case JOB_HEALER:            s =  4; i =  4; d =  2; hp = 13; mp = 1; break;
    case JOB_PRIEST:            s =  4; i =  4; d =  2; hp = 12; mp = 1; break;

    case JOB_ASSASSIN:          s =  2; i =  2; d =  6; hp = 12; mp = 0; break;
    case JOB_THIEF:             s =  3; i =  2; d =  5; hp = 13; mp = 0; break;
    case JOB_STALKER:           s =  2; i =  3; d =  5; hp = 12; mp = 1; break;

    case JOB_HUNTER:            s =  3; i =  3; d =  4; hp = 13; mp = 0; break;
    case JOB_WARPER:            s =  3; i =  4; d =  3; hp = 12; mp = 1; break;

    case JOB_MONK:              s =  2; i =  2; d =  6; hp = 13; mp = 0; break;
    case JOB_TRANSMUTER:        s =  2; i =  4; d =  4; hp = 12; mp = 1; break;

    case JOB_WIZARD:            s = -1; i =  8; d =  3; hp =  8; mp = 5; break;
    case JOB_CONJURER:          s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_ENCHANTER:         s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_FIRE_ELEMENTALIST: s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_ICE_ELEMENTALIST:  s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_AIR_ELEMENTALIST:  s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_EARTH_ELEMENTALIST:s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_SUMMONER:          s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_VENOM_MAGE:        s =  0; i =  6; d =  4; hp = 10; mp = 3; break;
    case JOB_NECROMANCER:       s =  0; i =  6; d =  4; hp = 10; mp = 3; break;

    case JOB_WANDERER:          s =  2; i =  2; d =  2; hp = 11; mp = 1; break;
    default:                    s =  0; i =  0; d =  0; hp = 10; mp = 0; break;
    }

    modify_all_stats( s, i, d );

    set_hp( hp, true );
    set_mp( mp, true );
}

static void give_basic_mutations(unsigned char speci)
{
    // We should switch over to a size-based system
    // for the fast/slow metabolism when we get around to it.
    switch ( speci )
    {
    case SP_OGRE:
        you.mutation[MUT_FAST_METABOLISM] = 1;
        break;
    case SP_OGRE_MAGE:
        you.mutation[MUT_FAST_METABOLISM] = 1;
        break;
    case SP_HALFLING:
        you.mutation[MUT_SLOW_METABOLISM] = 1;
        break;
    case SP_DEMIGOD:
        you.mutation[MUT_FAST_METABOLISM] = 1;
        break;
    case SP_MINOTAUR:
        you.mutation[MUT_HORNS] = 2;
        break;
    case SP_SPRIGGAN:
        you.mutation[MUT_ACUTE_VISION] = 1;
        you.mutation[MUT_FAST] = 3;
        you.mutation[MUT_HERBIVOROUS] = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 3;
        break;
    case SP_CENTAUR:
        you.mutation[MUT_FAST] = 2;
        you.mutation[MUT_DEFORMED] = 1;
        you.mutation[MUT_FAST_METABOLISM] = 2;
        break;
    case SP_NAGA:
        you.mutation[MUT_ACUTE_VISION] = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_DEFORMED] = 1;
        break;
    case SP_MUMMY:
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_COLD_RESISTANCE] = 1;
        break;
    case SP_GNOME:
        you.mutation[MUT_MAPPING] = 2;
        break;
    case SP_GHOUL:
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_COLD_RESISTANCE] = 1;
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_TROLL:
        you.mutation[MUT_REGENERATION] = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        break;
    case SP_KOBOLD:
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    default:
        break;
    }

    // starting mutations are unremoveable
    for ( int i = 0; i < NUM_MUTATIONS; ++i )
        you.demon_pow[i] = you.mutation[i];
}

static void give_basic_knowledge(int which_job)
{
    switch (which_job)
    {
    case JOB_PRIEST:
    case JOB_PALADIN:
        set_ident_type( OBJ_POTIONS, POT_HEALING, ID_KNOWN_TYPE );
        break;

    case JOB_HEALER:
        set_ident_type( OBJ_POTIONS, POT_HEALING, ID_KNOWN_TYPE );
        set_ident_type( OBJ_POTIONS, POT_HEAL_WOUNDS, ID_KNOWN_TYPE );
        break;

    case JOB_ASSASSIN:
    case JOB_STALKER:
    case JOB_VENOM_MAGE:
        set_ident_type( OBJ_POTIONS, POT_POISON, ID_KNOWN_TYPE );
        break;

    case JOB_WARPER:
        set_ident_type( OBJ_SCROLLS, SCR_BLINKING, ID_KNOWN_TYPE );
        break;

    case JOB_TRANSMUTER:
        set_ident_type( OBJ_POTIONS, POT_WATER, ID_KNOWN_TYPE );
        set_ident_type( OBJ_POTIONS, POT_CONFUSION, ID_KNOWN_TYPE );
        set_ident_type( OBJ_POTIONS, POT_POISON, ID_KNOWN_TYPE );
        break;

    default:
        break;
    }

    return;
}                               // end give_basic_knowledge()

static void give_basic_spells(int which_job)
{
    // wanderers may or may not already have a spell -- bwr
    if (which_job == JOB_WANDERER)
        return;

    spell_type which_spell = SPELL_NO_SPELL;

    switch (which_job)
    {
    case JOB_CONJURER:
    case JOB_REAVER:
    case JOB_WIZARD:
        which_spell = SPELL_MAGIC_DART;
        break;
    case JOB_STALKER:
    case JOB_VENOM_MAGE:
        which_spell = SPELL_STING;
        break;
    case JOB_SUMMONER:
        which_spell = SPELL_SUMMON_SMALL_MAMMAL;
        break;
    case JOB_ICE_ELEMENTALIST:
        which_spell = SPELL_FREEZE;
        break;
    case JOB_NECROMANCER:
        which_spell = SPELL_PAIN;
        break;
    case JOB_ENCHANTER:
        which_spell = SPELL_BACKLIGHT;
        break;
    case JOB_FIRE_ELEMENTALIST:
        which_spell = SPELL_FLAME_TONGUE;
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
}                               // end give_basic_spells()


/* ************************************************************************

// MAKE INTO FUNCTION!!! {dlb}
// randomly boost stats a number of times based on species {dlb}
    unsigned char points_left = ( you.species == SP_DEMIGOD || you.species == SP_DEMONSPAWN ) ? 15 : 8;

    do
    {
        switch ( random2(NUM_STATS) )
        {
          case STAT_STRENGTH:
            if ( you.strength > 17 && coinflip() )
              continue;
            you.strength++;
            break;
          case STAT_DEXTERITY:
            if ( you.dex > 17 && coinflip() )
              continue;
            you.dex++;
            break;
          case STAT_INTELLIGENCE:
            if ( you.intel > 17 && coinflip() )
              continue;
            you.intel++;
            break;
        }
        points_left--;
    }
    while (points_left > 0);

************************************************************************ */


// eventually, this should be something more grand {dlb}
static void openingScreen(void)
{
    textcolor( YELLOW );
    cprintf("Hello, welcome to " CRAWL " " VERSION "!");
    textcolor( BROWN );
    cprintf(EOL "(c) Copyright 1997-2002 Linley Henzell, 2002-2007 Crawl DevTeam");
    cprintf(EOL 
        "Please consult crawl_manual.txt for instructions and legal details."
        EOL);

    bool init_found = init_file_location.find("not found") == std::string::npos;
    if (!init_found)
        textcolor( LIGHTRED );
    else
        textcolor( LIGHTGREY );

    cprintf("Init file %s%s" EOL,
            init_found? "read: " : "",
            init_file_location.c_str());
    textcolor( LIGHTGREY );

    return;
}                               // end openingScreen()

static void show_name_prompt(int where, bool blankOK, 
        const std::vector<player> &existing_chars,
        slider_menu &menu)
{
    gotoxy(1, where);
    textcolor( CYAN );
    if (blankOK)
    {
        if (Options.prev_name.length() && Options.remember_name)
            cprintf(EOL
                    "Press <Enter> for \"%s\", or . to be prompted later."
                    EOL,
                    Options.prev_name.c_str());
        else
            cprintf(EOL 
                    "Press <Enter> to answer this after race and "
                    "class are chosen." EOL);
    }

    cprintf(EOL "What is your name today? ");

    if (!existing_chars.empty())
    {
        const int name_x = wherex(), name_y = wherey();
        menu.set_limits(name_y + 3, get_number_of_lines());
        menu.display();
        gotoxy(name_x, name_y);
    }

    textcolor( LIGHTGREY );
}

static void preprocess_character_name(char *name, bool blankOK)
{
    if (!*name && blankOK && Options.prev_name.length() &&
            Options.remember_name)
    {
        strncpy(name, Options.prev_name.c_str(), kNameLen);
        name[kNameLen - 1] = 0;
    }

    // '.', '?' and '*' are blanked.
    if (!name[1] && (*name == '.' ||
                      *name == '*' ||
                      *name == '?'))
        *name = 0;
}

static bool is_good_name(char *name, bool blankOK, bool verbose)
{
    preprocess_character_name(name, blankOK);

    // verification begins here {dlb}:
    if (you.your_name[0] == 0)
    {
        if (blankOK)
            return (true);

        if (verbose)
            cprintf(EOL "That's a silly name!" EOL);
        return (false);
    }

    // if MULTIUSER is defined, userid will be tacked onto the end
    // of each character's files, making bones a valid player name.
#ifndef MULTIUSER
    // this would cause big probs with ghosts
    // what would? {dlb}
    // ... having the name "bones" of course! The problem comes from
    // the fact that bones files would have the exact same filename
    // as level files for a character named "bones".  -- bwr
    if (stricmp(you.your_name, "bones") == 0)
    {
        if (verbose)
            cprintf(EOL "That's a silly name!" EOL);
        return (false);
    }
#endif
    return (validate_player_name(verbose));
}

static int newname_keyfilter(int &ch)
{
    if (ch == CK_DOWN || ch == CK_PGDN || ch == '\t')
        return -1;
    return 1;
}

static bool read_player_name(
        char *name, 
        int len,
        const std::vector<player> &existing,
        slider_menu &menu)
{
    const int name_x = wherex(), name_y = wherey();
    int (*keyfilter)(int &) = newname_keyfilter;
    if (existing.empty())
        keyfilter = NULL;

    line_reader reader(name, len);
    reader.set_keyproc(keyfilter);

    for (;;)
    {
        gotoxy(name_x, name_y);
        if (name_x <= 80)
            cprintf("%-*s", 80 - name_x + 1, "");

        gotoxy(name_x, name_y);
        int ret = reader.read_line(false);
        if (!ret)
            return (true);

        if (ret == CK_ESCAPE)
            return (false);

        if (ret != CK_ESCAPE && existing.size())
        {
            menu.set_search(name);
            menu.show();
            const MenuEntry *sel = menu.selected_entry();
            if (sel)
            {
                const player &p = *static_cast<player*>( sel->data );
                strncpy(name, p.your_name, kNameLen);
                name[kNameLen - 1] = 0;
                return (true);
            }
        }

        // Go back and prompt the user.
    }
}

static void enter_player_name(bool blankOK)
{
    int prompt_start = wherey();
    bool ask_name = true;
    char *name = you.your_name;
    std::vector<player> existing_chars;
    slider_menu char_menu;

    if (you.your_name[0] != 0)
        ask_name = false;

    if (blankOK && (ask_name || !is_good_name(you.your_name, false, false)))
    {
        existing_chars = find_saved_characters();
        if (existing_chars.size() == 0) 
        {
             gotoxy(1,12);
             cprintf("  If you've never been here before, "
                     "you might want to try out" EOL);
             cprintf("  the Dungeon Crawl tutorial. To do this, press ");
             textcolor(WHITE);
             cprintf("T");
             textcolor(LIGHTGREY);
             cprintf(" on the next" EOL);
             cprintf("  screen.");
        }    

        MenuEntry *title = new MenuEntry("Or choose an existing character:");
        title->colour = LIGHTCYAN;
        char_menu.set_title( title );
        for (int i = 0, size = existing_chars.size(); i < size; ++i)
        {
            std::string desc = " " + existing_chars[i].short_desc();
            if ((int) desc.length() >= get_number_of_cols())
                desc = desc.substr(0, get_number_of_cols() - 1);

            MenuEntry *me = new MenuEntry(desc);
            me->data = &existing_chars[i];
            char_menu.add_entry(me);
        }
        char_menu.set_flags(MF_NOWRAP | MF_SINGLESELECT);
    }

    do
    {
        // prompt for a new name if current one unsatisfactory {dlb}:
        if (ask_name)
        {
            show_name_prompt(prompt_start, blankOK, existing_chars, char_menu);

            // If the player wants out, we bail out.
            if (!read_player_name(name, kNameLen, existing_chars, char_menu))
                end(0);

            // Laboriously trim the damn thing.
            std::string read_name = name;
            trim_string(read_name);
            strncpy(name, read_name.c_str(), kNameLen);
            name[kNameLen - 1] = 0;
        }
    }
    while (ask_name = !is_good_name(you.your_name, blankOK, true));
}                               // end enter_player_name()

static bool validate_player_name(bool verbose)
{
#if defined(DOS) || defined(WIN32CONSOLE)
    // quick check for CON -- blows up real good under DOS/Windows
    if (stricmp(you.your_name, "con") == 0
        || stricmp(you.your_name, "nul") == 0
        || stricmp(you.your_name, "prn") == 0
        || strnicmp(you.your_name, "LPT", 3) == 0)
    {
        if (verbose)
            cprintf(EOL "Sorry, that name gives your OS a headache." EOL);
        return (false);
    }
#endif

    for (const char *pn = you.your_name; *pn; ++pn)
    {
        char c = *pn;
        // Note that this includes systems which may be using the
        // packaging system.  The packaging system is very simple 
        // and doesn't take the time to escape every characters that
        // might be a problem for some random shell or OS... so we 
        // play it very conservative here.  -- bwr
        if (!isalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
        {
            if (verbose)
                cprintf( EOL
                         "Alpha-numerics, spaces, dashes, periods "
                         "and underscores only, please."
                         EOL );
            return (false);
        }
    }

    return (true);
}                               // end validate_player_name()

#ifdef USE_SPELLCASTER_AND_RANGER_WANDERER_TEMPLATES
static void give_random_scroll( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_SCROLLS;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].special = 0;

    switch (random2(8))
    {
    case 0:
        you.inv[ slot ].sub_type = SCR_DETECT_CURSE;
        break;

    case 1:
        you.inv[ slot ].sub_type = SCR_IDENTIFY;
        break;

    case 2:
    case 3:
        you.inv[ slot ].sub_type = SCR_BLINKING;
        break;

    case 4:
        you.inv[ slot ].sub_type = SCR_FEAR;
        break;

    case 5:
        you.inv[ slot ].sub_type = SCR_SUMMONING;
        break;

    case 6:
    case 7:
    default:
        you.inv[ slot ].sub_type = SCR_TELEPORTATION;
        break;
    }
}
#endif

static void give_random_potion( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_POTIONS;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;

    switch (random2(8))
    {
    case 0:
    case 1:
    case 2:
        you.inv[ slot ].sub_type = POT_HEALING;
        break;
    case 3:
    case 4:
        you.inv[ slot ].sub_type = POT_HEAL_WOUNDS;
        break;
    case 5:
        you.inv[ slot ].sub_type = POT_SPEED;
        break;
    case 6:
        you.inv[ slot ].sub_type = POT_MIGHT;
        break;
    case 7:
        you.inv[ slot ].sub_type = POT_BERSERK_RAGE;
        break;
    }
}

#ifdef USE_SPELLCASTER_AND_RANGER_WANDERER_TEMPLATES
static void give_random_wand( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_WANDS;
    you.inv[ slot ].special = 0;
    you.inv[ slot ].plus2 = 0;

    switch (random2(4))
    {
    case 0:
        you.inv[ slot ].sub_type = WAND_SLOWING;
        you.inv[ slot ].plus = 7 + random2(5);
        break;
    case 1:
        you.inv[ slot ].sub_type = WAND_PARALYSIS;
        you.inv[ slot ].plus = 5 + random2(4);
        break;
    case 2:
        you.inv[ slot ].sub_type = coinflip() ? WAND_FROST : WAND_FLAME;
        you.inv[ slot ].plus = 6 + random2(4);
        break;
    case 3:
        you.inv[ slot ].sub_type = WAND_TELEPORTATION;
        you.inv[ slot ].plus = 3 + random2(4);
        break;
    }
}
#endif

static void give_random_secondary_armour( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_ARMOUR;
    you.inv[ slot ].special = 0;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;

    switch (random2(4))
    {
    case 0:
        you.inv[ slot ].sub_type = ARM_CLOAK;
        you.equip[EQ_CLOAK] = slot;
        break;
    case 1:
        you.inv[ slot ].sub_type = ARM_BOOTS;
        you.equip[EQ_BOOTS] = slot;
        break;
    case 2:
        you.inv[ slot ].sub_type = ARM_GLOVES;
        you.equip[EQ_GLOVES] = slot;
        break;
    case 3:
        you.inv[ slot ].sub_type = ARM_HELMET;
        you.equip[EQ_HELMET] = slot;
        break;
    }
}

// Returns true if a "good" weapon is given
static bool give_wanderer_weapon( int slot, int wpn_skill )
{
    bool ret = false;

    // Slot's always zero, but we pass it anyways.

    // We'll also re-fill the template, all this for later possible
    // safe reuse of code in the future.
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_WEAPONS;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;
    you.inv[ slot ].special = 0;

    // Now fill in the type according to the random wpn_skill
    switch (wpn_skill)
    {
    case SK_MACES_FLAILS:
        you.inv[ slot ].sub_type = WPN_CLUB;
        break;

    case SK_POLEARMS:
        you.inv[ slot ].sub_type = WPN_SPEAR;
        break;

    case SK_SHORT_BLADES:
        you.inv[ slot ].sub_type = WPN_DAGGER;
        break;

    case SK_AXES:
        you.inv[ slot ].sub_type = WPN_HAND_AXE;
        ret = true;
        break;

    case SK_STAVES:
        you.inv[ slot ].sub_type = WPN_QUARTERSTAFF;
        ret = true;
        break;

    case SK_LONG_SWORDS:
    default:
        // all long swords are too good for a starting character...
        // especially this class where we have to be careful about
        // giving away anything good at all.
        // We default here if the character only has fighting skill -- bwr
        you.inv[ slot ].sub_type = WPN_SHORT_SWORD;
        ret = true;
        break;
    }

    return (ret);
}

static void make_rod(item_def &item, int rod_type)
{
    item.base_type = OBJ_STAVES;
    item.sub_type = rod_type;
    item.quantity = 1;
    item.special = 0;
    item.colour = BROWN;
    
    init_rod_mp(item);
}

//
// The idea behind wanderers is a class that has various different
// random skills that's a challenge to play... not a class that can
// be continually rerolled to gain the ideal character.  To maintain
// this, we have to try and make sure that they typically get worse
// equipment than any other class... this for certain means no
// spellbooks ever, and the bows and xbows down below might be too
// much... so pretty much things should be removed rather than
// added here. -- bwr
//
static void create_wanderer( void )
{
    const int util_skills[] =
        { SK_DARTS, SK_RANGED_COMBAT, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_util_skills = sizeof(util_skills) / sizeof(int);

    // Long swords is missing to increase its rarity because we
    // can't give out a long sword to a starting character (they're
    // all too good)... Staves is also removed because it's not
    // one of the fighter options.-- bwr
    const int fight_util_skills[] =
        { SK_FIGHTING, SK_SHORT_BLADES, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS,
          SK_DARTS, SK_RANGED_COMBAT, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_fight_util_skills = sizeof(fight_util_skills) / sizeof(int);

    const int not_rare_skills[] =
        { SK_SLINGS, SK_BOWS, SK_CROSSBOWS,
          SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS,
          SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,
          SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_SWORDS, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES,
          SK_DARTS, SK_RANGED_COMBAT, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_not_rare_skills = sizeof(not_rare_skills) / sizeof(int);

    const int all_skills[] =
        { SK_SUMMONINGS, SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMIGRATION,
          SK_DIVINATIONS, SK_POISON_MAGIC,
          SK_SLINGS, SK_BOWS, SK_CROSSBOWS,
          SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS,
          SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,
          SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_SWORDS, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES,
          SK_DARTS, SK_RANGED_COMBAT, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_all_skills = sizeof(all_skills) / sizeof(int);

    int skill;

    for (int i = 0; i < 2; i++)
    {
        do
        {
            skill = random2( num_util_skills );
        }
        while (you.skills[ util_skills[ skill ]] >= 2);

        you.skills[ util_skills[ skill ]] += 1;
    }

    for (int i = 0; i < 3; i++)
    {
        do
        {
            skill = random2( num_fight_util_skills );
        }
        while (you.skills[ fight_util_skills[ skill ]] >= 2);

        you.skills[ fight_util_skills[ skill ]] += 1;
    }

    // Spell skills are possible past this point, but we won't
    // allow two levels of any of them -- bwr
    for (int i = 0; i < 3; i++)
    {
        do
        {
            skill = random2( num_not_rare_skills );
        }
        while (you.skills[ not_rare_skills[ skill ]] >= 2
                || (not_rare_skills[ skill ] >= SK_SPELLCASTING
                    && you.skills[ not_rare_skills[ skill ]]));

        you.skills[ not_rare_skills[ skill ]] += 1;
    }

    for (int i = 0; i < 2; i++)
    {
        do
        {
            skill = random2( num_all_skills );
        }
        while (you.skills[all_skills[ skill ]] >= 2
                || (all_skills[ skill ] >= SK_SPELLCASTING
                    && you.skills[ all_skills[ skill ]]));

        you.skills[ all_skills[ skill ]] += 1;
    }

    // Demigods can't use invocations so we'll swap it for something else
    if (you.species == SP_DEMIGOD && you.skills[ SK_INVOCATIONS ])
    {
        you.skills[ SK_INVOCATIONS ] = 0;

        do
        {
            skill = random2( num_all_skills );
        }
        while (skill == SK_INVOCATIONS && you.skills[all_skills[ skill ]]);

        you.skills[ skill ] = 1;
    }

    int wpn_skill = SK_FIGHTING;  // preferred weapon type
    int wpn_skill_size = 0;       // level of skill in preferred weapon type
    int num_wpn_skills = 0;       // used to choose preferred weapon
    int total_wpn_skills = 0;     // used to choose template

    // This algorithm is the same as the one used to pick a random
    // angry god for retribution, except that whenever a higher skill
    // is found than the current one, we automatically take it and
    // only consider skills at that level or higher from that point on,
    // This should give a random wpn skill from the set of skills with
    // the highest value. -- bwr
    for (int i = SK_SHORT_BLADES; i <= SK_STAVES; i++)
    {
        if (you.skills[i] > 0)
        {
            total_wpn_skills++;

            if (you.skills[i] > wpn_skill_size)
            {
                // switch to looking in the new set of better skills
                num_wpn_skills = 1; // reset to one, because it's a new set
                wpn_skill = i;
                wpn_skill_size = you.skills[i];
            }
            else if (you.skills[i] == wpn_skill_size)
            {
                // still looking at the old level
                num_wpn_skills++;
                if (one_chance_in( num_wpn_skills ))
                {
                    wpn_skill = i;
                    wpn_skill_size = you.skills[i];
                }
            }
        }
    }

    // Let's try to make an appropriate weapon
    // Start with a template for a weapon
    you.inv[0].quantity = 1;
    you.inv[0].base_type = OBJ_WEAPONS;
    you.inv[0].sub_type = WPN_KNIFE;
    you.inv[0].plus = 0;
    you.inv[0].plus2 = 0;
    you.inv[0].special = 0;

    // And a default armour template for a robe (leaving slot 1 open for
    // a secondary weapon).
    you.inv[2].quantity = 1;
    you.inv[2].base_type = OBJ_ARMOUR;
    you.inv[2].sub_type = ARM_ROBE;
    you.inv[2].plus = 0;
    you.inv[2].special = 0;

    // Wanderers have at least seen one type of potion, and if they
    // don't get anything else good, they'll get to keep this one...
    // Note:  even if this is taken away, the knowledge of the potion
    // type is still given to the character.
    give_random_potion(3);

    if (you.skills[SK_FIGHTING] || total_wpn_skills >= 3)
    {
        // Fighter style wanderer
        if (you.skills[SK_ARMOUR])
        {
            you.inv[2].sub_type = ARM_RING_MAIL;
            you.inv[3].quantity = 0;            // remove potion
        }
        else if (you.skills[SK_SHIELDS] && wpn_skill != SK_STAVES)
        {
            you.inv[4].quantity = 1;
            you.inv[4].base_type = OBJ_ARMOUR;
            you.inv[4].sub_type = ARM_BUCKLER;
            you.inv[4].plus = 0;
            you.inv[4].special = 0;
            you.equip[EQ_SHIELD] = 4;

            you.inv[3].quantity = 0;            // remove potion
        }
        else
        {
            give_random_secondary_armour(5);
        }

        // remove potion if good weapon is given:
        if (give_wanderer_weapon( 0, wpn_skill ))
            you.inv[3].quantity = 0;
    }
#ifdef USE_SPELLCASTER_AND_RANGER_WANDERER_TEMPLATES
    else if (you.skills[ SK_SPELLCASTING ])
    {
        // Spellcaster style wanderer

        // Could only have learned spells in common schools...
        const int school_list[5] =
            { SK_CONJURATIONS,
              SK_ENCHANTMENTS, SK_ENCHANTMENTS,
              SK_TRANSLOCATIONS, SK_NECROMANCY };

        //jmf: Two of those spells are gone due to their munchkinicity.
        //     crush() and arc() are like having good melee capability.
        //     Therefore giving them to "harder" class makes less-than-
        //     zero sense, and they're now gone.
        const int spell_list[5] =
           { SPELL_MAGIC_DART,
             SPELL_CONFUSING_TOUCH, SPELL_BACKLIGHT,
             SPELL_APPORTATION, SPELL_ANIMATE_SKELETON };

        // Choose one of the schools we have at random.
        int school = SK_SPELLCASTING;
        int num_schools = 0;
        for (int i = 0; i < 5; i++)
        {
            if (you.skills[ school_list[ i ]])
            {
                num_schools++;
                if (one_chance_in( num_schools ))
                    school = i;
            }
        }

        // Magic dart is quite a good spell, so if the player only has
        // spellcasting and conjurations, we sometimes hold off... and
        // treat them like an unskilled spellcaster.
        if (school == SK_SPELLCASTING
            || (num_schools == 1 && school == SK_CONJURATIONS && coinflip()))
        {
            // Not much melee potential and no common spell school,
            // we'll give the player a dagger.
            you.inv[0].sub_type = WPN_DAGGER;

            // ... and a random scroll
            give_random_scroll(4);

            // ... and knowledge of another
            give_random_scroll(5);
            you.inv[5].quantity = 0;

            // ... and a wand.
            give_random_wand(6);
        }
        else
        {
            // Give them an appropriate spell
            add_spell_to_memory( spell_list[ school ] );
        }
    }
    else if (you.skills[ SK_RANGED_COMBAT ] && one_chance_in(3)) // these are rare
    {
        // Ranger style wanderer
        // Rare since starting with a throwing weapon is very good

        // Create a default launcher template, but the
        // quantity may be reset to 0 if we don't want one -- bwr
        // throwing weapons are lowered to -1 to make them
        // not as good as the one's hunters get, ammo is
        // also much smaller -- bwr
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_BOW;
        you.inv[1].plus = -1;
        you.inv[1].plus2 = -1;
        you.inv[1].special = 0;

        // Create default ammo template (darts) (armour is slot 2)
        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 6 );
        you.inv[4].plus = 0;
        you.inv[4].plus2 = 0;
        you.inv[4].special = 0;

        if (you.skills[ SK_SLINGS ])
        {
            // slingers get some extra ammo
            you.inv[4].quantity += random2avg(20,5);
            you.inv[4].sub_type = MI_STONE;
            you.inv[1].sub_type = WPN_SLING;
            you.inv[1].plus = 0;               // slings aren't so good
            you.inv[1].plus2 = 0;              // so we'll make them +0

            you.inv[3].quantity = 0;            // remove potion
            you.inv[3].base_type = 0;               // forget potion
            you.inv[3].sub_type = 0;
        }
        else if (you.skills[ SK_BOWS ])
        {
            you.inv[4].sub_type = MI_ARROW;
            you.inv[1].sub_type = WPN_BOW;

            you.inv[3].quantity = 0;            // remove potion
            you.inv[3].base_type = 0;               // forget potion
            you.inv[3].sub_type = 0;
        }
        else if (you.skills[ SK_CROSSBOWS ])
        {
            // Hand crossbows want the darts.
            you.inv[1].sub_type = WPN_HAND_CROSSBOW;

            you.inv[3].quantity = 0;            // remove potion
            you.inv[3].base_type = 0;               // forget potion
            you.inv[3].sub_type = 0;
        }
        else
        {
            // little extra poisoned darts for throwers
            you.inv[4].quantity += random2avg(10,5);
            set_item_ego_type( you.inv[4], OBJ_MISSILES, SPMSL_POISONED );

            you.inv[0].sub_type = WPN_DAGGER;       // up knife to dagger
            you.inv[1].quantity = 0;            // remove bow
        }
    }
#endif
    else
    {
        // Generic wanderer
        give_wanderer_weapon( 0, wpn_skill );
        give_random_secondary_armour(5);
    }

    you.equip[EQ_WEAPON] = 0;
    you.equip[EQ_BODY_ARMOUR] = 2;
}

static char letter_to_class(int keyn)
{
    if (keyn == 'a')
        return JOB_FIGHTER;
    else if (keyn == 'b')
        return JOB_WIZARD;
    else if (keyn == 'c')
        return JOB_PRIEST;
    else if (keyn == 'd')
        return JOB_THIEF;
    else if (keyn == 'e')
        return JOB_GLADIATOR;
    else if (keyn == 'f')
        return JOB_NECROMANCER;
    else if (keyn == 'g')
        return JOB_PALADIN;
    else if (keyn == 'h')
        return JOB_ASSASSIN;
    else if (keyn == 'i')
        return JOB_BERSERKER;
    else if (keyn == 'j')
        return JOB_HUNTER;
    else if (keyn == 'k')
        return JOB_CONJURER;
    else if (keyn == 'l')
        return JOB_ENCHANTER;
    else if (keyn == 'm')
        return JOB_FIRE_ELEMENTALIST;
    else if (keyn == 'n')
        return JOB_ICE_ELEMENTALIST;
    else if (keyn == 'o')
        return JOB_SUMMONER;
    else if (keyn == 'p')
        return JOB_AIR_ELEMENTALIST;
    else if (keyn == 'q')
        return JOB_EARTH_ELEMENTALIST;
    else if (keyn == 'r')
        return JOB_CRUSADER;
    else if (keyn == 's')
        return JOB_DEATH_KNIGHT;
    else if (keyn == 't')
        return JOB_VENOM_MAGE;
    else if (keyn == 'u')
        return JOB_CHAOS_KNIGHT;
    else if (keyn == 'v')
        return JOB_TRANSMUTER;
    else if (keyn == 'w')
        return JOB_HEALER;
    else if (keyn == 'y')
        return JOB_REAVER;
    else if (keyn == 'z')
        return JOB_STALKER;
    else if (keyn == 'A')
        return JOB_MONK;
    else if (keyn == 'B')
        return JOB_WARPER;
    else if (keyn == 'C')
        return JOB_WANDERER;
    return JOB_UNKNOWN;
}

static char letter_to_species(int keyn)
{
    switch (keyn)
    {
    case 'a':
        return SP_HUMAN;
    case 'b':
        return SP_ELF;
    case 'c':
        return SP_HIGH_ELF;
    case 'd':
        return SP_GREY_ELF;
    case 'e':
        return SP_DEEP_ELF;
    case 'f':
        return SP_SLUDGE_ELF;
    case 'g':
        return SP_HILL_DWARF;
    case 'h':
        return SP_MOUNTAIN_DWARF;
    case 'i':
        return SP_HALFLING;
    case 'j':
        return SP_HILL_ORC;
    case 'k':
        return SP_KOBOLD;
    case 'l':
        return SP_MUMMY;
    case 'm':
        return SP_NAGA;
    case 'n':
        return SP_GNOME;
    case 'o':
        return SP_OGRE;
    case 'p':
        return SP_TROLL;
    case 'q':
        return SP_OGRE_MAGE;
    case 'r':                   // draconian
        return SP_RED_DRACONIAN + random2(9);    // random drac
    case 's':
        return SP_CENTAUR;
    case 't':
        return SP_DEMIGOD;
    case 'u':
        return SP_SPRIGGAN;
    case 'v':
        return SP_MINOTAUR;
    case 'w':
        return SP_DEMONSPAWN;
    case 'x':
        return SP_GHOUL;
    case 'y':
        return SP_KENKU;
    case 'z':
        return SP_MERFOLK;
    default:
        return 0;
    }
}

static char species_to_letter(int spec)
{
    if (spec > SP_RED_DRACONIAN && spec <= SP_BASE_DRACONIAN)
        spec = SP_RED_DRACONIAN;
    else if (spec > SP_BASE_DRACONIAN)
        spec -= SP_BASE_DRACONIAN - SP_RED_DRACONIAN;
    return 'a' + spec - 1;
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
        you.char_class = letter_to_class(Options.cls);
        ng_cls = Options.cls;
    }
        
    if (Options.race != 0)
        printed = true;

spec_query:
    bool prevraceok = Options.prev_race == '*';
    if (!printed)
    {
        clrscr();

        if (you.char_class != JOB_UNKNOWN)
        {
            textcolor( BROWN );
            bool shortgreet = false;
            if (strlen(you.your_name) || you.char_class != JOB_UNKNOWN)
                cprintf("Welcome, ");
            else
            {
                cprintf("Welcome.");
                shortgreet = true;
            }

            textcolor( YELLOW );
            if (strlen(you.your_name) > 0)
            {
                cprintf("%s", you.your_name);
                if (you.char_class != JOB_UNKNOWN)
                    cprintf(" the ");
            }
            if (you.char_class != JOB_UNKNOWN)
                cprintf("%s", get_class_name(you.char_class));

            if (!shortgreet)
                cprintf(".");
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press T to enter a tutorial.)");
        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:");
        cprintf("  (Press ? for more information)"); 
        cprintf(EOL EOL);

        textcolor( LIGHTGREY );

        int linec = 0;
        char linebuf[200];
        *linebuf = 0;
        for (int i = SP_HUMAN; i < NUM_SPECIES; ++i)
        {
            if (i > SP_RED_DRACONIAN && i <= SP_BASE_DRACONIAN)
                continue;

            if (you.char_class != JOB_UNKNOWN && 
                    !class_allowed(i, you.char_class))
                continue;

            char buf[100];
            char sletter = species_to_letter(i);
            snprintf(buf, sizeof buf, "%c - %-26s",
                        sletter,
                        species_name(i, 1));
            if (sletter == Options.prev_race)
                prevraceok = true;
            strncat(linebuf, buf, sizeof linebuf);
            if (++linec >= 2)
            {
                cprintf("%s" EOL, linebuf);
                *linebuf = 0;
                linec = 0;
            }
        }
        
        if (linec)
            cprintf("%s" EOL, linebuf);

        textcolor( BROWN );
        
        if (you.char_class == JOB_UNKNOWN)
            cprintf(EOL
                    "SPACE - Choose class first; * - Random Species; "
                    "! - Random Character; X - Quit"
                    EOL);
        else
            cprintf(EOL
                    "* - Random; Bksp - Back to class selection; X - Quit" 
                    EOL);

        if (Options.prev_race)
        {
            if (prevraceok)
                cprintf("Enter - %s", get_opt_race_name(Options.prev_race).c_str());
            if (prev_startup_options_set())
                cprintf("%sTAB - %s", 
                        prevraceok? "; " : "",
                        prev_startup_description().c_str());
            cprintf(EOL);
        }

        textcolor( CYAN );
        cprintf(EOL "Which one? ");
        textcolor( LIGHTGREY );

        printed = true;
    }

    if (Options.race != 0)
    {
        keyn = Options.race;
    }
    else
    {
        keyn = c_getch();
    }

    if ( keyn == '?' )
    {
        list_commands(false, '1');
        return choose_race();
    }

    if ((keyn == '\r' || keyn == '\n') && Options.prev_race && prevraceok)
        keyn = Options.prev_race;

    if (keyn == '\t' && prev_startup_options_set())
    {
        if (Options.prev_randpick || 
            (Options.prev_race == '*' && Options.prev_cls == '*'))
        {
            Options.random_pick = true;
            ng_random = true;
            pick_random_species_and_class();
            return false;
        }
        set_startup_options();
        you.species = 0;
        you.char_class = JOB_UNKNOWN;
        return true;
    }

    if (keyn == CK_BKSP || keyn == ' ')
    {
        you.species = 0;
        Options.race = 0;
        return true;
    }

    bool randrace = (keyn == '*');
    if (keyn == '*')
    {
        do
            keyn = 'a' + random2(26);
        while (you.char_class != JOB_UNKNOWN &&
                !class_allowed(letter_to_species(keyn), you.char_class));
    }
    else if (keyn == '!')
    {
        pick_random_species_and_class();
        Options.random_pick = true; // used to give random weapon/god as well
        ng_random = true;
        return false;
    }
    else if (keyn == 'T')
    {
        return !pick_tutorial();
    }

    if (!(you.species = letter_to_species(keyn)))
    {
        switch (keyn)
        {
        case 'X':
            cprintf(EOL "Goodbye!");
            end(0);
            break;
        default:
            if (Options.race != 0)
            {
                Options.race = 0;
                printed = false;
            }
            goto spec_query;
        }
    }
    if (you.species && you.char_class != JOB_UNKNOWN
            && !class_allowed(you.species, you.char_class))
        goto spec_query;

    // set to 0 in case we come back from choose_class()
    Options.race = 0;
    ng_race = randrace? '*' : keyn;

    return true;
}

// returns true if a class was chosen, false if we should go back to
// race selection.

bool choose_class(void)
{
    char keyn;
    int i,j;

    bool printed = false;
    if (Options.cls != 0)
        printed = true;

    if (you.species && you.char_class != JOB_UNKNOWN)
        return true;

    ng_cls = 0;
    
job_query:
    bool prevclassok = Options.prev_cls == '*';
    if (!printed)
    {
        clrscr();

        if (you.species)
        {
            textcolor( BROWN );
            bool shortgreet = false;
            if (strlen(you.your_name) || you.species)
                cprintf("Welcome, ");
            else
            {
                cprintf("Welcome.");
                shortgreet = true;
            }

            textcolor( YELLOW );
            if (strlen(you.your_name) > 0)
            {
                cprintf("%s", you.your_name);
                if (you.species)
                    cprintf(" the ");
            }
            if (you.species)
                cprintf("%s", species_name(you.species,you.experience_level));

            if (!shortgreet)
                cprintf(".");
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press T to enter a tutorial.)");

        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:");
        cprintf("  (Press ? for more information)"); 
        cprintf(EOL EOL);

        textcolor( LIGHTGREY );

        j = 0;               // used within for loop to determine newline {dlb}

        for (i = 0; i < NUM_JOBS; i++)
        {
            if (you.species? !class_allowed(you.species, i) : i == JOB_QUITTER)
                continue;

            char letter = index_to_letter(i);

            if (letter == Options.prev_cls)
                prevclassok = true;
            
            putch( letter );
            cprintf( " - " );
            cprintf( "%s", get_class_name(i) );

            if (j % 2)
                cprintf(EOL);
            else
                gotoxy(31, wherey());

            j++;
        }

        if (j % 2)
            cprintf(EOL);

        textcolor( BROWN );
        if (!you.species)
            cprintf(EOL
                    "SPACE - Choose species first; * - Random Class; "
                    "! - Random Character; X - Quit"
                    EOL);
        else
            cprintf(EOL
                    "* - Random; Bksp - Back to species selection; X - Quit" 
                    EOL);

        if (Options.prev_cls)
        {
            if (prevclassok)
                cprintf("Enter - %s", get_opt_class_name(Options.prev_cls).c_str());
            if (prev_startup_options_set())
                cprintf("%sTAB - %s", 
                        prevclassok? "; " : "",
                        prev_startup_description().c_str());
            cprintf(EOL);
        }
        
        textcolor( CYAN );
        cprintf(EOL "Which one? ");
        textcolor( LIGHTGREY );

        printed = true;
    }

    if (Options.cls != 0)
    {
        keyn = Options.cls;
    }
    else
    {
        keyn = c_getch();
    }

    if ( keyn == '?' )
    {
        list_commands(false, '2');
        return choose_class();
    }

    if ((keyn == '\r' || keyn == '\n') && Options.prev_cls && prevclassok)
        keyn = Options.prev_cls;

    if (keyn == '\t' && prev_startup_options_set())
    {
        if (Options.prev_randpick || 
                (Options.prev_race == '?' && Options.prev_cls == '?'))
        {
            Options.random_pick = true;
            ng_random = true;
            pick_random_species_and_class();
            return true;
        }
        set_startup_options();

        // Toss out old species selection, if any.
        you.species = 0;
        you.char_class = JOB_UNKNOWN;

        return false;
    }

    if ((you.char_class = letter_to_class(keyn)) == JOB_UNKNOWN) 
    {
        if (keyn == '*')
        {
            // pick a job at random... see god retribution for proof this
            // is uniform. -- bwr
            int job_count = 0;
            int job = -1;

            for (i = 0; i < NUM_JOBS; i++)
            {
                if ( i == JOB_QUITTER )
                    continue;

                if (!you.species || class_allowed(you.species, i))
                {
                    job_count++;
                    if (one_chance_in( job_count ))
                        job = i;
                }
            }

            ASSERT( job != -1 );  // at least one class should have been allowed
            you.char_class = job;

            ng_cls = '*';
        }
        else if (keyn == '!')
        {
            pick_random_species_and_class();
            // used to give random weapon/god as well
            Options.random_pick = true;
            ng_random = true;
            return true;
        }
        else if (keyn == 'T')
        {
            return pick_tutorial();
        }        
        else if ((keyn == ' ' && !you.species) ||
                    keyn == 'x' || keyn == ESCAPE || keyn == CK_BKSP)
        {
            you.char_class = JOB_UNKNOWN;
            return false;
        }
        else if (keyn == 'X')
        {
            cprintf(EOL "Goodbye!");
            end(0);
        }
        else
        {
            if (Options.cls != 0)
            {
                Options.cls = 0;
                printed = false;
            }
            goto job_query;
        }
    }

    if (you.species && !class_allowed(you.species, you.char_class))
    {
        if (Options.cls != 0)
        {
            Options.cls = 0;
            printed = false;
        }
        goto job_query;
    }

    if (ng_cls != '*')
        ng_cls = keyn;

    return you.char_class != JOB_UNKNOWN && you.species;
}


void give_items_skills()
{
    char keyn;
    int weap_skill = 0;
    int to_hit_bonus;           // used for assigning primary weapons {dlb}
    int choice;                 // used for third-screen choices

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        if (you.species == SP_OGRE || you.species == SP_TROLL
            || player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            if (you.species == SP_OGRE)
            {
                you.inv[0].quantity = 1;
                you.inv[0].base_type = OBJ_WEAPONS;
                you.inv[0].sub_type = WPN_ANCUS;
                you.inv[0].plus = 0;
                you.inv[0].special = 0;
            }
            else if (you.species == SP_TROLL)
            {
                // no weapon for trolls
                you.inv[0] = item_def();
            }
            else if (player_genus(GENPC_DRACONIAN))
            {
                you.inv[2].quantity = 1;
                you.inv[2].base_type = OBJ_ARMOUR;
                you.inv[2].sub_type = ARM_SHIELD;
                you.inv[2].plus = 0;
                you.inv[2].special = 0;
            }
        }
        else if (you.species == SP_GHOUL || you.species == SP_MUMMY)
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ROBE;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            if (you.species == SP_MUMMY)
            {
                you.inv[2].quantity = 1;
                you.inv[2].base_type = OBJ_ARMOUR;
                you.inv[2].sub_type = ARM_SHIELD;
                you.inv[2].plus = 0;
                you.inv[2].special = 0;
            }
        }
        else if (you.species == SP_KOBOLD)
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_LEATHER_ARMOUR;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            you.inv[2].base_type = OBJ_MISSILES;
            you.inv[2].sub_type = MI_DART;
            you.inv[2].quantity = 10 + roll_dice( 2, 10 );
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
        }
        else
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_SCALE_MAIL;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_SHIELD;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;

            choose_weapon();
        }

        if (you.species != SP_TROLL)
            you.equip[EQ_WEAPON] = 0;

        you.equip[EQ_BODY_ARMOUR] = 1;

        if (you.species != SP_KOBOLD && you.species != SP_OGRE
            && you.species != SP_TROLL && you.species != SP_GHOUL)
        {
            you.equip[EQ_SHIELD] = 2;
        }

        you.skills[SK_FIGHTING] = 3;

        if (you.species != SP_TROLL)
            weap_skill = 2;

        if (you.species == SP_KOBOLD)
        {
            you.skills[SK_RANGED_COMBAT] = 1;
            you.skills[SK_DARTS] = 1;
            you.skills[SK_DODGING] = 1;
            you.skills[SK_STEALTH] = 1;
            you.skills[SK_STABBING] = 1;
            you.skills[SK_DODGING + random2(3)] += 1;
        }
        else if (you.species == SP_OGRE || you.species == SP_TROLL)
        {
            if (you.species == SP_TROLL)  //jmf: these guys get no weapon!
                you.skills[SK_UNARMED_COMBAT] += 3;
            else 
                you.skills[SK_FIGHTING] += 2;

            // BWR sez Ogres & Trolls should probably start w/ Dodge 2 -- GDL
            you.skills[SK_DODGING] = 3;
        }
        else
        {
            // Players get dodging or armour skill depending on their
            // starting armour now (note: the armour has to be equipped
            // for this function to work)

            you.skills[(player_light_armour()? SK_DODGING : SK_ARMOUR)] = 2;

            you.skills[SK_SHIELDS] = 2;
            you.skills[SK_RANGED_COMBAT] = 2;
            you.skills[(coinflip() ? SK_STABBING : SK_SHIELDS)]++;
        }
        break;

    case JOB_WIZARD:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;

        if (you.species == SP_OGRE_MAGE)
            you.inv[0].sub_type = WPN_QUARTERSTAFF;
        else if (player_genus(GENPC_DWARVEN))
            you.inv[0].sub_type = WPN_HAMMER;
        else
            you.inv[0].sub_type = WPN_DAGGER;

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;

        switch (random2(7))
        {
        case 0:
        case 1:
        default:
            set_equip_desc( you.inv[1], ISFLAG_EMBROIDERED_SHINY );
            break;
        case 2:
        case 3:
            set_equip_desc( you.inv[1], ISFLAG_GLOWING );
            break;
        case 4:
        case 5:
            set_equip_desc( you.inv[1], ISFLAG_RUNED );
            break;
        case 6:
            set_equip_race( you.inv[1], ISFLAG_ELVEN );
            break;
        }

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        // extra items being tested:
        choose_book( you.inv[2], BOOK_MINOR_MAGIC_I, 3 );

        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;
        you.skills[(coinflip() ? SK_DODGING : SK_STEALTH)]++;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_ENCHANTMENTS] = 1;
        you.skills[SK_SPELLCASTING + random2(3)]++;
        you.skills[SK_SUMMONINGS + random2(5)]++;

        if (player_genus(GENPC_DWARVEN))
            you.skills[SK_MACES_FLAILS] = 1;
        else 
            you.skills[SK_SHORT_BLADES] = 1;

        you.skills[SK_STAVES] = 1;
        break;

    case JOB_PRIEST:
        you.piety = 45;

        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_MACE; //jmf: moved from "case 'b'" below
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_POTIONS;
        you.inv[2].sub_type = POT_HEALING;
        you.inv[2].quantity = 2;
        you.inv[2].plus = 0;

        you.equip[EQ_WEAPON] = 0;

        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_SHIELDS] = 1;
        you.skills[SK_MACES_FLAILS] = 2;
        you.skills[SK_STAVES] = 1;

        you.skills[SK_INVOCATIONS] = 4;

        if (Options.priest != GOD_NO_GOD && Options.priest != GOD_RANDOM)
            ng_pr = you.religion = static_cast<god_type>( Options.priest );
        else if (Options.random_pick || Options.priest == GOD_RANDOM)
        {
            you.religion = coinflip() ? GOD_YREDELEMNUL : GOD_ZIN;
            ng_pr = GOD_RANDOM;
        }
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " Which god do you wish to serve?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Zin (for traditional priests)" EOL);
            cprintf("b - Yredelemnul (for priests of death)" EOL);

            if (Options.prev_pr != GOD_NO_GOD)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_pr == GOD_ZIN? "Zin" :
                        Options.prev_pr == GOD_YREDELEMNUL? "Yredelemnul" :
                                            "Random");
            }

          getkey:
            keyn = get_ch();

            if ((keyn == '\r' || keyn == '\n') 
                    && Options.prev_pr != GOD_NO_GOD)
            {
                keyn =  Options.prev_pr == GOD_ZIN? 'a' :
                        Options.prev_pr == GOD_YREDELEMNUL? 'b' :
                                            '?';

            }
            
            switch (keyn)
            {
            case '?':
                you.religion = coinflip()? GOD_ZIN : GOD_YREDELEMNUL;
                break;
            case 'a':
                you.religion = GOD_ZIN;
                break;
            case 'b':
                you.religion = GOD_YREDELEMNUL;
                break;
            default:
                goto getkey;
            }

            ng_pr = keyn == '?'? GOD_RANDOM : you.religion;
        }
        break;

    case JOB_THIEF:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_DAGGER;

        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.inv[3].quantity = 1;
        you.inv[3].base_type = OBJ_ARMOUR;
        you.inv[3].sub_type = ARM_CLOAK;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;

        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 10 );
        you.inv[4].plus = 0;
        you.inv[4].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 2;
        you.equip[EQ_CLOAK] = 3;

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_SHORT_BLADES] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_STEALTH] = 2;
        you.skills[SK_STABBING] = 1;
        you.skills[SK_DODGING + random2(3)]++;
        you.skills[SK_RANGED_COMBAT] = 1;
        you.skills[SK_DARTS] = 1;
        you.skills[SK_TRAPS_DOORS] = 2;
        break;

    case JOB_GLADIATOR:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        choose_weapon();

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        if (player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_SHIELD;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
        }
        else
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_RING_MAIL;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_BUCKLER;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
        }

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;
        you.equip[EQ_SHIELD] = 2;

        you.skills[SK_FIGHTING] = 3;
        weap_skill = 3;

        if (player_genus(GENPC_DRACONIAN))
            you.skills[SK_DODGING] = 2;
        else
            you.skills[SK_ARMOUR] = 2;

        you.skills[SK_SHIELDS] = 1;
        you.skills[SK_UNARMED_COMBAT] = 2;
        break;


    case JOB_NECROMANCER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_NECROMANCY;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;    // = 127
        you.inv[2].special = 0;     // = 1;

        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;
        you.skills[(coinflip()? SK_DODGING : SK_STEALTH)]++;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_NECROMANCY] = 4;
        you.skills[SK_SHORT_BLADES] = 1;
        you.skills[SK_STAVES] = 1;
        break;

    case JOB_PALADIN:
        you.religion = GOD_SHINING_ONE;
        you.piety = 28;

        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_FALCHION;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_SHIELD;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;
        you.equip[EQ_SHIELD] = 2;

        you.inv[3].base_type = OBJ_POTIONS;
        you.inv[3].sub_type = POT_HEALING;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[(coinflip()? SK_ARMOUR : SK_DODGING)]++;
        you.skills[SK_SHIELDS] = 2;
        you.skills[SK_LONG_SWORDS] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_ASSASSIN:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        to_hit_bonus = random2(3);
        you.inv[0].plus = 1 + to_hit_bonus;
        you.inv[0].plus2 = 1 + (2 - to_hit_bonus);
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_BLOWGUN;
        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.inv[3].quantity = 1;
        you.inv[3].base_type = OBJ_ARMOUR;
        you.inv[3].sub_type = ARM_CLOAK;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;

        // deep elves get hand crossbows, everyone else gets blowguns
        // (deep elves tend to suck at melee and need something that
        // can do ranged damage)
        you.inv[4].base_type = OBJ_MISSILES;
        if (you.species == SP_DEEP_ELF)
        {
            you.inv[1].sub_type = WPN_HAND_CROSSBOW;
            you.inv[4].base_type = OBJ_MISSILES;
            you.inv[4].sub_type = MI_DART;
            you.inv[4].quantity = 10 + roll_dice( 2, 10 );
            you.inv[4].plus = 0;
            set_item_ego_type( you.inv[4], OBJ_MISSILES, SPMSL_POISONED );
        }
        else
        {
            you.inv[4].base_type = OBJ_MISSILES;
            you.inv[4].sub_type = MI_NEEDLE;
            you.inv[4].plus = 0;

            you.inv[5] = you.inv[4];
            
            you.inv[4].quantity = 5 + roll_dice(2, 5);
            set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_POISONED);

            you.inv[5].quantity = 1 + random2(4);
            set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_CURARE);
        }

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 2;
        you.equip[EQ_CLOAK] = 3;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_SHORT_BLADES] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 3;
        you.skills[SK_STABBING] = 2;
        you.skills[SK_RANGED_COMBAT] = 1;
        you.skills[SK_DARTS] = 1;
        if (you.species == SP_DEEP_ELF)
            you.skills[SK_CROSSBOWS] = 1;
        else
            you.skills[SK_RANGED_COMBAT] += 1;

        break;

    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        // WEAPONS
        if (you.species == SP_OGRE)
        {
            you.inv[0].quantity = 1;
            you.inv[0].base_type = OBJ_WEAPONS;
            you.inv[0].sub_type = WPN_ANCUS;
            you.inv[0].plus = 0;
            you.inv[0].plus2 = 0;
            you.inv[0].special = 0;
            you.equip[EQ_WEAPON] = 0;
        }
        else if (you.species == SP_TROLL)
        {
            you.equip[EQ_WEAPON] = -1;
        }
        else
        {
            you.inv[0].quantity = 1;
            you.inv[0].base_type = OBJ_WEAPONS;
            you.inv[0].sub_type = WPN_HAND_AXE;
            you.inv[0].plus = 0;
            you.inv[0].plus2 = 0;
            you.inv[0].special = 0;
            you.equip[EQ_WEAPON] = 0;

            for (unsigned char i = 1; i <= 3; i++)
            {
                you.inv[i].quantity = 1;
                you.inv[i].base_type = OBJ_WEAPONS;
                you.inv[i].sub_type = WPN_SPEAR;
                you.inv[i].plus = 0;
                you.inv[i].plus2 = 0;
                you.inv[i].special = 0;
            }
        }

        // ARMOUR

        if (you.species == SP_OGRE || you.species == SP_TROLL
            || player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.equip[EQ_BODY_ARMOUR] = 1;
        }
        else
        {
            you.inv[4].quantity = 1;
            you.inv[4].base_type = OBJ_ARMOUR;
            you.inv[4].sub_type = ARM_LEATHER_ARMOUR;
            you.inv[4].plus = 0;
            you.inv[4].special = 0;
            you.equip[EQ_BODY_ARMOUR] = 4;
        }

        // SKILLS
        you.skills[SK_FIGHTING] = 2;

        if (you.species == SP_TROLL)
        {
            // no wep - give them unarmed.
            you.skills[SK_FIGHTING] += 3;
            you.skills[SK_DODGING] = 2;
            you.skills[SK_UNARMED_COMBAT] = 2;
        }
        else if (you.species == SP_OGRE)
        {
            you.skills[SK_FIGHTING] += 3;
            you.skills[SK_AXES] = 1;
            you.skills[SK_MACES_FLAILS] = 3;
        }
        else
        {
            you.skills[SK_AXES] = 3;
            you.skills[SK_POLEARMS] = 1;
            you.skills[SK_ARMOUR] = 2;
            you.skills[SK_DODGING] = 2;
            you.skills[SK_RANGED_COMBAT] = 2;
        }
        break;

    case JOB_HUNTER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[4].quantity = 1;
        you.inv[4].base_type = OBJ_ARMOUR;
        you.inv[4].sub_type = ARM_LEATHER_ARMOUR;
        you.inv[4].plus = 0;
        you.inv[4].special = 0;

        if (you.species != SP_MERFOLK)
        {
            you.inv[2].quantity = 15 + random2avg(21, 5);
            you.inv[2].base_type = OBJ_MISSILES;
            you.inv[2].sub_type = MI_ARROW;
            you.inv[2].plus = 0;
            you.inv[2].plus2 = 0;
            you.inv[2].special = 0;

            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_WEAPONS;
            you.inv[1].sub_type = WPN_BOW;
            you.inv[1].plus = 0;
            you.inv[1].plus2 = 0;
            you.inv[1].special = 0;
        }
        else
        {
            // Merfolk are spear hunters -- clobber bow, give three spears
            for (unsigned char i = 1; i <= 3; i++)
            {
                you.inv[i].quantity = 1;
                you.inv[i].base_type = OBJ_WEAPONS;
                you.inv[i].sub_type = WPN_SPEAR;
                you.inv[i].plus = 0;
                you.inv[i].plus2 = 0;
                you.inv[i].special = 0;
            }
        }

        if (player_genus(GENPC_DRACONIAN))
            you.inv[4].sub_type = ARM_ROBE;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 4;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_RANGED_COMBAT] = 3;

        // Removing spellcasting -- bwr
        // you.skills[SK_SPELLCASTING] = 1;

        switch (you.species)
        {
        case SP_HALFLING:
        case SP_GNOME:
            you.inv[2].quantity += random2avg(15, 2);
            you.inv[2].sub_type = MI_STONE;
            you.inv[1].sub_type = WPN_SLING;

            you.skills[SK_DODGING] = 2;
            you.skills[SK_STEALTH] = 2;
            you.skills[SK_SLINGS] = 2;
            break;

        case SP_HILL_DWARF:
        case SP_MOUNTAIN_DWARF:
        case SP_HILL_ORC:
            you.inv[2].sub_type = MI_BOLT;
            you.inv[1].sub_type = WPN_CROSSBOW;

            if (you.species == SP_HILL_ORC)
            {
                you.inv[0].sub_type = WPN_SHORT_SWORD;
                you.skills[SK_SHORT_BLADES] = 1;
            }
            else
            {
                you.inv[0].sub_type = WPN_HAND_AXE;
                you.skills[SK_AXES] = 1;
            }

            you.skills[SK_DODGING] = 1;
            you.skills[SK_SHIELDS] = 1;
            you.skills[SK_CROSSBOWS] = 2;
            break;

        case SP_MERFOLK:
            you.inv[0].sub_type = WPN_TRIDENT;

            you.skills[SK_POLEARMS] = 2;
            you.skills[SK_DODGING] = 2;
            you.skills[SK_RANGED_COMBAT] += 1;
            break;

        default:
            you.skills[SK_DODGING] = 1;
            you.skills[SK_STEALTH] = 1;
            you.skills[(coinflip() ? SK_STABBING : SK_SHIELDS)]++;
            you.skills[SK_BOWS] = 2;
            break;
        }
        break;

    case JOB_CONJURER:
    case JOB_ENCHANTER:
    case JOB_SUMMONER:
    case JOB_FIRE_ELEMENTALIST:
    case JOB_ICE_ELEMENTALIST:
    case JOB_AIR_ELEMENTALIST:
    case JOB_EARTH_ELEMENTALIST:
    case JOB_VENOM_MAGE:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;

        if (you.char_class == JOB_ENCHANTER)
        {
            you.inv[0].plus = 1;
            you.inv[0].plus2 = 1;
            you.inv[1].plus = 1;
        }

        you.inv[1].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        if ( you.char_class == JOB_CONJURER )
            choose_book( you.inv[2], BOOK_CONJURATIONS_I, 2 );
        else
        {
            you.inv[2].base_type = OBJ_BOOKS;
            // subtype will always be overridden
            you.inv[2].plus = 0;
        }

        switch (you.char_class)
        {
        case JOB_SUMMONER:
            you.inv[2].sub_type = BOOK_CALLINGS;

            you.skills[SK_SUMMONINGS] = 4;

            // gets some darts - this class is difficult to start off with
            you.inv[3].base_type = OBJ_MISSILES;
            you.inv[3].sub_type = MI_DART;
            you.inv[3].quantity = 8 + roll_dice( 2, 8 );
            you.inv[3].plus = 0;
            you.inv[3].special = 0;
            break;

        case JOB_CONJURER:
            you.skills[SK_CONJURATIONS] = 4;
            break;

        case JOB_ENCHANTER:
            you.inv[2].sub_type = BOOK_CHARMS;

            you.skills[SK_ENCHANTMENTS] = 4;

            // gets some darts - this class is difficult to start off with
            you.inv[3].base_type = OBJ_MISSILES;
            you.inv[3].sub_type = MI_DART;
            you.inv[3].quantity = 8 + roll_dice( 2, 8 );
            you.inv[3].plus = 1;
            you.inv[3].special = 0;

            if (you.species == SP_SPRIGGAN)
                make_rod(you.inv[0], STAFF_STRIKING);
            break;

        case JOB_FIRE_ELEMENTALIST:
            you.inv[2].sub_type = BOOK_FLAMES;

            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_FIRE_MAGIC] = 3;
            //you.skills [SK_ENCHANTMENTS] = 1;
            break;

        case JOB_ICE_ELEMENTALIST:
            you.inv[2].sub_type = BOOK_FROST;

            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_ICE_MAGIC] = 3;
            //you.skills [SK_ENCHANTMENTS] = 1;
            break;

        case JOB_AIR_ELEMENTALIST:
            you.inv[2].sub_type = BOOK_AIR;

            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_AIR_MAGIC] = 3;
            //you.skills [SK_ENCHANTMENTS] = 1;
            break;

        case JOB_EARTH_ELEMENTALIST:
            you.inv[2].sub_type = BOOK_GEOMANCY;

            you.inv[3].quantity = random2avg(12, 2) + 6;
            you.inv[3].base_type = OBJ_MISSILES;
            you.inv[3].sub_type = MI_STONE;
            you.inv[3].plus = 0;
            you.inv[3].plus2 = 0;
            you.inv[3].special = 0;

            if (you.species == SP_GNOME)
            {
                you.inv[1].quantity = 1;
                you.inv[1].base_type = OBJ_WEAPONS;
                you.inv[1].sub_type = WPN_SLING;
                you.inv[1].plus = 0;
                you.inv[1].plus2 = 0;
                you.inv[1].special = 0;

                you.inv[4].quantity = 1;
                you.inv[4].base_type = OBJ_ARMOUR;
                you.inv[4].sub_type = ARM_ROBE;
                you.inv[4].plus = 0;
                you.inv[4].plus2 = 0;
                you.inv[4].special = 0;
                you.equip[EQ_BODY_ARMOUR] = 4;

            }
            you.skills[SK_TRANSMIGRATION] = 1;
            you.skills[SK_EARTH_MAGIC] = 3;
            break;

        case JOB_VENOM_MAGE:
            you.inv[2].sub_type = BOOK_YOUNG_POISONERS;
            you.skills[SK_POISON_MAGIC] = 4;
            break;
        }

        if (you.species == SP_OGRE_MAGE)
            you.inv[0].sub_type = WPN_QUARTERSTAFF;
        else if (player_genus(GENPC_DWARVEN))
            you.inv[0].sub_type = WPN_HAMMER;

        you.inv[2].quantity = 1;
        you.inv[2].special = 0;

        you.skills[SK_SPELLCASTING] = 1;

        // These summoner races start with polearms and should they
        // get their hands on a polearm of reaching they should have
        // lots of fun... -- bwr
        if (you.char_class == JOB_SUMMONER
            && (you.species == SP_MERFOLK || you.species == SP_HILL_ORC ||
                you.species == SP_KENKU || you.species == SP_MINOTAUR))
        {
            if (you.species == SP_MERFOLK)
                you.inv[0].sub_type = WPN_TRIDENT;
            else
                you.inv[0].sub_type = WPN_SPEAR;

            you.skills[SK_POLEARMS] = 1;
        }
        else if (player_genus(GENPC_DWARVEN))
        {
            you.skills[SK_MACES_FLAILS] = 1;
        }
        else if (you.char_class == JOB_ENCHANTER && you.species == SP_SPRIGGAN)
        {
            you.skills[SK_EVOCATIONS] = 1;
        }
        else
        {
            you.skills[SK_SHORT_BLADES] = 1;
        }

        if (you.species == SP_GNOME)
            you.skills[SK_SLINGS]++;
        else
            you.skills[SK_STAVES]++;

        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;

        if (you.species == SP_GNOME && you.char_class == JOB_EARTH_ELEMENTALIST)
            you.skills[SK_RANGED_COMBAT]++;
        else
            you.skills[ coinflip() ? SK_DODGING : SK_STEALTH ]++;
        break;

    case JOB_TRANSMUTER:
        // some sticks for sticks to snakes:
        you.inv[1].quantity = 6 + roll_dice( 3, 4 );
        you.inv[1].base_type = OBJ_MISSILES;
        you.inv[1].sub_type = MI_ARROW;
        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].quantity = 1;

        you.inv[3].base_type = OBJ_BOOKS;
        you.inv[3].sub_type = BOOK_CHANGES;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;

        // A little bit of starting ammo for evaporate... don't need too
        // much now that the character can make their own. -- bwr
        //
        // some ammo for evaporate:
        you.inv[4].base_type = OBJ_POTIONS;
        you.inv[4].sub_type = POT_CONFUSION;
        you.inv[4].quantity = 2;
        you.inv[4].plus = 0;

        // some more ammo for evaporate:
        you.inv[5].base_type = OBJ_POTIONS;
        you.inv[5].sub_type = POT_POISON;
        you.inv[5].quantity = 1;
        you.inv[5].plus = 0;

        you.equip[EQ_WEAPON] = -1;
        you.equip[EQ_BODY_ARMOUR] = 2;

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_RANGED_COMBAT] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_TRANSMIGRATION] = 2;

        if (you.species == SP_SPRIGGAN)
        {
            make_rod(you.inv[0], STAFF_STRIKING);

            you.skills[SK_EVOCATIONS] = 2;
            you.skills[SK_FIGHTING] = 0;

            you.equip[EQ_WEAPON] = 0;
        }
        break;

    case JOB_WARPER:
        you.inv[0].quantity = 1;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        if (you.species == SP_SPRIGGAN)
        {
            make_rod(you.inv[0], STAFF_STRIKING);

            you.skills[SK_EVOCATIONS] = 3;
        }
        else
        {
            you.inv[0].base_type = OBJ_WEAPONS;
            you.inv[0].sub_type = WPN_SHORT_SWORD;

            if (you.species == SP_OGRE_MAGE)
                you.inv[0].sub_type = WPN_QUARTERSTAFF;

            weap_skill = 2;
            you.skills[SK_FIGHTING] = 1;
        }

        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_LEATHER_ARMOUR;
        you.inv[1].quantity = 1;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        if (you.species == SP_SPRIGGAN || you.species == SP_OGRE_MAGE)
            you.inv[1].sub_type = ARM_ROBE;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_SPATIAL_TRANSLOCATIONS;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        // one free escape:
        you.inv[3].base_type = OBJ_SCROLLS;
        you.inv[3].sub_type = SCR_BLINKING;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;

        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 10 );
        you.inv[4].plus = 0;
        you.inv[4].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_RANGED_COMBAT] = 1;
        you.skills[SK_DARTS] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_STEALTH] = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_TRANSLOCATIONS] = 2;
        break;

    case JOB_CRUSADER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;

        //if (you.species == SP_OGRE_MAGE) you.inv_sub_type [0] = WPN_GLAIVE;

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        choose_weapon();
        weap_skill = 2;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_WAR_CHANTS;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_ENCHANTMENTS] = 2;
        break;


    case JOB_DEATH_KNIGHT:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        choose_weapon();
        weap_skill = 2;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_NECROMANCY;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        choice = DK_NO_SELECTION;

        // order is important here -- bwr
        if (you.species == SP_DEMIGOD)
            choice = DK_NECROMANCY;
        else if (Options.death_knight != DK_NO_SELECTION
                && Options.death_knight != DK_RANDOM)
        {
            ng_dk = choice = Options.death_knight;
        }
        else if (Options.random_pick || Options.death_knight == DK_RANDOM)
        {
            choice = (coinflip() ? DK_NECROMANCY : DK_YREDELEMNUL);
            ng_dk  = DK_RANDOM;
        }
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " From where do you draw your power?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Necromantic magic" EOL);
            cprintf("b - the god Yredelemnul" EOL);

            if (Options.prev_dk != DK_NO_SELECTION)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL, 
                        Options.prev_dk == DK_NECROMANCY? "Necromancy" :
                        Options.prev_dk == DK_YREDELEMNUL? "Yredelemnul" :
                                                           "Random");
            }

          getkey1:
            keyn = get_ch();

            if ((keyn == '\r' || keyn == '\n')
                    && Options.prev_dk != DK_NO_SELECTION)
            {
                keyn = Options.prev_dk == DK_NECROMANCY? 'a' :
                       Options.prev_dk == DK_YREDELEMNUL? 'b' :
                                                          '?';
            }
            
            switch (keyn)
            {
            case '?':
                choice = coinflip()? DK_NECROMANCY : DK_YREDELEMNUL;
                break;
            case 'a':
                cprintf(EOL "Very well.");
                choice = DK_NECROMANCY;
                break;
            case 'b':
                choice = DK_YREDELEMNUL;
                break;
            default:
                goto getkey1;
            }

            ng_dk = keyn == '?'? DK_RANDOM : choice;
        }

        switch (choice)
        {
        default:  // this shouldn't happen anyways -- bwr
        case DK_NECROMANCY:
            you.skills[SK_SPELLCASTING] = 1;
            you.skills[SK_NECROMANCY] = 2;
            break;
        case DK_YREDELEMNUL:
            you.religion = GOD_YREDELEMNUL;
            you.piety = 28;
            you.inv[0].plus = 1;
            you.inv[0].plus2 = 1;
            you.inv[2].quantity = 0;
            you.skills[SK_INVOCATIONS] = 3;
            break;
        }

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;
        //you.skills [SK_SHORT_BLADES] = 2;
        you.skills[SK_STABBING] = 1;
        break;

    case JOB_CHAOS_KNIGHT:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        you.inv[0].plus = random2(3);
        you.inv[0].plus2 = random2(3);
        you.inv[0].special = 0;

        if (one_chance_in(5))
            set_equip_desc( you.inv[0], ISFLAG_RUNED );

        if (one_chance_in(5))
            set_equip_desc( you.inv[0], ISFLAG_GLOWING );

        choose_weapon();
        weap_skill = 2;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = random2(3);
        you.inv[1].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[(coinflip()? SK_ARMOUR : SK_DODGING)]++;
        you.skills[SK_STABBING] = 1;

        if (Options.chaos_knight != GOD_NO_GOD
            && Options.chaos_knight != GOD_RANDOM)
        {
            ng_ck = you.religion =
                static_cast<god_type>( Options.chaos_knight );
        }
        else if (Options.random_pick || Options.chaos_knight == GOD_RANDOM)
        {
            you.religion = coinflip() ? GOD_XOM : GOD_MAKHLEB;
            ng_ck = GOD_RANDOM;
        }
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " Which god of chaos do you wish to serve?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Xom of Chaos" EOL);
            cprintf("b - Makhleb the Destroyer" EOL);

            if (Options.prev_ck != GOD_NO_GOD)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_ck == GOD_XOM? "Xom" :
                        Options.prev_ck == GOD_MAKHLEB? "Makhleb" :
                                                    "Random");
                textcolor(LIGHTGREY);
            }

          getkey2:

            keyn = get_ch();

            if ((keyn == '\r' || keyn == '\n') 
                    && Options.prev_ck != GOD_NO_GOD)
            {
                keyn = Options.prev_ck == GOD_XOM? 'a' :
                       Options.prev_ck == GOD_MAKHLEB? 'b' :
                                                    '?';
            }

            switch (keyn)
            {
            case '?':
                you.religion = coinflip()? GOD_XOM : GOD_MAKHLEB;
                break;
            case 'a':
                you.religion = GOD_XOM;
                break;
            case 'b':
                you.religion = GOD_MAKHLEB;
                break;
            default:
                goto getkey2;
            }

            ng_ck = keyn == '?'? GOD_RANDOM : you.religion;
        }

        if (you.religion == GOD_XOM)
        {
            you.skills[SK_FIGHTING]++;
            // the new (piety-aware) Xom uses piety in his own special way...
            // (namely, 100 is neutral)
            you.piety = 100;
            // the new (piety-aware) Xom uses gift_timeout in his own special
            // way... (namely, a countdown to becoming bored)
            you.gift_timeout = random2(40) + random2(40);
        }
        else // Makhleb
        {
            you.piety = 25;
            you.skills[SK_INVOCATIONS] = 2;
        }

        break;

    case JOB_HEALER:
        you.religion = GOD_ELYVILON;
        you.piety = 45;

        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_QUARTERSTAFF;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        // Robe
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_POTIONS;
        you.inv[2].sub_type = POT_HEALING;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;

        you.inv[3].base_type = OBJ_POTIONS;
        you.inv[3].sub_type = POT_HEAL_WOUNDS;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_SHIELDS] = 1;
        you.skills[SK_RANGED_COMBAT] = 2;
        you.skills[SK_STAVES] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_REAVER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        choose_weapon();
        weap_skill = 3;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = give_first_conjuration_book();
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;    // = 127
        you.inv[2].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;

        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_CONJURATIONS] = 2;
        break;

    case JOB_STALKER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        to_hit_bonus = random2(3);
        you.inv[0].plus = 1 + to_hit_bonus;
        you.inv[0].plus2 = 1 + (2 - to_hit_bonus);
        you.inv[0].special = 0;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_CLOAK;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;

        you.inv[3].base_type = OBJ_BOOKS;
        //you.inv[3].sub_type = BOOK_YOUNG_POISONERS;
        you.inv[3].sub_type = BOOK_STALKING;   //jmf: new book!
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;
        you.equip[EQ_CLOAK] = 2;

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_SHORT_BLADES] = 1;
        you.skills[SK_POISON_MAGIC] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 2;
        you.skills[SK_STABBING] = 2;
        you.skills[SK_DODGING + random2(3)]++;
        //you.skills[SK_RANGED_COMBAT] = 1; //jmf: removed these, added magic below
        //you.skills[SK_DARTS] = 1;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_ENCHANTMENTS] = 1;
        break;

    case JOB_MONK:
        you.inv[0].base_type = OBJ_ARMOUR;
        you.inv[0].sub_type = ARM_ROBE;
        you.inv[0].plus = 0;
        you.inv[0].special = 0;
        you.inv[0].quantity = 1;

        you.equip[EQ_WEAPON] = -1;
        you.equip[EQ_BODY_ARMOUR] = 0;

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_UNARMED_COMBAT] = 4;
        you.skills[SK_DODGING] = 3;
        you.skills[SK_STEALTH] = 2;
        break;

    case JOB_WANDERER:
        create_wanderer();
        break;
    }

    if (weap_skill)
        you.skills[weapon_skill(OBJ_WEAPONS, you.inv[0].sub_type)] = weap_skill;

    init_skill_order();

    if (you.religion != GOD_NO_GOD)
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
    }
}

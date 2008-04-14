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
 *                                    Halflings can be assassins and
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
#include "cio.h"
#include "command.h"
#include "describe.h"
#include "files.h"
#include "fight.h"
#include "food.h"
#include "initfile.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "makeitem.h"
#include "menu.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "randart.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "tutorial.h"
#include "version.h"
#include "view.h"

extern std::string init_file_location;

#define MIN_START_STAT       1

static bool _class_allowed(species_type speci, job_type char_class);
static bool _validate_player_name(bool verbose);
static bool _choose_weapon(void);
static void _enter_player_name(bool blankOK);
static void _give_basic_knowledge(job_type which_job);
static void _give_basic_spells(job_type which_job);
static void _give_basic_mutations(species_type speci);
static void _give_last_paycheck(job_type which_job);
static void _init_player(void);
static void _jobs_stat_init(job_type which_job);
static void _opening_screen(void);
static void _species_stat_init(species_type which_species);

static void _give_random_potion( int slot );
static void _give_random_secondary_armour( int slot );
static bool _give_wanderer_weapon( int slot, int wpn_skill );
static void _create_wanderer(void);
static bool _give_items_skills(void);

////////////////////////////////////////////////////////////////////////
// Remember player's startup options
//

static char ng_race, ng_cls;
static bool ng_random;
static int  ng_ck, ng_dk;
static int  ng_weapon;
static int  ng_book;
static god_type ng_pr;

/* March 2008: change order of species and jobs on character selection screen
   as suggested by Markus Maier. Summarizing comments below are copied directly
   from Markus' SourceForge comments. */

// listed in two columns to match the selection screen output
// Take care to list all valid species here, or they cannot be directly chosen.
// The red draconian is later replaced by a random variant.
// The old and new lists are expected to have the same length.
static species_type old_species_order[] = {
    SP_HUMAN,       SP_HIGH_ELF,
    SP_GREY_ELF,    SP_DEEP_ELF,
    SP_SLUDGE_ELF,  SP_MOUNTAIN_DWARF,
    SP_HALFLING,    SP_HILL_ORC,
    SP_KOBOLD,      SP_MUMMY,
    SP_NAGA,        SP_GNOME,
    SP_OGRE,        SP_TROLL,
    SP_OGRE_MAGE,   SP_RED_DRACONIAN,
    SP_CENTAUR,     SP_DEMIGOD,
    SP_SPRIGGAN,    SP_MINOTAUR,
    SP_DEMONSPAWN,  SP_GHOUL,
    SP_KENKU,       SP_MERFOLK,
    SP_VAMPIRE
};

// Fantasy staples and humanoid creatures come first, then dimunitive and
// stealthy creatures, then monstrous creatures, then planetouched and after
// all living creatures finally the undead. (MM)
static species_type new_species_order[] = {
    // comparatively human-like looks
    SP_HUMAN,       SP_HIGH_ELF,
    SP_GREY_ELF,    SP_DEEP_ELF,
    SP_SLUDGE_ELF,  SP_MOUNTAIN_DWARF,
    SP_HILL_ORC,    SP_MERFOLK,
    // small species
    SP_HALFLING,    SP_GNOME,
    SP_KOBOLD,      SP_SPRIGGAN,
    // significantly different body type than human
    SP_NAGA,        SP_CENTAUR,
    SP_OGRE,        SP_OGRE_MAGE,
    SP_TROLL,       SP_MINOTAUR,
    SP_KENKU,       SP_RED_DRACONIAN,
    // celestial species
    SP_DEMIGOD,     SP_DEMONSPAWN,
    // undead species
    SP_MUMMY,       SP_GHOUL,
    SP_VAMPIRE
};

static species_type _random_draconian_species()
{
    const int num_drac = SP_PALE_DRACONIAN - SP_RED_DRACONIAN;
    return static_cast<species_type>(SP_RED_DRACONIAN + random2(num_drac));
}

static species_type _get_species(const int index)
{
    if (index < 0 || (unsigned int) index >= ARRAYSIZE(old_species_order))
    {
        return (SP_UNKNOWN);
    }

    return (Options.use_old_selection_order ? old_species_order[index]
                                            : new_species_order[index]);
}

// listed in two columns to match the selection screen output
// Take care to list all valid classes here, or they cannot be directly chosen.
// The old and new lists are expected to have the same length.
static job_type old_jobs_order[] = {
    JOB_FIGHTER,            JOB_WIZARD,
    JOB_PRIEST,             JOB_THIEF,
    JOB_GLADIATOR,          JOB_NECROMANCER,
    JOB_PALADIN,            JOB_ASSASSIN,
    JOB_BERSERKER,          JOB_HUNTER,
    JOB_CONJURER,           JOB_ENCHANTER,
    JOB_FIRE_ELEMENTALIST,  JOB_ICE_ELEMENTALIST,
    JOB_SUMMONER,           JOB_AIR_ELEMENTALIST,
    JOB_EARTH_ELEMENTALIST, JOB_CRUSADER,
    JOB_DEATH_KNIGHT,       JOB_VENOM_MAGE,
    JOB_CHAOS_KNIGHT,       JOB_TRANSMUTER,
    JOB_HEALER,             JOB_REAVER,
    JOB_STALKER,            JOB_MONK,
    JOB_WARPER,             JOB_WANDERER
};

// First plain fighters, then religious fighters, then spell-casting
// fighters, then primary spell-casters, then stabbers and shooters. (MM)
static job_type new_jobs_order[] = {
    // fighters
    JOB_FIGHTER,            JOB_GLADIATOR,
    JOB_MONK,               JOB_BERSERKER,
    // religious professions (incl. Berserker above)
    JOB_PALADIN,            JOB_PRIEST,
    JOB_CHAOS_KNIGHT,       JOB_DEATH_KNIGHT,
    JOB_HEALER,             JOB_CRUSADER,
    // general and niche spellcasters (incl. Crusader above)
    JOB_REAVER,             JOB_WIZARD,
    JOB_CONJURER,           JOB_ENCHANTER,
    JOB_SUMMONER,           JOB_NECROMANCER,
    JOB_WARPER,             JOB_TRANSMUTER,
    JOB_FIRE_ELEMENTALIST,  JOB_ICE_ELEMENTALIST,
    JOB_AIR_ELEMENTALIST,   JOB_EARTH_ELEMENTALIST,
    // poison specialists and stabbers
    JOB_VENOM_MAGE,         JOB_STALKER,
    JOB_THIEF,              JOB_ASSASSIN,
    JOB_HUNTER,             JOB_WANDERER
};

static job_type _get_class(const int index)
{
    if (index < 0 || (unsigned int) index >= ARRAYSIZE(old_jobs_order))
       return JOB_UNKNOWN;

    return (Options.use_old_selection_order? old_jobs_order[index]
                                           : new_jobs_order[index]);
}

static const char * Species_Abbrev_List[ NUM_SPECIES ] =
    { "XX", "Hu", "HE", "GE", "DE", "SE", "MD", "Ha",
      "HO", "Ko", "Mu", "Na", "Gn", "Og", "Tr", "OM",
      // the draconians
      "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr", "Dr",
      "Ce", "DG", "Sp", "Mi", "DS", "Gh", "Ke", "Mf", "Vp",
      // placeholders
      "HD", "El" };

int get_species_index_by_abbrev( const char *abbrev )
{
    COMPILE_CHECK(ARRAYSIZE(Species_Abbrev_List) == NUM_SPECIES, c1);

    for (unsigned i = 0; i < ARRAYSIZE(old_species_order); i++)
    {
        const int sp = (Options.use_old_selection_order ? old_species_order[i]
                                                        : new_species_order[i]);

        if (tolower( abbrev[0] ) == tolower( Species_Abbrev_List[sp][0] )
            && tolower( abbrev[1] ) == tolower( Species_Abbrev_List[sp][1] ))
        {
            return i;
        }
    }

    return (-1);
}

int get_species_index_by_name( const char *name )
{
    unsigned int i;
    int sp = -1;

    std::string::size_type pos = std::string::npos;
    char lowered_buff[80];

    strncpy( lowered_buff, name, sizeof( lowered_buff ) );
    strlwr( lowered_buff );

    for (i = 0; i < ARRAYSIZE(old_species_order); i++)
    {
        const species_type real_sp
                   = (Options.use_old_selection_order ? old_species_order[i]
                                                      : new_species_order[i]);

        const std::string lowered_species =
            lowercase_string(species_name(real_sp,1));
        pos = lowered_species.find( lowered_buff );
        if (pos != std::string::npos)
        {
            sp = i;
            if (pos == 0)  // prefix takes preference
                break;
        }
    }

    return (sp);
}

const char *get_species_abbrev( int which_species )
{
    ASSERT( which_species > 0 && which_species < NUM_SPECIES );

    return (Species_Abbrev_List[ which_species ]);
}

// needed for debug.cc and hiscores.cc
int get_species_by_abbrev( const char *abbrev )
{
    int i;
    COMPILE_CHECK(ARRAYSIZE(Species_Abbrev_List) == NUM_SPECIES, c1);
    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
         if (tolower( abbrev[0] ) == tolower( Species_Abbrev_List[i][0] )
             && tolower( abbrev[1] ) == tolower( Species_Abbrev_List[i][1] ))
         {
             break;
         }
    }

    return ((i < NUM_SPECIES) ? i : -1);
}

static const char * Class_Abbrev_List[ NUM_JOBS ] =
    { "Fi", "Wz", "Pr", "Th", "Gl", "Ne", "Pa", "As", "Be", "Hu",
      "Cj", "En", "FE", "IE", "Su", "AE", "EE", "Cr", "DK", "VM",
      "CK", "Tm", "He", "Re", "St", "Mo", "Wr", "Wn" };

static const char * Class_Name_List[ NUM_JOBS ] =
    { "Fighter", "Wizard", "Priest", "Thief", "Gladiator", "Necromancer",
      "Paladin", "Assassin", "Berserker", "Hunter", "Conjurer", "Enchanter",
      "Fire Elementalist", "Ice Elementalist", "Summoner", "Air Elementalist",
      "Earth Elementalist", "Crusader", "Death Knight", "Venom Mage",
      "Chaos Knight", "Transmuter", "Healer", "Reaver", "Stalker",
      "Monk", "Warper", "Wanderer" };

int get_class_index_by_abbrev( const char *abbrev )
{
    COMPILE_CHECK(ARRAYSIZE(Class_Abbrev_List) == NUM_JOBS, c1);

    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSIZE(old_jobs_order); i++)
    {
        job = (Options.use_old_selection_order ? old_jobs_order[i]
                                               : new_jobs_order[i]);

        if (tolower( abbrev[0] ) == tolower( Class_Abbrev_List[job][0] )
            && tolower( abbrev[1] ) == tolower( Class_Abbrev_List[job][1] ))
        {
            return i;
        }
    }

    return (-1);
}

const char *get_class_abbrev( int which_job )
{
    ASSERT( which_job < NUM_JOBS );

    return (Class_Abbrev_List[ which_job ]);
}

int get_class_by_abbrev( const char *abbrev )
{
    int i;

    for (i = 0; i < NUM_JOBS; i++)
    {
        if (tolower( abbrev[0] ) == tolower( Class_Abbrev_List[i][0] )
            && tolower( abbrev[1] ) == tolower( Class_Abbrev_List[i][1] ))
        {
            break;
        }
    }

    return ((i < NUM_JOBS) ? i : -1);
}

int get_class_index_by_name( const char *name )
{
    COMPILE_CHECK(ARRAYSIZE(Class_Name_List)   == NUM_JOBS, c1);

    char *ptr;
    char lowered_buff[80];
    char lowered_class[80];

    strncpy( lowered_buff, name, sizeof( lowered_buff ) );
    strlwr( lowered_buff );

    int cl = -1;
    unsigned int job;
    for (unsigned int i = 0; i < ARRAYSIZE(old_jobs_order); i++)
    {
        job = (Options.use_old_selection_order ? old_jobs_order[i]
                                               : new_jobs_order[i]);

        strncpy( lowered_class, Class_Name_List[job], sizeof( lowered_class ) );
        strlwr( lowered_class );

        ptr = strstr( lowered_class, lowered_buff );
        if (ptr != NULL)
        {
            cl = i;
            if (ptr == lowered_class)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

const char *get_class_name( int which_job )
{
    ASSERT( which_job < NUM_JOBS );

    return (Class_Name_List[ which_job ]);
}

int get_class_by_name( const char *name )
{
    int i;
    int cl = -1;

    char *ptr;
    char lowered_buff[80];
    char lowered_class[80];

    strncpy( lowered_buff, name, sizeof( lowered_buff ) );
    strlwr( lowered_buff );

    for (i = 0; i < NUM_JOBS; i++)
    {
        strncpy( lowered_class, Class_Name_List[i], sizeof( lowered_class ) );
        strlwr( lowered_class );

        ptr = strstr( lowered_class, lowered_buff );
        if (ptr != NULL)
        {
            cl = i;
            if (ptr == lowered_class)  // prefix takes preference
                break;
        }
    }

    return (cl);
}

static void _reset_newgame_options(void)
{
    ng_race   = ng_cls = 0;
    ng_random = false;
    ng_ck     = GOD_NO_GOD;
    ng_dk     = DK_NO_SELECTION;
    ng_pr     = GOD_NO_GOD;
    ng_weapon = WPN_UNKNOWN;
    ng_book   = SBT_NO_SELECTION;
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
}

static bool _prev_startup_options_set(void)
{
    // Are these two enough? They should be, in theory, since we'll
    // remember the player's weapon and god choices.
    return (Options.prev_race && Options.prev_cls);
}

static std::string _get_opt_race_name(char race)
{
    species_type prace = _get_species(letter_to_index(race));
    return (prace == SP_UNKNOWN? "Random" : species_name(prace, 1));
}

static std::string _get_opt_class_name(char oclass)
{
    job_type pclass = _get_class(letter_to_index(oclass));
    return (pclass == JOB_UNKNOWN? "Random" : get_class_name(pclass));
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
    if (strlen(you.your_name) > 0
        && you.char_class != JOB_UNKNOWN && you.species != SP_UNKNOWN)
    {
        cprintf("Welcome, ");
        textcolor( YELLOW );
        cprintf("%s the %s %s." EOL, you.your_name, species_name(you.species, 1).c_str(),
                get_class_name(you.char_class));
    }
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

// Determines if a species is valid. If 'display' is true, returns if
// the species is displayable in the new game screen - this is
// primarily used to suppress the display of the draconian variants.
static bool _is_species_valid_choice(species_type species, bool display = true)
{
    if (!species) // species only start at 1
        return (false);

    if (species >= SP_ELF) // these are all invalid
        return (false);

    // no problem with these
    if (species <= SP_RED_DRACONIAN || species > SP_BASE_DRACONIAN)
        return (true);

    // draconians other than red return false if display == true
    return (!display);
}

static void _pick_random_species_and_class( void )
{
    //
    // We pick both species and class at the same time to give each
    // valid possibility a fair chance.  For proof that this will
    // work correctly see the proof in religion.cc:handle_god_time().
    //
    int job_count = 0;

    species_type species = SP_UNKNOWN;
    job_type job = JOB_UNKNOWN;

    // for each valid (species, class) choose one randomly
    for (int sp = SP_HUMAN; sp < NUM_SPECIES; sp++)
    {
        // we only want draconians counted once in this loop...
        // we'll add the variety lower down -- bwr
        if (!_is_species_valid_choice(static_cast<species_type>(sp)))
            continue;

        for (int cl = JOB_FIGHTER; cl < NUM_JOBS; cl++)
        {
            if (_class_allowed(static_cast<species_type>(sp),
                               static_cast<job_type>(cl)))
            {
                job_count++;
                if (one_chance_in( job_count ))
                {
                    species = static_cast<species_type>(sp);
                    job = static_cast<job_type>(cl);
                }
            }
        }
    }

    // at least one job must exist in the game else we're in big trouble
    ASSERT( species != SP_UNKNOWN && job != JOB_UNKNOWN );

    // return draconian variety here
    if (species == SP_RED_DRACONIAN)
        you.species = _random_draconian_species();
    else
        you.species = species;

    you.char_class = job;
}

static bool _check_saved_game(void)
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

static unsigned char _random_potion_description()
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
    }
    while ((colour == PDC_CLEAR && nature > PDQ_VISCOUS)
           || desc == PDESCS(PDC_CLEAR)
           || desc == PDESCQ(PDQ_GLUGGY, PDC_WHITE));

    return static_cast<unsigned char>(desc);
}

// Determine starting depths of branches
static void _initialise_branch_depths()
{
    branches[BRANCH_ECUMENICAL_TEMPLE].startdepth = random_range(4, 7);
    branches[BRANCH_ORCISH_MINES].startdepth      = random_range(6, 11);
    branches[BRANCH_ELVEN_HALLS].startdepth       = random_range(3, 4);
    branches[BRANCH_LAIR].startdepth              = random_range(8, 13);
    branches[BRANCH_HIVE].startdepth              = random_range(11, 16);
    branches[BRANCH_SLIME_PITS].startdepth        = random_range(8, 10);
    if ( coinflip() )
    {
        branches[BRANCH_SWAMP].startdepth  = random_range(2, 7);
        branches[BRANCH_SHOALS].startdepth = -1;
    }
    else
    {
        branches[BRANCH_SWAMP].startdepth  = -1;
        branches[BRANCH_SHOALS].startdepth = random_range(2, 7);
    }
    branches[BRANCH_SNAKE_PIT].startdepth      = random_range(3, 8);
    branches[BRANCH_VAULTS].startdepth         = random_range(14, 19);
    branches[BRANCH_CRYPT].startdepth          = random_range(2, 4);
    branches[BRANCH_HALL_OF_BLADES].startdepth = random_range(4, 6);
    branches[BRANCH_TOMB].startdepth           = random_range(2, 3);
}

static int _get_random_coagulated_blood_desc()
{
    potion_description_qualifier_type qualifier = PDQ_NONE;
    switch (random2(4))
    {
    case 0:
        qualifier = PDQ_GLUGGY;
        break;
    case 1:
        qualifier = PDQ_LUMPY;
        break;
    case 2:
        qualifier = PDQ_SEDIMENTED;
        break;
    case 3:
        qualifier = PDQ_VISCOUS;
        break;
    }

    potion_description_colour_type colour = (coinflip() ? PDC_RED : PDC_BROWN);

    return PDESCQ(qualifier, colour);
}

static void _initialise_item_descriptions()
{
    // must remember to check for already existing colours/combinations
    you.item_description.init(255);

    you.item_description[IDESC_POTIONS][POT_PORRIDGE]
        = PDESCQ(PDQ_GLUGGY, PDC_WHITE);

    you.item_description[IDESC_POTIONS][POT_WATER] = PDESCS(PDC_CLEAR);
    you.item_description[IDESC_POTIONS][POT_BLOOD] = PDESCS(PDC_RED);
    you.item_description[IDESC_POTIONS][POT_BLOOD_COAGULATED]
         = _get_random_coagulated_blood_desc();

    // The order here must match that of IDESC in describe.h
    // (I don't really know about scrolls, which is why I left the height value.)
    const int max_item_number[6] = { NUM_WANDS, NUM_POTIONS,
                                     you.item_description.height(),
                                     NUM_JEWELLERY,
                                     you.item_description.height(),
                                     NUM_STAVES };

    for (int i = 0; i < NUM_IDESC; i++)
    {
        // only loop until NUM_WANDS etc.
        for (int j = 0; j < max_item_number[i]; j++)
        {
            // Don't override predefines
            if (you.item_description[i][j] != 255)
                continue;

            // pick a new description until it's good
            while (true)
            {
                // The numbers below are always secondary * primary (itemname.cc)
                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2( 16 * 12 );
                    if (coinflip())
                        you.item_description[i][j] %= 12;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = _random_potion_description();
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

                case IDESC_STAVES: // staves and rods
                    you.item_description[i][j] = random2( 10 * 4 );
                    break;
                }

                bool is_ok = true;

                // test whether we've used this description before
                // don't have p < j because some are preassigned
                for (int p = 0; p < you.item_description.height(); p++)
                {
                    if ( p == j )
                        continue;

                    if (you.item_description[i][p] == you.item_description[i][j])
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

static void _give_starting_food()
{
    // these undead start with no food
    if (you.species == SP_MUMMY || you.species == SP_GHOUL)
        return;

    item_def item;
    item.quantity = 1;
    if ( you.species == SP_SPRIGGAN )
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
        if (you.species == SP_HILL_ORC || you.species == SP_KOBOLD ||
            you.species == SP_OGRE || you.species == SP_TROLL)
        {
            item.sub_type = FOOD_MEAT_RATION;
        }
        else
            item.sub_type = FOOD_BREAD_RATION;
    }

    const int slot = find_free_slot(item);
    you.inv[slot] = item;       // will ASSERT if couldn't find free slot
}

static void _mark_starting_books()
{
    for (int i = 0; i < ENDOFPACK; i++)
        if (is_valid_item(you.inv[i]) && you.inv[i].base_type == OBJ_BOOKS)
            mark_had_book(you.inv[i].sub_type);
}

static void _racialise_starting_equipment()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]))
        {
            // don't change object type modifier unless it starts plain
            if ((you.inv[i].base_type == OBJ_ARMOUR ||
                 you.inv[i].base_type == OBJ_WEAPONS ||
                 you.inv[i].base_type == OBJ_MISSILES)
                && get_equip_race(you.inv[i]) == ISFLAG_NO_RACE)
            {
                int speci = (player_genus(GENPC_ELVEN))   ? 1 :
                            (player_genus(GENPC_DWARVEN)) ? 2 :
                            (you.species == SP_HILL_ORC)  ? 3 :
                                                            0;

                // now add appropriate species type mod
                switch (speci)
                {
                case 1:
                    set_equip_race( you.inv[i], ISFLAG_ELVEN );
                    break;

                case 2:
                    set_equip_race( you.inv[i], ISFLAG_DWARVEN );
                    break;

                case 3:
                    set_equip_race( you.inv[i], ISFLAG_ORCISH );
                    break;

                default:
                    break;
                }
            }
        }
    }
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
static void _reassess_starting_skills()
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

        // Spellcasters should always have Spellcasting skill.
        if ( i == SK_SPELLCASTING && you.skills[i] == 0 )
        {
            you.skill_points[i] = (skill_exp_needed(2) * sp_diff) / 100;
            you.skills[i] = 1;
        }
    }
}

// randomly boost stats a number of times
static void _assign_remaining_stats( int points_left )
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
        case SP_OGRE_MAGE:
        case SP_DEMIGOD:
            inc_max_hp(2);
            break;

        case SP_HILL_ORC:
        case SP_MUMMY:
        case SP_MERFOLK:
            inc_max_hp(1);
            break;

        case SP_GREY_ELF:
        case SP_HIGH_ELF:
        case SP_VAMPIRE:
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

static void _give_species_bonus_mp()
{
    // adjust max_magic_points by species {dlb}
    switch (you.species)
    {
    case SP_VAMPIRE:
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

static bool _species_is_undead( const species_type speci )
{
    return (speci == SP_MUMMY || speci == SP_GHOUL || speci == SP_VAMPIRE);
}

static undead_state_type _get_undead_state(const species_type sp)
{
    switch(sp)
    {
    case SP_MUMMY:
        return US_UNDEAD;
    case SP_GHOUL:
    case SP_VAMPIRE:
        return US_HUNGRY_DEAD;
    default:
        ASSERT(!_species_is_undead(sp));
        return (US_ALIVE);
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

    _opening_screen();
    _enter_player_name(true);

    if (you.your_name[0] != 0)
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
        _pick_random_species_and_class();
        ng_random = true;
    }
    else
    {
        if (Options.race != 0 && Options.cls != 0
            && Options.race != '*' && Options.cls != '*'
            && !_class_allowed(_get_species(letter_to_index(Options.race)),
                               _get_class(letter_to_index(Options.cls))))
        {
            end(1, false, "Incompatible race and class specified in "
                "options file.");
        }

        // repeat until valid race/class combination found
        while (choose_race() && !choose_class());
    }

    strcpy( you.class_name, get_class_name( you.char_class ) );

    // new: pick name _after_ race and class choices
    if (you.your_name[0] == 0)
    {
        clrscr();

        std::string specs = species_name(you.species, you.experience_level);
        if (specs.length() > 79)
            specs = specs.substr(0, 79);

        cprintf( "You are a%s %s %s." EOL,
                 (is_vowel( specs[0] )) ? "n" : "", specs.c_str(),
                 you.class_name );

        _enter_player_name(false);

        if (_check_saved_game())
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

    _species_stat_init( you.species );     // must be down here {dlb}

    you.is_undead = _get_undead_state(you.species);

    // before we get into the inventory init, set light radius based
    // on species vision. currently, all species see out to 8 squares.
    you.normal_vision  = 8;
    you.current_vision = 8;

    _jobs_stat_init( you.char_class );
    _give_last_paycheck( you.char_class );

    _assign_remaining_stats((you.species == SP_DEMIGOD ||
                             you.species == SP_DEMONSPAWN) ? 15 : 8);

    // this function depends on stats being finalized
    // returns false if Backspace on god/weapon/... selection
    if (!_give_items_skills())
    {
        // now choose again, name stays same
        const std::string old_name = you.your_name;

        Options.prev_randpick = false;
        Options.prev_race   = ng_race;
        Options.prev_cls    = ng_cls;
        Options.prev_weapon = ng_weapon;
        // ck, dk, pr and book are asked last
        // --> don't need to be changed

        // reset stats
        _init_player();

        Options.reset_startup_options();

        // Restore old name
        strncpy(you.your_name, old_name.c_str(), kNameLen);
        you.your_name[kNameLen - 1] = 0;

        // choose new character
        goto game_start;
    }

    _give_species_bonus_hp();
    _give_species_bonus_mp();

    // these need to be set above using functions!!! {dlb}
    you.max_dex = you.dex;
    you.max_strength = you.strength;
    you.max_intel = you.intel;

    _give_starting_food();
    _mark_starting_books();
    _racialise_starting_equipment();

    _initialise_item_descriptions();

    _reassess_starting_skills();
    calc_total_skill_points();

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]))
        {
            // Why is this here? Elsewhere it's only ever used for runes
            you.inv[i].flags |= ISFLAG_BEEN_IN_INV;

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

    // Apply autoinscribe rules to inventory.
    request_autoinscribe();
    autoinscribe();

    // Brand items as original equipment.
    origin_set_inventory(origin_set_startequip);

    // we calculate hp and mp here; all relevant factors should be
    // finalized by now (GDL)
    calc_hp();
    calc_mp();

    // make sure the starting player is fully charged up
    set_hp( you.hp_max, false );
    set_mp( you.max_magic_points, false );

    _give_basic_spells(you.char_class);
    _give_basic_knowledge(you.char_class);

    // tmpfile purging removed in favour of marking
    tmp_file_pairs.init(false);

    _give_basic_mutations(you.species);
    _initialise_branch_depths();

    _save_newgame_options();
    return (true);
}                               // end of new_game()

static bool _class_allowed( species_type speci, job_type char_class )
{
    switch (char_class)
    {
    case JOB_FIGHTER:
        switch (speci)
        {
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
            return false;
        default:
            return true;
        }

    case JOB_WIZARD:
        if (speci == SP_MUMMY)
            return true;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_PRIEST:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_DEMIGOD:
        case SP_GNOME:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_GHOUL:
        case SP_VAMPIRE:
            return false;
        default:
            return true;
        }

    case JOB_THIEF:
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_GLADIATOR:
        if (player_genus(GENPC_ELVEN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_NAGA:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_NECROMANCER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
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
        default:
            return true;
        }

    case JOB_PALADIN:
        switch (speci)
        {
        case SP_HUMAN:
        case SP_MOUNTAIN_DWARF:
        case SP_HIGH_ELF:
        case SP_CENTAUR:
        case SP_MERFOLK:
            return true;
        default:
            return false;
        }

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
        default:
            return true;
        }

    case JOB_BERSERKER:
        if (speci == SP_SLUDGE_ELF)
            return true;
        if (player_genus(GENPC_ELVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_NAGA:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_HUNTER:
        if (player_genus(GENPC_DRACONIAN, speci))   // use bows
            return true;
        if (player_genus(GENPC_DWARVEN, speci))     // use xbows
            return true;

        switch (speci)
        {
            // bows --
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_CENTAUR:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_GREY_ELF:
        case SP_HIGH_ELF:
        case SP_DEEP_ELF:
        case SP_SLUDGE_ELF:
        case SP_HUMAN:
        case SP_KENKU:
            // xbows --
        case SP_HILL_ORC:
            // slings --
        case SP_GNOME:
        case SP_HALFLING:
        case SP_OGRE:
            // javelins
        case SP_MERFOLK:
            return true;
        default:
            return false;
        }

    case JOB_CONJURER:
        if (_species_is_undead( speci ))
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
        default:
            return true;
        }

    case JOB_ENCHANTER:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_SLUDGE_ELF:
            return false;
        default:
            return true;
        }

    case JOB_FIRE_ELEMENTALIST:
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_ICE_ELEMENTALIST:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_SUMMONER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_HALFLING:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_GHOUL:
            return false;
        default:
            return true;
        }

    case JOB_AIR_ELEMENTALIST:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_EARTH_ELEMENTALIST:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_CRUSADER:
        if (_species_is_undead( speci ))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_SLUDGE_ELF:
            return false;
        default:
            return true;
        }

    case JOB_DEATH_KNIGHT:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;

        switch (speci)
        {
        case SP_GHOUL:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_VENOM_MAGE:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_HIGH_ELF:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_CHAOS_KNIGHT:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (_species_is_undead( speci ))
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
        default:
            return true;
        }

    case JOB_TRANSMUTER:
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_HALFLING:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_HEALER:
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_REAVER:
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_GREY_ELF:
        case SP_HALFLING:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_STALKER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return false;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_MONK:
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_GNOME:
        case SP_KOBOLD:
        case SP_NAGA:
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_SPRIGGAN:
        case SP_TROLL:
            return false;
        default:
            return true;
        }

    case JOB_WARPER:
        if (player_genus(GENPC_DWARVEN, speci))
            return false;
        if (player_genus(GENPC_DRACONIAN, speci))
            return true;
        if (_species_is_undead( speci ))
            return false;

        switch (speci)
        {
        case SP_CENTAUR:
        case SP_GNOME:
        case SP_HILL_ORC:
        case SP_KENKU:
        case SP_MINOTAUR:
        case SP_OGRE:
        case SP_TROLL:
        case SP_MERFOLK:
            return false;
        default:
            return true;
        }

    case JOB_WANDERER:
        if (_species_is_undead( speci ))
            return true;
        if (player_genus(GENPC_DRACONIAN, speci))
            return true;

        switch (speci)
        {
        case SP_HUMAN:
        case SP_DEMIGOD:
        case SP_DEMONSPAWN:
        case SP_KOBOLD:
        case SP_NAGA:
        case SP_HILL_ORC:
        case SP_OGRE_MAGE:
        case SP_CENTAUR:
        case SP_KENKU:
        case SP_MERFOLK:
        case SP_SLUDGE_ELF:
            return true;
        default:
            return false;
        }

    default:
        return false;
    }
}                               // end class_allowed()


static bool _choose_book( item_def& book, int firstbook, int numbooks )
{
    int keyin = 0;
    clrscr();
    book.base_type = OBJ_BOOKS;
    book.quantity  = 1;
    book.plus      = 0;
    book.special   = 1;

    // using the fact that CONJ_I and MINOR_MAGIC_I are both
    // fire books, CONJ_II and MINOR_MAGIC_II are both ice books
    if ( Options.book && Options.book <= numbooks )
    {
        book.sub_type = firstbook + Options.book - 1;
        ng_book = Options.book;
        return true;
    }

    if ( Options.prev_book > numbooks && Options.prev_book != SBT_RANDOM )
        Options.prev_book = SBT_NO_SELECTION;

    if ( !Options.random_pick )
    {
        _print_character_info();

        textcolor( CYAN );
        cprintf(EOL "You have a choice of books:" EOL);
        textcolor( LIGHTGREY );

        for (int i = 0; i < numbooks; ++i)
        {
            book.sub_type = firstbook + i;
            cprintf("%c - %s" EOL, 'a' + i, book.name(DESC_PLAIN).c_str());
        }

        textcolor(BROWN);
        cprintf(EOL "* - Random choice; "
                    "Bksp - Back to species and class selection; "
                    "X - Quit" EOL);

        if ( Options.prev_book != SBT_NO_SELECTION )
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
                return false;
            case '\r':
            case '\n':
                if ( Options.prev_book != SBT_NO_SELECTION )
                {
                    if ( Options.prev_book == SBT_RANDOM )
                        keyin = '*';
                    else
                        keyin = ('a' +  Options.prev_book - 1);
                }
            default:
                break;
            }
        }
        while (keyin != '*' && (keyin < 'a' || keyin >= ('a' + numbooks)));
    }

    if (Options.random_pick || Options.book == SBT_RANDOM || keyin == '*')
        ng_book = SBT_RANDOM;
    else
        ng_book = keyin - 'a' + 1;

    if ( Options.random_pick || keyin == '*' )
        keyin = random2(numbooks) + 'a';

    book.sub_type = firstbook + keyin - 'a';
    return true;
}

static bool _choose_weapon()
{
    const weapon_type startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
                                      WPN_HAND_AXE, WPN_SPEAR, WPN_TRIDENT };
    int keyin = 0;
    int num_choices = 4;

    if (you.char_class == JOB_GLADIATOR && you.species != SP_KOBOLD
        || you.species == SP_MERFOLK)
    {
        num_choices = 5;
    }

    if (Options.weapon != WPN_UNKNOWN && Options.weapon != WPN_RANDOM
        && (Options.weapon != WPN_TRIDENT || num_choices == 5))
    {
        you.inv[0].sub_type = Options.weapon;
        ng_weapon = Options.weapon;
        return true;
    }

    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        _print_character_info();

        textcolor( CYAN );
        cprintf(EOL "You have a choice of weapons:" EOL);
        textcolor( LIGHTGREY );

        bool prevmatch = false;
        for (int i = 0; i < num_choices; i++)
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
        cprintf(EOL "* - Random choice; "
                    "Bksp - Back to species and class selection; "
                    "X - Quit" EOL);

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
                return false;
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
           }
        }
        while (keyin != '*' && (keyin < 'a' || keyin > ('a' + num_choices)));


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
        for (int times = 0; times < 50; times++)
        {
            keyin = random2(num_choices);
            int x = effective_stat_bonus(startwep[keyin]);
            if (x > -2)
                break;
        }
        keyin += 'a';
        ng_weapon = WPN_RANDOM;
    }
    else
        ng_weapon = startwep[keyin-'a'];

    you.inv[0].sub_type = startwep[keyin-'a'];

    return true;
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
static void _species_stat_init(species_type which_species)
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

    case SP_HIGH_ELF:           sb =  5; ib =  9; db =  8;      break;  // 22
    case SP_GREY_ELF:           sb =  4; ib =  9; db =  8;      break;  // 21
    case SP_DEEP_ELF:           sb =  3; ib = 10; db =  8;      break;  // 21
    case SP_SLUDGE_ELF:         sb =  6; ib =  7; db =  7;      break;  // 20

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
    case SP_VAMPIRE:            sb =  5; ib =  8; db =  7;      break;  // 20

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_GOLDEN_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:     sb =  9; ib =  6; db =  2;      break;  // 17
    }

    modify_all_stats( sb, ib, db );
}

static void _jobs_stat_init(job_type which_job)
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

static void _give_basic_mutations(species_type speci)
{
    // We should switch over to a size-based system
    // for the fast/slow metabolism when we get around to it.
    switch ( speci )
    {
    case SP_HILL_ORC:
        you.mutation[MUT_SAPROVOROUS] = 1;
        break;
    case SP_OGRE:
        you.mutation[MUT_FAST_METABOLISM] = 1;
        you.mutation[MUT_SAPROVOROUS] = 1;
        break;
    case SP_OGRE_MAGE:
        you.mutation[MUT_FAST_METABOLISM] = 1;
        break;
    case SP_HALFLING:
        you.mutation[MUT_SLOW_METABOLISM] = 1;
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
        you.mutation[MUT_HOOVES] = 1;
        break;
    case SP_NAGA:
        you.mutation[MUT_ACUTE_VISION] = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_DEFORMED] = 1;
        break;
    case SP_MUMMY:
        you.mutation[MUT_TORMENT_RESISTANCE] = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_COLD_RESISTANCE] = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        break;
    case SP_GNOME:
        you.mutation[MUT_MAPPING] = 2;
        break;
    case SP_GHOUL:
        you.mutation[MUT_TORMENT_RESISTANCE] = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_COLD_RESISTANCE] = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        you.mutation[MUT_SAPROVOROUS] = 3;
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_KENKU:
        you.mutation[MUT_TALONS] = 1;
        break;
    case SP_TROLL:
        you.mutation[MUT_REGENERATION] = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        you.mutation[MUT_SAPROVOROUS] = 2;
        you.mutation[MUT_SHAGGY_FUR] = 1;
        break;
    case SP_KOBOLD:
        you.mutation[MUT_SAPROVOROUS] = 2;
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_VAMPIRE:
        you.mutation[MUT_FANGS] = 3;
        you.mutation[MUT_ACUTE_VISION] = 1;
        break;
    default:
        break;
    }

    // starting mutations are unremoveable
    for ( int i = 0; i < NUM_MUTATIONS; ++i )
        you.demon_pow[i] = you.mutation[i];
}

static void _give_basic_knowledge(job_type which_job)
{
    if (you.species == SP_VAMPIRE)
    {
        set_ident_type( OBJ_POTIONS, POT_BLOOD, ID_KNOWN_TYPE );
        set_ident_type( OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE );
    }

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

static void _give_basic_spells(job_type which_job)
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

// eventually, this should be something more grand {dlb}
static void _opening_screen(void)
{
#ifdef USE_TILE
    // more grand... Like this? ;)
    if (Options.tile_title_screen)
        TileDrawTitle();
#endif

    std::string msg =
        "<yellow>Hello, welcome to " CRAWL " " VERSION "!</yellow>" EOL
        "<brown>(c) Copyright 1997-2002 Linley Henzell, "
        "2002-2008 Crawl DevTeam" EOL
        "Please consult crawl_manual.txt for instructions and legal details."
        "</brown>" EOL;

    const bool init_found =
        (init_file_location.find("not found") == std::string::npos);

    if (!init_found)
        msg += "<lightred>Init file ";
    else
        msg += "<lightgrey>Init file read: ";

    msg += init_file_location;
    msg += EOL;

    formatted_string::parse_string(msg).display();
    textcolor( LIGHTGREY );
    return;
}

static void _show_name_prompt(int where, bool blankOK,
        const std::vector<player_save_info> &existing_chars,
        slider_menu &menu)
{
    cgotoxy(1, where);
    textcolor( CYAN );
    if (blankOK)
    {
        if (Options.prev_name.length() && Options.remember_name)
        {
            cprintf(EOL
                    "Press <Enter> for \"%s\", or . to be prompted later."
                    EOL,
                    Options.prev_name.c_str());
        }
        else
        {
            cprintf(EOL
                    "Press <Enter> to answer this after race and "
                    "class are chosen." EOL);
        }
    }

    cprintf(EOL "What is your name today? ");

    if (!existing_chars.empty())
    {
        const int name_x = wherex(), name_y = wherey();
        menu.set_limits(name_y + 3, get_number_of_lines());
        menu.display();
        cgotoxy(name_x, name_y);
    }

    textcolor( LIGHTGREY );
}

static void _preprocess_character_name(char *name, bool blankOK)
{
    if (!*name && blankOK && Options.prev_name.length() &&
            Options.remember_name)
    {
        strncpy(name, Options.prev_name.c_str(), kNameLen);
        name[kNameLen - 1] = 0;
    }

    // '.', '?' and '*' are blanked.
    if (!name[1] && (*name == '.' || *name == '*' || *name == '?'))
        *name = 0;
}

static bool _is_good_name(char *name, bool blankOK, bool verbose)
{
    _preprocess_character_name(name, blankOK);

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
    return (_validate_player_name(verbose));
}

static int newname_keyfilter(int &ch)
{
    if (ch == CK_DOWN || ch == CK_PGDN || ch == '\t')
        return -1;
    return 1;
}

static bool _read_player_name( char *name, int len,
        const std::vector<player_save_info> &existing,
        slider_menu &menu)
{
    const int name_x = wherex(), name_y = wherey();
    int (*keyfilter)(int &) = newname_keyfilter;
    if (existing.empty())
        keyfilter = NULL;

    line_reader reader(name, len);
    reader.set_keyproc(keyfilter);

    while (true)
    {
        cgotoxy(name_x, name_y);
        if (name_x <= 80)
            cprintf("%-*s", 80 - name_x + 1, "");

        cgotoxy(name_x, name_y);
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
                const player_save_info &p =
                    *static_cast<player_save_info*>( sel->data );
                strncpy(name, p.name.c_str(), kNameLen);
                name[kNameLen - 1] = 0;
                return (true);
            }
        }
        // Go back and prompt the user.
    }
}

static void _enter_player_name(bool blankOK)
{
    int prompt_start = wherey();
    bool ask_name = true;
    char *name = you.your_name;
    std::vector<player_save_info> existing_chars;
    slider_menu char_menu;

    if (you.your_name[0] != 0)
        ask_name = false;

    if (blankOK && (ask_name || !_is_good_name(you.your_name, false, false)))
    {
        existing_chars = find_saved_characters();
        if (existing_chars.empty())
        {
             cgotoxy(1,12);
             formatted_string::parse_string(
                "  If you've never been here before, you might want to try out" EOL
                "  the Dungeon Crawl tutorial. To do this, press "
                "<white>T</white> on the next" EOL
                "  screen.").display();
        }
        else
        {
             MenuEntry *title = new MenuEntry("Or choose an existing character:");
             title->colour = LIGHTCYAN;
             char_menu.set_title( title );
             for (unsigned int i = 0; i < existing_chars.size(); ++i)
             {
                 std::string desc = " " + existing_chars[i].short_desc();
                 if (static_cast<int>(desc.length()) >= get_number_of_cols())
                     desc = desc.substr(0, get_number_of_cols() - 1);

                 MenuEntry *me = new MenuEntry(desc);
                 me->data = &existing_chars[i];
                 char_menu.add_entry(me);
             }
             char_menu.set_flags(MF_NOWRAP | MF_SINGLESELECT);
        }
    }

    do
    {
        // prompt for a new name if current one unsatisfactory {dlb}:
        if (ask_name)
        {
            _show_name_prompt(prompt_start, blankOK, existing_chars, char_menu);

            // If the player wants out, we bail out.
            if (!_read_player_name(name, kNameLen, existing_chars, char_menu))
                end(0);

#ifdef USE_TILE
            clrscr();
            cgotoxy(1, 1);
#endif

            // Laboriously trim the damn thing.
            std::string read_name = name;
            trim_string(read_name);
            strncpy(name, read_name.c_str(), kNameLen);
            name[kNameLen - 1] = 0;
        }
    }
    while (ask_name = !_is_good_name(you.your_name, blankOK, true));
}                               // end enter_player_name()

static bool _validate_player_name(bool verbose)
{
#if defined(DOS) || defined(WIN32CONSOLE) || defined(WIN32TILES)
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

static void _give_random_potion( int slot )
{
    // If you can't quaff, you don't care
    if (you.is_undead == US_UNDEAD)
        return;

    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_POTIONS;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;

    int temp_rand = 8;
    if (you.is_undead) // no Berserk for undeads
        temp_rand--;

    switch (random2(temp_rand))
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

static void _give_random_secondary_armour( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_ARMOUR;
    you.inv[ slot ].special = 0;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;

    switch (random2(4))
    {
    case 0:
        if (you_can_wear(EQ_BOOTS))
        {
            you.inv[ slot ].sub_type = ARM_BOOTS;
            you.equip[EQ_BOOTS] = slot;
            break;
        }
        // else fall through
    case 1:
        if (you_can_wear(EQ_HELMET))
        {
            you.inv[ slot ].sub_type = ARM_HELMET;
            you.equip[EQ_HELMET] = slot;
            break;
        }
        // else fall through
    case 2:
        if (you_can_wear(EQ_GLOVES))
        {
            you.inv[ slot ].sub_type = ARM_GLOVES;
            you.equip[EQ_GLOVES] = slot;
            break;
        }
        // else fall through
    case 3: // anyone can wear this
        you.inv[ slot ].sub_type = ARM_CLOAK;
        you.equip[EQ_CLOAK] = slot;
        break;
    }
}

// Returns true if a "good" weapon is given
static bool _give_wanderer_weapon( int slot, int wpn_skill )
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

    case SK_LONG_BLADES:
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

static void _make_rod(item_def &item, stave_type rod_type)
{
    item.base_type = OBJ_STAVES;
    item.sub_type = rod_type;
    item.quantity = 1;
    item.special = you.item_description[IDESC_STAVES][rod_type];
    item.colour = BROWN;

    init_rod_mp(item);
}

static void _newgame_make_item(int slot, equipment_type eqslot,
                               object_class_type base,
                               int sub_type, int qty = 1,
                               int plus = 0, int plus2 = 0)
{
    if (slot == -1)
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!is_valid_item(you.inv[i]))
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

    if (eqslot != EQ_NONE)
        you.equip[eqslot] = slot;
}

static void _newgame_clear_item(int slot)
{
    you.inv[slot] = item_def();

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        if (you.equip[i] == slot)
            you.equip[i] = -1;
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
static void _create_wanderer( void )
{
    const skill_type util_skills[] =
        { SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };

    // Long swords is missing to increase its rarity because we
    // can't give out a long sword to a starting character (they're
    // all too good)... Staves is also removed because it's not
    // one of the fighter options.-- bwr
    const skill_type fight_util_skills[] =
        { SK_FIGHTING, SK_SHORT_BLADES, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS,
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };

    const skill_type not_rare_skills[] =
        { SK_SLINGS, SK_BOWS, SK_CROSSBOWS,
          SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS,
          SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,
          SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES,
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };

    const skill_type all_skills[] =
        { SK_SUMMONINGS, SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMIGRATION,
          SK_DIVINATIONS, SK_POISON_MAGIC,
          SK_SLINGS, SK_BOWS, SK_CROSSBOWS,
          SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS,
          SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,
          SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES,
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };

    skill_type skill;

    for (int i = 0; i < 2; i++)
    {
        do
        {
            skill = RANDOM_ELEMENT(util_skills);
        }
        while (you.skills[skill] >= 2);

        you.skills[skill]++;
    }

    for (int i = 0; i < 3; i++)
    {
        do
        {
            skill = RANDOM_ELEMENT(fight_util_skills);
        }
        while (you.skills[skill] >= 2);

        you.skills[skill]++;
    }

    // Spell skills are possible past this point, but we won't
    // allow two levels of any of them -- bwr
    for (int i = 0; i < 3; i++)
    {
        do
        {
            skill = RANDOM_ELEMENT(not_rare_skills);
        }
        while (you.skills[skill] >= 2
               || (skill >= SK_SPELLCASTING && you.skills[skill] > 0));

        you.skills[skill]++;
    }

    for (int i = 0; i < 2; i++)
    {
        do
        {
            skill = RANDOM_ELEMENT(all_skills);
        }
        while (you.skills[skill] >= 2
               || (skill >= SK_SPELLCASTING && you.skills[skill] > 0));

        you.skills[skill]++;
    }

    // Demigods can't use invocations so we'll swap it for something else
    if (you.species == SP_DEMIGOD && you.skills[ SK_INVOCATIONS ])
    {
        do
        {
            skill = RANDOM_ELEMENT(all_skills);
        }
        while (you.skills[skill] > 0);

        you.skills[skill] = you.skills[SK_INVOCATIONS];
        you.skills[SK_INVOCATIONS] = 0;
    }

    // ogres and draconians cannot wear armour
    if ((you.species == SP_OGRE_MAGE || player_genus(GENPC_DRACONIAN))
        && you.skills[ SK_ARMOUR ])
    {
        do
        {
            skill = RANDOM_ELEMENT(all_skills);
        }
        while (you.skills[skill] > 0);

        you.skills[skill] = you.skills[SK_ARMOUR];
        you.skills[SK_ARMOUR] = 0;
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
    _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_KNIFE);

    // And a default armour template for a robe (leaving slot 1 open for
    // a secondary weapon).
    _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

    // Wanderers have at least seen one type of potion, and if they
    // don't get anything else good, they'll get to keep this one...
    // Note:  even if this is taken away, the knowledge of the potion
    // type is still given to the character.
    _give_random_potion(3);

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
            _newgame_make_item(4, EQ_SHIELD, OBJ_ARMOUR,
                (player_genus(GENPC_DRACONIAN) || you.species == SP_OGRE_MAGE) ?
                    ARM_SHIELD : ARM_BUCKLER);
            you.inv[3].quantity = 0;            // remove potion
        }
        else
        {
            _give_random_secondary_armour(5);
        }

        // remove potion if good weapon is given:
        if (_give_wanderer_weapon( 0, wpn_skill ))
            you.inv[3].quantity = 0;
    }
    else
    {
        // Generic wanderer
        _give_wanderer_weapon( 0, wpn_skill );
        _give_random_secondary_armour(5);
    }

    you.equip[EQ_WEAPON] = 0;
    you.equip[EQ_BODY_ARMOUR] = 2;
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
        you.char_class = _get_class(letter_to_index(Options.cls));
        ng_cls = Options.cls;
    }

    if (Options.race != 0)
        printed = true;

    // the list musn't be longer than the number of actual species
    COMPILE_CHECK(ARRAYSIZE(old_species_order) <= NUM_SPECIES, c1);

    // check whether the two lists have the same size
    COMPILE_CHECK(ARRAYSIZE(old_species_order) == ARRAYSIZE(new_species_order), c2);

    const int num_species = ARRAYSIZE(old_species_order);

spec_query:
    bool prevraceok = (Options.prev_race == '*');
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

            textcolor( WHITE ); // for the tutorial
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press T to enter a tutorial.)");
        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:  "
                "(Press ? for more information, %% for a list of aptitudes)");
        cprintf(EOL EOL);

        textcolor( LIGHTGREY );

        int j = 0;
        for (int i = 0; i < num_species; ++i)
        {
            const species_type si = _get_species(i);

            if (!_is_species_valid_choice(si))
                continue;

            if (you.char_class != JOB_UNKNOWN
                && !_class_allowed(si, you.char_class))
            {
                continue;
            }

            char sletter = index_to_letter(i);

            if (sletter == Options.prev_race)
                prevraceok = true;

            cprintf( "%c - %s", sletter, species_name(si,1).c_str() );

            if (j % 2)
                cprintf(EOL);
            else
                cgotoxy(31, wherey());

            j++;
        }

        if (j % 2)
            cprintf(EOL);

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
            {
                cprintf("Enter - %s",
                        _get_opt_race_name(Options.prev_race).c_str());
            }
            if (_prev_startup_options_set())
            {
                cprintf("%sTAB - %s",
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
    {
        keyn = Options.race;
    }
    else
    {
        keyn = c_getch();
    }

    switch (keyn)
    {
    case 'X':
    case ESCAPE:
        cprintf(EOL "Goodbye!");
        end(0);
        break;
    case CK_BKSP:
    case ' ':
        you.species = SP_UNKNOWN;
        Options.race = 0;
        return true;
    case '!':
        _pick_random_species_and_class();
        Options.random_pick = true; // used to give random weapon/god as well
        ng_random = true;
        return false;
    case '\r':
    case '\n':
        if (Options.prev_race && prevraceok)
            keyn = Options.prev_race;
        break;
    case '\t':
        if (_prev_startup_options_set())
        {
            if (Options.prev_randpick ||
                (Options.prev_race == '*' && Options.prev_cls == '*'))
            {
                Options.random_pick = true;
                ng_random = true;
                _pick_random_species_and_class();
                return false;
            }
            _set_startup_options();
            you.species = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;
            return true;
        }
        break;
    // access to the help files
    case '?':
        list_commands(false, '1');
        return choose_race();
    case '%':
        list_commands(false, '%');
        return choose_race();
    default:
        break;
    }

    // These are handled specially as they _could_ be set
    // using Options.race or prev_race
    if (keyn == 'T') // easy to set in init.txt
    {
        return !pick_tutorial();
    }

    bool randrace = (keyn == '*');
    if (randrace)
    {
        int index;
        do
        {
            index = random2(num_species);
        }
        while (!_is_species_valid_choice(_get_species(index), false)
               || (you.char_class != JOB_UNKNOWN
                   && !_class_allowed(_get_species(index), you.char_class)));

        keyn = index_to_letter(index);
    }

    if (keyn >= 'a' && keyn <= 'z' || keyn >= 'A' && keyn <= 'Z')
    {
        you.species = _get_species(letter_to_index(keyn));
    }

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
        && !_class_allowed(you.species, you.char_class))
    {
        goto spec_query;
    }

    // set to 0 in case we come back from choose_class()
    Options.race = 0;
    ng_race = (randrace? '*' : keyn);

    if (you.species == SP_RED_DRACONIAN)
        you.species = _random_draconian_species();

    return true;
}

// returns true if a class was chosen, false if we should go back to
// race selection.
bool choose_class(void)
{
    char keyn;

    bool printed = false;
    if (Options.cls != 0)
        printed = true;

    if (you.species != SP_UNKNOWN && you.char_class != JOB_UNKNOWN)
        return true;

    ng_cls = 0;

    // the list musn't be longer than the number of actual classes
    COMPILE_CHECK(ARRAYSIZE(old_jobs_order) <= NUM_JOBS, c1);

    // check whether the two lists have the same size
    COMPILE_CHECK(ARRAYSIZE(old_jobs_order) == ARRAYSIZE(new_jobs_order), c2);

    const int num_classes = ARRAYSIZE(old_jobs_order);

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
            if (you.your_name[0])
                cprintf("%s the ", you.your_name);
            cprintf("%s.", species_name(you.species, 1).c_str());
            textcolor( WHITE );
        }
        else
        {
            textcolor( WHITE );
            cprintf("You must be new here!");
        }
        cprintf("  (Press T to enter a tutorial.)");

        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:  "
                "(Press ? for more information, %% for a list of aptitudes)" EOL EOL);
        textcolor( LIGHTGREY );

        int j = 0;
        for (int i = 0; i < num_classes; i++)
        {
            if (you.species != SP_UNKNOWN
                && !_class_allowed(you.species, _get_class(i)))
            {
                continue;
            }

            char letter = index_to_letter(i);

            if (letter == Options.prev_cls)
                prevclassok = true;

            cprintf( "%c - %s", letter, get_class_name(_get_class(i)) );

            if (j % 2)
                cprintf(EOL);
            else
                cgotoxy(31, wherey());

            j++;
        }

        if (j % 2)
            cprintf(EOL);

        textcolor( BROWN );
        if (you.species == SP_UNKNOWN)
        {
            cprintf(EOL
                    "SPACE - Choose species first; * - Random Class; "
                    "! - Random Character; X - Quit"
                    EOL);
        }
        else
        {
            cprintf(EOL
                    "* - Random; Bksp - Back to species selection; X - Quit"
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
                cprintf("%sTAB - %s",
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
    {
        keyn = Options.cls;
    }
    else
    {
        keyn = c_getch();
    }

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
            return false;
        }
    case 'T':
        return pick_tutorial();
    case '!':
        _pick_random_species_and_class();
        // used to give random weapon/god as well
        Options.random_pick = true;
        ng_random = true;
        return true;
    case '\r':
    case '\n':
        if (Options.prev_cls && prevclassok)
            keyn = Options.prev_cls;
        break;
    case '\t':
        if (_prev_startup_options_set())
        {
            if (Options.prev_randpick ||
                (Options.prev_race == '*' && Options.prev_cls == '*'))
            {
                Options.random_pick = true;
                ng_random = true;
                _pick_random_species_and_class();
                return true;
            }
            _set_startup_options();

            // Toss out old species selection, if any.
            you.species = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;

            return false;
        }
        break;
    // help files
    case '?':
        list_commands(false, '2');
        return choose_class();
    case '%':
        list_commands(false, '%');
        return choose_class();
    default:
        break;
    }

    job_type chosen_job = JOB_UNKNOWN;
    if (keyn == '*')
    {
        // pick a job at random... see god retribution for proof this
        // is uniform. -- bwr
        int job_count = 0;

        for (int i = 0; i < NUM_JOBS; i++)
        {
             if (you.species == SP_UNKNOWN
                 || _class_allowed(you.species, static_cast<job_type>(i)))
             {
                 job_count++;
                 if (one_chance_in( job_count ))
                     chosen_job = static_cast<job_type>(i);
             }
         }
         ASSERT( chosen_job != JOB_UNKNOWN );

         ng_cls = '*';
    }
    else if (keyn >= 'a' && keyn <= 'z' || keyn >= 'A' && keyn <= 'Z')
    {
         chosen_job = _get_class(letter_to_index(keyn));
    }

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
        && !_class_allowed(you.species, chosen_job))
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

    you.char_class = chosen_job;
    return (you.char_class != JOB_UNKNOWN && you.species != SP_UNKNOWN);
}

bool _give_items_skills()
{
    char keyn;
    int weap_skill = 0;
    int to_hit_bonus = 0;       // used for assigning primary weapons {dlb}
    int choice;                 // used for third-screen choices

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (you.species == SP_OGRE || you.species == SP_TROLL)
        {
            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ANIMAL_SKIN);

            if (you.species == SP_OGRE)
                _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_ANCUS);
            else if (you.species == SP_TROLL)
                _newgame_clear_item(0);
        }
        else if (player_genus(GENPC_DRACONIAN))
        {
            if (!_choose_weapon())
                return (false);

            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
            _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD);
        }
        else if (you.species == SP_HALFLING || you.species == SP_KOBOLD
                 || you.species == SP_GNOME || you.species == SP_VAMPIRE)
        {
            if (!_choose_weapon())
                return false;

            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR,
                               ARM_LEATHER_ARMOUR);

            if (you.species != SP_VAMPIRE)
            {
                _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_DART,
                                   10 + roll_dice( 2, 10 ));
            }
        }
        else
        {
            if (!_choose_weapon())
                return false;

            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL);
            _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD);
        }

        you.skills[SK_FIGHTING] = 3;

        if (you.species != SP_TROLL)
            weap_skill = 2;

        if (you.species == SP_HALFLING || you.species == SP_KOBOLD ||
            you.species == SP_GNOME)
        {
            you.skills[SK_THROWING] = 1;
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

            // Elven armour is light, we need to know this up front.
            _racialise_starting_equipment();

            you.skills[(player_light_armour()? SK_DODGING : SK_ARMOUR)] = 2;

            you.skills[SK_THROWING] = 2;

            if (you.species != SP_VAMPIRE)
                you.skills[SK_SHIELDS] = 2;

            if (you.species == SP_VAMPIRE || coinflip())
                you.skills[SK_STABBING]++;
            else
                you.skills[SK_SHIELDS]++;
        }
        break;

    case JOB_WIZARD:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS,
                           you.species == SP_OGRE_MAGE? WPN_QUARTERSTAFF :
                           player_genus(GENPC_DWARVEN)? WPN_HAMMER :
                           WPN_DAGGER);

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

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

        // extra items being tested:
        if (!_choose_book( you.inv[2], BOOK_MINOR_MAGIC_I, 3 ))
            return false;

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

        if (you.species == SP_KOBOLD || you.species == SP_HALFLING)
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_KNIFE, 1, 1, 1);
        else
        {
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS,
                (player_genus(GENPC_ELVEN) || you.species == SP_MERFOLK) ?
                WPN_QUARTERSTAFF : WPN_MACE);
        }

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        if (you.is_undead != US_UNDEAD)
            _newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEALING, 2);

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_INVOCATIONS] = 4;
        you.skills[ weapon_skill(you.inv[0]) ] = 2;

        // set gods
        if (you.species == SP_MUMMY || you.species == SP_DEMONSPAWN)
        {
            you.religion = GOD_YREDELEMNUL;
        }
        else
        {
            // disallow invalid choices
            if (you.species != SP_HILL_ORC && Options.priest == GOD_BEOGH)
                Options.priest = GOD_NO_GOD;

            if (Options.priest != GOD_NO_GOD && Options.priest != GOD_RANDOM)
                ng_pr = you.religion = static_cast<god_type>( Options.priest );
            else if (Options.random_pick || Options.priest == GOD_RANDOM)
            {
                you.religion = coinflip() ? GOD_YREDELEMNUL : GOD_ZIN;

                // for orcs 50% chance of Beogh instead
                if (you.species == SP_HILL_ORC && coinflip())
                    you.religion = GOD_BEOGH;

                ng_pr = GOD_RANDOM;
            }
            else
            {
                _print_character_info();

                textcolor( CYAN );
                cprintf(EOL "Which god do you wish to serve?" EOL);

                textcolor( LIGHTGREY );
                cprintf("a - Zin (for traditional priests)" EOL);
                cprintf("b - Yredelemnul (for priests of death)" EOL);
                if (you.species == SP_HILL_ORC)
                    cprintf("c - Beogh (priest of Orcs)" EOL);

                textcolor( BROWN );
                cprintf(EOL "* - Random choice; "
                            "Bksp - Back to species and class selection; "
                            "X - Quit" EOL);

                if (Options.prev_pr == GOD_BEOGH && you.species != SP_HILL_ORC)
                    Options.prev_pr = GOD_NO_GOD;

                if (Options.prev_pr != GOD_NO_GOD)
                {
                    textcolor(BROWN);
                    cprintf(EOL "Enter - %s" EOL,
                            Options.prev_pr == GOD_ZIN?         "Zin" :
                            Options.prev_pr == GOD_YREDELEMNUL? "Yredelemnul" :
                            Options.prev_pr == GOD_BEOGH?       "Beogh"
                                                        :       "Random");
                }

                do {
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
                        return false;
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
                    case '*':
                        you.religion = coinflip()? GOD_ZIN : GOD_YREDELEMNUL;
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

                ng_pr = (keyn == '*'? GOD_RANDOM : you.religion);
            }
        }
        break;

    case JOB_THIEF:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_DAGGER);
        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);
        _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART,
                           10 + roll_dice( 2, 10 ));

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_SHORT_BLADES] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_STEALTH] = 2;
        you.skills[SK_STABBING] = 1;
        you.skills[SK_DODGING + random2(3)]++;
        you.skills[SK_THROWING] = 1;
        you.skills[SK_DARTS] = 1;
        you.skills[SK_TRAPS_DOORS] = 2;
        break;

    case JOB_GLADIATOR:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return false;

        if (player_genus(GENPC_DRACONIAN) || you.species == SP_OGRE)
        {
            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ANIMAL_SKIN);
            _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD);
        }
        else
        {
            _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR,
                               ARM_LEATHER_ARMOUR);
            _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_BUCKLER);
        }

        if (you.species != SP_KOBOLD)
        {
            _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, 4);
            you.skills[SK_THROWING] = 1;
        }
        else
            you.skills[SK_DODGING] = 1;

        you.skills[SK_FIGHTING] = 3;
        weap_skill = 3;

        you.skills[SK_DODGING] += 2;
        you.skills[SK_SHIELDS] = 1;
        you.skills[SK_UNARMED_COMBAT] = 2;
        break;


    case JOB_NECROMANCER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);
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

        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_FALCHION);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD);
        _newgame_make_item(3, EQ_NONE, OBJ_POTIONS, POT_HEALING);

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[(coinflip()? SK_ARMOUR : SK_DODGING)]++;
        you.skills[SK_SHIELDS] = 2;
        you.skills[SK_LONG_BLADES] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_ASSASSIN:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, 1,
                           1 + to_hit_bonus, 1 + (2 - to_hit_bonus));
        _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BLOWGUN);
        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);

        // deep elves get hand crossbows, everyone else gets blowguns
        // (deep elves tend to suck at melee and need something that
        // can do ranged damage)
        if (you.species == SP_DEEP_ELF)
        {
            you.inv[1].sub_type = WPN_HAND_CROSSBOW;
            _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART,
                               10 + roll_dice( 2, 10 ));
            set_item_ego_type( you.inv[4], OBJ_MISSILES, SPMSL_POISONED );
        }
        else
        {
            _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_NEEDLE,
                               5 + roll_dice(2, 5));
            set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_POISONED);
            _newgame_make_item(5, EQ_NONE, OBJ_MISSILES, MI_NEEDLE,
                               1 + random2(4));
            set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_CURARE);
        }

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_SHORT_BLADES] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 3;
        you.skills[SK_STABBING] = 2;
        // DE still get Throwing skill because of Assassin/darts association
        you.skills[SK_THROWING] = 1;
        you.skills[SK_DARTS] = 1;
        if (you.species == SP_DEEP_ELF)
            you.skills[SK_CROSSBOWS] = 1;
        else
            you.skills[SK_THROWING] += 1;

        break;

    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        // WEAPONS
        if (you.species == SP_OGRE)
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_ANCUS);
        else if (you.species == SP_TROLL)
            you.equip[EQ_WEAPON] = -1;
        else
        {
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_HAND_AXE);
            for (int i = 2; i <= 4; i++)
                _newgame_make_item(i, EQ_NONE, OBJ_WEAPONS, WPN_SPEAR);
        }

        // ARMOUR
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        if (!can_wear_armour(you.inv[1], false, true))
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;

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
            you.skills[SK_MACES_FLAILS] = 3;
        }
        else
        {
            you.skills[SK_AXES] = 3;
            you.skills[SK_ARMOUR] = 2;
            you.skills[SK_DODGING] = 2;
            you.skills[SK_THROWING] = 2;
        }
        break;

    case JOB_HUNTER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        if (!can_wear_armour(you.inv[2], false, true))
        {
            you.inv[2].sub_type =
                player_genus(GENPC_DRACONIAN)? ARM_ROBE : ARM_ANIMAL_SKIN;
        }

        if (you.species == SP_MERFOLK)
        {
            // Merfolk are spear hunters -- clobber bow, give six javelins
            // possibly allow choice between javelin and net
            you.inv[1] = you.inv[2];
            you.equip[EQ_BODY_ARMOUR] = 1;
            _newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, 6);
        }
        else
        {
            _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_ARROW,
                               15 + random2avg(21, 5));
            _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
        }

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 1;

        switch (you.species)
        {
        case SP_OGRE:
            // Ogres chop up their food without finesse.
            you.inv[0].sub_type = WPN_HAND_AXE;
            you.inv[1].quantity = 0;

            // And they get to throw rocks.
            you.inv[3].sub_type = MI_LARGE_ROCK;
            you.inv[3].quantity = 4;
            break;

        case SP_HALFLING:
        case SP_GNOME:
            you.inv[3].quantity += random2avg(15, 2);
            you.inv[3].sub_type = MI_SLING_BULLET;
            you.inv[1].sub_type = WPN_SLING;

            you.skills[SK_DODGING] = 2;
            you.skills[SK_STEALTH] = 2;
            you.skills[SK_SLINGS] = 2;
            you.skills[SK_THROWING] += 2;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_HILL_ORC:
            you.inv[3].sub_type = MI_BOLT;
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
            you.skills[SK_CROSSBOWS] = 3;
            break;

        case SP_MERFOLK:
            you.inv[0].sub_type = WPN_TRIDENT;
            you.skills[SK_POLEARMS] = 2;
            you.skills[SK_DODGING] = 2;
            you.skills[SK_THROWING] += 3;

            // And a hunting knife.
            _newgame_make_item(-1, EQ_NONE, OBJ_WEAPONS, WPN_KNIFE);
            break;

        default:
            you.skills[SK_DODGING] = 1;
            you.skills[SK_STEALTH] = 1;
            you.skills[(coinflip() ? SK_STABBING : SK_STEALTH)]++;
            you.skills[SK_BOWS] = 3;
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
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        if (you.char_class == JOB_ENCHANTER)
        {
            you.inv[0].plus = 1;
            you.inv[0].plus2 = 1;
            you.inv[1].plus = 1;
        }

        if ( you.char_class == JOB_CONJURER )
        {
            if (!_choose_book( you.inv[2], BOOK_CONJURATIONS_I, 2 ))
                return false;
        }

        switch (you.char_class)
        {
        case JOB_SUMMONER:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_CALLINGS);
            you.skills[SK_SUMMONINGS] = 4;
            // gets some darts - this class is difficult to start off with
            _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART,
                               8 + roll_dice( 2, 8 ));
            break;

        case JOB_CONJURER:
            you.skills[SK_CONJURATIONS] = 4;
            break;

        case JOB_ENCHANTER:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_CHARMS);

            you.skills[SK_ENCHANTMENTS] = 4;

            // gets some darts - this class is difficult to start off with
            _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART,
                               8 + roll_dice( 2, 8 ), 1);

            if (you.species == SP_SPRIGGAN)
                _make_rod(you.inv[0], STAFF_STRIKING);
            break;

        case JOB_FIRE_ELEMENTALIST:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_FLAMES);
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_FIRE_MAGIC] = 3;
            break;

        case JOB_ICE_ELEMENTALIST:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_FROST);
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_ICE_MAGIC] = 3;
            break;

        case JOB_AIR_ELEMENTALIST:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_AIR);
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_AIR_MAGIC] = 3;
            break;

        case JOB_EARTH_ELEMENTALIST:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_GEOMANCY);
            _newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_STONE, 20);

            if (you.species == SP_GNOME)
            {
                _newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
                _newgame_make_item(4, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
            }
            you.skills[SK_TRANSMIGRATION] = 1;
            you.skills[SK_EARTH_MAGIC] = 3;
            break;

        case JOB_VENOM_MAGE:
            _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_YOUNG_POISONERS);
            you.skills[SK_POISON_MAGIC] = 4;
            break;

        default:
            break;
        }

        if (you.species == SP_OGRE_MAGE)
            you.inv[0].sub_type = WPN_QUARTERSTAFF;
        else if (player_genus(GENPC_DWARVEN))
            you.inv[0].sub_type = WPN_HAMMER;

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
            you.skills[SK_THROWING]++;
        else
            you.skills[ coinflip() ? SK_DODGING : SK_STEALTH ]++;
        break;

    case JOB_TRANSMUTER:
        // some sticks for sticks to snakes:
        _newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_ARROW,
                           6 + roll_dice( 3, 4 ));
        _newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_CHANGES);

        // A little bit of starting ammo for evaporate... don't need too
        // much now that the character can make their own. -- bwr
        //
        // some ammo for evaporate:
        _newgame_make_item(4, EQ_NONE, OBJ_POTIONS, POT_CONFUSION, 2);
        _newgame_make_item(5, EQ_NONE, OBJ_POTIONS, POT_POISON);

        you.equip[EQ_WEAPON] = -1;

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_TRANSMIGRATION] = 2;

        if (you.species == SP_SPRIGGAN)
        {
            _make_rod(you.inv[0], STAFF_STRIKING);

            you.skills[SK_EVOCATIONS] = 2;
            you.skills[SK_FIGHTING] = 0;

            you.equip[EQ_WEAPON] = 0;
        }
        break;

    case JOB_WARPER:
        if (you.species == SP_SPRIGGAN)
        {
            _make_rod(you.inv[0], STAFF_STRIKING);

            you.skills[SK_EVOCATIONS] = 3;
        }
        else
        {
            _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
            if (you.species == SP_OGRE_MAGE)
                you.inv[0].sub_type = WPN_QUARTERSTAFF;

            weap_skill = 2;
            you.skills[SK_FIGHTING] = 1;
        }

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        if (you.species == SP_SPRIGGAN || you.species == SP_OGRE_MAGE
            || player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].sub_type = ARM_ROBE;
        }

        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_SPATIAL_TRANSLOCATIONS);

        // one free escape:
        _newgame_make_item(3, EQ_NONE, OBJ_SCROLLS, SCR_BLINKING);
        _newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART,
                           10 + roll_dice( 2, 10 ));

        you.skills[SK_THROWING] = 1;
        you.skills[SK_DARTS] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_STEALTH] = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_TRANSLOCATIONS] = 2;
        break;

    case JOB_CRUSADER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return false;

        weap_skill = 2;
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_WAR_CHANTS);

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_ENCHANTMENTS] = 2;
        break;


    case JOB_DEATH_KNIGHT:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        if (!_choose_weapon())
            return false;

        weap_skill = 2;

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

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
            _print_character_info();

            textcolor( CYAN );
            cprintf(EOL "From where do you draw your power?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Necromantic magic" EOL);
            cprintf("b - the god Yredelemnul" EOL);

            textcolor( BROWN );
            cprintf(EOL "* - Random choice; "
                        "Bksp - Back to species and class selection; "
                        "X - Quit" EOL);

            if (Options.prev_dk != DK_NO_SELECTION)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_dk == DK_NECROMANCY? "Necromancy" :
                        Options.prev_dk == DK_YREDELEMNUL? "Yredelemnul" :
                                                           "Random");
            }

            do {
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
                    return false;
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
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, 1,
                           random2(3), random2(3));

        if (one_chance_in(5))
            set_equip_desc( you.inv[0], ISFLAG_RUNED );

        if (one_chance_in(5))
            set_equip_desc( you.inv[0], ISFLAG_GLOWING );

        if (!_choose_weapon())
            return false;

        weap_skill = 2;
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE, 1,
                           random2(3));

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
            _print_character_info();

            textcolor( CYAN );
            cprintf(EOL "Which god of chaos do you wish to serve?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Xom of Chaos" EOL);
            cprintf("b - Makhleb the Destroyer" EOL);

            textcolor( BROWN );
            cprintf(EOL "* - Random choice; "
                        "Bksp - Back to species and class selection; "
                        "X - Quit" EOL);

            if (Options.prev_ck != GOD_NO_GOD)
            {
                textcolor(BROWN);
                cprintf(EOL "Enter - %s" EOL,
                        Options.prev_ck == GOD_XOM?     "Xom" :
                        Options.prev_ck == GOD_MAKHLEB? "Makhleb"
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
                    return false;
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
                case '*':
                    you.religion = (coinflip()? GOD_XOM : GOD_MAKHLEB);
                    break;
                case 'a':
                    you.religion = GOD_XOM;
                    break;
                case 'b':
                    you.religion = GOD_MAKHLEB;
                    // fall through
                default:
                    break;
                }
            }
            while (you.religion == GOD_NO_GOD);

            ng_ck = (keyn == '*') ? GOD_RANDOM
                                  : you.religion;
        }

        if (you.religion == GOD_XOM)
        {
            you.skills[SK_FIGHTING]++;
            // the new (piety-aware) Xom uses piety in his own special way...
            // (namely, 100 is neutral)
            you.piety = 100;
            // the new Xom also uses gift_timeout in his own special way...
            // (namely, a countdown to becoming bored)
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

        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_QUARTERSTAFF);
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEALING);
        _newgame_make_item(3, EQ_NONE, OBJ_POTIONS, POT_HEAL_WOUNDS);

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_STAVES] = 3;
        you.skills[SK_INVOCATIONS] = 3;
        break;

    case JOB_REAVER:
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        if (!_choose_weapon())
            return false;

        weap_skill = 3;

        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        if (!_choose_book( you.inv[2], BOOK_CONJURATIONS_I, 2 ))
            return false;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_ARMOUR] = 1;
        you.skills[SK_DODGING] = 1;

        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_CONJURATIONS] = 2;
        break;

    case JOB_STALKER:
        to_hit_bonus = random2(3);
        _newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, 1,
                           1 + to_hit_bonus, 1 + (2 - to_hit_bonus));
        _newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _newgame_make_item(2, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);
        _newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_STALKING);

        you.skills[SK_FIGHTING] = 1;
        you.skills[SK_SHORT_BLADES] = 1;
        you.skills[SK_POISON_MAGIC] = 1;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 2;
        you.skills[SK_STABBING] = 2;
        you.skills[SK_DODGING + random2(3)]++;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_ENCHANTMENTS] = 1;
        break;

    case JOB_MONK:
        _newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        you.equip[EQ_WEAPON] = -1;

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_UNARMED_COMBAT] = 4;
        you.skills[SK_DODGING] = 3;
        you.skills[SK_STEALTH] = 2;
        break;

    case JOB_WANDERER:
        _create_wanderer();
        break;

    default:
        break;
    }

    // Vampires always start with unarmed combat skill.
    if (you.species == SP_VAMPIRE && you.skills[SK_UNARMED_COMBAT] < 2)
        you.skills[SK_UNARMED_COMBAT] = 2;

    if (weap_skill)
        you.skills[weapon_skill(OBJ_WEAPONS, you.inv[0].sub_type)] = weap_skill;

    init_skill_order();

    if (you.religion != GOD_NO_GOD)
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
    }
    return true;
}

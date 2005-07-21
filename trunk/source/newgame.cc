/*
 *  File:       newgame.cc
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
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
#endif

#ifdef LINUX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef USE_EMX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef OS9
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "dungeon.h"
#include "files.h"
#include "fight.h"
#include "itemname.h"
#include "items.h"
#include "macro.h"
#include "player.h"
#include "randart.h"
#include "skills.h"
#include "skills2.h"
#include "spl-util.h"
#include "stuff.h"
#include "version.h"
#include "wpn-misc.h"


#define MIN_START_STAT       1

bool class_allowed(unsigned char speci, int char_class);
bool verifyPlayerName(void);
void choose_weapon(void);
void enterPlayerName(bool blankOK);
void give_basic_knowledge(int which_job);
void give_basic_spells(int which_job);
void give_last_paycheck(int which_job);
void init_player(void);
void jobs_stat_init(int which_job);
void openingScreen(void);
void species_stat_init(unsigned char which_species);

#if 0
// currently unused -- bwr
static void give_random_wand( int slot );
static void give_random_scroll( int slot );
#endif

static void give_random_potion( int slot );
static void give_random_secondary_armour( int slot );
static bool give_wanderer_weapon( int slot, int wpn_skill );
static void create_wanderer(void);
static void give_items_skills(void);
static bool choose_race(void);
static bool choose_class(void);

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
        if (sp >= SP_WHITE_DRACONIAN && sp <= SP_UNK2_DRACONIAN)
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
    char char_fil[kFileNameSize];

#ifdef LOAD_UNPACKAGE_CMD
    // Create the file name base
    char name_buff[kFileNameLen];

    snprintf( name_buff, sizeof(name_buff), 
              SAVE_DIR_PATH "%s%d", you.your_name, (int) getuid() );

    char zip_buff[kFileNameLen];

    strcpy(zip_buff, name_buff);
    strcat(zip_buff, PACKAGE_SUFFIX);

    // Create save dir name
    strcpy(char_fil, name_buff);
    strcat(char_fil, ".sav");

    handle = fopen(zip_buff, "rb+");
    if (handle != NULL)
    {
        cprintf(EOL "Loading game..." EOL);

        // Create command
        char cmd_buff[1024];

        snprintf( cmd_buff, sizeof(cmd_buff), LOAD_UNPACKAGE_CMD, name_buff );

        if (system( cmd_buff ) != 0)
        {
            cprintf( EOL "Warning: Zip command (LOAD_UNPACKAGE_CMD) returned non-zero value!" EOL );
        }

        fclose(handle);

        // Remove save game package
        unlink(zip_buff);
    }
    else
    {
#ifdef DO_ANTICHEAT_CHECKS
        // Simple security patch -- must have zip file otherwise invalidate
        // the character.  Right now this just renames the .sav file to
        // .bak, allowing anyone with the appropriate permissions to
        // fix a character in the case of a bug.  This could be changed
        // to unlinking the file(s) which would remove the character.
        strcat(name_buff, ".bak");
        rename(char_fil, name_buff);
#endif
    }

#else
    strcpy(char_fil, "");
    strncat(char_fil, you.your_name, kFileNameLen);
    strcat(char_fil, ".sav");
#endif

    handle = fopen(char_fil, "rb+");

    if (handle != NULL)
    {
        fclose(handle);
        return true;
    }
    return false;
}

bool new_game(void)
{
    int i, j;                   // loop variables {dlb}

    //jmf: NEW ASSERTS: we ought to do a *lot* of these
    ASSERT(NUM_SPELLS < SPELL_NO_SPELL);
    ASSERT(NUM_DURATIONS > DUR_LAST_DUR);
    ASSERT(NUM_JOBS < JOB_UNKNOWN);
    ASSERT(NUM_ATTRIBUTES < 30);

    init_player();

    you.exp_available = 25;     // now why is this all the way up here? {dlb}

    textcolor(LIGHTGREY);

    // copy name into you.your_name if set from environment --
    // note that you.your_name could already be set from init.txt
    // this, clearly, will overwrite such information {dlb}
    if (SysEnv.crawl_name)
    {
        strncpy( you.your_name, SysEnv.crawl_name, kNameLen );
        you.your_name[ kNameLen - 1 ] = '\0';
    }

    openingScreen();
    enterPlayerName(true);

    if (you.your_name[0] != '\0')
    {
        if (check_saved_game())
        {
            textcolor( BROWN );
            cprintf( EOL "Welcome back, " );
            textcolor( YELLOW );
            cprintf( you.your_name );
            cprintf( "!" );
            textcolor( LIGHTGREY );

            return (false);
        }
    }

    if (Options.random_pick)
    {
        pick_random_species_and_class();
    }
    else
    {
        bool keep_going = true;
        while (keep_going)
        {
            if (choose_race())
                keep_going = !choose_class();
            else
                keep_going = false;
        }
    }

    strcpy( you.class_name, get_class_name( you.char_class ) );

    // new: pick name _after_ race and class choices
    if (you.your_name[0] == '\0')
    {
        clrscr();

        char spec_buff[80];
        strncpy(spec_buff, species_name(you.species, you.experience_level), 80);

        snprintf( info, INFO_SIZE, "You are a%s %s %s." EOL, 
                  (is_vowel( spec_buff[0] )) ? "n" : "", spec_buff,
                  you.class_name );

        cprintf( info );

        enterPlayerName(false);

        if (check_saved_game())
        {
            cprintf(EOL "Do you really want to overwrite your old game?");
            char c = getch();
            if (!(c == 'Y' || c == 'y'))
            {
                textcolor( BROWN );
                cprintf(EOL EOL "Welcome back, ");
                textcolor( YELLOW );
                cprintf(you.your_name);
                cprintf("!");
                textcolor( LIGHTGREY );

                return (false);
            }
        }
    }


// ************ round-out character statistics and such ************

    species_stat_init( you.species );     // must be down here {dlb}

    you.is_undead = ((you.species == SP_MUMMY) ? US_UNDEAD :
                     (you.species == SP_GHOUL) ? US_HUNGRY_DEAD : US_ALIVE);

    // before we get into the inventory init,  set light radius based
    // on species vision. currently,  all species see out to 8 squares.
    you.normal_vision = 8;
    you.current_vision = 8;

    jobs_stat_init( you.char_class );
    give_last_paycheck( you.char_class );

    // randomly boost stats a number of times based on species
    // - should be a function {dlb}
    unsigned char points_left = (you.species == SP_DEMIGOD
                                 || you.species == SP_DEMONSPAWN) ? 15 : 8;

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

    // this function depends on stats being finalized
    give_items_skills();

    // then: adjust hp_max by species {dlb}
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

    // then: adjust max_magic_points by species {dlb}
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

    // these need to be set above using functions!!! {dlb}
    you.max_dex = you.dex;
    you.max_strength = you.strength;
    you.max_intel = you.intel;

    if (!you.is_undead)
    {
        for (i = 0; i < ENDOFPACK; i++)
        {
            if (!you.inv[i].quantity)
            {
                you.inv[i].quantity = 1;
                you.inv[i].base_type = OBJ_FOOD;
                you.inv[i].sub_type = FOOD_BREAD_RATION;

                if (you.species == SP_HILL_ORC || you.species == SP_KOBOLD
                    || you.species == SP_OGRE || you.species == SP_TROLL)
                {
                    you.inv[i].sub_type = FOOD_MEAT_RATION;
                }

                you.inv[i].colour = BROWN;
                break;
            }
        }
    }

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].quantity)
        {
            if (you.inv[i].base_type == OBJ_BOOKS)
            {
                you.had_book[you.inv[i].sub_type] = 1;
                if (you.inv[i].sub_type == BOOK_MINOR_MAGIC_I
                    || you.inv[i].sub_type == BOOK_MINOR_MAGIC_II
                    || you.inv[i].sub_type == BOOK_MINOR_MAGIC_III)
                {
                    you.had_book[BOOK_MINOR_MAGIC_I] = 1;
                    you.had_book[BOOK_MINOR_MAGIC_II] = 1;
                    you.had_book[BOOK_MINOR_MAGIC_III] = 1;
                }
                if (you.inv[i].sub_type == BOOK_CONJURATIONS_I
                    || you.inv[i].sub_type == BOOK_CONJURATIONS_II)
                {
                    you.had_book[BOOK_CONJURATIONS_I] = 1;
                    you.had_book[BOOK_CONJURATIONS_II] = 1;
                }
            }

            // don't change object type modifier unless it starts plain
            if (you.inv[i].base_type <= OBJ_ARMOUR
                && cmp_equip_race( you.inv[i], 0 ))   // == DARM_PLAIN
            {
                // now add appropriate species type mod:
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
                    if (you.inv[i].colour == LIGHTCYAN)
                        you.inv[i].colour = CYAN;
                    break;

                case SP_HILL_ORC:
                    set_equip_race( you.inv[i], ISFLAG_ORCISH );
                    break;
                }
            }
        }
    }

    // must remember to check for already existing colours/combinations
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 50; j++)
        {
            you.item_description[i][j] = 255;
        }
    }

    you.item_description[IDESC_POTIONS][POT_PORRIDGE] = 153;  // "gluggy white"
    you.item_description[IDESC_POTIONS][POT_WATER] = 0;       // "clear"

    int passout;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 50; j++)
        {
            if (you.item_description[i][j] != 255)
                continue;

            do
            {
                passout = 1;

                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2( 16 * 12 );
                    if (coinflip())
                        you.item_description[i][j] %= 12;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = random2( 15 * 14 );
                    if (coinflip())
                        you.item_description[i][j] %= 14;
                    break;

                case IDESC_SCROLLS: // scrolls
                    you.item_description[i][j] = random2(151);
                    you.item_description[IDESC_SCROLLS_II][j] = random2(151);
                    break;

                case IDESC_RINGS: // rings
                    you.item_description[i][j] = random2( 13 * 13 );
                    if (coinflip())
                        you.item_description[i][j] %= 13;
                    break;
                }

                // don't have p < j because some are preassigned
                for (int p = 0; p < 50; p++)
                {
                    if (you.item_description[i][p] == you.item_description[i][j]
                        && j != p)
                    {
                        passout = 0;
                    }
                }
            }
            while (passout == 0);
        }
    }

    for (i = 0; i < 50; i++)
    {
        if (!you.skills[i])
            continue;

        // Start with the amount of skill points required for a human...
        const int points = skill_exp_needed( you.skills[i] + 1 );

        you.skill_points[i] = points + 1;

        if (i == SK_SPELLCASTING)
            you.skill_points[i] = (points * 130) / 100 + 1;
        else if (i == SK_INVOCATIONS || i == SK_EVOCATIONS)
            you.skill_points[i] = (points * 75) / 100 + 1;

        // ...and find out what level that earns this character.
        const int sp_diff = species_skills( i, you.species );
        you.skills[i] = 0;

        for (int lvl = 1; lvl <= 8; lvl++) 
        {
            if (you.skill_points[i] > (skill_exp_needed(lvl+1) * sp_diff) / 100)
                you.skills[i] = lvl;
            else
                break;
        }
    }

    calc_total_skill_points();

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].base_type != OBJ_WEAPONS)
        {
            set_ident_type( you.inv[i].base_type, 
                            you.inv[i].sub_type, ID_KNOWN_TYPE );
        }

        if (you.inv[i].base_type == OBJ_POTIONS
            || you.inv[i].base_type == OBJ_WANDS
            || you.inv[i].base_type == OBJ_JEWELLERY)
        {
            item_colour( you.inv[i] );  // set correct special and colour
        }
    }

    // we calculate hp and mp here;  all relevant factors should be
    // finalized by now (GDL)
    calc_hp();
    calc_mp();

    // make sure the starting player is fully charged up
    set_hp( you.hp_max, false );
    set_mp( you.max_magic_points, false ); 

    give_basic_spells(you.char_class);
    give_basic_knowledge(you.char_class);

    // tmpfile purging removed in favour of marking
    for (int lvl = 0; lvl < MAX_LEVELS; lvl++)
    {
        for (int dng = 0; dng < MAX_BRANCHES; dng++)
        {
            tmp_file_pairs[lvl][dng] = false;
        }
    }

    // places staircases to the branch levels:
    for (i = 0; i < 30; i++)
    {
        you.branch_stairs[i] = 100;
    }

    you.branch_stairs[STAIRS_ECUMENICAL_TEMPLE] = 3 + random2(4); // avg:  4.5

    you.branch_stairs[STAIRS_ORCISH_MINES] = 5 + random2(6);    // avg:  7.5

    you.branch_stairs[STAIRS_ELVEN_HALLS] =
        you.branch_stairs[STAIRS_ORCISH_MINES] + (coinflip() ? 4 : 3);  // 11.0

    you.branch_stairs[STAIRS_LAIR] = 7 + random2(6);    // avg:  9.5

    you.branch_stairs[STAIRS_HIVE] = 10 + random2(6);   // avg: 12.5

    you.branch_stairs[STAIRS_SLIME_PITS] =
        you.branch_stairs[STAIRS_LAIR] + 3 + random2(4);        // avg: 14.0

    you.branch_stairs[STAIRS_SWAMP] =
        you.branch_stairs[STAIRS_LAIR] + 2 + random2(6);        // avg: 14.0

    you.branch_stairs[STAIRS_SNAKE_PIT] =
        you.branch_stairs[STAIRS_LAIR] + (coinflip() ? 7 : 6);   // avg: 16.0

    you.branch_stairs[STAIRS_VAULTS] = 13 + random2(6); // avg: 15.5

    you.branch_stairs[STAIRS_CRYPT] =
        you.branch_stairs[STAIRS_VAULTS] + 2 + random2(3);      // avg: 18.5

    you.branch_stairs[STAIRS_HALL_OF_BLADES] =
        you.branch_stairs[STAIRS_VAULTS] + 4;   // avg: 19.5

    you.branch_stairs[STAIRS_TOMB] =
        you.branch_stairs[STAIRS_CRYPT] + ((coinflip()) ? 3 : 2);   // avg: 20.0

    you.branch_stairs[STAIRS_HALL_OF_ZOT] = 26; // always 26

    return (true);
}                               // end of new_game()

static bool species_is_undead( unsigned char speci )
{
    return (speci == SP_MUMMY || speci == SP_GHOUL);
}

bool class_allowed( unsigned char speci, int char_class )
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
            return true;
        }
        return false;

    case JOB_QUITTER:   // shouldn't happen since 'x' is handled specially
    default:
        return false;
    }
}                               // end class_allowed()

static char startwep[5] = { WPN_SHORT_SWORD, WPN_MACE,
    WPN_HAND_AXE, WPN_SPEAR, WPN_TRIDENT };

void choose_weapon( void )
{
    char wepName[ ITEMNAME_SIZE ];
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
        return;
    }

    if (!Options.random_pick && Options.weapon != WPN_RANDOM)
    {
        clrscr();

        textcolor( CYAN );
        cprintf(EOL " You have a choice of weapons:" EOL);
        textcolor( LIGHTGREY );

        for(int i=0; i<num_choices; i++)
        {
            int x = effective_stat_bonus(startwep[i]);
            standard_name_weap(startwep[i], wepName);

            snprintf( info, INFO_SIZE, "%c - %s%s" EOL, 'a' + i, wepName,
                      (x <= -4) ? " (not ideal)" : "" );

            cprintf(info);
        }

        cprintf(EOL "? - Random" EOL);

        do
        {
            textcolor( CYAN );
            cprintf(EOL "Which weapon? ");
            textcolor( LIGHTGREY );

            keyin = get_ch();
        }
        while (keyin != '?' && (keyin < 'a' || keyin > ('a' + num_choices)));

        if (keyin != '?' && effective_stat_bonus(startwep[keyin-'a']) > -4)
            cprintf(EOL "A fine choice. " EOL);
    }

    if (Options.random_pick || Options.weapon == WPN_RANDOM || keyin == '?')
    {
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
}

void init_player(void)
{
    unsigned char i = 0;        // loop variable

    you.birth_time = time( NULL );
    you.real_time = 0;
    you.num_turns = 0;

#ifdef WIZARD
    you.wizard = (Options.wiz_mode == WIZ_YES) ? true : false;
#else
    you.wizard = false;
#endif

    you.berserk_penalty = 0;
    you.berserker = 0;
    you.conf = 0;
    you.confusing_touch = 0;
    you.deaths_door = 0;
    you.disease = 0;
    you.elapsed_time = 0;
    you.exhausted = 0;
    you.haste = 0;
    you.invis = 0;
    you.levitation = 0;
    you.might = 0;
    you.paralysis = 0;
    you.poison = 0;
    you.rotting = 0;
    you.fire_shield = 0;
    you.slow = 0;
    you.special_wield = SPWLD_NONE;
    you.sure_blade = 0;
    you.synch_time = 0;

    you.base_hp = 5000;
    you.base_hp2 = 5000;
    you.base_magic_points = 5000;
    you.base_magic_points2 = 5000;

    you.magic_points_regeneration = 0;
    you.strength = 0;
    you.max_strength = 0;
    you.intel = 0;
    you.max_intel = 0;
    you.dex = 0;
    you.max_dex = 0;
    you.experience = 0;
    you.experience_level = 1;
    you.max_level = 1;
    you.char_class = JOB_FIGHTER;

    you.hunger = 6000;
    you.hunger_state = HS_SATIATED;

    you.gold = 0;
    // you.speed = 10;             // 0.75;  // unused

    you.burden = 0;
    you.burden_state = BS_UNENCUMBERED;

    you.spell_no = 0;

    you.your_level = 0;
    you.level_type = LEVEL_DUNGEON;
    you.where_are_you = BRANCH_MAIN_DUNGEON;
    you.char_direction = DIR_DESCENDING;

    you.prev_targ = MHITNOT;
    you.pet_target = MHITNOT;

    you.x_pos = 0;
    you.y_pos = 0;

    you.running = 0;
    you.run_x = 0;
    you.run_y = 0;
    for (i = 0; i < 3; i++)
    {
        you.run_check[i].grid = 0;
        you.run_check[i].dx = 0;
        you.run_check[i].dy = 0;
    }

    you.religion = GOD_NO_GOD;
    you.piety = 0;

    you.gift_timeout = 0;

    for (i = 0; i < MAX_NUM_GODS; i++)
    {
        you.penance[i] = 0;
        you.worshipped[i] = 0;
    }

    ghost.name[0] = '\0';

    for (i = 0; i < NUM_GHOST_VALUES; i++)
        ghost.values[i] = 0;

    for (i = EQ_WEAPON; i < NUM_EQUIP; i++)
        you.equip[i] = -1;

    for (i = 0; i < 25; i++)
        you.spells[i] = SPELL_NO_SPELL;

    for (i = 0; i < 52; i++)
    {
        you.spell_letter_table[i] = -1;
        you.ability_letter_table[i] = ABIL_NON_ABILITY;
    }

    for (i = 0; i < 100; i++)
        you.mutation[i] = 0;

    for (i = 0; i < 100; i++)
        you.demon_pow[i] = 0;

    for (i = 0; i < 50; i++)
        you.had_book[i] = 0;

    for (i = 0; i < 50; i++)
        you.unique_items[i] = UNIQ_NOT_EXISTS;

    for (i = 0; i < NO_UNRANDARTS; i++)
        set_unrandart_exist(i, 0);

    for (i = 0; i < 50; i++)
    {
        you.skills[i] = 0;
        you.skill_points[i] = 0;
        you.skill_order[i] = MAX_SKILL_ORDER;
        you.practise_skill[i] = 1;
    }

    you.skill_cost_level = 1;
    you.total_skill_points = 0;

    for (i = 0; i < 30; i++)
        you.attribute[i] = 0;

    for (i = 0; i < ENDOFPACK; i++)
    {
        you.inv[i].quantity = 0;
        you.inv[i].base_type = OBJ_WEAPONS;
        you.inv[i].sub_type = WPN_CLUB;
        you.inv[i].plus = 0;
        you.inv[i].plus2 = 0;
        you.inv[i].special = 0;
        you.inv[i].colour = 0;
        set_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );

        you.inv[i].x = -1;
        you.inv[i].y = -1;
        you.inv[i].link = i;
    }

    for (i = 0; i < NUM_DURATIONS; i++)
        you.duration[i] = 0;
}

void give_last_paycheck(int which_job)
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
// that demonspawn & demingods get more later on {dlb}
void species_stat_init(unsigned char which_species)
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
    case SP_UNK2_DRACONIAN:     sb =  9; ib =  6; db =  2;      break;  // 17
    }

    modify_all_stats( sb, ib, db );
}

void jobs_stat_init(int which_job)
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

void give_basic_knowledge(int which_job)
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

void give_basic_spells(int which_job)
{
    // wanderers may or may not already have a spell -- bwr
    if (which_job == JOB_WANDERER)
        return;

    unsigned char which_spell = SPELL_NO_SPELL;

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
void openingScreen(void)
{
/* **********************************************
// this does not work just yet ... {dlb}:
    cprintf(EOL "Hello, ");

    if ( you.your_name[0] != '\0' )
    {
       cprintf(you.your_name); // better be less than 31 characters :P {dlb}
                               // of course, invalid names will appear {dlb}
       cprintf(", ");
    }
********************************************** */

    textcolor( YELLOW );
    cprintf("Hello, welcome to Dungeon Crawl " VERSION "!");
    textcolor( BROWN );
    cprintf(EOL "(c) Copyright 1997-2002 Linley Henzell");
    cprintf(EOL "Please consult crawl.txt for instructions and legal details."
            EOL);
    textcolor( LIGHTGREY );

    return;
}                               // end openingScreen()


void enterPlayerName(bool blankOK)
{
    // temporary 'til copyover to you.your_name {dlb}
    // made this rediculously long so that the game doesn't
    // crash if a really really long name is entered (argh).  {gdl}
    char name_entered[200];

    // anything to avoid goto statements {dlb}
    bool acceptable_name = false;
    bool first_time = true;

    // first time -- names set through init.txt/environment assumed ok {dlb}
    if (you.your_name[0] != '\0')
        acceptable_name = true;

    do
    {
        // prompt for a new name if current one unsatisfactory {dlb}:
        if (!acceptable_name)
        {
            textcolor( CYAN );
            if (blankOK && first_time)
                cprintf(EOL "Press <Enter> to answer this after race and class are chosen."EOL);

            first_time = false;

            cprintf(EOL "What is your name today? ");
            textcolor( LIGHTGREY );
            get_input_line( name_entered, sizeof( name_entered ) );
            
            strncpy( you.your_name, name_entered, kNameLen );
            you.your_name[ kNameLen - 1 ] = '\0';
        }

        // verification begins here {dlb}:
        if (you.your_name[0] == '\0')
        {
            if (blankOK)
                return;

            cprintf(EOL "That's a silly name!" EOL);
            acceptable_name = false;
        }

        // if SAVE_DIR_PATH is defined, userid will be tacked onto the end
        // of each character's files, making bones a valid player name.
#ifndef SAVE_DIR_PATH
        // this would cause big probs with ghosts
        // what would? {dlb}
        // ... having the name "bones" of course! The problem comes from
        // the fact that bones files would have the exact same filename
        // as level files for a character named "bones".  -- bwr
        else if (stricmp(you.your_name, "bones") == 0)
        {
            cprintf(EOL "That's a silly name!" EOL);
            acceptable_name = false;
        }
#endif
        else
            acceptable_name = verifyPlayerName();

    }
    while (!acceptable_name);
}                               // end enterPlayerName()

bool verifyPlayerName(void)
{
#if defined(DOS) || defined(WIN32CONSOLE)
    static int william_tanksley_asked_for_this = 2;

    // quick check for CON -- blows up real good under DOS/Windows
    if (stricmp(you.your_name, "con") == 0)
    {
        cprintf(EOL "Sorry, that name gives your OS a headache." EOL);
        return (false);
    }

    // quick check for LPTx -- thank you,  Mr. Tanksley!   ;-)
    if (strnicmp(you.your_name, "LPT", 3) == 0)
    {
        switch (william_tanksley_asked_for_this)
        {
            case 2:
                cprintf(EOL "Hello, William!  How is work on Omega going?" EOL);
                break;
            case 1:
                cprintf(EOL "Look, it's just not a legal name." EOL);
                break;
            case 0:
                strcpy(you.your_name, "William");
                return (true);
        } // end switch

        william_tanksley_asked_for_this --;
        return (false);
    }
#endif

    const size_t len = strlen( you.your_name );
    for (unsigned int i = 0; i < len; i++)
    {
#if MAC
        // the only bad character on Macs is the path seperator
        if (you.your_name[i] == ':')
        {
            cprintf(EOL "No colons, please." EOL);
            return (false);
        }
#else
        // Note that this includes systems which may be using the
        // packaging system.  The packaging system is very simple 
        // and doesn't take the time to escape every characters that
        // might be a problem for some random shell or OS... so we 
        // play it very conservative here.  -- bwr
        if (!isalnum( you.your_name[i] ) && you.your_name[i] != '_')
        {
            cprintf( EOL "Alpha-numerics and underscores only, please." EOL );
            return (false);
        }
#endif
    }

#ifdef SAVE_DIR_PATH
    // Until we have a better way to handle the fact that this could lead 
    // to some confusion with where the name ends and the uid begins. -- bwr
    if (isdigit( you.your_name[ len - 1 ] ))
    {
        cprintf( EOL "Sorry, your name cannot end with a digit." EOL );
        return (false);
    }
#endif

    return (true);
}                               // end verifyPlayerName()

#if 0
// currently unused
static void give_random_scroll( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_SCROLLS;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].special = 0;
    you.inv[ slot ].colour = WHITE;

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

#if 0
// currently unused
static void give_random_wand( int slot )
{
    you.inv[ slot ].quantity = 1;
    you.inv[ slot ].base_type = OBJ_WANDS;
    you.inv[ slot ].special = 0;
    you.inv[ slot ].plus2 = 0;
    you.inv[ slot ].colour = random_colour();

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
    you.inv[ slot ].colour = BROWN;

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
    you.inv[ slot ].colour = LIGHTCYAN;
    you.inv[ slot ].plus = 0;
    you.inv[ slot ].plus2 = 0;
    you.inv[ slot ].special = 0;

    // Now fill in the type according to the random wpn_skill
    switch (wpn_skill)
    {
    case SK_MACES_FLAILS:
        you.inv[ slot ].sub_type = WPN_CLUB;
        you.inv[ slot ].colour = BROWN;
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
        you.inv[ slot ].colour = BROWN;
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
        { SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_util_skills = sizeof(util_skills) / sizeof(int);

    // Long swords is missing to increae it's rarity because we
    // can't give out a long sword to a starting character (they're
    // all too good)... Staves is also removed because it's not
    // one of the fighter options.-- bwr
    const int fight_util_skills[] =
        { SK_FIGHTING, SK_SHORT_BLADES, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS,
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
          SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS, SK_UNARMED_COMBAT,
          SK_INVOCATIONS, SK_EVOCATIONS };
    const int num_fight_util_skills = sizeof(fight_util_skills) / sizeof(int);

    const int not_rare_skills[] =
        { SK_SLINGS, SK_BOWS, SK_CROSSBOWS,
          SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS,
          SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,
          SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_SWORDS, SK_AXES,
          SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES,
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
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
          SK_DARTS, SK_THROWING, SK_ARMOUR, SK_DODGING, SK_STEALTH,
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

    int wpn_skill = SK_FIGHTING;  // prefered weapon type
    int wpn_skill_size = 0;       // level of skill in prefered weapon type
    int num_wpn_skills = 0;       // used to choose prefered weapon
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
    you.inv[0].colour = LIGHTCYAN;
    you.inv[0].plus = 0;
    you.inv[0].plus2 = 0;
    you.inv[0].special = 0;

    // And a default armour template for a robe (leaving slot 1 open for
    // a secondary weapon).
    you.inv[2].quantity = 1;
    you.inv[2].base_type = OBJ_ARMOUR;
    you.inv[2].sub_type = ARM_ROBE;
    you.inv[2].colour = BROWN;
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
            you.inv[2].colour = LIGHTCYAN;

            you.inv[3].quantity = 0;            // remove potion
        }
        else if (you.skills[SK_SHIELDS] && wpn_skill != SK_STAVES)
        {
            you.inv[4].quantity = 1;
            you.inv[4].base_type = OBJ_ARMOUR;
            you.inv[4].sub_type = ARM_BUCKLER;
            you.inv[4].plus = 0;
            you.inv[4].special = 0;
            you.inv[4].colour = LIGHTCYAN;
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
    else if (you.skills[ SK_THROWING ] && one_chance_in(3)) // these are rare
    {
        // Ranger style wanderer
        // Rare since starting with a throwing weapon is very good

        // Create a default launcher template, but the
        // quantity may be reset to 0 if we don't want one -- bwr
        // thorwing weapons are lowered to -1 to make them
        // not as good as the one's hunters get, ammo is
        // also much smaller -- bwr
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_BOW;
        you.inv[1].plus = -1;
        you.inv[1].plus2 = -1;
        you.inv[1].special = 0;
        you.inv[1].colour = BROWN;

        // Create default ammo template (darts) (armour is slot 2)
        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 6 );
        you.inv[4].plus = 0;
        you.inv[4].plus2 = 0;
        you.inv[4].special = 0;
        you.inv[4].colour = LIGHTCYAN;

        if (you.skills[ SK_SLINGS ])
        {
            // slingers get some extra ammo
            you.inv[4].quantity += random2avg(20,5);
            you.inv[4].sub_type = MI_STONE;
            you.inv[4].colour = BROWN;
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
            you.inv[4].colour = BROWN;
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

// choose_race returns true if the player should also pick a class.
// This is done because of the '*' option which will pick a random
// character, obviating the necessity of choosing a class.

bool choose_race()
{
    char keyn;

    bool printed = false;
    if (Options.race != 0)
        printed = true;

spec_query:
    if (!printed)
    {
        clrscr();

        textcolor( WHITE );
        cprintf("You must be new here!" EOL EOL);

        textcolor( CYAN );
        cprintf("You can be:" EOL EOL);
        textcolor( LIGHTGREY );

        cprintf("a - Human                     b - Elf" EOL);
        cprintf("c - High Elf                  d - Grey Elf" EOL);
        cprintf("e - Deep Elf                  f - Sludge Elf" EOL);
        cprintf("g - Hill Dwarf                h - Mountain Dwarf" EOL);
        cprintf("i - Halfling                  j - Hill Orc" EOL);
        cprintf("k - Kobold                    l - Mummy" EOL);
        cprintf("m - Naga                      n - Gnome" EOL);
        cprintf("o - Ogre                      p - Troll" EOL);
        cprintf("q - Ogre-Mage                 r - Draconian" EOL);
        cprintf("s - Centaur                   t - Demigod" EOL);
        cprintf("u - Spriggan                  v - Minotaur" EOL);
        cprintf("w - Demonspawn                x - Ghoul" EOL);
        cprintf("y - Kenku                     z - Merfolk" EOL);

        textcolor( BROWN );
        cprintf(EOL "? - Random Species            * - Random Character" EOL);
        cprintf(    "X - Quit" EOL);

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
        keyn = getch();
        if (keyn == 0)
        {
            getch();
            goto spec_query;
        }
    }

    if (keyn == '?')
        keyn = 'a' + random2(26);
    else if (keyn == '*')
    {
        pick_random_species_and_class();
        Options.random_pick = true; // used to give random weapon/god as well
        return false;
    }


    switch (keyn)
    {
    case 'a':
        you.species = SP_HUMAN;
        break;
    case 'b':
        you.species = SP_ELF;
        break;
    case 'c':
        you.species = SP_HIGH_ELF;
        break;
    case 'd':
        you.species = SP_GREY_ELF;
        break;
    case 'e':
        you.species = SP_DEEP_ELF;
        break;
    case 'f':
        you.species = SP_SLUDGE_ELF;
        break;
    case 'g':
        you.species = SP_HILL_DWARF;
        break;
    case 'h':
        you.species = SP_MOUNTAIN_DWARF;
        break;
    case 'i':
        you.species = SP_HALFLING;
        break;
    case 'j':
        you.species = SP_HILL_ORC;
        break;
    case 'k':
        you.species = SP_KOBOLD;
        break;
    case 'l':
        you.species = SP_MUMMY;
        break;
    case 'm':
        you.species = SP_NAGA;
        break;
    case 'n':
        you.species = SP_GNOME;
        break;
    case 'o':
        you.species = SP_OGRE;
        break;
    case 'p':
        you.species = SP_TROLL;
        break;
    case 'q':
        you.species = SP_OGRE_MAGE;
        break;
    case 'r':                   // draconian
        you.species = SP_RED_DRACONIAN + random2(9);    // random drac
        break;
    case 's':
        you.species = SP_CENTAUR;
        break;
    case 't':
        you.species = SP_DEMIGOD;
        break;
    case 'u':
        you.species = SP_SPRIGGAN;
        break;
    case 'v':
        you.species = SP_MINOTAUR;
        break;
    case 'w':
        you.species = SP_DEMONSPAWN;
        break;
    case 'x':
        you.species = SP_GHOUL;
        break;
    case 'y':
        you.species = SP_KENKU;
        break;
    case 'z':
        you.species = SP_MERFOLK;
        break;
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

    // set to 0 in case we come back from choose_class()
    Options.race = 0;

    return true;
}

// returns true if a class was chosen,  false if we should go back to
// race selection.

bool choose_class(void)
{
    char keyn;
    int i,j;

    bool printed = false;
    if (Options.cls != 0)
        printed = true;
job_query:
    if (!printed)
    {
        clrscr();

        textcolor( BROWN );
        cprintf(EOL EOL);
        cprintf("Welcome, ");
        textcolor( YELLOW );
        if (strlen(you.your_name) > 0)
        {
            cprintf(you.your_name);
            cprintf(" the ");
        }
        cprintf(species_name(you.species,you.experience_level));
        cprintf("." EOL EOL);

        textcolor( CYAN );
        cprintf("You can be any of the following :" EOL);
        textcolor( LIGHTGREY );

        j = 0;               // used within for loop to determine newline {dlb}

        for (i = 0; i < NUM_JOBS; i++)
        {
            if (!class_allowed(you.species, i))
                continue;

            putch( index_to_letter(i) );
            cprintf( " - " );
            cprintf( get_class_name(i) );

            if (j % 2)
                cprintf(EOL);
            else
                gotoxy(40, wherey());

            j++;
        }

        if (wherex() >= 40)
            cprintf(EOL);

        textcolor( BROWN );
        cprintf(EOL "? - Random; x - Back to species selection; X - Quit" EOL);

        textcolor( CYAN );
        cprintf(EOL "What kind of character are you? ");
        textcolor( LIGHTGREY );

        printed = true;
    }

    if (Options.cls != 0)
    {
        keyn = Options.cls;
    }
    else
    {
        keyn = getch();
        if (keyn == 0)
        {
            getch();
            goto job_query;
        }
    }

    if (keyn == 'a')
        you.char_class = JOB_FIGHTER;
    else if (keyn == 'b')
        you.char_class = JOB_WIZARD;
    else if (keyn == 'c')
        you.char_class = JOB_PRIEST;
    else if (keyn == 'd')
        you.char_class = JOB_THIEF;
    else if (keyn == 'e')
        you.char_class = JOB_GLADIATOR;
    else if (keyn == 'f')
        you.char_class = JOB_NECROMANCER;
    else if (keyn == 'g')
        you.char_class = JOB_PALADIN;
    else if (keyn == 'h')
        you.char_class = JOB_ASSASSIN;
    else if (keyn == 'i')
        you.char_class = JOB_BERSERKER;
    else if (keyn == 'j')
        you.char_class = JOB_HUNTER;
    else if (keyn == 'k')
        you.char_class = JOB_CONJURER;
    else if (keyn == 'l')
        you.char_class = JOB_ENCHANTER;
    else if (keyn == 'm')
        you.char_class = JOB_FIRE_ELEMENTALIST;
    else if (keyn == 'n')
        you.char_class = JOB_ICE_ELEMENTALIST;
    else if (keyn == 'o')
        you.char_class = JOB_SUMMONER;
    else if (keyn == 'p')
        you.char_class = JOB_AIR_ELEMENTALIST;
    else if (keyn == 'q')
        you.char_class = JOB_EARTH_ELEMENTALIST;
    else if (keyn == 'r')
        you.char_class = JOB_CRUSADER;
    else if (keyn == 's')
        you.char_class = JOB_DEATH_KNIGHT;
    else if (keyn == 't')
        you.char_class = JOB_VENOM_MAGE;
    else if (keyn == 'u')
        you.char_class = JOB_CHAOS_KNIGHT;
    else if (keyn == 'v')
        you.char_class = JOB_TRANSMUTER;
    else if (keyn == 'w')
        you.char_class = JOB_HEALER;
    else if (keyn == 'y')
        you.char_class = JOB_REAVER;
    else if (keyn == 'z')
        you.char_class = JOB_STALKER;
    else if (keyn == 'A')
        you.char_class = JOB_MONK;
    else if (keyn == 'B')
        you.char_class = JOB_WARPER;
    else if (keyn == 'C')
        you.char_class = JOB_WANDERER;
    else if (keyn == '?')
    {
        // pick a job at random... see god retribution for proof this
        // is uniform. -- bwr
        int job_count = 0;
        int job = -1;

        for (int i = 0; i < NUM_JOBS; i++)
        {
            if (class_allowed(you.species, i))
            {
                job_count++;
                if (one_chance_in( job_count ))
                    job = i;
            }
        }

        ASSERT( job != -1 );   // at least one class should have been allowed
        you.char_class = job;
    }
    else if (keyn == 'x' || keyn == ESCAPE)
    {
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

    if (!class_allowed(you.species, you.char_class))
    {
        if (Options.cls != 0)
        {
            Options.cls = 0;
            printed = false;
        }
        goto job_query;
    }

    return true;
}


void give_items_skills()
{
    char keyn;
    int weap_skill = 0;
    int to_hit_bonus;           // used for assigning primary weapons {dlb}
    int choice;                 // used for third-screen choices
    int tmp;

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        you.inv[0].colour = LIGHTCYAN;

        if (you.species == SP_OGRE || you.species == SP_TROLL
            || player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = BROWN;

            if (you.species == SP_OGRE)
            {
                you.inv[0].quantity = 1;
                you.inv[0].base_type = OBJ_WEAPONS;
                you.inv[0].sub_type = WPN_CLUB;
                you.inv[0].plus = 0;
                you.inv[0].special = 0;
                you.inv[0].colour = BROWN;

            }
            else if (you.species == SP_TROLL)
            {
                you.inv[0].quantity = 0;
                you.inv[0].base_type = OBJ_WEAPONS;
                you.inv[0].sub_type = WPN_CLUB;
                you.inv[0].plus = 0;
                you.inv[0].special = 0;
                you.inv[0].colour = BROWN;

            }
            else if (player_genus(GENPC_DRACONIAN))
            {
                you.inv[2].quantity = 1;
                you.inv[2].base_type = OBJ_ARMOUR;
                you.inv[2].sub_type = ARM_SHIELD;
                you.inv[2].plus = 0;
                you.inv[2].special = 0;
                you.inv[2].colour = LIGHTCYAN;
            }
        }
        else if (you.species == SP_GHOUL || you.species == SP_MUMMY)
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ROBE;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = WHITE;  // grave shroud

            if (you.species == SP_MUMMY)
            {
                you.inv[2].quantity = 1;
                you.inv[2].base_type = OBJ_ARMOUR;
                you.inv[2].sub_type = ARM_SHIELD;
                you.inv[2].plus = 0;
                you.inv[2].special = 0;
                you.inv[2].colour = LIGHTCYAN;
            }
        }
        else if (you.species == SP_KOBOLD)
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_LEATHER_ARMOUR;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = BROWN;

            you.inv[2].base_type = OBJ_MISSILES;
            you.inv[2].sub_type = MI_DART;
            you.inv[2].quantity = 10 + roll_dice( 2, 10 );
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
            you.inv[2].colour = LIGHTCYAN;

        }
        else
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_SCALE_MAIL;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = LIGHTCYAN;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_SHIELD;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
            you.inv[2].colour = LIGHTCYAN;

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

        weap_skill = 2;

        if (you.species == SP_KOBOLD)
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
            // starting armour now (note: the armour has to be quiped
            // for this function to work)

            you.skills[(player_light_armour()? SK_DODGING : SK_ARMOUR)] = 2;

            you.skills[SK_SHIELDS] = 2;
            you.skills[SK_THROWING] = 2;
            you.skills[(coinflip() ? SK_STABBING : SK_SHIELDS)]++;
        }
        break;

    case JOB_WIZARD:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;

        if (you.species == SP_OGRE_MAGE)
        {
            you.inv[0].sub_type = WPN_QUARTERSTAFF;
            you.inv[0].colour = BROWN;
        }
        else if (player_genus(GENPC_DWARVEN))
        {
            you.inv[0].sub_type = WPN_HAMMER;
            you.inv[0].colour = CYAN;
        }
        else
        {
            you.inv[0].sub_type = WPN_DAGGER;
            you.inv[0].colour = LIGHTCYAN;
        }

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


        you.inv[1].colour = random_colour();
        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        // extra items being tested:
        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_MINOR_MAGIC_I + random2(3);
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;    // = 127
        you.inv[2].special = 1;
        you.inv[2].colour = CYAN;

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
        you.inv[0].colour = LIGHTCYAN;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = WHITE;

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
            you.religion = Options.priest;
        else if (Options.random_pick || Options.priest == GOD_RANDOM)
            you.religion = coinflip() ? GOD_YREDELEMNUL : GOD_ZIN;
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " Which god do you wish to serve?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Zin (for traditional priests)" EOL);
            cprintf("b - Yredelemnul (for priests of death)" EOL);

          getkey:
            keyn = get_ch();

            switch (keyn)
            {
            case 'a':
                you.religion = GOD_ZIN;
                break;
            case 'b':
                you.religion = GOD_YREDELEMNUL;
                break;
            default:
                goto getkey;
            }
        }
        break;

    case JOB_THIEF:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        you.inv[0].colour = LIGHTCYAN;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_DAGGER;

        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = LIGHTCYAN;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = BROWN;

        you.inv[3].quantity = 1;
        you.inv[3].base_type = OBJ_ARMOUR;
        you.inv[3].sub_type = ARM_CLOAK;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;
        you.inv[3].colour = DARKGREY;

        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 10 );
        you.inv[4].plus = 0;
        you.inv[4].special = 0;
        you.inv[4].colour = LIGHTCYAN;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 2;
        you.equip[EQ_CLOAK] = 3;

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
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_SHORT_SWORD;
        choose_weapon();

        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        you.inv[0].colour = LIGHTCYAN;

        if (player_genus(GENPC_DRACONIAN))
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_ANIMAL_SKIN;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = BROWN;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_SHIELD;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
            you.inv[2].colour = LIGHTCYAN;
        }
        else
        {
            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_ARMOUR;
            you.inv[1].sub_type = ARM_RING_MAIL;
            you.inv[1].plus = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = LIGHTCYAN;

            you.inv[2].quantity = 1;
            you.inv[2].base_type = OBJ_ARMOUR;
            you.inv[2].sub_type = ARM_BUCKLER;
            you.inv[2].plus = 0;
            you.inv[2].special = 0;
            you.inv[2].colour = LIGHTCYAN;
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
        you.inv[0].colour = LIGHTCYAN;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = DARKGREY;
        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_NECROMANCY;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;    // = 127
        you.inv[2].special = 0;     // = 1;
        you.inv[2].colour = DARKGREY;

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
        you.inv[0].colour = LIGHTCYAN;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = WHITE;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_SHIELD;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = LIGHTCYAN;

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
        you.inv[0].colour = LIGHTCYAN;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_WEAPONS;
        you.inv[1].sub_type = WPN_BLOWGUN;
        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = LIGHTGREY;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = DARKGREY;

        you.inv[3].quantity = 1;
        you.inv[3].base_type = OBJ_ARMOUR;
        you.inv[3].sub_type = ARM_CLOAK;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;
        you.inv[3].colour = DARKGREY;

        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_NEEDLE;
        you.inv[4].quantity = 10 + roll_dice( 2, 10 );
        you.inv[4].plus = 0;
        you.inv[4].colour = WHITE;
        set_item_ego_type( you.inv[4], OBJ_MISSILES, SPMSL_POISONED );

        // deep elves get hand crossbows, everyone else gets blowguns
        // (deep elves tend to suck at melee and need something that
        // can do ranged damage)
        if (you.species == SP_DEEP_ELF)
        {
            you.inv[1].sub_type = WPN_HAND_CROSSBOW;
            you.inv[1].colour = BROWN;

            you.inv[4].sub_type = MI_DART;
            you.inv[4].colour = LIGHTCYAN;
        }


        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 2;
        you.equip[EQ_CLOAK] = 3;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_SHORT_BLADES] = 2;
        you.skills[SK_DODGING] = 1;
        you.skills[SK_STEALTH] = 3;
        you.skills[SK_STABBING] = 2;
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
        {
            you.inv[0].quantity = 1;
            you.inv[0].base_type = OBJ_WEAPONS;
            you.inv[0].sub_type = WPN_CLUB;
            you.inv[0].plus = 0;
            you.inv[0].plus2 = 0;
            you.inv[0].special = 0;
            you.inv[0].colour = LIGHTCYAN;
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
            you.inv[0].colour = LIGHTCYAN;
            you.equip[EQ_WEAPON] = 0;

            for (unsigned char i = 1; i <= 3; i++)
            {
                you.inv[i].quantity = 1;
                you.inv[i].base_type = OBJ_WEAPONS;
                you.inv[i].sub_type = WPN_SPEAR;
                you.inv[i].plus = 0;
                you.inv[i].plus2 = 0;
                you.inv[i].special = 0;
                you.inv[i].colour = LIGHTCYAN;
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
            you.inv[1].colour = BROWN;
            you.equip[EQ_BODY_ARMOUR] = 1;
        }
        else
        {
            you.inv[4].quantity = 1;
            you.inv[4].base_type = OBJ_ARMOUR;
            you.inv[4].sub_type = ARM_LEATHER_ARMOUR;
            you.inv[4].plus = 0;
            you.inv[4].special = 0;
            you.inv[4].colour = BROWN;
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
            you.skills[SK_THROWING] = 2;
        }
        break;

    case JOB_HUNTER:
        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_DAGGER;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;
        you.inv[0].colour = LIGHTCYAN;

        you.inv[4].quantity = 1;
        you.inv[4].base_type = OBJ_ARMOUR;
        you.inv[4].sub_type = ARM_LEATHER_ARMOUR;
        you.inv[4].plus = 0;
        you.inv[4].special = 0;
        you.inv[4].colour = BROWN;

        if (you.species != SP_MERFOLK)
        {
            you.inv[2].quantity = 15 + random2avg(21, 5);
            you.inv[2].base_type = OBJ_MISSILES;
            you.inv[2].sub_type = MI_ARROW;
            you.inv[2].plus = 0;
            you.inv[2].plus2 = 0;
            you.inv[2].special = 0;
            you.inv[2].colour = BROWN;

            you.inv[1].quantity = 1;
            you.inv[1].base_type = OBJ_WEAPONS;
            you.inv[1].sub_type = WPN_BOW;
            you.inv[1].plus = 0;
            you.inv[1].plus2 = 0;
            you.inv[1].special = 0;
            you.inv[1].colour = BROWN;
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
                you.inv[i].colour = LIGHTCYAN;
            }
        }

        if (player_genus(GENPC_DRACONIAN))
        {
            you.inv[4].sub_type = ARM_ROBE;
            you.inv[4].colour = GREEN;
        }

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 4;

        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 3;

        // Removing spellcasting -- bwr
        // you.skills[SK_SPELLCASTING] = 1;

        switch (you.species)
        {
        case SP_HALFLING:
        case SP_GNOME:
            you.inv[2].quantity += random2avg(15, 2);
            you.inv[2].sub_type = MI_STONE;
            you.inv[2].colour = BROWN;
            you.inv[1].sub_type = WPN_SLING;

            you.skills[SK_DODGING] = 2;
            you.skills[SK_STEALTH] = 2;
            you.skills[SK_SLINGS] = 2;
            break;

        case SP_HILL_DWARF:
        case SP_MOUNTAIN_DWARF:
        case SP_HILL_ORC:
            you.inv[2].sub_type = MI_BOLT;
            you.inv[2].colour = LIGHTCYAN;
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
            you.skills[SK_THROWING] += 1;
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
        you.inv[0].colour = LIGHTCYAN;
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
        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = give_first_conjuration_book();
        you.inv[2].plus = 0;

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
            you.inv[3].colour = LIGHTCYAN;
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
            you.inv[3].colour = LIGHTCYAN;

            if (you.species == SP_SPRIGGAN)
            {
                you.inv[0].base_type = OBJ_STAVES;
                you.inv[0].sub_type = STAFF_STRIKING;
                you.inv[0].colour = BROWN;
            }
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
            you.inv[3].colour = BROWN;

            if (you.species == SP_GNOME)
            {
                you.inv[1].quantity = 1;
                you.inv[1].base_type = OBJ_WEAPONS;
                you.inv[1].sub_type = WPN_SLING;
                you.inv[1].plus = 0;
                you.inv[1].plus2 = 0;
                you.inv[1].special = 0;
                you.inv[1].colour = BROWN;

                you.inv[4].quantity = 1;
                you.inv[4].base_type = OBJ_ARMOUR;
                you.inv[4].sub_type = ARM_ROBE;
                you.inv[4].plus = 0;
                you.inv[4].plus2 = 0;
                you.inv[4].special = 0;
                you.inv[4].colour = BROWN;
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
        {
            you.inv[0].sub_type = WPN_QUARTERSTAFF;
            you.inv[0].colour = BROWN;
        }
        else if (player_genus(GENPC_DWARVEN))
        {
            you.inv[0].sub_type = WPN_HAMMER;
            you.inv[0].colour = CYAN;
        }

        you.inv[2].quantity = 1;
        you.inv[2].special = 0;

        switch (you.char_class) 
        {
        case JOB_FIRE_ELEMENTALIST:  tmp = RED;                 break;
        case JOB_ICE_ELEMENTALIST:   tmp = LIGHTCYAN;           break;
        case JOB_AIR_ELEMENTALIST:   tmp = LIGHTBLUE;           break;
        case JOB_EARTH_ELEMENTALIST: tmp = BROWN;               break;
        case JOB_VENOM_MAGE:         tmp = GREEN;               break;
        default:                     tmp = random_colour();     break;
        }

        you.inv[1].colour = tmp;        // robe
        you.inv[2].colour = tmp;        // book

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
        you.inv[1].quantity = 6 + roll_dice( 3, 4 );
        you.inv[1].base_type = OBJ_MISSILES;
        you.inv[1].sub_type = MI_ARROW;
        you.inv[1].plus = 0;
        you.inv[1].plus2 = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = BROWN;

        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_ROBE;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].quantity = 1;
        you.inv[2].colour = BROWN;

        you.inv[3].base_type = OBJ_BOOKS;
        you.inv[3].sub_type = BOOK_CHANGES;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;
        you.inv[3].colour = random_colour();

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
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING] = 2;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_TRANSMIGRATION] = 2;

        if (you.species == SP_SPRIGGAN)
        {
            you.inv[0].base_type = OBJ_STAVES;
            you.inv[0].sub_type = STAFF_STRIKING;
            you.inv[0].quantity = 1;
            you.inv[0].plus = 0;
            you.inv[0].plus2 = 0;
            you.inv[0].special = 0;
            you.inv[0].colour = BROWN;

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
        you.inv[0].colour = LIGHTCYAN;

        if (you.species == SP_SPRIGGAN)
        {
            you.inv[0].base_type = OBJ_STAVES;
            you.inv[0].sub_type = STAFF_STRIKING;
            you.inv[0].colour = BROWN;

            you.skills[SK_EVOCATIONS] = 3;
        }
        else
        {
            you.inv[0].base_type = OBJ_WEAPONS;
            you.inv[0].sub_type = WPN_SHORT_SWORD;

            if (you.species == SP_OGRE_MAGE)
            {
                you.inv[0].sub_type = WPN_QUARTERSTAFF;
                you.inv[0].colour = BROWN;
            }

            weap_skill = 2;
            you.skills[SK_FIGHTING] = 1;
        }

        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_LEATHER_ARMOUR;
        you.inv[1].quantity = 1;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = BROWN;

        if (you.species == SP_SPRIGGAN || you.species == SP_OGRE_MAGE)
            you.inv[1].sub_type = ARM_ROBE;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_SPATIAL_TRANSLOCATIONS;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = random_colour();

        // one free escape:
        you.inv[3].base_type = OBJ_SCROLLS;
        you.inv[3].sub_type = SCR_BLINKING;
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;
        you.inv[3].colour = WHITE;

        you.inv[4].base_type = OBJ_MISSILES;
        you.inv[4].sub_type = MI_DART;
        you.inv[4].quantity = 10 + roll_dice( 2, 10 );
        you.inv[4].plus = 0;
        you.inv[4].special = 0;
        you.inv[4].colour = LIGHTCYAN;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        you.skills[SK_THROWING] = 1;
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
        you.inv[0].colour = LIGHTCYAN;
        choose_weapon();
        weap_skill = 2;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = random_colour();

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_WAR_CHANTS;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = random_colour();

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
        you.inv[0].colour = LIGHTCYAN;
        choose_weapon();
        weap_skill = 2;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = DARKGREY;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = BOOK_NECROMANCY;
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = DARKGREY;

        you.equip[EQ_WEAPON] = 0;
        you.equip[EQ_BODY_ARMOUR] = 1;

        choice = DK_NO_SELECTION;

        // order is important here -- bwr
        if (you.species == SP_DEMIGOD)
            choice = DK_NECROMANCY;
        else if (Options.death_knight != DK_NO_SELECTION
                && Options.death_knight != DK_RANDOM)
        {
            choice = Options.death_knight;
        }
        else if (Options.random_pick || Options.death_knight == DK_RANDOM)
            choice = (coinflip() ? DK_NECROMANCY : DK_YREDELEMNUL);
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " From where do you draw your power?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Necromantic magic" EOL);
            cprintf("b - the god Yredelemnul" EOL);

          getkey1:
            keyn = get_ch();

            switch (keyn)
            {
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
        you.piety = 25;         // irrelevant for Xom, of course
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

        you.inv[0].colour = LIGHTCYAN;
        choose_weapon();
        weap_skill = 2;
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = random2(3);
        you.inv[1].special = 0;
        you.inv[1].colour = random_colour();

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
            you.religion = Options.chaos_knight;
        }
        else if (Options.random_pick || Options.chaos_knight == GOD_RANDOM)
            you.religion = coinflip() ? GOD_XOM : GOD_MAKHLEB;
        else
        {
            clrscr();

            textcolor( CYAN );
            cprintf(EOL " Which god of chaos do you wish to serve?" EOL);

            textcolor( LIGHTGREY );
            cprintf("a - Xom of Chaos" EOL);
            cprintf("b - Makhleb the Destroyer" EOL);

          getkey2:

            keyn = get_ch();

            switch (keyn)
            {
            case 'a':
                you.religion = GOD_XOM;
                break;
            case 'b':
                you.religion = GOD_MAKHLEB;
                break;
            default:
                goto getkey2;
            }
        }

        if (you.religion == GOD_XOM)
            you.skills[SK_FIGHTING]++;
        else  // you.religion == GOD_MAKHLEB
            you.skills[SK_INVOCATIONS] = 2;

        break;

    case JOB_HEALER:
        you.religion = GOD_ELYVILON;
        you.piety = 45;

        you.inv[0].quantity = 1;
        you.inv[0].base_type = OBJ_WEAPONS;
        you.inv[0].sub_type = WPN_QUARTERSTAFF;
        you.inv[0].colour = BROWN;
        you.inv[0].plus = 0;
        you.inv[0].plus2 = 0;
        you.inv[0].special = 0;

        // Robe
        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = WHITE;

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
        you.skills[SK_THROWING] = 2;
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
        you.inv[0].colour = LIGHTCYAN;
        choose_weapon();
        weap_skill = 3;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = RED;

        you.inv[2].base_type = OBJ_BOOKS;
        you.inv[2].sub_type = give_first_conjuration_book();
        you.inv[2].quantity = 1;
        you.inv[2].plus = 0;    // = 127
        you.inv[2].special = 0;
        you.inv[2].colour = RED;

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
        you.inv[0].colour = LIGHTCYAN;

        you.inv[1].quantity = 1;
        you.inv[1].base_type = OBJ_ARMOUR;
        you.inv[1].sub_type = ARM_ROBE;
        you.inv[1].plus = 0;
        you.inv[1].special = 0;
        you.inv[1].colour = GREEN;

        you.inv[2].quantity = 1;
        you.inv[2].base_type = OBJ_ARMOUR;
        you.inv[2].sub_type = ARM_CLOAK;
        you.inv[2].plus = 0;
        you.inv[2].special = 0;
        you.inv[2].colour = DARKGREY;

        you.inv[3].base_type = OBJ_BOOKS;
        //you.inv[3].sub_type = BOOK_YOUNG_POISONERS;
        you.inv[3].sub_type = BOOK_STALKING;   //jmf: new book!
        you.inv[3].quantity = 1;
        you.inv[3].plus = 0;
        you.inv[3].special = 0;
        you.inv[3].colour = GREEN;

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
        //you.skills[SK_THROWING] = 1; //jmf: removed these, added magic below
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
        you.inv[0].colour = BROWN;

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

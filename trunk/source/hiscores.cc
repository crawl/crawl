/*
 *  File: highscore.cc
 *  Summary: deal with reading and writing of highscore file
 *  Written by: Gordon Lipford
 *
 *  Change History (most recent first):
 *
 *   <1>   16feb2001      gdl    Created
 */

/*
 * ----------- MODIFYING THE PRINTED SCORE FORMAT ---------------------
 *   Do this at your leisure.  Change hiscores_format_single() as much
 * as you like.
 *
 *
 * ----------- IF YOU MODIFY THE INTERNAL SCOREFILE FORMAT ------------
 *              .. as defined by the struct 'scorefile_entry' ..
 *   You MUST change hs_copy(),  hs_parse_numeric(),  hs_parse_string(),
 *       and hs_write().  It's also a really good idea to change the
 *       version numbers assigned in ouch() so that Crawl can tell the
 *       difference between your new entry and previous versions.
 *
 *
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "AppHdr.h"
#include "externs.h"

#include "hiscores.h"
#include "itemname.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "stuff.h"
#include "tags.h"
#include "view.h"

#include "skills2.h"

#ifdef MULTIUSER
    // includes to get passwd file access:
    #include <pwd.h>
    #include <sys/types.h>
#endif

// enough memory allocated to snarf in the scorefile entries
static struct scorefile_entry hs_list[SCORE_FILE_ENTRIES];

// hackish: scorefile position of newest entry.  Will be highlit during
// highscore printing (always -1 when run from command line).
static int newest_entry = -1;

static FILE *hs_open(const char *mode);
static void hs_close(FILE *handle, const char *mode);
static bool hs_read(FILE *scores, struct scorefile_entry &dest);
static void hs_parse_numeric(char *inbuf, struct scorefile_entry &dest);
static void hs_parse_string(char *inbuf, struct scorefile_entry &dest);
static void hs_copy(struct scorefile_entry &dest, struct scorefile_entry &src);
static void hs_write(FILE *scores, struct scorefile_entry &entry);
static void hs_nextstring(char *&inbuf, char *dest);
static int hs_nextint(char *&inbuf);
static long hs_nextlong(char *&inbuf);

// functions dealing with old scorefile entries
static void hs_parse_generic_1(char *&inbuf, char *outbuf, const char *stopvalues);
static void hs_parse_generic_2(char *&inbuf, char *outbuf, const char *continuevalues);
static void hs_stripblanks(char *buf);
static void hs_search_death(char *inbuf, struct scorefile_entry &se);
static void hs_search_where(char *inbuf, struct scorefile_entry &se);

// file locking stuff
#ifdef USE_FILE_LOCKING
static bool lock_file_handle( FILE *handle, int type );
static bool unlock_file_handle( FILE *handle );
#endif // USE_FILE_LOCKING

void hiscores_new_entry( struct scorefile_entry &ne )
{
    FILE *scores;
    int i, total_entries;
    bool inserted = false;

    // open highscore file (reading) -- note that NULL is not fatal!
    scores = hs_open("r");

    // read highscore file, inserting new entry at appropriate point,
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        if (hs_read(scores, hs_list[i]) == false)
            break;

        // compare points..
        if (ne.points >= hs_list[i].points && inserted == false)
        {
            newest_entry = i;           // for later printing
            inserted = true;
            // copy read entry to i+1th position
            // Fixed a nasty overflow bug here -- Sharp
            if (i+1 < SCORE_FILE_ENTRIES)
            {
                hs_copy(hs_list[i+1], hs_list[i]);
                hs_copy(hs_list[i], ne);
                i++;
            } else {
            // copy new entry to current position
                hs_copy(hs_list[i], ne);
            }
        }
    }

    // special case: lowest score,  with room
    if (!inserted && i < SCORE_FILE_ENTRIES)
    {
        newest_entry = i;
        inserted = true;
        // copy new entry
        hs_copy(hs_list[i], ne);
        i++;
    }

    total_entries = i;

    // close so we can re-open for writing
    hs_close(scores,"r");

    // open highscore file (writing) -- NULL *is* fatal here.
    scores = hs_open("w");
    if (scores == NULL)
    {
        perror("Entry not added - failure opening score file for writing.");
        return;
    }

    // write scorefile entries.
    for (i = 0; i < total_entries; i++)
    {
        hs_write(scores, hs_list[i]);
    }

    // close scorefile.
    hs_close(scores, "w");
}

void hiscores_print_list( int display_count, int format )
{
    FILE *scores;
    int i, total_entries;
    bool use_printf = (Options.sc_entries > 0);

    if (display_count <= 0)
        display_count = SCORE_FILE_ENTRIES;

    // open highscore file (reading)
    scores = hs_open("r");
    if (scores == NULL)
    {
        // will only happen from command line
        puts( "No high scores." );
        return;
    }

    // read highscore file
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        if (hs_read( scores, hs_list[i] ) == false)
            break;
    }
    total_entries = i;

    // close off
    hs_close( scores, "r" );

    if (!use_printf) 
        textcolor(LIGHTGREY);

    int start = (newest_entry > 10) ? newest_entry - 10: 0;

    if (start + display_count > total_entries)
       start = total_entries - display_count;

    if (start < 0)
       start = 0;

    const int finish = start + display_count;

    for (i = start; i < finish && i < total_entries; i++)
    {
        // check for recently added entry
        if (i == newest_entry && !use_printf)
            textcolor(YELLOW);

        // print position (tracked implicitly by order score file)
        snprintf( info, INFO_SIZE, "%3d.", i + 1 );
        if (use_printf)
            printf(info);
        else
            cprintf(info);

        // format the entry
        if (format == SCORE_TERSE)
        {
            hiscores_format_single( info, hs_list[i] );
            // truncate if we want short format
            info[75] = '\0';
        }
        else
        {
            hiscores_format_single_long( info, hs_list[i], 
                                         (format == SCORE_VERBOSE) );
        }

        // print entry
        strcat(info, EOL);
        if(use_printf)
            printf(info);
        else
            cprintf(info);

        if (i == newest_entry && !use_printf)
            textcolor(LIGHTGREY);
    }
}

// Trying to supply an appropriate verb for the attack type. -- bwr
static const char *const range_type_verb( const char *const aux )
{
    if (strncmp( aux, "Shot ", 5 ) == 0)                // launched
        return ("shot");
    else if (aux[0] == '\0'                             // unknown
            || strncmp( aux, "Hit ", 4 ) == 0           // thrown
            || strncmp( aux, "volley ", 7 ) == 0)       // manticore spikes
    {
        return ("hit from afar");
    }

    return ("blasted");                                 // spells, wands
}

void hiscores_format_single(char *buf, struct scorefile_entry &se)
{
    char scratch[100];

    // Now that we have a long format, I'm starting to make this
    // more terse, in hopes that it will better fit. -- bwr

    // race_class_name overrides race & class
    if (se.race_class_name[0] == '\0')
    {
        snprintf( scratch, sizeof(scratch), "%s%s", 
                  get_species_abbrev( se.race ), get_class_abbrev( se.cls ) );
    }
    else
    {
        strcpy( scratch, se.race_class_name );
    }

    se.name[10]='\0';
    sprintf( buf, "%8ld %-10s %s-%02d%s", se.points, se.name,
             scratch, se.lvl, (se.wiz_mode == 1) ? "W" : "" );

    // get monster type & number, if applicable
    int mon_type = se.death_source;
    int mon_number = se.mon_num;

    // remember -- we have 36 characters (not including initial space):
    switch (se.death_type)
    {
    case KILLED_BY_MONSTER:
        strcat( buf, " slain by " );

        // if death_source_name is non-null,  override lookup (names might have
        // changed!)
        if (se.death_source_name[0] != '\0')
            strcat( buf, se.death_source_name );
        else
            strcat( buf, monam( mon_number, mon_type, true, DESC_PLAIN ) );

        break;

    case KILLED_BY_POISON:
        //if (dam == -9999) strcat(buf, "an overload of ");
        strcat( buf, " succumbed to poison" );
        break;

    case KILLED_BY_CLOUD:
        if (se.auxkilldata[0] == '\0')
            strcat( buf, " engulfed by a cloud" );
        else
        {
            const int len = strlen( se.auxkilldata );

            // Squeeze out "a cloud of" if required. -- bwr
            snprintf( scratch, sizeof(scratch), " engulfed by %s%s", 
                      (len < 15) ? "a cloud of " : "",
                      se.auxkilldata );

            strcat( buf, scratch );
        }
        break;

    case KILLED_BY_BEAM:
        // keeping this short to leave room for the deep elf spellcasters:
        snprintf( scratch, sizeof(scratch), " %s by ", 
                  range_type_verb( se.auxkilldata ) );  
        strcat( buf, scratch );

        // if death_source_name is non-null,  override this
        if (se.death_source_name[0] != '\0')
            strcat( buf, se.death_source_name );
        else
            strcat( buf, monam( mon_number, mon_type, true, DESC_PLAIN ) );
        break;

/*
    case KILLED_BY_DEATHS_DOOR:
        // death's door running out - NOTE: This is no longer fatal
        strcat(buf, " ran out of time");
        break;
*/

    case KILLED_BY_LAVA:
        if (se.race == SP_MUMMY)
            strcat( buf, " turned to ash by lava" );
        else
            strcat( buf, " took a swim in lava" );
        break;

    case KILLED_BY_WATER:
        if (se.race == SP_MUMMY)
            strcat( buf, " soaked and fell apart" );
        else
            strcat( buf, " drowned" );
        break;

    // these three are probably only possible if you wear a ring
    // of >= +3 ability, get drained to 3, then take it off, or have a very
    // low abil and wear a -ve ring. or, as of 2.7x, mutations can cause this
    // Don't forget decks of cards (they have some nasty code for this) -- bwr
    case KILLED_BY_STUPIDITY:
        strcat( buf, " died of stupidity" );
        break;

    case KILLED_BY_WEAKNESS:
        strcat( buf, " became too weak to continue" );
        break;

    case KILLED_BY_CLUMSINESS:
        strcat( buf, " slipped on a banana peel" );
        break;

    case KILLED_BY_TRAP:
        snprintf( scratch, sizeof(scratch), " triggered a%s trap", 
                 (se.auxkilldata[0] != '\0') ? se.auxkilldata : "" );
        strcat( buf, scratch );
        break;

    case KILLED_BY_LEAVING:
        strcat( buf, " got out of the dungeon alive" );
        break;

    case KILLED_BY_WINNING:
        strcat( buf, " escaped with the Orb!" );
        break;

    case KILLED_BY_QUITTING:
        strcat( buf, " quit the game" );
        break;

    case KILLED_BY_DRAINING:
        strcat( buf, " drained of all life" );
        break;

    case KILLED_BY_STARVATION:
        strcat( buf, " starved to death" );
        break;

    case KILLED_BY_FREEZING: 
        strcat( buf, " froze to death" );
        break;

    case KILLED_BY_BURNING:     // only sticky flame
        strcat( buf, " burnt to a crisp" );
        break;

    case KILLED_BY_WILD_MAGIC:
        if (se.auxkilldata[0] == '\0')
            strcat( buf, " killed by wild magic" );
        else
        {
            const bool need_by = (strncmp( se.auxkilldata, "by ", 3 ) == 0);
            const int  len = strlen( se.auxkilldata );

            // Squeeze out "killed" if we're getting a bit long. -- bwr
            snprintf( scratch, sizeof(scratch), " %s%s%s", 
                      (len + 3 * (need_by) < 30) ? "killed" : "",
                      (need_by) ? "by " : "",
                      se.auxkilldata );

            strcat( buf, scratch );
        }
        break;

    case KILLED_BY_XOM:  // only used for old Xom kills
        strcat( buf, " killed for Xom's enjoyment" ); 
        break;

    case KILLED_BY_STATUE:
        strcat( buf, " killed by a statue" );
        break;

    case KILLED_BY_ROTTING:
        strcat( buf, " rotted away" );
        break;

    case KILLED_BY_TARGETTING:
        strcat( buf, " killed by bad targeting" );
        break;

    case KILLED_BY_SPORE:
        strcat( buf, " killed by an exploding spore" );
        break;

    case KILLED_BY_TSO_SMITING:
        strcat( buf, " smote by The Shining One" );
        break;

    case KILLED_BY_PETRIFICATION:
        strcat( buf, " turned to stone" );
        break;

    case KILLED_BY_SHUGGOTH:
        strcat( buf, " eviscerated by a hatching shuggoth" );
        break;

    case KILLED_BY_SOMETHING:
        strcat( buf, " died" );
        break;

    case KILLED_BY_FALLING_DOWN_STAIRS:
        strcat( buf, " fell down a flight of stairs" );
        break;

    case KILLED_BY_ACID:
        strcat( buf, " splashed by acid" );
        break;

    default:
        strcat( buf, " nibbled to death by software bugs" );
        break;
    }                           // end switch

    if (se.death_type != KILLED_BY_LEAVING && se.death_type != KILLED_BY_WINNING)
    {
        if (se.level_type == LEVEL_ABYSS)
        {
            strcat(buf, " (Abyss)");
            return;
        }
        else if (se.level_type == LEVEL_PANDEMONIUM)
        {
            strcat(buf, " (Pan)");
            return;
        }
        else if (se.level_type == LEVEL_LABYRINTH)
        {
            strcat(buf, " (Lab)");
            return;
        }
        else if (se.branch == BRANCH_VESTIBULE_OF_HELL)
        {
            strcat(buf, " (Hell)");  // Gate? Vest?
            return;
        }
        else if (se.branch == BRANCH_HALL_OF_BLADES)
        {
            strcat(buf, " (Blade)");
            return;
        }
        else if (se.branch == BRANCH_ECUMENICAL_TEMPLE)
        {
            strcat(buf, " (Temple)");
            return;
        }

        snprintf( scratch, sizeof(scratch), " (%s%d)",
                    (se.branch == BRANCH_DIS)          ? "Dis " :
                    (se.branch == BRANCH_GEHENNA)      ? "Geh " :
                    (se.branch == BRANCH_COCYTUS)      ? "Coc " :
                    (se.branch == BRANCH_TARTARUS)     ? "Tar " :
                    (se.branch == BRANCH_ORCISH_MINES) ? "Orc " :
                    (se.branch == BRANCH_HIVE)         ? "Hive " :
                    (se.branch == BRANCH_LAIR)         ? "Lair " :
                    (se.branch == BRANCH_SLIME_PITS)   ? "Slime " :
                    (se.branch == BRANCH_VAULTS)       ? "Vault " :
                    (se.branch == BRANCH_CRYPT)        ? "Crypt " :
                    (se.branch == BRANCH_HALL_OF_ZOT)  ? "Zot " :
                    (se.branch == BRANCH_SNAKE_PIT)    ? "Snake " :
                    (se.branch == BRANCH_ELVEN_HALLS)  ? "Elf " :
                    (se.branch == BRANCH_TOMB)         ? "Tomb " :
                    (se.branch == BRANCH_SWAMP)        ? "Swamp " : "DLv ", 
                    se.dlvl );

        strcat( buf, scratch );
    } // endif - killed by winning

    return;
}

static bool hiscore_same_day( time_t t1, time_t t2 )
{
    struct tm *d1  = localtime( &t1 );
    const int year = d1->tm_year;
    const int mon  = d1->tm_mon;
    const int day  = d1->tm_mday;

    struct tm *d2  = localtime( &t2 );

    return (d2->tm_mday == day && d2->tm_mon == mon && d2->tm_year == year);
}

static void hiscore_date_string( time_t time, char buff[INFO_SIZE] )
{
    struct tm *date = localtime( &time );

    const char *mons[12] = { "Jan", "Feb", "Mar", "Apr", "May", "June",
                             "July", "Aug", "Sept", "Oct", "Nov", "Dec" };

    snprintf( buff, INFO_SIZE, "%s %d, %d", mons[date->tm_mon], 
              date->tm_mday, date->tm_year + 1900 );
}

static void hiscore_newline( char *buf, int &line_count ) 
{
    strncat( buf, EOL "             ", HIGHSCORE_SIZE );
    line_count++;
}

int hiscores_format_single_long( char *buf, struct scorefile_entry &se, 
                                 bool verbose )
{
    char  scratch[INFO_SIZE];
    int   line_count = 1; 

    // race_class_name could/used to override race & class
    // strcpy(scratch, se.race_class_name);

    // Please excuse the following bit of mess in the name of flavour ;)
    if (verbose)
    {
        strncpy( scratch, skill_title( se.best_skill, se.best_skill_lvl, 
                                       se.race, se.str, se.dex, se.god ), 
                 INFO_SIZE );

        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s (level %d",
                  se.points, se.name, scratch, se.lvl );

    }
    else 
    {
        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s %s (level %d",
                  se.points, se.name, species_name(se.race, se.lvl), 
                  get_class_name(se.cls), se.lvl );
    }

    if (se.final_max_max_hp > 0)  // as the other two may be negative
    {
        snprintf( scratch, INFO_SIZE, ", %d/%d", se.final_hp, se.final_max_hp );
        strncat( buf, scratch, HIGHSCORE_SIZE );

        if (se.final_max_hp < se.final_max_max_hp)
        {
            snprintf( scratch, INFO_SIZE, " (%d)", se.final_max_max_hp );
            strncat( buf, scratch, HIGHSCORE_SIZE );
        }

        strncat( buf, " HPs", HIGHSCORE_SIZE );
    }

    strncat( buf, ((se.wiz_mode) ? ") *WIZ*" : ")"), HIGHSCORE_SIZE );
    hiscore_newline( buf, line_count );

    if (verbose)
    {
        const char *const race = species_name( se.race, se.lvl );

        snprintf( scratch, INFO_SIZE, "Began as a%s %s %s",
                  is_vowel(race[0]) ? "n" : "", race, get_class_name(se.cls) );
        strncat( buf, scratch, HIGHSCORE_SIZE );

        if (se.birth_time > 0)
        {
            strncat( buf, " on ", HIGHSCORE_SIZE );
            hiscore_date_string( se.birth_time, scratch );
            strncat( buf, scratch, HIGHSCORE_SIZE );
        }

        strncat( buf, "." , HIGHSCORE_SIZE );
        hiscore_newline( buf, line_count );

        if (se.race != SP_DEMIGOD && se.god != -1)
        {
            if (se.god == GOD_XOM)
            {
                snprintf( scratch, INFO_SIZE, "Was a %sPlaything of Xom.",
                          (se.lvl >= 20) ? "Favourite " : "" );

                strncat( buf, scratch, HIGHSCORE_SIZE );
                hiscore_newline( buf, line_count );
            }
            else if (se.god != GOD_NO_GOD)
            {
                // Not exactly the same as the religon screen, but
                // good enough to fill this slot for now.
                snprintf( scratch, INFO_SIZE, "Was %s of %s%s",
                          (se.piety >  160) ? "the Champion" :
                          (se.piety >= 120) ? "a High Priest" :
                          (se.piety >= 100) ? "an Elder" :
                          (se.piety >=  75) ? "a Priest" :
                          (se.piety >=  50) ? "a Believer" :
                          (se.piety >=  30) ? "a Follower" 
                                            : "an Initiate",
                          god_name( se.god ), 
                          (se.penance > 0) ? " (penitent)." : "." );

                strncat( buf, scratch, HIGHSCORE_SIZE );
                hiscore_newline( buf, line_count );
            }
        }
    }

    // get monster type & number, if applicable
    int mon_type = se.death_source;
    int mon_number = se.mon_num;

    bool needs_beam_cause_line = false;
    bool needs_called_by_monster_line = false;
    bool needs_damage = false;

    switch (se.death_type)
    {
    case KILLED_BY_MONSTER:
        // GDL: here's an example of using final_hp.  Verbiage could be better.
        // bwr: changed "blasted" since this is for melee
        snprintf( scratch, INFO_SIZE, "%s %s",
                    (se.final_hp > -6)  ? "Slain by"   :
                    (se.final_hp > -14) ? "Mangled by" :
                    (se.final_hp > -22) ? "Demolished by" 
                                        : "Annihilated by",

                    (se.death_source_name[0] != '\0')
                            ? se.death_source_name
                            : monam( mon_number, mon_type, true, DESC_PLAIN ) );

        strncat( buf, scratch, HIGHSCORE_SIZE );

        // put the damage on the weapon line if there is one
        if (se.auxkilldata[0] == '\0')
            needs_damage = true;
        break;

    case KILLED_BY_POISON:
        //if (dam == -9999) strcat(buf, "an overload of ");
        strcat( buf, "Succumbed to poison" );
        break;

    case KILLED_BY_CLOUD:
        if (se.auxkilldata[0] == '\0')
            strcat( buf, "Engulfed by a cloud" );
        else
        {
            snprintf( scratch, sizeof(scratch), "Engulfed by a cloud of %s", 
                      se.auxkilldata );
            strcat( buf, scratch );
        }
        needs_damage = true;
        break;

    case KILLED_BY_BEAM:
        if (isupper( se.auxkilldata[0] ))  // already made (ie shot arrows)
        {
            strcat( buf, se.auxkilldata );
            needs_damage = true;
        }
        else if (verbose && strncmp( se.auxkilldata, "by ", 3 ) == 0)
        {
            // "by" is used for priest attacks where the effect is indirect
            // in verbose format we have another line for the monster
            needs_called_by_monster_line = true;
            snprintf( scratch, sizeof(scratch), "Killed %s", se.auxkilldata );
            strncat( buf, scratch, HIGHSCORE_SIZE );
        }
        else
        {
            // Note: This is also used for the "by" cases in non-verbose
            //       mode since listing the monster is more imporatant.
            strcat( buf, "Killed from afar by " );

            // if death_source_name is non-null,  override this
            if (se.death_source_name[0] != '\0')
                strcat(buf, se.death_source_name);
            else
                strcat(buf, monam( mon_number, mon_type, true, DESC_PLAIN ));

            if (se.auxkilldata[0] != '\0')
                needs_beam_cause_line = true;
        }
        break;

    case KILLED_BY_LAVA:
        if (se.race == SP_MUMMY)
            strcat( buf, "Turned to ash by lava" );
        else
            strcat( buf, "Took a swim in molten lava" );
        break;

    case KILLED_BY_WATER:
        if (se.race == SP_MUMMY)
            strcat( buf, "Soaked and fell apart" );
        else
            strcat( buf, "Drowned" );
        break;

    case KILLED_BY_STUPIDITY:
        strcat( buf, "Forgot to breathe" );
        break;

    case KILLED_BY_WEAKNESS:
        strcat( buf, "Collapsed under their own weight" );
        break;

    case KILLED_BY_CLUMSINESS:
        strcat( buf, "Slipped on a banana peel" );
        break;

    case KILLED_BY_TRAP:
        snprintf( scratch, sizeof(scratch), "Killed by triggering a%s trap", 
                 (se.auxkilldata[0] != '\0') ? se.auxkilldata : "" );
        strcat( buf, scratch );
        needs_damage = true;
        break;

    case KILLED_BY_LEAVING:
        if (se.num_runes > 0)
            strcat( buf, "Got out of the dungeon" );
        else
            strcat( buf, "Got out of the dungeon alive!" );
        break;

    case KILLED_BY_WINNING:
        strcat( buf, "Escaped with the Orb" );
        if (se.num_runes < 1)
            strcat( buf, "!" );
        break;

    case KILLED_BY_QUITTING:
        strcat( buf, "Quit the game" );
        break;

    case KILLED_BY_DRAINING:
        strcat( buf, "Was drained of all life" );
        break;

    case KILLED_BY_STARVATION:
        strcat( buf, "Starved to death" );
        break;

    case KILLED_BY_FREEZING:    // refrigeration spell
        strcat( buf, "Froze to death" );
        needs_damage = true;
        break;

    case KILLED_BY_BURNING:     // sticky flame
        strcat( buf, "Burnt to a crisp" );
        needs_damage = true;
        break;

    case KILLED_BY_WILD_MAGIC:
        if (se.auxkilldata[0] == '\0')
            strcat( buf, "Killed by wild magic" );
        else
        {
            // A lot of sources for this case... some have "by" already.
            snprintf( scratch, sizeof(scratch), "Killed %s%s", 
                      (strncmp( se.auxkilldata, "by ", 3 ) != 0) ? "by " : "",
                      se.auxkilldata );

            strcat( buf, scratch );
        }

        needs_damage = true;
        break;

    case KILLED_BY_XOM:  // only used for old Xom kills
        strcat( buf, "It was good that Xom killed them" );
        needs_damage = true;
        break;

    case KILLED_BY_STATUE:
        strcat( buf, "Killed by a statue" );
        needs_damage = true;
        break;

    case KILLED_BY_ROTTING:
        strcat( buf, "Rotted away" );
        break;

    case KILLED_BY_TARGETTING:
        strcat( buf, "Killed themselves with bad targeting" );
        needs_damage = true;
        break;

    case KILLED_BY_SPORE:
        strcat( buf, "Killed by an exploding spore" );
        needs_damage = true;
        break;

    case KILLED_BY_TSO_SMITING:
        strcat( buf, "Smote by The Shining One" );
        needs_damage = true;
        break;

    case KILLED_BY_PETRIFICATION:
        strcat( buf, "Turned to stone" );
        break;

    case KILLED_BY_SHUGGOTH:
        strcat( buf, "Eviscerated by a hatching shuggoth" );
        needs_damage = true;
        break;

    case KILLED_BY_SOMETHING:
        strcat( buf, "Died" );
        break;

    case KILLED_BY_FALLING_DOWN_STAIRS:
        strcat( buf, "Fell down a flight of stairs" );
        needs_damage = true;
        break;

    case KILLED_BY_ACID:
        strcat( buf, "Splashed by acid" );
        needs_damage = true;
        break;

    default:
        strcat( buf, "Nibbled to death by software bugs" );
        break;
    }                           // end switch

    // TODO: Eventually, get rid of "..." for cases where the text fits.
    if (verbose)
    {
        bool done_damage = false;  // paranoia

        if (needs_damage && se.damage > 0)
        {
            snprintf( scratch, INFO_SIZE, " (%d damage)", se.damage );
            strncat( buf, scratch, HIGHSCORE_SIZE );
            needs_damage = false;
            done_damage = true;
        }

        if ((se.death_type == KILLED_BY_LEAVING 
                        || se.death_type == KILLED_BY_WINNING)
                    && se.num_runes > 0)
        {
            hiscore_newline( buf, line_count );

            snprintf( scratch, INFO_SIZE, "... %s %d rune%s", 
                      (se.death_type == KILLED_BY_WINNING) ? "and" : "with",
                      se.num_runes, (se.num_runes > 1) ? "s" : "" );
            strncat( buf, scratch, HIGHSCORE_SIZE );

            if (se.num_diff_runes > 1)
            {
                snprintf( scratch, INFO_SIZE, " (of %d types)",
                          se.num_diff_runes );
                strncat( buf, scratch, HIGHSCORE_SIZE );
            }

            if (se.death_time > 0 
                && !hiscore_same_day( se.birth_time, se.death_time ))
            {
                strcat( buf, " on " );
                hiscore_date_string( se.death_time, scratch );
                strcat( buf, scratch );
            }

            strcat( buf, "!" );
            hiscore_newline( buf, line_count );
        }
        else if (se.death_type != KILLED_BY_QUITTING)
        {
            hiscore_newline( buf, line_count );

            if (se.death_type == KILLED_BY_MONSTER && se.auxkilldata[0])
            {
                snprintf(scratch, INFO_SIZE, "... wielding %s", se.auxkilldata);
                strncat(buf, scratch, HIGHSCORE_SIZE);
                needs_damage = true;
            }
            else if (needs_beam_cause_line)
            {
                strcat( buf, (is_vowel( se.auxkilldata[0] )) ? "... with an " 
                                                             : "... with a " );
                strcat( buf, se.auxkilldata );
                needs_damage = true;
            }
            else if (needs_called_by_monster_line)
            {
                snprintf( scratch, sizeof(scratch), "... called down by %s",
                          se.death_source_name );
                strncat( buf, scratch, HIGHSCORE_SIZE );
                needs_damage = true;
            }

            if (needs_damage && !done_damage) 
            {
                if (se.damage > 0)
                {
                    snprintf( scratch, INFO_SIZE, " (%d damage)", se.damage );
                    strncat( buf, scratch, HIGHSCORE_SIZE );
                    hiscore_newline( buf, line_count );
                }
            }
        }
    }
    
    if (se.death_type == KILLED_BY_LEAVING
        || se.death_type == KILLED_BY_WINNING)
    {
        // TODO: strcat "after reaching level %d"; for LEAVING
        if (!verbose)
        {
            if (se.num_runes > 0)
                strcat( buf, "!" );

            hiscore_newline( buf, line_count );
        }
    }
    else 
    {
        if (verbose && se.death_type != KILLED_BY_QUITTING)
            strcat( buf, "..." );

        if (se.level_type == LEVEL_ABYSS)
            strcat( buf, " in the Abyss" );
        else if (se.level_type == LEVEL_PANDEMONIUM)
            strcat( buf, " in Pandemonium" );
        else if (se.level_type == LEVEL_LABYRINTH)
            strcat( buf, " in a labyrinth" );
        else 
        {
            switch (se.branch)
            {
            case BRANCH_ECUMENICAL_TEMPLE:
                strcat( buf, " in the Ecumenical Temple" );
                break;
            case BRANCH_HALL_OF_BLADES:
                strcat( buf, " in the Hall of Blades" );
                break;
            case BRANCH_VESTIBULE_OF_HELL:
                strcat( buf, " in the Vestibule" );
                break;

            case BRANCH_DIS:
                strcat( buf, " on Dis" );
                break;
            case BRANCH_GEHENNA:
                strcat( buf, " on Gehenna" );
                break;
            case BRANCH_COCYTUS:
                strcat( buf, " on Cocytus" );
                break;
            case BRANCH_TARTARUS:
                strcat( buf, " on Tartarus" );
                break;
            case BRANCH_ORCISH_MINES:
                strcat( buf, " on Orcish Mines" );
                break;
            case BRANCH_HIVE:
                strcat( buf, " on Hive" );
                break;
            case BRANCH_LAIR:
                strcat( buf, " on Lair" );
                break;
            case BRANCH_SLIME_PITS:
                strcat( buf, " on Slime Pits" );
                break;
            case BRANCH_VAULTS:
                strcat( buf, " on Vault" );
                break;
            case BRANCH_CRYPT:
                strcat( buf, " on Crypt" );
                break;
            case BRANCH_HALL_OF_ZOT:
                strcat( buf, " on Hall of Zot" );
                break;
            case BRANCH_SNAKE_PIT:
                strcat( buf, " on Snake Pit" );
                break;
            case BRANCH_ELVEN_HALLS:
                strcat( buf, " on Elven Halls" );
                break;
            case BRANCH_TOMB:
                strcat( buf, " on Tomb" );
                break;
            case BRANCH_SWAMP:
                strcat( buf, " on Swamp" );
                break;
            case BRANCH_MAIN_DUNGEON:
                strcat( buf, " on Dungeon" );
                break;
            }

            if (se.branch != BRANCH_VESTIBULE_OF_HELL
                && se.branch != BRANCH_ECUMENICAL_TEMPLE
                && se.branch != BRANCH_HALL_OF_BLADES)
            {
                snprintf( scratch, sizeof(scratch), " Level %d", se.dlvl );
                strcat( buf, scratch );
            }
        }

        if (verbose && se.death_time 
            && !hiscore_same_day( se.birth_time, se.death_time ))
        {
            strcat( buf, " on " );
            hiscore_date_string( se.death_time, scratch );
            strcat( buf, scratch );
        }

        strcat( buf, "." );
        hiscore_newline( buf, line_count );
    } // endif - killed by winning

    if (verbose)
    {
        if (se.real_time > 0)
        {
            char username[80] = "The";
            char tmp[80];

#ifdef MULTIUSER
            if (se.uid > 0)
            {
                struct passwd *pw_entry = getpwuid( se.uid );
                strncpy( username, pw_entry->pw_name, sizeof(username) );
                strncat( username, "'s", sizeof(username) );
                username[0] = toupper( username[0] );
            }
#endif

            make_time_string( se.real_time, tmp, sizeof(tmp) );

            snprintf( scratch, INFO_SIZE, "%s game lasted %s (%ld turns).",
                      username, tmp, se.num_turns );

            strncat( buf, scratch, HIGHSCORE_SIZE );
            hiscore_newline( buf, line_count );
        }
    }

    return (line_count);
}

// --------------------------------------------------------------------------
// BEGIN private functions
// --------------------------------------------------------------------------

// first, some file locking stuff for multiuser crawl
#ifdef USE_FILE_LOCKING

static bool lock_file_handle( FILE *handle, int type )
{
    struct flock  lock;
    int           status;

    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = type;

#ifdef USE_BLOCKING_LOCK

    status = fcntl( fileno( handle ), F_SETLKW, &lock );

#else

    for (int i = 0; i < 30; i++)
    {
        status = fcntl( fileno( handle ), F_SETLK, &lock );

        // success
        if (status == 0)
            break;

        // known failure
        if (status == -1 && (errno != EACCES && errno != EAGAIN))
            break;

        perror( "Problems locking file... retrying..." );
        delay( 1000 );
    }

#endif

    return (status == 0);
}

static bool unlock_file_handle( FILE *handle )
{
    struct flock  lock;
    int           status;

    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = F_UNLCK;

#ifdef USE_BLOCKING_LOCK

    status = fcntl( fileno( handle ), F_SETLKW, &lock );

#else

    for (int i = 0; i < 30; i++)
    {
        status = fcntl( fileno( handle ), F_SETLK, &lock );

        // success
        if (status == 0)
            break;

        // known failure
        if (status == -1 && (errno != EACCES && errno != EAGAIN))
            break;

        perror( "Problems unlocking file... retrying..." );
        delay( 1000 );
    }

#endif

    return (status == 0);
}

#endif



FILE *hs_open( const char *mode )
{
#ifdef SAVE_DIR_PATH
    FILE *handle = fopen(SAVE_DIR_PATH "scores", mode);
#else
    FILE *handle = fopen("scores", mode);
#endif

#ifdef USE_FILE_LOCKING
    int locktype = F_RDLCK;
    if (stricmp(mode, "w") == 0)
        locktype = F_WRLCK;

    if (handle && !lock_file_handle( handle, locktype ))
    {
        perror( "Could not lock scorefile... " );
        fclose( handle );
        handle = NULL;
    }
#endif
    return handle;
}

void hs_close( FILE *handle, const char *mode )
{
    UNUSED( mode );

    if (handle == NULL)
        return;

#ifdef USE_FILE_LOCKING
    unlock_file_handle( handle );
#endif

    // actually close
    fclose(handle);

#ifdef SHARED_FILES_CHMOD_PUBLIC
    if (stricmp(mode, "w") == 0)
    {
  #ifdef SAVE_DIR_PATH
        chmod(SAVE_DIR_PATH "scores", SHARED_FILES_CHMOD_PUBLIC);
  #else
        chmod("scores", SHARED_FILES_CHMOD_PUBLIC);
  #endif
    }
#endif
}

static void hs_init( struct scorefile_entry &dest )
{
    // simple init
    dest.version = 0;
    dest.release = 0;
    dest.points = -1;
    dest.name[0] = '\0';
    dest.uid = 0;
    dest.race = 0; 
    dest.cls = 0;
    dest.lvl = 0;
    dest.race_class_name[0] = '\0';
    dest.best_skill = 0;
    dest.best_skill_lvl = 0;
    dest.death_type = KILLED_BY_SOMETHING;
    dest.death_source = 0;
    dest.mon_num = 0;
    dest.death_source_name[0] = '\0';
    dest.auxkilldata[0] = '\0';
    dest.dlvl = 0;
    dest.level_type = 0;
    dest.branch = 0;
    dest.final_hp = -1;
    dest.final_max_hp = -1;
    dest.final_max_max_hp = -1;
    dest.str = -1;
    dest.intel = -1;
    dest.dex = -1;
    dest.damage = -1;
    dest.god = -1;
    dest.piety = -1;
    dest.penance = -1;
    dest.wiz_mode = 0;
    dest.birth_time = 0;
    dest.death_time = 0;
    dest.real_time = -1;
    dest.num_turns = -1;
    dest.num_diff_runes = 0;
    dest.num_runes = 0;
}

void hs_copy(struct scorefile_entry &dest, struct scorefile_entry &src)
{
    // simple field copy -- assume src is well constructed.

    dest.version = src.version;
    dest.release = src.release;
    dest.points = src.points;
    strcpy(dest.name, src.name);
    dest.uid = src.uid;
    dest.race = src.race;
    dest.cls = src.cls;
    dest.lvl = src.lvl;
    strcpy(dest.race_class_name, src.race_class_name);
    dest.best_skill = src.best_skill;
    dest.best_skill_lvl = src.best_skill_lvl;
    dest.death_type = src.death_type;
    dest.death_source = src.death_source;
    dest.mon_num = src.mon_num;
    strcpy( dest.death_source_name, src.death_source_name );
    strcpy( dest.auxkilldata, src.auxkilldata );
    dest.dlvl = src.dlvl;
    dest.level_type = src.level_type;
    dest.branch = src.branch;
    dest.final_hp = src.final_hp;
    dest.final_max_hp = src.final_max_hp;
    dest.final_max_max_hp = src.final_max_max_hp;
    dest.str = src.str;
    dest.intel = src.intel;
    dest.dex = src.dex;
    dest.damage = src.damage;
    dest.god = src.god;
    dest.piety = src.piety;
    dest.penance = src.penance;
    dest.wiz_mode = src.wiz_mode;
    dest.birth_time = src.birth_time;
    dest.death_time = src.death_time;
    dest.real_time = src.real_time;
    dest.num_turns = src.num_turns;
    dest.num_diff_runes = src.num_diff_runes;
    dest.num_runes = src.num_runes;
}

bool hs_read( FILE *scores, struct scorefile_entry &dest )
{
    char inbuf[200];
    int c = EOF;

    hs_init( dest );

    // get a character..
    if (scores != NULL)
        c = fgetc(scores);

    // check for NULL scores file or EOF
    if (scores == NULL || c == EOF)
        return false;

    // get a line - this is tricky.  "Lines" come in three flavors:
    // 1) old-style lines which were 80 character blocks
    // 2) 4.0 pr1 through pr7 versions which were newline terminated
    // 3) 4.0 pr8 and onwards which are 'current' ASCII format, and
    //    may exceed 80 characters!

    // put 'c' in first spot
    inbuf[0] = c;

    if (fgets(&inbuf[1], (c==':') ? (sizeof(inbuf) - 2) : 81, scores) == NULL)
        return false;

    // check type;  lines starting with a colon are new-style scores.
    if (c == ':')
        hs_parse_numeric(inbuf, dest);
    else
        hs_parse_string(inbuf, dest);

    return true;
}

static void hs_nextstring(char *&inbuf, char *dest)
{
    char *p = dest;

    if (*inbuf == '\0')
    {
        *p = '\0';
        return;
    }

    // assume we're on a ':'
    inbuf ++;
    while(*inbuf != ':' && *inbuf != '\0')
        *p++ = *inbuf++;

    *p = '\0';
}

static int hs_nextint(char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num);

    return (num[0] == '\0' ? 0 : atoi(num));
}

static long hs_nextlong(char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num);

    return (num[0] == '\0' ? 0 : atol(num));
}

static int val_char( char digit )
{
    return (digit - '0');
}

static time_t hs_nextdate(char *&inbuf)
{
    char       buff[20];
    struct tm  date;

    hs_nextstring( inbuf, buff );

    if (strlen( buff ) < 15)
        return (static_cast<time_t>(0));

    date.tm_year = val_char( buff[0] ) * 1000 + val_char( buff[1] ) * 100
                    + val_char( buff[2] ) * 10 + val_char( buff[3] ) - 1900;

    date.tm_mon   = val_char( buff[4] ) * 10 + val_char( buff[5] );
    date.tm_mday  = val_char( buff[6] ) * 10 + val_char( buff[7] );
    date.tm_hour  = val_char( buff[8] ) * 10 + val_char( buff[9] );
    date.tm_min   = val_char( buff[10] ) * 10 + val_char( buff[11] );
    date.tm_sec   = val_char( buff[12] ) * 10 + val_char( buff[13] );
    date.tm_isdst = (buff[14] == 'D');

    return (mktime( &date ));
}

static void hs_parse_numeric(char *inbuf, struct scorefile_entry &se)
{
    se.version = hs_nextint(inbuf);
    se.release = hs_nextint(inbuf);

    // this would be a good point to check for version numbers and branch
    // appropriately

    // acceptable versions are 0 (converted from old hiscore format) and 4
    if (se.version != 0 && se.version != 4)
        return;

    se.points = hs_nextlong(inbuf);

    hs_nextstring(inbuf, se.name);

    se.uid = hs_nextlong(inbuf);
    se.race = hs_nextint(inbuf);
    se.cls = hs_nextint(inbuf);

    hs_nextstring(inbuf, se.race_class_name);

    se.lvl = hs_nextint(inbuf);
    se.best_skill = hs_nextint(inbuf);
    se.best_skill_lvl = hs_nextint(inbuf);
    se.death_type = hs_nextint(inbuf);
    se.death_source = hs_nextint(inbuf);
    se.mon_num = hs_nextint(inbuf);

    hs_nextstring(inbuf, se.death_source_name);

    // To try and keep the scorefile backwards compatible,
    // we'll branch on version > 4.0 to read the auxkilldata
    // text field.  
    if (se.version == 4 && se.release >= 1)
        hs_nextstring( inbuf, se.auxkilldata );
    else
        se.auxkilldata[0] = '\0';

    se.dlvl = hs_nextint(inbuf);
    se.level_type = hs_nextint(inbuf);
    se.branch = hs_nextint(inbuf);

    // Trying to fix some bugs that have been around since at 
    // least pr19, if not longer.  From now on, dlvl should
    // be calculated on death and need no further modification.
    if (se.version < 4 || se.release < 2)
    {
        if (se.level_type == LEVEL_DUNGEON)
        {
            if (se.branch == BRANCH_MAIN_DUNGEON)
                se.dlvl += 1;
            else if (se.branch < BRANCH_ORCISH_MINES)  // ie the hells
                se.dlvl -= 1;
        }
    }

    se.final_hp = hs_nextint(inbuf);
    if (se.version == 4 && se.release >= 2)
    {
        se.final_max_hp = hs_nextint(inbuf);
        se.final_max_max_hp = hs_nextint(inbuf);
        se.damage = hs_nextint(inbuf);
        se.str = hs_nextint(inbuf);
        se.intel = hs_nextint(inbuf);
        se.dex = hs_nextint(inbuf);
        se.god = hs_nextint(inbuf);
        se.piety = hs_nextint(inbuf);
        se.penance = hs_nextint(inbuf);
    }
    else
    {
        se.final_max_hp = -1;
        se.final_max_max_hp = -1;
        se.damage = -1;
        se.str = -1;
        se.intel = -1;
        se.dex = -1;
        se.god = -1;
        se.piety = -1;
        se.penance = -1;
    }

    se.wiz_mode = hs_nextint(inbuf);

    se.birth_time = hs_nextdate(inbuf);
    se.death_time = hs_nextdate(inbuf);

    if (se.version == 4 && se.release >= 2)
    {
        se.real_time = hs_nextint(inbuf);
        se.num_turns = hs_nextint(inbuf);
    }
    else
    {
        se.real_time = -1;
        se.num_turns = -1;
    }

    se.num_diff_runes = hs_nextint(inbuf);
    se.num_runes = hs_nextint(inbuf);
}

static void hs_write( FILE *scores, struct scorefile_entry &se )
{
    char buff[80];  // should be more than enough for date stamps

    se.version = 4;
    se.release = 2;

    fprintf( scores, ":%d:%d:%ld:%s:%ld:%d:%d:%s:%d:%d:%d",
             se.version, se.release, se.points, se.name,
             se.uid, se.race, se.cls, se.race_class_name, se.lvl,
             se.best_skill, se.best_skill_lvl );

    // XXX: need damage
    fprintf( scores, ":%d:%d:%d:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
             se.death_type, se.death_source, se.mon_num,
             se.death_source_name, se.auxkilldata, 
             se.dlvl, se.level_type, se.branch, 
             se.final_hp, se.final_max_hp, se.final_max_max_hp, se.damage,
             se.str, se.intel, se.dex, 
             se.god, se.piety, se.penance, se.wiz_mode );

    make_date_string( se.birth_time, buff );
    fprintf( scores, ":%s", buff );

    make_date_string( se.death_time, buff );
    fprintf( scores, ":%s", buff );

    fprintf( scores, ":%ld:%ld:%d:%d:\n", 
             se.real_time, se.num_turns, se.num_diff_runes, se.num_runes );
} 
// -------------------------------------------------------------------------
// functions dealing with old-style scorefile entries.
// -------------------------------------------------------------------------

static void hs_parse_string(char *inbuf, struct scorefile_entry &se)
{
    /* old entries are of the following format (Brent introduced some
       spacing at one point,  we have to take this into account):

   // Actually, I believe it might have been Brian who added the spaces, 
   // I was quite happy with the condensed version, given the 80 column
   // restriction. -- bwr

6263    BoBo       - DSD10 Wiz, killed by an acid blob on L1 of the Slime Pits.
5877    Aldus-DGM10, killed by a lethal dose of poison on L10.
5419    Yarf       - Koa10, killed by a warg on L1 of the Mines.

    1. All numerics up to the first non-numeric are the score
    2. All non '-' characters are the name.  Strip spaces.
    3. All alphabetics up to the first numeric are race/class
    4. All numerics up to the comma are the clevel
    5. From the comma, search for known fixed substrings and
       translate to death_type.  Leave death source = 0 for old
       scores,  and just copy in the monster name.
    6. Look for the branch type (again, substring search for
       fixed strings) and level.

    Very ugly and time consuming.

    */

    char scratch[80];

    // 1. get score
    hs_parse_generic_2(inbuf, scratch, "0123456789");

    se.version = 0;         // version # of converted score
    se.release = 0;
    se.points = atoi(scratch);

    // 2. get name
    hs_parse_generic_1(inbuf, scratch, "-");
    hs_stripblanks(scratch);
    strcpy(se.name, scratch);

    // 3. get race, class
    inbuf++;    // skip '-'
    hs_parse_generic_1(inbuf, scratch, "0123456789");
    hs_stripblanks(scratch);
    strcpy(se.race_class_name, scratch);
    se.race = 0;
    se.cls = 0;

    // 4. get clevel
    hs_parse_generic_2(inbuf, scratch, "0123456789");
    se.lvl = atoi(scratch);

    // 4a. get wizard mode
    hs_parse_generic_1(inbuf, scratch, ",");
    if (strstr(scratch, "Wiz") != NULL)
        se.wiz_mode = 1;
    else
        se.wiz_mode = 0;

    // 5. get death type
    inbuf++;    // skip comma
    hs_search_death(inbuf, se);

    // 6. get branch, level
    hs_search_where(inbuf, se);

    // set things that can't be picked out of old scorefile entries
    se.uid = 0;
    se.best_skill = 0;
    se.best_skill_lvl = 0;
    se.final_hp = 0;
    se.final_max_hp = -1;
    se.final_max_max_hp = -1;
    se.damage = -1;
    se.str = -1;
    se.intel = -1;
    se.dex = -1;
    se.god = -1;
    se.piety = -1;
    se.penance = -1;
    se.birth_time = 0;
    se.death_time = 0;
    se.real_time = -1;
    se.num_turns = -1;
    se.num_runes = 0; 
    se.num_diff_runes = 0; 
    se.auxkilldata[0] = '\0';
}

static void hs_parse_generic_1(char *&inbuf, char *outbuf, const char *stopvalues)
{
    char *p = outbuf;

    while(strchr(stopvalues, *inbuf) == NULL && *inbuf != '\0')
        *p++ = *inbuf++;

    *p = '\0';
}

static void hs_parse_generic_2(char *&inbuf, char *outbuf, const char *continuevalues)
{
    char *p = outbuf;

    while(strchr(continuevalues, *inbuf) != NULL && *inbuf != '\0')
        *p++ = *inbuf++;

    *p = '\0';
}

static void hs_stripblanks(char *buf)
{
    char *p = buf;
    char *q = buf;

    // strip leading
    while(*p == ' ')
        p++;
    while(*p != '\0')
        *q++ = *p++;

    *q-- = '\0';
    // strip trailing
    while(*q == ' ')
    {
        *q = '\0';
        q--;
    }
}

static void hs_search_death(char *inbuf, struct scorefile_entry &se)
{
    // assume killed by monster
    se.death_type = KILLED_BY_MONSTER;

    // sigh..
    if (strstr(inbuf, "killed by a lethal dose of poison") != NULL)
        se.death_type = KILLED_BY_POISON;
    else if (strstr(inbuf, "killed by a cloud") != NULL)
        se.death_type = KILLED_BY_CLOUD;
    else if (strstr(inbuf, "killed from afar by") != NULL)
        se.death_type = KILLED_BY_BEAM;
    else if (strstr(inbuf, "took a swim in molten lava") != NULL)
        se.death_type = KILLED_BY_LAVA;
    else if (strstr(inbuf, "soaked and fell apart") != NULL)
    {
        se.death_type = KILLED_BY_WATER;
        se.race = SP_MUMMY;
    }
    else if (strstr(inbuf, "drowned") != NULL)
        se.death_type = KILLED_BY_WATER;
    else if (strstr(inbuf, "died of stupidity") != NULL)
        se.death_type = KILLED_BY_STUPIDITY;
    else if (strstr(inbuf, "too weak to continue adventuring") != NULL)
        se.death_type = KILLED_BY_WEAKNESS;
    else if (strstr(inbuf, "slipped on a banana peel") != NULL)
        se.death_type = KILLED_BY_CLUMSINESS;
    else if (strstr(inbuf, "killed by a trap") != NULL)
        se.death_type = KILLED_BY_TRAP;
    else if (strstr(inbuf, "got out of the dungeon alive") != NULL)
        se.death_type = KILLED_BY_LEAVING;
    else if (strstr(inbuf, "escaped with the Orb") != NULL)
        se.death_type = KILLED_BY_WINNING;
    else if (strstr(inbuf, "quit") != NULL)
        se.death_type = KILLED_BY_QUITTING;
    else if (strstr(inbuf, "was drained of all life") != NULL)
        se.death_type = KILLED_BY_DRAINING;
    else if (strstr(inbuf, "starved to death") != NULL)
        se.death_type = KILLED_BY_STARVATION;
    else if (strstr(inbuf, "froze to death") != NULL)
        se.death_type = KILLED_BY_FREEZING;
    else if (strstr(inbuf, "burnt to a crisp") != NULL)
        se.death_type = KILLED_BY_BURNING;
    else if (strstr(inbuf, "killed by wild magic") != NULL)
        se.death_type = KILLED_BY_WILD_MAGIC;
    else if (strstr(inbuf, "killed by Xom") != NULL)
        se.death_type = KILLED_BY_XOM;
    else if (strstr(inbuf, "killed by a statue") != NULL)
        se.death_type = KILLED_BY_STATUE;
    else if (strstr(inbuf, "rotted away") != NULL)
        se.death_type = KILLED_BY_ROTTING;
    else if (strstr(inbuf, "killed by bad target") != NULL)
        se.death_type = KILLED_BY_TARGETTING;
    else if (strstr(inbuf, "killed by an exploding spore") != NULL)
        se.death_type = KILLED_BY_SPORE;
    else if (strstr(inbuf, "smote by The Shining One") != NULL)
        se.death_type = KILLED_BY_TSO_SMITING;
    else if (strstr(inbuf, "turned to stone") != NULL)
        se.death_type = KILLED_BY_PETRIFICATION;
    else if (strstr(inbuf, "eviscerated by a hatching") != NULL)
        se.death_type = KILLED_BY_SHUGGOTH;

    // whew!

    // now, if we're still KILLED_BY_MONSTER,  make sure that there is
    // a "killed by" somewhere,  or else we're setting it to UNKNOWN.
    if (se.death_type == KILLED_BY_MONSTER)
    {
        if (strstr(inbuf, "killed by") == NULL)
            se.death_type = KILLED_BY_SOMETHING;
    }

    // set some fields
    se.death_source = 0;
    se.mon_num = 0;
    strcpy(se.death_source_name, "");

    // now try to pull the monster out.
    if (se.death_type == KILLED_BY_MONSTER || se.death_type == KILLED_BY_BEAM)
    {
        char *p = strstr(inbuf, " by ");
        p += 4;
        char *q = strstr(inbuf, " on ");
        if (q == NULL)
            q = strstr(inbuf, " in ");
        char *d = se.death_source_name;
        while(p != q)
            *d++ = *p++;

        *d = '\0';
    }
}

static void hs_search_where(char *inbuf, struct scorefile_entry &se)
{
    char scratch[6];

    se.level_type = LEVEL_DUNGEON;
    se.branch = BRANCH_MAIN_DUNGEON;
    se.dlvl = 0;

    // early out
    if (se.death_type == KILLED_BY_LEAVING || se.death_type == KILLED_BY_WINNING)
        return;

    // here we go again.
    if (strstr(inbuf, "in the Abyss") != NULL)
        se.level_type = LEVEL_ABYSS;
    else if (strstr(inbuf, "in Pandemonium") != NULL)
        se.level_type = LEVEL_PANDEMONIUM;
    else if (strstr(inbuf, "in a labyrinth") != NULL)
        se.level_type = LEVEL_LABYRINTH;

    // early out for special level types
    if (se.level_type != LEVEL_DUNGEON)
        return;

    // check for vestible
    if (strstr(inbuf, "in the Vestibule") != NULL)
    {
        se.branch = BRANCH_VESTIBULE_OF_HELL;
        return;
    }

    // from here,  we have branch and level.
    char *p = strstr(inbuf, "on L");
    if (p != NULL)
    {
        p += 4;
        hs_parse_generic_2(p, scratch, "0123456789");
        se.dlvl = atoi( scratch );
    }

    // get branch.
    if (strstr(inbuf, "of Dis") != NULL)
        se.branch = BRANCH_DIS;
    else if (strstr(inbuf, "of Gehenna") != NULL)
        se.branch = BRANCH_GEHENNA;
    else if (strstr(inbuf, "of Cocytus") != NULL)
        se.branch = BRANCH_COCYTUS;
    else if (strstr(inbuf, "of Tartarus") != NULL)
        se.branch = BRANCH_TARTARUS;
    else if (strstr(inbuf, "of the Mines") != NULL)
        se.branch = BRANCH_ORCISH_MINES;
    else if (strstr(inbuf, "of the Hive") != NULL)
        se.branch = BRANCH_HIVE;
    else if (strstr(inbuf, "of the Lair") != NULL)
        se.branch = BRANCH_LAIR;
    else if (strstr(inbuf, "of the Slime Pits") != NULL)
        se.branch = BRANCH_SLIME_PITS;
    else if (strstr(inbuf, "of the Vaults") != NULL)
        se.branch = BRANCH_VAULTS;
    else if (strstr(inbuf, "of the Crypt") != NULL)
        se.branch = BRANCH_CRYPT;
    else if (strstr(inbuf, "of the Hall") != NULL)
        se.branch = BRANCH_HALL_OF_BLADES;
    else if (strstr(inbuf, "of Zot's Hall") != NULL)
        se.branch = BRANCH_HALL_OF_ZOT;
    else if (strstr(inbuf, "of the Temple") != NULL)
        se.branch = BRANCH_ECUMENICAL_TEMPLE;
    else if (strstr(inbuf, "of the Snake Pit") != NULL)
        se.branch = BRANCH_SNAKE_PIT;
    else if (strstr(inbuf, "of the Elf Hall") != NULL)
        se.branch = BRANCH_ELVEN_HALLS;
    else if (strstr(inbuf, "of the Tomb") != NULL)
        se.branch = BRANCH_TOMB;
    else if (strstr(inbuf, "of the Swamp") != NULL)
        se.branch = BRANCH_SWAMP;
}

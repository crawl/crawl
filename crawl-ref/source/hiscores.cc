/*
 *  File: highscore.cc
 *  Summary: deal with reading and writing of highscore file
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "misc.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
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
            info[75] = 0;
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
    else if (aux[0] == 0                                // unknown
            || strncmp( aux, "Hit ", 4 ) == 0           // thrown
            || strncmp( aux, "volley ", 7 ) == 0)       // manticore spikes
    {
        return ("hit from afar");
    }

    return ("blasted");                                 // spells, wands
}

void hiscores_format_single(char *buf, const scorefile_entry &se)
{
    std::string line = se.hiscore_line(scorefile_entry::DDV_ONELINE);
    strncpy(buf, line.c_str(), HIGHSCORE_SIZE);
    buf[HIGHSCORE_SIZE - 1] = 0;
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

static std::string hiscore_newline_string()
{
    return (EOL "             ");
}

void hiscores_format_single_long( char *buf, const scorefile_entry &se, 
                                 bool verbose )
{
    std::string line = 
        se.hiscore_line( 
                verbose? 
                    scorefile_entry::DDV_VERBOSE 
                  : scorefile_entry::DDV_NORMAL );
    strncpy(buf, line.c_str(), HIGHSCORE_SIZE);
    buf[HIGHSCORE_SIZE - 1] = 0;
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
#ifdef SHARED_FILES_CHMOD_PUBLIC
    chmod(SAVE_DIR_PATH "scores", SHARED_FILES_CHMOD_PUBLIC);
#endif
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
    dest.name[0] = 0;
    dest.uid = 0;
    dest.race = 0; 
    dest.cls = 0;
    dest.lvl = 0;
    dest.race_class_name[0] = 0;
    dest.best_skill = 0;
    dest.best_skill_lvl = 0;
    dest.death_type = KILLED_BY_SOMETHING;
    dest.death_source = 0;
    dest.mon_num = 0;
    dest.death_source_name[0] = 0;
    dest.auxkilldata[0] = 0;
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

    if (*inbuf == 0)
    {
        *p = 0;
        return;
    }

    // assume we're on a ':'
    inbuf ++;
    while(*inbuf != ':' && *inbuf != 0)
        *p++ = *inbuf++;

    *p = 0;
}

static int hs_nextint(char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num);

    return (num[0] == 0 ? 0 : atoi(num));
}

static long hs_nextlong(char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num);

    return (num[0] == 0 ? 0 : atol(num));
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
        se.auxkilldata[0] = 0;

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
    se.auxkilldata[0] = 0;
}

static void hs_parse_generic_1(char *&inbuf, char *outbuf, const char *stopvalues)
{
    char *p = outbuf;

    while(strchr(stopvalues, *inbuf) == NULL && *inbuf != 0)
        *p++ = *inbuf++;

    *p = 0;
}

static void hs_parse_generic_2(char *&inbuf, char *outbuf, const char *continuevalues)
{
    char *p = outbuf;

    while(strchr(continuevalues, *inbuf) != NULL && *inbuf != 0)
        *p++ = *inbuf++;

    *p = 0;
}

static void hs_stripblanks(char *buf)
{
    char *p = buf;
    char *q = buf;

    // strip leading
    while(*p == ' ')
        p++;
    while(*p != 0)
        *q++ = *p++;

    *q-- = 0;
    // strip trailing
    while(*q == ' ')
    {
        *q = 0;
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
    else if (strstr(inbuf, "asphyxiated"))
        se.death_type = KILLED_BY_CURARE;
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
    else if (strstr(inbuf, "melted into a puddle") != NULL)
        se.death_type = KILLED_BY_MELTING;
    else if (strstr(inbuf, "bled to death") != NULL)
        se.death_type = KILLED_BY_BLEEDING;

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

        *d = 0;
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

//////////////////////////////////////////////////////////////////////////
// scorefile_entry

scorefile_entry::scorefile_entry(int dam, int dsource, int dtype,
                                 const char *aux, bool death_cause_only)
{
    init_death_cause(dam, dsource, dtype, aux);
    if (!death_cause_only)
        init();
}

scorefile_entry::scorefile_entry()
{
    // Completely uninitialized, caveat user.
}

void scorefile_entry::init_death_cause(int dam, int dsrc, 
                                       int dtype, const char *aux)
{
    death_source = dsrc;
    death_type   = dtype;
    damage       = dam;

    // Set the default aux data value... 
    // If aux is passed in (ie for a trap), we'll default to that.
    if (aux == NULL)
        auxkilldata[0] = 0;
    else
    {
        strncpy( auxkilldata, aux, ITEMNAME_SIZE );
        auxkilldata[ ITEMNAME_SIZE - 1 ] = 0;
    }

    // for death by monster
    if ((death_type == KILLED_BY_MONSTER || death_type == KILLED_BY_BEAM)  
        && death_source >= 0 && death_source < MAX_MONSTERS)
    {
        const monsters *monster = &menv[death_source];

        if (monster->type > 0 || monster->type <= NUM_MONSTERS) 
        { 
            death_source = monster->type;
            mon_num = monster->number;

            // Previously the weapon was only used for dancing weapons,
            // but now we pass it in as a string through the scorefile
            // entry to be appended in hiscores_format_single in long or
            // medium scorefile formats.
            // It still isn't used in monam for anything but flying weapons
            // though
            if (death_type == KILLED_BY_MONSTER 
                && monster->inv[MSLOT_WEAPON] != NON_ITEM)
            {
                // [ds] The highscore entry may be constructed while the player
                // is alive (for notes), so make sure we don't reveal info we
                // shouldn't.
                if (you.hp <= 0)
                {
#if HISCORE_WEAPON_DETAIL
                    set_ident_flags( mitm[monster->inv[MSLOT_WEAPON]],
                                     ISFLAG_IDENT_MASK );
#else
                    // changing this to ignore the pluses to keep it short
                    unset_ident_flags( mitm[monster->inv[MSLOT_WEAPON]],
                                       ISFLAG_IDENT_MASK );

                    set_ident_flags( mitm[monster->inv[MSLOT_WEAPON]], 
                                     ISFLAG_KNOW_TYPE );

                    // clear "runed" description text to make shorter yet
                    set_equip_desc( mitm[monster->inv[MSLOT_WEAPON]], 0 );
#endif
                }

                // Setting this is redundant for dancing weapons, however
                // we do care about the above indentification. -- bwr
                if (monster->type != MONS_DANCING_WEAPON) 
                {
                    it_name( monster->inv[MSLOT_WEAPON], 
                            DESC_NOCAP_A, 
                            info );
                    strncpy( auxkilldata, info, ITEMNAME_SIZE );
                    auxkilldata[ ITEMNAME_SIZE - 1 ] = 0;
                }
            }
            
            strcpy( info,
                    monam( monster->number, monster->type, true, DESC_NOCAP_A, 
                           monster->inv[MSLOT_WEAPON] ) );

            strncpy( death_source_name, info, 40 );
            death_source_name[39] = 0;
        }
    }
    else
    {
        death_source = death_source;
        mon_num = 0;
        death_source_name[0] = 0;
    }
}

void scorefile_entry::init()
{
    // Score file entry version:
    //
    // 4.0      - original versioned entry
    // 4.1      - added real_time and num_turn fields
    // 4.2      - stats and god info

    version = 4;
    release = 2;
    strncpy( name, you.your_name, kNameLen );
    
    name[ kNameLen - 1 ] = 0;
#ifdef MULTIUSER
    uid = (int) getuid();
#else
    uid = 0;
#endif

    // do points first.
    points = you.gold;
    points += (you.experience * 7) / 10;

    //if (death_type == KILLED_BY_WINNING) points += points / 2;
    //if (death_type == KILLED_BY_LEAVING) points += points / 10;
    // these now handled by giving player the value of their inventory
    char temp_id[4][50];

    for (int d = 0; d < 4; d++)
    {
        for (int e = 0; e < 50; e++)
            temp_id[d][e] = 1;
    }

    FixedVector< int, NUM_RUNE_TYPES >  rune_array;

    num_runes = 0;
    num_diff_runes = 0;

    for (int i = 0; i < NUM_RUNE_TYPES; i++)
        rune_array[i] = 0;

    // Calculate value of pack and runes when character leaves dungeon
    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
    {
        for (int d = 0; d < ENDOFPACK; d++)
        {
            if (is_valid_item( you.inv[d] ))
            {
                points += item_value( you.inv[d], temp_id, true );

                if (you.inv[d].base_type == OBJ_MISCELLANY
                    && you.inv[d].sub_type == MISC_RUNE_OF_ZOT)
                {
                    if (rune_array[ you.inv[d].plus ] == 0)
                        num_diff_runes++;

                    num_runes += you.inv[d].quantity;
                    rune_array[ you.inv[d].plus ] += you.inv[d].quantity;
                }
            }
        }

        // Bonus for exploring different areas, not for collecting a 
        // huge stack of demonic runes in Pandemonium (gold value 
        // is enough for those). -- bwr
        if (num_diff_runes >= 3)
            points += ((num_diff_runes + 2) * (num_diff_runes + 2) * 1000);
    }

    // Players will have a hard time getting 1/10 of this (see XP cap):
    if (points > 99999999)
        points = 99999999;

    race = you.species;
    cls = you.char_class;

    race_class_name[0] = 0;

    lvl = you.experience_level;
    best_skill = ::best_skill( SK_FIGHTING, NUM_SKILLS - 1, 99 );
    best_skill_lvl = you.skills[ best_skill ];

    final_hp = you.hp;
    final_max_hp = you.hp_max;
    final_max_max_hp = you.hp_max + player_rotted();
    str = you.strength;
    intel = you.intel;
    dex = you.dex;

    god = you.religion;
    if (you.religion != GOD_NO_GOD)
    {
        piety = you.piety;
        penance = you.penance[you.religion];
    }

    // main dungeon: level is simply level
    dlvl = you.your_level + 1;
    switch (you.where_are_you)
    {
        case BRANCH_ORCISH_MINES:
        case BRANCH_HIVE:
        case BRANCH_LAIR:
        case BRANCH_SLIME_PITS:
        case BRANCH_VAULTS:
        case BRANCH_CRYPT:
        case BRANCH_HALL_OF_BLADES:
        case BRANCH_HALL_OF_ZOT:
        case BRANCH_ECUMENICAL_TEMPLE:
        case BRANCH_SNAKE_PIT:
        case BRANCH_ELVEN_HALLS:
        case BRANCH_TOMB:
        case BRANCH_SWAMP:
            dlvl = you.your_level - you.branch_stairs[you.where_are_you - 10];
            break;

        case BRANCH_DIS:
        case BRANCH_GEHENNA:
        case BRANCH_VESTIBULE_OF_HELL:
        case BRANCH_COCYTUS:
        case BRANCH_TARTARUS:
        case BRANCH_INFERNO:
        case BRANCH_THE_PIT:
            dlvl = you.your_level - 26;
            break;
    }

    branch = you.where_are_you;      // no adjustments necessary.
    level_type = you.level_type;     // pandemonium, labyrinth, dungeon..

    birth_time = you.birth_time;     // start time of game
    death_time = time( NULL );         // end time of game

    if (you.real_time != -1)
        real_time = you.real_time + (death_time - you.start_time);
    else 
        real_time = -1;

    num_turns = you.num_turns;

#ifdef WIZARD
    wiz_mode = (you.wizard ? 1 : 0);
#else
    wiz_mode = 0;
#endif
}

std::string scorefile_entry::hiscore_line(death_desc_verbosity verbosity) const
{
    std::string line = character_description(verbosity);
    line += death_description(verbosity);
    line += death_place(verbosity);
    line += game_time(verbosity);

    return (line);
}

std::string scorefile_entry::game_time(death_desc_verbosity verbosity) const
{
    std::string line;

    if (verbosity == DDV_VERBOSE)
    {
        if (real_time > 0)
        {
            char username[80] = "The";
            char scratch[INFO_SIZE];
            char tmp[80];

#ifdef MULTIUSER
            if (uid > 0)
            {
                struct passwd *pw_entry = getpwuid( uid );
                strncpy( username, pw_entry->pw_name, sizeof(username) );
                strncat( username, "'s", sizeof(username) );
                username[0] = toupper( username[0] );
            }
#endif

            make_time_string( real_time, tmp, sizeof(tmp) );

            snprintf( scratch, INFO_SIZE, "%s game lasted %s (%ld turns).",
                      username, tmp, num_turns );

            line += scratch;
            line += hiscore_newline_string();
        }
    }

    return (line);
}

const char *scorefile_entry::damage_verb() const
{
    // GDL: here's an example of using final_hp.  Verbiage could be better.
    // bwr: changed "blasted" since this is for melee
    return (final_hp > -6)  ? "Slain"   :
           (final_hp > -14) ? "Mangled" :
           (final_hp > -22) ? "Demolished" :
                              "Annihilated";
}

const char *scorefile_entry::death_source_desc() const
{
    if (death_type != KILLED_BY_MONSTER && death_type != KILLED_BY_BEAM)
        return ("");

    return (*death_source_name? 
                death_source_name
              : monam( mon_num, death_source, true, DESC_PLAIN ) );
}

std::string scorefile_entry::damage_string(bool terse) const
{
    char scratch[50];
    snprintf( scratch, sizeof scratch, "(%d%s)", damage, 
            terse? "" : " damage" );
    return (scratch);
}

std::string scorefile_entry::strip_article_a(const std::string &s) const
{
    if (s.find("a ") == 0)
        return (s.substr(2));
    else if (s.find("an ") == 0)
        return (s.substr(3));
    return (s);
}

std::string scorefile_entry::terse_missile_cause() const
{
    std::string cause;
    std::string aux = auxkilldata;

    std::string monster_prefix = " by ";
    // We're looking for Shot with a%s %s by %s/ Hit by a%s %s thrown by %s
    std::string::size_type by = aux.rfind(monster_prefix);
    if (by == std::string::npos)
        return ("???");

    std::string mcause = aux.substr(by + monster_prefix.length());
    mcause = strip_article_a(mcause);

    std::string missile;
    const std::string pre_post[][2] = {
        { "Shot with a", " by " },
        { "Hit by a",    " thrown by " }
    };

    for (unsigned i = 0; i < sizeof(pre_post) / sizeof(*pre_post); ++i)
    {
        if (aux.find(pre_post[i][0]) != 0)
            continue;

        std::string::size_type end = aux.rfind(pre_post[i][1]);
        if (end == std::string::npos)
            continue;

        int istart = pre_post[i][0].length();
        int nchars = end - istart;
        missile = aux.substr(istart, nchars);

        // Was this prefixed by "an"?
        if (missile.find("n ") == 0)
            missile = missile.substr(2);
    }

    if (missile.length())
        mcause += "/" + missile;

    return (mcause);
}

std::string scorefile_entry::terse_beam_cause() const
{
    std::string cause = auxkilldata;
    if (cause.find("by ") == 0 || cause.find("By ") == 0)
        cause = cause.substr(3);
    return (cause);
}

std::string scorefile_entry::terse_wild_magic() const
{
    return terse_beam_cause();
}

std::string scorefile_entry::terse_trap() const
{
    std::string trap = *auxkilldata? std::string(auxkilldata) + " trap"
                                   : "trap";
    if (trap.find("n ") == 0)
        trap = trap.substr(2);
    trim_string(trap);

    return (trap);
}

std::string scorefile_entry::single_cdesc() const
{
    char  scratch[INFO_SIZE];
    char  buf[INFO_SIZE];

    if (!*race_class_name)
    {
        snprintf( scratch, sizeof(scratch), "%s%s", 
                  get_species_abbrev( race ), get_class_abbrev( cls ) );
    }
    else
    {
        strncpy( scratch, race_class_name, sizeof scratch );
        scratch[ sizeof(scratch) - 1 ] = 0;
    }


    std::string scname = name;
    if (scname.length() > 10)
        scname = scname.substr(0, 10);

    snprintf( buf, sizeof buf, 
             "%8ld %-10s %s-%02d%s", points, scname.c_str(),
             scratch, lvl, (wiz_mode == 1) ? "W" : "" );

    return (buf);
}

std::string
scorefile_entry::character_description(death_desc_verbosity verbosity) const
{
    bool single  = verbosity == DDV_TERSE || verbosity == DDV_ONELINE;

    if (single)
        return single_cdesc();

    bool verbose = verbosity == DDV_VERBOSE;
    char  scratch[INFO_SIZE];
    char  buf[INFO_SIZE];

    std::string desc;
    // Please excuse the following bit of mess in the name of flavour ;)
    if (verbose)
    {
        strncpy( scratch, skill_title( best_skill, best_skill_lvl, 
                                       race, str, dex, god ), 
                 INFO_SIZE );

        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s (level %d",
                  points, name, scratch, lvl );

        desc = buf;
    }
    else 
    {
        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s %s (level %d",
                  points, name, species_name(race, lvl), 
                  get_class_name(cls), lvl );
        desc = buf;
    }

    if (final_max_max_hp > 0)  // as the other two may be negative
    {
        snprintf( scratch, INFO_SIZE, ", %d/%d", final_hp, final_max_hp );
        desc += scratch;

        if (final_max_hp < final_max_max_hp)
        {
            snprintf( scratch, INFO_SIZE, " (%d)", final_max_max_hp );
            desc += scratch;
        }

        desc += " HPs";
    }

    desc += wiz_mode? ") *WIZ*" : ")";
    desc += hiscore_newline_string();

    if (verbose)
    {
        const char *const srace = species_name( race, lvl );

        snprintf( scratch, INFO_SIZE, "Began as a%s %s %s",
                  is_vowel(*srace) ? "n" : "", srace, get_class_name(cls) );
        desc += scratch;

        if (birth_time > 0)
        {
            desc += " on ";
            hiscore_date_string( birth_time, scratch );
            desc += scratch;
        }

        desc += ".";
        desc += hiscore_newline_string();

        if (race != SP_DEMIGOD && god != -1)
        {
            if (god == GOD_XOM)
            {
                snprintf( scratch, INFO_SIZE, "Was a %sPlaything of Xom.",
                          (lvl >= 20) ? "Favourite " : "" );

                desc += scratch;
                desc += hiscore_newline_string();
            }
            else if (god != GOD_NO_GOD)
            {
                // Not exactly the same as the religon screen, but
                // good enough to fill this slot for now.
                snprintf( scratch, INFO_SIZE, "Was %s of %s%s",
                          (piety >  160) ? "the Champion" :
                          (piety >= 120) ? "a High Priest" :
                          (piety >= 100) ? "an Elder" :
                          (piety >=  75) ? "a Priest" :
                          (piety >=  50) ? "a Believer" :
                          (piety >=  30) ? "a Follower" 
                                            : "an Initiate",
                          god_name( god ), 
                          (penance > 0) ? " (penitent)." : "." );

                desc += scratch;
                desc += hiscore_newline_string();
            }
        }
    }

    return (desc);
}

std::string scorefile_entry::death_place(death_desc_verbosity verbosity) const
{
    bool verbose = verbosity == DDV_VERBOSE;
    std::string place;

    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
        return ("");

    char scratch[ INFO_SIZE ];

    if (verbosity == DDV_ONELINE || verbosity == DDV_TERSE)
    {
        snprintf( scratch, sizeof scratch,  " (%s)",
                  place_name(get_packed_place(branch, dlvl, level_type),
                             false, true).c_str());
        return (scratch);
    }

    if (verbose && death_type != KILLED_BY_QUITTING)
        place += "...";

    // where did we die?
    std::string placename =
        place_name(get_packed_place(branch, dlvl, level_type),
                   true, true);

    // add appropriate prefix
    if (placename.find("Level") == 0)
        place += " on ";
    else
        place += " in ";

    place += placename;

    if (verbose && death_time 
        && !hiscore_same_day( birth_time, death_time ))
    {
        place += " on ";
        hiscore_date_string( death_time, scratch );
        place += scratch;
    }

    place += ".";
    place += hiscore_newline_string();

    return (place);
}

std::string 
scorefile_entry::death_description(death_desc_verbosity verbosity) const
{
    bool needs_beam_cause_line = false;
    bool needs_called_by_monster_line = false;
    bool needs_damage = false;

    bool terse   = verbosity == DDV_TERSE;
    bool verbose = verbosity == DDV_VERBOSE;
    bool oneline = verbosity == DDV_ONELINE;

    char scratch[INFO_SIZE];

    std::string desc;
    
    if (oneline)
        desc = " ";

    switch (death_type)
    {
    case KILLED_BY_MONSTER:
        if (terse)
            desc += death_source_desc();
        else if (oneline)
            desc += std::string("slain by ") + death_source_desc();
        else
        {
            snprintf( scratch, sizeof scratch, "%s by %s",
                        damage_verb(),
                        death_source_desc() );

            desc += scratch;
        }

        // put the damage on the weapon line if there is one
        if (auxkilldata[0] == 0)
            needs_damage = true;
        break;

    case KILLED_BY_POISON:
        desc += terse? "poison" : "Succumbed to poison";
        break;

    case KILLED_BY_CLOUD:
        if (auxkilldata[0] == 0)
            desc += terse? "cloud" : "Engulfed by a cloud";
        else
        {
            snprintf( scratch, sizeof(scratch), "%scloud of %s",
                      terse? "" : "Engulfed by a ",
                      auxkilldata );
            desc += scratch;
        }
        needs_damage = true;
        break;

    case KILLED_BY_BEAM:
        if (oneline)
        {
            // keeping this short to leave room for the deep elf spellcasters:
            snprintf( scratch, sizeof(scratch), "%s by ", 
                      range_type_verb( auxkilldata ) );  
            desc += scratch;
            desc += death_source_desc();
        }
        else if (isupper( auxkilldata[0] ))  // already made (ie shot arrows)
        {
            // If terse we have to parse the information from the string.
            // Darn it to heck.
            desc += terse? terse_missile_cause() : auxkilldata;
            needs_damage = true;
        }
        else if (verbose && strncmp( auxkilldata, "by ", 3 ) == 0)
        {
            // "by" is used for priest attacks where the effect is indirect
            // in verbose format we have another line for the monster
            needs_called_by_monster_line = true;
            snprintf( scratch, sizeof(scratch), "Killed %s", auxkilldata );
            desc += scratch;
        }
        else
        {
            // Note: This is also used for the "by" cases in non-verbose
            //       mode since listing the monster is more imporatant.
            if (!terse)
                desc += "Killed from afar by ";

            desc += death_source_desc();

            if (*auxkilldata)
                needs_beam_cause_line = true;

            needs_damage = true;
        }
        break;

    case KILLED_BY_CURARE:
        desc += terse? "asphyx" : "Asphyxiated";
        break;

    case KILLED_BY_LAVA:
        if (terse)
            desc += "lava";
        else
        {
            if (race == SP_MUMMY)
                desc += "Turned to ash by lava";
            else
                desc += "Took a swim in molten lava";
        }
        break;

    case KILLED_BY_WATER:
        if (race == SP_MUMMY)
            desc += terse? "fell apart" : "Soaked and fell apart";
        else
            desc += terse? "drowned" : "Drowned";
        break;

    case KILLED_BY_STUPIDITY:
        desc += terse? "stupidity" : "Forgot to breathe";
        break;

    case KILLED_BY_WEAKNESS:
        desc += terse? "collapsed " : "Collapsed under their own weight";
        break;

    case KILLED_BY_CLUMSINESS:
        desc += terse? "clumsiness" : "Slipped on a banana peel";
        break;

    case KILLED_BY_TRAP:
        if (terse)
            desc += terse_trap();
        else
        {
            snprintf( scratch, sizeof(scratch), "Killed by triggering a%s trap", 
                     (*auxkilldata) ? auxkilldata : "" );
            desc += scratch;
        }
        needs_damage = true;
        break;

    case KILLED_BY_LEAVING:
        if (terse)
            desc += "left";
        else
        {
            if (num_runes > 0)
                desc += "Got out of the dungeon";
            else
                desc += "Got out of the dungeon alive";
        }
        break;

    case KILLED_BY_WINNING:
        desc += terse? "escaped" : "Escaped with the Orb";
        if (num_runes < 1)
            desc += "!";
        break;

    case KILLED_BY_QUITTING:
        desc += terse? "quit" : "Quit the game";
        break;

    case KILLED_BY_DRAINING:
        desc += terse? "drained" : "Was drained of all life";
        break;

    case KILLED_BY_STARVATION:
        desc += terse? "starvation" : "Starved to death";
        break;

    case KILLED_BY_FREEZING:    // refrigeration spell
        desc += terse? "frozen" : "Froze to death";
        needs_damage = true;
        break;

    case KILLED_BY_BURNING:     // sticky flame
        desc += terse? "burnt" : "Burnt to a crisp";
        needs_damage = true;
        break;

    case KILLED_BY_WILD_MAGIC:
        if (!*auxkilldata)
            desc += terse? "wild magic" : "Killed by wild magic";
        else
        {
            if (terse)
                desc += terse_wild_magic();
            else
            {
                // A lot of sources for this case... some have "by" already.
                snprintf( scratch, sizeof(scratch), "Killed %s%s", 
                      (strncmp( auxkilldata, "by ", 3 ) != 0) ? "by " : "",
                      auxkilldata );
                desc += scratch;
            }
        }

        needs_damage = true;
        break;

    case KILLED_BY_XOM:  // only used for old Xom kills
        desc += terse? "xom" : "Killed for Xom's enjoyment";
        needs_damage = true;
        break;

    case KILLED_BY_STATUE:
        desc += terse? "statue" : "Killed by a statue";
        needs_damage = true;
        break;

    case KILLED_BY_ROTTING:
        desc += terse? "rotting" : "Rotted away";
        break;

    case KILLED_BY_TARGETTING:
        desc += terse? "shot self" : "Killed themselves with bad targeting";
        needs_damage = true;
        break;

    case KILLED_BY_SPORE:
        desc += terse? "spore" : "Killed by an exploding spore";
        needs_damage = true;
        break;

    case KILLED_BY_TSO_SMITING:
        desc += terse? "smote by Shining One" : "Smote by The Shining One";
        needs_damage = true;
        break;

    case KILLED_BY_PETRIFICATION:
        desc += terse? "petrified" : "Turned to stone";
        break;

    case KILLED_BY_MELTING:
        desc += terse? "melted" : " melted into a puddle";
        break;

    case KILLED_BY_BLEEDING:
        desc += terse? "bleeding" : " bled to death";
        break;

    case KILLED_BY_SOMETHING:
        if (auxkilldata)
            desc += auxkilldata;
        else
            desc += terse? "died" : "Died";
        needs_damage = true;
        break;

    case KILLED_BY_FALLING_DOWN_STAIRS:
        desc += terse? "fell downstairs" : "Fell down a flight of stairs";
        needs_damage = true;
        break;

    case KILLED_BY_ACID:
        desc += terse? "acid" : "Splashed by acid";
        needs_damage = true;
        break;

    default:
        desc += terse? "program bug" : "Nibbled to death by software bugs";
        break;
    }                           // end switch

    if (oneline && desc.length() > 2)
        desc[1] = tolower(desc[1]);

    // TODO: Eventually, get rid of "..." for cases where the text fits.
    if (terse)
    {
        if (death_type == KILLED_BY_MONSTER && *auxkilldata)
        {
            desc += "/";
            desc += strip_article_a(auxkilldata);
            needs_damage = true;
        }
        else if (needs_beam_cause_line)
            desc += "/" + terse_beam_cause();
        else if (needs_called_by_monster_line)
            desc += death_source_name;

        if (needs_damage && damage > 0)
            desc += " " + damage_string(true);
    }
    else if (verbose)
    {
        bool done_damage = false;  // paranoia

        if (needs_damage && damage > 0)
        {
            desc += " " + damage_string();
            needs_damage = false;
            done_damage = true;
        }

        if ((death_type == KILLED_BY_LEAVING 
                        || death_type == KILLED_BY_WINNING)
                    && num_runes > 0)
        {
            desc += hiscore_newline_string();

            snprintf( scratch, INFO_SIZE, "... %s %d rune%s", 
                      (death_type == KILLED_BY_WINNING) ? "and" : "with",
                      num_runes, (num_runes > 1) ? "s" : "" );
            desc += scratch;

            if (num_diff_runes > 1)
            {
                snprintf( scratch, INFO_SIZE, " (of %d types)",
                          num_diff_runes );
                desc += scratch;
            }

            if (death_time > 0 
                && !hiscore_same_day( birth_time, death_time ))
            {
                desc += " on ";
                hiscore_date_string( death_time, scratch );
                desc += scratch;
            }

            desc += "!";
            desc += hiscore_newline_string();
        }
        else if (death_type != KILLED_BY_QUITTING)
        {
            desc += hiscore_newline_string();

            if (death_type == KILLED_BY_MONSTER && auxkilldata[0])
            {
                snprintf(scratch, INFO_SIZE, "... wielding %s", auxkilldata);
                desc += scratch;
                needs_damage = true;
            }
            else if (needs_beam_cause_line)
            {
                desc += (is_vowel( auxkilldata[0] )) ? "... with an " 
                                                        : "... with a ";
                desc += auxkilldata;
                needs_damage = true;
            }
            else if (needs_called_by_monster_line)
            {
                snprintf( scratch, sizeof(scratch), "... called down by %s",
                          death_source_name );
                desc += scratch;
                needs_damage = true;
            }

            if (needs_damage && !done_damage && damage > 0) 
                desc += " " + damage_string();

            if (needs_damage)
                desc += hiscore_newline_string();
        }
    }

    if (!oneline)
    {
        if (death_type == KILLED_BY_LEAVING
            || death_type == KILLED_BY_WINNING)
        {
            // TODO: strcat "after reaching level %d"; for LEAVING
            if (verbosity == DDV_NORMAL)
            {
                if (num_runes > 0)
                    desc += "!";
            }
            desc += hiscore_newline_string();
        }
    }

    if (terse)
    {
        trim_string(desc);
        desc = strip_article_a(desc);
    }

    return (desc);
}

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

#include "branch.h"
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
#include "version.h"
#include "view.h"

#include "skills2.h"

#define SCORE_VERSION "0.1"

#ifdef MULTIUSER
    // includes to get passwd file access:
    #include <pwd.h>
    #include <sys/types.h>
#endif

// enough memory allocated to snarf in the scorefile entries
static scorefile_entry hs_list[SCORE_FILE_ENTRIES];

// hackish: scorefile position of newest entry.  Will be highlit during
// highscore printing (always -1 when run from command line).
static int newest_entry = -1;

static FILE *hs_open(const char *mode, const std::string &filename);
static void hs_close(FILE *handle, const char *mode,
                     const std::string &filename);
static bool hs_read(FILE *scores, scorefile_entry &dest);
static void hs_write(FILE *scores, scorefile_entry &entry);
static void hs_nextstring(const char *&inbuf, char *dest, size_t bufsize);
static int hs_nextint(const char *&inbuf);
static long hs_nextlong(const char *&inbuf);
static time_t parse_time(const std::string &st);

// file locking stuff
#ifdef USE_FILE_LOCKING
static bool lock_file_handle( FILE *handle, int type );
static bool unlock_file_handle( FILE *handle );
#endif // USE_FILE_LOCKING

std::string score_file_name()
{
    if (!SysEnv.scorefile.empty())
        return (SysEnv.scorefile);
    
    return (Options.save_dir + "scores");
}

std::string log_file_name()
{
    return (Options.save_dir + "logfile");
}

void hiscores_new_entry( const scorefile_entry &ne )
{
    unwind_bool score_update(crawl_state.updating_scores, true);
    
    FILE *scores;
    int i, total_entries;
    bool inserted = false;

    // open highscore file (reading) -- note that NULL is not fatal!
    scores = hs_open("r", score_file_name());

    for (i = 0; i < SCORE_FILE_ENTRIES; ++i)
        hs_list[i].reset();

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
                hs_list[i + 1] = hs_list[i];
                hs_list[i] = ne;
                i++;
            } else {
                // copy new entry to current position
                hs_list[i] = ne;
            }
        }
    }

    // special case: lowest score, with room
    if (!inserted && i < SCORE_FILE_ENTRIES)
    {
        newest_entry = i;
        inserted = true;
        // copy new entry
        hs_list[i] = ne;
        i++;
    }

    total_entries = i;

    // close so we can re-open for writing
    hs_close(scores,"r", score_file_name());

    // open highscore file (writing) -- NULL *is* fatal here.
    scores = hs_open("w", score_file_name());
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
    hs_close(scores, "w", score_file_name());
}

void logfile_new_entry( const scorefile_entry &ne )
{
    unwind_bool logfile_update(crawl_state.updating_scores, true);
    
    FILE *logfile;
    scorefile_entry le = ne;

    // open logfile (appending) -- NULL *is* fatal here.
    logfile = hs_open("a", log_file_name());
    if (logfile == NULL)
    {
        perror("Entry not added - failure opening logfile for appending.");
        return;
    }

    hs_write(logfile, le);

    // close logfile.
    hs_close(logfile, "a", log_file_name());
}

template <class t_printf>
static void hiscores_print_entry(const scorefile_entry &se,
                                 int index,
                                 int format,
                                 t_printf pf)
{
    char buf[INFO_SIZE];
    // print position (tracked implicitly by order score file)
    snprintf( buf, sizeof buf, "%3d.", index + 1 );

    pf("%s", buf);

    std::string entry;
    // format the entry
    if (format == SCORE_TERSE)
    {
        entry = hiscores_format_single( se );
        // truncate if we want short format
        if (entry.length() > 75)
            entry = entry.substr(0, 75);
    }
    else
    {
        entry = hiscores_format_single_long( se, (format == SCORE_VERBOSE) );
    }

    entry += EOL;
    pf("%s", entry.c_str());
}

// Writes all entries in the scorefile to stdout in human-readable form.
void hiscores_print_all(int display_count, int format)
{
    unwind_bool scorefile_display(crawl_state.updating_scores, true);
    
    FILE *scores = hs_open("r", score_file_name());
    if (scores == NULL)
    {
        // will only happen from command line
        puts( "No scores." );
        return;
    }

    for (int entry = 0; display_count <= 0 || entry < display_count; ++entry)
    {
        scorefile_entry se;
        if (!hs_read(scores, se))
            break;

        if (format == -1)
            printf("%s\n", se.raw_string().c_str());
        else
            hiscores_print_entry(se, entry, format, printf);
    }

    hs_close( scores, "r", score_file_name() );
}

// Displays high scores using curses. For output to the console, use
// hiscores_print_all.
void hiscores_print_list( int display_count, int format )
{
    unwind_bool scorefile_display(crawl_state.updating_scores, true);
    
    FILE *scores;
    int i, total_entries;

    if (display_count <= 0)
        return;

    // open highscore file (reading)
    scores = hs_open("r", score_file_name());
    if (scores == NULL)
        return;

    // read highscore file
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        if (hs_read( scores, hs_list[i] ) == false)
            break;
    }
    total_entries = i;

    // close off
    hs_close( scores, "r", score_file_name() );

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
        if (i == newest_entry)
            textcolor(YELLOW);

        hiscores_print_entry(hs_list[i], i, format, cprintf);

        if (i == newest_entry)
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

std::string hiscores_format_single(const scorefile_entry &se)
{
    return se.hiscore_line(scorefile_entry::DDV_ONELINE);
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

std::string hiscores_format_single_long( const scorefile_entry &se, 
                                         bool verbose )
{
    return se.hiscore_line( 
                verbose? 
                    scorefile_entry::DDV_VERBOSE 
                  : scorefile_entry::DDV_NORMAL );

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

FILE *hs_open( const char *mode, const std::string &scores )
{
    // allow reading from standard input
    if ( scores == "-" )
        return stdin;

    FILE *handle = fopen(scores.c_str(), mode);
#ifdef SHARED_FILES_CHMOD_PUBLIC
    chmod(scores.c_str(), SHARED_FILES_CHMOD_PUBLIC);
#endif

#ifdef USE_FILE_LOCKING
    int locktype = F_RDLCK;
    if (stricmp(mode, "r"))
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

void hs_close( FILE *handle, const char *mode, const std::string &scores )
{
    UNUSED( mode );

    if (handle == NULL || handle == stdin)
        return;

#ifdef USE_FILE_LOCKING
    unlock_file_handle( handle );
#endif

    // actually close
    fclose(handle);

#ifdef SHARED_FILES_CHMOD_PUBLIC
    if (stricmp(mode, "w") == 0)
        chmod(scores.c_str(), SHARED_FILES_CHMOD_PUBLIC);
#endif
}

bool hs_read( FILE *scores, scorefile_entry &dest )
{
    char inbuf[1300];
    if (!scores || feof(scores))
        return (false);

    memset(inbuf, 0, sizeof inbuf);
    dest.reset();

    if (!fgets(inbuf, sizeof inbuf, scores))
        return (false);

    return (dest.parse(inbuf));
}

static std::string hs_nextstring(const char *&inbuf, size_t destsize = 800)
{
    char *buf = new char[destsize];
    if (!buf)
        return ("");
    hs_nextstring(inbuf, buf, destsize);
    const std::string res = buf;
    delete [] buf;
    return (res);
}

static void hs_nextstring(const char *&inbuf, char *dest, size_t destsize)
{
    ASSERT(destsize > 0);

    char *p = dest;

    if (*inbuf == 0)
    {
        *p = 0;
        return;
    }

    // assume we're on a ':'
    if (*inbuf == ':')
        inbuf++;

    while (*inbuf && *inbuf != ':' && (p - dest) < (int) destsize - 1)
        *p++ = *inbuf++;

    // If we ran out of buffer, discard the rest of the field.
    while (*inbuf && *inbuf != ':')
        inbuf++;

    *p = 0;
}

static int hs_nextint(const char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num, sizeof num);

    return (num[0] == 0 ? 0 : atoi(num));
}

static long hs_nextlong(const char *&inbuf)
{
    char num[20];
    hs_nextstring(inbuf, num, sizeof num);

    return (num[0] == 0 ? 0 : atol(num));
}

static int val_char( char digit )
{
    return (digit - '0');
}

static time_t hs_nextdate(const char *&inbuf)
{
    char       buff[20];
    hs_nextstring(inbuf, buff, sizeof buff);

    return parse_time(buff);
}

static time_t parse_time(const std::string &st)
{
    struct tm  date;

    if (st.length() < 15)
        return (static_cast<time_t>(0));

    date.tm_year = val_char( st[0] ) * 1000 + val_char( st[1] ) * 100
                    + val_char( st[2] ) * 10 + val_char( st[3] ) - 1900;

    date.tm_mon   = val_char( st[4] ) * 10 + val_char( st[5] );
    date.tm_mday  = val_char( st[6] ) * 10 + val_char( st[7] );
    date.tm_hour  = val_char( st[8] ) * 10 + val_char( st[9] );
    date.tm_min   = val_char( st[10] ) * 10 + val_char( st[11] );
    date.tm_sec   = val_char( st[12] ) * 10 + val_char( st[13] );
    date.tm_isdst = (st[14] == 'D');

    return (mktime( &date ));
}

static void hs_write( FILE *scores, scorefile_entry &se )
{
    fprintf(scores, "%s\n", se.raw_string().c_str());

    /*
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
    */
}

static const char *kill_method_names[] =
{
    "mon", "pois", "cloud", "beam", "deaths_door", "lava", "water",
    "stupidity", "weakness", "clumsiness", "trap", "leaving", "winning",
    "quitting", "draining", "starvation", "freezing", "burning", "wild_magic",
    "xom", "statue", "rotting", "targetting", "spore", "tso_smiting",
    "petrification", "unknown", "something", "falling_down_stairs", "acid",
    "curare", "melting", "bleeding",
};

const char *kill_method_name(kill_method_type kmt)
{
    ASSERT(NUM_KILLBY ==
           (int) sizeof(kill_method_names) / sizeof(*kill_method_names));

    if (kmt == NUM_KILLBY)
        return ("");

    return kill_method_names[kmt];
}

kill_method_type str_to_kill_method(const std::string &s)
{
    ASSERT(NUM_KILLBY ==
           (int) sizeof(kill_method_names) / sizeof(*kill_method_names));
    
    for (int i = 0; i < NUM_KILLBY; ++i)
    {
        if (s == kill_method_names[i])
            return static_cast<kill_method_type>(i);
    }

    return (NUM_KILLBY);
}

//////////////////////////////////////////////////////////////////////////
// scorefile_entry

scorefile_entry::scorefile_entry(int dam, int dsource, int dtype,
                                 const char *aux, bool death_cause_only)
{
    reset();

    init_death_cause(dam, dsource, dtype, aux);
    if (!death_cause_only)
        init();
}

scorefile_entry::scorefile_entry()
{
    // Completely uninitialized, caveat user.
    reset();
}

scorefile_entry::scorefile_entry(const scorefile_entry &se)
{
    init_from(se);
}

scorefile_entry &scorefile_entry::operator = (const scorefile_entry &se)
{
    init_from(se);
    return (*this);
}

void scorefile_entry::init_from(const scorefile_entry &se)
{
    version = se.version;
    release = se.release;
    points = se.points;
    name = se.name;
    uid = se.uid;
    race = se.race;
    cls = se.cls;
    race_class_name = se.race_class_name;
    lvl = se.lvl;
    best_skill = se.best_skill;
    best_skill_lvl = se.best_skill_lvl;
    death_type = se.death_type;
    death_source = se.death_source;
    mon_num = se.mon_num;
    death_source_name = se.death_source_name;
    auxkilldata = se.auxkilldata;
    dlvl = se.dlvl;
    level_type = se.level_type;
    branch = se.branch;
    final_hp = se.final_hp;
    final_max_hp = se.final_max_hp;
    final_max_max_hp = se.final_max_max_hp;
    damage = se.damage;
    str = se.str;
    intel = se.intel;
    dex = se.dex;
    god = se.god;
    piety = se.piety;
    penance = se.penance;
    wiz_mode = se.wiz_mode;
    birth_time = se.birth_time;
    death_time = se.death_time;
    real_time = se.real_time;
    num_turns = se.num_turns;
    num_diff_runes = se.num_diff_runes;
    num_runes = se.num_runes;
}

bool scorefile_entry::parse(const std::string &line)
{
    // Scorefile formats down the ages:
    //
    // 1) old-style lines which were 80 character blocks
    // 2) 4.0 pr1 through pr7 versions which were newline terminated
    // 3) 4.0 pr8 and onwards which are colon-separated fields (and
    //    start with a colon), and may exceed 80 characters!
    // 4) 0.2 and onwards, which are xlogfile format - no leading
    //    colon, fields separated by colons, each field specified as
    //    key=value. Colons are not allowed in key names, must be escaped to
    //    | in values. Literal | must be escaped as || in values.
    //
    // 0.2 only reads entries of type (3) and (4), and only writes entries of
    // type (4).

    // Leading colon implies 4.0 style line:
    if (line[0] == ':')
        return (parse_obsolete_scoreline(line));
    else
        return (parse_scoreline(line));
}

// xlogfile escape: s/\\/\\\\/g, s/|/\\|/g, s/:/|/g, 
std::string scorefile_entry::xlog_escape(const std::string &s) const
{
    return
        replace_all_of(
            replace_all_of(
                replace_all_of(s, "\\", "\\\\"),
                "|", "\\|" ),
            ":", "|" );
}

// xlogfile unescape: s/\\(.)/$1/g, s/|/:/g
std::string scorefile_entry::xlog_unescape(const std::string &s) const
{
    std::string unesc = s;
    bool escaped = false;
    for (int i = 0, size = unesc.size(); i < size; ++i)
    {
        const char c = unesc[i];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        
        if (c == '|')
            unesc[i] = ':';
        else if (c == '\\')
        {
            escaped = true;
            unesc.erase(i--, 1);
            size--;
        }
    }
    return (unesc);
}

std::string scorefile_entry::raw_string() const
{
    set_score_fields();
    
    if (!fields.get())
        return ("");
    
    std::string line;
    for (int i = 0, size = fields->size(); i < size; ++i)
    {
        const std::pair<std::string, std::string> &f = (*fields)[i];

        // Don't write empty fields.
        if (f.second.empty())
            continue;
        
        if (!line.empty())
            line += ":";

        line += f.first;
        line += "=";
        line += xlog_escape(f.second);
    }

    return (line);
}

bool scorefile_entry::parse_scoreline(const std::string &line)
{
    std::vector<std::string> rawfields = split_string(":", line);
    fields.reset(new hs_fields);
    for (int i = 0, size = rawfields.size(); i < size; ++i)
    {
        const std::string field = rawfields[i];
        std::string::size_type st = field.find('=');
        if (st == std::string::npos)
            continue;

        fields->push_back(
            std::pair<std::string, std::string>(
                field.substr(0, st),
                xlog_unescape(field.substr(st + 1)) ) );
    }

    init_with_fields();
    
    return (true);
}

void scorefile_entry::add_field(const std::string &key,
                                const char *format,
                                ...) const
{
    char buf[400];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);

    fields->push_back(
        std::pair<std::string, std::string>( key, buf ) );
}

void scorefile_entry::add_auxkill_field() const
{
    const char *what =
        death_type == KILLED_BY_MONSTER? "kweap" :
        death_type == KILLED_BY_BEAM?    "kbeam" :
        "kaux";
    
    add_field(what, "%s", auxkilldata.c_str());
}

void scorefile_entry::read_auxkill_field()
{
    const char *what =
        death_type == KILLED_BY_MONSTER? "kweap" :
        death_type == KILLED_BY_BEAM?    "kbeam" :
        "kaux";

    auxkilldata = str_field(what);
}

std::string scorefile_entry::str_field(const std::string &s) const
{
    hs_map::const_iterator i = fieldmap->find(s);
    if (i == fieldmap->end())
        return ("");

    return i->second;
}

int scorefile_entry::int_field(const std::string &s) const
{
    std::string field = str_field(s);
    return atoi(field.c_str());
}

long scorefile_entry::long_field(const std::string &s) const
{
    std::string field = str_field(s);
    return atol(field.c_str());
}

void scorefile_entry::map_fields()
{
    fieldmap.reset(new hs_map);
    for (int i = 0, size = fields->size(); i < size; ++i)
    {
        const std::pair<std::string, std::string> f = (*fields)[i];
        
        (*fieldmap)[f.first] = f.second;
    }
}

static const char *short_branch_name(int branch)
{
    if (branch >= 0 && branch < NUM_BRANCHES)
        return branches[branch].abbrevname;
    return ("");
}

static int str_to_branch(const std::string &branch)
{
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].abbrevname && branches[i].abbrevname == branch)
            return (i);
    }
    return (BRANCH_MAIN_DUNGEON);
}

static const char *level_type_names[] =
{
    "D", "Lab", "Abyss", "Pan"
};

static const char *level_area_type_name(int level_type)
{
    if (level_type >= 0 && level_type < NUM_LEVEL_AREA_TYPES)
        return level_type_names[level_type];
    return ("");
}

static int str_to_level_area_type(const std::string &s)
{
    for (int i = 0; i < NUM_LEVEL_AREA_TYPES; ++i)
        if (s == level_type_names[i])
            return (i);
    return (LEVEL_DUNGEON);
}

static int str_to_god(const std::string &god)
{
    if (god.empty())
        return GOD_NO_GOD;
    
    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
    {
        if (god_name(i) == god)
            return (i);
    }
    return (GOD_NO_GOD);
}

void scorefile_entry::init_with_fields()
{
    map_fields();
    points = long_field("sc");
    name   = str_field("name");
    uid    = int_field("uid");

    race   = str_to_species(str_field("race"));
    cls    = get_class_index_by_name(str_field("cls").c_str());

    lvl    = int_field("xl");

    best_skill = str_to_skill(str_field("sk"));
    best_skill_lvl = int_field("sklev");
    death_type = str_to_kill_method(str_field("ktyp"));
    death_source_name = str_field("killer");

    read_auxkill_field();

    branch = str_to_branch(str_field("br"));
    dlvl   = int_field("lvl");
    level_type = str_to_level_area_type(str_field("ltyp"));

    final_hp = int_field("hp");
    final_max_hp = int_field("mhp");
    final_max_max_hp = int_field("mmhp");
    damage = int_field("dam");
    str = int_field("str");
    intel = int_field("int");
    dex = int_field("dex");

    god = str_to_god(str_field("god"));
    piety = int_field("piety");
    penance = int_field("pen");
    wiz_mode = int_field("wiz");
    birth_time = parse_time(str_field("start"));
    death_time = parse_time(str_field("end"));
    real_time  = long_field("dur");
    num_turns  = long_field("turn");
    num_diff_runes = int_field("urune");
    num_runes = int_field("nrune");
}

void scorefile_entry::set_score_fields() const
{
    fields.reset(new hs_fields());

    if (!fields.get())
        return;

    add_field("v", VER_NUM);
    add_field("lv", SCORE_VERSION);
    add_field("sc", "%ld", points);
    add_field("name", "%s", name.c_str());
    add_field("uid", "%d", uid);
    add_field("race", "%s", species_name(race, lvl));
    add_field("cls", "%s", get_class_name(cls));
    add_field("xl", "%d", lvl);
    add_field("sk", "%s", skill_name(best_skill));
    add_field("sklev", "%d", best_skill_lvl);
    add_field("title", "%s", skill_title( best_skill, best_skill_lvl, 
                                                  race, str, dex, god ) );
    add_field("ktyp", ::kill_method_name(kill_method_type(death_type)));
    add_field("killer", death_source_desc());

    add_auxkill_field();

    add_field("place", "%s",
              place_name(get_packed_place(branch, dlvl, level_type),
                         false, true).c_str());
    add_field("br", "%s", short_branch_name(branch));
    add_field("lvl", "%d", dlvl);
    add_field("ltyp", "%s", level_area_type_name(level_type));

    add_field("hp", "%d", final_hp);
    add_field("mhp", "%d", final_max_hp);
    add_field("mmhp", "%d", final_max_max_hp);
    add_field("dam", "%d", damage);
    add_field("str", "%d", str);
    add_field("int", "%d", intel);
    add_field("dex", "%d", dex);

    // Don't write No God to save some space.
    if (god != -1)
        add_field("god", "%s", god == GOD_NO_GOD? "" : god_name(god));
    if (piety > 0)
        add_field("piety", "%d", piety);
    if (penance > 0)
        add_field("pen", "%d", penance);
    if (wiz_mode)
        add_field("wiz", "%d", wiz_mode);
    
    add_field("start", "%s", make_date_string(birth_time).c_str());
    add_field("end", "%s", make_date_string(death_time).c_str());
    add_field("dur", "%ld", real_time);
    add_field("turn", "%ld", num_turns);

    if (num_diff_runes)
        add_field("urune", "%d", num_diff_runes);

    if (num_runes)
        add_field("nrune", "%d", num_runes);

#ifdef DGL_EXTENDED_LOGFILES
    const std::string short_msg = short_kill_message();
    add_field("tmsg", "%s", short_msg.c_str());
    const std::string long_msg = long_kill_message();
    if (long_msg != short_msg)
        add_field("vmsg", "%s", long_msg.c_str());
#endif
}

std::string scorefile_entry::make_oneline(const std::string &ml) const
{
    std::vector<std::string> lines = split_string(EOL, ml);
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        std::string &s = lines[i];
        if (s.find("...") == 0)
        {
            s = s.substr(3);
            trim_string(s);
        }
    }
    return comma_separated_line(lines.begin(), lines.end(), " ", " ");
}

std::string scorefile_entry::long_kill_message() const
{
    std::string msg = death_description(DDV_LOGVERBOSE);
    msg = make_oneline(msg);
    msg[0] = tolower(msg[0]);
    trim_string(msg);
    return (msg);
}

std::string scorefile_entry::short_kill_message() const
{
    std::string msg = death_description(DDV_ONELINE);
    msg = make_oneline(msg);
    msg[0] = tolower(msg[0]);
    trim_string(msg);
    return (msg);
}

// Maps a 0.1.x branch id to a 0.2 branch id. Ugh. Fortunately we need this
// only to read old logfiles/scorefiles.
int scorefile_entry::kludge_branch(int branch_01) const
{
    static int branch_map[] = {
        BRANCH_MAIN_DUNGEON, BRANCH_DIS, BRANCH_GEHENNA,
        BRANCH_VESTIBULE_OF_HELL, BRANCH_COCYTUS, BRANCH_TARTARUS,
        BRANCH_INFERNO, BRANCH_THE_PIT, BRANCH_MAIN_DUNGEON,
        BRANCH_MAIN_DUNGEON, BRANCH_ORCISH_MINES, BRANCH_HIVE,
        BRANCH_LAIR, BRANCH_SLIME_PITS, BRANCH_VAULTS, BRANCH_CRYPT,
        BRANCH_HALL_OF_BLADES, BRANCH_HALL_OF_ZOT, BRANCH_ECUMENICAL_TEMPLE,
        BRANCH_SNAKE_PIT, BRANCH_ELVEN_HALLS, BRANCH_TOMB, BRANCH_SWAMP,
        BRANCH_CAVERNS
    };

    if (branch_01 < 0
        || branch_01 > (int) (sizeof(branch_map) / sizeof(*branch_map)))
    {
        return (BRANCH_MAIN_DUNGEON);
    }
    return branch_map[branch_01];
}

// [ds] This is the 4.0 b26 logfile parser. Old-style logs are now deprecated;
// support for reading them may be discontinued in the next version.
bool scorefile_entry::parse_obsolete_scoreline(const std::string &line)
{
    const char *inbuf = line.c_str();
    
    version = hs_nextint(inbuf);
    release = hs_nextint(inbuf);

    // this would be a good point to check for version numbers and branch
    // appropriately

    // acceptable versions are 0 (converted from old hiscore format) and 4
    if (version != 4 || release < 2)
        return (false);

    points = hs_nextlong(inbuf);

    name = hs_nextstring(inbuf);

    uid = hs_nextlong(inbuf);
    race = hs_nextint(inbuf);
    cls = hs_nextint(inbuf);

    race_class_name = hs_nextstring(inbuf, 6);

    lvl = hs_nextint(inbuf);
    best_skill = hs_nextint(inbuf);
    best_skill_lvl = hs_nextint(inbuf);
    death_type = hs_nextint(inbuf);
    death_source = hs_nextint(inbuf);
    mon_num = hs_nextint(inbuf);

    death_source_name = hs_nextstring(inbuf);

    // To try and keep the scorefile backwards compatible,
    // we'll branch on version > 4.0 to read the auxkilldata
    // text field.  
    if (version == 4 && release >= 1)
        auxkilldata = hs_nextstring( inbuf, ITEMNAME_SIZE );
    else
        auxkilldata[0] = 0;

    dlvl = hs_nextint(inbuf);
    level_type = hs_nextint(inbuf);
    branch = kludge_branch( hs_nextint(inbuf) );

    final_hp = hs_nextint(inbuf);
    final_max_hp = hs_nextint(inbuf);
    final_max_max_hp = hs_nextint(inbuf);
    damage = hs_nextint(inbuf);
    str = hs_nextint(inbuf);
    intel = hs_nextint(inbuf);
    dex = hs_nextint(inbuf);
    god = hs_nextint(inbuf);
    piety = hs_nextint(inbuf);
    penance = hs_nextint(inbuf);

    wiz_mode = hs_nextint(inbuf);

    birth_time = hs_nextdate(inbuf);
    death_time = hs_nextdate(inbuf);

    real_time = hs_nextint(inbuf);
    num_turns = hs_nextint(inbuf);

    num_diff_runes = hs_nextint(inbuf);
    num_runes = hs_nextint(inbuf);

    return (true);
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
        auxkilldata.clear();
    else
        auxkilldata = aux;

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
                    auxkilldata = it_name( monster->inv[MSLOT_WEAPON], 
                                           DESC_NOCAP_A );
            }

            death_source_name =
                monam( monster->number, monster->type, true, DESC_NOCAP_A, 
                       monster->inv[MSLOT_WEAPON] );
        }
    }
    else
    {
        death_source = death_source;
        mon_num = 0;
        death_source_name[0] = 0;
    }
}

void scorefile_entry::reset()
{
    // simple init
    version = 0;
    release = 0;
    points = -1;
    name[0] = 0;
    uid = 0;
    race = 0; 
    cls = 0;
    lvl = 0;
    race_class_name[0] = 0;
    best_skill = 0;
    best_skill_lvl = 0;
    death_type = KILLED_BY_SOMETHING;
    death_source = 0;
    mon_num = 0;
    death_source_name[0] = 0;
    auxkilldata[0] = 0;
    dlvl = 0;
    level_type = 0;
    branch = 0;
    final_hp = -1;
    final_max_hp = -1;
    final_max_max_hp = -1;
    str = -1;
    intel = -1;
    dex = -1;
    damage = -1;
    god = -1;
    piety = -1;
    penance = -1;
    wiz_mode = 0;
    birth_time = 0;
    death_time = 0;
    real_time = -1;
    num_turns = -1;
    num_diff_runes = 0;
    num_runes = 0;
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

    name = you.your_name;

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
    dlvl = player_branch_depth();
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
                if (pw_entry)
                {
                    strncpy( username, pw_entry->pw_name, sizeof(username) );
                    strncat( username, "'s", sizeof(username) );
                    username[0] = toupper( username[0] );
                }
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

    return (!death_source_name.empty()? 
            death_source_name.c_str()
            : monam( mon_num, death_source, true, DESC_NOCAP_A ) );
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

std::string scorefile_entry::terse_missile_name() const
{
    const std::string pre_post[][2] = {
        { "Shot with a", " by " },
        { "Hit by a",    " thrown by " }
    };
    const std::string &aux = auxkilldata;
    std::string missile;
    
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
    return (missile);
}

std::string scorefile_entry::terse_missile_cause() const
{
    std::string cause;
    const std::string &aux = auxkilldata;

    std::string monster_prefix = " by ";
    // We're looking for Shot with a%s %s by %s/ Hit by a%s %s thrown by %s
    std::string::size_type by = aux.rfind(monster_prefix);
    if (by == std::string::npos)
        return ("???");

    std::string mcause = aux.substr(by + monster_prefix.length());
    mcause = strip_article_a(mcause);

    std::string missile = terse_missile_name();

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
    std::string trap = !auxkilldata.empty()? auxkilldata + " trap"
                                   : "trap";
    if (trap.find("n ") == 0)
        trap = trap.substr(2);
    trim_string(trap);

    return (trap);
}

std::string scorefile_entry::single_cdesc() const
{
    std::string char_desc;

    if (race_class_name.empty())
    {
        char_desc =
            make_stringf("%s%s", 
                         get_species_abbrev( race ),
                         get_class_abbrev( cls ) );
    }
    else
    {
        char_desc = race_class_name;
    }

    std::string scname = name;
    if (scname.length() > 10)
        scname = scname.substr(0, 10);

    return make_stringf(
             "%8ld %-10s %s-%02d%s", points, scname.c_str(),
             char_desc.c_str(), lvl, (wiz_mode == 1) ? "W" : "" );
}

std::string
scorefile_entry::character_description(death_desc_verbosity verbosity) const
{
    bool single  = verbosity == DDV_TERSE || verbosity == DDV_ONELINE;

    if (single)
        return single_cdesc();

    bool verbose = verbosity == DDV_VERBOSE;
    char  scratch[INFO_SIZE];
    char  buf[HIGHSCORE_SIZE];

    std::string desc;
    // Please excuse the following bit of mess in the name of flavour ;)
    if (verbose)
    {
        strncpy( scratch, skill_title( best_skill, best_skill_lvl, 
                                       race, str, dex, god ), 
                 INFO_SIZE );

        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s (level %d",
                  points, name.c_str(), scratch, lvl );

        desc = buf;
    }
    else 
    {
        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s %s (level %d",
                  points, name.c_str(), species_name(race, lvl), 
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
        snprintf( scratch, sizeof scratch, " (%s)",
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

    const bool terse   = verbosity == DDV_TERSE;
    const bool semiverbose = verbosity == DDV_LOGVERBOSE;
    const bool verbose = verbosity == DDV_VERBOSE || semiverbose;
    const bool oneline = verbosity == DDV_ONELINE;

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
                      auxkilldata.c_str() );
            desc += scratch;
        }
        needs_damage = true;
        break;

    case KILLED_BY_BEAM:
        if (oneline || semiverbose)
        {
            // keeping this short to leave room for the deep elf spellcasters:
            snprintf( scratch, sizeof(scratch), "%s by ", 
                      range_type_verb( auxkilldata.c_str() ) );
            desc += scratch;
            desc += death_source_desc();

            if (semiverbose)
            {
                std::string beam = terse_missile_name();
                if (beam.empty())
                    beam = terse_beam_cause();
                if (!beam.empty())
                    desc += make_stringf(" (%s)", beam.c_str());
            }
        }
        else if (isupper( auxkilldata[0] ))  // already made (ie shot arrows)
        {
            // If terse we have to parse the information from the string.
            // Darn it to heck.
            desc += terse? terse_missile_cause() : auxkilldata;
            needs_damage = true;
        }
        else if (verbose && auxkilldata.find("by ") == 0)
        {
            // "by" is used for priest attacks where the effect is indirect
            // in verbose format we have another line for the monster
            needs_called_by_monster_line = true;
            snprintf( scratch, sizeof(scratch), "Killed %s",
                      auxkilldata.c_str() );
            desc += scratch;
        }
        else
        {
            // Note: This is also used for the "by" cases in non-verbose
            //       mode since listing the monster is more imporatant.
            if (semiverbose)
                desc += "Killed by ";
            else if (!terse)
                desc += "Killed from afar by ";

            desc += death_source_desc();

            if (!auxkilldata.empty())
                needs_beam_cause_line = true;

            needs_damage = true;
        }
        break;

/* deprecated */
/*
    case KILLED_BY_DEATHS_DOOR:
        desc += terse? "Death's door" : "Knocked on Death's door";
        break;
*/

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
            snprintf( scratch, sizeof(scratch),
                      "Killed by triggering a%s trap", 
                      auxkilldata.c_str() );
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
        if (auxkilldata.empty())
            desc += terse? "wild magic" : "Killed by wild magic";
        else
        {
            if (terse)
                desc += terse_wild_magic();
            else
            {
                // A lot of sources for this case... some have "by" already.
                snprintf( scratch, sizeof(scratch), "Killed %s%s", 
                          (auxkilldata.find("by ") != 0) ? "by " : "",
                          auxkilldata.c_str() );
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

    /* case 26 */

    case KILLED_BY_SOMETHING:
        if (!auxkilldata.empty())
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
    
    case KILLED_BY_CURARE:
        desc += terse? "asphyx" : "Asphyxiated";
        break;

    case KILLED_BY_MELTING:
        desc += terse? "melted" : " melted into a puddle";
        break;

    case KILLED_BY_BLEEDING:
        desc += terse? "bleeding" : " bled to death";
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
        if (death_type == KILLED_BY_MONSTER && !auxkilldata.empty())
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

        if (!semiverbose && needs_damage && damage > 0)
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

            if (!semiverbose && num_diff_runes > 1)
            {
                snprintf( scratch, INFO_SIZE, " (of %d types)",
                          num_diff_runes );
                desc += scratch;
            }

            if (!semiverbose
                && death_time > 0 
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
                if (!semiverbose)
                {
                    snprintf(scratch, INFO_SIZE, "... wielding %s",
                             auxkilldata.c_str());
                    desc += scratch;
                    needs_damage = true;
                }
            }
            else if (needs_beam_cause_line)
            {
                if (!semiverbose)
                {
                    desc += (is_vowel( auxkilldata[0] )) ? "... with an " 
                        : "... with a ";
                    desc += auxkilldata;
                    needs_damage = true;
                }
            }
            else if (needs_called_by_monster_line)
            {
                snprintf( scratch, sizeof(scratch), "... invoked by %s",
                          death_source_name.c_str() );
                desc += scratch;
                needs_damage = true;
            }

            if (!semiverbose)
            {
                if (needs_damage && !done_damage && damage > 0) 
                    desc += " " + damage_string();

                if (needs_damage)
                    desc += hiscore_newline_string();
            }
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

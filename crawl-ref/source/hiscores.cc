/*
 *  File: highscore.cc
 *  Summary: deal with reading and writing of highscore file
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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
#include <algorithm>
#include <memory>
#if !_MSC_VER
#include <unistd.h>
#endif

#include "AppHdr.h"
REVISION("$Rev$");

#include "branch.h"
#include "files.h"
#include "hiscores.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "mon-util.h"
#include "newgame.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "state.h"
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
static std::auto_ptr<scorefile_entry> hs_list[SCORE_FILE_ENTRIES];

// hackish: scorefile position of newest entry.  Will be highlit during
// highscore printing (always -1 when run from command line).
static int newest_entry = -1;

static FILE *_hs_open(const char *mode, const std::string &filename);
static void  _hs_close(FILE *handle, const char *mode,
                       const std::string &filename);
static bool  _hs_read(FILE *scores, scorefile_entry &dest);
static void  _hs_write(FILE *scores, scorefile_entry &entry);
static time_t _parse_time(const std::string &st);

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

    // open highscore file (reading) -- NULL is fatal!
    //
    // Opening as a+ instead of r+ to force an exclusive lock (see
    // hs_open) and to create the file if it's not there already.
    scores = _hs_open("a+", score_file_name());
    if (scores == NULL)
        end(1, true, "failed to open score file for writing");

    // we're at the end of the file, seek back to beginning.
    fseek(scores, 0, SEEK_SET);

    // read highscore file, inserting new entry at appropriate point,
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        hs_list[i].reset(new scorefile_entry);
        if (_hs_read(scores, *hs_list[i]) == false)
            break;

        // compare points..
        if (!inserted && ne.points >= hs_list[i]->points)
        {
            newest_entry = i;           // for later printing
            inserted = true;
            // copy read entry to i+1th position
            // Fixed a nasty overflow bug here -- Sharp
            if (i+1 < SCORE_FILE_ENTRIES)
            {
                hs_list[i + 1] = hs_list[i];
                hs_list[i].reset(new scorefile_entry(ne));
                i++;
            }
            else
                *hs_list[i] = ne; // copy new entry to current position
        }
    }

    // special case: lowest score, with room
    if (!inserted && i < SCORE_FILE_ENTRIES)
    {
        newest_entry = i;
        inserted = true;
        // copy new entry
        hs_list[i].reset(new scorefile_entry(ne));
        i++;
    }

    // If we've still not inserted it, it's not a highscore.
    if (!inserted)
    {
        _hs_close(scores, "a+", score_file_name());
        return;
    }

    total_entries = i;

    // The old code closed and reopened the score file, leading to a
    // race condition where one Crawl process could overwrite the
    // other's highscore. Now we truncate and rewrite the file without
    // closing it.
    if (ftruncate(fileno(scores), 0))
        end(1, true, "unable to truncate scorefile");

    rewind(scores);

    // write scorefile entries.
    for (i = 0; i < total_entries; i++)
    {
        _hs_write(scores, *hs_list[i]);
        hs_list[i].reset(NULL);
    }

    // close scorefile.
    _hs_close(scores, "a+", score_file_name());
}

void logfile_new_entry( const scorefile_entry &ne )
{
    unwind_bool logfile_update(crawl_state.updating_scores, true);

    FILE *logfile;
    scorefile_entry le = ne;

    // open logfile (appending) -- NULL *is* fatal here.
    logfile = _hs_open("a", log_file_name());
    if (logfile == NULL)
    {
        perror("Entry not added - failure opening logfile for appending.");
        return;
    }

    _hs_write(logfile, le);

    // close logfile.
    _hs_close(logfile, "a", log_file_name());
}

template <class t_printf>
static void _hiscores_print_entry(const scorefile_entry &se,
                                  int index, int format, t_printf pf)
{
    char buf[INFO_SIZE];
    // print position (tracked implicitly by order score file)
    snprintf( buf, sizeof buf, "%3d.", index + 1 );

    pf("%s", buf);

    std::string entry;
    // format the entry
    if (format == SCORE_TERSE)
        entry = hiscores_format_single( se );
    else
        entry = hiscores_format_single_long( se, (format == SCORE_VERBOSE) );

    entry += EOL;
    pf("%s", entry.c_str());
}

// Writes all entries in the scorefile to stdout in human-readable form.
void hiscores_print_all(int display_count, int format)
{
    unwind_bool scorefile_display(crawl_state.updating_scores, true);

    FILE *scores = _hs_open("r", score_file_name());
    if (scores == NULL)
    {
        // will only happen from command line
        puts( "No scores." );
        return;
    }

    for (int entry = 0; display_count <= 0 || entry < display_count; ++entry)
    {
        scorefile_entry se;
        if (!_hs_read(scores, se))
            break;

        if (format == -1)
            printf("%s\n", se.raw_string().c_str());
        else
            _hiscores_print_entry(se, entry, format, printf);
    }

    _hs_close( scores, "r", score_file_name() );
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
    scores = _hs_open("r", score_file_name());
    if (scores == NULL)
        return;

    // read highscore file
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        hs_list[i].reset(new scorefile_entry);
        if (_hs_read( scores, *hs_list[i] ) == false)
            break;
    }
    total_entries = i;

    // close off
    _hs_close( scores, "r", score_file_name() );

    textcolor(LIGHTGREY);

    int start = (newest_entry > 10) ? newest_entry - 10 : 0;

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

        _hiscores_print_entry(*hs_list[i], i, format, cprintf);

        if (i == newest_entry)
            textcolor(LIGHTGREY);
    }
}

// Trying to supply an appropriate verb for the attack type. -- bwr
static const char *const _range_type_verb( const char *const aux )
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

static bool _hiscore_same_day( time_t t1, time_t t2 )
{
    struct tm *d1  = TIME_FN( &t1 );
    const int year = d1->tm_year;
    const int mon  = d1->tm_mon;
    const int day  = d1->tm_mday;

    struct tm *d2  = TIME_FN( &t2 );

    return (d2->tm_mday == day && d2->tm_mon == mon && d2->tm_year == year);
}

static void _hiscore_date_string( time_t time, char buff[INFO_SIZE] )
{
    struct tm *date = TIME_FN( &time );

    const char *mons[12] = { "Jan", "Feb", "Mar", "Apr", "May", "June",
                             "July", "Aug", "Sept", "Oct", "Nov", "Dec" };

    snprintf( buff, INFO_SIZE, "%s %d, %d", mons[date->tm_mon],
              date->tm_mday, date->tm_year + 1900 );
}

static std::string _hiscore_newline_string()
{
    return (EOL "             ");
}

std::string hiscores_format_single_long( const scorefile_entry &se,
                                         bool verbose )
{
    return se.hiscore_line( verbose ? scorefile_entry::DDV_VERBOSE
                                    : scorefile_entry::DDV_NORMAL );
}

// --------------------------------------------------------------------------
// BEGIN private functions
// --------------------------------------------------------------------------

FILE *_hs_open( const char *mode, const std::string &scores )
{
    // allow reading from standard input
    if ( scores == "-" )
        return stdin;

    return lk_open( mode, scores );
}

void _hs_close( FILE *handle, const char *mode, const std::string &scores )
{
    lk_close(handle, mode, scores);
}

bool _hs_read( FILE *scores, scorefile_entry &dest )
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

static int _val_char( char digit )
{
    return (digit - '0');
}

static time_t _parse_time(const std::string &st)
{
    struct tm  date;

    if (st.length() < 15)
        return (static_cast<time_t>(0));

    date.tm_year  =   _val_char( st[0] ) * 1000 + _val_char( st[1] ) * 100
                    + _val_char( st[2] ) *   10 + _val_char( st[3] ) - 1900;

    date.tm_mon   = _val_char( st[4] )  * 10 + _val_char( st[5] );
    date.tm_mday  = _val_char( st[6] )  * 10 + _val_char( st[7] );
    date.tm_hour  = _val_char( st[8] )  * 10 + _val_char( st[9] );
    date.tm_min   = _val_char( st[10] ) * 10 + _val_char( st[11] );
    date.tm_sec   = _val_char( st[12] ) * 10 + _val_char( st[13] );
    date.tm_isdst = (st[14] == 'D');

    return (mktime( &date ));
}

static void _hs_write( FILE *scores, scorefile_entry &se )
{
    fprintf(scores, "%s\n", se.raw_string().c_str());
}

static const char *kill_method_names[] =
{
    "mon", "pois", "cloud", "beam", "deaths_door", "lava", "water",
    "stupidity", "weakness", "clumsiness", "trap", "leaving", "winning",
    "quitting", "draining", "starvation", "freezing", "burning",
    "wild_magic", "xom", "statue", "rotting", "targeting", "spore",
    "tso_smiting", "petrification", "unknown", "something",
    "falling_down_stairs", "acid", "curare", "melting", "bleeding",
    "beogh_smiting", "divine_wrath", "bounce", "reflect"
};

const char *kill_method_name(kill_method_type kmt)
{
    ASSERT(NUM_KILLBY == ARRAYSZ(kill_method_names));

    if (kmt == NUM_KILLBY)
        return ("");

    return kill_method_names[kmt];
}

kill_method_type str_to_kill_method(const std::string &s)
{
    ASSERT(NUM_KILLBY == ARRAYSZ(kill_method_names));

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
    version           = se.version;
    points            = se.points;
    name              = se.name;
    uid               = se.uid;
    race              = se.race;
    cls               = se.cls;
    race_class_name   = se.race_class_name;
    lvl               = se.lvl;
    best_skill        = se.best_skill;
    best_skill_lvl    = se.best_skill_lvl;
    death_type        = se.death_type;
    death_source      = se.death_source;
    mon_num           = se.mon_num;
    death_source_name = se.death_source_name;
    auxkilldata       = se.auxkilldata;
    dlvl              = se.dlvl;
    level_type        = se.level_type;
    branch            = se.branch;
    final_hp          = se.final_hp;
    final_max_hp      = se.final_max_hp;
    final_max_max_hp  = se.final_max_max_hp;
    damage            = se.damage;
    str               = se.str;
    intel             = se.intel;
    dex               = se.dex;
    god               = se.god;
    piety             = se.piety;
    penance           = se.penance;
    wiz_mode          = se.wiz_mode;
    birth_time        = se.birth_time;
    death_time        = se.death_time;
    real_time         = se.real_time;
    num_turns         = se.num_turns;
    num_diff_runes    = se.num_diff_runes;
    num_runes         = se.num_runes;
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
    //    :: in values.
    //
    // 0.3 only reads and writes entries of type (4).

    // Leading colon implies 4.0 style line:
    if (line[0] == ':')
    {
        end(1, false, "Cannot read 4.0-style scorefiles");
        // Keep gcc happy:
        return (false);
    }
    else
        return (parse_scoreline(line));
}

std::string scorefile_entry::raw_string() const
{
    set_score_fields();

    if (!fields.get())
        return ("");

    return fields->xlog_line();
}

bool scorefile_entry::parse_scoreline(const std::string &line)
{
    fields.reset(new xlog_fields(line));
    init_with_fields();

    return (true);
}

static const char* _short_branch_name(int branch)
{
    if (branch >= 0 && branch < NUM_BRANCHES)
        return branches[branch].abbrevname;
    return ("");
}

void scorefile_entry::init_with_fields()
{
    version = fields->str_field("v");
    points  = fields->long_field("sc");

    name    = fields->str_field("name");
    uid     = fields->int_field("uid");
    race    = str_to_species(fields->str_field("race"));
    cls     = get_class_by_name(fields->str_field("cls").c_str());
    lvl     = fields->int_field("xl");

    best_skill     = str_to_skill(fields->str_field("sk"));
    best_skill_lvl = fields->int_field("sklev");

    death_type        = str_to_kill_method(fields->str_field("ktyp"));
    death_source_name = fields->str_field("killer");
    auxkilldata       = fields->str_field("kaux");

    branch     = str_to_branch(fields->str_field("br"), BRANCH_MAIN_DUNGEON);
    dlvl       = fields->int_field("lvl");
    level_type = str_to_level_area_type(fields->str_field("ltyp"));

    final_hp         = fields->int_field("hp");
    final_max_hp     = fields->int_field("mhp");
    final_max_max_hp = fields->int_field("mmhp");

    damage = fields->int_field("dam");
    str    = fields->int_field("str");
    intel  = fields->int_field("int");
    dex    = fields->int_field("dex");

    god      = str_to_god(fields->str_field("god"));
    piety    = fields->int_field("piety");
    penance  = fields->int_field("pen");
    wiz_mode = fields->int_field("wiz");

    birth_time = _parse_time(fields->str_field("start"));
    death_time = _parse_time(fields->str_field("end"));
    real_time  = fields->long_field("dur");
    num_turns  = fields->long_field("turn");

    num_diff_runes = fields->int_field("urune");
    num_runes      = fields->int_field("nrune");
}

void scorefile_entry::set_base_xlog_fields() const
{
    if (!fields.get())
        fields.reset(new xlog_fields);

    fields->add_field("v", "%s", version.empty()? VER_NUM : version.c_str());
    fields->add_field("lv", SCORE_VERSION);
    fields->add_field("name", "%s", name.c_str());
    fields->add_field("uid",  "%d", uid);
    fields->add_field("race", "%s",
                      species_name(static_cast<species_type>(race),
                                   lvl).c_str());
    fields->add_field("cls",  "%s", get_class_name(cls));
    fields->add_field("char", "%s%s",
                      get_species_abbrev(race), get_class_abbrev(cls));
    fields->add_field("xl",    "%d", lvl);
    fields->add_field("sk",    "%s", skill_name(best_skill));
    fields->add_field("sklev", "%d", best_skill_lvl);
    fields->add_field("title", "%s",
                      skill_title( best_skill, best_skill_lvl,
                                   race, str, dex, god ).c_str() );

    // "place" is a human readable place name, and it is write-only,
    // so we can write place names like "Bazaar" that Crawl cannot
    // translate back. This does have the unfortunate side-effect that
    // Crawl will not preserve the "place" field in the highscores file.
    fields->add_field("place", "%s",
                      place_name(get_packed_place(branch, dlvl, level_type),
                                 false, true).c_str());

    // Note: "br", "lvl" and "ltyp" are saved in canonical names that
    // can be read back by future versions of Crawl.
    fields->add_field("br",   "%s", _short_branch_name(branch));
    fields->add_field("lvl",  "%d", dlvl);
    fields->add_field("ltyp", "%s", level_area_type_name(level_type));

    fields->add_field("hp",   "%d", final_hp);
    fields->add_field("mhp",  "%d", final_max_hp);
    fields->add_field("mmhp", "%d", final_max_max_hp);
    fields->add_field("str", "%d", str);
    fields->add_field("int", "%d", intel);
    fields->add_field("dex", "%d", dex);

    // Don't write No God to save some space.
    if (god != -1)
        fields->add_field("god", "%s", god == GOD_NO_GOD? "" :
                          god_name(god).c_str());

    if (wiz_mode)
        fields->add_field("wiz", "%d", wiz_mode);

    fields->add_field("start", "%s", make_date_string(birth_time).c_str());
    fields->add_field("dur",   "%ld", real_time);
    fields->add_field("turn",  "%ld", num_turns);

    if (num_diff_runes)
        fields->add_field("urune", "%d", num_diff_runes);

    if (num_runes)
        fields->add_field("nrune", "%d", num_runes);
}

void scorefile_entry::set_score_fields() const
{
    fields.reset(new xlog_fields);

    if (!fields.get())
        return;

    set_base_xlog_fields();

    fields->add_field("sc", "%ld", points);
    fields->add_field("ktyp", ::kill_method_name(kill_method_type(death_type)));
    fields->add_field("killer", death_source_desc().c_str());
    fields->add_field("dam", "%d", damage);

    fields->add_field("kaux", "%s", auxkilldata.c_str());

    if (piety > 0)
        fields->add_field("piety", "%d", piety);
    if (penance > 0)
        fields->add_field("pen", "%d", penance);

    fields->add_field("end", "%s", make_date_string(death_time).c_str());

#ifdef DGL_EXTENDED_LOGFILES
    const std::string short_msg = short_kill_message();
    fields->add_field("tmsg", "%s", short_msg.c_str());
    const std::string long_msg = long_kill_message();
    if (long_msg != short_msg)
        fields->add_field("vmsg", "%s", long_msg.c_str());
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
    return (comma_separated_line(lines.begin(), lines.end(), " ", " "));
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
    if ((death_type == KILLED_BY_MONSTER
         || death_type == KILLED_BY_BEAM
         || death_type == KILLED_BY_SPORE
         || death_type == KILLED_BY_REFLECTION)
        && !invalid_monster_index(death_source)
        && menv[death_source].type != -1)
    {
        const monsters *monster = &menv[death_source];

        death_source = monster->type;
        mon_num = monster->base_monster;

        // Previously the weapon was only used for dancing weapons,
        // but now we pass it in as a string through the scorefile
        // entry to be appended in hiscores_format_single in long or
        // medium scorefile formats.
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
                auxkilldata = mitm[monster->inv[MSLOT_WEAPON]].name(DESC_NOCAP_A);
        }

        const bool death = you.hp <= 0;

        const description_level_type desc =
            death_type == KILLED_BY_SPORE ? DESC_PLAIN : DESC_NOCAP_A;

        death_source_name = monster->name(desc, death);
        if (death || you.can_see(monster))
            death_source_name = monster->full_name(desc, true);

        if (monster->has_ench(ENCH_SHAPESHIFTER))
            death_source_name += " (shapeshifter)";
        else if (monster->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            death_source_name += " (glowing shapeshifter)";
    }
    else
    {
        mon_num = 0;
        death_source_name.clear();
    }

    if (death_type == KILLED_BY_WEAKNESS
        || death_type == KILLED_BY_STUPIDITY
        || death_type == KILLED_BY_CLUMSINESS)
    {
        if (auxkilldata.empty())
            auxkilldata = "unknown source";
    }
}

void scorefile_entry::reset()
{
    // simple init
    version.clear();
    points               = -1;
    name.clear();
    uid                  = 0;
    race                 = 0;
    cls                  = 0;
    lvl                  = 0;
    race_class_name.clear();
    best_skill           = 0;
    best_skill_lvl       = 0;
    death_type           = KILLED_BY_SOMETHING;
    death_source         = NON_MONSTER;
    mon_num              = 0;
    death_source_name.clear();
    auxkilldata.clear();
    dlvl                 = 0;
    level_type           = LEVEL_DUNGEON;
    branch               = BRANCH_MAIN_DUNGEON;
    final_hp             = -1;
    final_max_hp         = -1;
    final_max_max_hp     = -1;
    str                  = -1;
    intel                = -1;
    dex                  = -1;
    damage               = -1;
    god                  = GOD_NO_GOD;
    piety                = -1;
    penance              = -1;
    wiz_mode             = 0;
    birth_time           = 0;
    death_time           = 0;
    real_time            = -1;
    num_turns            = -1;
    num_diff_runes       = 0;
    num_runes            = 0;
}

static int _award_modified_experience()
{
    int xp = you.experience;
    int result = 0;

    if (xp <= 250000)
        return ((xp * 7) / 10);

    result += (250000 * 7) / 10;
    xp -= 250000;

    if (xp <= 750000)
    {
        result += (xp * 4) / 10;
        return (result);
    }

    result += (750000 * 4) / 10;
    xp -= 750000;

    if (xp <= 2000000)
    {
        result += (xp * 2) / 10;
        return (result);
    }

    result += (2000000 * 2) / 10;
    xp -= 2000000;

    result += (xp / 10);

    return (result);
}

void scorefile_entry::init()
{
    // Score file entry version:
    //
    // 4.0      - original versioned entry
    // 4.1      - added real_time and num_turn fields
    // 4.2      - stats and god info

    version = VER_NUM;
    name    = you.your_name;

#ifdef MULTIUSER
    uid = static_cast<int>(getuid());
#else
    uid = 0;
#endif

    /*
     *  old scoring system:
     *
     *    Gold
     *    + 0.7 * Experience
     *    + (distinct Runes +2)^2 * 1000, winners with distinct runes >= 3 only
     *    + value of Inventory, for winners only
     *
     *
     *  new scoring system, as suggested by Lemuel:
     *
     *    Gold
     *    + 0.7 * Experience up to 250,000
     *    + 0.4 * Experience between 250,000 and 1,000,000
     *    + 0.2 * Experience between 1,000,000 and 3,000,000
     *    + 0.1 * Experience above 3,000,000
     *    + (distinct Runes +2)^2 * 1000, winners with distinct runes >= 3 only
     *    + value of Inventory, for winners only
     *    + (250,000 * d. runes) * (25,000/(turns/d. runes)), for winners only
     *
     */

    // do points first.
    points = you.gold;
    points += _award_modified_experience();

    num_runes      = 0;
    num_diff_runes = 0;

    FixedVector< int, NUM_RUNE_TYPES >  rune_array;
    rune_array.init(0);

    // inventory value is only calculated for winners
    const bool calc_item_values = (death_type == KILLED_BY_WINNING);

    // Calculate value of pack and runes when character leaves dungeon
    for (int d = 0; d < ENDOFPACK; d++)
    {
        if (is_valid_item( you.inv[d] ))
        {
            if (calc_item_values)
                points += item_value( you.inv[d], true );

            if (you.inv[d].base_type == OBJ_MISCELLANY
                && you.inv[d].sub_type == MISC_RUNE_OF_ZOT)
            {
                num_runes += you.inv[d].quantity;

                // Don't assert in rune_array[] due to buggy runes,
                // since checks for buggy runes are already done
                // elsewhere.
                if (you.inv[d].plus < 0 || you.inv[d].plus >= NUM_RUNE_TYPES)
                {
                    mpr("WARNING: Buggy rune in pack!", MSGCH_ERROR);
                    // Be nice and assume the buggy rune was originally
                    // different from any of the other rune types.
                    num_diff_runes++;
                    continue;
                }

                if (rune_array[ you.inv[d].plus ] == 0)
                    num_diff_runes++;

                rune_array[ you.inv[d].plus ] += you.inv[d].quantity;
            }
        }
    }

    // Bonus for exploring different areas, not for collecting a
    // huge stack of demonic runes in Pandemonium (gold value
    // is enough for those). -- bwr

    if (calc_item_values && num_diff_runes >= 3)
        points += ((num_diff_runes + 2) * (num_diff_runes + 2) * 1000);

    if (calc_item_values) // winners only
    {
        points +=
            static_cast<long>(
                (250000 * num_diff_runes)
                * ((25000.0 * num_diff_runes) / (1+you.num_turns)));
    }

    // Players will have a hard time getting 1/10 of this (see XP cap):
    if (points > 99999999)
        points = 99999999;

    race = you.species;
    cls  = you.char_class;

    race_class_name.clear();

    lvl            = you.experience_level;
    best_skill     = ::best_skill( SK_FIGHTING, NUM_SKILLS - 1, 99 );
    best_skill_lvl = you.skills[ best_skill ];

    final_hp         = you.hp;
    final_max_hp     = you.hp_max;
    final_max_max_hp = get_real_hp(true, true);

    str   = std::max(you.strength - stat_modifier(STAT_STRENGTH), 1);
    intel = std::max(you.intel - stat_modifier(STAT_INTELLIGENCE), 1);
    dex   = std::max(you.dex - stat_modifier(STAT_DEXTERITY), 1);

    god = you.religion;
    if (you.religion != GOD_NO_GOD)
    {
        piety   = you.piety;
        penance = you.penance[you.religion];
    }

    // main dungeon: level is simply level
    dlvl       = player_branch_depth();
    branch     = you.where_are_you;  // no adjustments necessary.
    level_type = you.level_type;     // pandemonium, labyrinth, dungeon..

    birth_time = you.birth_time;     // start time of game
    death_time = time( NULL );       // end time of game

    if (you.real_time != -1)
        real_time = you.real_time + long(death_time - you.start_time);
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

#ifdef MULTIUSER
            if (uid > 0)
            {
                struct passwd *pw_entry = getpwuid(uid);
                if (pw_entry)
                {
                    strncpy(username, pw_entry->pw_name, sizeof(username));
                    username[0] = toupper(username[0]);
                    strncat(username, "'s", sizeof(username));
                }
            }
#endif
            snprintf(scratch, INFO_SIZE, "%s game lasted %s (%ld turns).",
                     username, make_time_string(real_time).c_str(),
                     num_turns);

            line += scratch;
            line += _hiscore_newline_string();
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

std::string scorefile_entry::death_source_desc() const
{
    if (death_type != KILLED_BY_MONSTER && death_type != KILLED_BY_BEAM)
        return ("");

    // XXX no longer handles mons_num correctly! FIXME
    return (!death_source_name.empty() ?
            death_source_name : mons_type_name(death_source, DESC_NOCAP_A));
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
        char_desc = make_stringf("%s%s", get_species_abbrev( race ),
                                         get_class_abbrev( cls ) );
    }
    else
        char_desc = race_class_name;

    std::string scname = name;
    if (scname.length() > 10)
        scname = scname.substr(0, 10);

    return make_stringf( "%8ld %-10s %s-%02d%s", points, scname.c_str(),
                         char_desc.c_str(), lvl, (wiz_mode == 1) ? "W" : "" );
}

std::string
scorefile_entry::character_description(death_desc_verbosity verbosity) const
{
    bool single  = verbosity == DDV_TERSE || verbosity == DDV_ONELINE;

    if (single)
        return single_cdesc();

    bool verbose = verbosity == DDV_VERBOSE;
    char scratch[INFO_SIZE];
    char buf[HIGHSCORE_SIZE];

    std::string desc;
    // Please excuse the following bit of mess in the name of flavour ;)
    if (verbose)
    {
        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s (level %d",
                  points, name.c_str(),
                  skill_title( best_skill, best_skill_lvl,
                               race, str, dex, god ).c_str(), lvl );
        desc = buf;
    }
    else
    {
        snprintf( buf, HIGHSCORE_SIZE, "%8ld %s the %s %s (level %d",
                  points, name.c_str(),
                  species_name(static_cast<species_type>(race), lvl).c_str(),
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
    desc += _hiscore_newline_string();

    if (verbose)
    {
        std::string srace = species_name(static_cast<species_type>(race), lvl);
        snprintf( scratch, INFO_SIZE, "Began as a%s %s %s",
                  is_vowel(srace[0]) ? "n" : "",
                  srace.c_str(),
                  get_class_name(cls) );
        desc += scratch;

        if (birth_time > 0)
        {
            desc += " on ";
            _hiscore_date_string( birth_time, scratch );
            desc += scratch;
        }

        desc += ".";
        desc += _hiscore_newline_string();

        if (race != SP_DEMIGOD && god != -1)
        {
            if (god == GOD_XOM)
            {
                snprintf( scratch, INFO_SIZE, "Was a %sPlaything of Xom.",
                                   (lvl >= 20) ? "Favourite " : "" );

                desc += scratch;
                desc += _hiscore_newline_string();
            }
            else if (god != GOD_NO_GOD)
            {
                // Not exactly the same as the religion screen, but
                // good enough to fill this slot for now.
                snprintf( scratch, INFO_SIZE, "Was %s of %s%s",
                             (piety >  160) ? "the Champion" :
                             (piety >= 120) ? "a High Priest" :
                             (piety >= 100) ? "an Elder" :
                             (piety >=  75) ? "a Priest" :
                             (piety >=  50) ? "a Believer" :
                             (piety >=  30) ? "a Follower"
                                            : "an Initiate",
                          god_name(god).c_str(),
                             (penance > 0) ? " (penitent)." : "." );

                desc += scratch;
                desc += _hiscore_newline_string();
            }
        }
    }

    return (desc);
}

std::string scorefile_entry::death_place(death_desc_verbosity verbosity) const
{
    bool verbose = (verbosity == DDV_VERBOSE);
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
        place_name(get_packed_place(branch, dlvl, level_type), true, true);

    // add appropriate prefix
    if (placename.find("Level") == 0)
        place += " on ";
    else
        place += " in ";

    place += placename;

    if (verbose && death_time
        && !_hiscore_same_day( birth_time, death_time ))
    {
        place += " on ";
        _hiscore_date_string( death_time, scratch );
        place += scratch;
    }

    place += ".";
    place += _hiscore_newline_string();

    return (place);
}

static bool _species_is_undead(int sp)
{
    return (sp == SP_MUMMY || sp == SP_GHOUL || sp == SP_VAMPIRE);
}

std::string scorefile_entry::death_description(death_desc_verbosity verbosity)
            const
{
    bool needs_beam_cause_line = false;
    bool needs_called_by_monster_line = false;
    bool needs_damage = false;

    const bool terse   = (verbosity == DDV_TERSE);
    const bool semiverbose = (verbosity == DDV_LOGVERBOSE);
    const bool verbose = (verbosity == DDV_VERBOSE || semiverbose);
    const bool oneline = (verbosity == DDV_ONELINE);

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
            desc += "slain by " + death_source_desc();
        else
        {
            desc += damage_verb();
            desc += " by ";
            desc += death_source_desc();
        }

        // put the damage on the weapon line if there is one
        if (auxkilldata.empty())
            needs_damage = true;
        break;

    case KILLED_BY_POISON:
        desc += terse? "poison" : "Succumbed to poison";
        break;

    case KILLED_BY_CLOUD:
        if (auxkilldata.empty())
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
                      _range_type_verb( auxkilldata.c_str() ) );
            desc += scratch;
            desc += death_source_desc();

            if (semiverbose)
            {
                std::string beam = terse_missile_name();
                if (beam.empty())
                    beam = terse_beam_cause();
                trim_string(beam);
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
        if (terse)
            desc += "stupidity";
        else if (_species_is_undead(race))
            desc += "Forgot to exist";
        else
            desc += "Forgot to breathe";
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
                      "Killed by triggering a%s%s trap",
                      auxkilldata.empty() ? "" : " ",
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
            else if (_species_is_undead(race))
                desc += "Safely got out of the dungeon";
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

    case KILLED_BY_XOM:
        if (terse)
            desc += "xom";
        else
            desc += auxkilldata.empty() ? "Killed for Xom's enjoyment"
                                        : auxkilldata;
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

    case KILLED_BY_REFLECTION:
        needs_damage = true;
        if (terse)
            desc += "reflected bolt";
        else
        {
            desc += "Killed by a reflected ";
            if (auxkilldata.empty())
                desc += "bolt";
            else
                desc += auxkilldata;

            if (!death_source_name.empty() && !oneline && !semiverbose)
            {
                desc += "\n";
                desc += "             ";
                desc += "... reflected by ";
                desc += death_source_name;
                needs_damage = false;
            }
        }
        break;

    case KILLED_BY_BOUNCE:
        if (terse)
            desc += "bounced beam";
        else
        {
            desc += "Killed themselves with a bounced ";
            if (auxkilldata.empty())
                desc += "beam";
            else
                desc += auxkilldata;
        }
        needs_damage = true;
        break;

    case KILLED_BY_SPORE:
        if (terse)
        {
            if (death_source_name.empty())
                desc += "spore";
            else
                desc += get_monster_data(death_source)->name;
        }
        else
        {
            desc += "Killed by an exploding ";
            if (death_source_name.empty())
                desc += "spore";
            else
                desc += death_source_name;
        }
        needs_damage = true;
        break;

    case KILLED_BY_TSO_SMITING:
        desc += terse? "smote by Shining One" : "Smote by the Shining One";
        needs_damage = true;
        break;

    case KILLED_BY_BEOGH_SMITING:
        desc += terse? "smote by Beogh" : "Smote by Beogh";
        needs_damage = true;
        break;

    case KILLED_BY_PETRIFICATION:
        desc += terse? "petrified" : "Turned to stone";
        break;

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

    case KILLED_BY_DIVINE_WRATH:
        if (terse)
            desc += "divine wrath";
        else
            desc += auxkilldata.empty() ? "Divine wrath" : auxkilldata;
        needs_damage = true;
        break;

    default:
        desc += terse? "program bug" : "Nibbled to death by software bugs";
        break;
    }                           // end switch

    switch (death_type)
    {
    case KILLED_BY_STUPIDITY:
    case KILLED_BY_WEAKNESS:
    case KILLED_BY_CLUMSINESS:
        if (terse)
        {
            desc += " (";
            desc += auxkilldata;
            desc += ")";
        }
        else
        {
            desc += "\n";
            desc += "             ";
            desc += "... caused by ";
            desc += auxkilldata;
        }
        break;

    default:
        break;
    }

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

        if (death_type == KILLED_BY_LEAVING
            || death_type == KILLED_BY_WINNING)
        {
            if (num_runes > 0)
            {
                desc += _hiscore_newline_string();

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
                    && !_hiscore_same_day( birth_time, death_time ))
                {
                    desc += " on ";
                    _hiscore_date_string( death_time, scratch );
                    desc += scratch;
                }

                desc += "!";
                desc += _hiscore_newline_string();
            }
            else
                desc += ".";
        }
        else if (death_type != KILLED_BY_QUITTING)
        {
            desc += _hiscore_newline_string();

            if (death_type == KILLED_BY_MONSTER && !auxkilldata.empty())
            {
                if (!semiverbose)
                {
                    snprintf(scratch, INFO_SIZE, "... wielding %s",
                             auxkilldata.c_str());
                    desc += scratch;
                    needs_damage = true;
                }
                else
                    desc += make_stringf(" (%s)", auxkilldata.c_str());
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
                    desc += _hiscore_newline_string();
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
                else
                    desc += ".";
            }
            desc += _hiscore_newline_string();
        }
    }

    if (death_type == KILLED_BY_SPORE && !terse && !auxkilldata.empty())
    {
        desc += "... ";
        desc += auxkilldata;
        desc += "\n";
        desc += "             ";
    }

    if (terse)
    {
        trim_string(desc);
        desc = strip_article_a(desc);
    }

    return (desc);
}

//////////////////////////////////////////////////////////////////////////////
// xlog_fields

xlog_fields::xlog_fields() : fields(), fieldmap()
{
}

xlog_fields::xlog_fields(const std::string &line) : fields(), fieldmap()
{
    init(line);
}

std::string::size_type
xlog_fields::next_separator(const std::string &s,
                            std::string::size_type start) const
{
    std::string::size_type p = s.find(':', start);
    if (p != std::string::npos && p < s.length() - 1 && s[p + 1] == ':')
        return next_separator(s, p + 2);

    return (p);
}

std::vector<std::string> xlog_fields::split_fields(const std::string &s) const
{
    std::string::size_type start = 0, end = 0;
    std::vector<std::string> fs;

    for ( ; (end = next_separator(s, start)) != std::string::npos;
          start = end + 1  )
    {
        fs.push_back( s.substr(start, end - start) );
    }

    if (start < s.length())
        fs.push_back( s.substr(start) );

    return (fs);
}

void xlog_fields::init(const std::string &line)
{
    std::vector<std::string> rawfields = split_fields(line);
    for (int i = 0, size = rawfields.size(); i < size; ++i)
    {
        const std::string field = rawfields[i];
        std::string::size_type st = field.find('=');
        if (st == std::string::npos)
            continue;

        fields.push_back(
            std::pair<std::string, std::string>(
                field.substr(0, st),
                xlog_unescape(field.substr(st + 1)) ) );
    }

    map_fields();
}

// xlogfile escape: s/:/::/g
std::string xlog_fields::xlog_escape(const std::string &s) const
{
    return replace_all(s, ":", "::");
}

// xlogfile unescape: s/::/:/g
std::string xlog_fields::xlog_unescape(const std::string &s) const
{
    return replace_all(s, "::", ":");
}

void xlog_fields::add_field(const std::string &key,
                            const char *format,
                            ...)
{
    char buf[500];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);

    fields.push_back( std::pair<std::string, std::string>( key, buf ) );
    fieldmap[key] = buf;
}

std::string xlog_fields::str_field(const std::string &s) const
{
    xl_map::const_iterator i = fieldmap.find(s);
    if (i == fieldmap.end())
        return ("");

    return i->second;
}

int xlog_fields::int_field(const std::string &s) const
{
    std::string field = str_field(s);
    return atoi(field.c_str());
}

long xlog_fields::long_field(const std::string &s) const
{
    std::string field = str_field(s);
    return atol(field.c_str());
}

void xlog_fields::map_fields() const
{
    fieldmap.clear();
    for (int i = 0, size = fields.size(); i < size; ++i)
    {
        const std::pair<std::string, std::string> &f = fields[i];
        fieldmap[f.first] = f.second;
    }
}

std::string xlog_fields::xlog_line() const
{
    std::string line;
    for (int i = 0, size = fields.size(); i < size; ++i)
    {
        const std::pair<std::string, std::string> &f = fields[i];

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

///////////////////////////////////////////////////////////////////////////////
// Milestones

#ifdef DGL_MILESTONES

void mark_milestone(const std::string &type, const std::string &milestone)
{
    const std::string milestone_file = Options.save_dir + "milestones.txt";
    if (FILE *fp = lk_open("a", milestone_file))
    {
        const scorefile_entry se(0, 0, KILL_MISC, NULL);
        se.set_base_xlog_fields();
        xlog_fields xl = *se.fields;
        xl.add_field("time", "%s", make_date_string(se.death_time).c_str());
        xl.add_field("type", "%s", type.c_str());
        xl.add_field("milestone", "%s", milestone.c_str());
        fprintf(fp, "%s\n", xl.xlog_line().c_str());
        lk_close(fp, "a", milestone_file);
    }
}

#endif // DGL_MILESTONES

#ifdef DGL_WHEREIS
std::string xlog_status_line()
{
    const scorefile_entry se(0, 0, KILL_MISC, NULL);
    se.set_base_xlog_fields();
    se.fields->add_field("time", "%s", make_date_string(time(NULL)).c_str());
    return (se.fields->xlog_line());
}
#endif // DGL_WHEREIS

/**
 * @file
 * @brief deal with reading and writing of highscore file
**/

/*
 * ----------- MODIFYING THE PRINTED SCORE FORMAT ---------------------
 *   Do this at your leisure. Change hiscores_format_single() as much
 * as you like.
 *
 */

#include "AppHdr.h"

#include "hiscores.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#ifndef TARGET_COMPILER_VC
#include <unistd.h>
#endif

#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "dungeon.h"
#include "end.h"
#include "english.h"
#include "files.h"
#include "initfile.h"
#include "itemprop.h"
#include "items.h"
#include "jobs.h"
#include "kills.h"
#include "libutil.h"
#include "menu.h"
#include "misc.h"
#include "mon-util.h"
#include "options.h"
#include "ouch.h"
#include "place.h"
#include "religion.h"
#include "skills.h"
#include "state.h"
#include "status.h"
#include "stringutil.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilereg-crt.h"
#endif
#include "unwind.h"
#include "version.h"

#define SCORE_VERSION "0.1"

// enough memory allocated to snarf in the scorefile entries
static unique_ptr<scorefile_entry> hs_list[SCORE_FILE_ENTRIES];

static FILE *_hs_open(const char *mode, const string &filename);
static void  _hs_close(FILE *handle, const string &filename);
static bool  _hs_read(FILE *scores, scorefile_entry &dest);
static void  _hs_write(FILE *scores, scorefile_entry &entry);
static time_t _parse_time(const string &st);
static string _xlog_escape(const string &s);
static string _xlog_unescape(const string &s);
static vector<string> _xlog_split_fields(const string &s);

static string _score_file_name()
{
    string ret;
    if (!SysEnv.scorefile.empty())
        ret = SysEnv.scorefile;
    else
        ret = Options.shared_dir + "scores";

    ret += crawl_state.game_type_qualifier();
    if (crawl_state.game_is_sprint() && crawl_state.map != "")
        ret += "-" + crawl_state.map;

    return ret;
}

static string _log_file_name()
{
    return Options.shared_dir + "logfile" + crawl_state.game_type_qualifier();
}

int hiscores_new_entry(const scorefile_entry &ne)
{
    unwind_bool score_update(crawl_state.updating_scores, true);

    FILE *scores;
    int i, total_entries;
    bool inserted = false;
    int newest_entry = -1;

    // open highscore file (reading) -- nullptr is fatal!
    //
    // Opening as a+ instead of r+ to force an exclusive lock (see
    // hs_open) and to create the file if it's not there already.
    scores = _hs_open("a+", _score_file_name());
    if (scores == nullptr)
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
        if (!inserted && ne.get_score() >= hs_list[i]->get_score())
        {
            newest_entry = i;           // for later printing
            inserted = true;
            // copy read entry to i+1th position
            // Fixed a nasty overflow bug here -- Sharp
            if (i+1 < SCORE_FILE_ENTRIES)
            {
                hs_list[i + 1] = move(hs_list[i]);
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
        _hs_close(scores, _score_file_name());
        return -1;
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
        hs_list[i].reset(nullptr);
    }

    // close scorefile.
    _hs_close(scores, _score_file_name());
    return newest_entry;
}

void logfile_new_entry(const scorefile_entry &ne)
{
    unwind_bool logfile_update(crawl_state.updating_scores, true);

    FILE *logfile;
    scorefile_entry le = ne;

    // open logfile (appending) -- nullptr *is* fatal here.
    logfile = _hs_open("a", _log_file_name());
    if (logfile == nullptr)
    {
        mprf(MSGCH_ERROR, "ERROR: failure writing to the logfile.");
        return;
    }

    _hs_write(logfile, le);

    // close logfile.
    _hs_close(logfile, _log_file_name());
}

template <class t_printf>
static void _hiscores_print_entry(const scorefile_entry &se,
                                  int index, int format, t_printf pf)
{
    char buf[200];
    // print position (tracked implicitly by order score file)
    snprintf(buf, sizeof buf, "%3d.", index + 1);

    pf("%s", buf);

    string entry;
    // format the entry
    if (format == SCORE_TERSE)
        entry = hiscores_format_single(se);
    else
        entry = hiscores_format_single_long(se, (format == SCORE_VERBOSE));

    entry += "\n";
    pf("%s", entry.c_str());
}

// Writes all entries in the scorefile to stdout in human-readable form.
void hiscores_print_all(int display_count, int format)
{
    unwind_bool scorefile_display(crawl_state.updating_scores, true);

    FILE *scores = _hs_open("r", _score_file_name());
    if (scores == nullptr)
    {
        // will only happen from command line
        puts("No scores.");
        return;
    }

    for (int entry = 0; display_count <= 0 || entry < display_count; ++entry)
    {
        scorefile_entry se;
        if (!_hs_read(scores, se))
            break;

        if (format == -1)
            printf("%s", se.raw_string().c_str());
        else
            _hiscores_print_entry(se, entry, format, printf);
    }

    _hs_close(scores, _score_file_name());
}

// Displays high scores using curses. For output to the console, use
// hiscores_print_all.
void hiscores_print_list(int display_count, int format, int newest_entry)
{
    unwind_bool scorefile_display(crawl_state.updating_scores, true);

    FILE *scores;
    int i, total_entries;

    if (display_count <= 0)
        return;

    // open highscore file (reading)
    scores = _hs_open("r", _score_file_name());
    if (scores == nullptr)
        return;

    // read highscore file
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        hs_list[i].reset(new scorefile_entry);
        if (_hs_read(scores, *hs_list[i]) == false)
            break;
    }
    total_entries = i;

    // close off
    _hs_close(scores, _score_file_name());

    textcolour(LIGHTGREY);

    int start = newest_entry - display_count / 2;

    if (start + display_count > total_entries)
        start = total_entries - display_count;

    if (start < 0)
        start = 0;

    const int finish = start + display_count;

    for (i = start; i < finish && i < total_entries; i++)
    {
        // check for recently added entry
        if (i == newest_entry)
            textcolour(YELLOW);

        _hiscores_print_entry(*hs_list[i], i, format, cprintf);

        if (i == newest_entry)
            textcolour(LIGHTGREY);
    }
}

static void _add_hiscore_row(MenuScroller* scroller, scorefile_entry& se, int id)
{
    TextItem* tmp = nullptr;
    tmp = new TextItem();

    coord_def min_coord(1,1);
    coord_def max_coord(1,2);

    tmp->set_fg_colour(WHITE);
    tmp->set_highlight_colour(WHITE);

    tmp->set_text(hiscores_format_single(se));
    tmp->set_description_text(hiscores_format_single_long(se, true));
    tmp->set_id(id);
    tmp->set_bounds(coord_def(1,1), coord_def(1,2));

    scroller->attach_item(tmp);
    tmp->set_visible(true);
}

static void _construct_hiscore_table(MenuScroller* scroller)
{
    FILE *scores = _hs_open("r", _score_file_name());

    if (scores == nullptr)
        return;

    int i;
    // read highscore file
    for (i = 0; i < SCORE_FILE_ENTRIES; i++)
    {
        hs_list[i].reset(new scorefile_entry);
        if (_hs_read(scores, *hs_list[i]) == false)
            break;
    }

    _hs_close(scores, _score_file_name());

    for (int j=0; j<i; j++)
        _add_hiscore_row(scroller, *hs_list[j], j);
}

static void _show_morgue(scorefile_entry& se)
{
    formatted_scroller morgue_file;
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP;
    if (Options.easy_exit_menu)
        flags |= MF_EASY_EXIT;

    morgue_file.set_flags(flags, false);
    morgue_file.set_tag("morgue");
    morgue_file.set_more();

    string morgue_base = morgue_name(se.get_name(), se.get_death_time());
    string morgue_path = morgue_directory()
                         + strip_filename_unsafe_chars(morgue_base) + ".txt";
    FILE* morgue = lk_open("r", morgue_path);

    if (!morgue)
        return;

    char buf[200];
    string morgue_text = "";

    while (fgets(buf, sizeof buf, morgue) != nullptr)
    {
        string line = string(buf);
        size_t newline_pos = line.find_last_of('\n');
        if (newline_pos != string::npos)
            line.erase(newline_pos);
        morgue_text += "<w>" + line + "</w>" + '\n';
    }

    lk_close(morgue, morgue_path);

    clrscr();

    column_composer cols(2, 40);
    cols.add_formatted(
            0,
            morgue_text,
            true);

    vector<formatted_string> blines = cols.formatted_lines();

    unsigned i;
    for (i = 0; i < blines.size(); ++i)
        morgue_file.add_item_formatted_string(blines[i]);

    textcolour(WHITE);
    morgue_file.show();
}

void show_hiscore_table()
{
    unwind_var<string> sprintmap(crawl_state.map, crawl_state.sprint_map);
    const int max_line   = get_number_of_lines() - 1;
    const int max_col    = get_number_of_cols() - 1;

    const int scores_col_start = 4;
    const int descriptor_col_start = 4;
    const int scores_row_start = 10;
    const int scores_col_end = max_col;
    const int scores_row_end = max_line - 1;

    bool smart_cursor_enabled = is_smart_cursor_enabled();

    clrscr();

    PrecisionMenu menu;
    menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);

    MenuScroller* score_entries = new MenuScroller();

    score_entries->init(coord_def(scores_col_start, scores_row_start),
            coord_def(scores_col_end, scores_row_end), "score entries");

    _construct_hiscore_table(score_entries);

    MenuDescriptor* descriptor = new MenuDescriptor(&menu);
    descriptor->init(coord_def(descriptor_col_start, 1),
            coord_def(get_number_of_cols(), scores_row_start - 1),
            "descriptor");

#ifdef USE_TILE_LOCAL
    BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
#else
    BlackWhiteHighlighter* highlighter = new BlackWhiteHighlighter(&menu);
#endif
    highlighter->init(coord_def(-1,-1), coord_def(-1,-1), "highlighter");

    MenuFreeform* freeform = new MenuFreeform();
    freeform->init(coord_def(1, 1), coord_def(max_col, max_line), "freeform");
    // This freeform will only contain unfocusable texts
    freeform->allow_focus(false);
    freeform->set_visible(true);

    NoSelectTextItem* tmp = new NoSelectTextItem();
    string text = "[  Up/Down or PgUp/PgDn to scroll.         Esc or R-click "
        "exits.  ]";
    tmp->set_text(text);
    tmp->set_bounds(coord_def(1, max_line - 1), coord_def(max_col - 1, max_line));
    tmp->set_fg_colour(CYAN);
    freeform->attach_item(tmp);
    tmp->set_visible(true);

#ifdef USE_TILE_LOCAL
    tiles.get_crt()->attach_menu(&menu);
#endif

    score_entries->set_visible(true);
    descriptor->set_visible(true);
    highlighter->set_visible(true);

    menu.attach_object(freeform);
    menu.attach_object(score_entries);
    menu.attach_object(descriptor);
    menu.attach_object(highlighter);

    menu.set_active_object(score_entries);
    score_entries->set_active_item((MenuItem*) nullptr);
    score_entries->activate_first_item();

    enable_smart_cursor(false);
    while (true)
    {
        menu.draw_menu();
        textcolour(WHITE);
        const int keyn = getch_ck();

        if (keyn == CK_REDRAW)
            continue;

        if (key_is_escape(keyn) || keyn == CK_MOUSE_CMD)
        {
            // Go back to the menu and return the smart cursor to its previous state
            enable_smart_cursor(smart_cursor_enabled);
            return;
        }

        if (menu.process_key(keyn))
        {
            menu.clear_selections();
            _show_morgue(*hs_list[menu.get_active_item()->get_id()]);
            clrscr();
#ifdef USE_TILE_LOCAL
            tiles.get_crt()->attach_menu(&menu);
#endif
        }
    }
}

// Trying to supply an appropriate verb for the attack type. -- bwr
static const char *_range_type_verb(const char *const aux)
{
    if (strncmp(aux, "Shot ", 5) == 0)                // launched
        return "shot";
    else if (aux[0] == 0                                // unknown
             || strncmp(aux, "Hit ", 4) == 0          // thrown
             || strncmp(aux, "volley ", 7) == 0)      // manticore spikes
    {
        return "hit from afar";
    }

    return "blasted";                                 // spells, wands
}

string hiscores_format_single(const scorefile_entry &se)
{
    return se.hiscore_line(scorefile_entry::DDV_ONELINE);
}

static bool _hiscore_same_day(time_t t1, time_t t2)
{
    struct tm *d1  = TIME_FN(&t1);
    const int year = d1->tm_year;
    const int mon  = d1->tm_mon;
    const int day  = d1->tm_mday;

    struct tm *d2  = TIME_FN(&t2);

    return d2->tm_mday == day && d2->tm_mon == mon && d2->tm_year == year;
}

static string _hiscore_date_string(time_t time)
{
    struct tm *date = TIME_FN(&time);

    const char *mons[12] = { "Jan", "Feb", "Mar", "Apr", "May", "June",
                             "July", "Aug", "Sept", "Oct", "Nov", "Dec" };

    return make_stringf("%s %d, %d", mons[date->tm_mon], date->tm_mday,
                                     date->tm_year + 1900);
}

static string _hiscore_newline_string()
{
    return "\n             ";
}

string hiscores_format_single_long(const scorefile_entry &se, bool verbose)
{
    return se.hiscore_line(verbose ? scorefile_entry::DDV_VERBOSE
                                   : scorefile_entry::DDV_NORMAL);
}

// --------------------------------------------------------------------------
// BEGIN private functions
// --------------------------------------------------------------------------

static FILE *_hs_open(const char *mode, const string &scores)
{
    // allow reading from standard input
    if (scores == "-")
        return stdin;

    return lk_open(mode, scores);
}

static void _hs_close(FILE *handle, const string &scores)
{
    lk_close(handle, scores);
}

static bool _hs_read(FILE *scores, scorefile_entry &dest)
{
    char inbuf[1300];
    if (!scores || feof(scores))
        return false;

    memset(inbuf, 0, sizeof inbuf);
    dest.reset();

    if (!fgets(inbuf, sizeof inbuf, scores))
        return false;

    return dest.parse(inbuf);
}

static int _val_char(char digit)
{
    return digit - '0';
}

static time_t _parse_time(const string &st)
{
    struct tm  date;

    if (st.length() < 15)
        return static_cast<time_t>(0);

    date.tm_year  =   _val_char(st[0]) * 1000 + _val_char(st[1]) * 100
                    + _val_char(st[2]) *   10 + _val_char(st[3]) - 1900;

    date.tm_mon   = _val_char(st[4])  * 10 + _val_char(st[5]);
    date.tm_mday  = _val_char(st[6])  * 10 + _val_char(st[7]);
    date.tm_hour  = _val_char(st[8])  * 10 + _val_char(st[9]);
    date.tm_min   = _val_char(st[10]) * 10 + _val_char(st[11]);
    date.tm_sec   = _val_char(st[12]) * 10 + _val_char(st[13]);
    date.tm_isdst = (st[14] == 'D');

    return mktime(&date);
}

static void _hs_write(FILE *scores, scorefile_entry &se)
{
    fprintf(scores, "%s", se.raw_string().c_str());
}

static const char *kill_method_names[] =
{
    "mon", "pois", "cloud", "beam", "lava", "water",
    "stupidity", "weakness", "clumsiness", "trap", "leaving", "winning",
    "quitting", "wizmode", "draining", "starvation", "freezing", "burning",
    "wild_magic", "xom", "rotting", "targeting", "spore",
    "tso_smiting", "petrification", "something",
    "falling_down_stairs", "acid", "curare",
    "beogh_smiting", "divine_wrath", "bounce", "reflect", "self_aimed",
    "falling_through_gate", "disintegration", "headbutt", "rolling",
    "mirror_damage", "spines", "frailty", "barbs", "being_thrown",
    "collision",
};

static const char *_kill_method_name(kill_method_type kmt)
{
    COMPILE_CHECK(NUM_KILLBY == ARRAYSZ(kill_method_names));

    if (kmt == NUM_KILLBY)
        return "";

    return kill_method_names[kmt];
}

static kill_method_type _str_to_kill_method(const string &s)
{
    COMPILE_CHECK(NUM_KILLBY == ARRAYSZ(kill_method_names));

    for (int i = 0; i < NUM_KILLBY; ++i)
    {
        if (s == kill_method_names[i])
            return static_cast<kill_method_type>(i);
    }

    return NUM_KILLBY;
}

//////////////////////////////////////////////////////////////////////////
// scorefile_entry

scorefile_entry::scorefile_entry(int dam, mid_t dsource, int dtype,
                                 const char *aux, bool death_cause_only,
                                 const char *dsource_name, time_t dt)
{
    reset();

    init_death_cause(dam, dsource, dtype, aux, dsource_name);
    if (!death_cause_only)
        init(dt);
}

scorefile_entry::scorefile_entry()
{
    // Completely uninitialised, caveat user.
    reset();
}

scorefile_entry::scorefile_entry(const scorefile_entry &se)
{
    init_from(se);
}

scorefile_entry &scorefile_entry::operator = (const scorefile_entry &se)
{
    init_from(se);
    return *this;
}

void scorefile_entry::init_from(const scorefile_entry &se)
{
    version            = se.version;
    save_rcs_version   = se.save_rcs_version;
    save_tag_version   = se.save_tag_version;
    tiles              = se.tiles;
    points             = se.points;
    name               = se.name;
    race               = se.race;
    job                = se.job;
    race_class_name    = se.race_class_name;
    lvl                = se.lvl;
    best_skill         = se.best_skill;
    best_skill_lvl     = se.best_skill_lvl;
    title              = se.title;
    death_type         = se.death_type;
    death_source       = se.death_source;
    death_source_name  = se.death_source_name;
    death_source_flags = se.death_source_flags;
    auxkilldata        = se.auxkilldata;
    indirectkiller     = se.indirectkiller;
    killerpath         = se.killerpath;
    last_banisher      = se.last_banisher;
    dlvl               = se.dlvl;
    absdepth           = se.absdepth;
    branch             = se.branch;
    map                = se.map;
    mapdesc            = se.mapdesc;
    killer_map         = se.killer_map;
    final_hp           = se.final_hp;
    final_max_hp       = se.final_max_hp;
    final_max_max_hp   = se.final_max_max_hp;
    final_mp           = se.final_mp;
    final_max_mp       = se.final_max_mp;
    final_base_max_mp  = se.final_base_max_mp;
    damage             = se.damage;
    source_damage      = se.source_damage;
    turn_damage        = se.turn_damage;
    str                = se.str;
    intel              = se.intel;
    dex                = se.dex;
    ac                 = se.ac;
    ev                 = se.ev;
    sh                 = se.sh;
    god                = se.god;
    piety              = se.piety;
    penance            = se.penance;
    wiz_mode           = se.wiz_mode;
    explore_mode       = se.explore_mode;
    birth_time         = se.birth_time;
    death_time         = se.death_time;
    real_time          = se.real_time;
    num_turns          = se.num_turns;
    num_aut            = se.num_aut;
    num_diff_runes     = se.num_diff_runes;
    num_runes          = se.num_runes;
    kills              = se.kills;
    maxed_skills       = se.maxed_skills;
    fifteen_skills     = se.fifteen_skills;
    status_effects     = se.status_effects;
    gold               = se.gold;
    gold_spent         = se.gold_spent;
    gold_found         = se.gold_found;
    zigs               = se.zigs;
    zigmax             = se.zigmax;
    scrolls_used       = se.scrolls_used;
    potions_used       = se.potions_used;
    fixup_char_name();

    // We could just reset raw_line to "" instead.
    raw_line          = se.raw_line;
}

actor* scorefile_entry::killer() const
{
    return actor_by_mid(death_source);
}

xlog_fields scorefile_entry::get_fields() const
{
    if (!fields.get())
        return xlog_fields();
    else
        return *fields.get();
}

bool scorefile_entry::parse(const string &line)
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
        dprf("Corrupted xlog-line: %s", line.c_str());
        return false;
    }

    raw_line = line;
    return parse_scoreline(line);
}

string scorefile_entry::raw_string() const
{
    if (!raw_line.empty())
        return raw_line;

    set_score_fields();

    if (!fields.get())
        return "";

    return fields->xlog_line() + "\n";
}

bool scorefile_entry::parse_scoreline(const string &line)
{
    fields.reset(new xlog_fields(line));
    init_with_fields();

    return true;
}

static const char* _short_branch_name(int branch)
{
    if (branch >= 0 && branch < NUM_BRANCHES)
        return branches[branch].abbrevname;
    return "";
}

enum old_job_type
{
    OLD_JOB_THIEF        = -1,
    OLD_JOB_DEATH_KNIGHT = -2,
    OLD_JOB_PALADIN      = -3,
    OLD_JOB_REAVER       = -4,
    OLD_JOB_STALKER      = -5,
    OLD_JOB_JESTER       = -6,
    OLD_JOB_PRIEST       = -7,
    OLD_JOB_HEALER       = -8,
    NUM_OLD_JOBS = -OLD_JOB_HEALER
};

static const char* _job_name(int job)
{
    switch (job)
    {
    case OLD_JOB_THIEF:
        return "Thief";
    case OLD_JOB_DEATH_KNIGHT:
        return "Death Knight";
    case OLD_JOB_PALADIN:
        return "Paladin";
    case OLD_JOB_REAVER:
        return "Reaver";
    case OLD_JOB_STALKER:
        return "Stalker";
    case OLD_JOB_JESTER:
        return "Jester";
    case OLD_JOB_PRIEST:
        return "Priest";
    case OLD_JOB_HEALER:
        return "Healer";
    }

    return get_job_name(static_cast<job_type>(job));
}

static const char* _job_abbrev(int job)
{
    switch (job)
    {
    case OLD_JOB_THIEF:
        return "Th";
    case OLD_JOB_DEATH_KNIGHT:
        return "DK";
    case OLD_JOB_PALADIN:
        return "Pa";
    case OLD_JOB_REAVER:
        return "Re";
    case OLD_JOB_STALKER:
        return "St";
    case OLD_JOB_JESTER:
        return "Jr";
    case OLD_JOB_PRIEST:
        return "Pr";
    case OLD_JOB_HEALER:
        return "He";
    }

    return get_job_abbrev(static_cast<job_type>(job));
}

static int _job_by_name(const string& name)
{
    int job = get_job_by_name(name.c_str());

    if (job != JOB_UNKNOWN)
        return job;

    for (job = -1; job >= -NUM_OLD_JOBS; job--)
        if (name == _job_name(job))
            return job;

    return JOB_UNKNOWN;
}

enum old_species_type
{
    OLD_SP_ELF = -1,
    OLD_SP_HILL_DWARF = -2,
    OLD_SP_OGRE_MAGE = -3,
    OLD_SP_GREY_ELF = -4,
    OLD_SP_GNOME = -5,
    OLD_SP_MOUNTAIN_DWARF = -6,
    OLD_SP_SLUDGE_ELF = -7,
    OLD_SP_DJINNI = -8,
    OLD_SP_LAVA_ORC = -9,
    NUM_OLD_SPECIES = -OLD_SP_LAVA_ORC
};

static string _species_name(int race)
{
    switch (race)
    {
    case OLD_SP_ELF: return "Elf";
    case OLD_SP_HILL_DWARF: return "Hill Dwarf";
    case OLD_SP_OGRE_MAGE: return "Ogre-Mage";
    case OLD_SP_GREY_ELF: return "Grey Elf";
    case OLD_SP_GNOME: return "Gnome";
    case OLD_SP_MOUNTAIN_DWARF: return "Mountain Dwarf";
    case OLD_SP_SLUDGE_ELF: return "Sludge Elf";
    case OLD_SP_DJINNI: return "Djinni";
    case OLD_SP_LAVA_ORC: return "Lava Orc";
    }

    return species_name(static_cast<species_type>(race));
}

static const char* _species_abbrev(int race)
{
    switch (race)
    {
    case OLD_SP_ELF: return "El";
    case OLD_SP_HILL_DWARF: return "HD";
    case OLD_SP_OGRE_MAGE: return "OM";
    case OLD_SP_GREY_ELF: return "GE";
    case OLD_SP_GNOME: return "Gn";
    case OLD_SP_MOUNTAIN_DWARF: return "MD";
    case OLD_SP_SLUDGE_ELF: return "SE";
    case OLD_SP_DJINNI: return "Dj";
    case OLD_SP_LAVA_ORC: return "LO";
    }

    return get_species_abbrev(static_cast<species_type>(race));
}

static int _species_by_name(const string& name)
{
    int race = str_to_species(name);

    if (race != SP_UNKNOWN)
        return race;

    for (race = -1; race >= -NUM_OLD_SPECIES; race--)
        if (name == _species_name(race))
            return race;

    return SP_UNKNOWN;
}

void scorefile_entry::init_with_fields()
{
    version = fields->str_field("v");
    save_rcs_version = fields->str_field("vsavrv");
    save_tag_version = fields->str_field("vsav");

    tiles   = fields->int_field("tiles");
    points  = fields->int_field("sc");

    name    = fields->str_field("name");
    race    = _species_by_name(fields->str_field("race"));
    job     = _job_by_name(fields->str_field("cls"));
    lvl     = fields->int_field("xl");
    race_class_name = fields->str_field("char");

    best_skill     = str_to_skill(fields->str_field("sk"));
    best_skill_lvl = fields->int_field("sklev");
    title          = fields->str_field("title");

    death_type        = _str_to_kill_method(fields->str_field("ktyp"));
    death_source_name = fields->str_field("killer");
    const vector<string> kflags =
        split_string(" ", fields->str_field("killer_flags"));
    death_source_flags = set<string>(kflags.begin(), kflags.end());

    auxkilldata       = fields->str_field("kaux");
    indirectkiller    = fields->str_field("ikiller");
    if (indirectkiller.empty())
        indirectkiller = death_source_name;
    killerpath        = fields->str_field("kpath");
    last_banisher     = fields->str_field("banisher");

    branch     = branch_by_abbrevname(fields->str_field("br"), BRANCH_DUNGEON);
    dlvl       = fields->int_field("lvl");
    absdepth   = fields->int_field("absdepth");

    map        = fields->str_field("map");
    mapdesc    = fields->str_field("mapdesc");
    killer_map = fields->str_field("killermap");

    final_hp         = fields->int_field("hp");
    final_max_hp     = fields->int_field("mhp");
    final_max_max_hp = fields->int_field("mmhp");
    final_mp          = fields->int_field("mp");
    final_max_mp      = fields->int_field("mmp");
    final_base_max_mp = fields->int_field("bmmp");

    damage        = fields->int_field("dam");
    source_damage = fields->int_field("sdam");
    turn_damage   = fields->int_field("tdam");

    str   = fields->int_field("str");
    intel = fields->int_field("int");
    dex   = fields->int_field("dex");

    ac    = fields->int_field("ac");
    ev    = fields->int_field("ev");
    sh    = fields->int_field("sh");

    god          = str_to_god(fields->str_field("god"));
    piety        = fields->int_field("piety");
    penance      = fields->int_field("pen");
    wiz_mode     = fields->int_field("wiz");
    explore_mode = fields->int_field("explore");

    birth_time = _parse_time(fields->str_field("start"));
    death_time = _parse_time(fields->str_field("end"));
    real_time  = fields->int_field("dur");
    num_turns  = fields->int_field("turn");
    num_aut    = fields->int_field("aut");

    num_diff_runes = fields->int_field("urune");
    num_runes      = fields->int_field("nrune");

    kills = fields->int_field("kills");
    maxed_skills = fields->str_field("maxskills");
    fifteen_skills = fields->str_field("fifteenskills");
    status_effects = fields->str_field("status");

    gold       = fields->int_field("gold");
    gold_found = fields->int_field("goldfound");
    gold_spent = fields->int_field("goldspent");

    zigs       = fields->int_field("zigscompleted");
    zigmax     = fields->int_field("zigdeepest");

    scrolls_used = fields->int_field("scrollsused");
    potions_used = fields->int_field("potionsused");

    fixup_char_name();
}

void scorefile_entry::set_base_xlog_fields() const
{
    if (!fields.get())
        fields.reset(new xlog_fields);

    string score_version = SCORE_VERSION;
    if (crawl_state.game_is_sprint())
    {
        /* XXX: hmmm, something better here? */
        score_version += "-sprint.1";
    }
    fields->add_field("v", "%s", Version::Short);
    fields->add_field("vlong", "%s", Version::Long);
    fields->add_field("lv", "%s", score_version.c_str());
    if (!save_rcs_version.empty())
        fields->add_field("vsavrv", "%s", save_rcs_version.c_str());
    if (!save_tag_version.empty())
        fields->add_field("vsav", "%s", save_tag_version.c_str());

#ifdef EXPERIMENTAL_BRANCH
    fields->add_field("explbr", EXPERIMENTAL_BRANCH);
#endif
    if (tiles)
        fields->add_field("tiles", "%d", tiles);
    fields->add_field("name", "%s", name.c_str());
    fields->add_field("race", "%s", _species_name(race).c_str());
    fields->add_field("cls",  "%s", _job_name(job));
    fields->add_field("char", "%s", race_class_name.c_str());
    fields->add_field("xl",    "%d", lvl);
    fields->add_field("sk",    "%s", skill_name(best_skill));
    fields->add_field("sklev", "%d", best_skill_lvl);
    fields->add_field("title", "%s", title.c_str());

    fields->add_field("place", "%s",
                      level_id(branch, dlvl).describe().c_str());

    if (!last_banisher.empty())
        fields->add_field("banisher", "%s", last_banisher.c_str());

    // Note: "br", "lvl" (and former "ltyp") are redundant with "place"
    // but may still be used by DGL logs.
    fields->add_field("br",   "%s", _short_branch_name(branch));
    fields->add_field("lvl",  "%d", dlvl);
    fields->add_field("absdepth", "%d", absdepth);

    fields->add_field("hp",   "%d", final_hp);
    fields->add_field("mhp",  "%d", final_max_hp);
    fields->add_field("mmhp", "%d", final_max_max_hp);
    fields->add_field("mp",   "%d", final_mp);
    fields->add_field("mmp",  "%d", final_max_mp);
    fields->add_field("bmmp", "%d", final_base_max_mp);
    fields->add_field("str", "%d", str);
    fields->add_field("int", "%d", intel);
    fields->add_field("dex", "%d", dex);
    fields->add_field("ac", "%d", ac);
    fields->add_field("ev", "%d", ev);
    fields->add_field("sh", "%d", sh);

    fields->add_field("god", "%s", god == GOD_NO_GOD ? "" :
                      god_name(god).c_str());

    if (wiz_mode)
        fields->add_field("wiz", "%d", wiz_mode);
    if (explore_mode)
        fields->add_field("explore", "%d", explore_mode);

    fields->add_field("start", "%s", make_date_string(birth_time).c_str());
    fields->add_field("dur",   "%d", (int)real_time);
    fields->add_field("turn",  "%d", num_turns);
    fields->add_field("aut",   "%d", num_aut);

    if (num_diff_runes)
        fields->add_field("urune", "%d", num_diff_runes);

    if (num_runes)
        fields->add_field("nrune", "%d", num_runes);

    fields->add_field("kills", "%d", kills);
    if (!maxed_skills.empty())
        fields->add_field("maxskills", "%s", maxed_skills.c_str());
    if (!fifteen_skills.empty())
        fields->add_field("fifteenskills", "%s", fifteen_skills.c_str());
    if (!status_effects.empty())
        fields->add_field("status", "%s", status_effects.c_str());

    fields->add_field("gold", "%d", gold);
    fields->add_field("goldfound", "%d", gold_found);
    fields->add_field("goldspent", "%d", gold_spent);
    if (zigs)
        fields->add_field("zigscompleted", "%d", zigs);
    if (zigmax)
        fields->add_field("zigdeepest", "%d", zigmax);
    fields->add_field("scrollsused", "%d", scrolls_used);
    fields->add_field("potionsused", "%d", potions_used);
}

void scorefile_entry::set_score_fields() const
{
    fields.reset(new xlog_fields);

    if (!fields.get())
        return;

    set_base_xlog_fields();

    fields->add_field("sc", "%d", points);
    fields->add_field("ktyp", "%s", _kill_method_name(kill_method_type(death_type)));

    fields->add_field("killer", "%s", death_source_desc().c_str());
    if (!death_source_flags.empty())
    {
        const string kflags = comma_separated_line(
            death_source_flags.begin(),
            death_source_flags.end(),
            " ", " ");
        fields->add_field("killer_flags", "%s", kflags.c_str());
    }
    fields->add_field("dam", "%d", damage);
    fields->add_field("sdam", "%d", source_damage);
    fields->add_field("tdam", "%d", turn_damage);

    fields->add_field("kaux", "%s", auxkilldata.c_str());

    if (indirectkiller != death_source_desc())
        fields->add_field("ikiller", "%s", indirectkiller.c_str());

    if (!killerpath.empty())
        fields->add_field("kpath", "%s", killerpath.c_str());

    if (piety > 0)
        fields->add_field("piety", "%d", piety);
    if (penance > 0)
        fields->add_field("pen", "%d", penance);

    fields->add_field("end", "%s", make_date_string(death_time).c_str());

    if (!map.empty())
    {
        fields->add_field("map", "%s", map.c_str());
        if (!mapdesc.empty())
            fields->add_field("mapdesc", "%s", mapdesc.c_str());
    }

    if (!killer_map.empty())
        fields->add_field("killermap", "%s", killer_map.c_str());

#ifdef DGL_EXTENDED_LOGFILES
    const string short_msg = short_kill_message();
    fields->add_field("tmsg", "%s", short_msg.c_str());
    const string long_msg = long_kill_message();
    if (long_msg != short_msg)
        fields->add_field("vmsg", "%s", long_msg.c_str());
#endif
}

string scorefile_entry::make_oneline(const string &ml) const
{
    vector<string> lines = split_string("\n", ml);
    for (string &s : lines)
    {
        if (starts_with(s, "..."))
        {
            s = s.substr(3);
            trim_string(s);
        }
    }
    return comma_separated_line(lines.begin(), lines.end(), " ", " ");
}

string scorefile_entry::long_kill_message() const
{
    string msg = death_description(DDV_LOGVERBOSE);
    msg = make_oneline(msg);
    msg[0] = tolower(msg[0]);
    trim_string(msg);
    return msg;
}

string scorefile_entry::short_kill_message() const
{
    string msg = death_description(DDV_ONELINE);
    msg = make_oneline(msg);
    msg[0] = tolower(msg[0]);
    trim_string(msg);
    return msg;
}

/**
 * Remove from a string everything up to and including a given infix.
 *
 * @param[in,out] str   The string to modify.
 * @param[in]     infix The infix to remove.
 * @post If \c infix occurred as a substring of <tt>str</tt>, \c str is updated
 *       by removing all characters up to and including the last character
 *       of the the first occurrence. Otherwise, \c str is unchanged.
 * @return \c true if \c str was modified, \c false otherwise.
 */
static bool _strip_to(string &str, const char *infix)
{
    // Don't treat stripping the empty string as a change.
    if (*infix == '\0')
        return false;

    size_t pos = str.find(infix);
    if (pos != string::npos)
    {
        str.erase(0, pos + strlen(infix));
        return true;
    }
    return false;
}

void scorefile_entry::init_death_cause(int dam, mid_t dsrc,
                                       int dtype, const char *aux,
                                       const char *dsrc_name)
{
    death_source = dsrc;
    death_type   = dtype;
    damage       = dam;

    const monster *source_monster = monster_by_mid(death_source);
    if (source_monster)
        killer_map = source_monster->originating_map();

    // Set the default aux data value...
    // If aux is passed in (ie for a trap), we'll default to that.
    if (aux == nullptr)
        auxkilldata.clear();
    else
        auxkilldata = aux;

    // for death by monster
    if ((death_type == KILLED_BY_MONSTER
            || death_type == KILLED_BY_HEADBUTT
            || death_type == KILLED_BY_BEAM
            || death_type == KILLED_BY_DISINT
            || death_type == KILLED_BY_ACID
            || death_type == KILLED_BY_DRAINING
            || death_type == KILLED_BY_BURNING
            || death_type == KILLED_BY_SPORE
            || death_type == KILLED_BY_CLOUD
            || death_type == KILLED_BY_ROTTING
            || death_type == KILLED_BY_REFLECTION
            || death_type == KILLED_BY_ROLLING
            || death_type == KILLED_BY_SPINES
            || death_type == KILLED_BY_WATER
            || death_type == KILLED_BY_BEING_THROWN
            || death_type == KILLED_BY_COLLISION)
        && monster_by_mid(death_source))
    {
        const monster* mons = monster_by_mid(death_source);
        ASSERT(mons);

        // Previously the weapon was only used for dancing weapons,
        // but now we pass it in as a string through the scorefile
        // entry to be appended in hiscores_format_single in long or
        // medium scorefile formats.
        if (death_type == KILLED_BY_MONSTER
            && mons->inv[MSLOT_WEAPON] != NON_ITEM)
        {
            // [ds] The highscore entry may be constructed while the player
            // is alive (for notes), so make sure we don't reveal info we
            // shouldn't.
            if (you.hp <= 0)
            {
                set_ident_flags(mitm[mons->inv[MSLOT_WEAPON]],
                                 ISFLAG_IDENT_MASK);
            }

            // Setting this is redundant for dancing weapons, however
            // we do care about the above indentification. -- bwr
            if (mons->type != MONS_DANCING_WEAPON)
                auxkilldata = mitm[mons->inv[MSLOT_WEAPON]].name(DESC_A);
        }

        const bool death = (you.hp <= 0 || death_type == KILLED_BY_DRAINING);

        const description_level_type desc =
            death_type == KILLED_BY_SPORE ? DESC_PLAIN : DESC_A;

        death_source_name = mons->name(desc, death);

        if (death || you.can_see(*mons))
            death_source_name = mons->full_name(desc);

        if (mons_is_player_shadow(*mons))
            death_source_name = "their own shadow"; // heh

        if (mons->mid == MID_YOU_FAULTLESS)
            death_source_name = "themself";

        if (mons->has_ench(ENCH_SHAPESHIFTER))
            death_source_name += " (shapeshifter)";
        else if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
            death_source_name += " (glowing shapeshifter)";

        if (mons->type == MONS_PANDEMONIUM_LORD)
            death_source_name += " the pandemonium lord";

        if (mons->has_ench(ENCH_PHANTOM_MIRROR))
            death_source_name += " (illusionary)";

        if (mons_is_unique(mons->type))
            death_source_flags.insert("unique");

        if (mons->props.exists("blame"))
        {
            const CrawlVector& blame = mons->props["blame"].get_vector();

            indirectkiller = blame[blame.size() - 1].get_string();
            _strip_to(indirectkiller, " by ");
            _strip_to(indirectkiller, "ed to "); // "attached to" and similar

            killerpath = "";

            for (const auto &bl : blame)
                killerpath = killerpath + ":" + _xlog_escape(bl.get_string());

            killerpath.erase(killerpath.begin());
        }
        else
        {
            indirectkiller = death_source_name;
            killerpath = "";
        }
    }
    else if (death_type == KILLED_BY_DISINT
             || death_type == KILLED_BY_CLOUD)
    {
        death_source_name = dsrc_name ? dsrc_name :
                            dsrc == MHITYOU ? "you" :
                            "";
        indirectkiller = killerpath = "";
    }
    else
    {
        if (dsrc_name)
            death_source_name = dsrc_name;
        else
            death_source_name.clear();
        indirectkiller = killerpath = "";
    }

    if (death_type == KILLED_BY_WEAKNESS
        || death_type == KILLED_BY_STUPIDITY
        || death_type == KILLED_BY_CLUMSINESS)
    {
        if (auxkilldata.empty())
            auxkilldata = "unknown source";
    }

    if (death_type == KILLED_BY_POISON)
    {
        death_source_name = you.props["poisoner"].get_string();
        auxkilldata = you.props["poison_aux"].get_string();
    }

    if (death_type == KILLED_BY_BURNING)
    {
        death_source_name = you.props["sticky_flame_source"].get_string();
        auxkilldata = you.props["sticky_flame_aux"].get_string();
    }
}

void scorefile_entry::reset()
{
    // simple init
    raw_line.clear();
    version.clear();
    save_rcs_version.clear();
    save_tag_version.clear();
    tiles                = 0;
    points               = -1;
    name.clear();
    race                 = SP_UNKNOWN;
    job                  = JOB_UNKNOWN;
    lvl                  = 0;
    race_class_name.clear();
    best_skill           = SK_NONE;
    best_skill_lvl       = 0;
    title.clear();
    death_type           = KILLED_BY_SOMETHING;
    death_source         = MID_NOBODY;
    death_source_name.clear();
    auxkilldata.clear();
    indirectkiller.clear();
    killerpath.clear();
    last_banisher.clear();
    dlvl                 = 0;
    absdepth             = 1;
    branch               = BRANCH_DUNGEON;
    map.clear();
    mapdesc.clear();
    final_hp             = -1;
    final_max_hp         = -1;
    final_max_max_hp     = -1;
    final_mp             = -1;
    final_max_mp         = -1;
    final_base_max_mp    = -1;
    str                  = -1;
    intel                = -1;
    dex                  = -1;
    ac                   = -1;
    ev                   = -1;
    sh                   = -1;
    damage               = -1;
    source_damage        = -1;
    turn_damage          = -1;
    god                  = GOD_NO_GOD;
    piety                = -1;
    penance              = -1;
    wiz_mode             = 0;
    explore_mode         = 0;
    birth_time           = 0;
    death_time           = 0;
    real_time            = -1;
    num_turns            = -1;
    num_aut              = -1;
    num_diff_runes       = 0;
    num_runes            = 0;
    kills                = 0;
    maxed_skills.clear();
    fifteen_skills.clear();
    status_effects.clear();
    gold                 = 0;
    gold_found           = 0;
    gold_spent           = 0;
    zigs                 = 0;
    zigmax               = 0;
    scrolls_used         = 0;
    potions_used         = 0;
}

static int _award_modified_experience()
{
    int xp = you.experience;
    int result = 0;

    if (xp <= 250000)
        return xp * 7 / 10;

    result += 250000 * 7 / 10;
    xp -= 250000;

    if (xp <= 750000)
    {
        result += xp * 4 / 10;
        return result;
    }

    result += 750000 * 4 / 10;
    xp -= 750000;

    if (xp <= 2000000)
    {
        result += xp * 2 / 10;
        return result;
    }

    result += 2000000 * 2 / 10;
    xp -= 2000000;

    result += xp / 10;

    return result;
}

void scorefile_entry::init(time_t dt)
{
    // Score file entry version:
    //
    // 4.0      - original versioned entry
    // 4.1      - added real_time and num_turn fields
    // 4.2      - stats and god info

    version = Version::Short;
#ifdef USE_TILE_LOCAL
    tiles   = 1;
#elif defined (USE_TILE_WEB)
    tiles   = ::tiles.is_controlled_from_web();
#else
    tiles   = 0;
#endif
    name    = you.your_name;

    save_rcs_version = crawl_state.save_rcs_version;
    if (crawl_state.minor_version > 0)
    {
        save_tag_version = make_stringf("%d.%d", TAG_MAJOR_VERSION,
                                        crawl_state.minor_version);
    }

    /*
     *  old scoring system (0.1-0.3):
     *
     *    Gold
     *    + 0.7 * Experience
     *    + (distinct Runes +2)^2 * 1000, winners with distinct runes >= 3 only
     *    + value of Inventory, for winners only
     *
     *
     *  0.4 scoring system, as suggested by Lemuel:
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
     *  current scoring system (mostly the same as above):
     *
     *    Experience terms as above
     *    + runes * (runes + 12) * 1000        (for everyone)
     *    + (250000 + 2 * (runes + 2) * 1000)  (winners only)
     *    + 250000 * 25000 * runes^2 / turns   (winners only)
     */

    // do points first.
    points = 0;
    bool base_score = true;

    dlua.pushglobal("dgn.persist.calc_score");
    lua_pushboolean(dlua, death_type == KILLED_BY_WINNING);
    if (dlua.callfn(nullptr, 1, 2))
        dlua.fnreturns(">db", &points, &base_score);

    // If calc_score didn't exist, or returned true as its second value,
    // use the default formula.
    if (base_score)
    {
        // sprint games could overflow a 32 bit value
        uint64_t pt = points + _award_modified_experience();

        num_runes      = runes_in_pack();
        num_diff_runes = num_runes;

        // There's no point in rewarding lugging artefacts. Thus, no points
        // for the value of the inventory. -- 1KB
        if (death_type == KILLED_BY_WINNING)
        {
            pt += 250000; // the Orb
            pt += num_runes * 2000 + 4000;
            pt += ((uint64_t)250000) * 25000 * num_runes * num_runes
                / (1+you.num_turns);
        }
        pt += num_runes * 10000;
        pt += num_runes * (num_runes + 2) * 1000;

        points = pt;
    }
    else
        ASSERT(crawl_state.game_is_sprint());
        // only sprint should use custom scores

    race = you.species;
    job  = you.char_class;

    race_class_name.clear();
    fixup_char_name();

    lvl            = you.experience_level;
    best_skill     = ::best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    best_skill_lvl = you.skills[ best_skill ];
    title          = player_title(false);

    // Note all skills at level 27, and also all skills at level >= 15.
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        if (you.skills[sk] == 27)
        {
            if (!maxed_skills.empty())
                maxed_skills += ",";
            maxed_skills += skill_name(sk);
        }
        if (you.skills[sk] >= 15)
        {
            if (!fifteen_skills.empty())
                fifteen_skills += ",";
            fifteen_skills += skill_name(sk);
        }
    }

    status_info inf;
    for (unsigned i = 0; i <= STATUS_LAST_STATUS; ++i)
    {
        if (fill_status_info(i, &inf) && !inf.short_text.empty())
        {
            if (!status_effects.empty())
                status_effects += ",";
            status_effects += inf.short_text;
        }
    }

    kills            = you.kills.total_kills();

    final_hp         = you.hp;
    final_max_hp     = you.hp_max;
    final_max_max_hp = get_real_hp(true, true);

    final_mp          = you.magic_points;
    final_max_mp      = you.max_magic_points;
    final_base_max_mp = get_real_mp(false);

    source_damage    = you.source_damage;
    turn_damage      = you.turn_damage;

    // Use possibly negative stat values.
    str   = you.stat(STAT_STR, false);
    intel = you.stat(STAT_INT, false);
    dex   = you.stat(STAT_DEX, false);

    ac    = you.armour_class();
    ev    = you.evasion();
    sh    = player_displayed_shield_class();

    god = you.religion;
    if (!you_worship(GOD_NO_GOD))
    {
        piety   = you.piety;
        penance = you.penance[you.religion];
    }

    branch     = you.where_are_you;  // no adjustments necessary.
    dlvl       = you.depth;

    absdepth   = env.absdepth0 + 1;  // 1-based absolute depth.

    last_banisher = you.banished_by;

    if (const vault_placement *vp = dgn_vault_at(you.pos()))
    {
        map     = vp->map_name_at(you.pos());
        mapdesc = vp->map.description;
    }

    birth_time = you.birth_time;     // start time of game
    death_time = (dt != 0 ? dt : time(nullptr)); // end time of game

    handle_real_time(chrono::system_clock::from_time_t(death_time));
    real_time = you.real_time();

    num_turns = you.num_turns;
    num_aut = you.elapsed_time;

    gold       = you.gold;
    gold_found = you.attribute[ATTR_GOLD_FOUND];
    gold_spent = you.attribute[ATTR_PURCHASES];

    zigs       = you.zigs_completed;
    zigmax     = you.zig_max;

    scrolls_used = 0;
    pair<caction_type, int> p(CACT_USE, caction_compound(OBJ_SCROLLS));

    const int maxlev = min<int>(you.max_level, 27);
    if (you.action_count.count(p))
        for (int i = 0; i < maxlev; i++)
            scrolls_used += you.action_count[p][i];

    potions_used = 0;
    p = make_pair(CACT_USE, caction_compound(OBJ_POTIONS));
    if (you.action_count.count(p))
        for (int i = 0; i < maxlev; i++)
            potions_used += you.action_count[p][i];

    wiz_mode = (you.wizard ? 1 : 0);
    explore_mode = (you.explore ? 1 : 0);
}

string scorefile_entry::hiscore_line(death_desc_verbosity verbosity) const
{
    string line = character_description(verbosity);
    line += death_description(verbosity);
    line += death_place(verbosity);
    line += game_time(verbosity);

    return line;
}

string scorefile_entry::game_time(death_desc_verbosity verbosity) const
{
    string line;

    if (verbosity == DDV_VERBOSE)
    {
        line += make_stringf("The game lasted %s (%d turns).",
                             make_time_string(real_time).c_str(), num_turns);

        line += _hiscore_newline_string();
    }

    return line;
}

const char *scorefile_entry::damage_verb() const
{
    // GDL: here's an example of using final_hp. Verbiage could be better.
    // bwr: changed "blasted" since this is for melee
    return (final_hp > -6)  ? "Slain"   :
           (final_hp > -14) ? "Mangled" :
           (final_hp > -22) ? "Demolished"
                            : "Annihilated";
}

string scorefile_entry::death_source_desc() const
{
    return death_source_name;
}

string scorefile_entry::damage_string(bool terse) const
{
    return make_stringf("(%d%s)", damage,
                        terse? "" : " damage");
}

string scorefile_entry::strip_article_a(const string &s) const
{
    if (starts_with(s, "a "))
        return s.substr(2);
    else if (starts_with(s, "an "))
        return s.substr(3);
    return s;
}

string scorefile_entry::terse_missile_name() const
{
    const string pre_post[][2] =
    {
        { "Shot with ", " by " },
        { "Hit by ",     " thrown by " }
    };
    const string &aux = auxkilldata;
    string missile;

    for (const string (&affixes)[2] : pre_post)
    {
        if (!starts_with(aux, affixes[0]))
            continue;

        string::size_type end = aux.rfind(affixes[1]);
        if (end == string::npos)
            continue;

        int istart = affixes[0].length();
        int nchars = end - istart;
        missile = aux.substr(istart, nchars);

        // Was this prefixed by "a" or "an"?
        // (This should only ever not be the case with Robin and Ijyb.)
        missile = strip_article_a(missile);
    }
    return missile;
}

string scorefile_entry::terse_missile_cause() const
{
    const string &aux = auxkilldata;

    string monster_prefix = " by ";
    // We're looking for Shot with a%s %s by %s/ Hit by a%s %s thrown by %s
    string::size_type by = aux.rfind(monster_prefix);
    if (by == string::npos)
        return "???";

    string mcause = aux.substr(by + monster_prefix.length());
    mcause = strip_article_a(mcause);

    string missile = terse_missile_name();

    if (!missile.empty())
        mcause += "/" + missile;

    return mcause;
}

string scorefile_entry::terse_beam_cause() const
{
    string cause = auxkilldata;
    if (starts_with(cause, "by ") || starts_with(cause, "By "))
        cause = cause.substr(3);
    return cause;
}

string scorefile_entry::terse_wild_magic() const
{
    return terse_beam_cause();
}

void scorefile_entry::fixup_char_name()
{
    if (race_class_name.empty())
    {
        race_class_name = make_stringf("%s%s",
                                       _species_abbrev(race),
                                       _job_abbrev(job));
    }
}

string scorefile_entry::single_cdesc() const
{
    string scname;
    scname = chop_string(name, 10);

    return make_stringf("%8d %s %s-%02d%s", points, scname.c_str(),
                        race_class_name.c_str(), lvl,
                        (wiz_mode == 1) ? "W" : (explore_mode == 1) ? "E" : "");
}

static string _append_sentence_delimiter(const string &sentence,
                                         const string &delimiter)
{
    if (sentence.empty())
        return sentence;

    const char lastch = sentence[sentence.length() - 1];
    if (lastch == '!' || lastch == '.')
        return sentence;

    return sentence + delimiter;
}

string
scorefile_entry::character_description(death_desc_verbosity verbosity) const
{
    bool single  = verbosity == DDV_TERSE || verbosity == DDV_ONELINE;

    if (single)
        return single_cdesc();

    bool verbose = verbosity == DDV_VERBOSE;

    string desc;
    // Please excuse the following bit of mess in the name of flavour ;)
    if (verbose)
    {
        desc = make_stringf("%8d %s the %s (level %d",
                  points, name.c_str(), title.c_str(), lvl);
    }
    else
    {
        desc = make_stringf("%8d %s the %s %s (level %d",
                  points, name.c_str(),
                  _species_name(race).c_str(),
                  _job_name(job), lvl);
    }

    if (final_max_max_hp > 0)  // as the other two may be negative
    {
        desc += make_stringf(", %d/%d", final_hp, final_max_hp);

        if (final_max_hp < final_max_max_hp)
            desc += make_stringf(" (%d)", final_max_max_hp);

        desc += " HPs";
    }

    desc += wiz_mode ? ") *WIZ*" : explore_mode ? ") *EXPLORE*" : ")";
    desc += _hiscore_newline_string();

    if (verbose)
    {
        string srace = _species_name(race);
        desc += make_stringf("Began as a%s %s %s",
                 is_vowel(srace[0]) ? "n" : "",
                 srace.c_str(),
                 _job_name(job));

        ASSERT(birth_time);
        desc += " on ";
        desc += _hiscore_date_string(birth_time);

        desc = _append_sentence_delimiter(desc, ".");
        desc += _hiscore_newline_string();

        if (race != SP_DEMIGOD && god != GOD_NO_GOD)
        {
            if (god == GOD_XOM)
            {
                desc + make_stringf("Was a %sPlaything of Xom.",
                                    (lvl >= 20) ? "Favourite " : "");

                desc += _hiscore_newline_string();
            }
            else
            {
                // Not exactly the same as the religion screen, but
                // good enough to fill this slot for now.
                desc += make_stringf("Was %s of %s%s",
                             (piety >= piety_breakpoint(5)) ? "the Champion" :
                             (piety >= piety_breakpoint(4)) ? "a High Priest" :
                             (piety >= piety_breakpoint(3)) ? "an Elder" :
                             (piety >= piety_breakpoint(2)) ? "a Priest" :
                             (piety >= piety_breakpoint(1)) ? "a Believer" :
                             (piety >= piety_breakpoint(0)) ? "a Follower"
                                                            : "an Initiate",
                          god_name(god).c_str(),
                             (penance > 0) ? " (penitent)." : ".");

                desc += _hiscore_newline_string();
            }
        }
    }

    return desc;
}

string scorefile_entry::death_place(death_desc_verbosity verbosity) const
{
    bool verbose = (verbosity == DDV_VERBOSE);
    string place;

    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
        return "";

    if (verbosity == DDV_ONELINE || verbosity == DDV_TERSE)
        return " (" + level_id(branch, dlvl).describe() + ")";

    if (verbose && death_type != KILLED_BY_QUITTING && death_type != KILLED_BY_WIZMODE)
        place += "...";

    // where did we die?
    place += " " + prep_branch_level_name(level_id(branch, dlvl));

    if (!mapdesc.empty())
        place += make_stringf(" (%s)", mapdesc.c_str());

    if (verbose && death_time
        && !_hiscore_same_day(birth_time, death_time))
    {
        place += " on ";
        place += _hiscore_date_string(death_time);
    }

    place = _append_sentence_delimiter(place, ".");
    place += _hiscore_newline_string();

    return place;
}

/**
 * Describes the cause of the player's death.
 *
 * @param verbosity     The verbosity of the description.
 * @return              A description of the cause of death.
 */
string scorefile_entry::death_description(death_desc_verbosity verbosity) const
{
    bool needs_beam_cause_line = false;
    bool needs_called_by_monster_line = false;
    bool needs_damage = false;

    const bool terse   = (verbosity == DDV_TERSE);
    const bool semiverbose = (verbosity == DDV_LOGVERBOSE);
    const bool verbose = (verbosity == DDV_VERBOSE || semiverbose);
    const bool oneline = (verbosity == DDV_ONELINE);

    string desc;

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

    case KILLED_BY_HEADBUTT:
        if (terse)
            desc += apostrophise(death_source_desc()) + " headbutt";
        else
            desc += "Headbutted by " + death_source_desc();
        needs_damage = true;
        break;

    case KILLED_BY_ROLLING:
        if (terse)
            desc += "squashed by " + death_source_desc();
        else
            desc += "Rolled over by " + death_source_desc();
        needs_damage = true;
        break;

    case KILLED_BY_SPINES:
        if (terse)
            desc += apostrophise(death_source_desc()) + " spines";
        else
            desc += "Impaled on " + apostrophise(death_source_desc()) + " spines" ;
        needs_damage = true;
        break;

    case KILLED_BY_POISON:
        if (death_source_name.empty() || terse)
        {
            if (!terse)
                desc += "Succumbed to poison";
            else if (!death_source_name.empty())
                desc += "poisoned by " + death_source_name;
            else
                desc += "poison";
            if (!auxkilldata.empty())
                desc += " (" + auxkilldata + ")";
        }
        else if (auxkilldata.empty()
                 && death_source_name.find("poison") != string::npos)
        {
            desc += "Succumbed to " + death_source_name;
        }
        else
        {
            desc += "Succumbed to " + ((death_source_name == "you")
                      ? "their own" : apostrophise(death_source_name)) + " "
                    + (auxkilldata.empty()? "poison" : auxkilldata);
        }
        break;

    case KILLED_BY_CLOUD:
        ASSERT(!auxkilldata.empty()); // there are no nameless clouds
        if (terse)
            if (death_source_name.empty())
                desc += "cloud of " + auxkilldata;
            else
                desc += "cloud of " +auxkilldata + " [" +
                        death_source_name == "you" ? "self" : death_source_name
                        + "]";
        else
        {
            desc += make_stringf("Engulfed by %s%s %s",
                death_source_name.empty() ? "a" :
                  death_source_name == "you" ? "their own" :
                  apostrophise(death_source_name).c_str(),
                death_source_name.empty() ? " cloud of" : "",
                auxkilldata.c_str());
        }
        needs_damage = true;
        break;

    case KILLED_BY_BEAM:
        if (oneline || semiverbose)
        {
            // keeping this short to leave room for the deep elf spellcasters:
            desc += make_stringf("%s by ",
                      _range_type_verb(auxkilldata.c_str()));
            desc += (death_source_name == "you") ? "themself"
                                                 : death_source_desc();

            if (semiverbose)
            {
                string beam = terse_missile_name();
                if (beam.empty())
                    beam = terse_beam_cause();
                trim_string(beam);
                if (!beam.empty())
                    desc += make_stringf(" (%s)", beam.c_str());
            }
        }
        else if (isupper(auxkilldata[0]))  // already made (ie shot arrows)
        {
            // If terse we have to parse the information from the string.
            // Darn it to heck.
            desc += terse? terse_missile_cause() : auxkilldata;
            needs_damage = true;
        }
        else if (verbose && starts_with(auxkilldata, "by "))
        {
            // "by" is used for priest attacks where the effect is indirect
            // in verbose format we have another line for the monster
            if (death_source_name == "you")
            {
                needs_damage = true;
                desc += make_stringf("Killed by their own %s",
                         auxkilldata.substr(3).c_str());
            }
            else
            {
                needs_called_by_monster_line = true;
                desc += make_stringf("Killed %s",
                          auxkilldata.c_str());
            }
        }
        else
        {
            // Note: This is also used for the "by" cases in non-verbose
            //       mode since listing the monster is more imporatant.
            if (semiverbose)
                desc += "Killed by ";
            else if (!terse)
                desc += "Killed from afar by ";

            if (death_source_name == "you")
                desc += "themself";
            else
                desc += death_source_desc();

            if (!auxkilldata.empty())
                needs_beam_cause_line = true;

            needs_damage = true;
        }
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
        if (you.undead_state())
        {
            if (terse)
                desc = "fell apart";
            else if (race == SP_MUMMY)
                desc = "Soaked and fell apart";
            else
                desc = "Sank and fell apart";
        }
        else
        {
            if (!death_source_name.empty())
            {
                desc += terse? "drowned by " : "Drowned by ";
                desc += death_source_name;
                needs_damage = true;
            }
            else
                desc += terse? "drowned" : "Drowned";
        }
        break;

    case KILLED_BY_STUPIDITY:
        if (terse)
            desc += "stupidity";
        else if (race >= 0 && // not a removed race
                 species_is_unbreathing(static_cast<species_type>(race)))
        {
            desc += "Forgot to exist";
        }
        else
            desc += "Forgot to breathe";
        break;

    case KILLED_BY_WEAKNESS:
        desc += terse? "collapsed" : "Collapsed under their own weight";
        break;

    case KILLED_BY_CLUMSINESS:
        desc += terse? "clumsiness" : "Slipped on a banana peel";
        break;

    case KILLED_BY_TRAP:
        if (terse)
            desc += auxkilldata.c_str();
        else
        {
            desc += make_stringf("Killed by triggering %s",
                                 auxkilldata.c_str());
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
            else if (species_is_undead(static_cast<species_type>(race)))
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

    case KILLED_BY_WIZMODE:
        desc += terse? "wizmode" : "Entered wizard mode";
        break;

    case KILLED_BY_DRAINING:
        if (terse)
            desc += "drained";
        else
        {
            desc += "Drained of all life";
            if (!death_source_desc().empty())
            {
                desc += " by " + death_source_desc();

                if (!auxkilldata.empty())
                    needs_beam_cause_line = true;
            }
            else if (!auxkilldata.empty())
                desc += " by " + auxkilldata;
        }
        break;

    case KILLED_BY_STARVATION:
        desc += terse? "starvation" : "Starved to death";
        break;

    case KILLED_BY_FREEZING:    // refrigeration spell
        desc += terse? "frozen" : "Froze to death";
        needs_damage = true;
        break;

    case KILLED_BY_BURNING:     // sticky flame
        if (terse)
            desc += "burnt";
        else if (!death_source_desc().empty())
        {
            desc += "Incinerated by " + death_source_desc();

            if (!auxkilldata.empty())
                needs_beam_cause_line = true;
        }
        else
            desc += "Burnt to a crisp";

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
                desc += make_stringf("Killed %s%s",
                          (auxkilldata.find("by ") != 0) ? "by " : "",
                          auxkilldata.c_str());
            }
        }

        needs_damage = true;
        break;

    case KILLED_BY_XOM:
        if (terse)
            desc += "xom";
        else
            desc += auxkilldata.empty() ? "Killed for Xom's enjoyment"
                                        : "Killed by " + auxkilldata;
        needs_damage = true;
        break;

    case KILLED_BY_ROTTING:
        desc += terse? "rotting" : "Rotted away";
        if (!auxkilldata.empty())
            desc += " (" + auxkilldata + ")";
        if (!death_source_desc().empty())
            desc += " (" + death_source_desc() + ")";
        break;

    case KILLED_BY_TARGETING:
        if (terse)
            desc += "shot self";
        else
        {
            desc += "Killed themself with ";
            if (auxkilldata.empty())
                desc += "bad targeting";
            else
                desc += "a badly aimed " + auxkilldata;
        }
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
            desc += "Killed themself with a bounced ";
            if (auxkilldata.empty())
                desc += "beam";
            else
                desc += auxkilldata;
        }
        needs_damage = true;
        break;

    case KILLED_BY_SELF_AIMED:
        if (terse)
            desc += "suicidal targeting";
        else
        {
            desc += "Shot themself with ";
            if (auxkilldata.empty())
                desc += "a beam";
            else
                desc += article_a(auxkilldata, true);
        }
        needs_damage = true;
        break;

    case KILLED_BY_SPORE:
        if (terse)
        {
            if (death_source_name.empty())
                desc += "spore";
            else
                desc += death_source_name;
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
        desc += terse? "smitten by Shining One" : "Smitten by the Shining One";
        needs_damage = true;
        break;

    case KILLED_BY_BEOGH_SMITING:
        desc += terse? "smitten by Beogh" : "Smitten by Beogh";
        needs_damage = true;
        break;

    case KILLED_BY_PETRIFICATION:
        desc += terse? "petrified" : "Turned to stone";
        break;

    case KILLED_BY_SOMETHING:
        if (!auxkilldata.empty())
            desc += (terse ? "" : "Killed by ") + auxkilldata;
        else
            desc += terse? "died" : "Died";
        needs_damage = true;
        break;

    case KILLED_BY_FALLING_DOWN_STAIRS:
        desc += terse? "fell downstairs" : "Fell down a flight of stairs";
        needs_damage = true;
        break;

    case KILLED_BY_FALLING_THROUGH_GATE:
        desc += terse? "fell through a gate" : "Fell down through a gate";
        needs_damage = true;
        break;

    case KILLED_BY_ACID:
        if (terse)
            desc += "acid";
        else if (!death_source_desc().empty())
        {
            desc += "Splashed by "
                    + apostrophise(death_source_desc())
                    + " acid";
        }
        else
            desc += "Splashed with acid";
        needs_damage = true;
        break;

    case KILLED_BY_CURARE:
        desc += terse? "asphyx" : "Asphyxiated";
        break;

    case KILLED_BY_DIVINE_WRATH:
        if (terse)
            desc += "divine wrath";
        else
        {
            desc += "Killed by ";
            if (auxkilldata.empty())
                desc += "divine wrath";
            else
            {
                // Lugonu's touch or "the <retribution> of <deity>";
                // otherwise it's a beam
                if (!isupper(auxkilldata[0])
                    && !starts_with(auxkilldata, "the "))
                {
                    desc += is_vowel(auxkilldata[0]) ? "an " : "a ";
                }

                desc += auxkilldata;
            }
        }
        needs_damage = true;
        if (!death_source_name.empty())
            needs_called_by_monster_line = true;
        break;

    case KILLED_BY_DISINT:
        if (terse)
            desc += "disintegration";
        else
        {
            if (death_source_name == "you")
                desc += "Blew themself up";
            else
                desc += "Blown up by " + death_source_desc();
            needs_beam_cause_line = true;
        }

        needs_damage = true;
        break;

    case KILLED_BY_MIRROR_DAMAGE:
        desc += terse ? "mirror damage" : "Killed by mirror damage";
        needs_damage = true;
        break;

    case KILLED_BY_FRAILTY:
        desc += terse ? "frailty" : "Became unviable by " + auxkilldata;
        break;

    case KILLED_BY_BARBS:
        desc += terse ? "barbs" : "Succumbed to barbed spike wounds";
        break;

    case KILLED_BY_BEING_THROWN:
        if (terse)
            desc += apostrophise(death_source_desc()) + " throw";
        else
            desc += "Thrown by " + death_source_desc();
        needs_damage = true;
        break;

    case KILLED_BY_COLLISION:
        if (terse)
            desc += auxkilldata + " collision";
        else
        {
            desc += "Collided with " + auxkilldata;
            needs_called_by_monster_line = true;
        }
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
        if (terse || oneline)
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

        if (!killerpath.empty())
            desc += "[" + indirectkiller + "]";

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

                desc += make_stringf("... %s %d rune%s",
                         (death_type == KILLED_BY_WINNING) ? "and" : "with",
                          num_runes, (num_runes > 1) ? "s" : "");

                if (!semiverbose
                    && death_time > 0
                    && !_hiscore_same_day(birth_time, death_time))
                {
                    desc += " on ";
                    desc += _hiscore_date_string(death_time);
                }

                desc = _append_sentence_delimiter(desc, "!");
                desc += _hiscore_newline_string();
            }
            else
                desc = _append_sentence_delimiter(desc, ".");
        }
        else if (death_type != KILLED_BY_QUITTING
                 && death_type != KILLED_BY_WIZMODE)
        {
            desc += _hiscore_newline_string();

            if (death_type == KILLED_BY_MONSTER && !auxkilldata.empty())
            {
                if (!semiverbose)
                {
                    desc += make_stringf("... wielding %s",
                             auxkilldata.c_str());
                    needs_damage = true;
                    desc += _hiscore_newline_string();
                }
                else
                    desc += make_stringf(" (%s)", auxkilldata.c_str());
            }
            else if (needs_beam_cause_line)
            {
                if (!semiverbose)
                {
                    desc += auxkilldata == "damnation" ? "... with " :
                            (is_vowel(auxkilldata[0])) ? "... with an "
                                                       : "... with a ";
                    desc += auxkilldata;
                    desc += _hiscore_newline_string();
                    needs_damage = true;
                }
                else if (death_type == KILLED_BY_DRAINING
                         || death_type == KILLED_BY_BURNING)
                {
                    desc += make_stringf(" (%s)", auxkilldata.c_str());
                }
            }
            else if (needs_called_by_monster_line)
            {
                desc += make_stringf("... %s by %s",
                         death_type == KILLED_BY_COLLISION ? "caused" :
                         auxkilldata == "by angry trees"   ? "awakened"
                                                           : "invoked",
                         death_source_name.c_str());
                desc += _hiscore_newline_string();
                needs_damage = true;
            }

            if (!killerpath.empty())
            {
                vector<string> summoners = _xlog_split_fields(killerpath);

                for (const auto &sumname : summoners)
                {
                    if (!semiverbose)
                    {
                        desc += "... " + sumname;
                        desc += _hiscore_newline_string();
                    }
                    else
                        desc += " (" + sumname;
                }

                if (semiverbose)
                    desc += string(summoners.size(), ')');
            }

            if (!semiverbose)
            {
                if (needs_damage && !done_damage && damage > 0)
                    desc += " " + damage_string();

                if (needs_damage && !done_damage)
                    desc += _hiscore_newline_string();

                if (you.duration[DUR_PARALYSIS])
                {
                    desc += "... while paralysed";
                    if (you.props.exists("paralysed_by"))
                        desc += " by " + you.props["paralysed_by"].get_string();
                    desc += _hiscore_newline_string();
                }

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
                desc = _append_sentence_delimiter(desc,
                                                  num_runes > 0? "!" : ".");
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

    return desc;
}

//////////////////////////////////////////////////////////////////////////////
// xlog_fields

xlog_fields::xlog_fields() : fields(), fieldmap()
{
}

xlog_fields::xlog_fields(const string &line) : fields(), fieldmap()
{
    init(line);
}

// xlogfile escape: s/:/::/g
static string _xlog_escape(const string &s)
{
    return replace_all(s, ":", "::");
}

// xlogfile unescape: s/::/:/g
static string _xlog_unescape(const string &s)
{
    return replace_all(s, "::", ":");
}

static string::size_type _xlog_next_separator(const string &s,
                                              string::size_type start)
{
    string::size_type p = s.find(':', start);
    if (p != string::npos && p < s.length() - 1 && s[p + 1] == ':')
        return _xlog_next_separator(s, p + 2);

    return p;
}

static vector<string> _xlog_split_fields(const string &s)
{
    string::size_type start = 0, end = 0;
    vector<string> fs;

    for (; (end = _xlog_next_separator(s, start)) != string::npos;
          start = end + 1)
    {
        fs.push_back(s.substr(start, end - start));
    }

    if (start < s.length())
        fs.push_back(s.substr(start));

    return fs;
}

void xlog_fields::init(const string &line)
{
    for (const string &field : _xlog_split_fields(line))
    {
        string::size_type st = field.find('=');
        if (st == string::npos)
            continue;

        fields.emplace_back(field.substr(0, st),
                            _xlog_unescape(field.substr(st + 1)));
    }

    map_fields();
}

void xlog_fields::add_field(const string &key, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    string buf = vmake_stringf(format, args);
    va_end(args);

    fields.emplace_back(key, buf);
    fieldmap[key] = buf;
}

string xlog_fields::str_field(const string &s) const
{
    return lookup(fieldmap, s, "");
}

int xlog_fields::int_field(const string &s) const
{
    string field = str_field(s);
    return atoi(field.c_str());
}

void xlog_fields::map_fields() const
{
    fieldmap.clear();
    for (const pair<string, string> &f : fields)
        fieldmap[f.first] = f.second;
}

string xlog_fields::xlog_line() const
{
    string line;
    for (const pair<string, string> &f : fields)
    {
        // Don't write empty fields.
        if (f.second.empty())
            continue;

        if (!line.empty())
            line += ":";

        line += f.first;
        line += "=";
        line += _xlog_escape(f.second);
    }

    return line;
}

///////////////////////////////////////////////////////////////////////////////
// Milestones

/**
 * @brief Record the player reaching a milestone, if ::DGL_MILESTONES is defined.
 * @callergraph
 */
void mark_milestone(const string &type, const string &milestone,
                    const string &origin_level, time_t milestone_time)
{
#ifdef DGL_MILESTONES
    static string lasttype, lastmilestone;
    static long lastturn = -1;

    if (crawl_state.game_is_arena()
        || !crawl_state.need_save
        // Suppress duplicate milestones on the same turn.
        || (lastturn == you.num_turns
            && lasttype == type
            && lastmilestone == milestone)
#ifndef SCORE_WIZARD_CHARACTERS
        // Don't mark normal milestones in wizmode or explore mode
        || (type != "crash" && (you.wizard || you.explore))
#endif
        )
    {
        return;
    }

    lasttype      = type;
    lastmilestone = milestone;
    lastturn      = you.num_turns;

    const string milestone_file =
        (Options.save_dir + "milestones" + crawl_state.game_type_qualifier());
    const scorefile_entry se(0, MID_NOBODY, KILL_MISC, nullptr);
    se.set_base_xlog_fields();
    xlog_fields xl = se.get_fields();
    if (!origin_level.empty())
    {
        xl.add_field("oplace", "%s",
                     ((origin_level == "parent") ?
                      current_level_parent().describe() :
                      origin_level).c_str());
    }
    xl.add_field("time", "%s",
                 make_date_string(
                     milestone_time ? milestone_time
                                    : se.get_death_time()).c_str());
    xl.add_field("type", "%s", type.c_str());
    xl.add_field("milestone", "%s", milestone.c_str());
    const string xlog_line = xl.xlog_line();
    if (FILE *fp = lk_open("a", milestone_file))
    {
        fprintf(fp, "%s\n", xlog_line.c_str());
        lk_close(fp, milestone_file);
    }
#endif // DGL_MILESTONES
}

#ifdef DGL_WHEREIS
string xlog_status_line()
{
    const scorefile_entry se(0, MID_NOBODY, KILL_MISC, nullptr);
    se.set_base_xlog_fields();
    xlog_fields xl = se.get_fields();
    xl.add_field("time", "%s", make_date_string(time(nullptr)).c_str());
    return xl.xlog_line();
}
#endif // DGL_WHEREIS

/**
 * @file
 * @brief Scorefile manipulation functions
**/

#ifndef HISCORES_H
#define HISCORES_H

class scorefile_entry;

int hiscores_new_entry(const scorefile_entry &se);

void logfile_new_entry(const scorefile_entry &se);

void hiscores_print_list(int display_count = -1, int format = SCORE_TERSE,
                         int newest_entry = -1);
void hiscores_print_all(int display_count = -1, int format = SCORE_TERSE);
void show_hiscore_table();

string hiscores_format_single(const scorefile_entry &se);
string hiscores_format_single_long(const scorefile_entry &se,
                                   bool verbose = false);

void mark_milestone(const string &type, const string &milestone,
                    const string &origin_level = "", time_t t = 0);

#ifdef DGL_WHEREIS
string xlog_status_line();
#endif

class xlog_fields
{
public:
    xlog_fields();
    xlog_fields(const string &line);

    void init(const string &line);
    string xlog_line() const;

    void add_field(const string &key, PRINTF(2, ));

    string str_field(const string &) const;
    int int_field(const string &) const;

private:
    void map_fields() const;

private:
    typedef vector< pair<string, string> > xl_fields;
    typedef map<string, string> xl_map;

    xl_fields fields;
    mutable xl_map fieldmap;
};

class scorefile_entry
{
private:
    string      raw_line;

    string      version;
    string      save_rcs_version;
    string      save_tag_version;
    uint8_t     tiles;
    int         points;
    string      name;
    int         race;               // species_type + legacy values
    int         job;                // job_type + legacy values
    string      race_class_name;    // overrides race & cls if non-empty.
    uint8_t     lvl;                // player level.
    skill_type  best_skill;         // best skill #
    uint8_t     best_skill_lvl;     // best skill level
    string      title;              // title
    int         death_type;
    mid_t       death_source;       // killer (maybe be MID_NOBODY)
    string      death_source_name;  // overrides death_source
    set<string> death_source_flags; // misc flags about killer
    string      auxkilldata;        // weapon wielded, spell cast, etc
    string      indirectkiller;     // the effect or real monster that summoned
    string      killerpath;         // colon-separated intermediate killers
    string      last_banisher;      // the name of the last thing that banished
                                    // the player
    uint8_t     dlvl;               // dungeon level (relative)
    short       absdepth;           // 1-based absolute depth
    branch_type branch;             // dungeon branch
    string      map;                // the vault (if any) the player is in
    string      mapdesc;            // DESC: of the vault the player is in.
    string      killer_map;         // the vault (if any) that placed the killer
    int         final_hp;           // actual current HPs (probably <= 0)
    int         final_max_hp;       // net HPs after rot
    int         final_max_max_hp;   // gross HPs before rot
    int         final_mp;           // actual current MP
    int         final_max_mp;       // max MP
    int         final_base_max_mp;  // max MP ignoring equipped items
    int         damage;             // damage of final attack
    int         source_damage;      // total damage done by death_source
    int         turn_damage;        // total damage done last turn
    int         str;                // final str (useful for nickname)
    int         intel;              // final int
    int         dex;                // final dex (useful for nickname)
    int         ac;                 // AC
    int         ev;                 // EV
    int         sh;                 // SH
    god_type    god;                // god
    int         piety;              // piety
    int         penance;            // penance
    uint8_t     wiz_mode;           // character used wiz mode
    uint8_t     explore_mode;       // character used explore mode
    time_t      birth_time;         // start time of character
    time_t      death_time;         // end time of character
    time_t      real_time;          // real playing time in seconds
    int         num_turns;          // number of turns taken
    int         num_aut;            // quantity of aut taken
    int         num_diff_runes;     // number of rune types in inventory
    int         num_runes;          // total number of runes in inventory
    int         kills;              // number of monsters killed
    string      maxed_skills;       // comma-separated list of skills
                                    // at level 27
    string      fifteen_skills;     // comma-separated list of skills
                                    // at level >= 15
    string      status_effects;     // comma-separated list of status effects
    int         gold;               // Remaining gold.
    int         gold_found;         // Gold found.
    int         gold_spent;         // Gold spent shopping.

    int         zigs;               // Ziggurats completed.
    int         zigmax;             // Max level reached in a ziggurat.

    int         scrolls_used;       // Number of scrolls used.
    int         potions_used;       // Number of potions used.

    mutable unique_ptr<xlog_fields> fields;

public:
    scorefile_entry();
    scorefile_entry(int damage, mid_t death_source, int death_type,
                    const char *aux, bool death_cause_only = false,
                    const char *death_source_name = nullptr,
                    time_t death_time = 0);
    scorefile_entry(const scorefile_entry &se);

    scorefile_entry &operator = (const scorefile_entry &other);

    void init_death_cause(int damage, mid_t death_source, int death_type,
                          const char *aux, const char *death_source_name);
    void init(time_t death_time = 0);
    void reset();

    enum death_desc_verbosity
    {
        DDV_TERSE,
        DDV_ONELINE,
        DDV_NORMAL,
        DDV_VERBOSE,
        DDV_LOGVERBOSE     // Semi-verbose for logging purposes
    };

    string raw_string() const;
    bool parse(const string &line);

    string hiscore_line(death_desc_verbosity verbosity) const;

    string character_description(death_desc_verbosity) const;
    // Full description of death: Killed by an xyz wielding foo
    string death_description(death_desc_verbosity) const;
    string death_place(death_desc_verbosity) const;
    string game_time(death_desc_verbosity) const;

    string get_name() const       { return name; }
    int    get_score() const      { return points; }
    int    get_death_type() const { return death_type; }
    time_t get_death_time() const { return death_time; }
    actor* killer() const; // Obviously does not work across games.
    xlog_fields get_fields() const;

    void set_base_xlog_fields() const;
    string short_kill_message() const;
    string long_kill_message() const;

private:
    string single_cdesc() const;
    string strip_article_a(const string &s) const;
    string terse_missile_cause() const;
    string terse_missile_name() const;
    string terse_beam_cause() const;
    string terse_wild_magic() const;
    const char *damage_verb() const;
    string death_source_desc() const;
    string damage_string(bool terse = false) const;

    bool parse_scoreline(const string &line);

    void init_with_fields();
    void add_auxkill_field() const;
    void set_score_fields() const;
    void fixup_char_name();

    string make_oneline(const string &s) const;

    void init_from(const scorefile_entry &other);
};

#endif  // HISCORES_H

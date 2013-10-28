/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#ifndef DESCRIBE_H
#define DESCRIBE_H

#include <string>
#include <sstream>
#include "externs.h"
#include "enum.h"
#include "mon-util.h"

struct monster_info;

// If you add any more description types, remember to also
// change item_description in externs.h
enum item_description_type
{
    IDESC_WANDS = 0,
    IDESC_POTIONS,
    IDESC_SCROLLS,                      // special field (like the others)
    IDESC_RINGS,
    IDESC_SCROLLS_II,
    IDESC_STAVES,
    NUM_IDESC
};

enum book_mem_or_forget
{
    BOOK_MEM,
    BOOK_FORGET,
    BOOK_NEITHER
};

struct describe_info
{
    ostringstream body;
    string title;
    string prefix;
    string suffix;
    string footer;
    string quote;
};

void append_spells(string &desc, const item_def &item);

bool is_dumpable_artefact(const item_def &item, bool verbose);

string get_item_description(const item_def &item, bool verbose,
                            bool dump = false, bool noquote = false);

string god_title(god_type which_god, species_type which_species, int piety);
void describe_god(god_type which_god, bool give_title);

void describe_feature_wide(const coord_def& pos, bool show_quote = false);
void get_feature_desc(const coord_def &gc, describe_info &inf);

bool describe_item(item_def &item, bool allow_inscribe = false,
                   bool shopping = false);
void get_item_desc(const item_def &item, describe_info &inf);
void inscribe_item(item_def &item, bool msgwin);

void append_weapon_stats(string &description, const item_def &item);
void append_armour_stats(string &description, const item_def &item);
void append_missile_info(string &description);

int describe_monsters(const monster_info &mi, bool force_seen = false,
                      const string &footer = "");

void get_monster_db_desc(const monster_info &mi, describe_info &inf,
                         bool &has_stat_desc, bool force_seen = false);

void get_spell_desc(const spell_type spell, describe_info &inf);
void describe_spell(spell_type spelled, const item_def* item = NULL);

string short_ghost_description(const monster *mon, bool abbrev = false);
string get_ghost_description(const monster_info &mi, bool concise = false);

string get_skill_description(skill_type skill, bool need_title = false);

void describe_skill(skill_type skill);

#ifdef USE_TILE
string get_command_description(const command_type cmd, bool terse);
#endif

void print_description(const string &desc);
void print_description(const describe_info &inf);

string artefact_inscription(const item_def& item);
void add_inscription(item_def &item, string inscrip);

string trap_name(trap_type trap);
int str_to_trap(const string &s);

int count_desc_lines(const string& _desc, const int width);

class alt_desc_proc
{
public:
    alt_desc_proc(int _w, int _h) { w = _w; h = _h; }

    int width() { return w; }
    int height() { return h; }

    void nextline();
    void print(const string &str);
    static int count_newlines(const string &str);

    // Remove trailing newlines.
    static void trim(string &str);
    // rfind consecutive newlines and truncate.
    static bool chop(string &str);

    void get_string(string &str);

protected:
    int w;
    int h;
    ostringstream ostr;
};

#endif


/*
 *  File:       describe.h
 *  Summary:    Functions used to print information about various game objects.
 *  Written by: Linley Henzell
 */

#ifndef DESCRIBE_H
#define DESCRIBE_H

#include <string>
#include <sstream>
#include "externs.h"
#include "enum.h"
#include "mon-info.h"

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
    std::ostringstream body;
    std::string title;
    std::string prefix;
    std::string suffix;
    std::string footer;
    std::string quote;
};

void append_spells(std::string &desc, const item_def &item);

bool is_dumpable_artefact(const item_def &item, bool verbose);

std::string get_item_description(const item_def &item, bool verbose,
                                  bool dump = false, bool noquote = false);

std::string god_title(god_type which_god, species_type which_species);
void describe_god(god_type which_god, bool give_title);

void describe_feature_wide(const coord_def& pos);
void get_feature_desc(const coord_def &gc, describe_info &inf);

void set_feature_desc_long(const std::string &raw_name,
                           const std::string &desc);

void set_feature_quote(const std::string &raw_name,
                       const std::string &quote);

bool describe_item(item_def &item, bool allow_inscribe = false,
                   bool shopping = false);
void get_item_desc(const item_def &item, describe_info &inf,
                   bool terse = false);
void inscribe_item(item_def &item, bool msgwin);

void append_weapon_stats(std::string &description, const item_def &item);
void append_armour_stats(std::string &description, const item_def &item);
void append_missile_info(std::string &description);

void describe_monsters(const monster_info &mi, bool force_seen = false,
                       const std::string &footer = "",
                       bool wait_until_key_pressed = true);

void get_monster_db_desc(const monster_info &mi, describe_info &inf,
                         bool &has_stat_desc, bool force_seen = false);

void get_spell_desc(const spell_type spell, describe_info &inf);
void describe_spell(spell_type spelled, const item_def* item = NULL);

std::string get_ghost_description(const monster_info &mi, bool concise = false);

std::string get_skill_description(int skill, bool need_title = false);

void describe_skill(int skill);

void print_description(const std::string &desc);
void print_description(const describe_info &inf);

template<class T> void process_description(T &proc, const describe_info &inf);

void trim_randart_inscrip(item_def& item);
std::string artefact_auto_inscription(const item_def& item);
void add_autoinscription(item_def &item, std::string ainscrip);
void add_autoinscription(item_def &item);
void add_inscription(item_def &item, std::string inscrip);

const char *trap_name(trap_type trap);
int str_to_trap(const std::string &s);

int count_desc_lines(const std::string _desc, const int width);

class alt_desc_proc
{
public:
    alt_desc_proc(int _w, int _h) { w = _w; h = _h; }

    int width() { return w; }
    int height() { return h; }

    void nextline();
    void print(const std::string &str);
    static int count_newlines(const std::string &str);

    // Remove trailing newlines.
    static void trim(std::string &str);
    // rfind consecutive newlines and truncate.
    static bool chop(std::string &str);

    void get_string(std::string &str);

protected:
    int w;
    int h;
    std::ostringstream ostr;
};

/* ***********************************************************************
 * template implementations
 * *********************************************************************** */
// My kingdom for a closure.
template<class T>
inline void process_description(T &proc, const describe_info &inf)
{
    const unsigned int line_width = proc.width();
    const          int height     = proc.height();

    std::string desc;

    // How many lines is the title; we also seem to be adding 1 to
    // start with.
    int num_lines = count_desc_lines(inf.title, line_width) + 1;

    int body_lines   = count_desc_lines(inf.body.str(), line_width);
    const int suffix_lines = count_desc_lines(inf.suffix, line_width);
    const int prefix_lines = count_desc_lines(inf.prefix, line_width);
    const int footer_lines = count_desc_lines(inf.footer, line_width)
                             + (inf.footer.empty() ? 0 : 1);
    const int quote_lines  = count_desc_lines(inf.quote, line_width);

    // Maybe skip the body if body + title would be too many lines.
    if (inf.title.empty())
    {
        desc = inf.body.str();
        // There is a default 1 line addition for some reason.
        num_lines = body_lines + 1;
    }
    else if (body_lines + num_lines + 2 <= height)
    {
        desc = inf.title + "\n\n";
        desc += inf.body.str();
        // Got 2 lines from the two \ns that weren't counted yet.
        num_lines += body_lines + 2;
    }
    else
        desc = inf.title + "\n";

    // Prefer the footer over the suffix.
    if (num_lines + suffix_lines + footer_lines <= height)
    {
        desc = desc + inf.suffix;
        num_lines += suffix_lines;
    }

    // Prefer the footer over the prefix.
    if (num_lines + prefix_lines + footer_lines <= height)
    {
        desc = inf.prefix + desc;
        num_lines += prefix_lines;
    }

    // Prefer the footer over the quote.
    if (num_lines + footer_lines + quote_lines + 1 <= height)
    {
        if (!desc.empty())
        {
            desc += "\n";
            num_lines++;
        }
        desc = desc + inf.quote;
        num_lines += quote_lines;
    }

    if (!inf.footer.empty() && num_lines + footer_lines <= height)
    {
        const int bottom_line = std::min(std::max(24, num_lines + 2),
                                         height - footer_lines + 1);
        const int newlines = bottom_line - num_lines;

        if (newlines >= 0)
        {
            desc.append(newlines, '\n');
            desc = desc + inf.footer;
        }
    }

    std::string::size_type nextLine = std::string::npos;
    unsigned int  currentPos = 0;

    while (currentPos < desc.length())
    {
        if (currentPos != 0)
            proc.nextline();

        // See if '\n' is within one line_width.
        nextLine = desc.find('\n', currentPos);

        if (nextLine >= currentPos && nextLine < currentPos + line_width)
        {
            proc.print(desc.substr(currentPos, nextLine-currentPos));
            currentPos = nextLine + 1;
            continue;
        }

        // Handle real line breaks.  No substitutions necessary, just update
        // the counts.
        nextLine = desc.find('\n', currentPos);
        if (nextLine >= currentPos && nextLine < currentPos + line_width)
        {
            proc.print(desc.substr(currentPos, nextLine-currentPos));
            currentPos = nextLine + 1;
            continue;
        }

        // No newline -- see if rest of string will fit.
        if (currentPos + line_width >= desc.length())
        {
            proc.print(desc.substr(currentPos));
            return;
        }


        // Ok, try to truncate at space.
        nextLine = desc.rfind(' ', currentPos + line_width);

        if (nextLine > 0)
        {
            proc.print(desc.substr(currentPos, nextLine - currentPos));
            currentPos = nextLine + 1;
            continue;
        }

        // Oops.  Just truncate.
        nextLine = currentPos + line_width;

        nextLine = std::min(inf.body.str().length(), nextLine);

        proc.print(desc.substr(currentPos, nextLine - currentPos));
        currentPos = nextLine;
    }
}




#endif

/**
 * @file
 * @brief Functions used to print information about spells, spellbooks, etc.
 **/

#include "AppHdr.h"

#include "describe-spells.h"

#include "cio.h"
#include "delay.h"
#include "describe.h"
#include "externs.h"
#include "invent.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "monster.h" // SEEN_SPELLS_KEY
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stringutil.h"
#include "state.h"
#include "tileweb.h"
#include "unicode.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif

/**
 * Returns a spellset containing the spells for the given item.
 *
 * @param item      The item in question.
 * @return          A single-element vector, containing the list of all
 *                  non-null spells in the given book, blank-labeled.
 */
spellset item_spellset(const item_def &item)
{
    if (!item.has_spells())
        return {};

    return { { "\n", spells_in_book(item) } };
}

/**
 * What's the appropriate descriptor for a given type of "spell" that's not
 * really a spell?
 *
 * @param type              The type of spell-ability; e.g. MON_SPELL_MAGICAL.
 * @return                  A descriptor of the spell type; e.g. "divine",
 *                          "magical", etc.
 */
static string _ability_type_descriptor(mon_spell_slot_flag type)
{
    static const map<mon_spell_slot_flag, string> descriptors =
    {
        { MON_SPELL_NATURAL, "natural" },
        { MON_SPELL_MAGICAL, "magical" },
        { MON_SPELL_PRIEST,  "divine" },
    };

    return lookup(descriptors, type, "buggy");
}

/**
 * What type of effects is this spell type vulnerable to?
 *
 * @param type              The type of spell-ability; e.g. MON_SPELL_MAGICAL.
 * @param silencable        Whether any of the spells are subject to Silence
 *                          despite being non-wizardly and non-priestly.
 * @return                  A description of the spell's vulnerabilities.
 */
static string _ability_type_vulnerabilities(mon_spell_slot_flag type,
                                            bool silencable)
{
    if (type == MON_SPELL_NATURAL && !silencable)
        return "";
    silencable |= type == MON_SPELL_WIZARD || type == MON_SPELL_PRIEST;
    const bool antimagicable
        = type == MON_SPELL_WIZARD || type == MON_SPELL_MAGICAL;
    ASSERT(silencable || antimagicable);
    return make_stringf(", which are affected by%s%s%s",
                        silencable ? " silence" : "",
                        silencable && antimagicable ? " and" : "",
                        antimagicable ? " antimagic" : "");
}

/**
 * Produces a portion of the spellbook description: the portion indicating
 * whether the list of spellbooks has been filtered based on which spells you
 * have seen the monster cast already.
 *
 * @param type    The type of ability set / spellbook we're decribing.
 * @param pronoun The monster pronoun to use (should be derived from PRONOUN_OBJECTIVE).
 * @return        A string to include in the spellbook description.
 */
static string _describe_spell_filtering(mon_spell_slot_flag type, const char* pronoun)
{
    const bool is_spell = type = MON_SPELL_WIZARD;
    return make_stringf(" (judging by the %s you have seen %s %s)",
                        is_spell ? "spells" : "abilities",
                        pronoun,
                        is_spell ? "cast" : "use");
}

/**
 * What description should a given (set of) monster spellbooks be prefixed
 * with?
 *
 * @param type              The type of book(s); e.g. MON_SPELL_MAGICAL.
 * @param num_books         The number of books in the set.
 * @param has_silencable    Whether any of the spells are subject to Silence
 *                          despite being non-wizardly and non-priestly.
 * @param has_filtered      Whether any spellbooks have been filtered out due
 *                          to the spells you've seen the monster cast.
 * @param pronoun           The pronoun to use in describing which spells
 *                          the monster has been seen casting.
 * @return                  A header string for the bookset; e.g.,
 *                          "has mastered one of the following spellbooks:"
 *                          "possesses the following natural abilities:"
 */
static string _booktype_header(mon_spell_slot_flag type, size_t num_books,
                               bool has_silencable, bool has_filtered, const char* pronoun)
{
    const string vulnerabilities =
        _ability_type_vulnerabilities(type, has_silencable);
    const string spell_filter_desc = has_filtered ? _describe_spell_filtering(type, pronoun)
                                                  : "";

    if (type == MON_SPELL_WIZARD)
    {
        return make_stringf("has mastered %s%s%s:",
                            num_books > 1 ? "one of the following spellbooks"
                                          : "the following spells",
                            spell_filter_desc.c_str(),
                            vulnerabilities.c_str());
    }

    const string descriptor = _ability_type_descriptor(type);

    return make_stringf("possesses the following %s abilities%s%s:",
                        descriptor.c_str(),
                        spell_filter_desc.c_str(),
                        vulnerabilities.c_str());
}

static bool _spell_in_book(spell_type spell, const vector<mon_spell_slot> &book)
{
    return any_of(book.begin(), book.end(),
                  [=](mon_spell_slot slot){return slot.spell == spell;});
}

/**
 * Is it possible that the given monster could be using the given book, from
 * what the player knows about each?
 *
 * @param book          A list of spells.
 * @param mon_owner     The monster being examined.
 * @return              Whether it's possible for the given monster to
 */
static bool _book_valid(const vector<mon_spell_slot> &book,
                        const monster_info &mi)
{
    if (!mi.props.exists(SEEN_SPELLS_KEY))
        return true;

    auto seen_spells = mi.props[SEEN_SPELLS_KEY].get_vector();

    // assumption: any monster with multiple true spellbooks will only ever
    // use one of them
    return all_of(seen_spells.begin(), seen_spells.end(),
                  [&](int spell){return _spell_in_book((spell_type)spell, book);});
}

static void _split_by_silflag(unique_books &books)
{
    unique_books result;

    for (auto book : books)
    {
        vector<mon_spell_slot> silflag;
        vector<mon_spell_slot> no_silflag;

        for (auto i : book)
        {
            if (i.flags & MON_SPELL_NO_SILENT)
                silflag.push_back(i);
            else no_silflag.push_back(i);
        }

        if (!no_silflag.empty())
            result.push_back(no_silflag);
        if (!silflag.empty())
            result.push_back(silflag);
    }

    books = result;
}

/**
 * Append all spells of a given type that a given monster may know to the
 * provided vector.
 *
 * @param mi                The player's knowledge of a monster.
 * @param type              The type of spells to select.
 *                          (E.g. MON_SPELL_MAGICAL, MON_SPELL_WIZARD...)
 * @param[out] all_books    An output vector of "spellbooks".
 */
static void _monster_spellbooks(const monster_info &mi,
                                mon_spell_slot_flag type,
                                spellset &all_books)
{
    unique_books books = get_unique_spells(mi, type);

    // Books of natural abilities get special treatment, because there should
    // be information about silence in the label(s).
    const bool ability_case =
        (bool) (type & (MON_SPELL_MAGICAL | MON_SPELL_NATURAL));
    // We must split them now; later we'll label them separately.
    if (ability_case)
        _split_by_silflag(books);

    const size_t num_books = books.size();

    if (num_books == 0)
        return;

    const string set_name = type == MON_SPELL_WIZARD ? "Book" : "Set";

    // filter out books we know this monster can't cast (conflicting books)
    std::vector<size_t> valid_books;
    bool filtered_books = false;
    for (size_t i = 0; i < num_books; ++i)
    {
        if (num_books <= 1 || _book_valid(books[i], mi))
            valid_books.emplace_back(i);
        else if (!_book_valid(books[i], mi))
            filtered_books = true;
    }

    // Loop through books and display spells/abilities for each of them
    for (size_t i = 0; i < valid_books.size(); ++i)
    {
        const vector<mon_spell_slot> &book_slots = books[valid_books[i]];
        spellbook_contents output_book;

        const bool has_silencable = any_of(begin(book_slots), end(book_slots),
            [](const mon_spell_slot& slot)
            {
                return slot.flags & MON_SPELL_NO_SILENT;
            });

        if (i == 0 || ability_case)
        {
            output_book.label +=
                "\n" +
                uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)) +
                " " +
                _booktype_header(type, valid_books.size(), has_silencable,
                                 filtered_books, mi.pronoun(PRONOUN_OBJECTIVE));
        }
        else
        {
            output_book.label += make_stringf("\n%s %d:",
                                              set_name.c_str(), (int) i + 1);
        }

        // Does the monster have a spell that allows them to cast Abjuration?
        bool mons_abjure = false;

        for (const auto& slot : book_slots)
        {
            const spell_type spell = slot.spell;
            if (!spell_is_soh_breath(spell))
            {
                output_book.spells.emplace_back(spell);
                if (get_spell_flags(spell) & SPFLAG_MONS_ABJURE)
                    mons_abjure = true;
                continue;
            }

            const vector<spell_type> *breaths = soh_breath_spells(spell);
            ASSERT(breaths);
            for (auto breath : *breaths)
                output_book.spells.emplace_back(breath);
        }

        if (mons_abjure)
            output_book.spells.emplace_back(SPELL_ABJURATION);

        all_books.emplace_back(output_book);
    }
}

/**
 * Return a spellset containing the spells potentially given by the given
 * monster information.
 *
 * @param mi    The player's knowledge of a monster.
 * @return      The spells potentially castable by that monster (as far as
 *              the player knows).
 */
spellset monster_spellset(const monster_info &mi)
{
    if (!mi.has_spells())
        return {};

    static const mon_spell_slot_flag book_flags[] =
    {
        MON_SPELL_NATURAL,
        MON_SPELL_MAGICAL,
        MON_SPELL_PRIEST,
        MON_SPELL_WIZARD,
    };

    spellset books;

    for (auto book_flag : book_flags)
        _monster_spellbooks(mi, book_flag, books);

    ASSERT(books.size());
    return books;
}


/**
 * Build a flat vector containing all unique spells in a given spellset.
 *
 * @param spellset      The spells in question.
 * @return              An ordered set of unique spells in the given set.
 *                      Guaranteed to be in the same order as in the spellset.
 */
static vector<spell_type> _spellset_contents(const spellset &spells)
{
    // find unique spells (O(nlogn))
    set<spell_type> unique_spells;
    for (auto &book : spells)
        for (auto spell : book.spells)
            unique_spells.insert(spell);

    // list spells in original order (O(nlogn)?)
    vector<spell_type> spell_list;
    for (auto &book : spells)
    {
        for (auto spell : book.spells)
        {
            if (unique_spells.erase(spell) == 1)
                spell_list.emplace_back(spell);
        }
    }

    return spell_list;
}

/**
 * What spell should a given colour be listed with?
 *
 * @param spell         The spell in question.
 * @param source_item   The physical item holding the spells. May be null.
 */
static int _spell_colour(spell_type spell, const item_def* const source_item)
{
    if (!crawl_state.need_save)
        return COL_UNKNOWN;

    if (!source_item)
        return spell_highlight_by_utility(spell, COL_UNKNOWN);

    if (you.has_spell(spell))
        return COL_MEMORIZED;

    // this is kind of ugly.
    if (!you_can_memorise(spell)
        || you.experience_level < spell_difficulty(spell)
        || player_spell_levels() < spell_levels_required(spell))
    {
        return COL_USELESS;
    }

    if (god_hates_spell(spell, you.religion))
        return COL_FORBIDDEN;

    if (!you.has_spell(spell))
        return COL_UNMEMORIZED;

    return spell_highlight_by_utility(spell, COL_UNKNOWN);
}

/**
 * List the name(s) of the school(s) the given spell is in.
 *
 * XXX: This is almost certainly duplicating something somewhere. Also, it's
 * pretty ugly.
 *
 * @param spell     The spell in question.
 * @return          A '/'-separated list of spellschool names.
 */
static string _spell_schools(spell_type spell)
{
    string schools;

    for (const auto school_flag : spschools_type::range())
    {
        if (!spell_typematch(spell, school_flag))
            continue;

        if (!schools.empty())
            schools += "/";
        schools += spelltype_long_name(school_flag);
    }

    return schools;
}

/**
 * Should spells from the given source be listed in two columns instead of
 * one?
 *
 * @param source_item   The source of the spells; a book, or nullptr in the
 *                      case of monster spellbooks.
 * @return              source_item == nullptr
 */
static bool _list_spells_doublecolumn(const item_def* const source_item)
{
    return !source_item;
}

/**
 * Produce a mapping from characters (used as indices) to spell types in
 * the given spellset.
 *
 * @param spells        A list of 'books' of spells.
 * @param source_item   The source of the spells; a book, or nullptr in the
 *                      case of monster spellbooks.
 * @return              A list of all unique spells in the given set, ordered
 *                      either in original order or column-major order, the
 *                      latter in the case of a double-column layout.
 */
vector<pair<spell_type,char>> map_chars_to_spells(const spellset &spells,
                                       const item_def* const source_item)
{
    char next_ch = 'a';
    const vector<spell_type> flat_spells = _spellset_contents(spells);
    vector<pair<spell_type,char>> ret;
    if (!_list_spells_doublecolumn(source_item))
    {
        for (auto spell : flat_spells)
            ret.emplace_back(pair<spell_type,char>(spell, next_ch++));
    }
    else
    {
        for (size_t i = 0; i < flat_spells.size(); i += 2)
            ret.emplace_back(pair<spell_type,char>(flat_spells[i], next_ch++));
        for (size_t i = 1; i < flat_spells.size(); i += 2)
            ret.emplace_back(pair<spell_type,char>(flat_spells[i], next_ch++));
    }
    return ret;
}

/**
 * Describe a given set of spells.
 *
 * @param book              A labeled set of spells, corresponding to a book
 *                          or monster spellbook.
 * @param spell_map         The letters to use for each spell.
 * @param source_item       The physical item holding the spells. May be null.
 * @param description[out]  An output string to append to.
 * @param mon_owner         If this spellset is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 */
static void _describe_book(const spellbook_contents &book,
                           vector<pair<spell_type,char>> &spell_map,
                           const item_def* const source_item,
                           formatted_string &description,
                           const monster_info *mon_owner)
{
    description.textcolour(LIGHTGREY);

    description.cprintf("%s", book.label.c_str());

    // only display header for book spells
    if (source_item)
    {
        description.cprintf(
            "\n Spells                           Type                      Level");
    }
    description.cprintf("\n");

    // list spells in two columns, instead of one? (monster books)
    const bool doublecolumn = _list_spells_doublecolumn(source_item);

    bool first_line_element = true;
    const int hd = mon_owner ? mon_owner->spell_hd() : 0;
    for (auto spell : book.spells)
    {
        description.cprintf(" ");

        description.textcolour(_spell_colour(spell, source_item));

        // don't crash if we have more spells than letters.
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [&spell](const pair<spell_type,char>& e)
                {
                    return e.first == spell;
                });
        const char spell_letter = entry != spell_map.end()
                                            ? entry->second : ' ';

        auto flags = get_spell_flags(spell);
        int pow = mons_power_for_hd(spell, hd, false);
        int range = spell_range(spell, pow, false);
        const bool has_range = mon_owner
                            && range > 0
                            && !testbits(flags, SPFLAG_SELFENCH);
        const bool in_range = has_range
                        && grid_distance(you.pos(), mon_owner->pos) <= range;
        const char *range_col = in_range ? "lightred" : "lightgray";
        string range_str = has_range
            ? make_stringf(" (<%s>%d</%s>)", range_col, range, range_col)
            : "";
        string hex_str = "";

        if (hd > 0 && crawl_state.need_save
#ifndef DEBUG_DIAGNOSTICS
            && mon_owner->attitude != ATT_FRIENDLY
#endif
            && testbits(flags, SPFLAG_MR_CHECK))
        {
            if (you.immune_to_hex(spell))
                hex_str = "(immune) ";
            else
                hex_str = make_stringf("(%d%%) ", hex_chance(spell, hd));
        }

        int hex_len = hex_str.length(), range_len = range_str.empty() ? 0 : 4;

        description += formatted_string::parse_string(
                make_stringf("%c - %s%s%s", spell_letter,
                chop_string(spell_title(spell), 29-hex_len-range_len).c_str(),
                hex_str.c_str(),
                range_str.c_str()));

        // only display type & level for book spells
        if (doublecolumn)
        {
            // print monster spells in two columns
            if (first_line_element)
                description.cprintf("    ");
            else
                description.cprintf("\n");
            first_line_element = !first_line_element;
            continue;
        }

        string schools =
            source_item->base_type == OBJ_RODS ? "Evocations"
                                               : _spell_schools(spell);
        description.cprintf("%s%d\n",
                            chop_string(schools, 30).c_str(),
                            spell_difficulty(spell));
    }

    // are we halfway through a column?
    if (doublecolumn && book.spells.size() % 2)
        description.cprintf("\n");
}


/**
 * List a given set of spells.
 *
 * @param spells            The set of spells to be listed.
 * @param source_item       The physical item holding the spells. May be null.
 * @param description[out]  An output string to append to.
 * @param mon_owner         If this spellset is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 */
void describe_spellset(const spellset &spells,
                       const item_def* const source_item,
                       formatted_string &description,
                       const monster_info *mon_owner)
{
    auto spell_map = map_chars_to_spells(spells, source_item);
    for (auto book : spells)
        _describe_book(book, spell_map, source_item, description, mon_owner);

    if (mon_owner)
    {
        description += formatted_string::parse_string(
            "\n(x%) indicates the chance to beat your MR, "
            "and (y) indicates the spell range; shown as (<red>y</red>) if "
            "you are in range.\n");
    }
}

#ifdef USE_TILE_WEB
static void _write_book(const spellbook_contents &book,
                           vector<pair<spell_type,char>> &spell_map,
                           const item_def* const source_item,
                           const monster_info *mon_owner)
{
    tiles.json_open_object();
    tiles.json_write_string("label", book.label);
    const int hd = mon_owner ? mon_owner->spell_hd() : 0;
    tiles.json_open_array("spells");
    for (auto spell : book.spells)
    {
        tiles.json_open_object();
        tiles.json_write_string("title", spell_title(spell));
        tiles.json_write_int("colour", _spell_colour(spell, source_item));
        tiles.json_write_name("tile");
        tiles.write_tileidx(tileidx_spell(spell));

        // don't crash if we have more spells than letters.
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [&spell](const pair<spell_type,char>& e) { return e.first == spell; });
        const char spell_letter = entry != spell_map.end() ? entry->second : ' ';
        tiles.json_write_string("letter", string(1, spell_letter));

        int pow = mons_power_for_hd(spell, hd, false);
        int range = spell_range(spell, pow, false);
        bool in_range = mon_owner && grid_distance(you.pos(), mon_owner->pos) <= range;
        const char *range_col = in_range ? "lightred" : "lightgray";
        if (mon_owner && range >= 0)
        {
            tiles.json_write_string("range_string",
                    make_stringf("<%s>%d</%s>", range_col, range, range_col));
        }

        if (hd > 0 && crawl_state.need_save
#ifndef DEBUG_DIAGNOSTICS
            && mon_owner->attitude != ATT_FRIENDLY
#endif
            && (get_spell_flags(spell) & SPFLAG_MR_CHECK))
        {
            if (you.immune_to_hex(spell))
                tiles.json_write_string("hex_chance", "immune");
            else
                tiles.json_write_string("hex_chance",
                        make_stringf("%d%%", hex_chance(spell, hd)));
        }

        string schools = (source_item && source_item->base_type == OBJ_RODS) ?
                "Evocations" : _spell_schools(spell);
        tiles.json_write_string("schools", schools);
        tiles.json_write_int("level", spell_difficulty(spell));
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.json_close_object();
}

void write_spellset(const spellset &spells,
                       const item_def* const source_item,
                       const monster_info *mon_owner)
{
    auto spell_map = map_chars_to_spells(spells, source_item);
    tiles.json_open_array("spellset");
    for (auto book : spells)
        _write_book(book, spell_map, source_item, mon_owner);
    tiles.json_close_array();
}
#endif

/**
 * Return a description of the spells in the given item.
 *
 * @param item      The book in question.
 * @return          A column-and-row listing of the spells in the given item,
 *                  including names, schools & levels.
 */
string describe_item_spells(const item_def &item)
{
    formatted_string description;
    describe_spellset(item_spellset(item), &item, description);
    return description.tostring();
}

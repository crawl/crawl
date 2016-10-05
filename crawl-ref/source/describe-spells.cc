/**
 * @file
 * @brief Functions used to print information about spells, spellbooks, rods,
 *        etc.
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
#include "monster.h" // SEEN_SPELLS_KEY
#include "prompt.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stringutil.h"
#include "state.h"
#include "unicode.h"

/**
 * Returns a spellset containing the spells for the given item.
 *
 * @param item      The item in question.
 * @return          A single-element vector, containing the list of all
 *                  non-null spells in the given book/rod, blank-labeled.
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

    if (mi.type != MONS_PANDEMONIUM_LORD)
        for (auto book_flag : book_flags)
            _monster_spellbooks(mi, book_flag, books);
    else if (mi.props.exists(SEEN_SPELLS_KEY))
    {
        spellbook_contents output_book;
        output_book.label
          = make_stringf("You have seen %s using the following:",
                         mi.pronoun(PRONOUN_SUBJECTIVE));
        for (int spell : mi.props[SEEN_SPELLS_KEY].get_vector())
            output_book.spells.emplace_back((spell_type)spell);
        books.emplace_back(output_book);
    }

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

    const bool rod = source_item && OBJ_RODS == source_item->base_type;
    if (!source_item || source_item->base_type != OBJ_BOOKS)
        return spell_highlight_by_utility(spell, COL_UNKNOWN, false, rod);

    if (you.has_spell(spell))
        return COL_MEMORIZED;

    // this is kind of ugly.
    if (!you_can_memorise(spell)
        || you.experience_level < spell_difficulty(spell)
        || player_spell_levels() < spell_levels_required(spell))
    {
        return COL_USELESS;
    }

    if (god_hates_spell(spell, you.religion, rod))
        return COL_FORBIDDEN;

    if (!you.has_spell(spell))
        return COL_UNMEMORIZED;

    return spell_highlight_by_utility(spell, COL_UNKNOWN, false, rod);
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
 * @param source_item   The source of the spells; a book, rod, or nullptr in
 *                      the case of monster spellbooks.
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
 * @param source_item   The source of the spells (book or rod),, or nullptr in
 *                      the case of monster spellbooks.
 * @return              A list of all unique spells in the given set, ordered
 *                      either in original order or column-major order, the
 *                      latter in the case of a double-column layout.
 */
vector<spell_type> map_chars_to_spells(const spellset &spells,
                                       const item_def* const source_item)
{
    const vector<spell_type> flat_spells = _spellset_contents(spells);
    if (!_list_spells_doublecolumn(source_item))
        return flat_spells;

    vector<spell_type> column_spells;
    for (size_t i = 0; i < flat_spells.size(); i += 2)
        column_spells.emplace_back(flat_spells[i]);
    for (size_t i = 1; i < flat_spells.size(); i += 2)
        column_spells.emplace_back(flat_spells[i]);
    return column_spells;
}

/**
 * Describe a given set of spells.
 *
 * @param book              A labeled set of spells, corresponding to a book,
 *                          rod, or monster spellbook.
 * @param spell_letters     The letters to use for each spell.
 * @param source_item       The physical item holding the spells. May be null.
 * @param description[out]  An output string to append to.
 * @param mon_owner         If this spellset is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 */
static void _describe_book(const spellbook_contents &book,
                           map<spell_type, char> &spell_letters,
                           const item_def* const source_item,
                           formatted_string &description,
                           const monster_info *mon_owner)
{
    description.textcolour(LIGHTGREY);

    description.cprintf("%s", book.label.c_str());

    // only display header for book/rod spells
    if (source_item)
        description.cprintf("\n Spells                             Type                      Level");
    description.cprintf("\n");

    // list spells in two columns, instead of one? (monster books)
    const bool doublecolumn = _list_spells_doublecolumn(source_item);

    bool first_line_element = true;
    const int hd = mon_owner ? mon_owner->hd : 0;
    for (auto spell : book.spells)
    {
        description.cprintf(" ");

        description.textcolour(_spell_colour(spell, source_item));

        // don't crash if we have more spells than letters.
        const char *spell_letter_index = map_find(spell_letters, spell);
        const char spell_letter = spell_letter_index ?
                                  index_to_letter(*spell_letter_index) :
                                  ' ';
        if (hd > 0 && crawl_state.need_save
            && (get_spell_flags(spell) & SPFLAG_MR_CHECK)
            && mon_owner->attitude != ATT_FRIENDLY)
        {
            description.cprintf("%c - (%d%%) %s",
                            spell_letter,
                            hex_chance(spell, hd),
                            chop_string(spell_title(spell), 22).c_str());
        }
        else
        {
            description.cprintf("%c - %s",
                            spell_letter,
                            chop_string(spell_title(spell), 29).c_str());
        }

        // only display type & level for book/rod spells
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
    // make a map of characters to spells...
    const vector<spell_type> flat_spells = map_chars_to_spells(spells,
                                                               source_item);
    // .. and spells to characters.
    map<spell_type, char> spell_letters;
    // TODO: support more than 26 spells
    for (size_t c = 0; c < flat_spells.size() && c < 26; c++)
        spell_letters[flat_spells[c]] = (char) c;

    for (auto book : spells)
        _describe_book(book, spell_letters, source_item, description, mon_owner);
}

/**
 * Return a description of the spells in the given item.
 *
 * @param item      The book or rod in question.
 * @return          A column-and-row listing of the spells in the given item,
 *                  including names, schools & levels.
 */
string describe_item_spells(const item_def &item)
{
    formatted_string description;
    describe_spellset(item_spellset(item), &item, description);
    return description.tostring();
}

/**
 * List a given set of spells & allow the player to select them for further
 * information/interaction.
 *
 * @param spells            The set of spells to be listed.
 * @param mon_owner         If this spell is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 * @param source_item       The physical item holding the spells. May be null.
 * @param initial_desc      A description to prefix the spellset with.
 */
void list_spellset(const spellset &spells, const monster_info *mon_owner,
                   const item_def *source_item, formatted_string &initial_desc)
{
    const bool can_memorise = source_item
                              && source_item->base_type == OBJ_BOOKS
                              && in_inventory(*source_item);

    formatted_string &description = initial_desc;
    describe_spellset(spells, source_item, description, mon_owner);

    description.textcolour(LIGHTGREY);

    description.cprintf("Select a spell to read its description");
    if (can_memorise)
        description.cprintf(" or to to memorise it");
    description.cprintf(".\n");

    spell_scroller ssc(spells, mon_owner, source_item);
    ssc.wrap_formatted_string(description);
    ssc.show();
}

/**
 * Handle a keypress while looking at a scrollable list of spells.
 *
 * @param keyin     The character corresponding to the pressed key.
 * @return          True if the menu should continue running; false if the
 *                  menu should exit & return control to its caller.
 */
bool spell_scroller::process_key(int keyin)
{
    lastch = keyin;

    if (keyin == ' ')
        return false; // in ?/m, indicates you're looking for an inexact match

    // TOOD: support more than 26 spells
    if (keyin < 'a' || keyin > 'z')
        return formatted_scroller::process_key(keyin);

    // make a map of characters to spells.
    const vector<spell_type> flat_spells = map_chars_to_spells(spells,
                                                               source_item);

    const int spell_index = letter_to_index(keyin);
    ASSERT(spell_index >= 0);
    if ((size_t) spell_index >= flat_spells.size())
        return formatted_scroller::process_key(keyin);

    const spell_type chosen_spell = flat_spells[spell_index];
    describe_spell(chosen_spell, mon_owner, source_item);

    if (already_learning_spell()) // player began (M)emorizing
        return false; // time to leave the menu

    draw_menu();
    return true; // loop
}

spell_scroller::~spell_scroller() { }

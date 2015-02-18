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
#include "prompt.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stringutil.h"
#include "state.h"
#include "unicode.h"

extern const spell_type serpent_of_hell_breaths[4][3];

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
 * @param type              The type of spell-ability; e.g. MON_SPELL_DEMONIC.
 * @param caster_holiness   The holiness of the caster; e.g. MH_NATURAL.
 * @return                  A descriptor of the spell type; e.g. "natural",
 *                          "angelic", "demonic", etc.
 */
static string _ability_type_descriptor(mon_spell_slot_flags type,
                                       mon_holy_type caster_holiness)
{
    // special case (:
    if (type == MON_SPELL_DEMONIC && caster_holiness == MH_HOLY)
        return "angelic";

    static map<mon_spell_slot_flags, string> descriptors =
    {
        { MON_SPELL_NATURAL, "special" },
        { MON_SPELL_MAGICAL, "magical" },
        { MON_SPELL_DEMONIC, "demonic" },
        { MON_SPELL_PRIEST,  "divine" },
    };

    return lookup(descriptors, type, "buggy");
}

/**
 * What description should a given (set of) monster spellbooks be prefixed
 * with?
 *
 * @param type              The type of book(s); e.g. MON_SPELL_DEMONIC.
 * @param num_books         The number of books in the set.
 * @param mi                The player's information about the caster.
 * @return                  A header string for the bookset; e.g.,
 *                          "She has mastered one of the following spellbooks:\n"
 *                          "It possesses the following special abilities:\n"
 */
static string _booktype_header(mon_spell_slot_flags type, size_t num_books,
                               const monster_info &mi)
{
    const string pronoun = uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));

    if (type == MON_SPELL_WIZARD)
    {
        return make_stringf("\n%s has mastered %s:", pronoun.c_str(),
                            num_books > 1 ? "one of the following spellbooks"
                                          : "the following spells");
    }

    const string descriptor = _ability_type_descriptor(type, mi.holi);

    if (num_books > 1)
    {
        return make_stringf("\n%s possesses one of the following sets of %s abilities:",
                            pronoun.c_str(), descriptor.c_str());
    }

    return make_stringf("\n%s possesses the following %s abilities:",
                        pronoun.c_str(), descriptor.c_str());
}

/**
 * Append all spells of a given type that a given monster may know to the
 * provided vector.
 *
 * @param mi                The player's knowledge of a monster.
 * @param type              The type of spells to select.
 *                          (E.g. MON_SPELL_DEMONIC, MON_SPELL_WIZARD...)
 * @param[out] all_books    An output vector of "spellbooks".
 */
static void _monster_spellbooks(const monster_info &mi,
                                mon_spell_slot_flags type,
                                spellset &all_books)
{
    const unique_books books = get_unique_spells(mi, type);
    const size_t num_books = books.size();

    if (num_books == 0)
        return;

    const string set_name = type == MON_SPELL_WIZARD ? "Book" : "Set";

    // Loop through books and display spells/abilities for each of them
    for (size_t i = 0; i < num_books; ++i)
    {
        const vector<spell_type> &book_spells = books[i];
        spellbook_contents output_book;

        if (i == 0)
            output_book.label += _booktype_header(type, num_books, mi);
        if (num_books > 1)
        {
            output_book.label += make_stringf("\n%s %" PRIuSIZET ":",
                                              set_name.c_str(), i+1);
        }

        for (auto spell : book_spells)
        {
            if (spell != SPELL_SERPENT_OF_HELL_BREATH)
            {
                output_book.spells.emplace_back(spell);
                continue;
            }

            static map<monster_type, size_t> serpent_indices =
            {
                { MONS_SERPENT_OF_HELL, 0 },
                { MONS_SERPENT_OF_HELL_COCYTUS, 1 },
                { MONS_SERPENT_OF_HELL_DIS, 2 },
                { MONS_SERPENT_OF_HELL_TARTARUS, 3 },
            };

            const size_t *s_i_ptr = map_find(serpent_indices, mi.type);
            ASSERT(s_i_ptr);
            const size_t serpent_index = *s_i_ptr;
            ASSERT_RANGE(serpent_index, 0, ARRAYSZ(serpent_of_hell_breaths));

            for (auto breath : serpent_of_hell_breaths[serpent_index])
                output_book.spells.emplace_back(breath);
        }

        // XXX: it seems like we should be able to just place this in the
        // vector at the start, without having to copy it in now...?
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

    static const mon_spell_slot_flags book_flags[] =
    {
        MON_SPELL_NATURAL,
        MON_SPELL_MAGICAL,
        MON_SPELL_DEMONIC,
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
            if (unique_spells.find(spell) != unique_spells.end())
            {
                unique_spells.erase(spell);
                spell_list.emplace_back(spell);
            }
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
        || player_spell_levels() < spell_levels_required(spell)
        || !player_can_memorise_from_spellbook(*source_item))
    {
        return COL_USELESS;
    }

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

    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        const auto school_flag = spschools_type::exponent(i);
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
 */
static void _describe_book(const spellbook_contents &book,
                           map<spell_type, char> &spell_letters,
                           const item_def* const source_item,
                           formatted_string &description)
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
    for (auto spell : book.spells)
    {
        description.cprintf(" ");

        description.textcolour(_spell_colour(spell, source_item));

        // don't crash if we have more spells than letters.
        const char *spell_letter_index = map_find(spell_letters, spell);
        const char spell_letter = spell_letter_index ?
                                  index_to_letter(*spell_letter_index) :
                                  ' ';
        description.cprintf("%c - %s",
                            spell_letter,
                            chop_string(spell_title(spell), 29).c_str());

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
 */
void describe_spellset(const spellset &spells,
                       const item_def* const source_item,
                       formatted_string &description)
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
        _describe_book(book, spell_letters, source_item, description);
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
    const bool can_memorize =
        source_item && source_item->base_type == OBJ_BOOKS
        && in_inventory(*source_item)
        && player_can_memorise_from_spellbook(*source_item);

    formatted_string &description = initial_desc;
    describe_spellset(spells, source_item, description);

    description.textcolour(LIGHTGREY);

    description.cprintf("Select a spell to read its description");
    if (can_memorize)
        description.cprintf(", to memorize it or to forget it");
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

    const bool used_amnesia = source_item && !source_item->is_valid();
    const bool memorized = already_learning_spell();
    const bool exit_menu = used_amnesia || memorized;

    if (!exit_menu)
        draw_menu();
    return !exit_menu;
}

spell_scroller::~spell_scroller() { }

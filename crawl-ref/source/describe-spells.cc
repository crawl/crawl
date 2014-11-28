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
#include "prompt.h"
#include "spl-book.h"
#include "spl-util.h"
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

    vector<spell_type> spells;
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type spell = which_spell_in_book(item, i);
        if (spell != SPELL_NO_SPELL)
            spells.emplace_back(spell);
    }
    return { { "", spells } };
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
    vector<spell_type> list;
    for (auto &book : spells)
        for (auto spell : book.spells)
            list.emplace_back(spell);
    return list;
}

/**
 * What spell should a given colour be listed with?
 *
 * @param spell         The spell in question.
 * @param source_item   The physical item holding the spells. May be null.
 */
static int _spell_colour(spell_type spell, const item_def* const source_item)
{
    if (!source_item || source_item->base_type != OBJ_BOOKS)
        return spell_highlight_by_utility(spell);

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

    return spell_highlight_by_utility(spell);
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
        const int school_flag = 1 << i;
        if (!spell_typematch(spell, school_flag))
            continue;

        if (!schools.empty())
            schools += "/";
        schools += spelltype_long_name(school_flag);
    }

    return schools;
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

    description.cprintf("\n\n Spells");
    // only display type & level for book/rod spells
    if (source_item)
        description.cprintf("                             Type                      Level");
    description.cprintf("\n");

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
        if (!source_item)
            continue;

        string schools =
            source_item->base_type == OBJ_RODS ? "Evocations"
                                               : _spell_schools(spell);
        description.cprintf("%s%d\n",
                            chop_string(schools, 30).c_str(),
                            spell_difficulty(spell));
    }
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
    const vector<spell_type> flat_spells = _spellset_contents(spells);
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
 * @param source_item       The physical item holding the spells. May be null.
 * @param initial_desc      A description to prefix the spellset with.
 */
void list_spellset(const spellset &spells, const item_def *source_item,
                   formatted_string &initial_desc)
{
    // make a map of characters to spells.
    const vector<spell_type> flat_spells = _spellset_contents(spells);

    const bool can_memorize =
        source_item && source_item->base_type == OBJ_BOOKS
        && in_inventory(*source_item)
        && player_can_memorise_from_spellbook(*source_item);

    formatted_string &description = initial_desc;
    describe_spellset(spells, source_item, description);

    description.textcolour(LIGHTGREY);
    description.cprintf("\n");

    description.cprintf("Select a spell to read its description");
    if (can_memorize)
        description.cprintf(", to memorize it or to forget it");
    description.cprintf(".\n");

    // don't examine spellbooks that have been destroyed (by tearing out a
    // page for amnesia), and break out of the loop if you start to memorize
    // something.
    while ((!source_item || source_item->is_valid())
           && !already_learning_spell())
    {
        if (!crawl_state.is_replaying_keys())
        {
            cursor_control coff(false);
            clrscr();

            description.display();
        }

        const char input_char = toalower(getchm(KMC_MENU));
        if (input_char < 'a' || input_char > 'z')
            return; // TOOD: support more than 26 spells

        const int spell_index = letter_to_index(input_char);
        ASSERT(spell_index >= 0);
        if ((size_t) spell_index >= flat_spells.size())
            return;

        const spell_type chosen_spell = flat_spells[spell_index];
        describe_spell(chosen_spell, source_item);
    }
}

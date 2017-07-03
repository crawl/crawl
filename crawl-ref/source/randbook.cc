/**
 * @file
 * @brief Functions for generating random spellbooks.
 **/

#include "AppHdr.h"

#include "randbook.h"

#include <functional>

#include "artefact.h"
#include "database.h"
#include "english.h"
#include "god-item.h"
#include "item-name.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "religion.h"
#include "spl-book.h"
#include "stringutil.h"

static string _gen_randbook_name(string subject, string owner,
                                 spschool_flag_type disc1,
                                 spschool_flag_type disc2);
static string _gen_randbook_owner(god_type god, spschool_flag_type disc1,
                                  spschool_flag_type disc2,
                                  const vector<spell_type> &spells);

/// How many spells should be in a random theme book?
int theme_book_size() { return random2avg(7, 3) + 2; }

/// A discipline chooser that only ever returns the given discipline.
function<spschool_flag_type()> forced_book_theme(spschool_flag_type theme)
{
    return [theme]() { return theme; };
}

/// Choose a random valid discipline for a themed randbook.
spschool_flag_type random_book_theme()
{
    vector<spschool_flag_type> disciplines;
    for (auto discipline : spschools_type::range())
        disciplines.push_back(discipline);
    return disciplines[random2(disciplines.size())];
}

/**
 * Attempt to choose a valid discipline for a themed randbook which contains
 * at least the given spells.
 *
 * XXX: really we should be trying to create a pair that covers the set,
 * rather than trying to do it all with one...
 *
 * @param forced_spells     A set of spells guaranteed to be in the book.
 * @return                  A discipline which will match as many of those
 *                          spells as possible.
 */
spschool_flag_type matching_book_theme(const vector<spell_type> &forced_spells)
{
    map<spschool_flag_type, int> seen_disciplines;
    for (auto spell : forced_spells)
    {
        const spschools_type disciplines = get_spell_disciplines(spell);
        for (auto discipline : spschools_type::range())
            if (disciplines & discipline)
                ++seen_disciplines[discipline];
    }

    bool matched = false;
    for (auto seen : seen_disciplines)
    {
        if (seen.second == (int)forced_spells.size())
        {
            matched = true;
            break;
        }
    }

    if (!matched)
    {
        const spschool_flag_type *discipline
            = random_choose_weighted(seen_disciplines);
        if (discipline)
            return *discipline;
        return random_book_theme();
    }

    for (auto seen : seen_disciplines)
        seen.second = seen.second == (int)forced_spells.size() ? 1 : 0;
    const spschool_flag_type *discipline
        = random_choose_weighted(seen_disciplines);
    ASSERT(discipline);
    return *discipline;
}

/// Is the given spell found in rarebooks?
static bool _is_rare_spell(spell_type spell)
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        const book_type book = static_cast<book_type>(i);
        if (is_rare_book(book))
            for (spell_type rare_spell : spellbook_template(book))
                if (rare_spell == spell)
                    return true;
    }

    return false;
}


/**
 * Can we include the given spell in our spellbook?
 *
 * @param agent             The entity creating the book; possibly a god.
 * @param spell             The spell to be filtered.
 * @return                  Whether the spell can be included.
 */
static bool _agent_spell_filter(int agent, spell_type spell)
{
    // Only use spells available in books you might find lying about
    // the dungeon; rarebook spells are restricted to Sif-made books.
    if (spell_rarity(spell) == -1
        && (agent != GOD_SIF_MUNA || !_is_rare_spell(spell)))
    {
        return false;
    }

    // Don't include spells a god dislikes, if this is an acquirement
    // or a god gift.
    const god_type god = agent >= AQ_SCROLL ? you.religion : (god_type)agent;
    if (god_hates_spell(spell, god))
        return false;

    return true;
}

/**
 * Can we include the given spell in our themed spellbook?
 *
 * @param discipline_1      The first spellschool of the book.
 * @param discipline_2      The second spellschool of the book.
 * @param agent             The entity creating the book; possibly a god.
 * @param prev              A list of spells already chosen for the book.
 * @param spell             The spell to be filtered.
 * @return                  Whether the spell can be included.
 */
bool basic_themed_spell_filter(spschool_flag_type discipline_1,
                               spschool_flag_type discipline_2,
                               int agent,
                               const vector<spell_type> &prev,
                               spell_type spell)
{
    if (!is_valid_spell(spell))
        return false;

    // Only include spells matching at least one of the book's disciplines.
    const spschools_type disciplines = get_spell_disciplines(spell);
    if (!(disciplines & discipline_1) && !(disciplines & discipline_2))
        return false;

    // Only include spells we haven't already.
    if (count(prev.begin(), prev.end(), spell))
        return false;

    if (!_agent_spell_filter(agent, spell))
        return false;

    return true;
}

/**
 * Build and return a spell filter that excludes spells that would push us over
 * the maximum total spell levels allowed in the book.
 *
 * @param max_levels    The max total spell levels allowed.
 * @param subfilter     A filter to check further.
 */
themed_spell_filter capped_spell_filter(int max_levels,
                                        themed_spell_filter subfilter)
{
    if (max_levels < 1)
        return subfilter; // don't even bother.

    return [max_levels, subfilter](spschool_flag_type discipline_1,
                                   spschool_flag_type discipline_2,
                                   int agent,
                                   const vector<spell_type> &prev,
                                   spell_type spell)
    {
        if (!subfilter(discipline_1, discipline_2, agent, prev, spell))
            return false;

        int prev_levels = 0;
        for (auto prev_spell : prev)
            prev_levels += spell_difficulty(prev_spell);
        if (spell_difficulty(spell) + prev_levels > max_levels)
            return false;

        return true;
    };
}

/**
 * Build and return a spell filter that forces the first several spells to
 * be from the given list, disregarding other constraints
 *
 * @param forced_spells     Spells to force.
 * @param subfilter         A filter to check after all forced spells are in.
 */
themed_spell_filter forced_spell_filter(const vector<spell_type> &forced_spells,
                                        themed_spell_filter subfilter)
{
    return [&forced_spells, subfilter](spschool_flag_type discipline_1,
                                       spschool_flag_type discipline_2,
                                       int agent,
                                       const vector<spell_type> &prev,
                                       spell_type spell)
    {
        if (prev.size() < forced_spells.size())
            return spell == forced_spells[prev.size()];
        return subfilter(discipline_1, discipline_2, agent, prev, spell);
    };
}

/**
 * Generate a list of spells for a themebook.
 *
 * @param discipline_1      The first spellschool of the book.
 * @param discipline_2      The second spellschool of the book.
 * @param filter            A filter specifying which spells can be included.
 * @param agent             The entity creating the book; possibly a god.
 * @param num_spells        How many spells should be included.
 * @param spells[out]       The list to be populated.
 */
void theme_book_spells(spschool_flag_type discipline_1,
                       spschool_flag_type discipline_2,
                       themed_spell_filter filter,
                       int agent,
                       int num_spells,
                       vector<spell_type> &spells)
{
    ASSERT(num_spells >= 1);
    for (int i = 0; i < num_spells; ++i)
    {
        vector<spell_type> possible_spells;
        for (int s = 0; s < NUM_SPELLS; ++s)
        {
            const spell_type spell = static_cast<spell_type>(s);
            if (filter(discipline_1, discipline_2, agent, spells, spell))
                possible_spells.push_back(spell);
        }

        if (!possible_spells.size())
        {
            dprf("Couldn't find any valid spell for slot %d!", i);
            return;
        }

        spells.push_back(possible_spells[random2(possible_spells.size())]);
    }

    ASSERT(spells.size());
}

/**
 * Try to remove any discipline that's not actually being used by a given
 * randbook, setting it to the other (used) discipline.
 *
 * E.g., if a cj/ne randbook is generated with only cj spells, set discipline_2
 * to cj as well.
 *
 * @param discipline_1[in,out]      The first book discipline.
 * @param discipline_1[in,out]      The second book discipline.
 * @param spells[in]                The list of spells the book should contain.
 */
void fixup_randbook_disciplines(spschool_flag_type &discipline_1,
                                spschool_flag_type &discipline_2,
                                const vector<spell_type> &spells)
{
    bool has_d1 = false, has_d2 = false;
    for (auto spell : spells)
    {
        const spschools_type disciplines = get_spell_disciplines(spell);
        if (disciplines & discipline_1)
            has_d1 = true;
        if (disciplines & discipline_2)
            has_d2 = true;
    }

    if (has_d1 == has_d2)
        return; // both schools or neither used; can't do anything regardless

    if (has_d1)
        discipline_2 = discipline_1;
    else
        discipline_1 = discipline_2;
}

/**
 * Turn a given book into a themed spellbook.
 *
 * @param book[in,out]      The book in question.
 * @param filter            A filter specifying which spells can be included.
 * @param get_discipline    A function to choose themes for the book.
 * @param num_spells        The number of spells the book should include.
 *                          Not guaranteed, but should be fairly reliable.
 * @param owner             The name of the book's owner, if any. Cosmetic.
 * @param subject           The subject of the book, if any. Cosmetic.
 */
void build_themed_book(item_def &book, themed_spell_filter filter,
                       function<spschool_flag_type()> get_discipline,
                       int num_spells, string owner, string subject)
{
    if (num_spells < 1)
        num_spells = theme_book_size();

    spschool_flag_type discipline_1 = get_discipline();
    spschool_flag_type discipline_2 = get_discipline();

    item_source_type agent;
    if (!origin_is_acquirement(book, &agent))
        agent = (item_source_type)origin_as_god_gift(book);

    vector<spell_type> spells;
    theme_book_spells(discipline_1, discipline_2, filter, agent, num_spells,
                      spells);
    fixup_randbook_disciplines(discipline_1, discipline_2, spells);
    init_book_theme_randart(book, spells);
    name_book_theme_randart(book, discipline_1, discipline_2, owner, subject);
}

static bool _compare_spells(spell_type a, spell_type b)
{
    if (a == SPELL_NO_SPELL && b == SPELL_NO_SPELL)
        return false;
    else if (a != SPELL_NO_SPELL && b == SPELL_NO_SPELL)
        return true;
    else if (a == SPELL_NO_SPELL && b != SPELL_NO_SPELL)
        return false;

    int level_a = spell_difficulty(a);
    int level_b = spell_difficulty(b);

    if (level_a != level_b)
        return level_a < level_b;

    spschools_type schools_a = get_spell_disciplines(a);
    spschools_type schools_b = get_spell_disciplines(b);

    if (schools_a != schools_b && schools_a != SPTYP_NONE
        && schools_b != SPTYP_NONE)
    {
        const char* a_type = nullptr;
        const char* b_type = nullptr;

        // Find lowest/earliest school for each spell.
        for (const auto mask : spschools_type::range())
        {
            if (a_type == nullptr && (schools_a & mask))
                a_type = spelltype_long_name(mask);
            if (b_type == nullptr && (schools_b & mask))
                b_type = spelltype_long_name(mask);
        }
        ASSERT(a_type != nullptr);
        ASSERT(b_type != nullptr);
        return strcmp(a_type, b_type) < 0;
    }

    return strcmp(spell_title(a), spell_title(b)) < 0;
}

static void _get_spell_list(vector<spell_type> &spells, int level,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    // For randarts handed out by Sif Muna, spells contained in the
    // special books are fair game.
    // We store them in an extra vector that (once sorted) can later
    // be checked for each spell with a rarity -1 (i.e. not normally
    // appearing randomly).
    vector<spell_type> special_spells;
    if (god == GOD_SIF_MUNA)
    {
        for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        {
            const book_type book = static_cast<book_type>(i);
            if (is_rare_book(book))
            {
                for (spell_type spell : spellbook_template(book))
                {
                    if (spell_rarity(spell) != -1)
                        continue;

                    special_spells.push_back(spell);
                }
            }
        }

        sort(special_spells.begin(), special_spells.end());
    }

    int specnum = 0;
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        const spell_type spell = (spell_type) i;

        if (!is_valid_spell(spell))
            continue;

        // Only use spells available in books you might find lying about
        // the dungeon.
        if (spell_rarity(spell) == -1)
        {
            bool skip_spell = true;
            while ((unsigned int) specnum < special_spells.size()
                   && spell == special_spells[specnum])
            {
                specnum++;
                skip_spell = false;
            }

            if (skip_spell)
                continue;
        }

        if (avoid_known && you.spell_library[spell])
            continue;

        // fixed level randart: only include spells of the given level
        if (level != -1 && spell_difficulty(spell) != level)
            continue;

        if (avoid_uncastable && !you_can_memorise(spell))
        {
            uncastable_discard++;
            continue;
        }

        if (god_hates_spell(spell, god))
        {
            god_discard++;
            continue;
        }

        // Passed all tests.
        spells.push_back(spell);
    }
}

static void _make_book_randart(item_def &book)
{
    if (!is_artefact(book))
    {
        book.flags |= ISFLAG_RANDART;
        if (!book.props.exists(ARTEFACT_APPEAR_KEY))
        {
            book.props[ARTEFACT_APPEAR_KEY].get_string() =
            make_artefact_name(book, true);
        }
    }
}

/**
 * Choose an owner for a randomly-generated single-level spellbook.
 *
 * @param god       The god responsible for the book, if any.
 *                  If set, will be the book's owner.
 * @return          An owner for the book; may be the empty string.
 */
static string _gen_randlevel_owner(god_type god)
{
    if (god != GOD_NO_GOD)
        return god_name(god, false);
    if (one_chance_in(30))
        return god_name(GOD_SIF_MUNA, false);
    if (one_chance_in(3))
        return make_name();
    return "";
}

/// What's the DB lookup string for a given randbook spell level?
static string _randlevel_difficulty_name(int level)
{
    if (level == 1)
        return "starting";
    if (level <= 3 || level == 4 && coinflip())
        return "easy";
    if (level <= 6)
        return "moderate";
    return "difficult";
}

/**
 * Generate a name for a randomly-generated single-level spellbook.
 *
 * @param level     The level of the spells in the book.
 * @param god       The god responsible for the book, if any.
 * @return          A spellbook name. May contain placeholders (@foo@).
 */
static string _gen_randlevel_name(int level, god_type god)
{
    const string owner_name = _gen_randlevel_owner(god);
    const bool has_owner = !owner_name.empty();
    const string apostrophised_owner = owner_name.empty() ? "" :
    apostrophise(owner_name) + " ";

    if (god == GOD_XOM && coinflip())
    {
        const string xomname = getRandNameString("book_noun") + " of "
        + getRandNameString("Xom_book_title");
        return apostrophised_owner + xomname;
    }

    const string lookup = _randlevel_difficulty_name(level) + " level book";

    // First try for names respecting the book's previous owner/author
    // (if one exists), then check for general difficulty.
    string bookname;
    if (has_owner)
        bookname = getRandNameString(lookup + " owner");

    if (bookname.empty())
        bookname = getRandNameString(lookup);

    bookname = uppercase_first(bookname);
    if (has_owner)
    {
        if (bookname.substr(0, 4) == "The ")
            bookname = bookname.substr(4);
        else if (bookname.substr(0, 2) == "A ")
            bookname = bookname.substr(2);
        else if (bookname.substr(0, 3) == "An ")
            bookname = bookname.substr(3);
    }

    if (bookname.find("@level@", 0) != string::npos)
    {
        const string level_name = uppercase_first(number_in_words(level));
        bookname = replace_all(bookname, "@level@", level_name);
    }

    if (bookname.empty())
        bookname = getRandNameString("book");

    return apostrophised_owner + bookname;
}

/**
 * Turn the given book into a randomly-generated spellbook ("randbook"),
 * containing only spells of a given level.
 *
 * @param book[out]    The book in question.
 * @param level        The level of the spells. If -1, choose a level randomly.
 * @return             Whether the book was successfully transformed.
 */
bool make_book_level_randart(item_def &book, int level)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    const god_type god = origin_as_god_gift(book);

    const bool completely_random =
        god == GOD_XOM || (god == GOD_NO_GOD && !origin_is_acquirement(book));

    if (level == -1)
    {
        int max_level =
            (completely_random ? 9
             : min(9, you.get_experience_level()));

        level = random_range(1, max_level);
    }
    ASSERT_RANGE(level, 0 + 1, 9 + 1);

    // Book level:       1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
    // Number of spells: 5 | 5 | 5 | 6 | 6 | 6 | 4 | 2 | 1
    int num_spells = max(1, min(5 + (level - 1)/3,
                                18 - 2*level));
    ASSERT_RANGE(num_spells, 0 + 1, RANDBOOK_SIZE + 1);

    book.sub_type = BOOK_RANDART_LEVEL;
    _make_book_randart(book);

    int god_discard        = 0;
    int uncastable_discard = 0;

    vector<spell_type> spells;
    // Which spells are valid choices?
    _get_spell_list(spells, level, god, !completely_random,
                    god_discard, uncastable_discard);

    if (spells.empty())
    {
        if (level > 1)
            return make_book_level_randart(book, level - 1);
        char buf[80];

        if (god_discard > 0 && uncastable_discard == 0)
        {
            snprintf(buf, sizeof(buf), "%s disliked all level %d spells",
                     god_name(god).c_str(), level);
        }
        else if (god_discard == 0 && uncastable_discard > 0)
            sprintf(buf, "No level %d spells can be cast by you", level);
        else if (god_discard > 0 && uncastable_discard > 0)
        {
            snprintf(buf, sizeof(buf),
                     "All level %d spells are either disliked by %s "
                     "or cannot be cast by you.",
                     level, god_name(god).c_str());
        }
        else
            sprintf(buf, "No level %d spells?!?!?!", level);

        mprf(MSGCH_ERROR, "Could not create fixed level randart spellbook: %s",
             buf);

        return false;
    }
    shuffle_array(spells);

    if (num_spells > (int) spells.size())
    {
        num_spells = spells.size();
#if defined(DEBUG) || defined(DEBUG_DIAGNOSTICS)
        mprf(MSGCH_WARN, "More spells requested for fixed level (%d) "
             "randart spellbook than there are valid spells.",
             level);
        mprf(MSGCH_WARN, "Discarded %d spells due to being uncastable and "
             "%d spells due to being disliked by %s.",
             uncastable_discard, god_discard, god_name(god).c_str());
#endif
    }

    vector<bool> spell_used(spells.size(), false);
    vector<bool> avoid_memorised(spells.size(), !completely_random);
    vector<bool> avoid_seen(spells.size(), !completely_random);

    spell_type chosen_spells[RANDBOOK_SIZE];
    for (int i = 0; i < RANDBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    int book_pos = 0;
    while (book_pos < num_spells)
    {
        int spell_pos = random2(spells.size());

        if (spell_used[spell_pos])
            continue;

        spell_type spell = spells[spell_pos];
        ASSERT(spell != SPELL_NO_SPELL);

        if (avoid_memorised[spell_pos] && you.has_spell(spell))
        {
            // Only once.
            avoid_memorised[spell_pos] = false;
            continue;
        }

        if (avoid_seen[spell_pos] && you.spell_library[spell] && coinflip())
        {
            // Only once.
            avoid_seen[spell_pos] = false;
            continue;
        }

        spell_used[spell_pos]     = true;
        chosen_spells[book_pos++] = spell;
    }
    sort(chosen_spells, chosen_spells + RANDBOOK_SIZE, _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(RANDBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(RANDBOOK_SIZE);

    for (int i = 0; i < RANDBOOK_SIZE; i++)
        spell_vec[i].get_int() = chosen_spells[i];

    const string name = _gen_randlevel_name(level, god);
    set_artefact_name(book, replace_name_parts(name, book));
    // None of these books need a definite article prepended.
    book.props[BOOK_TITLED_KEY].get_bool() = true;

    return true;
}

/**
 * Initialize a themed randbook, & fill it with the given spells.
 *
 * @param book[in,out]      The book to be initialized.
 * @param spells            The spells to fill the book with.
 *                          Not passed by reference since we want to sort it.
 */
void init_book_theme_randart(item_def &book, vector<spell_type> spells)
{
    book.sub_type = BOOK_RANDART_THEME;
    _make_book_randart(book);

    spells.resize(RANDBOOK_SIZE, SPELL_NO_SPELL);
    sort(spells.begin(), spells.end(), _compare_spells);
    ASSERT(spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(RANDBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(RANDBOOK_SIZE);
    for (int i = 0; i < RANDBOOK_SIZE; i++)
        spell_vec[i].get_int() = spells[i];
}

/**
 * Generate and apply a name for a themed randbook.
 *
 * @param book[in,out]      The book to be named.
 * @param discipline_1      The first spellschool.
 * @param discipline_2      The second spellschool.
 * @param owner             The book's owner; e.g. "Xom". May be empty.
 * @param subject           The subject of the book. May be empty.
 */
void name_book_theme_randart(item_def &book, spschool_flag_type discipline_1,
                             spschool_flag_type discipline_2,
                             string owner, string subject)
{
    if (owner.empty())
    {
        const vector<spell_type> spells = spells_in_book(book);
        owner = _gen_randbook_owner(origin_as_god_gift(book), discipline_1,
                                    discipline_2, spells);
    }

    book.props[BOOK_TITLED_KEY].get_bool() = !owner.empty();
    const string name = _gen_randbook_name(subject, owner,
                                           discipline_1, discipline_2);
    set_artefact_name(book, replace_name_parts(name, book));
}

/**
 * Possibly generate a 'subject' for a book based on its owner.
 *
 * @param owner     The book's owner; e.g. "Xom".
 * @return          A random book subject, or the empty string.
 *                  May contain placeholders (@foo@).
 */
static string _maybe_gen_book_subject(string owner)
{
    // Sometimes use a completely random title.
    if (owner == "Xom" && !one_chance_in(20))
        return getRandNameString("Xom_book_title");
    if (one_chance_in(20) && (owner.empty() || one_chance_in(3)))
        return getRandNameString("random_book_title");
    return "";
}

/**
 * Generates a random, vaguely appropriate name for a randbook.
 *
 * @param   subject     The subject of the book. If non-empty, the book will
 *                      have a name of the form "[Foo] of <subject>".
 * @param   owner       The name of the book's 'owner', if any.
 *                      (E.g., Xom, Cerebov, Boris...)
 *                      Prepended to the book's name (Foo's...); "Xom" has
 *                      further effects.
 * @param   disc1       A spellschool (discipline) associated with the book.
 * @param   disc2       A spellschool (discipline) associated with the book.
 * @return              A book name. May contain placeholders (@foo@).
 */
static string _gen_randbook_name(string subject, string owner,
                                 spschool_flag_type disc1,
                                 spschool_flag_type disc2)
{
    const string apostrophised_owner = owner.empty() ?
        "" :
        apostrophise(owner) + " ";

    const string real_subject = subject.empty() ?
        _maybe_gen_book_subject(owner) :
        subject;

    if (!real_subject.empty())
    {
        return make_stringf("%s%s of %s",
                            apostrophised_owner.c_str(),
                            getRandNameString("book_noun").c_str(),
                            real_subject.c_str());
    }

    string name = apostrophised_owner;

    // Give a name that reflects the primary and secondary
    // spell disciplines of the spells contained in the book.
    name += getRandNameString("book_name") + " ";

    // For the actual name there's a 66% chance of getting something like
    //  <book> of the Fiery Traveller (Translocation/Fire), else
    //  <book> of Displacement and Flames.
    string type_name;
    if (disc1 != disc2 && !one_chance_in(3))
    {
        string lookup = spelltype_long_name(disc2);
        type_name = getRandNameString(lookup + " adj");
    }

    if (type_name.empty())
    {
        // No adjective found, use the normal method of combining two nouns.
        type_name = getRandNameString(spelltype_long_name(disc1));
        if (type_name.empty())
            name += spelltype_long_name(disc1);
        else
            name += type_name;

        if (disc1 != disc2)
        {
            name += " and ";
            type_name = getRandNameString(spelltype_long_name(disc2));

            if (type_name.empty())
                name += spelltype_long_name(disc2);
            else
                name += type_name;
        }
    }
    else
    {
        string bookname = type_name + " ";

        // Add the noun for the first discipline.
        type_name = getRandNameString(spelltype_long_name(disc1));
        if (type_name.empty())
            bookname += spelltype_long_name(disc1);
        else
        {
            if (type_name.find("the ", 0) != string::npos)
            {
                type_name = replace_all(type_name, "the ", "");
                bookname = "the " + bookname;
            }
            bookname += type_name;
        }
        name += bookname;
    }

    return name;
}

/**
 * Possibly choose a random 'owner' for a themed random spellbook.
 *
 * @param god           The god responsible for gifting the book, if any.
 * @param disc1         A spellschool (discipline) associated with the book.
 * @param disc2         A spellschool (discipline) associated with the book.
 * @param spells        The spells in the book.
 * @return              The name of the book's 'owner', or the empty string.
 */
static string _gen_randbook_owner(god_type god, spschool_flag_type disc1,
                                  spschool_flag_type disc2,
                                  const vector<spell_type> &spells)
{
    // If the owner hasn't been set already use
    // a) the god's name for god gifts (only applies to Sif Muna and Xom),
    // b) a name depending on the spell disciplines, for pure books
    // c) a random name (all god gifts not named earlier)
    // d) an applicable god's name
    // ... else leave it unnamed (around 57% chance for non-god gifts)

    int highest_level = 0;
    int lowest_level = 27;
    bool all_spells_disc1 = true;
    for (auto spell : spells)
    {
        const int level = spell_difficulty(spell);
        highest_level = max(level, highest_level);
        lowest_level = min(level, lowest_level);

        if (!(get_spell_disciplines(spell) & disc1))
            all_spells_disc1 = false;
    }

    // this logic is very odd...
    const bool highlevel = highest_level >= 7 + random2(3)
                           && (lowest_level > 1 || coinflip());


    // name of gifting god?
    const bool god_gift = god != GOD_NO_GOD;
    if (god_gift && !one_chance_in(4))
        return god_name(god, false);

    // thematically appropriate name?
    if (god_gift && one_chance_in(3) || one_chance_in(5))
    {
        vector<string> lookups;
        const string d1_name = spelltype_long_name(disc1);

        if (disc1 != disc2)
        {
            const string lookup = d1_name + " " + spelltype_long_name(disc2);
            if (highlevel)
                lookups.push_back("highlevel " + lookup + " owner");
            lookups.push_back(lookup + " owner");
        }

        if (all_spells_disc1)
        {
            if (highlevel)
                lookups.push_back("highlevel " + d1_name + " owner");
            lookups.push_back(d1_name + "owner");
        }

        for (string &lookup : lookups)
        {
            const string owner = getRandNameString(lookup);
            if (!owner.empty() && owner != "__NONE")
                return owner;
        }
    }

    // random name?
    if (god_gift || one_chance_in(5))
        return make_name();

    // applicable god's name?
    if (!god_gift && one_chance_in(9))
    {
        switch (disc1)
        {
            case SPTYP_NECROMANCY:
                if (all_spells_disc1 && !one_chance_in(6))
                    return god_name(GOD_KIKUBAAQUDGHA, false);
                break;
            case SPTYP_CONJURATION:
                if (all_spells_disc1 && !one_chance_in(4))
                    return god_name(GOD_VEHUMET, false);
                break;
            default:
                break;
        }
        return god_name(GOD_SIF_MUNA, false);
    }

    return "";
}

// Give Roxanne a randart spellbook of the disciplines Transmutations/Earth
// that includes Statue Form and is named after her.
void make_book_roxanne_special(item_def *book)
{
    spschool_flag_type disc = random_choose(SPTYP_TRANSMUTATION, SPTYP_EARTH);
    vector<spell_type> forced_spell = {SPELL_STATUE_FORM};
    build_themed_book(*book,
                      forced_spell_filter(forced_spell,
                                           capped_spell_filter(19)),
                      forced_book_theme(disc), 5, "Roxanne");
}

void make_book_kiku_gift(item_def &book, bool first)
{
    book.sub_type = BOOK_RANDART_THEME;
    _make_book_randart(book);

    spell_type chosen_spells[RANDBOOK_SIZE];
    for (int i = 0; i < RANDBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    if (first)
    {
        bool can_bleed = you.species != SP_GARGOYLE
            && you.species != SP_GHOUL
            && you.species != SP_MUMMY;
        bool can_regen = you.species != SP_DEEP_DWARF
            && you.species != SP_MUMMY;
        bool pain = coinflip();

        chosen_spells[0] = pain ? SPELL_PAIN : SPELL_ANIMATE_SKELETON;
        chosen_spells[1] = SPELL_CORPSE_ROT;
        chosen_spells[2] = (can_bleed ? SPELL_SUBLIMATION_OF_BLOOD
                            : pain ? SPELL_ANIMATE_SKELETON
                            : SPELL_PAIN);
        chosen_spells[3] = (!can_regen || coinflip())
            ? SPELL_VAMPIRIC_DRAINING : SPELL_REGENERATION;
        chosen_spells[4] = SPELL_CONTROL_UNDEAD;

    }
    else
    {
        chosen_spells[0] = coinflip() ? SPELL_ANIMATE_DEAD
            : SPELL_CIGOTUVIS_EMBRACE;
        chosen_spells[1] = (you.species == SP_FELID || coinflip())
            ? SPELL_AGONY : SPELL_EXCRUCIATING_WOUNDS;
        chosen_spells[2] = random_choose(SPELL_BOLT_OF_DRAINING,
                                         SPELL_SIMULACRUM,
                                         SPELL_DEATH_CHANNEL);
        spell_type extra_spell;
        do
        {
            extra_spell = random_choose(SPELL_ANIMATE_DEAD,
                                        SPELL_CIGOTUVIS_EMBRACE,
                                        SPELL_AGONY,
                                        SPELL_EXCRUCIATING_WOUNDS,
                                        SPELL_BOLT_OF_DRAINING,
                                        SPELL_SIMULACRUM,
                                        SPELL_DEATH_CHANNEL);
            if (you.species == SP_FELID && extra_spell == SPELL_EXCRUCIATING_WOUNDS)
                extra_spell = SPELL_NO_SPELL;
            for (int i = 0; i < 3; i++)
                if (extra_spell == chosen_spells[i])
                    extra_spell = SPELL_NO_SPELL;
        }
        while (extra_spell == SPELL_NO_SPELL);
        chosen_spells[3] = extra_spell;
        chosen_spells[4] = SPELL_DISPEL_UNDEAD;
    }

    sort(chosen_spells, chosen_spells + RANDBOOK_SIZE, _compare_spells);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(RANDBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(RANDBOOK_SIZE);

    for (int i = 0; i < RANDBOOK_SIZE; i++)
        spell_vec[i].get_int() = chosen_spells[i];

    string name = "Kikubaaqudgha's ";
    book.props[BOOK_TITLED_KEY].get_bool() = true;
    name += getRandNameString("book_name") + " ";
    string type_name = getRandNameString("Necromancy");
    if (type_name.empty())
        name += "Necromancy";
    else
        name += type_name;
    set_artefact_name(book, name);
}

/// Does the given acq source generate books totally randomly?
static bool _completely_random_books(int agent)
{
    // only acq & god gifts from sane gods weight spells/disciplines
    // for player utility.
    return agent == GOD_XOM || agent == GOD_NO_GOD;
}

/// How desireable is the given spell for inclusion in an acquired randbook?
static int _randbook_spell_weight(spell_type spell, int agent)
{
    if (_completely_random_books(agent))
        return 1;

    // prefer unseen spells
    const int seen_weight = you.spell_library[spell] ? 1 : 4;

    // prefer spells roughly approximating the player's overall spellcasting
    // ability (?????)
    const int Spc = div_rand_round(you.skill(SK_SPELLCASTING, 256, true), 256);
    const int difficult_weight = 5 - abs(3 * spell_difficulty(spell) - Spc) / 7;

    // prefer spells in disciplines the player is skilled with
    const spschools_type disciplines = get_spell_disciplines(spell);
    int total_skill = 0;
    int num_skills  = 0;
    for (const auto disc : spschools_type::range())
    {
        if (disciplines & disc)
        {
            const skill_type sk = spell_type2skill(disc);
            total_skill += div_rand_round(you.skill(sk, 256, true), 256);
            num_skills++;
        }
    }
    int skill_weight = 1;
    if (num_skills > 0)
        skill_weight = (2 + (total_skill / num_skills)) / 3;
    skill_weight = max(1, skill_weight);

    const int weight = seen_weight * skill_weight * difficult_weight;
    ASSERT(weight > 0);
    return weight;
    /// XXX: I'm not sure how much impact all this actually has.
}

typedef map<spell_type, int> weighted_spells;

/**
 * Populate a list of possible spells to be included in acquired books,
 * weighted by desireability.
 *
 * @param possible_spells[out]  The list to be populated.
 * @param agent         The entity creating the item; possibly a god.
 */
static void _get_weighted_randbook_spells(weighted_spells &possible_spells,
                                          int agent)
{
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        const spell_type spell = (spell_type) i;

        if (!is_valid_spell(spell)
            || !_agent_spell_filter(agent, spell)
            || !you_can_memorise(spell))
        {
            continue;
        }

        // Passed all tests.
        const int weight = _randbook_spell_weight(spell, agent);
        possible_spells[spell] = weight;
    }
}

/**
 * Choose a spell discipline for a randbook, weighted by the the value of all
 * possible spells in that discipline.
 *
 * @param possibles     A weighted list of all possible spells to include in
 *                      the book.
 * @param agent         The entity creating the item; possibly a god.
 * @return              An appropriate spell school; e.g. SPTYP_FIRE.
 */
static spschool_flag_type _choose_randbook_discipline(weighted_spells
                                                      &possible_spells,
                                                      int agent)
{
    map<spschool_flag_type, int> discipline_weights;
    for (auto weighted_spell : possible_spells)
    {
        const spell_type spell = weighted_spell.first;
        const int weight = weighted_spell.second;
        const spschools_type disciplines = get_spell_disciplines(spell);
        for (const auto disc : spschools_type::range())
        {
            if (disciplines & disc)
            {
                if (_completely_random_books(agent))
                    discipline_weights[disc] = 1;
                else
                    discipline_weights[disc] += weight;
            }
        }
    }

    const spschool_flag_type *discipline
        = random_choose_weighted(discipline_weights);
    ASSERT(discipline);
    return *discipline;
}

/**
 * From a given weighted list of possible spells, choose a set to include in
 * a randbook, filtered by discipline.
 *
 * @param[in,out] possible_spells   All possible spells, weighted by value.
 Modified in-place for efficiency.
 * @param discipline_1              The first spellschool.
 * @param discipline_2              The second spellschool.
 * @param size                      The number of spells to include.
 * @param[out] spells               The chosen spells.
 */
static void _choose_themed_randbook_spells(weighted_spells &possible_spells,
                                           spschool_flag_type discipline_1,
                                           spschool_flag_type discipline_2,
                                           int size, vector<spell_type> &spells)
{
    for (auto &weighted_spell : possible_spells)
    {
        const spell_type spell = weighted_spell.first;
        const spschools_type disciplines = get_spell_disciplines(spell);
        if (!(disciplines & discipline_1) && !(disciplines & discipline_2))
            weighted_spell.second = 0; // filter it out
    }

    for (int i = 0; i < size; ++i)
    {
        const spell_type *spell = random_choose_weighted(possible_spells);
        ASSERT(spell);
        spells.push_back(*spell);
        possible_spells[*spell] = 0; // don't choose the same one twice!
    }
}

/**
 * Turn a given book into an acquirement-quality themed spellbook.
 *
 * @param book[out]     The book to be turned into a randbook.
 * @param agent         The entity creating the item; possibly a god.
 */
void acquire_themed_randbook(item_def &book, int agent)
{
    weighted_spells possible_spells;
    _get_weighted_randbook_spells(possible_spells, agent);

    // include 2-8 spells in the book, leaning heavily toward 5
    const int size = min(2 + random2avg(7, 3),
                         (int)possible_spells.size());
    ASSERT(size);

    // XXX: we could cache this...
    spschool_flag_type discipline_1
        = _choose_randbook_discipline(possible_spells, agent);
    spschool_flag_type discipline_2
        = _choose_randbook_discipline(possible_spells, agent);

    vector<spell_type> spells;
    _choose_themed_randbook_spells(possible_spells, discipline_1, discipline_2,
                                   size, spells);

    fixup_randbook_disciplines(discipline_1, discipline_2, spells);

    // Acquired randart books have a chance of being named after the player.
    const string owner = agent == AQ_SCROLL && one_chance_in(12) ?
        you.your_name :
        "";

    init_book_theme_randart(book, spells);
    name_book_theme_randart(book, discipline_1, discipline_2, owner);
}

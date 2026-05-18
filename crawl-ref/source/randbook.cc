/**
 * @file
 * @brief Functions for generating random spellbooks.
 **/

#include "AppHdr.h"

#include "mpr.h"
#include "randbook.h"

#include <functional>

#include "artefact.h"
#include "database.h"
#include "english.h"
#include "item-name.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "religion.h"
#include "spl-book.h"
#include "stringutil.h"

static string _gen_randbook_name(string subject, string owner,
                                 spschool disc1, spschool disc2);
static string _gen_randbook_owner(god_type god, spschool disc1,
                                  spschool disc2,
                                  const vector<spell_type> &spells);

/// How many spells should be in a random theme book?
int theme_book_size() { return random2avg(4, 3) + 2; }

/// A discipline chooser that only ever returns the given discipline.
function<spschool()> forced_book_theme(spschool theme)
{
    return [theme]() { return theme; };
}

/// Choose a random valid discipline for a themed randbook.
spschool random_book_theme()
{
    vector<spschool> disciplines;
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
spschool matching_book_theme(const vector<spell_type> &forced_spells)
{
    map<spschool, int> seen_disciplines;
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
        const spschool *discipline
            = random_choose_weighted(seen_disciplines);
        if (discipline)
            return *discipline;
        return random_book_theme();
    }

    for (auto seen : seen_disciplines)
        seen.second = seen.second == (int)forced_spells.size() ? 1 : 0;
    const spschool *discipline
        = random_choose_weighted(seen_disciplines);
    ASSERT(discipline);
    return *discipline;
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
    // Only use actual player spells.
    if (!is_player_book_spell(spell))
        return false;

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
bool basic_themed_spell_filter(spschool discipline_1,
                               spschool discipline_2,
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

    return [max_levels, subfilter](spschool discipline_1,
                                   spschool discipline_2,
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
    return [&forced_spells, subfilter](spschool discipline_1,
                                       spschool discipline_2,
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
void theme_book_spells(spschool discipline_1,
                       spschool discipline_2,
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
void fixup_randbook_disciplines(spschool &discipline_1,
                                spschool &discipline_2,
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
                       function<spschool()> get_discipline,
                       int num_spells, string owner, string subject)
{
    if (num_spells < 1)
        num_spells = theme_book_size();

    spschool discipline_1 = get_discipline();
    spschool discipline_2 = get_discipline();

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

    if (schools_a != schools_b && schools_a != spschool::none
        && schools_b != spschool::none)
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

static void _make_book_randart(item_def &book)
{
    if (is_artefact(book))
        return;

    book.flags |= ISFLAG_RANDART;
    book.flags |= ISFLAG_IDENTIFIED;
    if (!book.props.exists(ARTEFACT_APPEAR_KEY))
    {
        const string name = make_artefact_name(book, true);
        book.props[ARTEFACT_APPEAR_KEY].get_string() = name;
    }
}

void _set_book_spell_list(item_def &book, vector<spell_type> spells)
{
    ASSERT(!spells.empty());
    sort(begin(spells), end(spells), _compare_spells);
    spells.resize(RANDBOOK_SIZE, SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(RANDBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(RANDBOOK_SIZE);

    for (int i = 0; i < RANDBOOK_SIZE; i++)
        spell_vec[i].get_int() = spells[i];
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
    _set_book_spell_list(book, std::move(spells));
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
void name_book_theme_randart(item_def &book, spschool discipline_1,
                             spschool discipline_2,
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
                                 spschool disc1,
                                 spschool disc2)
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
static string _gen_randbook_owner(god_type god, spschool disc1,
                                  spschool disc2,
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
            case spschool::necromancy:
                if (all_spells_disc1 && !one_chance_in(6))
                    return god_name(GOD_KIKUBAAQUDGHA, false);
                break;
            case spschool::conjuration:
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

/// Does the given acq source generate books totally randomly?
static bool _completely_random_books(int agent)
{
    // only acq & god gifts from sane gods weight spells/disciplines
    // for player utility.
    return agent == GOD_XOM || agent == GOD_NO_GOD;
}

/// How desirable is the given spell for inclusion in an acquired randbook?
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
 * @return              An appropriate spell school; e.g. spschool::fire.
 */
static spschool _choose_randbook_discipline(weighted_spells
                                                      &possible_spells,
                                                      int agent)
{
    map<spschool, int> discipline_weights;
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

    const spschool *discipline
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
                                           spschool discipline_1,
                                           spschool discipline_2,
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
        if (!spell)
            break;
        spells.push_back(*spell);
        possible_spells[*spell] = 0; // don't choose the same one twice!
    }
    // `size` is guaranteed to be >0 by an ASSERT in the calling function
    ASSERT(spells.size() > 0);
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
    spschool discipline_1
        = _choose_randbook_discipline(possible_spells, agent);
    spschool discipline_2
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

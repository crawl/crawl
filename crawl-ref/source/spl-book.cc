/**
 * @file
 * @brief Spellbook contents array and management functions
**/

#include "AppHdr.h"

#include "spl-book.h"
#include "book-data.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <unordered_set>

#include "artefact.h"
#include "colour.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "describe-spells.h"
#include "end.h"
#include "english.h"
#include "god-conduct.h"
#include "god-item.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "unicode.h"

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

typedef vector<spell_type>                     spell_list;
typedef unordered_set<spell_type, hash<int>>   spell_set;
static spell_type _choose_mem_spell(spell_list &spells, unsigned int num_misc);

static const map<wand_type, spell_type> _wand_spells =
{
    { WAND_FLAME, SPELL_THROW_FLAME },
    { WAND_PARALYSIS, SPELL_PARALYSE },
    { WAND_CONFUSION, SPELL_CONFUSE },
    { WAND_DIGGING, SPELL_DIG },
    { WAND_ICEBLAST, SPELL_ICEBLAST },
    { WAND_LIGHTNING, SPELL_LIGHTNING_BOLT },
    { WAND_POLYMORPH, SPELL_POLYMORPH },
    { WAND_ENSLAVEMENT, SPELL_ENSLAVEMENT },
    { WAND_ACID, SPELL_CORROSIVE_BOLT },
    { WAND_DISINTEGRATION, SPELL_DISINTEGRATE },
    { WAND_CLOUDS, SPELL_CLOUD_CONE },
    { WAND_SCATTERSHOT, SPELL_SCATTERSHOT },
    { WAND_RANDOM_EFFECTS, SPELL_RANDOM_EFFECTS },
};


spell_type spell_in_wand(wand_type wand)
{
    if (item_type_removed(OBJ_WANDS, wand))
        return SPELL_NO_SPELL;

    if (const spell_type* const spl = map_find(_wand_spells, wand))
        return *spl;

    die("Unknown wand: %d", wand);
}

vector<spell_type> spells_in_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    vector<spell_type> ret;
    const CrawlHashTable &props = book.props;
    if (!props.exists(SPELL_LIST_KEY))
        return spellbook_template(static_cast<book_type>(book.sub_type));

    const CrawlVector &spells = props[SPELL_LIST_KEY].get_vector();

    ASSERT(spells.get_type() == SV_INT);
    ASSERT(spells.size() == RANDBOOK_SIZE);

    for (int spell : spells)
        //TODO: don't put SPELL_NO_SPELL in them in the first place.
        if (spell != SPELL_NO_SPELL)
            ret.emplace_back(static_cast<spell_type>(spell));

    return ret;
}

vector<spell_type> spellbook_template(book_type book)
{
    ASSERT_RANGE(book, 0, (int)ARRAYSZ(spellbook_templates));
    return spellbook_templates[book];
}

// Rarity 100 is reserved for unused books.
int book_rarity(book_type which_book)
{
    if (item_type_removed(OBJ_BOOKS, which_book))
        return 100;

    switch (which_book)
    {
    case BOOK_MINOR_MAGIC:
    case BOOK_MISFORTUNE:
    case BOOK_CANTRIPS:
        return 1;

    case BOOK_CHANGES:
    case BOOK_MALEDICT:
        return 2;

    case BOOK_CONJURATIONS:
    case BOOK_NECROMANCY:
    case BOOK_CALLINGS:
        return 3;

    case BOOK_FLAMES:
    case BOOK_FROST:
    case BOOK_AIR:
    case BOOK_GEOMANCY:
        return 4;

    case BOOK_YOUNG_POISONERS:
    case BOOK_BATTLE:
    case BOOK_DEBILITATION:
        return 5;

    case BOOK_CLOUDS:
    case BOOK_POWER:
        return 6;

    case BOOK_ENCHANTMENTS:
    case BOOK_PARTY_TRICKS:
        return 7;

    case BOOK_TRANSFIGURATIONS:
    case BOOK_BEASTS:
        return 8;

    case BOOK_FIRE:
    case BOOK_ICE:
    case BOOK_SKY:
    case BOOK_EARTH:
    case BOOK_UNLIFE:
    case BOOK_SPATIAL_TRANSLOCATIONS:
        return 10;

    case BOOK_TEMPESTS:
    case BOOK_DEATH:
    case BOOK_SUMMONINGS:
        return 11;

    case BOOK_BURGLARY:
    case BOOK_ALCHEMY:
    case BOOK_DREAMS:
    case BOOK_FEN:
        return 12;

    case BOOK_WARP:
    case BOOK_DRAGON:
        return 15;

    case BOOK_ANNIHILATIONS:
    case BOOK_GRAND_GRIMOIRE:
    case BOOK_NECRONOMICON:  // Kikubaaqudgha special
    case BOOK_MANUAL:
        return 20;

    default:
        return 1;
    }
}

static uint8_t _lowest_rarity[NUM_SPELLS];

static const set<book_type> rare_books =
{
    BOOK_ANNIHILATIONS, BOOK_GRAND_GRIMOIRE, BOOK_NECRONOMICON,
};

bool is_rare_book(book_type type)
{
    return rare_books.find(type) != rare_books.end();
}

void init_spell_rarities()
{
    for (int i = 0; i < NUM_SPELLS; ++i)
        _lowest_rarity[i] = 255;

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        const book_type book = static_cast<book_type>(i);
        // Manuals and books of destruction are not even part of this loop.
        if (is_rare_book(book))
            continue;

#ifdef DEBUG
        spell_type last = SPELL_NO_SPELL;
#endif
        for (spell_type spell : spellbook_template(book))
        {
#ifdef DEBUG
            ASSERT(spell != SPELL_NO_SPELL);
            if (last != SPELL_NO_SPELL
                && spell_difficulty(last) > spell_difficulty(spell))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' has spells out of level order "
                    "('%s' is before '%s')",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(last),
                    spell_title(spell));
            }
            last = spell;

            unsigned int flags = get_spell_flags(spell);

            if (flags & (SPFLAG_MONSTER | SPFLAG_TESTING))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' contains invalid spell "
                             "'%s'",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(spell));
            }
#endif

            const int rarity = book_rarity(book);
            if (rarity < _lowest_rarity[spell])
                _lowest_rarity[spell] = rarity;
        }
    }
}

bool is_player_spell(spell_type which_spell)
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        for (spell_type spell : spellbook_template(static_cast<book_type>(i)))
            if (spell == which_spell)
                return true;
    return false;
}

int spell_rarity(spell_type which_spell)
{
    const int rarity = _lowest_rarity[which_spell];

    if (rarity == 255)
        return -1;

    return rarity;
}

void mark_had_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!item_is_spellbook(book))
        return;

    for (spell_type stype : spells_in_book(book))
        you.seen_spell.set(stype);

    if (!book.props.exists(SPELL_LIST_KEY))
        mark_had_book(static_cast<book_type>(book.sub_type));
}

void mark_had_book(book_type booktype)
{
    ASSERT_RANGE(booktype, 0, MAX_FIXED_BOOK + 1);

    you.had_book.set(booktype);
}

void read_book(item_def &book)
{
    clrscr();
    describe_item(book);
    redraw_screen();
}

/**
 * Is the player ever allowed to memorise the given spell? (Based on race, not
 * spell slot restrictions, etc)
 *
 * @param spell     The type of spell in question.
 * @return          Whether the the player is allowed to memorise the spell.
 */
bool you_can_memorise(spell_type spell)
{
    return !spell_is_useless(spell, false, true);
}

bool player_can_memorise(const item_def &book)
{
    if (!item_is_spellbook(book) || !player_spell_levels())
        return false;

    for (spell_type stype : spells_in_book(book))
    {
        // Easiest spell already too difficult?
        if (spell_difficulty(stype) > you.experience_level
            || player_spell_levels() < spell_levels_required(stype))
        {
            return false;
        }

        if (!you.has_spell(stype))
            return true;
    }
    return false;
}

/**
 * List all spells in the given book.
 *
 * @param book      The book to be read.
 * @param spells    The list of spells to populate. Doesn't have to be empty.
 * @return          Whether the given book is invalid (empty).
 */
static bool _get_book_spells(const item_def& book, spell_set &spells)
{
    int num_spells = 0;
    for (spell_type spell : spells_in_book(book))
    {
        num_spells++;
        spells.insert(spell);
    }

    if (num_spells == 0)
    {
        mprf(MSGCH_ERROR, "Spellbook \"%s\" contains no spells! Please "
             "file a bug report.", book.name(DESC_PLAIN).c_str());
        return true;
    }

    return false;
}

/**
 * Populate the given list with all spells the player can currently memorize,
 * from library or Vehumet. Does not filter by currently known spells, spell
 * levels, etc.
 *
 * @param available_spells  A list to be populated with available spells.
 */
static void _list_available_spells(spell_set &available_spells)
{
    for (spell_type st = SPELL_NO_SPELL; st < NUM_SPELLS; st++)
    {
        if(you.spell_library[st])
            available_spells.insert(st);
    }

    // Handle Vehumet gifts
    for (auto gift : you.vehumet_gifts)
        available_spells.insert(gift);
}

/**
 * Take a list of spells and filter them to only those that the player can
 * currently memorize.
 *
 * @param available_spells      The list of spells to be filtered.
 * @param mem_spells[out]       The list of memorizeable spells to populate.
 * @param num_misc[out]         How many spells are unmemorizable for 'misc
 *                              reasons' (e.g. player species)
 * @param return                The reason the player can't currently memorize
 *                              any spells, if they can't. Otherwise, "".
 */
static string _filter_memorizable_spells(const spell_set &available_spells,
                                         spell_list &mem_spells,
                                         unsigned int &num_misc)
{
    unsigned int num_known      = 0;
                 num_misc       = 0;
    unsigned int num_restricted = 0;
    unsigned int num_low_xl     = 0;
    unsigned int num_low_levels = 0;
    unsigned int num_memable    = 0;

    for (const spell_type spell : available_spells)
    {
        if (you.has_spell(spell))
            num_known++;
        else if (!you_can_memorise(spell))
        {
            if (cannot_use_schools(get_spell_disciplines(spell)))
                num_restricted++;
            else
                num_misc++;
        }
        else
        {
            mem_spells.push_back(spell);

            const int avail_slots = player_spell_levels();

            // don't filter out spells that are too high-level for us; we
            // probably still want to see them. (since that's temporary.)

            if (spell_difficulty(spell) > you.experience_level)
                num_low_xl++;
            else if (avail_slots < spell_levels_required(spell))
                num_low_levels++;
            else
                num_memable++;
        }
    }

    if (num_memable || num_low_levels > 0 || num_low_xl > 0)
        return "";

    unsigned int total = num_known + num_misc + num_low_xl + num_low_levels
                         + num_restricted;

    if (num_known == total)
        return "You already know all available spells.";
    if (num_restricted == total || num_restricted + num_known == total)
    {
        return "You cannot currently memorise any of the available spells "
               "because you cannot use those schools of magic.";
    }
    if (num_misc == total || (num_known + num_misc) == total
        || num_misc + num_known + num_restricted == total)
    {
        return "You cannot memorise any of the available spells.";
    }

    return "You can't memorise any new spells for an unknown reason; please "
           "file a bug report.";
}


static void _get_mem_list(spell_list &mem_spells,
                          unsigned int &num_misc,
                          bool just_check = false)
{
    spell_set     available_spells;
    _list_available_spells(available_spells);

    if (available_spells.empty())
    {
        if (!just_check)
        {
            mprf(MSGCH_PROMPT, "Your library has no spells.");
        }
        return;
    }

    const string unavail_reason = _filter_memorizable_spells(available_spells,
                                                             mem_spells,
                                                             num_misc);
    if (!just_check && !unavail_reason.empty())
        mprf(MSGCH_PROMPT, "%s", unavail_reason.c_str());
}

/// Give the player a memorization prompt for the spells from the given book.
void learn_spell_from(const item_def &book)
{
    spell_set available_spells;
    const bool book_error = _get_book_spells(book, available_spells);
    if (book_error)
        more();
    if (available_spells.empty())
        return;

    spell_list mem_spells;
    unsigned int num_misc;
    const string unavail_reason = _filter_memorizable_spells(available_spells,
                                                             mem_spells,
                                                             num_misc);
    if (!unavail_reason.empty())
        mprf(MSGCH_PROMPT, "%s", unavail_reason.c_str());
    if (mem_spells.empty())
        return;

    const spell_type specspell = _choose_mem_spell(mem_spells, num_misc);

    if (specspell == SPELL_NO_SPELL)
        canned_msg(MSG_OK);
    else
        learn_spell(specspell);
}

bool has_spells_to_memorise(bool silent)
{
    spell_list      mem_spells;
    unsigned int    num_misc;

    _get_mem_list(mem_spells, num_misc, silent);
    return !mem_spells.empty();
}

static bool _sort_mem_spells(spell_type a, spell_type b)
{
    // List the Vehumet gifts at the very top.
    bool offering_a = vehumet_is_offering(a);
    bool offering_b = vehumet_is_offering(b);
    if (offering_a != offering_b)
        return offering_a;

    // List spells we can memorise right away first.
    if (player_spell_levels() >= spell_levels_required(a)
        && player_spell_levels() < spell_levels_required(b))
    {
        return true;
    }
    else if (player_spell_levels() < spell_levels_required(a)
             && player_spell_levels() >= spell_levels_required(b))
    {
        return false;
    }

    // Don't sort by failure rate beyond what the player can see in the
    // success descriptions.
    const int fail_rate_a = failure_rate_to_int(raw_spell_fail(a));
    const int fail_rate_b = failure_rate_to_int(raw_spell_fail(b));
    if (fail_rate_a != fail_rate_b)
        return fail_rate_a < fail_rate_b;

    if (spell_difficulty(a) != spell_difficulty(b))
        return spell_difficulty(a) < spell_difficulty(b);

    return strcasecmp(spell_title(a), spell_title(b)) < 0;
}

vector<spell_type> get_mem_spell_list()
{
    spell_list      mem_spells;
    unsigned int    num_misc;
    _get_mem_list(mem_spells, num_misc);

    if (mem_spells.empty())
        return spell_list();

    sort(mem_spells.begin(), mem_spells.end(), _sort_mem_spells);

    return mem_spells;
}

static spell_type _choose_mem_spell(spell_list &spells,
                                    unsigned int num_misc)
{
    sort(spells.begin(), spells.end(), _sort_mem_spells);

#ifdef USE_TILE_LOCAL
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                    | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                    text_only);
#ifdef USE_TILE_LOCAL
    // [enne] Hack. Use a separate title, so the column headers are aligned.
    spell_menu.set_title(
        new MenuEntry(" Your Spells - Memorisation  (toggle to descriptions with '!')",
            MEL_TITLE));

    spell_menu.set_title(
        new MenuEntry(" Your Spells - Descriptions  (toggle to memorisation with '!')",
            MEL_TITLE), false);

    {
        MenuEntry* me =
            new MenuEntry("     Spells                        Type          "
                          "                Failure  Level",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new MenuEntry("     Spells (Memorisation)         Type          "
                      "                Failure  Level",
            MEL_TITLE));

    spell_menu.set_title(
        new MenuEntry("     Spells (Description)          Type          "
                      "                Failure  Level",
            MEL_TITLE), false);
#endif

    spell_menu.set_highlighter(nullptr);
    spell_menu.set_tag("spell");

    spell_menu.action_cycle = Menu::CYCLE_TOGGLE;
    spell_menu.menu_action  = Menu::ACT_EXECUTE;

    string more_str = make_stringf("<lightgreen>%d spell level%s left"
                                   "<lightgreen>",
                                   player_spell_levels(),
                                   (player_spell_levels() > 1
                                    || player_spell_levels() == 0) ? "s" : "");

    if (num_misc > 0)
    {
        more_str += make_stringf(", <lightred>%u spell%s unmemorisable"
                                 "</lightred>",
                                 num_misc,
                                 num_misc > 1 ? "s" : "");
    }

#ifndef USE_TILE_LOCAL
    // Tiles menus get this information in the title.
    more_str += "   Toggle display with '<w>!</w>'";
#endif

    spell_menu.set_more(formatted_string::parse_string(more_str));

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (spells.size() > 52
        && (spell_menu.maxpagesize() > 52 || spell_menu.maxpagesize() == 0))
    {
        spell_menu.set_maxpagesize(52);
    }

    for (unsigned int i = 0; i < spells.size(); i++)
    {
        const spell_type spell = spells[i];

        ostringstream desc;

        int colour = LIGHTGRAY;
        if (vehumet_is_offering(spell))
            colour = LIGHTBLUE;
        // Grey out spells for which you lack experience or spell levels.
        else if (spell_difficulty(spell) > you.experience_level
                 || player_spell_levels() < spell_levels_required(spell))
            colour = DARKGRAY;
        else
            colour = spell_highlight_by_utility(spell);

        desc << "<" << colour_to_str(colour) << ">";

        desc << left;
        desc << chop_string(spell_title(spell), 30);
        desc << spell_schools_string(spell);

        int so_far = strwidth(desc.str()) - (colour_to_str(colour).length()+2);
        if (so_far < 60)
            desc << string(60 - so_far, ' ');
        desc << "</" << colour_to_str(colour) << ">";

        colour = failure_rate_colour(spell);
        desc << "<" << colour_to_str(colour) << ">";
        desc << chop_string(failure_rate_to_string(raw_spell_fail(spell)), 12);
        desc << "</" << colour_to_str(colour) << ">";
        desc << spell_difficulty(spell);

        MenuEntry* me =
            new MenuEntry(desc.str(), MEL_ITEM, 1,
                          index_to_letter(i % 52));

#ifdef USE_TILE
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif

        me->data = &spells[i];
        spell_menu.add_entry(me);
    }

    while (true)
    {
        vector<MenuEntry*> sel = spell_menu.show();

        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();

        if (sel.empty())
            return SPELL_NO_SPELL;

        ASSERT(sel.size() == 1);

        const spell_type spell = *static_cast<spell_type*>(sel[0]->data);
        ASSERT(is_valid_spell(spell));

        if (spell_menu.menu_action == Menu::ACT_EXAMINE)
            describe_spell(spell, nullptr);
        else
            return spell;
    }
}

bool can_learn_spell(bool silent)
{
    if (you.duration[DUR_BRAINLESS])
    {
        if (!silent)
            mpr("Your brain is not functional enough to learn spells.");
        return false;
    }

    if (you.confused())
    {
        if (!silent)
            canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (you.berserk())
    {
        if (!silent)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    return true;
}

bool learn_spell()
{
    if (!can_learn_spell())
        return false;

    spell_list      mem_spells;
    unsigned int num_misc;
    _get_mem_list(mem_spells, num_misc);

    if (mem_spells.empty())
        return false;

    spell_type specspell = _choose_mem_spell(mem_spells, num_misc);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg(MSG_OK);
        return false;
    }

    return learn_spell(specspell);
}

/**
 * Why can't the player memorise the given spell?
 *
 * @param spell     The spell in question.
 * @return          A string describing (one of) the reason(s) the player
 *                  can't memorise this spell.
 */
string desc_cannot_memorise_reason(spell_type spell)
{
    return spell_uselessness_reason(spell, false, true);
}

/**
 * Can the player learn the given spell?
 *
 * @param   specspell  The spell to be learned.
 * @param   wizard     Whether to skip some checks for wizmode memorisation.
 * @return             false if the player can't learn the spell for any
 *                     reason, true otherwise.
*/
static bool _learn_spell_checks(spell_type specspell, bool wizard = false)
{
    if (spell_removed(specspell))
    {
        mpr("Sorry, this spell is gone!");
        return false;
    }

    if (!wizard && !can_learn_spell())
        return false;

    if (already_learning_spell((int) specspell))
        return false;

    if (!you_can_memorise(specspell))
    {
        mpr(desc_cannot_memorise_reason(specspell));
        return false;
    }

    if (you.has_spell(specspell))
    {
        mpr("You already know that spell!");
        return false;
    }

    if (you.spell_no >= MAX_KNOWN_SPELLS)
    {
        mpr("Your head is already too full of spells!");
        return false;
    }

    if (you.experience_level < spell_difficulty(specspell) && !wizard)
    {
        mpr("You're too inexperienced to learn that spell!");
        return false;
    }

    if (player_spell_levels() < spell_levels_required(specspell) && !wizard)
    {
        mpr("You can't memorise that many levels of magic yet!");
        return false;
    }

    return true;
}

/**
 * Attempt to make the player learn the given spell.
 *
 * @param   specspell  The spell to be learned.
 * @param   wizard     Whether to memorise instantly and skip some checks for
 *                     wizmode memorisation.
 * @return             true if the player learned the spell, false
 *                     otherwise.
*/
bool learn_spell(spell_type specspell, bool wizard)
{
    if (!_learn_spell_checks(specspell, wizard))
        return false;

    if (!wizard)
    {
        if (specspell == SPELL_OZOCUBUS_ARMOUR
            && !player_effectively_in_light_armour())
        {
            mprf(MSGCH_WARN,
                 "Your armour is too heavy for you to cast this spell!");
        }

        const int severity = fail_severity(specspell);

        if (raw_spell_fail(specspell) >= 100 && !vehumet_is_offering(specspell))
            mprf(MSGCH_WARN, "This spell is impossible to cast!");
        else if (severity > 0)
        {
            mprf(MSGCH_WARN, "This spell is %s to cast%s",
                             fail_severity_adjs[severity],
                             severity > 1 ? "!" : ".");
        }
    }

    const string prompt = make_stringf(
             "Memorise %s, consuming %d spell level%s and leaving %d?",
             spell_title(specspell), spell_levels_required(specspell),
             spell_levels_required(specspell) != 1 ? "s" : "",
             player_spell_levels() - spell_levels_required(specspell));

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(prompt.c_str(), true, 'n', false))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (wizard)
        add_spell_to_memory(specspell);
    else
    {
        if (!already_learning_spell(specspell))
            start_delay<MemoriseDelay>(spell_difficulty(specspell), specspell);
        you.turn_is_over = true;

        did_god_conduct(DID_SPELL_CASTING, 2 + random2(5));
    }

    return true;
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!is_artefact(book))
        return false;

    return book.props.exists(BOOK_TITLED_KEY)
           && book.props[BOOK_TITLED_KEY].get_bool() == true;
}

/**
 * @file
 * @brief Spellbook/rod contents array and management functions
**/

#include "AppHdr.h"

#include "spl-book.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <iomanip>

#include "artefact.h"
#include "cio.h"
#include "colour.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "effects.h"
#include "externs.h"
#include "food.h"
#include "format.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "player.h"
#include "religion.h"
#include "species.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"

#define SPELL_LIST_KEY "spell_list"

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

// The list of spells in spellbooks:
static spell_type spellbook_template_array[][SPELLBOOK_SIZE] =
{
#   include "book-data.h"
};

spell_type which_spell_in_book(const item_def &book, int spl)
{
    ASSERT(book.base_type == OBJ_BOOKS || book.base_type == OBJ_RODS);

    const CrawlHashTable &props = book.props;
    if (!props.exists(SPELL_LIST_KEY))
        return which_spell_in_book(book.book_number(), spl);

    const CrawlVector &spells = props[SPELL_LIST_KEY].get_vector();

    ASSERT(spells.get_type() == SV_INT);
    ASSERT(spells.size() == SPELLBOOK_SIZE);

    return static_cast<spell_type>(spells[spl].get_int());
}

spell_type which_spell_in_book(int sbook_type, int spl)
{
    ASSERT_RANGE(sbook_type, 0, (int)ARRAYSZ(spellbook_template_array));
    return spellbook_template_array[sbook_type][spl];
}

// If fs is not NULL, updates will be to the formatted_string instead of
// the display.
int spellbook_contents(item_def &book, read_book_action_type action,
                        formatted_string *fs)
{
    int spelcount = 0;
    int i, j;
    bool update_screen = !fs;

    const int spell_levels = player_spell_levels();

    formatted_string out;
    out.textcolor(LIGHTGREY);

    out.cprintf("%s", book.name(DESC_THE).c_str());

    out.cprintf("\n\n Spells                             Type                      Level\n");

    for (j = 0; j < SPELLBOOK_SIZE; j++)
    {
        spell_type stype = which_spell_in_book(book, j);
        if (stype == SPELL_NO_SPELL)
            continue;

        out.cprintf(" ");

        const int level_diff = spell_difficulty(stype);
        const int levels_req = spell_levels_required(stype);

        int colour = DARKGREY;
        if (action == RBOOK_USE_ROD)
        {
            ASSERT(book.base_type == OBJ_RODS);
            if (book.plus >= level_diff * ROD_CHARGE_MULT)
                colour = spell_highlight_by_utility(stype, COL_UNKNOWN, false, true);
            else
                colour = COL_USELESS;
        }
        else
        {
            if (you.has_spell(stype))
                colour = COL_MEMORIZED;
            else if (you_cannot_memorise(stype)
                || you.experience_level < level_diff
                || spell_levels < levels_req
                || book.base_type == OBJ_BOOKS
                   && !player_can_memorise_from_spellbook(book))
            {
                colour = COL_USELESS;
            }
            else if (!you.has_spell(stype))
                colour = COL_UNMEMORIZED;
            else
                colour = spell_highlight_by_utility(stype);
        }

        out.textcolor(colour);

        char strng[2];
        strng[0] = index_to_letter(spelcount);
        strng[1] = 0;

        out.cprintf("%s", strng);
        out.cprintf(" - ");

        out.cprintf("%s", chop_string(spell_title(stype), 29).c_str());

        string schools;
        if (action == RBOOK_USE_ROD)
            schools = "Evocations";
        else
        {
            bool first = true;
            for (i = 0; i <= SPTYP_LAST_EXPONENT; i++)
            {
                if (spell_typematch(stype, 1 << i))
                {
                    if (!first)
                        schools += "/";
                    schools += spelltype_long_name(1 << i);
                    first = false;
                }
            }
        }
        out.cprintf("%s%d\n", chop_string(schools, 30).c_str(), level_diff);
        spelcount++;
    }

    out.textcolor(LIGHTGREY);
    out.cprintf("\n");

    switch (action)
    {
    case RBOOK_USE_ROD:
        out.cprintf("Select a spell to cast.\n");
        break;

    case RBOOK_READ_SPELL:
        if (book.base_type == OBJ_BOOKS && in_inventory(book)
            && item_type_known(book)
            && player_can_memorise_from_spellbook(book))
        {
            out.cprintf("Select a spell to read its description, to "
                         "memorise it or to forget it.\n");
        }
        else
            out.cprintf("Select a spell to read its description.\n");
        break;

    default:
        break;
    }

    if (fs)
        *fs = out;

    int keyn = 0;
    if (update_screen && !crawl_state.is_replaying_keys())
    {
        cursor_control coff(false);
        clrscr();

        out.display();
    }

    if (update_screen)
        keyn = toalower(getchm(KMC_MENU));

    return keyn;     // either ignored or spell letter
}

// Rarity 100 is reserved for unused books.
int book_rarity(uint8_t which_book)
{
    switch (which_book)
    {
    case BOOK_MINOR_MAGIC:
    case BOOK_HINDERANCE:
    case BOOK_CANTRIPS:
        return 1;

    case BOOK_CHANGES:
    case BOOK_MALEDICT:
        return 2;

    case BOOK_CONJURATIONS:
    case BOOK_NECROMANCY:
    case BOOK_CALLINGS:
    case BOOK_WIZARDRY:
        return 3;

    case BOOK_FLAMES:
    case BOOK_FROST:
    case BOOK_AIR:
    case BOOK_GEOMANCY:
        return 4;

    case BOOK_YOUNG_POISONERS:
    case BOOK_WAR_CHANTS:
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
    case BOOK_CONTROL:
    case BOOK_SPATIAL_TRANSLOCATIONS:
        return 10;

    case BOOK_TEMPESTS:
    case BOOK_DEATH:
        return 11;

    case BOOK_BURGLARY:
    case BOOK_ALCHEMY:
    case BOOK_DREAMS:
        return 12;

    case BOOK_ENVENOMATIONS:
    case BOOK_WARP:
    case BOOK_DRAGON:
        return 15;

    case BOOK_SUMMONINGS:
        return 18;

    case BOOK_ANNIHILATIONS:
    case BOOK_GRAND_GRIMOIRE:
    case BOOK_NECRONOMICON:  // Kikubaaqudgha special
    case BOOK_MANUAL:
        return 20;

    case BOOK_DESTRUCTION:
        return 30;

#if TAG_MAJOR_VERSION == 34
    case BOOK_STALKING:
        return 100;
#endif

    default:
        return 1;
    }
}

static uint8_t _lowest_rarity[NUM_SPELLS];

void init_spell_rarities()
{
    for (int i = 0; i < NUM_SPELLS; ++i)
        _lowest_rarity[i] = 255;

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        // Manuals and books of destruction are not even part of this loop.
        if (i >= MIN_RARE_BOOK && i <= MAX_RARE_BOOK)
            continue;

        for (int j = 0; j < SPELLBOOK_SIZE; ++j)
        {
            spell_type spell = which_spell_in_book(i, j);
            if (spell == SPELL_NO_SPELL)
                continue;

#ifdef DEBUG
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

            const int rarity = book_rarity(i);
            if (rarity < _lowest_rarity[spell])
                _lowest_rarity[spell] = rarity;
        }
    }
}

bool is_player_spell(spell_type which_spell)
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        for (int j = 0; j < SPELLBOOK_SIZE; ++j)
        {
            if (which_spell_in_book(i, j) == which_spell)
                return true;
        }
    }
    return false;
}

int spell_rarity(spell_type which_spell)
{
    const int rarity = _lowest_rarity[which_spell];

    if (rarity == 255)
        return -1;

    return rarity;
}

static bool _is_valid_spell_in_book(const item_def &book, int spell)
{
    return which_spell_in_book(book, spell) != SPELL_NO_SPELL;
}

// Returns false if the player cannot memorise from the book,
// and true otherwise. -- bwr
bool player_can_memorise_from_spellbook(const item_def &book)
{
    if (book.base_type != OBJ_BOOKS)
        return true;

    if (book.props.exists(SPELL_LIST_KEY))
        return true;

    if ((book.sub_type == BOOK_ANNIHILATIONS
         && (you.skill(SK_CONJURATIONS) < 10
             || you.skill(SK_SPELLCASTING) < 6))
        || (book.sub_type == BOOK_GRAND_GRIMOIRE
            && (you.skill(SK_SUMMONINGS) < 10
                || you.skill(SK_SPELLCASTING) < 6))
        || (book.sub_type == BOOK_NECRONOMICON
            && !you_worship(GOD_KIKUBAAQUDGHA)
            && (you.skill(SK_NECROMANCY) < 10
                || you.skill(SK_SPELLCASTING) < 6)))
    {
        return false;
    }

    return true;
}

void mark_had_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!item_is_spellbook(book))
        return;

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type stype = which_spell_in_book(book, i);
        if (stype == SPELL_NO_SPELL)
            continue;

        you.seen_spell.set(stype);
    }

    if (book.sub_type == BOOK_RANDART_LEVEL)
        ASSERT_RANGE(book.plus, 1, 10); // book's level

    if (!book.props.exists(SPELL_LIST_KEY))
        mark_had_book(book.book_number());
}

void mark_had_book(int booktype)
{
    ASSERT_RANGE(booktype, 0, MAX_FIXED_BOOK + 1);

    you.had_book.set(booktype);
}

void inscribe_book_highlevel(item_def &book)
{
    if (!item_type_known(book)
        && book.inscription.find("highlevel") == string::npos)
    {
        add_inscription(book, "highlevel");
    }
}

/**
 * Identify a held book/rod, if appropriate.
 * @return whether we can see its spells
 */
bool maybe_id_book(item_def &book, bool silent)
{
    if (book.base_type != OBJ_BOOKS && book.base_type != OBJ_RODS)
        return false;

    if (book.base_type == OBJ_BOOKS && book.sub_type == BOOK_DESTRUCTION)
    {
        ASSERT(fully_identified(book));
        return false;
    }

    if (book.base_type == OBJ_BOOKS && book.sub_type == BOOK_MANUAL)
    {
        set_ident_flags(book, ISFLAG_IDENT_MASK);
        return false;
    }

    if (book.base_type == OBJ_BOOKS && !item_type_known(book)
        && !player_can_memorise_from_spellbook(book))
    {
        if (!silent)
        {
            mpr("This book is beyond your current level of understanding.");
            more();
        }

        inscribe_book_highlevel(book);
        return false;
    }

    set_ident_flags(book, ISFLAG_IDENT_MASK);

    if (book.base_type == OBJ_BOOKS)
        mark_had_book(book);

    return true;
}

int read_book(item_def &book, read_book_action_type action)
{
    if (!maybe_id_book(book))
        return 0;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "read_book");
#endif

    // Remember that this function is called for rods as well.
    const int keyin = spellbook_contents(book, action);

    if (!crawl_state.is_replaying_keys())
        redraw_screen();

    return keyin;
}

bool you_cannot_memorise(spell_type spell)
{
    bool temp;
    return you_cannot_memorise(spell, temp);
}

// form is set to true if a form (lich or wisp) prevents us from
// memorising the spell.
bool you_cannot_memorise(spell_type spell, bool &form)
{
    bool rc = false;

    if (you.is_undead)
    {
        switch (spell)
        {
        case SPELL_BORGNJORS_REVIVIFICATION:
        case SPELL_DEATHS_DOOR:
        case SPELL_NECROMUTATION:
            // Prohibited to all undead.
            rc = true;
            break;

        case SPELL_CURE_POISON:
        case SPELL_STONESKIN:
        case SPELL_BEASTLY_APPENDAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_DRAGON_FORM:
        case SPELL_ICE_FORM:
        case SPELL_SPIDER_FORM:
        case SPELL_STATUE_FORM:
            // Allowed for vampires (depending on hunger).
            rc = (you.is_undead != US_SEMI_UNDEAD);
            break;

        case SPELL_INTOXICATE:
        case SPELL_REGENERATION:
            // Only prohibited for liches and mummies.
            rc = (you.is_undead == US_UNDEAD);
            break;

        default:
            break;
        }

        // If our undeath is only temporary, mark that fact. This
        // assumes that the already undead cannot enter lich form.
        if (rc && you.form == TRAN_LICH)
            form = true;
    }

    if (you.species == SP_GARGOYLE && spell == SPELL_CURE_POISON)
        rc = true, form = false;

    if (you.species == SP_DEEP_DWARF && spell == SPELL_REGENERATION)
        rc = true, form = false;

    if (you.species == SP_FELID
        && (spell == SPELL_PORTAL_PROJECTILE
         // weapon branding is useless
         || spell == SPELL_FIRE_BRAND
         || spell == SPELL_FREEZING_AURA
         || spell == SPELL_LETHAL_INFUSION
         || spell == SPELL_WARP_BRAND
         || spell == SPELL_EXCRUCIATING_WOUNDS
         || spell == SPELL_POISON_WEAPON
         || spell == SPELL_SURE_BLADE
         // could be useful if it didn't require wielding
         || spell == SPELL_TUKIMAS_DANCE
         || spell == SPELL_SPECTRAL_WEAPON))
    {
        rc = true, form = false;
    }

    if (you.species == SP_DJINNI
        && (spell == SPELL_ICE_FORM
         || spell == SPELL_OZOCUBUS_ARMOUR
         || spell == SPELL_LEDAS_LIQUEFACTION))
    {
        rc = true, form = false;
    }

    if (you.species == SP_LAVA_ORC
        && (spell == SPELL_STONESKIN
         || spell == SPELL_OZOCUBUS_ARMOUR))
    {
        rc = true, form = false;
    }

    if (you.form == TRAN_WISP)
    {
        // If we were otherwise allowed to memorise the spell.
        if (!rc)
            form = true;
        return true;
    }

    if (you.species == SP_FORMICID
        && (spell == SPELL_BLINK
         || spell == SPELL_CONTROL_TELEPORT
         || spell == SPELL_CONTROLLED_BLINK))
    {
        rc = true, form = false;
    }

    return rc;
}

bool player_can_memorise(const item_def &book)
{
    if (!item_is_spellbook(book) || !player_spell_levels())
        return false;

    for (int j = 0; j < SPELLBOOK_SIZE; j++)
    {
        const spell_type stype = which_spell_in_book(book, j);

        if (stype == SPELL_NO_SPELL)
            continue;

        // Easiest spell already too difficult?
        if (spell_difficulty(stype) > you.experience_level
            || player_spell_levels() < spell_levels_required(stype))
        {
            return false;
        }

        bool knows_spell = false;
        for (int i = 0; i < MAX_KNOWN_SPELLS && !knows_spell; i++)
            knows_spell = (you.spells[i] == stype);

        // You don't already know this spell.
        if (!knows_spell)
            return true;
    }
    return false;
}

typedef vector<spell_type>   spell_list;
typedef map<spell_type, int> spells_to_books;

static void _index_book(item_def& book, spells_to_books &book_hash,
                        unsigned int &num_unreadable, bool &book_errors)
{
    if (!player_can_memorise_from_spellbook(book))
    {
        inscribe_book_highlevel(book);
        num_unreadable++;
        return;
    }

    mark_had_book(book);
    set_ident_flags(book, ISFLAG_KNOW_TYPE);
    set_ident_flags(book, ISFLAG_IDENT_MASK);

    int spells_in_book = 0;
    for (int j = 0; j < SPELLBOOK_SIZE; j++)
    {
        if (!_is_valid_spell_in_book(book, j))
            continue;

        const spell_type spell = which_spell_in_book(book, j);

        spells_in_book++;

        spells_to_books::iterator it = book_hash.find(spell);
        if (it == book_hash.end())
            book_hash[spell] = book.sub_type;
    }

    if (spells_in_book == 0)
    {
        mprf(MSGCH_ERROR, "Spellbook \"%s\" contains no spells! Please "
             "file a bug report.", book.name(DESC_PLAIN).c_str());
        book_errors = true;
    }
}

static bool _get_mem_list(spell_list &mem_spells,
                          spells_to_books &book_hash,
                          unsigned int &num_unreadable,
                          unsigned int &num_race,
                          bool just_check = false,
                          spell_type current_spell = SPELL_NO_SPELL)
{
    if (you.form == TRAN_WISP)
    {
        if (!just_check)
            mprf(MSGCH_PROMPT, "You can't handle any books in this form.");
        return false;
    }

    bool          book_errors    = false;
    unsigned int  num_on_ground  = 0;
    unsigned int  num_books      = 0;
    unsigned int  num_unknown    = 0;
                  num_unreadable = 0;

    // Collect the list of all spells in all available spellbooks.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& book(you.inv[i]);

        if (!item_is_spellbook(book))
            continue;

        num_books++;
        _index_book(book, book_hash, num_unreadable, book_errors);
    }

    // We also check the ground
    vector<const item_def*> items;
    item_list_on_square(items, you.visible_igrd(you.pos()));

    for (unsigned int i = 0; i < items.size(); ++i)
    {
        item_def book(*items[i]);
        if (!item_is_spellbook(book))
            continue;

        if (!item_type_known(book))
        {
            num_unknown++;
            continue;
        }

        num_books++;
        num_on_ground++;
        _index_book(book, book_hash, num_unreadable, book_errors);
    }

    // Handle Vehumet gifts
    set<spell_type>::iterator gift_iterator = you.vehumet_gifts.begin();
    if (gift_iterator != you.vehumet_gifts.end())
    {
        num_books++;
        while (gift_iterator != you.vehumet_gifts.end())
            book_hash[*gift_iterator++] = NUM_BOOKS;
    }

    if (book_errors)
        more();

    if (num_books == 0)
    {
        if (!just_check)
        {
            if (num_unknown > 1)
                mprf(MSGCH_PROMPT, "You must pick up those books before reading them.");
            else if (num_unknown == 1)
                mprf(MSGCH_PROMPT, "You must pick up this book before reading it.");
            else
                mprf(MSGCH_PROMPT, "You aren't carrying or standing over any spellbooks.");
        }
        return false;
    }
    else if (num_unreadable == num_books)
    {
        if (!just_check)
        {
            mprf(MSGCH_PROMPT, "All of the spellbooks%s are beyond your "
                 "current level of comprehension.",
                 num_on_ground == 0 ? " you're carrying" : "");
        }
        return false;
    }
    else if (book_hash.empty())
    {
        if (!just_check)
            mprf(MSGCH_PROMPT, "None of the spellbooks you are carrying contain any spells.");
        return false;
    }

    unsigned int num_known      = 0;
                 num_race       = 0;
    unsigned int num_low_xl     = 0;
    unsigned int num_low_levels = 0;
    unsigned int num_memable    = 0;

    for (spells_to_books::iterator i = book_hash.begin();
         i != book_hash.end(); ++i)
    {
        const spell_type spell = i->first;

        if (spell == current_spell || you.has_spell(spell))
            num_known++;
        else if (you_cannot_memorise(spell))
            num_race++;
        else
        {
            mem_spells.push_back(spell);

            int avail_slots = player_spell_levels();
            if (current_spell != SPELL_NO_SPELL)
                avail_slots -= spell_levels_required(current_spell);

            if (spell_difficulty(spell) > you.experience_level)
                num_low_xl++;
            else if (avail_slots < spell_levels_required(spell))
                num_low_levels++;
            else
                num_memable++;
        }
    }

    if (num_memable > 0 && you.spell_no >= MAX_KNOWN_SPELLS)
    {
        if (!just_check)
            mpr("Your head is already too full of spells!");
        return false;
    }

    if (num_memable)
        return true;

    // Return true even if there are only spells we can't memorise _yet_.
    if (just_check)
        return num_low_levels > 0 || num_low_xl > 0;

    unsigned int total = num_known + num_race + num_low_xl + num_low_levels;

    if (num_known == total)
        mprf(MSGCH_PROMPT, "You already know all available spells.");
    else if (num_race == total || (num_known + num_race) == total)
    {
        const bool lichform = (you.form == TRAN_LICH);
        const string species = "a " + species_name(you.species);
        mprf(MSGCH_PROMPT,
             "You cannot memorise any of the available spells because you "
             "are %s.", lichform ? "in Lich form"
                                 : lowercase_string(species).c_str());
    }
    else if (num_low_levels > 0 || num_low_xl > 0)
    {
        // Just because we can't memorise them doesn't mean we don't want to
        // see what we have available. See FR #235. {due}
        return true;
    }
    else
    {
        mprf(MSGCH_PROMPT, "You can't memorise any new spells for an unknown "
                           "reason; please file a bug report.");
    }

    if (num_unreadable)
    {
        mprf(MSGCH_PROMPT, "Additionally, %u of your spellbooks are beyond "
             "your current level of understanding, and thus none of the "
             "spells in them are available to you.", num_unreadable);
    }

    return false;
}

// If current_spell is a valid spell, returns whether you'll be able to
// memorise any further spells once this one is committed to memory.
bool has_spells_to_memorise(bool silent, int current_spell)
{
    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_unreadable;
    unsigned int    num_race;

    return _get_mem_list(mem_spells, book_hash, num_unreadable, num_race,
                         silent, (spell_type) current_spell);
}

static bool _sort_mem_spells(spell_type a, spell_type b)
{
    // List the Vehumet gifts at the very top.
    bool offering_a = vehumet_is_offering(a);
    bool offering_b = vehumet_is_offering(b);
    if (offering_a != offering_b)
        return offering_a;

    // List spells we can memorize right away first.
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
    const int fail_rate_a = failure_rate_to_int(spell_fail(a));
    const int fail_rate_b = failure_rate_to_int(spell_fail(b));
    if (fail_rate_a != fail_rate_b)
        return fail_rate_a < fail_rate_b;

    if (spell_difficulty(a) != spell_difficulty(b))
        return spell_difficulty(a) < spell_difficulty(b);

    return strcasecmp(spell_title(a), spell_title(b)) < 0;
}

vector<spell_type> get_mem_spell_list(vector<int> &books)
{
    vector<spell_type> spells;

    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_unreadable;
    unsigned int    num_race;

    if (!_get_mem_list(mem_spells, book_hash, num_unreadable, num_race))
        return spells;

    sort(mem_spells.begin(), mem_spells.end(), _sort_mem_spells);

    for (unsigned int i = 0; i < mem_spells.size(); i++)
    {
        spell_type spell = mem_spells[i];
        spells.push_back(spell);

        spells_to_books::iterator it = book_hash.find(spell);
        books.push_back(it->second);
    }

    return spells;
}

static spell_type _choose_mem_spell(spell_list &spells,
                                    spells_to_books &book_hash,
                                    unsigned int num_unreadable,
                                    unsigned int num_race)
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
    // [enne] Hack.  Use a separate title, so the column headers are aligned.
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

    spell_menu.set_highlighter(NULL);
    spell_menu.set_tag("spell");

    spell_menu.action_cycle = Menu::CYCLE_TOGGLE;
    spell_menu.menu_action  = Menu::ACT_EXECUTE;

    string more_str = make_stringf("<lightgreen>%d spell level%s left"
                                   "<lightgreen>",
                                   player_spell_levels(),
                                   (player_spell_levels() > 1
                                    || player_spell_levels() == 0) ? "s" : "");

    if (num_unreadable > 0)
    {
        more_str += make_stringf(", <lightmagenta>%u overly difficult "
                                 "spellbook%s</lightmagenta>",
                                 num_unreadable,
                                 num_unreadable > 1 ? "s" : "");
    }

    if (num_race > 0)
    {
        more_str += make_stringf(", <lightred>%u spell%s unmemorisable"
                                 "</lightred>",
                                 num_race,
                                 num_race > 1 ? "s" : "");
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
        char* failure = failure_rate_to_string(spell_fail(spell));
        desc << chop_string(failure, 12);
        free(failure);
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
            describe_spell(spell);
        else
            return spell;
    }
}

bool can_learn_spell(bool silent)
{
    if (you.form == TRAN_BAT)
    {
        if (!silent)
            canned_msg(MSG_PRESENT_FORM);
        return false;
    }

    if (you.stat_zero[STAT_INT])
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
    spells_to_books book_hash;

    unsigned int num_unreadable, num_race;

    if (!_get_mem_list(mem_spells, book_hash, num_unreadable, num_race))
        return false;

    spell_type specspell = _choose_mem_spell(mem_spells, book_hash,
                                             num_unreadable, num_race);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg(MSG_OK);
        return false;
    }

    return learn_spell(specspell);
}

// Returns a string about why a character can't memorise a spell.
string desc_cannot_memorise_reason(bool form)
{
    string desc = "You cannot ";
    if (form)
        desc += "currently ";
    desc += "memorise or cast this spell because you are ";

    if (form)
        desc += "in " + uppercase_first(transform_name()) + " form";
    else
        desc += "a " + lowercase_string(species_name(you.species));

    desc += ".";

    return desc;
}

static bool _learn_spell_checks(spell_type specspell)
{
    if (!can_learn_spell())
        return false;

    if (already_learning_spell((int) specspell))
        return false;

    bool form = false;
    if (you_cannot_memorise(specspell, form))
    {
        mpr(desc_cannot_memorise_reason(form).c_str());
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

    if (you.experience_level < spell_difficulty(specspell))
    {
        mpr("You're too inexperienced to learn that spell!");
        return false;
    }

    if (player_spell_levels() < spell_levels_required(specspell))
    {
        mpr("You can't memorise that many levels of magic yet!");
        return false;
    }

    if (spell_fail(specspell) >= 100 && !vehumet_is_offering(specspell))
    {
        mpr("This spell is too difficult to memorise!");
        return false;
    }

    return true;
}

bool learn_spell(spell_type specspell)
{
    if (!_learn_spell_checks(specspell))
        return false;

    double chance = get_miscast_chance(specspell);

    if (chance >= 0.025)
        mprf(MSGCH_WARN, "This spell is very dangerous to cast!");
    else if (chance >= 0.005)
        mprf(MSGCH_WARN, "This spell is quite dangerous to cast!");
    else if (chance >= 0.001)
        mpr("This spell is slightly dangerous to cast.");

    snprintf(info, INFO_SIZE,
             "Memorise %s, consuming %d spell level%s and leaving %d?",
             spell_title(specspell), spell_levels_required(specspell),
             spell_levels_required(specspell) != 1 ? "s" : "",
             player_spell_levels() - spell_levels_required(specspell));

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(info, true, 'n', false))
    {
        canned_msg(MSG_OK);
        return false;
    }

    start_delay(DELAY_MEMORISE, spell_difficulty(specspell), specspell);
    you.turn_is_over = true;

    did_god_conduct(DID_SPELL_CASTING, 2 + random2(5));

    return true;
}

bool forget_spell_from_book(spell_type spell, const item_def* book)
{
    string prompt;

    prompt += make_stringf("Forgetting %s from %s will destroy the book! "
                           "Are you sure?",
                           spell_title(spell),
                           book->name(DESC_THE).c_str());

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    mprf("As you tear out the page describing %s, the book crumbles to dust.",
        spell_title(spell));

    if (del_spell_from_memory(spell))
    {
        item_was_destroyed(*book);
        destroy_spellbook(*book);
        dec_inv_item_quantity(book->link, 1);
        you.turn_is_over = true;
        return true;
    }
    else
    {
        // This shouldn't happen.
        mprf("A bug prevents you from forgetting %s.", spell_title(spell));
        return false;
    }
}

int count_rod_spells(const item_def &item, bool need_id)
{
    if (item.base_type != OBJ_RODS)
        return -1;

    if (need_id && !item_type_known(item))
        return 0;

    const int type = item.book_number();
    if (type == -1)
        return 0;

    int nspel = 0;
    while (nspel < SPELLBOOK_SIZE && _is_valid_spell_in_book(item, nspel))
        ++nspel;

    return nspel;
}

int rod_spell(int rod)
{
    item_def& irod(you.inv[rod]);

    if (irod.base_type != OBJ_RODS)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return -1;
    }

    // ID code got moved to item_use::wield_effects. {due}

    const int num_spells = count_rod_spells(irod, false);

    int keyin = 0;
    if (num_spells == 0)
    {
        canned_msg(MSG_NOTHING_HAPPENS);  // shouldn't happen
        return 0;
    }
    else if (num_spells == 1)
        keyin = 'a';  // automatically selected if it's the only option
    else
    {
        mprf(MSGCH_PROMPT,
             "Evoke which spell from the rod ([a-%c] spell [?*] list)? ",
             'a' + num_spells - 1);

        // Note that auto_list is ignored here.
        keyin = get_ch();

        if (keyin == '?' || keyin == '*')
        {
            keyin = read_book(you.inv[rod], RBOOK_USE_ROD);
            // [ds] read_book sets turn_is_over.
            you.turn_is_over = false;
        }
    }

    if (key_is_escape(keyin) || keyin == ' ' || keyin == '\r' || keyin == '\n')
    {
        canned_msg(MSG_OK);
        return -1;
    }

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_HUH);
        return -1;
    }

    const int idx = letter_to_index(keyin);

    if ((idx >= SPELLBOOK_SIZE) || !_is_valid_spell_in_book(irod, idx))
    {
        canned_msg(MSG_HUH);
        return -1;
    }

    const spell_type spell = which_spell_in_book(irod, idx);
    int mana = spell_mana(spell) * ROD_CHARGE_MULT;
    int power = calc_spell_power(spell, false, false, true, true);

    int food = spell_hunger(spell, true);

    if (you.is_undead == US_UNDEAD)
        food = 0;

    if (food && (you.hunger_state == HS_STARVING || you.hunger <= food)
        && !you.is_undead)
    {
        canned_msg(MSG_NO_ENERGY);
        crawl_state.zero_turns_taken();
        return -1;
    }

    if (spell == SPELL_THUNDERBOLT && you.props.exists("thunderbolt_last")
        && you.props["thunderbolt_last"].get_int() + 1 == you.num_turns)
    {
        // Starting it up takes 2 mana, continuing any amount up to 5.
        // You don't get to expend less (other than stopping the zap completely).
        mana = min(5 * ROD_CHARGE_MULT, (int)irod.plus);
        // Never allow using less than a whole point of charge.
        mana = max(mana, ROD_CHARGE_MULT);
        you.props["thunderbolt_mana"].get_int() = mana;
    }

    if (irod.plus < mana)
    {
        mpr("The rod doesn't have enough magic points.");
        crawl_state.zero_turns_taken();
        // Don't lose a turn for trying to evoke without enough MP - that's
        // needlessly cruel for an honest error.
        return -1;
    }

    // All checks passed, we can cast the spell.
    if (you.confused())
        random_uselessness();
    else if (your_spells(spell, power, false, false)
                == SPRET_ABORT)
    {
        crawl_state.zero_turns_taken();
        return -1;
    }

    make_hungry(food, true, true);
    irod.plus -= mana;
    you.wield_change = true;
    you.turn_is_over = true;

    return roll_dice(1, 1 + spell_difficulty(spell) / 2);
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

    unsigned int schools_a = get_spell_disciplines(a);
    unsigned int schools_b = get_spell_disciplines(b);

    if (schools_a != schools_b && schools_a != 0 && schools_b != 0)
    {
        const char* a_type = NULL;
        const char* b_type = NULL;

        // Find lowest/earliest school for each spell.
        for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
        {
            int mask = 1 << i;
            if (a_type == NULL && (schools_a & mask))
                a_type = spelltype_long_name(mask);
            if (b_type == NULL && (schools_b & mask))
                b_type = spelltype_long_name(mask);
        }
        ASSERT(a_type != NULL);
        ASSERT(b_type != NULL);
        return strcmp(a_type, b_type) < 0;
    }

    return strcmp(spell_title(a), spell_title(b)) < 0;
}

bool is_memorised(spell_type spell)
{
    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
        if (you.spells[i] == spell)
            return true;

    return false;
}

static void _get_spell_list(vector<spell_type> &spells, int level,
                            unsigned int disc1, unsigned int disc2,
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
        for (int i = MIN_RARE_BOOK; i <= MAX_RARE_BOOK; ++i)
            for (int j = 0; j < SPELLBOOK_SIZE; ++j)
            {
                spell_type spell = which_spell_in_book(i, j);
                if (spell == SPELL_NO_SPELL)
                    continue;

                if (spell_rarity(spell) != -1)
                    continue;

                special_spells.push_back(spell);
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

        if (avoid_known && you.seen_spell[spell])
            continue;

        if (level != -1)
        {
            // fixed level randart: only include spells of the given level
            if (spell_difficulty(spell) != level)
                continue;
        }
        else
        {
            // themed randart: only include spells of the given disciplines
            const unsigned int disciplines = get_spell_disciplines(spell);
            if ((!(disciplines & disc1) && !(disciplines & disc2))
                 || disciplines_conflict(disc1, disciplines)
                 || disciplines_conflict(disc2, disciplines))
            {
                continue;
            }
        }

        if (avoid_uncastable && you_cannot_memorise(spell))
        {
            uncastable_discard++;
            continue;
        }

        if (god_dislikes_spell_type(spell, god))
        {
            god_discard++;
            continue;
        }

        // Passed all tests.
        spells.push_back(spell);
    }
}

static void _get_spell_list(vector<spell_type> &spells,
                            unsigned int disc1, unsigned int disc2,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    _get_spell_list(spells, -1, disc1, disc2,
                    god, avoid_uncastable, god_discard, uncastable_discard,
                    avoid_known);
}

static void _get_spell_list(vector<spell_type> &spells, int level,
                            god_type god, bool avoid_uncastable,
                            int &god_discard, int &uncastable_discard,
                            bool avoid_known = false)
{
    _get_spell_list(spells, level, SPTYP_NONE, SPTYP_NONE,
                    god, avoid_uncastable, god_discard, uncastable_discard,
                    avoid_known);
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

bool make_book_level_randart(item_def &book, int level, int num_spells,
                             string owner)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    god_type god;
    (void) origin_is_god_gift(book, &god);

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

    if (num_spells == -1)
    {
        //555666421
        num_spells = min(5 + (level - 1)/3, 18 - 2*level);
        num_spells = max(1, num_spells);
    }
    ASSERT_RANGE(num_spells, 0 + 1, SPELLBOOK_SIZE + 1);

    book.plus  = level;
    book.plus2 = num_spells;

    book.sub_type = BOOK_RANDART_LEVEL;
    _make_book_randart(book);

    int god_discard        = 0;
    int uncastable_discard = 0;

    vector<spell_type> spells;
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

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    int book_pos = 0;
    while (book_pos < num_spells)
    {
        int spell_pos = random2(spells.size());

        if (spell_used[spell_pos])
            continue;

        spell_type spell = spells[spell_pos];
        ASSERT(spell != SPELL_NO_SPELL);

        if (avoid_memorised[spell_pos] && is_memorised(spell))
        {
            // Only once.
            avoid_memorised[spell_pos] = false;
            continue;
        }

        if (avoid_seen[spell_pos] && you.seen_spell[spell] && coinflip())
        {
            // Only once.
            avoid_seen[spell_pos] = false;
            continue;
        }

        spell_used[spell_pos]     = true;
        chosen_spells[book_pos++] = spell;
    }
    sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE, _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        spell_vec[i].get_int() = chosen_spells[i];

    bool has_owner = true;
    string name = "";
    if (!owner.empty())
        name = owner;
    else if (god != GOD_NO_GOD)
        name = god_name(god, false);
    else if (one_chance_in(30))
        name = god_name(GOD_SIF_MUNA, false);
    else if (one_chance_in(3))
        name = make_name(random_int(), false);
    else
        has_owner = false;

    if (has_owner)
        name = apostrophise(name) + " ";

    // None of these books need a definite article prepended.
    book.props["is_named"].get_bool() = true;

    string bookname;
    if (god == GOD_XOM && coinflip())
    {
        bookname = getRandNameString("book_noun") + " of "
                   + getRandNameString("Xom_book_title");
        bookname = replace_name_parts(bookname, book);
    }
    else
    {
        string lookup;
        if (level == 1)
            lookup = "starting";
        else if (level <= 3 || level == 4 && coinflip())
            lookup = "easy";
        else if (level <= 6)
            lookup = "moderate";
        else
            lookup = "difficult";

        lookup += " level book";

        // First try for names respecting the book's previous owner/author
        // (if one exists), then check for general difficulty.
        if (has_owner)
            bookname = getRandNameString(lookup + " owner");

        if (!has_owner || bookname.empty())
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
            string number;
            switch (level)
            {
            case 1: number = "One"; break;
            case 2: number = "Two"; break;
            case 3: number = "Three"; break;
            case 4: number = "Four"; break;
            case 5: number = "Five"; break;
            case 6: number = "Six"; break;
            case 7: number = "Seven"; break;
            case 8: number = "Eight"; break;
            case 9: number = "Nine"; break;
            default:
                number = ""; break;
            }
            bookname = replace_all(bookname, "@level@", number);
        }
    }

    if (bookname.empty())
        bookname = getRandNameString("book");

    name += bookname;

    set_artefact_name(book, name);

    return true;
}

static bool _get_weighted_discs(bool completely_random, god_type god,
                                int &disc1, int &disc2)
{
    // Eliminate disciplines that the god dislikes or from which all
    // spells are discarded.
    vector<int> ok_discs;
    vector<skill_type> skills;
    vector<int> spellcount;
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        int disc = 1 << i;
        if (disc & SPTYP_DIVINATION)
            continue;

        if (god_dislikes_spell_discipline(disc, god))
            continue;

        int junk1 = 0, junk2 = 0;
        vector<spell_type> spells;
        _get_spell_list(spells, disc, disc, god, !completely_random,
                        junk1, junk2, !completely_random);

        if (spells.empty())
            continue;

        ok_discs.push_back(disc);
        skills.push_back(spell_type2skill(disc));
        spellcount.push_back(spells.size());
    }

    int num_discs = ok_discs.size();

    if (num_discs == 0)
    {
#ifdef DEBUG
        mprf(MSGCH_ERROR, "No valid disciplines with which to make a themed "
                          "randart spellbook.");
#endif
        // Only happens if !completely_random and the player already knows
        // all available spells. We could simply re-allow all disciplines
        // but the player isn't going to get any new spells, anyway, so just
        // consider this acquirement failed. (jpeg)
        return false;
    }

    int skill_weights[SPTYP_LAST_EXPONENT + 1];

    memset(skill_weights, 0, (SPTYP_LAST_EXPONENT + 1) * sizeof(int));

    if (!completely_random)
    {
        int total_skills = 0;
        for (int i = 0; i < num_discs; i++)
        {
            skill_type skill  = skills[i];
            int weight = 2 * you.skills[skill] + 1;

            if (spellcount[i] < 3)
                weight *= spellcount[i]/3;

            skill_weights[i] = max(0, weight);
            total_skills += skill_weights[i];
        }

        if (total_skills == 0)
            completely_random = true;
    }

    if (completely_random)
    {
        for (int i = 0; i < num_discs; i++)
            skill_weights[i] = 1;
    }

    do
    {
        disc1 = ok_discs[choose_random_weighted(skill_weights,
                                                skill_weights + num_discs)];
        disc2 = ok_discs[choose_random_weighted(skill_weights,
                                                skill_weights + num_discs)];
    }
    while (disciplines_conflict(disc1, disc2));

    return true;
}

static bool _get_weighted_spells(bool completely_random, god_type god,
                                 int disc1, int disc2,
                                 int num_spells, int max_levels,
                                 const vector<spell_type> &spells,
                                 spell_type chosen_spells[])
{
    ASSERT(num_spells <= (int) spells.size());
    ASSERT(num_spells <= SPELLBOOK_SIZE);
    ASSERT(num_spells > 0);
    ASSERT(max_levels > 0);

    int spell_weights[NUM_SPELLS];
    memset(spell_weights, 0, NUM_SPELLS * sizeof(int));

    if (completely_random)
    {
        for (unsigned int i = 0; i < spells.size(); i++)
        {
            spell_type spl = spells[i];
            if (god == GOD_XOM)
                spell_weights[spl] = count_bits(get_spell_disciplines(spl));
            else
                spell_weights[spl] = 1;
        }
    }
    else
    {
        const int Spc = you.skills[SK_SPELLCASTING];
        for (unsigned int i = 0; i < spells.size(); i++)
        {
            spell_type spell = spells[i];
            unsigned int disciplines = get_spell_disciplines(spell);

            int d = 1;
            if ((disciplines & disc1) && (disciplines & disc2))
                d = 2;

            int c = 1;
            if (!you.seen_spell[spell])
                c = 4;
            else if (!is_memorised(spell))
                c = 2;

            int total_skill = 0;
            int num_skills  = 0;
            for (int j = 0; j <= SPTYP_LAST_EXPONENT; j++)
            {
                int disc = 1 << j;

                if (disciplines & disc)
                {
                    total_skill += you.skills[spell_type2skill(disc)];
                    num_skills++;
                }
            }
            int w = 1;
            if (num_skills > 0)
                w = (2 + (total_skill / num_skills)) / 3;
            w = max(1, w);

            int l = 5 - abs(3 * spell_difficulty(spell) - Spc) / 7;

            int weight = d * c * w * l;

            ASSERT(weight > 0);
            spell_weights[spell] = weight;
        }
    }

    int book_pos    = 0;
    int spells_left = spells.size();
    while (book_pos < num_spells && max_levels > 0 && spells_left > 0)
    {
        if (chosen_spells[book_pos] != SPELL_NO_SPELL)
        {
            spell_type spell = chosen_spells[book_pos];
            spell_weights[spell]  = 0;
            max_levels           -= spell_difficulty(spell);
            spells_left--;
            book_pos++;
            continue;
        }

        spell_type spell =
            (spell_type) choose_random_weighted(spell_weights,
                                                spell_weights + NUM_SPELLS);
        ASSERT(is_valid_spell(spell));
        ASSERT(spell_weights[spell] > 0);

        int levels = spell_difficulty(spell);

        if (levels > max_levels)
        {
            spell_weights[spell] = 0;
            spells_left--;
            continue;
        }
        chosen_spells[book_pos++] = spell;
        spell_weights[spell]      = 0;
        max_levels               -= levels;
        spells_left--;
    }
    ASSERT(max_levels >= 0);

    return book_pos > 0;
}

static void _remove_nondiscipline_spells(spell_type chosen_spells[],
                                         int d1, int d2,
                                         spell_type exclude = SPELL_NO_SPELL)
{
    int replace = -1;
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        if (chosen_spells[i] == SPELL_NO_SPELL)
            break;

        if (chosen_spells[i] == exclude)
            continue;

        // If a spell matches neither the first nor the second type
        // (that may be the same) remove it.
        if (!spell_typematch(chosen_spells[i], d1)
            && !spell_typematch(chosen_spells[i], d2))
        {
            chosen_spells[i] = SPELL_NO_SPELL;
            if (replace == -1)
                replace = i;
        }
        else if (replace != -1)
        {
            chosen_spells[replace] = chosen_spells[i];
            chosen_spells[i] = SPELL_NO_SPELL;
            replace++;
        }
    }
}

static void _add_included_spells(spell_type chosen_spells[SPELLBOOK_SIZE],
                                 vector<spell_type> incl_spells)
{
    for (unsigned int i = 0; i < incl_spells.size(); ++i)
    {
        spell_type incl_spell = incl_spells[i];

        if (incl_spell == SPELL_NO_SPELL)
            continue;

        for (int j = 0; j < SPELLBOOK_SIZE; ++j)
        {
            // Already included.
            if (chosen_spells[j] == incl_spell)
                break;

            if (chosen_spells[j] == SPELL_NO_SPELL)
            {
                // Add to spells.
                chosen_spells[j] = incl_spell;
                break;
            }
        }
    }
}

// Takes a book of any type, a spell discipline or two, the number of spells
// (up to 8), the total spell levels of all spells, a spell that absolutely
// has to be included, and the name of whomever the book should be named after.
// With all that information the book is turned into a random artefact
// containing random spells of the given disciplines (random if none set).
bool make_book_theme_randart(item_def &book, int disc1, int disc2,
                             int num_spells, int max_levels,
                             spell_type incl_spell, string owner,
                             string title)
{
    vector<spell_type> spells;
    if (incl_spell != SPELL_NO_SPELL)
        spells.push_back(incl_spell);
    return make_book_theme_randart(book, spells, disc1, disc2,
                                   num_spells, max_levels, owner, title);
}

bool make_book_theme_randart(item_def &book,
                             vector<spell_type> incl_spells,
                             int disc1, int disc2,
                             int num_spells, int max_levels,
                             string owner, string title)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    god_type god;
    (void) origin_is_god_gift(book, &god);

    const bool completely_random =
        god == GOD_XOM || (god == GOD_NO_GOD && !origin_is_acquirement(book));

    if (num_spells == -1)
        num_spells = SPELLBOOK_SIZE;
    ASSERT_RANGE(num_spells, 0 + 1, SPELLBOOK_SIZE + 1);

    if (max_levels == -1)
        max_levels = 255;

    if (disc1 == 0 && disc2 == 0)
    {
        if (!_get_weighted_discs(completely_random, god, disc1, disc2))
        {
            if (completely_random)
                return false;

            // Rather than give up at this point, choose schools randomly.
            // This way, an acquirement won't fail once the player has
            // seen all spells.
            if (!_get_weighted_discs(true, god, disc1, disc2))
                return false;
        }
    }
    else if (disc2 == 0)
        disc2 = disc1;

    ASSERT(disc1 < (1 << (SPTYP_LAST_EXPONENT + 1)));
    ASSERT(disc2 < (1 << (SPTYP_LAST_EXPONENT + 1)));
    ASSERT(count_bits(disc1) == 1);
    ASSERT(count_bits(disc2) == 1);

    int disc1_pos = 0, disc2_pos = 0;
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        if (disc1 & (1 << i))
            disc1_pos = i;
        if (disc2 & (1 << i))
            disc2_pos = i;
    }

    book.plus  = num_spells | (max_levels << 8);
    book.plus2 = disc1_pos  | (disc2_pos  << 8);

    book.sub_type = BOOK_RANDART_THEME;
    _make_book_randart(book);

    int god_discard        = 0;
    int uncastable_discard = 0;

    vector<spell_type> spells;
    _get_spell_list(spells, disc1, disc2, god, !completely_random,
                    god_discard, uncastable_discard);

    if (num_spells > (int) spells.size())
        num_spells = spells.size();

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    _add_included_spells(chosen_spells, incl_spells);

    // If max_levels is 1, there might not be any suitable spells (for
    // example, in Charms).  Try one more time with max_levels = 2.
    while (!_get_weighted_spells(completely_random, god, disc1, disc2,
                                 num_spells, max_levels, spells,
                                 chosen_spells))
    {
        if (max_levels != 1)
            die("_get_weighted_spells() failed");

        ++max_levels;
    }

    sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE, _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    // Count how often each spell school appears in the book.
    int count[SPTYP_LAST_EXPONENT+1];
    for (int k = 0; k <= SPTYP_LAST_EXPONENT; k++)
        count[k] = 0;

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        if (chosen_spells[i] == SPELL_NO_SPELL)
            continue;

        for (int k = 0; k <= SPTYP_LAST_EXPONENT; k++)
            if (spell_typematch(chosen_spells[i], 1 << k))
                count[k]++;
    }

    // Remember the two dominant spell schools ...
    int max1 = 0;
    int max2 = 0;
    int num1 = 1;
    int num2 = 0;
    for (int k = 1; k <= SPTYP_LAST_EXPONENT; k++)
    {
        if (count[k] > count[max1])
        {
            max2 = max1;
            num2 = num1;
            max1 = k;
            num1 = 1;
        }
        else
        {
            if (count[k] == count[max1])
                num1++;

            if (max2 == max1 || count[k] > count[max2])
            {
                max2 = k;
                if (count[k] == count[max1])
                    num2 = num1;
                else
                    num2 = 1;
            }
            else if (count[k] == count[max2])
                num2++;
        }
    }

    // If there are several "secondary" disciplines with the same count
    // ignore all of them. Same, if the secondary discipline appears only once.
    if (num2 > 1 && count[max1] > count[max2] || count[max2] < 2)
        max2 = max1;

    // Remove spells that don't fit either discipline.
    _remove_nondiscipline_spells(chosen_spells, 1 << max1, 1 << max2);
    _add_included_spells(chosen_spells, incl_spells);

    // Resort spells.
    if (!incl_spells.empty())
        sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE, _compare_spells);
    ASSERT(chosen_spells[0] != SPELL_NO_SPELL);

    // ... and change disc1 and disc2 accordingly.
    disc1 = 1 << max1;
    if (max1 == max2)
        disc2 = disc1;
    else
        disc2 = 1 << max2;

    int highest_level = 0;
    int lowest_level  = 10;
    bool all_spells_disc1 = true;

    // Finally fill the spell vector.
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_vec[i].get_int() = chosen_spells[i];
        int diff = spell_difficulty(chosen_spells[i]);
        if (diff > highest_level)
            highest_level = diff;
        else if (diff < lowest_level)
            lowest_level = diff;

        if (all_spells_disc1 && is_valid_spell(chosen_spells[i])
            && !spell_typematch(chosen_spells[i], disc1))
        {
            all_spells_disc1 = false;
        }
    }

    // Every spell in the book is of school disc1.
    if (disc1 == disc2)
        all_spells_disc1 = true;

    // If the owner hasn't been set already use
    // a) the god's name for god gifts (only applies to Sif Muna and Xom),
    // b) a name depending on the spell disciplines, for pure books
    // c) a random name (all god gifts not named earlier)
    // d) an applicable god's name
    // ... else leave it unnamed (around 57% chance for non-god gifts)
    if (owner.empty())
    {
        const bool god_gift = (god != GOD_NO_GOD);
        if (god_gift && !one_chance_in(4))
            owner = god_name(god, false);
        else if (god_gift && one_chance_in(3) || one_chance_in(5))
        {
            bool highlevel = (highest_level >= 7 + random2(3)
                              && (lowest_level > 1 || coinflip()));

            if (disc1 != disc2)
            {
                string schools[2];
                schools[0] = spelltype_long_name(disc1);
                schools[1] = spelltype_long_name(disc2);
                sort(schools, schools + 2);
                string lookup = schools[0] + " " + schools[1];

                if (highlevel)
                    owner = getRandNameString("highlevel " + lookup + " owner");

                if (owner.empty() || owner == "__NONE")
                    owner = getRandNameString(lookup + " owner");

                if (owner == "__NONE")
                    owner = "";
            }

            if (owner.empty() && all_spells_disc1)
            {
                string lookup = spelltype_long_name(disc1);
                if (highlevel && disc1 == disc2)
                    owner = getRandNameString("highlevel " + lookup + " owner");

                if (owner.empty() || owner == "__NONE")
                    owner = getRandNameString(lookup + " owner");

                if (owner == "__NONE")
                    owner = "";
            }
        }

        if (owner.empty())
        {
            if (god_gift || one_chance_in(5)) // Use a random name.
                owner = make_name(random_int(), false);
            else if (!god_gift && one_chance_in(9))
            {
                god = GOD_SIF_MUNA;
                switch (disc1)
                {
                case SPTYP_NECROMANCY:
                    if (all_spells_disc1 && !one_chance_in(6))
                        god = GOD_KIKUBAAQUDGHA;
                    break;
                case SPTYP_CONJURATION:
                    if (all_spells_disc1 && !one_chance_in(4))
                        god = GOD_VEHUMET;
                    break;
                default:
                    break;
                }
                owner = god_name(god, false);
            }
        }
    }

    string name = "";

    if (!owner.empty())
    {
        name = apostrophise(owner) + " ";
        book.props["is_named"].get_bool() = true;
    }
    else
        book.props["is_named"].get_bool() = false;

    string bookname = "";
    if (!title.empty())
        bookname = title;
    else
    {
        // Sometimes use a completely random title.
        if (owner == "Xom" && !one_chance_in(20))
            bookname = getRandNameString("Xom_book_title");
        else if (one_chance_in(20) && (owner.empty() || one_chance_in(3)))
            bookname = getRandNameString("random_book_title");
        bookname = replace_name_parts(bookname, book);
    }

    if (!bookname.empty())
        name += getRandNameString("book_noun") + " of " + bookname;
    else
    {
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
            bookname = type_name + " ";

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
    }

    set_artefact_name(book, name);

    // Save primary/secondary disciplines back into the book.
    book.plus  = max1;
    book.plus2 = max2;

    return true;
}

// Give Roxanne a randart spellbook of the disciplines Transmutations/Earth
// that includes Statue Form and is named after her.
void make_book_Roxanne_special(item_def *book)
{
    int disc =  coinflip() ? SPTYP_TRANSMUTATION : SPTYP_EARTH;
    make_book_theme_randart(*book, disc, 0, 5, 19,
                            SPELL_STATUE_FORM, "Roxanne");
}

void make_book_Kiku_gift(item_def &book, bool first)
{
    book.sub_type = BOOK_RANDART_THEME;
    _make_book_randart(book);

    spell_type chosen_spells[SPELLBOOK_SIZE];
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        chosen_spells[i] = SPELL_NO_SPELL;

    if (first)
    {
        chosen_spells[0] = coinflip() ? SPELL_PAIN : SPELL_ANIMATE_SKELETON;
        if (you.species == SP_FELID || one_chance_in(3))
        {
            chosen_spells[1] = SPELL_CORPSE_ROT;
            chosen_spells[2] = SPELL_SUBLIMATION_OF_BLOOD;
        }
        else
        {
            chosen_spells[1] = coinflip() ? SPELL_CORPSE_ROT : SPELL_SUBLIMATION_OF_BLOOD;
            chosen_spells[2] = SPELL_LETHAL_INFUSION;
        }
        chosen_spells[3] = (you.species == SP_DEEP_DWARF
                            || you.species == SP_MUMMY
                            || coinflip())
                           ? SPELL_VAMPIRIC_DRAINING : SPELL_REGENERATION;
        chosen_spells[4] = SPELL_CONTROL_UNDEAD;
    }
    else
    {
        chosen_spells[0] = coinflip() ? SPELL_ANIMATE_DEAD : SPELL_TWISTED_RESURRECTION;
        chosen_spells[1] = (you.species == SP_FELID || coinflip())
                           ? SPELL_AGONY : SPELL_EXCRUCIATING_WOUNDS;
        chosen_spells[2] = random_choose(SPELL_BOLT_OF_DRAINING,
                                         SPELL_SIMULACRUM,
                                         SPELL_DEATH_CHANNEL,
                                         -1);
        spell_type extra_spell;
        do
        {
            extra_spell = random_choose(SPELL_ANIMATE_DEAD,
                                        SPELL_TWISTED_RESURRECTION,
                                        SPELL_AGONY,
                                        SPELL_EXCRUCIATING_WOUNDS,
                                        SPELL_BOLT_OF_DRAINING,
                                        SPELL_SIMULACRUM,
                                        SPELL_DEATH_CHANNEL,
                                        -1);
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

    sort(chosen_spells, chosen_spells + SPELLBOOK_SIZE, _compare_spells);

    CrawlHashTable &props = book.props;
    props.erase(SPELL_LIST_KEY);
    props[SPELL_LIST_KEY].new_vector(SV_INT).resize(SPELLBOOK_SIZE);

    CrawlVector &spell_vec = props[SPELL_LIST_KEY].get_vector();
    spell_vec.set_max_size(SPELLBOOK_SIZE);

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
        spell_vec[i].get_int() = chosen_spells[i];

    string name = "Kikubaaqudgha's ";
    book.props["is_named"].get_bool() = true;
    name += getRandNameString("book_name") + " ";
    string type_name = getRandNameString("Necromancy");
    if (type_name.empty())
        name += "Necromancy";
    else
        name += type_name;
    set_artefact_name(book, name);
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!is_artefact(book))
        return false;

    return book.props.exists("is_named")
           && book.props["is_named"].get_bool() == true;
}

void destroy_spellbook(const item_def &book)
{
    int j, maxlevel = 0;
    for (j = 0; j < SPELLBOOK_SIZE; j++)
    {
        spell_type stype = which_spell_in_book(book, j);
        if (stype == SPELL_NO_SPELL)
            continue;
        maxlevel = max(maxlevel, spell_difficulty(stype));
    }

    did_god_conduct(DID_DESTROY_SPELLBOOK, maxlevel + 5);
}

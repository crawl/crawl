/**
 * @file
 * @brief Spellbook contents array and management functions
**/

#include "AppHdr.h"

#include "options.h"
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
#include "command.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "end.h"
#include "god-conduct.h"
#include "invent.h"
#include "item-prop.h"
#include "libutil.h"
#include "localise.h"
#include "message.h"
#include "output.h"
#include "prompt.h"
#include "random-pick.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tilepick.h"
#include "transform.h"
#include "unicode.h"

// @noloc section start (keys)

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

// @noloc section end

struct sortable_spell
{
    sortable_spell(spell_type s) : spell(s),
                raw_fail(raw_spell_fail(s)),
                fail_rate(failure_rate_to_int(raw_fail)),
                fail_rate_colour(failure_rate_colour(s)),
                level(spell_levels_required(s)),
                difficulty(spell_difficulty(s)),
                name(localise(spell_title(s))),
                school(spell_schools_string(s))
    {
    }

    spell_type spell;
    int raw_fail;
    int fail_rate;
    int fail_rate_colour;
    int level;
    int difficulty;
    string name;
    string school; // TODO: set?

    friend bool operator==(const sortable_spell& x, const sortable_spell& y)
    {
        return x.spell == y.spell;
    }
};


struct hash_sortable_spell
{
    spell_type operator()(const sortable_spell& s) const
    {
        return s.spell;
    }
};

typedef vector<sortable_spell>                 spell_list;
typedef unordered_set<spell_type, hash<int>>   spell_set;

static const map<wand_type, spell_type> _wand_spells =
{
    { WAND_FLAME, SPELL_THROW_FLAME },
    { WAND_PARALYSIS, SPELL_PARALYSE },
    { WAND_DIGGING, SPELL_DIG },
    { WAND_ICEBLAST, SPELL_ICEBLAST },
    { WAND_POLYMORPH, SPELL_POLYMORPH },
    { WAND_CHARMING, SPELL_CHARMING },
    { WAND_ACID, SPELL_CORROSIVE_BOLT },
    { WAND_MINDBURST, SPELL_MINDBURST },
};


spell_type spell_in_wand(wand_type wand)
{
    if (item_type_removed(OBJ_WANDS, wand))
        return SPELL_NO_SPELL;

    if (const spell_type* const spl = map_find(_wand_spells, wand))
        return *spl;

    die("Unknown wand: %d", wand);
}

bool is_wand_spell(spell_type spell)
{
    for (auto p : _wand_spells)
        if (spell == p.second)
            return true;
    return false;
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

bool book_exists(book_type bt)
{
    switch (bt)
    {
    case BOOK_RANDART_LEVEL:
    case BOOK_RANDART_THEME:
    case BOOK_MANUAL:
    case NUM_BOOKS:
        return false;
    default:
        return !item_type_removed(OBJ_BOOKS, bt);
    }
}

#ifdef DEBUG
void validate_spellbooks()
{
    for (int i = 0; i < NUM_BOOKS; ++i)
    {
        const book_type book = static_cast<book_type>(i);
        if (!book_exists(book))
            continue;

        spell_type last = SPELL_NO_SPELL;
        for (spell_type spell : spellbook_template(book))
        {
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

            spell_flags flags = get_spell_flags(spell);

            if (flags & (spflag::monster | spflag::testing))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' contains invalid spell "
                             "'%s'",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(spell));
            }
        }
    }
}
#endif

bool is_player_book_spell(spell_type which_spell)
{
    for (int i = 0; i < NUM_BOOKS; ++i)
    {
        auto bt = static_cast<book_type>(i);
        if (!book_exists(bt))
            continue;
        for (spell_type spell : spellbook_template(bt))
            if (spell == which_spell)
                return true;
    }
    return false;
}

// Needs to be castable by the player somehow, but via idiosyncratic means.
// Religion reusing a spell enum, or something weirder like sonic wave.
// A spell doesn't need to be here if it just the beam type that is used.
static unordered_set<int> _player_nonbook_spells =
{
    // items
    SPELL_THUNDERBOLT,
    SPELL_PHANTOM_MIRROR, // this isn't cast directly, but the player code at
                          // least uses the enum value
    SPELL_SONIC_WAVE,
    // religion
    SPELL_SMITING,
    // Ds powers
    SPELL_HURL_DAMNATION,
};

bool is_player_spell(spell_type which_spell)
{
    return !spell_removed(which_spell)
        && (is_player_book_spell(which_spell)
            || is_wand_spell(which_spell)
            || _player_nonbook_spells.count(which_spell) > 0);
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
 * Populate the given list with all spells the player can currently memorise,
 * from library or Vehumet. Does not filter by currently known spells, spell
 * levels, etc.
 *
 * @param available_spells  A list to be populated with available spells.
 */
static void _list_available_spells(spell_set &available_spells)
{
    for (spell_type st = SPELL_NO_SPELL; st < NUM_SPELLS; st++)
    {
        if (you.spell_library[st])
            available_spells.insert(st);
    }

    // Handle Vehumet gifts
    for (auto gift : you.vehumet_gifts)
        available_spells.insert(gift);
}

bool player_has_available_spells()
{
    spell_set available_spells;
    _list_available_spells(available_spells);

    const int avail_slots = player_spell_levels();

    for (const spell_type spell : available_spells)
    {
        if (!you.has_spell(spell) && you_can_memorise(spell)
            && spell_difficulty(spell) <= avail_slots
            && spell_difficulty(spell) <= you.experience_level)
        {
            return true;
        }
    }
    return false;
}

static spell_list _get_spell_list(bool just_check = false,
                                  bool memorise_only = true)
{
    spell_list mem_spells;
    spell_set available_spells;
    _list_available_spells(available_spells);

    if (available_spells.empty())
    {
        if (!just_check)
        {
            if (you.has_mutation(MUT_INNATE_CASTER))
                mprf(MSGCH_PROMPT, "You need no library to learn spells.");
            else
                mprf(MSGCH_PROMPT, "Your library has no spells.");
        }
        return mem_spells;
    }

    int num_known      = 0;
    int num_misc       = 0;
    int num_restricted = 0;
    int num_low_xl     = 0;
    int num_low_levels = 0;
    int num_memable    = 0;

    for (const spell_type spell : available_spells)
    {
        if (you.has_spell(spell))
        {
            num_known++;

            // Divine Exegesis includes spells the player already knows.
            if (you.divine_exegesis)
                mem_spells.emplace_back(spell);
        }
        else if (!you_can_memorise(spell))
        {
            if (!memorise_only)
                mem_spells.emplace_back(spell);

            if (cannot_use_schools(get_spell_disciplines(spell)))
                num_restricted++;
            else
                num_misc++;
        }
        else
        {
            mem_spells.emplace_back(spell);

            const int avail_slots = player_spell_levels();

            // don't filter out spells that are too high-level for us; we
            // probably still want to see them. (since that's temporary.)

            if (mem_spells.back().difficulty > you.experience_level)
                num_low_xl++;
            else if (avail_slots < mem_spells.back().level)
                num_low_levels++;
            else
                num_memable++;
        }
    }

    const int total = num_known + num_misc + num_low_xl + num_low_levels
                      + num_restricted;

    const char* unavail_reason;

    if (num_memable || num_low_levels > 0 || num_low_xl > 0)
        unavail_reason = "";
    else if (num_known == total)
        unavail_reason = "You already know all available spells.";
    else if (num_restricted == total || num_restricted + num_known == total)
    {
        unavail_reason = "You cannot currently memorise any of the available "
                         "spells because you cannot use those schools of "
                         "magic.";
    }
    else if (num_misc == total || (num_known + num_misc) == total
             || num_misc + num_known + num_restricted == total)
    {
        unavail_reason = "You cannot memorise any of the available spells.";
    }
    else
    {
        unavail_reason = "You can't memorise any new spells for an unknown "
                         "reason; please file a bug report.";
    }

    if (!just_check && *unavail_reason)
        mprf(MSGCH_PROMPT, "%s", unavail_reason);
    return mem_spells;
}

bool library_add_spells(vector<spell_type> spells)
{
    vector<spell_type> new_spells;
    for (spell_type st : spells)
    {
        if (!you.spell_library[st])
        {
            you.spell_library.set(st, true);
            bool memorise = you_can_memorise(st);
            if (memorise)
                new_spells.push_back(st);
            if (!memorise || Options.auto_hide_spells)
                you.hidden_spells.set(st, true);
        }
    }
    if (!new_spells.empty())
    {
        vector<string> spellnames(new_spells.size());
        transform(new_spells.begin(), new_spells.end(), spellnames.begin(), spell_title);
        mprf(spellnames.size() > 1 ? "You add the spells %s to your library."
                                   : "You add the spell %s to your library.",
             comma_separated_line(spellnames.begin(),
                                  spellnames.end()).c_str());
        return true;
    }
    return false;
}

bool has_spells_to_memorise(bool silent)
{
    // TODO: this is a bit dumb
    spell_list mem_spells(_get_spell_list(silent, true));
    return !mem_spells.empty();
}

static bool _sort_mem_spells(const sortable_spell &a, const sortable_spell &b)
{
    // Put unmemorisable spells last
    const bool mem_a = you_can_memorise(a.spell);
    const bool mem_b = you_can_memorise(b.spell);
    if (mem_a != mem_b)
        return mem_a;

    // List the Vehumet gifts at the very top.
    const bool offering_a = vehumet_is_offering(a.spell);
    const bool offering_b = vehumet_is_offering(b.spell);
    if (offering_a != offering_b)
        return offering_a;

    // List spells we can memorise right away first.
    const int player_levels = player_spell_levels();
    if (player_levels >= a.level && player_spell_levels() < b.level)
        return true;
    else if (player_spell_levels() < a.level && player_spell_levels() >= b.level)
        return false;

    // Don't sort by failure rate beyond what the player can see in the
    // success descriptions.
    if (a.fail_rate != b.fail_rate)
        return a.fail_rate < b.fail_rate;

    if (a.difficulty != b.difficulty)
        return a.difficulty < b.difficulty;

    return strcasecmp(spell_title(a.spell), spell_title(b.spell)) < 0;
}

static bool _sort_divine_spells(const sortable_spell &a, const sortable_spell &b)
{
    // Put useless spells last
    const bool useful_a = !spell_is_useless(a.spell, true, true);
    const bool useful_b = !spell_is_useless(b.spell, true, true);
    if (useful_a != useful_b)
        return useful_a;

    // Put higher levels spells first, as they're more likely to be what we
    // want.
    if (a.difficulty != b.difficulty)
        return a.difficulty > b.difficulty;

    return strcasecmp(spell_title(a.spell), spell_title(b.spell)) < 0;
}

vector<spell_type> get_sorted_spell_list(bool silent, bool memorise_only)
{
    spell_list mem_spells(_get_spell_list(silent, memorise_only));

    if (you.divine_exegesis)
        sort(mem_spells.begin(), mem_spells.end(), _sort_divine_spells);
    else
        sort(mem_spells.begin(), mem_spells.end(), _sort_mem_spells);

    vector<spell_type> result;
    for (auto s : mem_spells)
        result.push_back(s.spell);

    return result;
}

class SpellLibraryMenu : public Menu
{
public:
    enum class action { cast, memorise, describe, hide, unhide } current_action;

protected:
    virtual formatted_string calc_title() override
    {
        string action_name;
        if (current_action == action::cast)
            action_name = "Cast";
        else if (current_action == action::memorise)
            action_name = "Memorise";
        else if (current_action == action::describe)
            action_name = "Describe";
        else if (current_action == action::hide)
            action_name = "Hide";
        else
            action_name = "Show";

        string text = localise("Spells");
        text += make_stringf(" (%s)", localise(action_name).c_str());
        text = chop_string(text, 34, true);

        text += chop_string(localise("Type"), 30, true);

        if (!you.divine_exegesis)
            text += chop_string(localise("Failure"), 9, true);

        text += localise("Level");

        return formatted_string(text, WHITE);
    }

private:
    spell_list& spells;
    string spell_levels_str;
    string search_text;
    int hidden_count;

    void update_more()
    {
        // TODO: convert this all to widgets
        ostringstream desc;

        // line 1
        desc << spell_levels_str << "    ";
        if (search_text.size())
        {
            // TODO: couldn't figure out how to do this in pure c++
            string match_text = localise("Matches:");
            match_text += make_stringf(" '<w>%.20s</w>'", // @noloc
                            replace_all(search_text, "<", "<<").c_str());
            int escaped_count = (int) std::count(search_text.begin(),
                                                    search_text.end(), '<');
            // the width here is a bit complicated because it needs to ignore
            // any color codes and escaped '<'s.
            desc << std::left << std::setw(43 + escaped_count) << match_text;
        }
        else
            desc << std::setw(36) << "";
        if (hidden_count)
        {
            if (hidden_count == 1)
                desc << " " << localise("1 spell hidden");
            else
                desc << localise("%d spells hidden", hidden_count);
        }
        desc << "\n";

        const string act = localise(you.divine_exegesis ? "Cast" : "Memorise");
        // line 2
        desc << "[<yellow>?</yellow>] " // @noloc
             << chop_string(localise("help"), 20)
             << "[<yellow>" // @noloc
             << localise("Ctrl-F")
             << "</yellow>] " // @noloc
             << chop_string(localise("search"), 12)
             << "[<yellow>!</yellow>] "; // @noloc
        desc << ( current_action == action::cast
                            ? localise("<w>Cast</w>|Describe|Hide|Show")
                 : current_action == action::memorise
                            ? localise("<w>Memorise</w>|Describe|Hide|Show")
                 : current_action == action::describe
                            ? act + localise("|<w>Describe</w>|Hide|Show")
                 : current_action == action::hide
                            ? act + localise("|Describe|<w>Hide</w>|Show")
                 : act + localise("|Describe|Hide|<w>Show</w>"));

        set_more(formatted_string::parse_string(desc.str()));
    }

    virtual bool process_key(int keyin) override
    {
        bool entries_changed = false;
        switch (keyin)
        {
        case '!':
#ifdef TOUCH_UI
        case CK_TOUCH_DUMMY:
#endif
            switch (current_action)
            {
                case action::cast:
                case action::memorise:
                    current_action = action::describe;
                    entries_changed = true; // need to add hotkeys
                    break;
                case action::describe:
                    current_action = action::hide;
                    break;
                case action::hide:
                    current_action = action::unhide;
                    entries_changed = true;
                    break;
                case action::unhide:
                    current_action = you.divine_exegesis ? action::cast
                                                          : action::memorise;
                    entries_changed = true;
                    break;
            }
            update_title();
            update_more();
            break;

        case CONTROL('F'):
        {
            char linebuf[80] = "";
            const string prompt = localise("Search for what? (regex) ");
            const bool validline = title_prompt(linebuf, sizeof linebuf,
                                                prompt.c_str());
            string old_search = search_text;
            if (validline)
                search_text = linebuf;
            else
                search_text = "";
            entries_changed = old_search != search_text;
            break;
        }

        case '?':
            show_spell_library_help();
            break;
        case CK_MOUSE_B2:
        case CK_MOUSE_CMD:
        CASE_ESCAPE
            if (search_text.size())
            {
                search_text = "";
                entries_changed = true;
                break;
            }
            // intentional fallthrough if search is empty
        default:
            return Menu::process_key(keyin);
        }

        if (entries_changed)
        {
            update_entries();
            update_more();
        }
        return true;
    }

    colour_t entry_colour(const sortable_spell& entry)
    {
        if (vehumet_is_offering(entry.spell))
            return LIGHTBLUE;
        else
        {
            return spell_highlight_by_utility(entry.spell, COL_UNKNOWN, false,
                    you.divine_exegesis ? false : true);
        }
    }

    // Update the list of spells. If show_hidden is true, show only hidden
    // ones; otherwise, show only non-hidden ones.
    void update_entries()
    {
        clear();
        hidden_count = 0;
        const bool show_hidden = current_action == action::unhide;
        menu_letter hotkey;
        text_pattern pat(search_text, true);
        for (auto& spell : spells)
        {
            if (!search_text.empty()
                && !pat.matches(spell.name)
                && !pat.matches(spell.school))
            {
                continue;
            }

            const bool spell_hidden = you.hidden_spells.get(spell.spell);

            if (spell_hidden)
                hidden_count++;

            if (spell_hidden != show_hidden)
                continue;

            // TODO: it might be cleaner to reorder and put unavailable spells
            // under a different category
            // n.b. this memorise code only checks spell-specific constraints,
            // so it adds hotkeys e.g. if you lack the spell levels. Not sure
            // if this is intentional.
            const bool unavailable = current_action == action::memorise &&
                                        !you_can_memorise(spell.spell)
                                || current_action == action::cast &&
                                    spell_is_useless(spell.spell, true, true);

            const colour_t colour = unavailable ? (colour_t) DARKGRAY
                                                : entry_colour(spell);

            ostringstream desc;
            desc << "<" << colour_to_str(colour) << ">";

            desc << left;
            desc << chop_string(spell.name, 29) + " ";
            desc << chop_string(spell.school, 29) + " ";

            desc << "</" << colour_to_str(colour) << ">";

            if (!you.divine_exegesis)
            {
                desc << "<" << colour_to_str(spell.fail_rate_colour) << ">";
                desc << chop_string(failure_rate_to_string(spell.raw_fail), 12);
                desc << "</" << colour_to_str(spell.fail_rate_colour) << ">";
            }

            desc << spell.difficulty;

            MenuEntry* me = new MenuEntry(desc.str(), MEL_ITEM, 1,
                    // don't add a hotkey if you can't memorise/cast it
                    unavailable ? 0 : char(hotkey));
            // But do increment hotkeys anyway, to keep the hotkeys consistent.
            ++hotkey;

            me->indent_no_hotkeys = true;
            me->colour = colour;
            me->add_tile(tile_def(tileidx_spell(spell.spell)));

            me->data = &(spell.spell);
            add_entry(me);
        }
        update_menu(true);
    }

public:
    SpellLibraryMenu(spell_list& list)
        : Menu(MF_SINGLESELECT | MF_ANYPRINTABLE
               | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING
               // To have the ctrl-f menu show up in webtiles
               | MF_ALLOW_FILTER, "spell"),
        current_action(you.divine_exegesis ? action::cast : action::memorise),
        spells(list),
        hidden_count(0)
    {
        set_highlighter(nullptr);
        // Actual text handled by calc_title
        set_title(new MenuEntry(""), true, true);

        if (you.divine_exegesis)
        {
            spell_levels_str = "<lightgreen>"; // @noloc
            spell_levels_str += localise(
                "Select a spell to cast with Divine Exegesis: %d MP available",
                you.magic_points
            );
            spell_levels_str += "</lightgreen>"; // @noloc
        }
        else
        {
            spell_levels_str = "<lightgreen>"; // @noloc
            if (player_spell_levels() == 1)
                spell_levels_str += localise("1 spell level left");
            else
            {
                spell_levels_str +=
                    localise("%d spell levels left", player_spell_levels());
            }
            spell_levels_str += "</lightgreen>"; // @noloc
            if (player_spell_levels() < 9)
                spell_levels_str += " ";
        }
        set_more(formatted_string::parse_string(spell_levels_str + "\n"));

#ifdef USE_TILE_LOCAL
        FontWrapper *font = tiles.get_crt_font();
        int title_width = font->string_width(calc_title());
        m_ui.vbox->min_size().width = 38 + title_width + 10;
#endif
        m_ui.scroller->expand_v = true; // TODO: doesn't work on webtiles

        update_entries();
        update_more();
        on_single_selection = [this](const MenuEntry& item)
        {
            const spell_type spell = *static_cast<spell_type*>(item.data);
            ASSERT(is_valid_spell(spell));

            switch (current_action)
            {
            case action::memorise:
            case action::cast:
                return false;
            case action::describe:
                describe_spell(spell, nullptr);
                break;
            case action::hide:
            case action::unhide:
                you.hidden_spells.set(spell, !you.hidden_spells.get(spell));
                update_entries();
                update_menu(true);
                update_more();
                break;
            }
            return true;
        };
    }
};

static spell_type _choose_mem_spell(spell_list &spells)
{
    // If we've gotten this far, we know that at least one spell here is
    // memorisable, which is enough.

    SpellLibraryMenu spell_menu(spells);

    const vector<MenuEntry*> sel = spell_menu.show();
    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }
    if (sel.empty())
        return SPELL_NO_SPELL;
    const spell_type spell = *static_cast<spell_type*>(sel[0]->data);
    ASSERT(is_valid_spell(spell));
    return spell;
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
    spell_list spells(_get_spell_list());
    if (spells.empty())
        return false;

    sort(spells.begin(), spells.end(), _sort_mem_spells);

    spell_type specspell = _choose_mem_spell(spells);

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
 * @param   interactive whether to do the interactive prompt
 * @return             true if the player learned the spell, false
 *                     otherwise.
*/
bool learn_spell(spell_type specspell, bool wizard, bool interactive)
{
    if (!_learn_spell_checks(specspell, wizard))
        return false;

    string mem_spell_warning_string = god_spell_warn_string(specspell, you.religion);

    if (!mem_spell_warning_string.empty())
        mprf(MSGCH_WARN, "%s", mem_spell_warning_string.c_str());

    if (!wizard)
    {
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

    if (interactive)
    {
        const string prompt = localise(
                 spell_levels_required(specspell) == 1
                 ? "Memorise %s, consuming %d spell level and leaving %d?"
                 : "Memorise %s, consuming %d spell levels and leaving %d?",
                 spell_title(specspell), spell_levels_required(specspell),
                 player_spell_levels() - spell_levels_required(specspell));

        if (!yesno(prompt.c_str(), true, 'n', false))
        {
            canned_msg(MSG_OK);
            return false;
        }
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

    quiver::on_actions_changed();

    return true;
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    // No "A Great Wizards, Vol. II"
    if (book.sub_type == BOOK_BIOGRAPHIES_II
        || book.sub_type == BOOK_BIOGRAPHIES_VII
        || book.sub_type == BOOK_OZOCUBU
        || book.sub_type == BOOK_UNRESTRAINED)
    {
        return true;
    }

    if (!is_artefact(book))
        return false;

    return book.props.exists(BOOK_TITLED_KEY)
           && book.props[BOOK_TITLED_KEY].get_bool() == true;
}

spret divine_exegesis(bool fail)
{
    unwind_var<bool> dk(you.divine_exegesis, true);

    spell_list spells(_get_spell_list(true, true));
    if (spells.empty())
    {
        mpr("You don't know of any spells!");
        return spret::abort;
    }

    sort(spells.begin(), spells.end(), _sort_divine_spells);
    // If we've gotten this far, we know at least one useful spell.

    SpellLibraryMenu spell_menu(spells);

    const vector<MenuEntry*> sel = spell_menu.show();
    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }

    if (sel.empty())
        return spret::abort;

    const spell_type spell = *static_cast<spell_type*>(sel[0]->data);
    if (spell == SPELL_NO_SPELL)
        return spret::abort;

    ASSERT(is_valid_spell(spell));

    if (fail)
        return spret::fail;

    if (cast_a_spell(false, spell))
        return spret::success;

    return spret::abort;
}

/// For a given dungeon depth (or item level), how much weight should we give
/// to a spellbook for having a spell of the given level in it?
/// We add a flat 10 to all these weights later.
static const vector<random_pick_entry<int>> spell_level_weights = {
    {  0, 35, 100, FLAT, 1 },

    {  0, 35, 100, FLAT, 2 },

    {  0, 35, 100, FLAT, 3 },

    {  4,  8, 100, RISE, 4 },
    {  9, 35, 100, FLAT, 4 },

    {  6, 10, 100, RISE, 5 },
    { 11, 35,  80, FLAT, 5 },

    { 10, 14, 100, RISE, 6 },
    { 15, 35, 100, FLAT, 6 },

    { 14, 22, 100, RISE, 7 },
    { 23, 35, 100, FLAT, 7 },

    { 18, 26, 100, RISE, 8 },
    { 27, 35, 100, FLAT, 8 },

    { 22, 30, 100, RISE, 9 },
    { 31, 35, 100, FLAT, 9 },
};

/// Cap item_level at this for generation. This is roughly the bottom of the hells.
static const int MAX_BOOK_LEVEL = 35;

static int _book_weight(book_type book, int item_level, int scale)
{
    item_level = min(item_level, MAX_BOOK_LEVEL);
    random_picker<int, 9> spell_weighter;
    int n_spells = 0;
    int weight = 0;
    for (spell_type spell : spellbook_template(book))
    {
        const int lvl = spell_difficulty(spell);
        const int spell_weight =
            spell_weighter.probability_at(lvl, spell_level_weights,
                                          item_level, scale);
        weight += spell_weight + scale/20; // Every spell gets a flat weight.
        ++n_spells;
    }
    return weight / n_spells;
}

book_type choose_book_type(int item_level)
{
    map<book_type, int> book_weights;
    for (int i = 0; i < NUM_BOOKS; i++)
    {
        const book_type bt = (book_type)i;
        if (book_exists(bt))
            book_weights[bt] = _book_weight(bt, item_level, 1000);
    }
    return *random_choose_weighted(book_weights);
}

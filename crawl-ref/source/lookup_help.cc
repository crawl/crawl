/**
 * @file
 * @brief Let the player search for descriptions of monsters, items, etc.
 **/

#include "AppHdr.h"

#include "lookup_help.h"

#include <functional> // mem_fn

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "database.h"
#include "decks.h"
#include "describe.h"
#include "describe-god.h"
#include "describe-spells.h"
#include "directn.h"
#include "english.h"
#include "enum.h"
#include "env.h"
#include "godmenu.h"
#include "itemprop.h"
#include "itemname.h"
#include "items.h"
#include "libutil.h" // map_find
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "mon-info.h"
#include "mon-tentacle.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "stringutil.h"
#include "spl-book.h"
#include "spl-util.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tilepick.h"
#endif
#include "view.h"
#include "viewchar.h"


static bool _compare_mon_names(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_info* a = static_cast<monster_info* >(entry_a->data);
    monster_info* b = static_cast<monster_info* >(entry_b->data);

    if (a->type == b->type)
        return false;

    string a_name = mons_type_name(a->type, DESC_PLAIN);
    string b_name = mons_type_name(b->type, DESC_PLAIN);
    return lowercase(a_name) < lowercase(b_name);
}

// Compare monsters by location-independent level, or by hitdice if
// levels are equal, or by name if both level and hitdice are equal.
static bool _compare_mon_toughness(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_info* a = static_cast<monster_info* >(entry_a->data);
    monster_info* b = static_cast<monster_info* >(entry_b->data);

    if (a->type == b->type)
        return false;

    int a_toughness = mons_avg_hp(a->type);
    int b_toughness = mons_avg_hp(b->type);

    if (a_toughness == b_toughness)
    {
        string a_name = mons_type_name(a->type, DESC_PLAIN);
        string b_name = mons_type_name(b->type, DESC_PLAIN);
        return lowercase(a_name) < lowercase(b_name);
    }
    return a_toughness > b_toughness;
}

class DescMenu : public Menu
{
public:
    DescMenu(int _flags, bool _show_mon, bool _text_only)
    : Menu(_flags, "", _text_only), sort_alpha(true),
    showing_monsters(_show_mon)
    {
        set_highlighter(nullptr);

        if (_show_mon)
            toggle_sorting();

        set_prompt();
    }

    bool sort_alpha;
    bool showing_monsters;

    void set_prompt()
    {
        string prompt = "Describe which? ";

        if (showing_monsters)
        {
            if (sort_alpha)
                prompt += "(CTRL-S to sort by monster toughness)";
            else
                prompt += "(CTRL-S to sort by name)";
        }
        set_title(new MenuEntry(prompt, MEL_TITLE));
    }

    void sort()
    {
        if (!showing_monsters)
            return;

        if (sort_alpha)
            ::sort(items.begin(), items.end(), _compare_mon_names);
        else
            ::sort(items.begin(), items.end(), _compare_mon_toughness);

        for (unsigned int i = 0, size = items.size(); i < size; i++)
        {
            const char letter = index_to_letter(i);

            items[i]->hotkeys.clear();
            items[i]->add_hotkey(letter);
        }
    }

    void toggle_sorting()
    {
        if (!showing_monsters)
            return;

        sort_alpha = !sort_alpha;

        sort();
        set_prompt();
    }
};

static vector<string> _get_desc_keys(string regex, db_find_filter filter)
{
    vector<string> key_matches = getLongDescKeysByRegex(regex, filter);

    if (key_matches.size() == 1)
        return key_matches;
    else if (key_matches.size() > 52)
        return key_matches;

    vector<string> body_matches = getLongDescBodiesByRegex(regex, filter);

    if (key_matches.empty() && body_matches.empty())
        return key_matches;
    else if (key_matches.empty() && body_matches.size() == 1)
        return body_matches;

    // Merge key_matches and body_matches, discarding duplicates.
    vector<string> tmp = key_matches;
    tmp.insert(tmp.end(), body_matches.begin(), body_matches.end());
    sort(tmp.begin(), tmp.end());
    vector<string> all_matches;
    for (unsigned int i = 0, size = tmp.size(); i < size; i++)
        if (i == 0 || all_matches[all_matches.size() - 1] != tmp[i])
            all_matches.push_back(tmp[i]);

    return all_matches;
}

static vector<string> _get_monster_keys(ucs_t showchar)
{
    vector<string> mon_keys;

    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        if (i == MONS_PROGRAM_BUG)
            continue;

        const monsterentry *me = get_monster_data(i);

        if (me == nullptr || me->name == nullptr || me->name[0] == '\0')
            continue;

        if (me->mc != i)
            continue;

        if (getLongDescription(me->name).empty())
            continue;

        if ((ucs_t)me->basechar == showchar)
            mon_keys.push_back(me->name);
    }

    return mon_keys;
}

static vector<string> _get_god_keys()
{
    vector<string> names;

    for (int i = GOD_NO_GOD + 1; i < NUM_GODS; i++)
    {
        god_type which_god = static_cast<god_type>(i);
        names.push_back(god_name(which_god));
    }

    return names;
}

static vector<string> _get_branch_keys()
{
    vector<string> names;

    for (branch_iterator it; it; ++it)
    {
        // Skip unimplemented branches
        if (branch_is_unfinished(it->id))
            continue;

        names.push_back(it->shortname);
    }
    return names;
}

static bool _monster_filter(string key, string body)
{
    monster_type mon_num = get_monster_by_name(key.c_str());
    return mons_class_flag(mon_num, M_CANT_SPAWN);
}

static bool _spell_filter(string key, string body)
{
    if (!ends_with(key, " spell"))
        return true;
    key.erase(key.length() - 6);

    spell_type spell = spell_by_name(key);

    if (spell == SPELL_NO_SPELL)
        return true;

    if (get_spell_flags(spell) & SPFLAG_TESTING)
        return !you.wizard;

    return false;
}

static bool _item_filter(string key, string body)
{
    return item_kind_by_name(key).base_type == OBJ_UNASSIGNED;
}

static bool _skill_filter(string key, string body)
{
    lowercase(key);
    string name;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
    {
        skill_type sk = static_cast<skill_type>(i);
        // There are a couple of nullptr entries in the skill set.
        if (!skill_name(sk))
            continue;

        name = lowercase_string(skill_name(sk));

        if (name.find(key) != string::npos)
            return false;
    }
    return true;
}

static bool _feature_filter(string key, string body)
{
    return feat_by_desc(key) == DNGN_UNSEEN;
}

static bool _card_filter(string key, string body)
{
    lowercase(key);

    // Every card description contains the keyword "card".
    if (!ends_with(key, " card"))
        return true;
    key.erase(key.length() - 5);

    for (int i = 0; i < NUM_CARDS; ++i)
    {
        if (key == lowercase_string(card_name(static_cast<card_type>(i))))
            return false;
    }
    return true;
}

static bool _ability_filter(string key, string body)
{
    lowercase(key);
    if (!ends_with(key, " ability"))
        return true;
    key.erase(key.length() - 8);

    if (string_matches_ability_name(key))
        return false;

    return true;
}

typedef void (*db_keys_recap)(vector<string>&);

static void _recap_mon_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        monster_type type = get_monster_by_name(keys[i]);
        keys[i] = mons_type_name(type, DESC_PLAIN);
    }
}

static void _recap_feat_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        dungeon_feature_type type = feat_by_desc(keys[i]);
        if (type == DNGN_ENTER_SHOP)
            keys[i] = "A shop";
        else
        {
            keys[i] = feature_description(type, NUM_TRAPS, "", DESC_A,
                                          false);
        }
    }
}

static void _recap_card_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        lowercase(keys[i]);

        for (int j = 0; j < NUM_CARDS; ++j)
        {
            card_type card = static_cast<card_type>(j);
            if (keys[i] == lowercase_string(card_name(card)) + " card")
            {
                keys[i] = string(card_name(card)) + " card";
                break;
            }
        }
    }
}

static bool _is_rod_spell(spell_type spell)
{
    if (spell == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_RODS; i++)
        if (spell_in_rod(static_cast<rod_type>(i)) == spell)
            return true;

    return false;
}

// Adds a list of all books/rods that contain a given spell (by name)
// to a description string.
static bool _append_books(string &desc, item_def &item, string key)
{
    if (ends_with(key, " spell"))
        key.erase(key.length() - 6);

    spell_type type = spell_by_name(key, true);

    if (type == SPELL_NO_SPELL)
        return false;

    desc += "\nType:       ";
    bool already = false;

    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        if (spell_typematch(type, 1 << i))
        {
            if (already)
                desc += "/" ;

            desc += spelltype_long_name(1 << i);
            already = true;
        }
    }
    if (!already)
        desc += "None";

    desc += make_stringf("\nLevel:      %d", spell_difficulty(type));

    if (!you_can_memorise(type))
        desc += "\n" + desc_cannot_memorise_reason(type);

    set_ident_flags(item, ISFLAG_IDENT_MASK);
    vector<string> books;
    vector<string> rods;

    item.base_type = OBJ_BOOKS;
    for (int i = 0; i < NUM_FIXED_BOOKS; i++)
        for (spell_type sp : spellbook_template(static_cast<book_type>(i)))
            if (sp == type)
            {
                item.sub_type = i;
                books.push_back(item.name(DESC_PLAIN));
            }

    item.base_type = OBJ_RODS;
    for (int i = 0; i < NUM_RODS; i++)
    {
        item.sub_type = i;
        if (spell_in_rod(static_cast<rod_type>(i)) == type)
            rods.push_back(item.name(DESC_BASENAME));
    }

    if (!books.empty())
    {
        desc += "\n\nThis spell can be found in the following book";
        if (books.size() > 1)
            desc += "s";
        desc += ":\n ";
        desc += comma_separated_line(books.begin(), books.end(), "\n ", "\n ");

    }

    if (!rods.empty())
    {
        desc += "\n\nThis spell can be found in the following rod";
        if (rods.size() > 1)
            desc += "s";
        desc += ":\n ";
        desc += comma_separated_line(rods.begin(), rods.end(), "\n ", "\n ");
    }

    if (books.empty() && rods.empty())
        desc += "\n\nThis spell is not found in any books or rods.";

    return true;
}

// Returns the result of the keypress.
static int _do_description(string key, string type, const string &suffix,
                           string footer = "")
{
    god_type which_god = str_to_god(key);
    if (which_god != GOD_NO_GOD)
    {
#ifdef USE_TILE_WEB
        tiles_crt_control show_as_menu(CRT_MENU, "describe_god");
#endif
        describe_god(which_god, true);
        return true;
    }

    describe_info inf;
    inf.quote = getQuoteString(key);

    string desc = getLongDescription(key);
    int width = min(80, get_number_of_cols());

    monster_type mon_num = get_monster_by_name(key);
    // Don't attempt to get more information on ghost demon
    // monsters, as the ghost struct has not been initialised, which
    // will cause a crash.  Similarly for zombified monsters, since
    // they require a base monster.
    if (mon_num != MONS_PROGRAM_BUG && !mons_is_ghost_demon(mon_num)
        && !mons_class_is_zombified(mon_num))
    {
        monster_info mi(mon_num);
        // Avoid slime creature being described as "buggy"
        if (mi.type == MONS_SLIME_CREATURE)
            mi.slime_size = 1;
        return describe_monsters(mi, true, footer);
    }

    int thing_created = get_mitm_slot();
    if (thing_created != NON_ITEM
        && (type == "item" || type == "spell"))
    {
        char name[80];
        strncpy(name, key.c_str(), sizeof(name));
        if (get_item_by_name(&mitm[thing_created], name, OBJ_WEAPONS))
        {
            append_weapon_stats(desc, mitm[thing_created]);
            desc += "\n";
        }
        else if (get_item_by_name(&mitm[thing_created], name, OBJ_ARMOUR))
        {
            append_armour_stats(desc, mitm[thing_created]);
            desc += "\n";
        }
        else if (get_item_by_name(&mitm[thing_created], name, OBJ_MISSILES))
        {
            append_missile_info(desc, mitm[thing_created]);
            desc += "\n";
        }
        else if (type == "spell"
                 || get_item_by_name(&mitm[thing_created], name, OBJ_BOOKS)
                 || get_item_by_name(&mitm[thing_created], name, OBJ_RODS))
        {
            if (!_append_books(desc, mitm[thing_created], key))
            {
                // FIXME: Duplicates messages from describe.cc.
                if (!player_can_memorise_from_spellbook(mitm[thing_created]))
                {
                    desc += "This book is beyond your current level "
                    "of understanding.";
                }
                desc += describe_item_spells(mitm[thing_created]);
            }
        }
    }
    else if (type == "card")
    {
        // 5 - " card"
        card_type which_card =
        name_to_card(key.substr(0, key.length() - 5));
        if (which_card != NUM_CARDS)
            desc += which_decks(which_card) + "\n";
    }

    // Now we don't need the item anymore.
    if (thing_created != NON_ITEM)
        destroy_item(thing_created);

    inf.body << desc;

    if (ends_with(key, suffix))
        key.erase(key.length() - suffix.length());
    key = uppercase_first(key);
    linebreak_string(footer, width - 1);

    inf.footer = footer;
    inf.title  = key;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "description");
#endif

    print_description(inf);
    return getchm();
}



/// A set of optional functionality for lookup types.
enum lookup_type_flag
{
    LTYPF_NONE          = 0,
    /// append the 'type' to the db lookup (e.g. "<input> spell")
    LTYPF_DB_SUFFIX     = 1<<0,
    /// single letter input searches for corresponding glyphs (o is for orc)
    LTYPF_SINGLE_LETTER = 1<<1,
    /// whether the search functionality should be turned off
    /// (just display a constant list)
    LTYPF_DISABLE_REGEX = 1<<2,
    /// whether the sorting functionality should be turned off
    LTYPF_DISABLE_SORT  = 1<<3,
};
DEF_BITFIELD(lookup_type_flags, lookup_type_flag);

/// A description of a lookup that the player can do. (e.g. (M)onster data)
class LookupType
{
public:
    LookupType(char _symbol, string _type, db_keys_recap _recap,
               db_find_filter _filter_forbid, lookup_type_flags _flags)
    : symbol(_symbol), type(_type), recap(_recap),
      filter_forbid(_filter_forbid), flags(_flags)
    { }

    /**
     * How should this type be expressed in the prompt string?
     *
     * @return The 'type', with the first instance of the 'symbol' found &
     *          replaced with an uppercase version surrounded by parens
     *          e.g. "monster", 'm' -> "(M)onster"
     */
    string prompt_string() const
    {
        string prompt_str = lowercase_string(type);
        const size_t symbol_pos = prompt_str.find(tolower(symbol));
        ASSERT(symbol_pos != string::npos);

        prompt_str.replace(symbol_pos, 1, make_stringf("(%c)",
                                                       toupper(symbol)));
        return prompt_str;
    }

    /**
     * A suffix to be appended to the provided search string when looking for
     * db info.
     *
     * @return      An appropriate suffix for types that need them (e.g.
     *              " cards"); otherwise "".
     */
    string suffix() const
    {
        if (flags & LTYPF_DB_SUFFIX)
            return " " + type;
        return "";
    }

public:
    /// The letter pressed to choose this (e.g. 'M'). case insensitive
    char symbol;
    /// A description of the lookup type (e.g. "monster"). case insensitive
    string type;
    /// no idea, sorry :(
    db_keys_recap recap;
    /// a function returning 'true' if the search result corresponding to
    /// the corresponding search should be filtered out of the results
    db_find_filter filter_forbid;
    /// A set of optional functionality; see lookup_type_flag for details
    lookup_type_flags flags;
};

static const vector<LookupType> lookup_types = {
    LookupType('M', "monster", _recap_mon_keys, _monster_filter,
               LTYPF_SINGLE_LETTER),
    LookupType('S', "spell", nullptr, _spell_filter,
               LTYPF_DB_SUFFIX),
    LookupType('K', "skill", nullptr, _skill_filter,
               LTYPF_NONE),
    LookupType('A', "ability", nullptr, _ability_filter,
               LTYPF_DB_SUFFIX),
    LookupType('C', "card", _recap_card_keys, _card_filter,
               LTYPF_DB_SUFFIX),
    LookupType('I', "item", nullptr, _item_filter,
               LTYPF_SINGLE_LETTER),
    LookupType('F', "feature", _recap_feat_keys, _feature_filter,
               LTYPF_NONE),
    LookupType('G', "god", nullptr, nullptr,
               LTYPF_DISABLE_REGEX),
    LookupType('B', "branch", nullptr, nullptr,
               LTYPF_DISABLE_REGEX | LTYPF_DISABLE_SORT)
};

/**
 * Build a mapping from LookupTypes' symbols to the objects themselves.
 */
static map<char, const LookupType*> _build_lookup_type_map()
{
    map<char, const LookupType*> lookup_map;
    for (const auto &lookup : lookup_types)
        lookup_map[lookup.symbol] = &lookup;
    return lookup_map;
}
static const map<char, const LookupType*> _lookup_types_by_symbol
    = _build_lookup_type_map();

/**
 * Prompt the player for a search string for the given lookup type.
 *
 * @param lookup_type  The LookupType in question (e.g. monsters, items...)
 * @param err[out]     Will be set to a non-empty string if the user failed to
 *                     provide a string.
 * @return             A search string, if one was provided; else "".
 */
static string _prompt_for_regex(const LookupType &lookup_type, string &err)
{
    const string type = lowercase_string(lookup_type.type);
    const string extra = lookup_type.flags & LTYPF_SINGLE_LETTER ?
        make_stringf(" Enter a single letter to list %s displayed by that"
                     " symbol.", pluralise(type).c_str()) :
        "";
    mprf(MSGCH_PROMPT,
         "Describe a %s; partial names and regexps are fine.%s",
         type.c_str(), extra.c_str());

    mprf(MSGCH_PROMPT, "Describe what? ");
    char buf[80];
    if (cancellable_get_line(buf, sizeof(buf)) || buf[0] == '\0')
    {
        err = "Okay, then.";
        return "";
    }

    const string regex = strlen(buf) == 1 ? buf : trimmed_string(buf);
    return regex;
}

static bool _exact_lookup_match(const LookupType &lookup_type,
                                const string &regex)
{
    if (lookup_type.flags & LTYPF_DISABLE_REGEX)
        return false; // no search, no exact match

    if (lookup_type.flags & LTYPF_SINGLE_LETTER && regex.size() == 1)
        return false; // glyph search doesn't have the concept

    if ((*lookup_type.filter_forbid)(regex, ""))
        return false; // match found, but incredibly illegal to display

    return !getLongDescription(regex + lookup_type.suffix()).empty();
}

/**
 * Run an iteration of ?/.
 *
 * @param response[out]   A response to input, to print before the next iter.
 * @return                true if the ?/ loop should continue
 *                        false if it should return control to the caller
 */
static bool _find_description(string &response)
{

    const string lookup_type_prompts =
        comma_separated_fn(lookup_types.begin(), lookup_types.end(),
                           mem_fn(&LookupType::prompt_string), " or ");
    mprf(MSGCH_PROMPT, "Describe a %s? ", lookup_type_prompts.c_str());

    int ch;
    {
        cursor_control con(true);
        ch = toupper(getchm());
    }

    const LookupType * const *lookup_type_ptr
        = map_find(_lookup_types_by_symbol, ch);
    if (!lookup_type_ptr)
        return false;

    ASSERT(*lookup_type_ptr);
    const LookupType lookup_type = **lookup_type_ptr;

    // All this will soon pass.
    const string type = lowercase_string(lookup_type.type);
    const string suffix = lookup_type.suffix();
    const db_find_filter filter = lookup_type.filter_forbid;
    const db_keys_recap recap = lookup_type.recap;
    const bool want_regex = !(lookup_type.flags & LTYPF_DISABLE_REGEX);
    const bool want_sort = !(lookup_type.flags & LTYPF_DISABLE_SORT);

    // ...but especially this.
    const bool doing_mons = ch == 'M';
    const bool doing_items = ch == 'I';
    const bool doing_gods = ch == 'G';
    const bool doing_branches = ch == 'B';
    const bool doing_features = ch == 'F';
    const bool doing_spells = ch == 'S';

    const string regex = want_regex ?
                         _prompt_for_regex(lookup_type, response) :
                         "";

    if (!response.empty())
        return true;

    // not actually sure how to trigger this branch...
    if (want_regex && regex.empty())
    {
        response = "Description must contain at least one non-space.";
        return true;
    }

    const bool by_symbol = (lookup_type.flags & LTYPF_SINGLE_LETTER)
                            && regex.size() == 1;
    const bool by_mon_symbol = by_symbol && doing_mons;
    const bool by_item_symbol = by_symbol && doing_items;

    // Try to get an exact match first.
    const bool exact_match = _exact_lookup_match(lookup_type, regex);

    vector<string> key_list;

    if (by_mon_symbol)
        key_list = _get_monster_keys(regex[0]);
    else if (by_item_symbol)
        key_list = item_name_list_for_glyph(regex[0]);
    else if (doing_gods)
        key_list = _get_god_keys();
    else if (doing_branches)
        key_list = _get_branch_keys();
    else
        key_list = _get_desc_keys(regex, filter);

    if (recap != nullptr)
        (*recap)(key_list);

    const string plur_type = pluralise(type);

    if (key_list.empty())
    {
        if (by_symbol)
            response = "No " + plur_type + " with symbol '" + regex + "'.";
        else
            response = "No matching " + plur_type + ".";
        return true;
    }
    else if (key_list.size() > 52)
    {
        if (by_symbol)
        {
            response = "Too many " + plur_type + " with symbol '" + regex +
                        "' to display.";
        }
        else
        {
            response = make_stringf("Too many matching %s (%" PRIuSIZET ") to"
                                    " display.",
                                    plur_type.c_str(), key_list.size());
        }
        return true;
    }
    else if (key_list.size() == 1)
    {
        _do_description(key_list[0], type, suffix);
        return true;
    }

    if (exact_match)
    {
        string footer = "This entry is an exact match for '" + regex
                        + "'. To see non-exact matches, press space.";

        if (_do_description(regex, type, suffix, footer) != ' ')
            return true;
    }

    if (want_sort)
        sort(key_list.begin(), key_list.end());

    // For tiles builds use a tiles menu to display monsters.
    const bool text_only =
#ifdef USE_TILE_LOCAL
    !(doing_mons || doing_features || doing_spells);
#else
    true;
#endif

    DescMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING,
                       doing_mons, text_only);
    desc_menu.set_tag("description");
    list<monster_info> monster_list;
    list<item_def> item_list;
    for (unsigned int i = 0, size = key_list.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);
        string     str    = uppercase_first(key_list[i]);

        if (ends_with(str, suffix)) // perhaps we should assert this?
            str.erase(str.length() - suffix.length());

        MenuEntry *me = nullptr;

        if (doing_mons)
        {
            // Create and store fake monsters, so the menu code will
            // have something valid to refer to.
            monster_type m_type = get_monster_by_name(str);

            // No proper monster, and causes crashes in Tiles.
            if (mons_is_tentacle_segment(m_type))
                continue;

            monster_type base_type = MONS_NO_MONSTER;
            // HACK: Set an arbitrary humanoid monster as base type.
            if (mons_class_is_zombified(m_type))
                base_type = MONS_GOBLIN;
            monster_info fake_mon(m_type, base_type);
            fake_mon.props["fake"] = true;

            // FIXME: This doesn't generate proper draconian monsters.
            monster_list.push_back(fake_mon);

#ifndef USE_TILE_LOCAL
            int colour = mons_class_colour(m_type);
            if (colour == BLACK)
                colour = LIGHTGREY;

            string prefix = "(<";
            prefix += colour_to_str(colour);
            prefix += ">";
            prefix += stringize_glyph(mons_char(m_type));
            prefix += "</";
            prefix += colour_to_str(colour);
            prefix += ">) ";

            str = prefix + str;
#endif

            // NOTE: MonsterMenuEntry::get_tiles() takes care of setting
            // up a fake weapon when displaying a fake dancing weapon's
            // tile.
            me = new MonsterMenuEntry(str, &(monster_list.back()),
                                      letter);
        }
        else if (doing_features)
            me = new FeatureMenuEntry(str, feat_by_desc(str), letter);
        else if (doing_gods)
            me = new GodMenuEntry(str_to_god(key_list[i]));
        else
        {
            me = new MenuEntry(str, MEL_ITEM, 1, letter);

            if (doing_spells)
            {
                spell_type spell = spell_by_name(str);
#ifdef USE_TILE
                if (spell != SPELL_NO_SPELL)
                    me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
                me->colour = is_player_spell(spell) ? WHITE
                : _is_rod_spell(spell)   ? LIGHTGREY
                : DARKGREY; // monster-only
            }

            me->data = &key_list[i];
        }

        desc_menu.add_entry(me);
    }

    desc_menu.sort();

    while (true)
    {
        vector<MenuEntry*> sel = desc_menu.show();
        redraw_screen();
        if (sel.empty())
        {
            if (doing_mons && desc_menu.getkey() == CONTROL('S'))
                desc_menu.toggle_sorting();
            else
                return true;
        }
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() >= 1);

            string key;

            if (doing_mons)
            {
                monster_info* mon = (monster_info*) sel[0]->data;
                key = mons_type_name(mon->type, DESC_PLAIN);
            }
            else if (doing_features)
                key = sel[0]->text;
            else
                key = *((string*) sel[0]->data);

            _do_description(key, type, suffix);
        }
    }
}

/**
 * Run the ?/ loop, repeatedly prompting the player to query for monsters,
 * etc, until they indicate they're done.
 */
void keyhelp_query_descriptions()
{
    string error;
    while (true)
    {
        redraw_screen();

        if (!error.empty())
            mprf(MSGCH_PROMPT, "%s", error.c_str());
        error = "";

        if (!_find_description(error))
            break;

        clear_messages();
    }

    viewwindow();
    mpr("Okay, then.");
}

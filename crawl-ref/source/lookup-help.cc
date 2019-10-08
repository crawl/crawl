/**
 * @file
 * @brief Let the player search for descriptions of monsters, items, etc.
 **/

#include "AppHdr.h"

#include "lookup-help.h"

#include <functional>

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "cloud.h"
#include "database.h"
#include "dbg-util.h"
#include "decks.h"
#include "describe.h"
#include "describe-god.h"
#include "describe-spells.h"
#include "directn.h"
#include "english.h"
#include "enum.h"
#include "env.h"
#include "god-menu.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-name.h"
#include "items.h"
#include "libutil.h" // map_find
#include "macro.h"
#include "makeitem.h" // item_colour
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
#include "tile-flags.h"
#include "tiledef-main.h"
#include "tilepick.h"
#include "tileview.h"
#endif
#include "ui.h"
#include "view.h"
#include "viewchar.h"


typedef vector<string> (*keys_by_glyph)(char32_t showchar);
typedef vector<string> (*simple_key_list)();
typedef void (*db_keys_recap)(vector<string>&);
typedef MenuEntry* (*menu_entry_generator)(char letter, const string &str,
                                           string &key);
typedef function<int (const string &, const string &, string)> key_describer;

/// A set of optional functionality for lookup types.
enum class lookup_type
{
    none            = 0,
    /// append the 'type' to the db lookup (e.g. "<input> spell")
    db_suffix       = 1<<0,
    /// whether the sorting functionality should be turned off
    disable_sort    = 1<<1,
    /// whether the display menu for this has toggleable sorting
    toggleable_sort = 1<<2,
};
DEF_BITFIELD(lookup_type_flags, lookup_type);

/// A description of a lookup that the player can do. (e.g. (M)onster data)
class LookupType
{
public:
    LookupType(char _symbol, string _type, db_keys_recap _recap,
               db_find_filter _filter_forbid, keys_by_glyph _glyph_fetch,
               simple_key_list _simple_key_fetch,
               menu_entry_generator _menu_gen, key_describer _describer,
               lookup_type_flags _flags)
    : symbol(_symbol), type(_type), filter_forbid(_filter_forbid),
      flags(_flags),
      simple_key_fetch(_simple_key_fetch), glyph_fetch(_glyph_fetch),
      recap(_recap), menu_gen(_menu_gen), describer(_describer)
    {
        // XXX: will crash at startup; compile-time would be better
        // also, ugh
        ASSERT(menu_gen != nullptr || type == "monster");
        ASSERT(describer != nullptr);
    }

    string prompt_string() const;
    string suffix() const;
    vector<string> matching_keys(string regex) const;
    void display_keys(vector<string> &key_list) const;

    /**
     * Does this lookup type have special support for single-character input
     * (looking up corresponding glyphs - e.g. 'o' for orc, '(' for ammo...)
     */
    bool supports_glyph_lookup() const { return glyph_fetch != nullptr; }

    /**
     * Does this lookup type return a list of keys without taking a search
     * request (e.g. branches or gods)?
     */
    bool no_search() const { return simple_key_fetch != nullptr; }

    int describe(const string &key, bool exact_match = false) const;

public:
    /// The letter pressed to choose this (e.g. 'M'). case insensitive
    char symbol;
    /// A description of the lookup type (e.g. "monster"). case insensitive
    string type;
    /// a function returning 'true' if the search result corresponding to
    /// the corresponding search should be filtered out of the results
    db_find_filter filter_forbid;
    /// A set of optional functionality; see lookup_type for details
    lookup_type_flags flags;
private:
    MenuEntry* make_menu_entry(char letter, string &key) const;
    string key_to_menu_str(const string &key) const;

    /**
     * Does this lookup type support toggling the sort order of results?
     */
    bool toggleable_sort() const
    {
        return bool(flags & lookup_type::toggleable_sort);
    }

private:
    /// Function that fetches a list of keys, without taking arguments.
    simple_key_list simple_key_fetch;
    /// a function taking a single character & returning a list of keys
    /// corresponding to that glyph
    keys_by_glyph glyph_fetch;
    /// take the list of keys that were automatically found and fix them
    /// up if necessary
    db_keys_recap recap;
    /// take a letter & a key, return a corresponding new menu entry
    menu_entry_generator menu_gen;
    /// A function to handle describing & interacting with a given key.
    key_describer describer;
};




/**
 * What monster enum corresponds to the given Serpent of Hell name?
 *
 * @param soh_name  The name of the monster; e.g. "the Serpent of Hell dis".
 * @return          The corresponding enum; e.g. MONS_SERPENT_OF_HELL_DIS.
 */
static monster_type _soh_type(string &soh_name)
{
    const string flavour = lowercase_string(soh_name.substr(soh_name.find_last_of(' ')+1));

    branch_type branch = NUM_BRANCHES;
    for (int b = BRANCH_FIRST_HELL; b <= BRANCH_LAST_HELL; ++b)
        if (ends_with(flavour, lowercase_string(branches[b].shortname)))
            branch = (branch_type)b;

    switch (branch)
    {
        case BRANCH_COCYTUS:
            return MONS_SERPENT_OF_HELL_COCYTUS;
        case BRANCH_DIS:
            return MONS_SERPENT_OF_HELL_DIS;
        case BRANCH_TARTARUS:
            return MONS_SERPENT_OF_HELL_TARTARUS;
        case BRANCH_GEHENNA:
            return MONS_SERPENT_OF_HELL;
        default:
            die("bad serpent of hell name");
    }
}

static bool _is_soh(string name)
{
    return starts_with(lowercase(name), "the serpent of hell");
}

static string _soh_name(monster_type m_type)
{
    branch_type b = serpent_of_hell_branch(m_type);
    return string("The Serpent of Hell (") + branches[b].longname + ")";
}

static monster_type _mon_by_name(string name)
{
    return _is_soh(name) ? _soh_type(name) : get_monster_by_name(name);
}

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
    DescMenu(int _flags, bool _toggleable_sort) : Menu(_flags, ""), sort_alpha(true),
    toggleable_sort(_toggleable_sort)
    {
        set_highlighter(nullptr);

        if (_toggleable_sort)
            toggle_sorting();

        set_prompt();
    }

    bool sort_alpha;
    bool toggleable_sort;

    void set_prompt()
    {
        string prompt = "Describe which? ";

        if (toggleable_sort)
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
        if (!toggleable_sort)
            return;

        if (sort_alpha)
            ::sort(items.begin(), items.end(), _compare_mon_names);
        else
            ::sort(items.begin(), items.end(), _compare_mon_toughness);

        for (unsigned int i = 0, size = items.size(); i < size; i++)
        {
            const char letter = index_to_letter(i % 52);

            items[i]->hotkeys.clear();
            items[i]->add_hotkey(letter);
        }
    }

    void toggle_sorting()
    {
        if (!toggleable_sort)
            return;

        sort_alpha = !sort_alpha;

        sort();
        set_prompt();
    }
};

static vector<string> _get_desc_keys(string regex, db_find_filter filter)
{
    vector<string> key_matches = getLongDescKeysByRegex(regex, filter);
    vector<string> body_matches = getLongDescBodiesByRegex(regex, filter);

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

static vector<string> _get_monster_keys(char32_t showchar)
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

        if ((char32_t)me->basechar != showchar)
            continue;

        if (mons_species(i) == MONS_SERPENT_OF_HELL)
        {
            mon_keys.push_back(string(me->name) + " "
                               + serpent_of_hell_flavour(i));
            continue;
        }

        if (getLongDescription(me->name).empty())
            continue;

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
#if TAG_MAJOR_VERSION == 34
        // XXX: currently disabled.
        if (which_god != GOD_PAKELLAS)
#endif
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

static vector<string> _get_cloud_keys()
{
    vector<string> names;

    for (int i = CLOUD_NONE + 1; i < NUM_CLOUD_TYPES; i++)
        names.push_back(cloud_type_name((cloud_type) i) + " cloud");

    return names;
}

/**
 * Return a list of all skill names.
 */
static vector<string> _get_skill_keys()
{
    vector<string> names;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        const string name = lowercase_string(skill_name(sk));
#if TAG_MAJOR_VERSION == 34
        if (getLongDescription(name).empty())
            continue; // obsolete skills
#endif

        names.emplace_back(name);
    }
    return names;
}

static bool _monster_filter(string key, string body)
{
    const monster_type mon_num = _mon_by_name(key);
    return mons_class_flag(mon_num, M_CANT_SPAWN)
           || mons_is_tentacle_segment(mon_num);
}

static bool _spell_filter(string key, string body)
{
    if (!strip_suffix(key, "spell"))
        return true;

    spell_type spell = spell_by_name(key);

    if (spell == SPELL_NO_SPELL)
        return true;

    if (get_spell_flags(spell) & spflag::testing)
        return !you.wizard;

    return false;
}

static bool _item_filter(string key, string body)
{
    return item_kind_by_name(key).base_type == OBJ_UNASSIGNED;
}

static bool _feature_filter(string key, string body)
{
    return feat_by_desc(key) == DNGN_UNSEEN;
}

static bool _card_filter(string key, string body)
{
    lowercase(key);

    // Every card description contains the keyword "card".
    if (!strip_suffix(key, "card"))
        return true;

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

    if (!strip_suffix(key, "ability"))
        return true;

    return !string_matches_ability_name(key);
}

static bool _status_filter(string key, string body)
{
    return !strip_suffix(lowercase(key), " status");
}


static void _recap_mon_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        if (!_is_soh(keys[i]))
        {
            monster_type type = get_monster_by_name(keys[i]);
            keys[i] = mons_type_name(type, DESC_PLAIN);
        }
    }
}

/**
 * Fixup spell names. (Correcting capitalization, mainly.)
 *
 * @param[in,out] keys      A lowercased list of spell names.
 */
static void _recap_spell_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        // first, strip " spell"
        const string key_name = keys[i].substr(0, keys[i].length() - 6);
        // then get the real name
        keys[i] = make_stringf("%s spell",
                               spell_title(spell_by_name(key_name)));
    }
}

/**
 * Fixup ability names. (Correcting capitalization, mainly.)
 *
 * @param[in,out] keys      A lowercased list of ability names.
 */
static void _recap_ability_keys(vector<string> &keys)
{
    for (auto &key : keys)
    {
        strip_suffix(key, "ability");
        // get the real name
        key = make_stringf("%s ability", ability_name(ability_by_name(key)));
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

/**
 * Make a basic, no-frills ?/<foo> menu entry.
 *
 * @param letter    The letter for the entry. (E.g. 'e' for the fifth entry.)
 * @param str       A processed string for the entry. (E.g. "Blade".)
 * @param key       The raw database key for the entry. (E.g. "blade card".)
 * @return          A new menu entry.
 */
static MenuEntry* _simple_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = new MenuEntry(str, MEL_ITEM, 1, letter);
    me->data = &key;
    return me;
}

/**
 * Generate a ?/M entry.
 *
 * @param letter      The letter for the entry. (E.g. 'e' for the fifth entry.)
 * @param str         A processed string for the entry. (E.g. "Blade".)
 * @param mslot[out]  A space in memory to store a fake monster.
 * @return            A new menu entry.
 */
static MenuEntry* _monster_menu_gen(char letter, const string &str,
                                    monster_info &mslot)
{
    // Create and store fake monsters, so the menu code will
    // have something valid to refer to.
    monster_type m_type = _mon_by_name(str);
    const string name = _is_soh(str) ? _soh_name(m_type) : str;

    monster_type base_type = MONS_NO_MONSTER;
    // HACK: Set an arbitrary humanoid monster as base type.
    if (mons_class_is_zombified(m_type))
        base_type = MONS_GOBLIN;

    monster_info fake_mon(m_type, base_type);
    fake_mon.props["fake"] = true;

    mslot = fake_mon;

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

    const string title = prefix + name;
#else
    const string &title = name;
#endif

    // NOTE: MonsterMenuEntry::get_tiles() takes care of setting
    // up a fake weapon when displaying a fake dancing weapon's
    // tile.
    return new MonsterMenuEntry(title, &mslot, letter);
}

/**
 * Generate a ?/I menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _item_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);
#ifdef USE_TILE
    item_def item;
    item_kind kind = item_kind_by_name(key);
    get_item_by_name(&item, key.c_str(), kind.base_type);
    item_colour(item);
    tileidx_t idx = tileidx_item(get_item_info(item));
    tileidx_t base_item = tileidx_known_base_item(idx);
    if (base_item)
        me->add_tile(tile_def(base_item, TEX_DEFAULT));
    me->add_tile(tile_def(idx, TEX_DEFAULT));
#endif
    return me;
}

/**
 * Generate a ?/F menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _feature_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = new MenuEntry(str, MEL_ITEM, 1, letter);
    me->data = &key;

#ifdef USE_TILE
    const dungeon_feature_type feat = feat_by_desc(str);
    if (feat)
    {
        const tileidx_t idx = tileidx_feature_base(feat);
        me->add_tile(tile_def(idx, get_dngn_tex(idx)));
    }
#endif

    return me;
}

/**
 * Generate a ?/G menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _god_menu_gen(char letter, const string &str, string &key)
{
    return new GodMenuEntry(str_to_god(key));
}

/**
 * Generate a ?/A menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _ability_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

#ifdef USE_TILE
    const ability_type ability = ability_by_name(str);
    if (ability != ABIL_NON_ABILITY)
        me->add_tile(tile_def(tileidx_ability(ability), TEX_GUI));
#endif

    return me;
}

/**
 * Generate a ?/C menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _card_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);
#ifdef USE_TILE
    me->add_tile(tile_def(TILEG_NEMELEX_CARD, TEX_GUI));
#endif
    return me;
}

/**
 * Generate a ?/S menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _spell_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

    const spell_type spell = spell_by_name(str);
#ifdef USE_TILE
    if (spell != SPELL_NO_SPELL)
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
    me->colour = is_player_spell(spell) ? WHITE
                                        : DARKGREY; // monster-only

    return me;
}

/**
 * Generate a ?/K menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _skill_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

#ifdef USE_TILE
    const skill_type skill = str_to_skill_safe(str);
    me->add_tile(tile_def(tileidx_skill(skill, TRAINING_ENABLED), TEX_GUI));
#endif

    return me;
}

/**
 * Generate a ?/B menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _branch_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

    const branch_type branch = branch_by_shortname(str);
    int hotkey = branches[branch].travel_shortcut;
    me->hotkeys = {hotkey, tolower_safe(hotkey)};
#ifdef USE_TILE
    me->add_tile(tile_def(tileidx_branch(branch), TEX_FEAT));
#endif

    return me;
}

/**
 * Generate a ?/L menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _cloud_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

    const string cloud_name = lowercase_string(str);
    const cloud_type cloud = cloud_name_to_type(cloud_name);
    ASSERT(cloud != NUM_CLOUD_TYPES);

    cloud_struct fake_cloud;
    fake_cloud.type = cloud;
    fake_cloud.decay = 1000;
    me->colour = element_colour(get_cloud_colour(fake_cloud));

#ifdef USE_TILE
    cloud_info fake_cloud_info;
    fake_cloud_info.type = cloud;
    fake_cloud_info.colour = me->colour;
    const tileidx_t idx = tileidx_cloud(fake_cloud_info) & ~TILE_FLAG_FLYING;
    me->add_tile(tile_def(idx, TEX_DEFAULT));
#endif

    return me;
}


/**
 * How should this type be expressed in the prompt string?
 *
 * @return The 'type', with the first instance of the 'symbol' found &
 *          replaced with an uppercase version surrounded by parens
 *          e.g. "monster", 'm' -> "(M)onster"
 */
string LookupType::prompt_string() const
{
    string prompt_str = lowercase_string(type);
    const size_t symbol_pos = prompt_str.find(tolower_safe(symbol));
    ASSERT(symbol_pos != string::npos);

    prompt_str.replace(symbol_pos, 1, make_stringf("(%c)", toupper_safe(symbol)));
    return prompt_str;
}

/**
 * A suffix to be appended to the provided search string when looking for
 * db info.
 *
 * @return      An appropriate suffix for types that need them (e.g.
 *              " cards"); otherwise "".
 */
string LookupType::suffix() const
{
    if (flags & lookup_type::db_suffix)
        return " " + type;
    return "";
}

/**
 * Get a list of string corresponding to the given regex.
 */
vector<string> LookupType::matching_keys(string regex) const
{
    vector<string> key_list;

    if (no_search())
        key_list = simple_key_fetch();
    else if (regex.size() == 1 && supports_glyph_lookup())
        key_list = glyph_fetch(regex[0]);
    else
        key_list = _get_desc_keys(regex, filter_forbid);

    if (recap != nullptr)
        (*recap)(key_list);

    return key_list;
}

static string _mons_desc_key(monster_type type)
{
    const string name = mons_type_name(type, DESC_PLAIN);
    if (mons_species(type) == MONS_SERPENT_OF_HELL)
        return name + " " + serpent_of_hell_flavour(type);
    return name;
}

/**
 * Build a menu listing the given keys, and allow the player to interact
 * with them.
 */
void LookupType::display_keys(vector<string> &key_list) const
{
    DescMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING
            | MF_NO_SELECT_QTY | MF_USE_TWO_COLUMNS , toggleable_sort());
    desc_menu.set_tag("description");

    // XXX: ugh
    const bool doing_mons = type == "monster";
    vector<monster_info> monster_list(key_list.size());
    for (unsigned int i = 0, size = key_list.size(); i < size; i++)
    {
        const char letter = index_to_letter(i % 52);
        string &key = key_list[i];
        // XXX: double ugh
        if (doing_mons)
        {
            desc_menu.add_entry(_monster_menu_gen(letter,
                                                  key_to_menu_str(key),
                                                  monster_list[i]));
        } else
            desc_menu.add_entry(make_menu_entry(letter, key));
    }

    desc_menu.sort();

    desc_menu.on_single_selection = [this, doing_mons](const MenuEntry& item)
    {
        ASSERT(item.hotkeys.size() >= 1);

        string key;

        if (doing_mons)
        {
            monster_info* mon = (monster_info*) item.data;
            key = _mons_desc_key(mon->type);
        }
        else
            key = *((string*) item.data);

        describe(key);
        return true;
    };

    while (true)
    {
        desc_menu.show();
        if (toggleable_sort() && desc_menu.getkey() == CONTROL('S'))
            desc_menu.toggle_sorting();
        else
            break;
    }
}

/**
 * Generate a description menu entry for the given key.
 *
 * @param letter    The letter with which the entry should be labeled.
 * @param key       The key for the entry.
 * @return          A pointer to a new MenuEntry object.
 */
MenuEntry* LookupType::make_menu_entry(char letter, string &key) const
{
    ASSERT(menu_gen);
    return menu_gen(letter, key_to_menu_str(key), key);
}

/**
 * Turn a DB string into a nice menu title.
 *
 * @param key       The key in question. (E.g. "blade card").
 * @return          A nicer string. (E.g. "Blade").
 */
string LookupType::key_to_menu_str(const string &key) const
{
    string str = uppercase_first(key);
    // perhaps we should assert this?
    strip_suffix(str, suffix());
    return str;
}

/**
 * Handle describing & interacting with a given key.
 * @return the last key pressed.
 */
int LookupType::describe(const string &key, bool exact_match) const
{
    const string footer
        = exact_match ? "This entry is an exact match for '" + key
        + "'. To see non-exact matches, press space."
        : "";
    return describer(key, suffix(), footer);
}

/**
 * Describe the thing with the given name.
 *
 * @param key           The name of the thing in question.
 * @param suffix        A suffix to trim from the key when making the title.
 * @param footer        A footer to append to the end of descriptions.
 * @param extra_info    Extra info to append to the database description.
 * @return              The keypress the user made to exit.
 */
static int _describe_key(const string &key, const string &suffix,
                         string footer, const string &extra_info,
                         const tile_def *tile = nullptr)
{
    describe_info inf;
    inf.quote = getQuoteString(key);

    const string desc = getLongDescription(key);
    const int width = min(80, get_number_of_cols());

    inf.body << desc << extra_info;

    string title = key;
    strip_suffix(title, suffix);
    title = uppercase_first(title);
    linebreak_string(footer, width - 1);

    inf.footer = footer;
    inf.title  = title;

    return show_description(inf, tile);
}

/**
 * Describe the thing with the given name.
 *
 * @param key       The name of the thing in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_generic(const string &key, const string &suffix,
                             string footer)
{
    return _describe_key(key, suffix, footer, "");
}

/**
 * Describe & allow examination of the monster with the given name.
 *
 * @param key       The name of the monster in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_monster(const string &key, const string &suffix,
                             string footer)
{
    const monster_type mon_num = _mon_by_name(key);
    ASSERT(mon_num != MONS_PROGRAM_BUG);
    // Don't attempt to get more information on ghost demon
    // monsters, as the ghost struct has not been initialised, which
    // will cause a crash. Similarly for zombified monsters, since
    // they require a base monster.
    if (mons_is_ghost_demon(mon_num) || mons_class_is_zombified(mon_num))
        return _describe_generic(key, suffix, footer);

    monster_type base_type = MONS_NO_MONSTER;
    // Might be better to show all possible combinations rather than picking
    // one at random as this does?
    if (mons_is_draconian_job(mon_num))
        base_type = random_draconian_monster_species();
    else if (mons_is_demonspawn_job(mon_num))
        base_type = random_demonspawn_monster_species();
    monster_info mi(mon_num, base_type);
    // Avoid slime creature being described as "buggy"
    if (mi.type == MONS_SLIME_CREATURE)
        mi.slime_size = 1;
    return describe_monsters(mi, true, footer);
}


/**
 * Describe the spell with the given name.
 *
 * @param key       The name of the spell in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_spell(const string &key, const string &suffix,
                             string footer)
{
    const string spell_name = key.substr(0, key.size() - suffix.size());
    const spell_type spell = spell_by_name(spell_name, true);
    ASSERT(spell != SPELL_NO_SPELL);
    describe_spell(spell, nullptr, nullptr, true);
    return 0;
}

static int _describe_skill(const string &key, const string &suffix,
                             string footer)
{
    const string skill_name = key.substr(0, key.size() - suffix.size());
    const skill_type skill = skill_from_name(skill_name.c_str());
    describe_skill(skill);
    return 0;
}

static int _describe_ability(const string &key, const string &suffix,
                             string footer)
{
    const string abil_name = key.substr(0, key.size() - suffix.size());
    const ability_type abil = ability_by_name(abil_name.c_str());
    describe_ability(abil);
    return 0;
}

/**
 * Describe the card with the given name.
 *
 * @param key       The name of the card in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_card(const string &key, const string &suffix,
                           string footer)
{
    const string card_name = key.substr(0, key.size() - suffix.size());
    const card_type card = name_to_card(card_name);
    ASSERT(card != NUM_CARDS);
#ifdef USE_TILE
    tile_def tile = tile_def(TILEG_NEMELEX_CARD, TEX_GUI);
    return _describe_key(key, suffix, footer, which_decks(card) + "\n", &tile);
#else
    return _describe_key(key, suffix, footer, which_decks(card) + "\n");
#endif
}

/**
 * Describe the cloud with the given name.
 *
 * @param key       The name of the cloud in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_cloud(const string &key, const string &suffix,
                           string footer)
{
    const string cloud_name = key.substr(0, key.size() - suffix.size());
    const cloud_type cloud = cloud_name_to_type(cloud_name);
    ASSERT(cloud != NUM_CLOUD_TYPES);
#ifdef USE_TILE
    cloud_info fake_cloud_info;
    fake_cloud_info.type = cloud;
    const tileidx_t idx = tileidx_cloud(fake_cloud_info) & ~TILE_FLAG_FLYING;
    tile_def tile = tile_def(idx, TEX_DEFAULT);
    return _describe_key(key, suffix, footer, extra_cloud_info(cloud), &tile);
#else
    return _describe_key(key, suffix, footer, extra_cloud_info(cloud));
#endif
}

/**
 * Describe the item with the given name.
 *
 * @param key       The name of the item in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_item(const string &key, const string &suffix,
                           string footer)
{
    item_def item;
    if (!get_item_by_exact_name(item, key.c_str()))
        die("Unable to get item %s by name", key.c_str());
    describe_item(item);
    return 0;
}

static int _describe_feature(const string &key, const string &suffix,
                             string footer)
{
    const string feat_name = key.substr(0, key.size() - suffix.size());
    const dungeon_feature_type feat = feat_by_desc(feat_name);
    describe_feature_type(feat);
    return 0;
}

/**
 * Describe the god with the given name.
 *
 * @param key       The name of the god in question.
 * @return          0.
 */
static int _describe_god(const string &key, const string &/*suffix*/,
                          string /*footer*/)
{
    const god_type which_god = str_to_god(key);
    ASSERT(which_god != GOD_NO_GOD);
    describe_god(which_god);

    return 0; // no exact matches for gods, so output doesn't matter
}

static string _branch_entry_runes(branch_type br)
{
    string desc;
    const int num_runes = runes_for_branch(br);

    if (num_runes > 0)
    {
        desc = make_stringf("\n\nThis %s can only be entered while carrying "
                            "at least %d rune%s of Zot.",
                            br == BRANCH_ZIGGURAT ? "portal" : "branch",
                            num_runes, num_runes > 1 ? "s" : "");
    }

    return desc;
}

static string _branch_depth(branch_type br)
{
    string desc;
    const int depth = branches[br].numlevels;

    // Abyss depth is explained in the description.
    if (depth > 1 && br != BRANCH_ABYSS)
    {
        desc = make_stringf("\n\nThis %s is %d levels deep.",
                            br == BRANCH_ZIGGURAT ? "portal"
                                                  : "branch",
                            depth);
    }

    return desc;
}

static string _branch_location(branch_type br)
{
    string desc;
    const branch_type parent = branches[br].parent_branch;
    const int min = branches[br].mindepth;
    const int max = branches[br].maxdepth;

    // Ziggurat locations are explained in the description.
    if (parent != NUM_BRANCHES && br != BRANCH_ZIGGURAT)
    {
        desc = "\n\nThe entrance to this branch can be found ";
        if (min == max)
        {
            if (branches[parent].numlevels == 1)
                desc += "in ";
            else
                desc += make_stringf("on level %d of ", min);
        }
        else
            desc += make_stringf("between levels %d and %d of ", min, max);
        desc += branches[parent].longname;
        desc += ".";
    }

    return desc;
}

static string _branch_subbranches(branch_type br)
{
    string desc;
    vector<string> subbranch_names;

    for (branch_iterator it; it; ++it)
        if (it->parent_branch == br && !branch_is_unfinished(it->id))
            subbranch_names.push_back(it->longname);

    // Lair's random branches are explained in the description.
    if (!subbranch_names.empty() && br != BRANCH_LAIR)
    {
        desc += make_stringf("\n\nThis branch contains the entrance%s to %s.",
                             subbranch_names.size() > 1 ? "s" : "",
                             comma_separated_line(begin(subbranch_names),
                                                  end(subbranch_names)).c_str());
    }

    return desc;
}

/**
 * Describe the branch with the given name.
 *
 * @param key       The name of the branch in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_branch(const string &key, const string &suffix,
                            string footer)
{
    const string branch_name = key.substr(0, key.size() - suffix.size());
    const branch_type branch = branch_by_shortname(branch_name);
    ASSERT(branch != NUM_BRANCHES);

    string info = "";
    const string noise_desc = branch_noise_desc(branch);
    if (!noise_desc.empty())
        info += "\n\n" + noise_desc;

    info += _branch_location(branch)
            + _branch_entry_runes(branch)
            + _branch_depth(branch)
            + _branch_subbranches(branch)
            + "\n\n"
            + branch_rune_desc(branch, false);

#ifdef USE_TILE
    tile_def tile = tile_def(tileidx_branch(branch), TEX_FEAT);
    return _describe_key(key, suffix, footer, info, &tile);
#else
    return _describe_key(key, suffix, footer, info);
#endif
}

/// All types of ?/ queries the player can enter.
static const vector<LookupType> lookup_types = {
    LookupType('M', "monster", _recap_mon_keys, _monster_filter,
               _get_monster_keys, nullptr, nullptr,
               _describe_monster, lookup_type::toggleable_sort),
    LookupType('S', "spell", _recap_spell_keys, _spell_filter,
               nullptr, nullptr, _spell_menu_gen,
               _describe_spell, lookup_type::db_suffix),
    LookupType('K', "skill", nullptr, nullptr,
               nullptr, _get_skill_keys, _skill_menu_gen,
               _describe_skill, lookup_type::none),
    LookupType('A', "ability", _recap_ability_keys, _ability_filter,
               nullptr, nullptr, _ability_menu_gen,
               _describe_ability, lookup_type::db_suffix),
    LookupType('C', "card", _recap_card_keys, _card_filter,
               nullptr, nullptr, _card_menu_gen,
               _describe_card, lookup_type::db_suffix),
    LookupType('I', "item", nullptr, _item_filter,
               item_name_list_for_glyph, nullptr, _item_menu_gen,
               _describe_item, lookup_type::none),
    LookupType('F', "feature", _recap_feat_keys, _feature_filter,
               nullptr, nullptr, _feature_menu_gen,
               _describe_feature, lookup_type::none),
    LookupType('G', "god", nullptr, nullptr,
               nullptr, _get_god_keys, _god_menu_gen,
               _describe_god, lookup_type::none),
    LookupType('B', "branch", nullptr, nullptr,
               nullptr, _get_branch_keys, _branch_menu_gen,
               _describe_branch, lookup_type::disable_sort),
    LookupType('L', "cloud", nullptr, nullptr,
               nullptr, _get_cloud_keys, _cloud_menu_gen,
               _describe_cloud, lookup_type::db_suffix),
    LookupType('T', "status", nullptr, _status_filter,
               nullptr, nullptr, _simple_menu_gen,
               _describe_generic, lookup_type::db_suffix),
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
    const string extra = lookup_type.supports_glyph_lookup() ?
        make_stringf(" Enter a single letter to list %s displayed by that"
                     " symbol.", pluralise(type).c_str()) :
        "";
    const string prompt = make_stringf(
         "Describe a %s; partial names and regexps are fine.%s\n"
         "Describe what? ",
         type.c_str(), extra.c_str());

    char buf[80];
    if (msgwin_get_line(prompt, buf, sizeof(buf)) || buf[0] == '\0')
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
    if (lookup_type.no_search())
        return false; // no search, no exact match

    if (lookup_type.supports_glyph_lookup() && regex.size() == 1)
        return false; // glyph search doesn't have the concept

    if (lookup_type.filter_forbid && (*lookup_type.filter_forbid)(regex, ""))
        return false; // match found, but incredibly illegal to display

    return !getLongDescription(regex + lookup_type.suffix()).empty();
}

/**
 * Check if the provided keylist is invalid; if so, return the reason why.
 *
 * @param key_list      The list of keys to be checked before display.
 * @param type          The singular name of the things being listed.
 * @param regex         The search term that was used to fetch this list.
 * @param by_symbol     Whether the search is by regex or by glyph.
 * @return              A reason why the list is invalid, if it is
 *                      (e.g. "No monsters with symbol '👻'."),
 *                      or the empty string if the list is valid.
 */
static string _keylist_invalid_reason(const vector<string> &key_list,
                                      const string &type,
                                      const string &regex,
                                      bool by_symbol)
{
    const string plur_type = pluralise(type);

    if (key_list.empty())
    {
        if (by_symbol)
            return "No " + plur_type + " with symbol '" + regex + "'.";
        return "No matching " + plur_type + ".";
    }

    // we're good!
    return "";
}

static int _lookup_prompt()
{
    // TODO: show this + the regex prompt in the same menu?
#ifdef TOUCH_UI
    bool use_popup = true;
#else
    bool use_popup = !crawl_state.need_save || ui::has_layout();
#endif

    int ch = -1;
    const string lookup_type_prompts =
        comma_separated_fn(lookup_types.begin(), lookup_types.end(),
                           mem_fn(&LookupType::prompt_string), " or ");
    if (use_popup)
    {
        string prompt = make_stringf("Describe a %s? ",
                                                lookup_type_prompts.c_str());
        linebreak_string(prompt, 72);

        auto prompt_ui =
                make_shared<ui::Text>(formatted_string::parse_string(prompt));
        bool done = false;

        prompt_ui->on(ui::Widget::slots.event, [&](wm_event ev) {
            if (ev.type == WME_KEYDOWN)
            {
                ch = ev.key.keysym.sym;
                done = true;
            }
            return done;
        });

        mouse_control mc(MOUSE_MODE_MORE);
        auto popup = make_shared<ui::Popup>(prompt_ui);
        ui::run_layout(move(popup), done);
    }
    else
    {
        mprf(MSGCH_PROMPT, "Describe a %s? ", lookup_type_prompts.c_str());

        {
            cursor_control con(true);
            ch = getchm();
        }
    }
    return toupper_safe(ch);
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
    int ch = _lookup_prompt();
    const LookupType * const *lookup_type_ptr
        = map_find(_lookup_types_by_symbol, ch);
    if (!lookup_type_ptr)
        return false;

    ASSERT(*lookup_type_ptr);
    const LookupType ltype = **lookup_type_ptr;

    const bool want_regex = !(ltype.no_search());
    const string regex = want_regex ?
                         _prompt_for_regex(ltype, response) :
                         "";

    if (!response.empty())
        return true;

    // not actually sure how to trigger this branch...
    if (want_regex && regex.empty())
    {
        response = "Description must contain at least one non-space.";
        return true;
    }


    // Try to get an exact match first.
    const bool exact_match = _exact_lookup_match(ltype, regex);

    vector<string> key_list = ltype.matching_keys(regex);

    const bool by_symbol = ltype.supports_glyph_lookup()
                           && regex.size() == 1;
    const string type = lowercase_string(ltype.type);
    response = _keylist_invalid_reason(key_list, type, regex, by_symbol);
    if (!response.empty())
        return true;

    if (key_list.size() == 1)
    {
        ltype.describe(key_list[0]);
        return true;
    }

    if (exact_match && ltype.describe(regex, true) != ' ')
        return true;

    if (!(ltype.flags & lookup_type::disable_sort))
        sort(key_list.begin(), key_list.end());

    ltype.display_keys(key_list);
    return true;
}

/**
 * Run the ?/ loop, repeatedly prompting the player to query for monsters,
 * etc, until they indicate they're done.
 */
void keyhelp_query_descriptions()
{
    string response;
    while (true)
    {
        redraw_screen();

        if (!response.empty())
            mprf(MSGCH_PROMPT, "%s", response.c_str());
        response = "";

        if (!_find_description(response))
            break;

        clear_messages();
    }

    viewwindow();
    mpr("Okay, then.");
}

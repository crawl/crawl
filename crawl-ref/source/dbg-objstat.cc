/**
 * @file
 * @brief Objstat: monster and item generation statistics
**/

#include "AppHdr.h"

#include "dbg-objstat.h"

#include <cerrno>
#include <cmath>
#include <sstream>

#include "artefact.h"
#include "branch.h"
#include "corpse.h"
#include "dbg-maps.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "feature.h"
#include "initfile.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-prop-enum.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-util.h"
#include "ng-init.h"
#include "shopping.h"
#include "spl-book.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "version.h"

#ifdef DEBUG_STATISTICS

#define STAT_PRECISION 2

const static char *stat_out_prefix = "objstat_";
const static char *stat_out_ext = ".txt";
static FILE *stat_outf;

enum item_base_type
{
    ITEM_GOLD,
    ITEM_SCROLLS,
    ITEM_POTIONS,
    ITEM_WANDS,
    ITEM_WEAPONS,
    ITEM_MISSILES,
    ITEM_STAVES,
    ITEM_ARMOUR,
    ITEM_JEWELLERY,
    ITEM_MISCELLANY,
    ITEM_SPELLBOOKS,
    ITEM_MANUALS,
    NUM_ITEM_BASE_TYPES,
    ITEM_IGNORE = 100,
};

// Holds item data needed for item statistics and maps values from item_def
// where changes are needed.
class objstat_item
{
public:
    objstat_item(item_base_type ct, int st) : base_type(ct), sub_type(st)
    {
    }

    objstat_item(const item_def &item);

    item_base_type base_type;
    int sub_type;
    int quantity;
    int plus;
    int brand;
    vector<spell_type> spells;

    bool in_vault;
    bool is_arte;
    bool in_shop;
    bool held_mons;
};

static level_id all_lev(NUM_BRANCHES, -1);
static set<level_id> stat_levels;
static int num_branches = 0;
static int num_levels = 0;

// Ex: item_recs[level][item.base_type][item.sub_type][field]
static map<level_id, map<item_base_type, map<int, map<string, int> > > >
       item_recs;
static map<item_base_type, vector<string> > item_fields = {
    { ITEM_GOLD,
        { "Num", "NumVault", "NumMons", "NumMin", "NumMax", "NumSD" }
    },
    { ITEM_SCROLLS,
        { "Num", "NumVault", "NumShop", "NumMons", "NumMin", "NumMax", "NumSD" }
    },
    { ITEM_POTIONS,
        { "Num", "NumVault", "NumShop", "NumMons", "NumMin", "NumMax", "NumSD" }
    },
    { ITEM_WANDS,
        { "Num", "NumVault", "NumShop", "NumMons", "NumMin", "NumMax", "NumSD",
            "Chrg", "ChrgVault", "ChrgShop", "ChrgMons" },
    },
    { ITEM_WEAPONS,
        { "Num", "NumBrand", "NumArte", "NumVault", "NumShop", "NumMons",
            "NumMin", "NumMax", "NumSD", "Ench", "EnchBrand", "EnchArte",
            "EnchVault", "EnchShop", "EnchMons" },
    },
    { ITEM_MISSILES,
        { "Num", "NumBrand", "NumVault", "NumShop", "NumMons", "Num",
            "NumMin", "NumMax", "NumSD" },
    },
    { ITEM_STAVES,
        { "Num", "NumVault", "NumShop", "NumMons", "Num", "NumMin", "NumMax",
            "NumSD" },
    },
    { ITEM_ARMOUR,
        { "Num", "NumBrand", "NumArte", "NumVault", "NumShop", "NumMons",
            "NumMin", "NumMax", "NumSD", "Ench", "EnchBrand", "EnchArte",
            "EnchVault", "EnchShop", "EnchMons" }
    },
    { ITEM_JEWELLERY,
        { "Num", "NumArte", "NumVault", "NumShop", "NumMons", "NumMin",
            "NumMax", "NumSD" },
    },
    { ITEM_MISCELLANY,
        { "Num", "NumVault", "NumShop", "NumMin", "NumMax", "NumSD" },
    },
    { ITEM_SPELLBOOKS,
        { "Num", "NumVault", "NumShop", "NumMin", "NumMax", "NumSD" },
    },
    { ITEM_MANUALS,
        { "Num", "NumVault", "NumShop", "NumMin", "NumMax", "NumSD" },
    },
};

enum stat_category_type
{
    CATEGORY_ALL,      // regardless of below categories
    CATEGORY_ARTEFACT, // is artefact
    CATEGORY_VAULT,    // in a vault
    CATEGORY_SHOP,     // in a shop
    CATEGORY_MONSTER,  // held by a monster
    NUM_STAT_CATEGORIES
};

// Ex: brand_recs[level][item.base_type][item.sub_type][CATEGORY_ALL][brand];
static map<level_id, map<item_base_type, map<int, map<stat_category_type,
            map<int, int> > > > > brand_recs;
static map<item_base_type, vector<string> > brand_fields = {
    {ITEM_WEAPONS,
        { "Brands", "BrandsArte", "BrandsVault", "BrandsShop", "BrandsMons" } },
    {ITEM_ARMOUR,
        {"Brands", "BrandsArte", "BrandsVault", "BrandsShop", "BrandsMons" } },
    {ITEM_MISSILES, { "Brands", "BrandsVault", "BrandsShop", "BrandsMons" } },
};

static set<monster_type> objstat_monsters;
// Ex: monster_recs[level][mc]["Num"]
static map<level_id, map<monster_type, map <string, int> > > monster_recs;
static const vector<string> monster_fields = {
    "Num", "NumVault", "NumMin", "NumMax", "NumSD", "MonsHD", "MonsHP",
    "MonsXP", "TotalXP", "TotalXPVault"
};

static set<dungeon_feature_type> objstat_features;
typedef map<dungeon_feature_type, map <string, int> > feature_stats;
static map<level_id, feature_stats> feature_recs;
static const vector<string> feature_fields = {
    "Num", "NumVault", "NumMin", "NumMax", "NumSD",
};

static set<spell_type> objstat_spells;
// Ex: spell_recs[level][spell]["Num"]
static map<level_id, map<spell_type, map<string, int> > > spell_recs;
static const vector<string> spell_fields = {
    "Num", "NumVault", "NumShop", "NumArte", "NumMin", "NumMax", "NumSD",
    "Chance", "ChanceVault", "ChanceShop", "ChanceArte"
};

static item_base_type _item_base_type(const item_def &item)
{
    item_base_type type;
    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
        type = ITEM_MISCELLANY;
        break;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_MANUAL)
            type = ITEM_MANUALS;
        else
            type = ITEM_SPELLBOOKS;
        break;
    case OBJ_GOLD:
        type = ITEM_GOLD;
        break;
    case OBJ_SCROLLS:
        type = ITEM_SCROLLS;
        break;
    case OBJ_POTIONS:
        type = ITEM_POTIONS;
        break;
    case OBJ_WANDS:
        type = ITEM_WANDS;
        break;
    case OBJ_WEAPONS:
        type = ITEM_WEAPONS;
        break;
    case OBJ_MISSILES:
        type = ITEM_MISSILES;
        break;
    case OBJ_STAVES:
        type = ITEM_STAVES;
        break;
    case OBJ_ARMOUR:
        type = ITEM_ARMOUR;
        break;
    case OBJ_JEWELLERY:
        type = ITEM_JEWELLERY;
        break;
    default:
        type = ITEM_IGNORE;
        break;
    }
    return type;
}

static object_class_type _item_orig_base_type(item_base_type base_type)
{
    object_class_type type;
    switch (base_type)
    {
    case ITEM_GOLD:
        type = OBJ_GOLD;
        break;
    case ITEM_SCROLLS:
        type = OBJ_SCROLLS;
        break;
    case ITEM_POTIONS:
        type = OBJ_POTIONS;
        break;
    case ITEM_WANDS:
        type = OBJ_WANDS;
        break;
    case ITEM_WEAPONS:
        type = OBJ_WEAPONS;
        break;
    case ITEM_MISSILES:
        type = OBJ_MISSILES;
        break;
    case ITEM_STAVES:
        type = OBJ_STAVES;
        break;
    case ITEM_ARMOUR:
        type = OBJ_ARMOUR;
        break;
    case ITEM_JEWELLERY:
        type = OBJ_JEWELLERY;
        break;
    case ITEM_MISCELLANY:
        type = OBJ_MISCELLANY;
        break;
    case ITEM_MANUALS:
    case ITEM_SPELLBOOKS:
        type = OBJ_BOOKS;
        break;
    default:
        type = OBJ_UNASSIGNED;
        break;
    }
    return type;
}

static string _item_class_name(item_base_type base_type)
{
    string name;
    switch (base_type)
    {
    case ITEM_WEAPONS:
        name = "Weapons";
        break;
    case ITEM_SPELLBOOKS:
        name = "Spellbooks";
        break;
    case ITEM_MANUALS:
        name = "Manuals";
        break;
    default:
        name = item_class_name(_item_orig_base_type(base_type));
    }
    return name;
}

static int _item_orig_sub_type(item_base_type base_type, int sub_type)
{
    int type;
    switch (base_type)
    {
    case ITEM_MANUALS:
        type = BOOK_MANUAL;
        break;
    default:
        type = sub_type;
        break;
    }
    return type;
}

static int _item_max_sub_type(item_base_type base_type)
{
    int num = 0;
    switch (base_type)
    {
    case ITEM_MANUALS:
        num = NUM_SKILLS;
        break;
    default:
        num = get_max_subtype(_item_orig_base_type(base_type));
        break;
    }
    return num;
}

static bool _item_tracks_artefact(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
    case ITEM_SPELLBOOKS:
        return true;
    default:
        return false;
    }
}

static bool _item_tracks_plus(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
    case ITEM_WANDS:
    case ITEM_MISCELLANY:
        return true;
    default:
        return false;
    }
}

static bool _item_tracks_brand(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_MISSILES:
        return true;
    default:
        return false;
    }
}

static bool _item_tracks_monster(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_MISCELLANY:
    case ITEM_SPELLBOOKS:
    case ITEM_MANUALS:
        return false;
    default:
        return true;
    }
}

static bool _item_tracks_shop(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_GOLD:
        return false;
    default:
        return true;
    }
}

static item_def _dummy_item(item_base_type base_type, int sub_type,
        int brand = 0)
{
    item_def dummy_item;

    dummy_item.base_type = _item_orig_base_type(base_type);
    if (sub_type == _item_max_sub_type(base_type))
        dummy_item.sub_type = 0;
    else
        dummy_item.sub_type = _item_orig_sub_type(base_type, sub_type);

    dummy_item.brand = brand;

    if (base_type == ITEM_MANUALS)
        dummy_item.skill = static_cast<skill_type>(sub_type);

    dummy_item.quantity = 1;

    return dummy_item;
}

static string _brand_name(item_base_type base_type, int sub_type, int brand)
{
    if (!brand)
        return "none";

    string brand_name = "";
    const item_def dummy_item = _dummy_item(base_type, sub_type, brand);
    switch (base_type)
    {
    case ITEM_WEAPONS:
        brand_name = weapon_brand_name(dummy_item, true);
        break;
    case ITEM_ARMOUR:
        brand_name = armour_ego_name(dummy_item, true);
        break;
    case ITEM_MISSILES:
        brand_name = missile_brand_name(dummy_item, MBN_TERSE);
        break;
    default:
        break;
    }
    return brand_name;
}

static string _item_name(item_base_type base_type, int sub_type)
{
    string name = "";
    description_level_type desc_type = DESC_DBNAME;

    // These types need special handling, otherwise we use the original
    // item_def name.
    if (sub_type == _item_max_sub_type(base_type))
        name = "All " + _item_class_name(base_type);
    else if (base_type == ITEM_SPELLBOOKS)
    {
        int orig_type = _item_orig_sub_type(base_type, sub_type);
        if (orig_type == BOOK_RANDART_LEVEL)
            name = "Level Artefact Book";
        else if (orig_type == BOOK_RANDART_LEVEL)
            name = "Theme Artefact Book";
    }
    else if (base_type == ITEM_MANUALS)
        desc_type = DESC_QUALNAME;

    if (name.empty())
    {
        const item_def item = _dummy_item(base_type, sub_type);
        name = item.name(desc_type, true, true);
    }
    return name;
}

static string _level_name(const level_id &lev)
{
    string name;
    if (lev.branch == NUM_BRANCHES)
        name = "AllLevels";
    else if (lev.depth == -1 || brdepth[lev.branch] == 1)
        name = lev.describe(false, false);
    else if (brdepth[lev.branch] < 10)
        name = lev.describe(false, true);
    else
    {
        name = make_stringf("%s:%02d", lev.describe(false, false).c_str(),
                            lev.depth);
    }
    return name;
}

objstat_item::objstat_item(const item_def &item)
{
    base_type = _item_base_type(item);

    if (base_type == ITEM_MANUALS)
        sub_type = item.skill;
    else
        sub_type = item.sub_type;

    quantity = item.quantity;
    plus = item.plus;

    is_arte = is_artefact(item);

    // The item's position won't be valid for these two first cases, as it's
    // set to a special indicator value.
    if (item.holding_monster())
    {
        in_vault = map_masked(item.holding_monster()->pos(), MMT_VAULT);
        held_mons = true;
        in_shop = false;
    }
    else if (is_shop_item(item))
    {
        // XXX I don't think shops can place outside of vaults, but it would be
        // nice to find the item's true location, so we wouldn't have to
        // assume. -gammafunk
        in_vault = true;
        held_mons = false;
        in_shop = true;
    }
    else
    {
        in_vault = map_masked(item.pos, MMT_VAULT);
        held_mons = false;
        in_shop = false;
    }

    brand = item.brand;
    if (base_type == ITEM_MISSILES)
        brand = get_ammo_brand(item);
    else if (base_type == ITEM_WEAPONS)
        brand = get_weapon_brand(item);
    else if (base_type == ITEM_ARMOUR)
        brand = get_armour_ego_type(item);

    if (base_type == ITEM_SPELLBOOKS)
        spells = spells_in_book(item);
}

static void _init_monsters()
{
    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        const monster_type mc = static_cast<monster_type>(i);

        if (mons_class_gives_xp(mc) && !mons_class_flag(mc, M_CANT_SPAWN))
            objstat_monsters.insert(mc);
    }
    // For the all-monsters summary
    objstat_monsters.insert(NUM_MONSTERS);
}

static void _init_features()
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        const dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);

        if (is_valid_feature_type(feat))
            objstat_features.insert(feat);
    }
}

static void _init_spells()
{
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        const auto spell = static_cast<spell_type>(i);

        if (is_valid_spell(spell) && is_player_book_spell(spell))
            objstat_spells.insert(spell);
    }
    // For the all-spells summary
    objstat_spells.insert(NUM_SPELLS);
}

// Initialize field data that needs non-default intialization (i.e. to
// something other than zero). Handling this in one pass creates a lot of
// potentially unused entries, but it's better than trying to guard against
// default initialization everywhere. For the rest of the fields, it's fine to
// default initialize to zero whenever they're first referenced.
static void _init_stats()
{
    for (const auto &level : stat_levels)
    {
        for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
        {
            const item_base_type base_type = static_cast<item_base_type>(i);

            int num_entries = _item_max_sub_type(base_type);
            // An additional entry for the across-subtype summary if there's
            // more than one subtype.
            num_entries += num_entries > 1;
            for (int  j = 0; j < num_entries; j++)
            {
                item_recs[level][base_type][j]["NumMin"] = INT_MAX;
                item_recs[level][base_type][j]["NumMax"] = -1;
            }
        }

        for (auto mc : objstat_monsters)
        {
            monster_recs[level][mc]["NumMin"] = INT_MAX;
            monster_recs[level][mc]["NumMax"] = -1;
        }

        for (auto feat : objstat_features)
        {
            feature_recs[level][feat]["NumMin"] = INT_MAX;
            feature_recs[level][feat]["NumMax"] = -1;
        }

        for (auto spell : objstat_spells)
        {
            spell_recs[level][spell]["NumMin"] = INT_MAX;
            spell_recs[level][spell]["NumMax"] = -1;
        }
    }
}

static void _record_item_stat(const objstat_item &item, string field, int value)
{
    const level_id cur_lev = level_id::current();

    bool need_all = false;
    if (stat_levels.count(all_lev))
        need_all = true;

    const level_id br_lev = level_id(cur_lev.branch, -1);
    bool need_branch = false;
    if (stat_levels.count(br_lev))
        need_branch = true;

    item_recs[cur_lev][item.base_type][item.sub_type][field] += value;

    if (need_all)
        item_recs[all_lev][item.base_type][item.sub_type][field] += value;

    if (need_branch)
        item_recs[br_lev][item.base_type][item.sub_type][field] += value;

    // Record a class summary if more than one subtype exists.
    const int class_sum = _item_max_sub_type(item.base_type);
    if (class_sum > 1)
    {
        item_recs[cur_lev][item.base_type][class_sum][field] += value;

        if (need_all)
            item_recs[all_lev][item.base_type][class_sum][field] += value;

        if (need_branch)
            item_recs[br_lev][item.base_type][class_sum][field] += value;
    }
}

static void _record_item_brand_category(const objstat_item &item,
        stat_category_type cat)
{
    const level_id cur_lev = level_id::current();

    bool need_all = false;
    if (stat_levels.count(all_lev))
        need_all = true;

    const level_id br_lev = level_id(cur_lev.branch, -1);
    bool need_branch = false;
    if (stat_levels.count(br_lev))
        need_branch = true;

    brand_recs[cur_lev][item.base_type][item.sub_type][cat][item.brand] +=
        item.quantity;

    if (need_all)
    {
        brand_recs[all_lev][item.base_type][item.sub_type][cat][item.brand] +=
            item.quantity;
    }

    if (need_branch)
    {
        brand_recs[br_lev][item.base_type][item.sub_type][cat][item.brand] +=
            item.quantity;
    }

    // Record a class summary if more than one subtype exists.
    const int cls = _item_max_sub_type(item.base_type);
    if (cls > 1)
    {
        brand_recs[cur_lev][item.base_type][cls][cat][item.brand] +=
            item.quantity;

        if (need_all)
        {
            brand_recs[all_lev][item.base_type][cls][cat][item.brand] +=
                item.quantity;
        }

        if (need_branch)
        {
            brand_recs[br_lev][item.base_type][cls][cat][item.brand] +=
                item.quantity;
        }
    }
}

static void _record_item_brand(const objstat_item &item)
{
    ASSERT(_item_tracks_brand(item.base_type));

    _record_item_brand_category(item, CATEGORY_ALL);

    if (item.in_vault)
        _record_item_brand_category(item, CATEGORY_VAULT);

    if (item.is_arte)
        _record_item_brand_category(item, CATEGORY_ARTEFACT);

    if (item.in_shop)
        _record_item_brand_category(item, CATEGORY_SHOP);

    if (item.held_mons)
        _record_item_brand_category(item, CATEGORY_MONSTER);
}

static void _record_spell_stat(spell_type spell, string field, int value)
{
    const level_id cur_lev = level_id::current();

    bool need_all = false;
    if (stat_levels.count(all_lev))
        need_all = true;

    const level_id br_lev = level_id(cur_lev.branch, -1);
    bool need_branch = false;
    if (stat_levels.count(br_lev))
        need_branch = true;

    spell_recs[cur_lev][spell][field] += value;
    spell_recs[cur_lev][NUM_SPELLS][field] += value;

    if (need_all)
    {
        spell_recs[all_lev][spell][field] += value;
        spell_recs[all_lev][NUM_SPELLS][field] += value;
    }

    if (need_branch)
    {
        spell_recs[br_lev][spell][field] += value;
        spell_recs[br_lev][NUM_SPELLS][field] += value;
    }
}

static void _record_book_spells(const objstat_item &item)
{
    for (auto spell : item.spells)
    {
        _record_spell_stat(spell, "Num", 1);
        _record_spell_stat(spell, "NumForIter", 1);

        if (item.in_vault)
        {
            _record_spell_stat(spell, "NumVault", 1);
            _record_spell_stat(spell, "NumForIterVault", 1);
        }

        if (item.is_arte)
        {
            _record_spell_stat(spell, "NumArte", 1);
            _record_spell_stat(spell, "NumForIterArte", 1);
        }

        if (item.in_shop)
        {
            _record_spell_stat(spell, "NumShop", 1);
            _record_spell_stat(spell, "NumForIterShop", 1);
        }
    }
}

void objstat_record_item(const item_def &item)
{
    const objstat_item objs_item(item);

    // Just in case, don't count mimics as items; these are converted
    // explicitely in mg_do_build_level().
    if (item.flags & ISFLAG_MIMIC || objs_item.base_type == ITEM_IGNORE)
        return;

    const bool track_plus = _item_tracks_plus(objs_item.base_type);
    string plus_field = "Ench";
    if (objs_item.base_type == ITEM_WANDS)
        plus_field = "Chrg";

    _record_item_stat(objs_item, "Num", objs_item.quantity);
    _record_item_stat(objs_item, "NumForIter", objs_item.quantity);

    if (track_plus)
        _record_item_stat(objs_item, plus_field, objs_item.plus);

    if (objs_item.in_vault)
    {
        _record_item_stat(objs_item, "NumVault", objs_item.quantity);

        if (track_plus)
            _record_item_stat(objs_item, plus_field + "Vault", objs_item.plus);
    }

    if (_item_tracks_artefact(objs_item.base_type) && objs_item.is_arte)
    {
        _record_item_stat(objs_item, "NumArte", objs_item.quantity);

        if (track_plus)
            _record_item_stat(objs_item, plus_field + "Arte", objs_item.plus);
    }

    if (_item_tracks_brand(objs_item.base_type) && objs_item.brand > 0)
    {
        _record_item_stat(objs_item, "NumBrand", objs_item.quantity);

        if (track_plus)
            _record_item_stat(objs_item, plus_field + "Brand", objs_item.plus);

        _record_item_brand(objs_item);
    }

    if (_item_tracks_shop(objs_item.base_type) && objs_item.in_shop)
    {
        _record_item_stat(objs_item, "NumShop", objs_item.quantity);

        if (track_plus)
            _record_item_stat(objs_item, plus_field + "Shop", objs_item.plus);
    }

    if (_item_tracks_monster(objs_item.base_type) && objs_item.held_mons)
    {
        _record_item_stat(objs_item, "NumMons", objs_item.quantity);

        if (track_plus)
            _record_item_stat(objs_item, plus_field + "Mons", objs_item.plus);
    }

    _record_book_spells(objs_item);
}

static void _record_monster_stat(monster_type mc, string field, int value)
{
    const level_id cur_lev = level_id::current();

    bool need_all = false;
    if (stat_levels.count(all_lev))
        need_all = true;

    const level_id br_lev = level_id(cur_lev.branch, -1);
    bool need_branch = false;
    if (stat_levels.count(br_lev))
        need_branch = true;

    monster_recs[cur_lev][mc][field] += value;
    monster_recs[cur_lev][NUM_MONSTERS][field] += value;

    if (need_all)
    {
        monster_recs[all_lev][mc][field] += value;
        monster_recs[all_lev][NUM_MONSTERS][field] += value;
    }

    if (need_branch)
    {
        monster_recs[br_lev][mc][field] += value;
        monster_recs[br_lev][NUM_MONSTERS][field] += value;
    }
}

void objstat_record_monster(const monster *mons)
{
    monster_type type;
    if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        type = MONS_GLOWING_SHAPESHIFTER;
    else if (mons->has_ench(ENCH_SHAPESHIFTER))
        type = MONS_SHAPESHIFTER;
    else
        type = mons->type;

    if (!objstat_monsters.count(type))
        return;

    _record_monster_stat(type, "Num", 1);
    _record_monster_stat(type, "NumForIter", 1);
    _record_monster_stat(type, "MonsXP", exper_value(*mons));
    _record_monster_stat(type, "TotalXP", exper_value(*mons));
    _record_monster_stat(type, "MonsHP", mons->max_hit_points);
    _record_monster_stat(type, "MonsHD", mons->get_experience_level());

    if (!mons->originating_map().empty())
    {
        _record_monster_stat(type, "NumVault", 1);
        _record_monster_stat(type, "TotalXPVault", exper_value(*mons));
    }
}

static void _record_feature_stat(dungeon_feature_type feat_type, string field,
        int value)
{
    const level_id cur_lev = level_id::current();

    bool need_all = false;
    if (stat_levels.count(all_lev))
        need_all = true;

    const level_id br_lev = level_id(cur_lev.branch, -1);
    bool need_branch = false;
    if (stat_levels.count(br_lev))
        need_branch = true;

    feature_recs[cur_lev][feat_type][field] += value;

    if (need_all)
        feature_recs[all_lev][feat_type][field] += value;

    if (need_branch)
        feature_recs[br_lev][feat_type][field] += value;
}

void objstat_record_feature(dungeon_feature_type feat, bool in_vault)
{
    if (!objstat_features.count(feat))
        return;

    _record_feature_stat(feat, "Num", 1);
    _record_feature_stat(feat, "NumForIter", 1);

    if (in_vault)
        _record_feature_stat(feat, "NumVault", 1);
}

void objstat_iteration_stats()
{
    for (const auto level : stat_levels)
    {
        for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
        {
            item_base_type base_type = static_cast<item_base_type>(i);

            int num_entries = _item_max_sub_type(base_type);
            num_entries = num_entries == 1 ? 1 : num_entries + 1;
            for (int  j = 0; j < num_entries ; j++)
            {
                map<string, int> &stats = item_recs[level][base_type][j];

                if (stats["NumForIter"] > stats["NumMax"])
                    stats["NumMax"] = stats["NumForIter"];

                if (stats["NumForIter"] < stats["NumMin"])
                    stats["NumMin"] = stats["NumForIter"];

                stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];

                stats["NumForIter"] = 0;
            }
        }

        for (auto spell : objstat_spells)
        {
            map<string, int> &stats = spell_recs[level][spell];

            if (stats["NumForIter"] > 0)
                stats["Chance"] += 1;

            if (stats["NumForIter"] > stats["NumMax"])
                stats["NumMax"] = stats["NumForIter"];

            if (stats["NumForIter"] < stats["NumMin"])
                stats["NumMin"] = stats["NumForIter"];

            stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];

            stats["NumForIter"] = 0;

            if (stats["NumForIterVault"])
                stats["ChanceVault"] += 1;
            stats["NumForIterVault"] = 0;

            if (stats["NumForIterArte"])
                stats["ChanceArte"] += 1;
            stats["NumForIterArte"] = 0;

            if (stats["NumForIterShop"])
                stats["ChanceShop"] += 1;
            stats["NumForIterShop"] = 0;
        }

        for (auto mc : objstat_monsters)
        {
            map<string, int> &stats = monster_recs[level][mc];

            if (stats["NumForIter"] > stats["NumMax"])
                stats["NumMax"] = stats["NumForIter"];

            if (stats["NumForIter"] < stats["NumMin"])
                stats["NumMin"] = stats["NumForIter"];

            stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];

            stats["NumForIter"] = 0;
        }

        for (auto feat : objstat_features)
        {
            map<string, int> &stats = feature_recs[level][feat];

            if (stats["NumForIter"] > stats["NumMax"])
                stats["NumMax"] = stats["NumForIter"];

            if (stats["NumForIter"] < stats["NumMin"])
                stats["NumMin"] = stats["NumForIter"];

            stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];

            stats["NumForIter"] = 0;
        }
    }
}

static FILE * _open_stat_file(string stat_file)
{
    FILE *stat_fh = nullptr;
    stat_fh = fopen(stat_file.c_str(), "w");

    if (!stat_fh)
    {
        end(1, false, "Unable to open objstat output file: %s\n"
                "Error: %s", stat_file.c_str(), strerror(errno));
    }

    return stat_fh;
}

static void _write_stat_info()
{
    string all_desc;
    if (num_branches > 1)
    {
        if (SysEnv.map_gen_range)
            all_desc = SysEnv.map_gen_range->describe();
        else
            all_desc = "All Levels";
        all_desc = "Levels included in AllLevels: " + all_desc + "\n";
    }

    const string out_file = make_stringf("%s%s%s", stat_out_prefix,
                                         "Info", stat_out_ext);
    stat_outf = _open_stat_file(out_file.c_str());

    fprintf(stat_outf, "Object Generation Stats\n"
            "Number of iterations: %d\n"
            "Number of branches: %d\n"
            "%s"
            "Number of levels: %d\n"
            "Version: %s\n", SysEnv.map_gen_iters, num_branches,
            all_desc.c_str(), num_levels, Version::Long);

    fclose(stat_outf);
    printf("Wrote Objstat Info to %s.\n", out_file.c_str());
}

static void _write_stat_headers(const vector<string> &fields, const string &desc)
{
    fprintf(stat_outf, "%s\tLevel", desc.c_str());

    for (const string &field : fields)
        fprintf(stat_outf, "\t%s", field.c_str());

    fprintf(stat_outf, "\n");
}

static void _write_stat(map<string, int> &stats, const string &field)
{
    const double field_val = stats[field];
    double out_val = 0;
    ostringstream output;
    bool is_chance = false;

    output.precision(STAT_PRECISION);
    output.setf(ios_base::fixed);

    // These fields want a per-instance average.
    if (starts_with(field, "Ench")
        || starts_with(field, "Chrg")
        || field == "MonsHD"
        || field == "MonsHP"
        || field == "MonsXP")
    {
        string num_field = "Num";
        // The classed fields need to be average relative to the right count.
        if (ends_with(field, "Vault"))
            num_field = "NumVault";
        else if (ends_with(field, "Shop"))
            num_field = "NumShop";
        else if (ends_with(field, "Mons"))
            num_field = "NumMons";
        out_val = field_val / stats[num_field.c_str()];
    }
    // Turn the sum of squares into the standard deviation.
    else if (field == "NumSD")
    {
        if (SysEnv.map_gen_iters == 1)
            out_val = 0;
        else
        {
            const double mean = (double) stats["Num"] / SysEnv.map_gen_iters;
            out_val = sqrt((SysEnv.map_gen_iters / (SysEnv.map_gen_iters - 1.0))
                         * (field_val / SysEnv.map_gen_iters - mean * mean));
        }
    }
    else if (ends_with(field, "Min") || ends_with(field, "Max"))
        out_val = field_val;
    // Turn a probability into a chance percentage.
    else if (field.find("Chance") == 0)
    {
        out_val = 100 * field_val / SysEnv.map_gen_iters;
        is_chance = true;
    }
    else
        out_val = field_val / SysEnv.map_gen_iters;

    output << "\t" << out_val;

    if (is_chance)
        output << "%";

    fprintf(stat_outf, "%s", output.str().c_str());
}

static void _write_level_item_brand_stats(const level_id &level,
        item_base_type base_type, int sub_type)
{
    for (int i = 0; i < NUM_STAT_CATEGORIES; i++)
    {
        bool first_brand = true;

        ostringstream brand_summary;
        brand_summary.setf(ios_base::fixed);
        brand_summary.precision(STAT_PRECISION);

        const auto cat = static_cast<stat_category_type>(i);

        for (auto mentry : brand_recs[level][base_type][sub_type][cat])
        {
            // No instances for this brand.
            if (mentry.second == 0)
                continue;

            const double value = (double) mentry.second / SysEnv.map_gen_iters;

            if (first_brand)
                first_brand = false;
            else
                brand_summary << ";";

            string brand_name = _brand_name(base_type, sub_type, mentry.first);
            brand_summary << brand_name.c_str() << ":" << value;
        }

        fprintf(stat_outf, "\t%s", brand_summary.str().c_str());
    }
}

static void _write_level_item_stats(const level_id &level,
        item_base_type base_type, int sub_type)
{
    // Don't print stats if no instances.
    if (item_recs[level][base_type][sub_type]["Num"] < 1)
        return;

    fprintf(stat_outf, "%s\t%s", _item_name(base_type, sub_type).c_str(),
            _level_name(level).c_str());

    for (const auto &field : item_fields[base_type])
        _write_stat(item_recs[level][base_type][sub_type], field);

    if (_item_tracks_brand(base_type))
        _write_level_item_brand_stats(level, base_type, sub_type);

    fprintf(stat_outf, "\n");
}

static void _write_base_item_stats(item_base_type base_type)
{
    const string out_file = make_stringf("%s%s%s", stat_out_prefix,
            strip_filename_unsafe_chars(_item_class_name(base_type)).c_str(),
            stat_out_ext);
    stat_outf = _open_stat_file(out_file.c_str());

    vector<string> fields = item_fields[base_type];

    if (_item_tracks_brand(base_type))
    {
        fields.insert(fields.end(), brand_fields[base_type].begin(),
                brand_fields[base_type].end());
    }

    _write_stat_headers(fields, "Item");

    // If there is more than one subtype, we have an additional entry for
    // the sum across subtypes.
    const int num_types = _item_max_sub_type(base_type);
    int num_entries = num_types == 1 ? 1 : num_types + 1;
    for (int j = 0; j < num_entries; j++)
        for (const auto &level : stat_levels)
            _write_level_item_stats(level, base_type, j);

    fclose(stat_outf);
    printf("Wrote %s item stats to %s.\n", _item_class_name(base_type).c_str(),
           out_file.c_str());
}

static void _write_monster_stats(monster_type mc)
{
    string mons_name;
    if (mc == NUM_MONSTERS)
        mons_name = "All Monsters";
    else
        mons_name = mons_type_name(mc, DESC_PLAIN);

    for (const level_id &level: stat_levels)
    {
        if (monster_recs[level][mc]["Num"] < 1)
            continue;

        fprintf(stat_outf, "%s\t%s", mons_name.c_str(),
                _level_name(level).c_str());

        for (const string &field : monster_fields)
            _write_stat(monster_recs[level][mc], field);

        fprintf(stat_outf, "\n");
    }
}

static void _write_feature_stats(dungeon_feature_type feat_type)
{
    const char *feat_name = get_feature_def(feat_type).name;

    for (const level_id &level : stat_levels)
    {
        if (feature_recs[level][feat_type]["Num"] < 1)
            continue;

        fprintf(stat_outf, "%s\t%s", feat_name, _level_name(level).c_str());

        for (const string &field : feature_fields)
            _write_stat(feature_recs[level][feat_type], field);

        fprintf(stat_outf, "\n");
    }
}

static void _write_spell_stats(spell_type spell)
{
    string spell_name;
    if (spell == NUM_SPELLS)
        spell_name = "All Spells";
    else
        spell_name = spell_title(spell);

    for (const level_id &level : stat_levels)
    {
        if (spell_recs[level][spell]["Num"] < 1)
            continue;

        fprintf(stat_outf, "%s\t%s", spell_name.c_str(),
                _level_name(level).c_str());

        for (const string &field : spell_fields)
            _write_stat(spell_recs[level][spell], field);

        fprintf(stat_outf, "\n");
    }
}

static void _write_object_stats()
{
    _write_stat_info();

    for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
    {
        item_base_type base_type = static_cast<item_base_type>(i);
        _write_base_item_stats(base_type);
    }

    string out_file = make_stringf("%s%s%s", stat_out_prefix, "Spells",
                                   stat_out_ext);
    stat_outf = _open_stat_file(out_file.c_str());

    _write_stat_headers(spell_fields, "Spell");
    for (const auto spell : objstat_spells)
        _write_spell_stats(spell);

    fclose(stat_outf);
    printf("Wrote Spell stats to %s.\n", out_file.c_str());

    out_file = make_stringf("%s%s%s", stat_out_prefix, "Monsters",
                                   stat_out_ext);
    stat_outf = _open_stat_file(out_file.c_str());

    _write_stat_headers(monster_fields, "Monster");
    for (const auto mc : objstat_monsters)
        _write_monster_stats(mc);

    fclose(stat_outf);
    printf("Wrote Monster stats to %s.\n", out_file.c_str());

    out_file = make_stringf("%s%s%s", stat_out_prefix, "Features",
                            stat_out_ext);
    stat_outf = _open_stat_file(out_file.c_str());

    _write_stat_headers(feature_fields, "Feature");
    for (auto feat : objstat_features)
        _write_feature_stats(feat);

    fclose(stat_outf);
    printf("Wrote Feature stats to %s.\n", out_file.c_str());
}

void objstat_generate_stats()
{
    // Warn assertions about possible oddities like the artefact list being
    // cleared.
    you.wizard = true;
    // Let "acquire foo" have skill aptitudes to work with.
    you.species = SP_HUMAN;

    if (!crawl_state.force_map.empty() && !mapstat_find_forced_map())
        return;

    initialise_item_descriptions();
    initialise_branch_depths();

    // We have to run map preludes ourselves.
    run_map_global_preludes();
    run_map_local_preludes();

    // Populate a vector of the levels ids for levels we're tabulating.
    for (branch_iterator it; it; ++it)
    {
        if (brdepth[it->id] == -1)
            continue;

        const branch_type br = it->id;
#if TAG_MAJOR_VERSION == 34
        // Don't want to include Forest since it doesn't generate
        if (br == BRANCH_FOREST)
            continue;
#endif
        vector<level_id> levels;
        int branch_level_count = 0;
        for (int dep = 1; dep <= brdepth[br]; ++dep)
        {
            const level_id lid(br, dep);
            if (SysEnv.map_gen_range
                && !SysEnv.map_gen_range->is_usable_in(lid))
            {
                continue;
            }
            stat_levels.insert(lid);
            ++branch_level_count;
            ++num_levels;
        }

        if (branch_level_count > 0)
        {
            ++num_branches;
            // Multiple levels for this branch, so we do a branch-wise summary.
            if (branch_level_count > 1)
                stat_levels.insert(level_id(br, -1));

        }
    }

    // If there's only one branch, an all-levels summary isn't necessary.
    if (num_branches > 1)
        stat_levels.insert(all_lev);

    printf("Generating object statistics for %d iteration(s) of %d "
           "level(s) over %d branch(es).\n", SysEnv.map_gen_iters,
           num_levels, num_branches);

    _init_spells();
    _init_features();
    _init_monsters();

    _init_stats();

    if (mapstat_build_levels())
    {
        _write_object_stats();
        printf("Object statistics complete.\n");
    }
}
#endif // DEBUG_STATISTICS

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
#include "butcher.h"
#include "chardump.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-maps.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "god-abil.h"
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
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "version.h"

#ifdef DEBUG_STATISTICS
static FILE *stat_outf;
const static char *stat_out_prefix = "objstat_";
const static char *stat_out_ext = ".txt";
#define STAT_PRECISION 2

// This must match the order of item_fields
enum item_base_type
{
    ITEM_FOOD,
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
    ITEM_BOOKS,
    ITEM_ARTEBOOKS,
    ITEM_MANUALS,
    NUM_ITEM_BASE_TYPES,
    ITEM_IGNORE = 100,
};

enum antiquity_level
{
    ANTIQ_ORDINARY,
    ANTIQ_ARTEFACT,
    ANTIQ_ALL,
};

class item_type
{
public:
    item_type(item_base_type ct, int st) : base_type(ct), sub_type(st)
    {
    }
    item_type(const item_def &item);

    item_base_type base_type;
    int sub_type;
};

static level_id all_lev(NUM_BRANCHES, -1);
static map<branch_type, vector<level_id> > stat_branches;
static int num_branches = 0;
static int num_levels = 0;

// item_recs[level_id][item.base_type][item.sub_type][field]
static map<level_id, vector< vector< map<string, double> > > > item_recs;

// weapon_brands[level_id][item.base_type][item.sub_type][antiquity_level][brand];
typedef map<level_id, vector< vector< vector< int> > > > brand_records;
static brand_records weapon_brands;
static brand_records armour_brands;

static map<level_id, vector< vector< int> > > missile_brands;

// This must match the order of item_base_type
static const vector<string> item_fields[NUM_ITEM_BASE_TYPES] = {
    { // ITEM_FOOD
        "Num", "NumMin", "NumMax", "NumSD", "NumPiles", "PileQuant",
        "TotalNormNutr", "TotalCarnNutr", "TotalHerbNutr"
    },
    { // ITEM_GOLD
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
        "NumPiles", "PileQuant"
    },
    { // ITEM_SCROLLS
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
        "NumPiles", "PileQuant"
    },
    { // ITEM_POTIONS
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
        "NumPiles", "PileQuant"
    },
    { // ITEM_WANDS
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons", "WandCharges"
    },
    { // ITEM_WEAPONS
        "OrdNum", "ArteNum", "AllNum", "AllNumMin",
        "AllNumMax", "AllNumSD", "OrdEnch", "ArteEnch",
        "AllEnch", "OrdNumCursed", "ArteNumCursed",
        "AllNumCursed", "OrdNumBranded", "OrdNumHeldMons",
        "ArteNumHeldMons", "AllNumHeldMons"
    },
    { // ITEM_MISSILES
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
        "NumBranded", "NumPiles", "PileQuant"
    },
    { // ITEM_STAVES
        "Num", "NumMin", "NumMax", "NumSD", "NumCursed", "NumHeldMons"
    },
    { // ITEM_ARMOUR
        "OrdNum", "ArteNum", "AllNum", "AllNumMin", "AllNumMax", "AllNumSD",
        "OrdEnch", "ArteEnch", "AllEnch",
        "OrdNumCursed", "ArteNumCursed", "AllNumCursed", "OrdNumBranded",
        "OrdNumHeldMons", "ArteNumHeldMons", "AllNumHeldMons"
    },
    { // ITEM_JEWELLERY
        "OrdNum", "ArteNum", "AllNum", "AllNumMin", "AllNumMax", "AllNumSD",
        "OrdNumCursed", "ArteNumCursed", "AllNumCursed",
        "OrdNumHeldMons", "ArteNumHeldMons", "AllNumHeldMons",
        "OrdEnch", "ArteEnch", "AllEnch"
    },
    { // ITEM_MISCELLANY
        "Num", "NumMin", "NumMax", "NumSD", "MiscPlus"
    },
    { // ITEM_BOOKS
        "Num", "NumMin", "NumMax", "NumSD"
    },
    { // ITEM_ARTEBOOKS
        "Num", "NumMin", "NumMax", "NumSD"
    },
    { // ITEM_MANUALS
        "Num", "NumMin", "NumMax", "NumSD"
    },
};

static const char* equip_brand_fields[] = {"OrdBrandNums", "ArteBrandNums",
                                           "AllBrandNums"};
static const char* missile_brand_field = "BrandNums";

static const vector<string> monster_fields = {
    "Num", "NumNonVault", "NumVault", "NumMin", "NumMax", "NumSD", "MonsHD",
    "MonsHP", "MonsXP", "TotalXP", "TotalNonVaultXP", "TotalVaultXP",
    "MonsNumChunks", "MonsNumMutChunks", "TotalNutr", "TotalCarnNutr",
    "TotalGhoulNutr",
};

static map<monster_type, int> valid_monsters;
static map<level_id, map<int, map <string, double> > > monster_recs;

static void _init_monsters()
{
    int num_mons = 0;
    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        monster_type mc = static_cast<monster_type>(i);
        if (mons_class_gives_xp(mc) && !mons_class_flag(mc, M_CANT_SPAWN))
            valid_monsters[mc] = num_mons++;
    }
    // For the all-monster summary
    valid_monsters[NUM_MONSTERS] = num_mons;
}

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
        else if (is_artefact(item))
            type = ITEM_ARTEBOOKS;
        else
            type = ITEM_BOOKS;
        break;
    case OBJ_FOOD:
        type = ITEM_FOOD;
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
    case ITEM_FOOD:
        type = OBJ_FOOD;
        break;
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
    case ITEM_ARTEBOOKS:
    case ITEM_MANUALS:
    case ITEM_BOOKS:
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
    case ITEM_ARTEBOOKS:
        name = "Artefact Spellbooks";
        break;
    case ITEM_MANUALS:
        name = "Manuals";
        break;
    default:
        name = item_class_name(_item_orig_base_type(base_type));
    }
    return name;
}

static int _item_orig_sub_type(const item_type &item)
{
    int type;
    switch (item.base_type)
    {
    case ITEM_MISCELLANY:
        type = misc_types[item.sub_type];
        break;
    case ITEM_ARTEBOOKS:
        type = item.sub_type + BOOK_RANDART_LEVEL;
        break;
    case ITEM_MANUALS:
        type = BOOK_MANUAL;
        break;
    default:
        type = item.sub_type;
        break;
    }
    return type;
}

static int _item_max_sub_type(item_base_type base_type)
{
    int num = 0;
    switch (base_type)
    {
    case ITEM_MISCELLANY:
        num = misc_types.size();
        break;
    case ITEM_BOOKS:
        num = MAX_FIXED_BOOK + 1;
        break;
    case ITEM_ARTEBOOKS:
        num = 2;
        break;
    case ITEM_MANUALS:
        num = 1;
        break;
    default:
        num = get_max_subtype(_item_orig_base_type(base_type));
        break;
    }
    return num;
}

static item_def _dummy_item(const item_type &item)
{
    item_def dummy_item;
    dummy_item.base_type = _item_orig_base_type(item.base_type);
    dummy_item.sub_type = _item_orig_sub_type(item);
    dummy_item.quantity = 1;
    return dummy_item;
}

/**
 * Return true if the item type can ever be an artefact.
 * @param base_type the item base type.
 * @returns true if the item type can ever be an artefact.
*/
static bool _item_has_antiquity(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

item_type::item_type(const item_def &item)
{
    base_type = _item_base_type(item);
    if (base_type == ITEM_MISCELLANY)
    {
        sub_type = find(misc_types.begin(), misc_types.end(), item.sub_type)
                        - misc_types.begin();
        ASSERT(sub_type < (int) misc_types.size());
    }
    else if (base_type == ITEM_ARTEBOOKS)
        sub_type = item.sub_type - BOOK_RANDART_LEVEL;
    else if (base_type == ITEM_MANUALS)
        sub_type = 0;
    else
        sub_type = item.sub_type;
}

static void _init_stats()
{
    for (const auto &entry : stat_branches)
    {
        for (unsigned int l = 0; l <= entry.second.size(); l++)
        {
            level_id lev;
            if (l == entry.second.size())
            {
                if (entry.first == NUM_BRANCHES)
                    continue;
                lev.branch = entry.first;
                lev.depth = -1;
            }
            else
                lev = (entry.second)[l];
            item_recs[lev] = { };
            for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
            {
                item_base_type base_type = static_cast<item_base_type>(i);
                string min_field = _item_has_antiquity(base_type)
                    ? "AllNumMin" : "NumMin";
                string max_field = _item_has_antiquity(base_type)
                    ? "AllNumMax" : "NumMax";
                item_recs[lev].emplace_back();
                // An additional entry for the across-subtype summary if
                // there's more than one.
                int num_entries = _item_max_sub_type(base_type);
                num_entries = num_entries == 1 ? 1 : num_entries + 1;
                for (int  j = 0; j < num_entries; j++)
                {
                    item_recs[lev][i].emplace_back();
                    for (const string &field : item_fields[i])
                        item_recs[lev][i][j][field] = 0;
                    // For determining the NumSD, NumMin and NumMax fields.
                    item_recs[lev][i][j]["NumForIter"] = 0;
                    item_recs[lev][i][j][min_field] = INFINITY;
                    item_recs[lev][i][j][max_field] = -1;
                }
            }
            weapon_brands[lev] = { };
            for (int i = 0; i < NUM_WEAPONS + 1; i++)
            {
                weapon_brands[lev].emplace_back(3,
                        vector<int>(NUM_SPECIAL_WEAPONS, 0));
            }

            armour_brands[lev] = { };
            for (int i = 0; i < NUM_ARMOURS + 1; i++)
            {
                armour_brands[lev].emplace_back(3,
                        vector<int>(NUM_SPECIAL_ARMOURS, 0));
            }

            missile_brands[lev] = { };
            for (int i = 0; i < NUM_MISSILES + 1; i++)
                missile_brands[lev].emplace_back(NUM_SPECIAL_MISSILES, 0);

            for (const auto &mentry : valid_monsters)
            {
                for (const string &field : monster_fields)
                    monster_recs[lev][mentry.second][field] = 0;
                // For determining the NumMin and NumMax fields.
                monster_recs[lev][mentry.second]["NumForIter"] = 0;
                monster_recs[lev][mentry.second]["NumMin"] = INFINITY;
                monster_recs[lev][mentry.second]["NumMax"] = -1;
            }
        }
    }
}

static void _record_item_stat(const level_id &lev, const item_type &item,
                              string field, double value)
{
    int class_sum = _item_max_sub_type(item.base_type);
    level_id br_lev(lev.branch, -1);

    item_recs[lev][item.base_type][item.sub_type][field] += value;

    item_recs[br_lev][item.base_type][item.sub_type][field] += value;
    item_recs[all_lev][item.base_type][item.sub_type][field] += value;
    // Only record a class summary if more than one subtype exists
    if (class_sum > 1)
    {
        item_recs[lev][item.base_type][class_sum][field] += value;
        item_recs[br_lev][item.base_type][class_sum][field] += value;
        item_recs[all_lev][item.base_type][class_sum][field] += value;
    }
}

static void _record_equip_brand(brand_records &brands, const level_id &lev,
                                const item_type &item, int quantity,
                                bool is_arte, int brand)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR);

    int allst = _item_max_sub_type(item.base_type);
    int antiq = is_arte ? ANTIQ_ARTEFACT : ANTIQ_ORDINARY;
    level_id br_lev(lev.branch, -1);

    brands[lev][item.sub_type][antiq][brand] += quantity;
    brands[lev][item.sub_type][ANTIQ_ALL][brand] += quantity;
    brands[lev][allst][antiq][brand] += quantity;
    brands[lev][allst][ANTIQ_ALL][brand] += quantity;

    brands[br_lev][item.sub_type][antiq][brand] += quantity;
    brands[br_lev][item.sub_type][ANTIQ_ALL][brand] += quantity;
    brands[br_lev][allst][antiq][brand] += quantity;
    brands[br_lev][allst][ANTIQ_ALL][brand] += quantity;

    brands[all_lev][item.sub_type][antiq][brand] += quantity;
    brands[all_lev][item.sub_type][ANTIQ_ALL][brand] += quantity;
    brands[all_lev][allst][antiq][brand] += quantity;
    brands[all_lev][allst][ANTIQ_ALL][brand] += quantity;
}


static void _record_brand(const level_id &lev, const item_type &item,
                          int quantity, bool is_arte, int brand)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    const int allst = _item_max_sub_type(item.base_type);
    const bool is_weap = item.base_type == ITEM_WEAPONS;
    const bool is_armour = item.base_type == ITEM_ARMOUR;
    const level_id br_lev(lev.branch, -1);

    if (is_weap)
        _record_equip_brand(weapon_brands, lev, item, quantity, is_arte, brand);
    else if (is_armour)
        _record_equip_brand(armour_brands, lev, item, quantity, is_arte, brand);
    else
    {
        missile_brands[lev][item.sub_type][brand] += quantity;
        missile_brands[lev][allst][brand] += quantity;
        missile_brands[br_lev][item.sub_type][brand] += quantity;
        missile_brands[br_lev][allst][brand] += quantity;
        missile_brands[all_lev][item.sub_type][brand] += quantity;
        missile_brands[all_lev][allst][brand] += quantity;
    }
}

static bool _item_track_piles(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_GOLD:
    case ITEM_POTIONS:
    case ITEM_SCROLLS:
    case ITEM_FOOD:
    case ITEM_MISSILES:
        return true;
    default:
        return false;
    }
}

static bool _item_track_curse(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_STAVES:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

static bool _item_track_plus(item_base_type base_type)
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

static bool _item_track_brand(item_base_type base_type)
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

static bool _item_track_monster(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_GOLD:
    case ITEM_POTIONS:
    case ITEM_SCROLLS:
    case ITEM_WANDS:
    case ITEM_WEAPONS:
    case ITEM_STAVES:
    case ITEM_ARMOUR:
    case ITEM_MISSILES:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

void objstat_record_item(const item_def &item)
{
    level_id cur_lev = level_id::current();
    item_type itype(item);
    bool is_arte = is_artefact(item);
    int brand = -1;
    bool has_antiq = _item_has_antiquity(itype.base_type);
    string all_num_f = has_antiq ? "AllNum" : "Num";
    string antiq_num_f = is_arte ? "ArteNum" : "OrdNum";
    string all_cursed_f = has_antiq ? "AllNumCursed" : "NumCursed";
    string antiq_cursed_f = is_arte ? "ArteNumCursed" : "OrdNumCursed";
    string all_num_hm_f = has_antiq ? "AllNumHeldMons" : "NumHeldMons";
    string antiq_num_hm_f = is_arte ? "ArteNumHeldMons" : "OrdNumHeldMons";
    string all_plus_f = "AllEnch";
    string antiq_plus_f = is_arte ? "ArteEnch" : "OrdEnch";
    string num_brand_f = has_antiq ? "OrdNumBranded" : "NumBranded";

    // Just in case, don't count mimics as items; these are converted
    // explicitely in mg_do_build_level().
    if (item.flags & ISFLAG_MIMIC || itype.base_type == ITEM_IGNORE)
        return;

    // Some type-specific calculations.
    switch (itype.base_type)
    {
    case ITEM_MISSILES:
        brand = get_ammo_brand(item);
        break;
    case ITEM_FOOD:
        _record_item_stat(cur_lev, itype, "TotalNormNutr", food_value(item));
        // Set these dietary mutations so we can get accurate nutrition.
        you.mutation[MUT_CARNIVOROUS] = 3;
        _record_item_stat(cur_lev, itype, "TotalCarnNutr",
                             food_value(item) * food_is_meaty(item.sub_type));
        you.mutation[MUT_CARNIVOROUS] = 0;
        you.mutation[MUT_HERBIVOROUS] = 3;
        _record_item_stat(cur_lev, itype, "TotalHerbNutr",
                             food_value(item) * food_is_veggie(item.sub_type));
        you.mutation[MUT_HERBIVOROUS] = 0;
        break;
    case ITEM_WEAPONS:
        brand = get_weapon_brand(item);
        break;
    case ITEM_ARMOUR:
        brand = get_armour_ego_type(item);
        break;
    case ITEM_WANDS:
        all_plus_f = "WandCharges";
        break;
    case ITEM_MISCELLANY:
        all_plus_f = "MiscPlus";
        break;
    default:
        break;
    }
    if (_item_track_piles(itype.base_type))
        _record_item_stat(cur_lev, itype, "NumPiles", 1);
    if (_item_track_curse(itype.base_type) && item.cursed())
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_cursed_f, 1);
        _record_item_stat(cur_lev, itype, all_cursed_f, 1);
    }
    if (_item_track_plus(itype.base_type))
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_plus_f, item.plus);
        _record_item_stat(cur_lev, itype, all_plus_f, item.plus);
    }

    if (_item_track_brand(itype.base_type))
    {
        _record_brand(cur_lev, itype, item.quantity, is_arte, brand);
        if (!is_arte && brand > 0)
            _record_item_stat(cur_lev, itype, num_brand_f, item.quantity);
    }
    if (_item_track_monster(itype.base_type) && item.holding_monster())
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_num_hm_f, item.quantity);
        _record_item_stat(cur_lev, itype, all_num_hm_f, item.quantity);
    }
    if (has_antiq)
        _record_item_stat(cur_lev, itype, antiq_num_f, item.quantity);
    _record_item_stat(cur_lev, itype, all_num_f, item.quantity);
    _record_item_stat(cur_lev, itype, "NumForIter", item.quantity);
}

static void _record_monster_stat(const level_id &lev, int mons_ind, string field,
                                 double value)
{
    const level_id br_lev(lev.branch, -1);
    const int sum_ind = valid_monsters[NUM_MONSTERS];

    monster_recs[lev][mons_ind][field] += value;
    monster_recs[lev][sum_ind][field] += value;
    monster_recs[br_lev][mons_ind][field] += value;
    monster_recs[br_lev][sum_ind][field] += value;
    monster_recs[all_lev][mons_ind][field] += value;
    monster_recs[all_lev][sum_ind][field] += value;
}

void objstat_record_monster(const monster *mons)
{
    monster_type type;
    bool from_vault = !mons->originating_map().empty();

    if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        type = MONS_GLOWING_SHAPESHIFTER;
    else if (mons->has_ench(ENCH_SHAPESHIFTER))
        type = MONS_SHAPESHIFTER;
    else
        type = mons->type;

    if (!valid_monsters.count(type))
        return;

    int mons_ind = valid_monsters[type];
    level_id lev = level_id::current();

    _record_monster_stat(lev, mons_ind, "Num", 1);
    if (from_vault)
        _record_monster_stat(lev, mons_ind, "NumVault", 1);
    else
        _record_monster_stat(lev, mons_ind, "NumNonVault", 1);
    _record_monster_stat(lev, mons_ind, "NumForIter", 1);
    _record_monster_stat(lev, mons_ind, "MonsXP", exper_value(*mons));
    _record_monster_stat(lev, mons_ind, "TotalXP", exper_value(*mons));
    if (from_vault)
    {
        _record_monster_stat(lev, mons_ind, "TotalVaultXP",
                exper_value(*mons));
    }
    else
    {
        _record_monster_stat(lev, mons_ind, "TotalNonVaultXP",
                exper_value(*mons));
    }
    _record_monster_stat(lev, mons_ind, "MonsHP", mons->max_hit_points);
    _record_monster_stat(lev, mons_ind, "MonsHD", mons->get_experience_level());

    const corpse_effect_type chunk_effect = mons_corpse_effect(type);
    // Record chunks/nutrition if monster leaves a corpse.
    if (chunk_effect != CE_NOCORPSE && mons_class_can_leave_corpse(type))
    {
        // copied from turn_corpse_into_chunks()
        double chunks = (1 + stepdown_value(max_corpse_chunks(type),
                                            4, 4, 12, 12)) / 2.0;
        item_def chunk_item = _dummy_item(item_type(ITEM_FOOD, FOOD_CHUNK));

        you.mutation[MUT_CARNIVOROUS] = 3;
        int carn_value = food_value(chunk_item);
        you.mutation[MUT_CARNIVOROUS] = 0;

        _record_monster_stat(lev, mons_ind, "MonsNumChunks", chunks);
        if (chunk_effect == CE_MUTAGEN)
            _record_monster_stat(lev, mons_ind, "MonsNumMutChunks", chunks);

        if (chunk_effect == CE_CLEAN)
        {
            _record_monster_stat(lev, mons_ind, "TotalNutr",
                                 chunks * food_value(chunk_item));
            _record_monster_stat(lev, mons_ind, "TotalCarnNutr",
                                 chunks * carn_value);
        }
        _record_monster_stat(lev, mons_ind, "TotalGhoulNutr",
                             chunks * carn_value);
    }
}

void objstat_iteration_stats()
{
    for (const auto &entry : stat_branches)
    {
        for (unsigned int l = 0; l <= entry.second.size(); l++)
        {
            level_id lev;
            if (l == entry.second.size())
            {
                if (entry.first == NUM_BRANCHES)
                    continue;
                lev.branch = entry.first;
                lev.depth = -1;
            }
            else
                lev = (entry.second)[l];

            for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
            {
                item_base_type base_type = static_cast<item_base_type>(i);
                int num_entries = _item_max_sub_type(base_type);
                num_entries = num_entries == 1 ? 1 : num_entries + 1;
                for (int  j = 0; j < num_entries ; j++)
                {
                    bool use_all = _item_has_antiquity(base_type);
                    string min_f = use_all ? "AllNumMin" : "NumMin";
                    string max_f = use_all ? "AllNumMax" : "NumMax";
                    string sd_f = use_all ? "AllNumSD" : "NumSD";
                    map<string, double> &stats = item_recs[lev][i][j];
                    if (stats["NumForIter"] > stats[max_f])
                        stats[max_f] = stats["NumForIter"];
                    if (stats["NumForIter"] < stats[min_f])
                        stats[min_f] = stats["NumForIter"];
                    stats[sd_f] += stats["NumForIter"] * stats["NumForIter"];
                    stats["NumForIter"] = 0;
                }
            }

            for (const auto &mentry : valid_monsters)
            {
                map<string, double> &stats = monster_recs[lev][mentry.second];
                if (stats["NumForIter"] > stats["NumMax"])
                    stats["NumMax"] = stats["NumForIter"];
                if (stats["NumForIter"] < stats["NumMin"])
                    stats["NumMin"] = stats["NumForIter"];
                stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];
                stats["NumForIter"] = 0;
            }
        }
    }
}

static void _write_stat_headers(const vector<string> &fields, bool items = true)
{
    fprintf(stat_outf, "%s\tLevel", items ? "Item" : "Monster");
    for (const string &field : fields)
        fprintf(stat_outf, "\t%s", field.c_str());
    fprintf(stat_outf, "\n");
}

static void _write_stat(map<string, double> &stats, string field)
{
    ostringstream output;
    double value = 0;

    output.precision(STAT_PRECISION);
    output.setf(ios_base::fixed);
    if (field == "PileQuant")
        value = stats["Num"] / stats["NumPiles"];
    else if (field == "WandCharges"
             || field == "MiscPlus"
             || field == "MonsHD"
             || field == "MonsHP"
             || field == "MonsXP"
             || field == "MonsNumChunks"
             || field == "MonsNumMutChunks")
    {
        value = stats[field] / stats["Num"];
    }
    else if (field == "AllEnch")
        value = stats[field] / stats["AllNum"];
    else if (field == "ArteEnch")
        value = stats[field] / stats["ArteNum"];
    else if (field == "OrdEnch")
        value = stats[field] / stats["OrdNum"];
    else if (field == "NumSD" || field == "AllNumSD")
    {
        const string num_f = field == "NumSD" ? "Num" : "AllNum";
        if (SysEnv.map_gen_iters == 1)
            value = 0;
        else
        {
            const double mean = stats[num_f] / SysEnv.map_gen_iters;
            value = sqrt((SysEnv.map_gen_iters / (SysEnv.map_gen_iters - 1.0))
                         * (stats[field] / SysEnv.map_gen_iters - mean * mean));
        }
    }
    else if (field == "NumMin" || field == "AllNumMin" || field == "NumMax"
             || field == "AllNumMax")
    {
        value = stats[field];
    }
    else
        value = stats[field] / SysEnv.map_gen_iters;
    output << "\t" << value;
    fprintf(stat_outf, "%s", output.str().c_str());
}

static string _brand_name(const item_type &item, int brand)
{
    string brand_name = "";
    item_def dummy_item = _dummy_item(item);

    if (!brand)
        brand_name = "none";
    else
    {
        dummy_item.brand = brand;
        switch (item.base_type)
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
    }
    return brand_name;
}

static string _item_name(const item_type &item)
{
    string name = "";

    if (item.sub_type == _item_max_sub_type(item.base_type))
        name = "All " + _item_class_name(item.base_type);
    else if (item.base_type == ITEM_MANUALS)
        name = "Manual";
    else if (item.base_type == ITEM_ARTEBOOKS)
    {
        int orig_type = _item_orig_sub_type(item);
        if (orig_type == BOOK_RANDART_LEVEL)
            name = "Level Artefact Book";
        else
            name = "Theme Artefact Book";
    }
    else
    {
        item_def dummy_item = _dummy_item(item);
        name = dummy_item.name(DESC_DBNAME, true, true);
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

static void _write_brand_stats(const vector<int> &brand_stats,
                               const item_type &item)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    const item_def dummy_item = _dummy_item(item);
    const unsigned int num_brands = brand_stats.size();
    bool first_brand = true;
    ostringstream brand_summary;

    brand_summary.setf(ios_base::fixed);
    brand_summary.precision(STAT_PRECISION);
    for (unsigned int i = 0; i < num_brands; i++)
    {
        if (brand_stats[i] == 0)
            continue;

        string brand_name = "";
        const double value = (double) brand_stats[i] / SysEnv.map_gen_iters;

        if (first_brand)
            first_brand = false;
        else
            brand_summary << ";";
        brand_name = _brand_name(item, i);
        brand_summary << brand_name.c_str() << ":" << value;
    }
    fprintf(stat_outf, "\t%s", brand_summary.str().c_str());
}

static void _write_level_brand_stats(const level_id &lev, const item_type &item)
{
    if (item.base_type == ITEM_WEAPONS)
    {
        for (int j = 0; j < 3; j++)
            _write_brand_stats(weapon_brands[lev][item.sub_type][j], item);
    }
    else if (item.base_type == ITEM_ARMOUR)
    {
        for (int j = 0; j < 3; j++)
            _write_brand_stats(armour_brands[lev][item.sub_type][j], item);
    }
    else if (item.base_type == ITEM_MISSILES)
        _write_brand_stats(missile_brands[lev][item.sub_type], item);
}

static void _write_branch_item_stats(branch_type br, const item_type &item)
{
    unsigned int level_count = 0;
    const vector<string> &fields = item_fields[item.base_type];
    vector<level_id>::const_iterator li;
    const string name = _item_name(item);
    const char *num_field = _item_has_antiquity(item.base_type) ? "AllNum"
        : "Num";
    const level_id br_lev(br, -1);

    for (level_id lid : stat_branches[br])
    {
        ++level_count;
        if (item_recs[lid][item.base_type][item.sub_type][num_field] < 1)
            continue;
        fprintf(stat_outf, "%s\t%s", name.c_str(), _level_name(lid).c_str());
        map <string, double> &item_stats =
            item_recs[lid][item.base_type][item.sub_type];
        for (const string &field : fields)
            _write_stat(item_stats, field);
        _write_level_brand_stats(lid, item);
        fprintf(stat_outf, "\n");
    }
    if (level_count > 1
        && item_recs[br_lev][item.base_type][item.sub_type][num_field] > 0)
    {
        map <string, double> &branch_stats =
            item_recs[br_lev][item.base_type][item.sub_type];
        fprintf(stat_outf, "%s\t%s", name.c_str(), _level_name(br_lev).c_str());
        for (const string &field : fields)
            _write_stat(branch_stats, field);
        _write_level_brand_stats(br_lev, item);
        fprintf(stat_outf, "\n");
    }
}

static void _write_branch_monster_stats(branch_type br, monster_type mons_type,
                                        int mons_ind)
{
    unsigned int level_count = 0;
    const vector<string> &fields = monster_fields;
    string mons_name;
    const level_id br_lev(br, -1);

    if (mons_ind == valid_monsters[NUM_MONSTERS])
        mons_name = "All Monsters";
    else
        mons_name = mons_type_name(mons_type, DESC_PLAIN);
    for (level_id lid : stat_branches[br])
    {
        ++level_count;
        if (monster_recs[lid][mons_ind]["Num"] < 1)
            continue;
        fprintf(stat_outf, "%s\t%s", mons_name.c_str(),
                _level_name(lid).c_str());
        for (const string &field : fields)
            _write_stat(monster_recs[lid][mons_ind], field);
        fprintf(stat_outf, "\n");
    }
    // If there are multiple levels for this branch, print a branch summary.
    if (level_count > 1 && monster_recs[br_lev][mons_ind]["Num"] > 0)
    {
        fprintf(stat_outf, "%s\t%s", mons_name.c_str(),
                _level_name(br_lev).c_str());
        for (const string &field : fields)
            _write_stat(monster_recs[br_lev][mons_ind], field);
        fprintf(stat_outf, "\n");
    }
}

static FILE * _open_stat_file(string stat_file)
{
    FILE *stat_fh = nullptr;
    stat_fh = fopen(stat_file.c_str(), "w");
    if (!stat_fh)
    {
        fprintf(stderr, "Unable to open objstat output file: %s\n"
                "Error: %s", stat_file.c_str(), strerror(errno));
        end(1);
    }
    return stat_fh;
}

static void _write_item_stats(item_base_type base_type)
{
    ostringstream out_file;
    out_file << stat_out_prefix << _item_class_name(base_type).c_str()
             << stat_out_ext;
    stat_outf = _open_stat_file(out_file.str());
    const int num_types = _item_max_sub_type(base_type);
    vector<string> fields = item_fields[base_type];
    stat_outf = _open_stat_file(out_file.str());
    if (base_type == ITEM_WEAPONS || base_type == ITEM_ARMOUR)
    {
        for (int j = 0; j < 3; j++)
            fields.push_back(equip_brand_fields[j]);
    }
    else if (base_type == ITEM_MISSILES)
        fields.push_back(missile_brand_field);

    _write_stat_headers(fields);
    // If there is more than one subtype, we have an additional entry for
    // the sum across subtypes.
    int num_entries = num_types == 1 ? 1 : num_types + 1;
    for (int j = 0; j < num_entries; j++)
    {
        item_type item(base_type, j);
        for (const auto &br : stat_branches)
            _write_branch_item_stats(br.first, item);
    }
    fclose(stat_outf);
    printf("Wrote %s item stats to %s.\n", _item_class_name(base_type).c_str(),
           out_file.str().c_str());
}

static void _write_stat_info()
{
    string all_desc;
    if (num_branches > 1)
    {
        if (SysEnv.map_gen_range.get())
            all_desc = SysEnv.map_gen_range.get()->describe();
        else
            all_desc = "All Levels";
        all_desc = "Levels included in AllLevels: " + all_desc + "\n";
    }
    ostringstream out_file;
    out_file << stat_out_prefix << "Info" << stat_out_ext;
    stat_outf = _open_stat_file(out_file.str());
    fprintf(stat_outf, "Object Generation Stats\n"
            "Number of iterations: %d\n"
            "Number of branches: %d\n"
            "%s"
            "Number of levels: %d\n"
            "Version: %s\n", SysEnv.map_gen_iters, num_branches,
            all_desc.c_str(), num_levels, Version::Long);
    fclose(stat_outf);
    printf("Wrote Objstat Info to %s.\n", out_file.str().c_str());
}

static void _write_object_stats()
{
    string all_desc = "";
    _write_stat_info();
    for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
    {
        item_base_type base_type = static_cast<item_base_type>(i);
        _write_item_stats(base_type);
    }
    ostringstream out_file;
    out_file << stat_out_prefix << "Monsters" << stat_out_ext;
    stat_outf = _open_stat_file(out_file.str());
    _write_stat_headers(monster_fields, false);
    for (const auto &entry : valid_monsters)
        for (const auto &br : stat_branches)
            _write_branch_monster_stats(br.first, entry.first, entry.second);
    printf("Wrote Monster stats to %s.\n", out_file.str().c_str());
    fclose(stat_outf);

}

void objstat_generate_stats()
{
    // Warn assertions about possible oddities like the artefact list being
    // cleared.
    you.wizard = true;
    // Let "acquire foo" have skill aptitudes to work with.
    you.species = SP_HUMAN;

    initialise_item_descriptions();
    initialise_branch_depths();
    // We have to run map preludes ourselves.
    run_map_global_preludes();
    run_map_local_preludes();

    // Populate a vector of the levels ids we've made
    // This represents the AllLevels summary.
    stat_branches[NUM_BRANCHES] = { level_id(NUM_BRANCHES, -1) };
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
        for (int dep = 1; dep <= brdepth[br]; ++dep)
        {
            const level_id lid(br, dep);
            if (SysEnv.map_gen_range.get()
                && !SysEnv.map_gen_range->is_usable_in(lid))
            {
                continue;
            }
            levels.push_back(lid);
            ++num_levels;
        }
        if (levels.size())
        {
            stat_branches[br] = levels;
            ++num_branches;
        }
    }
    printf("Generating object statistics for %d iteration(s) of %d "
           "level(s) over %d branch(es).\n", SysEnv.map_gen_iters,
           num_levels, num_branches);
    _init_monsters();
    _init_stats();
    if (mapstat_build_levels())
    {
        _write_object_stats();
        printf("Object statistics complete.\n");
    }
}
#endif // DEBUG_STATISTICS

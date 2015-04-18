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
#include "decks.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "godabil.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
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
    ITEM_RODS,
    ITEM_DECKS,
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
    item_type(item_def &item);

    item_base_type base_type;
    int sub_type;
};

static level_id all_lev(NUM_BRANCHES, -1);
static map<branch_type, vector<level_id> > stat_branches;
static int num_branches = 0;
static int num_levels = 0;

// item_recs[level_id][item.base_type][item.sub_type][field]
static map<level_id, FixedVector<map<int, map<string, double> >, NUM_ITEM_BASE_TYPES> > item_recs;

// weapon_brands[level_id][item.base_type][item.sub_type][antiquity_level][brand];
// arte_sum is 0 for ordinary, 1 for artefact, or 2 for all
static map<level_id, vector <vector< vector< vector< int> > > > > equip_brands;
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
    { // ITEM_RODS
        "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
        "RodMana", "RodRecharge", "NumCursed"
    },
    { // ITEM_DECKS
        "PlainNum", "OrnateNum", "LegendaryNum", "AllNum",
        "AllNumMin", "AllNumMax", "AllNumSD", "AllDeckCards"
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

static map<int, int> valid_foods;

static const vector<string> monster_fields = {
    "Num", "NumMin", "NumMax", "NumSD", "MonsHD", "MonsHP",
    "MonsXP", "TotalXP", "MonsNumChunks", "TotalNutr"
};

static map<monster_type, int> valid_monsters;
static map<level_id, map<int, map <string, double> > > monster_recs;

static bool _is_valid_food_type(int sub_type)
{
    return sub_type != FOOD_CHUNK && food_type_name(sub_type) != "buggy";
}

static void _init_foods()
{
    int num_foods = 0;
    for (int i = 0; i < NUM_FOODS; i++)
    {
        if (_is_valid_food_type(i))
            valid_foods[i] = num_foods++;
    }
}

static void _init_monsters()
{
    int num_mons = 0;
    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        monster_type mc = static_cast<monster_type>(i);
        if (!mons_class_flag(mc, M_NO_EXP_GAIN)
            && !mons_class_flag(mc, M_CANT_SPAWN))
        {
            valid_monsters[mc] = num_mons++;
        }
    }
    // For the all-monster summary
    valid_monsters[NUM_MONSTERS] = num_mons;
}

static item_base_type _item_base_type(item_def &item)
{
    item_base_type type;
    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
        type = is_deck(item) ? ITEM_DECKS : ITEM_MISCELLANY;
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
        if (valid_foods.count(item.sub_type))
            type = ITEM_FOOD;
        else
            type = ITEM_IGNORE;
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
    case OBJ_RODS:
        type = ITEM_RODS;
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
    case ITEM_RODS:
        type = OBJ_RODS;
        break;
    case ITEM_DECKS:
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

// Get the actual food subtype
static int _orig_food_subtype(int sub_type)
{
    for (const auto &entry : valid_foods)
        if (entry.second == sub_type)
            return entry.first;

    die("Invalid food subtype");
    return 0;
}

static string _item_class_name(item_base_type base_type)
{
    string name;
    switch (base_type)
    {
    case ITEM_DECKS:
        name = "Decks";
        break;
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

static int _item_orig_sub_type(item_type &item)
{
    int type;
    switch (item.base_type)
    {
    case ITEM_DECKS:
        type = deck_types[item.sub_type];
        break;
    case ITEM_MISCELLANY:
        type = misc_types[item.sub_type];
        break;
    case ITEM_FOOD:
        type = _orig_food_subtype(item.sub_type);
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
    case ITEM_FOOD:
        num = valid_foods.size();
        break;
    case ITEM_MISCELLANY:
        num = misc_types.size();
        break;
    case ITEM_DECKS:
        num = deck_types.size();
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

static item_def _dummy_item(item_type &item)
{
    item_def dummy_item;
    dummy_item.base_type = _item_orig_base_type(item.base_type);
    dummy_item.sub_type = _item_orig_sub_type(item);
    // Deck name is reported as buggy if this is not done.
    if (item.base_type == ITEM_DECKS)
    {
        dummy_item.plus = 1;
        dummy_item.special  = DECK_RARITY_COMMON;
        init_deck(dummy_item);
    }
    return dummy_item;
}

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

item_type::item_type(item_def &item)
{
    base_type = _item_base_type(item);
    if (base_type == ITEM_DECKS)
    {
        sub_type = find(deck_types.begin(), deck_types.end(), item.sub_type)
                   - deck_types.begin();
        ASSERT(sub_type < (int) deck_types.size());
    }
    else if (base_type == ITEM_MISCELLANY)
    {
        sub_type = find(misc_types.begin(), misc_types.end(), item.sub_type)
                        - misc_types.begin();
        ASSERT(sub_type < (int) misc_types.size());
    }
    else if (base_type == ITEM_FOOD)
        sub_type = valid_foods[item.sub_type];
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
            for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
            {
                item_base_type base_type = static_cast<item_base_type>(i);
                string min_field = _item_has_antiquity(base_type)
                    ? "AllNumMin" : "NumMin";
                string max_field = _item_has_antiquity(base_type)
                    ? "AllNumMax" : "NumMax";
                for (int  j = 0; j <= _item_max_sub_type(base_type); j++)
                {
                    for (const string &field : item_fields[i])
                        item_recs[lev][i][j][field] = 0;
                    // For determining the NumSD, NumMin and NumMax fields.
                    item_recs[lev][i][j]["NumForIter"] = 0;
                    item_recs[lev][i][j][min_field] = INFINITY;
                    item_recs[lev][i][j][max_field] = -1;
                }
            }
            equip_brands[lev] = { };
            equip_brands[lev].emplace_back();
            for (int i = 0; i <= NUM_WEAPONS; i++)
            {
                equip_brands[lev][0].emplace_back(3,
                        vector<int>(NUM_SPECIAL_WEAPONS, 0));
            }

            equip_brands[lev].emplace_back();
            for (int i = 0; i <= NUM_ARMOURS; i++)
            {
                equip_brands[lev][1].emplace_back(3,
                        vector<int>(NUM_SPECIAL_ARMOURS, 0));
            }

            missile_brands[lev].emplace_back();
            for (int i = 0; i <= NUM_MISSILES; i++)
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

static void _record_item_stat(level_id &lev, item_type &item, string field,
                          double value)
{
    int class_sum = _item_max_sub_type(item.base_type);
    level_id br_lev(lev.branch, -1);

    item_recs[lev][item.base_type][item.sub_type][field] += value;
    item_recs[lev][item.base_type][class_sum][field] += value;
    item_recs[br_lev][item.base_type][item.sub_type][field] += value;
    item_recs[br_lev][item.base_type][class_sum][field] += value;
    item_recs[all_lev][item.base_type][item.sub_type][field] += value;
    item_recs[all_lev][item.base_type][class_sum][field] += value;
}

static void _record_brand(level_id &lev, item_type &item, int quantity,
                             bool is_arte, int brand)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    int cst = _item_max_sub_type(item.base_type);
    int antiq = is_arte ? ANTIQ_ARTEFACT : ANTIQ_ORDINARY;
    bool is_equip = item.base_type == ITEM_WEAPONS
        || item.base_type == ITEM_ARMOUR;
    int bt = item.base_type == ITEM_WEAPONS ? 0 : 1;
    int st = item.sub_type;
    level_id br_lev(lev.branch, -1);

    if (is_equip)
    {
        equip_brands[lev][bt][st][antiq][brand] += quantity;
        equip_brands[lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[lev][bt][cst][antiq][brand] += quantity;
        equip_brands[lev][bt][cst][ANTIQ_ALL][brand] += quantity;

        equip_brands[br_lev][bt][st][antiq][brand] += quantity;
        equip_brands[br_lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[br_lev][bt][cst][antiq][brand] += quantity;
        equip_brands[br_lev][bt][cst][ANTIQ_ALL][brand] += quantity;

        equip_brands[all_lev][bt][st][antiq][brand] += quantity;
        equip_brands[all_lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[all_lev][bt][cst][antiq][brand] += quantity;
        equip_brands[all_lev][bt][cst][ANTIQ_ALL][brand] += quantity;
    }
    else
    {
        missile_brands[lev][st][brand] += quantity;
        missile_brands[lev][cst][brand] += quantity;
        missile_brands[br_lev][st][brand] += quantity;
        missile_brands[br_lev][cst][brand] += quantity;
        missile_brands[all_lev][st][brand] += quantity;
        missile_brands[all_lev][cst][brand] += quantity;
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
    case ITEM_RODS:
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
    case ITEM_DECKS:
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
    case ITEM_RODS:
    case ITEM_MISSILES:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

void objstat_record_item(item_def &item)
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
    case ITEM_RODS:
        _record_item_stat(cur_lev, itype, "RodMana",
                             item.charge_cap / ROD_CHARGE_MULT);
        _record_item_stat(cur_lev, itype, "RodRecharge", item.special);
        break;
    case ITEM_MISCELLANY:
        all_plus_f = "MiscPlus";
        break;
    case ITEM_DECKS:
        switch (item.deck_rarity)
        {
        case DECK_RARITY_COMMON:
            _record_item_stat(cur_lev, itype, "PlainNum", 1);
            break;
        case DECK_RARITY_RARE:
            _record_item_stat(cur_lev, itype, "OrnateNum", 1);
            break;
        case DECK_RARITY_LEGENDARY:
            _record_item_stat(cur_lev, itype, "LegendaryNum", 1);
            break;
        default:
            break;
        }
        all_plus_f = "AllDeckCards";
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

static void _record_monster_stat(level_id &lev, int mons_ind, string field,
                                 double value)
{
    level_id br_lev(lev.branch, -1);
    int sum_ind = valid_monsters[NUM_MONSTERS];

    monster_recs[lev][mons_ind][field] += value;
    monster_recs[lev][sum_ind][field] += value;
    monster_recs[br_lev][mons_ind][field] += value;
    monster_recs[br_lev][sum_ind][field] += value;
    monster_recs[all_lev][mons_ind][field] += value;
    monster_recs[all_lev][sum_ind][field] += value;
}

void objstat_record_monster(monster *mons)
{
    monster_type type;
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
    _record_monster_stat(lev, mons_ind, "NumForIter", 1);
    _record_monster_stat(lev, mons_ind, "MonsXP", exper_value(mons));
    _record_monster_stat(lev, mons_ind, "TotalXP", exper_value(mons));
    _record_monster_stat(lev, mons_ind, "MonsHP", mons->max_hit_points);
    _record_monster_stat(lev, mons_ind, "MonsHD", mons->get_experience_level());

    const corpse_effect_type chunk_effect = mons_corpse_effect(type);
    // Record chunks/nutrition if monster leaves a corpse.
    if (chunk_effect != CE_NOCORPSE && mons_class_can_leave_corpse(type))
    {
        // copied from turn_corpse_into_chunks()
        double chunks = (1 + stepdown_value(max_corpse_chunks(type),
                                            4, 4, 12, 12)) / 2.0;
        _record_monster_stat(lev, mons_ind, "MonsNumChunks", chunks);
        if (chunk_effect == CE_CLEAN)
        {
            _record_monster_stat(lev, mons_ind, "TotalNutr",
                                    chunks * CHUNK_BASE_NUTRITION);
        }
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
                for (int  j = 0; j <= _item_max_sub_type(base_type); j++)
                {
                    bool use_all = _item_has_antiquity(base_type)
                        || base_type == ITEM_DECKS;
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

static void _write_level_headers(branch_type br, int num_fields)
{
    unsigned int level_count = 0;
    vector<level_id> &levels = stat_branches[br];

    fprintf(stat_outf, "Place");
    for (level_id lid : levels)
    {
        if (br == NUM_BRANCHES)
            fprintf(stat_outf, "\tAllLevels");
        else
            fprintf(stat_outf, "\t%s", lid.describe().c_str());

        for (int i = 0; i < num_fields - 1; i++)
            fprintf(stat_outf, "\t");
        if (++level_count == levels.size() && level_count > 1)
        {
            fprintf(stat_outf, "\t%s", branches[lid.branch].abbrevname);
            for (int i = 0; i < num_fields - 1; i++)
                fprintf(stat_outf, "\t");
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_stat_headers(branch_type br, const vector<string> &fields)
{
    unsigned int level_count = 0;
    vector<level_id> &levels = stat_branches[br];

    fprintf(stat_outf, "Property");
    for (level_id lid : levels)
    {
        UNUSED(lid);
        for (const string &field : fields)
            fprintf(stat_outf, "\t%s", field.c_str());

        if (++level_count == levels.size() && level_count > 1)
        {
            for (const string &field : fields)
                fprintf(stat_outf, "\t%s", field.c_str());
        }
    }
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
             || field == "RodMana"
             || field == "RodRecharge"
             || field == "MiscPlus"
             || field == "MonsHD"
             || field == "MonsHP"
             || field == "MonsXP"
             || field == "MonsNumChunks")
    {
        value = stats[field] / stats["Num"];
    }
    else if (field == "AllEnch" || field == "AllDeckCards")
        value = stats[field] / stats["AllNum"];
    else if (field == "ArteEnch")
        value = stats[field] / stats["ArteNum"];
    else if (field == "OrdEnch")
        value = stats[field] / stats["OrdNum"];
    else if (field == "NumSD" || field == "AllNumSD")
    {
        string num_f = field == "NumSD" ? "Num" : "AllNum";
        if (SysEnv.map_gen_iters == 1)
            value = 0;
        else
        {
            double mean = stats[num_f] / SysEnv.map_gen_iters;
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

static string _brand_name(item_type &item, int brand)
 {
     string brand_name = "";
     item_def dummy_item = _dummy_item(item);

     if (!brand)
            brand_name = "none";
     else
     {
         dummy_item.special = brand;
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

static void _write_brand_stats(vector<int> &brand_stats, item_type &item)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    item_def dummy_item = _dummy_item(item);
    unsigned int num_brands = brand_stats.size();
    bool first_brand = true;
    ostringstream brand_summary;

    brand_summary.setf(ios_base::fixed);
    brand_summary.precision(STAT_PRECISION);
    for (unsigned int i = 0; i < num_brands; i++)
    {
        string brand_name = "";
        double value = (double) brand_stats[i] / SysEnv.map_gen_iters;
        if (brand_stats[i] == 0)
            continue;

        if (first_brand)
            first_brand = false;
        else
            brand_summary << ",";

        brand_name = _brand_name(item, i);
        brand_summary << brand_name.c_str() << "(" << value << ")";
    }
    fprintf(stat_outf, "\t%s", brand_summary.str().c_str());
}

static string _item_name(item_type &item)
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

static void _write_item_stats(branch_type br, item_type &item)
{
    bool is_brand_equip = item.base_type == ITEM_WEAPONS
        || item.base_type == ITEM_ARMOUR;
    int equip_ind = is_brand_equip
        ? (item.base_type == ITEM_WEAPONS ? 0 : 1) : -1;
    unsigned int level_count = 0;
    const vector<string> &fields = item_fields[item.base_type];
    vector<level_id>::const_iterator li;

    fprintf(stat_outf, "%s", _item_name(item).c_str());
    for (level_id lid : stat_branches[br])
    {
        map <string, double> &item_stats =
            item_recs[lid][item.base_type][item.sub_type];
        for (const string &field : fields)
            _write_stat(item_stats, field);

        if (is_brand_equip)
        {
            vector< vector<int> > &brand_stats =
                equip_brands[lid][equip_ind][item.sub_type];
            for (int j = 0; j < 3; j++)
                _write_brand_stats(brand_stats[j], item);
        }
        else if (item.base_type == ITEM_MISSILES)
            _write_brand_stats(missile_brands[lid][item.sub_type], item);

        if (++level_count == stat_branches[lid.branch].size() && level_count > 1)
        {
            level_id br_lev(lid.branch, -1);
            map <string, double> &branch_stats =
                item_recs[br_lev][item.base_type][item.sub_type];

            for (const string &field : fields)
                _write_stat(branch_stats, field);

            if (is_brand_equip)
            {
                vector< vector<int> > &branch_brand_stats =
                    equip_brands[br_lev][equip_ind][item.sub_type];
                for (int j = 0; j < 3; j++)
                    _write_brand_stats(branch_brand_stats[j], item);
            }
            else if (item.base_type == ITEM_MISSILES)
            {
                _write_brand_stats(missile_brands[br_lev][item.sub_type],
                                      item);
            }
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_monster_stats(branch_type br, monster_type mons_type,
                                    int mons_ind)
{
    unsigned int level_count = 0;
    const vector<string> &fields = monster_fields;

    if (mons_ind == valid_monsters[NUM_MONSTERS])
        fprintf(stat_outf, "All Monsters");
    else
        fprintf(stat_outf, "%s", mons_type_name(mons_type, DESC_PLAIN).c_str());
    for (level_id lid : stat_branches[br])
    {
        for (const string &field : fields)
            _write_stat(monster_recs[lid][mons_ind], field);

        if (++level_count == stat_branches[lid.branch].size() && level_count > 1)
        {
            level_id br_lev(lid.branch, -1);
            for (const string &field : fields)
                _write_stat(monster_recs[br_lev][mons_ind], field);
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_branch_stats(branch_type br)
{
    fprintf(stat_outf, "\n\nItem Generation Stats:");
    for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
    {
        item_base_type base_type = static_cast<item_base_type>(i);
        int num_types = _item_max_sub_type(base_type);
        vector<string> fields = item_fields[base_type];

        fprintf(stat_outf, "\n%s\n", _item_class_name(base_type).c_str());
        if (base_type == ITEM_WEAPONS || base_type == ITEM_ARMOUR)
        {
            for (int j = 0; j < 3; j++)
                fields.push_back(equip_brand_fields[j]);
        }
        else if (base_type == ITEM_MISSILES)
            fields.push_back(missile_brand_field);

        _write_level_headers(br, fields.size());
        _write_stat_headers(br, fields);
        for (int j = 0; j <= num_types; j++)
        {
            item_type item(base_type, j);
            _write_item_stats(br, item);
        }
    }
    fprintf(stat_outf, "\n\nMonster Generation Stats:\n");
    _write_level_headers(br, monster_fields.size());
    _write_stat_headers(br, monster_fields);

    for (const auto &entry : valid_monsters)
        _write_monster_stats(br, entry.first, entry.second);
}

static void _write_object_stats()
{
    string all_desc = "";

    for (const auto &entry : stat_branches)
    {
        string branch_name;
        if (entry.first == NUM_BRANCHES)
        {
            if (num_branches == 1)
                continue;
            branch_name = "AllLevels";
            if (SysEnv.map_gen_range.get())
                all_desc = SysEnv.map_gen_range.get()->describe();
            else
                all_desc = "All Branches";
            all_desc = "Levels included in AllLevels: " + all_desc + "\n";
        }
        else
            branch_name = branches[entry.first].abbrevname;
        ostringstream out_file;
        out_file << stat_out_prefix << branch_name << stat_out_ext;
        stat_outf = fopen(out_file.str().c_str(), "w");
        if (!stat_outf)
        {
            fprintf(stderr, "Unable to open objstat output file: %s\n"
                    "Error: %s", out_file.str().c_str(), strerror(errno));
            end(1);
        }
        fprintf(stat_outf, "Object Generation Stats\n"
                "Number of iterations: %d\n"
                "Number of branches: %d\n"
                "%s"
                "Number of levels: %d\n"
                "Version: %s\n", SysEnv.map_gen_iters, num_branches,
                all_desc.c_str(), num_levels, Version::Long);
        _write_branch_stats(entry.first);
        fclose(stat_outf);
        printf("Wrote statistics for branch %s to %s.\n", branch_name.c_str(),
               out_file.str().c_str());
    }
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
    // This represents the AllLevels summary.
    stat_branches[NUM_BRANCHES] = { level_id(NUM_BRANCHES, -1) };
    printf("Generating object statistics for %d iteration(s) of %d "
           "level(s) over %d branch(es).\n", SysEnv.map_gen_iters,
           num_levels, num_branches);
    _init_foods();
    _init_monsters();
    _init_stats();
    if (mapstat_build_levels())
    {
        _write_object_stats();
        printf("Object statistics complete.\n");
    }
}
#endif // DEBUG_STATISTICS

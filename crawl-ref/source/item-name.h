/**
 * @file
 * @brief Misc functions.
**/

#pragma once

#include <vector>

#include "item-prop-enum.h"
#include "mon-flags.h"
#include "random.h"
#include "tag-version.h"

using std::vector;

#define CORPSE_NAME_KEY      "corpse_name_key"
#define CORPSE_NAME_TYPE_KEY "corpse_name_type_key"
#define IDENTIFIED_ALL_KEY   "identified_all_key"

struct item_kind
{
    object_class_type base_type;
    uint8_t           sub_type;
    int8_t            plus, plus2;
};

// [dshaligram] If you edit potion colours/descriptions, also update
// item-name.cc.
enum potion_description_colour_type
{
#if TAG_MAJOR_VERSION == 34
    PDC_CLEAR,
#endif
    PDC_BLUE,
    PDC_BLACK,
    PDC_SILVERY,
    PDC_CYAN,
    PDC_PURPLE,
    PDC_ORANGE,
    PDC_INKY,
    PDC_RED,
    PDC_YELLOW,
    PDC_GREEN,
    PDC_BROWN,
    PDC_RUBY,
    PDC_WHITE,
    PDC_EMERALD,
    PDC_GREY,
    PDC_PINK,
    PDC_COPPERY,
    PDC_GOLDEN,
    PDC_DARK,
    PDC_PUCE,
    PDC_AMETHYST,
    PDC_SAPPHIRE,
    PDC_NCOLOURS
};

// [dshaligram] If you edit potion colours/descriptions, also update
// item-name.cc.
enum potion_description_qualifier_type
{
    PDQ_NONE,
    PDQ_BUBBLING,
    PDQ_FUMING,
    PDQ_FIZZY,
    PDQ_VISCOUS,
    PDQ_LUMPY,
    PDQ_SMOKY,
    PDQ_GLOWING,
    PDQ_SEDIMENTED,
    PDQ_METALLIC,
    PDQ_MURKY,
    PDQ_GLUGGY,
    PDQ_OILY,
    PDQ_SLIMY,
    PDQ_EMULSIFIED,
    PDQ_NQUALS
};

// Primary and secondary description counts for some types of items.
// If you change these counts, update item-name.cc.
enum
{
    NDSC_JEWEL_PRI  = 29,
    NDSC_JEWEL_SEC  = 13,
    NDSC_STAVE_PRI = 4,
    NDSC_STAVE_SEC = 10,
    NDSC_WAND_PRI  = 12,
    NDSC_WAND_SEC  = 16,
    NDSC_POT_PRI   = PDC_NCOLOURS,
    NDSC_POT_SEC   = PDQ_NQUALS,
    NDSC_BOOK_PRI  = 5,
};

enum mbn_type
{
    MBN_TERSE, // terse brand name
    MBN_NAME,  // brand name for item naming (adj for prefix, noun for postfix)
    MBN_BRAND, // plain brand name
};

/// What kind of special behaviour should make_name use?
enum makename_type
{
    MNAME_DEFAULT, /// No special behaviour.
    MNAME_SCROLL, /// Allcaps, longer.
    MNAME_JIYVA, /// No spaces, starts with J, Plog -> Jiyva
};

void display_runes();

string quant_name(const item_def &item, int quant,
                  description_level_type des, bool terse = false);

bool item_type_known(const item_def &item);
bool item_type_known(const object_class_type base_type, const int sub_type);

bool is_interesting_item(const item_def& item);
bool is_emergency_item(const item_def& item);
bool is_good_item(const item_def &item);
bool is_bad_item(const item_def &item);
bool is_dangerous_item(const item_def& item, bool temp = false);
bool is_useless_item(const item_def &item, bool temp = false,
                     bool ident = false);
string cannot_read_item_reason(const item_def *item=nullptr, bool temp=true, bool ident=false);
string cannot_drink_item_reason(const item_def *item=nullptr,
                                bool temp=true, bool use_check=false, bool ident = false);

string make_name(uint32_t seed = rng::get_uint32(),
                 makename_type name_type = MNAME_DEFAULT);
void make_name_tests();

const char* brand_type_name(brand_type brand, bool terse) PURE;
const char* brand_type_adj(brand_type brand) PURE;
const char* weapon_brand_name(const item_def& item, bool terse, brand_type override_brand = SPWPN_NORMAL) PURE;
const char* special_armour_type_name(special_armour_type ego, bool terse);
const char* armour_ego_name(const item_def& item, bool terse);
const char* missile_brand_name(const item_def& item, mbn_type t);

bool item_type_has_ids(object_class_type base_type);
void check_if_everything_is_identified();
bool type_is_identified(const item_def &item);
bool type_is_identified(object_class_type basetype, int subtype);
bool identify_item_type(object_class_type basetype, int subtype);

string item_prefix(const item_def &item, bool temp = true);
string menu_colour_item_name(const item_def &item,
                                   description_level_type desc);

void            init_item_name_cache();
item_kind item_kind_by_name(const string &name);

vector<string> item_name_list_for_glyph(char32_t glyph);

const char* rune_type_name(short p);

bool   is_named_corpse(const item_def &corpse);
string get_corpse_name(const item_def &corpse,
                       monster_flags_t *name_type = nullptr);
const char *base_type_string(object_class_type type);
const char *base_type_string(const item_def &item);

string sub_type_string(const item_def &item, bool known = true);

string ego_type_string(const item_def &item, bool terse = false);
string ghost_brand_name(brand_type brand, monster_type mtype);
string weapon_brand_desc(const char *body, const item_def &weap,
                         bool terse = false,
                         brand_type override_brand = SPWPN_NORMAL);

const char* potion_type_name(int potiontype);  //used in xom.cc
const char* jewellery_effect_name(int jeweltype, bool terse = false) PURE; //used in l-item.cc
const char* gizmo_effect_name(int gizmotype);

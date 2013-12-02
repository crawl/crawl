/**
 * @file
 * @brief Misc functions.
**/


#ifndef ITEMNAME_H
#define ITEMNAME_H

#include "externs.h"

#define CORPSE_NAME_KEY      "corpse_name_key"
#define CORPSE_NAME_TYPE_KEY "corpse_name_type_key"

struct item_kind
{
    object_class_type base_type;
    uint8_t           sub_type;
    int8_t            plus, plus2;
};

// [dshaligram] If you edit potion colours/descriptions, also update
// itemname.cc.
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
    PDC_PINK,
    PDC_WHITE,
    PDC_NCOLOURS
};

// [dshaligram] If you edit potion colours/descriptions, also update
// itemname.cc.
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
// If you change these counts, update itemname.cc.
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
    NDSC_BOOK_PRI  = 10,
    NDSC_BOOK_SEC  = 8,
};

bool is_vowel(const ucs_t chr);
int property(const item_def &item, int prop_type);

const char* racial_description_string(const item_def& item, bool terse = false);

void check_item_knowledge(bool unknown_items = false);
void display_runes();

string quant_name(const item_def &item, int quant,
                  description_level_type des, bool terse = false);

bool item_type_known(const item_def &item);
bool item_type_unknown(const item_def &item);
bool item_type_known(const object_class_type base_type, const int sub_type);
bool item_type_tried(const item_def &item);

bool is_interesting_item(const item_def& item);
bool is_emergency_item(const item_def& item);
bool is_good_item(const item_def &item);
bool is_bad_item(const item_def &item, bool temp = false);
bool is_dangerous_item(const item_def& item, bool temp = false);
bool is_useless_item(const item_def &item, bool temp = false);

string make_name(uint32_t seed, bool all_caps, int maxlen = -1, char start = 0);

const char* weapon_brand_name(const item_def& item, bool terse);
const char* armour_ego_name(const item_def& item, bool terse);

void init_properties();

bool item_type_has_ids(object_class_type base_type);
item_type_id_state_type get_ident_type(const item_def &item);
item_type_id_state_type get_ident_type(object_class_type basetype,
                                       int subtype);
void set_ident_type(item_def &item, item_type_id_state_type setting,
                     bool force = false);
void set_ident_type(object_class_type basetype, int subtype,
                     item_type_id_state_type setting, bool force = false);

string item_prefix(const item_def &item, bool temp = true);
string get_menu_colour_prefix_tags(const item_def &item,
                                   description_level_type desc);

void            init_item_name_cache();
item_kind item_kind_by_name(string name);

vector<string> item_name_list_for_glyph(unsigned glyph);

const char* rune_type_name(int p);

bool   is_named_corpse(const item_def &corpse);
string get_corpse_name(const item_def &corpse, uint64_t *name_type = NULL);
string base_type_string(object_class_type type, bool known = true);
string base_type_string(const item_def &item, bool known = true);

string sub_type_string(const item_def &item, bool known = true);

string ego_type_string(const item_def &item, bool terse = false);
#endif

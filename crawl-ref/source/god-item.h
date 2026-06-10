#pragma once

#include <vector>

#include "enum.h"
#include "forbidden-act-type.h"
#include "player.h"
#include "spl-util.h"

using std::vector;

bool is_holy_item(const item_def& item, bool calc_unid = true);
bool is_potentially_evil_item(const item_def& item, bool calc_unid = true);
bool is_evil_item(const item_def& item, bool calc_unid = true);
bool is_chaotic_item(const item_def& item, bool calc_unid = true);
bool is_hasty_item(const item_def& item, bool calc_unid = true);
bool is_wizardly_item(const item_def& item, bool calc_unid = true);
bool is_transforming_item(const item_def& item, bool calc_unid = true);
bool is_evil_brand(int brand);
bool is_chaotic_brand(int brand);
bool is_hasty_brand(int brand);
bool is_evil_spell(spell_type spell);
bool is_chaotic_spell(spell_type spell);
bool is_hasty_spell(spell_type spell);
vector<forbidden_act_type> forbidden_acts(const item_def &item);
bool god_forbids_item(const item_def &item);
bool god_forbids_item(const item_def &item, god_type which_god);
bool god_likes_item_type(const object_class_type &base_type,
                         const uint16_t sub_type, god_type which_god);

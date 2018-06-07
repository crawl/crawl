#pragma once

#include "enum.h"
#include "player.h"
#include "spl-util.h"

bool is_holy_item(const item_def& item, bool calc_unid = true);
bool is_potentially_evil_item(const item_def& item, bool calc_unid = true);
bool is_corpse_violating_item(const item_def& item, bool calc_unid = true);
bool is_evil_item(const item_def& item, bool calc_unid = true);
bool is_unclean_item(const item_def& item, bool calc_unid = true);
bool is_chaotic_item(const item_def& item, bool calc_unid = true);
bool is_hasty_item(const item_def& item, bool calc_unid = true);
bool is_channeling_item(const item_def& item, bool calc_unid = true);
bool is_corpse_violating_spell(spell_type spell);
bool is_evil_spell(spell_type spell);
bool is_unclean_spell(spell_type spell);
bool is_chaotic_spell(spell_type spell);
bool is_hasty_spell(spell_type spell);
vector<conduct_type> item_conducts(const item_def &item);
bool god_hates_item(const item_def &item);
bool god_likes_item_type(const item_def &item, god_type which_god);

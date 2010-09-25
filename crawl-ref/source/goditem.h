#ifndef GODITEM_H
#define GODITEM_H

#include "enum.h"
#include "externs.h"

#include "player.h"

bool is_holy_item(const item_def& item);
bool is_potentially_unholy_item(const item_def& item);
bool is_unholy_item(const item_def& item);
bool is_potentially_evil_item(const item_def& item);
bool is_corpse_violating_item(const item_def& item);
bool is_evil_item(const item_def& item);
bool is_unclean_item(const item_def& item);
bool is_chaotic_item(const item_def& item);
bool is_potentially_hasty_item(const item_def& item);
bool is_hasty_item(const item_def& item);
bool is_holy_discipline(int discipline);
bool is_evil_discipline(int discipline);
bool is_holy_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_unholy_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_corpse_violating_spell(spell_type spell, god_type = GOD_NO_GOD);
bool is_evil_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_unclean_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_chaotic_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_hasty_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_any_spell(spell_type spell, god_type god = GOD_NO_GOD);
bool is_spellbook_type(const item_def& item, bool book_or_rod,
                       bool (*suitable)(spell_type spell, god_type god) =
                           is_any_spell,
                       god_type god = you.religion);
bool is_holy_spellbook(const item_def& item);
bool is_unholy_spellbook(const item_def& item);
bool is_evil_spellbook(const item_def& item);
bool is_unclean_spellbook(const item_def& item);
bool is_chaotic_spellbook(const item_def& item);
bool is_hasty_spellbook(const item_def& item);
bool is_corpse_violating_spellbook(const item_def & item);
bool god_hates_spellbook(const item_def& item);
bool is_holy_rod(const item_def& item);
bool is_unholy_rod(const item_def& item);
bool is_evil_rod(const item_def& item);
bool is_unclean_rod(const item_def& item);
bool is_chaotic_rod(const item_def& item);
bool is_hasty_rod(const item_def& item);
bool is_corpse_violating_rod(const item_def & item);
bool god_hates_rod(const item_def& item);
conduct_type good_god_hates_item_handling(const item_def &item);
conduct_type god_hates_item_handling(const item_def &item);

// NOTE: As of now, these two functions only say if a god won't give a
// spell/school when giving a gift.
bool god_dislikes_spell_type(spell_type spell, god_type god = you.religion);
bool god_dislikes_spell_discipline(int discipline, god_type god = you.religion);
#endif

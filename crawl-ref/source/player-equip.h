#pragma once

#include "equipment-type.h"

// Any code that equips or melds items should go through these, to
// make sure equip/unequip effects are done the right amount of
// times, and to make sure melded flags don't get out of sync.

// XXX: the msg flag isn't implemented in all cases.
void equip_item(equipment_type slot, int item_slot, bool msg=true,
                bool skip_effects=false);
bool unequip_item(equipment_type slot, bool msg=true, bool skip_effects=false);
bool meld_slot(equipment_type slot);
bool unmeld_slot(equipment_type slot);

void equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg);
void unequip_effect(equipment_type slot, int item_slot, bool meld, bool msg);

struct item_def;
void equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld,
                           equipment_type slot);
void unequip_artefact_effect(item_def &item,  bool *show_msgs, bool meld,
                             equipment_type slot, bool weapon);

bool unwield_item(const item_def &item, bool showMsgs = true);

bool acrobat_boost_active();

void unwield_distortion(bool brand = false);

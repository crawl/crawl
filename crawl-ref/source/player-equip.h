#ifndef PLAYER_EQUIP_H
#define PLAYER_EQUIP_H

// Any code that equips or melds items should go through these, to
// make sure equip/unequip effects are done the right amount of
// times, and to make sure melded flags don't get out of sync.

// XXX: the msg flag isn't implemented in all cases.
void equip_item(equipment_type slot, int item_slot, bool msg=true);
bool unequip_item(equipment_type slot, bool msg=true);
bool meld_slot(equipment_type slot, bool msg=true);
bool unmeld_slot(equipment_type slot, bool msg=true);

bool unwield_item(bool showMsgs = true);

#endif

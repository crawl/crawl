/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/


#ifndef MONPICK_H
#define MONPICK_H

#include "externs.h"
#include "random-pick.h"

#define DEPTH_NOWHERE 999

#define pop_entry random_pick_entry<monster_type>

typedef bool (*mon_pick_vetoer)(monster_type);

int mons_rarity(monster_type mcls, branch_type branch);
int mons_depth(monster_type mcls, branch_type branch);

monster_type pick_monster(level_id place, mon_pick_vetoer veto = nullptr);
monster_type pick_monster_from(const pop_entry *fpop, int depth,
                               mon_pick_vetoer = nullptr);
monster_type pick_monster_no_rarity(branch_type branch);
monster_type pick_monster_by_hash(branch_type branch, uint32_t hash);
monster_type pick_monster_all_branches(int absdepth0, mon_pick_vetoer veto = nullptr);
bool branch_has_monsters(branch_type branch);
int branch_ood_cap(branch_type branch);

void debug_monpick();

// Subclass the random_picker template to make a monster_picker class.
// The main reason for this is that passing delegates into template functions
// is fraught with peril when the delegate's arguments use T, until 0x at least.
// There is supposedly a nasty workaround but this wouldn't even compile for me.
class monster_picker : public random_picker<monster_type, NUM_MONSTERS>
{
public:
    monster_picker(mon_pick_vetoer vetoer = nullptr) : _veto(vetoer) { };

protected:
    virtual bool veto(monster_type mon);

private:
    mon_pick_vetoer _veto;
};

#endif

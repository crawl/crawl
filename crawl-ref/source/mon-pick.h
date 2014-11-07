/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/

#ifndef MONPICK_H
#define MONPICK_H

#include "random-pick.h"

#define DEPTH_NOWHERE 999

#define pop_entry random_pick_entry<monster_type>

typedef bool (*mon_pick_vetoer)(monster_type);
typedef bool (*mon_pick_pos_vetoer)(monster_type, coord_def);

int mons_rarity(monster_type mcls, branch_type branch);
int mons_depth(monster_type mcls, branch_type branch);

monster_type pick_monster(level_id place, mon_pick_vetoer veto = nullptr);
monster_type pick_monster_from(const pop_entry *fpop, int depth,
                               mon_pick_vetoer = nullptr);
monster_type pick_monster_no_rarity(branch_type branch);
monster_type pick_monster_by_hash(branch_type branch, uint32_t hash);
monster_type pick_monster_all_branches(int absdepth0, mon_pick_vetoer veto = nullptr);
int branch_ood_cap(branch_type branch);
bool branch_has_monsters(branch_type branch);
const pop_entry* fish_population(branch_type br, bool lava);
const pop_entry* zombie_population(branch_type br);

void debug_monpick();

// Subclass the random_picker template to make a monster_picker class.
// The main reason for this is that passing delegates into template functions
// is fraught with peril when the delegate's arguments use T, until 0x at least.
// There is supposedly a nasty workaround but this wouldn't even compile for me.
class monster_picker : public random_picker<monster_type, NUM_MONSTERS>
{
public:
    monster_picker() : _veto(nullptr) { };

    monster_type pick_with_veto(const pop_entry *weights, int level,
                                monster_type none,
                                mon_pick_vetoer vetoer = nullptr);

    virtual bool veto(monster_type mon);

private:
    mon_pick_vetoer _veto;
};

class positioned_monster_picker : public monster_picker
{
public:
    positioned_monster_picker(const coord_def &_pos,
                              mon_pick_pos_vetoer _posveto = nullptr)
        : monster_picker(), pos(_pos), posveto(_posveto) { };

    virtual bool veto(monster_type mon);

protected:
    const coord_def &pos;

private:
    mon_pick_pos_vetoer posveto;
};

class zombie_picker : public positioned_monster_picker
{
public:
    zombie_picker(const coord_def &_pos, monster_type _ztype)
        : positioned_monster_picker(_pos),
          zombie_kind(_ztype)
          { };

    virtual bool veto(monster_type mon);

private:
    monster_type zombie_kind;
};

monster_type pick_monster(level_id place, monster_picker &picker,
                          mon_pick_vetoer veto = nullptr);
monster_type pick_monster_all_branches(int absdepth0, monster_picker &picker,
                                       mon_pick_vetoer veto = nullptr);

#endif

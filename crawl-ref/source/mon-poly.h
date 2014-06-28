/**
 * @file
 * @brief Monster polymorph and mimic functions.
**/

#ifndef MONPOLY_H
#define MONPOLY_H
const item_def *give_mimic_item(monster* mimic);
dungeon_feature_type get_mimic_feat(const monster* mimic);
bool feature_mimic_at(const coord_def &c);
item_def* item_mimic_at(const coord_def &c);
bool mimic_at(const coord_def &c);

#define MONST_INTERESTING(x) (x->flags & MF_INTERESTING)

enum poly_power_type
{
    PPT_LESS,
    PPT_MORE,
    PPT_SAME,
};

bool is_any_item(const item_def& item);
void monster_drop_things(
    monster* mons,
    bool mark_item_origins = false,
    bool (*suitable)(const item_def& item) = is_any_item,
    int owner_id = NON_ITEM);

void change_monster_type(monster* mons, monster_type targetc);
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power = PPT_SAME,
                       bool force_beh = false);

void slimify_monster(monster* mons, bool hostile = false);
bool mon_can_be_slimified(monster* mons);

void seen_monster(monster* mons);
#endif

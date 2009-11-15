#ifndef SHOW_H
#define SHOW_H

#include "fixedarray.h"

enum show_item_type
{
    SHOW_ITEM_ORB,
    SHOW_ITEM_WEAPON,
    SHOW_ITEM_ARMOUR,
    SHOW_ITEM_WAND,
    SHOW_ITEM_FOOD,
    SHOW_ITEM_SCROLL,
    SHOW_ITEM_RING,
    SHOW_ITEM_POTION,
    SHOW_ITEM_MISSILE,
    SHOW_ITEM_BOOK,
    SHOW_ITEM_STAVE,
    SHOW_ITEM_MISCELLANY,
    SHOW_ITEM_CORPSE,
    SHOW_ITEM_GOLD,
    SHOW_ITEM_AMULET,
    SHOW_ITEM_DETECTED,
    NUM_SHOW_ITEMS
};

enum show_class
{
    SH_NOTHING,
    SH_FEATURE,
    SH_ITEM,
    SH_CLOUD,
    SH_INVIS_EXPOSED,
    SH_MONSTER,
    NUM_SHOW_CLASSES
};

struct show_type
{
    show_class cls;
    union
    {
        dungeon_feature_type feat;
        show_item_type item;
        monster_type mons;
    };
    unsigned short colour;

    show_type();
    show_type(dungeon_feature_type f);
    show_type(const item_def &item);
    show_type(show_item_type itemtype);
    show_type(const monsters* mons);

    operator bool() const { return (cls != SH_NOTHING); }

    bool operator < (const show_type &other) const;
};

struct show_info
{
    dungeon_feature_type feat;
    show_item_type item;
    monster_type mons;
};

class monsters;
class show_def
{
    FixedArray<show_type, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> grid;
    FixedArray<show_type, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> backup;

    void _update_feat_at(const coord_def &gp, const coord_def &ep);
    void _update_item_at(const coord_def &gp, const coord_def &ep);
    void _update_cloud(int cloudno);
    void _update_monster(const monsters *monster);
    void _set_backup(const coord_def &e);

public:
    show_type operator()(const coord_def &ep) const { return grid(ep); }
    show_type get_backup(const coord_def &ep) const { return backup(ep); }

    void init();
    void update_at(const coord_def &gp, const coord_def &ep);
};

#endif

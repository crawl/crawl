#pragma once

#include <libutil.h>

#include "enum.h"
#include "mon-info.h"
#include "monster.h"
#include "tag-version.h"

typedef uint64_t map_flag_t;

constexpr map_flag_t MAP_MAGIC_MAPPED_FLAG          = 0x01;
constexpr map_flag_t MAP_SEEN_FLAG                  = 0x02;
// FIXME: MAP_CHANGED_FLAG doesn't belong here
constexpr map_flag_t MAP_CHANGED_FLAG               = 0x04;
constexpr map_flag_t MAP_DETECTED_MONSTER           = 0x08;
constexpr map_flag_t MAP_INVISIBLE_MONSTER          = 0x10;
constexpr map_flag_t MAP_DETECTED_ITEM              = 0x20;
constexpr map_flag_t MAP_VISIBLE_FLAG               = 0x40;
constexpr map_flag_t MAP_OLD_INVIS_MONSTER          = 0x80;

constexpr map_flag_t MAP_EMPHASIZE                 = 0x100;
constexpr map_flag_t MAP_MORE_ITEMS                = 0x200;
constexpr map_flag_t MAP_HALOED                    = 0x400;
constexpr map_flag_t MAP_SILENCED                  = 0x800;
constexpr map_flag_t MAP_BLOODY                   = 0x1000;
constexpr map_flag_t MAP_CORRODING                = 0x2000;
constexpr map_flag_t MAP_ICY                      = 0x8000;

/* these flags require more space to serialize: put infrequently used ones there */
constexpr map_flag_t MAP_DOOR_CONNECT_1          = 0x10000;
constexpr map_flag_t MAP_BLOOD_WEST              = 0x20000;
constexpr map_flag_t MAP_BLOOD_NORTH             = 0x40000;
constexpr map_flag_t MAP_SANCTUARY_1             = 0x80000;
constexpr map_flag_t MAP_SANCTUARY_2            = 0x100000;
constexpr map_flag_t MAP_WITHHELD               = 0x200000;
constexpr map_flag_t MAP_LIQUEFIED              = 0x400000;
constexpr map_flag_t MAP_ORB_HALOED             = 0x800000;
constexpr map_flag_t MAP_UMBRAED               = 0x1000000;
constexpr map_flag_t MAP_OLD_BLOOD             = 0x2000000;
constexpr map_flag_t MAP_QUAD_HALOED           = 0x4000000;
constexpr map_flag_t MAP_DISJUNCT              = 0x8000000;
constexpr map_flag_t MAP_BLASPHEMY            = 0x10000000;
constexpr map_flag_t MAP_BFB_CORPSE           = 0x20000000;
constexpr map_flag_t MAP_DOOR_CONNECT_2       = 0x40000000;
constexpr map_flag_t MAP_DOOR_CONNECT_3       = 0x80000000;

constexpr map_flag_t MAP_MORE_ITEMS_GOOD     = 0x100000000;
constexpr map_flag_t MAP_MORE_ITEMS_ARTEFACT = 0x200000000;
constexpr map_flag_t MAP_AWOKEN_FOREST       = 0x400000000;

struct cloud_info
{
    cloud_info() : type(CLOUD_NONE), colour(0), variety(3), tile(0), pos(0, 0),
                   killer(KILL_NONE)
    { }

    cloud_info(cloud_type t, colour_t c,
               uint8_t dur, unsigned short til, coord_def gc,
               killer_type kill)
        : type(t), colour(c), variety(dur), tile(til), pos(gc), killer(kill)
    { }

    friend bool operator==(const cloud_info &lhs, const cloud_info &rhs) {
        return lhs.type == rhs.type
               && lhs.colour == rhs.colour
               && lhs.variety == rhs.variety
               && lhs.tile == rhs.tile
               && lhs.pos == rhs.pos
               && lhs.killer == rhs.killer;
    }

    friend bool operator!=(const cloud_info &lhs, const cloud_info &rhs) {
        return !(lhs == rhs);
    }


    cloud_type type:8;
    colour_t colour;
    // for clouds with duration: decay/20, clamped to 0-3
    // for vortex clouds: the vortex phase
    uint8_t variety;
    tileidx_t tile;
    coord_def pos;
    killer_type killer;
};

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge.
 * TODO: this can shrink to 32 bytes by shrinking enums
 */
struct map_cell
{
    // TODO: in C++20 we can give these a default member initializer
    map_cell() : _feat(DNGN_UNSEEN)
    {
    }

    ~map_cell() = default;

    // copy constructor
    map_cell(const map_cell& o)
    {
        *this = o;
    }

    // copy assignment
    map_cell& operator=(const map_cell& o)
    {
        if (this == &o)
            return *this;

        flags = o.flags;
        _feat = o._feat;
        _feat_colour = o._feat_colour;
        _cloud = o._cloud ? make_unique<cloud_info>(*o._cloud) : nullptr;
        _item = o._item ? make_unique<item_def>(*o._item) : nullptr;
        _mons = o._mons ? make_unique<monster_info>(*o._mons) : nullptr;

        return *this;
    }

    // move constructor
    map_cell(map_cell&& o) noexcept = default;
    // move assignment
    // XXX: Using the default implementation causes a compiler error on gcc
    // 4.7, so we specify the implementation for now.
    map_cell& operator=(map_cell&& o) noexcept
    {
        flags = o.flags;
        _feat = o._feat;
        _feat_colour = o._feat_colour;
        _cloud = std::move(o._cloud);
        _item = std::move(o._item);
        _mons = std::move(o._mons);
        return *this;
    }

    friend bool operator==(const map_cell &lhs, const map_cell &rhs) {
        // TODO: consider providing a proper equality operator
        // item_def and monster_info currently lack such operators
        // Which makes it impossible for map_cell to provide one
        // As far as I can tell, packed_cell is the only user of this
        // And packed_cell operator== doesn't *seem* to be used
        return &lhs == &rhs;
    }

    friend bool operator!=(const map_cell &lhs, const map_cell &rhs) {
        return !(lhs == rhs);
    }

    void clear()
    {
        *this = map_cell();
    }

    // Clear prior to show update. Need to retain at least "seen" flag.
    void clear_data()
    {
        constexpr map_flag_t kept_flags = MAP_SEEN_FLAG | MAP_CHANGED_FLAG
                                          | MAP_VISIBLE_FLAG;
        const map_flag_t f = flags & kept_flags;
        clear();
        flags = f;
    }

    dungeon_feature_type feat() const
    {
        // Ugh; MSVC makes the bit field signed even though that means it can't
        // actually hold all the enum values. That seems to be in contradiction
        // of the standard (§9.6 [class.bit] paragraph 4) but what can you do?
        return static_cast<dungeon_feature_type>(static_cast<uint8_t>(_feat));
    }

    unsigned feat_colour() const
    {
        return _feat_colour;
    }

    void set_feature(dungeon_feature_type nfeat)
    {
        _feat = nfeat;
    }

    void set_feat_colour(colour_t colour = 0)
    {
        _feat_colour = colour;
    }

    item_def* item() const
    {
        return _item.get();
    }

    bool detected_item() const
    {
        const bool ret = !!(flags & MAP_DETECTED_ITEM);
        // TODO: change to an ASSERT when the underlying crash goes away
        if (ret && !_item)
        {
            //clear_item();
            return false;
        }
        return ret;
    }

    void set_item(const item_def& ii)
    {
        clear_item();
        _item = make_unique<item_def>(ii);
    }

    void set_detected_item();

    void clear_item()
    {
        // TODO: internal callers are doing a bit of duplicate work here
        _item.reset();
        flags &= ~(MAP_DETECTED_ITEM | MAP_MORE_ITEMS
                   | MAP_MORE_ITEMS_GOOD | MAP_MORE_ITEMS_ARTEFACT);
    }

    monster_type mon_type() const
    {
        return _mons ? _mons->type : MONS_NO_MONSTER;
    }

    monster_info* monsterinfo() const
    {
        return _mons.get();
    }

    void set_monster(const monster_info& mi)
    {
        clear_monster();
        _mons = make_unique<monster_info>(mi);
    }

    bool detected_monster() const
    {
        return !!(flags & MAP_DETECTED_MONSTER);
    }

    // An invisible monster which the player is unambiguously aware is currently here.
    bool invisible_monster() const
    {
        return !!(flags & MAP_INVISIBLE_MONSTER);
    }

    // The last-known location of an invisible monster that is no longer here.
    bool old_invisible_monster() const
    {
        return !!(flags & MAP_OLD_INVIS_MONSTER);
    }

    void set_detected_monster(monster_type mons)
    {
        clear_monster();
        _mons = make_unique<monster_info>(MONS_SENSED);
        _mons->base_type = mons;
        flags |= MAP_DETECTED_MONSTER;
    }

    void set_invisible_monster(const monster* mon)
    {
        clear_monster();
        _mons = make_unique<monster_info>(mon);
        _mons->mb.set(MB_INVISIBLE, false); // Avoid redundant invisibility descriptions.
        flags |= MAP_INVISIBLE_MONSTER;
        _mons->mb.set(MB_KNOWN_INVIS);
    }

    void set_old_invisible_monster(const monster* mon)
    {
        _mons = make_unique<monster_info>(mon->type, mon->base_monster);
        _mons->mb.set(MB_INVISIBLE, false); // Avoid redundant invisibility descriptions.
        flags |= MAP_OLD_INVIS_MONSTER;
        _mons->mb.set(MB_REMEMBERED_INVIS);
    }

    void clear_monster()
    {
        // TODO: internal callers are doing a bit of duplicate work here
        _mons.reset();
        flags &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER | MAP_OLD_INVIS_MONSTER);
    }

    cloud_type cloud() const
    {
        return _cloud ? _cloud->type : CLOUD_NONE;
    }

    // TODO: should this be colour_t?
    unsigned cloud_colour() const
    {
        return _cloud ? _cloud->colour : static_cast<colour_t>(0);
    }

    cloud_info* cloudinfo() const
    {
        return _cloud.get();
    }

    void set_cloud(const cloud_info& ci)
    {
        _cloud = make_unique<cloud_info>(ci);
    }

    void clear_cloud()
    {
        _cloud.reset();
    }

    bool update_cloud_state();

    bool known() const
    {
        constexpr map_flag_t known_flags = MAP_MAGIC_MAPPED_FLAG
                                           | MAP_SEEN_FLAG
                                           | MAP_DETECTED_MONSTER
                                           | MAP_INVISIBLE_MONSTER
                                           | MAP_DETECTED_ITEM
                                           | MAP_VISIBLE_FLAG
                                           | MAP_OLD_INVIS_MONSTER;
        return !!(flags & known_flags);
    }

    bool seen() const
    {
        return !!(flags & MAP_SEEN_FLAG);
    }

    bool visible() const
    {
        return !!(flags & MAP_VISIBLE_FLAG);
    }

    bool changed() const
    {
        return !!(flags & MAP_CHANGED_FLAG);
    }

    bool mapped() const
    {
        return !!(flags & MAP_MAGIC_MAPPED_FLAG);
    }

    bool feat_known() const
    {
        return !!(flags & (MAP_MAGIC_MAPPED_FLAG | MAP_SEEN_FLAG));
    }

#ifdef USE_TILE
    char blood_rotation() const noexcept
    {
        char result = 0;
        if (flags & MAP_BLOOD_WEST)
            result += 1;
        if (flags & MAP_BLOOD_NORTH)
            result += 2;
        return result;
    }
#endif

    void set_door_connect(unsigned short door_connect)
    {
        constexpr map_flag_t door_connect_flags = MAP_DOOR_CONNECT_1
                                                  | MAP_DOOR_CONNECT_2
                                                  | MAP_DOOR_CONNECT_3;
        flags &= ~door_connect_flags;
        ASSERT(door_connect < 7);
        if (door_connect & 1)
            flags |= MAP_DOOR_CONNECT_1;
        if ((door_connect >> 1) & 1)
            flags |= MAP_DOOR_CONNECT_2;
        if ((door_connect >> 2) & 1)
            flags |= MAP_DOOR_CONNECT_3;
    }

    unsigned short door_connect() const
    {
        unsigned short result = 0;
        if (flags & MAP_DOOR_CONNECT_1)
            result += 1;
        if (flags & MAP_DOOR_CONNECT_2)
            result += 2;
        if (flags & MAP_DOOR_CONNECT_3)
            result += 4;
        return result;
    }

public:
    map_flag_t flags = 0;   // Flags describing the mappedness of this square.
private:
    // TODO: shrink enums, shrink/re-order cloud_info and inline it
    dungeon_feature_type _feat:8;
    colour_t _feat_colour = 0;
    unique_ptr<cloud_info> _cloud;
    unique_ptr<item_def> _item;
    unique_ptr<monster_info> _mons;
};

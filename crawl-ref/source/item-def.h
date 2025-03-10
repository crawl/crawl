/**
 * @file
 * @brief Item definition.
**/

#pragma once

#include "description-level-type.h"
#include "level-id.h"
#include "monster-type.h"
#include "object-class-type.h"
#include "skill-type.h"
#include "store.h"

// We are not 64 bits clean here yet since many places still pass (or store!)
// it as 32 bits or, worse, longs. I considered setting this as uint32_t,
// however, since free bits are exhausted, it's very likely we'll have to
// extend this in the future, so this should be easier than undoing the change.
typedef uint32_t iflags_t;

struct item_def
{
    object_class_type base_type; ///< basic class (eg OBJ_WEAPON)
    uint8_t        sub_type;       ///< type within that class (eg WPN_DAGGER)
#pragma pack(push,2)
    union
    {
        // These must all be the same size!
        short plus;                 ///< + to hit/dam (weapons)
        monster_type mon_type:16;   ///< corpse/chunk monster type
        skill_type skill:16;        ///< the skill provided by a manual
        short charges;              ///< # of charges held by a wand, etc
        short net_durability;       ///< damage dealt to a net
        short tithe_state;          ///< tithe state of a stack of gold
    };
    union
    {
        // These must all be the same size!
        short plus2;        ///< legacy/generic name for this union
        short net_placed;   ///< is this throwing net trapping something?
        short skill_points; ///< # of skill points a manual gives
        short stash_freshness; ///< where stash.cc stores corpse freshness
    };
#pragma pack(pop)
    union
    {
        // These must all be the same size!
        int special;            ///< legacy/generic name
        int unrand_idx;         ///< unrandart index (for get_unrand_entry)
        uint32_t subtype_rnd;   ///< appearance of un-ID'd items, by subtype.
                                /// jewellery, scroll, staff, wand, potions
                                // see comment in item_colour()
        int brand;              ///< weapon and armour brands
        int freshness;          ///< remaining time until a corpse rots
    };
    uint8_t        rnd;            ///< random number, used for tile choice,
                                   /// randart colours, and other per-item
                                   /// random cosmetics. 0 = uninitialized
    short          quantity;       ///< number of items
    iflags_t       flags;          ///< item status flags

    /// The location of the item. Items in player inventory are indicated by
    /// pos (-1, -1), items in monster inventory by (-2, -2), and items
    /// in shops by (0, y) for y >= 5.
    coord_def pos;
    /// For floor items, index in the env.item array of the next item in the
    /// pile. NON_ITEM for the last item in a pile. For items in player
    /// inventory, instead the index into you.inv. For items in monster
    /// inventory, equal to NON_ITEM + 1 + mindex. For items in shops,
    /// equal to ITEM_IN_SHOP.
    short  link;
    /// Inventory letter of the item. For items in player inventory, equal
    /// to index_to_letter(link). For other items, equal to the slot letter
    /// the item had when it was last in player inventory.
    short  slot;

    level_id orig_place;
    short    orig_monnum;

    string inscription;

    CrawlHashTable props;

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0), rnd(0), quantity(0), flags(0),
                 pos(), link(NON_ITEM), slot(0), orig_place(),
                 orig_monnum(0), inscription()
    {
    }

    string name(description_level_type descrip, bool terse = false,
                bool ident = false, bool with_inscription = true,
                bool quantity_in_words = false) const;
    bool has_spells() const;
    bool cursed() const;
    colour_t get_colour() const;

    bool is_type(int base, int sub) const
    {
        return base_type == base && sub_type == sub;
    }

    /**
     * Find the index of an item in the env.item array. Results are undefined
     * if this item is not in the array!
     *
     * @pre The item is actually in the env.item array.
     * @return  The index of this item in the env.item array, between
     *          0 and MAX_ITEMS-1.
     */
    int  index() const;

    int  armour_rating() const;

    void clear()
    {
        *this = item_def();
    }

    /**
     * Sets this item as being held by a given monster.
     *
     * @param mon The monster. Must be in env.mons!
     */
    void set_holding_monster(const monster& mon);

    /**
     * Get the monster holding this item.
     *
     * @return A pointer to the monster holding this item, null if none.
     */
    monster* holding_monster() const;

    /** Is this item being held by a monster? */
    bool held_by_monster() const;

    bool defined() const;
    bool appearance_initialized() const;
    bool is_valid(bool info = false, bool error=false) const;

    /** Should this item be preserved as far as possible? */
    bool is_critical() const;

    /** Is this item of a type that should not be generated enchanted? */
    bool is_mundane() const;

    /** Is this item fully identified? */
    bool is_identified() const;

    /// If this is a gem, what colour is it in console?
    colour_t gem_colour() const;

private:
    string name_aux(description_level_type desc, bool terse, bool ident,
                    bool with_inscription) const;

    colour_t randart_colour() const;

    colour_t ring_colour() const;
    colour_t amulet_colour() const;

    colour_t rune_colour() const;

    colour_t weapon_colour() const;
    colour_t missile_colour() const;
    colour_t armour_colour() const;
    colour_t wand_colour() const;
    colour_t jewellery_colour() const;
    colour_t potion_colour() const;
    colour_t book_colour() const;
    colour_t miscellany_colour() const;
    colour_t talisman_colour() const;
    colour_t corpse_colour() const;
};

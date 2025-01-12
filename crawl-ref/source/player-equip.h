#pragma once
#include <cstdint>

#include "art-enum.h"
#include "artefact-prop-type.h"
#include "artefact.h"
#include "bitary.h"
#include "equipment-slot.h"
#include "fixedvector.h"
#include "transformation.h"
#include "object-class-type.h"

// Represents a single instance of an item being equipped in a slot by a player.
struct player_equip_entry
{
    // The index of the equipped item in the player's inventory.
    int8_t item;

    // The type of slot being occupied by this item.
    equipment_slot slot;

    // True if this item is currently melded.
    bool melded;

    // True if this is an item that activates when worn with max HP/MP (ie:
    // regeneration items) and max HP/MP has been reached while wearing it.
    bool attuned;

    // True if this is an item which occupies multiple slots (ie: Lear's Hauberk)
    // and this is not its main slot. (So we can know which slots are occupied
    // without counting the item's bonuses multiple times.)
    bool is_overflow;

    item_def& get_item() const;

    player_equip_entry(item_def& _item, equipment_slot _slot, bool _is_overflow = false);
    player_equip_entry(int _item, equipment_slot _slot, bool _melded, bool _attuned,
                       bool _is_overflow);
};
struct player_equip_set
{
    // The number of each type of equipment slot that the player currently has
    FixedVector<int, NUM_EQUIP_SLOTS> num_slots;

    // List of every equipped item and properties relevant to its equip state
    vector<player_equip_entry> items;

    // Combined total of all artprops on all equipped and active artefacts
    // (including talisman)
    artefact_properties_t artprop_cache;

    // Cache of which unrandarts are currently equipped, stored as a set of
    // bitflags corresponding to that unrand's ID. The corresponding bit will
    // be set in unrand_equipped whether the unrand is melded or not, but only
    // in unrand_active if it is currently unmelded.
    FixedBitVector<NUM_UNRANDARTS> unrand_equipped;
    FixedBitVector<NUM_UNRANDARTS> unrand_active;

    // Number of unrands that we should run the _*_world_reacts function for.
    int do_unrand_reacts;

    // Number of unrands that we should run the _*_death_effects function for.
    int do_unrand_death_effects;

    player_equip_set();

    // Initialises proper values for cached values. (To be called after full
    // set of items is added.)
    void update();

    // Querying methods
    int wearing_ego(object_class_type obj_type, int ego) const;
    int wearing(object_class_type obj_type, int sub_type,
                bool count_plus, bool check_attunement) const;
    int get_artprop(artefact_prop_type prop) const;
    vector<item_def*> get_slot_items(equipment_slot slot, bool include_melded = false,
                                     bool attuned_only = false) const;
    vector<player_equip_entry> get_slot_entries(equipment_slot slot) const;
    item_def* get_first_slot_item(equipment_slot slot, bool include_melded = false) const;
    player_equip_entry& get_entry_for(const item_def& item);

    bool slot_is_fully_covered(equipment_slot slot) const;
    bool has_compatible_slot(equipment_slot slot, bool include_form = false) const;

    // Basic mutators
    void add(item_def& item, equipment_slot slot);
    void remove(const item_def& item);

    // Melding-related functions
    void meld_equipment(int slots, bool skip_effects = false);
    void unmeld_slot(equipment_slot slot, bool skip_effects = false);
    void unmeld_all_equipment(bool skip_effects = false);
    bool is_melded(const item_def& item);

    // Functions related to the equipping/unequipping process
    equipment_slot find_compatible_occupied_slot(const item_def& old_item,
                                                 const item_def& new_item) const;
    equipment_slot find_equipped_slot(const item_def& item) const;
    equipment_slot find_slot_to_equip_item(const item_def& item,
                                           vector<vector<item_def*>>& to_replace,
                                           bool ignore_curses = false) const;

    int needs_chain_removal(const item_def& item, vector<item_def*>& to_replace,
                            bool cursed_okay = false);

    vector<item_def*> get_forced_removal_list(bool force_full_check = false);

private:
    equipment_slot find_slot_to_equip_item(equipment_slot base_slot,
                                           vector<item_def*>& to_replace,
                                           bool ignore_curses) const;

    void handle_unmelding(vector<item_def*>& to_unmeld, bool skip_effects);
};

int get_player_equip_slot_count(equipment_slot slot, string* zero_reason = nullptr);
FixedVector<int, NUM_EQUIP_SLOTS> get_total_player_equip_slots();

bool can_equip_item(const item_def& item, bool include_form = false,
                    string* veto_reason = nullptr);

// XXX: the msg flag isn't implemented in all cases.
void equip_item(equipment_slot slot, int item_slot, bool msg=true,
                bool skip_effects=false);
bool unequip_item(item_def& item, bool msg=true, bool skip_effects=false);

bool slot_is_melded(equipment_slot slot);

void autoequip_item(item_def& item);

void equip_effect(int item_slot, bool unmeld, bool msg);
void unequip_effect(int item_slot, bool meld, bool msg);

struct item_def;
void equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld);
void unequip_artefact_effect(item_def &item, bool *show_msgs, bool meld);

bool acrobat_boost_active();

void unwield_distortion(bool brand = false);

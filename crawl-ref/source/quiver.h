/**
 * @file
 * @brief Player quiver functionality
**/

#ifndef QUIVER_H
#define QUIVER_H

#include "externs.h" /* item_def */
#include <vector>

class reader;
class writer;
class preserve_quiver_slots;

enum ammo_t
{
    AMMO_THROW,           // no launcher wielded -> darts, stones, ...
    AMMO_BOW,             // wielded bow -> arrows
    AMMO_SLING,           // wielded sling -> stones, sling bullets
    AMMO_CROSSBOW,        // wielded crossbow -> bolts
    // Used to be hand crossbows
    AMMO_BLOWGUN,         // wielded blowgun -> needles
    NUM_AMMO
};

class player_quiver
{
    friend class preserve_quiver_slots;
 public:
    player_quiver();

    // Queries from engine -- don't affect state
    void get_desired_item(const item_def** item_out, int* slot_out) const;
    int get_fire_item(string* no_item_reason = 0) const;
    void get_fire_order(vector<int>& v, bool manual) const;

    // Callbacks from engine
    void set_quiver(const item_def &item, ammo_t ammo_type);
    void empty_quiver(ammo_t ammo_type);
    void on_item_fired(const item_def &item, bool explicitly_chosen = false);
    void on_item_fired_fi(const item_def &item);
    void on_inv_quantity_changed(int slot, int amt);
    void on_weapon_changed();

    // save/load
    void save(writer&) const;
    void load(reader&);

 private:
    void _get_fire_order(vector<int>&,
                         bool ignore_inscription_etc,
                         const item_def* launcher,
                         bool manual) const;
    void _maybe_fill_empty_slot();

 private:
    item_def m_last_weapon;

    ammo_t m_last_used_type;
    item_def m_last_used_of_type[NUM_AMMO];
};

// Quiver tracks items, which in most cases is the Right Thing.  But
// when a quivered item is identified, the quiver doesn't change to
// match.  We would like the quiver to store the identified item.
//
// This class saves off the quiver item slots, and restores them when
// destroyed.  The expected use is to create one of these around code
// that identifies items in inv.
class preserve_quiver_slots
{
public:
    preserve_quiver_slots();
    ~preserve_quiver_slots();

 private:
    int m_last_used_of_type[NUM_AMMO];
};

void quiver_item(int slot);
void choose_item_for_quiver(void);

#endif

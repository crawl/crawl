/*
 *  File:       quiver.h
 *  Summary:    Player quiver functionality
 *
 *  Last modified by $Author: $ on $Date: $
 */

#ifndef QUIVER_H
#define QUIVER_H

#include "externs.h" /* item_def */
#include <vector>

enum ammo_t
{
    AMMO_THROW,           // no launcher wielded -> darts, stones, ...
    AMMO_BOW,             // wielded bow -> arrows
    AMMO_SLING,           // wielded sling -> stones, sling bullets
    AMMO_CROSSBOW,        // wielded crossbow -> bolts
    AMMO_HAND_CROSSBOW,   // wielded hand crossbow -> darts
    AMMO_BLOWGUN,         // wielded blowgun -> needles
    NUM_AMMO,
    AMMO_INVALID=-1
};

class player_quiver
{
 public:
    player_quiver();

    void get_desired_item(const item_def** item_out, int* slot_out) const;
    int get_fire_item(std::string* no_item_reason=0) const;
    void get_fire_order(std::vector<int>& v) const;

    void on_item_fired(const item_def&);
    void on_item_fired_fi(const item_def&);
    void on_inv_quantity_change(int slot, int amt);
    void on_weapon_changed();

 private:
    void _get_fire_order(std::vector<int>&,
                         bool ignore_inscription_etc,
                         const item_def* launcher) const;
    void _maybe_fill_empty_slot();
 private:
    item_def m_last_weapon;

    ammo_t m_last_used_type;
    item_def m_last_used_of_type[NUM_AMMO];
};

#endif

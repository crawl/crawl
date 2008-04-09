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
    // Returns an item, or a reason why we returned an invalid item
    int get_default_slot(std::string& no_item_reason) const;
    void get_fire_order(std::vector<int>& v) const { _get_fire_order(v, false); }

    void on_item_fired(const item_def&);
    void on_item_thrown(const item_def&);
    void on_weapon_changed();

 private:
    void _get_fire_order(std::vector<int>&, bool ignore_inscription_etc) const;
 private:
    ammo_t m_last_used_type;
    item_def m_last_used_of_type[NUM_AMMO];
};

#endif

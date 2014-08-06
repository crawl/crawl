/**
 * @file
 * @brief Misc function related to player transformations.
**/

#ifndef TRANSFOR_H
#define TRANSFOR_H

#include <set>

#include "enum.h"
#include "player.h"

class Form
{
public:
    Form(const char *_name, int _blocked_slots,
         size_type _size, int _stealth_mod,
         monster_type _equivalent_mons) :
    name(_name), blocked_slots(_blocked_slots),
    size(_size), stealth_mod(_stealth_mod),
    equivalent_mons(_equivalent_mons)
    { };

    bool slot_available(int slot) const;
    bool can_wear_item(const item_def& item) const;
    virtual monster_type get_equivalent_mons() const;
    virtual int get_stealth_mod() const;

public:
    const char* const name;
    const int blocked_slots;
    const size_type size;

protected:
    const int stealth_mod;

private:
    bool all_blocked(int slotflags) const;

private:
    const monster_type equivalent_mons;
};
const Form* get_form(transformation_type form = you.form);

bool form_can_wield(transformation_type form = you.form);
bool form_can_wear(transformation_type form = you.form);
bool form_can_fly(transformation_type form = you.form);
bool form_can_swim(transformation_type form = you.form);
bool form_likes_water(transformation_type form = you.form);
bool form_likes_lava(transformation_type form = you.form);
bool form_changed_physiology(transformation_type form = you.form);
bool form_can_bleed(transformation_type form = you.form);
bool form_can_use_wand(transformation_type form = you.form);
bool form_can_wear_item(const item_def& item,
                        transformation_type form = you.form);
// Does the form keep the benefits of resistance, scale, and aux mutations?
bool form_keeps_mutations(transformation_type form = you.form);

bool feat_dangerous_for_form(transformation_type which_trans,
                             dungeon_feature_type feat);
bool transform(int pow, transformation_type which_trans,
               bool involuntary = false, bool just_check = false);

// skip_move: don't make player re-enter current cell
void untransform(bool skip_wielding = false, bool skip_move = false);

size_type transform_size(int psize = PSIZE_BODY,
                         transformation_type form = you.form);

void remove_one_equip(equipment_type eq, bool meld = true,
                      bool mutation = false);
void unmeld_one_equip(equipment_type eq);

monster_type transform_mons();
string blade_parts(bool terse = false);
monster_type dragon_form_dragon_type();
const char* transform_name(transformation_type form = you.form);

int form_hp_mod();

void emergency_untransform();
void merfolk_start_swimming(bool step = false);
void merfolk_stop_swimming();

#endif

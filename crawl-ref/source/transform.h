/**
 * @file
 * @brief Misc function related to player transformations.
**/

#ifndef TRANSFOR_H
#define TRANSFOR_H

#include <set>

#include "enum.h"
#include "player.h"

enum form_capability
{
    DEFAULT,
    ENABLE,
    FORBID
};

class Form
{
public:
    Form(string _short_name, string _long_name, string _wiz_name,
         string _description,
         int _blocked_slots,
         int _str_mod, int _dex_mod,
         size_type _size, int _stealth_mod,
         int _unarmed_hit_bonus, int _base_unarmed_damage, int _uc_colour,
         form_capability _can_fly, form_capability _can_swim,
         monster_type _equivalent_mons) :
    short_name(_short_name), wiz_name(_wiz_name),
    str_mod(_str_mod), dex_mod(_dex_mod),
    blocked_slots(_blocked_slots), size(_size),
    unarmed_hit_bonus(_unarmed_hit_bonus), uc_colour(_uc_colour),
    long_name(_long_name), description(_description),
    stealth_mod(_stealth_mod),
    base_unarmed_damage(_base_unarmed_damage),
    can_fly(_can_fly), can_swim(_can_swim),
    equivalent_mons(_equivalent_mons)
    { };

    bool slot_available(int slot) const;
    bool can_wear_item(const item_def& item) const;

    virtual monster_type get_equivalent_mons() const { return equivalent_mons; }

    virtual string get_long_name() const { return long_name; }
    virtual string get_description(bool past_tense = false) const;
    virtual string transform_message(transformation_type previous_trans) const;
    virtual string get_transform_description() const { return description; }
    virtual string get_untransform_message() const;

    virtual int get_stealth_mod() const { return stealth_mod; }
    virtual int get_base_unarmed_damage() const { return base_unarmed_damage; }

    bool player_can_fly() const;
    bool player_can_swim() const;

public:
    const string short_name;
    const string wiz_name;

    const int str_mod;
    const int dex_mod;

    const int blocked_slots;
    const size_type size;

    const int unarmed_hit_bonus;
    const int uc_colour;

protected:
    const string long_name;
    const string description;
    const int stealth_mod;
    const int base_unarmed_damage;

private:
    bool all_blocked(int slotflags) const;

private:
    const form_capability can_fly;
    const form_capability can_swim;

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

bool check_form_stat_safety(transformation_type new_form);

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

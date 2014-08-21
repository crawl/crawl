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
    FC_DEFAULT,
    FC_ENABLE,
    FC_FORBID
};

class FormAttackVerbs
{
public:
    FormAttackVerbs(const char *_weak, const char *_medium,
                    const char *_strong, const char *_devastating) :
    weak(_weak), medium(_medium), strong(_strong), devastating(_devastating)
    { };

public:
    const char * const weak;
    const char * const medium;
    const char * const strong;
    const char * const devastating;
};

enum duration_power_scaling
{
    PS_NONE,                // no bonus
    PS_SINGLE,              // bonus based on rand2(power)
    PS_ONE_AND_A_HALF,      // bonus based on r(power) + r(power/2)
    PS_DOUBLE               // bonus based on r(power) + r(power)
};

class FormDuration
{
public:
    FormDuration(int _base, duration_power_scaling _scaling_type, int _max) :
    base(_base), scaling_type(_scaling_type), max(_max) { };

    int power_bonus(int pow) const;

public:
    const int base;
    const duration_power_scaling scaling_type;
    const int max;
};

class Form
{
public:
    Form(string _short_name, string _long_name, string _wiz_name,
         string _description,
         int _blocked_slots,
         int _resists,
         FormDuration _duration,
         int _str_mod, int _dex_mod,
         size_type _size, int _hp_mod, int _stealth_mod,
         int _spellcasting_penalty,
         int _unarmed_hit_bonus, int _base_unarmed_damage, int _uc_colour,
         FormAttackVerbs _uc_attack_verbs,
         form_capability _can_fly, form_capability _can_swim,
         form_capability _can_bleed, bool _breathes, bool _keeps_mutations,
         monster_type _equivalent_mons) :
    short_name(_short_name), wiz_name(_wiz_name),
    duration(_duration),
    str_mod(_str_mod), dex_mod(_dex_mod),
    blocked_slots(_blocked_slots), size(_size), hp_mod(_hp_mod),
    spellcasting_penalty(_spellcasting_penalty),
    unarmed_hit_bonus(_unarmed_hit_bonus), uc_colour(_uc_colour),
    uc_attack_verbs(_uc_attack_verbs),
    can_bleed(_can_bleed), breathes(_breathes),
    keeps_mutations(_keeps_mutations),
    long_name(_long_name), description(_description),
    resists(_resists), stealth_mod(_stealth_mod),
    base_unarmed_damage(_base_unarmed_damage),
    can_fly(_can_fly), can_swim(_can_swim),
    equivalent_mons(_equivalent_mons)
    { };

    bool slot_available(int slot) const;
    bool can_wear_item(const item_def& item) const;

    int get_duration(int pow) const;

    virtual monster_type get_equivalent_mons() const { return equivalent_mons; }

    virtual string get_long_name() const { return long_name; }
    virtual string get_description(bool past_tense = false) const;
    virtual string transform_message(transformation_type previous_trans) const;
    virtual string get_transform_description() const { return description; }
    virtual string get_untransform_message() const;

    virtual int res_fire() const;
    virtual int res_cold() const;
    int res_neg() const;
    bool res_elec() const;
    int res_pois() const;
    bool res_rot() const;
    bool res_acid() const;
    bool res_sticky_flame() const;

    virtual int get_stealth_mod() const { return stealth_mod; }
    virtual int get_base_unarmed_damage() const { return base_unarmed_damage; }

    bool enables_flight() const;
    bool forbids_flight() const;
    bool forbids_swimming() const;

    bool player_can_fly() const;
    bool player_can_swim() const;

public:
    const string short_name;
    const string wiz_name;

    const FormDuration duration;

    const int str_mod;
    const int dex_mod;

    const int blocked_slots;
    const size_type size;
    const int hp_mod;

    const int spellcasting_penalty;

    const int unarmed_hit_bonus;
    const int uc_colour;
    const FormAttackVerbs uc_attack_verbs;

    const form_capability can_bleed;
    const bool breathes;
    const bool keeps_mutations;

protected:
    const string long_name;
    const string description;

    const int resists;

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

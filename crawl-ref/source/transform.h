/**
 * @file
 * @brief Misc function related to player transformations.
**/

#pragma once

#include <set>

#include "enum.h"
#include "player.h"

#define DRAGON_CLAWS 3
#define DRAGON_FANGS 5

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
                    const char *_strong, const char *_devastating)
        : weak(_weak), medium(_medium), strong(_strong),
          devastating(_devastating)
    { }

    FormAttackVerbs(const char *msg)
        : weak(msg), medium(msg), strong(msg), devastating(msg)
    { }

public:
    const char * const weak;
    const char * const medium;
    const char * const strong;
    const char * const devastating;
};

enum duration_power_scaling
{
    PS_NONE,                ///< no bonus
    PS_SINGLE,              ///< bonus based on rand2(power)
    PS_ONE_AND_A_HALF,      ///< bonus based on r(power) + r(power/2)
    PS_DOUBLE               ///< bonus based on r(power) + r(power)
};

class FormDuration
{
public:
    FormDuration(int _base, duration_power_scaling _scaling_type, int _max) :
    base(_base), scaling_type(_scaling_type), max(_max) { };

    int power_bonus(int pow) const;

public:
    /// base duration (in 10*aut, probably)
    const int base;
    /// the extent to which spellpower affects duration scaling
    const duration_power_scaling scaling_type;
    /// max duration (in 10*aut, probably)
    const int max;
};

class FormScaling {
public:
    FormScaling() : base(0), scaling(0), xl_based(false) {}
    FormScaling& Base(int b) { base = b; return *this; }
    FormScaling& Scaling(int s) { scaling = s; return *this; }
    FormScaling& XLBased() { xl_based = true; return *this; }

    int base;      // value at 0 XL/skill (as applicable)
    int scaling;   // value added to base at max XL/skill
    bool xl_based; // if false, scale on Shapeshifting skill
};

struct form_entry; // defined in form-data.h (private)
class Form
{
private:
    Form() = delete;
    DISALLOW_COPY_AND_ASSIGN(Form);
    Form(const form_entry &fe);
protected:
    Form(transformation tran);
public:
    bool slot_available(int slot) const;
    bool can_wield() const { return slot_available(EQ_WEAPON); }
    virtual bool can_wear_item(const item_def& item) const;

    int get_duration(int pow) const;

    int get_level(int scale) const;

    int mult_hp(int base_hp, bool force_talisman = false) const;

    /// Is the player below the minimum skill for this form?
    bool underskilled() const { return get_level(1) < min_skill; }

    /**
     * What monster corresponds to this form?
     *
     * Used for console player glyphs.
     *
     * @return The monster_type corresponding to this form.
     */
    virtual monster_type get_equivalent_mons() const { return equivalent_mons; }

    /**
     * A name for the form longer than that used by the status light.
     *
     * E.g. "foo-form", "blade tentacles", &c. Used for dumps, morgues &
     * the % screen.
     *
     * @return The 'long name' of the form.
     */
    virtual string get_long_name() const { return long_name; }

    /**
     * A description of this form.
     *
     * E.g. "a fearsome dragon!", punctuation included.
     *
     * Used for the @ screen and, by default, transformation messages.
     *
     * @return A description of the form.
     */
    virtual string get_transform_description() const { return description; }

    virtual string get_description(bool past_tense = false) const;
    virtual string transform_message(bool was_flying) const;
    virtual string get_untransform_message() const;

    virtual int res_fire() const;
    virtual int res_cold() const;
    int res_neg() const;
    bool res_elec() const;
    int res_pois() const;
    bool res_rot() const;
    bool res_acid() const;
    bool res_miasma() const;
    bool res_petrify() const;

    /**
     * Base unarmed damage provided by the form.
     */
    int get_base_unarmed_damage(bool random = true, bool max = false) const;

    /// Damage done by a custom aux attack of this form.
    virtual int get_aux_damage(bool /*random*/ = true,
                               bool /*max*/ = false) const {
        return 0;
    }

    /// Does this form care about skill for UC damage and accuracy, or only XL?
    virtual bool get_unarmed_uses_skill() const { return !unarmed_bonus_dam.xl_based; }

    /**
     * The brand of this form's unarmed attacks (SPWPN_FREEZING, etc).
     */
    virtual brand_type get_uc_brand() const { return uc_brand; }

    virtual bool can_offhand_punch() const { return can_wield(); }
    virtual string get_uc_attack_name(string default_name) const;
    virtual int slay_bonus(bool /*random*/ = true, bool /*max*/ = false) const { return 0; }
    virtual int contam_dam(bool /*random*/ = true, bool /*max*/ = false) const { return 0; }
    virtual int get_ac_bonus(bool max = false) const;
    virtual int ev_bonus(bool /*max*/ = false) const { return 0; }
    virtual int get_base_ac_penalty(int /*base*/) const { return 0; }

    bool enables_flight() const;
    bool forbids_flight() const;

    bool player_can_fly() const;
    bool player_can_swim() const;
    bool player_likes_water() const;

    string player_prayer_action() const;
    string melding_description() const;

    virtual vector<string> get_fakemuts(bool terse) const;
    virtual vector<string> get_bad_fakemuts(bool terse) const;

public:
    /// Status light ("Foo"); "" for none
    const string short_name;
    /// "foo"; used for wizmode transformation dialogue
    const string wiz_name;

    /// A struct representing the duration of the form, based on power etc
    const FormDuration duration;

    /// The skill level below which the player gets HP penalties for using the form.
    const int min_skill;
    /// The skill level beyond which further skill provides no benefit.
    const int max_skill;

    /// flat str bonus
    const int str_mod;
    /// flat dex bonus
    const int dex_mod;

    /// Equipment types unusable in this form.
    /** A bitfield representing a union of (1 << equipment_type) values for
     * equipment types that are unusable in this form.
     */
    const int blocked_slots; // XX check enum size at compile time?
    /// size of the form
    const size_type size;

    /// can the player cast while in this form?
    const bool can_cast;

    /// colour of 'weapon' in UI
    const int uc_colour;
    /// a set of verbs to use based on damage done, when using UC in this form
    const FormAttackVerbs uc_attack_verbs;

    /// "Used to mark forms which keep most form-based mutations."
    const bool keeps_mutations;
    //
    const bool changes_physiology;

    /// Does this form have blood (used for sublimation and bloodsplatters)?
    const form_capability has_blood;
    /// Does this form have hair?
    const form_capability has_hair;
    /// Does this form have bones?
    const form_capability has_bones;
    /// Does this form have feet?
    const form_capability has_feet;
    /// Does this form have ears?
    const form_capability has_ears;

    /// what verb does the player use when shouting in this form?
    const string shout_verb;
    /// a flat bonus (or penalty) to shout volume
    const int shout_volume_modifier;

    /// The name of this form's hand-equivalents; "" defaults to species.
    const string hand_name;
    /// The name of this form's foot-equivalents; "" defaults to species.
    const string foot_name;
    /// The name of this form's flesh-equivalent; "" defaults to species.
    const string flesh_equivalent;

protected:
    /// See Form::get_long_name().
    const string long_name;
    /// See Form::get_transform_description().
    const string description;

    /// Resistances granted by this form.
    /** A bitfield holding a union of mon_resist_flags for resists granted
     * by the form.
     */
    const int resists;

    /// bonuses to AC when in this form, potentially scaling with skill or XL
    const FormScaling ac;

    /// See Form::get_base_unarmed_damage().
    const FormScaling unarmed_bonus_dam;

    /// Calculate the given FormScaling for this form, multiplied by scale.
    int scaling_value(const FormScaling &sc, bool random,
                      bool max = false, int scale = 1) const;
    /// Calculate the given FormScaling for this form, with math internally multiplied by scale.
    int divided_scaling(const FormScaling &sc, bool random,
                        bool max = false, int scale = 1) const;

private:
    bool all_blocked(int slotflags) const;

private:
    /// Can this form fly?
    /** Whether the form enables, forbids, or does nothing to the player's
     * ability to fly.
     */
    const form_capability can_fly;
    /// Can this form swim?
    /** Whether the form enables, forbids, or does nothing to the player's
     * ability to swim (traverse deep water).
     */
    const form_capability can_swim;

    /// See Form::get_uc_brand().
    const brand_type uc_brand;
    /// the name of the uc 'weapon' in the HUD; "" uses species defaults.
    const string uc_attack;

    /// Altar prayer action; "" uses defaults. See Form::player_prayer_action()
    const string prayer_action;

    /// See Form::get_equivalent_mons().
    const monster_type equivalent_mons;

    /// 10 * multiplier to hp/mhp (that is, 10 is base, 15 is 1.5x, etc)
    const int hp_mod;

    vector<pair<string,string>> fakemuts;
    vector<pair<string,string>> badmuts;
};
const Form* get_form(transformation form = you.form);
const Form* cur_form(bool temp);

enum undead_form_reason
{
    UFR_TOO_DEAD  = -1,
    UFR_GOOD      = 0, // Must be 0, so we convert to bool sanely.
    UFR_TOO_ALIVE = 1,
};
undead_form_reason lifeless_prevents_form(transformation form = you.form,
                                          bool involuntary = false,
                                          bool temp = true);

bool form_can_wield(transformation form = you.form);
bool form_can_wear(transformation form = you.form);
bool form_can_fly(transformation form = you.form);
bool form_can_swim(transformation form = you.form);
bool form_changes_physiology(transformation form = you.form);
// Does the form keep the benefits of resistance, scale, and aux mutations?
bool form_keeps_mutations(transformation form = you.form);
bool form_has_blood(transformation form = you.form);
bool form_has_hair(transformation form = you.form);
bool form_has_bones(transformation form = you.form);
bool form_has_feet(transformation form = you.form);
bool form_has_ears(transformation form = you.form);

bool feat_dangerous_for_form(transformation which_trans,
                             dungeon_feature_type feat);

bool check_form_stat_safety(transformation new_form, bool quiet = false);

string cant_transform_reason(transformation which_trans, bool involuntary = false,
                             bool temp = true);
bool check_transform_into(transformation which_trans, bool involuntary = false);
bool transform(int pow, transformation which_trans, bool involuntary = false);

// skip_move: don't make player re-enter current cell
void untransform(bool skip_move = false);

void unset_default_form();
void set_default_form(transformation t, const item_def *source);

void set_form(transformation which_trans, int dur);
void return_to_default_form();

void remove_one_equip(equipment_type eq, bool meld = true,
                      bool mutation = false);
void unmeld_one_equip(equipment_type eq);

monster_type transform_mons();
string blade_parts(bool terse = false);
const char* transform_name(transformation form = you.form);

void merfolk_check_swimming(dungeon_feature_type old_grid,
                            bool stepped = false);
void merfolk_start_swimming(bool step = false);
void merfolk_stop_swimming();
void vampire_update_transformations();
int form_base_movespeed(transformation tran);
bool draconian_dragon_exception();

transformation form_for_talisman(const item_def &talisman);

struct talisman_form_desc {
    vector<pair<string, string>> skills;
    vector<pair<string, string>> defenses;
    vector<pair<string, string>> offenses; // heh
};
void describe_talisman_form(transformation form_type, talisman_form_desc &d,
                            bool incl_special);

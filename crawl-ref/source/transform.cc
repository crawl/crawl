/**
 * @file
 * @brief Misc function related to player transformations.
**/

#include "AppHdr.h"

#include "transform.h"

#include <cstdio>
#include <cstring>

#include "ability.h"
#include "artefact.h"
#include "art-enum.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "english.h"
#include "env.h"
#include "equipment-slot.h"
#include "god-abil.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::resist_polymorph
#include "invent.h" // check_old_item_warning
#include "item-use.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "message.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-cast.h"
#include "spl-zap.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "timed-effects.h"
#include "traps.h"
#include "xom.h"

// List of valid monsters newly seen this turn for a sphinx to tell a riddle to.
vector<monster*> riddle_targs;

#define HAS_USED_DRAGON_TALISMAN_KEY "used_dragon_talisman"

// transform slot enums into flags
constexpr int SLOTF(equipment_slot s) {
    return 1 << s;
}

static constexpr int EQF_NONE = 0;

// Weapons and offhand items
static constexpr int EQF_HELD = SLOTF(SLOT_WEAPON) | SLOTF(SLOT_OFFHAND)
                                | SLOTF(SLOT_WEAPON_OR_OFFHAND);
// auxen
static constexpr int EQF_AUXES = SLOTF(SLOT_GLOVES) | SLOTF(SLOT_BOOTS)
                                 | SLOTF(SLOT_BARDING)
                                 | SLOTF(SLOT_CLOAK) | SLOTF(SLOT_HELMET);
// core body slots (statue form)
static constexpr int EQF_STATUE = SLOTF(SLOT_GLOVES) | SLOTF(SLOT_BOOTS)
                                  | SLOTF(SLOT_BARDING)
                                  | SLOTF(SLOT_BODY_ARMOUR);
// everything you can (W)ear
static constexpr int EQF_WEAR = EQF_AUXES | SLOTF(SLOT_BODY_ARMOUR)
                            | SLOTF(SLOT_OFFHAND) | SLOTF(SLOT_WEAPON_OR_OFFHAND);
// everything but jewellery
static constexpr int EQF_PHYSICAL = EQF_HELD | EQF_WEAR;
// just rings
static constexpr int EQF_RINGS = SLOTF(SLOT_RING);
// all jewellery
static constexpr int EQF_JEWELLERY = EQF_RINGS | SLOTF(SLOT_AMULET);
// everything
static constexpr int EQF_ALL = EQF_PHYSICAL | EQF_JEWELLERY;

string Form::melding_description(bool itemized) const
{
    vector<string> tags;

    if (!itemized)
    {
        // this is a bit rough and ready...
        if (blocked_slots == EQF_ALL)
            return "Your equipment is entirely melded.";
        else if (blocked_slots == EQF_PHYSICAL
                 && !(you.has_mutation(MUT_NO_ARMOUR) && you.has_mutation(MUT_NO_GRASPING)))
        {
            return "Your weapons and armour are melded.";
        }
        else
        {
            for (int i = SLOT_WEAPON; i < SLOT_GIZMO; ++i)
            {
                equipment_slot slot = static_cast<equipment_slot>(i);
                if (testbits(blocked_slots, SLOTF(slot))
                    && you.equipment.num_slots[slot] > 0)
                {
                    tags.emplace_back(lowercase_string(equip_slot_name(slot)));
                }
            }
            if (!tags.empty())
            {
                if (testbits(blocked_slots, EQF_AUXES))
                    return "Your auxiliary armour is melded.";
                else if (tags.size() > 5)
                    return "Your equipment is almost entirely melded";
                else
                {
                    return make_stringf("Your %s %s melded.",
                                        comma_separated_line(tags.begin(), tags.end()).c_str(),
                                        tags.size() > 1 ? "are" : "is");
                }
            }
        }
        // Nothing melded (for this player, anyway).
        return "";
    }

    if (blocked_slots == EQF_ALL)
        tags.emplace_back("All Equipment");
    else if (blocked_slots == EQF_PHYSICAL)
        tags.emplace_back("All Weapons and Armour");
    else
    {
        for (int i = SLOT_WEAPON; i < SLOT_GIZMO; ++i)
        {
            equipment_slot slot = static_cast<equipment_slot>(i);
            if (testbits(blocked_slots, SLOTF(slot)))
                tags.emplace_back(equip_slot_name(slot));
        }
    }

    return comma_separated_line(tags.begin(), tags.end(), ", ");
}

static const FormAttackVerbs DEFAULT_VERBS = FormAttackVerbs(nullptr, nullptr,
                                                             nullptr, nullptr);
static const FormAttackVerbs ANIMAL_VERBS = FormAttackVerbs("hit", "bite",
                                                            "maul", "maul");

// Class form_entry and the formdata array
#include "form-data.h"

static const form_entry &_find_form_entry(transformation form)
{
    for (const form_entry &entry : formdata)
        if (entry.tran == form)
            return entry;
    die("No formdata entry found for form %d", static_cast<int>(form));
}

Form::Form(const form_entry &fe)
    : short_name(fe.short_name), wiz_name(fe.wiz_name),
      min_skill(fe.min_skill), max_skill(fe.max_skill),
      hp_skill_penalty_mult(fe.hp_skill_penalty_mult),
      str_mod(fe.str_mod), dex_mod(fe.dex_mod), base_move_speed(fe.move_speed),
      blocked_slots(fe.blocked_slots), size(fe.size),
      can_cast(fe.can_cast),
      uc_colour(fe.uc_colour), uc_attack_verbs(fe.uc_attack_verbs),
      changes_anatomy(fe.changes_anatomy),
      changes_substance(fe.changes_substance),
      holiness(fe.holiness),
      is_badform(fe.is_badform),
      has_blood(fe.has_blood), has_hair(fe.has_hair),
      has_bones(fe.has_bones), has_feet(fe.has_feet),
      has_ears(fe.has_ears),
      shout_verb(fe.shout_verb),
      shout_volume_modifier(fe.shout_volume_modifier),
      hand_name(fe.hand_name), foot_name(fe.foot_name),
      flesh_equivalent(fe.flesh_equivalent),
      special_dice_name(fe.special_dice_name),
      long_name(fe.long_name), description(fe.description),
      resists(fe.resists), ac(fe.ac), ev(fe.ev), body_ac_mult(fe.body_ac_mult),
      unarmed_bonus_dam(fe.unarmed_bonus_dam),
      fakemuts(fe.fakemuts), badmuts(fe.badmuts),
      can_fly(fe.can_fly), can_swim(fe.can_swim), offhand_punch(fe.offhand_punch),
      uc_brand(fe.uc_brand), uc_attack(fe.uc_attack),
      prayer_action(fe.prayer_action), equivalent_mons(fe.equivalent_mons),
      hp_mod(fe.hp_mod), special_dice(fe.special_dice)
{ }

Form::Form(transformation tran)
    : Form(_find_form_entry(tran))
{ }
/**
 * Is the given equipment slot blocked by this form?
 *
 * @param slot      The equipment slot in question.
 * @return          True if this slot is fully blocked by this form.
 */
bool Form::slot_is_blocked(equipment_slot slot) const
{
    ASSERT_RANGE(slot, 0, NUM_EQUIP_SLOTS);
    return (1 << slot) & blocked_slots;
}

/**
 * Get the (capped) skill level the player has with this form.
 *
 * @param scale  A scaling factor to avoid integer rounding issues.
 * @return      The 'level' of the form.
 */
int Form::get_level(int scale) const
{
    return min(you.skill(SK_SHAPESHIFTING, scale), max_skill * scale);
}

/**
 * Get a verbose description for the form.
 *
 * @param past_tense     Whether the description should be in past or present
 *                       tense.
 * @return               A description for the form.
 */
string Form::get_description(bool past_tense) const
{
    return make_stringf("You %s %s",
                        past_tense ? "were" : "are",
                        get_transform_description().c_str());
}

/**
 * Get a message for transforming into this form, based on your current
 * situation (e.g. in water...)
 *
 * @return The message for turning into this form.
 */
string Form::transform_message() const
{
    return make_stringf("You turn into %s", get_transform_description().c_str());
}

/**
 * Get a message for untransforming from this form.
 *
 * @return "Your transform has ended."
 */
string Form::get_untransform_message() const
{
    return "Your transformation has ended.";
}

int Form::raw_scaling_value(const FormScaling &sc, int level) const
{
    const int scale = 100;

    if (sc.xl_based)
    {
        const int s = sc.scaling * you.experience_level * scale;
        return sc.base * scale + s / 27;
    }
    if (max_skill == min_skill)
        return sc.base * scale;

    const int lvl = level == -1 ? get_level(scale) : level * scale;
    const int over_min = lvl - min_skill * scale; // may be negative
    const int denom = max_skill - min_skill;

    return sc.base * scale + over_min * sc.scaling / denom;
}

int Form::scaling_value(const FormScaling &sc, int level, bool random, int divisor) const
{
    const int scaled_val = raw_scaling_value(sc, level);
    if (random)
        return div_rand_round(scaled_val, divisor);
    return scaled_val / divisor;
}

/**
 * What AC bonus does the player get while in this form?
 *
 * @param level The shapeshifting skill level to calculate this bonus for.
 *              (Default is -1, meaning 'Use the player's current skill')
 *
 * @return  The AC bonus currently granted by the form, multiplied by 100 to
 *          allow for pseudo-decimal flexibility (& to match
 *          player::armour_class())
 */
int Form::get_ac_bonus(int skill) const
{
    return max(0, scaling_value(ac, skill, false, 1));
}


/**
 * What EV bonus does the player get while in this form?
 *
 * @param level The shapeshifting skill level to calculate this bonus for.
 *              (Default is -1, meaning 'Use the player's current skill')
 *
 * @return  The EV bonus currently granted by the form, multiplied by 100 to
 *          allow for pseudo-decimal flexibility and to match the scale used
 *          in the player evasion calculation.
 */
int Form::ev_bonus(int skill) const
{
    return max(0, scaling_value(ev, skill, false, 1));
}

/**
 * What percentile modifier to base body armour AC does the player get while
 * in this form?
 *
 * @param level The shapeshifting skill level to calculate this bonus for.
 *              (Default is -1, meaning 'Use the player's current skill')
 *
 * @return  A percentile bonus/penalty to base body armour AC. (ie: 0 is
 *          equivalent to 'no change', while '20' is '+20% body armour AC' and
 *          '-20' is '-20% body armour AC')
 */
int Form::get_body_ac_mult(int skill) const
{
    return max(-100, scaling_value(body_ac_mult, skill));
}

int Form::get_base_unarmed_damage(bool random, int skill) const
{
    // All forms start with base 3 UC damage.
    return 3 + max(0, scaling_value(unarmed_bonus_dam, skill, random));
}

bool Form::can_offhand_punch() const
{
    if (offhand_punch == FC_ENABLE)
        return true;
    else if (offhand_punch == FC_FORBID)
        return false;
    else
        return can_wield();
}

/// `force_talisman` means to calculate HP as if we were in a talisman form (i.e. with penalties with insufficient Shapeshifting skill),
/// without checking whether we actually are.
int Form::mult_hp(int base_hp, bool force_talisman, int skill) const
{
    const int scale = 100;
    const int lvl = skill == -1 ? get_level(scale) : skill * scale;
    // Only penalize if you're in a talisman/bauble form with insufficient skill.
    const int shortfall = (min_skill * scale - lvl) * hp_skill_penalty_mult;
    const bool should_downscale = force_talisman
                                  || you.default_form == you.form
                                  || you.form == transformation::flux;

    if (shortfall <= 0 || !should_downscale)
        return hp_mod * base_hp / 100;
    // -10% hp per skill level short, down to -90%
    const int penalty = min(shortfall, 9 * scale);
    return base_hp * hp_mod * (10 * scale - penalty) / (scale * 100 * 10);
}

/**
 * What is the damage of some form-specific specify ability or passive used
 * by this form (eg: Blinkbolt damage for Storm or contam damage for Flux)?
 *
 * @param random    Whether to randomly divide power or round down.
 * @param skill     Shapeshifting skill to use (default of -1 to use the
 *                  player's current skill with this form).
 *
 * @return The damage dice used by this form's special action.
 */
dice_def Form::get_special_damage(bool random, int skill) const
{
    if (skill == -1)
        skill = get_level(1);

    if (special_dice)
    {
        dice_def dmg = (*special_dice)(skill, random);
        if (dmg.size <= 0)
            dmg.size = 1;
        return dmg;
    }
    else
        return dice_def();
}

/**
 * (freeze)
 */
static string _brand_suffix(brand_type brand)
{
    if (brand == SPWPN_NORMAL)
        return "";
    return make_stringf(" (%s)", brand_type_name(brand, true));
}

/**
 * What name should be used for the player's means of unarmed attack while
 * in this form?
 *
 * (E.g. for display in the top-right of the UI.)
 *
 * @param   The player's UC weapon when not in a form (claws, etc)
 * @return  A string describing the form's UC attack 'weapon'.
 */
string Form::get_uc_attack_name(string default_name) const
{
    const string brand_suffix = _brand_suffix(get_uc_brand());
    if (uc_attack.empty())
        return default_name + brand_suffix;
    return uc_attack + brand_suffix;
}

/**
 * How many levels of resistance against fire does this form provide?
 */
int Form::res_fire() const
{
    return get_resist(resists, MR_RES_FIRE);
}

/**
 * How many levels of resistance against cold does this form provide?
 */
int Form::res_cold() const
{
    return get_resist(resists, MR_RES_COLD);
}

/**
 * How many levels of resistance against negative energy does this form give?
 */
int Form::res_neg() const
{
    return get_resist(resists, MR_RES_NEG);
}

/**
 * Does this form provide resistance to electricity?
 */
bool Form::res_elec() const
{
    return get_resist(resists, MR_RES_ELEC);
}

/**
 * How many levels of resistance against poison does this form give?
 */
int Form::res_pois() const
{
    return get_resist(resists, MR_RES_POISON);
}

/**
 * Does this form provide resistance to miasma?
 */
bool Form::res_miasma() const
{
    return get_resist(resists, MR_RES_MIASMA);
}

/**
 * Does this form provide resistance against acid?
 */
bool Form::res_corr() const
{
    return get_resist(resists, MR_RES_CORR);
}

/**
 * Does this form provide resistance to petrification?
 */
bool Form::res_petrify() const
{
    return get_resist(resists, MR_RES_PETRIFY);
}

/**
 * Does this form enable flight?
 *
 * @return Whether this form allows flight for characters which don't already
 *         have access to it.
 */
bool Form::enables_flight() const
{
    return can_fly == FC_ENABLE;
}

/**
 * Does this form disable flight?
 *
 * @return Whether flight is always impossible while in this form.
 */
bool Form::forbids_flight() const
{
    return can_fly == FC_FORBID;
}

/**
 * Can the player fly, if in this form?
 *
 * DOES consider player state besides form.
 * @return  Whether the player will be able to fly in this form.
 */
bool Form::player_can_fly() const
{
    return !forbids_flight()
           && (enables_flight()
               || you.racial_permanent_flight()); // XX other cases??
}

/**
 * Can the player swim, if in this form?
 *
 * DOES consider player state besides form.
 * @return  Whether the player will be able to swim in this form.
 */
bool Form::player_can_swim() const
{
    // XX this is kind of a mess w.r.t. player::can_swim
    const size_type player_size = size == SIZE_CHARACTER ?
                                          you.body_size(PSIZE_BODY, true) :
                                          size;
    return can_swim == FC_ENABLE
           || species::can_swim(you.species)
              && can_swim != FC_FORBID
           || player_size >= SIZE_GIANT;
}

/**
 * What message should be printed when the player prays at an altar?
 * To be inserted into "You %s the altar of foo."
 *
 * If the form has a valid custom action, print that; otherwise, default to the
 * 'flying' or species-specific actions, as appropriate.
 *
 * @return  An action to be printed when the player prays at an altar.
 *          E.g., "perch on", "crawl onto", "sway towards", etc.
 */
string Form::player_prayer_action() const
{
    // If the form is naturally flying & specifies an action, use that.
    if (can_fly == FC_ENABLE && !prayer_action.empty())
        return prayer_action;
    // Otherwise, if you're flying, use the generic flying action.
    // XXX: if we ever get a default-permaflying species again that wants to
    // have a separate verb, we'll want to check for that right here.
    if (you.airborne())
        return "hover solemnly before";
    // Otherwise, if you have a verb, use that...
    if (!prayer_action.empty())
        return prayer_action;
    // Finally, default to your species' verb.
    return species::prayer_action(you.species);
}

vector<pair<string, string>> Form::get_fakemuts() const
{
    return fakemuts;
}

vector<pair<string, string>> Form::get_bad_fakemuts() const
{
    return badmuts;
}


class FormNone : public Form
{
private:
    FormNone() : Form(transformation::none) { }
    DISALLOW_COPY_AND_ASSIGN(FormNone);
public:
    static const FormNone &instance() { static FormNone inst; return inst; }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override { return "your old self."; }
};

class FormSpider : public Form
{
private:
    FormSpider() : Form(transformation::spider) { }
    DISALLOW_COPY_AND_ASSIGN(FormSpider);
public:
    static const FormSpider &instance() { static FormSpider inst; return inst; }

    int get_web_chance(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(20).Scaling(20), skill);
    }
};

class FormFlux : public Form
{
private:
    FormFlux() : Form(transformation::flux) { }
    DISALLOW_COPY_AND_ASSIGN(FormFlux);
public:
    static const FormFlux &instance() { static FormFlux inst; return inst; }

    string get_description(bool past_tense) const override
    {
        return make_stringf("You %s overflowing with transmutational energy.",
                            past_tense ? "were" : "are");
    }

    string transform_message() const override
    {
        return "Your body destabilises.";
    }

    string get_untransform_message() const override
    {
        return "Your body stabilises again.";
    }
};

class FormBlade : public Form
{
private:
    FormBlade() : Form(transformation::blade) { }
    DISALLOW_COPY_AND_ASSIGN(FormBlade);
public:
    static const FormBlade &instance() { static FormBlade inst; return inst; }

    /**
     * @ description
     */
    string get_description(bool past_tense) const override
    {
        return make_stringf("You %s blades growing out of your body.",
                            past_tense ? "had" : "have");
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
        return "Blades grow out of your body!";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        return "Your blades shrink back into your body and disappear.";
    }

    int get_aux_damage(bool random, int skill) const override
    {
        return scaling_value(FormScaling().Base(10).Scaling(6), skill, random);
    }

    // Base parrying bonus
    int get_effect_size(int skill = -1) const override
    {
        return max(0, scaling_value(FormScaling().Base(6).Scaling(6), skill));
    }
};

class FormStatue : public Form
{
private:
    FormStatue() : Form(transformation::statue) { }
    DISALLOW_COPY_AND_ASSIGN(FormStatue);
public:
    static const FormStatue &instance() { static FormStatue inst; return inst; }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DEEP_DWARF && one_chance_in(10))
            return "You inwardly fear your resemblance to a lawn ornament.";
#endif
        return Form::transform_message();
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override
    {
        return "a living statue of rough stone.";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        return "You revert to your normal fleshy form.";
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        // there's special casing in base_hand_name to get "fists"
        string hand = you.base_hand_name(true, true);
        return make_stringf("Stone %s", hand.c_str());
    }
};

class FormSerpent : public Form
{
private:
    FormSerpent() : Form(transformation::serpent) { }
    DISALLOW_COPY_AND_ASSIGN(FormSerpent);
public:
    static const FormSerpent &instance() { static FormSerpent inst; return inst; }

    vector<pair<string, string>> get_fakemuts() const override
    {
        // Don't claim felids can wear two hats
        if (you.has_mutation(MUT_NO_ARMOUR))
            return vector<pair<string, string>>({fakemuts[0]});

        return fakemuts;
    }
};

class FormDragon : public Form
{
private:
    FormDragon() : Form(transformation::dragon) { }
    DISALLOW_COPY_AND_ASSIGN(FormDragon);
public:
    static const FormDragon &instance() { static FormDragon inst; return inst; }

    /**
     * Get an monster type corresponding to the transformation.
     *
     * (Used for console player glyphs.)
     *
     * @return  A monster type corresponding to the player in this form.
     */
    monster_type get_equivalent_mons() const override
    {
        return species::dragon_form(you.species);
    }

    string get_transform_description() const override
    {
        if (species::is_draconian(you.species))
        {
            return make_stringf("a fearsome %s!",
                          mons_class_name(get_equivalent_mons()));
        }
        else
            return description;
    }

    /**
     * The AC bonus of the form, multiplied by 100 to match
     * player::armour_class().
     */
    int get_ac_bonus(int skill = -1) const override
    {
        const int normal = Form::get_ac_bonus(skill);
        if (!species::is_draconian(you.species))
            return normal;
        return normal - 600;
    }
    /**
     * How many levels of resistance against fire does this form provide?
     */
    int res_fire() const override
    {
        switch (species::dragon_form(you.species))
        {
            case MONS_GOLDEN_DRAGON:
                return 1;
            default:
                return 0;
        }
    }

    /**
     * How many levels of resistance against cold does this form provide?
     */
    int res_cold() const override
    {
        switch (species::dragon_form(you.species))
        {
            case MONS_GOLDEN_DRAGON:
                return 1;
            default:
                return 0;
        }
    }

    // Note that this is only used for UI purposes. The actual breath weapons
    // calculate their damage from draconian_breath_power() directly.
    dice_def get_special_damage(bool random = true, int skill = -1) const override
    {
        ability_type abil = species::draconian_breath(you.species);
        spell_type spell = abil == ABIL_NON_ABILITY ? SPELL_GOLDEN_BREATH
                                                    : draconian_breath_to_spell(abil);

        const zap_type zap = spell_to_zap(spell);

        if (skill == -1)
            skill = get_level(1);

        if (spell == SPELL_COMBUSTION_BREATH)
            return combustion_breath_damage(draconian_breath_power(skill), random);
        else
            return zap_damage(zap, draconian_breath_power(skill), false, random);
    }
};

class FormDeath : public Form
{
private:
    FormDeath() : Form(transformation::death) { }
    DISALLOW_COPY_AND_ASSIGN(FormDeath);
public:
    static const FormDeath &instance() { static FormDeath inst; return inst; }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
        return "Your flesh twists and warps into a mockery of life!";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        return "You feel yourself come back to life.";
    }

    int will_bonus() const override { return WL_PIP; }
};

class FormBat : public Form
{
private:
    FormBat() : Form(transformation::bat) { }
    DISALLOW_COPY_AND_ASSIGN(FormBat);
public:
    static const FormBat &instance() { static FormBat inst; return inst; }

    string get_untransform_message() const override { return "You feel less batty."; }
};

class FormPig : public Form
{
private:
    FormPig() : Form(transformation::pig) { }
    DISALLOW_COPY_AND_ASSIGN(FormPig);
public:
    static const FormPig &instance() { static FormPig inst; return inst; }
    string get_untransform_message() const override { return "You feel less porcine."; }
};

#if TAG_MAJOR_VERSION == 34
class FormAppendage : public Form
{
private:
    FormAppendage() : Form(transformation::appendage) { }
    DISALLOW_COPY_AND_ASSIGN(FormAppendage);
public:
    static const FormAppendage &instance()
    {
        static FormAppendage inst;
        return inst;
    }
};
#endif

class FormTree : public Form
{
private:
    FormTree() : Form(transformation::tree) { }
    DISALLOW_COPY_AND_ASSIGN(FormTree);
public:
    static const FormTree &instance() { static FormTree inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override { return "You feel less wooden."; }
};

#if TAG_MAJOR_VERSION == 34
class FormPorcupine : public Form
{
private:
    FormPorcupine() : Form(transformation::porcupine) { }
    DISALLOW_COPY_AND_ASSIGN(FormPorcupine);
public:
    static const FormPorcupine &instance()
    {
        static FormPorcupine inst;
        return inst;
    }
};
#endif

class FormWisp : public Form
{
private:
    FormWisp() : Form(transformation::wisp) { }
    DISALLOW_COPY_AND_ASSIGN(FormWisp);
public:
    static const FormWisp &instance() { static FormWisp inst; return inst; }
    string get_untransform_message() const override { return "You condense into your normal self."; }
};

class FormJelly : public Form
{
private:
    FormJelly() : Form(transformation::jelly) { }
    DISALLOW_COPY_AND_ASSIGN(FormJelly);
public:
    static const FormJelly &instance() { static FormJelly inst; return inst; }
};

class FormFungus : public Form
{
private:
    FormFungus() : Form(transformation::fungus) { }
    DISALLOW_COPY_AND_ASSIGN(FormFungus);
public:
    static const FormFungus &instance() { static FormFungus inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override { return "You stop sporulating."; }
};

#if TAG_MAJOR_VERSION == 34
class FormShadow : public Form
{
private:
    FormShadow() : Form(transformation::shadow) { }
    DISALLOW_COPY_AND_ASSIGN(FormShadow);
public:
    static const FormShadow &instance() { static FormShadow inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        if (you.invisible())
            return "You feel less shadowy.";
        return "You emerge from the shadows.";
    }
};
#endif

class FormStorm : public Form
{
private:
    FormStorm() : Form(transformation::storm) { }
    DISALLOW_COPY_AND_ASSIGN(FormStorm);
public:
    static const FormStorm &instance() { static FormStorm inst; return inst; }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        // there's special casing in base_hand_name to get "fists"
        string hand = you.base_hand_name(true, true);
        return make_stringf("Storm %s", hand.c_str());
    }
};

class FormQuill : public Form
{
private:
    FormQuill() : Form(transformation::quill) { }
    DISALLOW_COPY_AND_ASSIGN(FormQuill);
public:
    static const FormQuill &instance() { static FormQuill inst; return inst; }

    string transform_message() const override
    {
        return "Sharp quills grow all over your body.";
    }

    string get_untransform_message() const override
    {
        return "Your quills recede back into your body.";
    }
};

class FormMaw : public Form
{
private:
    FormMaw() : Form(transformation::maw) { }
    DISALLOW_COPY_AND_ASSIGN(FormMaw);
public:
    static const FormMaw &instance() { static FormMaw inst; return inst; }

    int get_aux_damage(bool random, int skill) const override
    {
        return scaling_value(FormScaling().Base(8).Scaling(6), skill, random)
                    + (random ? div_rand_round(you.strength() * 3, 4)
                              : you.strength() * 3 / 4);
    }

    // Engorged regeneration rate
    int get_effect_size(int skill = -1) const override
    {
        return max(0, scaling_value(FormScaling().Base(220).Scaling(280), skill));
    }
};

#if TAG_MAJOR_VERSION == 34

class FormHydra : public Form
{
private:
    FormHydra() : Form(transformation::hydra) { }
    DISALLOW_COPY_AND_ASSIGN(FormHydra);
public:
    static const FormHydra &instance() { static FormHydra inst; return inst; }
};
#endif

class FormSlaughter : public Form
{
private:
    FormSlaughter() : Form(transformation::slaughter) { }
    DISALLOW_COPY_AND_ASSIGN(FormSlaughter);
public:
    static const FormSlaughter &instance() { static FormSlaughter inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        return "Makhleb calls their payment due...";
    }

    /**
     * % screen description
     */
    string get_long_name() const override
    {
        const int boost = you.props[MAKHLEB_SLAUGHTER_BOOST_KEY].get_int();
        return make_stringf("vessel of slaughter (+%d%% damage done)", boost);
    }
};

class FormVampire : public Form
{
private:
    FormVampire() : Form(transformation::vampire) { }
    DISALLOW_COPY_AND_ASSIGN(FormVampire);
public:
    static const FormVampire &instance() { static FormVampire inst; return inst; }

    // Bat swarm recharge rate
    int get_effect_size(int skill = -1) const override
    {
        return max(50, scaling_value(FormScaling().Base(100).Scaling(100), skill));
    }

    // Daze power
    int get_effect_chance(int skill = -1) const override
    {
        return max(0, scaling_value(FormScaling().Base(70).Scaling(75), skill));
    }
};

class FormBatswarm : public Form
{
private:
    FormBatswarm() : Form(transformation::bat_swarm) { }
    DISALLOW_COPY_AND_ASSIGN(FormBatswarm);
public:
    static const FormBatswarm &instance() { static FormBatswarm inst; return inst; }
};

class FormRimeYak : public Form
{
private:
FormRimeYak() : Form(transformation::rime_yak) { }
    DISALLOW_COPY_AND_ASSIGN(FormRimeYak);
public:
    static const FormRimeYak &instance() { static FormRimeYak inst; return inst; }
};

class FormHive : public Form
{
private:
FormHive() : Form(transformation::hive) { }
    DISALLOW_COPY_AND_ASSIGN(FormHive);
public:
    static const FormHive &instance() { static FormHive inst; return inst; }

    int regen_bonus(int skill = -1) const override
    {
        return max(0, scaling_value(FormScaling().Base(160).Scaling(120), skill));
    }

    int mp_regen_bonus(int skill = -1) const override
    {
        return max(0, scaling_value(FormScaling().Base(60).Scaling(40), skill));
    }

    // Number of bees created (x10)
    int get_effect_size(int skill = -1) const override
    {
        return max(10, scaling_value(FormScaling().Base(32).Scaling(23), skill));
    }
};

class FormWater : public Form
{
private:
FormWater() : Form(transformation::aqua) { }
    DISALLOW_COPY_AND_ASSIGN(FormWater);
public:
    static const FormWater &instance() { static FormWater inst; return inst; }

    string get_description(bool past_tense) const override
    {
        return make_stringf("Your body %s made of elemental water.",
                            past_tense ? "was" : "is");
    }

    string transform_message() const override
    {
        return "Your body transforms into elemental water.";
    }

    string get_untransform_message() const override
    {
        return "Your body returns to its normal shape and substance.";
    }
};

class FormSphinx : public Form
{
private:
FormSphinx() : Form(transformation::sphinx) { }
    DISALLOW_COPY_AND_ASSIGN(FormSphinx);
public:
    static const FormSphinx &instance() { static FormSphinx inst; return inst; }

    int will_bonus() const override { return WL_PIP; }
};

dice_def player_airstrike_melee_damage(int open_spaces, int skill)
{
    if (skill == -1)
        skill = FormSphinx::instance().get_level(1);
    return dice_def(1 + open_spaces / 2, 1 + skill * 5 / 7);
}

class FormWerewolf : public Form
{
private:
FormWerewolf() : Form(transformation::werewolf) { }
    DISALLOW_COPY_AND_ASSIGN(FormWerewolf);
public:
    static const FormWerewolf &instance() { static FormWerewolf inst; return inst; }

    int will_bonus() const override { return -WL_PIP; }

    int regen_bonus(int /*skill*/ = -1) const override { return REGEN_PIP; }

    // Amount of slaying gained per kill (multiplied by 10). 50% more for initial kill.
    int get_werefury_kill_bonus(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(12).Scaling(10), skill);
    }

    virtual int get_takedown_multiplier(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(75).Scaling(50), skill);
    }

    virtual int get_howl_power(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(80).Scaling(40), skill);
    }
};

class FormWalkingScroll : public Form
{
private:
FormWalkingScroll() : Form(transformation::walking_scroll) { }
    DISALLOW_COPY_AND_ASSIGN(FormWalkingScroll);
public:
    static const FormWalkingScroll &instance() { static FormWalkingScroll inst; return inst; }

    int max_mp_bonus(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(4).Scaling(5), skill);
    }
};

int walking_scroll_skill_bonus(int scale, int skill)
{
    int scaled_skill = skill == -1 ? FormWalkingScroll::instance().get_level(10) : skill * 10;
    return (10 + scaled_skill) * scale / 20;
}

class FormFortressCrab : public Form
{
private:
FormFortressCrab() : Form(transformation::fortress_crab) { }
    DISALLOW_COPY_AND_ASSIGN(FormFortressCrab);
public:
    static const FormFortressCrab &instance() { static FormFortressCrab inst; return inst; }

    // Number of clouds placed
    int get_effect_size(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(9).Scaling(16), skill);
    }
};

class FormSunScarab : public Form
{
private:
    FormSunScarab() : Form(transformation::sun_scarab) { }
    DISALLOW_COPY_AND_ASSIGN(FormSunScarab);
public:
    static const FormSunScarab &instance() { static FormSunScarab inst; return inst; }

};

class FormMedusa : public Form
{
private:
FormMedusa() : Form(transformation::medusa) { }
    DISALLOW_COPY_AND_ASSIGN(FormMedusa);
public:
    static const FormMedusa &instance() { static FormMedusa inst; return inst; }

    string transform_message() const override
    {
        return "A mane of stinging tendrils grows from your head.";
    }

    string get_untransform_message() const override
    {
        return "Your tendrils shrivel away.";
    }

    string get_description(bool past_tense) const override
    {
        return make_stringf("You %s a mane of long, stinging tendrils on your head.",
                            past_tense ? "had" : "have");
    }

    // Number of monsters affected by tendrils per attack (multiplied by 10,
    // so that it can start at 2.5)
    int get_effect_size(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(25).Scaling(15), skill);
    }

    // Chance of lithotoxin petrification.
    int get_effect_chance(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(55).Scaling(15), skill);
    }
};

class FormEelHands : public Form
{
private:
    FormEelHands() : Form(transformation::eel_hands) { }
    DISALLOW_COPY_AND_ASSIGN(FormEelHands);
public:
    static const FormEelHands &instance() { static FormEelHands inst; return inst; }

    /**
     * % screen description
     */
    string get_long_name() const override
    {
        return you.base_hand_name(true, true);
    }

    /**
     * @ description
     */
    string get_description(bool past_tense) const override
    {
        return make_stringf("You %s %s for %s.",
                            past_tense ? "had" : "have",
                            you.arm_count() == 1 ? "an electric eel" : "electric eels",
                            hand_transform_parts().c_str());
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
        const bool singular = you.arm_count() == 1;

        // XXX: a little ugly
        return make_stringf("Your %s turn%s into%s wriggling electric eel%s!",
                            hand_transform_parts().c_str(), singular ? "s" : "",
                            singular ? " a" : " a pair of", singular ? "" : "s");
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        const bool singular = you.arm_count() == 1;

        // XXX: a little ugly
        return make_stringf("Your %s revert%s to %s normal form.",
                            hand_transform_parts().c_str(), singular ? "s" : "",
                            singular ? "its" : "their");
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        return "Eel " + hand_transform_parts(true);
    }
};

class FormSpore : public Form
{
private:
    FormSpore() : Form(transformation::spore) { }
    DISALLOW_COPY_AND_ASSIGN(FormSpore);
public:
    static const FormSpore &instance() { static FormSpore inst; return inst; }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
        return make_stringf("Dense mycelia sprout from your %s and %s.",
                            you.arm_name(false).c_str(),
                            you.foot_name(true).c_str());
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        return make_stringf("Your mycelia shrivel away.");
    }

    string get_description(bool past_tense) const override
    {
        return make_stringf("Your %s %s a mass of colorful fungus.",
                            you.arm_name(false).c_str(),
                            past_tense ? "was" : "is");
    }
};

static const Form* forms[] =
{
    &FormNone::instance(),
    &FormSpider::instance(),
    &FormBlade::instance(),
    &FormStatue::instance(),

    &FormSerpent::instance(),
    &FormDragon::instance(),
    &FormDeath::instance(),
    &FormBat::instance(),

    &FormPig::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormAppendage::instance(),
#endif
    &FormTree::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormPorcupine::instance(),
#endif

    &FormWisp::instance(),
    &FormJelly::instance(),
    &FormFungus::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormShadow::instance(),
    &FormHydra::instance(),
#endif
    &FormStorm::instance(),
    &FormQuill::instance(),
    &FormMaw::instance(),
    &FormFlux::instance(),
    &FormSlaughter::instance(),
    &FormVampire::instance(),
    &FormBatswarm::instance(),
    &FormRimeYak::instance(),
    &FormHive::instance(),
    &FormWater::instance(),
    &FormSphinx::instance(),
    &FormWerewolf::instance(),
    &FormWalkingScroll::instance(),
    &FormFortressCrab::instance(),
    &FormSunScarab::instance(),
    &FormMedusa::instance(),
    &FormEelHands::instance(),
    &FormSpore::instance(),
};

const Form* get_form(transformation xform)
{
    COMPILE_CHECK(ARRAYSZ(forms) == NUM_TRANSFORMS);
    const int form = static_cast<int>(xform);
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form];
}

const Form* cur_form(bool temp)
{
    if (temp)
        return get_form();
    return get_form(you.default_form);
}


/**
 * Get the wizmode name of a form.
 *
 * @param form      The form in question.
 * @return          The form's casual, wizmode name.
 */
const char* transform_name(transformation form)
{
    return get_form(form)->wiz_name.c_str();
}

/**
 * Can the player (w)ield weapons when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wield items when in that form.
*/
bool form_can_wield(transformation form)
{
    return get_form(form)->can_wield();
}

/**
 * Can the player (W)ear armour when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wear armour when in that form.
 */
bool form_can_wear(transformation form)
{
    return !testbits(get_form(form)->blocked_slots, EQF_WEAR);
}

/**
 * Can the player fly, if in this form?
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to fly in this form.
 */
bool form_can_fly(transformation form)
{
    return get_form(form)->player_can_fly();
}

/**
 * Can the player swim, if in this form?
 *
 * (Swimming = traversing deep & shallow water without penalties; includes
 * swimming (snake form) and wading forms (giants - currently just dragon form,
 * which normally flies anyway...))
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to swim in this form.
 */
bool form_can_swim(transformation form)
{
    return get_form(form)->player_can_swim();
}

bool form_is_bad(transformation form)
{
    return get_form(form)->is_badform;
}

// Used to mark transformations which change the basic matter the player is
// made up of (ie: statue/storm form)
bool form_changes_substance(transformation form)
{
    return get_form(form)->changes_substance;
}

// Used to mark forms which have a significantly different body plan and
// thus suppress most anatomy-based mutations.
bool form_changes_anatomy(transformation form)
{
    return get_form(form)->changes_anatomy;
}

/**
 * Does this form have blood?
 *
 * @param form      The form in question.
 * @return          Whether the form has blood (can bleed, sublime, etc.).
 */
bool form_has_blood(transformation form)
{
    form_capability result = get_form(form)->has_blood;

    if (result == FC_ENABLE)
        return true;
    else if (result == FC_FORBID)
        return false;
    else
        return species::has_blood(you.species);
}

/**
 * Does this form have hair?
 *
 * @param form      The form in question.
 * @return          Whether the form has hair.
 */
bool form_has_hair(transformation form)
{
    form_capability result = get_form(form)->has_hair;

    if (result == FC_ENABLE)
        return true;
    else if (result == FC_FORBID)
        return false;
    else
        return species::has_hair(you.species);
}

/**
 * Does this form have bones?
 *
 * @param form      The form in question.
 * @return          Whether the form has bones.
 */
bool form_has_bones(transformation form)
{
    form_capability result = get_form(form)->has_bones;

    if (result == FC_ENABLE)
        return true;
    else if (result == FC_FORBID)
        return false;
    else
        return species::has_bones(you.species);
}

/**
 * Does this form have feet?
 *
 * @param form      The form in question.
 * @return          Whether the form has feet.
 */
bool form_has_feet(transformation form)
{
    form_capability result = get_form(form)->has_feet;

    if (result == FC_ENABLE)
        return true;
    else if (result == FC_FORBID)
        return false;
    else
        return species::has_feet(you.species);
}

/**
 * Does this form have ears?
 *
 * @param form      The form in question.
 * @return          Whether the form has ears.
 */
bool form_has_ears(transformation form)
{
    form_capability result = get_form(form)->has_ears;

    if (result == FC_ENABLE)
        return true;
    else if (result == FC_FORBID)
        return false;
    else
        return species::has_ears(you.species);
}

/**
 * Get an monster type corresponding to the player's current form.
 *
 * (Used for console player glyphs.)
 *
 * @return  A monster type corresponding to the player in the form.
 */
monster_type transform_mons()
{
    return get_form()->get_equivalent_mons();
}

/**
 * What is the name of the player parts that will transform with an eel talisman?
 */
string hand_transform_parts(bool terse)
{
    // there's special casing in base_hand_name to use "eel" everywhere, so
    // use the non-temp name
    string str = you.base_hand_name(true, false);

    // creatures with paws (aka felids) have four paws, but only two of them transform.
    if (!terse && you.has_mutation(MUT_PAWS, false))
        str = "front " + str;
    else if (!terse && you.arm_count() > 2)
        str = "main " + str; // Op have four main tentacles

    if (you.arm_count() == 1)
        str = "a " + str;

    return str;
}

// Checks whether the player would be flying in a given form (possibly caused by
// using a given talisman).
static bool _flying_in_new_form(transformation which_trans, const item_def* talisman)
{
    if (get_form(which_trans)->forbids_flight())
        return false;

    // sources of permanent flight besides equipment
    if (you.racial_permanent_flight())
        return true;

    // tempflight (e.g. from potion) enabled, no need for equip check
    if (you.duration[DUR_FLIGHT])
        return true;

    // We know for sure this can't get melded or fall off.
    if (which_trans != transformation::none && talisman && is_artefact(*talisman)
        && artefact_property(*talisman, ARTP_FLY))
    {
        return true;
    }

    // At this point, check the player's gear to see if anything they have can
    // theoretically grant flight (whether currently melded or not).
    bool has_flight_item = false;
    for (player_equip_entry& entry : you.equipment.items)
    {
        if (entry.is_overflow)
            continue;

        if (item_grants_flight(entry.get_item()))
        {
            has_flight_item = true;
            break;
        }
    }

    // If there's no relevant item, we can abort before doing a full melding sim.
    if (!has_flight_item)
        return false;

    // Finally, test actually transforming to see if there will be equip sources
    // left. This is a bit heavyweight, but should only be called when a player
    // tries to evoke a talisman (or a monster tries to polymorph you) and only
    // while it is possible that this could return true.
    //
    // It would be nice to have a simpler way to do this, but overflow items and
    // items which grant equipment slots can cause effects that are hard to
    // predict without actually simulating them.
    unwind_var<player_equip_set> unwind_eq(you.equipment);
    unwind_var<int8_t> unwind_talisman(you.cur_talisman);
    unwind_var<transformation> unwind_default_form(you.default_form);
    unwind_var<transformation> unwind_form(you.form);

    you.default_form = which_trans;
    you.form = which_trans;
    if (talisman)
    {
        ASSERT(in_inventory(*talisman));
        you.cur_talisman = talisman->link;
    }
    else
        you.cur_talisman = -1;

    you.equipment.unmeld_all_equipment(true);
    you.equipment.meld_equipment(get_form(which_trans)->blocked_slots, true);

    // Pretend incompatible items fell away.
    vector<item_def*> forced_remove = you.equipment.get_forced_removal_list();
    for (item_def* item : forced_remove)
        you.equipment.remove(*item);

    you.equipment.update();

    return you.equip_flight();
}

/**
 * Check if it'd be lethal for the player to enter a form in a given terrain.
 *
 * In addition to checking whether the feature is dangerous for the form
 * itself (form_likes_*), the function checks to see if the player is safe
 * due to flying or similar effects.
 *
 * @param which_trans       The form being checked.
 * @param feat              The dungeon feature to be checked for danger.
 * @param talisman          The talisman by which the player is entering this
 *                          form. (Can be nullptr if untransforming, or no
 *                          talisman is involved - polymorph, etc.)
 * @return                  If the feat is lethal for the player in the form.
 */
bool feat_dangerous_for_form(transformation which_trans,
                             dungeon_feature_type feat,
                             const item_def* talisman)
{
    // Only these terrain are potentially dangerous.
    if (feat != DNGN_DEEP_WATER && feat != DNGN_LAVA)
        return false;

    // Check for swimming first (since _flying_in_new_form is potentially
    // heavyweight if it has do a melding sim).
    if (feat == DNGN_DEEP_WATER && (you.can_water_walk() || form_can_swim(which_trans)))
        return false;

    // Beyond this, we require flight.
    return !form_can_fly(which_trans) && !_flying_in_new_form(which_trans, talisman);
}

/**
 * Checks if it would be unsafe for the player to transform into a specific form
 * at the present time (and prints an appropriate message, if so)
 */
bool transforming_is_unsafe(transformation which_trans)
{
    if (feat_dangerous_for_form(which_trans, env.grid(you.pos())))
    {
        mprf(MSGCH_PROMPT, "%s right now would cause you to %s!",
                which_trans == transformation::none ? "Untransforming" : "Transforming",
                env.grid(you.pos()) == DNGN_LAVA ? "burn" : "drown");
        return true;
    }

    // Now check if there are any items that would break if we changed form in
    // this way.
    unwind_var<player_equip_set> unwind_eq(you.equipment);
    unwind_var<transformation> unwind_default_form(you.default_form);
    unwind_var<transformation> unwind_form(you.form);

    you.default_form = which_trans;
    you.form = which_trans;

    you.equipment.unmeld_all_equipment(true);
    you.equipment.meld_equipment(get_form(which_trans)->blocked_slots, true);

    // Pretend incompatible items fell away.
    vector<item_def*> forced_remove = you.equipment.get_forced_removal_list(true);
    for (item_def* item : forced_remove)
    {
        // Now see if any of them would break if they did so.
        if (item->cursed()
            || (is_artefact(*item) && artefact_property(*item, ARTP_FRAGILE)))
        {
            mprf(MSGCH_PROMPT, "%s right now would shatter %s!",
                 which_trans == transformation::none ? "Untransforming" : "Transforming",
                 item->name(DESC_YOUR).c_str());
            return true;
        }
    }

    return false;
}

/**
 * Is the player alive enough to become the given form?
 *
 * All undead can use Vessel of Slaughter; vampires also can also use their
 * batform ability.
 *
 * @param which_trans   The transformation which the player is undergoing
 *                      (default you.form).
 * @return              True if the player is not blocked from entering the
 *                      given form by their undead race; false otherwise.
 */
bool lifeless_prevents_form(transformation which_trans)
{
    if (!you.undead_state(false)) // intentionally don't pass temp in here
        return false; // not undead!

    if (which_trans == transformation::none)
        return false; // everything can become itself

    if (which_trans == transformation::slaughter)
        return false; // Godly power can transcend such things as unlife

    return true;
}

/**
 * Is the player unable to enter the given form? If so, why?
 */
string cant_transform_reason(transformation which_trans,
                             bool involuntary, bool temp)
{
    if (!involuntary && you.has_mutation(MUT_NO_FORMS))
        return "You have sacrificed the ability to change form!";

    // the undead cannot enter most forms.
    if (lifeless_prevents_form(which_trans))
        return "Your unliving flesh cannot be transformed in this way.";

    if (SP_GARGOYLE == you.species && which_trans == transformation::statue)
        return "You're already a statue.";

    if (!temp)
        return "";

    if (you.transform_uncancellable && which_trans != transformation::slaughter)
        return "You are stuck in your current form!";

    if (which_trans == transformation::death && you.duration[DUR_DEATHS_DOOR])
        return "You cannot mock death while in death's door.";

    return "";
}

bool check_transform_into(transformation which_trans, bool involuntary,
                          const item_def* talisman)
{

    if (!involuntary && talisman && you.active_talisman()
            && !check_warning_inscriptions(*you.active_talisman(), OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return false;
    }
    if (!involuntary && talisman && you.active_talisman() != talisman
            && !check_warning_inscriptions(*talisman , OPER_PUTON))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const string reason = cant_transform_reason(which_trans, involuntary, true);
    if (!reason.empty())
    {
        if (!involuntary)
            mpr(reason);
        return false;
    }

    // XXX: This is left out of cant_transform_reason because it can involve
    //      melding and unwinding the player's entire inventory, which feels a
    //      bit heavyweight for item_is_useless()
    const auto feat = env.grid(you.pos());
    if (!involuntary && feat_dangerous_for_form(which_trans, feat, talisman))
    {
        mprf("Transforming right now would cause you to %s!",
             feat == DNGN_DEEP_WATER ? "drown" : "burn");
        return false;
    }

    if (!involuntary && get_form(which_trans)->mult_hp(100) < 90)
    {
        if (!yesno("This transformation would significantly lower your maximum hit points. "
                  "Transform anyway?", true, 'n'))
        {
            return false;
        }
    }

    return true;
}

static void _print_death_brand_changes(item_def *weapon, bool entering_death)
{
    if (!weapon
        || get_weapon_brand(*weapon) != SPWPN_HOLY_WRATH
        || you.undead_or_demonic(false))
    {
        return;
    }
    if (entering_death)
    {
        mprf("%s goes dull and lifeless in your grasp.",
             weapon->name(DESC_YOUR).c_str());
    }
    else
    {
        mprf("%s softly glows with a divine radiance!",
             uppercase_first(weapon->name(DESC_YOUR)).c_str());
    }
}

/// Form-specific special effects. Should be in a class?
static void _on_enter_form(transformation which_trans)
{
    // Extra effects
    switch (which_trans)
    {
    case transformation::tree:
        mpr("Your roots penetrate the ground.");
        if (you.duration[DUR_TELEPORT])
        {
            you.duration[DUR_TELEPORT] = 0;
            mpr("You feel strangely stable.");
        }
        you.duration[DUR_FLIGHT] = 0;
        break;

    case transformation::dragon:
        // The first time the player becomes a dragon, given them a charge of
        // their breath weapon so they can actually use them.
        if (!you.props.exists(HAS_USED_DRAGON_TALISMAN_KEY)
            && !species::is_draconian(you.species))
        {
            gain_draconian_breath_uses(1);
            you.props[HAS_USED_DRAGON_TALISMAN_KEY] = true;
        }
        break;

    case transformation::death:
        you.redraw_status_lights = true;
        _print_death_brand_changes(you.weapon(), true);
        _print_death_brand_changes(you.offhand_weapon(), true);
        break;

    case transformation::maw:
        if (have_passive(passive_t::goldify_corpses))
        {
            mprf(MSGCH_WARN, "Gozag's golden gift will leave your new mouth "
                             "with nothing to eat.");
        }
        break;

    case transformation::sun_scarab:
        sun_scarab_spawn_ember(true);
        break;

    // It's hard to tell what properties might be affected by armour doubling, so redraw all.
    case transformation::fortress_crab:
        notify_stat_change();
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        break;

    default:
        break;
    }
}

void set_form(transformation which_trans, int dur, bool scale_hp)
{
    you.form = which_trans;
    you.duration[DUR_TRANSFORMATION] = max(1, dur * BASELINE_DELAY);
    update_player_symbol();

    const int str_mod = get_form(which_trans)->str_mod;
    const int dex_mod = get_form(which_trans)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, dex_mod, true);

    calc_hp(scale_hp);
    calc_mp();

    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;
    quiver::set_needs_redraw();
}

static void _enter_form(int dur, transformation which_trans, bool using_talisman = true)
{
    const bool was_flying = you.airborne();

    // Give the transformation message.
    // (Vampire bat swarm ability skips this part.)
    if (!(you.form == transformation::vampire || you.form == transformation::bat_swarm)
         && !(which_trans == transformation::vampire || which_trans == transformation::bat_swarm))
    {
        mpr(get_form(which_trans)->transform_message());
    }

    // If we're wielding a two-hander, shift it into the crab two-hander slot
    // *before* melding gear (or it will be caught be melding offhand)
    if (which_trans == transformation::fortress_crab)
        you.equipment.shift_twohander_to_slot(SLOT_TWOHANDER_ONLY);

    // Update your status.
    // Order matters here, take stuff off (and handle attendant HP and stat
    // changes) before adjusting the player to be transformed.
    you.equipment.meld_equipment(get_form(which_trans)->blocked_slots, false);
    set_form(which_trans, dur, !using_talisman);

    if (you.digging && form_changes_anatomy(which_trans))
    {
        mpr("Your mandibles meld away.");
        you.digging = false;
    }

    _on_enter_form(which_trans);

    // Stop constricting, if appropriate. In principle, we could be switching
    // from one form that allows constricting to another, but that seems too
    // rare to justify the complexity. The confusion of changing forms makes
    // you lose your grip, or something.
    you.stop_directly_constricting_all();

    // Stop being constricted if we are now too large, or are now immune.
    if (you.constricted_type == CONSTRICT_MELEE)
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        ASSERT(constrictor);

        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY)
            || you.res_constrict())
        {
            you.stop_being_constricted();
        }
    }

    // If we are no longer living, end an effect that afflicts only the living
    if (you.duration[DUR_FLAYED] && !(you.holiness() & MH_NATURAL))
    {
        // Heal a little extra if we gained max hp from this transformation
        const int dam = you.props[FLAY_DAMAGE_KEY].get_int();
        const int mult_dam = get_form(which_trans)->mult_hp(dam);
        if (mult_dam > dam)
            you.heal(mult_dam - dam);
        heal_flayed_effect(&you);
    }

    // This only has an effect if the transformation happens passively,
    // for example if Xom decides to transform you while you're busy
    // running around.
    // If you're turned into a tree, you stop taking stairs.
    if (!using_talisman)
        stop_delay(which_trans == transformation::tree);

    if (crawl_state.which_god_acting() == GOD_XOM)
       you.transform_uncancellable = true;

    // Stop emergency flight if it's activated and this form can fly
    if (you.props[EMERGENCY_FLIGHT_KEY]
        && form_can_fly()
        && you.airborne())
    {
        you.props.erase(EMERGENCY_FLIGHT_KEY);
    }

    // Update merfolk swimming for the form change.
    if (you.has_innate_mutation(MUT_MERTAIL))
        merfolk_check_swimming(env.grid(you.pos()));

    // In the case where we didn't actually meld any gear (but possibly used
    // a new artefact talisman or were forcibly polymorphed away from one),
    // refresh equipment properties.
    you.equipment.update();
    if (which_trans == transformation::fortress_crab)
        calc_mp();

    if (using_talisman && is_artefact(*you.active_talisman()))
        equip_artefact_effect(*you.active_talisman(), nullptr, false);

    // Update flight status now (won't actually land the player if we're still flying).
    if (was_flying)
        land_player();

    // Update skill boosts for the current state of equipment melds
    // Must happen before the HP check!
    ash_check_bondage();

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("gaining the %s transformation",
                          transform_name(which_trans)).c_str());
    }
}

/**
 * Attempts to transform the player into the specified form.
 *
 * If the player is already in that form, attempt to refresh its duration and
 * power.
 *
 * @param dur               The duration of the transformation (0 if the
 *                          transformation should be permanent.)
 * @param which_trans       The form which the player should become.
 * @param involuntary       Checks for inscription warnings are skipped, and
 *                          failure is silent.
 * @param using_talisman    True if this transformation was caused by the player
 *                          evoking a talisman.
 * @return                  If the player was transformed, or if they were
 *                          already in the given form, returns true.
 *                          Otherwise, false.
 */
bool transform(int dur, transformation which_trans, bool involuntary,
               bool using_talisman)
{
    // Zin's protection.
    if (have_passive(passive_t::resist_polymorph)
        && x_chance_in_y(you.piety(), piety_breakpoint(5))
        && which_trans != transformation::none)
    {
        simple_god_message(" protects your body from unnatural transformation!");
        return false;
    }

    if (!involuntary && crawl_state.is_god_acting())
        involuntary = true;

    if (!check_transform_into(which_trans, involuntary,
                                using_talisman ? you.active_talisman() : nullptr))
    {
        return false;
    }

    // If swapping to a different talisman of the same type, make sure to
    // activate properties of the new one.
    if (using_talisman && you.form == which_trans
        && is_artefact(*you.active_talisman()))
    {
        you.equipment.update();
        equip_artefact_effect(*you.active_talisman(), nullptr, false);

        return true;
    }

    // Vampire should shift into bat swarm without reverting to fully untransformed in the middle
    if (you.form != transformation::none
        && !(you.form == transformation::vampire && which_trans == transformation::bat_swarm))
    {
        untransform(true, !using_talisman, !using_talisman, which_trans);
    }

    _enter_form(dur, which_trans, using_talisman);

    return true;
}

/**
 * End the player's transformation and return them to having no
 * form.
 * @param skip_move      If true, skip any move that was in progress before
 *                       the transformation ended.
 * @param scale_hp       Whether keep the player's HP percentage the same if
 *                       their max HP changes. (Should be false for
 *                       talisman-related shapeshifting, to prevent exploits
 *                       such as instantly healing via entering a -90% HP form
 *                       and then leaving it again immediately.)
 * @param preserve_equipment    True if incompatible equipment should be melded
 *                              instead of being unequipped (such as when
 *                              entering a temporary form from a talisman that
 *                              gave additional equipment slotsshifting).
 * @param new_form       If this untransform is being done in the process of
 *                       entering a new form, what form is that?
 */
void untransform(bool skip_move, bool scale_hp, bool preserve_equipment,
                 transformation new_form)
{
    // Skip if there's nothing that needs doing.
    if (you.form == transformation::none)
        return;

    const transformation old_form = you.form;
    const bool was_flying = you.airborne();

    if (!form_can_wield(old_form))
        you.received_weapon_warning = false;

    const string message = get_form(old_form)->get_untransform_message();
    if (!message.empty())
        mprf(MSGCH_DURATION, "%s", message.c_str());

    set_form(transformation::none, 0, scale_hp);

    const int str_mod = get_form(old_form)->str_mod;
    const int dex_mod = get_form(old_form)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, -str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, -dex_mod, true);

    // This will keep merfolk boots melded, if mertail is currently active.
    you.equipment.unmeld_all_equipment();

    if (old_form == transformation::fortress_crab)
        you.equipment.shift_twohander_to_slot(SLOT_OFFHAND);

    // Update regarding talisman properties, just in case we didn't actually
    // meld or unmeld anything.
    you.equipment.update();

    if (old_form == transformation::death)
    {
        _print_death_brand_changes(you.weapon(), false);
        _print_death_brand_changes(you.offhand_weapon(), false);
    }
    else if (old_form == transformation::sun_scarab)
    {
        if (monster* ember = get_solar_ember())
        {
            monster_die(*ember, KILL_RESET, NON_MONSTER);
            mprf(MSGCH_DURATION, "Your tiny sun winks out.");
        }
    }
    else if (old_form == transformation::rime_yak)
    {
        you.duration[DUR_RIME_YAK_AURA] = 0;
        end_terrain_change(TERRAIN_CHANGE_RIME_YAK);
    }
    else if (old_form == transformation::werewolf)
        you.duration[DUR_WEREFURY] = 0;
    else if (old_form == transformation::maw)
        you.duration[DUR_ENGORGED] = 0;
    else if (old_form == transformation::eel_hands)
        you.duration[DUR_EELJOLT_COOLDOWN] = 0;
    else if (old_form == transformation::fortress_crab)
    {
        calc_mp();
        notify_stat_change();
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }

    // If the player is no longer be eligible to equip some of the items that
    // they were wearing (possibly due to losing slots from their default form
    // changing), calculate that now. If they're outright exiting the form,
    // make them fall off. If they're entering a temporary form, meld them.
    // If they're returning back to the form that granted those slots in the
    // first place, do nothing.
    vector<item_def*> forced_remove = you.equipment.get_forced_removal_list(true);
    if (preserve_equipment && new_form != you.default_form)
        you.equipment.meld_equipment(forced_remove);
    else if (!preserve_equipment)
    {
        for (item_def* item : forced_remove)
        {
            mprf("%s falls away%s!", item->name(DESC_YOUR).c_str(),
                    item->cursed() ? ", shattering the curse!" : "");

            unequip_item(*item, false);
        }
    }

    // Update skill boosts for the current state of equipment melds
    // Must happen before the HP check!
    ash_check_bondage();

    // Not necessary for proper functioning, but printing a message feels appropriate.
    if (get_form(old_form)->forbids_flight() && you.airborne())
        float_player();

    if (!skip_move)
    {
        // Activate emergency flight, if we have to.
        if (is_feat_dangerous(env.grid(you.pos())))
            enable_emergency_flight();

        // Update merfolk swimming for the form change.
        if (you.has_innate_mutation(MUT_MERTAIL))
            merfolk_check_swimming(env.grid(you.pos()));
    }

#ifdef USE_TILE
    if (you.has_innate_mutation(MUT_MERTAIL))
        init_player_doll();
#endif

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("losing the %s form",
                          transform_name(old_form)).c_str());
    }

    // Stop being constricted if we are now too large.
    if (you.constricted_type == CONSTRICT_MELEE)
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }

    // Called so that artprop flight on talismans specifically ends properly.
    // (Won't land the player if anything else is keeping them afloat.)
    if (was_flying)
        land_player();

    you.turn_is_over = true;
    if (you.transform_uncancellable)
        you.transform_uncancellable = false;

    if (old_form == transformation::slaughter)
        makhleb_enter_crucible_of_flesh(15);

    if (old_form == transformation::sphinx)
        riddle_targs.clear();
}

void return_to_default_form(bool new_form)
{
    if (you.default_form == transformation::none)
        untransform(false, false);
    else
    {
        // Forcibly break out of forced forms at this point (since this should
        // only be called in situations where those should end and transform()
        // will refuse to do that on its own)
        if (you.transform_uncancellable)
            untransform(true, false, !new_form, you.default_form);
        transform(0, you.default_form, true, new_form);
    }
    ASSERT(you.form == you.default_form);
}

/**
 * Update whether a merfolk should be swimming.
 *
 * Idempotent, so can be called after position/transformation change without
 * redundantly checking conditions.
 *
 * @param old_grid The feature type that the player was previously on.
 */
void merfolk_check_swimming(dungeon_feature_type old_grid)
{
    const dungeon_feature_type grid = env.grid(you.pos());
    if (!you.airborne()
        && feat_is_water(grid)
        && you.has_mutation(MUT_MERTAIL))
    {
        merfolk_start_swimming();
    }
    else
        merfolk_stop_swimming();

    // Flying above water grants evasion, so redraw even if not transforming.
    if (feat_is_water(grid) || feat_is_water(old_grid))
        you.redraw_evasion = true;
}

void merfolk_start_swimming()
{
    if (you.fishtail)
        return;

    mpr("Your legs become a tail as you dive into the water.");

    if (you.invisible())
        mpr("...but don't expect to remain undetected.");

    you.fishtail = true;
    you.redraw_evasion = true;

    // Don't meld boots if we're in a form that already melded them.
    if (!get_form()->slot_is_blocked(SLOT_BOOTS))
    {
        you.equipment.meld_equipment(1 << SLOT_BOOTS);
        ash_check_bondage();
    }

#ifdef USE_TILE
    init_player_doll();
#endif
}

void merfolk_stop_swimming()
{
    if (!you.fishtail)
        return;

    you.fishtail = false;
    you.redraw_evasion = true;

    // Don't unmeld boots if we're in a form that would meld them on its own.
    if (!get_form()->slot_is_blocked(SLOT_BOOTS))
    {
        you.equipment.unmeld_slot(SLOT_BOOTS);
        ash_check_bondage();
    }

#ifdef USE_TILE
    init_player_doll();
#endif
}

void unset_default_form()
{
    you.default_form = transformation::none;
    set_default_form(transformation::none, nullptr);
}

void set_default_form(transformation t, const item_def *talisman)
{
    if (item_def* old_talisman = you.active_talisman())
    {
        you.cur_talisman = -1;
        if (is_artefact(*old_talisman))
        {
            // We need to remove any artifact properties before running the unequip
            // effects so that max health and magic are updated properly
            you.equipment.update();

            unequip_artefact_effect(*old_talisman, nullptr, false);
        }
        item_skills(*old_talisman, you.skills_to_hide);
    }

    if (talisman)
    {
        ASSERT(in_inventory(*talisman));
        you.cur_talisman = talisman->link;
        item_skills(*talisman, you.skills_to_show);
    }

    // This has to be done after checking item skills, otherwise the new active
    // talisman might count as a useless item (the you.form != you.default_form
    // check in cannot_evoke_item_reason)
    you.default_form = t;
}

transformation form_for_talisman(const item_def &talisman)
{
    for (int i = 0; i < NUM_TRANSFORMS; i++)
        if (formdata[i].talisman == talisman.sub_type)
            return static_cast<transformation>(i);
    return transformation::none;
}

void clear_form_info_on_exit()
{
    for (const form_entry &entry : formdata)
        delete entry.special_dice;
}

void sphinx_notice_riddle_target(monster* mon)
{
    if (!mon->is_peripheral() && !mons_aligned(&you, mon))
        riddle_targs.push_back(mon);
}

static int _riddle_score(const monster& mon)
{
    return mons_intel(mon) + (mons_is_unique(mon.type) ? 3 : 0);
}

void sphinx_check_riddle()
{
    if (you.form != transformation::sphinx)
        return;

    bool unique_found = false;
    vector<monster*> valid_targs;
    for (monster* mon : riddle_targs)
    {
        if (mon->alive() && you.see_cell_no_trans(mon->pos()))
        {
            valid_targs.push_back(mon);
            if (mons_is_unique(mon->type))
                unique_found = true;
        }
    }

    riddle_targs.clear();

    // Be more likely to ask a riddle the more monsters we see at once, but always
    // ask one to any unique we see. They look more interesting!
    if (!unique_found && !x_chance_in_y(valid_targs.size(), valid_targs.size() + 5))
        return;

    // Pick the best candidate to pose a riddle to, favoring uniques and then
    // monsters with human intelligence. (But don't ignore the pseudo-shoutitits
    // against animals altogether.)
    monster* best_mon = valid_targs[0];
    for (monster* mon : valid_targs)
        if (_riddle_score(*mon) > _riddle_score(*best_mon))
            best_mon = mon;

    // Tiny chance to do something other than make noise.
    if (one_chance_in(20))
    {
        string msg = getShoutString("Sphinx riddle success");
        msg = do_mon_str_replacements(msg, *best_mon, S_SILENT);
        mpr(msg);

        if (coinflip())
            best_mon->vex(&you, random_range(5, 8));
        else
            best_mon->confuse(&you, 10);
    }
    else
    {
        // Check if a monster would have a specific reaction first, to skip
        // messages about them ignoring you.
        string mon_msg = getSpeakString(best_mon->name(DESC_PLAIN) + " riddle");

        string msg = getShoutString(mon_msg.empty() ? "Sphinx riddle failure"
                                                    : "Sphinx riddle failure acknowledged");
        msg = do_mon_str_replacements(msg, *best_mon, S_SILENT);
        mpr(msg);

        if (!mon_msg.empty())
            mons_speaks_msg(best_mon, mon_msg, MSGCH_TALK);
    }

    noisy(you.shout_volume(), you.pos(), MID_PLAYER);
}

void sun_scarab_spawn_ember(bool first_time)
{
    // Don't let the player cheat the revival timer by exiting and reentering
    // the form.
    if (first_time && you.props.exists(SOLAR_EMBER_REVIVAL_KEY))
        return;

    mgen_data mg(MONS_SOLAR_EMBER, BEH_COPY, you.pos(), MHITYOU, MG_AUTOFOE);
              mg.set_summoned(&you, MON_SUMM_SUN_SCARAB, 0, false)
                .set_range(1);

    if (monster* mon = create_monster(mg))
    {
        you.props[SOLAR_EMBER_MID_KEY].get_int() = mon->mid;
        mprf(MSGCH_DURATION, first_time ? "A tiny sun coalesces beside you."
                                        : "You reconstitute your solar ember.");
        you.props.erase(SOLAR_EMBER_REVIVAL_KEY);
    }
}

monster* get_solar_ember()
{
    if (!you.props.exists(SOLAR_EMBER_MID_KEY))
        return nullptr;

    return monster_by_mid(you.props[SOLAR_EMBER_MID_KEY].get_int());
}

bool maw_considers_appetising(const monster& mon)
{
    return (mon.holiness() & (MH_NATURAL | MH_PLANT))
           && !mon.is_firewood()
           && !mon.is_summoned()
           && !(mon.flags & MF_HARD_RESET);
}

bool maw_hunger_check(monster* mon)
{
    if (you.beheld())
        return false;

    // Only become mesmerised by things that look edible. (Alas, they still look
    // edible for Gozag worshippers, even if you are doomed to suffer the curse
    // of Midas.)
    if (maw_considers_appetising(*mon) && one_chance_in(6))
    {
        if (!you.clarity())
        {
            mprf("Your maw growls hungrily at the sight of %s.", mon->name(DESC_THE).c_str());
            you.add_beholder(*mon, true, random_range(6, 10));
        }
        else
        {
            mprf("Your maw growls hungrily at the sight of %s, but you resist your urges.",
                 mon->name(DESC_THE).c_str());
        }

        noisy(you.shout_volume(), you.pos(), MID_PLAYER);
        return true;
    }

    return false;
}

bool vampire_mesmerism_check(monster& mon)
{
    if (you.form == transformation::vampire && you.can_see(mon) && mon.can_see(you)
        && (mon.holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY))
        && !one_chance_in(4))
    {
        if (mon.check_willpower(&you, get_form()->get_effect_chance()) <= 0)
        {
            mprf("%s loses %s in your eye%s.",
                    mon.name(DESC_THE).c_str(),
                    mon.pronoun(PRONOUN_REFLEXIVE).c_str(),
                    you.has_mutation(MUT_MISSING_EYE) ? "" : "s");
            mon.daze(random_range(3, 5));
        }
        else
        {
            mprf("%s is briefly mesmerised by your gaze.", mon.name(DESC_THE).c_str());
            // This works even if called during the stealth check, whereas a 1-turn daze
            // would wear off with no effect and produce extra messages on top of that.
            mon.speed_increment -= 10;
        }

        return true;
    }

    return false;
}

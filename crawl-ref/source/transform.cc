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
#define SLOTF(s) (1 << s)

static const int EQF_NONE = 0;

// Weapons and offhand items
static const int EQF_HELD = SLOTF(SLOT_WEAPON) | SLOTF(SLOT_OFFHAND)
                             | SLOTF(SLOT_WEAPON_OR_OFFHAND);
// auxen
static const int EQF_AUXES = SLOTF(SLOT_GLOVES) | SLOTF(SLOT_BOOTS)
                              | SLOTF(SLOT_BARDING)
                              | SLOTF(SLOT_CLOAK) | SLOTF(SLOT_HELMET);
// core body slots (statue form)
static const int EQF_STATUE = SLOTF(SLOT_GLOVES) | SLOTF(SLOT_BOOTS)
                              | SLOTF(SLOT_BARDING)
                              | SLOTF(SLOT_BODY_ARMOUR);
// everything you can (W)ear
static const int EQF_WEAR = EQF_AUXES | SLOTF(SLOT_BODY_ARMOUR)
                            | SLOTF(SLOT_OFFHAND) | SLOTF(SLOT_WEAPON_OR_OFFHAND);
// everything but jewellery
static const int EQF_PHYSICAL = EQF_HELD | EQF_WEAR;
// just rings
static const int EQF_RINGS = SLOTF(SLOT_RING);
// all jewellery
static const int EQF_JEWELLERY = SLOTF(SLOT_RING) | SLOTF(SLOT_AMULET);
// everything
static const int EQF_ALL = EQF_PHYSICAL | EQF_JEWELLERY;

string Form::melding_description(bool itemized) const
{
    if (!itemized)
    {
        // this is a bit rough and ready...
        if (blocked_slots == EQF_ALL)
            return "Your equipment is entirely melded.";
        else if (blocked_slots == EQF_PHYSICAL)
            return "Your armour is entirely melded.";
        else if ((blocked_slots & EQF_PHYSICAL) == EQF_PHYSICAL)
            return "Your equipment is almost entirely melded.";
        else if ((blocked_slots & EQF_STATUE) == EQF_STATUE
                && (you_can_wear(SLOT_GLOVES, false) != false
                    || you_can_wear(SLOT_BOOTS, false) != false
                    || you_can_wear(SLOT_BARDING, false) != false
                    || you_can_wear(SLOT_BODY_ARMOUR, false) != false))
        {
            return "Your equipment is partially melded.";
        }
        // otherwise, rely on the form description to convey what is melded.
        return "";
    }

    vector<string> tags;
    if (blocked_slots == EQF_ALL)
        tags.emplace_back("All Equipment");
    else if (blocked_slots == EQF_PHYSICAL)
        tags.emplace_back("All Weapons and Armour");
    else
    {
        for (int i = SLOT_WEAPON; i < SLOT_GIZMO; ++i)
        {
            equipment_slot slot = static_cast<equipment_slot>(i);
            if (testbits(blocked_slots, SLOTF(i)))
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
    die("No formdata entry found for form %d", (int)form);
}

Form::Form(const form_entry &fe)
    : short_name(fe.short_name), wiz_name(fe.wiz_name),
      min_skill(fe.min_skill), max_skill(fe.max_skill),
      str_mod(fe.str_mod), dex_mod(fe.dex_mod), base_move_speed(fe.move_speed),
      blocked_slots(fe.blocked_slots), size(fe.size),
      can_cast(fe.can_cast),
      uc_colour(fe.uc_colour), uc_attack_verbs(fe.uc_attack_verbs),
      changes_anatomy(fe.changes_anatomy),
      changes_substance(fe.changes_substance),
      holiness(fe.holiness),
      has_blood(fe.has_blood), has_hair(fe.has_hair),
      has_bones(fe.has_bones), has_feet(fe.has_feet),
      has_ears(fe.has_ears),
      shout_verb(fe.shout_verb),
      shout_volume_modifier(fe.shout_volume_modifier),
      hand_name(fe.hand_name), foot_name(fe.foot_name),
      flesh_equivalent(fe.flesh_equivalent),
      special_dice_name(fe.special_dice_name),
      long_name(fe.long_name), description(fe.description),
      resists(fe.resists), ac(fe.ac), ev(fe.ev),
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

int Form::scaling_value(const FormScaling &sc, bool random,
                        int level, int scale) const
{
    if (sc.xl_based)
    {
        const int s = sc.scaling * you.experience_level * scale;
        if (random)
            return sc.base * scale + div_rand_round(s, 27);
        return sc.base * scale + s / 27;
    }
    if (max_skill == min_skill)
        return sc.base * scale;

    const int lvl = level == -1 ? get_level(scale) : level * scale;
    const int over_min = lvl - min_skill * scale; // may be negative
    const int denom = max_skill - min_skill;
    if (random)
        return sc.base * scale + div_rand_round(over_min * sc.scaling, denom);
    return sc.base * scale + over_min * sc.scaling / denom;
}

int Form::divided_scaling(const FormScaling &sc, bool random,
                          int level, int scale) const
{
    const int scaled_val = scaling_value(sc, random, level, scale);
    if (random)
        return div_rand_round(scaled_val, scale);
    return scaled_val / scale;
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
    return max(0, scaling_value(ac, false, skill, 100));
}

int Form::ev_bonus(int skill) const
{
    return max(0, scaling_value(ev, false, skill, 1));
}

int Form::get_base_unarmed_damage(bool random, int skill) const
{
    // All forms start with base 3 UC damage.
    return 3 + max(0, divided_scaling(unarmed_bonus_dam, random, skill, 100));
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
    // (Flux form gets double the HP penalty per level, since its min skill is so low.)
    const int shortfall = (min_skill * scale - lvl)
                          * (you.form == transformation::flux ? 2 : 1);
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
        return (*special_dice)(skill, random);
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

#if TAG_MAJOR_VERSION == 34
class FormSpider : public Form
{
private:
    FormSpider() : Form(transformation::spider) { }
    DISALLOW_COPY_AND_ASSIGN(FormSpider);
public:
    static const FormSpider &instance() { static FormSpider inst; return inst; }

    int get_web_chance(int skill = -1) const override
    {
        return divided_scaling(FormScaling().Base(20).Scaling(20), false, skill, 100);
    }
};
#endif

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
    FormBlade() : Form(transformation::blade_hands) { }
    DISALLOW_COPY_AND_ASSIGN(FormBlade);
public:
    static const FormBlade &instance() { static FormBlade inst; return inst; }

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
        return make_stringf("You %s blades for %s.",
                            past_tense ? "had" : "have",
                            blade_parts().c_str());
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message() const override
    {
        const bool singular = you.arm_count() == 1;

        // XXX: a little ugly
        return make_stringf("Your %s turn%s into%s razor-sharp scythe blade%s.",
                            blade_parts().c_str(), singular ? "s" : "",
                            singular ? " a" : "", singular ? "" : "s");
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        const bool singular = you.arm_count() == 1;

        // XXX: a little ugly
        return make_stringf("Your %s revert%s to %s normal proportions.",
                            blade_parts().c_str(), singular ? "s" : "",
                            singular ? "its" : "their");
    }

    /**
     * How much AC do you lose from body armour from being in this form?
     * 80% at `min_skill` or below, 0% at `max_skill` or above.
     */
    int get_base_ac_penalty(int base, int skill = -1) const override
    {
        const int scale = 100;
        const int lvl = max(skill == -1 ? get_level(scale) : skill * scale, min_skill * scale);
        const int shortfall = max(0, max_skill * scale - lvl);
        const int div = (max_skill - min_skill + 2) * scale;
        // Round up.
        return (shortfall * base + div - 1) / div;
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        return "Blade " + blade_parts(true);
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

    dice_def get_special_damage(bool random = true, int skill = -1) const override
    {
        ability_type abil = species::draconian_breath(you.species);
        spell_type spell = abil == ABIL_NON_ABILITY ? SPELL_GOLDEN_BREATH
                                                    : draconian_breath_to_spell(abil);

        const zap_type zap = spell_to_zap(spell);

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

#if TAG_MAJOR_VERSION == 34
class FormJelly : public Form
{
private:
    FormJelly() : Form(transformation::jelly) { }
    DISALLOW_COPY_AND_ASSIGN(FormJelly);
public:
    static const FormJelly &instance() { static FormJelly inst; return inst; }
};
#endif

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
        return divided_scaling(FormScaling().Base(10).Scaling(8), random, skill, 100);
    }

    int get_base_ac_penalty(int base, int /*skill*/ = -1) const override
    {
        return base * 4 / 5;
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

    int get_vamp_chance(int skill = -1) const override
    {
        if (skill == -1)
            skill = get_level(1);

        return 100 - (1000 / (skill + 10));
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

        dice_def get_special_damage(bool random, int skill = -1) const override
        {
            const int pow = skill == -1 ? get_level(10) : skill * 10;

            if (random)
                return dice_def(2, 4 + div_rand_round(pow, 10));
            else
                return dice_def(2, 4 + pow / 10);
        }
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
        return scaling_value(FormScaling().Base(160).Scaling(120), false, skill);
    }

    int mp_regen_bonus(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(60).Scaling(40), false, skill);
    }

    // Number of bees created (x10)
    int get_effect_size(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(32).Scaling(23), false, skill);
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
        return divided_scaling(FormScaling().Base(12).Scaling(10), false, skill);
    }

    virtual int get_takedown_multiplier(int skill = -1) const override
    {
        return divided_scaling(FormScaling().Base(75).Scaling(50), false, skill);
    }

    virtual int get_howl_power(int skill = -1) const override
    {
        return divided_scaling(FormScaling().Base(80).Scaling(40), false, skill);
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
        return scaling_value(FormScaling().Base(4).Scaling(5), false, skill);
    }
};

class FormFortressCrab : public Form
{
private:
FormFortressCrab() : Form(transformation::fortress_crab) { }
    DISALLOW_COPY_AND_ASSIGN(FormFortressCrab);
public:
    static const FormFortressCrab &instance() { static FormFortressCrab inst; return inst; }

    // XXX: Used here for how much AC you *gain* in this form (by giving a negative 'penalty'))
    int get_base_ac_penalty(int base, int skill = -1) const override
    {
        const int mult = scaling_value(FormScaling().Base(75).Scaling(75), false, skill);
        return -(base * mult / 100);
    }

    // Number of clouds placed
    int get_effect_size(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(9).Scaling(16), false, skill);
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

    int get_aux_damage(bool random, int skill) const override
    {
        return divided_scaling(FormScaling().Base(3).Scaling(3), random, skill, 100);
    }

    // Number of monsters affected by tendrils per attack (multiplied by 10,
    // so that it can start at 2.5)
    int get_effect_size(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(25).Scaling(15), false, skill);
    }

    // Chance of lithotoxin petrification.
    int get_effect_chance(int skill = -1) const override
    {
        return scaling_value(FormScaling().Base(55).Scaling(15), false, skill);
    }
};

static const Form* forms[] =
{
    &FormNone::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormSpider::instance(),
#endif
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
#if TAG_MAJOR_VERSION == 34
    &FormJelly::instance(),
#endif
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
 * What is the name of the player parts that will become blades?
 */
string blade_parts(bool terse)
{
    // there's special casing in base_hand_name to use "blade" everywhere, so
    // use the non-temp name
    string str = you.base_hand_name(true, false);

    // creatures with paws (aka felids) have four paws, but only two of them
    // turn into blades.
    if (!terse && you.has_mutation(MUT_PAWS, false))
        str = "front " + str;
    else if (!terse && you.arm_count() > 2)
        str = "main " + str; // Op have four main tentacles

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
    unwind_var<item_def> unwind_talisman(you.active_talisman);
    unwind_var<transformation> unwind_default_form(you.default_form);
    unwind_var<transformation> unwind_form(you.form);

    you.default_form = which_trans;
    you.form = which_trans;
    if (talisman)
        you.active_talisman = *talisman;
    else
        you.active_talisman.clear();

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
    if (feat_dangerous_for_form(which_trans, feat, talisman))
    {
        if (!involuntary)
        {
            mprf("Transforming right now would cause you to %s!",
                        feat == DNGN_DEEP_WATER ? "drown" : "burn");
        }
        return false;
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

        if (you.attribute[ATTR_HELD])
        {
            const trap_def *trap = trap_at(you.pos());
            if (trap && trap->type == TRAP_WEB)
            {
                leave_web(true);
                if (trap_at(you.pos()))
                    mpr("Your branches slip out of the web.");
                else
                    mpr("Your branches shred the web that entangled you.");
            }
        }
        // Fall through to dragon form to leave nets.

    case transformation::dragon:
        if (you.attribute[ATTR_HELD])
        {
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
            {
                mpr("The net rips apart!");
                destroy_item(net);
                stop_being_held();
            }
        }

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

static void _enter_form(int dur, transformation which_trans, bool scale_hp = true)
{
    const bool was_flying = you.airborne();

    // Give the transformation message.
    // (Vampire bat swarm ability skips this part.)
    if (!(you.form == transformation::vampire || you.form == transformation::bat_swarm)
         && !(which_trans == transformation::vampire || which_trans == transformation::bat_swarm))
    {
        mpr(get_form(which_trans)->transform_message());
    }

    // Update your status.
    // Order matters here, take stuff off (and handle attendant HP and stat
    // changes) before adjusting the player to be transformed.
    you.equipment.meld_equipment(get_form(which_trans)->blocked_slots);

    set_form(which_trans, dur, scale_hp);

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
    you.stop_directly_constricting_all(false);

    // Stop being constricted if we are now too large, or are now immune.
    if (you.get_constrict_type() == CONSTRICT_MELEE)
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
        merfolk_check_swimming(env.grid(you.pos()), false);

    if (is_artefact(you.active_talisman))
        equip_artefact_effect(you.active_talisman, nullptr, false);

    // In the case where we didn't actually meld any gear (but possibly used
    // a new artefact talisman or were forcibly polymorphed away from one),
    // refresh equipment properties.
    you.equipment.update();

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
        && x_chance_in_y(you.piety, MAX_PIETY)
        && which_trans != transformation::none)
    {
        simple_god_message(" protects your body from unnatural transformation!");
        return false;
    }

    if (!involuntary && crawl_state.is_god_acting())
        involuntary = true;

    if (!check_transform_into(which_trans, involuntary,
                                using_talisman ? &you.active_talisman : nullptr))
    {
        return false;
    }

    // Vampire should shift in and out of bat swarm without reverting to fully untransformed in the middle
    if (you.form != transformation::none
        && !((you.form == transformation::vampire || you.form == transformation::bat_swarm)
               && (which_trans == transformation::vampire || which_trans == transformation::bat_swarm)))
    {
        untransform(true, !using_talisman);
    }

    _enter_form(dur, which_trans, !using_talisman);

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
 */
void untransform(bool skip_move, bool scale_hp)
{
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

    // If the player is no longer be eligible to equip some of the items that
    // they were wearing (possibly due to losing slots from their default form
    // changing), calculate that now and make the fall off.
    vector<item_def*> forced_remove = you.equipment.get_forced_removal_list(true);
    for (item_def* item : forced_remove)
    {
        mprf("%s falls away%s!", item->name(DESC_YOUR).c_str(),
                item->cursed() ? ", shattering the curse!" : "");

        unequip_item(*item, false);
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
            merfolk_check_swimming(env.grid(you.pos()), false);
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
    if (you.get_constrict_type() == CONSTRICT_MELEE)
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

void return_to_default_form()
{
    if (you.default_form == transformation::none)
        untransform(false, false);
    else
    {
        // Forcibly break out of forced forms at this point (since this should
        // only be called in situations where those should end and transform()
        // will refuse to do that on its own)
        if (you.transform_uncancellable)
            untransform(true, false);
        transform(0, you.default_form, true, true);
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
 * @param stepped  Whether the player is performing a normal walking move.
 */
void merfolk_check_swimming(dungeon_feature_type old_grid, bool stepped)
{
    const dungeon_feature_type grid = env.grid(you.pos());
    if (you.ground_level()
        && feat_is_water(grid)
        && you.has_mutation(MUT_MERTAIL))
    {
        merfolk_start_swimming(stepped);
    }
    else
        merfolk_stop_swimming();

    // Flying above water grants evasion, so redraw even if not transforming.
    if (feat_is_water(grid) || feat_is_water(old_grid))
        you.redraw_evasion = true;
}

void merfolk_start_swimming(bool stepped)
{
    if (you.fishtail)
        return;

    if (stepped)
        mpr("Your legs become a tail as you enter the water.");
    else
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
    item_def talisman = you.active_talisman;

    you.default_form = transformation::none;
    you.active_talisman.clear();

    if (is_artefact(talisman))
        unequip_artefact_effect(talisman, nullptr, false);
    item_skills(talisman, you.skills_to_hide);
}

void set_default_form(transformation t, const item_def *source)
{
    item_def talisman = you.active_talisman;
    you.active_talisman.clear();

    if (is_artefact(talisman))
        unequip_artefact_effect(talisman, nullptr, false);
    item_skills(talisman, you.skills_to_hide);

    if (source)
    {
        you.active_talisman = *source; // iffy
        // XXX: Make the copied talisman not appear to be in the player's
        // inventory, so that examining it via the mutation menu won't display
        // unusable button prompts to set skill target.
        you.active_talisman.pos = coord_def();
        item_skills(you.active_talisman, you.skills_to_show);
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
    {
        mprf("Noticed %s.", mon->name(DESC_THE).c_str());
        riddle_targs.push_back(mon);
    }
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
        mprf(MSGCH_DURATION, first_time ? "A tiny sun coalesceses beside you."
                                        : "You reconstitue your solar ember.");
        you.props.erase(SOLAR_EMBER_REVIVAL_KEY);
    }
}

monster* get_solar_ember()
{
    if (!you.props.exists(SOLAR_EMBER_MID_KEY))
        return nullptr;

    return monster_by_mid(you.props[SOLAR_EMBER_MID_KEY].get_int());
}

bool maw_growl_check(const monster* mon)
{
    // Only growl at things that look edible. (Alas, they still look edible for
    // Gozag worshippers, even if you are doomed to suffer the curse of Midas.)
    if (mons_class_can_leave_corpse(mons_species(mon->type))
        && !mon->is_summoned()
        && !(mon->flags & MF_HARD_RESET)
        && one_chance_in(8))
    {
        mprf("Your maw growls hungrily at %s.", mon->name(DESC_THE).c_str());
        noisy(you.shout_volume(), you.pos(), MID_PLAYER);
        return true;
    }

    return false;
}

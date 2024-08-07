/**
 * @file
 * @brief Misc function related to player transformations.
**/

#include "AppHdr.h"

#include "transform.h"

#include <cstdio>
#include <cstring>

#include "artefact.h"
#include "art-enum.h"
#include "delay.h"
#include "describe.h"
#include "english.h"
#include "env.h"
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
#include "mutation.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "spl-cast.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

// transform slot enums into flags
#define SLOTF(s) (1 << s)

static const int EQF_NONE = 0;
// "hand" slots (not rings)
static const int EQF_HANDS = SLOTF(EQ_WEAPON) | SLOTF(EQ_OFFHAND)
                             | SLOTF(EQ_GLOVES);
// auxen
static const int EQF_AUXES = SLOTF(EQ_GLOVES) | SLOTF(EQ_BOOTS)
                              | SLOTF(EQ_CLOAK) | SLOTF(EQ_HELMET);
// core body slots (statue form)
static const int EQF_STATUE = SLOTF(EQ_GLOVES) | SLOTF(EQ_BOOTS)
                              | SLOTF(EQ_BODY_ARMOUR);
// more core body slots (Lear's Hauberk)
static const int EQF_LEAR = EQF_STATUE | SLOTF(EQ_HELMET);
// everything you can (W)ear
static const int EQF_WEAR = EQF_AUXES | SLOTF(EQ_BODY_ARMOUR)
                            | SLOTF(EQ_OFFHAND);
// everything but jewellery
static const int EQF_PHYSICAL = EQF_HANDS | EQF_WEAR;
// all rings (except for the macabre finger amulet's)
static const int EQF_RINGS = SLOTF(EQ_LEFT_RING) | SLOTF(EQ_RIGHT_RING)
                             | SLOTF(EQ_RING_ONE) | SLOTF(EQ_RING_TWO)
                             | SLOTF(EQ_RING_THREE) | SLOTF(EQ_RING_FOUR)
                             | SLOTF(EQ_RING_FIVE) | SLOTF(EQ_RING_SIX)
                             | SLOTF(EQ_RING_SEVEN) | SLOTF(EQ_RING_EIGHT);
// amulet & pal
static const int EQF_AMULETS = SLOTF(EQ_AMULET) | SLOTF(EQ_RING_AMULET);
// everything
static const int EQF_ALL = EQF_PHYSICAL | EQF_RINGS | EQF_AMULETS;

string Form::melding_description() const
{
    // this is a bit rough and ready...
    // XX simplify slot melding rather than complicate this function?
    if (blocked_slots == EQF_ALL)
        return "Your equipment is entirely melded.";
    else if (blocked_slots == EQF_PHYSICAL)
        return "Your armour is entirely melded.";
    else if ((blocked_slots & EQF_PHYSICAL) == EQF_PHYSICAL)
        return "Your equipment is almost entirely melded.";
    else if ((blocked_slots & EQF_STATUE) == EQF_STATUE
             && (you_can_wear(EQ_GLOVES, false) != false
                 || you_can_wear(EQ_BOOTS, false) != false
                 || you_can_wear(EQ_BODY_ARMOUR, false) != false))
    {
        return "Your equipment is partially melded.";
    }
    // otherwise, rely on the form description to convey what is melded.
    return "";
}

static const FormAttackVerbs DEFAULT_VERBS = FormAttackVerbs(nullptr, nullptr,
                                                             nullptr, nullptr);
static const FormAttackVerbs ANIMAL_VERBS = FormAttackVerbs("hit", "bite",
                                                            "maul", "maul");

static const FormDuration DEFAULT_DURATION = FormDuration(20, PS_DOUBLE, 100);
static const FormDuration BAD_DURATION = FormDuration(15, PS_ONE_AND_A_HALF,
                                                      100);

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
      duration(fe.duration),
      min_skill(fe.min_skill), max_skill(fe.max_skill),
      str_mod(fe.str_mod), dex_mod(fe.dex_mod),
      blocked_slots(fe.blocked_slots), size(fe.size),
      can_cast(fe.can_cast),
      uc_colour(fe.uc_colour), uc_attack_verbs(fe.uc_attack_verbs),
      keeps_mutations(fe.keeps_mutations),
      changes_physiology(fe.changes_physiology),
      has_blood(fe.has_blood), has_hair(fe.has_hair),
      has_bones(fe.has_bones), has_feet(fe.has_feet),
      has_ears(fe.has_ears),
      shout_verb(fe.shout_verb),
      shout_volume_modifier(fe.shout_volume_modifier),
      hand_name(fe.hand_name), foot_name(fe.foot_name),
      flesh_equivalent(fe.flesh_equivalent),
      long_name(fe.long_name), description(fe.description),
      resists(fe.resists), ac(fe.ac),
      unarmed_bonus_dam(fe.unarmed_bonus_dam),
      can_fly(fe.can_fly), can_swim(fe.can_swim),
      uc_brand(fe.uc_brand), uc_attack(fe.uc_attack),
      prayer_action(fe.prayer_action), equivalent_mons(fe.equivalent_mons),
      hp_mod(fe.hp_mod), fakemuts(fe.fakemuts), badmuts(fe.badmuts)
{ }

Form::Form(transformation tran)
    : Form(_find_form_entry(tran))
{ }
/**
 * Is the given equipment slot available for use in this form?
 *
 * @param slot      The equipment slot in question. (May be a weird fake
 *                  slot - EQ_STAFF or EQ_ALL_ARMOUR.)
 * @return          Whether at least some items can be worn in this slot in
 *                  this form.
 *                  (The player's race, or mutations, may still block the
 *                  slot, or it may be restricted to subtypes.)
 */
bool Form::slot_available(int slot) const
{
    if (slot == EQ_ALL_ARMOUR)
        return !all_blocked(EQF_WEAR);
    if (slot == EQ_RINGS || slot == EQ_RINGS_PLUS)
        return !all_blocked(EQF_RINGS);

    if (slot == EQ_STAFF)
        slot = EQ_WEAPON;
    return !(blocked_slots & SLOTF(slot));
}

/**
 * Can the player wear the given item while in this form?
 *
 * Does not take mutations into account.
 *
 * @param item  The item in question
 * @return      Whether this form prevents the player from wearing the
 *              item. (Other things may also prevent it, of course)
 */
bool Form::can_wear_item(const item_def& item) const
{
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (jewellery_is_amulet(item))
            return slot_available(EQ_AMULET);
        return !all_blocked(EQF_RINGS);
    }

    if (is_unrandom_artefact(item, UNRAND_LEAR))
        return !(blocked_slots & EQF_LEAR); // ok if no body slots blocked

    const equipment_type slot = is_weapon(item) ? EQ_OFFHAND
                                                : get_armour_slot(item);
    return slot_available(slot);
}

/**
 * Get the bonus to form duration granted for a given (spell)power.
 *
 * @param pow               The spellpower/equivalent of the form.
 * @return                  A bonus to form duration.
 */
int FormDuration::power_bonus(int pow) const
{
    switch (scaling_type)
    {
        case PS_NONE:
            return 0;
        case PS_SINGLE:
            return random2(pow);
        case PS_ONE_AND_A_HALF:
            return random2(pow) + random2(pow/2);
        case PS_DOUBLE:
            return random2(pow) + random2(pow);
        default:
            die("Unknown scaling type!");
            return -1;
    }
}

/**
 * Get the duration for this form, when newly entered.
 *
 * @param pow   The power of the effect creating this form. (Spellpower, etc.)
 * @return      The duration of the form. (XXX: in turns...?)
 */
int Form::get_duration(int pow) const
{
    return min(duration.base + duration.power_bonus(pow), duration.max);
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
string Form::transform_message(bool was_flying) const
{
    // XXX: refactor this into a second function (and also rethink the logic)
    string start = "Buggily, y";
    if (!was_flying
        && player_can_fly()
        && feat_is_water(env.grid(you.pos())))
    {
        start = "You fly out of the water as y";
    }
    else if (was_flying
             && player_can_swim()
             && feat_is_water(env.grid(you.pos())))
    {
        start = "As you dive into the water, y";
    }
    else
        start = "Y";

    return make_stringf("%sou turn into %s", start.c_str(),
                        get_transform_description().c_str());
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
                        bool get_max, int scale) const
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

    const int lvl = get_max ? max_skill * scale : get_level(scale);
    const int over_min = lvl - min_skill * scale; // may be negative
    const int denom = max_skill - min_skill;
    if (random)
        return sc.base * scale + div_rand_round(over_min * sc.scaling, denom);
    return sc.base * scale + over_min * sc.scaling / denom;
}

int Form::divided_scaling(const FormScaling &sc, bool random,
                          bool get_max, int scale) const
{
    const int scaled_val = scaling_value(sc, random, get_max, scale);
    if (random)
        return div_rand_round(scaled_val, scale);
    return scaled_val / scale;
}

/**
 * What AC bonus does the player get while in this form?
 *
 * Many forms are power-dependent, so the answer given may be strange if the
 * player isn't currently in the form in question.
 *
 * @return  The AC bonus currently granted by the form, multiplied by 100 to
 *          allow for pseudo-decimal flexibility (& to match
 *          player::armour_class())
 */
int Form::get_ac_bonus(bool get_max) const
{
    return max(0, scaling_value(ac, false, get_max, 100));
}

int Form::get_base_unarmed_damage(bool random, bool get_max) const
{
    // All forms start with base 3 UC damage.
    return 3 + max(0, divided_scaling(unarmed_bonus_dam, random, get_max, 100));
}

/// `force_talisman` means to calculate HP as if we were in a talisman form (i.e. with penalties with insufficient Shapeshifting skill),
/// without checking whether we actually are.
int Form::mult_hp(int base_hp, bool force_talisman) const
{
    const int scale = 100;
    const int lvl = get_level(scale);
    // Only penalize if you're in a talisman form with insufficient skill.
    const int shortfall = min_skill * scale - lvl;
    if (shortfall <= 0 || you.default_form != you.form && !force_talisman)
        return hp_mod * base_hp / 10;
    // -10% hp per skill level short, down to -90%
    const int penalty = min(shortfall, 9 * scale);
    return base_hp * hp_mod * (10 * scale - penalty) / (scale * 10 * 10);
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
bool Form::res_acid() const
{
    return get_resist(resists, MR_RES_ACID);
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
 * Are all of the given equipment slots blocked while in this form?
 *
 * @param slotflags     A set of flags, corresponding to the union of
 (1 << the slot enum) for each slot in question.
 * @return              Whether all of the given slots are blocked.
 */
bool Form::all_blocked(int slotflags) const
{
    return slotflags == (blocked_slots & slotflags);
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

vector<string> Form::get_fakemuts(bool terse) const
{
    vector<string> result;
    for (const auto &p : fakemuts)
        result.push_back(terse ? p.first : p.second);
    return result;
}

vector<string> Form::get_bad_fakemuts(bool terse) const
{
    vector<string> result;
    for (const auto &p : badmuts)
        result.push_back(terse ? p.first : p.second);
    return result;
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
};
#endif

class FormFlux : public Form
{
private:
    FormFlux() : Form(transformation::flux) { }
    DISALLOW_COPY_AND_ASSIGN(FormFlux);
public:
    static const FormFlux &instance() { static FormFlux inst; return inst; }

    int contam_dam(bool random = true, bool max = false) const override
    {
        return divided_scaling(FormScaling().Base(30).Scaling(20), random, max, 100);
    }

    int ev_bonus(bool /*get_max*/) const override
    {
        return 4;
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
    string transform_message(bool /*was_flying*/) const override
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
     * 100% at `min_skill` or below, 0% at `max_skill` or above.
     */
    int get_base_ac_penalty(int base) const override
    {
        const int scale = 100;
        const int lvl = max(get_level(scale), min_skill * scale);
        const int shortfall = max(0, max_skill * scale - lvl);
        const int div = (max_skill - min_skill) * scale;
        // Round up.
        return (shortfall * base + div - 1) / div;
    }

    bool can_offhand_punch() const override { return true; }

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
    string transform_message(bool was_flying) const override
    {
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DEEP_DWARF && one_chance_in(10))
            return "You inwardly fear your resemblance to a lawn ornament.";
#endif
        if (you.species == SP_GARGOYLE)
            return "Your body stiffens and grows slower.";
        return Form::transform_message(was_flying);
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
        // This only handles lava orcs going statue -> stoneskin.
        if (you.species == SP_GARGOYLE)
            return "You revert to a slightly less stony form.";
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
    int get_ac_bonus(bool max) const override
    {
        const int normal = Form::get_ac_bonus(max);
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
            case MONS_FIRE_DRAGON:
                return 2;
            case MONS_ICE_DRAGON:
                return -1;
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
            case MONS_ICE_DRAGON:
                return 2;
            case MONS_FIRE_DRAGON:
                return -1;
            default:
                return 0;
        }
    }

    bool can_offhand_punch() const override { return true; }
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
    string transform_message(bool /*was_flying*/) const override
    {
        return "Your flesh twists and warps into a mockery of life!";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const override
    {
        if (you.undead_state(false) == US_ALIVE)
            return "You feel yourself come back to life.";
        return "You feel your undeath return to normal.";
        // ^^^ vampires only, probably
    }
};

class FormBat : public Form
{
private:
    FormBat() : Form(transformation::bat) { }
    DISALLOW_COPY_AND_ASSIGN(FormBat);
public:
    static const FormBat &instance() { static FormBat inst; return inst; }

    /**
     * Get an monster type corresponding to the transformation.
     *
     * (Used for console player glyphs.)
     *
     * @return  A monster type corresponding to the player in this form.
     */
    monster_type get_equivalent_mons() const override
    {
        return you.has_mutation(MUT_VAMPIRISM) ? MONS_VAMPIRE_BAT : MONS_BAT;
    }

    /// Does this form care about skill for UC damage and accuracy, or only XL?
    bool get_unarmed_uses_skill() const override {
        return you.get_mutation_level(MUT_VAMPIRISM) >= 2;
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override
    {
        return make_stringf("a %sbat.",
                            you.has_mutation(MUT_VAMPIRISM) ? "vampire " : "");
    }

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

    int ev_bonus(bool get_max) const override
    {
        return max(0, divided_scaling(FormScaling().Base(20).Scaling(7),
                                    false, get_max, 100));
    }

    bool can_offhand_punch() const override { return true; }

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

class FormBeast : public Form
{
private:
    FormBeast() : Form(transformation::beast) { }
    DISALLOW_COPY_AND_ASSIGN(FormBeast);
public:
    static const FormBeast &instance() { static FormBeast inst; return inst; }
    int slay_bonus(bool random, bool max) const override
    {
        return divided_scaling(FormScaling().Scaling(4), random, max, 100);
    }

    vector<string> get_fakemuts(bool terse) const override {
        return {
            make_stringf(terse ?
                         "beast (slay +%d)" :
                         "Your limbs bulge with bestial killing power. (Slay +%d)",
                         slay_bonus(false, false))
        };
    }
};

class FormMaw : public Form
{
private:
    FormMaw() : Form(transformation::maw) { }
    DISALLOW_COPY_AND_ASSIGN(FormMaw);
public:
    static const FormMaw &instance() { static FormMaw inst; return inst; }

    int get_aux_damage(bool random, bool max) const override
    {
        return divided_scaling(FormScaling().Base(12).Scaling(8), random, max, 100);
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
    &FormBeast::instance(),
    &FormMaw::instance(),
    &FormFlux::instance(),
    &FormSlaughter::instance(),
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

// Used to mark transformations which override species intrinsics.
bool form_changes_physiology(transformation form)
{
    return get_form(form)->changes_physiology;
}

// Used to mark forms which keep most form-based mutations.
bool form_keeps_mutations(transformation form)
{
    return get_form(form)->keeps_mutations;
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

static set<equipment_type>
_init_equipment_removal(transformation form)
{
    set<equipment_type> result;
    if (!form_can_wield(form))
    {
        if (you.weapon() || you.melded[EQ_WEAPON])
            result.insert(EQ_WEAPON);
        if (you.offhand_weapon() || you.melded[EQ_OFFHAND])
            result.insert(EQ_OFFHAND); // ^ dubious!
    }

    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
    {
        if (i == EQ_WEAPON)
            continue;
        const equipment_type eq = static_cast<equipment_type>(i);
        const item_def *pitem = you.slot_item(eq, true);

        if (pitem && (get_form(form)->blocked_slots & SLOTF(i)
                      || (i != EQ_RING_AMULET && i != EQ_GIZMO
                          && !get_form(form)->can_wear_item(*pitem))))
        {
            result.insert(eq);
        }
    }
    return result;
}

static void _remove_equipment(const set<equipment_type>& removed,
                              transformation form,
                              bool meld = true, bool mutation = false)
{
    // Meld items into you in (reverse) order. (set is a sorted container)
    for (const equipment_type e : removed)
    {
        item_def *equip = you.slot_item(e, true);
        if (equip == nullptr)
            continue;

        bool unequip = !meld;
        if (!unequip && e == EQ_WEAPON)
        {
            if (form_can_wield(form))
                unequip = true;
            if (!is_weapon(*equip))
                unequip = true;
        }

        const string msg = make_stringf("%s %s%s %s",
             equip->name(DESC_YOUR).c_str(),
             unequip ? "fall" : "meld",
             equip->quantity > 1 ? "" : "s",
             unequip ? "away" : "into your body.");

        if (you_worship(GOD_ASHENZARI) && unequip && equip->cursed())
            mprf(MSGCH_GOD, "%s, shattering the curse!", msg.c_str());
        else if (unequip)
            mprf("%s!", msg.c_str());
        else
            mpr(msg);

        if (unequip)
        {
            if (e == EQ_WEAPON)
            {
                unwield_item(*you.weapon(), !you.berserk());
                canned_msg(MSG_EMPTY_HANDED_NOW);
            }
            else
                unequip_item(e);

            if (mutation)
            {
                // A mutation made us not only lose an equipment slot
                // but actually removed a worn item: Funny!
                xom_is_stimulated(is_artefact(*equip) ? 200 : 100);
            }
        }
        else
            meld_slot(e);
    }

    if (meld)
    {
        for (const equipment_type e : removed)
            if (you.slot_item(e, true) != nullptr)
                unequip_effect(e, you.equip[e], true, true);
    }
}

static void _unmeld_equipment_type(equipment_type e)
{
    item_def& item = you.inv[you.equip[e]];
    bool force_remove = false;

    if (e == EQ_WEAPON)
    {
        if (you.slot_item(EQ_OFFHAND)
            && is_shield_incompatible(item, you.slot_item(EQ_OFFHAND)))
        {
            force_remove = true;
        }
    }
    else if (item.base_type == OBJ_ARMOUR)
    {
        // This could happen if the player was mutated during the form.
        if (!can_wear_armour(item, false, true))
            force_remove = true;

        // If you switched weapons during the transformation, make
        // sure you can still wear your shield.
        // (This is only possible with Statue Form.)
        if (e == EQ_OFFHAND
            && you.weapon()
            && is_shield_incompatible(*you.weapon(), &item))
        {
            force_remove = true;
        }
    }

    if (force_remove)
    {
        mprf("%s is pushed off your body!", item.name(DESC_YOUR).c_str());
        unequip_item(e);
    }
    else
    {
        mprf("%s unmelds from your body.", item.name(DESC_YOUR).c_str());
        unmeld_slot(e);
    }
}

static void _unmeld_equipment(const set<equipment_type>& melded)
{
    // Unmeld items in order.
    for (const equipment_type e : melded)
    {
        if (you.equip[e] == -1)
            continue;

        _unmeld_equipment_type(e);
    }

    for (const equipment_type e : melded)
        if (you.equip[e] != -1)
            equip_effect(e, you.equip[e], true, true);
}

static bool _lears_takes_slot(equipment_type eq)
{
    return eq >= EQ_HELMET && eq <= EQ_BOOTS
        || eq == EQ_BODY_ARMOUR;
}

static bool _form_melds_lears(transformation which_trans)
{
    for (equipment_type eq : _init_equipment_removal(which_trans))
        if (_lears_takes_slot(eq))
            return true;
    return false;
}

void unmeld_one_equip(equipment_type eq)
{
    if (_lears_takes_slot(eq))
    {
        const item_def* arm = you.slot_item(EQ_BODY_ARMOUR, true);
        if (arm && is_unrandom_artefact(*arm, UNRAND_LEAR))
        {
            // Don't unmeld lears when de-fishtailing if you're in
            // a form that should keep it melded.
            if (_form_melds_lears(you.form))
                return;
            eq = EQ_BODY_ARMOUR;
        }
    }

    set<equipment_type> e;
    e.insert(eq);
    _unmeld_equipment(e);
}

void remove_one_equip(equipment_type eq, bool meld, bool mutation)
{
    if (player_equip_unrand(UNRAND_LEAR) && _lears_takes_slot(eq))
        eq = EQ_BODY_ARMOUR;

    set<equipment_type> r;
    r.insert(eq);
    _remove_equipment(r, you.form, meld, mutation);
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

static bool _flying_in_new_form(transformation which_trans)
{
    if (get_form(which_trans)->forbids_flight())
        return false;

    // sources of permanent flight besides equipment
    if (you.racial_permanent_flight())
        return true;

    // not airborne right now (XX does this handle emergency flight correctly?)
    if (!you.duration[DUR_FLIGHT] && !you.attribute[ATTR_PERM_FLIGHT])
        return false;

    // tempflight (e.g. from potion) enabled, no need for equip check
    if (you.duration[DUR_FLIGHT])
        return true;

    // Finally, do the calculation about what would be melded: are there equip
    // sources left?
    int sources = you.equip_flight();
    int sources_removed = 0;
    for (auto eq : _init_equipment_removal(which_trans))
    {
        item_def *item = you.slot_item(eq, true);
        if (item == nullptr)
            continue;
        item_def inf = get_item_known_info(*item);

        //similar code to safe_to_remove from item-use.cc
        if (inf.is_type(OBJ_JEWELLERY, RING_FLIGHT))
            sources_removed++;
        if (inf.base_type == OBJ_ARMOUR && inf.brand == SPARM_FLYING)
            sources_removed++;
        if (is_artefact(inf) && artefact_known_property(inf, ARTP_FLY))
            sources_removed++;
    }

    return sources > sources_removed;
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
 * @return                  If the feat is lethal for the player in the form.
 */
bool feat_dangerous_for_form(transformation which_trans,
                             dungeon_feature_type feat)
{
    // Everything is okay if we can fly.
    if (form_can_fly(which_trans) || _flying_in_new_form(which_trans))
        return false;

    if (feat == DNGN_LAVA)
        return true;

    if (feat == DNGN_DEEP_WATER)
        return !you.can_water_walk() && !form_can_swim(which_trans);

    return false;
}

/**
 * If we transform into the given form, will all of our stats remain above 0,
 * based purely on the stat modifiers of the current & destination form?
 *
 * May prompt the player.
 *
 * @param new_form  The form to check the safety of.
 * @param quiet     Whether to prompt the player.
 * @return          Whether it's okay to go ahead with the transformation.
 */
bool check_form_stat_safety(transformation new_form, bool quiet)
{
    const int str_mod = get_form(new_form)->str_mod - get_form()->str_mod;
    const int dex_mod = get_form(new_form)->dex_mod - get_form()->dex_mod;

    const bool bad_str = you.strength() > 0 && str_mod + you.strength() <= 0;
    const bool bad_dex = you.dex() > 0 && dex_mod + you.dex() <= 0;
    if (!bad_str && !bad_dex)
        return true;
    if (quiet)
        return false;

    string prompt = make_stringf("%s will reduce your %s to zero. Continue?",
                                 new_form == transformation::none
                                     ? "Turning back"
                                     : "Transforming",
                                 bad_str ? "strength" : "dexterity");
    if (yesno(prompt.c_str(), false, 'n'))
        return true;

    canned_msg(MSG_OK);
    return false;
}

static int _transform_duration(transformation which_trans, int pow)
{
    return get_form(which_trans)->get_duration(pow);
}

/**
 * Is the player alive enough to become the given form?
 *
 * All undead can enter shadow form; vampires also can enter batform, and, when
 * full, other forms (excepting lichform).
 *
 * @param which_trans   The transformation which the player is undergoing
 *                      (default you.form).
 * @param involuntary   Whether the transformation is involuntary or not.
 * @param temp                   Whether to factor in temporary limits, e.g. wrong blood level.
 * @return              UFR_GOOD if the player is not blocked from entering the
 *                      given form by their undead race; UFR_TOO_ALIVE if the
 *                      player is too satiated as a vampire; UFR_TOO_DEAD if
 *                      the player is too dead (or too thirsty as a vampire).
 */
undead_form_reason lifeless_prevents_form(transformation which_trans,
                                          bool involuntary, bool temp)
{
    if (!you.undead_state(false)) // intentionally don't pass temp in here
        return UFR_GOOD; // not undead!

    if (which_trans == transformation::none)
        return UFR_GOOD; // everything can become itself

    if (which_trans == transformation::slaughter)
        return UFR_GOOD; // Godly power can transcend such things as unlife

    if (!you.has_mutation(MUT_VAMPIRISM))
        return UFR_TOO_DEAD; // ghouls & mummies can't become anything else

    if (which_trans == transformation::death)
        return UFR_TOO_DEAD; // vampires can never lichform

    if (which_trans == transformation::bat) // can batform bloodless
    {
        if (involuntary)
            return UFR_TOO_DEAD; // but not as a forced polymorph effect

        return !you.vampire_alive || !temp ? UFR_GOOD : UFR_TOO_ALIVE;
    }

    // other forms can only be entered when alive
    return you.vampire_alive || !temp ? UFR_GOOD : UFR_TOO_DEAD;
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
    if (lifeless_prevents_form(which_trans, involuntary, temp) == UFR_TOO_DEAD)
        return "Your unliving flesh cannot be transformed in this way.";

    if (SP_GARGOYLE == you.species && which_trans == transformation::statue)
        return "You're already a statue.";

    if (!temp)
        return "";

    if (you.transform_uncancellable)
        return "You are stuck in your current form!";

    const auto feat = env.grid(you.pos());
    if (feat_dangerous_for_form(which_trans, feat))
    {
        return make_stringf("You would %s in your new form.",
                            feat == DNGN_DEEP_WATER ? "drown" : "burn");
    }

    if (which_trans == transformation::death && you.duration[DUR_DEATHS_DOOR])
        return "You cannot mock death while in death's door.";

    return "";
}

bool check_transform_into(transformation which_trans, bool involuntary)
{
    const string reason = cant_transform_reason(which_trans, involuntary);
    if (!reason.empty())
    {
        if (!involuntary)
            mpr(reason);
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

    default:
        break;
    }
}

void set_form(transformation which_trans, int dur)
{
    const transformation old_form = you.form;
    you.form = which_trans;
    you.duration[DUR_TRANSFORMATION] = dur * BASELINE_DELAY;
    update_player_symbol();

    const int str_mod = get_form(which_trans)->str_mod;
    const int dex_mod = get_form(which_trans)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, dex_mod, true);

    // Don't scale HP when going from nudity to a talisman form
    // or vice versa. This is to discourage regenerating in a -90%
    // underskilled talisman form and scaling back up to full, or
    // leaving a +HP form to regen.
    // Do scale HP when entering or leaving eg tree form, regardless
    // of whether you're going from a talisman form or not.
    const bool leaving_default = you.default_form == old_form
                                 && which_trans == transformation::none;
    const bool entering_default = you.default_form == which_trans
                                 && old_form == transformation::none;
    const bool scale_hp = !entering_default && !leaving_default;
    calc_hp(scale_hp);

    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;
    quiver::set_needs_redraw();
}

static void _enter_form(int pow, transformation which_trans, bool was_flying)
{
    set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);

    if (form_changes_physiology(which_trans))
        merfolk_stop_swimming();

    // Give the transformation message.
    mpr(get_form(which_trans)->transform_message(was_flying));

    // Update your status.
    // Order matters here, take stuff off (and handle attendant HP and stat
    // changes) before adjusting the player to be transformed.
    _remove_equipment(rem_stuff, which_trans);

    set_form(which_trans, _transform_duration(which_trans, pow));

    if (you.digging && !form_keeps_mutations(which_trans))
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

    // Land the player if we stopped flying.
    if (was_flying && !you.airborne())
        move_player_to_grid(you.pos(), false);

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
 * @param pow               The power of the transformation (equivalent to
 *                          spellpower of form spells)
 * @param which_trans       The form which the player should become.
 * @param involuntary       Checks for inscription warnings are skipped, and
 *                          failure is silent.
 * @return                  If the player was transformed, or if they were
 *                          already in the given form, returns true.
 *                          Otherwise, false.
 */
bool transform(int pow, transformation which_trans, bool involuntary)
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

    if (!check_transform_into(which_trans, involuntary))
        return false;

    // This must occur before the untransform().
    if (you.form == which_trans)
    {
        // update power
        if (which_trans != transformation::none)
        {
            you.redraw_armour_class = true;
            // ^ could check more carefully for the exact cases, but I'm
            // worried about making the code too fragile
        }

        int dur = _transform_duration(which_trans, pow);
        if (you.duration[DUR_TRANSFORMATION] < dur * BASELINE_DELAY)
        {
            mpr("You extend your transformation's duration.");
            you.duration[DUR_TRANSFORMATION] = dur * BASELINE_DELAY;

        }
        else if (!involuntary && which_trans != transformation::none)
            mpr("You fail to extend your transformation any further.");

        return true;
    }

    const bool was_flying = you.airborne();

    if (you.form != transformation::none)
        untransform(true);

    _enter_form(pow, which_trans, was_flying);

    return true;
}

/**
 * End the player's transformation and return them to having no
 * form.
 * @param skip_move      If true, skip any move that was in progress before
 *                       the transformation ended.
 */
void untransform(bool skip_move)
{
    const transformation old_form = you.form;
    const bool was_flying = you.airborne();

    if (!form_can_wield(old_form))
        you.received_weapon_warning = false;

    // We may have to unmeld a couple of equipment types.
    set<equipment_type> melded = _init_equipment_removal(old_form);

    const string message = get_form(old_form)->get_untransform_message();
    if (!message.empty())
        mprf(MSGCH_DURATION, "%s", message.c_str());

    set_form(transformation::none, 0);

    const int str_mod = get_form(old_form)->str_mod;
    const int dex_mod = get_form(old_form)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, -str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, -dex_mod, true);

    // If you're a mer in water, boots stay melded even after the form ends.
    if (you.fishtail)
    {
        melded.erase(EQ_BOOTS);
        const item_def* arm = you.slot_item(EQ_BODY_ARMOUR, true);
        if (arm && is_unrandom_artefact(*arm, UNRAND_LEAR))
        {
            // I hate you, King Lear.
            melded.erase(EQ_HELMET);
            melded.erase(EQ_GLOVES);
            melded.erase(EQ_BODY_ARMOUR);
        }
    }
    _unmeld_equipment(melded);

    if (old_form == transformation::death)
    {
        _print_death_brand_changes(you.weapon(), false);
        _print_death_brand_changes(you.offhand_weapon(), false);
    }

    // Update skill boosts for the current state of equipment melds
    // Must happen before the HP check!
    ash_check_bondage();

    if (!skip_move)
    {
        // Land the player if we stopped flying.
        if (is_feat_dangerous(env.grid(you.pos())))
            enable_emergency_flight();
        else if (was_flying && !you.airborne())
            move_player_to_grid(you.pos(), false);

        // Update merfolk swimming for the form change.
        if (you.has_innate_mutation(MUT_MERTAIL))
            merfolk_check_swimming(env.grid(you.pos()), false);
    }

#ifdef USE_TILE
    if (you.has_innate_mutation(MUT_MERTAIL))
        init_player_doll();
#endif

    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr

    // Removed barding check, no transformed creatures can wear barding
    // anyway.
    // *coughs* Ahem, blade hands... -- jpeg
    if (you.can_wear_barding())
    {
        const int arm = you.equip[EQ_BOOTS];

        if (arm != -1 && you.inv[arm].sub_type == ARM_BOOTS)
            remove_one_equip(EQ_BOOTS);
    }

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

    you.turn_is_over = true;
    if (you.transform_uncancellable)
        you.transform_uncancellable = false;

    if (old_form == transformation::slaughter)
        makhleb_enter_crucible_of_flesh(15);
}

void return_to_default_form()
{
    if (you.default_form == transformation::none)
        untransform();
    else
    {
        // Forcibly break out of forced forms at this point (since this should
        // only be called in situations where those should end and transform()
        // will refuse to do that on its own)
        if (you.transform_uncancellable)
            untransform(true);
        transform(0, you.default_form, true);
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

    if (!you.melded[EQ_BOOTS])
    {
        remove_one_equip(EQ_BOOTS);
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

    if (!_init_equipment_removal(you.form).count(EQ_BOOTS))
    {
        unmeld_one_equip(EQ_BOOTS);
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
        unequip_artefact_effect(talisman, nullptr, false, EQ_NONE, false);
}

void set_default_form(transformation t, const item_def *source)
{
    item_def talisman = you.active_talisman;
    you.active_talisman.clear();
    you.default_form = t;

    if (is_artefact(talisman))
        unequip_artefact_effect(talisman, nullptr, false, EQ_NONE, false);

    if (source)
    {
        you.active_talisman = *source; // iffy
        if (is_artefact(you.active_talisman))
            equip_artefact_effect(you.active_talisman, nullptr, false, EQ_NONE);
    }
}

void vampire_update_transformations()
{
    const undead_form_reason form_reason = lifeless_prevents_form();
    if (form_reason != UFR_GOOD && you.duration[DUR_TRANSFORMATION])
    {
        print_stats();
        update_screen();
        mprf(MSGCH_WARN,
             "Your blood-%s body can't sustain your transformation.",
             form_reason == UFR_TOO_DEAD ? "deprived" : "filled");
        unset_default_form();
        untransform();
    }
}

// TODO: dataify? move to member functions?
int form_base_movespeed(transformation tran)
{
    // statue form is handled as a multiplier in player_speed, not a movespeed.
    if (tran == transformation::bat)
        return 5; // but allowed minimum is six
    else if (tran == transformation::pig)
        return 7;
    else
        return 10;
}

bool draconian_dragon_exception()
{
    return species::is_draconian(you.species)
           && (you.form == transformation::dragon
               || !form_changes_physiology());
}

transformation form_for_talisman(const item_def &talisman)
{
    for (int i = 0; i < NUM_TRANSFORMS; i++)
        if (formdata[i].talisman == talisman.sub_type)
            return static_cast<transformation>(i);
    return transformation::none;
}

static void _pad_talisman_descs(vector<pair<string,string>> &descs)
{
    size_t max_len = 0;
    for (const pair<string,string> &d : descs)
        if (d.first.size() > max_len)
            max_len = d.first.size();
    for (pair<string,string> &d : descs)
        d.second = string(max_len - d.first.size(), ' ') + d.second;
}

static string _int_with_plus(int i)
{
    if (i < 0)
        return make_stringf("%d", i);
    return make_stringf("+%d", i);
}

static string _maybe_desc_prop(int val, int max = -1)
{
    if (val == 0 && max <= 0)
        return "";
    const string base = _int_with_plus(val);
    if (max == val || max == -1)
        return base;
    return base + make_stringf(" (%s at max skill)",
                               _int_with_plus(max).c_str());
}

static void _maybe_add_prop(vector<pair<string, string>> &props, string name,
                            int val, int max = -1)
{
    const string desc = _maybe_desc_prop(val, max);
    if (!desc.empty())
        props.push_back(pair<string, string>(name, desc));
}

void describe_talisman_form(transformation form_type, talisman_form_desc &d,
                            bool incl_special /* hack - TODO REMOVEME */)
{
    const Form* form = get_form(form_type);
    string minskill_desc = to_string(form->min_skill);
    const int sk = you.skill(SK_SHAPESHIFTING, 10);
    const bool below_min = sk/10 < form->min_skill;
    if (below_min)
        minskill_desc += " (insufficient skill lowers this form's max HP)";
    d.skills.push_back(pair<string, string>("Minimum skill", minskill_desc));
    d.skills.push_back(pair<string, string>("Maximum skill", to_string(form->max_skill)));
    if (incl_special)
    {
        const string sk_desc = make_stringf("%d.%d", sk / 10, sk % 10);
        d.skills.push_back(pair<string, string>("Your skill", sk_desc));
    }

    const int hp = form->mult_hp(100, true);
    if (below_min || hp != 100)
    {
        string hp_desc = make_stringf("%d%%", hp);
        if (below_min)
            hp_desc += " (reduced by your low skill)";
        d.defenses.push_back(pair<string,string>("HP", hp_desc));
    }
    _maybe_add_prop(d.defenses, "Bonus AC", form->get_ac_bonus() / 100,
                                            form->get_ac_bonus(true) / 100);
    _maybe_add_prop(d.defenses, "Bonus EV", form->ev_bonus(),
                                            form->ev_bonus(true));

    const int body_ac_loss_percent = form->get_base_ac_penalty(100);
    const bool loses_body_ac = body_ac_loss_percent && you_can_wear(EQ_BODY_ARMOUR) != false;
    if (loses_body_ac)
    {
        const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const int base_ac = body_armour ? property(*body_armour, PARM_AC) : 0;
        const int ac_penalty = form->get_base_ac_penalty(base_ac);
        const string body_loss = make_stringf("-%d (-%d%% of your body armour's %d base AC)",
                                              ac_penalty, body_ac_loss_percent, base_ac);
        d.defenses.push_back(pair<string,string>("AC", body_loss));
    }
    if (form->size != SIZE_CHARACTER)
        d.defenses.push_back(pair<string,string>("Size", uppercase_first(get_size_adj(form->size))));

    const int normal_uc = 3; // TODO: dedup this
    const int uc = form->get_base_unarmed_damage(false) - normal_uc;
    const int max_uc = form->get_base_unarmed_damage(false, true) - normal_uc;
    _maybe_add_prop(d.offenses, "UC base dam+", uc, max_uc);
    _maybe_add_prop(d.offenses, "Slay", form->slay_bonus(false),
                                         form->slay_bonus(false, true));
    switch (form_type) {
    case transformation::statue:
        d.offenses.push_back(pair<string, string>("Melee damage", "+50%"));
        break;
    case transformation::flux:
    {
        d.offenses.push_back(pair<string, string>("Melee damage", "-33%"));
        const int contam_dam = form->contam_dam(false);
        const int max_contam_dam = form->contam_dam(false, true);
        _maybe_add_prop(d.offenses, "Contam damage", contam_dam, max_contam_dam);
        break;
    }
    case transformation::maw:
    {
        if (!incl_special)
            break;
        const int aux_dam = form->get_aux_damage(false);
        const int max_aux_dam = form->get_aux_damage(false, true);
        _maybe_add_prop(d.offenses, "Maw damage", aux_dam, max_aux_dam);
        break;
    }
    case transformation::dragon:
    {
        if (!incl_special)
            break;
        _maybe_add_prop(d.offenses, "Bite dam", 1 + DRAGON_FANGS * 2);
        _maybe_add_prop(d.offenses, "Tail slap dam", 6); // big time hack alert
        break;
    }
    default:
        break;
    }
    _maybe_add_prop(d.offenses, "Str", form->str_mod);
    _maybe_add_prop(d.offenses, "Dex", form->dex_mod);

   _pad_talisman_descs(d.skills);
   _pad_talisman_descs(d.defenses);
   _pad_talisman_descs(d.offenses);
}

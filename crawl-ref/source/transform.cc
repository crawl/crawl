/**
 * @file
 * @brief Misc function related to player transformations.
**/

#include "AppHdr.h"

#include "transform.h"

#include <stdio.h>
#include <string.h>

#include "artefact.h"
#include "art-enum.h"
#include "cloud.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "godabil.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "mon-death.h"
#include "mutation.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

// transform slot enums into flags
#define SLOTF(s) (1 << s)

static const int EQF_NONE = 0;
// "hand" slots (not rings)
static const int EQF_HANDS = SLOTF(EQ_WEAPON) | SLOTF(EQ_SHIELD)
                             | SLOTF(EQ_GLOVES);
// core body slots (statue form)
static const int EQF_STATUE = SLOTF(EQ_GLOVES) | SLOTF(EQ_BOOTS)
                              | SLOTF(EQ_BODY_ARMOUR);
// more core body slots (Lear's Hauberk)
static const int EQF_LEAR = EQF_STATUE | SLOTF(EQ_HELMET);
// everything you can (W)ear
static const int EQF_WEAR = EQF_LEAR | SLOTF(EQ_CLOAK) | SLOTF(EQ_SHIELD);
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

static const FormAttackVerbs DEFAULT_VERBS = FormAttackVerbs(NULL, NULL,
                                                             NULL, NULL);
static const FormAttackVerbs ANIMAL_VERBS = FormAttackVerbs("hit", "bite",
                                                            "maul", "maul");

static const FormDuration DEFAULT_DURATION = FormDuration(20, PS_DOUBLE, 100);
static const FormDuration BAD_DURATION = FormDuration(15, PS_ONE_AND_A_HALF,
                                                      100);

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

    return slot_available(get_armour_slot(item));
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
        case PS_TENTH:
            return random2(pow)/10;
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
                        description.c_str());
}

/**
 * Get a message for transforming into this form, based on your current
 * situation (e.g. in water...)
 *
 * @return The message for turning into this form.
 */
string Form::transform_message(transformation_type previous_trans) const
{
    // XXX: refactor this into a second function (and also rethink the logic)
    string start = "Buggily, y";
    if (you.in_water() && player_can_fly())
        start = "You fly out of the water as y";
    else if (get_form(previous_trans)->player_can_fly()
             && player_can_swim()
             && feat_is_water(grd(you.pos())))
        start = "As you dive into the water, y";
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
int Form::get_ac_bonus() const
{
    return flat_ac * 100
           + power_ac * you.props[TRANSFORM_POW_KEY].get_int()
           + xl_ac * you.experience_level;
}

/**
 * (freeze)
 */
static string _brand_suffix(int brand)
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
    if (uc_attack == "")
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
 * Does this form provide resistance to rotting?
 */
bool Form::res_rot() const
{
    return get_resist(resists, MR_RES_ROTTING);
}

/**
 * Does this form provide resistance against acid?
 */
bool Form::res_acid() const
{
    return get_resist(resists, MR_RES_ACID);
}

/**
 * Does this form provide resistance to sticky flame?
 */
bool Form::res_sticky_flame() const
{
    return get_resist(resists, MR_RES_STICKY_FLAME);
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
 * Does this form disable swimming?
 *
 * @return Whether swimming is always impossible while in this form.
 */
bool Form::forbids_swimming() const
{
    return can_swim == FC_FORBID;
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
               || you.racial_permanent_flight() && you.permanent_flight());
}

/**
 * Can the player swim, if in this form?
 *
 * DOES consider player state besides form.
 * @return  Whether the player will be able to swim in this form.
 */
bool Form::player_can_swim() const
{
    const size_type player_size = size == SIZE_CHARACTER ?
                                          you.body_size(PSIZE_BODY, true) :
                                          size;
    return can_swim == FC_ENABLE
           || species_can_swim(you.species) && can_swim != FC_FORBID
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
    if (can_fly == FC_ENABLE && prayer_action != "")
        return prayer_action;
    // Otherwise, if you're flying, use the generic flying action.
    // XXX: if we ever get a default-permaflying species again that wants to
    // have a separate verb, we'll want to check for that right here.
    if (you.flight_mode())
        return "hover solemnly before";
    // Otherwise, if you have a verb, use that...
    if (prayer_action != "")
        return prayer_action;
    // Finally, default to your species' verb.
    return species_prayer_action(you.species);
}


class FormNone : public Form
{
private:
    FormNone()
    : Form("", "", "none", // short name, long name, wizmode name
           "",  // description
           EQF_NONE,  // blocked slots
           MR_NO_FLAGS, // resists
           FormDuration(0, PS_NONE, 0), // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 0,    // size, hp mod, stealth mod
           0, 0, 0,   // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_DEFAULT,     // can_fly, can_swim
           FC_DEFAULT, true, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "",              // prayer action
           MONS_PLAYER)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormNone);
public:
    static const FormNone &instance() { static FormNone inst; return inst; }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const { return "your old self."; }
};

class FormSpider : public Form
{
private:
    FormSpider()
    : Form("Spider", "spider-form", "spider", // short name, long name, wizmode name
           "a venomous arachnid creature.",  // description
           EQF_PHYSICAL,  // blocked slots
           MR_VUL_POISON, // resists
           FormDuration(10, PS_DOUBLE, 60), // duration
           0, 5,    // str mod, dex mod
           SIZE_TINY, 10, 21,    // size, hp mod, stealth mod
           2, 0, 0,    // flat AC, spellpower AC, XL AC
           true, 10,                 // can_cast, spellcasting penalty
           10, 5, SPWPN_VENOM, LIGHTGREEN,  // unarmed acc bonus, damage, brand, & ui colour
           "Fangs",             // name of unarmed-combat "weapon" (in UI)
           ANIMAL_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, true, false,        // can_bleed, breathes, keeps_mutations
           "hiss", -4,          // shout verb, shout volume modifier
           "front leg", "",          // hand name, foot name
           "crawl onto",              // prayer action
           MONS_SPIDER)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormSpider);
public:
    static const FormSpider &instance() { static FormSpider inst; return inst; }
};

class FormBlade : public Form
{
private:
    FormBlade()
    : Form("Blade", "", "blade", // short name, long name, wizmode name
           "",  // description
           EQF_HANDS,  // blocked slots
           MR_NO_FLAGS, // resists
           FormDuration(10, PS_SINGLE, 100), // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 0,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           true, 20,                 // can_cast, spellcasting penalty
           12, -1, SPWPN_NORMAL, RED,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("hit", "slash", "slice", "shred"), // verbs used for uc
           FC_DEFAULT, FC_DEFAULT,     // can_fly, can_swim
           FC_DEFAULT, true, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "scythe-like blade", "",          // hand name, foot name
           "",              // prayer action
           MONS_PLAYER)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormBlade);
public:
    static const FormBlade &instance() { static FormBlade inst; return inst; }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const
    {
        return 8 + div_rand_round(you.strength() + you.dex(), 3);
    }

    /**
     * % screen description
     */
    string get_long_name() const
    {
        return "blade " + blade_parts(true);
    }

    /**
     * @ description
     */
    string get_description(bool past_tense) const
    {
        return make_stringf("You %s blades for %s.",
                            past_tense ? "had" : "have",
                            blade_parts().c_str());
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message(transformation_type previous_trans) const
    {
        const bool singular = player_mutation_level(MUT_MISSING_HAND);

        // XXX: a little ugly
        return make_stringf("Your %s turn%s into%s razor-sharp scythe blade%s.",
                            blade_parts().c_str(), singular ? "s" : "",
                            singular ? " a" : "", singular ? "" : "s");
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const
    {
        const bool singular = player_mutation_level(MUT_MISSING_HAND);

        // XXX: a little ugly
        return make_stringf("Your %s revert%s to %s normal proportions.",
                            blade_parts().c_str(), singular ? "s" : "",
                            singular ? "its" : "their");
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string default_name) const
    {
        return "Blade " + blade_parts(true);
    }
};

class FormStatue : public Form
{
private:
    FormStatue()
    : Form("Statue", "statue-form", "statue", // short name, long name, wizmode name
           "a stone statue.",  // description
           EQF_STATUE,  // blocked slots
           MR_RES_ELEC | MR_RES_NEG, // resists - rrot implied by holiness
           DEFAULT_DURATION, // duration
           2, -2,    // str mod, dex mod
           SIZE_CHARACTER, 13, 0,    // size, hp mod, stealth mod
           17, 10, 0,    // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           9, -1, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, false, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "place yourself before",              // prayer action
           MONS_STATUE)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormStatue);
public:
    static const FormStatue &instance() { static FormStatue inst; return inst; }

    /**
     * The AC bonus of the form, multiplied by 100 to match
     * player::armour_class().
     */
    int get_ac_bonus() const
    {
        return Form::get_ac_bonus()
               - (you.species == SP_GARGOYLE ? 400 : 0);
    }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const
    {
        return 6 + div_rand_round(you.strength(), 3);
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message(transformation_type previous_trans) const
    {
        if (you.species == SP_DEEP_DWARF && one_chance_in(10))
            return "You inwardly fear your resemblance to a lawn ornament.";
        else if (you.species == SP_GARGOYLE)
            return "Your body stiffens and grows slower.";
        else
            return Form::transform_message(previous_trans);
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const
    {
        return "a living statue of rough stone.";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const
    {
        // This only handles lava orcs going statue -> stoneskin.
        if (
#if TAG_MAJOR_VERSION == 34
            you.species == SP_LAVA_ORC && temperature_effect(LORC_STONESKIN)
            ||
#endif
            you.species == SP_GARGOYLE)
        {
            return "You revert to a slightly less stony form.";
        }
#if TAG_MAJOR_VERSION == 34
        if (you.species != SP_LAVA_ORC)
#endif
            return "You revert to your normal fleshy form.";
#if TAG_MAJOR_VERSION == 34
        return Form::get_untransform_message();
#endif
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string default_name) const
    {
        if (you.has_usable_claws(true))
            return "Stone claws";
        if (you.has_usable_tentacles(true))
            return "Stone tentacles";

        const bool singular = player_mutation_level(MUT_MISSING_HAND);
        return make_stringf("Stone fist%s", singular ? "" : "s");
    }
};

class FormIce : public Form
{
private:
    FormIce()
    : Form("Ice", "ice-form", "ice", // short name, long name, wizmode name
           "a creature of crystalline ice.",  // description
           EQF_PHYSICAL,  // blocked slots
           MR_RES_POISON | MR_VUL_FIRE | mrd(MR_RES_COLD, 3), // resists
           FormDuration(30, PS_DOUBLE, 100), // duration
           0, 0,    // str mod, dex mod
           SIZE_LARGE, 12, 15,    // size, hp mod, stealth mod
           5, 7, 0,    // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           10, 12, SPWPN_FREEZING, WHITE,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_ENABLE,     // can_fly, can_swim
           FC_FORBID, true, false,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "front paw", "paw",          // hand name, foot name
           "bow your head before",              // prayer action
           MONS_ICE_BEAST)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormIce);
public:
    static const FormIce &instance() { static FormIce inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const
    {
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_LAVA_ORC && !temperature_effect(LORC_STONESKIN))
            return "Your icy form melts away into molten rock.";
        else
#endif
            return "You warm up again.";
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string default_name) const
    {
        const bool singular = player_mutation_level(MUT_MISSING_HAND);
        return make_stringf("Ice fist%s", singular ? "" : "s");
    }
};

class FormDragon : public Form
{
private:
    FormDragon()
    : Form("Dragon", "dragon-form", "dragon", // short name, long name, wizmode name
           "a fearsome dragon!",  // description
           EQF_PHYSICAL,  // blocked slots
           MR_RES_POISON, // resists - most handled in functions
           DEFAULT_DURATION, // duration
           10, 0,    // str mod, dex mod
           SIZE_GIANT, 15, 6,    // size, hp mod, stealth mod
           16, 0, 0,    // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           10, -1, SPWPN_NORMAL, GREEN,  // unarmed acc bonus, damage, brand, & ui colour
           "Teeth and claws",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("hit", "claw", "bite", "maul"), // verbs used for uc
           FC_ENABLE, FC_FORBID,     // can_fly, can_swim
           FC_ENABLE, true, false,        // can_bleed, breathes, keeps_mutations
           "roar", 6,          // shout verb, shout volume modifier
           "foreclaw", "",          // hand name, foot name
           "bow your head before",              // prayer action
           MONS_PROGRAM_BUG)       // equivalent monster
    { };

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
    monster_type get_equivalent_mons() const
    {
        return dragon_form_dragon_type();
    }

    /**
     * The AC bonus of the form, multiplied by 100 to match
     * player::armour_class().
     */
    int get_ac_bonus() const
    {
        if (player_genus(GENPC_DRACONIAN))
            return 1000;
        return Form::get_ac_bonus();
    }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const
    {
        return 12 + div_rand_round(you.strength() * 2, 3);
    }

    /**
     * How many levels of resistance against fire does this form provide?
     */
    int res_fire() const
    {
        switch (dragon_form_dragon_type())
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
    int res_cold() const
    {
        switch (dragon_form_dragon_type())
        {
            case MONS_ICE_DRAGON:
                return 2;
            case MONS_FIRE_DRAGON:
                return -1;
            default:
                return 0;
        }
    }
};

class FormLich : public Form
{
private:
    FormLich()
    : Form("Lich", "lich-form", "lich", // short name, long name, wizmode name
           "a lich.",  // description
           EQF_NONE,  // blocked slots
           MR_RES_COLD | mrd(MR_RES_NEG, 3), // resists - rp & rrot implied
           DEFAULT_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 0,    // size, hp mod, stealth mod
           6, 0, 0,   // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           10, 5, SPWPN_DRAINING, MAGENTA,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_DEFAULT,     // can_fly, can_swim
           FC_FORBID, false, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "",              // prayer action
           MONS_LICH)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormLich);
public:
    static const FormLich &instance() { static FormLich inst; return inst; }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message(transformation_type previous_trans) const
    {
        return "Your body is suffused with negative energy!";
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const
    {
        if (you.undead_state() == US_ALIVE)
            return "You feel yourself come back to life.";
        return "You feel your undeath return to normal.";
        // ^^^ vampires only, probably
    }
};

class FormBat : public Form
{
private:
    FormBat()
    : Form("Bat", "bat-form", "bat", // short name, long name, wizmode name
           "",  // description
           EQF_PHYSICAL | EQF_RINGS,  // blocked slots
           MR_NO_FLAGS, // resists
           DEFAULT_DURATION, // duration
           -5, 5,    // str mod, dex mod
           SIZE_TINY, 10, 17,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           false, 10,                 // can_cast, spellcasting penalty
           12, -1, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "Teeth",             // name of unarmed-combat "weapon" (in UI)
           ANIMAL_VERBS, // verbs used for uc
           FC_ENABLE, FC_FORBID,     // can_fly, can_swim
           FC_ENABLE, true, false,        // can_bleed, breathes, keeps_mutations
           "squeak", -8,          // shout verb, shout volume modifier
           "foreclaw", "",          // hand name, foot name
           "perch on",              // prayer action
           MONS_PROGRAM_BUG)       // equivalent monster
    { };

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
    monster_type get_equivalent_mons() const
    {
        return you.species == SP_VAMPIRE ? MONS_VAMPIRE_BAT : MONS_BAT;
    }

    /**
     * Get a multiplier for skill when calculating stealth values.
     *
     * @return  The stealth modifier for the given form. (0 = default to
     *          racial values.)
     */
    int get_stealth_mod() const
    {
        // vampires handle bat stealth in racial code
        return you.species == SP_VAMPIRE ? 0 : stealth_mod;
    }

    /**
     * What brand type does this form attack with when unarmed?
     */
    brand_type get_uc_brand() const
    {
        // thematic but probably irrelevant
        if (you.species == SP_VAMPIRE)
            return SPWPN_VAMPIRISM;
        return Form::get_uc_brand();
    }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const
    {
        return you.species == SP_VAMPIRE ? 2 : 1;
    }

    string get_description(bool past_tense) const
    {
        return make_stringf("You %s in %sbat-form.",
                            past_tense ? "were" : "are",
                            you.species == SP_VAMPIRE ?  "vampire-" : "");
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const
    {
        return make_stringf("a %sbat.",
                            you.species == SP_VAMPIRE ? "vampire " : "");
    }
};

class FormPig : public Form
{
private:
    FormPig()
    : Form("Pig", "pig-form", "pig", // short name, long name, wizmode name
           "a filthy swine.",  // description
           EQF_PHYSICAL | EQF_RINGS,  // blocked slots
           MR_NO_FLAGS, // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_SMALL, 10, 9,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           false, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "Teeth",             // name of unarmed-combat "weapon" (in UI)
           ANIMAL_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,  // can_fly (false for most pigs), can_swim
           FC_ENABLE, true, false,        // can_bleed, breathes, keeps_mutations
           "squeal", 0,          // shout verb, shout volume modifier
           "front trotter", "trotter",          // hand name, foot name
           "bow your head before",              // prayer action
           MONS_HOG)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormPig);
public:
    static const FormPig &instance() { static FormPig inst; return inst; }
};

class FormAppendage : public Form
{
private:
    FormAppendage()
    : Form("App", "appendage", "appendage", // short name, long name, wizmode name
           "",  // description
           EQF_NONE,  // blocked slots
           MR_NO_FLAGS, // resists
           FormDuration(10, PS_DOUBLE, 60), // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 0,    // size, hp mod, stealth mod
           0, 0, 0,   // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_DEFAULT,     // can_fly, can_swim
           FC_DEFAULT, true, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "",              // prayer action
           MONS_PLAYER)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormAppendage);
public:
    static const FormAppendage &instance()
    {
        static FormAppendage inst;
        return inst;
    }

    string get_description(bool past_tense) const
    {
        if (you.attribute[ATTR_APPENDAGE] == MUT_TENTACLE_SPIKE)
        {
            return make_stringf("One of your tentacles %s a temporary spike.",
                                 past_tense ? "had" : "has");
        }

        return make_stringf("You %s grown temporary %s.",
                            past_tense ? "had" : "have",
                            mutation_name((mutation_type)
                                          you.attribute[ATTR_APPENDAGE]));
    }

    /**
     * Get a message for transforming into this form.
     */
    string transform_message(transformation_type previous_trans) const
    {
        // ATTR_APPENDAGE must be set earlier!
        switch (you.attribute[ATTR_APPENDAGE])
        {
            case MUT_HORNS:
                return "You grow a pair of large bovine horns.";
            case MUT_TENTACLE_SPIKE:
                return "One of your tentacles grows a vicious spike.";
            case MUT_TALONS:
                return "Your feet morph into talons.";
            default:
                 die("Unknown beastly appendage.");
        }
    }

    /**
     * Get a message for untransforming from this form. (Handled elsewhere.)
     */
    string get_untransform_message() const { return ""; }
};

class FormTree : public Form
{
private:
    FormTree()
    : Form("Tree", "tree-form", "tree", // short name, long name, wizmode name
           "a tree.",  // description
           EQF_LEAR | SLOTF(EQ_CLOAK),  // blocked slots
           MR_RES_POISON | mrd(MR_RES_NEG, 3), // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 15, 27,    // size, hp mod, stealth mod
           20, 0, 50,     // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           10, 12, SPWPN_NORMAL, BROWN,  // unarmed acc bonus, damage, brand, & ui colour
           "Branches",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("hit", "smack", "pummel", "thrash"), // verbs used for uc
           FC_FORBID, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, false, false,        // can_bleed, breathes, keeps_mutations
           "creak", 0,          // shout verb, shout volume modifier
           "branch", "root",          // hand name, foot name
           "sway towards",              // prayer action
           MONS_ANIMATED_TREE)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormTree);
public:
    static const FormTree &instance() { static FormTree inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const { return "You feel less woody."; }
};

class FormPorcupine : public Form
{
private:
    FormPorcupine()
    : Form("Porc",  "porcupine-form", "porcupine", // short name, long name, wizmode name
           "a spiny porcupine.",  // description
           EQF_ALL,  // blocked slots
           MR_NO_FLAGS, // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_TINY, 10, 12,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           false, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "Teeth",             // name of unarmed-combat "weapon" (in UI)
           ANIMAL_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_ENABLE, true, false,        // can_bleed, breathes, keeps_mutations
           "squeak", -8,          // shout verb, shout volume modifier
           "front leg", "",          // hand name, foot name
           "curl into a sanctuary of spikes before",           // prayer action
           MONS_PORCUPINE)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormPorcupine);
public:
    static const FormPorcupine &instance()
    {
        static FormPorcupine inst;
        return inst;
    }
};

class FormWisp : public Form
{
private:
    FormWisp()
    : Form("Wisp",  "wisp-form", "wisp", // short name, long name, wizmode name
           "an insubstantial wisp.",  // description
           EQF_ALL,  // blocked slots
           mrd(MR_RES_FIRE, 2) | mrd(MR_RES_COLD, 2) | MR_RES_ELEC | MR_RES_POISON | MR_RES_STICKY_FLAME | mrd(MR_RES_NEG, 3) | MR_RES_ROTTING | MR_RES_ACID, // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_TINY, 10, 21,    // size, hp mod, stealth mod
           5, 0, 50,    // flat AC, spellpower AC, XL AC
           false, 0,                 // can_cast, spellcasting penalty
           10, 5, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "Misty tendrils",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("touch", "hit", "engulf", "engulf"), // verbs used for uc
           FC_ENABLE, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, false, false,        // can_bleed, breathes, keeps_mutations
           "whoosh", -8,          // shout verb, shout volume modifier
           "misty tendril", "strand",          // hand name, foot name
           "swirl around",           // prayer action
           MONS_INSUBSTANTIAL_WISP)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormWisp);
public:
    static const FormWisp &instance() { static FormWisp inst; return inst; }
};

#if TAG_MAJOR_VERSION == 34
class FormJelly : public Form
{
private:
    FormJelly()
    : Form("Jelly",  "jelly-form", "jelly", // short name, long name, wizmode name
           "a lump of jelly.",  // description
           EQF_PHYSICAL | EQF_RINGS,  // blocked slots
           MR_NO_FLAGS, // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 21,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           false, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, LIGHTGREY,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, false, false,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "",           // prayer action
           MONS_JELLY)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormJelly);
public:
    static const FormJelly &instance() { static FormJelly inst; return inst; }
};
#endif

class FormFungus : public Form
{
private:
    FormFungus()
    : Form("Fungus", "fungus-form", "fungus", // short name, long name, wizmode name
           "a sentient fungus.",  // description
           EQF_PHYSICAL & ~SLOTF(EQ_HELMET),  // blocked slots
           MR_RES_POISON | mrd(MR_RES_NEG, 3), // resists
           BAD_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_TINY, 10, 30,    // size, hp mod, stealth mod
           12, 0, 0,    // flat AC, spellpower AC, XL AC
           false, 0,                 // can_cast, spellcasting penalty
           10, 12, SPWPN_CONFUSE, BROWN,  // unarmed acc bonus, damage, brand, & ui colour
           "Spores",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("release spores at", "release spores at", "release spores at", "release spores at"), // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, false, false,        // can_bleed, breathes, keeps_mutations
           "sporulate", -8,          // shout verb, shout volume modifier
           "hypha", "",          // hand name, foot name
           "release spores on",           // prayer action
           MONS_WANDERING_MUSHROOM)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormFungus);
public:
    static const FormFungus &instance() { static FormFungus inst; return inst; }

    /**
     * Can the player wear the given item while in this form?
     *
     * Does not take mutations into account.
     *
     * @param item  The item in question
     * @return      Whether this form prevents the player from wearing the
     *              item. (Other things may also prevent it, of course)
     */
    bool can_wear_item(const item_def& item) const
    {
        if (is_helmet(item) && !is_hard_helmet(item))
            return true; // mushroom caps!
        return Form::can_wear_item(item);
    }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const { return "You stop sporulating."; }
};

class FormShadow : public Form
{
private:
    FormShadow()
    : Form("Shadow",  "shadow-form", "shadow", // short name, long name, wizmode name
           "a swirling mass of dark shadows.",  // description
           EQF_NONE,  // blocked slots
           mrd(MR_RES_POISON, 3) | mrd(MR_RES_NEG, 3) | MR_RES_ROTTING, // resists
           DEFAULT_DURATION, // duration
           0, 0,    // str mod, dex mod
           SIZE_CHARACTER, 10, 30,    // size, hp mod, stealth mod
           0, 0, 0,    // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           0, 3, SPWPN_NORMAL, MAGENTA,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           DEFAULT_VERBS, // verbs used for uc
           FC_DEFAULT, FC_FORBID,     // can_fly, can_swim
           FC_FORBID, true, true,        // can_bleed, breathes, keeps_mutations
           "", 0,          // shout verb, shout volume modifier
           "", "",          // hand name, foot name
           "",           // prayer action
           MONS_PLAYER_SHADOW)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormShadow);
public:
    static const FormShadow &instance() { static FormShadow inst; return inst; }

    /**
     * Get a message for untransforming from this form.
     */
    string get_untransform_message() const
    {
        if (you.invisible())
            return "You feel less shadowy.";
        return "You emerge from the shadows.";
    }
};

/**
 * Set the number of hydra heads that the player currently has.
 *
 * @param heads the new number of heads you should have.
 */
void set_hydra_form_heads(int heads)
{
    you.props[HYDRA_FORM_HEADS_KEY] = min(MAX_HYDRA_HEADS, max(1, heads));
    you.wield_change = true;
}

class FormHydra : public Form
{
private:
    FormHydra()
    : Form("Hydra", "hydra-form", "hydra", // short name, long name, wizmode name
           "",  // description
           EQF_PHYSICAL,  // blocked slots
           MR_RES_POISON, // resists
           FormDuration(4, PS_TENTH, 12), // duration
           8, 0,    // str mod, dex mod
           SIZE_BIG, 15, 6,    // size, hp mod, stealth mod
           6, 5, 0,    // flat AC, spellpower AC, XL AC
           true, 0,                 // can_cast, spellcasting penalty
           10, -1, SPWPN_NORMAL, GREEN,  // unarmed acc bonus, damage, brand, & ui colour
           "",             // name of unarmed-combat "weapon" (in UI)
           FormAttackVerbs("nip at", "bite", "gouge", "chomp"), // verbs used for uc
           FC_DEFAULT, FC_ENABLE,  // can_fly, can_swim
           FC_ENABLE, true, false,        // can_bleed, breathes, keeps_mutations
           "roar", 4,          // shout verb, shout volume modifier
           "foreclaw", "",          // hand name, foot name
           "bow your heads before",           // prayer action
           MONS_HYDRA)       // equivalent monster
    { };

    DISALLOW_COPY_AND_ASSIGN(FormHydra);
public:
    static const FormHydra &instance() { static FormHydra inst; return inst; }

    /**
     * Get a string describing the form you're turning into.
     */
    string get_transform_description() const
    {
        return make_stringf("a %d-headed hydra.", you.heads());
    }

    /**
     * @ description
     */
    string get_description(bool past_tense) const
    {
        return make_stringf("You %s %s",
                            past_tense ? "were" : "are",
                            get_transform_description().c_str());
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string default_name) const
    {
        return make_stringf("Bite (x%d)", you.heads());
    }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const
    {
        // 3 damage per head for 1-10
        const int normal_heads_damage = min(you.heads(), 10) * 3;
        // 3/2 damage per head for 11-20 (they get in each-other's way)
            // (and also a 62-base-damage form scares me)
        const int too_many_heads_damage = max(0, you.heads() - 10)
                                            * 3 / 2;
        // 2-47 (though more like 14-32 in practical ranges...)
        return 2 + normal_heads_damage + too_many_heads_damage;
    }

};

static const Form* forms[] =
{
    &FormNone::instance(),
    &FormSpider::instance(),
    &FormBlade::instance(),
    &FormStatue::instance(),

    &FormIce::instance(),
    &FormDragon::instance(),
    &FormLich::instance(),
    &FormBat::instance(),

    &FormPig::instance(),
    &FormAppendage::instance(),
    &FormTree::instance(),
    &FormPorcupine::instance(),

    &FormWisp::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormJelly::instance(),
#endif
    &FormFungus::instance(),
    &FormShadow::instance(),
    &FormHydra::instance(),
};

const Form* get_form(transformation_type form)
{
    COMPILE_CHECK(ARRAYSZ(forms) == NUM_TRANSFORMS);
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form];
}


static void _extra_hp(int amount_extra);

/**
 * Get the wizmode name of a form.
 *
 * @param form      The form in question.
 * @return          The form's casual, wizmode name.
 */
const char* transform_name(transformation_type form)
{
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form]->wiz_name.c_str();
}

/**
 * Can the player (w)ield weapons when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wield items when in that form.
*/
bool form_can_wield(transformation_type form)
{
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form]->slot_available(EQ_WEAPON);
}

/**
 * Can the player (W)ear armour when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wear armour when in that form.
 */
bool form_can_wear(transformation_type form)
{
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return (forms[form]->blocked_slots & EQF_WEAR) != EQF_WEAR;
}

/**
 * Can the player (W)ear or (P)ut on the given item when in the given form?
 *
 * @param form      The item in question.
 * @param form      The form in question.
 * @return          Whether the player can wear that item when in that form.
 */
bool form_can_wear_item(const item_def& item, transformation_type form)
{
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form]->can_wear_item(item);
}

/**
 * Can the player fly, if in this form?
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to fly in this form.
 */
bool form_can_fly(transformation_type form)
{
    return get_form(form)->player_can_fly();
}


/**
 * Can the player swim, if in this form?
 *
 * (Swimming = traversing deep & shallow water without penalties; includes
 * floating (ice form) and wading forms (giants - currently just dragon form,
 * which normally flies anyway...))
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to swim in this form.
 */
bool form_can_swim(transformation_type form)
{
    return get_form(form)->player_can_swim();
}

/**
 * Can the player survive in deep water when in the given form?
 *
 * Doesn't count flight or beogh water-walking.
 *
 * @param form      The form in question.
 * @return          Whether the player won't be killed when entering deep water
 *                  in that form.
 */
bool form_likes_water(transformation_type form)
{
    // Grey dracs can't swim, so can't statue form merfolk/octopodes
    // -- yet they can still survive in water.
    if (species_likes_water(you.species)
        && (form == TRAN_STATUE || !get_form(form)->forbids_swimming()))
    {
        return true;
    }

    // otherwise, you gotta swim to survive!
    return form_can_swim(form);
}

bool form_likes_lava(transformation_type form)
{
#if TAG_MAJOR_VERSION == 34
    // Lava orcs can only swim in non-phys-change forms.
    // However, ice beast & statue form will melt back to lava, so they're OK
    return you.species == SP_LAVA_ORC
           && (!form_changed_physiology(form)
               || form == TRAN_ICE_BEAST
               || form == TRAN_STATUE);
#else
    return false;
#endif
}

// Used to mark transformations which override species intrinsics.
bool form_changed_physiology(transformation_type form)
{
    return form != TRAN_NONE && form != TRAN_APPENDAGE
           && form != TRAN_BLADE_HANDS;
}

/**
 * Does this form have blood?
 *
 * @param form      The form in question.
 * @return          Whether the form can bleed, sublime, etc.
 */
bool form_can_bleed(transformation_type form)
{
    return get_form(form)->can_bleed != FC_FORBID;
}

bool form_can_use_wand(transformation_type form)
{
    return form_can_wield(form) || form == TRAN_DRAGON;
}

// Used to mark forms which keep most form-based mutations.
bool form_keeps_mutations(transformation_type form)
{
    return get_form(form)->keeps_mutations;
}

static set<equipment_type>
_init_equipment_removal(transformation_type form)
{
    set<equipment_type> result;
    if (!form_can_wield(form) && you.weapon() || you.melded[EQ_WEAPON])
        result.insert(EQ_WEAPON);

    // Liches can't wield holy weapons.
    if (form == TRAN_LICH && you.weapon() && is_holy_item(*you.weapon()))
        result.insert(EQ_WEAPON);

    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; ++i)
    {
        const equipment_type eq = static_cast<equipment_type>(i);
        const item_def *pitem = you.slot_item(eq, true);

        if (pitem && (get_form(form)->blocked_slots & SLOTF(i)
                      || !get_form(form)->can_wear_item(*pitem)))
        {
            result.insert(eq);
        }
    }
    return result;
}

static void _remove_equipment(const set<equipment_type>& removed,
                              bool meld = true, bool mutation = false)
{
    // Meld items into you in (reverse) order. (set is a sorted container)
    set<equipment_type>::const_iterator iter;
    for (iter = removed.begin(); iter != removed.end(); ++iter)
    {
        const equipment_type e = *iter;
        item_def *equip = you.slot_item(e, true);
        if (equip == NULL)
            continue;

        bool unequip = !meld;
        if (!unequip && e == EQ_WEAPON)
        {
            if (form_can_wield(you.form))
                unequip = true;
            if (!is_weapon(*equip))
                unequip = true;
        }

        mprf("%s %s%s %s", equip->name(DESC_YOUR).c_str(),
             unequip ? "fall" : "meld",
             equip->quantity > 1 ? "" : "s",
             unequip ? "away!" : "into your body.");

        if (unequip)
        {
            if (e == EQ_WEAPON)
            {
                unwield_item(!you.berserk());
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
        for (iter = removed.begin(); iter != removed.end(); ++iter)
            if (you.slot_item(*iter, true) != NULL)
                unequip_effect(*iter, you.equip[*iter], true, true);
}

// FIXME: merge this with you_can_wear(), can_wear_armour(), etc.
static bool _mutations_prevent_wearing(const item_def& item)
{
    const equipment_type eqslot = get_armour_slot(item);

    if (is_hard_helmet(item)
        && (player_mutation_level(MUT_HORNS)
            || player_mutation_level(MUT_ANTENNAE)
            || player_mutation_level(MUT_BEAK)))
    {
        return true;
    }

    // Barding is excepted here.
    if (item.sub_type == ARM_BOOTS
        && (player_mutation_level(MUT_HOOVES) >= 3
            || player_mutation_level(MUT_TALONS) >= 3))
    {
        return true;
    }

    if (eqslot == EQ_GLOVES && player_mutation_level(MUT_CLAWS) >= 3)
        return true;

    if (eqslot == EQ_HELMET
        && (player_mutation_level(MUT_HORNS) == 3
            || player_mutation_level(MUT_ANTENNAE) == 3))
    {
        return true;
    }

    return false;
}

static void _unmeld_equipment_type(equipment_type e)
{
    item_def& item = you.inv[you.equip[e]];
    bool force_remove = false;

    if (e == EQ_WEAPON)
    {
        if (you.slot_item(EQ_SHIELD)
            && is_shield_incompatible(item, you.slot_item(EQ_SHIELD)))
        {
            force_remove = true;
        }
    }
    else if (item.base_type != OBJ_JEWELLERY)
    {
        // In case the player was mutated during the transformation,
        // check whether the equipment is still wearable.
        if (_mutations_prevent_wearing(item))
            force_remove = true;

        // If you switched weapons during the transformation, make
        // sure you can still wear your shield.
        // (This is only possible with Statue Form.)
        if (e == EQ_SHIELD && you.weapon()
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
    set<equipment_type>::const_iterator iter;
    for (iter = melded.begin(); iter != melded.end(); ++iter)
    {
        const equipment_type e = *iter;
        if (you.equip[e] == -1)
            continue;

        _unmeld_equipment_type(e);
    }

    for (iter = melded.begin(); iter != melded.end(); ++iter)
        if (you.equip[*iter] != -1)
            equip_effect(*iter, you.equip[*iter], true, true);
}

void unmeld_one_equip(equipment_type eq)
{
    if (eq >= EQ_HELMET && eq <= EQ_BOOTS)
    {
        const item_def* arm = you.slot_item(EQ_BODY_ARMOUR, true);
        if (arm && is_unrandom_artefact(*arm, UNRAND_LEAR))
            eq = EQ_BODY_ARMOUR;
    }

    set<equipment_type> e;
    e.insert(eq);
    _unmeld_equipment(e);
}

void remove_one_equip(equipment_type eq, bool meld, bool mutation)
{
    if (player_equip_unrand(UNRAND_LEAR) && eq >= EQ_HELMET && eq <= EQ_BOOTS)
        eq = EQ_BODY_ARMOUR;

    set<equipment_type> r;
    r.insert(eq);
    _remove_equipment(r, meld, mutation);
}

/**
 * What size is the player, when in the given form?
 *
 * @param tform     The type of transformation in question.
 * @return          The size of the player when in the given form; may be
 *                  SIZE_CHARACTER (unchanged).
 */
size_type player::transform_size(transformation_type tform) const
{
    return get_form()->size;
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

static bool _abort_or_fizzle(bool just_check)
{
    if (!just_check && you.turn_is_over)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        move_player_to_grid(you.pos(), false);
        return true; // pay the necessary costs
    }
    return false; // SPRET_ABORT
}

string blade_parts(bool terse)
{
    string str;

    if (you.species == SP_FELID)
        str = terse ? "paw" : "front paw";
    else if (you.species == SP_OCTOPODE)
        str = "tentacle";
    else
        str = "hand";

    if (!player_mutation_level(MUT_MISSING_HAND))
        str = pluralise(str);

    return str;
}

monster_type dragon_form_dragon_type()
{
    switch (you.species)
    {
    case SP_WHITE_DRACONIAN:
        return MONS_ICE_DRAGON;
    case SP_GREEN_DRACONIAN:
        return MONS_SWAMP_DRAGON;
    case SP_YELLOW_DRACONIAN:
        return MONS_GOLDEN_DRAGON;
    case SP_GREY_DRACONIAN:
        return MONS_IRON_DRAGON;
    case SP_BLACK_DRACONIAN:
        return MONS_STORM_DRAGON;
    case SP_PURPLE_DRACONIAN:
        return MONS_QUICKSILVER_DRAGON;
    case SP_MOTTLED_DRACONIAN:
        return MONS_MOTTLED_DRAGON;
    case SP_PALE_DRACONIAN:
        return MONS_STEAM_DRAGON;
    case SP_RED_DRACONIAN:
    default:
        return MONS_FIRE_DRAGON;
    }
}

// with a denominator of 10
int form_hp_mod()
{
    return get_form()->hp_mod;
}

static bool _flying_in_new_form(transformation_type which_trans)
{
    if (get_form(which_trans)->forbids_flight())
        return false;

    // If our flight is uncancellable (or tenguish) then it's not from evoking
    if (you.attribute[ATTR_FLIGHT_UNCANCELLABLE]
        || you.permanent_flight() && you.racial_permanent_flight())
    {
        return true;
    }

    if (!you.duration[DUR_FLIGHT] && !you.attribute[ATTR_PERM_FLIGHT])
        return false;

    int sources = you.evokable_flight();
    int sources_removed = 0;
    set<equipment_type> removed = _init_equipment_removal(which_trans);
    for (set<equipment_type>::iterator iter = removed.begin();
         iter != removed.end(); ++iter)
    {
        item_def *item = you.slot_item(*iter, true);
        if (item == NULL)
            continue;
        item_info inf = get_item_info(*item);

        //similar code to safe_to_remove from item_use.cc
        if (inf.base_type == OBJ_JEWELLERY && inf.sub_type == RING_FLIGHT)
            sources_removed++;
        if (inf.base_type == OBJ_ARMOUR && inf.special == SPARM_FLYING)
            sources_removed++;
        if (is_artefact(inf) && artefact_known_wpn_property(inf, ARTP_FLY))
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
bool feat_dangerous_for_form(transformation_type which_trans,
                             dungeon_feature_type feat)
{
    // Everything is okay if we can fly.
    if (form_can_fly(which_trans) || _flying_in_new_form(which_trans))
        return false;

    if (feat == DNGN_LAVA)
        return !form_likes_lava(which_trans);

    if (feat == DNGN_DEEP_WATER)
    {
        if (beogh_water_walk())
            return false;

        return !form_likes_water(which_trans);
    }

    return false;
}

static mutation_type appendages[] =
{
    MUT_HORNS,
    MUT_TENTACLE_SPIKE,
    MUT_TALONS,
};

static bool _slot_conflict(equipment_type eq)
{
    // Choose uncovered slots only.  Melding could make people re-cast
    // until they get something that doesn't conflict with their randart
    // of überness.
    if (you.equip[eq] != -1)
    {
        // Horns + hat is fine.
        if (eq != EQ_HELMET
            || you.melded[eq]
            || is_hard_helmet(*(you.slot_item(eq))))
        {
            return true;
        }
    }

    for (int mut = 0; mut < NUM_MUTATIONS; mut++)
        if (you.mutation[mut] && eq == beastly_slot(mut))
            return true;

    return false;
}

static mutation_type _beastly_appendage()
{
    mutation_type chosen = NUM_MUTATIONS;
    int count = 0;

    for (unsigned int i = 0; i < ARRAYSZ(appendages); i++)
    {
        mutation_type app = appendages[i];

        if (_slot_conflict(beastly_slot(app)))
            continue;
        if (physiology_mutation_conflict(app))
            continue;

        if (one_chance_in(++count))
            chosen = app;
    }
    return chosen;
}

static bool _transformation_is_safe(transformation_type which_trans,
                                    dungeon_feature_type feat, bool quiet)
{
    if (which_trans == TRAN_TREE)
    {
        const cloud_type cloud = cloud_type_at(you.pos());
        if (cloud != CLOUD_NONE
            // Tree form is immune to these two.
            && cloud != CLOUD_MEPHITIC
            && cloud != CLOUD_POISON
            && is_damaging_cloud(cloud, false))
        {
            if (!quiet)
            {
                mprf("You can't transform into a tree while standing in a cloud of %s.",
                     cloud_type_name(cloud).c_str());
            }
            return false;
        }
    }
#if TAG_MAJOR_VERSION == 34

    if (which_trans == TRAN_ICE_BEAST && you.species == SP_DJINNI)
        return false; // melting is fatal...
#endif

    if (!feat_dangerous_for_form(which_trans, feat))
        return true;

    if (!quiet)
    {
        mprf("You would %s in your new form.",
             feat == DNGN_DEEP_WATER ? "drown" : "burn");
    }
    return false;
}

/**
 * If we transform into the given form, will all of our stats remain above 0,
 * based purely on the stat modifiers of the current & destination form?
 *
 * May prompt the player.
 *
 * @param new_form  The form to check the safety of.
 * @return          Whether it's okay to go ahead with the transformation.
 */
bool check_form_stat_safety(transformation_type new_form)
{
    const int str_mod = get_form(new_form)->str_mod - get_form()->str_mod;
    const int dex_mod = get_form(new_form)->dex_mod - get_form()->dex_mod;

    const bool bad_str = you.strength() > 0 && str_mod + you.strength() <= 0;
    const bool bad_dex = you.dex() > 0 && dex_mod + you.dex() <= 0;
    if (!bad_str && !bad_dex)
        return true;

    string prompt = make_stringf("%s will reduce your %s to zero.  Continue?",
                                 new_form == TRAN_NONE ? "Turning back"
                                                       : "Transforming",
                                 bad_str ? "strength" : "dexterity");
    if (yesno(prompt.c_str(), false, 'n'))
        return true;

    canned_msg(MSG_OK);
    return false;
}

static int _transform_duration(transformation_type which_trans, int pow)
{
    return get_form(which_trans)->get_duration(pow);
}

static int _beastly_appendage_level(int appendage)
{
    switch (appendage)
    {
    case MUT_HORNS: return 2;
    default:        return 3;
    }
}

// Transforms you into the specified form. If involuntary, checks for
// inscription warnings are skipped, and the transformation fails silently
// (if it fails). If just_check is true the transformation doesn't actually
// happen, but the method returns whether it would be successful.
bool transform(int pow, transformation_type which_trans, bool involuntary,
               bool just_check)
{
    const transformation_type previous_trans = you.form;
    const flight_type was_flying = you.flight_mode();

    // Zin's protection.
    if (!just_check && you_worship(GOD_ZIN)
        && x_chance_in_y(you.piety, MAX_PIETY) && which_trans != TRAN_NONE)
    {
        simple_god_message(" protects your body from unnatural transformation!");
        return false;
    }

    if (!involuntary && crawl_state.is_god_acting())
        involuntary = true;

    if (you.transform_uncancellable)
    {
        if (!involuntary)
            mpr("You are stuck in your current form!");
        return false;
    }

    if (!_transformation_is_safe(which_trans, env.grid(you.pos()),
        involuntary))
    {
        return false;
    }

    // This must occur before the untransform() and the undead_state() check.
    if (previous_trans == which_trans)
    {
        // update power
        if (!just_check && which_trans != TRAN_NONE)
        {
            you.props[TRANSFORM_POW_KEY] = pow;
            you.redraw_armour_class = true;
            // ^ could check more carefully for the exact cases, but I'm
            // worried about making the code too fragile
        }

        int dur = _transform_duration(which_trans, pow);
        if (you.duration[DUR_TRANSFORMATION] < dur * BASELINE_DELAY)
        {
            if (just_check)
                return true;

            mpr("You extend your transformation's duration.");
            you.duration[DUR_TRANSFORMATION] = dur * BASELINE_DELAY;

            return true;
        }
        else
        {
            if (!involuntary && which_trans != TRAN_NONE)
                mpr("You fail to extend your transformation any further.");
            return false;
        }
    }

    // The actual transformation may still fail later (e.g. due to cursed
    // equipment). Ideally, untransforming should cost a turn but nothing
    // else (as does the "End Transformation" ability). As it is, you
    // pay with mana and hunger if you already untransformed.
    if (!just_check && previous_trans != TRAN_NONE)
        untransform(true);

    // Catch some conditions which prevent transformation.
    if (you.undead_state()
        && which_trans != TRAN_SHADOW
        && (you.species != SP_VAMPIRE
            || which_trans != TRAN_BAT && you.hunger_state <= HS_SATIATED
            || which_trans == TRAN_LICH))
    {
        if (!involuntary)
            mpr("Your unliving flesh cannot be transformed in this way.");
        return _abort_or_fizzle(just_check);
    }

    if (which_trans == TRAN_LICH && you.duration[DUR_DEATHS_DOOR])
    {
        if (!involuntary)
        {
            mpr("The transformation conflicts with an enchantment "
                "already in effect.");
        }
        return _abort_or_fizzle(just_check);
    }

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC && !temperature_effect(LORC_STONESKIN)
        && (which_trans == TRAN_ICE_BEAST || which_trans == TRAN_STATUE))
    {
        if (!involuntary)
            mpr("Your temperature is too high to benefit from that spell.");
        return _abort_or_fizzle(just_check);
    }
#endif

    set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);

    if (which_trans == TRAN_APPENDAGE)
    {
        const mutation_type app = _beastly_appendage();
        if (app == NUM_MUTATIONS)
        {
            if (!involuntary)
                mpr("You have no appropriate body parts free.");
            return false;
        }

        if (!just_check)
        {
            you.attribute[ATTR_APPENDAGE] = app; // need to set it here so
                                                 // the message correlates
        }
    }

    if (!involuntary && just_check && !check_form_stat_safety(which_trans))
        return false;

    // If we're just pretending return now.
    if (just_check)
        return true;

    // Switching between forms takes a bit longer.
    if (!involuntary && previous_trans != TRAN_NONE
        && previous_trans != which_trans)
    {
        you.time_taken = div_rand_round(you.time_taken * 3, 2);
    }

    // All checks done, transformation will take place now.
    you.redraw_quiver       = true;
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;
    if (form_changed_physiology(which_trans))
        merfolk_stop_swimming();

    // Most transformations conflict with stone skin.
    if (form_changed_physiology(which_trans) && which_trans != TRAN_STATUE)
        you.duration[DUR_STONESKIN] = 0;

    if (which_trans == TRAN_HYDRA)
        set_hydra_form_heads(div_rand_round(pow, 10));

    // Give the transformation message.
    mpr(get_form(which_trans)->transform_message(previous_trans).c_str());

    // Update your status.
    you.form = which_trans;
    you.set_duration(DUR_TRANSFORMATION, _transform_duration(which_trans, pow));
    update_player_symbol();

    _remove_equipment(rem_stuff);

    you.props[TRANSFORM_POW_KEY] = pow;

    const int str_mod = get_form(which_trans)->str_mod;
    const int dex_mod = get_form(which_trans)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, dex_mod, true);

    _extra_hp(form_hp_mod());

    if (you.digging && which_trans == TRAN_TREE)
    {
        mpr("Your mandibles meld away.");
        you.digging = false;
    }

    // Extra effects
    switch (which_trans)
    {
    case TRAN_STATUE:
        if (you.duration[DUR_STONESKIN])
            mpr("Your new body merges with your stone armour.");
#if TAG_MAJOR_VERSION == 34
        else if (you.species == SP_LAVA_ORC)
            mpr("Your new body is particularly stony.");
#endif
        if (you.duration[DUR_ICY_ARMOUR])
        {
            mprf(MSGCH_DURATION, "Your new body cracks your icy armour.");
            you.duration[DUR_ICY_ARMOUR] = 0;
        }
        break;

    case TRAN_ICE_BEAST:
        if (you.duration[DUR_ICY_ARMOUR])
            mpr("Your new body merges with your icy armour.");
        break;

    case TRAN_SPIDER:
        if (you.attribute[ATTR_HELD])
        {
            trap_def *trap = find_trap(you.pos());
            if (trap && trap->type == TRAP_WEB)
            {
                mpr("You disentangle yourself from the web.");
                you.attribute[ATTR_HELD] = 0;
            }
        }
        break;

    case TRAN_TREE:
        mpr("Your roots penetrate the ground.");
        if (you.duration[DUR_TELEPORT])
        {
            you.duration[DUR_TELEPORT] = 0;
            mpr("You feel strangely stable.");
        }
        you.duration[DUR_FLIGHT] = 0;
        // break out of webs/nets as well

    case TRAN_DRAGON:
        if (you.attribute[ATTR_HELD])
        {
            trap_def *trap = find_trap(you.pos());
            if (trap && trap->type == TRAP_WEB)
            {
                mpr("You shred the web into pieces!");
                destroy_trap(you.pos());
            }
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
            {
                mpr("The net rips apart!");
                destroy_item(net);
            }

            you.attribute[ATTR_HELD] = 0;
        }
        break;

    case TRAN_LICH:
        // undead cannot regenerate -- bwr
        if (you.duration[DUR_REGENERATION])
        {
            mprf(MSGCH_DURATION, "You stop regenerating.");
            you.duration[DUR_REGENERATION] = 0;
        }

        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status(REDRAW_HUNGER);
        break;

    case TRAN_APPENDAGE:
        {
            int app = you.attribute[ATTR_APPENDAGE];
            ASSERT(app != NUM_MUTATIONS);
            ASSERT(beastly_slot(app) != EQ_NONE);
            you.mutation[app] = _beastly_appendage_level(app);
        }
        break;

    case TRAN_SHADOW:
        drain_player(25, true, true);
        if (you.invisible())
            mpr("You fade into the shadows.");
        else
            mpr("You feel less conspicuous.");
        break;

    default:
        break;
    }

    // Stop constricting if we can no longer constrict.  If any size-changing
    // transformations were to allow constriction, we would have to check
    // relative sizes as well.  Likewise, if any transformations were to allow
    // normally non-constricting players to constrict, this would need to
    // be changed.
    if (!form_keeps_mutations(which_trans))
        you.stop_constricting_all(false);

    // Stop being constricted if we are now too large.
    if (you.is_constricted())
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        ASSERT(constrictor);

        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }


    // If we are no longer living, end an effect that afflicts only the living
    if (you.duration[DUR_FLAYED] && you.holiness() != MH_NATURAL)
    {
        // Heal a little extra if we gained max hp from this transformation
        if (form_hp_mod() != 10)
        {
            int dam = you.props["flay_damage"].get_int();
            you.heal((dam * form_hp_mod() / 10) - dam);
        }
        heal_flayed_effect(&you);
    }

    // This only has an effect if the transformation happens passively,
    // for example if Xom decides to transform you while you're busy
    // running around or butchering corpses.
    // If you're turned into a tree, you stop taking stairs.
    stop_delay(which_trans == TRAN_TREE);

    if (crawl_state.which_god_acting() == GOD_XOM)
       you.transform_uncancellable = true;

    // Re-check terrain now that be may no longer be swimming or flying.
    if (was_flying && !you.flight_mode()
                   || feat_is_water(grd(you.pos()))
                      && (which_trans == TRAN_BLADE_HANDS
                          || which_trans == TRAN_APPENDAGE)
                      && you.species == SP_MERFOLK)
    {
        move_player_to_grid(you.pos(), false);
    }

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("gaining the %s transformation",
                          transform_name(which_trans)).c_str());
    }

    return true;
}

/**
 * End the player's transformation and return them to their normal
 * form.
 * @param skip_move      If true, skip any move that was in progress before
 *                       the transformation ended.
 */
void untransform(bool skip_move)
{
    const flight_type old_flight = you.flight_mode();

    you.redraw_quiver       = true;
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;
    if (you.props.exists(TRANSFORM_POW_KEY))
        you.props.erase(TRANSFORM_POW_KEY);
    if (you.props.exists(HYDRA_FORM_HEADS_KEY))
        you.props.erase(HYDRA_FORM_HEADS_KEY);

    // Must be unset first or else infinite loops might result. -- bwr
    const transformation_type old_form = you.form;
    int hp_downscale = form_hp_mod();

    // We may have to unmeld a couple of equipment types.
    set<equipment_type> melded = _init_equipment_removal(old_form);

    you.form = TRAN_NONE;
    you.duration[DUR_TRANSFORMATION]   = 0;
    update_player_symbol();

    switch (old_form)
    {
    case TRAN_STATUE:
        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_STONESKIN])
            you.duration[DUR_STONESKIN] = 1;
        break;

    case TRAN_ICE_BEAST:
        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_ICY_ARMOUR])
            you.duration[DUR_ICY_ARMOUR] = 1;
        break;

    case TRAN_APPENDAGE:
        {
            int app = you.attribute[ATTR_APPENDAGE];
            ASSERT(beastly_slot(app) != EQ_NONE);
            const int levels = you.mutation[app];
            // Preserve extra mutation levels acquired after transforming.
            const int beast_levels = _beastly_appendage_level(app);
            const int extra = max(0, levels - you.innate_mutation[app]
                                            - beast_levels);
            you.mutation[app] = you.innate_mutation[app] + extra;
            you.attribute[ATTR_APPENDAGE] = 0;

            // The mutation might have been removed already by a conflicting
            // demonspawn innate mutation; no message then.
            if (levels)
            {
                const char * const verb = you.mutation[app] ? "shrink"
                                                            : "disappear";
                mprf(MSGCH_DURATION, "Your %s %s%s.",
                     mutation_name(static_cast<mutation_type>(app)), verb,
                     app == MUT_TENTACLE_SPIKE ? "s" : "");
            }
        }
        break;

    default:
        break;
    }

    const string message = get_form(old_form)->get_untransform_message();
    if (message != "")
        mprf(MSGCH_DURATION, "%s", message.c_str());

    const int str_mod = get_form(old_form)->str_mod;
    const int dex_mod = get_form(old_form)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, -str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, -dex_mod, true);

    _unmeld_equipment(melded);

    // Re-check terrain now that be may no longer be swimming or flying.
    if (!skip_move && (old_flight && !you.flight_mode()
                       || (feat_is_water(grd(you.pos()))
                           && (old_form == TRAN_ICE_BEAST
                               || you.species == SP_MERFOLK))))
    {
        move_player_to_grid(you.pos(), false);
    }

#ifdef USE_TILE
    if (you.species == SP_MERFOLK)
        init_player_doll();
#endif

    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr

    // Removed barding check, no transformed creatures can wear barding
    // anyway.
    // *coughs* Ahem, blade hands... -- jpeg
    if (you.species == SP_NAGA || you.species == SP_CENTAUR)
    {
        const int arm = you.equip[EQ_BOOTS];

        if (arm != -1 && you.inv[arm].sub_type == ARM_BOOTS)
            remove_one_equip(EQ_BOOTS);
    }

    // End Ozocubu's Icy Armour if you unmelded wearing heavy armour
    if (you.duration[DUR_ICY_ARMOUR]
        && !player_effectively_in_light_armour())
    {
        you.duration[DUR_ICY_ARMOUR] = 0;

        const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
        mprf(MSGCH_DURATION, "%s cracks your icy armour.",
             armour->name(DESC_YOUR).c_str());
    }

    if (hp_downscale != 10 && you.hp != you.hp_max)
    {
        int hp = you.hp * 10 / hp_downscale;
        if (hp < 1)
            hp = 1;
        else if (hp > you.hp_max)
            hp = you.hp_max;
        set_hp(hp);
    }
    calc_hp();

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("losing the %s form",
                          transform_name(old_form)).c_str());
    }

    // Stop being constricted if we are now too large.
    if (you.is_constricted())
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }

    you.turn_is_over = true;
    if (you.transform_uncancellable)
        you.transform_uncancellable = false;
}

static void _extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
}

void emergency_untransform()
{
    mpr("You quickly transform back into your natural form.");
    untransform(true); // We're already entering the water.

    if (you.species == SP_MERFOLK)
        merfolk_start_swimming(false);
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
    remove_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

#ifdef USE_TILE
    init_player_doll();
#endif
}

void merfolk_stop_swimming()
{
    if (!you.fishtail)
        return;
    you.fishtail = false;
    unmeld_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

#ifdef USE_TILE
    init_player_doll();
#endif
}

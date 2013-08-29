/**
 * @file
 * @brief Misc function related to player transformations.
**/

#include "AppHdr.h"

#include "transform.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "art-enum.h"
#include "artefact.h"
#include "cloud.h"
#include "delay.h"
#include "env.h"
#include "godabil.h"
#include "goditem.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "misc.h"
#include "mon-abil.h"
#include "mutation.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

static void _extra_hp(int amount_extra);

static const char* form_names[] =
{
    "none",
    "spider",
    "blade",
    "statue",
    "ice",
    "dragon",
    "lich",
    "bat",
    "pig",
    "appendage",
    "tree",
    "porcupine",
    "wisp",
    "jelly",
    "fungus",
};

const char* transform_name(transformation_type form)
{
    COMPILE_CHECK(ARRAYSZ(form_names) == LAST_FORM + 1);
    ASSERT_RANGE(form, 0, LAST_FORM + 1);
    return form_names[form];
}

bool form_can_wield(transformation_type form)
{
    return (form == TRAN_NONE || form == TRAN_STATUE || form == TRAN_LICH
            || form == TRAN_APPENDAGE || form == TRAN_TREE);
}

bool form_can_wear(transformation_type form)
{
    return form_can_wield(form) || form == TRAN_BLADE_HANDS;
}

bool form_can_fly(transformation_type form)
{
    if (you.racial_permanent_flight() && you.permanent_flight())
        return true;
    return (form == TRAN_DRAGON || form == TRAN_BAT || form == TRAN_WISP);
}

bool form_can_swim(transformation_type form)
{
    // Ice floats, scum goes to the top.
    if (form == TRAN_ICE_BEAST || form == TRAN_JELLY)
        return true;

    if (you.species == SP_MERFOLK && !form_changed_physiology(form))
        return true;

    if (you.species == SP_OCTOPODE && !form_changed_physiology(form))
        return true;

    size_type size = you.transform_size(form, PSIZE_BODY);
    if (size == SIZE_CHARACTER)
        size = you.body_size(PSIZE_BODY, true);

    return (size >= SIZE_GIANT);
}

bool form_likes_water(transformation_type form)
{
    return (form_can_swim(form) || you.species == SP_GREY_DRACONIAN
                                   && !form_changed_physiology(form));
}

bool form_has_mouth(transformation_type form)
{
    return form != TRAN_TREE
        && form != TRAN_WISP
        && form != TRAN_JELLY
        && form != TRAN_FUNGUS;
}

bool form_likes_lava(transformation_type form)
{
    // Lava orcs can only swim in non-phys-change forms.
    return (you.species == SP_LAVA_ORC
            && !form_changed_physiology(form));
}

bool form_can_butcher_barehanded(transformation_type form)
{
    return (form == TRAN_BLADE_HANDS || form == TRAN_DRAGON
            || form == TRAN_ICE_BEAST);
}

// Used to mark transformations which override species intrinsics.
bool form_changed_physiology(transformation_type form)
{
    return (form != TRAN_NONE && form != TRAN_APPENDAGE
            && form != TRAN_BLADE_HANDS);
}

bool form_can_use_wand(transformation_type form)
{
    return (form_can_wield(form) || form == TRAN_DRAGON);
}

bool form_can_wear_item(const item_def& item, transformation_type form)
{
    if (form == TRAN_JELLY || form == TRAN_PORCUPINE || form == TRAN_WISP)
        return false;

    if (item.base_type == OBJ_JEWELLERY)
    {
        // Everyone but jellies, porcupines and wisps can wear amulets.
        if (jewellery_is_amulet(item))
            return true;
        // Bats and pigs can't wear rings.
        return (form != TRAN_BAT && form != TRAN_PIG);
    }

    // It's not jewellery, and it's worn, so it must be armour.
    const equipment_type eqslot = get_armour_slot(item);

    switch (form)
    {
    // Some forms can wear everything.
    case TRAN_NONE:
    case TRAN_LICH:
    case TRAN_APPENDAGE: // handled as mutations
        return true;

    // Some can't wear anything.
    case TRAN_DRAGON:
    case TRAN_BAT:
    case TRAN_PIG:
    case TRAN_SPIDER:
    case TRAN_ICE_BEAST:
        return false;

    // And some need more complicated logic.
    case TRAN_BLADE_HANDS:
        return (eqslot != EQ_SHIELD && eqslot != EQ_GLOVES);

    case TRAN_STATUE:
        return (eqslot == EQ_CLOAK || eqslot == EQ_HELMET
                || eqslot == EQ_SHIELD);

    case TRAN_FUNGUS:
        return (eqslot == EQ_HELMET && !is_hard_helmet(item));

    case TRAN_TREE:
        return (eqslot == EQ_SHIELD || eqslot == EQ_HELMET);

    default:                // Bug-catcher.
        die("Unknown transformation type %d in form_can_wear_item", you.form);
    }
}

// Used to mark forms which keep most form-based mutations.
bool form_keeps_mutations(transformation_type form)
{
    switch (form)
    {
    case TRAN_NONE:
    case TRAN_BLADE_HANDS:
    case TRAN_STATUE:
    case TRAN_LICH:
    case TRAN_APPENDAGE:
        return true;
    default:
        return false;
    }
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

        if (!pitem)
            continue;

        // Octopodes lose their extra ring slots (3--8) in forms that do not
        // have eight limbs.  Handled specially here because we do have to
        // distinguish between slots the same type.
        if (i >= EQ_RING_THREE && i <= EQ_RING_EIGHT
            && !(form_keeps_mutations(form) || form == TRAN_SPIDER))
        {
            result.insert(eq);
        }
        else if (!form_can_wear_item(*pitem, form))
            result.insert(eq);
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
            if (you.form == TRAN_NONE || form_can_wield(you.form))
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
                const int slot = you.equip[EQ_WEAPON];
                unwield_item(!you.berserk());
                canned_msg(MSG_EMPTY_HANDED_NOW);
                you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = slot + 1;
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

    if (eqslot == EQ_HELMET && (player_mutation_level(MUT_HORNS) == 3
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
        // if (item.base_type != OBJ_JEWELLERY)
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
}

void unmeld_one_equip(equipment_type eq)
{
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

size_type player::transform_size(transformation_type tform, int psize) const
{
    switch (tform)
    {
    case TRAN_SPIDER:
    case TRAN_BAT:
    case TRAN_PORCUPINE:
    case TRAN_WISP:
    case TRAN_FUNGUS:
        return SIZE_TINY;
    case TRAN_PIG:
    case TRAN_JELLY:
        return SIZE_SMALL;
    case TRAN_ICE_BEAST:
        return SIZE_LARGE;
    case TRAN_DRAGON:
    case TRAN_TREE:
        return SIZE_HUGE;
    default:
        return SIZE_CHARACTER;
    }
}

static bool _abort_or_fizzle(bool just_check)
{
    if (!just_check && you.turn_is_over)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        move_player_to_grid(you.pos(), false, true);
        return true; // pay the necessary costs
    }
    return false; // SPRET_ABORT
}

monster_type transform_mons()
{
    switch (you.form)
    {
    case TRAN_FUNGUS:
        return MONS_WANDERING_MUSHROOM;
    case TRAN_SPIDER:
        return MONS_SPIDER;
    case TRAN_STATUE:
        return MONS_STATUE;
    case TRAN_ICE_BEAST:
        return MONS_ICE_BEAST;
    case TRAN_DRAGON:
        return dragon_form_dragon_type();
    case TRAN_LICH:
        return MONS_LICH;
    case TRAN_BAT:
        return you.species == SP_VAMPIRE ? MONS_VAMPIRE_BAT : MONS_BAT;
    case TRAN_PIG:
        return MONS_HOG;
    case TRAN_JELLY:
        return MONS_JELLY;
    case TRAN_PORCUPINE:
        return MONS_PORCUPINE;
    case TRAN_TREE:
        return MONS_ANIMATED_TREE;
    case TRAN_WISP:
        return MONS_INSUBSTANTIAL_WISP;
    case TRAN_BLADE_HANDS:
    case TRAN_APPENDAGE:
    case TRAN_NONE:
        return MONS_PLAYER;
    }
    die("unknown transformation");
    return MONS_PLAYER;
}

string blade_parts(bool terse)
{
    if (you.species == SP_FELID)
        return terse ? "paws" : "front paws";
    if (you.species == SP_OCTOPODE)
        return "tentacles";
    return "hands";
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
        return MONS_DRAGON;
    }
}

// with a denominator of 10
int form_hp_mod()
{
    switch (you.form)
    {
    case TRAN_STATUE:
        return 13;
    case TRAN_ICE_BEAST:
        return 12;
    case TRAN_JELLY:
    case TRAN_DRAGON:
    case TRAN_TREE:
        return 15;
    default:
        return 10;
    }
}

static bool _flying_in_new_form(transformation_type which_trans)
{
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

    return (sources > sources_removed);
}

bool feat_dangerous_for_form(transformation_type which_trans,
                             dungeon_feature_type feat)
{
    // Everything is okay if we can fly.
    if (form_can_fly(which_trans) || _flying_in_new_form(which_trans))
        return false;

    // ... or hover our butts up if need arises.
    if (you.species == SP_DJINNI)
        return false;

    // We can only cling for safety if we're already doing so.
    if (which_trans == TRAN_SPIDER && you.is_wall_clinging())
        return false;

    if (feat == DNGN_LAVA)
        return !form_likes_lava(which_trans);

    if (feat == DNGN_DEEP_WATER)
        return (!form_likes_water(which_trans) && !beogh_water_walk());

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

const char* appendage_name(int app)
{
    ASSERT(beastly_slot(app) != EQ_NONE);
    const mutation_def& mdef = get_mutation_def((mutation_type) app);
    return mdef.short_desc;
}

static bool _transformation_is_safe(transformation_type which_trans,
                                    dungeon_feature_type feat, bool quiet)
{
    if (which_trans == TRAN_TREE)
    {
        const int cloud = env.cgrid(you.pos());
        if (cloud != EMPTY_CLOUD && is_damaging_cloud(env.cloud[cloud].type, false))
            return false;
    }

    if (which_trans == TRAN_ICE_BEAST && you.species == SP_DJINNI)
        return false; // melting is fatal...

    if (!feat_dangerous_for_form(which_trans, feat))
        return true;

    if (!quiet)
    {
        mprf("You would %s in your new form.",
             feat == DNGN_DEEP_WATER ? "drown" : "burn");
    }
    return false;
}

static int _transform_duration(transformation_type which_trans, int pow)
{
    switch (which_trans)
    {
    case TRAN_BLADE_HANDS:
        return min(10 + random2(pow), 100);
    case TRAN_APPENDAGE:
    case TRAN_SPIDER:
        return min(10 + random2(pow) + random2(pow), 60);
    case TRAN_STATUE:
    case TRAN_DRAGON:
    case TRAN_LICH:
    case TRAN_BAT:
        return min(20 + random2(pow) + random2(pow), 100);
    case TRAN_ICE_BEAST:
        return min(30 + random2(pow) + random2(pow), 100);
    case TRAN_FUNGUS:
    case TRAN_PIG:
    case TRAN_PORCUPINE:
    case TRAN_JELLY:
    case TRAN_TREE:
    case TRAN_WISP:
        return min(15 + random2(pow) + random2(pow / 2), 100);
    case TRAN_NONE:
        return 0;
    default:
        die("unknown transformation: %d", which_trans);
    }
}

// Transforms you into the specified form. If involuntary, checks for
// inscription warnings are skipped, and the transformation fails silently
// (if it fails). If just_check is true the transformation doesn't actually
// happen, but the method returns whether it would be successful.
bool transform(int pow, transformation_type which_trans, bool involuntary,
               bool just_check)
{
    transformation_type previous_trans = you.form;
    bool was_in_water = you.in_water();
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
        // Jiyva's wrath-induced transformation is blocking the attempt.
        // May need to be updated if transform_uncancellable is used for
        // other uses.
        if (!just_check)
            mpr("You are stuck in your current form!");
        return false;
    }

    if (!_transformation_is_safe(which_trans, env.grid(you.pos()),
        involuntary || just_check))
    {
        return false;
    }

    // This must occur before the untransform() and the is_undead check.
    if (previous_trans == which_trans)
    {
        int dur = _transform_duration(which_trans, pow);
        if (you.duration[DUR_TRANSFORMATION] < dur * BASELINE_DELAY)
        {
            if (just_check)
                return true;

            if (which_trans == TRAN_PIG)
                mpr("You feel you'll be a pig longer.");
            else
                mpr("You extend your transformation's duration.");
            you.duration[DUR_TRANSFORMATION] = dur * BASELINE_DELAY;

            return true;
        }
        else
        {
            if (!involuntary && which_trans != TRAN_PIG && which_trans != TRAN_NONE)
                mpr("You fail to extend your transformation any further.");
            return false;
        }
    }

    // The actual transformation may still fail later (e.g. due to cursed
    // equipment). Ideally, untransforming should cost a turn but nothing
    // else (as does the "End Transformation" ability). As it is, you
    // pay with mana and hunger if you already untransformed.
    if (!just_check && previous_trans != TRAN_NONE)
    {
        bool skip_wielding = false;
        switch (which_trans)
        {
        case TRAN_STATUE:
        case TRAN_LICH:
            break;
        default:
            skip_wielding = true;
            break;
        }
        // Skip wielding weapon if it gets unwielded again right away.
        untransform(skip_wielding, true);
    }

    // Catch some conditions which prevent transformation.
    if (you.is_undead
        && (you.species != SP_VAMPIRE
            || which_trans != TRAN_BAT && you.hunger_state <= HS_SATIATED))
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

    if (you.species == SP_LAVA_ORC && !temperature_effect(LORC_STONESKIN)
        && (which_trans == TRAN_ICE_BEAST || which_trans == TRAN_STATUE))
    {
        if (!involuntary)
            mpr("Your temperature is too high to benefit from that spell.");
        return _abort_or_fizzle(just_check);
    }

    set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);

    int str = 0, dex = 0;
    const char* tran_name = "buggy";
    string msg;

    if (was_in_water && form_can_fly(which_trans))
        msg = "You fly out of the water as you turn into ";
    else if (form_can_fly(previous_trans)
             && form_can_swim(which_trans)
             && feat_is_water(grd(you.pos())))
        msg = "As you dive into the water, you turn into ";
    else
        msg = "You turn into ";

    switch (which_trans)
    {
    case TRAN_SPIDER:
        tran_name = "spider";
        dex       = 5;
        msg      += "a venomous arachnid creature.";
        break;

    case TRAN_BLADE_HANDS:
        tran_name = ("Blade " + uppercase_first(blade_parts(true))).c_str();
        msg       = "Your " + blade_parts()
                    + " turn into razor-sharp scythe blades.";
        break;

    case TRAN_STATUE:
        tran_name = "statue";
        str       = 2;
        dex       = -2;
        if (you.species == SP_DEEP_DWARF && one_chance_in(10))
            msg = "You inwardly fear your resemblance to a lawn ornament.";
        else if (you.species == SP_GARGOYLE)
            msg = "Your body stiffens and grows slower.";
        else
            msg += "a living statue of rough stone.";
        break;

    case TRAN_ICE_BEAST:
        tran_name = "ice beast";
        msg      += "a creature of crystalline ice.";
        break;

    case TRAN_DRAGON:
        tran_name = "dragon";
        str       = 10;
        msg      += "a fearsome dragon!";
        break;

    case TRAN_LICH:
        tran_name = "lich";
        msg       = "Your body is suffused with negative energy!";
        break;

    case TRAN_BAT:
        tran_name = "bat";
        str       = -5;
        dex       = 5;
        if (you.species == SP_VAMPIRE)
            msg += "a vampire bat.";
        else
            msg += "a bat.";
        break;

    case TRAN_PIG:
        tran_name = "pig";
        msg       = "You have been turned into a pig!";
        if (!just_check)
            you.transform_uncancellable = true;
        break;

    case TRAN_APPENDAGE:
    {
        tran_name = "appendage";
        mutation_type app = _beastly_appendage();
        if (app == NUM_MUTATIONS)
        {
            mpr("You have no appropriate body parts free.");
            return false;
        }

        if (!just_check)
        {
            you.attribute[ATTR_APPENDAGE] = app;
            switch (app)
            {
            case MUT_HORNS:
                msg = "You grow a pair of large bovine horns.";
                break;
            case MUT_TENTACLE_SPIKE:
                msg = "One of your tentacles grows a vicious spike.";
                break;
            case MUT_CLAWS:
                msg = "Your hands morph into claws.";
                break;
            case MUT_TALONS:
                msg = "Your feet morph into talons.";
                break;
            default:
                die("Unknown beastly appendage.");
            }
        }
        break;
    }

    case TRAN_FUNGUS:
        tran_name = "fungus";
        msg      += "a fleshy mushroom.";
        if (!just_check)
        {
            you.set_duration(DUR_CONFUSING_TOUCH,
                             you.duration[DUR_TRANSFORMATION]
                                 ? you.duration[DUR_TRANSFORMATION]
                                 : INFINITE_DURATION);
        }
        break;

    case TRAN_JELLY:
        tran_name = "jelly";
        msg      += "a lump of acidic jelly.";
        break;

    case TRAN_PORCUPINE:
        tran_name = "porcupine";
        str       = -3;
        msg      += "a spiny porcupine.";
        break;

    case TRAN_TREE:
        tran_name = "tree";
        str       = 10;
        msg      += "an animated tree.";
        break;

    case TRAN_WISP:
        tran_name = "wisp";
        msg      += "an insubstantial wisp of gas.";
        break;

    case TRAN_NONE:
        tran_name = "null";
        msg += "your old self.";
        break;
    default:
        msg += "something buggy!";
    }

    const bool bad_str = you.strength() > 0 && str + you.strength() <= 0;
    if (!involuntary && just_check && (bad_str
            || you.dex() > 0 && dex + you.dex() <= 0))
    {
        string prompt = make_stringf("Transforming will reduce your %s to zero. Continue?",
                                     bad_str ? "strength" : "dexterity");
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

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

    // Give the transformation message.
    mpr(msg);

    // Update your status.
    you.form = which_trans;
    you.set_duration(DUR_TRANSFORMATION, _transform_duration(which_trans, pow));
    update_player_symbol();

    _remove_equipment(rem_stuff);
    burden_change();

    if (str)
    {
        notify_stat_change(STAT_STR, str, true,
                    make_stringf("gaining the %s transformation",
                                 tran_name).c_str());
    }

    if (dex)
    {
        notify_stat_change(STAT_DEX, dex, true,
                    make_stringf("gaining the %s transformation",
                                 tran_name).c_str());
    }

    _extra_hp(form_hp_mod());

    // Extra effects
    switch (which_trans)
    {
    case TRAN_STATUE:
        if (you.duration[DUR_STONESKIN])
            mpr("Your new body merges with your stone armour.");
        else if (you.species == SP_LAVA_ORC)
            mpr("Your new body is particularly stony.");
        if (you.duration[DUR_ICY_ARMOUR])
        {
            mpr("Your new body cracks your icy armour.", MSGCH_DURATION);
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
            // Some folks claims it's "a bug", and spiders should be immune
            // to webs.  They know how to walk safely, but not if already
            // entangled.  So let's give a message.
            if (trap && trap->type == TRAP_WEB)
                mpr("You wish you had such spider senses a moment ago.");
        }
        break;

    case TRAN_FUNGUS:
        // ignore hunger_state (but don't reset hunger)
        you.hunger_state = HS_SATIATED;
        set_redraw_status(REDRAW_HUNGER);
        break;

    case TRAN_TREE:
        if (you_worship(GOD_FEDHAS) && !player_under_penance())
            simple_god_message(" makes you hardy against extreme temperatures.");
        // ignore hunger_state (but don't reset hunger)
        you.hunger_state = HS_SATIATED;
        set_redraw_status(REDRAW_HUNGER);
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
            mpr("You stop regenerating.", MSGCH_DURATION);
            you.duration[DUR_REGENERATION] = 0;
        }

        you.is_undead = US_UNDEAD;
        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status(REDRAW_HUNGER);
        break;

    case TRAN_APPENDAGE:
        {
            int app = you.attribute[ATTR_APPENDAGE];
            ASSERT(app != NUM_MUTATIONS);
            ASSERT(beastly_slot(app) != EQ_NONE);
            you.mutation[app] = app == MUT_HORNS ? 2 : 3;
        }
        break;

    case TRAN_WISP:
        // ignore hunger_state (but don't reset hunger)
        you.hunger_state = HS_SATIATED;
        set_redraw_status(REDRAW_HUNGER);
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

    you.check_clinging(false);

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
        move_player_to_grid(you.pos(), false, true);
    }

    return true;
}

void untransform(bool skip_wielding, bool skip_move)
{
    const flight_type old_flight = you.flight_mode();

    you.redraw_quiver       = true;
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;

    // Must be unset first or else infinite loops might result. -- bwr
    const transformation_type old_form = you.form;
    int hp_downscale = form_hp_mod();

    // We may have to unmeld a couple of equipment types.
    set<equipment_type> melded = _init_equipment_removal(old_form);

    you.form = TRAN_NONE;
    you.duration[DUR_TRANSFORMATION]   = 0;
    update_player_symbol();

    burden_change();

    switch (old_form)
    {
    case TRAN_SPIDER:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        notify_stat_change(STAT_DEX, -5, true,
                     "losing the spider transformation");
        if (!skip_move)
            you.check_clinging(false);
        break;

    case TRAN_BAT:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        notify_stat_change(STAT_DEX, -5, true,
                     "losing the bat transformation");
        notify_stat_change(STAT_STR, 5, true,
                     "losing the bat transformation");
        break;

    case TRAN_BLADE_HANDS:
        mprf(MSGCH_DURATION, "Your %s revert to their normal proportions.",
             blade_parts().c_str());
        you.wield_change = true;
        break;

    case TRAN_STATUE:
        // This only handles lava orcs going statue -> stoneskin.
        if (you.species == SP_LAVA_ORC && temperature_effect(LORC_STONESKIN)
            || you.species == SP_GARGOYLE)
        {
            mpr("You revert to a slightly less stony form.", MSGCH_DURATION);
        }
        else if (you.species != SP_LAVA_ORC)
            mpr("You revert to your normal fleshy form.", MSGCH_DURATION);
        notify_stat_change(STAT_DEX, 2, true,
                     "losing the statue transformation");
        notify_stat_change(STAT_STR, -2, true,
                     "losing the statue transformation");

        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_STONESKIN])
            you.duration[DUR_STONESKIN] = 1;
        break;

    case TRAN_ICE_BEAST:
        if (you.species == SP_LAVA_ORC && !temperature_effect(LORC_STONESKIN))
            mpr("Your icy form melts away into molten rock.", MSGCH_DURATION);
        else
            mpr("You warm up again.", MSGCH_DURATION);

        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_ICY_ARMOUR])
            you.duration[DUR_ICY_ARMOUR] = 1;
        break;

    case TRAN_DRAGON:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        notify_stat_change(STAT_STR, -10, true,
                    "losing the dragon transformation");
        break;

    case TRAN_LICH:
        mpr("You feel yourself come back to life.", MSGCH_DURATION);
        you.is_undead = US_ALIVE;
        break;

    case TRAN_PIG:
    case TRAN_JELLY:
    case TRAN_PORCUPINE:
    case TRAN_WISP:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        break;

    case TRAN_APPENDAGE:
        {
            int app = you.attribute[ATTR_APPENDAGE];
            ASSERT(beastly_slot(app) != EQ_NONE);
            // would be lots of work to do it via delete_mutation, the hacky
            // way is one line:
            you.mutation[app] = you.innate_mutations[app];
            you.attribute[ATTR_APPENDAGE] = 0;
            mprf(MSGCH_DURATION, "Your %s disappear%s.", appendage_name(app),
                 (app == MUT_TENTACLE_SPIKE) ? "s" : "");
        }
        break;

    case TRAN_FUNGUS:
        mpr("You stop sporulating.", MSGCH_DURATION);
        you.set_duration(DUR_CONFUSING_TOUCH, 0);
        break;
    case TRAN_TREE:
        mpr("You feel less woody.", MSGCH_DURATION);
        if (grd(you.pos()) == DNGN_DEEP_WATER && you.species == SP_TENGU
            && you.experience_level >= 5)
        {
            // Flight was disabled, need to turn it back NOW.
            if (you.experience_level >= 15)
                you.attribute[ATTR_PERM_FLIGHT] = 1;
            else
                you.increase_duration(DUR_FLIGHT, 50, 100);
            mpr("You frantically escape the water.");
        }
        notify_stat_change(STAT_STR, -10, true,
                     "losing the tree transformation");
        break;

    default:
        break;
    }

    _unmeld_equipment(melded);

    if (old_form == TRAN_TREE && grd(you.pos()) == DNGN_DEEP_WATER
        && you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING)
        && !species_likes_water(you.species)
        && !you.attribute[ATTR_PERM_FLIGHT]) // tengu may have both
    {
        you.attribute[ATTR_PERM_FLIGHT] = 1;
        mpr("You frantically enable flight.");
    }

    // Re-check terrain now that be may no longer be swimming or flying.
    if (!skip_move && (old_flight && !you.flight_mode()
                       || (feat_is_water(grd(you.pos()))
                           && (old_form == TRAN_ICE_BEAST
                               || you.species == SP_MERFOLK))))
    {
        move_player_to_grid(you.pos(), false, true);
    }

#ifdef USE_TILE
    if (you.species == SP_MERFOLK)
        init_player_doll();
#endif

    if (form_can_butcher_barehanded(old_form))
        stop_butcher_delay();

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
        you.hp = you.hp * 10 / hp_downscale;
        if (you.hp < 1)
            you.hp = 1;
        else if (you.hp > you.hp_max)
            you.hp = you.hp_max;
    }
    calc_hp();

    // Stop being constricted if we are now too large.
    if (you.is_constricted())
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }

    if (!skip_wielding)
        handle_interrupted_swap(true, false);

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

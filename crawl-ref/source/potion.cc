/**
 * @file
 * @brief Potion and potion-like effects.
**/

#include "AppHdr.h"

#include "potion.h"

#include <cstdio>
#include <cstring>

#include "food.h"
#include "godconduct.h"
#include "hints.h"
#include "itemname.h"
#include "itemprop.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "player-stats.h"
#include "skill_menu.h"
#include "spl-goditem.h"
#include "stringutil.h"
#include "transform.h"
#include "xom.h"

int _xom_factor(bool was_known);

PotionEffect::PotionEffect(const potion_type pot)
    : potion_name(potion_type_name(pot)), kind(pot)
{ }

bool PotionEffect::can_quaff() const
{
    return true;
}

bool PotionEffect::quaff(bool was_known) const
{
    if (was_known && !can_quaff())
    {
        mpr("You shouldn't drink that now.");
        return false;
    }
    effect();
    return true;
}

class PotionCuring : public PotionEffect
{
private:
    PotionCuring() : PotionEffect(POT_CURING) { }
    DISALLOW_COPY_AND_ASSIGN(PotionCuring);
public:
    static const PotionCuring &instance()
    {
        static PotionCuring inst; return inst;
    }

    bool can_quaff() const
    {
        // heal
        // XXX: don't count this when at mhp?
        if (you.can_device_heal() && !you.duration[DUR_DEATHS_DOOR])
            return true;

        // cure status effects
        if (you.duration[DUR_CONF]
            || you.duration[DUR_POISONING]
            || you.rotting
            || you.disease)
        {
            return true;
        }

        // remove rot (when healed to full)
        // (you can actually remove rot when nearly at max hp, but not if you
        // have no device heal, so...)
        if (player_rotted() && you.hp == you.hp_max)
            return true;

        return false;
    }

    bool effect(bool=true, int=40) const
    {
        const bool ddoor = you.duration[DUR_DEATHS_DOOR];
        if (you.can_device_heal() && !ddoor)
        {
            inc_hp((5 + random2(7))
                * (3 - you.mutation[MUT_NO_DEVICE_HEAL])  / 3);
        }

        mprf("You feel %s.", ddoor ? "queasy" : "better");

        // Only fix rot when healed to full.
        if (you.hp == you.hp_max)
        {
            unrot_hp(1);
            set_hp(you.hp_max);
        }

        // need to redraw from yellow to green even if no hp was gained
        if (you.duration[DUR_POISONING])
            you.redraw_hit_points = true;

        you.duration[DUR_POISONING] = 0;
        you.rotting = 0;
        you.disease = 0;
        you.duration[DUR_CONF] = 0;

        return true;
    }

    bool quaff(bool was_known)  const
    {
        if (was_known && !can_quaff())
        {
            mprf("You have no ailments to cure%s.",
                    you.duration[DUR_DEATHS_DOOR] && you.can_device_heal()
                        ? ", and can't heal while in Death's door"
                        : "");
            return false;
        }

        effect(was_known);
        return true;
    }
};

class PotionHealWounds : public PotionEffect
{
private:
    PotionHealWounds() : PotionEffect(POT_HEAL_WOUNDS) { }
    DISALLOW_COPY_AND_ASSIGN(PotionHealWounds);
public:
    static const PotionHealWounds &instance()
    {
        static PotionHealWounds inst; return inst;
    }

    bool can_quaff() const
    {
        // can't heal at all
        if (you.duration[DUR_DEATHS_DOOR] || !you.can_device_heal())
            return false;

        // no point healing (for xom)
        if (you.hp == you.hp_max && player_rotted() == 0)
            return false;

        return true;
    }

    bool effect(bool=true, int=40) const
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            return false;
        }

        if (!you.can_device_heal())
        {
            mpr("That seemed strangely inert.");
            return false;
        }

        inc_hp((10 + random2avg(28, 3))
            * (3 - you.mutation[MUT_NO_DEVICE_HEAL] ) / 3);
        mpr("You feel much better.");

        // only fix rot when healed to full
        if (you.hp == you.hp_max)
        {
            unrot_hp(2 + random2avg(5, 2));
            set_hp(you.hp_max);
        }
        return true;
    }

    bool quaff(bool was_known) const
    {
        if (was_known)
        {
            if (you.duration[DUR_DEATHS_DOOR])
            {
                mpr("You cannot heal while in Death's door!");
                return false;
            }

            if (!you.can_device_heal())
            {
                mpr("That would not heal you.");
                return false;
            }
        }

        effect(was_known);
        return true;
    }
};

/**
 * Return a message for the player drinking blood when a non-vampire.
 */
static string _blood_flavour_message()
{
    if (player_mutation_level(MUT_HERBIVOROUS) < 3 && player_likes_chunks())
        return "This tastes like blood.";
    return "Yuck - this tastes like blood.";
}

class PotionBlood : public PotionEffect
{
private:
    PotionBlood() : PotionEffect(POT_BLOOD) { }
    DISALLOW_COPY_AND_ASSIGN(PotionBlood);
public:
    static const PotionBlood &instance()
    {
        static PotionBlood inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        if (you.species == SP_VAMPIRE)
        {
            mpr("Yummy - fresh blood!");
            lessen_hunger(pow, true);
        }
        else
            mpr(_blood_flavour_message());
            // no actual effect, just 'flavour' ha ha ha
        return true;
    }

    bool quaff(bool was_known) const
    {
        effect(was_known, 1040);
        did_god_conduct(DID_DRINK_BLOOD, 1 + random2(3), was_known);
        return true;
    }
};


class PotionHaste : public PotionEffect
{
private:
    PotionHaste() : PotionEffect(POT_HASTE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionHaste);
public:
    static const PotionHaste &instance()
    {
        static PotionHaste inst; return inst;
    }

    bool can_quaff() const
    {
        return !you.stasis(false);
    }

    bool effect(bool=true, int pow = 40) const
    {
        return haste_player(40 + random2(pow));
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("This potion cannot work under stasis.");
            return false;
        }

        if (effect(was_known))
            did_god_conduct(DID_HASTY, 10, was_known);
        return true;
    }
};

class PotionMight : public PotionEffect
{
private:
    PotionMight() : PotionEffect(POT_MIGHT) { }
    DISALLOW_COPY_AND_ASSIGN(PotionMight);
public:
    static const PotionMight &instance()
    {
        static PotionMight inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        const bool were_mighty = you.duration[DUR_MIGHT] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_mighty ? "mightier" : "very mighty");

        you.increase_duration(DUR_MIGHT, 35 + random2(pow), 80);

        if (!were_mighty)
            notify_stat_change(STAT_STR, 5, true);
        return true;
    }
};

class PotionBrilliance : public PotionEffect
{
private:
    PotionBrilliance() : PotionEffect(POT_BRILLIANCE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionBrilliance);
public:
    static const PotionBrilliance &instance()
    {
        static PotionBrilliance inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        const bool were_brilliant = you.duration[DUR_BRILLIANCE] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_brilliant ? "more clever" : "clever");

        you.increase_duration(DUR_BRILLIANCE, 35 + random2(pow), 80);

        if (!were_brilliant)
            notify_stat_change(STAT_INT, 5, true);

        return true;
    }
};

class PotionAgility : public PotionEffect
{
private:
    PotionAgility() : PotionEffect(POT_AGILITY) { }
    DISALLOW_COPY_AND_ASSIGN(PotionAgility);
public:
    static const PotionAgility &instance()
    {
        static PotionAgility inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        const bool were_agile = you.duration[DUR_AGILITY] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_agile ? "more agile" : "agile");

        you.increase_duration(DUR_AGILITY, 35 + random2(pow), 80);

        if (!were_agile)
            notify_stat_change(STAT_DEX, 5, true);
        return true;
    }
};


class PotionFlight : public PotionEffect
{
private:
    PotionFlight() : PotionEffect(POT_FLIGHT) { }
    DISALLOW_COPY_AND_ASSIGN(PotionFlight);
public:
    static const PotionFlight &instance()
    {
        static PotionFlight inst; return inst;
    }

    bool can_quaff() const
    {
        return flight_allowed();
    }

    bool effect(bool=true, int pow = 40) const
    {
        you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 1;
        fly_player(pow);
        return you.airborne();
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("You cannot fly right now.");
            return false;
        }

        effect(was_known);
        return true;
    }
};

class PotionPoison : public PotionEffect
{
private:
    PotionPoison() : PotionEffect(POT_POISON) { }
    DISALLOW_COPY_AND_ASSIGN(PotionPoison);
public:
    static const PotionPoison &instance()
    {
        static PotionPoison inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        mprf(MSGCH_WARN, "That liquid tasted very nasty...");
        return poison_player(10 + random2avg(15, 2), "", "a potion of poison");
    }

    bool quaff(bool was_known) const
    {
        if (player_res_poison() >= 1)
            mpr("You feel slightly nauseous.");
        else if (effect(was_known))
            xom_is_stimulated(100 / _xom_factor(was_known));
        return true;
    }
};

class PotionCancellation : public PotionEffect
{
private:
    PotionCancellation() : PotionEffect(POT_CANCELLATION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionCancellation);
public:
    static const PotionCancellation &instance()
    {
        static PotionCancellation inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        debuff_player();
        mpr("You feel magically purged.");
        return true;
    }
};

class PotionAmbrosia : public PotionEffect
{
private:
    PotionAmbrosia() : PotionEffect(POT_AMBROSIA) { }
    DISALLOW_COPY_AND_ASSIGN(PotionAmbrosia);
public:
    static const PotionAmbrosia &instance()
    {
        static PotionAmbrosia inst; return inst;
    }

    bool can_quaff() const
    {
        return !you.clarity() && !you.duration[DUR_DIVINE_STAMINA];
    }

    bool effect(bool=true, int=40) const
    {
        const int ambrosia_turns = 3 + random2(8);
        if (confuse_player(ambrosia_turns))
        {
            mprf("You feel%s invigorated.",
                 you.duration[DUR_AMBROSIA] ? " more" : "");
            you.increase_duration(DUR_AMBROSIA, ambrosia_turns);
            return true;
        }

        mpr("You feel briefly invigorated.");
        return false;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("The ambrosia would have no effect on you.");
            return false;
        }

        effect();
        return true;
    }
};

class PotionInvisibility : public PotionEffect
{
private:
    PotionInvisibility() : PotionEffect(POT_INVISIBILITY) { }
    DISALLOW_COPY_AND_ASSIGN(PotionInvisibility);
public:
    static const PotionInvisibility &instance()
    {
        static PotionInvisibility inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        if (you.backlit())
        {
            vector<const char *> afflictions;
            if (you.haloed() && !you.umbraed())
                afflictions.push_back("halo");
            if (get_contamination_level() > 1)
                afflictions.push_back("magical contamination");
            if (you.duration[DUR_CORONA])
                afflictions.push_back("corona");
            if (you.duration[DUR_LIQUID_FLAMES])
                afflictions.push_back("liquid flames");
            if (you.duration[DUR_QUAD_DAMAGE])
                afflictions.push_back("!!!QUAD DAMAGE!!!");

            mprf(MSGCH_DURATION,
                 "You become %stransparent, but the glow from %s "
                 "%s prevents you from becoming "
                 "completely invisible.",
                 you.duration[DUR_INVIS] ? "more " : "",
                 you.haloed() && you.halo_radius2() == -1 ? "the" : "your",
                 comma_separated_line(afflictions.begin(), afflictions.end()).c_str());
        }
        else
        {
            mprf(MSGCH_DURATION, !you.duration[DUR_INVIS]
                 ? "You fade into invisibility!"
                 : "You fade further into invisibility.");
        }

        // Now multiple invisiblity casts aren't as good. -- bwr
        if (!you.duration[DUR_INVIS])
            you.set_duration(DUR_INVIS, 15 + random2(pow), 100);
        else
            you.increase_duration(DUR_INVIS, random2(pow), 100);
        return true;
    }

    bool can_quaff() const
    {
        // TODO: merge with item_use.cc:dont_use_invis()
        return get_contamination_level() <= 1;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("You cannot turn invisible while glowing.");
            return false;
        }

        effect(was_known);
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 1;
        return true;
    }
};

class PotionExperience : public PotionEffect
{
private:
    PotionExperience() : PotionEffect(POT_EXPERIENCE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionExperience);
public:
    static const PotionExperience &instance()
    {
        static PotionExperience inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        if (you.experience_level < you.get_max_xl())
        {
            mpr("You feel more experienced!");
            adjust_level(1, true);

            // Deferred calling level_change() into item_use.cc:3919, after
            // dec_inv_item_quantity. This prevents using SIGHUP to get infinite
            // potions of experience. Reference Mantis #3245. [due]
        }
        else
            mpr("A flood of memories washes over you.");
        more();
        skill_menu(SKMF_EXPERIENCE_POTION, 750 * you.experience_level);
        return true;
    }
};

class PotionMagic : public PotionEffect
{
private:
    PotionMagic() : PotionEffect(POT_MAGIC) { }
    DISALLOW_COPY_AND_ASSIGN(PotionMagic);
public:
    static const PotionMagic &instance()
    {
        static PotionMagic inst; return inst;
    }

    bool can_quaff() const
    {
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DJINNI)
            return PotionHealWounds::instance().can_quaff();
#endif
        return you.magic_points != you.max_magic_points;
    }

    bool effect(bool=true, int pow = 40) const
    {
#if TAG_MAJOR_VERSION == 34
        // Allow repairing rot, disallow going through Death's Door.
        if (you.species == SP_DJINNI
            && PotionHealWounds::instance().can_quaff())
        {
            return PotionHealWounds::instance().effect(pow);
        }
#endif

        inc_mp(10 + random2avg(28, 3));
        mpr("Magic courses through your body.");
        return true;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("Your magic is already full.");
            return false;
        }

        effect();
        return true;
    }
};

class PotionRestoreAbilities : public PotionEffect
{
private:
    PotionRestoreAbilities() : PotionEffect(POT_RESTORE_ABILITIES) { }
    DISALLOW_COPY_AND_ASSIGN(PotionRestoreAbilities);
public:
    static const PotionRestoreAbilities &instance()
    {
        static PotionRestoreAbilities inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        bool nothing_happens = true;
        if (you.duration[DUR_BREATH_WEAPON])
        {
            mprf(MSGCH_RECOVERY, "You have got your breath back.");
            you.duration[DUR_BREATH_WEAPON] = 0;
            nothing_happens = false;
        }

        // Give a message if no message otherwise.
        if (!restore_stat(STAT_ALL, 0, false) && nothing_happens)
            mpr("You feel refreshed.");
        return nothing_happens;
    }
};

class PotionBerserk : public PotionEffect
{
private:
    PotionBerserk() : PotionEffect(POT_BERSERK_RAGE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionBerserk);
public:
    static const PotionBerserk &instance()
    {
        static PotionBerserk inst; return inst;
    }

    bool can_quaff() const
    {
        return you.can_go_berserk(true, true, false);
    }

    bool effect(bool was_known = true, int pow = 40) const
    {
        if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You feel slightly irritated.");
            make_hungry(100, false);
            return false;
        }

        you.go_berserk(was_known, true);
        return true;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && !can_quaff())
        {
            mpr("You cannot go berserk now.");
            return false;
        }

        if (effect(was_known))
            xom_is_stimulated(50);

        return true;
    }
};

/**
 * Is the player unable to mutate (temporarily or permanently) & thus unable
 * to drink a mut potion?
 *
 * Prints a message and returns whether the quaff should be aborted.
 */
static bool _disallow_mut()
{
    if (!undead_mutation_rot())
        return false;

    const bool normally_alive = !you.undead_state(false);
    mprf("You cannot mutate%s.",
         normally_alive ? " at present" : "");
    return true;
}

class PotionCureMutation : public PotionEffect
{
private:
    PotionCureMutation() : PotionEffect(POT_CURE_MUTATION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionCureMutation);
public:
    static const PotionCureMutation &instance()
    {
        static PotionCureMutation inst; return inst;
    }

    bool can_quaff() const
    {
        return you.can_safely_mutate();
    }

    bool effect(bool=true, int=40) const
    {
        mpr("It has a very clean taste.");
        bool mutated = false;
        for (int i = 0; i < 7; i++)
        {
            if (random2(9) >= i)
            {
                mutated |= delete_mutation(RANDOM_MUTATION,
                                           "potion of cure mutation",
                                           false);
            }
        }
        return mutated;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && _disallow_mut())
            return false;
        effect(was_known);
        return true;
    }
};

class PotionMutation : public PotionEffect
{
private:
    PotionMutation() : PotionEffect(POT_MUTATION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionMutation);
public:
    static const PotionMutation &instance()
    {
        static PotionMutation inst; return inst;
    }

    bool can_quaff() const
    {
        return you.can_safely_mutate();
    }

    bool effect(bool=true, int=40) const
    {
        mpr("You feel extremely strange.");
        bool mutated = false;
        for (int i = 0; i < 3; i++)
            mutated |= mutate(RANDOM_MUTATION, "potion of mutation", false);

        learned_something_new(HINT_YOU_MUTATED);
        return mutated;
    }

    bool quaff(bool was_known) const
    {
        if (was_known && _disallow_mut())
            return false;

        effect(was_known);

        // Zin conduct is violated even if you get lucky and don't mutate
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        return true;
    }
};

class PotionBeneficialMutation : public PotionEffect
{
private:
    PotionBeneficialMutation() : PotionEffect(POT_BENEFICIAL_MUTATION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionBeneficialMutation);
public:
    static const PotionBeneficialMutation &instance()
    {
        static PotionBeneficialMutation inst; return inst;
    }

    bool can_quaff() const
    {
        return you.can_safely_mutate();
    }

    bool effect(bool = true, int = 40) const
    {
        const bool mutated = mutate(RANDOM_GOOD_MUTATION,
                                    "potion of beneficial mutation",
                                    true, false, false, true);
        if (undead_mutation_rot()) {
            mpr("You feel dead inside.");
            return mutated;
        }

        if (mutated)
            mpr("You feel fantastic!");
        else
            mpr("You feel fantastic for a moment.");
        learned_something_new(HINT_YOU_MUTATED);
        return mutated;
    }
    bool quaff(bool was_known) const
    {
        if (was_known && _disallow_mut())
            return false;

        effect(was_known);

        // Zin conduct is violated even if you get lucky and don't mutate
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        return true;
    }
};

class PotionResistance : public PotionEffect
{
private:
    PotionResistance() : PotionEffect(POT_RESISTANCE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionResistance);
public:
    static const PotionResistance &instance()
    {
        static PotionResistance inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        mprf(MSGCH_DURATION, "You feel protected.");
        you.increase_duration(DUR_RESISTANCE, random2(pow) + 35);
        return true;
    }
};

class PotionDegeneration : public PotionEffect
{
private:
    PotionDegeneration() : PotionEffect(POT_DEGENERATION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionDegeneration);
public:
    static const PotionDegeneration &instance()
    {
        static PotionDegeneration inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        return lose_stat(STAT_RANDOM, 1 + random2avg(4, 2), false,
                           "drinking a potion of degeneration");
    }

    bool quaff(bool was_known) const
    {
        mpr("There was something very wrong with that liquid.");
        if (effect(was_known))
            xom_is_stimulated( 50 / _xom_factor(was_known));
        return true;
    }
};

class PotionDecay : public PotionEffect
{
private:
    PotionDecay() : PotionEffect(POT_DECAY) { }
    DISALLOW_COPY_AND_ASSIGN(PotionDecay);
public:
    static const PotionDecay &instance()
    {
        static PotionDecay inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        return you.rot(&you, 0, 3 + random2(3));;
    }

    bool quaff(bool was_known) const
    {
        if (effect(was_known))
            xom_is_stimulated( 50 / _xom_factor(was_known));
        return true;
    }
};

class PotionLignify : public PotionEffect
{
private:
    PotionLignify() : PotionEffect(POT_LIGNIFY) { }
    DISALLOW_COPY_AND_ASSIGN(PotionLignify);
public:
    static const PotionLignify &instance()
    {
        static PotionLignify inst; return inst;
    }

    bool can_quaff() const
    {
        // has side effects (prints a message)
        return transform(0, TRAN_TREE, false, true);
    }

    bool effect(bool was_known = true, int=40) const
    {
        return transform(30, TRAN_TREE, !was_known);
    }

    bool quaff(bool was_known) const
    {
        // can_quaff unfortunately prints the message
        if (was_known && !can_quaff())
            return false;

        if (effect(was_known))
        {
            you.transform_uncancellable = true;
            did_god_conduct(DID_CHAOS, 10, was_known);
        }
        else
            mpr("You feel woody for a moment.");
        return true;
    }
};

// Removed potions
#if TAG_MAJOR_VERSION == 34
class PotionBloodCoagulated : public PotionEffect
{
private:
    PotionBloodCoagulated() : PotionEffect(POT_BLOOD_COAGULATED) { }
    DISALLOW_COPY_AND_ASSIGN(PotionBloodCoagulated);
public:
    static const PotionBloodCoagulated &instance()
    {
        static PotionBloodCoagulated inst; return inst;
    }

    bool effect(bool=true, int pow = 40) const
    {
        if (you.species == SP_VAMPIRE)
        {
            mpr("This tastes delicious.");
            lessen_hunger(pow, true);
        }
        else
            mpr(_blood_flavour_message());
            // no actual effect, just 'flavour' ha ha ha
        return true;
    }

    bool quaff(bool was_known) const
    {
        effect(840);
        did_god_conduct(DID_DRINK_BLOOD, 1 + random2(3), was_known);
        return true;
    }
};

class PotionGainStrength : public PotionEffect
{
private:
    PotionGainStrength() : PotionEffect(POT_GAIN_STRENGTH) { }
    DISALLOW_COPY_AND_ASSIGN(PotionGainStrength);
public:
    static const PotionGainStrength &instance()
    {
        static PotionGainStrength inst; return inst;
    }

    bool effect(bool=true, int=40) const
    {
        return mutate(MUT_STRONG, "potion of gain strength",
                      true, false, false, true);
    }

    bool quaff(bool was_known) const
    {
        if (effect())
            learned_something_new(HINT_YOU_MUTATED);
        return true;
    }
};

class PotionGainDexterity : public PotionEffect
{
private:
    PotionGainDexterity() : PotionEffect(POT_GAIN_DEXTERITY) { }
    DISALLOW_COPY_AND_ASSIGN(PotionGainDexterity);
public:
    static const PotionGainDexterity &instance()
    {
        static PotionGainDexterity inst; return inst;
    }
    bool effect(bool=true, int=40) const
    {
        return mutate(MUT_AGILE, "potion of gain dexterity",
                      true, false, false, true);
    }
    bool quaff(bool was_known) const
    {
        if (effect())
            learned_something_new(HINT_YOU_MUTATED);
        return true;
    }
};

class PotionGainIntelligence : public PotionEffect
{
private:
    PotionGainIntelligence() : PotionEffect(POT_GAIN_INTELLIGENCE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionGainIntelligence);
public:
    static const PotionGainIntelligence &instance()
    {
        static PotionGainIntelligence inst; return inst;
    }
    bool effect(bool=true, int=40) const
    {
        return mutate(MUT_STRONG, "potion of gain intelligence",
                      true, false, false, true);
    }
    bool quaff(bool was_known) const
    {
        if (effect())
            learned_something_new(HINT_YOU_MUTATED);
        return true;
    }
};

class PotionPorridge : public PotionEffect
{
private:
    PotionPorridge() : PotionEffect(POT_PORRIDGE) { }
    DISALLOW_COPY_AND_ASSIGN(PotionPorridge);
public:
    static const PotionPorridge &instance()
    {
        static PotionPorridge inst; return inst;
    }
    bool effect(bool=true, int=40) const
    {
        if (you.species == SP_VAMPIRE
            || player_mutation_level(MUT_CARNIVOROUS) == 3)
        {
            mpr("Blech - that potion was really gluggy!");
        }
        else
        {
            mpr("That potion was really gluggy!");
            lessen_hunger(6000, true);
        }
        return true;
    }
};

class PotionWater : public PotionEffect
{
private:
    PotionWater() : PotionEffect(POT_WATER) { }
    DISALLOW_COPY_AND_ASSIGN(PotionWater);
public:
    static const PotionWater &instance()
    {
        static PotionWater inst; return inst;
    }
    bool effect(bool=true, int=40) const
    {
        if (you.species == SP_VAMPIRE)
            mpr("Blech - this tastes like water.");
        else
            mpr("This tastes like water.");
        return true;
    }
};

class PotionSlowing : public PotionEffect
{
private:
    PotionSlowing() : PotionEffect(POT_SLOWING) { }
    DISALLOW_COPY_AND_ASSIGN(PotionSlowing);
public:
    static const PotionSlowing &instance()
    {
        static PotionSlowing inst; return inst;
    }

    bool can_quaff() const
    {
        return you.stasis(false);
    }

    bool effect(bool=true, int pow = 40) const
    {
        return slow_player(10 + random2(pow));
    }
};
#endif

// placeholder 'buggy' potion
class PotionStale : public PotionEffect
{
private:
    PotionStale() : PotionEffect(NUM_POTIONS) { }
    DISALLOW_COPY_AND_ASSIGN(PotionStale);
public:
    static const PotionStale &instance()
    {
        static PotionStale inst; return inst;
    }
    bool effect(bool=true, int=40) const
    {
        mpr("That potion was far past its expiry date.");
        return true;
    }
};

static const PotionEffect* potion_effects[] =
{
    &PotionCuring::instance(),
    &PotionHealWounds::instance(),
    &PotionHaste::instance(),
    &PotionMight::instance(),
    &PotionBrilliance::instance(),
    &PotionAgility::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionGainStrength::instance(),
    &PotionGainDexterity::instance(),
    &PotionGainIntelligence::instance(),
#endif
    &PotionFlight::instance(),
    &PotionPoison::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionSlowing::instance(),
#endif
    &PotionCancellation::instance(),
    &PotionAmbrosia::instance(),
    &PotionInvisibility::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionPorridge::instance(),
#endif
    &PotionDegeneration::instance(),
    &PotionDecay::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionWater::instance(),
#endif
    &PotionExperience::instance(),
    &PotionMagic::instance(),
    &PotionRestoreAbilities::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionPoison::instance(),
#endif
    &PotionBerserk::instance(),
    &PotionCureMutation::instance(),
    &PotionMutation::instance(),
    &PotionResistance::instance(),
    &PotionBlood::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionBloodCoagulated::instance(),
#endif
    &PotionLignify::instance(),
    &PotionBeneficialMutation::instance(),
    &PotionStale::instance()
};

const PotionEffect* get_potion_effect(potion_type pot)
{
    COMPILE_CHECK(ARRAYSZ(potion_effects) == NUM_POTIONS+1);
    ASSERT_RANGE(pot, 0, NUM_POTIONS+1);
    return potion_effects[pot];
}

/**
 * Quaff a potion, identifying it if appropriate & triggering its effects on
 * the player. Does not handle decrementing item quantities.
 *
 * @param potion    The potion (stack) being quaffed.
 * @return          true if the potion was used; false if the player aborted.
 */
bool quaff_potion(item_def &potion)
{
    const bool was_known = item_type_known(potion);

    if (!was_known)
    {
        set_ident_flags(potion, ISFLAG_IDENT_MASK);
        set_ident_type(potion, ID_KNOWN_TYPE);
        mprf("It was a %s.", potion.name(DESC_QUALNAME).c_str());
        identify_healing_pots();
    }

    const potion_type ptyp = static_cast<potion_type>(potion.sub_type);
    return get_potion_effect(ptyp)->quaff(was_known);
}

/**
 * Trigger an effect on the player corresponding to the given potion type.
 *
 * @param effect        The type of potion in question.
 * @param pow           The power of the effect. (Only relevant for some pots.)
 * @param was_known     Whether the player should be held responsible.
 */
void potionlike_effect(potion_type effect, int pow, bool was_known)
{
    get_potion_effect(effect)->effect(was_known, pow);
}

int _xom_factor(bool was_known)
{
    // Knowingly drinking bad potions is much less amusing.
    int xom_factor = 1;
    if (was_known)
    {
        xom_factor *= 2;
        if (!player_in_a_dangerous_place())
            xom_factor *= 3;
    }
    return xom_factor;
}

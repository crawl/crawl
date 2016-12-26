/**
 * @file
 * @brief Potion and potion-like effects.
**/

#include "AppHdr.h"

#include "potion.h"

#include <cstdio>
#include <cstring>

#include "cloud.h"
#include "food.h"
#include "godconduct.h"
#include "godwrath.h" // reduce_xp_penance
#include "hints.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "message.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "skill_menu.h"
#include "spl-goditem.h"
#include "stringutil.h"
#include "transform.h"
#include "xom.h"

int _xom_factor(bool was_known);

PotionEffect::PotionEffect(const potion_type pot)
    : potion_name(potion_type_name(pot)), kind(pot)
{ }

bool PotionEffect::can_quaff(string *reason) const
{
    return true;
}

bool PotionEffect::quaff(bool was_known) const
{
    if (was_known && !check_known_quaff())
        return false;

    effect();
    return true;
}

bool PotionEffect::check_known_quaff() const
{
    string reason;
    if (!can_quaff(&reason))
    {
        mpr(reason);
        return false;
    }
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

    bool can_quaff(string *reason = nullptr) const override
    {
        // cure status effects
        if (you.duration[DUR_CONF]
            || you.duration[DUR_POISONING]
            || you.disease
            || player_rotted())
        {
            return true;
        }
        // heal
        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (reason)
                *reason = "You can't heal while in Death's door.";
            return false;
        }
        if (!you.can_potion_heal()
            || you.hp == you.hp_max && player_rotted() == 0)
        {
            if (reason)
                *reason = "You have no ailments to cure.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int=40, bool is_potion = true) const override
    {
        const bool ddoor = you.duration[DUR_DEATHS_DOOR];
        bool unrotted = false;

        if ((you.can_potion_heal() || !is_potion) && !ddoor || player_rotted())
        {
            int amount = 5 + random2(7);
            if (is_potion && !you.can_potion_heal() && player_rotted())
            {
                // Treat the effectiveness of rot removal as if the player
                // had two levels of MUT_NO_POTION_HEAL
                unrot_hp(div_rand_round(amount,3));
                unrotted = true;
            }
            else
            {
                if (is_potion)
                    amount = you.scale_potion_healing(amount);
                if (player_rotted())
                    unrotted = true;
                // Pay for rot right off the top.
                amount = unrot_hp(amount);
                inc_hp(amount);
            }
        }

        if (ddoor)
            mpr("You feel queasy.");
        else if (you.can_potion_heal()
                 || !is_potion
                 || you.duration[DUR_POISONING]
                 || you.duration[DUR_CONF]
                 || unrotted)
        {
            if (is_potion)
                print_potion_heal_message();
            canned_msg(MSG_GAIN_HEALTH);
        }
        else
            mpr("That felt strangely inert.");
        // need to redraw from yellow to green even if no hp was gained
        if (you.duration[DUR_POISONING])
            you.redraw_hit_points = true;
        you.duration[DUR_POISONING] = 0;
        you.disease = 0;
        you.duration[DUR_CONF] = 0;
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (!you.can_potion_heal())
        {
            if (reason)
                *reason = "That would not heal you.";
            return false;
        }
        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (reason)
                *reason = "You cannot heal while in Death's door!";
            return false;
        }
        if (you.hp == you.hp_max && player_rotted() == 0)
        {
            if (reason)
                *reason = "Your health is already full.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int=40, bool is_potion = true) const override
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            return false;
        }
        if (!you.can_potion_heal() && is_potion)
        {
            mpr("That seemed strangely inert.");
            return false;
        }

        int amount = 10 + random2avg(28, 3);
        if (is_potion)
            amount = you.scale_potion_healing(amount);
        // Pay for rot right off the top.
        amount = unrot_hp(amount);
        inc_hp(amount);
        if (is_potion)
            print_potion_heal_message();
        mpr("You feel much better.");
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

    bool effect(bool=true, int pow = 40, bool=true) const override
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (you.hunger_state == HS_ENGORGED)
        {
            if (reason)
                *reason = "You are much too full right now.";
            return false;
        }
        return true;
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        effect(was_known, 1000);
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (you.stasis())
        {
            if (reason)
                *reason = "Your stasis prevents you from being hasted.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        return haste_player(40 + random2(pow));
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        if (effect())
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

    bool effect(bool=true, int pow = 40, bool=true) const override
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

    bool effect(bool=true, int pow = 40, bool=true) const override
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

    bool effect(bool=true, int pow = 40, bool=true) const override
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (!flight_allowed(true, reason))
        {
            if (reason)
                *reason = "You cannot fly right now.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 1;
        fly_player(pow);
        return you.airborne();
    }
};

#if TAG_MAJOR_VERSION == 34
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

    bool effect(bool=true, int=40, bool=true) const override
    {
        mprf(MSGCH_WARN, "That liquid tasted very nasty...");
        return poison_player(10 + random2avg(15, 2), "", "a potion of poison");
    }

    bool quaff(bool was_known) const override
    {
        if (player_res_poison() >= 1)
            mpr("You feel slightly nauseous.");
        else if (effect(was_known))
            xom_is_stimulated(100 / _xom_factor(was_known));
        return true;
    }
};
#endif

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

    bool effect(bool=true, int=40, bool=true) const override
    {
        debuff_player();
        mpr("You feel magically purged.");
        const int old_contam_level = get_contamination_level();
        contaminate_player(-1 * (1000 + random2(4000)));
        if (old_contam_level && old_contam_level == get_contamination_level())
            mpr("You feel slightly less contaminated with magical energies.");
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

    bool effect(bool=true, int=40, bool=true) const override
    {
        const int ambrosia_turns = 3 + random2(8);
        if (confuse_player(ambrosia_turns, false, true))
        {
            print_potion_heal_message();
            mprf("You feel%s invigorated.",
                 you.duration[DUR_AMBROSIA] ? " more" : "");
            you.increase_duration(DUR_AMBROSIA, ambrosia_turns);
            return true;
        }

        mpr("You feel briefly invigorated.");
        return false;
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

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        if (you.backlit())
        {
            vector<const char *> afflictions;
            if (you.haloed() && !you.umbraed())
                afflictions.push_back("halo");
            if (player_severe_contamination())
                afflictions.push_back("magical contamination");
            if (you.duration[DUR_CORONA])
                afflictions.push_back("corona");
            if (you.duration[DUR_LIQUID_FLAMES])
                afflictions.push_back("liquid flames");
            if (you.duration[DUR_QUAD_DAMAGE])
                afflictions.push_back("!!!QUAD DAMAGE!!!");
            mprf(MSGCH_DURATION,
                 "You become %stransparent, but the glow from %s "
                 "%s prevents you from becoming completely invisible.",
                 you.duration[DUR_INVIS] ? "more " : "",
                 you.haloed() && you.halo_radius() == -1 ? "the" : "your",
                 comma_separated_line(afflictions.begin(),
                                      afflictions.end()).c_str());
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

    bool can_quaff(string *reason = nullptr) const override
    {
        return invis_allowed(true, reason);
    }

    bool quaff(bool was_known) const override
    {
        // Let invis_allowed print the messages and possibly do a prompt.
        if (was_known && !invis_allowed())
            return false;

        effect();
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

    bool effect(bool=true, int=40, bool=true) const override
    {
        if (player_under_penance(GOD_HEPLIAKLQANA))
        {
            simple_god_message(" appreciates the memories.",
                               GOD_HEPLIAKLQANA);
            reduce_xp_penance(GOD_HEPLIAKLQANA, 750 * you.experience_level);
            return true;
        }

        if (you.experience_level < you.get_max_xl())
        {
            mpr("You feel more experienced!");
            // Defer calling level_change() until later in drink() to prevent
            // SIGHUP abuse.
            adjust_level(1, true);
        }
        else
            mpr("A flood of memories washes over you.");
        // these are included in default force_more_message
        skill_menu(SKMF_EXPERIENCE, 750 * you.experience_level);
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

    bool can_quaff(string *reason = nullptr) const override
    {
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DJINNI)
            return PotionHealWounds::instance().can_quaff(reason);
#endif
        if (you.magic_points == you.max_magic_points)
        {
            if (reason)
                *reason = "Your magic is already full.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
#if TAG_MAJOR_VERSION == 34
        // Allow repairing rot, disallow going through Death's Door.
        if (you.species == SP_DJINNI
            && PotionHealWounds::instance().can_quaff())
        {
            return PotionHealWounds::instance().effect(pow);
        }
#endif
        inc_mp(POT_MAGIC_MP);
        mpr("Magic courses through your body.");
        return true;
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

    bool can_quaff(string *reason = nullptr) const override
    {
        return you.can_go_berserk(true, true, true, reason);
    }

    bool effect(bool was_known = true, int pow = 40, bool=true) const override
    {
        if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED)
        {
            mpr("You feel slightly irritated.");
            return false;
        }

        you.go_berserk(was_known, true);
        return true;
    }

    bool quaff(bool was_known) const override
    {
        if (was_known
            && (!check_known_quaff() || !berserk_check_wielded_weapon()))
        {
            return false;
        }

        if (effect(was_known))
            xom_is_stimulated(50);
        return true;
    }
};

/**
 * Is the player unable to mutate (temporarily or permanently) & thus unable
 * to drink a mutation-causing potion?
 * @param reason Pointer to a string where the reason will be stored if unable
 *               to mutate
 * @returns True if the player is able to mutate now.
 */
static bool _disallow_mutate(string *reason)
{
    if (!undead_mutation_rot())
        return false;

    if (reason)
    {
        *reason = make_stringf("You cannot mutate%s.",
                               !you.undead_state(false) ? " at present" : "");
    }
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (_disallow_mutate(reason))
            return false;

        if (!how_mutated(false, false, false))
        {
            if (reason)
            {
                *reason = make_stringf("You have no %smutations to cure!",
                                       how_mutated(false, false, true)
                                       ? "permanent " : "");
            }
            return false;
        }
        return true;
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        if (was_known
            && how_mutated(false, true) > how_mutated(false, true, false)
            && !yesno("Your transient mutations will not be cured; Quaff anyway?",
                      false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
        effect();
        return true;
    }

    bool effect(bool=true, int=40, bool=true) const override
    {
        mpr("It has a very clean taste.");
        bool mutated = false;
        for (int i = 0; i < 7; i++)
        {
            if (random2(9) >= i)
            {
                mutated |= delete_mutation(RANDOM_MUTATION,
                                           "potion of cure mutation", false);
            }
        }
        return mutated;
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (_disallow_mutate(reason))
            return false;
        return true;
    }

    bool effect(bool=true, int=40, bool=true) const override
    {
        mpr("You feel extremely strange.");
        bool mutated = false;
        for (int i = 0; i < 3; i++)
            mutated |= mutate(RANDOM_MUTATION, "potion of mutation", false);

        learned_something_new(HINT_YOU_MUTATED);
        return mutated;
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        string msg = "Really drink that potion of mutation";
        msg += you.rmut_from_item() ? " while resistant to mutation?" : "?";
        if (was_known && (you_worship(GOD_ZIN) || you.rmut_from_item())
            && !yesno(msg.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

        effect();
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (_disallow_mutate(reason))
            return false;
        return true;
    }

    bool effect(bool = true, int = 40, bool=true) const override
    {
        const bool mutated = mutate(RANDOM_GOOD_MUTATION,
                                    "potion of beneficial mutation",
                                    true, false, false, true);
        if (undead_mutation_rot())
        {
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

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        // Beneficial mutations go rMut, so don't prompt in this case.
        if (was_known && you_worship(GOD_ZIN)
            && !yesno("Really drink that potion of beneficial mutation?",
                      false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

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

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        mprf(MSGCH_DURATION, "You feel protected.");
        you.increase_duration(DUR_RESISTANCE, random2(pow) + 35);
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

    bool can_quaff(string *reason = nullptr) const override
    {
        return transform(0, TRAN_TREE, false, true, reason);
    }

    bool effect(bool was_known = true, int=40, bool=true) const override
    {
        return transform(30, TRAN_TREE, !was_known);
    }

    bool quaff(bool was_known) const override
    {
        if (was_known)
        {
            if (!check_known_quaff() || !check_form_stat_safety(TRAN_TREE))
                return false;

            const cloud_type cloud = cloud_type_at(you.pos());
            if (is_damaging_cloud(cloud, false)
                // Tree form is immune to these two.
                && cloud != CLOUD_MEPHITIC && cloud != CLOUD_POISON
                && !yesno(make_stringf("Really become a tree while standing in "
                                       "a cloud of %s?",
                                       cloud_type_name(cloud).c_str()).c_str(),
                          false, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }

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

    bool effect(bool=true, int=40, bool=true) const override
    {
        return you.rot(&you, 3 + random2(3));;
    }

    bool quaff(bool was_known) const override
    {
        if (effect())
            xom_is_stimulated( 50 / _xom_factor(was_known));
        return true;
    }
};

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

    bool effect(bool=true, int pow = 40, bool=true) const override
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (you.hunger_state == HS_ENGORGED)
        {
            *reason = "You are much too full right now.";
            return false;
        }
        return true;
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        effect(was_known, 840);
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

    bool effect(bool=true, int=40, bool=true) const override
    {
        return mutate(MUT_STRONG, "potion of gain strength",
                      true, false, false, true);
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

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

    bool effect(bool=true, int=40, bool=true) const override
    {
        return mutate(MUT_AGILE, "potion of gain dexterity",
                      true, false, false, true);
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

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

    bool effect(bool=true, int=40, bool=true) const override
    {
        return mutate(MUT_STRONG, "potion of gain intelligence",
                      true, false, false, true);
    }

    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

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

    bool effect(bool=true, int=40, bool=true) const override
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

    bool can_quaff(string *reason) const override
    {
        if (you.hunger_state == HS_ENGORGED)
        {
            if (reason)
                *reason = "You are much too full right now.";
            return false;
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
    bool effect(bool=true, int=40, bool=true) const override
    {
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

    bool can_quaff(string *reason = nullptr) const override
    {
        if (you.stasis())
        {
            if (reason)
                *reason = "Your stasis prevents you from being slowed.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        return slow_player(10 + random2(pow));
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

    bool effect(bool=true, int=40, bool=true) const override
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
#endif

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

    bool effect(bool=true, int=40, bool=true) const override
    {
        mpr("There was something very wrong with that liquid.");
        bool success = false;
        for (int i = 0; i < NUM_STATS; ++i)
            if (lose_stat(static_cast<stat_type>(i), 1 + random2(3)))
                success = true;
        return success;
    }

    bool quaff(bool was_known) const override
    {
        if (effect())
            xom_is_stimulated( 50 / _xom_factor(was_known));
        return true;
    }
};

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
    bool effect(bool=true, int=40, bool=true) const override
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
#if TAG_MAJOR_VERSION == 34
    &PotionPoison::instance(),
    &PotionSlowing::instance(),
#endif
    &PotionCancellation::instance(),
    &PotionAmbrosia::instance(),
    &PotionInvisibility::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionPorridge::instance(),
#endif
    &PotionDegeneration::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionDecay::instance(),
    &PotionWater::instance(),
#endif
    &PotionExperience::instance(),
    &PotionMagic::instance(),
#if TAG_MAJOR_VERSION == 34
    &PotionRestoreAbilities::instance(),
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
        set_ident_type(potion, true);
        mprf("It was a %s.", potion.name(DESC_QUALNAME).c_str());
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
    get_potion_effect(effect)->effect(was_known, pow, false);
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

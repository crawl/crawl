/**
 * @file
 * @brief Potion and potion-like effects.
**/

#include "AppHdr.h"

#include "potion.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "art-enum.h"
#include "cloud.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-wrath.h" // reduce_xp_penance
#include "hints.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "message.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "player-stats.h"
#include "potion-type.h"
#include "prompt.h"
#include "religion.h"
#include "skill-menu.h"
#include "spl-goditem.h"
#include "stringutil.h"
#include "transform.h"
#include "xom.h"

int _xom_factor(bool was_known);

PotionEffect::PotionEffect(const potion_type pot)
    : potion_name(potion_type_name(pot)), kind(pot)
{ }

bool PotionEffect::can_quaff(string */*reason*/, bool /*temp*/) const
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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        // no species that can quaff at all have permanent restrictions for
        // curing
        if (!temp)
            return true;

        // cure status effects, allowed even in death's door
        if (you.duration[DUR_CONF]
                    || you.duration[DUR_POISONING])
        {
            return true;
        }
        // heal
        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (reason)
                *reason = "You cannot heal while in death's door.";
            return false;
        }
        if (!you.can_potion_heal(true) || temp && you.hp == you.hp_max)
        {
            // It's not useless to drink at full health if you could hit things.
            if (you.has_mutation(MUT_DRUNKEN_BRAWLING) && has_drunken_brawl_targets())
                return true;

            if (reason)
                *reason = "You have no ailments to cure.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int=40, bool is_potion = true) const override
    {
        const bool ddoor = you.duration[DUR_DEATHS_DOOR];

        if ((you.can_potion_heal() || !is_potion) && !ddoor)
        {
            const int base = 5 + random2(7);
            int amount = base;
            if (is_potion)
                amount = you.scale_potion_healing(amount);
            inc_hp(amount);
        }

        if (ddoor)
            mpr("You feel queasy.");
        else if (you.can_potion_heal()
                 || !is_potion
                 || you.duration[DUR_POISONING]
                 || you.duration[DUR_CONF])
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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        // note: equip, i.e. vines, leads to temp uselessness. Should this act
        // as permauselessness?
        if (!you.can_potion_heal(temp))
        {
            if (reason)
            {
                if (!temp || !you.can_potion_heal(false))
                    *reason = "You cannot be healed by potions.";
                else
                    *reason = "You cannot currently be healed by potions.";
            }
            return false;
        }
        if (temp && you.duration[DUR_DEATHS_DOOR])
        {
            if (reason)
                *reason = "You cannot heal while in death's door.";
            return false;
        }
        if (temp && you.hp == you.hp_max)
        {
            // It's not useless to drink at full health if you could hit things.
            if (you.has_mutation(MUT_DRUNKEN_BRAWLING) && has_drunken_brawl_targets())
                return true;

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

        const int base = 10 + random2avg(28, 3);
        int amount = base;
        if (is_potion)
            amount = you.scale_potion_healing(amount);
        inc_hp(amount);
        if (is_potion)
            print_potion_heal_message();
        mpr("You feel much better.");
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

    bool can_quaff(string *reason = nullptr, bool /* temp */ = true) const override
    {
        if (you.stasis())
        {
            if (reason)
                *reason = "Your stasis prevents you from being hasted.";
            return false;
        }
        else if (have_passive(passive_t::no_haste))
        {
            if (reason)
                *reason = "You are protected from being hasted by Cheibriados.";
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

        effect(); // Chei prevents haste in haste_player().
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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        if (you_worship(GOD_TROG))
        {
            // technically can work under Trog, but it does nothing; so give
            // an informative message instead.
            if (reason)
                *reason = "Trog doesn't allow you to cast spells!";
            return false;
        }
        if (temp && player_equip_unrand(UNRAND_FOLLY))
        {
            if (reason)
                *reason = "Your robe already provides the effects of brilliance.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        const bool were_brilliant = you.duration[DUR_BRILLIANCE] > 0;

        mprf(MSGCH_DURATION, "You feel %sclever all of a sudden.",
             were_brilliant ? "more " : "");
        you.increase_duration(DUR_BRILLIANCE, 35 + random2(pow), 80);
        return true;
    }
};

class PotionAttraction : public PotionEffect
{
private:
    PotionAttraction() : PotionEffect(POT_ATTRACTION) { }
    DISALLOW_COPY_AND_ASSIGN(PotionAttraction);
public:
    static const PotionAttraction &instance()
    {
        static PotionAttraction inst; return inst;
    }

    // note on uselessness: this potion works by status, so should still be
    // allowed with no monsters in LOS. Because it is marked as dangerous,
    // it always prompts. XX maybe add info to the prompt?

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        const bool was_attractive = you.duration[DUR_ATTRACTIVE] > 0;

        mprf(MSGCH_DURATION, "You feel %sattractive to monsters.",
             was_attractive ? "more " : "");

        you.increase_duration(DUR_ATTRACTIVE, 20 + random2(pow)/2);
        return true;
    }
};


class PotionEnlightenment : public PotionEffect
{
private:
    PotionEnlightenment() : PotionEffect(POT_ENLIGHTENMENT) { }
    DISALLOW_COPY_AND_ASSIGN(PotionEnlightenment);
public:
    static const PotionEnlightenment &instance()
    {
        static PotionEnlightenment inst; return inst;
    }

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        fly_player(pow);
        // Try to sync up the flying and the enlightenment.
        // ...sorry about this.
        you.duration[DUR_ENLIGHTENED] = max(you.duration[DUR_FLIGHT],
                                            max(you.duration[DUR_ENLIGHTENED],
                                                25 + random2(pow)));
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

    bool can_quaff(string *reason = nullptr, bool temp=true) const override
    {
        if (temp && !player_is_cancellable())
        {
            if (reason)
                *reason = "There is nothing to cancel.";
            return false;
        }

        return true;
    }

    bool effect(bool=true, int=40, bool=true) const override
    {
        debuff_player(true);
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

        // should be unreachable: nothing blocks intentional confusion. (If
        // this ever changes, consider adding a `can_quaff`)
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
            if (you.duration[DUR_STICKY_FLAME])
                afflictions.push_back("liquid flames");
            if (you.duration[DUR_QUAD_DAMAGE])
                afflictions.push_back("!!!QUAD DAMAGE!!!");
            if (you.form == transformation::flux)
                afflictions.push_back("form");
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

        you.increase_duration(DUR_INVIS, 15 + random2(pow), 100);
        return true;
    }

    bool check_known_quaff() const override
    {
        string reason;
        // invis can be drunk while temp useless, if the player accepts at the
        // prompt
        if (!can_quaff(&reason, false))
        {
            mpr(reason);
            return false;
        }
        if (!can_quaff(&reason, true)
             && !yesno((reason + " Use anyway?").c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
        return true;
    }

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        // quaffing invis
        return invis_allowed(true, reason, temp);
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

    bool effect(bool=true, int pow = 40, bool=true) const override
    {
        if (player_under_penance(GOD_HEPLIAKLQANA))
        {
            simple_god_message(" appreciates the memories.", false,
                               GOD_HEPLIAKLQANA);
            reduce_xp_penance(GOD_HEPLIAKLQANA,
                              750 * you.experience_level * pow / 40);
            return true;
        }

        if (you.experience_level < you.get_max_xl())
        {
            const int levels = min(you.get_max_xl(), pow / 40);
            mpr("You feel more experienced!");
            // Defer calling level_change() until later in drink() to prevent
            // SIGHUP abuse.
            adjust_level(levels, true);
        }
        else
            mpr("A flood of memories washes over you.");

        // these are included in default force_more_message
        const int exp = 7500 * you.experience_level * pow / 40;
        if (you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        {
            you.exp_available += exp;
            train_skills();
        }
        else
            skill_menu(SKMF_EXPERIENCE, exp);

        // the player might meet training targets and need to choose
        // skills
        check_selected_skills();

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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        if (you.has_mutation(MUT_HP_CASTING) || temp && !you.max_magic_points)
        {
            if (reason)
                *reason = "You have no magic to restore.";
            return false;
        }
        else if (temp && you.magic_points == you.max_magic_points)
        {
            // It's not useless to drink at full health if you could hit things.
            if (you.has_mutation(MUT_DRUNKEN_BRAWLING) && has_drunken_brawl_targets())
                return true;

            if (reason)
                *reason = "Your magic is already full.";
            return false;
        }
        return true;
    }

    bool effect(bool=true, int = 40, bool is_potion = true) const override
    {
        int amount = is_potion ? you.scale_potion_mp_healing(POT_MAGIC_MP)
                               : POT_MAGIC_MP;
        inc_mp(amount);
        if (you.has_mutation(MUT_HP_CASTING))
            mpr("Magic washes over you without effect.");
        else
        {
            if (is_potion && player_equip_unrand(UNRAND_KRYIAS))
            {
                item_def* item = you.slot_item(EQ_BODY_ARMOUR);
                mprf("%s enhances the restoration.",
                     item->name(DESC_THE, false, false, false).c_str());
            }
            else if (is_potion && you.has_mutation(MUT_DOUBLE_POTION_HEAL))
                mpr("You savour every drop.");

            mpr("Magic courses through your body.");
        }
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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        return you.can_go_berserk(true, true, true, reason, temp);
    }

    bool effect(bool was_known = true, int = 40, bool=true) const override
    {
        if (you.is_lifeless_undead())
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
 * Is the player able to mutate (temporarily or permanently) & thus unable
 * to drink a mutation-causing potion?  This is a wrapper on `you.can_safely_mutate`.
 *
 * @param reason Pointer to a string where the reason will be stored if unable
 *               to mutate
 * @returns true if the player is able to mutate right now, otherwise false.
 */
static bool _can_mutate(string *reason, bool temp)
{
    if (you.can_safely_mutate(temp))
        return true;

    if (reason)
    {
        *reason = make_stringf("You cannot mutate%s.",
                               you.can_safely_mutate(false) ? " at present" : "");
    }
    return false;
}

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
        const int add = random2(pow) + 35;
        you.increase_duration(DUR_RESISTANCE, add);
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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        const string treason = cant_transform_reason(transformation::tree,
                                                     false, temp);
        if (treason.empty())
            return true;

        if (reason)
            *reason = treason;
        return false;
    }

    bool effect(bool was_known = true, int=40, bool=true) const override
    {
        if (you.form == transformation::death) // Gozag potion petition
        {
            mpr("You're too dead to put down roots!");
            return false;
        }
        return transform(30, transformation::tree, !was_known);
    }

    bool quaff(bool was_known) const override
    {
        if (was_known)
        {
            if (!check_known_quaff()
                || !check_form_stat_safety(transformation::tree))
            {
                return false;
            }

            const cloud_type cloud = cloud_type_at(you.pos());
            if (cloud_damages_over_time(cloud, false)
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

const int MIN_REMOVED = 2;
const int MAX_REMOVED = 3;
const int MIN_ADDED = 1;
const int MAX_ADDED = 3;

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

    bool can_quaff(string *reason = nullptr, bool temp = true) const override
    {
        return _can_mutate(reason, temp);
    }

    bool effect(bool = true, int = 40, bool = true) const override
    {
        if (have_passive(passive_t::cleanse_mut_potions))
            simple_god_message(" cleanses your potion of mutation!");
        else
            mpr("You feel extremely strange.");
        bool mutated = false;
        int remove_mutations = random_range(MIN_REMOVED, MAX_REMOVED);
        int add_mutations = random_range(MIN_ADDED, MAX_ADDED);

        // Remove mutations.
        for (int i = 0; i < remove_mutations; i++)
            mutated |= delete_mutation(RANDOM_MUTATION, "potion of mutation", false);
        if (have_passive(passive_t::cleanse_mut_potions))
            return mutated;
        // Add mutations.
        for (int i = 0; i < add_mutations; i++)
            mutated |= mutate(RANDOM_MUTATION, "potion of mutation", false);
        // Sometimes one good mutation.
        if (coinflip())
        {
            mutated |= mutate(RANDOM_GOOD_MUTATION, "potion of mutation",
                              false);
        }

        learned_something_new(HINT_YOU_MUTATED);
        return mutated;
    }


    bool quaff(bool was_known) const override
    {
        if (was_known && !check_known_quaff())
            return false;

        string msg = "Really drink that potion of mutation";
        msg += you.rmut_from_item() ? " while resistant to mutation?" : "?";
        const bool zin_check = you_worship(GOD_ZIN)
                            && !have_passive(passive_t::cleanse_mut_potions);
        if (zin_check)
            msg += " Zin will disapprove.";
        if (was_known && (zin_check || you.rmut_from_item())
                      && !yesno(msg.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

        effect();
        if (zin_check)
            did_god_conduct(DID_DELIBERATE_MUTATING, 15, was_known);
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

    bool effect(bool=true, int=40, bool=true) const override
    {
        mpr("There was something very wrong with that liquid.");
        bool success = false;
        for (int i = 0; i < NUM_STATS; ++i)
        {
            if (lose_stat(static_cast<stat_type>(i), 1 + random2(3)))
                success = true;
        }
        return success;
    }

    bool quaff(bool was_known) const override
    {
        if (effect())
            xom_is_stimulated( 50 / _xom_factor(was_known));
        return true;
    }
};

static const unordered_map<potion_type, const PotionEffect*, std::hash<int>> potion_effects = {
    { POT_CURING, &PotionCuring::instance(), },
    { POT_HEAL_WOUNDS, &PotionHealWounds::instance(), },
    { POT_HASTE, &PotionHaste::instance(), },
    { POT_MIGHT, &PotionMight::instance(), },
    { POT_BRILLIANCE, &PotionBrilliance::instance(), },
    { POT_ATTRACTION, &PotionAttraction::instance(), },
    { POT_ENLIGHTENMENT, &PotionEnlightenment::instance(), },
    { POT_CANCELLATION, &PotionCancellation::instance(), },
    { POT_AMBROSIA, &PotionAmbrosia::instance(), },
    { POT_INVISIBILITY, &PotionInvisibility::instance(), },
    { POT_DEGENERATION, &PotionDegeneration::instance(), },
    { POT_EXPERIENCE, &PotionExperience::instance(), },
    { POT_MAGIC, &PotionMagic::instance(), },
    { POT_BERSERK_RAGE, &PotionBerserk::instance(), },
    { POT_MUTATION, &PotionMutation::instance(), },
    { POT_RESISTANCE, &PotionResistance::instance(), },
    { POT_LIGNIFY, &PotionLignify::instance(), },
};

const PotionEffect* get_potion_effect(potion_type pot)
{
    switch (pot)
    {
    default:
        return potion_effects.at(pot);
    CASE_REMOVED_POTIONS(pot);
    }
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

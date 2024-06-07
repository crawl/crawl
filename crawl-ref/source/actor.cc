#include "AppHdr.h"

#include "actor.h"

#include <cmath>
#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "attack.h"
#include "chardump.h"
#include "delay.h"
#include "directn.h"
#include "env.h"
#include "fight.h" // apply_chunked_ac
#include "fprop.h"
#include "god-passive.h"
#include "item-prop.h"
#include "los.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "religion.h"
#include "spl-damage.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"

actor::~actor()
{
    if (constricting)
        delete constricting;
}

bool actor::will_trigger_shaft() const
{
    return is_valid_shaft_level()
           // let's pretend that they always make their saving roll
           && !(is_monster()
                && (mons_is_elven_twin(static_cast<const monster* >(this))
                    || as_monster()->type == MONS_ORC_APOSTLE));
}

level_id actor::shaft_dest() const
{
    return generic_shaft_dest(level_id::current());
}

/**
 * Check if the actor is on the ground (or in water).
 */
bool actor::ground_level() const
{
    return !airborne();
}

// Give hands required to wield weapon.
hands_reqd_type actor::hands_reqd(const item_def &item, bool base) const
{
    return basic_hands_reqd(item, body_size(PSIZE_TORSO, base));
}

/**
 * Wrapper around the virtual actor::can_wield(const item_def&,bool,bool,bool,bool) const overload.
 * @param item May be nullptr, in which case a dummy item will be passed in.
 */
bool actor::can_wield(const item_def* item, bool ignore_curse,
                      bool ignore_brand, bool ignore_shield,
                      bool ignore_transform) const
{
    if (item == nullptr)
    {
        // Unarmed combat.
        item_def fake;
        fake.base_type = OBJ_UNASSIGNED;
        return can_wield(fake, ignore_curse, ignore_brand, ignore_shield, ignore_transform);
    }
    else
        return can_wield(*item, ignore_curse, ignore_brand, ignore_shield, ignore_transform);
}

bool actor::can_pass_through(int x, int y) const
{
    return can_pass_through_feat(env.grid[x][y]);
}

bool actor::can_pass_through(const coord_def &c) const
{
    return can_pass_through_feat(env.grid(c));
}

bool actor::is_habitable(const coord_def &_pos) const
{
    return is_habitable_feat(env.grid(_pos));
}

bool actor::is_dragonkind() const {
    return mons_class_is_draconic(mons_species());
}

int actor::dragon_level() const {
    if (!is_dragonkind())
        return 0;
    return min(get_experience_level(), 18);
}

bool actor::handle_trap()
{
    trap_def* trap = trap_at(pos());
    if (trap)
        trap->trigger(*this);
    return trap != nullptr;
}

int actor::skill_rdiv(skill_type sk, int mult, int div) const
{
    return div_rand_round(skill(sk, mult * 256), div * 256);
}

int actor::check_willpower(const actor* source, int power) const
{
    int wl = willpower();

    if (wl == WILL_INVULN)
        return 100;

    if (source && source->wearing_ego(EQ_ALL_ARMOUR, SPARM_GUILE))
        wl = guile_adjust_willpower(wl);

    // Marionettes get better hex success against friends to avoid hex casts
    // often being wasted with normal monster spellpower.
    if (source && source->is_monster()
        && source->as_monster()->attitude == ATT_MARIONETTE
        && mons_atts_aligned(source->real_attitude(), temp_attitude()))
    {
        wl /= 2;
    }

    const int adj_pow = ench_power_stepdown(power);

    const int wlchance = (100 + wl) - adj_pow;
    int wlch2 = random2(100);
    wlch2 += random2(101);

    dprf("Power: %d (%d pre-stepdown), WL: %d, target: %d, roll: %d",
         adj_pow, power, wl, wlchance, wlch2);

    return wlchance - wlch2;
}

void actor::set_position(const coord_def &c)
{
    const coord_def oldpos = position;
    position = c;
    los_actor_moved(this, oldpos);
    areas_actor_moved(this, oldpos);
}

bool actor::can_hibernate(bool holi_only, bool intrinsic_only) const
{
    // Undead, nonliving, and plants don't sleep. If the monster is
    // berserk or already asleep, it doesn't sleep either.
    if (!can_sleep(holi_only))
        return false;

    if (!holi_only)
    {
        // The monster is cold-resistant and can't be hibernated.
        if (intrinsic_only && is_monster()
                ? get_mons_resist(*as_monster(), MR_RES_COLD) > 0
                : res_cold() > 0)
        {
            return false;
        }

        // The monster has slept recently.
        if (is_monster() && !intrinsic_only
            && static_cast<const monster* >(this)->has_ench(ENCH_SLEEP_WARY))
        {
            return false;
        }
    }

    return true;
}

bool actor::can_sleep(bool holi_only) const
{
    const mon_holy_type holi = holiness();
    if (holi & (MH_UNDEAD | MH_NONLIVING | MH_PLANT))
        return false;

    if (!holi_only)
        return !(berserk() || clarity() || asleep());

    return true;
}

void actor::shield_block_succeeded(actor *attacker)
{
    item_def *sh = shield();
    const unrandart_entry *unrand_entry;

    if (sh
        && sh->base_type == OBJ_ARMOUR
        && get_armour_slot(*sh) == EQ_OFFHAND
        && is_artefact(*sh)
        && is_unrandom_artefact(*sh)
        && (unrand_entry = get_unrand_entry(sh->unrand_idx))
        && unrand_entry->melee_effects)
    {
        unrand_entry->melee_effects(sh, this, attacker, false, 0);
    }
}

int actor::get_res(int res) const
{
    switch (res)
    {
    case MR_RES_ELEC:      return res_elec();
    case MR_RES_POISON:    return res_poison();
    case MR_RES_FIRE:      return res_fire();
    case MR_RES_COLD:      return res_cold();
    case MR_RES_NEG:       return res_negative_energy();
    case MR_RES_MIASMA:    return res_miasma();
    case MR_RES_ACID:      return res_acid();
    case MR_RES_TORMENT:   return res_torment();
    case MR_RES_PETRIFY:   return res_petrify();
    case MR_RES_DAMNATION: return res_damnation();
    case MR_RES_STEAM:     return res_steam();
    default:
        ASSERT(false);
        return -1;
    }
}

/// How many levels of penalties does this actor have from inaccuracy-conferring effects?
int actor::inaccuracy() const
{
    const item_def *amu = slot_item(EQ_AMULET);
    return amu && is_unrandom_artefact(*amu, UNRAND_AIR);
}

/// How great are the penalties to this actor's to-hit from any inaccuracy effects they have?
int actor::inaccuracy_penalty() const
{
    return inaccuracy() * 5;
}

bool actor::res_corr(bool /*allow_random*/, bool temp) const
{
    return temp && (wearing(EQ_RINGS, RING_RESIST_CORROSION)
                    || wearing(EQ_BODY_ARMOUR, ARM_ACID_DRAGON_ARMOUR)
                    || scan_artefacts(ARTP_RCORR)
                    || wearing_ego(EQ_ALL_ARMOUR, SPARM_PRESERVATION));
}

bool actor::cloud_immune(bool items) const
{
    const item_def *body_armour = slot_item(EQ_BODY_ARMOUR);
    return items && body_armour
           && is_unrandom_artefact(*body_armour, UNRAND_RCLOUDS);
}

bool actor::holy_wrath_susceptible() const
{
    return res_holy_energy() < 0;
}

// This is a bit confusing. This is not the function that determines whether or
// not an actor is capable of teleporting, only whether they are specifically
// under the influence of the "notele" effect. See actor::no_tele() for a
// superset of this function.
bool actor::has_notele_item(vector<const item_def *> *matches) const
{
    return scan_artefacts(ARTP_PREVENT_TELEPORTATION, matches);
}

int actor::angry(bool items) const
{
    int anger = 0;
    if (is_player() && you.has_mutation(MUT_BERSERK))
        anger += pow(3, you.get_mutation_level(MUT_BERSERK));

    if (!items)
        return anger;

    return anger + 20 * wearing_ego(EQ_ALL_ARMOUR, SPARM_RAGE)
                 + scan_artefacts(ARTP_ANGRY);
}

bool actor::clarity(bool items) const
{
    return items && scan_artefacts(ARTP_CLARITY);
}

bool actor::faith(bool items) const
{
    return items && wearing(EQ_AMULET, AMU_FAITH);
}

int actor::archmagi(bool items) const
{
    return items ? wearing_ego(EQ_ALL_ARMOUR, SPARM_ARCHMAGI)
                   + scan_artefacts(ARTP_ARCHMAGI) : 0;
}

bool actor::no_cast(bool items) const
{
    return items && scan_artefacts(ARTP_PREVENT_SPELLCASTING);
}

bool actor::reflection(bool items) const
{
    return items &&
           (wearing(EQ_AMULET, AMU_REFLECTION)
            || wearing_ego(EQ_ALL_ARMOUR, SPARM_REFLECTION));
}

int actor::extra_harm(bool items) const
{
    if (!items)
        return 0;

    int harm = wearing_ego(EQ_CLOAK, SPARM_HARM) + scan_artefacts(ARTP_HARM);

    return harm > 2 ? 2 : harm;
}

bool actor::rmut_from_item() const
{
    return scan_artefacts(ARTP_RMUT);
}

bool actor::evokable_invis() const
{
    return wearing_ego(EQ_CLOAK, SPARM_INVISIBILITY)
           || scan_artefacts(ARTP_INVISIBLE);
}

// Return an int so we know whether an item is the sole source.
int actor::equip_flight() const
{
    if (is_player() && get_form()->forbids_flight())
        return 0;

    // For the player, this is cached on ATTR_PERM_FLIGHT
    return wearing(EQ_RINGS, RING_FLIGHT)
           + wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING)
           + scan_artefacts(ARTP_FLY);
}

int actor::spirit_shield(bool items) const
{
    int ss = 0;

    if (items)
    {
        ss += wearing_ego(EQ_ALL_ARMOUR, SPARM_SPIRIT_SHIELD);
        ss += wearing(EQ_AMULET, AMU_GUARDIAN_SPIRIT);
    }

    if (is_player())
        ss += you.get_mutation_level(MUT_MANA_SHIELD);

    return ss;
}

bool actor::rampaging() const
{
    return wearing_ego(EQ_ALL_ARMOUR, SPARM_RAMPAGING)
           || scan_artefacts(ARTP_RAMPAGING);
}

int actor::apply_ac(int damage, int max_damage, ac_type ac_rule, bool for_real) const
{
    int ac = max(armour_class(), 0);
    int gdr = gdr_perc();
    int saved = 0;
    switch (ac_rule)
    {
    case ac_type::none:
        return damage;
    case ac_type::proportional:
        saved = damage - apply_chunked_AC(damage, ac);
        return max(damage - saved, 0);
    case ac_type::normal:
        saved = random2(1 + ac);
        break;
    case ac_type::half:
        saved = random2(1 + ac) / 2;
        ac /= 2;
        break;
    case ac_type::triple:
        saved = random2(1 + ac);
        saved += random2(1 + ac);
        saved += random2(1 + ac);
        ac *= 3;
        break;
    default:
        die("invalid AC rule");
    }

    // We only support GDR for normal melee attacks at the moment.
    // EVIL HACK: other callers of this function always pass 0 for max_damage,
    // hence disabling GDR. This is very silly! We should do this better!
    if (ac_rule == ac_type::normal)
        saved = max(saved, min(gdr * max_damage / 100, div_rand_round(ac, 2)));
    if (for_real && (damage > 0) && is_player())
    {
        const item_def *body_armour = slot_item(EQ_BODY_ARMOUR);
        if (body_armour)
            count_action(CACT_ARMOUR, body_armour->sub_type);
        else
            count_action(CACT_ARMOUR, -1); // unarmoured subtype
    }

    return max(damage - saved, 0);
}

int actor::shield_block_limit() const
{
    const item_def *sh = shield();
    if (!sh)
        return 1;
    return ::shield_block_limit(*sh);
}

bool actor::shield_exhausted() const
{
    return shield_blocks >= shield_block_limit();
}

bool actor_slime_wall_immune(const actor *act)
{
    return act->is_player() && have_passive(passive_t::slime_wall_immune)
        || act->res_acid() == 3
        || act->is_monster() && mons_is_slime(*act->as_monster());
}

void actor::clear_constricted()
{
    constricted_by = 0;
    escape_attempts = 0;
}

// End my constriction of the target, but don't yet update my constricting list
// so as not to invalidate iteration.
void actor::end_constriction(mid_t whom, bool intentional, bool quiet,
                             const string& escape_verb)
{
    actor *const constrictee = actor_by_mid(whom);

    if (!constrictee)
        return;

    constrictee->clear_constricted();

    monster * const mons = monster_by_mid(whom);
    bool roots = constrictee->is_player() && you.duration[DUR_GRASPING_ROOTS]
        || mons && mons->has_ench(ENCH_GRASPING_ROOTS);
    bool vile_clutch = constrictee->is_player() && you.duration[DUR_VILE_CLUTCH]
        || mons && mons->has_ench(ENCH_VILE_CLUTCH);

    if (!quiet && alive() && constrictee->alive()
        && (you.see_cell(pos()) || you.see_cell(constrictee->pos())))
    {
        string attacker_desc;
        const string verb = intentional ? "release" : "lose";
        bool force_plural = false;

        if (vile_clutch)
        {
            attacker_desc = "The zombie hands";
            force_plural = true;
        }
        else if (roots)
        {
            attacker_desc = "The roots";
            force_plural = true;
        }
        else
            attacker_desc = name(DESC_THE);

        // Print a different message when breaking free of constriction via
        // blinking or similar
        if (!escape_verb.empty())
        {
            mprf("%s %s free of %s.",
                 constrictee->name(DESC_THE).c_str(), escape_verb.c_str(),
                 lowercase(attacker_desc).c_str());
        }
        else
        {
            mprf("%s %s %s grip on %s.",
                attacker_desc.c_str(),
                force_plural ? verb.c_str()
                            : conj_verb(verb).c_str(),
                force_plural ? "their" : pronoun(PRONOUN_POSSESSIVE).c_str(),
                constrictee->name(DESC_THE).c_str());
        }
    }

    if (vile_clutch)
    {
        if (mons)
            mons->del_ench(ENCH_VILE_CLUTCH);
        else
            you.duration[DUR_VILE_CLUTCH] = 0;
    }
    else if (roots)
    {
        if (mons)
            mons->del_ench(ENCH_GRASPING_ROOTS);
        else
            you.duration[DUR_GRASPING_ROOTS] = 0;
    }

    if (constrictee->is_player())
        you.redraw_evasion = true;
}

void actor::stop_constricting(mid_t whom, bool intentional, bool quiet,
                              const string& escape_verb)
{
    if (!constricting)
        return;

    for (int i = constricting->size() - 1; i >= 0; --i)
    {
        if ((*constricting)[i] == whom)
        {
            end_constriction(whom, intentional, quiet, escape_verb);
            constricting->erase(constricting->begin() + i);

            if (constricting->empty())
            {
                delete constricting;
                constricting = nullptr;
            }

            return;
        }
    }
}

/**
 * Stop constricting all defenders, regardless of type of constriction.
 *
 * @param intentional True if this was intentional, which affects the language
 *                    in any message.
 * @param quiet       If True, don't display a message.
 */
void actor::stop_constricting_all(bool intentional, bool quiet)
{
    if (!constricting)
        return;

    for (const auto &entry : *constricting)
        end_constriction(entry, intentional, quiet);

    delete constricting;
    constricting = nullptr;
}

static bool _invalid_constricting_map_entry(const actor *constrictee)
{
    return !constrictee
        || !constrictee->alive()
        || !constrictee->is_constricted();
}

/**
 * Stop directly constricting all defenders.
 *
 * @param intentional True if this was intentional, which affects the language
 *                    in any message.
 * @param quiet       If True, don't display a message.
 */
void actor::stop_directly_constricting_all(bool intentional, bool quiet)
{
    if (!constricting)
        return;

    for (int i = constricting->size() - 1; i >= 0; --i)
    {
        const actor * const constrictee = actor_by_mid((*constricting)[i]);
        if (_invalid_constricting_map_entry(constrictee)
            || constrictee->get_constrict_type() == CONSTRICT_MELEE)
        {
            end_constriction((*constricting)[i], intentional, quiet);
            constricting->erase(constricting->begin() + i);
        }
    }

    if (constricting->empty())
    {
        delete constricting;
        constricting = nullptr;
    }
}

void actor::stop_being_constricted(bool quiet, const string& escape_verb)
{
    // Make sure we are actually being constricted.
    actor* const constrictor = actor_by_mid(constricted_by);

    if (constrictor)
        constrictor->stop_constricting(mid, false, quiet, escape_verb);

    // In case the actor no longer exists.
    clear_constricted();
}

/**
 * Does the actor have an constrictor that's now invalid? Checks validity based
 * on the type of constriction being done to the actor.
 *
 * @param move True if we are checking after the actor has moved.
 * @returns    True if the constrictor is defined yet invalid, false
 *             otherwise.
 */
bool actor::has_invalid_constrictor(bool move) const
{
    if (!is_constricted())
        return false;

    const actor* const attacker = actor_by_mid(constricted_by, true);
    if (!attacker || !attacker->alive())
        return true;

    // When the player is at the origin, they don't have the normal
    // considerations, since they're just here to avoid messages or LOS
    // effects. Cheibriados' time step abilities are an exception to this as
    // they have the player "leave the normal flow of time" and so should break
    // constriction.
    const bool ignoring_player = attacker->is_player()
        && attacker->pos().origin()
        && !you.duration[DUR_TIME_STEP];

    // Direct constriction (e.g. by nagas and octopode players or AT_CONSTRICT)
    // must happen between adjacent squares.
    const auto typ = get_constrict_type();
    if (typ == CONSTRICT_MELEE)
        return !ignoring_player && !adjacent(attacker->pos(), pos());

    // Indirect constriction requires the defender not to move.
    return move
        // Constriction doesn't work out of LOS, to avoid sauciness.
        || !ignoring_player && !attacker->see_cell(pos())
        || !feat_has_solid_floor(env.grid(pos()));
}

/**
 * Clear any constrictions that are no longer valid.
 *
 * @param movement True if we are clearing invalid constrictions after
 *                 the actor has moved, false otherwise.
 */
void actor::clear_invalid_constrictions(bool move)
{
    if (has_invalid_constrictor(move))
        stop_being_constricted();

    if (!constricting)
        return;

    for (int i = constricting->size() - 1; i >= 0; --i)
    {
        const actor * const constrictee = actor_by_mid((*constricting)[i]);
        if (_invalid_constricting_map_entry(constrictee)
            || constrictee->has_invalid_constrictor())
        {
            stop_constricting((*constricting)[i], false, false);
        }
    }
}

void actor::start_constricting(actor &whom)
{
    if (!constricting)
        constricting = new vector<mid_t>;

    ASSERT(!is_constricting(whom));

    constricting->push_back(whom.mid);
    whom.constricted_by = mid;

    if (whom.is_player())
        you.redraw_evasion = true;
}

int actor::num_constricting() const
{
    return constricting ? constricting->size() : 0;
}

bool actor::is_constricting() const
{
    return constricting && !constricting->empty();
}

bool actor::is_constricting(const actor &victim) const
{
    return is_constricting()
           && find(constricting->begin(), constricting->end(), victim.mid)
              != constricting->end();
}

bool actor::is_constricted() const
{
    return constricted_by;
}

/// Is this actor currently being constricted? If so, in what way?
constrict_type actor::get_constrict_type() const
{
    if (!is_constricted())
        return CONSTRICT_NONE;
    if (is_player())
    {
        if (you.duration[DUR_GRASPING_ROOTS])
            return CONSTRICT_ROOTS;
        else if (you.duration[DUR_VILE_CLUTCH])
            return CONSTRICT_BVC;
        return CONSTRICT_MELEE;
    }
    const monster* mon = as_monster();
    if (mon->has_ench(ENCH_VILE_CLUTCH))
        return CONSTRICT_BVC;
    if (mon->has_ench(ENCH_GRASPING_ROOTS))
        return CONSTRICT_ROOTS;
    return CONSTRICT_MELEE;
}

bool actor::can_engulf(const actor &defender) const
{
    return can_see(defender)
        && !confused()
        && body_size(PSIZE_BODY) >= defender.body_size(PSIZE_BODY)
        && !defender.res_constrict()
        && adjacent(pos(), defender.pos());
}

bool actor::can_constrict(const actor &defender, constrict_type typ) const
{
    if (defender.is_constricted() || defender.res_constrict())
        return false;


    if (typ == CONSTRICT_MELEE)
    {
        return can_engulf(defender)
            && (!is_constricting() || has_usable_tentacle());
    }

    if (!see_cell_no_trans(defender.pos()))
        return false;

    return feat_has_solid_floor(env.grid(defender.pos()));
}

#ifdef DEBUG_DIAGNOSTICS
# define DIAG_ONLY(x) x
#else
# define DIAG_ONLY(x) (void)0
#endif

/*
 * Damage the defender with constriction damage.
 * (The exact damage formula depends on the type of constriction applied,
 * whether physical or magical)
 *
 * @param defender The defender being constricted.
 */
void actor::constriction_damage_defender(actor &defender)
{
    const auto typ = defender.get_constrict_type();
    int damage = constriction_damage(typ);
    DIAG_ONLY(const int basedam = damage);
    damage = defender.apply_ac(damage, 0, ac_type::half);
    DIAG_ONLY(const int acdam = damage);
    damage = timescale_damage(this, damage);
    DIAG_ONLY(const int timescale_dam = damage);

    string exclamations;
    if (damage <= 0 && is_player()
        && you.can_see(defender))
    {
        exclamations = ", but do no damage.";
    }
    else
        exclamations = attack_strength_punctuation(damage);

    if (is_player() || you.can_see(*this))
    {
        string attacker_desc;
        bool force_plural = false;
        switch (typ)
        {
        case CONSTRICT_BVC:
            attacker_desc = "The zombie hands";
            force_plural = true;
            break;
        case CONSTRICT_ROOTS:
            attacker_desc = "The grasping roots";
            force_plural = true;
            break;
        default:
            if (is_player())
                attacker_desc = "You";
            else
                attacker_desc = name(DESC_THE);
            break;
        }

        mprf("%s %s %s%s%s", attacker_desc.c_str(),
             force_plural ? "constrict"
                          : conj_verb("constrict").c_str(),
             defender.name(DESC_THE).c_str(),
#ifdef DEBUG_DIAGNOSTICS
             make_stringf(" for %d", damage).c_str(),
#else
             "",
#endif
             exclamations.c_str());
    }
    else if (you.can_see(defender) || defender.is_player())
    {
        mprf("%s %s constricted%s%s",
             defender.name(DESC_THE).c_str(),
             defender.conj_verb("are").c_str(),
#ifdef DEBUG_DIAGNOSTICS
             make_stringf(" for %d", damage).c_str(),
#else
             "",
#endif
             exclamations.c_str());
    }

    damage = defender.hurt(this, damage, BEAM_MISSILE, KILLED_BY_CONSTRICTION, "",
                           "", false);
    DIAG_ONLY(const int infdam = damage);

    dprf("constrict at: %s df: %s base %d ac %d tsc %d inf %d",
         name(DESC_PLAIN, true).c_str(),
         defender.name(DESC_PLAIN, true).c_str(),
         basedam, acdam, timescale_dam, infdam);

    if (defender.is_monster()
        && defender.type != MONS_NO_MONSTER // already dead and reset
        && defender.as_monster()->hit_points < 1)
    {
        monster_die(*defender.as_monster(), this);
    }
}

// Deal damage over time
void actor::handle_constriction()
{
    if (is_sanctuary(pos()))
        stop_constricting_all(true);

    // Constriction should have stopped the moment the actors became
    // non-adjacent; but disabling constriction by hand in every single place
    // is too error-prone.
    clear_invalid_constrictions();

    if (!constricting)
        return;

    // need to make a copy, since constriction_damage_defender can
    // unpredictably invalidate constricting
    vector<mid_t> tmp_constricting = *constricting;
    for (auto &i : tmp_constricting)
    {
        actor* const defender = actor_by_mid(i);
        if (defender && defender->alive())
            constriction_damage_defender(*defender);
    }

    clear_invalid_constrictions();
}

string actor::describe_props() const
{
    ostringstream oss;

    if (props.size() == 0)
        return "";

    for (auto i = props.begin(); i != props.end(); ++i)
    {
        if (i != props.begin())
            oss <<  ", ";
        oss << string(i->first) << ": ";

        CrawlStoreValue val = i->second;

        switch (val.get_type())
        {
            case SV_BOOL:
                oss << val.get_bool();
                break;
            case SV_BYTE:
                oss << val.get_byte();
                break;
            case SV_SHORT:
                oss << val.get_short();
                break;
            case SV_INT:
                oss << val.get_int();
                break;
            case SV_FLOAT:
                oss << val.get_float();
                break;
            case SV_STR:
                oss << val.get_string();
                break;
            case SV_COORD:
            {
                coord_def coord = val.get_coord();
                oss << "(" << coord.x << ", " << coord.y << ")";
                break;
            }
            case SV_MONST:
            {
                monster mon = val.get_monster();
                oss << mon.name(DESC_PLAIN) << "(" << mon.mid << ")";
                break;
            }
            case SV_INT64:
                oss << val.get_int64();
                break;

            default:
                oss << "???";
                break;
        }
    }
    return oss.str();
}

string actor::resist_margin_phrase(int margin) const
{
    if (willpower() == WILL_INVULN)
        return " " + conj_verb("are") + " unaffected.";

    static const string resist_messages[][2] =
    {
      { " barely %s.",                  "resist" },
      { " %s to resist.",               "struggle" },
      { " %s with significant effort.", "resist" },
      { " %s with some effort.",        "resist" },
      { " easily %s.",                  "resist" },
      { " %s with almost no effort.",   "resist" },
    };

    const int index = max(0, min((int)ARRAYSZ(resist_messages) - 1,
                                 ((margin + 45) / 15)));

    return make_stringf(resist_messages[index][0].c_str(),
                        conj_verb(resist_messages[index][1]).c_str());
}

void actor::collide(coord_def newpos, const actor *agent, int pow)
{
    actor *other = actor_at(newpos);
    // TODO: should the first of these check agent?
    const bool god_prot = god_protects(agent, as_monster());
    const bool god_prot_other = other && god_protects(agent, other->as_monster());

    ASSERT(this != other);
    ASSERT(alive());

    if (is_insubstantial()
        || mons_is_projectile(type)
        || other && mons_is_projectile(other->type))
    {
        return;
    }

    if (is_monster() && !god_prot)
        behaviour_event(as_monster(), ME_WHACK, agent);

    const dice_def damage = collision_damage(pow, true);
    const int dam = apply_ac(damage.roll());

    if (other && other->alive())
    {
        const int damother = other->apply_ac(damage.roll());
        if (you.can_see(*this) || you.can_see(*other))
        {
            mprf("%s %s with %s%s",
                 name(DESC_THE).c_str(),
                 conj_verb("collide").c_str(),
                 other->name(DESC_THE).c_str(),
                 attack_strength_punctuation((dam + damother) / 2).c_str());
            // OK, now do the messaging for god protection.
            if (god_prot)
                god_protects(agent, *as_monster(), false);
            if (god_prot_other)
                god_protects(agent, *other->as_monster(), false);
        }

        if (other->is_monster() && !god_prot_other)
            behaviour_event(other->as_monster(), ME_WHACK, agent);

        const string thisname = name(DESC_A, true);
        const string othername = other->name(DESC_A, true);
        if (other->alive() && !god_prot_other)
        {
            other->hurt(agent, damother, BEAM_MISSILE, KILLED_BY_COLLISION,
                        othername, thisname);
            if (damother && other->is_monster())
                print_wounds(*other->as_monster());
        }
        if (alive() && !god_prot)
        {
            hurt(agent, dam, BEAM_MISSILE, KILLED_BY_COLLISION,
                 thisname, othername);
            if (dam && is_monster())
                print_wounds(*as_monster());
        }
        return;
    }

    if (you.can_see(*this))
    {
        if (!can_pass_through_feat(env.grid(newpos)))
        {
            mprf("%s %s into %s%s",
                 name(DESC_THE).c_str(), conj_verb("slam").c_str(),
                 env.map_knowledge(newpos).known()
                 ? feature_description_at(newpos, false, DESC_THE)
                       .c_str()
                 : "something",
                 attack_strength_punctuation(dam).c_str());
        }
        else
        {
            mprf("%s violently %s moving%s",
                 name(DESC_THE).c_str(), conj_verb("stop").c_str(),
                 attack_strength_punctuation(dam).c_str());
        }

        if (god_prot)
            god_protects(agent, *as_monster(), false); // messaging
    }

    if (!god_prot)
    {
        hurt(agent, dam, BEAM_MISSILE, KILLED_BY_COLLISION,
             "", feature_description_at(newpos));
        if (dam && is_monster())
            print_wounds(*as_monster());
    }
}

/**
 * @brief Attempts to knock this actor away from a specific tile,
 *        using repeated one-tile knockbacks. Any LOS requirements
 *        must be checked by the calling function.
 * @param cause The actor responsible for the knockback.
 * @param dist How far back to try to push this actor.
 * @param pow Determines damage done to us if we hit something. If -1, don't do damage.
 * @param source_name The name of the thing that's pushing this actor.
 * @returns True if this actor is moved from their initial position; false otherwise.
 */

bool actor::knockback(const actor &cause, int dist, int pow, string source_name)
{
    if (is_stationary() || resists_dislodge("being knocked back"))
        return false;

    const coord_def source = cause.pos();
    const coord_def oldpos = pos();

    if (source == oldpos)
        return false;

    ray_def ray;
    fallback_ray(source, oldpos, ray);
    while (ray.pos() != oldpos)
        ray.advance();

    coord_def newpos = oldpos;
    for (int dist_travelled = 0; dist_travelled < dist; ++dist_travelled)
    {
        const ray_def oldray(ray);

        ray.advance();

        newpos = ray.pos();
        if (newpos == oldray.pos()
            || !in_bounds(newpos)
            || cell_is_solid(newpos)
            || actor_at(newpos)
            || !can_pass_through(newpos)
            || !is_habitable(newpos))
        {
            ray = oldray;
            break;
        }

        move_to_pos(newpos);
        if (is_player())
            stop_delay(true);
    }

    if (newpos == oldpos)
        return false;

    if (you.can_see(*this))
    {
        mprf("%s %s knocked back%s%s.",
             name(DESC_THE).c_str(),
             conj_verb("are").c_str(),
             !source_name.empty() ? " by the " : "",
             source_name.c_str());
    }

    if (pow != -1 && pos() != newpos)
        collide(newpos, &cause, pow);

    // Stun the monster briefly so that it doesn't look as though it wasn't
    // knocked back at all
    if (is_monster())
        as_monster()->speed_increment -= random2(6) + 4;

    apply_location_effects(oldpos, cause.is_player()? KILL_YOU_MISSILE : KILL_MON_MISSILE,
                                actor_to_death_source(&cause));

    return true;
}

coord_def actor::stumble_pos(coord_def targ) const
{
    if (is_stationary() || resists_dislodge("") || targ == pos())
        return coord_def();

    const coord_def oldpos = pos();
    ray_def ray;
    fallback_ray(oldpos, targ, ray);
    if (!ray.advance()) // !?
        return coord_def();

    const coord_def back_dir = oldpos - ray.pos();
    const coord_def newpos = oldpos + back_dir;
    if (!adjacent(newpos, oldpos)) // !?
        return coord_def();

    // copied from actor::knockback, ew
    if (!in_bounds(newpos)
        || cell_is_solid(newpos)
        || !can_pass_through(newpos)
        || !is_habitable(newpos))
    {
        return coord_def();
    }

    const actor* other = actor_at(newpos);
    if (other && can_see(*other))
        return coord_def();

    return newpos;
}

void actor::stumble_away_from(coord_def targ, string src)
{
    const coord_def oldpos = pos();
    const coord_def newpos = stumble_pos(targ);

    if (newpos.origin()
        || actor_at(newpos)
        || resists_dislodge("being knocked back"))
    {
        return;
    }

    if (is_player())
        mprf("%s sends you backwards.", uppercase_first(src).c_str());
    else if (you.can_see(*this))
        mprf("%s is knocked back by %s.", name(DESC_THE).c_str(), src.c_str());

    move_to_pos(newpos);

    stop_directly_constricting_all(true);
    if (get_constrict_type() == CONSTRICT_MELEE)
        stop_being_constricted();
    clear_far_engulf();

    apply_location_effects(oldpos, is_player() ? KILL_YOU_MISSILE
                                               : KILL_MON_MISSILE,
                           actor_to_death_source(this));
}

/// Is this creature despised by the so-called 'good gods'?
bool actor::evil() const
{
    return bool(holiness() & (MH_UNDEAD | MH_DEMONIC));
}

/**
 * Ensures that `act` is valid if possible. If this isn't possible,
 * return nullptr. This will convert YOU_FAULTLESS into `you`.
 *
 * @param act the actor to validate.
 *
 * @return an actor that is either the player or passes `!invalid_monster`, or
 *         otherwise `nullptr`.
 */
/* static */ const actor *actor::ensure_valid_actor(const actor *act)
{
    if (!act)
        return nullptr;
    if (act->is_player())
        return act;
    const monster *mon = act->as_monster();
    if (mon->mid == MID_YOU_FAULTLESS)
        return &you;
    if (invalid_monster(mon))
        return nullptr;
    return mon;
}

/// @copydoc actor::ensure_valid_actor(const actor *act)
/* static */ actor *actor::ensure_valid_actor(actor *act)
{
    // Defer to the other function. Since it returns only act, nullptr, or you,
    // none of which points to a const object, the const_cast here is safe.
    return const_cast<actor *>(ensure_valid_actor(static_cast<const actor *>(act)));
}

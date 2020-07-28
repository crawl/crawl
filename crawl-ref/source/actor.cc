#include "AppHdr.h"

#include "actor.h"

#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "attack.h"
#include "chardump.h"
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
                && mons_is_elven_twin(static_cast<const monster* >(this)));
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
    return can_pass_through_feat(grd[x][y]);
}

bool actor::can_pass_through(const coord_def &c) const
{
    return can_pass_through_feat(grd(c));
}

bool actor::is_habitable(const coord_def &_pos) const
{
    return is_habitable_feat(grd(_pos));
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

int actor::check_res_magic(int power)
{
    const int mrs = res_magic();

    if (mrs == MAG_IMMUNE)
        return 100;

    const int adj_pow = ench_power_stepdown(power);

    const int mrchance = (100 + mrs) - adj_pow;
    int mrch2 = random2(100);
    mrch2 += random2(101);

    dprf("Power: %d (%d pre-stepdown), MR: %d, target: %d, roll: %d",
         adj_pow, power, mrs, mrchance, mrch2);

    return mrchance - mrch2;
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

void actor::shield_block_succeeded(actor *foe)
{
    item_def *sh = shield();
    const unrandart_entry *unrand_entry;

    if (sh
        && sh->base_type == OBJ_ARMOUR
        && get_armour_slot(*sh) == EQ_SHIELD
        && is_artefact(*sh)
        && is_unrandom_artefact(*sh)
        && (unrand_entry = get_unrand_entry(sh->unrand_idx))
        && unrand_entry->melee_effects)
    {
        unrand_entry->melee_effects(sh, this, foe, false, 0);
    }
}

int actor::inaccuracy() const
{
    return wearing(EQ_AMULET, AMU_INACCURACY);
}

bool actor::res_corr(bool calc_unid, bool items) const
{
    return items && (wearing(EQ_RINGS, RING_RESIST_CORROSION, calc_unid)
                     || wearing(EQ_BODY_ARMOUR, ARM_ACID_DRAGON_ARMOUR, calc_unid)
                     || scan_artefacts(ARTP_RCORR, calc_unid)
                     || wearing_ego(EQ_ALL_ARMOUR, SPARM_PRESERVATION, calc_unid));
}

bool actor::cloud_immune(bool /*calc_unid*/, bool items) const
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
bool actor::has_notele_item(bool calc_unid, vector<const item_def *> *matches) const
{
    return scan_artefacts(ARTP_PREVENT_TELEPORTATION, calc_unid, matches);
}

// permaswift effects like boots of running and lightning scales
bool actor::run(bool calc_unid, bool items) const
{
    return items && wearing_ego(EQ_BOOTS, SPARM_RUNNING, calc_unid);
}

bool actor::angry(bool calc_unid, bool items) const
{
    return items && scan_artefacts(ARTP_ANGRY, calc_unid);
}

bool actor::clarity(bool calc_unid, bool items) const
{
    return items && scan_artefacts(ARTP_CLARITY, calc_unid);
}

bool actor::faith(bool calc_unid, bool items) const
{
    return items && wearing(EQ_AMULET, AMU_FAITH, calc_unid);
}

int actor::archmagi(bool calc_unid, bool items) const
{
    if (!items)
        return 0;

    return wearing_ego(EQ_ALL_ARMOUR, SPARM_ARCHMAGI, calc_unid);
}

/**
 * Indicates if the actor has an active evocations enhancer.
 *
 * @param calc_unid Whether to identify unknown items that enhance evocations.
 * @param items Whether to count item powers.
 * @return The number of levels of evocations enhancement this actor has.
 */
int actor::spec_evoke(bool calc_unid, bool items) const
{
    UNUSED(calc_unid);
    UNUSED(items);
    return 0;
}

bool actor::no_cast(bool calc_unid, bool items) const
{
    return items && scan_artefacts(ARTP_PREVENT_SPELLCASTING, calc_unid);
}

bool actor::reflection(bool calc_unid, bool items) const
{
    return items && wearing(EQ_AMULET, AMU_REFLECTION, calc_unid);
}

bool actor::extra_harm(bool calc_unid, bool items) const
{
    return items &&
           (wearing_ego(EQ_CLOAK, SPARM_HARM, calc_unid)
            || scan_artefacts(ARTP_HARM, calc_unid));
}

bool actor::rmut_from_item(bool calc_unid) const
{
    return scan_artefacts(ARTP_RMUT, calc_unid);
}

bool actor::evokable_berserk(bool calc_unid) const
{
    return scan_artefacts(ARTP_BERSERK, calc_unid);
}

int actor::evokable_invis(bool calc_unid) const
{
    return wearing_ego(EQ_CLOAK, SPARM_INVISIBILITY, calc_unid)
           + scan_artefacts(ARTP_INVISIBLE, calc_unid);
}

// Return an int so we know whether an item is the sole source.
int actor::evokable_flight(bool calc_unid) const
{
    if (is_player() && get_form()->forbids_flight())
        return 0;

    return wearing(EQ_RINGS, RING_FLIGHT, calc_unid)
           + wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING, calc_unid)
           + scan_artefacts(ARTP_FLY, calc_unid);
}

int actor::spirit_shield(bool calc_unid, bool items) const
{
    int ss = 0;

    if (items)
    {
        ss += wearing_ego(EQ_ALL_ARMOUR, SPARM_SPIRIT_SHIELD, calc_unid);
        ss += wearing(EQ_AMULET, AMU_GUARDIAN_SPIRIT, calc_unid);
    }

    if (is_player())
        ss += you.get_mutation_level(MUT_MANA_SHIELD);

    return ss;
}

bool actor::rampaging(bool calc_unid, bool items) const
{
    return items && wearing_ego(EQ_ALL_ARMOUR, SPARM_RAMPAGING, calc_unid);
}

int actor::apply_ac(int damage, int max_damage, ac_type ac_rule,
                    int stab_bypass, bool for_real) const
{
    int ac = max(armour_class() - stab_bypass, 0);
    int gdr = gdr_perc();
    int saved = 0;
    switch (ac_rule)
    {
    case ac_type::none:
        return damage; // no GDR, too
    case ac_type::proportional:
        ASSERT(stab_bypass == 0);
        saved = damage - apply_chunked_AC(damage, ac);
        saved = max(saved, div_rand_round(max_damage * gdr, 100));
        return max(damage - saved, 0);

    case ac_type::normal:
        saved = random2(1 + ac);
        break;
    case ac_type::half:
        saved = random2(1 + ac) / 2;
        ac /= 2;
        gdr /= 2;
        break;
    case ac_type::triple:
        saved = random2(1 + ac);
        saved += random2(1 + ac);
        saved += random2(1 + ac);
        ac *= 3;
        // apply GDR only twice rather than thrice, that's probably still waaay
        // too good. 50% gives 75% rather than 100%, too.
        gdr = 100 - gdr * gdr / 100;
        break;
    default:
        die("invalid AC rule");
    }

    saved = max(saved, min(gdr * max_damage / 100, ac / 2));
    if (for_real && (damage > 0) && (saved >= damage) && is_player())
    {
        const item_def *body_armour = slot_item(EQ_BODY_ARMOUR);
        if (body_armour)
            count_action(CACT_ARMOUR, body_armour->sub_type);
        else
            count_action(CACT_ARMOUR, -1); // unarmoured subtype
    }

    return max(damage - saved, 0);
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

// End my constriction of i->first, but don't yet update my constricting map,
// so as not to invalidate i.
void actor::end_constriction(mid_t whom, bool intentional, bool quiet)
{
    actor *const constrictee = actor_by_mid(whom);

    if (!constrictee)
        return;

    constrictee->clear_constricted();

    monster * const mons = monster_by_mid(whom);
    bool roots = constrictee->is_player() && you.duration[DUR_GRASPING_ROOTS]
        || mons && mons->has_ench(ENCH_GRASPING_ROOTS);
    bool vile_clutch = mons && mons->has_ench(ENCH_VILE_CLUTCH);

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

        mprf("%s %s %s grip on %s.",
             attacker_desc.c_str(),
             force_plural ? verb.c_str()
                          : conj_verb(verb).c_str(),
             force_plural ? "their" : pronoun(PRONOUN_POSSESSIVE).c_str(),
             constrictee->name(DESC_THE).c_str());
    }

    if (vile_clutch)
        mons->del_ench(ENCH_VILE_CLUTCH);
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

void actor::stop_constricting(mid_t whom, bool intentional, bool quiet)
{
    if (!constricting)
        return;

    auto i = constricting->find(whom);

    if (i != constricting->end())
    {
        end_constriction(whom, intentional, quiet);
        constricting->erase(i);

        if (constricting->empty())
        {
            delete constricting;
            constricting = nullptr;
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
        end_constriction(entry.first, intentional, quiet);

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

    vector<mid_t> need_cleared;
    for (const auto &entry : *constricting)
    {
        const actor * const constrictee = actor_by_mid(entry.first);
        if (_invalid_constricting_map_entry(constrictee)
            || constrictee->is_directly_constricted())
        {
            need_cleared.push_back(entry.first);
        }
    }

    for (auto whom : need_cleared)
    {
        end_constriction(whom, intentional, quiet);
        constricting->erase(whom);
    }

    if (constricting->empty())
    {
        delete constricting;
        constricting = nullptr;
    }
}

void actor::stop_being_constricted(bool quiet)
{
    // Make sure we are actually being constricted.
    actor* const constrictor = actor_by_mid(constricted_by);

    if (constrictor)
        constrictor->stop_constricting(mid, false, quiet);

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
    if (is_directly_constricted())
        return !ignoring_player && !adjacent(attacker->pos(), pos());

    // Indirect constriction requires the defender not to move.
    return move
        // Indirect constriction requires reachable ground.
        || !feat_has_solid_floor(grd(pos()))
        // Constriction doesn't work out of LOS.
        || !ignoring_player && !attacker->see_cell(pos());
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

    vector<mid_t> need_cleared;
    for (const auto &entry : *constricting)
    {
        const actor * const constrictee = actor_by_mid(entry.first, true);
        if (_invalid_constricting_map_entry(constrictee)
            || constrictee->has_invalid_constrictor())
        {
            need_cleared.push_back(entry.first);
        }
    }

    for (auto whom : need_cleared)
        stop_constricting(whom, false, false);
}

void actor::start_constricting(actor &whom, int dur)
{
    if (!constricting)
        constricting = new constricting_t();

    ASSERT(constricting->find(whom.mid) == constricting->end());

    (*constricting)[whom.mid] = dur;
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

bool actor::is_constricted() const
{
    return constricted_by;
}

bool actor::is_directly_constricted() const
{
    return is_constricted()
        && (is_player() && !you.duration[DUR_GRASPING_ROOTS]
            || is_monster()
               && !as_monster()->has_ench(ENCH_VILE_CLUTCH)
               && !as_monster()->has_ench(ENCH_GRASPING_ROOTS));
}

void actor::accum_has_constricted()
{
    if (!constricting)
        return;

    for (auto &entry : *constricting)
        entry.second += you.time_taken;
}

bool actor::can_constrict(const actor* defender, bool direct) const
{
    ASSERT(defender); // XXX: change to actor &defender

    if (direct)
    {
        return (!is_constricting() || has_usable_tentacle())
               && !defender->is_constricted()
               && can_see(*defender)
               && !confused()
               && body_size(PSIZE_BODY) >= defender->body_size(PSIZE_BODY)
               && defender->res_constrict() < 3
               && adjacent(pos(), defender->pos());
    }

    return can_see(*defender)
        && !defender->is_constricted()
        && defender->res_constrict() < 3
        // All current indrect forms of constriction require reachable ground.
        && feat_has_solid_floor(grd(defender->pos()));
}

#ifdef DEBUG_DIAGNOSTICS
# define DIAG_ONLY(x) x
#else
# define DIAG_ONLY(x) (void)0
#endif

/*
 * Damage the defender with constriction damage. Longer duration gives more
 * damage, but with a 50 aut step-down. Direct constriction uses strength-based
 * base damage that is modified by XL, whereas indirect, spell-based
 * constriction uses spellpower.
 *
 * @param defender The defender being constricted.
 * @param duration How long the defender has been constricted in AUT.
 */
void actor::constriction_damage_defender(actor &defender, int duration)
{
    const bool direct = defender.is_directly_constricted();
    const bool vile_clutch = !direct && defender.as_monster()
        && defender.as_monster()->has_ench(ENCH_VILE_CLUTCH);
    int damage = constriction_damage(direct);

    DIAG_ONLY(const int basedam = damage);
    damage += div_rand_round(damage * stepdown((float)duration, 50.0),
                             BASELINE_DELAY * 5);
    if (is_player() && direct)
        damage = div_rand_round(damage * (27 + 2 * you.experience_level), 81);

    DIAG_ONLY(const int durdam = damage);
    damage -= random2(1 + (defender.armour_class() / 2));
    DIAG_ONLY(const int acdam = damage);
    damage = timescale_damage(this, damage);
    DIAG_ONLY(const int timescale_dam = damage);

    damage = defender.hurt(this, damage, BEAM_MISSILE, KILLED_BY_MONSTER, "",
                           "", false);
    DIAG_ONLY(const int infdam = damage);

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
        if (vile_clutch)
        {
            attacker_desc = "The zombie hands";
            force_plural = true;
        }
        else if (!direct)
        {
            attacker_desc = "The grasping roots";
            force_plural = true;
        }
        else if (is_player())
            attacker_desc = "You";
        else
            attacker_desc = name(DESC_THE);

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

    dprf("constrict at: %s df: %s base %d dur %d ac %d tsc %d inf %d",
         name(DESC_PLAIN, true).c_str(),
         defender.name(DESC_PLAIN, true).c_str(),
         basedam, durdam, acdam, timescale_dam, infdam);

    if (defender.is_monster() && defender.as_monster()->hit_points < 1)
        monster_die(*defender.as_monster(), this);
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
    constricting_t tmp_constricting = *constricting;
    for (auto &i : tmp_constricting)
    {
        actor* const defender = actor_by_mid(i.first);
        const int duration = i.second;
        if (defender && defender->alive())
            constriction_damage_defender(*defender, duration);
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

/**
 * Is the actor currently being slowed by a torpor snail?
 */
bool actor::torpor_slowed() const
{
    if (!props.exists(TORPOR_SLOWED_KEY) || is_sanctuary(pos())
        || is_stationary()
        || stasis())
    {
        return false;
    }

    for (monster_near_iterator ri(pos(), LOS_SOLID_SEE); ri; ++ri)
    {
        const monster *mons = *ri;
        if (mons && mons->type == MONS_TORPOR_SNAIL
            && !is_sanctuary(mons->pos())
            && !mons_aligned(mons, this))
        {
            return true;
        }
    }

    return false;
}

string actor::resist_margin_phrase(int margin) const
{
    if (res_magic() == MAG_IMMUNE)
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
    const bool fedhas_prot = agent && agent->deity() == GOD_FEDHAS
                             && is_monster() && fedhas_protects(as_monster());
    const bool fedhas_prot_other = agent && agent->deity() == GOD_FEDHAS
                                   && other && other->is_monster()
                                   && fedhas_protects(other->as_monster());
    ASSERT(this != other);
    ASSERT(alive());

    if (is_insubstantial()
        || mons_is_projectile(type)
        || other && mons_is_projectile(other->type))
    {
        return;
    }

    if (is_monster() && !fedhas_prot)
        behaviour_event(as_monster(), ME_WHACK, agent);

    dice_def damage(2, 1 + pow / 10);

    if (other && other->alive())
    {
        if (you.can_see(*this) || you.can_see(*other))
        {
            mprf("%s %s with %s!",
                 name(DESC_THE).c_str(),
                 conj_verb("collide").c_str(),
                 other->name(DESC_THE).c_str());
            if (fedhas_prot || fedhas_prot_other)
            {
                const bool both = fedhas_prot && fedhas_prot_other;
                simple_god_message(
                    make_stringf(" protects %s plant%s from harm.",
                        agent->is_player() ? "your" :
                        both ? "some" : "a",
                        both ? "s" : "").c_str(), GOD_FEDHAS);
            }
        }

        if (other->is_monster() && !fedhas_prot_other)
            behaviour_event(other->as_monster(), ME_WHACK, agent);

        const string thisname = name(DESC_A, true);
        const string othername = other->name(DESC_A, true);
        if (other->alive() && !fedhas_prot_other)
        {
            other->hurt(agent, other->apply_ac(damage.roll()),
                        BEAM_MISSILE, KILLED_BY_COLLISION,
                        othername, thisname);
        }
        if (alive() && !fedhas_prot)
        {
            hurt(agent, apply_ac(damage.roll()), BEAM_MISSILE,
                 KILLED_BY_COLLISION, thisname, othername);
        }
        return;
    }

    if (you.can_see(*this))
    {
        if (!can_pass_through_feat(grd(newpos)))
        {
            mprf("%s %s into %s!",
                 name(DESC_THE).c_str(), conj_verb("slam").c_str(),
                 env.map_knowledge(newpos).known()
                 ? feature_description_at(newpos, false, DESC_THE)
                       .c_str()
                 : "something");
        }
        else
        {
            mprf("%s violently %s moving!",
                 name(DESC_THE).c_str(), conj_verb("stop").c_str());
        }

        if (fedhas_prot)
        {
            simple_god_message(
                make_stringf(" protects %s plant from harm.",
                    agent->is_player() ? "your" : "a").c_str(), GOD_FEDHAS);
        }
    }

    if (!fedhas_prot)
    {
        hurt(agent, apply_ac(damage.roll()), BEAM_MISSILE,
             KILLED_BY_COLLISION, "", feature_description_at(newpos));
    }
}

/// Is this creature despised by the so-called 'good gods'?
bool actor::evil() const
{
    return bool(holiness() & (MH_UNDEAD | MH_DEMONIC | MH_EVIL));
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

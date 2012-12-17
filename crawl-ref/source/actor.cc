#include "AppHdr.h"

#include "actor.h"
#include "areas.h"
#include "artefact.h"
#include "attack.h"
#include "coord.h"
#include "env.h"
#include "fprop.h"
#include "itemprop.h"
#include "libutil.h"
#include "los.h"
#include "mon-death.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"

actor::~actor()
{
    if (constricting)
        delete constricting;
}

bool actor::has_equipped(equipment_type eq, int sub_type) const
{
    const item_def *item = slot_item(eq, false);
    return (item && item->sub_type == sub_type);
}

bool actor::will_trigger_shaft() const
{
    return (ground_level() && total_weight() > 0 && is_valid_shaft_level()
            // let's pretend that they always make their saving roll
            && !(is_monster()
                 && mons_is_elven_twin(static_cast<const monster* >(this))));
}

level_id actor::shaft_dest(bool known = false) const
{
    return generic_shaft_dest(pos(), known);
}

bool actor::airborne() const
{
    return (is_levitating() || (flight_mode() == FL_FLY && !cannot_move()));
}

/**
 * Check if the actor is on the ground (or in water).
 */
bool actor::ground_level() const
{
    return (!airborne() && !is_wall_clinging());
}

bool actor::stand_on_solid_ground() const
{
    return ground_level() && feat_has_solid_floor(grd(pos()))
           && !feat_is_water(grd(pos()));
}

/**
 * Wrapper around the virtual actor::can_wield(const item_def&,bool,bool,bool,bool) const overload.
 * @param item May be NULL, in which case a dummy item will be passed in.
 */
bool actor::can_wield(const item_def* item, bool ignore_curse,
                      bool ignore_brand, bool ignore_shield,
                      bool ignore_transform) const
{
    if (item == NULL)
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
    if (can_cling_to(_pos))
        return true;

    return is_habitable_feat(grd(_pos));
}

bool actor::handle_trap()
{
    trap_def* trap = find_trap(pos());
    if (trap)
        trap->trigger(*this);
    return (trap != NULL);
}

int actor::skill_rdiv(skill_type sk, int mult, int div) const
{
    return div_rand_round(skill(sk, mult * 256), div * 256);
}

int actor::res_holy_fire() const
{
    if (is_evil() || is_unholy())
        return -1;
    else if (is_holy())
        return 3;
    return 0;
}

int actor::check_res_magic(int power)
{
    const int mrs = res_magic();

    if (mrs == MAG_IMMUNE)
        return 100;

    // Evil, evil hack to make weak one hd monsters easier for first level
    // characters who have resistable 1st level spells. Six is a very special
    // value because mrs = hd * 2 * 3 for most monsters, and the weak, low
    // level monsters have been adjusted so that the "3" is typically a 1.
    // There are some notable one hd monsters that shouldn't fall under this,
    // so we do < 6, instead of <= 6...  or checking mons->hit_dice.  The
    // goal here is to make the first level easier for these classes and give
    // them a better shot at getting to level two or three and spells that can
    // help them out (or building a level or two of their base skill so they
    // aren't resisted as often). - bwr
    if (is_monster() && mrs < 6 && coinflip())
        return -1;

    power = stepdown_value(power, 30, 40, 100, 120);

    const int mrchance = (100 + mrs) - power;
    const int mrch2 = random2(100) + random2(101);

    dprf("Power: %d, MR: %d, target: %d, roll: %d",
         power, mrs, mrchance, mrch2);

    return (mrchance - mrch2);
}

void actor::set_position(const coord_def &c)
{
    const coord_def oldpos = position;
    position = c;
    los_actor_moved(this, oldpos);
    areas_actor_moved(this, oldpos);
}

bool actor::can_hibernate(bool holi_only) const
{
    // Undead, nonliving, and plants don't sleep.
    const mon_holy_type holi = holiness();
    if (holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT)
        return false;

    if (!holi_only)
    {
        // The monster is berserk or already asleep.
        if (!can_sleep())
            return false;

        // The monster is cold-resistant and can't be hibernated.
        if (res_cold() > 0)
            return false;

        // The monster has slept recently.
        if (is_monster()
            && static_cast<const monster* >(this)->has_ench(ENCH_SLEEP_WARY))
        {
            return false;
        }
    }

    return true;
}

bool actor::can_sleep() const
{
    const mon_holy_type holi = holiness();
    if (holi == MH_UNDEAD || holi == MH_NONLIVING || holi == MH_PLANT)
        return false;
    return !(berserk() || asleep());
}

void actor::shield_block_succeeded(actor *foe)
{
    item_def *sh = shield();
    unrandart_entry *unrand_entry;

    if (sh
        && sh->base_type == OBJ_ARMOUR
        && get_armour_slot(*sh) == EQ_SHIELD
        && is_artefact(*sh)
        && is_unrandom_artefact(*sh)
        && (unrand_entry = get_unrand_entry(sh->special))
        && unrand_entry->fight_func.melee_effects)
    {
       unrand_entry->fight_func.melee_effects(sh, this, foe, false, 0);
    }
}

int actor::body_weight(bool base) const
{
    switch (body_size(PSIZE_BODY, base))
    {
    case SIZE_TINY:
        return 150;
    case SIZE_LITTLE:
        return 300;
    case SIZE_SMALL:
        return 425;
    case SIZE_MEDIUM:
        return 550;
    case SIZE_LARGE:
        return 1300;
    case SIZE_BIG:
        return 1500;
    case SIZE_GIANT:
        return 1800;
    case SIZE_HUGE:
        return 2200;
    default:
        die("invalid body weight");
    }
}

bool actor_slime_wall_immune(const actor *act)
{
    // res_acid is immunity only for monsters; players need Jiyva
    return (act->is_player() ?
              you.religion == GOD_JIYVA && !you.penance[GOD_JIYVA]
            : act->res_acid());
}
/**
 * Accessor method to the clinging member.
 *
 * @returns The value of clinging.
 */
bool actor::is_wall_clinging() const
{
    return props.exists("clinging") && props["clinging"].get_bool();
}

/**
 * Check a cell to see if actor can keep clinging if it moves to it.
 *
 * @param p Coordinates of the cell checked.
 * @returns Whether the actor can cling.
 */
bool actor::can_cling_to(const coord_def& p) const
{
    if (!is_wall_clinging() || !can_pass_through_feat(grd(p)))
        return false;

    return cell_can_cling_to(pos(), p);
}

/**
 * Update the clinging status of an actor.
 *
 * It checks adjacent orthogonal walls to see if the actor can cling to them.
 * If actor has fallen from the wall (wall dug or actor changed form), print a
 * message and apply location effects.
 *
 * @param stepped Whether the actor has taken a step.
 * @return the new clinging status.
 */
bool actor::check_clinging(bool stepped, bool door)
{
    bool was_clinging = is_wall_clinging();
    bool clinging = can_cling_to_walls() && cell_is_clingable(pos())
                    && !airborne();

    if (can_cling_to_walls())
        props["clinging"] = clinging;
    else if (props.exists("clinging"))
        props.erase("clinging");

    if (!stepped && was_clinging && !clinging)
    {
        if (you.can_see(this))
        {
            mprf("%s fall%s off the %s.", name(DESC_THE).c_str(),
                 is_player() ? "" : "s", door ? "door" : "wall");
        }
        apply_location_effects(pos());
    }
    return clinging;
}

void actor::clear_clinging()
{
    if (props.exists("clinging"))
        props["clinging"] = false;
}

void actor::clear_constricted()
{
    constricted_by = 0;
    held = HELD_NONE;
    escape_attempts = 0;
}

// End my constriction of i->first, but don't yet update my constricting map,
// so as not to invalidate i.
void actor::end_constriction(actor::constricting_t::iterator i,
                             bool intentional, bool quiet)
{
    actor *const constrictee = actor_by_mid(i->first);

    if (!constrictee)
        return;

    constrictee->clear_constricted();

    if (!quiet && alive() && constrictee->alive()
        && (you.see_cell(pos()) || you.see_cell(constrictee->pos())))
    {
        mprf("%s %s %s grip on %s.",
                name(DESC_THE).c_str(),
                conj_verb(intentional ? "release" : "lose").c_str(),
                pronoun(PRONOUN_POSSESSIVE).c_str(),
                constrictee->name(DESC_THE).c_str());
    }
}

void actor::stop_constricting(mid_t whom, bool intentional, bool quiet)
{
    if (!constricting)
        return;

    constricting_t::iterator i = constricting->find(whom);

    if (i != constricting->end())
    {
        end_constriction(i, intentional, quiet);
        constricting->erase(i);

        if (constricting->empty())
        {
            delete constricting;
            constricting = 0;
        }
    }
}

void actor::stop_constricting_all(bool intentional, bool quiet)
{
    if (!constricting)
        return;

    constricting_t::iterator i;

    for (i = constricting->begin(); i != constricting->end(); ++i)
        end_constriction(i, intentional, quiet);

    delete constricting;
    constricting = 0;
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

void actor::clear_far_constrictions()
{
    actor* const constrictor = actor_by_mid(constricted_by);

    if (!constrictor || !adjacent(pos(), constrictor->pos()))
        stop_being_constricted();

    if (!constricting)
        return;

    std::vector<mid_t> need_cleared;
    constricting_t::iterator i;
    for (i = constricting->begin(); i != constricting->end(); ++i)
    {
        actor* const constrictee = actor_by_mid(i->first);
        if (!constrictee || !adjacent(pos(), constrictee->pos()))
            need_cleared.push_back(i->first);
    }

    std::vector<mid_t>::iterator j;
    for (j = need_cleared.begin(); j != need_cleared.end(); ++j)
        stop_constricting(*j, false, false);
}

void actor::start_constricting(actor &whom, int dur)
{
    if (!constricting)
        constricting = new constricting_t();

    ASSERT(constricting->find(whom.mid) == constricting->end());

    (*constricting)[whom.mid] = dur;
    whom.constricted_by = mid;
    whom.held = constriction_damage() ? HELD_CONSTRICTED : HELD_MONSTER;
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

void actor::accum_has_constricted()
{
    if (!constricting)
        return;

    constricting_t::iterator i;
    for (i = constricting->begin(); i != constricting->end(); ++i)
        i->second += you.time_taken;
}

bool actor::can_constrict(actor* defender)
{
    return (!is_constricting() || has_usable_tentacle())
           && !defender->is_constricted()
           && can_see(defender)
           && !confused()
           && body_size(PSIZE_BODY) >= defender->body_size(PSIZE_BODY)
           && defender->res_constrict() < 3
           && adjacent(pos(), defender->pos());
}

#ifdef DEBUG_DIAGNOSTICS
# define DIAG_ONLY(x) x
#else
# define DIAG_ONLY(x) (void)0
#endif

// Deal damage over time
void actor::handle_constriction()
{
    if (is_sanctuary(pos()))
        stop_constricting_all(true);

    // Constriction should have stopped the moment the actors became
    // non-adjacent; but disabling constriction by hand in every single place
    // is too error-prone.
    clear_far_constrictions();

    if (!constricting || !constriction_damage())
        return;

    actor::constricting_t::iterator i = constricting->begin();
    // monster_die() can cause constricting() to go away.
    while (constricting && i != constricting->end())
    {
        actor* const defender = actor_by_mid(i->first);
        int duration = i->second;
        ASSERT(defender);

        // Must increment before potentially killing the constrictee and
        // thus invalidating the old i.
        ++i;

        int damage = constriction_damage();

        DIAG_ONLY(const int basedam = damage);
        damage += div_rand_round(damage * stepdown((float)duration, 50.0),
                                 BASELINE_DELAY * 5);
        if (is_player())
            damage = div_rand_round(damage * (27 + 2 * you.experience_level), 81);
        DIAG_ONLY(const int durdam = damage);
        damage -= random2(1 + (defender->armour_class() / 2));
        DIAG_ONLY(const int acdam = damage);
        damage = timescale_damage(this, damage);
        DIAG_ONLY(const int timescale_dam = damage);

        damage = defender->hurt(this, damage, BEAM_MISSILE, false);
        DIAG_ONLY(const int infdam = damage);

        std::string exclams;
        if (damage <= 0 && is_player()
            && you.can_see(defender))
        {
            exclams = ", but do no damage.";
        }
        else if (damage < HIT_WEAK)
            exclams = ".";
        else if (damage < HIT_MED)
            exclams = "!";
        else if (damage < HIT_STRONG)
            exclams = "!!";
        else
            exclams = "!!!";

        if (is_player() || you.can_see(this))
        {
            mprf("%s %s %s%s%s",
                 (is_player() ? "You"
                              : name(DESC_THE).c_str()),
                 conj_verb("constrict").c_str(),
                 defender->name(DESC_THE).c_str(),
#ifdef DEBUG_DIAGNOSTICS
                 make_stringf(" for %d", damage).c_str(),
#else
                 "",
#endif
                 exclams.c_str());
        }
        else if (you.can_see(defender) || defender->is_player())
        {
            mprf("%s %s constricted%s%s",
                 defender->name(DESC_THE).c_str(),
                 defender->conj_verb("are").c_str(),
#ifdef DEBUG_DIAGNOSTICS
                 make_stringf(" for %d", damage).c_str(),
#else
                 "",
#endif
                 exclams.c_str());
        }

        dprf("constrict at: %s df: %s base %d dur %d ac %d tsc %d inf %d",
             name(DESC_PLAIN, true).c_str(),
             defender->name(DESC_PLAIN, true).c_str(),
             basedam, durdam, acdam, timescale_dam, infdam);

        if (defender->is_monster()
            && defender->as_monster()->hit_points < 1)
        {
            monster_die(defender->as_monster(), this);
        }
    }
}

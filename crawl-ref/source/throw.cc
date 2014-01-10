/**
 * @file
 * @brief Throwing and launching stuff.
**/

#include "AppHdr.h"
#include <sstream>
#include <math.h>

#include "throw.h"

#include "externs.h"

#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "command.h"
#include "delay.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "fineff.h"
#include "godabil.h"
#include "godconduct.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mutation.h"
#include "options.h"
#include "religion.h"
#include "shout.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "version.h"
#include "view.h"
#include "viewchar.h"

static int  _fire_prompt_for_item();
static bool _fire_validate_item(int selected, string& err);

bool item_is_quivered(const item_def &item)
{
    return item.link == you.m_quiver->get_fire_item();
}

int get_next_fire_item(int current, int direction)
{
    vector<int> fire_order;
    you.m_quiver->get_fire_order(fire_order, true);

    if (fire_order.empty())
        return -1;

    int next = direction > 0 ? 0 : -1;
    for (unsigned i = 0; i < fire_order.size(); i++)
    {
        if (fire_order[i] == current)
        {
            next = i + direction;
            break;
        }
    }

    next = (next + fire_order.size()) % fire_order.size();
    return fire_order[next];
}

class fire_target_behaviour : public targeting_behaviour
{
public:
    fire_target_behaviour()
        : chosen_ammo(false),
          selected_from_inventory(false),
          need_redraw(false)
    {
        m_slot = you.m_quiver->get_fire_item(&m_noitem_reason);
        set_prompt();
    }

    // targeting_behaviour API
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw() const { return need_redraw; }
    virtual void clear_redraw()        { need_redraw = false; }
    virtual void update_top_prompt(string* p_top_prompt);
    virtual vector<string> get_monster_desc(const monster_info& mi);

public:
    const item_def* active_item() const;
    // FIXME: these should be privatized and given accessors.
    int m_slot;
    bool chosen_ammo;

private:
    void set_prompt();
    void cycle_fire_item(bool forward);
    void pick_fire_item_from_inventory();
    void display_help();

    string prompt;
    string m_noitem_reason;
    string internal_prompt;
    bool selected_from_inventory;
    bool need_redraw;
};

void fire_target_behaviour::update_top_prompt(string* p_top_prompt)
{
    *p_top_prompt = internal_prompt;
}

const item_def* fire_target_behaviour::active_item() const
{
    if (m_slot == -1)
        return NULL;
    else
        return &you.inv[m_slot];
}

void fire_target_behaviour::set_prompt()
{
    string old_prompt = internal_prompt; // Keep for comparison at the end.
    internal_prompt.clear();

    // Figure out if we have anything else to cycle to.
    const int next_item = get_next_fire_item(m_slot, +1);
    const bool no_other_items = (next_item == -1 || next_item == m_slot);

    ostringstream msg;

    // Build the action.
    if (!active_item())
        msg << "Firing ";
    else
    {
        const launch_retval projected = is_launched(&you, you.weapon(),
                                                    *active_item());
        switch (projected)
        {
        case LRET_FUMBLED:  msg << "Awkwardly throwing "; break;
        case LRET_LAUNCHED: msg << "Firing ";             break;
        case LRET_THROWN:   msg << "Throwing ";           break;
        }
    }

    // And a key hint.
    msg << (no_other_items ? "(i - inventory)"
                           : "(i - inventory. (,) - cycle)")
        << ": ";

    // Describe the selected item for firing.
    if (!active_item())
        msg << "<red>" << m_noitem_reason << "</red>";
    else
    {
        const char* colour = (selected_from_inventory ? "lightgrey" : "w");
        msg << "<" << colour << ">"
            << active_item()->name(DESC_INVENTORY_EQUIP)
            << "</" << colour << ">";
    }

    // Write it out.
    internal_prompt += msg.str();

    // Never unset need_redraw here, because we might have cleared the
    // screen or something else which demands a redraw.
    if (internal_prompt != old_prompt)
        need_redraw = true;
}

// Cycle to the next (forward == true) or previous (forward == false)
// fire item.
void fire_target_behaviour::cycle_fire_item(bool forward)
{
    const int next = get_next_fire_item(m_slot, forward ? 1 : -1);
    if (next != m_slot && next != -1)
    {
        m_slot = next;
        selected_from_inventory = false;
        chosen_ammo = true;
    }
    set_prompt();
}

void fire_target_behaviour::pick_fire_item_from_inventory()
{
    need_redraw = true;
    string err;
    const int selected = _fire_prompt_for_item();
    if (selected >= 0 && _fire_validate_item(selected, err))
    {
        m_slot = selected;
        selected_from_inventory = true;
        chosen_ammo = true;
    }
    else if (!err.empty())
    {
        mprf("%s", err.c_str());
        more();
    }
    set_prompt();
}

void fire_target_behaviour::display_help()
{
    show_targeting_help();
    redraw_screen();
    need_redraw = true;
    set_prompt();
}

command_type fire_target_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    switch (key)
    {
    case '(': case CONTROL('N'): cycle_fire_item(true);  return CMD_NO_CMD;
    case ')': case CONTROL('P'): cycle_fire_item(false); return CMD_NO_CMD;
    case 'i': pick_fire_item_from_inventory(); return CMD_NO_CMD;
    case '?': display_help(); return CMD_NO_CMD;
    case CMD_TARGET_CANCEL: chosen_ammo = false; break;
    }

    return targeting_behaviour::get_command(key);
}

vector<string> fire_target_behaviour::get_monster_desc(const monster_info& mi)
{
    vector<string> descs;
    if (const item_def* item = active_item())
    {
        if (get_ammo_brand(*item) == SPMSL_SILVER && mi.is(MB_CHAOTIC))
            descs.push_back("chaotic");
    }
    return descs;
}

static bool _fire_choose_item_and_target(int& slot, dist& target,
                                         bool teleport = false)
{
    fire_target_behaviour beh;
    const bool was_chosen = (slot != -1);

    if (was_chosen)
    {
        string warn;
        if (!_fire_validate_item(slot, warn))
        {
            mpr(warn.c_str());
            return false;
        }
        // Force item to be the prechosen one.
        beh.m_slot = slot;
    }

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.needs_path = !teleport;
    args.behaviour = &beh;

    direction(target, args);

    if (!beh.active_item())
    {
        canned_msg(MSG_OK);
        return false;
    }
    if (!target.isValid)
    {
        if (target.isCancel)
            canned_msg(MSG_OK);
        return false;
    }

    you.m_quiver->on_item_fired(*beh.active_item(), beh.chosen_ammo);
    you.redraw_quiver = true;
    slot = beh.m_slot;

    return true;
}

// Bring up an inventory screen and have user choose an item.
// Returns an item slot, or -1 on abort/failure
// On failure, returns error text, if any.
static int _fire_prompt_for_item()
{
    if (inv_count() < 1)
        return -1;

    int slot = prompt_invent_item("Fire/throw which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_THROWABLE, true, true, true, 0, -1,
                                   NULL, OPER_FIRE);

    if (slot == PROMPT_ABORT || slot == PROMPT_NOTHING)
        return -1;

    return slot;
}

// Returns false and err text if this item can't be fired.
static bool _fire_validate_item(int slot, string &err)
{
    if (slot == you.equip[EQ_WEAPON]
        && is_weapon(you.inv[slot])
        && you.inv[slot].cursed())
    {
        err = "That weapon is stuck to your " + you.hand_name(false) + "!";
        return false;
    }
    else if (item_is_worn(slot))
    {
        err = "You are wearing that object!";
        return false;
    }
    return true;
}

// Returns true if warning is given.
bool fire_warn_if_impossible(bool silent)
{
    if (you.species == SP_FELID)
    {
        if (!silent)
            mpr("You can't grasp things well enough to throw them.");
        return true;
    }

    // If you can't wield it, you can't throw it.
    if (!form_can_wield())
    {
        if (!silent)
            canned_msg(MSG_PRESENT_FORM);
        return true;
    }

    if (you.attribute[ATTR_HELD])
    {
        const item_def *weapon = you.weapon();
        if (!weapon || !is_range_weapon(*weapon))
        {
            if (!silent)
                mprf("You cannot throw anything while %s.", held_status());
            return true;
        }
        else if (weapon->sub_type != WPN_BLOWGUN)
        {
            if (!silent)
            {
                mprf("You cannot shoot with your %s while %s.",
                     weapon->name(DESC_BASENAME).c_str(), held_status());
            }
            return true;
        }
        // Else shooting is possible.
    }
    if (you.berserk())
    {
        if (!silent)
            canned_msg(MSG_TOO_BERSERK);
        return true;
    }
    return false;
}

static bool _autoswitch_to_ranged()
{
    if (you.equip[EQ_WEAPON] != 0 && you.equip[EQ_WEAPON] != 1)
        return false;

    int item_slot = you.equip[EQ_WEAPON] ^ 1;
    const item_def& launcher = you.inv[item_slot];
    if (!is_range_weapon(launcher))
        return false;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
        if (iter->launched_by(launcher))
        {
            if (!wield_weapon(true, item_slot))
                return false;

            you.turn_is_over = true;
            //XXX Hacky. Should use a delay instead.
            macro_buf_add(command_to_key(CMD_FIRE));
            return true;
        }

    return false;
}

int get_ammo_to_shoot(int item, dist &target, bool teleport)
{
    if (fire_warn_if_impossible())
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return -1;
    }

    if (Options.auto_switch && you.m_quiver->get_fire_item() == -1
       && _autoswitch_to_ranged())
    {
        return -1;
    }

    if (!_fire_choose_item_and_target(item, target, teleport))
        return -1;

    string warn;
    if (!_fire_validate_item(item, warn))
    {
        mpr(warn.c_str());
        return -1;
    }
    return item;
}

// If item == -1, prompt the user.
// If item passed, it will be put into the quiver.
void fire_thing(int item)
{
    dist target;
    item = get_ammo_to_shoot(item, target);
    if (item == -1)
        return;

    if (check_warning_inscriptions(you.inv[item], OPER_FIRE))
    {
        bolt beam;
        throw_it(beam, item, false, 0, &target);
        if (you_worship(GOD_DITHMENGOS))
            dithmengos_shadow_throw(beam.target);
    }
}

// Basically does what throwing used to do: throw an item without changing
// the quiver.
void throw_item_no_quiver()
{
    if (fire_warn_if_impossible())
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    string warn;
    int slot = _fire_prompt_for_item();

    if (slot == -1)
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!_fire_validate_item(slot, warn))
    {
        mpr(warn.c_str());
        return;
    }

    // Okay, item is valid.
    bolt beam;
    throw_it(beam, slot);
}

// Returns delay multiplier numerator (denominator should be 100) for the
// launcher with the currently equipped shield.
static int _launcher_shield_slowdown(const item_def &launcher,
                                     const item_def *shield)
{
    int speed_adjust = 100;
    if (!shield || you.hands_reqd(launcher) == HANDS_ONE)
        return speed_adjust;

    const int shield_type = shield->sub_type;
    speed_adjust = shield_type == ARM_BUCKLER  ? 125 :
                   shield_type == ARM_SHIELD   ? 150 :
                                                 200;

    // Adjust for shields skill.
    if (speed_adjust > 100)
        speed_adjust -= you.skill_rdiv(SK_SHIELDS, speed_adjust - 100, 27 * 2);

    return speed_adjust;
}

// Returns the attack cost of using the launcher, taking skill and shields
// into consideration. NOTE: You must pass in the shield; if you send in
// NULL, this function assumes no shield is in use.
int launcher_final_speed(const item_def &launcher, const item_def *shield, bool scaled)
{
    const int  str_weight   = weapon_str_weight(launcher);
    const int  dex_weight   = 10 - str_weight;
    const skill_type launcher_skill = range_skill(launcher);
    const int shoot_skill4 = you.skill(launcher_skill, 4);
    const int bow_brand = get_weapon_brand(launcher);

    int speed_base = 10 * property(launcher, PWPN_SPEED);
    int speed_min = 10 * weapon_min_delay(launcher);
    int speed_stat = str_weight * you.strength() + dex_weight * you.dex();

    if (shield)
    {
        const int speed_adjust = _launcher_shield_slowdown(launcher, shield);

        // Shields also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    // Do the same when trying to shoot while held in a net
    // (only possible with blowguns).
    if (you.attribute[ATTR_HELD])
    {
        int speed_adjust = 105; // Analogous to buckler and one-handed weapon.
        speed_adjust -= you.skill_rdiv(SK_THROWING, speed_adjust - 100, 27 * 2);

        // Also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    int speed = speed_base - shoot_skill4 * speed_stat / 250;
    if (speed < speed_min)
        speed = speed_min;

    if (bow_brand == SPWPN_SPEED)
    {
        // Speed nerf as per 4.1. Even with the nerf, bows of speed are the
        // best bows, bar none.
        speed = 2 * speed / 3;
    }

    if (scaled)
        speed = finesse_adjust_delay(speed);

    return speed;
}

// Determines if the combined launcher + ammo brands produce a
// fire/frost/chaos beam.
static bool _elemental_missile_beam(int launcher_brand, int ammo_brand)
{
    if (launcher_brand == SPWPN_FLAME && ammo_brand == SPMSL_FROST ||
        launcher_brand == SPWPN_FROST && ammo_brand == SPMSL_FLAME)
    {
        return false;
    }
    if (ammo_brand == SPMSL_CHAOS || ammo_brand == SPMSL_FROST || ammo_brand == SPMSL_FLAME)
        return true;
    if (ammo_brand != SPMSL_NORMAL)
        return false;
    return launcher_brand == SPWPN_CHAOS || launcher_brand == SPWPN_FROST ||
           launcher_brand == SPWPN_FLAME;
}

static bool _poison_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (victim->is_player())
        maybe_id_resist(BEAM_POISON);

    if (!victim->alive() || victim->res_poison() > 0)
        return false;

    if (beam.is_tracer)
        return true;

    int levels = 0;

    actor* agent = beam.agent();

    if (dmg > 0 || beam.ench_power == AUTOMATIC_HIT
                   && x_chance_in_y(90 - 3 * victim->armour_class(), 100))
    {
        levels = 1 + random2(3);
    }

    if (levels <= 0)
        return false;

    if (victim->poison(agent, levels) && victim->is_monster())
        behaviour_event(victim->as_monster(), ME_ANNOY, agent);

    return true;
}

static bool _item_penetrates_victim(const bolt &beam, int &used)
{
    if (beam.aimed_at_feet)
        return false;

    used = 0;

    return true;
}

static bool _silver_damages_victim(bolt &beam, actor* victim, int &dmg,
                                   string &dmg_msg)
{
    int mutated = 0;

    // For mutation damage, we want to count innate mutations for
    // the demonspawn, but not for other species.
    if (you.species == SP_DEMONSPAWN)
        mutated = how_mutated(true, true);
    else
        mutated = how_mutated(false, true);

    if (victim->is_chaotic()
        || (victim->is_player() && player_is_shapechanged()))
    {
        dmg *= 7;
        dmg /= 4;
    }
    else if (victim->is_player() && mutated > 0)
    {
        int multiplier = 100 + mutated * 5;

        if (multiplier > 175)
            multiplier = 175;

        dmg = dmg * multiplier / 100;
    }
    else
        return false;

    if (!beam.is_tracer && you.can_see(victim))
       dmg_msg = "The silver sears " + victim->name(DESC_THE) + "!";

    return false;
}

static bool _dispersal_hit_victim(bolt& beam, actor* victim, int dmg)
{
    const actor* agent = beam.agent();

    if (!victim->alive() || victim == agent || dmg == 0)
        return false;

    if (beam.is_tracer)
        return true;

    if (victim->no_tele(true, true))
    {
        if (victim->is_player())
            canned_msg(MSG_STRANGE_STASIS);
        return false;
    }

    const bool was_seen = you.can_see(victim);
    const bool no_sanct = victim->kill_alignment() == KC_OTHER;

    coord_def pos, pos2;

    int tries = 0;
    do
    {
        if (!random_near_space(victim->pos(), pos, false, true, false,
                               no_sanct))
        {
            return false;
        }
    }
    while (!victim->is_habitable(pos) && tries++ < 100);

    if (!victim->is_habitable(pos))
        return false;

    tries = 0;
    do
        if (!random_near_space(victim->pos(), pos2, false, true, false,
                               no_sanct))
        {
            return false;
        }
    while (!victim->is_habitable(pos2) && tries++ < 100);

    if (!victim->is_habitable(pos2))
        return false;

    // Pick the square further away from the agent.
    const coord_def from = agent->pos();
    if (in_bounds(pos2) && distance2(pos2, from) > distance2(pos, from))
        pos = pos2;

    if (pos == victim->pos())
        return false;

    const coord_def oldpos = victim->pos();
    victim->clear_clinging();

    if (victim->is_player())
    {
        stop_delay(true);

        // Leave a purple cloud.
        if (!cell_is_solid(you.pos()))
            place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), &you);

        canned_msg(MSG_YOU_BLINK);
        move_player_to_grid(pos, false, true);
    }
    else
    {
        monster* mon = victim->as_monster();

        if (!(mon->flags & MF_WAS_IN_VIEW))
            mon->seen_context = SC_TELEPORT_IN;

        mon->move_to_pos(pos);

        // Leave a purple cloud.
        if (!cell_is_solid(oldpos))
            place_cloud(CLOUD_TLOC_ENERGY, oldpos, 1 + random2(3), victim);

        mon->apply_location_effects(oldpos);
        mon->check_redraw(oldpos);

        const bool   seen = you.can_see(mon);
        const string name = mon->name(DESC_THE);
        if (was_seen && seen)
            mprf("%s blinks!", name.c_str());
        else if (was_seen && !seen)
            mprf("%s vanishes!", name.c_str());
    }

    return true;
}

static bool _charged_damages_victim(bolt &beam, actor* victim, int &dmg,
                                    string &dmg_msg)
{
    if (victim->airborne() || victim->res_elec() > 0 || !one_chance_in(3))
        return false;

    // A hack and code duplication, but that's easier than adding accounting
    // for each of multiple brands.
    if (victim->type == MONS_SIXFIRHY)
    {
        if (!beam.is_tracer)
            victim->heal(10 + random2(15));
        // physical damage is still done
    }
    else
        dmg += 10 + random2(15);

    if (beam.is_tracer)
        return false;

    if (you.can_see(victim))
    {
        if (victim->is_player())
            dmg_msg = "You are electrocuted!";
        else if (victim->type == MONS_SIXFIRHY)
            dmg_msg = victim->name(DESC_THE) + " is charged up!";
        else
            dmg_msg = "There is a sudden explosion of sparks!";
    }

    if (feat_is_water(grd(victim->pos())))
        (new lightning_fineff(beam.agent(), victim->pos()))->schedule();

    return false;
}

static bool _blessed_damages_victim(bolt &beam, actor* victim, int &dmg,
                                    string &dmg_msg)
{
    if (victim->undead_or_demonic())
    {
        dmg += 1 + random2(dmg * 15) / 10;

        if (!beam.is_tracer && you.can_see(victim))
           dmg_msg = victim->name(DESC_THE) + " "
                   + victim->conj_verb("convulse") + "!";
    }

    return false;
}

static int _blowgun_duration_roll(bolt &beam, const actor* victim,
                                  special_missile_type type)
{
    actor* agent = beam.agent(true);
    if (!agent)
        return 0;

    int base_power;
    item_def* blowgun;
    if (agent->is_monster())
    {
        base_power = agent->get_experience_level();
        blowgun = agent->as_monster()->launcher();
    }
    else
    {
        base_power = agent->skill_rdiv(SK_THROWING);
        blowgun = agent->weapon();
    }

    ASSERT(blowgun);
    ASSERT(blowgun->sub_type == WPN_BLOWGUN);

    // Scale down nastier needle effects against players.
    // Fixed duration regardless of power, since power already affects success
    // chance considerably, and this helps avoid effects being too nasty from
    // high HD shooters and too ignorable from low ones.
    if (victim->is_player())
    {
        switch (type)
        {
            case SPMSL_PARALYSIS:
                return 3 + random2(4);
            case SPMSL_SLEEP:
                return 5 + random2(5);
            case SPMSL_CONFUSION:
                return 2 + random2(4);
            case SPMSL_SLOW:
                return 5 + random2(7);
            default:
                return 5 + random2(5);
        }
    }
    else
        return 5 + random2(base_power + blowgun->plus);
}

static bool _blowgun_check(bolt &beam, actor* victim, special_missile_type type,
                           bool message = true)
{
    if (victim->holiness() == MH_UNDEAD || victim->holiness() == MH_NONLIVING)
    {
        if (victim->is_monster())
            simple_monster_message(victim->as_monster(), " is unaffected.");
        else
            canned_msg(MSG_YOU_UNAFFECTED);
        return false;
    }

    actor* agent = beam.agent(true);
    if (!agent)
        return false;

    const item_def* wp = agent->weapon();
    ASSERT(wp);
    ASSERT(wp->sub_type == WPN_BLOWGUN);
    const int enchantment = wp->plus;

    if (agent->is_monster())
    {
        int chance = 85 - ((victim->get_experience_level()
                            - agent->get_experience_level()) * 5 / 2);
        chance += wp->plus * 4;
        chance = min(95, chance);

        if (type == SPMSL_FRENZY)
            chance = chance / 2;
        else if (type == SPMSL_PARALYSIS || type == SPMSL_SLEEP)
            chance = chance * 4 / 5;

        return x_chance_in_y(chance, 100);
    }

    const int skill = you.skill_rdiv(SK_THROWING);

    // You have a really minor chance of hitting with no skills or good
    // enchants.
    if (victim->get_experience_level() < 15 && random2(100) <= 2)
        return true;

    const int resist_roll = 2 + random2(4 + skill + enchantment);

    dprf("Brand rolled %d against defender HD: %d.",
         resist_roll, victim->get_experience_level());

    if (resist_roll < victim->get_experience_level())
    {
        if (victim->is_monster())
            simple_monster_message(victim->as_monster(), " resists.");
        else
            canned_msg(MSG_YOU_RESIST);
        return false;
    }

    return true;
}

static bool _paralysis_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_PARALYSIS))
        return false;

    int dur = _blowgun_duration_roll(beam, victim, SPMSL_PARALYSIS);
    victim->paralyse(beam.agent(), dur);
    return true;
}

static bool _sleep_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_SLEEP))
        return false;

    int dur = _blowgun_duration_roll(beam, victim, SPMSL_SLEEP);
    victim->put_to_sleep(beam.agent(), dur);
    return true;
}

static bool _confusion_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_CONFUSION))
        return false;

    int dur = _blowgun_duration_roll(beam, victim, SPMSL_CONFUSION);
    victim->confuse(beam.agent(), dur);
    return true;
}

static bool _slow_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_SLOW))
        return false;

    int dur = _blowgun_duration_roll(beam, victim, SPMSL_SLOW);
    victim->slow_down(beam.agent(), dur);
    return true;
}

#if TAG_MAJOR_VERSION == 34
static bool _sickness_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_SICKNESS))
        return false;

    int dur = _blowgun_duration_roll(beam, victim, SPMSL_SICKNESS);
    victim->sicken(40 + random2(dur));
    return true;
}
#endif

static bool _rage_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!_blowgun_check(beam, victim, SPMSL_FRENZY))
        return false;

    if (victim->is_monster())
        victim->as_monster()->go_frenzy(beam.agent());
    else
        victim->go_berserk(false);

    return true;
}

#if TAG_MAJOR_VERSION == 34
static bool _blind_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return false;

    if (!victim->is_monster())
    {
        victim->confuse(beam.agent(), 7);
        return true;
    }

    if (victim->as_monster()->has_ench(ENCH_BLIND))
        return false;

    victim->as_monster()->add_ench(mon_enchant(ENCH_BLIND, 1, beam.agent(), 35));

    return true;
}
#endif

static bool _setup_missile_beam(const actor *agent, bolt &beam, item_def &item,
                                string &ammo_name, bool &returning)
{
    dungeon_char_type zapsym = NUM_DCHAR_TYPES;
    switch (item.base_type)
    {
    case OBJ_WEAPONS:    zapsym = DCHAR_FIRED_WEAPON;  break;
    case OBJ_MISSILES:   zapsym = DCHAR_FIRED_MISSILE; break;
    case OBJ_ARMOUR:     zapsym = DCHAR_FIRED_ARMOUR;  break;
    case OBJ_WANDS:      zapsym = DCHAR_FIRED_STICK;   break;
    case OBJ_FOOD:       zapsym = DCHAR_FIRED_CHUNK;   break;
    case OBJ_SCROLLS:    zapsym = DCHAR_FIRED_SCROLL;  break;
    case OBJ_JEWELLERY:  zapsym = DCHAR_FIRED_TRINKET; break;
    case OBJ_POTIONS:    zapsym = DCHAR_FIRED_FLASK;   break;
    case OBJ_BOOKS:      zapsym = DCHAR_FIRED_BOOK;    break;
    case OBJ_RODS:
    case OBJ_STAVES:     zapsym = DCHAR_FIRED_STICK;   break;
    default: break;
    }

    beam.glyph = dchar_glyph(zapsym);
    beam.was_missile = true;

    item_def *launcher  = const_cast<actor*>(agent)->weapon(0);
    if (launcher && !item.launched_by(*launcher))
        launcher = NULL;

    int bow_brand = SPWPN_NORMAL;
    if (launcher != NULL)
        bow_brand = get_weapon_brand(*launcher);

    int ammo_brand = get_ammo_brand(item);

    // Launcher brand does not override ammunition except when elemental
    // opposites (which cancel).
    if (ammo_brand != SPMSL_NORMAL && bow_brand != SPWPN_NORMAL)
    {
        if (bow_brand == SPWPN_FLAME && ammo_brand == SPMSL_FROST ||
            bow_brand == SPWPN_FROST && ammo_brand == SPMSL_FLAME)
        {
            bow_brand = SPWPN_NORMAL;
            ammo_brand = SPMSL_NORMAL;
        }
        // Nessos gets to cheat.
        else if (agent->type != MONS_NESSOS)
            bow_brand = SPWPN_NORMAL;
    }

    if (is_launched(agent, launcher, item) == LRET_FUMBLED)
    {
        // We want players to actually carry blowguns and bows, not just rely
        // on high to-hit modifiers.  Rationalization: The poison/magic/
        // whatever is only applied to the tips.  -sorear

        bow_brand = SPWPN_NORMAL;
        ammo_brand = SPMSL_NORMAL;
    }

    returning = ammo_brand == SPMSL_RETURNING;

    if (agent->is_player())
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.beam_source   = NON_MONSTER;
        beam.smart_monster = true;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monster* mon = agent->as_monster();

        beam.attitude      = mons_attitude(mon);
        beam.beam_source   = mon->mindex();
        beam.smart_monster = (mons_intel(mon) >= I_NORMAL);
        beam.thrower       = KILL_MON_MISSILE;
    }

    beam.item         = &item;
    beam.effect_known = item_ident(item, ISFLAG_KNOW_TYPE);
    beam.source       = agent->pos();
    beam.colour       = item.colour;
    beam.flavour      = BEAM_MISSILE;
    beam.is_beam      = false;
    beam.aux_source.clear();

    beam.can_see_invis = agent->can_see_invisible();

    beam.name = item.name(DESC_PLAIN, false, false, false);
    ammo_name = item.name(DESC_PLAIN);

    const unrandart_entry* entry = launcher && is_unrandom_artefact(*launcher)
        ? get_unrand_entry(launcher->special) : NULL;

    if (entry && entry->launch)
    {
        setup_missile_type sm =
            entry->launch(launcher, &beam, &ammo_name,
                                     &returning);

        switch (sm)
        {
        case SM_CONTINUE:
            break;
        case SM_FINISHED:
            return false;
        case SM_CANCEL:
            return true;
        }
    }

    bool poisoned   = (bow_brand == SPWPN_VENOM
                        || ammo_brand == SPMSL_POISONED);

    const bool exploding    = (ammo_brand == SPMSL_EXPLODING);
    const bool penetrating  = (bow_brand  == SPWPN_PENETRATION
                                || ammo_brand == SPMSL_PENETRATION
                                || (Version::ReleaseType == VER_ALPHA
                                    && item.base_type == OBJ_MISSILES
                                    && item.sub_type == MI_LARGE_ROCK));
    const bool silver       = (ammo_brand == SPMSL_SILVER);
    const bool disperses    = (ammo_brand == SPMSL_DISPERSAL);
    const bool charged      = bow_brand  == SPWPN_ELECTROCUTION;
    const bool blessed      = bow_brand == SPWPN_HOLY_WRATH;

    const bool paralysis    = ammo_brand == SPMSL_PARALYSIS;
    const bool slow         = ammo_brand == SPMSL_SLOW;
    const bool sleep        = ammo_brand == SPMSL_SLEEP;
    const bool confusion    = ammo_brand == SPMSL_CONFUSION;
#if TAG_MAJOR_VERSION == 34
    const bool sickness     = ammo_brand == SPMSL_SICKNESS;
#endif
    const bool rage         = ammo_brand == SPMSL_FRENZY;
#if TAG_MAJOR_VERSION == 34
    const bool blinding     = ammo_brand == SPMSL_BLINDING;
#endif

    ASSERT(!exploding || !is_artefact(item));

    // Note that bow_brand is known since the bow is equipped.

    bool beam_changed = false;

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        // Chaos can't be poisoned, since that might conflict with
        // the random healing effect or overlap with the random
        // poisoning effect.
        poisoned = false;
        if (item.special == SPWPN_VENOM || item.special == SPMSL_CURARE)
            item.special = SPMSL_NORMAL;

        beam.effect_known = false;
        beam.effect_wanton = true;

        beam.flavour = BEAM_CHAOS;
        if (ammo_brand != SPMSL_CHAOS)
        {
            beam.name    += " of chaos";
            ammo_name    += " of chaos";
        }
        else
            beam_changed = true;
        beam.colour  = ETC_RANDOM;
    }
    else if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
             && ammo_brand != SPMSL_FROST && bow_brand != SPWPN_FROST)
    {
        beam.flavour = BEAM_FIRE;
        if (ammo_brand != SPMSL_FLAME)
        {
            beam.name    += " of flame";
            ammo_name    += " of flame";
        }
        else
            beam_changed = true;

        beam.colour  = RED;
    }
    else if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_FROST)
             && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
    {
        beam.flavour = BEAM_COLD;
        if (ammo_brand != SPMSL_FROST)
        {
            beam.name    += " of frost";
            ammo_name   += " of frost";
        }
        else
            beam_changed = true;
        beam.colour  = WHITE;
    }

    if (beam_changed)
        beam.name = item.name(DESC_PLAIN, false, false, false);

    ASSERT(beam.flavour == BEAM_MISSILE || !is_artefact(item));

    if (silver)
        beam.damage_funcs.push_back(_silver_damages_victim);
    if (poisoned)
        beam.hit_funcs.push_back(_poison_hit_victim);
    if (penetrating)
    {
        beam.range_funcs.push_back(_item_penetrates_victim);
        if (item.base_type == OBJ_MISSILES && item.sub_type == MI_LARGE_ROCK)
            beam.hit_verb = "crashes through";
        else
            beam.hit_verb = "pierces through";
    }
    if (disperses)
        beam.hit_funcs.push_back(_dispersal_hit_victim);
    if (charged)
        beam.damage_funcs.push_back(_charged_damages_victim);
    if (blessed)
        beam.damage_funcs.push_back(_blessed_damages_victim);

    // New needle brands have no effect when thrown without launcher.
    if (launcher != NULL)
    {
        if (paralysis)
            beam.hit_funcs.push_back(_paralysis_hit_victim);
        if (slow)
            beam.hit_funcs.push_back(_slow_hit_victim);
        if (sleep)
            beam.hit_funcs.push_back(_sleep_hit_victim);
        if (confusion)
            beam.hit_funcs.push_back(_confusion_hit_victim);
#if TAG_MAJOR_VERSION == 34
        if (sickness)
            beam.hit_funcs.push_back(_sickness_hit_victim);
#endif
        if (rage)
            beam.hit_funcs.push_back(_rage_hit_victim);
    }

#if TAG_MAJOR_VERSION == 34
    if (blinding)
    {
        beam.hit_verb = "blinds";
        beam.hit_funcs.push_back(_blind_hit_victim);
    }
#endif

    if (disperses && item.special != SPMSL_DISPERSAL)
    {
        beam.name = "dispersing " + beam.name;
        ammo_name = "dispersing " + ammo_name;
    }

    // XXX: This doesn't make sense, but it works.
    if (poisoned && item.special != SPMSL_POISONED)
    {
        beam.name = "poisoned " + beam.name;
        ammo_name = "poisoned " + ammo_name;
    }

    if (penetrating && item.special != SPMSL_PENETRATION
        && item.sub_type != MI_LARGE_ROCK)
    {
        beam.name = "penetrating " + beam.name;
        ammo_name = "penetrating " + ammo_name;
    }

    if (silver && item.special != SPMSL_SILVER)
    {
        beam.name = "silvery " + beam.name;
        ammo_name = "silvery " + ammo_name;
    }

    if (blessed)
    {
        beam.name = "blessed " + beam.name;
        ammo_name = "blessed " + ammo_name;
    }

    // Do this here so that we get all the name mods except for a
    // redundant "exploding".
    if (exploding)
    {
        bolt *expl = new bolt(beam);

        expl->is_explosion = true;
        expl->damage       = dice_def(2, 5);
        expl->ex_size      = 1;

        if (beam.flavour == BEAM_MISSILE)
        {
            expl->flavour = BEAM_FRAG;
            expl->name   += " fragments";

            const string short_name =
                item.name(DESC_BASENAME, true, false, false, false,
                          ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK
                          | ISFLAG_RACIAL_MASK);

            expl->name = replace_all(expl->name, item.name(DESC_PLAIN),
                                     short_name);
        }
        expl->name = "explosion of " + expl->name;

        beam.special_explosion = expl;
    }

    if (exploding && item.special != SPMSL_EXPLODING)
    {
        beam.name = "exploding " + beam.name;
        ammo_name = "exploding " + ammo_name;
    }

    if (beam.flavour != BEAM_MISSILE)
    {
        returning = false;

        beam.glyph = dchar_glyph(DCHAR_FIRED_BOLT);
    }

    if (!is_artefact(item))
        ammo_name = article_a(ammo_name, true);
    else
        ammo_name = "the " + ammo_name;

    return false;
}

static int stat_adjust(int value, int stat, int statbase,
                       const int maxmult = 160, const int minmult = 40)
{
    int multiplier = (statbase + (stat - statbase) / 2) * 100 / statbase;
    if (multiplier > maxmult)
        multiplier = maxmult;
    else if (multiplier < minmult)
        multiplier = minmult;

    if (multiplier > 100)
        value = value * (100 + random2avg(multiplier - 100, 2)) / 100;
    else if (multiplier < 100)
        value = value * (100 - random2avg(100 - multiplier, 2)) / 100;

    return value;
}

static int str_adjust_thrown_damage(int dam)
{
    return stat_adjust(dam, you.strength(), 15, 160, 90);
}

static int dex_adjust_thrown_tohit(int hit)
{
    return stat_adjust(hit, you.dex(), 13, 160, 90);
}

static void _throw_noise(actor* act, const bolt &pbolt, const item_def &ammo)
{
    const item_def* launcher = act->weapon();

    if (launcher == NULL || launcher->base_type != OBJ_WEAPONS)
        return;

    if (is_launched(act, launcher, ammo) != LRET_LAUNCHED)
        return;

    // Throwing and blowguns are silent...
    int         level = 0;
    const char* msg   = NULL;

    switch (launcher->sub_type)
    {
    case WPN_BLOWGUN:
        return;

    case WPN_SLING:
        level = 1;
        msg   = "You hear a whirring sound.";
        break;
     case WPN_BOW:
        level = 5;
        msg   = "You hear a twanging sound.";
        break;
     case WPN_LONGBOW:
        level = 6;
        msg   = "You hear a loud twanging sound.";
        break;
     case WPN_CROSSBOW:
        level = 7;
        msg   = "You hear a thunk.";
        break;

    default:
        die("Invalid launcher '%s'",
                 launcher->name(DESC_PLAIN).c_str());
        return;
    }
    if (act->is_player() || you.can_see(act))
        msg = NULL;

    noisy(level, act->pos(), msg, act->mindex());
}

// throw_it - currently handles player throwing only.  Monster
// throwing is handled in mon-act:_mons_throw()
// Note: If teleport is true, assume that pbolt is already set up,
// and teleport the projectile onto the square.
//
// Return value is only relevant if dummy_target is non-NULL, and returns
// true if dummy_target is hit.
bool throw_it(bolt &pbolt, int throw_2, bool teleport, int acc_bonus,
              dist *target)
{
    dist thr;
    int shoot_skill = 0;

    int baseHit      = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;  // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;  // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0;    // 'extra' bonus from skill/dex/str
    int effSkill     = 0;        // effective launcher skill
    int dice_mult    = 100;
    bool returning   = false;    // Item can return to pack.
    bool did_return  = false;    // Returning item actually does return to pack.
    int slayDam      = 0;
    bool speed_brand = false;

    if (you.confused())
    {
        thr.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
        thr.isValid = true;
    }
    else if (target)
        thr = *target;
    else if (pbolt.target.zero())
    {
        direction_chooser_args args;
        args.mode = TARG_HOSTILE;
        direction(thr, args);

        if (!thr.isValid)
        {
            if (thr.isCancel)
                canned_msg(MSG_OK);

            return false;
        }
    }
    pbolt.set_target(thr);

    item_def& thrown = you.inv[throw_2];
    ASSERT(thrown.defined());

    // Figure out if we're thrown or launched.
    const launch_retval projected = is_launched(&you, you.weapon(), thrown);

    // Making a copy of the item: changed only for venom launchers.
    item_def item = thrown;
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);

    // Items that get a temporary brand from a player spell lose the
    // brand as soon as the player lets go of the item.  Can't call
    // unwield_item() yet since the beam might get cancelled.
    if (you.duration[DUR_WEAPON_BRAND] && projected != LRET_LAUNCHED
        && throw_2 == you.equip[EQ_WEAPON])
    {
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
    }

    string ammo_name;

    if (_setup_missile_beam(&you, pbolt, item, ammo_name, returning))
    {
        you.turn_is_over = false;
        return false;
    }

    // Did we know the ammo's brand before throwing it?
    const bool ammo_brand_known = item_type_known(thrown);

    // Get the ammo/weapon type.  Convenience.
    const object_class_type wepClass = thrown.base_type;
    const int               wepType  = thrown.sub_type;

    // Determine range.
    int max_range = 0;
    int range = 0;

    if (projected)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            range     = 1 + random2(you.strength() / 5);
            max_range = you.strength() / 5;
            if (you.can_throw_large_rocks())
            {
                range     += random_range(4, 7);
                max_range += 7;
            }
        }
        else if (wepType == MI_THROWING_NET)
            max_range = range = 2 + you.body_size(PSIZE_BODY);
        else
            max_range = range = you.current_vision;
    }
    else
    {
        // Range based on mass & strength, between 1 and 9.
        max_range = range = max(you.strength()-item_mass(thrown)/10 + 3, 1);
    }

    range = min(range, (int)you.current_vision);
    max_range = min(max_range, (int)you.current_vision);

    // For the tracer, use max_range. For the actual shot, use range.
    pbolt.range = max_range;

    // Save the special explosion (exploding missiles) for later.
    // Need to clear this if unknown to avoid giving away the explosion.
    bolt* expl = pbolt.special_explosion;
    if (!pbolt.effect_known)
        pbolt.special_explosion = NULL;

    // Don't do the tracing when using Portaled Projectile, or when confused.
    if (!teleport && !you.confused())
    {
        // Set values absurdly high to make sure the tracer will
        // complain if we're attempting to fire through allies.
        pbolt.hit    = 100;
        pbolt.damage = dice_def(1, 100);

        // Init tracer variables.
        pbolt.foe_info.reset();
        pbolt.friend_info.reset();
        pbolt.foe_ratio = 100;
        pbolt.is_tracer = true;

        pbolt.fire();

        // Should only happen if the player answered 'n' to one of those
        // "Fire through friendly?" prompts.
        if (pbolt.beam_cancelled)
        {
            canned_msg(MSG_OK);
            you.turn_is_over = false;
            if (pbolt.special_explosion != NULL)
                delete pbolt.special_explosion;
            return false;
        }
        pbolt.hit    = 0;
        pbolt.damage = dice_def();
    }
    pbolt.is_tracer = false;

    // Reset values.
    pbolt.range = range;
    pbolt.special_explosion = expl;

    bool unwielded = false;
    if (throw_2 == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        if (!wield_weapon(true, SLOT_BARE_HANDS, true, false, false))
            return false;

        unwielded = true;
    }

    // Now start real firing!
    origin_set_unknown(item);

    if (is_blood_potion(item) && thrown.quantity > 1)
    {
        // Initialise thrown potion with oldest potion in stack.
        int val = remove_oldest_blood_potion(thrown);
        val -= you.num_turns;
        item.props.clear();
        init_stack_blood_potions(item, val);
    }

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if (!teleport)
        pbolt.set_target(thr);

    // baseHit and damage for generic objects
    baseHit = min(0, you.strength() - item_mass(item) / 10);
    baseDam = item_mass(item) / 100;

    if (wepClass == OBJ_MISSILES)
    {
        skill_type sk = SK_THROWING;
        if (projected == LRET_LAUNCHED)
            sk = range_skill(*you.weapon());
        ammoHitBonus = ammoDamBonus = min(3, you.skill_rdiv(sk, 1, 3));
    }

    int bow_brand = SPWPN_NORMAL;

    if (projected == LRET_LAUNCHED)
        bow_brand = get_weapon_brand(*you.weapon());

    int ammo_brand = get_ammo_brand(item);

    if (projected == LRET_FUMBLED)
    {
        // See comment in setup_missile_beam.  Why is this duplicated?
        ammo_brand = SPMSL_NORMAL;
    }

    // CALCULATIONS FOR LAUNCHED MISSILES
    if (projected == LRET_LAUNCHED)
    {
        const item_def &launcher = *you.weapon();

        // Extract launcher bonuses due to magic.
        lnchHitBonus = launcher.plus;
        lnchDamBonus = launcher.plus2;

        const int item_base_dam = property(item, PWPN_DAMAGE);
        const int lnch_base_dam = property(launcher, PWPN_DAMAGE);

        const skill_type launcher_skill = range_skill(launcher);

        baseHit = property(launcher, PWPN_HIT);
        baseDam = lnch_base_dam + random2(1 + item_base_dam);

        // Slings are terribly weakened otherwise.
        if (lnch_base_dam == 0)
            baseDam = item_base_dam;

        // If we've a zero base damage + an elemental brand, up the damage
        // slightly so the brand has something to work with. This should
        // only apply to needles.
        if (!baseDam && _elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to consider
        // this beam "needle-like". (XXX)
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        dprf("Base hit == %d; Base damage == %d "
                "(item %d + launcher %d)",
                        baseHit, baseDam,
                        item_base_dam, lnch_base_dam);

        // elves with elven bows
        if (get_equip_race(*you.weapon()) == ISFLAG_ELVEN
            && player_genus(GENPC_ELVEN))
        {
            baseHit++;
        }

        // Lower accuracy if held in a net.
        if (you.attribute[ATTR_HELD])
            baseHit = baseHit / 2 - 1;

        // For all launched weapons, maximum effective specific skill
        // is twice throwing skill.  This models the fact that no matter
        // how 'good' you are with a bow, if you know nothing about
        // trajectories you're going to be a damn poor bowman.  Ditto
        // for crossbows and slings.

        // [dshaligram] Throwing now two parts launcher skill, one part
        // ranged combat. Removed the old model which is... silly.

        // [jpeg] Throwing now only affects actual throwing weapons,
        // i.e. not launched ones. (Sep 10, 2007)

        shoot_skill = you.skill_rdiv(launcher_skill);
        effSkill    = shoot_skill;

        const int speed = launcher_final_speed(launcher, you.shield());
        dprf("Final launcher speed: %d", speed);
        you.time_taken = div_rand_round(speed * you.time_taken, 100);

        // [dshaligram] Improving missile weapons:
        //  - Remove the strength/enchantment cap where you need to be strong
        //    to exploit a launcher bonus.
        //  - Add on launcher and missile pluses to extra damage.

        // [dshaligram] This can get large...
        exDamBonus = lnchDamBonus + random2(1 + ammoDamBonus);
        exDamBonus = (exDamBonus > 0   ? random2(exDamBonus + 1)
                                       : -random2(-exDamBonus + 1));
        exHitBonus = (lnchHitBonus > 0 ? random2(lnchHitBonus + 1)
                                       : -random2(-lnchHitBonus + 1));

        practise(EX_WILL_LAUNCH, launcher_skill);
        if (is_unrandom_artefact(launcher)
            && get_unrand_entry(launcher.special)->type_name)
        {
            count_action(CACT_FIRE, launcher.special);
        }
        else
            count_action(CACT_FIRE, launcher.sub_type);

        // Removed 2 random2(2)s from each of the learning curves, but
        // left slings because they're hard enough to develop without
        // a good source of shot in the dungeon.
        switch (launcher_skill)
        {
        case SK_SLINGS:
        {
            // Sling bullets are designed for slinging and easier to aim.
            if (wepType == MI_SLING_BULLET)
                baseHit += 4;

            exHitBonus += (effSkill * 3) / 2;

            // Strength is good if you're using a nice sling.
            int strbonus = (10 * (you.strength() - 10)) / 9;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // cap
            strbonus = min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;
            // Add skill for slings... helps to find those vulnerable spots.
            dice_mult = dice_mult * (14 + random2(1 + effSkill)) / 14;

            // Now kill the launcher damage bonus.
            lnchDamBonus = min(0, lnchDamBonus);
            break;
        }

        // Blowguns take a _very_ steady hand; a lot of the bonus
        // comes from dexterity.  (Dex bonus here as well as below.)
        case SK_THROWING:
            baseHit -= 2;
            exHitBonus += (effSkill * 3) / 2 + you.dex() / 2;

            // No extra damage for blowguns.
            // exDamBonus = 0;

            // Now kill the launcher damage and ammo bonuses.
            lnchDamBonus = min(0, lnchDamBonus);
            ammoDamBonus = min(0, ammoDamBonus);
            break;

        case SK_BOWS:
        {
            baseHit -= 3;
            exHitBonus += (effSkill * 2);

            // Strength is good if you're using a nice bow.
            int strbonus = (10 * (you.strength() - 10)) / 4;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // Cap; reduced this cap, because we don't want to allow
            // the extremely-strong to quadruple the enchantment bonus.
            strbonus = min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;

            // Add in skill for bows - helps you to find those vulnerable spots.
            // exDamBonus += effSkill;

            dice_mult = dice_mult * (17 + random2(1 + effSkill)) / 17;

            // Now kill the launcher damage bonus.
            lnchDamBonus = min(0, lnchDamBonus);
            break;
        }
            // Crossbows are easy for unskilled people.

        case SK_CROSSBOWS:
            baseHit++;
            exHitBonus += (3 * effSkill) / 2 + 6;
            // exDamBonus += effSkill * 2 / 3 + 4;

            dice_mult = dice_mult * (22 + random2(1 + effSkill)) / 22;

        default:
            break;
        }

        if (bow_brand == SPWPN_VORPAL)
        {
            // Vorpal brand adds 20% damage bonus. Decreased from 30% to
            // keep it more comparable with speed brand after the speed nerf.
            dice_mult = dice_mult * 120 / 100;
        }

        // Note that branded missile damage goes through defender
        // resists.
        if (ammo_brand == SPMSL_STEEL)
            dice_mult = dice_mult * 130 / 100;

        if (_elemental_missile_beam(bow_brand, ammo_brand))
            dice_mult = dice_mult * 140 / 100;

        if (get_weapon_brand(launcher) == SPWPN_SPEED)
            speed_brand = true;
    }

    // check for returning ammo from launchers
    if (returning && projected == LRET_LAUNCHED)
    {
        if (!x_chance_in_y(1, 1 + skill_bump(range_skill(*you.weapon()))))
            did_return = true;
    }

    // CALCULATIONS FOR THROWN MISSILES
    if (projected == LRET_THROWN)
    {
        returning = returning && !teleport;

        if (returning && !x_chance_in_y(1, 1 + skill_bump(SK_THROWING)))
            did_return = true;

        baseHit = 0;

        ASSERT(wepClass == OBJ_MISSILES);
        if (wepType == MI_STONE || wepType == MI_LARGE_ROCK
            || wepType == MI_DART || wepType == MI_JAVELIN
            || wepType == MI_TOMAHAWK)
        {
            // Elves with elven weapons.
            if (get_equip_race(item) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
            {
                baseHit++;
            }

            // Give an appropriate 'tohit':
            // * large rocks, stones and throwing nets are 0
            // * javelins are +1
            // * darts are +2
            switch (wepType)
            {
                case MI_DART:
                    baseHit += 2;
                    break;
                case MI_JAVELIN:
                    baseHit++;
                    break;
                default:
                    break;
            }

            exHitBonus = you.skill(SK_THROWING, 2);

            baseDam = property(item, PWPN_DAMAGE);

            // [dshaligram] The defined base damage applies only when used
            // for launchers. Hand-thrown stones do only half
            // base damage. Yet another evil 4.0ism.
            if (wepType == MI_STONE)
                baseDam = div_rand_round(baseDam, 2);

            exDamBonus = (you.skill(SK_THROWING, 5) + you.strength() * 10 - 100)
                       / 12;

            // Now, exDamBonus is a multiplier.  The full multiplier
            // is applied to base damage, but only a third is applied
            // to the magical modifier.
            exDamBonus = (exDamBonus * (3 * baseDam + ammoDamBonus)) / 30;
        }

        switch (wepType)
        {
        case MI_LARGE_ROCK:
            if (you.can_throw_large_rocks())
                baseHit = 1;
            break;

        case MI_DART:
        case MI_TOMAHAWK:
            // Darts use throwing skill.
            exHitBonus += skill_bump(SK_THROWING);
            exDamBonus += you.skill(SK_THROWING, 3) / 5;
            break;

        case MI_JAVELIN:
            // Javelins use throwing skill.
            exHitBonus += skill_bump(SK_THROWING);
            exDamBonus += you.skill(SK_THROWING, 3) / 5;

            // Adjust for strength and dex.
            exDamBonus = str_adjust_thrown_damage(exDamBonus);
            exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

            // High dex helps damage a bit, too (aim for weak spots).
            exDamBonus = stat_adjust(exDamBonus, you.dex(), 20, 150, 100);
            break;

        case MI_THROWING_NET:
            // Nets use throwing skill. They don't do any damage!
            baseDam = 0;
            exDamBonus = 0;
            ammoDamBonus = 0;

            // ...but accuracy is important for them.
            baseHit = 1;
            exHitBonus += skill_bump(SK_THROWING, 7) / 2;
            // Adjust for strength and dex.
            exHitBonus = dex_adjust_thrown_tohit(exHitBonus);
            break;
        }

        if (ammo_brand == SPMSL_STEEL)
            dice_mult = dice_mult * 130 / 100;

        practise(EX_WILL_THROW_MSL, wepType);
        count_action(CACT_THROW, wepType | (OBJ_MISSILES << 16));

        you.time_taken = finesse_adjust_delay(you.time_taken);
    }

    // Dexterity bonus, and possible skill increase for silly throwing.
    if (projected)
    {
        if (wepType != MI_LARGE_ROCK && wepType != MI_THROWING_NET)
        {
            exHitBonus += you.dex() / 2;

            // slaying bonuses
            if (wepType != MI_NEEDLE)
            {
                slayDam = slaying_bonus(PWPN_DAMAGE, true);
                slayDam = (slayDam < 0 ? -random2(1 - slayDam)
                                       :  random2(1 + slayDam));
            }

            exHitBonus += slaying_bonus(PWPN_HIT, true);
        }
    }
    else // LRET_FUMBLED
    {
        practise(EX_WILL_THROW_OTHER);

        exHitBonus = you.dex() / 4;
    }

    // FINALISE tohit and damage
    if (exHitBonus >= 0)
        pbolt.hit = baseHit + random2avg(exHitBonus + 1, 2);
    else
        pbolt.hit = baseHit - random2avg(0 - (exHitBonus - 1), 2);

    if (exDamBonus >= 0)
        pbolt.damage = dice_def(1, baseDam + random2(exDamBonus + 1));
    else
        pbolt.damage = dice_def(1, baseDam - random2(0 - (exDamBonus - 1)));

    pbolt.damage.size  = dice_mult * pbolt.damage.size / 100;
    pbolt.damage.size += slayDam;

    // Only add bonuses if we're throwing something sensible.
    if (projected || wepClass == OBJ_WEAPONS)
    {
        pbolt.hit += ammoHitBonus + lnchHitBonus;
        pbolt.damage.size += ammoDamBonus + lnchDamBonus;
    }

    if (speed_brand)
        pbolt.damage.size = div_rand_round(pbolt.damage.size * 9, 10);

    // Add in bonus (only from Portal Projectile for now).
    if (acc_bonus != DEBUG_COOKIE)
        pbolt.hit += acc_bonus;

    if (you.inaccuracy())
        pbolt.hit -= 5;

    scale_dice(pbolt.damage);

    dprf("H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size);

    // Create message.
    mprf("%s %s%s %s.",
          teleport  ? "Magically, you" : "You",
          projected ? "" : "awkwardly ",
          projected == LRET_LAUNCHED ? "shoot" : "throw",
          ammo_name.c_str());

    // Ensure we're firing a 'missile'-type beam.
    pbolt.is_beam   = false;
    pbolt.is_tracer = false;

    pbolt.loudness = int(sqrt(item_mass(item))/3 + 0.5);

    // Mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    bool hit = false;
    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        pbolt.use_target_as_pos = true;
        pbolt.affect_cell();
        pbolt.affect_endpoint();
        if (!did_return && acc_bonus != DEBUG_COOKIE)
            pbolt.drop_object();
    }
    else
    {
        if (crawl_state.game_is_hints())
            Hints.hints_throw_counter++;

        // Dropping item copy, since the launched item might be different.
        pbolt.drop_item = !did_return;
        pbolt.fire();

        hit = !pbolt.hit_verb.empty();

        // The item can be destroyed before returning.
        if (did_return && thrown_object_destroyed(&item, pbolt.target))
            did_return = false;
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        did_god_conduct(DID_CHAOS, 2 + random2(3),
                        bow_brand == SPWPN_CHAOS || ammo_brand_known);
    }

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_FRENZY)
        did_god_conduct(DID_HASTY, 6 + random2(3), ammo_brand_known);

    if (did_return)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow();
        pbolt.fire();

        msg::stream << item.name(DESC_THE) << " returns to your pack!"
                    << endl;

        // Player saw the item return.
        if (!is_artefact(you.inv[throw_2]))
            set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_TYPE);
    }
    else
    {
        // Should have returned but didn't.
        if (returning && item_type_known(you.inv[throw_2]))
        {
            msg::stream << item.name(DESC_THE)
                        << " fails to return to your pack!" << endl;
        }
        dec_inv_item_quantity(throw_2, 1);
        if (unwielded)
            canned_msg(MSG_EMPTY_HANDED_NOW);
    }

    _throw_noise(&you, pbolt, thrown);

    // ...any monster nearby can see that something has been thrown, even
    // if it didn't make any noise.
    alert_nearby_monsters();

    you.turn_is_over = true;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return hit;
}

void setup_monster_throw_beam(monster* mons, bolt &beam)
{
    // FIXME we should use a sensible range here
    beam.range = you.current_vision;
    beam.beam_source = mons->mindex();

    beam.glyph   = dchar_glyph(DCHAR_FIRED_MISSILE);
    beam.flavour = BEAM_MISSILE;
    beam.thrower = KILL_MON_MISSILE;
    beam.aux_source.clear();
    beam.is_beam = false;
}

// msl is the item index of the thrown missile (or weapon).
bool mons_throw(monster* mons, bolt &beam, int msl)
{
    string ammo_name;

    bool returning = false;
    bool speed_brand = false;

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int lnchBaseDam  = 0;

    int hitMult = 0;
    int damMult  = 0;
    int diceMult = 100;

    // Some initial convenience & initializations.
    ASSERT(mitm[msl].base_type == OBJ_MISSILES);
    const int wepType   = mitm[msl].sub_type;

    const int weapon    = mons->inv[MSLOT_WEAPON];
    const int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type : 0;

    mon_inv_type slot = get_mon_equip_slot(mons, mitm[msl]);
    ASSERT(slot != NUM_MONSTER_SLOTS);

    mons->lose_energy(EUT_MISSILE);
    const int throw_energy = mons->action_energy(EUT_MISSILE);

    // Dropping item copy, since the launched item might be different.
    item_def item = mitm[msl];
    item.quantity = 1;
    if (mons->friendly())
        item.flags |= ISFLAG_DROPPED_BY_ALLY;

    // FIXME we should actually determine a sensible range here
    beam.range         = you.current_vision;

    if (_setup_missile_beam(mons, beam, item, ammo_name, returning))
        return false;

    beam.aimed_at_spot = returning;

    const launch_retval projected =
        is_launched(mons, mons->mslot_item(MSLOT_WEAPON),
                    mitm[msl]);

    // extract launcher bonuses due to magic
    if (projected == LRET_LAUNCHED)
    {
        lnchHitBonus = mitm[weapon].plus;
        lnchDamBonus = mitm[weapon].plus2;
        lnchBaseDam  = property(mitm[weapon], PWPN_DAMAGE);
    }

    // FIXME: ammo enchantment
    ammoHitBonus = ammoDamBonus = min(3, div_rand_round(mons->hit_dice , 3));

    // Archers get a boost from their melee attack.
    if (mons->is_archer())
    {
        const mon_attack_def attk = mons_attack_spec(mons, 0);
        if (attk.type == AT_SHOOT)
        {
            if (projected == LRET_THROWN)
                ammoHitBonus += random2avg(attk.damage, 2);
            else
                ammoDamBonus += random2avg(attk.damage, 2);
        }
    }

    if (projected == LRET_THROWN)
    {
        // Darts are easy.
        if (wepType == MI_DART)
        {
            baseHit = 11;
            hitMult = 40;
            damMult = 25;
        }
        else
        {
            baseHit = 6;
            hitMult = 30;
            damMult = 25;
        }

        baseDam = property(item, PWPN_DAMAGE);

        // [dshaligram] Thrown stones/darts do only half the damage of
        // launched stones/darts. This matches 4.0 behaviour.
        if (wepType == MI_DART || wepType == MI_STONE
            || wepType == MI_SLING_BULLET)
        {
            baseDam = div_rand_round(baseDam, 2);
        }

        // give monster "skill" bonuses based on HD
        exHitBonus = (hitMult * mons->hit_dice) / 10 + 1;
        exDamBonus = (damMult * mons->hit_dice) / 10 + 1;
    }

    // Monsters no longer gain unfair advantages with weapons of
    // fire/ice and incorrect ammo.  They now have the same restrictions
    // as players.

    const int  ammo_brand = get_ammo_brand(item);

    if (projected == LRET_LAUNCHED)
    {
        int bow_brand = get_weapon_brand(mitm[weapon]);

        switch (lnchType)
        {
        case WPN_BLOWGUN:
            baseHit = 12;
            hitMult = 60;
            damMult = 0;
            lnchDamBonus = 0;
            break;
        case WPN_BOW:
        case WPN_LONGBOW:
            baseHit = 0;
            hitMult = 60;
            damMult = 35;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus = (lnchDamBonus + 1) / 2;
            break;
        case WPN_CROSSBOW:
            baseHit = 4;
            hitMult = 70;
            damMult = 30;
            break;
        case WPN_SLING:
            baseHit = 10;
            hitMult = 40;
            damMult = 20;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus /= 2;
            break;
        }

        // Launcher is now more important than ammo for base damage.
        baseDam = property(item, PWPN_DAMAGE);
        if (lnchBaseDam)
            baseDam = lnchBaseDam + random2(1 + baseDam);

        // missiles don't have pluses2;  use hit bonus
        ammoDamBonus = ammoHitBonus;

        exHitBonus = (hitMult * mons->hit_dice) / 10 + 1;
        exDamBonus = (damMult * mons->hit_dice) / 10 + 1;

        if (!baseDam && _elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to
        // consider this beam "needle-like".
        if (wepType == MI_NEEDLE)
            beam.ench_power = AUTOMATIC_HIT;

        // elf with elven launcher
        if (get_equip_race(mitm[weapon]) == ISFLAG_ELVEN
            && mons_genus(mons->type) == MONS_ELF)
        {
            beam.hit++;
        }

        // Vorpal brand increases damage dice size.
        if (bow_brand == SPWPN_VORPAL)
            diceMult = diceMult * 120 / 100;

        // As do steel ammo.
        if (ammo_brand == SPMSL_STEEL)
            diceMult = diceMult * 130 / 100;

        // Note: we already have throw_energy taken off.  -- bwr
        int speed_delta = 0;
        if (lnchType == WPN_CROSSBOW)
        {
            if (bow_brand == SPWPN_SPEED)
            {
                // Speed crossbows take 50% less time to use than
                // ordinary crossbows.
                speed_delta = div_rand_round(throw_energy * 2, 5);
                speed_brand = true;
            }
            else
            {
                // Ordinary crossbows take 20% more time to use
                // than ordinary bows.
                speed_delta = -div_rand_round(throw_energy, 5);
            }
        }
        else if (bow_brand == SPWPN_SPEED)
        {
            // Speed bows take 50% less time to use than
            // ordinary bows.
            speed_delta = div_rand_round(throw_energy, 2);
            speed_brand = true;
        }

        mons->speed_increment += speed_delta;
    }

    // Chaos, flame, and frost.
    if (beam.flavour != BEAM_MISSILE)
    {
        baseHit    += 2;
        exDamBonus += 6;
    }

    // monster intelligence bonus
    if (mons_intel(mons) == I_HIGH)
        exHitBonus += 10;

    // Identify before throwing, so we don't get different
    // messages for first and subsequent missiles.
    if (mons->observable())
    {
        if (projected == LRET_LAUNCHED
               && item_type_known(mitm[weapon])
            || projected == LRET_THROWN
               && mitm[msl].base_type == OBJ_MISSILES)
        {
            set_ident_flags(mitm[msl], ISFLAG_KNOW_TYPE);
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
        }
    }

    // Now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0.  Ah well.
    string msg = mons->name(DESC_THE);
    msg += ((projected == LRET_LAUNCHED) ? " shoots " : " throws ");

    if (!beam.name.empty() && projected == LRET_LAUNCHED)
        msg += article_a(beam.name);
    else
    {
        // build shoot message
        msg += item.name(DESC_A, false, false, false);

        // build beam name
        beam.name = item.name(DESC_PLAIN, false, false, false);
    }
    msg += ".";

    if (mons->observable())
    {
        mons->flags |= MF_SEEN_RANGED;
        mpr(msg.c_str());
    }

    _throw_noise(mons, beam, item);

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (projected == LRET_LAUNCHED)
    {
        beam.aux_source = make_stringf("Shot with a%s %s by %s",
                 (is_vowel(beam.name[0]) ? "n" : ""), beam.name.c_str(),
                 mons->name(DESC_A).c_str());
    }
    else
    {
        beam.aux_source = make_stringf("Hit by a%s %s thrown by %s",
                 (is_vowel(beam.name[0]) ? "n" : ""), beam.name.c_str(),
                 mons->name(DESC_A).c_str());
    }

    // Add everything up.
    beam.hit = baseHit + random2avg(exHitBonus, 2) + ammoHitBonus;
    beam.damage =
        dice_def(1, baseDam + random2avg(exDamBonus, 2) + ammoDamBonus);

    if (projected == LRET_LAUNCHED)
    {
        beam.damage.size += lnchDamBonus;
        beam.hit += lnchHitBonus;
    }
    beam.damage.size = diceMult * beam.damage.size / 100;

    int frenzy_degree = -1;

    if (mons->has_ench(ENCH_BATTLE_FRENZY))
        frenzy_degree = mons->get_ench(ENCH_BATTLE_FRENZY).degree;
    else if (mons->has_ench(ENCH_ROUSED))
        frenzy_degree = mons->get_ench(ENCH_ROUSED).degree;

    if (frenzy_degree != -1)
    {
#ifdef DEBUG_DIAGNOSTICS
        const dice_def orig_damage = beam.damage;
#endif

        beam.damage.size = beam.damage.size * (115 + frenzy_degree * 15) / 100;

        dprf("%s frenzy damage: %dd%d -> %dd%d",
             mons->name(DESC_PLAIN).c_str(),
             orig_damage.num, orig_damage.size,
             beam.damage.num, beam.damage.size);
    }

    // Skilled fighters get better to-hit and damage.
    if (mons->is_fighter())
    {
        beam.hit         = beam.hit * 120 / 100;
        beam.damage.size = beam.damage.size * 120 / 100;
    }

    if (mons->inaccuracy())
        beam.hit -= 5;

    if (mons->has_ench(ENCH_WIND_AIDED))
        beam.hit = beam.hit * 125 / 100;

    if (speed_brand)
        beam.damage.size = div_rand_round(beam.damage.size * 9, 10);

    scale_dice(beam.damage);

    // decrease inventory
    bool really_returns;
    if (returning && !one_chance_in(mons_power(mons->type) + 3))
        really_returns = true;
    else
        really_returns = false;

    beam.drop_item = !really_returns;

    // Redraw the screen before firing, in case the monster just
    // came into view and the screen hasn't been updated yet.
    viewwindow();
    beam.fire();

    // The item can be destroyed before returning.
    if (really_returns && thrown_object_destroyed(&item, beam.target))
        really_returns = false;

    if (really_returns)
    {
        // Fire beam in reverse.
        beam.setup_retrace();
        viewwindow();
        beam.fire();

        // Only print a message if you can see the target or the thrower.
        // Otherwise we get "The weapon returns whence it came from!" regardless.
        if (you.see_cell(beam.target) || you.can_see(mons))
        {
            msg::stream << "The weapon returns "
                        << (you.can_see(mons)?
                              ("to " + mons->name(DESC_THE))
                            : "from whence it came")
                        << "!" << endl;
        }

        // Player saw the item return.
        if (!is_artefact(item))
            set_ident_flags(mitm[msl], ISFLAG_KNOW_TYPE);
    }
    else if (dec_mitm_item_quantity(msl, 1))
        mons->inv[slot] = NON_ITEM;

    if (beam.special_explosion != NULL)
        delete beam.special_explosion;

    return true;
}

bool thrown_object_destroyed(item_def *item, const coord_def& where)
{
    ASSERT(item != NULL);

    string name = item->name(DESC_PLAIN, false, true, false);

    if (item->base_type != OBJ_MISSILES)
        return false;

    int brand = get_ammo_brand(*item);
    if (brand == SPMSL_CHAOS || brand == SPMSL_DISPERSAL || brand == SPMSL_EXPLODING)
        return true;

    // Nets don't get destroyed by throwing.
    if (item->sub_type == MI_THROWING_NET)
        return false;

    int chance;

    // [dshaligram] Removed influence of Throwing on ammo preservation.
    // The effect is nigh impossible to perceive.
    switch (item->sub_type)
    {
    case MI_NEEDLE:
        chance = (brand == SPMSL_CURARE ? 6 : 12);
        break;

    case MI_SLING_BULLET:
    case MI_STONE:
    case MI_ARROW:
    case MI_BOLT:
        chance = 8;
        break;

    case MI_DART:
        chance = 6;
        break;

    case MI_TOMAHAWK:
        chance = 30;
        break;

    case MI_JAVELIN:
        chance = 20;
        break;

    case MI_LARGE_ROCK:
        chance = 50;
        break;

    default:
        die("Unknown missile type");
    }

    // Inflate by 4 to avoid rounding errors.
    const int mult = 4;
    chance *= mult;

    if (brand == SPMSL_STEEL)
        chance *= 10;
    if (brand == SPMSL_FLAME)
        chance /= 2;
    if (brand == SPMSL_FROST)
        chance /= 2;

    return x_chance_in_y(mult, chance);
}

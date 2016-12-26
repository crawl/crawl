/**
 * @file
 * @brief Throwing and launching stuff.
**/

#include "AppHdr.h"

#include "throw.h"

#include <cmath>
#include <sstream>

#include "artefact.h"
#include "chardump.h"
#include "command.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "godabil.h"
#include "godconduct.h"
#include "godpassive.h" // passive_t::shadow_attacks
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item-use.h"
#include "macro.h"
#include "message.h"
#include "mon-behv.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static int  _fire_prompt_for_item();
static bool _fire_validate_item(int selected, string& err);

bool is_penetrating_attack(const actor& attacker, const item_def* weapon,
                           const item_def& projectile)
{
    return is_launched(&attacker, weapon, projectile) != LRET_FUMBLED
            && projectile.base_type == OBJ_MISSILES
            && get_ammo_brand(projectile) == SPMSL_PENETRATION
           || weapon
              && is_launched(&attacker, weapon, projectile) == LRET_LAUNCHED
              && get_weapon_brand(*weapon) == SPWPN_PENETRATION;
}

bool item_is_quivered(const item_def &item)
{
    return in_inventory(item) && item.link == you.m_quiver.get_fire_item();
}

int get_next_fire_item(int current, int direction)
{
    vector<int> fire_order;
    you.m_quiver.get_fire_order(fire_order, true);

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
        m_slot = you.m_quiver.get_fire_item(&m_noitem_reason);
        set_prompt();
    }

    // targeting_behaviour API
    virtual command_type get_command(int key = -1) override;
    virtual bool should_redraw() const override { return need_redraw; }
    virtual void clear_redraw()        override { need_redraw = false; }
    virtual void update_top_prompt(string* p_top_prompt) override;
    virtual vector<string> get_monster_desc(const monster_info& mi) override;

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
        return nullptr;
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
        case LRET_FUMBLED:  msg << "Tossing away "; break;
        case LRET_LAUNCHED: msg << "Firing ";             break;
        case LRET_THROWN:   msg << "Throwing ";           break;
        case LRET_BUGGY:    msg << "Bugging "; break;
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
        mpr(err);
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
            descs.emplace_back("chaotic");
        if (item->is_type(OBJ_MISSILES, MI_THROWING_NET)
            && (mi.body_size() >= SIZE_GIANT
                || mons_class_is_stationary(mi.type)
                || mons_class_flag(mi.type, M_INSUBSTANTIAL)))
        {
            descs.emplace_back("immune to nets");
        }
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
            mpr(warn);
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
    if (teleport && cell_is_solid(target.target))
    {
        const char *feat = feat_type_name(grd(target.target));
        mprf("There is %s there.", article_a(feat).c_str());
        return false;
    }

    you.m_quiver.on_item_fired(*beh.active_item(), beh.chosen_ammo);
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
                                   OSEL_THROWABLE, OPER_FIRE);

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
    int item_slot;
    if (you.equip[EQ_WEAPON] == letter_to_index('a'))
        item_slot = letter_to_index('b');
    else if (you.equip[EQ_WEAPON] == letter_to_index('b'))
        item_slot = letter_to_index('a');
    else
        return false;

    const item_def& launcher = you.inv[item_slot];
    if (!is_range_weapon(launcher))
        return false;
    if (none_of(you.inv.begin(), you.inv.end(), [&launcher](const item_def& it)
                { return it.launched_by(launcher);}))
    {
        return false;
    }

    if (!wield_weapon(true, item_slot))
        return false;

    you.turn_is_over = true;
    //XXX Hacky. Should use a delay instead.
    macro_buf_add(command_to_key(CMD_FIRE));
    return true;
}

int get_ammo_to_shoot(int item, dist &target, bool teleport)
{
    if (fire_warn_if_impossible())
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return -1;
    }

    if (Options.auto_switch && you.m_quiver.get_fire_item() == -1
       && _autoswitch_to_ranged())
    {
        return -1;
    }

    if (!_fire_choose_item_and_target(item, target, teleport))
        return -1;

    string warn;
    if (!_fire_validate_item(item, warn))
    {
        mpr(warn);
        return -1;
    }
    return item;
}

// Portal Projectile requires MP per shot.
bool is_pproj_active()
{
    return !you.confused() && you.duration[DUR_PORTAL_PROJECTILE]
           && enough_mp(1, true, false);
}

// If item == -1, prompt the user.
// If item passed, it will be put into the quiver.
void fire_thing(int item)
{
    dist target;
    item = get_ammo_to_shoot(item, target, is_pproj_active());
    if (item == -1)
        return;

    if (check_warning_inscriptions(you.inv[item], OPER_FIRE)
        && (!you.weapon()
            || is_launched(&you, you.weapon(), you.inv[item]) != LRET_LAUNCHED
            || check_warning_inscriptions(*you.weapon(), OPER_FIRE)))
    {
        bolt beam;
        throw_it(beam, item, &target);
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
        mpr(warn);
        return;
    }

    bolt beam;
    throw_it(beam, slot);
}

static bool _setup_missile_beam(const actor *agent, bolt &beam, item_def &item,
                                string &ammo_name, bool &returning)
{
    const auto cglyph = get_item_glyph(item);
    beam.glyph  = cglyph.ch;
    beam.colour = cglyph.col;
    beam.was_missile = true;

    item_def *launcher  = agent->weapon(0);
    if (launcher && !item.launched_by(*launcher))
        launcher = nullptr;

    if (agent->is_player())
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monster* mon = agent->as_monster();

        beam.attitude      = mons_attitude(*mon);
        beam.thrower       = KILL_MON_MISSILE;
    }

    beam.range        = you.current_vision;
    beam.source_id    = agent->mid;
    beam.item         = &item;
    beam.source       = agent->pos();
    beam.flavour      = BEAM_MISSILE;
    beam.pierce       = is_penetrating_attack(*agent, launcher, item);
    beam.aux_source.clear();

    beam.name = item.name(DESC_PLAIN, false, false, false);
    ammo_name = item.name(DESC_PLAIN);

    const unrandart_entry* entry = launcher && is_unrandom_artefact(*launcher)
        ? get_unrand_entry(launcher->unrand_idx) : nullptr;

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

    returning = item.base_type == OBJ_MISSILES
                && get_ammo_brand(item) == SPMSL_RETURNING;

    if (item.base_type == OBJ_MISSILES
        && get_ammo_brand(item) == SPMSL_EXPLODING)
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
                          ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK);

            expl->name = replace_all(expl->name, item.name(DESC_PLAIN),
                                     short_name);
        }
        expl->name = "explosion of " + expl->name;

        beam.special_explosion = expl;
    }

    if (!is_artefact(item))
        ammo_name = article_a(ammo_name, true);
    else
        ammo_name = "the " + ammo_name;

    return false;
}

static void _throw_noise(actor* act, const bolt &pbolt, const item_def &ammo)
{
    ASSERT(act); // XXX: change to actor &act
    const item_def* launcher = act->weapon();

    if (launcher == nullptr || launcher->base_type != OBJ_WEAPONS)
        return;

    if (is_launched(act, launcher, ammo) != LRET_LAUNCHED)
        return;

    // Throwing and blowguns are silent...
    int         level = 0;
    const char* msg   = nullptr;

    switch (launcher->sub_type)
    {
    case WPN_BLOWGUN:
        return;

    case WPN_HUNTING_SLING:
        level = 1;
        msg   = "You hear a whirring sound.";
        break;
    case WPN_FUSTIBALUS:
        level = 3;
        msg   = "You hear a loud whirring sound.";
        break;
    case WPN_SHORTBOW:
        level = 5;
        msg   = "You hear a twanging sound.";
        break;
    case WPN_LONGBOW:
        level = 6;
        msg   = "You hear a loud twanging sound.";
        break;
    case WPN_HAND_CROSSBOW:
        level = 2;
        msg   = "You hear a quiet thunk.";
        break;
    case WPN_ARBALEST:
        level = 7;
        msg   = "You hear a thunk.";
        break;
    case WPN_TRIPLE_CROSSBOW:
        level = 9;
        msg   = "You hear a triplet of thunks.";
        break;

    default:
        die("Invalid launcher '%s'",
                 launcher->name(DESC_PLAIN).c_str());
        return;
    }
    if (act->is_player() || you.can_see(*act))
        msg = nullptr;

    noisy(level, act->pos(), msg, act->mid);
}

// throw_it - currently handles player throwing only. Monster
// throwing is handled in mon-act:_mons_throw()
// Note: If teleport is true, assume that pbolt is already set up,
// and teleport the projectile onto the square.
//
// Return value is only relevant if dummy_target is non-nullptr, and returns
// true if dummy_target is hit.
bool throw_it(bolt &pbolt, int throw_2, dist *target)
{
    dist thr;
    bool returning   = false;    // Item can return to pack.
    bool did_return  = false;    // Returning item actually does return to pack.
    const bool teleport = is_pproj_active();

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

    string ammo_name;

    if (_setup_missile_beam(&you, pbolt, item, ammo_name, returning))
    {
        you.turn_is_over = false;
        return false;
    }

    // Get the ammo/weapon type. Convenience.
    const object_class_type wepClass = thrown.base_type;
    const int               wepType  = thrown.sub_type;

    // Don't trace at all when confused.
    // Give the player a chance to be warned about helpless targets when using
    // Portaled Projectile, but obviously don't trace a path.
    bool cancelled = false;
    if (!you.confused())
    {
        // Kludgy. Ideally this would handled by the same code.
        // Perhaps some notion of a zero length bolt, with the source and
        // target both set to the target?
        if (teleport)
        {
            // This block is roughly equivalent to bolt::affect_cell for
            // normal projectiles.
            // FIXME: this does not handle exploding ammo!
            monster *m = monster_at(thr.target);
            if (m)
                cancelled = stop_attack_prompt(m, false, thr.target);
        }
        else
        {
            // Set values absurdly high to make sure the tracer will
            // complain if we're attempting to fire through allies.
            pbolt.damage = dice_def(1, 100);

            // Init tracer variables.
            pbolt.foe_info.reset();
            pbolt.friend_info.reset();
            pbolt.foe_ratio = 100;
            pbolt.is_tracer = true;

            pbolt.fire();

            cancelled = pbolt.beam_cancelled;

            pbolt.hit    = 0;
            pbolt.damage = dice_def();
        }
    }

    // Should only happen if the player answered 'n' to one of those
    // "Fire through friendly?" prompts.
    if (cancelled)
    {
        you.turn_is_over = false;
        return false;
    }

    pbolt.is_tracer = false;

    bool unwielded = false;
    if (throw_2 == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        if (!wield_weapon(true, SLOT_BARE_HANDS, true, false, false, true, false))
            return false;

        if (!thrown.quantity)
            return false; // destroyed when unequipped (fragile)

        unwielded = true;
    }

    // Now start real firing!
    origin_set_unknown(item);

    // bloodpots & chunks need special handling.
    if (thrown.quantity > 1 && is_perishable_stack(item))
    {
        // Initialise thrown item with oldest item in stack.
        const int rot_timer = remove_oldest_perishable_item(thrown)
                              - you.elapsed_time;
        item.props.clear();
        init_perishable_stack(item, rot_timer);
    }

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if (!teleport)
        pbolt.set_target(thr);

    const int bow_brand = (projected == LRET_LAUNCHED)
                          ? get_weapon_brand(*you.weapon())
                          : SPWPN_NORMAL;
    const int ammo_brand = get_ammo_brand(item);

    switch (projected)
    {
    case LRET_LAUNCHED:
    {
        const item_def *launcher = you.weapon();
        ASSERT(launcher);
        practise_launching(*launcher);
        if (is_unrandom_artefact(*launcher)
            && get_unrand_entry(launcher->unrand_idx)->type_name)
        {
            count_action(CACT_FIRE, launcher->unrand_idx);
        }
        else
            count_action(CACT_FIRE, launcher->sub_type);
        break;
    }
    case LRET_THROWN:
        practise_throwing((missile_type)wepType);
        count_action(CACT_THROW, wepType, OBJ_MISSILES);
        break;
    case LRET_FUMBLED:
        break;
    case LRET_BUGGY:
        dprf("Unknown launch type for weapon."); // should never happen :)
        break;
    }

    // check for returning ammo
    if (teleport)
        returning = false;

    if (returning && projected != LRET_FUMBLED)
    {
        const skill_type sk =
            projected == LRET_THROWN ? SK_THROWING
                                     : item_attack_skill(*you.weapon());
        if (!one_chance_in(1 + skill_bump(sk)))
            did_return = true;
    }

    you.time_taken = you.attack_delay(&item).roll();

    // Create message.
    mprf("You %s%s %s.",
          teleport ? "magically " : "",
          (projected == LRET_FUMBLED ? "toss away" :
           projected == LRET_LAUNCHED ? "shoot" : "throw"),
          ammo_name.c_str());

    // Ensure we're firing a 'missile'-type beam.
    pbolt.pierce    = false;
    pbolt.is_tracer = false;

    pbolt.loudness = item.base_type == OBJ_MISSILES
                   ? ammo_type_damage(item.sub_type) / 3
                   : 0; // Maybe not accurate, but reflects the damage.

    // Mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    pbolt.hit = teleport ? random2(you.attribute[ATTR_PORTAL_PROJECTILE] / 4)
                         : 0;

    bool hit = false;
    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        pbolt.use_target_as_pos = true;
        pbolt.affect_cell();
        pbolt.affect_endpoint();
        if (!did_return)
            pbolt.drop_object();
        // Costs 1 MP per shot.
        dec_mp(1);
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
        did_god_conduct(DID_CHAOS, 2 + random2(3), bow_brand == SPWPN_CHAOS);

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_FRENZY)
        did_god_conduct(DID_HASTY, 6 + random2(3), true);

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

    if (pbolt.special_explosion != nullptr)
        delete pbolt.special_explosion;

    if (!teleport
        && projected
        && will_have_passive(passive_t::shadow_attacks)
        && thrown.base_type == OBJ_MISSILES
        && thrown.sub_type != MI_NEEDLE)
    {
        dithmenos_shadow_throw(thr, item);
    }

    return hit;
}

void setup_monster_throw_beam(monster* mons, bolt &beam)
{
    beam.range = you.current_vision;
    beam.source_id = mons->mid;

    beam.glyph   = dchar_glyph(DCHAR_FIRED_MISSILE);
    beam.flavour = BEAM_MISSILE;
    beam.thrower = KILL_MON_MISSILE;
    beam.aux_source.clear();
    beam.pierce  = false;
}

// msl is the item index of the thrown missile (or weapon).
bool mons_throw(monster* mons, bolt &beam, int msl, bool teleport)
{
    string ammo_name;

    bool returning = false;

    // Some initial convenience & initializations.
    ASSERT(mitm[msl].base_type == OBJ_MISSILES);

    const int weapon    = mons->inv[MSLOT_WEAPON];

    mon_inv_type slot = get_mon_equip_slot(mons, mitm[msl]);
    ASSERT(slot != NUM_MONSTER_SLOTS);

    // Energy is already deducted for the spell cast, if using portal projectile
    // FIXME: should it use this delay and not the spell delay?
    if (!teleport)
    {
        const int energy = mons->action_energy(EUT_MISSILE);
        const int delay = mons->attack_delay(&mitm[msl]).roll();
        ASSERT(energy > 0);
        ASSERT(delay > 0);
        mons->speed_increment -= div_rand_round(energy * delay, 10);
    }

    // Dropping item copy, since the launched item might be different.
    item_def item = mitm[msl];
    item.quantity = 1;

    if (_setup_missile_beam(mons, beam, item, ammo_name, returning))
        return false;

    beam.aimed_at_spot |= returning;

    const launch_retval projected =
        is_launched(mons, mons->mslot_item(MSLOT_WEAPON),
                    mitm[msl]);

    if (projected == LRET_THROWN)
        returning = returning && !teleport;

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
    // stupid, it will have baseHit of 0 and damage of 0. Ah well.
    string msg = mons->name(DESC_THE);
    if (teleport)
        msg += " magically";
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
        mpr(msg);
    }

    _throw_noise(mons, beam, item);

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
    if (teleport)
    {
        beam.use_target_as_pos = true;
        beam.affect_cell();
        beam.affect_endpoint();
        if (!really_returns)
            beam.drop_object();
    }
    else
    {
        beam.fire();

        // The item can be destroyed before returning.
        if (really_returns && thrown_object_destroyed(&item, beam.target))
            really_returns = false;
    }

    if (really_returns)
    {
        // Fire beam in reverse.
        beam.setup_retrace();
        viewwindow();
        beam.fire();

        // Only print a message if you can see the target or the thrower.
        // Otherwise we get "The weapon returns whence it came from!" regardless.
        if (you.see_cell(beam.target) || you.can_see(*mons))
        {
            msg::stream << "The weapon returns "
                        << (you.can_see(*mons)?
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

    if (beam.special_explosion != nullptr)
        delete beam.special_explosion;

    return true;
}

bool thrown_object_destroyed(item_def *item, const coord_def& where)
{
    ASSERT(item != nullptr);

    if (item->base_type != OBJ_MISSILES)
        return false;

    if (ammo_always_destroyed(*item))
        return true;

    if (ammo_never_destroyed(*item))
        return false;

    const int base_chance = ammo_type_destroy_chance(item->sub_type);
    const int brand = get_ammo_brand(*item);

    // Inflate by 2 to avoid rounding errors.
    const int mult = 2;
    int chance = base_chance * mult;

    if (brand == SPMSL_CURARE)
        chance /= 2;

    dprf("mulch chance: %d in %d", mult, chance);

    return x_chance_in_y(mult, chance);
}

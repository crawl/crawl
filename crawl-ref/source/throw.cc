/**
 * @file
 * @brief Throwing and launching stuff.
**/

#include "AppHdr.h"

#include "throw.h"

#include <cmath>
#include <sstream>

#include "art-enum.h"
#include "artefact.h"
#include "chardump.h"
#include "command.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "god-conduct.h"
#include "god-passive.h" // passive_t::shadow_attacks
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "output.h"
#include "prompt.h"
#include "ranged-attack.h"
#include "religion.h"
#include "shout.h"
#include "showsymb.h"
#include "skills.h"
#include "sound.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static int  _fire_prompt_for_item();
static bool _fire_validate_item(int selected, string& err);
static int  _get_dart_chance(const int hd);

bool is_penetrating_attack(const actor& attacker, const item_def* weapon,
                           const item_def& projectile)
{
    return is_launched(&attacker, weapon, projectile) != launch_retval::FUMBLED
            && projectile.base_type == OBJ_MISSILES
            && projectile.sub_type == MI_JAVELIN
           || weapon
              && is_launched(&attacker, weapon, projectile) == launch_retval::LAUNCHED
              && (get_weapon_brand(*weapon) == SPWPN_PENETRATION
                  || is_unrandom_artefact(*weapon, UNRAND_STORM_BOW));
}

class fire_target_behaviour : public targeting_behaviour
{
public:
    fire_target_behaviour(quiver::action &a)
        : action(a),
          need_redraw(false)
    {
        set_prompt();
        need_redraw = false; // XX simplify
        if (!targeted()
            || is_pproj_active() && action.affected_by_pproj())
        {
            needs_path = MB_FALSE;
        }
    }

    // targeting_behaviour API
    virtual bool should_redraw() const override { return need_redraw; }
    virtual void clear_redraw()        override { need_redraw = false; }
    virtual void update_top_prompt(string* p_top_prompt) override;
    virtual vector<string> get_monster_desc(const monster_info& mi) override;

    bool targeted() override;

    string get_error() override
    {
        if (you.confused())
            return "You are confused and cannot control your aim.";
        else
            return targeting_behaviour::get_error();
    }

public:
    item_def* active_item();

private:
    void display_help();
    void set_prompt();

    quiver::action &action;
    string prompt;
    string internal_prompt;
    bool need_redraw;
};

// This function handles a default "just looking" targeter for untargeted
// spells/actions that don't have something custom
// Maybe should be a method of action, it's in throw.cc for historical reasons;
// could be moved out of here if fire_target_behaviour is exposed.
void untargeted_fire(quiver::action &a)
{
    fire_target_behaviour beh(a);

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.behaviour = &beh;
    args.default_place = you.pos();

    direction(a.target, args);
}

void fire_target_behaviour::update_top_prompt(string* p_top_prompt)
{
    *p_top_prompt = internal_prompt;
}

item_def* fire_target_behaviour::active_item()
{
    const int slot = action.get_item();
    if (slot == -1)
        return nullptr;
    else
        return &you.inv[slot];
}

bool fire_target_behaviour::targeted()
{
    return action.is_targeted();
}

void fire_target_behaviour::set_prompt()
{
    string old_prompt = internal_prompt; // Keep for comparison at the end.
    internal_prompt.clear();

    ostringstream msg;


    if (action == quiver::action())
        internal_prompt = "No action selected";
    else
    {
        // TODO: might be nice to use colors here, but there's a wonky interaction
        // with the direction targeter's colors that would need to be fixed
        internal_prompt = action.quiver_description().tostring();

        if (!targeted())
            internal_prompt = string("Non-targeted ") + lowercase_first(internal_prompt);
    }

    // Write it out.
    internal_prompt += msg.str();

    // Never unset need_redraw here, because we might have cleared the
    // screen or something else which demands a redraw.
    if (internal_prompt != old_prompt)
        need_redraw = true;
}

void fire_target_behaviour::display_help()
{
    show_targeting_help();
    redraw_screen();
    update_screen();
    need_redraw = true;
    set_prompt();
}

vector<string> fire_target_behaviour::get_monster_desc(const monster_info& mi)
{
    vector<string> descs;
    item_def* item = active_item();
    if (!item || !targeted() || item->base_type != OBJ_MISSILES)
        return descs;

    ranged_attack attk(&you, nullptr, item, is_pproj_active());
    descs.emplace_back(make_stringf("%d%% to hit", to_hit_pct(mi, attk, false)));

    if (get_ammo_brand(*item) == SPMSL_SILVER && mi.is(MB_CHAOTIC))
        descs.emplace_back("chaotic");
    if (item->is_type(OBJ_MISSILES, MI_THROWING_NET)
        && (mi.body_size() >= SIZE_GIANT
            || mons_class_is_stationary(mi.type)
            || mons_class_flag(mi.type, M_INSUBSTANTIAL)))
    {
        descs.emplace_back("immune to nets");
    }

    // Display the chance for a dart of para/confuse/sleep/frenzy
    // to affect monster
    if (item->is_type(OBJ_MISSILES, MI_DART))
    {
        special_missile_type brand = get_ammo_brand(*item);
        if (brand == SPMSL_FRENZY || brand == SPMSL_BLINDING)
        {
            int chance = _get_dart_chance(mi.hd);
            bool immune = brand == SPMSL_FRENZY && !mi.can_go_frenzy;
            if (mi.holi & (MH_UNDEAD | MH_NONLIVING))
                immune = true;

            string verb = brand == SPMSL_FRENZY ? "frenzy" : "blind";

            string chance_string = immune ? "immune" :
                                   make_stringf("chance to %s on hit: %d%%",
                                                verb.c_str(), chance);
            descs.emplace_back(chance_string);
        }
    }
    return descs;
}

/**
 *  Chance for a dart fired by the player to affect a monster of a particular
 *  hit dice, given the player's throwing skill.
 *
 *    @param hd     The monster's hit dice.
 *    @return       The percentage chance for the player to affect the monster,
 *                  rounded down.
 *
 *  This chance is rolled in ranged_attack::dart_check using this formula for
 *  success:
 *      if hd < 15, fixed 3% chance to succeed regardless of roll
 *      else, or if the 3% chance fails,
 *            succeed if 2 + random2(4 + (2/3)*(throwing + stealth) ) >= hd
 */
static int _get_dart_chance(const int hd)
{
    const int pow = (2 * (you.skill_rdiv(SK_THROWING)
                          + you.skill_rdiv(SK_STEALTH))) / 3;

    int chance = 10000 - 10000 * (hd - 2) / (4 + pow);
    chance = min(max(chance, 0), 10000);
    if (hd < 15)
    {
        chance *= 97;
        chance /= 100;
        chance += 300; // 3% chance to ignore HD and affect enemy anyway
    }
    return chance / 100;
}

// Bring up an inventory screen and have user choose an item.
// Returns an item slot, or -1 on abort/failure
// On failure, returns error text, if any.
// TODO: consolidate with menu code in quiver.cc?
static int _fire_prompt_for_item()
{
    if (inv_count() < 1)
        return -1;

    // should this sound happen for `f` too? Not sure what the intent is
#ifdef USE_SOUND
    parse_sound(FIRE_PROMPT_SOUND);
#endif

    int slot = -1;
    if (any_items_of_type(OSEL_THROWABLE))
    {
        slot = prompt_invent_item("Fire/throw which item? (* to show all)",
                                   menu_type::invlist,
                                   OSEL_THROWABLE, OPER_FIRE,
                                   invprompt_flag::no_warning); // handled in quiver
    }
    else
    {
        slot = prompt_invent_item("Fire/throw which item? (Showing full inventory)",
                                   menu_type::invlist,
                                   OSEL_ANY, OPER_FIRE,
                                   invprompt_flag::no_warning);
    }

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
bool fire_warn_if_impossible(bool silent, item_def *weapon)
{
    // If you can't wield it, you can't throw it.
    if (!form_can_wield())
    {
        if (!silent)
            canned_msg(MSG_PRESENT_FORM);
        return true;
    }

    if (you.attribute[ATTR_HELD])
    {
        if (!weapon || !is_range_weapon(*weapon))
        {
            if (!silent)
                mprf("You cannot throw anything while %s.", held_status());
            return true;
        }
        else
#if TAG_MAJOR_VERSION == 34
             if (weapon->sub_type != WPN_BLOWGUN)
#endif
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
    // firing under confusion is allowed, but will be random
    return false;
}

// Portal Projectile requires MP per shot.
bool is_pproj_active()
{
    return !you.confused() && you.duration[DUR_PORTAL_PROJECTILE]
           && enough_mp(1, true, false);
}

class ammo_only_action_cycler : public quiver::action_cycler
{
public:
    // TODO: this could be much fancier, and perhaps allow reselecting an item
    // once you are already in this interface. As it is, this class exists to
    // keep the general quiver ui from appearing under throw_item_no_quiver.
    // Possibly refactor most of throw_item_no_quiver into this class?

    ammo_only_action_cycler()
        : quiver::action_cycler::action_cycler()
    {

    }

    string fire_key_hints() const override
    {
        return "";
    }

    bool targeter_handles_key(command_type) const override
    {
        return false;
    }
};

// Basically does what throwing used to do: throw/fire an item without changing
// the quiver.
void throw_item_no_quiver(dist *target)
{
    dist targ_local;
    if (!target)
        target = &targ_local;

    if (you.has_mutation(MUT_NO_GRASPING))
    {
        mpr("You can't grasp things well enough to throw them.");
        return;
    }

    if (fire_warn_if_impossible(false, you.weapon()))
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    // first find an action
    string warn;
    auto a = quiver::ammo_to_action(_fire_prompt_for_item(), true);

    // handles slot == -1
    if (!a || !a->is_valid())
    {
        canned_msg(MSG_OK);
        return;
    }

    if (a->get_item() >= 0 && !_fire_validate_item(a->get_item(), warn))
    {
        mpr(warn);
        return;
    }
    // This is kind of inelegant, but the following has two effects:
    // * For interactive targeting, use the action_cycler interface, which is
    //   more general (though right now this generality is mostly unused).
    // * Ensure that the regular fire history isn't affected by this call.
    ammo_only_action_cycler q;
    q.set(a, true);
    if (target->needs_targeting())
        q.target();
    else
        q.get()->trigger(*target);

    if (target->isCancel)
        canned_msg(MSG_OK);
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
                && item.sub_type == MI_BOOMERANG;

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

static void _throw_noise(actor* act, const item_def &ammo)
{
    ASSERT(act); // XXX: change to actor &act
    const item_def* launcher = act->weapon();

    if (launcher == nullptr || launcher->base_type != OBJ_WEAPONS)
        return;

    if (is_launched(act, launcher, ammo) != launch_retval::LAUNCHED)
        return;

    int         level = 0;
    const char* msg   = nullptr;

    switch (launcher->sub_type)
    {
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

// throw_it - handles player throwing/firing only. Monster throwing is handled
// in mons_throw().
// called only from ammo_action::trigger; this could probably be further
// refactored to be a method of quiver::ammo_action.
void throw_it(quiver::action &a)
{
    const int ammo_slot = a.get_item();
    item_def *launcher = a.get_launcher();

    bool returning   = false;    // Item can return to pack.
    bool did_return  = false;    // Returning item actually does return to pack.
    const bool teleport = is_pproj_active();

    if (you.confused())
    {
        a.target.target = you.pos();
        a.target.target.x += random2(13) - 6;
        a.target.target.y += random2(13) - 6;
        a.target.isValid = true;
        a.target.cmd_result = CMD_FIRE;
    }
    else
    {
        // non-confused interactive or non-interactive firing
        fire_target_behaviour beh(a);
        direction_chooser_args args;
        args.behaviour = &beh;
        args.mode = TARG_HOSTILE;
        // Makes no sense to aim in a cardinal direction while teleporting
        // projectiles.
        args.allow_shift_dir = !teleport;
        direction(a.target, args);
    }
    if (!a.target.isValid || a.target.isCancel)
        return;

    if (teleport)
    {
        if (!in_bounds(a.target.target))
        {
            // The player hit shift-dir. Boo! Bad player!
        }
        else if (cell_is_solid(a.target.target))
        {
            // why doesn't the targeter check this?
            const char *feat = feat_type_name(env.grid(a.target.target));
            mprf("There is %s there.", article_a(feat).c_str());
            a.target.isValid = false;
            return;
        }
    }

    bolt pbolt;
    pbolt.set_target(a.target);

    item_def& thrown = you.inv[ammo_slot];
    ASSERT(thrown.defined());

    // Figure out if we're thrown or launched.
    const launch_retval projected = is_launched(&you, launcher, thrown);

    const bool tossing = projected == launch_retval::FUMBLED;

    // Making a copy of the item: changed only for venom launchers.
    item_def item = thrown;
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);

    string ammo_name;

    if (_setup_missile_beam(&you, pbolt, item, ammo_name, returning))
    {
        you.turn_is_over = false;
        return;
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
            monster *m = monster_at(a.target.target);
            if (m)
                cancelled = stop_attack_prompt(m, false, a.target.target);

            if (!cancelled && (pbolt.is_explosion || pbolt.special_explosion))
            {
                for (adjacent_iterator ai(a.target.target); ai; ++ai)
                {
                    if (cancelled)
                        break;
                    monster *am = monster_at(*ai);
                    if (am)
                        cancelled = stop_attack_prompt(am, false, *ai);
                    else if (*ai == you.pos())
                    {
                        cancelled = !yesno("That is likely to hit you. Continue anyway?",
                                           false, 'n');
                    }
                }
            }
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
        return;
    }

    pbolt.is_tracer = false;

    bool unwielded = false;
    if (ammo_slot == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        if (!wield_weapon(true, SLOT_BARE_HANDS, true, false, true, false))
            return;

        if (!thrown.quantity)
            return; // destroyed when unequipped (fragile)

        unwielded = true;
    }

    // Now start real firing!
    origin_set_unknown(item);

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if (!teleport)
        pbolt.set_target(a.target);

    const int bow_brand = (projected == launch_retval::LAUNCHED)
                          ? get_weapon_brand(*launcher)
                          : SPWPN_NORMAL;
    const int ammo_brand = get_ammo_brand(item);

    switch (projected)
    {
    case launch_retval::LAUNCHED:
    {
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
    case launch_retval::THROWN:
        practise_throwing((missile_type)wepType);
        count_action(CACT_THROW, wepType, OBJ_MISSILES);
        break;
    case launch_retval::FUMBLED:
        break;
    case launch_retval::BUGGY:
        dprf("Unknown launch type for weapon."); // should never happen :)
        break;
    }

    // check for returning ammo
    if (teleport)
        returning = false;

    you.time_taken = you.attack_delay(&item).roll();

    // Create message.
    mprf("You %s%s %s.",
          teleport ? "magically " : "",
          (projected == launch_retval::FUMBLED ? "toss away" :
           projected == launch_retval::LAUNCHED ? "shoot" : "throw"),
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

    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        pbolt.use_target_as_pos = true;
        pbolt.affect_cell();
        pbolt.affect_endpoint();
        if (!did_return)
            pbolt.drop_object(!tossing);
        // Costs 1 MP per shot.
        pay_mp(1);
        finalize_mp_cost();
    }
    else
    {
        if (crawl_state.game_is_hints())
            Hints.hints_throw_counter++;

        pbolt.drop_item = !returning && !tossing;
        pbolt.fire();

        // For returning ammo, check for mulching before the return step
        if (tossing)
            pbolt.drop_object(false); // never mulch
        else if (returning && thrown_object_destroyed(&item))
            returning = false;
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
        did_god_conduct(DID_CHAOS, 2 + random2(3), bow_brand == SPWPN_CHAOS);

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_FRENZY)
        did_god_conduct(DID_HASTY, 6 + random2(3), true);

    if (returning)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow();
        update_screen();
        pbolt.fire();
    }
    else
    {
        dec_inv_item_quantity(ammo_slot, 1);
        if (unwielded)
            canned_msg(MSG_EMPTY_HANDED_NOW);
    }

    _throw_noise(&you, thrown);

    // ...any monster nearby can see that something has been thrown, even
    // if it didn't make any noise.
    alert_nearby_monsters();

    you.turn_is_over = true;

    if (pbolt.special_explosion != nullptr)
        delete pbolt.special_explosion;

    if (!teleport
        && !tossing
        && will_have_passive(passive_t::shadow_attacks)
        && thrown.base_type == OBJ_MISSILES
        && thrown.sub_type != MI_DART)
    {
        dithmenos_shadow_throw(a.target, item);
    }
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
    ASSERT(env.item[msl].base_type == OBJ_MISSILES);

    const int weapon    = mons->inv[MSLOT_WEAPON];

    mon_inv_type slot = get_mon_equip_slot(mons, env.item[msl]);
    ASSERT(slot != NUM_MONSTER_SLOTS);

    // Energy is already deducted for the spell cast, if using portal projectile
    // FIXME: should it use this delay and not the spell delay?
    if (!teleport)
    {
        const int energy = mons->action_energy(EUT_MISSILE);
        const int delay = mons->attack_delay(&env.item[msl]).roll();
        ASSERT(energy > 0);
        ASSERT(delay > 0);
        mons->speed_increment -= div_rand_round(energy * delay, 10);
    }

    // Dropping item copy, since the launched item might be different.
    item_def item = env.item[msl];
    item.quantity = 1;

    if (_setup_missile_beam(mons, beam, item, ammo_name, returning))
        return false;

    beam.aimed_at_spot |= returning;
    // Avoid overshooting and potentially hitting the player.
    // Piercing beams' tracers already account for this.
    beam.aimed_at_spot |= mons->temp_attitude() == ATT_FRIENDLY
                          && !beam.pierce;

    const launch_retval projected =
        is_launched(mons, mons->mslot_item(MSLOT_WEAPON),
                    env.item[msl]);

    // Identify before throwing, so we don't get different
    // messages for first and subsequent missiles.
    if (mons->observable())
    {
        if (projected == launch_retval::LAUNCHED
               && item_type_known(env.item[weapon])
            || projected == launch_retval::THROWN
               && env.item[msl].base_type == OBJ_MISSILES)
        {
            set_ident_flags(env.item[msl], ISFLAG_KNOW_TYPE);
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
        }
    }

    // Now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0. Ah well.
    string msg = mons->name(DESC_THE);
    if (teleport)
        msg += " magically";
    msg += ((projected == launch_retval::LAUNCHED) ? " shoots " : " throws ");

    if (!beam.name.empty() && projected == launch_retval::LAUNCHED)
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

    _throw_noise(mons, item);

    beam.drop_item = item.sub_type == MI_THROWING_NET;

    // Redraw the screen before firing, in case the monster just
    // came into view and the screen hasn't been updated yet.
    viewwindow();
    update_screen();
    if (teleport)
    {
        beam.use_target_as_pos = true;
        beam.affect_cell();
        beam.affect_endpoint();
    }
    else
        beam.fire();

    if (beam.drop_item && dec_mitm_item_quantity(msl, 1))
        mons->inv[slot] = NON_ITEM;

    if (beam.special_explosion != nullptr)
        delete beam.special_explosion;

    return true;
}

bool thrown_object_destroyed(item_def *item)
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

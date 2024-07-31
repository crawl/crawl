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
#include "describe.h"
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
#include "los.h" // fallback_ray
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

static shared_ptr<quiver::action> _fire_prompt_for_item();
static int  _get_dart_chance(const int hd);
static bool _thrown_object_destroyed(const item_def &item);

bool is_penetrating_attack(const actor& attacker, const item_def* weapon,
                           const item_def& projectile)
{
    return is_throwable(&attacker, projectile)
            && projectile.is_type(OBJ_MISSILES, MI_JAVELIN)
           || weapon
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
        if (!targeted())
            needs_path = false;
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
    if (!a.is_enabled())
    {
        // should this happen for targeted actions too?
        a.target.isValid = false;
        // trigger() is called for messaging in action_cycler::do_target
        return;
    }

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
        {
            internal_prompt = make_stringf("Non-targeted %s",
                lowercase_first(internal_prompt).c_str());
        }
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
    const item_def *launcher = action.get_launcher();
    const bool ranged = launcher && is_range_weapon(*launcher);
    if (!targeted() || !(ranged || (item && item->base_type == OBJ_MISSILES)))
        return descs;

    ostringstream result;
    describe_to_hit(mi, result, ranged ? launcher : item);
    descs.emplace_back(result.str());

    if (get_ammo_brand(*item) == SPMSL_SILVER && mi.is(MB_CHAOTIC))
        descs.emplace_back("chaotic");
    if (item->is_type(OBJ_MISSILES, MI_THROWING_NET) && mi.net_immune())
        descs.emplace_back("immune to nets");

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
static shared_ptr<quiver::action> _fire_prompt_for_item()
{
    if (inv_count() < 1)
        return nullptr;

    // should this sound happen for `f` too? Not sure what the intent is
#ifdef USE_SOUND
    parse_sound(FIRE_PROMPT_SOUND);
#endif

    const bool fireables = any_items_of_type(OSEL_QUIVER_ACTION);
    if (!fireables)
    {
        // TODO: right now disabled but valid items don't trigger this;
        // possibly they should get a similar message? They all do print a
        // more specific message if you try to use them, and some have a
        // prompt or the like (e.g. scroll of fear).
        mpr("You have nothing you can fire or use right now.");
        return make_shared<quiver::action>(); // hack: prevent "Ok, then."
    }

    // does it actually make sense that felid can't toss things?
    const bool can_throw = !you.has_mutation(MUT_NO_GRASPING)
        && !fire_warn_if_impossible(true, you.weapon()); // forms

    int slot = -1;
    const string title = make_stringf(
        "<lightgray>Fire%s/use which item?%s</lightgray>",
        (can_throw ? "/throw" : ""),
        (can_throw ? " ([<w>*</w>] to toss any item)" : ""));
    const string alt_title =
        "<lightgray>Toss away which item?</lightgray>";
    int selector = fireables ? OSEL_QUIVER_ACTION : OSEL_ANY;
    // TODO: the output api here is awkward
    // TODO: it would be nice if items with disabled actions got grayed out
    slot = prompt_invent_item(
                title.c_str(),
                menu_type::invlist,
                selector, OPER_FIRE,
                invprompt_flag::no_warning // warning handled in quiver
                    | invprompt_flag::hide_known,
                '\0',
                can_throw ? alt_title.c_str() : nullptr,
                &selector);
    if (slot == -1)
        return nullptr;

    return selector == OSEL_ANY && can_throw
        ? quiver::ammo_to_action(slot, true) // throw/toss only
        : quiver::slot_to_action(slot, false); // use
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
                mprf("You cannot throw/fire anything while %s.", held_status());
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

class ammo_only_action_cycler : public quiver::action_cycler
{
public:
    // TODO: this could be much fancier, and perhaps allow reselecting an item
    // once you are already in this interface. As it is, this class exists to
    // keep the general quiver ui from appearing under fire_item_no_quiver.
    // Possibly refactor most of fire_item_no_quiver into this class?

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
// TODO: move to quiver.cc?
void fire_item_no_quiver(dist *target)
{
    dist targ_local;
    if (!target)
        target = &targ_local;

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    // first find an action
    auto a = _fire_prompt_for_item();

    // handles slot == -1
    if (!a || !a->is_valid())
    {
        string warn;
        if (a && a->get_item() >= 0
                    && !quiver::toss_validate_item(a->get_item(), &warn))
        {
            mpr(warn);
        }
        else if (!a)
            canned_msg(MSG_OK);
        return;
    }

    // This is kind of inelegant, but the following has two effects:
    // * For interactive targeting, use the action_cycler interface, which is
    //   more general (though right now this generality is mostly unused).
    // * Ensure that the regular fire history isn't affected by this call.
    ammo_only_action_cycler q;
    q.set(a);
    if (target->needs_targeting())
        q.target();
    else
        q.get()->trigger(*target);

    if (target->isCancel)
        canned_msg(MSG_OK);
}

static string _ammo_name(const item_def &item, item_def const *launcher)
{
    if (launcher && is_unrandom_artefact(*launcher, UNRAND_DAMNATION))
        return "a damnation bolt";
    if (is_artefact(item))
        return "the " + item.name(DESC_PLAIN);
    return article_a(item.name(DESC_PLAIN), true);
}

static bool _returning(const item_def &item)
{
    return item.is_type(OBJ_MISSILES, MI_BOOMERANG);
}

static void _setup_missile_beam(const actor *agent, bolt &beam,
                                item_def &item, item_def const *launcher)
{
    const auto cglyph = get_item_glyph(item);
    beam.glyph  = cglyph.ch;
    beam.colour = cglyph.col;
    beam.was_missile = true;

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
    beam.launcher     = launcher;
    beam.item         = &item;
    beam.source       = agent->pos();
    beam.flavour      = BEAM_MISSILE;
    beam.pierce       = is_penetrating_attack(*agent, launcher, item);
    beam.aux_source.clear();

    beam.name = item.name(DESC_PLAIN, false, false, false);

    const unrandart_entry* entry = launcher && is_unrandom_artefact(*launcher)
        ? get_unrand_entry(launcher->unrand_idx) : nullptr;

    if (entry && entry->launch)
    {
        entry->launch(&beam);
        return;
    }

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
}

static void _handle_cannon_fx(actor &act, const item_def &weapon, coord_def targ)
{
    if (!weapon.is_type(OBJ_WEAPONS, WPN_HAND_CANNON))
        return;

    // blast smoke
    for (fair_adjacent_iterator ai(act.pos()); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cell_is_solid(*ai)
            || cloud_at(*ai))
        {
            continue;
        }
        place_cloud(CLOUD_MAGIC_TRAIL, *ai, random_range(3, 6), &act);
        break;
    }

    if (!is_unrandom_artefact(weapon, UNRAND_MULE))
        return;

    // knock back
    if (coinflip())
        act.stumble_away_from(targ, "Mule's kick");
}

static void _throw_noise(actor* act, const item_def &ammo)
{
    ASSERT(act); // XXX: change to actor &act

    if (is_throwable(act, ammo))
        return;

    const item_def* launcher = act->weapon();
    if (launcher == nullptr || !is_range_weapon(*launcher))
        return; // moooom, players are tossing their weapons again

    const char* msg   = nullptr;
    int noise = 5;

    // XXX: move both messages into item-prop.cc?
    switch (launcher->sub_type)
    {
    case WPN_SLING:
        msg   = "You hear a sling whirr.";
        break;
    case WPN_SHORTBOW:
    case WPN_ORCBOW:
    case WPN_LONGBOW:
        msg   = "You hear a bow twang.";
        break;
    case WPN_ARBALEST:
        msg   = "You hear a crossbow thunk.";
        break;
    case WPN_TRIPLE_CROSSBOW:
        msg   = "You hear a triple crossbow go thunk-thunk-thunk.";
        break;
    case WPN_HAND_CANNON:
        noise *= 2;
        msg = "You hear a hand cannon's boom.";
        break;

    default:
        die("Invalid launcher '%s'",
                 launcher->name(DESC_PLAIN).c_str());
        return;
    }

    if (act->is_player() || you.can_see(*act))
        msg = nullptr;


    noisy(noise, act->pos(), msg, act->mid);
}

static void _player_shoot(bolt &pbolt, item_def &item, item_def const *launcher);

// throw_it - handles player throwing/firing only. Monster throwing is handled
// in mons_throw().
// called only from ammo_action::trigger; this could probably be further
// refactored to be a method of quiver::ammo_action.
void throw_it(quiver::action &a)
{
    const item_def *primary = a.get_launcher();
    const item_def *offhand = you.offhand_weapon();
    const item_def *launcher = primary;
    const item_def *alt_launcher = offhand;
    if (primary && offhand && is_range_weapon(*offhand) && coinflip())
    {
        launcher = offhand;
        alt_launcher = primary;
    }
    // launchers have get_item set to the launcher. But, if we are tossing
    // the launcher itself, get_launcher() will be nullptr.
    // XX can this api be simplified now that projectiles and launchers are
    // completely distinct?
    const int ammo_slot = launcher ? -1 : a.get_item();

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
        args.self = confirm_prompt_type::cancel;
        direction(a.target, args);
    }
    if (!a.target.isValid || a.target.isCancel)
        return;

    bolt pbolt;
    pbolt.set_target(a.target);

    item_def fake_proj;
    item_def& thrown = fake_proj;
    if (launcher)
        populate_fake_projectile(*launcher, fake_proj);
    else
        thrown = you.inv[ammo_slot];
    ASSERT(thrown.defined());

    // Figure out if we're thrown or launched.
    const bool is_thrown = is_throwable(&you, thrown);

    // Make a copy of the item.
    item_def item = thrown;
    item.quantity = 1;
    if (ammo_slot != -1)
        item.slot     = index_to_letter(item.link);

    _setup_missile_beam(&you, pbolt, item, launcher);

    // Don't trace at all when confused.
    // Give the player a chance to be warned about helpless targets when using
    // Portaled Projectile, but obviously don't trace a path.
    bool aimed_at_foe = false;
    if (!you.confused())
    {
        // Set values absurdly high to make sure the tracer will
        // complain if we're attempting to fire through allies.
        pbolt.damage = dice_def(1, 100);

        // Init tracer variables.
        pbolt.foe_info.reset();
        pbolt.friend_info.reset();
        pbolt.foe_ratio = 100;
        pbolt.is_tracer = true;
        pbolt.overshoot_prompt = false;

        pbolt.fire();

        pbolt.hit    = 0;
        pbolt.damage = dice_def();
        if (pbolt.friendly_past_target)
            pbolt.aimed_at_spot = true;
        if (pbolt.foe_info.count)
            aimed_at_foe = true; // dubious

        // Should only happen if the player answered 'n' to one of those
        // "Fire through friendly?" prompts.
        if (pbolt.beam_cancelled)
        {
            you.turn_is_over = false;
            return;
        }
    }

    pbolt.is_tracer = false;

    // Now start real firing!
    origin_set_unknown(item);

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    pbolt.set_target(a.target);

    you.time_taken = you.attack_delay(&item).roll();
    _player_shoot(pbolt, item, launcher);
    if (ammo_slot != -1 && (pbolt.item_mulches || !_returning(item)))
        dec_inv_item_quantity(ammo_slot, 1);

    if (launcher && alt_launcher && is_range_weapon(*alt_launcher))
    {
        item_def alt_fake_proj;
        populate_fake_projectile(*alt_launcher, alt_fake_proj);

        bolt alt_pbolt;
        alt_pbolt.set_target(a.target);
        if (pbolt.friendly_past_target)
            alt_pbolt.aimed_at_spot = true;
        _setup_missile_beam(&you, alt_pbolt, alt_fake_proj, alt_launcher);
        _player_shoot(alt_pbolt, alt_fake_proj, alt_launcher);
    }

    // ...any monster nearby can see that something has been thrown, even
    // if it didn't make any noise.
    alert_nearby_monsters();

    you.turn_is_over = true;
    if (aimed_at_foe && launcher && you.has_mutation(MUT_WARMUP_STRIKES))
        you.rev_up(you.time_taken);

    if ((launcher || is_thrown)
        && will_have_passive(passive_t::shadow_attacks)
        && item.base_type == OBJ_MISSILES
        && item.sub_type != MI_DART
        && item.sub_type != MI_THROWING_NET)
    {
        dithmenos_shadow_shoot(a.target, item);
    }
}

// Once the player has committed to a target, shoot/throw/toss at it.
static void _player_shoot(bolt &pbolt, item_def &item, item_def const *launcher)
{
    const int bow_brand = launcher ? get_weapon_brand(*launcher) : SPWPN_NORMAL;
    const int ammo_brand = get_ammo_brand(item);
    const bool returning = _returning(item);
    const bool is_thrown = is_throwable(&you, item);
    const bool tossing = !launcher && !is_thrown;

    if (launcher)
    {
        practise_launching(*launcher);
        if (is_unrandom_artefact(*launcher)
            && get_unrand_entry(launcher->unrand_idx)->type_name)
        {
            count_action(CACT_FIRE, launcher->unrand_idx);
        }
        else
            count_action(CACT_FIRE, launcher->sub_type);
    }
    else if (is_thrown)
    {
        practise_throwing((missile_type)item.sub_type);
        count_action(CACT_THROW, item.sub_type, OBJ_MISSILES);
    }

    // Create message.
    mprf("You %s %s%s.",
          is_thrown ? "throw" : launcher ? "shoot" : "toss away",
          _ammo_name(item, launcher).c_str(),
          you.current_vision == 0 ? " into the darkness" : "");

    // Ensure we're firing a 'missile'-type beam.
    pbolt.pierce    = false;
    pbolt.is_tracer = false;

    pbolt.loudness = item.base_type == OBJ_MISSILES
                   ? ammo_type_damage(item.sub_type) / 3
                   : 0; // Maybe not accurate, but reflects the damage.

    // Mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (item.base_type == OBJ_MISSILES)
        item.flags |= ISFLAG_THROWN;
    pbolt.item_mulches = !tossing && _thrown_object_destroyed(item);
    pbolt.drop_item = !pbolt.item_mulches && !returning;
    pbolt.hit = 0;

    if (crawl_state.game_is_hints())
        Hints.hints_throw_counter++;

    const coord_def target = pbolt.target;

    // XXX: Firing via beam will never hit a target outside our vision range,
    //      so create the ranged_attack manually when bumping into something
    //      during the start of Primordial Nightfall
    if (you.current_vision == 0)
    {
        monster* mon = monster_at(target);
        if (mon && mon->alive())
        {
            ranged_attack attk(&you, mon, launcher, pbolt.item, false, &you, false);
            attk.attack();
        }
    }
    else
        pbolt.fire();

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
        did_god_conduct(DID_CHAOS, 2 + random2(3), bow_brand == SPWPN_CHAOS);

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_FRENZY)
        did_god_conduct(DID_HASTY, 6 + random2(3), true);

    if (returning && !pbolt.item_mulches)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow();
        update_screen();
        pbolt.fire();
    }

    _throw_noise(&you, item);

    if (launcher)
        _handle_cannon_fx(you, *launcher, target);

    if (pbolt.special_explosion != nullptr)
        delete pbolt.special_explosion;
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

bool mons_throw(monster* mons, bolt &beam, bool teleport)
{
    ASSERT(beam.item);
    const item_def &missile = *beam.item;
    ASSERT(missile.base_type == OBJ_MISSILES);

    // Energy is already deducted for the spell cast, if using portal projectile
    // FIXME: should it use this delay and not the spell delay?
    if (!teleport)
    {
        const int energy = mons->action_energy(EUT_MISSILE);
        const int delay = mons->attack_delay(&missile).roll();
        ASSERT(energy > 0);
        ASSERT(delay > 0);
        mons->speed_increment -= div_rand_round(energy * delay, 10);
    }

    // Dropping item copy, since the launched item might be different.
    item_def item = missile;
    item.quantity = 1;

    item_def *launcher = nullptr;
    if (!is_throwable(mons, item))
        launcher = mons->weapon(0);
    _setup_missile_beam(mons, beam, item, launcher);
    beam.aimed_at_spot |= _returning(item);
    // Avoid overshooting and potentially hitting the player.
    // Piercing beams' tracers already account for this.
    beam.aimed_at_spot |= mons->temp_attitude() == ATT_FRIENDLY
                          && !beam.pierce;

    if (beam.name.empty())
        beam.name = item.name(DESC_PLAIN, false, false, false);

    const bool thrown = is_throwable(mons, missile);
    if (mons->observable())
    {
        mpr(make_stringf("%s%s %s %s.",
                         mons->name(DESC_THE).c_str(),
                         teleport ? " magically" : "",
                         thrown ? "throws" : "shoots",
                         article_a(beam.name).c_str()).c_str());
    }

    _throw_noise(mons, item);

    beam.drop_item = item.sub_type == MI_THROWING_NET;

    // Redraw the screen before firing, in case the monster just
    // came into view and the screen hasn't been updated yet.
    viewwindow();
    update_screen();
    const coord_def target = beam.target;
    if (teleport)
    {
        beam.use_target_as_pos = true;
        beam.affect_cell();
        beam.affect_endpoint();
    }
    else
        beam.fire();

    if (beam.drop_item && dec_mitm_item_quantity(mons->inv[MSLOT_MISSILE], 1, false))
        mons->inv[MSLOT_MISSILE] = NON_ITEM;

    if (beam.special_explosion != nullptr)
        delete beam.special_explosion;

    // dubious...
    if (mons->alive() && mons->weapon())
        _handle_cannon_fx(*mons, *(mons->weapon()), target);

    return true;
}

static bool _thrown_object_destroyed(const item_def &item)
{
    if (item.base_type != OBJ_MISSILES)
        return false;

    if (ammo_always_destroyed(item))
        return true;

    if (ammo_never_destroyed(item))
        return false;

    const int base_chance = ammo_type_destroy_chance(item.sub_type);
    const int brand = get_ammo_brand(item);

    // Inflate by 2 to avoid rounding errors.
    const int mult = 2;
    int chance = base_chance * mult;

    if (brand == SPMSL_CURARE)
        chance /= 2;

    dprf("mulch chance: %d in %d", mult, chance);

    return x_chance_in_y(mult, chance);
}

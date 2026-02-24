/**
 * @file
 * @brief Throwing and launching stuff.
**/

#include "AppHdr.h"

#include "throw.h"

#include <cmath>
#include <sstream>

#include "act-iter.h"
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
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static shared_ptr<quiver::action> _fire_prompt_for_item();
static int  _get_dart_chance(const int hd);
static bool _thrown_object_destroyed(const item_def &item);

bool is_penetrating_attack(const item_def& weapon)
{
    return weapon.is_type(OBJ_MISSILES, MI_JAVELIN)
            || get_weapon_brand(weapon) == SPWPN_PENETRATION
            || is_unrandom_artefact(weapon, UNRAND_STORM_BOW);
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
    if (item->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        if (mi.net_immune())
            descs.emplace_back("immune to nets");
        else if (mi.net_escape_capable())
            descs.emplace_back("can blink free");
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
        "<lightgray>Fire%s/use which item?</lightgray>",
        (can_throw ? "/throw" : ""));
    // TODO: the output api here is awkward
    // TODO: it would be nice if items with disabled actions got grayed out
    slot = prompt_invent_item(
                title.c_str(),
                menu_type::invlist,
                OSEL_QUIVER_ACTION, OPER_FIRE,
                invprompt_flag::no_warning, // warning handled in quiver
                '\0');
    if (slot == -1)
        return nullptr;

    return quiver::slot_to_action(slot);
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

static void _player_shoot(ranged_attack_beam &pbolt, bool allow_salvo = true);

static bool _returning(const item_def &item)
{
    return item.is_type(OBJ_MISSILES, MI_BOOMERANG);
}

ranged_attack_beam::ranged_attack_beam(actor& agent, item_def& item)
    : atk(ranged_attack(&agent, nullptr, &item))
{
    initialise_beam(agent, item);
}

ranged_attack_beam::ranged_attack_beam(actor& agent, item_def& item, bolt& _beam)
    : beam(_beam), atk(ranged_attack(&agent, nullptr, &item))
{
    initialise_beam(agent, item);
}

void ranged_attack_beam::fire()
{
    beam.fire_as_ranged_attack(atk);
}

void ranged_attack_beam::initialise_beam(actor &agent, item_def &item)
{
    const auto cglyph = get_item_glyph(item);
    beam.glyph  = cglyph.ch;
    beam.colour = cglyph.col;
#ifdef USE_TILE
    beam.tile_beam = tileidx_item_projectile(item);
#endif

    if (agent.is_player())
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monster* mon = agent.as_monster();

        beam.attitude      = mons_attitude(*mon);
        beam.thrower       = KILL_MON_MISSILE;
    }

    // Arbitrary damage for the tracer (will be unused when fired for real)
    beam.damage       = dice_def(10, 10);
    beam.range        = you.current_vision;
    beam.source_id    = agent.mid;
    beam.source       = agent.pos();
    beam.flavour      = BEAM_MISSILE;
    beam.pierce       = is_penetrating_attack(item);
    beam.aux_source.clear();

    beam.name = launched_projectile_name(item);

    const unrandart_entry* entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.unrand_idx) : nullptr;

    if (entry && entry->launch)
        entry->launch(&beam);
}

static void _handle_cannon_fx(actor &act, const item_def &weapon, coord_def targ)
{
    if (!weapon.is_type(OBJ_WEAPONS, WPN_HAND_CANNON))
        return;

    // blast smoke
    for (fair_adjacent_iterator ai(act.pos()); ai; ++ai)
        if (place_cloud(CLOUD_MAGIC_TRAIL, *ai, random_range(3, 6), &act))
            break;

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

static vector<ranged_attack_beam> _construct_player_ranged_beams(item_def* throwing_weapon = nullptr)
{
    if (throwing_weapon)
        return vector<ranged_attack_beam>{ranged_attack_beam(you, *throwing_weapon)};
    else
    {
        // We can assume the player has at least one launcher equipped to get this far,
        // but it won't necessarily be in their main hand.
        item_def *launcher = you.weapon();
        if (!launcher || !is_range_weapon(*launcher))
            launcher = you.offhand_weapon();
        item_def *alt_launcher = you.offhand_weapon();
        if (!alt_launcher || !is_range_weapon(*alt_launcher) || launcher == alt_launcher)
            return vector<ranged_attack_beam>{ranged_attack_beam(you, *launcher)};

        return vector<ranged_attack_beam>{ranged_attack_beam(you, *launcher),
                                          ranged_attack_beam(you, *alt_launcher)};
    }
}

// Returns true if the player aborted for any reason.
static bool _trace_player_ranged_attacks(vector<ranged_attack_beam>& atks, bool no_harm_allies = false)
{
    // Don't trace at all when confused.
    if (you.confused())
        return false;

    player_beam_tracer tracer;
    bool using_mule = false;
    for (size_t i = 0; i < atks.size(); ++i)
    {
        atks[i].beam.overshoot_prompt = false;
        if (no_harm_allies)
            atks[i].beam.stop_at_allies = true;
        atks[i].beam.fire(tracer);
        if (atks[i].beam.friendly_past_target)
            atks[i].beam.aimed_at_spot = true;
        using_mule |= is_unrandom_artefact(*atks[i].atk.weapon, UNRAND_MULE);
    }

    if (no_harm_allies && tracer.has_any_warnings())
        return true;

    if (cancel_beam_prompt(atks[0].beam, tracer, atks.size()))
        return true;

    // Warn about Mule potentially knocking the player back into a trap.
    if (using_mule)
    {
        const coord_def back = you.stumble_pos(atks[0].beam.target);
        if (!back.origin()
            && back != you.pos()
            && !check_moveto(back, "potentially stumble back", false))
        {
            return true;
        }
    }

    return false;
}

static void _fire_player_ranged_attacks(vector<ranged_attack_beam>& atks, bool allow_salvo = true)
{
    // If attacking with more than one weapon, do so in a random order.
    if (atks.size() > 1)
        shuffle_array(atks);

    // XXX: We must save this before the ranged attack itself is fired, since
    //      consuming the last of a stack of missiles will erase all information
    //      about what type of missile was just thrown.
    const missile_type missile = atks[0].atk.weapon->base_type == OBJ_MISSILES
                                    ? static_cast<missile_type>(atks[0].atk.weapon->sub_type)
                                    : NUM_MISSILES;

    bool shot_at_enemy = false;
    for (ranged_attack_beam& atk : atks)
    {
        _player_shoot(atk, allow_salvo);
        if (atk.beam.foes_hurt)
            shot_at_enemy = true;
    }

    if (shot_at_enemy)
    {
        if (will_have_passive(passive_t::shadow_attacks)
            && (missile != MI_DART && missile != MI_THROWING_NET))
        {
            dithmenos_shadow_shoot(atks[0].beam.target, missile);
        }

        if (you.duration[DUR_PARAGON_ACTIVE] && !you.triggers_done[DID_PARAGON])
            paragon_attack_trigger();

        if (you.has_mutation(MUT_WARMUP_STRIKES) && !you.triggers_done[DID_REV_UP] && missile == NUM_MISSILES)
            you.rev_up(you.attack_delay().roll());
    }
}

// Aim and then perform a standard ranged attack, either by autofight or the quiver interface.
void aim_player_ranged_attack(quiver::action &a)
{
    ASSERT(a.get_item() >= 0);
    item_def* item = &you.inv[a.get_item()];
    const bool throwing = item->base_type == OBJ_MISSILES;

    if (you.confused())
    {
        do
        {
            a.target.target.x = you.pos().x + random2(13) - 6;
            a.target.target.y = you.pos().y + random2(13) - 6;
        } while (a.target.target == you.pos());

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

    // Now that we have a target, set up the attacks and run tracers
    // (which may also adjust targeting slightly).
    vector<ranged_attack_beam> atks = _construct_player_ranged_beams(throwing ? item : nullptr);

    for (ranged_attack_beam& atk : atks)
        atk.beam.set_target(a.target);

    if (_trace_player_ranged_attacks(atks))
        return;

    // Actually perform the attack and spend time.
    you.time_taken = you.attack_delay(item).roll();
    _fire_player_ranged_attacks(atks);
    you.turn_is_over = true;
}

// Make the player immediately perform a ranged attack at a given target, optionally
// with a specific projectile. Does not handle spending time.
bool do_player_ranged_attack(const coord_def& targ, item_def* thrown_projectile,
                             const ranged_attack* prototype, bool no_harm_allies,
                             bool allow_salvo)
{
    vector<ranged_attack_beam> atks = _construct_player_ranged_beams(thrown_projectile);
    for (ranged_attack_beam& atk : atks)
    {
        atk.beam.target = targ;
        if (prototype)
            prototype->copy_params_to(atk.atk);
    }
    if (_trace_player_ranged_attacks(atks, no_harm_allies))
        return false;
    _fire_player_ranged_attacks(atks, allow_salvo);
    return true;
}

// Runs a simple tracer from source to target, stopping before any allies and
// adding affected enemies to the affected param.
//
// Returns true of a given target will hit at least one enemy without also
// hitting any enemy already stored in affected.
static bool _salvo_shot_tracer(coord_def source, coord_def target, bool pierce,
                               set<mid_t>* affected)
{
    bolt tracer;
    tracer.attitude = ATT_FRIENDLY;
    tracer.range = LOS_RADIUS;
    tracer.source = source;
    tracer.target = target;
    tracer.source_id = MID_PLAYER;
    tracer.stop_at_allies = true;
    tracer.damage = dice_def(100, 1);
    tracer.pierce = pierce;
    targeting_tracer target_tracer;
    tracer.fire(target_tracer);

    if (target_tracer.friend_info.power != 0 || target_tracer.foe_info.power == 0)
        return false;

    if (affected)
    {
        for (std::map<mid_t,int>::iterator it = tracer.hit_count.begin(); it != tracer.hit_count.end(); ++it)
        {
            if (affected->count(it->first))
                return false;
            else
                affected->insert(it->first);
        }
    }

    return true;
}

static void _fire_salvo(const ranged_attack_beam &pbolt)
{
    set<mid_t> affected_mons;
    vector<monster*> targs;

    // Add affected targets from the originating shot.
    _salvo_shot_tracer(you.pos(), pbolt.beam.target, false, &affected_mons);

    // Trace all visiable monsters in LoS, starting with the closest, and see
    // how many 'different' shots are available (without overlapping against
    // one target)
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        monster* targ = monster_at(*ri);
        if (!targ || targ->wont_attack() || targ->is_firewood() || !you.can_see(*targ)
            || affected_mons.count(targ->mid))
        {
            continue;
        }

        if (_salvo_shot_tracer(you.pos(), targ->pos(), false, &affected_mons))
            targs.push_back(targ);
    }

    const int MAX_SHOTS = 5;
    if (targs.size() > MAX_SHOTS)
        shuffle_array(targs);

    // Now fire up to 5 bonus shots at random valid targets.
    for (size_t i = 0; i < targs.size() && i < MAX_SHOTS; ++i)
    {
        // We copy the ranged_attack_beam and fire manually to avoid
        // printing superfluous messages and triggering conducts repeatedly.
        ranged_attack_beam salvo = pbolt;
        salvo.beam.chose_ray = false;
        salvo.beam.target = targs[i]->pos();
        salvo.fire();
    }
}

// Once the player has committed to a target, shoot/throw/toss at it.
// Handles conducts, action counts, ammunition use, etc.
static void _player_shoot(ranged_attack_beam &pbolt, bool allow_salvo)
{
    const item_def& item = *pbolt.atk.weapon;
    const int bow_brand = get_weapon_brand(item);
    const int ammo_brand = get_ammo_brand(item);
    const bool returning = _returning(item);
    const bool is_thrown = is_throwable(&you, item);
    const bool will_mulch = _thrown_object_destroyed(item);

    if (is_range_weapon(item))
    {
        practise_launching(item);
        if (is_unrandom_artefact(item)
            && get_unrand_entry(item.unrand_idx)->type_name)
        {
            count_action(CACT_FIRE, item.unrand_idx);
        }
        else
            count_action(CACT_FIRE, item.sub_type);
    }
    else if (is_thrown)
    {
        practise_throwing((missile_type)item.sub_type);
        count_action(CACT_THROW, item.sub_type, OBJ_MISSILES);
    }

    const bool do_salvo = allow_salvo
                            && (you.duration[DUR_SALVO] && is_range_weapon(item)
                                || is_unrandom_artefact(item, UNRAND_ZEPHYR));
    const string proj_name = do_salvo ? make_stringf("a salvo of %ss", pbolt.atk.projectile_name().c_str())
                                      : article_a(pbolt.atk.projectile_name()).c_str();

    // Create message.
    mprf("You %s %s%s.",
          is_thrown ? "throw" : "shoot" ,
          proj_name.c_str(),
          you.current_vision == 0 ? " into the darkness" : "");

    pbolt.beam.set_is_tracer(false);

    pbolt.beam.loudness = item.base_type == OBJ_MISSILES
                   ? ammo_type_damage(item.sub_type) / 3
                   : 0; // Maybe not accurate, but reflects the damage.

    pbolt.beam.drop_item = is_thrown && !returning && !will_mulch;
    pbolt.atk.will_mulch = will_mulch;

    if (crawl_state.game_is_hints())
        Hints.hints_throw_counter++;

    const coord_def target = pbolt.beam.target;

    // XXX: Firing via beam will never hit a target outside our vision range,
    //      so create the ranged_attack manually when bumping into something
    //      during the start of Primordial Nightfall
    if (you.current_vision == 0)
    {
        monster* mon = monster_at(target);
        if (mon && mon->alive())
        {
            ranged_attack attk(&you, mon, &item, false, &you);
            attk.will_mulch = will_mulch;
            attk.attack();
        }
    }
    else
    {
        pbolt.fire();

        if (do_salvo)
        {
            _fire_salvo(pbolt);
            if (!is_unrandom_artefact(item, UNRAND_ZEPHYR))
            {
                if (--you.props[SALVO_KEY] == 0)
                    you.duration[DUR_SALVO] = 0;
                else
                    you.duration[DUR_SALVO] = random_range(30, 50);
            }
        }
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
        did_god_conduct(DID_CHAOS, 2 + random2(3), bow_brand == SPWPN_CHAOS);

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_FRENZY)
        did_god_conduct(DID_HASTY, 6 + random2(3), true);

    if (returning && !will_mulch)
    {
        // Fire beam in reverse.
        pbolt.beam.setup_retrace();
        viewwindow();
        update_screen();
        pbolt.fire();
    }

    _throw_noise(&you, item);

    _handle_cannon_fx(you, item, target);

    if (pbolt.beam.special_explosion != nullptr)
        delete pbolt.beam.special_explosion;

    // Actually expend a throwing item, if it didn't return to the player's hands.
    if (item.base_type == OBJ_MISSILES && (!returning || will_mulch))
        dec_inv_item_quantity(item.link, 1);
}

bool mons_throw(monster* mons, ranged_attack_beam& ratk, bool teleport, bool was_redirected)
{
    const item_def &weapon = *ratk.atk.weapon;
    bolt& beam = ratk.beam;

    // Energy is already deducted for the spell cast, if using portal projectile
    // FIXME: should it use this delay and not the spell delay?
    if (!teleport)
    {
        const int energy = mons->action_energy(EUT_MISSILE);
        const int delay = mons->attack_delay(&weapon).roll();
        ASSERT(energy > 0);
        ASSERT(delay > 0);
        mons->speed_increment -= div_rand_round(energy * delay, 10);
    }

    // Avoid overshooting unless we're *trying* to hit allies.
    // (Piercing beam tracers should already handle this).
    if (!was_redirected)
        beam.stop_at_allies |= !beam.pierce;
    beam.aimed_at_spot  |= _returning(weapon);

    const bool thrown = is_throwable(mons, weapon);
    if (mons->observable())
    {
        mpr(make_stringf("%s%s %s %s.",
                         mons->name(DESC_THE).c_str(),
                         teleport ? " magically" : "",
                         thrown ? "throws" : "shoots",
                         article_a(ratk.atk.projectile_name()).c_str()).c_str());
    }

    _throw_noise(mons, weapon);

    const bool uses_ammo = weapon.is_type(OBJ_MISSILES, MI_THROWING_NET);
    beam.drop_item = uses_ammo;

    // Redraw the screen before firing, in case the monster just
    // came into view and the screen hasn't been updated yet.
    viewwindow();
    update_screen();
    const coord_def target = beam.target;
    if (teleport)
    {
        beam.use_target_as_pos = true;
        unwind_var<ranged_attack*> old_atk(beam.ranged_atk, &ratk.atk);
        beam.affect_cell();
        beam.affect_endpoint();
    }
    else
        ratk.fire();

    if (_returning(weapon))
    {
        // Fire beam in reverse.
        beam.setup_retrace();
        viewwindow();
        update_screen();
        ratk.fire();
    }

    if (beam.special_explosion != nullptr)
        delete beam.special_explosion;

    if (uses_ammo && dec_mitm_item_quantity(mons->inv[MSLOT_MISSILE], 1, false))
        mons->inv[MSLOT_MISSILE] = NON_ITEM;

    // dubious...
    if (mons->alive())
        _handle_cannon_fx(*mons, weapon, target);

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

    return one_chance_in(ammo_destroy_chance(item));
}

bool do_west_wind_shot()
{
    // Perform at most one gale-force shot per turn, and only if the player moved.
    if (!you.has_mutation(MUT_WEST_WIND) || you.triggers_done[DID_WEST_WIND_SHOT]
        || you.pos() == you.pos_at_turn_start)
    {
        return false;
    }

    const item_def* wpn = you.weapon();
    if (!wpn || !is_range_weapon(*wpn) || you.berserk() || you.confused())
        return false;

    you.did_trigger(DID_WEST_WIND_SHOT);

    const coord_def targ = (you.pos() - you.pos_at_turn_start).sgn() + you.pos();
    ranged_attack prototype(&you, nullptr, wpn);

    prototype.dmg_mult = 33;
    prototype.pierce = true;
    prototype.set_projectile_prefix("wind-blessed");

    // Downscale damage from slow weapons versus the player's movement speed.
    const int attack_delay = you.attack_delay().roll() * BASELINE_DELAY;
    const int move_delay = player_movement_speed() * player_speed();
    if (attack_delay > move_delay)
        prototype.dmg_mult += (move_delay * 100 / attack_delay) - 100;

    if (_salvo_shot_tracer(you.pos(), targ, true, nullptr))
    {
        bolt wind;
        wind.source = you.pos();
        wind.target = targ;
        wind.pierce = true;
        wind.range  = you.current_vision;
        wind.set_is_tracer(true);
        wind.fire();

        // Push monsters from back to front (but never pushing them out of sight)
        for (int i = wind.path_taken.size() - 2; i >= 0; --i)
        {
            const int dist = min((int)wind.path_taken.size() - i - 1, random_range(1, 2));
            if (monster* mon = monster_at(wind.path_taken[i]))
                mon->knockback(you, dist, 0, "wind");
        }

        do_player_ranged_attack(targ, nullptr, &prototype, true, false);

        you.duration[DUR_SALVO] = random_range(20, 40);
        int& stacks = you.props[SALVO_KEY].get_int();
        stacks = min(5, stacks + 1);
    }

    return true;
}

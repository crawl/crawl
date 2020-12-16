/**
 * @file
 * @brief Player quiver functionality
**/

#include "AppHdr.h"

#include "quiver.h"

#include <algorithm>

#include "ability.h"
#include "artefact.h"
#include "art-enum.h"
#include "directn.h"
#include "env.h"
#include "evoke.h"
#include "fight.h"
#include "invent.h"
#include "item-prop.h"
#include "item-use.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "sound.h"
#include "spl-damage.h"
#include "stringutil.h"
#include "tags.h"
#include "target.h"
#include "terrain.h"
#include "tilepick.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "rltiles/tiledef-icons.h"

static int _get_pack_slot(const item_def&);
static bool _item_matches(const item_def &item, fire_type types,
                          const item_def* launcher, bool manual);
static bool _items_similar(const item_def& a, const item_def& b,
                           bool force = true);

// TODO: how should newquivers integrate with the local tiles UI?

namespace quiver
{
    // Returns the type of ammo used by the player's equipped weapon,
    // or AMMO_THROW if it's not a launcher.
    static launcher _get_weapon_ammo_type(const item_def* weapon)
    {
        if (weapon == nullptr)
            return AMMO_THROW;
        if (weapon->base_type != OBJ_WEAPONS)
            return AMMO_THROW;

        switch (weapon->sub_type)
        {
            case WPN_HUNTING_SLING:
            case WPN_FUSTIBALUS:
                return AMMO_SLING;
            case WPN_SHORTBOW:
            case WPN_LONGBOW:
                return AMMO_BOW;
            case WPN_HAND_CROSSBOW:
            case WPN_ARBALEST:
            case WPN_TRIPLE_CROSSBOW:
                return AMMO_CROSSBOW;
            default:
                return AMMO_THROW;
        }
    }

    static maybe_bool _fireorder_inscription_ok(int slot, bool manual = false)
    {
        if (slot < 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return MB_MAYBE;
        if (strstr(you.inv[slot].inscription.c_str(), manual ? "=F" : "=f"))
            return MB_FALSE;
        if (strstr(you.inv[slot].inscription.c_str(), manual ? "+F" : "+f"))
            return MB_TRUE;
        return MB_MAYBE;
    }

    void action::reset()
    {
        target = dist();
    }

    /**
     * Does this action meet preconditions for triggering? Checks configurable
     * HP and MP thresholds, aimed at autofight commands.
     * @return true if triggering should be prevented
     */
    bool action::autofight_check() const
    {
        // don't do these checks if the action will lead to interactive targeting
        if (target.needs_targeting())
            return false;
        bool af_hp_check = false;
        bool af_mp_check = false;
        if (!clua.callfn("af_hp_is_low", ">b", &af_hp_check)
            || uses_mp() && !clua.callfn("af_mp_is_low", ">b", &af_mp_check))
        {
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            return true;
        }
        if (af_hp_check)
            mpr("You are too injured to fight recklessly!");
        else if (af_mp_check)
            mpr("You are too depleted to draw on your mana recklessly!");
        return af_hp_check || af_mp_check;
    }

    formatted_string action::quiver_description(bool short_desc) const
    {
        return formatted_string::parse_string(
                        short_desc ? "<darkgrey>Empty</darkgrey>"
                                   : "<darkgrey>Nothing quivered</darkgrey>");
    }

    vector<tile_def> action::get_tiles() const
    {
        // generic handling for items: this covers a bunch of subclasses
        const int i = get_item();
        if (i >= 0 && i < ENDOFPACK && you.inv[i].defined())
        {
            vector<tile_def> ret;
            if (get_tiles_for_item(you.inv[i], ret, false))
                return ret;
        }

        // valid empty action
        if (is_valid())
            return { TILEI_DISABLED };

        // fallback in case a subclass fails to implement this
        return { TILE_ERROR };
    }

    shared_ptr<action> action::find_next(int dir, bool allow_disabled, bool loop) const
    {
        auto o = get_fire_order(allow_disabled);
        if (o.size() == 0)
            return nullptr; // or same type?

        if (dir < 0)
            reverse(o.begin(), o.end());

        if (!is_valid())
            return o[0];

        int i = 0;
        for (; i < static_cast<int>(o.size()); i++)
            if (*o[i] == *this)
                break;

        // the current action is not in the fire order at all; perhaps it is
        // disabled, or skipped for action-specific reasons. Just start at
        // the beginning.
        if (i == static_cast<int>(o.size()))
            return o[0];

        i++;
        if (!loop && i >= static_cast<int>(o.size()))
            return nullptr;

        return o[i % o.size()];
    }

    /**
     * Generic inscription check: if this action uses an item, is it inscribed
     * in a way that allows firing? Returns true if no item or an invalid item
     * is used.
     */
    bool action::do_inscription_check() const
    {
        const int slot = get_item();
        if (slot <= 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return true;

        return check_warning_inscriptions(you.inv[slot], OPER_FIRE);
    }

    shared_ptr<action> action_cycler::do_target()
    {
        // this would be better as an action method, but it's tricky without
        // moving untargeted_fire somewhere else
        // should this reset()?

        shared_ptr<action> a = get();
        if (!a || !a->is_valid())
            return nullptr;

        a->reset();
        a->target.target = coord_def(-1,-1);
        a->target.find_target = false;
        a->target.fire_context = this;
        a->target.interactive = true;

        if (a->is_targeted())
            a->trigger(a->target);
        else
        {
            untargeted_fire(a);
            if (!a->target.isCancel)
                a->trigger();
        }
        // TODO: does this cause dbl "ok then"s in some places?
        if (a->target.isCancel && a->target.cmd_result == CMD_NO_CMD)
            canned_msg(MSG_OK);

        // we return a; if it has become invalid (e.g. by running out of ammo),
        // it will no longer be accessible via get().
        return a;
    }

    string action_cycler::fire_key_hints()
    {
        const bool no_other_items = *get() == *next();
        string key_hint = no_other_items
                            ? ", <w>%</w> - select action"
                            : ", <w>%</w> - select action, <w>%</w>/<w>%</w> - cycle";
        insert_commands(key_hint,
                        { CMD_TARGET_SELECT_ACTION,
                          CMD_TARGET_CYCLE_QUIVER_BACKWARD,
                          CMD_TARGET_CYCLE_QUIVER_FORWARD });
        return key_hint;
    }

    static bool _autoswitch_active()
    {
        return Options.auto_switch
                && (you.equip[EQ_WEAPON] == letter_to_index('a')
                    || you.equip[EQ_WEAPON] == letter_to_index('b'));
    }

    static bool _autoswitch_ammo_check(const item_def &ammo)
    {
        if (!ammo.defined())
            return false;
        const item_def &w1 = you.inv[letter_to_index('a')];
        const item_def &w2 = you.inv[letter_to_index('b')];
        return w1.defined() && _item_matches(ammo, (fire_type) 0xffff, &w1, false)
            || w2.defined() && _item_matches(ammo, (fire_type) 0xffff, &w2, false);
    }


    static bool _autoswitch_to_ranged(item_def &ammo)
    {
        // TODO: switching away from ranged weapons with autoswitch on is a bit
        // wonky
        if (!_autoswitch_active())
            return false;

        // validated above
        const int item_slot = you.equip[EQ_WEAPON] == letter_to_index('a')
                                ? letter_to_index('b') : letter_to_index('a');

        const item_def& launcher = you.inv[item_slot];
        if (!_autoswitch_ammo_check(ammo))
            return false;
        if (!ammo.launched_by(launcher))
            return false;

        if (!wield_weapon(true, item_slot))
            return false;

        you.turn_is_over = true;
        // This just does the wield. The old implementation worked by
        // additionally firing immediately, but it seems better to do it step
        // by step to me. Will players dislike this?
        return true;
    }

    // Get a sorted list of items to show in the fire interface.
    //
    // If ignore_inscription_etc, ignore =f and Options.fire_items_start.
    // This is used for generating informational error messages, when the
    // fire order is empty.
    //
    // launcher determines what items match the 'launcher' fire_order type.
    static void _get_item_fire_order(vector<int>& order,
                                     bool ignore_inscription_etc,
                                     const item_def* launcher,
                                     bool manual)
    {
        const int inv_start = (ignore_inscription_etc ? 0
                                                      : Options.fire_items_start);

        for (int i_inv = inv_start; i_inv < ENDOFPACK; i_inv++)
        {
            const item_def& item = you.inv[i_inv];
            if (!item.defined())
                continue;

            const auto l = is_launched(&you, launcher, item);

            // don't swap to throwing when you run out of launcher ammo. (The
            // converse case should be ruled out by _item_matches below.)
            // TODO: (a) is this the right thing to do, and (b) should it be
            // done in _item_matches? I don't fully understand the logic there.
            if (!manual
                && _get_weapon_ammo_type(launcher) != AMMO_THROW
                && l == launch_retval::THROWN)
            {
                continue;
            }

            // =f prevents item from being in fire order.
            if (!ignore_inscription_etc
                    && _fireorder_inscription_ok(i_inv, manual) == MB_FALSE)
            {
                continue;
            }

            for (unsigned int i_flags = 0; i_flags < Options.fire_order.size();
                 i_flags++)
            {
                if (_item_matches(item, (fire_type) Options.fire_order[i_flags],
                                  launcher, manual)
                    || (launcher && _autoswitch_active()
                            && (launcher->link == letter_to_index('a')
                                || launcher->link == letter_to_index('b'))
                            && _autoswitch_ammo_check(item)))
                {
                    // this approach to sorting is pretty wtf
                    order.push_back((i_flags<<16) | (i_inv & 0xffff));
                    break;
                }
            }
        }

        sort(order.begin(), order.end());

        for (unsigned int i = 0; i < order.size(); i++)
            order[i] &= 0xffff;
    }

    // class isn't intended for quivering per se. Rather, it's a wrapper on
    // targeted attacks involving melee weapons or unarmed fighting. This
    // covers regular 1-space melee attacks, as well as reaching attacks of
    // arbitrary distance (currently only going up to 2 in practice).
    struct melee_action : public action
    {
        melee_action() : action()
        {
        }

        void save(CrawlHashTable &save_target) const override;

        bool is_enabled() const override
        {
            // just disable the action for these cases. In the long run it
            // may be good to merge swing_at_target into here?
            return !you.caught()
                && !you.confused();
        }

        bool is_valid() const override { return true; }
        bool is_targeted() const override { return true; }

        string quiver_verb() const
        {
            const item_def *weapon = you.weapon();

            if (!weapon)
            {
                const auto form_verbs = get_form(you.form)->uc_attack_verbs;
                if (form_verbs.medium) // we use med because it mostly has better flavor
                    return form_verbs.medium;
                else
                {
                    // this is actually a bitmask, but we will simplify quite a
                    // bit here and only use this for unarmed/forms. See
                    // melee_attack::set_attack_verb for the real thing.
                    const int dt = you.damage_type();
                    if (dt | DVORP_CLAWING || dt | DVORP_TENTACLE)
                        return "attack";
                }
                return "punch";
            }

            const launcher lt = _get_weapon_ammo_type(weapon);
            if (lt != AMMO_THROW)
                return lt == AMMO_SLING ? "fire" : "shoot";
            else if (weapon_reach(*weapon) > REACH_NONE)
                return "reach";
            else if (attack_cleaves(you))
                return "cleave";
            else
                return "hit"; // could use more subtype flavor Vs?
        }

        formatted_string quiver_description(bool short_desc=false) const override
        {
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;
            const item_def *weapon = you.weapon();

            // TODO: or just lightgrey?
            qdesc.textcolour(Options.status_caption_colour);

            if (!short_desc)
            {
                string verb = you.confused() ? "confused " : "";

                verb += quiver_verb();
                qdesc.cprintf("%s: %c) ", uppercase_first(verb).c_str(),
                                weapon ? index_to_letter(weapon->link) : '-');
            }

            const string prefix = weapon ? item_prefix(*weapon) : "";

            const int prefcol =
                menu_colour(weapon ? weapon->name(DESC_PLAIN) : "", prefix, "stats");
            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else if (prefcol != -1)
                qdesc.textcolour(prefcol);
            else
                qdesc.textcolour(LIGHTGREY);

            qdesc += weapon ? weapon->name(DESC_PLAIN, true)
                            : you.unarmed_attack_name();

            return qdesc;
        }

        void trigger(dist &t) override
        {
            // TODO: does it actually make sense to have this monster in
            // quiver.cc?

            target = t;

            if (you.confused() && target.needs_targeting())
            {
                mpr("You're too confused to aim your attacks!");
                return;
            }

            if (you.caught())
            {
                if (target.needs_targeting())
                    mprf("You cannot attack while %s.", held_status());
                else
                {
                    // assume that if a target was explicitly supplied, it was
                    // done intentionally
                    free_self_from_net();
                    you.turn_is_over = true;
                    return;
                }
                return;
            }

            bool targ_mid = false;

            const item_def *weapon = you.weapon();

            // TODO: is there any use case for allowing targeting in this case?
            // if this check isn't here, it is treated as a clumsy melee attack
            if (weapon && is_range_weapon(*weapon))
            {
                mprf("You do not have any ammo quivered for %s",
                                    you.weapon()->name(DESC_YOUR).c_str());
                return;
            }

            // This is redundant with a later check in fight_melee; but, the
            // way this check works, if the player overrides it once it won't
            // give a warning until they switch weapons. UI-wise, if there is
            // going to be a targeter it makes sense to show it first.
            if (target.needs_targeting() && !wielded_weapon_check(weapon))
                return;

            target.isEndpoint = true; // is this needed? imported from autofight code
            const reach_type reach_range = !weapon ? REACH_NONE
                                                    : weapon_reach(*weapon);

            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_HOSTILE;
            args.range = reach_range;
            args.top_prompt = quiver_description().tostring();
            args.self = confirm_prompt_type::cancel;

            unique_ptr<targeter> hitfunc;
            // Xom can give you cleaving status while wielding a reaching
            // weapon, just use the reach targeter for this case. (TODO:
            // show cleave effect in targeter.)
            if (attack_cleaves(you, -1) && reach_range < REACH_TWO)
                hitfunc = make_unique<targeter_cleave>(&you, you.pos());
            else
                hitfunc = make_unique<targeter_reach>(&you, reach_range);
            args.hitfunc = hitfunc.get();

            direction(target, args);
            t = target;

            if (!target.isValid)
            {
                if (target.isCancel)
                    canned_msg(MSG_OK);
                return;
            }

            if (target.isMe())
            {
                canned_msg(MSG_UNTHINKING_ACT);
                return;
            }

            const coord_def delta = target.target - you.pos();
            const int x_distance  = abs(delta.x);
            const int y_distance  = abs(delta.y);
            monster* mons = monster_at(target.target);
            // don't allow targeting of submerged monsters
            if (mons && mons->submerged())
                mons = nullptr;

            if (x_distance > reach_range || y_distance > reach_range)
            {
                mpr("Your weapon can't reach that far!");
                return;
            }

            if (feat_is_solid(env.grid(target.target)))
            {
                canned_msg(MSG_SOMETHING_IN_WAY);
                return;
            }

            // Failing to hit someone due to a friend blocking is infuriating,
            // shadow-boxing empty space is not (and would be abusable to wait
            // with no penalty).
            if (mons)
                you.apply_berserk_penalty = false;

            // Calculate attack delay now in case we have to apply it.
            const int attack_delay = you.attack_delay().roll();

            // Check for a monster in the way. If there is one, it blocks the reaching
            // attack 50% of the time, and the attack tries to hit it if it is hostile.
            // REACH_THREE entails smite targeting; this is a bit hacky in that
            // this is entirely for the sake of UNRAND_RIFT.
            if (reach_range < REACH_THREE && (x_distance > 1 || y_distance > 1))
            {
                const int x_first_middle = you.pos().x + (delta.x) / 2;
                const int y_first_middle = you.pos().y + (delta.y) / 2;
                const int x_second_middle = target.target.x - (delta.x) / 2;
                const int y_second_middle = target.target.y - (delta.y) / 2;
                const coord_def first_middle(x_first_middle, y_first_middle);
                const coord_def second_middle(x_second_middle, y_second_middle);

                if (!feat_is_reachable_past(env.grid(first_middle))
                    && !feat_is_reachable_past(env.grid(second_middle)))
                {
                    canned_msg(MSG_SOMETHING_IN_WAY);
                    return;
                }

                // Choose one of the two middle squares (which might be the same).
                const coord_def middle =
                    !feat_is_reachable_past(env.grid(first_middle)) ? second_middle :
                    !feat_is_reachable_past(env.grid(second_middle)) ? first_middle :
                random_choose(first_middle, second_middle);

                bool success = true;
                monster *midmons;
                if ((midmons = monster_at(middle))
                    && !midmons->submerged()
                    && coinflip())
                {
                    success = false;
                    target.target = middle;
                    mons = midmons;
                    targ_mid = true;
                    t = target;
                    if (mons->wont_attack())
                    {
                        // Let's assume friendlies cooperate.
                        mpr("You could not reach far enough!");
                        you.time_taken = attack_delay;
                        you.turn_is_over = true;
                        return;
                    }
                }

                if (success)
                    mpr("You reach to attack!");
                else
                {
                    mprf("%s is in the way.",
                         mons->observable() ? mons->name(DESC_THE).c_str()
                                            : "Something you can't see");
                }
            }

            if (mons == nullptr)
            {
                if (feat_is_solid(env.grid(target.target)) && !you.confused())
                    return;

                if (!force_player_cleave(target.target) && !you.fumbles_attack())
                {
                    if (x_distance <= 1 && y_distance <= 1)
                        mpr("You swing at nothing.");
                    else
                        mpr("You attack empty space.");
                }
                you.time_taken = attack_delay;
                you.turn_is_over = true;
            }
            else
            {
                // something to attack, let's do it:
                you.turn_is_over = true;
                if (!fight_melee(&you, mons) && targ_mid)
                {
                    // turn_is_over may have been reset to false by fight_melee, but
                    // a failed attempt to reach further should not be free; instead,
                    // charge the same as a successful attempt.
                    you.time_taken = attack_delay;
                    you.turn_is_over = true;
                }
                you.berserk_penalty = 0;
                you.apply_berserk_penalty = false;
            }

            return;
        }

        int get_item() const override
        {
            return you.equip[EQ_WEAPON];
        };

        vector<shared_ptr<action>> get_fire_order(bool allow_disabled=true, bool=false) const override
        {
            if (allow_disabled || is_enabled())
                return { make_shared<melee_action>() };
            else
                return { };
        }
    };

    /**
     * An ammo_action is an action that fires ammo from a slot in the
     * inventory. This covers both launcher-based firing, and throwing.
     */
    struct ammo_action : public action
    {
        // it could be simpler to have a distinct type for launcher ammo and
        // throwing ammo
        ammo_action(int slot=-1) : action(), ammo_slot(slot)
        {
        }

        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return ammo_slot == static_cast<const ammo_action &>(other).ammo_slot;
        }

        virtual bool launcher_check() const
        {
            if (ammo_slot < 0)
                return false;
            return _item_matches(you.inv[ammo_slot], (fire_type) 0xffff,
                you.weapon(), false);
        }

        bool do_inscription_check() const override
        {
            // need to also check the launcher's inscription here, in addition
            // to ammo
            if (!is_valid()) // sanity check
                return true;
            const item_def *weapon = you.weapon();
            const item_def& ammo = you.inv[ammo_slot];
            return action::do_inscription_check()
                && (!weapon
                    || is_launched(&you, weapon, ammo) != launch_retval::LAUNCHED
                    || check_warning_inscriptions(*weapon, OPER_FIRE));
        }

        virtual bool is_enabled() const override
        {
            if (!is_valid())
                return false;

            if (fire_warn_if_impossible(true))
                return false;

            if (!launcher_check())
                return false;

            // TODO: check inscriptions here? That code would need to be
            // refactored.
            return true;
        }

        virtual bool is_valid() const override
        {
            if (you.species == SP_FELID)
                return false;
            if (ammo_slot < 0 || ammo_slot >= ENDOFPACK)
                return false;
            const item_def& ammo = you.inv[ammo_slot];
            if (!ammo.defined())
                return false;

            if (_autoswitch_active())
            {
                // valid but potentially disabled. It seems like there could be
                // better ways of doing this given generalized quivers?
                return _autoswitch_ammo_check(ammo);
            }
            else
            {
                const item_def *weapon = you.weapon();
                return _item_matches(ammo, (fire_type) 0xffff, // TODO: ...
                                     weapon, false);
            }
        }

        bool is_targeted() const override
        {
            return !you.confused();
        }

        bool allow_autofight() const override
        {
            return is_enabled();
        }

        bool uses_mp() const override
        {
            return is_pproj_active();
        }

        void trigger(dist &t) override
        {
            target = t;
            if (!is_valid())
                return;
            if (!is_enabled())
            {
                // try autoswitching in case that's why it's disabled
                if (!_autoswitch_to_ranged(you.inv[ammo_slot]))
                    fire_warn_if_impossible(); // for messaging (TODO refactor; message about inscriptions?)
                return;
            }
            if (autofight_check() || !do_inscription_check())
                return;

            bolt beam;
            throw_it(beam, ammo_slot, &target);

            // TODO: eliminate this?
            you.m_quiver_history.on_item_fired(you.inv[ammo_slot], true);

            t = target; // copy back, in case they are different
        }

        virtual formatted_string quiver_description(bool short_desc) const override
        {
            ASSERT_RANGE(ammo_slot, -1, ENDOFPACK);
            // or error?
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;

            const item_def& quiver = you.inv[ammo_slot];
            ASSERT(quiver.link != NON_ITEM);
            // TODO: or just lightgrey?
            qdesc.textcolour(Options.status_caption_colour);
            const launch_retval projected = is_launched(&you, you.weapon(),
                                                                    quiver);
            if (!short_desc)
            {
                string verb = you.confused() ? "confused " : "";
                switch (projected)
                {
                    case launch_retval::FUMBLED:  verb += "toss (no damage)";  break;
                    case launch_retval::LAUNCHED: verb += "fire";  break;
                    case launch_retval::THROWN:   verb += "throw"; break;
                    case launch_retval::BUGGY:    verb += "bug";   break;
                }
                qdesc.cprintf("%s: ", uppercase_first(verb).c_str());
            }

            // TODO: I don't actually know what this prefix stuff is
            const string prefix = item_prefix(quiver);

            const int prefcol =
                menu_colour(quiver.name(DESC_PLAIN), prefix, "stats");
            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else if (prefcol != -1)
                qdesc.textcolour(prefcol);
            else
                qdesc.textcolour(LIGHTGREY);

            if (short_desc && quiver.sub_type == MI_SLING_BULLET)
            {
                qdesc.cprintf("%d bullet%s", quiver.quantity,
                                quiver.quantity > 1 ? "s" : "");
            }
            else
                qdesc += quiver.name(DESC_PLAIN, true);

            return qdesc;
        }

        int get_item() const override
        {
            return ammo_slot;
        }

        shared_ptr<action> find_replacement() const override
        {
            return find_action_from_launcher(you.weapon());
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool ignore_inscription=false) const override
        {
            vector<int> fire_order;
            _get_item_fire_order(fire_order, ignore_inscription, you.weapon(), true);

            vector<shared_ptr<action>> result;

            for (auto i : fire_order)
            {
                auto a = make_shared<ammo_action>(i);
                if (a->is_valid() && (allow_disabled || a->is_enabled()))
                    result.push_back(move(a));
                // TODO: allow arbitrary items with +f to get added here? I
                // have had some trouble getting this to work
            }
            return result;
        }

    protected:
        int ammo_slot;
    };

    // for fumble throwing / tossing
    struct fumble_action : public ammo_action
    {
        fumble_action(int slot=-1) : ammo_action(slot)
        {
        }

        void save(CrawlHashTable &save_target) const override; // defined below

        // uses ammo_action fire order

        bool launcher_check() const override
        {
            return true;
        }

        bool is_valid() const override
        {
            if (you.species == SP_FELID)
                return false;
            if (ammo_slot < 0 || ammo_slot >= ENDOFPACK)
                return false;
            const item_def& ammo = you.inv[ammo_slot];
            if (!ammo.defined())
                return false;

            // slightly weird looking, but this ensures that only tossing
            // is allowed with this class. (I guess in principle it could be
            // doable to let this class toss anything, but I'm not going to
            // do that.)
            if (ammo_action::is_valid())
                return false;
            return true;
        }
    };

    static bool _spell_needs_manual_targeting(spell_type s)
    {
        switch (s)
        {
        case SPELL_FULMINANT_PRISM:
        case SPELL_GRAVITAS: // will autotarget to a monster if allowed, should we allow?
        case SPELL_PASSWALL: // targeted, but doesn't make sense with autotarget
        case SPELL_GOLUBRIAS_PASSAGE: // targeted, but doesn't make sense with autotarget
            return true;
        default:
            return false;
        }
    }

    // for spells that are targeted, but should skip the lua target selection
    // pass for one reason or another
    static bool _spell_no_autofight_targeting(spell_type s)
    {
        // XX perhaps all spells should just use direction chooser target
        // selection? This is how automagic.lua handled it.
        auto h = find_spell_targeter(s, 100, LOS_RADIUS); // dummy values
        // use smarter direction chooser target selection for spells that have
        // explosition or cloud patterning, like fireball. This allows them
        // to autoselect targets at the edge of their range, which autofire
        // wouldn't handle.
        if (!Options.simple_targeting && h && h->can_affect_outside_range())
            return true;

        switch (s)
        {
        case SPELL_LRD: // skip initial autotarget for LRD so that it doesn't
                        // fix on a close monster that can't be targeted. I'm
                        // not quite sure what the right thing to do is?
                        // An alternative would be to just error if the closest
                        // monster can't be autotargeted, or pop out to manual
                        // targeting for that case; the behavior involved in
                        // listing it here just finds the closest targetable
                        // monster.
        case SPELL_INVISIBILITY: // targeted, but not to enemies. (Should this allow quivering at all?)
        case SPELL_APPORTATION: // Apport doesn't target monsters at all
            return true;
        default:
            return _spell_needs_manual_targeting(s);
        }
    }

    struct spell_action : public action
    {
        spell_action(spell_type s = SPELL_NO_SPELL)
            : spell(s), enabled_cache(false), col_cache(COL_USELESS)
        {
            invalidate();
        };

        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return spell == static_cast<const spell_action &>(other).spell;
        }

        bool is_dynamic_targeted() const
        {
            // TODO: what spells does this miss?
            return !!(get_spell_flags(spell) & spflag::targeting_mask);
        }

        void invalidate() override
        {
            // we cache enabled status and color because these calls are
            // extremely side-effect-y, and can crash if called at the wrong
            // time.
            enabled_cache = can_cast_spells(true)
                                    && !spell_is_useless(spell, true, false);
            // this imposes excommunication colors
            if (!enabled_cache)
                col_cache = COL_USELESS;
            else
            {
                col_cache = spell_highlight_by_utility(spell,
                                failure_rate_colour(spell), true, false);
            }
        }

        bool is_enabled() const override
        {
            return enabled_cache;
        }

        bool is_valid() const override
        {
            return is_valid_spell(spell) && you.has_spell(spell);
        }

        bool is_targeted() const override
        {
            return is_dynamic_targeted() || spell_has_targeter(spell);
        }

        bool use_autofight_targeting() const override
        {
            return is_dynamic_targeted()
                                && !_spell_no_autofight_targeting(spell);
        }

        bool allow_autofight() const override
        {
            if (!is_enabled())
                return false;
            if (_spell_needs_manual_targeting(spell))
                return false;
            if (spell_is_direct_attack(spell))
                return true;
            if (spell == SPELL_FOXFIRE) // not a direct attack, but has sensible autofight behavior
                return true;
            return false;
        }

        bool uses_mp() const override
        {
            return is_valid();
        }

        void trigger(dist &t) override
        {
            // note: we don't do the enabled check here, because cast_a_spell
            // duplicates it and does appropriate messaging
            // TODO refactor?
            if (!is_valid())
                return;

            target = t;

            // TODO: how to handle these in the fire interface?
            if (_spell_needs_manual_targeting(spell))
            {
                target.target = coord_def(-1,-1);
                target.find_target = false; // default, but here for clarity's sake
                target.interactive = true;
            }
            else if (_spell_no_autofight_targeting(spell))
            {
                target.target = coord_def(-1,-1);
                target.find_target = true;
            }
            else if (!is_dynamic_targeted())
                target.target = you.pos(); // hax -- never trigger static targeters
                                           // unless interactive is set.
                                           // will need to be fixed if `z` ever
                                           // calls here

            // don't do the range check check if doing manual firing. (It's
            // a bit hacky to condition this on whether there's a fire context...)
            const bool do_range_check = !target.fire_context;
            if (autofight_check())
                return;

            cast_a_spell(do_range_check, spell, &target);
            if (target.find_target && !target.isValid && !target.fire_context)
            {
                // It would be entirely possible to force manual targeting for
                // this case; I think it's not what players would expect so I'm
                // not doing it for now.
                // TODO: more consistency with autofight.lua messaging?
                mpr("Can't find an automatic target! Use Z to cast.");
            }
            t = target; // copy back, in case they are different
        }

        int quiver_color() const override
        {
            return col_cache;
        }

        vector<tile_def> get_tiles() const override
        {
            return { tile_def(get_spell_tile(spell)) };
        }

        formatted_string quiver_description(bool short_desc) const override
        {
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;

            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("Cast: ");

            qdesc.textcolour(quiver_color());

            // TODO: is showing the spell letter useful?
            qdesc.cprintf("%s", spell_title(spell));
            if (spell == SPELL_SANDBLAST)
                qdesc.cprintf(" (stones: %d)", sandblast_find_ammo().first);

            if (fail_severity(spell) > 0)
            {
                qdesc.cprintf(" (%s)",
                        failure_rate_to_string(raw_spell_fail(spell)).c_str());
            }

            return qdesc;
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool=false) const override
        {
            // goes by letter order
            vector<shared_ptr<action>> result;
            for (int i = 0; i < 52; i++)
            {
                const char letter = index_to_letter(i);
                const spell_type s = get_spell_by_letter(letter);
                auto a = make_shared<spell_action>(s);
                if (a->is_valid()
                    && (allow_disabled || a->is_enabled())
                    // some extra stuff for fire order in particular: don't
                    // cycle to spells that are dangerous to past or forbidden.
                    // These can still be force-quivered.
                    && fail_severity(s) < Options.fail_severity_to_quiver
                    && spell_highlight_by_utility(s, COL_UNKNOWN) != COL_FORBIDDEN)
                {
                    result.push_back(move(a));
                }
            }
            return result;
        }

    private:
        spell_type spell;
        bool enabled_cache;
        int col_cache;
    };

    // stuff that is silly to quiver. Basically four (overlapping) cases:
    // * one-off things that are implemented as abilities because that's what
    //   you do with random stuff that doesn't fit neatly into any triggerable
    //   type. E.g. abandon religion, choose ancestor type, etc.
    // * "stop X" type abilities, these just clutter up the list
    // * capstone abilities + stuff with a significant cost (revivify etc)
    // * abilities that vanish when triggered. E.g. fly *might* make more sense
    //   if it was a toggle, but that's not how it's implemented
    static bool _pseudoability(ability_type a)
    {
        if (   static_cast<int>(a) >= ABIL_FIRST_SACRIFICE
                    && static_cast<int>(a) <= ABIL_FINAL_SACRIFICE
            || static_cast<int>(a) >= ABIL_HEPLIAKLQANA_FIRST_TYPE
                    && static_cast<int>(a) <= ABIL_HEPLIAKLQANA_LAST_TYPE)
        {
            return true;
        }

        switch (a)
        {
        case ABIL_END_TRANSFORMATION:
        case ABIL_CANCEL_PPROJ:
        case ABIL_EXSANGUINATE:
        case ABIL_REVIVIFY:
        case ABIL_EVOKE_TURN_VISIBLE:
        case ABIL_ZIN_DONATE_GOLD:
        case ABIL_TSO_BLESS_WEAPON:
        case ABIL_KIKU_BLESS_WEAPON:
        case ABIL_KIKU_GIFT_NECRONOMICON:
        case ABIL_SIF_MUNA_FORGET_SPELL:
        case ABIL_LUGONU_BLESS_WEAPON:
        case ABIL_BEOGH_GIFT_ITEM:
        case ABIL_ASHENZARI_CURSE:
        case ABIL_RU_REJECT_SACRIFICES:
        case ABIL_HEPLIAKLQANA_IDENTITY:
        case ABIL_STOP_RECALL:
        case ABIL_RENOUNCE_RELIGION:
        case ABIL_CONVERT_TO_BEOGH:
        // not entirely pseudo, but doesn't make a lot of sense to quiver:
        case ABIL_FLY:
        case ABIL_TRAN_BAT:
            return true;
        default:
            return false;
        }
    }

    struct ability_action : public action
    {
        ability_action(ability_type a = ABIL_NON_ABILITY) : ability(a) { };
        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return ability == static_cast<const ability_action &>(other).ability;
        }

        bool is_valid() const override
        {
            // special case usk abilities to ignore piety: they come and go so
            // rapidly that they are effectively unquiverable otherwise.
            // Should any other god powers get this treatment?
            if (is_religious_ability(ability) && you_worship(GOD_USKAYAW))
            {
                auto *p = god_power_from_ability(ability);
                return p && god_power_usable(*p, true, false);
            }
            return player_has_ability(ability, true);
        }

        bool is_enabled() const override
        {
            // TODO: _check_ability_dangerous?
            return is_valid() && check_ability_possible(ability, true);
        }

        bool is_targeted() const override
        {
            // hard-coded list of abilities that have a targeter
            // there is no general way of getting this?
            // TODO: implement static targeters for relevant abilities (which
            // is pretty much everything)
            switch (ability)
            {
            case ABIL_HOP:
            case ABIL_ROLLING_CHARGE:
            case ABIL_SPIT_POISON:
            case ABIL_BREATHE_ACID:
            case ABIL_BREATHE_FIRE:
            case ABIL_BREATHE_FROST:
            case ABIL_BREATHE_POISON:
            case ABIL_BREATHE_POWER:
            case ABIL_BREATHE_STEAM:
            case ABIL_BREATHE_MEPHITIC:
            case ABIL_DAMNATION:
            case ABIL_ZIN_IMPRISON:
            case ABIL_MAKHLEB_MINOR_DESTRUCTION:
            case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
            case ABIL_LUGONU_BANISH:
            case ABIL_BEOGH_SMITING:
            case ABIL_DITHMENOS_SHADOW_STEP:
            case ABIL_QAZLAL_UPHEAVAL:
            case ABIL_RU_POWER_LEAP:
            case ABIL_USKAYAW_LINE_PASS:
            case ABIL_USKAYAW_GRAND_FINALE:
            case ABIL_WU_JIAN_WALLJUMP:
                return true;
            default:
                return false;
            }
        }

        bool allow_autofight() const override
        {
            if (!is_enabled())
                return false;
            switch (ability)
            {
            case ABIL_ROLLING_CHARGE: // TODO: disable under nomove?
            case ABIL_RU_POWER_LEAP: // disable under nomove, or altogether?
            case ABIL_SPIT_POISON:
            case ABIL_BREATHE_ACID:
            case ABIL_BREATHE_FIRE:
            case ABIL_BREATHE_FROST:
            case ABIL_BREATHE_POISON:
            case ABIL_BREATHE_POWER:
            case ABIL_BREATHE_STEAM:
            case ABIL_BREATHE_MEPHITIC:
            case ABIL_DAMNATION:
            case ABIL_MAKHLEB_MINOR_DESTRUCTION:
            case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
            case ABIL_LUGONU_BANISH:
            case ABIL_BEOGH_SMITING:
            case ABIL_QAZLAL_UPHEAVAL:
                return true;
            default:
                return false;
            }
        }

        bool use_autofight_targeting() const override
        {
            return false;
        }

        bool uses_mp() const override
        {
            return ability_mp_cost(ability) > 0;
        }

        void trigger(dist &t) override
        {
            if (!is_valid())
                return;

            if (!is_enabled())
            {
                check_ability_possible(ability, false);
                return;
            }
            target = t;
            target.find_target = true;

            if (autofight_check())
                return;

            talent tal = get_talent(ability, false);
            activate_talent(tal, &target);

            // TODO: does non-targeted case come up?
            if (target.isCancel && !target.interactive && is_targeted())
                mprf("No targets found!");

            target = t;

            t = target; // copy back, in case they are different
        }

        formatted_string quiver_description(bool short_desc) const override
        {
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;

            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("Abil: ");

            qdesc.textcolour(quiver_color());
            qdesc.cprintf("%s", ability_name(ability));

            return qdesc;
        }

        vector<tile_def> get_tiles() const override
        {
            return { tile_def(tileidx_ability(ability)) };
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool=false) const override
        {
            vector<talent> talents = your_talents(false, true, true);
            // goes by letter order
            vector<shared_ptr<action>> result;

            for (const auto &tal : talents)
            {
                if (_pseudoability(tal.which))
                    continue;
                auto a = make_shared<ability_action>(tal.which);
                if (a->is_valid() && (allow_disabled || a->is_enabled()))
                    result.push_back(move(a));
            }
            return result;
        }

    private:
        ability_type ability;
    };


    // TODO: generalize to misc evokables? Code should be similar if targeting
    // can be easily implemented
    struct wand_action : public action
    {
        wand_action(int slot=-1) : action(), wand_slot(slot)
        {
        }

        virtual void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return wand_slot == static_cast<const wand_action &>(other).wand_slot;
        }

        bool is_enabled() const override
        {
            return evoke_check(wand_slot, true);
        }

        // n.b. implementing do_inscription_check for OPER_EVOKE is not needed
        // for this class as long as it is checked in evoke_item.

        virtual bool is_valid() const override
        {
            if (wand_slot < 0 || wand_slot >= ENDOFPACK)
                return false;
            const item_def& wand = you.inv[wand_slot];
            if (!wand.defined() || wand.base_type != OBJ_WANDS)
                return false;
            return true;
        }

        bool is_targeted() const override
        {
            return true;
        }

        bool allow_autofight() const override
        {
            if (!is_valid() || !is_enabled()) // need to check item validity
                return false;

            switch (you.inv[wand_slot].sub_type)
            {
            case WAND_DIGGING:     // non-damaging wands
            case WAND_POLYMORPH:
            case WAND_ENSLAVEMENT:
            case WAND_PARALYSIS:
                return false;
            default:
                return true;
            }
        }

        // TOOD: uses_mp for wand mp mutation? Because this mut no longer forces
        // mp use, the result is somewhat weird

        void trigger(dist &t) override
        {
            target = t;
            if (!is_valid())
                return;

            if (!is_enabled())
            {
                evoke_check(wand_slot); // for messaging
                return;
            }

            // to apply smart targeting behavior for iceblast; should have no
            // impact on other wands
            target.find_target = true;
            if (autofight_check() || !do_inscription_check())
                return;

            evoke_item(wand_slot, &target);

            t = target; // copy back, in case they are different
        }

        virtual string quiver_verb() const
        {
            return "Zap";
        }

        virtual formatted_string quiver_description(bool short_desc) const override
        {
            ASSERT_RANGE(wand_slot, -1, ENDOFPACK);
            if (!is_valid())
                return action::quiver_description(short_desc);
            formatted_string qdesc;

            const item_def& quiver = you.inv[wand_slot];
            ASSERT(quiver.link != NON_ITEM);
            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("%s: ", quiver_verb().c_str());

            qdesc.textcolour(quiver_color());
            qdesc += quiver.name(DESC_PLAIN, true);
            return qdesc;
        }

        int get_item() const override
        {
            return wand_slot;
        }

        virtual vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool ignore_inscription=false) const override
        {
            // go by pack order
            vector<shared_ptr<action>> result;
            for (int slot = 0; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<wand_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled())
                    // for fire order, treat =f inscription as disabling
                    && (ignore_inscription || _fireorder_inscription_ok(slot) != MB_FALSE)
                    // skip digging for fire cycling, it seems kind of
                    // non-useful? Can still be force-quivered from inv.
                    // (Maybe do this with autoinscribe?)
                    && you.inv[slot].sub_type != WAND_DIGGING)
                {
                    result.push_back(move(w));
                }
            }
            return result;
        }

    protected:
        int wand_slot;
    };

    static bool _misc_needs_manual_targeting(int subtype)
    {
            // autotargeting seems less useful on the others. Maybe this should
            // be configurable somehow?
        return subtype != MISC_PHIAL_OF_FLOODS;
    }

    struct misc_action : public wand_action
    {
        misc_action(int slot=-1) : wand_action(slot)
        {
        }

        void save(CrawlHashTable &save_target) const override; // defined below

        bool is_valid() const override
        {
            if (wand_slot < 0 || wand_slot >= ENDOFPACK)
                return false;
            const item_def& wand = you.inv[wand_slot];
            // MISC_ZIGGURAT is valid (so can be force quivered) but is skipped
            // in the fire order
            if (!wand.defined() || wand.base_type != OBJ_MISCELLANY)
                return false;
            return true;
        }

        // equals should work without override

        bool use_autofight_targeting() const override
        {
            // all of these use the spell direction chooser
            return false;
        }

        bool allow_autofight() const override
        {
            if (!is_valid() || !is_enabled()) // need to check item validity
                return false;

            switch (you.inv[wand_slot].sub_type)
            {
            case MISC_ZIGGURAT:     // non-damaging misc items
            case MISC_BOX_OF_BEASTS:
            case MISC_HORN_OF_GERYON:
            case MISC_QUAD_DAMAGE:
            case MISC_PHANTOM_MIRROR:
                return false;
            default:
                return true;
            }
        }

        void trigger(dist &t) override
        {
            if (is_valid()
                && _misc_needs_manual_targeting(you.inv[wand_slot].sub_type))
            {
                t.interactive = true;
            }
            wand_action::trigger(t);
        }

        string quiver_verb() const override
        {
            ASSERT(is_valid());
            switch (you.inv[wand_slot].sub_type)
            {
            case MISC_TIN_OF_TREMORSTONES:
                return "Throw";
            case MISC_HORN_OF_GERYON:
                return "Blow";
            case MISC_BOX_OF_BEASTS:
                return "Open";
            default:
                return "Evoke";
            }
        }

        bool is_targeted() const override
        {
            if (!is_valid())
                return false;
            switch (you.inv[wand_slot].sub_type)
            {
            case MISC_PHIAL_OF_FLOODS:
            case MISC_LIGHTNING_ROD:
            case MISC_PHANTOM_MIRROR:
                return true;
            default:
                return false;
            }
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool ignore_inscription=false) const override
        {
            // go by pack order
            vector<shared_ptr<action>> result;
            for (int slot = 0; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<misc_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled())
                    && (ignore_inscription || _fireorder_inscription_ok(slot) != MB_FALSE)
                    // TODO: autoinscribe =f?
                    && you.inv[slot].sub_type != MISC_ZIGGURAT)
                {
                    result.push_back(move(w));
                }
            }
            return result;
        }
    };

    struct artefact_evoke_action : public wand_action
    {
        artefact_evoke_action(int slot=-1) : wand_action(slot)
        {
        }

        // equals should work without override

        void save(CrawlHashTable &save_target) const override; // defined below

        bool is_valid() const override
        {
            if (wand_slot < 0 || wand_slot >= ENDOFPACK)
                return false;

            const item_def& item = you.inv[wand_slot];
            if (!item.defined() || !is_unrandom_artefact(item)
                                || !item_is_equipped(item))
            {
                return false;
            }

            const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

            if (!entry || !(entry->evoke_func || entry->targeted_evoke_func))
                return false;

            return true;
        }

        // TODO: there's no generic API for this, and it would be a pain to
        // add one...and there's only three of these. So we do a bit of dumb
        // code duplication. Maybe this could eventually be moved into
        // evoke_check?
        bool artefact_evoke_check(bool quiet) const
        {
            if (!is_valid())
                return false;

            switch (you.inv[wand_slot].unrand_idx)
            {
            case UNRAND_DISPATER:
                return enough_hp(14, quiet) && enough_mp(4, quiet); // TODO: code duplication...
            case UNRAND_OLGREB:
                return enough_mp(4, quiet); // TODO: code duplication...
            default:
                return true; // UNRAND_ASMODEUS has no up-front cost
            }
        }

        bool is_enabled() const override
        {
            return artefact_evoke_check(true);
        }

        bool use_autofight_targeting() const override
        {
            // all of these use the spell direction chooser
            return false;
        }

        bool allow_autofight() const override
        {
            if (!is_valid() || !is_enabled()) // need to check item validity
                return false;

            switch (you.inv[wand_slot].unrand_idx)
            {
            case UNRAND_OLGREB: // only indirect damage
            case UNRAND_ASMODEUS:
                return false;
            default:
                return true;
            }
        }

        bool is_targeted() const override
        {
            if (!is_valid())
                return false;
            // is_valid checks the preconditions for this:
            return get_unrand_entry(you.inv[wand_slot].unrand_idx)->targeted_evoke_func;
        }

        string quiver_verb() const override
        {
            return "Evoke";
        }

        void trigger(dist &t) override
        {
            target = t;
            if (!is_valid())
                return;

            if (!artefact_evoke_check(false))
                return;

            target.find_target = true;
            if (autofight_check() || !do_inscription_check())
                return;

            evoke_item(wand_slot, &target);

            t = target; // copy back, in case they are different
        }


        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool ignore_inscription=false) const override
        {
            // go by pack order
            vector<shared_ptr<action>> result;
            for (int slot = 0; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<artefact_evoke_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled())
                    && (ignore_inscription || _fireorder_inscription_ok(slot) != MB_FALSE))
                {
                    result.push_back(move(w));
                }
            }
            return result;
        }
    };


    void action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "action";
    }

    void ammo_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "ammo_action";
        save_target["param"] = ammo_slot;
    }

    void fumble_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "fumble_action";
        save_target["param"] = ammo_slot;
    }

    void spell_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "spell_action";
        save_target["param"] = static_cast<int>(spell);
    }

    void ability_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "ability_action";
        save_target["param"] = static_cast<int>(ability);
    }

    void wand_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "wand_action";
        save_target["param"] = static_cast<int>(wand_slot);
    }

    void misc_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "misc_action";
        save_target["param"] = static_cast<int>(wand_slot);
    }

    void artefact_evoke_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "artefact_evoke_action";
        save_target["param"] = static_cast<int>(wand_slot);
    }

    // not currently used, but for completeness
    void melee_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "melee_action";
    }

    static shared_ptr<action> _load_action(CrawlHashTable &source)
    {
        // pretty minimal: but most actions that I can think of shouldn't need
        // a lot of effort to save. Something to tell you the type, and a
        // single value that is usually more or less an int. Using a hashtable
        // here is future proofing.

        // save compat (or bug compat): initialize to an invalid action if we
        // are missing the keys altogether
        if (!source.exists("type") || !source.exists("param"))
            return make_shared<ammo_action>(-1);

        const string &type = source["type"].get_string();
        const int param = source["param"].get_int();

        // is there something more elegant than this?
        if (type == "ammo_action")
            return make_shared<ammo_action>(param);
        else if (type == "spell_action")
            return make_shared<spell_action>(static_cast<spell_type>(param));
        else if (type == "ability_action")
            return make_shared<ability_action>(static_cast<ability_type>(param));
        else if (type == "wand_action")
            return make_shared<wand_action>(param);
        else if (type == "misc_action")
            return make_shared<misc_action>(param);
        else if (type == "artefact_evoke_action")
            return make_shared<artefact_evoke_action>(param);
        else if (type == "fumble_action")
            return make_shared<fumble_action>(param);
        else if (type == "melee_action")
            return make_shared<melee_action>();
        else
            return make_shared<action>();
    }

    shared_ptr<action> find_action_from_launcher(const item_def *item)
    {
        // Felids have no use for launchers or ammo.
        if (you.species == SP_FELID)
        {
            auto result = make_shared<ammo_action>(-1);
            result->error = "You can't grasp things well enough to shoot them.";
            return result;
        }

        int slot = -1;

        const int cur_launcher_item = you.launcher_action.get()->get_item();
        const int cur_quiver_item = you.quiver_action.get()->get_item();

        if (cur_launcher_item >= 0 && you.inv[cur_launcher_item].defined()
            && _item_matches(you.inv[cur_launcher_item], FIRE_LAUNCHER, item, false))
        {
            // prefer to keep the current ammo if not changing weapon types
            slot = cur_launcher_item;
        }
        else if (cur_quiver_item >= 0 && you.inv[cur_quiver_item].defined()
            && _item_matches(you.inv[cur_quiver_item], FIRE_LAUNCHER, item, false))
        {
            // if the right item type is currently present in the main quiver,
            // use that
            slot = cur_quiver_item;
        }
        else
        {
            // otherwise, find the last fired ammo for this launcher. (This is
            // an awful lot of effort to choose correctly between stones and
            // bullets...)
            slot = you.m_quiver_history.get_last_ammo(item);
        }

        // Finally, try looking at the fire order.
        if (slot == -1)
        {
            vector<int> order;
            _get_item_fire_order(order, false, item, false);
            if (!order.empty())
                slot = order[0];
        }

        auto result = make_shared<ammo_action>(slot);

        // if slot is still -1, we have failed, and the fire order is
        // empty for some reason. We should therefore populate the `error`
        // field for result.
        if (slot == -1)
        {
            vector<int> full_fire_order;
            _get_item_fire_order(full_fire_order, true, item, false);

            if (full_fire_order.empty())
                result->error = "No suitable missiles.";
            else
            {
                const int skipped_item = full_fire_order[0];
                if (skipped_item < Options.fire_items_start)
                {
                    result->error = make_stringf(
                        "Nothing suitable (fire_items_start = '%c').",
                        index_to_letter(Options.fire_items_start));
                }
                else
                {
                    result->error = make_stringf(
                        "Nothing suitable (ignored '=f'-inscribed item on '%c').",
                        index_to_letter(skipped_item));
                }
            }
        }

        return result;
    }

    // initialize as invalid, not empty
    action_cycler::action_cycler() : current(make_shared<ammo_action>(-1)) { }

    void action_cycler::save(const string key) const
    {
        auto &target = you.props[key].get_table();
        get()->save(target);
        CrawlVector &history_vec = you.props[key]["history"].get_vector();
        history_vec.clear();
        for (auto &a : history)
        {
            CrawlStoreValue v;
            a->save(v.get_table());
            history_vec.push_back(v);
        }
    }

    void action_cycler::load(const string key)
    {
        if (!you.props.exists(key))
        {
            // some light save compat: if there is no prop, attempt to fill
            // in the quiver from whatever is wielded -- will select launcher
            // ammo if applicable, or throwing.
            set(find_action_from_launcher(you.weapon()));
            if (!get()->is_valid())
                cycle();
            save(key);
        }

        auto &target = you.props[key].get_table();
        set(_load_action(target));
        CrawlVector &history_vec = you.props[key]["history"].get_vector();
        history.clear();
        for (auto &val : history_vec)
        {
            auto a = _load_action(val.get_table());
            history.push_back(move(a));
        }
    }

    /**
     * Returns the most recent valid action in the quiver history. May return
     * nullptr if no valid actions are found.
     */
    shared_ptr<action> action_cycler::find_last_valid()
    {
        for (auto it = history.rbegin(); it != history.rend(); ++it)
        {
            if ((*it) && (*it)->is_valid())
                return *it;
        }
        return nullptr;
    }

    /**
     * Set the current action for this action_cycler. If the provided action
     * is null, a valid `action` will be used (guaranteeing that the stored
     * action is never nullptr), corresponding to an empty quiver.
     *
     * @param new_act the action to fill in. nullptr is safe.
     * @return whether the action changed as a result of the call.
     */
    bool action_cycler::set(const shared_ptr<action> new_act)
    {
        auto n = new_act ? new_act : make_shared<action>();

        const bool diff = *n != *get();
        auto old = move(current);
        current = move(n);

        if (diff)
        {
            // only store the most recently used instance of any action in the
            // history
            history.erase(remove(history.begin(), history.end(), old), history.end());
            // this may push back an invalid action, which is useful for all
            // sorts of reasons
            history.push_back(move(old));

            if (history.size() > 6) // 6 chosen arbitrarily
                history.erase(history.begin());

            // side effects, ugh. Update the fire history, and play a sound
            // if needed. TODO: refactor so this is less side-effect-y
            // somehow?
            const int item_slot = get()->get_item();
            if (item_slot >= 0 && you.inv[item_slot].defined())
            {
                const item_def item = you.inv[item_slot];

                quiver::launcher t = quiver::AMMO_THROW;
                const item_def *weapon = you.weapon();
                if (weapon && item.launched_by(*weapon))
                    t = quiver::_get_weapon_ammo_type(weapon);

                you.m_quiver_history.set_quiver(you.inv[item_slot], t);
            }
#ifdef USE_SOUND
            parse_sound(CHANGE_QUIVER_SOUND);
#endif
        }
        set_needs_redraw();
        return diff;
    }

    static bool _is_currently_launched_ammo(int slot)
    {
        const item_def *weapon = you.weapon();
        return weapon && slot >= 0 && you.inv[slot].defined()
                                        && you.inv[slot].launched_by(*weapon);
    }

    // only reacts to ammo launched by the current weapon, or empty quiver
    // note that the action may still be valid on its own terms when this
    // returns true...
    bool launcher_action_cycler::is_empty() const
    {
        if (action_cycler::is_empty())
            return true;
        return !_is_currently_launched_ammo(get()->get_item());
    }

    bool launcher_action_cycler::set(const shared_ptr<action> new_act)
    {
        if (new_act &&
            (_is_currently_launched_ammo(new_act->get_item())
            || *new_act == action()))
        {
            return action_cycler::set(new_act);
        }
        else
            set_needs_redraw();
        return false;
    }

    void launcher_action_cycler::set_needs_redraw()
    {
        action_cycler::set_needs_redraw();
        you.wield_change = true;
    }

    /**
     * Set the action for this object based on another action_cycler. Guarantees
     * not null.
     *
     * @return whether the action changed.
     */
    bool action_cycler::set(const action_cycler &other)
    {
        const bool diff = current != other.current;
        // don't use regular set: avoid all the side effects when importing
        // from another action cycler. (Used in targeting.)
        current = other.get();
        set_needs_redraw();
        return diff;
    }

    /**
     * Get the currently chosen action for this action_cycler.
     * @return a shared_ptr of the action. Guarantees not null.
     */
    shared_ptr<action> action_cycler::get() const
    {
        ASSERT(current); // sanity check: `set` prevents nullptr

        return current;
    }

    bool action_cycler::spell_is_quivered(spell_type s) const
    {
        // validity check??
        return *get() == spell_action(s);
    }


    bool action_cycler::item_is_quivered(const item_def &item)
    {
        return in_inventory(item) && item_is_quivered(item.link);
    }

    bool action_cycler::item_is_quivered(int item_slot) const
    {
        return item_slot >= 0 && item_slot < ENDOFPACK
                              && get()->is_valid()
                              && get()->get_item() == item_slot;
    }

    static shared_ptr<action> _get_next_action_type(shared_ptr<action> a, int dir, bool allow_disabled)
    {
        // this all seems a bit messy

        // Construct the type order.
        vector<shared_ptr<action>> action_types;
        action_types.push_back(make_shared<ammo_action>(-1));
        action_types.push_back(make_shared<wand_action>(-1));
        action_types.push_back(make_shared<misc_action>(-1));
        action_types.push_back(make_shared<artefact_evoke_action>(-1));
        action_types.push_back(make_shared<spell_action>(SPELL_NO_SPELL));
        action_types.push_back(make_shared<ability_action>(ABIL_NON_ABILITY));

        if (dir < 0)
            reverse(action_types.begin(), action_types.end());

        size_t i = 0;
        // skip_first: true just in case the current action is valid and we
        // need to move on from it.
        bool skip_first = true;

        if (!a)
            skip_first = false; // and i = 0
        else
        {
            // find the type of a
            auto &a_ref = *a;
            for (i = 0; i < action_types.size(); i++)
            {
                auto rep = action_types[i];
                if (!rep) // should be impossible
                    continue;
                // we do it this way in order to silence some clang warnings
                auto &rep_ref = *rep;
                if (typeid(rep_ref) == typeid(a_ref))
                    break;
            }
            // unknown action type -- treat it like null. (Handles `action`.)
            if (i >= action_types.size())
            {
                i = 0;
                skip_first = false;
            }
        }

        // TODO: this logic could probably be reimplemented using only mod
        // math, but using rotate does make the final iteration very clean
        if (skip_first)
            i = (i + 1) % action_types.size();
        rotate(action_types.begin(), action_types.begin() + i, action_types.end());

        // now find the first result that is valid in this order. Will cycle
        // back to the current action type if nothing else works.
        for (auto result : action_types)
        {
            auto n = result->find_next(dir, allow_disabled, false);
            if (n && n->is_valid())
                return n;
        }

        // we've gone through everything -- somehow there are no valid actions,
        // not even using a
        return nullptr;
    }

    /**
     * Find the next item in the fire order relative to the current item.
     * @param dir negative for reverse, non-negative for forward
     * @param allow_disabled include valid, but disabled items in the fire order
     * @return the resulting action, which may be invalid or an empty quiver.
     *         Guaranteed to be not nullptr
     */
    shared_ptr<action> action_cycler::next(int dir, bool allow_disabled)
    {
        // first try the next action of the same type
        shared_ptr<action> result = get()->find_next(dir, allow_disabled, false);
        // then, try to find a different action type
        if (!result || !result->is_valid())
            result = _get_next_action_type(get(), dir, allow_disabled);

        // no valid actions, return an (invalid) empty-quiver action
        if (!result)
            return make_shared<ammo_action>(-1);

        return result;
    }

    /**
     * Cycle to the next action in the fire order. Guaranteed to result in a
     * non-nullptr action being quivered, but it may be an empty quiver or
     * invalid.
     * @param allow_disabled include valid, but disabled items in the fire order
     * @return whether the action changed
     */
    bool action_cycler::cycle(int dir, bool allow_disabled)
    {
        return set(next(dir, allow_disabled));
    }

    void action_cycler::on_actions_changed()
    {
        if (!get()->is_valid())
        {
            auto r = get()->find_replacement();
            if (r && r->is_valid())
                set(r);
        }
        set_needs_redraw();
    }

    void action_cycler::set_needs_redraw()
    {
        get()->invalidate();
        // TODO: abstract from `you`
        you.redraw_quiver = true;
    }

    /**
     * Given an inventory slot containing ammo, return an appropriate throwing
     * or firing action. If `slot` doesn't contain ammo, returns either an
     * invalid action or a fumble action based on `force`.
     *
     * @param slot the inventory slot number to use.
     * @param force whether to force non-ammo items to be throwable via
     *              fumble throwing.
     * @return the resulting action. May be invalid, or nullptr on an error.
     */
    shared_ptr<action> ammo_to_action(int slot, bool force)
    {
        if (slot < 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return nullptr;

        // is this legacy(?) check needed? Maybe only relevant for fumble throwing?
        for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
        {
            if (you.equip[i] == slot)
            {
                mpr("You can't toss equipped items.");
                return make_shared<ammo_action>(-1);
            }
        }

        // use ammo as the fallback -- may well end up invalid
        auto a = make_shared<ammo_action>(slot);
        if (force && (!a || !a->is_valid()))
            return make_shared<fumble_action>(slot);
        return a;
    }

    /**
     * Given an inventory slot, return an appropriate action if possible.
     * For a weapon with both an attack and an evokable ability, this
     * will always give the latter. If not possible, this will return an invalid
     * action or nullptr.
     *
     * @param slot the inventory slot number to use.
     * @param force whether to force non-throwable, non-evokable items to be
     *              throwable via fumble throwing.
     * @return the resulting action. May be invalid, or nullptr on an error.
     */
    shared_ptr<action> slot_to_action(int slot, bool force)
    {
        if (slot < 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return nullptr;

        if (you.inv[slot].base_type == OBJ_WANDS)
            return make_shared<wand_action>(slot);
        else if (you.inv[slot].base_type == OBJ_MISCELLANY)
            return make_shared<misc_action>(slot);
        else if (is_unrandom_artefact(you.inv[slot]))
        {
            auto a = make_shared<artefact_evoke_action>(slot);
            if (a->is_valid() || !force)
                return a;
            // otherwise, fall back on ammo
        }

        // use ammo as the fallback -- may well end up invalid. This means that
        // by this call, it isn't possible to get toss actions for anything
        // handled in the above conditional.
        return ammo_to_action(slot, force);
    }

    /**
     * Return an action corresponding to the wielded item. This action will
     * handle melee attacks, reaching attacks, or firing depending on what is
     * wielded. If nothing is wielded, the action will handle unarmed combat.
     * This action is guaranteed to be valid (hopefully).
     *
     * @return the resulting action
     */
    shared_ptr<action> get_primary_action()
    {
        if (you.launcher_action.is_empty())
            return make_shared<melee_action>(); // always valid
        else
            return you.launcher_action.get();
    }

    /**
     * Return the quivered action. Simply a convenience wrapper around
     * you.quiver_action.
     *
     * @return the resulting action. May be invalid.
     */
    shared_ptr<action> get_secondary_action()
    {
        auto a = you.quiver_action.get();
        ASSERT(a); // should be redundant
        return a;
    }

    /**
     * Return an action corresponding to a spell.
     *
     * @param spell the spell to use
     * @return the resulting action. May be invalid.
     */
    shared_ptr<action> spell_to_action(spell_type spell)
    {
        // could just expose spell_action outside this file?
        return make_shared<spell_action>(spell);
    }

    /**
     * Return an action corresponding to an ability.
     *
     * @abil the abilty to use
     * @return the resulting action. May be invalid.
     */
    shared_ptr<action> ability_to_action(ability_type abil)
    {
        // could just expose spell_action outside this file?
        return make_shared<ability_action>(abil);
    }

    /**
     * Set the quiver from an inventory slot. May result in an invalid action,
     * but guarantees a non-nullptr action.
     *
     * @param slot an inventory slot to use. An invalid slot (e.g. -1) results
     *             in an invalid action.
     */
    bool action_cycler::set_from_slot(int slot)
    {
        return set(slot_to_action(slot));
    }

    /**
     * Clear the quiver. This results in a valid, but inert action filling the
     * quiver.
     * @return whether the action changed
     */
    bool action_cycler::clear()
    {
        return(set(make_shared<action>()));
    }

    // note for editing this: Menu::action is defined and will take precedence
    // over quiver::action unless the quiver namespace is explicit.
    class ActionSelectMenu : public Menu
    {
    public:
        ActionSelectMenu(action_cycler &_quiver, bool _allow_empty)
            : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING),
              cur_quiver(_quiver), allow_empty(_allow_empty),
              any_spells(you.spell_no),
              any_abilities(your_talents(true, true).size() > 0),
              any_items(inv_count() > 0)
        {
            set_tag("actions");
            action_cycle = Menu::CYCLE_TOGGLE;
            menu_action  = Menu::ACT_EXECUTE;
        }

        action_cycler &cur_quiver;
        bool allow_empty;

        bool set_to_quiver(shared_ptr<quiver::action> s)
        {
            if (s && s->is_valid()
                && (allow_empty || *s != quiver::action()))
            {
                cur_quiver.set(s);
                // a bit hacky:
                if (&cur_quiver == &you.quiver_action)
                    you.launcher_action.set(s);
                return true;
            }
            return false;
        }

        bool pointless()
        {
            return !(any_spells || any_abilities || any_items);
        }

    protected:
        bool _choose_from_inv()
        {
            int slot = prompt_invent_item(allow_empty
                                            ? "Quiver which item? (- for none)"
                                            : "Quiver which item?",
                                          menu_type::invlist, OSEL_ANY,
                                          OPER_QUIVER, invprompt_flag::hide_known, '-');

            if (prompt_failed(slot))
                return true;

            if (slot == PROMPT_GOT_SPECIAL)  // '-' or empty quiver
            {
                if (!allow_empty)
                    return true;

                cur_quiver.clear();
                return false;
            }

            // At this point, an item has been selected, which will lead to
            // either an invalid or valid action. Either way, exit, so that
            // the message can  be seen.
            // TODO: in failure it would be better to set the more with an
            // error instead of exiting the menu
            set_to_quiver(slot_to_action(slot, true));
            return false;
        }

        bool _choose_from_abilities()
        {
            // currently doesn't show usk disabled abilities even though they
            // are shown for Q, is this confusing?
            vector<talent> talents = your_talents(false, true);
            // TODO: better handling for no abilities?
            int selected = choose_ability_menu(talents);

            return selected >= 0 && selected < static_cast<int>(talents.size())
                && !set_to_quiver(make_shared<ability_action>(talents[selected].which));
        }

        bool process_key(int key) override
        {
            // TODO: some kind of view action option?
            if (allow_empty && key == '-')
            {
                set_to_quiver(make_shared<quiver::action>());
                // TODO maybe drop this messaging?
                mprf("Clearing quiver.");
                return false;
            }
            else if (key == '*' && any_items)
                return _choose_from_inv();
            else if (key == '&' && any_spells)
            {
                const int skey = list_spells(false, false, false,
                                                    "Select a spell to quiver");
                if (skey == 0)
                    return true;
                if (isalpha(skey))
                {
                    auto s = make_shared<spell_action>(
                            static_cast<spell_type>(get_spell_by_letter(skey)));
                    return !set_to_quiver(s);
                }
                return false;
            }
            else if (key == '^' && any_abilities)
                return _choose_from_abilities();
            return Menu::process_key(key);
        }

        virtual formatted_string calc_title() override
        {
            string s = "Quiver which action?";
            vector<string> extra_cmds;

            if (allow_empty)
                extra_cmds.push_back("<w>-</w>: none");
            if (any_items)
                extra_cmds.push_back("<w>*</w>: full inventory");
            if (any_spells)
                extra_cmds.push_back("<w>&</w>: spells");
            if (any_abilities)
                extra_cmds.push_back(", <w>^</w>: abilities");
            if (extra_cmds.size())
                s += "(" + join_strings(extra_cmds.begin(), extra_cmds.end(), ", ") + ")";
            return formatted_string::parse_string(s);
        }

        bool any_spells;
        bool any_abilities;
        bool any_items;
    };

    /**
     * Do interactive targeting for the currently selected action. Allows the
     * player to change actions.
     */
    void action_cycler::target()
    {
        // This is a somewhat indirect interface that allows cycling between
        // arbitrary code paths that call a direction chooser. Because the
        // setup for direction choosers is so varied and complicated, we can't
        // implement the cycling internal to a direction chooser interface
        // (at least without a major refactor), so this UI takes the strategy
        // of rebuilding the direction chooser each time, but making it look
        // seamless from a user perspective. Each call to `do_target` leads to
        // the specific custom targeting code path for a particular action,
        // which then builds the direction chooser. The three CMD_TARGET...
        // commands below are not handled in a direction chooser, but rather
        // passed back via the `dist`. In addition, the `dist` object will
        // pass down a pointer to this that the direction chooser will use for
        // some custom prompt stuff. Obviously, it would better if this
        // message passing happened more directly, and in the long run perhaps
        // it would be possible to gradually refactor the various code paths
        // involved to support that, but for now that project is too
        // impractical, because each code path (except throwing) is called from
        // many places.
        shared_ptr<action> initial = get();
        clear_messages(); // this kind of looks better as a force clear, but
                          // for consistency with direct targeting commands,
                          // I will leave it as non-force
        msgwin_temporary_mode tmp;
        bool force_restore_initial;

        command_type what_happened = CMD_NO_CMD;
        do
        {
            flush_prev_message();
            msgwin_clear_temporary();
            force_restore_initial = false;
            auto a = do_target();

            // the point of this: if you cycle to or select some item, fire it,
            // and it becomes invalid (e.g. by using up ammo), this will try to
            // restore the initial quiver value rather than ending up with the
            // next in fire order item after the selected action
            if (!a || !a->is_valid())
                force_restore_initial = true;

            what_happened = a ? static_cast<command_type>(a->target.cmd_result)
                              : CMD_NO_CMD;

            switch (what_happened)
            {
            case CMD_TARGET_CYCLE_QUIVER_FORWARD:
                cycle(1, false);
                break;
            case CMD_TARGET_CYCLE_QUIVER_BACKWARD:
                cycle(-1, false);
                break;
            case CMD_TARGET_SELECT_ACTION:
                // choosing a disabled action here may exit the prompt
                // depending on the spell, it's a bit inconsistent.
                choose(*this, false);
                break;
            default:
                what_happened = CMD_NO_CMD; // shouldn't happen
                // fallthrough
            case CMD_FIRE:
            case CMD_NO_CMD:
                break;
            }
            if (!crawl_state.is_replaying_keys())
                flush_input_buffer(FLUSH_BEFORE_COMMAND);
            // right now this resets targeting on cycle; would it be better to
            // save it?
        } while (what_happened != CMD_NO_CMD && what_happened != CMD_FIRE);

        // Restore the quiver on cancel -- backwards compatible behavior.
        // Is it really the best behavior?
        if ((what_happened == CMD_NO_CMD || force_restore_initial)
            && initial && initial->is_valid())
        {
            set(initial);
        }
    }

    /**
     * Presents an interface for the player to choose an action to quiver from
     * a list of options.
     * @param cur_quiver a quiver to use for context
     * @param allow_empty whether to allow the player to set the empty quiver
     */
    void choose(action_cycler &cur_quiver, bool allow_empty)
    {
        // should be action_cycler method?
        // TODO: icons in tiles, dividers or subtitles for each category?
        ActionSelectMenu menu(cur_quiver, allow_empty);
        vector<shared_ptr<action>> actions;
        auto tmp = ammo_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = wand_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = misc_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = artefact_evoke_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = spell_action(SPELL_NO_SPELL).get_fire_order();
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = ability_action(ABIL_NON_ABILITY).get_fire_order();
        actions.insert(actions.end(), tmp.begin(), tmp.end());

        if (actions.size() == 0 && menu.pointless())
        {
            mpr("You have nothing to quiver.");
            return;
        }

        menu.set_title(new MenuEntry("", MEL_TITLE));

        menu_letter hotkey;
        // What to do if everything is disabled?
        for (const auto &a : actions)
        {
            if (!a || !a->is_valid())
                continue;
            string action_desc = a->quiver_description();
            if (you.launcher_action.item_is_quivered(a->get_item()))
                action_desc += " (quivered ammo)";
            else if (you.quiver_action.item_is_quivered(a->get_item()))
                action_desc += " (quivered)";
            MenuEntry *me = new MenuEntry(action_desc,
                                                MEL_ITEM, 1,
                                                (int) hotkey);
            // TODO: is there a way to show formatting in menu items?
            me->colour = a->quiver_color();
            me->data = (void *) &a; // pointer to vector element - don't change the vector!
#ifdef USE_TILE
            for (auto t : a->get_tiles())
                me->add_tile(t);
#endif
            menu.add_entry(me);
            hotkey++;
        }

        menu.on_single_selection = [&menu](const MenuEntry& item)
        {
            const shared_ptr<action> *a = static_cast<shared_ptr<action> *>(item.data);
            return !menu.set_to_quiver(*a);
        };
        menu.show();
    }

    // this class is largely legacy code -- can it be done away with?
    // or refactored to use actions.
    // TODO: auto switch to last action when swapping away from a launcher --
    // right now it goes to an ammo only in that case.
    ammo_history::ammo_history()
    {
        COMPILE_CHECK(ARRAYSZ(m_last_used_of_type) == quiver::NUM_LAUNCHERS);
    }

    int ammo_history::get_last_ammo(const item_def *launcher) const
    {
        return get_last_ammo(quiver::_get_weapon_ammo_type(launcher));
    }

    int ammo_history::get_last_ammo(quiver::launcher type) const
    {
        const int slot = _get_pack_slot(m_last_used_of_type[type]);
        ASSERT(slot < ENDOFPACK && (slot == -1 || you.inv[slot].defined()));
        return slot;
    }

    void ammo_history::set_quiver(const item_def &item, quiver::launcher ammo_type)
    {
        m_last_used_of_type[ammo_type] = item;
        m_last_used_of_type[ammo_type].quantity = 1;
        you.redraw_quiver = true;
    }

    // Notification that item was fired
    void ammo_history::on_item_fired(const item_def& item, bool explicitly_chosen)
    {
        if (!explicitly_chosen)
        {
            // If the item was not actively chosen, i.e. just automatically
            // passed into the quiver, don't change any of the quiver settings.
            you.redraw_quiver = true;
            return;
        }
        // If item matches the launcher, put it in that launcher's last-used item.
        // Otherwise, it goes into last hand-thrown item.

        const item_def *weapon = you.weapon();

        if (weapon && item.launched_by(*weapon))
        {
            const quiver::launcher t = quiver::_get_weapon_ammo_type(weapon);
            m_last_used_of_type[t] = item;
            m_last_used_of_type[t].quantity = 1;    // 0 makes it invalid :(
        }
        else
        {
            const launch_retval projected = is_launched(&you, you.weapon(),
                                                        item);

            // Don't do anything if this item is not really fit for throwing.
            if (projected == launch_retval::FUMBLED)
                return;

            m_last_used_of_type[quiver::AMMO_THROW] = item;
            m_last_used_of_type[quiver::AMMO_THROW].quantity = 1;
        }

        you.redraw_quiver = true;
    }

    // ----------------------------------------------------------------------
    // Save/load
    // ----------------------------------------------------------------------

    // legacy marshalling code, still semi-used
    static const short QUIVER_COOKIE = short(0xb015);
    void ammo_history::save(writer& outf) const
    {
        marshallShort(outf, QUIVER_COOKIE);

        marshallItem(outf, item_def()); // was: m_last_weapon
        marshallInt(outf, 0); // was: m_last_used_type
        marshallInt(outf, ARRAYSZ(m_last_used_of_type));

        for (unsigned int i = 0; i < ARRAYSZ(m_last_used_of_type); i++)
            marshallItem(outf, m_last_used_of_type[i]);
    }

    // legacy unmarshalling code, still semi-used
    void ammo_history::load(reader& inf)
    {
        // warning: this is called in the unmarshalling sequence before the
        // inventory is actually in place
        const short cooky = unmarshallShort(inf);
        ASSERT(cooky == QUIVER_COOKIE); (void)cooky;

        auto dummy = item_def();
        unmarshallItem(inf, dummy); // was: m_last_weapon
        unmarshallInt(inf); // was: m_last_used_type;

        const unsigned int count = unmarshallInt(inf);
        ASSERT(count <= ARRAYSZ(m_last_used_of_type));

        for (unsigned int i = 0; i < count; i++)
            unmarshallItem(inf, m_last_used_of_type[i]);
    }

    void on_actions_changed()
    {
        you.quiver_action.on_actions_changed();
        you.launcher_action.on_actions_changed();
    }

    // Called when the player has switched weapons
    // Some cases of interest:
    // * player picks up a new weapon and wields it
    // * player is swapping between a launcher and melee
    void on_weapon_changed()
    {
        const item_def* weapon = you.weapon();
        you.launcher_action.set(quiver::find_action_from_launcher(weapon));

        // if the new weapon is a launcher with ammo, and the autoquiver option
        // is set, quiver that ammo in the main quiver
        if (!you.launcher_action.is_empty() && Options.launcher_autoquiver)
            you.quiver_action.set(you.launcher_action.get());
        else if (weapon && is_unrandom_artefact(*weapon)
                                            && Options.launcher_autoquiver)
        {
            // apply similar logic to the (few) evokable weapons
            auto a = make_shared<artefact_evoke_action>(weapon->link);
            if (a->is_valid())
                you.quiver_action.set(a);
        }


        // If that failed, see if there's anything valid in the quiver history.
        // This is aimed at using the quiver history when switching away from
        // a weapon.
        if (!you.quiver_action.get()->is_valid())
        {
            auto a = you.quiver_action.find_last_valid();
            if (a)
                you.quiver_action.set(a);
        }
    }
}


// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

// Helper for _get_fire_order.
// Types may actually contain more than one fire_type.
static bool _item_matches(const item_def &item, fire_type types,
                          const item_def* launcher, bool manual)
{
    ASSERT(item.defined());

    if (types & FIRE_INSCRIBED)
        if (item.inscription.find(manual ? "+F" : "+f", 0) != string::npos)
            return true;

    if (item.base_type != OBJ_MISSILES)
        return false;

    if ((types & FIRE_STONE) && item.sub_type == MI_STONE)
        return true;
    if ((types & FIRE_JAVELIN) && item.sub_type == MI_JAVELIN)
        return true;
    if ((types & FIRE_ROCK) && item.sub_type == MI_LARGE_ROCK)
        return true;
    if ((types & FIRE_NET) && item.sub_type == MI_THROWING_NET)
        return true;
    if ((types & FIRE_BOOMERANG) && item.sub_type == MI_BOOMERANG)
        return true;
    if ((types & FIRE_DART) && item.sub_type == MI_DART)
        return true;

    if (types & FIRE_LAUNCHER)
    {
        if (launcher && item.launched_by(*launcher))
            return true;
    }

    return false;
}

// Returns inv slot that contains an item that looks like item,
// or -1 if not in inv.
static int _get_pack_slot(const item_def& item)
{
    if (!item.defined())
        return -1;

    if (in_inventory(item) && _items_similar(item, you.inv[item.link], false))
        return item.link;

    // First try to find the exact same item.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def &inv_item = you.inv[i];
        if (inv_item.quantity && _items_similar(item, inv_item, false))
            return i;
    }

    // If that fails, try to find an item sufficiently similar.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def &inv_item = you.inv[i];
        if (inv_item.quantity && _items_similar(item, inv_item, true))
        {
            // =f prevents item from being in fire order.
            if (quiver::_fireorder_inscription_ok(i, false) == MB_FALSE)
                return -1;

            return i;
        }
    }

    return -1;
}

static bool _items_similar(const item_def& a, const item_def& b, bool force)
{
    return items_similar(a, b) && (force || a.slot == b.slot);
}

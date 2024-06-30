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
#include "describe.h" // describe_to_hit
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
#include "movement.h"
#include "options.h"
#include "player.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "sound.h"
#include "spl-damage.h"
#include "spl-transloc.h"
#include "stringutil.h"
#include "tags.h"
#include "target.h"
#include "terrain.h"
#include "tilepick.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "rltiles/tiledef-icons.h"
#include "wiz-dgn.h"

#define LAST_MISSILE_SLOT_KEY "last_missile_type"

static int _get_pack_slot(const item_def&);
static bool _item_matches(const item_def &item, fire_type types, bool manual);
static bool _items_similar(const item_def& a, const item_def& b,
                           bool force = true);

// TODO: how should newquivers integrate with the local tiles UI?

static vector<string> _desc_hit_chance(const monster_info &mi)
{
    ostringstream result;
    describe_to_hit(mi, result, you.weapon());
    string str = result.str();
    if (str.empty())
        return vector<string>{};
    return vector<string>{str};
}

namespace quiver
{
    static bool _quiver_inscription_ok(int slot)
    {
        if (slot < 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return true;
        return !strstr(you.inv[slot].inscription.c_str(), "!Q");
    }

    static maybe_bool _fireorder_inscription_ok(int slot, bool cycling = false)
    {
        if (slot < 0 || slot >= ENDOFPACK || !you.inv[slot].defined())
            return maybe_bool::maybe;
        if (strstr(you.inv[slot].inscription.c_str(), cycling ? "=F" : "=f")
            || !_quiver_inscription_ok(slot))
        {
            return false;
        }
        // n.b. here for completeness, this is actually handled by dodgy
        // ancient fire order code...
        if (strstr(you.inv[slot].inscription.c_str(), cycling ? "+F" : "+f"))
            return true;
        return maybe_bool::maybe;
    }

    void action::set_target(const dist &t)
    {
        target = t;
        if (!target.fire_context)
            target.fire_context = default_fire_context;
    }

    void action::reset()
    {
        set_target(dist());
    }

    /**
     * Does this action meet preconditions for triggering? Checks configurable
     * HP and MP thresholds, aimed at autofight commands.
     * @return true if triggering should be prevented
     */
    bool action::autofight_check() const
    {
        if (crawl_state.skip_autofight_check)
            return false;
        // don't do these checks if the action will lead to interactive targeting
        if (target.needs_targeting())
            return false;
        bool af_hp_check = false;
        bool af_mp_check = false;
        if (!clua.callfn("af_hp_is_low", ">b", &af_hp_check)
            || uses_mp()
               && !clua.callfn("af_mp_is_low", ">b", &af_mp_check))
        {
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            return true;
        }
        if (af_hp_check)
            mpr("You are too injured to fight recklessly!");
        else if (af_mp_check && !you.has_mutation(MUT_HP_CASTING)
            && you.magic_points == 0)
        {
            mpr("You are out of magic!");
        }
        else if (af_mp_check)
        {
            mprf("You are too depleted to draw on your %s recklessly!",
                you.has_mutation(MUT_HP_CASTING) ? "health" : "magic");
        }
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

    int action::source_hotkey() const
    {
        if (get_item() >= 0 && is_valid())
            return index_to_letter(get_item());
        return 0;
    }


    shared_ptr<action> action_cycler::do_target()
    {
        // this might be better as an action method
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
            untargeted_fire(*a);
            if (!a->target.isCancel)
                a->trigger(a->target);
        }
        // TODO: does this cause dbl "ok then"s in some places?
        if (a->target.isCancel && a->target.cmd_result == CMD_NO_CMD)
            canned_msg(MSG_OK);

        // we return a; if it has become invalid (e.g. by running out of ammo),
        // it will no longer be accessible via get().
        return a;
    }

    string action_cycler::fire_key_hints() const
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

    bool action_cycler::targeter_handles_key(command_type c) const
    {
        // TODO: factor these out of the menu code into methods on this class?
        switch (c)
        {
        case CMD_TARGET_SELECT_ACTION:
        case CMD_TARGET_CYCLE_QUIVER_FORWARD:
        case CMD_TARGET_CYCLE_QUIVER_BACKWARD:
            return true;
        default:
            return false;
        }
    }

    // Get a sorted list of items to show in the fire interface.
    //
    // If ignore_inscription_etc, ignore =f and Options.fire_items_start.
    // This is used for generating informational error messages, when the
    // fire order is empty.
    static void _get_item_fire_order(vector<int>& order,
                                     bool ignore_inscription_etc,
                                     bool manual)
    {
        const int inv_start = (ignore_inscription_etc ? 0
                                                      : Options.fire_items_start);

        for (int i_inv = inv_start; i_inv < ENDOFPACK; i_inv++)
        {
            const item_def& item = you.inv[i_inv];
            if (!item.defined())
                continue;

            // =F prevents item from being in fire order.
            const maybe_bool i_check = _fireorder_inscription_ok(i_inv, true);
            if (!ignore_inscription_etc && bool(!i_check))
                continue;

            for (unsigned int i_flags = 0; i_flags < Options.fire_order.size();
                 i_flags++)
            {
                if (_item_matches(item, (fire_type)Options.fire_order[i_flags],
                                  manual))
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

    // Similar to melee_action (below), but for firing launchers.
    struct ranged_action : public action
    {
        ranged_action() : action() { }

        bool is_enabled() const override
        {
            return !fire_warn_if_impossible(true, you.weapon());
        }

        bool is_valid() const override
        {
            return you.weapon() && is_range_weapon(*you.weapon());
        }

        bool is_targeted() const override
        {
            return !you.confused();
        }

        bool allow_autofight() const override
        {
            return is_enabled();
        }

        void trigger(dist &t) override
        {
            set_target(t);
            if (!is_valid())
                return;
            if (!is_enabled())
            {
                fire_warn_if_impossible(false, you.weapon()); // for messaging (TODO refactor; message about inscriptions?)
                return;
            }
            if (autofight_check() || !do_inscription_check())
                return;

            throw_it(*this);
        }

        item_def *get_launcher() const override
        {
            return you.weapon();
        }

        int get_item() const override
        {
            return you.equip[EQ_WEAPON];
        };

        string quiver_verb() const override
        {
            return "fire";
        }

        formatted_string quiver_description(bool short_desc=false) const override
        {
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;
            const item_def &weapon = *get_launcher();

            // TODO: or just lightgrey?
            qdesc.textcolour(Options.status_caption_colour);

            if (!short_desc)
            {
                string verb = you.confused() ? "confused " : "";
                verb += quiver_verb();
                qdesc.cprintf("%s: %c) ", uppercase_first(verb).c_str(),
                                index_to_letter(weapon.link));
            }

            const string prefix = item_prefix(weapon);
            const int prefcol =
                menu_colour(weapon.name(DESC_PLAIN), prefix, "stats", false);
            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else if (prefcol != -1)
                qdesc.textcolour(prefcol);
            else
                qdesc.textcolour(LIGHTGREY);

            qdesc += weapon.name(DESC_PLAIN, true);
            return qdesc;
        }

        vector<shared_ptr<action>> get_fire_order(bool allow_disabled=true, bool=false) const override
        {
            if (is_valid() && (allow_disabled || is_enabled()))
                return { make_shared<ranged_action>() };
            else
                return { };
        }

        void save(CrawlHashTable &save_target) const override
        {
            save_target["type"] = "ranged_action";
        }
    };

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

        bool is_valid() const override
        {
            return !you.weapon() || !is_range_weapon(*you.weapon());
        }

        bool is_targeted() const override { return true; }

        string quiver_verb() const override
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
                    if (dt & DVORP_CLAWING || dt & DVORP_TENTACLE)
                        return "attack";
                }
                return "punch";
            }

            if (weapon_reach(*weapon) > REACH_NONE)
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
                menu_colour(weapon ? weapon->name(DESC_PLAIN) : "", prefix, "stats", false);
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

            set_target(t);

            if (you.confused())
            {
                if (!you.is_motile())
                {
                    // XX duplicate code with movement.cc:move_player_action
                    if (cancel_confused_move(true))
                        return;

                    if (!one_chance_in(3))
                    {
                        coord_def move(random2(3) - 1, random2(3) - 1);
                        if (move.origin())
                        {
                            mpr("You nearly hit yourself!");
                            you.turn_is_over = true;
                            return;
                        }
                        // replace input target with a confused one
                        target.target = you.pos() + move;
                    }
                    // fallthrough
                }
                else
                {
                    if (target.needs_targeting())
                        mpr("You're too confused to aim your attacks!");
                    else
                        mpr("You're too confused to attack without stumbling around!");
                    return;
                }
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

            // This is redundant with a later check in fight_melee; but, the
            // way this check works, if the player overrides it once it won't
            // give a warning until they switch weapons. UI-wise, if there is
            // going to be a targeter it makes sense to show it first.
            if (target.needs_targeting() && !wielded_weapon_check(weapon))
                return;

            target.isEndpoint = true; // is this needed? imported from autofight code
            const reach_type reach_range = you.reach_range();

            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.mode = TARG_HOSTILE;
            args.range = reach_range;
            args.top_prompt = quiver_description().tostring();
            args.self = confirm_prompt_type::cancel;

            unique_ptr<targeter> hitfunc;
            if (attack_cleaves(you))
            {
                const int range = reach_range;
                hitfunc = make_unique<targeter_cleave>(&you, you.pos(), range);
            }
            else
                hitfunc = make_unique<targeter_reach>(&you, reach_range);
            args.hitfunc = hitfunc.get();
            args.get_desc_func = bind(_desc_hit_chance, placeholders::_1);

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

            if (x_distance > reach_range || y_distance > reach_range)
            {
                mpr("Your weapon can't reach that far!");
                return;
            }

            // Failing to hit someone due to a friend blocking is infuriating,
            // shadow-boxing empty space is not (and would be abusable to wait
            // with no penalty).
            if (mons)
                you.apply_berserk_penalty = false;

            // Calculate attack delay now in case we have to apply it.
            const int attack_delay = you.attack_delay().roll();

            if (feat_is_solid(env.grid(target.target)))
            {
                if (you.confused())
                {
                    mprf("You attack %s.",
                         feature_description_at(target.target,
                                                false, DESC_THE).c_str());
                    you.time_taken = attack_delay;
                    you.turn_is_over = true;
                    return;
                }
                else
                {
                    canned_msg(MSG_SOMETHING_IN_WAY);
                    return;
                }
            }

            // Check for a monster in the way. If there is one, it blocks the reaching
            // attack 50% of the time, and the attack tries to hit it if it is hostile.
            // REACH_THREE entails smite targeting; this is a bit hacky in that
            // this is entirely for the sake of UNRAND_RIFT.
            // Cleaving reaches also will never fail to miss, since the player can
            // just attack another target in most cases to hit both.
            if (reach_range < REACH_THREE
                && !attack_cleaves(you)
                && (x_distance > 1 || y_distance > 1))
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
                    && !god_protects(&you, *midmons, true)
                    && (midmons->type != MONS_SPECTRAL_WEAPON
                        || !midmons->wont_attack())
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
            if (is_valid() && (allow_disabled || is_enabled()))
                return { make_shared<melee_action>() };
            else
                return { };
        }
    };

    struct item_action : public action
    {
        item_action(string _save_key, int slot)
            : action(),
                item_slot(slot), save_key(_save_key)
        {
        }

        bool equals(const action &other) const override
        {
            // type ensured in base class
            auto &o = static_cast<const item_action &>(other);
            return item_slot == o.item_slot
                && save_key == o.save_key; // XX hacky
        }

        void save(CrawlHashTable &save_target) const override
        {
            ASSERT(!save_key.empty());
            save_target["type"] = save_key;
            save_target["param"] = item_slot;
        }

        int get_item() const override
        {
            return item_slot;
        };

        bool is_valid() const override
        {
            return item_slot >=0 && item_slot < ENDOFPACK
                                        && you.inv[item_slot].defined();
        }

        formatted_string quiver_description(bool short_desc) const override
        {
            // TODO: generalize this code
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;

            const item_def& quiver = you.inv[item_slot];
            ASSERT(quiver.link != NON_ITEM);
            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("%s: ", quiver_verb().c_str());

            qdesc.textcolour(quiver_color());
            qdesc += quiver.name(DESC_PLAIN, true);

            return qdesc;
        }

        // TODO: can get_fire_order be generalized?

        string quiver_verb() const override { return "Activate"; }
        virtual bool is_enabled() const override = 0;
        virtual void trigger(dist &) override = 0;

    protected:
        int item_slot;
        string save_key;
    };

    /**
     * An ammo_action is an action that fires ammo from a slot in the
     * inventory. This covers throwing; tossing is handled by a subclass.
     */
    struct ammo_action : public item_action
    {
        ammo_action(int slot=-1, string _save_key="ammo_action")
            : item_action(_save_key, slot)
        {
        }

        /// does the launch type match the action type and current weapon?
        virtual bool launch_type_check() const
        {
            return is_throwable(&you, you.inv[item_slot]);
        }

        virtual bool is_enabled() const override
        {
            if (!is_valid())
                return false;

            if (fire_warn_if_impossible(true, nullptr))
                return false;

            // TODO: check inscriptions here? That code would need to be
            // refactored.
            return true;
        }

        virtual bool is_valid() const override
        {
            if (!item_action::is_valid())
                return false;
            if (you.has_mutation(MUT_NO_GRASPING))
                return false;

            return launch_type_check();
        }

        bool is_targeted() const override
        {
            return !you.confused();
        }

        bool allow_autofight() const override
        {
            if (!is_enabled())
                return false;

            // nets shouldn't be autofired with autofight_throw, only with
            // autofire.
            if (you.inv[item_slot].sub_type == MI_THROWING_NET)
                return false;

            return true;
        }

        void trigger(dist &t) override
        {
            set_target(t);
            if (!is_valid())
                return;
            if (!is_enabled())
            {
                fire_warn_if_impossible(false, nullptr); // for messaging (TODO refactor; message about inscriptions?)
                return;
            }
            if (autofight_check() || !do_inscription_check())
                return;

            // TODO: refactor throw_it into here?
            throw_it(*this);

            // Update the legacy quiver history data structure
            // TODO: eliminate this? History should be stored per quiver, not
            // globally
            you.m_quiver_history.on_item_fired(you.inv[item_slot]);
        }

        virtual formatted_string quiver_description(bool short_desc) const override
        {
            ASSERT_RANGE(item_slot, -1, ENDOFPACK);
            // or error?
            if (!is_valid())
                return action::quiver_description(short_desc);

            formatted_string qdesc;

            const item_def& quiver = you.inv[item_slot];
            ASSERT(quiver.link != NON_ITEM);
            // TODO: or just lightgrey?
            qdesc.textcolour(Options.status_caption_colour);
            // XX abstract to quiver verb?
            if (!short_desc)
            {
                const string verb =
                    make_stringf("%s%s",
                                 you.confused() ? "confused " : "",
                                 is_throwable(&you, quiver) ? "throw" : "toss (no damage)");
                qdesc.cprintf("%s: ", uppercase_first(verb).c_str());
            }

            // TODO: I don't actually know what this prefix stuff is
            const string prefix = item_prefix(quiver);

            const int prefcol =
                menu_colour(quiver.name(DESC_PLAIN), prefix, "stats", false);
            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else if (prefcol != -1)
                qdesc.textcolour(prefcol);
            else
                qdesc.textcolour(LIGHTGREY);
            qdesc += quiver.name(DESC_PLAIN, true);

            return qdesc;
        }

        virtual shared_ptr<action> find_replacement() const override
        {
            return find_ammo_action();
        }

        vector<shared_ptr<action>> get_menu_fire_order(bool allow_disabled) const
        {
            vector<shared_ptr<action>> result;
            for (int i_inv = 0; i_inv < ENDOFPACK; i_inv++)
            {
                auto a = make_shared<ammo_action>(i_inv);
                if (a->is_valid() && (allow_disabled || a->is_enabled()))
                    result.push_back(std::move(a));
            }
            return result;
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool menu=false) const override
        {
            if (menu)
                return get_menu_fire_order(allow_disabled);

            vector<int> fire_order;
            _get_item_fire_order(fire_order, menu, false);

            vector<shared_ptr<action>> result;

            for (auto i : fire_order)
            {
                auto a = make_shared<ammo_action>(i);
                if (a->is_valid() && (allow_disabled || a->is_enabled()))
                    result.push_back(std::move(a));
                // TODO: allow arbitrary items with +f to get added here? I
                // have had some trouble getting this to work
            }
            return result;
        }

    };

    bool toss_validate_item(int slot, string *err)
    {
        // misc tossing restrictions go here

        // No tossing cursed weapons.
        // this check would be safe to remove, but it's useful for
        // messaging purposes to see that the action is invalid. (It's
        // otherwise handled by the unwield call.)
        if (slot == you.equip[EQ_WEAPON]
            && is_weapon(you.inv[slot])
            && you.inv[slot].cursed())
        {
            if (err)
                *err = "That weapon is stuck to your " + you.hand_name(false) + "!";
            return false;
        }

        // make people manually take stuff off if they want to toss it
        // (weapons are still ok for some reason)
        if (item_is_worn(slot))
        {
            if (err)
                *err = "You are wearing that object!";
            return false;
        }
        return true;
    }

    // for fumble throwing / tossing
    struct fumble_action : public ammo_action
    {
        fumble_action(int slot=-1) : ammo_action(slot, "fumble_action")
        {
        }

        // uses ammo_action fire order

        bool launch_type_check() const override
        {
            return toss_validate_item(item_slot);
        }
    };

    static bool _spell_needs_manual_targeting(spell_type s)
    {
        switch (s)
        {
        case SPELL_FULMINANT_PRISM:
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
        case SPELL_SEARING_RAY:          // for autofight to work
        case SPELL_FLAME_WAVE:           // these need to
        case SPELL_MAXWELLS_COUPLING:    // skip autofight targeting
        case SPELL_LRD: // skip initial autotarget for LRD so that it doesn't
                        // fix on a close monster that can't be targeted. I'm
                        // not quite sure what the right thing to do is?
                        // An alternative would be to just error if the closest
                        // monster can't be autotargeted, or pop out to manual
                        // targeting for that case; the behavior involved in
                        // listing it here just finds the closest targetable
                        // monster.
        case SPELL_BORGNJORS_VILE_CLUTCH: // BVC shouldn't retarget monsters
                                          // that are clutched, and spell
                                          // targeting handles this case.
        case SPELL_APPORTATION: // Apport doesn't target monsters at all
        case SPELL_MAGNAVOLT:
            return true;
        default:
            return _spell_needs_manual_targeting(s);
        }
    }

    bool is_autofight_combat_spell(spell_type spell)
    {
        return spell_is_direct_attack(spell)
            || spell == SPELL_FOXFIRE; // not a direct attack, but has sensible autofight behavior
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
                            && !spell_is_useless(spell, true, false)
            // Use a version of the charge range check that
            // ignores things like butterflies, so that autofight doesn't get
            // tripped up.
                            && (spell != SPELL_ELECTRIC_CHARGE
                                || electric_charge_impossible_reason(false).empty());
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
            return is_autofight_combat_spell(spell);
        }

        bool uses_mp() const override
        {
            return is_valid();
        }

        bool check_wait_spells() const
        {
            if (!target.needs_targeting() && wait_spell_active(spell))
            {
                crawl_state.prev_cmd = CMD_WAIT; // hackiness, but easy
                update_acrobat_status();
                you.turn_is_over = true;
                return true;
            }
            return false;
        }

        void trigger(dist &t) override
        {
            // note: we don't do the enabled check here, because cast_a_spell
            // duplicates it and does appropriate messaging
            // TODO refactor?
            if (!is_valid())
                return;

            set_target(t);

            // don't do the range check check if doing manual firing.
            const bool do_range_check = !target.needs_targeting();

            if (_spell_needs_manual_targeting(spell))
            {
                // force interactive mode no matter what:
                // XX this overrides programmatic calls, is there another way?
                target.target = coord_def(-1,-1);
                target.find_target = false; // default, but here for clarity's sake
                target.interactive = true;
            }
            else if (_spell_no_autofight_targeting(spell))
            {
                // use direction chooser find_target behavior unless interactive
                // is set before the call:
                target.target = coord_def(-1,-1);
                target.find_target = true;
            }
            else if (!is_dynamic_targeted())
            {
                // this is a somewhat hacky way to allow non-interactive mode;
                // the value of `target` doesn't matter for static targeters.
                // Like the no-autofight case, if `interactive` is set
                // before the call, will still pop up an interactive targeter.
                target.target = you.pos();
            }

            if (autofight_check())
                return;

            if (!check_wait_spells())
                cast_a_spell(do_range_check, spell, &target);

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
            if (wait_spell_active(spell))
                qdesc.cprintf("Continue: ");
            else
                qdesc.cprintf("Cast: ");

            qdesc.textcolour(quiver_color());

            // TODO: is showing the spell letter useful?
            qdesc.cprintf("%s", spell == SPELL_MAXWELLS_COUPLING ?
                                "Capacitive Coupling" : spell_title(spell));

            if (spell == SPELL_GRAVE_CLAW)
            {
                qdesc.cprintf(" (%d/%d)", you.props[GRAVE_CLAW_CHARGES_KEY].get_int()
                                        , GRAVE_CLAW_MAX_CHARGES);
            }

            if (fail_severity(spell) > 0)
            {
                qdesc.cprintf(" (%s)",
                        failure_rate_to_string(raw_spell_fail(spell)).c_str());
            }

            return qdesc;
        }

        bool in_fire_order(bool allow_disabled, bool menu) const
        {
            return is_valid()
                && (allow_disabled || is_enabled())
                && (menu || Options.fire_order_spell.count(spell))
                // some extra stuff for fire order in particular: don't
                // show spells that are dangerous to cast or forbidden.
                // These can still be force-quivered.
                && fail_severity(spell) < Options.fail_severity_to_quiver
                && spell_highlight_by_utility(spell, COL_UNKNOWN) != COL_FORBIDDEN;
        }

        // TODO: rewrite all these fns as iterators?
        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool menu=false) const override
        {
            // goes by letter order
            vector<shared_ptr<action>> result;
            for (int i = 0; i < 52; i++)
            {
                auto a = make_shared<spell_action>(
                                get_spell_by_letter(index_to_letter(i)));
                if (a->in_fire_order(allow_disabled, menu))
                    result.push_back(std::move(a));
            }
            return result;
        }

        int source_hotkey() const override
        {
            // for get_spell_letter, invalid is -1, we need 0
            return max(0, get_spell_letter(spell));
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
        case ABIL_BEGIN_UNTRANSFORM:
        case ABIL_EXSANGUINATE:
        case ABIL_TSO_BLESS_WEAPON:
        case ABIL_KIKU_BLESS_WEAPON:
        case ABIL_KIKU_GIFT_CAPSTONE_SPELLS:
        case ABIL_SIF_MUNA_FORGET_SPELL:
        case ABIL_LUGONU_BLESS_WEAPON:
        case ABIL_ASHENZARI_CURSE:
        case ABIL_RU_REJECT_SACRIFICES:
        case ABIL_HEPLIAKLQANA_IDENTITY:
        case ABIL_RENOUNCE_RELIGION:
        case ABIL_CONVERT_TO_BEOGH:
        case ABIL_OKAWARU_GIFT_WEAPON:
        case ABIL_OKAWARU_GIFT_ARMOUR:
        case ABIL_INVENT_GIZMO:
        // high price zone
        case ABIL_ZIN_DONATE_GOLD:
        // not entirely pseudo, but doesn't make a lot of sense to quiver:
        case ABIL_TRAN_BAT:
            return true;
        default:
            return false;
        }
    }

    static bool _ability_quiver_range_check(ability_type abil,
                                            bool quiet = true)
    {
        // Hacky: do some quiver-specific range checks for the sake of
        // autofight. We can't approach this like spells (which are marked
        // as useless with no enemies in range), because there is no
        // equivalent of `Z` to force-activate an ability that is indicating
        // temporary uselessness. This way, the player can still activate
        // it from the `a` menu, just not from the quiver.
        // (What abilities are missing here?)

        if (get_dist_to_nearest_monster() > ability_range(abil)
            && (get_ability_flags(abil) & abflag::targeting_mask))

        {
            if (!quiet)
                mpr("You can't see any hostile targets in range.");
            return false;
        }
        return true;
    }

    bool is_autofight_combat_ability(ability_type ability)
    {
        switch (ability)
        {
        case ABIL_BLINKBOLT: // TODO: disable under nomove?
        case ABIL_RU_POWER_LEAP: // disable under nomove, or altogether?
        case ABIL_SPIT_POISON:
        case ABIL_CAUSTIC_BREATH:
        case ABIL_BREATHE_FIRE:
        case ABIL_GLACIAL_BREATH:
        case ABIL_BREATHE_POISON:
        case ABIL_NULLIFYING_BREATH:
        case ABIL_STEAM_BREATH:
        case ABIL_NOXIOUS_BREATH:
        case ABIL_DAMNATION:
        case ABIL_MAKHLEB_MINOR_DESTRUCTION:
        case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
        case ABIL_LUGONU_BANISH:
        case ABIL_BEOGH_SMITING:
        case ABIL_QAZLAL_UPHEAVAL:
        case ABIL_EVOKE_DISPATER:
        case ABIL_EVOKE_OLGREB:
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
            if (!is_valid())
                return false;

            if (!_ability_quiver_range_check(ability))
                return false;

            // TODO: _check_ability_dangerous?
            return check_ability_possible(ability, true);
        }

        bool is_dynamic_targeted() const
        {
            return !!(get_ability_flags(ability) & abflag::targeting_mask);
        }

        bool is_targeted() const override
        {
            // hard-coded list of abilities that have a targeter
            // there is no general way of getting this?
            // TODO: convert these to use the shared ability targeting code.
            switch (ability)
            {
            case ABIL_HOP:
            case ABIL_BLINKBOLT:
            case ABIL_COMBUSTION_BREATH:
            case ABIL_GLACIAL_BREATH:
            case ABIL_GALVANIC_BREATH:
            case ABIL_CAUSTIC_BREATH:
            case ABIL_NULLIFYING_BREATH:
            case ABIL_STEAM_BREATH:
            case ABIL_NOXIOUS_BREATH:
            case ABIL_MUD_BREATH:
            case ABIL_DAMNATION:
            case ABIL_ELYVILON_HEAL_OTHER:
            case ABIL_LUGONU_BANISH:
            case ABIL_BEOGH_SMITING:
            case ABIL_FEDHAS_OVERGROW:
            case ABIL_QAZLAL_UPHEAVAL:
            case ABIL_RU_POWER_LEAP:
            case ABIL_USKAYAW_LINE_PASS:
            case ABIL_USKAYAW_GRAND_FINALE:
            case ABIL_WU_JIAN_WALLJUMP:
            case ABIL_EVOKE_DISPATER:
            case ABIL_EVOKE_OLGREB:
#ifdef WIZARD
            case ABIL_WIZ_BUILD_TERRAIN:
            case ABIL_WIZ_CLEAR_TERRAIN:
#endif
                return true;
            default:
                return is_dynamic_targeted()
                       || ability_has_targeter(ability);
            }
        }

        bool allow_autofight() const override
        {
            if (!is_enabled())
                return false;
            return is_autofight_combat_ability(ability);
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

            // TODO: for more uniform behavior with spells, this should skip the
            // range check in firing mode. However, there's no clean way to
            // do this right now, so I'm just leaving this comment.
            if (!is_enabled())
            {
                // do some messaging
                if (_ability_quiver_range_check(ability, false))
                    check_ability_possible(ability, false);
                return;
            }

            set_target(t);
            if (ability != ABIL_HOP) // find_target will fail for ABIL_HOP
                target.find_target = true; // TODO: does this break lua targeting?
            if (autofight_check())
                return;

            talent tal = get_talent(ability, false);
            activate_talent(tal, &target);

            // TODO: does non-targeted case come up?
            if (target.isCancel && !target.interactive && is_targeted())
                mpr("No targets found!");

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
            string abil_name = ability_name(ability);
#ifdef WIZARD
            int last_feat = you.props[WIZ_LAST_FEATURE_TYPE_PROP].get_int();
            if (ability == ABIL_WIZ_BUILD_TERRAIN
                && last_feat != DNGN_UNSEEN)
            {
                qdesc.cprintf("Build '%s'", dungeon_feature_name(
                    static_cast<dungeon_feature_type>(last_feat)));
            }
            else
#endif
                qdesc.cprintf("%s", ability_name(ability).c_str());

            if (is_card_ability(ability))
                qdesc.cprintf(" %s", nemelex_card_text(ability).c_str());

            return qdesc;
        }

        vector<tile_def> get_tiles() const override
        {
            return { tile_def(tileidx_ability(ability)) };
        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool menu=false) const override
        {
            vector<talent> talents = your_talents(false, true, true);
            // goes by letter order
            vector<shared_ptr<action>> result;

            for (const auto &tal : talents)
            {
                if (_pseudoability(tal.which)
                    || !menu && Options.fire_order_ability.count(tal.which) == 0)
                {
                    continue;
                }
                auto a = make_shared<ability_action>(tal.which);
                if (a->is_valid() && (allow_disabled || a->is_enabled()))
                    result.push_back(std::move(a));
            }
            return result;
        }

        int source_hotkey() const override
        {
            int i = find_ability_slot(ability);
            if (i < 0)
                return 0;
            return index_to_letter(i);
        }

    private:
        ability_type ability;
    };

    struct consumable_action : public item_action
    {
        consumable_action(int slot=-1)
            : item_action("consumable_action", slot)
        {
        }

        bool is_valid() const override
        {
            if (!item_action::is_valid())
                return false;
            const item_def& c = you.inv[item_slot];
            return c.base_type == OBJ_POTIONS || c.base_type == OBJ_SCROLLS;
        }

        bool is_enabled() const override
        {
            if (!is_valid())
                return false;
            const item_def& c = you.inv[item_slot];
            if (!item_type_known(c))
                return false;
            else if (c.base_type == OBJ_POTIONS)
            {
                ASSERT(get_potion_effect(static_cast<potion_type>(c.sub_type)));
                return you.can_drink(true) && !you.berserk()
                    && get_potion_effect(static_cast<potion_type>(c.sub_type))->can_quaff();
            }
            else if (c.base_type == OBJ_SCROLLS)
            {
                return cannot_read_item_reason(&c).empty()
                    && scroll_hostile_check(
                                    static_cast<scroll_type>(c.sub_type));
            }
            else
                return false; // ASSERT?
        }

        bool is_targeted() const override
        {
            // only pot that might conceivably need a targeter is attraction?
            if (!is_valid() || you.inv[item_slot].base_type == OBJ_POTIONS
                || !item_type_known(you.inv[item_slot]))
            {
                return false;
            }
            return scroll_has_targeter(
                        static_cast<scroll_type>(you.inv[item_slot].sub_type));
        }

        string quiver_verb() const override
        {
            if (!is_valid())
                return "Buggy";
            return you.inv[item_slot].base_type == OBJ_SCROLLS ? "Read"
                                                               : "Drink";
        }

        bool use_autofight_targeting() const override { return false; }

        void trigger(dist &t) override
        {
            if (!is_valid())
                return;

            set_target(t);

            // enable autofire triggering for everything but blinking
            if (you.inv[item_slot].sub_type != SCR_BLINKING)
                target.target = you.pos();


            item_def& c = you.inv[item_slot];
            if (c.base_type == OBJ_POTIONS)
                drink(&c);
            else if (c.base_type == OBJ_SCROLLS)
                read(&c, &target);

        }

        vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool menu=false) const override
        {
            UNUSED(menu); // TODO: implement inscription checking for
            // !menu -- but this is pretty academic right now since these are
            // never in the menu or cycle order
            const int inv_start = (menu ? 0 : Options.fire_items_start);

            // go by pack order, subsort potions before scrolls
            vector<shared_ptr<action>> scrolls;
            vector<shared_ptr<action>> result;
            for (int slot = inv_start; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<consumable_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled()))
                {
                    if (you.inv[slot].base_type == OBJ_POTIONS)
                        result.push_back(std::move(w));
                    else
                        scrolls.push_back(std::move(w));
                }
            }
            result.insert(result.end(), scrolls.begin(), scrolls.end());
            return result;
        }
    };

    struct wand_action : public item_action
    {
        wand_action(int slot=-1, string _save_key="wand_action")
            : item_action(_save_key, slot)
        {
        }

        bool is_enabled() const override
        {
            const item_def *item = item_slot == -1 ? nullptr : &you.inv[item_slot];
            return cannot_evoke_item_reason(item).empty();
        }

        // n.b. implementing do_inscription_check for OPER_EVOKE is not needed
        // for this class as long as it is checked in evoke_item.

        virtual bool is_valid() const override
        {
            return item_action::is_valid()
                            && you.inv[item_slot].base_type == OBJ_WANDS;
        }

        bool is_targeted() const override
        {
            return true;
        }

        bool allow_autofight() const override
        {
            if (!is_valid() || !is_enabled()) // need to check item validity
                return false;

            switch (you.inv[item_slot].sub_type)
            {
            case WAND_DIGGING:     // non-damaging wands
            case WAND_POLYMORPH:
            case WAND_CHARMING:
            case WAND_PARALYSIS:
                return false;
            default:
                return true;
            }
        }

        // TODO: uses_mp for wand mp mutation? Because this mut no longer forces
        // mp use, the result is somewhat weird

        void trigger(dist &t) override
        {
            set_target(t);
            if (!is_valid())
                return;

            if (!is_enabled())
            {
                const item_def *item = item_slot == -1 ? nullptr : &you.inv[item_slot];
                mpr(cannot_evoke_item_reason(item));
                return;
            }

            // to apply smart targeting behavior for iceblast; should have no
            // impact on other wands
            target.find_target = true;
            if (autofight_check())
                return;
            if (!check_warning_inscriptions(you.inv[item_slot], OPER_EVOKE))
                return;

            evoke_item(you.inv[item_slot], &target);

            t = target; // copy back, in case they are different
        }

        virtual string quiver_verb() const override
        {
            return "Zap";
        }

        virtual vector<shared_ptr<action>> get_fire_order(
            bool allow_disabled=true, bool menu=false) const override
        {
            const int inv_start = (menu ? 0 : Options.fire_items_start);

            // go by pack order
            vector<shared_ptr<action>> result;
            for (int slot = inv_start; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<wand_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled())
                    // for fire order, treat =F inscription as disabling
                    && (menu
                        || _fireorder_inscription_ok(slot, true) != false)
                    // skip digging for fire cycling, it seems kind of
                    // non-useful? Can still be force-quivered from inv.
                    // (Maybe do this with autoinscribe?)
                    && you.inv[slot].sub_type != WAND_DIGGING)
                {
                    result.push_back(std::move(w));
                }
            }
            return result;
        }
    };

    static bool _misc_needs_manual_targeting(int subtype)
    {
            // autotargeting seems less useful on the others. Maybe this should
            // be configurable somehow?
        return subtype != MISC_PHIAL_OF_FLOODS;
    }

    struct misc_action : public wand_action
    {
        misc_action(int slot=-1) : wand_action(slot, "misc_action")
        {
        }

        bool is_valid() const override
        {
            // MISC_ZIGGURAT is valid (so can be force quivered) but is skipped
            // in the fire order
            return item_action::is_valid()
                            && you.inv[item_slot].base_type == OBJ_MISCELLANY;
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

            switch (you.inv[item_slot].sub_type)
            {
            case MISC_ZIGGURAT:     // non-damaging misc items
            case MISC_SACK_OF_SPIDERS:
            case MISC_BOX_OF_BEASTS:
            case MISC_HORN_OF_GERYON:
            case MISC_QUAD_DAMAGE:
            case MISC_PHANTOM_MIRROR:
            case MISC_GRAVITAMBOURINE:
                return false;
            default:
                return true;
            }
        }

        void trigger(dist &t) override
        {
            if (is_valid()
                && _misc_needs_manual_targeting(you.inv[item_slot].sub_type))
            {
                t.interactive = true;
            }
            wand_action::trigger(t);
        }

        string quiver_verb() const override
        {
            return "Evoke";
        }

        bool is_targeted() const override
        {
            if (!is_valid())
                return false;
            switch (you.inv[item_slot].sub_type)
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
            bool allow_disabled=true, bool menu=false) const override
        {
            // go by pack order
            const int inv_start = (menu ? 0 : Options.fire_items_start);
            vector<shared_ptr<action>> result;
            for (int slot = inv_start; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<misc_action>(slot);
                if (w->is_valid()
                    && (allow_disabled || w->is_enabled())
                    && (menu
                        || _fireorder_inscription_ok(slot, true) != false)
                    // TODO: autoinscribe =f?
                    && you.inv[slot].sub_type != MISC_ZIGGURAT)
                {
                    result.push_back(std::move(w));
                }
            }
            return result;
        }
    };

    void action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "action";
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
        // TODO: use save_key for item_action subtypes?
        if (type == "ammo_action")
            return make_shared<ammo_action>(param);
#if TAG_MAJOR_VERSION == 34
        else if (type == "launcher_ammo_action")
            return make_shared<ammo_action>(-1);
#endif
        else if (type == "spell_action")
            return make_shared<spell_action>(static_cast<spell_type>(param));
        else if (type == "ability_action")
            return make_shared<ability_action>(static_cast<ability_type>(param));
        else if (type == "consumable_action")
            return make_shared<consumable_action>(param);
        else if (type == "wand_action")
            return make_shared<wand_action>(param);
        else if (type == "misc_action")
            return make_shared<misc_action>(param);
        else if (type == "fumble_action")
            return make_shared<fumble_action>(param);
        else if (type == "melee_action")
            return make_shared<melee_action>();
        else if (type == "ranged_action")
            return make_shared<ranged_action>();
        else
            return make_shared<action>();
    }

    shared_ptr<action> find_ammo_action()
    {
        // Felids have no use for launchers or ammo.
        if (you.has_mutation(MUT_NO_GRASPING))
            return make_shared<ammo_action>(-1);

        if (you.weapon() && is_range_weapon(*you.weapon()))
            return make_shared<ranged_action>();

        vector<int> order;
        _get_item_fire_order(order, false, false);
        const int slot = order.empty() ? -1 : order[0];
        return make_shared<ammo_action>(slot);
    }

    action_cycler::action_cycler(shared_ptr<action> init)
        : current(init)
    { }

    // by default, initialize as invalid, not empty
    action_cycler::action_cycler()
        : action_cycler(make_shared<ammo_action>(-1))
    { }

    void action_cycler::save(const string key) const
    {
        auto &targ = you.props[key].get_table();
        get()->save(targ);
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
            // in the quiver.
            set(find_ammo_action());
            if (!get()->is_valid())
                cycle();
            save(key);
        }

        auto &targ = you.props[key].get_table();
        set(_load_action(targ));
        CrawlVector &history_vec = you.props[key]["history"].get_vector();
        history.clear();
        for (auto &val : history_vec)
        {
            auto a = _load_action(val.get_table());
            history.push_back(std::move(a));
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
            {
                if (!_fireorder_inscription_ok((*it)->get_item(), false))
                    continue;

                return *it;
            }
        }
        return nullptr;
    }

    /**
     * Replace the current action without affecting history.
     */
    bool action_cycler::replace(const shared_ptr<action> new_act)
    {
        auto n = new_act ? new_act : make_shared<action>();

        const bool diff = *n != *get();
        current = std::move(n);
        set_needs_redraw();
        return diff;
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
        auto old = std::move(current);
        current = std::move(n);

        if (diff)
        {
            // only store the most recently used instance of any action in the
            // history
            history.erase(remove(history.begin(), history.end(), old), history.end());
            // this may push back an invalid action, which is useful for all
            // sorts of reasons
            history.push_back(std::move(old));

            if (history.size() > 6) // 6 chosen arbitrarily
                history.erase(history.begin());

            // side effects, ugh. Update the fire history, and play a sound
            // if needed. TODO: refactor so this is less side-effect-y
            // somehow?
            const int item_slot = get()->get_item();
            if (item_slot >= 0 && you.inv[item_slot].defined())
                you.m_quiver_history.set_quiver(you.inv[item_slot]);
#ifdef USE_SOUND
            parse_sound(CHANGE_QUIVER_SOUND);
#endif
        }
        set_needs_redraw();
        return diff;
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

        // ugh. (n.b. this is ok with const because the pointer itself isn't modified)
        current->default_fire_context = this;
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

    // convert fire_type bitfields to action types
    static void _flag_to_action_types(vector<shared_ptr<action>> &action_types, int flag)
    {
         if (flag & FIRE_LAUNCHER)
            action_types.push_back(make_shared<ranged_action>());
        if (flag & FIRE_THROWING) // don't differentiate these, handled internal to ammo_action
            action_types.push_back(make_shared<ammo_action>(-1));
        if (flag & FIRE_SPELL)
            action_types.push_back(make_shared<spell_action>(SPELL_NO_SPELL));
        if (flag & FIRE_EVOKABLE)
        {
            action_types.push_back(make_shared<wand_action>(-1));
            action_types.push_back(make_shared<misc_action>(-1));
        }
        if (flag & FIRE_ABILITY)
            action_types.push_back(make_shared<ability_action>(ABIL_NON_ABILITY));
    }

    static void _check_and_add_actions(vector<shared_ptr<action>> &action_types,
        int f, int flag, int &done)
    {
        if ((f & flag) && !(flag & done))
        {
            _flag_to_action_types(action_types, flag);
            done |= flag;
        }
    }

    static shared_ptr<action> _get_next_action_type(shared_ptr<action> a,
        int dir, bool allow_disabled)
    {
        // this all seems a bit messy

        // Construct the type order from Options.fire_order:
        vector<shared_ptr<action>> action_types;
        int done = 0x0;
        // Produce an ordering of the broad classes, each corresponding to one
        // or more subtypes of ammo_action. Classes are responsible for ordering
        // within this, though currently only launchers and throwing implement
        // sub-ordering.
        // This doesn't support ordering ammo subtypes interleaved with anything
        // else
        vector<int> flags_to_check =
            { FIRE_LAUNCHER, FIRE_THROWING, FIRE_SPELL, FIRE_EVOKABLE, FIRE_ABILITY };
        for (auto f : Options.fire_order)
            for (auto flag : flags_to_check)
                _check_and_add_actions(action_types, f, flag, done);

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
            auto n = result->find_next(dir, allow_disabled, true);
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
    shared_ptr<action> action_cycler::next(int dir, bool allow_disabled) const
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

    void action_cycler::on_item_pickup(int slot)
    {
        if (get()->is_valid()
            || _fireorder_inscription_ok(slot, false) == false)
        {
            return;
        }
        const item_def &item = you.inv[slot];
        for (unsigned int i_flags = 0; i_flags < Options.fire_order.size();
             i_flags++)
        {
            const fire_type ftyp = (fire_type)Options.fire_order[i_flags];
            if (_item_matches(item, ftyp, false))
            {
                set(make_shared<ammo_action>(slot));
                return;
            }
        }
    }

    void action_cycler::on_actions_changed()
    {
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


        shared_ptr<action> a = nullptr;
        // use ammo as the fallback -- may well end up invalid
        if (!a || !a->is_valid())
            a = make_shared<ammo_action>(slot);
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
        else if (you.inv[slot].base_type == OBJ_SCROLLS
            || you.inv[slot].base_type == OBJ_POTIONS)
        {
            return make_shared<consumable_action>(slot);
        }
        else if (you.weapon() && you.weapon()->link == slot
                                && is_range_weapon(*you.weapon()))
        {
            return get_primary_action();
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
        const item_def* weapon = you.weapon();
        if (weapon && is_range_weapon(*weapon))
            return make_shared<ranged_action>();
        return make_shared<melee_action>(); // always valid
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
     * @abil the ability to use
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

    static vector<shared_ptr<action>> _menu_quiver_item_order()
    {
        vector<shared_ptr<action>> actions;
        // TODO: this is kind of ugly
        auto tmp = ranged_action().get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = ammo_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = wand_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        tmp = misc_action(-1).get_fire_order(true, true);
        actions.insert(actions.end(), tmp.begin(), tmp.end());
        return actions;
    }


    static bool _any_spells_to_quiver()
    {
        return you.spell_no;
    }

    static bool _any_abils_to_quiver()
    {
        return your_talents(true, true).size() > 0;
    }

    static bool _any_items_to_quiver()
    {
        // regular species can force-quiver any (non-equipped) item, but
        // felids have a more limited selection, so we need to directly
        // calculate it.
        return you.has_mutation(MUT_NO_GRASPING)
            ? any_items_of_type(OSEL_QUIVER_ACTION_FORCE)
            : inv_count() > 0;
    }

    // note for editing this: Menu::action is defined and will take precedence
    // over quiver::action unless the quiver namespace is explicit.
    class ActionSelectMenu : public Menu
    {
    public:
        ActionSelectMenu(action_cycler &_quiver, bool _allow_empty)
            : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
                    | MF_ARROWS_SELECT | MF_WRAP | MF_SPECIAL_MINUS),
              cur_quiver(_quiver), allow_empty(_allow_empty),
              any_spells(_any_spells_to_quiver()),
              any_abilities(_any_abils_to_quiver()),
              // regular species can force-quiver any (non-equipped) item, but
              // felids have a more limited selection, so we need to directly
              // calculate it.
              any_items(_any_items_to_quiver())
        {
            set_tag("actions");
            set_title(new MenuEntry("", MEL_TITLE));

            on_single_selection = [this](const MenuEntry& item)
                {
                    // XX can this use indices rather than pointers?
                    const shared_ptr<quiver::action> *a =
                        static_cast<shared_ptr<quiver::action> *>(item.data);
                    ASSERT(a);
                    return !set_to_quiver(*a);
                };

            action_cycle = Menu::CYCLE_TOGGLE;
            menu_action  = Menu::ACT_EXECUTE;

            // initial call only
            if (focus_mode == Focus::INIT)
            {
                focus_mode = Options.quiver_menu_focus
                    ? Focus::ITEM
                    : Focus::NONE;
            }

            populate();
        }

        action_cycler &cur_quiver;
        bool allow_empty;

        bool set_to_quiver(shared_ptr<quiver::action> s)
        {
            if (s && s->is_valid() && (allow_empty || *s != quiver::action()))
                return ::quiver::set_to_quiver(s, cur_quiver);
            return false;
        }

        bool pointless()
        {
            return !(any_spells || any_abilities || any_items);
        }

        string get_keyhelp(bool) const override
        {
            string s = more_message + "\n";

            if (any_items)
                s += "[<w>*/%</w>] inventory  ";
            if (any_spells)
                s += "[<w>&</w>] all spells  ";
            if (any_abilities)
                s += "[<w>^</w>] all abilities  ";


            string mode = make_stringf("%s focus mode: %s",
                menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE).c_str(),
                focus_mode == Focus::NONE ? "<w>off</w>|on"
                                          : "off|<w>on</w>");;

            return pad_more_with(s, mode);
        }

    protected:
        void add_action(const shared_ptr<quiver::action> &a, menu_letter hotkey)
        {
            if (!a || !a->is_valid())
                return;
            string action_desc = a->quiver_description();
            if (you.quiver_action.item_is_quivered(a->get_item()))
                action_desc += " (quivered)";
            MenuEntry *me = new MenuEntry(action_desc,
                                                MEL_ITEM, 1,
                                                (int) hotkey);
            me->colour = a->quiver_color();
            me->data = (void *) &a; // pointer to vector element - don't change the vector!
            me->indent_no_hotkeys = true;
#ifdef USE_TILE
            for (auto t : a->get_tiles())
                me->add_tile(t);
#endif
            add_entry(me);
        }

        void sync_focus(bool force=false)
        {
            if (focus_mode == Focus::NONE
                || last_hovered < 0 && is_set(MF_ARROWS_SELECT))
            {
                if (force)
                    update_menu(true);
                return;
            }

            if (last_hovered >= 0)
            {
                // make sure focus is consistent with current hover
                auto old_mode = focus_mode;
                if (last_hovered < static_cast<int>(first_spell))
                    focus_mode = Focus::ITEM;
                else if (last_hovered < static_cast<int>(first_abil))
                    focus_mode = Focus::SPELL;
                else
                    focus_mode = Focus::ABIL;
                if (focus_mode == old_mode && !force)
                    return;
            }

            size_t i;
            // XX generalize somehow? See also UseItemMenu
            for (i = 0; i < first_spell; i++)
                if (items[i]->level == MEL_ITEM)
                    items[i]->set_enabled(focus_mode == Focus::ITEM);
            for (; i < first_abil; i++)
                if (items[i]->level == MEL_ITEM)
                    items[i]->set_enabled(focus_mode == Focus::SPELL);
            for (; i < items.size(); i++)
                if (items[i]->level == MEL_ITEM)
                    items[i]->set_enabled(focus_mode == Focus::ABIL);

            update_menu(true);
        }

        void toggle_focus_mode()
        {
            if (focus_mode == Focus::NONE)
            {
                Options.quiver_menu_focus = true;
                if (!is_set(MF_ARROWS_SELECT)
                    || last_hovered < static_cast<int>(first_spell))
                {
                    // XX this is probably awkward if there are a lot of
                    // actions and the menu is scrolled, but that is rare
                    focus_mode = Focus::ITEM;
                }
                else if (last_hovered < static_cast<int>(first_abil))
                    focus_mode = Focus::SPELL;
                else
                    focus_mode = Focus::ABIL;
            }
            else
            {
                Options.quiver_menu_focus = false;
                focus_mode = Focus::NONE;
            }
            Options.prefs_dirty = true;
            update_more();
            populate();
        }

        void populate()
        {
            menu_letter hotkey;
            // resets everything
            clear();
            actions = _menu_quiver_item_order();
            const auto spell_actions =
                    spell_action(SPELL_NO_SPELL).get_fire_order(true, true);
            const auto abil_actions =
                    ability_action(ABIL_NON_ABILITY).get_fire_order(true, true);

            // don't bother with headers unless at least two categories are
            // present
            const bool show_headers = (!actions.empty() + !spell_actions.empty()
                                        + !abil_actions.empty()) > 1;
            const auto it_count = actions.size();
            const auto spell_count = spell_actions.size();
            actions.insert(actions.end(), spell_actions.begin(), spell_actions.end());
            actions.insert(actions.end(), abil_actions.begin(), abil_actions.end());

            // this key shortcut does still work without arrow selection, but
            // it typically doesn't do much in this menu.
            const string keyhelp =
                make_stringf(" <lightgrey>(%s to cycle)</lightgrey>",
                            menu_keyhelp_cmd(CMD_MENU_CYCLE_HEADERS).c_str());

            first_item = 0;
            first_spell = it_count;
            first_abil = first_spell + spell_count;

            if (it_count && show_headers)
            {
                add_entry(
                    new MenuEntry("<lightcyan>Items</lightcyan>" + keyhelp,
                    MEL_SUBTITLE));
                first_spell += 1;
                first_abil += 1;
            }
            size_t i = 0;
            for (const auto &a : actions)
            {
                if (i == 0)
                    first_item = items.size();
                if (i == it_count && spell_count && show_headers)
                {
                    add_entry(
                        new MenuEntry("<lightcyan>Spells</lightcyan>" + keyhelp,
                        MEL_SUBTITLE));
                    first_spell += 1;
                    first_abil += 1;
                }
                else if (i == it_count + spell_count && show_headers)
                {
                    add_entry(
                        new MenuEntry("<lightcyan>Abilities</lightcyan>" + keyhelp,
                        MEL_SUBTITLE));
                    first_abil += 1;
                }

                if (focus_mode != Focus::NONE)
                    hotkey = a->source_hotkey();

                add_action(a, hotkey);

                if (focus_mode == Focus::NONE)
                    hotkey++;
                i++;
            }

            if (actions.size() == 0)
            {
                more_message =
                    "<lightred>No regular actions available to quiver.</lightred>";
            }
            if (last_hovered < 0)
            {
                // initial display
                if (focus_mode == Focus::ITEM || focus_mode == Focus::NONE)
                    set_hovered(first_item);
                else if (focus_mode == Focus::SPELL)
                    set_hovered(first_spell);
                else if (focus_mode == Focus::ABIL)
                    set_hovered(first_abil);
                // fixup in case mode is items and there are no items, etc.
                if (last_hovered >= 0 && items[last_hovered]->level != MEL_ITEM)
                    cycle_hover();
            }
            sync_focus(true);
        }

        void set_hovered(int hovered, bool force=false) override
        {
            const bool skip_sync = hovered == last_hovered;
            Menu::set_hovered(hovered, force);
            // need to be a little bit careful about recursion potential here:
            // update_menu calls set_hovered to sanitize low level UI state.
            if (!skip_sync)
                sync_focus();
        }

        bool _choose_from_inv()
        {
            int slot = prompt_invent_item(allow_empty
                                            ? "Quiver which item? (- for none, * to toggle full inventory)"
                                            : "Quiver which item? (* to toggle full inventory)",
                                          menu_type::invlist, OSEL_QUIVER_ACTION,
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

            return selected < 0 || selected >= static_cast<int>(talents.size())
                || !set_to_quiver(make_shared<ability_action>(talents[selected].which));
        }

        bool cycle_headers(bool forward) override
        {
            // TODO: implement direction
            if (!forward)
                return false;
            if (!is_set(MF_ARROWS_SELECT))
            {
                last_hovered = -1; // suppress any mouse hover
                // need to manually focus mode for this case
                // can this be generalized?
                if (focus_mode == Focus::ITEM)
                {
                    focus_mode = Focus::SPELL;
                    if (!item_visible(first_spell))
                        set_scroll(first_spell);
                }
                else if (focus_mode == Focus::SPELL)
                {
                    focus_mode = Focus::ABIL;
                    if (!item_visible(first_abil))
                        set_scroll(first_abil);
                }
                else if (focus_mode == Focus::ABIL)
                {
                    if (!item_visible(first_item))
                        set_scroll(first_item);
                    focus_mode = Focus::ITEM;
                }
                sync_focus();
                return true;
            }
            else
                return Menu::cycle_headers(forward);
        }

        bool cycle_mode(bool) override
        {
            toggle_focus_mode();
            return true;
        }

        bool process_key(int key) override
        {
            // TODO: some kind of view action option?
            if (allow_empty && key == '-')
            {
                set_to_quiver(make_shared<quiver::action>());
                // TODO maybe drop this messaging?
                mpr("Clearing quiver.");
                return false;
            }
            else if (isadigit(key))
            {
                item_def *item = digit_inscription_to_item(key, OPER_QUIVER);
                if (item && in_inventory(*item))
                {
                    auto a = slot_to_action(item->link, true);
                    // XX would be better to show an error if a is invalid?
                    if (a->is_valid())
                    {
                        set_to_quiver(a);
                        return false;
                    }
                }
            }
            else if ((key == '*' || key == '%') && any_items)
                return _choose_from_inv();
            else if (key == '&' && any_spells)
            {
                const int skey = list_spells(false, false, false,
                                                    "quiver");
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
            string s = "Quiver which action? ";
            vector<string> extra_cmds;

            if (allow_empty)
                s += "([<w>-</w>] to clear)";
            return formatted_string::parse_string(s);
        }

        bool any_spells;
        bool any_abilities;
        bool any_items;
        size_t first_item;
        size_t first_spell;
        size_t first_abil;
        enum Focus { INIT, NONE, ITEM, SPELL, ABIL };
        static Focus focus_mode;
        vector<shared_ptr<quiver::action>> actions;
        string more_message;
        friend void quiver::reset_state();
    };

    ActionSelectMenu::Focus ActionSelectMenu::focus_mode = ActionSelectMenu::Focus::INIT;

    void reset_state()
    {
        ActionSelectMenu::focus_mode = ActionSelectMenu::Focus::INIT;
    }

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

            what_happened =
                a && targeter_handles_key(
                                static_cast<command_type>(a->target.cmd_result))
                        ? static_cast<command_type>(a->target.cmd_result)
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

    bool anything_to_quiver()
    {
        return _any_items_to_quiver()
            || _any_spells_to_quiver()
            || _any_abils_to_quiver();
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
        // TODO: dividers or subtitles for each category?
        ActionSelectMenu menu(cur_quiver, allow_empty);

        if (menu.pointless())
        {
            mpr("You have nothing to quiver.");
            return;
        }

        menu.show();
    }

    bool set_to_quiver(shared_ptr<quiver::action> s,
                                action_cycler &cur_quiver=you.quiver_action)
    {
        if (s && s->is_valid())
        {
            if (!_quiver_inscription_ok(s->get_item()))
            {
                const string prompt = make_stringf("Really quiver %s?",
                    you.inv[s->get_item()].name(DESC_INVENTORY).c_str());
                if (!yesno(prompt.c_str(), true, 'n'))
                    return false;
            }
            // XX does this really need to be so flexible
            cur_quiver.set(s);
            return true;
        }
        return false;
    }


    // this class is largely legacy code -- can it be done away with?
    // or refactored to use actions.
    ammo_history::ammo_history()
    {

    }

    int ammo_history::get_last_ammo() const
    {
        if (!you.props.exists(LAST_MISSILE_SLOT_KEY))
            return -1;
        return you.props[LAST_MISSILE_SLOT_KEY].get_int();
    }

    void ammo_history::set_quiver(const item_def &item)
    {
        you.props[LAST_MISSILE_SLOT_KEY] = _get_pack_slot(item);
        you.redraw_quiver = true;
    }

    void ammo_history::maybe_swap(int from_slot, int to_slot)
    {
        if (get_last_ammo() == from_slot)
        {
            you.props[LAST_MISSILE_SLOT_KEY] = to_slot;
            you.redraw_quiver = true;
        }
    }

    // Notification that item was fired
    void ammo_history::on_item_fired(const item_def& item, bool explicitly_chosen)
    {
        if (!explicitly_chosen)
        {
            // If the item was not actively chosen, i.e. just automatically
            // passed into the quiver, don't add it to the quiver history for
            // this ammo type. This lets us revert back to an actively chosen
            // ammo on pickup.
            you.redraw_quiver = true;
            return;
        }

        // Don't do anything if this item is not really fit for throwing.
        if (!is_throwable(&you, item))
            return;
        set_quiver(item);
    }

    // ----------------------------------------------------------------------
    // Save/load
    // ----------------------------------------------------------------------

    // legacy marshalling/unmarshalling code
#if TAG_MAJOR_VERSION == 34
    static const short QUIVER_COOKIE = short(0xb015);

    void ammo_history::load(reader& inf)
    {
        if (inf.getMinorVersion() >= TAG_MINOR_MOSTLY_REMOVE_AMMO)
            return;

        // warning: this is called in the unmarshalling sequence before the
        // inventory is actually in place
        const short cooky = unmarshallShort(inf);
        ASSERT(cooky == QUIVER_COOKIE); (void)cooky;

        auto dummy = item_def();
        unmarshallItem(inf, dummy); // was: m_last_weapon
        unmarshallInt(inf); // was: m_last_used_type;

        const unsigned int count = unmarshallInt(inf);
        for (unsigned int i = 0; i < count; i++)
            unmarshallItem(inf, dummy);
    }
#endif

    void on_actions_changed()
    {
        you.quiver_action.on_actions_changed();
    }

    void on_item_pickup(int slot)
    {
        you.quiver_action.on_item_pickup(slot);
    }

    void set_needs_redraw()
    {
        you.quiver_action.set_needs_redraw();
    }

    void on_weapon_changed()
    {
        if (Options.launcher_autoquiver && you.weapon()
            && is_range_weapon(*you.weapon())
            && _fireorder_inscription_ok(you.equip[EQ_WEAPON], false) != false)
        {
            you.quiver_action.set(get_primary_action());
        }

        if (!you.quiver_action.get()->is_valid())
        {
            auto a = you.quiver_action.find_last_valid();
            if (a)
                you.quiver_action.set(a);
        }
    }

    void on_newchar()
    {
        // look for something fun to quiver
        you.quiver_action.cycle();
    }
}


// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

// Helper for ammo _get_fire_order. Ammo only.
// Types may actually contain more than one fire_type.
static bool _item_matches(const item_def &item, fire_type types, bool manual)
{
    // TODO: refactor into something less annoying? This is all a semi-duplicate
    // of is_valid code...
    ASSERT(item.defined());

    // XX code duplication with _fireorder_inscription_ok
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

    return false;
}

// Returns inv slot that contains a missile of the given subtype.
// or -1 if not in inv.
static int _get_pack_slot(const item_def &item)
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
            // =f prevents item from being autoquivered.
            if (!quiver::_fireorder_inscription_ok(i, false))
                continue;

            return i;
        }
    }

    return -1;
}

static bool _items_similar(const item_def& a, const item_def& b, bool force)
{
    return items_similar(a, b) && (force || a.slot == b.slot);
}

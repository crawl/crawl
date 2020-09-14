/**
 * @file
 * @brief Player quiver functionality
**/

#include "AppHdr.h"

#include "quiver.h"

#include <algorithm>

#include "env.h"
#include "evoke.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "options.h"
#include "player.h"
#include "prompt.h"
#include "sound.h"
#include "stringutil.h"
#include "tags.h"
#include "throw.h"

static int _get_pack_slot(const item_def&);
static bool _item_matches(const item_def &item, fire_type types,
                          const item_def* launcher, bool manual);
static bool _items_similar(const item_def& a, const item_def& b,
                           bool force = true);

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

    void action::reset()
    {
        target = dist();
    };

    formatted_string action::quiver_description() const
    {
        // TODO: or unavailable?
        return formatted_string::parse_string(
                        "<darkgrey>Nothing quivered</darkgrey>");
    }

    shared_ptr<action> action::find_next(int dir, bool loop) const
    {
        auto o = get_fire_order();
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

        if (i == static_cast<int>(o.size()))
            return o[0];

        i++;
        if (!loop && i >= static_cast<int>(o.size()))
            return nullptr;

        return o[i % o.size()];
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

        // If in a net, cannot launch anything.
        if (you.attribute[ATTR_HELD])
            return;

        for (int i_inv = inv_start; i_inv < ENDOFPACK; i_inv++)
        {
            const item_def& item = you.inv[i_inv];
            if (!item.defined())
                continue;

            const auto l = is_launched(&you, launcher, item);

            // Don't do anything if this item is not really fit for throwing.
            if (l == launch_retval::FUMBLED)
                continue;

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
                && strstr(item.inscription.c_str(), manual ? "=F" : "=f"))
            {
                continue;
            }

            for (unsigned int i_flags = 0; i_flags < Options.fire_order.size();
                 i_flags++)
            {
                if (_item_matches(item, (fire_type) Options.fire_order[i_flags],
                                  launcher, manual))
                {
                    order.push_back((i_flags<<16) | (i_inv & 0xffff));
                    break;
                }
            }
        }

        sort(order.begin(), order.end());

        for (unsigned int i = 0; i < order.size(); i++)
            order[i] &= 0xffff;
    }

    /**
     * An ammo_action is an action that fires ammo from a slot in the
     * inventory. This covers both launcher-based firing, and throwing.
     */
    struct ammo_action : public action
    {
        ammo_action(int slot=-1) : action(), ammo_slot(slot)
        {
        }

        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return ammo_slot == static_cast<const ammo_action&>(other).ammo_slot;
        }

        bool is_enabled() const override
        {
            if (fire_warn_if_impossible(true))
                return false;

            // disable if there's a no-fire inscription on ammo
            // maybe this should just be skipped altogether for this case?
            // or prompt on trigger..
            return check_warning_inscriptions(you.inv[ammo_slot], OPER_FIRE)
                    && (!you.weapon()
                        || is_launched(&you, you.weapon(), you.inv[ammo_slot])
                                                    != launch_retval::LAUNCHED
                        || check_warning_inscriptions(*you.weapon(), OPER_FIRE));
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
            const item_def *weapon = you.weapon();

            return _item_matches(ammo, (fire_type) 0xffff, // TODO: ...
                                 weapon, false);
        }

        bool is_targeted() const override
        {
            return true;
        }

        void trigger(dist &t) override
        {
            target = t;
            if (!is_valid())
                return;
            if (!is_enabled())
            {
                // for messaging (TODO refactor; message about inscriptions?)
                fire_warn_if_impossible();
                return;
            }

            bolt beam;
            throw_it(beam, ammo_slot, &target);

            t = target; // copy back, in case they are different
        }

        virtual formatted_string quiver_description() const override
        {
            ASSERT_RANGE(ammo_slot, -1, ENDOFPACK);
            if (!is_valid())
            {
                // TODO: I don't quite understand the fire_warn... logic here,
                // and so have slightly simplified it from the output.cc version
                if (fire_warn_if_impossible(true))
                {
                    return formatted_string::parse_string(
                        "<darkgrey>Quiver unavailable</darkgrey>");
                }
                else
                {
                    // TODO: ???
                    return formatted_string::parse_string(
                        "<darkgrey>Nothing quivered</darkgrey>");
                }
            }
            formatted_string qdesc;

            char hud_letter = '-';
            const item_def& quiver = you.inv[ammo_slot];
            ASSERT(quiver.link != NON_ITEM);
            hud_letter = index_to_letter(quiver.link);
            // TODO: or just lightgrey?
            qdesc.textcolour(Options.status_caption_colour);
            const launch_retval projected = is_launched(&you, you.weapon(),
                                                                    quiver);
            switch (projected)
            {
                case launch_retval::FUMBLED:  qdesc.cprintf("Toss: ");  break;
                case launch_retval::LAUNCHED: qdesc.cprintf("Fire: ");  break;
                case launch_retval::THROWN:   qdesc.cprintf("Throw: "); break;
                case launch_retval::BUGGY:    qdesc.cprintf("Bug: ");   break;
            }
            qdesc.cprintf("%c) ", hud_letter);

            const string prefix = item_prefix(quiver);

            const int prefcol =
                menu_colour(quiver.name(DESC_PLAIN), prefix, "stats");
            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else if (prefcol != -1)
                qdesc.textcolour(prefcol);
            else
                qdesc.textcolour(LIGHTGREY);
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

        vector<shared_ptr<action>> get_fire_order() const override
        {
            vector<int> fire_order;
            _get_item_fire_order(fire_order, false, you.weapon(), true);

            vector<shared_ptr<action>> result;

            for (auto i : fire_order)
            {
                auto a = make_shared<ammo_action>(i);
                if (a->is_valid())
                    result.push_back(move(a));
            }
            return result;
        }

    private:
        int ammo_slot;
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
    static bool _spell_skips_initial_autotarget(spell_type s)
    {
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
        spell_action(spell_type s = SPELL_NO_SPELL) : spell(s) { };
        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return spell == static_cast<const spell_action&>(other).spell;
        }

        bool is_enabled() const override
        {
            return can_cast_spells(true) && !spell_is_useless(spell, true, true);
        }

        bool is_valid() const override
        {
            return is_valid_spell(spell) && you.has_spell(spell);
        }

        // as a practical matter, this determines whether the initial lua
        // target finder is used on shift-tab. It's also used in a somewhat
        // incorrect way right now to determine fire behavior (TODO).
        bool is_targeted() const override
        {
            // TODO: what spells does this miss?
            return !!(get_spell_flags(spell) & spflag::targeting_mask)
                    && !_spell_skips_initial_autotarget(spell);
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
            }
            else if (_spell_skips_initial_autotarget(spell))
            {
                target.target = coord_def(-1,-1);
                target.find_target = true;
            }

            // TODO: would it be feasible to allow dropping to manual targeting
            // if the only autotarget solution found would hit the player?
            // currently applies to something like firestorm, where direction
            // chooser autotargeting doesn't work very well (maybe on purpose)
            cast_a_spell(true, spell, &target);
            if (target.find_target && !target.isValid)
            {
                // It would be entirely possible to force manual targeting for
                // this case; I think it's not what players would expect so I'm
                // not doing it for now.
                // TODO: more consistency with autofight.lua messaging?
                mpr("Can't find an automatic target! Use z or Z to cast.");
            }
            t = target; // copy back, in case they are different
        }

        formatted_string quiver_description() const override
        {
            if (!is_valid())
            {
                return formatted_string::parse_string(
                    "<darkgrey>Nothing quivered</darkgrey>");
            }
            formatted_string qdesc;

            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("Cast: ");

            if (!is_enabled())
                qdesc.textcolour(DARKGREY);
            else
                qdesc.textcolour(LIGHTGREY);

            // TODO: is showing the spell letter useful?
            qdesc.cprintf("%s", spell_title(spell));
            return qdesc;
        }

        vector<shared_ptr<action>> get_fire_order() const override
        {
            // goes by letter order
            vector<shared_ptr<action>> result;
            for (int i = 0; i < 52; i++)
            {
                const char letter = index_to_letter(i);
                const spell_type s = get_spell_by_letter(letter);
                if (is_valid_spell(s)
                    && fail_severity(s) < Options.fail_severity_to_quiver)
                {
                    result.push_back(make_shared<spell_action>(s));
                }
            }
            return result;
        }

    private:
        spell_type spell;
    };

    // TODO: generalize to misc evokables? Code should be similar if targeting
    // can be easily implemented
    struct wand_action : public action
    {
        wand_action(int slot=-1) : action(), wand_slot(slot)
        {
        }

        void save(CrawlHashTable &save_target) const override; // defined below

        bool equals(const action &other) const override
        {
            // type ensured in base class
            return wand_slot == static_cast<const wand_action&>(other).wand_slot;
        }

        bool is_enabled() const override
        {
            return true;
        }

        bool is_valid() const override
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

        void trigger(dist &t) override
        {
            target = t;
            if (!is_valid() || !is_enabled())
                return;

            // to apply smart targeting behavior for iceblast; should have no
            // impact on other wands
            target.find_target = true;
            evoke_item(wand_slot, &target);

            t = target; // copy back, in case they are different
        }

        virtual formatted_string quiver_description() const override
        {
            ASSERT_RANGE(wand_slot, -1, ENDOFPACK);
            if (!is_valid())
            {
                return formatted_string::parse_string(
                                    "<darkgrey>Nothing quivered</darkgrey>");
            }
            formatted_string qdesc;

            char hud_letter = '-';
            const item_def& quiver = you.inv[wand_slot];
            ASSERT(quiver.link != NON_ITEM);
            hud_letter = index_to_letter(quiver.link);
            qdesc.textcolour(Options.status_caption_colour);
            qdesc.cprintf("Zap: %c) ", hud_letter);

            qdesc.textcolour(LIGHTGREY);
            qdesc += quiver.name(DESC_PLAIN, true);
            return qdesc;
        }

        int get_item() const override
        {
            return wand_slot;
        }

        vector<shared_ptr<action>> get_fire_order() const override
        {
            // go by pack order
            vector<shared_ptr<action>> result;
            for (int slot = 0; slot < ENDOFPACK; slot++)
            {
                auto w = make_shared<wand_action>(slot);
                if (!w->is_valid())
                    continue;
                // skip digging, it seems kind of non-useful? Can still be
                // force-quivered from inv
                if (you.inv[slot].sub_type == WAND_DIGGING)
                    continue;
                result.push_back(move(w));
            }
            return result;
        }

    private:
        int wand_slot;
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

    void spell_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "spell_action";
        save_target["param"] = static_cast<int>(spell);
    }

    void wand_action::save(CrawlHashTable &save_target) const
    {
        save_target["type"] = "wand_action";
        save_target["param"] = static_cast<int>(wand_slot);
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
            return make_shared<action>();

        const string &type = source["type"].get_string();

        if (type == "ammo_action")
            return make_shared<ammo_action>(source["param"].get_int());
        else if (type == "spell_action")
        {
            return make_shared<spell_action>(
                        static_cast<spell_type>(source["param"].get_int()));
        }
        else if (type == "wand_action")
            return make_shared<wand_action>(source["param"].get_int());
        else
            return make_shared<action>();
    }

    shared_ptr<action> find_action_from_launcher(const item_def *item)
    {
        // Felids have no use for launchers or ammo.
        if (you.species == SP_FELID)
        {
            auto result = make_shared<ammo_action>(-1);
            result->error = "You can't grasp things well enough to throw them.";
            return result;
        }

        int slot = you.m_quiver_history.get_last_ammo(item);

        // if no ammo of this type has been quivered, try looking at the fire
        // order.
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

    // TODO: is there a need for this function any more? It was originally used
    // on unmarshalling, but now everything that needs logic like this is just
    // calling find_action_from_launcher directly.
    shared_ptr<action> find_ammo_action()
    {
        // felids should never have an ammo history, so error setting will be
        // handled in find_action_from_launcher
        const int slot = you.m_quiver_history.get_last_ammo();
        if (slot < 0)
            return find_action_from_launcher(you.weapon());
        else
            return make_shared<ammo_action>(slot);
    }

    action_cycler::action_cycler() : current(make_shared<action>()) { };

    void action_cycler::save() const
    {
        auto &target = you.props["current_quiver_action"].get_table();
        get().save(target);
    }

    void action_cycler::load()
    {
        auto &target = you.props["current_quiver_action"].get_table();
        set(_load_action(target));
        // in case this is invalid, cycle. TODO: is this the right thing to do?
        on_actions_changed();
    }

    bool action_cycler::set(shared_ptr<action> n)
    {
        if (!n)
            n = make_shared<action>();
        const bool diff = *n != get();
        current = n;
        if (diff)
        {
            // side effects, ugh. Update the fire history, and play a sound
            // if needed. TODO: refactor so this is less side-effect-y
            // somehow?
            const int item_slot = get().get_item();
            if (item_slot >= 0)
            {
                const item_def item = you.inv[item_slot];
                ASSERT(item.defined());

                quiver::launcher t = quiver::AMMO_THROW;
                const item_def *weapon = you.weapon();
                if (weapon && item.launched_by(*weapon))
                    t = quiver::_get_weapon_ammo_type(weapon);

                you.m_quiver_history.set_quiver(you.inv[item_slot], t);

#ifdef USE_SOUND
                parse_sound(CHANGE_QUIVER_SOUND);
#endif
                // TODO: when should this messaging happen, if at all?
                // mprf("Quivering %s for %s.", you.inv[slot].name(DESC_INVENTORY).c_str(),
                //      t == quiver::AMMO_THROW    ? "throwing" :
                //      t == quiver::AMMO_SLING    ? "slings" :
                //      t == quiver::AMMO_BOW      ? "bows" :
                //                           "crossbows");
            }
        }
        return diff;
    }

    bool action_cycler::set(const action_cycler &other)
    {
        const bool diff = current != other.current;
        // don't use regular set: avoid all the side effects when importing
        // from another action cycler. (Used in targeting.)
        current = other.current;
        return diff;
    }

    // pitfall: if you do not use this return value by reference, polymorphism
    // will fail and you will end up with an action(). Easiest way to make
    // this mistake: `auto a = you.quiver_action.get()`.
    // (TODO: something to avoid this? Work with a shared_ptr after all?)
    action &action_cycler::get() const
    {
        // TODO: or find an action?
        ASSERT(current);

        return *current;
    }

    static shared_ptr<action> _get_next_action_type(shared_ptr<action> a, int dir)
    {
        // this all seems a bit messy

        // Construct the type order.
        vector<shared_ptr<action>> action_types;
        action_types.push_back(make_shared<ammo_action>(-1));
        action_types.push_back(make_shared<spell_action>(SPELL_NO_SPELL));
        action_types.push_back(make_shared<wand_action>(-1));

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
            auto n = result->find_next(dir, false);
            if (n && n->is_valid())
                return n;
        }

        // we've gone through everything -- somehow there are no valid actions,
        // not even using a
        return nullptr;
    }

    // not_null guaranteed
    shared_ptr<action> action_cycler::next(int dir)
    {
        // first try the next action of the same type
        shared_ptr<action> result = get().find_next(dir, false);
        // then, try to find a different action type
        if (!result || !result->is_valid())
            result = _get_next_action_type(get_ptr(), dir);

        // no valid actions
        if (!result)
            return make_shared<action>();

        return result;
    }

    bool action_cycler::cycle(int dir)
    {
        return set(next(dir));
    }

    void action_cycler::on_actions_changed()
    {
        if (!get().is_valid())
        {
            auto r = get().find_replacement();
            if (r && r->is_valid())
                set(r);
            else
                cycle();
        }
        you.redraw_quiver = true;
    }

    shared_ptr<action> slot_to_action(int slot)
    {
        if (slot < 0 || slot >= ENDOFPACK)
            return make_shared<action>();

        if (you.inv[slot].defined() && you.inv[slot].base_type == OBJ_WANDS)
            return make_shared<wand_action>(slot);

        // use ammo as the fallback -- may well end up invalid
        return make_shared<ammo_action>(slot);
    }

    bool action_cycler::set_from_slot(int slot)
    {
        return(set(slot_to_action(slot)));
    }

    bool action_cycler::clear()
    {
        return(set(make_shared<action>()));
    }

    // TODO: should this be a method of action_cycler?
    void choose()
    {
        // TODO: need to implement this for non-ammo actions
        if (you.species == SP_FELID)
        {
            mpr("You can't grasp things well enough to throw them.");
            return;
        }

        int slot = prompt_invent_item("Quiver which item? (- for none, * to show all)",
                                      menu_type::invlist, OSEL_THROWABLE,
                                      OPER_QUIVER, invprompt_flag::hide_known, '-');

        if (prompt_failed(slot))
            return;

        if (slot == PROMPT_GOT_SPECIAL)  // '-' or empty quiver
        {
            quiver::launcher t = quiver::_get_weapon_ammo_type(you.weapon());
            you.quiver_action.clear();
            you.m_quiver_history.empty_quiver(t);

            mprf("Clearing quiver.");
            return;
        }
        else
        {
            for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
            {
                if (you.equip[i] == slot)
                {
                    mpr("You can't quiver worn items.");
                    return;
                }
            }
        }
        you.quiver_action.set_from_slot(slot);
    }

    // this class is largely legacy code -- can it be done away with?
    // or refactored to use actions.
    // TODO: auto switch to last action when swapping away from a launcher --
    // right now it goes to an ammo only in that case.
    history::history()
        : m_last_used_type(quiver::AMMO_THROW)
    {
        COMPILE_CHECK(ARRAYSZ(m_last_used_of_type) == quiver::NUM_LAUNCHERS);
    }

    /**
     * Try to find a `default` ammo item to fire, based on whatever was last
     * used.
     */
    int history::get_last_ammo() const
    {
        return get_last_ammo(m_last_used_type);
    }

    int history::get_last_ammo(const item_def *launcher) const
    {
        return get_last_ammo(quiver::_get_weapon_ammo_type(launcher));
    }

    int history::get_last_ammo(quiver::launcher type) const
    {
        const int slot = _get_pack_slot(m_last_used_of_type[type]);
        ASSERT(slot < ENDOFPACK && (slot == -1 || you.inv[slot].defined()));
        return slot;
    }

    void history::set_quiver(const item_def &item, quiver::launcher ammo_type)
    {
        m_last_used_of_type[ammo_type] = item;
        m_last_used_of_type[ammo_type].quantity = 1;
        m_last_used_type  = ammo_type;
        you.redraw_quiver = true;
    }

    void history::empty_quiver(quiver::launcher ammo_type)
    {
        m_last_used_of_type[ammo_type] = item_def();
        m_last_used_of_type[ammo_type].quantity = 0;
        m_last_used_type  = ammo_type;
        you.redraw_quiver = true;
    }

    // Notification that item was fired
    void history::on_item_fired(const item_def& item, bool explicitly_chosen)
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
            m_last_used_type = t;
        }
        else
        {
            const launch_retval projected = is_launched(&you, you.weapon(),
                                                        item);

            // Don't do anything if this item is not really fit for throwing.
            if (projected == launch_retval::FUMBLED)
                return;

            dprf( "item %s is for throwing",
                 item.name(DESC_PLAIN).c_str());

            m_last_used_of_type[quiver::AMMO_THROW] = item;
            m_last_used_of_type[quiver::AMMO_THROW].quantity = 1;
            m_last_used_type = quiver::AMMO_THROW;
        }

        you.redraw_quiver = true;
    }

    // Called when the player has switched weapons
    void history::on_weapon_changed()
    {
        // Only switch actions if launcher type really changed
        const item_def* weapon = you.weapon();
        if (m_last_used_type != quiver::_get_weapon_ammo_type(weapon))
            you.quiver_action.set(quiver::find_action_from_launcher(weapon));
    }

    // ----------------------------------------------------------------------
    // Save/load
    // ----------------------------------------------------------------------

    // this save/load code is extremely legacy
    static const short QUIVER_COOKIE = short(0xb015);
    void history::save(writer& outf) const
    {
        // TODO: action-based marshalling/unmarshalling. But we will still need
        // the history here, probably.
        marshallShort(outf, QUIVER_COOKIE);

        marshallItem(outf, item_def()); // was: m_last_weapon
        marshallInt(outf, m_last_used_type);
        marshallInt(outf, ARRAYSZ(m_last_used_of_type));

        for (unsigned int i = 0; i < ARRAYSZ(m_last_used_of_type); i++)
            marshallItem(outf, m_last_used_of_type[i]);
    }

    void history::load(reader& inf)
    {
        // warning: this is called in the unmarshalling sequence before the
        // inventory is actually in place
        const short cooky = unmarshallShort(inf);
        ASSERT(cooky == QUIVER_COOKIE); (void)cooky;

        auto dummy = item_def();
        unmarshallItem(inf, dummy); // was: m_last_weapon
        m_last_used_type = (quiver::launcher)unmarshallInt(inf);
        ASSERT_RANGE(m_last_used_type, quiver::AMMO_THROW, quiver::NUM_LAUNCHERS);

        const unsigned int count = unmarshallInt(inf);
        ASSERT(count <= ARRAYSZ(m_last_used_of_type));

        for (unsigned int i = 0; i < count; i++)
            unmarshallItem(inf, m_last_used_of_type[i]);
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
            if (strstr(inv_item.inscription.c_str(), "=f"))
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

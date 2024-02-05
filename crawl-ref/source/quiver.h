/**
 * @file
 * @brief Player quiver functionality
**/

#pragma once

#include <vector>

#include "ability-type.h"
#include "command-type.h"
#include "format.h"
#include "spell-type.h"
#include "tiles.h" // for tile_def

class reader;
class writer;
class preserve_quiver_slots;

#define QUIVER_MAIN_SAVE_KEY "current_quiver_action"
#define QUIVER_LAUNCHER_SAVE_KEY "current_launcher_action"

namespace quiver
{
    void reset_state();

    struct action : public enable_shared_from_this<action>
    {
        action()
            : target(), default_fire_context(nullptr)
        { };
        virtual ~action() = default;
        void reset();
        void set_target(const dist &t);

        virtual bool operator==(const action &other) const
        {
            // TODO: typeid check?
            return typeid(other) == typeid(*this) && equals(other);
        }

        // this is an NVI idiom equality check
        virtual bool equals(const action &) const
        {
            // `action`s are all the same
            return true;
        }

        bool operator!=(const action &other) const
        {
            return !operator==(other);
        }

        virtual void invalidate() { }
        virtual bool is_enabled() const { return false; }
        virtual bool is_valid() const { return true; }
        virtual bool is_targeted() const { return false; }
        bool do_inscription_check() const;

        /// Should the action be triggered indirectly via autofight lua code,
        /// or does it need to be triggered directly?
        virtual bool use_autofight_targeting() const { return is_targeted(); }
        virtual bool allow_autofight() const { return false; }
        virtual bool uses_mp() const { return false; }
        virtual bool autofight_check() const;
        virtual item_def *get_launcher() const { return nullptr; }

        // main quiver color. In some cases used internally by
        // quiver_description, but also used in cases where for whatever reason
        // a full formatted_string can't be displayed
        virtual int quiver_color() const
        {
            return is_enabled() ? LIGHTGREY : DARKGREY;
        };

        virtual vector<tile_def> get_tiles() const;

        virtual formatted_string quiver_description(bool short_desc=false) const;
        virtual string quiver_verb() const { return ""; } // currently only for items

        // basically noops for this class, but keep `target` clean
        virtual void trigger()
        {
            reset();
            trigger(target);
        };

        virtual void trigger(dist &t) { target = t; }

        virtual void save(CrawlHashTable &save_target) const;

        virtual int get_item() const { return -1; };
        virtual shared_ptr<action> find_replacement() const { return nullptr; }
        virtual shared_ptr<action> find_next(int dir=1, bool allow_disabled=true, bool loop=false) const;

        virtual vector<shared_ptr<action>> get_fire_order(bool=true, bool=false) const // valid c++11 syntax!
        {
            return { };
        }

        virtual int source_hotkey() const;

        dist target;
        const action_cycler *default_fire_context;
    };

    bool is_autofight_combat_ability(ability_type ability);
    bool is_autofight_combat_spell(spell_type spell);

    bool toss_validate_item(int selected, string *err=nullptr);
    shared_ptr<action> find_ammo_action();
    shared_ptr<action> ammo_to_action(int slot, bool force=false);
    shared_ptr<action> slot_to_action(int slot, bool force=false);
    shared_ptr<action> spell_to_action(spell_type spell);
    shared_ptr<action> ability_to_action(ability_type abil);
    shared_ptr<action> get_primary_action();
    shared_ptr<action> get_secondary_action();
    void set_needs_redraw();

    bool anything_to_quiver();

    // this is roughly a custom not_null wrapper on shared_ptr<action>
    struct action_cycler
    {
        action_cycler();

        void save(const string key) const;
        void load(const string key);

        shared_ptr<action> get() const;
        virtual bool is_empty() const { return *current == action(); }
        bool spell_is_quivered(spell_type s) const;
        bool item_is_quivered(const item_def &item);
        bool item_is_quivered(int item_slot) const;

        shared_ptr<action> next(int dir = 0, bool allow_disabled=true) const;

        virtual bool set(const shared_ptr<action> n);
        bool set(const action_cycler &other);
        bool replace(const shared_ptr<action> new_act);
        bool set_from_slot(int slot);
        bool cycle(int dir = 0, bool allow_disabled=true);
        bool clear();
        void on_actions_changed();
        void on_item_pickup(int slot);
        virtual void set_needs_redraw();
        shared_ptr<action> find_last_valid();

        void target();
        shared_ptr<action> do_target();
        virtual string fire_key_hints() const;
        virtual bool targeter_handles_key(command_type c) const;
    protected:
        action_cycler(shared_ptr<action> init);

    private:
        shared_ptr<action> current;
        vector<shared_ptr<action>> history;
    };

    void choose(action_cycler &cur_quiver, bool allow_empty=true);
    bool set_to_quiver(shared_ptr<quiver::action> s, action_cycler &cur_quiver);
    void on_actions_changed();
    void on_weapon_changed();
    void on_item_pickup(int slot);
    void on_newchar();

    // TODO: perhaps this should be rolled into action_cycler?
    class ammo_history
    {
    public:
        ammo_history();

        // Queries from engine -- don't affect state
        int get_last_ammo() const;

        // Callbacks from engine
        // TODO: weird to have these on this object given the action refactor
        void set_quiver(const item_def &item);
        void on_item_fired(const item_def &item, bool explicitly_chosen = false);
        void maybe_swap(int from_slot, int to_slot);

#if TAG_MAJOR_VERSION == 34
        void load(reader&);
#endif
    };
}

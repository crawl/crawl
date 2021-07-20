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
    enum launcher
    {
        AMMO_THROW,           // no launcher wielded -> darts, stones, ...
        AMMO_BOW,             // wielded bow -> arrows
        AMMO_SLING,           // wielded sling -> stones, sling bullets
        AMMO_CROSSBOW,        // wielded crossbow -> bolts
        // Used to be hand crossbows
    #if TAG_MAJOR_VERSION == 34
        AMMO_BLOWGUN,         // wielded blowgun -> needles
    #endif
        NUM_LAUNCHERS
    };

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
        virtual bool do_inscription_check() const;

        /// Should the action be triggered indirectly via autofight lua code,
        /// or does it need to be triggered directly?
        virtual bool use_autofight_targeting() const { return is_targeted(); }
        virtual bool allow_autofight() const { return false; }
        virtual bool uses_mp() const { return false; }
        virtual bool autofight_check() const;
        virtual item_def *get_launcher() const { return nullptr; }
        virtual bool affected_by_pproj() const { return false; }

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

        dist target;
        const action_cycler *default_fire_context;
    };

    bool is_autofight_combat_ability(ability_type ability);
    bool is_autofight_combat_spell(spell_type spell);

    shared_ptr<action> find_ammo_action();
    shared_ptr<action> find_action_from_launcher(const item_def *item);

    shared_ptr<action> ammo_to_action(int slot, bool force=false);
    shared_ptr<action> slot_to_action(int slot, bool force=false);
    shared_ptr<action> spell_to_action(spell_type spell);
    shared_ptr<action> ability_to_action(ability_type abil);
    shared_ptr<action> get_primary_action();
    shared_ptr<action> get_secondary_action();
    void set_needs_redraw();

    int menu_size();

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

        virtual bool set(const shared_ptr<action> n, bool _autoswitched=false);
        bool set(const action_cycler &other);
        bool replace(const shared_ptr<action> new_act);
        bool set_from_slot(int slot);
        bool cycle(int dir = 0, bool allow_disabled=true);
        bool clear();
        void on_actions_changed(bool check_autoswitch=false);
        virtual void set_needs_redraw();
        shared_ptr<action> find_last_valid();

        void target();
        shared_ptr<action> do_target();
        virtual string fire_key_hints() const;
        virtual bool targeter_handles_key(command_type c) const;

        bool autoswitched;
    protected:
        action_cycler(shared_ptr<action> init);

    private:
        shared_ptr<action> current;
        vector<shared_ptr<action>> history;
    };

    struct launcher_action_cycler : public action_cycler
    {
        // some things about this class are not implemented to be launcher
        // specific because they aren't used, e.g. cycling
        launcher_action_cycler();

        using action_cycler::set; // unhide the other signature
        bool set(const shared_ptr<action> n, bool _autoswitched=false) override;
        bool is_empty() const override;
        void set_needs_redraw() override;
    };

    void choose(action_cycler &cur_quiver, bool allow_empty=true);
    void on_actions_changed(bool check_autoswitch=false);
    void on_weapon_changed();
    void on_newchar();

    // TODO: perhaps this should be rolled into action_cycler?
    class ammo_history
    {
    public:
        ammo_history();

        // Queries from engine -- don't affect state
        int get_last_ammo(const item_def *launcher) const;
        int get_last_ammo(quiver::launcher type) const;

        // Callbacks from engine
        // TODO: weird to have these on this object given the action refactor
        void set_quiver(const item_def &item, quiver::launcher ammo_type);
        void on_item_fired(const item_def &item, bool explicitly_chosen = false);

        // save/load
        void save(writer&) const;
        void load(reader&);

     private:
        item_def m_last_used_of_type[quiver::NUM_LAUNCHERS];
    };
}

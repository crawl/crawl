/**
 * @file
 * @brief Player quiver functionality
**/

#pragma once

#include <vector>

#include "format.h"

class reader;
class writer;
class preserve_quiver_slots;

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

    struct action
    {
        action() : target(), error() { };
        virtual ~action() = default;
        void reset();

        virtual bool operator==(const action &other) const
        {
            // TODO: typeid check?
            return get_item() == other.get_item();
        }

        bool operator!=(const action &other) const
        {
            return !operator==(other);
        }

        virtual bool is_enabled() const { return false; };
        virtual bool is_valid() const { return false; };
        virtual bool is_targeted() const { return false; };

        virtual formatted_string quiver_description() const;

        // basically noops for this class, but keep `target` clean
        virtual void trigger()
        {
            reset();
            trigger(target);
        };

        virtual void trigger(dist &t) { t = target; };

        virtual int get_item() const { return -1; };
        virtual shared_ptr<action> find_replacement() const { return nullptr; }

        dist target;
        string error;
    };

    shared_ptr<action> find_ammo_action();
    shared_ptr<action> find_action_from_launcher(const item_def *item);

    shared_ptr<action> slot_to_action(int slot);
    void choose();

    // this is roughly a custom not_null wrapper on shared_ptr<action>
    struct action_cycler
    {
        action_cycler();

        action &get() const;
        bool set(shared_ptr<action> n);
        bool set(const action_cycler &other);
        bool set_from_slot(int slot);
        shared_ptr<action> next(int dir = 0);
        bool cycle(int dir = 0);
        bool clear();

    private:
        shared_ptr<action> current;
    };

    // TODO: perhaps this should be rolled into action_cycler?
    class history
    {
    public:
        history();

        // Queries from engine -- don't affect state
        int get_last_ammo() const;
        int get_last_ammo(const item_def *launcher) const;
        int get_last_ammo(quiver::launcher type) const;

        // Callbacks from engine
        // TODO: weird to have these on this object given the action refactor
        void set_quiver(const item_def &item, quiver::launcher ammo_type);
        void empty_quiver(quiver::launcher ammo_type);
        void on_item_fired(const item_def &item, bool explicitly_chosen = false);
        void on_inv_quantity_changed(int slot);
        void on_weapon_changed();

        // save/load
        void save(writer&) const;
        void load(reader&);

     private:
        quiver::launcher m_last_used_type;
        item_def m_last_used_of_type[quiver::NUM_LAUNCHERS];
    };
}

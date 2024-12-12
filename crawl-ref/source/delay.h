/**
 * @file
 * @brief Functions for handling multi-turn actions.
**/

#pragma once

#include <vector>

#include "activity-interrupt-type.h"
#include "command-type.h"
#include "enum.h"
#include "equipment-slot.h"
#include "item-prop-enum.h"
#include "mpr.h"
#include "operation-types.h"
#include "seen-context-type.h"
#include "transformation.h"

using std::vector;

class interrupt_block
{
public:
    interrupt_block(bool block = true) {
        m_block = block;
        if (block)
            ++interrupts_blocked;
    }
    ~interrupt_block() {
        if (m_block)
            --interrupts_blocked;
    }

    static bool blocked() { return interrupts_blocked > 0; }
private:
    bool m_block;
    static int interrupts_blocked;
};

class monster;
struct ait_hp_loss;

enum class ai_payload // activity interrupt payloads
{
    none,
    int_payload,
    string_payload,
    monster,
    hp_loss,
};

struct activity_interrupt_data
{
    ai_payload apt;
    union
    {
        const char* string_data;
        const int*  int_data;
        const ait_hp_loss* ait_hp_loss_data;
        monster* mons_data;
        nullptr_t no_data;
    };

    seen_context_type context;

    activity_interrupt_data()
        : apt(ai_payload::none), no_data(nullptr), context(SC_NONE)
    {
    }
    activity_interrupt_data(const int *i)
        : apt(ai_payload::int_payload), int_data(i), context(SC_NONE)
    {
    }
    activity_interrupt_data(const char *s)
        : apt(ai_payload::string_payload), string_data(s), context(SC_NONE)
    {
    }
    activity_interrupt_data(const string &s)
        : apt(ai_payload::string_payload), string_data(s.c_str()),
          context(SC_NONE)
    {
    }
    activity_interrupt_data(monster* m, seen_context_type ctx = SC_NONE)
        : apt(ai_payload::monster), mons_data(m), context(ctx)
    {
    }
    activity_interrupt_data(const ait_hp_loss *ahl)
        : apt(ai_payload::hp_loss), ait_hp_loss_data(ahl), context(SC_NONE)
    {
    }
};

struct ait_hp_loss
{
    int hp;
    int hurt_type;  // KILLED_BY_POISON, etc.

    ait_hp_loss(int _hp, int _ht) : hp(_hp), hurt_type(_ht) { }
};

class Delay
{
    /**
     * This is run if `started` is not yet true.
     */
    virtual void start()
    { }

    /**
     * This is run before tick(); if it returns true, the delay is popped off
     * the queue and handle() returns early.
     */
    virtual bool invalidated()
    {
        return false;
    }

    /**
     * This is run once each turn of the delay, including the first, but not if
     * it has finished.
     */
    virtual void tick()
    { }

    /**
     * If the delay has finished, this is run instead. It should contain the payload of
     * the delay.
     */
    virtual void finish()
    {
        mpr("You finish doing something buggy.");
    }
protected:
    bool started = false;
public:
    int duration;

    Delay(int dur) : duration{dur}
    { }

    virtual ~Delay() = default;

    /**
     * @return whether this is a running delay (run, rest, travel), which
     * involves moving/resting each turn until completed, rather than depending
     * on `duration` itself.
     */
    virtual bool is_run() const
    {
        return false;
    }

    /**
     * @return whether this is a delay which relocates the player,
     * which are generally uninterruptible but are interrupted by teleport.
     * Note that no stairs are necessarily involved.
     */
    virtual bool is_relocation() const
    {
        return is_stairs();
    }

    /**
     * @return whether this involves going up or down stairs.
     */
    virtual bool is_stairs() const
    {
        return false;
    }

    virtual bool is_resting() const
    {
        return false;
    }

    /**
     * @return whether it's OK to start eating during this delay if hungry.
     */
    virtual bool want_autoeat() const
    {
        return false;
    }

    virtual bool is_macro() const
    {
        return false;
    }

    /**
     * @return true if this delay can act as a parent to other delays, i.e. if
     * other delays can be spawned while this delay is running. If is_parent
     * returns true, new delays will be pushed immediately to the front of the
     * delay in question, rather than at the end of the queue.
     */
    virtual bool is_parent() const
    {
        return false;
    }

    /**
     * This is called exactly each turn that the delay is active.
     */
    virtual void handle();

    /**
     * This is called on the front of the delay queue when stop_delay is called.
     * If the player needs to be notified, it should also print a message.
     * @return whether to pop the delay.
     */
    virtual bool try_interrupt(bool /*force*/)
    {
        // The default is for delays to be uninterruptible once started.
        return false;
    }

    /**
     *@return true if the delay involves using the item in this way (and
     * therefore the item should not be messed with).
     */
    virtual bool is_being_used(const item_def& /*item*/) const
    {
        return false;
    }

    /**
     * @return the delay's name; used in debugging and for the interrupt_ option family.
     */
    virtual const char* name() const = 0;
};

class EquipOnDelay : public Delay
{
    item_def& equip;
    equipment_slot slot;
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue %s %s.",
             get_verb(), equip.name(DESC_YOUR).c_str());
    }

    bool invalidated() override;

    void finish() override;
public:
    EquipOnDelay(int dur, item_def& item, equipment_slot _slot) :
                 Delay(dur), equip(item), slot(_slot)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "armour_on";
    }

    bool is_being_used(const item_def& item) const override
    {
        return &item == &equip;
    }
private:
    const char* get_verb();
};

class EquipOffDelay : public Delay
{
    item_def& equip;
    bool primary_weapon;
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue %s %s.",
             get_verb(), equip.name(DESC_YOUR).c_str());
    }

    bool invalidated() override;

    void finish() override;
public:
    EquipOffDelay(int dur, item_def& item, bool primary = false) :
                   Delay(dur), equip(item), primary_weapon(primary)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "armour_off";
    }

    bool is_being_used(const item_def& item) const override
    {
        return &item == &equip;
    }
private:
    const char* get_verb();
};

class MemoriseDelay : public Delay
{
    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue memorising.");
    }

    void finish() override;
public:
    spell_type spell;

    MemoriseDelay(int dur, spell_type sp) :
                  Delay(dur), spell{sp}
    { }

    bool try_interrupt(bool /*force*/) override;

    const char* name() const override
    {
        return "memorise";
    }
};

class PasswallDelay : public Delay
{
    coord_def dest;
    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue meditating on the rock.");
    }

    void finish() override;
public:
    PasswallDelay(int dur, coord_def pos) :
                  Delay(dur), dest{pos}
    { }

    bool try_interrupt(bool /*force*/) override;

    bool is_relocation() const override
    {
        return true;
    }

    const char* name() const override
    {
        return "passwall";
    }
};

class DropItemDelay : public Delay
{
    item_def& item;

    void tick() override;

    void finish() override;
public:
    DropItemDelay(int dur, item_def& it) :
                  Delay(dur), item(it)
    { }

    const char* name() const override
    {
        return "drop_item";
    }

    bool is_being_used(const item_def& item_) const override
    {
        return &item_ == &item;
    }
};

struct SelItem;
class MultidropDelay : public Delay
{
    vector<SelItem>& items;

    bool invalidated() override;
    void tick() override;
public:
    MultidropDelay(int dur, vector<SelItem>& vec) :
                  Delay(dur), items(vec)
    { }

    bool try_interrupt(bool /*force*/) override;

    const char* name() const override
    {
        return "multidrop";
    }

    bool is_parent() const override
    {
        // Can spawn delay_drop_item
        return true;
    }
};

class AscendingStairsDelay : public Delay
{
    void finish() override;
public:
    AscendingStairsDelay(int dur) : Delay(dur)
    { }

    bool try_interrupt(bool /*force*/) override;

    bool is_stairs() const override
    {
        return true;
    }

    const char* name() const override
    {
        return "ascending_stairs";
    }
};

class DescendingStairsDelay : public Delay
{
    void finish() override;
public:
    DescendingStairsDelay(int dur) : Delay(dur)
    { }

    bool try_interrupt(bool /*force*/) override;

    bool is_stairs() const override
    {
        return true;
    }

    const char* name() const override
    {
        return "descending_stairs";
    }
};

// The run delays (run, travel, rest) have some common code that does not apply
// to any other ones.
class BaseRunDelay : public Delay
{
    void handle() override;

    virtual bool want_move() const = 0;
    virtual bool want_clear_messages() const = 0;
    virtual command_type move_cmd() const = 0;

protected:
    bool unsafe_once = false;

public:
    bool is_run() const override
    {
        return true;
    }

    bool is_parent() const override
    {
        // Interlevel travel can do upstairs/downstairs delays.
        // Additionally, travel/rest can do autoeat.
        return true;
    }

    bool try_interrupt(bool /*force*/) override;

    BaseRunDelay() : Delay(1)
    { }
};

class RunDelay : public BaseRunDelay
{
    bool want_move() const override
    {
        return true;
    }

    bool want_clear_messages() const override
    {
        return true;
    }

    command_type move_cmd() const override;
public:
    RunDelay() : BaseRunDelay()
    { }

    const char* name() const override
    {
        return "run";
    }
};

class RestDelay : public BaseRunDelay
{
    bool want_move() const override
    {
        return false;
    }

    bool want_autoeat() const override
    {
        return true;
    }

    bool want_clear_messages() const override
    {
        return false;
    }

    command_type move_cmd() const override;
public:
    RestDelay() : BaseRunDelay()
    { }

    bool is_resting() const override
    {
        return true;
    }

    const char* name() const override
    {
        return "rest";
    }
};

class TravelDelay : public BaseRunDelay
{
    bool want_move() const override
    {
        return true;
    }

    bool want_autoeat() const override
    {
        return true;
    }

    bool want_clear_messages() const override
    {
        return true;
    }

    command_type move_cmd() const override;
public:
    TravelDelay(bool unsafe) : BaseRunDelay()
    {
        unsafe_once = unsafe;
    }

    const char* name() const override
    {
        return "travel";
    }
};

class MacroDelay : public Delay
{
    void handle() override;
public:
    MacroDelay() : Delay(1)
    { }

    bool try_interrupt(bool /*force*/) override;

    bool is_parent() const override
    {
        // Lua macros can in theory perform any of the other delays,
        // including travel; in practise travel still assumes there can be
        // no parent delay.
        return true;
    }

    bool is_macro() const override
    {
        return true;
    }

    const char* name() const override
    {
        //XXX: this is compared to in _userdef_interrupt_activity
        return "macro";
    }
};

class MacroProcessKeyDelay : public Delay
{
public:
    MacroProcessKeyDelay() : Delay(1)
    { }

    void handle() override
    {
        // If a Lua macro wanted Crawl to process a key normally, early exit.
        return;
    }

    const char* name() const override
    {
        return "macro_process_key";
    }
};

class ShaftSelfDelay : public Delay
{
    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue digging a shaft.");
    }

    void finish() override;
public:
    ShaftSelfDelay(int dur) : Delay(dur)
    { }

    bool try_interrupt(bool /*force*/) override;

    bool is_relocation() const override
    {
        return true;
    }

    const char* name() const override
    {
        return "shaft_self";
    }
};

class ExsanguinateDelay : public Delay
{
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue bloodletting.");
    }

    void finish() override;
public:
    ExsanguinateDelay(int dur) : Delay(dur)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "exsanguinate";
    }
};

class RevivifyDelay : public Delay
{
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue your ritual.");
    }

    void finish() override;
public:
    RevivifyDelay(int dur) : Delay(dur)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "revivify";
    }
};

class TransformDelay : public Delay
{
    transformation form;
    const item_def *talisman;

    bool was_prompted = false;

    void start() override;
    void tick() override;
    bool invalidated() override;
    void finish() override;
public:
    TransformDelay(transformation f, const item_def *t) :
                   Delay(3), form(f), talisman(t)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "transform";
    }
};

class ImbueDelay : public Delay
{
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue imbuing your servitor.");
    }

    void finish() override;
public:
    ImbueDelay(int dur, spell_type _spell) : Delay(dur), spell(_spell)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "imbue_servitor";
    }

private:
    spell_type spell;
};

class ImprintDelay : public Delay
{
    bool was_prompted = false;

    void start() override;

    void tick() override
    {
        mprf(MSGCH_MULTITURN_ACTION, "You continue imprinting.");
    }

    void finish() override;
public:
    ImprintDelay(int dur, const item_def& _weapon) : Delay(dur), wpn(_weapon)
    { }

    bool try_interrupt(bool force = false) override;

    const char* name() const override
    {
        return "imprint_weapon";
    }

private:
    item_def wpn;
};


void push_delay(shared_ptr<Delay> delay);

template<typename T, typename... Args>
shared_ptr<Delay> start_delay(Args&&... args)
{
    auto delay = make_shared<T>(std::forward<Args>(args)...);
    push_delay(delay);
    return delay;
}

void stop_delay(bool stop_stair_travel = false, bool force = false);
bool you_are_delayed();
shared_ptr<Delay> current_delay();
void handle_delay();

bool player_stair_delay();
bool already_learning_spell(int spell = -1);

void clear_macro_process_key_delay();

activity_interrupt get_activity_interrupt(const string &);

void run_macro(const char *macroname = nullptr);

void autotoggle_autopickup(bool off);
bool interrupt_activity(activity_interrupt ai,
                        const activity_interrupt_data &a
                            = activity_interrupt_data(),
                        vector<string>* msgs_buf = nullptr);

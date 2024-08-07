/**
 * @file
 * @brief Definitions for common structs.
**/

#pragma once

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#include "bitary.h"
#include "description-level-type.h"
#include "dungeon-feature-type.h"
#include "enum.h"
#include "spell-type.h"
#include "branch-type.h"
#include "fixedarray.h"
#include "kill-category.h"
#include "killer-type.h"
#include "map-marker-type.h"
#include "menu-type.h"

#include "object-class-type.h"
#include "pattern.h"
#include "skill-type.h"
#include "shop-type.h"
#include "cloud-type.h"
#include "store.h"
#include "rltiles/tiledef_defines.h"
#include "tag-version.h"

#include "coord-def.h"
#include "item-def.h"
#include "level-id.h"
#include "monster-type.h"

#include "ray.h"

struct tile_flavour
{
    unsigned short floor_idx;
    unsigned short wall_idx;
    unsigned short feat_idx;

    unsigned short floor;
    unsigned short wall;
    // Used (primarily) by the vault 'TILE' overlay.
    unsigned short feat;

    // Used as a random value or for special cases e.g. (bazaars, gates).
    unsigned short special;

    tile_flavour(): floor_idx(0), wall_idx(0), feat_idx(0),
                    floor(0), wall(0), feat(0), special(0) {}
};

// A glorified unsigned int that assists with ref-counting the mcache.
class tile_fg_store
{
public:
    tile_fg_store() : m_tile(0) {}
    tile_fg_store(tileidx_t tile) : m_tile(tile) {}
    operator tileidx_t() { return m_tile; }
    tileidx_t operator=(tileidx_t tile);
protected:
    tileidx_t m_tile;
};

#define MAX_NAME_LENGTH 30

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned int, GXM, GYM> map_mask;
typedef FixedBitArray<GXM, GYM> map_bitmask;

struct item_def;
struct coord_def;
class level_id;
class map_marker;
class actor;
class player;
class ghost_demon;

constexpr coord_def INVALID_COORD {-1, -1};
constexpr coord_def NO_CURSOR { INVALID_COORD };

typedef bool (*coord_predicate)(const coord_def &c);

struct run_check_dir
{
    dungeon_feature_type grid;
    coord_def delta;
};

/**
 * Persistent unique identifier for an actor.
 *
 * An mid_t is a persistent (across levels and across save/restore)
 * and unique (within a given game) identifier for a monster, player,
 * or fake actor. The value 0 indicates "no actor", and any value
 * greater than or equal to MID_FIRST_NON_MONSTER indicates an actor
 * other than a monster.
 *
 * mid_t should be used for anything that needs to remember monster
 * identity from one turn to the next, as mindexes may be reused
 * if a monster dies, and are not unique across levels.
 */
typedef uint32_t mid_t;
#define PRImidt PRIu32
#define MID_NOBODY        ((mid_t)0x00000000)
#define MID_PLAYER        ((mid_t)0xffffffff)
// the numbers are meaningless, there's just plenty of space for gods, env,
// and whatever else we want to have, while keeping all monster ids smaller.
#define MID_ANON_FRIEND   ((mid_t)0xffff0000)
#define MID_YOU_FAULTLESS ((mid_t)0xffff0001)
#define MID_PLAYER_SHADOW_DUMMY ((mid_t)0xffff0002)
/// Upper bound on the number of monsters that can ever exist in a game.
#define MID_FIRST_NON_MONSTER MID_ANON_FRIEND

/**
 * Define overloaded ++ and -- operators for the enum T.
 *
 * This macro produces several inline function definitions; use it only at
 * file/namespace scope. It requires a trailing semicolon.
 *
 * @param T A type expression naming the enum type to augment. Evaluated
 *          several times.
 */
#define DEF_ENUM_INC(T) \
    static inline T &operator++(T &x) { return x = static_cast<T>(x + 1); } \
    static inline T &operator--(T &x) { return x = static_cast<T>(x - 1); } \
    static inline T operator++(T &x, int) { T y = x; ++x; return y; } \
    static inline T operator--(T &x, int) { T y = x; --x; return y; } \
    COMPILE_CHECK(is_enum<T>::value)

DEF_ENUM_INC(monster_type);
DEF_ENUM_INC(spell_type);
DEF_ENUM_INC(skill_type);

/// Exception indicating a bad level_id, level_range, or depth_range.
struct bad_level_id : public runtime_error
{
    explicit bad_level_id(const string &msg) : runtime_error(msg) {}
    explicit bad_level_id(const char *msg) : runtime_error(msg) {}
};

/**
 * Create a bad_level_id exception from a printf-like specification.
 * Users of this macro must #include "stringutil.h" themselves.
 */
#define bad_level_id_f(...) bad_level_id(make_stringf(__VA_ARGS__))

class runrest
{
public:
    int runmode;
    int mp;
    int hp;
    bool notified_mp_full;
    bool notified_hp_full;
    bool notified_ancestor_hp_full;
    coord_def pos;
    int direction;
    int turns_passed;

    FixedVector<run_check_dir,3> run_check; // array of grids to check

public:
    runrest();
    void initialise(int rdir, int mode);

    // returns runmode
    operator int () const;

    // sets runmode
    const runrest &operator = (int newrunmode);

    // Returns true if we're currently resting.
    bool is_rest() const;
    bool is_explore() const;
    bool is_any_travel() const;

    string runmode_name() const;

    // Clears run state.
    void clear();

    // Stops running.
    void stop(bool clear_delays = true);

    // Take one off the rest counter.
    void rest();

    // Checks if shift-run should be aborted and aborts the run if necessary.
    // Returns true if you were running and are now no longer running.
    bool check_stop_running();

private:
    void set_run_check(int index, int compass_dir);
    bool run_should_stop() const;
    bool diag_run_passes_door() const;
};

enum mon_spell_slot_flag
{
    MON_SPELL_NO_FLAGS  = 0,
    MON_SPELL_EMERGENCY   = 1 <<  0, // only use this spell slot in emergencies
    MON_SPELL_NATURAL     = 1 <<  1, // physiological, not really a spell
    MON_SPELL_MAGICAL     = 1 <<  2, // magical ability, affected by AM
    MON_SPELL_VOCAL       = 1 <<  3, // natural ability, but affected by silence
    MON_SPELL_WIZARD      = 1 <<  4, // real spell, affected by AM and silence
    MON_SPELL_PRIEST      = 1 <<  5, // divine ability, affected by silence

    MON_SPELL_FIRST_CATEGORY = MON_SPELL_NATURAL,
    MON_SPELL_LAST_CATEGORY  = MON_SPELL_PRIEST,

    MON_SPELL_BREATH      = 1 <<  6, // sets a breath timer, requires it to be 0
#if TAG_MAJOR_VERSION == 34
    MON_SPELL_NO_SILENT   = 1 <<  7, // can't be used while silenced/mute/etc.
#endif

    MON_SPELL_INSTANT     = 1 <<  8, // allows another action on the same turn
    MON_SPELL_NOISY       = 1 <<  9, // makes noise despite being innate

    MON_SPELL_SHORT_RANGE = 1 << 10, // only use at short distances
    MON_SPELL_LONG_RANGE  = 1 << 11, // only use at long distances
    MON_SPELL_EVOKE       = 1 << 12, // a spell from an evoked item

    MON_SPELL_LAST_FLAG = MON_SPELL_EVOKE,
};
DEF_BITFIELD(mon_spell_slot_flags, mon_spell_slot_flag, 12);
const int MON_SPELL_LAST_EXPONENT = mon_spell_slot_flags::last_exponent;
COMPILE_CHECK(mon_spell_slot_flags::exponent(MON_SPELL_LAST_EXPONENT)
              == MON_SPELL_LAST_FLAG);

constexpr mon_spell_slot_flags MON_SPELL_TYPE_MASK
    = MON_SPELL_NATURAL | MON_SPELL_MAGICAL | MON_SPELL_WIZARD
      | MON_SPELL_PRIEST | MON_SPELL_VOCAL;

// Doesn't make noise when cast (unless flagged with MON_SPELL_NOISY).
constexpr mon_spell_slot_flags MON_SPELL_INNATE_MASK
    = MON_SPELL_NATURAL | MON_SPELL_MAGICAL;

// Affected by antimagic.
constexpr mon_spell_slot_flags MON_SPELL_ANTIMAGIC_MASK
    = MON_SPELL_MAGICAL | MON_SPELL_WIZARD;

// Affected by silence.
constexpr mon_spell_slot_flags MON_SPELL_SILENCE_MASK
    = MON_SPELL_WIZARD  | MON_SPELL_PRIEST  | MON_SPELL_VOCAL;

struct mon_spell_slot
{
    // Allow implicit conversion (and thus copy-initialization) from a
    // three-element initializer list, but not from a smaller list or
    // from a plain spell_type.
    constexpr mon_spell_slot(spell_type spell_, uint8_t freq_,
                             mon_spell_slot_flags flags_)
        : spell(spell_), freq(freq_), flags(flags_)
    { }
    explicit constexpr mon_spell_slot(spell_type spell_ = SPELL_NO_SPELL,
                                      uint8_t freq_ = 0)
        : mon_spell_slot(spell_, freq_, MON_SPELL_NO_FLAGS)
    { }

    spell_type spell;
    uint8_t freq;
    mon_spell_slot_flags flags;
};

typedef vector<mon_spell_slot> monster_spells;

class InvEntry;
typedef int (*item_sort_fn)(const InvEntry *a, const InvEntry *b);
struct item_comparator
{
    item_sort_fn cmpfn;
    bool negated;

    item_comparator(item_sort_fn cfn, bool neg = false)
        : cmpfn(cfn), negated(neg)
    {
    }
    int compare(const InvEntry *a, const InvEntry *b) const
    {
        return negated? -cmpfn(a, b) : cmpfn(a, b);
    }
};
typedef vector<item_comparator> item_sort_comparators;

struct menu_sort_condition
{
public:
    menu_type mtype;
    int       sort;
    item_sort_comparators cmp;

public:
    menu_sort_condition(menu_type mt = menu_type::invlist, int sort = 0);
    menu_sort_condition(const string &s);

    bool matches(menu_type mt) const;

private:
    void set_menu_type(string &s);
    void set_sort(string &s);
    void set_comparators(string &s);
};

struct cglyph_t
{
    char32_t ch;
    unsigned short col; // XXX: real or unreal depending on context...

    cglyph_t(char32_t _ch = 0, unsigned short _col = LIGHTGREY)
        : ch(_ch), col(_col)
    {
    }
};

typedef FixedArray<bool, NUM_OBJECT_CLASSES, MAX_SUBTYPES> id_arr;

namespace quiver
{
    struct action_cycler;
}

// input and output to targeting actions
// TODO: rename, move to its own .h, remove camel case
// For the exact details of interactive vs non-interactive targeting, see
// dist::needs_targeting() in directn.cc.
class dist
{
public:
    dist();

    bool isMe() const;
    bool needs_targeting() const;

    bool isValid;       // output: valid target chosen?
    bool isTarget;      // output: target (true), or direction (false)?
    bool isEndpoint;    // input: Does the player want the attack to stop at target?
    bool isCancel;      // output: user cancelled (usually <ESC> key)
    bool choseRay;      // output: user wants a specific beam
    bool interactive;   // input and output: if true, forces an interactive targeter.
                        // behavior when false depends on `target` and `find_target`.
                        // on output, whether an interactive targeter was chosen.

    coord_def target;   // input and output: target x,y or logical extension of beam to map edge
    coord_def delta;    // input and output: delta x and y if direction - always -1,0,1
    ray_def ray;        // output: ray chosen if necessary
    bool find_target;   // input: use the targeter to find a target, possibly by modifying `target`.
                        // requests non-interactive mode, but does not override `interactive`.
    const quiver::action_cycler *fire_context;
                            // input: if triggered from the action system, what the
                            // quiver was that triggered it. May be nullptr.
                            // sort of ugly to have this here...
    int cmd_result;     // output: a resulting command. See quiver::action_cycler::target for
                        // which commands may be handled and how. This is an `int` for include
                        // order reasons, unfortunately
};

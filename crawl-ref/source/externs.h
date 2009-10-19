/*
 *  File:       externs.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Linley Henzell
 */

#ifndef EXTERNS_H
#define EXTERNS_H

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <cstdlib>
#include <deque>

#include <time.h>

#include "defines.h"
#include "enum.h"
#include "fixary.h"
#include "libutil.h"
#include "mpr.h"
#include "store.h"

#ifdef USE_TILE
struct tile_flavour
{
    // The floor tile to use.
    unsigned short floor;
    // The wall tile to use.
    unsigned short wall;
    // Used as a random value or for special cases e.g. (bazaars, gates).
    unsigned short special;
};

// A glorified unsigned int that assists with ref-counting the mcache.
class tile_fg_store
{
public:
    tile_fg_store() : m_tile(0) {}
    tile_fg_store(unsigned int tile) : m_tile(tile) {}
    operator unsigned int() { return m_tile; }
    unsigned int operator=(unsigned int tile);
protected:
    unsigned int m_tile;
};


#endif

#define INFO_SIZE       200          // size of message buffers
#define ITEMNAME_SIZE   200          // size of item names/shop names/etc
#define HIGHSCORE_SIZE  800          // <= 10 Lines for long format scores

#define MAX_NUM_GODS    21

#define BURDEN_TO_AUM 0.1f           // scale factor for converting burden to aum

extern char info[INFO_SIZE];         // defined in acr.cc {dlb}

const int kNameLen = 30;
#ifdef SHORT_FILE_NAMES
    const int kFileNameLen = 6;
#else
    const int kFileNameLen = 250;
#endif

// Used only to bound the init file name length
const int kPathLen = 256;

// This value is used to mark that the current berserk is free from
// penalty (Xom's granted or from a deck of cards).
#define NO_BERSERK_PENALTY    -1

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
    env_show_grid;

struct item_def;
class melee_attack;
struct coord_def;
class level_id;
class player_quiver;

struct coord_def
{
    int         x;
    int         y;

    explicit coord_def( int x_in = 0, int y_in = 0 ) : x(x_in), y(y_in) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    int distance_from(const coord_def &b) const;

    bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    bool operator <  (const coord_def &other) const
    {
        return (x < other.x) || (x == other.x && y < other.y);
    }

    bool operator >  (const coord_def &other) const
    {
        return (x > other.x) || (x == other.x && y > other.y);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return (*this);
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return (*this);
    }

    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return (*this);
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return (*this);
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return (*this);
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return (*this);
    }

    coord_def operator + (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator + (int other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator - (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator - (int other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator / (int div) const
    {
        coord_def copy = *this;
        return (copy /= div);
    }

    coord_def operator * (int mul) const
    {
        coord_def copy = *this;
        return (copy *= mul);
    }

    int abs() const
    {
        return (x * x + y * y);
    }

    int rdist() const
    {
        return (std::max(std::abs(x), std::abs(y)));
    }

    bool origin() const
    {
        return (!x && !y);
    }

    bool zero() const
    {
        return origin();
    }

    bool equals(const int xi, const int yi) const
    {
        return (xi == x && yi == y);
    }
};

typedef bool (*coord_predicate)(const coord_def &c);

struct dice_def
{
    int         num;
    int         size;

    dice_def( int n = 0, int s = 0 ) : num(n), size(s) {}
    int roll() const;
};

struct run_check_dir
{
    dungeon_feature_type grid;
    coord_def delta;
};

struct cloud_struct
{
    coord_def     pos;
    cloud_type    type;
    int           decay;
    unsigned char spread_rate;
    kill_category whose;
    killer_type   killer;

    cloud_struct() : pos(), type(CLOUD_NONE), decay(0), spread_rate(0),
                     whose(KC_OTHER), killer(KILL_NONE)
    {
    }

    void set_whose(kill_category _whose);
    void set_killer(killer_type _killer);

    static kill_category killer_to_whose(killer_type killer);
    static killer_type   whose_to_killer(kill_category whose);

};

struct shop_struct
{
    coord_def           pos;
    unsigned char       greed;
    shop_type           type;
    unsigned char       level;

    FixedVector<unsigned char, 3> keeper_name;
};


struct delay_queue_item
{
    delay_type  type;
    int         duration;
    int         parm1;
    int         parm2;
    bool        started;
};


// Identifies a level. Should never include virtual methods or
// dynamically allocated memory (see code to push level_id onto Lua
// stack in l_dgn.cc)
class level_id
{
public:
    branch_type branch;     // The branch in which the level is.
    int depth;              // What depth (in this branch - starting from 1)
    level_area_type level_type;

public:
    // Returns the level_id of the current level.
    static level_id current();

    // Returns the level_id of the level that the stair/portal/whatever at
    // 'pos' on the current level leads to.
    static level_id get_next_level_id(const coord_def &pos);

    level_id()
        : branch(BRANCH_MAIN_DUNGEON), depth(-1),
          level_type(LEVEL_DUNGEON)
    {
    }
    level_id(branch_type br, int dep, level_area_type ltype = LEVEL_DUNGEON)
        : branch(br), depth(dep), level_type(ltype)
    {
    }
    level_id(const level_id &ot)
        : branch(ot.branch), depth(ot.depth), level_type(ot.level_type)
    {
    }
    level_id(level_area_type ltype)
        : branch(BRANCH_MAIN_DUNGEON), depth(-1), level_type(ltype)
    {
    }

    static level_id parse_level_id(const std::string &s) throw (std::string);
    static level_id from_packed_place(const unsigned short place);

    unsigned short packed_place() const;
    std::string describe(bool long_name = false, bool with_number = true) const;

    void clear()
    {
        branch = BRANCH_MAIN_DUNGEON;
        depth  = -1;
        level_type = LEVEL_DUNGEON;
    }

    // Returns the absolute depth in the dungeon for the level_id;
    // non-dungeon branches (specifically Abyss and Pan) will return
    // depths suitable for use in monster and item generation. If
    // you're looking for a depth to set you.your_level to, use
    // dungeon_absdepth().
    int absdepth() const;

    // Returns the absolute depth in the dungeon for the level_id, corresponding
    // to you.your_level.
    int dungeon_absdepth() const;

    bool is_valid() const
    {
        return (branch != NUM_BRANCHES && depth != -1)
            || level_type != LEVEL_DUNGEON;
    }

    const level_id &operator = (const level_id &id)
    {
        branch     = id.branch;
        depth      = id.depth;
        level_type = id.level_type;
        return (*this);
    }

    bool operator == ( const level_id &id ) const
    {
        return (level_type == id.level_type
                && (level_type != LEVEL_DUNGEON
                    || (branch == id.branch && depth == id.depth)));
    }

    bool operator != ( const level_id &id ) const
    {
        return !operator == (id);
    }

    bool operator <( const level_id &id ) const
    {
        if (level_type != id.level_type)
            return (level_type < id.level_type);

        if (level_type != LEVEL_DUNGEON)
            return (false);

        return (branch < id.branch) || (branch==id.branch && depth < id.depth);
    }

    bool operator == ( const branch_type _branch ) const
    {
        return (branch == _branch && level_type == LEVEL_DUNGEON);
    }

    bool operator != ( const branch_type _branch  ) const
    {
        return !operator == (_branch);
    }

    void save(writer&) const;
    void load(reader&);
};


struct item_def
{
    object_class_type  base_type;  // basic class (ie OBJ_WEAPON)
    unsigned char  sub_type;       // type within that class (ie WPN_DAGGER)
    short          plus;           // +to hit, charges, corpse mon id
    short          plus2;          // +to dam, sub-sub type for boots/helms
    long           special;        // special stuff
    unsigned char  colour;         // item colour
    unsigned long  flags;          // item status flags
    short          quantity;       // number of items

    coord_def pos;     // for inventory items == (-1, -1)
    short  link;       // link to next item;  for inventory items = slot
    short  slot;       // Inventory letter

    unsigned short orig_place;
    short          orig_monnum;

    std::string inscription;

    CrawlHashTable props;

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0L), colour(0), flags(0L), quantity(0),
                 pos(), link(NON_ITEM), slot(0), orig_place(0),
                 orig_monnum(0), inscription()
    {
    }

    std::string name(description_level_type descrip,
                     bool terse = false, bool ident = false,
                     bool with_inscription = true,
                     bool quantity_in_words = false,
                     unsigned long ignore_flags = 0x0) const;
    bool has_spells() const;
    bool cursed() const;
    int book_number() const;
    zap_type zap() const; // what kind of beam it shoots (if wand).

    // Returns index in mitm array. Results are undefined if this item is
    // not in the array!
    int  index() const;

    int  armour_rating() const;

    bool launched_by(const item_def &launcher) const;

    void clear()
    {
        *this = item_def();
    }

    // Sets this item as being held by a given monster.
    void set_holding_monster(int midx);

    // Returns monster holding this item.  NULL if none.
    monsters* holding_monster() const;

    // Returns true if a monster is holding this item.
    bool held_by_monster() const;

private:
    std::string name_aux(description_level_type desc,
                         bool terse, bool ident,
                         unsigned long ignore_flags) const;
};

class runrest
{
public:
    int runmode;
    int mp;
    int hp;
    coord_def pos;

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

    // Clears run state.
    void clear();

    // Stops running.
    void stop();

    // Take one off the rest counter.
    void rest();

    // Checks if shift-run should be aborted and aborts the run if necessary.
    // Returns true if you were running and are now no longer running.
    bool check_stop_running();

private:
    void set_run_check(int index, int compass_dir);
    bool run_grids_changed() const;
};

class PlaceInfo
{
public:
    int level_type;     // enum level_area_type
    int branch;         // enum branch_type if LEVEL_DUNGEON; otherwise -1

    unsigned long num_visits;
    unsigned long levels_seen;

    unsigned long mon_kill_exp;
    unsigned long mon_kill_exp_avail;
    unsigned long mon_kill_num[KC_NCATEGORIES];

    long turns_total;
    long turns_explore;
    long turns_travel;
    long turns_interlevel;
    long turns_resting;
    long turns_other;

    double elapsed_total;
    double elapsed_explore;
    double elapsed_travel;
    double elapsed_interlevel;
    double elapsed_resting;
    double elapsed_other;

public:
    PlaceInfo();

    bool is_global() const;
    void make_global();

    void assert_validity() const;

    const std::string short_name() const;

    const PlaceInfo &operator += (const PlaceInfo &other);
    const PlaceInfo &operator -= (const PlaceInfo &other);
    PlaceInfo operator + (const PlaceInfo &other) const;
    PlaceInfo operator - (const PlaceInfo &other) const;
};


typedef std::vector<delay_queue_item> delay_queue_type;

class KillMaster;




class monster_spells : public FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>
{
public:
    monster_spells()
        : FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>(SPELL_NO_SPELL)
    { }
    void clear() { init(SPELL_NO_SPELL); }
};

class ghost_demon;
class actor;
class monsters;

struct trap_def
{
    coord_def pos;
    trap_type type;
    int       ammo_qty;

    dungeon_feature_type category() const;
    std::string name(description_level_type desc = DESC_PLAIN) const;
    bool is_known(const actor* act = 0) const;
    void trigger(actor& triggerer, bool flat_footed = false);
    void disarm();
    void destroy();
    void hide();
    void reveal();
    void prepare_ammo();
    bool type_has_ammo() const;
    bool active() const;

private:
    void message_trap_entry();
    void shoot_ammo(actor& act, bool was_known);
    item_def generate_trap_item();
    int shot_damage(actor& act);
};

struct map_cell
{
    short object;           // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.
    unsigned short colour;
    unsigned long property; // Flags for blood, sanctuary, ...

    map_cell() : object(0), flags(0), colour(0), property(0) { }
    void clear() { flags = object = colour = 0; }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

class map_marker;
class reader;
class writer;
class map_markers
{
public:
    map_markers();
    map_markers(const map_markers &);
    map_markers &operator = (const map_markers &);
    ~map_markers();

    void activate_all(bool verbose = true);
    void add(map_marker *marker);
    void remove(map_marker *marker);
    void remove_markers_at(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(map_marker_type type);
    void move(const coord_def &from, const coord_def &to);
    void move_marker(map_marker *marker, const coord_def &to);
    std::vector<map_marker*> get_all(map_marker_type type = MAT_ANY);
    std::vector<map_marker*> get_all(const std::string &key,
                                     const std::string &val = "");
    std::vector<map_marker*> get_markers_at(const coord_def &c);
    std::string property_at(const coord_def &c, map_marker_type type,
                            const std::string &key);
    void clear();

    void write(writer &) const;
    void read(reader &, int minorVersion);

private:
    typedef std::multimap<coord_def, map_marker *> dgn_marker_map;
    typedef std::pair<coord_def, map_marker *> dgn_pos_marker;

    void init_from(const map_markers &);
    void unlink_marker(const map_marker *);

private:
    dgn_marker_map markers;
};

struct message_filter
{
    int             channel;        // Use -1 to match any channel.
    text_pattern    pattern;        // Empty pattern matches any message

    message_filter( int ch, const std::string &s )
        : channel(ch), pattern(s)
    {
    }

    message_filter( const std::string &s ) : channel(-1), pattern(s) { }

    bool is_filtered( int ch, const std::string &s ) const {
        bool channel_match = ch == channel || channel == -1;
        if (!channel_match || pattern.empty())
            return channel_match;
        return pattern.matches(s);
    }

};

struct sound_mapping
{
    text_pattern pattern;
    std::string  soundfile;
};

struct colour_mapping
{
    std::string tag;
    text_pattern pattern;
    int colour;
};

struct message_colour_mapping
{
    message_filter message;
    int colour;
};

struct feature_def
{
    dungeon_char_type   dchar;
    unsigned            symbol;          // symbol used for seen terrain
    unsigned            magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
    unsigned short      em_colour;       // Emphasised colour when in LoS.
    unsigned short      seen_em_colour;  // Emphasised colour when out of LoS
    unsigned            flags;
    map_feature         minimap;         // mini-map categorization

    bool is_notable() const { return (flags & FFT_NOTABLE); }
};

struct feature_override
{
    dungeon_feature_type    feat;
    feature_def             override;
};

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
        return (negated? -cmpfn(a, b) : cmpfn(a, b));
    }
};
typedef std::vector<item_comparator> item_sort_comparators;

struct menu_sort_condition
{
public:
    menu_type mtype;
    int       sort;
    item_sort_comparators cmp;

public:
    menu_sort_condition(menu_type mt = MT_INVLIST, int sort = 0);
    menu_sort_condition(const std::string &s);

    bool matches(menu_type mt) const;

private:
    void set_menu_type(std::string &s);
    void set_sort(std::string &s);
    void set_comparators(std::string &s);
};

struct mon_display
{
    monster_type type;
    unsigned     glyph;
    unsigned     colour;

    mon_display(monster_type m = MONS_PROGRAM_BUG,
                unsigned gly = 0,
                unsigned col = 0) : type(m), glyph(gly), colour(col) { }
};

class InitLineInput;
struct game_options
{
public:
    game_options();
    void reset_startup_options();
    void reset_options();

    void read_option_line(const std::string &s, bool runscripts = false);
    void read_options(InitLineInput &, bool runscripts,
                      bool clear_aliases = true);

    void include(const std::string &file, bool resolve, bool runscript);
    void report_error(const std::string &error);

    std::string resolve_include(const std::string &file,
                                const char *type = "");

    bool was_included(const std::string &file) const;

    static std::string resolve_include(
        std::string including_file,
        std::string included_file,
        const std::vector<std::string> *rcdirs = NULL)
               throw (std::string);

public:
    std::string filename;     // The name of the file containing options.
    std::string basefilename; // Base (pathless) file name
    int         line_num;     // Current line number being processed.

    // View options
    std::vector<feature_override> feature_overrides;
    std::vector<mon_display>      mon_glyph_overrides;
    unsigned cset_override[NUM_CSET][NUM_DCHAR_TYPES];

    std::string save_dir;       // Directory where saves and bones go.
    std::string macro_dir;      // Directory containing macro.txt
    std::string morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    std::vector<std::string> additional_macro_files;

    std::string player_name;

#ifdef DGL_SIMPLE_MESSAGING
    bool        messaging;      // Check for messages.
#endif

    bool        suppress_startup_errors;

    bool        mouse_input;

    int         view_max_width;
    int         view_max_height;
    int         mlist_min_height;
    int         msg_max_height;
    bool        mlist_allow_alternate_layout;
    int         mlist_targetting; // not actually a real option
    bool        classic_hud;
    bool        msg_condense_repeats;

    // The view lock variables force centering the viewport around the PC @
    // at all times (the default). If view locking is not enabled, the viewport
    // scrolls only when the PC hits the edge of it.
    bool        view_lock_x;
    bool        view_lock_y;

    // For an unlocked viewport, this will center the viewport when scrolling.
    bool        center_on_scroll;

    // If symmetric_scroll is set, for diagonal moves, if the view
    // scrolls at all, it'll scroll diagonally.
    bool        symmetric_scroll;

    // How far from the viewport edge is scrolling forced.
    int         scroll_margin_x;
    int         scroll_margin_y;

    bool        verbose_monster_pane;

    int         autopickup_on;
    int         default_friendly_pickup;
    bool        show_more_prompt;

    bool        show_gold_turns; // Show gold and turns in HUD.
    bool        show_beam;       // Show targetting beam by default.

    long        autopickups;     // items to autopickup
    bool        show_inventory_weights; // show weights in inventory listings
    bool        colour_map;      // add colour to the map
    bool        clean_map;       // remove unseen clouds/monsters
    bool        show_uncursed;   // label known uncursed items as "uncursed"
    bool        easy_open;       // open doors with movement
    bool        easy_unequip;    // allow auto-removing of armour / jewellery
    bool        equip_unequip;   // Make 'W' = 'T', and 'P' = 'R'.
    bool        easy_butcher;    // autoswap to butchering tool
    bool        always_confirm_butcher; // even if only one corpse
    bool        chunks_autopickup; // Autopickup chunks after butchering
    bool        prompt_for_swap; // Prompt to switch back from butchering
                                 // tool if hostile monsters are around.
    bool        list_rotten;     // list slots for rotting corpses/chunks
    bool        prefer_safe_chunks; // prefer clean chunks to contaminated ones
    bool        easy_eat_chunks; // make 'e' auto-eat the oldest safe chunk
    bool        easy_eat_gourmand; // with easy_eat_chunks, and wearing a
                                   // "OfGourmand, auto-eat contaminated
                                   // chunks if no safe ones are present
    bool        easy_eat_contaminated; // like easy_eat_gourmand, but
                                       // always active.
    bool        default_target;  // start targetting on a real target
    bool        autopickup_no_burden;   // don't autopickup if it changes burden

    bool        note_all_skill_levels;  // take note for all skill levels (1-27)
    bool        note_skill_max;   // take note when skills reach new max
    bool        note_all_spells;  // take note when learning any spell
    std::string user_note_prefix; // Prefix for user notes
    int         note_hp_percent;  // percentage hp for notetaking
    bool        note_xom_effects; // take note of all Xom effects
    int         ood_interesting;  // how many levels OOD is noteworthy?
    int         rare_interesting; // what monster rarity is noteworthy?
    confirm_level_type easy_confirm;    // make yesno() confirming easier
    bool        easy_quit_item_prompts; // make item prompts quitable on space
    confirm_prompt_type allow_self_target;      // yes, no, prompt

    int         colour[16];      // macro fg colours to other colours
    int         background;      // select default background colour
    int         channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    int         target_range; // for whether targetting is out of range

    bool        use_old_selection_order; // use old order of species/classes in
                                         // selection screen
    int         weapon;          // auto-choose weapon for character
    int         book;            // auto-choose book for character
    int         wand;            // auto-choose wand for character
    int         chaos_knight;    // choice of god for Chaos Knights (Xom/Makleb)
    int         death_knight;    // choice of god/necromancy for Death Knights
    god_type    priest;          // choice of god for priests (Zin/Yred)
    bool        random_pick;     // randomly generate character
    bool        good_random;     // when chosing randomly only choose
                                 // unrestricted combinations
    int         hp_warning;      // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    char        race;            // preselected race
    char        cls;             // preselected class
    bool        delay_message_clear;    // avoid clearing messages each turn
    unsigned    friend_brand;     // Attribute for branding friendly monsters
    unsigned    neutral_brand;    // Attribute for branding neutral monsters
    bool        no_dark_brand;    // Attribute for branding friendly monsters
    bool        macro_meta_entry; // Allow user to use numeric sequences when
                                  // creating macros

    int         fire_items_start; // index of first item for fire command
    std::vector<unsigned> fire_order;   // missile search order for 'f' command

    bool        auto_list;       // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    char_set_type  char_set;
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table;

    int         num_colours;     // used for setting up curses colour table (8 or 16)

    std::string pizza;

#ifdef WIZARD
    int         wiz_mode;        // yes, no, never in wiz mode to start
#endif

    // internal use only:
    int         sc_entries;      // # of score entries
    int         sc_format;       // Format for score entries

    std::vector<std::pair<int, int> > hp_colour;
    std::vector<std::pair<int, int> > mp_colour;
    std::vector<std::pair<int, int> > stat_colour;

    std::string map_file_name;   // name of mapping file to use
    std::vector<std::pair<text_pattern, bool> > force_autopickup;
    std::vector<text_pattern> note_monsters;  // Interesting monsters
    std::vector<text_pattern> note_messages;  // Interesting messages
    std::vector<std::pair<text_pattern, std::string> > autoinscriptions;
    std::vector<text_pattern> note_items;     // Objects to note
    std::vector<int> note_skill_levels;       // Skill levels to note

    bool        autoinscribe_artefacts; // Auto-inscribe identified artefacts.

    bool        pickup_thrown;  // Pickup thrown missiles
    bool        pickup_dropped; // Pickup dropped objects
    int         travel_delay;   // How long to pause between travel moves
    int         explore_delay;  // How long to pause between explore moves

    int         arena_delay;
    bool        arena_dump_msgs;
    bool        arena_dump_msgs_all;
    bool        arena_list_eq;
    bool        arena_force_ai;

    // Messages that stop travel
    std::vector<message_filter> travel_stop_message;
    std::vector<message_filter> force_more_message;

    int         stash_tracking; // How stashes are tracked

    int         tc_reachable;   // Colour for squares that are reachable
    int         tc_excluded;    // Colour for excluded squares.
    int         tc_exclude_circle; // Colour for squares in the exclusion radius
    int         tc_dangerous;   // Colour for trapped squares, deep water, lava.
    int         tc_disconnected;// Areas that are completely disconnected.
    std::vector<text_pattern> auto_exclude; // Automatically set an exclusion
                                            // around certain monsters.

    int         travel_stair_cost;

    bool        show_waypoints;

    bool        classic_item_colours;   // Use old-style item colours
    bool        item_colour;    // Colour items on level map

    unsigned    evil_colour; // Colour for things player's god dissapproves

    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items
    unsigned    status_caption_colour;      // Colour of captions in HUD.

    unsigned    heap_brand;         // Highlight heaps of items
    unsigned    stab_brand;         // Highlight monsters that are stabbable
    unsigned    may_stab_brand;     // Highlight potential stab candidates
    unsigned    feature_item_brand; // Highlight features covered by items.
    unsigned    trap_item_brand;    // Highlight traps covered by items.

    bool        trap_prompt;        // Prompt when stepping on mechnical traps
                                    // without enough hp (using trapwalk.lua)

    // What is the minimum number of items in a stack for which
    // you show summary (one-line) information
    int         item_stack_summary_minimum;

    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view

    int         explore_stop_prompt;

    bool        explore_greedy;    // Explore goes after items as well.

    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;

    // Some experimental improvements to explore
    bool        explore_improved;

    std::vector<sound_mapping> sound_mappings;
    std::vector<colour_mapping> menu_colour_mappings;
    std::vector<message_colour_mapping> message_colour_mappings;

    bool       menu_colour_prefix_class; // Prefix item class to string
    bool       menu_colour_shops;   // Use menu colours in shops?

    std::vector<menu_sort_condition> sort_menus;

    int         dump_kill_places;   // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;
    bool        dump_book_spells;

    // Order of sections in the character dump.
    std::vector<std::string> dump_order;

    bool        level_map_title;    // Show title in level map
    bool        target_zero_exp;    // If true, targetting targets zero-exp
                                    // monsters.
    bool        target_wrap;        // Wrap around from last to first target
    bool        target_oos;         // 'x' look around can target out-of-LOS
    bool        target_los_first;   // 'x' look around first goes to visible
                                    // objects/features, then goes to stuff
                                    // outside LOS.
    bool        target_unshifted_dirs; // Unshifted keys target if cursor is
                                       // on player.

    int         drop_mode;          // Controls whether single or multidrop
                                    // is the default.
    int         pickup_mode;        // -1 for single, 0 for menu,
                                    // X for 'if at least X items present'

    bool        easy_exit_menu;     // Menus are easier to get out of

    int         assign_item_slot;   // How free slots are assigned

    std::vector<text_pattern> drop_filter;

    FixedArray<bool, NUM_DELAYS, NUM_AINTERRUPTS> activity_interrupts;

    // Previous startup options
    bool        remember_name;      // Remember and reprompt with last name

    bool        dos_use_background_intensity;

    bool        use_fake_cursor;    // Draw a fake cursor instead of relying
                                    // on the term's own cursor.

    int         level_map_cursor_step;  // The cursor increment in the level
                                        // map.

    // If the player prefers to merge kill records, this option can do that.
    int         kill_map[KC_NCATEGORIES];

    bool        rest_wait_both; // Stop resting only when both HP and MP are
                                // fully restored.

#ifdef WIZARD
    // Parameters for fight simulations.
    long        fsim_rounds;
    int         fsim_str, fsim_int, fsim_dex;
    int         fsim_xl;
    std::string fsim_mons;
    std::vector<std::string> fsim_kit;
#endif  // WIZARD

#ifdef USE_TILE
    char        tile_show_items[20]; // show which item types in tile inventory
    bool        tile_title_screen;   // display title screen?
    bool        tile_menu_icons;     // display icons in menus?
    // minimap colours
    char        tile_player_col;
    char        tile_monster_col;
    char        tile_neutral_col;
    char        tile_friendly_col;
    char        tile_plant_col;
    char        tile_item_col;
    char        tile_unseen_col;
    char        tile_floor_col;
    char        tile_wall_col;
    char        tile_mapped_wall_col;
    char        tile_door_col;
    char        tile_downstairs_col;
    char        tile_upstairs_col;
    char        tile_feature_col;
    char        tile_trap_col;
    char        tile_water_col;
    char        tile_lava_col;
    char        tile_excluded_col;
    char        tile_excl_centre_col;
    char        tile_window_col;
    // font settings
    std::string tile_font_crt_file;
    int         tile_font_crt_size;
    std::string tile_font_msg_file;
    int         tile_font_msg_size;
    std::string tile_font_stat_file;
    int         tile_font_stat_size;
    std::string tile_font_lbl_file;
    int         tile_font_lbl_size;
    std::string tile_font_tip_file;
    int         tile_font_tip_size;
    // window settings
    screen_mode tile_full_screen;
    int         tile_window_width;
    int         tile_window_height;
    int         tile_map_pixels;
    // display settings
    int         tile_update_rate;
    int         tile_key_repeat_delay;
    int         tile_tooltip_ms;
    tag_pref    tile_tag_pref;
    tile_display_type  tile_display;
#endif

    typedef std::map<std::string, std::string> opt_map;
    opt_map     named_options;          // All options not caught above are
                                        // recorded here.

    ///////////////////////////////////////////////////////////////////////
    // These options cannot be directly set by the user. Instead they're
    // set indirectly to the choices the user made for the last character
    // created. XXX: Isn't there a better place for these?
    std::string prev_name;
    char        prev_race;
    char        prev_cls;
    int         prev_ck, prev_dk;
    god_type    prev_pr;
    int         prev_weapon;
    int         prev_book;
    int         prev_wand;
    bool        prev_randpick;

    ///////////////////////////////////////////////////////////////////////
    // tutorial
    FixedVector<bool, 85> tutorial_events;
    bool tut_explored;
    bool tut_stashes;
    bool tut_travel;
    unsigned int tut_spell_counter;
    unsigned int tut_throw_counter;
    unsigned int tut_berserk_counter;
    unsigned int tut_melee_counter;
    unsigned int tut_last_healed;
    unsigned int tut_seen_invisible;

    bool tut_just_triggered;
    unsigned int tutorial_type;
    unsigned int tutorial_left;

private:
    typedef std::map<std::string, std::string> string_map;
    string_map               aliases;
    string_map               variables;
    std::set<std::string>    constants; // Variables that can't be changed
    std::set<std::string>    included;  // Files we've included already.

public:
    // Convenience accessors for the second-class options in named_options.
    int         o_int(const char *name, int def = 0) const;
    long        o_long(const char *name, long def = 0L) const;
    bool        o_bool(const char *name, bool def = false) const;
    std::string o_str(const char *name, const char *def = NULL) const;
    int         o_colour(const char *name, int def = LIGHTGREY) const;

    // Fix option values if necessary, specifically file paths.
    void fixup_options();

private:
    std::string unalias(const std::string &key) const;
    std::string expand_vars(const std::string &field) const;
    void add_alias(const std::string &alias, const std::string &name);

    void clear_feature_overrides();
    void clear_cset_overrides();
    void add_cset_override(char_set_type set, const std::string &overrides);
    void add_cset_override(char_set_type set, dungeon_char_type dc,
                           unsigned symbol);
    void add_feature_override(const std::string &);

    void add_message_colour_mappings(const std::string &);
    void add_message_colour_mapping(const std::string &);
    message_filter parse_message_filter(const std::string &s);

    void set_default_activity_interrupts();
    void clear_activity_interrupts(FixedVector<bool, NUM_AINTERRUPTS> &eints);
    void set_activity_interrupt(
        FixedVector<bool, NUM_AINTERRUPTS> &eints,
        const std::string &interrupt);
    void set_activity_interrupt(const std::string &activity_name,
                                const std::string &interrupt_names,
                                bool append_interrupts,
                                bool remove_interrupts);
    void set_fire_order(const std::string &full, bool add);
    void add_fire_order_slot(const std::string &s);
    void set_menu_sort(std::string field);
    void new_dump_fields(const std::string &text, bool add = true);
    void do_kill_map(const std::string &from, const std::string &to);
    int  read_explore_stop_conditions(const std::string &) const;

    void split_parse(const std::string &s, const std::string &separator,
                     void (game_options::*add)(const std::string &));
    void add_mon_glyph_override(monster_type mtype, mon_display &mdisp);
    void add_mon_glyph_overrides(const std::string &mons, mon_display &mdisp);
    void add_mon_glyph_override(const std::string &);
    mon_display parse_mon_glyph(const std::string &s) const;
    void set_option_fragment(const std::string &s);

    static const std::string interrupt_prefix;
};

extern game_options  Options;

#include "msvc.h"

#endif // EXTERNS_H

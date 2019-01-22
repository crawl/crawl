#pragma once

#include <algorithm>

#include "activity-interrupt-type.h"
#include "char-set-type.h"
#include "confirm-level-type.h"
#include "confirm-prompt-type.h"
#include "feature.h"
#include "flang-t.h"
#include "flush-reason-type.h"
#include "hunger-state-t.h"
#include "lang-t.h"
#include "maybe-bool.h"
#include "mpr.h"
#include "newgame-def.h"
#include "pattern.h"
#include "screen-mode.h"
#include "skill-focus-mode.h"
#include "tag-pref.h"

enum autosac_type
{
    AS_NO,
    AS_YES,
    AS_PROMPT,
    AS_BEFORE_EXPLORE,
    AS_PROMPT_IGNORE,
};

struct message_filter
{
    int             channel;        // Use -1 to match any channel.
    text_pattern    pattern;        // Empty pattern matches any message

    message_filter(int ch, const string &s)
        : channel(ch), pattern(s)
    {
    }

    message_filter(const string &s) : channel(-1), pattern(s, true) { }

    bool operator== (const message_filter &mf) const
    {
        return channel == mf.channel && pattern == mf.pattern;
    }

    bool is_filtered(int ch, const string &s) const
    {
        bool channel_match = ch == channel || channel == -1;
        if (!channel_match || pattern.empty())
            return channel_match;
        return pattern.matches(s);
    }
};

struct sound_mapping
{
    text_pattern pattern;
    string       soundfile;
    bool         interrupt_game;

    bool operator== (const sound_mapping &o) const
    {
        return pattern == o.pattern
                && soundfile == o.soundfile
                && interrupt_game == o.interrupt_game;
    }
};

struct colour_mapping
{
    string tag;
    text_pattern pattern;
    colour_t colour;
    bool operator== (const colour_mapping &o) const
    {
        return tag == o.tag && pattern == o.pattern && colour == o.colour;
    }
};

struct message_colour_mapping
{
    message_filter message;
    msg_colour_type colour;
    bool operator== (const message_colour_mapping &o) const
    {
        return message == o.message && colour == o.colour;
    }
};

struct flang_entry
{
    flang_t lang_id;
    int value;
};

enum use_animation_type
{
    UA_NONE             = 0,
    // projectile animations, from throwing weapons, fireball, etc
    UA_BEAM             = (1 << 0),
    // flashes the screen when trying to cast a spell beyond its range
    UA_RANGE            = (1 << 1),
    // flashes the screen on low hitpoint warning
    UA_HP               = (1 << 2),
    // flashes the screen on attempt to travel or rest with a monster in view
    UA_MONSTER_IN_SIGHT = (1 << 3),
    // various animations for picking up runes and the orb
    UA_PICKUP           = (1 << 4),
    // various monster spell/ability effects (slime creature merging, etc)
    UA_MONSTER          = (1 << 5),
    // various player spell/ability animation effects (shatter, etc)
    UA_PLAYER           = (1 << 6),
    // animation when entering certain branches (abyss, zot, etc)
    UA_BRANCH_ENTRY     = (1 << 7),
    // animations that can't be turned off (please don't use unless they can
    // be turned off some other way)
    UA_ALWAYS_ON        = (1 << 8),
};
DEF_BITFIELD(use_animations_type, use_animation_type);

class LineInput;
class GameOption;
struct game_options
{
public:
    game_options();
    ~game_options();
    void reset_options();

    void read_option_line(const string &s, bool runscripts = false);
    void read_options(LineInput &, bool runscripts,
                      bool clear_aliases = true);

    void include(const string &file, bool resolve, bool runscript);
    void report_error(PRINTF(1, ));

    string resolve_include(const string &file, const char *type = "");

    bool was_included(const string &file) const;

    static string resolve_include(string including_file, string included_file,
                            const vector<string> *rcdirs = nullptr);

#ifdef USE_TILE_WEB
    void write_webtiles_options(const string &name);
#endif

public:
    string      filename;     // The name of the file containing options.
    string      basefilename; // Base (pathless) file name
    int         line_num;     // Current line number being processed.

    // View options
    map<dungeon_feature_type, feature_def> feature_colour_overrides;
    map<dungeon_feature_type, FixedVector<char32_t, 2> > feature_symbol_overrides;
    map<monster_type, cglyph_t> mon_glyph_overrides;
    char32_t cset_override[NUM_DCHAR_TYPES];
    typedef pair<string, cglyph_t> item_glyph_override_type;
    vector<item_glyph_override_type > item_glyph_overrides;
    map<string, cglyph_t> item_glyph_cache;

    string      save_dir;       // Directory where saves and bones go.
    string      macro_dir;      // Directory containing macro.txt
    string      morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    string      shared_dir;     // Directory where the logfile, scores and bones
                                // are stored. On a multi-user system, this dir
                                // should be accessible by different people.
    vector<string> additional_macro_files;

    uint64_t    seed;   // Non-random games.

#ifdef DGL_SIMPLE_MESSAGING
    bool        messaging;      // Check for messages.
#endif

    bool        suppress_startup_errors;

    bool        mouse_input;

    int         view_max_width;
    int         view_max_height;
    int         mlist_min_height;
    int         msg_min_height;
    int         msg_max_height;
    int         msg_webtiles_height;
    bool        mlist_allow_alternate_layout;
    bool        messages_at_top;
    bool        msg_condense_repeats;
    bool        msg_condense_short;

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

    // Whether exclusions and exclusion radius are visible in the viewport.
    bool        always_show_exclusions;

    int         autopickup_on;
    bool        autopickup_starting_ammo;
    bool        default_manual_training;
    bool        default_show_all_skills;

    bool        show_newturn_mark;// Show underscore prefix in messages for new turn
    bool        show_game_time; // Show game time instead of player turns.
    bool        equip_bar; // Show equip bar instead of noise bar.
    bool        animate_equip_bar; // Animate colours in equip bar.

    FixedBitVector<NUM_OBJECT_CLASSES> autopickups; // items to autopickup
    bool        auto_switch;     // switch melee&ranged weapons according to enemy range
    bool        travel_open_doors;     // open doors while exploring
    bool        easy_unequip;    // allow auto-removing of armour / jewellery
    bool        equip_unequip;   // Make 'W' = 'T', and 'P' = 'R'.
    bool        jewellery_prompt; // Always prompt for slot when changing jewellery.
    bool        easy_door;       // 'O', 'C' don't prompt with just one door.
    bool        warn_hatches;    // offer a y/n prompt when the player uses an escape hatch
    bool        enable_recast_spell; // Allow recasting spells with 'z' Enter.
    int         confirm_butcher; // When to prompt for butchery
    hunger_state_t auto_butcher; // auto-butcher corpses while travelling
    bool        easy_eat_chunks; // make 'e' auto-eat the oldest safe chunk
    bool        auto_eat_chunks; // allow eating chunks while resting or travelling
    skill_focus_mode skill_focus; // is the focus skills available
    bool        auto_hide_spells; // hide new spells

    bool        note_all_skill_levels;  // take note for all skill levels (1-27)
    bool        note_skill_max;   // take note when skills reach new max
    string      user_note_prefix; // Prefix for user notes
    int         note_hp_percent;  // percentage hp for notetaking
    bool        note_xom_effects; // take note of all Xom effects
    bool        note_chat_messages; // log chat in Webtiles
    bool        note_dgl_messages; // log chat in DGL
    confirm_level_type easy_confirm;    // make yesno() confirming easier
    bool        easy_quit_item_prompts; // make item prompts quitable on space
    confirm_prompt_type allow_self_target;      // yes, no, prompt
    bool        simple_targeting; // disable smart spell targeting

    int         colour[16];      // macro fg colours to other colours
    unsigned    background_colour; // select default background colour
    unsigned    foreground_colour; // select default foreground colour
    msg_colour_type channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    use_animations_type use_animations; // which animations to show
    bool        darken_beyond_range; // whether to darken squares out of range

    int         hp_warning;      // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    bool        clear_messages;   // clear messages each turn
    bool        show_more;        // Show message-full more prompts.
    bool        small_more;       // Show one-char more prompts.
    unsigned    friend_brand;     // Attribute for branding friendly monsters
    unsigned    neutral_brand;    // Attribute for branding neutral monsters
    bool        blink_brightens_background; // Assume blink will brighten bg.
    bool        bold_brightens_foreground; // Assume bold will brighten fg.
    bool        best_effort_brighten_background; // Allow bg brighten attempts.
    bool        best_effort_brighten_foreground; // Allow fg brighten attempts.
    bool        allow_extended_colours; // Use more than 8 terminal colours.
    bool        macro_meta_entry; // Allow user to use numeric sequences when
                                  // creating macros
    int         autofight_warning;      // Amount of real time required between
                                        // two autofight commands
    bool        cloud_status;     // Whether to show a cloud status light

    bool        wall_jump_prompt; // Whether to ask for confirmation before jumps.
    bool        wall_jump_move;   // Whether to allow wall jump via movement

    int         fire_items_start; // index of first item for fire command
    vector<unsigned> fire_order;  // missile search order for 'f' command

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    bool        sounds_on;              // Allow sound effects to play
    bool        one_SDL_sound_channel;  // Limit to one SDL sound at once

    char_set_type  char_set;
    FixedVector<char32_t, NUM_DCHAR_TYPES> char_table;

#ifdef WIZARD
    int            wiz_mode;      // no, never, start in wiz mode
    int            explore_mode;  // no, never, start in explore mode
#endif
    vector<string> terp_files; // Lua files to load for luaterp
    bool           no_save;    // don't use persistent save files

    // internal use only:
    int         sc_entries;      // # of score entries
    int         sc_format;       // Format for score entries

    vector<pair<int, int> > hp_colour;
    vector<pair<int, int> > mp_colour;
    vector<pair<int, int> > stat_colour;
    vector<int> enemy_hp_colour;

    string map_file_name;   // name of mapping file to use
    vector<pair<text_pattern, bool> > force_autopickup;
    vector<text_pattern> note_monsters;  // Interesting monsters
    vector<text_pattern> note_messages;  // Interesting messages
    vector<pair<text_pattern, string> > autoinscriptions;
    vector<text_pattern> note_items;     // Objects to note
    // Skill levels to note
    FixedBitVector<MAX_SKILL_LEVEL + 1> note_skill_levels;
    vector<pair<text_pattern, string>> auto_spell_letters;
    vector<pair<text_pattern, string>> auto_item_letters;
    vector<pair<text_pattern, string>> auto_ability_letters;

    bool        pickup_thrown;  // Pickup thrown missiles
    int         travel_delay;   // How long to pause between travel moves
    int         explore_delay;  // How long to pause between explore moves
    int         rest_delay;     // How long to pause between rest moves

    bool        show_travel_trail;

    int         view_delay;

    bool        arena_dump_msgs;
    bool        arena_dump_msgs_all;
    bool        arena_list_eq;

    vector<message_filter> force_more_message;
    vector<message_filter> flash_screen_message;
    vector<text_pattern> confirm_action;

    unsigned    tc_reachable;   // Colour for squares that are reachable
    unsigned    tc_excluded;    // Colour for excluded squares.
    unsigned    tc_exclude_circle; // Colour for squares in the exclusion radius
    // Colour for squares which are safely traversable but that travel
    // considers forbidden, either because they are themselves forbidden by
    // travel options or exclusions or because they are only accessible through
    // such squares.
    unsigned    tc_forbidden;
    // Colour for squares that are not safely traversable (e.g. deep water,
    // lava, or traps)
    unsigned    tc_dangerous;
    unsigned    tc_disconnected;// Areas that are completely disconnected.
    vector<text_pattern> auto_exclude; // Automatically set an exclusion
                                       // around certain monsters.

    unsigned    evil_colour; // Colour for things player's god dissapproves

    unsigned    remembered_monster_colour;  // Colour of remembered monsters
    unsigned    detected_monster_colour;    // Colour of detected monsters
    unsigned    detected_item_colour;       // Colour of detected items
    unsigned    status_caption_colour;      // Colour of captions in HUD.

    unsigned    heap_brand;         // Highlight heaps of items
    unsigned    stab_brand;         // Highlight monsters that are stabbable
    unsigned    may_stab_brand;     // Highlight potential stab candidates
    unsigned    feature_item_brand; // Highlight features covered by items.
    unsigned    trap_item_brand;    // Highlight traps covered by items.

    // What is the minimum number of items in a stack for which
    // you show summary (one-line) information
    int         item_stack_summary_minimum;

    int         explore_stop;      // Stop exploring if a previously unseen
                                   // item comes into view

    // Don't stop greedy explore when picking up an item which matches
    // any of these patterns.
    vector<text_pattern> explore_stop_pickup_ignore;

    bool        explore_greedy;    // Explore goes after items as well.

    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;

    // How much autoexplore favors visiting squares next to walls.
    int         explore_wall_bias;

    // Wait for rest wait percent HP and MP before exploring.
    bool        explore_auto_rest;

    bool        travel_key_stop;   // Travel stops on keypress.

    vector<sound_mapping> sound_mappings;
    string sound_file_path;
    vector<colour_mapping> menu_colour_mappings;
    vector<message_colour_mapping> message_colour_mappings;

    vector<menu_sort_condition> sort_menus;

    bool        dump_on_save;       // Automatically dump character when saving.
    int         dump_kill_places;   // How to dump place information for kills.
    int         dump_message_count; // How many old messages to dump

    int         dump_item_origins;  // Show where items came from?
    int         dump_item_origin_price;
    bool        dump_book_spells;

    // Order of sections in the character dump.
    vector<string> dump_order;

    int         pickup_menu_limit;  // Over this number of items, menu for
                                    // pickup
    bool        ability_menu;       // 'a'bility starts with a full-screen menu
    bool        easy_floor_use;     // , selects the floor item if there's 1

    int         assign_item_slot;   // How free slots are assigned
    maybe_bool  show_god_gift;      // Show {god gift} in item names

    maybe_bool  restart_after_game; // If true, Crawl will not close on game-end
                                    // If maybe, Crawl will restart only if the
                                    // CL options would bring up the startup
                                    // menu.
    bool        restart_after_save; // .. or on save
    bool        newgame_after_quit; // override the restart_after_game behavior
                                    // to always start a new game on quit.

    bool        name_bypasses_menu; // should the menu be skipped if there is
                                    // a name set on game start
    bool        read_persist_options; // If true, Crawl will try to load
                                      // options from c_persist.options

    vector<text_pattern> drop_filter;

    map<string, FixedBitVector<NUM_AINTERRUPTS>> activity_interrupts;
#ifdef DEBUG_DIAGNOSTICS
    FixedBitVector<NUM_DIAGNOSTICS> quiet_debug_messages;
#endif

    // Previous startup options
    bool        remember_name;      // Remember and reprompt with last name

    bool        dos_use_background_intensity;

    bool        use_fake_cursor;    // Draw a fake cursor instead of relying
                                    // on the term's own cursor.
    bool        use_fake_player_cursor;

    bool        show_player_species;

    int         level_map_cursor_step;  // The cursor increment in the level
                                        // map.

    bool        use_modifier_prefix_keys; // Treat '/' as SHIFT and '*' as CTRL

    // If the player prefers to merge kill records, this option can do that.
    int         kill_map[KC_NCATEGORIES];

    bool        rest_wait_both; // Stop resting only when both HP and MP are
                                // fully restored.

    bool        rest_wait_ancestor;// Stop resting only if the ancestor's HP
                                   // is fully restored.

    int         rest_wait_percent; // Stop resting after restoring this
                                   // fraction of HP or MP

    bool        regex_search; // whether to default to regex search for ^F
    bool        autopickup_search; // whether to annotate stash items with
                                   // autopickup status

    lang_t              language;         // Translation to use.
    const char*         lang_name;        // Database name of the language.
    vector<flang_entry> fake_langs;       // The fake language(s) in use.
    bool has_fake_lang(flang_t flang)
    {
        return any_of(begin(fake_langs), end(fake_langs),
                      [flang] (const flang_entry &f)
                      { return f.lang_id == flang; });
    }

    // -1 and 0 mean no confirmation, other possible values are 1,2,3 (see fail_severity())
    int         fail_severity_to_confirm;
#ifdef WIZARD
    // Parameters for fight simulations.
    string      fsim_mode;
    bool        fsim_csv;
    int         fsim_rounds;
    string      fsim_mons;
    vector<string> fsim_scale;
    vector<string> fsim_kit;
#endif  // WIZARD

#ifdef USE_TILE
    // TODO: have these present but ignored in non-tile builds
    string      tile_show_items; // show which item types in tile inventory
    bool        tile_skip_title; // wait for a key at title screen?
    bool        tile_menu_icons; // display icons in menus?

    // minimap colours
    VColour     tile_unseen_col;
    VColour     tile_floor_col;
    VColour     tile_wall_col;
    VColour     tile_mapped_floor_col;
    VColour     tile_mapped_wall_col;
    VColour     tile_door_col;
    VColour     tile_item_col;
    VColour     tile_monster_col;
    VColour     tile_plant_col;
    VColour     tile_upstairs_col;
    VColour     tile_downstairs_col;
    VColour     tile_branchstairs_col;
    VColour     tile_feature_col;
    VColour     tile_water_col;
    VColour     tile_lava_col;
    VColour     tile_trap_col;
    VColour     tile_excl_centre_col;
    VColour     tile_excluded_col;
    VColour     tile_player_col;
    VColour     tile_deep_water_col;
    VColour     tile_portal_col;
    VColour     tile_transporter_col;
    VColour     tile_transporter_landing_col;
    VColour     tile_explore_horizon_col;

    VColour     tile_window_col;
#ifdef USE_TILE_LOCAL
    // font settings
    string      tile_font_crt_file;
    string      tile_font_msg_file;
    string      tile_font_stat_file;
    string      tile_font_lbl_file;
    string      tile_font_tip_file;
    bool        tile_single_column_menus;
#endif
#ifdef USE_TILE_WEB
    string      tile_font_crt_family;
    string      tile_font_msg_family;
    string      tile_font_stat_family;
    string      tile_font_lbl_family;
#endif
    int         tile_font_crt_size;
    int         tile_font_msg_size;
    int         tile_font_stat_size;
    int         tile_font_lbl_size;
    int         tile_font_tip_size;
#ifdef USE_TILE_LOCAL
#ifdef USE_FT
    bool        tile_font_ft_light;
#endif
    // window settings
    screen_mode tile_full_screen;
    int         tile_window_width;
    int         tile_window_height;
    maybe_bool  tile_use_small_layout;
#endif
    int         tile_cell_pixels;
    bool        tile_filter_scaling;
    int         tile_map_pixels;

    bool        tile_force_overlay;
    // display settings
    int         tile_update_rate;
    int         tile_runrest_rate;
    int         tile_key_repeat_delay;
    int         tile_tooltip_ms;
    tag_pref    tile_tag_pref;

    bool        tile_show_minihealthbar;
    bool        tile_show_minimagicbar;
    bool        tile_show_demon_tier;
    bool        tile_water_anim;
    bool        tile_misc_anim;
    vector<string> tile_layout_priority;
    monster_type tile_use_monster;
    tileidx_t   tile_player_tile;
    pair<int, int> tile_weapon_offsets;
    pair<int, int> tile_shield_offsets;
#ifdef USE_TILE_WEB
    bool        tile_realtime_anim;
    string      tile_display_mode;
    bool        tile_level_map_hide_messages;
    bool        tile_level_map_hide_sidebar;
    bool        tile_web_mouse_control;
#endif
#endif // USE_TILE

    typedef map<string, string> opt_map;
    opt_map     named_options;          // All options not caught above are
                                        // recorded here.

    newgame_def game;      // Choices for new game.

private:
    typedef map<string, string> string_map;
    string_map     aliases;
    string_map     variables;
    set<string>    constants; // Variables that can't be changed
    set<string>    included;  // Files we've included already.

public:
    // Fix option values if necessary, specifically file paths.
    void fixup_options();

private:
    string unalias(const string &key) const;
    string expand_vars(const string &field) const;
    void add_alias(const string &alias, const string &name);

    void clear_feature_overrides();
    void clear_cset_overrides();
    void add_cset_override(dungeon_char_type dc, int symbol);
    void add_feature_override(const string &, bool prepend);
    void remove_feature_override(const string &, bool prepend);

    void add_message_colour_mappings(const string &, bool, bool);
    void add_message_colour_mapping(const string &, bool, bool);
    message_filter parse_message_filter(const string &s);

    void set_default_activity_interrupts();
    void set_activity_interrupt(FixedBitVector<NUM_AINTERRUPTS> &eints,
                                const string &interrupt);
    void set_activity_interrupt(const string &activity_name,
                                const string &interrupt_names,
                                bool append_interrupts,
                                bool remove_interrupts);
    void set_fire_order(const string &full, bool append, bool prepend);
    void add_fire_order_slot(const string &s, bool prepend);
    void set_menu_sort(string field);
    void str_to_enemy_hp_colour(const string &, bool);
    void new_dump_fields(const string &text, bool add = true,
                         bool prepend = false);
    void do_kill_map(const string &from, const string &to);
    int  read_explore_stop_conditions(const string &) const;
    use_animations_type read_use_animations(const string &) const;

    void split_parse(const string &s, const string &separator,
                     void (game_options::*add)(const string &, bool),
                     bool prepend = false);
    void add_mon_glyph_override(const string &, bool prepend);
    void remove_mon_glyph_override(const string &, bool prepend);
    cglyph_t parse_mon_glyph(const string &s) const;
    void add_item_glyph_override(const string &, bool prepend);
    void remove_item_glyph_override(const string &, bool prepend);
    void set_option_fragment(const string &s, bool prepend);
    bool set_lang(const char *s);
    void set_fake_langs(const string &input);
    void set_player_tile(const string &s);
    void set_tile_offsets(const string &s, bool set_shield);

    static const string interrupt_prefix;

    vector<GameOption*> option_behaviour;
    map<string, GameOption*> options_by_name;
    const vector<GameOption*> build_options_list();
    map<string, GameOption*> build_options_map(const vector<GameOption*> &opts);
};

char32_t get_glyph_override(int c);
object_class_type item_class_by_sym(char32_t c);

#ifdef DEBUG_GLOBALS
#define Options (*real_Options)
#endif
extern game_options  Options;

static inline short macro_colour(short col)
{
    ASSERT(col < NUM_TERM_COLOURS);
    return col < 0 ? col : Options.colour[ col ];
}

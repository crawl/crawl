#ifndef OPTIONS_H
#define OPTIONS_H

#include "feature.h"
#include "pattern.h"
#include "newgame_def.h"

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
    bool operator== (const sound_mapping &o) const
    {
        return pattern == o.pattern && soundfile == o.soundfile;
    }
};

struct colour_mapping
{
    string tag;
    text_pattern pattern;
    int colour;
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

class LineInput;
struct game_options
{
public:
    game_options();
    void reset_options();

    void read_option_line(const string &s, bool runscripts = false);
    void read_options(LineInput &, bool runscripts,
                      bool clear_aliases = true);

    void include(const string &file, bool resolve, bool runscript);
    void report_error(PRINTF(1, ));

    string resolve_include(const string &file, const char *type = "");

    bool was_included(const string &file) const;

    static string resolve_include(string including_file, string included_file,
                            const vector<string> *rcdirs = NULL) throw (string);

public:
    string      filename;     // The name of the file containing options.
    string      basefilename; // Base (pathless) file name
    int         line_num;     // Current line number being processed.

    // View options
    map<dungeon_feature_type, feature_def> feature_overrides;
    map<monster_type, cglyph_t> mon_glyph_overrides;
    ucs_t cset_override[NUM_DCHAR_TYPES];
    vector<pair<string, cglyph_t> > item_glyph_overrides;
    map<string, cglyph_t> item_glyph_cache;

    string      save_dir;       // Directory where saves and bones go.
    string      macro_dir;      // Directory containing macro.txt
    string      morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    string      shared_dir;     // Directory where the logfile, scores and bones
                                // are stored.  On a multi-user system, this dir
                                // should be accessible by different people.
    vector<string> additional_macro_files;

    uint32_t    seed;   // Non-random games.

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
    bool        mlist_allow_alternate_layout;
    bool        messages_at_top;
    bool        mlist_targetting;
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

    int         autopickup_on;
    bool        autopickup_starting_ammo;
    int         default_friendly_pickup;
    bool        default_manual_training;

    bool        show_newturn_mark;// Show underscore prefix in messages for new turn
    bool        show_gold_turns; // Show gold and turns in HUD.
    bool        show_game_turns; // Show game turns instead of player turns.

    FixedBitVector<NUM_OBJECT_CLASSES> autopickups; // items to autopickup
    bool        auto_switch;     // switch melee&ranged weapons according to enemy range
    bool        show_inventory_weights; // show weights in inventory listings
    bool        clean_map;       // remove unseen clouds/monsters
    bool        show_uncursed;   // label known uncursed items as "uncursed"
    bool        easy_open;       // open doors with movement
    bool        easy_unequip;    // allow auto-removing of armour / jewellery
    bool        equip_unequip;   // Make 'W' = 'T', and 'P' = 'R'.
    int         confirm_butcher; // When to prompt for butchery
    bool        chunks_autopickup; // Autopickup chunks after butchering
    bool        prompt_for_swap; // Prompt to switch back from butchering
                                 // tool if hostile monsters are around.
    chunk_drop_type auto_drop_chunks; // drop chunks when overburdened
    bool        easy_eat_chunks; // make 'e' auto-eat the oldest safe chunk
    bool        auto_eat_chunks; // allow eating chunks while resting or travelling
    bool        default_target;  // start targetting on a real target
    bool        autopickup_no_burden;   // don't autopickup if it changes burden
    skill_focus_mode skill_focus; // is the focus skills available

    bool        note_all_skill_levels;  // take note for all skill levels (1-27)
    bool        note_skill_max;   // take note when skills reach new max
    string      user_note_prefix; // Prefix for user notes
    int         note_hp_percent;  // percentage hp for notetaking
    bool        note_xom_effects; // take note of all Xom effects
    bool        note_chat_messages; // log chat in DGL/Webtiles
    confirm_level_type easy_confirm;    // make yesno() confirming easier
    bool        easy_quit_item_prompts; // make item prompts quitable on space
    confirm_prompt_type allow_self_target;      // yes, no, prompt

    int         colour[16];      // macro fg colours to other colours
    int         background_colour; // select default background colour
    msg_colour_type channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    bool        darken_beyond_range; // for whether targetting is out of range

    int         hp_warning;      // percentage hp for danger warning
    int         magic_point_warning;    // percentage mp for danger warning
    bool        clear_messages;   // clear messages each turn
    bool        show_more;        // Show message-full more prompts.
    bool        small_more;       // Show one-char more prompts.
    unsigned    friend_brand;     // Attribute for branding friendly monsters
    unsigned    neutral_brand;    // Attribute for branding neutral monsters
    bool        no_dark_brand;    // Attribute for branding friendly monsters
    bool        macro_meta_entry; // Allow user to use numeric sequences when
                                  // creating macros

    int         fire_items_start; // index of first item for fire command
    vector<unsigned> fire_order;  // missile search order for 'f' command

    bool        auto_list;       // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    char_set_type  char_set;
    FixedVector<ucs_t, NUM_DCHAR_TYPES> char_table;

    int         num_colours;     // used for setting up curses colour table (8 or 16)
    const char* no_gdb;          // reason for not running gdb

#ifdef WIZARD
    int            wiz_mode;   // no, never, start in wiz mode
    vector<string> terp_files; // Lua files to load for luaterp
    bool           no_save;    // don't use persistent save files
#endif

    // internal use only:
    int         sc_entries;      // # of score entries
    int         sc_format;       // Format for score entries

    vector<pair<int, int> > hp_colour;
    vector<pair<int, int> > mp_colour;
    vector<pair<int, int> > stat_colour;
    vector<int> enemy_hp_colour;
    bool visual_monster_hp;

    string map_file_name;   // name of mapping file to use
    vector<pair<text_pattern, bool> > force_autopickup;
    vector<text_pattern> note_monsters;  // Interesting monsters
    vector<text_pattern> note_messages;  // Interesting messages
    vector<pair<text_pattern, string> > autoinscriptions;
    vector<text_pattern> note_items;     // Objects to note
    FixedBitVector<27+1> note_skill_levels;   // Skill levels to note
    vector<pair<text_pattern, string> > auto_spell_letters;

    bool        autoinscribe_cursed; // Auto-inscribe previosly cursed items.

    bool        pickup_thrown;  // Pickup thrown missiles
    int         travel_delay;   // How long to pause between travel moves
    int         explore_delay;  // How long to pause between explore moves
    int         rest_delay;     // How long to pause between rest moves

    bool        show_travel_trail;

    int         arena_delay;
    bool        arena_dump_msgs;
    bool        arena_dump_msgs_all;
    bool        arena_list_eq;

    vector<message_filter> force_more_message;

    int         tc_reachable;   // Colour for squares that are reachable
    int         tc_excluded;    // Colour for excluded squares.
    int         tc_exclude_circle; // Colour for squares in the exclusion radius
    int         tc_dangerous;   // Colour for trapped squares, deep water, lava.
    int         tc_disconnected;// Areas that are completely disconnected.
    vector<text_pattern> auto_exclude; // Automatically set an exclusion
                                       // around certain monsters.

    int         travel_stair_cost;

    bool        show_waypoints;

    unsigned    evil_colour; // Colour for things player's god dissapproves

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

    int         explore_stop_prompt;

    // Don't stop greedy explore when picking up an item which matches
    // any of these patterns.
    vector<text_pattern> explore_stop_pickup_ignore;

    bool        explore_greedy;    // Explore goes after items as well.

    // How much more eager greedy-explore is for items than to explore.
    int         explore_item_greed;

    // How much autoexplore favors visiting squares next to walls.
    int         explore_wall_bias;

    // Some experimental improvements to explore
    bool        explore_improved;

    bool        travel_key_stop;   // Travel stops on keypress.

    autosac_type auto_sacrifice;

    vector<sound_mapping> sound_mappings;
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

    bool        level_map_title;    // Show title in level map
    bool        target_unshifted_dirs; // Unshifted keys target if cursor is
                                       // on player.

    int         drop_mode;          // Controls whether single or multidrop
                                    // is the default.
    int         pickup_mode;        // -1 for single, 0 for menu,
                                    // X for 'if at least X items present'

    bool        easy_exit_menu;     // Menus are easier to get out of

    int         assign_item_slot;   // How free slots are assigned
    maybe_bool  show_god_gift;      // Show {god gift} in item names

    bool        restart_after_game; // If true, Crawl will not close on game-end

    vector<text_pattern> drop_filter;

    FixedVector<FixedBitVector<NUM_AINTERRUPTS>, NUM_DELAYS> activity_interrupts;
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

    // If the player prefers to merge kill records, this option can do that.
    int         kill_map[KC_NCATEGORIES];

    bool        rest_wait_both; // Stop resting only when both HP and MP are
                                // fully restored.

    lang_t      lang;                // Translation to use.
    const char* lang_name;           // Database name of the language.

#ifdef WIZARD
    // Parameters for fight simulations.
    string      fsim_mode;
    int         fsim_rounds;
    string      fsim_mons;
    vector<string> fsim_scale;
    vector<string> fsim_kit;
#endif  // WIZARD

#ifdef USE_TILE
    string      tile_show_items; // show which item types in tile inventory
    bool        tile_skip_title; // wait for a key at title screen?
    bool        tile_menu_icons; // display icons in menus?

    // minimap colours
    char        tile_player_col;
    char        tile_monster_col;
    char        tile_neutral_col;
    char        tile_peaceful_col;
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
#endif
#ifdef USE_TILE_LOCAL
    // font settings
    string      tile_font_crt_file;
    int         tile_font_crt_size;
    string      tile_font_msg_file;
    int         tile_font_msg_size;
    string      tile_font_stat_file;
    int         tile_font_stat_size;
    string      tile_font_lbl_file;
    int         tile_font_lbl_size;
    string      tile_font_tip_file;
    int         tile_font_tip_size;
#ifdef USE_FT
    bool        tile_font_ft_light;
#endif
    // window settings
    screen_mode tile_full_screen;
    int         tile_window_width;
    int         tile_window_height;
    int         tile_map_pixels;
    int         tile_cell_pixels;
    bool        tile_filter_scaling;
    maybe_bool  tile_use_small_layout;
#endif

#ifdef USE_TILE
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
    bool        tile_force_regenerate_levels;
    bool        tile_water_anim;
    vector<string> tile_layout_priority;
#endif

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
    // Convenience accessors for the second-class options in named_options.
    int         o_int(const char *name, int def = 0) const;
    bool        o_bool(const char *name, bool def = false) const;
    string      o_str(const char *name, const char *def = NULL) const;
    int         o_colour(const char *name, int def = LIGHTGREY) const;

    // Fix option values if necessary, specifically file paths.
    void fixup_options();

private:
    string unalias(const string &key) const;
    string expand_vars(const string &field) const;
    void add_alias(const string &alias, const string &name);

    void clear_feature_overrides();
    void clear_cset_overrides();
    void add_cset_override(char_set_type set, const string &overrides);
    void add_cset_override(char_set_type set, dungeon_char_type dc, int symbol);
    void add_feature_override(const string &);

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

    void split_parse(const string &s, const string &separator,
                     void (game_options::*add)(const string &));
    void add_mon_glyph_overrides(const string &mons, cglyph_t &mdisp);
    void add_mon_glyph_override(const string &);
    cglyph_t parse_mon_glyph(const string &s) const;
    void add_item_glyph_override(const string &);
    void set_option_fragment(const string &s);

    static const string interrupt_prefix;
};

ucs_t get_glyph_override(int c);
object_class_type item_class_by_sym(ucs_t c);

#ifdef DEBUG_GLOBALS
#define Options (*real_Options)
#endif
extern game_options  Options;

#endif

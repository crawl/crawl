#ifndef OPTIONS_H
#define OPTIONS_H

#include "feature.h"
#include "pattern.h"
#include "newgame_def.h"

class InitLineInput;
struct game_options
{
public:
    game_options();
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
    ucs_t cset_override[NUM_CSET][NUM_DCHAR_TYPES];

    std::string save_dir;       // Directory where saves and bones go.
    std::string macro_dir;      // Directory containing macro.txt
    std::string morgue_dir;     // Directory where character dumps and morgue
                                // dumps are saved. Overrides crawl_dir.
    std::string shared_dir;     // Directory where the logfile, scores and bones
                                // are stored.  On a multi-user system, this dir
                                // should be accessible by different people.
    std::vector<std::string> additional_macro_files;

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
    bool        mlist_targeting;
    bool        classic_hud;
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

    bool        verbose_monster_pane;

    int         autopickup_on;
    int         default_friendly_pickup;

    bool        show_gold_turns; // Show gold and turns in HUD.
    bool        show_real_turns; // Show real turns instead of actions.
    bool        show_beam;       // Show targeting beam by default.

    uint32_t    autopickups;     // items to autopickup
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
    bool        default_target;  // start targeting on a real target
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
    int         background_colour; // select default background colour
    msg_colour_type channels[NUM_MESSAGE_CHANNELS];  // msg channel colouring
    bool        darken_beyond_range; // for whether targeting is out of range

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
    std::vector<unsigned> fire_order;   // missile search order for 'f' command

    bool        auto_list;       // automatically jump to appropriate item lists

    bool        flush_input[NUM_FLUSH_REASONS]; // when to flush input buff

    char_set_type  char_set;
    FixedVector<ucs_t, NUM_DCHAR_TYPES> char_table;

    int         num_colours;     // used for setting up curses colour table (8 or 16)

    std::string pizza;

#ifdef WIZARD
    int                      wiz_mode;   // no, never, start in wiz mode
    std::vector<std::string> terp_files; // Lua files to load for luaterp
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
    bool        autoinscribe_cursed; // Auto-inscribe previosly cursed items.

    bool        pickup_thrown;  // Pickup thrown missiles
    bool        pickup_dropped; // Pickup dropped objects
    int         travel_delay;   // How long to pause between travel moves
    int         explore_delay;  // How long to pause between explore moves

    int         arena_delay;
    bool        arena_dump_msgs;
    bool        arena_dump_msgs_all;
    bool        arena_list_eq;

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

    // Don't stop greedy explore when picking up an item which matches
    // any of these patterns.
    std::vector<text_pattern> explore_stop_pickup_ignore;

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
    bool        target_unshifted_dirs; // Unshifted keys target if cursor is
                                       // on player.

    int         drop_mode;          // Controls whether single or multidrop
                                    // is the default.
    int         pickup_mode;        // -1 for single, 0 for menu,
                                    // X for 'if at least X items present'

    bool        easy_exit_menu;     // Menus are easier to get out of

    int         assign_item_slot;   // How free slots are assigned

    bool        restart_after_game; // If true, Crawl will not close on game-end

    std::vector<text_pattern> drop_filter;

    FixedArray<bool, NUM_DELAYS, NUM_AINTERRUPTS> activity_interrupts;

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

#ifdef WIZARD
    // Parameters for fight simulations.
    int         fsim_rounds;
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
#endif

    typedef std::map<std::string, std::string> opt_map;
    opt_map     named_options;          // All options not caught above are
                                        // recorded here.

    newgame_def game;      // Choices for new game.

private:
    typedef std::map<std::string, std::string> string_map;
    string_map               aliases;
    string_map               variables;
    std::set<std::string>    constants; // Variables that can't be changed
    std::set<std::string>    included;  // Files we've included already.

public:
    // Convenience accessors for the second-class options in named_options.
    int         o_int(const char *name, int def = 0) const;
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

#endif

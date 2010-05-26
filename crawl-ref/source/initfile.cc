/*
 *  File:       initfile.cc
 *  Summary:    Simple reading of an init file and system variables
 *  Written by: David Loewenstern
 */

#include "AppHdr.h"

#include "initfile.h"
#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <ctype.h>

#include "chardump.h"
#include "clua.h"
#include "colour.h"
#include "dlua.h"
#include "delay.h"
#include "directn.h"
#include "kills.h"
#include "files.h"
#include "fprop.h"
#include "defines.h"
#ifdef USE_TILE
 #include "tilereg-map.h"
#endif
#include "invent.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-util.h"
#include "newgame.h"
#include "jobs.h"
#include "player.h"
#include "religion.h"
#include "species.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"
#include "items.h"
#include "view.h"
#include "viewchar.h"

// For finding the executable's path
#ifdef TARGET_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined ( __APPLE__ )
extern char **NXArgv;
#elif defined ( __linux__ )
#include <unistd.h>
#endif

const std::string game_options::interrupt_prefix = "interrupt_";
game_options Options;

const static char *obj_syms = ")([/%.?=!.+\\0}X$";
const static int   obj_syms_len = 16;

template<class A, class B> void append_vector(A &dest, const B &src)
{
    dest.insert( dest.end(), src.begin(), src.end() );
}

god_type str_to_god(std::string god)
{
    if (god.empty())
        return (GOD_NO_GOD);

    lowercase(god);

    if (god == "random")
        return (GOD_RANDOM);

    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
        if (lowercase_string(god_name(static_cast<god_type>(i))) == god)
            return (static_cast<god_type>(i));

    return (GOD_NO_GOD);
}

// Returns -1 if unmatched else returns 0-15.
static msg_colour_type _str_to_channel_colour(const std::string &str)
{
    int col = str_to_colour(str);
    msg_colour_type ret = MSGCOL_NONE;
    if (col == -1)
    {
        if (str == "mute")
            ret = MSGCOL_MUTED;
        else if (str == "plain" || str == "off")
            ret = MSGCOL_PLAIN;
        else if (str == "default" || str == "on")
            ret = MSGCOL_DEFAULT;
        else if (str == "alternate")
            ret = MSGCOL_ALTERNATE;
    }
    else
        ret = msg_colour(str_to_colour(str));

    return (ret);
}

static const std::string message_channel_names[ NUM_MESSAGE_CHANNELS ] =
{
    "plain", "friend_action", "prompt", "god", "pray", "duration", "danger",
    "warning", "food", "recovery", "sound", "talk", "talk_visual",
    "intrinsic_gain", "mutation", "monster_spell", "monster_enchant",
    "friend_spell", "friend_enchant", "monster_damage", "monster_target",
    "banishment", "rotten_meat", "equipment", "floor", "multiturn", "examine",
    "examine_filter", "diagnostic", "error", "tutorial"
};

// returns -1 if unmatched else returns 0--(NUM_MESSAGE_CHANNELS-1)
int str_to_channel( const std::string &str )
{
    int ret;

    for (ret = 0; ret < NUM_MESSAGE_CHANNELS; ret++)
    {
        if (str == message_channel_names[ret])
            break;
    }

    return (ret == NUM_MESSAGE_CHANNELS ? -1 : ret);
}

std::string channel_to_str( int channel )
{
    if (channel < 0 || channel >= NUM_MESSAGE_CHANNELS)
        return "";

    return message_channel_names[channel];
}

static startup_book_type _str_to_book(const std::string& str)
{
    if (str == "flame" || str == "fire")
        return (SBT_FIRE);
    if (str == "frost" || str == "cold" || str == "ice")
        return (SBT_COLD);
    if (str == "summ" || str == "summoning")
        return (SBT_SUMM);
    if (str == "random")
        return (SBT_RANDOM);
    if (str == "viable")
        return (SBT_VIABLE);

    return (SBT_NONE);
}

static weapon_type _str_to_weapon(const std::string &str)
{
    if (str == "shortsword" || str == "short sword")
        return (WPN_SHORT_SWORD);
    else if (str == "mace")
        return (WPN_MACE);
    else if (str == "ankus")
        return (WPN_ANKUS);
    else if (str == "spear")
        return (WPN_SPEAR);
    else if (str == "trident")
        return (WPN_TRIDENT);
    else if (str == "hand axe" || str == "handaxe")
        return (WPN_HAND_AXE);
    else if (str == "unarmed" || str == "claws")
        return (WPN_UNARMED);
    else if (str == "random")
        return (WPN_RANDOM);

    return (WPN_UNKNOWN);
}

static std::string _weapon_to_str( int weapon )
{
    switch (weapon)
    {
    case WPN_SHORT_SWORD:
        return "short sword";
    case WPN_MACE:
        return "mace";
    case WPN_ANKUS:
        return "ankus";
    case WPN_SPEAR:
        return "spear";
    case WPN_TRIDENT:
        return "trident";
    case WPN_HAND_AXE:
        return "hand axe";
    case WPN_UNARMED:
        return "claws";
    case WPN_RANDOM:
    default:
        return "random";
    }
}

static startup_wand_type _str_to_wand(const std::string& str)
{
    if (str == "enslavement")
        return (SWT_ENSLAVEMENT);
    if (str == "confusion")
        return (SWT_CONFUSION);
    if (str == "magic darts" || str == "magicdarts")
        return (SWT_MAGIC_DARTS);
    if (str == "frost" || str == "cold" || str == "ice")
        return (SWT_FROST);
    if (str == "flame" || str == "fire")
        return (SWT_FLAME);
    if (str == "striking")
        return (SWT_STRIKING);
    if (str == "random")
        return (SWT_RANDOM);

    return (SWT_NO_SELECTION);
}

// Summon types can be any of mon_summon_type (enum.h), or a relevant summoning
// spell.
int str_to_summon_type (const std::string &str)
{
    if (str == "clone")
        return (MON_SUMM_CLONE);
    if (str == "animate")
        return (MON_SUMM_ANIMATE);
    if (str == "chaos")
        return (MON_SUMM_CHAOS);
    if (str == "miscast")
        return (MON_SUMM_MISCAST);
    if (str == "zot")
        return (MON_SUMM_ZOT);
    if (str == "wrath")
        return (MON_SUMM_WRATH);
    if (str == "aid")
        return (MON_SUMM_AID);

    return (spell_by_name(str));
}

static std::string _wand_to_str( int weapon )
{
    switch (weapon)
    {
    case SWT_ENSLAVEMENT:
        return "enslavement";
    case SWT_CONFUSION:
        return "confusion";
    case SWT_MAGIC_DARTS:
        return "magic darts";
    case SWT_FROST:
        return "frost";
    case SWT_FLAME:
        return "flame";
    case SWT_STRIKING:
        return "striking";
    case SWT_RANDOM:
    default:
        return "random";
    }
}

static fire_type _str_to_fire_types( const std::string &str )
{
    if (str == "launcher")
        return (FIRE_LAUNCHER);
    else if (str == "dart")
        return (FIRE_DART);
    else if (str == "stone")
        return (FIRE_STONE);
    else if (str == "rock")
        return (FIRE_ROCK);
    else if (str == "dagger")
        return (FIRE_DAGGER);
    else if (str == "spear")
        return (FIRE_SPEAR);
    else if (str == "hand axe" || str == "handaxe" || str == "axe")
        return (FIRE_HAND_AXE);
    else if (str == "club")
        return (FIRE_CLUB);
    else if (str == "javelin")
        return (FIRE_JAVELIN);
    else if (str == "net")
        return (FIRE_NET);
    else if (str == "return" || str == "returning")
        return (FIRE_RETURNING);
    else if (str == "inscribed")
        return (FIRE_INSCRIBED);

    return (FIRE_NONE);
}

static std::string _gametype_to_str(game_type type)
{
    switch (type)
    {
    case GAME_TYPE_NORMAL:
        return ("normal");
    case GAME_TYPE_TUTORIAL:
        return ("tutorial");
    case GAME_TYPE_ARENA:
        return ("arena");
    case GAME_TYPE_SPRINT:
        return ("sprint");
    default:
        return ("none");
    }
}

static game_type _str_to_gametype(const std::string& s)
{
    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        game_type t = static_cast<game_type>(i);
        if (s == _gametype_to_str(t))
            return (t);
    }
    return (NUM_GAME_TYPE);
}

static std::string _species_to_str(species_type sp)
{
    if (sp == SP_RANDOM)
        return "random";
    else if (sp == SP_VIABLE)
        return "viable";
    else
        return species_name(sp, 1);
}

static species_type _str_to_species(const std::string &str)
{
    if (str == "random")
        return SP_RANDOM;
    else if (str == "viable")
        return SP_VIABLE;

    species_type ret = SP_UNKNOWN;
    if (str.length() == 2) // scan abbreviations
        ret = get_species_by_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (ret == SP_UNKNOWN)
        ret = str_to_species(str);

    if (ret == SP_UNKNOWN)
        fprintf(stderr, "Unknown species choice: %s\n", str.c_str());

    return (ret);
}

static std::string _job_to_str(job_type job)
{
    if (job == JOB_RANDOM)
        return "random";
    else if (job == JOB_VIABLE)
        return "viable";
    else
        return get_job_name(job);
}

static job_type _str_to_job(const std::string &str)
{
    if (str == "random")
        return JOB_RANDOM;
    else if (str == "viable")
        return JOB_VIABLE;

    job_type job = JOB_UNKNOWN;

    if (str.length() == 2) // scan abbreviations
        job = get_job_by_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (job == JOB_UNKNOWN)
        job = get_job_by_name(str.c_str());

    if (job == JOB_UNKNOWN)
        fprintf( stderr, "Unknown background choice: %s\n", str.c_str() );

    return (job);
}

static bool _read_bool( const std::string &field, bool def_value )
{
    bool ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = true;

    if (field == "false" || field == "0" || field == "no")
        ret = false;

    return (ret);
}

// read a value which can be either a boolean (in which case return
// 0 for true, -1 for false), or a string of the form PREFIX:NUMBER
// (e.g., auto:7), in which case return NUMBER as an int.
static int _read_bool_or_number( const std::string &field, int def_value,
                                 const std::string& num_prefix)
{
    int ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = 0;

    if (field == "false" || field == "0" || field == "no")
        ret = -1;

    if ( field.find(num_prefix) == 0 )
        ret = atoi(field.c_str() + num_prefix.size());

    return (ret);
}


static unsigned curses_attribute(const std::string &field)
{
    if (field == "standout")               // probably reverses
        return CHATTR_STANDOUT;
    else if (field == "bold")              // probably brightens fg
        return CHATTR_BOLD;
    else if (field == "blink")             // probably brightens bg
        return CHATTR_BLINK;
    else if (field == "underline")
        return CHATTR_UNDERLINE;
    else if (field == "reverse")
        return CHATTR_REVERSE;
    else if (field == "dim")
        return CHATTR_DIM;
    else if (field.find("hi:") == 0
             || field.find("hilite:") == 0
             || field.find("highlight:") == 0)
    {
        int col = field.find(":");
        int colour = str_to_colour(field.substr(col + 1));
        if (colour == -1)
        {
            Options.report_error(
                make_stringf("Bad highlight string -- %s\n", field.c_str()));
        }
        else
            return CHATTR_HILITE | (colour << 8);
    }
    else if (field != "none")
    {
        Options.report_error(
            make_stringf("Bad colour -- %s\n", field.c_str()));
    }
    return CHATTR_NORMAL;
}

void game_options::new_dump_fields(const std::string &text, bool add)
{
    // Easy; chardump.cc has most of the intelligence.
    std::vector<std::string> fields = split_string(",", text, true, true);
    if (add)
        append_vector(dump_order, fields);
    else
    {
        for (int f = 0, size = fields.size(); f < size; ++f)
            for (int i = 0, dsize = dump_order.size(); i < dsize; ++i)
            {
                if (dump_order[i] == fields[f])
                {
                    dump_order.erase( dump_order.begin() + i );
                    break;
                }
            }
    }
}

void game_options::set_default_activity_interrupts()
{
    for (int adelay = 0; adelay < NUM_DELAYS; ++adelay)
        for (int aint = 0; aint < NUM_AINTERRUPTS; ++aint)
            activity_interrupts[adelay][aint] = true;

    const char *default_activity_interrupts[] = {
        "interrupt_armour_on = hp_loss, monster_attack",
        "interrupt_armour_off = interrupt_armour_on",
        "interrupt_drop_item = interrupt_armour_on",
        "interrupt_jewellery_on = interrupt_armour_on",
        "interrupt_memorise = interrupt_armour_on, stat",
        "interrupt_butcher = interrupt_armour_on, teleport, stat",
        "interrupt_bottle_blood = interrupt_butcher",
        "interrupt_vampire_feed = interrupt_butcher",
        "interrupt_multidrop = interrupt_butcher",
        "interrupt_macro = interrupt_multidrop",
        "interrupt_travel = interrupt_butcher, statue, hungry, "
                            "burden, monster, hit_monster",
        "interrupt_run = interrupt_travel, message",
        "interrupt_rest = interrupt_run, full_hp, full_mp",

        // Stair ascents/descents cannot be interrupted except by
        // teleportation. Attempts to interrupt the delay will just
        // trash all queued delays, including travel.
        "interrupt_ascending_stairs = teleport",
        "interrupt_descending_stairs = teleport",
        "interrupt_recite = teleport",
        "interrupt_uninterruptible =",
        "interrupt_weapon_swap =",

        NULL
    };

    for (int i = 0; default_activity_interrupts[i]; ++i)
        read_option_line( default_activity_interrupts[i], false );
}

void game_options::clear_activity_interrupts(
        FixedVector<bool, NUM_AINTERRUPTS> &eints)
{
    for (int i = 0; i < NUM_AINTERRUPTS; ++i)
        eints[i] = false;
}

void game_options::set_activity_interrupt(
        FixedVector<bool, NUM_AINTERRUPTS> &eints,
        const std::string &interrupt)
{
    if (interrupt.find(interrupt_prefix) == 0)
    {
        std::string delay_name = interrupt.substr( interrupt_prefix.length() );
        delay_type delay = get_delay(delay_name);
        if (delay == NUM_DELAYS)
        {
            report_error (
                make_stringf("Unknown delay: %s\n", delay_name.c_str()));
            return;
        }

        FixedVector<bool, NUM_AINTERRUPTS> &refints =
            activity_interrupts[delay];

        for (int i = 0; i < NUM_AINTERRUPTS; ++i)
            if (refints[i])
                eints[i] = true;

        return;
    }

    activity_interrupt_type ai = get_activity_interrupt(interrupt);
    if (ai == NUM_AINTERRUPTS)
    {
        report_error (
            make_stringf("Delay interrupt name \"%s\" not recognised.\n",
                         interrupt.c_str()));
        return;
    }

    eints[ai] = true;
}

void game_options::set_activity_interrupt(const std::string &activity_name,
                                          const std::string &interrupt_names,
                                          bool append_interrupts,
                                          bool remove_interrupts)
{
    const delay_type delay = get_delay(activity_name);
    if (delay == NUM_DELAYS)
    {
        report_error (
            make_stringf("Unknown delay: %s\n", activity_name.c_str()));
        return;
    }

    std::vector<std::string> interrupts = split_string(",", interrupt_names);
    FixedVector<bool, NUM_AINTERRUPTS> &eints = activity_interrupts[ delay ];

    if (remove_interrupts)
    {
        FixedVector<bool, NUM_AINTERRUPTS> refints;
        clear_activity_interrupts(refints);
        for (int i = 0, size = interrupts.size(); i < size; ++i)
            set_activity_interrupt(refints, interrupts[i]);

        for (int i = 0; i < NUM_AINTERRUPTS; ++i)
            if (refints[i])
                eints[i] = false;
    }
    else
    {
        if (!append_interrupts)
            clear_activity_interrupts(eints);

        for (int i = 0, size = interrupts.size(); i < size; ++i)
            set_activity_interrupt(eints, interrupts[i]);
    }

    eints[AI_FORCE_INTERRUPT] = true;
}

void game_options::reset_options()
{
    filename     = "unknown";
    basefilename = "unknown";
    line_num     = -1;

    set_default_activity_interrupts();

#if defined(SAVE_DIR_PATH)
#if defined(DGAMELAUNCH)
    save_dir   = SAVE_DIR_PATH;
#else
    save_dir   = SAVE_DIR_PATH "/saves/";
    morgue_dir = SAVE_DIR_PATH "/morgue/";
#endif
#elif defined(TARGET_OS_MACOSX)
    std::string tmp_path_base = std::string(getenv("HOME")) + "/Library/Application Support/" CRAWL;
    save_dir   = tmp_path_base + "/saves/";
    morgue_dir = tmp_path_base + "/morgue/";
#elif !defined(TARGET_OS_DOS)
    save_dir   = "saves/";
#else
    save_dir.clear();
#endif

#if !defined(SHORT_FILE_NAMES) && !defined(SAVE_DIR_PATH) && !defined(TARGET_OS_MACOSX)
    morgue_dir = "morgue/";
#endif

#if !defined(DGAMELAUNCH)
    macro_dir = "settings/";
#endif

    additional_macro_files.clear();

    seed = 0;
#ifdef DGL_SIMPLE_MESSAGING
    messaging = true;
#endif

    mouse_input = false;

    view_max_width   = std::max(33, VIEW_MIN_WIDTH);
    view_max_height  = std::max(21, VIEW_MIN_HEIGHT);
    mlist_min_height = 4;
    msg_min_height   = std::max(7, MSG_MIN_HEIGHT);
    msg_max_height   = std::max(10, MSG_MIN_HEIGHT);
    mlist_allow_alternate_layout = false;
    messages_at_top  = false;
    mlist_targeting = false;
    classic_hud = false;
    msg_condense_repeats = true;
    msg_condense_short = true;

    view_lock_x = true;
    view_lock_y = true;

    center_on_scroll = false;
    symmetric_scroll = true;
    scroll_margin_x  = 2;
    scroll_margin_y  = 2;

    verbose_monster_pane = true;

    autopickup_on    = 1;
    default_friendly_pickup = FRIENDLY_PICKUP_FRIEND;

    show_gold_turns = false;
    show_beam       = true;

    game = newgame_def();

    remember_name = true;

#ifdef USE_ASCII_CHARACTERS
    char_set      = CSET_ASCII;
#else
    // NOTE: Tiles relies on the IBM character set for evaluating glyphs
    //       of magic mapped dungeon cells.
    char_set      = CSET_IBM;
#endif

    // set it to the .crawlrc default
    autopickups = ((1L << 15) | // gold
                   (1L <<  6) | // scrolls
                   (1L <<  8) | // potions
                   (1L << 10) | // books
                   (1L <<  7) | // jewellery
                   (1L <<  3) | // wands
                   (1L <<  4)); // food

    suppress_startup_errors = false;

    show_inventory_weights = false;
    colour_map             = true;
    clean_map              = false;
    show_uncursed          = true;
    easy_open              = true;
    easy_unequip           = true;
    equip_unequip          = false;
    easy_butcher           = true;
    always_confirm_butcher = false;
    chunks_autopickup      = true;
    prompt_for_swap        = true;
    list_rotten            = true;
    prefer_safe_chunks     = true;
    easy_eat_chunks        = false;
    easy_eat_gourmand      = false;
    easy_eat_contaminated  = false;
    easy_confirm           = CONFIRM_SAFE_EASY;
    easy_quit_item_prompts = true;
    allow_self_target      = CONFIRM_PROMPT;
    hp_warning             = 30;
    magic_point_warning    = 0;
    default_target         = true;
    autopickup_no_burden   = false;

    user_note_prefix       = "";
    note_all_skill_levels  = false;
    note_skill_max         = true;
    note_all_spells        = true;
    note_xom_effects       = true;
    note_hp_percent        = 5;
    ood_interesting        = 8;
    rare_interesting       = 9;

    // [ds] Grumble grumble.
    auto_list              = true;

    clear_messages         = false;
    show_more              = true;
    small_more             = false;

    pickup_dropped         = false;
    pickup_thrown          = true;

    travel_delay           = 20;
    explore_delay          = -1;
    travel_stair_cost      = 500;

    arena_delay            = 600;
    arena_dump_msgs        = false;
    arena_dump_msgs_all    = false;
    arena_list_eq          = false;

    // Sort only pickup menus by default.
    sort_menus.clear();
    set_menu_sort("pickup: true");

    tc_reachable           = BLUE;
    tc_excluded            = LIGHTMAGENTA;
    tc_exclude_circle      = RED;
    tc_dangerous           = CYAN;
    tc_disconnected        = DARKGREY;

    show_waypoints         = true;
    item_colour            = true;

    // [ds] Default to jazzy colours.
    detected_item_colour   = GREEN;
    detected_monster_colour= LIGHTRED;
    status_caption_colour  = BROWN;

#ifdef USE_TILE
    classic_item_colours   = true;
#else
    classic_item_colours   = false;
#endif

    easy_exit_menu         = true;
#ifdef TARGET_OS_DOS
    dos_use_background_intensity = false;
#else
    dos_use_background_intensity = true;
#endif

    level_map_title        = true;

    assign_item_slot       = SS_FORWARD;

    macro_meta_entry       = true;

    // 10 was the cursor step default on Linux.
    level_map_cursor_step  = 7;
#ifdef UNIX
    use_fake_cursor        = true;
#else
    use_fake_cursor        = false;
#endif
    use_fake_player_cursor = true;

    stash_tracking         = STM_ALL;

    explore_stop           = (ES_ITEM | ES_STAIR | ES_PORTAL | ES_SHOP
                              | ES_ALTAR | ES_GREEDY_PICKUP
                              | ES_GREEDY_VISITED_ITEM_STACK);

    // The prompt conditions will be combined into explore_stop after
    // reading options.
    explore_stop_prompt    = ES_NONE;

    explore_stop_pickup_ignore.clear();

    explore_item_greed     = 10;
    explore_greedy         = true;

    explore_improved       = false;
    trap_prompt            = true;

    target_unshifted_dirs  = false;
    darken_beyond_range    = true;

    dump_kill_places       = KDO_ONE_PLACE;
    dump_message_count     = 7;
    dump_item_origins      = IODS_ARTEFACTS | IODS_RODS;
    dump_item_origin_price = -1;
    dump_book_spells       = true;

    drop_mode              = DM_MULTI;
    pickup_mode            = -1;

    flush_input[ FLUSH_ON_FAILURE ]     = true;
    flush_input[ FLUSH_BEFORE_COMMAND ] = false;
    flush_input[ FLUSH_ON_MESSAGE ]     = false;
    flush_input[ FLUSH_LUA ]            = true;

    fire_items_start       = 0;           // start at slot 'a'

    // Clear fire_order and set up the defaults.
    set_fire_order("launcher, return, "
                   "javelin / dart / stone / rock /"
                   " spear / net / handaxe / dagger / club, inscribed",
                   false);

    item_stack_summary_minimum = 5;

    pizza.clear();

#ifdef WIZARD
    fsim_rounds = 40000L;
    fsim_mons   = "worm";
    fsim_str = fsim_int = fsim_dex = 15;
    fsim_xl  = 10;
    fsim_kit.clear();
#endif

    // These are only used internally, and only from the commandline:
    // XXX: These need a better place.
    sc_entries             = 0;
    sc_format              = -1;

    friend_brand       = CHATTR_HILITE | (GREEN << 8);
    neutral_brand      = CHATTR_HILITE | (LIGHTGREY << 8);
    stab_brand         = CHATTR_HILITE | (BLUE << 8);
    may_stab_brand     = CHATTR_HILITE | (YELLOW << 8);
    heap_brand         = CHATTR_REVERSE;
    feature_item_brand = CHATTR_REVERSE;
    trap_item_brand    = CHATTR_REVERSE;

    no_dark_brand      = true;

#ifdef WIZARD
    wiz_mode         = WIZ_NO;
    terp_files.clear();
#endif

#ifdef USE_TILE
    strcpy(tile_show_items, "!?/%=([)x}+\\_.");
    tile_title_screen    = true;
    tile_menu_icons      = true;

    // minimap colours
    tile_player_col      = MAP_WHITE;
    tile_monster_col     = MAP_RED;
    tile_neutral_col     = MAP_RED;
    tile_peaceful_col    = MAP_LTRED;
    tile_friendly_col    = MAP_LTRED;
    tile_plant_col       = MAP_DKGREEN;
    tile_item_col        = MAP_GREEN;
    tile_unseen_col      = MAP_BLACK;
    tile_floor_col       = MAP_LTGREY;
    tile_wall_col        = MAP_DKGREY;
    tile_mapped_wall_col = MAP_BLUE;
    tile_door_col        = MAP_BROWN;
    tile_downstairs_col  = MAP_MAGENTA;
    tile_upstairs_col    = MAP_BLUE;
    tile_feature_col     = MAP_CYAN;
    tile_trap_col        = MAP_YELLOW;
    tile_water_col       = MAP_MDGREY;
    tile_lava_col        = MAP_MDGREY;
    tile_excluded_col    = MAP_DKCYAN;
    tile_excl_centre_col = MAP_DKBLUE;
    tile_window_col      = MAP_YELLOW;

    // font selection
    tile_font_crt_file   = MONOSPACED_FONT;
    tile_font_crt_size   = 0;
    tile_font_stat_file  = MONOSPACED_FONT;
    tile_font_stat_size  = 0;
    tile_font_msg_file   = MONOSPACED_FONT;
    tile_font_msg_size   = 0;
    tile_font_tip_file   = MONOSPACED_FONT;
    tile_font_tip_size   = 0;
    tile_font_lbl_file   = PROPORTIONAL_FONT;
    tile_font_lbl_size   = 0;

    // window layout
    tile_full_screen      = SCREENMODE_AUTO;
    tile_window_width     = 0;
    tile_window_height    = 0;
    tile_map_pixels       = 0;
    tile_force_overlay    = false;

    // delays
    tile_update_rate      = 1000;
    tile_runrest_rate     = 100;
    tile_key_repeat_delay = 200;
    tile_tooltip_ms       = 500;
    // XXX: arena may now be chosen after options are read.
    tile_tag_pref         = crawl_state.game_is_arena() ? TAGPREF_NAMED
                                                        : TAGPREF_ENEMY;

    tile_show_minihealthbar  = true;
    tile_show_minimagicbar   = true;
    tile_show_demon_tier     = true;
#endif

    // map each colour to itself as default
    // If USE_8_COLOUR_TERM_MAP is defined, then we force 8 colors.
    // Otherwise, do a check to see if we're using Apple_Terminal.
#ifndef USE_8_COLOUR_TERM_MAP
    const char *term_program = getenv("TERM_PROGRAM");
    if (term_program && strcmp(term_program, "Apple_Terminal") == 0)
    {
#endif
        for (int i = 0; i < 16; ++i)
            colour[i] = i % 8;

        colour[ DARKGREY ] = COL_TO_REPLACE_DARKGREY;
#ifndef USE_8_COLOUR_TERM_MAP
    }
    else
    {
        for (int i = 0; i < 16; ++i)
            colour[i] = i;
    }
#endif

    // map each channel to plain (well, default for now since I'm testing)
    for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        channels[i] = MSGCOL_DEFAULT;

    // Clear vector options.
    dump_order.clear();
    new_dump_fields("header,hiscore,stats,misc,inventory,"
                    "skills,spells,overview,mutations,messages,"
                    "screenshot,monlist,kills,notes");

    hp_colour.clear();
    hp_colour.push_back(std::pair<int,int>(50, YELLOW));
    hp_colour.push_back(std::pair<int,int>(25, RED));
    mp_colour.clear();
    mp_colour.push_back(std::pair<int, int>(50, YELLOW));
    mp_colour.push_back(std::pair<int, int>(25, RED));
    stat_colour.clear();
    stat_colour.push_back(std::pair<int, int>(1, LIGHTRED));
    stat_colour.push_back(std::pair<int, int>(3, RED));

    force_autopickup.clear();
    note_monsters.clear();
    note_messages.clear();
    autoinscriptions.clear();
    autoinscribe_artefacts = true;
    autoinscribe_cursed = true;
    note_items.clear();
    note_skill_levels.clear();
    force_more_message.clear();
    sound_mappings.clear();
    menu_colour_mappings.clear();
    menu_colour_prefix_class = true;
    menu_colour_shops = true;
    message_colour_mappings.clear();
    drop_filter.clear();
    map_file_name.clear();
    named_options.clear();

    clear_cset_overrides();
#ifdef USE_TILE
    // HACK: For some reason, Tiles can't handle the default IBM book
    //       character. Redefine with plain old '+', so it shows up correctly.
    add_cset_override(CSET_IBM, dchar_by_name("item_book"), '+');
#endif

    clear_feature_overrides();
    mon_glyph_overrides.clear();

    rest_wait_both = false;

    // Map each category to itself. The user can override in init.txt
    kill_map[KC_YOU] = KC_YOU;
    kill_map[KC_FRIENDLY] = KC_FRIENDLY;
    kill_map[KC_OTHER] = KC_OTHER;

    // Setup travel information. What's a better place to do this?
    initialise_travel();

    // Forget any files we remembered as included.
    included.clear();

    // Forget variables and such.
    aliases.clear();
    variables.clear();
    constants.clear();
}

void game_options::clear_cset_overrides()
{
    memset(cset_override, 0, sizeof cset_override);
}

void game_options::clear_feature_overrides()
{
    feature_overrides.clear();
}

static unsigned read_symbol(std::string s)
{
    if (s.empty())
        return (0);

    if (s.length() > 1 && s[0] == '\\')
        s = s.substr(1);

    if (s.length() == 1)
        return s[0];

    int base = 10;
    if (s.length() > 1 && s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return (strtoul(s.c_str(), &tail, base));
}

void game_options::set_fire_order(const std::string &s, bool add)
{
    if (!add)
        fire_order.clear();
    std::vector<std::string> slots = split_string(",", s);
    for (int i = 0, size = slots.size(); i < size; ++i)
        add_fire_order_slot(slots[i]);
}

void game_options::add_fire_order_slot(const std::string &s)
{
    unsigned flags = 0;
    std::vector<std::string> alts = split_string("/", s);
    for (int i = 0, size = alts.size(); i < size; ++i)
        flags |= _str_to_fire_types(alts[i]);

    if (flags)
        fire_order.push_back(flags);
}

void game_options::add_mon_glyph_override(monster_type mtype,
                                          mon_display &mdisp)
{
    mdisp.type = mtype;
    mon_glyph_overrides.push_back(mdisp);
}

void game_options::add_mon_glyph_overrides(const std::string &mons,
                                           mon_display &mdisp)
{
    // If one character, this is a monster letter.
    int letter = -1;
    if (mons.length() == 1)
        letter = mons[0] == '_' ? ' ' : mons[0];

    bool found = false;
    for (int i = 0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (me->showchar == letter || me->name == mons)
        {
            found = true;
            add_mon_glyph_override(static_cast<monster_type>(i), mdisp);
        }
    }
    if (!found)
        report_error (
            make_stringf("Unknown monster: \"%s\"", mons.c_str()));
}

mon_display game_options::parse_mon_glyph(const std::string &s) const
{
    mon_display md;
    std::vector<std::string> phrases = split_string(" ", s);
    for (int i = 0, size = phrases.size(); i < size; ++i)
    {
        const std::string &p = phrases[i];
        const int col = str_to_colour(p, -1, false);
        if (col != -1 && colour)
            md.colour = col;
        else
            md.glyph = p == "_"? ' ' : read_symbol(p);
    }
    return (md);
}

void game_options::add_mon_glyph_override(const std::string &text)
{
    std::vector<std::string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    mon_display mdisp = parse_mon_glyph(override[1]);
    if (mdisp.glyph || mdisp.colour)
        add_mon_glyph_overrides(override[0], mdisp);
}

void game_options::add_feature_override(const std::string &text)
{
    std::string::size_type epos = text.rfind("}");
    if (epos == std::string::npos)
        return;

    std::string::size_type spos = text.rfind("{", epos);
    if (spos == std::string::npos)
        return;

    std::string fname = text.substr(0, spos);
    std::string props = text.substr(spos + 1, epos - spos - 1);
    std::vector<std::string> iprops = split_string(",", props, true, true);

    if (iprops.size() < 1 || iprops.size() > 7)
        return;

    if (iprops.size() < 7)
        iprops.resize(7);

    trim_string(fname);
    std::vector<dungeon_feature_type> feats =
        features_by_desc(text_pattern(fname));
    if (feats.empty())
        return;

    for (int i = 0, size = feats.size(); i < size; ++i)
    {
        if (feats[i] >= NUM_FEATURES)
            continue; // TODO: handle other object types.
        feature_override fov;
        fov.object.cls = SH_FEATURE;
        fov.object.feat = feats[i];

        fov.override.symbol         = read_symbol(iprops[0]);
        fov.override.magic_symbol   = read_symbol(iprops[1]);
        fov.override.colour         = str_to_colour(iprops[2], BLACK);
        fov.override.map_colour     = str_to_colour(iprops[3], BLACK);
        fov.override.seen_colour    = str_to_colour(iprops[4], BLACK);
        fov.override.em_colour      = str_to_colour(iprops[5], BLACK);
        fov.override.seen_em_colour = str_to_colour(iprops[6], BLACK);

        feature_overrides.push_back(fov);
    }
}

void game_options::add_cset_override(
        char_set_type set, const std::string &overrides)
{
    std::vector<std::string> overs = split_string(",", overrides);
    for (int i = 0, size = overs.size(); i < size; ++i)
    {
        std::vector<std::string> mapping = split_string(":", overs[i]);
        if (mapping.size() != 2)
            continue;

        dungeon_char_type dc = dchar_by_name(mapping[0]);
        if (dc == NUM_DCHAR_TYPES)
            continue;

        unsigned symbol =
            static_cast<unsigned>(read_symbol(mapping[1]));

        if (set == NUM_CSET)
            for (int c = 0; c < NUM_CSET; ++c)
                add_cset_override(char_set_type(c), dc, symbol);
        else
            add_cset_override(set, dc, symbol);
    }
}

void game_options::add_cset_override(char_set_type set, dungeon_char_type dc,
                                     unsigned symbol)
{
    cset_override[set][dc] = symbol;
}

static std::string _find_crawlrc()
{
    const char* locations_data[][2] = {
        { SysEnv.crawl_dir.c_str(), "init.txt" },
#ifdef MULTIUSER
        { SysEnv.home.c_str(), ".crawlrc" },
        { SysEnv.home.c_str(), "init.txt" },
#endif
#ifndef DATA_DIR_PATH
        { "", "init.txt" },
        { "..", "init.txt" },
#endif
        { NULL, NULL }                // placeholder to mark end
    };

    // We'll look for these files in any supplied -rcdirs.
    static const char *rc_dir_filenames[] = {
        ".crawlrc", "init.txt"
    };

    // -rc option always wins.
    if (!SysEnv.crawl_rc.empty())
        return (SysEnv.crawl_rc);

    // If we have any rcdirs, look in them for files from the
    // rc_dir_names list.
    for (int i = 0, size = SysEnv.rcdirs.size(); i < size; ++i)
    {
        for (unsigned n = 0; n < ARRAYSZ(rc_dir_filenames); ++n)
        {
            const std::string rc(
                catpath(SysEnv.rcdirs[i], rc_dir_filenames[n]));
            if (file_exists(rc))
                return (rc);
        }
    }

    // Check all possibilities for init.txt
    for (int i = 0; locations_data[i][1] != NULL; ++i)
    {
        // Don't look at unset options
        if (locations_data[i][0] != NULL)
        {
            const std::string rc =
                catpath(locations_data[i][0], locations_data[i][1]);
            if (file_exists(rc))
                return (rc);
        }
    }

    // Last attempt: pick up init.txt from datafile_path, which will
    // also search the settings/ directory.
    return (datafile_path("init.txt", false, false));
}

// Returns an error message if the init.txt was not found.
std::string read_init_file(bool runscript)
{
    Options.reset_options();

    Options.filename     = "extra opts first";
    Options.basefilename = "extra opts first";
    Options.line_num     = 0;
    for (unsigned int i = 0; i < SysEnv.extra_opts_first.size(); i++)
    {
        Options.line_num++;
        Options.read_option_line(SysEnv.extra_opts_first[i], true);
    }

    const std::string init_file_name( _find_crawlrc() );

    FILE* f = fopen(init_file_name.c_str(), "r");
    if ( f == NULL )
    {
        if (!init_file_name.empty())
            return make_stringf("(\"%s\" is not readable)",
                                init_file_name.c_str());

#ifdef MULTIUSER
        return "(~/.crawlrc missing)";
#else
        return "(no init.txt in current directory)";
#endif
    }

    Options.filename = init_file_name;
    Options.line_num = 0;
#ifdef MULTIUSER
    Options.basefilename = "~/.crawlrc";
#else
    Options.basefilename = "init.txt";
#endif
    read_options(f, runscript);
    fclose(f);

    Options.filename     = "extra opts last";
    Options.basefilename = "extra opts last";
    Options.line_num     = 0;
    for (unsigned int i = 0; i < SysEnv.extra_opts_last.size(); i++)
    {
        Options.line_num++;
        Options.read_option_line(SysEnv.extra_opts_last[i], false);
    }

    Options.filename     = "unknown";
    Options.basefilename = "unknown";
    Options.line_num     = -1;

    return ("");
}

newgame_def read_startup_prefs()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    std::string fn = get_prefs_filename();
    FILE *f = fopen(fn.c_str(), "r");
    if (!f)
        return newgame_def();

    game_options temp;
    FileLineInput fl(f);
    temp.read_options(fl, false);
    fclose(f);

    return (temp.game);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

#ifndef DISABLE_STICKY_STARTUP_OPTIONS
static void write_newgame_options(const newgame_def& prefs, FILE *f)
{
    if (prefs.type != NUM_GAME_TYPE)
        fprintf(f, "type = %s\n", _gametype_to_str(prefs.type).c_str());
    if (!prefs.map.empty())
        fprintf(f, "map = %s\n", prefs.map.c_str());
    if (!prefs.arena_teams.empty())
        fprintf(f, "arena_teams = %s\n", prefs.arena_teams.c_str());
    fprintf(f, "name = %s\n", prefs.name.c_str());
    if (prefs.species != SP_UNKNOWN)
        fprintf(f, "species = %s\n", _species_to_str(prefs.species).c_str());
    if (prefs.job != JOB_UNKNOWN)
        fprintf(f, "background = %s\n", _job_to_str(prefs.job).c_str());
    if (prefs.weapon != WPN_UNKNOWN)
        fprintf(f, "weapon = %s\n", _weapon_to_str(prefs.weapon).c_str());
    if (prefs.religion != GOD_NO_GOD)
        fprintf(f, "religion = %s\n", god_name(prefs.religion).c_str());
    if (prefs.book != SBT_NONE)
    {
        fprintf(f, "book = %s\n",
                prefs.book == SBT_FIRE ? "fire" :
                prefs.book == SBT_COLD ? "cold" :
                prefs.book == SBT_SUMM ? "summ" :
                prefs.book == SBT_RANDOM ? "random" :
                "viable");
    }
    if (prefs.wand != SWT_NO_SELECTION)
        fprintf(f, "wand = %s\n", _wand_to_str(prefs.wand).c_str());
    fprintf(f, "fully_random = %s\n", prefs.fully_random ? "yes" : "no");
}
#endif // !DISABLE_STICKY_STARTUP_OPTIONS

void write_newgame_options_file(const newgame_def& prefs)
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    std::string fn = get_prefs_filename();
    FILE *f = fopen(fn.c_str(), "w");
    if (!f)
        return;
    write_newgame_options(prefs, f);
    fclose(f);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

void save_player_name()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    if (!Options.remember_name)
        return ;

    // Read other preferences
    newgame_def prefs = read_startup_prefs();
    prefs.name = you.your_name;

    // And save
    write_newgame_options_file(prefs);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

void read_options(FILE *f, bool runscript)
{
    FileLineInput fl(f);
    Options.read_options(fl, runscript);
}

void read_options(const std::string &s, bool runscript, bool clear_aliases)
{
    StringLineInput st(s);
    Options.read_options(st, runscript, clear_aliases);
}

game_options::game_options()
{
    reset_options();
}

void game_options::read_options(InitLineInput &il, bool runscript,
                                bool clear_aliases)
{
    unsigned int line = 0;

    bool inscriptblock = false;
    bool inscriptcond  = false;
    bool isconditional = false;

    bool l_init        = false;

    if (clear_aliases)
        aliases.clear();

    dlua_chunk luacond(filename);
    dlua_chunk luacode(filename);

    while (!il.eof())
    {
        line_num++;
        std::string s   = il.getline();
        std::string str = s;
        line++;

        trim_string( str );

        // This is to make some efficient comments
        if ((str.empty() || str[0] == '#') && !inscriptcond && !inscriptblock)
            continue;

        if (!inscriptcond && str[0] == ':')
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            str = str.substr(1);
            if (!str.empty() && runscript)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]] )");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        if (!inscriptcond && (str.find("L<") == 0 || str.find("<") == 0))
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            inscriptcond  = true;

            str = str.substr( str.find("L<") == 0? 2 : 1 );
            // Is this a one-liner?
            if (!str.empty() && str[ str.length() - 1 ] == '>')
            {
                inscriptcond = false;
                str = str.substr(0, str.length() - 1);
            }

            if (!str.empty() && runscript)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]] )");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        else if (inscriptcond && !str.empty()
                 && (str.find(">") == str.length() - 1 || str == ">L"))
        {
            inscriptcond = false;
            str = str.substr(0, str.length() - 1);
            if (!str.empty() && runscript)
                luacond.add(line, str);
            continue;
        }
        else if (inscriptcond)
        {
            if (runscript)
                luacond.add(line, s);
            continue;
        }

        // Handle blocks of Lua
        if (!inscriptblock && (str.find("Lua{") == 0 || str.find("{") == 0))
        {
            inscriptblock = true;
            luacode.clear();
            luacode.set_file(filename);

            // Strip leading Lua[
            str = str.substr( str.find("Lua{") == 0? 4 : 1 );

            if (!str.empty() && str.find("}") == str.length() - 1)
            {
                str = str.substr(0, str.length() - 1);
                inscriptblock = false;
            }

            if (!str.empty())
                luacode.add(line, str);

            if (!inscriptblock && runscript)
            {
#ifdef CLUA_BINDINGS
                if (luacode.run(clua))
                {
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
                }
                luacode.clear();
#endif
            }

            continue;
        }
        else if (inscriptblock && (str == "}Lua" || str == "}"))
        {
            inscriptblock = false;
#ifdef CLUA_BINDINGS
            if (runscript)
            {
                if (luacode.run(clua))
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
            }
#endif
            luacode.clear();
            continue;
        }
        else if (inscriptblock)
        {
            luacode.add(line, s);
            continue;
        }

        if (isconditional && runscript)
        {
            if (!l_init)
            {
                luacond.add(line, "crawl.setopt( [[");
                l_init = true;
            }

            luacond.add(line, s);
            continue;
        }

        read_option_line(str, runscript);
    }

#ifdef CLUA_BINDINGS
    if (runscript && !luacond.empty())
    {
        if (l_init)
            luacond.add(line, "]] )");
        if (luacond.run(clua))
            mprf(MSGCH_ERROR, "Lua error: %s", luacond.orig_error().c_str());
    }
#endif

    Options.explore_stop |= Options.explore_stop_prompt;

    evil_colour = str_to_colour(variables["evil"]);
}

void game_options::fixup_options()
{
    // Validate save_dir
    if (!check_dir("Save directory", save_dir))
        end(1);

    if (!SysEnv.morgue_dir.empty())
        morgue_dir = SysEnv.morgue_dir;

    if (!check_dir("Morgue directory", morgue_dir))
        end(1);

    if (evil_colour == BLACK)
        evil_colour = MAGENTA;
}

static int _str_to_killcategory(const std::string &s)
{
    static const char *kc[] = {
        "you",
        "friend",
        "other",
    };

    for (unsigned i = 0; i < sizeof(kc) / sizeof(*kc); ++i)
        if (s == kc[i])
            return i;

    return -1;
}

void game_options::do_kill_map(const std::string &from, const std::string &to)
{
    int ifrom = _str_to_killcategory(from),
        ito   = _str_to_killcategory(to);
    if (ifrom != -1 && ito != -1)
        kill_map[ifrom] = ito;
}

int game_options::read_explore_stop_conditions(const std::string &field) const
{
    int conditions = 0;
    std::vector<std::string> stops = split_string(",", field);
    for (int i = 0, count = stops.size(); i < count; ++i)
    {
        const std::string c = replace_all_of(stops[i], " ", "_");
        if (c == "item" || c == "items")
            conditions |= ES_ITEM;
        else if (c == "greedy_pickup")
            conditions |= ES_GREEDY_PICKUP;
        else if (c == "greedy_pickup_gold")
            conditions |= ES_GREEDY_PICKUP_GOLD;
        else if (c == "greedy_pickup_smart")
            conditions |= ES_GREEDY_PICKUP_SMART;
        else if (c == "greedy_pickup_thrown")
            conditions |= ES_GREEDY_PICKUP_THROWN;
        else if (c == "shop" || c == "shops")
            conditions |= ES_SHOP;
        else if (c == "stair" || c == "stairs")
            conditions |= ES_STAIR;
        else if (c == "altar" || c == "altars")
            conditions |= ES_ALTAR;
        else if (c == "greedy_item" || c == "greedy_items")
            conditions |= ES_GREEDY_ITEM;
        else if (c == "greedy_visited_item_stack")
            conditions |= ES_GREEDY_VISITED_ITEM_STACK;
        else if (c == "glowing" || c == "glowing_item"
                 || c == "glowing_items")
            conditions |= ES_GLOWING_ITEM;
        else if (c == "artefact" || c == "artefacts"
                 || c == "artifact" || c == "artifacts")
            conditions |= ES_ARTEFACT;
        else if (c == "rune" || c == "runes")
            conditions |= ES_RUNE;
    }
    return (conditions);
}

void game_options::add_alias(const std::string &key, const std::string &val)
{
    if (key[0] == '$')
    {
        std::string name = key.substr(1);
        // Don't alter if it's a constant.
        if (constants.find(name) != constants.end())
            return;
        variables[name] = val;
    }
    else
        aliases[key] = val;
}

std::string game_options::unalias(const std::string &key) const
{
    string_map::const_iterator i = aliases.find(key);
    return (i == aliases.end()? key : i->second);
}

#define IS_VAR_CHAR(c) (isaalpha(c) || c == '_' || c == '-')

std::string game_options::expand_vars(const std::string &field) const
{
    std::string field_out = field;

    std::string::size_type curr_pos = 0;

    // Only try 100 times, so as to not get stuck in infinite recursion.
    for (int i = 0; i < 100; i++)
    {
        std::string::size_type dollar_pos = field_out.find("$", curr_pos);

        if (dollar_pos == std::string::npos
            || field_out.size() == (dollar_pos + 1))
        {
            break;
        }

        std::string::size_type start_pos = dollar_pos + 1;

        if (!IS_VAR_CHAR(field_out[start_pos]))
            continue;

        std::string::size_type end_pos;
        for (end_pos = start_pos; end_pos < field_out.size(); end_pos++)
        {
            if (!IS_VAR_CHAR(field_out[end_pos + 1]))
                break;
        }

        std::string var_name = field_out.substr(start_pos,
                                                end_pos - start_pos + 1);

        string_map::const_iterator x = variables.find(var_name);

        if (x == variables.end())
        {
            curr_pos = end_pos + 1;
            continue;
        }

        std::string dollar_plus_name = "$";
        dollar_plus_name += var_name;

        field_out = replace_all(field_out, dollar_plus_name, x->second);

        // Start over at begining
        curr_pos = 0;
    }

    return field_out;
}

void game_options::add_message_colour_mappings(const std::string &field)
{
    std::vector<std::string> fragments = split_string(",", field);
    for (int i = 0, count = fragments.size(); i < count; ++i)
        add_message_colour_mapping(fragments[i]);
}

message_filter game_options::parse_message_filter(const std::string &filter)
{
    std::string::size_type pos = filter.find(":");
    if (pos && pos != std::string::npos)
    {
        std::string prefix = filter.substr(0, pos);
        int channel = str_to_channel( prefix );
        if (channel != -1 || prefix == "any")
        {
            std::string s = filter.substr( pos + 1 );
            trim_string( s );
            return message_filter( channel, s );
        }
    }

    return message_filter( filter );
}

void game_options::add_message_colour_mapping(const std::string &field)
{
    std::vector<std::string> cmap = split_string(":", field, true, true, 1);

    if (cmap.size() != 2)
        return;

    const int col = str_to_colour(cmap[0]);
    msg_colour_type mcol;
    if (cmap[0] == "mute")
        mcol = MSGCOL_MUTED;
    else if (col == -1)
        return;
    else
        mcol = msg_colour(col);

    message_colour_mapping m = { parse_message_filter(cmap[1]), mcol };
    message_colour_mappings.push_back(m);
}

// Option syntax is:
// sort_menu = [menu_type:]yes|no|auto:n[:sort_conditions]
void game_options::set_menu_sort(std::string field)
{
    if (field.empty())
        return;

    menu_sort_condition cond(field);

    // Overrides all previous settings.
    if (cond.mtype == MT_ANY)
        sort_menus.clear();

    // Override existing values, if necessary.
    for (unsigned int i = 0; i < sort_menus.size(); i++)
        if (sort_menus[i].mtype == cond.mtype)
        {
            sort_menus[i].sort = cond.sort;
            sort_menus[i].cmp  = cond.cmp;
            return;
        }

    sort_menus.push_back(cond);
}

void game_options::split_parse(
    const std::string &s, const std::string &separator,
    void (game_options::*add)(const std::string &))
{
    const std::vector<std::string> defs = split_string(separator, s);
    for (int i = 0, size = defs.size(); i < size; ++i)
        (this->*add)( defs[i] );
}

void game_options::set_option_fragment(const std::string &s)
{
    if (s.empty())
        return;

    std::string::size_type st = s.find(':');
    if (st == std::string::npos)
    {
        // Boolean option.
        if (s[0] == '!')
            read_option_line(s.substr(1) + " = false");
        else
            read_option_line(s + " = true");
    }
    else
    {
        // key:val option.
        read_option_line(s.substr(0, st) + " = " + s.substr(st + 1));
    }
}

// Not a method of the game_options class since keybindings aren't
// stored in that class.
static void _bindkey(std::string field)
{
    const size_t start_bracket = field.find_first_of('[');
    const size_t end_bracket   = field.find_last_of(']');

    if (start_bracket == std::string::npos
        || end_bracket == std::string::npos
        || start_bracket > end_bracket)
    {
        mprf(MSGCH_ERROR, "Bad bindkey bracketing in '%s'",
             field.c_str());
        return;
    }

    const std::string key_str = field.substr(start_bracket + 1,
                                             end_bracket - start_bracket - 1);

    int key;

    // TODO: Function keys.
    if (key_str.length() == 0)
    {
        mprf(MSGCH_ERROR, "No key in bindkey directive '%s'",
             field.c_str());
        return;
    }
    else if (key_str.length() == 1)
    {
        key = key_str[0];
    }
    else if (key_str.length() == 2)
    {
        if (key_str[0] != '^')
        {
            mprf(MSGCH_ERROR, "Invalid key '%s' in bindkey directive '%s'",
                 key_str.c_str(), field.c_str());
            return;
        }
        key = CONTROL(key_str[1]);
    }
    else
    {
        mprf(MSGCH_ERROR, "Invalid key '%s' in bindkey directive '%s'",
             key_str.c_str(), field.c_str());
        return;
    }

    const size_t start_name = field.find_first_not_of(' ', end_bracket + 1);
    if (start_name == std::string::npos)
    {
        mprf(MSGCH_ERROR, "No command name for bindkey directive '%s'",
             field.c_str());
        return;
    }

    const std::string  name = field.substr(start_name);
    const command_type cmd  = name_to_command(name);
    if (cmd == CMD_NO_CMD)
    {
       mprf(MSGCH_ERROR, "No command named '%s'", name.c_str());
       return;
    }

    bind_command_to_key(cmd, key);
}

void game_options::read_option_line(const std::string &str, bool runscript)
{
#define BOOL_OPTION_NAMED(_opt_str, _opt_var)               \
    if (key == _opt_str) do {                               \
        this->_opt_var = _read_bool(field, this->_opt_var); \
    } while (false)
#define BOOL_OPTION(_opt) BOOL_OPTION_NAMED(#_opt, _opt)

#define COLOUR_OPTION_NAMED(_opt_str, _opt_var)                         \
    if (key == _opt_str) do {                                           \
        const int col = str_to_colour( field );                         \
        if (col != -1) {                                                \
            this->_opt_var = col;                                       \
        } else {                                                        \
            /*fprintf( stderr, "Bad %s -- %s\n", key, field.c_str() );*/ \
            report_error (                                              \
                make_stringf("Bad %s -- %s\n",                          \
                    key.c_str(), field.c_str()));                       \
        }                                                               \
    } while (false)
#define COLOUR_OPTION(_opt) COLOUR_OPTION_NAMED(#_opt, _opt)

#define CURSES_OPTION_NAMED(_opt_str, _opt_var)     \
    if (key == _opt_str) do {                       \
        this->_opt_var = curses_attribute(field);   \
    } while (false)
#define CURSES_OPTION(_opt) CURSES_OPTION_NAMED(#_opt, _opt)

#define INT_OPTION_NAMED(_opt_str, _opt_var, _min_val, _max_val)        \
    if (key == _opt_str) do {                                           \
        const int min_val = (_min_val);                                 \
        const int max_val = (_max_val);                                 \
        int val = atoi(field.c_str());                                  \
        if (val < min_val) {                                            \
            report_error (                                              \
                make_stringf("Bad %s: %d < %d", _opt_str, val, min_val)); \
            val = min_val;                                              \
        } else if (val > max_val) {                                     \
            report_error (                                              \
                make_stringf("Bad %s: %d > %d", _opt_str, val, max_val)); \
            val = max_val;                                              \
        }                                                               \
        this->_opt_var = val;                                           \
    } while (false)
#define INT_OPTION(_opt, _min_val, _max_val) \
    INT_OPTION_NAMED(#_opt, _opt, _min_val, _max_val)

    std::string key    = "";
    std::string subkey = "";
    std::string field  = "";

    bool plus_equal  = false;
    bool minus_equal = false;

    const int first_equals = str.find('=');

    // all lines with no equal-signs we ignore
    if (first_equals < 0)
        return;

    field = str.substr( first_equals + 1 );
    field = expand_vars(field);

    std::string prequal = trimmed_string( str.substr(0, first_equals) );

    // Is this a case of key += val?
    if (prequal.length() && prequal[prequal.length() - 1] == '+')
    {
        plus_equal = true;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == '-')
    {
        minus_equal = true;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == ':')
    {
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
        trim_string(field);

        add_alias(prequal, field);
        return;
    }

    prequal = unalias(prequal);

    const std::string::size_type first_dot = prequal.find('.');
    if (first_dot != std::string::npos)
    {
        key    = prequal.substr( 0, first_dot );
        subkey = prequal.substr( first_dot + 1 );
    }
    else
    {
        // no subkey (dots are okay in value field)
        key    = prequal;
    }

    // Clean up our data...
    lowercase( trim_string( key ) );
    lowercase( trim_string( subkey ) );

    // some fields want capitals... none care about external spaces
    trim_string( field );

    // Keep unlowercased field around
    const std::string orig_field = field;

    if (key != "name" && key != "crawl_dir" && key != "macro_dir"
        && key != "species" && key != "background" && key != "job"
        && key != "race" && key != "class" && key != "ban_pickup"
        && key != "autopickup_exceptions"
        && key != "explore_stop_pickup_ignore"
        && key != "stop_travel" && key != "sound"
        && key != "force_more_message"
        && key != "drop_filter" && key != "lua_file" && key != "terp_file"
        && key != "note_items" && key != "autoinscribe"
        && key != "note_monsters" && key != "note_messages"
        && key.find("cset") != 0 && key != "dungeon"
        && key != "feature" && key != "fire_items_start"
        && key != "mon_glyph" && key != "opt" && key != "option"
        && key != "menu_colour" && key != "menu_color"
        && key != "message_colour" && key != "message_color"
        && key != "levels" && key != "level" && key != "entries"
        && key != "include" && key != "bindkey"
        && key.find("font") == std::string::npos)
    {
        lowercase( field );
    }

    if (key == "include")
    {
        include(field, true, runscript);
    }
    else if (key == "opt" || key == "option")
    {
        split_parse(field, ",", &game_options::set_option_fragment);
    }
    else if (key == "autopickup")
    {
        // clear out autopickup
        autopickups = 0L;

        for (size_t i = 0; i < field.length(); i++)
        {
            char type = field[i];

            // Make the amulet symbol equiv to ring -- bwross
            switch (type)
            {
            case '"':
                // also represents jewellery
                type = '=';
                break;

            case '|':
                // also represents staves
                type = '\\';
                break;

            case ':':
                // also represents books
                type = '+';
                break;

            case '&':
            case 'x':
                // also corpses
                type = 'X';
                break;
            }

            int j;
            for (j = 0; j < obj_syms_len && type != obj_syms[j]; j++)
                ;

            if (j < obj_syms_len)
                autopickups |= (1L << j);
            else
            {
                report_error (
                    make_stringf("Bad object type '%c' for autopickup.\n",
                                 type));
            }
        }
    }
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else if (key == "name")
    {
        // field is already cleaned up from trim_string()
        game.name = field;
    }
#endif
#ifndef USE_TILE
    else if (key == "char_set" || key == "ascii_display")
    {
        bool valid = true;

        if (key == "ascii_display")
        {
            char_set =
                _read_bool(field, char_set == CSET_ASCII)?
                    CSET_ASCII
                  : CSET_IBM;
            valid = true;
        }
        else
        {
            if (field == "ascii")
                char_set = CSET_ASCII;
            else if (field == "ibm")
                char_set = CSET_IBM;
            else if (field == "dec")
                char_set = CSET_DEC;
            else if (field == "utf" || field == "unicode")
                char_set = CSET_UNICODE;
            else
            {
                fprintf( stderr, "Bad character set: %s\n", field.c_str() );
                valid = false;
            }
        }
    }
#endif
    else if (key == "default_autopickup")
    {
        if (_read_bool(field, true))
            autopickup_on = 1;
        else
            autopickup_on = 0;
    }
    else if (key == "default_friendly_pickup")
    {
        if (field == "none")
            default_friendly_pickup = FRIENDLY_PICKUP_NONE;
        else if (field == "friend")
            default_friendly_pickup = FRIENDLY_PICKUP_FRIEND;
        else if (field == "player")
            default_friendly_pickup = FRIENDLY_PICKUP_PLAYER;
        else if (field == "all")
            default_friendly_pickup = FRIENDLY_PICKUP_ALL;
    }
    else BOOL_OPTION(show_inventory_weights);
    else BOOL_OPTION(suppress_startup_errors);
    else BOOL_OPTION(clean_map);
    else BOOL_OPTION(colour_map);
    else BOOL_OPTION_NAMED("color_map", colour_map);  // common misspelling :)
    else if (key == "easy_confirm")
    {
        // allows both 'Y'/'N' and 'y'/'n' on yesno() prompts
        if (field == "none")
            easy_confirm = CONFIRM_NONE_EASY;
        else if (field == "safe")
            easy_confirm = CONFIRM_SAFE_EASY;
    }
    else if (key == "allow_self_target")
    {
        if (field == "yes")
            allow_self_target = CONFIRM_NONE;
        else if (field == "no")
            allow_self_target = CONFIRM_CANCEL;
        else if (field == "prompt")
            allow_self_target = CONFIRM_PROMPT;
    }
    else BOOL_OPTION(easy_quit_item_prompts);
    else BOOL_OPTION_NAMED("easy_quit_item_lists", easy_quit_item_prompts);
    else BOOL_OPTION(easy_open);
    else BOOL_OPTION(easy_unequip);
    else BOOL_OPTION(equip_unequip);
    else BOOL_OPTION_NAMED("easy_armour", easy_unequip);
    else BOOL_OPTION_NAMED("easy_armor", easy_unequip);
    else BOOL_OPTION(easy_butcher);
    else BOOL_OPTION(always_confirm_butcher);
    else BOOL_OPTION(chunks_autopickup);
    else BOOL_OPTION(prompt_for_swap);
    else BOOL_OPTION(list_rotten);
    else BOOL_OPTION(prefer_safe_chunks);
    else BOOL_OPTION(easy_eat_chunks);
    else BOOL_OPTION(easy_eat_gourmand);
    else BOOL_OPTION(easy_eat_contaminated);
    else if (key == "lua_file" && runscript)
    {
#ifdef CLUA_BINDINGS
        clua.execfile(field.c_str(), false, false);
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
#endif
    }
    else if (key == "terp_file" && runscript)
    {
#ifdef WIZARD
        terp_files.push_back(field);
#endif
    }
    else if (key == "colour" || key == "color")
    {
        const int orig_col   = str_to_colour( subkey );
        const int result_col = str_to_colour( field );

        if (orig_col != -1 && result_col != -1)
            colour[orig_col] = result_col;
        else
        {
            fprintf( stderr, "Bad colour -- %s=%d or %s=%d\n",
                     subkey.c_str(), orig_col, field.c_str(), result_col );
        }
    }
    else if (key == "channel")
    {
        const int chnl = str_to_channel( subkey );
        const msg_colour_type col  = _str_to_channel_colour( field );

        if (chnl != -1 && col != -1)
            channels[chnl] = col;
        else if (chnl == -1)
            fprintf( stderr, "Bad channel -- %s\n", subkey.c_str() );
        else if (col == MSGCOL_NONE)
            fprintf( stderr, "Bad colour -- %s\n", field.c_str() );
    }
    else COLOUR_OPTION(background_colour);
    else COLOUR_OPTION(detected_item_colour);
    else COLOUR_OPTION(detected_monster_colour);
    else if (key.find(interrupt_prefix) == 0)
    {
        set_activity_interrupt(key.substr(interrupt_prefix.length()),
                               field,
                               plus_equal,
                               minus_equal);
    }
    else if (key.find("cset") == 0)
    {
        std::string cset = key.substr(4);
        if (!cset.empty() && cset[0] == '_')
            cset = cset.substr(1);

        char_set_type cs = NUM_CSET;
        if (cset == "ascii")
            cs = CSET_ASCII;
        else if (cset == "ibm")
            cs = CSET_IBM;
        else if (cset == "dec")
            cs = CSET_DEC;
        else if (cset == "utf" || cset == "unicode")
            cs = CSET_UNICODE;

        add_cset_override(cs, field);
    }
    else if (key == "feature" || key == "dungeon")
    {
        split_parse(field, ";", &game_options::add_feature_override);
    }
    else if (key == "mon_glyph")
    {
        split_parse(field, ",", &game_options::add_mon_glyph_override);
    }
    else CURSES_OPTION(friend_brand);
    else CURSES_OPTION(neutral_brand);
    else CURSES_OPTION(stab_brand);
    else CURSES_OPTION(may_stab_brand);
    else CURSES_OPTION(feature_item_brand);
    else CURSES_OPTION(trap_item_brand);
    // This is useful for terms where dark grey does
    // not have standout modes (since it's black on black).
    // This option will use light-grey instead in these cases.
    else BOOL_OPTION(no_dark_brand);
    // no_dark_brand applies here as well.
    else CURSES_OPTION(heap_brand);
    else COLOUR_OPTION(status_caption_colour);
    else if (key == "map")
    {
        game.map = field;
    }
    else if (key == "arena_teams")
    {
        game.arena_teams = field;
    }
    else if (key == "type")
    {
        game.type = _str_to_gametype(field);
    }
    else if (key == "species" || key == "race")
    {
        game.species = _str_to_species(field);
    }
    else if (key == "background" || key == "job" || key == "class")
    {
        game.job = _str_to_job( field );
    }
    else if (key == "weapon")
    {
        // Choose this weapon for backgrounds that get choice.
        game.weapon = _str_to_weapon( field );
    }
    else if (key == "book")
    {
        // Choose this book for backgrounds that get choice.
        game.book = _str_to_book( field );
    }
    else if (key == "wand")
    {
        // Choose this wand for backgrounds that get choice.
        game.wand = _str_to_wand( field );
    }
    else if (key == "religion")
    {
        // Choose god for Chaos Knights or Priests.
        game.religion = str_to_god(field);
    }
    BOOL_OPTION_NAMED("fully_random", game.fully_random);
    else if (key == "fire_items_start")
    {
        if (isaalpha( field[0] ))
            fire_items_start = letter_to_index( field[0] );
        else
        {
            fprintf( stderr, "Bad fire item start index: %s\n",
                     field.c_str() );
        }
    }
    else if (key == "assign_item_slot")
    {
        if (field == "forward")
            assign_item_slot = SS_FORWARD;
        else if (field == "backward")
            assign_item_slot = SS_BACKWARD;
    }
    else if (key == "fire_order")
    {
        set_fire_order(field, plus_equal);
    }
    else if (key == "pizza")
    {
        // field is already cleaned up from trim_string()
        pizza = field;
    }
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else BOOL_OPTION(remember_name);
#endif
#ifndef DGAMELAUNCH
#ifndef SAVE_DIR_PATH
    else if (key == "save_dir")
    {
        // If SAVE_DIR_PATH was defined, there are very likely security issues
        // with allowing the user to specify a different directory.
        save_dir = field;
    }
#endif
#ifndef SAVE_DIR_PATH
    else if (key == "morgue_dir")
    {
        morgue_dir = field;
    }
#endif
#endif
    else BOOL_OPTION(show_gold_turns);
#ifndef USE_TILE
    else BOOL_OPTION(show_beam);
#endif
    else if (key == "hp_warning")
    {
        hp_warning = atoi( field.c_str() );
        if (hp_warning < 0 || hp_warning > 100)
        {
            hp_warning = 0;
            fprintf( stderr, "Bad HP warning percentage -- %s\n",
                     field.c_str() );
        }
    }
    else if (key == "mp_warning")
    {
        magic_point_warning = atoi( field.c_str() );
        if (magic_point_warning < 0 || magic_point_warning > 100)
        {
            magic_point_warning = 0;
            fprintf( stderr, "Bad MP warning percentage -- %s\n",
                     field.c_str() );
        }
    }
    else INT_OPTION(ood_interesting, 0, 500);
    else INT_OPTION(rare_interesting, 0, 99);
    else if (key == "note_monsters")
    {
        append_vector(note_monsters, split_string(",", field));
    }
    else if (key == "note_messages")
    {
        append_vector(note_messages, split_string(",", field));
    }
    else if (key == "note_hp_percent")
    {
        note_hp_percent = atoi( field.c_str() );
        if (note_hp_percent < 0 || note_hp_percent > 100)
        {
            note_hp_percent = 0;
            fprintf( stderr, "Bad HP note percentage -- %s\n",
                     field.c_str() );
        }
    }
#ifndef DGAMELAUNCH
    // If DATA_DIR_PATH is set, don't set crawl_dir from .crawlrc.
#ifndef DATA_DIR_PATH
    else if (key == "crawl_dir")
    {
        // We shouldn't bother to allocate this a second time
        // if the user puts two crawl_dir lines in the init file.
        SysEnv.crawl_dir = field;
    }
    else if (key == "macro_dir")
    {
        macro_dir = field;
    }
#endif
#endif
    else BOOL_OPTION(auto_list);
    else if (key == "default_target")
    {
        default_target = _read_bool( field, default_target );
        if (default_target)
            target_unshifted_dirs = false;
    }
    else BOOL_OPTION(autopickup_no_burden);
#ifdef DGL_SIMPLE_MESSAGING
    else BOOL_OPTION(messaging);
#endif
    else BOOL_OPTION(mouse_input);
    // These need to be odd, hence allow +1.
    else INT_OPTION(view_max_width, VIEW_MIN_WIDTH, GXM + 1);
    else INT_OPTION(view_max_height, VIEW_MIN_HEIGHT, GYM + 1);
    else INT_OPTION(mlist_min_height, 0, INT_MAX);
    else INT_OPTION(msg_min_height, MSG_MIN_HEIGHT, INT_MAX);
    else INT_OPTION(msg_max_height, MSG_MIN_HEIGHT, INT_MAX);
    else BOOL_OPTION(mlist_allow_alternate_layout);
    else BOOL_OPTION(messages_at_top);
#ifndef USE_TILE
    else BOOL_OPTION(mlist_targeting);
#endif
    else BOOL_OPTION(classic_hud);
    else BOOL_OPTION(msg_condense_repeats);
    else BOOL_OPTION(msg_condense_short);
    else BOOL_OPTION(view_lock_x);
    else BOOL_OPTION(view_lock_y);
    else if (key == "view_lock")
    {
        const bool lock = _read_bool(field, true);
        view_lock_x = view_lock_y = lock;
    }
    else BOOL_OPTION(center_on_scroll);
    else BOOL_OPTION(symmetric_scroll);
    else BOOL_OPTION(verbose_monster_pane);
    else if (key == "scroll_margin_x")
    {
        scroll_margin_x = atoi(field.c_str());
        if (scroll_margin_x < 0)
            scroll_margin_x = 0;
    }
    else if (key == "scroll_margin_y")
    {
        scroll_margin_y = atoi(field.c_str());
        if (scroll_margin_y < 0)
            scroll_margin_y = 0;
    }
    else if (key == "scroll_margin")
    {
        int scrollmarg = atoi(field.c_str());
        if (scrollmarg < 0)
            scrollmarg = 0;
        scroll_margin_x = scroll_margin_y = scrollmarg;
    }
    else if (key == "user_note_prefix")
    {
        // field is already cleaned up from trim_string()
        user_note_prefix = field;
    }
    else BOOL_OPTION(note_all_skill_levels);
    else BOOL_OPTION(note_skill_max);
    else BOOL_OPTION(note_all_spells);
    else BOOL_OPTION(note_xom_effects);
    else BOOL_OPTION(clear_messages);
    else BOOL_OPTION(show_more);
    else BOOL_OPTION(small_more);
    else if (key == "flush")
    {
        if (subkey == "failure")
        {
            flush_input[FLUSH_ON_FAILURE]
                = _read_bool(field, flush_input[FLUSH_ON_FAILURE]);
        }
        else if (subkey == "command")
        {
            flush_input[FLUSH_BEFORE_COMMAND]
                = _read_bool(field, flush_input[FLUSH_BEFORE_COMMAND]);
        }
        else if (subkey == "message")
        {
            flush_input[FLUSH_ON_MESSAGE]
                = _read_bool(field, flush_input[FLUSH_ON_MESSAGE]);
        }
        else if (subkey == "lua")
        {
            flush_input[FLUSH_LUA]
                = _read_bool(field, flush_input[FLUSH_LUA]);
        }
    }
    else if (key == "wiz_mode")
    {
        // wiz_mode is recognised as a legal key in all compiles -- bwr
#ifdef WIZARD
        if (field == "never")
            wiz_mode = WIZ_NEVER;
        else if (field == "no")
            wiz_mode = WIZ_NO;
        else if (field == "yes")
            wiz_mode = WIZ_YES;
        else
        {
            report_error (
                make_stringf("Unknown wiz_mode option: %s\n", field.c_str()));
        }
#endif
    }
    else if (key == "ban_pickup")
    {
        std::vector<std::string> args = split_string(",", field);
        for (int i = 0, size = args.size(); i < size; ++i)
        {
            const std::string &s = args[i];
            if (s.empty())
                continue;
            force_autopickup.push_back(std::make_pair(s, false));
        }
    }
    else if (key == "autopickup_exceptions")
    {
        std::vector<std::string> args = split_string(",", field);
        for (int i = 0, size = args.size(); i < size; ++i)
        {
            const std::string &s = args[i];
            if (s.empty())
                continue;

            if (s[0] == '>')
                force_autopickup.push_back(std::make_pair(s.substr(1), false));
            else if (s[0] == '<')
                force_autopickup.push_back(std::make_pair(s.substr(1), true));
            else
                force_autopickup.push_back(std::make_pair(s, false));
        }
    }
    else if (key == "note_items")
    {
        append_vector(note_items, split_string(",", field));
    }

#ifndef _MSC_VER
	// break if-else chain on broken Microsoft compilers with stupid nesting limits
	else
#endif

    if (key == "autoinscribe")
    {
        if (field.empty())
        {
            report_error("Autoinscirbe string is empty");
            return;
        }

        const size_t first = field.find_first_of(':');
        const size_t last  = field.find_last_of(':');
        if (first == std::string::npos || first != last)
        {
            report_error (
                make_stringf("Autoinscribe string must have exactly "
                             "one colon: %s\n", field.c_str()));
            return;
        }

        if (first == 0)
        {
            report_error (
                make_stringf("Autoinscribe pattern is empty: %s\n",
                             field.c_str()));
            return;
        }

        if (last == field.length() - 1)
        {
            report_error (
                make_stringf("Autoinscribe result is empty: %s\n",
                             field.c_str()));
            return;
        }

        std::vector<std::string> thesplit = split_string(":", field);

        if (thesplit.size() != 2)
        {
            report_error (
                make_stringf("Error parsing autoinscribe string: %s\n",
                             field.c_str()));
            return;
        }

        autoinscriptions.push_back(
            std::pair<text_pattern,std::string>(thesplit[0], thesplit[1]));
    }
    else BOOL_OPTION(autoinscribe_artefacts);
    else BOOL_OPTION(autoinscribe_cursed);
#ifndef DGAMELAUNCH
    else if (key == "map_file_name")
    {
        map_file_name = field;
    }
#endif
    else if (key == "hp_colour" || key == "hp_color")
    {
        hp_colour.clear();
        std::vector<std::string> thesplit = split_string(",", field);
        for ( unsigned i = 0; i < thesplit.size(); ++i )
        {
            std::vector<std::string> insplit = split_string(":", thesplit[i]);
            int hp_percent = 100;

            if ( insplit.size() == 0 || insplit.size() > 2
                 || insplit.size() == 1 && i != 0 )
            {
                report_error (
                    make_stringf("Bad hp_colour string: %s\n", field.c_str()));
                break;
            }

            if ( insplit.size() == 2 )
                hp_percent = atoi(insplit[0].c_str());

            int scolour = str_to_colour(insplit[(insplit.size() == 1) ? 0 : 1]);
            hp_colour.push_back(std::pair<int, int>(hp_percent, scolour));
        }
    }
    else if (key == "mp_color" || key == "mp_colour")
    {
        mp_colour.clear();
        std::vector<std::string> thesplit = split_string(",", field);
        for ( unsigned i = 0; i < thesplit.size(); ++i )
        {
            std::vector<std::string> insplit = split_string(":", thesplit[i]);
            int mp_percent = 100;

            if ( insplit.size() == 0 || insplit.size() > 2
                 || insplit.size() == 1 && i != 0 )
            {
                report_error (
                    make_stringf("Bad mp_colour string: %s\n", field.c_str()));
                break;
            }

            if ( insplit.size() == 2 )
                mp_percent = atoi(insplit[0].c_str());

            int scolour = str_to_colour(insplit[(insplit.size() == 1) ? 0 : 1]);
            mp_colour.push_back(std::pair<int, int>(mp_percent, scolour));
        }
    }
    else if (key == "stat_colour" || key == "stat_color")
    {
        stat_colour.clear();
        std::vector<std::string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            std::vector<std::string> insplit = split_string(":", thesplit[i]);

            if (insplit.size() == 0 || insplit.size() > 2
                || insplit.size() == 1 && i != 0)
            {
                report_error (
                    make_stringf("Bad stat_colour string: %s\n",
                                 field.c_str()));
                break;
            }

            int stat_limit = 1;
            if (insplit.size() == 2 )
                stat_limit = atoi(insplit[0].c_str());

            int scolour = str_to_colour(insplit[(insplit.size() == 1) ? 0 : 1]);
            stat_colour.push_back(std::pair<int, int>(stat_limit, scolour));
        }
    }
    else if (key == "note_skill_levels")
    {
        std::vector<std::string> thesplit = split_string(",", field);
        for ( unsigned i = 0; i < thesplit.size(); ++i )
        {
            int num = atoi(thesplit[i].c_str());
            if ( num > 0 && num <= 27 )
                note_skill_levels.push_back(num);
            else
            {
                report_error (
                    make_stringf("Bad skill level to note -- %s\n",
                                 thesplit[i].c_str()));
                continue;
            }
        }
    }
    else BOOL_OPTION(pickup_thrown);
    else BOOL_OPTION(pickup_dropped);
#ifdef WIZARD
    else if (key == "fsim_kit")
    {
        append_vector(fsim_kit, split_string(",", field));
    }
    else if (key == "fsim_rounds")
    {
        fsim_rounds = atol(field.c_str());
        if (fsim_rounds < 1000)
            fsim_rounds = 1000;
        if (fsim_rounds > 500000L)
            fsim_rounds = 500000L;
    }
    else if (key == "fsim_mons")
    {
        fsim_mons = field;
    }
    else if (key == "fsim_str")
    {
        fsim_str = atoi(field.c_str());
    }
    else if (key == "fsim_int")
    {
        fsim_int = atoi(field.c_str());
    }
    else if (key == "fsim_dex")
    {
        fsim_dex = atoi(field.c_str());
    }
    else if (key == "fsim_xl")
    {
        fsim_xl = atoi(field.c_str());
    }
#endif // WIZARD
    else if (key == "sort_menus")
    {
        std::vector<std::string> frags = split_string(";", field);
        for (int i = 0, size = frags.size(); i < size; ++i)
        {
            if (frags[i].empty())
                continue;
            set_menu_sort(frags[i]);
        }
    }
    else if (key == "travel_delay")
    {
        // Read travel delay in milliseconds.
        travel_delay = atoi( field.c_str() );
        if (travel_delay < -1)
            travel_delay = -1;
        if (travel_delay > 2000)
            travel_delay = 2000;
    }
    else if (key == "explore_delay")
    {
        // Read explore delay in milliseconds.
        explore_delay = atoi( field.c_str() );
        if (explore_delay < -1)
            explore_delay = -1;
        if (explore_delay > 2000)
            explore_delay = 2000;
    }
    else if (key == "level_map_cursor_step")
    {
        level_map_cursor_step = atoi( field.c_str() );
        if (level_map_cursor_step < 1)
            level_map_cursor_step = 1;
        if (level_map_cursor_step > 50)
            level_map_cursor_step = 50;
    }
    else BOOL_OPTION(use_fake_cursor);
    else BOOL_OPTION(use_fake_player_cursor);
    else BOOL_OPTION(macro_meta_entry);
    else if (key == "force_more_message")
    {
        std::vector<std::string> fragments = split_string(",", field);
        for (int i = 0, count = fragments.size(); i < count; ++i)
        {
            if (fragments[i].length() == 0)
                continue;

            std::string::size_type pos = fragments[i].find(":");
            if (pos && pos != std::string::npos)
            {
                std::string prefix = fragments[i].substr(0, pos);
                int channel = str_to_channel( prefix );
                if (channel != -1 || prefix == "any")
                {
                    std::string s = fragments[i].substr( pos + 1 );
                    trim_string( s );
                    force_more_message.push_back(
                        message_filter( channel, s ) );
                    continue;
                }
            }

            force_more_message.push_back(
                    message_filter( fragments[i] ) );
        }
    }
    else if (key == "drop_filter")
    {
        append_vector(drop_filter, split_string(",", field));
    }
    else if (key == "travel_avoid_terrain")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
            prevent_travel_to( seg[i] );
    }
    else if (key == "tc_reachable")
    {
        tc_reachable = str_to_colour(field, tc_reachable);
    }
    else if (key == "tc_excluded")
    {
        tc_excluded = str_to_colour(field, tc_excluded);
    }
    else if (key == "tc_exclude_circle")
    {
        tc_exclude_circle =
            str_to_colour(field, tc_exclude_circle);
    }
    else if (key == "tc_dangerous")
    {
        tc_dangerous = str_to_colour(field, tc_dangerous);
    }
    else if (key == "tc_disconnected")
    {
        tc_disconnected = str_to_colour(field, tc_disconnected);
    }
    else if (key == "auto_exclude")
    {
        append_vector(auto_exclude, split_string(",", field));
    }
    else BOOL_OPTION(classic_item_colours);
    else BOOL_OPTION(item_colour);
    else BOOL_OPTION_NAMED("item_color", item_colour);
    else BOOL_OPTION(easy_exit_menu);
    else BOOL_OPTION(dos_use_background_intensity);
    else if (key == "item_stack_summary_minimum")
    {
        item_stack_summary_minimum = atoi(field.c_str());
    }
    else if (key == "explore_stop")
    {
        if (!plus_equal && !minus_equal)
            explore_stop = ES_NONE;

        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop &= ~new_conditions;
        else
            explore_stop |= new_conditions;
    }
    else if (key == "explore_stop_prompt")
    {
        if (!plus_equal && !minus_equal)
            explore_stop_prompt = ES_NONE;
        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop_prompt &= ~new_conditions;
        else
            explore_stop_prompt |= new_conditions;
    }
    else if (key == "explore_stop_pickup_ignore")
    {
        append_vector(explore_stop_pickup_ignore, split_string(",", field));
    }
    else if (key == "explore_item_greed")
    {
        explore_item_greed = atoi( field.c_str() );
        if (explore_item_greed > 1000)
            explore_item_greed = 1000;
        else if (explore_item_greed < -1000)
            explore_item_greed = -1000;
    }
    else BOOL_OPTION(explore_greedy);
    else BOOL_OPTION(explore_improved);
    else BOOL_OPTION(trap_prompt);
    else if (key == "stash_filter")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
            Stash::filter( seg[i] );
    }
    else if (key == "sound")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const std::string &sub = seg[i];
            std::string::size_type cpos = sub.find(":", 0);
            if (cpos != std::string::npos)
            {
                sound_mapping mapping;
                mapping.pattern = sub.substr(0, cpos);
                mapping.soundfile = sub.substr(cpos + 1);
                sound_mappings.push_back(mapping);
            }
        }
    }
#ifndef TARGET_COMPILER_VC
    // MSVC has a limit on how many if/else if can be chained together.
    else
#endif
    if (key == "menu_colour" || key == "menu_color")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            // Format is "tag:colour:pattern" or "colour:pattern" (default tag).
            // FIXME: arrange so that you can use ':' inside a pattern
            std::vector<std::string> subseg = split_string(":", seg[i], false);
            std::string tagname, patname, colname;
            if (subseg.size() < 2)
                continue;
            if (subseg.size() >= 3)
            {
                tagname = subseg[0];
                colname = subseg[1];
                patname = subseg[2];
            }
            else
            {
                colname = subseg[0];
                patname = subseg[1];
            }

            colour_mapping mapping;
            mapping.tag     = tagname;
            mapping.pattern = patname;
            mapping.colour  = str_to_colour(colname);

            if (mapping.colour != -1)
                menu_colour_mappings.push_back(mapping);
        }
    }
    else BOOL_OPTION(menu_colour_prefix_class);
    else BOOL_OPTION_NAMED("menu_color_prefix_class", menu_colour_prefix_class);
    else BOOL_OPTION(menu_colour_shops);
    else BOOL_OPTION_NAMED("menu_color_shops", menu_colour_shops);
    else if (key == "message_colour" || key == "message_color")
    {
        add_message_colour_mappings(field);
    }
    else if (key == "dump_order")
    {
        if (!plus_equal)
            dump_order.clear();

        new_dump_fields(field, !minus_equal);
    }
    else if (key == "dump_kill_places")
    {
        dump_kill_places = (field == "none" ? KDO_NO_PLACES :
                            field == "all"  ? KDO_ALL_PLACES
                                            : KDO_ONE_PLACE);
    }
    else if (key == "kill_map")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const std::string &s = seg[i];
            std::string::size_type cpos = s.find(":", 0);
            if (cpos != std::string::npos)
            {
                std::string from = s.substr(0, cpos);
                std::string to   = s.substr(cpos + 1);
                do_kill_map(from, to);
            }
        }
    }
    else BOOL_OPTION(rest_wait_both);
    else if (key == "dump_message_count")
    {
        // Capping is implicit
        dump_message_count = atoi( field.c_str() );
    }
    else if (key == "dump_item_origins")
    {
        dump_item_origins = IODS_PRICE;
        std::vector<std::string> choices = split_string(",", field);
        for (int i = 0, count = choices.size(); i < count; ++i)
        {
            const std::string &ch = choices[i];
            if (ch == "artefacts" || ch == "artifacts"
                || ch == "artefact" || ch == "artifact")
            {
                dump_item_origins |= IODS_ARTEFACTS;
            }
            else if (ch == "ego_arm" || ch == "ego armour"
                     || ch == "ego_armour")
            {
                dump_item_origins |= IODS_EGO_ARMOUR;
            }
            else if (ch == "ego_weap" || ch == "ego weapon"
                     || ch == "ego_weapon" || ch == "ego weapons"
                     || ch == "ego_weapons")
            {
                dump_item_origins |= IODS_EGO_WEAPON;
            }
            else if (ch == "jewellery" || ch == "jewelry")
                dump_item_origins |= IODS_JEWELLERY;
            else if (ch == "runes")
                dump_item_origins |= IODS_RUNES;
            else if (ch == "rods")
                dump_item_origins |= IODS_RODS;
            else if (ch == "staves")
                dump_item_origins |= IODS_STAVES;
            else if (ch == "books")
                dump_item_origins |= IODS_BOOKS;
            else if (ch == "all" || ch == "everything")
                dump_item_origins = IODS_EVERYTHING;
        }
    }
    else if (key == "dump_item_origin_price")
    {
        dump_item_origin_price = atoi( field.c_str() );
        if (dump_item_origin_price < -1)
            dump_item_origin_price = -1;
    }
    else BOOL_OPTION(dump_book_spells);
    else BOOL_OPTION(level_map_title);
    else if (key == "target_unshifted_dirs")
    {
        target_unshifted_dirs = _read_bool(field, target_unshifted_dirs);
        if (target_unshifted_dirs)
            default_target = false;
    }
    else if (key == "darken_beyond_range")
    {
        darken_beyond_range = _read_bool(field, darken_beyond_range);
    }
    else if (key == "drop_mode")
    {
        if (field.find("multi") != std::string::npos)
            drop_mode = DM_MULTI;
        else
            drop_mode = DM_SINGLE;
    }
    else if (key == "pickup_mode")
    {
        if (field.find("multi") != std::string::npos)
            pickup_mode = 0;
        else if (field.find("single") != std::string::npos)
            pickup_mode = -1;
        else
            pickup_mode = _read_bool_or_number(field, pickup_mode, "auto:");
    }
    else if (key == "additional_macro_file")
    {
        const std::string resolved = resolve_include(orig_field, "macro ");
        if (!resolved.empty())
            additional_macro_files.push_back(resolved);
    }
#ifdef USE_TILE
    else if (key == "tile_show_items")
    {
        strncpy(tile_show_items, field.c_str(), 18);
    }
    else BOOL_OPTION(tile_title_screen);
    else BOOL_OPTION(tile_menu_icons);
    else if (key == "tile_player_col")
    {
        tile_player_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_monster_col")
    {
        tile_monster_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_neutral_col")
    {
        tile_neutral_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_friendly_col")
    {
        tile_friendly_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_plant_col")
    {
        tile_plant_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_item_col")
    {
        tile_item_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_unseen_col")
    {
        tile_unseen_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_floor_col")
    {
        tile_floor_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_wall_col")
    {
        tile_wall_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_mapped_wall_col")
    {
        tile_mapped_wall_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_door_col")
    {
        tile_door_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_downstairs_col")
    {
        tile_downstairs_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_upstairs_col")
    {
        tile_upstairs_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_feature_col")
    {
        tile_feature_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_trap_col")
    {
        tile_trap_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_water_col")
    {
        tile_water_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_lava_col")
    {
        tile_lava_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_excluded_col")
    {
        tile_excluded_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_excl_centre_col" || key == "tile_excl_center_col")
    {
        tile_excl_centre_col =
               str_to_tile_colour(field);
    }
    else if (key == "tile_window_col")
    {
        tile_window_col =
                str_to_tile_colour(field);
    }
    else if (key == "tile_font_crt_file")
    {
        tile_font_crt_file = field;
    }
    else INT_OPTION(tile_font_crt_size, 1, INT_MAX);
    else if (key == "tile_font_msg_file")
    {
        tile_font_msg_file = field;
    }
    else INT_OPTION(tile_font_msg_size, 1, INT_MAX);
    else if (key == "tile_font_stat_file")
    {
        tile_font_stat_file = field;
    }
    else INT_OPTION(tile_font_stat_size, 1, INT_MAX);
    else if (key == "tile_font_tip_file")
    {
        tile_font_tip_file = field;
    }
    else INT_OPTION(tile_font_tip_size, 1, INT_MAX);
    else if (key == "tile_font_lbl_file")
    {
        tile_font_lbl_file = field;
    }
    else INT_OPTION(tile_font_lbl_size, 1, INT_MAX);
    else INT_OPTION(tile_key_repeat_delay, 0, INT_MAX);
    else if (key == "tile_full_screen")
        tile_full_screen = (screen_mode)_read_bool(field, tile_full_screen);
    else INT_OPTION(tile_window_width, 0, INT_MAX);
    else INT_OPTION(tile_window_height, 0, INT_MAX);
    else INT_OPTION(tile_map_pixels, 1, INT_MAX);
    else BOOL_OPTION(tile_force_overlay);
    else INT_OPTION(tile_tooltip_ms, 0, INT_MAX);
    else INT_OPTION(tile_update_rate, 50, INT_MAX);
    else INT_OPTION(tile_runrest_rate, 0, INT_MAX);
    else BOOL_OPTION(tile_show_minihealthbar);
    else BOOL_OPTION(tile_show_minimagicbar);
    else BOOL_OPTION(tile_show_demon_tier);
    else if (key == "tile_tag_pref")
    {
        tile_tag_pref = string2tag_pref(field.c_str());
    }
#endif // USE_TILE

    else if (key == "bindkey")
    {
        _bindkey(field);
    }
    else if (key == "constant")
    {
        if (variables.find(field) == variables.end())
        {
            report_error(make_stringf("No variable named '%s' to make "
                                      "constant", field.c_str()));
        }
        else if (constants.find(field) != constants.end())
        {
            report_error(make_stringf("'%s' is already a constant",
                                      field.c_str()));
        }
        else
            constants.insert(field);
    }
    else INT_OPTION(arena_delay, 0, INT_MAX);
    else BOOL_OPTION(arena_dump_msgs);
    else BOOL_OPTION(arena_dump_msgs_all);
    else BOOL_OPTION(arena_list_eq);

    // Catch-all else, copies option into map
    else if (runscript)
    {
#ifdef CLUA_BINDINGS
        if (!clua.callbooleanfn(false, "c_process_lua_option", "ss",
                        key.c_str(), orig_field.c_str()))
#endif
        {
#ifdef CLUA_BINDINGS
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
#endif
            named_options[key] = orig_field;
        }
    }
}

// Checks an include file name for safety and resolves it to a readable path.
// If safety check fails, throws a string with the reason for failure.
// If file cannot be resolved, returns the empty string (this does not throw!)
// If file can be resolved, returns the resolved path.
std::string game_options::resolve_include(
    std::string parent_file,
    std::string included_file,
    const std::vector<std::string> *rcdirs)

    throw (std::string)
{
    // Before we start, make sure we convert forward slashes to the platform's
    // favoured file separator.
    parent_file   = canonicalise_file_separator(parent_file);
    included_file = canonicalise_file_separator(included_file);
#if defined(TARGET_OS_DOS)
    get_dos_compatible_file_name(&included_file);
#endif

    // How we resolve include paths:
    // 1. If it's an absolute path, use it directly.
    // 2. Try the name relative to the parent filename, if supplied.
    // 3. Try the name relative to any of the provided rcdirs.
    // 4. Try locating the name as a regular data file (also checks for the
    //    file name relative to the current directory).
    // 5. Fail, and return empty string.

    assert_read_safe_path(included_file);

    // There's only so much you can do with an absolute path.
    // Note: absolute paths can only get here if we're not on a
    // multiuser system. On multiuser systems assert_read_safe_path()
    // will throw an exception if it sees absolute paths.
    if (is_absolute_path(included_file))
        return (file_exists(included_file)? included_file : "");

    if (!parent_file.empty())
    {
        const std::string candidate =
            get_path_relative_to(parent_file, included_file);
        if (file_exists(candidate))
            return (candidate);
    }

    if (rcdirs)
    {
        const std::vector<std::string> &dirs(*rcdirs);
        for (int i = 0, size = dirs.size(); i < size; ++i)
        {
            const std::string candidate( catpath(dirs[i], included_file) );
            if (file_exists(candidate))
                return (candidate);
        }
    }

    return datafile_path(included_file, false, true);
}

std::string game_options::resolve_include( const std::string &file,
                                           const char *type)
{
    try
    {
        const std::string resolved =
            resolve_include(this->filename, file, &SysEnv.rcdirs);

        if (resolved.empty())
        {
            report_error(
                make_stringf("Cannot find %sfile \"%s\".",
                             type, file.c_str()));
        }
        return (resolved);
    }
    catch (const std::string &err)
    {
        report_error(
            make_stringf("Cannot include %sfile: %s", type, err.c_str()));
        return "";
    }
}

bool game_options::was_included(const std::string &file) const
{
    return (included.find(file) != included.end());
}

void game_options::include(const std::string &rawfilename,
                           bool resolve,
                           bool runscript)
{
    const std::string include_file =
        resolve ? resolve_include(rawfilename) : rawfilename;

    if (was_included(include_file))
    {
        // Report error with rawfilename, not the resolved file name - we
        // don't want to leak file paths in dgamelaunch installs.
        report_error(make_stringf("Skipping previously included file: \"%s\".",
                                  rawfilename.c_str()));
        return;
    }

    included.insert(include_file);

    // Change this->filename to the included filename while we're reading it.
    unwind_var<std::string> optfile(this->filename, include_file);
    unwind_var<std::string> basefile(this->basefilename, rawfilename);

    // Save current line number
    unwind_var<int> currlinenum(this->line_num, 0);

    // Also unwind any aliases defined in included files.
    unwind_var<string_map> unwalias(aliases);

    FILE* f = fopen( include_file.c_str(), "r" );
    if (f)
    {
        FileLineInput fl(f);
        this->read_options(fl, runscript, false);
        fclose(f);
    }
}

void game_options::report_error(const std::string &error)
{
    // If called before game starts, log a startup error,
    // otherwise spam the warning channel.
    if (crawl_state.need_save)
    {
        mprf(MSGCH_ERROR, "Warning: %s (%s:%d)", error.c_str(),
             basefilename.c_str(), line_num);
    }
    else
    {
        crawl_state.add_startup_error(make_stringf("%s (%s:%d)",
                                                   error.c_str(),
                                                   basefilename.c_str(),
                                                   line_num));
    }
}

static std::string check_string(const char *s)
{
    return (s? s : "");
}

void get_system_environment(void)
{
    // The player's name
    SysEnv.crawl_name = check_string( getenv("CRAWL_NAME") );

    // The directory which contians init.txt, macro.txt, morgue.txt
    // This should end with the appropriate path delimiter.
    SysEnv.crawl_dir = check_string( getenv("CRAWL_DIR") );

#ifdef DGL_SIMPLE_MESSAGING
    // Enable DGL_SIMPLE_MESSAGING only if SIMPLEMAIL and MAIL are set.
    const char *simplemail = getenv("SIMPLEMAIL");
    if (simplemail && strcmp(simplemail, "0"))
    {
        const char *mail = getenv("MAIL");
        SysEnv.messagefile = mail? mail : "";
    }
#endif

    // The full path to the init file -- this overrides CRAWL_DIR.
    SysEnv.crawl_rc = check_string( getenv("CRAWL_RC") );

#ifdef MULTIUSER
    // The user's home directory (used to look for ~/.crawlrc file)
    SysEnv.home = check_string( getenv("HOME") );
#endif
}                               // end get_system_environment()

static void set_crawl_base_dir(const char *arg)
{
    if (!arg)
        return;

    SysEnv.crawl_base = get_parent_directory(arg);
}

// parse args, filling in Options and game environment as we go.
// returns true if no unknown or malformed arguments were found.

// Keep this in sync with the option names.
enum commandline_option_type {
    CLO_SCORES,
    CLO_NAME,
    CLO_RACE,
    CLO_CLASS,
    CLO_PLAIN,
    CLO_DIR,
    CLO_RC,
    CLO_RCDIR,
    CLO_TSCORES,
    CLO_VSCORES,
    CLO_SCOREFILE,
    CLO_MORGUE,
    CLO_MACRO,
    CLO_MAPSTAT,
    CLO_ARENA,
    CLO_TEST,
    CLO_SCRIPT,
    CLO_BUILDDB,
    CLO_HELP,
    CLO_VERSION,
    CLO_SEED,
    CLO_SAVE_VERSION,
    CLO_SPRINT,
    CLO_EXTRA_OPT_FIRST,
    CLO_EXTRA_OPT_LAST,

    CLO_NOPS
};

static const char *cmd_ops[] = {
    "scores", "name", "species", "background", "plain", "dir", "rc",
    "rcdir", "tscores", "vscores", "scorefile", "morgue", "macro",
    "mapstat", "arena", "test", "script", "builddb", "help", "version",
    "seed", "save-version", "sprint", "extra-opt-first", "extra-opt-last"
};

const int num_cmd_ops = CLO_NOPS;
bool arg_seen[num_cmd_ops];

std::string find_executable_path()
{
    char tempPath[2048];

    // A lot of OSes give ways to find the location of the running app's
    // binary executable. This is useful, because argv[0] can be relative
    // when we really need an absolute path in order to locate the game's
    // resources.

    // Faster than a memset, and counts as a null-terminated string!
    tempPath[0] = 0;

#if defined ( TARGET_OS_WINDOWS )
    GetModuleFileName ( NULL, tempPath, sizeof(tempPath) );
#elif defined ( TARGET_OS_LINUX )
    const ssize_t rsize =
        readlink("/proc/self/exe", tempPath, sizeof(tempPath) - 1);
    if (rsize > 0)
        tempPath[rsize] = 0;
#elif defined ( TARGET_OS_MACOSX )
    strncpy ( tempPath, NXArgv[0], sizeof(tempPath) );
#else
    // We don't know how to find the executable's path on this OS.
#endif
    return std::string(tempPath);
}

static void _print_version()
{
    printf("Crawl version %s%s", Version::Long().c_str(), "\n");
    printf("Save file version %d.%d%s", TAG_MAJOR_VERSION, TAG_MINOR_VERSION, "\n");
    printf("%s", compilation_info().c_str());
}

static void _print_save_version(char *name)
{
#ifdef LOAD_UNPACKAGE_CMD
    bool need_unlink = false;
#endif
    std::string basename = get_savedir_filename(name, "", "");
    std::string filename = basename + ".chr";

    FILE *charf = fopen(name, "rb");

    if (!charf) {
#ifdef LOAD_UNPACKAGE_CMD
        std::string zipfile = basename + PACKAGE_SUFFIX;
        FILE *handle = fopen(zipfile.c_str(), "rb+");
        if (handle == NULL)
        {
            fprintf(stderr, "Unable to open %s for reading!\n",
                            zipfile.c_str());
            return;
        }
        else
        {
            fclose(handle);

            // Create command.
            char cmd_buff[1024];

            std::string zipname = basename;
            std::string directory = get_savedir();
            std::string savefile = filename;
            savefile.erase(0, savefile.rfind(FILE_SEPARATOR) + 1);

            escape_path_spaces(zipname);
            escape_path_spaces(directory);
            escape_path_spaces(savefile);
            snprintf( cmd_buff, sizeof(cmd_buff), UNPACK_SPECIFIC_FILE_CMD,
                      zipname.c_str(),
                      directory.c_str(),
                      savefile.c_str() );

            if (system( cmd_buff ) != 0)
            {
                fprintf(stderr, "Warning: Zip command "
                                "(UNPACK_SPECIFIC_FILE_CMD) "
                                "returned non-zero value!\n" );
            }
            need_unlink = true;
        }
#endif
        charf = fopen(filename.c_str(), "rb");
    }
    if (!charf) {
        fprintf(stderr, "Unable to open %s for reading!\n", filename.c_str());
        goto cleanup;
    }

    char major, minor;
    if (!get_save_version(charf, major, minor)) {
        fprintf(stderr, "Save file is invalid.\n");
    }
    else {
        printf("Save file version for %s is %d.%d\n", name, major, minor);
    }

cleanup:
#ifdef LOAD_UNPACKAGE_CMD
    if (need_unlink)
        unlink(filename.c_str());
#else
    ;
#endif
}

static bool _check_extra_opt(char* _opt)
{
    std::string opt(_opt);
    trim_string(opt);

    if (opt[0] == ':' || opt[0] == '<' || opt[0] == '{'
        || starts_with(opt, "L<") || starts_with(opt, "Lua{"))
    {
        fprintf(stderr, "An extra option can't use Lua (%s)\n",
                _opt);
        return (false);
    }

    if (opt[0] == '#')
    {
        fprintf(stderr, "An extra option can't be a comment (%s)\n",
                _opt);
        return (false);
    }

    if (opt.find_first_of('=') == std::string::npos)
    {
        fprintf(stderr, "An extra opt must contain a '=' (%s)\n",
                _opt);
        return (false);
    }

    std::vector<std::string> parts = split_string(opt, "=");
    if (opt.find_first_of('=') == 0 || parts[0].length() == 0)
    {
        fprintf(stderr, "An extra opt must have an option name (%s)\n",
                _opt);
        return (false);
    }

    return (true);
}

bool parse_args( int argc, char **argv, bool rc_only )
{
    COMPILE_CHECK(ARRAYSZ(cmd_ops) == CLO_NOPS, c1);

    std::string exe_path = find_executable_path();

    if (!exe_path.empty())
        set_crawl_base_dir(exe_path.c_str());
    else
        set_crawl_base_dir(argv[0]);

    SysEnv.crawl_exe = get_base_filename(argv[0]);

    SysEnv.rcdirs.clear();

    if (argc < 2)           // no args!
        return (true);

    char *arg, *next_arg;
    int current = 1;
    bool nextUsed = false;
    int ecount;

    // initialise
    for (int i = 0; i < num_cmd_ops; i++)
         arg_seen[i] = false;

    if (SysEnv.cmd_args.empty())
        for (int i = 1; i < argc; ++i)
            SysEnv.cmd_args.push_back(argv[i]);

    while (current < argc)
    {
        // get argument
        arg = argv[current];

        // next argument (if there is one)
        if (current+1 < argc)
            next_arg = argv[current+1];
        else
            next_arg = NULL;

        nextUsed = false;

        // arg MUST begin with '-'
        char c = arg[0];
        if (c != '-')
        {
            fprintf(stderr,
                    "Option '%s' is invalid; options must be prefixed "
                    "with -\n\n", arg);
            return (false);
        }

        // Look for match (we accept both -option and --option).
        if (arg[1] == '-')
            arg = &arg[2];
        else
            arg = &arg[1];

        int o;
        for (o = 0; o < num_cmd_ops; o++)
            if (stricmp(cmd_ops[o], arg) == 0)
                break;

        // Print the list of commandline options for "--help".
        if (o == CLO_HELP)
            return (false);

        if (o == num_cmd_ops)
        {
            fprintf(stderr,
                    "Unknown option: %s\n\n", argv[current]);
            return (false);
        }

        // Disallow options specified more than once.
        if (arg_seen[o] == true)
            return (false);

        // Set arg to 'seen'.
        arg_seen[o] = true;

        // Partially parse next argument.
        bool next_is_param = false;
        if (next_arg != NULL
            && (next_arg[0] != '-' || strlen(next_arg) == 1))
        {
            next_is_param = true;
        }

        // Take action according to the cmd chosen.
        switch (o)
        {
        case CLO_SCORES:
        case CLO_TSCORES:
        case CLO_VSCORES:
            if (!next_is_param)
                ecount = -1;            // default
            else // optional number given
            {
                ecount = atoi(next_arg);
                if (ecount < 1)
                    ecount = 1;

                if (ecount > SCORE_FILE_ENTRIES)
                    ecount = SCORE_FILE_ENTRIES;

                nextUsed = true;
            }

            if (!rc_only)
            {
                Options.sc_entries = ecount;

                if (o == CLO_TSCORES)
                    Options.sc_format = SCORE_TERSE;
                else if (o == CLO_VSCORES)
                    Options.sc_format = SCORE_VERBOSE;
                else if (o == CLO_SCORES)
                    Options.sc_format = SCORE_REGULAR;
            }
            break;

        case CLO_MAPSTAT:
            crawl_state.map_stat_gen = true;
            if (next_is_param)
            {
                SysEnv.map_gen_iters = atoi(next_arg);
                if (SysEnv.map_gen_iters < 1)
                    SysEnv.map_gen_iters = 1;
                else if (SysEnv.map_gen_iters > 10000)
                    SysEnv.map_gen_iters = 10000;
                nextUsed = true;
            }
            else
                SysEnv.map_gen_iters = 100;
            break;

        case CLO_ARENA:
            if (!rc_only)
                Options.game.type = GAME_TYPE_ARENA;
            if (next_is_param)
            {
                if (!rc_only)
                    Options.game.arena_teams = next_arg;
                nextUsed = true;
            }
            break;

        case CLO_TEST:
            crawl_state.test = true;
            if (next_is_param)
            {
                crawl_state.tests_selected = split_string(",", next_arg);
                nextUsed = true;
            }
            break;

        case CLO_SCRIPT:
            crawl_state.test   = true;
            crawl_state.script = true;
            if (current < argc - 1)
            {
                crawl_state.tests_selected = split_string(",", next_arg);
                for (int extra = current + 2; extra < argc; ++extra)
                    crawl_state.script_args.push_back(argv[extra]);
                current = argc;
            }
            else
                end(1, false,
                    "-script must specify comma-separated script names");
            break;

        case CLO_BUILDDB:
            if (next_is_param)
                return (false);
            crawl_state.build_db = true;
            break;

        case CLO_MACRO:
            if (!next_is_param)
                return (false);
            if (!rc_only)
                Options.macro_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_MORGUE:
            if (!next_is_param)
                return (false);
            if (!rc_only)
                SysEnv.morgue_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_SCOREFILE:
            if (!next_is_param)
                return (false);
            if (!rc_only)
                SysEnv.scorefile = next_arg;
            nextUsed = true;
            break;

        case CLO_NAME:
            if (!next_is_param)
                return (false);
            if (!rc_only)
                Options.game.name = next_arg;
            nextUsed = true;
            break;

        case CLO_RACE:
        case CLO_CLASS:
            if (!next_is_param)
                return (false);

            if (!rc_only)
            {
                if (o == 2)
                    Options.game.species = _str_to_species(std::string(next_arg));

                if (o == 3)
                    Options.game.job = _str_to_job(std::string(next_arg));
            }
            nextUsed = true;
            break;

        case CLO_PLAIN:
            if (next_is_param)
                return (false);

            if (!rc_only)
            {
                Options.char_set = CSET_ASCII;
                init_char_table(Options.char_set);
            }
            break;

        case CLO_RCDIR:
            // Always parse.
            if (!next_is_param)
                return (false);

            SysEnv.add_rcdir(next_arg);
            nextUsed = true;
            break;

        case CLO_DIR:
            // Always parse.
            if (!next_is_param)
                return (false);

            SysEnv.crawl_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_RC:
            // Always parse.
            if (!next_is_param)
                return (false);

            SysEnv.crawl_rc = next_arg;
            nextUsed = true;
            break;

        case CLO_HELP:
            // Shouldn't happen.
            return (false);

        case CLO_VERSION:
            _print_version();
            end(0);

        case CLO_SAVE_VERSION:
            // Always parse.
            if (!next_is_param)
                return (false);

            _print_save_version(next_arg);
            end(0);

        case CLO_SEED:
            if (!next_is_param)
                return (false);

            if (!sscanf(next_arg, "%lx", &Options.seed))
                return (false);
            nextUsed = true;
            break;

        case CLO_SPRINT:
            if (!rc_only)
                Options.game.type = GAME_TYPE_SPRINT;
            break;

        case CLO_EXTRA_OPT_FIRST:
            if (!next_is_param)
                return (false);

            if (!_check_extra_opt(next_arg))
                // Don't print the help message if the opt was wrong
                return (true);
            SysEnv.extra_opts_first.push_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            arg_seen[o] = false;
            break;

        case CLO_EXTRA_OPT_LAST:
            if (!next_is_param)
                return false;

            if (!_check_extra_opt(next_arg))
                // Don't print the help message if the opt was wrong
                return (true);
            SysEnv.extra_opts_last.push_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            arg_seen[o] = false;
            break;
        }

        // Update position.
        current++;
        if (nextUsed)
            current++;
    }

    return (true);
}

//////////////////////////////////////////////////////////////////////////
// game_options

int game_options::o_int(const char *name, int def) const
{
    int val = def;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
    {
        val = atoi(i->second.c_str());
    }
    return (val);
}

long game_options::o_long(const char *name, long def) const
{
    long val = def;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
    {
        const char *s = i->second.c_str();
        char *es = NULL;
        long num = strtol(s, &es, 10);
        if (s != (const char *) es && es)
            val = num;
    }
    return (val);
}

bool game_options::o_bool(const char *name, bool def) const
{
    bool val = def;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
        val = _read_bool(i->second, val);
    return (val);
}

std::string game_options::o_str(const char *name, const char *def) const
{
    std::string val;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
        val = i->second;
    else if (def)
        val = def;
    return (val);
}

int game_options::o_colour(const char *name, int def) const
{
    std::string val = o_str(name);
    trim_string(val);
    lowercase(val);
    int col = str_to_colour(val);
    return (col == -1? def : col);
}

///////////////////////////////////////////////////////////////////////
// system_environment

void system_environment::add_rcdir(const std::string &dir)
{
    std::string cdir = canonicalise_file_separator(dir);
    if (dir_exists(cdir))
        rcdirs.push_back(cdir);
    else
        end(1, false, "Cannot find -rcdir \"%s\"", cdir.c_str());
}

///////////////////////////////////////////////////////////////////////
// menu_sort_condition

menu_sort_condition::menu_sort_condition(menu_type _mt, int _sort)
    : mtype(_mt), sort(_sort), cmp()
{
}

menu_sort_condition::menu_sort_condition(const std::string &s)
    : mtype(MT_ANY), sort(-1), cmp()
{
    std::string cp = s;
    set_menu_type(cp);
    set_sort(cp);
    set_comparators(cp);
}

bool menu_sort_condition::matches(menu_type mt) const
{
    return (mtype == MT_ANY || mtype == mt);
}

void menu_sort_condition::set_menu_type(std::string &s)
{
    static struct
    {
        const std::string mname;
        menu_type mtype;
    } menu_type_map[] =
      {
          { "any:",    MT_ANY       },
          { "inv:",    MT_INVLIST   },
          { "drop:",   MT_DROP      },
          { "pickup:", MT_PICKUP    },
          { "know:",   MT_KNOW      }
      };

    for (unsigned mi = 0; mi < ARRAYSZ(menu_type_map); ++mi)
    {
        const std::string &name = menu_type_map[mi].mname;
        if (s.find(name) == 0)
        {
            s = s.substr(name.length());
            mtype = menu_type_map[mi].mtype;
            break;
        }
    }
}

void menu_sort_condition::set_sort(std::string &s)
{
    // Strip off the optional sort clauses and get the primary sort condition.
    std::string::size_type trail_pos = s.find(':');
    if (s.find("auto:") == 0)
        trail_pos = s.find(':', trail_pos + 1);

    std::string sort_cond =
        trail_pos == std::string::npos? s : s.substr(0, trail_pos);

    trim_string(sort_cond);
    sort = _read_bool_or_number(sort_cond, sort, "auto:");

    if (trail_pos != std::string::npos)
        s = s.substr(trail_pos + 1);
    else
        s.clear();
}

void menu_sort_condition::set_comparators(std::string &s)
{
    init_item_sort_comparators(
        cmp,
        s.empty()? "equipped, basename, qualname, curse, qty" : s);
}

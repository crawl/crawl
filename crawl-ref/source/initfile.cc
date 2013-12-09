/**
 * @file
 * @brief Simple reading of an init file and system variables
**/

#include "AppHdr.h"

#include "initfile.h"
#include "options.h"

#include <algorithm>
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
#include "errors.h"
#include "kills.h"
#include "files.h"
#include "defines.h"
#ifdef USE_TILE_LOCAL
 #include "tilereg-map.h"
#endif
#ifdef USE_TILE_WEB
 #include "tileweb.h"
#endif
#include "invent.h"
#include "libutil.h"
#include "macro.h"
#include "mapdef.h"
#include "message.h"
#include "mon-util.h"
#include "jobs.h"
#include "player.h"
#include "species.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "syscalls.h"
#include "tags.h"
#include "throw.h"
#include "travel.h"
#include "items.h"
#include "unwind.h"
#include "version.h"
#include "view.h"
#include "viewchar.h"

// For finding the executable's path
#ifdef TARGET_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#elif defined (__APPLE__)
extern char **NXArgv;
#elif defined (__linux__)
#include <unistd.h>
#endif

const string game_options::interrupt_prefix = "interrupt_";
system_environment SysEnv;
game_options Options;

template <class L, class E>
static L& remove_matching(L& lis, const E& entry)
{
    lis.erase(remove(lis.begin(), lis.end(), entry), lis.end());
    return lis;
}

object_class_type item_class_by_sym(ucs_t c)
{
    switch (c)
    {
    case ')':
        return OBJ_WEAPONS;
    case '(':
    case 0x27b9: // ➹
        return OBJ_MISSILES;
    case '[':
        return OBJ_ARMOUR;
    case '/':
        return OBJ_WANDS;
    case '%':
        return OBJ_FOOD;
    case '?':
        return OBJ_SCROLLS;
    case '"': // Make the amulet symbol equiv to ring -- bwross
    case '=':
    case 0x00B0: // °
        return OBJ_JEWELLERY;
    case '!':
        return OBJ_POTIONS;
    case ':':
    case '+': // ??? -- was the only symbol working for tile order up to 0.10,
              // so keeping it for compat purposes (user configs).
    case 0x221e: // ∞
        return OBJ_BOOKS;
    case '|':
        return OBJ_STAVES;
    case '0':
        return OBJ_ORBS;
    case '}':
        return OBJ_MISCELLANY;
    case '&':
    case 'X':
    case 'x':
        return OBJ_CORPSES;
    case '$':
    case 0x20ac: // €
    case 0x00a3: // £
    case 0x00a5: // ¥
        return OBJ_GOLD;
    case '\\': // Compat break: used to be staves (why not '|'?).
        return OBJ_RODS;
    default:
        return NUM_OBJECT_CLASSES;
    }

}

template<class A, class B> static void _merge_lists(A &dest, const B &src,
                                                    bool prepend)
{
    dest.insert(prepend ? dest.begin() : dest.end(), src.begin(), src.end());
}

// Returns MSGCOL_NONE if unmatched else returns 0-15.
static msg_colour_type _str_to_channel_colour(const string &str)
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
        ret = msg_colour(col);

    return ret;
}

static const string message_channel_names[] =
{
    "plain", "friend_action", "prompt", "god", "pray", "duration", "danger",
    "warning", "food", "recovery", "sound", "talk", "talk_visual",
    "intrinsic_gain", "mutation", "monster_spell", "monster_enchant",
    "friend_spell", "friend_enchant", "monster_damage", "monster_target",
    "banishment", "rotten_meat", "equipment", "floor", "multiturn", "examine",
    "examine_filter", "diagnostic", "error", "tutorial", "orb", "timed_portal",
    "hell_effect",
};

// returns -1 if unmatched else returns 0--(NUM_MESSAGE_CHANNELS-1)
int str_to_channel(const string &str)
{
    COMPILE_CHECK(ARRAYSZ(message_channel_names) == NUM_MESSAGE_CHANNELS);

    // widespread aliases
    if (str == "visual")
        return MSGCH_TALK_VISUAL;
    else if (str == "spell")
        return MSGCH_MONSTER_SPELL;

    for (int ret = 0; ret < NUM_MESSAGE_CHANNELS; ret++)
    {
        if (str == message_channel_names[ret])
            return ret;
    }

    return -1;
}

string channel_to_str(int channel)
{
    if (channel < 0 || channel >= NUM_MESSAGE_CHANNELS)
        return "";

    return message_channel_names[channel];
}

weapon_type str_to_weapon(const string &str)
{
    if (str == "shortsword" || str == "short sword")
        return WPN_SHORT_SWORD;
    else if (str == "falchion")
        return WPN_FALCHION;
    else if (str == "quarterstaff" || str == "staff")
        return WPN_QUARTERSTAFF;
    else if (str == "mace")
        return WPN_MACE;
    else if (str == "spear")
        return WPN_SPEAR;
    else if (str == "trident")
        return WPN_TRIDENT;
    else if (str == "hand axe" || str == "handaxe" || str == "axe")
        return WPN_HAND_AXE;
    else if (str == "unarmed" || str == "claws")
        return WPN_UNARMED;
    else if (str == "sling")
        return WPN_SLING;
    else if (str == "bow")
        return WPN_BOW;
    else if (str == "crossbow")
        return WPN_CROSSBOW;
    else if (str == "thrown"
             || str == "rocks"
             || str == "javelins"
             || str == "tomahawks")
    {
        return WPN_THROWN;
    }
    else if (str == "random")
        return WPN_RANDOM;
    else if (str == "viable")
        return WPN_VIABLE;

    return WPN_UNKNOWN;
}

static string _weapon_to_str(int weapon)
{
    switch (weapon)
    {
    case WPN_SHORT_SWORD:
        return "short sword";
    case WPN_FALCHION:
        return "falchion";
    case WPN_QUARTERSTAFF:
        return "quarterstaff";
    case WPN_MACE:
        return "mace";
    case WPN_SPEAR:
        return "spear";
    case WPN_TRIDENT:
        return "trident";
    case WPN_HAND_AXE:
        return "hand axe";
    case WPN_UNARMED:
        return "claws";
    case WPN_SLING:
        return "sling";
    case WPN_BOW:
        return "bow";
    case WPN_CROSSBOW:
        return "crossbow";
    case WPN_THROWN:
        return "thrown";
    case WPN_VIABLE:
        return "viable";
    case WPN_RANDOM:
    default:
        return "random";
    }
}

// Summon types can be any of mon_summon_type (enum.h), or a relevant summoning
// spell.
int str_to_summon_type(const string &str)
{
    if (str == "clone")
        return MON_SUMM_CLONE;
    if (str == "animate")
        return MON_SUMM_ANIMATE;
    if (str == "chaos")
        return MON_SUMM_CHAOS;
    if (str == "miscast")
        return MON_SUMM_MISCAST;
    if (str == "zot")
        return MON_SUMM_ZOT;
    if (str == "wrath")
        return MON_SUMM_WRATH;
    if (str == "aid")
        return MON_SUMM_AID;

    return spell_by_name(str);
}

static fire_type _str_to_fire_types(const string &str)
{
    if (str == "launcher")
        return FIRE_LAUNCHER;
    else if (str == "dart")
        return FIRE_DART;
    else if (str == "stone")
        return FIRE_STONE;
    else if (str == "rock")
        return FIRE_ROCK;
    else if (str == "javelin")
        return FIRE_JAVELIN;
    else if (str == "tomahawk")
        return FIRE_TOMAHAWK;
    else if (str == "net")
        return FIRE_NET;
    else if (str == "return" || str == "returning")
        return FIRE_RETURNING;
    else if (str == "inscribed")
        return FIRE_INSCRIBED;

    return FIRE_NONE;
}

string gametype_to_str(game_type type)
{
    switch (type)
    {
    case GAME_TYPE_NORMAL:
        return "normal";
    case GAME_TYPE_TUTORIAL:
        return "tutorial";
    case GAME_TYPE_ARENA:
        return "arena";
    case GAME_TYPE_SPRINT:
        return "sprint";
    case GAME_TYPE_ZOTDEF:
        return "zotdef";
    default:
        return "none";
    }
}

#ifndef DGAMELAUNCH
static game_type _str_to_gametype(const string& s)
{
    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        game_type t = static_cast<game_type>(i);
        if (s == gametype_to_str(t))
            return t;
    }
    return NUM_GAME_TYPE;
}
#endif

static string _species_to_str(species_type sp)
{
    if (sp == SP_RANDOM)
        return "random";
    else if (sp == SP_VIABLE)
        return "viable";
    else
        return species_name(sp);
}

static species_type _str_to_species(const string &str)
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

    if (!is_species_valid_choice(ret))
        ret = SP_UNKNOWN;

    if (ret == SP_UNKNOWN)
        fprintf(stderr, "Unknown species choice: %s\n", str.c_str());

    return ret;
}

static string _job_to_str(job_type job)
{
    if (job == JOB_RANDOM)
        return "random";
    else if (job == JOB_VIABLE)
        return "viable";
    else
        return get_job_name(job);
}

static job_type _str_to_job(const string &str)
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

    if (!is_job_valid_choice(job))
        job = JOB_UNKNOWN;

    if (job == JOB_UNKNOWN)
        fprintf(stderr, "Unknown background choice: %s\n", str.c_str());

    return job;
}

static bool _read_bool(const string &field, bool def_value)
{
    bool ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = true;

    if (field == "false" || field == "0" || field == "no")
        ret = false;

    return ret;
}

// read a value which can be either a boolean (in which case return
// 0 for true, -1 for false), or a string of the form PREFIX:NUMBER
// (e.g., auto:7), in which case return NUMBER as an int.
static int _read_bool_or_number(const string &field, int def_value,
                                const string& num_prefix)
{
    int ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = 0;

    if (field == "false" || field == "0" || field == "no")
        ret = -1;

    if (field.find(num_prefix) == 0)
        ret = atoi(field.c_str() + num_prefix.size());

    return ret;
}


static unsigned curses_attribute(const string &field)
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
            Options.report_error("Bad highlight string -- %s\n", field.c_str());
        else
            return CHATTR_HILITE | (colour << 8);
    }
    else if (field != "none")
        Options.report_error("Bad colour -- %s\n", field.c_str());
    return CHATTR_NORMAL;
}

void game_options::str_to_enemy_hp_colour(const string &colours, bool prepend)
{
    vector<string> colour_list = split_string(" ", colours, true, true);
    if (prepend)
        reverse(colour_list.begin(), colour_list.end());
    for (int i = 0, csize = colour_list.size(); i < csize; i++)
    {
        const int col = str_to_colour(colour_list[i]);
        if (col < 0)
        {
            Options.report_error("Bad enemy_hp_colour: %s\n",
                                 colour_list[i].c_str());
            return;
        }
        else if (prepend)
            enemy_hp_colour.insert(enemy_hp_colour.begin(), col);
        else
            enemy_hp_colour.push_back(col);
    }
}

#ifdef USE_TILE
static FixedVector<const char*, TAGPREF_MAX>
    tag_prefs("none", "tutorial", "named", "enemy");

static tag_pref _str_to_tag_pref(const char *opt)
{
    for (int i = 0; i < TAGPREF_MAX; i++)
    {
        if (!strcasecmp(opt, tag_prefs[i]))
            return (tag_pref)i;
    }

    return TAGPREF_ENEMY;
}
#endif

void game_options::new_dump_fields(const string &text, bool add, bool prepend)
{
    // Easy; chardump.cc has most of the intelligence.
    vector<string> fields = split_string(",", text, true, true);
    if (add)
        _merge_lists(dump_order, fields, prepend);
    else
    {
        for (int f = 0, size = fields.size(); f < size; ++f)
            for (int i = 0, dsize = dump_order.size(); i < dsize; ++i)
            {
                if (dump_order[i] == fields[f])
                {
                    dump_order.erase(dump_order.begin() + i);
                    break;
                }
            }
    }
}

void game_options::set_default_activity_interrupts()
{
    for (int adelay = 0; adelay < NUM_DELAYS; ++adelay)
        for (int aint = 0; aint < NUM_AINTERRUPTS; ++aint)
        {
            activity_interrupts[adelay].set(aint,
                is_delay_interruptible(static_cast<delay_type>(adelay)));
        }

    const char *default_activity_interrupts[] =
    {
        "interrupt_armour_on = hp_loss, monster_attack",
        "interrupt_armour_off = interrupt_armour_on",
        "interrupt_drop_item = interrupt_armour_on",
        "interrupt_jewellery_on = interrupt_armour_on",
        "interrupt_memorise = interrupt_armour_on, stat",
        "interrupt_butcher = interrupt_armour_on, teleport, stat, monster",
        "interrupt_bottle_blood = interrupt_butcher",
        "interrupt_vampire_feed = interrupt_butcher",
        "interrupt_multidrop = interrupt_armour_on, teleport, stat",
        "interrupt_macro = interrupt_multidrop",
        "interrupt_travel = interrupt_butcher, statue, hungry, "
                            "burden, hit_monster, sense_monster",
        "interrupt_run = interrupt_travel, message",
        "interrupt_rest = interrupt_run, full_hp, full_mp",

        // Stair ascents/descents cannot be interrupted except by
        // teleportation. Attempts to interrupt the delay will just
        // trash all queued delays, including travel.
        "interrupt_ascending_stairs = teleport",
        "interrupt_descending_stairs = teleport",
        "interrupt_uninterruptible =",
        "interrupt_weapon_swap =",

        NULL
    };

    for (int i = 0; default_activity_interrupts[i]; ++i)
        read_option_line(default_activity_interrupts[i], false);
}

void game_options::set_activity_interrupt(
        FixedBitVector<NUM_AINTERRUPTS> &eints,
        const string &interrupt)
{
    if (interrupt.find(interrupt_prefix) == 0)
    {
        string delay_name = interrupt.substr(interrupt_prefix.length());
        delay_type delay = get_delay(delay_name);
        if (delay == NUM_DELAYS)
            return report_error("Unknown delay: %s\n", delay_name.c_str());

        FixedBitVector<NUM_AINTERRUPTS> &refints =
            activity_interrupts[delay];

        eints |= refints;
        return;
    }

    activity_interrupt_type ai = get_activity_interrupt(interrupt);
    if (ai == NUM_AINTERRUPTS)
    {
        return report_error("Delay interrupt name \"%s\" not recognised.\n",
                            interrupt.c_str());
    }

    eints.set(ai);
}

void game_options::set_activity_interrupt(const string &activity_name,
                                          const string &interrupt_names,
                                          bool append_interrupts,
                                          bool remove_interrupts)
{
    const delay_type delay = get_delay(activity_name);
    if (delay == NUM_DELAYS)
        return report_error("Unknown delay: %s\n", activity_name.c_str());

    vector<string> interrupts = split_string(",", interrupt_names);
    FixedBitVector<NUM_AINTERRUPTS> &eints = activity_interrupts[ delay ];

    if (remove_interrupts)
    {
        FixedBitVector<NUM_AINTERRUPTS> refints;
        for (int i = 0, size = interrupts.size(); i < size; ++i)
            set_activity_interrupt(refints, interrupts[i]);

        for (int i = 0; i < NUM_AINTERRUPTS; ++i)
            if (refints[i])
                eints.set(i, false);
    }
    else
    {
        if (!append_interrupts)
            eints.reset();

        for (int i = 0, size = interrupts.size(); i < size; ++i)
            set_activity_interrupt(eints, interrupts[i]);
    }

    eints.set(AI_FORCE_INTERRUPT);
}

#if defined(DGAMELAUNCH)
static string _resolve_dir(const char* path, const char* suffix)
{
    return catpath(path, "");
}
#else

static string _user_home_dir()
{
#ifdef TARGET_OS_WINDOWS
    wchar_t home[MAX_PATH];
    if (SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, home))
        return "./";
    else
        return utf16_to_8(home);
#else
    const char *home = getenv("HOME");
    if (!home || !*home)
        return "./";
    else
        return mb_to_utf8(home);
#endif
}

static string _user_home_subpath(const string subpath)
{
    return catpath(_user_home_dir(), subpath);
}

static string _resolve_dir(const char* path, const char* suffix)
{
    if (path[0] != '~')
        return catpath(string(path), suffix);
    else
        return _user_home_subpath(catpath(path + 1, suffix));
}
#endif

void game_options::reset_options()
{
    filename     = "unknown";
    basefilename = "unknown";
    line_num     = -1;

    set_default_activity_interrupts();

#ifdef DEBUG_DIAGNOSTICS
    quiet_debug_messages.reset();
#endif

#if defined(USE_TILE_LOCAL)
    restart_after_game = true;
#else
    restart_after_game = false;
#endif

    macro_dir = SysEnv.macro_dir;

#if !defined(DGAMELAUNCH)
    if (macro_dir.empty())
    {
#ifdef UNIX
        macro_dir = _user_home_subpath(".crawl");
#else
        macro_dir = "settings/";
#endif
    }
#endif

#if defined(TARGET_OS_MACOSX)
    UNUSED(_resolve_dir);
    const string tmp_path_base =
        _user_home_subpath("Library/Application Support/" CRAWL);
    save_dir   = tmp_path_base + "/saves/";
    morgue_dir = tmp_path_base + "/morgue/";
    if (SysEnv.macro_dir.empty())
        macro_dir  = tmp_path_base;
#else
    save_dir   = _resolve_dir(SysEnv.crawl_dir.c_str(), "saves/");
    morgue_dir = _resolve_dir(SysEnv.crawl_dir.c_str(), "morgue/");
#endif

#if defined(SHARED_DIR_PATH)
    shared_dir = _resolve_dir(SHARED_DIR_PATH, "");
#else
    shared_dir = save_dir;
#endif

    additional_macro_files.clear();

    seed = 0;
#ifdef DGL_SIMPLE_MESSAGING
    messaging = true;
#endif

    mouse_input = false;

    view_max_width   = max(33, VIEW_MIN_WIDTH);
    view_max_height  = max(21, VIEW_MIN_HEIGHT);
    mlist_min_height = 4;
    msg_min_height   = max(7, MSG_MIN_HEIGHT);
    msg_max_height   = max(10, MSG_MIN_HEIGHT);
    mlist_allow_alternate_layout = false;
    messages_at_top  = false;
    mlist_targeting  = false;
    msg_condense_repeats = true;
    msg_condense_short = true;

    view_lock_x = true;
    view_lock_y = true;

    center_on_scroll = false;
    symmetric_scroll = true;
    scroll_margin_x  = 2;
    scroll_margin_y  = 2;

    autopickup_on    = 1;
    autopickup_starting_ammo = true;
    default_friendly_pickup = FRIENDLY_PICKUP_FRIEND;
    default_manual_training = false;

    show_newturn_mark = true;
    show_gold_turns = true;
    show_game_turns = true;

    game = newgame_def();

    remember_name = true;

    char_set      = CSET_DEFAULT;

    // set it to the .crawlrc default
    autopickups.reset();
    autopickups.set(OBJ_GOLD);
    autopickups.set(OBJ_SCROLLS);
    autopickups.set(OBJ_POTIONS);
    autopickups.set(OBJ_BOOKS);
    autopickups.set(OBJ_JEWELLERY);
    autopickups.set(OBJ_WANDS);
    autopickups.set(OBJ_FOOD);
    auto_switch             = false;
    suppress_startup_errors = false;

    show_inventory_weights = false;
    clean_map              = false;
    show_uncursed          = true;
    easy_open              = true;
    easy_unequip           = true;
    equip_unequip          = false;
    confirm_butcher        = CONFIRM_AUTO;
    chunks_autopickup      = true;
    prompt_for_swap        = true;
    auto_drop_chunks       = ADC_NEVER;
    easy_eat_chunks        = false;
    auto_eat_chunks        = false;
    easy_confirm           = CONFIRM_SAFE_EASY;
    easy_quit_item_prompts = true;
    allow_self_target      = CONFIRM_PROMPT;
    hp_warning             = 30;
    magic_point_warning    = 0;
    default_target         = true;
    autopickup_no_burden   = true;
    skill_focus            = SKM_FOCUS_ON;

    user_note_prefix       = "";
    note_all_skill_levels  = false;
    note_skill_max         = true;
    note_xom_effects       = true;
    note_chat_messages     = true;
    note_hp_percent        = 5;

    // [ds] Grumble grumble.
    auto_list              = true;

    clear_messages         = false;
#ifdef TOUCH_UI
    show_more              = false;
#else
    show_more              = true;
#endif
    small_more             = false;

    pickup_thrown          = true;

#ifdef DGAMELAUNCH
    travel_delay           = -1;
    explore_delay          = -1;
    rest_delay             = -1;
    show_travel_trail       = true;
#else
    travel_delay           = 20;
    explore_delay          = -1;
    rest_delay             = 0;
    show_travel_trail       = false;
#endif

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

    background_colour      = BLACK;
    // [ds] Default to jazzy colours.
    detected_item_colour   = GREEN;
    detected_monster_colour= LIGHTRED;
    status_caption_colour  = BROWN;

    easy_exit_menu         = false;
    dos_use_background_intensity = true;

    level_map_title        = true;

    assign_item_slot       = SS_FORWARD;
    show_god_gift          = MB_MAYBE;

    // 10 was the cursor step default on Linux.
    level_map_cursor_step  = 7;
#ifdef UNIX
    use_fake_cursor        = true;
#else
    use_fake_cursor        = false;
#endif
    use_fake_player_cursor = true;
    show_player_species    = false;

    explore_stop           = (ES_ITEM | ES_STAIR | ES_PORTAL | ES_BRANCH
                              | ES_SHOP | ES_ALTAR | ES_RUNED_DOOR
                              | ES_GREEDY_PICKUP_SMART
                              | ES_GREEDY_VISITED_ITEM_STACK
                              | ES_GREEDY_SACRIFICEABLE);

    // The prompt conditions will be combined into explore_stop after
    // reading options.
    explore_stop_prompt    = ES_NONE;

    explore_stop_pickup_ignore.clear();

    explore_item_greed     = 10;
    explore_greedy         = true;

    explore_wall_bias      = 0;
    explore_improved       = false;
    travel_key_stop        = true;
    auto_sacrifice         = AS_NO;

    target_unshifted_dirs  = false;
    darken_beyond_range    = true;

    dump_on_save           = true;
    dump_kill_places       = KDO_ONE_PLACE;
    dump_message_count     = 20;
    dump_item_origins      = IODS_ARTEFACTS | IODS_RODS;
    dump_item_origin_price = -1;
    dump_book_spells       = true;

    drop_mode              = DM_MULTI;
#ifdef TOUCH_UI
    pickup_mode            = 0;
#else
    pickup_mode            = -1;
#endif

    flush_input[ FLUSH_ON_FAILURE ]     = true;
    flush_input[ FLUSH_BEFORE_COMMAND ] = false;
    flush_input[ FLUSH_ON_MESSAGE ]     = false;
    flush_input[ FLUSH_LUA ]            = true;

    fire_items_start       = 0;           // start at slot 'a'

    // Clear fire_order and set up the defaults.
    set_fire_order("launcher, return, "
                   "javelin / tomahawk / dart / stone / rock / net, "
                   "inscribed",
                   false, false);

    item_stack_summary_minimum = 5;

#ifdef WIZARD
    fsim_rounds = 4000L;
    fsim_mons   = "";
    fsim_scale.clear();
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
#ifdef DGAMELAUNCH
    if (wiz_mode != WIZ_NO)
        wiz_mode         = WIZ_NEVER;
#else
    wiz_mode             = WIZ_NO;
#endif
    terp_files.clear();
    no_save              = false;
#endif

#ifdef USE_TILE
    tile_show_items      = "!?/%=([)x}:|\\";
    tile_skip_title      = false;
    tile_menu_icons      = true;
#endif

#ifdef USE_TILE_LOCAL
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
#ifdef USE_FT
    // TODO: init this from system settings.  This would probably require
    // using fontconfig, but that's planned.
    tile_font_ft_light   = false;
#endif

    // window layout
    tile_full_screen      = SCREENMODE_AUTO;
    tile_window_width     = -90;
    tile_window_height    = -90;
    tile_map_pixels       = 0;
    tile_cell_pixels      = 32;
    tile_filter_scaling   = false;
# ifdef TOUCH_UI
    tile_layout_priority = split_string(",", "minimap, command, gold_turn, "
                                             "inventory, command2, spell, "
                                             "ability, monster");
# else
    tile_layout_priority = split_string(",", "minimap, inventory, gold_turn, "
                                             "command, spell, ability, "
                                             "monster");
# endif
    tile_use_small_layout = MB_MAYBE;
#endif

#ifdef USE_TILE
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
    tile_water_anim          = true;
    tile_force_regenerate_levels = false;
#endif

    // map each colour to itself as default
    for (int i = 0; i < (int)ARRAYSZ(colour); ++i)
        colour[i] = i;

    // map each channel to plain (well, default for now since I'm testing)
    for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        channels[i] = MSGCOL_DEFAULT;

    // Clear vector options.
    dump_order.clear();
    new_dump_fields("header,hiscore,stats,misc,inventory,"
                    "skills,spells,overview,mutations,messages,"
                    "screenshot,monlist,kills,notes,action_counts");

    hp_colour.clear();
    hp_colour.push_back(pair<int,int>(50, YELLOW));
    hp_colour.push_back(pair<int,int>(25, RED));
    mp_colour.clear();
    mp_colour.push_back(pair<int, int>(50, YELLOW));
    mp_colour.push_back(pair<int, int>(25, RED));
    stat_colour.clear();
    stat_colour.push_back(pair<int, int>(3, RED));
    enemy_hp_colour.clear();
    // I think these defaults are pretty ugly but apparently OS X has problems
    // with lighter colours
    enemy_hp_colour.push_back(GREEN);
    enemy_hp_colour.push_back(GREEN);
    enemy_hp_colour.push_back(BROWN);
    enemy_hp_colour.push_back(BROWN);
    enemy_hp_colour.push_back(MAGENTA);
    enemy_hp_colour.push_back(RED);
    enemy_hp_colour.push_back(LIGHTGREY);
    visual_monster_hp = false;

    force_autopickup.clear();
    note_monsters.clear();
    note_messages.clear();
    autoinscriptions.clear();
    autoinscribe_cursed = true;
    note_items.clear();
    note_skill_levels.reset();
    note_skill_levels.set(1);
    note_skill_levels.set(5);
    note_skill_levels.set(10);
    note_skill_levels.set(15);
    note_skill_levels.set(27);
    auto_spell_letters.clear();
    force_more_message.clear();
    sound_mappings.clear();
    menu_colour_mappings.clear();
    message_colour_mappings.clear();
    drop_filter.clear();
    map_file_name.clear();
    named_options.clear();

    clear_cset_overrides();

    clear_feature_overrides();
    mon_glyph_overrides.clear();
    item_glyph_overrides.clear();
    item_glyph_cache.clear();

    rest_wait_both = false;

    // Map each category to itself. The user can override in init.txt
    kill_map[KC_YOU] = KC_YOU;
    kill_map[KC_FRIENDLY] = KC_FRIENDLY;
    kill_map[KC_OTHER] = KC_OTHER;

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

ucs_t get_glyph_override(int c)
{
    if (c < 0)
    {
        c = -c;
        if (Options.char_set == CSET_IBM)
            c = (c & ~0xff) ? 0 : charset_cp437[c & 0xff];
        else if (Options.char_set == CSET_DEC)
            c = (c & 0x80) ? charset_vt100[c & 0x7f] : c;
    }
    if (wcwidth(c) != 1)
    {
        mprf(MSGCH_ERROR, "Invalid glyph override: %X", c);
        c = 0;
    }
    return c;
}

static int read_symbol(string s)
{
    if (s.empty())
        return 0;

    if (s.length() > 1 && s[0] == '\\')
        s = s.substr(1);

    {
        ucs_t c;
        const char *nc = s.c_str();
        nc += utf8towc(&c, nc);
        // no control, combining or CJK characters, please
        if (!*nc && wcwidth(c) == 1)
            return c;
    }

    int base = 10;
    if (s.length() > 1 && s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return -strtoul(s.c_str(), &tail, base);
}

void game_options::set_fire_order(const string &s, bool append, bool prepend)
{
    if (!append && !prepend)
        fire_order.clear();
    vector<string> slots = split_string(",", s);
    if (prepend)
        reverse(slots.begin(), slots.end());
    for (int i = 0, size = slots.size(); i < size; ++i)
        add_fire_order_slot(slots[i], prepend);
}

void game_options::add_fire_order_slot(const string &s, bool prepend)
{
    unsigned flags = 0;
    vector<string> alts = split_string("/", s);
    for (int i = 0, size = alts.size(); i < size; ++i)
        flags |= _str_to_fire_types(alts[i]);

    if (flags)
    {
        if (prepend)
            fire_order.insert(fire_order.begin(), flags);
        else
            fire_order.push_back(flags);
    }
}

void game_options::add_mon_glyph_overrides(const string &mons,
                                           cglyph_t &mdisp)
{
    // If one character, this is a monster letter.
    int letter = -1;
    if (mons.length() == 1)
        letter = mons[0] == '_' ? ' ' : mons[0];

    bool found = false;
    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (me->basechar == letter || me->name == mons)
        {
            found = true;
            mon_glyph_overrides[i] = mdisp;
        }
    }
    if (!found)
        report_error("Unknown monster: \"%s\"", mons.c_str());
}

cglyph_t game_options::parse_mon_glyph(const string &s) const
{
    cglyph_t md;
    md.col = 0;
    vector<string> phrases = split_string(" ", s);
    for (int i = 0, size = phrases.size(); i < size; ++i)
    {
        const string &p = phrases[i];
        const int col = str_to_colour(p, -1, false);
        if (col != -1 && colour)
            md.col = col;
        else
            md.ch = p == "_"? ' ' : read_symbol(p);
    }
    return md;
}

void game_options::add_mon_glyph_override(const string &text)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    cglyph_t mdisp = parse_mon_glyph(override[1]);
    if (mdisp.ch || mdisp.col)
        add_mon_glyph_overrides(override[0], mdisp);
}

void game_options::add_item_glyph_override(const string &text)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    cglyph_t mdisp = parse_mon_glyph(override[1]);
    if (mdisp.ch || mdisp.col)
        item_glyph_overrides.push_back(pair<string, cglyph_t>(override[0], mdisp));
}

void game_options::add_feature_override(const string &text)
{
    string::size_type epos = text.rfind("}");
    if (epos == string::npos)
        return;

    string::size_type spos = text.rfind("{", epos);
    if (spos == string::npos)
        return;

    string fname = text.substr(0, spos);
    string props = text.substr(spos + 1, epos - spos - 1);
    vector<string> iprops = split_string(",", props, true, true);

    if (iprops.size() < 1 || iprops.size() > 7)
        return;

    if (iprops.size() < 7)
        iprops.resize(7);

    trim_string(fname);
    vector<dungeon_feature_type> feats = features_by_desc(text_pattern(fname));
    if (feats.empty())
        return;

    for (int i = 0, size = feats.size(); i < size; ++i)
    {
        if (feats[i] >= NUM_FEATURES)
            continue; // TODO: handle other object types.
        feature_def &fov(feature_overrides[feats[i]]);
#define SYM(n, field) if (ucs_t s = read_symbol(iprops[n])) \
                          fov.field = s;
#define COL(n, field) if (unsigned short c = str_to_colour(iprops[n], BLACK)) \
                          fov.field = c;
        SYM(0, symbol);
        SYM(1, magic_symbol);
        COL(2, colour);
        COL(3, map_colour);
        COL(4, seen_colour);
        COL(5, em_colour);
        COL(6, seen_em_colour);
#undef SYM
#undef COL
    }
}

void game_options::add_cset_override(char_set_type set, const string &overrides)
{
    vector<string> overs = split_string(",", overrides);
    for (int i = 0, size = overs.size(); i < size; ++i)
    {
        vector<string> mapping = split_string(":", overs[i]);
        if (mapping.size() != 2)
            continue;

        dungeon_char_type dc = dchar_by_name(mapping[0]);
        if (dc == NUM_DCHAR_TYPES)
            continue;

        add_cset_override(set, dc, read_symbol(mapping[1]));
    }
}

void game_options::add_cset_override(char_set_type set, dungeon_char_type dc,
                                     int symbol)
{
    cset_override[dc] = get_glyph_override(symbol);
}

static string _find_crawlrc()
{
    const char* locations_data[][2] =
    {
        { SysEnv.crawl_dir.c_str(), "init.txt" },
#ifdef UNIX
        { SysEnv.home.c_str(), ".crawl/init.txt" },
        { SysEnv.home.c_str(), ".crawlrc" },
        { SysEnv.home.c_str(), "init.txt" },
#endif
#ifndef DATA_DIR_PATH
        { "", "init.txt" },
        { "..", "init.txt" },
        { "../settings", "init.txt" },
#endif
        { NULL, NULL }                // placeholder to mark end
    };

    // We'll look for these files in any supplied -rcdirs.
    static const char *rc_dir_filenames[] =
    {
        ".crawlrc",
        "init.txt",
    };

    // -rc option always wins.
    if (!SysEnv.crawl_rc.empty())
        return SysEnv.crawl_rc;

    // If we have any rcdirs, look in them for files from the
    // rc_dir_names list.
    for (int i = 0, size = SysEnv.rcdirs.size(); i < size; ++i)
    {
        for (unsigned n = 0; n < ARRAYSZ(rc_dir_filenames); ++n)
        {
            const string rc(catpath(SysEnv.rcdirs[i], rc_dir_filenames[n]));
            if (file_exists(rc))
                return rc;
        }
    }

    // Check all possibilities for init.txt
    for (int i = 0; locations_data[i][1] != NULL; ++i)
    {
        // Don't look at unset options
        if (locations_data[i][0] != NULL)
        {
            const string rc = catpath(locations_data[i][0],
                                      locations_data[i][1]);
            if (file_exists(rc))
                return rc;
        }
    }

    // Last attempt: pick up init.txt from datafile_path, which will
    // also search the settings/ directory.
    return datafile_path("init.txt", false, false);
}

static const char* lua_builtins[] =
{
    "clua/stash.lua",
    "clua/wield.lua",
    "clua/runrest.lua",
    "clua/gearset.lua",
    "clua/trapwalk.lua",
    "clua/autofight.lua",
    "clua/automagic.lua",
    "clua/kills.lua",
};

static const char* config_defaults[] =
{
    "defaults/autopickup_exceptions.txt",
    "defaults/runrest_messages.txt",
    "defaults/standard_colours.txt",
    "defaults/food_colouring.txt",
    "defaults/menu_colours.txt",
    "defaults/messages.txt",
    "defaults/misc.txt",
};

// Returns an error message if the init.txt was not found.
string read_init_file(bool runscript)
{
    Options.reset_options();

#ifdef CLUA_BINDINGS
    if (runscript)
    {
        for (unsigned int i = 0; i < ARRAYSZ(lua_builtins); ++i)
        {
            clua.execfile(lua_builtins[i], false, false);
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        }
    }

    for (unsigned int i = 0; i < ARRAYSZ(config_defaults); ++i)
        Options.include(datafile_path(config_defaults[i]), false, runscript);
#else
    UNUSED(lua_builtins);
    UNUSED(config_defaults);
#endif

    Options.filename     = "extra opts first";
    Options.basefilename = "extra opts first";
    Options.line_num     = 0;
    for (unsigned int i = 0; i < SysEnv.extra_opts_first.size(); i++)
    {
        Options.line_num++;
        Options.read_option_line(SysEnv.extra_opts_first[i], true);
    }

    const string init_file_name(_find_crawlrc());

    FileLineInput f(init_file_name.c_str());
    if (f.error())
    {
        if (!init_file_name.empty())
        {
            return make_stringf("(\"%s\" is not readable)",
                                init_file_name.c_str());
        }

#ifdef UNIX
        return "(~/.crawlrc missing)";
#else
        return "(no init.txt in current directory)";
#endif
    }

    Options.filename = init_file_name;
    Options.line_num = 0;
#ifdef UNIX
    Options.basefilename = "~/.crawlrc";
#else
    Options.basefilename = "init.txt";
#endif
    Options.read_options(f, runscript);

    Options.filename     = "extra opts last";
    Options.basefilename = "extra opts last";
    Options.line_num     = 0;
    for (unsigned int i = 0; i < SysEnv.extra_opts_last.size(); i++)
    {
        Options.line_num++;
        Options.read_option_line(SysEnv.extra_opts_last[i], false);
    }

    Options.filename     = init_file_name;
    Options.basefilename = get_base_filename(init_file_name);
    Options.line_num     = -1;

    return "";
}

newgame_def read_startup_prefs()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    FileLineInput fl(get_prefs_filename().c_str());
    if (fl.error())
        return newgame_def();

    game_options temp;
    temp.read_options(fl, false);

    return temp.game;
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

#ifndef DISABLE_STICKY_STARTUP_OPTIONS
static void write_newgame_options(const newgame_def& prefs, FILE *f)
{
    if (prefs.type != NUM_GAME_TYPE)
        fprintf(f, "type = %s\n", gametype_to_str(prefs.type).c_str());
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
    fprintf(f, "fully_random = %s\n", prefs.fully_random ? "yes" : "no");
}
#endif // !DISABLE_STICKY_STARTUP_OPTIONS

void write_newgame_options_file(const newgame_def& prefs)
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    // [ds] Saving startup prefs should work like this:
    //
    // 1. If the game is started without specifying a game type, always
    //    save startup preferences in the base savedir.
    // 2. If the game is started with a game type (Sprint), save startup
    //    preferences in the game-specific savedir.
    //
    // The idea is that public servers can use one instance of Crawl
    // but present Crawl and Sprint as two separate games in the
    // server-specific game menu -- the startup prefs file for Crawl and
    // Sprint should never collide, because the public server config will
    // specify the game type on the command-line.
    //
    // For normal users, startup prefs should always be saved in the
    // same base savedir so that when they start Crawl with "./crawl"
    // or the equivalent, their last game choices will be remembered,
    // even if they chose a Sprint game.
    //
    // Yes, this is unnecessarily complex. Better ideas welcome.
    //
    unwind_var<game_type> gt(crawl_state.type, Options.game.type);

    string fn = get_prefs_filename();
    FILE *f = fopen_u(fn.c_str(), "w");
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

void read_options(const string &s, bool runscript, bool clear_aliases)
{
    StringLineInput st(s);
    Options.read_options(st, runscript, clear_aliases);
}

game_options::game_options()
{
    lang = LANG_EN; // FIXME: obtain from gettext
    lang_name = 0;
    reset_options();
}

void game_options::read_options(LineInput &il, bool runscript,
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
        string s   = il.get_line();
        string str = s;
        line++;

        trim_string(str);

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
                    luacond.add(line - 1, "]])");
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

            str = str.substr(str.find("L<") == 0? 2 : 1);
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
                    luacond.add(line - 1, "]])");
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
            str = str.substr(str.find("Lua{") == 0? 4 : 1);

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
                luacond.add(line, "crawl.setopt([[");
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
            luacond.add(line, "]])");
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
    if (!check_mkdir("Save directory", &save_dir))
        end(1);

    if (!SysEnv.morgue_dir.empty())
        morgue_dir = SysEnv.morgue_dir;

    if (!check_mkdir("Morgue directory", &morgue_dir))
        end(1);

    if (evil_colour == BLACK)
        evil_colour = MAGENTA;
}

static int _str_to_killcategory(const string &s)
{
    static const char *kc[] =
    {
        "you",
        "friend",
        "other",
    };

    for (unsigned i = 0; i < ARRAYSZ(kc); ++i)
        if (s == kc[i])
            return i;

    return -1;
}

void game_options::do_kill_map(const string &from, const string &to)
{
    int ifrom = _str_to_killcategory(from),
        ito   = _str_to_killcategory(to);
    if (ifrom != -1 && ito != -1)
        kill_map[ifrom] = ito;
}

int game_options::read_explore_stop_conditions(const string &field) const
{
    int conditions = 0;
    vector<string> stops = split_string(",", field);
    for (int i = 0, count = stops.size(); i < count; ++i)
    {
        const string c = replace_all_of(stops[i], " ", "_");
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
        else if (c == "branch" || c == "branches")
            conditions |= ES_BRANCH;
        else if (c == "portal" || c == "portals")
            conditions |= ES_PORTAL;
        else if (c == "altar" || c == "altars")
            conditions |= ES_ALTAR;
        else if (c == "runed_door")
            conditions |= ES_RUNED_DOOR;
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
        else if (c == "greedy_sacrificeable" || c == "greedy_sacrificeables"
                 || c == "greedy_sacrificable" || c == "greedy_sacrificables"
                 || c == "greedy_sacrificiable" || c == "greedy_sacrificiables")
        {
            conditions |= ES_GREEDY_SACRIFICEABLE;
        }
    }
    return conditions;
}

void game_options::add_alias(const string &key, const string &val)
{
    if (key[0] == '$')
    {
        string name = key.substr(1);
        // Don't alter if it's a constant.
        if (constants.count(name))
            return;
        variables[name] = val;
    }
    else
        aliases[key] = val;
}

string game_options::unalias(const string &key) const
{
    string_map::const_iterator i = aliases.find(key);
    return i == aliases.end()? key : i->second;
}

#define IS_VAR_CHAR(c) (isaalpha(c) || c == '_' || c == '-')

string game_options::expand_vars(const string &field) const
{
    string field_out = field;

    string::size_type curr_pos = 0;

    // Only try 100 times, so as to not get stuck in infinite recursion.
    for (int i = 0; i < 100; i++)
    {
        string::size_type dollar_pos = field_out.find("$", curr_pos);

        if (dollar_pos == string::npos || field_out.size() == (dollar_pos + 1))
            break;

        string::size_type start_pos = dollar_pos + 1;

        if (!IS_VAR_CHAR(field_out[start_pos]))
            continue;

        string::size_type end_pos;
        for (end_pos = start_pos; end_pos < field_out.size(); end_pos++)
        {
            if (!IS_VAR_CHAR(field_out[end_pos + 1]))
                break;
        }

        string var_name = field_out.substr(start_pos, end_pos - start_pos + 1);

        string_map::const_iterator x = variables.find(var_name);

        if (x == variables.end())
        {
            curr_pos = end_pos + 1;
            continue;
        }

        string dollar_plus_name = "$";
        dollar_plus_name += var_name;

        field_out = replace_all(field_out, dollar_plus_name, x->second);

        // Start over at beginning
        curr_pos = 0;
    }

    return field_out;
}

void game_options::add_message_colour_mappings(const string &field,
                                               bool prepend, bool subtract)
{
    vector<string> fragments = split_string(",", field);
    if (prepend)
        reverse(fragments.begin(), fragments.end());
    for (int i = 0, count = fragments.size(); i < count; ++i)
        add_message_colour_mapping(fragments[i], prepend, subtract);
}

message_filter game_options::parse_message_filter(const string &filter)
{
    string::size_type pos = filter.find(":");
    if (pos && pos != string::npos)
    {
        string prefix = filter.substr(0, pos);
        int channel = str_to_channel(prefix);
        if (channel != -1 || prefix == "any")
        {
            string s = filter.substr(pos + 1);
            trim_string(s);
            return message_filter(channel, s);
        }
    }

    return message_filter(filter);
}

void game_options::add_message_colour_mapping(const string &field,
                                              bool prepend, bool subtract)
{
    vector<string> cmap = split_string(":", field, true, true, 1);

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
    if (subtract)
        remove_matching(message_colour_mappings, m);
    else if (prepend)
        message_colour_mappings.insert(message_colour_mappings.begin(), m);
    else
        message_colour_mappings.push_back(m);
}

// Option syntax is:
// sort_menu = [menu_type:]yes|no|auto:n[:sort_conditions]
void game_options::set_menu_sort(string field)
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

void game_options::split_parse(const string &s, const string &separator,
                               void (game_options::*add)(const string &))
{
    const vector<string> defs = split_string(separator, s);
    for (int i = 0, size = defs.size(); i < size; ++i)
        (this->*add)(defs[i]);
}

void game_options::set_option_fragment(const string &s)
{
    if (s.empty())
        return;

    string::size_type st = s.find(':');
    if (st == string::npos)
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
static void _bindkey(string field)
{
    const size_t start_bracket = field.find_first_of('[');
    const size_t end_bracket   = field.find_last_of(']');

    if (start_bracket == string::npos
        || end_bracket == string::npos
        || start_bracket > end_bracket)
    {
        mprf(MSGCH_ERROR, "Bad bindkey bracketing in '%s'",
             field.c_str());
        return;
    }

    const string key_str = field.substr(start_bracket + 1,
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
        key = key_str[0];
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
    if (start_name == string::npos)
    {
        mprf(MSGCH_ERROR, "No command name for bindkey directive '%s'",
             field.c_str());
        return;
    }

    const string       name = field.substr(start_name);
    const command_type cmd  = name_to_command(name);
    if (cmd == CMD_NO_CMD)
    {
        mprf(MSGCH_ERROR, "No command named '%s'", name.c_str());
        return;
    }

    bind_command_to_key(cmd, key);
}

static bool _is_autopickup_ban(pair<text_pattern, bool> entry)
{
    return !entry.second;
}

static bool _first_less(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first < r.first;
}

static bool _first_greater(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first > r.first;
}

// T must be convertible to from a string.
template <class T>
static void _handle_list(vector<T> &value_list, string field,
                         bool append, bool prepend, bool subtract)
{
    if (!append && !prepend && !subtract)
        value_list.clear();

    vector<T> new_entries;
    vector<string> parts = split_string(",", field);
    for (vector<string>::iterator part = parts.begin();
         part != parts.end(); ++part)
    {
        if (part->empty())
            continue;

        if (subtract)
            remove_matching(value_list, *part);
        else
            new_entries.push_back(*part);
    }
    _merge_lists(value_list, new_entries, prepend);
}

void game_options::read_option_line(const string &str, bool runscript)
{
#define BOOL_OPTION_NAMED(_opt_str, _opt_var)               \
    if (key == _opt_str) do {                               \
        _opt_var = _read_bool(field, _opt_var); \
    } while (false)
#define BOOL_OPTION(_opt) BOOL_OPTION_NAMED(#_opt, _opt)

#define COLOUR_OPTION_NAMED(_opt_str, _opt_var)                         \
    if (key == _opt_str) do {                                           \
        const int col = str_to_colour(field);                           \
        if (col != -1) {                                                \
            _opt_var = col;                                       \
        } else {                                                        \
            /*fprintf(stderr, "Bad %s -- %s\n", key, field.c_str());*/  \
            report_error("Bad %s -- %s\n", key.c_str(), field.c_str()); \
        }                                                               \
    } while (false)
#define COLOUR_OPTION(_opt) COLOUR_OPTION_NAMED(#_opt, _opt)

#define CURSES_OPTION_NAMED(_opt_str, _opt_var)     \
    if (key == _opt_str) do {                       \
        _opt_var = curses_attribute(field);   \
    } while (false)
#define CURSES_OPTION(_opt) CURSES_OPTION_NAMED(#_opt, _opt)

#define INT_OPTION_NAMED(_opt_str, _opt_var, _min_val, _max_val)        \
    if (key == _opt_str) do {                                           \
        const int min_val = (_min_val);                                 \
        const int max_val = (_max_val);                                 \
        int val = _opt_var;                                             \
        if (!parse_int(field.c_str(), val))                             \
            report_error("Bad %s: \"%s\"", _opt_str, field.c_str());    \
        else if (val < min_val)                                         \
            report_error("Bad %s: %d < %d", _opt_str, val, min_val);    \
        else if (val > max_val)                                         \
            report_error("Bad %s: %d > %d", _opt_str, val, max_val);    \
        else                                                            \
            _opt_var = val;                                             \
    } while (false)
#define INT_OPTION(_opt, _min_val, _max_val) \
    INT_OPTION_NAMED(#_opt, _opt, _min_val, _max_val)

#define LIST_OPTION_NAMED(_opt_str, _opt_var)                                \
    if (key == _opt_str) do {                                                \
        _handle_list(_opt_var, field, plus_equal, caret_equal, minus_equal); \
    } while (false)
#define LIST_OPTION(_opt) LIST_OPTION_NAMED(#_opt, _opt)
    string key    = "";
    string subkey = "";
    string field  = "";

    bool plus_equal  = false;
    bool caret_equal = false;
    bool minus_equal = false;

    const int first_equals = str.find('=');

    // all lines with no equal-signs we ignore
    if (first_equals < 0)
        return;

    field = str.substr(first_equals + 1);
    field = expand_vars(field);

    string prequal = trimmed_string(str.substr(0, first_equals));

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
    else if (prequal.length() && prequal[prequal.length() - 1] == '^')
    {
        caret_equal = true;
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

    bool plain = !plus_equal && !minus_equal && !caret_equal;

    prequal = unalias(prequal);

    const string::size_type first_dot = prequal.find('.');
    if (first_dot != string::npos)
    {
        key    = prequal.substr(0, first_dot);
        subkey = prequal.substr(first_dot + 1);
    }
    else
    {
        // no subkey (dots are okay in value field)
        key    = prequal;
    }

    // Clean up our data...
    lowercase(trim_string(key));
    lowercase(trim_string(subkey));

    // some fields want capitals... none care about external spaces
    trim_string(field);

    // Keep unlowercased field around
    const string orig_field = field;

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
        && key != "mon_glyph" && key != "item_glyph"
        && key != "opt" && key != "option"
        && key != "menu_colour" && key != "menu_color"
        && key != "message_colour" && key != "message_color"
        && key != "levels" && key != "level" && key != "entries"
        && key != "include" && key != "bindkey"
        && key != "spell_slot"
        && key.find("font") == string::npos)
    {
        lowercase(field);
    }

    if (key == "include")
        include(field, true, runscript);
    else if (key == "opt" || key == "option")
        split_parse(field, ",", &game_options::set_option_fragment);
    else if (key == "autopickup")
    {
        // clear out autopickup
        autopickups.reset();

        ucs_t c;
        for (const char* tp = field.c_str(); int s = utf8towc(&c, tp); tp += s)
        {
            object_class_type type = item_class_by_sym(c);

            if (type < NUM_OBJECT_CLASSES)
                autopickups.set(type);
            else
                report_error("Bad object type '%*s' for autopickup.\n", s, tp);
        }
    }
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else if (key == "name")
    {
        // field is already cleaned up from trim_string()
        game.name = field;
    }
#endif
    else if (key == "char_set")
    {
        if (field == "ascii")
            char_set = CSET_ASCII;
        else if (field == "ibm")
            char_set = CSET_IBM;
        else if (field == "dec")
            char_set = CSET_DEC;
        else if (field == "utf" || field == "unicode")
            char_set = CSET_OLD_UNICODE;
        else if (field == "default")
            char_set = CSET_DEFAULT;
        else
            fprintf(stderr, "Bad character set: %s\n", field.c_str());
    }
    else if (key == "language")
    {
        // FIXME: should talk to gettext/etc instead
        if (field == "en" || field == "english")
            lang = LANG_EN, lang_name = 0; // disable the db
        else if (field == "cs" || field == "czech" || field == "český" || field == "cesky")
            lang = LANG_CS, lang_name = "cs";
        else if (field == "da" || field == "danish" || field == "dansk")
            lang = LANG_DA, lang_name = "da";
        else if (field == "de" || field == "german" || field == "deutsch")
            lang = LANG_DE, lang_name = "de";
        else if (field == "el" || field == "greek" || field == "ελληνικά" || field == "ελληνικα")
            lang = LANG_EL, lang_name = "el";
        else if (field == "es" || field == "spanish" || field == "español" || field == "espanol")
            lang = LANG_ES, lang_name = "es";
        else if (field == "fi" || field == "finnish" || field == "suomi")
            lang = LANG_FI, lang_name = "fi";
        else if (field == "fr" || field == "french" || field == "français" || field == "francais")
            lang = LANG_FR, lang_name = "fr";
        else if (field == "hu" || field == "hungarian" || field == "magyar")
            lang = LANG_HU, lang_name = "hu";
        else if (field == "it" || field == "italian" || field == "italiano")
            lang = LANG_IT, lang_name = "it";
        else if (field == "ja" || field == "japanese" || field == "日本人")
            lang = LANG_JA, lang_name = "ja";
        else if (field == "ko" || field == "korean" || field == "한국의")
            lang = LANG_KO, lang_name = "ko";
        else if (field == "lt" || field == "lithuanian" || field == "lietuvos")
            lang = LANG_LT, lang_name = "lt";
        else if (field == "lv" || field == "latvian" || field == "lettish"
                 || field == "latvijas" || field == "latviešu"
                 || field == "latvieshu" || field == "latviesu")
        {
            lang = LANG_LV, lang_name = "lv";
        }
        else if (field == "nl" || field == "dutch" || field == "nederlands")
            lang = LANG_NL, lang_name = "nl";
        else if (field == "pl" || field == "polish" || field == "polski")
            lang = LANG_PL, lang_name = "pl";
        else if (field == "pt" || field == "portuguese" || field == "português" || field == "portugues")
            lang = LANG_PT, lang_name = "pt";
        else if (field == "ru" || field == "russian" || field == "русский" || field == "русскии")
            lang = LANG_RU, lang_name = "ru";
        else if (field == "zh" || field == "chinese" || field == "中国的" || field == "中國的")
            lang = LANG_ZH, lang_name = "zh";
        // Fake languages do not reset lang_name, allowing a translated
        // database in an actual language.  This is probably pointless for
        // most fake langs, though.
        else if (field == "dwarven" || field == "dwarf")
            lang = LANG_DWARVEN;
        else if (field == "jäger" || field == "jägerkin" || field == "jager" || field == "jagerkin"
                 || field == "jaeger" || field == "jaegerkin")
        {
            lang = LANG_JAGERKIN;
        }
        // Due to a conflict with actual "de", this uses slang names for the
        // option.  Let's try to keep to less rude ones, though.
        else if (field == "kraut" || field == "jerry" || field == "fritz")
            lang = LANG_KRAUT;
        else if (field == "futhark" || field == "runes" || field == "runic")
            lang = LANG_FUTHARK;
/*
        else if (field == "cyr" || field == "cyrillic" || field == "commie" || field == "кириллица")
            lang = LANG_CYRILLIC;
*/
        else if (field == "wide" || field == "doublewidth" || field == "fullwidth")
            lang = LANG_WIDE;
        else
            report_error("No translations for language: %s\n", field.c_str());
    }
    else if (key == "default_autopickup")
    {
        if (_read_bool(field, true))
            autopickup_on = 1;
        else
            autopickup_on = 0;
    }
    else BOOL_OPTION(autopickup_starting_ammo);
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
    else if (key == "default_manual_training")
    {
        if (_read_bool(field, true))
            default_manual_training = true;
        else
            default_manual_training = false;
    }
#ifndef DGAMELAUNCH
    else BOOL_OPTION(restart_after_game);
#endif
    else BOOL_OPTION(show_inventory_weights);
    else BOOL_OPTION(auto_switch);
    else BOOL_OPTION(suppress_startup_errors);
    else BOOL_OPTION(clean_map);
    else if (key == "easy_confirm")
    {
        // decide when to allow both 'Y'/'N' and 'y'/'n' on yesno() prompts
        if (field == "none")
            easy_confirm = CONFIRM_NONE_EASY;
        else if (field == "safe")
            easy_confirm = CONFIRM_SAFE_EASY;
        else if (field == "all")
            easy_confirm = CONFIRM_ALL_EASY;
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
    else if (key == "confirm_butcher")
    {
        if (field == "always")
            confirm_butcher = CONFIRM_ALWAYS;
        else if (field == "never")
            confirm_butcher = CONFIRM_NEVER;
        else if (field == "auto")
            confirm_butcher = CONFIRM_AUTO;
    }
    else BOOL_OPTION(chunks_autopickup);
    else BOOL_OPTION(prompt_for_swap);
    else BOOL_OPTION(easy_eat_chunks);
    else BOOL_OPTION(auto_eat_chunks);
    else if (key == "auto_drop_chunks")
    {
        if (field == "never")
            auto_drop_chunks = ADC_NEVER;
        else if (field == "rotten")
            auto_drop_chunks = ADC_ROTTEN;
        else if (field == "yes" || field == "true")
            auto_drop_chunks = ADC_YES;
        else
            report_error("Invalid auto_drop_chunks: \"%s\"", field.c_str());
    }
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
        const int orig_col   = str_to_colour(subkey);
        const int result_col = str_to_colour(field);

        if (orig_col != -1 && result_col != -1)
            colour[orig_col] = result_col;
        else
        {
            fprintf(stderr, "Bad colour -- %s=%d or %s=%d\n",
                     subkey.c_str(), orig_col, field.c_str(), result_col);
        }
    }
    else if (key == "channel")
    {
        const int chnl = str_to_channel(subkey);
        const msg_colour_type col  = _str_to_channel_colour(field);

        if (chnl != -1 && col != MSGCOL_NONE)
            channels[chnl] = col;
        else if (chnl == -1)
            fprintf(stderr, "Bad channel -- %s\n", subkey.c_str());
        else if (col == MSGCOL_NONE)
            fprintf(stderr, "Bad colour -- %s\n", field.c_str());
    }
    else COLOUR_OPTION(background_colour);
    else COLOUR_OPTION(detected_item_colour);
    else COLOUR_OPTION(detected_monster_colour);
    else if (key.find(interrupt_prefix) == 0)
    {
        set_activity_interrupt(key.substr(interrupt_prefix.length()),
                               field,
                               plus_equal || caret_equal,
                               minus_equal);
    }
    else if (key.find("cset") == 0)
    {
        string cset = key.substr(4);
        if (!cset.empty() && cset[0] == '_')
            cset = cset.substr(1);

        char_set_type cs = NUM_CSET;
        if (cset == "ibm")
            cs = CSET_IBM;
        else if (cset == "dec")
            cs = CSET_DEC;

        add_cset_override(cs, field);
    }
    else if (key == "feature" || key == "dungeon")
        split_parse(field, ";", &game_options::add_feature_override);
    else if (key == "mon_glyph")
        split_parse(field, ",", &game_options::add_mon_glyph_override);
    else if (key == "item_glyph")
        split_parse(field, ",", &game_options::add_item_glyph_override);
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
    else if (key == "arena_teams")
        game.arena_teams = field;
    // [ds] Allow changing map only if the map hasn't been set on the
    // command-line.
    else if (key == "map" && crawl_state.sprint_map.empty())
        game.map = field;
    // [ds] For dgamelaunch setups, the player should *not* be able to
    // set game type in their rc; the only way to set game type for
    // DGL builds should be the command-line options.
    else if (key == "type")
    {
#if defined(DGAMELAUNCH)
        game.type = Options.game.type;
#else
        game.type = _str_to_gametype(field);
#endif
    }
    else if (key == "species" || key == "race")
        game.species = _str_to_species(field);
    else if (key == "background" || key == "job" || key == "class")
        game.job = _str_to_job(field);
    else if (key == "weapon")
    {
        // Choose this weapon for backgrounds that get choice.
        game.weapon = str_to_weapon(field);
    }
    BOOL_OPTION_NAMED("fully_random", game.fully_random);
    else if (key == "fire_items_start")
    {
        if (isaalpha(field[0]))
            fire_items_start = letter_to_index(field[0]);
        else
        {
            fprintf(stderr, "Bad fire item start index: %s\n",
                     field.c_str());
        }
    }
    else if (key == "assign_item_slot")
    {
        if (field == "forward")
            assign_item_slot = SS_FORWARD;
        else if (field == "backward")
            assign_item_slot = SS_BACKWARD;
    }
    else if (key == "show_god_gift")
    {
        if (field == "yes")
            show_god_gift = MB_TRUE;
        else if (field == "unid" || field == "unident" || field == "unidentified")
            show_god_gift = MB_MAYBE;
        else if (field == "no")
            show_god_gift = MB_FALSE;
        else
            report_error("Unknown show_god_gift value: %s\n", field.c_str());
    }
    else if (key == "fire_order")
        set_fire_order(field, plus_equal, caret_equal);
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else BOOL_OPTION(remember_name);
#endif
#ifndef DGAMELAUNCH
    else if (key == "save_dir")
        save_dir = field;
    else if (key == "morgue_dir")
        morgue_dir = field;
#endif
    else BOOL_OPTION(show_newturn_mark);
    else BOOL_OPTION(show_gold_turns);
    else BOOL_OPTION(show_game_turns);
    else INT_OPTION(hp_warning, 0, 100);
    else INT_OPTION_NAMED("mp_warning", magic_point_warning, 0, 100);
    else LIST_OPTION(note_monsters);
    else LIST_OPTION(note_messages);
    else INT_OPTION(note_hp_percent, 0, 100);
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
        macro_dir = field;
#endif
#endif
    else BOOL_OPTION(auto_list);
    else if (key == "default_target")
    {
        default_target = _read_bool(field, default_target);
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
    else BOOL_OPTION_NAMED("mlist_targetting", mlist_targeting);
#endif
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
    else if (key == "skill_focus")
    {
        if (field == "toggle")
            skill_focus = SKM_FOCUS_TOGGLE;
        else if (_read_bool(field, true))
            skill_focus = SKM_FOCUS_ON;
        else
            skill_focus = SKM_FOCUS_OFF;
    }
    else BOOL_OPTION(note_all_skill_levels);
    else BOOL_OPTION(note_skill_max);
    else BOOL_OPTION(note_xom_effects);
    else BOOL_OPTION(note_chat_messages);
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
    #ifndef DGAMELAUNCH
        if (field == "never")
            wiz_mode = WIZ_NEVER;
        else if (field == "no")
            wiz_mode = WIZ_NO;
        else if (field == "yes")
            wiz_mode = WIZ_YES;
        else
            report_error("Unknown wiz_mode option: %s\n", field.c_str());
    #endif
#endif
    }
    else if (key == "ban_pickup")
    {
        if (plain)
        {
            // Only remove negative, not positive, exceptions.
            force_autopickup.erase(remove_if(force_autopickup.begin(),
                                             force_autopickup.end(),
                                             _is_autopickup_ban),
                                   force_autopickup.end());
        }

        vector<pair<text_pattern, bool> > new_entries;
        vector<string> args = split_string(",", field);
        for (int i = 0, size = args.size(); i < size; ++i)
        {
            const string &s = args[i];
            if (s.empty())
                continue;

            const pair<text_pattern, bool> f_a(s, false);

            if (minus_equal)
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        _merge_lists(force_autopickup, new_entries, caret_equal);
    }
    else if (key == "autopickup_exceptions")
    {
        if (plain)
            force_autopickup.clear();

        vector<pair<text_pattern, bool> > new_entries;
        vector<string> args = split_string(",", field);
        for (int i = 0, size = args.size(); i < size; ++i)
        {
            const string &s = args[i];
            if (s.empty())
                continue;

            pair<text_pattern, bool> f_a;

            if (s[0] == '>')
                f_a = make_pair(s.substr(1), false);
            else if (s[0] == '<')
                f_a = make_pair(s.substr(1), true);
            else
                f_a = make_pair(s, false);

            if (minus_equal)
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        _merge_lists(force_autopickup, new_entries, caret_equal);
    }
    else LIST_OPTION(note_items);
#ifndef _MSC_VER
    // break if-else chain on broken Microsoft compilers with stupid nesting limits
    else
#endif

    if (key == "autoinscribe")
    {
        if (plain)
            autoinscriptions.clear();

        const size_t first = field.find_first_of(':');
        const size_t last  = field.find_last_of(':');
        if (first == string::npos || first != last)
        {
            return report_error("Autoinscribe string must have exactly "
                                "one colon: %s\n", field.c_str());
        }

        if (first == 0)
        {
            report_error("Autoinscribe pattern is empty: %s\n", field.c_str());
            return;
        }

        if (last == field.length() - 1)
        {
            report_error("Autoinscribe result is empty: %s\n", field.c_str());
            return;
        }

        vector<string> thesplit = split_string(":", field);

        if (thesplit.size() != 2)
        {
            report_error("Error parsing autoinscribe string: %s\n",
                         field.c_str());
            return;
        }

        pair<text_pattern,string> entry(thesplit[0], thesplit[1]);

        if (minus_equal)
            remove_matching(autoinscriptions, entry);
        else if (caret_equal)
            autoinscriptions.insert(autoinscriptions.begin(), entry);
        else
            autoinscriptions.push_back(entry);
    }
    else BOOL_OPTION(autoinscribe_cursed);
#ifndef DGAMELAUNCH
    else if (key == "map_file_name")
        map_file_name = field;
#endif
    else if (key == "hp_colour" || key == "hp_color")
    {
        if (plain)
            hp_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);
            int hp_percent = 100;

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && i != 0)
            {
                report_error("Bad hp_colour string: %s\n", field.c_str());
                break;
            }

            if (insplit.size() == 2)
                hp_percent = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(hp_percent, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(hp_colour, entry);
                else
                    hp_colour.push_back(entry);
            }
            else
            {
                report_error("Bad hp_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(hp_colour.begin(), hp_colour.end(), _first_greater);
    }
    else if (key == "mp_color" || key == "mp_colour")
    {
        if (plain)
            mp_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);
            int mp_percent = 100;

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && i != 0)
            {
                report_error("Bad mp_colour string: %s\n", field.c_str());
                break;
            }

            if (insplit.size() == 2)
                mp_percent = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(mp_percent, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(mp_colour, entry);
                else
                    mp_colour.push_back(entry);
            }
            else
            {
                report_error("Bad mp_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(mp_colour.begin(), mp_colour.end(), _first_greater);
    }
    else if (key == "stat_colour" || key == "stat_color")
    {
        if (plain)
            stat_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);

            if (insplit.empty() || insplit.size() > 2
                || insplit.size() == 1 && i != 0)
            {
                report_error("Bad stat_colour string: %s\n", field.c_str());
                break;
            }

            int stat_limit = 1;
            if (insplit.size() == 2)
                stat_limit = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(stat_limit, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(stat_colour, entry);
                else
                    stat_colour.push_back(entry);
            }
            else
            {
                report_error("Bad stat_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(stat_colour.begin(), stat_colour.end(), _first_less);
    }
    else if (key == "enemy_hp_colour" || key == "enemy_hp_color")
    {
        if (plain)
            enemy_hp_colour.clear();
        str_to_enemy_hp_colour(field, caret_equal);
    }
    else if (key == "monster_list_colour" || key == "monster_list_color")
    {
        if (plain)
            clear_monster_list_colours();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && !minus_equal
                 || insplit.size() == 2 && minus_equal)
            {
                report_error("Bad monster_list_colour string: %s\n",
                             field.c_str());
                break;
            }

            const int scolour = minus_equal ? -1 : str_to_colour(insplit[1]);

            // No elemental colours!
            if (scolour >= 16 || scolour < 0 && !minus_equal)
            {
                report_error("Bad monster_list_colour: %s", insplit[1].c_str());
                break;
            }
            if (!set_monster_list_colour(insplit[0], scolour))
            {
                report_error("Bad monster_list_colour key: %s\n",
                             insplit[0].c_str());
                break;
            }
        }
    }

    else if (key == "note_skill_levels")
    {
        if (plain)
            note_skill_levels.reset();
        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            int num = atoi(thesplit[i].c_str());
            if (num > 0 && num <= 27)
                note_skill_levels.set(num, !minus_equal);
            else
            {
                report_error("Bad skill level to note -- %s\n",
                             thesplit[i].c_str());
                continue;
            }
        }
    }
    else if (key == "spell_slot")
    {
        if (plain)
            auto_spell_letters.clear();

        vector<string> thesplit = split_string(":", field);
        if (thesplit.size() != 2)
        {
            return report_error("Error parsing spell lettering string: %s\n",
                                field.c_str());
        }
        pair<text_pattern,string> entry(lowercase_string(thesplit[0]),
                                        thesplit[1]);

        if (minus_equal)
            remove_matching(auto_spell_letters, entry);
        else if (caret_equal)
            auto_spell_letters.insert(auto_spell_letters.begin(), entry);
        else
            auto_spell_letters.push_back(entry);
    }
    else BOOL_OPTION(pickup_thrown);
#ifdef WIZARD
    else if (key == "fsim_mode")
        fsim_mode = field;
    else LIST_OPTION(fsim_scale);
    else LIST_OPTION(fsim_kit);
    else if (key == "fsim_rounds")
    {
        fsim_rounds = atol(field.c_str());
        if (fsim_rounds < 1000)
            fsim_rounds = 1000;
        if (fsim_rounds > 500000L)
            fsim_rounds = 500000L;
    }
    else if (key == "fsim_mons")
        fsim_mons = field;
#endif // WIZARD
    else if (key == "sort_menus")
    {
        vector<string> frags = split_string(";", field);
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
        travel_delay = atoi(field.c_str());
        if (travel_delay < -1)
            travel_delay = -1;
        if (travel_delay > 2000)
            travel_delay = 2000;
    }
    else if (key == "explore_delay")
    {
        // Read explore delay in milliseconds.
        explore_delay = atoi(field.c_str());
        if (explore_delay < -1)
            explore_delay = -1;
        if (explore_delay > 2000)
            explore_delay = 2000;
    }
    else if (key == "rest_delay")
    {
        // Read explore delay in milliseconds.
        rest_delay = atoi(field.c_str());
        if (rest_delay < -1)
            rest_delay = -1;
        if (rest_delay > 2000)
            rest_delay = 2000;
    }
    else BOOL_OPTION(show_travel_trail);
    else if (key == "level_map_cursor_step")
    {
        level_map_cursor_step = atoi(field.c_str());
        if (level_map_cursor_step < 1)
            level_map_cursor_step = 1;
        if (level_map_cursor_step > 50)
            level_map_cursor_step = 50;
    }
    else BOOL_OPTION(use_fake_cursor);
    else BOOL_OPTION(use_fake_player_cursor);
    else BOOL_OPTION(show_player_species);
    else if (key == "force_more_message")
    {
        if (plain)
            force_more_message.clear();

        vector<message_filter> new_entries;
        vector<string> fragments = split_string(",", field);
        for (int i = 0, count = fragments.size(); i < count; ++i)
        {
            if (fragments[i].empty())
                continue;

            message_filter mf(fragments[i]);

            string::size_type pos = fragments[i].find(":");
            if (pos && pos != string::npos)
            {
                string prefix = fragments[i].substr(0, pos);
                int channel = str_to_channel(prefix);
                if (channel != -1 || prefix == "any")
                {
                    string s = fragments[i].substr(pos + 1);
                    mf = message_filter(channel, trim_string(s));
                }
            }

            if (minus_equal)
                remove_matching(force_more_message, mf);
            else
                new_entries.push_back(mf);
        }
        _merge_lists(force_more_message, new_entries, caret_equal);
    }
    else LIST_OPTION(drop_filter);
    else if (key == "travel_avoid_terrain")
    {
        // TODO: allow resetting (need reset_forbidden_terrain())
        vector<string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
            prevent_travel_to(seg[i]);
    }
    else if (key == "tc_reachable")
        tc_reachable = str_to_colour(field, tc_reachable);
    else if (key == "tc_excluded")
        tc_excluded = str_to_colour(field, tc_excluded);
    else if (key == "tc_exclude_circle")
    {
        tc_exclude_circle =
            str_to_colour(field, tc_exclude_circle);
    }
    else if (key == "tc_dangerous")
        tc_dangerous = str_to_colour(field, tc_dangerous);
    else if (key == "tc_disconnected")
        tc_disconnected = str_to_colour(field, tc_disconnected);
    else LIST_OPTION(auto_exclude);
    else BOOL_OPTION(easy_exit_menu);
    else BOOL_OPTION(dos_use_background_intensity);
    else if (key == "item_stack_summary_minimum")
        item_stack_summary_minimum = atoi(field.c_str());
    else if (key == "explore_stop")
    {
        if (plain)
            explore_stop = ES_NONE;

        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop &= ~new_conditions;
        else
            explore_stop |= new_conditions;
    }
    else if (key == "explore_stop_prompt")
    {
        if (plain)
            explore_stop_prompt = ES_NONE;
        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop_prompt &= ~new_conditions;
        else
            explore_stop_prompt |= new_conditions;
    }
    else LIST_OPTION(explore_stop_pickup_ignore);
    else if (key == "explore_item_greed")
    {
        explore_item_greed = atoi(field.c_str());
        if (explore_item_greed > 1000)
            explore_item_greed = 1000;
        else if (explore_item_greed < -1000)
            explore_item_greed = -1000;
    }
    else BOOL_OPTION(explore_greedy);
    else if (key == "explore_wall_bias")
    {
        explore_wall_bias = atoi(field.c_str());
        if (explore_wall_bias > 1000)
            explore_wall_bias = 1000;
        else if (explore_wall_bias < 0)
            explore_wall_bias = 0;
    }
    else BOOL_OPTION(explore_improved);
    else BOOL_OPTION(travel_key_stop);
    else if (key == "auto_sacrifice")
    {
        if (field == "prompt_ignore")
            auto_sacrifice = AS_PROMPT_IGNORE;
        else if (field == "prompt" || field == "ask")
            auto_sacrifice = AS_PROMPT;
        else if (field == "before_explore")
            auto_sacrifice = AS_BEFORE_EXPLORE;
        else
            auto_sacrifice = _read_bool(field, false) ? AS_YES : AS_NO;
    }
    else if (key == "sound")
    {
        if (plain)
            sound_mappings.clear();

        vector<sound_mapping> new_entries;
        vector<string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const string &sub = seg[i];
            string::size_type cpos = sub.find(":", 0);
            if (cpos != string::npos)
            {
                sound_mapping entry;
                entry.pattern = sub.substr(0, cpos);
                entry.soundfile = sub.substr(cpos + 1);
                if (minus_equal)
                    remove_matching(sound_mappings, entry);
                else
                    new_entries.push_back(entry);
            }
        }
        _merge_lists(sound_mappings, new_entries, caret_equal);
    }
#ifndef TARGET_COMPILER_VC
    // MSVC has a limit on how many if/else if can be chained together.
    else
#endif
    if (key == "menu_colour" || key == "menu_color")
    {
        if (plain)
            menu_colour_mappings.clear();

        vector<colour_mapping> new_entries;
        vector<string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            // Format is "tag:colour:pattern" or "colour:pattern" (default tag).
            // FIXME: arrange so that you can use ':' inside a pattern
            vector<string> subseg = split_string(":", seg[i], false);
            string tagname, patname, colname;
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

            if (mapping.colour == -1)
                continue;
            else if (minus_equal)
                remove_matching(menu_colour_mappings, mapping);
            else
                new_entries.push_back(mapping);
        }
        _merge_lists(menu_colour_mappings, new_entries, caret_equal);
    }
    else if (key == "message_colour" || key == "message_color")
    {
        // TODO: support -= here.
        if (plain)
            message_colour_mappings.clear();

        add_message_colour_mappings(field, caret_equal, minus_equal);
    }
    else if (key == "dump_order")
    {
        if (plain)
            dump_order.clear();

        new_dump_fields(field, !minus_equal, caret_equal);
    }
    else BOOL_OPTION(dump_on_save);
    else if (key == "dump_kill_places")
    {
        dump_kill_places = (field == "none" ? KDO_NO_PLACES :
                            field == "all"  ? KDO_ALL_PLACES
                                            : KDO_ONE_PLACE);
    }
    else if (key == "kill_map")
    {
        // TODO: treat this as a map option (e.g. kill_map.you = friendly)
        if (plain && field.empty())
        {
            kill_map[KC_YOU] = KC_YOU;
            kill_map[KC_FRIENDLY] = KC_FRIENDLY;
            kill_map[KC_OTHER] = KC_OTHER;
        }

        vector<string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const string &s = seg[i];
            string::size_type cpos = s.find(":", 0);
            if (cpos != string::npos)
            {
                string from = s.substr(0, cpos);
                string to   = s.substr(cpos + 1);
                do_kill_map(from, to);
            }
        }
    }
    else BOOL_OPTION(rest_wait_both);
    else if (key == "dump_message_count")
    {
        // Capping is implicit
        dump_message_count = atoi(field.c_str());
    }
    else if (key == "dump_item_origins")
    {
        if (plain)
            dump_item_origins = IODS_PRICE;

        vector<string> choices = split_string(",", field);
        for (int i = 0, count = choices.size(); i < count; ++i)
        {
            const string &ch = choices[i];
            if (ch == "artefacts" || ch == "artifacts"
                || ch == "artefact" || ch == "artifact")
            {
                dump_item_origins |= IODS_ARTEFACTS;
            }
            else if (ch == "ego_arm" || ch == "ego armour"
                     || ch == "ego_armour" || ch == "ego armor"
                     || ch == "ego_armor")
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
        dump_item_origin_price = atoi(field.c_str());
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
        darken_beyond_range = _read_bool(field, darken_beyond_range);
    else if (key == "drop_mode")
    {
        if (field.find("multi") != string::npos)
            drop_mode = DM_MULTI;
        else
            drop_mode = DM_SINGLE;
    }
    else if (key == "pickup_mode")
    {
        if (field.find("multi") != string::npos)
            pickup_mode = 0;
        else if (field.find("single") != string::npos)
            pickup_mode = -1;
        else
            pickup_mode = _read_bool_or_number(field, pickup_mode, "auto:");
    }
    else if (key == "additional_macro_file")
    {
        // TODO: this option could probably be improved.  For now, keep the
        // "= means append" behaviour, and don't allow clearing the list;
        // if we rename to "additional_macro_files" then it could work like
        // other list options.
        const string resolved = resolve_include(orig_field, "macro ");
        if (!resolved.empty())
            additional_macro_files.push_back(resolved);
    }
#ifdef USE_TILE
    else if (key == "tile_show_items")
        tile_show_items = field;
    else BOOL_OPTION(tile_skip_title);
    else BOOL_OPTION(tile_menu_icons);
#endif
#ifdef USE_TILE_LOCAL
    else if (key == "tile_player_col")
        tile_player_col = str_to_tile_colour(field);
    else if (key == "tile_monster_col")
        tile_monster_col = str_to_tile_colour(field);
    else if (key == "tile_neutral_col")
        tile_neutral_col = str_to_tile_colour(field);
    else if (key == "tile_friendly_col")
        tile_friendly_col = str_to_tile_colour(field);
    else if (key == "tile_plant_col")
        tile_plant_col = str_to_tile_colour(field);
    else if (key == "tile_item_col")
        tile_item_col = str_to_tile_colour(field);
    else if (key == "tile_unseen_col")
        tile_unseen_col = str_to_tile_colour(field);
    else if (key == "tile_floor_col")
        tile_floor_col = str_to_tile_colour(field);
    else if (key == "tile_wall_col")
        tile_wall_col = str_to_tile_colour(field);
    else if (key == "tile_mapped_wall_col")
        tile_mapped_wall_col = str_to_tile_colour(field);
    else if (key == "tile_door_col")
        tile_door_col = str_to_tile_colour(field);
    else if (key == "tile_downstairs_col")
        tile_downstairs_col = str_to_tile_colour(field);
    else if (key == "tile_upstairs_col")
        tile_upstairs_col = str_to_tile_colour(field);
    else if (key == "tile_feature_col")
        tile_feature_col = str_to_tile_colour(field);
    else if (key == "tile_trap_col")
        tile_trap_col = str_to_tile_colour(field);
    else if (key == "tile_water_col")
        tile_water_col = str_to_tile_colour(field);
    else if (key == "tile_lava_col")
        tile_lava_col = str_to_tile_colour(field);
    else if (key == "tile_excluded_col")
        tile_excluded_col = str_to_tile_colour(field);
    else if (key == "tile_excl_centre_col" || key == "tile_excl_center_col")
        tile_excl_centre_col = str_to_tile_colour(field);
    else if (key == "tile_window_col")
        tile_window_col = str_to_tile_colour(field);
    else if (key == "tile_font_crt_file")
        tile_font_crt_file = field;
    else INT_OPTION(tile_font_crt_size, 1, INT_MAX);
    else if (key == "tile_font_msg_file")
        tile_font_msg_file = field;
    else INT_OPTION(tile_font_msg_size, 1, INT_MAX);
    else if (key == "tile_font_stat_file")
        tile_font_stat_file = field;
    else INT_OPTION(tile_font_stat_size, 1, INT_MAX);
    else if (key == "tile_font_tip_file")
        tile_font_tip_file = field;
    else INT_OPTION(tile_font_tip_size, 1, INT_MAX);
    else if (key == "tile_font_lbl_file")
        tile_font_lbl_file = field;
    else INT_OPTION(tile_font_lbl_size, 1, INT_MAX);
#ifdef USE_FT
    else BOOL_OPTION(tile_font_ft_light);
#endif
    else INT_OPTION(tile_key_repeat_delay, 0, INT_MAX);
    else if (key == "tile_full_screen")
        tile_full_screen = (screen_mode)_read_bool(field, tile_full_screen);
    else INT_OPTION(tile_window_width, INT_MIN, INT_MAX);
    else INT_OPTION(tile_window_height, INT_MIN, INT_MAX);
    else INT_OPTION(tile_map_pixels, 1, INT_MAX);
    else INT_OPTION(tile_cell_pixels, 1, INT_MAX);
    else BOOL_OPTION(tile_filter_scaling);
#endif // USE_TILE_LOCAL
#ifdef TOUCH_UI
//    else BOOL_OPTION(tile_use_small_layout);
    else if (key == "tile_use_small_layout")
    {
        if (field == "true")
            tile_use_small_layout = MB_TRUE;
        else if (field == "false")
            tile_use_small_layout = MB_FALSE;
        else
            tile_use_small_layout = MB_MAYBE;
    }
#endif
#ifdef USE_TILE
    else BOOL_OPTION(tile_force_overlay);
    else INT_OPTION(tile_tooltip_ms, 0, INT_MAX);
    else INT_OPTION(tile_update_rate, 50, INT_MAX);
    else INT_OPTION(tile_runrest_rate, 0, INT_MAX);
    else BOOL_OPTION(tile_show_minihealthbar);
    else BOOL_OPTION(tile_show_minimagicbar);
    else BOOL_OPTION(tile_show_demon_tier);
    else BOOL_OPTION(tile_water_anim);
    else BOOL_OPTION(tile_force_regenerate_levels);
    else LIST_OPTION(tile_layout_priority);
    else if (key == "tile_tag_pref")
        tile_tag_pref = _str_to_tag_pref(field.c_str());
#endif

    else if (key == "bindkey")
        _bindkey(field);
    else if (key == "constant")
    {
        if (!variables.count(field))
            report_error("No variable named '%s' to make constant", field.c_str());
        else if (constants.count(field))
            report_error("'%s' is already a constant", field.c_str());
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
        int setmode = 0;
        if (plus_equal)
            setmode = 1;
        if (minus_equal)
            setmode = -1;
        if (caret_equal)
            setmode = 2;

        if (!clua.callbooleanfn(false, "c_process_lua_option", "ssd",
                        key.c_str(), orig_field.c_str(), setmode))
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
string game_options::resolve_include(string parent_file, string included_file,
                                     const vector<string> *rcdirs)
    throw (string)
{
    // Before we start, make sure we convert forward slashes to the platform's
    // favoured file separator.
    parent_file   = canonicalise_file_separator(parent_file);
    included_file = canonicalise_file_separator(included_file);

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
        return file_exists(included_file)? included_file : "";

    if (!parent_file.empty())
    {
        const string candidate = get_path_relative_to(parent_file,
                                                      included_file);
        if (file_exists(candidate))
            return candidate;
    }

    if (rcdirs)
    {
        const vector<string> &dirs(*rcdirs);
        for (int i = 0, size = dirs.size(); i < size; ++i)
        {
            const string candidate(catpath(dirs[i], included_file));
            if (file_exists(candidate))
                return candidate;
        }
    }

    return datafile_path(included_file, false, true);
}

string game_options::resolve_include(const string &file, const char *type)
{
    try
    {
        const string resolved = resolve_include(filename, file, &SysEnv.rcdirs);

        if (resolved.empty())
            report_error("Cannot find %sfile \"%s\".", type, file.c_str());
        return resolved;
    }
    catch (const string &err)
    {
        report_error("Cannot include %sfile: %s", type, err.c_str());
        return "";
    }
}

bool game_options::was_included(const string &file) const
{
    return included.count(file);
}

void game_options::include(const string &rawfilename, bool resolve,
                           bool runscript)
{
    const string include_file = resolve ? resolve_include(rawfilename)
                                        : rawfilename;

    if (was_included(include_file))
        return;

    included.insert(include_file);

    // Change filename to the included filename while we're reading it.
    unwind_var<string> optfile(filename, include_file);
    unwind_var<string> basefile(basefilename, rawfilename);

    // Save current line number
    unwind_var<int> currlinenum(line_num, 0);

    // Also unwind any aliases defined in included files.
    unwind_var<string_map> unwalias(aliases);

    FileLineInput fl(include_file.c_str());
    if (!fl.error())
        read_options(fl, runscript, false);
}

void game_options::report_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    string error = vmake_stringf(format, args);
    va_end(args);

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

static string check_string(const char *s)
{
    return s? s : "";
}

void get_system_environment(void)
{
    // The player's name
    SysEnv.crawl_name = check_string(getenv("CRAWL_NAME"));

    // The directory which contians init.txt, macro.txt, morgue.txt
    // This should end with the appropriate path delimiter.
    SysEnv.crawl_dir = check_string(getenv("CRAWL_DIR"));

#ifdef SAVE_DIR_PATH
    if (SysEnv.crawl_dir == "")
        SysEnv.crawl_dir = SAVE_DIR_PATH;
#endif

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
    SysEnv.crawl_rc = check_string(getenv("CRAWL_RC"));

#ifdef UNIX
    // The user's home directory (used to look for ~/.crawlrc file)
    SysEnv.home = check_string(getenv("HOME"));
#endif
}

static void set_crawl_base_dir(const char *arg)
{
    if (!arg)
        return;

    SysEnv.crawl_base = get_parent_directory(arg);
}

// parse args, filling in Options and game environment as we go.
// returns true if no unknown or malformed arguments were found.

// Keep this in sync with the option names.
enum commandline_option_type
{
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
    CLO_DUMP_MAPS,
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
    CLO_SPRINT_MAP,
    CLO_EDIT_SAVE,
    CLO_PRINT_CHARSET,
    CLO_ZOTDEF,
    CLO_TUTORIAL,
    CLO_WIZARD,
    CLO_NO_SAVE,
    CLO_GDB,
    CLO_NO_GDB, CLO_NOGDB,
#ifdef USE_TILE_WEB
    CLO_WEBTILES_SOCKET,
    CLO_AWAIT_CONNECTION,
#endif

    CLO_NOPS
};

static const char *cmd_ops[] =
{
    "scores", "name", "species", "background", "plain", "dir", "rc",
    "rcdir", "tscores", "vscores", "scorefile", "morgue", "macro",
    "mapstat", "arena", "dump-maps", "test", "script", "builddb",
    "help", "version", "seed", "save-version", "sprint",
    "extra-opt-first", "extra-opt-last", "sprint-map", "edit-save",
    "print-charset", "zotdef", "tutorial", "wizard", "no-save",
    "gdb", "no-gdb", "nogdb",
#ifdef USE_TILE_WEB
    "webtiles-socket", "await-connection",
#endif
};

static const int num_cmd_ops = CLO_NOPS;
static bool arg_seen[num_cmd_ops];

static string _find_executable_path()
{
    // A lot of OSes give ways to find the location of the running app's
    // binary executable. This is useful, because argv[0] can be relative
    // when we really need an absolute path in order to locate the game's
    // resources.
#if defined (TARGET_OS_WINDOWS)
    wchar_t tempPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, tempPath, MAX_PATH))
        return utf16_to_8(tempPath);
    else
        return "";
#elif defined (TARGET_OS_LINUX)
    char tempPath[2048];
    const ssize_t rsize =
        readlink("/proc/self/exe", tempPath, sizeof(tempPath) - 1);
    if (rsize > 0)
    {
        tempPath[rsize] = 0;
        return mb_to_utf8(tempPath);
    }
    return "";
#elif defined (TARGET_OS_MACOSX)
    return mb_to_utf8(NXArgv[0]);
#else
    // We don't know how to find the executable's path on this OS.
    return "";
#endif
}

static void _print_version()
{
    printf("Crawl version %s%s", Version::Long, "\n");
    printf("Save file version %d.%d%s", TAG_MAJOR_VERSION, TAG_MINOR_VERSION, "\n");
    printf("%s", compilation_info);
}

static void _print_save_version(char *name)
{
    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), false);
        reader chrf(&save, "chr");

        int major, minor;
        if (!get_save_version(chrf, major, minor))
            fail("Save file is invalid.");
        else
            printf("Save file version for %s is %d.%d\n", name, major, minor);
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.msg.c_str());
    }
}

enum es_command_type
{
    ES_LS,
    ES_RM,
    ES_GET,
    ES_PUT,
    ES_REPACK,
    ES_INFO,
    NUM_ES
};

static struct es_command
{
    es_command_type cmd;
    const char* name;
    bool rw;
    int min_args, max_args;
} es_commands[] =
{
    { ES_LS,      "ls",      false, 0, 0, },
    { ES_GET,     "get",     false, 1, 2, },
    { ES_PUT,     "put",     true,  1, 2, },
    { ES_RM,      "rm",      true,  1, 1, },
    { ES_REPACK,  "repack",  false, 0, 0, },
    { ES_INFO,    "info",    false, 0, 0, },
};

#define FAIL(...) do { fprintf(stderr, __VA_ARGS__); return; } while (0)
static void _edit_save(int argc, char **argv)
{
    if (argc <= 1 || !strcmp(argv[1], "help"))
    {
        printf("Usage: crawl --edit-save <name> <command>, where <command> may be:\n"
               "  ls                          list the chunks\n"
               "  get <chunk> [<chunkfile>]   extract a chunk into <chunkfile>\n"
               "  put <chunk> [<chunkfile>]   import a chunk from <chunkfile>\n"
               "     <chunkfile> defaults to \"chunk\"; use \"-\" for stdout/stdin\n"
               "  rm <chunk>                  delete a chunk\n"
               "  repack                      defrag and reclaim unused space\n"
             );
        return;
    }
    const char *name = argv[0];
    const char *cmdn = argv[1];

    es_command_type cmd = NUM_ES;
    bool rw;

    for (unsigned int nc = 0; nc < ARRAYSZ(es_commands); nc++)
        if (!strcmp(es_commands[nc].name, cmdn))
        {
            if (argc < es_commands[nc].min_args + 2)
                FAIL("Too few arguments for %s.\n", cmdn);
            else if (argc > es_commands[nc].max_args + 2)
                FAIL("Too many arguments for %s.\n", cmdn);
            cmd = es_commands[nc].cmd;
            rw = es_commands[nc].rw;
            break;
        }
    if (cmd == NUM_ES)
        FAIL("Unknown command: %s.\n", cmdn);

    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), rw);

        if (cmd == ES_LS)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            for (size_t i = 0; i < list.size(); i++)
                printf("%s\n", list[i].c_str());
        }
        else if (cmd == ES_GET)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");
            chunk_reader inc(&save, chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "wb");
            else
                f = stdout;
            if (!f)
                sysfail("Can't open \"%s\" for writing", file);

            char buf[16384];
            while (size_t s = inc.read(buf, sizeof(buf)))
                if (fwrite(buf, 1, s, f) != s)
                    sysfail("Error writing \"%s\"", file);

            if (f != stdout)
                if (fclose(f))
                    sysfail("Write error on close of \"%s\"", file);
        }
        else if (cmd == ES_PUT)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "rb");
            else
                f = stdin;
            if (!f)
                sysfail("Can't read \"%s\"", file);
            chunk_writer outc(&save, chunk);

            char buf[16384];
            while (size_t s = fread(buf, 1, sizeof(buf), f))
                outc.write(buf, s);
            if (ferror(f))
                sysfail("Error reading \"%s\"", file);

            if (f != stdin)
                fclose(f);
        }
        else if (cmd == ES_RM)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");

            save.delete_chunk(chunk);
        }
        else if (cmd == ES_REPACK)
        {
            package save2((filename + ".tmp").c_str(), true, true);
            vector<string> list = save.list_chunks();
            for (size_t i = 0; i < list.size(); i++)
            {
                char buf[16384];

                chunk_reader in(&save, list[i]);
                chunk_writer out(&save2, list[i]);

                while (plen_t s = in.read(buf, sizeof(buf)))
                    out.write(buf, s);
            }
            save2.commit();
            save.unlink();
            rename_u((filename + ".tmp").c_str(), filename.c_str());
        }
        else if (cmd == ES_INFO)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            plen_t nchunks = list.size();
            plen_t frag = save.get_chunk_fragmentation("");
            plen_t flen = save.get_size();
            plen_t slack = save.get_slack();
            printf("Chunks: (size compressed/uncompressed, fragments, name)\n");
            for (size_t i = 0; i < list.size(); i++)
            {
                int cfrag = save.get_chunk_fragmentation(list[i]);
                frag += cfrag;
                int cclen = save.get_chunk_compressed_length(list[i]);

                char buf[16384];
                chunk_reader in(&save, list[i]);
                plen_t clen = 0;
                while (plen_t s = in.read(buf, sizeof(buf)))
                    clen += s;
                printf("%7u/%7u %3u %s\n", cclen, clen, cfrag, list[i].c_str());
            }
            // the directory is not a chunk visible from the outside
            printf("Fragmentation:    %u/%u (%4.2f)\n", frag, nchunks + 1,
                   ((float)frag) / (nchunks + 1));
            printf("Unused space:     %u/%u (%u%%)\n", slack, flen,
                   100 - (100 * (flen - slack) / flen));
            // there's also wasted space due to fragmentation, but since
            // it's linear, there's no need to print it
        }
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.msg.c_str());
    }
}
#undef FAIL

static bool _check_extra_opt(char* _opt)
{
    string opt(_opt);
    trim_string(opt);

    if (opt[0] == ':' || opt[0] == '<' || opt[0] == '{'
        || starts_with(opt, "L<") || starts_with(opt, "Lua{"))
    {
        fprintf(stderr, "An extra option can't use Lua (%s)\n",
                _opt);
        return false;
    }

    if (opt[0] == '#')
    {
        fprintf(stderr, "An extra option can't be a comment (%s)\n",
                _opt);
        return false;
    }

    if (opt.find_first_of('=') == string::npos)
    {
        fprintf(stderr, "An extra opt must contain a '=' (%s)\n",
                _opt);
        return false;
    }

    vector<string> parts = split_string(opt, "=");
    if (opt.find_first_of('=') == 0 || parts[0].length() == 0)
    {
        fprintf(stderr, "An extra opt must have an option name (%s)\n",
                _opt);
        return false;
    }

    return true;
}

bool parse_args(int argc, char **argv, bool rc_only)
{
    COMPILE_CHECK(ARRAYSZ(cmd_ops) == CLO_NOPS);

    if (crawl_state.command_line_arguments.empty())
    {
        crawl_state.command_line_arguments.insert(
            crawl_state.command_line_arguments.end(),
            argv, argv + argc);
    }

    string exe_path = _find_executable_path();

    if (!exe_path.empty())
        set_crawl_base_dir(exe_path.c_str());
    else
        set_crawl_base_dir(argv[0]);

    SysEnv.crawl_exe = get_base_filename(argv[0]);

    SysEnv.rcdirs.clear();

    if (argc < 2)           // no args!
        return true;

    char *arg, *next_arg;
    int current = 1;
    bool nextUsed = false;
    int ecount;

    // initialise
    for (int i = 0; i < num_cmd_ops; i++)
         arg_seen[i] = false;

    if (SysEnv.cmd_args.empty())
    {
        for (int i = 1; i < argc; ++i)
            SysEnv.cmd_args.push_back(argv[i]);
    }

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
            return false;
        }

        // Look for match (we accept both -option and --option).
        if (arg[1] == '-')
            arg = &arg[2];
        else
            arg = &arg[1];

        int o;
        for (o = 0; o < num_cmd_ops; o++)
            if (strcasecmp(cmd_ops[o], arg) == 0)
                break;

        // Print the list of commandline options for "--help".
        if (o == CLO_HELP)
            return false;

        if (o == num_cmd_ops)
        {
            fprintf(stderr,
                    "Unknown option: %s\n\n", argv[current]);
            return false;
        }

        // Disallow options specified more than once.
        if (arg_seen[o] == true)
            return false;

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
#ifdef DEBUG_DIAGNOSTICS
            crawl_state.map_stat_gen = true;
            SysEnv.map_gen_iters = 100;
            if (!next_is_param)
                ;
            else if (isadigit(*next_arg))
            {
                SysEnv.map_gen_iters = atoi(next_arg);
                if (SysEnv.map_gen_iters < 1)
                    SysEnv.map_gen_iters = 1;
                else if (SysEnv.map_gen_iters > 10000)
                    SysEnv.map_gen_iters = 10000;
                nextUsed = true;
            }
            else
            {
                SysEnv.map_gen_range.reset(new depth_ranges);
                *SysEnv.map_gen_range = depth_ranges::parse_depth_ranges(next_arg);
                nextUsed = true;
            }

            break;
#else
            fprintf(stderr, "mapstat is available only in DEBUG_DIAGNOSTICS builds.\n");
            end(1);
#endif

        case CLO_ARENA:
            if (!rc_only)
            {
                Options.game.type = GAME_TYPE_ARENA;
                Options.restart_after_game = false;
            }
            if (next_is_param)
            {
                if (!rc_only)
                    Options.game.arena_teams = next_arg;
                nextUsed = true;
            }
            break;

        case CLO_DUMP_MAPS:
            crawl_state.dump_maps = true;
            break;

        case CLO_TEST:
            crawl_state.test = true;
            if (next_is_param)
            {
                if (!(crawl_state.test_list = !strcmp(next_arg, "list")))
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
                return false;
            crawl_state.build_db = true;
            break;

        case CLO_GDB:
            crawl_state.no_gdb = 0;
            break;

        case CLO_NO_GDB:
        case CLO_NOGDB:
            crawl_state.no_gdb = "GDB disabled via the command line.";
            break;

        case CLO_MACRO:
            if (!next_is_param)
                return false;
            SysEnv.macro_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_MORGUE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.morgue_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_SCOREFILE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.scorefile = next_arg;
            nextUsed = true;
            break;

        case CLO_NAME:
            if (!next_is_param)
                return false;
            if (!rc_only)
                Options.game.name = next_arg;
            nextUsed = true;
            break;

        case CLO_RACE:
        case CLO_CLASS:
            if (!next_is_param)
                return false;

            if (!rc_only)
            {
                if (o == 2)
                    Options.game.species = _str_to_species(string(next_arg));

                if (o == 3)
                    Options.game.job = _str_to_job(string(next_arg));
            }
            nextUsed = true;
            break;

        case CLO_PLAIN:
            if (next_is_param)
                return false;

            if (!rc_only)
            {
                Options.char_set = CSET_ASCII;
                init_char_table(Options.char_set);
            }
            break;

        case CLO_RCDIR:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.add_rcdir(next_arg);
            nextUsed = true;
            break;

        case CLO_DIR:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.crawl_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_RC:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.crawl_rc = next_arg;
            nextUsed = true;
            break;

        case CLO_HELP:
            // Shouldn't happen.
            return false;

        case CLO_VERSION:
            _print_version();
            end(0);

        case CLO_SAVE_VERSION:
            // Always parse.
            if (!next_is_param)
                return false;

            _print_save_version(next_arg);
            end(0);

        case CLO_EDIT_SAVE:
            // Always parse.
            _edit_save(argc - current - 1, argv + current + 1);
            end(0);

        case CLO_SEED:
            if (!next_is_param)
                return false;

            if (!sscanf(next_arg, "%x", &Options.seed))
                return false;
            nextUsed = true;
            break;

        case CLO_SPRINT:
            if (!rc_only)
                Options.game.type = GAME_TYPE_SPRINT;
            break;

        case CLO_SPRINT_MAP:
            if (!next_is_param)
                return false;

            nextUsed               = true;
            crawl_state.sprint_map = next_arg;
            Options.game.map       = next_arg;
            break;

        case CLO_ZOTDEF:
            if (!rc_only)
                Options.game.type = GAME_TYPE_ZOTDEF;
            break;

        case CLO_TUTORIAL:
            if (!rc_only)
                Options.game.type = GAME_TYPE_TUTORIAL;
            break;

        case CLO_WIZARD:
#ifdef WIZARD
            if (!rc_only)
                Options.wiz_mode = WIZ_NO;
#endif
            break;

        case CLO_NO_SAVE:
#ifdef WIZARD
            if (!rc_only)
                Options.no_save = true;
#endif
            break;

#ifdef USE_TILE_WEB
        case CLO_WEBTILES_SOCKET:
            nextUsed          = true;
            tiles.m_sock_name = next_arg;
            break;

        case CLO_AWAIT_CONNECTION:
            tiles.m_await_connection = true;
            break;
#endif

        case CLO_PRINT_CHARSET:
            if (rc_only)
                break;
#ifdef DGAMELAUNCH
            // Tell DGL we don't use ancient charsets anymore.  The glyph set
            // doesn't matter here, just the encoding.
            printf("UNICODE\n");
#else
            printf("This option is for DGL use only.\n");
#endif
            end(0);
            break;

        case CLO_EXTRA_OPT_FIRST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

            SysEnv.extra_opts_first.push_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            arg_seen[o] = false;
            break;

        case CLO_EXTRA_OPT_LAST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

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

    return true;
}

//////////////////////////////////////////////////////////////////////////
// game_options

int game_options::o_int(const char *name, int def) const
{
    int val = def;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
        val = atoi(i->second.c_str());
    return val;
}

bool game_options::o_bool(const char *name, bool def) const
{
    bool val = def;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
        val = _read_bool(i->second, val);
    return val;
}

string game_options::o_str(const char *name, const char *def) const
{
    string val;
    opt_map::const_iterator i = named_options.find(name);
    if (i != named_options.end())
        val = i->second;
    else if (def)
        val = def;
    return val;
}

int game_options::o_colour(const char *name, int def) const
{
    string val = o_str(name);
    trim_string(val);
    lowercase(val);
    int col = str_to_colour(val);
    return col == -1? def : col;
}

///////////////////////////////////////////////////////////////////////
// system_environment

void system_environment::add_rcdir(const string &dir)
{
    string cdir = canonicalise_file_separator(dir);
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

menu_sort_condition::menu_sort_condition(const string &s)
    : mtype(MT_ANY), sort(-1), cmp()
{
    string cp = s;
    set_menu_type(cp);
    set_sort(cp);
    set_comparators(cp);
}

bool menu_sort_condition::matches(menu_type mt) const
{
    return mtype == MT_ANY || mtype == mt;
}

void menu_sort_condition::set_menu_type(string &s)
{
    static struct
    {
        const string mname;
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
        const string &name = menu_type_map[mi].mname;
        if (s.find(name) == 0)
        {
            s = s.substr(name.length());
            mtype = menu_type_map[mi].mtype;
            break;
        }
    }
}

void menu_sort_condition::set_sort(string &s)
{
    // Strip off the optional sort clauses and get the primary sort condition.
    string::size_type trail_pos = s.find(':');
    if (s.find("auto:") == 0)
        trail_pos = s.find(':', trail_pos + 1);

    string sort_cond = trail_pos == string::npos? s : s.substr(0, trail_pos);

    trim_string(sort_cond);
    sort = _read_bool_or_number(sort_cond, sort, "auto:");

    if (trail_pos != string::npos)
        s = s.substr(trail_pos + 1);
    else
        s.clear();
}

void menu_sort_condition::set_comparators(string &s)
{
    init_item_sort_comparators(
        cmp,
        s.empty()? "equipped, basename, qualname, curse, qty" : s);
}

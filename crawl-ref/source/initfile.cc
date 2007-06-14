/*
 *  File:       initfile.cc
 *  Summary:    Simple reading of an init file and system variables
 *  Written by: David Loewenstern
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <3>     5 May 2000      GDL             Add field stripping for 'name'
 *      <2>     6/12/99         BWR             Added get_system_environment
 *      <1>     6/9/99          DML             Created
 */

#include "AppHdr.h"
#include "externs.h"

#include "initfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <ctype.h>

#include "clua.h"
#include "delay.h"
#include "direct.h"
#include "Kills.h"
#include "files.h"
#include "defines.h"
#include "invent.h"
#include "libutil.h"
#include "player.h"
#include "religion.h"
#include "stash.h"
#include "stuff.h"
#include "travel.h"
#include "items.h"
#include "view.h"

const std::string game_options::interrupt_prefix = "interrupt_";
game_options Options;

const static char *obj_syms = ")([/%.?=!.+\\0}X$";
const static int   obj_syms_len = 16;

static void read_startup_prefs();

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
    {
        if (lowercase_string(god_name(static_cast<god_type>(i))) == god)
            return (static_cast<god_type>(i));
    }
    return (GOD_NO_GOD);
}

const std::string cols[16] =
{
    "black", "blue", "green", "cyan", "red", "magenta", "brown",
    "lightgrey", "darkgrey", "lightblue", "lightgreen", "lightcyan",
    "lightred", "lightmagenta", "yellow", "white"
};

const char* colour_to_str(unsigned char colour)
{
    if ( colour >= 16 )
        return "lightgrey";
    else
        return cols[colour].c_str();
}

// returns -1 if unmatched else returns 0-15
int str_to_colour( const std::string &str, int default_colour )
{
    int ret;

    const std::string element_cols[] =
    {
        "fire", "ice", "earth", "electricity", "air", "poison", "water",
        "magic", "mutagenic", "warp", "enchant", "heal", "holy", "dark",
        "death", "necro", "unholy", "vehumet", "crystal", "blood", "smoke",
        "slime", "jewel", "elven", "dwarven", "orcish", "gila", "floor",
        "rock", "stone", "mist", "random"
    };

    for (ret = 0; ret < 16; ret++)
    {
        if (str == cols[ret])
            break;
    }

    // check for alternate spellings
    if (ret == 16)
    {
        if (str == "lightgray")
            ret = 7;
        else if (str == "darkgray")
            ret = 8;
    }

    if (ret == 16)
    {
        // Maybe we have an element colour attribute.
        for (unsigned i = 0; i < sizeof(element_cols) / sizeof(*element_cols); 
                ++i)
        {
            if (str == element_cols[i])
            {
                // Ugh.
                ret = element_type(EC_FIRE + i);
                break;
            }
        }
    }

    if (ret == 16)
    {
        // Check if we have a direct colour index
        const char *s = str.c_str();
        char *es = NULL;
        const int ci = static_cast<int>(strtol(s, &es, 10));
        if (s != (const char *) es && es && ci >= 0 && ci < 16)
            ret = ci;
    }

    return ((ret == 16) ? default_colour : ret);
}

// returns -1 if unmatched else returns 0-15
static int str_to_channel_colour( const std::string &str )
{
    int ret = str_to_colour( str );

    if (ret == -1)
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

    return (ret);
}

static const std::string message_channel_names[ NUM_MESSAGE_CHANNELS ] =
{
    "plain", "prompt", "god", "pray", "duration", "danger", "warning", "food",
    "recovery", "sound", "talk", "intrinsic_gain", "mutation", "monster_spell",
    "monster_enchant", "monster_damage", "monster_target", 
    "rotten_meat", "equipment", "floor", "multiturn", "diagnostic","tutorial"
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

static int str_to_book( const std::string& str )
{
    if ( str == "fire" || str == "flame" )
        return SBT_FIRE;
    if ( str == "cold" || str == "ice" )
        return SBT_COLD;
    if ( str == "summ" || str == "summoning" )
        return SBT_SUMM;
    if ( str == "random" )
        return SBT_RANDOM;
    return SBT_NO_SELECTION;
}

static int str_to_weapon( const std::string &str )
{
    if (str == "shortsword" || str == "short sword")
        return (WPN_SHORT_SWORD);
    else if (str == "mace")
        return (WPN_MACE);
    else if (str == "spear")
        return (WPN_SPEAR);
    else if (str == "trident")
        return (WPN_TRIDENT);
    else if (str == "hand axe" || str == "handaxe")
        return (WPN_HAND_AXE);
    else if (str == "random")
        return (WPN_RANDOM);

    return (WPN_UNKNOWN);
}

std::string weapon_to_str( int weapon )
{
    switch (weapon)
    {
    case WPN_SHORT_SWORD:
        return "short sword";
    case WPN_MACE:
        return "mace";
    case WPN_SPEAR:
        return "spear";
    case WPN_TRIDENT:
        return "trident";
    case WPN_HAND_AXE:
        return "hand axe";
    case WPN_RANDOM:
        return "random";
    default:
        return "random";
    }
}

static unsigned int str_to_fire_types( const std::string &str )
{
    if (str == "launcher")
        return (FIRE_LAUNCHER);
    else if (str == "dart")
        return (FIRE_DART);
    else if (str == "stone")
        return (FIRE_STONE);
    else if (str == "dagger")
        return (FIRE_DAGGER);
    else if (str == "spear")
        return (FIRE_SPEAR);
    else if (str == "hand axe" || str == "handaxe")
        return (FIRE_HAND_AXE);
    else if (str == "club")
        return (FIRE_CLUB);

    return (FIRE_NONE);
}

static void str_to_fire_order( const std::string &str, 
                               FixedVector< int, NUM_FIRE_TYPES > &list )
{
    int i;
    size_t pos = 0;
    std::string item = "";

    for (i = 0; i < NUM_FIRE_TYPES; i++)
    {
        // get next item from comma delimited list
        const size_t end = str.find( ',', pos );
        item = str.substr( pos, end - pos );
        trim_string( item );

        list[i] = str_to_fire_types( item );

        if (end == std::string::npos)
            break;
        else
            pos = end + 1;
    }
}

static char str_to_race( const std::string &str )
{
    int index = -1;

    if (str.length() == 1)      // old system of using menu letter
        return (str[0]);
    else if (str.length() == 2) // scan abbreviations
        index = get_species_index_by_abbrev( str.c_str() );

    // if we don't have a match, scan the full names
    if (index == -1)
        index = get_species_index_by_name( str.c_str() );

    // skip over the extra draconians here
    if (index > SP_RED_DRACONIAN)
        index -= (SP_CENTAUR - SP_RED_DRACONIAN - 1);

    // SP_HUMAN is at 1, therefore we must subtract one.
    return ((index != -1) ? index_to_letter( index - 1 ) : 0); 
}

static int str_to_class( const std::string &str )
{
    int index = -1;

    if (str.length() == 1)      // old system of using menu letter
        return (str[0]);
    else if (str.length() == 2) // scan abbreviations
        index = get_class_index_by_abbrev( str.c_str() );

    // if we don't have a match, scan the full names
    if (index == -1)
        index = get_class_index_by_name( str.c_str() );

    return ((index != -1) ? index_to_letter( index ) : 0); 
}

static bool read_bool( const std::string &field, bool def_value )
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
static int read_bool_or_number( const std::string &field, int def_value,
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
    else if (field.find("hi:") == 0 || field.find("hilite:") == 0 ||
             field.find("highlight:") == 0)
    {
        int col = field.find(":");
        int colour = str_to_colour(field.substr(col + 1));
        if (colour == -1)
            fprintf(stderr, "Bad highlight string -- %s\n", field.c_str());
        else
            return CHATTR_HILITE | (colour << 8);          
    }
    else
        fprintf( stderr, "Bad colour -- %s\n", field.c_str() );
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
        {
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
}

void game_options::reset_startup_options()
{
    race                   = 0;
    cls                    = 0;
    weapon                 = WPN_UNKNOWN;
    book                   = SBT_NO_SELECTION;
    random_pick            = false;
    chaos_knight           = GOD_NO_GOD;
    death_knight           = DK_NO_SELECTION;
    priest                 = GOD_NO_GOD;
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
        "interrupt_passwall = interrupt_butcher",
        "interrupt_multidrop = interrupt_butcher",
        "interrupt_macro = interrupt_multidrop",
        "interrupt_travel = interrupt_butcher, statue, hungry, "
                            "burden, monster, hit_monster",
        "interrupt_run = interrupt_travel, message",
        "interrupt_rest = interrupt_run",

        // Stair ascents/descents cannot be interrupted, attempts to interrupt
        // the delay will just trash all queued delays, including travel.
        "interrupt_ascending_stairs =",
        "interrupt_descending_stairs =",
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
            fprintf(stderr, "Unknown delay: %s\n", delay_name.c_str());
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
        fprintf(stderr, "Delay interrupt name \"%s\" not recognised.\n",
                interrupt.c_str());
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
        fprintf(stderr, "Unknown delay: %s\n", activity_name.c_str());
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
    set_default_activity_interrupts();

    reset_startup_options();

#if defined(SAVE_DIR_PATH)
    save_dir  = SAVE_DIR_PATH;
#elif !defined(DOS)
    save_dir = "saves/";
#else
    save_dir.clear();
#endif

#ifndef SHORT_FILE_NAMES
    morgue_dir = "morgue/";
#endif

    player_name.clear();

#ifdef DGL_SIMPLE_MESSAGING
    messaging = true;
#endif

    view_max_width = VIEW_MIN_WIDTH;
    view_max_height = VIEW_MIN_HEIGHT;

    view_lock_x = true;
    view_lock_y = true;

    center_on_scroll = false;
    symmetric_scroll = true;
    scroll_margin_x   = 4;
    scroll_margin_y   = 4;

    autopickup_on = true;
    autoprayer_on = false;
    show_more_prompt = true;

    show_turns = false;

    prev_race = 0;
    prev_cls  = 0;
    prev_ck   = GOD_NO_GOD;
    prev_dk   = DK_NO_SELECTION;
    prev_pr   = GOD_NO_GOD;
    prev_weapon = WPN_UNKNOWN;
    prev_book = SBT_NO_SELECTION;
    prev_randpick = false;
    remember_name = false;

#ifdef USE_ASCII_CHARACTERS
    char_set               = CSET_ASCII;
#else
    char_set               = CSET_IBM;
#endif
    
    // set it to the .crawlrc default
    autopickups = ((1L << 15) | // gold
                   (1L <<  6) | // scrolls
                   (1L <<  8) | // potions
                   (1L << 10) | // books
                   (1L <<  7) | // jewellery
                   (1L <<  3) | // wands
                   (1L <<  4)); // food
    show_inventory_weights = false;
    colour_map             = true;
    clean_map              = false;
    show_uncursed          = true;
    always_greet           = true;
    easy_open              = true;
    easy_unequip           = true;
    easy_butcher           = true;
    easy_confirm           = CONFIRM_SAFE_EASY;
    easy_quit_item_prompts = true;
    hp_warning             = 10;
    magic_point_warning    = 0;
    confirm_self_target    = true;
    default_target         = true;
    safe_autopickup        = true;
    autopickup_no_burden   = false;
    use_notes              = true;
    note_skill_max         = false;
    note_all_spells        = false;
    note_hp_percent        = 5;
    ood_interesting        = 8;
    terse_hand             = true;
    increasing_skill_progress = true;

    // [ds] Grumble grumble.
    auto_list              = true;

    delay_message_clear    = false;
    pickup_dropped         = false;
    pickup_thrown          = true;

    travel_colour          = true;
    travel_delay           = 20;
    travel_stair_cost      = 500;
    travel_exclude_radius2 =  68;

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
    detected_item_colour   = LIGHTGREEN;
    detected_monster_colour= LIGHTRED;

    classic_item_colours   = false;

    easy_exit_menu         = true;
#ifdef DOS
    dos_use_background_intensity = false;
#else
    dos_use_background_intensity = true;
#endif

    level_map_title        = true;

    assign_item_slot       = SS_FORWARD;

    macro_meta_entry       = true;

    // 10 was the cursor step default on Linux.
    level_map_cursor_step  = 7;
    use_fake_cursor        = false;

    stash_tracking         = STM_ALL;

    explore_stop           = ES_ITEM | ES_STAIR | ES_SHOP | ES_ALTAR
                                     | ES_GREEDY_PICKUP;

    // The prompt conditions will be combined into explore_stop after
    // reading options.
    explore_stop_prompt    = ES_NONE;

    explore_item_greed     = 10;
    explore_greedy         = false;
    
    safe_zero_exp          = true;
    target_zero_exp        = false;
    target_wrap            = true;
    target_oos             = true;
    target_los_first       = true;
    target_unshifted_dirs  = false;
    
    dump_kill_places       = KDO_ONE_PLACE;
    dump_message_count     = 7;
    dump_item_origins      = IODS_ARTIFACTS | IODS_RODS;
    dump_item_origin_price = -1;

    drop_mode              = DM_MULTI;
    pickup_mode            = -1;

    flush_input[ FLUSH_ON_FAILURE ]     = true;
    flush_input[ FLUSH_BEFORE_COMMAND ] = false;
    flush_input[ FLUSH_ON_MESSAGE ]     = false;
    flush_input[ FLUSH_LUA ]            = true;

    lowercase_invocations  = true; 

    // Note: These fire options currently match the old behaviour. -- bwr
    fire_items_start       = 0;           // start at slot 'a'

    // Clear fire_order and set up the defaults.
    for (int i = 0; i < NUM_FIRE_TYPES; i++)
        fire_order[i] = FIRE_NONE;
    
    fire_order[0] = FIRE_LAUNCHER;      // fire first from bow...
    fire_order[1] = FIRE_DART;          // then only consider darts
    fire_order[2] = FIRE_STONE;         // and then chuck stones

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

    friend_brand     = CHATTR_NORMAL;
    heap_brand       = CHATTR_NORMAL;
    stab_brand       = CHATTR_NORMAL;
    may_stab_brand   = CHATTR_NORMAL;
    stair_item_brand = CHATTR_REVERSE;
    
    no_dark_brand    = true;

#ifdef WIZARD
    wiz_mode      = WIZ_NO;
#endif

    // map each colour to itself as default
#ifdef USE_8_COLOUR_TERM_MAP
    for (int i = 0; i < 16; i++)
        colour[i] = i % 8;

    colour[ DARKGREY ] = COL_TO_REPLACE_DARKGREY;
#else
    for (int i = 0; i < 16; i++)
        colour[i] = i;
#endif
    
    // map each channel to plain (well, default for now since I'm testing)
    for (int i = 0; i < NUM_MESSAGE_CHANNELS; i++)
        channels[i] = MSGCOL_DEFAULT;

    // Clear vector options.
    extra_levels.clear();
    
    dump_order.clear();
    new_dump_fields("header,stats,misc,inventory,skills,"
                   "spells,,overview,mutations,messages,screenshot,"
                    "kills,notes");

    hp_colour.clear();
    hp_colour.push_back(std::pair<int,int>(100, LIGHTGREY));
    hp_colour.push_back(std::pair<int,int>(50, YELLOW));
    hp_colour.push_back(std::pair<int,int>(25, RED));
    mp_colour.clear();
    mp_colour.push_back(std::pair<int, int>(100, LIGHTGREY));
    mp_colour.push_back(std::pair<int, int>(50, YELLOW));
    mp_colour.push_back(std::pair<int, int>(25, RED));
    never_pickup.clear();
    always_pickup.clear();
    note_monsters.clear(); 
    note_messages.clear(); 
    autoinscriptions.clear();
    note_items.clear();
    note_skill_levels.clear();
    travel_stop_message.clear();
    sound_mappings.clear();
    menu_colour_mappings.clear();
    message_colour_mappings.clear();
    drop_filter.clear();
    map_file_name.clear();
    named_options.clear();

    clear_cset_overrides();
    clear_feature_overrides();

    // Map each category to itself. The user can override in init.txt
    kill_map[KC_YOU] = KC_YOU;
    kill_map[KC_FRIENDLY] = KC_FRIENDLY;
    kill_map[KC_OTHER] = KC_OTHER;

    // Setup travel information. What's a better place to do this?
    initialise_travel();
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

    if (s.length() == 1)
        return s[0];

    if (s.length() > 1 && s[0] == '\\')
        s = s.substr(1);

    int base = 10;
    if (s.length() > 1 && s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return (strtoul(s.c_str(), &tail, base));
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

    if (iprops.size() < 1 || iprops.size() > 5)
        return;

    if (iprops.size() < 5)
        iprops.resize(5);

    trim_string(fname);
    std::vector<dungeon_feature_type> feats = features_by_desc(fname);
    if (feats.empty())
        return;

    for (int i = 0, size = feats.size(); i < size; ++i)
    {
        feature_override fov;
        fov.feat = feats[i];
        
        fov.override.symbol         = read_symbol(iprops[0]);
        fov.override.magic_symbol   = read_symbol(iprops[1]);
        fov.override.colour         = str_to_colour(iprops[2], BLACK);
        fov.override.map_colour     = str_to_colour(iprops[3], BLACK);
        fov.override.seen_colour    = str_to_colour(iprops[4], BLACK);

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

// returns where the init file was read from
std::string read_init_file(bool runscript)
{
    const char* locations_data[][2] = {
        { SysEnv.crawl_rc.c_str(), "" },
        { SysEnv.crawl_dir.c_str(), "init.txt" },
#ifdef MULTIUSER
        { SysEnv.home.c_str(), "/.crawlrc" },
        { SysEnv.home.c_str(), "init.txt" },
#endif
        { "", "init.txt" },
#ifdef WIN32CONSOLE
        { "", ".crawlrc" },
        { "", "_crawlrc" },
#endif
        { NULL, NULL }                // placeholder to mark end
    };

    Options.reset_options();

    FILE* f = NULL;
    char name_buff[kPathLen];
    // Check all possibilities for init.txt
    for ( int i = 0; f == NULL && locations_data[i][1] != NULL; ++i ) {
        // Don't look at unset options
        if ( locations_data[i][0] != NULL ) {
            snprintf( name_buff, sizeof name_buff, "%s%s",
                      locations_data[i][0], locations_data[i][1] );
            f = fopen( name_buff, "r" );
        }
    }

    if ( f == NULL )
    {
#ifdef MULTIUSER
        return "not found (~/.crawlrc missing)";
#else
        return "not found (init.txt missing from current directory)";
#endif
    }

    read_options(f, runscript);
    if (!runscript)
        read_startup_prefs();

    fclose(f);
    return std::string(name_buff);
}                               // end read_init_file()

static void read_startup_prefs()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    std::string fn = get_prefs_filename();
    FILE *f = fopen(fn.c_str(), "r");
    if (!f)
        return;

    game_options temp;
    FileLineInput fl(f);
    temp.read_options(fl, false);
    fclose(f);

    Options.prev_randpick = temp.random_pick;
    Options.prev_weapon   = temp.weapon;
    Options.prev_pr       = temp.priest;
    Options.prev_dk       = temp.death_knight;
    Options.prev_ck       = temp.chaos_knight;
    Options.prev_cls      = temp.cls;
    Options.prev_race     = temp.race;
    Options.prev_book     = temp.book;
    Options.prev_name     = temp.player_name;
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

#ifndef DISABLE_STICKY_STARTUP_OPTIONS
static void write_newgame_options(FILE *f)
{
    // Write current player name
    fprintf(f, "name = %s\n", you.your_name);

    if (Options.prev_randpick)
        Options.prev_race = Options.prev_cls = '?';

    // Race selection
    if (Options.prev_race)
        fprintf(f, "race = %c\n", Options.prev_race);
    if (Options.prev_cls)
        fprintf(f, "class = %c\n", Options.prev_cls);

    if (Options.prev_weapon != WPN_UNKNOWN)
        fprintf(f, "weapon = %s\n", weapon_to_str(Options.prev_weapon).c_str());

    if (Options.prev_ck != GOD_NO_GOD)
    {
        fprintf(f, "chaos_knight = %s\n",
                Options.prev_ck == GOD_XOM? "xom" :
                Options.prev_ck == GOD_MAKHLEB? "makhleb" :
                                            "random");
    }
    if (Options.prev_dk != DK_NO_SELECTION)
    {
        fprintf(f, "death_knight = %s\n",
                Options.prev_dk == DK_NECROMANCY? "necromancy" :
                Options.prev_dk == DK_YREDELEMNUL? "yredelemnul" :
                                            "random");
    }
    if (is_priest_god(Options.prev_pr) || Options.prev_pr == GOD_RANDOM)
    {
        fprintf(f, "priest = %s\n",
                lowercase_string(god_name(Options.prev_pr)).c_str());
    }

    if (Options.prev_book != SBT_NO_SELECTION )
    {
        fprintf(f, "book = %s\n",
                Options.prev_book == SBT_FIRE ? "fire" :
                Options.prev_book == SBT_COLD ? "cold" :
                Options.prev_book == SBT_SUMM ? "summ" :
                "random");
    }
}
#endif // !DISABLE_STICKY_STARTUP_OPTIONS

void write_newgame_options_file()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    std::string fn = get_prefs_filename();
    FILE *f = fopen(fn.c_str(), "w");
    if (!f)
        return;
    write_newgame_options(f);
    fclose(f);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

void save_player_name()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    if (!Options.remember_name)
        return ;

    // Read other preferences
    read_startup_prefs();

    // And save
    write_newgame_options_file();
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}
 
void read_options(FILE *f, bool runscript)
{
    FileLineInput fl(f);
    Options.read_options(fl, runscript);
}

void read_options(const std::string &s, bool runscript)
{
    StringLineInput st(s);
    Options.read_options(st, runscript);
}

game_options::game_options()
{
    reset_options();
}

void game_options::read_options(InitLineInput &il, bool runscript)
{
    unsigned int line = 0;
    
    bool inscriptblock = false;
    bool inscriptcond  = false;
    bool isconditional = false;

    bool l_init        = false;

    aliases.clear();
    
    std::string luacond;
    std::string luacode;
    while (!il.eof())
    {
        std::string s   = il.getline();
        std::string str = s;
        line++;

        trim_string( str );

        // This is to make some efficient comments
        if ((str.empty() || str[0] == '#') && !inscriptcond && !inscriptblock)
            continue;

        if (!inscriptcond && (str.find("L<") == 0 || str.find("<") == 0))
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            inscriptcond  = true;

            str = str.substr( str.find("L<") == 0? 2 : 1 );
            // Is this a one-liner?
            if (!str.empty() && str[ str.length() - 1 ] == '>') {
                inscriptcond = false;
                str = str.substr(0, str.length() - 1);
            }

            if (!str.empty() && runscript)
            {
                // If we're in the middle of an option block, close it.
                if (luacond.length() && l_init)
                {
                    luacond += "]] )\n";
                    l_init = false;
                }
                luacond += str + "\n";
            }
            continue;
        }
        else if (inscriptcond && 
                (str.find(">") == str.length() - 1 || str == ">L"))
        {
            inscriptcond = false;
            str = str.substr(0, str.length() - 1);
            if (!str.empty() && runscript)
                luacond += str + "\n";
            continue;
        }
        else if (inscriptcond)
        {
            if (runscript)
                luacond += s + "\n";
            continue;
        }

        // Handle blocks of Lua
        if (!inscriptblock && (str.find("Lua{") == 0 || str.find("{") == 0))
        {
            inscriptblock = true;
            luacode.clear();

            // Strip leading Lua[
            str = str.substr( str.find("Lua{") == 0? 4 : 1 );

            if (!str.empty() && str.find("}") == str.length() - 1)
            {
                str = str.substr(0, str.length() - 1);
                inscriptblock = false;
            }

            if (!str.empty())
                luacode += str + "\n";

            if (!inscriptblock && runscript)
            {
#ifdef CLUA_BINDINGS
                clua.execstring(luacode.c_str());
                if (clua.error.length())
                    fprintf(stderr, "Lua error: %s\n", clua.error.c_str());
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
                clua.execstring(luacode.c_str());
                if (clua.error.length())
                    fprintf(stderr, "Lua error: %s\n",
                            clua.error.c_str());
            }
#endif
            luacode.clear();
            continue;
        }
        else if (inscriptblock)
        {
            luacode += s + "\n";
            continue;
        }

        if (isconditional && runscript)
        {
            if (!l_init)
            {
                luacond += "crawl.setopt( [[\n";
                l_init = true;
            }

            luacond += s + "\n";
            continue;
        }

        read_option_line(str, runscript);
    }

#ifdef CLUA_BINDINGS
    if (runscript && !luacond.empty())
    {
        if (l_init)
            luacond += "]] )\n";
        clua.execstring(luacond.c_str());
        if (clua.error.length())
        {
            mpr( ("Lua error: " + clua.error).c_str() );
        }
    }
#endif

    Options.explore_stop |= Options.explore_stop_prompt;

    validate_options();
}

void game_options::validate_options()
{
    // Validate save_dir
    if (!check_dir("Save directory", save_dir))
        end(1);

    if (!check_dir("Morgue directory", morgue_dir))
        end(1);
}

static int str_to_killcategory(const std::string &s)
{
   static const char *kc[] = {
       "you",
       "friend",
       "other",
   };

   for (unsigned i = 0; i < sizeof(kc) / sizeof(*kc); ++i) {
       if (s == kc[i])
           return i;
   }
   return -1;
}

void game_options::do_kill_map(const std::string &from, const std::string &to)
{
    int ifrom = str_to_killcategory(from),
        ito   = str_to_killcategory(to);
    if (ifrom != -1 && ito != -1)
        kill_map[ifrom] = ito;
}

int game_options::read_explore_stop_conditions(const std::string &field) const
{
    int conditions = 0;
    std::vector<std::string> stops = split_string(",", field);
    for (int i = 0, count = stops.size(); i < count; ++i)
    {
        const std::string &c = stops[i];
        if (c == "item" || c == "items")
            conditions |= ES_ITEM;
        else if (c == "pickup")
            conditions |= ES_PICKUP;
        else if (c == "greedy_pickup" || c == "greedy pickup")
            conditions |= ES_GREEDY_PICKUP;
        else if (c == "shop" || c == "shops")
            conditions |= ES_SHOP;
        else if (c == "stair" || c == "stairs")
            conditions |= ES_STAIR;
        else if (c == "altar" || c == "altars")
            conditions |= ES_ALTAR;
    }
    return (conditions);
}

void game_options::add_alias(const std::string &key, const std::string &val)
{
    aliases[key] = val;
}

std::string game_options::unalias(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator i = aliases.find(key);
    return (i == aliases.end()? key : i->second);
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

    const int col = (cmap[0]=="mute") ? MSGCOL_MUTED : str_to_colour(cmap[0]);
    if (col == -1)
        return;

    message_colour_mapping m = { parse_message_filter( cmap[1] ), col };
    message_colour_mappings.push_back( m );
}

// Option syntax is:
// sort_menu = [menu_type:]yes|no|auto:n[:sort_conditions]
void game_options::set_menu_sort(std::string field)
{
    if (field.empty())
        return;
    
    menu_sort_condition cond(field);
    sort_menus.push_back(cond);
}

void game_options::read_option_line(const std::string &str, bool runscript)
{
    std::string key = "";
    std::string subkey = "";
    std::string field = "";

    bool plus_equal = false;
    bool minus_equal = false;

    const int first_equals = str.find('=');

    // all lines with no equal-signs we ignore
    if (first_equals < 0)
        return;

    field  = str.substr( first_equals + 1 );
    
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
    std::string orig_field = field;

    if (key != "name" && key != "crawl_dir" 
        && key != "race" && key != "class" && key != "ban_pickup"
        && key != "stop_travel" && key != "sound" 
        && key != "travel_stop_message"
        && key != "drop_filter" && key != "lua_file"
        && key != "note_items" && key != "autoinscribe"
        && key != "note_monsters" && key != "note_messages"
        && key.find("cset") != 0 && key != "dungeon"
        && key != "feature" && key != "fire_items_start"
        && key != "menu_colour" && key != "menu_color"
        && key != "message_colour" && key != "message_color"
        && key != "levels" && key != "level" && key != "entries")
    {
        lowercase( field );
    }

    // everything not a valid line is treated as a comment
    if (key == "autopickup")
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
                fprintf( stderr, "Bad object type '%c' for autopickup.\n",
                         type );
            }
        }
    }
    else if (key == "name")
    {
        // field is already cleaned up from trim_string()
        player_name = field;
    }
    else if (key == "char_set" || key == "ascii_display")
    {
        bool valid = true;

        if (key == "ascii_display")
        {
            char_set = 
                read_bool(field, char_set == CSET_ASCII)?
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
    else if (key == "default_autopickup")
    {
        // should autopickup default to on or off?
        autopickup_on = read_bool( field, autopickup_on );
    }
    else if (key == "default_autoprayer")
    {
        // should autoprayer default to on or off?
        autoprayer_on = read_bool( field, autoprayer_on );
    }
    else if (key == "show_inventory_weights")
    {
        // should weights be shown on inventory items?
        show_inventory_weights = read_bool( field, show_inventory_weights );
    }
    else if (key == "clean_map")
    {
        // removes monsters/clouds from map
        clean_map = read_bool( field, clean_map );
    }
    else if (key == "colour_map" || key == "color_map")
    {
        // colour-codes play-screen map
        colour_map = read_bool( field, colour_map );
    }
    else if (key == "easy_confirm")
    {
        // allows both 'Y'/'N' and 'y'/'n' on yesno() prompts
        if (field == "none")
            easy_confirm = CONFIRM_NONE_EASY;
        else if (field == "safe")
            easy_confirm = CONFIRM_SAFE_EASY;
        else if (field == "all")
            easy_confirm = CONFIRM_ALL_EASY;
    }
    else if (key == "easy_quit_item_lists"
            || key == "easy_quit_item_prompts")
    {
        // allow aborting of item lists with space
        easy_quit_item_prompts = read_bool( field, 
                                        easy_quit_item_prompts );
    }
    else if (key == "easy_open")
    {
        // automatic door opening with movement
        easy_open = read_bool( field, easy_open );
    }
    else if (key == "easy_armor" 
            || key == "easy_armour" 
            || key == "easy_unequip")
    {
        // automatic removal of armour when dropping
        easy_unequip = read_bool( field, easy_unequip );
    }
    else if (key == "easy_butcher")
    {
        // automatic knife switching
        easy_butcher = read_bool( field, easy_butcher );
    }
    else if (key == "lua_file" && runscript)
    {
#ifdef CLUA_BINDINGS
        const std::string lua_file = datafile_path(field, false, true);
        if (lua_file.empty())
        {
            fprintf(stderr, "Unable to find lua file: %s\n", field.c_str());
        }
        else
        {
            clua.execfile(lua_file.c_str());
            if (clua.error.length())
                fprintf(stderr, "Lua error: %s\n", clua.error.c_str());
        }
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
        const int col  = str_to_channel_colour( field );

        if (chnl != -1 && col != -1)
            channels[chnl] = col;
        else if (chnl == -1)
            fprintf( stderr, "Bad channel -- %s\n", subkey.c_str() );
        else if (col == -1)
            fprintf( stderr, "Bad colour -- %s\n", field.c_str() );
    }
    else if (key == "background")
    {
        // change background colour
        // Experimental! This may look really bad!
        const int col = str_to_colour( field );

        if (col != -1)
            background = col;
        else
            fprintf( stderr, "Bad colour -- %s\n", field.c_str() );

    }
    else if (key == "detected_item_colour")
    {
        const int col = str_to_colour( field );
        if (col != -1)
            detected_item_colour = col;
        else
            fprintf( stderr, "Bad detected_item_colour -- %s\n",
                     field.c_str());
    }
    else if (key == "detected_monster_colour")
    {
        const int col = str_to_colour( field );
        if (col != -1)
            detected_monster_colour = col;
        else
            fprintf( stderr, "Bad detected_monster_colour -- %s\n",
                     field.c_str());
    }
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
        std::vector<std::string> defs = split_string(";", field);
        
        for (int i = 0, size = defs.size(); i < size; ++i)
            add_feature_override( defs[i] );
    }
    else if (key == "friend_brand")
    {
        // Use curses attributes to mark friend
        // Some may look bad on some terminals.
        // As a suggestion, try "rxvt -rv -fn 10x20" under Un*xes
        friend_brand = curses_attribute(field);
    }
    else if (key == "levels" || key == "level" || key == "entries")
    {
        extra_levels.push_back(field);
    }
    else if (key == "stab_brand")
    {
        stab_brand = curses_attribute(field);
    }
    else if (key == "may_stab_brand")
    {
        may_stab_brand = curses_attribute(field);
    }
    else if (key == "stair_item_brand")
    {
        stair_item_brand = curses_attribute(field);
    }
    else if (key == "no_dark_brand")
    {
        // This is useful for terms where dark grey does
        // not have standout modes (since it's black on black).
        // This option will use light-grey instead in these cases.
        no_dark_brand = read_bool( field, no_dark_brand );
    }
    else if (key == "heap_brand")
    {
        // See friend_brand option upstairs. no_dark_brand applies
        // here as well.
        heap_brand = curses_attribute(field);
    }
    else if (key == "always_greet")
    {
        // show greeting when reloading game
        always_greet = read_bool( field, always_greet );
    }
    else if (key == "weapon")
    {
        // choose this weapon for classes that get choice
        weapon = str_to_weapon( field );
    }
    else if (key == "book")
    {
        // choose this book for classes that get choice
        book = str_to_book( field );
    }
    else if (key == "chaos_knight")
    {
        // choose god for Chaos Knights
        if (field == "xom")
            chaos_knight = GOD_XOM;
        else if (field == "makhleb")
            chaos_knight = GOD_MAKHLEB;
        else if (field == "random")
            chaos_knight = GOD_RANDOM;
    }
    else if (key == "death_knight")
    {
        if (field == "necromancy")
            death_knight = DK_NECROMANCY;
        else if (field == "yredelemnul")
            death_knight = DK_YREDELEMNUL;
        else if (field == "random")
            death_knight = DK_RANDOM;
    }
    else if (key == "priest")
    {
        // choose this weapon for classes that get choice
        priest = str_to_god(field);
        if (!is_priest_god(priest))
            priest = GOD_RANDOM;
    }
    else if (key == "fire_items_start")
    {
        if (isalpha( field[0] ))
            fire_items_start = letter_to_index( field[0] ); 
        else
        {
            fprintf( stderr, "Bad fire item start index -- %s\n",
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
        str_to_fire_order( field, fire_order );
    }
    else if (key == "random_pick")
    {
        // randomly generate character
        random_pick = read_bool( field, random_pick );
    }
    else if (key == "remember_name")
    {
        remember_name = read_bool( field, remember_name );
    }
    else if (key == "save_dir")
    {
        // If SAVE_DIR_PATH was defined, there are very likely security issues
        // with allowing the user to specify a different directory.
#ifndef SAVE_DIR_PATH
        save_dir = field;
#endif
    }
    else if (key == "show_turns")
    {
        show_turns = read_bool( field, show_turns );
    }
    else if (key == "morgue_dir")
    {
        morgue_dir = field;
    }
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
    else if (key == "ood_interesting")
    {
        ood_interesting = atoi( field.c_str() );
    }
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
    else if (key == "crawl_dir")
    {
        // We shouldn't bother to allocate this a second time
        // if the user puts two crawl_dir lines in the init file.
        SysEnv.crawl_dir = field;
    }
    else if (key == "race")
    {
        race = str_to_race( field );

        if (race == 0)
            fprintf( stderr, "Unknown race choice: %s\n", field.c_str() );
    }
    else if (key == "class")
    {
        cls = str_to_class( field );

        if (cls == 0)
            fprintf( stderr, "Unknown class choice: %s\n", field.c_str() );
    }
    else if (key == "auto_list")
    {
        auto_list = read_bool( field, auto_list );
    }
    else if (key == "confirm_self_target")
    {
        confirm_self_target = read_bool( field, confirm_self_target );
    }
    else if (key == "default_target")
    {
        default_target = read_bool( field, default_target );
        if (default_target)
            target_unshifted_dirs = false;
    }
    else if (key == "safe_autopickup")
    {
        safe_autopickup = read_bool( field, safe_autopickup );
    }
    else if (key == "autopickup_no_burden")
    {
        autopickup_no_burden = read_bool( field, autopickup_no_burden );
    }
#ifdef DGL_SIMPLE_MESSAGING
    else if (key == "messaging")
    {
        messaging = read_bool(field, messaging);
    }
#endif
    else if (key == "view_max_width")
    {
        view_max_width = atoi(field.c_str());
        if (view_max_width < VIEW_MIN_WIDTH)
            view_max_width = VIEW_MIN_WIDTH;

        // Allow the view to be one larger than GXM because the view width
        // needs to be odd, and GXM is even.
        else if (view_max_width > GXM + 1)
            view_max_width = GXM + 1;
    }
    else if (key == "view_max_height")
    {
        view_max_height = atoi(field.c_str());
        if (view_max_height < VIEW_MIN_HEIGHT)
            view_max_height = VIEW_MIN_HEIGHT;
        else if (view_max_height > GYM + 1)
            view_max_height = GYM + 1;
    }
    else if (key == "view_lock_x")
    {
        view_lock_x = read_bool(field, view_lock_x);
    }
    else if (key == "view_lock_y")
    {
        view_lock_y = read_bool(field, view_lock_y);
    }
    else if (key == "view_lock")
    {
        const bool lock = read_bool(field, true);
        view_lock_x = view_lock_y = lock;
    }
    else if (key == "center_on_scroll")
    {
        center_on_scroll = read_bool(field, center_on_scroll);
    }
    else if (key == "symmetric_scroll")
    {
        symmetric_scroll = read_bool(field, symmetric_scroll);
    }
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
    else if (key == "use_notes")
    {
        use_notes = read_bool( field, use_notes );
    }
    else if (key == "note_skill_max")
    {
        note_skill_max = read_bool( field, note_skill_max );
    }
    else if (key == "note_all_spells")
    {
        note_all_spells = read_bool( field, note_all_spells );
    }
    else if (key == "delay_message_clear")
    {
        delay_message_clear = read_bool( field, delay_message_clear );
    }
    else if (key == "terse_hand")
    {
        terse_hand = read_bool( field, terse_hand );
    }
    else if (key == "increasing_skill_progress")
    {
        increasing_skill_progress = read_bool( field, increasing_skill_progress );
    }
    else if (key == "flush")
    {
        if (subkey == "failure")
        {
            flush_input[FLUSH_ON_FAILURE] 
                = read_bool(field, flush_input[FLUSH_ON_FAILURE]);
        }
        else if (subkey == "command")
        {
            flush_input[FLUSH_BEFORE_COMMAND] 
                = read_bool(field, flush_input[FLUSH_BEFORE_COMMAND]);
        }
        else if (subkey == "message")
        {
            flush_input[FLUSH_ON_MESSAGE] 
                = read_bool(field, flush_input[FLUSH_ON_MESSAGE]);
        }
        else if (subkey == "lua")
        {
            flush_input[FLUSH_LUA] 
                = read_bool(field, flush_input[FLUSH_LUA]);
        }
    }
    else if (key == "lowercase_invocations")
    {
        lowercase_invocations 
                = read_bool(field, lowercase_invocations);
    }
    else if (key == "wiz_mode")
    {
        // wiz_mode is recognized as a legal key in all compiles -- bwr
#ifdef WIZARD
        if (field == "never")
            wiz_mode = WIZ_NEVER;
        else if (field == "no")
            wiz_mode = WIZ_NO;
        else if (field == "yes")
            wiz_mode = WIZ_YES;
        else
            fprintf(stderr, "Unknown wiz_mode option: %s\n", field.c_str());
#endif
    }
    else if (key == "ban_pickup")
    {
        append_vector(never_pickup, split_string(",", field));
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
                never_pickup.push_back( s.substr(1) );
            else if (s[0] == '<')
                always_pickup.push_back( s.substr(1) );
            else
                never_pickup.push_back( s );
        }
    }
    else if (key == "note_items")
    {
        append_vector(note_items, split_string(",", field));
    }
    else if (key == "autoinscribe")
    {
        std::vector<std::string> thesplit = split_string(":", field);
        autoinscriptions.push_back(
            std::pair<text_pattern,std::string>(thesplit[0],
                                                thesplit[1]));
    }
    else if (key == "map_file_name")
    {
        map_file_name = field;
    }
    else if (key == "hp_colour" || key == "hp_color")
    {
        hp_colour.clear();
        std::vector<std::string> thesplit = split_string(",", field);
        for ( unsigned i = 0; i < thesplit.size(); ++i )
        {
            std::vector<std::string> insplit = split_string(":", thesplit[i]);
            int hp_percent = 100;

            if ( insplit.size() == 0 || insplit.size() > 2 ||
                 (insplit.size() == 1 && i != 0) )
            {
                fprintf(stderr, "Bad hp_colour string: %s\n", field.c_str());
                break;
            }

            if ( insplit.size() == 2 )
                hp_percent = atoi(insplit[0].c_str());

            int scolour = str_to_colour(insplit[(insplit.size()==1) ? 0 : 1]);
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

            if ( insplit.size() == 0 || insplit.size() > 2 ||
                 (insplit.size() == 1 && i != 0) )
            {
                fprintf(stderr, "Bad mp_colour string: %s\n", field.c_str());
                break;
            }

            if ( insplit.size() == 2 )
                mp_percent = atoi(insplit[0].c_str());

            int scolour = str_to_colour(insplit[(insplit.size()==1) ? 0 : 1]);
            mp_colour.push_back(std::pair<int, int>(mp_percent, scolour));
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
                fprintf(stderr, "Bad skill level to note -- %s\n",
                        thesplit[i].c_str());
                continue;
            }
        }
    }
    else if (key == "pickup_thrown")
    {
        pickup_thrown = read_bool(field, pickup_thrown);
    }
    else if (key == "pickup_dropped")
    {
        pickup_dropped = read_bool(field, pickup_dropped);
    }
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
    else if (key == "level_map_cursor_step")
    {
        level_map_cursor_step = atoi( field.c_str() );
        if (level_map_cursor_step < 1)
            level_map_cursor_step = 1;
        if (level_map_cursor_step > 50)
            level_map_cursor_step = 50;
    }
    else if (key == "use_fake_cursor")
    {
        use_fake_cursor = read_bool(field, use_fake_cursor);
    }
    else if (key == "macro_meta_entry")
    {
        macro_meta_entry = read_bool(field, macro_meta_entry);
    }
    else if (key == "travel_exclude_radius2")
    {
        travel_exclude_radius2 = atoi( field.c_str() );
        if (travel_exclude_radius2 < 0)
            travel_exclude_radius2 = 0;
        else if (travel_exclude_radius2 > 400)
            travel_exclude_radius2 = 400;
    }
    else if (key == "stop_travel" || key == "travel_stop_message")
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
                    travel_stop_message.push_back(
                       message_filter( channel, s ) );
                    continue;
                }
            }

            travel_stop_message.push_back(
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
    else if (key == "classic_item_colours")
    {
        classic_item_colours = read_bool(field, classic_item_colours);
    }
    else if (key == "item_colour" || key == "item_color")
    {
        item_colour = read_bool(field, item_colour);
    }
    else if (key == "easy_exit_menu")
    {
        easy_exit_menu = read_bool(field, easy_exit_menu);
    }
    else if (key == "dos_use_background_intensity")
    {
        dos_use_background_intensity = 
            read_bool(field, dos_use_background_intensity);
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
    else if (key == "explore_item_greed")
    {
        explore_item_greed = atoi( field.c_str() );
        if (explore_item_greed > 1000)
            explore_item_greed = 1000;
        else if (explore_item_greed < -1000)
            explore_item_greed = -1000;
    }
    else if (key == "explore_greedy")
    {
        explore_greedy = read_bool(field, explore_greedy);
    }
    else if (key == "stash_tracking")
    {
        stash_tracking =
             field == "dropped" ? STM_DROPPED  :
             field == "all"     ? STM_ALL      :
                                  STM_EXPLICIT;
    }
    else if (key == "stash_filter")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
            Stash::filter( seg[i] );
    }
    else if (key == "sound")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i) {
            const std::string &sub = seg[i];
            std::string::size_type cpos = sub.find(":", 0);
            if (cpos != std::string::npos) {
                sound_mapping mapping;
                mapping.pattern = sub.substr(0, cpos);
                mapping.soundfile = sub.substr(cpos + 1);
                sound_mappings.push_back(mapping);
            }
        }
    }
    else if (key == "menu_colour" || key == "menu_color")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const std::string &sub = seg[i];
            std::string::size_type cpos = sub.find(":", 0);
            if (cpos != std::string::npos)
            {
                colour_mapping mapping;
                mapping.pattern = sub.substr(cpos + 1);
                mapping.colour  = str_to_colour(sub.substr(0, cpos));
                if (mapping.colour != -1)
                    menu_colour_mappings.push_back(mapping);
            }
        }
    }
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
        dump_kill_places =
            field == "none"? KDO_NO_PLACES :
            field == "all" ? KDO_ALL_PLACES :
                             KDO_ONE_PLACE;
    }
    else if (key == "kill_map")
    {
        std::vector<std::string> seg = split_string(",", field);
        for (int i = 0, count = seg.size(); i < count; ++i)
        {
            const std::string &s = seg[i];
            std::string::size_type cpos = s.find(":", 0);
            if (cpos != std::string::npos) {
                std::string from = s.substr(0, cpos);
                std::string to   = s.substr(cpos + 1);
                do_kill_map(from, to);
            }
        }
    }
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
            if (ch == "artifacts")
                dump_item_origins |= IODS_ARTIFACTS;
            else if (ch == "ego_arm" || ch == "ego armour" 
                    || ch == "ego_armour")
                dump_item_origins |= IODS_EGO_ARMOUR;
            else if (ch == "ego_weap" || ch == "ego weapon" 
                    || ch == "ego_weapon" || ch == "ego weapons"
                    || ch == "ego_weapons")
                dump_item_origins |= IODS_EGO_WEAPON;
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
    else if (key == "level_map_title")
    {
        level_map_title = read_bool(field, level_map_title);
    }
    else if (key == "safe_zero_exp")
    {
        safe_zero_exp = read_bool(field, safe_zero_exp);
    }
    else if (key == "target_zero_exp")
    {
        target_zero_exp = read_bool(field, target_zero_exp);
    }
    else if (key == "target_oos")
    {
        target_oos = read_bool(field, target_oos);
    }
    else if (key == "target_los_first")
    {
        target_los_first = read_bool(field, target_los_first);
    }
    else if (key == "target_unshifted_dirs")
    {
        target_unshifted_dirs = read_bool(field, target_unshifted_dirs);
        if (target_unshifted_dirs)
            default_target = false;
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
            pickup_mode = read_bool_or_number(field, pickup_mode, "auto:");
    }
    // Catch-all else, copies option into map
    else
    {
#ifdef CLUA_BINDINGS
        if (!clua.callbooleanfn(false, "c_process_lua_option", "ss", 
                        key.c_str(), orig_field.c_str()))
#endif
        {
#ifdef CLUA_BINDINGS
            if (clua.error.length())
                mpr(clua.error.c_str());
#endif
            named_options[key] = orig_field;
        }
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

    // The player's pizza
    SysEnv.crawl_pizza = check_string( getenv("CRAWL_PIZZA") );

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

    // The full path to the init file -- this over-rides CRAWL_DIR
    SysEnv.crawl_rc = check_string( getenv("CRAWL_RC") );

    // rename giant and giant spiked clubs
    SysEnv.board_with_nail = (getenv("BOARD_WITH_NAIL") != NULL);

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
    CLO_PIZZA,
    CLO_PLAIN,
    CLO_DIR,
    CLO_RC,
    CLO_TSCORES,
    CLO_VSCORES,
    CLO_SCOREFILE,
    CLO_MORGUE,
    CLO_MACRO,

    CLO_NOPS
};

static const char *cmd_ops[] = { "scores", "name", "race", "class",
                                 "pizza", "plain", "dir", "rc", "tscores",
                                 "vscores", "scorefile", "morgue",
                                 "macro" };

const int num_cmd_ops = CLO_NOPS;
bool arg_seen[num_cmd_ops];

bool parse_args( int argc, char **argv, bool rc_only )
{
    set_crawl_base_dir(argv[0]);
    
    if (argc < 2)           // no args!
        return (true);

    char *arg, *next_arg;
    int current = 1;
    bool nextUsed = false;
    int ecount;

    // initialize
    for(int i=0; i<num_cmd_ops; i++)
        arg_seen[i] = false;

    while(current < argc)
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

        // look for match (now we also except --scores)
        if (arg[1] == '-')
            arg = &arg[2];
        else
            arg = &arg[1];

        int o;
        for(o = 0; o < num_cmd_ops; o++)
        {
            if (stricmp(cmd_ops[o], arg) == 0)
                break;
        }

        if (o == num_cmd_ops)
        {
            fprintf(stderr,
                    "Unknown option: %s\n\n", argv[current]);
            return (false);
        }

        // disallow options specified more than once.
        if (arg_seen[o] == true)
            return (false);

        // set arg to 'seen'
        arg_seen[o] = true;

        // partially parse next argument
        bool next_is_param = false;
        if (next_arg != NULL)
        {
            if (next_arg[0] != '-' || strlen(next_arg) == 1)
                next_is_param = true;
        }

        //.take action according to the cmd chosen
        switch(o)
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
                Options.morgue_dir = next_arg;
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
                Options.player_name = next_arg;
            nextUsed = true;
            break;

        case CLO_RACE:
        case CLO_CLASS:
            if (!next_is_param)
                return (false);

            // if (strlen(next_arg) != 1)
            //    return (false);

            if (!rc_only)
            {
                if (o == 2)
                    Options.race = str_to_race( std::string( next_arg ) );

                if (o == 3)
                    Options.cls = str_to_class( std::string( next_arg ) );
            }
            nextUsed = true;
            break;

        case CLO_PIZZA:
            if (!next_is_param)
                return (false);

            if (!rc_only)
                SysEnv.crawl_pizza = next_arg;

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

        case CLO_DIR:
            // ALWAYS PARSE
            if (!next_is_param)
                return (false);

            SysEnv.crawl_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_RC:
            // ALWAYS PARSE
            if (!next_is_param)
                return (false);

            SysEnv.crawl_rc = next_arg;
            nextUsed = true;
            break;
        } // end switch -- which option?

        // update position
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
        val = read_bool(i->second, val);
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
          { "pickup:", MT_PICKUP    }
      };

    for (unsigned mi = 0; mi < ARRAYSIZE(menu_type_map); ++mi)
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
    sort = read_bool_or_number(sort_cond, sort, "auto:");

    if (trail_pos != std::string::npos)
        s = s.substr(trail_pos + 1);
    else
        s.clear();
}

void menu_sort_condition::set_comparators(std::string &s)
{
    init_item_sort_comparators(
        cmp,
        s.empty()? "basename, qualname, curse, qty" : s);
}

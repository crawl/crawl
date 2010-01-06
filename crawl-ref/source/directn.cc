/*
 *  File:       direct.cc
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "directn.h"
#include "format.h"

#include <cstdarg>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"
#include "options.h"

#include "cio.h"
#include "cloud.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-util.h"
#include "debug.h"
#include "describe.h"
#include "dungeon.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "invent.h"
#include "itemname.h"
#include "items.h"
#include "l_defs.h"
#include "los.h"
#include "macro.h"
#include "mapmark.h"
#include "message.h"
#include "menu.h"
#include "misc.h"
#include "mon-stuff.h"
#include "mon-info.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "shopping.h"
#include "show.h"
#include "showsymb.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "areas.h"
#include "stash.h"
#ifdef USE_TILE
 #include "tiles.h"
 #include "tilereg.h"
#endif
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "wiz-mon.h"


#define SHORT_DESC_KEY "short_desc_key"

typedef std::map<std::string, std::string> desc_map;

static desc_map base_desc_to_short;

enum LOSSelect
{
    LOS_ANY      = 0x00,

    // Check only visible squares
    LOS_VISIBLE  = 0x01,

    // Check only hidden squares
    LOS_HIDDEN   = 0x02,

    LOS_VISMASK  = 0x03,

    // Flip from visible to hidden when going forward,
    // or hidden to visible when going backwards.
    LOS_FLIPVH   = 0x20,

    // Flip from hidden to visible when going forward,
    // or visible to hidden when going backwards.
    LOS_FLIPHV   = 0x40,

    LOS_NONE     = 0xFFFF
};

static void _describe_feature(const coord_def& where, bool oos);
static void _describe_cell(const coord_def& where, bool in_range = true);

static bool _find_object(  const coord_def& where, int mode, bool need_path,
                           int range );
static bool _find_monster( const coord_def& where, int mode, bool need_path,
                           int range );
static bool _find_feature( const coord_def& where, int mode, bool need_path,
                           int range );

#ifndef USE_TILE
static bool _find_mlist( const coord_def& where, int mode, bool need_path,
                         int range );
#endif

static char _find_square_wrapper(coord_def &mfp, char direction,
                                 bool (*find_targ)(const coord_def&, int, bool, int),
                                 bool need_path, int mode,
                                 int range, bool wrap,
                                 int los = LOS_ANY);

static char _find_square(coord_def &mfp, int direction,
                         bool (*find_targ)(const coord_def&, int, bool, int),
                         bool need_path, int mode, int range,
                         bool wrap, int los = LOS_ANY);

static int  _targetting_cmd_to_compass( command_type command );
static void _describe_oos_square(const coord_def& where);
static void _extend_move_to_edge(dist &moves);
static std::string _get_monster_desc(const monsters *mon);

dist::dist()
    : isValid(false), isTarget(false), isMe(false), isEndpoint(false),
      isCancel(true), choseRay(false), target(), delta(), ray(),
      prev_target(MHITNOT)
{
}

void direction_choose_compass( dist& moves, targetting_behaviour *beh)
{
    moves.isValid       = true;
    moves.isTarget      = false;
    moves.isMe          = false;
    moves.isCancel      = false;
    moves.delta.reset();

    mouse_control mc(MOUSE_MODE_TARGET_DIR);

    beh->compass        = true;

    do
    {
        const command_type key_command = beh->get_command();

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
        // If we've received a HUP signal then the user can't choose a
        // target.
        if (crawl_state.seen_hups)
        {
            moves.isValid  = false;
            moves.isCancel = true;

            mpr("Targetting interrupted by HUP signal.", MSGCH_ERROR);
            return;
        }
#endif

#ifdef USE_TILE
        if (key_command == CMD_TARGET_MOUSE_MOVE)
        {
            continue;
        }
        else if (key_command == CMD_TARGET_MOUSE_SELECT)
        {
            const coord_def &gc = tiles.get_cursor();
            if (gc == Region::NO_CURSOR)
                continue;

            if (!map_bounds(gc))
                continue;

            coord_def delta = gc - you.pos();
            if (delta.x < -1 || delta.x > 1
                || delta.y < -1 || delta.y > 1)
            {
                tiles.place_cursor(CURSOR_MOUSE, gc);
                delta = tiles.get_cursor() - you.pos();
                ASSERT(delta.x >= -1 && delta.x <= 1);
                ASSERT(delta.y >= -1 && delta.y <= 1);
            }

            moves.delta = delta;
            moves.isMe  = delta.origin();
            break;
        }
#endif

        if (key_command == CMD_TARGET_SELECT)
        {
            moves.delta.reset();
            moves.isMe = true;
            break;
        }

        const int i = _targetting_cmd_to_compass(key_command);
        if (i != -1)
        {
            moves.delta = Compass[i];
        }
        else if (key_command == CMD_TARGET_CANCEL)
        {
            moves.isCancel = true;
            moves.isValid = false;
        }
    }
    while (!moves.isCancel && moves.delta.origin());

#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MOUSE, Region::NO_CURSOR);
#endif
}

static int _targetting_cmd_to_compass( command_type command )
{
    switch ( command )
    {
    case CMD_TARGET_UP:         case CMD_TARGET_DIR_UP:
        return 0;
    case CMD_TARGET_UP_RIGHT:   case CMD_TARGET_DIR_UP_RIGHT:
        return 1;
    case CMD_TARGET_RIGHT:      case CMD_TARGET_DIR_RIGHT:
        return 2;
    case CMD_TARGET_DOWN_RIGHT: case CMD_TARGET_DIR_DOWN_RIGHT:
        return 3;
    case CMD_TARGET_DOWN:       case CMD_TARGET_DIR_DOWN:
        return 4;
    case CMD_TARGET_DOWN_LEFT:  case CMD_TARGET_DIR_DOWN_LEFT:
        return 5;
    case CMD_TARGET_LEFT:       case CMD_TARGET_DIR_LEFT:
        return 6;
    case CMD_TARGET_UP_LEFT:    case CMD_TARGET_DIR_UP_LEFT:
        return 7;
    default:
        return -1;
    }
}

static int _targetting_cmd_to_feature( command_type command )
{
    switch ( command )
    {
    case CMD_TARGET_FIND_TRAP:      return '^';
    case CMD_TARGET_FIND_PORTAL:    return '\\';
    case CMD_TARGET_FIND_ALTAR:     return '_';
    case CMD_TARGET_FIND_UPSTAIR:   return '<';
    case CMD_TARGET_FIND_DOWNSTAIR: return '>';
    default:                        return 0;
    }
}

static command_type shift_direction(command_type cmd)
{
    switch (cmd)
    {
    case CMD_TARGET_DOWN_LEFT:  return CMD_TARGET_DIR_DOWN_LEFT;
    case CMD_TARGET_LEFT:       return CMD_TARGET_DIR_LEFT;
    case CMD_TARGET_DOWN:       return CMD_TARGET_DIR_DOWN;
    case CMD_TARGET_UP:         return CMD_TARGET_DIR_UP;
    case CMD_TARGET_RIGHT:      return CMD_TARGET_DIR_RIGHT;
    case CMD_TARGET_DOWN_RIGHT: return CMD_TARGET_DIR_DOWN_RIGHT;
    case CMD_TARGET_UP_RIGHT:   return CMD_TARGET_DIR_UP_RIGHT;
    case CMD_TARGET_UP_LEFT:    return CMD_TARGET_DIR_UP_LEFT;
    default: return (cmd);
    }
}

static const char *target_mode_help_text(int mode)
{
    switch (mode)
    {
    case DIR_NONE:
        return (Options.target_unshifted_dirs ? "? - help" :
                "? - help, Shift-Dir - shoot in a straight line");
    case DIR_TARGET:
        return "? - help, Dir - move target cursor";
    default:
        return "? - help";
    }
}

#ifndef USE_TILE
static void _draw_ray_glyph(const coord_def &pos, int colour,
                            int glych, int mcol, bool in_range)
{
    if (const monsters *mons = monster_at(pos))
    {
        if (mons->alive() && mons->visible_to(&you))
        {
            glych  = get_screen_glyph(pos);
            colour = mcol;
        }
    }
    const coord_def vp = grid2view(pos);
    cgotoxy(vp.x, vp.y, GOTO_DNGN);
    textcolor( real_colour(colour) );
    putch(glych);
}
#endif

// Unseen monsters in shallow water show a "strange disturbance".
// (Unless flying!)
static bool _mon_submerged_in_water(const monsters *mon)
{
    if (!mon)
        return (false);

    return (grd(mon->pos()) == DNGN_SHALLOW_WATER
            && you.see_cell(mon->pos())
            && !mon->visible_to(&you)
            && !mons_flies(mon));
}

static bool _mon_exposed_in_cloud(const monsters *mon)
{
    if (!mon)
        return (false);

    return (!mon->visible_to(&you)
            && is_opaque_cloud(env.cgrid(mon->pos()))
            && !mons_is_insubstantial(mon->type));
}

static bool _mon_exposed(const monsters* mon)
{
    return (_mon_submerged_in_water(mon) || _mon_exposed_in_cloud(mon));
}

static bool _is_target_in_range(const coord_def& where, int range)
{
    // Range doesn't matter.
    if (range == -1)
        return (true);

    return (grid_distance(you.pos(), where) <= range);
}

// We handle targetting for repeating commands and re-doing the
// previous command differently (i.e., not just letting the keys
// stuffed into the macro buffer replay as-is) because if the player
// targeted a monster using the movement keys and the monster then
// moved between repetitions, then simply replaying the keys in the
// buffer will target an empty square.
static void _direction_again(dist& moves, targetting_type restricts,
                             targ_mode_type mode, int range, bool just_looking,
                             const char *prompt, targetting_behaviour *beh)
{
    moves.isValid       = false;
    moves.isTarget      = false;
    moves.isMe          = false;
    moves.isCancel      = false;
    moves.isEndpoint    = false;
    moves.choseRay      = false;

    if (you.prev_targ == MHITNOT && you.prev_grd_targ.origin())
    {
        moves.isCancel = true;
        crawl_state.cancel_cmd_repeat();
        return;
    }

#ifdef DEBUG
    int targ_types = 0;
    if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
        targ_types++;
    if (you.prev_targ == MHITYOU)
        targ_types++;
    if (you.prev_grd_targ != coord_def(0, 0))
        targ_types++;
    ASSERT(targ_types == 1);
#endif

    // Discard keys until we get to a set-target command
    command_type key_command = CMD_NO_CMD;

    while (crawl_state.is_replaying_keys())
    {
        key_command = beh->get_command();

        if (key_command == CMD_TARGET_PREV_TARGET
            || key_command == CMD_TARGET_SELECT_ENDPOINT
            || key_command == CMD_TARGET_SELECT
            || key_command == CMD_TARGET_SELECT_FORCE_ENDPOINT
            || key_command == CMD_TARGET_SELECT_FORCE
            || key_command == CMD_TARGET_MAYBE_PREV_TARGET
            || key_command == CMD_TARGET_MOUSE_SELECT)
        {
            break;
        }
    }

    if (!crawl_state.is_replaying_keys())
    {
        moves.isCancel = true;

        mpr("Ran out of keys.");

        return;
    }

    if (key_command == CMD_TARGET_SELECT_ENDPOINT
        || key_command == CMD_TARGET_SELECT_FORCE_ENDPOINT)
    {
        moves.isEndpoint = true;
    }

    if (you.prev_grd_targ != coord_def(0, 0))
    {
        if (!you.see_cell(you.prev_grd_targ))
        {
            moves.isCancel = true;

            crawl_state.cancel_cmd_all("You can no longer see the dungeon "
                                       "square you previously targeted.");
            return;
        }
        else if (you.prev_grd_targ == you.pos())
        {
            moves.isCancel = true;

            crawl_state.cancel_cmd_all("You are now standing on your "
                                       "previously targeted dungeon "
                                       "square.");
            return;
        }
        else if (!_is_target_in_range(you.prev_grd_targ, range))
        {
            moves.isCancel = true;

            crawl_state.cancel_cmd_all("Your previous target is now out of "
                                       "range.");
            return;
        }

        moves.target = you.prev_grd_targ;

        moves.choseRay = find_ray(you.pos(), moves.target, moves.ray);
    }
    else if (you.prev_targ == MHITYOU)
    {
        moves.isMe = true;
        moves.target = you.pos();

        // Discard 'Y' player gave to yesno()
        // Changed this, was that necessary? -cao
        //if (mode == TARG_ENEMY)
        if (mode == TARG_ENEMY || mode == TARG_HOSTILE)
            getchm();
    }
    else
    {
        const monsters *montarget = &menv[you.prev_targ];

        if (!you.can_see(montarget))
        {
            moves.isCancel = true;
            crawl_state.cancel_cmd_all("Your target is gone.");
            return;
        }
        else if (!_is_target_in_range(montarget->pos(), range))
        {
            moves.isCancel = true;

            crawl_state.cancel_cmd_all("Your previous target is now out of "
                                       "range.");
            return;
        }

        moves.target = montarget->pos();

        moves.choseRay = find_ray(you.pos(), moves.target, moves.ray);
    }

    moves.isValid  = true;
    moves.isTarget = true;
}

class view_desc_proc
{
public:
    view_desc_proc()
    {
        // This thing seems to be starting off 1 line above where it
        // should be. -cao
        nextline();
    }
    int width() { return crawl_view.msgsz.x; }
    int height() { return crawl_view.msgsz.y; }
    void print(const std::string &str) { cprintf("%s", str.c_str()); }
    void nextline() { cgotoxy(1, wherey() + 1); }
};

static void _describe_monster(const monsters *mon);

// Lists all the monsters and items currently in view by the player.
// TODO: Allow sorting of items lists.
void full_describe_view()
{
    std::vector<monster_info> list_mons;
    std::vector<item_def> list_items;
    std::vector<coord_def> list_features;

    // Grab all items known (or thought) to be in the stashes in view.
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        if (feat_stair_direction(grd(*ri)) != CMD_NO_CMD
            || feat_is_altar(grd(*ri)))
        {
            list_features.push_back(*ri);
        }

        const monsters *mon = monster_at(*ri);
        const bool unknown_mimic = (mon && mons_is_unknown_mimic(mon));

        if (unknown_mimic)      // It'll be on top.
            list_items.push_back(get_mimic_item(mon));

        const int oid = you.visible_igrd(*ri);
        if (oid == NON_ITEM)
            continue;

        if (StashTracker::is_level_untrackable())
        {
            // On levels with no stashtracker, you can still see the top
            // item.
            if (!unknown_mimic)
                list_items.push_back(mitm[oid]);
        }
        else
        {
            const std::vector<item_def> items = item_list_in_stash(*ri);

#ifdef DEBUG_DIAGNOSTICS
            if (items.empty())
            {
                mprf(MSGCH_ERROR, "No items found in stash, but top item is %s",
                     mitm[oid].name(DESC_PLAIN).c_str());
                more();
            }
#endif
            list_items.insert(list_items.end(), items.begin(), items.end());
        }
    }

    // Get monsters via the monster_info, sorted by difficulty.
    get_monster_info(list_mons);
    std::sort(list_mons.begin(), list_mons.end(),
              monster_info::less_than_wrapper);

    if (list_mons.empty() && list_items.empty() && list_features.empty())
    {
        mprf("No monsters, items or features are visible.");
        return;
    }

    InvMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                        | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);

    std::string title = "";
    std::string action = "";
    if (!list_mons.empty())
    {
        title  = "Monsters";
        action = "view"; // toggle views monster description
    }
    bool nonmons = false;
    if (!list_items.empty())
    {
        if (!title.empty())
            title += "/";
        title += "Items";
        nonmons = true;
    }
    if (!list_features.empty())
    {
        if (!title.empty())
            title += "/";
        title += "Features";
        nonmons = true;
    }
    if (nonmons)
    {
        if (!action.empty())
            action += "/";
        action += "travel"; // toggle travels to items/features
    }
    title = "Visible " + title;
    std::string title1 = title + " (select to " + action + ", '!' to examine):";
    title += " (select for more detail, '!' to " + action + "):";

    desc_menu.set_title( new MenuEntry(title, MEL_TITLE), false);
    desc_menu.set_title( new MenuEntry(title1, MEL_TITLE) );

    desc_menu.set_tag("pickup");
    desc_menu.set_type(MT_PICKUP); // necessary for sorting of the item submenu
    desc_menu.action_cycle = Menu::CYCLE_TOGGLE;

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (list_mons.size() + list_items.size() + list_features.size() > 52
        && (desc_menu.maxpagesize() > 52 || desc_menu.maxpagesize() == 0))
    {
        desc_menu.set_maxpagesize(52);
    }

    // Start with hotkey 'a' and count from there.
    menu_letter hotkey;
    // Build menu entries for monsters.
    if (!list_mons.empty())
    {
        desc_menu.add_entry( new MenuEntry("Monsters", MEL_SUBTITLE) );
        std::vector<monster_info>::const_iterator mi;
        for (mi = list_mons.begin(); mi != list_mons.end(); ++mi)
        {
            // List monsters in the form
            // (A) An angel (neutral), wielding a glowing long sword

            std::string prefix = "";
#ifndef USE_TILE
            const std::string col_string = colour_to_str(mi->m_glyph_colour);
            prefix = "(<" + col_string + ">"
                     + stringize_glyph(mi->m_glyph)
                     + "</" + col_string + ">) ";
#endif
            std::string str = get_monster_equipment_desc(mi->m_mon, true,
                                                         DESC_CAP_A, true);

            if (you.beheld_by(mi->m_mon))
                str += ", keeping you mesmerised";

            if (mi->m_damage_level != MDAM_OKAY)
                str += ", " + mi->m_damage_desc;

#ifndef USE_TILE
            // Wraparound if the description is longer than allowed.
            linebreak_string2(str, get_number_of_cols() - 9);
#endif
            std::vector<formatted_string> fss;
            formatted_string::parse_string_to_multiple(str, fss);
            MenuEntry *me = NULL;
            for (unsigned int j = 0; j < fss.size(); ++j)
            {
                if (j == 0)
                    me = new MonsterMenuEntry(prefix+str, mi->m_mon, hotkey++);
#ifndef USE_TILE
                else
                {
                    str = "         " + fss[j].tostring();
                    me = new MenuEntry(str, MEL_ITEM, 1);
                }
#endif
                desc_menu.add_entry(me);
            }
        }
    }

    // Build menu entries for items.
    if (!list_items.empty())
    {
        std::vector<InvEntry*> all_items;
        for (unsigned int i = 0; i < list_items.size(); ++i)
            all_items.push_back( new InvEntry(list_items[i]) );

        const menu_sort_condition *cond = desc_menu.find_menu_sort_condition();
        desc_menu.sort_menu(all_items, cond);

        desc_menu.add_entry( new MenuEntry( "Items", MEL_SUBTITLE ) );
        for (unsigned int i = 0; i < all_items.size(); ++i, hotkey++)
        {
            InvEntry *me = all_items[i];
#ifndef USE_TILE
            // Show glyphs only for ASCII.
            me->set_show_glyph(true);
#endif
            me->tag = "pickup";
            me->hotkeys[0] = hotkey;
            me->quantity = 2; // Hack to make items selectable.

            desc_menu.add_entry(me);
        }
    }

    if (!list_features.empty())
    {
        desc_menu.add_entry( new MenuEntry("Features", MEL_SUBTITLE) );
        for (unsigned int i = 0; i < list_features.size(); ++i, hotkey++)
        {
            const coord_def c = list_features[i];
            std::string desc = "";
#ifndef USE_TILE
            const coord_def e  = c - you.pos() + coord_def(8,8);

            glyph g = get_show_glyph(env.show(e));
            const std::string colour_str = colour_to_str(g.col);
            desc = "(<" + colour_str + ">";
            desc += stringize_glyph(g.ch);
            if (g.ch == '<')
                desc += '<';

            desc += "</" + colour_str +">) ";
#endif
            desc += feature_description(c);
            if (is_unknown_stair(c))
                desc += " (not visited)";
            FeatureMenuEntry *me = new FeatureMenuEntry(desc, c, hotkey);
            me->tag        = "description";
            // Hack to make features selectable.
            me->quantity   = c.x*100 + c.y + 3;
            desc_menu.add_entry(me);
        }
    }

    // Select an item to read its full description, or a monster to read its
    // e'x'amine description. Toggle with '!' to travel to an item's position
    // or read a monster's database entry.
    // (Maybe that should be reversed in the case of monsters.)
    // For ASCII, the 'x' information may include short database descriptions.

    // Menu loop
    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = desc_menu.show();
        redraw_screen();

        if (sel.empty())
            break;

        // HACK: quantity == 1: monsters, quantity == 2: items
        const int quant = sel[0]->quantity;
        if (quant == 1)
        {
            // Get selected monster.
            monsters* m = (monsters*)(sel[0]->data);

#ifdef USE_TILE
            // Highlight selected monster on the screen.
            const coord_def gc(m->pos());
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            const std::string &desc = get_terse_square_desc(gc);
            tiles.clear_text_tags(TAG_TUTORIAL);
            tiles.add_text_tag(TAG_TUTORIAL, desc, gc);
#endif

            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
            {
                describe_info inf;
                get_square_desc(m->pos(), inf, true);
#ifndef USE_TILE
                // Hmpf. This was supposed to work for both ASCII *and* Tiles!
                view_desc_proc proc;
                process_description<view_desc_proc>(proc, inf);
#else
                mesclr();
                _describe_monster(m);
#endif
                if (getch() == 0)
                    getch();
            }
            else // ACT_EXECUTE, here used to view database entry
            {
                describe_monsters(*m);
                redraw_screen();
                mesclr(true);
            }
        }
        else if (quant == 2)
        {
            // Get selected item.
            item_def* i = (item_def*)(sel[0]->data);
            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
                describe_item( *i );
            else // ACT_EXECUTE -> travel to item
            {
                const coord_def c = i->pos;
                start_travel( c );
                break;
            }
        }
        else
        {
            const int num = quant - 3;
            const int y = num % 100;
            const int x = (num - y)/100;
            coord_def c(x,y);

            if (desc_menu.menu_action == InvMenu::ACT_EXAMINE)
                describe_feature_wide(c);
            else // ACT_EXECUTE -> travel to feature
            {
                start_travel( c );
                break;
            }
        }
    }

#ifndef USE_TILE
    if (!list_items.empty())
    {
        // Unset show_glyph for other menus.
        InvEntry *me = new InvEntry(list_items[0]);
        me->set_show_glyph(false);
        delete me;
    }
#else
    // Clear cursor placement.
    tiles.place_cursor(CURSOR_TUTORIAL, Region::NO_CURSOR);
    tiles.clear_text_tags(TAG_TUTORIAL);
#endif
}


//---------------------------------------------------------------
//
// direction
//
// use restrict == DIR_DIR to allow only a compass direction;
//              == DIR_TARGET to allow only choosing a square;
//              == DIR_NONE to allow either.
//
// outputs: dist structure:
//
//           isValid        a valid target or direction was chosen
//           isCancel       player hit 'escape'
//           isTarget       targetting was used
//           choseRay       player wants a specific ray
//           ray            ...this one
//           isEndpoint     player wants the ray to stop on the dime
//           tx,ty          target x,y
//           dx,dy          direction delta for DIR_DIR
//
//--------------------------------------------------------------

#ifndef USE_TILE
// XXX: Hack - can't pass mlist entries into _find_mlist().
bool mlist_full_info;
std::vector<monster_info> mlist;
static void _fill_monster_list(bool full_info)
{
    std::vector<monster_info> temp;
    get_monster_info(temp);
    mlist_full_info = full_info;

    // Get the unique entries.
    mlist.clear();
    unsigned int start = 0, end = 1;
    while (start < temp.size())
    {
        mlist.push_back(temp[start]);
        for (end = start + 1; end < temp.size(); ++end)
        {
            if (monster_info::less_than(temp[start], temp[end],
                                             full_info))
            {
                  break;
            }
        }
        start = end;
    }
}

static int _mlist_letter_to_index(char idx)
{
    if (idx >= 'b')
        idx--;
    if (idx >= 'h')
        idx--;
    if (idx >= 'j')
        idx--;
    if (idx >= 'k')
        idx--;
    if (idx >= 'l')
        idx--;

    return (idx - 'a');
}
#endif

range_view_annotator::range_view_annotator(int range)
{
    if (Options.darken_beyond_range && range >= 0)
    {
        crawl_state.darken_range = range;
        viewwindow(false, false);
    }
}

range_view_annotator::~range_view_annotator()
{
    if (Options.darken_beyond_range && crawl_state.darken_range >= 0)
    {
        crawl_state.darken_range = -1;
        viewwindow(false, false);
    }
}

bool _dist_ok(const dist& moves, int range, targ_mode_type mode,
              bool may_target_self, bool cancel_at_self)
{
    if (!moves.isCancel && moves.isTarget)
    {
        if (!you.see_cell(moves.target))
        {
            mpr("Sorry, you can't target what you can't see.",
                MSGCH_EXAMINE_FILTER);
            return (false);
        }

        if (moves.target == you.pos())
        {
            // may_target_self == makes (some) sense to target yourself
            // (SPFLAG_AREA)

            // cancel_at_self == not allowed to target yourself
            // (SPFLAG_NOT_SELF)

            if (cancel_at_self)
            {
                mpr("Sorry, you can't target yourself.", MSGCH_EXAMINE_FILTER);
                return (false);
            }

            if (!may_target_self && (mode == TARG_ENEMY
                                     || mode == TARG_HOSTILE))
            {
                if (Options.allow_self_target == CONFIRM_CANCEL)
                {
                    mpr("That would be overly suicidal.", MSGCH_EXAMINE_FILTER);
                    return (false);
                }
                else if (Options.allow_self_target == CONFIRM_PROMPT)
                {
                    return yesno("Really target yourself?", false, 'n');
                }
            }
        }

        // Check range
        if (range >= 0 && grid_distance(moves.target, you.pos()) > range)
        {
            mpr("That is beyond the maximum range.", MSGCH_EXAMINE_FILTER);
            return (false);
        }
    }

    // Some odd cases
    if (!moves.isValid && !moves.isCancel)
        return yesno("Are you sure you want to fizzle?", false, 'n');

    return (true);
}

// Assuming the target is in view, is line-of-fire
// blocked, and by what?
static bool _blocked_ray(const coord_def &where,
                         dungeon_feature_type* feat = NULL)
{
    if (exists_ray(you.pos(), where))
        return (false);
    if (feat == NULL)
        return (true);
    *feat = ray_blocker(you.pos(), where);
    return (true);
}

std::string _targ_mode_name(targ_mode_type mode)
{
    switch (mode)
    {
    case TARG_ANY:
        return ("any");
    case TARG_ENEMY:
        return ("enemies");
    case TARG_FRIEND:
        return ("friends");
    case TARG_HOSTILE:
        return ("hostiles");
    default:
        return ("buggy");
    }
}

#ifndef USE_TILE
bool _init_mlist()
{
    const int full_info = update_monster_pane();
    if (full_info != -1)
    {
        _fill_monster_list(full_info);
        return (true);
    }
    else
        return (false);
}
#endif

void direction(dist& moves, const targetting_type restricts,
               targ_mode_type mode, const int range, const bool just_looking,
               const bool needs_path, const bool may_target_monster,
               const bool may_target_self, const char *prompt,
               targetting_behaviour *beh, const bool cancel_at_self)
{
    if (!beh)
    {
        static targetting_behaviour stock_behaviour;
        beh = &stock_behaviour;
    }
    beh->just_looking = just_looking;

#ifndef USE_TILE
    crawl_state.mlist_targetting = false;
    if (may_target_monster && restricts != DIR_DIR
        && Options.mlist_targetting)
    {
        crawl_state.mlist_targetting = _init_mlist();
    }
#endif

    if (crawl_state.is_replaying_keys() && restricts != DIR_DIR)
    {
        _direction_again(moves, restricts, mode, range, just_looking,
                         prompt, beh);
        return;
    }

    // NOTE: Even if just_looking is set, moves is still interesting,
    // because we can travel there!

    if (restricts == DIR_DIR)
    {
        direction_choose_compass(moves, beh);
        return;
    }

    cursor_control ccon(!Options.use_fake_cursor);
    mouse_control mc(needs_path && !just_looking ? MOUSE_MODE_TARGET_PATH
                                                 : MOUSE_MODE_TARGET);
    range_view_annotator rva(range);

    int dir = 0;
    bool show_beam = Options.show_beam && !just_looking && needs_path;

    ray_def ray;             // The possibly filled in beam.
    bool have_beam = false;  // Whether it is filled in.
    coord_def beam_target;   // What target is was chosen for.

    coord_def objfind_pos, monsfind_pos;

    // init
    moves.delta.reset();
    moves.target = objfind_pos = monsfind_pos = you.pos();

    // If we show the beam on startup, we have to initialise it.
    if (show_beam)
    {
        have_beam = find_ray(you.pos(), moves.target, ray);
        beam_target = moves.target;
    }

    bool skip_iter = false;
    bool found_autotarget = false;
    bool target_unshifted = Options.target_unshifted_dirs;

    // Find a default target.
    if (Options.default_target)
    {
        if (restricts == DIR_TARGET_OBJECT)
        {
             skip_iter = true;
             found_autotarget = true;
        }
        else if (mode == TARG_ENEMY || mode == TARG_HOSTILE)
        {
            skip_iter = true;   // Skip first iteration...XXX mega-hack
            if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
            {
                const monsters *montarget = &menv[you.prev_targ];
                if (you.can_see(montarget)
                        // not made friendly since then
                    && (mons_attitude(montarget) == ATT_HOSTILE
                        || mode == TARG_ENEMY && !montarget->friendly())
                    && _is_target_in_range(montarget->pos(), range))
                {
                    found_autotarget = true;
                    moves.target = montarget->pos();
                }
            }
        }
    }

    bool show_prompt = true;
    bool moved_with_keys = true;

    while (true)
    {
#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
        // If we've received a HUP signal then the user can't choose a
        // target.
        if (crawl_state.seen_hups)
        {
            moves.isValid  = false;
            moves.isCancel = true;

            mpr("Targetting interrupted by HUP signal.", MSGCH_ERROR);
            return;
        }
#endif

        bool allow_out_of_range = false;

        // Prompts might get scrolled off if you have too few lines available.
        // We'll live with that.
        if (!just_looking && (show_prompt || beh->should_redraw()))
        {
            mprf(MSGCH_PROMPT, "%s (%s)", prompt ? prompt : "Aim",
                 target_mode_help_text(restricts));

            if ((mode == TARG_ANY || mode == TARG_FRIEND)
                && moves.target == you.pos())
            {
                terse_describe_square(moves.target);
            }
            show_prompt = false;
        }

        // Reinit...this needs to be done every loop iteration
        // because moves is more persistent than loop_done.
        moves.isValid       = false;
        moves.isTarget      = false;
        moves.isMe          = false;
        moves.isCancel      = false;
        moves.isEndpoint    = false;
        moves.choseRay      = false;

        // This probably is called too often for Tiles. (jpeg)
        if (moved_with_keys)
            cursorxy(grid2viewX(moves.target.x), grid2viewY(moves.target.y));

        command_type key_command;

        if (skip_iter)
        {
            if (restricts == DIR_TARGET_OBJECT)
                key_command = CMD_TARGET_OBJ_CYCLE_FORWARD;
            else if (found_autotarget)
                key_command = CMD_NO_CMD;
            else
                key_command = CMD_TARGET_CYCLE_FORWARD; // Find closest target.
        }
        else
                key_command = beh->get_command();

#ifdef USE_TILE
        // If a mouse command, update location to mouse position.
        if (key_command == CMD_TARGET_MOUSE_MOVE
            || key_command == CMD_TARGET_MOUSE_SELECT)
        {
            moved_with_keys = false;
            const coord_def &gc = tiles.get_cursor();
            if (gc != Region::NO_CURSOR)
            {
                if (!map_bounds(gc))
                    continue;

                moves.target = gc;

                if (key_command == CMD_TARGET_MOUSE_SELECT)
                    key_command = CMD_TARGET_SELECT;
            }
            else
                key_command = CMD_NO_CMD;
        }
        else
            moved_with_keys = true;
#endif

        if (target_unshifted && moves.target == you.pos()
            && restricts != DIR_TARGET)
        {
            key_command = shift_direction(key_command);
        }

        if (target_unshifted
            && (key_command == CMD_TARGET_CYCLE_FORWARD
                || key_command == CMD_TARGET_CYCLE_BACK
                || key_command == CMD_TARGET_OBJ_CYCLE_FORWARD
                || key_command == CMD_TARGET_OBJ_CYCLE_BACK))
        {
            target_unshifted = false;
        }


        if (key_command == CMD_TARGET_MAYBE_PREV_TARGET)
        {
            if (moves.target == you.pos())
                key_command = CMD_TARGET_PREV_TARGET;
            else
                key_command = CMD_TARGET_SELECT;
        }

        bool need_beam_redraw = false;
        bool force_redraw     = false;
        bool loop_done        = false;

        coord_def old_target = moves.target;
        if (skip_iter)
            old_target.x += 500; // hmmm...hack

        int i;

#ifndef USE_TILE
        if (key_command >= CMD_TARGET_CYCLE_MLIST
            && key_command <= CMD_TARGET_CYCLE_MLIST_END)
        {
            const int idx = _mlist_letter_to_index(key_command + 'a'
                                                   - CMD_TARGET_CYCLE_MLIST);

            if (_find_square_wrapper(monsfind_pos, 1,
                                     _find_mlist, needs_path, idx, range,
                                     true))
            {
                moves.target = monsfind_pos;
            }
            else if (!skip_iter)
                flush_input_buffer(FLUSH_ON_FAILURE);
        }
#endif

        switch (key_command)
        {
        // standard movement
        case CMD_TARGET_DOWN_LEFT:
        case CMD_TARGET_DOWN:
        case CMD_TARGET_DOWN_RIGHT:
        case CMD_TARGET_LEFT:
        case CMD_TARGET_RIGHT:
        case CMD_TARGET_UP_LEFT:
        case CMD_TARGET_UP:
        case CMD_TARGET_UP_RIGHT:
            i = _targetting_cmd_to_compass(key_command);
            moves.target += Compass[i];
            break;

        case CMD_TARGET_DIR_DOWN_LEFT:
        case CMD_TARGET_DIR_DOWN:
        case CMD_TARGET_DIR_DOWN_RIGHT:
        case CMD_TARGET_DIR_LEFT:
        case CMD_TARGET_DIR_RIGHT:
        case CMD_TARGET_DIR_UP_LEFT:
        case CMD_TARGET_DIR_UP:
        case CMD_TARGET_DIR_UP_RIGHT:
            i = _targetting_cmd_to_compass(key_command);

            if (restricts != DIR_TARGET)
            {
                // A direction is allowed, and we've selected it.
                moves.delta    = Compass[i];
                // Needed for now...eventually shouldn't be necessary
                moves.target   = you.pos() + moves.delta;
                moves.isValid  = true;
                moves.isTarget = false;
                have_beam      = false;
                show_beam      = false;
                moves.choseRay = false;
                loop_done      = true;
            }
            else
            {
                // Direction not allowed, so just move in that direction.
                // Maybe make this a bigger jump?
                moves.target += Compass[i] * 3;
            }
            break;

        case CMD_TARGET_SHOW_PROMPT:
            mprf(MSGCH_PROMPT, "%s (%s)", prompt? prompt : "Aim",
                 target_mode_help_text(restricts));
            break;

#ifndef USE_TILE
        case CMD_TARGET_TOGGLE_MLIST:
            if (!crawl_state.mlist_targetting)
                crawl_state.mlist_targetting = _init_mlist();
            else
                crawl_state.mlist_targetting = false;
            break;
#endif

#ifdef WIZARD
        case CMD_TARGET_CYCLE_BEAM:
            show_beam = true;
            have_beam = find_ray(you.pos(), moves.target, ray,
                                 opc_solid, BDS_DEFAULT, show_beam);
            beam_target = moves.target;
            need_beam_redraw = true;
            break;
#endif

        case CMD_TARGET_TOGGLE_BEAM:
            if (show_beam)
            {
                show_beam = false;
                need_beam_redraw = true;
            }
            else
            {
                if (!needs_path)
                {
                    mprf(MSGCH_EXAMINE_FILTER,
                         "This spell doesn't need a beam path.");
                    break;
                }

                have_beam = find_ray(you.pos(), moves.target, ray);
                beam_target = moves.target;
                show_beam = true;
                need_beam_redraw = true;
            }
            break;

        case CMD_TARGET_FIND_YOU:
            moves.target = you.pos();
            moves.delta.reset();
            break;

        case CMD_TARGET_FIND_TRAP:
        case CMD_TARGET_FIND_PORTAL:
        case CMD_TARGET_FIND_ALTAR:
        case CMD_TARGET_FIND_UPSTAIR:
        case CMD_TARGET_FIND_DOWNSTAIR:
        {
            const int thing_to_find = _targetting_cmd_to_feature(key_command);
            if (_find_square_wrapper(objfind_pos, 1, _find_feature,
                                     needs_path, thing_to_find,
                                     range, true, LOS_FLIPVH))
            {
                moves.target = objfind_pos;
            }
            else
            {
                if (!skip_iter)
                    flush_input_buffer(FLUSH_ON_FAILURE);
            }
            break;
        }

        case CMD_TARGET_CYCLE_TARGET_MODE:
            mode = static_cast<targ_mode_type>((mode + 1) % TARG_NUM_MODES);
            mprf("targetting mode is now: %s", _targ_mode_name(mode).c_str());
            break;

        case CMD_TARGET_PREV_TARGET:
            // Do we have a previous target?
            if (you.prev_targ == MHITNOT || you.prev_targ == MHITYOU)
            {
                mpr("You haven't got a previous target.", MSGCH_EXAMINE_FILTER);
                break;
            }

            // We have a valid previous target (maybe).
            {
                const monsters *montarget = &menv[you.prev_targ];

                if (!you.can_see(montarget))
                {
                    mpr("You can't see that creature any more.",
                        MSGCH_EXAMINE_FILTER);
                }
                else
                {
                    // We have all the information we need.
                    moves.isValid  = true;
                    moves.isTarget = true;
                    moves.target   = montarget->pos();
                    if (!just_looking)
                    {
                        have_beam = false;
                        loop_done = true;
                    }
                }
                break;
            }

            // some modifiers to the basic selection command
        case CMD_TARGET_SELECT_FORCE:
        case CMD_TARGET_SELECT_ENDPOINT:
        case CMD_TARGET_SELECT_FORCE_ENDPOINT:
            if (key_command == CMD_TARGET_SELECT_ENDPOINT
                || key_command == CMD_TARGET_SELECT_FORCE_ENDPOINT)
            {
                moves.isEndpoint = true;
            }
            if (key_command == CMD_TARGET_SELECT_FORCE
                || key_command == CMD_TARGET_SELECT_FORCE_ENDPOINT)
            {
                allow_out_of_range = true;
            }
            // intentional fall-through
        case CMD_TARGET_SELECT: // finalise current choice
            if (!moves.isEndpoint)
            {
                const monsters* m = monster_at(moves.target);
                if (m && _mon_exposed(m))
                    moves.isEndpoint = true;
            }
            moves.isValid  = true;
            moves.isTarget = true;
            loop_done      = true;

            you.prev_grd_targ.reset();

            // Maybe we should except just_looking here?
            if (const monsters* m = monster_at(moves.target))
                you.prev_targ = m->mindex();
            else if (moves.target == you.pos())
                you.prev_targ = MHITYOU;
            else
                you.prev_grd_targ = moves.target;

            break;

        case CMD_TARGET_OBJ_CYCLE_BACK:
        case CMD_TARGET_OBJ_CYCLE_FORWARD:
            dir = (key_command == CMD_TARGET_OBJ_CYCLE_BACK) ? -1 : 1;
            if (_find_square_wrapper(objfind_pos, dir, _find_object,
                                     needs_path, TARG_ANY, range, true,
                                     (dir > 0 ? LOS_FLIPVH : LOS_FLIPHV)))
            {
                moves.target = objfind_pos;
            }
            else if (!skip_iter)
                flush_input_buffer(FLUSH_ON_FAILURE);

            break;

        case CMD_TARGET_CYCLE_FORWARD:
        case CMD_TARGET_CYCLE_BACK:
            dir = (key_command == CMD_TARGET_CYCLE_BACK) ? -1 : 1;
            if (_find_square_wrapper(monsfind_pos, dir, _find_monster,
                                     needs_path, mode, range, true))
            {
                moves.target = monsfind_pos;
            }
            else if (skip_iter)
            {
                if (needs_path && !just_looking
                    && _find_square_wrapper(monsfind_pos, dir, _find_monster,
                                            false, mode, range, true))
                {
                    mpr("All monsters which could be auto-targeted "
                        "are covered by a wall or statue which interrupts "
                        "your line of fire, even though it doesn't "
                        "interrupt your line of sight.",
                        MSGCH_PROMPT);
                }
            }
            else
                flush_input_buffer(FLUSH_ON_FAILURE);
            break;

        case CMD_TARGET_CANCEL:
            loop_done = true;
            moves.isCancel = true;
            beh->mark_ammo_nonchosen();
            break;

        case CMD_TARGET_CENTER:
            moves.isValid  = true;
            moves.isTarget = true;
            moves.target   = you.pos();
            break;

#ifdef WIZARD
        case CMD_TARGET_WIZARD_MAKE_FRIENDLY:
            // Maybe we can skip this check...but it can't hurt
            if (!you.wizard || !in_bounds(moves.target))
                break;

            {
                monsters* m = monster_at(moves.target);

                if (m == NULL)
                    break;

                mon_attitude_type att = m->attitude;

                // During arena mode, skip directly from friendly to hostile.
                if (crawl_state.arena_suspended && att == ATT_FRIENDLY)
                    att = ATT_NEUTRAL;

                switch (att)
                {
                case ATT_FRIENDLY:
                    m->attitude = ATT_GOOD_NEUTRAL;
                    m->flags &= ~MF_NO_REWARD;
                    m->flags |= MF_WAS_NEUTRAL;
                    break;
                case ATT_GOOD_NEUTRAL:
                    m->attitude = ATT_STRICT_NEUTRAL;
                    break;
                case ATT_STRICT_NEUTRAL:
                    m->attitude = ATT_NEUTRAL;
                    break;
                case ATT_NEUTRAL:
                    m->attitude = ATT_HOSTILE;
                    m->flags &= ~MF_WAS_NEUTRAL;
                    break;
                case ATT_HOSTILE:
                    m->attitude = ATT_FRIENDLY;
                    m->flags |= MF_NO_REWARD;
                    break;
                }

                // To update visual branding of friendlies.  Only
                // seems capabable of adding bolding, not removing it,
                // though.
                viewwindow(false, true);
            }
            break;

        case CMD_TARGET_WIZARD_BLESS_MONSTER:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monsters* m = monster_at(moves.target))
                wizard_apply_monster_blessing(m);
            break;

        case CMD_TARGET_WIZARD_MAKE_SHOUT:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monsters* m = monster_at(moves.target))
                debug_make_monster_shout(m);
            break;

        case CMD_TARGET_WIZARD_GIVE_ITEM:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monsters* m = monster_at(moves.target))
                wizard_give_monster_item(m);
            break;

        case CMD_TARGET_WIZARD_MOVE:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            wizard_move_player_or_monster(moves.target);

            loop_done = true;
            skip_iter = true;

            break;

        case CMD_TARGET_WIZARD_PATHFIND:
            if (!you.wizard || !in_bounds(moves.target))
                break;

            if (monsters* m = monster_at(moves.target))
                debug_pathfind(m->mindex());
            break;

        case CMD_TARGET_WIZARD_GAIN_LEVEL:
            break;

        case CMD_TARGET_WIZARD_MISCAST:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (you.pos() == moves.target)
                debug_miscast(NON_MONSTER);
            if (monsters* m = monster_at(moves.target))
                debug_miscast(m->mindex());
            break;

        case CMD_TARGET_WIZARD_MAKE_SUMMONED:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monsters* m = monster_at(moves.target))
                wizard_make_monster_summoned(m);
            break;

        case CMD_TARGET_WIZARD_POLYMORPH:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monsters* m = monster_at(moves.target))
                wizard_polymorph_monster(m);
            break;

        case CMD_TARGET_WIZARD_DEBUG_MONSTER:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monster_at(moves.target))
                debug_stethoscope(mgrd(moves.target));
            break;

        case CMD_TARGET_WIZARD_HURT_MONSTER:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            if (monster_at(moves.target))
            {
                monster_at(moves.target)->hit_points = 1;
                mpr("Brought the mon down to near death.");
            }
            break;
#endif
        case CMD_TARGET_DESCRIBE:
            full_describe_square(moves.target);
            force_redraw = true;
            if (crawl_state.arena_suspended)
                need_beam_redraw = true;
            break;

        case CMD_TARGET_HELP:
            show_targetting_help();
            force_redraw = true;
            redraw_screen();
            mesclr(true);
            show_prompt = true;
            break;

        default:
            break;
        }

        flush_prev_message();
        if (loop_done)
        {
            // Confirm that the loop is really done. If it is,
            // break out. If not, just continue looping.

            if (just_looking) // easy out
                break;

            if (_dist_ok(moves, allow_out_of_range ? -1 : range,
                         mode, may_target_self, cancel_at_self))
            {
                // Finalise whatever is inside the loop
                // (moves-internal finalizations can be done later).
                moves.choseRay = have_beam;
                moves.ray = ray;
                break;
            }
            else
                show_prompt = true;
        }

        // We'll go on looping. Redraw whatever is necessary.
        if ( !in_viewport_bounds(grid2view(moves.target)) )
        {
            // Tried to step out of bounds
            moves.target = old_target;
        }

        bool have_moved = (old_target != moves.target);

        if (beam_target != moves.target && show_beam)
        {
             have_beam = find_ray(you.pos(), moves.target, ray);
             beam_target = moves.target;
             need_beam_redraw = true;
        }

        if (have_moved || force_redraw)
        {
            if (!skip_iter)     // Don't clear before we get a chance to see.
                mesclr(true);   // Maybe not completely necessary.

            bool in_range = (range < 0
                             || grid_distance(moves.target,you.pos()) <= range);
            terse_describe_square(moves.target, in_range);
            flush_prev_message();
        }

#ifdef USE_TILE
        // Tiles always need a beam redraw if show_beam is true (and valid...)
        if (show_beam)
            need_beam_redraw = true;
#endif
        if (need_beam_redraw)
        {
#ifndef USE_TILE
            // Clear the old ray.
            viewwindow(false, false);
#endif
            if (show_beam && have_beam)
            {
                // Draw the new ray with magenta '*'s, not including
                // your square or the target square.
                // Out-of-range cells get grey '*'s instead.
                ray_def raycopy = ray; // temporary copy to work with
                // XXX: should have moves.target == beam_target, but don't.
                while (raycopy.pos() != beam_target)
                {
                    if (raycopy.pos() != you.pos())
                    {
                        ASSERT(in_los(raycopy.pos()));

                        const bool in_range = (range < 0)
                            || grid_distance(raycopy.pos(), you.pos()) <= range;
#ifdef USE_TILE
                        tile_place_ray(raycopy.pos(), in_range);
#else
                        const int bcol = in_range ? MAGENTA : DARKGREY;
                        _draw_ray_glyph(raycopy.pos(), bcol, '*',
                                        bcol | COLFLAG_REVERSE, in_range);
#endif
                    }
                    raycopy.advance();
                }
                textcolor(LIGHTGREY);
#ifdef USE_TILE
                const bool in_range
                        = (range < 0
                           || grid_distance(raycopy.pos(), you.pos()) <= range);
                tile_place_ray(moves.target, in_range);
#endif
            }
#ifdef USE_TILE
            viewwindow(false, true);
#endif
        }
        skip_iter = false;      // Only skip one iteration at most.
    }
    moves.isMe = (moves.target == you.pos());
    mesclr();

    // We need this for directional explosions, otherwise they'll explode one
    // square away from the player.
    _extend_move_to_edge(moves);

#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MOUSE, Region::NO_CURSOR);
#endif
}

std::string get_terse_square_desc(const coord_def &gc)
{
    std::string desc = "";
    const char *unseen_desc = "[unseen terrain]";

    if (gc == you.pos())
        desc = you.your_name;
    else if (!map_bounds(gc))
        desc = unseen_desc;
    else if (!you.see_cell(gc))
    {
        if (is_terrain_seen(gc))
        {
            desc = "[" + feature_description(gc, false, DESC_PLAIN, false)
                       + "]";
        }
        else
            desc = unseen_desc;
    }
    else if (monster_at(gc) && you.can_see(monster_at(gc)))
    {
        const monsters& mons = *monster_at(gc);

        if (mons_is_mimic(mons.type) && !(mons.flags & MF_KNOWN_MIMIC))
            desc = get_mimic_item(&mons).name(DESC_PLAIN);
        else
            desc = mons.full_name(DESC_PLAIN, true);
    }
    else if (you.visible_igrd(gc) != NON_ITEM)
    {
        if (mitm[you.visible_igrd(gc)].is_valid())
            desc = mitm[you.visible_igrd(gc)].name(DESC_PLAIN);
    }
    else
        desc = feature_description(gc, false, DESC_PLAIN, false);

    return desc;
}

void terse_describe_square(const coord_def &c, bool in_range)
{
    if (!you.see_cell(c))
        _describe_oos_square(c);
    else if (in_bounds(c) )
        _describe_cell(c, in_range);
}

void get_square_desc(const coord_def &c, describe_info &inf,
                     bool examine_mons)
{
    // NOTE: Keep this function in sync with full_describe_square.

    // Don't give out information for things outside LOS
    if (!you.see_cell(c))
        return;

    const monsters* mons = monster_at(c);
    const int oid = you.visible_igrd(c);

    if (mons && mons->visible_to(&you))
    {
        // First priority: monsters.
        if (examine_mons && !mons_is_unknown_mimic(mons))
        {
            // If examine_mons is true (currently only for the Tiles
            // mouse-over information), set monster's
            // equipment/woundedness/enchantment description as title.
            std::string desc   = get_monster_equipment_desc(mons) + ".\n";
            std::string wounds = get_wounds_description(mons);
            if (!wounds.empty())
                desc += mons->pronoun(PRONOUN_CAP) + wounds + "\n";
            desc += _get_monster_desc(mons);

            inf.title = desc;
        }
        get_monster_db_desc(*mons, inf);
    }
    else if (oid != NON_ITEM)
    {
        // Second priority: objects.
        // If examine_mons is true, use terse descriptions.
        if (mitm[oid].is_valid())
            get_item_desc(mitm[oid], inf, examine_mons);
    }
    else
    {
        // Third priority: features.
        get_feature_desc(c, inf);
    }
}

void full_describe_square(const coord_def &c)
{
    // NOTE: Keep this function in sync with get_square_desc.

    // Don't give out information for things outside LOS
    if (!you.see_cell(c))
        return;

    const monsters* mons = monster_at(c);
    const int oid = you.visible_igrd(c);

    if (mons && mons->visible_to(&you))
    {
        // First priority: monsters.
        describe_monsters(*mons);
    }
    else if (oid != NON_ITEM)
    {
        // Second priority: objects.
        describe_item( mitm[oid] );
    }
    else
    {
        // Third priority: features.
        describe_feature_wide(c);
    }

    redraw_screen();
    mesclr(true);
}

static void _extend_move_to_edge(dist &moves)
{
    if (moves.delta.origin())
        return;

    // Now the tricky bit - extend the target x,y out to map edge.
    int mx = 0, my = 0;

    if (moves.delta.x > 0)
        mx = (GXM - 1) - you.pos().x;
    if (moves.delta.x < 0)
        mx = you.pos().x;

    if (moves.delta.y > 0)
        my = (GYM - 1) - you.pos().y;
    if (moves.delta.y < 0)
        my = you.pos().y;

    if (!(mx == 0 || my == 0))
    {
        if (mx < my)
            my = mx;
        else
            mx = my;
    }
    moves.target.x = you.pos().x + moves.delta.x * mx;
    moves.target.y = you.pos().y + moves.delta.y * my;
}

// Attempts to describe a square that's not in line-of-sight. If
// there's a stash on the square, announces the top item and number
// of items, otherwise, if there's a stair that's in the travel
// cache and noted in the Dungeon (O)verview, names the stair.
static void _describe_oos_square(const coord_def& where)
{
    mpr("You can't see that place.", MSGCH_EXAMINE_FILTER);

    if (!in_bounds(where) || !is_terrain_seen(where))
        return;

    describe_stash(where.x, where.y);
    _describe_feature(where, true);
}

bool in_vlos(int x, int y)
{
    return in_vlos(coord_def(x,y));
}

bool in_vlos(const coord_def &pos)
{
    return (in_los_bounds(pos) && (env.show(view2show(pos))
                                   || pos == grid2view(you.pos())));
}

bool in_los(int x, int y)
{
    return in_los(coord_def(x,y));
}

bool in_los(const coord_def& pos)
{
    return (in_vlos(grid2view(pos)));
}

static bool _mons_is_valid_target(const monsters *mon, int mode, int range)
{
    // Monster types that you can't gain experience from don't count as
    // monsters.
    if (mons_class_flag(mon->type, M_NO_EXP_GAIN))
        return (false);

    // Unknown mimics don't count as monsters, either.
    if (mons_is_mimic(mon->type)
        && !(mon->flags & MF_KNOWN_MIMIC))
    {
        return (false);
    }

    // Don't usually target unseen monsters...
    if (!mon->visible_to(&you))
    {
        // ...unless it creates a "disturbance in the water".
        // Since you can't see the monster, assume it's not a friend.
        // Also, don't target submerged monsters if there are other
        // targets in sight.  (This might be too restrictive.)
        return (mode != TARG_FRIEND
                && _mon_exposed(mon)
                && i_feel_safe(false, false, true, range));
    }

    return (true);
}

#ifndef USE_TILE
static bool _find_mlist(const coord_def& where, int idx, bool need_path,
                        int range = -1)
{
    if (static_cast<int>(mlist.size()) <= idx)
        return (false);

    if (!_is_target_in_range(where, range) || !in_los(where))
        return (false);

    const monsters* mon = monster_at(where);
    if (mon == NULL)
        return (false);

    int real_idx = 0;
    for (unsigned int i = 0; i+1 < mlist.size(); ++i)
    {
        if (real_idx == idx)
        {
            real_idx = i;
            break;
        }

        // While the monsters are identical, don't increase real_idx.
        if (!monster_info::less_than(mlist[i], mlist[i+1], mlist_full_info))
            continue;

        real_idx++;
   }

    if (!_mons_is_valid_target(mon, TARG_ANY, range))
        return (false);

    if (need_path && _blocked_ray(mon->pos()))
        return (false);

    const monsters *monl = mlist[real_idx].m_mon;
    extern mon_attitude_type mons_attitude(const monsters *m);

    if (mons_attitude(mon) != mlist[idx].m_attitude)
        return (false);

    if (mon->type != monl->type)
        return (mons_is_mimic(mon->type) && mons_is_mimic(monl->type));

    if (mlist_full_info)
    {
        if (mons_is_zombified(mon)) // Both monsters are zombies.
            return (mon->base_monster == monl->base_monster);

        if (mon->has_hydra_multi_attack())
            return (mon->number == monl->number);
    }

    if (mon->type == MONS_PLAYER_GHOST)
        return (mon->name(DESC_PLAIN) == monl->name(DESC_PLAIN));

    // Else the two monsters are identical.
    return (true);
}
#endif

static bool _find_monster( const coord_def& where, int mode, bool need_path,
                           int range = -1)
{
#ifdef CLUA_BINDINGS
    {
        coord_def dp = grid2player(where);
        // We could pass more info here.
        maybe_bool x = clua.callmbooleanfn("ch_target_monster", "dd",
                                           dp.x, dp.y);
        if (x != B_MAYBE)
            return (tobool(x));
    }
#endif

    // Target the player for friendly and general spells.
    if ((mode == TARG_FRIEND || mode == TARG_ANY) && where == you.pos())
        return (true);

    // Don't target out of range.
    if (!_is_target_in_range(where, range))
        return (false);

    const monsters* mon = monster_at(where);

    // No monster or outside LOS.
    if (mon == NULL || !in_los(where))
        return (false);

    // Monster in LOS but only via glass walls, so no direct path.
    if (need_path && !you.see_cell_no_trans(where))
        return (false);

    if (!_mons_is_valid_target(mon, mode, range))
        return (false);

    if (need_path && _blocked_ray(mon->pos()))
        return (false);

    // Now compare target modes.
    if (mode == TARG_ANY)
        return (true);

    if (mode == TARG_HOSTILE)
        return (mons_attitude(mon) == ATT_HOSTILE);

    if (mode == TARG_FRIEND)
        return (mon->friendly());

    ASSERT(mode == TARG_ENEMY);
    if (mon->friendly())
        return (false);

    // Don't target zero xp monsters.
    return (!mons_class_flag(mon->type, M_NO_EXP_GAIN));
}

static bool _find_feature( const coord_def& where, int mode,
                           bool /* need_path */, int /* range */)
{
    // The stair need not be in LOS if the square is mapped.
    if (!in_los(where) && !is_terrain_seen(where))
        return (false);

    return is_feature(mode, where);
}

static bool _find_object(const coord_def& where, int mode,
                         bool need_path, int range)
{
    // Don't target out of range.
    if (!_is_target_in_range(where, range))
        return (false);

    if (need_path && (!you.see_cell(where) || _blocked_ray(where)))
        return (false);

    return (env.map_knowledge(where).item() != SHOW_ITEM_NONE);
}

static int _next_los(int dir, int los, bool wrap)
{
    if (los == LOS_ANY)
        return (wrap? los : LOS_NONE);

    bool vis    = los & LOS_VISIBLE;
    bool hidden = los & LOS_HIDDEN;
    bool flipvh = los & LOS_FLIPVH;
    bool fliphv = los & LOS_FLIPHV;

    if (!vis && !hidden)
        vis = true;

    if (wrap)
    {
        if (!flipvh && !fliphv)
            return (los);

        // We have to invert flipvh and fliphv if we're wrapping. Here's
        // why:
        //
        //  * Say the cursor is on the last item in LOS, there are no
        //    items outside LOS, and wrap == true. flipvh is true.
        //  * We set wrap false and flip from visible to hidden, but there
        //    are no hidden items. So now we need to flip back to visible
        //    so we can go back to the first item in LOS. Unless we set
        //    fliphv, we can't flip from hidden to visible.
        //
        los = flipvh? LOS_FLIPHV : LOS_FLIPVH;
    }
    else
    {
        if (!flipvh && !fliphv)
            return (LOS_NONE);

        if (flipvh && vis != (dir == 1))
            return (LOS_NONE);

        if (fliphv && vis == (dir == 1))
            return (LOS_NONE);
    }

    los = (los & ~LOS_VISMASK) | (vis? LOS_HIDDEN : LOS_VISIBLE);
    return (los);
}

bool in_viewport_bounds(int x, int y)
{
    return crawl_view.in_view_viewport(coord_def(x, y));
}

bool in_los_bounds(const coord_def& p)
{
    return crawl_view.in_view_los(p);
}

//---------------------------------------------------------------
//
// find_square
//
// Finds the next monster/object/whatever (moving in a spiral
// outwards from the player, so closer targets are chosen first;
// starts to player's left) and puts its coordinates in mfp.
// Returns 1 if it found something, zero otherwise. If direction
// is -1, goes backwards.
//
//---------------------------------------------------------------
static char _find_square(coord_def &mfp, int direction,
                         bool (*find_targ)(const coord_def& wh, int mode,
                                           bool need_path, int range),
                         bool need_path, int mode, int range, bool wrap,
                         int los)
{
    // the day will come when [unsigned] chars will be consigned to
    // the fires of Gehenna. Not quite yet, though.
    int temp_xps = mfp.x;
    int temp_yps = mfp.y;
    int x_change = 0;
    int y_change = 0;

    bool onlyVis = false, onlyHidden = false;

    int i, j;

    if (los == LOS_NONE)
        return (0);

    if (los == LOS_FLIPVH || los == LOS_FLIPHV)
    {
        if (in_los_bounds(mfp))
        {
            // We've been told to flip between visible/hidden, so we
            // need to find what we're currently on.
            const bool vis = you.see_cell(view2grid(mfp));

            if (wrap && (vis != (los == LOS_FLIPVH)) == (direction == 1))
            {
                // We've already flipped over into the other direction,
                // so correct the flip direction if we're wrapping.
                los = (los == LOS_FLIPHV ? LOS_FLIPVH : LOS_FLIPHV);
            }

            los = (los & ~LOS_VISMASK) | (vis ? LOS_VISIBLE : LOS_HIDDEN);
        }
        else
        {
            if (wrap)
                los = LOS_HIDDEN | (direction > 0 ? LOS_FLIPHV : LOS_FLIPVH);
            else
                los |= LOS_HIDDEN;
        }
    }

    onlyVis     = (los & LOS_VISIBLE);
    onlyHidden  = (los & LOS_HIDDEN);

    int radius = 0;
    if (crawl_view.viewsz.x > crawl_view.viewsz.y)
        radius = crawl_view.viewsz.x - LOS_RADIUS - 1;
    else
        radius = crawl_view.viewsz.y - LOS_RADIUS - 1;

    const coord_def vyou = grid2view(you.pos());

    const int minx = vyou.x - radius, maxx = vyou.x + radius,
              miny = vyou.y - radius, maxy = vyou.y + radius,
              ctrx = vyou.x, ctry = vyou.y;

    while (temp_xps >= minx - 1 && temp_xps <= maxx
           && temp_yps <= maxy && temp_yps >= miny - 1)
    {
        if (direction == 1 && temp_xps == minx && temp_yps == maxy)
        {
            mfp = vyou;
            if (find_targ(you.pos(), mode, need_path, range))
                return (1);
            return (_find_square(mfp, direction,
                                 find_targ, need_path, mode, range, false,
                                 _next_los(direction, los, wrap)));
        }
        if (direction == -1 && temp_xps == ctrx && temp_yps == ctry)
        {
            mfp = coord_def(minx, maxy);
            return _find_square(mfp, direction, find_targ, need_path,
                                mode, range, false,
                                _next_los(direction, los, wrap));
        }

        if (direction == 1)
        {
            if (temp_xps == minx - 1)
            {
                x_change = 0;
                y_change = -1;
            }
            else if (temp_xps == ctrx && temp_yps == ctry)
            {
                x_change = -1;
                y_change = 0;
            }
            else if (abs(temp_xps - ctrx) <= abs(temp_yps - ctry))
            {
                if (temp_xps - ctrx >= 0 && temp_yps - ctry <= 0)
                {
                    if (abs(temp_xps - ctrx) > abs(temp_yps - ctry + 1))
                    {
                        x_change = 0;
                        y_change = -1;
                        if (temp_xps - ctrx > 0)
                            y_change = 1;
                        goto finished_spiralling;
                    }
                }
                x_change = -1;
                if (temp_yps - ctry < 0)
                    x_change = 1;
                y_change = 0;
            }
            else
            {
                x_change = 0;
                y_change = -1;
                if (temp_xps - ctrx > 0)
                    y_change = 1;
            }
        }                       // end if (direction == 1)
        else
        {
            // This part checks all eight surrounding squares to find the
            // one that leads on to the present square.
            for (i = -1; i < 2; ++i)
                for (j = -1; j < 2; ++j)
                {
                    if (i == 0 && j == 0)
                        continue;

                    if (temp_xps + i == minx - 1)
                    {
                        x_change = 0;
                        y_change = -1;
                    }
                    else if (temp_xps + i - ctrx == 0
                             && temp_yps + j - ctry == 0)
                    {
                        x_change = -1;
                        y_change = 0;
                    }
                    else if (abs(temp_xps + i - ctrx) <= abs(temp_yps + j - ctry))
                    {
                        const int xi = temp_xps + i - ctrx;
                        const int yj = temp_yps + j - ctry;

                        if (xi >= 0 && yj <= 0
                            && abs(xi) > abs(yj + 1))
                        {
                            x_change = 0;
                            y_change = -1;
                            if (xi > 0)
                                y_change = 1;
                            goto finished_spiralling;
                        }

                        x_change = -1;
                        if (yj < 0)
                            x_change = 1;
                        y_change = 0;
                    }
                    else
                    {
                        x_change = 0;
                        y_change = -1;
                        if (temp_xps + i - ctrx > 0)
                            y_change = 1;
                    }

                    if (temp_xps + i + x_change == temp_xps
                        && temp_yps + j + y_change == temp_yps)
                    {
                        goto finished_spiralling;
                    }
                }
        }                       // end else


      finished_spiralling:
        x_change *= direction;
        y_change *= direction;

        temp_xps += x_change;
        if (temp_yps + y_change <= maxy)  // it can wrap, unfortunately
            temp_yps += y_change;

        const int targ_x = you.pos().x + temp_xps - ctrx;
        const int targ_y = you.pos().y + temp_yps - ctry;

        if (!crawl_view.in_grid_viewport(coord_def(targ_x, targ_y)))
            continue;

        if (!in_bounds(targ_x, targ_y))
            continue;

        if ((onlyVis || onlyHidden) && onlyVis != in_los(targ_x, targ_y))
            continue;

        if (find_targ(coord_def(targ_x, targ_y), mode, need_path, range))
        {
            mfp.set(temp_xps, temp_yps);
            return (1);
        }
    }

    mfp = (direction > 0 ? coord_def(ctrx, ctry) : coord_def(minx, maxy));
    return (_find_square(mfp, direction, find_targ, need_path,
                         mode, range, false, _next_los(direction, los, wrap)));
}

// XXX Unbelievably hacky. And to think that my goal was to clean up the code.
// Identical to find_square, except that mfp is in grid coordinates
// rather than view coordinates.
static char _find_square_wrapper(coord_def& mfp, char direction,
                                 bool (*find_targ)(const coord_def& where, int mode,
                                                   bool need_path, int range),
                                 bool need_path, int mode, int range,
                                 bool wrap, int los )
{
    mfp = grid2view(mfp);
    const char r =  _find_square(mfp, direction, find_targ, need_path,
                                 mode, range, wrap, los);
    mfp = view2grid(mfp);
    return r;
}

static void _describe_feature(const coord_def& where, bool oos)
{
    if (oos && !is_terrain_seen(where))
        return;

    dungeon_feature_type grid = grd(where);
    if (grid == DNGN_SECRET_DOOR)
        grid = grid_secret_door_appearance(where);

    std::string desc;
    desc = feature_description(grid);

    if (desc.length())
    {
        if (oos)
            desc = "[" + desc + "]";

        msg_channel_type channel = MSGCH_EXAMINE;
        if (oos || grid == DNGN_FLOOR)
            channel = MSGCH_EXAMINE_FILTER;

        mpr(desc.c_str(), channel);
    }
}

// Returns a vector of features matching the given pattern.
std::vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern)
{
    std::vector<dungeon_feature_type> features;

    if (pattern.valid())
    {
        for (int i = 0; i < NUM_FEATURES; ++i)
        {
            std::string fdesc =
                feature_description(static_cast<dungeon_feature_type>(i));
            if (fdesc.empty())
                continue;

            if (pattern.matches( fdesc ))
                features.push_back( dungeon_feature_type(i) );
        }
    }
    return (features);
}

void describe_floor()
{
    dungeon_feature_type grid = grd(you.pos());

    std::string prefix = "There is ";
    std::string feat;
    std::string suffix = " here.";

    switch (grid)
    {
    case DNGN_FLOOR:
    case DNGN_FLOOR_SPECIAL:
        return;

    case DNGN_ENTER_SHOP:
        prefix = "There is an entrance to ";
        break;

    default:
        break;
    }

    feat = feature_description(you.pos(), is_bloodcovered(you.pos()),
                               DESC_NOCAP_A, false);
    if (feat.empty())
        return;

    msg_channel_type channel = MSGCH_EXAMINE;

    // Water is not terribly important if you don't mind it.
    if (feat_is_water(grid) && player_likes_water())
        channel = MSGCH_EXAMINE_FILTER;

    mpr((prefix + feat + suffix).c_str(), channel);
    if (grid == DNGN_ENTER_LABYRINTH && you.is_undead != US_UNDEAD)
        mpr("Beware, for starvation awaits!", MSGCH_EXAMINE);
}

std::string thing_do_grammar(description_level_type dtype,
                             bool add_stop,
                             bool force_article,
                             std::string desc)
{
    if (add_stop && (desc.empty() || desc[desc.length() - 1] != '.'))
        desc += ".";
    if (dtype == DESC_PLAIN || (!force_article && isupper(desc[0])))
    {
        if (isupper(desc[0]))
        {
            switch (dtype)
            {
            case DESC_PLAIN: case DESC_NOCAP_THE: case DESC_NOCAP_A:
                desc[0] = tolower(desc[0]);
                break;
            default:
                break;
            }
        }
        return (desc);
    }

    switch (dtype)
    {
    case DESC_CAP_THE:
        return "The " + desc;
    case DESC_NOCAP_THE:
        return "the " + desc;
    case DESC_CAP_A:
        return article_a(desc, false);
    case DESC_NOCAP_A:
        return article_a(desc, true);
    case DESC_NONE:
        return ("");
    default:
        return (desc);
    }
}

std::string feature_description(dungeon_feature_type grid,
                                trap_type trap, bool bloody,
                                description_level_type dtype,
                                bool add_stop, bool base_desc)
{
    std::string desc = raw_feature_description(grid, trap, base_desc);
    if (bloody)
        desc += ", spattered with blood";

    return thing_do_grammar(dtype, add_stop, feat_is_trap(grid), desc);
}

static std::string _base_feature_desc(dungeon_feature_type grid,
                                      trap_type trap)
{
    if (feat_is_trap(grid) && trap != NUM_TRAPS)
    {
        switch (trap)
        {
        case TRAP_DART:
            return ("dart trap");
        case TRAP_ARROW:
            return ("arrow trap");
        case TRAP_NEEDLE:
            return ("needle trap");
        case TRAP_BOLT:
            return ("bolt trap");
        case TRAP_SPEAR:
            return ("spear trap");
        case TRAP_AXE:
            return ("axe trap");
        case TRAP_BLADE:
            return ("blade trap");
        case TRAP_NET:
            return ("net trap");
        case TRAP_ALARM:
            return ("alarm trap");
        case TRAP_SHAFT:
            return ("shaft");
        case TRAP_TELEPORT:
            return ("teleportation trap");
        case TRAP_ZOT:
            return ("Zot trap");
        default:
            error_message_to_player();
            return ("undefined trap");
        }
    }

    switch (grid)
    {
    case DNGN_STONE_WALL:
        return ("stone wall");
    case DNGN_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        if (you.level_type == LEVEL_PANDEMONIUM)
            return ("wall of the weird stuff which makes up Pandemonium");
        else
            return ("rock wall");
    case DNGN_PERMAROCK_WALL:
        return ("unnaturally hard rock wall");
    case DNGN_OPEN_SEA:
        return ("open sea");
    case DNGN_CLOSED_DOOR:
        return ("closed door");
    case DNGN_DETECTED_SECRET_DOOR:
        return ("detected secret door");
    case DNGN_METAL_WALL:
        return ("metal wall");
    case DNGN_GREEN_CRYSTAL_WALL:
        return ("wall of green crystal");
    case DNGN_CLEAR_ROCK_WALL:
        return ("translucent rock wall");
    case DNGN_CLEAR_STONE_WALL:
        return ("translucent stone wall");
    case DNGN_CLEAR_PERMAROCK_WALL:
        return ("translucent unnaturally hard rock wall");
    case DNGN_TREES:
        return ("Trees");
    case DNGN_ORCISH_IDOL:
        if (you.species == SP_HILL_ORC)
           return ("idol of Beogh");
        else
           return ("orcish idol");
    case DNGN_WAX_WALL:
        return ("wall of solid wax");
    case DNGN_GRANITE_STATUE:
        return ("granite statue");
    case DNGN_LAVA:
        return ("Some lava");
    case DNGN_DEEP_WATER:
        return ("Some deep water");
    case DNGN_SHALLOW_WATER:
        return ("Some shallow water");
    case DNGN_UNDISCOVERED_TRAP:
    case DNGN_FLOOR:
    case DNGN_FLOOR_SPECIAL:
        return ("Floor");
    case DNGN_OPEN_DOOR:
        return ("open door");
    case DNGN_ESCAPE_HATCH_DOWN:
        return ("escape hatch in the floor");
    case DNGN_ESCAPE_HATCH_UP:
        return ("escape hatch in the ceiling");
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return ("stone staircase leading down");
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        if (player_in_hell())
            return ("gateway back to the Vestibule of Hell");
        return ("stone staircase leading up");
    case DNGN_ENTER_HELL:
        return ("gateway to Hell");
    case DNGN_EXIT_HELL:
        return ("gateway back into the Dungeon");
    case DNGN_TRAP_MECHANICAL:
        return ("mechanical trap");
    case DNGN_TRAP_MAGICAL:
        return ("magical trap");
    case DNGN_TRAP_NATURAL:
        return ("natural trap");
    case DNGN_ENTER_SHOP:
        return ("shop");
    case DNGN_ABANDONED_SHOP:
        return ("abandoned shop");
    case DNGN_ENTER_LABYRINTH:
        return ("labyrinth entrance");
    case DNGN_ENTER_DIS:
        return ("gateway to the Iron City of Dis");
    case DNGN_ENTER_GEHENNA:
        return ("gateway to Gehenna");
    case DNGN_ENTER_COCYTUS:
        return ("gateway to the freezing wastes of Cocytus");
    case DNGN_ENTER_TARTARUS:
        return ("gateway to the decaying netherworld of Tartarus");
    case DNGN_ENTER_ABYSS:
        return ("one-way gate to the infinite horrors of the Abyss");
    case DNGN_EXIT_ABYSS:
        return ("gateway leading out of the Abyss");
    case DNGN_STONE_ARCH:
        return ("empty arch of ancient stone");
    case DNGN_ENTER_PANDEMONIUM:
        return ("one-way gate leading to the halls of Pandemonium");
    case DNGN_EXIT_PANDEMONIUM:
        return ("gate leading out of Pandemonium");
    case DNGN_TRANSIT_PANDEMONIUM:
        return ("gate leading to another region of Pandemonium");
    case DNGN_ENTER_ORCISH_MINES:
        return ("staircase to the Orcish Mines");
    case DNGN_ENTER_HIVE:
        return ("staircase to the Hive");
    case DNGN_ENTER_LAIR:
        return ("staircase to the Lair");
    case DNGN_ENTER_SLIME_PITS:
        return ("staircase to the Slime Pits");
    case DNGN_ENTER_VAULTS:
        return ("staircase to the Vaults");
    case DNGN_ENTER_CRYPT:
        return ("staircase to the Crypt");
    case DNGN_ENTER_HALL_OF_BLADES:
        return ("staircase to the Hall of Blades");
    case DNGN_ENTER_ZOT:
        return ("gate to the Realm of Zot");
    case DNGN_ENTER_TEMPLE:
        return ("staircase to the Ecumenical Temple");
    case DNGN_ENTER_SNAKE_PIT:
        return ("staircase to the Snake Pit");
    case DNGN_ENTER_ELVEN_HALLS:
        return ("staircase to the Elven Halls");
    case DNGN_ENTER_TOMB:
        return ("staircase to the Tomb");
    case DNGN_ENTER_SWAMP:
        return ("staircase to the Swamp");
    case DNGN_ENTER_SHOALS:
        return ("staircase to the Shoals");
    case DNGN_ENTER_PORTAL_VAULT:
        // The bazaar description should be set in the bazaar marker; this
        // is the description for a portal of unknown type.
        return ("gate leading to a distant place");
    case DNGN_EXIT_PORTAL_VAULT:
        return ("gate leading back to the Dungeon");
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_TEMPLE:
        return ("staircase back to the Dungeon");
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
        return ("staircase back to the Lair");
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
        return ("staircase back to the Vaults");
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        return ("staircase back to the Mines");
    case DNGN_RETURN_FROM_TOMB:
        return ("staircase back to the Crypt");
    case DNGN_RETURN_FROM_ZOT:
        return ("gate leading back out of this place");

    // altars
    case DNGN_ALTAR_ZIN:
        return ("glowing silver altar of Zin");
    case DNGN_ALTAR_SHINING_ONE:
        return ("glowing golden altar of the Shining One");
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        return ("ancient bone altar of Kikubaaqudgha");
    case DNGN_ALTAR_YREDELEMNUL:
        return ("basalt altar of Yredelemnul");
    case DNGN_ALTAR_XOM:
        return ("shimmering altar of Xom");
    case DNGN_ALTAR_VEHUMET:
        return ("shining altar of Vehumet");
    case DNGN_ALTAR_OKAWARU:
        return ("iron altar of Okawaru");
    case DNGN_ALTAR_MAKHLEB:
        return ("burning altar of Makhleb");
    case DNGN_ALTAR_SIF_MUNA:
        return ("deep blue altar of Sif Muna");
    case DNGN_ALTAR_TROG:
        return ("bloodstained altar of Trog");
    case DNGN_ALTAR_NEMELEX_XOBEH:
        return ("sparkling altar of Nemelex Xobeh");
    case DNGN_ALTAR_ELYVILON:
        return ("white marble altar of Elyvilon");
    case DNGN_ALTAR_LUGONU:
        return ("corrupted altar of Lugonu");
    case DNGN_ALTAR_BEOGH:
        return ("roughly hewn altar of Beogh");
    case DNGN_ALTAR_JIYVA:
        return ("viscous altar of Jiyva");
    case DNGN_ALTAR_FEDHAS:
        return ("blossoming altar of Fedhas");
    case DNGN_ALTAR_CHEIBRIADOS:
        return ("snail-covered altar of Cheibriados");

    case DNGN_FOUNTAIN_BLUE:
        return ("fountain of clear blue water");
    case DNGN_FOUNTAIN_SPARKLING:
        return ("fountain of sparkling water");
    case DNGN_FOUNTAIN_BLOOD:
        return ("fountain of blood");
    case DNGN_DRY_FOUNTAIN_BLUE:
    case DNGN_DRY_FOUNTAIN_SPARKLING:
    case DNGN_DRY_FOUNTAIN_BLOOD:
    case DNGN_PERMADRY_FOUNTAIN:
        return ("dry fountain");
    default:
        return ("");
    }
}

std::string raw_feature_description(dungeon_feature_type grid,
                                    trap_type trap, bool base_desc)
{
    std::string base_str = _base_feature_desc(grid, trap);

    if (base_desc)
        return (base_str);

    if (you.level_type == LEVEL_DUNGEON)
    {
        switch (you.where_are_you)
        {
            case BRANCH_SLIME_PITS:
                if (grid == DNGN_ROCK_WALL)
                    base_str = "slime covered rock wall";
                break;
            default:
                break;
        }
    }

    desc_map::iterator i = base_desc_to_short.find(base_str);

    if (i != base_desc_to_short.end())
        return (i->second);

    return (base_str);
}

void set_feature_desc_short(dungeon_feature_type grid,
                            const std::string &desc)
{
    set_feature_desc_short(_base_feature_desc(grid, NUM_TRAPS), desc);
}

void set_feature_desc_short(const std::string &base_name,
                            const std::string &_desc)
{
    ASSERT(!base_name.empty());

    CrawlHashTable &props = env.properties;

    if (!props.exists(SHORT_DESC_KEY))
        props[SHORT_DESC_KEY].new_table();

    CrawlHashTable &desc_table = props[SHORT_DESC_KEY].get_table();

    if (_desc.empty())
    {
        base_desc_to_short.erase(base_name);
        desc_table.erase(base_name);
    }
    else
    {
        std::string desc = replace_all(_desc, "$BASE", base_name);
        base_desc_to_short[base_name] = desc;
        desc_table[base_name]         = desc;
    }
}

void setup_feature_descs_short()
{
    base_desc_to_short.clear();

    const CrawlHashTable &props = env.properties;

    if (!props.exists(SHORT_DESC_KEY))
        return;

    const CrawlHashTable &desc_table = props[SHORT_DESC_KEY].get_table();

    CrawlHashTable::const_iterator i;
    for (i = desc_table.begin(); i != desc_table.end(); ++i)
        base_desc_to_short[i->first] = i->second.get_string();
}

#ifndef DEBUG_DIAGNOSTICS
// Is a feature interesting enough to 'v'iew it, even if a player normally
// doesn't care about descriptions, i.e. does the description hold important
// information? (Yes, this is entirely subjective. --jpeg)
static bool _interesting_feature(dungeon_feature_type feat)
{
    return (get_feature_def(feat).flags & FFT_EXAMINE_HINT);
}
#endif

std::string feature_description(const coord_def& where, bool bloody,
                                description_level_type dtype, bool add_stop,
                                bool base_desc)
{
    std::string marker_desc =
        env.markers.property_at(where, MAT_ANY, "feature_description");

    if (!marker_desc.empty())
    {
        if (bloody)
            marker_desc += ", spattered with blood";

        return thing_do_grammar(dtype, add_stop, false, marker_desc);
    }

    dungeon_feature_type grid = grd(where);
    if (grid == DNGN_SECRET_DOOR)
        grid = grid_secret_door_appearance(where);

    if (grid == DNGN_OPEN_DOOR || feat_is_closed_door(grid))
    {
        const std::string door_desc_prefix =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_prefix");
        const std::string door_desc_suffix =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_suffix");
        const std::string door_desc_noun =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_noun");
        const std::string door_desc_adj  =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_adjective");
        const std::string door_desc_veto =
            env.markers.property_at(where, MAT_ANY,
                                    "door_description_veto");

        std::set<coord_def> all_door;
        find_connected_identical(where, grd(where), all_door);
        const char *adj, *noun;
        get_door_description(all_door.size(), &adj, &noun);

        std::string desc;
        if (!door_desc_adj.empty())
            desc += door_desc_adj;
        else
            desc += adj;

        if (door_desc_veto.empty() || door_desc_veto != "veto")
        {
            desc += (grid == DNGN_OPEN_DOOR) ? "open " : "closed ";
            if (grid == DNGN_DETECTED_SECRET_DOOR)
                desc += "detected secret ";
        }

        desc += door_desc_prefix;

        if (!door_desc_noun.empty())
            desc += door_desc_noun;
        else
            desc += noun;

        desc += door_desc_suffix;

        if (bloody)
            desc += ", spattered with blood";

        return thing_do_grammar(dtype, add_stop, false, desc);
    }

    if (grid == DNGN_OPEN_SEA)
    {
        switch (dtype)
        {
        case DESC_CAP_A:   dtype = DESC_CAP_THE;   break;
        case DESC_NOCAP_A: dtype = DESC_NOCAP_THE; break;
        default: break;
        }
    }

    switch (grid)
    {
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_NATURAL:
        return (feature_description(grid, get_trap_type(where), bloody,
                                    dtype, add_stop, base_desc));
    case DNGN_ABANDONED_SHOP:
        return thing_do_grammar(dtype, add_stop, false, "An abandoned shop");

    case DNGN_ENTER_SHOP:
        return shop_name(where, add_stop);

    case DNGN_ENTER_PORTAL_VAULT:
        // Should have been handled at the top of the function.
        return (thing_do_grammar(
                    dtype, add_stop, false,
                    "UNAMED PORTAL VAULT ENTRY"));
    default:
        return (feature_description(grid, NUM_TRAPS, bloody, dtype, add_stop,
                                    base_desc));
    }
}

static std::string _describe_mons_enchantment(const monsters &mons,
                                              const mon_enchant &ench,
                                              bool paralysed)
{
    // Suppress silly-looking combinations, even if they're
    // internally valid.
    if (paralysed && (ench.ench == ENCH_SLOW || ench.ench == ENCH_HASTE
                      || ench.ench == ENCH_SWIFT
                      || ench.ench == ENCH_PETRIFIED
                      || ench.ench == ENCH_PETRIFYING))
    {
        return "";
    }

    if ((ench.ench == ENCH_HASTE || ench.ench == ENCH_BATTLE_FRENZY
            || ench.ench == ENCH_MIGHT)
        && mons.berserk())
    {
        return "";
    }

    if (ench.ench == ENCH_HASTE && mons.has_ench(ENCH_SLOW))
        return "";

    if (ench.ench == ENCH_SLOW && mons.has_ench(ENCH_HASTE))
        return "";

    if (ench.ench == ENCH_PETRIFIED && mons.has_ench(ENCH_PETRIFYING))
        return "";

    switch (ench.ench)
    {
    case ENCH_POISON:        return "poisoned";
    case ENCH_SICK:          return "sick";
    case ENCH_ROT:           return "rotting away"; //jmf: "covered in sores"?
    case ENCH_CORONA:     return "softly glowing";
    case ENCH_SLOW:          return "moving slowly";
    case ENCH_BERSERK:       return "berserk";
    case ENCH_BATTLE_FRENZY: return "consumed by blood-lust";
    case ENCH_HASTE:         return "moving very quickly";
    case ENCH_MIGHT:         return "unusually strong";
    case ENCH_CONFUSION:     return "bewildered and confused";
    case ENCH_INVIS:         return "slightly transparent";
    case ENCH_CHARM:         return "in your thrall";
    case ENCH_STICKY_FLAME:  return "covered in liquid flames";
    case ENCH_HELD:          return "entangled in a net";
    case ENCH_PETRIFIED:     return "petrified";
    case ENCH_PETRIFYING:    return "slowly petrifying";
    case ENCH_LOWERED_MR:    return "susceptible to magic";
    case ENCH_SWIFT:         return "moving somewhat quickly";
    default:                 return "";
    }
}

static std::string _describe_monster_weapon(const monsters *mons)
{
    std::string desc = "";
    std::string name1, name2;
    const item_def *weap = mons->mslot_item(MSLOT_WEAPON);
    const item_def *alt  = mons->mslot_item(MSLOT_ALT_WEAPON);

    if (weap)
    {
        name1 = weap->name(DESC_NOCAP_A, false, false, true,
                           false, ISFLAG_KNOW_CURSE);
    }
    if (alt && mons_wields_two_weapons(mons))
    {
        name2 = alt->name(DESC_NOCAP_A, false, false, true,
                          false, ISFLAG_KNOW_CURSE);
    }

    if (name1.empty() && !name2.empty())
        name1.swap(name2);

    if (name1 == name2 && weap)
    {
        item_def dup = *weap;
        ++dup.quantity;
        name1 = dup.name(DESC_NOCAP_A, false, false, true, true,
                         ISFLAG_KNOW_CURSE);
        name2.clear();
    }

    if (name1.empty())
        return (desc);

    desc += " wielding ";
    desc += name1;

    if (!name2.empty())
    {
        desc += " and ";
        desc += name2;
    }

    return (desc);
}

#ifdef DEBUG_DIAGNOSTICS
static std::string _stair_destination_description(const coord_def &pos)
{
    if (LevelInfo *linf = travel_cache.find_level_info(level_id::current()))
    {
        const stair_info *si = linf->get_stair(pos);
        if (si)
            return (" " + si->describe());
        else if (feat_is_stair(grd(pos)))
            return (" (unknown stair)");
    }
    return ("");
}
#endif

std::string _mon_enchantments_string(const monsters* mon)
{
    const bool paralysed = mon->paralysed();
    std::vector<std::string> enchant_descriptors;

    for (mon_enchant_list::const_iterator e = mon->enchantments.begin();
         e != mon->enchantments.end(); ++e)
    {
        const std::string tmp =
            _describe_mons_enchantment(*mon, e->second, paralysed);

        if (!tmp.empty())
            enchant_descriptors.push_back(tmp);
    }
    if (paralysed)
        enchant_descriptors.push_back("paralysed");

    if (!enchant_descriptors.empty())
    {
        return
            mon->pronoun(PRONOUN_CAP)
            + " is "
            + comma_separated_line(enchant_descriptors.begin(),
                                   enchant_descriptors.end())
            + ".";
    }
    else
        return "";
}

// Returns the description string for a given monster, including attitude
// and enchantments but not equipment or wounds.
static std::string _get_monster_desc(const monsters *mon)
{
    std::string text    = "";
    std::string pronoun = mon->pronoun(PRONOUN_CAP);

    if (you.beheld_by(mon))
        text += "You are mesmerised by her song.\n";

    if (!mons_is_mimic(mon->type) && mons_behaviour_perceptible(mon))
    {
        if (mon->asleep())
        {
            text += pronoun + " appears to be "
                    + (mons_is_confused(mon, true) ? "sleepwalking"
                                                   : "resting")
                    + ".\n";
        }
        // Applies to both friendlies and hostiles
        else if (mons_is_fleeing(mon))
            text += pronoun + " is retreating.\n";
        // hostile with target != you
        else if (!mon->friendly() && !mon->neutral()
                 && mon->foe != MHITYOU && !crawl_state.arena_suspended)
        {
            // Special case: batty monsters get set to BEH_WANDER as
            // part of their special behaviour.
            if (!mons_is_batty(mon))
                text += pronoun + " doesn't appear to have noticed you.\n";
        }
    }

    if (mon->attitude == ATT_FRIENDLY)
        text += pronoun + " is friendly.\n";
    else if (mon->good_neutral())
        text += pronoun + " seems to be peaceful towards you.\n";
    else if (mon->neutral()) // don't differentiate between permanent or not
        text += pronoun + " is indifferent to you.\n";

    if (mon->is_summoned() && (mon->type != MONS_RAKSHASA_FAKE
                               && mon->type != MONS_MARA_FAKE))
    {
        text += pronoun + " has been summoned.\n";
    }

    if (mon->haloed())
        text += pronoun + " is illuminated by a divine halo.\n";

    if (mons_intel(mon) <= I_PLANT && mon->type != MONS_RAKSHASA_FAKE)
        text += pronoun + " is mindless.\n";

    if (mons_enslaved_body_and_soul(mon))
    {
        text += mon->pronoun(PRONOUN_CAP_POSSESSIVE)
                + " soul is ripe for the taking.\n";
    }
    else if (mons_enslaved_soul(mon))
        text += pronoun + " is a disembodied soul.\n";

    dungeon_feature_type blocking_feat;
    if (!crawl_state.arena_suspended
        && mon->pos() != you.pos()
        && _blocked_ray(mon->pos(), &blocking_feat))
    {
        text += "Your line of fire to " + mon->pronoun(PRONOUN_OBJECTIVE)
              + " is blocked by "
              + feature_description(blocking_feat, NUM_TRAPS, false,
                                    DESC_NOCAP_A)
              + "\n";
    }

    text += _mon_enchantments_string(mon);
    return text;
}

static void _describe_monster(const monsters *mon)
{
    // First print type and equipment.
    std::string text = get_monster_equipment_desc(mon) + ".";
    print_formatted_paragraph(text, MSGCH_EXAMINE);

    print_wounds(mon);

    // Print the rest of the description.
    text = _get_monster_desc(mon);
    if (!text.empty())
        print_formatted_paragraph(text, MSGCH_EXAMINE);
}

// This method is called in two cases:
// a) Monsters coming into view: "An ogre comes into view. It is wielding ..."
// b) Monster description via 'x': "An ogre, wielding a club, and wearing ..."
std::string get_monster_equipment_desc(const monsters *mon, bool full_desc,
                                       description_level_type mondtype,
                                       bool print_attitude)
{
    std::string desc = "";
    if (mondtype != DESC_NONE)
    {
        if (print_attitude && mon->type == MONS_PLAYER_GHOST)
            desc = get_ghost_description(*mon);
        else
            desc = mon->full_name(mondtype);

        if (print_attitude)
        {
            std::string str = "";
            if (mon->friendly())
                str = "friendly";
            else if (mon->neutral())
                str = "neutral";

            if (mon->is_summoned())
            {
                if (!str.empty())
                    str += ", ";
                str += "summoned";
            }

            if (mon->type == MONS_DANCING_WEAPON
                || mon->type == MONS_PANDEMONIUM_DEMON
                || mon->type == MONS_PLAYER_GHOST
                || mons_is_known_mimic(mon))
            {
                if (!str.empty())
                    str += " ";

                if (mon->type == MONS_DANCING_WEAPON)
                    str += "dancing weapon";
                else if (mon->type == MONS_PANDEMONIUM_DEMON)
                    str += "pandemonium demon";
                else if (mon->type == MONS_PLAYER_GHOST)
                    str += "ghost";
                else
                    str += "mimic";
            }
            if (!str.empty())
                desc += " (" + str + ")";
        }
    }

    std::string weap = "";

    // We don't report rakshasa equipment in order not to give away the
    // true rakshasa when it summons. But Mara is fine, because his weapons
    // and armour are cloned with him.

    if (mon->type != MONS_DANCING_WEAPON
        && (mon->type != MONS_RAKSHASA || mon->friendly()))
    {
        weap = _describe_monster_weapon(mon);
    }

    if (!weap.empty())
    {
        if (full_desc)
            desc += ",";
        desc += weap;
    }

    // Print the rest of the equipment only for full descriptions.
    if (full_desc && ((mon->type != MONS_RAKSHASA && mon->type != MONS_MARA
                       && mon->type != MONS_MARA_FAKE) || mon->friendly()))
    {
        const int mon_arm = mon->inv[MSLOT_ARMOUR];
        const int mon_shd = mon->inv[MSLOT_SHIELD];
        const int mon_qvr = mon->inv[MSLOT_MISSILE];
        const int mon_alt = mon->inv[MSLOT_ALT_WEAPON];

        const bool need_quiver  = (mon_qvr != NON_ITEM && mon->friendly());
        const bool need_alt_wpn = (mon_alt != NON_ITEM && mon->friendly()
                                   && !mons_wields_two_weapons(mon));
              bool found_sth    = !weap.empty();


        if (mon_arm != NON_ITEM)
        {
            desc += ", ";
            if (found_sth && mon_shd == NON_ITEM && !need_quiver
                          && !need_alt_wpn)
            {
                desc += "and ";
            }
            desc += "wearing ";
            desc += mitm[mon_arm].name(DESC_NOCAP_A);
            if (!found_sth)
                found_sth = true;
        }

        if (mon_shd != NON_ITEM)
        {
            desc += ", ";
            if (found_sth && !need_quiver && !need_alt_wpn)
                desc += "and ";
            desc += "wearing ";
            desc += mitm[mon_shd].name(DESC_NOCAP_A);
            if (!found_sth)
                found_sth = true;
        }

        // For friendly monsters, also list quivered missiles
        // and alternate weapon.
        if (mon->friendly())
        {
            if (mon_qvr != NON_ITEM)
            {
                desc += ", ";
                if (found_sth && !need_alt_wpn)
                    desc += "and ";
                desc += "quivering ";
                desc += mitm[mon_qvr].name(DESC_NOCAP_A);
                if (!found_sth)
                    found_sth = true;
            }

            if (need_alt_wpn)
            {
                desc += ", ";
                if (found_sth)
                    desc += "and ";
                desc += "carrying ";
                desc += mitm[mon_alt].name(DESC_NOCAP_A);
            }
        }
    }

    return desc;
}

// Describe a cell, guaranteed to be in view.
static void _describe_cell(const coord_def& where, bool in_range)
{
    bool mimic_item = false;
    bool monster_described = false;
    bool cloud_described = false;
    bool item_described = false;

    if (where == you.pos() && !crawl_state.arena_suspended)
        mpr("You.", MSGCH_EXAMINE_FILTER);

    if (const monsters* mon = monster_at(where))
    {
        if (_mon_submerged_in_water(mon))
        {
            mpr("There is a strange disturbance in the water here.",
                MSGCH_EXAMINE_FILTER);
        }
        else if (_mon_exposed_in_cloud(mon))
        {
            mpr("There is a strange disturbance in the cloud here.",
                MSGCH_EXAMINE_FILTER);
        }

#if DEBUG_DIAGNOSTICS
        if (!mon->visible_to(&you))
            mpr("There is a non-visible monster here.", MSGCH_DIAGNOSTICS);
#else
        if (!mon->visible_to(&you))
            goto look_clouds;
#endif

        if (mons_is_mimic(mon->type))
        {
            if (mons_is_known_mimic(mon))
                _describe_monster(mon);
            else
            {
                std::string name = get_menu_colour_prefix_tags(get_mimic_item(mon),
                                                               DESC_NOCAP_A);
                mprf(MSGCH_FLOOR_ITEMS, "You see %s here.", name.c_str());
            }
            mimic_item = true;
            item_described = true;
        }
        else
        {
            _describe_monster(mon);

            if (!in_range)
            {
                mprf(MSGCH_EXAMINE_FILTER, "%s is out of range.",
                     mon->pronoun(PRONOUN_CAP).c_str());
            }
            monster_described = true;
        }

#if DEBUG_DIAGNOSTICS
        debug_stethoscope(mgrd(where));
#endif
        if (Tutorial.tutorial_left && tutorial_monster_interesting(mon))
        {
            std::string msg;
#ifdef USE_TILE
            msg = "(<w>Right-click</w> for more information.)";
#else
            msg = "(Press <w>v</w> for more information.)";
#endif
            print_formatted_paragraph(msg);
        }
    }

#if (!DEBUG_DIAGNOSTICS)
  // removing warning
  look_clouds:
#endif

    if (is_sanctuary(where))
    {
        mprf("This square lies inside a sanctuary%s.",
             silenced(where) ? ", and is shrouded in silence" : "");
    }
    else if (silenced(where))
        mpr("This square is shrouded in silence.");

    if (env.cgrid(where) != EMPTY_CLOUD)
    {
        const int cloud_inspected = env.cgrid(where);

        mprf(MSGCH_EXAMINE, "There is a cloud of %s here.",
             cloud_name(cloud_inspected).c_str());

        cloud_described = true;
    }

    int targ_item = you.visible_igrd(where);

    if (targ_item != NON_ITEM)
    {
        // If a mimic is on this square, we pretend it's the first item - bwr
        if (mimic_item)
            mpr("There is something else lying underneath.", MSGCH_FLOOR_ITEMS);
        else
        {
            if (mitm[ targ_item ].base_type == OBJ_GOLD)
                mprf(MSGCH_FLOOR_ITEMS, "A pile of gold coins.");
            else
            {
                std::string name = get_menu_colour_prefix_tags(mitm[targ_item],
                                                               DESC_NOCAP_A);
                mprf(MSGCH_FLOOR_ITEMS, "You see %s here.", name.c_str());
            }

            if (mitm[ targ_item ].link != NON_ITEM)
            {
                mprf(MSGCH_FLOOR_ITEMS,
                     "There is something else lying underneath.");
            }
        }
        item_described = true;
    }

    bool bloody = false;
    if (is_bloodcovered(where))
        bloody = true;

    std::string feature_desc = feature_description(where, bloody);
#ifdef DEBUG_DIAGNOSTICS
    std::string marker;
    if (map_marker *mark = env.markers.find(where, MAT_ANY))
    {
        std::string desc = mark->debug_describe();
        if (desc.empty())
            desc = "?";
        marker = " (" + desc + ")";
    }
    const std::string traveldest = _stair_destination_description(where);
    std::string height_desc;
    if (env.heightmap.get())
        height_desc = make_stringf(" (height: %d)", (*env.heightmap)(where));
    const dungeon_feature_type feat = grd(where);
    mprf(MSGCH_DIAGNOSTICS, "(%d,%d): %s - %s (%d/%s)%s%s%s",
         where.x, where.y,
         stringize_glyph(get_screen_glyph(where)).c_str(),
         feature_desc.c_str(),
         feat,
         dungeon_feature_name(feat),
         marker.c_str(),
         traveldest.c_str(),
         height_desc.c_str());
#else
    if (Tutorial.tutorial_left && tutorial_pos_interesting(where.x, where.y))
    {
#ifdef USE_TILE
        feature_desc += " (<w>Right-click</w> for more information.)";
#else
        feature_desc += " (Press <w>v</w> for more information.)";
#endif
        print_formatted_paragraph(feature_desc);
    }
    else
    {
        const dungeon_feature_type feat = grd(where);

        if (_interesting_feature(feat))
        {
#ifdef USE_TILE
            feature_desc += " (Right-click for more information.)";
#else
            feature_desc += " (Press 'v' for more information.)";
#endif
        }

        // Suppress "Floor." if there's something on that square that we've
        // already described.
        if ((feat == DNGN_FLOOR || feat == DNGN_FLOOR_SPECIAL) && !bloody
            && (monster_described || item_described || cloud_described))
        {
            return;
        }

        msg_channel_type channel = MSGCH_EXAMINE;
        if (feat == DNGN_FLOOR
            || feat == DNGN_FLOOR_SPECIAL
            || feat_is_water(feat))
        {
            channel = MSGCH_EXAMINE_FILTER;
        }

        mpr(feature_desc.c_str(), channel);
    }
#endif
}

///////////////////////////////////////////////////////////////////////////
// targetting_behaviour

targetting_behaviour::targetting_behaviour(bool look_around)
    : just_looking(look_around), compass(false)
{
}

targetting_behaviour::~targetting_behaviour()
{
}

int targetting_behaviour::get_key()
{
    if (!crawl_state.is_replaying_keys())
        flush_input_buffer(FLUSH_BEFORE_COMMAND);

    return unmangle_direction_keys(getchm(KMC_TARGETTING), KMC_TARGETTING,
                                   false, false);
}

command_type targetting_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    command_type cmd = key_to_command(key, KMC_TARGETTING);
    if (cmd >= CMD_MIN_TARGET && cmd < CMD_TARGET_CYCLE_TARGET_MODE)
        return (cmd);

#ifndef USE_TILE
    // Overrides the movement keys while mlist_targetting is active.
    if (crawl_state.mlist_targetting && islower(key))
        return static_cast<command_type> (CMD_TARGET_CYCLE_MLIST + (key - 'a'));
#endif

    // XXX: hack
    if (cmd == CMD_TARGET_SELECT && key == ' ' && just_looking)
        cmd = CMD_TARGET_CANCEL;

    return (cmd);
}

bool targetting_behaviour::should_redraw()
{
    return (false);
}

void targetting_behaviour::mark_ammo_nonchosen()
{
    // Nothing to be done.
}

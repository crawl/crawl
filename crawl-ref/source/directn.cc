/*
 *  File:       direct.cc
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "directn.h"
#include "format.h"

#include <cstdarg>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#ifdef DOS
 #include <conio.h>
#endif

#include "externs.h"

#include "cio.h"
#include "cloud.h"
#include "command.h"
#include "debug.h"
#include "describe.h"
#include "dungeon.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "items.h"
#include "mapmark.h"
#include "message.h"
#include "menu.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "shopping.h"
#include "state.h"
#include "stuff.h"
#include "spells4.h"
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
#include "output.h"

#include "macro.h"

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

static char _find_square_wrapper( const coord_def& targ,
                                  coord_def &mfp, char direction,
                                  bool (*find_targ)(const coord_def&, int, bool, int),
                                  bool need_path = false, int mode = TARG_ANY,
                                  int range = -1, bool wrap = false,
                                  int los = LOS_ANY);

static char _find_square( const coord_def& where,
                          coord_def &mfp, int direction,
                          bool (*find_targ)(const coord_def&, int, bool, int),
                          bool need_path, int mode = TARG_ANY, int range = -1,
                          bool wrap = false, int los = LOS_ANY);

static int  _targeting_cmd_to_compass( command_type command );
static void _describe_oos_square(const coord_def& where);
static void _extend_move_to_edge(dist &moves);

void direction_choose_compass( dist& moves, targeting_behaviour *beh)
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
            moves.isMe = delta.origin();
            break;
        }
#endif

        if (key_command == CMD_TARGET_SELECT)
        {
            moves.delta.reset();
            moves.isMe = true;
            break;
        }

        const int i = _targeting_cmd_to_compass(key_command);
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

static int _targeting_cmd_to_compass( command_type command )
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

static int _targeting_cmd_to_feature( command_type command )
{
    switch ( command )
    {
    case CMD_TARGET_FIND_TRAP:
        return '^';
    case CMD_TARGET_FIND_PORTAL:
        return '\\';
    case CMD_TARGET_FIND_ALTAR:
        return  '_';
    case CMD_TARGET_FIND_UPSTAIR:
        return '<';
    case CMD_TARGET_FIND_DOWNSTAIR:
        return '>';
    default:
        return 0;
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

static void draw_ray_glyph(const coord_def &pos, int colour,
                           int glych, int mcol, bool in_range)
{
#ifdef USE_TILE
    tile_place_ray(pos, in_range);
#else
    int mid = mgrd(pos);
    if (mid != NON_MONSTER)
    {
        const monsters *mons = &menv[mid];
        if (mons->alive() && player_monster_visible(mons))
        {
            glych  = get_screen_glyph(pos.x, pos.y);
            colour = mcol;
        }
    }
    const coord_def vp = grid2view(pos);
    cgotoxy(vp.x, vp.y, GOTO_DNGN);
    textcolor( real_colour(colour) );
    putch(glych);
#endif
}

// Unseen monsters in shallow water show a "strange disturbance".
// (Unless flying!)
static bool _mon_submerged_in_water(const monsters *mon)
{
    if (!mon)
        return (false);

    return (grd(mon->pos()) == DNGN_SHALLOW_WATER
            && see_grid(mon->pos())
            && !player_monster_visible(mon)
            && !mons_flies(mon));
}

static bool _is_target_in_range(const coord_def& where, int range)
{
    // Range doesn't matter.
    if (range == -1)
        return (true);

    return (grid_distance(you.pos(), where) <= range);
}

// We handle targeting for repeating commands and re-doing the
// previous command differently (i.e., not just letting the keys
// stuffed into the macro buffer replay as-is) because if the player
// targeted a monster using the movement keys and the monster then
// moved between repetitions, then simply replaying the keys in the
// buffer will target an empty square.
static void _direction_again(dist& moves, targeting_type restricts,
                             targ_mode_type mode, int range, bool just_looking,
                             const char *prompt, targeting_behaviour *beh)
{
    moves.isValid       = false;
    moves.isTarget      = false;
    moves.isMe          = false;
    moves.isCancel      = false;
    moves.isEndpoint    = false;
    moves.choseRay      = false;

    if (you.prev_targ == MHITNOT && you.prev_grd_targ == coord_def(0, 0))
    {
        moves.isCancel = true;
        crawl_state.cancel_cmd_repeat();
        return;
    }

    int targ_types = 0;
    if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
        targ_types++;
    if (you.prev_targ == MHITYOU)
        targ_types++;
    if (you.prev_grd_targ != coord_def(0, 0))
        targ_types++;
    ASSERT(targ_types == 1);

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
            || key_command == CMD_TARGET_MAYBE_PREV_TARGET)
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
        if (!see_grid(you.prev_grd_targ))
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

        ray_def ray;
        find_ray(you.pos(), moves.target, true, ray, 0, true);
        moves.ray = ray;
    }
    else if (you.prev_targ == MHITYOU)
    {
        moves.isMe = true;
        moves.target = you.pos();

        // Discard 'Y' player gave to yesno()
        if (mode == TARG_ENEMY)
            getchm();
    }
    else
    {
        const monsters *montarget = &menv[you.prev_targ];

        if (!mons_near(montarget)
            || !player_monster_visible( montarget ))
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

        ray_def ray;
        find_ray(you.pos(), moves.target, true, ray, 0, true);
        moves.ray = ray;
    }

    moves.isValid  = true;
    moves.isTarget = true;
}

static void _describe_monster(const monsters *mon);

// Lists all the monsters and items currently in view by the player.
// TODO: Allow sorting of items lists.
void full_describe_view()
{
    const coord_def start = view2grid(coord_def(1,1));
    const coord_def end = start + crawl_view.viewsz - coord_def(1,1);

    std::vector<const monsters*> list_mons;
    std::vector<item_def> list_items;

    // Iterate over viewport and grab all items known (or thought)
    // to be in the stashes in view.
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        if (!in_bounds(*ri) || !see_grid(*ri))
            continue;

        const int oid = igrd(*ri);

        if (oid == NON_ITEM)
            continue;

        std::vector<item_def> items = item_list_in_stash(*ri);
        if (items.empty())
            continue;

        list_items.insert(list_items.end(), items.begin(), items.end());
    }

    // Get monsters via the monster_pane_info, sorted by difficulty.
    std::vector<monster_pane_info> mons;
    get_monster_pane_info(mons);
    std::sort(mons.begin(), mons.end(), monster_pane_info::less_than_wrapper);

    for (unsigned int i = 0; i < mons.size(); i++)
        list_mons.push_back(mons[i].m_mon);

    if (list_mons.empty() && list_items.empty())
    {
        mprf("Neither monsters nor items are visible.");
        return;
    }
/*
    InvMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE |
                      MF_ALLOW_FORMATTING,
                      "description", false);
*/
    InvMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE |
                      /*MF_ALWAYS_SHOW_MORE |*/ MF_ALLOW_FORMATTING);

    desc_menu.set_highlighter(NULL);
    desc_menu.set_title(
        new MenuEntry("Visible Monsters/Items (select for more detail):",
                      MEL_TITLE));

    desc_menu.set_tag("description");

    int menu_index = -1;
    // Build menu entries for monsters.
    if (!list_mons.empty())
    {
        desc_menu.add_entry( new MenuEntry("Monsters", MEL_SUBTITLE) );
        for (unsigned int i = 0; i < list_mons.size(); ++i)
        {
            // List monsters in the form
            // (A) An angel (neutral), wielding a glowing long sword

            ++menu_index;
            const char letter = index_to_letter(menu_index);

            // Get colour-coded letter.
            unsigned char colour = mons_class_colour(list_mons[i]->type);
            if (colour == BLACK)
                colour = LIGHTGREY;

            std::string prefix = "(<";
            prefix += colour_to_str(colour);
            prefix += ">";
            prefix += stringize_glyph(mons_char( list_mons[i]->type) );
            prefix += "</";
            prefix += colour_to_str(colour);
            prefix += ">) ";

            // Get damage level.
            std::string damage_desc;

            mon_dam_level_type damage_level;
            mons_get_damage_level(list_mons[i], damage_desc, damage_level);

            // If no messages about wounds, don't display damage level either.
            if (monster_descriptor(list_mons[i]->type, MDSC_NOMSG_WOUNDS))
                damage_level = MDAM_OKAY;

            std::vector<formatted_string> fss;
            std::string str = get_monster_desc(list_mons[i], true, DESC_CAP_A,
                                               true);
            if (player_mesmerised_by(list_mons[i]))
                str += ", keeping you mesmerised";

            if (damage_level != MDAM_OKAY)
                str += ", " + damage_desc;

            // Wraparound if the description is longer than allowed.
            linebreak_string2(str, get_number_of_cols() - 8);
            formatted_string::parse_string_to_multiple(str, fss);
            MenuEntry *me;
            for (unsigned int j = 0; j < fss.size(); j++)
            {
                if (j == 0)
                {
                    me = new MenuEntry(prefix + str, MEL_ITEM, 1, letter);
                    me->data = (void*) list_mons[i];
                    me->tag = "m";
                    me->quantity = 1; // Hack to make monsters selectable.
                }
                else
                {
                    str = "        " + fss[j].tostring();
                    me = new MenuEntry(str, MEL_ITEM, 1);
                }

                desc_menu.add_entry(me);
            }
        }
    }

    // Build menu entries for items.
    if (list_items.size())
    {
        desc_menu.add_entry( new MenuEntry( "Items", MEL_SUBTITLE ) );
        for (unsigned int i = 0; i < list_items.size(); ++i)
        {
            ++menu_index;
            const char letter = index_to_letter(menu_index);
            const item_def &item = list_items[i];

            InvEntry *me = new InvEntry(item);

#ifndef USE_TILE
            // Show glyphs only for ASCII.
            me->set_show_glyph(true);
#endif
            me->tag = "i";
            me->hotkeys[0] = letter;
            me->quantity = 2; // Hack to make items selectable.
            desc_menu.add_entry(me);
        }
    }

    // Menu loop
    while (true)
    {
        std::vector<MenuEntry*> sel = desc_menu.show();
        redraw_screen();

        if (sel.empty())
            break;

        // Possibility to select a menu item to get full description
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
            mesclr();
            _describe_monster( m );

            if (getch() == 0)
                getch();
        }
        else if (quant == 2)
        {
            // Get selected item.
            item_def* i = (item_def*)(sel[0]->data);
            describe_item( *i );
        }
    }

#ifndef USE_TILE
    if (!list_items.empty())
    {
        // Unset show_glyph for other menus.
        InvEntry *me = new InvEntry(list_items[0]);
        me->set_show_glyph(false);
    }
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
std::vector<monster_pane_info> mlist;
static void _fill_monster_list(bool full_info)
{
    std::vector<monster_pane_info> temp;
    get_monster_pane_info(temp);
    mlist_full_info = full_info;

    // Get the unique entries.
    mlist.clear();
    unsigned int start = 0, end = 1;
    while (start < temp.size())
    {
        mlist.push_back(temp[start]);
        for (end = start + 1; end < temp.size(); ++end)
        {
            if (monster_pane_info::less_than(temp[start], temp[end],
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

class range_view_annotator
{
public:
    range_view_annotator(int range) {
        do_anything = (range >= 0);
        if (range < 0)
            return;

        // Save and replace grid colours. -1 means unchanged.
        orig_colours.init(-1);
        const coord_def offset(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET);
        for ( radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri )
        {
            if (grid_distance(you.pos(), *ri) > range)
            {
                orig_colours(*ri - you.pos() + offset) = env.grid_colours(*ri);
                env.grid_colours(*ri) = DARKGREY;
            }
        }

        // Save and replace monster colours.
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            if (menv[i].alive()
                && grid_distance(menv[i].pos(), you.pos()) > range
                && you.can_see(&menv[i]))
            {
                orig_mon_colours[i] = menv[i].colour;
                menv[i].colour = DARKGREY;
            }
            else
            {
                orig_mon_colours[i] = -1;
            }
        }

        // Repaint.
        viewwindow(true, false);
    }

    ~range_view_annotator() {

        if (!do_anything)
            return;

        // Restore grid colours.
        coord_def c;
        const coord_def offset(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET);
        for ( c.x = 0; c.x < ENV_SHOW_DIAMETER; ++c.x )
        {
            for ( c.y = 0; c.y < ENV_SHOW_DIAMETER; ++c.y )
            {
                const int old_colour = orig_colours(c);
                if ( old_colour != -1 )
                    env.grid_colours(you.pos() + c - offset) = old_colour;
            }
        }

        // Restore monster colours.
        for (int i = 0; i < MAX_MONSTERS; ++i)
            if (orig_mon_colours[i] != -1)
                menv[i].colour = orig_mon_colours[i];

        // Repaint.
        viewwindow(true, false);
    }
private:
    bool do_anything;
    FixedArray<int, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> orig_colours;
    int orig_mon_colours[MAX_MONSTERS];
};

bool _dist_ok(const dist& moves, int range, targ_mode_type mode,
              bool may_target_self, bool cancel_at_self)
{
    if (!moves.isCancel && moves.isTarget)
    {
        if (!see_grid(moves.target))
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

            if (!may_target_self && mode == TARG_ENEMY)
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


void direction(dist& moves, targeting_type restricts,
               targ_mode_type mode, int range, bool just_looking,
               bool needs_path, bool may_target_monster,
               bool may_target_self, const char *prompt,
               targeting_behaviour *beh, bool cancel_at_self)
{
    if (!beh)
    {
        static targeting_behaviour stock_behaviour;
        beh = &stock_behaviour;
    }
    beh->just_looking = just_looking;

#ifndef USE_TILE
    if (may_target_monster && restricts != DIR_DIR
        && Options.mlist_targetting == MLIST_TARGET_HIDDEN)
    {
        Options.mlist_targetting = MLIST_TARGET_ON;

        const int full_info = update_monster_pane();
        if (full_info == -1)
        {
            // If there are no monsters after all, turn the the targetting
            // off again.
            Options.mlist_targetting = MLIST_TARGET_HIDDEN;
        }
        else
            _fill_monster_list(full_info);
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
        direction_choose_compass( moves, beh );
        return;
    }

    cursor_control con(!Options.use_fake_cursor);
    mouse_control mc(MOUSE_MODE_TARGET);
    range_view_annotator rva(range);

    int dir = 0;
    bool show_beam = Options.show_beam && !just_looking && needs_path;
    ray_def ray;

    coord_def objfind_pos, monsfind_pos;

    // init
    moves.delta.reset();
    moves.target = you.pos();

    // If we show the beam on startup, we have to initialise it.
    if (show_beam)
        find_ray(you.pos(), moves.target, true, ray);

    bool skip_iter = false;
    bool found_autotarget = false;
    bool target_unshifted = Options.target_unshifted_dirs;

    // Find a default target.
    if (Options.default_target && mode == TARG_ENEMY)
    {
        skip_iter = true;   // Skip first iteration...XXX mega-hack
        if (you.prev_targ != MHITNOT && you.prev_targ != MHITYOU)
        {
            const monsters *montarget = &menv[you.prev_targ];
            if (mons_near(montarget) && player_monster_visible(montarget)
                && !mons_friendly(montarget) // not made friendly since then
                && _is_target_in_range(montarget->pos(), range))
            {
                found_autotarget = true;
                moves.target = montarget->pos();
            }
        }
    }

    bool show_prompt = true;

    while (true)
    {
        bool allow_out_of_range = false;

        // Prompts might get scrolled off if you have too few lines available.
        // We'll live with that.
        if ( !just_looking && (show_prompt || beh->should_redraw()) )
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

        cursorxy( grid2viewX(moves.target.x), grid2viewY(moves.target.y) );

        command_type key_command;

        if (skip_iter)
        {
            if (found_autotarget)
                key_command = CMD_NO_CMD;
            else
                key_command = CMD_TARGET_CYCLE_FORWARD; // Find closest target.
        }
        else
        {
            key_command = beh->get_command();
        }

#ifdef USE_TILE
        // If a mouse command, update location to mouse position.
        if (key_command == CMD_TARGET_MOUSE_MOVE
            || key_command == CMD_TARGET_MOUSE_SELECT)
        {
            const coord_def &gc = tiles.get_cursor();
            if (gc != Region::NO_CURSOR)
            {
                moves.target = gc;

                if (key_command == CMD_TARGET_MOUSE_SELECT)
                {
                    key_command = CMD_TARGET_SELECT;

                    if (needs_path && range > 0)
                    {
                        ray_def raycopy = ray;
                        int l = 0;
                        while (raycopy.pos() != moves.target && l < range)
                        {
                            l++;
                            raycopy.advance_through(moves.target);
                        }
                        moves.target = raycopy.pos();
                    }
                }
            }
            else
            {
                key_command = CMD_NO_CMD;
            }
        }
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
        bool force_redraw = false;
        bool loop_done = false;

        coord_def old_target = moves.target;
        if (skip_iter)
            old_target.x += 500; // hmmm...hack

        int i, mid;

#ifndef USE_TILE
        if (key_command >= CMD_TARGET_CYCLE_MLIST
            && key_command <= CMD_TARGET_CYCLE_MLIST_END)
        {
            const int idx = _mlist_letter_to_index(key_command + 'a'
                                                   - CMD_TARGET_CYCLE_MLIST);

            if (_find_square_wrapper(moves.target, monsfind_pos, 1,
                                     _find_mlist, needs_path, idx, range,
                                     Options.target_wrap))
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
            i = _targeting_cmd_to_compass(key_command);
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
            i = _targeting_cmd_to_compass(key_command);

            if (restricts != DIR_TARGET)
            {
                // A direction is allowed, and we've selected it.
                moves.delta    = Compass[i];
                // Needed for now...eventually shouldn't be necessary
                moves.target   = you.pos() + moves.delta;
                moves.isValid  = true;
                moves.isTarget = false;
                show_beam      = false;
                moves.choseRay = false;
                loop_done      = true;
            }
            else
            {
                // Direction not allowed, so just move in that direction.
                // Maybe make this a bigger jump?
                if (restricts == DIR_TARGET)
                    moves.target += Compass[i] * 3;
                else
                    moves.target += Compass[i];
            }
            break;

        case CMD_TARGET_SHOW_PROMPT:
            mprf(MSGCH_PROMPT, "%s (%s)", prompt? prompt : "Aim",
                 target_mode_help_text(restricts));
            break;

#ifndef USE_TILE
        case CMD_TARGET_TOGGLE_MLIST:
        {
            if (Options.mlist_targetting == MLIST_TARGET_ON)
                Options.mlist_targetting = MLIST_TARGET_OFF;
            else
                Options.mlist_targetting = MLIST_TARGET_ON;

            const int full_info = update_monster_pane();
            if (Options.mlist_targetting == MLIST_TARGET_ON)
            {
                if (full_info == -1)
                    Options.mlist_targetting = MLIST_TARGET_HIDDEN;
                else
                    _fill_monster_list(full_info);
            }
            break;
        }
#endif

#ifdef WIZARD
        case CMD_TARGET_CYCLE_BEAM:
            show_beam = find_ray(you.pos(), moves.target,
                                 true, ray, (show_beam ? 1 : 0));
            need_beam_redraw = true;
            break;
#endif

        case CMD_TARGET_HIDE_BEAM:
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

                show_beam = find_ray(you.pos(), moves.target,
                                     true, ray, 0, true);
                need_beam_redraw = show_beam;
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
            const int thing_to_find = _targeting_cmd_to_feature(key_command);
            if (_find_square_wrapper(moves.target, objfind_pos, 1,
                                     _find_feature, needs_path, thing_to_find,
                                     range, true, Options.target_los_first ?
                                        LOS_FLIPVH : LOS_ANY))
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
            mprf( "Targeting mode is now: %s",
                  (mode == TARG_ANY)   ? "any" :
                  (mode == TARG_ENEMY) ? "enemies"
                                       : "friends" );
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

                if (!mons_near(montarget)
                    || !player_monster_visible( montarget ))
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
                        // We have to turn off show_beam, because
                        // when jumping to a previous target we don't
                        // care about the beam; otherwise Bad Things
                        // will happen because the ray is invalid,
                        // and we don't get a chance to update it before
                        // breaking from the loop.
                        show_beam = false;
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
        case CMD_TARGET_SELECT: // finalize current choice
            if (!moves.isEndpoint
                && mgrd(moves.target) != NON_MONSTER
                && _mon_submerged_in_water(&menv[mgrd(moves.target)]))
            {
                moves.isEndpoint = true;
            }
            moves.isValid  = true;
            moves.isTarget = true;
            loop_done      = true;

            you.prev_grd_targ.reset();

            // Maybe we should except just_looking here?
            mid = mgrd(moves.target);

            if (mid != NON_MONSTER)
                you.prev_targ = mid;
            else if (moves.target == you.pos())
                you.prev_targ = MHITYOU;
            else
                you.prev_grd_targ = moves.target;

#ifndef USE_TILE
            if (Options.mlist_targetting == MLIST_TARGET_ON)
                Options.mlist_targetting = MLIST_TARGET_HIDDEN;
#endif
            break;

        case CMD_TARGET_OBJ_CYCLE_BACK:
        case CMD_TARGET_OBJ_CYCLE_FORWARD:
            dir = (key_command == CMD_TARGET_OBJ_CYCLE_BACK) ? -1 : 1;
            if (_find_square_wrapper( moves.target, objfind_pos, dir,
                                      _find_object, needs_path, TARG_ANY, range,
                                      true, Options.target_los_first ?
                                          (dir == 1? LOS_FLIPVH : LOS_FLIPHV)
                                           : LOS_ANY))
            {
                moves.target = objfind_pos;
            }
            else if (!skip_iter)
                flush_input_buffer(FLUSH_ON_FAILURE);

            break;

        case CMD_TARGET_CYCLE_FORWARD:
        case CMD_TARGET_CYCLE_BACK:
            dir = (key_command == CMD_TARGET_CYCLE_BACK) ? -1 : 1;
            if (_find_square_wrapper( moves.target, monsfind_pos, dir,
                                      _find_monster, needs_path, mode, range,
                                      Options.target_wrap))
            {
                moves.target = monsfind_pos;
            }
            else if (!skip_iter)
                flush_input_buffer(FLUSH_ON_FAILURE);
            break;

        case CMD_TARGET_CANCEL:
            loop_done = true;
            moves.isCancel = true;
            beh->mark_ammo_nonchosen();

#ifndef USE_TILE
            if (Options.mlist_targetting == MLIST_TARGET_ON)
                Options.mlist_targetting = MLIST_TARGET_HIDDEN;
#endif
            break;

#ifdef WIZARD
        case CMD_TARGET_WIZARD_MAKE_FRIENDLY:
            // Maybe we can skip this check...but it can't hurt
            if (!you.wizard || !in_bounds(moves.target))
                break;

            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            {
                monsters &m = menv[mid];
                mon_attitude_type att = m.attitude;

                // During arena mode, skip directly from friendly to hostile.
                if (crawl_state.arena_suspended && att == ATT_FRIENDLY)
                    att = ATT_NEUTRAL;

                switch (att)
                {
                 case ATT_FRIENDLY:
                     m.attitude = ATT_GOOD_NEUTRAL;
                     m.flags &= ~MF_CREATED_FRIENDLY;
                     m.flags |= MF_WAS_NEUTRAL;
                     break;
                 case ATT_GOOD_NEUTRAL:
                     m.attitude = ATT_NEUTRAL;
                     break;
                 case ATT_NEUTRAL:
                     m.attitude = ATT_HOSTILE;
                     m.flags &= ~MF_WAS_NEUTRAL;
                     break;
                 case ATT_HOSTILE:
                     m.attitude = ATT_FRIENDLY;
                     m.flags |= MF_CREATED_FRIENDLY;
                     break;
                 default:
                     break;
                }

                // To update visual branding of friendlies.  Only
                // seems capabable of adding bolding, not removing it,
                // though.
                viewwindow(true, false);
            }
            break;

        case CMD_TARGET_WIZARD_BLESS_MONSTER:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            wizard_apply_monster_blessing(&menv[mid]);
            break;

        case CMD_TARGET_WIZARD_MAKE_SHOUT:
            // Maybe we can skip this check...but it can't hurt
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            debug_make_monster_shout(&menv[mid]);
            break;

        case CMD_TARGET_WIZARD_GIVE_ITEM:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            wizard_give_monster_item(&menv[mid]);
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
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER)
                break;

            debug_pathfind(mid);
            break;

        case CMD_TARGET_WIZARD_GAIN_LEVEL:
            break;

        case CMD_TARGET_WIZARD_MISCAST:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER && you.pos() != moves.target)
                break;

            debug_miscast(mid);
            break;

        case CMD_TARGET_WIZARD_MAKE_SUMMONED:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            wizard_make_monster_summoned(&menv[mid]);
            break;

        case CMD_TARGET_WIZARD_POLYMORPH:
            if (!you.wizard || !in_bounds(moves.target))
                break;
            mid = mgrd(moves.target);
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            wizard_polymorph_monster(&menv[mid]);
            break;

#endif
        case CMD_TARGET_DESCRIBE:
            full_describe_square(moves.target);
            force_redraw = true;
            if (crawl_state.arena_suspended)
                need_beam_redraw = true;
            break;

        case CMD_TARGET_ALL_DESCRIBE:
            full_describe_view();
            break;

        case CMD_TARGET_HELP:
            show_targeting_help();
            force_redraw = true;
            redraw_screen();
            mesclr(true);
            show_prompt = true;
            break;

        default:
            break;
        }

        if (loop_done == true)
        {
            // Confirm that the loop is really done. If it is,
            // break out. If not, just continue looping.

            if (just_looking) // easy out
                break;

            if (_dist_ok(moves, allow_out_of_range ? -1 : range,
                         mode, may_target_self, cancel_at_self))
            {
                // Finalize whatever is inside the loop
                // (moves-internal finalizations can be done later).
                moves.choseRay = show_beam;
                moves.ray = ray;
                break;
            }
            else
            {
                show_prompt = true;
            }
        }

        // We'll go on looping. Redraw whatever is necessary.
        if ( !in_viewport_bounds(grid2view(moves.target)) )
        {
            // Tried to step out of bounds
            moves.target = old_target;
        }

        bool have_moved = false;

        if (old_target != moves.target)
        {
            have_moved = true;
            show_beam = show_beam && find_ray(you.pos(), moves.target,
                                              true, ray, 0, true);
        }

        if (force_redraw)
            have_moved = true;

        if (have_moved)
        {
            // If the target x,y has changed, the beam must have changed.
            if (show_beam)
                need_beam_redraw = true;

            if (!skip_iter)     // Don't clear before we get a chance to see.
                mesclr(true);   // Maybe not completely necessary.

            bool in_range = (range < 0
                             || grid_distance(moves.target,you.pos()) <= range);
            terse_describe_square(moves.target, in_range);
        }

#ifdef USE_TILE
        // Tiles always need a beam redraw if show_beam is true (and valid...)
        if (need_beam_redraw
            || show_beam && find_ray(you.pos(), moves.target,
                                     true, ray, 0, true) )
        {
#else
        if (need_beam_redraw)
        {
            viewwindow(true, false);
#endif
            if (show_beam
                && in_vlos(grid2view(moves.target))
                && moves.target != you.pos() )
            {
                // Draw the new ray with magenta '*'s, not including
                // your square or the target square.
                // Out-of-range cells get grey '*'s instead.
                ray_def raycopy = ray; // temporary copy to work with
                while (raycopy.pos() != moves.target)
                {
                    if (raycopy.pos() != you.pos())
                    {
                        // Sanity: don't loop forever if the ray is problematic
                        if (!in_los(raycopy.pos()))
                            break;

                        const bool in_range = (range < 0)
                            || grid_distance(raycopy.pos(), you.pos()) <= range;
                        const int bcol = in_range ? MAGENTA : DARKGREY;

                        draw_ray_glyph(raycopy.pos(), bcol, '*',
                                       bcol | COLFLAG_REVERSE, in_range);
                    }
                    raycopy.advance_through(moves.target);
                }
                textcolor(LIGHTGREY);
#ifdef USE_TILE
                const bool in_range = (range < 0)
                    || grid_distance(raycopy.pos(), you.pos()) <= range;
                draw_ray_glyph(moves.target, MAGENTA, '*',
                               MAGENTA | COLFLAG_REVERSE, in_range);
            }
            viewwindow(true, false);
#else
            }
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
    std::string desc;
    const char *unseen_desc = "[unseen terrain]";
    if (gc == you.pos())
        desc = you.your_name;
    else if (!map_bounds(gc))
        desc = unseen_desc;
    else if (!see_grid(gc))
    {
        if (is_terrain_seen(gc))
        {
            desc = "[" + feature_description(gc, false, DESC_PLAIN, false)
                       + "]";
        }
        else
            desc = unseen_desc;
    }
    else if (mgrd(gc) != NON_MONSTER && you.can_see(&menv[mgrd(gc)]))
    {
        const monsters &mons = menv[mgrd(gc)];

        if (mons_is_mimic(mons.type) && !(mons.flags & MF_KNOWN_MIMIC))
        {
            item_def item;
            get_mimic_item(&mons, item);
            desc = item.name(DESC_PLAIN);
        }
        else
            desc = mons.full_name(DESC_PLAIN, true);
    }
    else if (igrd(gc) != NON_ITEM)
        desc = mitm[igrd(gc)].name(DESC_PLAIN);
    else
        desc = feature_description(gc, false, DESC_PLAIN, false);

    return desc;
}

void terse_describe_square(const coord_def &c, bool in_range)
{
    if (!see_grid(c))
        _describe_oos_square(c);
    else if (in_bounds(c) )
        _describe_cell(c, in_range);
}

void full_describe_square(const coord_def &c)
{
    // Don't give out information for things outside LOS
    if (!see_grid(c.x, c.y))
        return;

    const int mid = mgrd(c);
    const int oid = igrd(c);

    if (mid != NON_MONSTER && player_monster_visible(&menv[mid]))
    {
        // First priority: monsters.
        describe_monsters(menv[mid]);
    }
    else if (oid != NON_ITEM)
    {
        // Second priority: objects.
        describe_item( mitm[oid] );
    }
    else
    {
        // Third priority: features.
        describe_feature_wide(c.x, c.y);
    }

    redraw_screen();
    mesclr(true);
}

static void _extend_move_to_edge(dist &moves)
{
    if ( moves.delta.origin() )
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

static bool _mons_is_valid_target(monsters *mon, int mode, int range)
{
    // Unknown mimics don't count as monsters, either.
    if (mons_is_mimic(mon->type)
        && !(mon->flags & MF_KNOWN_MIMIC))
    {
        return (false);
    }

    // Don't usually target unseen monsters...
    if (!player_monster_visible(mon))
    {
        // ... unless it creates a "disturbance in the water".
        // Since you can't see the monster, assume it's not a friend.
        // Also don't target submerged monsters if there are other targets
        // in sight. (This might be too restrictive.)
        return (mode != TARG_FRIEND
                && _mon_submerged_in_water(mon)
                && i_feel_safe(false, false, true, range));
    }

    return (true);
}

#ifndef USE_TILE
static bool _find_mlist( const coord_def& where, int idx, bool need_path,
                         int range = -1)
{
    if ((int) mlist.size() <= idx)
        return (false);

    if (!_is_target_in_range(where, range) || !in_los(where))
        return (false);

    const int targ_mon = mgrd(where);
    if (targ_mon == NON_MONSTER)
        return (false);

    int real_idx = 0;
    for (unsigned int i = 0; i < mlist.size()-1; i++)
    {
        if (real_idx == idx)
        {
            real_idx = i;
            break;
        }

        // While the monsters are identical, don't increase real_idx.
        if (!monster_pane_info::less_than(mlist[i], mlist[i+1], mlist_full_info))
            continue;

        real_idx++;
   }

    monsters *mon  = &menv[targ_mon];

    if (!_mons_is_valid_target(mon, TARG_ANY, range))
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
    // Target the player for friendly and general spells.
    if ((mode == TARG_FRIEND || mode == TARG_ANY) && where == you.pos())
        return (true);

    // Don't target out of range.
    if (!_is_target_in_range(where, range))
        return (false);

    const int targ_mon = mgrd(where);

    // No monster or outside LOS.
    if (targ_mon == NON_MONSTER || !in_los(where))
        return (false);

    // Monster in LOS but only via glass walls, so no direct path.
    if (need_path && !see_grid_no_trans(where))
        return (false);

    monsters *mon = &menv[targ_mon];

    if (!_mons_is_valid_target(mon, mode, range))
        return (false);

    // Now compare target modes.
    if (mode == TARG_ANY)
        return (true);

    if (mode == TARG_FRIEND)
        return (mons_friendly(&menv[targ_mon] ));

    ASSERT(mode == TARG_ENEMY);
    if (mons_friendly(&menv[targ_mon]))
        return (false);

    // Don't target zero xp monsters, unless target_zero_exp is set.
    return (Options.target_zero_exp
            || !mons_class_flag( menv[targ_mon].type, M_NO_EXP_GAIN ));
}

static bool _find_feature( const coord_def& where, int mode,
                           bool /* need_path */, int /* range */)
{
    // The stair need not be in LOS if the square is mapped.
    if (!in_los(where) && (!Options.target_oos || !is_terrain_seen(where)))
        return (false);

    return is_feature(mode, where);
}

static bool _find_object(const coord_def& where, int mode,
                         bool /* need_path */, int /* range */)
{
    // First, check for mimics.
    bool is_mimic = false;
    const int mons = mgrd(where);
    if (mons != NON_MONSTER
        && player_monster_visible( &(menv[mons]) )
        && mons_is_mimic(menv[mons].type)
        && !(menv[mons].flags & MF_KNOWN_MIMIC))
    {
        is_mimic = true;
    }

    const int item = igrd(where);
    if (item == NON_ITEM && !is_mimic)
        return (false);

    return (in_los(where) || Options.target_oos && is_terrain_seen(where)
            && (is_stash(where.x,where.y) || is_mimic));
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
// If the game option target_zero_exp is true, zero experience
// monsters will be targeted.
//
//---------------------------------------------------------------
static char _find_square( const coord_def& where,
                          coord_def &mfp, int direction,
                          bool (*find_targ)( const coord_def& wh, int mode,
                                             bool need_path, int range ),
                          bool need_path, int mode, int range, bool wrap,
                          int los )
{
    // the day will come when [unsigned] chars will be consigned to
    // the fires of Gehenna. Not quite yet, though.

    int temp_xps = where.x;
    int temp_yps = where.y;
    int x_change = 0;
    int y_change = 0;

    bool onlyVis = false, onlyHidden = false;

    int i, j;

    if (los == LOS_NONE)
        return (0);

    if (los == LOS_FLIPVH || los == LOS_FLIPHV)
    {
        if (in_los_bounds(where))
        {
            // We've been told to flip between visible/hidden, so we
            // need to find what we're currently on.
            const bool vis = (env.show(view2show(where))
                              || view2grid(where) == you.pos());

            if (wrap && (vis != (los == LOS_FLIPVH)) == (direction == 1))
            {
                // We've already flipped over into the other direction,
                // so correct the flip direction if we're wrapping.
                los = los == LOS_FLIPHV? LOS_FLIPVH : LOS_FLIPHV;
            }

            los = (los & ~LOS_VISMASK) | (vis? LOS_VISIBLE : LOS_HIDDEN);
        }
        else
        {
            if (wrap)
                los = LOS_HIDDEN | (direction == 1? LOS_FLIPHV : LOS_FLIPVH);
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
            if (find_targ(you.pos(), mode, need_path, range))
            {
                mfp = vyou;
                return (1);
            }
            return _find_square(vyou, mfp, direction,
                                find_targ, need_path, mode, range, false,
                                _next_los(direction, los, wrap));
        }
        if (direction == -1 && temp_xps == ctrx && temp_yps == ctry)
        {
            return _find_square(coord_def(minx, maxy), mfp, direction,
                                find_targ, need_path, mode, range, false,
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
            for (i = -1; i < 2; i++)
            {
                for (j = -1; j < 2; j++)
                {
                    if (i == 0 && j == 0)
                        continue;

                    if (temp_xps + i == minx - 1)
                    {
                        x_change = 0;
                        y_change = -1;
                    }
                    else if (temp_xps + i - ctrx == 0 && temp_yps + j - ctry == 0)
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

        // We don't want to be looking outside the bounds of the arrays:
        //if (!in_los_bounds(temp_xps, temp_yps))
        //    continue;

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

    return (direction == 1?
      _find_square(coord_def(ctrx, ctry), mfp, direction, find_targ, need_path,
                   mode, range, false, _next_los(direction, los, wrap))
      : _find_square(coord_def(minx, maxy), mfp, direction, find_targ,
                     need_path, mode, range, false,
                     _next_los(direction, los, wrap)));
}

// XXX Unbelievably hacky. And to think that my goal was to clean up the code.
// Identical to find_square, except that input (tx, ty) and output
// (mfp) are in grid coordinates rather than view coordinates.
static char _find_square_wrapper( const coord_def& targ,
                                  coord_def& mfp, char direction,
                                  bool (*find_targ)(const coord_def& where, int mode,
                                                    bool need_path, int range),
                                  bool need_path, int mode, int range,
                                  bool wrap, int los )
{
    const char r =  _find_square(grid2view(targ), mfp,
                                 direction, find_targ, need_path, mode, range,
                                 wrap, los);
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
    if (grid_is_water(grid) && player_likes_water())
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

    return thing_do_grammar(dtype, add_stop, grid_is_trap(grid), desc);
}

static std::string _base_feature_desc(dungeon_feature_type grid,
                                      trap_type trap)
{
    if (grid_is_trap(grid) && trap != NUM_TRAPS)
    {
        switch (trap)
        {
        case TRAP_DART:
            return ("dart trap");
        case TRAP_ARROW:
            return ("arrow trap");
        case TRAP_SPEAR:
            return ("spear trap");
        case TRAP_AXE:
            return ("axe trap");
        case TRAP_TELEPORT:
            return ("teleportation trap");
        case TRAP_ALARM:
            return ("alarm trap");
        case TRAP_BLADE:
            return ("blade trap");
        case TRAP_BOLT:
            return ("bolt trap");
        case TRAP_NET:
            return ("net trap");
        case TRAP_ZOT:
            return ("Zot trap");
        case TRAP_NEEDLE:
            return ("needle trap");
        case TRAP_SHAFT:
            return ("shaft");
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
    case DNGN_CLOSED_DOOR:
        return ("closed door");
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
    case DNGN_ALTAR_ZIN:
        return ("glowing white marble altar of Zin");
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
        return ("silver altar of Elyvilon");
    case DNGN_ALTAR_LUGONU:
        return ("corrupted altar of Lugonu");
    case DNGN_ALTAR_BEOGH:
        return ("roughly hewn altar of Beogh");
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
        switch(you.where_are_you)
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
        props[SHORT_DESC_KEY].new_table(SV_STR);

    CrawlHashTable &desc_table = props[SHORT_DESC_KEY];

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

static std::string _marker_feature_description(const coord_def &p)
{
    std::vector<map_marker*> markers = env.markers.get_markers_at(p);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        const std::string desc = markers[i]->feature_description();
        if (!desc.empty())
            return (desc);
    }
    return ("");
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
    dungeon_feature_type grid = grd(where);
    if (grid == DNGN_SECRET_DOOR)
        grid = grid_secret_door_appearance(where);

    if (grid == DNGN_OPEN_DOOR || grid == DNGN_CLOSED_DOOR)
    {
        std::set<coord_def> all_door;
        find_connected_identical(where, grd(where), all_door);
        const char *adj, *noun;
        get_door_description(all_door.size(), &adj, &noun);

        std::string desc = adj;
        desc += (grid == DNGN_OPEN_DOOR) ? "open " : "closed ";
        desc += noun;

        if (bloody)
            desc += ", spattered with blood";

        return thing_do_grammar(dtype, add_stop, false, desc);
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
        return (thing_do_grammar(
                    dtype, add_stop, false,
                    _marker_feature_description(where)));
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
                      || ench.ench == ENCH_PETRIFIED
                      || ench.ench == ENCH_PETRIFYING))
    {
        return "";
    }

    if ((ench.ench == ENCH_HASTE || ench.ench == ENCH_BATTLE_FRENZY)
        && mons.has_ench(ENCH_BERSERK))
    {
        return "";
    }

    if (ench.ench == ENCH_PETRIFIED && mons.has_ench(ENCH_PETRIFYING))
        return "";

    switch (ench.ench)
    {
    case ENCH_POISON:
        return "poisoned";
    case ENCH_SICK:
        return "sick";
    case ENCH_ROT:
        return "rotting away"; //jmf: "covered in sores"?
    case ENCH_BACKLIGHT:
        return "softly glowing";
    case ENCH_SLOW:
        return "moving slowly";
    case ENCH_BERSERK:
        return "berserk";
    case ENCH_BATTLE_FRENZY:
        return "consumed by blood-lust";
    case ENCH_HASTE:
        return "moving very quickly";
    case ENCH_CONFUSION:
        return "bewildered and confused";
    case ENCH_INVIS:
        return "slightly transparent";
    case ENCH_CHARM:
        return "in your thrall";
    case ENCH_STICKY_FLAME:
        return "covered in liquid flames";
    case ENCH_HELD:
        return "entangled in a net";
    case ENCH_PETRIFIED:
        return "petrified";
    case ENCH_PETRIFYING:
        return "slowly petrifying";
    case ENCH_LOWERED_MR:
        return "susceptible to magic";
    default:
        return "";
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
        else if (is_stair(grd(pos)))
            return (" (unknown stair)");
    }
    return ("");
}
#endif

std::string _mon_enchantments_string(const monsters* mon)
{
    const bool paralysed = mons_is_paralysed(mon);
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

static void _describe_monster(const monsters *mon)
{
    // First print type and equipment.
    const int numcols = get_number_of_cols();
    std::string text = get_monster_desc(mon) + ".";
    print_formatted_paragraph(text, numcols);

    if (player_mesmerised_by(mon))
        mpr("You are mesmerised by her song.", MSGCH_EXAMINE);

    print_wounds(mon);

    if (!mons_is_mimic(mon->type) && mons_behaviour_perceptible(mon))
    {
        if (mons_is_sleeping(mon))
        {
            mprf(MSGCH_EXAMINE, "%s appears to be %s.",
                 mon->pronoun(PRONOUN_CAP).c_str(),
                 mons_is_confused(mon) ? "sleepwalking" : "resting");
        }
        // Applies to both friendlies and hostiles
        else if (mons_is_fleeing(mon))
        {
            mprf(MSGCH_EXAMINE, "%s is retreating.",
                 mon->pronoun(PRONOUN_CAP).c_str());
        }
        // hostile with target != you
        else if (!mons_friendly(mon) && !mons_neutral(mon)
                 && mon->foe != MHITYOU && !crawl_state.arena_suspended)
        {
            // special case: batty monsters get set to BEH_WANDER as
            // part of their special behaviour.
            if (!mons_is_batty(mon))
            {
                mprf(MSGCH_EXAMINE, "%s doesn't appear to have noticed you.",
                     mon->pronoun(PRONOUN_CAP).c_str());
            }
        }
    }

    if (mon->attitude == ATT_FRIENDLY)
    {
        mprf(MSGCH_EXAMINE, "%s is friendly.",
             mon->pronoun(PRONOUN_CAP).c_str());
    }
    else if (mons_neutral(mon)) // don't differentiate between permanent or not
    {
        mprf(MSGCH_EXAMINE, "%s is indifferent to you.",
             mon->pronoun(PRONOUN_CAP).c_str());
    }

    if (mon->haloed())
    {
        mprf(MSGCH_EXAMINE, "%s is illuminated by a divine halo.",
             mon->pronoun(PRONOUN_CAP).c_str());
    }

    // Give an indication of monsters being capable of seeing/sensing
    // invisible creatures.
    if (mons_behaviour_perceptible(mon) && !mons_is_sleeping(mon)
        && !mons_is_confused(mon)
        && (mons_see_invis(mon) || mons_sense_invis(mon)))
    {
        const actor* foe = mon->get_foe();
        if (foe && foe->invisible() && !mons_is_fleeing(mon))
        {
            if (!you.can_see(foe))
            {
                mprf(MSGCH_EXAMINE, "%s is looking at something unseen.",
                     mon->pronoun(PRONOUN_CAP).c_str());
            }
            else if (mons_see_invis(mon))
            {
                mprf(MSGCH_EXAMINE, "%s is watching %s carefully.",
                     mon->pronoun(PRONOUN_CAP).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());
            }
            else
            {
                std::string name = foe->atype() == ACT_PLAYER
                                 ? "your" : (foe->name(DESC_NOCAP_THE) + "'s");
                mprf(MSGCH_EXAMINE, "%s is looking in %s general direction.",
                     mon->pronoun(PRONOUN_CAP).c_str(),
                     name.c_str());
            }
        }
        else if (!foe || mons_is_fleeing(mon))
        {
            mprf(MSGCH_EXAMINE, "%s seems to be peering into the shadows.",
                 mon->pronoun(PRONOUN_CAP).c_str());
        }
    }

    if (mons_enslaved_body_and_soul(mon))
    {
        mprf(MSGCH_EXAMINE, "%s soul is ripe for the taking.",
             mon->pronoun(PRONOUN_CAP_POSSESSIVE).c_str());
    }
    else if (mons_enslaved_soul(mon))
    {
        mprf(MSGCH_EXAMINE, "%s is a disembodied soul.",
             mon->pronoun(PRONOUN_CAP).c_str());
    }

    text = _mon_enchantments_string(mon);
    if (!text.empty())
        print_formatted_paragraph(text, numcols);
}

// This method is called in two cases:
// a) Monsters coming into view: "An ogre comes into view. It is wielding ..."
// b) Monster description via 'x': "An ogre, wielding a club and wearing ..."
std::string get_monster_desc(const monsters *mon, bool full_desc,
                             description_level_type mondtype,
                             bool print_attitude)
{
    std::string desc = "";
    if (mondtype != DESC_NONE)
    {
        desc = mon->full_name(mondtype);

        if (print_attitude)
        {
            if (mons_friendly(mon))
                desc += " (friendly)";
            else if (mons_neutral(mon))
                desc += " (neutral)";
        }
    }

    std::string weap = "";

    // We don't report rakshasa equipment in order not to give away the
    // true rakshasa when it summons.

    if (mon->type != MONS_DANCING_WEAPON
        && (mon->type != MONS_RAKSHASA || mons_friendly(mon)))
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
    if (full_desc && (mon->type != MONS_RAKSHASA || mons_friendly(mon)))
    {
        const int mon_arm = mon->inv[MSLOT_ARMOUR];
        const int mon_shd = mon->inv[MSLOT_SHIELD];
        const int mon_qvr = mon->inv[MSLOT_MISSILE];
        const int mon_alt = mon->inv[MSLOT_ALT_WEAPON];

        const bool need_quiver  = (mon_qvr != NON_ITEM && mons_friendly(mon));
        const bool need_alt_wpn = (mon_alt != NON_ITEM && mons_friendly(mon)
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
        if (mons_friendly(mon))
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

static void _describe_cell(const coord_def& where, bool in_range)
{
    bool mimic_item = false;
    bool monster_described = false;
    bool cloud_described = false;
    bool item_described = false;

    if (where == you.pos() && !crawl_state.arena_suspended)
        mpr("You.", MSGCH_EXAMINE_FILTER);

    if (mgrd(where) != NON_MONSTER)
    {
        const monsters* mon = &menv[mgrd(where)];

        if (_mon_submerged_in_water(mon))
        {
            mpr("There is a strange disturbance in the water here.",
                MSGCH_EXAMINE_FILTER);
        }

#if DEBUG_DIAGNOSTICS
        if (!player_monster_visible(mon))
            mpr( "There is a non-visible monster here.", MSGCH_DIAGNOSTICS );
#else
        if (!player_monster_visible(mon))
            goto look_clouds;
#endif

        _describe_monster(mon);

        if (mons_is_mimic(mon->type))
        {
            mimic_item = true;
            item_described = true;
        }
        else
        {
            if ( !in_range )
            {
                mprf(MSGCH_EXAMINE_FILTER, "%s is out of range.",
                     mon->pronoun(PRONOUN_CAP).c_str());
            }
            monster_described = true;
        }

#if DEBUG_DIAGNOSTICS
        debug_stethoscope(mgrd(where));
#endif
        if (Options.tutorial_left && tutorial_monster_interesting(mon))
        {
            std::string msg;
#ifdef USE_TILE
            msg = "(<w>Right-click</w> for more information.)";
#else
            msg = "(Press <w>v</w> for more information.)";
#endif
            print_formatted_paragraph(msg, get_number_of_cols());
        }
    }

#if (!DEBUG_DIAGNOSTICS)
  // removing warning
  look_clouds:
#endif

    if (is_sanctuary(where))
        mpr("This square lies inside a sanctuary.");

    if (env.cgrid(where) != EMPTY_CLOUD)
    {
        const int cloud_inspected = env.cgrid(where);
        const cloud_type ctype = (cloud_type) env.cloud[cloud_inspected].type;

        mprf(MSGCH_EXAMINE, "There is a cloud of %s here.",
             cloud_name(ctype).c_str());

        cloud_described = true;
    }

    int targ_item = igrd(where);

    if (targ_item != NON_ITEM)
    {
        // If a mimic is on this square, we pretend it's the first item -- bwr
        if (mimic_item)
            mpr("There is something else lying underneath.", MSGCH_FLOOR_ITEMS);
        else
        {
            if (mitm[ targ_item ].base_type == OBJ_GOLD)
                mprf( MSGCH_FLOOR_ITEMS, "A pile of gold coins." );
            else
            {
                mprf( MSGCH_FLOOR_ITEMS, "You see %s here.",
                      mitm[targ_item].name(DESC_NOCAP_A).c_str());
            }

            if (mitm[ targ_item ].link != NON_ITEM)
            {
                mprf( MSGCH_FLOOR_ITEMS,
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
    const dungeon_feature_type feat = grd(where);
    mprf(MSGCH_DIAGNOSTICS, "(%d,%d): %s - %s (%d/%s)%s%s", where.x, where.y,
         stringize_glyph(get_screen_glyph(where)).c_str(),
         feature_desc.c_str(),
         feat,
         dungeon_feature_name(feat),
         marker.c_str(),
         traveldest.c_str());
#else
    if (Options.tutorial_left && tutorial_pos_interesting(where.x, where.y))
    {
#ifdef USE_TILE
        feature_desc += " (<w>Right-click</w> for more information.)";
#else
        feature_desc += " (Press <w>v</w> for more information.)";
#endif
        print_formatted_paragraph(feature_desc, get_number_of_cols());
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
            || grid_is_water(feat))
        {
            channel = MSGCH_EXAMINE_FILTER;
        }

        mpr(feature_desc.c_str(), channel);
    }
#endif
}

///////////////////////////////////////////////////////////////////////////
// targeting_behaviour

targeting_behaviour::targeting_behaviour(bool look_around)
    : just_looking(look_around), compass(false)
{
}

targeting_behaviour::~targeting_behaviour()
{
}

int targeting_behaviour::get_key()
{
    if (!crawl_state.is_replaying_keys())
        flush_input_buffer(FLUSH_BEFORE_COMMAND);

    return unmangle_direction_keys( getchm(KC_TARGETING), KC_TARGETING,
                                    false, false);
}

command_type targeting_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    command_type cmd = key_to_command(key, KC_TARGETING);
    if (cmd >= CMD_MIN_TARGET && cmd < CMD_TARGET_CYCLE_TARGET_MODE)
        return (cmd);

#ifndef USE_TILE
    // Overrides the movement keys while mlist_targetting is active.
    if (Options.mlist_targetting == MLIST_TARGET_ON && islower(key))
        return static_cast<command_type> (CMD_TARGET_CYCLE_MLIST + (key - 'a'));
#endif

    // XXX: hack
    if (cmd == CMD_TARGET_SELECT && key == ' ' && just_looking)
        cmd = CMD_TARGET_CANCEL;

    return (cmd);
}

bool targeting_behaviour::should_redraw()
{
    return (false);
}

void targeting_behaviour::mark_ammo_nonchosen()
{
    // Nothing to be done.
}

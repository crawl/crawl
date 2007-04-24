/*
 *  File:       direct.cc
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <5>  01/08/01       GDL   complete rewrite of direction()
 * <4>  11/23/99       LRH   Looking at monsters now
 *                           displays more info
 * <3>  5/12/99        BWR   changes to allow for space selection of target.
 *                           CR, ESC, and 't' in targeting.
 * <2>  5/09/99        JDJ   look_around no longer prints a prompt.
 * <1>  -/--/--        LRH   Created
 */

#include "AppHdr.h"
#include "direct.h"
#include "format.h"

#include <cstdarg>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "command.h"
#include "debug.h"
#include "describe.h"
#include "itemname.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "shopping.h"
#include "stuff.h"
#include "spells4.h"
#include "stash.h"
#include "travel.h"
#include "view.h"

#include "macro.h"

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

static void describe_feature(int mx, int my, bool oos);
static void describe_cell(int mx, int my);

static bool find_object( int x, int y, int mode );
static bool find_monster( int x, int y, int mode );
static bool find_feature( int x, int y, int mode );

static char find_square_wrapper( int tx, int ty, 
                                 FixedVector<char, 2> &mfp, char direction,
                                 bool (*targ)(int, int, int),
                                 int mode = TARG_ANY,
                                 bool wrap = false,
                                 int los = LOS_ANY);

static char find_square( unsigned char xps, unsigned char yps, 
                         FixedVector<char, 2> &mfp, char direction,
                         bool (*targ)(int, int, int),
                         int mode = TARG_ANY,
                         bool wrap = false,
                         int los = LOS_ANY);

static int targeting_cmd_to_compass( command_type command );
static void describe_oos_square(int x, int y);
static void extend_move_to_edge(dist &moves);

static bool is_mapped(int x, int y)
{
    return (is_player_mapped(x, y));
}

static command_type read_direction_key(bool just_looking = false)
{
    flush_input_buffer( FLUSH_BEFORE_COMMAND );
    
    const int key =
        unmangle_direction_keys(
            getchm(KC_TARGETING), KC_TARGETING, false, false);

    switch ( key )
    {
    case ESCAPE: case 'x': return CMD_TARGET_CANCEL;
        
#ifdef WIZARD
    case 'F': return CMD_TARGET_WIZARD_MAKE_FRIENDLY;
#endif
    case 'v': return CMD_TARGET_DESCRIBE;
    case '?': return CMD_TARGET_HELP;
    case ' ': return just_looking? CMD_TARGET_CANCEL : CMD_TARGET_SELECT;
    case CONTROL('C'): return CMD_TARGET_CYCLE_BEAM;
    case ':': return CMD_TARGET_HIDE_BEAM;
    case '\r': return CMD_TARGET_SELECT;
    case '.': return CMD_TARGET_SELECT;
    case '5': return CMD_TARGET_SELECT;
    case '!': return CMD_TARGET_SELECT_ENDPOINT;

    case '\\': case '\t': return CMD_TARGET_FIND_PORTAL;
    case '^': return CMD_TARGET_FIND_TRAP;
    case '_': return CMD_TARGET_FIND_ALTAR;
    case '<': return CMD_TARGET_FIND_UPSTAIR;
    case '>': return CMD_TARGET_FIND_DOWNSTAIR;

    case CONTROL('F'): return CMD_TARGET_CYCLE_TARGET_MODE;
    case 'p': case 'f': case 't': return CMD_TARGET_PREV_TARGET;
        
    case '-': return CMD_TARGET_CYCLE_BACK;
    case '+': case '=':  return CMD_TARGET_CYCLE_FORWARD;
    case ';': case '/':  return CMD_TARGET_OBJ_CYCLE_BACK;
    case '*': case '\'': return CMD_TARGET_OBJ_CYCLE_FORWARD;

    case 'b': return CMD_TARGET_DOWN_LEFT;
    case 'h': return CMD_TARGET_LEFT;
    case 'j': return CMD_TARGET_DOWN;
    case 'k': return CMD_TARGET_UP;
    case 'l': return CMD_TARGET_RIGHT;
    case 'n': return CMD_TARGET_DOWN_RIGHT;
    case 'u': return CMD_TARGET_UP_RIGHT;
    case 'y': return CMD_TARGET_UP_LEFT;

    case 'B': return CMD_TARGET_DIR_DOWN_LEFT;
    case 'H': return CMD_TARGET_DIR_LEFT;
    case 'J': return CMD_TARGET_DIR_DOWN;
    case 'K': return CMD_TARGET_DIR_UP;
    case 'L': return CMD_TARGET_DIR_RIGHT;
    case 'N': return CMD_TARGET_DIR_DOWN_RIGHT;
    case 'U': return CMD_TARGET_DIR_UP_RIGHT;
    case 'Y': return CMD_TARGET_DIR_UP_LEFT;

    default: return CMD_NO_CMD;
    }
}

void direction_choose_compass( struct dist& moves )
{
    moves.isValid       = true;
    moves.isTarget      = false;
    moves.isMe          = false;
    moves.isCancel      = false;
    moves.dx = moves.dy = 0;

    do {
        const command_type key_command = read_direction_key();

        if (key_command == CMD_TARGET_SELECT)
        {
            moves.dx = moves.dy = 0;
            moves.isMe = true;
            break;
        }
        
        const int i = targeting_cmd_to_compass(key_command);
        if ( i != -1 )
        {
            moves.dx = Compass[i].x;
            moves.dy = Compass[i].y;
        }
        else if ( key_command == CMD_TARGET_CANCEL )
        {
            moves.isCancel = true;
            moves.isValid = false;
        }
    } while ( !moves.isCancel && moves.dx == 0 && moves.dy == 0 );
    
    return;
}

static int targeting_cmd_to_compass( command_type command )
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

static int targeting_cmd_to_feature( command_type command )
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
        return "? - help, Shift-Dir - shoot in a straight line";
    case DIR_TARGET:
        return "? - help, Dir - move target cursor";
    default:
        return "? - help";
    }
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
//---------------------------------------------------------------
void direction(struct dist& moves, targeting_type restricts,
               int mode, bool just_looking, const char *prompt)
{
    // NOTE: Even if just_looking is set, moves is still interesting,
    // because we can travel there!

    if ( restricts == DIR_DIR )
    {
        direction_choose_compass( moves );
        return;
    }

    cursor_control con(!Options.use_fake_cursor);
    
    int dir = 0;
    bool show_beam = false;
    ray_def ray;

    FixedVector < char, 2 > objfind_pos;
    FixedVector < char, 2 > monsfind_pos;

    // init
    moves.dx = moves.dy = 0;
    moves.tx = you.x_pos;
    moves.ty = you.y_pos;

    bool skip_iter = false;
    bool found_autotarget = false;
    bool target_unshifted = Options.target_unshifted_dirs;

    // Find a default target
    if ( Options.default_target && mode == TARG_ENEMY )
    {
        skip_iter = true;   // skip first iteration...XXX mega-hack
        if ( you.prev_targ != MHITNOT && you.prev_targ != MHITYOU )
        {
            const monsters *montarget = &menv[you.prev_targ];
            if (mons_near(montarget) && player_monster_visible(montarget))
            {
                found_autotarget = true;
                moves.tx = montarget->x;
                moves.ty = montarget->y;
            }
        }
    }

    // Prompts might get scrolled off if you have too few lines available.
    // We'll live with that.
    if ( !just_looking )
        mprf(MSGCH_PROMPT, "%s (%s)", prompt? prompt : "Aim",
             target_mode_help_text(restricts));

    while (1)
    {
        // Reinit...this needs to be done every loop iteration
        // because moves is more persistent than loop_done.
        moves.isValid       = false;
        moves.isTarget      = false;
        moves.isMe          = false;
        moves.isCancel      = false;
        moves.isEndpoint    = false;
        moves.choseRay      = false;

        cursorxy( grid2viewX(moves.tx), grid2viewY(moves.ty) );

        command_type key_command;

        if ( skip_iter )
        {
            if ( found_autotarget )
                key_command = CMD_NO_CMD;
            else
                key_command = CMD_TARGET_CYCLE_FORWARD; // find closest enemy
        }
        else
            key_command = read_direction_key(just_looking);

        if (target_unshifted && moves.tx == you.x_pos && moves.ty == you.y_pos
            && restricts != DIR_TARGET)
        {
            key_command = shift_direction(key_command);
        }
        
        if (target_unshifted &&
            (key_command == CMD_TARGET_CYCLE_FORWARD
             || key_command == CMD_TARGET_CYCLE_BACK
             || key_command == CMD_TARGET_OBJ_CYCLE_FORWARD
             || key_command == CMD_TARGET_OBJ_CYCLE_BACK))
        {
            target_unshifted = false;
        }

        bool need_beam_redraw = false;
        bool force_redraw = false;
        bool loop_done = false;

        const int old_tx = moves.tx + (skip_iter ? 500 : 0); // hmmm...hack
        const int old_ty = moves.ty;

        int i, mid;

        switch ( key_command )
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
            i = targeting_cmd_to_compass(key_command);
            moves.tx += Compass[i].x;
            moves.ty += Compass[i].y;
            break;

        case CMD_TARGET_DIR_DOWN_LEFT:
        case CMD_TARGET_DIR_DOWN:
        case CMD_TARGET_DIR_DOWN_RIGHT:
        case CMD_TARGET_DIR_LEFT:
        case CMD_TARGET_DIR_RIGHT:
        case CMD_TARGET_DIR_UP_LEFT:
        case CMD_TARGET_DIR_UP:
        case CMD_TARGET_DIR_UP_RIGHT:
            i = targeting_cmd_to_compass(key_command);

            if ( restricts != DIR_TARGET )
            {
                // A direction is allowed, and we've selected it.
                moves.dx = Compass[i].x;
                moves.dy = Compass[i].y;
                // Needed for now...eventually shouldn't be necessary
                moves.tx = you.x_pos + moves.dx;
                moves.ty = you.y_pos + moves.dy;
                moves.isValid = true;
                moves.isTarget = false;
                loop_done = true;
            }
            else
            {
                // Direction not allowed, so just move in that direction.
                // Maybe make this a bigger jump?
                if (restricts == DIR_TARGET)
                {
                    moves.tx += Compass[i].x * 3;
                    moves.ty += Compass[i].y * 3;
                }
                else
                {
                    moves.tx += Compass[i].x;
                    moves.ty += Compass[i].y;
                }
            }
            break;

        case CMD_TARGET_CYCLE_BEAM:
            show_beam = find_ray(you.x_pos, you.y_pos, moves.tx, moves.ty,
                                 true, ray, (show_beam ? 1 : 0));
            need_beam_redraw = true;
            break;
            
        case CMD_TARGET_HIDE_BEAM:
            show_beam = false;
            need_beam_redraw = true;
            break;
            
        case CMD_TARGET_FIND_YOU:
            moves.tx = you.x_pos;
            moves.ty = you.y_pos;
            moves.dx = 0;
            moves.dy = 0;
            break;

        case CMD_TARGET_FIND_TRAP:
        case CMD_TARGET_FIND_PORTAL:
        case CMD_TARGET_FIND_ALTAR:
        case CMD_TARGET_FIND_UPSTAIR:
        case CMD_TARGET_FIND_DOWNSTAIR:
            int thing_to_find;
            thing_to_find = targeting_cmd_to_feature(key_command);
            if (find_square_wrapper(moves.tx, moves.ty, objfind_pos, 1,
                                    find_feature, thing_to_find, true,
                                    Options.target_los_first ?
                                    LOS_FLIPVH : LOS_ANY))
            {
                moves.tx = objfind_pos[0];
                moves.ty = objfind_pos[1];
            }
            else
            {
                flush_input_buffer(FLUSH_ON_FAILURE);
            }
            break;

        case CMD_TARGET_CYCLE_TARGET_MODE:
            mode = (mode + 1) % TARG_NUM_MODES;
            mprf( "Targeting mode is now: %s",
                  (mode == TARG_ANY)   ? "any" :
                  (mode == TARG_ENEMY) ? "enemies" :
                  "friends" );
            break;
            
        case CMD_TARGET_PREV_TARGET:
            // Do we have a previous target?
            if (you.prev_targ == MHITNOT || you.prev_targ == MHITYOU)
            {
                mpr("You haven't got a previous target.");
                break;
            }

            // we have a valid previous target (maybe)
            {
                const monsters *montarget = &menv[you.prev_targ];
                
                if (!mons_near(montarget) ||
                    !player_monster_visible( montarget ))
                {
                    mpr("You can't see that creature any more.");
                }
                else
                {
                    // We have all the information we need
                    moves.isValid = true;
                    moves.isTarget = true;
                    moves.tx = montarget->x;
                    moves.ty = montarget->y;
                    if ( !just_looking )
                        loop_done = true;
                }
                break;
            }

        case CMD_TARGET_SELECT_ENDPOINT:
            moves.isEndpoint = true;
            // intentional fall-through
        case CMD_TARGET_SELECT: // finalize current choice
            moves.isValid = true;
            moves.isTarget = true;
            loop_done = true;
            // maybe we should except just_looking here?
            mid = mgrd[moves.tx][moves.ty];
            if ( mid != NON_MONSTER )
                you.prev_targ = mid;
            break;

        case CMD_TARGET_OBJ_CYCLE_BACK:
        case CMD_TARGET_OBJ_CYCLE_FORWARD:
            dir = (key_command == CMD_TARGET_OBJ_CYCLE_BACK) ? -1 : 1;
            if (find_square_wrapper( moves.tx, moves.ty, objfind_pos, dir,
                                     find_object, 0, true,
                                     Options.target_los_first
                                     ? (dir == 1? LOS_FLIPVH : LOS_FLIPHV)
                                     : LOS_ANY))
            {
                moves.tx = objfind_pos[0];
                moves.ty = objfind_pos[1];
            }
            else
            {
                flush_input_buffer(FLUSH_ON_FAILURE);
            }
            break;
        
        case CMD_TARGET_CYCLE_FORWARD:
        case CMD_TARGET_CYCLE_BACK:
            dir = (key_command == CMD_TARGET_CYCLE_BACK) ? -1 : 1;
            if (find_square_wrapper( moves.tx, moves.ty, monsfind_pos, dir, 
                                     find_monster, mode, Options.target_wrap ))
            {
                moves.tx = monsfind_pos[0];
                moves.ty = monsfind_pos[1];
            }
            else
            {
                flush_input_buffer(FLUSH_ON_FAILURE);
            }
            break;

        case CMD_TARGET_CANCEL:
            loop_done = true;
            moves.isCancel = true;
            break;

#ifdef WIZARD
        case CMD_TARGET_WIZARD_MAKE_FRIENDLY:
            // Maybe we can skip this check...but it can't hurt
            if (!you.wizard || !in_bounds(moves.tx, moves.ty))
                break;
            mid = mgrd[moves.tx][moves.ty];
            if (mid == NON_MONSTER) // can put in terrain description here
                break;

            {
                monsters &m = menv[mid];
                m.attitude = m.attitude == ATT_FRIENDLY?
                    ATT_HOSTILE : ATT_FRIENDLY;
            }
            break;
#endif
            
        case CMD_TARGET_DESCRIBE:
            // Maybe we can skip this check...but it can't hurt
            if (!in_bounds(moves.tx, moves.ty))
                break;
            mid = mgrd[moves.tx][moves.ty];
            if (mid == NON_MONSTER) // can put in terrain description here
            {
                int oid = igrd[moves.tx][moves.ty];
                if ( oid == NON_ITEM || mitm[oid].base_type == OBJ_GOLD )
                    break;
                describe_item( mitm[igrd[moves.tx][moves.ty]] );
            }
            else
            {
#if (!DEBUG_DIAGNOSTICS)
                if (!player_monster_visible( &menv[mid] ))
                    break;
#endif

                describe_monsters(menv[mid].type, mid);
            }
            force_redraw = true;
            redraw_screen();
            mesclr(true);
            break;

        case CMD_TARGET_HELP:
            show_targeting_help();
            force_redraw = true;
            redraw_screen();
            mesclr(true);
            break;
            
        default:
            break;
        }
        
        if ( loop_done == true )
        {
            // This is where we either finalize everything, or else
            // decide that we're not really done and continue looping.

            if ( just_looking ) // easy out
                break;

            // A bunch of confirmation tests; if we survive them all,
            // then break out.

            // Conceivably we might want to confirm on TARG_ANY too.
            if ( moves.isTarget &&
                 moves.tx == you.x_pos && moves.ty == you.y_pos &&
                 mode == TARG_ENEMY && Options.confirm_self_target &&
                 !yesno("Really target yourself?"))
            {
                mesclr();
                mprf(MSGCH_PROMPT, "%s (%s)", prompt? prompt : "Aim",
                     target_mode_help_text(restricts));
            }
            else if ( moves.isTarget && !see_grid(moves.tx, moves.ty) )
            {
                mpr("Sorry, you can't target what you can't see.");
            }
            // Ask for confirmation if we're quitting for some odd reason
            else if ( moves.isValid || moves.isCancel ||
                 yesno("Are you sure you want to fizzle?") )
            {
                // Finalize whatever is inside the loop
                // (moves-internal finalizations can be done later)
                moves.choseRay = show_beam;
                moves.ray = ray;
                break;
            }
        }

        // We'll go on looping. Redraw whatever is necessary.

        // Tried to step out of bounds
        if ( !in_viewport_bounds(grid2viewX(moves.tx), grid2viewY(moves.ty)) )
        {
            moves.tx = old_tx;
            moves.ty = old_ty;
        }

        bool have_moved = false;

        if ( old_tx != moves.tx || old_ty != moves.ty )
        {
            have_moved = true;
            show_beam = show_beam &&
                find_ray(you.x_pos, you.y_pos, moves.tx, moves.ty, true, ray);
        }

        if ( force_redraw )
            have_moved = true;
        
        if ( have_moved )
        {
            // If the target x,y has changed, the beam must have changed.
            if ( show_beam )
                need_beam_redraw = true;

            if ( !skip_iter )   // don't clear before we get a chance to see
                mesclr(true);   // maybe not completely necessary

            if ( !in_vlos(grid2viewX(moves.tx), grid2viewY(moves.ty)) )
                describe_oos_square(moves.tx, moves.ty);
            else if ( in_bounds(moves.tx, moves.ty) )
                describe_cell(moves.tx, moves.ty);
        }

        if ( need_beam_redraw )
        {
            viewwindow(true, false);
            if ( show_beam &&
                 in_vlos(grid2viewX(moves.tx), grid2viewY(moves.ty)) )
            {
                // Draw the new ray with magenta '*'s, not including
                // your square or the target square.
                ray_def raycopy = ray; // temporary copy to work with
                textcolor(MAGENTA);
                while ( raycopy.x() != moves.tx || raycopy.y() != moves.ty )
                {
                    if ( raycopy.x() != you.x_pos || raycopy.y() != you.y_pos )
                    {
                        // Sanity: don't loop forever if the ray is problematic
                        if ( !in_los(raycopy.x(), raycopy.y()) )
                            break;
                        gotoxy( grid2viewX(raycopy.x()),
                                grid2viewY(raycopy.y()));
                        cprintf("*");
                    }
                    raycopy.advance();
                }
                textcolor(LIGHTGREY);
            }
        }
        skip_iter = false;      // only skip one iteration at most
    }
    moves.isMe = (moves.tx == you.x_pos && moves.ty == you.y_pos);
    mesclr();

    // We need this for directional explosions, otherwise they'll explode one
    // square away from the player.
    extend_move_to_edge(moves);
}

static void extend_move_to_edge(dist &moves)
{
    if (!moves.dx && !moves.dy)
        return;

    // now the tricky bit - extend the target x,y out to map edge.
    int mx = 0, my = 0;

    if (moves.dx > 0)
        mx = (GXM  - 1) - you.x_pos;
    if (moves.dx < 0)
        mx = you.x_pos;

    if (moves.dy > 0)
        my = (GYM - 1) - you.y_pos;
    if (moves.dy < 0)
        my = you.y_pos;

    if (!(mx == 0 || my == 0))
    {
        if (mx < my)
            my = mx;
        else
            mx = my;
    }
    moves.tx = you.x_pos + moves.dx * mx;
    moves.ty = you.y_pos + moves.dy * my;
}

// Attempts to describe a square that's not in line-of-sight. If
// there's a stash on the square, announces the top item and number
// of items, otherwise, if there's a stair that's in the travel
// cache and noted in the Dungeon (O)verview, names the stair.
static void describe_oos_square(int x, int y)
{
    mpr("You can't see that place.");
    
    if (!in_bounds(x, y) || !is_mapped(x, y))
        return;

    describe_stash(x, y);
    describe_feature(x, y, true);
}

bool in_vlos(int x, int y)
{
    return in_los_bounds(x, y) 
            && (env.show[x - LOS_SX][y] || (x == VIEW_CX && y == VIEW_CY));
}

bool in_los(int x, int y)
{
    const int tx = x + VIEW_CX - you.x_pos,
              ty = y + VIEW_CY - you.y_pos;
    
    if (!in_los_bounds(tx, ty))
        return (false);

    return (x == you.x_pos && y == you.y_pos) || env.show[tx - LOS_SX][ty];
}

static bool find_monster( int x, int y, int mode )
{
    const int targ_mon = mgrd[ x ][ y ];
    return (targ_mon != NON_MONSTER 
        && in_los(x, y)
        && player_monster_visible( &(menv[targ_mon]) )
        && !mons_is_mimic( menv[targ_mon].type )
        && (mode == TARG_ANY
            || (mode == TARG_FRIEND && mons_friendly( &menv[targ_mon] ))
            || (mode == TARG_ENEMY 
                && !mons_friendly( &menv[targ_mon] )
                && 
                (Options.target_zero_exp || 
                    !mons_class_flag( menv[targ_mon].type, M_NO_EXP_GAIN )) )));
}

static bool find_feature( int x, int y, int mode )
{
    // The stair need not be in LOS if the square is mapped.
    if (!in_los(x, y) && (!Options.target_oos || !is_mapped(x, y)))
        return (false);

    return is_feature(mode, x, y);
}

static bool find_object(int x, int y, int mode)
{
    const int item = igrd[x][y];
    // The square need not be in LOS if the stash tracker knows this item.
    return (item != NON_ITEM
            && (in_los(x, y)
                || (Options.target_oos && is_mapped(x, y) && is_stash(x, y))));
}

static int next_los(int dir, int los, bool wrap)
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
    return (x >= VIEW_SX && x <= VIEW_EX && y >= VIEW_SY && y <= VIEW_EY);
}

bool in_los_bounds(int x, int y)
{
    return !(x > LOS_EX || x < LOS_SX || y > LOS_EY || y < LOS_SY);
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
static char find_square( unsigned char xps, unsigned char yps,
                         FixedVector<char, 2> &mfp, char direction,
                         bool (*find_targ)( int x, int y, int mode ),
                         int mode, bool wrap, int los )
{
    // the day will come when [unsigned] chars will be consigned to
    // the fires of Gehenna. Not quite yet, though.

    int temp_xps = xps;
    int temp_yps = yps;
    char x_change = 0;
    char y_change = 0;

    bool onlyVis = false, onlyHidden = false;

    int i, j;

    if (los == LOS_NONE)
        return (0);

    if (los == LOS_FLIPVH || los == LOS_FLIPHV)
    {
        if (in_los_bounds(xps, yps))
        {
            // We've been told to flip between visible/hidden, so we
            // need to find what we're currently on.
            const bool vis = (env.show[xps - 8][yps] 
                              || (xps == VIEW_CX && yps == VIEW_CY));
            
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

    const int minx = VIEW_SX, maxx = VIEW_EX,
              miny = VIEW_SY - VIEW_Y_DIFF, maxy = VIEW_EY + VIEW_Y_DIFF,
              ctrx = VIEW_CX, ctry = VIEW_CY;

    while (temp_xps >= minx - 1 && temp_xps <= maxx
                && temp_yps <= maxy && temp_yps >= miny - 1)
    {
        if (direction == 1 && temp_xps == minx && temp_yps == maxy)
        {
            return find_square(ctrx, ctry, mfp, direction, find_targ, mode, 
                    false, next_los(direction, los, wrap));
        }
        if (direction == -1 && temp_xps == ctrx && temp_yps == ctry)
        {
            return find_square(minx, maxy, mfp, direction, find_targ, mode, 
                    false, next_los(direction, los, wrap));
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
            /*
               This part checks all eight surrounding squares to find the
               one that leads on to the present square.
             */
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

                        if (xi >= 0 && yj <= 0)
                        {
                            if (abs(xi) > abs(yj + 1))
                            {
                                x_change = 0;
                                y_change = -1;
                                if (xi > 0)
                                    y_change = 1;
                                goto finished_spiralling;
                            }
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

        const int targ_x = you.x_pos + temp_xps - ctrx;
        const int targ_y = you.y_pos + temp_yps - ctry;

        // We don't want to be looking outside the bounds of the arrays:
        //if (!in_los_bounds(temp_xps, temp_yps))
        //    continue;

        if (temp_xps < minx - 1 || temp_xps > maxx
                || temp_yps < VIEW_SY || temp_yps > VIEW_EY)
            continue;

        if (targ_x < 1 || targ_x >= GXM || targ_y < 1 || targ_y >= GYM)
            continue;

        if ((onlyVis || onlyHidden) && onlyVis != in_los(targ_x, targ_y))
            continue;

        if (find_targ(targ_x, targ_y, mode)) {
            mfp[0] = temp_xps;
            mfp[1] = temp_yps;
            return (1);
        }
    }

    return (direction == 1?
        find_square(ctrx, ctry, mfp, direction, find_targ, mode, false, 
                    next_los(direction, los, wrap))
      : find_square(minx, maxy, mfp, direction, find_targ, mode, false,
                    next_los(direction, los, wrap)));
}

// XXX Unbelievably hacky. And to think that my goal was to clean up the code.
// Identical to find_square, except that input (tx, ty) and output
// (mfp) are in grid coordinates rather than view coordinates.
static char find_square_wrapper( int tx, int ty,
                                 FixedVector<char, 2> &mfp, char direction,
                                 bool (*find_targ)( int x, int y, int mode ),
                                 int mode, bool wrap, int los )
{
    const char r =  find_square(grid2viewX(tx), grid2viewY(ty),
                                mfp, direction, find_targ, mode, wrap, los);
    mfp[0] = view2gridX(mfp[0]);
    mfp[1] = view2gridY(mfp[1]);
    return r;
}

static void describe_feature(int mx, int my, bool oos)
{
    if (oos && !is_terrain_seen(mx, my))
        return;

    std::string desc = feature_description(mx, my);
    if (desc.length())
    {
        if (oos)
            desc = "[" + desc + "]";
        mpr(desc.c_str());
    }
}



// Returns a vector of features matching the given pattern.
std::vector<dungeon_feature_type> features_by_desc(const text_pattern &pattern)
{
    std::vector<dungeon_feature_type> features;

    if (pattern.valid())
    {
        for (int i = 0; i < NUM_FEATURES; ++i)
        {
            std::string fdesc = feature_description(i);
            if (fdesc.empty())
                continue;

            if (pattern.matches( fdesc ))
                features.push_back( dungeon_feature_type(i) );
        }
    }
    return (features);
}

std::string feature_description(int grid)
{
    switch (grid)
    {
    case DNGN_STONE_WALL:
        return ("A stone wall.");
    case DNGN_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        if (you.level_type == LEVEL_PANDEMONIUM)
            return ("A wall of the weird stuff which makes up Pandemonium.");
        else
            return ("A rock wall.");
    case DNGN_PERMAROCK_WALL:
        return ("An unnaturally hard rock wall.");    
    case DNGN_CLOSED_DOOR:
        return ("A closed door.");
    case DNGN_METAL_WALL:
        return ("A metal wall.");
    case DNGN_GREEN_CRYSTAL_WALL:
        return ("A wall of green crystal.");
    case DNGN_ORCISH_IDOL:
        return ("An orcish idol.");
    case DNGN_WAX_WALL:
        return ("A wall of solid wax.");
    case DNGN_SILVER_STATUE:
        return ("A silver statue.");
    case DNGN_GRANITE_STATUE:
        return ("A granite statue.");
    case DNGN_ORANGE_CRYSTAL_STATUE:
        return ("An orange crystal statue.");
    case DNGN_LAVA:
        return ("Some lava.");
    case DNGN_DEEP_WATER:
        return ("Some deep water.");
    case DNGN_SHALLOW_WATER:
        return ("Some shallow water.");
    case DNGN_UNDISCOVERED_TRAP:
    case DNGN_FLOOR:
        return ("Floor.");
    case DNGN_OPEN_DOOR:
        return ("An open door.");
    case DNGN_ROCK_STAIRS_DOWN:
        return ("A rock staircase leading down.");
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return ("A stone staircase leading down.");
    case DNGN_ROCK_STAIRS_UP:
        return ("A rock staircase leading upwards.");
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        return ("A stone staircase leading up.");
    case DNGN_ENTER_HELL:
        return ("A gateway to Hell.");
    case DNGN_TRAP_MECHANICAL:
        return ("A mechanical trap.");
    case DNGN_TRAP_MAGICAL:
        return ("A magical trap.");
    case DNGN_TRAP_III:
        return ("A trap.");
    case DNGN_ENTER_SHOP:
        return ("A shop.");
    case DNGN_ENTER_LABYRINTH:
        return ("A labyrinth entrance.");
    case DNGN_ENTER_DIS:
        return ("A gateway to the Iron City of Dis.");
    case DNGN_ENTER_GEHENNA:
        return ("A gateway to Gehenna.");
    case DNGN_ENTER_COCYTUS:
        return ("A gateway to the freezing wastes of Cocytus.");
    case DNGN_ENTER_TARTARUS:
        return ("A gateway to the decaying netherworld of Tartarus.");
    case DNGN_ENTER_ABYSS:
        return ("A gateway to the infinite Abyss.");
    case DNGN_EXIT_ABYSS:
        return ("A gateway leading out of the Abyss.");
    case DNGN_STONE_ARCH:
        return ("An empty arch of ancient stone.");
    case DNGN_ENTER_PANDEMONIUM:
        return ("A gate leading to the halls of Pandemonium.");
    case DNGN_EXIT_PANDEMONIUM:
        return ("A gate leading out of Pandemonium.");
    case DNGN_TRANSIT_PANDEMONIUM:
        return ("A gate leading to another region of Pandemonium.");
    case DNGN_ENTER_ORCISH_MINES:
        return ("A staircase to the Orcish Mines.");
    case DNGN_ENTER_HIVE:
        return ("A staircase to the Hive.");
    case DNGN_ENTER_LAIR:
        return ("A staircase to the Lair.");
    case DNGN_ENTER_SLIME_PITS:
        return ("A staircase to the Slime Pits.");
    case DNGN_ENTER_VAULTS:
        return ("A staircase to the Vaults.");
    case DNGN_ENTER_CRYPT:
        return ("A staircase to the Crypt.");
    case DNGN_ENTER_HALL_OF_BLADES:
        return ("A staircase to the Hall of Blades.");
    case DNGN_ENTER_ZOT:
        return ("A gate to the Realm of Zot.");
    case DNGN_ENTER_TEMPLE:
        return ("A staircase to the Ecumenical Temple.");
    case DNGN_ENTER_SNAKE_PIT:
        return ("A staircase to the Snake Pit.");
    case DNGN_ENTER_ELVEN_HALLS:
        return ("A staircase to the Elven Halls.");
    case DNGN_ENTER_TOMB:
        return ("A staircase to the Tomb.");
    case DNGN_ENTER_SWAMP:
        return ("A staircase to the Swamp.");
    case DNGN_ENTER_ISLANDS:
        return ("A staircase to the Islands.");
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_TEMPLE:
        return ("A staircase back to the Dungeon.");
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_ISLANDS:
        return ("A staircase back to the Lair.");
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
        return ("A staircase back to the Vaults.");
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        return ("A staircase back to the Mines.");
    case DNGN_RETURN_FROM_TOMB:
        return ("A staircase back to the Crypt.");
    case DNGN_RETURN_FROM_ZOT:
        return ("A gate leading back out of this place.");
    case DNGN_ALTAR_ZIN:
        return ("A glowing white marble altar of Zin.");
    case DNGN_ALTAR_SHINING_ONE:
        return ("A glowing golden altar of the Shining One.");
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        return ("An ancient bone altar of Kikubaaqudgha.");
    case DNGN_ALTAR_YREDELEMNUL:
        return ("A basalt altar of Yredelemnul.");
    case DNGN_ALTAR_XOM:
        return ("A shimmering altar of Xom.");
    case DNGN_ALTAR_VEHUMET:
        return ("A shining altar of Vehumet.");
    case DNGN_ALTAR_OKAWARU:
        return ("An iron altar of Okawaru.");
    case DNGN_ALTAR_MAKHLEB:
        return ("A burning altar of Makhleb.");
    case DNGN_ALTAR_SIF_MUNA:
        return ("A deep blue altar of Sif Muna.");
    case DNGN_ALTAR_TROG:
        return ("A bloodstained altar of Trog.");
    case DNGN_ALTAR_NEMELEX_XOBEH:
        return ("A sparkling altar of Nemelex Xobeh.");
    case DNGN_ALTAR_ELYVILON:
        return ("A silver altar of Elyvilon.");
    case DNGN_ALTAR_LUGONU:
        return ("A corrupted altar of Lugonu.");
    case DNGN_BLUE_FOUNTAIN:
        return ("A fountain of clear blue water.");
    case DNGN_SPARKLING_FOUNTAIN:
        return ("A fountain of sparkling water.");
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        return ("A dry fountain.");
    default:
        return ("");
    }
}

std::string feature_description(int mx, int my)
{
    int   trf;            // used for trap type??
    
    const int grid = grd[mx][my];
    std::string desc = feature_description(grid);
    switch (grid)
    {
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        for (trf = 0; trf < MAX_TRAPS; trf++)
        {
            if (env.trap[trf].x == mx
                && env.trap[trf].y == my)
            {
                break;
            }

            if (trf == MAX_TRAPS - 1)
            {
                mpr("Error - couldn't find that trap.");
                error_message_to_player();
                break;
            }
        }

        switch (env.trap[trf].type)
        {
        case TRAP_DART:
            return ("A dart trap.");
        case TRAP_ARROW:
            return ("An arrow trap.");
        case TRAP_SPEAR:
            return ("A spear trap.");
        case TRAP_AXE:
            return ("An axe trap.");
        case TRAP_TELEPORT:
            return ("A teleportation trap.");
        case TRAP_AMNESIA:
            return ("An amnesia trap.");
        case TRAP_BLADE:
            return ("A blade trap.");
        case TRAP_BOLT:
            return ("A bolt trap.");
        case TRAP_ZOT:
            return ("A Zot trap.");
        case TRAP_NEEDLE:
            return ("A needle trap.");
        default:
            mpr("An undefined trap. Huh?");
            error_message_to_player();
            break;
        }
        break;
    case DNGN_ENTER_SHOP:
        return (shop_name(mx, my));
    default:
        break;
    }
    return (desc);
}

static void describe_mons_enchantment(const monsters &mons,
                                      const mon_enchant &ench,
                                      bool paralysed)
{
    // Suppress silly-looking combinations, even if they're
    // internally valid.
    if (paralysed && (ench.ench == ENCH_SLOW || ench.ench == ENCH_HASTE))
        return;

    if (ench.ench == ENCH_HASTE && mons.has_ench(ENCH_BERSERK))
        return;
            
    strcpy(info, mons_pronoun(mons.type, PRONOUN_CAP));

    switch (ench.ench)
    {
    case ENCH_POISON:
        strcat(info, " is poisoned.");
        break;
    case ENCH_SICK:
        strcat(info, " is sick.");
        break;
    case ENCH_ROT:
        strcat(info, " is rotting away."); //jmf: "covered in sores"?
        break;
    case ENCH_BACKLIGHT:
        strcat(info, " is softly glowing.");
        break;
    case ENCH_SLOW:
        strcat(info, " is moving slowly.");
        break;
    case ENCH_BERSERK:
        strcat(info, " is berserk!");
        break;
    case ENCH_HASTE:
        strcat(info, " is moving very quickly.");
        break;
    case ENCH_CONFUSION:
        strcat(info, " appears to be bewildered and confused.");
        break;
    case ENCH_INVIS:
        strcat(info, " is slightly transparent.");
        break;
    case ENCH_CHARM:
        strcat(info, " is in your thrall.");
        break;
    case ENCH_STICKY_FLAME:
        strcat(info, " is covered in liquid flames.");
        break;
    default:
        info[0] = 0;
        break;
    } // end switch
    if (info[0])
        mpr(info);
}

static void describe_cell(int mx, int my)
{
    bool mimic_item = false;
    
    if (mgrd[mx][my] != NON_MONSTER)
    {
        int i = mgrd[mx][my];

        if (grd[mx][my] == DNGN_SHALLOW_WATER)
        {
            if (!player_monster_visible(&menv[i]) && !mons_flies(&menv[i]))
            {
                mpr("There is a strange disturbance in the water here.");
            }
        }

#if DEBUG_DIAGNOSTICS
        if (!player_monster_visible( &menv[i] ))
            mpr( "There is a non-visible monster here.", MSGCH_DIAGNOSTICS );
#else
        if (!player_monster_visible( &menv[i] ))
            goto look_clouds;
#endif

        const int mon_wep = menv[i].inv[MSLOT_WEAPON];
        const int mon_arm = menv[i].inv[MSLOT_ARMOUR];
        mprf("%s. ('v' to describe)", ptr_monam(&(menv[i]), DESC_CAP_A));

        if (menv[i].type != MONS_DANCING_WEAPON && mon_wep != NON_ITEM)
        {
            std::ostringstream msg;
            msg << mons_pronoun( menv[i].type, PRONOUN_CAP )
                << " is wielding "
                << mitm[mon_wep].name(DESC_NOCAP_A);

            // 2-headed ogres can wield 2 weapons
            if ((menv[i].type == MONS_TWO_HEADED_OGRE
                 || menv[i].type == MONS_ETTIN)
                && menv[i].inv[MSLOT_MISSILE] != NON_ITEM)
            {
                msg << " and "
                    <<  mitm[menv[i].inv[MSLOT_MISSILE]].name(DESC_NOCAP_A);
            }
            msg << ".";
            mpr(msg.str().c_str());
        }

        if (mon_arm != NON_ITEM)
            mprf("%s is wearing %s.",
                 mons_pronoun(menv[i].type, PRONOUN_CAP),
                 mitm[mon_arm].name(DESC_NOCAP_A).c_str());

        if (menv[i].type == MONS_HYDRA)
            mprf("It has %d head%s.", menv[i].number,
                 (menv[i].number > 1? "s" : ""));;

        print_wounds(&menv[i]);

        if (mons_is_mimic( menv[i].type ))
            mimic_item = true;
        else if (!mons_class_flag(menv[i].type, M_NO_EXP_GAIN)
                 && !mons_is_statue(menv[i].type))
        {
            if (menv[i].behaviour == BEH_SLEEP)
            {
                mprf("%s doesn't appear to have noticed you.",
                     mons_pronoun(menv[i].type, PRONOUN_CAP));
            }
            // Applies to both friendlies and hostiles
            else if (menv[i].behaviour == BEH_FLEE)
            {
                mprf("%s is retreating.",
                     mons_pronoun(menv[i].type, PRONOUN_CAP));
            }
            // hostile with target != you
            else if (!mons_friendly(&menv[i]) && menv[i].foe != MHITYOU)
            {
                // special case: batty monsters get set to BEH_WANDER as
                // part of their special behaviour.
                if (!testbits(menv[i].flags, MF_BATTY))
                    mprf("%s doesn't appear to be interested in you.",
                         mons_pronoun(menv[i].type, PRONOUN_CAP));
            }
        }

        if (menv[i].attitude == ATT_FRIENDLY)
            mprf("%s is friendly.", mons_pronoun(menv[i].type, PRONOUN_CAP));

        const bool paralysed = mons_is_paralysed(&menv[i]);
        if (paralysed)
            mprf("%s is paralysed.", mons_pronoun(menv[i].type, PRONOUN_CAP));

        for (mon_enchant_list::const_iterator e = menv[i].enchantments.begin();
             e != menv[i].enchantments.end(); ++e)
        {
            describe_mons_enchantment(menv[i], *e, paralysed);
        }
#if DEBUG_DIAGNOSTICS
        stethoscope(i);
#endif
    }

#if (!DEBUG_DIAGNOSTICS)
  // removing warning
  look_clouds:
#endif
    if (env.cgrid[mx][my] != EMPTY_CLOUD)
    {
        const int cloud_inspected = env.cgrid[mx][my];
        const cloud_type ctype = (cloud_type) env.cloud[cloud_inspected].type;

        mprf("There is a cloud of %s here.", cloud_name(ctype).c_str());
    }

    int targ_item = igrd[ mx ][ my ];

    if (targ_item != NON_ITEM)
    {
        // If a mimic is on this square, we pretend its the first item -- bwr
        if (mimic_item)
            mpr("There is something else lying underneath.",MSGCH_FLOOR_ITEMS);
        else
        {
            if (mitm[ targ_item ].base_type == OBJ_GOLD)
                mprf( MSGCH_FLOOR_ITEMS, "A pile of gold coins." );
            else
                mprf( MSGCH_FLOOR_ITEMS, "You see %s here.",
                      mitm[targ_item].name(DESC_NOCAP_A).c_str());

            if (mitm[ targ_item ].link != NON_ITEM)
                mprf( MSGCH_FLOOR_ITEMS,
                      "There is something else lying underneath.");
        }
    }

    std::string feature_desc = feature_description(mx, my);
#ifdef DEBUG_DIAGNOSTICS
    mprf("(%d,%d): %s", mx, my, feature_desc.c_str());
#else
    mpr(feature_desc.c_str());
#endif
}

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

#include <cstdarg>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

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

#ifdef USE_MACROS
#include "macro.h"
#endif

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

// x and y offsets in the following order:
// SW, S, SE, W, E, NW, N, NE
static const char xcomp[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
static const char ycomp[9] = { 1, 1, 1, 0, 0, 0, -1, -1, -1 };

// [dshaligram] Removed . and 5 from dirchars so it's easier to
// special case them.
static const char dirchars[19] = { "b1j2n3h4bbl6y7k8u9" };
static const char DOSidiocy[10]     = { "OPQKSMGHI" };
static const char DOSunidiocy[10]   = { "bjnh.lyku" };
static const char *aim_prompt = "Aim (move cursor or -/+/=, change mode with CTRL-F, select with . or >)";

static void describe_feature(int mx, int my, bool oos);
static void describe_cell(int mx, int my);

static bool find_object( int x, int y, int mode );
static bool find_monster( int x, int y, int mode );
static bool find_feature( int x, int y, int mode );
static char find_square( unsigned char xps, unsigned char yps, 
                       FixedVector<char, 2> &mfp, char direction,
                       bool (*targ)(int, int, int),
                       int mode = TARG_ANY,
                       bool wrap = false,
                       int los = LOS_ANY);

static bool is_mapped(int x, int y)
{
    return (is_player_mapped(x, y));
}

int dos_direction_unmunge(int doskey)
{
    const char *pos = strchr(DOSidiocy, doskey);
    return (pos? DOSunidiocy[ pos - DOSidiocy ] : doskey);
}

//---------------------------------------------------------------
//
// direction
//
// input: restricts : DIR_NONE      accepts keypad dir or targetting
//                    DIR_TARGET    must use targetting.
//                    DIR_DIR       must use keypad direction
//
//
// outputs: dist structure:
//
//           isValid        a valid target or direction was chosen
//           isCancel       player hit 'escape'
//           isTarget       targetting was used
//           tx,ty          target x,y or logical beam extension to
//                             edge of map if keypad direction used.
//           dx,dy          direction delta if keypad used {-1,0,1}
//
// SYNOPSIS:
//
// gets a direction, or any of the follwing:
//
// *     go to targetting mode
// +,=   go to targetting mode, next monster
// -               "          , prev monster
// t,p   auto-select previous target
//
//
// targetting mode is handled by look_around()
//---------------------------------------------------------------

void direction2( struct dist &moves, int restrict, int mode )
{
    bool dirChosen = false;
    bool targChosen = false;
    int dir = 0;

    // init
    moves.isValid       = false;
    moves.isTarget      = false;
    moves.isMe          = false;
    moves.isCancel      = false;
    moves.dx = moves.dy = 0;
    moves.tx = moves.ty = 0;

    // XXX.  this is ALWAYS in relation to the player. But a bit of a hack
    // nonetheless!  --GDL
    gotoxy( VIEW_CX + 1, VIEW_CY );

    int keyin = getchm(KC_TARGETING);

    if (keyin == 0)             // DOS idiocy (emulated by win32 console)
    {
        keyin = getchm(KC_TARGETING);        // grrr.
        if (keyin == '*')
        {
            targChosen = true;
            dir = 0;
        }
        else
        {
            if (strchr(DOSidiocy, keyin) == NULL)
                return;
            dirChosen = true;
            dir = (int)(strchr(DOSidiocy, keyin) - DOSidiocy);
        }
    }
    else
    {
        if (strchr( dirchars, keyin ) != NULL)
        {
            dirChosen = true;
            dir = (int)(strchr(dirchars, keyin) - dirchars) / 2;
        }
        else
        {
            switch (keyin)
            {
                case CONTROL('F'):
                    mode = (mode + 1) % TARG_NUM_MODES;
                    
                    snprintf( info, INFO_SIZE, "Targeting mode is now: %s", 
                              (mode == TARG_ANY)   ? "any" :
                              (mode == TARG_ENEMY) ? "enemies" 
                                                   : "friends" );

                    mpr( info );

                    targChosen = true;
                    dir = 0;
                    break;

                case '-':
                    targChosen = true;
                    dir = -1;
                    break;

                case '*':
                    targChosen = true;
                    dir = 0;
                    break;

                case ';':
                    targChosen = true;
                    dir        = -3;
                    break;

                case '\'':
                    targChosen = true;
                    dir        = -2;
                    break;
                    
                case '+':
                case '=':
                    targChosen = true;
                    dir = 1;
                    break;

                case 't':
                case 'p':
                    targChosen = true;
                    dir = 2;
                    break;

                case '.':
                case '5':
                    dirChosen = true;
                    dir = 4;
                    break;

                case ESCAPE:
                    moves.isCancel = true;
                    return;

                default:
                    break;
            }
        }
    }

    // at this point, we know exactly the input - validate
    if (!(targChosen || dirChosen) || (targChosen && restrict == DIR_DIR))
    {
        mpr("What an unusual direction.");
        return;
    }

    // special case: they typed a dir key, but they're in target-only mode
    if (dirChosen && restrict == DIR_TARGET)
    {
        mpr(aim_prompt);
        look_around( moves, false, dir, mode );
        return;
    }

    if (targChosen)
    {
        if (dir < 2)
        {
            mpr(aim_prompt);
            moves.prev_target = dir;
            look_around( moves, false, -1, mode );
            if (moves.prev_target != -1)      // -1 means they pressed 'p'
                return;
        }

        // chose to aim at previous target.  do we have one?
        if (you.prev_targ == MHITNOT || you.prev_targ == MHITYOU)
        {
            mpr("You haven't got a target.");
            return;
        }

        // we have a valid previous target (maybe)
        struct monsters *montarget = &menv[you.prev_targ];

        if (!mons_near(montarget) || !player_monster_visible( montarget ))
        {
            mpr("You can't see that creature any more.");
            return;
        }
        else
        {
            moves.isValid = true;
            moves.isTarget = true;
            moves.tx = montarget->x;
            moves.ty = montarget->y;
        }
        return;
    }

    // at this point, we have a direction, and direction is allowed.
    moves.isValid = true;
    moves.isTarget = false;
    moves.dx = xcomp[dir];
    moves.dy = ycomp[dir];
    if (xcomp[dir] == 0 && ycomp[dir] == 0)
        moves.isMe = true;

    // now the tricky bit - extend the target x,y out to map edge.
    int mx, my;
    mx = my = 0;

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

/* safe version of direction */
void direction( struct dist &moves, int restrict, int mode,
                bool confirm_fizzle )
{
    while ( 1 )
    {
        direction2( moves, restrict, mode );
        if ( moves.isMe && Options.confirm_self_target == true &&
             mode != TARG_FRIEND )
        {
            if ( yesno("Really target yourself? ", false, 'n') )
                return;
            else
                mpr("Choose a better target.", MSGCH_PROMPT);
        }
        else if ( confirm_fizzle && !moves.isValid && Options.fizzlecheck_on )
        {
            if ( yesno("Really fizzle? ", false, 'n') )
                return;
            else
                mpr("Try again.", MSGCH_PROMPT);
        }
        else
        {
            return;
        }
    }
}


// Attempts to describe a square that's not in line-of-sight. If
// there's a stash on the square, announces the top item and number
// of items, otherwise, if there's a stair that's in the travel
// cache and noted in the Dungeon (O)verview, names the stair.
static void describe_oos_square(int x, int y)
{
    if (!in_bounds(x, y) || !is_mapped(x, y))
        return;

#ifdef STASH_TRACKING
    describe_stash(x, y);
#endif
    describe_feature(x, y, true);
}

//---------------------------------------------------------------
//
// look_around
//
// Accessible by the x key and when using cursor aiming. Lets you
// find out what symbols mean, and is the way to access monster
// descriptions.
//
// input: dist.prev_target : -1 is last monster
//                          0 is no monster selected
//                          1 is next monster
//
// input: first_move is -1 if no initial cursor move, otherwise
// make 1 move in that direction.
//
//
// output: if dist.prev_target is -1 on OUTPUT, it means that
//   player selected 'p' ot 't' for last targetted monster.
//
//   otherwise, usual dist fields are filled in (dx and dy are
//   always zero coming back from this function)
//
//---------------------------------------------------------------

void look_around(struct dist &moves, bool justLooking, int first_move, int mode)
{
    int keyin = 0;
    bool dirChosen = false;
    bool targChosen = false;
    int dir = 0;
    int cx = VIEW_CX;
    int cy = VIEW_CY;
    int newcx, newcy;
    int mx, my;         // actual map x,y (scratch)
    int mid;            // monster id (scratch)
    FixedVector < char, 2 > monsfind_pos;
    FixedVector < char, 2 > objfind_pos;

    monsfind_pos[0] = objfind_pos[0] = you.x_pos;
    monsfind_pos[1] = objfind_pos[1] = you.y_pos;

    message_current_target();

    // setup initial keystroke
    if (first_move >= 0)
        keyin = (int)'1' + first_move;
    if (moves.prev_target == -1)
        keyin = '-';
    if (moves.prev_target == 1)
        keyin = '+';
    if (moves.prev_target == -2)
        keyin = '\'';
    if (moves.prev_target == -3)
        keyin = ';';
    // reset
    moves.prev_target = 0;

    // loop until some exit criteria reached
    while(true)
    {
        dirChosen = false;
        targChosen = false;
        newcx = cx;
        newcy = cy;

        // move cursor to current position
        gotoxy(cx+1, cy);

        if (keyin == 0)
            keyin = getchm(KC_TARGETING);

        // [dshaligram] Classic Crawl behaviour was to use space to select
        // targets when targeting. The patch changed the meaning of space
        // from 'confirm' to 'cancel', which surprised some folks. I'm now
        // arbitrarily defining space as 'cancel' for look-around, and 
        // 'confirm' for targeting.
        if (!justLooking && keyin == ' ')
            keyin = '\r';

        // [dshaligram] Fudge: in targeting mode, '>' says shoot a missile
        // that lands here.
        if (!justLooking && keyin == '>')
            keyin = 'X';

        // DOS idiocy
        if (keyin == 0)
        {
            // get the extended key
            keyin = getchm(KC_TARGETING);

            // look for CR - change to '5' to indicate selection
            if (keyin == 13)
                keyin = 'S';

            if (strchr(DOSidiocy, keyin) == NULL)
                break;
            dirChosen = true;
            dir = (int)(strchr(DOSidiocy, keyin) - DOSidiocy);
        }
        else
        {
            if (strchr(dirchars, keyin) != NULL)
            {
                dirChosen = true;
                dir = (int)(strchr(dirchars, keyin) - dirchars) / 2;
            }
            else
            {
                // handle non-directional keys
                switch (keyin)
                {
#ifdef WIZARD
                    case 'F':
                        targChosen = true;
                        mx = you.x_pos + cx - VIEW_CX;
                        my = you.y_pos + cy - VIEW_CY;
                        if (!in_bounds(mx, my))
                            break;
                        mid = mgrd[mx][my];

                        if (mid == NON_MONSTER)
                            break;

                        mprf("Changing attitude of %s\n",
                                ptr_monam(&menv[mid], DESC_PLAIN));
                        menv[mid].attitude =
                            menv[mid].attitude == ATT_HOSTILE?
                                ATT_FRIENDLY
                              : ATT_HOSTILE;

                        describe_monsters( menv[ mid ].type, mid );
                        redraw_screen();
                        mesclr( true );
                        // describe the cell again.
                        describe_cell(view2gridX(cx), view2gridY(cy));
                        break;
#endif

                    case CONTROL('F'):
                        mode = (mode + 1) % TARG_NUM_MODES;
                        
                        snprintf( info, INFO_SIZE, "Targeting mode is now: %s",
                                  (mode == TARG_ANY)   ? "any" :
                                  (mode == TARG_ENEMY) ? "enemies" 
                                                       : "friends" );

                        mpr( info );
                        targChosen = true;
                        break;

                    case '^':
                    case '\t':
                    case '\\':
                    case '_':
                    case '<':
                    case '>':
                    {
                        if (find_square( cx, cy, objfind_pos, 1,
                                     find_feature, keyin, true,
                                     Options.target_los_first
                                        ? LOS_FLIPVH : LOS_ANY))
                        {
                            newcx = objfind_pos[0];
                            newcy = objfind_pos[1];
                        }
                        else
                            flush_input_buffer( FLUSH_ON_FAILURE );
                        targChosen = true;
                        break;
                    }
                    case ';':
                    case '/':
                    case '\'':
                    case '*':
                        {
                            dir = keyin == ';' || keyin == '/'? -1 : 1;
                            if (find_square( cx, cy, objfind_pos, dir,
                                     find_object, 0, true,
                                     Options.target_los_first
                                        ? (dir == 1? LOS_FLIPVH : LOS_FLIPHV)
                                        : LOS_ANY))

                            {
                                newcx = objfind_pos[0];
                                newcy = objfind_pos[1];
                            }
                            else
                                flush_input_buffer( FLUSH_ON_FAILURE );
                            targChosen = true;
                            break;
                        }

                    case '-':
                    case '+':
                    case '=':
                        {
                            dir = keyin == '-'? -1 : 1;
                            if (find_square( cx, cy, monsfind_pos, dir, 
                                     find_monster, mode, Options.target_wrap ))
                            {
                                newcx = monsfind_pos[0];
                                newcy = monsfind_pos[1];
                            }
                            else
                                flush_input_buffer( FLUSH_ON_FAILURE );
                            targChosen = true;
                            break;
                        }

                    case 't':
                    case 'p':
                        moves.prev_target = -1;
                        break;

                    case '?':
                        targChosen = true;
                        mx = you.x_pos + cx - VIEW_CX;
                        my = you.y_pos + cy - VIEW_CY;
                        if (!in_bounds(mx, my))
                            break;
                        mid = mgrd[mx][my];

                        if (mid == NON_MONSTER)
                            break;

#if (!DEBUG_DIAGNOSTICS)
                        if (!player_monster_visible( &menv[mid] ))
                            break;
#endif

                        describe_monsters( menv[ mid ].type, mid );
                        redraw_screen();
                        mesclr( true );
                        // describe the cell again.
                        describe_cell(view2gridX(cx), view2gridY(cy));
                        break;

                    case '\r':
                    case '\n':
                    case '.':
                    case '5':
                    case 'X':
                        // If we're in look-around mode, and the cursor is on
                        // the character and there's a valid travel target 
                        // within the viewport, jump to that target.
                        if (justLooking && cx == VIEW_CX && cy == VIEW_CY)
                        {
                            if (you.travel_x > 0 && you.travel_y > 0)
                            {
                                int nx = grid2viewX(you.travel_x);
                                int ny = grid2viewY(you.travel_y);
                                if (in_viewport_bounds(nx, ny))
                                {
                                    newcx = nx;
                                    newcy = ny;
                                    targChosen = true;
                                }
                            }
                        }
                        else
                        {
                            dirChosen = true;
                            dir = keyin == 'X'? -1 : 4;
                        }
                        break;

                    case ' ':
                    case ESCAPE:
                        moves.isCancel = true;
                        mesclr( true );
                        return;

                    default:
                        break;
                }
            }
        }

        // now we have parsed the input character completely. Reset & Evaluate:
        keyin = 0;
        if (!targChosen && !dirChosen)
            break;

        // check for SELECTION
        if (dirChosen && (dir == 4 || dir == -1))
        {
            // [dshaligram] We no longer vet the square coordinates if
            // we're justLooking. By not vetting the coordinates, we make 'x'
            // look_around() nicer for travel purposes.
            if (!justLooking || !in_bounds(view2gridX(cx), view2gridY(cy)))
            {
                // RULE: cannot target what you cannot see
                if (!in_vlos(cx, cy))
                {
                    // if (!justLooking)
                    mpr("Sorry, you can't target what you can't see.");
                    return;
                }
            }

            moves.isValid = true;
            moves.isTarget = true;
            moves.isEndpoint = (dir == -1);
            moves.tx = you.x_pos + cx - VIEW_CX;
            moves.ty = you.y_pos + cy - VIEW_CY;

            if (moves.tx == you.x_pos && moves.ty == you.y_pos)
                moves.isMe = true;
            else
            {
                // try to set you.previous target
                mx = you.x_pos + cx - VIEW_CX;
                my = you.y_pos + cy - VIEW_CY;
                mid = mgrd[mx][my];

                if (mid == NON_MONSTER)
                    break;

                if (!player_monster_visible( &(menv[mid]) ))
                    break;

                you.prev_targ = mid;
            }
            break;
        }

        // check for MOVE
        if (dirChosen)
        {
            newcx = cx + xcomp[dir];
            newcy = cy + ycomp[dir];
        }

        // bounds check for newcx, newcy
        if (newcx < VIEW_SX) newcx = VIEW_SX;
        if (newcx > VIEW_EX) newcx = VIEW_EX;
        if (newcy < VIEW_SY) newcy = VIEW_SY;
        if (newcy > VIEW_EY) newcy = VIEW_EY;

        // no-op if the cursor doesn't move.
        if (newcx == cx && newcy == cy)
            continue;

        // CURSOR MOVED - describe new cell.
        cx = newcx;
        cy = newcy;
        mesclr( true );

        const int gridX = view2gridX(cx),
                  gridY = view2gridY(cy);

        if (!in_vlos(cx, cy))
        {
            mpr("You can't see that place.");
            describe_oos_square(gridX, gridY);
            continue;
        }
        
        if (in_bounds(gridX, gridY))
            describe_cell(gridX, gridY);
    } // end WHILE

    mesclr( true );
}                               // end look_around()

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
#ifdef STASH_TRACKING
                || (Options.target_oos && is_mapped(x, y) && is_stash(x, y))
#endif
                    ));
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
            bool vis = (env.show[xps - 8][yps] 
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
        return ("A gateway to hell.");
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
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_TEMPLE:
        return ("A staircase back to the Dungeon.");
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_SWAMP:
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

static void describe_cell(int mx, int my)
{
    char  str_pass[ ITEMNAME_SIZE ];
    bool  mimic_item = false;

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

        strcpy(info, ptr_monam( &(menv[i]), DESC_CAP_A ));
        strcat(info, ".");
        mpr(info);

        if (menv[i].type != MONS_DANCING_WEAPON && mon_wep != NON_ITEM)
        {
            snprintf( info, INFO_SIZE, "%s is wielding ", mons_pronoun( menv[i].type, 
                                                           PRONOUN_CAP ));
            it_name(mon_wep, DESC_NOCAP_A, str_pass);
            strcat(info, str_pass);

            // 2-headed ogres can wield 2 weapons
            if ((menv[i].type == MONS_TWO_HEADED_OGRE 
                    || menv[i].type == MONS_ETTIN)
                && menv[i].inv[MSLOT_MISSILE] != NON_ITEM)
            {
                strcat( info, " and " );
                it_name(menv[i].inv[MSLOT_MISSILE], DESC_NOCAP_A, str_pass);
                strcat(info, str_pass);
                strcat(info, ".");

                mpr(info);
            }
            else
            {
                strcat(info, ".");
                mpr(info);
            }
        }

        if (mon_arm != NON_ITEM)
        {
            it_name( mon_arm, DESC_NOCAP_A, str_pass );
            snprintf( info, INFO_SIZE, "%s is wearing %s.", 
                      mons_pronoun( menv[i].type, PRONOUN_CAP ),
                      str_pass );

            mpr( info );
        }


        if (menv[i].type == MONS_HYDRA)
        {
            snprintf( info, INFO_SIZE, "It has %d head%s.", 
                    menv[i].number, (menv[i].number > 1? "s" : "") );
            mpr( info );
        }

        print_wounds(&menv[i]);


        if (mons_is_mimic( menv[i].type ))
            mimic_item = true;
        else if (!mons_class_flag(menv[i].type, M_NO_EXP_GAIN)
                 && !mons_is_statue(menv[i].type))
        {
            if (menv[i].behaviour == BEH_SLEEP)
            {
                strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
                strcat(info, " doesn't appear to have noticed you.");
                mpr(info);
            }
            // Applies to both friendlies and hostiles
            else if (menv[i].behaviour == BEH_FLEE)
            {
                strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
                strcat(info, " is retreating.");
                mpr(info);
            }
            // hostile with target != you
            else if (!mons_friendly(&menv[i]) && menv[i].foe != MHITYOU)
            {
                // special case: batty monsters get set to BEH_WANDER as
                // part of their special behaviour.
                if (!testbits(menv[i].flags, MF_BATTY))
                {
                    strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
                    strcat(info, " doesn't appear to be interested in you.");
                    mpr(info);
                }
            }
        }

        if (menv[i].attitude == ATT_FRIENDLY)
        {
            strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
            strcat(info, " is friendly.");
            mpr(info);
        }

        for (int p = 0; p < NUM_MON_ENCHANTS; p++)
        {
            strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
            switch (menv[i].enchantment[p])
            {
            case ENCH_YOUR_ROT_I:
            case ENCH_YOUR_ROT_II:
            case ENCH_YOUR_ROT_III:
            case ENCH_YOUR_ROT_IV:
                strcat(info, " is rotting away."); //jmf: "covered in sores"?
                break;
            case ENCH_BACKLIGHT_I:
            case ENCH_BACKLIGHT_II:
            case ENCH_BACKLIGHT_III:
            case ENCH_BACKLIGHT_IV:
                strcat(info, " is softly glowing.");
                break;
            case ENCH_SLOW:
                strcat(info, " is moving slowly.");
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
            case ENCH_YOUR_STICKY_FLAME_I:
            case ENCH_YOUR_STICKY_FLAME_II:
            case ENCH_YOUR_STICKY_FLAME_III:
            case ENCH_YOUR_STICKY_FLAME_IV:
            case ENCH_STICKY_FLAME_I:
            case ENCH_STICKY_FLAME_II:
            case ENCH_STICKY_FLAME_III:
            case ENCH_STICKY_FLAME_IV:
                strcat(info, " is covered in liquid flames.");
                break;
            default:
                info[0] = 0;
                break;
            } // end switch
            if (info[0])
                mpr(info);
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
        const char cloud_inspected = env.cgrid[mx][my];

        const char cloud_type = env.cloud[ cloud_inspected ].type;

        strcpy(info, "There is a cloud of ");
        strcat(info,
            (cloud_type == CLOUD_FIRE
              || cloud_type == CLOUD_FIRE_MON) ? "flame" :
            (cloud_type == CLOUD_STINK
              || cloud_type == CLOUD_STINK_MON) ? "noxious fumes" :
            (cloud_type == CLOUD_COLD
              || cloud_type == CLOUD_COLD_MON) ? "freezing vapour" :
            (cloud_type == CLOUD_POISON
              || cloud_type == CLOUD_POISON_MON) ? "poison gases" :
            (cloud_type == CLOUD_GREY_SMOKE
              || cloud_type == CLOUD_GREY_SMOKE_MON) ? "grey smoke" :
            (cloud_type == CLOUD_BLUE_SMOKE
              || cloud_type == CLOUD_BLUE_SMOKE_MON) ? "blue smoke" :
            (cloud_type == CLOUD_PURP_SMOKE
              || cloud_type == CLOUD_PURP_SMOKE_MON) ? "purple smoke" :
            (cloud_type == CLOUD_STEAM
              || cloud_type == CLOUD_STEAM_MON) ? "steam" :
            (cloud_type == CLOUD_MIASMA
              || cloud_type == CLOUD_MIASMA_MON) ? "foul pestilence" :
            (cloud_type == CLOUD_BLACK_SMOKE
              || cloud_type == CLOUD_BLACK_SMOKE_MON) ? "black smoke"
                                                      : "buggy goodness");
        strcat(info, " here.");
        mpr(info);
    }

    int targ_item = igrd[ mx ][ my ];

    if (targ_item != NON_ITEM)
    {
        // If a mimic is on this square, we pretend its the first item -- bwr
        if (mimic_item)
            mpr("There is something else lying underneath.");
        else
        {
            if (mitm[ targ_item ].base_type == OBJ_GOLD)
            {
                mpr( "A pile of gold coins." );
            }
            else
            {
                strcpy(info, "You see ");
                it_name( targ_item, DESC_NOCAP_A, str_pass);
                strcat(info, str_pass);
                strcat(info, " here.");
                mpr(info);
            }

            if (mitm[ targ_item ].link != NON_ITEM)
                mpr("There is something else lying underneath.");
        }
    }

    std::string feature_desc = feature_description(mx, my);
    mpr(feature_desc.c_str());
}

/*
 *  File:       direct.cc
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
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
#include "view.h"

#ifdef USE_MACROS
#include "macro.h"
#endif

// x and y offsets in the following order:
// SW, S, SE, W, E, NW, N, NE
static const char xcomp[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
static const char ycomp[9] = { 1, 1, 1, 0, 0, 0, -1, -1, -1 };
static const char dirchars[19] = { "b1j2n3h4.5l6y7k8u9" };
static const char DOSidiocy[10] = { "OPQKSMGHI" };
static const char *aim_prompt = "Aim (move cursor or -/+/=, change mode with CTRL-F, select with . or >)";

static void describe_cell(int mx, int my);
static char mons_find( unsigned char xps, unsigned char yps, 
                       FixedVector<char, 2> &mfp, char direction, 
                       int mode = TARG_ANY );

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
void direction( struct dist &moves, int restrict, int mode )
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
    gotoxy( 18, 9 );

    int keyin = getch();

    if (keyin == 0)             // DOS idiocy (emulated by win32 console)
    {
        keyin = getch();        // grrr.
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
    int cx = 17;
    int cy = 9;
    int newcx, newcy;
    int mx, my;         // actual map x,y (scratch)
    int mid;            // monster id (scratch)
    FixedVector < char, 2 > monsfind_pos;

    monsfind_pos[0] = you.x_pos;
    monsfind_pos[1] = you.y_pos;

    message_current_target();

    // setup initial keystroke
    if (first_move >= 0)
        keyin = (int)'1' + first_move;
    if (moves.prev_target == -1)
        keyin = '-';
    if (moves.prev_target == 1)
        keyin = '+';
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
            keyin = getch();

        // DOS idiocy
        if (keyin == 0)
        {
            // get the extended key
            keyin = getch();

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
                    case CONTROL('F'):
                        mode = (mode + 1) % TARG_NUM_MODES;
                        
                        snprintf( info, INFO_SIZE, "Targeting mode is now: %s",
                                  (mode == TARG_ANY)   ? "any" :
                                  (mode == TARG_ENEMY) ? "enemies" 
                                                       : "friends" );

                        mpr( info );
                        targChosen = true;
                        break;

                    case '-':
                        if (mons_find( cx, cy, monsfind_pos, -1, mode ) == 0)
                            flush_input_buffer( FLUSH_ON_FAILURE );
                        else
                        {
                            newcx = monsfind_pos[0];
                            newcy = monsfind_pos[1];
                        }
                        targChosen = true;
                        break;

                    case '+':
                    case '=':
                        if (mons_find( cx, cy, monsfind_pos, 1, mode ) == 0)
                            flush_input_buffer( FLUSH_ON_FAILURE );
                        else
                        {
                            newcx = monsfind_pos[0];
                            newcy = monsfind_pos[1];
                        }
                        targChosen = true;
                        break;

                    case 't':
                    case 'p':
                        moves.prev_target = -1;
                        break;

                    case '?':
                        targChosen = true;
                        mx = you.x_pos + cx - 17;
                        my = you.y_pos + cy - 9;
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
                        describe_cell(you.x_pos + cx - 17, you.y_pos + cy - 9);
                        break;

                    case '\r':
                    case '\n':
                    case '>':
                    case ' ':
                    case '.':
                        dirChosen = true;
                        dir = 4;
                        break;

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
        if (dirChosen && dir == 4)
        {
            // RULE: cannot target what you cannot see
            if (env.show[cx - 8][cy] == 0 && !(cx == 17 && cy == 9))
            {
                if (!justLooking)
                    mpr("Sorry, you can't target what you can't see.");
                return;
            }

            moves.isValid = true;
            moves.isTarget = true;
            moves.tx = you.x_pos + cx - 17;
            moves.ty = you.y_pos + cy - 9;

            if (moves.tx == you.x_pos && moves.ty == you.y_pos)
                moves.isMe = true;
            else
            {
                // try to set you.previous target
                mx = you.x_pos + cx - 17;
                my = you.y_pos + cy - 9;
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
        if (newcx < 9)  newcx = 9;
        if (newcx > 25) newcx = 25;
        if (newcy < 1)  newcy = 1;
        if (newcy > 17) newcy = 17;

        // no-op if the cursor doesn't move.
        if (newcx == cx && newcy == cy)
            continue;

        // CURSOR MOVED - describe new cell.
        cx = newcx;
        cy = newcy;
        mesclr( true );
        if (env.show[cx - 8][cy] == 0 && !(cx == 17 && cy == 9))
        {
            mpr("You can't see that place.");
            continue;
        }
        describe_cell(you.x_pos + cx - 17, you.y_pos + cy - 9);
    } // end WHILE

    mesclr( true );
}                               // end look_around()

//---------------------------------------------------------------
//
// mons_find
//
// Finds the next monster (moving in a spiral outwards from the
// player, so closer monsters are chosen first; starts to player's
// left) and puts its coordinates in mfp. Returns 1 if it found
// a monster, zero otherwise. If direction is -1, goes backwards.
//
//---------------------------------------------------------------
static char mons_find( unsigned char xps, unsigned char yps,
                       FixedVector<char, 2> &mfp, char direction, int mode )
{
    unsigned char temp_xps = xps;
    unsigned char temp_yps = yps;
    char x_change = 0;
    char y_change = 0;

    int i, j;

    if (direction == 1 && temp_xps == 9 && temp_yps == 17)
        return (0);               // end of spiral

    while (temp_xps >= 8 && temp_xps <= 25 && temp_yps <= 17) // yps always >= 0
    {
        if (direction == -1 && temp_xps == 17 && temp_yps == 9)
            return (0);           // can't go backwards from you

        if (direction == 1)
        {
            if (temp_xps == 8)
            {
                x_change = 0;
                y_change = -1;
            }
            else if (temp_xps - 17 == 0 && temp_yps - 9 == 0)
            {
                x_change = -1;
                y_change = 0;
            }
            else if (abs(temp_xps - 17) <= abs(temp_yps - 9))
            {
                if (temp_xps - 17 >= 0 && temp_yps - 9 <= 0)
                {
                    if (abs(temp_xps - 17) > abs(temp_yps - 9 + 1))
                    {
                        x_change = 0;
                        y_change = -1;
                        if (temp_xps - 17 > 0)
                            y_change = 1;
                        goto finished_spiralling;
                    }
                }
                x_change = -1;
                if (temp_yps - 9 < 0)
                    x_change = 1;
                y_change = 0;
            }
            else
            {
                x_change = 0;
                y_change = -1;
                if (temp_xps - 17 > 0)
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

                    if (temp_xps + i == 8)
                    {
                        x_change = 0;
                        y_change = -1;
                    }
                    else if (temp_xps + i - 17 == 0 && temp_yps + j - 9 == 0)
                    {
                        x_change = -1;
                        y_change = 0;
                    }
                    else if (abs(temp_xps + i - 17) <= abs(temp_yps + j - 9))
                    {
                        const int xi = temp_xps + i - 17;
                        const int yj = temp_yps + j - 9;

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
                        if (temp_xps + i - 17 > 0)
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
        if (temp_yps + y_change <= 17)  // it can wrap, unfortunately
            temp_yps += y_change;

        const int targ_x = you.x_pos + temp_xps - 17;
        const int targ_y = you.y_pos + temp_yps - 9;

        // We don't want to be looking outside the bounds of the arrays:
        if (temp_xps > 25 || temp_xps < 8 || temp_yps > 17 || temp_yps < 1)
            continue;

        if (targ_x < 0 || targ_x >= GXM || targ_y < 0 || targ_y >= GYM)
            continue;

        const int targ_mon = mgrd[ targ_x ][ targ_y ];

        if (targ_mon != NON_MONSTER 
            && env.show[temp_xps - 8][temp_yps] != 0
            && player_monster_visible( &(menv[targ_mon]) )
            && !mons_is_mimic( menv[targ_mon].type )
            && (mode == TARG_ANY
                || (mode == TARG_FRIEND && mons_friendly( &menv[targ_mon] ))
                || (mode == TARG_ENEMY && !mons_friendly( &menv[targ_mon] ))))
        {
            //mpr("Found something!");
            //more();
            mfp[0] = temp_xps;
            mfp[1] = temp_yps;
            return (1);
        }
    }

    return (0);
}

static void describe_cell(int mx, int my)
{
    int   trf;            // used for trap type??
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
            it_name( mon_arm, DESC_PLAIN, str_pass );
            snprintf( info, INFO_SIZE, "%s is wearing %s.", 
                      mons_pronoun( menv[i].type, PRONOUN_CAP ),
                      str_pass );

            mpr( info );
        }


        if (menv[i].type == MONS_HYDRA)
        {
            snprintf( info, INFO_SIZE, "It has %d heads.", menv[i].number );
            mpr( info );
        }

        print_wounds(&menv[i]);


        if (mons_is_mimic( menv[i].type ))
            mimic_item = true;
        else if (!mons_flag(menv[i].type, M_NO_EXP_GAIN))
        {
            if (menv[i].behaviour == BEH_SLEEP)
            {
                strcpy(info, mons_pronoun(menv[i].type, PRONOUN_CAP));
                strcat(info, " doesn't appear to have noticed you.");
                mpr(info);
            }
            // wandering hostile with no target in LOS
            else if (menv[i].behaviour == BEH_WANDER && !mons_friendly(&menv[i])
                    && menv[i].foe == MHITNOT)
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
                info[0] = '\0';
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

    switch (grd[mx][my])
    {
    case DNGN_STONE_WALL:
        mpr("A stone wall.");
        break;
    case DNGN_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        if (you.level_type == LEVEL_PANDEMONIUM)
            mpr("A wall of the weird stuff which makes up Pandemonium.");
        else
            mpr("A rock wall.");
        break;
    case DNGN_PERMAROCK_WALL:
        mpr("An unnaturally hard rock wall.");
        break;    
    case DNGN_CLOSED_DOOR:
        mpr("A closed door.");
        break;
    case DNGN_METAL_WALL:
        mpr("A metal wall.");
        break;
    case DNGN_GREEN_CRYSTAL_WALL:
        mpr("A wall of green crystal.");
        break;
    case DNGN_ORCISH_IDOL:
        mpr("An orcish idol.");
        break;
    case DNGN_WAX_WALL:
        mpr("A wall of solid wax.");
        break;
    case DNGN_SILVER_STATUE:
        mpr("A silver statue.");
        break;
    case DNGN_GRANITE_STATUE:
        mpr("A granite statue.");
        break;
    case DNGN_ORANGE_CRYSTAL_STATUE:
        mpr("An orange crystal statue.");
        break;
    case DNGN_LAVA:
        mpr("Some lava.");
        break;
    case DNGN_DEEP_WATER:
        mpr("Some deep water.");
        break;
    case DNGN_SHALLOW_WATER:
        mpr("Some shallow water.");
        break;
    case DNGN_UNDISCOVERED_TRAP:
    case DNGN_FLOOR:
        mpr("Floor.");
        break;
    case DNGN_OPEN_DOOR:
        mpr("An open door.");
        break;
    case DNGN_ROCK_STAIRS_DOWN:
        mpr("A rock staircase leading down.");
        break;
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        mpr("A stone staircase leading down.");
        break;
    case DNGN_ROCK_STAIRS_UP:
        mpr("A rock staircase leading upwards.");
        break;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        mpr("A stone staircase leading up.");
        break;
    case DNGN_ENTER_HELL:
        mpr("A gateway to hell.");
        break;
    case DNGN_BRANCH_STAIRS:
        mpr("A staircase to a branch level.");
        break;
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
            mpr("A dart trap.");
            break;
        case TRAP_ARROW:
            mpr("An arrow trap.");
            break;
        case TRAP_SPEAR:
            mpr("A spear trap.");
            break;
        case TRAP_AXE:
            mpr("An axe trap.");
            break;
        case TRAP_TELEPORT:
            mpr("A teleportation trap.");
            break;
        case TRAP_AMNESIA:
            mpr("An amnesia trap.");
            break;
        case TRAP_BLADE:
            mpr("A blade trap.");
            break;
        case TRAP_BOLT:
            mpr("A bolt trap.");
            break;
        case TRAP_ZOT:
            mpr("A Zot trap.");
            break;
        case TRAP_NEEDLE:
            mpr("A needle trap.");
            break;
        default:
            mpr("An undefined trap. Huh?");
            error_message_to_player();
            break;
        }
        break;
    case DNGN_ENTER_SHOP:
        mpr(shop_name(mx, my));
        break;
    case DNGN_ENTER_LABYRINTH:
        mpr("A labyrinth entrance.");
        break;
    case DNGN_ENTER_DIS:
        mpr("A gateway to the Iron City of Dis.");
        break;
    case DNGN_ENTER_GEHENNA:
        mpr("A gateway to Gehenna.");
        break;
    case DNGN_ENTER_COCYTUS:
        mpr("A gateway to the freezing wastes of Cocytus.");
        break;
    case DNGN_ENTER_TARTARUS:
        mpr("A gateway to the decaying netherworld of Tartarus.");
        break;
    case DNGN_ENTER_ABYSS:
        mpr("A gateway to the infinite Abyss.");
        break;
    case DNGN_EXIT_ABYSS:
        mpr("A gateway leading out of the Abyss.");
        break;
    case DNGN_STONE_ARCH:
        mpr("An empty arch of ancient stone.");
        break;
    case DNGN_ENTER_PANDEMONIUM:
        mpr("A gate leading to the halls of Pandemonium.");
        break;
    case DNGN_EXIT_PANDEMONIUM:
        mpr("A gate leading out of Pandemonium.");
        break;
    case DNGN_TRANSIT_PANDEMONIUM:
        mpr("A gate leading to another region of Pandemonium.");
        break;
    case DNGN_ENTER_ORCISH_MINES:
        mpr("A staircase to the Orcish Mines.");
        break;
    case DNGN_ENTER_HIVE:
        mpr("A staircase to the Hive.");
        break;
    case DNGN_ENTER_LAIR:
        mpr("A staircase to the Lair.");
        break;
    case DNGN_ENTER_SLIME_PITS:
        mpr("A staircase to the Slime Pits.");
        break;
    case DNGN_ENTER_VAULTS:
        mpr("A staircase to the Vaults.");
        break;
    case DNGN_ENTER_CRYPT:
        mpr("A staircase to the Crypt.");
        break;
    case DNGN_ENTER_HALL_OF_BLADES:
        mpr("A staircase to the Hall of Blades.");
        break;
    case DNGN_ENTER_ZOT:
        mpr("A gate to the Realm of Zot.");
        break;
    case DNGN_ENTER_TEMPLE:
        mpr("A staircase to the Ecumenical Temple.");
        break;
    case DNGN_ENTER_SNAKE_PIT:
        mpr("A staircase to the Snake Pit.");
        break;
    case DNGN_ENTER_ELVEN_HALLS:
        mpr("A staircase to the Elven Halls.");
        break;
    case DNGN_ENTER_TOMB:
        mpr("A staircase to the Tomb.");
        break;
    case DNGN_ENTER_SWAMP:
        mpr("A staircase to the Swamp.");
        break;
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_TEMPLE:
        mpr("A staircase back to the Dungeon.");
        break;
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_SWAMP:
        mpr("A staircase back to the Lair.");
        break;
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
        mpr("A staircase back to the Vaults.");
        break;
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        mpr("A staircase back to the Mines.");
        break;
    case DNGN_RETURN_FROM_TOMB:
        mpr("A staircase back to the Crypt.");
        break;
    case DNGN_RETURN_FROM_ZOT:
        mpr("A gate leading back out of this place.");
        break;
    case DNGN_ALTAR_ZIN:
        mpr("A glowing white marble altar of Zin.");
        break;
    case DNGN_ALTAR_SHINING_ONE:
        mpr("A glowing golden altar of the Shining One.");
        break;
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        mpr("An ancient bone altar of Kikubaaqudgha.");
        break;
    case DNGN_ALTAR_YREDELEMNUL:
        mpr("A basalt altar of Yredelemnul.");
        break;
    case DNGN_ALTAR_XOM:
        mpr("A shimmering altar of Xom.");
        break;
    case DNGN_ALTAR_VEHUMET:
        mpr("A shining altar of Vehumet.");
        break;
    case DNGN_ALTAR_OKAWARU:
        mpr("An iron altar of Okawaru.");
        break;
    case DNGN_ALTAR_MAKHLEB:
        mpr("A burning altar of Makhleb.");
        break;
    case DNGN_ALTAR_SIF_MUNA:
        mpr("A deep blue altar of Sif Muna.");
        break;
    case DNGN_ALTAR_TROG:
        mpr("A bloodstained altar of Trog.");
        break;
    case DNGN_ALTAR_NEMELEX_XOBEH:
        mpr("A sparkling altar of Nemelex Xobeh.");
        break;
    case DNGN_ALTAR_ELYVILON:
        mpr("A silver altar of Elyvilon.");
        break;
    case DNGN_BLUE_FOUNTAIN:
        mpr("A fountain of clear blue water.");
        break;
    case DNGN_SPARKLING_FOUNTAIN:
        mpr("A fountain of sparkling water.");
        break;
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        mpr("A dry fountain.");
        break;
    }
}

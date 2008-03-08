/*
 *  File:       guic-win.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_X11 //Sys dep
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlocale.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xmd.h>
#endif

#include "cio.h"
#include "enum.h"
#include "externs.h"
#include "guic.h"
#include "libutil.h"

/*
 * Tile related stuff
 */
#ifdef USE_TILE

#include "tiles.h"

static Display *display=NULL;
static int screen;

static int x11_keypress(XKeyEvent *xev);
static void x11_check_exposure(XEvent *xev);

extern WinClass *win_main;

void GetNextEvent(int *etype, int *key, bool *shift, bool *ctrl,
    int *x1, int *y1, int *x2, int *y2)
{
    XEvent xev;

    while(1)
    {
        XNextEvent(display, &xev);

        if(xev.type==KeyPress)
	{
	    *etype = EV_KEYIN;
	    *key = x11_keypress(&(xev.xkey));
	    break;
	}
        else if(xev.type==Expose) 
	{
	    x11_check_exposure(&xev);
	}
	else if(xev.type == ConfigureNotify)
	{
	    win_main->ox = xev.xconfigure.x;
	    win_main->oy = xev.xconfigure.y;
	    break;
	}
	else if(xev.type==ButtonPress)
        {
	    *etype = EV_BUTTON;
            int button = xev.xbutton.button;
            *shift = (xev.xkey.state & ShiftMask)   ? true : false;
            *ctrl  = (xev.xkey.state & ControlMask) ? true : false;
            *x1 = xev.xbutton.x;
            *y1 = xev.xbutton.y;

            if (button == 3) button = 2;
            else if (button==2) button=3;
            *key = button;
	    break;
        }
	else if(xev.type==MotionNotify || xev.type==EnterNotify)
        {
	    *etype = EV_MOVE;
            *x1 = xev.xbutton.x;
            *y1 = xev.xbutton.y;
	    break;
        }
	else if (xev.type==LeaveNotify)
	{
	    *etype = EV_MOVE;
	    *x1 = -100;
	    *y1 = -100;
	    break;
	}
	else if(xev.type==ButtonRelease)
        {
	    *etype = EV_UNBUTTON;
            int button = xev.xbutton.button;
            if (button == 3) button = 2;
            else if (button==2) button=3;

            *x1 = xev.xbutton.x;
            *y1 = xev.xbutton.y;
            *key = button;
            break;
	}
    }/*while*/
}

char *my_getenv(const char *envname, const char *def)
{
    const char *result = getenv(envname);
    if (!result) result = def;
    return (char *)result;
}

int my_getenv_int(const char *envname, int def)
{
    const char *rstr = getenv(envname);
    if (!rstr) return def;
    return atoi(rstr);
}

extern WinClass *win_main;
extern TextRegionClass *region_tip;
extern TextRegionClass *region_crt;
void update_tip_text(const char *tip)
{
    const unsigned int tip_size = 512;
    static char old_tip[tip_size];
    char new_tip[tip_size];

    if (strncmp(old_tip, tip, tip_size) == 0)
        return;

    const bool is_main_screen = (win_main->active_layer == 0);
    const unsigned int height = is_main_screen ? region_tip->my : 1;
    const unsigned int width = is_main_screen ? region_tip->mx : region_crt->mx;

    strncpy(new_tip, tip, tip_size);
    strncpy(old_tip, new_tip, tip_size);

    if (is_main_screen)
    {
        region_tip->clear();
    }

    char *next_tip = new_tip;
    for (unsigned int i = 0; next_tip && i < height; i++)
    {
        char *this_tip = next_tip;

        // find end of line
        char *pc = strchr(next_tip, '\n');
        if (pc)
        {
            *pc = 0;
            next_tip = pc + 1;
        }
        else
        {
            next_tip = 0;
        }

        if (strlen(this_tip) > width)
        {
            // try to remove inscriptions
            char *ins = strchr(this_tip, '{');
            if (ins && ins[-1] == ' ')
                ins--;
            if (ins && (ins - this_tip <= (int)width))
            {
                *ins = 0;
            }
            else
            {
                // try to remove state, e.g. "(worn)"
                char *state = strchr(this_tip, '(');
                if (state && state[-1] == ' ')
                    state--;
                if (state && (state - this_tip <= (int)width))
                {
                    *state = 0;
                }
                else
                {
                    // if nothing else...
                    this_tip[width] = 0;
                    this_tip[width-1] = '.';
                    this_tip[width-2] = '.';
                }
            }
        }

        if (is_main_screen)
        {
            region_tip->cgotoxy(1, 1 + i);
            region_tip->addstr(this_tip);
            region_tip->make_active();
        }
        else
        {
            ASSERT(i == 0);
            textattr(WHITE);
            cgotoxy(1, get_number_of_lines() + 1);
            cprintf("%s", this_tip);
            clear_to_end_of_line();
        }
    }
}

void TileDrawDungeonAux()
{
}

#endif  /* USE_TILE */

/* X11 */
void x11_check_exposure(XEvent *xev){
    int sx, sy, ex, ey;

    sx = xev->xexpose.x;
    ex = xev->xexpose.x + (xev->xexpose.width)-1;
    sy = xev->xexpose.y;
    ey = xev->xexpose.y + (xev->xexpose.height)-1;

    if (xev->xany.window != win_main->win) return;

    win_main->redraw(sx,sy,ex,ey);
}

/* X11 */
// This routine is taken from Angband main-x11.c
int x11_keypress(XKeyEvent *xev)
{

#define IsSpecialKey(keysym) \
  ((unsigned)(keysym) >= 0xFF00)

    const unsigned int ck_table[9]=
    {
	CK_END,  CK_DOWN,   CK_PGDN,
	CK_LEFT, CK_INSERT, CK_RIGHT,
	CK_HOME, CK_UP,     CK_PGUP
    };

    int dir, base;

    int n;
    bool mc, ms, ma;
    unsigned int ks1;

    XKeyEvent *ev = (XKeyEvent*)(xev);
    KeySym ks;
    char buf[256];

    n = XLookupString(ev, buf, 125, &ks, NULL);
    buf[n] = '\0';

    if (IsModifierKey(ks)) return 0;

    /* Extract "modifier flags" */
    mc = (ev->state & ControlMask) ? true : false;
    ms = (ev->state & ShiftMask) ? true : false;
    ma = (ev->state & Mod1Mask) ? true : false;

    /* Normal keys */
    if (n &&  !IsSpecialKey(ks))
    {
        buf[n] = 0;

	//Hack Ctrl+[0-9] etc.
	if (mc && ((ks>='0' && ks<='9') || buf[0]>=' '))
	{
	    return 1024|ks;
	}

	if (!ma)
        {   
	    return buf[0];
	}

	/* Alt + ? */
	return 2048|buf[0];
    }

    /* Hack -- convert into an unsigned int */
    ks1 = (uint)(ks);

    /* Handle a few standard keys (bypass modifiers) XXX XXX XXX */
    base = dir = 0;
    switch (ks1)
        {
                case XK_Escape:
                    base = 0x1b;
                    break;
                case XK_Return:
                    base = '\r';
                    break;
                case XK_Tab:
                    base = '\t';
                    break;
                case XK_Delete:
                case XK_BackSpace:
                    base = '\010';
                    break;

		// for menus
                case XK_Down:
		    return CK_DOWN;
                case XK_Up:
		    return CK_UP;
                case XK_Left:
		    return CK_LEFT;
                case XK_Right:
		    return CK_RIGHT;

		/*
		 * Keypad
		 */

                case XK_KP_1:
                case XK_KP_End:
		    dir = 1;
		    break;

                case XK_KP_2:
                case XK_KP_Down:
                    dir = 2;
                    break;

                case XK_KP_3:
                case XK_KP_Page_Down:
                    dir = 3;
                    break;

                case XK_KP_6:
                case XK_KP_Right:
                    dir = 6;
                    break;

                case XK_KP_9:
                case XK_KP_Page_Up:
                    dir = 9;
                    break;

                case XK_KP_8:
                case XK_KP_Up:
                    dir = 8;
                    break;

                case XK_KP_7:
                case XK_KP_Home:
                    dir = 7;
                    break;

                case XK_KP_4:
                case XK_KP_Left:
                    dir = 4;
                    break;

                case XK_KP_5:
                    dir = 5;
                    break;
    }/* switch */

    //Handle keypad first
    if (dir != 0)
    {
	int result = ck_table[dir-1];

	if (ms) result += CK_SHIFT_UP - CK_UP;
	if (mc) result += CK_CTRL_UP - CK_UP;
	return result;
    }

    if (base != 0)
    {
	if (ms) base |= 1024;
	if (mc) base |= 2048;
	if (ma) base |= 4096;
	return base;
    }
    //Hack Special key
    if (ks1 >=0xff00)
    {
	base = 512 + ks1 - 0xff00;
	if (ms) base |= 1024;
	if (mc) base |= 2048;
	if (ma) base |= 4096;
	return base;
    }

    return 0;
}

void libgui_init_sys()
{
    GuicInit(&display, &screen);
}

void libgui_shutdown_sys()
{
    GuicDeinit();
}

void update_screen()
{
    XFlush(display);
}

int kbhit()
{
    XEvent xev;

    if (XCheckMaskEvent(display, 
        KeyPressMask | ButtonPressMask, &xev))
    {
        XPutBackEvent(display, &xev);
        return 1;
    }
    return 0;
}

void delay( unsigned long time )
{
    usleep( time * 1000 );
}

/* Convert value to string */
int itoa(int value, char *strptr, int radix)
{
    unsigned int bitmask = 32768;
    int ctr = 0;
    int startflag = 0;

    if (radix == 10)
    {
        sprintf(strptr, "%i", value);
    }
    if (radix == 2)             /* int to "binary string" */
    {
        while (bitmask)
        {
            if (value & bitmask)
            {
                startflag = 1;
                sprintf(strptr + ctr, "1");
            }
            else
            {
                if (startflag)
                    sprintf(strptr + ctr, "0");
            }

            bitmask = bitmask >> 1;
            if (startflag)
                ctr++;
        }

        if (!startflag)         /* Special case if value == 0 */
            sprintf((strptr + ctr++), "0");

        strptr[ctr] = (char) NULL;
    }
    return (0);                /* Me? Fail? Nah. */
}


// Convert string to lowercase.
char *strlwr(char *str)
{
    unsigned int i;

    for (i = 0; i < strlen(str); i++)
        str[i] = tolower(str[i]);

    return (str);
}

int stricmp( const char *str1, const char *str2 )
{
    return (strcmp(str1, str2));
}

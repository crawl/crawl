/*
 *  File:       spl-util.h                                           *
 *  Summary:    data handlers for player-avilable spell list         *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Changelog(most recent first):                                    *
 *
 *  <3>     04oct2001     bwr     absorbed spells0.cc
 *  <2>     24jun2000     jmf     changed to use new data structure
 *  <1>     12jun2000     dlb     created after much thought         
 */

#include "AppHdr.h"
#include "spl-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "externs.h"

#include "direct.h"
#include "debug.h"
#include "stuff.h"
#include "itemname.h"
#include "macro.h"
#include "monstuff.h"
#include "player.h"
#include "spl-book.h"
#include "view.h"


#ifdef DOS
#include <conio.h>
#endif


static struct playerspell spelldata[] = {
#include "spl-data.h"
};

static int plyrspell_list[NUM_SPELLS];

#define PLYRSPELLDATASIZE (sizeof(spelldata)/sizeof(struct playerspell))

static struct playerspell *seekspell(int spellid);
static bool cloud_helper( int (*func) (int, int, int, int), int x, int y, 
                          int pow, int ctype );

/*
 *             BEGIN PUBLIC FUNCTIONS
 */

// all this does is merely refresh the internal spell list {dlb}:
void init_playerspells(void)
{
    unsigned int x = 0;

    for (x = 0; x < NUM_SPELLS; x++)
        plyrspell_list[x] = -1;

    // can only use up to PLYRSPELLDATASIZE _MINUS ONE_,  or the 
    // last entry tries to set plyrspell_list[SPELL_NO_SPELL] 
    // which corrupts the heap.
    for (x = 0; x < PLYRSPELLDATASIZE - 1; x++)
        plyrspell_list[spelldata[x].id] = x;

    for (x = 0; x < NUM_SPELLS; x++)
    {
        if (plyrspell_list[x] == -1)
            plyrspell_list[x] = plyrspell_list[SPELL_NO_SPELL];
    }

    return;                     // return value should not matter here {dlb}
};                              // end init_playerspells()

int get_spell_slot_by_letter( char letter )
{
    ASSERT( isalpha( letter ) );

    const int index = letter_to_index( letter );

    if (you.spell_letter_table[ index ] == -1)
        return (-1);

    return (you.spell_letter_table[index]);
}

int get_spell_by_letter( char letter )
{
    ASSERT( isalpha( letter ) );

    const int slot = get_spell_slot_by_letter( letter );

    return ((slot == -1) ? SPELL_NO_SPELL : you.spells[slot]);
}

bool add_spell_to_memory( int spell )
{
    int i, j;

    // first we find a slot in our head:
    for (i = 0; i < 25; i++)
    {
        if (you.spells[i] == SPELL_NO_SPELL)
            break;
    }

    you.spells[i] = spell;

    // now we find an available label:
    for (j = 0; j < 52; j++)
    {
        if (you.spell_letter_table[j] == -1)
            break;
    }

    you.spell_letter_table[j] = i;

    you.spell_no++;

    return (true);
}

bool del_spell_from_memory_by_slot( int slot )
{
    int j;

    you.spells[ slot ] = SPELL_NO_SPELL;

    for (j = 0; j < 52; j++)
    {
        if (you.spell_letter_table[j] == slot)
            you.spell_letter_table[j] = -1;

    }

    you.spell_no--;

    return (true);
}


int spell_hunger(int which_spell)
{
    int level = seekspell(which_spell)->level;

    switch (level)
    {
    case  1: return   50;
    case  2: return   95;
    case  3: return  160;
    case  4: return  250;
    case  5: return  350;
    case  6: return  550;
    case  7: return  700;
    case  8: return  850;
    case  9: return 1000;
    case 10: return 1000;
    case 11: return 1100;
    case 12: return 1250;
    case 13: return 1380;
    case 14: return 1500;
    case 15: return 1600;
    default: return 1600 + (20 * level);
    }
}                               // end spell_hunger();

// applied to spell misfires (more power = worse) and triggers
// for Xom acting (more power = more likely to grab his attention) {dlb}
int spell_mana(int which_spell)
{
    return (seekspell(which_spell)->level);
}

// applied in naughties (more difficult = higher level knowledge = worse)
// and triggers for Sif acting (same reasoning as above, just good) {dlb}
int spell_difficulty(int which_spell)
{
    return (seekspell(which_spell)->level);
}

int spell_levels_required( int which_spell )
{
    int levels = spell_difficulty( which_spell );

    if (which_spell == SPELL_DELAYED_FIREBALL
        && player_has_spell( SPELL_FIREBALL ))
    {
        levels -= spell_difficulty( SPELL_FIREBALL );
    }
    else if (which_spell == SPELL_FIREBALL
            && player_has_spell( SPELL_DELAYED_FIREBALL ))
    {
        levels = 0;
    }

    return (levels);
}

bool spell_typematch(int which_spell, unsigned int which_discipline)
{
    return (seekspell(which_spell)->disciplines & which_discipline);
}

//jmf: next two for simple bit handling
unsigned int spell_type(int spell)
{
    return (seekspell(spell)->disciplines);
}

int count_bits(unsigned int bits)
{
    unsigned int n;
    int c = 0;

    for (n = 1; n < INT_MAX; n <<= 1)
    {
        if (n & bits)
            c++;
    }

    return (c);
}

// this will probably be used often, so rather than use malloc/free
// (which may lead to memory fragmentation) I'll just use a static
// array of characters -- if/when the String changeover takes place,
// this will all shift, no doubt {dlb}
/*
  const char *spell_title( int which_spell )
  {
  static char this_title[41] = ""; // this is generous, to say the least {dlb}
  strncpy(this_title, seekspell(which_spell)->title, 41);
  // truncation better than overrun {dlb}
  return ( this_title );
  }         // end spell_title()
*/

const char *spell_title(int spell)      //jmf: ah the joys of driving ms. data
{
    return (seekspell(spell)->title);
}


// FUNCTION APPLICATORS: Idea from Juho Snellman <jsnell@lyseo.edu.ouka.fi>
//                       on the Roguelike News pages, Development section.
//                       <URL:http://www.skoardy.demon.co.uk/rlnews/>
// Here are some function applicators: sort of like brain-dead,
// home-grown iterators for the container "dungeon".

// Apply a function-pointer to all visible squares
// Returns summation of return values from passed in function.
int apply_area_visible( int (*func) (int, int, int, int), int power )
{
    int x, y;
    int rv = 0;

    //jmf: FIXME: randomly start from other quadrants, like raise_dead?
    for (x = you.x_pos - 8; x <= you.x_pos + 8; x++)
    {
        for (y = you.y_pos - 8; y <= you.y_pos + 8; y++)
        {
            if (see_grid(x, y))
                rv += func(x, y, power, 0);
        }
    }

    return (rv);
}                               // end apply_area_visible()

// Applies the effect to all nine squares around/including the target.
// Returns summation of return values from passed in function.
int apply_area_square( int (*func) (int, int, int, int), int cx, int cy, 
                        int power )
{
    int x, y;
    int rv = 0;

    for (x = cx - 1; x <= cx + 1; x++)
    {
        for (y = cy - 1; y <= cy + 1; y++)
        {
            rv += func(x, y, power, 0);
        }
    }

    return (rv);
}                               // end apply_area_square()


// Applies the effect to the eight squares beside the target.
// Returns summation of return values from passed in function.
int apply_area_around_square( int (*func) (int, int, int, int), 
                              int targ_x, int targ_y, int power)
{
    int x, y;
    int rv = 0;

    for (x = targ_x - 1; x <= targ_x + 1; x++)
    {
        for (y = targ_y - 1; y <= targ_y + 1; y++)
        {
            if (x == targ_x && y == targ_y)
                continue;
            else
                rv += func(x, y, power, 0);
        }
    }
    return (rv);
}                               // end apply_area_around_square()

// Effect up to max_targs monsters around a point, chosen randomly
// Return varies with the function called;  return values will be added up.
int apply_random_around_square( int (*func) (int, int, int, int),
                                int targ_x, int targ_y, 
                                bool hole_in_middle, int power, int max_targs )
{
    int rv = 0;

    if (max_targs <= 0)
        return 0;

    if (max_targs >= 9 && !hole_in_middle)
    {
        return (apply_area_square( func, targ_x, targ_y, power ));
    }

    if (max_targs >= 8 && hole_in_middle)
    {
        return (apply_area_around_square( func, targ_x, targ_y, power ));
    }

    FixedVector< coord_def, 8 > targs;
    int count = 0;

    for (int x = targ_x - 1; x <= targ_x + 1; x++)
    {
        for (int y = targ_y - 1; y <= targ_y + 1; y++)
        {
            if (hole_in_middle && (x == targ_x && y == targ_y)) 
                continue;

            if (mgrd[x][y] == NON_MONSTER 
                && !(x == you.x_pos && y == you.y_pos))
            {
                continue;
            }
                
            // Found target
            count++;

            // Slight differece here over the basic algorithm...
            //
            // For cases where the number of choices <= max_targs it's
            // obvious (all available choices will be selected).
            //
            // For choices > max_targs, here's a brief proof:
            //
            // Let m = max_targs, k = choices - max_targs, k > 0.
            //
            // Proof, by induction (over k):
            //
            // 1) Show n = m + 1 (k = 1) gives uniform distribution,
            //    P(new one not chosen) = 1 / (m + 1).
            //                                         m     1     1
            //    P(specific previous one replaced) = --- * --- = ---
            //                                        m+1    m    m+1
            //
            //    So the probablity is uniform (ie. any element has
            //    a 1/(m+1) chance of being in the unchosen slot).
            //
            // 2) Assume the distribution is uniform at n = m+k.
            //    (ie. the probablity that any of the found elements
            //     was chosen = m / (m+k) (the slots are symetric,
            //     so it's the sum of the probabilities of being in
            //     any of them)).
            //
            // 3) Show n = m + k + 1 gives a uniform distribution.
            //    P(new one chosen) = m / (m + k + 1)
            //    P(any specific previous choice remaining chosen)
            //    = [1 - P(swaped into m+k+1 position)] * P(prev. chosen)
            //              m      1       m
            //    = [ 1 - ----- * --- ] * ---
            //            m+k+1    m      m+k
            //
            //       m+k     m       m
            //    = ----- * ---  = -----
            //      m+k+1   m+k    m+k+1
            //
            // Therefore, it's uniform for n = m + k + 1.  QED
            //
            // The important thing to note in calculating the last
            // probability is that the chosen elements have already
            // passed tests which verify that they *don't* belong
            // in slots m+1...m+k, so the only positions an already
            // chosen element can end up in are it's original
            // position (in one of the chosen slots), or in the
            // new slot.
            //
            // The new item can, of course, be placed in any slot,
            // swapping the value there into the new slot... we
            // just don't care about the non-chosen slots enough
            // to store them, so it might look like the item
            // automatically takes the new slot when not chosen
            // (although, by symetry all the non-chosen slots are
            // the same... and similarly, by symetry, all chosen
            // slots are the same).
            //
            // Yes, that's a long comment for a short piece of
            // code, but I want people to have an understanding
            // of why this works (or at least make them wary about
            // changing it without proof and breaking this code). -- bwr

            // Accept the first max_targs choices, then when
            // new choices come up, replace one of the choices
            // at random, max_targs/count of the time (the rest
            // of the time it replaces an element in an unchosen
            // slot -- but we don't care about them).
            if (count <= max_targs)
            {
                targs[ count - 1 ].x = x;
                targs[ count - 1 ].y = y;
            }
            else if (random2( count ) < max_targs)
            {
                const int pick = random2( max_targs );  
                targs[ pick ].x = x;
                targs[ pick ].y = y;
            }
        }
    }

    const int targs_found = (count < max_targs) ? count : max_targs;

    if (targs_found)
    {
        // Used to divide the power up among the targets here, but
        // it's probably better to allow the full power through and
        // balance the called function. -- bwr
        for (int i = 0; i < targs_found; i++)
        {
            ASSERT( targs[i].x && targs[i].y );
            rv += func( targs[i].x, targs[i].y, power, 0 );
        }
    }

    return (rv);
}                               // end apply_random_around_square()

// apply func to one square of player's choice beside the player
int apply_one_neighbouring_square(int (*func) (int, int, int, int), int power)
{
    struct dist bmove;

    mpr("Which direction? [ESC to cancel]", MSGCH_PROMPT);
    direction( bmove, DIR_DIR, TARG_ENEMY );

    if (!bmove.isValid)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return (0);
    }

    int rv = func(you.x_pos + bmove.dx, you.y_pos + bmove.dy, power, 1);

    if (rv == 0)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (rv);
}                               // end apply_one_neighbouring_square()

int apply_area_within_radius( int (*func) (int, int, int, int),
                              int x, int y, int pow, int radius, int ctype )
{
    int ix, iy;
    int sq_radius = radius * radius;
    int sx, sy, ex, ey;       // start and end x, y - bounds checked
    int rv = 0;

    // begin x,y
    sx = x - radius;
    sy = y - radius;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;

    // end x,y
    ex = x + radius;
    ey = y + radius;
    if (ex > GXM) ex = GXM;
    if (ey > GYM) ey = GYM;

    for (ix = sx; ix < ex; ix++)
    {
        for (iy = sy; iy < ey; iy++)
        {
            if (distance(x, y, ix, iy) <= sq_radius)
                rv += func(ix, iy, pow, ctype);
        }
    }

    return (rv);
}                               // end apply_area_within_radius()

// apply_area_cloud:
// Try to make a realistic cloud by expanding from a point, filling empty
// floor tiles until we run out of material (passed in as number).
// We really need some sort of a queue structure, since ideally I'd like
// to do a (shallow) breadth-first-search of the dungeon floor.
// This ought to work okay for small clouds.
void apply_area_cloud( int (*func) (int, int, int, int), int x, int y,
                       int pow, int number, int ctype )
{
    int spread, clouds_left = number;
    int good_squares = 0, neighbours[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int dx = 1, dy = 1;
    bool x_first;

    if (clouds_left && cloud_helper(func, x, y, pow, ctype))
        clouds_left--;

    if (!clouds_left)
        return;

    if (coinflip())
        dx *= -1;
    if (coinflip())
        dy *= -1;

    x_first = coinflip();

    if (x_first)
    {
        if (clouds_left && cloud_helper(func, x + dx, y, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[0]++;
        }

        if (clouds_left && cloud_helper(func, x - dx, y, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[1]++;
        }

        if (clouds_left && cloud_helper(func, x, y + dy, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[2]++;
        }

        if (clouds_left && cloud_helper(func, x, y - dy, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[3]++;
        }
    }
    else
    {
        if (clouds_left && cloud_helper(func, x, y + dy, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[2]++;
        }

        if (clouds_left && cloud_helper(func, x, y - dy, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[3]++;
        }

        if (clouds_left && cloud_helper(func, x + dx, y, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[0]++;
        }

        if (clouds_left && cloud_helper(func, x - dx, y, pow, ctype))
        {
            clouds_left--;
            good_squares++;
            neighbours[1]++;
        }
    }

    // now diagonals; we could randomize dx & dy again here
    if (clouds_left && cloud_helper(func, x + dx, y + dy, pow, ctype))
    {
        clouds_left--;
        good_squares++;
        neighbours[4]++;
    }

    if (clouds_left && cloud_helper(func, x - dx, y + dy, pow, ctype))
    {
        clouds_left--;
        good_squares++;
        neighbours[5]++;
    }

    if (clouds_left && cloud_helper(func, x + dx, y - dy, pow, ctype))
    {
        clouds_left--;
        good_squares++;
        neighbours[6]++;
    }

    if (clouds_left && cloud_helper(func, x - dx, y - dy, pow, ctype))
    {
        clouds_left--;
        good_squares++;
        neighbours[7]++;
    }

    if (!(clouds_left && good_squares))
        return;

    for (int i = 0; i < 8 && clouds_left; i++)
    {
        if (neighbours[i] == 0)
            continue;

        spread = clouds_left / good_squares;
        clouds_left -= spread;
        good_squares--;

        switch (i)
        {
        case 0:
            apply_area_cloud(func, x + dx, y, pow, spread, ctype);
            break;
        case 1:
            apply_area_cloud(func, x - dx, y, pow, spread, ctype);
            break;
        case 2:
            apply_area_cloud(func, x, y + dy, pow, spread, ctype);
            break;
        case 3:
            apply_area_cloud(func, x, y - dy, pow, spread, ctype);
            break;
        case 4:
            apply_area_cloud(func, x + dx, y + dy, pow, spread, ctype);
            break;
        case 5:
            apply_area_cloud(func, x - dx, y + dy, pow, spread, ctype);
            break;
        case 6:
            apply_area_cloud(func, x + dx, y - dy, pow, spread, ctype);
            break;
        case 7:
            apply_area_cloud(func, x - dx, y - dy, pow, spread, ctype);
            break;
        }
    }
}                               // end apply_area_cloud()

char spell_direction( struct dist &spelld, struct bolt &pbolt, 
                      int restrict, int mode )
{
    if (restrict == DIR_TARGET)
        mpr( "Choose a target (+/- for next/prev monster)", MSGCH_PROMPT );
    else
        mpr( STD_DIRECTION_PROMPT, MSGCH_PROMPT );

    message_current_target();

    direction( spelld, restrict, mode );

    if (!spelld.isValid)
    {
        // check for user cancel
        canned_msg(MSG_SPELL_FIZZLES);
        return -1;
    }

    pbolt.target_x = spelld.tx;
    pbolt.target_y = spelld.ty;
    pbolt.source_x = you.x_pos;
    pbolt.source_y = you.y_pos;

    return 1;
}                               // end spell_direction()

const char *spelltype_name(unsigned int which_spelltype)
{
    static char bug_string[80];

    switch (which_spelltype)
    {
    case SPTYP_CONJURATION:
        return ("Conjuration");
    case SPTYP_ENCHANTMENT:
        return ("Enchantment");
    case SPTYP_FIRE:
        return ("Fire");
    case SPTYP_ICE:
        return ("Ice");
    case SPTYP_TRANSMIGRATION:
        return ("Transmigration");
    case SPTYP_NECROMANCY:
        return ("Necromancy");
    case SPTYP_HOLY:
        return ("Holy");
    case SPTYP_SUMMONING:
        return ("Summoning");
    case SPTYP_DIVINATION:
        return ("Divination");
    case SPTYP_TRANSLOCATION:
        return ("Translocation");
    case SPTYP_POISON:
        return ("Poison");
    case SPTYP_EARTH:
        return ("Earth");
    case SPTYP_AIR:
        return ("Air");
    default:
        snprintf( bug_string, sizeof(bug_string), 
                  "invalid(%d)", which_spelltype );

        return (bug_string);
    }
}                               // end spelltype_name()

int spell_type2skill(unsigned int spelltype)
{
    char buffer[80];

    switch (spelltype)
    {
    case SPTYP_CONJURATION:    return (SK_CONJURATIONS);
    case SPTYP_ENCHANTMENT:    return (SK_ENCHANTMENTS);
    case SPTYP_FIRE:           return (SK_FIRE_MAGIC);
    case SPTYP_ICE:            return (SK_ICE_MAGIC);
    case SPTYP_TRANSMIGRATION: return (SK_TRANSMIGRATION);
    case SPTYP_NECROMANCY:     return (SK_NECROMANCY);
    case SPTYP_SUMMONING:      return (SK_SUMMONINGS);
    case SPTYP_DIVINATION:     return (SK_DIVINATIONS);
    case SPTYP_TRANSLOCATION:  return (SK_TRANSLOCATIONS);
    case SPTYP_POISON:         return (SK_POISON_MAGIC);
    case SPTYP_EARTH:          return (SK_EARTH_MAGIC);
    case SPTYP_AIR:            return (SK_AIR_MAGIC);

    default:
    case SPTYP_HOLY:
        snprintf( buffer, sizeof(buffer), 
                  "spell_type2skill: called with spelltype %d", spelltype );

        mpr( buffer );
        return (-1);
    }
}                               // end spell_type2skill()

/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
 */

//jmf: simplified; moved init code to top function, init_playerspells()
static struct playerspell *seekspell(int spell)
{
    return (&spelldata[plyrspell_list[spell]]);
}

static bool cloud_helper( int (*func) (int, int, int, int), int x, int y, 
                          int pow, int ctype )
{
    if (grd[x][y] > DNGN_LAST_SOLID_TILE && env.cgrid[x][y] == EMPTY_CLOUD)
    {
        func(x, y, pow, ctype);
        return true;
    }

    return false;
}

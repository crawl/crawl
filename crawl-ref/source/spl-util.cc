/*
 *  File:       spl-util.h                                           *
 *  Summary:    data handlers for player-avilable spell list         *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
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

#include "beam.h"
#include "directn.h"
#include "debug.h"
#include "stuff.h"
#include "itemname.h"
#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "notes.h"
#include "player.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "terrain.h"
#include "view.h"


#ifdef DOS
#include <conio.h>
#endif

static struct spell_desc spelldata[] = {
#include "spl-data.h"
};

static int spell_list[NUM_SPELLS];

#define SPELLDATASIZE (sizeof(spelldata)/sizeof(struct spell_desc))

static struct spell_desc *_seekspell(spell_type spellid);
static bool cloud_helper(int (*func)(int, int, int, int, cloud_type,
                                     kill_category),
                         int x, int y, int pow, int spread_rate,
                         cloud_type ctype, kill_category );

/*
 *             BEGIN PUBLIC FUNCTIONS
 */

// all this does is merely refresh the internal spell list {dlb}:
void init_spell_descs(void)
{
    for (int i = 0; i < NUM_SPELLS; i++)
        spell_list[i] = -1;

    // can only use up to SPELLDATASIZE _MINUS ONE_, or the
    // last entry tries to set spell_list[SPELL_NO_SPELL] 
    // which corrupts the heap.
    for (unsigned int i = 0; i < SPELLDATASIZE - 1; i++)
        spell_list[spelldata[i].id] = i;
}                               // end init_spell_descs()

typedef std::map<std::string, spell_type> spell_name_map;
static spell_name_map spell_name_cache;

void init_spell_name_cache()
{
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type type = static_cast<spell_type>(i);

        if (!is_valid_spell(type))
            continue;
        
        const char *sptitle = spell_title(type);
        ASSERT(sptitle);
        const std::string spell_name = lowercase_string(sptitle);
        spell_name_cache[spell_name] = type;
    }
}

spell_type spell_by_name(std::string name, bool partial_match)
{
    if (name.empty())
        return (SPELL_NO_SPELL);
    lowercase(name);

    if (!partial_match)
    {
        spell_name_map::iterator i = spell_name_cache.find(name);

        if (i != spell_name_cache.end())
            return (i->second);

        return (SPELL_NO_SPELL);
    }

    int spellmatch = -1;
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type type = static_cast<spell_type>(i);
        if (!is_valid_spell(type))
            continue;
        
        const char *sptitle = spell_title(type);
        const std::string spell_name = lowercase_string(sptitle);

        if (spell_name.find(name) != std::string::npos)
            spellmatch = i;
    }

    return (spellmatch != -1? static_cast<spell_type>(spellmatch)
            : SPELL_NO_SPELL);
}

int get_spell_slot_by_letter( char letter )
{
    ASSERT( isalpha( letter ) );

    const int index = letter_to_index( letter );

    if (you.spell_letter_table[ index ] == -1)
        return (-1);

    return (you.spell_letter_table[index]);
}

spell_type get_spell_by_letter( char letter )
{
    ASSERT( isalpha( letter ) );

    const int slot = get_spell_slot_by_letter( letter );

    return ((slot == -1) ? SPELL_NO_SPELL : you.spells[slot]);
}

bool add_spell_to_memory( spell_type spell )
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

    take_note(Note(NOTE_LEARN_SPELL, spell));

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


int spell_hunger(spell_type which_spell)
{
    const int level = spell_difficulty(which_spell);

    const int basehunger[] = {
        50, 95, 160, 250, 350, 550, 700, 850, 1000
    };

    int hunger;

    if ( level < 10 && level > 0 )
        hunger = basehunger[level-1];
    else
        hunger = (basehunger[0] * level * level) / 4;

    hunger -= you.intel * you.skills[SK_SPELLCASTING];

    if ( hunger < 0 )
        hunger = 0;

    return hunger;
}

bool spell_needs_tracer(spell_type spell)
{
    return (_seekspell(spell)->ms_needs_tracer);
}

bool spell_needs_foe(spell_type spell)
{
    return (!_seekspell(spell)->ms_utility);
}

// applied to spell misfires (more power = worse) and triggers
// for Xom acting (more power = more likely to grab his attention) {dlb}
int spell_mana(spell_type which_spell)
{
    return (_seekspell(which_spell)->level);
}

// applied in naughties (more difficult = higher level knowledge = worse)
// and triggers for Sif acting (same reasoning as above, just good) {dlb}
int spell_difficulty(spell_type which_spell)
{
    return (_seekspell(which_spell)->level);
}

int spell_levels_required( spell_type which_spell )
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

unsigned int get_spell_flags( spell_type which_spell )
{
    return (_seekspell(which_spell)->flags);
}

const char *get_spell_target_prompt( spell_type which_spell )
{
    return (_seekspell(which_spell)->target_prompt);
}

bool spell_typematch(spell_type which_spell, unsigned int which_discipline)
{
    return (_seekspell(which_spell)->disciplines & which_discipline);
}

//jmf: next two for simple bit handling
unsigned int get_spell_disciplines(spell_type spell)
{
    return (_seekspell(spell)->disciplines);
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

const char *spell_title(spell_type spell)
{
    return (_seekspell(spell)->title);
}


// FUNCTION APPLICATORS: Idea from Juho Snellman <jsnell@lyseo.edu.ouka.fi>
//                       on the Roguelike News pages, Development section.
//                       <URL:http://www.skoardy.demon.co.uk/rlnews/>
// Here are some function applicators: sort of like brain-dead,
// home-grown iterators for the container "dungeon".

// Apply a function-pointer to all visible squares
// Returns summation of return values from passed in function.
int apply_area_visible( int (*func) (int, int, int, int), int power,
                        bool pass_through_trans)
{
    int x, y;
    int rv = 0;

    //jmf: FIXME: randomly start from other quadrants, like raise_dead?
    for (x = you.x_pos - 8; x <= you.x_pos + 8; x++)
    {
        for (y = you.y_pos - 8; y <= you.y_pos + 8; y++)
        {
            if ((pass_through_trans && see_grid(x, y))
                 || (!pass_through_trans && see_grid_no_trans(x, y)))
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

            // Slight difference here over the basic algorithm...
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
            // chosen element can end up in are its original
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
        canned_msg(MSG_OK);
        return (-1);
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
void apply_area_cloud( int (*func) (int, int, int, int, cloud_type,
                                    kill_category),
                       int x, int y,
                       int pow, int number, cloud_type ctype,
                       kill_category whose, int spread_rate )
{
    int spread, clouds_left = number;
    int good_squares = 0, neighbours[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int dx = 1, dy = 1;
    bool x_first;

    if (clouds_left && cloud_helper(func, x, y, pow, spread_rate,
                                    ctype, whose))
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
        if (clouds_left && cloud_helper(func, x + dx, y, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[0]++;
        }

        if (clouds_left && cloud_helper(func, x - dx, y, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[1]++;
        }

        if (clouds_left && cloud_helper(func, x, y + dy, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[2]++;
        }

        if (clouds_left && cloud_helper(func, x, y - dy, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[3]++;
        }
    }
    else
    {
        if (clouds_left && cloud_helper(func, x, y + dy, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[2]++;
        }

        if (clouds_left && cloud_helper(func, x, y - dy, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[3]++;
        }

        if (clouds_left && cloud_helper(func, x + dx, y, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[0]++;
        }

        if (clouds_left && cloud_helper(func, x - dx, y, pow, spread_rate,
                                        ctype, whose))
        {
            clouds_left--;
            good_squares++;
            neighbours[1]++;
        }
    }

    // now diagonals; we could randomize dx & dy again here
    if (clouds_left && cloud_helper(func, x + dx, y + dy, pow, spread_rate,
                                    ctype, whose))
    {
        clouds_left--;
        good_squares++;
        neighbours[4]++;
    }

    if (clouds_left && cloud_helper(func, x - dx, y + dy, pow, spread_rate,
                                    ctype, whose))
    {
        clouds_left--;
        good_squares++;
        neighbours[5]++;
    }

    if (clouds_left && cloud_helper(func, x + dx, y - dy, pow, spread_rate,
                                    ctype, whose))
    {
        clouds_left--;
        good_squares++;
        neighbours[6]++;
    }

    if (clouds_left && cloud_helper(func, x - dx, y - dy, pow, spread_rate,
                                    ctype, whose))
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
            apply_area_cloud(func, x + dx, y, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 1:
            apply_area_cloud(func, x - dx, y, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 2:
            apply_area_cloud(func, x, y + dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 3:
            apply_area_cloud(func, x, y - dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 4:
            apply_area_cloud(func, x + dx, y + dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 5:
            apply_area_cloud(func, x - dx, y + dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 6:
            apply_area_cloud(func, x + dx, y - dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        case 7:
            apply_area_cloud(func, x - dx, y - dy, pow, spread, ctype, whose,
                             spread_rate);
            break;
        }
    }
}                               // end apply_area_cloud()

// Select a spell direction and fill dist and pbolt appropriately.
// Return false if the user canceled, true otherwise.
bool spell_direction( dist &spelld, bolt &pbolt, 
                      targeting_type restrict, targ_mode_type mode,
                      bool needs_path, const char *prompt )
{
    if (restrict != DIR_DIR)
        message_current_target();

    direction( spelld, restrict, mode, -1, false, needs_path, prompt );

    if (!spelld.isValid)
    {
        // check for user cancel
        canned_msg(MSG_OK);
        return false;
    }

    pbolt.set_target(spelld);
    pbolt.source_x = you.x_pos;
    pbolt.source_y = you.y_pos;

    return true;
}                               // end spell_direction()

const char* spelltype_short_name( int which_spelltype )
{
    switch (which_spelltype)
    {
    case SPTYP_CONJURATION:
        return ("Conj");
    case SPTYP_ENCHANTMENT:
        return ("Ench");
    case SPTYP_FIRE:
        return ("Fire");
    case SPTYP_ICE:
        return ("Ice");
    case SPTYP_TRANSMIGRATION:
        return ("Tmgr");
    case SPTYP_NECROMANCY:
        return ("Necr");
    case SPTYP_HOLY:
        return ("Holy");
    case SPTYP_SUMMONING:
        return ("Summ");
    case SPTYP_DIVINATION:
        return ("Divn");
    case SPTYP_TRANSLOCATION:
        return ("Tloc");
    case SPTYP_POISON:
        return ("Pois");
    case SPTYP_EARTH:
        return ("Erth");
    case SPTYP_AIR:
        return ("Air");
    default:
        return "Bug";
    }
}

const char *spelltype_name(unsigned int which_spelltype)
{
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
        return "Buggy";
    }
}                               // end spelltype_name()

int spell_type2skill(unsigned int spelltype)
{
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
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "spell_type2skill: called with spelltype %u",
             spelltype );
#endif
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

//jmf: simplified; moved init code to top function, init_spell_descs()
static spell_desc *_seekspell(spell_type spell)
{
    const int index = spell_list[spell];
    ASSERT(index != -1);
    return (&spelldata[index]);
}

bool is_valid_spell(spell_type spell)
{
    return (spell < NUM_SPELLS && spell_list[spell] != -1);
}

static bool cloud_helper(int (*func)(int, int, int, int, cloud_type,
                                     kill_category),
                         int x, int y, int pow, int spread_rate,
                         cloud_type ctype, kill_category whose )
{
    if (!grid_is_solid(grd[x][y]) && env.cgrid[x][y] == EMPTY_CLOUD)
    {
        func(x, y, pow, spread_rate, ctype, whose);
        return true;
    }

    return false;
}

int spell_power_cap(spell_type spell)
{
    return _seekspell(spell)->power_cap;
}

/*
 *  File:       spl-util.cc                                          *
 *  Summary:    data handlers for player-avilable spell list         *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "spl-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include <algorithm>

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
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "terrain.h"
#include "view.h"


#ifdef DOS
#include <conio.h>
#endif

struct spell_desc
{
    int id;
    const char  *title;
    unsigned int disciplines; // bitfield
    unsigned int flags;       // bitfield
    unsigned int level;
    int power_cap;

    // At power 0, you get min_range. At power power_cap, you get max_range.
    int min_range;
    int max_range;

    // How much louder or quieter the spell is than the default.
    int noise_mod;

    const char  *target_prompt;

    // If a monster is casting this, does it need a tracer?
    bool         ms_needs_tracer;

    // The spell can be used no matter what the monster's foe is.
    bool         ms_utility;
};

static struct spell_desc spelldata[] = {
#include "spl-data.h"
};

static int spell_list[NUM_SPELLS];

#define SPELLDATASIZE (sizeof(spelldata)/sizeof(struct spell_desc))

static struct spell_desc *_seekspell(spell_type spellid);
static bool _cloud_helper(cloud_func func, const coord_def& where,
                          int pow, int spread_rate,
                          cloud_type ctype, kill_category whose,
                          killer_type killer);

//
//             BEGIN PUBLIC FUNCTIONS
//

// All this does is merely refresh the internal spell list {dlb}:
void init_spell_descs(void)
{
    for (int i = 0; i < NUM_SPELLS; i++)
        spell_list[i] = -1;

    for (unsigned int i = 0; i < SPELLDATASIZE; i++)
    {
        spell_desc &data = spelldata[i];

#if DEBUG
        if (data.id < SPELL_NO_SPELL || data.id >= NUM_SPELLS)
            end(1, false, "spell #%d has invalid id %d", i, data.id);

        if (data.title == NULL || strlen(data.title) == 0)
            end(1, false, "spell #%d, id %d has no name", i, data.id);

        if (data.level < 1 || data.level > 9)
            end(1, false, "spell '%s' has invalid level %d",
                data.title, data.level);

        if (data.min_range > data.max_range)
            end(1, false, "spell '%s' has min_range larger than max_range",
                data.title);

        if (data.flags & SPFLAG_TARGETING_MASK)
        {
            if (data.min_range <= -1 || data.max_range <= 0)
                end(1, false, "targeted/directed spell '%s' has invalid range",
                    data.title);
        }
#endif

        spell_list[data.id] = i;
    }
}

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
        {
            if (spell_name == name)
                return static_cast<spell_type>(i);

            spellmatch = i;
        }
    }

    return (spellmatch != -1 ? static_cast<spell_type>(spellmatch)
                             : SPELL_NO_SPELL);
}

spschool_flag_type school_by_name(std::string name)
{
   spschool_flag_type short_match, long_match;
   int                short_matches, long_matches;

   short_match   = long_match   = SPTYP_NONE;
   short_matches = long_matches = 0;

   lowercase(name);

   for (int i = 0; i <= SPTYP_RANDOM; i++)
   {
       spschool_flag_type type = (spschool_flag_type) (1 << i);

       std::string short_name = spelltype_short_name(type);
       std::string long_name  = spelltype_long_name(type);

       lowercase(short_name);
       lowercase(long_name);

       if (name == short_name)
           return type;
       if (name == long_name)
           return type;

       if (short_name.find(name) != std::string::npos)
       {
           short_match = type;
           short_matches++;
       }
       if (long_name.find(name) != std::string::npos)
       {
           long_match = type;
           long_matches++;
       }
   }

   if (short_matches != 1 && long_matches != 1)
       return SPTYP_NONE;

   if (short_matches == 1 && long_matches != 1)
       return short_match;
   if (short_matches != 1 && long_matches == 1)
       return long_match;

   if (short_match == long_match)
       return short_match;

   return SPTYP_NONE;
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

// Used to determine whether or not a monster should always fire this spell
// if selected.  If not, we should use a tracer.

// Note - this function assumes that the monster is "nearby" its target!
bool spell_needs_tracer(spell_type spell)
{
    return (_seekspell(spell)->ms_needs_tracer);
}

// Checks if the spell is an explosion that can be placed anywhere even without
// an unobstructed beam path, such as fire storm.
bool spell_is_direct_explosion(spell_type spell)
{
    return (spell == SPELL_FIRE_STORM || spell == SPELL_HELLFIRE_BURST);
}

bool spell_needs_foe(spell_type spell)
{
    return (!_seekspell(spell)->ms_utility);
}

bool spell_harms_target(spell_type spell)
{
    const unsigned int flags = _seekspell(spell)->flags;

    if (flags & (SPFLAG_HELPFUL | SPFLAG_NEUTRAL))
        return false;

    if (flags & SPFLAG_TARGETING_MASK)
        return true;

    return false;
}

bool spell_harms_area(spell_type spell)
{
    const unsigned int flags = _seekspell(spell)->flags;

    if (flags & (SPFLAG_HELPFUL | SPFLAG_NEUTRAL))
        return false;

    if (flags & SPFLAG_AREA)
        return true;

    return false;
}

bool spell_sanctuary_castable(spell_type spell)
{
    return false;
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
        if (n & bits)
            c++;

    return (c);
}

// NOTE: Assumes that any single spell won't belong to conflicting
// disciplines.
bool disciplines_conflict(unsigned int disc1, unsigned int disc2)
{
    const unsigned int combined = disc1 | disc2;

    return (   (combined & SPTYP_EARTH) && (combined & SPTYP_AIR)
            || (combined & SPTYP_FIRE)  && (combined & SPTYP_ICE)
            || (combined & SPTYP_HOLY)  && (combined & SPTYP_NECROMANCY));
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
int apply_area_visible(cell_func cf, int power,
                       bool pass_through_trans, actor *agent)
{
    int rv = 0;

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        if (pass_through_trans || see_grid_no_trans(*ri))
            rv += cf(*ri, power, 0, agent);

    return (rv);
}

// Applies the effect to all nine squares around/including the target.
// Returns summation of return values from passed in function.
int apply_area_square(cell_func cf, const coord_def& where, int power,
                      actor *agent)
{
    int rv = 0;

    for (adjacent_iterator ai(where, false); ai; ++ai)
        rv += cf(*ai, power, 0, agent);

    return (rv);
}


// Applies the effect to the eight squares beside the target.
// Returns summation of return values from passed in function.
int apply_area_around_square(cell_func cf, const coord_def& where, int power,
                             actor *agent)
{
    int rv = 0;

    for (adjacent_iterator ai(where, true); ai; ++ai)
        rv += cf(*ai, power, 0, agent);

    return (rv);
}

// Affect up to max_targs monsters around a point, chosen randomly.
// Return varies with the function called; return values will be added up.
int apply_random_around_square(cell_func cf, const coord_def& where,
                               bool exclude_center, int power, int max_targs,
                               actor *agent)
{
    int rv = 0;

    if (max_targs <= 0)
        return 0;

    if (max_targs >= 9 && !exclude_center)
        return (apply_area_square(cf, where, power, agent));

    if (max_targs >= 8 && exclude_center)
        return (apply_area_around_square(cf, where, power, agent));

    coord_def targs[8];

    int count = 0;

    for (adjacent_iterator ai(where, exclude_center); ai; ++ai)
    {
        if (mgrd(*ai) == NON_MONSTER && *ai != you.pos())
            continue;

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
            targs[count - 1] = *ai;
        }
        else if (x_chance_in_y(max_targs, count))
        {
            const int pick = random2(max_targs);
            targs[ pick ] = *ai;
        }
    }

    const int targs_found = std::min(count, max_targs);

    if (targs_found)
    {
        // Used to divide the power up among the targets here, but
        // it's probably better to allow the full power through and
        // balance the called function. -- bwr
        for (int i = 0; i < targs_found; i++)
        {
            ASSERT(!targs[i].origin());
            rv += cf(targs[i], power, 0, agent);
        }
    }

    return (rv);
}

// Apply func to one square of player's choice beside the player.
int apply_one_neighbouring_square(cell_func cf, int power, actor *agent)
{
    dist bmove;

    mpr("Which direction? [ESC to cancel]", MSGCH_PROMPT);
    direction(bmove, DIR_DIR, TARG_ENEMY);

    if (!bmove.isValid)
    {
        canned_msg(MSG_OK);
        return (-1);
    }

    int rv = cf(you.pos() + bmove.delta, power, 1, agent);

    if (rv == 0)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (rv);
}

int apply_area_within_radius(cell_func cf, const coord_def& where,
                             int pow, int radius, int ctype,
                             actor *agent)
{

    int rv = 0;

    for (radius_iterator ri(where, radius, false, false); ri; ++ri)
        rv += cf(*ri, pow, ctype, agent);

    return (rv);
}

// apply_area_cloud:
// Try to make a realistic cloud by expanding from a point, filling empty
// floor tiles until we run out of material (passed in as number).
// We really need some sort of a queue structure, since ideally I'd like
// to do a (shallow) breadth-first-search of the dungeon floor.
// This ought to work okay for small clouds.
void apply_area_cloud( cloud_func func, const coord_def& where,
                       int pow, int number, cloud_type ctype,
                       kill_category whose, killer_type killer,
                       int spread_rate )
{
    int good_squares = 0;
    int neighbours[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    if (number && _cloud_helper(func, where, pow, spread_rate, ctype, whose,
                                killer))
        number--;

    if (number == 0)
        return;

    // These indices depend on the order in Compass (see acr.cc)
    int compass_order_orth[4] = { 2, 6, 4, 0 };
    int compass_order_diag[4] = { 1, 3, 5, 7 };

    int* const arrs[2] = { compass_order_orth, compass_order_diag };

    for ( int m = 0; m < 2; ++m )
    {
        // Randomise, but do orthogonals first and diagonals later.
        std::random_shuffle( arrs[m], arrs[m] + 4 );
        for ( int i = 0; i < 4 && number; ++i )
        {
            const int aux = arrs[m][i];
            if ( _cloud_helper(func, where + Compass[aux],
                               pow, spread_rate, ctype, whose, killer))
            {
                number--;
                good_squares++;
                neighbours[aux]++;
            }
        }
    }

    // Get a random permutation.
    int perm[8];
    for ( int i = 0; i < 8; ++i )
        perm[i] = i;
    std::random_shuffle(perm, perm+8);
    for (int i = 0; i < 8 && number; i++)
    {
        // Spread (in random order.)
        const int j = perm[i];

        if (neighbours[j] == 0)
            continue;

        int spread = number / good_squares;
        number -= spread;
        good_squares--;
        apply_area_cloud(func, where + Compass[j], pow, spread, ctype, whose,
                         killer, spread_rate);
    }
}

// Select a spell direction and fill dist and pbolt appropriately.
// Return false if the user canceled, true otherwise.
bool spell_direction( dist &spelld, bolt &pbolt,
                      targeting_type restrict, targ_mode_type mode,
                      int range,
                      bool needs_path, bool may_target_monster,
                      bool may_target_self, const char *prompt,
                      bool cancel_at_self )
{
    if (restrict != DIR_DIR)
        message_current_target();

    if (range < 1)
        range = (pbolt.range < 1) ? LOS_RADIUS : pbolt.range;

    direction( spelld, restrict, mode, range, false, needs_path,
               may_target_monster, may_target_self, prompt, NULL,
               cancel_at_self );

    if (!spelld.isValid)
    {
        // Check for user cancel.
        canned_msg(MSG_OK);
        return (false);
    }

    pbolt.set_target(spelld);
    pbolt.source = you.pos();

    return (true);
}

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
    case SPTYP_TRANSMUTATION:
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
    case SPTYP_RANDOM:
        return ("Rndm");
    default:
        return "Bug";
    }
}

const char* spelltype_long_name( int which_spelltype )
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
    case SPTYP_TRANSMUTATION:
        return ("Transmutation");
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
    case SPTYP_RANDOM:
        return ("Random");
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
    case SPTYP_TRANSMUTATION:
        return ("Transmutation");
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
    case SPTYP_TRANSMUTATION: return (SK_TRANSMUTATION);
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

int spell_skill2type(unsigned int skill)
{
    switch (skill)
    {
    case SK_CONJURATIONS:   return (SPTYP_CONJURATION);
    case SK_ENCHANTMENTS:   return (SPTYP_ENCHANTMENT);
    case SK_FIRE_MAGIC:     return (SPTYP_FIRE);
    case SK_ICE_MAGIC:      return (SPTYP_ICE);
    case SK_TRANSMUTATION: return (SPTYP_TRANSMUTATION);
    case SK_NECROMANCY:     return (SPTYP_NECROMANCY);
    case SK_SUMMONINGS:     return (SPTYP_SUMMONING);
    case SK_DIVINATIONS:    return (SPTYP_DIVINATION);
    case SK_TRANSLOCATIONS: return (SPTYP_TRANSLOCATION);
    case SK_POISON_MAGIC:   return (SPTYP_POISON);
    case SK_EARTH_MAGIC:    return (SPTYP_EARTH);
    case SK_AIR_MAGIC:      return (SPTYP_AIR);

    default:
    case SPTYP_HOLY:
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "spell_skill2type: called with skill %u",
             skill);
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

//jmf: Simplified; moved init code to top function, init_spell_descs().
static spell_desc *_seekspell(spell_type spell)
{
    ASSERT(spell >= 0 && spell < NUM_SPELLS);

    const int index = spell_list[spell];
    ASSERT(index != -1);

    return (&spelldata[index]);
}

bool is_valid_spell(spell_type spell)
{
    return (spell > SPELL_NO_SPELL && spell < NUM_SPELLS
            && spell_list[spell] != -1);
}

static bool _cloud_helper(cloud_func func, const coord_def& where,
                          int pow, int spread_rate,
                          cloud_type ctype, kill_category whose,
                          killer_type killer )
{
    if (!grid_is_solid(grd(where)) && env.cgrid(where) == EMPTY_CLOUD)
    {
        func(where, pow, spread_rate, ctype, whose, killer);
        return (true);
    }

    return (false);
}

int spell_power_cap(spell_type spell)
{
    return (_seekspell(spell)->power_cap);
}

// Sandblast range is 1 if not wielding rocks, 1-2 if you are.
// For targetting purposes, of course, be optimistic about range.
static int _sandblast_range(int pow, bool real_cast)
{
    int res = 1;

    if (wielding_rocks() && (!real_cast || coinflip()))
            res = 2;

    return (res);
}


int spell_range(spell_type spell, int pow, bool real_cast)
{
    const int minrange = _seekspell(spell)->min_range;
    const int maxrange = _seekspell(spell)->max_range;
    ASSERT(maxrange >= minrange);

    // Some cases need to be handled specially.
    if (spell == SPELL_SANDBLAST)
        return _sandblast_range(pow, real_cast);

    if (minrange == maxrange)
        return minrange;

    const int powercap = spell_power_cap(spell);

    if (powercap <= pow)
        return maxrange;

    // Round appropriately.
    return ((pow*(maxrange - minrange) + powercap/2) / powercap + minrange);
}

int spell_noise(spell_type spell)
{
    const spell_desc *desc = _seekspell(spell);

    return desc->noise_mod + spell_noise(desc->disciplines, desc->level);
}

int spell_noise(unsigned int disciplines, int level)
{
    if (disciplines == SPTYP_NONE)
        return (0);
    else if (disciplines & SPTYP_CONJURATION)
        return (level);
    else if (disciplines && !(disciplines & (SPTYP_POISON | SPTYP_AIR)))
        return div_round_up(level * 3, 4);
    else
        return div_round_up(level, 2);
}

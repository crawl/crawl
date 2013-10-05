/**
 * @file
 * @brief data handlers for player-available spell list
**/

#include "AppHdr.h"

#include "spl-util.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include <algorithm>

#include "externs.h"

#include "areas.h"
#include "beam.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "godabil.h"
#include "stuff.h"
#include "env.h"
#include "items.h"
#include "libutil.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-book.h"
#include "spl-damage.h"
#include "spl-zap.h"
#include "target.h"
#include "terrain.h"
#include "item_use.h"
#include "transform.h"


struct spell_desc
{
    int id;
    const char  *title;
    unsigned int disciplines; // bitfield
    unsigned int flags;       // bitfield
    unsigned int level;

    // Usually in the range 0..200 (0 means uncapped).
    // Note that some spells are also capped through zap_type.
    // See spell_power_cap below.
    int power_cap;

    // At power 0, you get min_range. At power power_cap, you get max_range.
    int min_range;
    int max_range;

    // Modify spell level for spell noise purposes.
    int noise_mod;

    const char  *target_prompt;

    // If a monster is casting this, does it need a tracer?
    bool         ms_needs_tracer;

    // The spell can be used no matter what the monster's foe is.
    bool         ms_utility;
};

static const struct spell_desc spelldata[] =
{
    #include "spl-data.h"
};

static int spell_list[NUM_SPELLS];

#define SPELLDATASIZE (sizeof(spelldata)/sizeof(struct spell_desc))

static const struct spell_desc *_seekspell(spell_type spellid);

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
        const spell_desc &data = spelldata[i];

        ASSERTM(data.id >= SPELL_NO_SPELL && data.id < NUM_SPELLS,
                "spell #%d has invalid id %d", i, data.id);

        ASSERTM(data.title != NULL && *data.title,
                "spell #%d, id %d has no name", i, data.id);

        ASSERTM(data.level >= 1 && data.level <= 9,
                "spell '%s' has invalid level %d", data.title, data.level);

        ASSERTM(data.min_range <= data.max_range,
                "spell '%s' has min_range larger than max_range", data.title);

        ASSERTM(!(data.flags & SPFLAG_TARGETTING_MASK)
                || (data.min_range >= 0 && data.max_range > 0),
                "targeted/directed spell '%s' has invalid range", data.title);

        spell_list[data.id] = i;
    }
}

typedef map<string, spell_type> spell_name_map;
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
        const string spell_name = lowercase_string(sptitle);
        spell_name_cache[spell_name] = type;
    }
}

spell_type spell_by_name(string name, bool partial_match)
{
    if (name.empty())
        return SPELL_NO_SPELL;

    lowercase(name);

    if (!partial_match)
    {
        spell_name_map::iterator i = spell_name_cache.find(name);

        if (i != spell_name_cache.end())
            return i->second;

        return SPELL_NO_SPELL;
    }

    spell_type spellmatch = SPELL_NO_SPELL;
    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type type = static_cast<spell_type>(i);
        if (!is_valid_spell(type))
            continue;

        const char *sptitle = spell_title(type);
        const string spell_name = lowercase_string(sptitle);

        if (spell_name.find(name) != string::npos)
        {
            if (spell_name == name)
                return type;

            spellmatch = type;
        }
    }

    return spellmatch;
}

spschool_flag_type school_by_name(string name)
{
    spschool_flag_type short_match, long_match;
    int                short_matches, long_matches;

    short_match   = long_match   = SPTYP_NONE;
    short_matches = long_matches = 0;

    lowercase(name);

    for (int i = 0; i <= SPTYP_RANDOM; i++)
    {
        spschool_flag_type type = (spschool_flag_type) (1 << i);

        string short_name = spelltype_short_name(type);
        string long_name  = spelltype_long_name(type);

        lowercase(short_name);
        lowercase(long_name);

        if (name == short_name)
            return type;
        if (name == long_name)
            return type;

        if (short_name.find(name) != string::npos)
        {
            short_match = type;
            short_matches++;
        }
        if (long_name.find(name) != string::npos)
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

int get_spell_slot_by_letter(char letter)
{
    ASSERT(isaalpha(letter));

    const int index = letter_to_index(letter);

    if (you.spell_letter_table[ index ] == -1)
        return -1;

    return (you.spell_letter_table[index]);
}

static int _get_spell_slot(spell_type spell)
{
    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
        if (you.spells[i] == spell)
            return i;

    return -1;
}

int get_spell_letter(spell_type spell)
{
    return index_to_letter(_get_spell_slot(spell));
}

spell_type get_spell_by_letter(char letter)
{
    ASSERT(isaalpha(letter));

    const int slot = get_spell_slot_by_letter(letter);

    return ((slot == -1) ? SPELL_NO_SPELL : you.spells[slot]);
}

bool add_spell_to_memory(spell_type spell)
{
    int i;
    int j = -1;
    string sname = spell_title(spell);
    lowercase(sname);
    // first we find a slot in our head:
    for (i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (you.spells[i] == SPELL_NO_SPELL)
            break;
    }

    you.spells[i] = spell;

    // now we find an available label:
    // first check to see whether we've chosen an automatic label:
    for (unsigned k = 0; k < Options.auto_spell_letters.size(); ++k)
    {
        if (!Options.auto_spell_letters[k].first.matches(sname))
            continue;
        for (unsigned l = 0; l < Options.auto_spell_letters[k].second.length(); ++l)
            if (isaalpha(Options.auto_spell_letters[k].second[l]) &&
                you.spell_letter_table[letter_to_index(Options.auto_spell_letters[k].second[l])] == -1)
            {
                j = letter_to_index(Options.auto_spell_letters[k].second[l]);
                mprf("Spell assigned to '%c'.",
                     Options.auto_spell_letters[k].second[l]);
                break;
            }
        if (j != -1)
            break;
    }
    // If we didn't find a label above, choose the first available one.
    if (j == -1)
        for (j = 0; j < 52; j++)
        {
            if (you.spell_letter_table[j] == -1)
                break;
        }

    you.spell_letter_table[j] = i;

    you.spell_no++;

    take_note(Note(NOTE_LEARN_SPELL, spell));

    spell_skills(spell, you.start_train);

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    return true;
}

bool del_spell_from_memory_by_slot(int slot)
{
    ASSERT_RANGE(slot, 0, MAX_KNOWN_SPELLS);
    int j;

    if (you.last_cast_spell == you.spells[slot])
        you.last_cast_spell = SPELL_NO_SPELL;

    spell_skills(you.spells[slot], you.stop_train);

    mprf("Your memory of %s unravels.", spell_title(you.spells[slot]));
    you.spells[ slot ] = SPELL_NO_SPELL;

    for (j = 0; j < 52; j++)
    {
        if (you.spell_letter_table[j] == slot)
            you.spell_letter_table[j] = -1;

    }

    you.spell_no--;

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    return true;
}

bool del_spell_from_memory(spell_type spell)
{
    int i = _get_spell_slot(spell);
    if (i == -1)
        return false;
    else
        return del_spell_from_memory_by_slot(i);
}

int spell_hunger(spell_type which_spell, bool rod)
{
    if (player_energy())
        return 0;

    const int level = spell_difficulty(which_spell);

    const int basehunger[] = { 50, 95, 160, 250, 350, 550, 700, 850, 1000 };

    int hunger;

    if (level < 10 && level > 0)
        hunger = basehunger[level-1];
    else
        hunger = (basehunger[0] * level * level) / 4;

    if (rod)
    {
        hunger -= you.skill(SK_EVOCATIONS, 10);
        hunger = max(hunger, level * 5);
    }
    else
        hunger -= you.skill(SK_SPELLCASTING, you.intel());

    if (hunger < 0)
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

    if (flags & SPFLAG_TARGETTING_MASK)
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

int spell_levels_required(spell_type which_spell)
{
    int levels = spell_difficulty(which_spell);

    if (which_spell == SPELL_DELAYED_FIREBALL
        && you.has_spell(SPELL_FIREBALL))
    {
        levels -= spell_difficulty(SPELL_FIREBALL);
    }
    else if (which_spell == SPELL_FIREBALL
            && you.has_spell(SPELL_DELAYED_FIREBALL))
    {
        levels = 0;
    }

    return levels;
}

unsigned int get_spell_flags(spell_type which_spell)
{
    return (_seekspell(which_spell)->flags);
}

const char *get_spell_target_prompt(spell_type which_spell)
{
    return (_seekspell(which_spell)->target_prompt);
}

bool spell_typematch(spell_type which_spell, unsigned int which_discipline)
{
    return (get_spell_disciplines(which_spell) & which_discipline);
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

    return c;
}

// NOTE: Assumes that any single spell won't belong to conflicting
// disciplines.
bool disciplines_conflict(unsigned int disc1, unsigned int disc2)
{
    const unsigned int combined = disc1 | disc2;

    return ((combined & SPTYP_EARTH) && (combined & SPTYP_AIR)
            || (combined & SPTYP_FIRE)  && (combined & SPTYP_ICE));
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
int apply_area_visible(cell_func cf, int power, actor *agent)
{
    int rv = 0;

    for (radius_iterator ri(agent->pos(), you.current_vision); ri; ++ri)
        if (agent->see_cell_no_trans(*ri))
            rv += cf(*ri, power, 0, agent);

    return rv;
}

// Applies the effect to all nine squares around/including the target.
// Returns summation of return values from passed in function.
static int _apply_area_square(cell_func cf, const coord_def& where,
                              int power, actor *agent)
{
    int rv = 0;

    for (adjacent_iterator ai(where, false); ai; ++ai)
        rv += cf(*ai, power, 0, agent);

    return rv;
}


// Applies the effect to the eight squares beside the target.
// Returns summation of return values from passed in function.
static int _apply_area_around_square(cell_func cf, const coord_def& where,
                                     int power, actor *agent)
{
    int rv = 0;

    for (adjacent_iterator ai(where, true); ai; ++ai)
        rv += cf(*ai, power, 0, agent);

    return rv;
}

// Like apply_area_around_square, but for monsters in those squares,
// and takes care not to affect monsters twice that change position.
int apply_monsters_around_square(monster_func mf, const coord_def& where,
                                  int power)
{
    int rv = 0;
    set<const monster*> affected;
    for (adjacent_iterator ai(where, true); ai; ++ai)
    {
        monster* mon = monster_at(*ai);
        if (mon && affected.find(mon) == affected.end())
        {
            rv += mf(mon, power);
            affected.insert(mon);
        }
    }

    return rv;
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
        return _apply_area_square(cf, where, power, agent);

    if (max_targs >= 8 && exclude_center)
        return _apply_area_around_square(cf, where, power, agent);

    coord_def targs[8];

    int count = 0;

    for (adjacent_iterator ai(where, exclude_center); ai; ++ai)
    {
        if (monster_at(*ai) == NULL && *ai != you.pos())
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
        //     was chosen = m / (m+k) (the slots are symmetric,
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
        // (although, by symmetry all the non-chosen slots are
        // the same... and similarly, by symmetry, all chosen
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
            targs[count - 1] = *ai;
        else if (x_chance_in_y(max_targs, count))
        {
            const int pick = random2(max_targs);
            targs[ pick ] = *ai;
        }
    }

    const int targs_found = min(count, max_targs);

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

    return rv;
}

void apply_area_cloud(cloud_func func, const coord_def& where,
                       int pow, int number, cloud_type ctype,
                       const actor *agent,
                       int spread_rate, int colour, string name,
                       string tile, int excl_rad)
{
    if (number <= 0)
        return;

    targetter_cloud place(0, 0, number, number);
    if (!place.set_aim(where))
        return;
    unsigned int dist = 0;
    while (number > 0)
    {
        while (place.queue[dist].empty())
            if (++dist >= place.queue.size())
                return;
        vector<coord_def> &q = place.queue[dist];
        int el = random2(q.size());
        coord_def c = q[el];
        q[el] = q[q.size() - 1];
        q.pop_back();

        if (place.seen[c] <= 0)
            continue;
        func(c, pow, spread_rate, ctype, agent, colour, name, tile, excl_rad);
        number--;
    }
}

// Select a spell direction and fill dist and pbolt appropriately.
// Return false if the user cancelled, true otherwise.
// FIXME: this should accept a direction_chooser_args directly rather
// than move the arguments into one.
bool spell_direction(dist &spelld, bolt &pbolt,
                      targetting_type restrict, targ_mode_type mode,
                      int range,
                      bool needs_path, bool may_target_monster,
                      bool may_target_self, const char *target_prefix,
                      const char* top_prompt, bool cancel_at_self,
                      targetter *hitfunc, desc_filter get_desc_func)
{
    if (range < 1)
        range = (pbolt.range < 1) ? you.current_vision : pbolt.range;

    direction_chooser_args args;
    args.restricts = restrict;
    args.mode = mode;
    args.range = range;
    args.just_looking = false;
    args.needs_path = needs_path;
    args.may_target_monster = may_target_monster;
    args.may_target_self = may_target_self;
    args.target_prefix = target_prefix;
    if (top_prompt)
        args.top_prompt = top_prompt;
    args.behaviour = NULL;
    args.cancel_at_self = cancel_at_self;
    args.hitfunc = hitfunc;
    args.get_desc_func = get_desc_func;

    direction(spelld, args);

    if (!spelld.isValid)
    {
        // Check for user cancel.
        canned_msg(MSG_OK);
        return false;
    }

    pbolt.set_target(spelld);
    pbolt.source = you.pos();

    return true;
}

const char* spelltype_short_name(int which_spelltype)
{
    switch (which_spelltype)
    {
    case SPTYP_CONJURATION:
        return "Conj";
    case SPTYP_HEXES:
        return "Hex";
    case SPTYP_CHARMS:
        return "Chrm";
    case SPTYP_FIRE:
        return "Fire";
    case SPTYP_ICE:
        return "Ice";
    case SPTYP_TRANSMUTATION:
        return "Trmt";
    case SPTYP_NECROMANCY:
        return "Necr";
    case SPTYP_SUMMONING:
        return "Summ";
    case SPTYP_DIVINATION:
        return "Divn";
    case SPTYP_TRANSLOCATION:
        return "Tloc";
    case SPTYP_POISON:
        return "Pois";
    case SPTYP_EARTH:
        return "Erth";
    case SPTYP_AIR:
        return "Air";
    case SPTYP_RANDOM:
        return "Rndm";
    default:
        return "Bug";
    }
}

const char* spelltype_long_name(int which_spelltype)
{
    switch (which_spelltype)
    {
    case SPTYP_CONJURATION:
        return "Conjuration";
    case SPTYP_HEXES:
        return "Hexes";
    case SPTYP_CHARMS:
        return "Charms";
    case SPTYP_FIRE:
        return "Fire";
    case SPTYP_ICE:
        return "Ice";
    case SPTYP_TRANSMUTATION:
        return "Transmutation";
    case SPTYP_NECROMANCY:
        return "Necromancy";
    case SPTYP_SUMMONING:
        return "Summoning";
    case SPTYP_DIVINATION:
        return "Divination";
    case SPTYP_TRANSLOCATION:
        return "Translocation";
    case SPTYP_POISON:
        return "Poison";
    case SPTYP_EARTH:
        return "Earth";
    case SPTYP_AIR:
        return "Air";
    case SPTYP_RANDOM:
        return "Random";
    default:
        return "Bug";
    }
}

skill_type spell_type2skill(unsigned int spelltype)
{
    switch (spelltype)
    {
    case SPTYP_CONJURATION:    return SK_CONJURATIONS;
    case SPTYP_HEXES:          return SK_HEXES;
    case SPTYP_CHARMS:         return SK_CHARMS;
    case SPTYP_FIRE:           return SK_FIRE_MAGIC;
    case SPTYP_ICE:            return SK_ICE_MAGIC;
    case SPTYP_TRANSMUTATION:  return SK_TRANSMUTATIONS;
    case SPTYP_NECROMANCY:     return SK_NECROMANCY;
    case SPTYP_SUMMONING:      return SK_SUMMONINGS;
    case SPTYP_TRANSLOCATION:  return SK_TRANSLOCATIONS;
    case SPTYP_POISON:         return SK_POISON_MAGIC;
    case SPTYP_EARTH:          return SK_EARTH_MAGIC;
    case SPTYP_AIR:            return SK_AIR_MAGIC;

    default:
    case SPTYP_DIVINATION:
        dprf("spell_type2skill: called with spelltype %u", spelltype);
        return SK_NONE;
    }
}

/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
 */

//jmf: Simplified; moved init code to top function, init_spell_descs().
static const spell_desc *_seekspell(spell_type spell)
{
    ASSERT_RANGE(spell, 0, NUM_SPELLS);
    const int index = spell_list[spell];
    ASSERT(index != -1);

    return &spelldata[index];
}

bool is_valid_spell(spell_type spell)
{
    return (spell > SPELL_NO_SPELL && spell < NUM_SPELLS
            && spell_list[spell] != -1);
}

static bool _spell_range_varies(spell_type spell)
{
    int minrange = _seekspell(spell)->min_range;
    int maxrange = _seekspell(spell)->max_range;

    return (minrange < maxrange);
}

int spell_power_cap(spell_type spell)
{
    const int scap = _seekspell(spell)->power_cap;
    const int zcap = spell_zap_power_cap(spell);

    if (scap == 0)
        return zcap;
    else if (zcap == 0)
        return scap;
    else
    {
        // Two separate power caps; pre-zapping spell power
        // goes into range.
        if (scap <= zcap || _spell_range_varies(spell))
            return scap;
        else
            return zcap;
    }
}

int spell_range(spell_type spell, int pow, bool player_spell)
{
    int minrange = _seekspell(spell)->min_range;
    int maxrange = _seekspell(spell)->max_range;
    ASSERT(maxrange >= minrange);

    // spells with no range have maxrange == minrange == -1
    if (maxrange < 0)
        return maxrange;

    // Sandblast is a special case.
    if (spell == SPELL_SANDBLAST && wielding_rocks())
    {
        minrange++;
        maxrange++;
    }

    if (player_spell
        && vehumet_supports_spell(spell)
        && you_worship(GOD_VEHUMET)
        && spell != SPELL_STICKY_FLAME
        && spell != SPELL_FREEZE
        && !player_under_penance()
        && you.piety >= piety_breakpoint(3))
    {
        maxrange++;
        minrange++;
    }

    if (minrange == maxrange)
        return min(minrange, (int)you.current_vision);

    const int powercap = spell_power_cap(spell);

    if (powercap <= pow)
        return min(maxrange, (int)you.current_vision);

    // Round appropriately.
    return min((int)you.current_vision,
           (pow * (maxrange - minrange) + powercap / 2) / powercap + minrange);
}

/**
 * Spell casting noise.
 *
 * Returns the noise generated by the casting of a spell. The noise depends on
 * the spell schools and level. A modifier (noise_mod) can be applied to the
 * spell level.
 * @see spl-data.h
 *
 * Formula (use first match):
 * - Conjuration (noisy)    = \f$ level \f$
 * - Air and poison (quiet) = \f$ \frac{level}{2} \f$
 * - Other (normal)         = \f$ \frac{3 \times level}{4} \f$
 *
 * \param spell  The spell being casted.
 * \return       The amount of noise generated.
**/
int spell_noise(spell_type spell)
{
    const spell_desc *desc = _seekspell(spell);
    unsigned int disciplines = desc->disciplines;
    int level = desc->level + desc->noise_mod;

    if (disciplines & SPTYP_CONJURATION)
        return level;
    else if (disciplines && !(disciplines & (SPTYP_POISON | SPTYP_AIR)))
        return div_round_up(level * 3, 4);
    else
        return div_round_up(level, 2);
}

spell_type zap_type_to_spell(zap_type zap)
{
    switch (zap)
    {
    case ZAP_THROW_FLAME:
        return SPELL_THROW_FLAME;
    case ZAP_THROW_FROST:
        return SPELL_THROW_FROST;
    case ZAP_SLOW:
        return SPELL_SLOW;
    case ZAP_HASTE:
        return SPELL_HASTE;
    case ZAP_MAGIC_DART:
        return SPELL_MAGIC_DART;
    case ZAP_MAJOR_HEALING:
        return SPELL_MAJOR_HEALING;
    case ZAP_PARALYSE:
        return SPELL_PARALYSE;
    case ZAP_BOLT_OF_FIRE:
        return SPELL_BOLT_OF_FIRE;
    case ZAP_BOLT_OF_COLD:
        return SPELL_BOLT_OF_COLD;
    case ZAP_PRIMAL_WAVE:
        return SPELL_PRIMAL_WAVE;
    case ZAP_CONFUSE:
        return SPELL_CONFUSE;
    case ZAP_INVISIBILITY:
        return SPELL_INVISIBILITY;
    case ZAP_DIG:
        return SPELL_DIG;
    case ZAP_FIREBALL:
        return SPELL_FIREBALL;
    case ZAP_TELEPORT_OTHER:
        return SPELL_TELEPORT_OTHER;
    case ZAP_LIGHTNING_BOLT:
        return SPELL_LIGHTNING_BOLT;
    case ZAP_POLYMORPH:
        return SPELL_POLYMORPH;
    case ZAP_BOLT_OF_DRAINING:
        return SPELL_BOLT_OF_DRAINING;
    case ZAP_ENSLAVEMENT:
        return SPELL_ENSLAVEMENT;
    case ZAP_DISINTEGRATE:
        return SPELL_DISINTEGRATE;
    default:
        die("zap_type_to_spell() only handles wand zaps for now");
    }
    return SPELL_NO_SPELL;
}

static bool _spell_is_empowered(spell_type spell)
{
    switch (spell)
    {
    case SPELL_STONESKIN:
        if (you.duration[DUR_TRANSFORMATION] > 0
            && you.form == TRAN_STATUE
            && !player_stoneskin())
        {
            return true;
        }
        break;
    case SPELL_OZOCUBUS_ARMOUR:
        if (you.duration[DUR_TRANSFORMATION] > 0
            && you.form == TRAN_ICE_BEAST
            && you.duration[DUR_ICY_ARMOUR] < 1)
        {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

// This function attempts to determine if 'spell' is useless to
// the player. if 'transient' is true, then it will include checks
// for volatile or temporary states (such as status effects, mana, etc.)
//
// its notably used by 'spell_highlight_by_utility'
bool spell_is_useless(spell_type spell, bool transient)
{
    if (you_cannot_memorise(spell))
        return true;

    if (transient)
    {
        if (you.duration[DUR_CONF] > 0
            || spell_mana(spell) > (you.species != SP_DJINNI ? you.magic_points
                                    : (you.hp - 1) / DJ_MP_RATE)
            || spell_no_hostile_in_range(spell))
        {
            return true;
        }

        if (you.species == SP_LAVA_ORC && !temperature_effect(LORC_STONESKIN))
        {
            switch (spell)
            {
            case SPELL_STATUE_FORM: // Stony self is too melty
            // Too hot for these ice spells:
            case SPELL_ICE_FORM:
            case SPELL_OZOCUBUS_ARMOUR:
            case SPELL_CONDENSATION_SHIELD:
                return true;
            default:
                break;
            }
        }
    }

    switch (spell)
    {
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_TELEPORT_SELF:
        if (you.no_tele(false, false, spell != SPELL_TELEPORT_SELF))
            return true;
        break;
    case SPELL_SWIFTNESS:
        if (transient && you.form == TRAN_TREE)
            return true;
        // looking at player_movement_speed, this should be correct ~DMB
        if (player_movement_speed() <= 6)
            return true;
        break;
    case SPELL_FLY:
        if (transient && you.form == TRAN_TREE)
            return true;
        if (you.racial_permanent_flight())
            return true;
        if (transient && you.permanent_flight())
            return true;
        break;
    case SPELL_INVISIBILITY:
        if (transient && you.backlit())
            return true;
        break;
    case SPELL_CONTROL_TELEPORT:
        // Can be cast in advance in most places with -cTele,
        // but useless once the orb is picked up.
        if (player_has_orb())
            return true;
        break;
    case SPELL_DARKNESS:
        // mere corona is not enough, but divine light blocks it completely
        if (transient && you.haloed())
            return true;
        if (you_worship(GOD_SHINING_ONE) && !player_under_penance())
            return true;
        break;
#if TAG_MAJOR_VERSION == 34
    case SPELL_INSULATION:
        if (player_res_electricity(false, transient, transient))
            return true;
        break;
#endif
    case SPELL_REPEL_MISSILES:
        if (player_mutation_level(MUT_DISTORTION_FIELD) == 3
            || !you.suppressed() && you.scan_artefacts(ARTP_RMSL, true))
        {
            return true;
        }
        break;

    case SPELL_STONESKIN:
        if (you.species == SP_LAVA_ORC)
            return true;
        break;

    case SPELL_LEDAS_LIQUEFACTION:
        if (!you.stand_on_solid_ground()
            || you.duration[DUR_LIQUEFYING]
            || liquefied(you.pos()))
        {
            return true;
        }
        break;

    case SPELL_DELAYED_FIREBALL:
        return transient && you.attribute[ATTR_DELAYED_FIREBALL];

    default:
        break; // quash unhandled constants warnings
    }

    return false;
}

// This function takes a spell, and determines what color it should be
// highlighted with. You shouldn't have to touch this unless you want
// to add new highlighting options.
int spell_highlight_by_utility(spell_type spell, int default_color,
                               bool transient, bool rod_spell)
{
    // If your god hates the spell, that
    // overrides all other concerns
    if (god_hates_spell(spell, you.religion, rod_spell))
        return COL_FORBIDDEN;

    if (_spell_is_empowered(spell) && !rod_spell)
        default_color = COL_EMPOWERED;

    if (spell_is_useless(spell, transient))
        default_color = COL_USELESS;

    return default_color;
}

bool spell_no_hostile_in_range(spell_type spell)
{
    int minRange = get_dist_to_nearest_monster();
    if (minRange < 0)
        return false;

    const int range = calc_spell_range(spell);
    if (range < 0)
        return false;

    switch (spell)
    {
    // These don't target monsters.
    case SPELL_APPORTATION:
    case SPELL_CONJURE_FLAME:
    case SPELL_DIG:
    case SPELL_PASSWALL:
    case SPELL_GOLUBRIAS_PASSAGE:
    case SPELL_LRD:
    case SPELL_FULMINANT_PRISM:

    // Shock and Lightning Bolt are no longer here, as the code below can
    // account for possible bounces.

    case SPELL_FIRE_STORM:
        return false;

    // Special handling for cloud spells.
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
    {
        targetter_cloud tgt(&you, range);
        for (radius_iterator ri(you.pos(), range, C_ROUND, you.get_los());
             ri; ++ri)
        {
            if (!tgt.valid_aim(*ri))
                continue;
            tgt.set_aim(*ri);
            for (map<coord_def, aff_type>::iterator it = tgt.seen.begin();
                 it != tgt.seen.end(); it++)
            {
                if (it->second == AFF_NO || it->second == AFF_TRACER)
                    continue;

                // Checks here are from get_dist_to_nearest_monster().
                const monster* mons = monster_at(it->first);
                if (mons && !mons->wont_attack()
                    && (!mons_class_flag(mons->type, M_NO_EXP_GAIN)
                        || (mons->type == MONS_BALLISTOMYCETE
                            && mons->number != 0)))
                    return false;
            }
        }

        return true;
    }
    default:
        break;
    }

    // The healing spells.
    if (testbits(get_spell_flags(spell), SPFLAG_HELPFUL))
        return false;

    bolt beam;
    beam.flavour = BEAM_VISUAL;

    zap_type zap = spell_to_zap(spell);
    if (spell == SPELL_FIREBALL)
        zap = ZAP_FIREBALL;

    if (zap != NUM_ZAPS)
    {
        beam.thrower = KILL_YOU_MISSILE;
        zappy(zap, calc_spell_power(spell, true), beam);
    }
    else if (spell == SPELL_MEPHITIC_CLOUD)
    {
        beam.flavour = BEAM_MEPHITIC;
        beam.ex_size = 1;
        beam.damage = dice_def(1, 1); // so that foe_info is populated
        beam.hit = 20;
        beam.thrower = KILL_YOU;
        beam.ench_power = calc_spell_power(spell, true);
        beam.is_beam = false;
        beam.is_explosion = true;
    }

    if (beam.flavour != BEAM_VISUAL)
    {
        bolt tempbeam;
        bool found = false;
        beam.beam_source = MHITYOU;
        beam.range = range;
        beam.is_tracer = true;
        beam.is_targetting = true;
        beam.source  = you.pos();
        beam.dont_stop_player = true;
        beam.friend_info.dont_stop = true;
        beam.foe_info.dont_stop = true;
        beam.attitude = ATT_FRIENDLY;
        beam.can_see_invis = you.can_see_invisible();
#ifdef DEBUG_DIAGNOSTICS
        beam.quiet_debug = true;
#endif
        for (radius_iterator ri(you.pos(), range, C_ROUND, you.get_los());
             ri; ++ri)
        {
            tempbeam = beam;
            tempbeam.target = *ri;
            tempbeam.fire();
            if (tempbeam.foe_info.count > 0)
            {
                found = true;
                break;
            }
        }
        return !found;
    }

    const int rsq = range * range + 1;
    if (rsq < minRange)
        return true;

    return false;
}

/**
 * @file
 * @brief data handlers for player-available spell list
**/

#include "AppHdr.h"

#include "spl-util.h"

#include <algorithm>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "areas.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "godabil.h"
#include "libutil.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "spl-zap.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
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

    // Noise made directly by casting this spell.
    // Noise used to be based directly on spell level:
    //  * for conjurations: spell level
    //  * for non-conj pois/air: spell level / 2 (rounded up)
    //  * for others: spell level * 3/4 (rounded up)
    // These are probably good guidelines for new spells.
    int noise;

    // Some spells have a noise at their place of effect, in addition
    // to at the place of casting. effect_noise handles that, and is also
    // used even if the spell is not casted directly (by Xom, for instance).
    int effect_noise;

    const char  *target_prompt;
};

#include "spl-data.h"

static int spell_list[NUM_SPELLS];

#define SPELLDATASIZE (sizeof(spelldata)/sizeof(struct spell_desc))

static const struct spell_desc *_seekspell(spell_type spellid);

//
//             BEGIN PUBLIC FUNCTIONS
//

// All this does is merely refresh the internal spell list {dlb}:
void init_spell_descs()
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

        ASSERTM(!(data.flags & SPFLAG_TARGETING_MASK)
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

    const spell_type sp = find_earliest_match(name, SPELL_NO_SPELL, NUM_SPELLS,
                                              is_valid_spell, spell_title);
    return sp == NUM_SPELLS ? SPELL_NO_SPELL : sp;
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

    return you.spell_letter_table[index];
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

    return (slot == -1) ? SPELL_NO_SPELL : you.spells[slot];
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

    if (you.num_turns)
        mprf("Spell assigned to '%c'.", index_to_letter(j));

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

    const int basehunger[] = { 50, 100, 150, 250, 400, 550, 700, 850, 1000 };

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

// Checks if the spell is an explosion that can be placed anywhere even without
// an unobstructed beam path, such as fire storm.
bool spell_is_direct_explosion(spell_type spell)
{
    return spell == SPELL_FIRE_STORM || spell == SPELL_HELLFIRE_BURST;
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

// applied to spell misfires (more power = worse) and triggers
// for Xom acting (more power = more likely to grab his attention) {dlb}
int spell_mana(spell_type which_spell)
{
    return _seekspell(which_spell)->level;
}

// applied in naughties (more difficult = higher level knowledge = worse)
// and triggers for Sif acting (same reasoning as above, just good) {dlb}
int spell_difficulty(spell_type which_spell)
{
    return _seekspell(which_spell)->level;
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
    return _seekspell(which_spell)->flags;
}

const char *get_spell_target_prompt(spell_type which_spell)
{
    return _seekspell(which_spell)->target_prompt;
}

bool spell_typematch(spell_type which_spell, unsigned int which_discipline)
{
    return get_spell_disciplines(which_spell) & which_discipline;
}

//jmf: next two for simple bit handling
unsigned int get_spell_disciplines(spell_type spell)
{
    return _seekspell(spell)->disciplines;
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

    return (combined & SPTYP_EARTH) && (combined & SPTYP_AIR)
           || (combined & SPTYP_FIRE)  && (combined & SPTYP_ICE);
}

const char *spell_title(spell_type spell)
{
    return _seekspell(spell)->title;
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

    for (radius_iterator ri(agent->pos(), LOS_NO_TRANS); ri; ++ri)
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
        if (mon && !affected.count(mon))
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
        //    = [1 - P(swapped into m+k+1 position)] * P(prev. chosen)
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

    targetter_cloud place(agent, GDM, number, number);
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

        if (place.seen[c] <= 0 || cell_is_solid(c))
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
                      targeting_type restrict, targ_mode_type mode,
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

unsigned int skill2spell_type(skill_type spell_skill)
{
    switch (spell_skill)
    {
    case SK_CONJURATIONS:    return SPTYP_CONJURATION;
    case SK_HEXES:           return SPTYP_HEXES;
    case SK_CHARMS:          return SPTYP_CHARMS;
    case SK_FIRE_MAGIC:      return SPTYP_FIRE;
    case SK_ICE_MAGIC:       return SPTYP_ICE;
    case SK_TRANSMUTATIONS:  return SPTYP_TRANSMUTATION;
    case SK_NECROMANCY:      return SPTYP_NECROMANCY;
    case SK_SUMMONINGS:      return SPTYP_SUMMONING;
    case SK_TRANSLOCATIONS:  return SPTYP_TRANSLOCATION;
    case SK_POISON_MAGIC:    return SPTYP_POISON;
    case SK_EARTH_MAGIC:     return SPTYP_EARTH;
    case SK_AIR_MAGIC:       return SPTYP_AIR;

    default:
        return SPTYP_NONE;
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
    return spell > SPELL_NO_SPELL && spell < NUM_SPELLS
           && spell_list[spell] != -1;
}

static bool _spell_range_varies(spell_type spell)
{
    int minrange = _seekspell(spell)->min_range;
    int maxrange = _seekspell(spell)->max_range;

    return minrange < maxrange;
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
        && in_good_standing(GOD_VEHUMET, 3)
        && maxrange > 1
        && spell != SPELL_GLACIATE)
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
 * @param spell  The spell being casted.
 * @return       The amount of noise generated on cast.
 */
int spell_noise(spell_type spell)
{
    return _seekspell(spell)->noise;
}

/**
 * Miscellaneous spell casting noise.
 *
 * This returns the usual spell noise for the effects of this spell.
 * Used for various noisy() calls, as well as the I screen; see effect_noise
 * comment above for more information.
 * @param spell  The spell being casted.
 * @return       The amount of noise generated by the effects of the spell.
 */
int spell_effect_noise(spell_type spell)
{
    int expl_size;
    switch (spell)
    {
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_FIREBALL:
        expl_size = 1;
        break;

    case SPELL_LRD:
        expl_size = 2; // Can reach 3 only with green crystal, which is rare
        break;

    // worst case scenario for these
    case SPELL_FIRE_STORM:
    case SPELL_CONJURE_BALL_LIGHTNING:
        expl_size = 3;
        break;

    default:
        expl_size = 0;
    }

    if (expl_size)
        return explosion_noise(expl_size);

    return _seekspell(spell)->effect_noise;
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
    case SPELL_DRAGON_CALL:
        return you.form == TRAN_DRAGON;
    default:
        break;
    }

    return false;
}

/**
 * Does the given spell map to a player transformation?
 *
 * @param spell     The spell in question.
 * @return          Whether the spell, when cast, sets a TRAN_ on the player.
 */
bool spell_is_form(spell_type spell)
{
    switch (spell)
    {
        case SPELL_BEASTLY_APPENDAGE:
        case SPELL_BLADE_HANDS:
        case SPELL_DRAGON_FORM:
        case SPELL_HYDRA_FORM:
        case SPELL_ICE_FORM:
        case SPELL_SPIDER_FORM:
        case SPELL_STATUE_FORM:
        case SPELL_NECROMUTATION:
            return true;
        default:
            return false;
    }
}

/**
 * This function attempts to determine if a given spell is useless to the
 * player.
 *
 * @param spell     The spell in question.
 * @param temp      Include checks for volatile or temporary states
 *                  (status effects, mana, gods, items, etc.)
 * @param prevent   Whether to only check for effects which prevent casting,
 *                  rather than just ones that make it unproductive.
 * @return          Whether the given spell has no chance of being useful.
 */
bool spell_is_useless(spell_type spell, bool temp, bool prevent)
{
    return spell_uselessness_reason(spell, temp, prevent) != "";
}

/**
 * This function gives the reason that a spell is currently useless to the
 * player, if it is.
 *
 * @param spell     The spell in question.
 * @param temp      Include checks for volatile or temporary states
 *                  (status effects, mana, gods, items, etc.)
 * @param prevent   Whether to only check for effects which prevent casting,
 *                  rather than just ones that make it unproductive.
 * @param evoked    Is the spell being evoked from an item?
 * @return          The reason a spell is useless to the player, if it is;
 *                  "" otherwise;
 */
string spell_uselessness_reason(spell_type spell, bool temp, bool prevent,
                                bool evoked)
{
    if (temp)
    {
        if (you.duration[DUR_CONF] > 0)
            return "You're too confused!";
        if (!enough_mp(spell_mana(spell), true, false))
            return "You don't have enough magic!";
        if (!prevent && spell_no_hostile_in_range(spell))
            return "You can't see any valid targets!";
    }

    // Check for banned schools (Currently just Ru sacrifices)
    if (!evoked && cannot_use_schools(get_spell_disciplines(spell)))
        return "You cannot use spells of this school!";


#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {
        if (spell == SPELL_ICE_FORM  || spell == SPELL_OZOCUBUS_ARMOUR)
            return "You're too hot!";

        if (spell == SPELL_LEDAS_LIQUEFACTION)
            return "You can't cast this while perpetually flying!";
    }

    if (you.species == SP_LAVA_ORC)
    {
        if (spell == SPELL_OZOCUBUS_ARMOUR)
            return "Your stony body would shatter the ice!";
        if (spell == SPELL_STONESKIN)
            return "Your skin is already made of stone!";

        if (temp && !temperature_effect(LORC_STONESKIN))
        {
            switch (spell)
            {
                case SPELL_STATUE_FORM:
                case SPELL_ICE_FORM:
                case SPELL_CONDENSATION_SHIELD:
                    return "You're too hot!";
                default:
                    break;
            }
        }

    }
#endif

    switch (spell)
    {
    case SPELL_CONTROL_TELEPORT:
        if (player_has_orb())
            return "The orb interferes with controlled teleportation!";
        // fallthrough to blink/cblink
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
        // XXX: this is a little redundant with you_no_tele_reason()
        // but trying to sort out temp and ctele and so on is a mess
        if (you.species == SP_FORMICID)
            return pluralise(species_name(you.species)) + " cannot teleport.";

        if (temp && you.no_tele(false, false, true)
            && (!prevent || spell != SPELL_CONTROL_TELEPORT))
        {
            return you.no_tele_reason(false, true);
        }
        break;

    case SPELL_SWIFTNESS:
        if (temp && !prevent)
        {
            if (player_movement_speed() <= FASTEST_PLAYER_MOVE_SPEED)
                return "You're already traveling as fast as you can!";
            if (you.is_stationary())
                return "You can't move!";
        }
        break;

    case SPELL_FLY:
        if (!prevent && you.racial_permanent_flight())
            return "You can already fly whenever you want!";
        if (temp)
        {
            if (get_form()->forbids_flight())
                return "Your current form prevents flight!";
            if (you.permanent_flight())
                return "You can already fly indefinitely!";
        }
        break;

    case SPELL_INVISIBILITY:
        if (!prevent && temp && you.backlit())
            return "Invisibility won't help you when you glow in the dark!";
        break;

    case SPELL_DARKNESS:
        // mere corona is not enough, but divine light blocks it completely
        if (!prevent && temp && (you.haloed()
                                 || in_good_standing(GOD_SHINING_ONE)))
        {
            return "Darkness is useless against divine light!";
        }
        break;

    case SPELL_REPEL_MISSILES:
        if (temp && (player_mutation_level(MUT_DISTORTION_FIELD) == 3
                        || you.scan_artefacts(ARTP_RMSL, true)))
        {
            return "You're already repelling missiles!";
        }
        break;

    case SPELL_STONESKIN:
    case SPELL_BEASTLY_APPENDAGE:
    case SPELL_BLADE_HANDS:
    case SPELL_DRAGON_FORM:
    case SPELL_HYDRA_FORM:
    case SPELL_ICE_FORM:
    case SPELL_SPIDER_FORM:
    case SPELL_STATUE_FORM:
        if (you.undead_state(temp) == US_UNDEAD
            || you.undead_state(temp) == US_HUNGRY_DEAD)
        {
            return "Your undead flesh cannot be transformed!";
        }
        if (temp && you.is_lifeless_undead())
            return "Your current blood level is not sufficient!";
        break;

    case SPELL_REGENERATION:
        if (you.species == SP_DEEP_DWARF)
            return "You can't regenerate without divine aid!";
        if (you.undead_state(temp) == US_UNDEAD)
            return "You're too dead to regenerate!";
        break;

    case SPELL_INTOXICATE:
        if (you.undead_state(temp) == US_UNDEAD)
            return "Your brain is too dead to use!";
        break;

    case SPELL_PORTAL_PROJECTILE:
    case SPELL_WARP_BRAND:
    case SPELL_EXCRUCIATING_WOUNDS:
    case SPELL_SURE_BLADE:
    case SPELL_SPECTRAL_WEAPON:
        if (you.species == SP_FELID)
            return "This spell is useless without hands!";
        break;

    case SPELL_LEDAS_LIQUEFACTION:
        if (temp && (!you.stand_on_solid_ground()
                        || you.duration[DUR_LIQUEFYING]
                        || liquefied(you.pos())))
        {
            return "You must stand on solid ground to cast this!";
        }
        break;

    case SPELL_DELAYED_FIREBALL:
        if (temp && you.attribute[ATTR_DELAYED_FIREBALL])
            return "You are already charged!";
        break;

    case SPELL_BORGNJORS_REVIVIFICATION:
    case SPELL_DEATHS_DOOR:
        // Prohibited to all undead.
        if (you.undead_state(temp))
            return "You're too dead!";
        break;
    case SPELL_NECROMUTATION:
        // only prohibted to actual undead, not lichformed players
        if (you.undead_state(false))
            return "You're too dead!";
        break;

    case SPELL_CURE_POISON:
        // no good for poison-immune species (ghoul, mummy, garg)
        if (player_res_poison(false, temp, temp) == 3
            // allow starving vampires to memorize cpois
            && you.undead_state() != US_SEMI_UNDEAD)
        {
            return "You can't be poisoned!";
        }
        break;

    case SPELL_SUBLIMATION_OF_BLOOD:
        // XXX: write player_can_bleed(bool temp) & use that
        if (you.species == SP_GARGOYLE
            || you.species == SP_GHOUL
            || you.species == SP_MUMMY
            || (temp && !form_can_bleed(you.form)))
        {
            return "You have no blood to sublime!";
        }
        break;

    case SPELL_ENSLAVEMENT:
        if (player_mutation_level(MUT_NO_LOVE))
            return "You cannot make allies!";

    case SPELL_MALIGN_GATEWAY:
        if (temp && !can_cast_malign_gateway())
        {
            return "The dungeon can only cope with one malign gateway"
                    " at a time!";
        }
        break;

    case SPELL_TORNADO:
        if (temp && (you.duration[DUR_TORNADO]
                     || you.duration[DUR_TORNADO_COOLDOWN]))
        {
            return "You need to wait for the winds to calm down.";
        }
        break;

    case SPELL_SUMMON_FOREST:
        if (temp && you.duration[DUR_FORESTED])
            return "You can only summon one forest at a time!";
        break;

    default:
        break;
    }

    return "";
}

/**
 * Determines what colour a spell should be highlighted with.
 *
 * @param spell           The type of spell to be coloured.
 * @param default_colour   Colour to be used if the spell is unremarkable.
 * @param transient       If true, check if spell is temporarily useless.
 * @param rod_spell       If the spell being evoked from a rod.
 * @return                The colour to highlight the spell.
 */
int spell_highlight_by_utility(spell_type spell, int default_colour,
                               bool transient, bool rod_spell)
{
    // If your god hates the spell, that
    // overrides all other concerns
    if (god_hates_spell(spell, you.religion, rod_spell)
        || is_good_god(you.religion) && you.spellcasting_unholy())
    {
        return COL_FORBIDDEN;
    }

    if (_spell_is_empowered(spell) && !rod_spell)
        default_colour = COL_EMPOWERED;

    if (spell_is_useless(spell, transient))
        default_colour = COL_USELESS;

    return default_colour;
}

bool spell_no_hostile_in_range(spell_type spell, bool rod)
{
    const int range = calc_spell_range(spell, 0, rod);
    const int minRange = get_dist_to_nearest_monster();
    switch (spell)
    {
    // These don't target monsters.
    case SPELL_APPORTATION:
    case SPELL_CONJURE_FLAME:
    case SPELL_PASSWALL:
    case SPELL_GOLUBRIAS_PASSAGE:
    case SPELL_LRD:
    case SPELL_FULMINANT_PRISM:
    case SPELL_SUMMON_LIGHTNING_SPIRE:

    // Shock and Lightning Bolt are no longer here, as the code below can
    // account for possible bounces.

    case SPELL_FIRE_STORM:
        return false;

    case SPELL_CHAIN_LIGHTNING:
    case SPELL_OZOCUBUS_REFRIGERATION:
    case SPELL_OLGREBS_TOXIC_RADIANCE:
        return minRange > LOS_RADIUS_SQ;

    // Special handling for cloud spells.
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
    {
        targetter_cloud tgt(&you, range);
        // Accept monsters that are in clouds for the hostiles-in-range check
        // (not for actual targetting).
        tgt.avoid_clouds = false;
        for (radius_iterator ri(you.pos(), range, C_ROUND, LOS_NO_TRANS);
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
                        || mons->type == MONS_BALLISTOMYCETE
                            && mons->ballisto_activity))
                {
                    return false;
                }
            }
        }

        return true;
    }
    default:
        break;
    }

    if (minRange < 0 || range < 0)
        return false;

    // The healing spells.
    if (testbits(get_spell_flags(spell), SPFLAG_HELPFUL))
        return false;

    const bool neutral = testbits(get_spell_flags(spell), SPFLAG_NEUTRAL);

    bolt beam;
    beam.flavour = BEAM_VISUAL;
    beam.origin_spell = spell;

    zap_type zap = spell_to_zap(spell);
    if (spell == SPELL_FIREBALL)
        zap = ZAP_FIREBALL;
    else if (spell == SPELL_RANDOM_BOLT) // don't let it think that there are no susceptible monsters in range
        zap = ZAP_DEBUGGING_RAY;

    if (zap != NUM_ZAPS)
    {
        beam.thrower = KILL_YOU_MISSILE;
        zappy(zap, calc_spell_power(spell, true, false, true, rod), beam);
    }
    else if (spell == SPELL_MEPHITIC_CLOUD)
    {
        beam.flavour = BEAM_MEPHITIC;
        beam.ex_size = 1;
        beam.damage = dice_def(1, 1); // so that foe_info is populated
        beam.hit = 20;
        beam.thrower = KILL_YOU;
        beam.ench_power = calc_spell_power(spell, true, false, true, rod);
        beam.is_beam = false;
        beam.is_explosion = true;
    }

    if (beam.flavour != BEAM_VISUAL)
    {
        bolt tempbeam;
        bool found = false;
        beam.source_id = MID_PLAYER;
        beam.range = range;
        beam.is_tracer = true;
        beam.is_targeting = true;
        beam.source  = you.pos();
        beam.dont_stop_player = true;
        beam.friend_info.dont_stop = true;
        beam.foe_info.dont_stop = true;
        beam.attitude = ATT_FRIENDLY;
#ifdef DEBUG_DIAGNOSTICS
        beam.quiet_debug = true;
#endif
        for (radius_iterator ri(you.pos(), range, C_ROUND, LOS_DEFAULT);
             ri; ++ri)
        {
            tempbeam = beam;
            tempbeam.target = *ri;
            tempbeam.fire();
            if (tempbeam.foe_info.count > 0
                || neutral && tempbeam.friend_info.count > 0)
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

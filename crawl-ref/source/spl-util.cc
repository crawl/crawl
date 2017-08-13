/**
 * @file
 * @brief data handlers for player-available spell list
**/

#include "AppHdr.h"

#include "spl-util.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "areas.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "god-passive.h"
#include "god-abil.h"
#include "item-prop.h"
#include "level-state-type.h"
#include "libutil.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "orb.h"
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
#include "tiledef-gui.h"    // spell tiles
#include "tiles-build-specific.h"
#include "transform.h"

struct spell_desc
{
    spell_type id;
    const char  *title;
    spschools_type disciplines;
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

    /// Icon for the spell in e.g. spellbooks, casting menus, etc.
    tileidx_t tile;
};

#include "spl-data.h"

static int spell_list[NUM_SPELLS];

#define SPELLDATASIZE ARRAYSZ(spelldata)

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

        ASSERTM(data.title != nullptr && *data.title,
                "spell #%d, id %d has no name", i, data.id);

        ASSERTM(data.level >= 1 && data.level <= 9,
                "spell '%s' has invalid level %d", data.title, data.level);

        ASSERTM(data.min_range <= data.max_range,
                "spell '%s' has min_range larger than max_range", data.title);

        ASSERTM(!(data.flags & SPFLAG_TARGETING_MASK)
                || (data.min_range >= 0 && data.max_range > 0),
                "targeted/directed spell '%s' has invalid range", data.title);

        ASSERTM(!(data.flags & SPFLAG_MONSTER && is_player_spell(data.id)),
                "spell '%s' is declared as a monster spell but is a player spell", data.title);

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
        return lookup(spell_name_cache, name, SPELL_NO_SPELL);

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
        const auto type = spschools_type::exponent(i);

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
    // you.spells is a FixedVector of spells in some arbitrary order. It
    // doesn't corespond to letters.
    auto i = find(begin(you.spells), end(you.spells), spell);
    return i == end(you.spells) ? -1 : i - begin(you.spells);
}

static int _get_spell_letter_from_slot(int slot)
{
    // you.spell_letter_table is a FixedVector that is basically a mapping
    // from alpha char indices to spell slots (e.g. indices in you.spells).
    auto letter = find(begin(you.spell_letter_table), end(you.spell_letter_table), slot);
    return letter == end(you.spell_letter_table) ? -1 : letter - begin(you.spell_letter_table);
}

int get_spell_letter(spell_type spell)
{
    int i = _get_spell_letter_from_slot(_get_spell_slot(spell));
    return (i == -1) ? -1 : index_to_letter(i);
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
    bool overwrite = false;
    for (const auto &entry : Options.auto_spell_letters)
    {
        if (!entry.first.matches(sname))
            continue;
        for (char ch : entry.second)
        {
            if (ch == '+')
                overwrite = true;
            else if (ch == '-')
                overwrite = false;
            else if (isaalpha(ch))
            {
                const int slot = letter_to_index(ch);
                const int existing = you.spell_letter_table[slot];
                if (existing == -1)
                {
                    j = slot;
                    break;
                }
                else if (overwrite)
                {
                    const string ename = lowercase_string(
                            spell_title(static_cast<spell_type>(existing)));
                    // Don't overwrite a spell matched by the same rule.
                    if (!entry.first.matches(ename))
                    {
                        j = slot;
                        break;
                    }
                }
                // Otherwise continue on to the next letter in this rule.
            }
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

    // Swapping with an existing spell.
    if (you.spell_letter_table[j] != -1)
    {
        // Find a spot for the spell being moved. Assumes there will be one.
        for (int free = 0; free < 52; free++)
            if (you.spell_letter_table[free] == -1)
            {
                you.spell_letter_table[free] = you.spell_letter_table[j];
                break;
            }
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

static void _remove_spell_attributes(spell_type spell)
{
    switch (spell)
    {
    case SPELL_DEFLECT_MISSILES:
        if (you.attribute[ATTR_DEFLECT_MISSILES])
        {
            const int orig_defl = you.missile_deflection();
            you.attribute[ATTR_DEFLECT_MISSILES] = 0;
            mprf(MSGCH_DURATION, "You feel %s from missiles.",
                                 you.missile_deflection() < orig_defl
                                 ? "less protected"
                                 : "your spell is no longer protecting you");
        }
        break;
#if TAG_MAJOR_VERSION == 34
    case SPELL_DELAYED_FIREBALL:
        if (you.attribute[ATTR_DELAYED_FIREBALL])
        {
            you.attribute[ATTR_DELAYED_FIREBALL] = 0;
            mprf(MSGCH_DURATION, "Your charged fireball dissipates.");
        }
        break;
#endif
    default:
        break;
    }
    return;
}

bool del_spell_from_memory_by_slot(int slot)
{
    ASSERT_RANGE(slot, 0, MAX_KNOWN_SPELLS);

    if (you.last_cast_spell == you.spells[slot])
        you.last_cast_spell = SPELL_NO_SPELL;

    spell_skills(you.spells[slot], you.stop_train);

    mprf("Your memory of %s unravels.", spell_title(you.spells[slot]));
    _remove_spell_attributes(you.spells[slot]);

    you.spells[slot] = SPELL_NO_SPELL;

    for (int j = 0; j < 52; j++)
        if (you.spell_letter_table[j] == slot)
            you.spell_letter_table[j] = -1;

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

int spell_hunger(spell_type which_spell)
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

    hunger -= you.skill(SK_SPELLCASTING, you.intel());

    if (hunger < 0)
        hunger = 0;

    return hunger;
}

// Checks if the spell is an explosion that can be placed anywhere even without
// an unobstructed beam path, such as fire storm.
bool spell_is_direct_explosion(spell_type spell)
{
    return spell == SPELL_FIRE_STORM || spell == SPELL_CALL_DOWN_DAMNATION
           || spell == SPELL_GHOSTLY_SACRIFICE || spell == SPELL_UPHEAVAL;
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
#if TAG_MAJOR_VERSION == 34
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
#endif

    return levels;
}

unsigned int get_spell_flags(spell_type which_spell)
{
    return _seekspell(which_spell)->flags;
}

const char *get_spell_target_prompt(spell_type which_spell)
{
    switch (which_spell)
    {
    case SPELL_APPORTATION:
        return "Apport";
    case SPELL_SMITING:
        return "Smite";
    case SPELL_LRD:
        return "Fragment what (e.g. wall or brittle monster)?";
    default:
        return nullptr;
    }
}

/// What's the icon for the given spell?
tileidx_t get_spell_tile(spell_type which_spell)
{
    return _seekspell(which_spell)->tile;
}

bool spell_typematch(spell_type which_spell, spschool_flag_type which_disc)
{
    return bool(get_spell_disciplines(which_spell) & which_disc);
}

//jmf: next two for simple bit handling
spschools_type get_spell_disciplines(spell_type spell)
{
    return _seekspell(spell)->disciplines;
}

int count_bits(uint64_t bits)
{
    uint64_t n;
    int c = 0;

    for (n = 1; n; n <<= 1)
        if (n & bits)
            c++;

    return c;
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
int apply_area_visible(cell_func cf, const coord_def &where)
{
    int rv = 0;

    for (radius_iterator ri(where, LOS_NO_TRANS); ri; ++ri)
        rv += cf(*ri);

    return rv;
}

// Applies the effect to all nine squares around/including the target.
// Returns summation of return values from passed in function.
static int _apply_area_square(cell_func cf, const coord_def& where)
{
    int rv = 0;

    for (adjacent_iterator ai(where, false); ai; ++ai)
        rv += cf(*ai);

    return rv;
}

// Applies the effect to the eight squares beside the target.
// Returns summation of return values from passed in function.
static int _apply_area_around_square(cell_func cf, const coord_def& where)
{
    int rv = 0;

    for (adjacent_iterator ai(where, true); ai; ++ai)
        rv += cf(*ai);

    return rv;
}

// Affect up to max_targs monsters around a point, chosen randomly.
// Return varies with the function called; return values will be added up.
int apply_random_around_square(cell_func cf, const coord_def& where,
                               bool exclude_center, int max_targs)
{
    int rv = 0;

    if (max_targs <= 0)
        return 0;

    if (max_targs >= 9 && !exclude_center)
        return _apply_area_square(cf, where);

    if (max_targs >= 8 && exclude_center)
        return _apply_area_around_square(cf, where);

    coord_def targs[8];

    int count = 0;

    for (adjacent_iterator ai(where, exclude_center); ai; ++ai)
    {
        if (monster_at(*ai) == nullptr && *ai != you.pos())
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
        // Therefore, it's uniform for n = m + k + 1. QED
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
            rv += cf(targs[i]);
        }
    }

    return rv;
}

void apply_area_cloud(cloud_func func, const coord_def& where,
                       int pow, int number, cloud_type ctype,
                       const actor *agent, int spread_rate, int excl_rad)
{
    if (number <= 0)
        return;

    targeter_cloud place(agent, GDM, number, number);
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
        func(c, pow, spread_rate, ctype, agent, excl_rad);
        number--;
    }
}

/**
 * Select a spell target and fill dist and pbolt appropriately.
 *
 * @param[out] spelld    the output of the direction() call.
 * @param[in, out] pbolt a beam; its range is used if none is set in args, and
 *                       its source and target are set if the direction() call
 *                       succeeds.
 * @param[in] args       The arguments for the direction() call. May be null,
 *                       which case a default is used.
 * @return false if the user cancelled, true otherwise.
 */
bool spell_direction(dist &spelld, bolt &pbolt, direction_chooser_args *args)
{
    direction_chooser_args newargs;
    // This should be before the overwrite, so callers can specify a different
    // mode if they want.
    newargs.mode = TARG_HOSTILE;
    if (args)
        newargs = *args;
    if (newargs.range < 1)
        newargs.range = (pbolt.range < 1) ? you.current_vision : pbolt.range;

    direction(spelld, newargs);

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

const char* spelltype_short_name(spschool_flag_type which_spelltype)
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
        return "Tmut";
    case SPTYP_NECROMANCY:
        return "Necr";
    case SPTYP_SUMMONING:
        return "Summ";
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

const char* spelltype_long_name(spschool_flag_type which_spelltype)
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

skill_type spell_type2skill(spschool_flag_type spelltype)
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
        dprf("spell_type2skill: called with spelltype %u", spelltype);
        return SK_NONE;
    }
}

spschool_flag_type skill2spell_type(skill_type spell_skill)
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

int spell_range(spell_type spell, int pow, bool allow_bonus)
{
    int minrange = _seekspell(spell)->min_range;
    int maxrange = _seekspell(spell)->max_range;
    ASSERT(maxrange >= minrange);

    // spells with no range have maxrange == minrange == -1
    if (maxrange < 0)
        return maxrange;

    if (allow_bonus
        && vehumet_supports_spell(spell)
        && have_passive(passive_t::spells_range)
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
    case SPELL_VIOLENT_UNRAVELLING:
    case SPELL_IGNITION:
        expl_size = 1;
        break;

    case SPELL_LRD:
        expl_size = 2; // Can reach 3 only with crystal walls, which are rare
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

/**
 * Does the given spell map to a player transformation?
 *
 * @param spell     The spell in question.
 * @return          Whether the spell, when cast, puts the player in a form.
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
 * @param spell      The spell in question.
 * @param temp       Include checks for volatile or temporary states
 *                   (status effects, mana, gods, items, etc.)
 * @param prevent    Whether to only check for effects which prevent casting,
 *                   rather than just ones that make it unproductive.
 * @param fake_spell true if the spell is evoked or from an innate or divine ability
 *                   false if it is a spell being cast normally.
 * @return           Whether the given spell has no chance of being useful.
 */
bool spell_is_useless(spell_type spell, bool temp, bool prevent,
                      bool fake_spell)
{
    return spell_uselessness_reason(spell, temp, prevent, fake_spell) != "";
}

/**
 * This function gives the reason that a spell is currently useless to the
 * player, if it is.
 *
 * @param spell      The spell in question.
 * @param temp       Include checks for volatile or temporary states
 *                   (status effects, mana, gods, items, etc.)
 * @param prevent    Whether to only check for effects which prevent casting,
 *                   rather than just ones that make it unproductive.
 * @param fake_spell true if the spell is evoked or from an innate or divine ability
 *                   false if it is a spell being cast normally.
 * @return           The reason a spell is useless to the player, if it is;
 *                   "" otherwise. The string should be a full clause, but
 *                   begin with a lowercase letter so callers can put it in
 *                   the middle of a sentence.
 */
string spell_uselessness_reason(spell_type spell, bool temp, bool prevent,
                                bool fake_spell)
{
    if (temp)
    {
        if (!fake_spell && you.duration[DUR_CONF] > 0)
            return "you're too confused.";
        if (!enough_mp(spell_mana(spell), true, false)
            && !fake_spell)
        {
            return "you don't have enough magic.";
        }
        if (!prevent && spell_no_hostile_in_range(spell))
            return "you can't see any valid targets.";
    }

    // Check for banned schools (Currently just Ru sacrifices)
    if (!fake_spell && cannot_use_schools(get_spell_disciplines(spell)))
        return "you cannot use spells of this school.";

    switch (spell)
    {
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
        // XXX: this is a little redundant with you_no_tele_reason()
        // but trying to sort out temp and so on is a mess
        if (you.species == SP_FORMICID)
            return "your stasis prevents you from teleporting.";

        if (temp && you.no_tele(false, false, true))
            return lowercase_first(you.no_tele_reason(false, true));
        break;

    case SPELL_SWIFTNESS:
        if (temp)
        {
            if (you.duration[DUR_SWIFTNESS])
                return "this spell is already in effect.";
            if (player_movement_speed() <= FASTEST_PLAYER_MOVE_SPEED)
                return "you're already traveling as fast as you can.";
            if (you.is_stationary())
                return "you can't move.";
        }
        break;

    case SPELL_INVISIBILITY:
        if (!prevent && temp && you.backlit())
            return "invisibility won't help you when you glow in the dark.";
        break;

    case SPELL_DARKNESS:
        // mere corona is not enough, but divine light blocks it completely
        if (temp && (you.haloed() || !prevent && have_passive(passive_t::halo)))
            return "darkness is useless against divine light.";
        break;

    case SPELL_DEFLECT_MISSILES:
        if (temp && you.attribute[ATTR_DEFLECT_MISSILES])
            return "you're already deflecting missiles.";
        break;

    case SPELL_STATUE_FORM:
        if (SP_GARGOYLE == you.species)
            return "you're already a statue.";
        // fallthrough to other forms

    case SPELL_BEASTLY_APPENDAGE:
    case SPELL_BLADE_HANDS:
    case SPELL_DRAGON_FORM:
    case SPELL_HYDRA_FORM:
    case SPELL_ICE_FORM:
    case SPELL_SPIDER_FORM:
        if (you.undead_state(temp) == US_UNDEAD
            || you.undead_state(temp) == US_HUNGRY_DEAD)
        {
            return "your undead flesh cannot be transformed.";
        }
        if (temp && you.is_lifeless_undead())
            return "your current blood level is not sufficient.";
        break;

    case SPELL_REGENERATION:
        if (you.species == SP_DEEP_DWARF)
            return "you can't regenerate without divine aid.";
        if (you.undead_state(temp) == US_UNDEAD)
            return "you're too dead to regenerate.";
        break;

    case SPELL_EXCRUCIATING_WOUNDS:
        if (temp
            && (!you.weapon()
                || you.weapon()->base_type != OBJ_WEAPONS
                || !is_brandable_weapon(*you.weapon(), true)))
        {
            return "you aren't wielding a brandable weapon.";
        }
        // intentional fallthrough
    case SPELL_PORTAL_PROJECTILE:
    case SPELL_SPECTRAL_WEAPON:
        if (you.species == SP_FELID)
            return "this spell is useless without hands.";
        break;

    case SPELL_LEDAS_LIQUEFACTION:
        if (temp && (!you.stand_on_solid_ground()
                     || you.duration[DUR_LIQUEFYING]
                     || liquefied(you.pos())))
        {
            return "you must stand on solid ground to cast this.";
        }
        break;
#if TAG_MAJOR_VERSION == 34
    case SPELL_DELAYED_FIREBALL:
        if (temp && you.attribute[ATTR_DELAYED_FIREBALL])
            return "you are already charged.";
        break;
#endif
    case SPELL_BORGNJORS_REVIVIFICATION:
        if (temp && you.hp == you.hp_max)
            return "you cannot be healed further.";
        if (temp && you.hp_max < 21)
            return "you lack the resilience to cast this spell.";
        // Prohibited to all undead.
        if (you.undead_state(temp))
            return "you're too dead.";
        break;
    case SPELL_DEATHS_DOOR:
        if (temp && you.duration[DUR_DEATHS_DOOR])
            return "you are already standing in death's doorway.";
        if (temp && you.duration[DUR_DEATHS_DOOR_COOLDOWN])
            return "you are still too close to death's doorway.";
        // Prohibited to all undead.
        if (you.undead_state(temp))
            return "you're too dead.";
        break;
    case SPELL_NECROMUTATION:
        // only prohibted to actual undead, not lichformed players
        if (you.undead_state(false))
            return "you're too dead.";
        break;

    case SPELL_OZOCUBUS_ARMOUR:
        if (temp && !player_effectively_in_light_armour())
            return "your body armour is too heavy.";
        if (temp && you.form == transformation::statue)
            return "the film of ice won't work on stone.";
        if (temp && you.duration[DUR_FIRE_SHIELD])
            return "your ring of flames would instantly melt the ice.";
        break;

    case SPELL_CIGOTUVIS_EMBRACE:
        if (temp && you.form == transformation::statue)
            return "the corpses won't embrace your stony flesh.";
        if (temp && you.duration[DUR_ICY_ARMOUR])
            return "the corpses won't embrace your icy flesh.";
        break;

    case SPELL_SUBLIMATION_OF_BLOOD:
        // XXX: write player_can_bleed(bool temp) & use that
        if (you.species == SP_GARGOYLE
            || you.species == SP_GHOUL
            || you.species == SP_MUMMY
            || (temp && !form_can_bleed(you.form)))
        {
            return "you have no blood to sublime.";
        }
        if (you.magic_points == you.max_magic_points && temp)
            return "your reserves of magic are already full.";
        break;

    case SPELL_TORNADO:
        if (temp && (you.duration[DUR_TORNADO]
                     || you.duration[DUR_TORNADO_COOLDOWN]))
        {
            return "you need to wait for the winds to calm down.";
        }
        break;

    case SPELL_MALIGN_GATEWAY:
        if (temp && !can_cast_malign_gateway())
        {
            return "the dungeon can only cope with one malign gateway"
                    " at a time.";
        }
        break;

    case SPELL_SUMMON_FOREST:
        if (temp && you.duration[DUR_FORESTED])
            return "you can only summon one forest at a time.";
        break;

    case SPELL_PASSWALL:
        if (temp && you.is_stationary())
            return "you can't move";
        break;

    case SPELL_ANIMATE_DEAD:
    case SPELL_ANIMATE_SKELETON:
    case SPELL_TWISTED_RESURRECTION:
    case SPELL_CONTROL_UNDEAD:
    case SPELL_DEATH_CHANNEL:
    case SPELL_SIMULACRUM:
    case SPELL_INFESTATION:
        if (you.get_mutation_level(MUT_NO_LOVE))
            return "you cannot coerce anything to obey you.";
        break;

    case SPELL_CORPSE_ROT:
    case SPELL_POISONOUS_VAPOURS:
    case SPELL_CONJURE_FLAME:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_FREEZING_CLOUD:
    case SPELL_MEPHITIC_CLOUD:
        if (env.level_state & LSTATE_STILL_WINDS)
            return "the air is too still for clouds to form.";
        break;

    case SPELL_GOLUBRIAS_PASSAGE:
        if (orb_limits_translocation())
            return "the Orb prevents this spell from working.";

    default:
        break;
    }

    if (get_spell_disciplines(spell) & SPTYP_SUMMONING
        && spell != SPELL_AURA_OF_ABJURATION
        && you.get_mutation_level(MUT_NO_LOVE))
    {
        return "you cannot coerce anything to answer your summons.";
    }

    return "";
}

/**
 * Determines what colour a spell should be highlighted with.
 *
 * @param spell           The type of spell to be coloured.
 * @param default_colour   Colour to be used if the spell is unremarkable.
 * @param transient       If true, check if spell is temporarily useless.
 * @param memcheck        If true, check if spell can be memorised
 * @return                The colour to highlight the spell.
 */
int spell_highlight_by_utility(spell_type spell, int default_colour,
                               bool transient, bool memcheck)
{
    // If your god hates the spell, that overrides all other concerns.
    if (god_hates_spell(spell, you.religion)
        || is_good_god(you.religion) && you.spellcasting_unholy())
    {
        return COL_FORBIDDEN;
    }
    // Grey out spells for which you lack experience or spell levels.
    if (memcheck && (spell_difficulty(spell) > you.experience_level
        || player_spell_levels() < spell_levels_required(spell)))
    {
        return COL_INAPPLICABLE;
    }
    // Check if the spell is considered useless based on your current status
    if (spell_is_useless(spell, transient))
        return COL_USELESS;

    return default_colour;
}

bool spell_no_hostile_in_range(spell_type spell)
{
    const int range = calc_spell_range(spell, 0);
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
    case SPELL_INTOXICATE:
    case SPELL_IGNITION:
        return minRange > you.current_vision;

    // Special handling for cloud spells.
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
    case SPELL_HOLY_BREATH:
    {
        targeter_cloud tgt(&you, range);
        // Accept monsters that are in clouds for the hostiles-in-range check
        // (not for actual targeting).
        tgt.avoid_clouds = false;
        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_NO_TRANS);
             ri; ++ri)
        {
            if (!tgt.valid_aim(*ri))
                continue;
            tgt.set_aim(*ri);
            for (const auto &entry : tgt.seen)
            {
                if (entry.second == AFF_NO || entry.second == AFF_TRACER)
                    continue;

                // Checks here are from get_dist_to_nearest_monster().
                const monster* mons = monster_at(entry.first);
                if (mons && !mons->wont_attack() && mons_is_threatening(*mons))
                    return false;
            }
        }

        return true;
    }

    case SPELL_IGNITE_POISON:
        return cast_ignite_poison(&you, -1, false, true) == SPRET_ABORT;

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
    if (spell == SPELL_RANDOM_BOLT) // don't let it think that there are no susceptible monsters in range
        zap = ZAP_DEBUGGING_RAY;

    if (zap != NUM_ZAPS)
    {
        beam.thrower = KILL_YOU_MISSILE;
        zappy(zap, calc_spell_power(spell, true, false, true), false,
              beam);
        if (spell == SPELL_MEPHITIC_CLOUD)
            beam.damage = dice_def(1, 1); // so that foe_info is populated
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
        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_DEFAULT);
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

    if (range < minRange)
        return true;

    return false;
}


// a map of schools to the corresponding sacrifice 'mutations'.
static const mutation_type arcana_sacrifice_map[] = {
    MUT_NO_CONJURATION_MAGIC,
    MUT_NO_HEXES_MAGIC,
    MUT_NO_CHARM_MAGIC,
    MUT_NO_FIRE_MAGIC,
    MUT_NO_ICE_MAGIC,
    MUT_NO_TRANSMUTATION_MAGIC,
    MUT_NO_NECROMANCY_MAGIC,
    MUT_NO_SUMMONING_MAGIC,
    MUT_NO_TRANSLOCATION_MAGIC,
    MUT_NO_POISON_MAGIC,
    MUT_NO_EARTH_MAGIC,
    MUT_NO_AIR_MAGIC
};

/**
 * Are some subset of the given schools unusable by the player?
 * (Due to Sacrifice Arcana)
 *
 * @param schools   A bitfield containing a union of spschool_flag_types.
 * @return          Whether the player is unable use any of the given schools.
 */
bool cannot_use_schools(spschools_type schools)
{
    COMPILE_CHECK(ARRAYSZ(arcana_sacrifice_map) == SPTYP_LAST_EXPONENT + 1);

    // iter over every school
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        // skip schools not in the provided set
        const auto school = spschools_type::exponent(i);
        if (!(schools & school))
            continue;

        // check if the player has this school locked out
        const mutation_type lockout_mut = arcana_sacrifice_map[i];
        if (you.has_mutation(lockout_mut))
            return true;
    }

    return false;
}


/**
 * What's the spell school corresponding to the given Ru mutation?
 *
 * @param mutation  The variety of MUT_NO_*_MAGIC in question.
 * @return          The skill of the appropriate school (SK_AIR_MAGIC, etc).
 *                  If no school corresponds, returns SK_NONE.
 */
skill_type arcane_mutation_to_skill(mutation_type mutation)
{
    for (int exp = 0; exp <= SPTYP_LAST_EXPONENT; exp++)
        if (arcana_sacrifice_map[exp] == mutation)
            return spell_type2skill(spschools_type::exponent(exp));
    return SK_NONE;
}

bool spell_is_soh_breath(spell_type spell)
{
    return spell == SPELL_SERPENT_OF_HELL_GEH_BREATH
        || spell == SPELL_SERPENT_OF_HELL_COC_BREATH
        || spell == SPELL_SERPENT_OF_HELL_DIS_BREATH
        || spell == SPELL_SERPENT_OF_HELL_TAR_BREATH;
}

const vector<spell_type> *soh_breath_spells(spell_type spell)
{
    static const map<spell_type, vector<spell_type>> soh_breaths = {
        { SPELL_SERPENT_OF_HELL_GEH_BREATH,
            { SPELL_FIRE_BREATH,
              SPELL_FLAMING_CLOUD,
              SPELL_FIREBALL } },
        { SPELL_SERPENT_OF_HELL_COC_BREATH,
            { SPELL_COLD_BREATH,
              SPELL_FREEZING_CLOUD,
              SPELL_FLASH_FREEZE } },
        { SPELL_SERPENT_OF_HELL_DIS_BREATH,
            { SPELL_METAL_SPLINTERS,
              SPELL_QUICKSILVER_BOLT,
              SPELL_LEHUDIBS_CRYSTAL_SPEAR } },
        { SPELL_SERPENT_OF_HELL_TAR_BREATH,
            { SPELL_BOLT_OF_DRAINING,
              SPELL_MIASMA_BREATH,
              SPELL_CORROSIVE_BOLT } },
    };

    return map_find(soh_breaths, spell);
}
